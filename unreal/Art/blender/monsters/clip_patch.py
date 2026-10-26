"""Add clips to a shipped monster without re-exporting what it already ships (dark remake Wave 2 art, 2H).

    /opt/venvs/blender/bin/python unreal/Art/blender/monsters/clip_patch.py <generator> <Clip> [<Clip> …]
        [--keys notes,fx,…] [--previews <glob> …] [--sockets name,…] [--replace Clip,…] [--ship-args "…"] [--dry-run]

``<generator>`` is a monster script (``gravegnawer``, ``corpseherd``, ``oath_zealot`` …: ``monsters/<name>.py``)
whose clip list now holds the new clips. Blender's FBX exporter stamps every file with its creation time and the
mesh builders do not keep a stable vertex order, so a full re-ship rewrites every FBX of the asset (identical in
content, different in bytes). This tool ships the generator into a scratch export / preview / palette root
(``$AF_SCRATCH_ROOT/clip_patch/<asset>``; the generator runs as is, with its QA, ink gate and re-import verify) and
then takes only what changed:

1. **Proof first.** Every clip the manifest already lists is compared curve by curve (``mship.fbx_local_curves``:
   every bone's Lcl translation / rotation / scaling at every key) with the shipped FBX, and its manifest entry
   (length, frames, loop, notifies, contact / release) with the shipped entry; the skeleton (bones), the triangle
   counts and the palette registries must match too. Any difference aborts: the generator changed a shipped clip
   (re-ship it in full instead).
2. **The new clips** (``A_<token>_<Clip>.fbx``) are copied into ``Art/Export/<category>/`` and appended to the
   entry's ``anims`` in the generator's order (``kit.export.Manifest``: locked, merged).
3. ``--keys``: top-level entry keys taken from the scratch ship (``notes``, ``fx``, ``corpse`` …); ``--sockets``:
   socket entries taken from it by name (a socket moved in the kit; sockets are created in UE from the manifest, so
   the shipped SK FBX stands, as ``elite_sigil.patch_gnawer_sockets``).
4. **Previews** of the new clips only: ``anim_<Clip>.png``, ``anim_back_<Clip>.png`` and ``game_w2*.png``
   (``mship.w2_sheet``) by default, plus any ``--previews`` globs; they are copied into ``Art/Previews/<dir>/`` and
   added to the entry's ``previews`` (the reviewed sheets of the shipped clips stay as they are).

``--replace Clip,…`` re-takes clips the manifest already lists — for a clip an earlier ``clip_patch`` added and a
review sent back (Wave 2: the zealot's Roar), not for a Wave 1 clip (re-ship that asset in full): the listed clips
skip the "unchanged" proof and are copied like new ones; every other shipped clip must still be the same.
"""
from __future__ import annotations

import fnmatch
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

from kit import export, paths  # noqa: E402

CLIP_FIELDS = ("name", "asset", "lengthMs", "frames", "fps", "loop", "notifies", "contactMs", "releaseMs",
               "additive", "refSpeedCmS")
TOL = {"Lcl Translation": 1e-4, "Lcl Rotation": 1e-2, "Lcl Scaling": 1e-4}


def curves_equal(fa: Path, fb: Path) -> tuple[bool, dict]:
    import mship
    ca, cb = mship.fbx_local_curves(fa), mship.fbx_local_curves(fb)
    worst = {k: 0.0 for k in TOL}
    for bone in set(ca) | set(cb):
        for prop in TOL:
            x, y = ca.get(bone, {}).get(prop), cb.get(bone, {}).get(prop)
            if x is None and y is None:
                continue
            if x is None or y is None or len(x) != len(y):
                worst[prop] = float("inf")
                continue
            for p, q in zip(x, y):
                d = max(abs(p[i] - q[i]) for i in range(3))
                if prop == "Lcl Rotation":
                    d = min(d, abs(360.0 - d))
                worst[prop] = max(worst[prop], d)
    return all(worst[k] <= TOL[k] for k in TOL), worst


