"""Story decorations re-skinned for the march (dark remake A10; chapter1.md §12 "Story decorations (positions and
``spriteType`` kept; meshes re-pointed by the presentation theme)"; palette ``Props_March``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/story_march.py [names…]
        [--quick] [--sheet] [--family] [--no-previews]

| Asset | game id (themed) | story decoration | look |
|---|---|---|---|
| ``SM_Prop_March_BrokenBelfry`` | ``decor_ruins@march`` (2 × 1, blocks) | ``story_rf_belfry`` (55,45) 灰麦村钟楼 "Ashwheat's Broken Belfry" | Ashwheat's belfry burnt down: one charred post still standing with its broken head, a stump, the cross-beam fallen against it, roof planks in the mud, and the bell split in two halves sunk in the mud ``#3A322A`` (caked on their low sides), a dull dark bronze ``#64553C`` with thin verdigris streaks, its clapper gone |
| ``SM_Prop_March_RedBanner`` | ``decor_war_banner@march`` (1 × 1, blocks) | ``story_rf_red_banner`` (40,70) 赤幡 | a mound of crusade shields stacked edge on edge, all faced one way, a tall pole driven through it with the Red Banner ``#9A2020`` — the brightest crimson in the decor — torn swallow-tail swaying (``MI_AF_Toon_Props_March_Wind``) |
| ``SM_Prop_March_PenitentsAltar`` | ``decor_broken_altar@march`` (1 × 1, blocks) | ``story_rf_penitents_altar`` (88,55) 赎罪坛 | a cold stone altar ``#5A565C`` with a basin cut in its top and a groove running down its front toward the breach: ash in the basin, something dried black ``#1A1012`` in the groove; a kneeling step and rubble |

Story decorations face the camera (``AAbyssZoneActor::BuildMarkers``: front = Blender −Y) and take the decor ink
(1.4 px, ``Props_March``). No emissive: these are aftermath, not light (story-bible tone pillars).
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
import camp_march as CM  # noqa: E402
import foliage as F  # noqa: E402
import story as ST  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"
FAM = EM.PROPS


def reg(name: str) -> Region:
    return EM.region(FAM, name)


_rx, _ry, _rz = CM._rx, CM._ry, CM._rz

STORY = dict(CM.CAMP, mud=reg("belfry.mud"), redbanner=reg("redbanner.cloth"), altar=reg("altar.stone"),
             groove=reg("altar.groove"), dried=reg("altar.dried"), stake=reg("stake.wood"),
             ritual_ash=reg("ritual.ash"), altar_line=Region("#5A565C").line_region(),
             # the Red Banner's trim and tears: a darker crimson (appended; same hue family as redbanner.cloth)
             redbanner_dark=Region("#5E1618", .42, .30))


def new(pal: Palette, name: str):
    b = M.Builder(name, pal, prefix="story")
    b.regions(**STORY)
    return b, scene.rng(name)


# ── the broken belfry ──────────────────────────────────────────────────────────────────────────────────
# (env art review: "Fallen" is on story-bible §12's never-use list; chapter1.md §19 renames story_rf_belfry to
# "Ashwheat's Broken Belfry" — the mesh was SM_Prop_March_FallenBelfry until then)
BELL_PROF = [(0.4, 0.0), (0.42, 0.045), (0.37, 0.11), (0.29, 0.3), (0.26, 0.5), (0.245, 0.63), (0.2, 0.73),
             (0.1, 0.785), (0.02, 0.8)]


def half_bell(k: float, a0: float, a1: float, thick: float = 0.035, steps: int = 9, seed: float = 0.0,
              blotches: bool = False) -> M.Part:
    """A closed solid piece of a bell shell (azimuth ``a0``..``a1``): outer and inner surfaces from ``BELL_PROF``,
    the lip, the crown and the two broken faces. The inside and the breaks carry ``af_sub`` 1 (dark bronze), the lip
    ``af_sub`` 3 (the one lit rim); ``blotches`` = the old verdigris blotches on the outside (``af_sub`` 2) — off: the
    belfry's verdigris runs in streaks (env art review: square patches read as camouflage at 1080p)."""
    bm = bmesh.new()
    lay = bm.faces.layers.int.new("af_sub")
    prof = [(r * k, z * k) for r, z in BELL_PROF]
    outer, inner = [], []
    for i in range(steps + 1):
        a = a0 + (a1 - a0) * i / steps
        ca, sa = math.cos(a), math.sin(a)
        outer.append([bm.verts.new((ca * r, sa * r, z)) for r, z in prof])
        inner.append([bm.verts.new((ca * max(r - thick, 0.004), sa * max(r - thick, 0.004), z + thick * 0.4))
                      for r, z in prof])
    n = len(prof)
    for i in range(steps):
        for j in range(n - 1):
            f = bm.faces.new((outer[i][j], outer[i + 1][j], outer[i + 1][j + 1], outer[i][j + 1]))
            a = a0 + (a1 - a0) * (i + 0.5) / steps
            if blotches and j < 5 and math.sin(3.0 * a + seed) * math.sin(2.2 * j + seed) > 0.55:
                f[lay] = 2
            elif j == 0:
                f[lay] = 3
            g = bm.faces.new((inner[i][j + 1], inner[i + 1][j + 1], inner[i + 1][j], inner[i][j]))
            g[lay] = 1
        bm.faces.new((outer[i][0], inner[i][0], inner[i + 1][0], outer[i + 1][0]))              # lip
        bm.faces.new((outer[i][-1], outer[i + 1][-1], inner[i + 1][-1], inner[i][-1]))          # crown edge
    for i, flip in ((0, False), (steps, True)):                                                 # the breaks
        for j in range(n - 1):
            q = (outer[i][j], outer[i][j + 1], inner[i][j + 1], inner[i][j])
            f = bm.faces.new(tuple(reversed(q)) if flip else q)
            f[lay] = 1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if bm.calc_volume(signed=True) < 0:      # the thin shell fools recalc's heuristic: make the solid face outward
        for f in bm.faces:
            f.normal_flip()
    return M.Part(bm).auto_sharp(40)


