"""The Chapter-1 NPC looks (spec art-inventory-ch1.md §5.2 exact hex values; DECISIONS I7 stash keeper; R2 heights).

Each :class:`NpcDef` = a ``folk.Look`` + its work loop (spec §5.3) + the web READY tweaks + presentation data
(game ids, display names, blob shadow, runtime FX cards, extra clips, idle-fidget gesture, portrait framing).
Web sources: ``src/graphics/sprites/npcs/{Blacksmith,Merchant,QuestElder,Stash,WanderingMerchant}.ts``,
``FieldNPCs.ts:9-38`` (herbalist, wanderer), ``EventNPCs.ts:10-93`` (lost traveller, escort merchant).
"""
from __future__ import annotations

from dataclasses import dataclass, field

from folk import Look


@dataclass
class NpcDef:
    id: str                       # asset token: SK_NPC_<id>, A_NPC_<id>_<Clip>, Previews/npc_<snake>
    snake: str                    # preview dir suffix
    game_ids: tuple
    display: dict
    look: Look
    work: str                     # Hammer | Count | Read | Guard | Lookout
    work_ms: float
    fidget: str                   # IdleNPC_Fidget gesture: stretch | mustache | beard | hair | hat | specs | look | straps
    gesture_hand: str = "l"       # the Talk / Alert hand (web: the off hand unless it holds an upright prop)
    extra_clips: tuple = ()       # escort: Idle, Walk, Run, Hurt, Death; rescue: Cower
    fx: dict = field(default_factory=dict)
    scenery: tuple = ()           # [(asset, offset (m, body space), yaw deg)]
    scenery_note: str = ""        # why the 3D placement deviates from the spec (manifest scenery[].note)
    blob_radius: float = 0.36
    notes: str = ""

    @property
    def asset(self) -> str:
        return f"SK_NPC_{self.id}"

    @property
    def token(self) -> str:
        return f"NPC_{self.id}"

    @property
    def preview_dir(self) -> str:
        return f"npc_{self.snake}"


COIN_GLINT = {"socket": "fx_hand_r", "color": "#FFD860", "radiusCm": 6.0, "alpha": 0.9,
              "riseCm": 16.0, "notifyOn": "FX_CoinToss", "notifyOff": "FX_CoinCatch",
              "note": "web: glow r 1.8 u #FFD860 a .9 at the near hand, 5 u above the palm x the pose fx (toss)"}

