"""The dark remake's elite marker (monsters.md §3; implementation-plan.md A8): ``SM_FX_EliteSigil`` (W2) and the W1
recolour ``SM_FX_EliteCrown_March`` (``elite_crown@march``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/elite_sigil.py
        (no flag) ship both · --quick (close-ups / turnaround / game crop → scratch) · --no-previews
        --gaps (the gnawer facing sheets → scratch) · --patch-gnawer-sockets (manifest fx_overhead of the gnawers)
        --solve-elite [--only=FOOTMAN,…] [--dry-run] (Wave 2: fx_elite per asset → fx_elite_w2.json + manifest)
        --roster [--scratch] [--only=…] (Wave 2: the sigil on fx_elite over the whole march roster, four facings →
        sigil_over_roster* + the manifest clearance) · --over (sigil_over_monsters / _gnawers_facings and the crown's,
        in place, no re-export)

* ``SM_FX_EliteSigil`` (W2, game id ``elite_sigil``): "a small floating broken iron halo with a crimson ember" — a
  29 cm hoop of dark iron (``elite_sigil.iron`` #4A4C52, the rule ``elite_sigil.iron_line``) crowned with seven
  short iron nails of uneven length, broken open at the back (the ends torn and bent, two shards drifting in the
  gap), and at the front a faceted ember held in an iron claw (``elite_sigil.core`` #C0283A, emissive .9: crimson,
  never orange). It floats level 6 cm over its own socket ``fx_elite`` (Wave 2: ``--solve-elite``; the W1 camera's
  −50° sees a level ring as an open ellipse), bobs ±3 cm about that rest and turns slowly about its axis (manifest
  ``fx.motion``); the crimson glow is a runtime card (``fx.glow``).
* ``SM_FX_EliteCrown_March`` (W1, ``elite_crown@march``): ``elite_crown.build_crown`` unchanged in shape, its
  regions mapped onto ``Monsters_March`` swatches — review round 2: tarnished, low-light iron (``elite_crown.iron``
  #55575B at .22, the ball tips ``elite_crown.iron_hi`` #66686B at .20; the A1 #5E6066 / #8A8C90 lit a clean silver
  crown), dark patina pits on the front arc (``elite_crown.patina``), the crimson stone #C0283A the only accent — the
  marker the W1 runtime draws over every elite in the march zone (U1 resolves ``<id>@march``).

The W1 crown rides ``fx_overhead`` (over the gnawers their A8 one: ``gnawers.GnawerKit._place_overhead``, on
``root``, centred over the feet, 1.57 × the head centre's height; ``--patch-gnawer-sockets``), the W2 sigil rides
``fx_elite`` (Wave 2 art review: fx_overhead also carries the nameplates, floating text, stun stars and the death
mark, so the sigil's clearance got a socket of its own): the ``*_over_gnawers_facings.png`` sheets measure the clear
gap in se / sw / ne / nw (gate ``GAP_MIN_PX`` 4 px at 1080p). Valdras opts out (manifest ``eliteMarker: "none"``):
``*_over_monsters`` show him bare, then with the marker on his fallback socket ``fx_elite``.

Both ink with the monster class (3.0 px at 1080p): the weight of the elite they mark. ``elite_crown.py`` is only
read (its builder runs with this module's regions).
"""
from __future__ import annotations

import copy
import json
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
SIGIL_FLOAT = 0.06                # the hoop's lowest point floats this far above its socket (the pivot)
GLOW = "#C0283A"