def mud_caked(part: M.Part, z: float = 0.07) -> M.Part:
    """``af_sub`` 4 (the belfry mud) on the outer faces lower than ``z``: the halves sunk in the mud carry it."""
    bm = part.bm
    lay = bm.faces.layers.int.get("af_sub") or bm.faces.layers.int.new("af_sub")
    for f in bm.faces:
        if f[lay] in (0, 3) and f.calc_center_median().z < z:
            f[lay] = 4
    return part


def build_broken_belfry(pal: Palette):
    name = "SM_Prop_March_BrokenBelfry"
    b, rnd = new(pal, name)
    # the standing post (its head broken off in a splintered point) and a burnt stump
    post = E.spike(0.2, 1.9, 0.22, seed=(name, "post"), square=True, rows=10).translate((0.62, 0.12, 0))
    pid = b.add(post, "charred", name="post", hull_pad=True)
    stump = E.spike(0.2, 0.62, 0.18, seed=(name, "stump"), square=True, rows=4).translate((-0.66, 0.2, 0))
    b.add(stump, "charred", name="stump", hull_pad=True)
    for z in (0.5, 1.2):
        C_band = E.rope_band((0.101, 0.101), z, height=0.05, thick=0.008, sides=12, exp=10.0).translate((0.62, 0.12, 0))
        b.add(C_band, "iron", name="band", outline=False, covered_by=pid)
    # the cross-beam fallen against the post, its far end in the mud
    a = Vector((0.55, 0.05, 1.55))
    e = Vector((-0.95, -0.12, -0.04))
    beam = M.tube([a, a.lerp(e, 0.5), e], [(0.1, 0.09)] * 3, sides=4, exp=12.0, dome0=0.0, dome1=0.0)
    beam.recalc_normals()
    b.add(beam, "timber", name="beam", hull_pad=True)
    # roof planks in the mud
    for q, (x, y, yaw, tilt) in enumerate(((-0.35, 0.42, 20, 4), (0.05, 0.5, -35, -6), (-0.9, 0.3, 75, 8))):
        pl = M.box((0.7, 0.16, 0.04), (0, 0, 0), bevel=0.008)
        pl.transform(Matrix.Translation(Vector((x, y, 0.02))) @ _rz(yaw) @ _rx(tilt))
        b.add(pl, "charred", name="plank", hull_pad=True)
    # the bell lying on its side in the mud, its axis across the front (its profile reads at the W1 pitch), a quarter
    # broken out of its upper front (the break shows the dark inside) and lying beside it; sunk ~8 cm
    # env art review: the sandy bronze (A1's bell.bronze, lit L* 63) with teal blocks read as camouflage at 1080p —
    # the halves are the dull dark bronze of the keepers' bell (camp_march ``bronze_dull``), the lip the one lit
    # rim, verdigris in thin streaks, mud caked on their sunk sides
    k = 0.95
    subs = {1: "bronze_dark", 3: "bronze", 4: "mud"}
    a0, a1 = math.radians(-150), math.radians(105)
    base = Matrix.Translation(Vector((-0.28, -0.3, 0.32))) @ _rz(12) @ _ry(-90)
    h1 = half_bell(k, a0, a1, steps=12, seed=1.3)
    h1.transform(base)
    pid1 = b.add(mud_caked(h1), "bronze_dull", name="bell", hull_pad=True, sub_regions=subs)
    CM.patina_streaks(b, rnd, k, base, pid1, [math.radians(d) for d in (-128, -70, -16, 44, 88)], "patina_streak",
                      prof=BELL_PROF, facet=(a1 - a0) / 12, phase=a0, min_z=0.06)
    b0, b1 = math.radians(110), math.radians(205)
    m2 = Matrix.Translation(Vector((0.42, -0.5, 0.14))) @ _rz(-40) @ _ry(-70) @ _rx(-20)
    h2 = half_bell(k, b0, b1, steps=5, seed=2.1)
    h2.transform(m2)
    pid2 = b.add(mud_caked(h2), "bronze_dull", name="bell_shard", hull_pad=True, sub_regions=subs)
    CM.patina_streaks(b, rnd, k, m2, pid2, [math.radians(d) for d in (128, 172)], "patina_streak", prof=BELL_PROF,
                      facet=(b1 - b0) / 5, phase=b0, min_z=0.06)
    # mud thrown up round the bell: low lumps hugging the rim (no ground mound: the terrain is the ground)
    for q in range(6):
        ang = rnd.uniform(0, math.tau)
        c = Vector((-0.12 + math.cos(ang) * 0.38, -0.18 + math.sin(ang) * 0.3, 0.0))
        shape = E.RockShape((rnd.uniform(0.09, 0.13), rnd.uniform(0.07, 0.1), 0.05), (name, "mud", q), bumps=4,
                            amp=0.1, top_cut=0.0)
        part = E.rock(shape, subdiv=1, facet=None, yaw=rnd.uniform(0, 360))
        E.unflare_foot(part, 0.03)
        b.add(part.translate(c), "mud", name="mud", hull_pad=True)
    F.tuft(b, Vector((0.85, -0.25, 0)), 0.22, 6, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=1.0, name=name, ground_band=0.02)


