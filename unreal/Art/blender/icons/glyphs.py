"""UI glyphs, quest-item icons and story emblems (spec §9.3–§9.5, DECISIONS R10 / R11).

* ``T_UI_Glyph_<id>`` (15, web ``UiGlyphs.ts``): gold, str, dex, vit, int, spi, lck, atk, def, hp, mp,
  quest_available, quest_complete, close, sort — 128² (web 64²; shown at 14–32 px), ink 3.2 u, rim 1.4 u
  (quest markers 3.6 u: the same ``SM_UI_QuestMarker_*`` meshes the NPCs wear, gold, with the web's glow);
  coins are the shipped ``SM_Pickup_GoldCoin``, the sword / shield the shipped ``SM_Weapon_*``.
* ``T_UI_QuestItem_<kind>`` (Chapter 1: herb, gel, pendant; web ``QuestItemIcons.ts``) — 256², rendered from the
  ground meshes ``SM_Pickup_Quest_*``.
* ``T_UI_Emblem_Villain`` / ``T_UI_Emblem_Generic`` (story ``say`` portraits without art, quests-story §8.5) —
  512²: the villain's violet eye in violet flame, the generic ember sigil in a gold ring.

Run:  EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/glyphs.py [--only id,…]
"""
from __future__ import annotations

import math
import sys
import time
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
from item_models import W, Wp, U  # noqa: E402
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
    F.weapon(c, "sword", "T2", 26, 72, math.pi / 4, 1.62, d=0, widen=1.7)
    c.style["ink_u"] = 2.4 * GLYPH_PX / 96 * 96 / GLYPH_PX


def g_def(c):
    F.weapon(c, "shield", "T4", 48, 50, 0.0, 1.38, d=0)


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
QUEST_ITEMS = {"herb": "SM_Pickup_Quest_Herb", "gel": "SM_Pickup_Quest_Gel", "pendant": "SM_Pickup_Quest_Pendant"}


def quest_item(kind: str):
    o = K.import_fbx("Pickups", QUEST_ITEMS[kind])
    acc = []
    if kind == "herb":
        K.place(o, toward=(0, 0, 1), up=(1, 0, 0), roll=-35, pitch=-12)
        acc = [sparkle(70, 26, 5)]
    elif kind == "gel":
        K.place(o, pitch=16)
        acc = [glow(48, 60, 34, "#6EE04A", .3, under=True), sparkle(36, 36, 5)]
    else:
        K.place(o, toward=(0, 0, 1), up=(0, 1, 0), roll=10, pitch=-8)
        acc = [glow(50, 52, 16, "#FF4050", .3), sparkle(64, 34, 5)]
    return [o], acc