SIGIL_FX = {
    "glow": {"color": GLOW, "alpha": 0.38, "radiusCm": 12.0, "blend": "additive", "falloff": "web glow",
             "socket": "ember", "pulse": {"periodMs": 1400, "alphaMin": 0.24},
             "note": "crimson (monsters.md §3: a crimson ember — never orange; DR5)"},
    "motion": {"bobCm": 3.0, "bobPeriodMs": 1600, "bob": "symmetric", "spinDegPerSec": 45.0, "faceCameraYaw": False,
               "note": "W2: replaces SM_FX_EliteCrown over elites; floats level 6 cm over fx_elite, bobs as a symmetric "
                       "sine of bobCm about that rest (rest - bobCm .. rest + bobCm: the clearance gate is measured at "
                       "the trough) and turns slowly about its own axis (the break and the shards come round every 8 s)"},
    "socket": "fx_elite", "absoluteScale": True,
    "socketNote": "fx_elite (Wave 2), not fx_overhead: fx_overhead carries the nameplates, floating text, stun stars, "
                  "the death mark and the W1 crown and stays where Wave 1 shipped it; fx_elite is solved per asset "
                  "(clearance + closeness, elite_sigil.py --solve-elite). An asset without fx_elite falls back to "
                  "fx_overhead",
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
    """``<which>_over_monsters.png``: the marker on its socket (``marker_socket``: the sigil ``fx_elite``, the crown
    ``fx_overhead``) over shipped march monsters (re-imported from their FBX by ``coherence/cohkit``, Idle frame 0),
    native 1080p crops at the W1 camera on the march ash ground, the glow card on; each cell ink-measured (body +
    marker)."""
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
        else:                    # Wave 2: the sigil rides fx_elite, the W1 crown fx_overhead
            runs.append((asset, label, marker_socket(man["assets"][asset], which)))
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
            off = man["assets"][asset].get("eliteMarker") == "none"
            at = f" @{sock.upper()}" if which == "sigil" or off else ""      # the crown's W1 labels unchanged
            review.draw_text(img, 3, 3, f"{label} + {name}{at}" + (" (FALLBACK, OFF)" if off else ""))
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


def marker_socket(entry: dict, which: str) -> str:
    """The socket a marker rides on an asset: the W2 sigil ``fx_elite`` (when the asset has one), the W1 crown
    ``fx_overhead``."""
    if which == "sigil" and any(s["name"] == "fx_elite" for s in entry.get("sockets", [])):
        return "fx_elite"
    return "fx_overhead"


def _place(mk, which: str, wp: Vector, cam) -> None:
    if which == "sigil":
        mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.radians(-20.0), 4, "Z")
    else:
        dv = cam.matrix_world.translation - wp
        mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.atan2(dv.x, -dv.y), 4, "Z")
    bpy.context.view_layer.update()


def over_gnawers_facings(out_dir: Path, which: str) -> dict:
    """``<which>_over_gnawers_facings.png`` (review round 2): the marker on its socket (``marker_socket``) over the
    gravegnawer and the bonegorger (shipped FBX, Idle frame 0) in the four diagonal facings at the W1 camera, native
    1080p, each cell labelled with the measured clear gap between the marker and the creature (ink included; gate
    ``GAP_MIN_PX``)."""
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
            wp = cohkit._socket_world(e, fig.arm, marker_socket(e, which))
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
            review.draw_text(img, 3, 3, f"{label} {fac.upper()} + {'SIGIL' if which == 'sigil' else 'CROWN MARCH'}"
                             + (f" @{marker_socket(e, which).upper()}" if which == "sigil" else ""))
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


# Wave 2 art (2H): the sigil replaces the crown over every elite of the march (monsters.md 3), so its clearance is
# measured over the whole Chapter 1 roster that can carry it — every regular (a champion pack or the Hollow Watch
# leader can be any of them), the hunt leaders at their x1.25 (absolute-scale marker on a scaled actor) and the
# mini-boss; Valdras opts out (eliteMarker none).
ROSTER = [("SK_Mon_RisenFootman", None, "FOOTMAN"), ("SK_Mon_RisenArbalest", None, "ARBALESTER"),
          ("SK_Mon_RisenShieldbearer", None, "SHIELDBEARER"), ("SK_Mon_DeadStandardBearer", None, "STANDARD-BEARER"),
          ("SK_Mon_OathZealot", None, "ZEALOT"), ("SK_Mon_Deserter", None, "DESERTER"),
          ("SK_Mon_DeserterDrummer", None, "DRUMMER"), ("SK_Mon_Deserter", "hunt_dreck", "DRECK"),
          ("SK_Mon_Deserter", "hunt_harke", "HARKE"), ("SK_Mon_Gravegnawer", None, "GNAWER"),
          ("SK_Mon_Bonegorger", None, "BONEGORGER"), ("SK_Mon_Bonegorger", "hunt_hungerbell", "HUNGERBELL"),
          ("SK_Mon_Breachclot", None, "BREACHCLOT"), ("SK_Mon_Cinderbloat", None, "CINDERBLOAT"),
          ("SK_Mon_Corpseherd", None, "CORPSEHERD")]

