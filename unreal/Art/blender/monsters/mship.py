"""Ship + review helpers shared by the monster generators (``monsters/*.py``): bake every clip, export FBX
(SK + A_ per clip + held items / attachments as SM), write the manifest entries (merging, through
``kit.export.Manifest``), per-frame geometry QA on the deformed meshes (``heroes/hero_base.PairQA``: ground,
item × body), re-import verification of what UE receives, and the review set in ``Art/Previews/<dir>/`` — every
game-camera cell rendered at the shipped look (screen-constant ink, bloom, runtime glow cards) and measured by the
kit's ink gate (``review.ink_verdict``; the ship exits 1 on a failure).
"""
from __future__ import annotations

import json
import math
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Sequence

import bpy
import numpy as np
from mathutils import Euler, Matrix, Vector

from kit import anim, export, mesh as M, outline, paths, pngio, review, rig as R, scene, shading

import common as C


# LOD1 screen size (UE ComputeBoundsScreenSize = diameter / view height at the distance): a monster is 0.10–0.20 at
# every W1 zoom (0.75×–1.25× of the default distance), so desktop always draws LOD0; LOD1 is the mobile-low mesh
# (skeletal LOD bias 1 / MinLOD 1) and the far fallback below 0.05.
LOD1_SCREEN_SIZE = 0.05


def screen_size(radius_m: float, zoom: float = 1.0) -> float:
    """UE screen size of a bounding sphere of ``radius_m`` at the W1 camera (16:9, horizontal FOV), ``zoom`` ×
    the default distance."""
    p11 = (16.0 / 9.0) / math.tan(math.radians(shading.CAM_FOV_H / 2.0))
    return 2.0 * 0.5 * p11 * radius_m / (shading.CAM_DIST_DEFAULT * zoom)


def toon_tris(obj) -> int:
    """Toon-lit triangles: every material slot but the outline hull (slot 1) — the slime's dithered shell (slot
    2) included."""
    return sum(M.tri_count(obj, i) for i in range(len(obj.data.materials)) if i != 1)


def apply_bp(A, bp) -> None:
    anim.assign(A.rig, None)
    anim.apply_pose(A.solve(bp))


class MReview(review.Review):
    """``kit.review.Review`` whose measured game cells also report the rim fraction at the **class width itself**
    (``outline.SCREEN_PX_1080``: monster 3.0 px) next to the kit gate (3.0 − 0.5 slack = 2.5 px): ``fracRimSpec``
    / ``specPx`` in each ink report (the brief asks for ≥ 3 px on ≥ 90 % of character edges; the kit's gate is
    unchanged). Same renders and passes as ``Review.game_shot`` — one more ``ink_stats`` on the same coverage."""

    native_prefixes: tuple = ()          # sheets stored truecolour at scale 1 (``pngio.write_png_native``: tiled)

    def _save(self, name: str, img: np.ndarray) -> Path:
        if any(name.startswith(pfx) for pfx in self.native_prefixes):
            written = pngio.write_png_native(self.out / name, img)
            self.written.extend(written)
            return written[0][0]
        return super()._save(name, img)

    def game_shot(self, border=None, res=(1920, 1080), measure: bool = False, tag: str = "", glow: bool = True,
                  bloom_on=None):
        if not measure:
            return super().game_shot(border, res, measure=False, tag=tag, glow=glow, bloom_on=bloom_on)
        cam = bpy.context.scene.camera
        raw = review.render_array(border)
        body, hull = review.coverage_pass(self.meshes, border, ground=self.ground_occludes,
                                          extra_body=self.extra_body_slots)
        ignore = None
        if self.ground_occludes:
            b0, h0 = review.coverage_pass(self.meshes, border, ground=False, extra_body=self.extra_body_slots)
            buried = (b0 + h0) - (body + hull) > 0.25
            for _ in range(2):
                pb = np.pad(buried, 1, constant_values=False)
                buried = buried | pb[:-2, 1:-1] | pb[2:, 1:-1] | pb[1:-1, :-2] | pb[1:-1, 2:]
            ignore = buried
        img = self.post(raw, cam, res, border, glow=glow, bloom_on=bloom_on)
        st = review.ink_stats(body, hull, img, min_px=self.gate["minPx"], dark_lum=self.gate["darkLum"],
                              within=self.gate["darkWithinPx"], hull_rgb=raw, ignore=ignore)
        spec = float(outline.SCREEN_PX_1080.get(self.outline_class, self.gate["minPx"]))
        st2 = review.ink_stats(body, hull, None, min_px=spec, ignore=ignore)
        st["specPx"], st["fracRimSpec"] = spec, st2["fracRim"]
        st["tag"] = tag
        st["gate"] = self.gate
        self.ink_reports.append(st)
        return img, st


def ink_summary(reports) -> dict:
    """``review.ink_verdict`` + the fraction of edges at the class width (``MReview``): worst cell and median."""
    v = review.ink_verdict(reports)
    gated = [r for r in reports if r.get("gated", True) and "fracRimSpec" in r]
    if gated:
        fr = [r["fracRimSpec"] for r in gated]
        worst = min(gated, key=lambda r: r["fracRimSpec"])
        v["specPx"] = gated[0]["specPx"]
        v["minFracRimSpec"] = round(min(fr), 4)
        v["medianFracRimSpec"] = round(float(np.median(fr)), 4)
        v["worstSpecCell"] = worst.get("tag", "")
        v["specNote"] = (f"fraction of silhouette edge pixels with a rim >= {v['specPx']:g} px (the class width, "
                         f"spec §1.3) on every gated cell; the kit gate is {v['gate']['minPx']:g} px "
                         f"(outline.GATE_SLACK_PX)")
    return v


INK_KEYS = ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx", "maxHullLumP95", "gate", "specPx",
            "minFracRimSpec", "medianFracRimSpec", "worstSpecCell", "specNote", "outlinePx1080")


@dataclass
class Item:
    """A static mesh shipped with a skeletal monster: a held weapon (``socket`` = ``weapon_r``…) or an optional
    attachment of a variant (``variant`` set: not attached by default)."""
    obj: bpy.types.Object
    asset: str
    socket: str
    weapon_type: str = ""
    sockets: list = field(default_factory=list)          # [{'name', 'pos' (m, item-local), 'rot'}]
    extra: dict = field(default_factory=dict)
    grip: Callable[[Vector], bool] | None = None          # item-local face centre → in the fist (QA baseline)
    variant: str | None = None
    category: str = "Weapons"
    preview_obj: bpy.types.Object | None = None          # review stand-in (e.g. the totem without its flame)
    fx_objs: list = field(default_factory=list)          # runtime-lit FX parts: off for the ink measurement
    preview_up: str = "y"                                # items.png: the item axis laid up the frame ("z": a shield;
                                                         # "weapon": a variant held item laid out like a weapon;
                                                         # "weapon_flat": the same rolled 90° about +Y, a flat blade)


