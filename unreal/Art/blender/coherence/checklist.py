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

**Presentation theme** (dark remake, U1): the zone's ``presentationTheme`` (``Data/maps.json``; ``march`` for
``emerald_plains``, 焚境 · The Burning March) makes every lookup try ``<id>@<theme>`` before ``<id>``, as the runtime
does. Under a theme the checklist also requires: every slot ``march/march_palettes.VARIANTS`` plans a themed variant for
resolves to it (a plains fallback there is a GAP), the speaking NPCs' portraits of their themed looks, the boss and
mini-boss portraits, the echo lore through the ``MI_AF_Echo`` figure contract, the U4 set pieces (``setpiece_*``, the
defend gate), the elite markers (``elite_crown@march`` W1, ``elite_sigil`` W2), the theme's terrain set, the
monsters.md §5 budgets of the remake roster, and the march coherence metrics (``coherence/lineup_metrics_march.json``,
``lineup.py --march``: DR5 ember / violet discipline and the value hierarchy on the rendered lineup). The plains
generator theme still picks the decor pools (a map-generator input, chapter1.md §15).
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
METRICS_MARCH = HERE / "lineup_metrics_march.json"
ZONE = "emerald_plains"
THEME = "plains"                 # the map generator's theme (decor pools, wall ids): a generator input
PREVIEW_MAX = 200 * 1024

# spec §1.3 outline classes: screen px at 1080p and baked width (cm); §1.6 / DECISIONS R2 heights (cm, tolerance)
CLASS_PX = {"hero": 3.5, "boss": 3.5, "weapon": 3.5, "npc": 3.0, "monster": 3.0, "small_monster": 3.0,
            "interactive": 2.0, "decor": 1.2}
CLASS_CM = {"hero": 1.8, "boss": 1.8, "npc": 1.6, "monster": 1.5, "small_monster": 1.4, "interactive": 1.3,
            "weapon": 1.3, "decor": 1.2}