# ── the Red Banner ─────────────────────────────────────────────────────────────────────────────────────
def heater(b: M.Builder, c: Vector, s: float, rot: Matrix, region: str = "shield", rim: bool = True) -> int:
    """A crusade heater shield (point down), domed, with an iron rim strip on its face."""
    sw, sh = 0.34 * s, 0.44 * s
    out = [(-sw / 2, sh * 0.5), (sw / 2, sh * 0.5), (sw / 2, sh * 0.05), (sw * 0.32, -sh * 0.28), (0.0, -sh * 0.5),
           (-sw * 0.32, -sh * 0.28), (-sw / 2, sh * 0.05)]
    p = M.plate(out, 0.024 * s, bevel=0.004)
    for v in p.bm.verts:
        v.co.z += 0.03 * s * (1 - (v.co.x / (sw / 2)) ** 2)
    m = Matrix.Translation(c) @ rot @ _rx(90)
    p.transform(m)
    pid = b.add(p, region, name="shield", hull_pad=True)
    if rim:
        pts = [Vector((x * 0.93, 0.0, y * 0.93)) for x, y in reversed(out + out[:1])]
        wp = []
        for q in pts:
            bulge = 0.03 * s * (1 - (q.x / (sw / 2)) ** 2)
            wp.append(m @ Vector((q.x, q.z, 0.012 * s + bulge + 0.0015)))
        nn = (m.to_3x3() @ Vector((0, 0, 1))).normalized()
        b.add(E.ribbon(wp, [nn] * len(wp), 0.02 * s, lift=0.0, thick=0.002), "shield_rim", name="rim",
              outline=False, covered_by=pid)
    return pid


