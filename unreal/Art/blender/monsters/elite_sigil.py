"""The dark remake's elite marker (monsters.md §3; implementation-plan.md A8): ``SM_FX_EliteSigil`` (W2) and the W1
recolour ``SM_FX_EliteCrown_March`` (``elite_crown@march``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/elite_sigil.py
        (no flag) ship both · --quick (close-ups / turnaround / game crop → scratch) · --no-previews
        --gaps (the gnawer facing sheets → scratch) · --patch-gnawer-sockets (manifest fx_overhead of the gnawers)

* ``SM_FX_EliteSigil`` (W2, game id ``elite_sigil``): "a small floating broken iron halo with a crimson ember" — a
  29 cm hoop of dark iron (``elite_sigil.iron`` #4A4C52, the rule ``elite_sigil.iron_line``) crowned with seven
  short iron nails of uneven length, broken open at the back (the ends torn and bent, two shards drifting in the
  gap), and at the front a faceted ember held in an iron claw (``elite_sigil.core`` #C0283A, emissive .9: crimson,
  never orange). It floats level 6 cm over ``fx_overhead`` (the W1 camera's −50° sees a level ring as an open ellipse) and
  turns slowly about its axis (manifest ``fx.motion``); the crimson glow is a runtime card (``fx.glow``).
* ``SM_FX_EliteCrown_March`` (W1, ``elite_crown@march``): ``elite_crown.build_crown`` unchanged in shape, its
  regions mapped onto ``Monsters_March`` swatches — review round 2: tarnished, low-light iron (``elite_crown.iron``
  #55575B at .22, the ball tips ``elite_crown.iron_hi`` #66686B at .20; the A1 #5E6066 / #8A8C90 lit a clean silver
  crown), dark patina pits on the front arc (``elite_crown.patina``), the crimson stone #C0283A the only accent — the
  marker the W1 runtime draws over every elite in the march zone (U1 resolves ``<id>@march``).

Over the gnawers both ride their new ``fx_overhead`` (``gnawers.GnawerKit._place_overhead``: on ``root``, centred
over the feet, 1.57 × the head centre's height; ``--patch-gnawer-sockets`` writes it into the manifest): the
``*_over_gnawers_facings.png`` sheets measure the clear gap in se / sw / ne / nw (gate ``GAP_MIN_PX`` 4 px at
1080p). Valdras opts out (manifest ``eliteMarker: "none"``): ``*_over_monsters`` show him bare, then with the marker
on his fallback socket ``fx_elite``.

Both ink with the monster class (3.0 px at 1080p): the weight of the elite they mark. ``elite_crown.py`` is only
read (its builder runs with this module's regions).
"""
from __future__ import annotations

import copy
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE.parent / "coherence", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, review, scene, shading  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
import elite_crown as EC  # noqa: E402
import hero_base as HB  # noqa: E402

FAMILY = "Monsters_March"
OUTLINE_CLASS = "monster"
SIGIL = "SM_FX_EliteSigil"
CROWN = "SM_FX_EliteCrown_March"
R_HOOP = 0.128                    # hoop centre-line radius (29 cm across the outer edge)
HOOP_H, HOOP_T = 0.024, 0.013     # band height (vertical) and thickness (radial)
GAP = (128.0, 172.0)              # the break, degrees from the front (0) toward the character's left (+)
NAILS = [(-150.0, 0.050), (-104.0, 0.064), (-58.0, 0.046), (-20.0, 0.070), (22.0, 0.058), (66.0, 0.068),
         (104.0, 0.040)]          # (angle, length) — uneven, one stub (104°) snapped short
EMBER_R = 0.026
SIGIL_FLOAT = 0.06                # the hoop's lowest point floats this far above fx_overhead (the pivot)
GLOW = "#C0283A"

SIGIL_FX = {
    "glow": {"color": GLOW, "alpha": 0.38, "radiusCm": 12.0, "blend": "additive", "falloff": "web glow",
             "socket": "ember", "pulse": {"periodMs": 1400, "alphaMin": 0.24},
             "note": "crimson (monsters.md §3: a crimson ember — never orange; DR5)"},
    "motion": {"bobCm": 3.0, "bobPeriodMs": 1600, "spinDegPerSec": 45.0, "faceCameraYaw": False,
               "note": "W2: replaces SM_FX_EliteCrown over elites; floats level 6 cm over fx_overhead and turns slowly about "
                       "its own axis (the break and the shards come round every 8 s)"},
    "socket": "fx_overhead", "absoluteScale": True,
    "scaleNote": "keeps its 29 cm on scaled elites (hunt leaders x1.25, the 2.1 m boss): attach with absolute scale",
}