# Wave 2 (2H, art review round 1): the sigil rides its own socket, ``fx_elite``. The roster sweep found the kit's
# fx_overhead (crown + 25 cm on the head bone; root / body_top on the gnawers and clots) too low for the march's
# hunched figures: the hoop's near rim lands on the helm / hood in the back views, on the clots' dome and on the
# standard-bearer's banner. A first pass moved fx_overhead itself, but UE hangs much more there — the nameplates and
# floating text (AbyssWorldBuilder OverheadLocation), the status.stun stars and the skill.death_mark skull (recipe
# height 'overhead'), the W1 crown (EAbyssAnchor::Overhead) — so fx_overhead stays as Wave 1 shipped it and the
# sigil gets ``fx_elite`` (the name Valdras' opted-out fallback already had). ``solve_elite`` takes, per asset (its
# hunt variants included), the candidate nearest the W1 fx_overhead (Idle F0 pose, lateral offsets weighted most,
# then forward) that passes, at the W1 camera, native 1080p, Idle F0:
#   * clearance: >= GAP_MIN_PX + SOLVE_MARGIN_PX between the sigil (ink included) and the figure in se / sw / ne / nw
#     at the TROUGH of its bob (fx.motion: a symmetric sine of bobCm about the rest offset, the rest - bobCm);
#   * belonging: <= GAP_CAP_PX at rest in se / sw (a marker 30-100 px off the head reads as a neighbour's in a crowd).
# The result is ``monsters/fx_elite_w2.json`` (``mship.ship`` writes it on every re-ship) and the manifest socket
# (UE creates skeletal sockets from the manifest's relLocCm, so the shipped SK FBX stand).
ELITE_SOCKET = "fx_elite"
ELITE_TABLE = HERE / "fx_elite_w2.json"
BOB_CM = float(SIGIL_FX["motion"]["bobCm"])
SOLVE_MARGIN_PX = 1
GAP_CAP_PX = 20
CAP_FACINGS = ("se", "sw")
ELITE_UP_CM = tuple(range(0, 64, 4))                 # over the W1 fx_overhead (posed)
ELITE_LAT_CM = (0, 8, -8, 16, -16, 24, -24)          # + toward the character's left
ELITE_LAT_EXTRA = {"SK_Mon_DeadStandardBearer": (32, -32, 40, -40, 48, -48)}   # wider: the banner by his head
# the grid stays over the W1 socket (up >= 0): opened to the helm's height (up -20 .. -4, 56-64 cm to the side) the
# nearest-on-screen pick for the standard-bearer was a sigil floating beside his helm 69 cm to his left (ne gap 51 px)
# — in the Muster (a bearer and three footmen) it reads as a neighbour's marker; over the banner side reads as his
ELITE_UP_EXTRA: dict = {}
# the cheapest passing candidate floats far over the head when a held prop blocks the near ones (the standard-bearer's
# banner): there every candidate is walked and the one nearest the head on screen (se / sw) wins
ELITE_CLOSEST = ("SK_Mon_DeadStandardBearer",)
ELITE_FWD_CM = tuple(range(-24, 64, 8))              # + forward (Valdras' fallback: 36 cm forward of his standard)
COST_W = (1.6, 1.25, 1.0)                            # lateral, forward, up: cm -> cost
SOLVE_RES = (1920, 1080)


def _rest_world(arm, bone: str, rest_bl: Vector) -> Vector:
    """World position of a rest-space (Blender armature, m) point carried by ``bone`` in the current pose."""
    b, pb = arm.data.bones[bone], arm.pose.bones[bone]
    return arm.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ rest_bl))


def _pose_to_rest(arm, bone: str, pose_bl: Vector) -> Vector:
    """The rest-space point that ``bone`` carries to armature-space ``pose_bl`` in the current pose."""
    b, pb = arm.data.bones[bone], arm.pose.bones[bone]
    return b.matrix_local @ (pb.matrix.inverted() @ pose_bl)


def _elite_candidates(arm, sk: dict, asset: str = "") -> list:
    """(bone, rest m, label, cost, offset cm) around the W1 fx_overhead in the Idle F0 pose (armature space: +X the
    character's left, -Y forward), cheapest first: on ``root`` (level and steady: the hoop floats level by its motion
    contract, and a root socket has no rotation to cancel; it does not nod or droop with the head) and on
    fx_overhead's own bone, the offsets (left, forward, up) of ELITE_LAT_CM x ELITE_FWD_CM x ELITE_UP_CM; cost = the
    COST_W-weighted distance from the W1 socket (+0.5 off root: root wins a tie)."""
    rest0 = Vector((sk["restLocCm"][0] / 100.0, -sk["restLocCm"][1] / 100.0, sk["restLocCm"][2] / 100.0))
    b0, pb0 = arm.data.bones[sk["bone"]], arm.pose.bones[sk["bone"]]
    pose0 = pb0.matrix @ (b0.matrix_local.inverted() @ rest0)
    bones = ["root"] + ([sk["bone"]] if sk["bone"] != "root" else [])
    out = []
    for bone in bones:
        for up in ELITE_UP_EXTRA.get(asset, ()) + ELITE_UP_CM:
            for lat in ELITE_LAT_CM + ELITE_LAT_EXTRA.get(asset, ()):
                for fwd in ELITE_FWD_CM:
                    pose = pose0 + Vector((lat / 100.0, -fwd / 100.0, up / 100.0))
                    rest = _pose_to_rest(arm, bone, pose)
                    cost = math.sqrt((COST_W[0] * lat) ** 2 + (COST_W[1] * fwd) ** 2 + (COST_W[2] * up) ** 2) \
                        + (0.5 if bone != "root" else 0.0)
                    out.append((bone, rest, f"{bone} left {lat} fwd {fwd} up {up}", cost, (lat, fwd, up)))
    out.sort(key=lambda c: c[3])
    return out


