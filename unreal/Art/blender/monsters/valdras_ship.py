"""Ship of 焚旗元帅·瓦德拉斯 Lord-Marshal Valdras (``SK_Mon_Valdras`` + ``SM_MonWeapon_OathSword`` + the portrait):
``mship.MonsterShip`` — bake, FBX (SK, LOD1, one ``A_Mon_Valdras_<Clip>`` per clip, the sword SM), manifest (merging),
per-frame QA, the ink gate (boss class 3.5 px, gate 3.0), the re-import check and the review set in
``Art/Previews/mon_valdras/``. Run through ``valdras.py`` (``main``)."""
from __future__ import annotations

import math
import sys

import bpy

from mathutils import Vector

from kit import mesh as M, rig as R, scene

import valdras as VD
import valdras_clips as VC

# ── QA (cloth × body, sword × body / standard, the pole × the head and arms, tabard × legs, ground) ──────────────
GRIP_PARTS = ("fist", "thumb", "knuckles", "cuff")
UPPER = ["helm", "plume", "plume_ash", "plume_holder", "gorget", "pauldron", "lame2", "lame3", "haute",
         "haute_rim", "rerebrace", "couter", "couter_wing", "vambrace", "cuff", "fist", "thumb", "knuckles", "ua",
         "visor", "visor_lip", "visor_frame", "brow", "ridge"]
LEGS = ["cuisse", "poleyn", "poleyn_wing", "greave", "sabaton", "thigh"]
QA_GROUPS = {"flag": list(VD.FLAG_PARTS), "pole": ["pole", "pole_collar", "crossbar", "crossbar_knob", "finial"],
             "upper": UPPER, "legs": LEGS, "tabard": ["tabard_skirt", "tabard_hem"],
             "standard": list(VD.FLAG_PARTS) + ["pole", "pole_collar", "crossbar", "crossbar_knob", "finial"]}
QA_PAIRS = {"flagBody": ("flag", "body_nf_std"), "poleUpper": ("pole", "upper"), "tabardLegs": ("tabard", "legs")}


def sword_grip(co) -> bool:
    """Sword faces in or welded to the fists (the grip, the melt over the cross and ricasso, the pommel under the
    left hand at attention): the QA baseline."""
    return (VD.SW_POMMEL_Y - 0.05 < co.y < 0.26) and abs(co.x) < 0.08 and abs(co.z) < 0.11


def budget_record(ms) -> dict:
    import mship
    rec = {
        "spec": f"monsters.md §2.6 Valdras: LOD0 <= {VD.LOD0_MAX} toon triangles, LOD1 <= {VD.LOD1_MAX} (a boss "
                "exception above the family's 9 k; the standard's cloth is the biggest single part); hull <= 0.75 x "
                "toon; toon = every toon-lit slot (slot 0 + the cloth slot 2); the oath-sword is its own static mesh",
        "lod0": mship.tri_record(ms.body),
        "items": {it.asset: {"toon": M.tri_count(it.obj, 0), "outline": M.tri_count(it.obj, 1)} for it in ms.items},
        "weightedBones": len({g.name for g in ms.body.vertex_groups}),
        "pass": mship.toon_tris(ms.body) <= VD.LOD0_MAX and (ms.lod1 is None or mship.toon_tris(ms.lod1) <= VD.LOD1_MAX)
                and M.tri_count(ms.body, 1) <= 0.75 * mship.toon_tris(ms.body),
    }
    if ms.lod1 is not None:
        rec["lod1"] = mship.tri_record(ms.lod1)
    return rec


PORTRAIT_TOP = 0.42    # frame cap above the head centre: the bust, not the standard (the banner stays behind)
PORTRAIT_ZOOM = (150, 406, 150, 406)    # the 2x defect crop (y0, y1, x0, x1): the helm, the slit, the plume