def _march_regions() -> dict:
    import march_palettes as MP
    return {n: r for n, r, _ in MP.family_regions(FAMILY)}


def _dir(a: float) -> Vector:
    """Unit vector in the ring plane at angle ``a`` (deg; 0 = front −Y, + toward the character's left +X)."""
    t = math.radians(a)
    return Vector((math.sin(t), -math.cos(t), 0.0))


def _in_gap(a: float) -> bool:
    a = (a + 360.0) % 360.0
    return GAP[0] <= a <= GAP[1]


# ── the sigil ──────────────────────────────────────────────────────────────────────────────────────────────────
def build_sigil(pal: Palette) -> bpy.types.Object:
    regs = _march_regions()
    b = M.Builder(SIGIL, pal, prefix="elite_sigil")
    for key in ("iron", "iron_line", "core"):
        b.swatch[key] = pal.add(f"elite_sigil.{key}", regs[f"elite_sigil.{key}"])
    rig = Bind.rigid("root")
    # the hoop: a squared band swept round the circle, open across the gap; its ends torn (tapered, bent up / out)
    a0, a1 = GAP[1], GAP[0] + 360.0
    n = 22
    angs = list(np.linspace(a0, a1, n + 1))
    pts, scales = [], []
    for i, a in enumerate(angs):
        t = i / n
        end = min(t, 1.0 - t)
        tear = smooth(min(1.0, end / 0.06))          # the last 6 % of each end tapers to a torn tip
        lift = (1.0 - tear) * 0.012
        r = R_HOOP + (1.0 - tear) * 0.010
        pts.append(_dir(a) * r + Vector((0.0, 0.0, lift)))
        scales.append((HOOP_T * (0.35 + 0.65 * tear), HOOP_H * (0.30 + 0.70 * tear)))
    prof = M.superellipse(1.0, 1.0, 6, 3.0)
    hoop = M.sweep(pts, prof, scales=scales, up=[(0, 0, 1)] * len(pts), dome0=0.004, dome1=0.004)
    hid = b.add(hoop, "iron", rig, name="hoop", hull_pad=True)
    # the rule: a dark line round the hoop's outer face (it reads as wrought iron at 1080p)
    rpts = [_dir(a) * (R_HOOP + HOOP_T * 0.5 + 0.0010) + Vector((0, 0, HOOP_H * 0.18)) for a in angs[2:-2:2]]
    b.add(M.tube(rpts, [0.0024] * len(rpts), sides=3, dome0=0.0, dome1=0.0), "iron_line", rig, name="rule",
          outline=False, covered_by=hid)
    # nails standing out of the hoop, leaning outward (a broken halo of iron spikes)
    for a, ln in NAILS:
        if _in_gap(a):
            continue
        d = _dir(a)
        base = d * R_HOOP + Vector((0.0, 0.0, HOOP_H * 0.35))
        tip = base + (d * 0.42 + Vector((0, 0, 1.0))).normalized() * ln
        nail = M.tube([base, base.lerp(tip, 0.55), tip], [0.0085, 0.0065, 0.0015], sides=4, dome0=0.0, dome1=0.001)
        b.add(nail, "iron", rig, name="nail", hull_pad=True)
    # two shards drifting in the gap
    for a, z, s in ((140.0, 0.016, 1.0), (158.0, -0.010, 0.8)):
        d = _dir(a)
        c = d * (R_HOOP + 0.008) + Vector((0, 0, z))
        sh = M.box((0.012 * s, 0.026 * s, 0.016 * s), (0, 0, 0), bevel=0.0)
        sh.transform(Matrix.Translation(c) @ Matrix.Rotation(math.radians(a + 30.0), 4, "Z")
                     @ Matrix.Rotation(math.radians(24.0), 4, "X"))
        b.add(sh, "iron", rig, name="shard", hull_pad=True)
    # the ember at the front, in an iron claw on the hoop: a faceted bead (octahedron, stretched up)
    front = _dir(0.0)
    ec = front * (R_HOOP + 0.016) + Vector((0.0, 0.0, 0.026))
    claw_pts = []
    for k in range(3):
        aa = 2.0 * math.pi * k / 3.0
        side = Vector((math.cos(aa), 0.0, math.sin(aa)))
        p0 = front * (R_HOOP + 0.004) + Vector((side.x * 0.016, 0.0, side.z * 0.012 + 0.008))
        p1 = ec + Vector((side.x * 0.020, -0.004, side.z * 0.020))
        claw_pts.append((p0, p1))
    for p0, p1 in claw_pts:
        b.add(M.tube([p0, p0.lerp(p1, 0.5) + front * 0.004, p1], [0.0042, 0.0038, 0.0025], sides=4, dome0=0.0,
                     dome1=0.001), "iron", rig, name="claw", hull_pad=True)
    bm = bmesh.new()
    rx, ry, rz = EMBER_R * 0.85, EMBER_R * 0.85, EMBER_R * 1.25
    vs = [bm.verts.new(tuple(ec + v)) for v in (Vector((rx, 0, 0)), Vector((0, -ry, 0)), Vector((-rx, 0, 0)),
                                                 Vector((0, ry, 0)))]
    top = bm.verts.new(tuple(ec + Vector((0, 0, rz))))
    bot = bm.verts.new(tuple(ec - Vector((0, 0, rz * 0.8))))
    for i in range(4):
        bm.faces.new((vs[i], vs[(i + 1) % 4], top))
        bm.faces.new((vs[(i + 1) % 4], vs[i], bot))
    bm.normal_update()
    b.add(M.Part(bm).flat(), "core", rig, name="ember", hull_pad=True)
    obj = b.build(SIGIL, grounding_height=None)
    zmin = min(v.co.z for v in obj.data.vertices)
    # pivot 6 cm under the lowest point: the halo floats clear of the socket (the gnawers' fx_overhead sits close
    # over a hunched head, where a ring resting on the socket overlapped the head at the W1 camera)
    obj.data.transform(Matrix.Translation((0.0, 0.0, -zmin + SIGIL_FLOAT)))
    obj.data.update()
    obj["af_ember"] = list(ec + Vector((0.0, 0.0, -zmin + SIGIL_FLOAT)))
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {SIGIL}: {msg}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