NPCS = [
    NpcDef(
        id="Blacksmith", snake="blacksmith", game_ids=("npc_blacksmith",), display={"zh": "铁匠", "en": "Blacksmith"},
        look=Look(skin="#C98B62", build="stout", hair=("#7A3A1C", "bald"), beard=("#9A4A22", "braided"),
                  top=("#5A5A62", "tunic", None), sleeves="#C98B62", apron="#6E4424", belt="#3A2414",
                  legs="#4A3A2C", boots="#2E2018", item="hammer", off="tongs", brow_k=1.25),
        work="Hammer", work_ms=1333.0, fidget="stretch", gesture_hand="l",
        fx={"anvilSparks": {"notify": "FX_HammerStrike", "atMs": 427.0, "socket": "strike",
                            "on": "SM_Prop_Camp_AnvilStump", "glow": "#FFA040", "sparks": "#FFD070",
                            "flashRadiusCm": 16.0, "flashAlpha": 0.7, "flashMs": 140.0,
                            "note": "spec §8.7 NS_NPC_AnvilSparks; previews draw the strike flash as a 16 cm #FFA040 "
                                    "card (core #FFD070) fading over 140 ms, 3 cm above the hot bar"},
            "hotBarGlow": {"item": "SM_NPCKit_Prop_Tongs", "socket": "glow", "color": "#FF8A3A", "radiusCm": 7.0,
                           "alpha": 0.45, "coreColor": "#FFD08A", "coreFrac": 0.35, "coreAlpha": 0.6,
                           "note": "the hot iron in the tongs' jaws (the forge's focal accent, spec §1.5): a steady "
                                   "glow card on the tongs' glow socket over the emissive bar"}},
        scenery=(("SM_Prop_Camp_AnvilStump", (0.12, -0.52, 0.0), 0.0),),
        scenery_note="spec §5.2 gives ≈ 35 cm forward / 17 cm to the front-left (web x +11 u, lateral −5.5 u); the "
                     "stout 3D smith's boots and belly reach into the stump there (body-anvil gap 0 cm at 35-42 cm "
                     "forward, 0.1-2.3 cm at 47 cm, measured on the skinned mesh over the Work_Hammer keys), so the "
                     "stump stands 52 cm forward, 12 cm left (gap >= 3.5 cm on every key); Work_Hammer's strike, "
                     "tongs and FX_HammerStrike socket are solved on this offset — place it exactly here",
        blob_radius=0.40,
        notes="stout, bald with a braided red beard; sleeveless soot-grey tunic, leather apron; hammer + tongs at the "
              "stump anvil (SM_Prop_Camp_AnvilStump at the scenery offset)"),
    NpcDef(
        id="Merchant", snake="merchant", game_ids=("npc_merchant",), display={"zh": "商人", "en": "Merchant"},
        look=Look(skin="#C79A70", build="stout", hair=("#3A2A1A", "short"), beard=("#3A2A1A", "mustache"),
                  hat=("cap", "#1A5A4A", "#D4AA30"), hat_tilt=16.0, visor_k=0.8,
                  top=("#1E4A70", "vest", "#D4AA30"), sleeves="#D8CCAA",
                  belt="#4A3015", legs="#4A3A2A", boots="#2A1A0A", off="pouch"),
        work="Count", work_ms=1600.0, fidget="mustache", gesture_hand="l", fx={"coinGlint": COIN_GLINT},
        blob_radius=0.40, notes="teal cap with a gold badge, blue vest over a cream shirt, mustache; fat purse"),
    NpcDef(
        id="QuestElder", snake="quest_elder", game_ids=("npc_quest_elder",),
        display={"zh": "村长", "en": "Village Elder"},
        look=Look(skin="#C49C78", build="slim", hair=("#B8B4AE", "long"), beard=("#D6D2CC", "long"),
                  top=("#5A5A28", "robe", "#C8960E"), mantle="#6E6E36", belt="#303015", legs="#3A3A1A",
                  boots="#2A1A0A", item="staff", off="scroll", brow_k=1.45, lids=0.30),
        work="Read", work_ms=2667.0, fidget="ponder", gesture_hand="l",
        fx={"orbGlow": {"socket": "orb", "item": "SM_NPCKit_Prop_Staff", "color": "#FFCF50", "radiusCm": 12.0,
                        "alpha": 0.30, "coreColor": "#E8FFD0", "coreRadiusCm": 3.0, "coreAlpha": 0.0,
                        "note": "web glow r 4 u #FFCF50 a .55 over the orb r 2.2 u #7FD6A0: a halo ~1.7x the orb "
                                "radius. 3D 12 cm a .30 additive, no core (the orb's own emissive highlight is the "
                                "hot spot): the orb body keeps its green at 1080p and the yellow reads round it (art "
                                "review: 11 cm a .5 + a pale core washed the orb to white)"}},
        blob_radius=0.36, notes="slim elder, long grey hair and beard, sage robe with gold trim, olive mantle; staff "
                                "with a glowing orb, scroll; cutscene speaker (portrait)"),
    NpcDef(
        id="PlainsHerbalist", snake="plains_herbalist", game_ids=("npc_plains_herbalist",),
        display={"zh": "平原药师", "en": "Plains Herbalist"},
        look=Look(skin="#E0B08A", build="slim", hair=("#7A4A26", "bun"), hat=("headband", "#5F8F3A", None),
                  top=("#6F8F4A", "tunic", "#D8C9A0"), apron="#E2D6B4", belt="#6A4A2A", legs="#4F5F3A",
                  boots="#3A2A1A", pack="#8A6A3A", pack_scale=0.85, item="pouch"),
        work="Count", work_ms=2000.0, fidget="hair", gesture_hand="l",
        fx={"herbGlow": {"socket": "fx_hand_r", "color": "#9FE07A", "radiusCm": 11.0, "alpha": 0.45,
                         "pulseMs": 2000.0, "pulse": 0.17, "note": "web glow r 3.5 ± 0.6 u #9FE07A a .45 over the hand"}},
        blob_radius=0.34, notes="herbalist: bun + green headband, sage tunic with a linen apron, small backpack, "
                                "pouch of herbs"),
    NpcDef(
        id="PlainsWanderer", snake="plains_wanderer", game_ids=("npc_plains_wanderer",),
        display={"zh": "流浪剑客", "en": "Wandering Swordsman"},
        look=Look(skin="#C8966C", build="slim", height=1.76, hair=("#1E1A1E", "ponytail"),
                  beard=("#1E1A1E", "stubble"), hat=("widebrim", "#C6A860", "#7A2A22"), hat_tilt=10.0,
                  top=("#2F3D5C", "coat", "#9A2A2A"), mantle="#9A2A2A", scarf=True, belt="#2A1C14",
                  legs="#262A36", boots="#1A140E", item="sword"),
        work="Guard", work_ms=2000.0, fidget="hat", gesture_hand="l",
        blob_radius=0.36, notes="tall slim swordsman: straw wide-brim hat with a red band, black ponytail, stubble, "
                                "indigo travel coat, red scarf; leans on his sword"),
    NpcDef(
        id="EscortTravelingMerchant", snake="escort_traveling_merchant",
        game_ids=("npc_escort_traveling_merchant",), display={"zh": "旅行商人", "en": "Traveling Merchant"},
        look=Look(skin="#C49C78", build="stout", hair=("#3F2D21", "short"), beard=("#3F2D21", "mustache"),
                  hat=("widebrim", "#6B3E39", "#28706D"), hat_tilt=13.0, brim_k=1.02, brim_curl=0.35,
                  top=("#6B3E39", "coat", "#28706D"), belt="#68472D",
                  legs="#354458", boots="#2A1E14", pack="#68472D", off="pouch"),
        work="Count", work_ms=2000.0, fidget="hat", gesture_hand="r",
        extra_clips=("Idle", "Walk", "Run", "Hurt", "Death"), fx={"coinGlint": COIN_GLINT},
        blob_radius=0.40, notes="escort quest q_escort_merchant_plains: walks / runs with the hero (Run ref 2.85 m/s), "
                                "flinches, can die"),
    NpcDef(
        id="RescueLostTraveler", snake="rescue_lost_traveler", game_ids=("npc_rescue_lost_traveler",),
        display={"zh": "迷路的旅人", "en": "Lost Traveler"},
        look=Look(skin="#C49C78", build="slim", hair=("#493326", "ponytail"), top=("#526B62", "coat", "#677D4D"),
                  belt="#674931", legs="#4A4F43", boots="#3A2A1C", pack="#674931", pack_scale=0.9),
        work="Lookout", work_ms=2000.0, fidget="look", gesture_hand="l", extra_clips=("Cower",),
        blob_radius=0.34, notes="rescue event: shades the eyes looking out (Work_Lookout); Cower while hostiles "
                                "are near (NEW); '!' marker overhead"),
    NpcDef(
        id="WanderingMerchant", snake="wandering_merchant", game_ids=("npc_wandering_merchant",),
        display={"zh": "流浪商人", "en": "Wandering Merchant"},
        look=Look(skin="#C49A72", build="normal", hair=("#5A4028", "short"), beard=("#5A4028", "short"),
                  hat=("widebrim", "#5A4A30", "#9A2A2A"), hat_tilt=15.0, brim_k=0.98, brim_curl=0.0, brim_droop=0.9,
                  top=("#3A4A30", "coat", "#B08A3A"), belt="#3A2414",
                  legs="#4A3A2A", boots="#2A1A0A", pack="#7A5A36", pack_scale=1.35, off="pouch"),
        work="Count", work_ms=1600.0, fidget="straps", gesture_hand="r", fx={"coinGlint": COIN_GLINT},
        blob_radius=0.38, notes="random-event peddler with a huge pack (pot, rug, bedroll); Talk while the shop is "
                                "open"),
    NpcDef(
        id="StashKeeper", snake="stash_keeper", game_ids=("npc_stash",),
        display={"zh": "仓库管理员", "en": "Stash Keeper"},
        look=Look(skin="#C2A07E", build="slim", hair=("#4A3D30", "bun"), top=("#3A1C4A", "robe", "#8A5AC0"),
                  belt="#2A1A3A", legs="#2A1A3A", boots="#1A1A2A", specs=True, keyring=True, off="book"),
        work="Read", work_ms=2667.0, fidget="specs", gesture_hand="r",
        blob_radius=0.34, notes="DECISIONS I7: the Chapter-1 camp stash keeper (web npc_stash): bun, round "
                                "spectacles, plum robe with arcane trim, key ring at the hip, inventory book"),
]

BY_ID = {n.id: n for n in NPCS}
BY_SNAKE = {n.snake: n for n in NPCS}


def find(name: str) -> NpcDef:
    k = name.strip()
    if k in BY_ID:
        return BY_ID[k]
    if k in BY_SNAKE:
        return BY_SNAKE[k]
    for n in NPCS:
        if k.lower() in (n.id.lower(), n.snake, *n.game_ids):
            return n
    raise KeyError(name)