@dataclass
class MonsterShip:
    asset: str
    token: str                      # A_<token>_<Clip>
    preview_dir: str                # Art/Previews/<preview_dir>
    pal: object
    A: object                       # GoblinAnims-like (rig, solve, clips)
    body: bpy.types.Object
    items: list                     # [Item]
    clips: list
    outline_class: str = "monster"
    game_ids: Sequence[str] = ()
    blob_radius: float = 0.22
    height: float = 0.92
    display: dict = field(default_factory=dict)
    fx: dict = field(default_factory=dict)
    glows: Callable | None = None
    variants: dict = field(default_factory=dict)
    extra: dict = field(default_factory=dict)
    qa_pairs: dict = field(default_factory=dict)     # {metric: (groupA, groupB)}
    qa_groups: dict = field(default_factory=dict)
    qa_limits: dict = field(default_factory=dict)
    qa_clip_limits: dict = field(default_factory=dict)
    game_keys: list = field(default_factory=list)    # [(clip, frame, facing)]
    contact_keys: list = field(default_factory=list)
    back_clips: Sequence[str] = ("Attack01", "Death")
    close_keys: list = field(default_factory=list)
    category: str = "Monsters"
    skeleton_bones_required: Sequence[str] = ()
    extra_previews: Callable | None = None           # fn(ship, rv) after the standard set
    variant_previews: Sequence[str] = ()             # extra preview files listed on the variant attachments
    lod_note: str = ""
    lod1: bpy.types.Object | None = None             # SK_<asset>_LOD1 (same generator, decimated, trims skipped)
    lod1_screen_size: float = LOD1_SCREEN_SIZE
    portrait: Callable | None = None                 # fn(ship, rv, actions) → manifest path (``Portraits/T_UI_…``)
    outline_px: float | None = None                  # per-asset OutlinePx1080 of the hull slot (None = the class's)
    outline_px_note: str = ""                        # why the asset asks for more ink than its class (manifest)
    native_sheets: bool = False                      # game_* sheets truecolour at scale 1, tiled <stem>_2.png … (a
                                                     # tall figure's big 1080p cells would otherwise be downscaled)


def ink_px(ms: MonsterShip) -> float:
    """Screen ink width at 1080p the asset ships with: its own ``outline_px`` request (the art review's per-asset
    hull request, e.g. 3.5 px on a monster whose thin ears / spear / slime base lost ink at the class's 3.0 px) or
    the class width (``outline.SCREEN_PX_1080``)."""
    return float(ms.outline_px) if ms.outline_px else float(outline.SCREEN_PX_1080[ms.outline_class])


def gate_for(outline_class: str, px: float) -> dict:
    """``review.ink_gate`` of the class with the rim threshold taken from the shipped request ``px`` (px −
    ``GATE_SLACK_PX``): a 3.5 px monster is gated at 3.0 px — the class width itself."""
    g = dict(review.ink_gate(outline_class))
    g["minPx"] = round(max(0.5, px - outline.GATE_SLACK_PX), 2)
    return g


def bake_clip(rig, c, token):
    """Pose-driven clips bake with ``kit.anim.bake``; basis-driven ones (``c.basis_fn``: the slime's squash and
    stretch bone scales) with ``kit.anim.bake_basis``."""
    fn = getattr(c, "basis_fn", None)
    return anim.bake_basis(rig, c, token, fn) if fn is not None else anim.bake(rig, c, token)


def held(ms: MonsterShip) -> list:
    return [it for it in ms.items if it.variant is None]


def shown(it: Item):
    return it.preview_obj if it.preview_obj is not None else it.obj


def fx_objects(ms: MonsterShip) -> list:
    return [o for it in held(ms) for o in it.fx_objs]


class FxOff:
    """Runtime-lit FX parts hidden and glow cards off (the ink is measured on the inked body only)."""

    def __init__(self, ms: MonsterShip, rv):
        self.objs, self.rv = fx_objects(ms), rv

    def __enter__(self):
        for o in self.objs:
            o.hide_render = True
        self.g = self.rv.glow_scale
        self.rv.glow_scale = 0.0 if self.objs else self.g
        return self

    def __exit__(self, *exc):
        for o in self.objs:
            o.hide_render = False
        self.rv.glow_scale = self.g


def fx_shot(ms: MonsterShip, rv, box):
    """The displayed cell with the FX parts on (in the bloom source too) and the glow cards."""
    objs = fx_objects(ms)
    rv.meshes.extend(objs)
    try:
        img, _ = rv.game_shot(box, measure=False)
    finally:
        for o in objs:
            rv.meshes.remove(o)
    return img


def parts_except(body, exclude: Sequence[str]) -> list[str]:
    """Body part names (no hull proxies) minus ``exclude`` (exact names) — QA groups like 'everything but the
    gripping fist'."""
    names = json.loads(body["af_parts"])
    return sorted({n for n in names if not n.endswith("_hull") and n not in set(exclude)})


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
ELITE_TABLE = Path(__file__).resolve().parent / "fx_elite_w2.json"


def apply_elite(asset: str, rig) -> dict | None:
    """Wave 2 (2H): the asset's ``fx_elite`` from ``fx_elite_w2.json`` (``elite_sigil.py --solve-elite``: the socket
    SM_FX_EliteSigil rides — clear of the figure in every facing at its bob's trough, close over the head in the front
    views), so a re-ship writes the solved socket. ``fx_overhead`` is never touched here: nameplates, floating text,
    stun stars, the death mark and the W1 crown ride it."""
    if not ELITE_TABLE.exists():
        return None
    t = json.loads(ELITE_TABLE.read_text()).get("assets", {}).get(asset)
    if t:
        R.add_socket(rig, "fx_elite", t["bone"], Vector(t["posM"]))
    return t


