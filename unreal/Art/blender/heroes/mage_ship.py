"""Ship definition of the mage (``hero_ship.HeroDef``): items, FX block, QA groups / limits, review frames.

Run through ``mage.py`` (no flag = ship; ``--quick`` / ``--anims`` / ``--frames`` / ``--game`` for iteration).
"""
from __future__ import annotations

import math

from mathutils import Matrix, Vector

import hero_ship as HS
import mage as MG
import mage_clips as MC
from kit import review

# spec §3 general + §3.2 FX attachments (for the UE module; colours are the web's)
EYE_GLOW = {"color": "#9FE6FF", "radiusCm": 9.0, "alpha": 0.5, "coreColor": "#E8FBFF", "coreRadiusCm": 3.2,
            "portraitEyeRadiusCm": 5.9, "portraitEyeAlpha": 0.8,     # web per-eye glow r 2 u (UI portrait only)
            "coreAlpha": 0.55}
FX = {
    "classColor": "#9B59D6", "impactColor": "#C7A6FF", "spiritColor": "#A98BFF",
    # glowing eyes under the hood (web glow r 2 u #9FE6FF α .45 + .3·fx per eye): one card between the eyes on the
    # `eyes` socket (UE reads it as the hero's visorGlow card: facing fade, off late in Death)
    "visorGlow": {"socket": "eyes", "color": EYE_GLOW["color"], "radiusCm": EYE_GLOW["radiusCm"],
                  "alpha": EYE_GLOW["alpha"], "alphaPerFx": 0.3, "coreColor": EYE_GLOW["coreColor"],
                  "coreRadiusCm": EYE_GLOW["coreRadiusCm"], "coreAlpha": EYE_GLOW["coreAlpha"], "blend": "additive",
                  "falloff": [list(x) for x in review.GLOW_STOPS],
                  "facingFade": "clamp(2 * dot(headFwd, toCamera) + 0.3, 0, 1)", "offAtDeathFraction": 0.7},
    "bloom": {"source": "palette P.b emissive regions (eyes, clasp gem, crystal)",
              "lobesSigmaPx1080": [list(x) for x in review.BLOOM_LOBES], "strength": review.BLOOM_STRENGTH},
    # web smear along the crystal path (tip = the crystal, mid = 10 u up the staff)
    "attackTrail": {"color": "#A47BFF", "alpha": 0.5, "sockets": ["tip", "mid"], "samples": 7, "sampleMs": 12},
    # cast: the crystal flares (web glow r 6 + 8·fx u #A47BFF α .35 + .4·fx, core 2.5 + 3·fx u #9FE6FF) — the tip card
    # sits on the staff `tip` socket (= the crystal); the mid card is off
    "castBladeGlow": {"color": "#A47BFF", "radiusCm": 0.0, "alpha": 0.0, "tipColor": "#A47BFF", "tipRadiusCm": 41.0,
                      "tipAlpha": 0.75, "embers": {"count": 0, "color": "#9FE6FF"}},
    # not yet read by UAbyssVfxSystem (documented for it): the always-on crystal hum, orbiting motes, rune ring,
    # off-hand palm flare, the 120 ms release flash and the dodge motes (web drawFx :678-743)
    "crystalHum": {"itemSocket": "crystal", "color": "#A47BFF", "radiusCm": [18.0, 41.0], "alpha": [0.35, 0.75],
                   "coreColor": "#9FE6FF", "coreRadiusCm": [7.4, 16.3], "coreAlpha": [0.6, 0.95],
                   "note": "ranges = fx 0 .. 1 (idle fx .25 ± .15, attack / cast up to 1)",
                   "bob": {"amplitudeCm": 1.8, "perLoop": 2, "note": "the crystal floats: ±0.6 u twice per loop"}},
    "crystalMotes": {"count": 5, "color": "#9FE6FF", "ellipseCm": [16.3, 7.1], "when": "attack / cast with fx > 0.6"},
    "castRuneRing": {"socket": "fx_feet", "color": "#B89BFF", "radiusCm": [36.0, 44.0], "alpha": 0.75,
                     "innerRadiusCm": 27.0, "innerDashed": True, "innerSpinRad": 6.0},
    "palmFlare": {"socket": "fx_hand_l", "color": "#A47BFF", "radiusCm": [12.0, 27.0], "alpha": 0.6},
    "releaseFlash": {"color": "#B9A5FF", "ms": 120},
    "dodgeMotes": {"count": 4, "color": "#A47BFF", "alpha": 0.5},
    "heroHalo": {"color": "#FFEEDD", "radiusCm": 200},
}


