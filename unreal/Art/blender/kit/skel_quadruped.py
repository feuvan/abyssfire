"""Quadruped armature builder — ``SKEL_Quadruped`` (Chapter 2, package 4R-1; Docs/dark-remake/chapter2.md §4.4,
§17.1: "spine 4, neck 2, head, jaw, tail 3, four legs of 3 bones + toe, ear pair; sockets fx_overhead, fx_elite,
bite, collar").

The second rig family of the kit beside the humanoid (``kit.rig.build_humanoid``) and the slime's own bones. First
member: the moon-hound (``monsters/moon_hound.py``); the same builder makes the Pack-Leader (the hound's skeleton, a
heavier mesh), Volgan (the hound rig at actor scale 2.2 with his own proportions: ``QuadrupedSpec.scaled`` /
``.but`` — same bone names and hierarchy, so the UE skeleton asset is shared and his clips carry his own bone
translations) and the wolf cub pet.

Conventions (the kit's): the animal faces **−Y**, +X is its left, +Z up, ``root`` at the ground point under the
middle of the trunk. Every bone points along its limb / toward the head (tail bones toward the tip), rolled so local
**X is the lateral hinge** (the animal's left): legs, spine, neck and jaw all bend about +X.

```
root
├─ pelvis ─ spine_01 ─ spine_02 ─ spine_03 ─ spine_04 ─ neck_01 ─ neck_02 ─ head ─ jaw
│    │                                         │                              ├─ ear_l, ear_r
│    │                                         └─ foreleg_01_l/r ─ foreleg_02 ─ foreleg_03 ─ foretoe   (humerus,
│    │                                                                         radius, pastern, paw)
│    ├─ hindleg_01_l/r ─ hindleg_02 ─ hindleg_03 ─ hindtoe      (femur, tibia, metatarsus, paw)
│    └─ tail_01 ─ tail_02 ─ tail_03
└─ ik_foot_root ─ ik_foot_fl / fr / bl / br      (no weights; copy the four paws: foretoe_* / hindtoe_*)
```

36 bones. ``pelvis`` runs from the tail's root to the loin, ``spine_04`` is the chest between the shoulder blades
(the forelegs hang from it), so a roach, a stretch, a crouch on the forehand and a rear are spine curves, not a
rigid trunk. The legs are **three bones and a paw**: the hound's zigzag (shoulder → elbow back → wrist → paw; hip →
stifle forward → hock back → paw) is in the rest pose, so a two-bone IK of the upper pair plus a set pastern /
metatarsus angle poses a leg (``monsters/qclips.py``).

Sockets (``default_sockets``; manifest ``sockets``, bone-relative UE cm):

| Socket | Bone | Where | For |
|---|---|---|---|
| ``fx_feet`` | root | the ground point | ground decals, landings |
| ``fx_chest`` | spine_04 | inside the chest | hit sparks, status loops |
| ``fx_head`` | head | the skull's centre | hit sparks, the portrait's framing |
| ``fx_overhead`` | root | over the withers, a hand above the back line | nameplate, floating text, stun stars, the W1 crown (a low animal: it follows the actor, not the head, so it does not swing with a bite) |
| ``fx_elite`` | root | above ``fx_overhead`` (``elite_sigil.py --solve-elite`` re-solves it per asset) | SM_FX_EliteSigil |
| ``fx_eye_l`` / ``fx_eye_r`` | head | the eyes | eye cards (portraits), the non-emissive eye-shine needs none in game |
| ``mouth`` | head | between the teeth, at the back of the muzzle | a light in the maw (Volgan's moon-tarnish ``boss.volgan.maw`` loop), breath |
| ``bite`` | jaw | the lower fangs | the bite's contact spark, drool, a held thing |
| ``collar`` | neck_01 | under the throat, where the collar's ring hangs | the chain (a UE cable from ``collar`` to the oak's ring-bolt: Volgan), a leash |
| ``tail_tip`` | tail_03 | the tail's tip | trails |
"""
from __future__ import annotations

from dataclasses import dataclass, field, replace

import bpy
from mathutils import Vector

from . import scene
from .rig import LEFT, Rig

SKELETON = "SKEL_Quadruped"
SIDES = (("l", 1.0), ("r", -1.0))
LEGS = (("fl", "fore", "l"), ("fr", "fore", "r"), ("bl", "hind", "l"), ("br", "hind", "r"))
SPINE = ("pelvis", "spine_01", "spine_02", "spine_03", "spine_04")
NECK = ("neck_01", "neck_02")
TAIL = ("tail_01", "tail_02", "tail_03")
FORELEG = ("foreleg_01", "foreleg_02", "foreleg_03", "foretoe")
HINDLEG = ("hindleg_01", "hindleg_02", "hindleg_03", "hindtoe")
IK_BONES = ("ik_foot_root", "ik_foot_fl", "ik_foot_fr", "ik_foot_bl", "ik_foot_br")
NO_WEIGHT_BONES = set(IK_BONES) | {"root"}
CORE_BONES = (("root",) + SPINE + NECK + ("head", "jaw", "ear_l", "ear_r") + TAIL
              + tuple(f"{b}_{s}" for s in ("l", "r") for b in FORELEG + HINDLEG))