def ship(ms: MonsterShip, previews: bool = True, verify: bool = True) -> dict:
    t0 = time.time()
    rig, body, pal = ms.A.rig, ms.body, ms.pal
    if apply_elite(ms.asset, rig):
        print(f"[socket] {ms.asset}: fx_elite from {ELITE_TABLE.name}")
    print(f"[build] {ms.asset}: body {toon_tris(body)} toon + {M.tri_count(body, 1)} hull tris, "
          f"bones {len(rig.obj.data.bones)}; items " + ", ".join(
              f"{it.asset} {M.tri_count(it.obj, 0)}+{M.tri_count(it.obj, 1)}" for it in ms.items))
    st = R.weight_stats(body)
    print(f"[weights] max {st['maxInfluences']} influences, {st['unweighted']} unweighted, "
          f"{st['notNormalised']} not normalised, {len(st['groups'])} groups")
    assert st["unweighted"] == 0 and st["maxInfluences"] <= 4
    if ms.lod1 is not None:
        st1 = R.weight_stats(ms.lod1)
        print(f"[build] LOD1 {toon_tris(ms.lod1)} toon + {M.tri_count(ms.lod1, 1)} hull tris; weights max "
              f"{st1['maxInfluences']}, {st1['unweighted']} unweighted")
        assert st1["unweighted"] == 0 and st1["maxInfluences"] <= 4
        ms.lod1.hide_render = True
    pal.save()
    # items (exported unattached, grip / socket origin at the origin)
    for it in ms.items:
        ship_item(it, pal, ms.asset, ms)
    actions = [bake_clip(rig, c, ms.token) for c in ms.clips]
    sk = export.export_skeletal_mesh(rig, [body], ms.asset, ms.category)
    sk1 = None
    if ms.lod1 is not None:
        sk1 = export.export_skeletal_mesh(rig, [ms.lod1], f"{ms.asset}_LOD1", ms.category)
        ms.lod1.hide_render = True
    entries = []
    for c, a in zip(ms.clips, actions):
        fbx = export.export_animation(rig, a, ms.category)
        e = export.clip_entry(c, a, fbx)
        for k, v in getattr(c, "manifest_extra", {}).items():
            e[k] = v
        entries.append(e)
    anim.assign(rig, None)              # bounds / sockets of the bind pose (not the last exported clip)
    anim.clear_pose(rig)
    bpy.context.view_layer.update()
    man = export.Manifest()
    man.set_palette(pal)
    extra = {"previews": [], "displayName": ms.display, "budget": budget_record(ms)}
    if ms.fx:
        extra["fx"] = ms.fx
    if ms.variants:
        extra["variants"] = ms.variants
    extra.update(ms.extra)
    entry = export.skeletal_entry(rig, body, sk, ms.category, pal, ms.outline_class, entries, list(ms.game_ids),
                                  blob_radius_cm=round(ms.blob_radius * 100.0, 1), extra=extra)
    entry["attachments"] = [{"object": it.asset, "socket": it.socket} for it in held(ms)]
    entry["materialSlots"][1]["outlineMix"] = outline.INK_MIX[ms.outline_class]
    _slot_px(entry["materialSlots"][1], ms)
    _extra_slots(entry, body, pal)
    entry["triangles"] = tri_record(body)
    if sk1 is not None:
        lo, hi = scene.world_bounds([body])
        rad = 0.5 * (hi - lo).length
        entry["lods"] = [{
            "index": 1, "asset": f"{ms.asset}_LOD1", "fbx": paths.rel_to_export(sk1),
            "screenSize": ms.lod1_screen_size,
            "screenSizeNote": f"UE screen size of {ms.asset} at the W1 camera: {screen_size(rad, 1.25):.3f} (max "
                              f"zoom-out) to {screen_size(rad, 0.75):.3f} (max zoom-in), {screen_size(rad):.3f} at the "
                              f"default distance — desktop always draws LOD0; LOD1 is the mobile-low gameplay mesh "
                              f"(skeletal LOD bias / MinLOD 1) and the far fallback",
            "triangles": tri_record(ms.lod1),
            "import": f"same {entry.get('skeleton', 'skeleton')} armature and bind pose; import as LOD index 1 of "
                      f"{ms.asset} (USkeletalMeshEditorSubsystem::ImportLOD / Interchange LOD import) — Blender's FBX "
                      "exporter cannot write an FbxLODGroup, so the LOD ships as its own file"}]
    man.set_asset(ms.asset, entry)
    man.save()
    print(f"[export] {sk.name} + {len(entries)} clips + {len(ms.items)} items ({time.time() - t0:.1f}s)")
    result = {"sk": str(sk), "lod1": str(sk1) if sk1 is not None else None, "anims": [e["fbx"] for e in entries],
              "items": [str(paths.category_dir(it.category) / f"{it.asset}.fbx") for it in ms.items],
              "manifest": str(man.path)}
    # per-frame geometry gate on the deformed meshes (variant attachments too: all of them at once)
    for it in ms.items:
        R.attach_to_bone(it.obj, rig, it.socket)
    rep = qa_run(ms, actions)
    print("\n[qa] per-clip worst values (ground cm; overlaps = intersecting triangle pairs)")
    from hero_base import PairQA
    print(PairQA.table(rep))
    fails = PairQA.verdict(rep, ms.qa_limits or {"ground": -0.5}, ms.qa_clip_limits)
    for f in fails:
        print("  QA FAIL " + f)
    result["qa"] = {"pass": not fails, "failures": fails, "worst": {k: v["worst"] for k, v in rep.items()}}
    if verify:
        src = record(rig, ms.clips, actions)
    if previews:
        out = paths.preview_root() / ms.preview_dir
        result["previews"], result["ink"], result["portrait"] = render_previews(ms, out, actions)
        man = export.Manifest()
        man.data["assets"][ms.asset]["previews"] = [f"{ms.preview_dir}/{p.name}" for p in sorted(out.glob("*.png"))]
        for it in ms.items:
            if it.asset in man.data["assets"]:
                man.data["assets"][it.asset]["previews"] = [f"{ms.preview_dir}/items.png"] + (
                    [f"{ms.preview_dir}/{p}" for p in ms.variant_previews] if it.variant is not None else [])
        ink = result["ink"]
        man.data["assets"][ms.asset]["inkGate"] = {k: ink[k] for k in INK_KEYS if k in ink}
        if result.get("portrait"):
            man.data["assets"][ms.asset]["portrait"] = result["portrait"]
        man.save()
    if verify:
        result["verify"] = verify_export(ms, Path(sk), src, entry)
        if sk1 is not None:
            lv = verify_lod(Path(sk), Path(sk1), entry)
            result["verify"]["pass"] = result["verify"]["pass"] and lv["pass"]
            result["verify"]["failures"] += lv["failures"]
    print(f"[ship] {ms.asset} done in {time.time() - t0:.1f}s")
    return result


def _slot_px(slot: dict, ms: MonsterShip) -> None:
    """The per-asset ink request on an outline slot (the UE importer makes an ``M_AF_Outline`` instance with this
    ``OutlinePx1080`` when it differs from the class's)."""
    if ms.outline_px and abs(float(ms.outline_px) - float(slot.get("outlinePx1080", 0.0))) > 1e-6:
        slot["outlinePx1080"] = float(ms.outline_px)
        slot["outlinePxClass"] = float(outline.SCREEN_PX_1080.get(slot.get("class", ms.outline_class), 0.0))
        if ms.outline_px_note:
            slot["outlinePxNote"] = ms.outline_px_note


def _extra_slots(entry: dict, body, pal) -> None:
    """Material slots beyond toon + outline (the slime's dithered shell): from the mesh's material list."""
    mats = body.data.materials
    for i in range(2, len(mats)):
        m = mats[i]
        e = {"index": i, "name": m.name, "parent": m.get("af_parent", "M_AF_Toon"), "palette": pal.family}
        if "af_params" in m:
            e["params"] = json.loads(m["af_params"])
        entry["materialSlots"].append(e)


def ship_item(it: Item, pal, holder: str, ms: MonsterShip | None = None) -> dict:
    """Export in the item's own space (grip / socket origin at the origin): the generators attach the held weapons to
    their bones before shipping, and an attached item would carry the bone's bind-pose frame into the FBX node
    transform, the ``SOCKET_*`` empties and ``boundsCm`` — so it is unparented for the export, then re-attached."""
    o = it.obj
    saved = (o.parent, o.parent_type, o.parent_bone, o.matrix_parent_inverse.copy(), o.matrix_basis.copy())
    o.parent = None
    o.matrix_world = Matrix.Identity(4)
    bpy.context.view_layer.update()
    try:
        return _ship_item(it, pal, holder, ms)
    finally:
        o.parent, o.parent_type, o.parent_bone = saved[:3]
        o.matrix_parent_inverse = saved[3]
        o.matrix_basis = saved[4]
        bpy.context.view_layer.update()


def _ship_item(it: Item, pal, holder: str, ms: MonsterShip | None = None) -> dict:
    fbx = export.export_static_mesh(it.obj, it.asset, it.category, it.sockets)
    man = export.Manifest()
    man.set_palette(pal)
    cls = str(it.obj.get("af_outline_class", "monster"))
    ex = {"heldBy": [holder], **it.extra}
    if it.variant is None:
        ex.update({"attachSocket": it.socket})
        if it.weapon_type:
            ex["weaponType"] = it.weapon_type
    else:
        ex.update({"attachSocket": it.socket, "variant": it.variant})
    e = export.static_entry(it.obj, fbx, it.category, pal, cls, sockets=it.sockets, extra=ex)
    if len(e["materialSlots"]) > 1:
        e["materialSlots"][1]["outlineMix"] = outline.INK_MIX.get(cls, 1.0)
        if ms is not None:                    # held items / attachments ink like their holder
            _slot_px(e["materialSlots"][1], ms)
    man.set_asset(it.asset, e)
    man.save()
    return e