def smooth(x: float) -> float:
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)


# ── the W1 crown recolour ──────────────────────────────────────────────────────────────────────────────────────
CROWN_KEYS = ("gold", "gold_light", "gold_rule", "gem", "gem_rim")


# review round 2: the A1 iron (#5E6066 at .42, the ball tips #8A8C90 at .45) lit a pale, clean silver crown at the
# game camera; tarnished iron keeps a low light band (L* ~50 / 55 lit) and carries patina pits — the crimson stone is
# the only accent. Appended swatches (the A1 ``elite_crown.*`` stay in the registry): (hex, shadow, light, emissive)
CROWN_TARNISH = {"elite_crown.iron": ("#55575B", 0.50, 0.22, 0.0),
                 "elite_crown.iron_hi": ("#66686B", 0.45, 0.20, 0.0),
                 "elite_crown.patina": ("#2E2B26", 0.45, 0.08, 0.0)}
CROWN_REMAP = {"elite_crown.gold": "elite_crown.iron", "elite_crown.gold_light": "elite_crown.iron_hi"}
# patina pits / streaks on the front arc (deg from the front, z of the centre, half-width deg, half-height m)
PATINA = [(-41.0, 0.050, 4.0, 0.012), (-28.0, 0.034, 2.6, 0.007), (-17.0, 0.050, 3.0, 0.016), (-6.0, 0.040, 2.2, 0.008),
          (8.0, 0.092, 2.4, 0.010), (19.0, 0.041, 3.4, 0.010), (33.0, 0.060, 2.6, 0.013), (46.0, 0.074, 3.0, 0.009),
          (55.0, 0.040, 2.4, 0.010), (-55.0, 0.074, 2.4, 0.010), (2.0, 0.012, 5.0, 0.005), (-60.0, 0.028, 3.0, 0.008)]


