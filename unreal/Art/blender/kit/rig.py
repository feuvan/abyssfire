"""Humanoid armature builder (``SK_Humanoid`` → SKEL_Human / SKEL_Goblin) and skin-weight helpers.

Bone names follow the UE mannequin (ARCHITECTURE.md §5, spec art-inventory-ch1.md §2.4):
``root → pelvis → spine_01..03 → neck_01 → head``, ``clavicle_l/r → upperarm → lowerarm → hand →
(thumb_01, fingers_01)``, ``thigh → calf → foot → ball``, item bones ``weapon_r`` / ``weapon_l`` (children of the
hands), IK helpers ``ik_foot_root/ik_foot_l/r``, ``ik_hand_root/ik_hand_gun/ik_hand_l/r`` (no weights, kept in sync
with the FK bones by constraints so the exporter bakes them), optional face bones ``jaw, eye_l, eye_r`` and any
secondary-motion chain (``cape_c_01..``) passed in ``chains``.

Conventions: character faces **−Y**, +Z up, root at the ground point between the feet, rest pose = A-pose
(arms ``arm_down`` degrees below horizontal), legs straight with a 2–3° knee pre-bend so IK always bends the
right way. Limb bones are rolled so their local **X axis is the joint hinge** (knee/elbow bend about +X).
``weapon_*`` bones: head = grip centre, local **+Y = blade/tip direction** (forward in the rest pose),
**+Z = up**, so weapon meshes are authored with the grip at the origin and the blade along +Y.

The same builder makes ``SK_Goblin`` (``HumanoidSpec(skeleton='SKEL_Goblin', rest_lean=…)`` + goblin chains).
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field, replace
from typing import Iterable, Sequence

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

from . import scene

UP = Vector((0, 0, 1))
FWD = Vector((0, -1, 0))   # character forward
LEFT = Vector((1, 0, 0))   # character left (+X, since the character faces −Y)

CORE_BONES = (
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "thumb_01_l", "fingers_01_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "thumb_01_r", "fingers_01_r",
    "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r",
    "weapon_r", "weapon_l",
)
IK_BONES = ("ik_foot_root", "ik_foot_l", "ik_foot_r", "ik_hand_root", "ik_hand_gun", "ik_hand_l", "ik_hand_r")
FACE_BONES = ("jaw", "eye_l", "eye_r")
NO_WEIGHT_BONES = set(IK_BONES) | {"root", "weapon_r", "weapon_l"}


@dataclass
class HumanoidSpec:
    """Proportions in metres (rest A-pose). Defaults ≈ the web warrior (spec §3.1, 3.01 cm/unit)."""

    name: str = "SK_Humanoid"
    skeleton: str = "SKEL_Human"
    thigh: float = 0.376
    shin: float = 0.361
    ankle: float = 0.078          # sole → ankle joint
    torso: float = 0.497          # pelvis → neck base
    neck: float = 0.217           # neck base → head centre
    head: float = 0.46            # head height (incl. helm / hood)
    hip_half: float = 0.087       # hip joint half-width
    shoulder_half: float = 0.168  # shoulder joint half-width
    upper_arm: float = 0.30
    fore_arm: float = 0.286
    hand: float = 0.115           # wrist → mitten tip
    foot_len: float = 0.28        # heel → toe (oversized ×1.3, spec §1.6)
    arm_down: float = 50.0        # A-pose: degrees below horizontal
    elbow_prebend: float = 8.0
    knee_prebend: float = 3.0
    rest_lean: float = 0.0        # radians, forward hunch of the rest spine (goblins 0.24–0.34)
    rest_head: float = 0.0        # radians, head counter-pitch (− looks up)
    face: bool = False            # jaw / eye bones
    ik_bones: bool = True
    chains: list = field(default_factory=list)   # [(prefix, parent_bone, [points...]), ...]

    def crown_height(self) -> float:
        hip = self.ankle + self.shin + self.thigh
        pz = hip + self.pelvis_offset
        # spine and head follow the rest lean
        dz = self.torso * _avg_cos(self.rest_lean) + self.neck * math.cos(self.rest_lean + self.rest_head)
        return pz + dz + self.head * 0.5

    @property
    def pelvis_offset(self) -> float:
        return 0.03 * (self.torso / 0.5)

    def scaled(self, k: float) -> "HumanoidSpec":
        f = ("thigh", "shin", "ankle", "torso", "neck", "head", "hip_half", "shoulder_half", "upper_arm",
             "fore_arm", "hand", "foot_len")
        out = replace(self, **{n: getattr(self, n) * k for n in f})
        out.chains = [(p, b, [Vector(pt) * k for pt in pts]) for (p, b, pts) in self.chains]
        return out

    def fit_height(self, crown: float) -> "HumanoidSpec":
        """Uniformly scale so the crown (top of head/helm) is at ``crown`` metres."""
        return self.scaled(crown / self.crown_height())

    @staticmethod
    def from_web(units: dict, cm_per_unit: float, **over) -> "HumanoidSpec":
        """Build from web rig ``Proportions`` (96-unit rig space) using the spec §0.1 cm/unit table."""
        k = cm_per_unit / 100.0
        m = {"thigh": "thigh", "shin": "shin", "upperArm": "upper_arm", "foreArm": "fore_arm",
             "torso": "torso", "neck": "neck", "ankle": "ankle", "head": "head", "hip": "hip_half",
             "shoulder": "shoulder_half", "hand": "hand", "foot": "foot_len"}
        kw = {m[key]: v * k for key, v in units.items() if key in m}
        kw.update(over)
        return HumanoidSpec(**kw)


def _avg_cos(lean: float) -> float:
    # spine segments lean progressively (1/3, 2/3, 3/3 of the rest lean); weights 0.12/0.28/0.28/0.32
    segs = ((0.12, 0.0), (0.28, lean / 3), (0.28, 2 * lean / 3), (0.32, lean))
    return sum(w * math.cos(a) for w, a in segs)


@dataclass
class Rig:
    """A built armature plus its rest-pose landmarks (armature space, metres)."""

    obj: bpy.types.Object
    spec: HumanoidSpec
    joints: dict
    sockets: list

    @property
    def name(self) -> str:
        return self.obj.name

    def joint(self, name: str) -> Vector:
        return self.joints[name].copy()

    def rest(self, bone: str) -> Matrix:
        """Rest matrix (armature space) of a bone: translation = head, columns = local axes."""
        return self.obj.data.bones[bone].matrix_local.copy()

    def head(self, bone: str) -> Vector:
        return self.obj.data.bones[bone].head_local.copy()

    def tail(self, bone: str) -> Vector:
        return self.obj.data.bones[bone].tail_local.copy()

    def bone_names(self) -> list[str]:
        return [b.name for b in self.obj.data.bones]

    def deform_bones(self) -> list[str]:
        return [b.name for b in self.obj.data.bones if b.use_deform]

    def weightable_bones(self) -> list[str]:
        return [b.name for b in self.obj.data.bones if b.use_deform and b.name not in NO_WEIGHT_BONES]


def frame(origin: Sequence[float], y_dir: Sequence[float], z_hint: Sequence[float]) -> Matrix:
    """4×4 frame with +Y along ``y_dir`` and +Z as close as possible to ``z_hint``."""
    y = Vector(y_dir).normalized()
    z = Vector(z_hint)
    z = (z - y * z.dot(y))
    if z.length < 1e-6:
        z = Vector((0, 0, 1)) if abs(y.z) < 0.9 else Vector((0, -1, 0))
        z = (z - y * z.dot(y))
    z.normalize()
    x = y.cross(z)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0], m[i][1], m[i][2], m[i][3] = x[i], y[i], z[i], origin[i]
    return m


def _rot(v: Vector, axis: Vector, deg: float) -> Vector:
    return Matrix.Rotation(math.radians(deg), 3, axis) @ v


def build_humanoid(spec: HumanoidSpec, collection=None) -> Rig:
    """Create the armature object (named ``spec.name``) in rest A-pose and return a :class:`Rig`."""
    scene.ensure_object_mode()
    arm_data = bpy.data.armatures.new(spec.skeleton)
    obj = bpy.data.objects.new(spec.name, arm_data)
    scene.link(obj, collection)
    arm_data.display_type = "STICK"
    obj.show_in_front = True
    scene.select_only([obj])
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm_data.edit_bones
    J: dict[str, Vector] = {}

    def bone(name, head, tail, parent=None, x_axis=None, roll_z=None, deform=True, connect=False):
        b = eb.new(name)
        b.head = Vector(head)
        b.tail = Vector(tail)
        if parent:
            b.parent = eb[parent]
            b.use_connect = connect
        y = (b.tail - b.head).normalized()
        if x_axis is not None:
            x = Vector(x_axis)
            x = (x - y * x.dot(y)).normalized()
            b.align_roll(x.cross(y))           # Z = X × Y
        elif roll_z is not None:
            b.align_roll(Vector(roll_z))
        else:
            b.align_roll(LEFT.cross(y) if abs(y.dot(LEFT)) < 0.95 else UP)
        b.use_deform = deform
        return b

    hip_z = spec.ankle + spec.shin + spec.thigh
    pz = hip_z + spec.pelvis_offset
    P = Vector((0, 0, pz))
    J["pelvis"] = P.copy()
    bone("root", (0, 0, 0), (0, 0, 0.1 * spec.torso / 0.5))
    # spine: progressive lean (+ = forward = toward −Y)
    fr = [0.12, 0.40, 0.68, 1.0]
    leans = [0.0, spec.rest_lean / 3, 2 * spec.rest_lean / 3, spec.rest_lean]
    pts = [P.copy()]
    prev = 0.0
    for f, a in zip(fr, leans):
        seg = (f - prev) * spec.torso
        d = Vector((0, -math.sin(a), math.cos(a)))
        pts.append(pts[-1] + d * seg)
        prev = f
    names = ["pelvis", "spine_01", "spine_02", "spine_03"]
    parents = ["root", "pelvis", "spine_01", "spine_02"]
    for i, nm in enumerate(names):
        bone(nm, pts[i], pts[i + 1], parents[i], x_axis=LEFT, connect=i > 0)
    N = pts[-1]
    J["neck"] = N.copy()
    J["chest"] = pts[3].lerp(pts[4], 0.5)
    hd = Vector((0, -math.sin(spec.rest_lean + spec.rest_head), math.cos(spec.rest_lean + spec.rest_head)))
    head_c = N + hd * spec.neck
    pivot_len = max(0.03, spec.neck - 0.30 * spec.head)
    H0 = N + hd * pivot_len
    crown = head_c + hd * (0.5 * spec.head)
    J["head_center"], J["head_pivot"], J["crown"] = head_c, H0, crown
    bone("neck_01", N, H0, "spine_03", x_axis=LEFT, connect=True)
    bone("head", H0, crown, "neck_01", x_axis=LEFT, connect=True)
    if spec.face:
        jaw0 = H0 + FWD * 0.02 * spec.head / 0.46
        bone("jaw", jaw0, head_c + FWD * 0.42 * spec.head - hd * 0.32 * spec.head, "head", x_axis=LEFT)
        for side, sx in (("l", 1), ("r", -1)):
            e = head_c + Vector((sx * 0.17 * spec.head, 0, 0)) + FWD * 0.36 * spec.head + hd * 0.02 * spec.head
            J[f"eye_{side}"] = e
            bone(f"eye_{side}", e, e + FWD * 0.04, "head", roll_z=UP)

    # spine frame for shoulders (follows the chest lean)
    up3 = (pts[4] - pts[3]).normalized()
    a = math.radians(spec.arm_down)
    for side, sx in (("l", 1.0), ("r", -1.0)):
        C = pts[3] + up3 * (0.62 * (pts[4] - pts[3]).length) + Vector((sx * 0.02 * spec.torso / 0.5, 0, 0)) \
            + FWD * 0.015
        S = N - up3 * 0.10 * spec.torso + Vector((sx * spec.shoulder_half, 0, 0))
        J[f"shoulder_{side}"] = S
        bone(f"clavicle_{side}", C, S, "spine_03", roll_z=UP)
        dA = Vector((sx * math.cos(a), 0, -math.sin(a)))
        hinge = dA.cross(FWD)          # rotating about d × fwd moves the tip forward
        E = S + dA * spec.upper_arm
        dF = _rot(dA, hinge, spec.elbow_prebend)
        W = E + dF * spec.fore_arm
        J[f"elbow_{side}"], J[f"wrist_{side}"] = E, W
        # elbow hinge axis X = cross(upper, fore) so IK bends the forearm forward
        hx = dA.cross(dF).normalized()
        bone(f"upperarm_{side}", S, E, f"clavicle_{side}", x_axis=hx, connect=True)
        bone(f"lowerarm_{side}", E, W, f"upperarm_{side}", x_axis=hx, connect=True)
        K = W + dF * 0.5 * spec.hand
        T = W + dF * spec.hand
        J[f"knuckles_{side}"], J[f"handtip_{side}"] = K, T
        bone(f"hand_{side}", W, K, f"lowerarm_{side}", x_axis=hx, connect=True)
        bone(f"fingers_01_{side}", K, T, f"hand_{side}", x_axis=hx, connect=True)
        th0 = W + dF * 0.12 * spec.hand + FWD * 0.22 * spec.hand
        th1 = W + dF * 0.48 * spec.hand + FWD * 0.34 * spec.hand
        bone(f"thumb_01_{side}", th0, th1, f"hand_{side}", x_axis=hx)
        G = W + dF * 0.42 * spec.hand
        J[f"grip_{side}"] = G
        bone(f"weapon_{side}", G, G + FWD * 0.12 * spec.torso / 0.5, f"hand_{side}", roll_z=UP)

    # legs
    for side, sx in (("l", 1.0), ("r", -1.0)):
        Hp = Vector((sx * spec.hip_half, 0, hip_z))
        A = Vector((sx * spec.hip_half, 0, spec.ankle))
        # ankle straight below the hip; the knee is nudged forward (pre-bend) so IK bends it the right way
        Kn = Hp + Vector((0, -math.sin(math.radians(spec.knee_prebend)) * spec.thigh, -spec.thigh))
        ball = Vector((sx * spec.hip_half, -0.50 * spec.foot_len, 0.30 * spec.ankle))
        toe = Vector((sx * spec.hip_half, -0.78 * spec.foot_len, 0.30 * spec.ankle))
        heel = Vector((sx * spec.hip_half, 0.22 * spec.foot_len, 0.0))
        J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"] = Hp, Kn, A
        J[f"ball_{side}"], J[f"toe_{side}"], J[f"heel_{side}"] = ball, toe, heel
        bone(f"thigh_{side}", Hp, Kn, "pelvis", x_axis=LEFT)
        bone(f"calf_{side}", Kn, A, f"thigh_{side}", x_axis=LEFT, connect=True)
        bone(f"foot_{side}", A, ball, f"calf_{side}", x_axis=LEFT, connect=True)
        bone(f"ball_{side}", ball, toe, f"foot_{side}", x_axis=LEFT, connect=True)

    if spec.ik_bones:
        s = spec.torso / 0.5
        bone("ik_foot_root", (0, 0, 0), (0, 0, 0.1 * s), "root")
        bone("ik_hand_root", (0, 0, 0), (0, 0, 0.1 * s), "root")
        for side in ("l", "r"):
            fb = eb[f"foot_{side}"]
            bone(f"ik_foot_{side}", fb.head, fb.tail, "ik_foot_root", roll_z=fb.z_axis)
        hb = eb["hand_r"]
        bone("ik_hand_gun", hb.head, hb.tail, "ik_hand_root", roll_z=hb.z_axis)
        for side in ("l", "r"):
            hb = eb[f"hand_{side}"]
            bone(f"ik_hand_{side}", hb.head, hb.tail, "ik_hand_gun", roll_z=hb.z_axis)

    for prefix, parent, pts_ in spec.chains:
        prev_name = parent
        for i in range(len(pts_) - 1):
            nm = f"{prefix}_{i + 1:02d}"
            bone(nm, pts_[i], pts_[i + 1], prev_name, connect=i > 0)
            prev_name = nm

    bpy.ops.object.mode_set(mode="OBJECT")
    if spec.ik_bones:
        _ik_sync_constraints(obj)
    for pb in obj.pose.bones:
        pb.rotation_mode = "QUATERNION"
    sockets = default_sockets(obj, J, spec)
    rig = Rig(obj, spec, J, sockets)
    obj["af_skeleton"] = spec.skeleton
    return rig


def _ik_sync_constraints(obj: bpy.types.Object) -> None:
    """IK helper bones copy their FK counterparts (the FBX exporter bakes evaluated poses)."""
    pairs = {"ik_foot_l": "foot_l", "ik_foot_r": "foot_r", "ik_hand_gun": "hand_r",
             "ik_hand_l": "hand_l", "ik_hand_r": "hand_r"}
    for ik, fk in pairs.items():
        pb = obj.pose.bones.get(ik)
        if pb is None:
            continue
        c = pb.constraints.new("COPY_TRANSFORMS")
        c.target = obj
        c.subtarget = fk


def default_sockets(obj, J: dict, spec: HumanoidSpec) -> list[dict]:
    """fx sockets of spec §2.4 / §3.7 as (bone, armature-space rest location)."""
    height = spec.crown_height()
    chest = Vector((0, 0, 0.55 * height))
    out = [
        {"name": "fx_feet", "bone": "root", "pos": Vector((0, 0, 0))},
        {"name": "fx_chest", "bone": "spine_03", "pos": chest},
        {"name": "fx_head", "bone": "head", "pos": J["head_center"]},
        {"name": "fx_overhead", "bone": "head", "pos": J["crown"] + Vector((0, 0, 0.25))},
        {"name": "fx_hand_l", "bone": "hand_l", "pos": J["knuckles_l"]},
        {"name": "fx_hand_r", "bone": "hand_r", "pos": J["knuckles_r"]},
    ]
    if spec.face:
        out += [{"name": "fx_eye_l", "bone": "eye_l", "pos": J["eye_l"]},
                {"name": "fx_eye_r", "bone": "eye_r", "pos": J["eye_r"]}]
    return out


def add_socket(rig: Rig, name: str, bone: str, pos: Sequence[float], rot_deg: Sequence[float] = (0, 0, 0)) -> None:
    rig.sockets = [s for s in rig.sockets if s["name"] != name]
    rig.sockets.append({"name": name, "bone": bone, "pos": Vector(pos), "rot": tuple(rot_deg)})


def socket_manifest(rig: Rig) -> list[dict]:
    """Sockets for manifest.json: bone-relative location in UE bone space (cm) + rest component-space cm."""
    out = []
    for s in rig.sockets:
        b = rig.obj.data.bones[s["bone"]]
        local = b.matrix_local.inverted() @ Vector(s["pos"])
        out.append({
            "name": s["name"], "bone": s["bone"],
            "relLocCm": scene.blender_to_ue_cm(local),
            "relRotDeg": list(s.get("rot", (0, 0, 0))),
            "restLocCm": scene.blender_to_ue_cm(s["pos"]),
        })
    return out


# ── skin weights ─────────────────────────────────────────────────────────────────────────────────────
@dataclass
class Bind:
    """How a mesh part is weighted. ``kind``: 'rigid' (one bone) | 'blend' (proximity among ``bones``) |
    'custom' (``fn(co N×3 bind-pose metres) → N×len(bones)`` weights, e.g. a cloth sheet weighted from its own
    surface parameters so neighbouring bone chains blend smoothly across it)."""

    kind: str
    bones: tuple = ()
    falloff: float = 4.0     # inverse-distance power for 'blend'
    smooth: float = 0.0      # extra blending radius (m) added to distances → softer joints
    fn: object = None        # 'custom' weight function

    @staticmethod
    def custom(bones, fn) -> "Bind":
        return Bind("custom", tuple(bones), fn=fn)

    @staticmethod
    def rigid(bone: str) -> "Bind":
        return Bind("rigid", (bone,))

    @staticmethod
    def blend(*bones: str, falloff: float = 4.0, smooth: float = 0.0) -> "Bind":
        return Bind("blend", tuple(bones), falloff, smooth)


def _segment_dist(p: np.ndarray, a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Distances from N points to B segments → N×B."""
    ab = b - a                                           # B×3
    ap = p[:, None, :] - a[None, :, :]                   # N×B×3
    t = np.clip((ap * ab[None]).sum(-1) / np.maximum((ab * ab).sum(-1), 1e-12)[None], 0.0, 1.0)
    closest = a[None] + t[..., None] * ab[None]
    return np.linalg.norm(p[:, None, :] - closest, axis=-1)


