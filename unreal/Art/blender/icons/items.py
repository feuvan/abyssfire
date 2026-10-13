"""Item icons (spec §9.1, DECISIONS R10): one 256² icon per item look — every base in ``Data/item_bases.json``
resolves to one (``T_UI_ItemIcon_<iconId>__<baseId>`` where the base looks different from its icon's default,
else ``T_UI_ItemIcon_<iconId>``: the UI's lookup, ``FAbyssUiContext::ItemIcon``, tries exactly that order).

Sources (the same meshes the game shows, ``Art/Export`` FBX re-imported):
* weapons — ``SM_Weapon_<Family>`` with the base's tier material (``manifest → SM_Weapon_* → bases / tiers``:
  T1 = ``Weapons``, ``Weapons_T2`` … ``Weapons_T6``, ``Weapons_Worn``), presented like the web (blades on the
  diagonal tip up-right, bows with the string down-right, shields face-on);
* consumables / gems — the ground-loot meshes ``SM_Pickup_*`` (potion sizes and gem tiers compare in size);
* helms, body armour, gloves, boots, belts, rings, amulets, crafting materials — dedicated icon models
  (``item_models.py``; icon-only, never exported as meshes).

Run (Blender Python):
    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/items.py          # ship all
    … items.py --only sword,helm      # subset → scratch sheet only (iteration), nothing shipped
"""
from __future__ import annotations

import json
import math
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import iconkit as K  # noqa: E402
import item_models as IM  # noqa: E402
from kit import paths, pngio  # noqa: E402
from mathutils import Vector  # noqa: E402

DATA = paths.UNREAL_ROOT / "Data"
GROUP_DIR = "Icons/Items"


@dataclass
class Look:
    key: str                         # unique look id
    icon: str                        # web icon id
    kind: str                        # weapon | pickup | model
    args: tuple
    bases: list = field(default_factory=list)
    default: bool = False
    category: str = ""

    @property
    def texture(self) -> str:
        if self.default:
            return f"T_UI_ItemIcon_{self.icon}"
        return f"T_UI_ItemIcon_{self.icon}__{self.bases[0]}"


# ── weapons ─────────────────────────────────────────────────────────────────────────────────────────────
FAMILY_ICON = {"sword": "w_sword", "axe": "w_axe", "mace": "w_mace", "dagger": "w_dagger", "bow": "w_bow",
               "staff": "w_staff", "wand": "w_wand", "shield": "w_shield"}
DEFAULT_TIER = {"sword": "T1", "axe": "T2", "mace": "T1", "dagger": "T1", "bow": "T1", "staff": "T1", "wand": "T1",
                "shield": "T1"}
POSE = {   # place() kwargs per family (local axes of the socket-space meshes: grip at the origin, blade +Y)
    "sword": dict(toward=(1, 0, 0), up=(0, 1, 0), roll=45, spin=-12),
    "dagger": dict(toward=(1, 0, 0), up=(0, 1, 0), roll=45, spin=-12),
    "axe": dict(toward=(1, 0, 0), up=(0, 1, 0), roll=40, spin=-20),
    "mace": dict(toward=(1, 0, 0), up=(0, 1, 0), roll=40, spin=-20, pitch=-35),
    "bow": dict(toward=(-1, 0, 0), up=(0, 0, 1), roll=45),
    "staff": dict(toward=(1, 0, 0), up=(0, 1, 0), roll=34, spin=-20, pitch=-58),
    "wand": dict(toward=(1, 0, 0), up=(0, 1, 0), roll=40, spin=-20),
    "shield": dict(toward=(0, 1, 0), up=(0, 0, 1), yaw=16, pitch=6),
}
SPARKLE = {   # local point (mesh space) of the focal sparkle
    "sword": (0.0, 0.62, -0.015), "dagger": (0.0, 0.25, -0.012), "axe": (0.0, 0.62, -0.18), "mace": (0.0, 0.55, 0.0),
    "bow": (0.0, -0.06, 0.30), "staff": (0.0, 0.82, 0.0), "wand": (0.0, 0.29, 0.0), "shield": (-0.14, 0.0, 0.17),
}
FILL = {"staff": 0.86, "wand": 0.84, "shield": 0.80}