def build_red_banner(pal: Palette):
    """A mound of crusade shields stacked on a heap of earth, every face turned one way (the decoration's front, −Y,
    the camera's side), a tall pole driven through the heap with the Red Banner: torn swallow-tail, a darker
    crimson band at the bar, the cloth swaying from the bar."""
    name = "SM_Prop_March_RedBanner"
    b, rnd = new(pal, name)
    # the heap: three low lumps of mud the shields lean on
    for q, (x, y, r) in enumerate(((0.0, 0.1, 0.4), (-0.22, -0.05, 0.28), (0.24, -0.02, 0.26))):
        shape = E.RockShape((r, r * 0.85, r * 0.65), (name, "heap", q), bumps=4, amp=0.08, top_cut=0.0)
        part = E.rock(shape, subdiv=2, facet=None)
        E.unflare_foot(part, 0.05)
        b.add(part.translate((x, y, 0)), "mud", name="heap", hull_pad=True)
    # shields in two rings leaning on the heap, faces toward −Y (a few turned, as they fell)
    k = 0
    for ring, (n_, rr, z, lean, s) in enumerate(((7, 0.48, 0.2, 18, 1.0), (5, 0.26, 0.48, 12, 0.9))):
        for i in range(n_):
            ang = -math.pi / 2 + (i - (n_ - 1) / 2) * (1.6 / n_) * (1.0 if ring == 0 else 1.2)
            if ring == 0 and i in (0, n_ - 1):
                ang += math.pi * 0.35 * (1 if i else -1)
            c = Vector((math.cos(ang) * rr, math.sin(ang) * rr + 0.06, z))
            face_yaw = math.degrees(ang) + 90 + rnd.uniform(-12, 12)
            rot = _rz(face_yaw) @ _rx(-lean - rnd.uniform(0, 10)) @ _ry(rnd.uniform(-14, 14))
            heater(b, c, s * rnd.uniform(0.9, 1.08), rot, region=("shield", "charred", "shield", "helmet")[k % 4])
            k += 1
    # the pole and the banner
    H = 2.75
    pole = M.tube([(0.02, 0.12, 0.1), (0.02, 0.12, H * 0.5), (0.03, 0.12, H)], [0.05, 0.045, 0.04], sides=8)
    poleid = b.add(pole, "pole", name="pole", hull_pad=True)
    fin = M.gem(0.05, 0.22, 0.05, sides=4).translate((0.03, 0.12, H + 0.03))
    b.add(fin, "iron", name="finial")
    bar = M.tube([(-0.06, 0.12, H - 0.15), (0.72, 0.12, H - 0.15)], [0.034, 0.034], sides=8)
    b.add(bar, "bar", name="bar")
    w0, L = 0.62, 1.3
    pts = [(0.0, 0.0), (w0, 0.0), (w0 + 0.03, -L * 0.92), (w0 * 0.72, -L * 0.7), (w0 * 0.5, -L),
           (w0 * 0.3, -L * 0.78), (0.0, -L * 0.95)]
    cloth = M.plate(list(reversed(pts)), 0.02, bevel=0.004)

    def bulge(x, y):
        return (0.05 * math.sin(math.pi * min(max(x / w0, 0.0), 1.0)) +
                0.035 * math.sin(x * 9 + 0.6) * max(0.0, (-y - 0.7) / 0.5))
    for v in cloth.bm.verts:
        v.co.z += bulge(v.co.x, v.co.y)
    cloth.transform(_rx(90)).translate((0.05, 0.12, H - 0.18))
    cid = b.add(cloth, "redbanner", name="cloth", hull_pad=True)
    sway = {cid}
    band = M.box((w0 - 0.02, 0.03, 0.09), (0.05 + w0 / 2, 0.12, H - 0.24), bevel=0.008)
    sway.add(b.add(band, "redbanner_dark", name="trim", outline=False, covered_by=cid))
    # burnt holes / tears: darker crimson patches lying on both faces
    for side in (-1, 1):
        for (x, z, r) in ((0.38, -0.55, 0.06), (0.16, -0.9, 0.045)):
            off = -bulge(x, z)
            c = Vector((0.05 + x, 0.12 + off + side * 0.0115, H - 0.18 + z))
            ring = [c + Vector((math.cos(a) * r, 0, math.sin(a) * r * 1.3)) for a in
                    (math.tau * i / 8 for i in range(9))]
            bm = bmesh.new()
            vs = [bm.verts.new(p) for p in ring[:-1]]
            f = bm.faces.new(vs)
            bm.normal_update()
            if f.normal.y * side < 0:
                f.normal_flip()
            sway.add(b.add(M.Part(bm), "redbanner_dark", name="tear", outline=False, covered_by=cid))
    hull_ids = {kk for kk, p in enumerate(b.parts) if p.get("hullOnly") and p["name"].startswith("cloth")}
    wind = E.cloth_wind({p: (H - 0.18, L, 0.2) for p in sway | hull_ids})
    return E.finish(b, pal, "decor", ground_h=1.0, wind=wind, name=name, ground_band=0.02)


