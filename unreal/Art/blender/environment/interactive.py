"""Interactive props and world markers (spec art-inventory-ch1.md §6.8; web ``TreasureChest.ts``, ``GoldPile.ts``,
``EventProps.ts``, ``ExitPortal.ts``, ``QuestWorld.ts`` markers; DECISIONS W7 sealed chapter-2 gate, R7).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/interactive.py [names…]
        [--sheet] [--no-previews]

| Asset | game ids | look / behaviour |
|---|---|---|
| ``SK_Prop_TreasureChest`` | ``decor_treasure_chest``, ``treasure_chest`` | ≈ 0.65 × 0.5 m chest ``#8A4E2A`` with brass straps / rim ``#E0AA3A``, lock ``#F0C050`` (keyhole ``#3A1E0C``), an ember-gold seam ``#FFE080`` (focal accent), gold + a ``#FF5A7A`` gem inside; skeleton ``SKEL_Prop_Chest`` (``root``, ``lid``), clips ``Idle`` (1000 ms loop, a restless rattle) and ``Open`` (400 ms, notify ``ChestOpen`` → NS_World_ChestOpen) |
| ``SM_Prop_GoldPile`` | ``decor_gold_pile``, ``gold_pile`` | coin mound ≈ 0.53 × 0.28 m ``#F0B83A`` with spilled coins and a gem |
| ``SM_Prop_Event_RunePillar`` | ``decor_event_puzzle_rune_pillar`` | base ``#5A6664`` 0.7 × 0.2 m, tapered pillar ``#6F7C7A`` to 1.48 m, zig-zag rune ``#2A9AD8`` / core ``#DFF8FF`` |
| ``SM_Prop_ExitPortal`` | ``exit_portal`` | 2.6 m elven stone frame round a teal vortex disc (``#3FE0A0`` → ``#1A6A8A``, swirl strokes), two 1.2 m rune stones ``#6A7A8A``, a ground ring of rune marks; NS_World_ExitPortal adds the motion |
| ``SM_Prop_ExitGate_Sealed`` | ``exit_gate_sealed``, ``exit_sealed`` | W7: the same frame barricaded — nailed planks, an iron chain and padlock across a dormant, crimson-sealed vortex ``#6A2230``, a warning plank; "第二章即将开放" is UI |
| ``SM_FX_ClueMagnifier`` | ``clue_mark``, ``fx_clue`` | floating magnifying glass: brass rim ``#E0B04A``, lens ``#E9FBFF`` → ``#7FC6E0`` + glint, handle ``#7A4A22`` |
| ``SM_FX_GuideArrow`` | ``guide_arrow`` | flat gold chevron ``#FFF2B8`` / ``#FFCF4A`` / ``#D88A1A``, ink ``#2A1606``, points along the mesh forward (−Y) |
| ``SM_Prop_Gather_EmeraldHerb`` | ``gather_herb`` | ≈ 30 cm sprig of 翡翠草药: leaves ``#5FB84A`` with glowing veins ``#9FE07A`` |
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit as E  # noqa: E402
import foliage as F  # noqa: E402
import story as ST  # noqa: E402
import bmesh  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import export, outline, paths, review, scene, shading  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


REGIONS = dict(
    wood=Region("#8A4E2A", .40, .32),
    wood_line=Region("#8A4E2A").shade_region(.30, .12),
    wood_in=Region("#3A1E10", .30, .10),
    brass=Region("#E0AA3A", .42, .50),
    lock=Region("#F0C050", .40, .50),
    keyhole=Region("#3A1E0C", .30, .10),
    seam=Region("#FFE080", .0, .0, e=0.85),
    gold=Region("#F0B83A", .35, .50),
    gold_hi=Region("#FFE89A", .25, .40),
    gem=Region("#FF5A7A", .30, .40, e=0.35),
    gem_core=Region("#FFD0DC", .0, .0, e=0.8),
    pillar_base=Region("#5A6664", .45, .35),
    pillar=Region("#6F7C7A", .42, .35),
    rune_blue=Region("#2A9AD8", .0, .0, e=1.0),
    rune_core=Region("#DFF8FF", .0, .0, e=1.0),
    frame=Region("#8E96A0", .45, .38),
    frame_lt=Region("#A2AAB2", .45, .40),
    frame_line=Region("#8E96A0").line_region(),
    runestone=Region("#6A7A8A", .42, .35),
    vortex_deep=Region("#10304A", .20, .10, e=0.55),
    vortex_rim=Region("#0A2A3A", .30, .20, o=.3),
    sealed_rim=Region("#3A1018", .30, .20, o=.3),
    vortex_mid=Region("#1A6A8A", .10, .10, e=0.9),
    vortex=Region("#3FE0A0", .0, .0, e=1.0),
    vortex_core=Region("#F0FFF8", .0, .0, e=1.0),
    swirl=Region("#DCFFF0", .0, .0, e=1.0),
    swirl_b=Region("#80F0FF", .0, .0, e=1.0),
    ring_mark=Region("#D8FFF0", .0, .0, e=1.0),
    sealed_deep=Region("#2A1018", .25, .10, e=0.25),
    sealed=Region("#6A2230", .20, .15, e=0.45),
    sealed_glyph=Region("#C04050", .0, .0, e=0.7),
    plank=Region("#8A6440", .42, .32),
    plank_line=Region("#8A6440").shade_region(.30, .12),
    iron=Region("#4A4E5A", .42, .45),
    nail=Region("#2E2A30", .40, .20),
    sign_paint=Region("#C8322E", .40, .30),
    brass_rim=Region("#E0B04A", .42, .50),
    lens=Region("#E9FBFF", .20, .25),
    lens_out=Region("#7FC6E0", .30, .30),
    glint=Region("#FFFFFF", .0, .0, e=0.6),
    handle=Region("#7A4A22", .42, .30),
    arrow_hi=Region("#FFF2B8", .25, .30),
    arrow=Region("#FFCF4A", .38, .40),
    arrow_dk=Region("#D88A1A", .40, .35),
    leaf=Region("#5FB84A", .40, .32),
    vein=Region("#9FE07A", .0, .0, e=0.9),
    bud=Region("#9FE07A", .30, .30, e=0.35, o=.3),
    stem=Region("#3F7A33", .40, .25),
    soil=Region("#7A5A3A", .42, .30),
    stone=Region("#7A7E88", .42, .35),
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def new(pal, name):
    b = M.Builder(name, pal, prefix="interact")
    b.regions(**REGIONS)
    return b, scene.rng(name)


def paint(b, region, pts, normal, width, pid, lift=0.002, thick=0.003):
    pts = [Vector(p) for p in pts]
    b.add(E.ribbon(pts, [Vector(normal)] * len(pts), width, lift=lift, thick=thick), region, name=region,
          outline=False, covered_by=pid)


# ── treasure chest (skeletal) ───────────────────────────────────────────────────────────────────────────
CW, CD, CH = 0.62, 0.44, 0.30          # body width (X), depth (Y), height
LID_R = CD / 2                          # barrel lid radius
HINGE = Vector((0, CD / 2, CH))         # back top edge


def rrect(ax: float, ay: float, rc: float, z: float, nx: int = 9, ny: int = 5) -> list[Vector]:
    """Points round a rounded rectangle (half sizes ``ax`` × ``ay``, corner radius ``rc``) at height ``z``,
    counter-clockwise: both ends of every corner arc (the corners stay square to the planks) and ``nx`` / ``ny``
    interior points on the sides along X / Y."""
    sx, sy = ax - rc, ay - rc
    corners = (((sx, sy), 0.0), ((-sx, sy), 0.5 * math.pi), ((-sx, -sy), math.pi), ((sx, -sy), 1.5 * math.pi))
    out = []
    for k, ((cx, cy), a0) in enumerate(corners):
        for t in (0.0, 1.0):
            ang = a0 + 0.5 * math.pi * t
            out.append(Vector((cx + rc * math.cos(ang), cy + rc * math.sin(ang), z)))
        p0 = out[-1]
        (nx2, ny2), _ = corners[(k + 1) % 4]
        a1 = corners[(k + 1) % 4][1]
        p1 = Vector((nx2 + rc * math.cos(a1), ny2 + rc * math.sin(a1), z))
        n = nx if k % 2 == 0 else ny
        for i in range(1, n + 1):
            out.append(p0.lerp(p1, i / (n + 1)))
    return out


def chest_body_proxy() -> M.Part:
    """Hull proxy of the chest body (art review 9): a dense closed tube hugging the planks (+1 mm) up to the
    lid line that **curls under** at the base (inset 3 mm at 3 cm, 13 mm at the floor) and widens
    back to the planks under the floor (hidden by the terrain; the ink gate reads that contact line as buried, like
    a character's soles). Under the W1 camera the ink beside a box's lower vertical edges can only come from a hull surface that
    faces up there: a vertical hull wall is edge-on and its bottom is under the terrain, so the old proxy (a plain
    box padded round straps, lock and seam) only inked them by growing out and under the floor — the dark line
    standing off the planks and the wedges at the base corners. ``hull_pad`` now grows the tube only where the rim
    band and the corner straps stand proud (nothing is padded below z = 0); the lock plate and the hasp carry their
    own small hulls."""
    curl = ((-0.045, -0.001), (-0.008, 0.013), (0.004, 0.013), (0.02, 0.006), (0.045, 0.001), (0.08, -0.001))
    zs = list(curl) + [(z, -0.001) for z in (CH - 0.075, CH - 0.05, CH - 0.03, CH - 0.01, CH + 0.001)]
    rings = [rrect(CW / 2 - inset, CD / 2 - inset, 0.006, z) for z, inset in zs]
    part = E.ring_loft(rings, cap0="fan", cap1="fan", smooth=True)
    bm = part.bm
    bm.normal_update()
    out = sum(f.normal.dot(f.calc_center_median() - Vector((0, 0, CH / 2))) for f in bm.faces)
    if out < 0:                        # the waist can fool recalc into the inward side
        for f in bm.faces:
            f.normal_flip()
        bm.normal_update()
    return part


def chest_lid_proxy() -> M.Part:
    """Hull proxy of the barrel lid: the same half cylinder (+1 mm) with 9 rings along the hinge axis and 13
    points round, so the padding round the brass bands stays local."""
    rings = []
    for k in range(9):
        x = -CW / 2 - 0.001 + (CW + 0.002) * k / 8
        ring = []
        for i in range(13):
            a = math.pi * i / 12
            r = LID_R + 0.001
            ring.append(Vector((x, -math.cos(a) * r, CH - 0.0005 + math.sin(a) * r * 0.72)))
        rings.append(ring)
    return E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=True)


def build_chest(pal: Palette):
    """Returns (mesh, binds {part id: 'root' | 'lid'})."""
    name = "SK_Prop_TreasureChest"
    b, rnd = new(pal, name)
    lid_parts = []
    body = M.box((CW, CD, CH), (0, 0, CH / 2), bevel=0.02)
    bid = b.add(body, "wood", name="body", hull=chest_body_proxy(), hull_pad=True)
    # dark interior (visible when open; its top flush with the body top) + the treasure: gold heap + coins + gem
    # (inside the closed lid: no ink, not padded into the body's hull)
    b.add(M.box((CW - 0.07, CD - 0.07, 0.02), (0, 0, CH - 0.0095), bevel=0.004), "wood_in", name="inside",
          outline=False, covered_by=bid)
    heap = M.ellipsoid((CW * 0.4, CD * 0.36, 0.07), (0, 0, CH), sides=12, rings=6)
    b.add(heap, "gold", name="heap", outline=False)
    for q in range(7):
        c = Vector((rnd.uniform(-0.2, 0.2), rnd.uniform(-0.12, 0.12), CH + 0.04 + rnd.uniform(0, 0.02)))
        coin = M.cylinder(0.035, 0.035, -0.004, 0.004, sides=10)
        coin.transform(_rx(rnd.uniform(-25, 25)) @ _ry(rnd.uniform(-25, 25))).translate(c)
        b.add(coin, "gold_hi", name="coin", outline=False)
    gem = M.gem(0.04, 0.05, 0.035, sides=6).translate((0.05, -0.02, CH + 0.07))
    b.add(gem, "gem", name="gem", outline=False)
    # plank seams (front/back/sides), brass corner straps + rim band, lock plate
    for z in (CH * 0.36, CH * 0.7):
        paint(b, "wood_line", [(-CW / 2 + 0.03, -CD / 2, z), (0, -CD / 2, z), (CW / 2 - 0.03, -CD / 2, z)], (0, -1, 0),
              0.012, bid)
        paint(b, "wood_line", [(-CW / 2 + 0.03, CD / 2, z), (0, CD / 2, z), (CW / 2 - 0.03, CD / 2, z)], (0, 1, 0),
              0.012, bid)
        for s in (-1, 1):
            paint(b, "wood_line", [(s * CW / 2, -CD / 2 + 0.03, z), (s * CW / 2, 0, z), (s * CW / 2, CD / 2 - 0.03, z)],
                  (s, 0, 0), 0.012, bid)
    rim = M.belt(CW / 2 - 0.004, CD / 2 - 0.004, CH - 0.05, 0.045, 0.012, sides=20, exp=9)
    b.add(rim, "brass", name="rim", outline=False, covered_by=bid)
    for sx in (-1, 1):
        for sy in (-1, 1):
            st = M.box((0.05, 0.05, CH - 0.06), (sx * (CW / 2 - 0.016), sy * (CD / 2 - 0.016), CH / 2 - 0.02),
                       bevel=0.006)
            b.add(st, "brass", name="strap", outline=False, covered_by=bid)
    plate = M.box((0.11, 0.02, 0.12), (0, -CD / 2 - 0.006, CH - 0.07), bevel=0.008)
    lock_id = b.add(plate, "lock", name="lock", hull_pad=True)
    hole = M.ellipsoid((0.013, 0.006, 0.024), (0, -CD / 2 - 0.016, CH - 0.08), sides=6, rings=3)
    b.add(hole, "keyhole", name="keyhole", outline=False, covered_by=lock_id)
    # barrel lid (half cylinder along X, hinged at the back top edge) + bands + the glowing seam
    rings = []
    for x in (-CW / 2, CW / 2):
        ring = []
        for i in range(9):
            a = math.pi * i / 8
            ring.append(Vector((x, -math.cos(a) * LID_R, CH + math.sin(a) * LID_R * 0.72)))
        rings.append(ring)
    lid = E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=True)
    lid.auto_sharp(50)
    lid_id = b.add(lid, "wood", name="lid", hull=chest_lid_proxy(), hull_pad=True)
    lid_parts.append(lid_id)
    for x in (-CW / 2 + 0.07, CW / 2 - 0.07, 0.0):
        band = []
        for i in range(9):
            a = math.pi * i / 8
            band.append(Vector((x, -math.cos(a) * (LID_R + 0.003), CH + math.sin(a) * (LID_R + 0.003) * 0.72)))
        nrm = [Vector((0, -math.cos(math.pi * i / 8), math.sin(math.pi * i / 8))) for i in range(9)]
        rb = E.ribbon(band, nrm, 0.05 if x else 0.04, lift=0.0, thick=0.005)
        lid_parts.append(b.add(rb, "brass", name="lid_band", outline=False, covered_by=lid_id))
    seam = M.box((CW - 0.12, 0.01, 0.016), (0, -CD / 2 - 0.002, CH - 0.006), bevel=0.003)
    b.add(seam, "seam", name="seam", outline=False)
    hasp = M.box((0.06, 0.02, 0.1), (0, -LID_R - 0.004, CH + 0.02), bevel=0.006)
    lid_parts.append(b.add(hasp, "lock", name="hasp", hull_pad=True))
    obj = E.finish(b, pal, "interactive", ground_h=0.4, name=name)
    return obj, b, lid_parts


def chest_rig(obj, b, lid_parts):
    """Two-bone skeleton ``root`` → ``lid`` (head on the hinge, pointing forward: local X = the hinge axis); the
    lid's parts (and their hull copies) are weighted to ``lid``, everything else to ``root``."""
    arm_data = bpy.data.armatures.new("SKEL_Prop_Chest")
    arm = bpy.data.objects.new("SK_Prop_TreasureChest_Rig", arm_data)
    scene.link(arm)
    scene.select_only([arm], active=arm)
    bpy.ops.object.mode_set(mode="EDIT")
    root = arm_data.edit_bones.new("root")
    root.head, root.tail = (0, 0, 0), (0, 0.15, 0)
    root.roll = 0.0
    lid = arm_data.edit_bones.new("lid")
    lid.head, lid.tail = HINGE, HINGE + Vector((0, -0.2, 0))
    lid.roll = 0.0
    lid.parent = root
    bpy.ops.object.mode_set(mode="OBJECT")
    for bn in arm_data.bones:
        bn.use_deform = True
    me = obj.data
    import numpy as np
    pid = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", pid)
    hull_src = {}
    for k, p in enumerate(b.parts):        # hull proxies of lid parts move with the lid
        if p.get("hullOnly"):
            base = p["name"][:-5]
            hull_src[k] = base
    names = [p["name"] for p in b.parts]
    lid_ids = set(lid_parts) | {k for k, base in hull_src.items() if any(names[i] == base for i in lid_parts)}
    on_lid = set()
    for poly in me.polygons:
        if int(pid[poly.index]) in lid_ids:
            on_lid.update(poly.vertices)
    vg_root = obj.vertex_groups.new(name="root")
    vg_lid = obj.vertex_groups.new(name="lid")
    lid_v = sorted(on_lid)
    root_v = [v.index for v in me.vertices if v.index not in on_lid]
    vg_lid.add(lid_v, 1.0, "REPLACE")
    vg_root.add(root_v, 1.0, "REPLACE")
    obj.parent = arm
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    return arm


CHEST_CLIPS = {
    # name: (length ms, loop, keys [(ms, lid angle deg, ease)], notifies)
    "Idle": (1000.0, True, [(0, 0.0), (90, 2.4), (170, 0.0), (260, 1.4), (340, 0.0), (1000, 0.0)], {}),
    "Open": (400.0, False, [(0, 0.0), (60, 4.0), (230, -88.0), (320, -112.0), (400, -105.0)], {"ChestOpen": 120.0}),
}


def _ease(t):
    return t * t * (3 - 2 * t)


def chest_action(arm, clip: str):
    length, loop, keys, notifies = CHEST_CLIPS[clip]
    act = bpy.data.actions.new(f"A_Prop_TreasureChest_{clip}")
    ad = arm.animation_data or arm.animation_data_create()
    ad.action = act
    pb = arm.pose.bones["lid"]
    pb.rotation_mode = "QUATERNION"
    n = scene.ms_to_frame(length)
    for f in range(n + 1):
        ms = scene.frame_to_ms(f)
        for (t0, a0), (t1, a1) in zip(keys, keys[1:]):
            if t0 <= ms <= t1:
                ang = a0 + (a1 - a0) * _ease((ms - t0) / max(t1 - t0, 1e-6))
                break
        else:
            ang = keys[-1][1]
        # the lid bone points forward (−Y): positive rotation about its local X lifts the front edge
        pb.rotation_quaternion = Matrix.Rotation(math.radians(-ang), 4, "X").to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=f)
        for bname in ("root",):
            rb = arm.pose.bones[bname]
            rb.rotation_mode = "QUATERNION"
            rb.keyframe_insert("rotation_quaternion", frame=f)
            rb.keyframe_insert("location", frame=f)
        pb.keyframe_insert("location", frame=f)
    act.frame_range = (0, n)
    clip_ns = SimpleNamespace(name=clip, length_ms=length, frames=n, loop=loop, notifies=dict(notifies),
                              additive=False, ref_speed=None,
                              notify_list=lambda nf=notifies: sorted([{"name": k, "ms": float(v)} for k, v in nf.items()],
                                                                     key=lambda d: d["ms"]))
    return act, clip_ns


