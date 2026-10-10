"""Abyssfire content build: Art/Export + Audio/Export -> /Game/Abyssfire (ue58-platform.md 11, DECISIONS P5-P11).

Runs inside the Unreal Editor's Python (CPython 3.11, standard library only), headless:

    UnrealEditor-Cmd <repo>/unreal/Abyssfire.uproject -run=pythonscript \
        -script="<repo>/unreal/Scripts/build_content.py [options]" -unattended -nosplash -nop4 -stdout -FullStdOutLogOutput
    (macOS: .../UnrealEditor.app/Contents/MacOS/UnrealEditor with the same arguments; see Scripts/README.md)

or from the editor's Python console: `py "<repo>/unreal/Scripts/build_content.py" --only materials`.
`python3 unreal/Scripts/build_content.py --plan` (no editor) validates the art manifest and prints the import plan.

Pipeline (each step is idempotent; unchanged sources / graphs are skipped):
    textures   generated defaults + palettes, portraits, icons, FX sprites       -> Textures/, UI/, FX/Textures/
    mpc        MPC_AF_Lighting                                                   -> Materials/
    materials  M_AF_Toon(+_Foliage, _Slime), M_AF_Outline(+_Foliage), M_AF_Terrain, M_AF_Water, M_AF_PP_Grade,
               M_AF_FX_Additive / _Translucent / _Mesh, M_AF_Ghost, M_AF_LightPool, M_AF_TargetRing, M_AF_AffixAura,
               M_AF_BlobShadow                                                   -> Materials/, Materials/FX/
    instances  MI_AF_Toon_<Family>, MI_AF_Outline_<Family>_<Class>               -> Materials/Instances/
    fxquad     SM_FX_Quad                                                        -> FX/
    skeletal   SK_* (+ SKEL_* skeletons, LODs, sockets)                          -> <fbx dir>/, Skeletons/
    anims      A_* onto the family skeletons (additive settings, length check)    -> <fbx dir>/
    static     SM_* (+ sockets)                                                  -> <fbx dir>/
    assign     material slots (slot 0 toon, slot 1 outline hull; hull casts no shadow)
    audio      unreal/Audio/ue/import_audio.py (SoundWaves)                      -> Audio/
    level      L_Main (empty persistent map, world settings)                     -> Maps/
    verify     material shader statistics, project settings, cvars

Options:
    --only STEPS        comma list of the steps above (default: all)
    --skip STEPS        comma list of steps to leave out
    --family NAMES      only manifest assets matching these families / categories / names (e.g. "heroes")
    --force             re-import / rebuild everything even when unchanged
    --force-materials   rebuild the master materials even when their graph fingerprint is unchanged
    --prune-audio       delete SoundWaves the audio manifest no longer lists
    --audio-args ARGS   extra arguments for import_audio.py (quoted)
    --report PATH       JSON report path (default: $AF_CONTENT_REPORT or Saved/ContentBuild/build_report.json)
    --strict            warnings fail the build too
    --plan              validate the manifest and print the plan; no editor needed, nothing is written to the project

The JSON report contains "ok": false when anything failed (CI contract, Docs/CI.md 8); a failed build also ends with
a Python exception so `LogPython: Error` shows in the log.
"""
from __future__ import annotations

import argparse
import json
import shlex
import sys
import time
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
if __name__ == "__main__":
    # Re-running the script in a live editor session must pick up edited modules: drop the cached package.
    for _name in [n for n in sys.modules if n == "abyss_content" or n.startswith("abyss_content.")]:
        del sys.modules[_name]

from abyss_content import __version__, paths  # noqa: E402
from abyss_content.manifest import ManifestError, build_plan  # noqa: E402
from abyss_content.report import BuildReport  # noqa: E402

STEPS = ("textures", "mpc", "materials", "instances", "fxquad", "skeletal", "anims", "static", "assign", "audio",
         "level", "verify")


class ContentBuildFailed(RuntimeError):
    pass


def _split(value: str) -> list[str]:
    return [v.strip() for v in value.split(",") if v.strip()]


def parse_args(argv: list[str]) -> argparse.Namespace:
    ap = argparse.ArgumentParser(prog="build_content.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", default="")
    ap.add_argument("--skip", default="")
    ap.add_argument("--family", action="append", default=[])
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--force-materials", action="store_true")
    ap.add_argument("--prune-audio", action="store_true")
    ap.add_argument("--audio-args", default="")
    ap.add_argument("--report", default="")
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--plan", action="store_true")
    args = ap.parse_args(argv)
    only, skip = _split(args.only), _split(args.skip)
    unknown = [s for s in only + skip if s not in STEPS]
    if unknown:
        ap.error(f"unknown step(s) {unknown}; steps: {', '.join(STEPS)}")
    args.steps = [s for s in STEPS if (not only or s in only) and s not in skip]
    args.families = [f for item in args.family for f in _split(item)]
    return args


def _have_unreal() -> bool:
    try:
        import unreal  # noqa: F401
    except ImportError:
        return False
    return True