def skin(obj: bpy.types.Object, rig: Rig, part_binds: dict[int, Bind], max_influences: int = 4,
         min_weight: float = 0.02) -> None:
    """Assign vertex groups from per-part :class:`Bind` specs (face attribute ``af_part`` → part id).

    'blend' parts get inverse-distance-to-bone-segment weights (power ``falloff``) restricted to their bone
    list — clean, predictable joints for chunky stylised meshes. Adds the Armature modifier + parent.
    """
    me = obj.data
    nv = len(me.vertices)
    co = np.zeros(nv * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(nv, 3)
    # vertex → part (from faces)
    part_attr = me.attributes.get("af_part")
    if part_attr is None:
        raise RuntimeError("mesh has no af_part attribute (build it with kit.mesh.Builder)")
    nf = len(me.polygons)
    fpart = np.zeros(nf, np.int32)
    part_attr.data.foreach_get("value", fpart)
    lt = np.zeros(nf, np.int32)
    me.polygons.foreach_get("loop_total", lt)
    lv = np.zeros(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", lv)
    vpart = np.full(nv, -1, np.int32)
    vpart[lv] = np.repeat(fpart, lt)
    bones = rig.obj.data.bones
    for g in list(obj.vertex_groups):
        obj.vertex_groups.remove(g)
    groups = {}
    W = {}
    for pid in np.unique(vpart):
        if pid < 0:
            continue
        bind = part_binds.get(int(pid))
        if bind is None:
            raise RuntimeError(f"no Bind for part {pid}")
        idx = np.nonzero(vpart == pid)[0]
        if bind.kind == "rigid":
            W.setdefault(bind.bones[0], []).append((idx, np.ones(len(idx), np.float32)))
            continue
        if bind.kind == "custom":
            w = np.maximum(np.asarray(bind.fn(co[idx]), np.float64), 0.0) + 1e-9
        else:
            a = np.array([tuple(bones[b].head_local) for b in bind.bones], np.float32)
            b = np.array([tuple(bones[b].tail_local) for b in bind.bones], np.float32)
            d = _segment_dist(co[idx], a, b) + bind.smooth + 1e-4
            w = 1.0 / d ** bind.falloff
        # keep the top-k influences
        k = min(max_influences, w.shape[1])
        if w.shape[1] > k:
            cut = np.partition(w, -k, axis=1)[:, -k][:, None]
            w = np.where(w >= cut, w, 0.0)
        w /= w.sum(axis=1, keepdims=True)
        w = np.where(w < min_weight, 0.0, w)
        w /= w.sum(axis=1, keepdims=True)
        for j, bn in enumerate(bind.bones):
            nz = w[:, j] > 0
            if nz.any():
                W.setdefault(bn, []).append((idx[nz], w[nz, j]))
    for bn in sorted(W):
        g = groups.get(bn) or obj.vertex_groups.new(name=bn)
        groups[bn] = g
        for idx, ww in W[bn]:
            for i, wv in zip(idx.tolist(), ww.tolist()):
                g.add([i], float(wv), "REPLACE")
    attach_armature(obj, rig)


def attach_armature(obj: bpy.types.Object, rig: Rig) -> None:
    obj.parent = rig.obj
    obj.matrix_parent_inverse = Matrix.Identity(4)
    mod = obj.modifiers.get("Armature") or obj.modifiers.new("Armature", "ARMATURE")
    mod.object = rig.obj
    mod.use_deform_preserve_volume = False
    # Armature must be first in the stack
    i = obj.modifiers.find("Armature")
    if i > 0:
        obj.modifiers.move(i, 0)


def bind_auto_heat(obj: bpy.types.Object, rig: Rig) -> None:
    """Blender bone-heat automatic weights (organic single-surface meshes), then cleanup()."""
    for b in rig.obj.data.bones:
        b.use_deform = b.name not in NO_WEIGHT_BONES
    scene.select_only([obj, rig.obj], active=rig.obj)
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    for b in rig.obj.data.bones:
        b.use_deform = True
    cleanup_weights(obj)


def cleanup_weights(obj: bpy.types.Object, max_influences: int = 4, min_weight: float = 0.02) -> dict:
    """Limit influences, drop tiny weights, normalise, remove empty groups. Returns stats."""
    me = obj.data
    names = {g.index: g.name for g in obj.vertex_groups}
    max_inf = 0
    for v in me.vertices:
        ws = sorted(((g.weight, g.group) for g in v.groups), reverse=True)
        keep = [(w, gi) for w, gi in ws[:max_influences] if w >= min_weight]
        drop = [gi for w, gi in ws if (w, gi) not in keep]
        tot = sum(w for w, _ in keep) or 1.0
        for gi in drop:
            obj.vertex_groups[names[gi]].remove([v.index])
        for w, gi in keep:
            obj.vertex_groups[names[gi]].add([v.index], w / tot, "REPLACE")
        max_inf = max(max_inf, len(keep))
    used = set()
    for v in me.vertices:
        for g in v.groups:
            used.add(g.group)
    for gi in sorted(set(names) - used, reverse=True):
        obj.vertex_groups.remove(obj.vertex_groups[names[gi]])
    return {"maxInfluences": max_inf, "groups": len(obj.vertex_groups)}


def weight_stats(obj: bpy.types.Object) -> dict:
    me = obj.data
    max_inf, unweighted, bad_sum = 0, 0, 0
    for v in me.vertices:
        n = len(v.groups)
        max_inf = max(max_inf, n)
        if n == 0:
            unweighted += 1
        elif abs(sum(g.weight for g in v.groups) - 1.0) > 0.01:
            bad_sum += 1
    return {"vertices": len(me.vertices), "maxInfluences": max_inf, "unweighted": unweighted,
            "notNormalised": bad_sum, "groups": sorted(g.name for g in obj.vertex_groups)}


def attach_to_bone(obj: bpy.types.Object, rig: Rig, bone: str) -> None:
    """Parent ``obj`` to ``bone`` so its local frame = the bone's frame at the bone **head** (UE socket
    convention). Weapon meshes authored in weapon-bone space (grip at origin, blade +Y) then sit exactly as
    they will on the UE ``weapon_r`` / ``weapon_l`` sockets."""
    obj.parent = rig.obj
    obj.parent_type = "BONE"
    obj.parent_bone = bone
    obj.matrix_parent_inverse = Matrix.Translation((0.0, -rig.obj.data.bones[bone].length, 0.0))
    obj.matrix_basis = Matrix.Identity(4)
