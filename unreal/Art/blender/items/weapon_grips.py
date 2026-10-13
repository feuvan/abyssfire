"""Grips of the generic ``SM_Weapon_*`` meshes in each hero's hands + the whole-clip in-hand QA (art review: weapons-loot).

The generic family meshes ride clips authored for each class's *own* weapon (warrior sword + strapped shield, mage
staff, rogue daggers), so an identity attach put the staff's orb at the rogue's feet and 8.6 cm under the floor on a
hit-stop frame, a sword's blade through the mage's hood, a bow inside the warrior's breastplate. Each (class, family)
therefore gets **grips**: socket-space transforms the runtime applies on attach — a ``carry`` grip (READY, locomotion,
hurt) and a ``strike`` grip (attacks, casts), plus ``dodge`` / ``death`` (or a per-clip one) where neither passes. The
grip keeps the item's handle in the fist (the grip point may slide along a haft: ``slide``) and turns it about the
grip centre; the runtime blends the attach transform over ``BLEND_MS`` when the clip changes. Where no static grip
clears a clip (a 1.27 m staff in the warrior's sword swings, a shield on the mage's robe in Walk), the clip gets a
**keyed grip track** (``tracks``: one grip per frame from an exact-feasibility Viterbi pass, ≤ 12°/frame, constant runs
collapsed; manifest ``clips[<clip>].keys`` sampled by clip time) — a weapon-socket correction curve, the cheap form of a
per-class clip override.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/weapon_grips.py --qa
    ... weapon_grips.py --search [Warrior,Mage,Rogue] [--fams staff,bow]     # re-plan grips → weapon_grips.json
    ... weapon_grips.py --tracks Warrior[,Mage,Rogue] [--fams …]               # keyed tracks for the clips that fail

**QA** (``GripQA``; the checks of ``heroes/qa.py`` on the shipped FBX: every frame of every clip, the body deformed
by the re-imported ``A_Hero_*`` action): lowest point of the item ≥ −0.5 cm; item × body intersecting triangle pairs
(the hilt zone — corners within 13 cm of the grip centre — is tested without this hand and forearm, the rest without
this hand; a shield's handle + straps without the forearm) = 0 on rigid parts, ≤ 20 on cloth chains (cape, tabard,
robe skirt, sash, scarf, bell sleeves, plume — the warrior's own sword is allowed 20 in Run); item × head (head,
neck, hood tip, beard) = 0; item × the class's other default item (warrior shield / sword, rogue off dagger / dagger,
quiver; the mage staff) = 0. ``FACE`` frames (Idle / Walk / Run) also count item points in front of the face disc in
the se and sw views (the focal accent: visor, glowing eyes, mask).

**Search** (``--search``): per (class, family, clip group) thousands of candidate grips (item axis over the sphere ×
roll × slide) are scored on coarse signed-distance grids of the posed rigid body (every 2nd–3rd frame) — clearance,
ground, the other hand's item, the face disc, and readability: the staff carried head-up (warrior, rogue), short
weapons low and outboard at the mage's side, bows with their D toward the W1 camera in se *and* sw, blades flat to
the camera (rogue); attacks / casts prefer the identity attach (the class's own weapon motion). The best candidates
are validated with the exact QA; failing clips get their own search. The result is ``weapon_grips.json``.

**Readability in motion** (art review, second pass: the warrior's Run staff swung up the view ray behind the helm on
half the cycle, his Cast01 / the rogue's Attack03 staff pointed at the camera as an orb blob at the fist, the bow
went edge-on in every warrior strike): staff tracks (warrior, rogue; not READY) score ``read_staff_screen`` — the
fist → orb vector *on screen* in se and sw (≥ 65 % of its length, the orb above the fist: carry .30, casts .40,
attacks level, not behind the head); bow strikes score ``read_bow``; staffs and bows keep out of the face disc in
strikes and casts too, from 2 frames before the contact / release on (``FACE_STRIKE``; the QA gates face frames). The Viterbi turn cost takes the smaller of the turn in the fist and the turn
in the world (a staff held steady while the casting hand rolls under it is calm on screen), staff / bow strikes may
flick (> 12°/frame) for readability (cost 30, else 300) and casts turn ≤ 45°/frame at the release; a **readability
rescue** checks more candidates where the plan's soft cost is > 2 above that frame's best.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import itemkit as IK  # noqa: E402

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

from kit import paths, scene  # noqa: E402

GRIPS_JSON = HERE / "weapon_grips.json"
CLASSES = ("Warrior", "Mage", "Rogue")
FAMS = ("sword", "axe", "mace", "dagger", "bow", "staff", "wand", "shield")
SOCKET = {"sword": "weapon_r", "axe": "weapon_r", "mace": "weapon_r", "dagger": "weapon_r", "staff": "weapon_r",
          "wand": "weapon_r", "bow": "weapon_l", "shield": "weapon_l"}
# FindWeapon returns the hero's own SM_Hero_* weapon for these (the generic mesh is never shown on them today)
OWN = {"Warrior": ("sword", "shield"), "Mage": ("staff",), "Rogue": ("dagger", "bow")}
BLEND_MS = 150
GROUPS = {"carry": ("Idle", "Walk", "Run", "Hurt", "HurtAdd", "Portrait"),
          "strike": ("Attack01", "Attack02", "Attack03", "Cast01", "Cast02", "Cast_Whirlwind", "Cast_Charge",
                     "Cast_Blizzard", "Cast_Meteor", "Cast_Multishot"),
          "dodge": ("Dodge",), "death": ("Death",)}
# face: frames with item points in front of the face disc (se / sw) in READY / locomotion, and in staff / bow strikes
LIMITS = {"body": 0, "head": 0, "other": 0, "cloth": 20, "lowCm": -0.5, "clearCm": 2.0, "face": 0}
# families that must also keep their far parts (beyond the hilt zone) this far off the rigid body — torso, helm,
# legs; not the holding hand / forearm (art review: a bow's limbs ≥ 2 cm off the torso and helm, string included)
CLEAR = {"bow": 0.02}
FACE_CLIPS = ("Idle", "Walk", "Run")
# long / wide items whose strikes and casts must also keep out of the face disc (art review: a staff butt across the
# rogue's mask on the Attack03 hit-stop, a bow string across the mage's beard at the Cast01 release); rolls and falls
# tumble the head through everything and are left out, and the mage's Cast_Meteor raises the casting fist across her
# own face (frames 5–11: every item in that hand crosses the disc, the fist and sleeve already cover it)
FACE_STRIKE = ("staff", "bow")
FACE_STRIKE_SKIP = ("Dodge", "Death", "Portrait", "Cast_Meteor")


# strikes: from 2 frames before the contact / release on (the hit-stop and the follow-through hold still); the
# wind-up before it whips the hand across the face in a few frames (the rogue's Attack01 / 03 raise the fist past
# the mask for every weapon)
FACE_STRIKE_LEAD = 2


def face_clip(fam: str, clip: str, fr: int | None = None, beat: int = -1) -> bool:
    """Is the face disc a hard constraint for ``fam`` in ``clip`` (at frame ``fr`` of a clip whose contact /
    release is ``beat``; track feasibility, search scoring, the QA)?"""
    if clip in FACE_CLIPS:
        return True
    if fam not in FACE_STRIKE or clip in FACE_STRIKE_SKIP:
        return False
    return fr is None or beat < 0 or fr >= beat - FACE_STRIKE_LEAD
# the Portrait clip is the offline UI portrait bake pose (T_UI_Portrait_*): never shown with a generic weapon
QA_SKIP = ("Portrait",)
HEAD_GROUPS = ("head", "neck_01", "hood_tip_01", "hood_tip_02", "beard_01", "beard_02")
CLOTH_PREFIX = ("cape_", "tabard_", "skirt_", "sash_", "scarf_", "sleeve_", "hood_tip", "beard_", "plume_", "tome_",
                "tasset_")
HILT_R = 0.13
HAND_SOCKETS = ("weapon_r", "weapon_l")
SLING_BONE = "spine_03"        # a shield the class's arm cannot carry is slung on the back (``slung`` in the table)
# default attachments the runtime keeps beside a generic weapon (manifest ``attachments`` / ``itemAttachments``)
DEFAULTS = {"Warrior": [("SM_Hero_Warrior_Sword", "weapon_r"), ("SM_Hero_Warrior_Shield", "weapon_l")],
            "Mage": [("SM_Hero_Mage_Staff", "weapon_r")],
            "Rogue": [("SM_Hero_Rogue_Dagger", "weapon_r"), ("SM_Hero_Rogue_Dagger_L", "weapon_l"),
                      ("SM_Hero_Rogue_Quiver", "quiver")]}
# face focal accent (manifest sockets ``visor`` / ``eyes``, head-bone local, Blender axes) and the disc radius
FACE_LOCAL = {"Warrior": (0.0, 0.12846, 0.19457), "Mage": (0.0, 0.17104, 0.17613), "Rogue": (0.0, 0.17384, 0.15874)}
FACE_R = {"Warrior": 0.12, "Mage": 0.11, "Rogue": 0.11}
QUIVER_LOCAL = (-0.03058, 0.13081, -0.19286)            # SK_Hero_Rogue socket ``quiver`` on the pelvis
CAM_F = np.array(Vector((0.4545, -0.4545, -0.7660)).normalized())
FACING = {"se": -90.0, "sw": 180.0}


def shown(cls: str, fam: str) -> bool:
    """Does the runtime show the generic mesh for this combination (``FindWeapon``)?"""
    return fam not in OWN[cls]


# ── grip table ──────────────────────────────────────────────────────────────────────────────────────────
def load_table() -> dict:
    if GRIPS_JSON.exists():
        return json.loads(GRIPS_JSON.read_text())
    return {"schema": 1, "classes": {}}


def save_table(t: dict) -> None:
    t["schema"] = 1
    t["note"] = ("socket-space grips of SM_Weapon_<Family> per hero class (weapon_grips.py): quat = Blender (w, x, "
                 "y, z), loc = metres in the socket (bone head) frame; clips → grip name")
    GRIPS_JSON.write_text(json.dumps(t, indent=1, sort_keys=True) + "\n")


def gmat(g: dict) -> np.ndarray:
    m = np.eye(4)
    m[:3, :3] = np.array(Quaternion(g["quat"]).to_matrix())
    m[:3, 3] = g["loc"]
    return m


def gjson(G: np.ndarray, **meta) -> dict:
    q = Matrix(G[:3, :3].tolist()).to_quaternion()
    d = {"quat": [round(x, 6) for x in q], "loc": [round(float(x), 5) for x in G[:3, 3]]}
    d.update({k: (round(v, 3) if isinstance(v, float) else v) for k, v in meta.items()})
    return d


def socket_for(table: dict, cls: str, fam: str) -> str:
    """The socket ``fam`` attaches to in ``cls``'s hands (the table may move the bow to the main hand)."""
    e = table.get("classes", {}).get(cls, {}).get(fam)
    return (e or {}).get("socket", SOCKET[fam])


