"""Planner tests: the real art manifest plus synthetic manifests for the edge cases (no `unreal` needed)."""
from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from abyss_content import manifest, paths, pngio  # noqa: E402


class RealManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        if not paths.MANIFEST_PATH.is_file():
            self.skipTest("no Art/Export/manifest.json")
        self.plan = manifest.build_plan()

    def test_plan_is_valid(self):
        self.assertEqual(self.plan.errors, [], "\n".join(map(str, self.plan.errors)))

    def test_contract_paths(self):
        for asset in self.plan.assets:
            rel = asset.fbx.relative_to(self.plan.export_dir).as_posix()
            self.assertEqual(asset.content_path, f"/Game/Abyssfire/{rel.rsplit('/', 1)[0]}/{asset.name}")
            for clip in asset.clips:
                crel = clip.fbx.relative_to(self.plan.export_dir).as_posix()
                self.assertEqual(clip.content_path, f"/Game/Abyssfire/{crel.rsplit('/', 1)[0]}/{clip.asset}")

    def test_every_slot_has_a_material(self):
        for asset in self.plan.assets:
            mapping = self.plan.slot_materials[asset.name]
            self.assertEqual(sorted(mapping), [s.index for s in asset.slots])
            for name in mapping.values():
                self.assertTrue(name in self.plan.instances or name.startswith("M_AF_"), name)

    def test_outline_instances_carry_the_manifest_widths(self):
        for asset in self.plan.assets:
            for slot in asset.slots:
                if not slot.is_outline:
                    continue
                inst = self.plan.instances[self.plan.slot_materials[asset.name][slot.index]]
                self.assertEqual(inst.scalars["OutlinePx1080"], slot.px1080)
                self.assertEqual(inst.scalars["BakedWidthCm"], slot.width_cm)
                self.assertIn(inst.parent, paths.OUTLINE_MASTERS)

    def test_shading_contract(self):
        sh = self.plan.shading
        self.assertAlmostEqual(sum(x * x for x in sh.key_light), 1.0, places=5)
        self.assertAlmostEqual(sh.rim_band[0], 0.56 ** 4, places=4)
        self.assertAlmostEqual(sh.rim_band[1], 0.62 ** 4, places=4)

    def test_textures_classified(self):
        kinds = {t.name: t.kind for t in self.plan.textures}
        for pal in self.plan.palettes.values():
            self.assertEqual(kinds[pal.bc_name], "palette_bc")
            self.assertEqual(kinds[pal.p_name], "palette_p")
        self.assertFalse(any(n.startswith("T_AF_Palette_Test_") for n in kinds))


