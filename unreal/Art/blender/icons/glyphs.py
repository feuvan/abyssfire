"""UI glyphs, quest-item icons and story emblems (spec §9.3–§9.5, DECISIONS R10 / R11).

* ``T_UI_Glyph_<id>`` (15, web ``UiGlyphs.ts``): gold, str, dex, vit, int, spi, lck, atk, def, hp, mp,
  quest_available, quest_complete, close, sort — 128² (web 64²; shown at 14–32 px), ink 3.2 u, rim 1.4 u
  (quest markers 3.6 u: the same ``SM_UI_QuestMarker_*`` meshes the NPCs wear, gold, with the web's glow);
  coins are the shipped ``SM_Pickup_GoldCoin``, the sword the shipped ``SM_Weapon_Sword``; ``def`` is the web's own
  heater (steel rim, blue field, gold lozenge — the in-world T4 shield's cross read as a different emblem).
* ``T_UI_QuestItem_<kind>`` (Chapter 1: herb, gel, pendant; web ``QuestItemIcons.ts``) — 256²: herb and gel are icon
  models of the web drawings (``q_herb``: five sprigs in their own greens, crescent leaf, twine, splayed stems;
  ``q_gel``: the jelly blob with its web under-glow), the pendant the ground mesh ``SM_Pickup_Quest_Pendant`` with
  icon-only edits (a chain loop in place of the ground coil).
* ``T_UI_Emblem_Villain`` / ``T_UI_Emblem_Generic`` (story ``say`` portraits without art, quests-story §8.5) —
  512²: the villain's violet eye in a crown of violet flame tongues (clamped inside the ring, one line each), the
  generic ember sigil in a gold ring.

Run:  EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/glyphs.py [--only id,…]
"""
from __future__ import annotations

import math
import sys
import time
from dataclasses import replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import bpy  # noqa: E402,F401
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import iconkit as K  # noqa: E402
import item_models as IM  # noqa: E402
import motifs as F  # noqa: E402
import shapes as S  # noqa: E402
from item_models import W, Wb, Wp, U  # noqa: E402
from kit import mesh as M, paths, pngio  # noqa: E402
from motifs import R, at, pill  # noqa: E402

GLYPH_PX = 128
GOLD = "#F0C040"


def sparkle(x, y, r=5.0, hex="#FFFFFF", alpha=1.0):
    return K.Accent("sparkle", u=(x, y), r_u=r, hex=hex, alpha=alpha)


def glow(x, y, r, hex, alpha, under=False):
    return K.Accent("glow", u=(x, y), r_u=r, hex=hex, alpha=alpha, under=under)


# ── glyphs ──────────────────────────────────────────────────────────────────────────────────────────────
def g_gold(c):
    for (x, y, r) in ((36, 64, 26), (60, 52, 26), (44, 34, 24)):
        o = K.import_fbx("Pickups", "SM_Pickup_GoldCoin")
        K.place(o, toward=(0, 0, 1), up=(0, 1, 0), pitch=-52, at=at(x, y, -(96 - y) * .3), scale=r * U / .065)
        c.obj(o)
    c.acc.append(sparkle(30, 26, 8))


def g_str(c):
    ft = R("#E0A070", .42, .4)
    c.add(pill(S.rrect2d(56, 50, 14, 4, 48, 51), depth=12, d=0, edge=.5), "fist", ft)
    for i in range(4):
        c.add(pill(S.circle2d(7.5, 16, 28 + i * 13.3, 28, rz=9), depth=8, d=-5, edge=.6), "fist")
    c.add(pill(S.capsule2d((20, 56), (52, 50), 8, 7), depth=7, d=-11, edge=.6), "fist")
    c.add(pill(S.rrect2d(52, 18, 4, 3, 48, 81), depth=9, d=1, edge=.5), "cuff", R("#C83030", .42, .38))


def g_dex(c):
    vane = S.chain2d(S.bez([(0, 40), (-22, 10), (-18, -30), (0, -44)], 14), S.bez([(0, -44), (18, -30), (22, 10), (0, 40)], 14))
    vane = F.xform2d(vane, 48, 48, math.pi / 4)
    body = pill(vane, depth=5, d=0, edge=.6, dome=.4)
    c.add(body, "vane", R("#5AD070", .42, .45))
    surf = S.Surface(body)
    for y in range(-30, 34, 9):
        for sx in (-1, 1):
            seg = F.xform2d([(0, y + 6), (sx * 18, y - 3)], 48, 48, math.pi / 4)
            c.add(IM.drape_strip(surf, Wp(seg), .009, .004, step=.006), "barb", R("#2F8A48", .3, .15))
    surf.free()
    shaft = F.xform2d([(0, -40), (0, 46)], 48, 48, math.pi / 4)
    F.ribbon3(c, S.resample2d(shaft, 4), lambda t: 1.5 * (1 - .4 * t), depth=3, d=-5.5, tag="shaft", reg=R("#F0E8D0", .35, .4))


