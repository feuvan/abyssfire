"""Camp theme ``march``: Knellwatch's kit on the plains camp slots (dark remake A10; chapter1.md §2 "Hub: 丧钟堡 /
Knellwatch", §15 reserved light; palette ``Props_March``, A1 ``march/march_palettes.py``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/camp_march.py [names…]
        [--quick] [--sheet] [--family] [--no-previews]

| Asset | game id (themed) | look |
|---|---|---|
| ``SM_Palisade_March_Pew_A/B/C`` | ``palisade_plains@march`` (one per stake spot, random yaw) | the palisade the survivors nailed together, points out (R1 1.32 / 1.40 / 1.48 m incl. the point): A a chapel-pew end board ``#4A3426`` sharpened, worn edges ``#6E5642``, its seat-rail stub nailed across; B a charred stake ``#3A302A`` with a crusade shield ``#5A2220`` nailed on, iron rim, the crusade's **open palm** scorched into the field (env art review: the old cross-bar mark read as a Latin cross — the church is fictional, story-bible §12), a scorch streak and two scratches; C a timber stake ``#5A4A3C`` with a broken cart-wheel hub whose spokes were sharpened into spikes |
| ``SM_Palisade_March_GatePost`` | ``palisade_gate_plains@march`` | a squared post with iron bands, a crusade kettle helm ``#5E6166`` spiked on top and a second hung by its strap; the arm toward the camp hangs Knellwatch's black pennant with its pale bell |
| ``SM_Prop_March_BellFrame`` | ``camp_tent@march`` (the tent slot, 2 × 2) + ``setpiece_bell_frame`` (camp layout v2 places no tents: U4 places it as a set piece) | the bell frame: a squat timber belfry (four posts, braces, a charred plank roof) with the keepers' cracked bell in a dull dark bronze ``#64553C`` (lit L* 42; the lip flare ``#6E5A3A`` the one lit rim, verdigris in thin streaks ``#4E5A54``) hung low enough to touch, its rope, and the bell-frame lantern — one of the zone's four warm lights (``#FFB060``, socket ``lantern``) |
| ``SM_Prop_March_Hearth`` | ``camp_campfire@march`` (the respawn point, 1 × 1, not blocking) | the hearth: the only warm light in the open (``#FFB060`` core, embers ``#FF7A2A``), a ring of hearth stones, crossed logs (socket ``flame``) — inside ≈ 1.1 m and 0.3 m tall like the plains campfire |
| ``SM_Prop_March_HearthDressing`` | ``setpiece_hearth_dressing`` (U4: with every march hearth, same tile and yaw; blocks ``HEARTH_DRESSING_CELLS``) | the hearth's three log stools at 0.92 m and the blanket line drying behind it (env art review: on the 1 × 1 respawn tile they made the hearth 1.95 × 1.82 m and heroes respawned inside the stools) |
| ``SM_Prop_March_KnellwatchBanner`` | ``camp_banner@march`` | Knellwatch's banner: a black swallow-tail ``#1E1C20`` with a pale bell ``#B8B0A0``, an iron finial (cloth sways: ``MI_AF_Toon_Props_March_Wind``) |
| ``SM_Prop_March_SupplyCrate`` | ``camp_crate@march`` | crusade supply crates: a battened crate ``#4E4032`` stencilled with the crusade's open palm ``#6B2420`` (the old ring-and-sword read as a Greek Φ) and chalk tallies ``#D8D2C4``, a smaller crate on top |
| ``SM_Prop_March_RationSacks`` | ``camp_barrel@march`` | ration sacks ``#5E5546`` heaped and tied, counted in chalk laid on the cloth (env art review: chalk floating off the lumpy sack padded the hull into a flat black slab seen from behind) |
| ``SM_Prop_March_Well`` | ``camp_well@march`` | the plains well rebuilt in grey stone ``#5E5C58`` under a charred plank roof, black water with an oily sheen, two buckets, a ladle chained to the post |
| ``SM_Prop_March_Brazier`` | ``camp_torch@march`` | a smouldering iron brazier on three legs: coals ``#6A2A1A`` (emissive .25, below the ember band — **no warm light**: chapter1.md §15), soot and ash (socket ``flame``: ``camp.torch@march``, the smoulder recipe — smoke, dull sparks, no light; blocked on U1 resolving themed recipe ids) |

Footprints and blocking are the plains slots' (W5 / W11, ``CampLayout.h``): the bell frame records the tent's 2 × 2 body;
the hearth, banners, braziers and gate posts do not block; crates, sacks and the well do. Ink: decor 1.4 px (``Props_March``,
R14). Reserved light (``envkit_march.check_light``): ember / warm emissives only on the hearth and the bell-frame lantern.
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit_march as EM  # noqa: E402
import envkit as E  # noqa: E402
import camp as C  # noqa: E402
import foliage as F  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"
FAM = EM.PROPS


def reg(name: str) -> Region:
    return EM.region(FAM, name)


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


# The kit's regions: A1's swatches by role (``camp.*``, ``bell.*``, ``banner.*``, ``hearth.*``, ``lantern.*``,
# ``brazier.*``), plus the few this generator appends (thin parts' darker outline mix, like the plains poles; line
# tones of the stones and planks).
CAMP = dict(
    charred=reg("camp.charred"), charred_line=reg("camp.charred_line"), timber=reg("camp.timber"),
    timber_end=reg("camp.timber_end"), timber_line=reg("camp.timber_line"), pew=reg("camp.pew"),
    pew_worn=reg("camp.pew_worn"), shield=reg("camp.shield"), shield_rim=reg("camp.shield_rim"),
    helmet=reg("camp.helmet"), iron=reg("camp.iron"), iron_dark=reg("camp.iron_dark"), nail=reg("camp.nail"),
    rope=reg("camp.rope"), canvas=reg("camp.canvas"), canvas_dark=reg("camp.canvas_dark"),
    canvas_line=reg("camp.canvas_line"), sack=reg("camp.sack"), chalk=reg("camp.chalk"), crate=reg("camp.crate"),
    stencil=reg("camp.crate_stencil"), stone=reg("camp.stone"), stone_top=reg("camp.stone_top"),
    stone_line=reg("camp.stone_line"), ash=reg("camp.ash"), soot=reg("camp.soot"), water=reg("camp.water"),
    water_sheen=reg("camp.water_sheen"), bronze=reg("bell.bronze"), bronze_dark=reg("bell.bronze_dark"),
    patina=reg("bell.patina"), crack=reg("bell.crack"), banner=reg("banner.black"), banner_bell=reg("banner.bell"),
    banner_trim=reg("banner.trim"), ember=reg("hearth.ember"), ember_hot=reg("hearth.ember_hot"),
    ember_dim=reg("hearth.ember_dim"), log=reg("hearth.log"), log_end=reg("hearth.log_end"),
    hearth_stone=reg("hearth.stone"), lantern=reg("lantern.frame"), lantern_glow=reg("lantern.glow"),
    coals=reg("brazier.coals"), wood=reg("wagon.wood"), wood_dark=reg("wagon.wood_dark"),
    dark=reg("ground.dark"), light=reg("ground.light"),
    # appended: thin poles / bars keep a darker outline mix over the lit edge (plains camp.py: o .3-.6)
    pole=Region("#5A4A3C", .48, .24, o=.55), bar=Region("#5A4A3C", .48, .18, o=.3),
    # roofs of charred planks: A1's camp.charred lit band (.32) turns a sky-facing roof tan at the W1 pitch
    charred_deep=Region("#3A302A", .50, .14),
    plank_line=Region("#4E4032").shade_region(.3, .12), log_line=Region("#4A3424").line_region(),
    # env art review (march): A1's bell.bronze (decor_metal, light .40) lit the keepers' bell a sandy L* 59-63, the
    # brightest warm-neutral mass in the camp beside the reserved lantern; its verdigris blotches read as camouflage.
    # The bell body is a dull dark bronze (lit L* 42), the lip keeps bell.bronze as the one lit rim, verdigris runs
    # in thin grey-green streaks (also story_march's split Ashwheat bell)
    bronze_dull=Region("#64553C", .48, .08), patina_streak=Region("#4E5A54", .46, .12),
)


def new(pal: Palette, name: str):
    b = M.Builder(name, pal, prefix="camp")
    b.regions(**CAMP)
    return b, scene.rng(name)


def palm_sigil(b: M.Builder, at, size: float, region: str, cover: int | None, lift: float = 0.0012,
               thick: float = 0.002, name: str = "sigil") -> None:
    """The Church of the Kept Flame's sigil reduced to what reads at the W1 camera: an **open palm, fingers up**
    (story-bible §3 / §4: the crusade bears the open palm; the church is fictional — never a cross, story-bible §12).
    ``at(u, v) -> (point, normal)`` maps sigil coordinates (u across, v up, metres; wrist centre at (0, 0)) onto the
    surface; ``size`` = wrist to the middle fingertip. A solid palm, four fingers fanning so their tips part by
    ≥ 0.07 × size (≥ 1.3 px at 1080p for size ≥ 0.15 m), the thumb out to the side. Flat strips, no hull
    (``covered_by`` the surface's padded proxy)."""
    s = size

    def strip(path, widths):
        pts, nrm = zip(*(at(u, v) for u, v in path))
        b.add(E.ribbon(list(pts), list(nrm), [w * s for w in widths], lift=lift, thick=thick), region, name=name,
              outline=False, covered_by=cover)
    strip([(0.0, 0.0), (0.0, 0.2 * s), (0.0, 0.44 * s)], [0.30, 0.40, 0.42])            # the palm
    for du0, du1, ln in ((-0.135, -0.22, 0.40), (-0.045, -0.075, 0.52), (0.045, 0.075, 0.55), (0.135, 0.215, 0.44)):
        v0 = 0.40 * s
        strip([(du0 * s, v0), ((du0 + du1) / 2 * s, v0 + ln * 0.5 * s), (du1 * s, v0 + ln * s)], [0.085, 0.078, 0.06])
    strip([(0.16 * s, 0.12 * s), (0.26 * s, 0.22 * s), (0.33 * s, 0.33 * s)], [0.095, 0.085, 0.065])   # the thumb


def nail(b: M.Builder, at: Vector, normal: Vector, pid: int | None = None, r: float = 0.014):
    """A square nail head flush on a face (no hull: covered by the board's padded proxy)."""
    n = Vector(normal).normalized()
    p = M.cylinder(r, r, 0.0, 0.006, sides=4)
    p.transform(Vector((0, 0, 1)).rotation_difference(n).to_matrix().to_4x4())
    p.translate(at)
    b.add(p, "nail", name="nail", outline=False, covered_by=pid)


# ── the pew palisade ────────────────────────────────────────────────────────────────────────────────────
STAKE_H = C.STAKE_H          # 1.32 / 1.40 / 1.48 m incl. the point (R1 mean 1.4 m)


def pew_board(b: M.Builder, h: float, rnd: random.Random) -> int:
    """A chapel-pew end board set upright and sharpened: shouldered top (the pew end's scroll), cut to a point;
    the seat-rail stub still nailed across it; worn edges on the lit side."""
    w, t = 0.24, 0.05
    tip = 0.2
    sh = h - tip
    out = [(-w / 2, -0.04), (w / 2, -0.04), (w / 2, sh - 0.06), (w / 2 - 0.035, sh), (0.03, h), (-0.02, h - 0.01),
           (-w / 2 + 0.03, sh + 0.02), (-w / 2, sh - 0.03)]
    p = M.plate(out, t, bevel=0.008)
    p.transform(_rx(90))
    pid = b.add(p, "pew", name="board", hull_pad=True)
    # worn lit edge strips on the board's upper-left (the key light's side)
    for s in (-1, 1):
        b.add(E.ribbon([Vector((-w / 2 - 0.001, s * (t / 2 - 0.006), z)) for z in (0.25, 0.7, sh - 0.1)],
                       [Vector((-1, 0, 0))] * 3, 0.012, lift=0.0005, thick=0.002), "pew_worn", name="worn",
              outline=False, covered_by=pid)
    # carved quatrefoil-ish recess near the top (the pew end's ornament), a dark inset on both faces
    for s in (-1, 1):
        c = Vector((0.0, s * (t / 2 + 0.0008), sh - 0.2))
        ring = [c + Vector((math.cos(a) * 0.045, 0, math.sin(a) * 0.045)) for a in
                (math.tau * i / 10 for i in range(11))]
        b.add(E.ribbon(ring, [Vector((0, s, 0))] * len(ring), 0.012, lift=0.0, thick=0.0015), "timber_line",
              name="carve", outline=False, covered_by=pid)
    # the seat-rail stub nailed across at seat height
    rail = M.box((0.42, 0.06, 0.05), (0.06, -t / 2 - 0.032, 0.48), bevel=0.008)
    rail.transform(Matrix.Translation(Vector((0.06, -t / 2 - 0.032, 0.48))) @ _ry(rnd.uniform(-8, 8))
                   @ Matrix.Translation(-Vector((0.06, -t / 2 - 0.032, 0.48))))
    rid = b.add(rail, "pew", name="rail", hull_pad=True)
    for x in (-0.06, 0.06):
        nail(b, Vector((x, -t / 2 - 0.063, 0.48)), (0, -1, 0), rid)
    return pid


def shield_stake(b: M.Builder, h: float, rnd: random.Random) -> int:
    """A charred sharpened stake with a crusade shield nailed on it (heater, point down, faded field, iron rim)."""
    w = 0.13
    sp = E.spike(w, h - w * C.STAKE_TIP, w * C.STAKE_TIP, seed=("shieldstake", h, rnd.random()), square=True,
                 rows=12)
    pid = b.add(sp, "charred", name="stake", hull_pad=True)
    sw, shh = 0.3, 0.4
    out = [(-sw / 2, shh * 0.5), (sw / 2, shh * 0.5), (sw / 2, shh * 0.05), (sw * 0.32, -shh * 0.28),
           (0.0, -shh * 0.5), (-sw * 0.32, -shh * 0.28), (-sw / 2, shh * 0.05)]
    sh = M.plate(out, 0.022, bevel=0.004)
    for v in sh.bm.verts:                        # gently domed (a heater bulges forward)
        v.co.z += 0.03 * (1 - (v.co.x / (sw / 2)) ** 2)
    zc = h * 0.5
    sh.transform(Matrix.Translation(Vector((0, -w / 2 - 0.02, zc))) @ _rx(90))
    sid = b.add(sh, "shield", name="shield", hull_pad=True)

    def front(x: float, lift: float = 0.0015) -> float:
        """y of the shield's domed front face at x (+ lift)."""
        return -w / 2 - 0.02 - 0.011 - 0.03 * (1 - (x / (sw / 2)) ** 2) - lift
    rim = [(-sw / 2, shh * 0.5), (-sw / 2, shh * 0.05), (-sw * 0.32, -shh * 0.28), (0.0, -shh * 0.5),
           (sw * 0.32, -shh * 0.28), (sw / 2, shh * 0.05), (sw / 2, shh * 0.5)]
    pts = [Vector((x * 0.94, front(x * 0.94), zc + y * 0.94)) for x, y in rim]
    b.add(E.ribbon(pts, [Vector((0, -1, 0))] * len(pts), 0.022, lift=0.0, thick=0.003), "shield_rim", name="rim",
          outline=False, covered_by=sid)
    # the crusade's open palm scorched into the field (burnt dark; env art review: the old cross-bar mark read as a
    # Latin cross at 1080p — the church is fictional, story-bible §12), 19 cm wrist to fingertip, a little above
    # the middle; a scorch streak and two scratches on the field
    palm_sigil(b, lambda u, v: (Vector((u, front(u, 0.0), zc - 0.085 + v)), Vector((0, -1, 0))), 0.19,
               "charred_line", sid)
    b.add(E.ribbon([Vector((x, front(x), z)) for x, z in ((-0.11, zc + 0.0), (-0.095, zc - 0.07), (-0.07, zc - 0.12))],
                   [Vector((0, -1, 0))] * 3, [0.02, 0.028, 0.01], lift=0.0012, thick=0.002), "soot", name="scorch",
          outline=False, covered_by=sid)
    for (x0, z0), (x1, z1) in (((0.06, zc - 0.03), (0.095, zc - 0.09)), ((0.08, zc + 0.0), (0.11, zc - 0.05))):
        b.add(E.ribbon([Vector((x0, front(x0), z0)), Vector(((x0 + x1) / 2, front((x0 + x1) / 2), (z0 + z1) / 2)),
                        Vector((x1, front(x1), z1))], [Vector((0, -1, 0))] * 3, 0.006, lift=0.0012, thick=0.002),
              "shield_rim", name="scratch", outline=False, covered_by=sid)
    for x, z in ((-0.09, zc + 0.15), (0.09, zc + 0.15)):
        nail(b, Vector((x, front(x, 0.0), z)), (0, -1, 0), sid)
    return pid


def wheel_stake(b: M.Builder, h: float, rnd: random.Random) -> int:
    """A timber stake with a broken cart-wheel hub nailed to it, its spokes sawn short and sharpened (points
    out); two rope lashings."""
    w = 0.14
    sp = E.spike(w, h - w * C.STAKE_TIP, w * C.STAKE_TIP, seed=("wheelstake", h, rnd.random()), square=True,
                 rows=12)
    pid = b.add(sp, "timber", name="stake", hull_pad=True)
    zc = h * 0.58
    hub_c = Vector((0, -w / 2 - 0.05, zc))
    hub = M.cylinder(0.075, 0.075, -0.05, 0.05, sides=10)
    hub.transform(Matrix.Translation(hub_c) @ _rx(90))
    hid = b.add(hub, "wood_dark", name="hub", hull_pad=True)
    band = M.cylinder(0.08, 0.08, -0.015, 0.015, sides=10)
    band.transform(Matrix.Translation(hub_c + Vector((0, -0.03, 0))) @ _rx(90))
    b.add(band, "iron", name="hubband", outline=False, covered_by=hid)
    a0 = rnd.uniform(0, 0.6)
    for i in range(5):
        a = a0 + math.tau * i / 5
        d = Vector((math.cos(a), 0, math.sin(a)))
        L = rnd.uniform(0.17, 0.24)
        sp_ = M.tube([hub_c + d * 0.05, hub_c + d * (L * 0.7), hub_c + d * L], [0.02, 0.018, 0.003], sides=5,
                     dome0=0.0, dome1=0.0)
        sp_.recalc_normals()
        b.add(sp_, "wood", name="spoke")
    for z in (0.3, 0.86):
        hw = w / 2 + 0.001
        bnd = E.rope_band((hw, hw), h * z, height=0.05, thick=0.006, sides=16, exp=12.0)
        b.add(bnd, "rope", name="band", outline=False, covered_by=pid)
    return pid


def build_pew(pal: Palette, variant: str):
    name = f"SM_Palisade_March_Pew_{variant}"
    b, rnd = new(pal, name)
    h = STAKE_H[variant]
    {"A": pew_board, "B": shield_stake, "C": wheel_stake}[variant](b, h, rnd)
    return E.finish(b, pal, "decor", ground_h=1.2, name=name)


def helm(b: M.Builder, c: Vector, r: float, rot: Matrix, region: str = "helmet", name: str = "helm") -> int:
    """A crusade kettle helm: a rounded crown on a wide brim (lathe), dented."""
    p = M.lathe([(r * 1.55, 0.0), (r * 1.5, 0.018), (r * 1.0, 0.024), (r * 0.98, r * 0.55), (r * 0.78, r * 0.95),
                 (r * 0.3, r * 1.18), (0.01, r * 1.22)], sides=12, cap0="fan", cap1="fan")
    p.transform(Matrix.Translation(c) @ rot)
    return b.add(p, region, name=name, hull_pad=True)


def build_gatepost(pal: Palette):
    """The gate post (half-tile spots each side of the 2-tile gate, not blocking): a squared post with iron bands, a
    kettle helm spiked on top, a second one hung by its strap; the arm toward the camp (−Y) hangs Knellwatch's
    black pennant with its pale bell."""
    name = "SM_Palisade_March_GatePost"
    b, rnd = new(pal, name)
    h, w = 1.78, 0.22
    sp = E.spike(w, h, 0.0001, seed=name, square=True, taper=0.97, rows=18)
    pid = b.add(sp, "timber", name="post", hull_pad=True)
    for f in (0.24, 0.52, 0.8):
        band = E.rope_band((w / 2 * 1.04, w / 2 * 1.04), h * f, height=0.06, thick=0.012, sides=16, exp=8.0)
        b.add(band, "iron", name="band", outline=False, covered_by=pid)
    # helm spiked on the post top, tilted a little
    helm(b, Vector((0, 0, h - 0.06)), 0.13, _rx(rnd.uniform(-6, 6)) @ _ry(rnd.uniform(-8, 8)), name="helm_top")
    # a second helm hung by its chin strap from a nail on the outer face (+Y, away from the camp)
    hc = Vector((0.0, w / 2 + 0.12, h * 0.62))
    helm(b, hc, 0.11, _rx(80) @ _rz(rnd.uniform(-20, 20)), name="helm_hung")
    b.add(M.tube([Vector((0, w / 2 + 0.005, h * 0.7)), hc + Vector((0, -0.04, 0.1))], [0.008, 0.008], sides=4),
          "rope", name="strap", outline=False)
    # arm + pennant toward the camp
    arm = M.box((0.08, 0.36, 0.07), (0, -0.22, h - 0.26), bevel=0.012)
    b.add(arm, "bar", name="arm")
    pts = [(-0.11, 0.0), (0.11, 0.0), (0.11, -0.46), (0.0, -0.36), (-0.11, -0.46)]
    pen = M.plate([(x, y) for x, y in reversed(pts)], 0.012, bevel=0.003)
    pen.transform(_rx(90)).transform(_rz(90)).translate((0, -0.34, h - 0.3))
    penid = b.add(pen, "banner", name="pennant", hull_pad=True)
    bell_emblem(b, Vector((0.0075, -0.34, h - 0.52)), 0.07, Vector((1, 0, 0)), penid)
    bell_emblem(b, Vector((-0.0075, -0.34, h - 0.52)), 0.07, Vector((-1, 0, 0)), penid)
    for q in range(3):
        C.grain(b, "timber_line", (w / 2 * 1.01, rnd.uniform(-0.05, 0.05), 0.2 + q * 0.45),
                (w / 2 * 1.01, rnd.uniform(-0.05, 0.05), 0.42 + q * 0.45), (1, 0, 0), 0.016, pid)
    return E.finish(b, pal, "decor", ground_h=1.2, name=name)


def bell_emblem(b: M.Builder, c: Vector, s: float, normal: Vector, cover: int | None):
    """Knellwatch's pale bell, a flat silhouette lying on a cloth face (``normal`` = the face's outward normal)."""
    n = Vector(normal).normalized()
    ref = Vector((0, 0, 1))
    side = ref.cross(n).normalized()
    outline = [(-0.12, -0.5), (0.12, -0.5), (0.1, -0.42), (0.5, -0.36), (0.42, -0.28), (0.36, 0.2), (0.24, 0.42),
               (0.08, 0.5), (-0.08, 0.5), (-0.24, 0.42), (-0.36, 0.2), (-0.42, -0.28), (-0.5, -0.36), (-0.1, -0.42)]
    bm = bmesh.new()
    vs = [bm.verts.new(c + side * (x * s) + ref * (y * s * 1.1)) for x, y in outline]   # mouth down, crown up
    f = bm.faces.new(vs)
    bm.normal_update()
    if f.normal.dot(n) < 0:
        f.normal_flip()
    bmesh.ops.triangulate(bm, faces=[f], ngon_method="EAR_CLIP")
    part = M.Part(bm).translate(n * 0.0015)
    b.add(part, "banner_bell", name="emblem", outline=False, covered_by=cover)


# ── banner ──────────────────────────────────────────────────────────────────────────────────────────────
def build_banner(pal: Palette):
    """Knellwatch's banner (the plains banner's pole, bar and swallow-tail): a black field with a pale bell on both
    faces, a dark trim band at the bar, an iron spike finial; the cloth sways from the bar (``envkit.cloth_wind``)."""
    name = "SM_Prop_March_KnellwatchBanner"
    b, rnd = new(pal, name)
    H = 2.35
    pole = M.tube([(0, 0, -0.05), (0, 0, H * 0.5), (0, 0, H)], [0.05, 0.045, 0.04], sides=8)
    pid = b.add(pole, "pole", name="pole", hull_pad=True)
    fin = M.gem(0.05, 0.24, 0.06, sides=4).translate((0, 0, H + 0.04))
    b.add(fin, "iron", name="finial")
    bar = M.tube([(-0.07, 0, H - 0.17), (0.6, 0, H - 0.17)], [0.036, 0.036], sides=8)
    b.add(bar, "bar", name="bar")
    w0, L = 0.5, 1.15
    pts = [(0.0, 0.0), (w0, 0.0), (w0 + 0.03, -L), (w0 / 2, -L + 0.24), (0.0, -L)]
    cloth = M.plate(list(reversed(pts)), 0.02, bevel=0.004)

    def bulge(x, y):
        return (0.04 * math.sin(math.pi * min(max(x / w0, 0.0), 1.0)) +
                0.03 * math.sin(x * 11 + 1.0) * max(0.0, (-y - 0.8) / 0.35))
    for v in cloth.bm.verts:
        v.co.z += bulge(v.co.x, v.co.y)
    cloth.transform(_rx(90)).translate((0.03, 0.0, H - 0.2))
    cid = b.add(cloth, "banner", name="cloth", hull_pad=True)
    sway = {cid}
    trim = M.box((w0 - 0.02, 0.03, 0.06), (0.03 + w0 / 2, 0.0, H - 0.24), bevel=0.008)
    sway.add(b.add(trim, "banner_trim", name="trim", outline=False, covered_by=cid))
    cx, cz = 0.03 + w0 / 2, H - 0.2 - 0.46
    off = -bulge(w0 / 2, -0.48)
    for side in (-1, 1):
        n = Vector((0, side, 0))
        bell_emblem(b, Vector((cx, off + side * 0.0105, cz)), 0.3, n, cid)
        sway.add(len(b.parts) - 1)
    for i in range(3):
        a = math.tau * i / 3 + 0.4
        shape = E.RockShape((0.09, 0.08, 0.07), (name, i), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=26.0).translate((math.cos(a) * 0.1, math.sin(a) * 0.1, 0)), "stone",
              name="wedge", hull_pad=True)
    hull_ids = {k for k, p in enumerate(b.parts) if p.get("hullOnly") and p["name"].startswith("cloth")}
    wind = E.cloth_wind({p: (H - 0.2, L, 0.15) for p in sway | hull_ids})
    return E.finish(b, pal, "decor", ground_h=1.0, wind=wind, name=name)


