"""Ship definition of the rogue (``hero_ship.HeroDef``): items (twin daggers, flash-bow swap, hip quiver), FX block,
QA groups / limits, review frames, the flash-bow visibility and the preview-only energy string.

Run through ``rogue.py`` (no flag = ship; ``--quick`` / ``--anims`` / ``--frames`` / ``--game`` for iteration).
"""
from __future__ import annotations

import math

import bpy
from mathutils import Matrix, Vector

import hero_ship as HS
import rogue as RG
import rogue_clips as RC
from kit import review

# spec §3 general + §3.3 FX attachments (for the UE module; colours are the web's)
EYE_GLOW = {"color": "#FFC24A", "radiusCm": 7.5, "alpha": 0.42, "coreColor": "#FFE3A0", "coreRadiusCm": 2.6,
            "portraitEyeRadiusCm": 5.0,          # per-eye glow (UI portrait only)
            "coreAlpha": 0.5}
STRING_COLOR = "#9DFFC8"
FX = {
    "classColor": "#3FB86A", "impactColor": "#9DFFC8", "spiritColor": "#66E58A",
    # amber eyes in the mask's slit (web glow r 1.8 u #FFC24A α .4 per eye): one card between the eyes on the `eyes`
    # socket (UE reads it as the hero's visorGlow card: facing fade, off late in Death — web: t < 0.6)
    "visorGlow": {"socket": "eyes", "color": EYE_GLOW["color"], "radiusCm": EYE_GLOW["radiusCm"],
                  "alpha": EYE_GLOW["alpha"], "alphaPerFx": 0.0, "coreColor": EYE_GLOW["coreColor"],
                  "coreRadiusCm": EYE_GLOW["coreRadiusCm"], "coreAlpha": EYE_GLOW["coreAlpha"], "blend": "additive",
                  "falloff": [list(x) for x in review.GLOW_STOPS],
                  "facingFade": "clamp(2 * dot(headFwd, toCamera) + 0.3, 0, 1)", "offAtDeathFraction": 0.6},
    "bloom": {"source": "palette P.b emissive regions (eyes, dagger poison edges, bow nocks)",
              "lobesSigmaPx1080": [list(x) for x in review.BLOOM_LOBES], "strength": review.BLOOM_STRENGTH},
    # web attack smear: main blade #D8FFE8 α .55·fx (tip = blade tip, base 3 u up the blade), sample spacing 0.02 of
    # the clip (8.9 ms)
    "attackTrail": {"color": "#D8FFE8", "alpha": 0.55, "sockets": ["tip", "mid"], "samples": 7, "sampleMs": 9},
    # the cast glows on the hands, not on a blade: the blade cards are off (UE castBladeGlow)
    "castBladeGlow": {"color": "#6DFFA0", "radiusCm": 0.0, "alpha": 0.0, "tipColor": "#6DFFA0", "tipRadiusCm": 0.0,
                      "tipAlpha": 0.0, "embers": {"count": 0, "color": "#2FD6A0"}},
    # not yet read by UAbyssVfxSystem (documented for it, web drawFx :604-645)
    "offhandTrail": {"item": RG.DAGGER_L, "color": "#6DFFA0", "alpha": 0.35, "sockets": ["tip", "mid"], "samples": 7,
                     "sampleMs": 9, "note": "the off-hand poison smear (web α .35·fx)"},
    "castHandGlow": {"sockets": ["fx_hand_r", "fx_hand_l"], "color": "#6DFFA0", "radiusCm": [11.0, 22.0],
                     "alpha": 0.5, "note": "r 4 + 4·fx u, α .5·fx on both hands during Cast01 / Cast02"},
    "shadowWisps": {"socket": "fx_feet", "count": 5, "color": "#2FD6A0", "riseCm": 72.0, "radiusCm": 6.0,
                    "alpha": 0.45, "note": "5 wisps rising 26 u off the body while casting, 1.3 cycles per clip"},
    "bowString": {"item": RG.BOW, "sockets": ["string_top", "string_bottom"], "drawSocket": "fx_hand_r",
                  "color": STRING_COLOR, "widthCm": 0.9, "alpha": 0.9,
                  "note": "Cast_Multishot: a beam string_top → fx_hand_r → string_bottom from FX_BowShow to Release, "
                          "then straight string_top → string_bottom (twang 60 ms) until FX_BowHide"},
    "nockedArrows": {"count": 3, "fanDeg": 9.0, "from": "fx_hand_r", "toSocket": "arrow_rest", "overshootCm": 22.0,
                     "color": STRING_COLOR, "alpha": 0.8, "fromMs": 110.0, "toMs": 246.0,
                     "note": "energy arrows on the string during the draw (multishot fan); SM_FX_Arrow flies at Release"},
    "bowFlash": {"notifies": ["FX_BowShow", "FX_BowHide"], "color": "#66E58A", "radiusCm": 40.0, "ms": 120,
                 "note": "the bow flashes in / out of the off hand (the off-hand dagger hidden meanwhile)"},
    "heroHalo": {"color": "#FFEEDD", "radiusCm": 200},
}