def _px_box(cam, pts, pad: int) -> tuple[int, int, int, int]:
    """Integer pixel box (x0, y0, x1, y1; y down) round the projected ``pts`` at SOLVE_RES, ``pad`` px wider."""
    W, H = SOLVE_RES
    xs, ys = [], []
    for p in pts:
        u, v = review.project(cam, p)
        xs.append(u * W)
        ys.append((1.0 - v) * H)
    x0, x1 = max(0, int(min(xs)) - pad), min(W, int(max(xs)) + pad + 1)
    y0, y1 = max(0, int(min(ys)) - pad), min(H, int(max(ys)) + pad + 1)
    return x0, y0, x1, y1


def _border(box) -> tuple[float, float, float, float]:
    W, H = SOLVE_RES
    x0, y0, x1, y1 = box
    return x0 / W, (H - y1) / H, x1 / W, (H - y0) / H


def _cov(objs, box) -> np.ndarray:
    b, h = review.coverage_pass(objs, _border(box))
    m = (b + h) > 0.5
    want = (box[3] - box[1], box[2] - box[0])
    if m.shape != want:                    # Blender rounds the border to pixels: keep both masks on one grid
        out = np.zeros(want, bool)
        hh, ww = min(want[0], m.shape[0]), min(want[1], m.shape[1])
        out[:hh, :ww] = m[:hh, :ww]
        m = out
    return m


def _edge(m: np.ndarray) -> np.ndarray:
    """(y, x) of the mask's boundary pixels."""
    pad = np.pad(m, 1)
    inner = pad[1:-1, 1:-1] & pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:]
    return np.argwhere(m & ~inner)


def _gap(fig_mask: np.ndarray, fig_edge: np.ndarray, mk_mask: np.ndarray) -> float:
    """The clear background pixels between two masks on one grid (``marker_gap_px``'s metric: the smallest
    pixel-centre distance - 1; -1 = overlapping; 99 = no figure pixel in the box)."""
    if not mk_mask.any():
        return 99.0
    if (fig_mask & mk_mask).any():
        return -1.0
    if not len(fig_edge):
        return 99.0
    me = _edge(mk_mask)
    best = 1e9
    for k in range(0, len(me), 256):
        d = np.hypot(me[k:k + 256, None, 0] - fig_edge[None, :, 0], me[k:k + 256, None, 1] - fig_edge[None, :, 1])
        best = min(best, float(d.min()))
    return round(best - 1.0, 1)


def _solve_views(asset: str, variant, e: dict, sk: dict, cands_rest: list) -> dict:
    """Import ``asset`` (``variant``) at Idle F0 and, per facing, cache what the candidate walk needs without the
    figure in the scene: every candidate's world position, a pixel box round them all and the figure's coverage mask
    (body, held items, ink; the variant's W1 crown hidden) in that box."""
    import cohkit
    from kit import scene as S
    scene.reset()
    review.setup_render(SOLVE_RES, 4)
    cam = review.game_camera()
    fig = cohkit.import_character(asset, (0.0, 0.0, 0.0), 0.0, variant=variant, cam=cam)
    for o in fig.extra:
        o.hide_render = True
    fig.extra = []
    rv = review.Review(asset, fig.root, fig.all(), max(1.0, fig.height_m), blob_radius=0.3,
                       out_dir=scratch_root("a8", "solve"), game_outline_px=outline.SCREEN_PX_1080["monster"],
                       outline_class="monster")
    rel0 = S.blender_to_ue_cm(fig.arm.data.bones[sk["bone"]].matrix_local.inverted()
                              @ Vector((sk["restLocCm"][0] / 100.0, -sk["restLocCm"][1] / 100.0,
                                        sk["restLocCm"][2] / 100.0)))
    if max(abs(a - b) for a, b in zip(rel0, sk["relLocCm"])) > 0.05:
        raise RuntimeError(f"{asset}: imported bone frame gives relLocCm {rel0}, manifest {sk['relLocCm']}")
    for o in fig.meshes:
        review.set_outline_px([o], float(o.get("af_px1080", fig.px)), Vector((0, 0, 0.8)))
    views = {}
    for fac in GAP_FACINGS:
        rv.facing(fac)
        bpy.context.view_layer.update()
        wps = [_rest_world(fig.arm, bone, rest) for bone, rest in cands_rest]
        drop = Vector((0.0, 0.0, BOB_CM / 100.0))
        # the hoop spans ~18 cm round the pivot and 6-17 cm over it: pad the box by the marker and the 40 px the
        # gap metric looks round it
        box = _px_box(cam, wps + [w - drop for w in wps], 96)
        fm = _cov(fig.all(), box)
        # the hoop's centre (pivot + float + ~5 cm) and the W1 fx_overhead (over the head) on screen, for "closest"
        lift = Vector((0.0, 0.0, SIGIL_FLOAT + 0.05))
        px = np.array([review.project_px(cam, w + lift, SOLVE_RES) for w in wps])
        head = np.array(review.project_px(cam, cohkit._socket_world(e, fig.arm, "fx_overhead"), SOLVE_RES))
        views[fac] = {"wps": wps, "box": box, "fig": fm, "edge": _edge(fm), "headPx": np.hypot(*(px - head).T)}
    out = {"views": views, "cam": tuple(review.project(cam, Vector((0.0, 0.0, 1.0))))}
    rel = {}
    for bone in {b for b, _ in cands_rest}:
        rel[bone] = fig.arm.data.bones[bone].matrix_local.inverted()
    out["toRel"] = rel
    return out


