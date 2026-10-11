"""绿色史莱姆 Green Slime — ``SK_Mon_Slime`` on ``SKEL_Slime`` (spec art-inventory-ch1.md §4.1, §2.4, §10; R2, R12).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/slime.py
        [--quick] [--anims Clip …] [--no-previews] [--no-verify]

A glossy jiggling jelly dome (0.43 m tall × 0.88 m wide, round footprint) with a dark jelly core, a half-digested
bone and rising bubbles inside, big glaring eyes, a pseudopod for the slam.

* **Look (R12)**: the shell is its own material slot ``MI_AF_Toon_Slime`` (parent ``M_AF_Toon_Slime``: masked,
  dithered ``BodyOpacity`` .65 + the web jelly rim rgba(230,255,220,.6)); the core, bone, bubbles, eyes, brows and
  the sheen are opaque palette toon (slot 0); one ink hull round the whole jelly (slot 1). The previews render the
  TAA-resolved dither (the shell mixed with what lies behind it, back faces culled like UE). BodyOpacity .82: the jelly
  reads as solid as the web's, the dark core and the bone still show through.
* **Rig (SKEL_Slime, 16 bones, all children of ``root``)**: ``body_base / body_mid / body_top`` carry the web pose
  (``sx, sy`` squash/stretch as non-uniform **bone scale**, ``lean`` as a height-proportional shift, ``hop``),
  ``pod_01..02`` the pseudopod reach, ``eye_l/r`` (openness = vertical scale), ``lid_l/r`` (the angry brows,
  scaled in when attacking), ``xeye_l/r`` (X-ed out eyes at death), ``core``, ``bone_prop``, ``bubble_01..02``
  (rising bubbles, scaled out when they pop). Clips are keyed as bone basis matrices (``kit.anim.bake_basis``).
* **Clips** (web ``Slime.ts`` :46-72; ref speed 1.20 m/s): Idle 667 · Walk (hop) 600 with no sliding — the base is
  planted while the body squashes (FX_Land 510) · Attack01 body slam 500 (WindupEnd 155, Contact 250) · Hurt 200 ·
  Death 500 (melts into a puddle; the runtime adds the puddle decal and the fade).
"""
from __future__ import annotations

import json
import math
import sys
from dataclasses import dataclass, field, replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "heroes"))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import anim, mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.anim import Clip  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

import mgeo as G  # noqa: E402

ASSET = "SK_Mon_Slime"
TOKEN = "Mon_Slime"
SKELETON = "SKEL_Slime"
FAMILY = "Monsters_Plains"
PREFIX = "slime"
OUTLINE_CLASS = "monster"
W = 0.44                    # dome half-width (web W 20 px × 2.21 cm)
H = 0.43                    # dome height (web H 17 px × 2.552 cm)
UW = 0.0221                 # web px → m, ground
UH = 0.02552                # web px → m, upright
REF_SPEED = 1.20
BODY_OPACITY = 0.82
CONTACT_MS = 250.0
WINDUP_END_MS = 155.0

REGIONS = outline.ink_regions(dict(
    body=Region("#5FCF5A", .35, .45),
    core=Region("#14592A", .35, .15),
    bone=Region("#ECE2C6", .30, .40),
    bubble=Region("#D2FFC8", .15, .30),
    sclera=Region("#F7FFF0", .20, .25),
    eye_rim=Region("#1D4A24", .30, .10),
    pupil=Region("#10200F", .30, .10),
    glint=Region("#FFFFFF", .05, .20),
    brow=Region("#1D4A24", .30, .10),
    xeye=Region("#123018", .30, .10),
    sheen=Region("#F2FFEE", .10, .30),
), OUTLINE_CLASS)

# dome profile (radius, z) bottom → top: flat sole, a lower bulge, a soft dome
PROFILE = [(0.36, 0.0), (0.415, 0.012), (0.44, 0.045), (0.445, 0.085), (0.43, 0.15), (0.395, 0.215),
           (0.34, 0.28), (0.26, 0.345), (0.16, 0.395), (0.075, 0.421), (0.03, 0.428)]
PROFILE_PLOT = PROFILE + [(0.0, 0.43)]          # (the mesh closes the top with a fan: no degenerate pole ring)