def rest_pose(arm) -> None:
    """Closed rest pose: no action evaluating (``export_animation`` leaves the last exported clip assigned — the
    Open end frame showed in every review image and in ``boundsCm`` before art review 9), identity bones."""
    ad = arm.animation_data
    if ad is not None:
        ad.action = None
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
        pb.scale = (1, 1, 1)
    bpy.context.scene.frame_set(0)
    bpy.context.view_layer.update()


def open_pose(rig, act, clip) -> None:
    from kit import anim
    anim.assign(rig, act)
    anim.set_frame(clip.frames)
    bpy.context.view_layer.update()


def chest_open_views(rv, rig, act, clip) -> None:
    """The Open end frame as its own gated 1080p cell (shipped ink + baked hull alone) and a close-up."""
    open_pose(rig, act, clip)
    rv.facing("front")
    review.setup_render((1920, 1080), rv.samples)
    review.hide_stage(False)
    cam = review.game_camera()
    box = rv.game_crop_box(cam)
    rv.game_outline(1080)
    ship, st = rv.game_shot(box, measure=True, tag="game_open_1080crop front shipped (Open end frame)")
    rv.game_outline(1080, baked=True)
    base, st_b = rv.game_shot(box, measure=True, tag="game_open_1080crop front baked (Open end frame)", glow=False,
                              bloom_on=False)
    st_b["gated"] = False
    rv.game_outline(1080)
    review.draw_text(ship, 4, 4, f"OPEN END F{clip.frames}  1080P 1:1 SHIPPED  INK {rv.game_outline_px:g}PX + BLOOM")
    review.draw_text(ship, 4, 14, rv.ink_label(st), rgb=(255, 220, 140))
    review.draw_text(base, 4, 4, "OPEN END  BAKED 1.3CM ONLY (MOBILE LOW)")
    review.draw_text(base, 4, 14, f"RIM MEDIAN {st_b['rimPxMedian']:.1f}PX P10 {st_b['rimPxP10']:.1f}PX",
                     rgb=(255, 220, 140))
    rv._save("game_open_1080crop.png", review.grid([ship, base], 2, pad=2))
    res = (480, 600)
    review.setup_render(res, rv.samples)
    pts = review.mesh_points(rv.meshes)
    review.frame_points(pts, res, fov_h=28.0, pitch_ue=-22.0, fill=0.9)
    rv.ink(at=Vector(pts.mean(0)))
    img = review.render_array()
    review.draw_text(img, 6, 6, f"{rv.asset}  OPEN END F{clip.frames}  FRONT 3/4")
    rv._save("closeup_open.png", img)


