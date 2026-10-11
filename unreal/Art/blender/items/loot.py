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
    "coin": dict(rim=COIN_RIM, face=COIN_FACE, heap=R("#E9AE36", .36, .45)),
    "potion": dict(glass=GLASS, glint=GLINT, cork=CORK, cork_line=R("#7A4E26", .35, .2), collar=GOLD,
                   hp=R("#E0283A", .40, .45, e=0.22), hp_hi=R("#FF8A92", .30, .40, e=0.35),
                   mp=R("#2A5AE8", .40, .45, e=0.22), mp_hi=R("#8AB0FF", .30, .40, e=0.35),
                   anti=R("#5FD03A", .40, .45, e=0.22), anti_hi=R("#B8F49A", .30, .40, e=0.35),
                   label=R("#EEE2C0", .35, .40), leafmark=R("#3A9A3A", .35, .25)),
    "scroll": dict(sheet=R("#F0DCAA", .38, .35), roll=R("#E2C890", .38, .40), knob=R("#C8A86A", .42, .35),
                   ink=R("#C8A86A", .30, .2).shade_region(.25, .1),
                   tp=R("#3E8CFF", .30, .30, e=0.55), tp_seal=R("#2F5AC0", .40, .40), tp_ribbon=R("#3E8CFF", .40, .35),
                   id=R("#D8A030", .35, .35), id_seal=R("#C0282E", .40, .40), id_ribbon=R("#D8A030", .40, .35)),
    "fruit": dict(skin=R("#3FD0B0", .40, .45, e=0.05), vein=R("#C8FFF0", 0.0, 0.0, e=0.6),
                  vein2=R("#9FF6E0", 0.0, 0.0, e=0.45), stem=R("#6B4428", .42, .30), leaf=R("#3FAE5C", .42, .40),
                  rib=R("#7FD88A", .30, .30)),
    "quest": dict(herb_a=R("#5AA83A"), herb_b=R("#6CBF42"), herb_c=R("#4F9A34"), herb_d=R("#7CCC4C"),
                  herb_e=R("#62B43E"), stem=R("#4A7A28"), twine=R("#C89A5A", .40, .35),
                  gel=R("#6EE04A", .38, .50, e=0.10), gel_hi=R("#FFFFFF", 0.0, 0.0, e=0.6),
                  gel_core=R("#B6F49A", .30, .45, e=0.25),
                  locket=R("#EEC05A", .40, .36), filigree=R("#FFE7A0", .30, .40), hinge=R("#9A6A1A", .35, .2),
                  ruby=R("#E0283C", .35, .45, e=0.40), chain=R("#C99A36", .40, .40)),
}
GEMS = {   # web ItemIcons.ts gem colours and cuts (gemSpec)
    "Ruby": ("#E0283C", "oval"), "Sapphire": ("#2F6CF0", "pear"), "Emerald": ("#22C060", "emerald"),
    "Topaz": ("#FFC02A", "trillion"), "Diamond": ("#D8F4FF", "round"),
}
GEM_TIERS = {"Ruby": 3, "Sapphire": 3, "Emerald": 3, "Topaz": 3, "Diamond": 5}
GEM_R = {1: 0.044, 2: 0.053, 3: 0.060, 4: 0.064, 5: 0.068}


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
        o = 0.7 if dia else 1.0
        out[f"{k}_a"] = Region(hexm((150, 190, 225), .25) if dia else hx, .55, .22, o=o)
        out[f"{k}_b"] = Region(hexm((255, 255, 255), .18), .50, .30, e=0.05, o=o)
        out[f"{k}_table"] = Region(hexm((255, 255, 255), .55), .35, .40, e=0.10, o=o)
        out[f"{k}_glow"] = Region(hexm((255, 255, 255), .62), .25, .40, e=0.22, o=o)
        out[f"{k}_deep"] = Region(hexm((40, 80, 140), .50) if dia else hexm((16, 8, 32), .45), .50, .20, o=o)
    return out


REGIONS["gem"] = gem_regions()