def g_vit(c):
    heart = S.chain2d(S.bez([(48, 86), (4, 56), (12, 12), (48, 30)], 16), S.bez([(48, 30), (84, 12), (92, 56), (48, 86)], 16))
    c.add(pill(heart, depth=13, d=0, edge=.5, dome=.4), "heart", R("#E0303C", .42, .45))
    gt = R(GOLD, .4, .5)
    c.add(pill(S.rrect2d(10, 32, 3, 3, 49, 56), depth=4, d=-12, edge=.6), "plus", gt)
    c.add(pill(S.rrect2d(32, 10, 3, 3, 49, 56), depth=4, d=-12, edge=.6), "plus")


def g_int(c):
    cover = S.chain2d(S.bez([(8, 30), (28, 20), (48, 30)], 8), S.bez([(48, 30), (68, 20), (88, 30)], 8), [(88, 80)],
                      S.bez([(88, 80), (68, 70), (48, 80)], 8), S.bez([(48, 80), (28, 70), (8, 80)], 8))
    c.add(pill(cover, depth=4, d=4, edge=.5), "cover", R("#2F5AB0", .42, .4))
    pt = R("#F0E2BC", .42, .3)
    for s in (-1, 1):
        page = S.chain2d(S.bez([(48, 28), (48 + s * 20, 18), (48 + s * 36, 26)], 8), [(48 + s * 36, 72)],
                         S.bez([(48 + s * 36, 72), (48 + s * 20, 64), (48, 74)], 8))
        body = pill(page, depth=5, d=-1, edge=.45, dome=.3)
        c.add(body, "page", pt)
        surf = S.Surface(body)
        for i in range(3):
            c.add(IM.drape_strip(surf, Wp([(48 + s * 10, 38 + i * 10), (48 + s * 28, 36 + i * 10)]), .008, .004), "text",
                  R("#B8A27A", .3, .1))
        surf.free()
    c.acc += [sparkle(48, 20, 9, "#BFE0FF"), glow(48, 20, 16, "#80C0FF", .5)]


def g_spi(c):
    for i, (s, col) in enumerate(((1, "#8A4AE0"), (.66, "#B888FF"), (.36, "#EEDCFF"))):
        pts = S.chain2d(S.bez([(48, 90 - 4 * s), (48 - 30 * s, 84), (48 - 26 * s, 48), (48 - 4 * s, 10 + (1 - s) * 40)], 12),
                        S.bez([(48 - 4 * s, 10 + (1 - s) * 40), (48 + 2 * s, 36), (48 + 30 * s, 50), (48 + 26 * s, 70)], 12),
                        S.bez([(48 + 26 * s, 70), (48 + 22 * s, 84), (48 + 8 * s, 90), (48, 90 - 4 * s)], 8))
        c.add(pill(pts, depth=5 + 2 * i, d=-i * 4, edge=.6, dome=.3), f"fl{i}", R(col, .3, .5, e=(.2, .4, .65)[i]))
    c.acc.append(glow(48, 64, 26, "#B070FF", .35))


def g_lck(c):
    F.ribbon3(c, S.bez([(50, 52), (58, 76), (70, 90)], 10), 2.5, depth=4, d=2, tag="stem", reg=R("#2F7A2F", .42, .35))
    lt = R("#48B848", .42, .45)
    for i in range(4):
        leaf = S.chain2d(S.bez([(0, 0), (-26, -10), (-20, -36), (-6, -32)], 10), S.bez([(-6, -32), (0, -30), (0, -24)], 4),
                         S.bez([(0, -24), (0, -30), (6, -32)], 4), S.bez([(6, -32), (20, -36), (26, -10), (0, 0)], 10))
        leaf = F.xform2d(leaf, 48, 48, i * math.pi / 2 + math.pi / 4)
        c.add(pill(leaf, depth=6, d=-i * .6, edge=.6, dome=.35), "leaf", lt)
    core = M.ellipsoid((5 * U, 3.5 * U, 5 * U), (0, 0, 0), sides=16, rings=8)
    core.translate(at(48, 48, -6))
    c.add(core, "core", R(GOLD, .4, .55))
    c.acc.append(sparkle(28, 24, 7))


def g_atk(c):
    # the glyph set's own 3.2 u ink (a thinner 2.4 u line made atk the one light-weight glyph of the row); the blade
    # and guard widened 2.1× so the heavier line does not swallow them at 14–32 px
    F.weapon(c, "sword", "T2", 25, 73, math.pi / 4, 1.58, d=0, widen=2.1)