def _add_patina(b) -> None:
    """Dark pits and streaks painted on the outer face of the crown's front arc (single-sided, just proud)."""
    for a, zc, da, dz in PATINA:
        grid = []
        for zz in (zc - dz, zc + dz):
            row = []
            for aa in (a - da, a + da * 0.6, a + da):
                h = min(zz, EC.top_h(aa) - 0.004)
                row.append(EC._ring_dir(aa) * (EC._rad(h) + EC.TH * 0.5 + 0.0012) + Vector((0, 0, h)))
            grid.append(row)
        part = HB.sheet_grid(grid)
        f = next(iter(part.bm.faces))
        c = f.calc_center_median()
        if f.normal.dot(Vector((c.x, c.y, 0.0))) < 0:
            bmesh.ops.reverse_faces(part.bm, faces=list(part.bm.faces))
            part.bm.normal_update()
        b.add(part, "patina", Bind.rigid("root"), name="patina", outline=False)


def build_crown_march(pal: Palette) -> bpy.types.Object:
    """``elite_crown.build_crown`` with its regions on the march swatches and the asset renamed — the shape is the
    shipped W1 crown's. Review round 2: the iron re-mapped onto tarnished, low-light swatches (``CROWN_TARNISH``;
    the build uses the A1 names, then the faces move to the appended swatches) and patina pits painted on the
    front arc; the crimson stone (A1) is the only accent."""
    regs = _march_regions()
    saved = (EC.ASSET, EC.REGIONS, M.Builder.build)
    orig_build = M.Builder.build

    def build_with_patina(self, name, *a, **kw):
        if name == CROWN:
            _add_patina(self)
        return orig_build(self, name, *a, **kw)
    try:
        EC.ASSET = CROWN
        EC.REGIONS = {k: regs[f"elite_crown.{k}"] for k in CROWN_KEYS}
        hx, s_, l_, e_ = CROWN_TARNISH["elite_crown.patina"]
        EC.REGIONS["patina"] = Region(hx, s_, l_, e_, regs["elite_crown.gold_rule"].o)
        M.Builder.build = build_with_patina
        obj = EC.build_crown(pal)
    finally:
        EC.ASSET, EC.REGIONS, M.Builder.build = saved
    _remap_swatches(obj, pal, regs)
    return obj


def _remap_swatches(obj, pal: Palette, regs: dict) -> None:
    """Move the faces (toon and hull) on the A1 iron swatches to the appended tarnished ones and re-bake the UVs."""
    o = regs["elite_crown.gold"].o
    idx = {}
    for old, new in CROWN_REMAP.items():
        hx, s_, l_, e_ = CROWN_TARNISH[new]
        idx[pal._by_name[old]] = pal.add(new, Region(hx, s_, l_, e_, o))
    me = obj.data
    attr = me.attributes["af_swatch"]
    sw = np.zeros(len(me.polygons), np.int32)
    attr.data.foreach_get("value", sw)
    for a, b_ in idx.items():
        sw[sw == a] = b_
    attr.data.foreach_set("value", sw)
    pal.bake_uvs(me)
    me.update()
    shading.toon_material(pal)            # reload the atlases: the appended swatches are in them now
    shading.outline_material(pal)


def crown_fx() -> dict:
    fx = copy.deepcopy(EC.FX)
    fx["glow"]["color"] = GLOW
    fx["glow"]["alpha"] = 0.30
    fx["glow"]["note"] = "march recolour: a crimson glow (the plains crown's warm #FFD040 is ember-adjacent: DR5)"
    return fx


# ── review ─────────────────────────────────────────────────────────────────────────────────────────────────────
def ember_world(obj) -> Vector:
    return obj.matrix_world @ Vector(obj.get("af_ember", (0.0, -0.15, 0.03)))


def glow_cards(obj, fx: dict, at=None):
    g = fx["glow"]

    def fn(cam, res):
        c = at(obj) if at is not None else obj.matrix_world @ Vector((0, 0, 0.06))
        px, py = review.project_px(cam, c, res)
        wpp = outline.pixel_world_size(cam, c, res[0])
        return [dict(center_px=(px, py), radius_px=g["radiusCm"] / 100.0 / wpp, color=g["color"], alpha=g["alpha"],
                     core=None, core_alpha=0.0)]
    return fn


def carrier_review(obj, name: str, out: Path, fx: dict, at=None) -> review.Review:
    carrier = bpy.data.objects.new(f"AF_Carrier_{name}", None)
    scene.link(carrier)
    obj.parent = carrier
    rv = review.Review(name, carrier, [obj], 0.16, blob_radius=0.0, out_dir=out,
                       game_outline_px=outline.SCREEN_PX_1080[OUTLINE_CLASS], outline_class=OUTLINE_CLASS,
                       glows=glow_cards(obj, fx, at))
    rv.ground_occludes = True
    return rv