def class_regions(prefix: str) -> dict:
    """The prefix's regions with the class outline mix (line tone); gem regions keep their own ``o``."""
    regs = REGIONS[prefix]
    return dict(regs) if prefix == "gem" else outline.ink_regions(regs, CLS)


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
        # a heap: a lumpy gold mound under 22 coins lying on its surface at random tilts, + a short stack beside
        hr, hh = 0.150, 0.075

        def hz(x, y):
            d = math.hypot(x / hr, y / (hr * 0.86))
            return hh * max(0.0, 1.0 - d * d) ** 0.85

        rings = []
        for k, f in enumerate((1.0, 0.86, 0.68, 0.48, 0.26, 0.08)):
            pts = []
            for i in range(22):
                a = TAU * i / 22
                wob = 1.0 + 0.07 * math.sin(4 * a + 1.3) + 0.04 * math.sin(7 * a)
                x, y = hr * f * wob * math.cos(a), hr * 0.86 * f * wob * math.sin(a)
                pts.append(Vector((x, y, hz(x / wob, y / wob) if k else 0.0)))
            rings.append(pts)
        mound = IK.loft_rings(rings, cap0="fan", cap1="fan", dome1=0.004)
        mid = b.add(mound, "heap", name="mound", hull_pad=True)
        placed = 0
        tries = 0
        while placed < 22 and tries < 400:
            tries += 1
            a, d = rng.uniform(0, TAU), math.sqrt(rng.uniform(0.0, 0.92))
            x, y = hr * d * math.cos(a), hr * 0.86 * d * math.sin(a)
            z = hz(x, y)
            gx = (hz(x + 0.004, y) - hz(x - 0.004, y)) / 0.008
            gy = (hz(x, y + 0.004) - hz(x, y - 0.004)) / 0.008
            nrm = Vector((-gx, -gy, 1.0)).normalized()
            rot = nrm.to_track_quat("Z", "Y").to_matrix().to_4x4() @ Matrix.Rotation(rng.uniform(0, TAU), 4, "Z")
            jitter = Matrix.Rotation(math.radians(rng.uniform(-9, 9)), 4, "X")
            add_coin(b, Matrix.Translation(Vector((x, y, z - h * 0.05))) @ rot @ jitter, cover=mid, sides=10)
            placed += 1
        z = h / 2
        for k in range(4):
            add_coin(b, tilt(rz=rng.uniform(0, 360), t=(0.160 + rng.uniform(-0.002, 0.002), -0.060, z)))
            z += h * 0.98
        add_coin(b, tilt(rx=80, rz=40, t=(-0.150, -0.075, r)))
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    lo = GOLD_PILES[size]["amount"]
    return obj, sockets(top_z), {"kind": "gold", "goldAmount": {"min": lo[0], "max": lo[1]}}


