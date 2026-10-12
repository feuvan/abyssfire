"""Grips of the generic ``SM_Weapon_*`` meshes in each hero's hands + the whole-clip in-hand QA (art review: weapons-loot).

The generic family meshes ride clips authored for each class's *own* weapon (warrior sword + strapped shield, mage
staff, rogue daggers), so an identity attach put the staff's orb at the rogue's feet and 8.6 cm under the floor on a
hit-stop frame, a sword's blade through the mage's hood, a bow inside the warrior's breastplate. Each (class, family)
therefore gets **grips**: socket-space transforms the runtime applies on attach — a ``carry`` grip (READY, locomotion,
hurt) and a ``strike`` grip (attacks, casts), plus ``dodge`` / ``death`` (or a per-clip one) where neither passes. The
grip keeps the item's handle in the fist (the grip point may slide along a haft: ``slide``) and turns it about the
grip centre; the runtime blends the attach transform over ``BLEND_MS`` when the clip changes.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/weapon_grips.py --qa
    ... weapon_grips.py --search [Warrior,Mage,Rogue] [--fams staff,bow]     # re-plan grips → weapon_grips.json

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
LIMITS = {"body": 0, "head": 0, "other": 0, "cloth": 20, "lowCm": -0.5}
FACE_CLIPS = ("Idle", "Walk", "Run")
HEAD_GROUPS = ("head", "neck_01", "hood_tip_01", "hood_tip_02", "beard_01", "beard_02")
CLOTH_PREFIX = ("cape_", "tabard_", "skirt_", "sash_", "scarf_", "sleeve_", "hood_tip", "beard_", "plume_", "tome_",
                "tasset_")
HILT_R = 0.13
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


def grip(table: dict, cls: str, fam: str, clip: str) -> np.ndarray:
    """The 4×4 socket-space grip of ``fam`` in ``cls``'s hand for ``clip`` (identity when the table has none)."""
    e = table.get("classes", {}).get(cls, {}).get(fam)
    if not e:
        return np.eye(4)
    name = e["clips"].get(clip, "carry")
    return gmat(e["grips"].get(name) or e["grips"]["carry"])


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
            name = e["clips"].get(clip, carry)
            if name != carry:
                rec["clips"][clip] = ue_transform(gmat(e["grips"][name]))
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
    BONES = ("weapon_r", "weapon_l", "head", "pelvis")

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
            self.clips.append({"name": a["name"], "frames": a["frames"], "start": start,
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

    def run(self, items: dict[str, Item], grips: dict[str, dict | np.ndarray], clips=None, frames=None) -> dict:
        """``grips[key]``: a 4×4 or {clip: 4×4, 'default': 4×4}; ``items[key]``: the Item. → {key: {clip: [rec]}}"""
        p = self.p
        rep = {k: {} for k in grips}
        for c in p.clips:
            if clips and c["name"] not in clips:
                continue
            frs = frames(c) if frames else range(c["frames"])
            for fr in frs:
                fi = c["start"] + fr
                v = p.verts[fi].astype(np.float64)
                body = {}
                for sock in ("weapon_r", "weapon_l"):
                    keep = np.nonzero(~p.m_hand[sock])[0]
                    keep_h = np.nonzero(~p.m_forearm[sock])[0]
                    keep_s = np.nonzero(~p.m_sleeve[sock])[0]
                    body[sock] = (keep, _bvh(v, p.tris[keep]), keep_h, _bvh(v, p.tris[keep_h]), keep_s,
                                  _bvh(v, p.tris[keep_s]))
                t_head = _bvh(v, p.tris[p.m_head])
                atts = {a: (s, _bvh(aco @ p.att_matrix(s, fi)[:3, :3].T + p.att_matrix(s, fi)[:3, 3], atr))
                        for a, (s, aco, atr) in p.atts.items()}
                for key, g in grips.items():
                    G = (g.get(c["name"], g["default"]) if isinstance(g, dict) else g)
                    it = items[key]
                    sock = it.socket
                    Ms = p.bones[sock][fi] @ np.asarray(G, np.float64)
                    w = it.co @ Ms[:3, :3].T + Ms[:3, 3]
                    tm, th = _bvh(w, it.t_main), _bvh(w, it.t_hilt)
                    keep, tb, keep_h, tbh, keep_s, tbs = body[sock]
                    if it.fam == "shield":       # boards strapped to this forearm: its sleeve runs behind them
                        keep, tb = keep_s, tbs
                    rigid = cloth = 0
                    for idx, pairs in ((keep, _ov(tm, tb)), (keep_h, _ov(th, tbh))):
                        for _, j in pairs:
                            if p.m_cloth[idx[j]]:
                                cloth += 1
                            else:
                                rigid += 1
                    head = len(_ov(tm, t_head)) + len(_ov(th, t_head))
                    other = 0
                    for a, (asock, ta) in atts.items():
                        if asock == sock or (it.fam == "bow" and asock in ("weapon_r", "weapon_l")):
                            continue      # replaced on its socket / hidden by a bow (occupiesOffhand)
                        other += len(_ov(tm, ta)) + len(_ov(th, ta))
                    rec = {"fr": fr, "lowCm": round(float(w[it.used, 2].min()) * 100.0, 2), "body": rigid,
                           "cloth": cloth, "head": head, "other": other}
                    if c["name"] in FACE_CLIPS:
                        rec["face"] = _face_hits(p.face_center(fi), w[it.used], FACE_R[p.cls])
                    rep[key].setdefault(c["name"], []).append(rec)
        return rep

    @staticmethod
    def worst(frames: list[dict]) -> dict:
        w = {"body": max(r["body"] for r in frames), "cloth": max(r["cloth"] for r in frames),
             "head": max(r["head"] for r in frames), "other": max(r["other"] for r in frames),
             "lowCm": min(r["lowCm"] for r in frames)}
        if "face" in frames[0]:
            w["faceFrames"] = sum(1 for r in frames if r["face"])
        return w

    @staticmethod
    def passes(w: dict) -> bool:
        return (w["body"] <= LIMITS["body"] and w["head"] <= LIMITS["head"] and w["other"] <= LIMITS["other"]
                and w["cloth"] <= LIMITS["cloth"] and w["lowCm"] >= LIMITS["lowCm"])

    @classmethod
    def verdict(cls_, rep: dict) -> list[str]:
        """Failures of one item's report {clip: [rec]}."""
        out = []
        for clip, frs in rep.items():
            w = cls_.worst(frs)
            if not cls_.passes(w):
                bad = [r["fr"] for r in frs if not cls_.passes(cls_.worst([r]))]
                out.append(f"{clip}: body {w['body']} cloth {w['cloth']} head {w['head']} other {w['other']} "
                           f"low {w['lowCm']:.1f} cm (frames {bad[:10]})")
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
        grips = {}
        for fam in items:
            e = table.get("classes", {}).get(cls, {}).get(fam)
            g = {"default": grip(table, cls, fam, "Idle")}
            for c in p.clips:
                g[c["name"]] = grip(table, cls, fam, c["name"])
            grips[fam] = g
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
        print(f"[grips] QA {cls}: {sum(len(c['frames']) for c in p.clips)} frames x {len(items)} families", flush=True)
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
            rows.append(f"{fam:7s} {cls:7s} {str(shown(cls, fam)):5s} {low:7.1f} {b:4d} {cl:5d} {hd:4d} {ot:5d} "
                        f"{fc:4d}  low@{wc}")
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
    """Orb clearly above the fist, shaft near vertical, orb outboard of the head (warrior / rogue carry)."""
    orb = _world(Gs, Ms, np.array([[0.0, 0.815, 0.0]]))[:, 0]
    rise = orb[:, 2] - Ms[2, 3]
    ax = _axis(Ms, Gs, (0, 1, 0))
    lateral = np.abs(orb[:, 0] - p.bones["head"][fi][0, 3])
    return np.maximum(0, 0.55 - rise) * 4 + np.maximum(0, 0.85 - ax[:, 2]) + np.maximum(0, 0.28 - lateral) * 4


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


def search_spec(cls: str, fam: str, group: str):
    """(readability, max tilt, w_tilt, w_read) per group / family / class."""
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

    def score(self, item: Item, cands: list, clips, read=None) -> dict:
        p, sdf = self.p, self.sdf
        fam, sock = item.fam, item.socket
        co = item.co[item.used]
        r = np.linalg.norm(co, axis=1)
        far = _fps(co[co[:, 1] > 0.085] if fam == "shield" else co[r > HILT_R], 70)
        smp = {"tip": co[np.argmax(co[:, 1])], "length": float(co[:, 1].max() - co[:, 1].min())}
        ground = _fps(co, 120)
        others = [(s, _fps(aco[np.unique(atr)], 40)) for a, (s, aco, atr) in p.atts.items()
                  if s != sock and not (fam == "bow" and s in ("weapon_r", "weapon_l"))]
        far_sub = far[:: max(1, len(far) // 36)]
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
            dc = self.sdf_all.sample(k, Pf).min(1)
            low = _world(Gs, Ms, ground)[..., 2].min(1)
            bad = (df < 0.008) | (dc < -0.012) | (low < 0.0)
            if others:
                Pq = _world(Gs, Ms, far_sub)
                for asock, apts in others:
                    Ao = p.att_matrix(asock, fi)
                    aw = apts @ Ao[:3, :3].T + Ao[:3, 3]
                    dmin = np.full(C, 9.0)
                    for j in range(0, len(aw), 10):
                        dmin = np.minimum(dmin, np.linalg.norm(Pq[:, :, None, :] - aw[None, None, j:j + 10, :],
                                                               axis=-1).min(axis=(1, 2)))
                    bad |= dmin < 0.03
            st["bad"] += bad
            st["minfar"] = np.minimum(st["minfar"], df)
            st["low"] = np.minimum(st["low"], low)
            if cn in FACE_CLIPS:
                fc = p.face_center(fi)
                hits = np.zeros(C)
                for v in V_VIEW.values():
                    d = Pf - fc
                    along = d @ v
                    perp = d - along[..., None] * v
                    hits += ((np.linalg.norm(perp, axis=-1) < FACE_R[p.cls]) & (along < 0.0)).sum(1)
                st["face"] += hits > 0
            if read is not None:
                st["read"] += read(p, fi, Ms, Gs, smp)
            st["n"] += 1
        st["face"] /= max(st["n"], 1)
        st["read"] /= max(st["n"], 1)
        return st

    @staticmethod
    def rank(fam: str, cands: list, st: dict, w_tilt: float, w_read: float) -> list:
        rows = []
        for i, (G, tilt, roll, s) in enumerate(cands):
            per = ROLL_SYM.get(fam, 360.0)
            rd = min(roll % per, per - roll % per)
            score = (st["bad"][i] * 10 + 6.0 * st["face"][i] + w_read * st["read"][i] + w_tilt * tilt + 0.006 * rd
                     + 0.3 * abs(s) - 2.0 * min(st["minfar"][i], 0.012))
            rows.append((score, i))
        rows.sort()
        return [i for _, i in rows]

    def exact(self, item: Item, keyed: dict[str, np.ndarray], clips, frames: bool = False) -> dict:
        rep = self.qa.run({k: item for k in keyed}, keyed, clips=clips)
        if frames:
            return rep
        return {k: {clip: GripQA.worst(frs) for clip, frs in r.items()} for k, r in rep.items()}

    def refine_clip(self, item: Item, cls: str, group: str, clip: str, topn: int = 24):
        """A clip no static grip passes: the candidate with the fewest failing frames (the run's best + ranked
        candidates, the group grip included) → (G, meta, hide window [f0, f1] or None)."""
        read, mt, wt, wr = search_spec(cls, item.fam, group)
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
        read, mt, wt, wr = search_spec(cls, fam, group)
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
        for sock in ("weapon_l", "weapon_r"):
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
    a = ap.parse_args(argv)
    if a.out:
        global GRIPS_JSON
        GRIPS_JSON = Path(a.out)
    scene.reset()
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
    if a.qa:
        posed = {c: Posed(c) for c in CLASSES}
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
