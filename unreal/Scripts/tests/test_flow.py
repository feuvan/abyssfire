"""Runs the whole content build twice against tests/fake_unreal.py (an in-memory `unreal` stand-in).

Checks the flow and the contract results (asset paths, skeleton sharing, slot assignment, sockets, additive clips,
outline sections without shadows, map file, MPC, masters) and that a second run with unchanged sources is a no-op.
Engine API names are checked separately (test_api_names.py).
"""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import fake_unreal  # noqa: E402
from abyss_content import manifest, paths  # noqa: E402


class FlowTest(unittest.TestCase):
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
        self.build = build_content
        self.report_path = tmp / "report.json"

    def tearDown(self) -> None:
        paths.GENERATED_DIR, paths.CONTENT_DIR, paths.RUNTIME_MANIFEST_COPY = self._saved
        sys.modules.pop("unreal", None)
        self.tmp.cleanup()

    def run_build(self, *extra: str):
        return self.build.run(["--report", str(self.report_path), *extra])

    def test_full_build_then_noop_rebuild(self):
        report = self.run_build()
        self.assertTrue(report.ok, "\n".join(report.errors))
        self.assertEqual(report.warnings, [], "\n".join(report.warnings))
        data = json.loads(self.report_path.read_text(encoding="utf-8"))
        self.assertTrue(data["ok"])
        self.assertTrue(self.report_path.with_suffix(".md").is_file())

        reg = fake_unreal.REGISTRY
        root = paths.CONTENT_ROOT
        # skeletal mesh, shared skeleton, LOD, sockets, materials, shadows
        sk = reg[f"{root}/Characters/SK_Hero_Warrior"]
        skel = reg[f"{root}/Skeletons/SKEL_Human"]
        self.assertIs(sk.get_editor_property("skeleton"), skel)
        self.assertNotIn(f"{root}/Characters/SK_Hero_Warrior_Skeleton", reg)
        slots = sk.get_editor_property("materials")
        self.assertIs(slots[0].get_editor_property("material_interface"), reg[f"{paths.INSTANCES_DIR}/MI_AF_Toon_Heroes"])
        self.assertIs(slots[1].get_editor_property("material_interface"),
                      reg[f"{paths.INSTANCES_DIR}/MI_AF_Outline_Heroes_Hero"])
        self.assertFalse(sk._shadow[(0, 1)])
        self.assertNotIn((0, 0), sk._shadow)
        sockets = {str(s.get_editor_property("socket_name")) for s in sk._sockets}
        warrior = self.plan.asset("SK_Hero_Warrior")
        self.assertEqual(sockets, {s.name for s in warrior.sockets})
        infos = sk.get_editor_property("lod_info")
        self.assertEqual(len(infos), 2)
        self.assertAlmostEqual(infos[1].get_editor_property("screen_size").default, 0.15)
        # animations on the shared skeleton; the additive hurt clip
        for clip in warrior.clips:
            seq = reg[f"{root}/{clip.folder}/{clip.asset}"]
            self.assertIs(seq.get_editor_property("skeleton"), skel)
            self.assertEqual(seq.get_editor_property("loop"), clip.loop)
        hurt = reg[f"{root}/Characters/A_Hero_Warrior_HurtAdd"]
        self.assertEqual(hurt.get_editor_property("additive_anim_type"), "AdditiveAnimationType.AAT_LOCAL_SPACE_BASE")
        self.assertEqual(hurt.get_editor_property("ref_pose_type"), "AdditiveBasePoseType.ABPT_LOCAL_ANIM_FRAME")
        # static meshes: slots, sockets from the manifest, outline without shadow
        sword = reg[f"{root}/Weapons/SM_Hero_Warrior_Sword"]
        # the fake imports the static slots in reverse order: matched by name, not by index
        self.assertIs(sword.get_material(0), reg[f"{paths.INSTANCES_DIR}/MI_AF_Outline_Heroes_Weapon"])
        self.assertIs(sword.get_material(1), reg[f"{paths.INSTANCES_DIR}/MI_AF_Toon_Heroes"])
        self.assertEqual({str(s.get_editor_property("socket_name")) for s in sword._sockets}, {"tip", "mid", "guard"})
        self.assertFalse(sword._shadow[(0, 0)])
        self.assertNotIn((0, 1), sword._shadow)
        # instances bind the palette textures and the outline widths
        outline = reg[f"{paths.INSTANCES_DIR}/MI_AF_Outline_Heroes_Hero"]
        self.assertIs(outline.get_editor_property("parent"), reg[f"{paths.MATERIALS_DIR}/{paths.M_OUTLINE}"])
        self.assertEqual(outline._params[("s", "OutlinePx1080")], 3.5)
        self.assertEqual(outline._params[("s", "BakedWidthCm")], 1.8)
        self.assertIs(outline._params[("t", "PaletteBC")], reg[f"{paths.TEXTURES_DIR}/T_AF_Palette_Heroes_BC"])
        # the painted ground: MI_AF_Terrain_<Theme> = M_AF_Terrain_Painted with the manifest's layers + detail atlas
        # (imported even without the texture scan, which this flow skips)
        terrain_plans = [i for i in self.plan.instances.values() if i.parent == paths.M_TERRAIN_PAINTED]
        self.assertTrue(terrain_plans, "no painted terrain instance planned from manifest terrain.*")
        for inst in terrain_plans:
            mi = reg[f"{paths.INSTANCES_DIR}/{inst.name}"]
            self.assertIs(mi.get_editor_property("parent"), reg[f"{paths.MATERIALS_DIR}/{paths.M_TERRAIN_PAINTED}"])
            self.assertEqual(set(inst.textures), {paths.terrain_layer_param(t) for t in paths.TERRAIN_PAINT_TILES}
                             | {paths.TERRAIN_DETAILS_PARAM})
            for param, tex_name in inst.textures.items():
                self.assertIs(mi._params[("t", param)], reg[f"{paths.TEXTURES_DIR}/{tex_name}"])
            atlas = reg[f"{paths.TEXTURES_DIR}/{inst.textures[paths.TERRAIN_DETAILS_PARAM]}"]
            self.assertEqual(atlas.get_editor_property("compression_settings"), "TextureCompressionSettings.TC_BC7")
            self.assertEqual(atlas.get_editor_property("address_x"), "TextureAddress.TA_CLAMP")
        # every master material exists and has outputs; the MPC carries the key light
        from abyss_content.ue import materials as mats
        for m in mats.MASTERS:
            mat = reg[f"{m.folder}/{m.name}"]
            self.assertTrue(mat._expressions, m.name)
            self.assertTrue(mat._outputs, m.name)
        toon = reg[f"{paths.MATERIALS_DIR}/{paths.M_TOON}"]
        self.assertIn("MaterialProperty.MP_EMISSIVE_COLOR", toon._outputs)
        self.assertIn("MaterialProperty.MP_OPACITY_MASK", toon._outputs)
        mpc = reg[f"{paths.MATERIALS_DIR}/{paths.MPC_NAME}"]
        names = {str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("vector_parameters")}
        slots = {f"Light{i}{k}" for i in range(8) for k in ("PosR", "Col")}   # toon light slots (UAbyssVfxSystem)
        self.assertEqual(names, {"SunDir", "KeyLightDir", "SunColor", "Ambient", "RimColor"} | slots)
        scalars = {str(p.get_editor_property("parameter_name")): p.get_editor_property("default_value")
                   for p in mpc.get_editor_property("scalar_parameters")}
        self.assertEqual(set(scalars), {"CameraTanHalfFovY", "TimeSec", "SunScale", "InkScale"})
        self.assertEqual((scalars["SunScale"], scalars["InkScale"]), (0.55, 1.5))
        # the shadow-catcher toon: emissive + lit base colour from AF_ToonSplit's additional output, normal = key light
        self.assertEqual(toon._output_pins["MaterialProperty.MP_BASE_COLOR"], "Lit")
        self.assertIs(toon._outputs["MaterialProperty.MP_BASE_COLOR"], toon._outputs["MaterialProperty.MP_EMISSIVE_COLOR"])
        for prop in ("MP_NORMAL", "MP_SPECULAR", "MP_METALLIC", "MP_ROUGHNESS"):
            self.assertIn(f"MaterialProperty.{prop}", toon._outputs)
        custom = toon._outputs["MaterialProperty.MP_EMISSIVE_COLOR"]
        self.assertEqual([str(o.get_editor_property("output_name")) for o in custom.get_editor_property(
            "additional_outputs")], ["Lit"])
        self.assertEqual(custom.get_editor_property("description"), "AF_ToonSplit")
        # the selection rim instance and the art-pass look parameters
        rim = reg[f"{paths.INSTANCES_DIR}/{paths.MI_SELECT_RIM}"]
        self.assertIs(rim.get_editor_property("parent"), reg[f"{paths.MATERIALS_DIR}/{paths.M_SELECT_RIM}"])
        self.assertEqual(rim._params[("s", "RimPx720")], 2.0)
        heroes = reg[f"{paths.INSTANCES_DIR}/MI_AF_Toon_Heroes"]
        self.assertEqual(heroes._params[("s", "CastShadowStrength")], 0.7)
        # utility content
        self.assertIn(f"{paths.FX_DIR}/{paths.SM_FX_QUAD}", reg)
        self.assertIn(f"{paths.FX_TEXTURES_DIR}/{paths.T_DEFAULT_GLOW}", reg)
        self.assertIn(f"{paths.UI_PORTRAITS_DIR}/T_UI_Portrait_Hero_Warrior", reg)
        self.assertTrue((paths.CONTENT_DIR / "Abyssfire" / "Maps" / "L_Main.umap").is_file())
        self.assertTrue(any(p.startswith(paths.AUDIO_ROOT) for p in reg), "audio import did not run")

        # second run: nothing changed -> no import, no material rebuild
        imports, compiles = fake_unreal.STATE["imports"], fake_unreal.STATE["compiles"]
        report2 = self.run_build()
        self.assertTrue(report2.ok, "\n".join(report2.errors))
        self.assertEqual(fake_unreal.STATE["imports"], imports, "unchanged sources were re-imported")
        self.assertEqual(fake_unreal.STATE["compiles"], compiles, "unchanged materials were rebuilt")
        actions = {e["action"] for e in report2.assets.values()}
        self.assertLessEqual(actions, {"unchanged"}, f"second run actions: {actions}")

    def test_force_materials_rebuilds_only_materials(self):
        self.assertTrue(self.run_build("--skip", "audio").ok)
        imports, compiles = fake_unreal.STATE["imports"], fake_unreal.STATE["compiles"]
        report = self.run_build("--only", "materials", "--force-materials")
        self.assertTrue(report.ok, "\n".join(report.errors))
        self.assertEqual(fake_unreal.STATE["imports"], imports)
        from abyss_content.ue import materials as mats
        self.assertEqual(fake_unreal.STATE["compiles"] - compiles, len(mats.MASTERS))

    def test_family_filter(self):
        report = self.run_build("--family", "weapons", "--skip", "audio,level")
        self.assertTrue(report.ok, "\n".join(report.errors))
        self.assertNotIn(f"{paths.CONTENT_ROOT}/Characters/SK_Hero_Warrior", fake_unreal.REGISTRY)
        self.assertIn(f"{paths.CONTENT_ROOT}/Weapons/SM_Hero_Warrior_Sword", fake_unreal.REGISTRY)


if __name__ == "__main__":
    unittest.main()