# ── brazier ─────────────────────────────────────────────────────────────────────────────────────────────
def build_brazier(pal: Palette):
    """A smouldering iron brazier (the plains torch slot): a deep basket of iron straps on three splayed legs, the
    coals gone mostly to soot and ash with a few dull red specks (``brazier.coals`` #6A2A1A, e .25, hue 12°: below
    the ember band). No warm light (chapter1.md §15): the runtime recipe at socket ``flame`` must be the smoulder
    (smoke, a few dull sparks)."""
    name = "SM_Prop_March_Brazier"
    b, rnd = new(pal, name)
    top = 0.9
    for i in range(3):
        a = math.tau * i / 3 + 0.3
        d = Vector((math.cos(a), math.sin(a), 0))
        leg = M.tube([d * 0.34 + Vector((0, 0, -0.02)), d * 0.24 + Vector((0, 0, 0.36)), d * 0.17 +
                      Vector((0, 0, top - 0.12))], [0.03, 0.027, 0.026], sides=6, dome0=0.0, dome1=0.0)
        leg.recalc_normals()
        b.add(leg, "iron_dark", name="leg", hull_pad=True)
        foot = M.box((0.08, 0.08, 0.035), d * 0.35, bevel=0.01)
        b.add(foot, "iron_dark", name="foot")
    ring = M.belt(0.21, 0.21, 0.34, 0.045, 0.014, sides=12, exp=2.0)
    b.add(ring, "iron_dark", name="brace")
    # the basket: a deep iron bowl flaring out, ringed by straps
    bowl = M.loft_z([(top - 0.3, 0.07, 0.07), (top - 0.2, 0.19, 0.19), (top - 0.05, 0.27, 0.27),
                     (top + 0.04, 0.3, 0.3)], sides=14, cap0="ngon", cap1=None)
    bid = b.add(bowl, "iron", name="bowl", hull_pad=True)
    for i in range(7):
        a = math.tau * (i + 0.5) / 7
        pts, nrm = [], []
        for z, r in ((top - 0.27, 0.1), (top - 0.17, 0.21), (top - 0.04, 0.28), (top + 0.03, 0.3)):
            pts.append(Vector((math.cos(a) * (r + 0.003), math.sin(a) * (r + 0.003), z)))
            nrm.append(Vector((math.cos(a), math.sin(a), 0.4)).normalized())
        b.add(E.ribbon(pts, nrm, 0.03, lift=0.001, thick=0.003), "iron_dark", name="strap", outline=False,
              covered_by=bid)
    rim = M.belt(0.295, 0.295, top + 0.02, 0.04, 0.016, sides=14, exp=2.0)
    b.add(rim, "iron_dark", name="rim")
    # iron prongs round the rim, curling out (the brazier's crown: a silhouette no stool has)
    for i in range(7):
        a = math.tau * (i + 0.5) / 7
        d = Vector((math.cos(a), math.sin(a), 0))
        p0 = d * 0.29 + Vector((0, 0, top + 0.03))
        pr = M.tube([p0, p0 + d * 0.04 + Vector((0, 0, 0.12)), p0 + d * 0.1 + Vector((0, 0, 0.19))],
                    [0.014, 0.011, 0.004], sides=4, dome0=0.0, dome1=0.0)
        pr.recalc_normals()
        b.add(pr, "iron_dark", name="prong")
    # the bed: soot heaped in the bowl, pale ash on its crown, a few dull red specks still alive
    bed = M.loft_z([(top - 0.05, 0.27, 0.27), (top + 0.02, 0.26, 0.26), (top + 0.06, 0.16, 0.16),
                    (top + 0.075, 0.04, 0.04)], sides=12, cap0="ngon", cap1="fan")
    b.add(bed, "soot", name="bed", outline=False)
    for q in range(9):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0.02, 0.2)
        s_ = rnd.uniform(0.035, 0.06)
        z = top + 0.075 - rr * 0.22
        b.add(M.ellipsoid((s_, s_ * 0.85, s_ * 0.55), (math.cos(a) * rr, math.sin(a) * rr, z), sides=6, rings=3),
              "ash" if q % 3 == 0 else "soot", name="lump", outline=False)
    for q in range(5):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0.03, 0.17)
        z = top + 0.078 - rr * 0.22
        b.add(M.ellipsoid((0.016, 0.014, 0.01), (math.cos(a) * rr, math.sin(a) * rr, z), sides=5, rings=3),
              "coals", name="speck", outline=False)
    return E.finish(b, pal, "decor", ground_h=0.9, name=name, ground_band=0.02)


