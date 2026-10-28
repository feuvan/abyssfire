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

    def test_painted_terrain_instance(self):
        """manifest terrain.plains -> MI_AF_Terrain_Plains (M_AF_Terrain_Painted), every layer + the detail atlas."""
        if "plains" not in (manifest.load_manifest(paths.MANIFEST_PATH).get("terrain") or {}):
            self.skipTest("no terrain.plains in the art manifest")
        inst = self.plan.instances[paths.terrain_instance_name("plains")]
        self.assertEqual(inst.name, "MI_AF_Terrain_Plains")
        self.assertEqual(inst.parent, paths.M_TERRAIN_PAINTED)
        self.assertEqual(inst.textures[paths.terrain_layer_param(0)], "T_Terrain_Plains_Grass_BC")
        self.assertEqual(inst.textures[paths.terrain_layer_param(3)], "T_Water_Plains_BC")
        self.assertEqual(inst.textures[paths.TERRAIN_DETAILS_PARAM], "T_Terrain_Plains_Details_BC")
        self.assertEqual(len(inst.textures), len(paths.TERRAIN_PAINT_TILES) + 1)
        kinds = {t.name: t.kind for t in self.plan.textures}
        self.assertEqual(kinds["T_Terrain_Plains_Details_BC"], "atlas")
        for tile in paths.TERRAIN_PAINT_TILES:
            self.assertEqual(kinds[inst.textures[paths.terrain_layer_param(tile)]], "world")
        # without the texture scan the terrain textures are still planned (the manifest names them)
        bare = manifest.build_plan(scan_textures=False)
        self.assertIn(inst.name, bare.instances)