def g_def(c):
    """Web ``def``: a steel heater, a blue field inset (0.8 × 0.82 about (48, 46)) and a gold lozenge."""
    import weapon_models as WM
    outer = WM.heater2d(48, 8, 36, 84, n=14)
    c.add(pill(outer, depth=8, d=0, edge=.5, dome=.12), "rim", R("#9AA4B4", .42, .5))
    inner = [(48 + (x - 48) * .8, 46 + (y - 46) * .82) for x, y in outer]
    c.add(pill(inner, depth=6, d=-8, edge=.55, dome=.25), "field", R("#3E6AB0", .42, .4))
    c.add(pill([(48, 22), (58, 44), (48, 70), (38, 44)], depth=4, d=-14, edge=.6, dome=.45), "lozenge", R(GOLD, .4, .5))


def _split(part: M.Part, keep_top: bool, fn) -> M.Part:
    """Keep the faces whose centre is above (``keep_top``) or below the curve z = fn(x)."""
    import bmesh
    bm = part.bm
    dead = [f for f in bm.faces if (f.calc_center_median().z > fn(f.calc_center_median().x)) != keep_top]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    return part


def orb(color, dark):
    def build(c):
        def wave(x):        # the liquid line, model space (web: 0,60 q 30,52 48,60 q 70,68 96,58)
            u = x / U + 48
            return W(0, 60 + 4.5 * math.sin((u - 6) / 96 * 2 * math.pi * 1.1))[1]
        whole = M.ellipsoid((34 * U, 30 * U, 34 * U), (0, 0, 0), sides=48, rings=24)
        whole.translate(at(48, 50, 0))
        surf = S.Surface(whole)
        for top in (True, False):
            sph = whole.copy()
            _split(sph, top, wave)
            c.add(sph, "orb" if top else "liquid",
                  R(color, .42, .5, e=.1) if top else R(dark, .42, .35, e=.05))
        xs = [x * U for x in range(-33, 34)]
        line_pts = [(x, wave(x)) for x in xs if (x / U) ** 2 + ((wave(x) - W(48, 50)[1]) / U) ** 2 < 33.2 ** 2]
        c.add(IM.drape_strip(surf, line_pts, .016, .002, step=.004, sink=.006), "liquid")
        whole.bm.free()
        surf.free()
        hl = pill(S.circle2d(10, 20, 36, 34, rz=6), depth=1.5, d=-27, edge=.8)
        hl.rotate("y", 34, pivot=at(36, 34, -27))
        c.add(hl, "glint", R("#FFFFFF", .1, .6, e=.85))
    return build


def g_quest(asset):
    def build(c):
        o = K.import_fbx("NPCs", asset)
        K.place(o, yaw=0, at=at(48, 88, 0), scale=.74 / .40)
        c.obj(o)
        c.style["ink_u"] = 3.6 * 96 / 96
        c.acc.append(glow(48, 48, 44, "#FFC040", .45, under=True))
    return build


def g_close(c):
    c.add(pill(S.circle2d(40, 48, 48, 48), depth=8, d=2, edge=.4), "ring", R("#4A4450", .42, .4))
    c.add(pill(S.circle2d(32, 44, 48, 48), depth=8, d=-3, edge=.5, dome=.5), "button", R("#B82A30", .42, .4))
    xt = R("#F4E8D0", .42, .3)
    c.add(pill(S.capsule2d((34, 34), (62, 62), 5.5), depth=4, d=-10, edge=.6), "x", xt)
    c.add(pill(S.capsule2d((62, 34), (34, 62), 5.5), depth=4, d=-10, edge=.6), "x")


def g_sort(c):
    t = R("#E8D8B0", .42, .4)
    for (y, w) in ((16, 46), (38, 34), (60, 22)):
        c.add(pill(S.rrect2d(w, 13, 4, 3, 8 + w / 2, y + 6.5), depth=6, d=0, edge=.5), "bar", t)
    c.add(pill([(66, 12), (78, 12), (78, 60), (90, 60), (72, 88), (54, 60), (66, 60)], depth=7, d=0, edge=.5, dome=.4),
          "arrow", R(GOLD, .4, .5))


GLYPHS = {"gold": g_gold, "str": g_str, "dex": g_dex, "vit": g_vit, "int": g_int, "spi": g_spi, "lck": g_lck,
          "atk": g_atk, "def": g_def, "hp": orb("#D8283A", "#9A1424"), "mp": orb("#2F6CF0", "#1C3E9A"),
          "quest_available": g_quest("SM_UI_QuestMarker_Excl"), "quest_complete": g_quest("SM_UI_QuestMarker_Quest"),
          "close": g_close, "sort": g_sort}


# ── quest items ─────────────────────────────────────────────────────────────────────────────────────────
# pendant: the ground mesh (SM_Pickup_Quest_Pendant) with icon-only edits; herb / gel: icon models of the web drawings
QUEST_ITEMS = {"herb": "SM_Pickup_Quest_Herb", "gel": "SM_Pickup_Quest_Gel", "pendant": "SM_Pickup_Quest_Pendant"}
QUEST_SOURCE = {
    "herb": {"model": "icons/glyphs.py:q_herb (web QuestItemIcons herb: five sprigs in their own greens, crescent "
                      "leaf, twine #C89A5A, splayed cut stems)", "inWorld": "SM_Pickup_Quest_Herb"},
    "gel": {"model": "icons/glyphs.py:q_gel (web QuestItemIcons gel: blob, light core, nucleus, bubbles, drip)",
            "inWorld": "SM_Pickup_Quest_Gel"},
    "pendant": {"mesh": "SM_Pickup_Quest_Pendant",
                "iconEdits": "ground coil replaced by a loop of chain links (web pendant chain); glow on the ruby"},
}
GEL = "#6EE04A"