def radius_at(z: float) -> float:
    zs = [p[1] for p in PROFILE_PLOT]
    rs = [p[0] for p in PROFILE_PLOT]
    return float(np.interp(z, zs, rs))


def surface_point(lat: float, z: float, out: float = 0.0) -> Vector:
    """Front surface point of the dome at lateral offset ``lat`` and height ``z`` (+ ``out`` along the normal)."""
    r = radius_at(z)
    y = -math.sqrt(max(r * r - lat * lat, 1e-6))
    p = Vector((lat, y, z))
    dz = 1e-3
    dr = (radius_at(z + dz) - radius_at(z - dz)) / (2 * dz)
    n = Vector((lat / r, y / r, -dr)).normalized()
    return p + n * out


@dataclass
class SlimeSpec:
    name: str = ASSET
    skeleton: str = SKELETON

    def crown_height(self) -> float:
        return H


EYE_LAT, EYE_Z, EYE_R = 0.098, 0.255, 0.080
EYE_SINK = 0.016            # eye bone below the jelly surface
EYE_TILT = 25.0             # deg the eyes face down from the surface normal (their tops tuck into the dome)
EYE_YS = 1.12               # taller eyes: the tilt foreshortens them from the W1 camera


def build_rig() -> R.Rig:
    scene.ensure_object_mode()
    data = bpy.data.armatures.new(SKELETON)
    obj = bpy.data.objects.new(ASSET, data)
    scene.link(obj)
    data.display_type = "STICK"
    scene.select_only([obj])
    bpy.ops.object.mode_set(mode="EDIT")
    eb = data.edit_bones
    J = {}

    def bone(name, head, tail, parent="root", x_axis=(1, 0, 0)):
        b = eb.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        if parent:
            b.parent = eb[parent]
        y = (b.tail - b.head).normalized()
        x = Vector(x_axis)
        x = (x - y * x.dot(y)).normalized()
        b.align_roll(x.cross(y))
        b.use_deform = True
        J[name] = Vector(head)
        return b
    bone("root", (0, 0, 0), (0, 0, 0.05), parent=None)
    bone("body_base", (0, 0, 0.0), (0, 0, 0.1))
    bone("body_mid", (0, 0, 0.18), (0, 0, 0.27))
    bone("body_top", (0, 0, 0.34), (0, 0, 0.43))
    bone("pod_01", (0, -0.25, 0.05), (0, -0.33, 0.05))
    bone("pod_02", (0, -0.37, 0.04), (0, -0.45, 0.04))
    for s, sg in (("l", 1), ("r", -1)):
        e = surface_point(sg * EYE_LAT, EYE_Z, -EYE_SINK)
        bone(f"eye_{s}", e, e + Vector((0, -0.04, 0)))
        bone(f"xeye_{s}", e + Vector((0, -0.03, 0)), e + Vector((0, -0.07, 0)))
        lid = surface_point(sg * (EYE_LAT + 0.004), EYE_Z + 0.082, -0.004)
        bone(f"lid_{s}", lid, lid + Vector((0, -0.04, 0)))
    bone("core", (0, 0.04, 0.16), (0, 0.04, 0.26))
    bone("bone_prop", (0.12, -0.21, 0.12), (0.12, -0.21, 0.2))
    bone("bubble_01", (-0.15, -0.22, 0.06), (-0.15, -0.22, 0.12))
    bone("bubble_02", (0.05, -0.28, 0.08), (0.05, -0.28, 0.14))
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in obj.pose.bones:
        pb.rotation_mode = "QUATERNION"
    obj["af_skeleton"] = SKELETON
    sockets = [
        {"name": "fx_feet", "bone": "root", "pos": Vector((0, 0, 0))},
        {"name": "fx_chest", "bone": "body_mid", "pos": Vector((0, 0, 0.55 * H))},
        {"name": "fx_head", "bone": "body_top", "pos": Vector((0, 0, 0.8 * H))},
        {"name": "fx_overhead", "bone": "body_top", "pos": Vector((0, 0, H + 0.25))},
        {"name": "fx_eye_l", "bone": "eye_l", "pos": J["eye_l"]},
        {"name": "fx_eye_r", "bone": "eye_r", "pos": J["eye_r"]},
        {"name": "fx_pod", "bone": "pod_02", "pos": J["pod_02"]},
    ]
    return R.Rig(obj, SlimeSpec(), J, sockets)