def _marker_mask(mk, wp: Vector, box) -> np.ndarray:
    _place(mk, "sigil", wp, None)
    review.set_outline_px([mk], outline.SCREEN_PX_1080[OUTLINE_CLASS], wp)
    return _cov([mk], box)


def solve_elite(only=(), cap: float | None = GAP_CAP_PX, closest=()) -> dict:
    """Per roster asset the cheapest ``fx_elite`` candidate (``_elite_candidates``) that clears every facing at the
    bob's trough and stays within GAP_CAP_PX in se / sw at rest, for the asset and its hunt variants together
    (assets labelled in ``closest`` / ELITE_CLOSEST: of every candidate that passes, the one whose hoop centre lies
    nearest the W1 fx_overhead on screen in se / sw — the walk does not stop at the first):
    ``{asset: {bone, posM, restLocCm, relLocCm, choice, offsetCm, gapsPx: {variant: {facing: {rest, trough}}}}}``."""
    import cohkit
    from kit import scene as S
    import unburied as UB
    UB.march_stage()
    groups: dict[str, list] = {}
    for asset, variant, label in ROSTER:
        if only and label not in only:
            continue
        groups.setdefault(asset, []).append(variant)
    out = {}
    for asset, variants in groups.items():
        e = cohkit.entry(asset)
        sk = next(x for x in e["sockets"] if x["name"] == "fx_overhead")
        scene.reset()
        fig = cohkit.import_character(asset, (0.0, 0.0, 0.0), 0.0)
        cands = _elite_candidates(fig.arm, sk, asset)
        cands_rest = [(c[0], c[1]) for c in cands]
        views = {v: _solve_views(asset, v, e, sk, cands_rest) for v in variants}
        # the walk: the marker alone in the scene (the figure masks are cached), cheapest candidate first
        scene.reset()
        review.setup_render(SOLVE_RES, 4)
        cam = review.game_camera()
        for v, vv in views.items():
            if max(abs(a - b) for a, b in zip(vv["cam"], review.project(cam, Vector((0.0, 0.0, 1.0))))) > 1e-6:
                raise RuntimeError("the game camera moved between the passes")
        pal = Palette(FAMILY)
        mk = build_sigil(pal)
        drop = Vector((0.0, 0.0, BOB_CM / 100.0))
        fails: dict[tuple, int] = {}
        chosen, tried = None, 0
        order = [(v, f) for v in variants for f in GAP_FACINGS]
        near = asset in {a for a, _, lbl in ROSTER if lbl in closest or a in ELITE_CLOSEST}
        best = None

        def head_px(i):
            return max(float(views[v]["views"][f]["headPx"][i]) for v in variants for f in CAP_FACINGS)
        for i, (bone, rest, label, cost, off) in enumerate(cands):
            if near and best is not None and head_px(i) >= best[0]:
                continue
            tried += 1
            gaps: dict = {}
            ok = True
            for v, fac in sorted(order, key=lambda k: -fails.get(k, 0)):
                vw = views[v]["views"][fac]
                wp = vw["wps"][i]
                g_t = _gap(vw["fig"], vw["edge"], _marker_mask(mk, wp - drop, vw["box"]))
                gaps.setdefault(v or "base", {})[fac] = {"trough": g_t}
                if g_t < GAP_MIN_PX + SOLVE_MARGIN_PX:
                    ok = False
                elif fac in CAP_FACINGS and cap is not None:
                    g_r = _gap(vw["fig"], vw["edge"], _marker_mask(mk, wp, vw["box"]))
                    gaps[v or "base"][fac]["rest"] = g_r
                    ok = g_r <= cap
                if not ok:
                    fails[(v, fac)] = fails.get((v, fac), 0) + 1
                    break
            if ok and near:
                best = (head_px(i), i, gaps)
                continue
            if ok:
                chosen = (i, gaps)
                break
        if near and best is not None:
            chosen = best[1:]
        if chosen is None:
            print(f"  [solve] {asset}: NO candidate passes ({tried} tried)", flush=True)
            out[asset] = {"solved": False}
            continue
        i, gaps = chosen
        bone, rest, label, cost, off = cands[i]
        # the rest gaps of ne / nw too (reported)
        for v in variants:
            for fac in GAP_FACINGS:
                gv = gaps[v or "base"][fac]
                if "rest" not in gv:
                    vw = views[v]["views"][fac]
                    gv["rest"] = _gap(vw["fig"], vw["edge"], _marker_mask(mk, vw["wps"][i], vw["box"]))
        rel = S.blender_to_ue_cm(views[variants[0]]["toRel"][bone] @ rest)
        out[asset] = {"solved": True, "bone": bone, "choice": label, "costCm": round(cost, 1),
                      "pick": "closest to the head on screen (se / sw)" if near else "cheapest",
                      "headDistPx": round(head_px(i), 1),
                      "offsetCm": {"left": off[0], "forward": off[1], "up": off[2],
                                   "frame": "from the W1 fx_overhead, Idle F0 pose, character frame"},
                      "posM": [round(x, 5) for x in rest], "restLocCm": S.blender_to_ue_cm(rest), "relLocCm": rel,
                      "gapsPx": gaps, "tried": tried}
        print(f"[solve] {asset}: {label} (cost {cost:.1f}, {tried} tried) "
              + " ".join(f"{vk}:{fk}={g['rest']:g}/{g['trough']:g}" for vk, d in gaps.items() for fk, g in d.items()),
              flush=True)
    return out