class LookParameterTests(unittest.TestCase):
    """Art-pass instance parameters (DECISIONS P11 revised, R16, R19)."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.plan = manifest.build_plan(scan_textures=False)

    def test_cast_shadow_strength_by_family(self):
        want = {"Heroes": 0.7, "Monsters": 0.7, "NPC": 0.7, "Pets": 0.7, "Weapons": 0.7, "Pickups": 0.8, "Props": 1.0,
                "Foliage": 0.6}
        seen = set()
        for inst in self.plan.instances.values():
            if inst.parent not in (paths.M_TOON, paths.M_TOON_FOLIAGE):
                self.assertNotIn("CastShadowStrength", inst.scalars, inst.name)
                continue
            family = inst.palette.split("_")[0]
            expected = 1.0 if inst.name.endswith("_Ground") else want[family]
            self.assertEqual(inst.scalars.get("CastShadowStrength"), expected, inst.name)
            seen.add(family)
        self.assertEqual(seen, set(want), "every family has a toon instance in the art manifest")

    def test_ground_grade_on_the_plains(self):
        ground = self.plan.instances["MI_AF_Toon_Foliage_Plains_Ground"]
        self.assertEqual((ground.scalars["BaseSaturation"], ground.scalars["BaseValue"]), (0.85, 0.93))
        self.assertNotIn("BaseSaturation", self.plan.instances["MI_AF_Toon_Foliage_Plains"].scalars)
        terrain = self.plan.instances.get(paths.terrain_instance_name("plains"))
        if terrain is not None:
            self.assertEqual((terrain.scalars["GroundSaturation"], terrain.scalars["GroundValue"]), (0.85, 0.93))

    def test_select_rim_instance(self):
        rim = self.plan.instances[paths.MI_SELECT_RIM]
        self.assertEqual(rim.parent, paths.M_SELECT_RIM)
        self.assertEqual(rim.scalars, {"RimPx720": 2.0, "RimDepthPushCm": 30.0, "OutlinePx1080": 3.5,
                                       "BakedWidthCm": 1.8})
        self.assertEqual(rim.vectors["RimColor"], (*manifest.hex_to_linear("#FFF4DC"), 1.0))
        self.assertEqual(manifest.SELECT_RIM_STYLES["target"], ("#FF5A3C", 1.5))

    def test_telegraph_contract(self):
        """M_AF_Telegraph's constants (dark remake U2) from materials.M_AF_Telegraph; its textures are planned."""
        t = self.plan.telegraph
        self.assertTrue(t.present)
        self.assertEqual((t.fill_color, t.rim_color, t.front_color, t.flash_rim_color, t.wash_color),
                         ("#FFE8DC", "#D11F2E", "#FFF4EE", "#FF3A48", "#D11F2E"))
        self.assertEqual((t.fill_grain, t.fill_tile_cm, t.front_width_cm, t.rim_tile_cm, t.wash_fade_in_cm),
                         (0.3, 160.0, 5.0, 120.0, 60.0))
        self.assertEqual((t.rim_texture, t.fill_texture), ("T_FX_TelegraphRim", "T_FX_TelegraphFill"))
        for name in (t.rim_texture, t.fill_texture):
            entry = self.plan.texture(name)
            self.assertIsNotNone(entry, name)
            self.assertEqual(entry.folder, paths.FX_TEXTURES_DIR)
            self.assertFalse(entry.srgb)
        # the shipped defaults are the contract (the runtime keeps the same copy: AbyssTelegraphRules DefaultContract)
        self.assertEqual(manifest.TelegraphContract(present=True), t)

    def test_ash_contract(self):
        """M_AF_Toon_Ash / M_AF_Outline_Ash's constants (dark remake U3) from materials.M_AF_AshDissolve; the noise is
        planned without the texture scan."""
        a = self.plan.ash
        self.assertTrue(a.present)
        self.assertEqual((a.ash_color, a.ash_shade, a.edge_color), ("#6E6A62", "#3E3B37", "#B8B2A6"))
        self.assertEqual((a.edge_width, a.noise_tile_cm, a.noise_texture), (0.06, 60.0, "T_FX_AshNoise"))
        entry = self.plan.texture(a.noise_texture)
        self.assertIsNotNone(entry)
        self.assertEqual(entry.folder, paths.FX_TEXTURES_DIR)
        self.assertFalse(entry.srgb)
        # the shipped defaults are the contract (the runtime keeps the same copy: AbyssCorpseRules FAshContract)
        self.assertEqual(manifest.AshContract(present=True), a)

    def test_manifest_params_win(self):
        inst = manifest.InstancePlan("MI_AF_Toon_Heroes_X", paths.M_TOON, "Heroes", scalars={"CastShadowStrength": 0.2})
        manifest.plan_look_parameters({inst.name: inst})
        self.assertEqual(inst.scalars["CastShadowStrength"], 0.2)


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

    def test_painted_terrain_needs_every_texture(self):
        data = copy.deepcopy(self.BASE)
        layers = {}
        for tile, name in ((0, "Grass"), (1, "Dirt"), (2, "Stone"), (3, "Water"), (5, "Camp")):
            layers[str(tile)] = {"texture": f"T_Terrain_Test_{name}_BC",
                                 "file": f"Terrain/T_Terrain_Test_{name}_BC.png"}
        data["terrain"] = {"test": {"periodTiles": 3, "layers": layers, "details": {
            "texture": "T_Terrain_Test_Details_BC", "file": "Terrain/T_Terrain_Test_Details_BC.png"}}}
        path = self.make(data)
        root = path.parent
        (root / "Terrain").mkdir(parents=True, exist_ok=True)
        for rel in [v["file"] for v in layers.values()] + ["Terrain/T_Terrain_Test_Details_BC.png"]:
            pngio.write_png(root / rel, 2, 2, bytes(16))
        plan = manifest.build_plan(path, scan_textures=False)
        inst = plan.instances["MI_AF_Terrain_Test"]
        self.assertEqual(inst.parent, paths.M_TERRAIN_PAINTED)
        self.assertEqual(inst.textures["Tile5Layer"], "T_Terrain_Test_Camp_BC")
        self.assertEqual(inst.textures["Details"], "T_Terrain_Test_Details_BC")
        # a missing layer: no instance (the theme keeps the procedural M_AF_Terrain), a warning, no error
        (root / "Terrain/T_Terrain_Test_Stone_BC.png").unlink()
        plan = manifest.build_plan(path, scan_textures=False)
        self.assertNotIn("MI_AF_Terrain_Test", plan.instances)
        self.assertEqual(plan.errors, [])
        self.assertTrue(any("T_Terrain_Test_Stone_BC" in p.message and p.level == "warning" for p in plan.problems),
                        [str(p) for p in plan.problems])

    def test_telegraph_contract_fields_and_textures(self):
        data = copy.deepcopy(self.BASE)
        plan = manifest.build_plan(self.make(data), scan_textures=False)
        self.assertFalse(plan.telegraph.present)      # no contract: the shipped defaults, nothing planned
        self.assertEqual(plan.telegraph, manifest.TelegraphContract())
        self.assertIsNone(plan.texture("T_FX_TelegraphRim"))
        data["materials"] = {"M_AF_Telegraph": {
            "params": {"FillColor": "#102030", "RimTileCm": 90, "FrontWidthCm": "wide"},
            "contact": {"look": "... the rim to #FF0000, then ..."},
            "styles": {"arena": {"outerWash": {"color": "#00FF00", "fadeInCm": 40}}},
            "textures": {"Rim": {"texture": "T_FX_RimTest"}}}}
        path = self.make(data)
        (path.parent / "VFX").mkdir(parents=True, exist_ok=True)
        pngio.write_png(path.parent / "VFX" / "T_FX_RimTest.png", 4, 2, bytes(32))
        plan = manifest.build_plan(path, scan_textures=False)
        t = plan.telegraph
        self.assertTrue(t.present)
        self.assertEqual(t.fill_color, "#102030")
        self.assertEqual(t.rim_color, "#D11F2E")
        self.assertEqual(t.rim_tile_cm, 90.0)
        self.assertEqual(t.front_width_cm, 5.0)          # not a number: the default
        self.assertEqual(t.flash_rim_color, "#FF0000")
        self.assertEqual((t.wash_color, t.wash_fade_in_cm), ("#00FF00", 40.0))
        self.assertEqual(t.rim_texture, "T_FX_RimTest")
        self.assertIsNotNone(plan.texture("T_FX_RimTest"))
        self.assertEqual(plan.texture("T_FX_RimTest").kind, "fx")
        # the fill texture is missing: a warning (the decal draws on the default glow), no error
        self.assertEqual(plan.errors, [])
        self.assertTrue(any("T_FX_TelegraphFill.png missing" in p.message and p.level == "warning"
                            for p in plan.problems), [str(p) for p in plan.problems])
        # a malformed colour is an error
        data["materials"]["M_AF_Telegraph"]["params"]["RimColor"] = "crimson"
        plan = manifest.build_plan(self.make(data), scan_textures=False)
        self.assertTrue(any("params.RimColor" in p.message for p in plan.errors), [str(p) for p in plan.problems])

    def test_ash_contract_fields_and_texture(self):
        data = copy.deepcopy(self.BASE)
        plan = manifest.build_plan(self.make(data), scan_textures=False)
        self.assertFalse(plan.ash.present)      # no contract: the shipped defaults, nothing planned
        self.assertEqual(plan.ash, manifest.AshContract())
        self.assertIsNone(plan.texture("T_FX_AshNoise"))
        data["materials"] = {"M_AF_AshDissolve": {
            "params": {"AshColor": "#102030", "EdgeWidth": 2.0},
            "textures": {"Noise": {"texture": "T_FX_NoiseTest", "map": "object-space XZ / 45 cm (R, G, B)"}}}}
        path = self.make(data)
        (path.parent / "VFX").mkdir(parents=True, exist_ok=True)
        pngio.write_png(path.parent / "VFX" / "T_FX_NoiseTest.png", 4, 4, bytes(64))
        plan = manifest.build_plan(path, scan_textures=False)
        a = plan.ash
        self.assertTrue(a.present)
        self.assertEqual((a.ash_color, a.ash_shade), ("#102030", "#3E3B37"))
        self.assertEqual(a.edge_width, 0.06)            # out of 0..1: the default
        self.assertEqual((a.noise_texture, a.noise_tile_cm), ("T_FX_NoiseTest", 45.0))
        self.assertEqual(plan.texture("T_FX_NoiseTest").kind, "fx")
        self.assertEqual(plan.errors, [])
        # a missing noise is a warning (the dissolve samples the default glow), a malformed colour an error
        (path.parent / "VFX" / "T_FX_NoiseTest.png").unlink()
        data["materials"]["M_AF_AshDissolve"]["params"]["EdgeColor"] = "pale"
        plan = manifest.build_plan(self.make(data), scan_textures=False)
        self.assertTrue(any("T_FX_NoiseTest.png missing" in p.message and p.level == "warning" for p in plan.problems),
                        [str(p) for p in plan.problems])
        self.assertTrue(any("params.EdgeColor" in p.message for p in plan.errors), [str(p) for p in plan.problems])

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

    def test_empty_outline_slots_are_not_planned(self):
        """A hull without triangles (manifest `triangles.outline` 0, or bySlot 0) is a slot Interchange never creates:
        dropped with a note, no outline instance only it would use; skeletal and static meshes alike."""
        data = copy.deepcopy(self.BASE)
        data["assets"]["SK_Mon_Slime"]["triangles"] = {"toon": 288, "outline": 0}
        data["assets"]["SM_Foliage_Plains_Oak_A"]["triangles"] = {"toon": 900, "outline": 0,
                                                                  "bySlot": {"MI_AF_ToonFoliage_Mon": 900,
                                                                             "M_AF_Outline": 0}}
        plan = manifest.build_plan(self.make(data))
        self.assertEqual([p for p in plan.problems if p.level != "note"], [])
        notes = [p for p in plan.problems if p.level == "note"]
        self.assertEqual(sorted(p.subject for p in notes), ["SK_Mon_Slime", "SM_Foliage_Plains_Oak_A"])
        for name in ("SK_Mon_Slime", "SM_Foliage_Plains_Oak_A"):
            asset = plan.asset(name)
            self.assertEqual([s.index for s in asset.slots], [0])
            self.assertEqual([s.name for s in asset.empty_slots], ["M_AF_Outline"])
            self.assertEqual(sorted(plan.slot_materials[name]), [0])
        self.assertFalse([n for n in plan.instances if n.startswith(("MI_AF_Outline", "MI_AF_OutlineFoliage"))],
                         sorted(plan.instances))

    def test_outline_slot_kept_when_any_lod_or_count_says_it_has_faces(self):
        data = copy.deepcopy(self.BASE)
        slime = data["assets"]["SK_Mon_Slime"]
        slime["triangles"] = {"toon": 288, "outline": 0}
        slime["lods"] = [{"index": 1, "fbx": "Monsters/SK_Mon_Slime_LOD1.fbx",
                          "triangles": {"toon": 90, "outline": 40}}]
        data["assets"]["SM_Foliage_Plains_Oak_A"]["triangles"] = {"toon": 900, "outline": 120}
        plan = manifest.build_plan(self.make(data))
        for name in ("SK_Mon_Slime", "SM_Foliage_Plains_Oak_A"):
            self.assertEqual([s.index for s in plan.asset(name).slots], [0, 1], name)
        self.assertIn("MI_AF_Outline_Mon_SmallMonster", plan.instances)
        self.assertFalse([p for p in plan.problems if p.level == "note"])

    def test_clip_fbx_span_must_match_the_manifest_frames(self):
        def fbx(start_s: float, stop_s: float) -> bytes:
            def prop(name: str, seconds: float) -> bytes:
                rec = b""
                for text in (name, "KTime", "Time", ""):
                    rec += b"S" + len(text).to_bytes(4, "little") + text.encode()
                return rec + b"L" + round(seconds * manifest.FBX_KTIME_PER_SECOND).to_bytes(8, "little", signed=True)
            # a Definitions template default first (0), then the object's own records (the last ones win)
            return (b"Kaydara FBX Binary  \x00\x1a\x00" + prop("LocalStart", 0) + prop("LocalStop", 0)
                    + prop("LocalStart", start_s) + prop("LocalStop", stop_s))

        path = self.make(copy.deepcopy(self.BASE))
        root = path.parent
        (root / "Monsters/A_Mon_Slime_Idle.fbx").write_bytes(fbx(0.0, 1.0))          # 60 frames, as listed
        (root / "Monsters/A_Mon_Slime_HurtAdd.fbx").write_bytes(fbx(0.0, 0.5))       # 30 frames, manifest 18
        self.assertEqual(manifest.fbx_anim_stack_seconds(root / "Monsters/A_Mon_Slime_Idle.fbx"), (0.0, 1.0))
        self.assertIsNone(manifest.fbx_anim_stack_seconds(root / "Textures/T_AF_Palette_Mon_BC.png"))
        plan = manifest.build_plan(path)
        spans = [str(p) for p in plan.problems if "animation stack" in p.message]
        self.assertEqual(len(spans), 1, spans)
        self.assertIn("A_Mon_Slime_HurtAdd", spans[0])
        self.assertIn("spans frames 0.00..30.00 at 60 fps, the manifest 18 frames", spans[0])
        # the right length starting later is wrong too: the import window is frames 0..frames
        (root / "Monsters/A_Mon_Slime_HurtAdd.fbx").write_bytes(fbx(0.1, 0.4))
        spans = [str(p) for p in manifest.build_plan(path).problems if "animation stack" in p.message]
        self.assertEqual(len(spans), 1, spans)
        self.assertIn("spans frames 6.00..24.00", spans[0])