class SyntheticManifestTests(unittest.TestCase):
    BASE = {
        "schemaVersion": 1,
        "palettes": {"Mon": {"material": "MI_AF_Toon_Mon", "parent": "M_AF_Toon",
                             "baseColor": {"name": "T_AF_Palette_Mon_BC", "file": "Textures/T_AF_Palette_Mon_BC.png"},
                             "params": {"name": "T_AF_Palette_Mon_P", "file": "Textures/T_AF_Palette_Mon_P.png"}}},
        "assets": {
            "SK_Mon_Slime": {
                "kind": "SkeletalMesh", "category": "Monsters", "fbx": "Monsters/SK_Mon_Slime.fbx",
                "skeleton": "SKEL_Slime",
                "materialSlots": [
                    {"index": 0, "name": "MI_AF_Toon_Slime", "parent": "M_AF_Toon_Slime", "palette": "Mon",
                     "params": {"BodyOpacity": 0.85, "RimOverrideColor": "#E6FFDC"}},
                    {"index": 1, "name": "M_AF_Outline", "parent": "M_AF_Outline", "palette": "Mon",
                     "class": "small_monster"}],
                "anims": [
                    {"name": "HurtAdd", "asset": "A_Mon_Slime_HurtAdd", "fbx": "Monsters/A_Mon_Slime_HurtAdd.fbx",
                     "lengthMs": 300, "frames": 18, "fps": 60, "additive": True,
                     "additiveBase": {"type": "AnimFrame", "anim": "A_Mon_Slime_Idle", "frame": 0}},
                    {"name": "Idle", "asset": "A_Mon_Slime_Idle", "fbx": "Monsters/A_Mon_Slime_Idle.fbx",
                     "lengthMs": 1000, "frames": 60, "fps": 60, "loop": True}],
                "sockets": [{"name": "fx_chest", "bone": "body_mid", "relLocCm": [0, 10, 0], "relRotDeg": [0, 0, 0]}],
            },
            "SM_Foliage_Plains_Oak_A": {
                "kind": "StaticMesh", "category": "Foliage", "fbx": "Foliage/SM_Foliage_Plains_Oak_A.fbx",
                "materialSlots": [
                    {"index": 0, "name": "MI_AF_ToonFoliage_Mon", "parent": "M_AF_Toon_Foliage", "palette": "Mon"},
                    {"index": 1, "name": "M_AF_Outline", "parent": "M_AF_Outline", "palette": "Mon", "class": "decor"}],
            },
        },
    }

    def make(self, data: dict) -> Path:
        root = Path(self.tmp.name)
        for rel in ("Textures/T_AF_Palette_Mon_BC.png", "Textures/T_AF_Palette_Mon_P.png"):
            (root / rel).parent.mkdir(parents=True, exist_ok=True)
            pngio.write_png(root / rel, 2, 2, bytes(16))
        for asset in data["assets"].values():
            for rel in [asset["fbx"]] + [a["fbx"] for a in asset.get("anims", [])]:
                (root / rel).parent.mkdir(parents=True, exist_ok=True)
                (root / rel).write_bytes(b"fbx")
        path = root / "manifest.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return path

    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_variants_params_and_additive_bases(self):
        plan = manifest.build_plan(self.make(copy.deepcopy(self.BASE)))
        self.assertEqual([p for p in plan.problems if p.level == "error"], [])
        slime = plan.instances["MI_AF_Toon_Slime"]
        self.assertEqual(slime.parent, "M_AF_Toon_Slime")
        self.assertEqual(slime.scalars, {"BodyOpacity": 0.85})
        self.assertAlmostEqual(slime.vectors["RimOverrideColor"][1], 1.0)
        # the foliage mesh's hull gets the swaying outline master
        oak = plan.slot_materials["SM_Foliage_Plains_Oak_A"]
        self.assertEqual(plan.instances[oak[1]].parent, "M_AF_Outline_Foliage")
        self.assertEqual(oak[1], "MI_AF_OutlineFoliage_Mon_Decor")
        self.assertEqual(plan.instances[oak[0]].parent, "M_AF_Toon_Foliage")
        hurt = next(c for c in plan.asset("SK_Mon_Slime").clips if c.asset == "A_Mon_Slime_HurtAdd")
        self.assertEqual(hurt.additive_base_type, "ABPT_ANIM_FRAME")
        self.assertEqual(hurt.additive_base_anim, "A_Mon_Slime_Idle")
        self.assertEqual(plan.skeleton_families, {"SKEL_Slime": ["SK_Mon_Slime"]})

    def test_conflicting_instance_definitions_are_errors(self):
        data = copy.deepcopy(self.BASE)
        # the palette instance MI_AF_Toon_Mon (parent M_AF_Toon) reused with another parent
        data["assets"]["SM_Foliage_Plains_Oak_A"]["materialSlots"][0]["name"] = "MI_AF_Toon_Mon"
        plan = manifest.build_plan(self.make(data))
        self.assertTrue(any("defined twice" in p.message for p in plan.errors), [str(p) for p in plan.problems])

    def test_missing_files_and_bad_names(self):
        data = copy.deepcopy(self.BASE)
        path = self.make(data)
        (path.parent / "Monsters/A_Mon_Slime_Idle.fbx").unlink()
        data["assets"]["bad name"] = {"kind": "StaticMesh", "fbx": "Props/bad name.fbx"}
        path.write_text(json.dumps(data), encoding="utf-8")
        plan = manifest.build_plan(path)
        messages = [str(p) for p in plan.errors]
        self.assertTrue(any("clip FBX missing" in m for m in messages), messages)
        self.assertTrue(any("not a valid UE asset name" in m for m in messages), messages)

    def test_game_id_variants_share_footprint(self):
        data = copy.deepcopy(self.BASE)
        oak_b = copy.deepcopy(data["assets"]["SM_Foliage_Plains_Oak_A"])
        oak_b["fbx"] = "Foliage/SM_Foliage_Plains_Oak_B.fbx"
        data["assets"]["SM_Foliage_Plains_Oak_B"] = oak_b
        for name in ("SM_Foliage_Plains_Oak_A", "SM_Foliage_Plains_Oak_B"):
            data["assets"][name].update({"gameIds": ["tree"], "footprintTiles": [1, 1], "blocking": True})
        plan = manifest.build_plan(self.make(copy.deepcopy(data)))
        self.assertFalse(any("game id tree" in str(p) for p in plan.errors), [str(p) for p in plan.problems])
        # W5: the core bakes the first variant's footprint, the UE may show the other one
        data["assets"]["SM_Foliage_Plains_Oak_B"]["footprintTiles"] = [2, 2]
        plan = manifest.build_plan(self.make(data))
        self.assertTrue(any("game id tree" in str(p) and "disagree" in str(p) for p in plan.errors),
                        [str(p) for p in plan.problems])

    def test_schema_version(self):
        data = copy.deepcopy(self.BASE)
        data["schemaVersion"] = 2
        with self.assertRaises(manifest.ManifestError):
            manifest.build_plan(self.make(data))

    def test_family_filter(self):
        plan = manifest.build_plan(self.make(copy.deepcopy(self.BASE)), families=["foliage"])
        self.assertEqual([a.name for a in plan.assets], ["SM_Foliage_Plains_Oak_A"])
        self.assertIn("SK_Mon_Slime", plan.skipped)