OVER = [("SK_Mon_RisenFootman", "FOOTMAN"), ("SK_Mon_OathZealot", "ZEALOT"), ("SK_Mon_Gravegnawer", "GNAWER"),
        ("SK_Mon_Bonegorger", "BONEGORGER"), ("SK_Mon_Valdras", "VALDRAS")]
GAP_MIN_PX = 4                    # review round 2: the marker floats clear of the creature by >= 4 px at 1080p
GNAWERS = (("SK_Mon_Gravegnawer", "GNAWER"), ("SK_Mon_Bonegorger", "BONEGORGER"))
GAP_FACINGS = ("se", "sw", "ne", "nw")


def _mask(objs, box, hide) -> np.ndarray:
    """Coverage (body + visible ink hull) of ``objs`` over ``box`` with ``hide`` out of the render."""
    for o in hide:
        o.hide_render = True
    try:
        b, h = review.coverage_pass(objs, box)
    finally:
        for o in hide:
            o.hide_render = False
    return (b + h) > 0.5


def marker_gap_px(fig_objs, mk, box) -> float:
    """Clear background pixels between the marker (with its ink) and the creature (body, held items, ink) at the
    current camera / pose: the smallest pixel-centre distance between the two masks − 1 (0 = touching; negative =
    overlapping)."""
    fm = _mask(fig_objs, box, [mk])
    mm = _mask([mk], box, fig_objs)
    if not mm.any() or not fm.any():
        return 99.0
    if (fm & mm).any():
        return -1.0
    my, mx = np.nonzero(mm)
    y0, y1, x0, x1 = my.min() - 40, my.max() + 40, mx.min() - 40, mx.max() + 40
    fy, fx = np.nonzero(fm[max(0, y0):y1, max(0, x0):x1])
    if not len(fy):
        return 99.0
    fy, fx = fy + max(0, y0), fx + max(0, x0)
    # boundary pixels only (cheap): the marker's and the figure's edge pixels
    best = 1e9
    for k in range(0, len(my), 512):
        d = np.hypot(my[k:k + 512, None] - fy[None, :], mx[k:k + 512, None] - fx[None, :])
        best = min(best, float(d.min()))
    return round(best - 1.0, 1)