# ── the bell frame (tent slot) ──────────────────────────────────────────────────────────────────────────
BELL_LIP_Z = 0.86          # the lip hangs at hip height: low enough to touch (chapter1.md §2)
LANTERN = Vector((-0.86, -0.78, 1.66))     # the bell-frame lantern (socket ``lantern``: the warm light #FFB060)


BELL_PROFILE = [(0.4, 0.0), (0.42, 0.045), (0.37, 0.11), (0.29, 0.3), (0.26, 0.5), (0.245, 0.63), (0.2, 0.73),
                (0.1, 0.785), (0.01, 0.8)]


def bell_radius(z: float, k: float = 1.0, prof=BELL_PROFILE) -> float:
    """Outer radius of the bell profile (× ``k``) at height ``z`` (m above the lip)."""
    for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
        if z0 * k <= z <= z1 * k:
            return (r0 + (r1 - r0) * (z - z0 * k) / max((z1 - z0) * k, 1e-6)) * k
    return prof[-1][0] * k


def patina_streaks(b: M.Builder, rnd: random.Random, k: float, xf: Matrix, cover: int, azimuths, region: str,
                   z_top: float = 0.62, z_low=(0.16, 0.34), prof=BELL_PROFILE, facet: float = math.tau / 16,
                   phase: float = 0.0, min_z: float | None = None) -> None:
    """Verdigris run down a bronze bell in thin streaks (env art review: square blotches read as camouflage at
    1080p): from under the shoulder down toward the lip, 1.2-1.8 cm wide, tapering; ``xf`` places the bell's local
    frame (lip at z 0, axis +Z); ``facet`` / ``phase`` = the shell's angular facet size and first vertex azimuth (the
    strips lie on the facets); streaks reaching below world ``min_z`` (sunk in the mud) are skipped."""
    rot = xf.to_3x3()
    for a in azimuths:
        z0, z1 = z_top * k, rnd.uniform(*z_low) * k
        w = rnd.uniform(0.012, 0.018)
        pts, nrm = [], []
        for i in range(4):
            z = z0 + (z1 - z0) * i / 3
            aa = a + 0.03 * math.sin(i * 2.1 + a)
            # on the lathe's facets (inscribed radius at this azimuth), not its vertex circle: a strip on the circle
            # stood up to 6 mm off a 16-sided bell and padded its hull 17 mm
            r = bell_radius(z, k, prof) * math.cos(facet / 2) / math.cos(((aa - phase) % facet) - facet / 2) + 0.001
            pts.append(xf @ Vector((math.cos(aa) * r, math.sin(aa) * r, z)))
            nrm.append((rot @ Vector((math.cos(aa), math.sin(aa), 0.25))).normalized())
        if min_z is not None and min(q.z for q in pts) < min_z:
            continue
        b.add(E.ribbon(pts, nrm, [w * 0.7, w, w * 0.8, w * 0.35], lift=0.0, thick=0.0015), region, name="patina",
              outline=False, covered_by=cover)