def tier_palette(tier: str) -> str:
    return "Weapons" if tier == "T1" else f"Weapons_{tier}"


def weapon_looks(bases: list[dict]) -> list[Look]:
    man = K.manifest()["assets"]
    looks: dict[str, Look] = {}
    for fam, icon in FAMILY_ICON.items():
        d = Look(f"{fam}:{DEFAULT_TIER[fam]}", icon, "weapon", (fam, DEFAULT_TIER[fam]), default=True,
                 category="weapons")
        looks[d.key] = d
    for b in bases:
        if b.get("type") != "weapon" and b.get("slot") != "offhand":
            continue
        fam = b.get("weaponType")
        tier = man[f"SM_Weapon_{fam.capitalize()}"]["bases"].get(b["id"])
        if tier is None:
            raise KeyError(f"{b['id']}: no tier in the manifest of SM_Weapon_{fam.capitalize()}")
        key = f"{fam}:{tier}"
        if key not in looks:
            looks[key] = Look(key, FAMILY_ICON[fam], "weapon", (fam, tier), category="weapons")
        looks[key].bases.append(b["id"])
    return list(looks.values())


def build_weapon(fam: str, tier: str):
    o = K.import_fbx("Weapons", f"SM_Weapon_{fam.capitalize()}", K.ShippedPalette(tier_palette(tier)))
    K.place(o, **POSE[fam])
    sp = o.matrix_world @ Vector(SPARKLE[fam])
    acc = [K.Accent("sparkle", at=tuple(sp), r_u=5.0, alpha=0.95)]
    style = dict(auto_glow=0.42 if tier in ("T5", "T6") or fam in ("staff", "wand") else 0.0)
    return [o], acc, style, dict(fill=FILL.get(fam, 0.82))


# ── pickups (consumables, gems) ─────────────────────────────────────────────────────────────────────────
POTION = {"c_hp_potion_s": "SM_Pickup_PotionHP_S", "c_hp_potion_m": "SM_Pickup_PotionHP",
          "c_hp_potion_l": "SM_Pickup_PotionHP_L", "c_mp_potion_s": "SM_Pickup_PotionMP_S",
          "c_mp_potion_m": "SM_Pickup_PotionMP", "c_antidote": "SM_Pickup_Antidote"}
POTION_GLOW = {"HP": "#E02838", "MP": "#2F6CF0", "Antidote": "#5FD03A"}
SCROLL = {"c_tp_scroll": "SM_Pickup_Scroll_TP", "c_id_scroll": "SM_Pickup_Scroll_ID"}
GEM_FILL = {1: 0.60, 2: 0.70, 3: 0.78, 4: 0.82, 5: 0.86}
GEM_COLOR = {"ruby": "#E0283C", "sapphire": "#2F6CF0", "emerald": "#22C060", "topaz": "#FFC02A", "diamond": "#D8F4FF"}
# potions share one scale (S < M < L on a common baseline); 1 u = this many metres
POTION_WPU = 0.43 / (0.86 * K.UNIT)


