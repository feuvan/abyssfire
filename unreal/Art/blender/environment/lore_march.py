"""The march's lore collectibles, the Five Handbells post and the Dead Knight's Strongbox (dark remake A10 slots
planned in ``march/march_palettes.VARIANTS`` and left unbuilt by the A10 passes; Wave 1 coherence pass). Palette
``Props_March`` (the A1 swatches ``journal.* seal.* catechism.* lore.glyph strongbox.* handbell.glow`` were registered
for them); same game ids, footprints and blocking as the plains slots, themed ``<id>@march`` (U1).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/lore_march.py [names…]
        [--quick] [--no-previews] [--family]
    #   (no flag) ship each (FBX, manifest, previews, ink gate, reserved-light check) + the family sheet
    #   --quick   close 3/4 + 1080p game crop → $AF_SCRATCH_ROOT · --family   only the family sheet (nothing shipped)

| Asset | game id (themed) | lore / event (chapter1.md §12-13) | look |
|---|---|---|---|
| ``SM_Prop_March_TornJournal`` | ``lore_torn_journal@march`` (1 × 1, open) | ``lore_rf_02`` 逃兵的日记 / A Deserter's Journal (105,15, hidden) | a small field journal ``journal.leather`` dropped open on a flat stone, its pages ash-greyed, the last page torn half out and caught under a pebble, a pencil stub; three lines of a cramped hand and the lore glyph (``lore.glyph``, the warm interaction mark) on the open page |
| ``SM_Prop_March_SealShard`` | ``lore_crystal_shard@march`` (1 × 1, open) | ``lore_rf_03`` 封印碎片 / Shard of the Seal (15,108, hidden) | a wedge broken from the Seal of Veins (``seal.stone``, the dais slab's dark stone), propped on two rubble stones; its carved lines run in the brand's pattern and glow warm (``seal.lines`` #FF9A4A: the Abyssfire — the hero's brand — never the breach's violet) |
| ``SM_Prop_March_CatechismStone`` | ``lore_carved_stone@march`` (1 × 1, open) | ``lore_rf_04`` 教会问答石 / The Catechism Stone (85,80) | a weathered stele ``catechism.stone`` ≈ 0.95 m with a rounded head, the church's open palm cut at the top (never a cross, story-bible §12), two blocks of question-and-answer lines below (``catechism.carve``), soot at its foot, a chip off the shoulder; the lore glyph by the first line |
| ``SM_Prop_March_HandbellPost`` | ``decor_event_puzzle_rune_pillar@march`` (1 × 1, open) | ``environmental_puzzle`` 五口小钟 / Five Handbells | a broken stake ≈ 1.3 m (charred top) with a peg, a small bronze handbell on a cord (``bell.bronze``, verdigris streak), tally cuts for its number on the post and a warm mark below the bell (``handbell.glow``: the puzzle's interaction feedback; the runtime adds the ring) |
| ``SK_Prop_March_Strongbox`` | ``treasure_chest@march``, ``decor_treasure_chest@march`` (1 × 1, open) | ``treasure_cache`` 阵亡骑士的铁箱 / A Dead Knight's Strongbox, ``hidden_rf_paymaster``'s chest | the plains chest's body, lid, skeleton (``SKEL_Prop_Chest``: ``root`` → ``lid``) and clips (``Idle`` rattle, ``Open`` 400 ms + ``ChestOpen``), re-dressed: black oak ``strongbox.wood``, iron bands ``strongbox.iron``, a tarnished brass lock ``strongbox.brass``, the crusade's open palm stencilled on the front, the loot seam ``strongbox.seam`` (warm: loot, DR5) |

Lore props take the interactive ink (2.0 px, ``Props_March``); their only glow is the warm lore glyph (interaction,
``march_palettes.WARM_ALLOWED``); no violet anywhere (``envkit_march.check_light`` raises).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit_march as EM  # noqa: E402
import envkit as E  # noqa: E402
import camp_march as CM  # noqa: E402
import interactive as I  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import export, outline, paths, scene, shading  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"
FAM = EM.PROPS
FAMILY = "environment.lore_march"


_REG: dict | None = None


def reg(name: str) -> Region:
    """A ``Props_March`` swatch by name: an A1 region (``march_palettes``) or one a later march generator appended
    (read back from ``palettes/Props_March.json`` with its tone parameters, so the name shares that swatch)."""
    global _REG
    try:
        return EM.region(FAM, name)
    except KeyError:
        if _REG is None:
            import json
            _REG = {n: sw for sw in json.loads((paths.palette_root() / f"{FAM}.json").read_text())["swatches"]
                    if not sw.get("free") for n in sw["names"]}
        sw = _REG[name]
        return Region(sw["hex"], sw.get("s", 0.42), sw.get("l", 0.36), sw.get("e", 0.0), sw.get("o", 1.0))


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


V = Vector
REGIONS = dict(
    leather=reg("journal.leather"), leather_dk=reg("camp.wood_dark"), paper=reg("orderpost.paper"),
    ink=reg("orderpost.ink"), glyph=reg("lore.glyph"), stone=reg("rock.stone"), stone_line=reg("rock.crack"),
    ash=reg("camp.ash"), soot=reg("camp.soot"), pencil=reg("camp.charred"),
    seal_stone=reg("seal.stone"), seal_line=reg("seal.lines"), seal_break=reg("setpiece.dais_slab"),
    cat_stone=reg("catechism.stone"), carve=reg("catechism.carve"),
    post=reg("camp.crate"), charred=reg("camp.charred"), charred_line=reg("camp.charred_line"),
    bronze=reg("bell.bronze"), bronze_dk=reg("bell.bronze_dark"), patina=reg("camp.patina_streak"),
    rope=reg("camp.rope"), nail=reg("camp.nail"), bell_glow=reg("handbell.glow"),
)


def new(pal: Palette, name: str):
    b = M.Builder(name, pal, prefix="lore_march")
    b.regions(**REGIONS)
    return b, scene.rng(name)


def flat_stone(b: M.Builder, c: Vector, r: tuple, seed, region: str = "stone") -> int:
    shape = E.RockShape(r, seed, bumps=4, amp=0.06, top_cut=0.45, boxy=2.4)
    return b.add(E.rock(shape, center=tuple(c), subdiv=2, facet=32.0), region, name="stone", hull_pad=True)


def surface_lines(b: M.Builder, at, rows, region: str, width: float, cover: int | None, name: str = "line",
                  lift: float = 0.0015) -> None:
    """Short strokes of writing / carving on a surface: ``at(u, v) -> (point, normal)``; ``rows`` [(v, u0, u1)]."""
    for k, (v, u0, u1) in enumerate(rows):
        pts, nrm = zip(*(at(u0 + (u1 - u0) * q / 5, v + 0.0035 * math.sin(q * 2.3 + k)) for q in range(6)))
        b.add(E.ribbon(list(pts), list(nrm), width, lift=lift, thick=0.002), region, name=name, outline=False,
              covered_by=cover)


# ── lore_rf_02: a deserter's journal ─────────────────────────────────────────────────────────────────────
def build_journal(pal: Palette):
    """An open field journal dropped on a flat stone (≈ 0.46 m across): two boards hinged on a rounded spine, the page
    blocks curling up to the spine, the last page torn half out and pinned by a pebble, a pencil stub; a cramped hand
    in three lines on each page and the warm lore glyph at the head of the right page."""
    name = "SM_Prop_March_TornJournal"
    b, rnd = new(pal, name)
    sid = flat_stone(b, V((0.0, 0.02, -0.025)), (0.25, 0.2, 0.075), (name, "stone"))
    top = 0.045                                             # the stone's flat top under the book
    tilt = _rx(4.0) @ _rz(-12.0)
    for s in (-1, 1):                                       # covers, a little open past flat
        cov = M.box((0.135, 0.2, 0.014), (s * 0.072, 0, 0.0), bevel=0.004)
        cov.transform(_ry(s * 5.0))
        cov.transform(tilt).translate((0, 0, top + 0.008))
        b.add(cov, "leather", name="cover", hull_pad=True)
    spine = M.tube([V((0, -0.1, 0.012)), V((0, 0.1, 0.012))], [0.011, 0.011], sides=8)
    b.add(spine.transform(tilt).translate((0, 0, top)), "leather_dk", name="spine")
    page_ids = []
    for s in (-1, 1):                                       # page blocks curling up toward the spine
        rows = []
        for j in range(5):
            t = j / 4
            x = s * (0.008 + 0.118 * t)
            z = 0.018 + 0.028 * (1 - t) ** 2
            rows.append([V((x, y, z)) for y in (-0.088, 0.0, 0.088)])
        bm = bmesh.new()
        vv = [[bm.verts.new(p) for p in row] for row in rows]
        for j in range(4):
            for i in range(2):
                bm.faces.new((vv[j][i], vv[j][i + 1], vv[j + 1][i + 1], vv[j + 1][i]))
        bm.normal_update()
        bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.012)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        pg = M.Part(bm).smooth(True)
        pg.transform(tilt).translate((0, 0, top))
        page_ids.append(b.add(pg, "paper", name="pages", hull_pad=True))
    nrm_up = (tilt.to_3x3() @ V((0, 0, 1))).normalized()

    def on_page(s):
        def at(u, v):
            t = (u - 0.008) / 0.118
            z = 0.018 + 0.028 * (1 - max(0.0, min(1.0, t))) ** 2 + 0.0015
            return tilt @ V((s * u, v, z)) + V((0, 0, top)), nrm_up
        return at
    surface_lines(b, on_page(-1), [(0.05, 0.03, 0.105), (0.018, 0.028, 0.11), (-0.014, 0.03, 0.09),
                                   (-0.046, 0.03, 0.1)], "ink", 0.006, page_ids[0], name="writing")
    surface_lines(b, on_page(1), [(0.018, 0.028, 0.1), (-0.014, 0.03, 0.108), (-0.046, 0.03, 0.08)], "ink", 0.006,
                  page_ids[1], name="writing")
    # the lore glyph: a small warm ring-mark at the head of the right page (interaction)
    gpts = [on_page(1)(0.07 + 0.016 * math.cos(a), 0.058 + 0.012 * math.sin(a))[0] for a in
            [math.tau * k / 8 for k in range(9)]]
    b.add(E.ribbon(gpts, [nrm_up] * len(gpts), 0.006, lift=0.002, thick=0.002), "glyph", name="glyph_mark",
          outline=False, covered_by=page_ids[1])
    # the torn last page: half out, curling, pinned at the stone's edge by a pebble
    rows = []
    for j in range(4):
        t = j / 3
        rows.append([V((0.13 + 0.12 * t, y, 0.03 + 0.05 * t * (1 - t))) for y in (-0.05, 0.05)])
    bm = bmesh.new()
    vv = [[bm.verts.new(p) for p in row] for row in rows]
    for j in range(3):
        bm.faces.new((vv[j][0], vv[j][1], vv[j + 1][1], vv[j + 1][0]))
    bm.normal_update()
    bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.005)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    tp = M.Part(bm).smooth(True)
    tp.transform(_rz(-28.0)).translate((0, 0, top - 0.012))
    b.add(tp, "paper", name="torn", hull_pad=True)
    shape = E.RockShape((0.03, 0.025, 0.022), (name, "pebble"), bumps=3, amp=0.08, top_cut=0.2)
    b.add(E.rock(shape, center=(0.22, -0.1, top - 0.02), subdiv=1, facet=30.0), "stone", name="pebble", hull_pad=True)
    # a pencil stub on the left page
    a, e = V((-0.15, -0.07, top + 0.04)), V((-0.06, 0.02, top + 0.052))
    b.add(M.tube([a, e], [0.0055, 0.005], sides=6), "pencil", name="pencil")
    # ash drifted against the stone
    for k, (x, y, r) in enumerate(((-0.2, -0.14, 0.07), (0.18, 0.16, 0.06))):
        shape = E.RockShape((r, r * 0.8, 0.018), (name, "ash", k), bumps=3, amp=0.05)
        b.add(E.rock(shape, center=(x, y, -0.008), subdiv=1, facet=None).auto_sharp(55), "ash", name="ash", hull_pad=True)
    obj = E.finish(b, pal, "interactive", ground_h=0.15, name=name, ground_band=0.02)
    obj["af_sockets"] = '[{"name": "glyph", "pos": [%f, %f, %f]}]' % tuple(on_page(1)(0.07, 0.058)[0])
    return obj


# ── lore_rf_03: the shard of the seal ────────────────────────────────────────────────────────────────────
def build_seal_shard(pal: Palette):
    """A wedge broken from the Seal of Veins (≈ 0.55 m), propped on two rubble stones and tilted toward the camera:
    the carved face (the dais's dark slab stone, ``seal.stone``) with a ring arc and three spokes of the brand's
    pattern glowing warm (``seal.lines``), the broken edges paler stone; ash round its foot."""
    name = "SM_Prop_March_SealShard"
    b, rnd = new(pal, name)
    # the wedge: a thick sector of a disc (the seal's outer ring), broken irregularly along both radial cuts
    r0, r1, a0, a1, th = 0.12, 0.5, math.radians(-26), math.radians(26), 0.07
    ring_o, ring_i = [], []
    for k in range(9):
        a = a0 + (a1 - a0) * k / 8
        jag = 0.012 * math.sin(k * 2.7) if k in (0, 8) else 0.0
        ring_o.append((math.cos(a) * (r1 + jag), math.sin(a) * (r1 + jag)))
    for k in range(5):
        a = a1 - (a1 - a0) * k / 4
        ring_i.append((math.cos(a) * r0, math.sin(a) * r0))
    outline_xy = ring_o + ring_i
    cx = sum(p[0] for p in outline_xy) / len(outline_xy)
    outline_xy = [(x - cx, y) for x, y in outline_xy]
    slab = M.plate(outline_xy, th, center=False)
    slab.flat().auto_sharp(35)
    # stood on its wide outer arc, leaning back 30° from upright onto rubble: the carved face toward the front (−Y,
    # the side the runtime turns to the camera) and up
    xf = Matrix.Translation((0.0, 0.0, 0.15)) @ _rx(60.0) @ _rz(-90.0)
    slab.transform(xf)
    sid = b.add(slab, "seal_stone", name="shard", hull_pad=True)

    def at(u, v):                                   # sector coordinates on the carved (top) face
        return xf @ V((u - cx, v, th + 0.0005)), (xf.to_3x3() @ V((0, 0, 1))).normalized()
    # the brand's pattern: an arc of the inner ring and three spokes (warm: the hero's brand, the Abyssfire)
    # (a ring arc broken off at both ends, a shorter outer arc, and spokes that stop at the rings — a piece of the
    # seal's wheel, never three strokes meeting in a letter)
    for r, f0, f1, w in ((0.3, 0.06, 0.94, 0.024), (0.44, 0.22, 0.86, 0.02)):
        arc = [at(math.cos(a) * r, math.sin(a) * r) for a in [a0 + (a1 - a0) * (f0 + (f1 - f0) * k / 8)
                                                              for k in range(9)]]
        b.add(E.ribbon([p for p, _ in arc], [n for _, n in arc], w, lift=0.0, thick=0.003), "seal_line",
              name="brand_line", outline=False, covered_by=sid)
    for a, ra, rb in ((a0 + 0.13, 0.3, 0.44), (0.05, 0.16, 0.3), (a1 - 0.1, 0.3, 0.47)):
        sp = [at(math.cos(a) * r, math.sin(a) * r) for r in (ra, (ra + rb) / 2, rb)]
        b.add(E.ribbon([p for p, _ in sp], [n for _, n in sp], 0.019, lift=0.0, thick=0.003), "seal_line",
              name="brand_line", outline=False, covered_by=sid)
    for k in range(4):                              # the dark carving of the rest of the seal (not glowing)
        a = a0 + 0.07 + (a1 - a0 - 0.14) * (k + 0.5) / 4
        cv = [at(math.cos(a) * r, math.sin(a) * r) for r in (0.42, 0.47)]
        b.add(E.ribbon([p for p, _ in cv], [n for _, n in cv], 0.01, lift=0.0, thick=0.002), "seal_break",
              name="carve", outline=False, covered_by=sid)
    for k, (x, y, s) in enumerate(((-0.1, 0.2, 0.1), (0.12, 0.17, 0.085), (0.22, -0.12, 0.06))):
        shape = E.RockShape((s, s * 0.85, s * 0.7), (name, "rubble", k), bumps=4, amp=0.1, top_cut=0.3, boxy=2.6)
        b.add(E.rock(shape, center=(x, y, 0.0), subdiv=1, facet=30.0, yaw=37.0 * k), "seal_break", name="rubble",
              hull_pad=True)
    for k, (x, y, r) in enumerate(((-0.22, -0.06, 0.09), (0.2, -0.12, 0.08))):
        shape = E.RockShape((r, r * 0.8, 0.02), (name, "ash", k), bumps=3, amp=0.05)
        b.add(E.rock(shape, center=(x, y, -0.008), subdiv=1, facet=None).auto_sharp(55), "ash", name="ash", hull_pad=True)
    obj = E.finish(b, pal, "interactive", ground_h=0.3, name=name, ground_band=0.02)
    g = at(math.cos(0.0) * 0.36, 0.0)[0]
    obj["af_sockets"] = '[{"name": "glyph", "pos": [%f, %f, %f]}]' % tuple(g)
    return obj


# ── lore_rf_04: the catechism stone ──────────────────────────────────────────────────────────────────────
def build_catechism(pal: Palette):
    """A weathered stele (0.52 × 0.16 × 0.95 m, rounded head, a chip off the left shoulder): the church's open palm
    cut at the top, two blocks of question / answer lines below, the lore glyph by the first line, soot at the foot."""
    name = "SM_Prop_March_CatechismStone"
    b, rnd = new(pal, name)
    W, T, H = 0.52, 0.16, 0.95
    rings = []
    for z in (-0.04, 0.2, 0.5, H - W / 2):
        rings.append((z, W / 2 * (1.0 - 0.05 * z), T / 2 + 0.012))
    for i in range(1, 7):
        a = math.pi / 2 * i / 6
        rings.append((H - W / 2 + W / 2 * 0.86 * math.sin(a), max(W / 2 * math.cos(a), 0.012),
                      max((T / 2 + 0.012) * (0.6 + 0.4 * math.cos(a)), 0.012)))
    body = M.loft_z(rings, sides=16, exp=3.0, cap0="ngon", cap1="ngon")
    for v in body.bm.verts:                                 # the chip off the left shoulder (screen-left: +X here)
        if v.co.x > W * 0.28 and v.co.z > H * 0.62:
            v.co.x -= 0.035 * min(1.0, (v.co.z - H * 0.62) / 0.12)
            v.co.z -= 0.02
    body.recalc_normals()
    body.smooth(True).auto_sharp(48)
    sid = b.add(body, "cat_stone", name="stele", hull_pad=True)
    yf = -(T / 2 + 0.012)

    def at(u, v):
        return V((u, yf - 0.0005, v)), V((0, -1, 0))
    CM.palm_sigil(b, lambda u, v: at(u, H - 0.33 + v), 0.17, "carve", sid, lift=0.0008, thick=0.002, name="palm")
    surface_lines(b, at, [(0.52, -0.17, 0.12), (0.48, -0.2, 0.18), (0.44, -0.2, 0.1)], "carve", 0.011, sid,
                  name="carve")
    surface_lines(b, at, [(0.36, -0.17, 0.16), (0.32, -0.2, 0.19), (0.28, -0.2, 0.15), (0.24, -0.2, 0.06)],
                  "carve", 0.011, sid, name="carve")
    gpts = [at(-0.205 + 0.014 * math.cos(a), 0.52 + 0.012 * math.sin(a))[0] for a in
            [math.tau * k / 8 for k in range(9)]]
    b.add(E.ribbon(gpts, [V((0, -1, 0))] * len(gpts), 0.006, lift=0.0016, thick=0.002), "glyph", name="glyph_mark",
          outline=False, covered_by=sid)
    # soot along the foot: a ragged band (two upright streaks read as a pair of dark slots at the W1 camera)
    pts = [V((-0.22 + 0.44 * q / 8, yf - 0.0006, 0.045 + 0.018 * math.sin(q * 1.9))) for q in range(9)]
    b.add(E.ribbon(pts, [V((0, -1, 0))] * 9, [0.05, 0.07, 0.05, 0.08, 0.06, 0.07, 0.05, 0.07, 0.04], lift=0.001,
                   thick=0.002), "soot", name="soot", outline=False, covered_by=sid)
    for k, (x, y, s) in enumerate(((-0.3, -0.1, 0.07), (0.28, -0.04, 0.06))):
        shape = E.RockShape((s, s * 0.8, s * 0.6), (name, "rubble", k), bumps=4, amp=0.1, top_cut=0.3, boxy=2.6)
        b.add(E.rock(shape, center=(x, y, 0.0), subdiv=1, facet=30.0, yaw=41.0 * k), "stone", name="rubble",
              hull_pad=True)
    shape = E.RockShape((0.2, 0.13, 0.025), (name, "ash"), bumps=3, amp=0.05)
    b.add(E.rock(shape, center=(0.0, -0.14, -0.008), subdiv=1, facet=None).auto_sharp(55), "ash", name="ash", hull_pad=True)
    obj = E.finish(b, pal, "interactive", ground_h=0.6, name=name, ground_band=0.02)
    obj["af_sockets"] = '[{"name": "glyph", "pos": [%f, %f, %f]}]' % tuple(at(-0.205, 0.52)[0])
    return obj


# ── environmental_puzzle: one of the Five Handbells ─────────────────────────────────────────────────────
def build_handbell_post(pal: Palette):
    """A broken stake (1.3 m, charred top) with a peg, a small bronze handbell hanging on a cord from it (its clapper
    carved with the number: the tally cuts on the post), and the warm interaction mark under the bell."""
    name = "SM_Prop_March_HandbellPost"
    b, rnd = new(pal, name)
    H = 1.3
    rings = []
    for k in range(6):
        z = -0.05 + (H + 0.05) * k / 5
        rr = 0.055 * (1.0 + rnd.uniform(-0.05, 0.05)) * (1.0 - 0.12 * k / 5)
        rings.append((z, rr, rr * 0.92))
    post = M.loft_z(rings, sides=8, exp=2.0, cap0="ngon", cap1="ngon")
    teeth_ph = rnd.uniform(0, math.tau)
    for v in post.bm.verts:                                 # burnt teeth at the top
        dz = H - v.co.z
        if dz < 0.18:
            k = 1.0 - max(dz, 0.0) / 0.18
            az = math.atan2(v.co.y, v.co.x)
            v.co.z -= 0.08 * k * (0.5 + 0.5 * math.sin(az * 5 + teeth_ph))
            v.co.x *= 1.0 - 0.15 * k
            v.co.y *= 1.0 - 0.15 * k
    post.transform(_rx(-3.0) @ _ry(2.5))
    pid = b.add(post.smooth(False).auto_sharp(35), "post", name="post", hull_pad=True)
    b.add(M.ellipsoid((0.058, 0.052, 0.07), (0.0, 0.0, H - 0.08), sides=8, rings=4).transform(_rx(-3.0) @ _ry(2.5)),
          "charred", name="char", outline=False, covered_by=pid)
    # the peg, out to the side (the bell hangs clear of the post in the front view)
    peg_a, peg_b = V((-0.03, -0.01, 0.98)), V((-0.21, -0.05, 1.02))
    b.add(M.tube([peg_a, peg_b], [0.016, 0.013], sides=6), "post", name="peg", hull_pad=True)
    # the cord and the bell (mouth down), hung from the peg's end
    cord_top = peg_b + V((0, -0.01, -0.005))
    bell_top = cord_top + V((0.0, 0.0, -0.17))
    b.add(M.tube([cord_top, cord_top + V((0.004, 0.0, -0.09)), bell_top], [0.0045, 0.0045, 0.0045], sides=5),
          "rope", name="cord")
    prof = [(0.008, 0.0), (0.03, -0.012), (0.042, -0.05), (0.05, -0.09), (0.068, -0.115), (0.072, -0.124)]
    bell = M.lathe(list(reversed(prof)), sides=14, cap0="ngon", cap1="fan").translate(bell_top)
    bid = b.add(bell.smooth(True).auto_sharp(50), "bronze", name="bell", hull_pad=True)
    lip = M.cylinder(0.073, 0.073, -0.126, -0.112, sides=14).translate(bell_top)
    b.add(lip, "bronze_dk", name="lip", outline=False, covered_by=bid)
    streak = [bell_top + V((0.05 * math.cos(-2.0), 0.05 * math.sin(-2.0), z)) for z in (-0.045, -0.07, -0.095)]
    b.add(E.ribbon(streak, [V((math.cos(-2.0), math.sin(-2.0), 0.0))] * 3, 0.012, lift=0.004, thick=0.002), "patina",
          name="patina", outline=False, covered_by=bid)
    b.add(M.ellipsoid((0.014, 0.014, 0.018), tuple(bell_top + V((0.0, 0.0, -0.104))), sides=6, rings=3),
          "bronze_dk", name="clapper", outline=False)            # inside the mouth: no ink of its own
    # the number: tally cuts on the post's front, and the warm interaction mark beneath the bell
    nrm = V((0, -1, 0))
    for k in range(3):
        x = -0.02 + 0.02 * k
        pts = [V((x, -0.056, 0.62)), V((x + 0.004, -0.056, 0.7))]
        b.add(E.ribbon(pts, [nrm, nrm], 0.008, lift=0.001, thick=0.002), "charred_line", name="tally", outline=False,
              covered_by=pid)
    gpts = [V((0.018 * math.cos(a), -0.057, 0.8 + 0.018 * math.sin(a))) for a in [math.tau * k / 8 for k in range(9)]]
    b.add(E.ribbon(gpts, [nrm] * len(gpts), 0.007, lift=0.0016, thick=0.002), "bell_glow", name="glyph_mark",
          outline=False, covered_by=pid)
    for k, (x, y, s) in enumerate(((-0.1, 0.05, 0.06), (0.09, 0.07, 0.05))):
        shape = E.RockShape((s, s * 0.8, s * 0.6), (name, "rubble", k), bumps=4, amp=0.1, top_cut=0.3, boxy=2.6)
        b.add(E.rock(shape, center=(x, y, 0.0), subdiv=1, facet=30.0, yaw=53.0 * k), "stone", name="foot",
              hull_pad=True)
    obj = E.finish(b, pal, "interactive", ground_h=0.8, name=name, ground_band=0.02)
    obj["af_sockets"] = ('[{"name": "bell", "pos": [%f, %f, %f]}, {"name": "glyph", "pos": [0.0, -0.06, 0.8]}]'
                         % tuple(bell_top + V((0, 0, -0.07))))
    return obj


# ── treasure_cache / hidden area: the Dead Knight's Strongbox (skeletal: the plains chest's rig and clips) ──────
STRONGBOX = "SK_Prop_March_Strongbox"
BOX_REGIONS = dict(
    wood=reg("strongbox.wood"), wood_line=Region("#4A3A2E").shade_region(.30, .12),
    wood_in=reg("camp.timber_line"), brass=reg("strongbox.iron"), lock=reg("strongbox.brass"),
    keyhole=reg("camp.nail"), seam=reg("strongbox.seam"), gold=reg("strongbox.brass"),
    gold_hi=Region("#C8A050", .32, .45), gem=reg("orderpost.seal"), stencil=reg("camp.crate_stencil"),
)


def build_strongbox(pal: Palette):
    """``interactive.build_chest`` on the strongbox regions (black oak, iron bands, tarnished brass lock, the loot
    seam) + the crusade's open palm stencilled on the front, left of the lock (added to the chest's builder before
    it is finished, covered by the body's hull)."""
    saved_regions, saved_new, saved_finish = I.REGIONS, I.new, I.E.finish

    def new_(pal_, name_):
        b = M.Builder(STRONGBOX, pal_, prefix="strongbox_m")
        b.regions(**BOX_REGIONS)
        return b, scene.rng(STRONGBOX)

    def finish_(b, pal_, cls, **kw):
        body = next(k for k, p in enumerate(b.parts) if p["name"] == "body")
        y = -I.CD / 2 - 0.0005

        def at(u, v):
            return V((-0.17 + u, y, 0.05 + v)), V((0, -1, 0))
        CM.palm_sigil(b, at, 0.15, "stencil", body, lift=0.0008, thick=0.002, name="stencil")
        kw["name"] = STRONGBOX
        return saved_finish(b, pal_, cls, **kw)
    I.REGIONS, I.new, I.E.finish = BOX_REGIONS, new_, finish_
    try:
        obj, b, lid_parts = I.build_chest(pal)
    finally:
        I.REGIONS, I.new, I.E.finish = saved_regions, saved_new, saved_finish
    obj.name = STRONGBOX
    return obj, b, lid_parts


def ship_strongbox(pal: Palette, previews: bool = True) -> dict:
    """``interactive.ship_chest`` for the strongbox: the same skeleton (``SKEL_Prop_Chest``) and clips under its own
    names (``A_Prop_March_Strongbox_Idle`` / ``_Open``), themed game ids, the reserved-light check."""
    obj, b, lid_parts = build_strongbox(pal)
    pal.save()
    problems = EM.check_light(obj, pal)
    if problems:
        raise RuntimeError(f"{STRONGBOX}: " + "; ".join(problems))
    arm = I.chest_rig(obj, b, lid_parts)
    arm.name = STRONGBOX + "_Rig"
    rig = SimpleNamespace(obj=arm)
    acts = []
    for clip in I.CHEST_CLIPS:
        act, c = I.chest_action(arm, clip)
        act.name = f"A_Prop_March_Strongbox_{clip}"
        acts.append((act, c))
    I.open_pose(rig, *acts[1])
    open_bounds = export.bounds_ue([obj])
    I.rest_pose(arm)
    sk = export.export_skeletal_mesh(rig, [obj], STRONGBOX, CAT)
    anim_entries = []
    for act, clip in acts:
        fbx = export.export_animation(rig, act, CAT)
        anim_entries.append(export.clip_entry(clip, act, fbx))
    I.rest_pose(arm)
    bounds = export.bounds_ue([obj])
    man = E.EnvManifest()
    man.set_palette(pal)
    width = float(obj.get("af_outline_width", 0.0))
    from kit import theme
    entry = {
        "kind": "SkeletalMesh", "category": CAT, "fbx": paths.rel_to_export(sk),
        "gameIds": theme.themed_ids(["decor_treasure_chest", "treasure_chest"], EM.THEME), "skeleton": "SKEL_Prop_Chest",
        "scale": 1.0, "heightCm": round((I.CH + I.LID_R * 0.72) * 100.0, 1), "boundsCm": bounds,
        "boundsNote": "closed rest pose (bind pose, no clip); openBoundsCm = the Open end frame (lid up 105 deg)",
        "openBoundsCm": open_bounds, "bones": len(arm.data.bones),
        "triangles": {"toon": E.tris(obj, 0), "outline": E.tris(obj, 1)},
        "materialSlots": [
            {"index": 0, "name": pal.material_name, "parent": "M_AF_Toon", "palette": pal.family},
            {"index": 1, "name": shading.OUTLINE_MATERIAL, "parent": "M_AF_Outline", "palette": pal.family,
             "class": "interactive", "widthCm": round(width * 100.0, 2),
             "outlinePx1080": E.ink_px(pal.family, "interactive"), "outlineMix": outline.INK_MIX["interactive"]}],
        "sockets": [{"name": "fx_open", "bone": "root", "relLocCm": [0.0, 0.0, round((I.CH + 0.05) * 100, 1)],
                     "relRotDeg": [0.0, 0.0, 0.0], "restLocCm": [0.0, 0.0, round((I.CH + 0.05) * 100, 1)]},
                    {"name": "fx_seam", "bone": "root", "relLocCm": [0.0, round(I.CD / 2 * 100 + 1, 1),
                                                                     round(I.CH * 100, 1)],
                     "relRotDeg": [0.0, 0.0, 0.0], "restLocCm": [0.0, round(I.CD / 2 * 100 + 1, 1),
                                                                 round(I.CH * 100, 1)]}],
        "anims": anim_entries, "footprintTiles": [1, 1], "blocking": False, "blobShadowRadiusCm": 42.0,
        "family": FAMILY, "displayName": "Dead Knight's Strongbox", "theme": EM.THEME,
        "themeOf": ["decor_treasure_chest", "treasure_chest"],
        "themeNote": "presentation theme march (chapter1.md §15): the runtime resolves <id>@march before <id> for "
                     "emerald_plains (U1); same footprint, blocking, skeleton, clips and sockets as SK_Prop_TreasureChest",
        "note": "treasure_cache 阵亡骑士的铁箱 / A Dead Knight's Strongbox and hidden_rf_paymaster's chest (chapter1.md "
                "§12-13): the plains chest's body, lid, skeleton and clips re-dressed (black oak, iron bands, a "
                "tarnished brass lock, the crusade palm stencilled on the front); the seam glow strongbox.seam is the "
                "loot's warm (DR5). Idle = restless rattle, Open = lid up 105 deg in 400 ms, ChestOpen notify -> "
                "the chest-open FX at fx_open",
    }
    if previews:
        rv = E.EnvReview(STRONGBOX, arm, [obj], 0.62, blob_radius=0.42,
                         game_outline_px=E.ink_px(pal.family, "interactive"), outline_class="interactive")
        rv.ground_occludes = True
        I.rest_pose(arm)
        rv.game_view("front")
        rv.closeup("front")
        rv.turnaround()
        I.chest_open_views(rv, rig, *acts[1])
        I.chest_sheet(rv, rig, acts)
        I.rest_pose(arm)
        entry["previews"] = [str(Path(p).relative_to(rv.out.parent.parent)) for p, _ in rv.written]
        v = rv.ink_verdict()
        entry["inkGate"] = {k: v[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx",
                                              "maxHullLumP95", "gate", "baked")}
        entry["inkGate"]["bakedRimPx"] = E.baked_bound(v, E.ink_px(pal.family, "interactive"))
        entry["inkGate"]["cellsNote"] = "closed rest pose (game_1080crop) + Open end frame (game_open_1080crop)"
        E.RESULTS.append({"asset": STRONGBOX, "fbx": str(sk), "toonTris": E.tris(obj, 0), "hullTris": E.tris(obj, 1),
                          "ink": {k: v[k] for k in ("pass", "minFracRim", "minFracDark", "medianRimPx",
                                                    "maxHullLumP95")}, "inkFailures": v["failures"]})
    man.set_asset(STRONGBOX, entry)
    man.save()
    return entry


# ── registry / ship ─────────────────────────────────────────────────────────────────────────────────────
ASSETS: dict = {
    "SM_Prop_March_TornJournal": (build_journal, dict(base_ids=["lore_torn_journal"], footprint=[1, 1],
                                                      blocking=False, blob=0.3)),
    "SM_Prop_March_SealShard": (build_seal_shard, dict(base_ids=["lore_crystal_shard"], footprint=[1, 1],
                                                       blocking=False, blob=0.32)),
    "SM_Prop_March_CatechismStone": (build_catechism, dict(base_ids=["lore_carved_stone"], footprint=[1, 1],
                                                           blocking=False, blob=0.4)),
    "SM_Prop_March_HandbellPost": (build_handbell_post, dict(base_ids=["decor_event_puzzle_rune_pillar"],
                                                             footprint=[1, 1], blocking=False, blob=0.3)),
}
NOTES = {
    "SM_Prop_March_TornJournal": "lore_rf_02 逃兵的日记 / A Deserter's Journal (105,15, hidden): lore collectible on "
                                 "the plains slot lore_torn_journal (same footprint, open); interactive ink; the only "
                                 "glow is the warm lore glyph (interaction); the runtime adds the prop.lore light pool",
    "SM_Prop_March_SealShard": "lore_rf_03 封印碎片 / Shard of the Seal (15,108, hidden): the brand lines are warm "
                               "(seal.lines, the Abyssfire: they match the hero's brand), never breach-violet",
    "SM_Prop_March_CatechismStone": "lore_rf_04 教会问答石 / The Catechism Stone (85,80): the open palm (never a "
                                    "cross, story-bible §12) and the question / answer lines carved; the warm lore "
                                    "glyph by the first line",
    "SM_Prop_March_HandbellPost": "environmental_puzzle 五口小钟 / Five Handbells: one of the five posts the event "
                                  "places (the rune-pillar slot's logic and footprint); the warm mark under the bell "
                                  "is the interaction feedback (handbell.glow); socket bell = the ring FX origin",
}


def main(argv: list[str]) -> int:
    quick = "--quick" in argv
    previews = "--no-previews" not in argv
    every = list(ASSETS) + [STRONGBOX]
    names = [a for a in argv if not a.startswith("--")] or every
    names = [n for n in every if any(n == a or a in n for a in names)]
    if quick:
        scene.reset()
        pal = Palette(FAM)
        objs = [ASSETS[n][0](pal) for n in names if n in ASSETS]
        if STRONGBOX in names:
            objs.append(build_strongbox(pal)[0])
        pal.save()
        for o in objs:
            o.rotation_euler = (0, 0, math.radians(-135.0))
        E.quick(objs, "march_lore_" + "_".join(n.split("_")[-1][:6] for n in names)[:60], spacing=1.0)
        return 0
    family_only = "--family" in argv
    for n in ([] if family_only else names):
        scene.reset()
        pal = Palette(FAM)
        if n == STRONGBOX:
            ship_strongbox(pal, previews)
            continue
        fn, kw = ASSETS[n]
        obj = fn(pal)
        pal.save()
        socks = [dict(s) for s in __import__("json").loads(obj.get("af_sockets", "[]"))]
        EM.ship(obj, n, CAT, pal, "interactive", family=FAMILY,
                extra={"displayName": n.split("_", 3)[-1], "note": NOTES[n]}, previews=previews,
                close_facing="front", sockets=socks, **kw)
    if family_only or (previews and len(names) > 1):
        scene.reset()
        pal = Palette(FAM)
        objs = [ASSETS[n][0](pal) for n in names if n in ASSETS]
        if STRONGBOX in names:
            objs.append(build_strongbox(pal)[0])
        EM.family_sheet(objs, "lore_march.png", "MARCH LORE, HANDBELL POST, STRONGBOX", cols=5)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