def _hound_points() -> dict:
    """Side-view landmarks (y, z) in metres of the moon-hound: 1.30 m nose to rump, withers 0.75 m, the head
    carried low, the tail straight. Paired landmarks take their half width from ``QuadrupedSpec.half``."""
    return {
        # trunk: tail root → loin → back → withers → neck base (the bones' joints)
        "pelvis": (0.47, 0.670), "spine_01": (0.30, 0.700), "spine_02": (0.13, 0.712), "spine_03": (-0.04, 0.716),
        "spine_04": (-0.20, 0.716), "neck_01": (-0.34, 0.700), "neck_02": (-0.465, 0.682),
        "head": (-0.585, 0.655), "nose": (-0.83, 0.600),
        "jaw": (-0.635, 0.612), "chin": (-0.805, 0.566),
        "ear": (-0.605, 0.700), "ear_tip": (-0.518, 0.790),
        "eye": (-0.672, 0.672),
        "tail_01": (0.47, 0.655), "tail_02": (0.60, 0.615), "tail_03": (0.73, 0.582), "tail_tip": (0.87, 0.556),
        # foreleg: shoulder joint → elbow (back) → wrist → paw ball → toe tip
        "shoulder": (-0.275, 0.560), "elbow": (-0.205, 0.400), "wrist": (-0.235, 0.170), "fore_ball": (-0.262, 0.032),
        "fore_tip": (-0.335, 0.022),
        # hind leg: hip → stifle (forward) → hock (back) → paw ball → toe tip
        "hip": (0.385, 0.620), "stifle": (0.275, 0.405), "hock": (0.500, 0.205), "hind_ball": (0.440, 0.032),
        "hind_tip": (0.365, 0.022),
    }


@dataclass
class QuadrupedSpec:
    """Rest-pose landmarks of a quadruped (metres). ``points``: side view (y, z); ``half``: half widths (x) of the
    paired landmarks. ``withers`` is the top of the back line over the shoulders (the review framing height and the
    manifest's ``heightCm``)."""

    name: str = "SK_Quadruped"
    skeleton: str = SKELETON
    points: dict = field(default_factory=_hound_points)
    half: dict = field(default_factory=lambda: {"shoulder": 0.085, "elbow": 0.092, "wrist": 0.090, "fore_ball": 0.090,
                                                "hip": 0.084, "stifle": 0.098, "hock": 0.095, "hind_ball": 0.098,
                                                "ear": 0.047, "ear_tip": 0.090, "eye": 0.046})
    withers: float = 0.75
    ik_bones: bool = True

    def crown_height(self) -> float:
        """The tallest rest landmark of the body line (withers; the ears of a low head sit a little above it)."""
        return max(self.withers, self.points["ear_tip"][1])

    def scaled(self, k: float) -> "QuadrupedSpec":
        return replace(self, points={n: (y * k, z * k) for n, (y, z) in self.points.items()},
                       half={n: v * k for n, v in self.half.items()}, withers=self.withers * k)

    def but(self, points: dict | None = None, half: dict | None = None, **kw) -> "QuadrupedSpec":
        """A variant: some landmarks moved (a heavier forehand, longer legs), names unchanged."""
        return replace(self, points={**self.points, **(points or {})}, half={**self.half, **(half or {})}, **kw)

    def p(self, name: str, side: float = 0.0) -> Vector:
        y, z = self.points[name]
        key = {"fore_tip": "fore_ball", "hind_tip": "hind_ball"}.get(name, name)
        return Vector((side * self.half.get(key, 0.0), y, z))


