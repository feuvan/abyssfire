"""Engine-version behaviour of the editor-side build against tests/fake_unreal.py (UE 5.8 by default):

* material-instance parameters are verified by reading them back (5.8 setters apply but return False), with the C++
  helper as the fallback and a clear error when it is unavailable; a changed instance is saved either way;
* an editor binary older than the helpers the scripts call is one warning, not one per asset;
* a NullRHI commandlet lists the masters as not compiled (one note); --shaders reads real statistics / compile errors;
* Interchange properties a version renamed or dropped are set by their replacement and reported once per build;
* clips import an explicit frame range from the manifest (the keyed range and one note where the engine cannot set it).
"""
from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import fake_unreal  # noqa: E402
from abyss_content import manifest, paths  # noqa: E402


class EditorTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        tmp = Path(self.tmp.name)
        self._saved = (paths.GENERATED_DIR, paths.CONTENT_DIR, paths.RUNTIME_MANIFEST_COPY)
        paths.GENERATED_DIR = tmp / "Generated"
        paths.CONTENT_DIR = tmp / "Content"
        paths.RUNTIME_MANIFEST_COPY = tmp / "Data" / "assets.json"   # absent: the build does not depend on Data/
        self.plan = manifest.build_plan(scan_textures=False)
        anim_meta = {c.asset: (c.length_ms, c.frames) for a in self.plan.assets for c in a.clips}
        fake_unreal.reset(paths.CONTENT_DIR, paths.UNREAL_DIR, anim_meta)
        sys.modules["unreal"] = fake_unreal.MOD
        for name in list(sys.modules):
            if name.startswith("abyss_content.ue") or name in ("build_content", "abyss_import_audio"):
                del sys.modules[name]
        import build_content
        from abyss_content.ue import core, materials, meshes
        self.build, self.core, self.materials, self.meshes = build_content, core, materials, meshes
        self.report_path = tmp / "report.json"
        env = mock.patch.dict(os.environ, {k: v for k, v in os.environ.items() if k != core.NULLRHI_ENV}, clear=True)
        env.start()
        self.addCleanup(env.stop)

    def tearDown(self) -> None:
        paths.GENERATED_DIR, paths.CONTENT_DIR, paths.RUNTIME_MANIFEST_COPY = self._saved
        sys.modules.pop("unreal", None)
        self.tmp.cleanup()

    def run_build(self, *extra: str):
        return self.build.run(["--report", str(self.report_path), *extra])

    def compat(self, report, needle: str) -> list[tuple[str, list[str]]]:
        return [(k, v) for k, v in report.compat.items() if needle in k]


