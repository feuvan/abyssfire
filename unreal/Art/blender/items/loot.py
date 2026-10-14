"""Ground loot and pickups (spec §7, §8.7 ``NS_Loot_Beam``, §9.1 / §9.3 identities; DECISIONS I2 / I4 / I6, P6).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/loot.py            # ship all
    ... loot.py --only bag,potion        # ship a subset (groups: bag coin gold potion scroll fruit gem quest)
    ... loot.py --quick [groups]         # iteration: studio sheet + W1 1080p cells → scratch, no export

One ``Pickups`` palette (``MI_AF_Toon_Pickups``), category ``Pickups``, outline class **interactive** (spec §1.3: the
prop's line tone, 2.0 px at 1080p, gate 1.5 px), pivots at the centre of the ground contact (Z = 0), every mesh
readable from any yaw (the world spawns props with a hashed yaw and bobs them 0…12.8 cm). Pickup scale is
exaggerated like the web (a 1.76 m hero; a potion ≈ 0.33 m, the bag ≈ 0.42 m, a coin 6 cm).

| Asset | Game ids | Web source |
|---|---|---|
| ``SM_Loot_Bag`` | ``loot_bag`` | ``LootBag.ts``: bright low-saturation sack (tinted by quality), gathered neck, cord, a coin peeking out |
| ``SM_Pickup_GoldCoin`` | ``fx_coin`` | ``FxTextures.ts`` ``fx_coin`` — the ``reward.gold`` burst mesh (no hull: VFX, §1.3) |
| ``SM_Pickup_Gold_S / _M / _L`` | ``gold_pile_s/m/l`` | coins by amount (a few · a stack · a heap); ``GoldPile.ts`` coin language |
| ``SM_Pickup_PotionHP_S``, ``SM_Pickup_PotionHP``, ``_L``; ``SM_Pickup_PotionMP_S``, ``SM_Pickup_PotionMP`` | ``potion_drop_hp[_s/_l]``, ``loot_c_hp_potion_*`` … | ``Pickups.ts`` flask (glass, liquid lower ⅔, meniscus, cork) + ``ItemIcons.ts`` potionSpec sizes |
| ``SM_Pickup_Antidote`` | ``loot_c_antidote`` | potionSpec 'square' green + label |
| ``SM_Pickup_Scroll_TP / _ID`` | ``loot_c_tp_scroll`` / ``loot_c_id_scroll`` | scrollSpec: parchment, two rolls, swirl / eye glyph, ribbon + wax seal |
| ``SM_Pickup_LeyFruit`` | ``loot_c_ley_fruit`` | drawLeyFruit: teal fruit, ley-vein seams, stem + leaf |
| ``SM_Pickup_Gem_<Type>_<Tier>`` (17) | ``loot_g_<type>_<tier>`` | gemSpec cuts: ruby oval · sapphire pear · emerald step · topaz trillion · diamond round; tier 1 chipped … 5 |
| ``SM_Pickup_Quest_Herb / _Gel / _Pendant`` | ``quest_item_<kind>`` | ``QuestItemIcons.ts`` herb bundle, slime gel, heirloom pendant (§9.3) |

**Quality hooks** (manifest ``loot`` record on every entry; the UE side reads them): ``tint`` — only the bag is
tinted (``AbyssCpd`` Tint × 0.85, canonical palette ``qualityPalette``, normal untinted); every pickup has the
sockets ``beam`` (ground centre: light pool + ``NS_Loot_Beam`` base), ``label`` (name plate anchor, top + 18 cm)
and ``glint`` (sparkle point); ``beam`` lists the §8.7 pool / pillar sizes per quality. Ground items other than
the bag are offered as ``loot_<baseId>`` game ids (the world currently resolves ``loot_bag`` for every ground item).

Previews: ``Previews/<asset>/`` = ``game.png``, ``game_1080crop.png`` (shipped ink + bloom | baked hull),
``game_inkgate.png``, ``game_yaws.png`` (four more yaws, each ink-gated: 5 gated cells per pickup),
``closeup.png`` (ink in screen pixels); ``Previews/loot_family/``: ``quality_bag.png`` (the bag in the five
qualities with tint, pool, beam at W1 1080p 1:1), ``game_lineup.png`` (every pickup round the warrior at W1 1080p
1:1), ``studio_*.png`` / ``gems.png`` (close-ups).
"""
from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import itemkit as IK  # noqa: E402  (sets sys.path for kit / heroes)

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import asset, color, export, mesh as M, outline, paths, review, scene, shading  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

import common as C  # noqa: E402

CATEGORY = "Pickups"
FAMILY = "Pickups"
CLS = "interactive"
TAU = 2.0 * math.pi


def R(h, s=.42, l=.32, e=0.0):
    return Region(h, s, l, e=e)


# ── shared geometry helpers ─────────────────────────────────────────────────────────────────────────────
def ring_pts(c: Vector, r_fn, n: int, z: float) -> list[Vector]:
    return [Vector((c.x + r_fn(TAU * i / n) * math.cos(TAU * i / n), c.y + r_fn(TAU * i / n) * math.sin(TAU * i / n),
                    z)) for i in range(n)]


def closed_tube(center: Vector, radius: float, tube_r: float, n: int = 24, sides: int = 8, z_wave=None) -> M.Part:
    """A torus-like closed tube round +Z (cords, lips, rings)."""
    path = []
    for i in range(n):
        a = TAU * i / n
        dz = z_wave(a) if z_wave else 0.0
        path.append(center + Vector((radius * math.cos(a), radius * math.sin(a), dz)))
    return M.sweep(path, M.superellipse(tube_r, tube_r, sides), closed_path=True, up=(0, 0, 1))


def swap_yz(p: M.Part) -> M.Part:
    """slab2d builds plates in XZ (depth Y); swap to an XY plate with depth Z (mirror → normals recomputed)."""
    p.deform(lambda co: Vector((co.x, co.z, co.y)))
    return p.recalc_normals()


def plate_xy(outline_xy, z0: float, z1: float, rings: int = 3) -> M.Part:
    """Closed plate from a star-shaped 2-D outline in the XY plane between heights z0 … z1."""
    return swap_yz(C.slab2d(C.ccw(outline_xy), z0, z1, rings=rings))


def lens_leaf(length: float, width: float, thick: float, curl: float = 0.0, n: int = 9) -> M.Part:
    """A pointed leaf along +X (base at the origin), width along Y, thickness Z; ``curl`` lifts the tip."""
    o = []
    for i in range(n + 1):
        t = i / n
        o.append((length * t, width * 0.5 * math.sin(math.pi * t) ** 0.8))
    for i in range(n - 1, 0, -1):
        t = i / n
        o.append((length * t, -width * 0.5 * math.sin(math.pi * t) ** 0.8))
    p = plate_xy(o, -thick / 2, thick / 2, rings=2)
    if curl:
        p.deform(lambda co: Vector((co.x, co.y, co.z + curl * (co.x / length) ** 2 - 0.25 * curl * (co.y / width) ** 2)))
    return p


def coin(r: float = 0.030, h: float = 0.0085, sides: int = 16) -> tuple[M.Part, M.Part]:
    """(rim, face) of a gold coin lying flat (centre at the origin): a bevelled rim and a raised embossed face on
    both sides (the face disc is a little proud of the rim's inner shelf so the two tones always show)."""
    rim = IK.revolve([(r * 0.80, -h / 2), (r * 0.96, -h / 2), (r, -h * 0.25), (r, h * 0.25), (r * 0.96, h / 2),
                      (r * 0.80, h / 2)], sides=sides).auto_sharp(40)
    face = M.cylinder(r * 0.78, r * 0.80, -h * 0.60, -h * 0.40, sides=sides)
    face.merge(M.cylinder(r * 0.80, r * 0.78, h * 0.40, h * 0.60, sides=sides))
    return rim, face.auto_sharp(30)


def place(p: M.Part, m: Matrix) -> M.Part:
    return p.transform(m)


def tilt(rx: float = 0.0, ry: float = 0.0, rz: float = 0.0, t=(0, 0, 0)) -> Matrix:
    return (Matrix.Translation(Vector(t)) @ Matrix.Rotation(math.radians(rz), 4, "Z")
            @ Matrix.Rotation(math.radians(ry), 4, "Y") @ Matrix.Rotation(math.radians(rx), 4, "X"))


def lowest_z(obj) -> float:
    """Lowest world z of the toon surface (slot 0; the hull hangs ~1.3 cm below a ground contact by design)."""
    me = obj.data
    vs = {i for p in me.polygons if p.material_index == 0 for i in p.vertices}
    return min((obj.matrix_world @ me.vertices[i].co).z for i in vs)


def top_z(obj) -> float:
    me = obj.data
    vs = {i for p in me.polygons if p.material_index == 0 for i in p.vertices}
    return max((obj.matrix_world @ me.vertices[i].co).z for i in vs)


def footprint_r(obj) -> float:
    me = obj.data
    vs = {i for p in me.polygons if p.material_index == 0 for i in p.vertices}
    return max(math.hypot(me.vertices[i].co.x, me.vertices[i].co.y) for i in vs)


# ── regions ─────────────────────────────────────────────────────────────────────────────────────────────
CLOTH = R("#E8DCC4", .38, .45)                  # LootBag.ts: deliberately bright, low saturation (tinted)
COIN_RIM = R("#E3A631", .38, .45)
COIN_FACE = R("#F6C84A", .32, .55)
GLASS = R("#DFE8F4", .30, .50)                   # Pickups.ts glass (s .3, l .5)
GLINT = R("#FFFFFF", 0.0, 0.0, e=0.7)
CORK = R("#B07A44", .42, .30)
GOLD = R("#E3B44C", .42, .50)

REGIONS = {
    "bag": dict(cloth=CLOTH, fold=CLOTH.shade_region(.30, .20), patch=R("#D3C19C", .38, .40),
                stitch=R("#8A6A44", .35, .2), cord=R("#8A6A44", .40, .30), coin=COIN_FACE, coin_rim=COIN_RIM),
    # heap: the mound between the coins of Gold_L reads as the shadowed coins inside the pile (a shade darker than a
    # coin's rim); *_lean: an upright coin keeps gold in the shade band at any spawn yaw (low shadowAmt)
    "coin": dict(rim=COIN_RIM, face=COIN_FACE, heap=R("#D69A2A", .40, .40), rim_lean=R("#E3A631", .16, .45),
                 face_lean=R("#F6C84A", .14, .55)),
    "potion": dict(glass=GLASS, glint=GLINT, cork=CORK, cork_line=R("#7A4E26", .35, .2), collar=GOLD,
                   # glass tinted 22 % toward its liquid (art review 5: the colourless shoulder dominated from −50°)
                   glass_hp=R("#E0BCC8", .30, .50), glass_mp=R("#BCCAF0", .30, .50), glass_anti=R("#E6F8E0", .26, .55),
                   hp=R("#E0283A", .40, .45, e=0.22), hp_hi=R("#FF8A92", .30, .40, e=0.35),
                   mp=R("#2A5AE8", .40, .45, e=0.22), mp_hi=R("#8AB0FF", .30, .40, e=0.35),
                   # antidote: a lighter lime liquid and near-white glass (art review, fourth look: saturated green
                   # on the plains grass #74A247 at equal value — only the ink carried it)
                   anti=R("#9CEB5A", .34, .50, e=0.26), anti_hi=R("#E4FFC8", .25, .45, e=0.45),
                   label=R("#EEE2C0", .35, .40), leafmark=R("#3A9A3A", .35, .25)),
    "scroll": dict(sheet=R("#F0DCAA", .38, .35), roll=R("#E2C890", .38, .40), knob=R("#C8A86A", .42, .35),
                   ink=R("#C8A86A", .30, .2).shade_region(.25, .1),
                   tp=R("#3E8CFF", .30, .30, e=0.55), tp_seal=R("#2F5AC0", .40, .40), tp_ribbon=R("#3E8CFF", .40, .35),
                   id=R("#9A5A14", .35, .15), id_pupil=R("#4A200C", .30, .10), id_seal=R("#C0282E", .40, .40),
                   id_ribbon=R("#D8A030", .40, .35)),
    # the ley-vein seams are emissive lines (web #C8FFF0 / #9FF6E0, art review, fourth look: faint lines on a plain
    # teal apple; spec §1.1 one emissive accent per interactive prop)
    # (e .55: at e 1.0 the seams' bloom lifted the ink above the dark limit where a seam meets the silhouette)
    "fruit": dict(skin=R("#3FD0B0", .40, .45, e=0.05), vein=R("#C8FFF0", 0.0, 0.0, e=0.55),
                  vein2=R("#9FF6E0", 0.0, 0.0, e=0.45), stem=R("#6B4428", .42, .30), leaf=R("#3FAE5C", .42, .40),
                  rib=R("#7FD88A", .30, .30)),
    "quest": dict(herb_a=R("#5AA83A"), herb_b=R("#6CBF42"), herb_c=R("#4F9A34"), herb_d=R("#7CCC4C"),
                  herb_e=R("#62B43E"), stem=R("#4A7A28"), twine=R("#FFF4D6", .30, .50),
                  petal=R("#FFFDF4", .25, .50, e=0.12), pistil=R("#FFD24A", .30, .45, e=0.15),
                  gel=R("#6EE04A", .38, .50, e=0.10), gel_hi=R("#FFFFFF", 0.0, 0.0, e=0.85),
                  gel_core=R("#B6F49A", .30, .45, e=0.25),
                  locket=R("#EEC05A", .40, .36), filigree=R("#FFE7A0", .30, .40), hinge=R("#9A6A1A", .35, .2),
                  ruby=R("#E0283C", .35, .45, e=0.40), chain=R("#E2B64E", .40, .45), bail=R("#FFE09A", .35, .45)),
}
GEMS = {   # web ItemIcons.ts gem colours and cuts (gemSpec)
    "Ruby": ("#E0283C", "oval"), "Sapphire": ("#2F6CF0", "pear"), "Emerald": ("#22C060", "emerald"),
    "Topaz": ("#FFC02A", "trillion"), "Diamond": ("#D8F4FF", "round"),
}
GEM_TIERS = {"Ruby": 3, "Sapphire": 3, "Emerald": 3, "Topaz": 3, "Diamond": 5}
# pickup scale (art review, fourth look: 8–16 cm stones were 10–22 px dots at 1080p beside 24 cm potions, and the
# tier silhouettes could not be told apart): ×1.75–1.85, about 14 → 30 cm across the tier ramp
GEM_R = {1: 0.070, 2: 0.088, 3: 0.106, 4: 0.126, 5: 0.148}
# tier → silhouette (crown height / table / girdle sides): chipped · faceted · crowned (a two-step crown) · brilliant
GEM_FORM = {1: dict(crown=0.26, table=0.62, steps=1), 2: dict(crown=0.40, table=0.54, steps=1),
            3: dict(crown=0.62, table=0.40, steps=2), 4: dict(crown=0.70, table=0.36, steps=2, sides=16),
            5: dict(crown=0.82, table=0.32, steps=2, sides=16, pavilion=0.95)}