# ── the Penitents' Altar ───────────────────────────────────────────────────────────────────────────────
def build_penitents_altar(pal: Palette):
    """The plains broken altar's footprint (≈ 1.2 × 0.7 m slab + body), cold stone: a coursed block with a round basin
    cut in its top, a groove running from the basin over the front edge and down the face (toward the breach), ash
    settled in the basin, something dried black down the groove; a kneeling step before it, rubble."""
    name = "SM_Prop_March_PenitentsAltar"
    b, rnd = new(pal, name)
    ST.stone(b, "altar", (1.2, 0.72, 0.22), (0, 0.0, 0.11), rnd, tilt=0.6, name="slab")
    for kk, (z0, h) in enumerate(((0.22, 0.3), (0.52, 0.3))):
        ws = [0.46, 0.44] if kk == 0 else [0.3, 0.32, 0.28]
        x = -0.45
        for j, w in enumerate(ws):
            ST.stone(b, "altar", (w - 0.012, 0.54, h - 0.012), (x + w / 2, 0.0, z0 + h / 2), rnd, tilt=1.0,
                     name="body")
            x += w
    top_z = 0.82
    top = M.box((0.98, 0.64, 0.12), (0, 0, top_z + 0.06), bevel=0.02)
    tid = b.add(top, "altar", name="top", hull_pad=True)
    # the basin: a dark ring + ash bed lying in the top face
    bz = top_z + 0.121
    ring = M.belt(0.2, 0.17, bz - 0.002, 0.004, 0.035, sides=16, exp=2.0)
    b.add(ring, "groove", name="basin_rim", outline=False, covered_by=tid)
    ash = M.cylinder(0.17, 0.14, bz - 0.001, bz + 0.002, sides=14)
    b.add(ash, "ritual_ash", name="basin_ash", outline=False, covered_by=tid)
    # the groove: from the basin to the front edge, then down the front face of the top block and the body
    fy = -0.321
    g_top = [Vector((0.02, -0.15, bz)), Vector((0.03, -0.24, bz)), Vector((0.04, -0.31, bz))]
    b.add(E.ribbon(g_top, [Vector((0, 0, 1))] * 3, 0.04, lift=0.0005, thick=0.002), "groove", name="groove",
          outline=False, covered_by=tid)
    b.add(E.ribbon([Vector((0.035, -0.15, bz + 0.001)), Vector((0.04, -0.3, bz + 0.001))], [Vector((0, 0, 1))] * 2,
                   0.018, lift=0.001, thick=0.002), "dried", name="dried", outline=False, covered_by=tid)
    down = [Vector((0.045, fy - 0.001, top_z + 0.1)), Vector((0.05, fy - 0.001, top_z + 0.02)),
            Vector((0.06, fy + 0.04, top_z - 0.15)), Vector((0.07, fy + 0.04, top_z - 0.42))]
    b.add(E.ribbon(down, [Vector((0, -1, 0))] * 4, [0.03, 0.028, 0.024, 0.016], lift=0.002, thick=0.003), "dried",
          name="dried", outline=False)
    # a kneeling step in front and rubble
    ST.stone(b, "altar", (0.7, 0.26, 0.12), (0.0, -0.6, 0.06), rnd, tilt=2.0, chip=True, name="step")
    for q, (x, y, r) in enumerate(((0.8, -0.24, 0.13), (-0.84, 0.16, 0.1), (0.62, 0.44, 0.08))):
        shape = E.RockShape((r, r * 0.85, r * 0.7), (name, "rub", q), bumps=4, amp=0.08, top_cut=0.3)
        part = E.rock(shape, subdiv=1, facet=26.0)
        E.unflare_foot(part, 0.04)
        b.add(part.translate((x, y, 0)), "altar", name="rubble", hull_pad=True)
    F.tuft(b, Vector((-0.66, -0.44, 0)), 0.2, 6, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.9, name=name, ground_band=0.02)