def tri_record(obj) -> dict:
    """``{toon, outline}`` (+ ``bySlot`` when a mesh has more than toon + outline: the slime's dithered shell)."""
    rec = {"toon": toon_tris(obj), "outline": M.tri_count(obj, 1)}
    if len(obj.data.materials) > 2:
        rec["bySlot"] = {m.name: M.tri_count(obj, i) for i, m in enumerate(obj.data.materials)}
    return rec


def budget_record(ms: MonsterShip) -> dict:
    t = M.tri_count
    body = ms.body
    items = {"toon": sum(t(i.obj, 0) for i in held(ms)), "outline": sum(t(i.obj, 1) for i in held(ms))}
    weighted = sorted({g.name for g in body.vertex_groups})
    rec = {
        "spec": "art-inventory-ch1 §1.9 goblin family: LOD0 6-9k, LOD1 3.5k triangles, <= 60 bones per section; "
                "slime 2-3k, LOD1 1.5k, <= 16 bones",
        "lod0": tri_record(body),
        "heldItems": items,
        "weightedBones": len(weighted),
        "decision": "The §1.9 figures budget the toon surface (every toon-lit slot); the baked hull (P7) is a second "
                    "cheap unlit draw (same skinning, one-colour pixel shader) budgeted separately at <= 0.75x the "
                    "toon triangles via padded low-poly proxies and no-hull trims. Small trims are low-poly (5-sided "
                    "studs, painted single-sided strokes), skin fully under gear is cut. " + ms.lod_note,
    }
    if ms.lod1 is not None:
        rec["lod1"] = tri_record(ms.lod1)
    return rec


# ── QA ──────────────────────────────────────────────────────────────────────────────────────────────────
def qa_run(ms: MonsterShip, actions) -> dict:
    from hero_base import PairQA
    names = json.loads(ms.body["af_parts"])
    items = {it.asset: it.obj for it in ms.items if it.obj.parent is not None}
    grips = {it.asset: it.grip for it in ms.items if it.grip is not None}
    groups = {"body": "*"}
    groups.update(ms.qa_groups)
    pairs = dict(ms.qa_pairs)
    q = PairQA(ms.A.rig, ms.body, items, names, groups, pairs, grips)
    return q.run(ms.clips, actions)


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
def render_previews(ms: MonsterShip, out: Path, actions) -> tuple[list, dict]:
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("*.png"):
        p.unlink()
    rig = ms.A.rig
    for it in held(ms):
        R.attach_to_bone(it.obj, rig, it.socket)
        if it.preview_obj is not None:
            R.attach_to_bone(it.preview_obj, rig, it.socket)
            it.obj.hide_render = True
    for it in ms.items:
        if it.variant is not None:
            it.obj.hide_render = True
    meshes = [ms.body] + [shown(it) for it in held(ms)]
    rv = MReview(ms.asset, rig.obj, meshes, ms.height, blob_radius=ms.blob_radius, out_dir=out,
                 game_outline_px=ink_px(ms), outline_class=ms.outline_class, glows=ms.glows)
    rv.gate = gate_for(ms.outline_class, ink_px(ms))
    rv.ground_occludes = True          # hull under planted soles is hidden by the terrain (UE) and the stage here
    rv.extra_body_slots = True         # a dithered shell slot (the slime) is body in the ink coverage
    if ms.native_sheets:
        rv.native_prefixes = ("game_",)
    acts = dict(zip([c.name for c in ms.clips], actions))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    with FxOff(ms, rv):
        rv.game_view()
    if fx_objects(ms):
        fx_view(ms, rv)
    rv.closeup()
    (out / "closeup.png").rename(out / "closeup_se.png")
    rv.closeup("front", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    rv.closeup("ne", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_ne.png")
    rv.turnaround()
    rv.material_ab()
    for c, a in zip(ms.clips, actions):
        rv.contact_sheet(rig, [(c, a)], frames=8)
        (out / "anims.png").rename(out / f"anim_{c.name}.png")
    game_sheet(ms, rv, ms.game_keys, "game_poses.png", actions)
    if ms.contact_keys:
        game_sheet(ms, rv, ms.contact_keys, "game_contacts.png", actions)
    facing_keys = [(n, f, fac) for fac in ("front", "sw", "ne", "back") for n, f in (("Idle", 0), ("Walk", 9))]
    game_sheet(ms, rv, facing_keys, "game_facings.png", actions)
    if ms.close_keys:
        closeup_poses(ms, rv, actions)
    back_sheets(ms, rv, actions)
    items_sheet(ms, rv)
    if ms.lod1 is not None:
        lod_sheet(ms, rv, actions)
    if ms.extra_previews is not None:
        ms.extra_previews(ms, rv, actions)
    portrait = ms.portrait(ms, rv, actions) if ms.portrait is not None else None
    rv.facing("se")
    for p, size in rv.written:
        print(f"  {p.name:28s} {size // 1024:4d} KB")
    ink = ink_summary(rv.ink_reports)
    ink["outlinePx1080"] = ink_px(ms)
    if ms.outline_px:
        ink["specNote"] += (f"; shipped request {ink_px(ms):g} px (per-asset outlinePx1080, class "
                            f"{outline.SCREEN_PX_1080[ms.outline_class]:g} px): gated at {rv.gate['minPx']:g} px")
    print(f"\n[ink] {ink['cells']} gated 1080p cells: min rim {ink['minFracRim'] * 100:.1f} %, "
          f"min dark {ink['minFracDark'] * 100:.1f} %, median rim {ink['medianRimPx']} px; at the class width "
          f"{ink.get('specPx', 0):g} px: min {ink.get('minFracRimSpec', 0) * 100:.1f} % ({ink.get('worstSpecCell')}), "
          f"median {ink.get('medianFracRimSpec', 0) * 100:.1f} %")
    for f in ink["failures"]:
        print("  INK FAIL " + f)
    for it in ms.items:
        it.obj.hide_render = False
        if it.preview_obj is not None:
            it.preview_obj.hide_render = True
    return [str(p) for p, _ in rv.written], ink, portrait


def _crop_hw(ms: MonsterShip) -> float:
    """Half width (fraction of 1920) of a game cell: the warrior's 0.075 for 1.76 m, scaled, ≥ 0.05."""
    return max(0.05, 0.075 * (ms.height + 0.35) / 2.1)


def game_sheet(ms: MonsterShip, rv, keys, fname, actions, cols: int = 4) -> None:
    """Native 1080p crops at the W1 camera with the shipped look (UE ink width, bloom, glow cards); every cell's
    ink rim is measured (``rv.ink_reports``) and printed under its label."""
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    cells = []
    review.setup_render((1920, 1080), 8)
    hw = _crop_hw(ms)
    zc = ms.height * 0.45
    for name, fr, fac in keys:
        rv.facing(fac)
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        if hasattr(ms, "glow_frame"):
            ms.glow_frame(rv, name, fr)
        cam = review.game_camera()
        rv.game_outline(1080, Vector((0, 0, zc)))
        c = review.project(cam, Vector((0, 0, zc)))
        hh = hw * 1920 / 1080
        box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
        with FxOff(ms, rv):
            img, st = rv.game_shot(box, measure=True, tag=f"{fname} {name} F{fr} {fac}")
        if fx_objects(ms):
            img = fx_shot(ms, rv, box)
        review.draw_text(img, 3, 3, f"{name} F{fr} {fac}")
        review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
        cells.append(img)
    rv.glow_scale = 1.0
    rv._save(fname, review.grid(cells, cols))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)


def w2_sheet(keys, fname: str = "game_w2.png", cols: int = 4):
    """``extra_previews`` hook of the Wave 2 clips (dark remake 2H): the game-camera cells of the clips Wave 2 added
    (``keys`` = [(clip, frame, facing)]) on a sheet of their own, so ``clip_patch.py`` can add a clip to a shipped
    monster and take only that clip's previews (the shipped sheets of the unchanged clips stay as reviewed)."""
    def fn(ms, rv, actions):
        game_sheet(ms, rv, keys, fname, actions, cols)
    return fn


def chain_previews(*fns):
    """Several ``extra_previews`` hooks in order (``None`` entries skipped)."""
    def fn(ms, rv, actions):
        for f in fns:
            if f is not None:
                f(ms, rv, actions)
    return fn


def fx_view(ms: MonsterShip, rv, facing: str = "se") -> None:
    """``game_fx.png``: the 1080p 1:1 crop with the runtime FX on (flame mesh, bloom, glow cards) — the measured
    ``game_1080crop.png`` / ``game_inkgate.png`` are the inked body without them."""
    rv.facing(facing)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera()
    box = rv.game_crop_box(cam)
    rv.game_outline(1080)
    img = fx_shot(ms, rv, box)
    review.draw_text(img, 4, 4, "1080P 1:1 SHIPPED + RUNTIME FX (FLAME, GLOW CARDS, BLOOM)")
    rv._save("game_fx.png", img)


POSES_PER_SHEET = 5


def closeup_poses(ms: MonsterShip, rv, actions) -> None:
    """The intersection check: one close-up per ``close_keys`` entry, ``POSES_PER_SHEET`` cells per sheet
    (``poses_close_a.png``, ``_b`` …) — a single 2-row sheet went over the 200 KB budget and was downscaled to
    ~130 px cells with illegible labels; one row of 5 stays 1:1 (cells 240 × 290, labels at native size)."""
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    meshes = [ms.body] + [shown(it) for it in held(ms)]
    cells = []
    res = (240, 290)
    for name, fr, fac in ms.close_keys:
        rv.facing(fac)
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        review.setup_render(res, 8)
        pts = review.mesh_points(meshes)
        review.frame_points(pts, res, fov_h=26, pitch_ue=-35, fill=0.92)
        rv.ink(at=Vector(pts.mean(0)))
        img = review.render_array()
        clip = next(c for c in ms.clips if c.name == name)
        mark = ""
        for d in clip.notify_list():
            if scene.ms_to_frame(d["ms"]) == fr:
                mark = f" {d['name'].upper()}"
        review.draw_text(img, 3, 3, f"{name} F{fr}{mark}")
        cells.append(img)
    n = POSES_PER_SHEET
    for k in range(0, len(cells), n):
        part = cells[k:k + n]
        while len(part) < n and len(cells) > n:
            part.append(np.full_like(part[0], 24))
        rv._save(f"poses_close_{chr(ord('a') + k // n)}.png", review.grid(part, len(part)))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)