GEM_STAR_TIER = 3          # tiers ≥ this (and every diamond) carry the emissive glint star on the table


def gem_regions() -> dict:
    """Saturated cut stones (web facetGem: tone(color, shadow .5, light .45) with deep / glint facets): the crown
    alternates the base colour and a slightly lighter star facet, the table is the pale glint (emissive on the
    perfect cuts → bloom), the pavilion the deep tone. Low lightAmt keeps the warm light band from washing the
    colour to pastel. The near-white diamond takes cool blue-grey deep facets for contrast and a darker hull mix
    (o .7) so its ink stays below the line-tone limit under the bloom."""
    out = {}
    for name, (hx, _) in GEMS.items():
        k = name.lower()
        rgb = color.hex_to_rgb(hx)
        hexm = lambda c, t: color.rgb_to_hex(color.mix(rgb, c, t))  # noqa: E731
        dia = name == "Diamond"
        # bloom of the glowing tables / glint stars over the hull (fourth look: larger, brighter accents)
        o = 0.45 if dia else 0.5
        out[f"{k}_a"] = Region(hexm((150, 190, 225), .25), .55, .22, o=o) if dia else Region(hx, .55, .14, o=o)
        out[f"{k}_b"] = (Region(hexm((255, 255, 255), .18), .50, .30, e=0.05, o=o) if dia
                         else Region(hexm((255, 255, 255), .09), .50, .18, e=0.05, o=o))
        # the big upward table catches the warm light band: saturated stones keep a low lightAmt and less white
        # so the ruby stays #E0283C-red, not salmon (art review 7)
        out[f"{k}_table"] = (Region(hexm((255, 255, 255), .55), .35, .40, e=0.10, o=o) if dia
                             else Region(hexm((255, 255, 255), .26), .40, .12, e=0.06, o=o))
        out[f"{k}_glow"] = (Region(hexm((255, 255, 255), .62), .25, .40, e=0.22, o=o) if dia
                            else Region(hexm((255, 255, 255), .34), .30, .12, e=0.20, o=o))
        out[f"{k}_deep"] = Region(hexm((40, 80, 140), .50) if dia else hexm((16, 8, 32), .45), .50, .20, o=o)
        if name == "Ruby":         # the crown still read pink / salmon under the warm band (fourth look): web #E0283C
            # (the pink came from the warm rim band on the steep crown facets: e .3 mixes it back toward the base)
            out[f"{k}_a"] = Region(hx, .55, .05, e=0.30, o=o)
            out[f"{k}_b"] = Region(hexm((255, 255, 255), .04), .50, .07, e=0.30, o=o)
            out[f"{k}_table"] = Region(hexm((255, 255, 255), .16), .40, .05, e=0.06, o=o)
            out[f"{k}_glow"] = Region(hexm((255, 255, 255), .22), .30, .05, e=0.45, o=o)
        if name == "Emerald":      # green on the plains grass at equal value (fourth look): deep body, bright facets
            out[f"{k}_a"] = Region(hexm((8, 40, 24), .38), .55, .14, o=o)
            out[f"{k}_b"] = Region(hexm((255, 255, 255), .22), .45, .25, e=0.08, o=o)
            out[f"{k}_table"] = Region(hexm((255, 255, 255), .45), .35, .30, e=0.12, o=o)
            out[f"{k}_glow"] = Region(hexm((255, 255, 255), .52), .25, .30, e=0.45, o=o)
        elif name != "Ruby":
            # the table of the crowned / brilliant cuts glows (a focal accent at 1080p; the web icons sparkle)
            out[f"{k}_glow"] = Region(out[f"{k}_glow"].hex, out[f"{k}_glow"].s, out[f"{k}_glow"].l, e=0.42, o=o)
        if dia:      # a pale ice-blue table under the white glint star (a white table swallowed the star)
            out[f"{k}_glow"] = Region(hexm((120, 190, 240), .30), .25, .30, e=0.30, o=o)
        out[f"{k}_star"] = Region(hexm((255, 255, 255), .80), 0.0, 0.0, e=1.0, o=o)
    return out


REGIONS["gem"] = gem_regions()


# darker hull mixes where a lighter / more emissive region (art review, fourth look) put its line tone or the bloom of
# an emissive accent next to the silhouette above the interactive dark limit (lum 64): the antidote's lime liquid,
# the ley fruit's emissive seams, the gel's highlight band
HULL_O = {"potion": {"anti": 0.55, "glass_anti": 0.55, "anti_hi": 0.55},
          "fruit": {"skin": 0.4, "vein": 0.4, "vein2": 0.4, "leaf": 0.5, "stem": 0.5, "rib": 0.5},
          # fifth look: the herb's hull p95 lum 58.3 / 64 left little margin; the pendant's chain / bail line tone
          "quest": {"gel": 0.5, "gel_hi": 0.5, "gel_core": 0.5, "herb_a": 0.78, "herb_b": 0.78, "herb_c": 0.78,
                    "herb_d": 0.78, "herb_e": 0.78, "stem": 0.78, "twine": 0.7, "petal": 0.7,
                    "chain": 0.8, "bail": 0.8}}
# thinner baked hull on the thin layered parts of two quest pickups (art review, fifth look: median ink rim 3.95 px on
# the herb, 4.06 px on the pendant against the 2 px interactive class — fifteen leaves and a coiled cord each carry
# their own hull, so the walk from a silhouette edge to the body crosses two or three hull layers). The UE outline
# WPO adds the same screen-constant term to every hull vertex (OutlinePx × PixelWorldSize − BakedWidth), so a part
# baked at k × the class width draws k × 2 px at the default W1 distance
HULL_SCALE = {"SM_Pickup_Quest_Herb": {"leaf_hull": 0.8, "crescent_hull": 0.8, "blossom_hull": 0.85},
              "SM_Pickup_Quest_Pendant": {"chain_hull": 0.75, "bail_hull": 0.75}}
# the per-asset ink exception (manifest ``inkGate.exception``): the measured median stays ~4 px after the thinning —
# the rim walk (``review.ink_stats``: hull coverage from a silhouette edge inward until the body is solid) crosses two or
# three hull layers where leaves / coil turns overlap or a part is thinner than its own two rims, and a flat part
# lying on the ground shows its far rim through the hull's up-offset at −50°; every single hull still draws ≤ 2 px
INK_EXCEPTION = {
    name: {"medianRimPxClass": 2.0, "reason": (
        "layered thin parts: the median rim counts 2–3 overlapping hull layers (leaf on leaf, coil on coil) and the "
        "far rim of flat parts lying on the ground; the thin parts' hulls are baked at " + ", ".join(
            f"{k.replace('_hull', '')} ×{v:g}" for k, v in sc.items()) + " of the class width (≤ 2 px each at "
        "the W1 default distance); gate: rim ≥ 1.5 px on ≥ 90 % of the edges, hull p95 lum ≤ 64")}
    for name, sc in HULL_SCALE.items()}


def thin_hull(obj, scale: dict[str, float]) -> dict[str, int]:
    """Pull the baked hull of the named hull parts toward their toon surface: hull vertex −= n × width × (1 − k)
    (n = the hull normal in ``AF_Data``; ``af_outline_width`` = the class width). → {part: vertices moved}."""
    names = json.loads(obj["af_parts"])
    me = obj.data
    part = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", part)
    col = me.attributes["AF_Data"]
    width = float(obj.get("af_outline_width", outline.WIDTH_CM[CLS] / 100.0))
    moved = {}
    seen = set()
    for poly, pid in zip(me.polygons, part.tolist()):
        if poly.material_index != outline.HULL_SLOT or names[pid] not in scale:
            continue
        k = scale[names[pid]]
        for vi in poly.vertices:
            if vi in seen:
                continue
            seen.add(vi)
            c = col.data[vi].color
            n = Vector((c[0] * 2 - 1, c[1] * 2 - 1, c[2] * 2 - 1))
            if n.length < 0.5:
                continue
            me.vertices[vi].co -= n.normalized() * width * (1.0 - k)
            moved[names[pid]] = moved.get(names[pid], 0) + 1
    me.update()
    return moved


def class_regions(prefix: str) -> dict:
    """The prefix's regions with the class outline mix (line tone); gem regions keep their own ``o``; ``HULL_O``
    darkens a few."""
    from dataclasses import replace
    regs = REGIONS[prefix]
    if prefix == "gem":
        return dict(regs)
    out = outline.ink_regions(regs, CLS)
    for k, o in HULL_O.get(prefix, {}).items():
        out[k] = replace(out[k], o=o)
    return out


def palette() -> Palette:
    pal = Palette(FAMILY)
    for pre in REGIONS:
        for k, r in class_regions(pre).items():
            pal.add(f"{pre}.{k}", r)
    return pal


def builder(asset_name: str, pal: Palette, prefix: str) -> M.Builder:
    b = M.Builder(asset_name, pal, prefix=prefix)
    b.regions(**class_regions(prefix))
    return b


def sockets(top: float, glint=(0.0, 0.0, None)) -> list[dict]:
    g = (glint[0], glint[1], top * 0.85 if glint[2] is None else glint[2])
    return [{"name": "beam", "pos": (0.0, 0.0, 0.0)}, {"name": "label", "pos": (0.0, 0.0, top + 0.18)},
            {"name": "glint", "pos": g}]


# ── loot bag ────────────────────────────────────────────────────────────────────────────────────────────
BAG_R, BAG_HB = 0.190, 0.285          # max body radius, body height (neck + ruffle above → ≈ 0.40 m)


def bag_surface(th: float, u: float) -> tuple[Vector, Vector]:
    """Point and outward normal of the sack body at azimuth ``th`` and normalised height ``u`` (0 … 1)."""
    def pt(t, w):
        prof = float(np.interp(w, [0.0, 0.035, 0.10, 0.28, 0.52, 0.74, 0.90, 1.0],
                               [0.70, 0.86, 0.95, 1.00, 0.96, 0.78, 0.52, 0.34]))
        wl = math.sin(math.pi * min(max(w, 0.0), 1.0)) ** 0.7
        lobe = 1.0 + wl * (0.055 * math.sin(3 * t + 0.7) + 0.028 * math.sin(5 * t + 2.1) + 0.018 * math.sin(2 * t))
        r = BAG_R * prof * lobe
        cx = 0.016 * w * w
        return Vector((cx + r * math.cos(t), 0.93 * r * math.sin(t), BAG_HB * w))
    p = pt(th, u)
    du, dt = 0.01, 0.01
    a = pt(th, min(u + du, 1.0)) - pt(th, max(u - du, 0.0))
    b = pt(th + dt, u) - pt(th - dt, u)
    n = b.cross(a).normalized()
    if n.dot(Vector((p.x, p.y, 0.0))) < 0:
        n = -n
    return p, n


def build_bag(pal: Palette):
    b = builder("SM_Loot_Bag", pal, "bag")
    n = 28
    us = [0.0, 0.035, 0.10, 0.19, 0.28, 0.40, 0.52, 0.63, 0.74, 0.83, 0.90, 0.96, 1.0]
    rings = [[bag_surface(TAU * i / n, u)[0] for i in range(n)] for u in us]
    # neck: gathered under the cord, then the pleated ruffle flaring open (8 pleats), dipping into the mouth
    top = BAG_HB
    neck = [(0.012, 0.30, 0.05), (0.028, 0.27, 0.07), (0.048, 0.31, 0.12), (0.074, 0.42, 0.20), (0.094, 0.50, 0.24),
            (0.104, 0.50, 0.24)]
    cx = 0.016
    for dz, k, pleat in neck:
        def rr(a, k=k, pleat=pleat):
            return BAG_R * k * (1.0 + pleat * math.cos(8 * a + 0.3) + 0.03 * math.sin(3 * a))
        rings.append(ring_pts(Vector((cx, 0.0, 0.0)), rr, n, top + dz))
    body = IK.loft_rings(rings, cap0="fan", cap1="fan", dome0=0.0, dome1=-0.045)
    bid = b.add(body, "cloth", name="sack", hull_pad=True)
    # folds: two shade creases running down from the neck, a third short one at the base
    for th, u0, u1, sw in ((2.25, 0.88, 0.30, 0.35), (4.05, 0.86, 0.36, -0.30), (0.75, 0.42, 0.10, 0.2)):
        pts, nrm = [], []
        for i in range(10):
            t = i / 9
            p, nn = bag_surface(th + sw * t * t, u0 + (u1 - u0) * t)
            pts.append(p)
            nrm.append(nn)
        b.add(IK.surface_strip(pts, nrm, 0.0075, lift=0.0010, depth=0.004), "fold", name="fold", outline=False,
              covered_by=bid)
    # a darned patch on the belly (four running stitches round it)
    pc_th, pc_u = 2.75, 0.40
    o3, i3 = [], []
    for j in range(5):
        row_o, row_i = [], []
        for i in range(5):
            th = pc_th + (i - 2) * 0.115
            u = pc_u + (j - 2) * 0.085
            p, nn = bag_surface(th, u)
            row_o.append(p + nn * 0.0035)
            row_i.append(p - nn * 0.004)
        o3.append(row_o)
        i3.append(row_i)
    b.add(C.thick_patch(o3, i3).auto_sharp(50), "patch", name="patch", outline=False, covered_by=bid)
    for side in range(4):
        for s in range(3):
            if side in (0, 1):
                th = pc_th + (-1 if side == 0 else 1) * 0.235
                u = pc_u + (s - 1) * 0.095
                d_th, d_u = 0.0, 0.035
            else:
                th = pc_th + (s - 1) * 0.13
                u = pc_u + (-1 if side == 2 else 1) * 0.175
                d_th, d_u = 0.06, 0.0
            a, na = bag_surface(th - d_th, u - d_u)
            c, nc = bag_surface(th + d_th, u + d_u)
            b.add(IK.surface_strip([a + na * 0.004, c + nc * 0.004], [na, nc], 0.006, lift=0.0008, depth=0.003),
                  "stitch", name="stitch", outline=False, covered_by=bid)
    # cord tied round the neck, knot on the front, two loose ends hanging down the sack
    zt = top + 0.028
    cord_r = BAG_R * 0.27 * 1.07 + 0.006
    b.add(closed_tube(Vector((cx, 0.0, zt)), cord_r, 0.0078, n=28, sides=8,
                      z_wave=lambda a: 0.003 * math.sin(2 * a)), "cord", name="cord", outline=False, covered_by=bid)
    kth = -0.85
    kn = Vector((cx + (cord_r + 0.008) * math.cos(kth), (cord_r + 0.008) * math.sin(kth), zt))
    b.add(IK.ell((0.017, 0.014, 0.013), tuple(kn), sides=10, rings=6), "cord", name="knot", outline=False,
          covered_by=bid)
    for dth, u_end in ((-0.20, 0.66), (0.16, 0.60)):
        pts = [kn]
        for i in range(1, 7):
            t = i / 6
            p, nn = bag_surface(kth + dth * t, 0.97 - (0.97 - u_end) * t)
            pts.append(p + nn * 0.010)
        b.add(M.tube(pts, [0.0068] * len(pts), sides=7, dome1=0.003), "cord", name="cord_end", outline=False,
              covered_by=bid)
        b.add(IK.ell((0.010, 0.010, 0.013), tuple(pts[-1] + (pts[-1] - pts[-2]).normalized() * 0.006),
                     sides=8, rings=5), "cord", name="cord_tip", outline=False, covered_by=bid)
    # a coin peeking out of the mouth (web: #FFF0B0 with a #8A6A20 line), tilted toward the open side
    rim, face = coin(0.036, 0.0095, sides=16)
    m = tilt(rx=62, rz=20, t=(cx - 0.006, 0.010, top + 0.088))
    crid = b.add(place(rim, m), "coin_rim", name="coin_rim", hull_pad=True)
    b.add(place(face, m), "coin", name="coin_face", outline=False, covered_by=crid)
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (cx - 0.006, 0.02, top + 0.11)), {"kind": "bag", "heightCm": round(top_z * 100, 1)}