class InstanceParameterTests(EditorTestCase):
    def instance(self):
        from abyss_content.report import BuildReport
        ed = self.core.Editor()
        parent = fake_unreal.register(fake_unreal.Material(), f"{paths.MATERIALS_DIR}/{paths.M_TOON}")
        mel = fake_unreal.MaterialEditingLibrary
        for cls, name in (("MaterialExpressionTextureSampleParameter2D", "PaletteBC"),
                          ("MaterialExpressionScalarParameter", "BakedWidthCm"),
                          ("MaterialExpressionScalarParameter", "BodyOpacity"),
                          ("MaterialExpressionVectorParameter", "Tint")):
            mel.create_material_expression(parent, getattr(fake_unreal.MOD, cls)).set_editor_property(
                "parameter_name", name)
        mi = fake_unreal.register(fake_unreal.MaterialInstanceConstant(), f"{paths.INSTANCES_DIR}/MI_Test")
        tex = fake_unreal.register(fake_unreal.Texture2D(), f"{paths.TEXTURES_DIR}/T_Test")
        return ed, parent, mi, tex, BuildReport()

    def test_58_setter_false_is_accepted_after_reading_back(self):
        ed, parent, mi, tex, report = self.instance()
        set_param = self.materials.set_instance_parameter
        for kind, param, value in (("texture", "PaletteBC", tex), ("scalar", "BakedWidthCm", 1.8),
                                   ("vector", "Tint", (0.25, 0.5, 0.75, 1.0))):
            self.assertTrue(set_param(ed, mi, parent, "MI_Test", kind, param, value, report), kind)
            self.assertFalse(set_param(ed, mi, parent, "MI_Test", kind, param, value, report), f"{kind} again")
        self.assertIs(mi._params[("t", "PaletteBC")], tex)
        self.assertEqual(mi._params[("s", "BakedWidthCm")], 1.8)
        self.assertEqual(mi._params[("v", "Tint")], fake_unreal.LinearColor(0.25, 0.5, 0.75, 1.0))
        self.assertEqual(report.compat, {self.materials.SETTER_RETURNED_FALSE: ["MI_Test"]})
        self.assertEqual(report.warnings, [])

    def test_value_not_applied_goes_through_the_helper(self):
        ed, parent, mi, tex, report = self.instance()
        fake_unreal.STATE["mel_refuse"] = {"PaletteBC", "BakedWidthCm", "Tint"}
        for kind, param, value in (("texture", "PaletteBC", tex), ("scalar", "BakedWidthCm", 1.8),
                                   ("vector", "Tint", (0.25, 0.5, 0.75, 1.0))):
            changed = self.materials.set_instance_parameter(ed, mi, parent, "MI_Test", kind, param, value, report)
            self.assertTrue(changed, kind)
        self.assertIs(mi._params[("t", "PaletteBC")], tex)
        self.assertEqual(mi._params[("s", "BakedWidthCm")], 1.8)
        self.assertEqual(len(report.warnings), 3)
        self.assertTrue(all("set through the C++ helper" in w for w in report.warnings), report.warnings)

    def test_helper_unavailable_is_a_clear_error(self):
        fake_unreal.use_content_library("stale")
        ed, parent, mi, tex, report = self.instance()
        fake_unreal.STATE["mel_refuse"] = {"PaletteBC", "BodyOpacity"}
        with self.assertRaises(self.core.BuildError) as cm:
            self.materials.set_instance_parameter(ed, mi, parent, "MI_Test", "texture", "PaletteBC", tex, report)
        msg = str(cm.exception)
        self.assertIn("helper unavailable", msg)
        self.assertIn("infos=helper unavailable", msg)
        self.assertNotIn("infos=[]", msg)
        self.assertIn("returned False", msg)
        with self.assertRaises(self.core.BuildError) as cm:
            self.materials.set_instance_parameter(ed, mi, parent, "MI_Test", "scalar", "BodyOpacity", 0.82, report)
        self.assertIn("scalar parameter BodyOpacity not applied", str(cm.exception))

    def test_parameter_the_parent_lacks_is_an_error_before_any_setter(self):
        # An instance keeps an override for any name (MaterialEditingLibrary on some versions, the helper's
        # Set*ParameterValueEditorOnly always): it would read back as applied and do nothing.
        ed, parent, mi, tex, report = self.instance()
        fake_unreal.STATE["mel_refuse"] = {"WindStrength"}
        for kind, param, value in (("scalar", "WindStrength", 0.5), ("vector", "WindDir", (1.0, 0.0, 0.0, 1.0)),
                                   ("texture", "PaletteX", tex), ("scalar", "Zero", 0.0)):
            with self.assertRaises(self.core.BuildError) as cm:
                self.materials.set_instance_parameter(ed, mi, parent, "MI_Test", kind, param, value, report)
            self.assertIn(f"parent has no {kind} parameter {param}", str(cm.exception))
        self.assertEqual(mi._params, {})
        self.assertEqual(report.warnings + list(report.compat), [])
        self.assertIn("editor=['BakedWidthCm', 'BodyOpacity']", str(cm.exception))

    def test_unreadable_parent_names_leave_the_read_back_in_charge(self):
        ed, parent, mi, tex, report = self.instance()
        with mock.patch.object(fake_unreal.MaterialEditingLibrary, "get_scalar_parameter_names",
                               side_effect=RuntimeError("no such function")):
            self.assertTrue(self.materials.set_instance_parameter(ed, mi, parent, "MI_Test", "scalar", "Gloss", 0.3,
                                                                  report))
        self.assertEqual(mi._params[("s", "Gloss")], 0.3)

    def test_instances_saved_although_setters_return_false(self):
        report = self.run_build("--only", "textures,mpc,materials,instances")
        self.assertTrue(report.ok, "\n".join(report.errors))
        self.assertEqual(report.warnings, [], "\n".join(report.warnings))
        saved = set(fake_unreal.STATE["saved"])
        for name, inst in self.plan.instances.items():
            mi = fake_unreal.REGISTRY[f"{paths.INSTANCES_DIR}/{name}"]
            self.assertIn(mi._package, saved, name)
            for param, value in inst.scalars.items():
                self.assertAlmostEqual(mi._params[("s", param)], value, msg=f"{name}.{param}")
            for param, tex_name in inst.textures.items():
                self.assertIs(mi._params[("t", param)], fake_unreal.REGISTRY[f"{paths.TEXTURES_DIR}/{tex_name}"])
        (note, subjects), = self.compat(report, "returned False")
        self.assertEqual(sorted(subjects), sorted(self.plan.instances))

    def test_failed_parameter_keeps_the_others_and_saves(self):
        fake_unreal.use_content_library("stale")
        fake_unreal.STATE["mel_refuse"] = {"BodyOpacity"}
        report = self.run_build("--only", "textures,mpc,materials,instances")
        self.assertEqual(len(report.errors), 1, report.errors)
        self.assertIn("MI_AF_Toon_Slime: scalar parameter BodyOpacity not applied", report.errors[0])
        self.assertIn("helper unavailable", report.errors[0])
        slime = fake_unreal.REGISTRY[f"{paths.INSTANCES_DIR}/MI_AF_Toon_Slime"]
        self.assertIn(("t", "PaletteBC"), slime._params)
        self.assertIn(slime._package, fake_unreal.STATE["saved"])