def portrait(ms, rv, actions) -> str:
    """``T_UI_Portrait_Valdras`` (boss intro cs_boss_valdras, cs_rf_marshal_falls). Review round 2: a 3/4 helm and
    one pauldron, low camera, the charred banner edge and the scorched palm behind; the plate in the darkened
    swatches with its violet seams; the plume char-black, ash-tipped; no foreground fist (the left hand rests on
    the grip by the melted cross, below the frame). The slit is a **portrait variant** of the visor (a third of the
    game slit's height, graded: dark frame → violet lips → the pale marker → a hot core line; the 4 cm game slit
    stays: it is the 3-px marker at the W1 camera) under a soft additive halo (the runtime visorGlow's colours, no
    flat white core). The framing set is capped at ``PORTRAIT_TOP`` above the head centre."""
    import mship
    rig = ms.A.rig
    kit = ms.A.kit

    def sock(rig_, nm):
        so = next(x for x in rig_.sockets if x["name"] == nm)
        b = rig_.obj.data.bones[so["bone"]]
        pb = rig_.obj.pose.bones[so["bone"]]
        return rig_.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(so["pos"])))

    def on_helm(rig_, a_deg, out=0.012):
        """A point on the slit line (rest pose, ``a_deg`` round the helm) carried by the posed head bone."""
        b = rig_.obj.data.bones["head"]
        pb = rig_.obj.pose.bones["head"]
        p = kit.helm.point(kit.hc + VD.VISOR_Z, a_deg, out)
        return rig_.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ p))

    def glows(rig_):
        # round 3: the halo follows the slit — a wide violet wash, a pale bloom along the burning middle (cards at
        # 0 and ±22°, so it is longer than it is tall) and a small hot centre; the ends stay violet
        wp = sock(rig_, "visor")
        cards = [{"at": wp, "radiusCm": 24.0, "color": VD.SEAM_HEX, "alpha": 0.34, "core": 0.0}]
        cards += [{"at": on_helm(rig_, a), "radiusCm": 12.0, "color": VD.SEAM_HEX, "alpha": 0.22, "core": 0.0}
                  for a in (-26.0, 26.0)]
        cards += [{"at": on_helm(rig_, a), "radiusCm": 7.0, "color": "#D08CFF", "alpha": 0.26, "core": 0.0}
                  for a in (-14.0, 0.0, 14.0)]
        cards.append({"at": on_helm(rig_, 0.0), "radiusCm": 3.5, "color": "#F4ECFF", "alpha": 0.30, "core": 0.0})
        return cards
    orig = mship.review.mesh_points

    def capped(objs):
        pts = orig(objs)
        top = sock(rig, "fx_head").z + PORTRAIT_TOP
        return pts[pts[:, 2] < top]
    kit.portrait_slit = True
    try:
        pbody = kit.build(ms.pal, name=f"{VD.ASSET}_PortraitMesh")
    finally:
        kit.portrait_slit = False
    body0 = ms.body
    body0.hide_render = True
    ms.body = pbody
    mship.review.mesh_points = capped
    try:
        return mship.portrait_shot(ms, rv, VC.portrait_pose(ms.A), VD.PORTRAIT, eye_glow=None, glows=glows,
                                   yaw=-100.0, bust_below_head=0.30, bust_radius=0.42, pitch=-5.0,
                                   zoom=PORTRAIT_ZOOM)
    finally:
        mship.review.mesh_points = orig
        ms.body = body0
        body0.hide_render = False
        me = pbody.data
        bpy.data.objects.remove(pbody, do_unlink=True)
        bpy.data.meshes.remove(me)