def build_pickup(asset: str, kind: str, extra=None):
    o = K.import_fbx("Pickups", asset)
    acc = []
    style = {}
    fitk = {}
    if kind == "potion":
        K.place(o, pitch=8)
        tag = "HP" if "HP" in asset else ("MP" if "MP" in asset else "Antidote")
        fitk = dict(world_per_unit=POTION_WPU, anchor="bottom", center_u=(47.5, 88.0))
        acc = [K.Accent("glow", at=tuple(o.matrix_world @ Vector((0, 0, .12))), r_u=30, hex=POTION_GLOW[tag], alpha=.32,
                        under=True),
               K.Accent("sparkle", at=tuple(o.matrix_world @ Vector((.07, -.07, .2))), r_u=5, alpha=.9)]
    elif kind == "scroll":
        K.place(o, toward=(0, 0, 1), up=(0, 1, 0), roll=-28, pitch=-10)
        fitk = dict(fill=0.86)
        if "TP" in asset:
            acc = [K.Accent("glow", u=(48, 48), r_u=18, hex="#3E8CFF", alpha=.28)]
    elif kind == "fruit":
        K.place(o, pitch=10)
        fitk = dict(fill=0.80)
        acc = [K.Accent("glow", u=(48, 56), r_u=40, hex="#6FF0C8", alpha=.5, under=True),
               K.Accent("sparkle", u=(30, 40), r_u=5, hex="#E8FFF8"), K.Accent("sparkle", u=(70, 72), r_u=4, hex="#BFF8EA", alpha=.9)]
    elif kind == "gem":
        tier = extra
        K.place(o, toward=(0, 0, 1), up=(0, 1, 0), yaw=-6)
        fitk = dict(fill=GEM_FILL[tier], center_u=(48, 49))
        style = dict(auto_glow=0.0 if tier < 3 else (0.5 if tier >= 4 else 0.35), seams=False)
        if tier >= 2:
            acc.append(K.Accent("sparkle", u=(48 + 0.62 * 30 * GEM_FILL[tier] / .8, 49 - 0.7 * 30 * GEM_FILL[tier] / .8),
                                r_u=6 if tier >= 3 else 4.5, alpha=.95))
        if tier >= 4:
            acc.append(K.Accent("sparkle", u=(48 - 0.85 * 30 * GEM_FILL[tier] / .8, 49 + 0.6 * 30 * GEM_FILL[tier] / .8),
                                r_u=4.5, alpha=.85))
    return [o], acc, style, fitk


def consumable_looks(bases: list[dict]) -> list[Look]:
    out = []
    by = {b["id"]: b for b in bases}
    out.append(Look("c_hp:m", "c_hp", "pickup", (POTION["c_hp_potion_m"], "potion"), ["c_hp_potion_m"], True, "consumables"))
    out.append(Look("c_hp:s", "c_hp", "pickup", (POTION["c_hp_potion_s"], "potion"), ["c_hp_potion_s"], False, "consumables"))
    out.append(Look("c_hp:l", "c_hp", "pickup", (POTION["c_hp_potion_l"], "potion"), ["c_hp_potion_l"], False, "consumables"))
    out.append(Look("c_mp:m", "c_mp", "pickup", (POTION["c_mp_potion_m"], "potion"), ["c_mp_potion_m"], True, "consumables"))
    out.append(Look("c_mp:s", "c_mp", "pickup", (POTION["c_mp_potion_s"], "potion"), ["c_mp_potion_s"], False, "consumables"))
    out.append(Look("c_antidote", "c_antidote", "pickup", (POTION["c_antidote"], "potion"), ["c_antidote"], True, "consumables"))
    out.append(Look("c_scroll:tp", "c_scroll", "pickup", (SCROLL["c_tp_scroll"], "scroll"), ["c_tp_scroll"], True, "consumables"))
    out.append(Look("c_scroll:id", "c_scroll", "pickup", (SCROLL["c_id_scroll"], "scroll"), ["c_id_scroll"], False, "consumables"))
    out.append(Look("c_ley_fruit", "c_ley_fruit", "pickup", ("SM_Pickup_LeyFruit", "fruit"), ["c_ley_fruit"], True, "consumables"))
    for lk in out:
        for b in lk.bases:
            assert b in by, b
    return out


def gem_looks(bases: list[dict]) -> list[Look]:
    out = []
    for b in bases:
        if b.get("type") != "gem" and not b["id"].startswith("g_"):
            continue
        _, typ, tier = b["id"].split("_")
        tier = int(tier)
        asset = f"SM_Pickup_Gem_{typ.capitalize()}_{tier}"
        out.append(Look(f"{b['icon']}:{tier}", b["icon"], "pickup", (asset, "gem", tier), [b["id"]], tier == 2, "gems"))
    return out


