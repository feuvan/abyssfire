"""Goblin family (``SKEL_Goblin``): the shared body kit for 哥布林 / 碎牙·格罗克 / 哥布林萨满 (spec §4.2–§4.5).

``GoblinKit(asset, look, web)`` builds the rig (kit humanoid builder, goblin proportions, the hunched rest pose of
the web READY spine, **the same bone names for every goblin**: core list + ``jaw, eye_l/r, ear_l/r_01..02,
loin_f/b_01..02, pelt_01..03, headdress_01..04, totem_feather_01..02`` — the union of the family's chains, so
``SKEL_Goblin`` is one skeleton asset in UE) and adds the shared parts to a ``kit.mesh.Builder``:

* head — the web goblin skull (``GOBLIN_SKULL`` rings), heavy brow ridge, yellow slit-pupil eyes, the long hooked
  nose, a toothy grin on a jaw that opens, huge swept-back ears with dark inner ears (``ear_*`` chains);
* pot-bellied hunched torso on a curved spine (:class:`mgeo.SpineLoft`) with the belly highlight, rope belt,
  the shoulder-to-hip sash and the bone-tooth necklace;
* skinny knobbly limbs, big clawed fists, long pointed bare feet with bone toe claws;
* ragged two-sided loincloth flaps on ``loin_*`` chains.

Monster scripts add their own gear (chief helm / mantle / cuirass, shaman mask / headdress / shawl / skirt) with
the surfaces exposed here (``self.head`` / ``self.torso`` / joints). Web source: ``src/graphics/sprites/monsters/
Goblin.ts`` (+ ``GoblinChief.ts``, ``GoblinShaman.ts``), ``rig/MonsterView.ts``.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

import bpy
import numpy as np
from mathutils import Matrix, Vector

from kit import mesh as M, outline, rig as R
from kit.palette import Region
from kit.rig import Bind, HumanoidSpec

import common as C
import mgeo as G
from mgeo import lerp, smooth01

FAMILY = "Monsters_Plains"
SKELETON = "SKEL_Goblin"

# web goblin skull (Goblin.ts GOBLIN_SKULL; head-local units, x forward, y down) sampled into rings: (h, back, front)
SKULL_RINGS = [(7.05, -0.4, 0.9), (6.5, -2.77, 3.08), (6.0, -3.68, 4.03), (5.0, -4.68, 5.28), (4.0, -5.17, 6.05),
               (3.0, -5.39, 6.54), (2.0, -5.48, 6.83), (1.0, -5.48, 7.02), (0.0, -5.38, 7.13), (-1.0, -5.2, 7.14),
               (-2.0, -4.94, 7.05), (-3.0, -4.52, 6.81), (-4.0, -3.82, 6.26), (-5.0, -2.55, 5.1), (-5.6, -1.2, 3.8)]


@dataclass
class Web:
    """Per-goblin web numbers (rig units) + cm per unit (spec §0.1)."""
    U: float                         # metres per rig unit
    thigh: float = 9.0
    shin: float = 9.0
    upper_arm: float = 7.6
    fore_arm: float = 7.2
    torso: float = 12.5
    neck: float = 6.2
    ankle: float = 1.6
    hip: float = 2.2
    shoulder: float = 4.4
    lean: float = 0.34               # READY lean (rad) = rest lean of this mesh
    head_tilt: float = -0.30         # READY head (rad)
    root: tuple = (47.0, 74.0)       # READY root (web frame units)
    bulk: float = 1.0                # torso girth
    head_k: float = 1.0              # head size
    foot_len: float = 8.6            # heel → toe (units)
    grin: float = 1.0


@dataclass
class Look:
    """Material regions of one goblin (hex, shadow, light) — spec §4.2–§4.4 tables."""
    skin: str = "#76A33E"
    skin_l: float = 0.35
    skin_dark: str = "#4E742A"
    skin_dark_l: float = 0.20
    ear_in: str = "#3C5A22"
    cloth: str = "#7C4F2D"
    strap: str = "#4A2F1C"
    rope: str = "#B89A62"
    bone: str = "#ECE2C6"
    eye: str = "#FFD83A"
    eye_e: float = 0.0
    pupil: str = "#1A0E08"
    mouth: str = "#2A0F0A"
    extra: dict = field(default_factory=dict)


def belly_hex(skin: str) -> str:
    """Web belly highlight rgba(255,250,210,0.14) over the skin."""
    from kit import color
    c = color.hex_to_rgb(skin)
    return color.rgb_to_hex(color.mix(c, (255, 250, 210), 0.14))


def regions_for(look: Look) -> dict:
    sk = Region(look.skin, .42, look.skin_l)
    r = dict(
        skin=sk,
        skin_dark=Region(look.skin_dark, .42, look.skin_dark_l),
        skin_line=sk.line_region(),                              # knuckle creases, nostrils
        belly=Region(belly_hex(look.skin), .42, look.skin_l),
        ear_in=Region(look.ear_in, .42, .15),
        cloth=Region(look.cloth, .42, .40),
        cloth_in=Region(look.cloth).shade_region(.35, .12),      # flap linings
        strap=Region(look.strap, .42, .28),
        rope=Region(look.rope),
        rope_line=Region(look.rope).shade_region(.30, .12),      # rope twist strokes
        bone=Region(look.bone, .30, .40),
        bone_dark=Region(look.bone, .30, .40).shade_region(.25, .15),   # claw tips / knuckle claw (web BONE.shade)
        eye=Region(look.eye, .30, .40, e=look.eye_e),
        pupil=Region(look.pupil, .30, .10),
        mouth=Region(look.mouth, .30, .10),
    )
    r.update(look.extra)
    return outline.ink_regions(r, "monster")


def _smooth_w(x, e0, e1):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def chain_bind(top: Vector, bottom: Vector, bones: tuple[str, ...]) -> Bind:
    """Custom weights for a panel hanging along a chain: ``bones[0]`` = the parent (holds the top edge),
    ``bones[1:]`` = the chain links, blended smoothly by the fraction along top → bottom."""
    d = bottom - top
    L2 = max(d.length_squared, 1e-9)
    n = len(bones) - 1

    def fn(co):
        t = np.clip(((co - np.array(top)) @ np.array(d)) / L2, 0.0, 1.2)
        ws = [1.0 - _smooth_w(t, 0.0, 0.22)]
        for k in range(n):
            a, b = k / n, (k + 1) / n
            lo = _smooth_w(t, a - 0.02, a + 0.22) if k > 0 else _smooth_w(t, 0.0, 0.22)
            hi = 1.0 - _smooth_w(t, b - 0.02, b + 0.22) if k < n - 1 else np.ones_like(t)
            ws.append(lo * hi)
        return np.stack(ws, axis=1)
    return Bind.custom(bones, fn)


class GoblinKit:
    """Rig + shared body parts of one goblin-family mesh."""

    def __init__(self, asset: str, prefix: str, web: Web, look: Look):
        self.asset, self.prefix, self.w, self.look = asset, prefix, web, look
        U = web.U
        self.U = U
        self.spec = HumanoidSpec(
            name=asset, skeleton=SKELETON, thigh=web.thigh * U, shin=web.shin * U, ankle=web.ankle * U,
            torso=web.torso * U, neck=web.neck * U, head=14.0 * web.head_k * U, hip_half=web.hip * U,
            shoulder_half=web.shoulder * U, upper_arm=web.upper_arm * U, fore_arm=web.fore_arm * U,
            hand=4.4 * U, foot_len=web.foot_len * U, arm_down=55.0, elbow_prebend=12.0, knee_prebend=4.0,
            rest_lean=web.lean, rest_head=web.head_tilt, face=True)
        self.rig = R.build_humanoid(self.spec)
        J = self.rig.joints
        self.J = J
        self.tilt = web.lean + web.head_tilt
        self.HC = J["head_center"].copy()
        # head frame: local (lat, back, up) in metres, up along the neck→crown direction
        self.Hm = Matrix.Translation(self.HC) @ Matrix.Rotation(self.tilt, 4, "X")
        self.head = G.ProfileLoft(self.skull_profile(), self.Hm)
        self.torso = self.torso_loft()
        self._place_face_bones()
        self._add_chains()

    # ── units / frames ─────────────────────────────────────────────────────────────────────────────────
    def u(self, n: float) -> float:
        return n * self.U

    def hk(self, n: float) -> float:
        """Head units (scaled with the head)."""
        return n * self.U * self.w.head_k

    def hp(self, h: float, f: float, lat: float = 0.0) -> Vector:
        """Web head-view point (h up, f forward, lat left; head units) → armature space."""
        return self.Hm @ Vector((self.hk(lat), -self.hk(f), self.hk(h)))

    def hdir(self, h: float, f: float, lat: float = 0.0) -> Vector:
        return (self.Hm.to_3x3() @ Vector((lat, -f, h))).normalized()

    # ── head ───────────────────────────────────────────────────────────────────────────────────────────
    def skull_profile(self) -> C.Profile:
        rings = []
        for h, back, front in SKULL_RINGS:
            d = (front - back) / 2
            c = (front + back) / 2
            # cranium a touch wider than deep (big goblin brain-case), cheeks full for the grin, chin narrow
            k = 0.96 if h > 1.5 else (0.98 if h > -3.5 else 0.88)
            rings.append((self.hk(h), self.hk(d * k), self.hk(d), -self.hk(c)))
        return C.Profile(rings, exp=2.15)

    def head_point(self, h: float, a: float, out: float = 0.0) -> Vector:
        return self.head.point(self.hk(h), a, out)

    def head_normal(self, h: float, a: float) -> Vector:
        return self.head.normal(self.hk(h), a)

    def _place_face_bones(self) -> None:
        """Eye bones on the eyes (fx_eye sockets), the jaw hinge under the ears pointing to the chin."""
        arm = self.rig.obj
        from kit import scene
        scene.select_only([arm])
        bpy.ops.object.mode_set(mode="EDIT")
        eb = arm.data.edit_bones
        for s, sg in (("l", 1), ("r", -1)):
            e = self.eye_center(sg)
            b = eb[f"eye_{s}"]
            b.head, b.tail = e, e + self.hdir(0, 1) * 0.03
            b.align_roll(self.hdir(1, 0))
            self.J[f"eye_{s}"] = e
        jb = eb["jaw"]
        jb.head = self.hp(-0.6, -1.0)
        jb.tail = self.hp(-5.2, 4.6)
        jb.align_roll(Vector((1, 0, 0)).cross((jb.tail - jb.head).normalized()))
        bpy.ops.object.mode_set(mode="OBJECT")
        for so in self.rig.sockets:
            if so["name"] in ("fx_eye_l", "fx_eye_r"):
                so["pos"] = self.J["eye_" + so["name"][-1]]

    def eye_center(self, sg: int) -> Vector:
        return self.head_point(3.1, sg * 25.0, -self.hk(0.55))

    def _add_bones(self, specs) -> None:
        """Extra bones (name, head, tail, parent) — siblings allowed (feather fans)."""
        arm = self.rig.obj
        from kit import scene
        scene.select_only([arm])
        bpy.ops.object.mode_set(mode="EDIT")
        eb = arm.data.edit_bones
        for name, h, t, parent in specs:
            b = eb.new(name)
            b.head, b.tail = Vector(h), Vector(t)
            b.parent = eb[parent]
            y = (b.tail - b.head).normalized()
            b.align_roll(Vector((1, 0, 0)).cross(y) if abs(y.x) < 0.95 else Vector((0, 0, 1)))
            b.use_deform = True
        bpy.ops.object.mode_set(mode="OBJECT")
        for pb in arm.pose.bones:
            pb.rotation_mode = "QUATERNION"

    def _add_chains(self) -> None:
        """The SKEL_Goblin union of chains, placed for this mesh (unused ones carry no weights)."""
        J = self.J
        sp = []
        for s, sg in (("l", 1), ("r", -1)):
            base = self.ear_base(sg)
            mid, tip = self.ear_mid(sg), self.ear_tip(sg)
            sp += [(f"ear_{s}_01", base, mid, "head"), (f"ear_{s}_02", mid, tip, f"ear_{s}_01")]
        for fb, sg, nm in (("f", 1, "loin_f"), ("b", -1, "loin_b")):
            top, mid, bot = self.loin_points(sg)
            sp += [(f"{nm}_01", top, mid, "pelvis"), (f"{nm}_02", mid, bot, f"{nm}_01")]
        # pelt (chief mantle tail down the back), feather fan behind the head, totem feathers off the right hand
        t = self.torso
        p0, p1 = t.point(0.92, 180, self.u(1.4)), t.point(0.62, 180, self.u(2.6))
        p2, p3 = t.point(0.36, 180, self.u(3.4)), t.point(0.12, 180, self.u(3.9))
        sp += [("pelt_01", p0, p1, "spine_03"), ("pelt_02", p1, p2, "pelt_01"), ("pelt_03", p2, p3, "pelt_02")]
        for i, (r0, r1) in enumerate(self.fan_roots()):
            sp.append((f"headdress_{i + 1:02d}", r0, r1, "head"))
        lash = self.totem_lash()
        sp += [("totem_feather_01", lash, lash + Vector((0, 0.01, -0.07)), "weapon_r"),
               ("totem_feather_02", lash + Vector((-0.012, 0.0, 0.0)), lash + Vector((-0.02, 0.01, -0.065)),
                "weapon_r")]
        self._add_bones(sp)

    # placements of the chains (overridden by the chief / shaman for their gear)
    def ear_base(self, sg: int) -> Vector:
        return self.head_point(1.2, sg * 92.0, -self.hk(0.6))

    def ear_mid(self, sg: int) -> Vector:
        return self.hp(3.4, -1.9, sg * 9.4)

    def ear_tip(self, sg: int) -> Vector:
        return self.hp(5.6, -3.2, sg * 13.6)

    def loin_points(self, sg: int):
        s = 0.06
        a = 0.0 if sg > 0 else 180.0
        top = self.torso.point(s, a, self.u(0.9))
        L = self.u(7.8)
        top = top + Vector((0, 0, -self.u(0.6)))
        return top, top + Vector((0, -sg * self.u(0.4), -L * 0.5)), top + Vector((0, -sg * self.u(0.6), -L))

    def fan_roots(self):
        """Feather-fan bone roots → tips (behind the head, 4 feathers; the shaman's headdress)."""
        out = []
        for lat, ln in ((-3.4, 12.0), (-1.2, 13.5), (1.2, 12.5), (3.4, 11.0)):
            r0 = self.hp(5.2, -3.4, lat)
            r1 = self.hp(5.2 + ln * 0.55, -3.4 - ln * 0.75, lat * 2)
            out.append((r0, r0 + (r1 - r0) * 0.5))
        return out

    def totem_lash(self) -> Vector:
        """Where the shaman totem's feathers are tied (weapon_r space: 0.36 m up the staff)."""
        rb = self.rig.obj.data.bones["weapon_r"]
        return rb.matrix_local @ Vector((0.0, 0.36, 0.0))

    # ── torso ──────────────────────────────────────────────────────────────────────────────────────────
    def spine_points(self) -> list[Vector]:
        r = self.rig
        return [r.head("pelvis"), r.head("spine_01"), r.head("spine_02"), r.head("spine_03"), r.tail("spine_03")]

    def torso_rings(self):
        k = self.w.bulk
        u = self.u
        return [(-0.16, u(3.6 * k), u(3.3 * k), u(0.0)), (-0.05, u(4.7 * k), u(4.2 * k), u(0.4)),
                (0.10, u(5.3 * k), u(5.0 * k), u(0.8)), (0.28, u(5.6 * k), u(5.7 * k), u(1.25)),
                (0.45, u(5.6 * k), u(5.9 * k), u(1.35)), (0.62, u(5.4 * k), u(5.4 * k), u(0.9)),
                (0.78, u(5.2 * k), u(4.6 * k), u(0.2)), (0.90, u(4.8 * k), u(3.8 * k), u(-0.35)),
                (0.98, u(3.7 * k), u(2.9 * k), u(-0.5)), (1.05, u(2.5), u(2.3), u(-0.3))]

    def torso_loft(self) -> G.SpineLoft:
        return G.SpineLoft(self.spine_points(), self.torso_rings(), exp=2.15)

    # ── build the shared parts ─────────────────────────────────────────────────────────────────────────
    def build_parts(self, b: M.Builder, *, skip=(), necklace: bool = True, sash: bool = True,
                    loincloth: bool = True, belt: bool = True, ears: bool = True) -> dict:
        ids = {}
        ids.update(self.add_head(b, ears=ears))
        ids.update(self.add_torso(b, necklace=necklace, sash=sash, belt=belt))
        if loincloth:
            self.add_loincloth(b)
        for s, sg in (("l", 1), ("r", -1)):
            self.add_arm(b, s, sg)
            self.add_leg(b, s, sg)
        return ids

    def add_head(self, b: M.Builder, ears: bool = True) -> dict:
        hk = self.hk
        hd = self.head
        zs = sorted(set([hk(h) for h in np.linspace(-5.6, 7.05, 16)]))
        skull = hd.loft(zs, sides=28, cap0="fan", cap1="fan", dome0=hk(0.25), dome1=hk(0.05))
        # jaw: the lower face follows the jaw bone (grin opens), the cranium the head
        Hinv = self.Hm.inverted()
        hku = hk(1.0)

        def jaw_w(co):
            loc = (np.c_[co, np.ones(len(co))] @ np.array(Hinv).T)[:, :3] / hku
            h, f = loc[:, 2], -loc[:, 1]
            wj = (1.0 - _smooth_w(h, -3.6, -2.2)) * _smooth_w(f, -2.5, 1.0)
            return np.stack([1.0 - wj, wj], axis=1)
        jbind = Bind.custom(("head", "jaw"), jaw_w)
        skull_id = b.add(skull, "skin", jbind, name="skull", hull_pad=True)
        ids = {"skull": skull_id}
        # brow ridge: a heavy darker ridge over the eyes (web surfPatch h 4.4–6, ±54°)
        ids["brow"] = b.add(self._closed_brow(), "skin_dark", Bind.rigid("head"), name="brow", hull_pad=True)
        # eyes: yellow, slit pupils, set under the brow
        for s, sg in (("l", 1), ("r", -1)):
            ec = self.eye_center(sg)
            n = self.head_normal(3.1, sg * 25.0)
            up = self.hdir(1, 0)
            m = G.frame_z(ec, n, up)
            eye = M.ellipsoid((hk(1.75), hk(1.35), hk(1.15)), (0, 0, 0), sides=14, rings=8)
            eye.scale((1.0, 1.0, 1.0)).transform(m @ Matrix.Rotation(math.radians(-sg * 8), 4, "Z"))
            b.add(eye, "eye", Bind.rigid(f"eye_{s}"), name=f"eye_{s}", outline=False, covered_by=skull_id)
            pup = M.ellipsoid((hk(0.34), hk(1.0), hk(0.25)), (0, 0, 0), sides=8, rings=6)
            pup.transform(m @ Matrix.Translation((sg * hk(0.15), 0.0, hk(1.03))))
            b.add(pup, "pupil", Bind.rigid(f"eye_{s}"), name=f"pupil_{s}", outline=False, covered_by=skull_id)
        # hooked nose (web view: root h 3.6 f 6 → tip h 0.2 f 10.6 → base h −1 f 6.2, nostrils ±1.8)
        path = [self.hp(3.8, 5.0), self.hp(3.1, 7.3), self.hp(2.0, 9.4), self.hp(0.7, 10.8), self.hp(-0.4, 10.7),
                self.hp(-1.0, 10.0)]
        radii = [(hk(1.15), hk(1.05)), (hk(1.2), hk(0.98)), (hk(1.08), hk(0.88)), (hk(0.82), hk(0.68)),
                 (hk(0.5), hk(0.42)), (hk(0.22), hk(0.2))]
        nose = M.tube(path, radii, sides=10, up=tuple(self.hdir(0, 1)), dome0=0.0, dome1=hk(0.15))
        # no hull of its own: an inverted hull inked the face down both sides of the nose (front views); the
        # skull's padded hull encloses it, so it is inked only where it forms the head silhouette (profiles)
        ids["nose"] = b.add(nose, "skin", Bind.rigid("head"), name="nose", outline=False, covered_by=skull_id)
        for sg in (1, -1):                     # nostril wings
            nw = M.ellipsoid((hk(0.85), hk(0.95), hk(0.75)), (0, 0, 0), sides=10, rings=6)
            nw.transform(Matrix.Translation(self.hp(0.0, 7.6, sg * 1.3)))
            b.add(nw, "skin", Bind.rigid("head"), name="nostril", outline=False, covered_by=skull_id)
        # grin (web surfPatch h −1.2..−2.2, ±0.62 rad, lower edge dipping with ``grin``) + three teeth
        grin = self.w.grin
        mp = self._grin_part(grin)
        b.add(mp, "mouth", jbind, name="mouth", outline=False, covered_by=skull_id)
        for i, a in enumerate((-19.0, -1.0, 17.0)):
            top = self.head_point(-2.15, a, hk(0.2))
            n = self.head_normal(-2.15, a)
            dn = -self.hdir(1, 0)
            tooth = G.cone(top - dn * hk(0.15), top + dn * hk(1.15 if i != 1 else 0.95) + n * hk(0.15),
                           hk(0.42), sides=5, r_tip=hk(0.08))
            b.add(tooth, "bone", jbind, name="tooth", outline=False, covered_by=skull_id)
        if ears:
            for s, sg in (("l", 1), ("r", -1)):
                ids[f"ear_{s}"] = self.add_ear(b, s, sg)
        # neck
        J = self.J
        n0 = J["neck"] + Vector((0, 0.004, -0.02))
        n1 = self.hp(-2.6, -0.6)
        neck = M.tube([n0, n0.lerp(n1, 0.5), n1], [self.u(2.5), self.u(2.2), self.u(2.4)], sides=12)
        b.add(neck, "skin", Bind.blend("spine_03", "neck_01", "head", falloff=3.0), name="neck")
        return ids

    def _closed_brow(self) -> M.Part:
        """The brow ridge: a sweep across the face above the eyes, thick in the middle, tapering to the temples,
        overhanging the eyes (dark skin)."""
        hk = self.hk
        pts, rad = [], []
        for a in np.linspace(-66, 66, 15):
            t = abs(a) / 66.0
            dip = 0.35 * math.cos(math.radians(a) * 2.6)             # sags toward the nose bridge
            pts.append(self.head_point(4.75 - 0.35 * dip, a, hk(0.18 + 0.35 * (1 - t))))
            rad.append((hk(0.95 * (1 - 0.55 * t ** 2)), hk(0.62 * (1 - 0.5 * t ** 2))))
        up = [tuple(self.hdir(1, 0))] * len(pts)
        return M.sweep(pts, M.superellipse(1, 1, 10, 2.4), scales=rad, up=up[0], dome0=hk(0.2), dome1=hk(0.2))

    def _grin_part(self, grin: float) -> M.Part:
        """Dark mouth crescent lying on the face (upper lip straight, lower edge dipping: a grin)."""
        hk = self.hk
        surf = _HeadSurf(self)
        grid = []
        for j in range(13):
            ta = j / 12
            a = lerp(-46.0, 46.0, ta)
            corner = abs(ta - 0.5) * 2
            top_h = -2.0 - 0.2 * (1 - corner) + 0.55 * corner ** 2            # corners curl up
            low_h = top_h - (0.45 + (0.6 + grin * 0.7) * math.sin(ta * math.pi)) * (1 - 0.15 * corner)
            grid.append([surf.point(lerp(top_h, low_h, k / 3), a, hk(0.1)) for k in range(4)])
        import bmesh
        bm = bmesh.new()
        vs = [[bm.verts.new(tuple(p)) for p in row] for row in grid]
        for r in range(len(vs) - 1):
            for c in range(3):
                bm.faces.new((vs[r][c], vs[r + 1][c], vs[r + 1][c + 1], vs[r][c + 1]))
        bm.normal_update()
        f = next(iter(bm.faces))
        if f.normal.dot(self.head_normal(-1.6, 0.0)) < 0:
            bmesh.ops.reverse_faces(bm, faces=list(bm.faces))
        # give it a little thickness (closed solid) so it never z-fights the skull
        bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=-hk(0.18))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return M.Part(bm).smooth(True)

    def add_ear(self, b: M.Builder, s: str, sg: int) -> int:
        hk = self.hk
        base, tip = self.ear_base(sg), self.ear_tip(sg)
        d = tip - base
        L = d.length
        # the ear's open face looks forward and a little outward; the cup opens toward it
        face = (self.hdir(0.3, 1.0, sg * 0.55)).normalized()
        m = R.frame(base - d.normalized() * hk(0.6), d, -face)      # leaf front face is −Z → toward ``face``
        ear = G.leaf(L + hk(0.6), hk(5.4), hk(0.55), sides=9, curl_deg=-38.0, bend_deg=-14.0, taper=0.75, tip=1.6,
                     base_round=0.30)
        ear.transform(m)
        bind = Bind.blend(f"ear_{s}_01", f"ear_{s}_02", falloff=3.0, smooth=0.01)
        eid = b.add(ear, "skin", bind, name=f"ear_{s}", hull_pad=True)
        inner = G.leaf(L * 0.78, hk(3.2), hk(0.3), sides=8, curl_deg=-44.0, bend_deg=-12.0, taper=0.8, tip=1.5,
                       base_round=0.35)
        inner.transform(m @ Matrix.Translation((0.0, hk(0.9), -hk(0.32))))
        b.add(inner, "ear_in", bind, name=f"ear_in_{s}", outline=False, covered_by=eid)
        return eid

    # torso parts ------------------------------------------------------------------------------------------
    def add_torso(self, b: M.Builder, necklace: bool = True, sash: bool = True, belt: bool = True) -> dict:
        t = self.torso
        u = self.u
        spine = Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", falloff=3.0, smooth=0.02)
        body = t.loft(-0.16, 1.05, ss=list(np.linspace(-0.16, 1.05, 16)), sides=26, dome0=u(0.6), cap1=None)
        rest, bel = G.split_by(body, lambda s, a, c: 0.12 < s < 0.62 and abs(a) < 42
                               and ((s - 0.36) / 0.26) ** 2 + (a / 42.0) ** 2 < 1.0)
        tid = b.add(rest, "skin", spine, name="torso",
                    hull=t.loft(-0.16, 1.05, ss=list(np.linspace(-0.16, 1.05, 11)), sides=20, dome0=u(0.6),
                                cap1="fan"), hull_pad=True)
        b.add(bel, "belly", spine, name="belly", outline=False, covered_by=tid)
        ids = {"torso": tid}
        if belt:
            ids["belt"] = self.add_rope_belt(b, tid)
        if sash:
            for path in ([(0.97, 92.0), (0.70, 40.0), (0.45, 0.0), (0.22, -52.0), (0.10, -74.0)],
                         [(0.97, 92.0), (0.70, 140.0), (0.45, 180.0), (0.22, 232.0), (0.10, 254.0)]):
                st = G.surface_strap(t, path, u(1.5), u(0.32), steps=26)
                b.add(st, "strap", spine, name="sash", outline=False, covered_by=tid)
        if necklace:
            self.add_necklace(b, tid)
        # shoulders (deltoids round the arm roots)
        for s, sg in (("l", 1), ("r", -1)):
            S = self.J[f"shoulder_{s}"]
            d = M.ellipsoid((u(2.6), u(2.5), u(2.5)), S + Vector((-sg * u(0.3), 0.0, u(0.2))), sides=14, rings=8)
            b.add(d, "skin", Bind.blend(f"clavicle_{s}", f"upperarm_{s}", "spine_03", falloff=4.0),
                  name=f"deltoid_{s}")
        return ids

    def add_rope_belt(self, b: M.Builder, tid: int, s0: float = 0.10, region: str = "rope",
                      stroke: str | None = "rope_line", width_u: float = 1.6, thick_u: float = 0.7) -> int:
        """Rope belt: a round cord band with diagonal twist strokes and a knot at the front-left."""
        t = self.torso
        u = self.u
        band = G.surface_ring(t, lambda a: s0 + 0.012 * math.cos(math.radians(a)), u(width_u), u(thick_u),
                              segs=40)
        bind = Bind.blend("pelvis", "spine_01", falloff=3.0)
        bid = b.add(band, region, bind, name="belt", outline=False, covered_by=tid)
        if stroke:
            for k in range(20):
                a = -180 + k * 18 + 4
                st = G.surface_strap(t, [(s0 - 0.045, a - 4), (s0 + 0.045, a + 6)], u(0.42), u(thick_u) + 0.0015,
                                     steps=3)
                b.add(st, stroke, bind, name="belt_twist", outline=False, covered_by=tid)
        knot = M.ellipsoid((u(1.2), u(0.9), u(1.0)), t.point(s0, 28.0, u(thick_u) + u(0.4)), sides=10, rings=6)
        b.add(knot, region, bind, name="belt_knot", outline=False, covered_by=tid)
        for dz, da in ((-1.0, 22.0), (-1.0, 34.0)):
            p0 = t.point(s0, da, u(thick_u) + u(0.3))
            end = M.tube([p0, p0 + Vector((0, -u(0.5), -u(2.4))), p0 + Vector((0, -u(0.7), -u(3.6)))],
                         [u(0.42), u(0.38), u(0.32)], sides=6)
            b.add(end, region, bind, name="belt_end", outline=False, covered_by=tid)
            del dz
        return bid

    def add_necklace(self, b: M.Builder, tid: int) -> None:
        t = self.torso
        u = self.u
        bind = Bind.blend("spine_03", "spine_02", falloff=3.0)
        cord = G.surface_ring(t, lambda a: 0.90 - 0.06 * math.cos(math.radians(a)) ** 2, u(0.4), u(0.25), segs=28)
        b.add(cord, "strap", bind, name="neck_cord", outline=False, covered_by=tid)
        for i in range(5):
            a = -48.0 + i * 24.0
            s = 0.90 - 0.06 * math.cos(math.radians(a)) ** 2 - 0.012
            top = t.point(s, a, u(0.35))
            n = t.normal(s, a)
            down = (t.point(s - 0.06, a) - t.point(s, a)).normalized()
            tooth = G.cone(top, top + down * u(2.3) + n * u(0.35), u(0.55), sides=5)
            b.add(tooth, "bone", bind, name="neck_tooth", outline=False, covered_by=tid)

    def add_loincloth(self, b: M.Builder, region: str = "cloth", lining: str = "cloth_in", w_top: float = 5.4,
                      w_bot: float = 4.6, length: float = 8.0, back_k: float = 1.0) -> None:
        u = self.u
        for nm, sg in (("loin_f", 1), ("loin_b", -1)):
            top, mid, bot = self.loin_points(sg)
            L = (bot - top).length
            k = 1.0 if sg > 0 else back_k
            fl = G.flap(u(w_top) * k, u(w_bot) * k, L, thick=0.007, cols=6, rows=5, notch=u(1.5), tails=2,
                        ragged=u(0.35), seed=7 + sg, bulge=u(0.3), flare=u(0.05))
            rot = Matrix.Identity(4) if sg > 0 else Matrix.Rotation(math.pi, 4, "Z")
            fl.transform(Matrix.Translation(top) @ rot)
            b.add(fl, region, chain_bind(top, bot, ("pelvis", f"{nm}_01", f"{nm}_02")), name=nm,
                  sub_regions={1: region, 2: lining}, hull_pad=True)

    # limbs ------------------------------------------------------------------------------------------------
    def add_arm(self, b: M.Builder, s: str, sg: int, r_scale: float = 1.0) -> None:
        J = self.J
        u = self.u
        S, E, W = J[f"shoulder_{s}"], J[f"elbow_{s}"], J[f"wrist_{s}"]
        k = r_scale
        pts = [S + (S - E).normalized() * u(0.4), S.lerp(E, 0.5), E, E.lerp(W, 0.5), W]
        rad = [u(2.15 * k), u(1.95 * k), u(1.75 * k), u(1.75 * k), u(1.5 * k)]
        arm = M.tube(pts, rad, sides=12, dome0=u(0.3), dome1=u(0.3))
        bind = Bind.blend(f"clavicle_{s}", f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}", falloff=4.0, smooth=0.008)
        b.add(arm, "skin", bind, name=f"arm_{s}")
        # knobbly elbow
        eb = M.ellipsoid((u(1.9 * k), u(1.95 * k), u(1.9 * k)), E + (E - S).normalized() * u(0.1)
                         + Vector((sg * u(0.25), u(0.35), 0)), sides=10, rings=6)
        b.add(eb, "skin", Bind.blend(f"upperarm_{s}", f"lowerarm_{s}", falloff=4.0), name=f"elbow_{s}")
        self.add_fist(b, s, sg)

    def add_fist(self, b: M.Builder, s: str, sg: int) -> int:
        """Big goblin fist (web ellipse 2.3 × 2.1 u): rounded mitten curled round the grip, knuckle bumps, a
        thumb over the fingers and three bone-dark claws."""
        J = self.J
        u = self.u
        W, K, T = J[f"wrist_{s}"], J[f"knuckles_{s}"], J[f"handtip_{s}"]
        hb = self.rig.obj.data.bones[f"hand_{s}"]
        X = hb.matrix_local.to_3x3() @ Vector((1, 0, 0))       # hinge (curl axis)
        d = (T - W).normalized()
        palm = X.cross(d).normalized()                           # toward the palm side (fingers curl here)
        fwd_w = Vector((0, -1, 0))
        if palm.dot(fwd_w) < 0:
            palm = -palm
        m = R.frame(W, d, palm)
        fist = M.ellipsoid((u(1.95), u(2.3), u(1.8)), (0, u(2.0), u(0.3)), sides=12, rings=8)
        fist.transform(m)
        hb_bind = Bind.blend(f"hand_{s}", f"fingers_01_{s}", f"lowerarm_{s}", falloff=5.0)
        fid = b.add(fist, "skin", hb_bind, name=f"fist_{s}", hull_pad=True)
        for i, lat in enumerate((-1.0, 0.0, 1.0)):
            kn = m @ Vector((lat * u(0.95), u(3.3), u(1.45)))
            b.add(M.ellipsoid((u(0.62), u(0.62), u(0.55)), kn, sides=8, rings=6), "skin", hb_bind,
                  name=f"knuckle_{s}", outline=False, covered_by=fid)
            c0 = m @ Vector((lat * u(0.9), u(3.9), u(0.7)))
            c1 = m @ Vector((lat * u(0.9), u(4.25), u(-0.25)))
            b.add(G.cone(c0, c1, u(0.42), sides=5), "bone_dark", hb_bind, name=f"claw_{s}", outline=False,
                  covered_by=fid)
        th = M.capsule(m @ Vector((-sg * u(1.4), u(1.0), u(1.2))), m @ Vector((-sg * u(0.9), u(2.6), u(1.9))),
                       u(0.75), u(0.62), sides=8, cap_rings=1, mid=0)
        b.add(th, "skin", hb_bind, name=f"thumb_{s}", outline=False, covered_by=fid)
        return fid

    def add_leg(self, b: M.Builder, s: str, sg: int, r_scale: float = 1.0) -> None:
        J = self.J
        u = self.u
        k = r_scale
        Hp, Kn, A = J[f"hip_{s}"], J[f"knee_{s}"], J[f"ankle_{s}"]
        top = Hp + Vector((0, 0, u(1.6)))
        pts = [top, Hp.lerp(Kn, 0.45), Kn, Kn.lerp(A, 0.4), A.lerp(Kn, 0.08)]
        rad = [u(2.55 * k), u(2.35 * k), u(1.95 * k), u(1.9 * k), u(1.35 * k)]
        leg = M.tube(pts, rad, sides=12, dome0=u(0.8), dome1=u(0.2))
        bind = Bind.blend("pelvis", f"thigh_{s}", f"calf_{s}", f"foot_{s}", falloff=4.0, smooth=0.008)
        b.add(leg, "skin", bind, name=f"leg_{s}")
        kn = M.ellipsoid((u(2.05 * k), u(1.85 * k), u(2.0 * k)), Kn + Vector((0, -u(0.5), u(0.1))), sides=10, rings=6)
        b.add(kn, "skin", Bind.blend(f"thigh_{s}", f"calf_{s}", falloff=4.0), name=f"knee_{s}")
        self.add_foot(b, s, sg)

    def add_foot(self, b: M.Builder, s: str, sg: int) -> int:
        """Long pointed bare goblin foot (web goblinFoot: heel 2.4 u behind the ankle, toe 6 u ahead, a bone claw
        at the tip): a swept section tapering to an upturned point, bone toe claws."""
        J = self.J
        u = self.u
        A = J[f"ankle_{s}"]
        L = self.spec.foot_len
        x = A.x
        st = [  # (y, half-width, half-height, centre z)
            (0.24 * L, u(1.05), u(1.1), u(1.15)),
            (0.10 * L, u(1.6), u(1.55), u(1.45)),
            (-0.12 * L, u(1.75), u(1.25), u(1.15)),
            (-0.38 * L, u(1.7), u(0.95), u(0.92)),
            (-0.60 * L, u(1.25), u(0.72), u(0.78)),
            (-0.78 * L, u(0.62), u(0.45), u(0.85)),
        ]
        path = [Vector((x, y, cz)) for y, _, _, cz in st]
        foot = M.sweep(path, M.superellipse(1, 1, 12, 2.6), scales=[(hh, hw) for _, hw, hh, _ in st], up=(0, 0, 1),
                       dome0=u(0.6), dome1=u(0.35))
        foot.deform(lambda co: Vector((co.x, co.y, max(co.z, 0.0))))
        fb = Bind.blend(f"calf_{s}", f"foot_{s}", f"ball_{s}", falloff=5.0)
        fid = b.add(foot.smooth(True), "skin", fb, name=f"foot_{s}", hull_pad=True)
        for i, lat in enumerate((-0.75, 0.0, 0.75)):
            base = Vector((x + lat * u(1.0), -0.70 * L + abs(lat) * u(0.9), u(0.55)))
            tip = base + Vector((lat * u(0.25), -u(1.15 if lat == 0 else 0.85), -u(0.25)))
            b.add(G.cone(base, tip, u(0.45 if lat == 0 else 0.38), sides=5), "bone",
                  Bind.blend(f"foot_{s}", f"ball_{s}", falloff=5.0), name=f"toe_claw_{s}", outline=False,
                  covered_by=fid)
        return fid


class _HeadSurf:
    """Adapter: the head profile with web head units for ``u`` (h) — for ``mgeo`` surface helpers."""

    def __init__(self, kit: GoblinKit):
        self.k = kit

    def point(self, h, a, out=0.0):
        return self.k.head_point(h, a, out)

    def normal(self, h, a):
        return self.k.head_normal(h, a)
