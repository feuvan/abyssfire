"""Chapter-1 art checklist — resolves every art id the game asks for in ``emerald_plains`` against
``Art/Export/manifest.json`` (the runtime's own resolution chains, ``Source/Abyssfire/Public/World/WorldContract.md``
§2.2 and ``AbyssUiContext``), checks every referenced file and the per-kind manifest fields, measures the
cross-family coherence rules (spec art-inventory-ch1.md §1.3 outline classes, §1.5 saturation hierarchy, §1.6 / R2
heights, §1.9 budgets) and writes ``Art/Export/CHECKLIST.md``.

    python3 unreal/Art/blender/coherence/checklist.py            # plain CPython 3.11, no bpy
        [--no-write]       print only
        [--strict]         exit 1 on any GAP (default: exit 1 on GAP too; NOTE rows never fail)

Sources (read-only): ``Data/{classes,monsters,minibosses,quest_hunts,npcs,maps,map_gen,quests,lore,random_events,
item_bases,pets,story}.json``, ``Art/Export/VFX/vfx_recipes.json``, ``Source/AbyssCore/Public/abyss/world/CampLayout.h``
(camp prop types), the render metrics of the coherence lineup (``coherence/lineup_metrics.json``, written by
``coherence/lineup.py``; optional).

Status per row: **OK** (resolves to existing files with complete fields), **GAP** (the game asks for it and gets
nothing / a fallback), **NOTE** (intentional and documented: fallback chain, later milestone, runtime-owned).
"""
from __future__ import annotations

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ART = HERE.parent.parent                    # unreal/Art
UNREAL = ART.parent
EXPORT = ART / "Export"
PREVIEWS = ART / "Previews"
DATA = UNREAL / "Data"
OUT = EXPORT / "CHECKLIST.md"
METRICS = HERE / "lineup_metrics.json"
ZONE = "emerald_plains"
THEME = "plains"
PREVIEW_MAX = 200 * 1024

# spec §1.3 outline classes: screen px at 1080p and baked width (cm); §1.6 / DECISIONS R2 heights (cm, tolerance)
CLASS_PX = {"hero": 3.5, "boss": 3.5, "weapon": 3.5, "npc": 3.0, "monster": 3.0, "small_monster": 3.0,
            "interactive": 2.0, "decor": 1.2}
CLASS_CM = {"hero": 1.8, "boss": 1.8, "npc": 1.6, "monster": 1.5, "small_monster": 1.4, "interactive": 1.3,
            "weapon": 1.3, "decor": 1.2}
# documented family-wide departures (envkit.PX_OVERRIDE: Props_Plains decor 1.4 px, see envkit.py)
PX_OVERRIDE = {("Props_Plains", "decor"): 1.4}
R2_HEIGHT = {"SK_Hero_Warrior": (176, 2), "SK_Hero_Mage": (171, 2), "SK_Hero_Rogue": (162, 2),
             "SK_Mon_Goblin": (92, 2), "SK_Mon_GoblinChief": (117, 2), "SK_Mon_GoblinShaman": (95, 2.5),
             "SK_Mon_Slime": (43, 1.5)}
NPC_HEIGHT = (170, 176)
# spec §1.9 LOD0 toon triangle budgets (min, max) and LOD1 caps
BUDGET = {"hero": (14000, 18000, 7000), "npc": (8000, 12000, 5000), "goblin": (6000, 9000, 3500),
          "slime": (2000, 3000, 1500), "tree": (3000, 5000, 1500), "ground": (0, 300, None),
          "pet": (1500, 4000, 2000)}          # no §1.9 row: the small-creature (slime) row, documented in the entry
CHARACTER_PREFIXES = ("SK_Hero_", "SK_Mon_", "SK_NPC_", "SK_Pet_")
CHAR_GATE_PX = 3.0          # Chapter-1 character gate: dark rim >= 3 px on >= 90 % of the silhouette edges at 1080p
BONES_PER_SECTION = {"SKEL_Human": 75, "SKEL_Goblin": 60, "SKEL_Slime": 16, "SKEL_Pet_Sprite": 24}

HERO_CLIPS = ["Idle", "Walk", "Run", "Attack01", "Attack02", "Attack03", "Cast01", "Cast02", "Hurt", "HurtAdd",
              "Dodge", "Death", "Portrait"]
HERO_SIGNATURE = {"warrior": ["Cast_Whirlwind", "Cast_Charge"], "mage": ["Cast_Blizzard", "Cast_Meteor"],
                  "rogue": ["Cast_Multishot"]}          # DECISIONS R5
MONSTER_CLIPS = ["Idle", "Walk", "Attack01", "Hurt", "Death"]
NPC_CLIPS = ["IdleNPC", "Alert", "Talk"]
ESCORT_CLIPS = ["Run", "Hurt", "Death"]
WEAPON_TYPES = ["sword", "axe", "mace", "dagger", "bow", "staff", "wand", "shield"]   # I6 / R4 FindWeapon


def load(p: Path):
    return json.loads(p.read_text(encoding="utf-8"))