class ExportedClipTests(unittest.TestCase):
    """The clips import frames 0..frames (ue/meshes.py set_anim_range): every exported clip FBX spans exactly that."""

    def test_exported_stacks_match_the_manifest(self):
        if not paths.MANIFEST_PATH.is_file():
            self.skipTest("no Art/Export/manifest.json")
        plan = manifest.build_plan(scan_textures=False)
        checked = 0
        for asset in plan.assets:
            for clip in asset.clips:
                span = manifest.fbx_anim_stack_seconds(clip.fbx)
                self.assertIsNotNone(span, clip.asset)
                self.assertAlmostEqual((span[1] - span[0]) * clip.fps, clip.frames, places=2, msg=clip.asset)
                checked += 1
        self.assertGreater(checked, 0)


class RuntimeCopyTests(unittest.TestCase):
    """Data/assets.json (the game's copy of the art manifest) must match the export the content build imports."""

    def write(self, path: Path, data: object) -> Path:
        path.write_text(json.dumps(data), encoding="utf-8")
        return path

    def test_matching_or_absent_copy_is_fine(self):
        with tempfile.TemporaryDirectory() as tmp:
            art = self.write(Path(tmp) / "manifest.json", {"schemaVersion": 1, "assets": {"SM_A": {}, "SM_B": {}}})
            copy_path = self.write(Path(tmp) / "assets.json",
                                   {"schemaVersion": 1, "manifest": json.loads(art.read_text(encoding="utf-8"))})
            self.assertIsNone(manifest.runtime_copy_problem(art, copy_path))
            self.assertIsNone(manifest.runtime_copy_problem(art, Path(tmp) / "missing.json"))

    def test_stale_copy_is_a_warning_naming_the_exporter(self):
        with tempfile.TemporaryDirectory() as tmp:
            art = self.write(Path(tmp) / "manifest.json", {"schemaVersion": 1, "assets": {"SM_A": {}, "SM_B": {}}})
            copy_path = self.write(Path(tmp) / "assets.json", {"manifest": {"schemaVersion": 1, "assets": {"SM_A": {}}}})
            problem = manifest.runtime_copy_problem(art, copy_path)
            self.assertIsNotNone(problem)
            assert problem is not None
            self.assertEqual(problem.level, "warning")
            self.assertIn("(1 vs 2 assets)", problem.message)
            self.assertIn("export-data/run.mjs", problem.message)


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