# ── mesh ───────────────────────────────────────────────────────────────────────────────────────────────
def _smooth(x, e0, e1):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def shell_bind() -> Bind:
    """Body weights by height (base / mid / top, smooth bells) and the pseudopod on the front-bottom quarter."""
    names = ("body_base", "body_mid", "body_top", "pod_01", "pod_02")

    def fn(co):
        x, y, z = co[:, 0], co[:, 1], co[:, 2]
        k = np.clip(z / H, 0, 1)
        wb = 1.0 - _smooth(k, 0.05, 0.42)
        wt = _smooth(k, 0.42, 0.85)
        wm = np.clip(1.0 - wb - wt, 0, 1)
        r = np.hypot(x, y)
        cosf = np.where(r > 1e-6, -y / np.maximum(r, 1e-6), 0.0)            # 1 = straight ahead
        front = np.clip((cosf - 0.25) / 0.75, 0, 1) ** 1.5
        low = 1.0 - _smooth(k, 0.0, 0.42)
        pod = front * low * 0.95
        outer = _smooth(r, 0.28, 0.42)
        w = np.stack([wb * (1 - pod), wm * (1 - pod), wt * (1 - pod), pod * (1 - outer), pod * outer], axis=1)
        return w + 1e-6
    return Bind.custom(names, fn)