def pascal(s: str) -> str:
    return "".join(w[:1].upper() + w[1:] for w in re.split(r"[_\- ]+", s) if w)


class Check:
    def __init__(self):
        self.man = load(EXPORT / "manifest.json")
        self.assets: dict = self.man["assets"]
        self.gids: dict = self.man.get("gameIds", {})
        self.ui: dict = self.man.get("ui", {})
        self.rows: list[dict] = []
        self.asset_status: dict[str, tuple[str, list[str]]] = {}
        self.used: dict[str, list[str]] = defaultdict(list)      # asset → the ids that resolved to it
        self.by_gid: dict[str, list[str]] = defaultdict(list)
        for name in sorted(self.assets):
            for g in self.assets[name].get("gameIds", []) or []:
                self.by_gid[g].append(name)

    # ── resolution ──────────────────────────────────────────────────────────────────────────────────
    def resolve(self, cands: list[str]) -> tuple[str | None, list[str]]:
        """First candidate that is a game id (all variants listing it, asset-name order) or an asset name."""
        for c in cands:
            if c in self.by_gid:
                return c, self.by_gid[c]
            if c in self.assets:
                return c, [c]
        return None, []

    def preview_exists(self, p: str) -> Path | None:
        for base in (PREVIEWS, ART, UNREAL.parent):
            q = base / p
            if q.is_file():
                return q
        return None

    def asset_problems(self, name: str, need_clips: list[str] = (), gated: bool = True) -> list[str]:
        e = self.assets.get(name)
        if e is None:
            return [f"{name} not in the manifest"]
        probs = []
        kind = e.get("kind")
        req = ["kind", "category", "fbx", "boundsCm", "triangles", "materialSlots", "previews"]
        if kind == "SkeletalMesh":
            req += ["skeleton", "scale", "heightCm", "bones", "sockets", "anims", "blobShadowRadiusCm"]
        for k in req:
            if k not in e:
                probs.append(f"field `{k}` missing")
        if not (EXPORT / e.get("fbx", "")).is_file():
            probs.append(f"fbx {e.get('fbx')} missing")
        for lod in e.get("lods", []) or []:
            if not (EXPORT / lod.get("fbx", "")).is_file():
                probs.append(f"LOD {lod.get('fbx')} missing")
        names = set()
        for a in e.get("anims", []) or []:
            names.add(a.get("name"))
            if not (EXPORT / a.get("fbx", "")).is_file():
                probs.append(f"clip {a.get('fbx')} missing")
            for k in ("name", "asset", "fbx", "lengthMs", "frames", "fps", "loop", "notifies"):
                if k not in a:
                    probs.append(f"clip {a.get('name')} field `{k}` missing")
            if a.get("name", "").startswith("Attack") and "contactMs" not in a:
                probs.append(f"clip {a['name']} has no contactMs")
            if a.get("name", "").startswith("Cast") and not ("releaseMs" in a or "contactMs" in a):
                probs.append(f"clip {a['name']} has no releaseMs")
        for c in need_clips:
            if c not in names:
                probs.append(f"clip {c} missing")
        for s in e.get("materialSlots", []):
            pal = s.get("palette")
            if pal and pal not in self.man.get("palettes", {}):
                probs.append(f"palette {pal} not in manifest.palettes")
        prev = e.get("previews", []) or []
        if not prev:
            probs.append("no previews")
        for p in prev:
            q = self.preview_exists(p)
            if q is None:
                probs.append(f"preview {p} missing")
            elif q.stat().st_size > PREVIEW_MAX:
                probs.append(f"preview {p} {q.stat().st_size // 1024} KB > 200 KB")
        has_hull = any(s.get("index", 0) >= 1 and str(s.get("parent", "")).startswith("M_AF_Outline")
                       for s in e.get("materialSlots", []))
        if gated and has_hull:
            ig = e.get("inkGate")
            if not ig:
                probs.append("no inkGate record")
            elif not ig.get("pass"):
                probs.append("inkGate FAIL")
        for att in e.get("attachments", []) or []:
            if att.get("object") not in self.assets:
                probs.append(f"attachment {att.get('object')} missing")
        for att in e.get("itemAttachments", []) or []:
            if att.get("object") not in self.assets:
                probs.append(f"item attachment {att.get('object')} missing")
        for v, ve in (e.get("variants") or {}).items():
            for att in ve.get("attachments", []):
                if att.get("object") not in self.assets:
                    probs.append(f"variant {v}: attachment {att.get('object')} missing")
        for sc in e.get("scenery", []) or []:
            if sc.get("asset") not in self.assets:
                probs.append(f"scenery {sc.get('asset')} missing")
        return probs

    def ui_file(self, tex: str) -> Path | None:
        e = self.ui.get(tex)
        if e is not None:
            p = EXPORT / e.get("file", "")
            return p if p.is_file() else None
        for e in self.assets.values():                 # portraits owned by an SK entry (`portrait` field)
            f = e.get("portrait")
            if isinstance(f, str) and Path(f).stem == tex and (EXPORT / f).is_file():
                return EXPORT / f
        for p in list(EXPORT.glob(f"Icons/**/{tex}.png")) + list(EXPORT.glob(f"Portraits/{tex}.png")):
            return p
        return None

    # ── rows ────────────────────────────────────────────────────────────────────────────────────────
    def row(self, section: str, gid: str, assets: list[str], status: str, note: str = "") -> None:
        self.rows.append({"section": section, "id": gid, "assets": assets, "status": status, "note": note})

    def need(self, section: str, gid: str, cands: list[str], clips: list[str] = (), gated: bool = True,
             note: str = "") -> list[str]:
        hit, names = self.resolve(cands)
        if not names:
            self.row(section, gid, [], "GAP", f"no asset for any of {', '.join(cands)}" + (f"; {note}" if note else ""))
            return []
        probs = []
        for n in names:
            probs += [f"{n}: {p}" for p in self.asset_problems(n, clips, gated)]
            self.used[n].append(gid)
        self.row(section, gid, names, "GAP" if probs else "OK",
                 "; ".join(probs) if probs else (f"via `{hit}`" + (f" — {note}" if note else "")))
        return names

    def need_ui(self, section: str, gid: str, cands: list[str], note: str = "", gap: bool = True) -> None:
        for c in cands:
            f = self.ui_file(c)
            if f is not None:
                self.row(section, gid, [c], "OK", f"`{f.relative_to(EXPORT).as_posix()}`" + (f" — {note}" if note else ""))
                self.used[c].append(gid)
                return
        self.row(section, gid, [], "GAP" if gap else "NOTE", f"none of {', '.join(cands)}" + (f"; {note}" if note else ""))

    # ── Chapter-1 inventory ─────────────────────────────────────────────────────────────────────────
    def run(self) -> None:
        classes = load(DATA / "classes.json")["classes"]
        monsters = load(DATA / "monsters.json")
        mb = load(DATA / "minibosses.json")
        hunts = [h for h in load(DATA / "quest_hunts.json")["hunts"] if h["zone"] == ZONE]
        npcs = load(DATA / "npcs.json")["npcs"]
        zmap = load(DATA / "maps.json")["maps"][ZONE]
        mg = load(DATA / "map_gen.json")
        quests = load(DATA / "quests.json")["quests"]
        quests = quests if isinstance(quests, list) else list(quests.values())
        zq = [q for q in quests if q.get("zone") == ZONE]
        lore = load(DATA / "lore.json")["byZone"].get(ZONE, [])
        rev = load(DATA / "random_events.json")["zones"].get(ZONE, {})
        pets = load(DATA / "pets.json")["pets"]
        story = load(DATA / "story.json")
        recipes = load(EXPORT / "VFX" / "vfx_recipes.json")

        # heroes + their items, portraits, skill icons, skill VFX recipes
        for cl in classes:
            cid = cl["id"]
            self.need("Heroes", f"player_{cid}", [f"player_{cid}", cid], HERO_CLIPS + HERO_SIGNATURE.get(cid, []))
            self.need_ui("Portraits", f"hero {cid}", [f"T_UI_Portrait_Hero_{pascal(cid)}"])
            for sk in cl["skills"]:
                sid = sk["id"]
                self.need_ui("Skill icons", sid, [f"T_UI_SkillIcon_{sid}"])
                vfx = (sk.get("derived") or {}).get("vfxId") or sid
                rid = next((r for r in (f"skill.{vfx}", f"skill.{sid}") if r in recipes["recipes"]), None)
                if rid is None:
                    self.row("Skill VFX", sid, [], "GAP", f"no recipe skill.{vfx} / skill.{sid} (would play skill.default)")
                else:
                    miss = self.recipe_missing(recipes, rid)
                    self.row("Skill VFX", sid, [rid], "GAP" if miss else "OK",
                             "; ".join(miss) if miss else f"`{rid}` ({len(recipes['recipes'][rid].get('layers', []))} layers)")
        for wt in WEAPON_TYPES:                          # I6 / R4: generic weapon per base type family
            names = [n for n, e in sorted(self.assets.items()) if e.get("weaponType") == wt and not e.get("heldBy")]
            if not names:
                self.row("Weapons (I6)", wt, [], "GAP", "no generic SM_Weapon_* with this weaponType")
                continue
            probs = [f"{n}: {p}" for n in names for p in self.asset_problems(n, gated=False)]
            e = self.assets[names[0]]
            tiers = e.get("tiers") or []
            for t in tiers if isinstance(tiers, list) else tiers.values():
                pal = t.get("palette") if isinstance(t, dict) else None
                if pal and pal not in self.man.get("palettes", {}):
                    probs.append(f"tier palette {pal} missing")
            for n in names:
                self.used[n].append(wt)
            self.row("Weapons (I6)", wt, names, "GAP" if probs else "OK", "; ".join(probs) or
                     f"{len(tiers)} tiers, grips for {', '.join(sorted((e.get('grips') or {}).keys()))}")

        # monsters of the zone, the mini-boss, quest hunts, elite crown, boss portrait
        defs = monsters["defs"]
        mids = list(monsters["byZone"].get(ZONE, []))
        mini = mb["byZone"].get(ZONE)
        for mid in mids:
            d = defs[mid]
            art = d.get("spriteKey", "")
            self.need("Monsters", mid, [art, f"monster_{art}", mid, f"monster_{mid}"], MONSTER_CLIPS)
        if mini:
            mdef = (mb.get("defs") or {}).get(mini) or {}
            art = mdef.get("spriteKey") or mini
            self.need("Monsters", mini, [art, f"monster_{art}", mini, f"monster_{mini}"], MONSTER_CLIPS,
                      note="mini-boss")
        for h in hunts:
            base = defs[h["monsterId"]]["spriteKey"]
            names = self.need("Monsters", h["huntId"], [base, f"monster_{base}", h["huntId"]], MONSTER_CLIPS,
                              note=f"hunt leader of {h['questId']}")
            if names:
                v = (self.assets[names[0]].get("variants") or {}).get(h["huntId"])
                self.row("Monsters", f"{h['huntId']} (variant)", names[:1], "OK" if v else "GAP",
                         f"×{v.get('scale')} + {', '.join(a['object'] for a in v.get('attachments', []))}" if v else
                         "no `variants` entry (scale / attachments) on the base asset")
        self.need("Monsters", "elite crown", ["elite_crown", "SM_FX_EliteCrown"])
        for bi in story.get("bossIntros", []):
            if bi["monsterId"] in mids:
                self.need_ui("Portraits", f"boss {bi['monsterId']}", [f"T_UI_Portrait_{pascal(bi['monsterId'])}"])

        # NPCs: camps, field NPCs, quest escort, random-event NPCs; portraits; quest markers
        npc_ids = []
        for camp in zmap.get("camps", []):
            npc_ids += camp.get("npcs", [])
        npc_ids += [f["npcId"] for f in zmap.get("fieldNpcs", [])]
        seen = set()
        for nid in npc_ids:
            if nid in seen:
                continue
            seen.add(nid)
            n = npcs.get(nid, {})
            sp = n.get("spriteId")
            cands = ([f"npc_{sp}", sp] if sp else []) + [f"npc_{nid}", nid]
            self.need("NPCs", nid, cands, NPC_CLIPS)
            self.need_ui("Portraits", f"npc {nid}", [f"T_UI_Portrait_{pascal(nid)}"])
        for q in zq:
            if q.get("escortNpc"):
                art = q["escortNpc"].get("spriteKey")
                self.need("NPCs", f"escort ({q['id']})", [art, f"npc_{art}"], NPC_CLIPS + ESCORT_CLIPS)
                self.need_ui("Portraits", f"escort {art}", [f"T_UI_Portrait_{pascal(art.removeprefix('npc_'))}"])
        for art in (rev.get("rescueNpcSpriteKey"), "npc_wandering_merchant"):
            if art:
                self.need("NPCs", f"event {art}", [art, f"npc_{art}"], NPC_CLIPS)
                self.need_ui("Portraits", f"event {art}", [f"T_UI_Portrait_{pascal(art.removeprefix('npc_'))}"])
        for g in ("quest_marker_available", "quest_marker_complete", "quest_marker_progress"):
            self.need("NPCs", g, [g], gated=False, note="overhead marker (gated with the NPC look)")
        self.need("NPCs", "npc_scenery_anvil", ["npc_scenery_anvil"])

        # pets (DECISIONS Q3: Chapter 1 ships the pet_sprite companion)
        for p in pets:
            if p.get("chapter") != 1:
                continue
            pid = p["id"]
            self.need("Pets (Q3)", pid, [f"beast_{pid}", f"pet_{pid}", pid], ["Idle", "Walk", "Attack01", "Cast01", "Hurt"])
            for st in (1, 2):           # evolved looks (pets.json evolutionLevels 10 / 20: artId beast_<id>_e<st>)
                self.need("Pets (Q3)", f"beast_{pid}_e{st}", [f"beast_{pid}_e{st}"],
                          ["Idle", "Walk", "Attack01", "Cast01", "Hurt"], note=f"evolution stage {st}")
            short = pid.removeprefix("pet_")
            self.need_ui("Portraits", f"pet {pid}", [f"T_UI_Portrait_Pet_{pascal(short)}", f"T_UI_Portrait_{pascal(pid)}"])

        # decorations the plains MapGen / zone actor places, camp props, story, lore, exits, events, quests
        pools = mg["decorPools"][THEME]
        dtypes = sorted({t for k in ("grove", "open") for t, _ in pools[k]} | set(pools.get("tall", []))
                        | set(mg["themes"][THEME]["decorTypes"]))
        for t in dtypes:
            names = self.need("Decorations", t, [t, f"decor_{t}"])
            fps = {(json.dumps(self.assets[n].get("footprintTiles")), self.assets[n].get("blocking")) for n in names}
            if len(fps) > 1:
                self.row("Decorations", f"{t} (variants)", names, "GAP", f"variants disagree on footprint/blocking {fps}")
        self.need("Walls", f"outcrop_{THEME}", [f"outcrop_{THEME}", f"wall_{THEME}", "outcrop", "wall"])
        self.need("Walls", f"palisade_{THEME}", [f"palisade_{THEME}", "palisade", "camp_wall"])
        self.need("Walls", f"palisade_gate_{THEME}", [f"palisade_gate_{THEME}", "palisade_gate"])
        self.need("Walls", f"lily_pad_{THEME}", [f"lily_pad_{THEME}", "decor_lily_pad", "lily_pad"])
        for t in self.camp_types():
            self.need("Camp (W11)", t, [f"camp_{t}", f"decor_camp_{t}", t])
        for sd in zmap.get("storyDecorations", []):
            st = sd["spriteType"]
            self.need("Story decor", sd["id"], [f"decor_{st}", st, f"story_{sd['id']}"])
        for ha in zmap.get("hiddenAreas", []):
            for rw in ha.get("rewards", []):
                if rw["type"] == "chest":
                    self.need("Hidden areas", f"{ha['id']} chest", ["decor_treasure_chest", "treasure_chest"], ["Idle", "Open"])
                else:
                    self.need("Hidden areas", f"{ha['id']} {rw['type']}", [f"decor_{rw['type']}", rw["type"]])
        for lo in lore:
            st = lo["spriteType"]
            self.need("Lore", lo["id"], [f"lore_{st}", st, f"decor_{st}"])
        for ex in zmap.get("exits", []):
            self.need("Exits", f"exit → {ex['targetMap']} (W7 sealed)", ["exit_gate_sealed", "exit_sealed"])
            self.need("Exits", f"exit → {ex['targetMap']} (open, later)", ["exit_portal"])
        self.need("Events", "treasure_cache", ["decor_treasure_chest", "treasure_chest"], ["Idle", "Open"])
        self.need("Events", "environmental_puzzle", [rev.get("puzzleSpriteKey", "decor_event_puzzle_rune_pillar")])
        kinds = set()
        for q in zq:
            for o in q.get("objectives", []):
                if o.get("itemKind"):
                    kinds.add((o["itemKind"], (o.get("source") or {}).get("kind")))
        for kind, src in sorted(kinds, key=lambda x: x[0]):
            if src == "gather":
                self.need("Quests", f"gather {kind}", [f"gather_{kind}", f"quest_item_{kind}", kind, "gather_node"])
            self.need("Quests", f"quest item {kind}", [f"quest_item_{kind}", kind])
            self.need_ui("Quest-item icons", kind, [f"T_UI_QuestItem_{kind}"])
        self.need("Quests", "clue marker", ["clue_mark", "fx_clue", "SM_FX_ClueMagnifier"])
        self.need("Quests", "guide arrow", ["guide_arrow", "SM_FX_GuideArrow"])
        for g in ("goblin_hut", "goblin_fire_pit", "goblin_bone_totem", "goblin_spear_rack", "goblin_crates"):
            self.need("Goblin camp (§6.9)", g, [g], note="placed by the level script (no Source/Data placement yet)")

        # ground loot
        self.need("Loot", "loot_bag", ["loot_bag", "SM_Loot_Bag"])
        self.need("Loot", "potion_drop_hp", ["potion_drop_hp", "SM_Pickup_PotionHP"])
        self.need("Loot", "potion_drop_mp", ["potion_drop_mp", "SM_Pickup_PotionMP"])
        self.need("Loot", "gold coin (reward.gold FX mesh)", ["fx_coin", "SM_Pickup_GoldCoin"], gated=False,
                  note="FX particle mesh, no hull by design")

        # item icons (every base: T_UI_ItemIcon_<icon>__<id> → <icon> → <id>)
        ib = load(DATA / "item_bases.json")
        for grp, items in ib["groups"].items():
            for it in items:
                icon = it.get("icon") or ""
                cands = ([f"T_UI_ItemIcon_{icon}__{it['id']}", f"T_UI_ItemIcon_{icon}"] if icon else []) + \
                        [f"T_UI_ItemIcon_{it['id']}"]
                self.need_ui("Item icons", it["id"], cands)

        # glyphs, emblems (story overlay / UI)
        for g in ("gold", "str", "dex", "vit", "int", "spi", "lck", "atk", "def", "hp", "mp", "quest_available",
                  "quest_complete", "close", "sort"):
            self.need_ui("Glyphs", g, [f"T_UI_Glyph_{g}"])
        speakers = set()
        for cid, cs in story["cutscenes"].items():
            if not (cid.startswith("cs_ep_") or cid == "cs_boss_goblin_chief"):
                continue
            for stp in cs["steps"]:
                spk = stp.get("speaker")
                if isinstance(spk, dict):
                    speakers.add(next(iter(spk.values())))
                if stp.get("kind") == "whisper":
                    speakers.add("emblem_villain")
        for s in sorted(speakers):
            if s.startswith("emblem_"):
                self.need_ui("Portraits", f"speaker {s}", [f"T_UI_Emblem_{pascal(s[7:])}"])
            else:
                self.need_ui("Portraits", f"speaker {s}", [f"T_UI_Portrait_{pascal(s)}"])
        self.need_ui("Portraits", "emblem_generic", ["T_UI_Emblem_Generic"])

        # palettes, terrain, VFX sprites
        for fam, pe in sorted(self.man.get("palettes", {}).items()):
            files = [pe.get("baseColor", {}).get("file"), pe.get("params", {}).get("file")]
            miss = [f for f in files if not f or not (EXPORT / f).is_file()]
            self.row("Palettes", fam, [pe.get("material", "")], "GAP" if miss else "OK",
                     f"missing {miss}" if miss else "BC + P atlases")
        ter = self.man.get("terrain", {}).get(THEME)
        if not ter:
            self.row("Terrain", THEME, [], "GAP", "manifest.terrain.plains missing")
        else:
            files = [v["file"] for v in ter["layers"].values()] + [ter["details"]["file"]]
            for k in ("noise", "water"):
                v = ter.get(k)
                if isinstance(v, dict) and v.get("file"):
                    files.append(v["file"])
            miss = [f for f in files if not (EXPORT / f).is_file()]
            self.row("Terrain", THEME, ["MI_AF_Terrain_Plains"], "GAP" if miss else "OK",
                     f"missing {miss}" if miss else f"{len(files)} textures")
        for sp, se in sorted(recipes.get("sprites", {}).items()):
            tex = se.get("texture") or f"T_FX_{sp}"
            ok = (EXPORT / "VFX" / f"{tex}.png").is_file()
            self.row("VFX sprites", sp, [tex], "OK" if ok else "GAP", "" if ok else f"VFX/{tex}.png missing")
        for mn, me in sorted(recipes.get("meshes", {}).items()):
            ok = mn in self.assets and (EXPORT / me.get("fbx", "")).is_file()
            self.row("VFX meshes", mn, [mn], "OK" if ok else "GAP", "" if ok else "not exported")
            self.used[mn].append("vfx")

    def camp_types(self) -> list[str]:
        src = (UNREAL / "Source/AbyssCore/Public/abyss/world/CampLayout.h").read_text()
        body = src.split("kCampProps", 1)[1]
        return sorted(set(re.findall(r'\{"([a-z_]+)",', body)))

    def recipe_missing(self, recipes: dict, rid: str) -> list[str]:
        miss = []
        sprites, meshes = recipes.get("sprites", {}), recipes.get("meshes", {})
        for layer in recipes["recipes"][rid].get("layers", []):
            sp = layer.get("sprite")
            if sp and sp not in sprites and not (EXPORT / "VFX" / f"T_FX_{sp}.png").is_file():
                miss.append(f"sprite {sp}")
            mh = layer.get("mesh")
            if mh and mh not in meshes and mh not in self.assets:
                miss.append(f"mesh {mh}")
        return miss

    # ── coherence ───────────────────────────────────────────────────────────────────────────────────
    def coherence(self) -> list[dict]:
        out = []

        def add(rule, subject, ok, detail):
            out.append({"rule": rule, "subject": subject, "status": "OK" if ok else "GAP", "detail": detail})
        per_asset: dict[str, list[str]] = defaultdict(list)      # documented per-asset ink requests
        char_gate: list[str] = []
        n_chars = 0
        for name, e in sorted(self.assets.items()):
            if name.startswith(("SK_NPCKit_", "SK_Test_", "SM_Test_")):
                continue
            slots = e.get("materialSlots", [])
            pal = slots[0].get("palette") if slots else None
            hull = next((s for s in slots if s.get("index", 0) >= 1
                         and str(s.get("parent", "")).startswith("M_AF_Outline")), None)
            if hull:
                cls = hull.get("class")
                want_px = PX_OVERRIDE.get((pal, cls), CLASS_PX.get(cls))
                px = hull.get("outlinePx1080", -1)
                # a per-asset request (``outlinePxNote`` + ``outlinePxClass``: the art review's / coherence pass's
                # documented hull request, e.g. the 3.5 px characters) may ask for more ink than its class, never less
                documented = bool(hull.get("outlinePxNote")) and want_px is not None and \
                    abs(hull.get("outlinePxClass", -1) - want_px) < 1e-6 and px >= want_px
                ok = want_px is not None and (abs(px - want_px) < 1e-6 or documented) and \
                    abs(hull.get("widthCm", -1) - CLASS_CM.get(cls, -1)) < 1e-6
                if not ok:
                    add("§1.3 outline class", name, False, f"class {cls}: {hull.get('outlinePx1080')} px / "
                        f"{hull.get('widthCm')} cm, expected {want_px} px / {CLASS_CM.get(cls)} cm")
                elif documented and abs(px - want_px) >= 1e-6:
                    per_asset[f"{cls} {want_px:g} → {px:g} px"].append(name)
            # Chapter-1 character gate (task contract): every shipped character keeps a dark rim >= 3 px on >= 90 % of
            # its silhouette edges in every gated 1080p cell — the ship's inkGate must be gated at >= 3.0 px and pass
            if name.startswith(CHARACTER_PREFIXES) and e.get("kind") == "SkeletalMesh":
                n_chars += 1
                ig = e.get("inkGate") or {}
                gpx = (ig.get("gate") or {}).get("minPx", 0.0)
                if not ig.get("pass") or gpx < CHAR_GATE_PX - 1e-6:
                    char_gate.append(f"{name} (gated at {gpx:g} px, pass {ig.get('pass')}, min rim "
                                     f"{ig.get('minFracRim', 0) * 100:.1f} %)")
                cat = e.get("category")
                expect = {"Characters": {"hero"}, "NPCs": {"npc"}, "Foliage": {"decor"},
                          "Pickups": {"interactive"}}.get(cat)
                if name.startswith("SK_Mon_"):
                    expect = {"monster", "boss"}
                if name.startswith("SK_Pet_"):
                    expect = {"small_monster"}                    # the ley-beast companion (pets/sprite.py)
                if expect and cls not in expect:
                    add("§1.3 category ink", name, False, f"{cat} asset drawn with class {cls} (expected {expect})")
            if name in R2_HEIGHT and "heightCm" in e:
                h, tol = R2_HEIGHT[name]
                if abs(e["heightCm"] - h) > tol:
                    add("R2 height", name, False, f"{e['heightCm']} cm vs {h} ± {tol}")
            if name.startswith("SK_NPC_") and "heightCm" in e:
                if not NPC_HEIGHT[0] <= e["heightCm"] <= NPC_HEIGHT[1]:
                    add("R2 height", name, False, f"{e['heightCm']} cm outside the NPC band {NPC_HEIGHT}")
            tri = (e.get("triangles") or {}).get("toon")
            fam = None
            if name.startswith("SK_Hero_"):
                fam = "hero"
            elif name.startswith("SK_NPC_"):
                fam = "npc"
            elif name.startswith("SK_Mon_Goblin"):
                fam = "goblin"
            elif name == "SK_Mon_Slime":
                fam = "slime"
            elif name.startswith("SK_Pet_"):
                fam = "pet"
            elif name.startswith(("SM_Foliage_Plains_Oak", "SM_Foliage_Plains_FruitTree")):
                fam = "tree"
            elif name.startswith(("SM_Foliage_Plains_GrassTuft", "SM_Foliage_Plains_FlowerPatch")):
                fam = "ground"
            if fam and tri is not None:
                lo, hi, lod1 = BUDGET[fam]
                if not lo <= tri <= hi:
                    add("§1.9 LOD0 budget", name, False, f"{tri} toon tris outside {lo}-{hi}")
                if lod1:
                    lods = e.get("lods") or []
                    if not lods:
                        add("§1.9 LOD1", name, False, f"no LOD1 (budget {lod1})")
                    elif (lods[0].get("triangles") or {}).get("toon", 0) > lod1 * 1.05:
                        add("§1.9 LOD1", name, False, f"LOD1 {lods[0]['triangles']['toon']} > {lod1}")
            if e.get("kind") == "SkeletalMesh":
                cap = BONES_PER_SECTION.get(e.get("skeleton"))
                wb = (e.get("budget") or {}).get("weightedBones")
                if cap and wb and wb > cap:
                    add("§1.9 bones", name, False, f"{wb} weighted bones > {cap}")
        add("character ink gate (ships)", f">= {CHAR_GATE_PX:g} px", not char_gate,
            f"{n_chars - len(char_gate)}/{n_chars} shipped characters gated at >= {CHAR_GATE_PX:g} px on >= 90 % of "
            "the silhouette edges of every 1080p cell" + (f"; below: {', '.join(char_gate)}" if char_gate else ""))
        if per_asset:
            add("§1.3 per-asset ink requests", "documented", True,
                "; ".join(f"{k}: {', '.join(v)}" for k, v in sorted(per_asset.items())) +
                " (outlinePxNote on each slot: the class width kept < 3 px on 10-40 % of the silhouette; the "
                "Chapter-1 character gate needs the 3.5 px request)")
        # render metrics of the coherence lineup (saturation hierarchy §1.5, measured ink per family)
        if METRICS.is_file():
            met = load(METRICS)
            for r in met.get("rules", []):
                add(r["rule"], r["subject"], r["ok"], r["detail"])
        else:
            add("§1.5 saturation hierarchy", "lineup", False, "coherence/lineup_metrics.json missing "
                "(run coherence/lineup.py)")
        return out

    # ── report ──────────────────────────────────────────────────────────────────────────────────────
    def write(self, coh: list[dict], path: Path) -> None:
        gaps = [r for r in self.rows if r["status"] == "GAP"]
        cgaps = [c for c in coh if c["status"] == "GAP"]
        shipped = sorted(n for n in self.assets if not n.startswith(("SK_Test_", "SM_Test_")))
        L = ["# Chapter-1 art checklist (`emerald_plains`)", "",
             "Generated by `Art/blender/coherence/checklist.py` from `Art/Export/manifest.json` and the game data "
             "(`Data/*.json`, `CampLayout.h`) with the runtime's own id resolution (`WorldContract.md` §2.2, "
             "`AbyssUiContext`). Do not edit by hand: re-run the script.", "",
             f"**{len(self.rows)} requirements: {sum(r['status'] == 'OK' for r in self.rows)} OK, {len(gaps)} GAP, "
             f"{sum(r['status'] == 'NOTE' for r in self.rows)} NOTE · coherence rules: {len(cgaps)} GAP** · "
             f"{len(shipped)} manifest assets, {len(self.ui)} UI textures.", "",
             "Status: **OK** = resolves to existing files with complete manifest fields (FBX, clips, LODs, previews "
             "≤ 200 KB, ink gate passed); **GAP** = the game asks for it and gets nothing or a fallback; **NOTE** = "
             "intentional, documented fallback.", ""]
        if gaps or cgaps:
            L += ["## Open gaps", ""]
            for r in gaps:
                L.append(f"* **{r['section']} / {r['id']}** — {r['note']}")
            for c in cgaps:
                L.append(f"* **{c['rule']} / {c['subject']}** — {c['detail']}")
            L.append("")
        else:
            L += ["## Open gaps", "", "None.", ""]
        sections = []
        for r in self.rows:
            if r["section"] not in sections:
                sections.append(r["section"])
        L += ["## Requirements by section", ""]
        for s in sections:
            rs = [r for r in self.rows if r["section"] == s]
            n_ok = sum(r["status"] == "OK" for r in rs)
            L += [f"### {s} ({n_ok}/{len(rs)} OK)", "", "| Id | Asset(s) | Status | Notes |", "|---|---|---|---|"]
            for r in rs:
                a = ", ".join(f"`{x}`" for x in r["assets"]) or "—"
                note = r["note"].replace("|", "\\|")
                L.append(f"| `{r['id']}` | {a} | {r['status']} | {note} |")
            L.append("")
        L += ["## Cross-family coherence", "",
              "Rules: outline class per category (spec §1.3; Props_Plains decor 1.4 px documented in `envkit.PX_OVERRIDE`; "
              "documented per-asset requests above the class width, `outlinePxNote`), the Chapter-1 character gate "
              "(every hero, monster, NPC and companion ship gated at ≥ 3 px dark rim on ≥ 90 % of the silhouette edges of "
              "every 1080p cell), R2 heights, §1.9 LOD0 / LOD1 / bone budgets, and the render-measured rules of the "
              "coherence lineup (`Previews/ch1_coherence/`): §1.5 saturation hierarchy, measured ink per family at "
              "1080p, the character gate on the lineup figures, key-light side, FBX re-import scale.", "",
              "| Rule | Subject | Status | Detail |", "|---|---|---|---|"]
        for c in coh:
            L.append(f"| {c['rule']} | {c['subject']} | {c['status']} | {c['detail'].replace('|', chr(92) + '|')} |")
        L.append("")
        # every shipped asset with its use
        L += ["## Every manifest asset", "",
              "`used by` = the Chapter-1 ids above that resolve to it (empty: kit part, held item, attachment or later "
              "content listed for completeness).", "",
              "| Asset | Kind | Category | Status | Used by |", "|---|---|---|---|---|"]
        for n in shipped:
            e = self.assets[n]
            gated = not (n.startswith("SK_NPCKit_") or e.get("heldBy") or n.startswith("SM_FX_"))
            probs = self.asset_problems(n, gated=gated)
            if n.startswith("SK_NPCKit_"):
                probs = [p for p in probs if "blobShadow" not in p]
            st = "OK" if not probs else "GAP: " + "; ".join(probs)
            used = ", ".join(sorted(set(self.used.get(n, []))))[:160]
            L.append(f"| `{n}` | {e.get('kind', '')[:8]} | {e.get('category', '')} | {st} | {used} |")
        L.append("")
        L += ["## UI textures", "", "| Texture | Kind | File | Used by |", "|---|---|---|---|"]
        for t, e in sorted(self.ui.items()):
            ok = (EXPORT / e.get("file", "")).is_file()
            L.append(f"| `{t}` | {e.get('kind', '')} | {'OK' if ok else 'MISSING'} `{e.get('file')}` | "
                     f"{', '.join(sorted(set(self.used.get(t, []))))[:80]} |")
        L.append("")
        path.write_text("\n".join(L) + "\n", encoding="utf-8")


def main(argv: list[str]) -> int:
    c = Check()
    c.run()
    coh = c.coherence()
    gaps = [r for r in c.rows if r["status"] == "GAP"]
    cg = [x for x in coh if x["status"] == "GAP"]
    for r in gaps:
        print(f"GAP  {r['section']:20s} {r['id']:36s} {r['note']}")
    for x in cg:
        print(f"GAP  {x['rule']:20s} {x['subject']:36s} {x['detail']}")
    for r in c.rows:
        if r["status"] == "NOTE":
            print(f"NOTE {r['section']:20s} {r['id']:36s} {r['note'][:120]}")
    print(f"{len(c.rows)} requirements: {len(c.rows) - len(gaps)} OK/NOTE, {len(gaps)} GAP; coherence {len(cg)} GAP")
    if "--no-write" not in argv:
        c.write(coh, OUT)
        print(f"wrote {OUT}")
    return 1 if (gaps or cg) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