def chest_sheet(rv, rig, acts, frames: int = 8, cell=(220, 196)) -> None:
    """Contact sheets (``anims.png`` + ``anim_<Clip>.png``) framed on every sampled frame of every clip with room
    to spare (fill .8: the kit sheet's .95 clipped the chest at the cell edge)."""
    from kit import anim
    rv.facing("se")
    plan, allpts = [], []
    for act, clip in acts:
        anim.assign(rig, act)
        n = clip.frames
        picks = sorted(set(int(round(x)) for x in np.linspace(0, n if not clip.loop else n - 1, frames)))
        marks = {}
        for d in clip.notify_list():
            fr = scene.ms_to_frame(d["ms"])
            marks.setdefault(fr, []).append(d)
            if fr not in picks:
                picks[picks.index(min(picks, key=lambda p: abs(p - fr)))] = fr
        picks = sorted(set(picks))
        plan.append((clip, act, picks, marks))
        for fr in picks:
            anim.set_frame(fr)
            allpts.append(review.mesh_points(rv.meshes, step=2))
    pts = np.concatenate(allpts)
    review.setup_render(cell, max(4, rv.samples // 2))
    review.frame_points(pts, cell, fov_h=24.0, pitch_ue=shading.CAM_PITCH_UE, fill=0.8)
    rv.ink(at=Vector(pts.mean(0)))
    rows = []
    width = max(len(p[2]) for p in plan)
    for clip, act, picks, marks in plan:
        anim.assign(rig, act)
        cells = []
        for fr in picks:
            anim.set_frame(fr)
            img = review.render_array()
            review.draw_text(img, 3, cell[1] - 10, f"F{fr} {scene.frame_to_ms(fr):.0f}MS")
            if fr in marks:
                review.border(img)
                for i, d in enumerate(marks[fr]):
                    review.draw_text(img, 3, 14 + i * 9, f"{d['name']} {d['ms']:.0f}", rgb=(255, 170, 70))
            cells.append(img)
        while len(cells) < width:
            cells.append(np.full_like(cells[0], 24))
        row = review.grid(cells, width, pad=2)
        label = np.zeros((14, row.shape[1], 3), np.uint8)
        label[:, :] = (24, 20, 30)
        review.draw_text(label, 4, 4, f"{clip.name}  {clip.length_ms:.0f}MS  {clip.frames}F{' LOOP' if clip.loop else ''}")
        rowimg = np.vstack([label, row])
        rows.append(rowimg)
        rv._save(f"anim_{clip.name}.png", rowimg)
    rv._save("anims.png", np.vstack(rows))


def ship_chest(pal: Palette, previews: bool = True):
    name = "SK_Prop_TreasureChest"
    obj, b, lid_parts = build_chest(pal)
    pal.save()
    arm = chest_rig(obj, b, lid_parts)
    rig = SimpleNamespace(obj=arm)
    acts = []
    for clip in CHEST_CLIPS:
        acts.append(chest_action(arm, clip))
    # verify the lid opens upward: the lid's front vertex rises at the Open end frame
    open_pose(rig, *acts[1])
    lo, hi = scene.world_bounds([obj])
    print(f"[chest] open bounds z {lo.z:.3f}..{hi.z:.3f} (closed top {CH + LID_R * 0.72:.3f})")
    open_bounds = export.bounds_ue([obj])
    rest_pose(arm)
    sk = export.export_skeletal_mesh(rig, [obj], name, CAT)
    anim_entries = []
    for act, clip in acts:
        fbx = export.export_animation(rig, act, CAT)
        anim_entries.append(export.clip_entry(clip, act, fbx))
    rest_pose(arm)                     # export_animation leaves the last clip (Open) assigned
    bounds = export.bounds_ue([obj])
    lo, hi = scene.world_bounds([obj])
    assert hi.z < CH + LID_R * 0.72 + 0.03, f"chest bounds not in the closed rest pose (top {hi.z:.3f} m)"
    print(f"[chest] rest bounds {bounds}")
    man = E.EnvManifest()
    man.set_palette(pal)
    width = float(obj.get("af_outline_width", 0.0))
    entry = {
        "kind": "SkeletalMesh", "category": CAT, "fbx": paths.rel_to_export(sk),
        "gameIds": ["decor_treasure_chest", "treasure_chest"], "skeleton": "SKEL_Prop_Chest", "scale": 1.0,
        "heightCm": round((CH + LID_R * 0.72) * 100.0, 1), "boundsCm": bounds,
        "boundsNote": "closed rest pose (bind pose, no clip); openBoundsCm = the Open end frame (lid up 105 deg)",
        "openBoundsCm": open_bounds,
        "bones": len(arm.data.bones),
        "triangles": {"toon": E.tris(obj, 0), "outline": E.tris(obj, 1)},
        "materialSlots": [
            {"index": 0, "name": pal.material_name, "parent": "M_AF_Toon", "palette": pal.family},
            {"index": 1, "name": shading.OUTLINE_MATERIAL, "parent": "M_AF_Outline", "palette": pal.family,
             "class": "interactive", "widthCm": round(width * 100.0, 2),
             "outlinePx1080": outline.SCREEN_PX_1080["interactive"], "outlineMix": outline.INK_MIX["interactive"]}],
        "sockets": [{"name": "fx_open", "bone": "root", "relLocCm": [0.0, 0.0, round((CH + 0.05) * 100, 1)],
                     "relRotDeg": [0.0, 0.0, 0.0], "restLocCm": [0.0, 0.0, round((CH + 0.05) * 100, 1)]},
                    {"name": "fx_seam", "bone": "root", "relLocCm": [0.0, round(CD / 2 * 100 + 1, 1),
                                                                     round(CH * 100, 1)],
                     "relRotDeg": [0.0, 0.0, 0.0], "restLocCm": [0.0, round(CD / 2 * 100 + 1, 1), round(CH * 100, 1)]}],
        "anims": anim_entries, "footprintTiles": [1, 1], "blocking": False, "blobShadowRadiusCm": 42.0,
        "family": "environment.interactive", "displayName": "TreasureChest",
        "note": "hidden-area reward / random-event cache (spec 6.8): Idle = restless rattle (the seam glow pulse "
                "0.55 +- 0.25 is the material's emissive x runtime CPD), Open = lid up 105 deg in 400 ms; "
                "ChestOpen notify -> NS_World_ChestOpen (light shaft, sparkles) at socket fx_open",
    }
    if previews:
        rv = E.EnvReview(name, arm, [obj], 0.62, blob_radius=0.42,
                         game_outline_px=outline.SCREEN_PX_1080["interactive"], outline_class="interactive")
        rv.ground_occludes = True
        rest_pose(arm)
        rv.game_view("front")
        rv.closeup("front")
        rv.turnaround()
        chest_open_views(rv, rig, *acts[1])
        chest_sheet(rv, rig, acts)
        rest_pose(arm)
        entry["previews"] = [str(Path(p).relative_to(rv.out.parent.parent)) for p, _ in rv.written]
        v = rv.ink_verdict()
        entry["inkGate"] = {k: v[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx",
                                              "maxHullLumP95", "gate", "baked")}
        entry["inkGate"]["cellsNote"] = "closed rest pose (game_1080crop) + Open end frame (game_open_1080crop)"
        E.RESULTS.append({"asset": name, "fbx": str(sk), "toonTris": E.tris(obj, 0), "hullTris": E.tris(obj, 1),
                          "ink": {k: v[k] for k in ("pass", "minFracRim", "minFracDark", "medianRimPx",
                                                    "maxHullLumP95")}, "inkFailures": v["failures"]})
        print(f"[chest] ink cells: " + "; ".join(f"{r['tag']}: median {r['rimPxMedian']:.2f}px "
                                                 f"rim {r['fracRim'] * 100:.0f}%" for r in rv.ink_reports))
    man.set_asset(name, entry)
    man.save()
    return obj


# ── gold pile ───────────────────────────────────────────────────────────────────────────────────────────
def build_goldpile(pal: Palette):
    name = "SM_Prop_GoldPile"
    b, rnd = new(pal, name)
    mound = M.ellipsoid((0.27, 0.21, 0.16), (0, 0, 0.02), sides=14, rings=7)
    for v in mound.bm.verts:
        v.co.z = max(v.co.z, -0.01)
    mid = b.add(mound, "gold", name="mound", hull_pad=True)
    # coins lying on the mound (lit discs) and spilled round it
    for q in range(16):
        a = rnd.uniform(0, math.tau)
        e = rnd.uniform(0.15, 1.2)
        d = Vector((math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)))
        p = Vector((d.x * 0.27, d.y * 0.21, 0.02 + d.z * 0.16))
        n = Vector((d.x / 0.27, d.y / 0.21, d.z / 0.16)).normalized()
        coin = M.cylinder(0.042, 0.042, -0.005, 0.005, sides=10)
        coin.transform(Vector((0, 0, 1)).rotation_difference(n.lerp(Vector((0, 0, 1)), 0.4)).to_matrix().to_4x4())
        coin.translate(p + n * 0.004)
        b.add(coin, "gold_hi" if q % 3 else "gold", name="coin", outline=False, covered_by=mid)
    for q in range(6):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0.3, 0.4)
        coin = M.cylinder(0.042, 0.042, 0.0, 0.01, sides=10)
        coin.transform(_rx(rnd.uniform(-12, 12))).translate((math.cos(a) * rr, math.sin(a) * rr * 0.8, 0.0))
        b.add(coin, "gold", name="spill")
    b.add(M.gem(0.045, 0.06, 0.04, sides=6).translate((0.06, -0.05, 0.17)), "gem", name="gem")
    return E.finish(b, pal, "interactive", ground_h=0.25, name=name)