# ── coins and gold piles ────────────────────────────────────────────────────────────────────────────────
def add_coin(b: M.Builder, m: Matrix, r: float = 0.030, h: float = 0.0085, hull=True, cover: int | None = None,
             ground: bool = True, sides: int = 12):
    """One coin at ``m``; ``ground``: lifted so its lowest point rests on the floor (tilted / on-edge strays).
    12 sides: a 6 cm coin is ~7 px at 1080p (the heap uses 10)."""
    rim, face = coin(r, h, sides=sides)
    if ground:
        lo = min((m @ v.co).z for v in rim.bm.verts)
        if lo < 0.0:
            m = Matrix.Translation((0.0, 0.0, -lo)) @ m
    if cover is not None:
        b.add(place(rim, m), "rim", name="coin_rim", outline=False, covered_by=cover)
        b.add(place(face, m), "face", name="coin_face", outline=False, covered_by=cover)
        return cover
    rid = b.add(place(rim, m), "rim", name="coin_rim", hull_pad=hull)
    b.add(place(face, m), "face", name="coin_face", outline=False, covered_by=rid if hull else None)
    return rid


def heap_coin(r: float = 0.030, h: float = 0.0085, sides: int = 8) -> tuple[M.Part, M.Part]:
    """(rim, top face) of a coin lying on the heap: 8 sides, the face only on top (the underside rests on the
    mound) — ~90 tris instead of 200; a 6 cm coin is ~7 px at 1080p."""
    rim = IK.revolve([(r * 0.84, -h / 2), (r, -h * 0.2), (r, h * 0.2), (r * 0.84, h / 2)], sides=sides).auto_sharp(40)
    face = M.cylinder(r * 0.80, r * 0.80, h * 0.30, h * 0.62, sides=sides).auto_sharp(30)
    return rim, face


def heap_coin2(r: float = 0.030, h: float = 0.0085, sides: int = 8) -> tuple[M.Part, M.Part]:
    """A light coin with a face on both sides (crest / edge coins tilt far enough to show their underside)."""
    rim, face = heap_coin(r, h, sides)
    face.merge(M.cylinder(r * 0.80, r * 0.80, -h * 0.62, -h * 0.30, sides=sides).auto_sharp(30))
    return rim, face


def stack(b: M.Builder, rng, xy, z0: float, n: int, r: float, h: float, sides: int = 8) -> None:
    """A short stack of light coins (each a little off-centre) sharing one padded cylinder hull: one 8-sided ink
    round the stack instead of a hull per coin."""
    rims = faces = None
    z = z0
    for k in range(n):
        m = tilt(rx=rng.uniform(-3, 3), rz=rng.uniform(0, 360), t=(xy[0] + rng.uniform(-0.003, 0.003),
                                                                    xy[1] + rng.uniform(-0.003, 0.003), z))
        rp, fp = heap_coin2(r, h, sides) if k == 0 else heap_coin(r, h, sides)
        rp, fp = place(rp, m), place(fp, m)
        rims = rp if rims is None else rims.merge(rp)
        faces = fp if faces is None else faces.merge(fp)
        z += h * 0.98
    lo = min(v.co.z for v in rims.bm.verts)
    lift = max(0.0, -lo)                                  # a tilted bottom coin on the floor: rest it on the floor
    rims.translate((0.0, 0.0, lift))
    faces.translate((0.0, 0.0, lift))
    proxy = M.cylinder(r, r, z0 - h / 2 + lift, z - h * 0.48 + lift, sides=sides).translate((xy[0], xy[1], 0.0))
    rid = b.add(rims, "rim", name="coin_stack", hull=proxy, hull_pad=True)
    b.add(faces, "face", name="coin_face", outline=False, covered_by=rid)


def build_heap(b: M.Builder, rng, r: float, h: float) -> None:
    """``SM_Pickup_Gold_L``: a heap of coins (art review, fourth look: the mound with flush, sunk octagons read as
    dough or cheese with embossed shapes, both in the close-up and the 1080p crop). The mound is low and a shade
    darker (the shadowed coins inside the heap); every coin on it rests *on* the surface with its rim band showing
    (lifted by its half-height, tilted ±16° off the slope) on a Poisson-spaced layout; four coins stuck upright into
    the crest and a three-coin stack on top break the silhouette with coin rims (as Gold_M's stack); the outer ring
    carries its own hulls (+3 coins slid off the foot, tilted out); the stack of three and the leaning coin beside
    keep the pile's base readable. The leaning coin faces between the key light and the camera at the 'front' yaw
    and uses the low-shadow coin regions so it stays gold in the shade band at any spawn yaw.
    Ink (art review 4): the mound keeps its own (unpadded) hull; only coins that reach the silhouette carry hulls."""
    hr, hh = 0.150, 0.062

    def hz(x, y):
        d = math.hypot(x / hr, y / (hr * 0.86))
        return hh * max(0.0, 1.0 - d * d) ** 0.85

    def slope_n(x, y):
        gx = (hz(x + 0.004, y) - hz(x - 0.004, y)) / 0.008
        gy = (hz(x, y + 0.004) - hz(x, y - 0.004)) / 0.008
        return Vector((-gx, -gy, 1.0)).normalized()

    rings = []
    for k, f in enumerate((1.0, 0.86, 0.68, 0.48, 0.26, 0.08)):
        pts = []
        for i in range(20):
            a = TAU * i / 20
            wob = 1.0 + 0.07 * math.sin(4 * a + 1.3) + 0.04 * math.sin(7 * a)
            x, y = hr * f * wob * math.cos(a), hr * 0.86 * f * wob * math.sin(a)
            pts.append(Vector((x, y, hz(x / wob, y / wob) if k else 0.0)))
        rings.append(pts)
    b.add(IK.loft_rings(rings, cap0="fan", cap1="fan", dome1=0.004), "heap", name="mound")

    def put(x, y, lift, tilt_deg, spin, hull, both=False, sides=8, rim="rim", face="face", up_extra=None):
        n = slope_n(x, y) if up_extra is None else up_extra
        rot = n.to_track_quat("Z", "Y").to_matrix().to_4x4() @ Matrix.Rotation(spin, 4, "Z")
        rot = rot @ Matrix.Rotation(math.radians(tilt_deg), 4, "X")
        m = Matrix.Translation(Vector((x, y, hz(x, y) + lift))) @ rot
        rp, fp = (heap_coin2 if both else heap_coin)(r, h, sides)
        rp, fp = place(rp, m), place(fp, m)
        lo = min(v.co.z for v in rp.bm.verts)
        if lo < 0.0005:                                   # an edge coin dipping under the floor: lift it
            rp.translate((0.0, 0.0, 0.0005 - lo))
            fp.translate((0.0, 0.0, 0.0005 - lo))
        if hull:
            rid = b.add(rp, rim, name="coin_rim", hull_pad=True)
            b.add(fp, face, name="coin_face", outline=False, covered_by=rid)
        else:
            b.add(rp, rim, name="coin_rim_in", outline=False)
            b.add(fp, face, name="coin_face_in", outline=False)

    # surface coins: Poisson-spaced (≥ 4.6 cm apart), resting on the mound with the rim band above the surface
    placed: list[tuple[float, float]] = []
    tries = 0
    while len(placed) < 14 and tries < 900:
        tries += 1
        a, d = rng.uniform(0, TAU), math.sqrt(rng.uniform(0.0, 0.90))
        x, y = hr * d * math.cos(a), hr * 0.86 * d * math.sin(a)
        if any(math.hypot(x - px, y - py) < 0.050 for px, py in placed):
            continue
        placed.append((x, y))
        put(x, y, h * 0.42, rng.uniform(-16, 16), rng.uniform(0, TAU), hull=d > 0.52)
    # crest: three coins stuck into the top at 45–60°, rims up, and a stack of three on the summit
    for k, (ang, dd, tilt_d) in enumerate(((0.4, 0.34, 52.0), (2.6, 0.30, 60.0), (4.5, 0.36, 46.0))):
        x, y = hr * dd * math.cos(ang), hr * 0.86 * dd * math.sin(ang)
        out = Vector((math.cos(ang), math.sin(ang), 0.0))
        n = (slope_n(x, y) + out * math.tan(math.radians(tilt_d))).normalized()
        put(x, y, r * math.sin(math.radians(tilt_d)) * 0.55, 0.0, rng.uniform(0, TAU), hull=True, both=True,
            up_extra=n)
    stack(b, rng, (0.012, -0.010), hz(0.012, -0.010) + h * 0.5, 3, r, h)
    # the foot: three coins slid off the heap, tilted out (their own hulls: they set the silhouette's lower edge)
    for ang in (1.1, 2.9, 4.6):
        x, y = hr * 0.98 * math.cos(ang), hr * 0.86 * 0.98 * math.sin(ang)
        out = Vector((math.cos(ang), math.sin(ang), 0.0))
        n = (Vector((0, 0, 1)) - out * 0.35).normalized()
        put(x, y, h * 0.5 + 0.004, 0.0, rng.uniform(0, TAU), hull=True, up_extra=n)
    stack(b, rng, (0.160, -0.060), h / 2, 3, r, h)
    # the leaning coin: face between the key light (+X +Y) and the camera (−X +Y) at the front yaw, tilted back 40°
    lean = Vector((-0.707, -0.707, 0.85)).normalized()          # front yaw: camera at local −Y, key light from −X
    m = Matrix.Translation(Vector((-0.150, -0.075, 0.0))) @ lean.to_track_quat("Z", "X").to_matrix().to_4x4()
    rim_p, face_p = coin(r, h, sides=10)
    lo = min((m @ v.co).z for v in rim_p.bm.verts)
    m = Matrix.Translation((0.0, 0.0, -lo)) @ m
    rid = b.add(place(rim_p, m), "rim_lean", name="coin_rim", hull_pad=True)
    b.add(place(face_p, m), "face_lean", name="coin_face", outline=False, covered_by=rid)


def build_coin_fx(pal: Palette):
    """``SM_Pickup_GoldCoin``: the ``reward.gold`` burst coin (VFX mesh; no outline slot — ``M_AF_FX_Mesh`` replaces
    every slot at runtime). Disc in the XY plane, centred (the VFX system tumbles it)."""
    b = builder("SM_Pickup_GoldCoin", pal, "coin")
    rim, face = coin(0.065, 0.016, sides=20)
    b.add(rim, "rim", name="coin_rim", outline=False)
    b.add(face, "face", name="coin_face", outline=False)
    obj = asset.finish_mesh(b, pal, "none")
    return obj, [], {"kind": "fx_coin"}


GOLD_PILES = {   # amount bands (web gold rewards: monster gold 1–60 in Ch1, hidden pile +200)
    "S": dict(amount=(1, 24)), "M": dict(amount=(25, 99)), "L": dict(amount=(100, None)),
}


def build_gold(pal: Palette, size: str):
    name = f"SM_Pickup_Gold_{size}"
    b = builder(name, pal, "coin")
    rng = scene.rng(name, "coins")
    r, h = 0.030, 0.0085
    if size == "S":
        # three coins: one flat, one leaning on it, one on edge rolling away
        add_coin(b, tilt(t=(0.0, 0.0, h / 2)))
        add_coin(b, tilt(rx=22, rz=35, t=(0.030, 0.018, h * 0.5 + 0.012)))
        add_coin(b, tilt(rx=88, rz=-60, t=(-0.044, -0.030, r)))
    elif size == "M":
        # a stack of six (each a little off-centre) + four strays
        z = h / 2
        for k in range(6):
            add_coin(b, tilt(rz=rng.uniform(0, 360), t=(rng.uniform(-0.003, 0.003), rng.uniform(-0.003, 0.003), z)))
            z += h * 0.98
        for k, (x, y, rx, rz) in enumerate(((0.062, 0.010, 14, 10), (-0.050, 0.040, 8, 80), (0.014, -0.064, 20, 200),
                                            (-0.040, -0.044, 82, 135))):
            zz = r if rx > 60 else h / 2 + 0.006 * math.sin(math.radians(rx))
            add_coin(b, tilt(rx=rx, rz=rz, t=(x, y, zz)))
    else:
        build_heap(b, rng, r, h)
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    lo = GOLD_PILES[size]["amount"]
    return obj, sockets(top_z), {"kind": "gold", "goldAmount": {"min": lo[0], "max": lo[1]}}