def bell(b: M.Builder, c: Vector, rnd: random.Random, k: float = 1.0) -> int:
    """The keepers' bronze bell: a lathe (lip flare, waist, shoulder, crown), the mouth a dark recessed cap, the crack
    running up from the lip on the front (−Y). Env art review: a dull dark body (``bronze_dull``, lit L* 42) with
    the lip flare as its one lit rim (A1's ``bell.bronze``), verdigris in thin streaks (``patina_streak``) — the
    sandy body and its teal blotches out-valued the reserved lantern beside it."""
    prof = BELL_PROFILE
    p = M.lathe([(r * k, z * k) for r, z in prof], sides=16, cap0="fan", cap1="fan")
    # recess the mouth: the cap's centre goes up into the bell
    bm = p.bm
    for v in bm.verts:
        if abs(v.co.z) < 1e-6 and v.co.xy.length < 1e-4:
            v.co.z = 0.28 * k
    p.recalc_normals()
    bm.normal_update()
    lay = bm.faces.layers.int.new("af_sub")
    for f in bm.faces:
        cc = f.calc_center_median()
        if f.normal.z < -0.25 and cc.z < 0.3 * k and cc.xy.length < 0.39 * k:
            f[lay] = 1                                   # the mouth's dark inside
        elif cc.z < 0.08 * k:
            f[lay] = 3                                   # the lip flare: the one lit rim
    p.translate(c)
    pid = b.add(p, "bronze_dull", name="bell", hull_pad=True, sub_regions={1: "bronze_dark", 3: "bronze"})
    a0 = rnd.uniform(0, 1.0)
    patina_streaks(b, rnd, k, Matrix.Translation(c), pid,
                   [a0 + math.tau * q / 7 + rnd.uniform(-0.15, 0.15) for q in range(7)], "patina_streak")
    # the crack: from the lip up the front, zigzag, on the surface
    pts, nrm = [], []
    for i, (zf, da) in enumerate(((0.02, 0.0), (0.12, 0.05), (0.22, -0.02), (0.33, 0.06), (0.42, 0.01))):
        z = zf * k
        r = None
        for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
            if z0 * k <= z <= z1 * k:
                r = (r0 + (r1 - r0) * (z - z0 * k) / max((z1 - z0) * k, 1e-6)) * k
                break
        a = -math.pi / 2 + 0.25 + da
        fa = math.tau / 16                               # on the facet (inscribed), not the vertex circle
        r = r * math.cos(fa / 2) / math.cos((a % fa) - fa / 2) + 0.001
        pts.append(c + Vector((math.cos(a) * r, math.sin(a) * r, z)))
        nrm.append(Vector((math.cos(a), math.sin(a), 0.2)).normalized())
    b.add(E.ribbon(pts, nrm, [0.022, 0.018, 0.014, 0.01, 0.006], lift=0.001, thick=0.002), "crack", name="crack",
          outline=False, covered_by=pid)
    return pid