# ── emblems ─────────────────────────────────────────────────────────────────────────────────────────────
def e_villain(c):
    for i, (col, s, e) in enumerate((("#4A1088", 1.0, .25), ("#6A1FB0", .82, .35), ("#9B4DFF", .64, .5))):
        for k in range(9):
            a = -math.pi / 2 + (k - 4) * .38
            x, y = 48 + math.cos(a) * 22 * s, 52 + math.sin(a) * 15 * s
            h = (30 - abs(k - 4) * 4.2) * s
            F.flame(c, x, y + 8, h, 6.5 * s, lean=math.cos(a) * .35, d=12 - i * 4, rot=0.0,
                    cols=(col, col, col), tag=f"vf{i}")
    ring = F.ring3(36, 36, 3, 3)
    ring.translate(at(48, 52, 14))
    c.add(ring, "rim", R("#9B4DFF", .3, .5, e=.5))
    disc = pill(S.circle2d(34, 48, 48, 52), depth=3, d=16, edge=.6)
    c.add(disc, "disc", R("#12061A", .3, .1))
    eye = S.chain2d(S.bez([(18, 52), (34, 36), (62, 36), (78, 52)], 14), S.bez([(78, 52), (62, 68), (34, 68), (18, 52)], 14))
    c.add(pill(eye, depth=6, d=-4, edge=.5, dome=.5), "eye", R("#FFD0FF", .3, .5, e=.45))
    iris = pill(S.circle2d(12, 32, 48, 52), depth=3, d=-9.5, edge=.6)
    c.add(iris, "iris", R("#9B4DFF", .3, .5, e=.6))
    c.add(pill(S.chain2d(S.bez([(48, 41), (52.5, 52), (48, 63)], 8), S.bez([(48, 63), (43.5, 52), (48, 41)], 8)), depth=1.5, d=-12.5,
               edge=.7), "pupil", R("#2A0036", .2, .05))
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
    objs = c.build()
    if fit:
        K.apply_frame(cam, K.fit(objs, fill=.84))
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
        K.clear_objects()
        objs, acc = quest_item(kind)
        K.apply_frame(cam, K.fit(objs, fill=.82))
        r = K.render_icon(accents=acc)
        out[f"T_UI_QuestItem_{kind}"] = ("questItem", kind, r, r.rgba)
        print(f"[glyphs] quest {kind:10s} gate {'ok ' if r.gate['pass'] else 'FAIL'} span {r.gate['span']:.2f}")
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
        return 0
    return ship(cam, out)


def ship(cam, out) -> int:
    files = {}
    for name, (kind, ident, r, img) in out.items():
        rel = {"glyph": f"Icons/Glyphs/{name}.png", "questItem": f"Icons/QuestItems/{name}.png",
               "emblem": f"Portraits/{name}.png"}[kind]
        K.write_icon(rel, img)
        files[name] = rel
    previews = []
    gl = [(k.replace("T_UI_Glyph_", ""), v[3]) for k, v in out.items() if v[0] == "glyph"]
    previews += K.save_preview("icons_glyphs", "glyphs_1x.png", K.atlas(gl, cols=8))
    previews += K.save_preview("icons_glyphs", "glyphs_half.png", K.atlas(gl, cols=8, scale=.5))
    previews += K.save_preview("icons_glyphs", "glyphs_ui.png", K.ui_sheet(gl, cols=8, sizes=(32, 20), qualities=("normal",)))
    qi = [(k.replace("T_UI_QuestItem_", ""), v[3]) for k, v in out.items() if v[0] == "questItem"]
    previews += K.save_preview("icons_quest", "quest_items_1x.png", K.atlas(qi, cols=3))
    previews += K.save_preview("icons_quest", "quest_items_half.png", K.atlas(qi, cols=3, scale=.5))
    previews += K.save_preview("icons_quest", "quest_items_ui.png", K.ui_sheet(qi, cols=3, qualities=("normal",)))
    em = [(k.replace("T_UI_Emblem_", ""), v[3]) for k, v in out.items() if v[0] == "emblem"]
    if em:
        previews += K.save_preview("portraits", "emblems_1x.png", K.atlas(em, cols=2))
        previews += K.save_preview("portraits", "emblems_ui.png", K.ui_sheet(em, cols=2, sizes=(64, 26), qualities=("normal",)))
    man = K.IconManifest()
    for name, (kind, ident, r, img) in out.items():
        px = img.shape[0]
        extra = {"glyph": dict(glyphId=ident, renderedAt=K.ICON_PX),
                 "questItem": dict(questItemKind=ident, source={"mesh": QUEST_ITEMS.get(ident)}),
                 "emblem": dict(emblemId=ident.lower(), artId=f"emblem_{ident.lower()}")}[kind]
        man.data["ui"][name] = K.ui_entry({"glyph": "glyph", "questItem": "questItemIcon", "emblem": "emblem"}[kind],
                                          files[name], px, gate=r.gate, previews=previews, **extra)
    man.save()
    fails = [n for n, v in out.items() if not v[2].gate["pass"]]
    print(f"[glyphs] shipped {len(out)} textures; gate fails: {fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]))