def clip_meta(c, e: dict) -> None:
    if c.name == "Cast_Multishot":
        e["signature"] = True
        e["skill"] = "multishot"
        e["skills"] = ["multishot", "piercing_arrow", "poison_arrow", "arrow_rain"]
        e["itemSwap"] = {"show": RG.BOW, "hide": RG.DAGGER_L, "socket": "weapon_l",
                         "showNotify": "FX_BowShow", "hideNotify": "FX_BowHide"}
    elif c.name == "Cast02":
        e["skills"] = ["explosive_trap", "slow_trap", "chain_trap", "vanish"]
    elif c.name == "Cast01":
        e["skills"] = ["poison_blade", "death_mark", "poison_cloud", "shadow_step"]
    elif c.name.startswith("Attack"):
        e["skills"] = ["basic", "backstab"]


def visibility(clip, ms: float) -> dict:
    """The flash-bow replaces the off-hand dagger between FX_BowShow and FX_BowHide of Cast_Multishot."""
    on = clip == "Cast_Multishot" and RC.RogueAnims.BOW_SHOW <= ms < RC.RogueAnims.BOW_HIDE
    return {RG.BOW: on, RG.DAGGER_L: not on}


# ── preview-only energy string (UE draws it as runtime FX: ``fx.bowString`` / ``fx.nockedArrows``) ──────
_STRING = "FX_Preview_BowString"


def _string_obj():
    ob = bpy.data.objects.get(_STRING)
    if ob is not None:
        return ob
    me = bpy.data.meshes.new(_STRING)
    ob = bpy.data.objects.new(_STRING, me)
    bpy.context.scene.collection.objects.link(ob)
    mat = bpy.data.materials.new(_STRING)
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    em = nt.nodes.new("ShaderNodeEmission")
    h = STRING_COLOR.lstrip("#")
    em.inputs["Color"].default_value = tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)) + (1.0,)
    em.inputs["Strength"].default_value = 1.0
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(em.outputs[0], out.inputs[0])
    me.materials.append(mat)
    ob["af_preview_fx"] = True
    return ob


def _sock_world(item_obj, name: str, sockets: list) -> Vector:
    s = next(x for x in sockets if x["name"] == name)
    return item_obj.matrix_world @ Vector(s["pos"])


def _beam(bm, a: Vector, b: Vector, r: float, sides: int = 6) -> None:
    import bmesh  # noqa: F401
    d = (b - a)
    if d.length < 1e-6:
        return
    n = d.normalized()
    x = n.orthogonal().normalized()
    y = n.cross(x)
    ra = [bm.verts.new(a + (x * math.cos(2 * math.pi * i / sides) + y * math.sin(2 * math.pi * i / sides)) * r)
          for i in range(sides)]
    rb = [bm.verts.new(b + (x * math.cos(2 * math.pi * i / sides) + y * math.sin(2 * math.pi * i / sides)) * r)
          for i in range(sides)]
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((ra[i], ra[j], rb[j], rb[i]))