def build_bell_frame(pal: Palette):
    """The bell frame in the tent slot (2 × 2 body, door side −Y toward the camp centre): four squared posts on
    stones, knee braces, a heavy cross-beam and a charred plank gable; the cracked bell on its wooden yoke hangs from
    two iron straps with its lip at 0.86 m, a rope from the yoke lever; the bell-frame lantern on an iron arm."""
    name = "SM_Prop_March_BellFrame"
    b, rnd = new(pal, name)
    X, Y, H = 0.86, 0.62, 2.3
    pw = 0.2
    pids = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            post = M.box((pw, pw, H + 0.05), (sx * X, sy * Y, (H + 0.05) / 2 - 0.05), bevel=0.02)
            pids.append(b.add(post, "timber", name="post", hull_pad=True))
            shape = E.RockShape((0.17, 0.15, 0.1), (name, "foot", sx, sy), bumps=4, amp=0.08, top_cut=0.3)
            part = E.rock(shape, subdiv=1, facet=26.0, yaw=rnd.uniform(0, 360))
            E.unflare_foot(part, 0.05)
            b.add(part.translate((sx * X + sx * 0.08, sy * Y + sy * 0.06, 0)), "stone", name="footstone",
                  hull_pad=True)
    for sy in (-1, 1):                                   # top plates along X over each pair
        plate_ = M.box((2 * X + 0.42, 0.22, 0.2), (0, sy * Y, H + 0.1), bevel=0.02)
        b.add(plate_, "timber", name="plate", hull_pad=True)
    beam = M.box((0.22, 2 * Y + 0.5, 0.24), (0, 0, H + 0.32), bevel=0.02)   # the bell beam across, on the plates
    b.add(beam, "timber", name="beam", hull_pad=True)
    for sx in (-1, 1):                                   # side rails low down (the frame's sills) + knee braces
        rail = M.box((0.14, 2 * Y, 0.14), (sx * X, 0, 0.42), bevel=0.015)
        b.add(rail, "timber", name="rail", hull_pad=True)
        for sy in (-1, 1):
            a = Vector((sx * X, sy * Y - sy * 0.08, H - 0.55))
            e = Vector((sx * X, sy * Y - sy * 0.55, H - 0.02))
            br = M.tube([a, a.lerp(e, 0.5), e], [0.055, 0.055, 0.055], sides=4, exp=12.0, dome0=0.0, dome1=0.0)
            br.recalc_normals()
            b.add(br, "timber", name="brace")
    # the gable roof: two slopes of charred planks along Y, ridge over the beam
    ridge_z, eave_z = H + 1.0, H + 0.42
    half = X + 0.36
    for s_ in (-1, 1):
        n = Vector((s_ * (ridge_z - eave_z), 0, half)).normalized()
        for q in range(5):
            y0 = -Y - 0.3 + q * (2 * Y + 0.6) / 5
            pl = M.box((half * 1.16, (2 * Y + 0.6) / 5 - 0.012, 0.05), (0, 0, 0), bevel=0.01)
            ang = math.degrees(math.atan2(ridge_z - eave_z, half))
            pl.transform(Matrix.Translation(Vector((s_ * half / 2, y0 + (2 * Y + 0.6) / 10,
                                                    (ridge_z + eave_z) / 2 + 0.03)))
                         @ _ry(s_ * ang) @ _rz(rnd.uniform(-1.5, 1.5)))
            b.add(pl, "charred_deep", name="roof", hull_pad=True)
    rb = M.box((0.16, 2 * Y + 0.7, 0.14), (0, 0, ridge_z + 0.04), bevel=0.02)
    b.add(rb, "charred", name="ridge", hull_pad=True)
    for s_ in (-1, 1):                                   # king posts from the beam to the ridge (gable ends)
        kp = M.box((0.12, 0.12, ridge_z - H - 0.44), (0, s_ * (Y + 0.1), (H + 0.44 + ridge_z) / 2), bevel=0.012)
        b.add(kp, "timber", name="kingpost", hull_pad=True)
    # yoke + straps + bell
    bell_c = Vector((0, 0, BELL_LIP_Z))
    bell(b, bell_c, rnd)
    yz = BELL_LIP_Z + 0.8 + 0.06
    yoke = M.box((0.2, 0.62, 0.18), (0, 0, yz + 0.07), bevel=0.02)
    b.add(yoke, "timber", name="yoke", hull_pad=True)
    for sy in (-1, 1):
        strap = M.box((0.05, 0.035, H + 0.2 - yz), (0, sy * 0.24, (H + 0.2 + yz) / 2), bevel=0.006)
        b.add(strap, "iron", name="strap")
    lever = M.box((0.5, 0.06, 0.06), (0.32, 0.0, yz + 0.1), bevel=0.01)
    b.add(lever, "timber", name="lever")
    rope = M.tube([Vector((0.55, 0.0, yz + 0.08)), Vector((0.57, -0.02, 1.2)), Vector((0.58, -0.03, 0.72))],
                  [0.016, 0.016, 0.016], sides=6, dome0=0.0, dome1=0.01)
    b.add(rope, "rope", name="rope")
    knot = M.ellipsoid((0.035, 0.035, 0.05), (0.58, -0.03, 0.7), sides=6, rings=4)
    b.add(knot, "rope", name="knot")
    # the lantern on an iron arm from the front-left post
    arm = M.tube([Vector((-X, -Y - pw / 2, LANTERN.z + 0.26)), Vector((-X, LANTERN.y, LANTERN.z + 0.26))],
                 [0.014, 0.014], sides=4, dome0=0.0, dome1=0.0)
    b.add(arm, "iron_dark", name="arm")
    hook = M.tube([Vector((-X, LANTERN.y, LANTERN.z + 0.26)), LANTERN + Vector((0, 0, 0.14))], [0.008, 0.008],
                  sides=4, dome0=0.0, dome1=0.0)
    b.add(hook, "iron_dark", name="hook", outline=False)
    cage = M.loft_z([(-0.1, 0.07, 0.07), (0.08, 0.075, 0.075), (0.12, 0.05, 0.05), (0.15, 0.02, 0.02)], sides=4,
                    exp=2.0, cap0="ngon", cap1="fan", phase=math.pi / 4)
    cage.translate(LANTERN)
    cid = b.add(cage, "lantern", name="lantern", hull_pad=True)
    for i in range(4):                                   # the warm panes between the frame's corner bars
        a = math.pi / 4 + math.tau * i / 4 + math.pi / 4
        n = Vector((math.cos(a), math.sin(a), 0))
        pane = M.box((0.075, 0.004, 0.13), (0, 0, 0), bevel=0.0)
        pane.transform(Matrix.Translation(LANTERN + n * 0.0545 + Vector((0, 0, -0.005)))
                       @ Matrix.Rotation(a - math.pi / 2, 4, "Z"))
        b.add(pane, "lantern_glow", name="glow", outline=False, covered_by=cid)
    return E.finish(b, pal, "decor", ground_h=1.6, name=name)


# ── the hearth (campfire slot) ──────────────────────────────────────────────────────────────────────────
def stool(b: M.Builder, at: Vector, rnd: random.Random, h: float = 0.36):
    """A log-round stool: a short sawn log on its end, the cut top lit."""
    r = rnd.uniform(0.13, 0.15)
    p = M.cylinder(r, r * 1.04, -0.01, h, sides=10)
    p.translate(at)
    pid = b.add(p, "log", name="stool", hull_pad=True)
    top = M.cylinder(r * 0.82, r * 0.82, h, h + 0.004, sides=10)
    top.translate(at)
    b.add(top, "log_end", name="stool_top", outline=False, covered_by=pid)
    return pid


def blanket(b: M.Builder, x0: float, x1: float, y: float, z: float, drop: float, region: str, rnd: random.Random):
    """A blanket hung over the line: a sheet folded over the rope, both halves hanging (two-sided plates)."""
    w = x1 - x0
    pids = []
    for s in (-1, 1):
        pts = [(x0, 0.0), (x1, 0.0), (x1 - 0.01, -drop * (1.0 if s < 0 else 0.7)),
               ((x0 + x1) / 2, -drop * (1.0 if s < 0 else 0.7) - 0.03), (x0 + 0.01, -drop * (0.95 if s < 0 else 0.68))]
        pl = M.plate(list(reversed(pts)), 0.012, bevel=0.003)
        for v in pl.bm.verts:
            v.co.z += 0.025 * math.sin((v.co.x - x0) / w * math.pi * 3 + rnd.uniform(0, 0.3))
        pl.transform(Matrix.Translation(Vector((0, y + s * 0.02, z))) @ _rx(90 + s * 8))
        pids.append(b.add(pl, region, name="blanket", hull_pad=True))
    return pids


HEARTH_R = 0.42            # the stone ring (centre radius): the whole hearth stays inside ≈ 1.1 m (the 1 × 1 tile)
DRESS_R = 0.92             # the stools' ring round the hearth (SM_Prop_March_HearthDressing)