# ── potions ─────────────────────────────────────────────────────────────────────────────────────────────
POTIONS = {   # web potionSpec sizes (s slim vial · m round · l round + gold collar); amounts from groundPotions.
    # ``level``: the liquid fills the bulb up to the shoulder (art review 5: at −50° pitch the web's "lower ⅔" left a
    # colourless glass dome dominating the silhouette)
    "S": dict(prof=[(0.040, 0.0), (0.060, 0.012), (0.072, 0.045), (0.071, 0.088), (0.060, 0.128), (0.042, 0.160),
                    (0.028, 0.180), (0.026, 0.232)], level=0.150, neck=0.026, lip=0.232, cork=0.050),
    "M": dict(prof=[(0.056, 0.0), (0.086, 0.012), (0.104, 0.050), (0.108, 0.104), (0.100, 0.150), (0.080, 0.190),
                    (0.046, 0.214), (0.035, 0.226), (0.033, 0.268)], level=0.194, neck=0.033, lip=0.268, cork=0.052),
    "L": dict(prof=[(0.070, 0.0), (0.105, 0.014), (0.127, 0.060), (0.134, 0.124), (0.124, 0.188), (0.098, 0.236),
                    (0.056, 0.264), (0.043, 0.278), (0.041, 0.336)], level=0.238, neck=0.041, lip=0.336, cork=0.058,
              collar=0.292),
}
POTION_KINDS = {"HP": ("hp", {"S": 50, "M": 150, "L": 400}), "MP": ("mp", {"S": 30, "M": 80})}


def _split_profile(prof, level):
    """(lower, upper) lathe profiles split at height ``level`` (the radius interpolated there)."""
    for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
        if z0 <= level <= z1:
            rl = r0 + (r1 - r0) * (level - z0) / (z1 - z0)
            lo = [p for p in prof if p[1] < level] + [(rl, level)]
            hi = [(rl, level)] + [p for p in prof if p[1] > level]
            return lo, hi, rl
    raise ValueError("level outside the profile")


GLINT_PHASES = (3.95, 0.81)      # local azimuths: screen upper-left (the key light) at the 'front' yaw + its opposite


def glass_glints(b: M.Builder, prof, cover: int, phases=GLINT_PHASES):
    """Painted specular streaks on the glass (web: white streak + dot) on the lit upper-left at the 'front' yaw
    (art review 5: they sat on the shaded lower-right) and, for other spawn yaws, the opposite side; ~45° in from
    the silhouette so the ink never swallows them, the dot above the streak's top."""
    zs = [p[1] for p in prof]
    zlo, zhi = zs[0] + (zs[-1] - zs[0]) * 0.34, zs[-1] - (zs[-1] - zs[0]) * 0.30

    def rad(z):
        return float(np.interp(z, zs, [p[0] for p in prof]))
    for ph in phases:
        pts, nrm = [], []
        for i in range(8):
            t = i / 7
            z = zlo + (zhi - zlo) * t
            a = ph - 0.18 * math.sin(math.pi * t)
            r_ = rad(z)
            pts.append(Vector((r_ * math.cos(a), r_ * math.sin(a), z)))
            nrm.append(Vector((math.cos(a), math.sin(a), 0.25 * (0.5 - t))).normalized())
        b.add(IK.surface_strip(pts, nrm, 0.0085, lift=0.0010, depth=0.003), "glint", name="glint", outline=False,
              covered_by=cover)
        z = zhi + (zs[-1] - zhi) * 0.10
        a = ph + 0.30
        p = Vector((rad(z) * math.cos(a), rad(z) * math.sin(a), z))
        b.add(IK.ell((0.0055, 0.0055, 0.0055), tuple(p), sides=8, rings=5), "glint", name="glint_dot",
              outline=False, covered_by=cover)


def bottle_top(b: M.Builder, neck: float, lip: float, cork: float, cover: int, collar: float | None = None):
    b.add(M.lathe([(neck - 0.002, lip - 0.008), (neck + 0.009, lip - 0.005), (neck + 0.010, lip + 0.004),
                   (neck - 0.002, lip + 0.008)], sides=14, cap0="fan", cap1="fan").auto_sharp(50), "glass",
          name="lip", outline=False, covered_by=cover)
    ck = M.lathe([(neck * 0.84, lip - 0.004), (neck * 0.93, lip + 0.010), (neck * 0.96, lip + cork * 0.62),
                  (neck * 0.86, lip + cork * 0.92), (neck * 0.40, lip + cork), (0.0006, lip + cork + 0.002)],
                 sides=12, cap0="fan", cap1="fan")
    cid = b.add(ck, "cork", name="cork", hull_pad=True)
    b.add(IK.ring_y(0.0, neck * 0.965, 0.0055, 0.004, sides=12).rotate("x", 90).translate((0, 0, lip + cork * 0.42)),
          "cork_line", name="cork_line", outline=False, covered_by=cid)
    if collar is not None:
        b.add(M.lathe([(neck - 0.002, collar - 0.009), (neck + 0.008, collar - 0.007), (neck + 0.009, collar + 0.007),
                       (neck - 0.002, collar + 0.009)], sides=14, cap0="fan", cap1="fan").auto_sharp(50), "collar",
              name="collar", outline=False, covered_by=cover)


def build_potion(pal: Palette, kind: str, size: str):
    liq, amounts = POTION_KINDS[kind]
    name = f"SM_Pickup_Potion{kind}" + ("" if size == "M" else f"_{size}")
    b = builder(name, pal, "potion")
    g = POTIONS[size]
    lo, hi, rl = _split_profile(g["prof"], g["level"])
    full = M.lathe(g["prof"], sides=20, cap0="fan", cap1="fan")
    lid = b.add(M.lathe(lo, sides=20, cap0="fan", cap1="ngon"), liq, name="liquid", hull=full, hull_pad=True)
    b.add(M.lathe(hi, sides=20, cap0=None, cap1="fan"), f"glass_{liq}", name="glass", outline=False, covered_by=lid)
    b.add(M.lathe([(rl - 0.002, g["level"] - 0.0035), (rl + 0.0018, g["level"] - 0.0030),
                   (rl + 0.0018, g["level"] + 0.0030), (rl - 0.002, g["level"] + 0.0035)], sides=20, cap0="fan",
                  cap1="fan"), f"{liq}_hi", name="meniscus", outline=False, covered_by=lid)
    glass_glints(b, g["prof"], lid)
    bottle_top(b, g["neck"], g["lip"], g["cork"], lid, g.get("collar"))
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (0.0, 0.0, g["level"] * 0.62)), {"kind": "potion", "potion": liq,
                                                               "potionAmount": amounts.get(size), "size": size,
                                                               "glintZ": round(g["level"] * 0.62, 4),
                                                               "flaskRadiusCm": round(max(r for r, _ in g["prof"])
                                                                                      * 100.0, 1)}


def build_antidote(pal: Palette):
    b = builder("SM_Pickup_Antidote", pal, "potion")
    rings = [(0.0, 0.074, 0.060), (0.012, 0.086, 0.071), (0.160, 0.088, 0.073), (0.186, 0.070, 0.057),
             (0.206, 0.038, 0.036), (0.214, 0.032, 0.032), (0.256, 0.031, 0.031)]
    level = 0.172                       # up to the shoulder (art review 5: it read as a white bottle)
    t = (level - 0.160) / (0.186 - 0.160)
    lx, ly = 0.088 + (0.070 - 0.088) * t, 0.073 + (0.057 - 0.073) * t
    full = M.loft_z(rings, sides=20, exp=3.6)
    lo = [r for r in rings if r[0] < level] + [(level, lx, ly)]
    hi = [(level, lx, ly)] + [r for r in rings if r[0] > level]
    lid = b.add(M.loft_z(lo, sides=20, exp=3.6, cap1="ngon"), "anti", name="liquid", hull=full, hull_pad=True)
    b.add(M.loft_z(hi, sides=20, exp=3.6, cap0=None), "glass_anti", name="glass", outline=False, covered_by=lid)
    b.add(M.loft_z([(level - 0.0035, lx + 0.0005, ly + 0.0005), (level + 0.0035, lx + 0.0005, ly + 0.0005)],
                   sides=20, exp=3.6), "anti_hi", name="meniscus", outline=False, covered_by=lid)
    # paper label on both broad faces with the web's leaf glyph
    for sg in (1, -1):
        lab = M.box((0.100, 0.006, 0.072), (0.0, sg * 0.0745, 0.088), bevel=0.003)
        b.add(lab, "label", name="label", outline=False, covered_by=lid)
        leaf = lens_leaf(0.046, 0.026, 0.004).translate((-0.023, 0.0, 0.0)).rotate("z", 0.0)
        leaf.rotate("y", -55).rotate("x", 90 * sg).translate((0.0, sg * 0.0785, 0.088))
        b.add(leaf, "leafmark", name="leafmark", outline=False, covered_by=lid)
    prof = [(0.088 * 0.95, z) for z, *_ in rings[1:4]]
    glass_glints(b, [(0.086, 0.02), (0.088, 0.16)], lid)
    del prof
    bottle_top(b, 0.031, 0.256, 0.050, lid)
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (0.0, 0.0, level * 0.62)), {"kind": "potion", "potion": "antidote",
                                                           "glintZ": round(level * 0.62, 4)}


# ── scrolls ─────────────────────────────────────────────────────────────────────────────────────────────
SW, SD, ROLL_R = 0.135, 0.100, 0.030          # sheet half-width (x), half-depth (y), roll radius


def sheet_z(y: float) -> float:
    return 0.010 + 0.016 * (abs(y) / SD) ** 3


def build_scroll(pal: Palette, kind: str):
    name = f"SM_Pickup_Scroll_{kind.upper()}"
    b = builder(name, pal, "scroll")
    acc, seal, ribbon = kind, f"{kind}_seal", f"{kind}_ribbon"
    nx, ny = 10, 10
    outer = [[Vector((-SW + 2 * SW * i / nx, -SD + 2 * SD * j / ny, sheet_z(-SD + 2 * SD * j / ny) + 0.0025))
              for i in range(nx + 1)] for j in range(ny + 1)]
    inner = [[p - Vector((0, 0, 0.005)) for p in row] for row in outer]
    sid = b.add(C.thick_patch(outer, inner), "sheet", name="sheet", hull_pad=True)
    for sg in (1, -1):
        y = sg * (SD + ROLL_R * 0.55)
        roll = M.tube([(-SW - 0.012, y, ROLL_R), (SW + 0.012, y, ROLL_R)], [ROLL_R, ROLL_R], sides=14, dome0=0.0,
                      dome1=0.0)
        rid = b.add(roll, "roll", name="roll", hull_pad=True)
        b.add(IK.ring_y(0.0, ROLL_R * 0.65, 0.004, ROLL_R * 0.4, sides=12).rotate("z", 90)
              .translate((SW + 0.012, y, ROLL_R)), "knob", name="roll_end", outline=False, covered_by=rid)
        b.add(IK.ring_y(0.0, ROLL_R * 0.65, 0.004, ROLL_R * 0.4, sides=12).rotate("z", -90)
              .translate((-SW - 0.012, y, ROLL_R)), "knob", name="roll_end", outline=False, covered_by=rid)
        for sx in (1, -1):
            b.add(M.tube([(sx * (SW + 0.010), y, ROLL_R), (sx * (SW + 0.030), y, ROLL_R)], [0.011, 0.012], sides=10,
                         dome0=0.0, dome1=0.006), "knob", name="rod", hull_pad=True)

    def on_sheet(x, y, lift=0.0032):
        return Vector((x, y, sheet_z(y) + lift))
    up = Vector((0, 0, 1))
    if kind == "tp":
        # the web swirl (40 points, r 1 → 15 icon units over 0.42 rad steps) scaled onto the sheet
        pts = []
        for i in range(41):
            a = i * 0.42
            rr = (1 + i * 0.36) / 15.4 * 0.074
            pts.append(on_sheet(rr * math.cos(a), rr * math.sin(a) * 0.9))
        b.add(IK.surface_strip(pts, [up] * len(pts), 0.0095, lift=0.0010, depth=0.0025), acc, name="glyph",
              outline=False, covered_by=sid)
    else:
        # the eye: a bold dark-amber almond (pointed corners) round a near-black pupil — the first pass's thin gold
        # ring on cream vanished at 1080p, leaving TP / ID told apart by the ribbon only (art review 12)
        el = []
        for i in range(29):
            a = TAU * i / 28
            el.append(on_sheet(0.082 * math.cos(a), 0.046 * math.sin(a) * abs(math.sin(a)) ** 0.35))
        b.add(IK.surface_strip(el, [up] * len(el), 0.0150, lift=0.0010, depth=0.0025), acc, name="glyph",
              outline=False, covered_by=sid)
        b.add(M.cylinder(0.027, 0.027, sheet_z(0) + 0.0028, sheet_z(0) + 0.0052, sides=16), "id_pupil", name="pupil",
              outline=False, covered_by=sid)
    for yy in (0.058, -0.050):
        ln = [on_sheet(-SW * 0.72 + SW * 1.44 * t / 6, yy) for t in range(7)]
        b.add(IK.surface_strip(ln, [up] * len(ln), 0.0045, lift=0.0008, depth=0.0025), "ink", name="line",
              outline=False, covered_by=sid)
    # ribbon tails from the near roll + wax seal on the sheet edge (web: two tails, blob seal with a ring)
    y0 = -(SD + ROLL_R * 0.55)
    for dx, ang in ((0.030, -18.0), (0.058, 12.0)):
        p0 = Vector((dx, y0 - ROLL_R * 0.2, ROLL_R * 0.9))
        d = Vector((math.sin(math.radians(ang)), -1.0, 0.0)).normalized()
        path = [p0, p0 + d * 0.030 + Vector((0, 0, -0.018)), p0 + d * 0.060 + Vector((0, 0, -0.024)),
                p0 + d * 0.085 + Vector((0, 0, -0.025))]
        for p in path[1:]:
            p.z = max(p.z, 0.004)
        b.add(M.strap(path, 0.017, 0.004, up=(0, 0, 1)), ribbon, name="ribbon", hull_pad=True)
    sc = Vector((0.045, y0 - 0.004, ROLL_R * 2.0 + 0.002))
    seal_o = [(0.022 * (1 + 0.10 * math.sin(5 * a + 0.4)) * math.cos(a), 0.022 * (1 + 0.10 * math.sin(5 * a + 0.4))
               * math.sin(a)) for a in [TAU * i / 16 for i in range(16)]]
    sp = plate_xy(seal_o, -0.005, 0.005, rings=2)
    sp.rotate("x", -12).translate(tuple(sc))
    eid = b.add(sp, seal, name="seal", hull_pad=True)
    ring = [sc + Matrix.Rotation(math.radians(-12), 3, "X") @ Vector((0.012 * math.cos(TAU * i / 14),
                                                                      0.012 * math.sin(TAU * i / 14), 0.0058))
            for i in range(15)]
    nr = Matrix.Rotation(math.radians(-12), 3, "X") @ Vector((0, 0, 1))
    b.add(IK.surface_strip(ring, [nr] * len(ring), 0.0035, lift=0.0006, depth=0.002), ribbon, name="seal_ring",
          outline=False, covered_by=eid)
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (0.0, 0.0, 0.03)), {"kind": "scroll", "scroll": kind}