def qa_setup(lod1: bool = True):
    import mship
    B = VD.build_all(lod1=lod1)
    kit, body = B["kit"], B["body"]
    rig = kit.rig
    A = VC.anims(kit)
    R.attach_to_bone(B["sword"], rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": B["sword"]})
    cs = VC.clips(A)
    items = [mship.Item(B["sword"], VD.SWORD, "weapon_r", "greatsword", sockets=B["sword_socks"],
                        extra={"lengthCm": round((VD.SW_TIP - VD.SW_POMMEL_Y + 0.04) * 100.0, 1), "twoHanded": True,
                               "leftGripCm": round(VD.SW_LEFT_Y * 100.0, 1),
                               "note": "monsters.md §2.6: the two-handed oath-sword (#6A6D73) whose grip has melted into "
                                       "his gauntlet — the melt is modelled on the item round the right fist, so the "
                                       "sword never leaves weapon_r; the left fist rides the grip (two_y) or the pommel"},
                        grip=sword_grip, preview_up="weapon_flat")]
    f = scene.ms_to_frame
    body_nf = mship.parts_except(body, ())
    body_nf = [n for n in body_nf if VD.GB.lod_base(n) not in GRIP_PARTS]
    body_nf_std = [n for n in mship.parts_except(body, ()) if VD.GB.lod_base(n) not in QA_GROUPS["standard"]]
    body_nf_sw = [n for n in body_nf if VD.GB.lod_base(n) not in QA_GROUPS["standard"]]
    fx = {
        "visorGlow": VD.VISOR_FX,
        "telegraph": {"normal": "#FFE8DC", "elite": "#D11F2E", "untilMs": VC.windup(VC.CONTACT_MS),
                      "note": "dark remake: bone fill / crimson rim (monsters.md §1.2); specials wind up >= 700 ms "
                              "(chapter1.md §11) — each clip's WindupEnd / Contact notifies"},
        "attackTrail": {"color": VD.SEAM_HEX, "alpha": 0.45, "sockets": ["tip", "mid"], "samples": 7, "sampleMs": 13,
                        "item": VD.SWORD, "note": "monsters.md §2.6: attackTrail breach-violet (never ember)"},
        "standard": {"sockets": ["standard_top", "banner", "standard_root"], "flareClip": "Roar",
                     "flareMs": VC.ROAR_FX, "igniteNote": "P3 No Retreat: the standard ignites with breach fire "
                                                         "(violet, A12) on the banner socket at Roar's FX_Roar",
                     "droopClip": "Death", "droopMs": 1450.0, "droopDeg": VC.DROOP_DEG,
                     "droopNote": "the pole is one rigid piece pivoting at the rupture (standard_01; standard_02 "
                                  "never turns against it): Death tips it 36 deg back and to his left, the crossbar "
                                  "tilting ~22 deg, the banner slack and plumb under it (held in the cutscene pose)"},
        "seams": {"color": VD.SEAM_HEX, "core": "#F0E0FF", "note": "the plate's seams are emissive palette swatches "
                                                                  "(valdras.seam / seam_core): they bloom on desktop"},
    }
    ms = mship.MonsterShip(
        asset=VD.ASSET, token=VD.TOKEN, preview_dir=VD.PREVIEW, pal=B["pal"], A=A, body=body, items=items, clips=cs,
        outline_class=VD.OUTLINE_CLASS, game_ids=VD.GAME_IDS, blob_radius=VD.BLOB, height=VD.CROP_H, lod1=B["body1"],
        display=VD.DISPLAY, fx=fx, glows=VD.visor_glows(rig), portrait=portrait,
        qa_groups={"body_nf": body_nf, "body_nf_std": body_nf_std, "body_nf_sw": body_nf_sw, **QA_GROUPS},
        qa_pairs={"swordBody": ("item:" + VD.SWORD, "body_nf_sw"), "swordStd": ("item:" + VD.SWORD, "standard"),
                  **QA_PAIRS},
        qa_limits={"ground": -0.5, "swordBody": 0, "swordStd": 0, **{k: 0 for k in QA_PAIRS}},
        qa_clip_limits={"Cast01": {"ground": -35.0}},
        native_sheets=True,
        game_keys=[("Idle", 0, "se"), ("Walk", 10, "se"), ("Attack01", f(VC.windup(VC.CONTACT_MS)), "se"),
                   ("Attack01", f(VC.CONTACT_MS), "se"), ("Attack01", f(VC.CONTACT_MS), "sw"),
                   ("Attack02", f(VC.SWEEP_CONTACT), "se")],
        contact_keys=[("Cast01", f(VC.BRAND_CONTACT), "se"), ("Roar", f(VC.ROAR_FX), "se"),
                      ("Lunge", f(VC.LUNGE_CONTACT), "se"), ("Death", f(VC.DEATH_MS), "se"), ("Hurt", 5, "se"),
                      ("Stun", 24, "se")],
        close_keys=[("Attack01", f(VC.windup(VC.CONTACT_MS)), "se"), ("Attack01", f(VC.CONTACT_MS), "se"),
                    ("Attack01", f(VC.CONTACT2_MS), "se"), ("Attack02", f(VC.SWEEP_CONTACT), "se"),
                    ("Cast01", f(VC.windup(VC.BRAND_CONTACT)), "se"), ("Cast01", f(VC.BRAND_CONTACT), "side"),
                    ("Roar", f(VC.ROAR_FX), "front"), ("Lunge", f(VC.LUNGE_CONTACT), "side"),
                    ("Death", f(VC.DEATH_MS), "se"), ("Rise", 60, "se")],
        back_clips=("Attack01", "Attack02", "Cast01", "Roar", "Death"),
        skeleton_bones_required=("root", "pelvis", "head", "weapon_r", "weapon_l", "tabard_f_02", "tabard_b_02",
                                 "standard_01", "standard_02", "flag_01", "flag_03", "ik_foot_l", "ik_hand_gun"),
        lod_note="LOD1: the same kit at a collapse ratio iterated to <= 4 k toon (the visor slit, seams and cracks built "
                 "exact); rivets, breaths, roots, the banner's sigil and burns, sabaton lames and trims are LOD0 only.",
        extra={"refSpeedCmS": round(VD.REF_SPEED * 100.0, 1), "attackStyle": "two-handed", "family": "unburied",
               "storyBoss": True, "marker": {"swatches": list(VD.MARKER),
                                             "note": "the single visor slit #E8D8FF (monsters.md §1.2 / §2.6)"},
               "eliteMarker": "none",
               "eliteMarkerNote": "story boss: the visor slit is his marker. GameData gives valdras elite: true, but "
                                  "monsters.md §2.6 bans a crown on him (no face, no crown, no skull, no throne) and "
                                  "fx_overhead projects onto the standard's pole at the W1 camera: the runtime skips "
                                  "the overhead elite marker (SM_FX_EliteCrown_March / SM_FX_EliteSigil) for an "
                                  "asset with eliteMarker none (core lane: AbyssWorldBuilder::UpdateElite). If one "
                                  "must ever show, attach it to fx_elite (head, 36 cm forward of and 70 cm over the "
                                  "head centre: >= 6.6 px clear of helm, plume, pole and banner in se / sw / ne / nw "
                                  "at the W1 camera, 1080p)",
               "scaleNote": "monsters.md 'Scale x1.4' is modelled into the mesh (manifest scale 1.0; the core sets no "
                            "visual scale for the story boss): 2.10 m to the bare crown, 2.13 m to the helm (a head and "
                            "a half over the 1.76 m hero, chapter1.md §11), x1.4 the hero's bulk (0.82 m pauldron span), "
                            "~2.65 m to the standard's finial (lowered 32 cm in review round 2; monsters.md 'about "
                            "2.4 m with the standard')",
               "skeletonNote": "SKEL_Human + tabard_f/b_01..02 (the warrior's / Unburied names, pelvis), standard_01..02 "
                               "(spine_03: the pole grown through his back) and flag_01..03 (standard_02: the banner "
                               "cloth) — new names, so the shared skeleton merges without a hierarchy clash",
               "materialNote": f"slot 2 {VD.CLOTH_MATERIAL}: the palette toon (M_AF_Toon) with RimOverride 1, "
                               f"RimOverrideColor {VD.CLOTH_RIM_HEX}, RimOverrideAlpha {VD.CLOTH_RIM_ALPHA} on the "
                               "banner and tabard faces (the cream rim lit the dark crimson cloth pale pink at grazing "
                               "angles); RimOverrideColor is sRGB (see README §23's importer note)",
               "notes": "Act boss of Chapter 1 (storyBoss). Clips in place (Walk play rate = v / ref). Idle = at attention, "
                        "the sword planted. Attack01 = the two-hit combo (W1 damage at Contact 650; Contact2 1000 for "
                        "W2). Attack02 = Banner Sweep, Cast01 = Oath-Brand slam, Lunge = P3 charge: [W2] skills with "
                        "WindupEnd / Contact notifies (windups >= 700 ms). Roar = boss intro + phase changes (the "
                        "standard flares). Death kneels (both knees, sword planted, helm bowed; the standard droops at "
                        "Droop 1450); Rise = Death reversed (his spawn). Stun loops."},
    )
    ms.extra["budget"] = budget_record(ms)
    return ms


def main(args) -> int:
    import mship
    import unburied as UB
    UB.march_stage()
    ms = qa_setup(lod1=True)
    res = mship.ship(ms, previews="--no-previews" not in args, verify="--no-verify" not in args)
    rc = mship.main_result(res)
    b = ms.extra["budget"]
    print(f"[budget] LOD0 toon {b['lod0']['toon']} hull {b['lod0']['outline']}; LOD1 "
          f"{b.get('lod1', {}).get('toon')} / {b.get('lod1', {}).get('outline')}; pass {b['pass']}")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