def string_hook(clip, ms: float) -> None:
    """Frame-change hook: rebuild the preview string (and the three nocked energy arrows during the draw)."""
    import bmesh
    ob = _string_obj()
    bow = bpy.data.objects.get(RG.BOW)
    rig = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
    bm = bmesh.new()
    visible = (bow is not None and not bow.hide_render and rig is not None and bow.parent is not None)
    if visible:
        import json
        socks = json.loads(bow["af_sockets"])
        top, bot = _sock_world(bow, "string_top", socks), _sock_world(bow, "string_bottom", socks)
        if ms < RC.RogueAnims.REL * RC.RogueAnims.SPAN_C:
            pb = rig.pose.bones["hand_r"]
            hb = rig.data.bones["hand_r"]
            k = rig.matrix_world @ (pb.matrix @ (hb.matrix_local.inverted() @ rig.data.bones["fingers_01_r"].head_local))
            hand = k
            _beam(bm, top, hand, 0.0045)
            _beam(bm, hand, bot, 0.0045)
            if ms >= 110.0:
                rest = _sock_world(bow, "arrow_rest", socks)
                d = (rest - hand)
                L = d.length + 0.22
                dn = d.normalized()
                side = (top - bot).normalized().cross(dn).normalized()
                for ang in (-9.0, 0.0, 9.0):
                    q = Matrix.Rotation(math.radians(ang), 3, (top - bot).normalized())
                    dd = q @ dn
                    _beam(bm, hand + dd * 0.02, hand + dd * L, 0.006 if ang == 0.0 else 0.0045)
                del side
        else:
            _beam(bm, top, bot, 0.0045)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    ob.hide_render = not visible


def preview_extra(rv, rig, meshes, clips, actions, items) -> None:
    """Rogue extras: a sheet of the flash-bow swap (Cast_Multishot from the front 3/4 and the side)."""
    from kit import anim
    import hero_ship
    acts = dict(zip([c.name for c in clips], actions))
    cl = {c.name: c for c in clips}
    c = cl["Cast_Multishot"]
    anim.assign(rig, acts["Cast_Multishot"])
    import numpy as np
    from kit import scene
    frames = sorted({scene.ms_to_frame(ms) for ms in (0.0, 60.0, 120.0, 168.0, 246.0, 336.0, 420.0, 480.0)})
    cells = []
    res = (220, 260)
    for fac in ("se", "sw"):
        rv.facing(fac)
        pts = []
        for fr in frames:
            anim.set_frame(fr)
            pts.append(review.mesh_points([m for m in meshes if not m.hide_render], step=4))
        pts = np.concatenate(pts)
        review.setup_render(res, 8)
        review.frame_points(pts, res, fov_h=24.0, pitch_ue=-35.0, fill=0.95)
        review.set_outline_px(meshes, 1.6, Vector(pts.mean(0)))
        for fr in frames:
            anim.set_frame(fr)
            img = review.render_array()
            review.draw_text(img, 3, 3, f"{fac} F{fr}")
            if fr == scene.ms_to_frame(246.0):
                review.border(img)
                review.draw_text(img, 3, 12, "RELEASE", rgb=(255, 170, 70))
            cells.append(img)
    rv._save("multishot_bow_swap.png", review.grid(cells, len(frames)))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")
    del c, hero_ship