def elite_table() -> dict:
    """``fx_elite_w2.json`` (asset -> fx_elite), empty when absent."""
    if ELITE_TABLE.exists():
        return json.loads(ELITE_TABLE.read_text())["assets"]
    return {}


def write_elite(solved: dict) -> None:
    """Merge solved sockets into ``fx_elite_w2.json`` and add / replace ``fx_elite`` in the manifest entries (the
    W1 sockets, fx_overhead among them, are left as they are)."""
    from kit import export
    table = elite_table()
    man = export.Manifest()
    for asset, r in sorted(solved.items()):
        if not r.get("solved"):
            continue
        table[asset] = {k: r[k] for k in ("bone", "posM", "restLocCm", "relLocCm", "choice", "offsetCm", "gapsPx")}
        if r.get("pick", "cheapest") != "cheapest":
            table[asset].update(pick=r["pick"], headDistPx=r["headDistPx"])
        socks = man.data["assets"][asset]["sockets"]
        ent = {"bone": r["bone"], "name": ELITE_SOCKET, "relLocCm": r["relLocCm"], "relRotDeg": [0, 0, 0],
               "restLocCm": r["restLocCm"]}
        for j, s in enumerate(socks):
            if s["name"] == ELITE_SOCKET:
                socks[j] = ent
                break
        else:
            socks.append(ent)
        print(f"[socket] {asset} fx_elite -> {r['choice']}: rest {r['restLocCm']} rel {r['relLocCm']}")
    man.save()
    doc = {"_doc": "Wave 2 art (2H, monsters/elite_sigil.py --solve-elite): fx_elite, the socket SM_FX_EliteSigil "
                   "rides (fx_overhead stays Wave 1's: nameplates, floating text, stun stars, the death mark and the "
                   "W1 crown hang there). Per asset the candidate nearest the W1 fx_overhead (Idle F0, lateral "
                   "weighted most) that clears the figure by >= "
                   f"{GAP_MIN_PX + SOLVE_MARGIN_PX} px at the bob's trough (rest - {BOB_CM:g} cm) in se / sw / ne / "
                   f"nw and stays <= {GAP_CAP_PX} px off it at rest in se / sw, 1080p, the W1 camera, hunt variants "
                   "included. gapsPx = {rest, trough} px per variant and facing; pick / headDistPx where the "
                   "closest-to-the-head walk ran (ELITE_CLOSEST). posM = Blender armature space (m) on bone; "
                   "mship.ship applies it on every re-ship.",
           "assets": dict(sorted(table.items()))}
    ELITE_TABLE.write_text(json.dumps(doc, indent=1) + "\n")