def build_body(pal: Palette, rig: R.Rig):
    from kit import asset
    b = M.Builder(ASSET, pal, prefix=PREFIX)
    b.regions(**REGIONS)
    shell = M.lathe(PROFILE, sides=36, cap0="fan", cap1="fan")
    sid = b.add(shell, "body", shell_bind(), name="shell", hull_pad=True)
    # eyes: dark rim, sclera, pupil looking slightly down-in (glaring), glint
    for s, sg in (("l", 1), ("r", -1)):
        e = rig.joints[f"eye_{s}"]
        n = (surface_point(sg * EYE_LAT, EYE_Z, 0.01) - surface_point(sg * EYE_LAT, EYE_Z, 0.0)).normalized()
        side = n.cross(Vector((0, 0, 1))).normalized()
        n = Matrix.Rotation(-math.radians(EYE_TILT), 3, side) @ n       # (side = −X for the front: tips n down)
        m = G.frame_z(e, n, Vector((0, 0, 1)))
        rim = M.ellipsoid((EYE_R * 0.95, EYE_R * 1.1 * EYE_YS, 0.028), (0, 0, -0.001), sides=16, rings=8).transform(m)
        b.add(rim, "eye_rim", Bind.rigid(f"eye_{s}"), name=f"eye_rim_{s}", outline=False, covered_by=sid)
        scl = M.ellipsoid((EYE_R * 0.85, EYE_R * EYE_YS, 0.030), (0, 0, 0.006), sides=16, rings=8).transform(m)
        b.add(scl, "sclera", Bind.rigid(f"eye_{s}"), name=f"sclera_{s}", outline=False, covered_by=sid)
        pup = M.ellipsoid((EYE_R * 0.42, EYE_R * 0.52 * EYE_YS, 0.012), (-sg * EYE_R * 0.1, -EYE_R * 0.08, 0.031),
                          sides=12, rings=6).transform(m)
        b.add(pup, "pupil", Bind.rigid(f"eye_{s}"), name=f"pupil_{s}", outline=False, covered_by=sid)
        gl = M.ellipsoid((EYE_R * 0.13, EYE_R * 0.13, 0.006), (sg * EYE_R * 0.08, EYE_R * 0.22, 0.041), sides=8,
                         rings=4).transform(m)
        b.add(gl, "glint", Bind.rigid(f"eye_{s}"), name=f"glint_{s}", outline=False, covered_by=sid)
        # X-ed out eye (death): two crossed bars over the eye (scaled in by xeye_*)
        for ang in (40.0, -40.0):
            bar = M.box((EYE_R * 1.45, 0.013, 0.012), (0, 0, 0), bevel=0.004)
            bar.transform(m @ Matrix.Translation((0, 0, 0.034)) @ Matrix.Rotation(math.radians(ang), 4, "Z"))
            b.add(bar, "xeye", Bind.rigid(f"xeye_{s}"), name=f"xeye_{s}", outline=False, covered_by=sid)
        # angry brow (attack): a slanted bar above the eye, lower toward the middle (scaled in by lid_*)
        lid = rig.joints[f"lid_{s}"]
        a0 = lid + Vector((sg * 0.05, 0, 0.01))
        a1 = lid + Vector((-sg * 0.045, 0, -0.022))
        brow = M.capsule(a0, a1, 0.012, 0.014, sides=8, cap_rings=1)
        b.add(brow, "brow", Bind.rigid(f"lid_{s}"), name=f"brow_{s}", outline=False, covered_by=sid)
    # glossy sheen patch (upper front-right) and a specular dot
    for c, rr, reg in ((surface_point(-0.17, 0.33, 0.002), (0.07, 0.035, 0.012), "sheen"),
                       (surface_point(-0.06, 0.39, 0.002), (0.016, 0.016, 0.008), "sheen")):
        dz = 1e-3
        n = (surface_point(c.x, c.z + dz) - surface_point(c.x, c.z - dz)).cross(Vector((1, 0, 0)))
        nn = (c - Vector((0, 0, 0.12))).normalized()
        m = G.frame_z(c, nn, Vector((0, 0, 1))) @ Matrix.Rotation(math.radians(-28), 4, "Z")
        sh = M.ellipsoid(rr, (0, 0, 0), sides=12, rings=5).transform(m)
        b.add(sh, reg, shell_bind(), name="sheen", outline=False, covered_by=sid)
    # inside: the dark jelly core (two merged lobes), the half-digested bone, bubbles
    core = M.ellipsoid((0.27, 0.24, 0.17), (0.0, 0.03, 0.19), sides=18, rings=10)
    b.add(core, "core", Bind.rigid("core"), name="core", outline=False)
    lobe = M.ellipsoid((0.15, 0.13, 0.11), (-0.09, -0.04, 0.12), sides=14, rings=8)
    b.add(lobe, "core", Bind.rigid("core"), name="core", outline=False)
    bp = rig.joints["bone_prop"]
    tilt = Matrix.Translation(bp) @ Matrix.Rotation(math.radians(-29), 4, "Y") @ Matrix.Rotation(math.radians(25), 4, "Z")
    shaft = M.capsule((-0.065, 0, 0), (0.065, 0, 0), 0.014, 0.014, sides=8, cap_rings=1).transform(tilt)
    b.add(shaft, "bone", Bind.rigid("bone_prop"), name="bone_prop", outline=False)
    for x in (-0.072, 0.072):
        for dz in (-0.015, 0.015):
            k = M.ellipsoid((0.02, 0.02, 0.02), (x, 0, dz), sides=8, rings=5).transform(tilt)
            b.add(k, "bone", Bind.rigid("bone_prop"), name="bone_prop", outline=False)
    for nm, offs in (("bubble_01", ((0, 0, 0, 0.022), (0.05, 0.03, 0.04, 0.014))),
                     ("bubble_02", ((0, 0, 0, 0.018), (-0.06, 0.02, 0.05, 0.012)))):
        c0 = rig.joints[nm]
        for dx, dy, dz, r in offs:
            bb = M.ellipsoid((r, r, r), c0 + Vector((dx, dy, dz)), sides=10, rings=6)
            b.add(bb, "bubble", Bind.rigid(nm), name="bubble", outline=False)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=H)
    # the shell's toon faces → slot 2, the dithered jelly material (R12)
    me = body.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    mi = np.where((fp == sid) & (mi == 0), 2, mi)
    me.materials.append(dither_material(pal, BODY_OPACITY))
    me.polygons.foreach_set("material_index", mi)
    me.update()
    return body, sid