class StaleEditorTests(EditorTestCase):
    def test_stale_binary_is_one_warning(self):
        fake_unreal.use_content_library("stale")
        report = self.run_build("--skip", "audio")
        self.assertTrue(report.ok, "\n".join(report.errors))
        self.assertEqual(len(report.warnings), 1, "\n".join(report.warnings))
        warning = report.warnings[0]
        self.assertIn("STALE EDITOR BINARY", warning)
        missing = [f for f in self.core.CONTENT_LIB_FUNCTIONS if f not in fake_unreal.STALE_LIBRARY_FUNCTIONS]
        for name in missing:
            self.assertIn(name, warning)
        for name in fake_unreal.STALE_LIBRARY_FUNCTIONS:
            self.assertNotIn(name, warning)
        self.assertIn("Build.sh\" AbyssfireEditor", warning)
        self.assertIn("Abyssfire.uproject", warning)
        (note, subjects), = self.compat(report, "LOD screen sizes left at the engine defaults")
        self.assertEqual(sorted(subjects), sorted(a.name for a in self.plan.assets if a.lods))
        notes = [n for s in report.steps for n in s.notes]
        self.assertFalse([n for n in notes if "unavailable" in n and "warning" in n], notes)
        self.assertEqual(1, sum("STALE EDITOR BINARY" in m for kind, m in fake_unreal.LOG if kind == "warning"))

    def test_library_missing_entirely(self):
        fake_unreal.use_content_library("none")
        report = self.run_build("--skip", "audio")
        self.assertEqual(len(report.warnings), 1, "\n".join(report.warnings))
        self.assertIn("not loaded at all", report.warnings[0])
        (note, subjects), = self.compat(report, "skeletal sockets not created")
        self.assertIn("SK_Hero_Warrior", subjects)

    def test_current_binary_is_silent(self):
        ed = self.core.Editor()
        self.assertEqual(ed.missing_helpers, [])
        self.assertIsNone(self.core.stale_editor_warning(ed))
        with self.assertRaises(self.core.BuildError):
            ed.helper("not_a_listed_helper")


class ShaderCheckTests(EditorTestCase):
    def masters(self) -> list[str]:
        return [m.name for m in self.materials.MASTERS]

    def test_nullrhi_commandlet_lists_masters_as_not_compiled(self):
        report = self.run_build("--only", "textures,mpc,materials,verify")
        self.assertTrue(report.ok, "\n".join(report.errors))
        self.assertEqual(report.warnings, [], "\n".join(report.warnings))
        self.assertEqual(sorted(report.materials), sorted(self.masters()))
        self.assertEqual({m["status"] for m in report.materials.values()}, {self.materials.NOT_COMPILED})
        verify = next(s for s in report.steps if s.name == "verify")
        self.assertEqual(sum("NullRHI" in n for n in verify.notes), 1, verify.notes)

    def test_shaders_flag_under_nullrhi_is_an_error(self):
        report = self.run_build("--only", "textures,mpc,materials,verify", "--shaders")
        self.assertFalse(report.ok)
        self.assertTrue(any("--shaders" in e and "AllowCommandletRendering" in e for e in report.errors), report.errors)

    def test_rendering_session_reports_compile_errors(self):
        fake_unreal.STATE["command_line"] += " -AllowCommandletRendering"
        fake_unreal.STATE["compile_errors"] = {paths.M_TOON: ["/Engine/Generated/Material.ush(12): error: AF_Toon"]}
        report = self.run_build("--only", "textures,mpc,materials,verify", "--shaders")
        self.assertEqual(report.errors, [f"{paths.M_TOON}: shader compile error: "
                                         "/Engine/Generated/Material.ush(12): error: AF_Toon"])
        self.assertEqual(report.materials[paths.M_TOON]["status"], "COMPILE ERRORS (1)")
        others = {m["status"] for n, m in report.materials.items() if n != paths.M_TOON}
        self.assertEqual(others, {"ok"})
        self.assertEqual(report.materials[paths.M_OUTLINE]["ps"], 42)

    def test_env_override(self):
        with mock.patch.dict(os.environ, {self.core.NULLRHI_ENV: "0"}):
            self.assertFalse(self.core.shaders_disabled())
            report = self.run_build("--only", "textures,mpc,materials,verify")
        # the fake still compiled nothing: every master is now a "no pixel shader" warning
        self.assertEqual(len(report.warnings), len(self.masters()))
        fake_unreal.STATE["command_line"] += " -AllowCommandletRendering"
        self.assertFalse(self.core.shaders_disabled())
        with mock.patch.dict(os.environ, {self.core.NULLRHI_ENV: "1"}):
            self.assertTrue(self.core.shaders_disabled())
        fake_unreal.STATE["command_line"] = "-nullrhi"
        self.assertTrue(self.core.shaders_disabled())
        fake_unreal.STATE["command_line"] = '"/p/Abyssfire.uproject" -game'
        self.assertFalse(self.core.shaders_disabled())