def over_roster(out_dir: Path, facings=GAP_FACINGS, only=()) -> dict:
    """``sigil_over_roster*.png`` + ``sigil_over_roster.json`` (Wave 2, 2H): the sigil on ``fx_elite`` over every
    march monster that can be elite (``ROSTER``; Idle frame 0, the shipped FBX through ``coherence/cohkit``, hunt
    variants at their scale with their attachments, the marker at absolute scale) in the four diagonal facings at
    the W1 camera, native 1080p on the march ground, the sigil at rest; each cell labelled with the clear gap at rest
    and at the bob's trough (gates: trough >= ``GAP_MIN_PX`` everywhere, rest <= ``GAP_CAP_PX`` in se / sw)."""
    import cohkit
    from kit import pngio
    import unburied as UB
    UB.march_stage()
    gaps, written = {}, []
    drop = Vector((0.0, 0.0, BOB_CM / 100.0))
    for asset, variant, label in ROSTER:
        if asset not in cohkit.manifest()["assets"] or (only and label not in only):
            continue
        cells = []
        for fac in facings:
            scene.reset()
            pal = Palette(FAMILY)
            review.setup_render((1920, 1080), 8)
            cam = review.game_camera()
            fig = cohkit.import_character(asset, (0.0, 0.0, 0.0), 0.0, variant=variant, cam=cam)
            for o in fig.extra:                         # the variant's W1 crown: the sigil replaces it here
                o.hide_render = True
            fig.extra = []
            mk = build_sigil(pal)
            e = cohkit.entry(asset)
            rv = review.Review(asset, fig.root, fig.all() + [mk], max(1.0, fig.height_m), blob_radius=0.3,
                               out_dir=out_dir, game_outline_px=outline.SCREEN_PX_1080["monster"],
                               outline_class="monster", glows=glow_cards(mk, SIGIL_FX, ember_world))
            rv.ground_occludes = True
            rv.facing(fac)
            bpy.context.view_layer.update()
            wp = cohkit._socket_world(e, fig.arm, ELITE_SOCKET)
            for m in fig.meshes:
                review.set_outline_px([m], float(m.get("af_px1080", fig.px)), Vector((0, 0, 0.8)))
            c = review.project(cam, wp * 0.55)
            hw = 0.075 * max(1.0, (wp.z + 0.3) / 1.8)
            hh = hw * 1920 / 1080
            box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
            _place(mk, "sigil", wp - drop, cam)
            review.set_outline_px([mk], outline.SCREEN_PX_1080[OUTLINE_CLASS], wp - drop)
            g_t = marker_gap_px(fig.all(), mk, box)
            _place(mk, "sigil", wp, cam)
            review.set_outline_px([mk], outline.SCREEN_PX_1080[OUTLINE_CLASS], wp)
            g_r = marker_gap_px(fig.all(), mk, box)
            img, _ = rv.game_shot(box)
            key = f"{asset}{'@' + variant if variant else ''}:{fac}"
            gaps[key] = {"rest": g_r, "trough": g_t}
            ok = g_t >= GAP_MIN_PX and (fac not in CAP_FACINGS or g_r <= GAP_CAP_PX)
            img = np.ascontiguousarray(img)
            review.draw_text(img, 3, 3, f"{label} {fac.upper()} + SIGIL @FX_ELITE")
            review.draw_text(img, 3, 13, f"GAP {g_r:g}PX, BOB TROUGH {g_t:g}PX " + ("OK" if ok else "FAIL"),
                             rgb=(160, 230, 160) if ok else (240, 90, 90))
            cells.append(img)
            print(f"  [gap] sigil over {key}: rest {g_r:g} px, trough {g_t:g} px", flush=True)
        h = max(c.shape[0] for c in cells)
        w = max(c.shape[1] for c in cells)
        cells = [np.pad(c, ((0, h - c.shape[0]), (0, w - c.shape[1]), (0, 0)), constant_values=24) for c in cells]
        out_dir.mkdir(parents=True, exist_ok=True)
        p = out_dir / f"sigil_over_roster_{label.lower().replace('-', '_')}.png"
        for q, sz in pngio.write_png_native(p, review.grid(cells, len(facings))):
            print(f"  {q.name} {sz // 1024} KB")
            written.append(q.name)
    fail_min = [k for k, v in gaps.items() if v["trough"] < GAP_MIN_PX]
    fail_cap = [k for k, v in gaps.items() if k.rsplit(":", 1)[1] in CAP_FACINGS and v["rest"] > GAP_CAP_PX]
    worst = min((v["trough"] for v in gaps.values()), default=99.0)
    widest = max((v["rest"] for k, v in gaps.items() if k.rsplit(":", 1)[1] in CAP_FACINGS), default=0.0)
    rep = {"gate": {"socket": ELITE_SOCKET, "minGapPx": GAP_MIN_PX, "measuredAt": f"bob trough (rest - {BOB_CM:g} cm)",
                    "capPx": GAP_CAP_PX, "capFacings": list(CAP_FACINGS), "capAt": "rest", "facings": list(facings),
                    "frame": "Idle F0", "res": "1080p"},
           "pass": not fail_min and not fail_cap, "worstTroughPx": worst, "widestFrontRestPx": widest,
           "failures": fail_min + fail_cap, "gaps": gaps, "sheets": written}
    (out_dir / "sigil_over_roster.json").write_text(json.dumps(rep, indent=1, sort_keys=True) + "\n")
    return rep