def run(argv: list[str]) -> BuildReport:
    args = parse_args(argv)
    report = BuildReport(args=list(argv))
    json_path = Path(args.report) if args.report else paths.report_json_path()
    md_path = json_path.with_suffix(".md")

    # ---- plan (pure Python) ----
    report.begin("plan")
    try:
        plan = build_plan(families=args.families)
    except ManifestError as e:
        report.error(str(e))
        report.end()
        report.write(json_path, md_path)
        raise ContentBuildFailed(str(e)) from e
    report.plan = plan.summary()
    for problem in plan.problems:
        (report.error if problem.level == "error" else report.warning)(str(problem))
    report.count("assets", len(plan.assets))
    report.count("textures", len(plan.textures))
    report.end()

    if args.plan or not _have_unreal():
        if not args.plan:
            print("build_content.py: the `unreal` module is not available (run inside the editor); printing the plan")
        print(json.dumps(plan.summary(), indent=2, ensure_ascii=False))
        report.write(json_path, md_path)
        return report

    # ---- editor steps ----
    import unreal  # noqa: F401 - fails early with a clear ImportError outside the editor

    from abyss_content.ue import audio, core, fxmeshes, level, materials, meshes, textures
    ed = core.Editor()
    try:
        report.engine = str(unreal.SystemLibrary.get_engine_version())
    except Exception:  # noqa: BLE001
        report.engine = "unknown"
    core.log(f"abyss_content {__version__} on {report.engine}: steps {', '.join(args.steps)}")
    if ed.content_lib is None:
        report.warnings.append("AbyssContentBuildLibrary (C++ helper) not loaded: skeletal sockets are skipped and the "
                               "build cannot wait for shader compilation; build the AbyssfireEditor target first")

    def step(name: str, fn) -> None:
        if name not in args.steps:
            report.skip(name, "not selected")
            return
        report.begin(name)
        core.log(f"step {name}")
        try:
            fn()
        except core.BuildError as e:
            report.error(f"{name}: {e}")
        except Exception as e:  # noqa: BLE001 - one broken step must not hide the others' results
            report.error(f"{name}: unexpected {type(e).__name__}: {e}")
            report.note(traceback.format_exc())
        report.end()

    for folder in (paths.MATERIALS_DIR, paths.INSTANCES_DIR, paths.FX_MATERIALS_DIR, paths.TEXTURES_DIR,
                   paths.DEFAULT_TEXTURES_DIR, paths.FX_DIR, paths.FX_TEXTURES_DIR, paths.UI_ICONS_DIR,
                   paths.UI_PORTRAITS_DIR, paths.SKELETONS_DIR, paths.MAPS_DIR):
        ed.ensure_dir(folder)

    step("textures", lambda: textures.import_all(ed, plan, report, args.force))
    step("mpc", lambda: materials.ensure_mpc(ed, plan.shading, report))
    step("materials", lambda: materials.build_masters(ed, plan, report, args.force or args.force_materials))
    step("instances", lambda: materials.build_instances(ed, plan, report))
    step("fxquad", lambda: fxmeshes.ensure_quad(ed, plan, report))
    step("skeletal", lambda: meshes.import_all_skeletal(ed, plan, report, args.force))
    step("anims", lambda: meshes.import_all_clips(ed, plan, report, args.force))
    step("static", lambda: meshes.import_all_static(ed, plan, report, args.force))

    def assign() -> None:
        for asset in plan.assets:
            mesh = ed.load(asset.content_path)
            if mesh is None:
                report.error(f"{asset.name}: not imported, materials not assigned")
                continue
            materials.assign_slots(ed, asset, mesh, plan.slot_materials.get(asset.name, {}), report)

    step("assign", assign)
    step("audio", lambda: audio.import_audio(report, args.force, args.prune_audio, shlex.split(args.audio_args)))
    step("level", lambda: level.ensure_main_map(ed, report))

    def verify() -> None:
        if not ed.finish_compilation():
            report.note("shader compilation not awaited (C++ helper missing): statistics may be incomplete")
        materials.check_material_stats(ed, report)
        level.check_project_settings(report)

    step("verify", verify)

    report.begin("save")
    try:
        ed.finish_compilation()
        ed.assets.save_directory(paths.CONTENT_ROOT, True, True)
    except Exception as e:  # noqa: BLE001
        report.error(f"saving {paths.CONTENT_ROOT}: {e}")
    report.end()

    if args.strict and report.warnings:
        report.errors.append(f"--strict: {len(report.warnings)} warning(s)")
    report.write(json_path, md_path)
    summary = (f"content build {'OK' if report.ok else 'FAILED'}: {len(report.errors)} error(s), "
               f"{len(report.warnings)} warning(s), {len(report.assets)} asset(s); report {json_path}")
    (core.log if report.ok else core.error)(summary)
    for e in report.errors:
        core.error(e)
    return report


def main(argv: list[str]) -> int:
    started = time.time()
    report = run(argv)
    if not report.ok:
        raise ContentBuildFailed(f"{len(report.errors)} error(s); see {paths.report_json_path()}")
    print(f"build_content.py: done in {time.time() - started:.1f} s")
    return 0


if __name__ == "__main__":
    # -run=pythonscript -script="build_content.py --only materials" puts the words after the script path in sys.argv[1:].
    main(sys.argv[1:])