def q_gel(c):
    """web gel (QuestItemIcons.ts:191-216): a glossy jelly blob (radial white → base → dark), a darker nucleus, three
    bubble rings, a drip hanging off the bottom right, a white specular streak and dot, an under-glow."""
    body = [(16, 74), (18, 52), (34, 32), (52, 26), (70, 36), (80, 58), (80, 76), (64, 82), (48, 80), (32, 83)]
    blob = pill(body, depth=13, back=5, d=0, edge=.55, dome=.35, smooth=True)
    c.add(blob, "gel", R(GEL, .38, .50, e=.08))
    surf = S.Surface(blob)
    # light jelly core (web gradient: 55 % white toward the upper left) laid on the surface
    core = S.blob2d(Wp([(24, 60), (28, 44), (40, 35), (54, 35), (58, 46), (48, 58), (34, 66)]), 6)
    c.add(conform(core, surf, .35, .5), "core", R(F._mix(GEL, "#FFFFFF", .42), .30, .45, e=.12))
    # nucleus: web ellipse (56, 62) 7 × 5 rotated .3, mix(col, #0A3018, .35) — the darker body inside the jelly
    nuc = F.xform2d([(8 * math.cos(a), 6 * math.sin(a)) for a in [2 * math.pi * i / 22 for i in range(22)]], 57, 62, .3)
    c.add(conform(Wp(nuc), surf, .35, .6), "nucleus", R(F._mix(GEL, "#0A3018", .5), .35, .30))
    for (x, y, r) in ((36, 66, 2.8), (68, 50, 2.0), (45, 74, 1.8)):
        c.add(conform(S.ribbon2d(Wp([(x + r * math.cos(a), y + r * math.sin(a)) for a in
                                     [2 * math.pi * i / 16 for i in range(17)]]), .45 * U), surf, .5, .3),
              "bubble", R("#FFFFFF", .1, .5, e=.6))
    drip = S.chain2d(S.bez([(66, 78), (71.5, 88), (66.5, 94)], 8), S.bez([(66.5, 94), (61, 90), (61.5, 79)], 8))
    c.add(pill(drip, depth=4.0, back=3, d=-3.0, edge=.7, dome=.4), "gel")
    streak = S.ribbon2d(Wb([(25, 61), (29, 45), (43, 34)], 14), lambda t: 1.8 * U * (.3 + .7 * math.sin(math.pi * t)))
    c.add(conform(streak, surf, .7, .4), "glint", R("#FFFFFF", .0, .0, e=.9))
    c.add(conform(S.circle2d(2.1 * U, 12, *W(26, 69)), surf, .7, .4), "glint")
    surf.free()
    c.style.update(seam_min_contrast=.12)
    c.nolines = ("core", "glint", "bubble")          # soft gradient / highlights: no seam lines round them
    c.view = (1.1, (48, 59))                         # web 1 u = 1.1 icon u about the blob's centre (drip inside)
    # web: glow(c, (48, 58), 40, #6EE04A, .3) under the blob — the web stops ('webglow'): the steep (1 − d)² glow
    # vanished under the body (review r3)
    c.acc += [K.Accent("webglow", u=(48, 58), r_u=40, hex=GEL, alpha=.3, under=True), sparkle(60, 34, 4.5, alpha=.9)]


HERB_SPRIGS = (((24, 26), (30, 44), "#5AA83A"), ((46, 14), (46, 38), "#6CBF42"), ((72, 22), (64, 42), "#4F9A34"),
               ((34, 18), (40, 40), "#7CCC4C"), ((62, 14), (56, 38), "#62B43E"))      # web herb: (tip, mid, green)
HERB_STEM, HERB_TWINE = "#4A7A28", "#C89A5A"


def _quad(a, ctl, b, n=10):
    return [((1 - t) ** 2 * a[0] + 2 * (1 - t) * t * ctl[0] + t * t * b[0],
             (1 - t) ** 2 * a[1] + 2 * (1 - t) * t * ctl[1] + t * t * b[1]) for t in [i / n for i in range(n + 1)]]