def build_hearth(pal: Palette):
    """The hearth (the respawn point; the only warm light in the open, chapter1.md §15): a ring of hearth stones, an
    ash bed, a heap of coals (``hearth.ember_hot`` #FFB060 core, ``hearth.ember`` ring, dim edge), crossed logs with
    lit ends. Env art review: the stools and the blanket line made it 1.95 × 1.82 × 1.28 m on a 1 × 1 non-blocking
    respawn tile (heroes respawned inside the stools); the hearth now keeps inside ≈ 1.1 m and 0.3 m tall like the
    plains campfire (107 × 99 × 29 cm), the stools and the blanket line are ``SM_Prop_March_HearthDressing`` (a
    blocking set piece placed with it)."""
    name = "SM_Prop_March_Hearth"
    b, rnd = new(pal, name)
    k = HEARTH_R / 0.55
    bed = M.cylinder(0.42 * k, 0.39 * k, -0.02, 0.025, sides=18)
    b.add(bed, "ash", name="ash")
    glow = M.cylinder(0.26 * k, 0.22 * k, 0.0, 0.034, sides=16)
    b.add(glow, "ember_dim", name="embers_dim", outline=False)
    for rr, z1, reg_ in ((0.17, 0.042, "ember"), (0.08, 0.05, "ember_hot")):
        b.add(M.cylinder(rr * k, rr * k * 0.9, 0.03, z1, sides=12), reg_, name="coals", outline=False)
    for q in range(12):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0.02, 0.22) * k
        s_ = rnd.uniform(0.03, 0.05)
        b.add(M.ellipsoid((s_, s_ * 0.8, s_ * 0.5), (math.cos(a) * rr, math.sin(a) * rr, 0.04), sides=6, rings=3),
              "ember_hot" if rr < 0.1 * k else "ember", name="coal", outline=False)
    C.log(b, Vector((-0.4, -0.14, 0.05)) * k, Vector((0.3 * k, 0.12 * k, 0.22)), 0.065)
    C.log(b, Vector((0.4, -0.16, 0.05)) * k, Vector((-0.28 * k, 0.14 * k, 0.22)), 0.065)
    C.log(b, Vector((0.02 * k, 0.42 * k, 0.05)), Vector((-0.02 * k, -0.14 * k, 0.22)), 0.055, name="log3")
    for i in range(12):
        a = math.tau * i / 12 + rnd.uniform(-0.06, 0.06)
        c = Vector((math.cos(a) * HEARTH_R, math.sin(a) * HEARTH_R, 0))
        r = (rnd.uniform(0.09, 0.11), rnd.uniform(0.075, 0.088), rnd.uniform(0.075, 0.09))
        shape = E.RockShape(r, (name, i), bumps=4, amp=0.08, top_cut=0.3)
        part = E.rock(shape, subdiv=1, facet=26.0, yaw=math.degrees(a))
        E.unflare_foot(part, 0.045)
        b.add(part.translate(c), "hearth_stone", name="stone", hull_pad=True)
    return E.finish(b, pal, "decor", ground_h=0.3, name=name, ground_band=0.02)


def build_hearth_dressing(pal: Palette):
    """The hearth's dressing, split off the respawn tile (env art review): three log stools round the hearth at
    0.92 m and the blanket line drying behind it (+Y, the camera's far side) — two poles, a sagging rope, two
    blankets (cloth sways: ``MI_AF_Toon_Props_March_Wind``). Same pivot and yaw as ``SM_Prop_March_Hearth``; the
    centre ≈ 1.1 m stays clear (the hearth, the respawn point)."""
    name = "SM_Prop_March_HearthDressing"
    b, rnd = new(pal, name)
    for a in (math.radians(-35), math.radians(200), math.radians(85)):
        stool(b, Vector((math.cos(a) * DRESS_R, math.sin(a) * DRESS_R, 0)), rnd)
    y, hz = 1.0, 1.2
    for x in (-0.8, 0.8):
        pole = M.tube([(x, y, -0.04), (x, y, hz * 0.5), (x, y, hz + 0.05)], [0.035, 0.032, 0.03], sides=6)
        b.add(pole, "pole", name="linepole", hull_pad=True)
    line = M.tube([Vector((-0.8, y, hz)), Vector((-0.3, y, hz - 0.06)), Vector((0.3, y, hz - 0.06)),
                   Vector((0.8, y, hz))], [0.012] * 4, sides=5, dome0=0.0, dome1=0.0)
    b.add(line, "rope", name="line")
    cloth = blanket(b, -0.68, -0.06, y, hz - 0.05, 0.62, "canvas", rnd)
    cloth += blanket(b, 0.06, 0.62, y, hz - 0.055, 0.5, "canvas_dark", rnd)
    hull_ids = {k for k, p in enumerate(b.parts) if p.get("hullOnly") and p["name"].startswith("blanket")}
    wind = E.cloth_wind({p: (hz, 0.6, 0.3) for p in set(cloth) | hull_ids})
    return E.finish(b, pal, "decor", ground_h=0.6, name=name, ground_band=0.02, wind=wind)


# ── the well ────────────────────────────────────────────────────────────────────────────────────────────
def build_well(pal: Palette):
    """The plains well (``camp.build_well``: two coursed stone rings + cap stones, posts, windlass, gable) in grey
    stone under a charred plank roof; black water with the oily sheen; a bucket on the rope, a second on the curb's
    foot, a ladle chained to the post."""
    name = "SM_Prop_March_Well"
    b, rnd = new(pal, name)
    R0, R1, H = 0.56, 0.42, 0.58
    for course in range(2):
        n = 9
        for i in range(n):
            a0 = math.tau * (i + 0.5 * course) / n
            a1 = a0 + math.tau / n * 0.94
            z0, z1 = course * H / 2 - 0.02, (course + 1) * H / 2 - 0.01
            blk = C._block(R1, R0 + rnd.uniform(-0.01, 0.015), a0, a1, z0, z1)
            b.add(blk, "stone", name="block", hull_pad=True)
    for i in range(10):
        a0 = math.tau * i / 10
        a1 = a0 + math.tau / 10 * 0.95
        blk = C._block(R1 - 0.03, R0 + 0.04, a0, a1, H - 0.01, H + 0.09)
        b.add(blk, "stone_top", name="cap", hull_pad=True)
    water = M.cylinder(R1 + 0.01, R1 + 0.01, 0.0, H - 0.18, sides=18)
    b.add(water, "water", name="water", outline=False)
    sheen = M.ellipsoid((0.2, 0.06, 0.006), (0.06, -0.05, H - 0.175), sides=10, rings=3)
    b.add(sheen, "water_sheen", name="sheen", outline=False)
    for s in (-1, 1):
        post = M.box((0.11, 0.11, 1.45), (s * 0.5, 0.0, 0.62 + 0.72 - 0.08), bevel=0.015)
        b.add(post, "timber", name="post", hull_pad=True)
    drum = M.tube([(-0.16, 0, 1.2), (0.16, 0, 1.2)], [0.07, 0.07], sides=10)
    b.add(drum, "rope", name="drum")
    axle = M.tube([(-0.46, 0, 1.2), (0.46, 0, 1.2)], [0.025, 0.025], sides=6)
    b.add(axle, "timber", name="axle")
    crank = M.box((0.04, 0.04, 0.2), (0.6, 0, 1.12), bevel=0.008)
    b.add(crank, "timber", name="crank")
    rope = M.tube([(0.0, -0.06, 1.15), (0.0, -0.06, 0.82)], [0.012, 0.012], sides=6)
    b.add(rope, "rope", name="rope")
    bucket = M.loft_z([(0.62, 0.085, 0.085), (0.8, 0.11, 0.11)], sides=12, cap0="ngon", cap1="ngon")
    b.add(bucket.translate((0, -0.06, 0)), "wood", name="bucket")
    hoop = M.belt(0.105, 0.105, 0.74, 0.025, 0.01, sides=12)
    b.add(hoop.translate((0, -0.06, 0)), "iron", name="hoop", outline=False)
    # a second bucket on the ground at the curb, tipped a little
    b2 = M.loft_z([(0.0, 0.1, 0.1), (0.22, 0.125, 0.125)], sides=12, cap0="ngon", cap1="ngon")
    b2.transform(Matrix.Translation(Vector((-0.62, -0.42, 0.0))) @ _rx(6))
    b.add(b2, "wood", name="bucket2", hull_pad=True)
    h2 = M.belt(0.12, 0.12, 0.15, 0.025, 0.01, sides=12)
    h2.transform(Matrix.Translation(Vector((-0.62, -0.42, 0.0))) @ _rx(6))
    b.add(h2, "iron", name="hoop", outline=False)
    # the ladle chained to the right post: a short chain of links down to a ladle hanging against the curb
    top = Vector((0.5, -0.07, 0.95))
    pts = [top + Vector((0.0, -0.01 * t, -0.33 * t + 0.03 * math.sin(t * 3.0))) for t in (0.0, 0.33, 0.66, 1.0)]
    b.add(M.tube(pts, [0.009] * 4, sides=4, dome0=0.0, dome1=0.0), "iron_dark", name="chain")
    lad = M.lathe([(0.01, -0.05), (0.05, -0.04), (0.055, 0.0)], sides=8, cap0="fan", cap1="fan")
    lad.translate(pts[-1] + Vector((0, -0.02, -0.07)))
    b.add(lad, "iron", name="ladle")
    for s in (-1, 1):
        roof = M.box((1.42, 0.58, 0.05), (0, 0, 0), bevel=0.012)
        roof.transform(_rx(-s * 34)).translate((0, s * 0.24, 1.64))
        rid = b.add(roof, "charred_deep", name="roof", hull_pad=True)
        n = Vector((0, s * math.sin(math.radians(34)), math.cos(math.radians(34))))
        for q in range(1, 6):
            x = -0.71 + q * 1.42 / 6
            p0 = Vector((x, s * 0.03, 1.64 + 0.2)) + n * 0.03
            p1 = Vector((x, s * 0.47, 1.64 - 0.11)) + n * 0.03
            C.grain(b, "charred_line", p0, p1, n, 0.016, rid)
    ridge = M.box((1.5, 0.09, 0.09), (0, 0, 1.86), bevel=0.012)
    b.add(ridge, "timber", name="ridge")
    return E.finish(b, pal, "decor", ground_h=0.9, name=name)