# ── ley fruit ───────────────────────────────────────────────────────────────────────────────────────────
def fruit_surface(a: float, z: float) -> tuple[Vector, Vector]:
    zs = [0.0, 0.018, 0.06, 0.11, 0.155, 0.188, 0.205]
    rs = [0.030, 0.072, 0.097, 0.099, 0.082, 0.050, 0.020]

    def pt(a_, z_):
        r = float(np.interp(z_, zs, rs)) * (1.0 + 0.045 * math.sin(3 * a_ + 0.5) * math.sin(math.pi * z_ / 0.205))
        return Vector((r * math.cos(a_), r * math.sin(a_), z_))
    p = pt(a, z)
    t1 = pt(a + 0.01, z) - pt(a - 0.01, z)
    t2 = pt(a, min(z + 0.004, 0.205)) - pt(a, max(z - 0.004, 0.0))
    n = t1.cross(t2).normalized()
    if n.dot(Vector((p.x, p.y, 0))) < 0:
        n = -n
    return p, n


def build_fruit(pal: Palette):
    b = builder("SM_Pickup_LeyFruit", pal, "fruit")
    n = 20
    zs = [0.0, 0.006, 0.018, 0.04, 0.06, 0.085, 0.11, 0.135, 0.155, 0.173, 0.188, 0.199, 0.205]
    rings = [[fruit_surface(TAU * i / n, z)[0] for i in range(n)] for z in zs]
    fid = b.add(IK.loft_rings(rings, cap0="fan", cap1="fan", dome1=-0.006), "skin", name="fruit", hull_pad=True)
    # two seams on the 'front' half (the camera sits at local −Y = azimuth 4.71 at the front yaw), one behind
    for a0, w, reg in ((4.15, 0.0175, "vein"), (5.30, 0.0135, "vein2"), (1.6, 0.0120, "vein2")):
        pts, nrm = [], []
        for i in range(12):
            t = i / 11
            z = 0.020 + 0.170 * t
            p, nn = fruit_surface(a0 + 0.30 * math.sin(math.pi * t * 1.2), z)
            pts.append(p)
            nrm.append(nn)
        b.add(IK.surface_strip(pts, nrm, w, lift=0.0014, depth=0.003), reg, name="vein", outline=False,
              covered_by=fid)
    stem = [Vector((0.0, 0.0, 0.192)), Vector((0.004, 0.0, 0.222)), Vector((0.014, 0.0, 0.246)),
            Vector((0.028, 0.0, 0.260))]
    b.add(M.tube(stem, [0.0085, 0.0075, 0.0068, 0.0060], sides=8, dome1=0.003), "stem", name="stem", hull_pad=True)
    leaf = lens_leaf(0.105, 0.050, 0.006, curl=0.018)
    m = tilt(rx=-12, ry=-22, rz=-25, t=(0.012, 0.0, 0.236))
    lid = b.add(place(leaf, m), "leaf", name="leaf", hull_pad=True)
    rib = [m @ Vector((0.105 * t / 6, 0.0, 0.0036 + 0.018 * (t / 6) ** 2)) for t in range(7)]
    b.add(IK.surface_strip(rib, [(m.to_3x3() @ Vector((0, 0, 1))).normalized()] * 7, 0.004, lift=0.0006,
                           depth=0.002), "rib", name="rib", outline=False, covered_by=lid)
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (0.0, 0.0, 0.12)), {"kind": "fruit", "glintZ": 0.12}


# ── gems ────────────────────────────────────────────────────────────────────────────────────────────────
def gem_cut(kind: str, r: float, sides: int | None = None) -> list[tuple[float, float]]:
    """Web ``gemCut`` girdle outlines (IconKit.ts), x across, y depth (the icon's y → the gem's +Y); ``sides``
    overrides the round / oval girdle count (the brilliant tiers: 16)."""
    pts = []
    if kind in ("round", "oval"):
        k = 0.8 if kind == "oval" else 1.0
        n = sides or (12 if kind == "round" else 10)
        for i in range(n):
            a = -math.pi / 2 + TAU * i / n
            pts.append((math.cos(a) * r * k, math.sin(a) * r))
    elif kind == "emerald":
        w, h, c = r * 0.72, r, r * 0.28
        pts = [(-w + c, -h), (w - c, -h), (w, -h + c), (w, h - c), (w - c, h), (-w + c, h), (-w, h - c), (-w, -h + c)]
    elif kind == "pear":
        for i in range(11):
            a = -math.pi / 2 + TAU * i / 11
            top = 1 - 0.55 * (-math.sin(a)) ** 3 if math.sin(a) < 0 else 1
            pts.append((math.cos(a) * r * 0.78 * top, math.sin(a) * r + (-r * 0.12 if math.sin(a) < 0 else 0)))
    elif kind == "trillion":
        # web trillion (corners + bowed edge midpoints), the corners chamfered: a needle-sharp 60° point carries
        # no ink at 1080p on a 9 cm stone
        for i in range(3):
            a = -math.pi / 2 + TAU * i / 3
            b_ = a + math.pi / 3
            for da in (-0.20, 0.20):
                pts.append((math.cos(a + da) * r * 0.90, math.sin(a + da) * r * 0.90))
            pts.append((math.cos(b_) * r * 0.62, math.sin(b_) * r * 0.62))
    return C.ccw(pts)


def gem_part(outline_xy, r: float, chipped: bool, crown: float = 0.40, table: float = 0.54, steps: int = 1,
             pavilion: float = 0.78) -> M.Part:
    """Faceted gem, table up: girdle band, crown (each quad split into two star facets, af_sub 0 / 1; ``steps`` 2 =
    a second, steeper crown ring: the 'crowned' tiers), table (af_sub 2), pavilion to a culet (af_sub 3 / 0
    alternating). ``crown`` / ``pavilion`` heights in units of ``r``. Culet at z = 0."""
    pts = list(outline_xy)
    if chipped:   # web: knock a corner off, roughen the girdle
        pts = [(x * 0.86, y * 0.90) if i % 3 == 1 else (x, y) for i, (x, y) in enumerate(pts)]
        del pts[int(len(pts) * 0.3)]
    n = len(pts)
    g, hc, hp = r * 0.10, r * crown, r * pavilion
    toff = (-0.05 * r, -0.04 * r)
    bm = bmesh.new()
    sub = bm.faces.layers.int.new("af_sub")
    z0 = hp
    g0 = [bm.verts.new((x, y, z0)) for x, y in pts]
    g1 = [bm.verts.new((x, y, z0 + g)) for x, y in pts]
    if steps == 2:
        km = 0.5 * (1.0 + table) + 0.08
        mid = [bm.verts.new((x * km + toff[0] * 0.5, y * km + toff[1] * 0.5, z0 + g + hc * 0.42)) for x, y in pts]
        rings = [g1, mid]
    else:
        rings = [g1]
    tb = [bm.verts.new((x * table + toff[0], y * table + toff[1], z0 + g + hc)) for x, y in pts]
    rings.append(tb)
    cul = bm.verts.new((0.0, 0.0, 0.0))

    def face(vs, tag):
        f = bm.faces.new(vs)
        f[sub] = tag
    for i in range(n):
        j = (i + 1) % n
        face((g0[i], g0[j], g1[j], g1[i]), 1)
        for k, (ra, rb) in enumerate(zip(rings, rings[1:])):
            face((ra[i], ra[j], rb[j]), 0 if (i + k) % 2 == 0 else 1)
            face((ra[i], rb[j], rb[i]), 1 if (i + k) % 2 == 0 else 0)
        face((g0[j], g0[i], cul), 3 if i % 2 == 0 else 0)
    face(list(tb), 2)
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).flat()


def build_gem(pal: Palette, gname: str, tier: int):
    name = f"SM_Pickup_Gem_{gname}_{tier}"
    hx, cut = GEMS[gname]
    k = gname.lower()
    b = builder(name, pal, "gem")
    r = GEM_R[tier]
    form = dict(GEM_FORM[tier])
    sides = form.pop("sides", None)
    part = gem_part(gem_cut(cut, r, sides), r, tier == 1, **form)
    dia = gname == "Diamond"
    table = f"{k}_glow" if (tier >= 3 or (dia and tier >= 2)) else f"{k}_table"
    tz = max(v.co.z for v in part.bm.verts)
    b.add(part, f"{k}_a", name="gem", sub_regions={1: f"{k}_b", 2: table, 3: f"{k}_deep"})
    if tier >= GEM_STAR_TIER or dia:
        # the glint: a four-point emissive star on the table (web facetGem sparkle; fourth look: no focal accent)
        tb = form.get("table", 0.54)
        R_ = r * tb * (0.80 if tier >= 4 else 0.70)
        star = []
        npts = 8 if tier >= 5 else 4                     # the perfect cut: an eight-point star (web tier-5 rays)
        for i in range(2 * npts):
            a_ = math.pi / 4 + math.pi * i / npts
            rr = (R_ if (i // 2) % 2 == 0 or npts == 4 else R_ * 0.62) if i % 2 == 0 else R_ * 0.22
            star.append((rr * math.cos(a_) - 0.05 * r, rr * math.sin(a_) - 0.04 * r))
        b.add(plate_xy(star, tz - 0.0004, tz + 0.0009, rings=2), f"{k}_star", name="glint_star", outline=False)
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    base_id = f"g_{k}_{tier}"
    return obj, sockets(top_z, (0.0, 0.0, top_z)), {"kind": "gem", "gem": k, "tier": tier, "baseId": base_id,
                                                    "cut": cut, "form": ("chipped", "faceted", "crowned",
                                                                         "brilliant", "perfect")[tier - 1]}


# ── quest items (§9.3: herb, gel, pendant) ──────────────────────────────────────────────────────────────
def build_quest_herb(pal: Palette):
    """Bundle of five leafy sprigs tied with twine, lying on the ground (QuestItemIcons.ts herb)."""
    b = builder("SM_Pickup_Quest_Herb", pal, "quest")
    rng = scene.rng("SM_Pickup_Quest_Herb", "leaves")
    cols = ["herb_a", "herb_b", "herb_c", "herb_d", "herb_e"]
    tie = Vector((-0.050, 0.0, 0.024))
    for k in range(5):
        fan = math.radians((k - 2) * 9.0)
        d = Vector((math.cos(fan), math.sin(fan), 0.0))
        z_lift = 0.010 + 0.012 * (1 - abs(k - 2) / 2)
        base = Vector((-0.150, (k - 2) * 0.006, 0.0064))
        mid = tie + Vector((0, (k - 2) * 0.007, z_lift * 0.4))
        tip = tie + d * (0.180 + 0.02 * math.cos(k * 1.7)) + Vector((0, 0, 0.030 + z_lift))
        path = [base, mid, mid + (tip - mid) * 0.5 + Vector((0, 0, 0.008)), tip]
        b.add(M.tube(path, [0.0062, 0.0058, 0.0050, 0.0040], sides=7, dome0=0.002, dome1=0.003), "stem",
              name="stem", hull_pad=True)
        for j, t in enumerate((0.36, 0.66, 0.97)):
            p = mid + (tip - mid) * t
            side = 1 if (j + k) % 2 == 0 else -1
            ln = 0.072 - 0.008 * j + rng.uniform(-0.005, 0.005)
            if j == 2:
                side, ln = 0, 0.060
            leaf = lens_leaf(ln, ln * 0.50, 0.0065, curl=0.010)
            yaw = math.degrees(math.atan2(d.y, d.x)) + side * 42.0
            m = tilt(rx=side * -6.0, ry=-12.0, rz=yaw, t=tuple(p + Vector((0, 0, 0.004 * j))))
            b.add(place(leaf, m), cols[(k + j) % 5], name="leaf", hull_pad=True)
    # crescent leaf crowning the middle sprig (the web's moon-curl)
    cres = [(0.0, 0.0)] + [(0.052 * math.sin(math.pi * t), 0.050 * t - 0.004 * math.sin(math.pi * t))
                           for t in [i / 8 for i in range(1, 8)]] + [(0.0, 0.050)] + \
           [(0.030 * math.sin(math.pi * t), 0.050 * (1 - t)) for t in [i / 8 for i in range(1, 8)]]
    cp = plate_xy([(x - 0.02, y - 0.025) for x, y in cres], -0.003, 0.003, rings=2)
    b.add(place(cp, tilt(rx=-55, rz=0, t=(0.150, 0.0, 0.062))), "herb_d", name="crescent", hull_pad=True)
    for dx in (-0.004, 0.012, 0.028):
        b.add(closed_tube(Vector((0, 0, 0)), 0.023, 0.0068, n=16, sides=6).rotate("y", 90)
              .translate((tie.x + dx, 0.0, tie.z + 0.004)), "twine", name="twine", hull_pad=True)
    # three pale blossoms on the outer sprigs (value contrast on the plains grass)
    for k, (dx, dy, dz) in enumerate(((0.130, -0.046, 0.050), (0.165, 0.040, 0.054), (0.092, 0.012, 0.062))):
        petals = []
        for i in range(10):
            a = TAU * i / 10
            rr = 0.031 if i % 2 == 0 else 0.015          # larger, brighter blossoms (fourth look: barely visible)
            petals.append((rr * math.cos(a), rr * math.sin(a)))
        fl = plate_xy(petals, -0.0025, 0.0025, rings=2)
        m = tilt(rx=-25 + 12 * k, rz=36 * k, t=(dx, dy, dz))
        pid = b.add(place(fl, m), "petal", name="blossom", hull_pad=True)
        b.add(place(IK.ell((0.0095, 0.0095, 0.0045), (0, 0, 0.0028), sides=8, rings=4), m), "pistil",
              name="pistil", outline=False, covered_by=pid)
    obj = asset.finish_mesh(b, pal, CLS)
    thin_hull(obj, HULL_SCALE["SM_Pickup_Quest_Herb"])
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (0.10, 0.0, top_z)), {"kind": "quest", "itemKind": "herb", "questItem": "mat_herb",
                                                     "hullScale": HULL_SCALE["SM_Pickup_Quest_Herb"]}


GEL_LIFT = 0.018      # coherence pass: the blob stands on a tucked base (see build_quest_gel)


def build_quest_gel(pal: Palette):
    """Glossy slime-gel blob with a drip and a lighter core (QuestItemIcons.ts gel).

    Coherence pass: the blob stands on a tucked base (r 5.6 cm at the ground, the 11 cm belly 3.8 cm up,
    ``GEL_LIFT``). Spread flat on the ground (r 9 cm at z 0, widest at 2 cm), the W1 camera's lower contour was the
    ground contact itself — the ink under it lay below the terrain on ~41 % of the silhouette, and the light-green
    body met the green plains grass with no line at all; with the belly lifted, the lower contour is inked like the
    potions'."""
    b = builder("SM_Pickup_Quest_Gel", pal, "quest")
    n = 22
    L = GEL_LIFT
    zs = [0.0, 0.008, 0.016] + [z + L for z in (0.006, 0.020, 0.040, 0.062, 0.084, 0.102, 0.116, 0.126, 0.132)]
    rs = [0.056, 0.080, 0.096] + [0.104, 0.110, 0.106, 0.094, 0.076, 0.054, 0.034, 0.016, 0.004]

    def surf(a, z):
        r = float(np.interp(z, zs, rs)) * (1 + 0.06 * math.sin(3 * a + 0.4) + 0.03 * math.sin(5 * a + 1.1))
        return Vector((r * math.cos(a), 0.92 * r * math.sin(a), z))
    rings = [[surf(TAU * i / n, z) for i in range(n)] for z in zs]
    gid = b.add(IK.loft_rings(rings, cap0="fan", cap1="fan", dome1=0.002), "gel", name="gel", hull_pad=True)
    # lighter jelly core showing on the crown (web: radial white → base) and two specular streaks
    core = [[surf(TAU * i / 12, z) * 0.0 for i in range(12)] for z in (0.0,)]
    del core
    for ph in (2.4, 5.6):
        pts, nrm = [], []
        for i in range(7):
            t = i / 6
            z = 0.055 + 0.055 * t + L
            a = ph + 0.45 * (t - 0.5)
            p = surf(a, z)
            pts.append(p)
            nrm.append(Vector((p.x, p.y, (p.z - 0.02 - L) * 1.6)).normalized())
        b.add(IK.surface_strip(pts, nrm, 0.016, lift=0.0012, depth=0.004), "gel_hi", name="glint", outline=False,
              covered_by=gid)
    cap = []
    for a in [TAU * i / 14 for i in range(14)]:
        cap.append((0.040 * math.cos(a) - 0.012, 0.034 * math.sin(a) + 0.006))
    cp = plate_xy(cap, 0.0, 0.0025, rings=2)
    cp.deform(lambda co: Vector((co.x, co.y, co.z + 0.126 + L - 18.0 * (co.x ** 2 + co.y ** 2) - 0.0005)))
    b.add(cp, "gel_core", name="core", outline=False, covered_by=gid)
    # drip running down the side + a droplet beside the blob
    a0 = -0.6
    pts = [surf(a0, z) * 1.0 + Vector((math.cos(a0), math.sin(a0), 0)) * 0.006 for z in (0.07 + L, 0.05 + L, 0.03 + L,
                                                                                          0.014 + L)]
    b.add(M.tube(pts, [0.008, 0.010, 0.012, 0.013], sides=10, dome0=0.004, dome1=0.010), "gel", name="drip",
          outline=False, covered_by=gid)
    did = b.add(IK.ell((0.018, 0.016, 0.012), (0.128 * math.cos(a0 - 0.25), 0.128 * math.sin(a0 - 0.25), 0.012),
                       sides=10, rings=6), "gel", name="droplet", hull_pad=True)
    del did
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z), {"kind": "quest", "itemKind": "gel", "questItem": "mat_slime_gel"}