def leaf_halves(a, b, w, bend=0.0):
    """web leafPath(a, b, w, bend) split along its midrib: (lit half, shaded half) outlines — the seam between the
    two halves is the web's midrib line."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    ln = math.hypot(dx, dy) or 1.0
    nx, ny = -dy / ln, dx / ln
    mx, my = (a[0] + b[0]) / 2 + nx * bend, (a[1] + b[1]) / 2 + ny * bend
    s1 = _quad(a, (mx + nx * w * 1.3, my + ny * w * 1.3), b)
    s2 = _quad(b, (mx - nx * w * 1.3, my - ny * w * 1.3), a)
    rib = _quad(b, (mx, my), a, 6)[1:-1]
    h1, h2 = s1 + rib, s2 + list(reversed(rib))
    lit = (nx * -.72 + ny * -.7) > 0                 # the half bulging toward the upper-left light
    return (h1, h2) if lit else (h2, h1)


def _spline(pts, n=8):
    """Catmull-Rom through web points (open)."""
    out = []
    P_ = [pts[0]] + list(pts) + [pts[-1]]
    for i in range(1, len(P_) - 2):
        p0, p1, p2, p3 = P_[i - 1], P_[i], P_[i + 1], P_[i + 2]
        for k in range(n):
            t = k / n
            out.append(tuple(.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t * t
                                   + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t ** 3) for j in range(2)))
    out.append(pts[-1])
    return out


def q_herb(c):
    """web herb (QuestItemIcons.ts:113-137) + spec §9.3: five sprigs, each its own green (#5AA83A / #6CBF42 / #4F9A34 /
    #7CCC4C / #62B43E) — a long leaf to the tip and a side leaf mixed toward #2A5A18 — on #4A7A28 stems tied with
    #C89A5A twine (three wraps, a loose end), a crescent leaf crowning the middle sprig; the stems splay below the
    twine and taper to cut ends (review r3: the ground mesh's blossoms read as specks, its greens as one, the stem
    bundle as a square tube). Later sprigs lie in front, as the web draws them."""
    tie = (47, 66)
    stem_r = R(HERB_STEM, .42, .35)
    for k, (tip, mid, col) in enumerate(HERB_SPRIGS):
        d = 6.0 - k * 3.2                                   # web draw order: later sprigs nearer the viewer
        foot = (46 + (k - 2) * 3.4 + (mid[0] - 48) * .06, 88 - abs(k - 2) * 1.2)
        path = _spline([foot, (46 + (k - 2) * 1.6, 76), (tie[0] + (mid[0] - tie[0]) * .2, tie[1]), mid, tip], 6)
        n = len(path)
        rad = [U * (0.55 + 0.6 * min(1.0, i / 3.0)) * (1 - .45 * i / (n - 1)) for i in range(n)]   # cut end, taper up
        c.add(M.tube([at(x, y, d + 1.6) for x, y in path], rad, sides=8), f"stem{k}", stem_r)
        lt, dk = leaf_halves(mid, tip, 5.6)
        c.add(pill(lt, depth=2.0, back=.8, d=d, edge=.7, dome=.2), f"leaf{k}", R(col, .42, .18))
        c.add(pill(dk, depth=2.0, back=.8, d=d, edge=.7, dome=.2), f"leaf{k}d", R(F._mix(col, "#2A5A18", .22), .42, .3))
        base2 = (mid[0] + (tie[0] - mid[0]) * .35, mid[1] + (tie[1] - mid[1]) * .35)
        tip2 = (mid[0] + (-12 if mid[0] < 48 else 12), mid[1] + 2)
        side = F._mix(col, "#2A5A18", .2)
        lt, dk = leaf_halves(base2, tip2, 4.4)
        c.add(pill(lt, depth=1.8, back=.7, d=d - .4, edge=.7, dome=.2), f"side{k}", R(side, .42, .16))
        c.add(pill(dk, depth=1.8, back=.7, d=d - .4, edge=.7, dome=.2), f"side{k}d", R(F._mix(side, "#2A5A18", .22), .42, .3))
    # crescent leaf crowning the middle sprig (spec §9.3; the ground mesh's moon-curl)
    cres = S.chain2d(S.bez([(43, 16), (40, 5), (52, 2), (58, 9)], 10), S.bez([(58, 9), (51, 6), (45, 9), (46, 16)], 10))
    c.add(pill(cres, depth=2.0, back=.8, d=-1.5, edge=.7, dome=.2), "cres", R("#7CCC4C", .42, .2))
    # twine: three wraps and the loose end (web roundRect 37, 62, 20 × 8 and leafPath (56, 66) → (66, 76))
    tw = R(HERB_TWINE, .42, .3)
    tw2 = R(F._mix(HERB_TWINE, "#8A6232", .3), .42, .25)
    for i in range(3):                       # three abutting wraps: one band, the wrap lines are the seams
        x0 = 37.0 + i * 20.0 / 3
        c.add(pill([(x0, 62), (x0 + 20 / 3, 62.4), (x0 + 20 / 3, 70.4), (x0, 70)], depth=3.0, back=1.6, d=-13.0,
                   edge=.35, dome=.3), f"twine{i}", tw if i % 2 == 0 else tw2)
    lt, dk = leaf_halves((56, 66), (66, 77), 2.4, bend=1.0)
    c.add(pill(lt + list(reversed(dk))[1:-1], depth=1.6, back=.8, d=-12.0, edge=.7), "twine_end", tw)
    c.style.update(seam_min_contrast=.08)
    c.acc += [sparkle(30, 30, 4, "#FFF6C8", .9)]


def conform(outline_model, surf: S.Surface, lift_u: float, depth_u: float):
    """A thin painted patch (model outline) laid on ``surf``: each vertex keeps its own thickness offset and rides the
    surface ``lift_u`` units in front of it (decals on a domed body)."""
    p = S.pillow(outline_model, depth=depth_u * U, back=depth_u * .5 * U, wall=depth_u * .3 * U, edge=.8, dome=.2)

    def f(co):
        hit, _n = surf.at(co.x, co.z)
        base = hit.y if hit is not None else 0.0
        return Vector((co.x, base + co.y - lift_u * U, co.z))
    return p.deform(f)


QUEST_MODELS = {"herb": q_herb, "gel": q_gel}           # icon models (render_comp); the rest: ground meshes


def quest_item(kind: str):
    o = K.import_fbx("Pickups", QUEST_ITEMS[kind])
    K.place(o, toward=(0, 0, 1), up=(0, 1, 0), roll=10, pitch=-8)
    # the ground coil → the web's chain: a loose loop of links hanging from the bail up to the top corners
    K.delete_swatch_faces(o, "Pickups", "quest.chain")
    bail = K.swatch_centroid(o, "Pickups", "quest.bail")
    ruby = K.swatch_centroid(o, "Pickups", "quest.ruby")
    vs = {v for i in K.swatch_faces(o, "Pickups", "quest.locket") for v in o.data.polygons[i].vertices}
    lk = [o.matrix_world @ o.data.vertices[v].co for v in vs]
    k = (max(p.x for p in lk) - min(p.x for p in lk)) / 40.0          # world metres per web unit (locket ≈ 40 u wide)
    pts = []
    for i in range(29):
        t = i / 28
        a = math.pi * (1 - t)
        x, y = 48 + math.cos(a) * 27, 13 + math.sin(a) * 27.5
        pts.append(bail + Vector(((x - 48) * k, 3.0 * k, -(y - 40.5) * k)))
    links = S.links(pts, 6.4 * k, 1.05 * k, width=3.9 * k, face=(0, -1, 0))
    ch = K.build_model("quest_pendant_chain", IM.pal("QuestIcons"), {"chain": R("#E2B64E", .40, .45)}, [(links, "chain")])
    acc = [K.Accent("glow", at=tuple(ruby), r_u=14, hex="#FF4050", alpha=.3), sparkle(34, 50, 4.5, alpha=.95)]
    return [o, ch], acc


# ── emblems ─────────────────────────────────────────────────────────────────────────────────────────────
VILLAIN_FLAME = (("#4A1088", .25), ("#6A1FB0", .35), ("#9B4DFF", .5))     # outer → core (colour, emissive)
VILLAIN_R_IN = 30.5          # flame tips stay inside the ring's inner edge (ring r 36, band 3, ink): review r3


def tongue(c, base, ang, length, w, d, tag, lean=0.0):
    """One flame tongue of the villain's crown: three nested layers (outer → core) from ``base`` along ``ang`` (canvas
    radians), each a pointed flame shape; the layers sit 0.6 u apart (under the emblem's 2.4 u depth step), so only
    the tongue's own silhouette gets an overlap line and the colour layers meet in thin seams (review r3: nested
    pillows 2.2 u apart drew doubled, offset outlines)."""
    for i, ((col, e), s) in enumerate(zip(VILLAIN_FLAME, (1.0, .70, .42))):
        hh, ww = length * s, w * s
        lx = lean * hh                       # the tip licks sideways (web flame lean): S-curved sides
        pts = S.chain2d(S.bez([(-ww, ww * .25), (-ww * 1.15, -hh * .40), (-ww * .1 + lx * .55, -hh * .66), (lx, -hh)], 10),
                        S.bez([(lx, -hh), (ww * .45 + lx * .35, -hh * .64), (ww * 1.15, -hh * .38), (ww, ww * .25)], 10),
                        S.bez([(ww, ww * .25), (0, ww * 1.1), (-ww, ww * .25)], 8))
        pts = F.xform2d(pts, base[0], base[1], ang + math.pi / 2)
        c.add(pill(pts, depth=2.0, back=1.0, d=d - i * .6, edge=.6, dome=.25), f"{tag}{i}", R(col, .3, .45, e=e))


def e_villain(c):
    """The villain's violet eye in a crown of violet flame on a dark disc inside a violet ring (quests-story §8.5)."""
    ring = F.ring3(36, 36, 3, 3)
    ring.translate(at(48, 52, 14))
    c.add(ring, "rim", R("#9B4DFF", .3, .5, e=.5))
    disc = pill(S.circle2d(34, 48, 48, 52), depth=3, d=16, edge=.6)
    c.add(disc, "disc", R("#12061A", .3, .1))
    # crown: seven tongues fanned behind the eye, the centre one tallest and nearest; every tip clamped inside the ring
    for k in (-3, 3, -2, 2, -1, 1, 0):
        a = -math.pi / 2 + k * .40
        base = (48 + math.cos(a) * 10, 54 + math.sin(a) * 6)
        length = min(VILLAIN_R_IN * (1 - abs(k) * .09), 30.0)
        tip_r = math.hypot(base[0] + math.cos(a) * length - 48, base[1] + math.sin(a) * length - 52)
        length -= max(0.0, tip_r - VILLAIN_R_IN)
        tongue(c, base, a, length, 8.4 - abs(k) * .7, 2.0 + abs(k) * 2.8, f"vf{abs(k)}", lean=.10 * k + (.06 if k == 0 else 0))
    eye = S.chain2d(S.bez([(18, 52), (34, 36), (62, 36), (78, 52)], 14), S.bez([(78, 52), (62, 68), (34, 68), (18, 52)], 14))
    c.add(pill(eye, depth=6, d=-4, edge=.5, dome=.5), "eye", R("#FFD0FF", .3, .5, e=.45))
    iris = pill(S.circle2d(12, 32, 48, 52), depth=4, d=-9.0, edge=.6)
    c.add(iris, "iris", R("#9B4DFF", .3, .5, e=.6))
    # the slit tapers to points well inside the iris and lies almost flush on it (a seam, not an overlap line: its
    # rounded ends used to ink as black dots)
    slit = S.chain2d(S.bez([(48, 43.5), (52.6, 52), (48, 60.5)], 12), S.bez([(48, 60.5), (43.4, 52), (48, 43.5)], 12))
    c.add(pill(slit, depth=.9, back=.3, d=-12.6, edge=.85, dome=.1), "pupil", R("#2A0036", .2, .05))
    c.style.update(depth_step_u=2.4, line_min_u=3.0)
    c.acc += [glow(48, 52, 40, "#9B4DFF", .45, under=True), sparkle(40, 46, 4, "#FFE8FF")]