def dither_material(pal: Palette, opacity: float) -> bpy.types.Material:
    """Preview of ``MI_AF_Toon_Slime`` (parent ``M_AF_Toon_Slime``): the palette toon, dithered to ``opacity`` —
    rendered as the TAA-resolved mix with what lies behind; back faces are culled as in UE (one-sided masked)."""
    base = shading.toon_material(pal)
    m = base.copy()
    m.name = "MI_AF_Toon_Slime"
    nt = m.node_tree
    em = next(n for n in nt.nodes if n.type == "EMISSION")
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = opacity
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    cull = nt.nodes.new("ShaderNodeMixShader")
    tr2 = nt.nodes.new("ShaderNodeBsdfTransparent")
    nt.links.new(geo.outputs["Backfacing"], cull.inputs["Fac"])
    nt.links.new(mix.outputs[0], cull.inputs[1])
    nt.links.new(tr2.outputs[0], cull.inputs[2])
    nt.links.new(cull.outputs[0], out.inputs["Surface"])
    m["af_parent"] = "M_AF_Toon_Slime"
    m["af_params"] = json.dumps({"BodyOpacity": opacity})
    m["af_toon_slot"] = True
    return m


# ── pose model (web SlimePose) → bone basis matrices ───────────────────────────────────────────────────
@dataclass
class SP:
    x: float = 0.0          # forward offset (m)
    hop: float = 0.0        # height off the ground (m)
    sx: float = 1.0
    sy: float = 1.0
    lean: float = 0.0       # top leans forward (+) or back (−)
    eyes: float = 1.0       # 0 X-ed out … 1 open
    reach: float = 0.0      # pseudopod 0..1
    melt: float = 0.0
    fx: float = 0.0         # attack intensity (angry brows)
    yaw: float = 0.0        # deg, a little body turn (3D life)

    def lerp(self, o: "SP", t: float) -> "SP":
        return SP(*[a + (b - a) * t for a, b in zip(self.__dict__.values(), o.__dict__.values())])


REST = SP()


def P(**kw) -> SP:
    return replace(REST, **kw)


def sample(keys, t: float) -> SP:
    """Web ``samplePoseTrack``: keys [(at, ease, SP)], easing of the destination key."""
    if t <= keys[0][0]:
        return keys[0][2]
    for (a0, _, p0), (a1, e1, p1) in zip(keys, keys[1:]):
        if a0 <= t <= a1:
            return p0.lerp(p1, anim.ease((t - a0) / max(a1 - a0, 1e-9), e1))
    return keys[-1][2]