def over_monsters(out_dir: Path, which: str) -> Path:
    """``<which>_over_monsters.png``: the marker at ``fx_overhead`` over shipped march monsters (re-imported from
    their FBX by ``coherence/cohkit``, Idle frame 0), native 1080p crops at the W1 camera on the march ash ground,
    the glow card on; each cell ink-measured (body + marker)."""
    import cohkit
    from kit import pngio
    import unburied as UB
    UB.march_stage()
    cells = []
    man = cohkit.manifest()
    runs = []
    for asset, label in OVER:
        if asset not in man["assets"]:
            continue
        if man["assets"][asset].get("eliteMarker") == "none":
            # review round 2: the story boss opts out (manifest eliteMarker none: the visor slit is his marker) —
            # shown bare, then with the marker on the fallback socket fx_elite (forward of the helm), never on
            # fx_overhead (it projected onto the standard's pole)
            runs += [(asset, label, None), (asset, label, "fx_elite")]
        else:
            runs.append((asset, label, "fx_overhead"))
    for asset, label, sock in runs:
        scene.reset()
        pal = Palette(FAMILY)
        review.setup_render((1920, 1080), 8)
        cam = review.game_camera()
        fig = cohkit.import_character(asset, (0.0, 0.0, 0.0), 0.0, cam=cam)
        mk = build_sigil(pal) if which == "sigil" else build_crown_march(pal)
        e = cohkit.entry(asset)
        wp = cohkit._socket_world(e, fig.arm, sock or "fx_overhead")
        if which == "sigil":
            mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.radians(-20.0), 4, "Z")
        else:
            dv = cam.matrix_world.translation - wp
            mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.atan2(dv.x, -dv.y), 4, "Z")
        bpy.context.view_layer.update()
        fx = SIGIL_FX if which == "sigil" else crown_fx()
        at = ember_world if which == "sigil" else None
        rv = review.Review(asset, fig.root, fig.all() + [mk], max(1.0, fig.height_m), blob_radius=0.3,
                           out_dir=out_dir, game_outline_px=outline.SCREEN_PX_1080["monster"],
                           outline_class="monster", glows=glow_cards(mk, fx, at))
        rv.ground_occludes = True
        rv.facing("se")
        bpy.context.view_layer.update()
        if which == "sigil":                       # the facing turned the figure: keep the sigil level on the socket
            wp = cohkit._socket_world(e, fig.arm, sock or "fx_overhead")
            mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.radians(-20.0), 4, "Z")
        else:
            wp = cohkit._socket_world(e, fig.arm, sock or "fx_overhead")
            dv = cam.matrix_world.translation - wp
            mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.atan2(dv.x, -dv.y), 4, "Z")
        if sock is None:
            mk.hide_render = True
            rv.glows = None
        bpy.context.view_layer.update()
        for m in fig.meshes:
            px = float(m.get("af_px1080", fig.px))
            review.set_outline_px([m], px, Vector((0, 0, 0.8)))
        review.set_outline_px([mk], outline.SCREEN_PX_1080[OUTLINE_CLASS], wp)
        c = review.project(cam, wp * 0.55)
        hw = 0.075 * max(1.0, (wp.z + 0.3) / 1.8)
        hh = hw * 1920 / 1080
        box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
        if sock is None:
            rv.meshes = [m for m in rv.meshes if m is not mk]
        img, st = rv.game_shot(box, measure=True, tag=f"{which} over {asset}" + (f" @{sock}" if sock else ""))
        name = 'SIGIL' if which == 'sigil' else 'CROWN MARCH'
        if sock is None:
            review.draw_text(img, 3, 3, f"{label}: NO MARKER (ELITEMARKER NONE)")
        else:
            gap = marker_gap_px(fig.all(), mk, box)
            review.draw_text(img, 3, 3, f"{label} + {name}" + (" @FX_ELITE (FALLBACK, OFF)" if sock == "fx_elite"
                                                               else ""))
            review.draw_text(img, 3, 13, f"GAP {gap:g}PX " + ("OK" if gap >= GAP_MIN_PX else "< 4PX"),
                             rgb=(160, 230, 160) if gap >= GAP_MIN_PX else (240, 90, 90))
            print(f"  [gap] {which} over {asset} @{sock}: {gap:g} px")
        review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
        cells.append(img)
    h = max(c.shape[0] for c in cells)
    cells = [np.pad(c, ((0, h - c.shape[0]), (0, 0), (0, 0)), constant_values=24) for c in cells]
    out_dir.mkdir(parents=True, exist_ok=True)
    p = out_dir / f"{which}_over_monsters.png"
    written = pngio.write_png_native(p, review.grid(cells, len(cells)))
    for q, sz in written:
        print(f"  {q.name} {sz // 1024} KB")
    return p


def _place(mk, which: str, wp: Vector, cam) -> None:
    if which == "sigil":
        mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.radians(-20.0), 4, "Z")
    else:
        dv = cam.matrix_world.translation - wp
        mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.atan2(dv.x, -dv.y), 4, "Z")
    bpy.context.view_layer.update()