# ── potions ─────────────────────────────────────────────────────────────────────────────────────────────
POTIONS = {   # web potionSpec sizes (s slim vial · m round · l round + gold collar); amounts from groundPotions
    "S": dict(prof=[(0.040, 0.0), (0.060, 0.012), (0.072, 0.045), (0.071, 0.088), (0.060, 0.128), (0.042, 0.160),
                    (0.028, 0.180), (0.026, 0.232)], level=0.112, neck=0.026, lip=0.232, cork=0.050),
    "M": dict(prof=[(0.056, 0.0), (0.086, 0.012), (0.104, 0.050), (0.108, 0.104), (0.100, 0.150), (0.080, 0.190),
                    (0.046, 0.214), (0.035, 0.226), (0.033, 0.268)], level=0.150, neck=0.033, lip=0.268, cork=0.052),
    "L": dict(prof=[(0.070, 0.0), (0.105, 0.014), (0.127, 0.060), (0.134, 0.124), (0.124, 0.188), (0.098, 0.236),
                    (0.056, 0.264), (0.043, 0.278), (0.041, 0.336)], level=0.184, neck=0.041, lip=0.336, cork=0.058,
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


def glass_glints(b: M.Builder, prof, cover: int, phases=(2.35, 5.5)):
    """Painted specular streaks on the glass (web: white streak + dot): one on each side so every yaw shows one."""
    zs = [p[1] for p in prof]
    zlo, zhi = zs[0] + (zs[-1] - zs[0]) * 0.18, zs[-1] - (zs[-1] - zs[0]) * 0.30

    def rad(z):
        return float(np.interp(z, zs, [p[0] for p in prof]))
    for ph in phases:
        pts, nrm = [], []
        for i in range(8):
            t = i / 7
            z = zlo + (zhi - zlo) * t
            a = ph + 0.35 * math.sin(math.pi * t)
            r_ = rad(z)
            pts.append(Vector((r_ * math.cos(a), r_ * math.sin(a), z)))
            nrm.append(Vector((math.cos(a), math.sin(a), 0.25 * (0.5 - t))).normalized())
        b.add(IK.surface_strip(pts, nrm, 0.0085, lift=0.0010, depth=0.003), "glint", name="glint", outline=False,
              covered_by=cover)
        z = zlo + (zhi - zlo) * 0.05
        a = ph + 0.55
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
    b.add(M.lathe(hi, sides=20, cap0=None, cap1="fan"), "glass", name="glass", outline=False, covered_by=lid)
    b.add(M.lathe([(rl - 0.002, g["level"] - 0.0035), (rl + 0.0018, g["level"] - 0.0030),
                   (rl + 0.0018, g["level"] + 0.0030), (rl - 0.002, g["level"] + 0.0035)], sides=20, cap0="fan",
                  cap1="fan"), f"{liq}_hi", name="meniscus", outline=False, covered_by=lid)
    glass_glints(b, g["prof"], lid)
    bottle_top(b, g["neck"], g["lip"], g["cork"], lid, g.get("collar"))
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (0.0, 0.0, g["level"])), {"kind": "potion", "potion": liq,
                                                        "potionAmount": amounts.get(size), "size": size}


def build_antidote(pal: Palette):
    b = builder("SM_Pickup_Antidote", pal, "potion")
    rings = [(0.0, 0.074, 0.060), (0.012, 0.086, 0.071), (0.160, 0.088, 0.073), (0.186, 0.070, 0.057),
             (0.206, 0.038, 0.036), (0.214, 0.032, 0.032), (0.256, 0.031, 0.031)]
    level = 0.118
    full = M.loft_z(rings, sides=20, exp=3.6)
    lo = [r for r in rings if r[0] < level] + [(level, 0.088, 0.073)]
    hi = [(level, 0.088, 0.073)] + [r for r in rings if r[0] > level]
    lid = b.add(M.loft_z(lo, sides=20, exp=3.6, cap1="ngon"), "anti", name="liquid", hull=full, hull_pad=True)
    b.add(M.loft_z(hi, sides=20, exp=3.6, cap0=None), "glass", name="glass", outline=False, covered_by=lid)
    b.add(M.loft_z([(level - 0.0035, 0.0885, 0.0735), (level + 0.0035, 0.0885, 0.0735)], sides=20, exp=3.6),
          "anti_hi", name="meniscus", outline=False, covered_by=lid)
    # paper label on both broad faces with the web's leaf glyph
    for sg in (1, -1):
        lab = M.box((0.100, 0.006, 0.072), (0.0, sg * 0.0745, 0.088), bevel=0.003)
        b.add(lab, "label", name="label", outline=False, covered_by=lid)
        leaf = lens_leaf(0.046, 0.026, 0.004).translate((-0.023, 0.0, 0.0)).rotate("z", 0.0)
        leaf.rotate("y", -55).rotate("x", 90 * sg).translate((0.0, sg * 0.0785, 0.088))
        b.add(leaf, "leafmark", name="leafmark", outline=False, covered_by=lid)
    prof = [(0.088 * 0.95, z) for z, *_ in rings[1:4]]
    glass_glints(b, [(0.086, 0.02), (0.088, 0.16)], lid, phases=(2.6, 5.75))
    del prof
    bottle_top(b, 0.031, 0.256, 0.050, lid)
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (0.0, 0.0, level)), {"kind": "potion", "potion": "antidote"}


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
        el = [on_sheet(0.062 * math.cos(TAU * i / 24), 0.038 * math.sin(TAU * i / 24)) for i in range(25)]
        b.add(IK.surface_strip(el, [up] * len(el), 0.0095, lift=0.0010, depth=0.0025), acc, name="glyph",
              outline=False, covered_by=sid)
        b.add(M.cylinder(0.019, 0.019, sheet_z(0) + 0.0028, sheet_z(0) + 0.0050, sides=14), acc, name="pupil",
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
    for a0, w, reg in ((0.6, 0.0105, "vein"), (3.5, 0.0085, "vein2"), (2.0, 0.006, "vein2")):
        pts, nrm = [], []
        for i in range(12):
            t = i / 11
            z = 0.020 + 0.170 * t
            p, nn = fruit_surface(a0 + 0.30 * math.sin(math.pi * t * 1.2), z)
            pts.append(p)
            nrm.append(nn)
        b.add(IK.surface_strip(pts, nrm, w, lift=0.0010, depth=0.003), reg, name="vein", outline=False,
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
    return obj, sockets(top_z, (0.0, 0.0, 0.14)), {"kind": "fruit"}


# ── gems ────────────────────────────────────────────────────────────────────────────────────────────────
def gem_cut(kind: str, r: float) -> list[tuple[float, float]]:
    """Web ``gemCut`` girdle outlines (IconKit.ts), x across, y depth (the icon's y → the gem's +Y)."""
    pts = []
    if kind in ("round", "oval"):
        k = 0.8 if kind == "oval" else 1.0
        n = 12 if kind == "round" else 10
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


def gem_part(outline_xy, r: float, chipped: bool) -> M.Part:
    """Faceted gem, table up: girdle band, crown (each quad split into two star facets, af_sub 0 / 1), table
    (af_sub 2), pavilion to a culet (af_sub 3 / 0 alternating). Culet at z = 0."""
    pts = list(outline_xy)
    if chipped:   # web: knock a corner off, roughen the girdle
        pts = [(x * 0.86, y * 0.90) if i % 3 == 1 else (x, y) for i, (x, y) in enumerate(pts)]
        del pts[int(len(pts) * 0.3)]
    n = len(pts)
    g, hc, hp = r * 0.10, r * 0.40, r * 0.78
    tk, toff = 0.54, (-0.05 * r, -0.04 * r)
    bm = bmesh.new()
    sub = bm.faces.layers.int.new("af_sub")
    z0 = hp
    g0 = [bm.verts.new((x, y, z0)) for x, y in pts]
    g1 = [bm.verts.new((x, y, z0 + g)) for x, y in pts]
    tb = [bm.verts.new((x * tk + toff[0], y * tk + toff[1], z0 + g + hc)) for x, y in pts]
    cul = bm.verts.new((0.0, 0.0, 0.0))

    def face(vs, tag):
        f = bm.faces.new(vs)
        f[sub] = tag
    for i in range(n):
        j = (i + 1) % n
        face((g0[i], g0[j], g1[j], g1[i]), 1)
        face((g1[i], g1[j], tb[j]), 0 if i % 2 == 0 else 1)
        face((g1[i], tb[j], tb[i]), 1 if i % 2 == 0 else 0)
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
    part = gem_part(gem_cut(cut, r), r, tier == 1)
    table = f"{k}_glow" if tier >= 3 else f"{k}_table"
    b.add(part, f"{k}_a", name="gem", sub_regions={1: f"{k}_b", 2: table, 3: f"{k}_deep"})
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    base_id = f"g_{k}_{tier}"
    return obj, sockets(top_z, (0.0, 0.0, top_z)), {"kind": "gem", "gem": k, "tier": tier, "baseId": base_id,
                                                    "cut": cut}


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
    for dx in (0.0, 0.016):
        b.add(closed_tube(Vector((0, 0, 0)), 0.021, 0.0055, n=16, sides=6).rotate("y", 90)
              .translate((tie.x + dx, 0.0, tie.z + 0.004)), "twine", name="twine", hull_pad=True)
    obj = asset.finish_mesh(b, pal, CLS)
    top_z = max(v.co.z for v in obj.data.vertices)
    return obj, sockets(top_z, (0.10, 0.0, top_z)), {"kind": "quest", "itemKind": "herb", "questItem": "mat_herb"}


def build_quest_gel(pal: Palette):
    """Glossy slime-gel blob with a drip and a lighter core (QuestItemIcons.ts gel)."""
    b = builder("SM_Pickup_Quest_Gel", pal, "quest")
    n = 22
    zs = [0.0, 0.006, 0.020, 0.040, 0.062, 0.084, 0.102, 0.116, 0.126, 0.132]
    rs = [0.090, 0.104, 0.110, 0.106, 0.094, 0.076, 0.054, 0.034, 0.016, 0.004]

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
            z = 0.055 + 0.055 * t
            a = ph + 0.45 * (t - 0.5)
            p = surf(a, z)
            pts.append(p)
            nrm.append(Vector((p.x, p.y, (p.z - 0.02) * 1.6)).normalized())
        b.add(IK.surface_strip(pts, nrm, 0.011, lift=0.0010, depth=0.004), "gel_hi", name="glint", outline=False,
              covered_by=gid)
    cap = []
    for a in [TAU * i / 14 for i in range(14)]:
        cap.append((0.040 * math.cos(a) - 0.012, 0.034 * math.sin(a) + 0.006))
    cp = plate_xy(cap, 0.0, 0.0025, rings=2)
    cp.deform(lambda co: Vector((co.x, co.y, co.z + 0.126 - 18.0 * (co.x ** 2 + co.y ** 2) - 0.0005)))
    b.add(cp, "gel_core", name="core", outline=False, covered_by=gid)
    # drip running down the side + a droplet beside the blob
    a0 = -0.6
    pts = [surf(a0, z) * 1.0 + Vector((math.cos(a0), math.sin(a0), 0)) * 0.006 for z in (0.07, 0.05, 0.03, 0.014)]
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
    # bail ring between the lobes and the chain lying in a loose loop round the heart
    top = Vector((0.0, 0.010 + 5 * s_ * 1.0 + 0.008, 0.0145))
    bail = closed_tube(Vector((0, 0, 0)), 0.0105, 0.0036, n=14, sides=6).rotate("y", 90).translate(tuple(top))
    b.add(bail, "chain", name="bail", hull_pad=True)
    loop = [top + Vector((0.0, 0.006, 0.0))]
    for i in range(1, 24):
        t = i / 24
        a = math.radians(95 + 340 * t)
        rr = 0.098 * (1 + 0.10 * math.sin(2 * a + 0.4))
        loop.append(Vector((rr * math.cos(a) * 0.95, rr * math.sin(a) + 0.008, 0.0050)))
    loop.append(top + Vector((0.004, 0.006, 0.0)))
    b.add(M.tube(loop, [0.0045] * len(loop), sides=8, dome0=0.0, dome1=0.0), "chain", name="chain", hull_pad=True)
    obj = asset.finish_mesh(b, pal, CLS)
    tz = top_z(obj)
    return obj, sockets(tz, (0.0, 0.004, tz)), {"kind": "quest", "itemKind": "pendant", "questItem": "mat_pendant"}


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


def loot_entry(meta: dict, tint: bool) -> dict:
    e = {"tint": tint, "tintAmount": 0.85 if tint else 0.0, "sockets": {"beam": "ground centre (pool + pillar base)",
                                                                        "label": "name plate anchor",
                                                                        "glint": "sparkle"}}
    e.update(QUALITY_HOOKS)
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


def review_asset(obj, name: str, out_dir: Path | None = None) -> tuple[list[Path], dict]:
    """The standard static review at the shipped ink (game.png, game_1080crop.png, game_inkgate.png, closeup.png),
    measured by the interactive ink gate (the floor occludes the hull under the contact, as the terrain does)."""
    obj.hide_render = False
    top = top_z(obj)
    rv = review.Review(name, obj, [obj], max(top, 0.05), blob_radius=max(0.08, footprint_r(obj) * 1.15),
                       game_outline_px=outline.SCREEN_PX_1080[CLS], outline_class=CLS, out_dir=out_dir)
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
    for ri, (row, dist, step) in enumerate(zip(rows, (0.75, 1.35, 1.85), (0.46, 0.40, 0.21))):
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
            extra["note"] = ("VFX mesh (reward.gold burst, AbyssVfxDefaults): toon slot only, no outline (spec §1.3 "
                             "VFX); the VFX system replaces slot 0 with M_AF_FX_Mesh and sizes it by its bounds")
        e = export.static_entry(obj, fbx, CATEGORY, pal, cls, ids, socks, footprint_tiles=[1, 1], blocking=False,
                                extra=extra)
        if cls in outline.INK_MIX and len(e["materialSlots"]) > 1:
            e["materialSlots"][1]["outlineMix"] = outline.INK_MIX[cls]
        if previews:
            out = paths.preview_dir(name)
            if grp == "coin":
                with IK.only_visible([obj]):
                    obj.hide_render = False
                    img = IK.studio([obj], res=(300, 260), pitch=-38.0, ink_px=0.0, label=name)
                    obj.hide_render = True
                files = [IK.save(out, "closeup.png", img)]
                v = None
            else:
                files, v = review_asset(obj, name)
            e["previews"] = [str(p.relative_to(paths.preview_root())) for p in files]
            if v is not None:
                e["inkGate"] = IK.ink_entry(v)
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
        files, v = review_asset(o[0], name, out_dir=out / name)
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
