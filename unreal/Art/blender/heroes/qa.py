"""Per-frame geometric QA for hero clips (the review's blind spots, made automatic).

For every frame of every baked clip, on the **deformed** meshes exactly as the review renders show them:

* **ground** — lowest point of the body / main-hand item / off-hand item (cm; negative = under the floor);
* **sword × shield**, **sword × body**, **shield × body** — intersecting triangle pairs (BVH overlap of the
  toon surfaces), with the baseline that the item's grip legitimately sits inside its fist (the grip zone of the
  item is only excluded against that hand's parts);
* **cape × armour** — intersecting triangle pairs between the cape surface and the plates under / around it
  (back plate, gorget, pauldrons, arms, gauntlets, helm), outside the pinned yoke (the top edge tucked under the
  pauldrons by design);
* **head** — where the visor points relative to the strike line (yaw) and how far it tips down (pitch), sampled
  on the contact / release frames.

``Checker.run(clips, actions)`` → ``{clip: {"frames": [...], "worst": {...}}}``; ``Checker.verdict(report,
limits)`` compares the worst values with the limits (the ship step fails when one is exceeded).
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Callable, Sequence

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

from kit import anim, scene


@dataclass
class Groups:
    """Body part name prefixes (``Builder`` part names, ``_l``/``_r`` suffixes kept) per QA group."""
    cape: tuple = ("cape", "cape_crease", "cape_hem")
    cape_skin: tuple = ()                    # swatches of the cape's visible outer surface (others: lining, walls)
    armour: tuple = ()                       # plates the cape must never cross
    hand_r: tuple = ()                       # parts of the main-hand fist (grip baseline)
    hand_l: tuple = ()                       # parts of the off-hand fist (shield handle baseline)
    yoke_rows: float = 0.0                   # cape faces whose rest z is above this are the pinned yoke
    cape_interior: Callable[[Vector], bool] = lambda co: True   # rest face centre → inside the side tucks?
    item_r_grip: Callable[[Vector], bool] = lambda co: False    # item-local: is this face in the grip zone?
    item_l_grip: Callable[[Vector], bool] = lambda co: False


def _face_parts(obj: bpy.types.Object, part_names: Sequence[str]) -> np.ndarray:
    """Per-face part names (toon + hull) of a Builder mesh."""
    me = obj.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    names = np.array(list(part_names) + ["?"], dtype=object)
    return names[np.clip(fp, -1, len(part_names) - 1)]


def _match(names: np.ndarray, parts: Sequence[str]) -> np.ndarray:
    """Faces whose part is one of ``parts`` (a name matches itself and its ``_l`` / ``_r`` variants)."""
    want = set()
    for p in parts:
        want |= {p, p + "_l", p + "_r"}
    return np.array([n in want for n in names], bool)


class Checker:
    def __init__(self, rig, body: bpy.types.Object, item_r: bpy.types.Object, item_l: bpy.types.Object,
                 part_names: Sequence[str], groups: Groups):
        self.rig, self.body, self.item_r, self.item_l = rig, body, item_r, item_l
        self.g = groups
        me = body.data
        mi = np.zeros(len(me.polygons), np.int32)
        me.polygons.foreach_get("material_index", mi)
        toon = mi == 0
        names = _face_parts(body, part_names)
        self.names = names
        rest_c = np.zeros(len(me.polygons) * 3, np.float32)
        me.polygons.foreach_get("center", rest_c)
        rest_c = rest_c.reshape(-1, 3)
        cape = toon & _match(names, groups.cape)
        if groups.cape_skin:                 # only the outer skin: plates inside the cloth's thickness are hidden
            sw = np.zeros(len(me.polygons), np.int32)
            me.attributes["af_swatch"].data.foreach_get("value", sw)
            cape &= np.isin(sw, groups.cape_skin) | ~_match(names, groups.cape[:1])
        yoke = cape & (rest_c[:, 2] > groups.yoke_rows) if groups.yoke_rows else np.zeros_like(cape)
        # the cape's side edges curl forward round the arms and tuck behind them by design: only the interior
        # (what covers the back) must never be crossed
        yoke |= cape & ~np.array([groups.cape_interior(Vector(c)) for c in rest_c], bool)
        self.f_cape = np.nonzero(cape & ~yoke)[0]
        self.f_yoke = np.nonzero(yoke)[0]
        self.f_armour = np.nonzero(toon & _match(names, groups.armour))[0]
        self.f_hand_r = np.nonzero(toon & _match(names, groups.hand_r))[0]
        self.f_hand_l = np.nonzero(toon & _match(names, groups.hand_l))[0]
        self.f_body = np.nonzero(toon)[0]
        self.f_body_nr = np.nonzero(toon & ~_match(names, groups.hand_r))[0]
        self.f_body_nl = np.nonzero(toon & ~_match(names, groups.hand_l))[0]

        def item_masks(obj, grip):
            m = obj.data
            mi_ = np.zeros(len(m.polygons), np.int32)
            m.polygons.foreach_get("material_index", mi_)
            c = np.zeros(len(m.polygons) * 3, np.float32)
            m.polygons.foreach_get("center", c)
            c = c.reshape(-1, 3)
            t = mi_ == 0
            g = np.array([grip(Vector(x)) for x in c], bool)
            return np.nonzero(t)[0], np.nonzero(t & ~g)[0]
        self.r_all, self.r_free = item_masks(item_r, groups.item_r_grip)
        self.l_all, self.l_free = item_masks(item_l, groups.item_l_grip)

    # ── deformed geometry ──────────────────────────────────────────────────────────────────────────────
    @staticmethod
    def _geom(obj):
        dg = bpy.context.evaluated_depsgraph_get()
        ev = obj.evaluated_get(dg)
        me = ev.to_mesh()
        n = len(me.vertices)
        co = np.zeros(n * 3, np.float32)
        me.vertices.foreach_get("co", co)
        co = co.reshape(n, 3)
        mw = np.array(ev.matrix_world)
        co = co @ mw[:3, :3].T + mw[:3, 3]
        lt = np.zeros(len(me.polygons), np.int32)
        me.polygons.foreach_get("loop_total", lt)
        ls = np.zeros(len(me.polygons), np.int32)
        me.polygons.foreach_get("loop_start", ls)
        lv = np.zeros(len(me.loops), np.int32)
        me.loops.foreach_get("vertex_index", lv)
        ev.to_mesh_clear()
        return co, lt, ls, lv, co.tolist()

    @staticmethod
    def _tree(geom, faces: np.ndarray):
        co, lt, ls, lv, co_list = geom
        if not len(faces):
            return None, 0.0
        polys = [lv[ls[f]:ls[f] + lt[f]].tolist() for f in faces]
        used = np.unique(np.concatenate([lv[ls[f]:ls[f] + lt[f]] for f in faces]))
        return BVHTree.FromPolygons(co_list, polys, all_triangles=False), float(co[used, 2].min())

    @staticmethod
    def _pairs(a, b) -> int:
        if a is None or b is None:
            return 0
        return len(a.overlap(b))

    def frame(self) -> dict:
        gb, gr, gl = self._geom(self.body), self._geom(self.item_r), self._geom(self.item_l)
        t_body, zb = self._tree(gb, self.f_body)
        co, lt, ls, lv, _ = gb
        fz = np.array([co[lv[ls[f]:ls[f] + lt[f]], 2].min() for f in self.f_body])
        low_part = self.names[self.f_body[int(fz.argmin())]]
        t_r, zr = self._tree(gr, self.r_all)
        t_l, zl = self._tree(gl, self.l_all)
        t_rf, _ = self._tree(gr, self.r_free)
        t_lf, _ = self._tree(gl, self.l_free)
        t_bnr, _ = self._tree(gb, self.f_body_nr)
        t_bnl, _ = self._tree(gb, self.f_body_nl)
        t_hr, _ = self._tree(gb, self.f_hand_r)
        t_hl, _ = self._tree(gb, self.f_hand_l)
        t_cape, _ = self._tree(gb, self.f_cape)
        t_arm, _ = self._tree(gb, self.f_armour)
        out = {
            "groundBody": zb * 100.0, "groundR": zr * 100.0, "groundL": zl * 100.0, "lowPart": low_part,
            "swordShield": self._pairs(t_r, t_l),
            "swordBody": self._pairs(t_r, t_bnr) + self._pairs(t_rf, t_hr),
            "shieldBody": self._pairs(t_l, t_bnl) + self._pairs(t_lf, t_hl),
            "capeArmour": self._pairs(t_cape, t_arm),
        }
        for key, ta, tb, fb in (("swordParts", t_r, t_bnr, self.f_body_nr), ("shieldParts", t_l, t_bnl, self.f_body_nl)):
            if ta is not None and tb is not None:
                hit = {}
                for i, j in ta.overlap(tb):
                    n = self.names[fb[j]]
                    hit[n] = hit.get(n, 0) + 1
                if hit:
                    out[key] = hit
        if out["capeArmour"]:
            hit = {}
            for i, j in t_cape.overlap(t_arm):
                n = self.names[self.f_armour[j]]
                hit[n] = hit.get(n, 0) + 1
            out["capeParts"] = hit
        return out

    # ── head direction ─────────────────────────────────────────────────────────────────────────────────
    def head(self) -> dict:
        """Visor direction: yaw off the strike line (−Y) and pitch (+ = up), degrees."""
        rig = self.rig
        pb = rig.obj.pose.bones["head"]
        b = rig.obj.data.bones["head"]
        n = (rig.obj.matrix_world.to_3x3() @ pb.matrix.to_3x3() @ b.matrix_local.to_3x3().inverted()
             @ Vector((0, -1, 0))).normalized()
        cb = rig.obj.pose.bones["spine_03"]
        c = (rig.obj.matrix_world.to_3x3() @ cb.matrix.to_3x3() @ rig.obj.data.bones["spine_03"].matrix_local
             .to_3x3().inverted() @ Vector((0, -1, 0))).normalized()
        return {"headYaw": math.degrees(math.atan2(n.x, -n.y)), "headPitch": math.degrees(math.asin(max(-1, min(1, n.z)))),
                "chestYaw": math.degrees(math.atan2(c.x, -c.y))}

    def run(self, clips, actions, step: int = 1, names: Sequence[str] | None = None) -> dict:
        rep = {}
        saved = self.rig.obj.rotation_euler.copy()
        self.rig.obj.rotation_euler = (0, 0, 0)
        for c, a in zip(clips, actions):
            if names and c.name not in names:
                continue
            anim.assign(self.rig, a)
            notif = {scene.ms_to_frame(d["ms"]) for d in c.notify_list() if d["name"] in ("Contact", "Release")}
            frames = []
            for fr in sorted(set(range(0, c.frames + 1, step)) | notif):
                anim.set_frame(fr)
                f = self.frame()
                f["frame"] = fr
                if fr in notif:
                    f.update(self.head())
                frames.append(f)
            worst = {
                "groundBody": min(f["groundBody"] for f in frames),
                "groundR": min(f["groundR"] for f in frames),
                "groundL": min(f["groundL"] for f in frames),
                "swordShield": max(f["swordShield"] for f in frames),
                "swordBody": max(f["swordBody"] for f in frames),
                "shieldBody": max(f["shieldBody"] for f in frames),
                "capeArmour": max(f["capeArmour"] for f in frames),
            }
            rep[c.name] = {"frames": frames, "worst": worst}
        self.rig.obj.rotation_euler = saved
        anim.set_frame(0)
        return rep

    @staticmethod
    def verdict(rep: dict, limits: dict, per_clip: dict | None = None) -> list[str]:
        """Failures (``limits``: {metric: bound}; ground metrics are lower bounds in cm, the others upper bounds
        in triangle pairs; ``per_clip`` overrides per clip name)."""
        fails = []
        for clip, r in rep.items():
            lim = dict(limits)
            lim.update((per_clip or {}).get(clip, {}))
            for k, v in r["worst"].items():
                if k not in lim:
                    continue
                bad = v < lim[k] if k.startswith("ground") else v > lim[k]
                if bad:
                    frs = [f["frame"] for f in r["frames"]
                           if (f[k] < lim[k] if k.startswith("ground") else f[k] > lim[k])]
                    fails.append(f"{clip}: {k} {v:.1f} beyond {lim[k]} (frames {frs[:12]})")
        return fails

    @staticmethod
    def table(rep: dict) -> str:
        rows = [f"{'clip':16s} {'gBody':>6s} {'gSword':>6s} {'gShld':>6s} {'sw×sh':>6s} {'sw×bd':>6s} "
                f"{'sh×bd':>6s} {'cape':>6s}  head@contact (yaw/pitch, chest yaw)"]
        for clip, r in rep.items():
            w = r["worst"]
            hd = "; ".join(f"F{f['frame']} {f['headYaw']:+.0f}/{f['headPitch']:+.0f} c{f['chestYaw']:+.0f}"
                           for f in r["frames"] if "headYaw" in f)
            rows.append(f"{clip:16s} {w['groundBody']:6.1f} {w['groundR']:6.1f} {w['groundL']:6.1f} "
                        f"{w['swordShield']:6d} {w['swordBody']:6d} {w['shieldBody']:6d} {w['capeArmour']:6d}  {hd}")
        return "\n".join(rows)