def build_quadruped(spec: QuadrupedSpec, collection=None) -> Rig:
    """Create the armature object (named ``spec.name``) in its rest stand and return a :class:`kit.rig.Rig`
    (``.joints``: every landmark, the paired ones as ``<name>_l`` / ``<name>_r``; ``.sockets``)."""
    scene.ensure_object_mode()
    arm = bpy.data.armatures.new(spec.skeleton)
    obj = bpy.data.objects.new(spec.name, arm)
    scene.link(obj, collection)
    arm.display_type = "STICK"
    obj.show_in_front = True
    scene.select_only([obj])
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.edit_bones
    J: dict[str, Vector] = {}
    for n in spec.points:
        key = {"fore_tip": "fore_ball", "hind_tip": "hind_ball"}.get(n, n)
        if key in spec.half:
            for s, sx in SIDES:
                J[f"{n}_{s}"] = spec.p(n, sx)
        else:
            J[n] = spec.p(n)

    def bone(name, head, tail, parent=None, deform=True, connect=False, x_axis=LEFT):
        b = eb.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        if parent:
            b.parent = eb[parent]
            b.use_connect = connect
        y = (b.tail - b.head).normalized()
        x = Vector(x_axis)
        x = (x - y * x.dot(y)).normalized()
        b.align_roll(x.cross(y))                             # Z = X × Y: local X is the lateral hinge
        b.use_deform = deform
        return b

    k = spec.withers / 0.75
    bone("root", (0, 0, 0), (0, 0, 0.08 * k))
    trunk = list(SPINE) + list(NECK) + ["head"]
    ends = [J[n] for n in trunk] + [J["nose"]]
    for i, n in enumerate(trunk):
        bone(n, ends[i], ends[i + 1], "root" if i == 0 else trunk[i - 1], connect=i > 0)
    bone("jaw", J["jaw"], J["chin"], "head")
    for s, _ in SIDES:
        bone(f"ear_{s}", J[f"ear_{s}"], J[f"ear_tip_{s}"], "head")
    tail = [J[n] for n in TAIL] + [J["tail_tip"]]
    for i, n in enumerate(TAIL):
        bone(n, tail[i], tail[i + 1], "pelvis" if i == 0 else TAIL[i - 1], connect=i > 0)
    for s, _ in SIDES:
        pts = [J[f"{n}_{s}"] for n in ("shoulder", "elbow", "wrist", "fore_ball", "fore_tip")]
        for i, n in enumerate(FORELEG):
            bone(f"{n}_{s}", pts[i], pts[i + 1], "spine_04" if i == 0 else f"{FORELEG[i - 1]}_{s}", connect=i > 0)
        pts = [J[f"{n}_{s}"] for n in ("hip", "stifle", "hock", "hind_ball", "hind_tip")]
        for i, n in enumerate(HINDLEG):
            bone(f"{n}_{s}", pts[i], pts[i + 1], "pelvis" if i == 0 else f"{HINDLEG[i - 1]}_{s}", connect=i > 0)
    if spec.ik_bones:
        bone("ik_foot_root", (0, 0, 0), (0, 0, 0.08 * k), "root", deform=True)
        for leg, kind, s in LEGS:
            src = eb[f"{kind}toe_{s}"]
            b = bone(f"ik_foot_{leg}", src.head, src.tail, "ik_foot_root")
            b.roll = src.roll
    bpy.ops.object.mode_set(mode="OBJECT")
    if spec.ik_bones:
        for leg, kind, s in LEGS:
            c = obj.pose.bones[f"ik_foot_{leg}"].constraints.new("COPY_TRANSFORMS")
            c.target = obj
            c.subtarget = f"{kind}toe_{s}"
    for pb in obj.pose.bones:
        pb.rotation_mode = "QUATERNION"
    obj["af_skeleton"] = spec.skeleton
    rig = Rig(obj, spec, J, [])
    rig.sockets = default_sockets(rig)
    return rig


def default_sockets(rig: Rig) -> list[dict]:
    """The family's sockets as (bone, armature-space rest location); see the module table."""
    J, spec = rig.joints, rig.spec
    k = spec.withers / 0.75
    over = Vector((0.0, J["spine_04"].y + 0.02 * k, spec.withers + 0.30 * k))
    mouth = J["jaw"].lerp(J["nose"], 0.42) + Vector((0, 0, -0.012 * k))
    out = [
        {"name": "fx_feet", "bone": "root", "pos": Vector((0, 0, 0))},
        {"name": "fx_chest", "bone": "spine_04", "pos": Vector((0.0, J["spine_04"].y - 0.06 * k, 0.60 * k))},
        {"name": "fx_head", "bone": "head", "pos": J["head"].lerp(J["nose"], 0.30) + Vector((0, 0, 0.012 * k))},
        {"name": "fx_overhead", "bone": "root", "pos": over},
        {"name": "fx_elite", "bone": "root", "pos": over + Vector((0, 0, 0.22 * k))},
        {"name": "fx_eye_l", "bone": "head", "pos": J["eye_l"]},
        {"name": "fx_eye_r", "bone": "head", "pos": J["eye_r"]},
        {"name": "mouth", "bone": "head", "pos": mouth},
        {"name": "bite", "bone": "jaw", "pos": J["jaw"].lerp(J["chin"], 0.86) + Vector((0, 0, 0.012 * k))},
        {"name": "collar", "bone": "neck_01", "pos": J["neck_01"].lerp(J["neck_02"], 0.45) + Vector((0, 0, -0.105 * k))},
        {"name": "tail_tip", "bone": "tail_03", "pos": J["tail_tip"]},
    ]
    return out