class PngTests(unittest.TestCase):
    def test_roundtrip_and_determinism(self):
        rgba = bytes(range(256)) * 4  # 16 x 16 pixels
        data = pngio.encode_png(16, 16, rgba)
        self.assertEqual(pngio.decode_png_rgba(data), (16, 16, rgba))
        self.assertEqual(data, pngio.encode_png(16, 16, rgba))

    def test_generated_textures(self):
        gens = pngio.default_textures("/Game/X", "/Game/Y")
        names = {g.name for g in gens}
        self.assertEqual(names, {paths.T_DEFAULT_WHITE, paths.T_DEFAULT_PALETTE_P, paths.T_DEFAULT_TILE_IDS,
                                 paths.T_DEFAULT_GLOW})
        glow = next(g for g in gens if g.name == paths.T_DEFAULT_GLOW)
        self.assertEqual(len(glow.rgba), 64 * 64 * 4)
        centre = glow.rgba[(32 * 64 + 32) * 4 + 3]
        corner = glow.rgba[3]
        self.assertGreater(centre, 240)
        self.assertEqual(corner, 0)
        pal = next(g for g in gens if g.name == paths.T_DEFAULT_PALETTE_P)
        self.assertEqual(tuple(pal.rgba[:4]), (107, 82, 0, 255))

    def test_glow_falloff_matches_the_web_stops(self):
        for r, a in pngio.GLOW_STOPS:
            self.assertAlmostEqual(pngio.glow_falloff(r), a)
        self.assertAlmostEqual(pngio.glow_falloff(0.275), 0.75)

    def test_read_info(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "a.png"
            pngio.write_png(p, 3, 2, bytes(24))
            info = pngio.read_png_info(p)
            self.assertEqual((info.width, info.height, info.channels, info.has_alpha), (3, 2, 4, True))


if __name__ == "__main__":
    unittest.main()