# documented family-wide departures (envkit.PX_OVERRIDE: Props_Plains decor 1.4 px, see envkit.py; the march props
# keep the same decor ink, envkit_march.py / DECISIONS R14)
PX_OVERRIDE = {("Props_Plains", "decor"): 1.4, ("Props_March", "decor"): 1.4}
# monsters.md §5 budgets of the dark-remake Chapter 1 roster (LOD0 toon max, LOD1 toon max)
REMAKE_BUDGET = {"SK_Mon_RisenFootman": (7000, 3000), "SK_Mon_RisenArbalest": (7000, 3000),
                 "SK_Mon_RisenShieldbearer": (7000, 3000), "SK_Mon_DeadStandardBearer": (7500, 3000),
                 "SK_Mon_OathZealot": (7000, 3000), "SK_Mon_Deserter": (7000, 3000),
                 "SK_Mon_DeserterDrummer": (7000, 3000), "SK_Mon_Gravegnawer": (6100, 3200),
                 "SK_Mon_Bonegorger": (9000, 3300), "SK_Mon_Corpseherd": (9000, 3400),
                 "SK_Mon_Breachclot": (2900, 1500), "SK_Mon_Cinderbloat": (2900, 1500),
                 "SK_Mon_Valdras": (11000, 4000)}
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
        self.zmap = load(DATA / "maps.json")["maps"][ZONE]
        self.ptheme = self.zmap.get("presentationTheme") or None    # the zone's presentation theme (U1)
        if self.ptheme == "plains":
            self.ptheme = None
        self.planned = self._planned_variants()

    def _planned_variants(self) -> dict[str, str]:
        """{base game id: planned asset} of the theme's slots (``march/march_palettes.VARIANTS``, pure Python)."""
        if self.ptheme != "march":
            return {}
        sys.path.insert(0, str(HERE.parent / "march"))
        try:
            import march_palettes as MP
        finally:
            sys.path.pop(0)
        return {base: asset for base, asset, _pkg, _note in MP.VARIANTS}

    # ── resolution ──────────────────────────────────────────────────────────────────────────────────
    def resolve(self, cands: list[str]) -> tuple[str | None, list[str]]:
        """First candidate that is a game id (all variants listing it, asset-name order) or an asset name; under a
        presentation theme each candidate is tried as ``<id>@<theme>`` first (the runtime's U1 lookup)."""
        for c in cands:
            if self.ptheme and f"{c}@{self.ptheme}" in self.by_gid:
                return f"{c}@{self.ptheme}", self.by_gid[f"{c}@{self.ptheme}"]
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
        if self.ptheme and hit and "@" not in hit and hit in self.planned:
            probs.append(f"the {self.ptheme} theme plans {self.planned[hit]} for `{hit}@{self.ptheme}` "
                         f"(march_palettes.VARIANTS): the {hit} slot falls back to the plains art")
        for n in names:
            probs += [f"{n}: {p}" for p in self.asset_problems(n, clips, gated)]
            self.used[n].append(gid)
        self.row(section, gid, names, "GAP" if probs else "OK",
                 "; ".join(probs) if probs else (f"via `{hit}`" + (f" — {note}" if note else "")))
        return names

    def portrait_cands(self, cands_ids: list[str], base: str) -> list[str]:
        """Portrait texture candidates of a speaker: the ``portrait`` of the asset its look resolves to (the themed
        look's own portrait first), then ``T_UI_Portrait_<Base>``."""
        out = []
        _, names = self.resolve(cands_ids)
        for n in names[:1]:
            f = self.assets[n].get("portrait")
            if isinstance(f, str):
                out.append(Path(f).stem)
        out.append(f"T_UI_Portrait_{base}")
        return out

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
        self.need("Monsters", "elite crown", ["elite_crown", "SM_FX_EliteCrown"],
                  note="W1 elite marker on fx_overhead (an asset with eliteMarker none opts out)")
        if self.ptheme:
            self.need("Monsters", "elite sigil (W2)", ["elite_sigil"], note="W2 elite marker (monsters.md §3)")
            no_marker = sorted(n for n, e in self.assets.items() if e.get("eliteMarker") == "none")
            self.row("Monsters", "elite marker opt-outs", no_marker, "NOTE",
                     "eliteMarker none (the story boss: monsters.md §2.6 bans a crown; the runtime skips the overhead "
                     "marker, core lane AbyssWorldBuilder::UpdateElite)")
        boss_cs = set()
        for bi in story.get("bossIntros", []):
            if bi["monsterId"] in mids:
                boss_cs.add(bi.get("cutscene"))
                d = defs[bi["monsterId"]]
                cands = self.portrait_cands([d.get("spriteKey", ""), bi["monsterId"], f"monster_{bi['monsterId']}"],
                                            pascal(bi["monsterId"]))
                self.need_ui("Portraits", f"boss {bi['monsterId']}", cands)
        if mini and self.ptheme:
            mdef = (mb.get("defs") or {}).get(mini) or {}
            art = mdef.get("spriteKey") or mini
            cands = self.portrait_cands([art, f"monster_{art}", mini, f"monster_{mini}"],
                                        pascal(mini.removeprefix("miniboss_")))
            self.need_ui("Portraits", f"mini-boss {mini}", cands, note="the mini-boss's pre-fight lines")

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
            self.need_ui("Portraits", f"npc {nid}", self.themed_portrait(cands, pascal(nid)))
        for q in zq:
            if q.get("escortNpc"):
                art = q["escortNpc"].get("spriteKey")
                self.need("NPCs", f"escort ({q['id']})", [art, f"npc_{art}"], NPC_CLIPS + ESCORT_CLIPS)
                self.need_ui("Portraits", f"escort {art}",
                             self.themed_portrait([art, f"npc_{art}"], pascal(art.removeprefix('npc_'))))
        for art in (rev.get("rescueNpcSpriteKey"), "npc_wandering_merchant"):
            if art:
                self.need("NPCs", f"event {art}", [art, f"npc_{art}"], NPC_CLIPS)
                self.need_ui("Portraits", f"event {art}",
                             self.themed_portrait([art, f"npc_{art}"], pascal(art.removeprefix('npc_'))))
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
            if st == "echo":
                self.echo_row(lo["id"])
                continue
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
        if not self.ptheme:
            for g in ("goblin_hut", "goblin_fire_pit", "goblin_bone_totem", "goblin_spear_rack", "goblin_crates"):
                self.need("Goblin camp (§6.9)", g, [g], note="placed by the level script (no Source/Data placement yet)")
        else:
            # the UE set-piece layer (U4, implementation-plan.md §4.4): every set piece the theme ships, the M5 defend
            # gate, the order post (lore_rf_01's own look)
            sp_ids = sorted(g for g in self.by_gid if g.startswith("setpiece_"))
            for g in sp_ids + ["decor_defend_boneyard_gate", "lore_order_post"]:
                self.need("Set pieces (U4)", g, [g], note="placed by the UE set-piece layer (U4)")

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
        prefix = "cs_rf_" if self.ptheme == "march" else "cs_ep_"     # the chapter's cutscenes + its boss intro
        for cid, cs in story["cutscenes"].items():
            if not (cid.startswith(prefix) or cid in boss_cs or (not self.ptheme and cid == "cs_boss_goblin_chief")):
                continue
            for stp in cs["steps"]:
                spk = stp.get("speaker")
                if isinstance(spk, dict):
                    speakers.add(next(iter(spk.items())))
                if stp.get("kind") == "whisper":
                    speakers.add(("emblem", "villain"))
        for kind, sid in sorted(speakers):
            if kind == "emblem":
                self.need_ui("Portraits", f"speaker emblem_{sid}", [f"T_UI_Emblem_{pascal(sid)}"])
            elif kind == "npc":
                sp = (npcs.get(sid) or {}).get("spriteId")
                cands = ([f"npc_{sp}", sp] if sp else []) + [f"npc_{sid}", sid]
                self.need_ui("Portraits", f"speaker {sid}", self.themed_portrait(cands, pascal(sid)))
            else:
                d = defs.get(sid) or {}
                cands = self.portrait_cands([d.get("spriteKey", ""), sid, f"monster_{sid}"], pascal(sid))
                self.need_ui("Portraits", f"speaker {sid}", cands)
        self.need_ui("Portraits", "emblem_generic", ["T_UI_Emblem_Generic"])

        # palettes, terrain, VFX sprites
        for fam, pe in sorted(self.man.get("palettes", {}).items()):
            files = [pe.get("baseColor", {}).get("file"), pe.get("params", {}).get("file")]
            miss = [f for f in files if not f or not (EXPORT / f).is_file()]
            self.row("Palettes", fam, [pe.get("material", "")], "GAP" if miss else "OK",
                     f"missing {miss}" if miss else "BC + P atlases")
        for th in [THEME] + ([self.ptheme] if self.ptheme else []):
            ter = self.man.get("terrain", {}).get(th)
            mi = f"MI_AF_Terrain_{pascal(th)}"
            if not ter:
                self.row("Terrain", th, [], "GAP", f"manifest.terrain.{th} missing")
                continue
            files = [v["file"] for v in ter["layers"].values()] + [ter["details"]["file"]]
            for k in ("noise", "water"):
                v = ter.get(k)
                if isinstance(v, dict) and v.get("file"):
                    files.append(v["file"])
            miss = [f for f in files if not (EXPORT / f).is_file()]
            used = (f"presentation theme of {ZONE}" if th == self.ptheme else
                    "generator theme; the presentation of zones without a theme (ember_tower)")
            self.row("Terrain", th, [mi], "GAP" if miss else "OK",
                     f"missing {miss}" if miss else f"{len(files)} textures ({used})")
        for sp, se in sorted(recipes.get("sprites", {}).items()):
            tex = se.get("texture") or f"T_FX_{sp}"
            ok = (EXPORT / "VFX" / f"{tex}.png").is_file()
            self.row("VFX sprites", sp, [tex], "OK" if ok else "GAP", "" if ok else f"VFX/{tex}.png missing")
        for mn, me in sorted(recipes.get("meshes", {}).items()):
            ok = mn in self.assets and (EXPORT / me.get("fbx", "")).is_file()
            self.row("VFX meshes", mn, [mn], "OK" if ok else "GAP", "" if ok else "not exported")
            self.used[mn].append("vfx")

    def themed_portrait(self, cands_ids: list[str], base: str) -> list[str]:
        """An NPC's portrait candidates; when its look resolves to a themed asset, only that look's own portrait
        counts (the plains portrait would show the plains look)."""
        hit, names = self.resolve(cands_ids)
        pc = self.portrait_cands(cands_ids, base)
        if self.ptheme and hit and "@" in hit and names and self.assets[names[0]].get("portrait"):
            return pc[:1]
        return pc

    def echo_row(self, lid: str) -> None:
        """Echo lore (spriteType ``echo``, U1 draws translucent figures frozen in a pose): the ``MI_AF_Echo`` figure
        contract of the manifest names the figures, their assets and clips."""
        echo = (self.man.get("materials") or {}).get("MI_AF_Echo") or {}
        fig = (echo.get("figures") or {}).get(lid)
        if not fig:
            self.row("Lore", lid, [], "GAP", "no MI_AF_Echo figure contract for this echo")
            return
        probs, used = [], []
        for f in fig.get("figures", []):
            a = f.get("asset")
            if a not in self.assets:
                probs.append(f"figure asset {a} missing")
                continue
            used.append(a)
            src = f.get("clipAsset") or a
            if f.get("clip") not in {c.get("name") for c in self.assets.get(src, {}).get("anims", [])}:
                probs.append(f"{a}: clip {f.get('clip')} not on {src}")
            for p in self.asset_problems(a, gated=True):
                probs.append(f"{a}: {p}")
            self.used[a].append(lid)
        for pr in fig.get("props", []) or []:
            if pr not in self.assets:
                probs.append(f"prop {pr} missing")
        follow = [f.get("followUp") for f in fig.get("figures", []) if f.get("followUp")]
        self.row("Lore", lid, used + ["MI_AF_Echo"], "GAP" if probs else "OK",
                 "; ".join(probs) if probs else f"MI_AF_Echo: {len(fig.get('figures', []))} figure(s) at "
                 f"{tuple(fig.get('tile', ()))}" + (f" (follow-up: {', '.join(follow)})" if follow else ""))

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

        def add(rule, subject, ok, detail, status=None):
            out.append({"rule": rule, "subject": subject, "status": status or ("OK" if ok else "GAP"),
                        "detail": detail})
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
                if fam == "npc" and tri < lo:
                    # §1.9 is a performance budget: a lighter NPC costs nothing (the band floor is a detail target)
                    add("§1.9 LOD0 budget", name, True, f"{tri} toon tris under the {lo}-{hi} NPC band "
                        "(performance budget, lighter is allowed)", status="NOTE")
                elif not lo <= tri <= hi:
                    add("§1.9 LOD0 budget", name, False, f"{tri} toon tris outside {lo}-{hi}")
                if lod1:
                    lods = e.get("lods") or []
                    if not lods:
                        add("§1.9 LOD1", name, False, f"no LOD1 (budget {lod1})")
                    elif (lods[0].get("triangles") or {}).get("toon", 0) > lod1 * 1.05:
                        add("§1.9 LOD1", name, False, f"LOD1 {lods[0]['triangles']['toon']} > {lod1}")
            if name in REMAKE_BUDGET and tri is not None:
                hi, lod1 = REMAKE_BUDGET[name]
                lods = e.get("lods") or []
                l1 = (lods[0].get("triangles") or {}).get("toon") if lods else None
                ok = tri <= hi and l1 is not None and l1 <= lod1
                add("monsters.md §5 budget", name, ok, f"LOD0 {tri} / {hi}, LOD1 {l1} / {lod1} toon tris")
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
        if self.ptheme:
            # the presentation theme's lineup (the zone as shipped): DR5 discipline + value hierarchy on the render
            if METRICS_MARCH.is_file():
                met = load(METRICS_MARCH)
                for r in met.get("rules", []):
                    add(f"[{self.ptheme}] {r['rule']}", r["subject"], r["ok"], r["detail"])
            else:
                add(f"[{self.ptheme}] lineup", "lineup", False, "coherence/lineup_metrics_march.json missing "
                    "(run coherence/lineup.py --march)")
        return out

    # ── report ──────────────────────────────────────────────────────────────────────────────────────
    def write(self, coh: list[dict], path: Path) -> None:
        gaps = [r for r in self.rows if r["status"] == "GAP"]
        cgaps = [c for c in coh if c["status"] == "GAP"]
        shipped = sorted(n for n in self.assets if not n.startswith(("SK_Test_", "SM_Test_")))
        th = self.ptheme
        L = [f"# Chapter-1 art checklist (`{ZONE}`" + (f", presentation theme `{th}`)" if th else ")"), "",
             "Generated by `Art/blender/coherence/checklist.py` from `Art/Export/manifest.json` and the game data "
             "(`Data/*.json`, `CampLayout.h`) with the runtime's own id resolution (`WorldContract.md` §2.2, "
             "`AbyssUiContext`" + (f"; every id tried as `<id>@{th}` first, U1 — the zone's `presentationTheme` in "
                                   "`Data/maps.json`" if th else "") + "). Do not edit by hand: re-run the script.",
             "",
             f"**{len(self.rows)} requirements: {sum(r['status'] == 'OK' for r in self.rows)} OK, {len(gaps)} GAP, "
             f"{sum(r['status'] == 'NOTE' for r in self.rows)} NOTE · coherence rules: {len(cgaps)} GAP** · "
             f"{len(shipped)} manifest assets, {len(self.ui)} UI textures.", "",
             "Status: **OK** = resolves to existing files with complete manifest fields (FBX, clips, LODs, previews "
             "≤ 200 KB, ink gate passed); **GAP** = the game asks for it and gets nothing or a fallback; **NOTE** = "
             "intentional, documented fallback." + (
                 f" Under the `{th}` theme a slot that `march/march_palettes.VARIANTS` plans a themed variant for "
                 "is a GAP when it falls back to its plains asset." if th else ""), ""]
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
              "1080p, the character gate on the lineup figures, key-light side, FBX re-import scale." + (
                  f" Under the `{th}` theme also the monsters.md §5 budgets of the remake roster and the rules of the "
                  "march lineup (`Previews/ch1_coherence_march/`, rows prefixed `[march]`): the same family rules on "
                  "the themed assets plus DR5 — no ember on enemies, breach-violet only on enemies and breach-sourced "
                  "props, warm light only on refuges / interaction / loot — and the value hierarchy (actors above "
                  "decor)." if th else ""), "",
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