BACK_FACINGS = ("ne", "back", "nw")


def back_sheets(ms: MonsterShip, rv, actions, cell=(150, 180)) -> None:
    """Key poses + contact from behind at the game pitch (rows ne / back / nw): ``anim_back_<Clip>.png``."""
    rig = ms.A.rig
    meshes = [ms.body] + [shown(it) for it in held(ms)]
    acts = dict(zip([c.name for c in ms.clips], actions))
    for c in ms.clips:
        if c.name not in ms.back_clips:
            continue
        anim.assign(rig, acts[c.name])
        notif = {scene.ms_to_frame(d["ms"]): d for d in c.notify_list() if d["name"] in ("Contact", "Release")}
        keys = getattr(c, "key_poses_ms", None) or [c.length_ms * i / 5 for i in range(6)]
        frames = sorted({min(c.frames, scene.ms_to_frame(ms_)) for ms_ in keys} | set(notif))
        pts = []
        for fc in BACK_FACINGS:
            rv.facing(fc)
            for fr in frames:
                anim.set_frame(fr)
                pts.append(review.mesh_points(meshes, step=4))
        pts = np.concatenate(pts)
        review.setup_render(cell, 4)
        rows = []
        for fc in BACK_FACINGS:
            rv.facing(fc)
            review.frame_points(pts, cell, fov_h=24.0, pitch_ue=shading.CAM_PITCH_UE, fill=0.95)
            rv.ink(at=Vector(pts.mean(0)))
            cells = []
            for fr in frames:
                anim.set_frame(fr)
                img = review.render_array()
                review.draw_text(img, 3, 3, f"{fc} F{fr}")
                if fr in notif:
                    review.border(img)
                    review.draw_text(img, 3, 12, notif[fr]["name"], rgb=(255, 170, 70))
                cells.append(img)
            rows.append(review.grid(cells, len(cells), pad=2))
        sheet = np.vstack(rows)
        label = np.zeros((14, sheet.shape[1], 3), np.uint8)
        label[:, :] = (24, 20, 30)
        review.draw_text(label, 4, 4, f"{c.name}  BACK VIEWS NE / BACK / NW  PITCH -50  KEY POSES")
        rv._save(f"anim_back_{c.name}.png", np.vstack([label, sheet]))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")


def items_sheet(ms: MonsterShip, rv) -> None:
    """Every shipped item on its own (two sides) over the studio colour: ``items.png``."""
    if not ms.items:
        return
    cells = []
    res = (240, 300)
    objs = [it.obj for it in ms.items]
    saved = {o.name: (o.parent, o.parent_type, o.parent_bone, o.matrix_parent_inverse.copy(), o.matrix_basis.copy(),
                      o.hide_render) for o in objs}
    hidden = [o for o in bpy.data.objects if o.type == "MESH" and o not in objs and not o.hide_render]
    for o in hidden:
        o.hide_render = True
    review.hide_stage(True)
    for o in objs:
        o.parent = None
        o.matrix_world = Matrix.Identity(4)
        o.hide_render = True
    for it in ms.items:
        o = it.obj
        o.hide_render = False
        for yaw, side in ((135, "A"), (-45, "B")):
            if it.variant is None and it.preview_up == "z":     # a shield: upright, face and back
                o.matrix_world = Matrix.Rotation(math.radians(yaw + 45.0), 4, "Z")
            elif it.variant is None or it.preview_up.startswith("weapon"):   # weapons: the +Y blade axis up the frame
                roll = Matrix.Rotation(math.radians(90), 4, "Y") if it.preview_up == "weapon_flat" else Matrix()
                o.matrix_world = (Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X")
                                  @ roll)
            else:                    # attachments (bone-local): upright as worn on the rest pose
                bm = ms.A.rig.obj.data.bones[it.socket].matrix_local.to_3x3().to_4x4()
                o.matrix_world = Matrix.Rotation(math.radians(yaw - 90), 4, "Z") @ bm
            bpy.context.view_layer.update()
            review.setup_render(res, 8, bg_hex="#3B3546")
            pts = review.mesh_points([o])
            review.frame_points(pts, res, fov_h=24, pitch_ue=-15, fill=0.86, include_ground=False)
            review.set_outline_px([o], 1.6, Vector(pts.mean(0)))
            img = review.render_array()
            review.draw_text(img, 3, 3, f"{it.asset.replace('SM_Mon_', '')} {side}")
            cells.append(img)
        o.hide_render = True
    rv._save("items.png", review.grid(cells, min(4, len(cells))))
    for o in hidden:
        o.hide_render = False
    review.hide_stage(False)
    for o in objs:
        par, ptype, pbone, mpi, mb, hr = saved[o.name]
        o.parent, o.parent_type, o.parent_bone = par, ptype, pbone
        o.matrix_parent_inverse = mpi
        o.matrix_basis = mb
        o.hide_render = hr