# ── icon-only models ────────────────────────────────────────────────────────────────────────────────────
MODEL_TABLE = {
    # icon: (builder, {kind: [bases]}, default kind)
    "a_helm": ("helm", {"cloth": ["a_cloth_cap"], "leather": ["a_leather_helm"], "iron": ["a_iron_helm"],
                        "full": ["a_full_helm"], "plate": ["a_plate_helm"], "demon": ["a_demon_helm"],
                        "dragon": ["a_dragon_helm"]}, "iron"),
    "a_armor": ("armor", {"quilted": ["a_quilted_armor"], "leather": ["a_leather_armor"], "chain": ["a_chain_mail"],
                          "scale": ["a_scale_mail"], "plate": ["a_plate_armor"], "heavy": ["a_heavy_plate_armor"],
                          "demon": ["a_demon_armor"], "dragon": ["a_dragon_armor"]}, "leather"),
    "a_gloves": ("gloves", {"leather": ["a_leather_gloves"], "chain": ["a_chain_gloves"], "iron": ["a_gauntlets"],
                            "plate": ["a_plate_gloves"], "demon": ["a_demon_gloves"]}, "leather"),
    "a_boots": ("boots", {"leather": ["a_leather_boots"], "chain": ["a_chain_boots"], "iron": ["a_greaves"],
                          "plate": ["a_plate_boots"], "demon": ["a_demon_boots"]}, "leather"),
    "a_belt": ("belts", {"leather": ["a_leather_belt"], "heavy": ["a_heavy_belt"], "war": ["a_war_belt"],
                         "plated": ["a_plated_belt"]}, "leather"),
    "j_ring": ("ring", {"copper": ["j_copper_ring"], "silver": ["j_silver_ring"], "gold": ["j_gold_ring"],
                        "platinum": ["j_platinum_ring"]}, "gold"),
    "j_amulet": ("amulet", {"bone": ["j_bone_amulet"], "jade": ["j_jade_amulet"], "arcane": ["j_arcane_amulet"]}, "jade"),
    "m_ore": ("material", {"ore": []}, "ore"),
    "m_scrap": ("material", {"scrap": ["m_scrap"]}, "scrap"),
    "m_dust": ("material", {"dust": ["m_dust"]}, "dust"),
    "m_essence": ("material", {"essence": ["m_essence"]}, "essence"),
}
MODEL_CATEGORY = {"a_helm": "armour", "a_armor": "armour", "a_gloves": "armour", "a_boots": "armour",
                  "a_belt": "armour", "j_ring": "jewellery", "j_amulet": "jewellery", "m_ore": "materials",
                  "m_scrap": "materials", "m_dust": "materials", "m_essence": "materials"}


def model_looks() -> list[Look]:
    out = []
    for icon, (fn, kinds, dflt) in MODEL_TABLE.items():
        for kind, bases in kinds.items():
            out.append(Look(f"{icon}:{kind}", icon, "model", (fn, kind), list(bases), kind == dflt,
                            MODEL_CATEGORY[icon]))
    return out


def all_looks() -> list[Look]:
    data = json.loads((DATA / "item_bases.json").read_text())
    bases = [b for grp in data["groups"].values() for b in grp]
    looks = weapon_looks(bases) + model_looks() + consumable_looks(bases) + gem_looks(bases)
    # coverage: every base resolves to exactly one look
    seen: dict[str, str] = {}
    for lk in looks:
        for b in lk.bases:
            if b in seen:
                raise RuntimeError(f"{b} in two looks: {seen[b]} / {lk.key}")
            seen[b] = lk.key
    missing = [b["id"] for b in bases if b["id"] not in seen]
    if missing:
        raise RuntimeError(f"item bases without an icon look: {missing}")
    icons = {lk.icon for lk in looks if lk.default}
    want = {b["icon"] for b in bases} | {"w_mace", "w_wand", "m_ore"}
    if want - icons:
        raise RuntimeError(f"icon ids without a default texture: {sorted(want - icons)}")
    return looks