def patch_sigil_entry(out_dir: Path) -> None:
    """The SM_FX_EliteSigil manifest entry without a re-export (the FBX stands): ``attachSocket`` / ``fx`` from
    ``SIGIL_FX`` (fx_elite, the bob contract), ``clearance`` from ``sigil_over_roster.json``, the previews listed."""
    from kit import export
    rep = json.loads((out_dir / "sigil_over_roster.json").read_text())
    man = export.Manifest()
    e = man.data["assets"][SIGIL]
    e["attachSocket"] = ELITE_SOCKET
    e["fx"] = json.loads(json.dumps(SIGIL_FX))
    e["previews"] = [f"{SIGIL}/{p.name}" for p in sorted(out_dir.glob("*.png"))]
    e["clearance"] = {
        "report": f"{SIGIL}/sigil_over_roster.json", "pass": rep["pass"], "socket": ELITE_SOCKET,
        "worstTroughGapPx": rep["worstTroughPx"], "widestFrontGapPx": rep["widestFrontRestPx"], "gate": rep["gate"],
        "table": "Art/blender/monsters/fx_elite_w2.json",
        "note": "Wave 2 (2H, art review round 1): the sigil rides fx_elite on every march monster that can be elite "
                "(regulars, hunt leaders at x1.25, the mini-boss; Valdras opts out, his fx_elite is the W1 fallback "
                "forward of his standard); fx_overhead is Wave 1's (nameplates, floating text, stun stars, the death "
                "mark, the W1 crown). Gates: >= minGapPx clear of the figure at the bob's trough in every facing, "
                "<= capPx off it at rest in se / sw (elite_sigil.py --solve-elite / --roster)"}
    man.save()


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
            extra = {"attachSocket": "fx_elite", "fx": fx, "widthCm": round(2 * (R_HOOP + 0.06) * 100, 1),
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
    elif "--solve-elite" in args:
        # Wave 2 (2H): fx_elite per asset (clear at the bob's trough, close in se / sw) -> fx_elite_w2.json + manifest
        only = [a.split("=", 1)[1].upper() for a in args if a.startswith("--only=")]
        cap_arg = next((a.split("=", 1)[1] for a in args if a.startswith("--cap=")), None)
        solved = solve_elite(tuple(x for o in only for x in o.split(",")),
                             GAP_CAP_PX if cap_arg is None else (None if cap_arg == "none" else float(cap_arg)))
        print(json.dumps({k: v.get("choice", "UNSOLVED") for k, v in solved.items()}, indent=1))
        if "--dry-run" not in args:
            write_elite(solved)
        sys.exit(0 if all(v.get("solved") for v in solved.values()) else 1)
    elif "--roster" in args:
        # Wave 2 (2H): the sigil on fx_elite over the whole march roster -> Previews/SM_FX_EliteSigil (+ the manifest
        # clearance block) or --scratch
        from kit import paths
        out = scratch_root("a8", "roster") if "--scratch" in args else paths.preview_dir(SIGIL)
        only = [a.split("=", 1)[1].upper() for a in args if a.startswith("--only=")]
        rep = over_roster(out, only=tuple(x for o in only for x in o.split(",")))
        print(f"[roster] worst trough gap {rep['worstTroughPx']:g} px, widest se/sw gap {rep['widestFrontRestPx']:g} "
              f"px, failures {rep['failures']}")
        if "--scratch" not in args and not only:
            patch_sigil_entry(out)
        sys.exit(0 if rep["pass"] else 1)
    elif "--over" in args:
        # the marker-over-monster sheets of both markers, in place (the FBX stand; the manifest previews unchanged)
        from kit import paths
        for name, w_ in ((SIGIL, "sigil"), (CROWN, "crown")):
            for f in paths.preview_dir(name).glob(f"{w_}_over_monsters*.png"):
                f.unlink()
            for f in paths.preview_dir(name).glob(f"{w_}_over_gnawers_facings*.png"):
                f.unlink()
            over_monsters(paths.preview_dir(name), w_)
            over_gnawers_facings(paths.preview_dir(name), w_)
        if (paths.preview_dir(SIGIL) / "sigil_over_roster.json").exists():
            patch_sigil_entry(paths.preview_dir(SIGIL))
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