def build_quest_pendant(pal: Palette):
    """Heirloom locket (QuestItemIcons.ts pendant): a puffy gold heart with a filigree ring, a hinge line and an
    oval ruby, lying on a loop of its chain (bail at the top lobe)."""
    b = builder("SM_Pickup_Quest_Pendant", pal, "quest")
    s_ = 0.0043
    heart = []
    for i in range(36):
        t = TAU * i / 36
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        heart.append((x * s_, y * s_ + 0.010))
    heart = C.ccw(heart)
    # pillow loft: (z, inset) — a flat back on the ground, a rounded shoulder, a domed face
    # pillow loft: rings scaled toward the heart's centre (insets by offset would cross at the sharp tip and the
    # notch): a flat back on the ground, a rounded shoulder, a domed face
    hc = Vector((sum(x for x, _ in heart) / len(heart), sum(y for _, y in heart) / len(heart), 0.0))
    levels = [(0.0, 0.92), (0.004, 0.985), (0.009, 1.0), (0.0145, 0.965), (0.019, 0.89), (0.0225, 0.76),
              (0.0245, 0.60)]

    def ring_at(k, z, lift=0.0):
        return [Vector((hc.x + (x - hc.x) * k, hc.y + (y - hc.y) * k, z + lift)) for x, y in heart]
    rings = [ring_at(k, z) for z, k in levels]
    lid = b.add(IK.loft_rings(rings, cap0="fan", cap1="fan", dome1=0.002), "locket", name="locket", hull_pad=True)
    zf = float(np.interp(0.82, [lv[1] for lv in reversed(levels[2:])], [lv[0] for lv in reversed(levels[2:])]))
    fil = ring_at(0.82, zf, 0.0008)
    nf = [Vector((0, 0, 1))] * (len(fil) + 1)
    b.add(IK.surface_strip(fil + fil[:1], nf, 0.0034, lift=0.0006, depth=0.002), "filigree", name="filigree",
          outline=False, covered_by=lid)
    hinge = [Vector((x, 0.024, 0.0247 - 2.0 * x * x)) for x in np.linspace(-0.026, 0.026, 7)]
    b.add(IK.surface_strip(hinge, [Vector((0, 0, 1))] * 7, 0.0026, lift=0.0006, depth=0.002), "hinge",
          name="hinge", outline=False, covered_by=lid)
    gem = IK.revolve([(0.0165, 0.0), (0.0158, 0.003), (0.0125, 0.0068), (0.0065, 0.0092), (0.0006, 0.0098)],
                     sides=14, squash_y=1.22).translate((0.0, 0.004, 0.0215))
    b.add(gem, "ruby", name="ruby", hull_pad=True)
    # bail ring at the top lobe (light gold: it read as a dark blob) and the chain: a short run from the bail into a
    # flat coil beside the locket, the cord 1.5 cm wide (art review 6: a 9 mm loop round the heart was swallowed by
    # its 2 px ink and read as a muddy olive ring)
    top = Vector((0.0, 0.010 + 5 * s_ * 1.0 + 0.008, 0.0175))          # the 1.2 cm ring clears the floor
    bail = closed_tube(Vector((0, 0, 0)), 0.0120, 0.0048, n=14, sides=6).rotate("y", 90).translate(tuple(top))
    b.add(bail, "bail", name="bail", hull_pad=True)
    cc = Vector((-0.118, 0.084, 0.0))                     # coil centre, beside the heart's upper-left lobe
    path = [top + Vector((0.0, 0.008, -0.004)), top + Vector((-0.020, 0.024, -0.008))]
    turns, r0, r1 = 2.3, 0.050, 0.011                     # 1.7 cm per turn: the 1.5 cm cord never overlaps
    a0 = math.atan2(path[-1].y - cc.y, path[-1].x - cc.x)
    for i in range(1, 41):
        t = i / 40
        a = a0 + TAU * turns * t
        rr = r0 + (r1 - r0) * t
        path.append(Vector((cc.x + rr * math.cos(a), cc.y + rr * math.sin(a), 0.0)))
    for p in path:
        p.z = max(p.z, 0.0047)
    b.add(M.tube(path, [(0.0042, 0.0075)] * len(path), sides=8, up=(0, 0, 1), dome0=0.003, dome1=0.003), "chain",
          name="chain", hull_pad=True)
    obj = asset.finish_mesh(b, pal, CLS)
    thin_hull(obj, HULL_SCALE["SM_Pickup_Quest_Pendant"])
    tz = top_z(obj)
    return obj, sockets(tz, (0.0, 0.004, tz)), {"kind": "quest", "itemKind": "pendant", "questItem": "mat_pendant",
                                                "hullScale": HULL_SCALE["SM_Pickup_Quest_Pendant"]}


# ── catalogue ───────────────────────────────────────────────────────────────────────────────────────────
def catalogue() -> list[tuple[str, str, callable, list[str]]]:
    """(group, asset, build(pal), game ids)."""
    out = [("bag", "SM_Loot_Bag", build_bag, ["loot_bag"]),
           ("coin", "SM_Pickup_GoldCoin", build_coin_fx, ["fx_coin", "pickup_gold_coin"])]
    for s in ("S", "M", "L"):
        out.append(("gold", f"SM_Pickup_Gold_{s}", lambda p, s=s: build_gold(p, s), [f"gold_pile_{s.lower()}"]))
    for kind, (liq, amounts) in POTION_KINDS.items():
        for s in amounts:
            nm = f"SM_Pickup_Potion{kind}" + ("" if s == "M" else f"_{s}")
            base = f"c_{liq}_potion_{s.lower()}"
            ids = [f"potion_drop_{liq}_{s.lower()}", f"loot_{base}"] + ([f"potion_drop_{liq}"] if s == "M" else [])
            out.append(("potion", nm, lambda p, k=kind, s=s: build_potion(p, k, s), ids))
    out.append(("potion", "SM_Pickup_Antidote", build_antidote, ["loot_c_antidote"]))
    out.append(("scroll", "SM_Pickup_Scroll_TP", lambda p: build_scroll(p, "tp"), ["loot_c_tp_scroll"]))
    out.append(("scroll", "SM_Pickup_Scroll_ID", lambda p: build_scroll(p, "id"), ["loot_c_id_scroll"]))
    out.append(("fruit", "SM_Pickup_LeyFruit", build_fruit, ["loot_c_ley_fruit"]))
    for g, n in GEM_TIERS.items():
        for t in range(1, n + 1):
            out.append(("gem", f"SM_Pickup_Gem_{g}_{t}", lambda p, g=g, t=t: build_gem(p, g, t),
                        [f"loot_g_{g.lower()}_{t}"]))
    out.append(("quest", "SM_Pickup_Quest_Herb", build_quest_herb, ["quest_item_herb"]))
    out.append(("quest", "SM_Pickup_Quest_Gel", build_quest_gel, ["quest_item_gel"]))
    out.append(("quest", "SM_Pickup_Quest_Pendant", build_quest_pendant, ["quest_item_pendant"]))
    return out


QUALITY_HOOKS = {
    "qualityPalette": dict(IK.QUALITY),
    "beam": {   # spec §8.7 NS_Loot_Beam (W1 cm): pool scale and pillar per quality; normal: none
        "magic": {"poolCm": [40, 18], "poolAlpha": 0.7},
        "rare": {"poolCm": [40, 18], "poolAlpha": 0.7, "pillarCm": 180, "pillarWidthCm": 22, "pillarAlpha": 0.55,
                 "coreAlpha": 0.5},
        "legendary": {"poolCm": [55, 24], "poolAlpha": 0.7, "pillarCm": 280, "pillarWidthCm": 30, "pillarAlpha": 0.75,
                      "coreAlpha": 0.7, "motes": 10},
        "set": {"poolCm": [55, 24], "poolAlpha": 0.7, "pillarCm": 280, "pillarWidthCm": 30, "pillarAlpha": 0.75,
                "coreAlpha": 0.7, "motes": 10},
    },
    "bob": {"amplitudeCm": 12.8, "periodMs": 1600},
}
# spec §7: potions bob 5 px / 600 ms yoyo (the bag 800 ms) and carry a glow (HP #FF4A4A / MP #4A8AFF, r 10 px α .45;
# web px → 2.2 cm, the VFX conversion) on the ``glint`` socket at the liquid's centre
POTION_BOB = {"amplitudeCm": 12.8, "periodMs": 1200}
POTION_GLOW = {"hp": "#FF4A4A", "mp": "#4A8AFF"}
# the ley fruit's soft glow (web drawLeyFruit: glow #6FF0C8 α .6, r 40 / 27 of the fruit): additive — it brightens
# the grass round the fruit to a luminous mint (a light, not a colour over the ground like the potions)
FRUIT_GLOW = {"color": "#6FF0C8", "radiusCm": 20.0, "alpha": 0.6, "blend": "additive"}
# translucent 'over' (art review, fourth look: added over the plains grass the red card turned the grass amber, the
# 'rare' colour): red mixed into green stays khaki / orange below ~60 % coverage, so the card's peak opacity is .85
# (web falloff: ~.45 at the flask's silhouette, as the web's α .45 glow under its sprite)
GLOW_RADIUS_CM, GLOW_ALPHA = 22.0, 0.85
GLOW_BLEND = "alpha"
# saturated core (art review, fifth look: the spec halo, α .45 #FF4A4A 'over' the plains grass #74A247, samples
# (174,146,75) — khaki next to the 'rare' gold; the MP halo drifts teal): a second card in a deeper, saturated tone of
# the liquid, α .92 solid to 1.22 × the flask's radius and gone at 1.65 × (computed over grass #74A247 / dirt #C09A60:
# (213,48,39) / (216,46,42) to 1.2 flask radii, red-orange to 1.45, the olive transition only in the last ~3 px), so the
# band hugging the flask reads red / blue at full strength; the spec halo keeps its α at the ring beyond it
POTION_GLOW_CORE = {"hp": "#D81822", "mp": "#1E46E6"}
CORE_ALPHA, CORE_SCALE = 0.92, 1.65
CORE_MIN_CM = 15.0        # the slim S vial (7.2 cm) kept a 6 px khaki ring inside the 22 cm halo at 1.65 × its radius


def core_radius_cm(flask_cm: float) -> float:
    return round(max(CORE_SCALE * flask_cm, CORE_MIN_CM), 1)
CORE_STOPS = ((0.0, 1.0), (0.74, 1.0), (0.90, 0.6), (1.0, 0.0))
LATER = {"tp": "DECISIONS I4: the TP scroll is removed in milestone 1", "id": "DECISIONS I2: no ID scrolls in milestone 1"}


