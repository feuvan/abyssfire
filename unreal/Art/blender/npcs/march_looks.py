"""The eleven march NPC looks (Docs/dark-remake/chapter1.md §3; implementation-plan.md §5 A9).

Each :class:`MarchNpcDef` is a ``looks.NpcDef`` whose look is a :class:`march_folk.MarchLook`: the **colours** are the
A1 colour script's (``march_palettes.NPC_LOOKS[<snake>]``, registered under ``npc.march.<snake>``; the re-dress never
changes them), the **shapes** (build, hair style, hat kind, props, dress pieces) are A9's. Nine re-dress the shipped
plains roles (``SK_NPC_<Id>_March``, themed game id ``npc_<id>@march``: the runtime resolves it before the plains
``npc_<id>`` in a ``march`` zone, ``kit.theme``), two are new (``SK_NPC_KnellChaplain`` / ``SK_NPC_CrowTrader``, plain
ids ``npc_knell_chaplain`` / ``npc_crow_trader``).

Work loops: the Captain and Corran lean on a sword (``Guard``), Hald hammers at the charred anvil (``Hammer``), Vira
keeps her ledger (``Read``), Tamsin leans on her grave spade (``Spade``), Marl holds a phial of black blood up to the
light of her lantern (``Vial``), Mag and the Relic Peddler toss a coin (``Count``), Aldwin tolls his handbell
(``Toll``, notify ``FX_HandbellRing``), Old Bern stands at his barrow (``Barrow``; escort Walk / Run lift it onto its
wheel and push it), the Bound Pilgrim looks out for the procession (``Lookout``; ``Cower`` while hostiles are near).
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

from looks import NpcDef  # noqa: E402
from march_folk import MarchLook  # noqa: E402
import march_palettes as MP  # noqa: E402

from kit import theme  # noqa: E402


def look(snake: str, **shape) -> MarchLook:
    """The A1 colours of ``snake`` + A9 shapes. The colour fields belong to ``march_palettes.NPC_LOOKS``: ``shape``
    may restyle hair / beard (same colour, another style) and add a hat accent, never recolour."""
    colours = dict(MP.NPC_LOOKS[snake])
    for k in ("skin", "top", "sleeves", "apron", "belt", "legs", "boots", "mantle", "pack"):
        if k in shape:
            raise ValueError(f"{snake}: colour field {k!r} belongs to march_palettes.NPC_LOOKS")
    for k in ("hair", "beard"):
        if k in shape and (colours.get(k) is None or shape[k][0].lower() != colours[k][0].lower()):
            raise ValueError(f"{snake}: {k} colour must stay {colours.get(k)}")
    if "hat" in shape:
        old, new = colours.get("hat"), shape["hat"]
        if old is None or new[:2] != old[:2] or (old[2] is not None and new[2] != old[2]):
            raise ValueError(f"{snake}: hat {new} must keep {old} (an accent may only be added)")
    colours.update(shape)
    return MarchLook(**colours)


@dataclass
class MarchNpcDef(NpcDef):
    pal_snake: str = ""           # march_palettes.NPC_LOOKS key → palette prefix npc.march.<pal_snake>
    base_id: str = ""             # the plains look this one re-dresses ("" for a new NPC)
    portrait: bool = True

    @property
    def prefix(self) -> str:
        return f"{MP.NPC_PREFIX}.{self.pal_snake}"


def ids(base: str) -> tuple:
    return tuple(theme.themed_ids([base], "march"))


LANTERN_GLOW = {"socket": "glow", "item": "SM_NPCKit_Prop_Lantern", "color": "#FFB060", "radiusCm": 13.0,
                "alpha": 0.32, "coreColor": "#FFE2B0", "coreFrac": 0.30, "coreAlpha": 0.45,
                "note": "Marl's lantern (chapter1.md §15: her dressing station is a refuge; the warm light is the "
                        "reserved #FFB060): a steady card on the lantern's glow socket over its emissive panes"}
BELL_TOLL = {"notify": "FX_HandbellRing", "socket": "bell", "item": "SM_NPCKit_Prop_Handbell",
             "note": "Aldwin's handbell stroke: an audio cue (a small bronze handbell, not the keepers' bell_toll) and "
                     "no VFX — distinct from the Corpseherd's FX_BellToll (her violet bell-toll bolt); no light (bronze, "
                     "never ember)"}

NPCS = [
    MarchNpcDef(
        id="QuestElder_March", snake="quest_elder_march", pal_snake="quest_elder", base_id="QuestElder",
        game_ids=ids("npc_quest_elder"), display={"zh": "瑟琳·哈洛队长", "en": "Captain Serin Harrow"},
        look=look("quest_elder", build="slim", hair=("#B8B4AE", "crop"), item="sword", brow_k=0.8, expr="grim_f",
                  jaw_k=0.86, lashes=True, lip=True, crop="pixie", nose_k=0.84,
                  dress=("halfplate", "gorget", "tabard", "sigil_cut", "burn_scar")),
        work="Guard", work_ms=2400.0, fidget="ponder", gesture_hand="l",
        blob_radius=0.36,
        notes="the Captain: dented half-plate (spaulders, vambraces, gorget) under the crusade tabard with the "
              "church's sigil cut out of its chest (a frayed hole onto dented plate), a grey pixie crop longer at the "
              "sides and nape, a woman's grim face (thin arched brows, lashes, a narrow jaw, a defined lip), a rough "
              "burn scar down the right side of the jaw and neck; leans on her sword"),
    MarchNpcDef(
        id="KnellChaplain", snake="knell_chaplain", pal_snake="knell_chaplain", base_id="",
        game_ids=("npc_knell_chaplain",), display={"zh": "奥德温修士", "en": "Brother Aldwin"},
        look=look("knell_chaplain", build="normal", hair=("#4A3D30", "bald"), item="handbell", brow_k=1.1,
                  lids=0.18, expr="weary", dress=("rope_belt", "ash_brow", "threadbare", "tonsure")),
        work="Toll", work_ms=2667.0, fidget="ponder", gesture_hand="l", fx={"handbellRing": BELL_TOLL},
        blob_radius=0.36,
        notes="the chaplain: a threadbare black cassock (ragged hem and cuffs, two sewn patches), a rope cincture, a "
              "thick grizzled monk's fringe, ash on the brow; tolls a bronze handbell (Work_Toll, FX_HandbellRing on "
              "each stroke), the free hand flat on his sternum"),
    MarchNpcDef(
        id="Blacksmith_March", snake="blacksmith_march", pal_snake="blacksmith", base_id="Blacksmith",
        game_ids=ids("npc_blacksmith"), display={"zh": "哈尔德·熔剑", "en": "Hald Swordmelter"},
        look=look("blacksmith", item="hammer", off="tongs", brow_k=1.25, expr="grim", dress=("name_tags",)),
        work="Hammer", work_ms=1333.0, fidget="stretch", gesture_hand="l",
        fx={"anvilSparks": {"notify": "FX_HammerStrike", "atMs": 427.0, "socket": "strike",
                            "on": "SM_Prop_March_AnvilStump", "glow": "#FFA040", "sparks": "#FFD070",
                            "flashRadiusCm": 16.0, "flashAlpha": 0.7, "flashMs": 140.0,
                            "note": "as the plains smith (the forge is one of the march's refuges: warm light "
                                    "allowed, chapter1.md §15)"},
            "hotBarGlow": {"item": "SM_NPCKit_Prop_Tongs", "socket": "glow", "color": "#FF8A3A", "radiusCm": 7.0,
                           "alpha": 0.45, "coreColor": "#FFD08A", "coreFrac": 0.35, "coreAlpha": 0.6,
                           "note": "the hot iron in the tongs' jaws (the forge's focal accent)"}},
        scenery=(("SM_Prop_March_AnvilStump", (0.12, -0.52, 0.0), 0.0),),
        scenery_note="the plains smith's solved offset (52 cm forward, 12 cm left): Work_Hammer's strike, tongs and "
                     "FX_HammerStrike socket are solved on it — place the charred stump exactly here",
        blob_radius=0.40,
        notes="Hald: stout, bald, a dark braided beard, soot-grey tunic, leather apron; his two sons' bronze "
              "name-tags on a cord round his neck; hammer + tongs at the charred anvil stump"),
    MarchNpcDef(
        id="Merchant_March", snake="merchant_march", pal_snake="merchant", base_id="Merchant",
        game_ids=ids("npc_merchant"), display={"zh": "军需官薇拉", "en": "Quartermaster Vira"},
        look=look("merchant", hair=("#3A2A1A", "bun"), off="ledger", expr="flat", dress=("tally_cord",)),
        work="Read", work_ms=2667.0, fidget="stretch", gesture_hand="r",
        blob_radius=0.40,
        notes="Vira: stout, hair in a bun, drab army vest with brass edging over greyed linen; keeps the ration "
              "ledger (Work_Read), a knotted tally cord at her hip"),
    MarchNpcDef(
        id="StashKeeper_March", snake="stash_keeper_march", pal_snake="stash_keeper", base_id="StashKeeper",
        game_ids=ids("npc_stash"), display={"zh": "守冢人塔姆欣", "en": "Tamsin the Gravewarden"},
        look=look("stash_keeper", build="slim", hair=("#4A3D30", "crop"), item="spade", expr="weary", lids=0.15,
                  dress=("crypt_keys",)),
        work="Spade", work_ms=2667.0, fidget="look", gesture_hand="l",
        blob_radius=0.34,
        notes="Tamsin: a grave-digger's long coat, a big ring of crypt keys at the left hip; leans on her grave spade "
              "(Work_Spade: both hands stacked on its grip, a weary breath, a look over the graves) — apart from "
              "Vira's ledger loop at the same camp"),
    MarchNpcDef(
        id="PlainsHerbalist_March", snake="plains_herbalist_march", pal_snake="plains_herbalist",
        base_id="PlainsHerbalist", game_ids=ids("npc_plains_herbalist"),
        display={"zh": "军医玛勒", "en": "Mother Marl"},
        look=look("plains_herbalist", build="slim", pack_scale=0.85, item="vial", off="lantern", expr="grim",
                  dress=("rolled_sleeves", "gloves", "apron_stains")),
        work="Vial", work_ms=2400.0, fidget="stretch", gesture_hand="l", fx={"lanternGlow": LANTERN_GLOW},
        blob_radius=0.34,
        notes="Mother Marl: grey bun under a drab headband, sleeves rolled above the elbow, stained leather gloves "
              "and apron; holds a phial of black blood up to her lantern's light (the dressing station's warm "
              "light); she greets and talks with the lantern hand (the phial hand's grip swung its forearm across "
              "her face in the raised greeting)"),
    MarchNpcDef(
        id="PlainsWanderer_March", snake="plains_wanderer_march", pal_snake="plains_wanderer",
        base_id="PlainsWanderer", game_ids=ids("npc_plains_wanderer"),
        display={"zh": "逃兵柯兰", "en": "Corran the deserter"},
        look=look("plains_wanderer", build="slim", hair=("#1E1A1E", "short"), item="sword_notched", expr="haunted",
                  dress=("gambeson", "insignia_torn")),
        work="Guard", work_ms=2000.0, fidget="look", gesture_hand="l",
        blob_radius=0.36,
        notes="Corran: a slate hood and cowl over the deserters' quilted gambeson, the company badge torn off the "
              "left sleeve, stubble, a looted pack; leans on his old sword (notched, the point chipped, the gilt gone: "
              "SM_NPCKit_Prop_SwordNotched, not the Captain's plains sword) and glances over his shoulders"),
    MarchNpcDef(
        id="CrowTrader", snake="crow_trader", pal_snake="crow_trader", base_id="",
        game_ids=("npc_crow_trader",), display={"zh": "鸦嫂玛格", "en": "Mag Crowe"},
        look=look("crow_trader", build="normal", hair=("#1E1A1E", "braid"), off="pouch", expr="sly",
                  jaw_k=0.9, lashes=True, lip=True, nose_k=0.9, dress=("feather_collar", "slung_boots")),
        work="Count", work_ms=1600.0, fidget="stretch", gesture_hand="r", fx={},
        blob_radius=0.38,
        notes="Mag Crowe: long black hair in a braid down her back (her face and the boots clear of it), a black "
              "coat under a collar of flat crow feathers fanned past the "
              "shoulders, a pair of dead men's boots tied at the shafts and slung over her left shoulder (one on "
              "the chest in profile, one on the back, dark soles); tosses a coin from her purse (no glint: warm "
              "light is the refuges', chapter1.md §15)"),
    MarchNpcDef(
        id="EscortTravelingMerchant_March", snake="escort_traveling_merchant_march",
        pal_snake="escort_traveling_merchant", base_id="EscortTravelingMerchant",
        game_ids=ids("npc_escort_traveling_merchant"), display={"zh": "掘墓人伯恩", "en": "Old Bern the Gravedigger"},
        look=look("escort_traveling_merchant", build="slim", item="barrow", expr="weary", lids=0.2, brow_k=1.3,
                  dress=("earth_stains",)),
        work="Barrow", work_ms=2400.0, fidget="beard", gesture_hand="l",
        extra_clips=("Idle", "Walk", "Run", "Hurt", "Death"),
        blob_radius=0.38,
        notes="Old Bern: white hair and a long white beard, an earth-stained coat; pushes a barrow of shrouded dead "
              "(SM_NPCKit_Prop_Barrow on the socket `barrow`, bone barrow_01 at the wheel's axle: Walk / Run pitch it "
              "up onto its wheel, the legs off the ground, the wheel built into his mesh spinning on barrow_wheel_01) "
              "— both hands on its handles in every clip but the greeting / talk hand and the fidget; Death: he "
              "sinks to his knees over the handles"),
    MarchNpcDef(
        id="RescueLostTraveler_March", snake="rescue_lost_traveler_march", pal_snake="rescue_lost_traveler",
        base_id="RescueLostTraveler", game_ids=ids("npc_rescue_lost_traveler"),
        display={"zh": "被缚的朝圣者", "en": "the Bound Pilgrim"},
        look=look("rescue_lost_traveler", build="slim", expr="fear", dress=("rope_belt", "wrist_rope", "pilgrim_cloak")),
        work="Lookout", work_ms=2000.0, fidget="look", gesture_hand="l", extra_clips=("Cower",),
        blob_radius=0.34,
        notes="the Bound Pilgrim: undyed wool robe, a dusty pilgrim's cloak to mid-back with its hood bunched at the "
              "neck, a rope cincture, cut rope at both wrists; looks out after the procession (Work_Lookout), Cower "
              "while hostiles are near"),
    MarchNpcDef(
        id="WanderingMerchant_March", snake="wandering_merchant_march", pal_snake="wandering_merchant",
        base_id="WanderingMerchant", game_ids=ids("npc_wandering_merchant"),
        display={"zh": "圣骨贩子", "en": "the Relic Peddler"},
        look=look("wandering_merchant", build="normal", hat=("widebrim", "#5A4A30", "#B08A3A"), hat_tilt=15.0,
                  brim_k=0.98, brim_curl=0.0, brim_droop=0.9, pack_scale=1.35, expr="sly", dress=("relic_tray",)),
        work="Count", work_ms=1600.0, fidget="straps", gesture_hand="r", fx={},
        blob_radius=0.38,
        notes="the Relic Peddler: a drooping felt hat with a brass band, the huge pack, a tray of reliquary wares on "
              "black cloth before his belly (bone fragments bound in cord, a finger bone in a brass-wire cage, "
              "wax-sealed ossuary boxes, a knuckle on a cord; no skull); flips a coin over it (no glint: a gold "
              "sparkle in the field reads as loot)"),
]

BY_ID = {n.id: n for n in NPCS}
BY_SNAKE = {n.snake: n for n in NPCS}


def find(name: str) -> MarchNpcDef:
    k = name.strip()
    if k in BY_ID:
        return BY_ID[k]
    if k in BY_SNAKE:
        return BY_SNAKE[k]
    for n in NPCS:
        if k.lower() in (n.id.lower(), n.snake, n.pal_snake, n.base_id.lower(), *n.game_ids):
            return n
    raise KeyError(name)