class SlimeAnims:
    def __init__(self, rig: R.Rig):
        self.rig = rig
        self.rest = {b.name: b.matrix_local.copy() for b in rig.obj.data.bones}

    def body_x(self, p: SP, k: float) -> Matrix:
        """Armature-space map of the body at height fraction ``k``: scale (sx, sx, sy·(1 − melt/2)) about the
        ground point, the lean shift (web x += lean·h·k·1.2), hop and forward offset, a small yaw."""
        sy = p.sy * (1.0 - 0.5 * p.melt)
        S = Matrix.Diagonal((p.sx, p.sx, sy, 1.0))
        shift = Vector((0.0, -(p.x + p.lean * H * sy * k * 1.2), p.hop))
        return Matrix.Translation(shift) @ Matrix.Rotation(math.radians(p.yaw), 4, "Z") @ S

    def basis(self, p: SP, t_ms: float, loop_ms: float) -> dict:
        out = {}
        rest = self.rest

        def put(name, X):
            Mr = rest[name]
            out[name] = Mr.inverted() @ X @ Mr
        for nm in ("body_base", "body_mid", "body_top", "core", "bone_prop"):
            k = rest[nm].translation.z / H
            put(nm, self.body_x(p, k))
        # pseudopod: the front-bottom reaches forward (web: x += reach·7 px at the base, y += reach·2)
        for nm, d in (("pod_01", 0.09), ("pod_02", 7.0 * UW)):
            X = Matrix.Translation(Vector((0.0, -p.reach * d, -p.reach * 0.012))) @ self.body_x(p, 0.0)
            S = Matrix.Diagonal((1.0 + 0.25 * p.reach, 1.0 + 0.6 * p.reach, 1.0 - 0.35 * p.reach, 1.0))
            c = rest[nm].translation
            put(nm, X @ Matrix.Translation(c) @ S @ Matrix.Translation(-c))
        # eyes ride the surface (no squash of their own), open = vertical scale; brows / X-eyes scale in
        xe = max(0.0, min(1.0, (0.12 - p.eyes) / 0.1))
        open_ = max(0.02, (0.35 + 0.65 * max(p.eyes, 0.0)) * (1.0 - xe))
        brow = max(0.001, min(1.0, (p.fx - 0.2) / 0.25))
        for s in ("l", "r"):
            for nm, sc in ((f"eye_{s}", (1.0, 1.0, open_)), (f"xeye_{s}", (max(0.001, xe),) * 3),
                           (f"lid_{s}", (brow,) * 3)):
                a = rest[nm].translation
                k = a.z / H
                pa = self.body_x(p, k) @ a
                D = Matrix.Diagonal((sc[0], sc[1], sc[2], 1.0))
                put(nm, Matrix.Translation(pa) @ Matrix.Rotation(math.radians(p.yaw), 4, "Z") @ D
                    @ Matrix.Translation(-a))
        # bubbles rise inside and pop near the top (web: 4 bubbles, k = (i/4 + t) % 1)
        for i, nm in enumerate(("bubble_01", "bubble_02")):
            a = rest[nm].translation
            ph = ((t_ms / loop_ms) + i * 0.5) % 1.0
            z = a.z + ph * (0.62 * H - a.z) * 1.0
            pop = max(0.001, min(1.0, (0.92 - ph) / 0.12)) * min(1.0, 0.3 + ph / 0.15)
            pa = self.body_x(p, z / H) @ Vector((a.x, a.y, z))
            D = Matrix.Diagonal((pop, pop, pop, 1.0))
            put(nm, Matrix.Translation(pa) @ D @ Matrix.Translation(-a))
        return out

    def clip(self, name, length_ms, fn, loop=False, notifies=None, ref_speed=None, loop_ms=667.0, key_ms=None):
        c = Clip(name, length_ms, loop=loop, notifies=notifies or {}, ref_speed=ref_speed)
        c.basis_fn = lambda t: self.basis(fn(t), t, loop_ms)
        c.sampler = lambda t: None
        c.key_poses_ms = key_ms or [length_ms * i / 5 for i in range(6)]
        return c

    # web tracks (Slime.ts :46-72), t = ms / span
    def idle(self):
        def fn(t):
            ph = 2 * math.pi * t / 667.0
            return P(sx=1 + math.sin(ph) * 0.06, sy=1 - math.sin(ph) * 0.07, lean=math.sin(ph + 0.8) * 0.05,
                     yaw=3.0 * math.sin(ph * 0.5))
        return self.clip("Idle", 667.0, fn, loop=True)

    WALK = [(0.0, "smooth", P(sx=1.18, sy=0.8)),
            (0.2, "out", P(sx=0.86, sy=1.18, hop=6 * UH, lean=0.12)),
            (0.45, "smooth", P(sx=0.92, sy=1.1, hop=9 * UH, lean=0.06)),
            (0.7, "in", P(sx=1.0, sy=1.02, hop=3 * UH)),
            (0.85, "smooth", P(sx=1.25, sy=0.74)),
            (1.0, "smooth", P(sx=1.18, sy=0.8))]

    def walk(self):
        """One hop per 600 ms at 1.20 m/s (0.72 m). No sliding (R9): while the sole is on the ground (t ≥ .82 and
        t < .08) the body moves back at exactly the ground speed; in the air it lunges forward (web keys give the
        squash / hop)."""
        L = 600.0
        c0, c1 = 0.82, 1.08                 # contact window (wraps)
        A = REF_SPEED * L / 1000.0 * (c1 - c0) / 2.0

        def xoff(t):
            u = t if t >= c0 else t + 1.0
            if u <= c1:                                   # planted: +A → −A linearly
                return A - 2 * A * (u - c0) / (c1 - c0)
            f = (t - (c1 - 1.0)) / (c0 - (c1 - 1.0))      # airborne: −A → +A
            e = f * f * (3 - 2 * f)
            return -A + 2 * A * e

        def fn(t_ms):
            t = (t_ms / L) % 1.0
            p = sample(self.WALK, t)
            return replace(p, x=xoff(t), yaw=2.0 * math.sin(2 * math.pi * t))
        return self.clip("Walk", L, fn, loop=True, notifies={"FX_Land": 510.0}, ref_speed=REF_SPEED, loop_ms=600.0,
                         key_ms=[0.0, 120.0, 270.0, 420.0, 510.0, 600.0])

    def attack(self):
        keys = [(0.0, "smooth", REST),
                (0.33, "out", P(x=-3 * UW, sx=0.85, sy=1.26, lean=-0.3, fx=0.3)),     # web .8 / 1.3: held at .85 so the
                # skinned hull keeps the 2.5 px ink floor on the narrowed sides (the WPO subtracts the full baked width)
                (0.67, "in", P(x=4 * UW, sx=0.95, sy=1.1, hop=6 * UH, lean=0.35, reach=0.4, fx=0.7)),
                (1.0, "linear", P(x=5 * UW, sx=1.3, sy=0.72, lean=0.25, reach=1.0, fx=1.0)),
                (1.32, "out", P(x=4.6 * UW, sx=1.22, sy=0.8, lean=0.18, reach=0.85, fx=0.9)),
                (1.65, "smooth", P(x=1.5 * UW, sx=0.95, sy=1.06, lean=-0.04, reach=0.2, fx=0.5)),
                (2.0, "smooth", REST)]

        def fn(t_ms):
            return sample(keys, t_ms / CONTACT_MS)
        return self.clip("Attack01", 500.0, fn, notifies={"WindupEnd": WINDUP_END_MS, "Contact": CONTACT_MS},
                         key_ms=[0.0, 83.3, 166.7, 250.0, 330.0, 412.0, 500.0])

    def hurt(self):
        keys = [(0.0, "smooth", P(x=-3 * UW, sx=1.35, sy=0.68, lean=-0.25, eyes=0.35)),
                (0.5, "smooth", P(x=-1.5 * UW, sx=0.9, sy=1.12, lean=0.1, eyes=0.7)),
                (1.0, "smooth", P(x=-0.5 * UW, sx=1.02, sy=0.98, eyes=0.95))]

        def fn(t_ms):
            return sample(keys, t_ms / 200.0)
        return self.clip("Hurt", 200.0, fn, key_ms=[0.0, 100.0, 200.0])

    def death(self):
        keys = [(0.0, "smooth", P(x=-2 * UW, sx=1.35, sy=0.7, lean=-0.2, eyes=0.3)),
                (0.33, "smooth", P(x=-2 * UW, sx=0.85, sy=1.25, eyes=0.0)),
                (0.67, "in", P(x=-2 * UW, sx=1.6, sy=0.5, eyes=0.0, melt=0.5)),
                (1.0, "out", P(x=-1 * UW, sx=1.65, sy=0.3, eyes=0.0, melt=1.0))]

        def fn(t_ms):
            return sample(keys, t_ms / 500.0)
        c = self.clip("Death", 500.0, fn, key_ms=[0.0, 165.0, 335.0, 500.0])
        c.manifest_extra = {"puddleDecal": {"color": "#5FCF5A", "alpha": 0.35, "radiusFactor": 1.1,
                                            "note": "web fx: rgba(95,207,90,0.35·melt) ellipse r = 1.1·W·sx"}}
        return c

    def all_clips(self):
        return [self.idle(), self.walk(), self.attack(), self.hurt(), self.death()]