ITEM_VIEWS = {
    RG.DAGGER: [("DAGGER R FLAT", Matrix.Rotation(math.radians(90), 4, "X")),
                ("DAGGER R EDGE", Matrix.Rotation(math.radians(90), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"))],
    RG.DAGGER_L: [("DAGGER L FLAT", Matrix.Rotation(math.radians(-80), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"))],
    RG.BOW: [("BOW SIDE", Matrix.Rotation(math.radians(90), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X")),
             ("BOW 3/4", Matrix.Rotation(math.radians(135), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"))],
    RG.QUIVER: [("QUIVER", Matrix.Rotation(math.radians(200), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"))],
}
RG.ITEM_VIEWS = ITEM_VIEWS

LEGS = ("leg_l", "leg_r", "kneepad_l", "kneepad_r", "boot_l", "boot_r", "boot_cuff_l", "boot_cuff_r")
TORSO = ("capelet", "jerkin", "bandolier", "belt", "buckle", "tasset", "pouch", "pouch_flap", "knife",
         "knife_pommel")
QA_GROUPS = {
    "bodyNoGripR": ("!", ("hand_r", "thumb_r", "wrist_r", "glove_band_r")),
    "bodyNoGripL": ("!", ("hand_l", "thumb_l", "wrist_l", "glove_band_l")),
    "all": "*",
    "scarf": ("scarf_tail0", "scarf_tail1"),
    "torso": TORSO,
    "tassets": ("tasset",),
    "legs": LEGS,
    "hoodTip": ("hood_tip",),
}
QA_PAIRS = {
    "daggerBody": ("item:" + RG.DAGGER, "bodyNoGripR"),
    "daggerLBody": ("item:" + RG.DAGGER_L, "bodyNoGripL"),
    "daggers": ("item:" + RG.DAGGER, "item:" + RG.DAGGER_L),
    "bowBody": ("item:" + RG.BOW, "bodyNoGripL"),
    "bowDagger": ("item:" + RG.BOW, "item:" + RG.DAGGER),
    "quiverBody": ("item:" + RG.QUIVER, "all"),
    "quiverDagger": ("item:" + RG.QUIVER, "item:" + RG.DAGGER),
    "quiverDaggerL": ("item:" + RG.QUIVER, "item:" + RG.DAGGER_L),
    "scarfBody": ("scarf", "torso"),
    "scarfLegs": ("scarf", "legs"),
    "scarfQuiver": ("scarf", "item:" + RG.QUIVER),
    "tassetLegs": ("tassets", "legs"),
    "hoodTipBody": ("hoodTip", "torso"),
}
QA_LIMITS = {"ground": -0.5, "daggerBody": 0, "daggerLBody": 0, "daggers": 0, "bowBody": 0, "bowDagger": 0,
             "quiverBody": 0, "quiverDagger": 0, "quiverDaggerL": 0, "scarfBody": 6, "scarfLegs": 6,
             "scarfQuiver": 4, "tassetLegs": 8, "hoodTipBody": 6}
# Dodge (a 240 ms tuck roll: thighs folded to the chest) and Death (kneel → on the back): the split tassets are
# pressed between the folded thighs and the belly for a few frames
QA_CLIP_LIMITS = {"Death": {"scarfBody": 40, "scarfLegs": 30, "tassetLegs": 60, "hoodTipBody": 20},
                  "Dodge": {"scarfBody": 20, "scarfLegs": 20, "tassetLegs": 140}}

GAME_KEYS = [("Idle", 0, "se"), ("Run", 8, "se"), ("Attack01", "beat", "se"), ("Attack02", "beat", "se"),
             ("Attack03", "beat", "se"), ("Cast01", "beat", "se"), ("Cast_Multishot", "beat", "se"),
             ("Death", 32, "se")]
CONTACT_KEYS = [(n, "beat", fac) for fac in ("se", "sw") for n in ("Attack01", "Attack02", "Attack03", "Cast01")]
FACING_KEYS = [(n, f, fac) for fac in ("front", "sw", "ne", "back") for n, f in (("Idle", 0), ("Run", 8))]
CLOSE_KEYS = [("Attack01", "beat", "se"), ("Attack02", "beat", "se"), ("Attack03", "beat", "se"),
              ("Cast01", "beat", "se"), ("Cast02", "beat", "se"), ("Cast_Multishot", "beat", "se"),
              ("Cast_Multishot", 10, "side"), ("Dodge", 6, "side"), ("Hurt", 0, "se"), ("Death", 32, "front")]
BACK_CLIPS = ("Attack01", "Attack02", "Attack03", "Cast01", "Cast02", "Cast_Multishot", "Dodge", "Death")

HERO = HS.HeroDef(
    asset=RG.ASSET, token=RG.TOKEN, game_id="player_rogue",
    display_name={"zh-CN": "影刃", "en": "Shadowblade"},
    preview_dir="hero_rogue", module=RG, anims=RC.RogueAnims,
    items=[HS.Item(RG.DAGGER, "weapon_r", "dagger",
                   extra={"bladeCm": round((RG.BLADE1 - RG.BLADE0) * RG.U * 100.0, 1),
                          "note": "main hand; the off hand carries the mirrored SM_Hero_Rogue_Dagger_L"}),
           HS.Item(RG.DAGGER_L, "weapon_l", "dagger",
                   extra={"mirrorOf": RG.DAGGER, "note": "the main dagger mirrored (curve and poison edge) for the "
                                                         "off-hand hammer grip"}),
           HS.Item(RG.BOW, "weapon_l", "bow", kind="swap",
                   extra={"showNotify": "FX_BowShow", "hideNotify": "FX_BowHide", "replaces": RG.DAGGER_L,
                          "clips": ["Cast_Multishot"],
                          "note": "flash-bow (spec §3.3 NEW): hidden by default; on FX_BowShow hide the off-hand "
                                  "dagger and show this on weapon_l, on FX_BowHide swap back (fx.bowFlash); the string "
                                  "is runtime FX (fx.bowString)"}),
           HS.Item(RG.QUIVER, "quiver", "quiver", kind="body",
                   extra={"note": "hip quiver on the body socket `quiver` (pelvis), authored in the socket frame: "
                                  "attach with no extra transform"})],
    blob_radius=0.36, fx=FX, eye_socket="eyes", eye_glow=EYE_GLOW, clip_meta=clip_meta,
    qa_groups=QA_GROUPS, qa_pairs=QA_PAIRS, qa_limits=QA_LIMITS, qa_clip_limits=QA_CLIP_LIMITS,
    qa_head={"clips": ("Attack01", "Attack02", "Attack03", "Cast01"), "maxYaw": 14.0, "maxDown": 12.0},
    item_grips={RG.DAGGER: lambda co: -0.075 < co.y < 0.06 and math.hypot(co.x, co.z) < 0.03,
                RG.DAGGER_L: lambda co: -0.075 < co.y < 0.06 and math.hypot(co.x, co.z) < 0.03,
                RG.BOW: lambda co: abs(co.y) < 0.07 and math.hypot(co.x, co.z) < 0.045},
    game_keys=GAME_KEYS, contact_keys=CONTACT_KEYS, facing_keys=FACING_KEYS, close_keys=CLOSE_KEYS,
    back_clips=BACK_CLIPS, portrait_bust_z=0.98, portrait_zoom=(120, 376, 150, 406),
    verify_bones=("scarf_a_05", "scarf_b_05", "hood_tip_02", "tasset_0_01", "tasset_7_01"),
    verify_top=(1.60, 1.66), visibility=visibility, preview_extra=preview_extra,
    notes="Twin daggers: SM_Hero_Rogue_Dagger on weapon_r, its mirror SM_Hero_Rogue_Dagger_L on weapon_l. "
          "Cast_Multishot (R5 signature) swaps the off-hand dagger for SM_Hero_Rogue_Bow between its FX_BowShow / "
          "FX_BowHide notifies (itemAttachments kind swap); the quiver is a body item on the `quiver` socket. Dodge "
          "is a 360° tuck roll in place; the UE module lerps a mesh offset (2.6 tiles). Chain bones beyond SKEL_Human: "
          "scarf_a_01..05, scarf_b_01..05 (spine_03), hood_tip_01..02 (head), tasset_0..7_01 (pelvis: one hinge per "
          "split tasset, swung clear of the thighs in every key).",
)