def clip_meta(c, e: dict) -> None:
    if c.name == "Cast_Blizzard":
        e["signature"] = True
        e["skill"] = "blizzard"
    elif c.name == "Cast_Meteor":
        e["signature"] = True
        e["skill"] = "meteor"
    elif c.name == "Cast02":
        e["skills"] = ["ice_armor", "mana_shield", "teleport"]
    elif c.name == "Cast01":
        e["skills"] = ["fireball", "ice_arrow", "chain_lightning", "fire_wall", "freeze", "combustion",
                       "arcane_torrent"]


ITEM_VIEWS = {
    MG.STAFF: [("STAFF FRONT", Matrix.Rotation(math.radians(135), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X")),
               ("STAFF SIDE", Matrix.Rotation(math.radians(45), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"))],
    MG.CRYSTAL: [("CRYSTAL", Matrix.Rotation(math.radians(135), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"))],
}
MG.ITEM_VIEWS = ITEM_VIEWS

LEGS = ("leg_l", "leg_r", "boot_l", "boot_r", "boot_cuff_l", "boot_cuff_r")
QA_GROUPS = {
    "bodyNoGrip": ("!", ("hand_r", "thumb_r", "wrist_r")),
    "all": "*",
    "skirt": ("skirt",),
    "legs": LEGS,
    "sash": ("sash_tail0", "sash_tail1"),
    "torso": ("robe", "mantle", "clasp", "sash", "sash_knot"),
    "hoodTip": ("hood_tip",),
    "beard": ("beard",),
    "tome": ("tome", "tome_cord"),
}
QA_PAIRS = {
    "staffBody": ("item:" + MG.STAFF, "bodyNoGrip"),
    "crystalBody": ("item:" + MG.CRYSTAL, "all"),
    "crystalStaff": ("item:" + MG.CRYSTAL, "item:" + MG.STAFF),
    "skirtLegs": ("skirt", "legs"),
    "sashSkirt": ("sash", "skirt"),
    "hoodTipBody": ("hoodTip", "torso"),
    "beardBody": ("beard", "torso"),
    "tomeSkirt": ("tome", "skirt"),
}
QA_LIMITS = {"ground": -0.5, "staffBody": 0, "crystalBody": 0, "crystalStaff": 0, "skirtLegs": 6, "sashSkirt": 6,
             "hoodTipBody": 6, "beardBody": 6, "tomeSkirt": 8}
# Death (tall kneel → topple → face down): the robe's front panel lies flat on the floor under the prone knees, where
# the four-chain hem ring grazes the underside of the legs (facing the floor, hidden by the body: ≤ 48 tri pairs in the
# held pose); the sash tails and the tome settle onto the skirt as the body goes over
QA_CLIP_LIMITS = {"Death": {"ground": -4.0, "skirtLegs": 50, "sashSkirt": 20, "tomeSkirt": 14},
                  "Dodge": {"skirtLegs": 20}}

GAME_KEYS = [("Idle", 0, "se"), ("Run", 8, "se"), ("Attack01", "beat", "se"), ("Cast01", "beat", "se"),
             ("Cast_Blizzard", "beat", "se"), ("Cast_Meteor", "beat", "se"), ("Dodge", 8, "se"), ("Death", 40, "se")]
CONTACT_KEYS = [(n, "beat", fac) for fac in ("se", "sw") for n in ("Attack01", "Attack02", "Attack03", "Cast01")]
FACING_KEYS = [(n, f, fac) for fac in ("front", "sw", "ne", "back") for n, f in (("Idle", 0), ("Run", 8))]
CLOSE_KEYS = [("Attack01", "beat", "se"), ("Attack02", "beat", "se"), ("Attack03", "beat", "se"),
              ("Cast01", "beat", "se"), ("Cast02", "beat", "se"), ("Cast_Blizzard", "beat", "se"),
              ("Cast_Meteor", "beat", "se"), ("Dodge", 8, "side"), ("Hurt", 0, "se"), ("Death", 40, "side")]
BACK_CLIPS = ("Attack01", "Attack02", "Attack03", "Cast01", "Cast02", "Cast_Blizzard", "Cast_Meteor", "Dodge", "Death")

def portrait_glows(rig, meshes) -> list:
    """The crystal hum at idle strength (fx.crystalHum, fx ≈ .25) baked into the UI portrait (no bloom there)."""
    cr = next((m for m in meshes if m.name == MG.CRYSTAL), None)
    if cr is None:
        return []
    hum = FX["crystalHum"]
    return [{"at": cr.matrix_world.translation.copy(), "radiusCm": 24.0, "color": hum["color"], "alpha": 0.45,
             "core": 0.32}]


HERO = HS.HeroDef(
    asset=MG.ASSET, token=MG.TOKEN, game_id="player_mage",
    display_name={"zh-CN": "渊火术士", "en": "Abyssfire Arcanist"},
    preview_dir="hero_mage", module=MG, anims=MC.MageAnims,
    items=[HS.Item(MG.STAFF, "weapon_r", "staff",
                   extra={"lengthCm": round((MG.STAFF_UP + MG.STAFF_DOWN + 3.4 + 4.4) * MG.U * 100.0, 1)}),
           HS.Item(MG.CRYSTAL, "crystal", "crystal", kind="item", parent=MG.STAFF,
                   extra={"note": "floats in the staff's claw: attach to the staff socket `crystal`; bob ±1.8 cm "
                                  "twice per idle loop + a slow spin in the BP / material (web staff :132-142)"})],
    blob_radius=0.38, fx=FX, eye_socket="eyes", eye_glow=EYE_GLOW, clip_meta=clip_meta,
    qa_groups=QA_GROUPS, qa_pairs=QA_PAIRS, qa_limits=QA_LIMITS, qa_clip_limits=QA_CLIP_LIMITS,
    qa_head={"clips": ("Attack01", "Attack02", "Attack03", "Cast01"), "maxYaw": 14.0, "maxDown": 12.0},
    item_grips={MG.STAFF: lambda co: abs(co.y) < 0.07 and math.hypot(co.x, co.z) < 0.05},
    game_keys=GAME_KEYS, contact_keys=CONTACT_KEYS, facing_keys=FACING_KEYS, close_keys=CLOSE_KEYS,
    back_clips=BACK_CLIPS, portrait_bust_z=1.02, portrait_zoom=(120, 376, 150, 406),
    verify_bones=("skirt_f_02", "skirt_r_02", "sash_a_03", "sash_b_03", "hood_tip_02", "beard_02", "tome_01"),
    verify_top=(1.70, 1.77), portrait_glows=portrait_glows,
    notes="Dodge (low arcane skim) is in place; the UE module lerps a mesh offset (2.25 tiles). Signature casts keep "
          "Cast01's release (230 ms). The crystal is a child item of the staff (itemAttachments) — attach it to the "
          "staff's `crystal` socket. Death (web: drops to the knees, pitches forward face-down): buckle → tall kneel "
          "(the robe pooled round the knees) → topples forward over the knees → face down, the staff arm flung past "
          "the hood and the staff settling on the floor along the right side; the body ends ~1.3 m ahead of the root "
          "(in place, no root motion). Chain bones beyond SKEL_Human: skirt_f/b/l/r, sash_a/b, hood_tip, beard, tome, "
          "sleeve_l/r (bell-sleeve sag + slide, location keys).",
)