def e_generic(c):
    ring = F.ring3(34, 34, 6, 5)
    ring.translate(at(48, 50, 2))
    c.add(ring, "ring", R("#C9A45A", .42, .55))
    rs = S.Surface(ring)
    for k in range(8):
        a = k * math.pi / 4
        c.add(S.dome_on(rs, *W(48 + math.cos(a) * 34, 50 + math.sin(a) * 34), .018, .012), "stud", R("#E8D08A", .3, .6))
    rs.free()
    c.add(pill(S.circle2d(31, 48, 48, 50), depth=3, d=6, edge=.6), "field", R("#2A1A12", .3, .15))
    F.flame(c, 48, 72, 40, 13, 0, d=-2, tag="sig")
    c.acc += [glow(48, 54, 30, "#FF9A3C", .35, under=True)]


EMBLEMS = {"Villain": e_villain, "Generic": e_generic}


# ── main ────────────────────────────────────────────────────────────────────────────────────────────────
def render_comp(cam, fn, name: str, size: int, ink_u: float, rim_u: float = K.RIM_U, fit=False):
    K.clear_objects()
    c = F.Comp(name, IM.pal("Glyphs"))
    fn(c)
    objs = c.build(getattr(c, "nolines", ()))
    view = getattr(c, "view", None)          # (scale, web centre): the web square scaled about a point (accents too)
    if fit:
        K.apply_frame(cam, K.fit(objs, fill=.84))
    elif view is not None:
        sc, (vx, vy) = view
        wx, wz = W(vx, vy)
        K.apply_frame(cam, K.Frame(world_per_unit=U / sc, cx=wx, cz=wz))
        c.acc = [replace(a, u=None if a.u is None else (48 + (a.u[0] - vx) * sc, 48 + (a.u[1] - vy) * sc),
                         r_u=a.r_u * sc, len_u=a.len_u * sc, width_u=a.width_u * sc) for a in c.acc]
    else:
        K.apply_frame(cam, K.Frame(world_per_unit=U, cx=0.0, cz=0.0))
    st = dict(ink_u=c.style.pop("ink_u", ink_u), rim_u=rim_u)
    st.update(c.style)
    # glyphs render at 256 (lines authored there) and are resampled to 128; emblems render natively at 512
    return K.render_icon(size=max(size, K.ICON_PX), style=K.IconStyle(**st), accents=c.acc)