# ── registry ────────────────────────────────────────────────────────────────────────────────────────────
ASSETS: dict = {
    "SM_Prop_March_BrokenBelfry": (build_broken_belfry, dict(base_ids=["decor_ruins"], footprint=[2, 1],
                                                             blocking=True, blob=1.0)),
    "SM_Prop_March_RedBanner": (build_red_banner, dict(base_ids=["decor_war_banner"], footprint=[1, 1],
                                                       blocking=True, blob=0.55, wind_slot=EM.PROP_WIND_SLOT,
                                                       toon_params=EM.PROP_WIND_PARAMS)),
    "SM_Prop_March_PenitentsAltar": (build_penitents_altar, dict(base_ids=["decor_broken_altar"], footprint=[1, 1],
                                                                 blocking=True, blob=0.7)),
}
NOTES = {
    "SM_Prop_March_BrokenBelfry": "story_rf_belfry (55,45) \"Ashwheat's Broken Belfry\" (chapter1.md §19), spriteType "
                                  "ruins: story decoration, faces the camera (front -Y), inspect via its interaction "
                                  "radius; blocks its 2 x 1 footprint (W5). Renamed from SM_Prop_March_FallenBelfry "
                                  "(story-bible §12 never-use list)",
    "SM_Prop_March_RedBanner": "story_rf_red_banner (40,70), spriteType war_banner: faces the camera; the shields "
                               "face the front (-Y). Blocks 1 x 1",
    "SM_Prop_March_PenitentsAltar": "story_rf_penitents_altar (88,55), spriteType broken_altar: faces the camera; the "
                                    "groove runs down the front face. Blocks 1 x 1",
}


# the story decoration's display name follows chapter1.md §19 (en "Ashwheat's Broken Belfry")
DISPLAY = {"SM_Prop_March_BrokenBelfry": "Ashwheat's Broken Belfry"}
RENAMED = {"SM_Prop_March_BrokenBelfry": "SM_Prop_March_FallenBelfry"}


def retire_renamed(previews: bool = True) -> list[str]:
    """Drop what a renamed asset left under its old name: the manifest entry (+ its game-id mappings to it), the FBX
    and the review folder (the re-import check reads the new entry). Returns what was removed."""
    import shutil
    from kit import export, paths
    out = []
    man = export.Manifest()
    for new_name, old in RENAMED.items():
        if old in man.data["assets"]:
            fbx = man.data["assets"][old].get("fbx")
            del man.data["assets"][old]
            for gid, nm in list(man.data["gameIds"].items()):
                if nm == old:
                    if new_name in man.data["assets"]:
                        man.data["gameIds"][gid] = new_name
                    else:
                        del man.data["gameIds"][gid]
            out.append(f"manifest {old}")
            if fbx and (paths.export_root() / fbx).exists():
                (paths.export_root() / fbx).unlink()
                out.append(f"Export/{fbx}")
        pv = paths.preview_root() / old
        if previews and pv.is_dir():
            shutil.rmtree(pv)
            out.append(f"Previews/{old}/")
    man.save()
    return out


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
        tag = "march_story_" + "_".join(n.split("_")[-1][:6] for n in names)[:80]
        if sheet:
            E.sheet(objs, tag, cols=3)
        else:
            E.quick(objs, tag, spacing=2.4)
        return 0
    shipped = []
    for n in (names if "--family" not in argv else []):
        scene.reset()
        pal = Palette(FAM)
        fn, kw = ASSETS[n]
        obj = fn(pal)
        pal.save()
        extra = {"displayName": DISPLAY.get(n, n.split("_", 2)[-1]), "note": NOTES[n]}
        if n in RENAMED:
            extra["renamedFrom"] = RENAMED[n]
        EM.ship(obj, n, CAT, pal, obj["af_outline_class"], family="environment.story", extra=extra,
                previews=previews, close_facing="front", **kw)
        shipped.append(n)
    if any(n in RENAMED for n in shipped):
        for r in retire_renamed(previews):
            print(f"  retired {r}")
    if "--family" in argv:
        shipped = names
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(FAM)
        objs = [ASSETS[n][0](pal) for n in shipped]
        EM.family_sheet(objs, "story_decor.png", "MARCH STORY DECORATIONS", cols=3)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