def track_matrix(tr: dict, fr: float) -> np.ndarray:
    """A keyed grip track at clip frame ``fr``: keys {fr, quat (w, x, y, z), loc}, slerp / lerp between them,
    held before the first and after the last key (the runtime samples the same by clip time)."""
    keys = tr["keys"]
    if fr <= keys[0]["fr"]:
        return gmat(keys[0])
    for k0, k1 in zip(keys, keys[1:]):
        if fr <= k1["fr"]:
            t = (fr - k0["fr"]) / max(k1["fr"] - k0["fr"], 1e-9)
            q = Quaternion(k0["quat"]).slerp(Quaternion(k1["quat"]), t)
            m = np.eye(4)
            m[:3, :3] = np.array(q.to_matrix())
            m[:3, 3] = (1 - t) * np.asarray(k0["loc"]) + t * np.asarray(k1["loc"])
            return m
    return gmat(keys[-1])


def grip(table: dict, cls: str, fam: str, clip: str, fr: float = 0) -> np.ndarray:
    """The 4×4 socket-space grip of ``fam`` in ``cls``'s hand for ``clip`` at frame ``fr`` (identity when the
    table has none; a keyed track where the clip has one)."""
    e = table.get("classes", {}).get(cls, {}).get(fam)
    if not e:
        return np.eye(4)
    tr = e.get("tracks", {}).get(clip)
    if tr:
        return track_matrix(tr, fr)
    name = e["clips"].get(clip, "carry")
    return gmat(e["grips"].get(name) or e["grips"]["carry"])


def grip_label(table: dict, cls: str, fam: str, clip: str) -> str:
    e = table.get("classes", {}).get(cls, {}).get(fam) or {}
    if clip in e.get("tracks", {}):
        return f"track ({len(e['tracks'][clip]['keys'])} keys)"
    return e.get("clips", {}).get(clip, "identity" if not e else "carry")


def ue_transform(G: np.ndarray) -> dict:
    """Socket-relative transform in UE terms: ``locCm`` (X, −Y, Z), ``quat`` FQuat (X, Y, Z, W) and ``rotDeg``
    FRotator (pitch, yaw, roll) of the mirrored rotation S·R·S (S = diag(1, −1, 1))."""
    S = np.diag([1.0, -1.0, 1.0])
    R = S @ G[:3, :3] @ S
    q = Matrix(G[:3, :3].tolist()).to_quaternion()
    X, Y, Z = R[:, 0], R[:, 1], R[:, 2]
    pitch = math.degrees(math.atan2(X[2], math.hypot(X[0], X[1])))
    yaw = math.degrees(math.atan2(X[1], X[0]))
    sy = np.array([-math.sin(math.radians(yaw)), math.cos(math.radians(yaw)), 0.0])
    roll = math.degrees(math.atan2(Z @ sy, Y @ sy))
    t = G[:3, 3]
    return {"locCm": [round(t[0] * 100.0, 3), round(-t[1] * 100.0, 3), round(t[2] * 100.0, 3)],
            "quat": [round(-q.x, 6), round(q.y, 6), round(-q.z, 6), round(q.w, 6)],
            "rotDeg": [round(pitch, 3), round(yaw, 3), round(roll, 3)]}


def manifest_grips(table: dict, fam: str, clip_names: dict[str, list[str]]) -> dict:
    """Manifest ``grips`` for one family: per class ``default`` (the carry grip) + ``clips`` {clip: transform}
    for every clip whose grip differs, ``shown`` (the runtime shows this generic mesh on that class today)."""
    out = {}
    for cls in CLASSES:
        e = table.get("classes", {}).get(cls, {}).get(fam)
        if not e:
            continue
        carry = e["clips"].get("Idle", "carry")
        rec = {"socket": socket_for(table, cls, fam), "default": ue_transform(gmat(e["grips"][carry])), "clips": {},
               "shown": shown(cls, fam)}
        for clip in clip_names.get(cls, []):
            if clip in QA_SKIP:
                continue
            tr = e.get("tracks", {}).get(clip)
            if tr:
                rec["clips"][clip] = {"keys": [dict(ms=round(k["fr"] * 1000.0 / 60.0, 2), **ue_transform(gmat(k)))
                                               for k in tr["keys"]]}
                continue
            name = e["clips"].get(clip, carry)
            if name != carry:
                rec["clips"][clip] = ue_transform(gmat(e["grips"][name]))
        if e.get("slung"):
            rec["slung"] = True       # carried on the back (socket = a spine bone), not in the hand
        if e.get("hide"):
            rec["hideMs"] = {clip: [round(f0 * 1000.0 / 60.0), round((f1 + 1) * 1000.0 / 60.0)]
                             for clip, (f0, f1) in e["hide"].items()}
        out[cls] = rec
    return out


# ── posed heroes (shipped FBX, every frame) ─────────────────────────────────────────────────────────────
def _vgroup_dominant(body) -> tuple[list[str], np.ndarray]:
    gname = {g.index: g.name for g in body.vertex_groups}
    dom = []
    for v in body.data.vertices:
        best = max(v.groups, key=lambda g: g.weight, default=None)
        dom.append(gname.get(best.group, "?") if best else "?")
    names = sorted(set(dom))
    return names, np.array([names.index(d) for d in dom], np.int32)


def _sm_local(asset: str) -> tuple[np.ndarray, np.ndarray]:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(paths.category_dir("Weapons") / f"{asset}.fbx"))
    new = [o for o in bpy.data.objects if o not in before]
    o = next(x for x in new if x.type == "MESH")
    me = o.data
    me.calc_loop_triangles()
    co = np.array([v.co[:] for v in me.vertices], np.float64)
    mw = np.array(o.matrix_world)
    co = co @ mw[:3, :3].T + mw[:3, 3]
    tris = np.array([t.vertices[:] for t in me.loop_triangles if t.material_index == 0], np.int32)
    for x in new:
        bpy.data.objects.remove(x)
    return co, tris


class Posed:
    """One hero over every frame of every shipped clip: deformed toon vertices, bone frames, dominant-bone face
    groups and the default attachments (item space)."""
    BONES = ("weapon_r", "weapon_l", "head", "pelvis", "spine_03")

    def __init__(self, cls: str, arm=None, body=None):
        self.cls = cls
        own = arm is None
        if own:
            arm, body = IK.import_hero(cls)
        saved = tuple(arm.rotation_euler)
        arm.rotation_euler = (0, 0, 0)
        me = body.data
        me.calc_loop_triangles()
        self.tris = np.array([t.vertices[:] for t in me.loop_triangles if t.material_index == 0], np.int32)
        names, dom = _vgroup_dominant(body)
        fdom = dom[self.tris]
        self.tri_group = np.array([names[np.bincount(r).argmax()] for r in fdom], dtype=object)

        def mask(groups):
            ids = [names.index(g) for g in groups if g in names]
            return np.isin(fdom, ids).sum(1) >= 2
        self.m_head = mask(HEAD_GROUPS)
        self.m_hand = {"weapon_r": mask(("hand_r",)), "weapon_l": mask(("hand_l",))}
        # the forearm + its bell sleeve (the mage's): a hilt / handle / strap zone sits in them by design
        self.m_forearm = {"weapon_r": mask(("lowerarm_r", "hand_r", "sleeve_r_01")),
                          "weapon_l": mask(("lowerarm_l", "hand_l", "sleeve_l_01"))}
        self.m_sleeve = {"weapon_r": mask(("hand_r", "sleeve_r_01")), "weapon_l": mask(("hand_l", "sleeve_l_01"))}
        self.m_cloth = np.array([g.startswith(CLOTH_PREFIX) for g in self.tri_group], bool)
        man = json.loads(paths.manifest_path().read_text())
        self.clips = []
        verts, bones = [], {b: [] for b in self.BONES}
        start = 0
        for a in man["assets"][f"SK_Hero_{cls}"]["anims"]:
            ms = a.get("contactMs") or a.get("releaseMs")
            self.clips.append({"name": a["name"], "frames": a["frames"], "start": start, "loop": bool(a.get("loop")),
                               "beat": int(round(ms * 0.06)) if ms else -1})
            start += a["frames"]
            for fr in range(a["frames"]):
                IK.hero_pose(arm, cls, a["name"], fr)
                dg = bpy.context.evaluated_depsgraph_get()
                ev = body.evaluated_get(dg)
                m = ev.to_mesh()
                co = np.zeros(len(m.vertices) * 3, np.float32)
                m.vertices.foreach_get("co", co)
                ev.to_mesh_clear()
                mw = np.array(body.matrix_world, np.float32)
                verts.append(co.reshape(-1, 3) @ mw[:3, :3].T + mw[:3, 3])
                for b in self.BONES:
                    bones[b].append(np.array(arm.matrix_world @ arm.pose.bones[b].matrix, np.float64))
        self.verts = np.stack(verts)
        self.bones = {b: np.stack(v) for b, v in bones.items()}
        self.atts = {a: (s, *_sm_local(a)) for a, s in DEFAULTS[cls]}
        IK.hero_pose(arm, cls, "Idle", 0)
        arm.rotation_euler = saved
        if own:
            IK.remove_hero(arm)

    def clip(self, name: str) -> dict:
        return next(c for c in self.clips if c["name"] == name)

    def att_matrix(self, sock: str, fi: int) -> np.ndarray:
        if sock == "quiver":
            m = self.bones["pelvis"][fi].copy()
            m[:3, 3] = m[:3, :3] @ np.array(QUIVER_LOCAL) + m[:3, 3]
            return m
        return self.bones[sock][fi]

    def face_center(self, fi: int) -> np.ndarray:
        m = self.bones["head"][fi]
        return m[:3, :3] @ np.array(FACE_LOCAL[self.cls]) + m[:3, 3]