# ── puzzle rune pillar ──────────────────────────────────────────────────────────────────────────────────
def build_runepillar(pal: Palette):
    name = "SM_Prop_Event_RunePillar"
    b, rnd = new(pal, name)
    ST.stone(b, "pillar_base", (0.7, 0.62, 0.2), (0, 0, 0.1), rnd, tilt=0.5, name="base")
    rings = []
    for z, w in ((0.19, 0.2), (1.28, 0.15)):
        rings.append([Vector((x * w, y * w * 0.9, z)) for x, y in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
    p = E.ring_loft(rings, cap0="ngon", cap1=None, smooth=False)
    top = [v for v in p.bm.verts if v.co.z > 1.0]
    top.sort(key=lambda v: math.atan2(v.co.y, v.co.x))
    apex = p.bm.verts.new((0, 0, 1.48))
    for i in range(4):
        p.bm.faces.new((top[i], top[(i + 1) % 4], apex))
    p.recalc_normals()
    p.auto_sharp(30)
    pid = b.add(p, "pillar", name="pillar", hull_pad=True)
    # zig-zag rune down the front (−Y) face + hot core (focal accent)
    zz = [(0.0, 1.15), (0.08, 1.0), (-0.07, 0.85), (0.07, 0.68), (-0.06, 0.52), (0.0, 0.38)]
    pts = []
    for x, z in zz:
        w = 0.2 + (0.15 - 0.2) * (z - 0.19) / (1.28 - 0.19)
        pts.append(Vector((x * w / 0.2, -w * 0.9 - 0.002, z)))
    nrm = Vector((0, -1, 0.04)).normalized()
    b.add(E.ribbon(pts, [nrm] * len(pts), 0.038, lift=0.0, thick=0.004), "rune_blue", name="rune", outline=False,
          covered_by=pid)
    b.add(E.ribbon(pts, [nrm] * len(pts), 0.012, lift=0.004, thick=0.002), "rune_core", name="rune_core",
          outline=False, covered_by=pid)
    for q in range(3):
        a = rnd.uniform(0, math.tau)
        shape = E.RockShape((0.09, 0.08, 0.06), (name, q), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=24.0).translate((math.cos(a) * 0.48, math.sin(a) * 0.42, 0)),
              "pillar_base", name="rubble", hull_pad=True)
    return E.finish(b, pal, "interactive", ground_h=0.9, name=name)


# ── exit portal / sealed gate ───────────────────────────────────────────────────────────────────────────
ARCH_W, ARCH_H = 1.0, 2.35        # inner half width, inner height of the frame


def portal_frame(b: M.Builder, rnd: random.Random, name: str):
    """Elven stone frame (two coursed pillars + a voussoir arch, keystone) ≈ 2.6 m, opening along X, faces −Y."""
    for s in (-1, 1):
        z = 0.0
        k = 0
        while z < 1.75:
            h = 0.3 if k % 2 == 0 else 0.26
            ST.stone(b, "frame" if k % 2 else "frame_lt", (0.34, 0.4, h - 0.01),
                     (s * (ARCH_W + 0.17), 0, z + h / 2), rnd, tilt=0.8, name="pillar")
            z += h
            k += 1
    # arch: voussoirs on a semicircle springing at z = 1.75
    R0, R1, zc = ARCH_W, ARCH_W + 0.34, 1.75
    n = 7
    for i in range(n):
        a0 = math.pi * i / n
        a1 = math.pi * (i + 1) / n - 0.012
        rings = []
        for a in (a0, (a0 + a1) / 2, a1):
            ca, sa = math.cos(a), math.sin(a)
            rings.append([Vector((ca * R0, -0.2, zc + sa * R0 * 0.62)), Vector((ca * R1, -0.2, zc + sa * R1 * 0.62)),
                          Vector((ca * R1, 0.2, zc + sa * R1 * 0.62)), Vector((ca * R0, 0.2, zc + sa * R0 * 0.62))])
        part = E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=False).auto_sharp(40)
        b.add(part, "frame_lt" if i == n // 2 else "frame", name="voussoir", hull_pad=True)
    for s in (-1, 1):
        ST.stone(b, "frame", (0.46, 0.5, 0.12), (s * (ARCH_W + 0.17), 0, 0.06), rnd, tilt=0.3, name="plinth")


def vortex(b: M.Builder, sealed: bool):
    """The standing oval vortex disc filling the opening (two-sided): deep rim → mid → core, with four swirl
    strokes (web ExitPortal swirl arcs); sealed = a dormant crimson seal carrying a runic ward (rings, a band of runes, a
    triangle-and-eye sigil)."""
    cz = 1.25
    rx, rz = ARCH_W - 0.04, 1.2
    # the open vortex keeps a deep teal body (web rgba(16,48,74,.95) rim → #1A6A8A) with a small mint / white eye
    # (VFX review 14: a 0.55 mint disc + 0.22 white core bloomed the whole centre to white); the swirl strokes carry
    # the brightness
    layers = ((1.0, "sealed_deep" if sealed else "vortex_deep"), (0.86, "sealed" if sealed else "vortex_mid"),
              (0.55 if sealed else 0.4, "sealed" if sealed else "vortex"),
              (0.22 if sealed else 0.13, "sealed" if sealed else "vortex_core"))
    for k, (f, reg) in enumerate(layers):
        disc = M.cylinder(1.0, 1.0, -0.012 - 0.004 * k, 0.012 + 0.004 * k, sides=28)
        disc.scale((rx * f, rz * f * (1.0 if f > 0.3 else 1.4), 1.0))
        disc.transform(_rx(90)).translate((0, 0, cz))
        b.add(disc, reg, name="vortex", outline=False)
    # dark rim round the vortex (web ExitPortal rim rgba(10,42,58,.9)): carries the ink where the disc shows
    # below the arch
    rim = [Vector((math.cos(a) * rx * 1.0, 0, cz + math.sin(a) * rz * 1.0)) for a in (math.tau * k / 32 for k in range(33))]
    b.add(M.tube(rim, [0.045] * len(rim), sides=6, cap0=None, cap1=None), "sealed_rim" if sealed else "vortex_rim",
          name="vortex_rim")
    if not sealed:
        for i in range(4):
            a0 = i * math.pi / 2
            pts = []
            for k in range(13):
                t = k / 12
                rr = 0.92 * (1 - t * 0.85)
                a = a0 + t * math.pi * 1.6
                pts.append(Vector((math.cos(a) * rx * rr, 0, cz + math.sin(a) * rz * rr)))
            for side in (-1, 1):
                q = [p + Vector((0, side * 0.03, 0)) for p in pts]
                b.add(E.ribbon(q, [Vector((0, side, 0))] * len(q), [0.06 - 0.03 * t for t in
                                                                     (k / 12 for k in range(13))], lift=0.0,
                               thick=0.004), "swirl" if i % 2 else "swirl_b", name="swirl", outline=False)
    else:
        # a runic ward seal (VFX review 14: the ring + one diagonal read as a 'no entry' road sign): two rings with a
        # band of eight angular runes between them and a triangle-and-eye sigil in the centre
        def at(a: float, rr: float, t: float, r: float, side: int, y: float) -> Vector:
            """(t tangential, r radial) metres round the point at angle ``a`` on the oval of relative radius rr"""
            u = Vector((math.cos(a), 0, math.sin(a)))
            v = Vector((-math.sin(a), 0, math.cos(a)))
            c = Vector((math.cos(a) * rx * rr, side * y, cz + math.sin(a) * rz * rr))
            return c + u * r + v * t

        runes = (  # polylines in (t, r) units of the band half-height (0.075 m)
            [[(0, -1), (0, 1)], [(-0.7, 1), (0, 0.15), (0.7, 1)]],              # algiz
            [[(0, -1), (0, 1)], [(0, 0.7), (0.65, 0.2), (0, -0.25)]],           # thurisaz
            [[(0.55, 1), (-0.45, 0), (0.55, -1)]],                               # kaunan
            [[(0, -1), (0, 1)], [(-0.65, 0.35), (0, 1), (0.65, 0.35)]],          # tiwaz
            [[(-0.55, 1), (0.45, 0.2), (-0.45, -0.2), (0.55, -1)]],              # sowilo
            [[(-0.5, -1), (-0.5, 1)], [(0.5, -1), (0.5, 1)], [(-0.5, 0.45), (0.5, -0.15)]],  # hagalaz
            [[(0, -1), (0, 1)], [(0, 1), (0.6, 0.45), (0, -0.05)], [(0, -0.05), (0.6, -1)]],  # raido
            [[(-0.6, -1), (0, 1), (0.6, -1)]],                                   # inverted v (ingwaz half)
        )
        for side in (-1, 1):
            nrm = [Vector((0, side, 0))]
            for rr, w in ((0.66, 0.034), (0.47, 0.026)):
                ring = [Vector((math.cos(a) * rx * rr, side * 0.032, cz + math.sin(a) * rz * rr)) for a in
                        (math.tau * i / 28 for i in range(29))]
                b.add(E.ribbon(ring, nrm * len(ring), w, lift=0.0, thick=0.004), "sealed_glyph", name="ward",
                      outline=False)
            for k, rune in enumerate(runes):
                a = math.tau * (k + 0.5) / len(runes) + math.pi / 2
                for line in rune:
                    pts = [at(a, 0.565, t * 0.075, r * 0.075, side, 0.034) for t, r in line]
                    if len(pts) == 2:
                        pts.insert(1, (pts[0] + pts[1]) / 2)
                    b.add(E.ribbon(pts, nrm * len(pts), 0.022, lift=0.0, thick=0.004), "sealed_glyph",
                          name="ward_rune", outline=False)
            # centre sigil: an upward triangle round a small eye (ring + pupil)
            tri = [at(math.pi / 2 + math.tau * i / 3, 0.30, 0, 0, side, 0.034) for i in range(4)]
            b.add(E.ribbon(tri, nrm * len(tri), 0.026, lift=0.0, thick=0.004), "sealed_glyph", name="ward_sigil",
                  outline=False)
            eye = [Vector((math.cos(a) * 0.1, side * 0.034, cz + math.sin(a) * 0.065)) for a in
                   (math.tau * i / 16 for i in range(17))]
            b.add(E.ribbon(eye, nrm * len(eye), 0.02, lift=0.0, thick=0.004), "sealed_glyph", name="ward_sigil",
                  outline=False)
            b.add(M.cylinder(0.028, 0.028, 0.0, 0.006, sides=8).transform(_rx(90))
                  .translate((0, side * 0.036, cz)), "sealed_glyph", name="ward_sigil", outline=False)


def rune_stones(b: M.Builder, rnd: random.Random, glow_region: str):
    for s in (-1, 1):
        x = s * 1.85
        rings = []
        for z, w, d in ((0.0, 0.17, 0.13), (0.95, 0.14, 0.11), (1.12, 0.07, 0.07)):
            rings.append([Vector((x + math.cos(a) * w + (0.04 * s if z > 1 else 0), -0.15 + math.sin(a) * d, z))
                          for a in (math.tau * i / 6 for i in range(6))])
        st = E.ring_loft(rings, cap0="ngon", cap1="fan", smooth=False).auto_sharp(30)
        sid = b.add(st, "runestone", name="runestone", hull_pad=True)
        c = Vector((x, -0.15 - 0.125, 0.62))
        pts = [c + Vector((0, 0, 0.12)), c + Vector((0.035 * s, 0, 0.02)), c + Vector((-0.02 * s, 0, -0.05)),
               c + Vector((0, 0, -0.13))]
        b.add(E.ribbon(pts, [Vector((0, -1, 0))] * 4, 0.035, lift=0.0, thick=0.004), glow_region, name="rune",
              outline=False, covered_by=sid)


def ground_ring(b: M.Builder, region: str):
    for i in range(12):
        a = math.tau * i / 12
        c = Vector((math.cos(a) * 1.25, -0.9 + math.sin(a) * 0.62, 0.006))
        m = M.box((0.07, 0.11, 0.012), (0, 0, 0), bevel=0.003)
        m.transform(_rz(math.degrees(a) + 90)).translate(c)
        b.add(m, region, name="ring_mark", outline=False)


def build_portal(pal: Palette):
    name = "SM_Prop_ExitPortal"
    b, rnd = new(pal, name)
    portal_frame(b, rnd, name)
    vortex(b, sealed=False)
    rune_stones(b, rnd, "vortex")
    # the ground rune ring is the runtime FX (NS_World_ExitPortal ground ring at socket fx_ring), not mesh
    shape = E.RockShape((0.14, 0.12, 0.1), (name, "rock"), bumps=4, amp=0.08, top_cut=0.3)
    b.add(E.rock(shape, subdiv=1, facet=24.0).translate((1.45, -0.7, 0)), "runestone", name="rock", hull_pad=True)
    F.tuft(b, Vector((-1.5, -0.6, 0)), 0.24, 8, rnd, segs=2, inked=True)
    return E.finish(b, pal, "interactive", ground_h=1.6, name=name)


def build_sealed(pal: Palette):
    name = "SM_Prop_ExitGate_Sealed"
    b, rnd = new(pal, name)
    portal_frame(b, rnd, name)
    vortex(b, sealed=True)
    rune_stones(b, rnd, "sealed_glyph")
    # barricade: three heavy planks nailed across the opening (front, −Y), two crossed
    planks = [((-0.95, 0.55), (0.95, 0.75), 0.22), ((-0.95, 1.55), (0.95, 1.35), 0.2), ((-0.85, 0.25), (0.8, 2.0), 0.2),
              ((0.85, 0.3), (-0.8, 1.95), 0.18)]
    for k, ((x0, z0), (x1, z1), w) in enumerate(planks):
        a = Vector((x0, -0.27 - 0.05 * (k >= 2), z0))
        e = Vector((x1, -0.27 - 0.05 * (k >= 2), z1))
        L = (e - a).length + 0.18
        pl = M.box((L, 0.05, w), (0, 0, 0), bevel=0.012)
        ang = math.degrees(math.atan2(e.z - a.z, e.x - a.x))
        pl.transform(_ry(-ang)).translate((a + e) / 2)
        pid = b.add(pl, "plank", name="plank", hull_pad=True)
        d = (e - a).normalized()
        for t in (-0.42, 0.42):
            c = (a + e) / 2 + d * (L / 2 * t * 2)
            b.add(M.cylinder(0.022, 0.022, 0.0, 0.01, sides=6).transform(_rx(90)).translate(c + Vector((0, -0.026, 0))),
                  "nail", name="nail", outline=False, covered_by=pid)
        paint(b, "plank_line", [(a + e) / 2 - d * L * 0.4 + Vector((0, -0.026, w * 0.15)),
                                (a + e) / 2 + Vector((0, -0.026, w * 0.15)),
                                (a + e) / 2 + d * L * 0.4 + Vector((0, -0.026, w * 0.15))], (0, -1, 0), 0.012, pid)
    # iron chain across the arch + padlock
    links = []
    for i in range(11):
        t = i / 10
        x = -0.95 + 1.9 * t
        z = 1.05 - 0.22 * math.sin(math.pi * t)
        links.append(Vector((x, -0.38, z)))
    for i, c in enumerate(links):
        ring = M.tube([c + Vector((math.cos(a) * 0.06, 0, math.sin(a) * 0.035)) for a in
                       (math.tau * k / 8 for k in range(9))], [0.012] * 9, sides=5, cap0=None, cap1=None)
        if i % 2:
            ring.transform(Matrix.Translation(c) @ _rz(90) @ Matrix.Translation(-c))
        b.add(ring, "iron", name="chain")
    lock = M.box((0.13, 0.06, 0.15), links[5] + Vector((0, -0.02, -0.12)), bevel=0.02)
    lid = b.add(lock, "iron", name="padlock", hull_pad=True)
    shackle = M.tube([links[5] + Vector((-0.04, -0.02, -0.06)), links[5] + Vector((-0.04, -0.02, 0.0)),
                      links[5] + Vector((0.04, -0.02, 0.0)), links[5] + Vector((0.04, -0.02, -0.06))], [0.013] * 4,
                     sides=6)
    b.add(shackle, "iron", name="shackle")
    # a red-painted warning plank on a stake beside the gate
    post = M.box((0.07, 0.07, 1.05), (-1.62, -0.75, 0.5), bevel=0.01)
    b.add(post, "plank", name="sign_post")
    board = M.box((0.62, 0.04, 0.28), (-1.62, -0.8, 0.88), bevel=0.01)
    board.transform(Matrix.Translation((-1.62, -0.8, 0.88)) @ _ry(-6) @ Matrix.Translation((1.62, 0.8, -0.88)))
    sid = b.add(board, "plank", name="sign", hull_pad=True)
    paint(b, "sign_paint", [(-1.82, -0.823, 0.95), (-1.62, -0.823, 0.82), (-1.42, -0.823, 0.95)], (0, -1, 0), 0.045, sid)
    paint(b, "sign_paint", [(-1.82, -0.823, 0.8), (-1.62, -0.823, 0.93), (-1.42, -0.823, 0.8)], (0, -1, 0), 0.045, sid)
    F.tuft(b, Vector((1.5, -0.55, 0)), 0.24, 8, rnd, segs=2, inked=True)
    return E.finish(b, pal, "interactive", ground_h=1.6, name=name)


# ── markers ─────────────────────────────────────────────────────────────────────────────────────────────
def build_magnifier(pal: Palette):
    name = "SM_FX_ClueMagnifier"
    b, rnd = new(pal, name)
    # lens plane tilted back 40° (reads from the high W1 camera at any yaw); floats 0.55 m up (the Pickup bob /
    # rock of AAbyssPropActor runs about the ground pivot)
    mtx = Matrix.Translation((0, 0, 0.55)) @ _rx(-40)

    def T(p):
        return p.transform(mtx)
    R = 0.15
    rim = M.tube([Vector((math.cos(a) * R, 0, math.sin(a) * R)) for a in (math.tau * k / 20 for k in range(21))],
                 [0.028] * 21, sides=8, cap0=None, cap1=None)
    b.add(T(rim), "brass_rim", name="rim")
    lens = M.cylinder(R * 0.98, R * 0.98, -0.012, 0.012, sides=20)
    lens.transform(_rx(90))
    b.add(T(lens), "lens_out", name="lens")
    inner = M.cylinder(R * 0.62, R * 0.62, -0.016, 0.016, sides=18)
    inner.transform(_rx(90)).translate((-0.025, 0, 0.025))
    b.add(T(inner), "lens", name="lens_in", outline=False)
    for side in (-1, 1):
        gl = E.ribbon([Vector((-0.09, side * 0.02, 0.03)), Vector((-0.06, side * 0.02, 0.08)),
                       Vector((-0.02, side * 0.02, 0.105))], [Vector((0, side, 0))] * 3, 0.022, lift=0.0, thick=0.003)
        b.add(T(gl), "glint", name="glint", outline=False)
    d = Vector((0.62, 0, -0.78)).normalized()
    a = d * (R + 0.02)
    ferrule = M.tube([a, a + d * 0.05], [0.032, 0.032], sides=8)
    b.add(T(ferrule), "brass_rim", name="ferrule")
    handle = M.tube([a + d * 0.05, a + d * 0.2, a + d * 0.27], [0.03, 0.034, 0.03], sides=8)
    b.add(T(handle), "handle", name="handle")
    return E.finish(b, pal, "interactive", ground_h=None, name=name)


def build_arrow(pal: Palette):
    name = "SM_FX_GuideArrow"
    b, rnd = new(pal, name)
    # chevron lying flat, tip toward −Y (the mesh forward)
    outer = [(0.0, -0.2), (0.17, 0.02), (0.1, 0.1), (0.0, -0.03), (-0.1, 0.1), (-0.17, 0.02)]
    body = M.plate(outer[::-1] if False else outer, 0.045, bevel=0.01)
    body.translate((0, 0, 0.03))
    bid = b.add(body, "arrow", name="chevron", hull_pad=True)
    hi = [(0.0, -0.17), (0.075, -0.075), (0.0, -0.06), (-0.075, -0.075)]
    top = M.plate(hi, 0.008, bevel=0.002).translate((0, 0, 0.056))
    b.add(top, "arrow_hi", name="hi", outline=False, covered_by=bid)
    for s in (-1, 1):
        back = [(s * 0.1, 0.1), (s * 0.17, 0.02), (s * 0.14, 0.02), (s * 0.085, 0.072)]
        if s < 0:
            back = back[::-1]
        bk = M.plate(back if s > 0 else back, 0.008, bevel=0.002).translate((0, 0, 0.056))
        b.add(bk, "arrow_dk", name="back", outline=False, covered_by=bid)
    obj = E.finish(b, pal, "interactive", ground_h=None, name=name)
    return obj


def herb_leaf(base: Vector, d: Vector, L: float, W: float, side: Vector, segs: int = 5) -> M.Part:
    """A lanceolate leaf: ``E.blade`` re-profiled so it is narrow at the stem, widest at 40 % and pointed."""
    lf = E.blade(base, d, L, W, bend=0.16, side=side, segs=segs, thick=0.006)
    bm = lf.bm
    bm.verts.ensure_lookup_table()
    s_ax = side.normalized()
    for i in range(segs + 1):
        t = i / segs
        lin = W * (1 - t)
        if t <= 0.4:
            prof = W * (0.4 + 0.6 * math.sin(0.5 * math.pi * t / 0.4))
        else:
            prof = W * max(0.06, ((1 - t) / 0.6) ** 0.8)
        vs = bm.verts[4 * i:4 * i + 4]
        c = sum((v.co for v in vs), Vector()) / 4
        for v in vs:
            lat = (v.co - c).dot(s_ax)
            side_sign = 1.0 if lat >= 0 else -1.0
            if lin > 1e-4:
                v.co = v.co + s_ax * (lat * (prof / lin - 1.0))
            else:                               # the tip section collapsed to a point: re-open it
                v.co = v.co + s_ax * (side_sign * prof / 2 - lat)
    bm.normal_update()
    return lf


def build_herb(pal: Palette):
    """翡翠草药 sprig. Art review 9: seven overlapping leaves whose padded hulls (veins 6 mm proud) were as wide as
    the leaves read as a dark-green blot (median rim 5.4 px for the 2.0 px class: 2 px of ink each side of a 7 px
    leaf). Now five broad leaves (9.5 → 6.7 cm, 20 → 14 cm long) fanned apart on the golden angle and laid out flat
    enough to face the high camera, veins lying flush (1.5 mm), leaves 6 mm thick — each leaf shows its green face
    inside the screen-constant 2 px line."""
    name = "SM_Prop_Gather_EmeraldHerb"
    b, rnd = new(pal, name)
    mound = M.ellipsoid((0.11, 0.09, 0.03), (0, 0, 0.0), sides=10, rings=4)
    b.add(mound, "soil", name="soil")
    stem = M.tube([Vector((0, 0, 0.0)), Vector((0.01, 0.0, 0.12)), Vector((-0.01, 0.01, 0.27))], [0.012, 0.01, 0.006],
                  sides=6)
    b.add(stem, "stem", name="stem")
    for i in range(5):
        a = i * 2.4 + rnd.uniform(-0.15, 0.15)
        z = 0.05 + i * 0.045
        L = 0.2 - i * 0.014
        d = Vector((math.cos(a), math.sin(a), 0.32 + i * 0.12)).normalized()
        base = Vector((0, 0, z))
        side = d.cross(Vector((0, 0, 1))).normalized()
        lf = herb_leaf(base, d, L, 0.095 - i * 0.007, side)
        lid = b.add(lf, "leaf", name="leaf", hull_pad=True)
        # glowing midrib vein lying on the leaf's upper face
        out = d.cross(side).normalized()
        if out.z < 0:
            out = -out
        horiz = Vector((d.x, d.y, 0)).normalized()
        pts = []
        for k in range(4):
            t = 0.1 + 0.75 * k / 3
            p = base + d * (L * t) + horiz * (0.16 * L * t * t) - Vector((0, 0, 0.16 * L * t * t * 0.6))
            pts.append(p)
        b.add(E.ribbon(pts, [out] * 4, [0.014, 0.012, 0.009, 0.005], lift=0.0032, thick=0.0015), "vein",
              name="vein", outline=False, covered_by=lid)
    bud = M.ellipsoid((0.025, 0.025, 0.035), (-0.01, 0.01, 0.29), sides=8, rings=4)
    b.add(bud, "bud", name="bud")
    return E.finish(b, pal, "interactive", ground_h=0.2, name=name)


ASSETS: dict = {
    "SM_Prop_GoldPile": (build_goldpile, dict(game_ids=["decor_gold_pile", "gold_pile"], footprint=[1, 1],
                                              blocking=False, blob=0.36)),
    "SM_Prop_Event_RunePillar": (build_runepillar, dict(game_ids=["decor_event_puzzle_rune_pillar"],
                                                        footprint=[1, 1], blocking=False, blob=0.45)),
    "SM_Prop_ExitPortal": (build_portal, dict(game_ids=["exit_portal"], footprint=[1, 1], blocking=False, blob=1.3,
                                              sockets=[{"name": "fx_vortex", "pos": (0, 0, 1.25)},
                                                       {"name": "fx_ring", "pos": (0, -0.9, 0.02)}])),
    "SM_Prop_ExitGate_Sealed": (build_sealed, dict(game_ids=["exit_gate_sealed", "exit_sealed"], footprint=[1, 1],
                                                   blocking=False, blob=1.3,
                                                   sockets=[{"name": "fx_vortex", "pos": (0, 0, 1.25)}])),
    "SM_FX_ClueMagnifier": (build_magnifier, dict(game_ids=["clue_mark", "fx_clue"], blob=0.25, cat="VFX",
                                                  blocking=False)),
    "SM_FX_GuideArrow": (build_arrow, dict(game_ids=["guide_arrow"], blob=0.01, cat="VFX", blocking=False,
                                           extra_kw={"meshYawOffsetDeg": -90.0})),
    "SM_Prop_Gather_EmeraldHerb": (build_herb, dict(game_ids=["gather_herb"], footprint=[1, 1], blocking=False,
                                                    blob=0.15)),
}
NOTES = {
    "SM_Prop_ExitPortal": "open zone exit (later chapters; W7 seals the Chapter-1 exit): faces into the map (-Y); "
                          "NS_World_ExitPortal (rotating swirl, motes, ground ring pulse) + teal light at fx_vortex",
    "SM_Prop_ExitGate_Sealed": "W7: the twilight_forest exit (119,60) shows this sealed gate; the '第二章即将开放' message "
                               "is UI (zone.exit.sealedChapter2); exit.sealed recipe at fx_vortex. Not blocking: the "
                               "hero walks onto the exit tile to get the message (W8 walk-then-act); the wall ring round "
                               "it blocks",
    "SM_FX_ClueMagnifier": "quest clue marker (QuestWorld investigate_clue): floats 0.5 m up, bobs 6 px and rocks +-8 "
                           "deg (1100 ms yoyo) over NS_World_ClueRing; pivot = lens centre",
    "SM_FX_GuideArrow": "quest guide chevron on a 1.1 m ring round the hero, 30 cm up; tip = mesh forward (-Y, UE +Y "
                        "before meshYawOffsetDeg); CPD Tint (9..12) recolours it cream/white for turn-ins",
    "SM_Prop_Gather_EmeraldHerb": "gather node of q_herb_gathering (翡翠草药): bob + gold glow ring are runtime FX",
    "SM_Prop_GoldPile": "hidden-area reward (110,107): glint = runtime glow card",
    "SM_Prop_Event_RunePillar": "random event environmental_puzzle (plains): glow #72D8FF r 18 px = runtime card",
}


def main(argv: list[str]) -> int:
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or ["SK_Prop_TreasureChest"] + list(ASSETS)
    all_names = ["SK_Prop_TreasureChest"] + list(ASSETS)
    names = [n for n in all_names if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(E.PROPS)
    if sheet:
        objs = []
        for n in names:
            if n == "SK_Prop_TreasureChest":
                objs.append(build_chest(pal)[0])
            else:
                objs.append(ASSETS[n][0](pal))
        pal.save()
        E.sheet(objs, "sheet_interactive_" + "_".join(n.split("_")[-1][:6] for n in names)[:80], cols=3)
        return 0
    for n in (names if "--family" not in argv else []):
        scene.reset()
        pal = Palette(E.PROPS)
        if n == "SK_Prop_TreasureChest":
            ship_chest(pal, previews)
            continue
        fn, kw = ASSETS[n]
        kw = dict(kw)
        cat = kw.pop("cat", CAT)
        extra = {"displayName": n.split("_", 2)[-1], "family": "environment.interactive"}
        extra.update(kw.pop("extra_kw", {}))
        if n in NOTES:
            extra["note"] = NOTES[n]
        obj = fn(pal)
        pal.save()
        E.ship(obj, n, cat, pal, obj["af_outline_class"], extra=extra, previews=previews, close_facing="front", **kw)
    if previews and (len(names) > 1 or "--family" in argv):
        scene.reset()
        pal = Palette(E.PROPS)
        objs = [build_chest(pal)[0] if n == "SK_Prop_TreasureChest" else ASSETS[n][0](pal) for n in names]
        for o in objs:
            if o.name.startswith("SM_FX_ClueMagnifier"):
                o.location.z = 0.0
        big = [o for o in objs if "Exit" in o.name]
        small = [o for o in objs if o not in big]
        E.family_sheet(small, "interactive_props.png", "INTERACTIVE PROPS AND MARKERS", cols=4, facing_deg=-135.0)
        for o in small:
            o.hide_render = True
        if big:
            E.family_sheet(big, "exit_gates.png", "ZONE EXIT: PORTAL (LATER) AND SEALED GATE (W7)", cols=3)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