# ── crates and sacks ───────────────────────────────────────────────────────────────────────────────────
def crate(b: M.Builder, S: float, c: Vector, yaw: float, rnd: random.Random, stencil: bool = True,
          tally: bool = False, name: str = "box") -> int:
    """A battened supply crate (the plains crate's frame and braces) with the crusade's stencil on two faces and
    chalk tallies."""
    m = Matrix.Translation(c) @ _rz(yaw)
    box = M.box((S - 0.04, S - 0.04, S - 0.04), (0, 0, S / 2), bevel=0.012)
    box.transform(m)
    pid = b.add(box, "crate", name=name, hull_pad=True)
    h = S / 2
    fw = 0.07 * S / 0.74
    for ax in range(3):
        for s1 in (-1, 1):
            for s2 in (-1, 1):
                cc = [0.0, 0.0, 0.0]
                size = [fw, fw, fw]
                size[ax] = S
                o = [i for i in range(3) if i != ax]
                cc[o[0]] = s1 * (h - fw / 2 + 0.004)
                cc[o[1]] = s2 * (h - fw / 2 + 0.004)
                cc[2] += h
                bt = M.box(size, cc, bevel=0.01)
                bt.transform(m)
                b.add(bt, "wood_dark", name="batten")
    faces = ((0, -1), (1, 0), (-1, 0), (0, 1))
    for k, (nx, ny) in enumerate(faces):
        n = Vector((nx, ny, 0))
        t = Vector((-ny, nx, 0))
        fc = n * (h - 0.016) + Vector((0, 0, h))
        if stencil and k < 2:
            # the crusade's stencil: the open palm (env art review: the old ring-and-sword stencil read as a Greek Φ);
            # the front face carries it centred, the tallied face smaller and up to the right, clear of the chalk
            sk = S / 0.74
            nn = (m.to_3x3() @ n).normalized()
            u0, v0, size = (0.0, -0.17, 0.34) if not (tally and k == 1) else (0.09, -0.07, 0.25)

            def on_face(u, v, u0=u0, v0=v0, nn=nn):
                return m @ (fc + t * ((u0 + u) * sk) + Vector((0, 0, (v0 + v) * sk))), nn
            palm_sigil(b, on_face, size, "stencil", pid, lift=0.002, name="stencil")
        if tally and k == 1:
            nn = (m.to_3x3() @ n).normalized()
            base = fc + t * (-0.25) + Vector((0, 0, -0.27))
            for q in range(4):
                p0 = base + t * (q * 0.05)
                b.add(E.ribbon([m @ p0, m @ (p0 + Vector((0, 0, 0.12))), m @ (p0 + Vector((0.0, 0, 0.24)))],
                               [nn] * 3, 0.014, lift=0.003, thick=0.002), "chalk", name="chalk", outline=False,
                      covered_by=pid)
            p0 = base + t * (-0.03) + Vector((0, 0, 0.03))
            p1 = base + t * 0.19 + Vector((0, 0, 0.2))
            b.add(E.ribbon([m @ p0, m @ p0.lerp(p1, 0.5), m @ p1], [nn] * 3, 0.014, lift=0.004, thick=0.002), "chalk",
                  name="chalk", outline=False, covered_by=pid)
    return pid


def build_crate(pal: Palette):
    name = "SM_Prop_March_SupplyCrate"
    b, rnd = new(pal, name)
    crate(b, 0.74, Vector((0, 0, 0)), 0.0, rnd, stencil=True, tally=True)
    crate(b, 0.46, Vector((0.04, 0.03, 0.74)), 22.0, rnd, stencil=False, name="box2")
    return E.finish(b, pal, "decor", ground_h=0.9, name=name)


def sack(b: M.Builder, c: Vector, size, rot: Matrix, rnd: random.Random, name: str = "sack",
         surf: dict | None = None) -> int:
    """A tied grain sack: a lumpy, bottom-heavy loft with a gathered neck and a rope tie. ``surf`` collects
    {part id: BVHTree of the sack} so chalk can be laid exactly on its lumpy face (:func:`on_sack`)."""
    w, d, h = size
    rings = [(0.0, w * 0.42, d * 0.42), (h * 0.12, w * 0.5, d * 0.5), (h * 0.45, w * 0.52, d * 0.5),
             (h * 0.72, w * 0.44, d * 0.42), (h * 0.86, w * 0.2, d * 0.2), (h * 0.9, w * 0.1, d * 0.1),
             (h * 0.98, w * 0.16, d * 0.15), (h * 1.04, w * 0.06, d * 0.06)]
    p = M.loft_z(rings, sides=12, cap0="ngon", cap1="fan")
    k1 = rnd.uniform(0, 6.3)
    for v in p.bm.verts:
        a = math.atan2(v.co.y, v.co.x)
        v.co.x *= 1 + 0.06 * math.sin(3 * a + k1)
        v.co.y *= 1 + 0.06 * math.sin(2 * a - k1)
    p.smooth(True)
    p.transform(Matrix.Translation(c) @ rot)
    if surf is not None:
        from mathutils.bvhtree import BVHTree
        tree = BVHTree.FromBMesh(p.bm)
    pid = b.add(p, "sack", name=name, hull_pad=True)
    if surf is not None:
        surf[pid] = tree
    tie = M.belt(w * 0.11, d * 0.11, h * 0.88, 0.03, 0.012, sides=10)
    tie.transform(Matrix.Translation(c) @ rot)
    b.add(tie, "rope", name="tie", outline=False, covered_by=pid)
    return pid


def on_sack(tree, p: Vector, nn: Vector, lift: float = 0.0015) -> Vector:
    """``p`` moved along ``nn`` onto the sack surface (``tree``) + ``lift``: the chalk floated up to 1.2 cm off the
    lumpy cloth and its padded hull bulged into a flat black slab seen on the sacks' far side (env art review)."""
    hit = tree.ray_cast(p + nn * 0.6, -nn, 1.2)[0]          # from well outside: the first hit is the near face
    return (hit + nn * lift) if hit is not None else p


def build_sacks(pal: Palette):
    """Ration sacks counted in chalk (the plains barrel slot, body r ≈ 0.36): two sacks lying at the foot, two
    standing behind leaning together, the front one tallied in chalk (four strokes and a slash, and a ring)."""
    name = "SM_Prop_March_RationSacks"
    b, rnd = new(pal, name)
    surf: dict = {}
    s1 = sack(b, Vector((-0.06, 0.12, 0.0)), (0.42, 0.36, 0.66), _rx(-6) @ _ry(8), rnd, "sack", surf=surf)
    sack(b, Vector((0.2, 0.18, 0.0)), (0.4, 0.34, 0.6), _rx(-4) @ _ry(-10) @ _rz(30), rnd, "sack")
    front = sack(b, Vector((-0.02, -0.06, 0.2)), (0.38, 0.34, 0.6), Matrix.Translation(Vector((0, 0, -0.2)))
                 @ _rx(-88) @ _rz(10), rnd, "sack_front")
    sack(b, Vector((-0.3, -0.12, 0.17)), (0.32, 0.28, 0.5), Matrix.Translation(Vector((0, 0, -0.17)))
         @ _ry(85) @ _rx(-8), rnd, "sack")
    # chalk tally on the standing back sack's front face (−Y), facing the camera's side: every stroke point laid on
    # the cloth (ray cast along the face normal), 1.5 mm proud, so the padded hull stays the sack's own
    base = Vector((-0.12, -0.075, 0.36))
    nn = Vector((0, -1, 0.15)).normalized()
    tree = surf[s1]

    def stroke(pts):
        pts = [on_sack(tree, q, nn) for q in pts]
        b.add(E.ribbon(pts, [nn] * len(pts), 0.013, lift=0.0, thick=0.0015), "chalk", name="chalk", outline=False,
              covered_by=s1)
    for q in range(4):
        p0 = base + Vector((q * 0.04, -0.005 * q, 0.0))
        stroke([p0 + Vector((0.001 * t, 0, 0.14 * t / 4)) for t in range(5)])
    p0, p1 = base + Vector((-0.03, -0.004, 0.02)), base + Vector((0.16, -0.012, 0.12))
    stroke([p0.lerp(p1, t / 4) for t in range(5)])
    return E.finish(b, pal, "decor", ground_h=0.7, name=name, ground_band=0.02)


# ── registry ────────────────────────────────────────────────────────────────────────────────────────────
ASSETS: dict = {}
for _v in "ABC":
    ASSETS[f"SM_Palisade_March_Pew_{_v}"] = (lambda pal, v=_v: build_pew(pal, v),
                                            dict(base_ids=["palisade_plains"], footprint=[1, 1], blocking=True,
                                                 blob=0.14))
ASSETS["SM_Palisade_March_GatePost"] = (build_gatepost, dict(base_ids=["palisade_gate_plains"], footprint=[1, 1],
                                                             blocking=False, blob=0.16))
ASSETS["SM_Prop_March_KnellwatchBanner"] = (build_banner, dict(base_ids=["camp_banner"], footprint=[1, 1],
                                                               blocking=False, blob=0.2, wind_slot=EM.PROP_WIND_SLOT,
                                                               toon_params=EM.PROP_WIND_PARAMS))