def main(argv: list[str]) -> int:
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    cam = K.studio()
    IM.reset_palettes()
    out = {}
    t0 = time.time()
    for gid, fn in GLYPHS.items():
        if only and gid not in only:
            continue
        r = render_comp(cam, fn, f"glyph_{gid}", GLYPH_PX, ink_u=3.2, rim_u=1.4)
        out[f"T_UI_Glyph_{gid}"] = ("glyph", gid, r, K.resize(r.rgba, GLYPH_PX))
        print(f"[glyphs] {gid:16s} gate {'ok ' if r.gate['pass'] else 'FAIL'} span {r.gate['span']:.2f} {time.time() - t0:4.0f}s"
              + ("" if r.gate["pass"] else f" {r.gate}"))
    for kind in QUEST_ITEMS:
        if only and kind not in only:
            continue
        if kind in QUEST_MODELS:
            r = render_comp(cam, QUEST_MODELS[kind], f"quest_{kind}", K.ICON_PX, ink_u=K.INK_U)
        else:
            K.clear_objects()
            objs, acc = quest_item(kind)
            K.apply_frame(cam, K.fit(objs, fill=.82))
            r = K.render_icon(accents=acc)
        out[f"T_UI_QuestItem_{kind}"] = ("questItem", kind, r, r.rgba)
        print(f"[glyphs] quest {kind:10s} gate {'ok ' if r.gate['pass'] else 'FAIL'} span {r.gate['span']:.2f}"
              + ("" if r.gate["pass"] else f" {r.gate}"))
    for eid, fn in EMBLEMS.items():
        if only and eid not in only:
            continue
        r = render_comp(cam, fn, f"emblem_{eid}", 512, ink_u=2.0)
        out[f"T_UI_Emblem_{eid}"] = ("emblem", eid, r, r.rgba)
        print(f"[glyphs] emblem {eid:8s} gate {'ok ' if r.gate['pass'] else 'FAIL'} span {r.gate['span']:.2f}")
    if only:
        cells = [(k.replace("T_UI_", ""), K.resize(v[2].rgba, 256)) for k, v in out.items()]
        pngio.write_png(K.SCRATCH / "glyphs_only.png", K.atlas(cells, cols=6, scale=.5))
        pngio.write_png(K.SCRATCH / "glyphs_only_ui.png", K.ui_sheet(cells, cols=6, sizes=(32, 20), qualities=("normal",)))
        pngio.write_png(K.SCRATCH / "glyphs_only_1x.png", K.atlas(cells[:6], cols=3, scale=1.0))
        return 0
    return ship(cam, out)