PORTRAIT_INK_PX = 4.5            # silhouette ink of the 512² UI portraits (heroes / NPCs: the same)


def portrait_shot(ms: MonsterShip, rv, bp, name: str, eye_sockets=("fx_eye_l", "fx_eye_r"), eye_glow=None,
                  yaw: float = -110.0, bust_below_head: float = 0.42, bust_radius: float = 0.45,
                  zoom: tuple = (96, 352, 128, 384), glows=None, pitch: float = -9.0) -> str:
    """Spec §9.5 / R11 UI portrait of a monster (the heroes' / NPCs' pattern): the **Portrait pose** ``bp`` (a
    BP solved on the rig with the clip cloth; no exported clip), 3/4 front (``yaw``), 512² RGBA head and
    shoulders, toon + screen-space ink (``review.ink_render``, no hull at 1:1), the runtime eye cards composited
    (``eye_glow`` = {radiusCm, color, alpha, coreColor, coreAlpha, coreFrac}; no bloom on a UI texture) and
    ``glows(rig) → [{at, radiusCm, color, alpha, core}]`` extra cards → ``Art/Export/Portraits/<name>.png`` +
    ``portrait.png`` (over the studio colour) and ``portrait_zoom2x.png`` (the 2× defect check) in the preview set.
    Returns the manifest path ``Portraits/<name>.png``."""
    rig = ms.A.rig
    apply_bp(ms.A, bp)
    bpy.context.view_layer.update()
    rv.facing(yaw)
    res = (512, 512)
    review.hide_stage(True)
    review.setup_render(res, 16)
    vis = [m for m in [ms.body] + [shown(it) for it in held(ms)] if not m.hide_render]
    pts = review.mesh_points(vis)

    def sock(nm):
        so = next(x for x in rig.sockets if x["name"] == nm)
        b = rig.obj.data.bones[so["bone"]]
        pb = rig.obj.pose.bones[so["bone"]]
        return rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(so["pos"])))
    hc = sock("fx_head")
    sel = pts[(pts[:, 2] > hc.z - bust_below_head) &
              (np.linalg.norm(pts[:, :2] - np.array(hc[:2]), axis=1) < bust_radius)]
    cam = review.frame_points(sel, res, fov_h=24, pitch_ue=pitch, fill=0.96, include_ground=False)
    for m in vis:
        outline.remove_preview(m)
    rgba = review.ink_render(vis, res, silhouette_px=PORTRAIT_INK_PX, interior_px=2.0, depth_step=0.018)
    if eye_glow:
        hb = rig.obj.data.bones["head"]
        pbh = rig.obj.pose.bones["head"]
        fwd = (rig.obj.matrix_world.to_3x3() @ pbh.matrix.to_3x3() @ hb.matrix_local.to_3x3().inverted()
               @ Vector((0, -1, 0))).normalized()
        for nm in eye_sockets:
            wp = sock(nm)
            vis_k = max(0.0, min(1.0, 2.0 * fwd.dot((cam.matrix_world.translation - wp).normalized()) + 0.4))
            cx, cy = review.project_px(cam, wp, res)
            rad = eye_glow["radiusCm"] / 100.0 / outline.pixel_world_size(cam, wp, res[0])
            review.composite_glow(rgba, (cx, cy), rad, eye_glow["color"], eye_glow["alpha"] * vis_k,
                                  core=eye_glow.get("coreFrac", 0.3))
    for g in (glows(rig) if glows else []):
        cx, cy = review.project_px(cam, g["at"], res)
        rad = g["radiusCm"] / 100.0 / outline.pixel_world_size(cam, g["at"], res[0])
        review.composite_glow(rgba, (cx, cy), rad, g["color"], g["alpha"], core=g.get("core", 0.0))
    outp = paths.export_root() / "Portraits" / f"{name}.png"
    outp.parent.mkdir(parents=True, exist_ok=True)
    pngio.write_png(outp, rgba)
    review.hide_stage(False)
    bg = np.array([59, 53, 70], np.float32)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    comp = (rgba[:, :, :3].astype(np.float32) * a + bg * (1 - a) + 0.5).astype(np.uint8)
    review.draw_text(comp, 4, 4, name.upper())
    rv._save("portrait.png", comp)
    y0, y1, x0, x1 = zoom
    z = comp[y0:y1, x0:x1].repeat(2, 0).repeat(2, 1)
    review.draw_text(z, 4, 4, "PORTRAIT 2X ZOOM")
    rv._save("portrait_zoom2x.png", z)
    anim.assign(rig, None)
    anim.clear_pose(rig)
    rv.facing("se")
    print(f"[portrait] {outp}")
    return f"Portraits/{name}.png"


def lod_sheet(ms: MonsterShip, rv, actions) -> None:
    """``lod1.png``: LOD0 vs LOD1 — se close-ups with the shipped ink in world units (top: Idle, Attack01 contact)
    and native 1080p game crops at the W1 camera (bottom): LOD0 / LOD1 with the screen-constant ink, and LOD1 with
    the baked hull alone = the mobile-low look (LOD bias 1, no WPO)."""
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    its = [shown(it) for it in held(ms)]
    body, body1 = ms.body, ms.lod1
    px = ink_px(ms)
    contact = ("Attack01", scene.ms_to_frame(250.0))
    cells = []
    res = (250, 300)                   # 1000 × 600: stays full-size under the 200 KB preview budget
    try:
        for clip, fr in (("Idle", 0), contact):
            anim.assign(rig, acts[clip])
            anim.set_frame(fr)
            rv.facing("se")
            for show, hide, tag in ((body, body1, "LOD0"), (body1, body, "LOD1")):
                show.hide_render, hide.hide_render = False, True
                meshes = [show] + its
                review.setup_render(res, 8)
                pts = review.mesh_points(meshes)
                review.frame_points(pts, res, fov_h=28.0, pitch_ue=-30.0, fill=0.9)
                for m in meshes:
                    if "af_outline_width" in m:
                        outline.set_preview_screen(m, world=review.ink_world(px))
                img = review.render_array()
                # triangle counts of the shipped mesh (= manifest triangles.lod0 / lods[0].triangles), drawn at 2×:
                # the 5×7 font's 8 / 6 and 9 / 3 are ambiguous at 1×
                review.draw_text(img, 3, 3, f"{tag} {clip} F{fr}")
                review.draw_text(img, 3, 13, f"TOON {toon_tris(show)}", scale=2)
                review.draw_text(img, 3, 29, f"HULL {M.tri_count(show, 1)}", scale=2)
                cells.append(img)
        zc = ms.height * 0.45
        for (clip, fr), show, baked, tag in ((("Idle", 0), body, False, "LOD0 SHIPPED"),
                                             (("Idle", 0), body1, False, "LOD1 SHIPPED"),
                                             (("Idle", 0), body1, True, "LOD1 MOBILE LOW"),
                                             (contact, body1, True, "LOD1 MOBILE LOW")):
            anim.assign(rig, acts[clip])
            anim.set_frame(fr)
            rv.facing("se")
            other = body1 if show is body else body
            show.hide_render, other.hide_render = False, True
            review.setup_render((1920, 1080), 8)
            cam = review.game_camera()
            review.set_outline_px([show] + its, None if baked else px, Vector((0, 0, zc)))
            c = review.project(cam, Vector((0, 0, zc)))
            hw, hh = res[0] / 1920 / 2, res[1] / 1080 / 2
            crop = review.render_array((c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh))
            img = np.zeros((res[1], res[0], 3), np.uint8)
            img[:, :] = crop[0, 0]
            h_, w_ = min(res[1], crop.shape[0]), min(res[0], crop.shape[1])
            img[:h_, :w_] = crop[:h_, :w_]
            review.draw_text(img, 3, 3, f"{tag} 1080P 1:1")
            review.draw_text(img, 3, 12, f"{clip} F{fr}")
            cells.append(img)
    finally:
        body.hide_render, body1.hide_render = False, True
    rv._save("lod1.png", review.grid(cells, 4))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")