# ── build / review / ship ──────────────────────────────────────────────────────────────────────────────
def build_all():
    scene.reset()
    pal = Palette(FAMILY)
    rig = build_rig()
    body, sid = build_body(pal, rig)
    return pal, rig, body


FX = {"jellyRim": {"color": "#E6FFDC", "alpha": 0.6, "material": "M_AF_Toon_Slime RimOverride"},
      "bodyOpacity": BODY_OPACITY,
      "puddle": {"color": "#5FCF5A", "alpha": 0.35, "radiusFactorOfWidth": 1.1, "fromClip": "Death"},
      "blobShadow": {"radiusFactorOfWidth": 1.05, "liftShrink": "hop × 1.5 (web)"},
      "telegraph": {"normal": "#FFC4B0", "elite": "#FF7A5C", "untilMs": 155},
      "land": {"notify": "FX_Land", "atMs": 510}}


class _A:
    """Duck-typed anims holder for ``mship`` (rig + clips)."""

    def __init__(self, rig):
        self.rig = rig


def quick(out: Path, clip: str | None = None, frame: int = 0) -> None:
    from kit import review
    pal, rig, body = build_all()
    S = SlimeAnims(rig)
    out.mkdir(parents=True, exist_ok=True)
    if clip:
        c = next(c for c in S.all_clips() if c.name == clip)
        a = anim.bake_basis(rig, c, TOKEN, c.basis_fn)
        anim.set_frame(frame)
    rv = review.Review(ASSET, rig.obj, [body], H, blob_radius=W * 1.05, out_dir=out,
                       game_outline_px=outline.SCREEN_PX_1080[OUTLINE_CLASS], outline_class=OUTLINE_CLASS)
    rv.ground_occludes = True
    rv.extra_body_slots = True
    rv.closeup("se", pitch=-35.0)
    (out / "closeup.png").rename(out / "closeup_se.png")
    rv.closeup("front", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    rv.turnaround()
    rv.game_view()
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")


def anim_quick(out: Path, names) -> None:
    import mship
    pal, rig, body = build_all()
    S = SlimeAnims(rig)
    cs = [c for c in S.all_clips() if not names or c.name in names]
    mship.anim_review(rig, [body], [], cs, TOKEN, out, H, OUTLINE_CLASS, blob_radius=W * 1.05)


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    pal, rig, body = build_all()
    S = SlimeAnims(rig)
    cs = S.all_clips()
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_slime", pal=pal, A=_A(rig), body=body, items=[], clips=cs,
        outline_class=OUTLINE_CLASS, game_ids=["monster_slime"], blob_radius=round(W * 1.05, 3), height=H,
        display={"zh-CN": "绿色史莱姆", "en": "Green Slime"}, fx=FX,
        qa_limits={"ground": -0.5},
        game_keys=[("Idle", 0, "se"), ("Walk", 7, "se"), ("Walk", 16, "se"), ("Walk", 31, "se"),
                   ("Attack01", 5, "se"), ("Attack01", 15, "se"), ("Hurt", 0, "se"), ("Death", 30, "se")],
        close_keys=[("Idle", 0, "front"), ("Walk", 12, "side"), ("Attack01", 5, "se"), ("Attack01", 15, "se"),
                    ("Attack01", 15, "side"), ("Hurt", 0, "se"), ("Death", 10, "se"), ("Death", 30, "se")],
        back_clips=("Attack01", "Death"),
        skeleton_bones_required=("root", "body_base", "body_mid", "body_top", "pod_01", "pod_02", "eye_l", "eye_r",
                                 "lid_l", "lid_r", "xeye_l", "xeye_r", "core", "bone_prop", "bubble_01",
                                 "bubble_02"),
        extra={"refSpeedCmS": REF_SPEED * 100.0, "attackStyle": "slam", "footprintRadiusCm": round(W * 100, 1),
               "notes": "SKEL_Slime: squash / stretch as bone scale keys (body_*), pseudopod pod_*, eye openness, "
                        "angry brows (lid_*) and X-eyes (xeye_*) scaled in, bubbles rising and popping. Shell = "
                        "slot 2 MI_AF_Toon_Slime (M_AF_Toon_Slime, dithered BodyOpacity, jelly rim, R12); the core, "
                        "bone, bubbles, eyes are opaque toon (slot 0). Walk is a no-slide hop: play rate = v / 1.20 "
                        "m/s."},
        lod_note="Slime budget 2-3k toon tris, <= 16 bones (SKEL_Slime has 16).",
    )
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/mq/slime")
    if "--quick" in args:
        k = args.index("--quick")
        rest = [a for a in args[k + 1:] if not a.startswith("--")]
        quick(scratch, rest[0] if rest else None, int(rest[1]) if len(rest) > 1 else 0)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        anim_quick(scratch / "anims", names)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))