def main(argv: list[str]) -> int:
    if not argv or argv[0].startswith("--"):
        print(__doc__)
        return 2
    gen = argv[0]
    clips, keys, globs, sockets, ship_args, dry, replace = [], [], [], [], [], False, []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--keys":
            keys += [k for k in argv[i + 1].split(",") if k]
            i += 2
        elif a == "--sockets":
            sockets += [k for k in argv[i + 1].split(",") if k]
            i += 2
        elif a == "--replace":
            replace += [k for k in argv[i + 1].split(",") if k]
            i += 2
        elif a == "--previews":
            i += 1
            while i < len(argv) and not argv[i].startswith("--"):
                globs.append(argv[i])
                i += 1
        elif a == "--ship-args":
            ship_args = argv[i + 1].split()
            i += 2
        elif a == "--dry-run":
            dry = True
            i += 1
        else:
            clips.append(a)
            i += 1
    script = HERE / f"{gen}.py"
    if not script.exists():
        raise SystemExit(f"no generator {script}")
    mod_src = script.read_text(encoding="utf-8")
    asset = next((ln.split("=", 1)[1].strip().strip('"') for ln in mod_src.splitlines()
                  if ln.startswith("ASSET = ")), None)
    if asset is None:
        raise SystemExit(f"{script.name}: no ASSET = \"…\" line")

    real_export, real_prev, real_pal = paths.export_root(), paths.preview_root(), paths.palette_root()
    work = paths.scratch_root("clip_patch", asset)
    exp, prev, pal = work / "export", work / "previews", work / "palettes"
    for d in (exp, prev, pal):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    shutil.copy2(real_export / "manifest.json", exp / "manifest.json")
    for f in real_pal.glob("*.json"):
        shutil.copy2(f, pal / f.name)
    env = dict(os.environ, AF_EXPORT_ROOT=str(exp), AF_PREVIEW_ROOT=str(prev), AF_PALETTE_ROOT=str(pal),
               EGL_PLATFORM=os.environ.get("EGL_PLATFORM", "surfaceless"))
    log = work / "ship.log"
    print(f"[clip_patch] shipping {script.name} into {work} (log {log.name}) …", flush=True)
    with open(log, "w") as fh:
        r = subprocess.run([sys.executable, str(script), *ship_args], env=env, stdout=fh, stderr=subprocess.STDOUT)
    tail = log.read_text(errors="replace").splitlines()[-25:]
    if r.returncode != 0:
        print("\n".join(tail))
        raise SystemExit(f"[clip_patch] the scratch ship failed (exit {r.returncode}); nothing changed")

    old = json.loads((real_export / "manifest.json").read_text())["assets"][asset]
    new = json.loads((exp / "manifest.json").read_text())["assets"][asset]
    old_clips = {c["name"]: c for c in old["anims"]}
    new_clips = {c["name"]: c for c in new["anims"]}
    problems = []
    for name, oc in old_clips.items():
        nc = new_clips.get(name)
        if nc is None:
            problems.append(f"clip {name} is shipped but the generator no longer makes it")
            continue
        if name in replace:
            print(f"  {name:16s} REPLACED (--replace: not compared)")
            continue
        for f in CLIP_FIELDS:
            if oc.get(f) != nc.get(f):
                problems.append(f"clip {name}: {f} {oc.get(f)!r} -> {nc.get(f)!r}")
        ok, worst = curves_equal(exp / nc["fbx"], real_export / oc["fbx"])
        print(f"  {name:16s} curves " + ("SAME" if ok else "CHANGED") + "  " +
              "  ".join(f"{k.split()[1][:5]} {v:.2g}" for k, v in worst.items()))
        if not ok:
            problems.append(f"clip {name}: curves changed {worst}")
    for name in replace:
        if name not in old_clips:
            problems.append(f"--replace {name}: not shipped (list it as a new clip)")
        elif name in clips:
            problems.append(f"{name} is both new and --replace")
    for name in clips:
        if name not in new_clips:
            problems.append(f"the generator made no clip {name}")
        elif name in old_clips:
            problems.append(f"clip {name} is already shipped (re-ship the asset in full to change it, or --replace "
                            f"a clip a clip_patch added)")
    extra_new = [n for n in new_clips if n not in old_clips and n not in clips]
    if extra_new:
        problems.append(f"the generator makes clips not asked for: {extra_new}")
    for f in ("bones", "triangles", "skeleton", "heightCm"):
        if old.get(f) != new.get(f):
            problems.append(f"entry {f}: {old.get(f)!r} -> {new.get(f)!r}")
    for f in pal.glob("*.json"):
        rf = real_pal / f.name
        if not rf.exists() or json.loads(rf.read_text()) != json.loads(f.read_text()):
            problems.append(f"palette registry {f.name} changed (a swatch was appended): re-ship in full")
    if problems:
        print("[clip_patch] ABORT:\n  " + "\n  ".join(problems))
        return 1

    pdir = Path(new["previews"][0]).parts[0] if new.get("previews") else None
    taken_clips = clips + replace
    pats = ([f"anim_{c}.png" for c in taken_clips] + [f"anim_back_{c}.png" for c in taken_clips] + ["game_w2*.png"]
            + globs)
    taken = []
    if pdir is not None:
        for p in sorted((prev / pdir).glob("*.png")):
            if any(fnmatch.fnmatch(p.name, g) for g in pats):
                taken.append(p)
    print(f"[clip_patch] new clips {clips}; replaced {replace}; keys {keys}; sockets {sockets}; previews "
          f"{[p.name for p in taken]}")
    if dry:
        return 0

    for name in taken_clips:
        src = exp / new_clips[name]["fbx"]
        dst = real_export / new_clips[name]["fbx"]
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    if pdir is not None:
        (real_prev / pdir).mkdir(parents=True, exist_ok=True)
        for p in taken:
            shutil.copy2(p, real_prev / pdir / p.name)
    man = export.Manifest()
    e = man.data["assets"][asset]
    e["anims"] = [c if c["name"] in replace else old_clips.get(c["name"], c) for c in new["anims"]]
    for k in keys:
        if k in new:
            e[k] = new[k]
        else:
            e.pop(k, None)
    if sockets:
        by = {s["name"]: s for s in new.get("sockets", [])}
        socks = e.setdefault("sockets", [])
        for s_name in sockets:
            if s_name not in by:
                raise SystemExit(f"[clip_patch] the scratch ship has no socket {s_name}")
            for j, s in enumerate(socks):
                if s["name"] == s_name:
                    socks[j] = by[s_name]
                    break
            else:
                socks.append(by[s_name])
    if pdir is not None:
        e["previews"] = sorted(set(e.get("previews", [])) | {f"{pdir}/{p.name}" for p in taken})
    man.save()
    print(f"[clip_patch] {asset}: +{len(clips)} clips, {len(replace)} replaced, manifest merged, {len(taken)} "
          f"previews")
    return 0


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(args))