def over_gnawers_facings(out_dir: Path, which: str) -> dict:
    """``<which>_over_gnawers_facings.png`` (review round 2): the marker on ``fx_overhead`` over the gravegnawer and
    the bonegorger (shipped FBX, Idle frame 0) in the four diagonal facings at the W1 camera, native 1080p, each cell
    labelled with the measured clear gap between the marker and the creature (ink included; gate ``GAP_MIN_PX``)."""
    import cohkit
    from kit import pngio
    import unburied as UB
    UB.march_stage()
    cells, gaps = [], {}
    for asset, label in GNAWERS:
        for fac in GAP_FACINGS:
            scene.reset()
            pal = Palette(FAMILY)
            review.setup_render((1920, 1080), 8)
            cam = review.game_camera()
            fig = cohkit.import_character(asset, (0.0, 0.0, 0.0), 0.0, cam=cam)
            mk = build_sigil(pal) if which == "sigil" else build_crown_march(pal)
            e = cohkit.entry(asset)
            fx = SIGIL_FX if which == "sigil" else crown_fx()
            rv = review.Review(asset, fig.root, fig.all() + [mk], max(1.0, fig.height_m), blob_radius=0.3,
                               out_dir=out_dir, game_outline_px=outline.SCREEN_PX_1080["monster"],
                               outline_class="monster",
                               glows=glow_cards(mk, fx, ember_world if which == "sigil" else None))
            rv.ground_occludes = True
            rv.facing(fac)
            wp = cohkit._socket_world(e, fig.arm, "fx_overhead")
            _place(mk, which, wp, cam)
            for m in fig.meshes:
                review.set_outline_px([m], float(m.get("af_px1080", fig.px)), Vector((0, 0, 0.8)))
            review.set_outline_px([mk], outline.SCREEN_PX_1080[OUTLINE_CLASS], wp)
            c = review.project(cam, wp * 0.55)
            hw = 0.075 * max(1.0, (wp.z + 0.3) / 1.8)
            hh = hw * 1920 / 1080
            box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
            img, _ = rv.game_shot(box)
            gap = marker_gap_px(fig.all(), mk, box)
            gaps[f"{asset}:{fac}"] = gap
            img = np.ascontiguousarray(img)
            review.draw_text(img, 3, 3, f"{label} {fac.upper()} + {'SIGIL' if which == 'sigil' else 'CROWN MARCH'}")
            review.draw_text(img, 3, 13, f"GAP {gap:g}PX " + ("OK" if gap >= GAP_MIN_PX else "< 4PX"),
                             rgb=(160, 230, 160) if gap >= GAP_MIN_PX else (240, 90, 90))
            cells.append(img)
            print(f"  [gap] {which} over {asset} {fac}: {gap:g} px")
    h = max(c.shape[0] for c in cells)
    w = max(c.shape[1] for c in cells)
    cells = [np.pad(c, ((0, h - c.shape[0]), (0, w - c.shape[1]), (0, 0)), constant_values=24) for c in cells]
    out_dir.mkdir(parents=True, exist_ok=True)
    p = out_dir / f"{which}_over_gnawers_facings.png"
    for q, sz in pngio.write_png_native(p, review.grid(cells, len(GAP_FACINGS))):
        print(f"  {q.name} {sz // 1024} KB")
    return gaps


def patch_gnawer_sockets() -> dict:
    """Write the gnawers' new ``fx_overhead`` (``gnawers.GnawerKit._place_overhead``: on ``root``, centred over the
    feet, ``OVERHEAD_K`` × the head centre's height) into their manifest entries — skeletal sockets are created in UE
    from the manifest, so the shipped FBX stand; the next gravegnawer / bonegorger ship writes the same socket."""
    import gnawers as GN
    import gravegnawer as GG
    import bonegorger as BG
    from kit import export, rig as R
    out = {}
    for mod, cls in ((GG, GN.GnawerKit), (BG, BG.BonegorgerKit)):
        scene.reset()
        kit = cls(mod.ASSET, mod.PREFIX, mod.WEB, mod.LOOK, mod.SHAPE)
        ent = next(x for x in R.socket_manifest(kit.rig) if x["name"] == "fx_overhead")
        man = export.Manifest()
        socks = man.data["assets"][mod.ASSET]["sockets"]
        for i, x in enumerate(socks):
            if x["name"] == "fx_overhead":
                socks[i] = ent
        man.save()
        out[mod.ASSET] = ent
        print(f"[socket] {mod.ASSET} fx_overhead -> {ent}")
    return out