def render_look(cam, lk: Look) -> K.IconResult:
    K.clear_objects()
    if lk.kind == "weapon":
        objs, acc, style, fitk = build_weapon(*lk.args)
    elif lk.kind == "pickup":
        objs, acc, style, fitk = build_pickup(lk.args[0], lk.args[1], lk.args[2] if len(lk.args) > 2 else None)
    else:
        m = getattr(IM, lk.args[0])(lk.args[1])
        objs, acc, style, fitk = m.objs, m.accents, m.style, m.fit
    K.apply_frame(cam, K.fit(objs, **({"fill": 0.80} | fitk)))
    return K.render_icon(style=K.IconStyle(**style), accents=acc)


CATEGORY_ORDER = ("weapons", "armour", "jewellery", "consumables", "gems", "materials")


def main(argv: list[str]) -> int:
    only = None
    if "--only" in argv:
        only = argv[argv.index("--only") + 1].split(",")
    looks = all_looks()
    if only:
        looks = [lk for lk in looks if any(o in lk.key or o in lk.texture for o in only)]
    print(f"[items] {len(looks)} looks, {sum(len(lk.bases) for lk in looks)} bases")
    cam = K.studio()
    IM.reset_palettes()
    results: dict[str, tuple[Look, K.IconResult]] = {}
    t0 = time.time()
    for i, lk in enumerate(looks):
        r = render_look(cam, lk)
        results[lk.texture] = (lk, r)
        print(f"[items] {i + 1:3d}/{len(looks)} {lk.texture:46s} gate {'ok ' if r.gate['pass'] else 'FAIL'} "
              f"span {r.gate['span']:.2f}  {time.time() - t0:5.0f}s")
    fails = [t for t, (_, r) in results.items() if not r.gate["pass"]]
    if only:
        cells = [(lk.texture.replace("T_UI_ItemIcon_", ""), r.rgba) for lk, r in results.values()]
        out = K.SCRATCH / "items_only.png"
        pngio.write_png(out, K.atlas(cells, cols=6, scale=0.5))
        pngio.write_png(K.SCRATCH / "items_only_ui.png", K.ui_sheet(cells, cols=6))
        print(f"[items] scratch sheet {out}; gate fails: {fails}")
        return 0
    # ship: textures, previews, manifest
    for tex, (lk, r) in results.items():
        K.write_icon(f"{GROUP_DIR}/{tex}.png", r.rgba)
    previews = write_previews(results)
    man = K.IconManifest()
    for tex, (lk, r) in results.items():
        src = ({"mesh": f"SM_Weapon_{lk.args[0].capitalize()}", "palette": tier_palette(lk.args[1]), "tier": lk.args[1]}
               if lk.kind == "weapon" else {"mesh": lk.args[0]} if lk.kind == "pickup"
               else {"model": f"icons/item_models.py:{lk.args[0]}('{lk.args[1]}')"})
        man.data["ui"][tex] = K.ui_entry("itemIcon", f"{GROUP_DIR}/{tex}.png", K.ICON_PX, iconId=lk.icon,
                                         default=lk.default, bases=sorted(lk.bases), category=lk.category,
                                         source=src, gate=r.gate,
                                         previews=previews.get(lk.category, []))
    man.save()
    print(f"[items] shipped {len(results)} icons; gate fails: {fails}")
    return 1 if fails else 0


def write_previews(results) -> dict[str, list[str]]:
    by_cat: dict[str, list] = {c: [] for c in CATEGORY_ORDER}
    for tex, (lk, r) in results.items():
        by_cat[lk.category].append((tex.replace("T_UI_ItemIcon_", ""), r.rgba))
    out: dict[str, list[str]] = {}
    for cat, cells in by_cat.items():
        if not cells:
            continue
        files = []
        for pi in range(0, len(cells), 16):
            page = cells[pi:pi + 16]
            sfx = "" if len(cells) <= 16 else f"_p{pi // 16 + 1}"
            files += K.save_preview("icons_items", f"{cat}{sfx}_1x.png", K.atlas(page, cols=4, scale=1.0))
        files += K.save_preview("icons_items", f"{cat}_half.png", K.atlas(cells, cols=8, scale=0.5))
        files += K.save_preview("icons_items", f"{cat}_ui.png", K.ui_sheet(cells, cols=6))
        out[cat] = files
    return out


if __name__ == "__main__":
    sys.exit(main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]))