class InterchangeTests(EditorTestCase):
    def parts(self, stack) -> dict[str, object]:
        p = stack.pipelines[0]
        return {k: p.get_editor_property(v) for k, v in (
            ("common", "common_meshes_properties"),
            ("skeletal_common", "common_skeletal_meshes_and_animations_properties"),
            ("mesh", "mesh_pipeline"), ("anim", "animation_pipeline"))}

    def test_58_replacements(self):
        notes: list[str] = []
        parts = self.parts(self.meshes.make_pipeline("static", notes=notes))
        self.assertIs(parts["common"]._props["convert_statics_with_animated_transform_to_skeletals"], False)
        self.assertIs(parts["common"]._props["convert_statics_with_morph_targets_to_skeletals"], False)
        self.assertEqual(repr(parts["mesh"]._props["combine_static_meshes_behavior"]), "CombineBehavior.COMBINE_ALL")
        self.assertIs(parts["mesh"]._props["collision"], False)
        for part, name in (("common", "auto_detect_mesh_type"), ("mesh", "combine_static_meshes"),
                           ("mesh", "import_collision"),
                           ("skeletal_common", "convert_statics_with_morph_targets_to_skeletals")):
            self.assertNotIn(name, parts[part]._props, f"{part}.{name} touched")
        self.assertEqual(len(notes), 1, notes)
        self.assertIn("snap_to_closest_frame_boundary", notes[0])

    def test_56_spellings(self):
        fake_unreal.STATE["interchange_missing"] = dict(fake_unreal.INTERCHANGE_56)
        notes: list[str] = []
        parts = self.parts(self.meshes.make_pipeline("skeletal", notes=notes))
        self.assertIs(parts["common"]._props["auto_detect_mesh_type"], False)
        self.assertIs(parts["skeletal_common"]._props["convert_statics_with_morph_targets_to_skeletals"], False)
        self.assertIs(parts["mesh"]._props["combine_static_meshes"], True)
        self.assertIs(parts["mesh"]._props["collision"], False)
        self.assertNotIn("import_collision", parts["mesh"]._props)
        self.assertEqual(notes, [])

    def test_no_spelling_is_one_note(self):
        fake_unreal.STATE["interchange_missing"].update({
            "mesh_pipeline.collision": "Failed to find property", "mesh_pipeline.import_collision": "is deprecated"})
        notes: list[str] = []
        self.meshes.make_pipeline("static", notes=notes)
        (note,) = [n for n in notes if "collision" in n]
        self.assertIn("none of mesh.collision, mesh.import_collision is settable", note)

    def test_property_notes_are_aggregated_once_per_build(self):
        report = self.run_build("--skip", "audio")
        self.assertTrue(report.ok, "\n".join(report.errors))
        entries = self.compat(report, "Interchange")
        self.assertEqual(len(entries), 1, list(report.compat))
        note, subjects = entries[0]
        self.assertIn("snap_to_closest_frame_boundary", note)
        imported = [a.name for a in self.plan.assets] + [c.asset for a in self.plan.assets for c in a.clips]
        self.assertEqual(sorted(subjects), sorted(imported))
        notes = [n for s in report.steps for n in s.notes]
        self.assertFalse([n for n in notes if "not available" in n], notes[:5])
        md = self.report_path.with_suffix(".md").read_text(encoding="utf-8")
        self.assertIn(f"— {len(imported)} asset(s):", md)