def review_set(obj, name: str, out: Path, fx: dict, at=None, facing: str = "front") -> dict:
    """Close-ups, turnaround and the measured 1080p game view **without** the glow card (the ink gate measures the
    mesh, as ``elite_crown.ship``), then ``game_glow.png``: the same crop with the runtime card."""
    rv = carrier_review(obj, name, out, fx, at)
    cards = rv.glows
    rv.glows = None
    rv.game_view(facing=facing)
    rv.glows = cards
    review.setup_render((1920, 1080), 8)
    rv.facing(facing)
    cam = review.game_camera()
    box = rv.game_crop_box(cam)
    rv.game_outline(1080)
    img, _ = rv.game_shot(box)
    review.draw_text(img, 4, 4, "1080P 1:1 + RUNTIME GLOW CARD")
    rv._save("game_glow.png", img)
    rv.closeup("front", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    rv.closeup("se", pitch=-50.0)
    (out / "closeup.png").rename(out / "closeup_w1.png")
    rv.turnaround()
    v = rv.ink_verdict()
    print(f"[ink] {name}: {v['cells']} cells, min rim {v['minFracRim'] * 100:.1f} %, min dark "
          f"{v['minFracDark'] * 100:.1f} % -> {'PASS' if v['pass'] else 'FAIL'}")
    for f in v["failures"]:
        print("  INK FAIL " + f)
    obj.parent = None
    return v


# ── ship ───────────────────────────────────────────────────────────────────────────────────────────────────────
def ship(previews: bool = True) -> int:
    from kit import export, paths
    ok = True
    for name in (SIGIL, CROWN):
        scene.reset()
        pal = Palette(FAMILY)
        obj = build_sigil(pal) if name == SIGIL else build_crown_march(pal)
        pal.save()
        print(f"[build] {name}: {M.tri_count(obj, 0)} toon + {M.tri_count(obj, 1)} hull tris")
        if name == SIGIL:
            sockets = [{"name": "base", "pos": (0.0, 0.0, 0.0)},
                       {"name": "ember", "pos": tuple(obj["af_ember"])}]
            fx, ids = SIGIL_FX, ["elite_sigil"]
            extra = {"attachSocket": "fx_overhead", "fx": fx, "widthCm": round(2 * (R_HOOP + 0.06) * 100, 1),
                     "displayName": {"zh-CN": "精英印记", "en": "Elite sigil"}, "wave": "W2",
                     "replaces": "SM_FX_EliteCrown / SM_FX_EliteCrown_March (W2)",
                     "palette": "Monsters_March elite_sigil.* (A1)", "previews": []}
        else:
            sockets = [{"name": "base", "pos": (0.0, 0.0, 0.0)}]
            fx, ids = crown_fx(), ["elite_crown@march"]
            extra = {"attachSocket": "fx_overhead", "fx": fx, "theme": "march", "base": "SM_FX_EliteCrown",
                     "widthCm": round(2 * (EC._rad(EC.PROFILE[0][1]) + EC.TH * 0.5) * 100, 1),
                     "displayName": {"zh-CN": "精英王冠（焚境）", "en": "Elite crown (march)"}, "wave": "W1",
                     "palette": "Monsters_March elite_crown.* (A1: tarnished iron, crimson stone)", "previews": []}
        fbx = export.export_static_mesh(obj, name, "VFX", sockets)
        man = export.Manifest()
        man.set_palette(pal)
        entry = export.static_entry(obj, fbx, "VFX", pal, OUTLINE_CLASS, ids, sockets, extra=extra)
        entry["materialSlots"][1]["outlineMix"] = outline.INK_MIX[OUTLINE_CLASS]
        man.set_asset(name, entry)
        man.save()
        if not previews:
            continue
        out = paths.preview_dir(name)
        out.mkdir(parents=True, exist_ok=True)
        for f in out.glob("*.png"):
            f.unlink()
        v = review_set(obj, name, out, fx, ember_world if name == SIGIL else None)
        ok = ok and v["pass"]
        man = export.Manifest()
        man.data["assets"][name]["inkGate"] = {k: v[k] for k in ("pass", "cells", "minFracRim", "minFracDark",
                                                                 "medianRimPx", "maxHullLumP95", "gate")}
        man.save()
        over_monsters(out, "sigil" if name == SIGIL else "crown")
        over_gnawers_facings(out, "sigil" if name == SIGIL else "crown")
        man = export.Manifest()
        man.data["assets"][name]["previews"] = [f"{name}/{p.name}" for p in sorted(out.glob("*.png"))]
        man.save()
    return 0 if ok else 1


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if "--patch-gnawer-sockets" in args:
        patch_gnawer_sockets()
    elif "--gaps" in args:
        out = scratch_root("a8", "gaps")
        for w_ in ("sigil", "crown"):
            over_gnawers_facings(out, w_)
    elif "--quick" in args:
        out = scratch_root("a8", "sigil")
        for name in (SIGIL, CROWN):
            scene.reset()
            pal = Palette(FAMILY)
            obj = build_sigil(pal) if name == SIGIL else build_crown_march(pal)
            print(f"[build] {name}: {M.tri_count(obj, 0)} toon + {M.tri_count(obj, 1)} hull tris")
            o = out / name
            o.mkdir(parents=True, exist_ok=True)
            review_set(obj, name, o, SIGIL_FX if name == SIGIL else crown_fx(),
                       ember_world if name == SIGIL else None)
        if "--over" in args:
            over_monsters(out, "sigil")
            over_monsters(out, "crown")
    else:
        sys.exit(ship(previews="--no-previews" not in args))
