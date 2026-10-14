"""Monster family R2 scale lineup — ``Art/Previews/mon_family/lineup_game.png`` (+ ``lineup_inkgate.png``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/lineup.py

The shipped warrior beside the four Chapter-1 monsters and a quest-hunt leader, every one **re-imported from its
FBX** (SK + ``Idle`` F0 + held items / attachments) with the shipped palettes — the R2 heights side by side at the
W1 camera (yaw 45°, pitch −50°, default distance), a native 1080p 1:1 crop, each figure in its se 3/4 view:

    warrior 1.76 m · chief 1.17 m (horns 1.37) · shaman 0.95 m · goblin 0.92 m · hunt leader (goblin × 1.25 + red
    cap + SM_FX_EliteCrown on fx_overhead, camera-yawed, absolute scale) · slime 0.43 × 0.88 m

Every figure is drawn with its shipped screen ink (``outlinePx1080`` of its manifest outline slot: hero / boss 3.5,
the goblin / shaman / slime's per-asset 3.5, monster 3.0) and **ink-gated** on its own coverage
(``review.ink_stats`` at that request − 0.5 px; the crown with the monster gate); the emissive regions bloom like UE. Exit 1 on a gate failure.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "environment", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import outline, paths, pngio, review, scene  # noqa: E402
from kit.paths import scratch_root  # noqa: E402

import envkit as EK  # noqa: E402

OUT_DIR = "mon_family"
FAMILY = "Monsters_Plains"
SPACING = 1.15                 # m between figures along the screen's horizontal


def _manifest() -> dict:
    return json.loads((paths.export_root() / "manifest.json").read_text())


def _dither_material(mt, opacity: float):
    """``MI_AF_Toon_Slime`` preview from the file-based toon material (as ``slime.dither_material``): the toon,
    dithered to ``opacity`` (TAA-resolved mix), back faces culled."""
    m = mt.copy()
    m.name = "AF_Files_Slime_Shell"
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
    m["af_toon_slot"] = True             # a toon slot for the emissive / coverage passes (no bloom of the shell)
    return m


def _import(fbx: Path) -> list:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(fbx))
    return [o for o in bpy.data.objects if o not in before]


def _static(fbx: Path, mats, cls: str, width_cm: float):
    new = _import(fbx)
    m = next(o for o in new if o.type == "MESH")
    for o in new:
        if o is not m:
            bpy.data.objects.remove(o)
    EK._set_slots(m, *mats)
    m["af_outline_width"] = width_cm / 100.0
    m["af_outline_class"] = cls
    return m


def import_monster(man: dict, asset: str, at: Vector, yaw_deg: float, scale: float = 1.0, attach=None):
    """A shipped monster: SK + ``Idle`` F0 (copy-transforms from the clip's armature), held items on their bones,
    optional variant attachments ``[(asset, bone)]``. Returns (root, armature, meshes, px1080, class)."""
    e = man["assets"][asset]
    root_dir = paths.export_root()
    mt, mo = EK.toon_material_files(FAMILY)
    slot = e["materialSlots"][1]
    cls, px, width = slot.get("class", "monster"), slot.get("outlinePx1080", 3.0), slot.get("widthCm", 1.5)
    new = _import(root_dir / e["fbx"])
    arm = next(o for o in new if o.type == "ARMATURE")
    meshes = [o for o in new if o.type == "MESH"]
    idle = next(a for a in e["anims"] if a["fbx"].endswith("_Idle.fbx"))
    new2 = _import(root_dir / idle["fbx"])
    arm2 = next(o for o in new2 if o.type == "ARMATURE")
    for pb in arm.pose.bones:
        c = pb.constraints.new("COPY_TRANSFORMS")
        c.target, c.subtarget = arm2, pb.name
        c.target_space = c.owner_space = "POSE"
    for o in new2:
        o.hide_render = True
    act = arm2.animation_data.action if arm2.animation_data else None
    if act is not None:
        bpy.context.scene.frame_set(int(act.frame_range[0]))
    shell = None
    for m in meshes:
        # slots by the imported material names, back in the manifest order (0 toon, 1 outline hull, 2 the slime's
        # dithered shell): the FBX importer need not keep it, and the coverage pass reads slot 1 as the hull
        mats = m.data.materials
        names = [im.name if im is not None else "" for im in mats]
        want = {i: (1 if "Outline" in nm else 2 if "Slime" in nm else 0) for i, nm in enumerate(names)}
        mi = np.zeros(len(m.data.polygons), np.int32)
        m.data.polygons.foreach_get("material_index", mi)
        m.data.polygons.foreach_set("material_index", np.array([want[int(k)] for k in mi], np.int32))
        n_slots = max(want.values()) + 1
        new = [mt, mo]
        if n_slots > 2:
            op = next((x.get("params", {}).get("BodyOpacity", 0.62) for x in e["materialSlots"]
                       if x["name"] == "MI_AF_Toon_Slime"), 0.62)
            shell = shell or _dither_material(mt, op)
            new.append(shell)
        while len(mats) < n_slots:
            mats.append(None)
        for i in range(len(mats)):
            mats[i] = new[i] if i < len(new) else mt
        m.data.update()
        m["af_outline_width"] = width / 100.0
        m["af_outline_class"] = cls
    from types import SimpleNamespace
    from kit import rig as krig
    rig = SimpleNamespace(obj=arm)
    for att in e.get("attachments", []) + [{"object": a, "socket": b} for a, b in (attach or [])]:
        ae = man["assets"][att["object"]]
        s1 = ae["materialSlots"][1]
        im = _static(root_dir / ae["fbx"], (mt, mo), s1.get("class", "weapon"), s1.get("widthCm", 1.3))
        krig.attach_to_bone(im, rig, att["socket"])
        im["af_px1080"] = s1.get("outlinePx1080", px)
        meshes.append(im)
    root = bpy.data.objects.new(f"Root_{asset}", None)
    scene.link(root)
    arm.parent = root
    arm2.parent = root
    root.location = at
    root.rotation_euler = (0.0, 0.0, math.radians(yaw_deg))
    root.scale = (scale, scale, scale)
    bpy.context.view_layer.update()
    return root, arm, meshes, px, cls


def _fx_off(meshes, family: str) -> list:
    """Swap each mesh's data for a copy without its fully emissive toon faces (palette swatches with e ≥ 0.99:
    the spirit-fire tongues, embers, glints). Returns [(mesh, original data)] for the meshes that had any."""
    import bmesh
    reg = json.loads((paths.palette_root() / f"{family}.json").read_text())
    emissive = {sw["i"] for sw in reg["swatches"] if sw and float(sw.get("e", 0.0)) >= 0.99}
    out = []
    for m in meshes:
        if m.type != "MESH" or not m.data.uv_layers:
            continue
        me = m.data
        uv = me.uv_layers.active.data
        bad = []
        for poly in me.polygons:
            if poly.material_index == 1:
                continue
            u, v = uv[poly.loop_start].uv
            idx = int(min(15, max(0, (1.0 - v) * 16))) * 16 + int(min(15, max(0, u * 16)))
            if idx in emissive:
                bad.append(poly.index)
        if not bad:
            continue
        cp = me.copy()
        bm = bmesh.new()
        bm.from_mesh(cp)
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[bm.faces[i] for i in bad], context="FACES")
        bm.to_mesh(cp)
        bm.free()
        out.append((m, me))
        m.data = cp
    return out


def socket_world(man: dict, asset: str, arm, name: str) -> Vector:
    """A manifest socket on the posed imported armature (rest location UE cm → Blender m, through the bone)."""
    s = next(x for x in man["assets"][asset]["sockets"] if x["name"] == name)
    rest = Vector((s["restLocCm"][0] / 100.0, -s["restLocCm"][1] / 100.0, s["restLocCm"][2] / 100.0))
    b = arm.data.bones[s["bone"]]
    pb = arm.pose.bones[s["bone"]]
    return arm.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ rest))


def main() -> int:
    scene.reset()
    man = _manifest()
    review.stage(radius=40.0, blob_radius=0.01)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera((0.0, 0.0))
    right = (cam.matrix_world.to_3x3() @ Vector((1, 0, 0)))
    right.z = 0.0
    right.normalize()
    se = review.FACINGS["se"]
    figs = []                                          # (label, meshes, px, class)
    order = ["warrior", "SK_Mon_GoblinChief", "SK_Mon_GoblinShaman", "SK_Mon_Goblin", "hunt", "SK_Mon_Slime"]
    n = len(order)
    crown = None
    for i, key in enumerate(order):
        at = right * ((i - (n - 1) / 2) * SPACING)
        if key == "warrior":
            w = EK.import_warrior(at=tuple(at), yaw_deg=se)
            figs.append(("WARRIOR 1.76M", w[1], outline.SCREEN_PX_1080["hero"], "hero"))
            continue
        asset = "SK_Mon_Goblin" if key == "hunt" else key
        scale = 1.25 if key == "hunt" else 1.0
        attach = [("SM_Mon_Goblin_Att_RedCap", "head")] if key == "hunt" else None
        root, arm, meshes, px, cls = import_monster(man, asset, at, se, scale, attach)
        h = man["assets"][asset].get("heightCm", 0) * scale / 100.0
        label = {"SK_Mon_GoblinChief": "CHIEF", "SK_Mon_GoblinShaman": "SHAMAN", "SK_Mon_Goblin": "GOBLIN",
                 "SK_Mon_Slime": "SLIME"}.get(asset, asset)
        if key == "hunt":
            label = "HUNT LEADER X1.25"
            ce = man["assets"]["SM_FX_EliteCrown"]
            mt, mo = EK.toon_material_files(FAMILY)
            crown = _static(paths.export_root() / ce["fbx"], (mt, mo), "monster", ce["materialSlots"][1].get("widthCm", 1.5))
            wp = socket_world(man, asset, arm, "fx_overhead")
            dv = cam.matrix_world.translation - wp
            crown.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.atan2(dv.x, -dv.y), 4, "Z")
        figs.append((f"{label} {h:.2f}M", meshes, px, cls))
    bpy.context.view_layer.update()
    # shipped ink per figure (WPO preview), the crown with the monster ink
    for _, ms, px, _ in figs:
        for m in ms:
            review.set_outline_px([m], m.get("af_px1080", px))
    if crown is not None:
        review.set_outline_px([crown], outline.SCREEN_PX_1080["monster"])
    every = [m for _, ms, _, _ in figs for m in ms] + ([crown] if crown is not None else [])
    pts = review.mesh_points(every, step=4)
    proj = [review.project(cam, Vector(p)) for p in pts]
    xs, ys = [c[0] for c in proj], [c[1] for c in proj]
    pad_x, pad_y = 40 / 1920, 40 / 1080
    box = (max(0.0, min(xs) - pad_x), max(0.0, min(ys) - pad_y), min(1.0, max(xs) + pad_x),
           min(1.0, max(ys) + pad_y + 20 / 1080))
    raw = review.render_array(box)
    if "--debug" in sys.argv:
        pngio.write_png(scratch_root("mq") / "lineup_raw.png", raw)
    em = review.emissive_pass(every, box)
    img = review.bloom(raw, em, 1080)
    # per-figure ink gate on its own coverage (figures stand apart: no mutual occlusion)
    inkimg = img.copy()
    reports, ok = [], True
    groups = [(lab, ms, cls, px) for lab, ms, px, cls in figs] + (
        [("CROWN", [crown], "monster", outline.SCREEN_PX_1080["monster"])] if crown else [])
    for lab, ms, cls, px in groups:
        # the gate of the figure's shipped request (manifest outlinePx1080 − GATE_SLACK_PX): a monster shipping a
        # per-asset 3.5 px hull is gated at 3.0 px, like the boss
        gate = dict(review.ink_gate("boss" if cls == "boss" else cls))
        gate["minPx"] = round(max(0.5, float(px) - outline.GATE_SLACK_PX), 2)
        # only this figure in the coverage pass (coverage_pass swaps the slots of the meshes it is given; anything
        # else visible would render in its toon colours into the red / green coverage channels)
        others = [o for o in every if o not in ms and not o.hide_render]
        for o in others:
            o.hide_render = True
        # runtime-lit FX parts (the shaman's spirit-fire: unlit emissive, no ink by spec, bloomed) are off for the
        # measurement, as in the ship gates (mship.FxOff): the figure is re-rendered without them
        swapped = _fx_off(ms, FAMILY) if cls != "hero" else []        # (the warrior is on the Heroes palette)
        g_img, g_raw = img, raw
        try:
            if swapped:
                g_raw = review.render_array(box)
                g_img = review.bloom(g_raw, review.emissive_pass(ms, box), 1080)
            body, hull = review.coverage_pass(ms, box, ground=True, extra_body=True)
            b0, h0 = review.coverage_pass(ms, box, ground=False, extra_body=True)
        finally:
            for o in others:
                o.hide_render = False
            for m, data in swapped:
                m.data = data
        # hull under the floor (planted soles) is the terrain's, as in the ship gates (mship.MReview)
        buried = (b0 + h0) - (body + hull) > 0.25
        for _ in range(2):
            pb_ = np.pad(buried, 1, constant_values=False)
            buried = buried | pb_[:-2, 1:-1] | pb_[2:, 1:-1] | pb_[1:-1, :-2] | pb_[1:-1, 2:]
        st = review.ink_stats(body, hull, g_img, min_px=gate["minPx"], dark_lum=gate["darkLum"],
                              within=gate["darkWithinPx"], hull_rgb=g_raw, ignore=buried)
        st["gate"] = gate
        if "--debug" in sys.argv:
            dbg = np.zeros(img.shape, np.uint8)
            dbg[..., 0] = (body * 255).astype(np.uint8)
            dbg[..., 1] = (hull * 255).astype(np.uint8)
            dbg[st["edge_yx"]] = (255, 255, 255)
            pngio.write_png(scratch_root("mq") / f"cov_{lab.split()[0]}.png", dbg)
        good = review.ink_gate_ok(st, gate)
        ok = ok and good
        reports.append((lab, st, good))
        ys_, xs_ = st["edge_yx"]
        inkimg[ys_[st["edge_ok"]], xs_[st["edge_ok"]]] = (60, 230, 90)
        inkimg[ys_[~st["edge_ok"]], xs_[~st["edge_ok"]]] = (240, 60, 60)
        print(f"[ink] {lab:22s} {cls:8s} rim >= {gate['minPx']:g}px {st['fracRim'] * 100:5.1f} %  dark "
              f"{st.get('fracDark', 1) * 100:5.1f} %  median {st.get('rimPxMedian', 0):.2f}px -> "
              f"{'PASS' if good else 'FAIL'}")
    # labels over each figure
    x0, y0 = review.border_origin(box, (1920, 1080))
    for (lab, ms, _, _), (_, st, good) in zip(figs, reports):
        p = review.mesh_points(ms)
        c = review.project_px(cam, Vector(p.mean(0)), (1920, 1080))
        review.draw_text(img, max(2, int(c[0] - x0 - len(lab) * 3)), img.shape[0] - 26, lab)
        tag = f"RIM {st['fracRim'] * 100:.0f}%"
        review.draw_text(img, max(2, int(c[0] - x0 - len(tag) * 3)), img.shape[0] - 14, tag,
                         rgb=(160, 230, 160) if good else (240, 90, 90))
    review.draw_text(img, 6, 6, "R2 LINEUP  W1 CAMERA 1080P 1:1  SHIPPED FBX + INK + BLOOM  (SE 3/4, IDLE F0)")
    review.draw_text(inkimg, 6, 6, "INK GATE PER FIGURE (GREEN = RIM >= CLASS GATE, RED = THINNER)")
    out = paths.preview_root() / OUT_DIR
    out.mkdir(parents=True, exist_ok=True)
    for f, im in (("lineup_game.png", img), ("lineup_inkgate.png", inkimg)):
        n_ = pngio.write_png_budget(out / f, im)
        print(f"  {out / f}  {im.shape[1]}x{im.shape[0]}  {n_ // 1024} KB")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