def ship(cam, out) -> int:
    files = {}
    for name, (kind, ident, r, img) in out.items():
        rel = {"glyph": f"Icons/Glyphs/{name}.png", "questItem": f"Icons/QuestItems/{name}.png",
               "emblem": f"Portraits/{name}.png"}[kind]
        K.write_icon(rel, img)
        files[name] = rel
    # previews per texture: only the sheets (and tiles of a split sheet) that show it (review art 10)
    previews: dict[str, list[str]] = {n: [] for n in out}

    def sheets(kind, prefix, folder, stem, cols, ui_sizes, half=True):
        rows = [(k, k.replace(prefix, ""), v[3]) for k, v in out.items() if v[0] == kind]
        if not rows:
            return
        names = [k for k, _, _ in rows]
        cells = [(lbl, img) for _, lbl, img in rows]
        px = cells[0][1].shape[0]
        jobs = [(f"{stem}_1x.png", K.atlas(cells, cols=cols), K.atlas_boxes(len(cells), px, cols))]
        if half:
            jobs.append((f"{stem}_half.png", K.atlas(cells, cols=cols, scale=.5), K.atlas_boxes(len(cells), px // 2, cols)))
        jobs.append((f"{stem}_ui.png", K.ui_sheet(cells, cols=cols, sizes=ui_sizes, qualities=("normal",)),
                     K.ui_sheet_boxes(len(cells), cols, ui_sizes)))
        for file, img, boxes in jobs:
            for n, files in zip(names, K.save_preview_cells(folder, file, img, boxes)):
                previews[n].extend(files)
    sheets("glyph", "T_UI_Glyph_", "icons_glyphs", "glyphs", 8, (32, 20))
    sheets("questItem", "T_UI_QuestItem_", "icons_quest", "quest_items", 3, (48, 36))
    sheets("emblem", "T_UI_Emblem_", "portraits", "emblems", 2, (64, 26), half=False)
    man = K.IconManifest()
    for name, (kind, ident, r, img) in out.items():
        px = img.shape[0]
        extra = {"glyph": dict(glyphId=ident, renderedAt=K.ICON_PX),
                 "questItem": dict(questItemKind=ident, source=QUEST_SOURCE.get(ident)),
                 "emblem": dict(emblemId=ident.lower(), artId=f"emblem_{ident.lower()}")}[kind]
        man.data["ui"][name] = K.ui_entry({"glyph": "glyph", "questItem": "questItemIcon", "emblem": "emblem"}[kind],
                                          files[name], px, gate=r.gate, previews=previews[name], **extra)
    man.save()
    fails = [n for n, v in out.items() if not v[2].gate["pass"]]
    print(f"[glyphs] shipped {len(out)} textures; gate fails: {fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]))