def loot_entry(meta: dict, tint: bool) -> dict:
    e = {"tint": tint, "tintAmount": 0.85 if tint else 0.0, "sockets": {"beam": "ground centre (pool + pillar base)",
                                                                        "label": "name plate anchor",
                                                                        "glint": "sparkle"}}
    e.update(QUALITY_HOOKS)
    if meta.get("kind") == "potion":
        e["bob"] = dict(POTION_BOB)
        if meta.get("potion") in POTION_GLOW:
            e["glow"] = {"color": POTION_GLOW[meta["potion"]], "radiusCm": GLOW_RADIUS_CM, "alpha": GLOW_ALPHA,
                         "socket": "glint", "blend": GLOW_BLEND, "falloff": "web light (review.GLOW_STOPS)",
                         "blendNote": ("alpha = translucent premultiplied 'over' (BLEND_Translucent, emissive = colour × "
                                       "a·f, opacity = a·f): the halo replaces the terrain with the liquid colour; "
                                       "additive turned the plains grass amber"),
                         "placement": "behind: camera-facing card pushed 15 cm back along the view ray, depth-tested "
                                      "(the flask and its ink occlude it; web draws the glow under the sprite)",
                         "depthOffsetCm": 15.0,
                         "core": {"color": POTION_GLOW_CORE[meta["potion"]], "alpha": CORE_ALPHA,
                                  "radiusCm": core_radius_cm(meta.get("flaskRadiusCm", 10.8)),
                                  "falloff": [list(x) for x in CORE_STOPS], "blend": GLOW_BLEND,
                                  "note": ("a second card on the same socket, drawn over the halo (same placement): "
                                           "the saturated band round the bulb keeps the hue red / blue over grass "
                                           "and dirt, where the spec α .45 halo alone mixes to khaki / teal")}}
    if meta.get("kind") == "fruit":
        e["glow"] = dict(FRUIT_GLOW, socket="glint", falloff="web light (review.GLOW_STOPS)",
                         placement="behind: camera-facing card pushed 15 cm back along the view ray, depth-tested",
                         depthOffsetCm=15.0)
    if meta.get("kind") == "scroll":
        e["milestone"] = "later"
        e["milestoneNote"] = LATER[meta["scroll"]]
    e.update({k: v for k, v in meta.items()})
    return e


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
OUT_FAMILY = "loot_family"


def studio_sheet(objs: list, label_fn, cols: int, cell=(240, 240), pitch=-38.0) -> np.ndarray:
    cells = []
    for o in objs:
        with IK.only_visible([o]):
            o.hide_render = False
            img = IK.studio([o], res=cell, pitch=pitch, yaw=shading.CAM_YAW_UE, ink_px=1.8, fill=0.82,
                            label=label_fn(o))
            o.hide_render = True
        cells.append(img)
    return review.grid(cells, cols)


class LootReview(review.Review):
    """``review.Review`` whose ``game_inkgate.png`` shows the **whole** shipped 1080p cell (art review 6: cropping
    to the gated edges cut a flat pickup down to its top edge, the only one above the ground-contact line)."""

    def game_view(self, facing: str = "se") -> list[Path]:
        out = super().game_view(facing)
        st = next(r for r in reversed(self.ink_reports) if r.get("tag", "").endswith("shipped"))
        ship = self._last_ship
        z = review.ink_map(ship, st, zoom=3)
        plain = ship.repeat(3, 0).repeat(3, 1)
        review.draw_text(plain, 4, 4, "SHIPPED 1080P  3X NEAREST")
        review.draw_text(z, 4, 4, f"INK GATE  GREEN >= {st['minPx']:g}PX  RED THINNER", rgb=(255, 255, 255))
        review.draw_text(z, 4, 14, self.ink_label(st), rgb=(255, 220, 140))
        review.draw_text(z, 4, 24, f"{st['edges']} EDGES GATED  {st.get('ignoredEdges', 0)} GROUND-CONTACT "
                                   f"(UNMARKED)", rgb=(200, 220, 255))
        self._save("game_inkgate.png", review.grid([plain, z], 2, pad=2))
        return out

    def post(self, img, cam, res, border=None, glow: bool = True, bloom_on=None):
        """Glow cards sit **behind** the pickup (manifest ``loot.glow.placement``: a depth-tested card pushed back
        along the view, as the web draws the glow under the flask): composited only where neither the body nor its
        ink covers the pixel, so the flask's ink stays dark (art review 5: the glow was missing from every cell)."""
        if not (glow and self.glows is not None):
            return super().post(img, cam, res, border, glow, bloom_on)
        base = super().post(img, cam, res, border, glow=False, bloom_on=bloom_on)
        lit = base
        x0, y0 = review.border_origin(border, res)
        for g in self.glows(cam, res):
            c = (g["center_px"][0] - x0, g["center_px"][1] - y0)
            if g.get("blend") == "alpha":
                lit = glow_over(lit, c, g["radius_px"], g["color"], g.get("alpha", 0.0) * self.glow_scale,
                                g.get("stops"))
            else:
                lit = review.glow_card(lit, c, g["radius_px"], g["color"], g.get("alpha", 0.0) * self.glow_scale)
        body, hull = review.coverage_pass(self.meshes, border, ground=False)
        A = np.clip(body + hull, 0.0, 1.0)[..., None]
        return (base.astype(np.float32) * A + lit.astype(np.float32) * (1.0 - A) + 0.5).astype(np.uint8)

    def game_shot(self, border=None, res=(1920, 1080), measure=False, tag="", glow=True, bloom_on=None):
        img, st = super().game_shot(border, res, measure, tag, glow, bloom_on)
        if tag.endswith("shipped"):
            self._last_ship = img.copy()
        return img, st


def glow_over(img: np.ndarray, center_px, radius_px: float, hex_color: str, alpha: float, stops=None) -> np.ndarray:
    """The potion glow card **alpha-blended** (premultiplied 'over', UE translucent): lin = lin·(1 − a·f) + a·f·c with
    the web light falloff f (``review.GLOW_STOPS``). Art review, fourth look: added over the plains grass the red
    glow (#FF4A4A α .45) shifted the grass (116,162,71) → (142,160,75) — amber, the 'rare' colour; blended over, the
    halo replaces the grass with the liquid's colour and reads red / blue."""
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot(xx + 0.5 - center_px[0], yy + 0.5 - center_px[1]) / max(radius_px, 1e-3)
    xs, ys = zip(*(stops or review.GLOW_STOPS))
    f = (alpha * np.interp(r, xs, ys)).astype(np.float32)[:, :, None]
    lin = IK._lin(img)
    lin = lin * (1.0 - f) + f * np.array(color.hex_linear(hex_color), np.float32)
    return IK._srgb(lin).astype(np.uint8)


def potion_glows(obj, meta: dict | None):
    """The runtime potion glow card (manifest ``loot.glow``) for the game cells: centred on the ``glint`` socket,
    radius 22 cm projected at the card's depth."""
    if meta and meta.get("kind") == "fruit":
        hx, gz, rad, al, bl = FRUIT_GLOW["color"], meta.get("glintZ", 0.12), FRUIT_GLOW["radiusCm"], \
            FRUIT_GLOW["alpha"], FRUIT_GLOW["blend"]
    elif not meta or meta.get("potion") not in POTION_GLOW:
        return None
    else:
        hx, gz, rad, al, bl = POTION_GLOW[meta["potion"]], meta.get("glintZ", 0.15), GLOW_RADIUS_CM, GLOW_ALPHA, \
            GLOW_BLEND

    core = None
    if meta.get("kind") == "potion" and meta.get("potion") in POTION_GLOW_CORE:
        core = (POTION_GLOW_CORE[meta["potion"]], core_radius_cm(meta.get("flaskRadiusCm", 10.8)))

    def glows(cam, res):
        c = obj.matrix_world @ Vector((0.0, 0.0, gz))
        right = cam.matrix_world.to_3x3() @ Vector((1.0, 0.0, 0.0))
        p0 = review.project_px(cam, c, res)
        px_cm = abs(review.project_px(cam, c + right * 0.01, res)[0] - p0[0])
        out = [dict(center_px=p0, radius_px=px_cm * rad, color=hx, alpha=al, blend=bl)]
        if core:            # the saturated band round the bulb, over the halo (fifth look)
            out.append(dict(center_px=p0, radius_px=px_cm * core[1], color=core[0], alpha=CORE_ALPHA, blend=bl,
                            stops=CORE_STOPS))
        return out
    return glows


def review_asset(obj, name: str, out_dir: Path | None = None, meta: dict | None = None) -> tuple[list[Path], dict]:
    """The standard static review at the shipped ink (game.png, game_1080crop.png, game_inkgate.png, closeup.png),
    measured by the interactive ink gate (the floor occludes the hull under the contact, as the terrain does);
    potions show their runtime glow card in every game cell."""
    obj.hide_render = False
    top = top_z(obj)
    rv = LootReview(name, obj, [obj], max(top, 0.05), blob_radius=max(0.08, footprint_r(obj) * 1.15),
                    game_outline_px=outline.SCREEN_PX_1080[CLS], outline_class=CLS, out_dir=out_dir,
                    glows=potion_glows(obj, meta))
    rv.ground_occludes = True
    files = rv.game_view("front")
    files.append(yaw_cells(rv))
    files.append(closeup(rv, obj, name))
    obj.hide_render = True
    return files, rv.ink_verdict()


YAWS = ("se", "side", "back", "sw")


def yaw_cells(rv: review.Review) -> Path:
    """``game_yaws.png``: four more yaws at W1 1080p 1:1 with the shipped ink + bloom, each **ink-gated** — the
    world spawns pickups at a hashed yaw, so the ink must hold from every side (``game.png`` is the 'front')."""
    cells = []
    for f in YAWS:
        rv.facing(f)
        review.setup_render((1920, 1080), rv.samples)
        cam = review.game_camera()
        box = rv.game_crop_box(cam, pad=0.02)
        rv.game_outline(1080)
        img, st = rv.game_shot(box, measure=True, tag=f"yaw {f}")
        cells.append((f, img, st))
    h = max(c[1].shape[0] for c in cells)
    w = max(c[1].shape[1] for c in cells)
    h, w = max(h, 110), max(w, 150)
    out = []
    for f, img, st in cells:
        c = IK.pad_to(img, h, w)
        review.draw_text(c, 3, 3, f"YAW {f.upper()}")
        review.draw_text(c, 3, 12, f"RIM {st['fracRim'] * 100:.0f}% D {st['fracDark'] * 100:.0f}%", rgb=(255, 220, 140))
        out.append(c)
    rv.facing("front")
    return rv._save("game_yaws.png", review.grid(out, len(out), pad=2))


def closeup(rv: review.Review, obj, name: str, res=(420, 380), pitch: float = -38.0) -> Path:
    """Front 3/4 close-up (W1 yaw, steeper than a character close-up: pickups are seen from above) with the ink
    in **screen pixels** (3 px at this size ≈ the 2 px game ink on a 1080p crop magnified ~1.5×): the kit's
    world-unit close-up ink (1.7 cm) is a third of a coin's width and hides small props."""
    review.setup_render(res, 8)
    pts = review.mesh_points([obj])
    review.frame_points(pts, res, fov_h=28.0, pitch_ue=pitch, fill=0.86)
    review.set_outline_px([obj], 3.0, Vector(pts.mean(0)))
    img = review.render_array()
    review.draw_text(img, 6, 6, f"{name}  FRONT 3/4  INK 3PX")
    return rv._save("closeup.png", img)


def coin_previews(obj, name: str) -> list[Path]:
    """``SM_Pickup_GoldCoin`` is the ``reward.gold`` burst mesh, so its game views show a burst: six coins in the
    runtime look (``M_AF_FX_Mesh`` tinted #FFC93A × 1.1, ``vfx/fxkit``), at the recipe's size (×1.7, ~22 cm) and
    spray height (0.3–1.0 m) over a goblin target beside the warrior — ``game.png`` (the 640×360 W1 frame) and
    ``game_1080crop.png`` (native 1080p pixels); ``closeup.png`` the mesh alone."""
    sys.path.insert(0, str(HERE.parent / "vfx"))
    import fxkit as K
    out = paths.preview_dir(name)
    files = []
    with IK.only_visible([obj]):
        obj.hide_render = False
        img = IK.studio([obj], res=(300, 260), pitch=-38.0, ink_px=0.0, label=f"{name}  (VFX MESH, NO HULL)")
        obj.hide_render = True
    files.append(IK.save(out, "closeup.png", img))
    right = Vector((-1.0, -1.0, 0.0)).normalized()
    into = Vector((1.0, -1.0, 0.0)).normalized()
    hero_at, tgt = Vector((0.0, 0.0, 0.0)), right * 1.6 + into * 0.12
    blow = (tgt - hero_at).normalized()
    yaw = math.degrees(math.atan2(blow.x, -blow.y))
    w = K.import_warrior("Idle", 0, at=tuple(hero_at), yaw_deg=yaw)
    g = K.import_goblin("Idle", 0, at=tuple(tgt), yaw_deg=yaw + 180.0)
    chars = (w[1] if w else []) + (g[1] if g else [])
    for m in chars:
        review.set_outline_px([m], outline.SCREEN_PX_1080[m.get("af_outline_class", "monster")])
    toon_mats = list(obj.data.materials)
    K.set_fx_look(obj, K.tint_linear("#FFC93A", 1.1), 1.0)
    rng = scene.rng(name, "burst")
    coins = []
    cam_q = Vector((0.4545, -0.4545, -0.7660)).to_track_quat("-Z", "Y")     # disc normal toward the W1 camera
    for i in range(6):
        c = obj.copy()
        scene.link(c)
        a = TAU * i / 6 + rng.uniform(-0.3, 0.3)
        c.location = tgt + Vector((0.42 * math.cos(a), 0.42 * math.sin(a), 0.0)) * rng.uniform(0.6, 1.1) + \
            Vector((0.0, 0.0, rng.uniform(0.40, 1.10)))
        tumble = (Matrix.Rotation(math.radians(rng.uniform(-55, 55)), 4, "X")
                  @ Matrix.Rotation(math.radians(rng.uniform(-55, 55)), 4, "Y"))
        c.rotation_euler = (cam_q.to_matrix().to_4x4() @ tumble).to_euler()
        c.scale = (1.7, 1.7, 1.7)
        c.hide_render = False
        coins.append(c)
    bpy.context.view_layer.update()
    keep = chars + coins
    with IK.only_visible(keep):
        review.stage(radius=40.0, blob_radius=0.0)
        review.hide_stage(False)
        mid = (hero_at + tgt) * 0.5
        review.setup_render(review.GAME_RES, 8)
        cam = review.game_camera((mid.x, mid.y))
        for m in chars:
            review.set_outline_px([m], outline.SCREEN_PX_1080[m.get("af_outline_class", "monster")] * 360 / 1080)
        img = review.render_array()
        review.draw_text(img, 6, 6, f"{name}  REWARD.GOLD BURST  GAME W1 640X360")
        files.append(IK.save(out, "game.png", img))
        review.setup_render((1920, 1080), 8)
        cam = review.game_camera((mid.x, mid.y))
        for m in chars:
            review.set_outline_px([m], outline.SCREEN_PX_1080[m.get("af_outline_class", "monster")])
        cpt = review.project(cam, tgt + Vector((0.0, 0.0, 0.6)))
        hw, hh = 560 / 1920 / 2, 380 / 1080 / 2
        box = (cpt[0] - hw, cpt[1] - hh, cpt[0] + hw, cpt[1] + hh)
        img = review.render_array(box)
        img = review.bloom(img, review.emissive_pass(coins, box), 1080)
        review.draw_text(img, 4, 4, "REWARD.GOLD BURST  W1 1080P 1:1  6 COINS x1.7  TINT #FFC93A")
        files.append(IK.save(out, "game_1080crop.png", img))
    for c in coins:
        bpy.data.objects.remove(c)
    # back to the shipped slot-0 toon material: the family studio sheet renders the mesh after this, and the FX look
    # (lit by the game stage) drew a solid black ellipse in the studio (art review, fourth look)
    for i, mt in enumerate(toon_mats):
        obj.data.materials[i] = mt
    for m in chars:
        m.hide_render = True
    for root in (w, g):
        if root:
            for o in [root[0], root[2]] + list(root[1]):
                if o and o.name in bpy.data.objects:
                    bpy.data.objects.remove(o)
    return files