class Item:
    """Toon triangles of an item in its own (socket) space; the hilt / strap zone split."""

    def __init__(self, fam: str, co: np.ndarray, tris: np.ndarray, socket: str | None = None):
        self.fam, self.co, self.tris = fam, co, tris
        self.socket = socket or SOCKET[fam]
        self.used = np.unique(tris)
        pts = co[tris].reshape(-1, 3)
        if fam == "shield":      # handle + straps: the forearm runs through them by design
            z = ((pts[:, 1] < 0.08) & (np.abs(pts[:, 2]) < 0.14) & (np.abs(pts[:, 0]) < 0.17))
        else:                    # the hilt: guard, grip, pommel
            z = np.linalg.norm(pts, axis=1) < HILT_R
        hilt = z.reshape(-1, 3).any(1)
        self.t_hilt, self.t_main = tris[hilt], tris[~hilt]
        far = np.setdiff1d(np.unique(self.t_main), np.unique(self.t_hilt))
        self.far = far[:: max(1, len(far) // 240)]

    def on(self, socket: str) -> "Item":
        return self if socket == self.socket else Item(self.fam, self.co, self.tris, socket)

    @staticmethod
    def from_object(fam: str, obj) -> "Item":
        me = obj.data
        me.calc_loop_triangles()
        co = np.array([v.co[:] for v in me.vertices], np.float64)
        tris = np.array([t.vertices[:] for t in me.loop_triangles if t.material_index == 0], np.int32)
        return Item(fam, co, tris)


def _bvh(co: np.ndarray, tris: np.ndarray):
    if len(tris) == 0:
        return None
    return BVHTree.FromPolygons(co.tolist(), tris.tolist(), all_triangles=True)


def _ov(a, b):
    return a.overlap(b) if (a is not None and b is not None) else []


def _clearance(tree, pts: np.ndarray, cap: float = 0.06) -> float:
    """Smallest distance (cm) from ``pts`` to the triangles of ``tree`` (capped at ``cap`` m)."""
    if tree is None or len(pts) == 0:
        return round(cap * 100.0, 2)
    best = cap
    for q in pts:
        hit = tree.find_nearest(Vector(q), best)
        if hit[0] is not None:
            best = min(best, hit[3])
    return round(best * 100.0, 2)


def _face_hits(fc: np.ndarray, pts: np.ndarray, r: float) -> int:
    n = 0
    for yaw in FACING.values():
        R = np.array(Matrix.Rotation(math.radians(yaw), 3, "Z"))
        d = pts @ R.T - R @ fc
        along = d @ CAM_F
        perp = d - np.outer(along, CAM_F)
        n += int(((np.linalg.norm(perp, axis=1) < r) & (along < 0.0)).sum())
    return n


class GripQA:
    """Exact per-frame QA of items in a hero's hands (see the module docstring)."""

    def __init__(self, posed: Posed):
        self.p = posed

    def frame_ctx(self, fi: int, sockets=("weapon_r", "weapon_l"), clear: bool = False) -> dict:
        """The posed body's trees on global frame ``fi``: per socket (without this hand · without this forearm ·
        without this hand + sleeve), the head, the default attachments, and (``clear``) the rigid body without this
        forearm for the bow clearance."""
        p = self.p
        v = p.verts[fi].astype(np.float64)
        body = {}
        none = np.zeros(len(p.tris), bool)
        for sock in sockets:
            keep = np.nonzero(~p.m_hand.get(sock, none))[0]
            keep_h = np.nonzero(~p.m_forearm.get(sock, none))[0]
            keep_s = np.nonzero(~p.m_sleeve.get(sock, none))[0]
            body[sock] = (keep, _bvh(v, p.tris[keep]), keep_h, _bvh(v, p.tris[keep_h]), keep_s,
                          _bvh(v, p.tris[keep_s]))
        t_clear = {s: _bvh(v, p.tris[~p.m_forearm.get(s, none) & ~p.m_cloth]) for s in sockets} if clear else {}
        atts = {a: (s, _bvh(aco @ p.att_matrix(s, fi)[:3, :3].T + p.att_matrix(s, fi)[:3, 3], atr))
                for a, (s, aco, atr) in p.atts.items()}
        return {"fi": fi, "body": body, "head": _bvh(v, p.tris[p.m_head]), "clear": t_clear, "atts": atts}

    def check(self, ctx: dict, it: Item, G: np.ndarray, fr: int = 0, face: bool = False) -> dict:
        """One item at socket-space grip ``G`` on the frame of ``ctx`` → the QA record."""
        p = self.p
        fi, sock = ctx["fi"], it.socket
        Ms = p.bones[sock][fi] @ np.asarray(G, np.float64)
        w = it.co @ Ms[:3, :3].T + Ms[:3, 3]
        tm, th = _bvh(w, it.t_main), _bvh(w, it.t_hilt)
        keep, tb, keep_h, tbh, keep_s, tbs = ctx["body"][sock]
        if it.fam == "shield":       # boards strapped to this forearm: its sleeve runs behind them
            keep, tb = keep_s, tbs
        rigid = cloth = 0
        # slung on the back (a body socket): the handle and straps lie against the back behind the boards, hidden
        # from every view — only the boards, rim and face are tested against the body
        hilt_pairs = _ov(th, tbh) if sock in HAND_SOCKETS else []
        for idx, pairs in ((keep, _ov(tm, tb)), (keep_h, hilt_pairs)):
            for _, j in pairs:
                if p.m_cloth[idx[j]]:
                    cloth += 1
                else:
                    rigid += 1
        head = len(_ov(tm, ctx["head"])) + len(_ov(th, ctx["head"]))
        other = 0
        for a, (asock, ta) in ctx["atts"].items():
            if asock == sock or (it.fam == "bow" and asock in ("weapon_r", "weapon_l")):
                continue      # replaced on its socket / hidden by a bow (occupiesOffhand)
            other += len(_ov(tm, ta)) + len(_ov(th, ta))
        rec = {"fr": fr, "lowCm": round(float(w[it.used, 2].min()) * 100.0, 2), "body": rigid,
               "cloth": cloth, "head": head, "other": other}
        if it.fam in CLEAR:
            rec["clearCm"] = _clearance(ctx["clear"][sock], w[it.far])
        if face:
            rec["face"] = _face_hits(p.face_center(fi), w[it.used], FACE_R[p.cls])
        return rec

    def run(self, items: dict[str, Item], grips: dict[str, dict | np.ndarray], clips=None, frames=None) -> dict:
        """``grips[key]``: a callable (clip, frame) → 4×4, a 4×4 or {clip: 4×4, 'default': 4×4}; ``items[key]``:
        the Item. → {key: {clip: [rec]}}"""
        p = self.p
        rep = {k: {} for k in grips}
        clear = any(it.fam in CLEAR for it in items.values())
        socks = tuple(dict.fromkeys(HAND_SOCKETS + tuple(it.socket for it in items.values())))
        for c in p.clips:
            if (clips and c["name"] not in clips) or c["name"] in QA_SKIP:
                continue
            frs = frames(c) if frames else range(c["frames"])
            for fr in frs:
                ctx = self.frame_ctx(c["start"] + fr, sockets=socks, clear=clear)
                for key, g in grips.items():
                    G = (g(c["name"], fr) if callable(g) else
                         g.get(c["name"], g["default"]) if isinstance(g, dict) else g)
                    rep[key].setdefault(c["name"], []).append(
                        self.check(ctx, items[key], G, fr, face=face_clip(items[key].fam, c["name"], fr, c["beat"])))
        return rep

    def feasible(self, item: Item, Gs: np.ndarray, clip: str) -> np.ndarray:
        """Exact pass / fail of every candidate grip ``Gs`` (K × 4 × 4) on every frame of ``clip`` → bool (F, K)
        (Idle / Walk / Run also fail on any item point in front of the face disc)."""
        c = self.p.clip(clip)
        out = np.zeros((c["frames"], len(Gs)), bool)
        for fr in range(c["frames"]):
            # READY / locomotion (+ staff / bow strikes from the contact on): nothing over the face
            face = face_clip(item.fam, clip, fr, c["beat"])
            ctx = self.frame_ctx(c["start"] + fr, sockets=(item.socket,), clear=item.fam in CLEAR)
            for k, G in enumerate(Gs):
                rec = self.check(ctx, item, G, fr, face=face)
                out[fr, k] = self.passes(self.worst([rec])) and not rec.get("face", 0)
        return out

    @staticmethod
    def worst(frames: list[dict]) -> dict:
        w = {"body": max(r["body"] for r in frames), "cloth": max(r["cloth"] for r in frames),
             "head": max(r["head"] for r in frames), "other": max(r["other"] for r in frames),
             "lowCm": min(r["lowCm"] for r in frames)}
        if "clearCm" in frames[0]:
            w["clearCm"] = min(r["clearCm"] for r in frames)
        if any("face" in r for r in frames):       # (strikes test the face from the contact on)
            w["faceFrames"] = sum(1 for r in frames if r.get("face"))
        return w

    @staticmethod
    def passes(w: dict) -> bool:
        return (w["body"] <= LIMITS["body"] and w["head"] <= LIMITS["head"] and w["other"] <= LIMITS["other"]
                and w["cloth"] <= LIMITS["cloth"] and w["lowCm"] >= LIMITS["lowCm"]
                and w.get("clearCm", 99.0) >= LIMITS["clearCm"] and w.get("faceFrames", 0) <= LIMITS["face"])

    @classmethod
    def verdict(cls_, rep: dict) -> list[str]:
        """Failures of one item's report {clip: [rec]}."""
        out = []
        for clip, frs in rep.items():
            w = cls_.worst(frs)
            if not cls_.passes(w):
                bad = [r["fr"] for r in frs if not cls_.passes(cls_.worst([r]))]
                out.append(f"{clip}: body {w['body']} cloth {w['cloth']} head {w['head']} other {w['other']} "
                           f"low {w['lowCm']:.1f} cm" + (f" clear {w['clearCm']:.1f} cm" if "clearCm" in w else "")
                           + (f" face {w['faceFrames']} frames" if w.get("faceFrames") else "")
                           + f" (frames {bad[:10]})")
        return out


def hide_window(table: dict, cls: str, fam: str, clip: str) -> tuple[int, int] | None:
    """Frames [f0, f1] of ``clip`` during which the runtime hides the weapon (no static grip clears the body /
    floor there: a long blade in the rogue's roll, a bow under the mage's raised staff arm)."""
    h = table.get("classes", {}).get(cls, {}).get(fam, {}).get("hide", {}).get(clip)
    return (int(h[0]), int(h[1])) if h else None


def qa_all(table: dict, items: dict[str, Item], posed: dict[str, Posed]) -> dict:
    """Every (class, family) with its grips over every frame of every clip → {cls: {fam: {clip: worst}}, failures}
    (frames inside a ``hide`` window are not measured: the weapon is not drawn there)."""
    out = {"classes": {}, "failures": [], "info": []}
    for cls, p in posed.items():
        qa = GripQA(p)
        grips = {fam: (lambda clip, fr, f=fam: grip(table, cls, f, clip, fr)) for fam in items}
        rep = qa.run({f: it.on(socket_for(table, cls, f)) for f, it in items.items()}, grips)
        out["classes"][cls] = {}
        for fam, r in rep.items():
            for clip in list(r):
                hw = hide_window(table, cls, fam, clip)
                if hw:
                    r[clip] = [x for x in r[clip] if not hw[0] <= x["fr"] <= hw[1]] or [r[clip][0]]
            out["classes"][cls][fam] = {clip: GripQA.worst(frs) for clip, frs in r.items()}
            fails = GripQA.verdict(r)
            tag = f"{fam} on {cls}"
            (out["failures"] if shown(cls, fam) else out["info"]).extend(f"{tag} {f}" for f in fails)
            if table.get("classes", {}).get(cls, {}).get(fam) is None:
                out["info"].append(f"{tag}: no grips in the table (identity)")
        print(f"[grips] QA {cls}: {sum(c['frames'] for c in p.clips)} frames x {len(items)} families", flush=True)
    return out


def qa_table(res: dict) -> str:
    rows = [f"{'family':7s} {'class':7s} {'shown':5s} {'low cm':>7s} {'body':>4s} {'cloth':>5s} {'head':>4s} "
            f"{'other':>5s} {'face':>4s}  worst clip"]
    for cls, fams in res["classes"].items():
        for fam, clips in fams.items():
            low = min(w["lowCm"] for w in clips.values())
            b = max(w["body"] for w in clips.values())
            cl = max(w["cloth"] for w in clips.values())
            hd = max(w["head"] for w in clips.values())
            ot = max(w["other"] for w in clips.values())
            fc = sum(w.get("faceFrames", 0) for w in clips.values())
            wc = min(clips, key=lambda k: clips[k]["lowCm"])
            clr = [w["clearCm"] for w in clips.values() if "clearCm" in w]
            rows.append(f"{fam:7s} {cls:7s} {str(shown(cls, fam)):5s} {low:7.1f} {b:4d} {cl:5d} {hd:4d} {ot:5d} "
                        f"{fc:4d}  low@{wc}" + (f"  clear {min(clr):.1f} cm" if clr else ""))
    return "\n".join(rows)


# ── search (coarse SDF + candidate scoring, then the exact QA) ─────────────────────────────────────────
SDF_STEP, SDF_LO, SDF_HI, SDF_DMAX = 0.04, np.array([-1.10, -1.25, -0.20]), np.array([1.10, 1.05, 2.20]), 0.12
SDF_SHAPE = tuple(int(round((SDF_HI[i] - SDF_LO[i]) / SDF_STEP)) + 1 for i in range(3))
V_VIEW = {"se": np.array([0.4545, 0.4545, -0.766]) / 1.0001, "sw": np.array([-0.4545, 0.4545, -0.766]) / 1.0001}
SLIDE = {"axe": (-0.12, -0.08, -0.04, 0.0), "mace": (-0.08, -0.04, 0.0), "staff": (-0.30, -0.15, 0.0, 0.15, 0.30),
         "wand": (0.0, 0.03)}
ROLLS = {"staff": 4, "mace": 2, "wand": 3}
ROLL_SPAN = {"staff": 120.0, "wand": 120.0, "mace": 60.0}
ROLL_SYM = {"sword": 180.0, "dagger": 180.0, "staff": 120.0, "mace": 60.0, "wand": 120.0}
SHORT = ("sword", "axe", "mace", "dagger", "wand")
BOW_SOCKETS: dict[str, tuple[str, ...]] = {}       # --bow-socket: plan the bow on this hand only


class SDF:
    """Signed distance of the posed body on a 4 cm grid, per frame: ``rigid`` (cloth excluded: it may be grazed) or
    the whole body (cloth may not be pierced deeply)."""

    def __init__(self, posed: Posed, frames: list[tuple[str, int, int]], rigid: bool = True):
        axes = [SDF_LO[i] + SDF_STEP * np.arange(SDF_SHAPE[i]) for i in range(3)]
        pts = np.stack(np.meshgrid(*axes, indexing="ij"), -1).reshape(-1, 3)
        tris = posed.tris[~posed.m_cloth] if rigid else posed.tris
        self.index = frames
        self.grids = []
        for _, _, fi in frames:
            v = posed.verts[fi].astype(np.float64)
            t = BVHTree.FromPolygons(v.tolist(), tris.tolist(), all_triangles=True)
            out = np.full(len(pts), SDF_DMAX, np.float32)
            lo, hi = v.min(0) - SDF_DMAX, v.max(0) + SDF_DMAX
            for i in np.nonzero(((pts >= lo) & (pts <= hi)).all(1))[0]:
                p = Vector(pts[i])
                loc, nrm, _, d = t.find_nearest(p, SDF_DMAX)
                if loc is not None:
                    out[i] = -d if (p - loc).dot(nrm) < 0 else d
            self.grids.append(out.reshape(SDF_SHAPE).astype(np.float16))
        self.shape = np.array(SDF_SHAPE)

    def sample(self, k: int, pts: np.ndarray) -> np.ndarray:
        sh = pts.shape[:-1]
        p = (pts.reshape(-1, 3) - SDF_LO) / SDF_STEP
        i0 = np.floor(p).astype(np.int64)
        f = p - i0
        out = np.full(len(p), SDF_DMAX, np.float32)
        ok = ((i0 >= 0) & (i0 < self.shape - 1)).all(1)
        i0, f = i0[ok], f[ok]
        g = self.grids[k]
        acc = np.zeros(len(i0), np.float32)
        for dx in (0, 1):
            wx = f[:, 0] if dx else 1 - f[:, 0]
            for dy in (0, 1):
                wy = f[:, 1] if dy else 1 - f[:, 1]
                for dz in (0, 1):
                    wz = f[:, 2] if dz else 1 - f[:, 2]
                    acc += (wx * wy * wz) * g[i0[:, 0] + dx, i0[:, 1] + dy, i0[:, 2] + dz].astype(np.float32)
        out[ok] = acc
        return out.reshape(sh)


def _qmul(a: np.ndarray, B: np.ndarray) -> np.ndarray:
    """Quaternion product a ⊗ B[k] (w, x, y, z) for every row of ``B``."""
    w0, x0, y0, z0 = a
    w1, x1, y1, z1 = B[:, 0], B[:, 1], B[:, 2], B[:, 3]
    return np.stack([w0 * w1 - x0 * x1 - y0 * y1 - z0 * z1, w0 * x1 + x0 * w1 + y0 * z1 - z0 * y1,
                     w0 * y1 - x0 * z1 + y0 * w1 + z0 * x1, w0 * z1 + x0 * y1 - y0 * x1 + z0 * w1], 1)


def _fib(n: int) -> np.ndarray:
    i = np.arange(n) + 0.5
    phi = np.arccos(1 - 2 * i / n)
    th = math.pi * (1 + 5 ** 0.5) * i
    return np.stack([np.cos(th) * np.sin(phi), np.cos(phi), np.sin(th) * np.sin(phi)], 1)


def _align_y(a: np.ndarray) -> np.ndarray:
    a = a / np.linalg.norm(a)
    c = float(a[1])
    if c < -0.999999:
        return np.diag([1.0, -1.0, -1.0])
    v = np.cross([0.0, 1.0, 0.0], a)
    k = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + k + k @ k / (1 + c)


def _rot_y(psi: float) -> np.ndarray:
    c, s = math.cos(psi), math.sin(psi)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def candidates(fam: str, max_tilt: float = 180.0, ndirs: int = 240) -> list:
    """(G, tilt°, roll°, slide m): item axis over the sphere (tilt from the fist axis) × roll × slide."""
    dirs = list(_fib(ndirs)) + [np.array([0.0, 1.0, 0.0]), np.array([0.0, -1.0, 0.0])]
    n = ROLLS.get(fam, 12)
    rolls = [math.radians(ROLL_SPAN.get(fam, 360.0) * k / n) for k in range(n)]
    out = []
    for a in dirs:
        tilt = math.degrees(math.acos(max(-1.0, min(1.0, float(a[1])))))
        if tilt > max_tilt + 1e-6:
            continue
        A = _align_y(np.asarray(a, float))
        for r in rolls:
            R = A @ _rot_y(r)
            for s in SLIDE.get(fam, (0.0,)):
                G = np.eye(4)
                G[:3, :3] = R
                G[:3, 3] = R @ np.array([0.0, -s, 0.0])
                out.append((G, tilt, math.degrees(r), s))
    return out


def _fps(pts: np.ndarray, k: int) -> np.ndarray:
    if len(pts) <= k:
        return pts
    sel = [int(np.argmax(np.linalg.norm(pts - pts.mean(0), axis=1)))]
    d = np.linalg.norm(pts - pts[sel[0]], axis=1)
    for _ in range(k - 1):
        i = int(np.argmax(d))
        sel.append(i)
        d = np.minimum(d, np.linalg.norm(pts - pts[i], axis=1))
    return pts[sel]


def _world(Gs: np.ndarray, Ms: np.ndarray, pts: np.ndarray) -> np.ndarray:
    A = np.einsum("ij,cjk->cik", Ms[:3, :3], Gs[:, :3, :3])
    t = np.einsum("ij,cj->ci", Ms[:3, :3], Gs[:, :3, 3]) + Ms[:3, 3]
    return np.einsum("ckj,cij->cki", np.broadcast_to(pts, (len(Gs),) + pts.shape), A) + t[:, None, :]


def _axis(Ms, Gs, local) -> np.ndarray:
    return np.einsum("ij,cjk,k->ci", Ms[:3, :3], Gs[:, :3, :3], np.asarray(local, float))


def read_staff_up(p, fi, Ms, Gs, smp):
    """Orb clearly above the fist, shaft near vertical, orb outboard of the head (warrior / rogue carry), and not
    foreshortened toward the W1 camera (a staff pointing up the view ray shrank to a stub with the orb at the fist)."""
    orb = _world(Gs, Ms, np.array([[0.0, 0.815, 0.0]]))[:, 0]
    rise = orb[:, 2] - Ms[2, 3]
    ax = _axis(Ms, Gs, (0, 1, 0))
    lateral = np.abs(orb[:, 0] - p.bones["head"][fi][0, 3])
    proj = np.minimum(np.sqrt(np.maximum(0.0, 1.0 - (ax @ V_VIEW["se"]) ** 2)),
                      np.sqrt(np.maximum(0.0, 1.0 - (ax @ V_VIEW["sw"]) ** 2)))
    return (np.maximum(0, 0.55 - rise) * 4 + np.maximum(0, 0.85 - ax[:, 2]) + np.maximum(0, 0.28 - lateral) * 4
            + np.maximum(0, 0.6 - proj) * 6)


def read_low_outboard(p, fi, Ms, Gs, smp):
    """Mage carry: the far end below the fist and outboard (the hero's right, −X), not trailing behind."""
    tip = _world(Gs, Ms, smp["tip"][None])[:, 0]
    drop = Ms[2, 3] - tip[:, 2]
    out = Ms[0, 3] - tip[:, 0]
    ax = _axis(Ms, Gs, (0, 1, 0))
    return (np.maximum(0, 0.6 * smp["length"] - drop) * 3 + np.maximum(0, 0.04 - out) * 4
            + np.maximum(0, ax[:, 1]) * 0.5)


def read_bow(p, fi, Ms, Gs, smp):
    """The bow's plane (normal = item X) toward the W1 camera in se *and* sw, limbs near vertical."""
    n = _axis(Ms, Gs, (1, 0, 0))
    f = np.minimum(np.abs(n @ V_VIEW["se"]), np.abs(n @ V_VIEW["sw"]))
    limb = _axis(Ms, Gs, (0, 0, 1))
    return np.maximum(0, 0.55 - f) * 3 + np.maximum(0, 0.6 - np.abs(limb[:, 2])) * 2


def read_flat(p, fi, Ms, Gs, smp):
    """Blades / bits: the flat toward the camera in se, the item not foreshortened along the view (rogue)."""
    n = _axis(Ms, Gs, (1, 0, 0))
    ax = _axis(Ms, Gs, (0, 1, 0))
    return np.maximum(0, 0.5 - np.abs(n @ V_VIEW["se"])) * 2 + np.maximum(0, np.abs(ax @ V_VIEW["se"]) - 0.6) * 3


STAFF_ORB_Y = 0.815                # the orb's centre on the generic staff (item +Y, metres from the grip)
# the orb's lift above the fist *on screen* as a fraction of the shaft (carry, casts, attacks)
SCREEN_UP = {"carry": 0.30, "cast": 0.40, "attack": -0.10}
CAST_CLIPS = ("Cast01", "Cast02", "Cast_Blizzard", "Cast_Meteor", "Cast_Multishot")


def _view_up(v: np.ndarray) -> np.ndarray:
    up = np.array([0.0, 0.0, 1.0]) - v[2] * v
    return up / np.linalg.norm(up)


def read_staff_screen(kind: str):
    """Staff readability measured on screen (art review: in the warrior's Run the staff swung up the view ray and hid
    behind the helm, in Cast01 / the rogue's Attack03 it pointed at the camera and shrank to an orb blob at the fist,
    with the orb down at the feet it read as a mop): in the se *and* sw W1 views the fist → orb vector keeps ≥ 65 % of
    its length, the orb sits above the fist on screen (``SCREEN_UP``: carry ≥ .30, casts ≥ .40 of the shaft, attacks
    level or above) and is not hidden behind the head."""
    lim = SCREEN_UP[kind]

    def fn(p, fi, Ms, Gs, smp):
        orb = _world(Gs, Ms, np.array([[0.0, STAFF_ORB_Y, 0.0]]))[:, 0]
        d = orb - Ms[:3, 3]
        rel = orb - p.bones["head"][fi][:3, 3]
        pen = np.zeros(len(Gs))
        for v in V_VIEW.values():
            sl = np.linalg.norm(d - np.outer(d @ v, v), axis=1) / STAFF_ORB_Y
            su = (d @ _view_up(v)) / STAFF_ORB_Y
            along = rel @ v                                    # > 0: farther from the camera than the head
            perp = np.linalg.norm(rel - np.outer(along, v), axis=1)
            pen += np.maximum(0, 0.65 - sl) * 4 + np.maximum(0, lim - su) * 3 + ((along > 0) & (perp < 0.22)) * 1.5
        return pen / len(V_VIEW)
    return fn


def search_spec(cls: str, fam: str, group: str, clip: str | None = None):
    """(readability, max tilt, w_tilt, w_read) per group / family / class (``clip``: the one clip a track or a
    per-clip search plans for)."""
    if fam == "staff" and cls != "Mage" and clip not in (None, "Idle", "Portrait") and group in ("carry", "strike"):
        kind = "carry" if group == "carry" else ("cast" if clip in CAST_CLIPS else "attack")
        return read_staff_screen(kind), 180.0, (0.002 if group == "carry" else 0.01), 1.0
    if fam == "bow" and group == "strike":       # the D toward the camera in attacks and casts too
        return read_bow, 180.0, 0.01, 1.0
    if group in ("strike", "dodge", "death"):
        return None, (35.0 if group == "strike" else 180.0), (0.05 if group == "strike" else 0.01), 0.0
    if fam == "bow":
        return read_bow, 180.0, 0.002, 1.0
    if fam == "shield":
        return None, 30.0, 0.05, 0.0
    if fam == "staff":
        return read_staff_up, 180.0, 0.002, 1.0
    if cls == "Mage":
        return read_low_outboard, 180.0, 0.002, 1.0
    if cls == "Rogue":
        return read_flat, 70.0, 0.01, 1.0
    return None, 40.0, 0.03, 0.0


def prefers_identity(cls: str, fam: str, group: str) -> bool:
    return group in ("strike", "dodge", "death") or fam == "shield" or (cls in ("Warrior", "Rogue") and fam in SHORT)


class Searcher:
    def __init__(self, posed: Posed):
        self.p = posed
        idx = []
        for c in posed.clips:
            if c["name"] == "Portrait":
                continue
            step = 3 if c["name"] in GROUPS["carry"] else 2
            frs = set(range(0, c["frames"], step)) | {c["frames"] - 1} | ({c["beat"]} if c["beat"] >= 0 else set())
            idx += [(c["name"], fr, c["start"] + fr) for fr in sorted(frs)]
        t0 = time.time()
        self.sdf = SDF(posed, idx)
        self.sdf_all = SDF(posed, idx, rigid=False)
        print(f"[grips] {posed.cls}: SDF on {len(idx)} frames in {time.time() - t0:.0f}s", flush=True)
        self.qa = GripQA(posed)
        self._clip_sdf = {}

    def score(self, item: Item, cands: list, clips, read=None, margin: float = 0.0, per_frame=None,
              sdfs=None) -> dict:
        """Aggregate candidate terms over the SDF frames of ``clips``; ``per_frame`` (a list) also collects
        (k, frame, bad, clearance, face, read) per frame for the track search; ``margin`` widens every clearance;
        ``sdfs`` = (rigid, whole) grids to use instead of the searcher's (the track search: every frame)."""
        p, sdf = self.p, self.sdf
        sdf_all = self.sdf_all
        if sdfs is not None:
            sdf, sdf_all = sdfs
        fam, sock = item.fam, item.socket
        co = item.co[item.used]
        r = np.linalg.norm(co, axis=1)
        far = _fps(co[co[:, 1] > 0.085] if fam == "shield" else co[r > HILT_R], 70)
        smp = {"tip": co[np.argmax(co[:, 1])], "length": float(co[:, 1].max() - co[:, 1].min())}
        ground = _fps(co, 120)
        others = [(s, _fps(aco[np.unique(atr)], 40)) for a, (s, aco, atr) in p.atts.items()
                  if s != sock and not (fam == "bow" and s in ("weapon_r", "weapon_l"))]
        far_sub = far[:: max(1, len(far) // 36)]
        # the hilt zone beyond the fist (8–13 cm from the grip centre): the exact QA tests it against the body
        # without this hand / forearm, the SDF keeps them — so only "not inside" is asked of it
        near = (_fps(co[(r > 0.08) & (r <= HILT_R)], 16) if fam != "shield" else np.zeros((0, 3)))
        Gs = np.stack([c[0] for c in cands])
        C = len(Gs)
        st = {"n": 0, "minfar": np.full(C, 9.0), "low": np.full(C, 9.0), "face": np.zeros(C), "read": np.zeros(C),
              "bad": np.zeros(C)}
        for k, (cn, fr, fi) in enumerate(sdf.index):
            if cn not in clips:
                continue
            Ms = p.bones[sock][fi]
            Pf = _world(Gs, Ms, far)
            df = sdf.sample(k, Pf).min(1)
            dc = sdf_all.sample(k, Pf).min(1)
            low = _world(Gs, Ms, ground)[..., 2].min(1)
            bad = (df < CLEAR.get(fam, 0.0) + 0.008 + margin) | (dc < -0.012 + margin) | (low < margin * 0.5)
            if len(near):
                bad |= sdf.sample(k, _world(Gs, Ms, near)).min(1) < margin * 0.5
            if others:
                Pq = _world(Gs, Ms, far_sub)
                for asock, apts in others:
                    Ao = p.att_matrix(asock, fi)
                    aw = apts @ Ao[:3, :3].T + Ao[:3, 3]
                    dmin = np.full(C, 9.0)
                    for j in range(0, len(aw), 10):
                        dmin = np.minimum(dmin, np.linalg.norm(Pq[:, :, None, :] - aw[None, None, j:j + 10, :],
                                                               axis=-1).min(axis=(1, 2)))
                    bad |= dmin < 0.03 + margin
            st["bad"] += bad
            st["minfar"] = np.minimum(st["minfar"], df)
            st["low"] = np.minimum(st["low"], low)
            hits = np.zeros(C)
            if face_clip(fam, cn, fr, p.clip(cn)["beat"]):
                fc = p.face_center(fi)
                for v in V_VIEW.values():
                    d = Pf - fc
                    along = d @ v
                    perp = d - along[..., None] * v
                    hits += ((np.linalg.norm(perp, axis=-1) < FACE_R[p.cls]) & (along < 0.0)).sum(1)
                st["face"] += hits > 0
            rd = read(p, fi, Ms, Gs, smp) if read is not None else np.zeros(C)
            st["read"] += rd
            if per_frame is not None:
                per_frame.append((k, fr, bad.copy(), df.copy(), hits > 0, rd))
            st["n"] += 1
        st["face"] /= max(st["n"], 1)
        st["read"] /= max(st["n"], 1)
        return st

    @staticmethod
    def rank(fam: str, cands: list, st: dict, w_tilt: float, w_read: float) -> list:
        rows = []
        pref = CLEAR.get(fam, 0.0) + 0.012
        for i, (G, tilt, roll, s) in enumerate(cands):
            per = ROLL_SYM.get(fam, 360.0)
            rd = min(roll % per, per - roll % per)
            score = (st["bad"][i] * 10 + 6.0 * st["face"][i] + w_read * st["read"][i] + w_tilt * tilt + 0.006 * rd
                     + 0.3 * abs(s) - 2.0 * min(st["minfar"][i], pref))
            rows.append((score, i))
        rows.sort()
        return [i for _, i in rows]

    def exact(self, item: Item, keyed: dict[str, np.ndarray], clips, frames: bool = False) -> dict:
        rep = self.qa.run({k: item for k in keyed}, keyed, clips=clips)
        if frames:
            return rep
        return {k: {clip: GripQA.worst(frs) for clip, frs in r.items()} for k, r in rep.items()}

    def clip_sdfs(self, clip: str):
        """Rigid + whole-body SDF grids on *every* frame of ``clip`` (cached): track keys sit on every frame."""
        if clip not in self._clip_sdf:
            c = self.p.clip(clip)
            idx = [(clip, fr, c["start"] + fr) for fr in range(c["frames"])]
            self._clip_sdf = {clip: (SDF(self.p, idx), SDF(self.p, idx, rigid=False))}
        return self._clip_sdf[clip]

    def track(self, item: Item, cls: str, group: str, clip: str, loop: bool, margin: float = 0.004,
              pool_all: int = 140, pool_frame: int = 10, rounds: int = 3, seeds=None,
              rescue: int = 2) -> tuple[dict, list[int]]:
        """A keyed grip track for a clip no static grip clears. On every frame of the clip the candidate grips are
        scored on that frame's SDF (clearance incl. the hilt zone, ground, face disc, readability, tilt); a pool of
        the best overall and per frame is checked with the **exact** QA on every frame (``GripQA.feasible``); a
        Viterbi pass then picks one feasible grip per frame minimising the soft costs + the turn between frames
        (≤ 12°/frame, slides cost extra); looping clips close on their first frame. Frames with no feasible pool
        member widen the pool there (``rounds``). ``seeds`` (4×4 grips: the READY carry, the clip's previous track
        keys) join the candidates. **Readability rescue** (``rescue`` rounds): where the planned grip's soft cost is
        > 2 above that frame's best candidate (the pool held no feasible readable grip there — a staff swung up the
        view ray behind the helm, an orb at the feet), the frame's next-best candidates are checked exactly and the
        path re-planned. Constant runs collapse to their end keys.
        → (track {keys, loop}, frames that still fail)."""
        read, mt, wt, wr = search_spec(cls, item.fam, group, clip)
        wr = wr if read is not None else 0.0
        # a roll / fall tumbles the whole body ~25°/frame: a faster weapon turn hides in it
        # (casts: the hand flings out at the release — a staff / bow held up through it may turn as fast)
        snap = 45.0 if group in ("dodge", "death") or (clip in CAST_CLIPS and item.fam in FACE_STRIKE) else 24.0
        # a turn faster than 12°/frame (in the fist *and* in the world) costs ``flick``: only an intersection was
        # worth one; a staff / bow strike may also flick to stay readable (~3 frames of the orb at the fist)
        flick = 30.0 if (group == "strike" and item.fam in FACE_STRIKE) else 300.0
        cands = candidates(item.fam, 180.0)
        for G in (seeds or []):
            G = np.asarray(G, float)
            s_ = -float((G[:3, :3].T @ G[:3, 3])[1])
            cands.append((G, math.degrees(math.acos(max(-1.0, min(1.0, float(G[1, 1]))))), 0.0, s_))
        Gs = np.stack([c[0] for c in cands])
        tilt = np.array([c[1] for c in cands])
        per = ROLL_SYM.get(item.fam, 360.0)
        rdev = np.array([min(c[2] % per, per - c[2] % per) for c in cands])
        slide = np.array([c[3] for c in cands])
        quats = np.array([list(Matrix(G[:3, :3].tolist()).to_quaternion()) for G in Gs])
        pref = CLEAR.get(item.fam, 0.0) + 0.012
        pf = []
        self.score(item, cands, (clip,), read, margin=margin, per_frame=pf, sdfs=self.clip_sdfs(clip))
        pf.sort(key=lambda x: x[1])
        # the exact feasibility decides clearance, so the coarse SDF flags only nudge and readability leads
        cost = np.stack([3.0 * x[2] + 6.0 * x[4] + 3.0 * wr * x[5] + wt * tilt + 0.006 * rdev + 0.3 * np.abs(slide)
                         - 2.0 * np.minimum(x[3], pref) for x in pf])
        F = cost.shape[0]
        order = np.argsort(cost, 1)
        pool = list(dict.fromkeys(np.argsort(cost.sum(0))[:pool_all].tolist()
                                  + [int(i) for row in order[:, :pool_frame] for i in row]))
        pool = [int(i) for i in pool]
        feas = self.qa.feasible(item, Gs[pool], clip)
        for r in range(1, rounds):
            dead = np.nonzero(~feas.any(1))[0]
            if not len(dead):
                break
            add = [int(i) for f in dead for i in order[f, :pool_frame * (2 + 2 * r)] if int(i) not in pool]
            add = list(dict.fromkeys(add))[:240]
            print(f"[grips] track {cls} {item.fam} {clip}: no feasible grip on frames {dead.tolist()[:12]}, "
                  f"+{len(add)} candidates", flush=True)
            if not add:
                break
            pool += add
            feas = np.concatenate([feas, self.qa.feasible(item, Gs[add], clip)], 1)
        # the hand's world rotation per frame: a turn between two frames costs the smaller of the turn in the
        # fist and the turn in the world — a staff held steady while the casting hand rolls fast under it is calm
        # on screen (art review: the grip-relative limit kept the warrior's Cast01 staff pointing at the camera)
        c0 = self.p.clip(clip)["start"]
        mq = np.array([list(Matrix(self.p.bones[item.socket][c0 + f][:3, :3].tolist()).normalized()
                            .to_quaternion()) for f in range(F)])

        def plan(pool, feas):
            P = np.array(pool)
            cp = cost[:, P] + 5000.0 * ~feas
            qP = quats[P]
            ang_rel = np.degrees(2.0 * np.arccos(np.clip(np.abs(qP @ qP.T), 0.0, 1.0)))
            ds = np.abs(slide[P][:, None] - slide[P][None, :])
            K = len(P)

            def trans(f0, f1):
                a, b = _qmul(mq[f0], qP), _qmul(mq[f1], qP)
                ang = np.minimum(ang_rel, np.degrees(2.0 * np.arccos(np.clip(np.abs(a @ b.T), 0.0, 1.0))))
                T = 0.03 * ang + 0.004 * ang * ang + 25.0 * ds
                T[ang > 12.0] += flick          # a quick flick beats an intersection; never a snap
                T[ang > snap] = 1e6
                return T

            Ts = [None] + [trans(f - 1, f) for f in range(1, F)]

            def run(a0):
                acc, back = a0.copy(), np.zeros((F, K), np.int64)
                for f in range(1, F):
                    tot = acc[:, None] + Ts[f]
                    back[f] = np.argmin(tot, 0)
                    acc = tot[back[f], np.arange(K)] + cp[f]
                return acc, back
            if not loop:
                acc, back = run(cp[0])
                e = int(np.argmin(acc))
            else:      # the last frame must turn back to the first within one frame
                best = None
                T0 = trans(F - 1, 0)
                for s0 in np.argsort(cp[0])[:10]:
                    a0 = np.full(K, 1e9)
                    a0[s0] = cp[0][s0]
                    acc, bk = run(a0)
                    fin = acc + T0[:, s0]
                    e = int(np.argmin(fin))
                    if best is None or fin[e] < best[0]:
                        best = (fin[e], e, bk)
                _, e, back = best
            path = [e]
            for f in range(F - 1, 0, -1):
                path.append(int(back[f][path[-1]]))
            return path[::-1]

        path = plan(pool, feas)
        for r in range(rounds):
            bad = [f for f, ci in enumerate(path) if not feas[f, ci]]
            if not bad:
                break
            # widen the pool round the failing stretch: every candidate within 20° of the path's grips there
            near = set()
            for f in bad:
                for g in range(max(0, f - 3), min(F, f + 4)):
                    qd = np.abs(quats @ quats[pool[path[g]]])
                    near |= set(np.nonzero(qd > math.cos(math.radians(snap * 0.5)))[0].tolist())
            have = set(pool)
            add = [i for i in sorted(near, key=lambda i: cost[bad, i].sum()) if i not in have][:260]
            print(f"[grips] track {cls} {item.fam} {clip}: failing {bad[:12]} -> +{len(add)} nearby candidates",
                  flush=True)
            if not add:
                break
            pool = pool + add
            feas = np.concatenate([feas, self.qa.feasible(item, Gs[add], clip)], 1)
            path = plan(pool, feas)
        best_soft = cost.min(1)
        rd_all = np.stack([x[5] for x in pf])
        sdf_bad = np.stack([x[2] for x in pf])
        for r in range(rescue):
            soft = cost[np.arange(F), [pool[ci] for ci in path]]
            weak = [f for f in range(F) if soft[f] - best_soft[f] > 2.0]
            if not weak:
                break
            have = set(pool)
            # per weak frame (and its neighbours, for the turns in and out) the readable, SDF-clear candidates in
            # soft-cost order, taken round-robin so every frame gets its share
            near_w = sorted({g for f in weak for g in range(max(0, f - 2), min(F, f + 3))})
            queues = []
            for f in near_w:
                ok_ = (rd_all[f] <= rd_all[f].min() + 0.3) & ~sdf_bad[f]
                q = [int(i) for i in order[f] if ok_[i] and int(i) not in have]
                queues.append(q)
            add, k = [], 0
            while len(add) < 700 and any(k < len(q) for q in queues) and k < 60:
                for q in queues:
                    if k < len(q) and q[k] not in have:
                        have.add(q[k])
                        add.append(q[k])
                k += 1
            print(f"[grips] track {cls} {item.fam} {clip}: weak readability on {len(weak)} frames "
                  f"{weak[:12]} -> +{len(add)} candidates", flush=True)
            if not add:
                break
            pool = pool + add
            feas = np.concatenate([feas, self.qa.feasible(item, Gs[add], clip)], 1)
            path = plan(pool, feas)
        self.last = {"pool": pool, "feas": feas, "cost": cost, "path": path, "cands": cands, "rd": rd_all}
        keys = []
        for f, ci in enumerate(path):
            if 0 < f < F - 1 and path[f - 1] == ci == path[f + 1]:
                continue
            keys.append(dict(gjson(Gs[pool[ci]]), fr=int(f)))
        K = len(pool)
        tr = {"keys": keys, "loop": bool(loop)}
        bad = [f for f, ci in enumerate(path) if not feas[f, ci]]
        print(f"[grips] track {cls} {item.fam} {clip}: pool {K}, {len(keys)} keys, failing {bad[:12]}", flush=True)
        return tr, bad

    def refine_clip(self, item: Item, cls: str, group: str, clip: str, topn: int = 24):
        """A clip no static grip passes: the candidate with the fewest failing frames (the run's best + ranked
        candidates, the group grip included) → (G, meta, hide window [f0, f1] or None)."""
        read, mt, wt, wr = search_spec(cls, item.fam, group, clip)
        cands = candidates(item.fam, 180.0)
        st = self.score(item, cands, (clip,), read)
        order = self.rank(item.fam, cands, st, wt, 0.0)[:topn]
        keyed = {f"c{i}": cands[i][0] for i in order}
        keyed["identity"] = np.eye(4)
        rep = self.exact(item, keyed, (clip,), frames=True)
        best = None
        for k, r in rep.items():
            frs = r[clip]
            bad = [x["fr"] for x in frs if not GripQA.passes(GripQA.worst([x]))]
            span = (bad[-1] - bad[0] + 1) if bad else 0
            key = (span, len(bad))
            if best is None or key < best[0]:
                best = (key, k, bad)
        _, k, bad = best
        G = keyed[k]
        meta = {"kind": "identity" if k == "identity" else "search", "refined": True}
        if k != "identity":
            i = int(k[1:])
            meta.update(tilt=cands[i][1], roll=cands[i][2], slide=cands[i][3])
        return G, meta, ([bad[0], bad[-1]] if bad else None)

    def pick(self, item: Item, cls: str, group: str, clips, topn: int = 10) -> tuple[np.ndarray, dict, bool]:
        fam = item.fam
        if prefers_identity(cls, fam, group):
            ids = {"identity": np.eye(4)}
            if fam in ("sword", "dagger"):
                R = np.eye(4)
                R[:3, :3] = _rot_y(math.pi)
                ids["identity180"] = R
            rep = self.exact(item, ids, clips)
            for k, G in ids.items():
                if all(GripQA.passes(w) for w in rep[k].values()):
                    return G, {"kind": k}, True
        read, mt, wt, wr = search_spec(cls, fam, group, clips[0] if len(clips) == 1 else None)
        best = None
        for max_tilt in ([mt, 180.0] if mt < 180.0 else [mt]):
            cands = candidates(fam, max_tilt)
            st = self.score(item, cands, clips, read)
            order = self.rank(fam, cands, st, wt, wr)[:topn]
            rep = self.exact(item, {f"c{i}": cands[i][0] for i in order}, clips)
            for i in order:
                G, tilt, roll, s = cands[i]
                meta = {"kind": "search", "tilt": tilt, "roll": roll, "slide": s, "maxTilt": max_tilt,
                        "faceFrac": float(st["face"][i]), "read": float(st["read"][i])}
                if all(GripQA.passes(w) for w in rep[f"c{i}"].values()):
                    return G, meta, True
                if best is None:
                    best = (G, dict(meta, failing=True))
        return best[0], best[1], False

    def plan(self, item: Item) -> dict:
        """Grips for every clip group; a bow is planned on both hands and the hand with fewer failing / searched
        clips wins (the mage's left hand rests on the robe skirt: from there any bow pierces it)."""
        if item.fam != "bow":
            return self._plan(item)
        best = None
        for sock in (BOW_SOCKETS.get(self.p.cls) or ("weapon_l", "weapon_r")):
            r = self._plan(item.on(sock))
            r["socket"] = sock
            cost = (sum(1 for v in r["clips"].values() if v.endswith("!fail")), len(r["grips"]))
            print(f"[grips] {self.p.cls} bow on {sock}: failing/grips {cost}", flush=True)
            if best is None or cost < best[0]:
                best = (cost, r)
        return best[1]

    def _plan(self, item: Item) -> dict:
        p, cls = self.p, self.p.cls
        present = {c["name"] for c in p.clips}
        res = {"grips": {}, "clips": {}}
        chosen = {}
        for group in ("carry", "strike"):
            clips = tuple(c for c in GROUPS[group] if c in present and c != "Portrait")
            G, meta, ok = self.pick(item, cls, group, clips)
            chosen[group] = G
            res["grips"][group] = gjson(G, **meta)
            rep = self.exact(item, {"g": G}, clips)["g"]
            for clip, w in rep.items():
                if GripQA.passes(w):
                    res["clips"][clip] = group
                    continue
                G2, meta2, ok2 = self.pick(item, cls, group, (clip,))
                name = f"{group}_{clip}"
                res["grips"][name] = gjson(G2, **meta2)
                res["clips"][clip] = name if ok2 else name + "!fail"
        for group in ("dodge", "death"):
            clip = GROUPS[group][0]
            rep = self.exact(item, {"carry": chosen["carry"], "strike": chosen["strike"]}, (clip,))
            if GripQA.passes(rep["carry"][clip]):
                res["clips"][clip] = "carry"
            elif GripQA.passes(rep["strike"][clip]):
                res["clips"][clip] = "strike"
            else:
                G, meta, ok = self.pick(item, cls, group, (clip,))
                res["grips"][group] = gjson(G, **meta)
                res["clips"][clip] = group if ok else group + "!fail"
        res["clips"]["Portrait"] = res["clips"].get("Idle", "carry")
        return res


def sling_plan(posed: Posed, item: Item) -> tuple[dict, dict]:
    """A shield slung on the back (``SLING_BONE``) for a class whose off-hand arm is authored against its body (the
    mage's left hand swings across the robe in Walk / Run: no grip on the forearm clears it). Candidates: face out
    (+Y = the hero's back), a diagonal sling (about the face normal), the top leaning off the back, the boards a few cm
    off the back, centred on the shoulder blades or lower; the exact QA over every frame of every clip picks the
    closest-fitting one that passes the most clips, then each failing clip gets the nearest candidate that clears it
    (a hood flopping back in Hurt / Death / the big casts). → (table entry, QA worst per clip)."""
    it = item.on(SLING_BONE)
    fi = posed.clip("Idle")["start"]
    B = posed.bones[SLING_BONE][fi]
    v = posed.verts[fi]
    band = v[(np.abs(v[:, 0]) < 0.10) & (np.abs(v[:, 2] - (B[2, 3] + 0.02)) < 0.10)]
    back_y = float(band[:, 1].max())
    boards_y = 0.092          # back of the boards on the centre line (weapons.SH_FACE_Y − 2.6 cm board thickness)

    def cand_set(diags, leans, offs, dzs):
        out = {}
        for diag in diags:
            for lean in leans:
                for off in offs:
                    for dz in dzs:
                        R = (np.array(Matrix.Rotation(math.radians(-lean), 3, "X"))
                             @ np.array(Matrix.Rotation(math.radians(diag), 3, "Y")))
                        M = np.eye(4)
                        M[:3, :3] = R
                        M[:3, 3] = (0.0, back_y + off - boards_y, B[2, 3] + 0.02 + dz)
                        out[f"d{diag:+.0f}_l{lean:.0f}_o{off * 100:.1f}_z{dz * 100:+.0f}"] = (
                            np.linalg.inv(B) @ M, {"diag": diag, "lean": lean, "offCm": off * 100, "dzCm": dz * 100})
        return out

    def rank(cands, clips=None):
        qa = GripQA(posed)
        rep_ = qa.run({k: it for k in cands}, {k: c[0] for k, c in cands.items()}, clips=clips)
        rows = []
        for k, r in rep_.items():
            worst = {clip: GripQA.worst(frs) for clip, frs in r.items()}
            nbad = sum(1 for frs in r.values() for x in frs if not GripQA.passes(GripQA.worst([x])))
            m = cands[k][1]
            rows.append((nbad, m["offCm"], abs(m["dzCm"]), abs(m["diag"] - 15.0), m["lean"], k, worst))
        rows.sort(key=lambda x: x[:5])
        return rows

    base = cand_set((-25.0, 0.0, 25.0), (0.0, 10.0, 20.0), (0.01, 0.025, 0.04, 0.06), (-0.06, 0.0, 0.06))
    rows = rank(base)
    nbad, _, _, _, _, k, worst = rows[0]
    print(f"[grips] sling {posed.cls} {item.fam}: base {k} failing frames {nbad}", flush=True)
    G, meta = base[k]
    entry = {"socket": SLING_BONE, "slung": True, "grips": {"carry": gjson(G, kind="sling", **meta)},
             "clips": {c["name"]: "carry" for c in posed.clips}}
    fails = [clip for clip, w in worst.items() if not GripQA.passes(w)]
    if fails:
        wide = cand_set((-25.0, 0.0, 25.0), (0.0, 15.0, 30.0), (0.04, 0.07, 0.10),
                        (-0.26, -0.20, -0.14, -0.08, 0.0))
        for clip in fails:
            r2 = rank(wide, clips=(clip,))
            # the nearest passing candidate to the base pose (blends in over gripBlendMs)
            ok = [x for x in r2 if x[0] == 0]
            pick = min(ok, key=lambda x: (abs(wide[x[5]][1]["dzCm"] - meta["dzCm"]) / 4
                                          + abs(wide[x[5]][1]["diag"] - meta["diag"]) / 25
                                          + abs(wide[x[5]][1]["lean"] - meta["lean"]) / 10
                                          + abs(wide[x[5]][1]["offCm"] - meta["offCm"]) / 2)) if ok else r2[0]
            name = f"sling_{clip}"
            entry["grips"][name] = gjson(wide[pick[5]][0], kind="sling", **wide[pick[5]][1])
            entry["clips"][clip] = name
            worst[clip] = pick[6][clip]
            print(f"[grips] sling {posed.cls} {item.fam} {clip}: {pick[5]} failing frames {pick[0]}", flush=True)
    return entry, worst


def build_items(fams=FAMS) -> dict[str, Item]:
    """The current generator meshes (``weapons.py``), item space."""
    import weapons as W
    pal, base, _ = W.palettes()
    out = {}
    for fam in fams:
        obj, _ = W.build(fam, pal, base)
        out[fam] = Item.from_object(fam, obj)
        bpy.data.objects.remove(obj)
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--search", nargs="?", const=",".join(CLASSES), default=None)
    ap.add_argument("--fams", default=",".join(FAMS))
    ap.add_argument("--qa", action="store_true")
    ap.add_argument("--refine", default="", help="classes: re-plan the clips the search could not clear (hide "
                                                  "windows where nothing passes)")
    ap.add_argument("--out", default="", help="write the table here instead (parallel searches; merge after)")
    ap.add_argument("--classes", default=",".join(CLASSES), help="--qa: only these classes")
    ap.add_argument("--tracks", default="", help="classes: give every clip that fails the exact QA at its static grip "
                                                  "a keyed grip track (Viterbi over the candidate grips)")
    ap.add_argument("--force", default="", help="--tracks: also re-track these clips when their grip passes "
                                                 "(readability: e.g. Idle,Walk,Run,Hurt,HurtAdd)")
    ap.add_argument("--sling", default="", help="classes: sling the --fams shield on the back (spine_03)")
    ap.add_argument("--bow-socket", default="", help="--search: plan the bow on this socket only (weapon_l|weapon_r)")
    a = ap.parse_args(argv)
    if a.out:
        global GRIPS_JSON
        GRIPS_JSON = Path(a.out)
    scene.reset()
    if a.bow_socket and a.search:
        for cls in a.search.split(","):
            BOW_SOCKETS[cls] = (a.bow_socket,)
    fams = [f for f in a.fams.split(",") if f]
    items = build_items(fams)
    if a.search:
        for cls in [c for c in a.search.split(",") if c]:
            posed = Posed(cls)
            s = Searcher(posed)
            for fam in fams:
                t0 = time.time()
                res = s.plan(items[fam])
                failing = [c for c, v in res["clips"].items() if v.endswith("!fail")]
                res["clips"] = {c: v.replace("!fail", "") for c, v in res["clips"].items()}
                if failing:
                    res["searchFailed"] = failing
                t = load_table()
                t.setdefault("classes", {}).setdefault(cls, {})[fam] = res
                save_table(t)
                print(f"[grips] {cls} {fam} {time.time() - t0:.0f}s {res['clips']}", flush=True)
    if a.refine:
        for cls in [c for c in a.refine.split(",") if c]:
            t = load_table()
            fams_c = {f: e for f, e in t.get("classes", {}).get(cls, {}).items() if f in fams and e.get("searchFailed")}
            if not fams_c:
                continue
            s = Searcher(Posed(cls))
            for fam, e in fams_c.items():
                for clip in list(e["searchFailed"]):
                    group = next(g for g, cl in GROUPS.items() if clip in cl)
                    G, meta, hide = s.refine_clip(items[fam].on(e.get("socket", SOCKET[fam])), cls, group, clip)
                    name = f"refined_{clip}"
                    e["grips"][name] = gjson(G, **meta)
                    e["clips"][clip] = name
                    if hide:
                        e.setdefault("hide", {})[clip] = hide
                    else:
                        e.get("hide", {}).pop(clip, None)
                    e["searchFailed"].remove(clip)
                    print(f"[grips] refine {cls} {fam} {clip}: {meta} hide {hide}", flush=True)
                if not e["searchFailed"]:
                    e.pop("searchFailed")
                t = load_table()
                t["classes"][cls][fam] = e
                save_table(t)
    if a.sling:
        for cls in [c for c in a.sling.split(",") if c]:
            posed = Posed(cls)
            for fam in fams:
                entry, worst = sling_plan(posed, items[fam])
                t = load_table()
                t.setdefault("classes", {}).setdefault(cls, {})[fam] = entry
                save_table(t)
                for clip, w in worst.items():
                    print(f"   {clip}: {w} {'ok' if GripQA.passes(w) else 'FAIL'}", flush=True)
    if a.tracks:
        for cls in [c for c in a.tracks.split(",") if c]:
            t = load_table()
            todo = [f for f in fams if t.get("classes", {}).get(cls, {}).get(f) and shown(cls, f)]
            if not todo:
                continue
            posed = Posed(cls)
            res = qa_all(t, {f: items[f] for f in todo}, {cls: posed})
            s = None
            for fam in todo:
                e = t["classes"][cls][fam]
                fails = [c for c, w in res["classes"][cls][fam].items()
                         if not GripQA.passes(w) or c in a.force.split(",")]
                print(f"[grips] tracks {cls} {fam}: static grips fail {fails}", flush=True)
                if not fails:
                    continue
                s = s or Searcher(posed)
                for clip in fails:
                    group = next(g for g, cl in GROUPS.items() if clip in cl)
                    t0 = time.time()
                    # seeds: the READY carry (the blend in from the guard) and the clip's current grip / track keys
                    seeds = [grip(t, cls, fam, "Idle", 0)]
                    old = e.get("tracks", {}).get(clip)
                    seeds += [gmat(k) for k in old["keys"]] if old else [grip(t, cls, fam, clip, 0)]
                    tr, bad = s.track(items[fam].on(socket_for(t, cls, fam)), cls, group, clip,
                                      posed.clip(clip)["loop"], seeds=seeds)
                    e.setdefault("tracks", {})[clip] = tr
                    e.get("hide", {}).pop(clip, None)
                    if bad:
                        e.setdefault("trackFailed", {})[clip] = bad
                    else:
                        e.get("trackFailed", {}).pop(clip, None)
                    print(f"[grips] tracks {cls} {fam} {clip}: {len(tr['keys'])} keys, failing {bad[:12]} "
                          f"({time.time() - t0:.0f}s)", flush=True)
                    e.pop("searchFailed", None)       # the exact QA (tracks / trackFailed) is the record now
                    for k in ("hide", "trackFailed"):
                        if k in e and not e[k]:
                            e.pop(k)
                    t2 = load_table()                 # saved per clip (a long run keeps what it finished)
                    t2["classes"][cls][fam] = e
                    save_table(t2)
    if a.qa:
        posed = {c: Posed(c) for c in a.classes.split(",") if c}
        res = qa_all(load_table(), items, posed)
        print(qa_table(res))
        for f in res["failures"]:
            print("  FAIL", f)
        for f in res["info"]:
            print("  info", f)
        return 1 if res["failures"] else 0
    return 0


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(argv))