def quick_review(asset, rig, meshes, attachments, A, out: Path, height: float, outline_class: str,
                 blob_radius: float = 0.22, glows=None, pose=None, px: float | None = None) -> None:
    """Mesh iteration: READY pose close-up (se / front / back), turnaround and the 1080p game crop → ``out``."""
    out.mkdir(parents=True, exist_ok=True)
    for o, bone in attachments:
        R.attach_to_bone(o, rig, bone)
    apply_bp(A, pose if pose is not None else A.ready())
    objs = list(meshes) + [o for o, _ in attachments]
    rv = review.Review(asset, rig.obj, objs, height, blob_radius=blob_radius, out_dir=out,
                       game_outline_px=px or outline.SCREEN_PX_1080[outline_class], outline_class=outline_class,
                       glows=glows)
    rv.gate = gate_for(outline_class, px or outline.SCREEN_PX_1080[outline_class])
    rv.ground_occludes = True
    rv.closeup("se")
    (out / "closeup.png").rename(out / "closeup_se.png")
    rv.closeup("front", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    rv.closeup("ne", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_ne.png")
    rv.turnaround()
    rv.game_view()
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")


def anim_review(rig, meshes, attachments, clips, token, out: Path, height: float, outline_class: str,
                blob_radius: float = 0.22, glows=None, facing: str = "se", px: float | None = None) -> None:
    """Clip iteration: bake + one contact sheet per clip → ``out``."""
    out.mkdir(parents=True, exist_ok=True)
    for o, bone in attachments:
        R.attach_to_bone(o, rig, bone)
    objs = list(meshes) + [o for o, _ in attachments]
    rv = review.Review(rig.obj.name, rig.obj, objs, height, blob_radius=blob_radius, out_dir=out,
                       game_outline_px=px or outline.SCREEN_PX_1080[outline_class], outline_class=outline_class,
                       glows=glows)
    for c in clips:
        a = bake_clip(rig, c, token)
        rv.contact_sheet(rig, [(c, a)], frames=8, facing=facing)
        (out / "anims.png").rename(out / f"anim_{c.name}_{facing}.png")
        print(f"  anim_{c.name}_{facing}.png")


# ── verification (re-import what UE receives) ───────────────────────────────────────────────────────────
def record(rig, clips, actions) -> dict:
    _PARENTS.clear()
    _PARENTS.update({b.name: b.parent.name for b in rig.obj.data.bones if b.parent is not None})
    rec = {}
    for c, a in zip(clips, actions):
        anim.assign(rig, a)
        for f in sorted({0, c.frames // 2, c.frames}):
            anim.set_frame(f)
            rec[(c.name, f)] = {pb.name: pb.matrix.copy() for pb in rig.obj.pose.bones}
    anim.assign(rig, None)
    anim.clear_pose(rig)
    return rec


def verify_export(ms: MonsterShip, sk_fbx: Path, src: dict, entry: dict) -> dict:
    """Re-import the SK, every clip and every item into an empty scene and check bones, slots, weights,
    AF_Data / custom normals, bounds, frame counts and posed bones against the source."""
    fails = []

    def check(ok, msg):
        print(("  ok   " if ok else "  FAIL ") + msg)
        if not ok:
            fails.append(msg)
    print(f"\n[verify] {ms.asset}: re-import into an empty scene")
    nslots = len(entry["materialSlots"])
    want_slots = [s["name"] for s in entry["materialSlots"]]
    scene.reset()
    bpy.ops.import_scene.fbx(filepath=str(sk_fbx))
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    check(len(arms) == 1 and len(meshes) == 1, f"one armature + one mesh ({len(arms)}, {len(meshes)})")
    arm, me = arms[0], meshes[0]
    bones = {b.name for b in arm.data.bones}
    check(len(bones) == entry["bones"], f"{len(bones)} bones == {entry['bones']}")
    for need in ms.skeleton_bones_required:
        check(need in bones, f"bone {need}")
    check("Armature" not in bones, "no extra root bone")
    mats = [m.name.split(".")[0] for m in me.data.materials]
    check(mats == want_slots and len(mats) == nslots, f"material slots {mats}")
    st = R.weight_stats(me)
    check(st["unweighted"] == 0 and st["maxInfluences"] <= 4, f"weights ({st['maxInfluences']} max, "
                                                                f"{st['unweighted']} unweighted)")
    lo, hi = scene.world_bounds([me])
    hb = entry["boundsCm"]
    check(abs(hi.z * 100 - hb["max"][2]) < 1.0 and abs(lo.z * 100 - hb["min"][2]) < 1.0,
          f"bind-pose bounds z {lo.z:.3f} .. {hi.z:.3f} m == manifest")
    check("AF_Data" in me.data.color_attributes and me.data.has_custom_normals, "AF_Data colours + custom normals")
    worst_mm, worst_deg = 0.0, 0.0
    for c in ms.clips:
        f = paths.category_dir(ms.category) / f"A_{ms.token}_{c.name}.fbx"
        before = set(bpy.data.actions)
        objs = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(f))
        new = [a for a in bpy.data.actions if a not in before]
        if len(new) != 1:
            check(False, f"{f.name}: one action")
            continue
        a = new[0]
        n = int(round(a.frame_range[1] - a.frame_range[0]))
        check(n == max(1, c.frames), f"{f.name}: {n} frames == {c.frames}")
        arm2 = next(o for o in bpy.data.objects if o not in objs and o.type == "ARMATURE")
        if getattr(c, "basis_fn", None) is not None:
            # bone-scale clips (the slime): Blender's FBX importer takes the first frame's Lcl Scaling as a rest
            # it cannot store and divides it out, so the curves themselves are compared (what UE reads)
            mm, dg = _curve_check(f, {fr: m for (cn, fr), m in src.items() if cn == c.name})
            worst_mm, worst_deg = max(worst_mm, mm), max(worst_deg, dg)
            bpy.data.objects.remove(arm2)
            continue
        for (cn, fr), mats_ in src.items():
            if cn != c.name:
                continue
            bpy.context.scene.frame_set(int(a.frame_range[0]) + fr)
            for bn, m in mats_.items():
                pb = arm2.pose.bones.get(bn)
                if pb is None:
                    continue
                worst_mm = max(worst_mm, (pb.matrix.translation - m.translation).length * 1000)
                q0, q1 = m.to_quaternion(), pb.matrix.to_quaternion()
                ang = q0.rotation_difference(q1).angle
                worst_deg = max(worst_deg, math.degrees(min(ang, 2 * math.pi - ang)))
                s0, s1 = m.to_scale(), pb.matrix.to_scale()
                worst_mm = max(worst_mm, max(abs(s0[i] - s1[i]) for i in range(3)) * 1000)
        bpy.data.objects.remove(arm2)
    check(worst_mm < 1.0 and worst_deg < 0.1, f"re-imported clips match the source (max {worst_mm:.3f} mm "
                                              f"(scale ×1000), {worst_deg:.4f} deg)")
    for it in ms.items:
        objs = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(paths.category_dir(it.category) / f"{it.asset}.fbx"))
        new = [o for o in bpy.data.objects if o not in objs]
        names = sorted(o.name for o in new)
        mesh = next(o for o in new if o.type == "MESH")
        mn = [m.name.split(".")[0] for m in mesh.data.materials]
        check(mn[0] == ms.pal.material_name and (len(mn) == 1 or mn[1] == "M_AF_Outline"),
              f"{it.asset}: material slots {mn}")
        if it.sockets:
            check(all(f"SOCKET_{s['name']}" in names for s in it.sockets),
                  f"{it.asset}: sockets {[x for x in names if x.startswith('SOCKET_')]}")
        mw = mesh.matrix_world
        check(all(abs(mw[i][j] - (i == j)) < 1e-5 for i in range(4) for j in range(4)),
              f"{it.asset}: exported in its own space (node transform identity)")
        for s in it.sockets:
            so = next((x for x in new if x.name == f"SOCKET_{s['name']}"), None)
            if so is not None:
                d = (so.matrix_world.translation - Vector(s["pos"])).length * 100.0
                check(d < 0.5, f"{it.asset}: SOCKET_{s['name']} at the manifest location ({d:.2f} cm)")
    print(f"[verify] {'PASS' if not fails else 'FAIL'} ({len(fails)} failures)")
    return {"pass": not fails, "failures": fails}


def verify_lod(sk_fbx: Path, sk1_fbx: Path, entry: dict) -> dict:
    """Re-import ``SK_*_LOD1.fbx`` next to LOD0: one armature + one mesh, the same bone set and bind pose, the same
    material slots, weights, triangles == manifest, AF_Data + custom normals."""
    fails = []

    def check(ok, msg):
        print(("  ok   " if ok else "  FAIL ") + msg)
        if not ok:
            fails.append(msg)
    print(f"\n[verify] {sk1_fbx.name}: re-import next to LOD0")
    scene.reset()
    bpy.ops.import_scene.fbx(filepath=str(sk_fbx))
    a0 = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    rest0 = {b.name: b.matrix_local.copy() for b in a0.data.bones}
    objs = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(sk1_fbx))
    new = [o for o in bpy.data.objects if o not in objs]
    arms = [o for o in new if o.type == "ARMATURE"]
    meshes = [o for o in new if o.type == "MESH"]
    check(len(arms) == 1 and len(meshes) == 1, f"LOD1: one armature + one mesh ({len(arms)}, {len(meshes)})")
    if arms and meshes:
        a1, m1 = arms[0], meshes[0]
        check({b.name for b in a1.data.bones} == set(rest0), "LOD1: same bone set as LOD0")
        dev = max((a1.data.bones[n].matrix_local.translation - rest0[n].translation).length
                  for n in rest0 if n in a1.data.bones)
        check(dev < 1e-4, f"LOD1: same bind pose ({dev * 1000:.3f} mm)")
        mats = [m.name.split(".")[0] for m in m1.data.materials]
        check(mats == [x["name"] for x in entry["materialSlots"]], f"LOD1: material slots {mats}")
        st = R.weight_stats(m1)
        check(st["unweighted"] == 0 and st["maxInfluences"] <= 4, "LOD1: weights")
        want = entry["lods"][0]["triangles"]
        got = {"toon": toon_tris(m1), "outline": M.tri_count(m1, 1)}
        check(got["toon"] == want["toon"] and got["outline"] == want["outline"], f"LOD1: triangles {got} == manifest")
        check("AF_Data" in m1.data.color_attributes and m1.data.has_custom_normals, "LOD1: AF_Data + custom normals")
    print(f"[verify] LOD1 {'PASS' if not fails else 'FAIL'} ({len(fails)} failures)")
    return {"pass": not fails, "failures": fails}


def fbx_local_curves(path: Path) -> dict:
    """{bone: {'Lcl Translation' | 'Lcl Rotation' | 'Lcl Scaling': [(x, y, z) per key]}} straight from the FBX
    curves (parent-relative, translation in metres as exported with FBX_SCALE_ALL, Euler XYZ degrees)."""
    from io_scene_fbx import parse_fbx
    root, _ = parse_fbx.parse(str(path))

    def sub(e, name):
        return [c for c in e.elems if c.id == name]
    models, nodes, curves = {}, {}, {}
    for e in sub(root, b"Objects")[0].elems:
        uid = e.props[0]
        nm = e.props[1].split(b"\x00")[0].decode() if len(e.props) > 1 and isinstance(e.props[1], bytes) else ""
        if e.id == b"Model":
            models[uid] = nm
        elif e.id == b"AnimationCurveNode":
            nodes[uid] = nm
        elif e.id == b"AnimationCurve":
            curves[uid] = list(sub(e, b"KeyValueFloat")[0].props[0])
    node_of, axes = {}, {}
    for c in sub(root, b"Connections")[0].elems:
        p = c.props
        if p[0] == b"OP" and p[1] in nodes and p[2] in models:
            node_of[p[1]] = (models[p[2]], p[3].decode())
        elif p[0] == b"OP" and p[1] in curves and p[2] in nodes:
            axes.setdefault(p[2], {})[p[3].decode()] = curves[p[1]]
    out = {}
    for nid, (bone, prop) in node_of.items():
        a = axes.get(nid, {})
        if all(k in a for k in ("d|X", "d|Y", "d|Z")):
            out.setdefault(bone, {})[prop] = list(zip(a["d|X"], a["d|Y"], a["d|Z"]))
    return out


def _curve_check(path: Path, frames: dict) -> tuple[float, float]:
    """Worst translation (mm, and scale ×1000) / rotation (deg) difference between the FBX curves of every
    non-root bone and the recorded source pose (armature-space matrices per frame)."""
    cv = fbx_local_curves(path)
    worst_mm, worst_deg = 0.0, 0.0
    for fr, mats_ in frames.items():
        for bn, m in mats_.items():
            pn = _PARENTS.get(bn)
            if pn is None or bn not in cv:
                continue
            loc = mats_[pn].inverted() @ m
            t, q, sc = loc.decompose()
            ch = cv[bn]
            if fr >= len(ch.get("Lcl Scaling", [])):
                worst_mm = max(worst_mm, 1e6)
                continue
            ft = Vector(ch["Lcl Translation"][fr])         # metres (FBX_SCALE_ALL bakes the unit scale)
            fe = Vector([math.radians(x) for x in ch["Lcl Rotation"][fr]])
            fq = Euler(fe, "XYZ").to_quaternion()
            fs = ch["Lcl Scaling"][fr]
            worst_mm = max(worst_mm, (ft - t).length * 1000.0, max(abs(fs[i] - sc[i]) for i in range(3)) * 1000.0)
            ang = q.rotation_difference(fq).angle
            worst_deg = max(worst_deg, math.degrees(min(ang, 2 * math.pi - ang)))
    return worst_mm, worst_deg


_PARENTS: dict = {}


def main_result(res: dict) -> int:
    out = {k: v for k, v in res.items() if k not in ("previews", "qa", "ink")}
    if res.get("ink"):
        out["ink"] = {k: v for k, v in res["ink"].items() if k not in ("baked",)}
    out["qa"] = {k: v for k, v in res["qa"].items() if k != "worst"}
    print(json.dumps(out, indent=1)[:3000])
    ok = res.get("verify", {"pass": True})["pass"] and res["qa"]["pass"]
    ink = res.get("ink")
    if ink is not None and not ink["pass"]:
        print("\nINK GATE FAILED (review.INK_GATE, see game_inkgate.png):\n  " + "\n  ".join(ink["failures"]))
        ok = False
    return 0 if ok else 1