def px_border(x0: float, y0: float, x1: float, y1: float, res=(1920, 1080)) -> tuple:
    """Pixel box (x right, y down) → Blender render border (normalised, y up), clamped to the frame."""
    W, H = res
    return (max(0.0, x0 / W), max(0.0, 1.0 - y1 / H), min(1.0, x1 / W), min(1.0, 1.0 - y0 / H))


def quality_bag(bag) -> np.ndarray:
    """The loot bag in the five qualities at W1 1080p 1:1: tint (×0.85 on the toon pixels), light pool, beam."""
    cells = []
    bag.hide_render = False
    review.stage(blob_radius=0.24)
    with IK.only_visible([bag]):
        for q in ("normal", "magic", "rare", "legendary", "set"):
            review.hide_stage(False)
            review.setup_render((1920, 1080), 8)
            cam = review.game_camera()
            review.set_outline_px([bag], outline.SCREEN_PX_1080[CLS], Vector((0, 0, 0.2)))
            p0 = review.project_px(cam, Vector((0, 0, 0)), (1920, 1080))
            box = px_border(p0[0] - 110, p0[1] - 290, p0[0] + 110, p0[1] + 50)
            img = review.render_array(box)
            body, hull = review.coverage_pass([bag], box)
            img = review.bloom(img, review.emissive_pass([bag], box), 1080)
            x0, y0 = review.border_origin(box, (1920, 1080))
            cx, cy = p0[0] - x0, p0[1] - y0
            hx = IK.QUALITY[q]
            if q != "normal":
                img = IK.tint_body(img, body, hx, 0.85)
                big = q in ("legendary", "set")
                sc = QUALITY_HOOKS["beam"][q]
                # the pool is a ground disc: project its radius (cm) at the ground
                rp = review.project_px(cam, Vector((sc["poolCm"][0] / 100.0, 0, 0)), (1920, 1080))
                rx = abs(rp[0] - p0[0]) * 1.4
                img = IK.add_glow(img, cx, cy, rx, rx * 0.64, hx, sc["poolAlpha"] * 0.55)
                # magic+: a soft quality halo round the silhouette (web: 6 px outline glow)
                halo = review.blur((body + hull).clip(0, 1).astype(np.float32), 4.0)
                lin = IK._lin(img) + IK._lin(color.hex_to_rgb(hx))[None, None] * (0.55 * halo * (1 - (body + hull)
                                                                                                .clip(0, 1)))[..., None]
                img = IK._srgb(lin).astype(np.uint8)
                if "pillarCm" in sc:
                    top = review.project_px(cam, Vector((0, 0, sc["pillarCm"] / 100.0)), (1920, 1080))
                    wpx = abs(review.project_px(cam, Vector((sc["pillarWidthCm"] / 100.0, 0, 0)), (1920, 1080))[0]
                              - p0[0]) * 1.4
                    img = IK.add_beam(img, cx, cy, top[1] - y0, wpx, hx, sc["pillarAlpha"], sc["coreAlpha"])
            review.draw_text(img, 4, 4, q.upper(), rgb=color.hex_to_rgb(hx) if q != "normal" else (230, 230, 230))
            cells.append(img)
    bag.hide_render = True
    return review.grid(cells, 5, pad=2)


def lineup(objs: dict) -> np.ndarray:
    """Every pickup laid round the warrior (re-imported, READY) at W1 1080p 1:1 with the shipped ink + bloom."""
    arm, body = IK.import_hero("Warrior")
    arm.rotation_euler = (0, 0, math.radians(review.FACINGS["se"]))
    order = [n for n in objs]
    keep = [body]
    # rows on the ground in front of the hero (toward the camera), laid across the screen: big pickups, potions /
    # scrolls / quest items, then the gems in a tight row
    fwd = Vector((1.0, -1.0, 0.0)).normalized()             # W1 camera forward on the ground (Blender axes)
    right = Vector((-1.0, -1.0, 0.0)).normalized()
    rows = [[n for n in order if objs[n][4] in ("bag", "gold")],
            [n for n in order if objs[n][4] in ("potion", "scroll", "fruit", "quest")],
            [n for n in order if objs[n][4] == "gem"]]
    for ri, (row, dist, step) in enumerate(zip(rows, (0.75, 1.35, 1.95), (0.46, 0.40, 0.31))):
        for i, n in enumerate(row):
            o = objs[n][0]
            off = (i - (len(row) - 1) / 2) * step
            o.matrix_world = Matrix.Translation(-fwd * dist + right * off)
            o.hide_render = False
            keep.append(o)
    bpy.context.view_layer.update()
    with IK.only_visible(keep):
        review.hide_stage(False)
        review.setup_render((1920, 1080), 8)
        review.stage(blob_radius=0.36)
        cam = review.game_camera((-0.6, 0.6))
        review.set_outline_px([o for o in keep if o is not body], outline.SCREEN_PX_1080[CLS], Vector((0, 0, 0.2)))
        review.set_outline_px([body], outline.SCREEN_PX_1080["hero"], Vector((0, 0, 0.9)))
        pts = review.mesh_points(keep, step=4)
        xy = np.array([review.project_px(cam, Vector(p), (1920, 1080)) for p in pts[::7]])
        x0, x1 = xy[:, 0].min() - 40, xy[:, 0].max() + 40
        y0, y1 = xy[:, 1].min() - 40, xy[:, 1].max() + 40
        box = px_border(x0, y0, x1, y1)
        img = review.render_array(box)
        img = review.bloom(img, review.emissive_pass(keep, box), 1080)
    review.draw_text(img, 4, 4, "PICKUPS ROUND SK_HERO_WARRIOR  W1 1080P 1:1  SHIPPED INK + BLOOM")
    for n in order:
        objs[n][0].hide_render = True
        objs[n][0].matrix_world = Matrix.Identity(4)
    IK.remove_hero(arm)
    return img


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def build_all(groups: list[str] | None):
    scene.reset()
    pal = palette()
    pal.save()
    objs = {}
    for grp, name, fn, ids in catalogue():
        if groups and grp not in groups:
            continue
        obj, socks, meta = fn(pal)
        obj.name = name
        obj.hide_render = True
        low = lowest_z(obj)
        if abs(low) > 0.0015 and grp != "coin":
            print(f"[loot] WARNING {name}: lowest point {low * 100:.2f} cm (pivot should be on the ground)")
        objs[name] = (obj, socks, meta, ids, grp)
        print(f"[loot] {name}: toon {M.tri_count(obj, 0)} hull {M.tri_count(obj, 1)}")
    return pal, objs


def ship(groups: list[str] | None, previews: bool = True, verify: bool = True) -> int:
    t0 = time.time()
    pal, objs = build_all(groups)
    fails = []
    for name, (obj, socks, meta, ids, grp) in objs.items():
        cls = "none" if grp == "coin" else CLS
        fbx = export.export_static_mesh(obj, name, CATEGORY, socks)
        man = export.Manifest()
        man.set_palette(pal)
        extra = {"loot": loot_entry(meta, tint=(grp == "bag")), "pickupGroup": grp}
        if grp == "coin":
            # a burst VFX mesh, not a ground pickup: no loot record (beam / label / glint sockets, bob) — art review 8
            extra = {"pickupGroup": grp,
                     "note": ("VFX mesh (reward.gold burst, AbyssVfxDefaults): toon slot only, no outline (spec §1.3 "
                              "VFX), no sockets; the VFX system replaces slot 0 with M_AF_FX_Mesh and sizes it by its "
                              "bounds; not a ground pickup (no loot record)")}
        e = export.static_entry(obj, fbx, CATEGORY, pal, cls, ids, socks, footprint_tiles=[1, 1], blocking=False,
                                extra=extra)
        if cls in outline.INK_MIX and len(e["materialSlots"]) > 1:
            e["materialSlots"][1]["outlineMix"] = outline.INK_MIX[cls]
        if previews:
            out = paths.preview_dir(name)
            if grp == "coin":
                files = coin_previews(obj, name)
                v = None
            else:
                files, v = review_asset(obj, name, meta=meta)
            e["previews"] = [str(p.relative_to(paths.preview_root())) for p in files]
            if v is not None:
                e["inkGate"] = IK.ink_entry(v)
                if name in INK_EXCEPTION:
                    e["inkGate"]["exception"] = INK_EXCEPTION[name]
                ok = v["pass"]
                print(f"[ink] {name}: {v['cells']} cells, min rim {v['minFracRim'] * 100:.1f} %, dark "
                      f"{v['minFracDark'] * 100:.1f} %, median {v['medianRimPx']} px -> {'PASS' if ok else 'FAIL'}")
                if not ok:
                    fails.append(f"ink {name}: {v['failures']}")
        man.set_asset(name, e)
        man.save()
    if previews and groups is None:          # family sheets need every pickup (a subset ship keeps them)
        fam = paths.preview_root() / OUT_FAMILY
        if "SM_Loot_Bag" in objs:
            IK.save(fam, "quality_bag.png", quality_bag(objs["SM_Loot_Bag"][0]))
        for sheet, grps in (("studio_bag_gold.png", ("bag", "coin", "gold")), ("studio_potions.png", ("potion",)),
                            ("studio_scroll_fruit_quest.png", ("scroll", "fruit", "quest"))):
            sel = [o[0] for n, o in objs.items() if o[4] in grps]
            if sel:
                IK.save(fam, sheet, studio_sheet(sel, lambda o: o.name.replace("SM_", ""), len(sel)))
        gems = [o[0] for n, o in objs.items() if o[4] == "gem"]
        if gems:
            IK.save(fam, "gems.png", studio_sheet(gems, lambda o: o.name.replace("SM_Pickup_Gem_", ""), 6,
                                                  cell=(170, 170), pitch=-42.0))
        IK.save(fam, "game_lineup.png", lineup({n: o for n, o in objs.items() if o[4] != "coin"}))
    if verify:
        for name, (obj, socks, meta, ids, grp) in objs.items():
            fails += IK.verify_fbx(name, CATEGORY, sum(p.loop_total - 2 for p in obj.data.polygons),
                                   [s["name"] for s in socks], FAMILY) if grp != "coin" else \
                _verify_coin(name, obj)
        print("[verify] " + ("OK" if not fails else "\n  ".join(["FAIL"] + fails)))
    print(f"[loot] {len(objs)} assets in {time.time() - t0:.0f}s")
    return 1 if fails else 0


def _verify_coin(name, obj) -> list[str]:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(paths.category_dir(CATEGORY) / f"{name}.fbx"))
    new = [o for o in bpy.data.objects if o not in before]
    fails = []
    me = [o for o in new if o.type == "MESH"]
    if len(me) != 1 or len(me[0].data.materials) != 1:
        fails.append(f"{name}: expected one mesh with one slot")
    for o in new:
        bpy.data.objects.remove(o)
    return fails


def quick(groups: list[str] | None) -> None:
    pal, objs = build_all(groups)
    out = IK.SCRATCH / "loot"
    sheet = [o[0] for n, o in objs.items() if o[4] != "gem"]
    if sheet:
        IK.save(out, "studio.png", studio_sheet(sheet, lambda o: o.name.replace("SM_", ""), 6))
    gems = [o[0] for n, o in objs.items() if o[4] == "gem"]
    if gems:
        IK.save(out, "gems.png", studio_sheet(gems, lambda o: o.name.replace("SM_Pickup_Gem_", ""), 6,
                                              cell=(170, 170), pitch=-42.0))
    if "SM_Loot_Bag" in objs:
        IK.save(out, "quality_bag.png", quality_bag(objs["SM_Loot_Bag"][0]))
    cells = []
    for name, o in objs.items():
        if o[4] in ("coin",):
            continue
        files, v = review_asset(o[0], name, out_dir=out / name, meta=o[2])
        print(f"[quick] {name} ink {'PASS' if v['pass'] else 'FAIL'} rim {v['minFracRim']:.3f} dark "
              f"{v['minFracDark']:.3f}")
    lu = {n: o for n, o in objs.items() if o[4] != "coin"}
    if lu:
        IK.save(out, "game_lineup.png", lineup(lu))
    del cells


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--quick", nargs="?", const="all", default=None)
    ap.add_argument("--no-previews", action="store_true")
    ap.add_argument("--no-verify", action="store_true")
    a = ap.parse_args(argv)
    if a.quick is not None:
        quick(None if a.quick == "all" else [g for g in a.quick.split(",") if g])
        return 0
    groups = [g for g in a.only.split(",") if g] or None
    return ship(groups, previews=not a.no_previews, verify=not a.no_verify)


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(argv))