class ClipRangeTests(EditorTestCase):
    def test_explicit_frame_range(self):
        skel = fake_unreal.Skeleton()
        anim = self.meshes.make_pipeline("anim", skel, 60, frames=37).pipelines[0].get_editor_property(
            "animation_pipeline")
        self.assertEqual(anim._props["animation_range"], "InterchangeAnimationRange.SET_RANGE")
        self.assertEqual(anim._props["frame_import_range"], fake_unreal.Int32Interval(0, 37))
        self.assertEqual(anim._props["custom_bone_animation_sample_rate"], 60)
        anim = self.meshes.make_pipeline("anim", skel, 60).pipelines[0].get_editor_property("animation_pipeline")
        self.assertEqual(anim._props["animation_range"], "InterchangeAnimationRange.ANIMATED")
        mesh = self.meshes.make_pipeline("static").pipelines[0].get_editor_property("animation_pipeline")
        self.assertNotIn("animation_range", mesh._props)

    def anim_part(self, frames: int, notes: list[str]):
        stack = self.meshes.make_pipeline("anim", fake_unreal.Skeleton(), 60, notes=notes, frames=frames)
        return stack.pipelines[0].get_editor_property("animation_pipeline")

    def test_range_property_missing_falls_back_to_the_keyed_range(self):
        fake_unreal.STATE["interchange_missing"]["animation_pipeline.frame_import_range"] = "Failed to find property"
        notes: list[str] = []
        anim = self.anim_part(37, notes)
        self.assertEqual(anim._props["animation_range"], "InterchangeAnimationRange.ANIMATED")
        (note,) = [n for n in notes if "clip range" in n]
        self.assertIn("clips import the ANIMATED range", note)
        self.assertIn("frame_import_range", note)

    def test_set_range_member_missing_falls_back_without_touching_the_range(self):
        has = self.meshes.has_enum
        with mock.patch.object(self.meshes, "has_enum", side_effect=lambda e, m: m != "SET_RANGE" and has(e, m)):
            notes: list[str] = []
            anim = self.anim_part(37, notes)
        self.assertEqual(anim._props["animation_range"], "InterchangeAnimationRange.ANIMATED")
        self.assertNotIn("frame_import_range", anim._props)
        self.assertEqual(len([n for n in notes if "clip range" in n]), 1, notes)

    def test_no_range_setting_at_all_is_a_note_not_an_error(self):
        fake_unreal.STATE["interchange_missing"]["animation_pipeline.animation_range"] = "Failed to find property"
        notes: list[str] = []
        self.anim_part(37, notes)
        (note,) = [n for n in notes if "clip range" in n]
        self.assertIn("default range", note)

    def test_unsettable_range_is_one_compat_line_for_the_build(self):
        fake_unreal.STATE["interchange_missing"]["animation_pipeline.frame_import_range"] = (
            "is deprecated: FrameImportRange is deprecated.")
        report = self.run_build("--only", "skeletal,anims")
        self.assertTrue(report.ok, "\n".join(report.errors))
        self.assertEqual(report.warnings, [], "\n".join(report.warnings))
        (note, subjects), = self.compat(report, "clip range")
        self.assertEqual(sorted(subjects), sorted(c.asset for a in self.plan.assets for c in a.clips))

    def test_every_clip_imports_its_manifest_frames(self):
        report = self.run_build("--only", "skeletal,anims")
        self.assertTrue(report.ok, "\n".join(report.errors))
        self.assertEqual(report.warnings, [], "\n".join(report.warnings))
        for asset in self.plan.assets:
            for clip in asset.clips:
                anim = fake_unreal.STATE["pipelines"][clip.asset].get_editor_property("animation_pipeline")
                self.assertEqual(anim.get_editor_property("frame_import_range").max, clip.frames, clip.asset)
                self.assertEqual(anim.get_editor_property("custom_bone_animation_sample_rate"), clip.fps)


class EmptySlotAssignTests(EditorTestCase):
    def test_dropped_outline_slot_is_not_expected(self):
        from abyss_content.report import BuildReport
        asset = manifest.Asset(name="SK_NPCKit_Specs", kind="SkeletalMesh", category="NPCs", folder="NPCs",
                               fbx=Path("SK_NPCKit_Specs.fbx"),
                               slots=[manifest.MaterialSlot(0, "MI_AF_Toon_NPC", "M_AF_Toon", "NPC")])
        report = BuildReport()
        self.assertEqual(self.materials.slot_index_map(asset, ["MI_AF_Toon_NPC"], report), {0: 0})
        self.assertEqual(report.errors + report.warnings, [])


if __name__ == "__main__":
    unittest.main()