# the bell frame also answers to a plain set-piece id: camp layout v2 places no tents, so U4 places the hub's
# centrepiece from the set-piece layer (env art review: it never appeared in game)
BELL_FRAME_ID = "setpiece_bell_frame"
ASSETS["SM_Prop_March_BellFrame"] = (build_bell_frame, dict(base_ids=["camp_tent"], plain_ids=[BELL_FRAME_ID],
                                                            footprint=[2, 2], blocking=True,
                                                            blob=1.1, sockets=[{"name": "lantern",
                                                                                "pos": tuple(LANTERN)},
                                                                               {"name": "bell",
                                                                                "pos": (0, 0, BELL_LIP_Z + 0.4)}]))
ASSETS["SM_Prop_March_Hearth"] = (build_hearth, dict(base_ids=["camp_campfire"], footprint=[1, 1], blocking=False,
                                                     blob=0.5, sockets=[{"name": "flame", "pos": (0, 0, 0.12)}]))
# the hearth's stools and blanket line: a set piece (plain set-piece id, placed by the UE set-piece layer with the
# hearth: same tile and yaw), blocking the cells it stands on (HEARTH_DRESSING_CELLS), never the respawn cell
HEARTH_DRESSING_ID = "setpiece_hearth_dressing"
ASSETS["SM_Prop_March_HearthDressing"] = (build_hearth_dressing,
                                          dict(ids=[HEARTH_DRESSING_ID], footprint=[3, 3], blocking=True, blob=0.01,
                                               wind_slot=EM.PROP_WIND_SLOT, toon_params=EM.PROP_WIND_PARAMS))
# mesh-local tile offsets (Blender +X right, +Y away from the camera before the hearth's camera-facing yaw) of the
# cells the dressing occupies: the blanket line's row behind the hearth and the two front stools' cells; the hearth
# cell (0, 0) and (0, -1), (+1, 0), (-1, -1) stay walkable
HEARTH_DRESSING_CELLS = [[-1, 1], [0, 1], [1, 1], [-1, 0], [1, -1]]
ASSETS["SM_Prop_March_Well"] = (build_well, dict(base_ids=["camp_well"], footprint=[1, 1], blocking=True, blob=0.7))
ASSETS["SM_Prop_March_SupplyCrate"] = (build_crate, dict(base_ids=["camp_crate"], footprint=[1, 1], blocking=True,
                                                         blob=0.5))
ASSETS["SM_Prop_March_RationSacks"] = (build_sacks, dict(base_ids=["camp_barrel"], footprint=[1, 1], blocking=True,
                                                         blob=0.42))
ASSETS["SM_Prop_March_Brazier"] = (build_brazier, dict(base_ids=["camp_torch"], footprint=[1, 1], blocking=False,
                                                       blob=0.3, sockets=[{"name": "flame", "pos": (0, 0, 0.98)}]))
BRAZIER_RECIPE = "camp.torch@march"        # vfx/fx_recipes_dark.py: the smoulder (no light)
NOTES = {
    "SM_Prop_March_BellFrame": "camp_tent@march + setpiece_bell_frame: the hub centrepiece. Camp layout v2 places "
                               "no tents, so the tent slot never shows it — HANDOFF U4 / core: place "
                               "setpiece_bell_frame in Knellwatch's set pieces (door side -Y toward the hearth, the "
                               "2 x 2 body blocking, >= 2 tiles clear of every NPC slot). Socket lantern = the "
                               "bell-frame lantern, one of the zone's four warm lights (#FFB060, chapter1.md §15: "
                               "the runtime light at this socket); socket bell = the toll's bell-ripple VFX origin "
                               "(camp.bell_toll, A12)",
    "SM_Prop_March_BellFrame#footprint": "footprintTiles [2,2] = the tent's nominal body (frame 1.92 x 1.44 m, roof "
                                         "2.44 x 1.94 m); the core blocks the real footprint like the tent's",
    "SM_Prop_March_Hearth": "camp_campfire@march: the respawn point (not blocking; inside ~1.1 m like the plains "
                            "campfire). The warm light #FFB060 + flames = the runtime recipe at socket flame "
                            "(camp.campfire's light, the hearth's only light). Its stools and blanket line are "
                            "SM_Prop_March_HearthDressing (setpiece_hearth_dressing), placed with it",
    "SM_Prop_March_HearthDressing": "setpiece_hearth_dressing: the hearth's three stools and blanket line (chapter1.md "
                                    "§2 'stools, a blanket line'), split off the 1 x 1 non-blocking respawn tile (env "
                                    "art review: heroes respawned inside the stools). U4: place it with every "
                                    "camp_campfire@march hearth at the same tile centre and yaw (the camera-facing "
                                    "yaw BuildCamps gives the campfire) and block blockingCells; the centre cell "
                                    "stays the respawn point",
    "SM_Prop_March_Brazier": "camp_torch@march: a smouldering brazier with NO warm light (chapter1.md §15 reserved "
                             "light). Its recipe is camp.torch@march (vfx_recipes.json: smoke, dull sparks, ash, no "
                             "light). NOT SHIPPABLE with the plains recipe: AAbyssZoneActor::BuildCamps adds "
                             "camp.torch (flame + #FFB060 light) at every torch prop — HANDOFF U1: resolve the "
                             "EffectSpot recipe as <recipe>@<zone presentation theme> first, like the decor "
                             "<id>@march lookup",
    "SM_Palisade_March_GatePost": "one each side of the 2-tile camp gate (half-tile positions, BuildWalls), pennant "
                                  "arm toward the camp (-Y); not blocking (the wall tile beside it blocks)",
    "SM_Palisade_March_Pew_A": "palisade stake spot (BuildWalls: centre + 0.2 / 0.4 toward every camp-wall "
                               "neighbour, random yaw); the variants mix like the plains stakes",
}


def main(argv: list[str]) -> int:
    quick = "--quick" in argv
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or list(ASSETS)
    names = [n for n in ASSETS if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(FAM)
    if sheet or quick:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        for o in objs:
            o.rotation_euler = (0, 0, math.radians(-135.0))
        tag = "march_camp_" + "_".join(n.split("_")[-1][:6] for n in names)[:80]
        if sheet:
            E.sheet(objs, tag, cols=3)
        else:
            E.quick(objs, tag, spacing=1.2 if all("Palisade" in n for n in names) else 2.2)
        return 0
    shipped = []
    for n in (names if "--family" not in argv else []):
        scene.reset()
        pal = Palette(FAM)
        fn, kw = ASSETS[n]
        obj = fn(pal)
        pal.save()
        extra = {"displayName": n.split("_", 2)[-1]}
        if n in NOTES:
            extra["note"] = NOTES[n]
        if f"{n}#footprint" in NOTES:
            extra["footprintNote"] = NOTES[f"{n}#footprint"]
        if "ids" in kw:                     # a set piece: plain ids, theme metadata, no themed slot
            kw = dict(kw)
            ids = kw.pop("ids")
            problems = EM.check_light(obj, pal)
            if problems:
                raise RuntimeError(f"{n}: " + "; ".join(problems))
            extra.update({"family": "environment.camp", "theme": EM.THEME,
                          "placement": "UE set-piece layer (U4, set_pieces.json; Private/World/AbyssSetPieces.cpp): "
                                       "Knellwatch (15,15), with the hearth (camp_campfire@march): same tile and yaw",
                          "blockingCells": HEARTH_DRESSING_CELLS,
                          "blockingNote": "mesh-local tile offsets [x, y] (Blender axes before the yaw; +y = behind "
                                          "the hearth, away from the camera) of the cells the stools and the line "
                                          "stand on; the hearth cell [0, 0] is never blocked"})
            E.ship(obj, n, CAT, pal, obj["af_outline_class"], game_ids=ids, extra=extra, previews=previews, **kw)
        else:
            if n == "SM_Prop_March_Brazier":
                extra["vfx"] = {"socket": "flame", "recipe": BRAZIER_RECIPE, "light": False,
                                "status": "blocked on U1: the runtime plays camp.torch (a flame and a warm light) "
                                          "until EffectSpots resolve camp.torch@march"}
            EM.ship(obj, n, CAT, pal, obj["af_outline_class"], family="environment.camp", extra=extra,
                    previews=previews, **kw)
        shipped.append(n)
    if "--family" in argv:
        shipped = names
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(FAM)
        objs = [ASSETS[n][0](pal) for n in shipped]
        pal_objs = [o for o in objs if "Palisade" in o.name]
        kit = [o for o in objs if o not in pal_objs]
        if pal_objs:
            EM.family_sheet(pal_objs, "camp_palisade.png", "MARCH PEW PALISADE (R1 1.4 M)", cols=4)
        for o in pal_objs:
            o.hide_render = True
        big = [o for o in kit if any(k in o.name for k in ("BellFrame", "Well", "Hearth", "Banner"))]
        # the hearth and its dressing as placed together (the dressing alone is a ring of stools round nothing)
        if any("HearthDressing" in o.name for o in big) and any(o.name == "SM_Prop_March_Hearth" for o in big):
            dress = next(o for o in big if "HearthDressing" in o.name)
            hearth = next(o for o in big if o.name == "SM_Prop_March_Hearth")
            big.remove(dress)
            dress.parent = hearth
        small = [o for o in kit if o not in big]
        if big:
            EM.family_sheet(big, "camp_kit.png", "MARCH CAMP KIT: BELL FRAME WELL HEARTH BANNER", cols=3)
        for o in big:
            o.hide_render = True
        if small:
            EM.family_sheet(small, "camp_kit_small.png", "MARCH CAMP KIT: SACKS CRATES BRAZIER", cols=5)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
