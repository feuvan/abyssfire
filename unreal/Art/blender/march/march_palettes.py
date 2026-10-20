"""焚境 · The Burning March: palette families and theme variants (package A1, Docs/dark-remake/implementation-plan.md §5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/march/march_palettes.py
        (no flag)            register every swatch below (append-only), write the atlases + manifest palette entries
        --check              rules + capacity on a scratch copy of the registries (writes nothing)
        --families=Monsters_March,NPC   only these families · --list   every name → swatch index after the run

This file is the **colour script** of the march theme, written down as swatches so every Wave-1 generator draws from
one registry: ``Monsters_March`` (A2-A8 monsters, the elite marker), ``Props_March`` (A10 camp kit, set pieces,
story / lore / interactive props, rocks), ``Foliage_March`` (A10 dead foliage) and the march NPC looks **appended to
``NPC``** (A9; the registry is append-only, so the plains NPCs keep their swatches). Heroes, weapons, pickups and pets
keep their families: ember orange is theirs (DR5). Sources: monsters.md §1.2 / §2 (exact briefs), chapter1.md §2-3,
§9-12, §15 (hub, NPCs, set pieces, mood), story-bible.md tone pillars 4-6.

**Rules the swatches follow** (``check`` enforces the gating ones):

* *Darkness you can read.* Low base values and a deeper shade band (shadowAmt .48-.55 where the plains used .42:
  monsters.md §5 "palette shadow mixes, not a renderer change"), but a light band that clears the march ground: every
  body swatch's light tone sits ≥ 15 L* above the darkest walkable reference tile (``MARCH_GROUND`` dirt, L* 21.8),
  the darkest-tile gate's lit-side rule (darkgate.py). Dark cloth (base L* < 25) takes the ``dark`` preset: a generous
  lightAmt (.36) so its lit band still reads.
* *One marker per hostile family*: violet pinpoint eyes ``#D08CFF`` (Unburied, Corpseherd; Valdras's visor ``#E8D8FF``;
  Breach-blood's sparks / fissures), bone-yellow eyes ``#EDE6B0`` (Gravegnawers), crimson cloth ``#7A1C1E``-``#A3221E``
  (the Living: no glow). Marker swatches are named ``*.eye`` / ``*.marker`` so darkgate and the hue audit find them;
  ship scripts still put them in the manifest ``"marker": {"swatches": [...]}``.
* *Ember is reserved* (hue 15-35°, sat > .6): never an enemy emissive or eye (hue_audit ``ember``); in Props_March
  only the refuges (hearth, Hald's forge, the bell-frame lantern, Crowmarket's cookfire, chapter1.md §15),
  interaction (lore glyphs, the strongbox seam, the handbell puzzle) and the Abyssfire (the seal shard's brand lines).
  Breach fire is violet ``#B02FD0`` round near-black ``#1A0820``. Telegraph crimson ``#D11F2E`` / bone ``#FFE8DC`` are
  decal colours (A12), never palette swatches.
* *Aftermath, not gore*: no blood red on bodies; maws and wounds are near-black, breach-blood is tar ``#2A0E14``.
* *Hierarchy* (spec §1.5): monsters' identity colours and interactive props out-chroma decor; NPCs stay below the
  heroes; foliage is the quietest family (the dull poppy is its only colour).
"""
from __future__ import annotations

import colorsys
import json
import os
import shutil
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parent
for _p in (BLENDER, BLENDER / "npcs"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

# ── reserved colours (story-bible DR5, monsters.md §1.2, chapter1.md §15) ──────────────────────────────────────
EMBER = "#FF7A2A"            # hero, Abyssfire, loot, refuges, interaction (hue 15-35°, sat > .6)
WARM_LIGHT = "#FFB060"       # the only warm light in the zone: hearth, forge, bell-frame lantern, Crowmarket cookfire
HERO_HALO = "#FFE2C0"        # the hero's own light
TELEGRAPH_RIM = "#D11F2E"    # enemy ground telegraph rim (decal only, A12)
TELEGRAPH_FILL = "#FFE8DC"   # … and its bone fill
BREACH = "#B02FD0"           # breach-violet: enemy magic, breach fire, the breach glow
BREACH_CORE = "#1A0820"      # … round near-black cores
BREACH_PIN = "#D08CFF"       # violet pinpoints: Unburied / Corpseherd eyes, clot sparks
BREACH_HOT = "#F0E0FF"       # hottest violet-white: Valdras's seam cores
VISOR = "#E8D8FF"            # Valdras's single visor slit (manifest visorGlow)
BONE_YELLOW = "#EDE6B0"      # Gravegnawer eyes (beasts, not breach-touched)
CRIMSON = ("#A3221E", "#C0283A")   # crusade tabards, the oath-scar, Harke's scarf, danger UI
MANA_SHIELD = "#8A5CFF"      # the mage's mana shield: breach-violet keeps ≥ 30° of hue from it

# Reference ground for the march presentation theme (a proposal for A10's terrain style ``march``; terrain keys of
# the map: 0 grass, 1 dirt, 2 stone, 3 water, 5 camp). Used by ``check`` (lit-band rule) and the review lineups.
MARCH_GROUND = {
    "0": ("grass", "#4C4940", "ash-grey dead grass"),
    "1": ("dirt", "#3A332D", "trampled mud, wheel ruts (the darkest walkable tile)"),
    "2": ("stone", "#55524C", "broken flagstone, ash-crusted"),
    "3": ("water", "#121418", "still black water (oily sheen #3A3E48)"),
    "5": ("camp", "#47423C", "packed ash, charred boards"),
}
SCORCH = "#26221F"           # scorched patches (terrain detail)
DARKEST_GROUND = MARCH_GROUND["1"][1]

# ── material presets: (shadowAmt, lightAmt) ─────────────────────────────────────────────────────────────────
MON_PRESETS = {
    "skin": (.48, .26), "skin_dark": (.45, .20), "hide": (.50, .28), "cloth": (.52, .30),
    "dark": (.52, .36),          # base L* < 25: deep shade, generous light band (clears the march ground)
    "metal": (.48, .42), "metal_dull": (.50, .34), "metal_dark": (.50, .32), "blackmetal": (.55, .42), "metal_bright": (.30, .45),
    "trim": (.45, .36), "wood": (.50, .28), "bright": (.40, .30), "bright_matte": (.45, .24),
    "edge": (.25, .40), "line": (.30, .12), "void": (.30, .08), "tar": (.45, .45),
    "glow": (0.0, 0.0), "glow_soft": (.30, .30), "flat": (0.0, 0.0),
}
PROP_PRESETS = {
    "decor": (.48, .24), "decor_lit": (.46, .30), "decor_dark": (.50, .32), "decor_metal": (.46, .40),
    "chalk": (.30, .20), "sheen": (.30, .30), "line": (.30, .12), "interact": (.42, .36),
    "interact_metal": (.44, .44), "glow": (0.0, 0.0), "glow_soft": (.20, .30),
    "cover_dark": (.40, .14), "cover_light": (.38, .12),
}
FOL_PRESETS = {
    "bark": (.48, .24), "bark_lit": (.45, .30), "bark_dark": (.42, .30), "leaf": (.45, .24), "line": (.30, .12),
    "cover_dark": (.40, .14), "cover_light": (.38, .12), "seed": (.25, .30), "petal": (.42, .30),
    "glossy": (.35, .38), "sheen": (.30, .35),
}
# outline-colour mix per family (kit.outline.INK_MIX: monsters / boss / NPC 0.4 = 60 % ink; props and foliage 1.0 =
# the region's line tone; small prop parts that sit on bright rims 0.6 like camp.rope)
O_MON, O_PROP, O_FOL = 0.4, 1.0, 1.0

# ── Monsters_March (A2-A8) ───────────────────────────────────────────────────────────────────────────────────
# (name, hex, preset, emissive, note). Names: <role or kit>.<region>; generators build with the same names (the kit
# Builder registers ``<prefix>.<region>``) so their swatches land here; a value change appends a new swatch.
MONSTERS = [
    # shared breach accents
    ("breach.glow", BREACH, "glow", .85, "breach fire on bodies: fissures, seams"),
    ("breach.core", BREACH_CORE, "void", 0, "the near-black core round every violet glow"),
    ("breach.pinpoint", BREACH_PIN, "glow", 1.0, "violet pinpoints (eyes, sparks)"),
    ("breach.hot", BREACH_HOT, "glow", 1.0, "hottest violet-white (Valdras's seam cores)"),
    # The Unburied kit (monsters.md §2.2): burnt-clay ash skin, torn crimson tabards, dull mail, violet pinpoint eyes
    ("unburied.skin", "#8C8F86", "skin", 0, "ash-grey burnt clay, finely cracked"),
    ("unburied.skin_shadow", "#4C4F4A", "skin_dark", 0, "painted shadow: sockets, cracks, under the jaw"),
    ("unburied.skin_line", "#2A2C29", "line", 0, "crack strokes"),
    ("unburied.ash", "#A39E94", "bright_matte", 0, "ash dusting on shoulders and helms"),
    ("unburied.eye", BREACH_PIN, "glow", 1.0, "FAMILY MARKER: violet pinpoint eyes"),
    ("unburied.socket", "#141016", "void", 0, "eye sockets round the pinpoints"),
    ("unburied.mouth", "#1C1416", "void", 0, "slack mouth (no teeth, no bone)"),
    ("unburied.tabard", "#6B1F1F", "cloth", 0, "torn crusade tabard"),
    ("unburied.tabard_char", "#2E1416", "dark", 0, "scorched, torn tabard edges"),
    ("unburied.trim", "#B49A5E", "trim", 0, "tarnished tabard trim"),
    ("unburied.sigil", "#241A18", "line", 0, "the open-palm sigil, scorched"),
    ("unburied.mail", "#5A5D63", "metal", 0, "mail shreds, coifs"),
    ("unburied.mail_line", "#24252A", "line", 0, "mail rows"),
    ("unburied.helm", "#6A6D72", "metal_dull", 0, "kettle helms (dull: a lit helm top must not out-shine the hero)"),
    ("unburied.helm_dent", "#3E4046", "metal_dark", 0, "dents, rims, rust-dark"),
    ("unburied.gambeson", "#6E6758", "cloth", 0, "quilted gambeson, ash-stained"),
    ("unburied.gambeson_line", "#2E2A24", "line", 0, "quilting"),
    ("unburied.hood", "#4A4642", "dark", 0, "hoods"),
    ("unburied.leather", "#4A3A2E", "cloth", 0, "belts, straps, boots"),
    ("unburied.leather_dark", "#2A201A", "dark", 0, "boot soles, buckles' shadow"),
    ("unburied.wood", "#6A5844", "wood", 0, "arrow shafts in the back, shovel hafts"),
    ("unburied.fletch", "#9A958A", "bright_matte", 0, "broken fletching"),
    ("unburied.steel", "#80848A", "metal", 0, "notched blades, shovel blades"),
    ("unburied.steel_edge", "#B0B4B8", "edge", 0, "edges"),
    ("unburied.rope", "#8A7E66", "wood", 0, "rope, cord"),
    # risen_arbalest: crossbow (SM_MonWeapon_Crossbow), bolt quiver, pavise
    ("crossbow.stock", "#5E4E3E", "wood", 0, ""),
    ("crossbow.stock_dark", "#2E241C", "dark", 0, ""),
    ("crossbow.iron", "#5C5F64", "metal", 0, "prod, stirrup, nut"),
    ("crossbow.string", "#B8B2A2", "bright", 0, ""),
    ("crossbow.bolt", "#E8E0CC", "bright", 0, "bone-white bolts (the violet trail is the projectileColor)"),
    ("arbalest.quiver", "#4A3A2E", "cloth", 0, "= unburied.leather"),
    ("arbalest.pavise", "#5A2422", "cloth", 0, "the pavise's faded crimson field"),
    ("arbalest.pavise_rim", "#3E4046", "metal_dark", 0, ""),
    # risen_shieldbearer: tower shield (crimson field, scorched sigil, ash streaks)
    ("towershield.field", "#7A2422", "cloth", 0, "reads as one flat slab on the left side"),
    ("towershield.ash", "#8A8680", "bright_matte", 0, "ash streaks"),
    ("towershield.sigil", "#241A18", "line", 0, "= unburied.sigil"),
    ("towershield.rim", "#4E5156", "metal", 0, ""),
    ("towershield.back", "#4A3C30", "wood", 0, "planks behind, grip"),
    # dead_standard_bearer: the tallest silhouette, a tattered crimson banner
    ("standard.cloth", "#6E1A1C", "cloth", 0, ""),
    ("standard.char", "#1A1210", "dark", 0, "charred banner edges"),
    ("standard.pole", "#4E4032", "wood", 0, ""),
    ("standard.finial", "#9A8458", "trim", 0, ""),
    # sub_pitwarden [W3]: an officer's sash, a gorget, a long spade
    ("pitwarden.sash", "#5A1A1C", "cloth", 0, ""),
    ("pitwarden.gorget", "#8E7A50", "trim", 0, ""),
    ("pitwarden.spade", "#6A6D72", "metal_dull", 0, "= unburied.helm"),
    ("pitwarden.haft", "#5A4A3A", "wood", 0, ""),
    # The Living (monsters.md §2.3): the same kit, living palettes, no glow; marker = crimson cloth
    ("living.skin", "#B88A6E", "skin", 0, "zealot skin"),
    ("living.skin_b", "#9A7460", "skin", 0, "a second living skin (deserters)"),
    ("living.skin_shade", "#6E5040", "skin_dark", 0, ""),
    ("living.mouth", "#3A2020", "void", 0, ""),
    ("living.eye_dark", "#1E1414", "void", 0, "plain dark eyes: the Living do not glow"),
    ("living.hair", "#2E2620", "dark", 0, ""),
    ("zealot.robe", "#7E1416", "cloth", 0, "hooded red robe over mail"),
    ("zealot.marker", "#7E1416", "cloth", 0, "FAMILY MARKER (the Living): the crimson robe"),
    ("zealot.robe_dark", "#4A0C0E", "dark", 0, "robe lining, folds"),
    ("zealot.rope", "#8A7A5E", "wood", 0, "rope belts"),
    ("zealot.scar", "#C0283A", "glow_soft", .25, "the oath-scar on the forearms: faint crimson, flares on frenzy"),
    ("flail.iron", "#4E5156", "metal", 0, "= towershield.rim"),
    ("flail.chain", "#6A6D72", "metal_dull", 0, "= unburied.helm"),
    ("flail.haft", "#4E4032", "wood", 0, "= standard.pole"),
    ("deserter.gambeson", "#4E5040", "cloth", 0, "drab gambeson, insignia torn off"),
    ("deserter.gambeson_line", "#22231C", "line", 0, ""),
    ("deserter.hood", "#2F2B26", "dark", 0, "hood and the scarf over the face"),
    ("deserter.leather", "#5E4630", "cloth", 0, ""),
    ("deserter.pack", "#5A4E3C", "cloth", 0, "looted pack"),
    ("deserter.insignia_scar", "#6A6A58", "cloth", 0, "the paler patch where the insignia was"),
    ("deserter.marker", "#7A1C1E", "cloth", 0, "FAMILY MARKER (the Living): a crimson tabard rag / armband, duller "
                                               "than Harke's scarf"),
    ("drum.shell", "#5A4434", "wood", 0, "deserter_drummer's battered company drum"),
    ("drum.head", "#B8AC94", "bright", 0, ""),
    ("drum.hoop", "#6B1F1F", "cloth", 0, "= unburied.tabard (the crusade's colours)"),
    ("drum.cord", "#A89A7E", "bright", 0, ""),
    ("harke.scarf", "#A3221E", "trim", 0, "hunt_harke: the only bright red on a deserter"),
    ("harke.marker", "#A3221E", "trim", 0, "FAMILY MARKER for Harke (his scarf)"),
    ("harke.gorget", "#8E7A50", "trim", 0, "= pitwarden.gorget (a captain's gorget)"),
    ("dreck.tags", "#A8ACB0", "metal_bright", 0, "hunt_dreck: a bandolier of silver name-tags"),
    ("dreck.chain", "#7A7E84", "metal", 0, ""),
    ("dreck.sack", "#6A5A44", "cloth", 0, "the re-used loot sack"),
    # The Gravegnawers (monsters.md §2.4): hairless ash hide, needle teeth, bone-yellow eyes
    ("gnawer.hide", "#6F6A60", "hide", 0, ""),
    ("gnawer.hide_dark", "#3A3631", "skin_dark", 0, "painted shadow: ribs, flanks"),
    ("gnawer.hide_line", "#1E1C1A", "line", 0, "rib strokes (surface_paint)"),
    ("gnawer.belly", "#9C8F7A", "hide", 0, ""),
    ("gnawer.ear_stub", "#5A5248", "hide", 0, "torn ear stubs"),
    ("gnawer.claw", "#D8CCB0", "bright", 0, ""),
    ("gnawer.teeth", "#E2DAC4", "bright", 0, "needle teeth"),
    ("gnawer.maw", "#24141A", "void", 0, "near-black maw (aftermath, not gore)"),
    ("gnawer.gum", "#4A2E2E", "skin_dark", 0, ""),
    ("gnawer.eye", BONE_YELLOW, "glow_soft", .35, "FAMILY MARKER: bone-yellow eyeshine"),
    ("gnawer.pupil", "#1A1410", "void", 0, "slit"),
    ("gnawer.trophy", "#5A1E1E", "cloth", 0, "crusade tabard scraps tied on"),
    ("gnawer.rope", "#7A6E58", "wood", 0, ""),
    ("gnawer.spearhead", "#6E7074", "metal", 0, "a broken crusade spearhead lashed to a stick"),
    ("gnawer.stick", "#5A4C3C", "wood", 0, ""),
    ("bonegorger.hide", "#5F5A50", "hide", 0, ""),
    ("bonegorger.hide_dark", "#34302A", "skin_dark", 0, ""),
    ("bonegorger.belly", "#8F826C", "hide", 0, ""),
    ("bonegorger.tuft", "#4A443A", "dark", 0, "mangy tufts"),
    ("bonegorger.eye", BONE_YELLOW, "glow_soft", .35, "FAMILY MARKER: = gnawer.eye"),
    ("clapper.bronze", "#8A6A3A", "trim", 0, "the church-bell clapper club (hue 36°: outside the ember band)"),
    ("clapper.bronze_dark", "#4A3A22", "skin_dark", 0, "grime"),
    ("clapper.patina", "#5A6E62", "cloth", 0, "verdigris"),
    ("hungerbell.tags", "#8A6E44", "trim", 0, "hunt_hungerbell: a necklace of bronze name-tags"),
    # miniboss_corpseherd (monsters.md §2.5): hood + widow's veil, banner-patched skirt, ash-wood crook, handbell
    ("corpseherd.hood", "#2A2624", "dark", 0, "deep hood"),
    ("corpseherd.veil", "#3B3530", "dark", 0, "widow's veil"),
    ("corpseherd.rags", "#3B3530", "dark", 0, "shawl rags (= veil)"),
    ("corpseherd.skirt", "#5E1012", "dark", 0, "skirt and shawl patched from crusade banners"),
    ("corpseherd.hands", "#A89A8C", "skin", 0, "long-fingered ashen human hands"),
    ("corpseherd.chin", "#8E8276", "skin", 0, "the thin grey chin under the veil"),
    ("corpseherd.face_void", "#100C0E", "void", 0, "the dark under the hood"),
    ("corpseherd.eye", BREACH_PIN, "glow", 1.0, "FAMILY MARKER: violet eyes"),
    ("crook.wood", "#D9CEB4", "bright", 0, "pale ash-wood shepherd's crook"),
    ("crook.wood_line", "#6A6050", "line", 0, ""),
    ("crook.bell", "#8A6A3A", "trim", 0, "cracked bronze handbell (= clapper.bronze)"),
    ("crook.cord", "#6A5E4A", "wood", 0, ""),
    # valdras (monsters.md §2.6): blackened plate leaking violet, the standard grown through his spine
    ("valdras.plate", "#2E2F33", "blackmetal", 0, ""),
    ("valdras.plate_edge", "#4A4C52", "metal", 0, "bevels, rims"),
    ("valdras.plate_line", "#141418", "line", 0, ""),
    ("valdras.seam", BREACH, "glow", .85, "= breach.glow (never ember)"),
    ("valdras.seam_core", BREACH_HOT, "glow", 1.0, "= breach.hot"),
    ("valdras.visor", VISOR, "glow", 1.0, "the single visor slit"),
    ("valdras.marker", VISOR, "glow", 1.0, "FAMILY MARKER (Unburied boss): the visor slit"),
    ("valdras.plume", "#241C1C", "dark", 0, "burned plume"),
    ("valdras.plume_ash", "#5A5450", "cloth", 0, ""),
    ("valdras.standard", "#5E1012", "dark", 0, "= corpseherd.skirt (the same crusade banners)"),
    ("valdras.standard_char", "#1A1210", "dark", 0, "= standard.char"),
    ("valdras.pole", "#3A3028", "dark", 0, ""),
    ("valdras.sword", "#6A6D73", "metal", 0, "two-handed oath-sword"),
    ("valdras.sword_edge", "#9A9EA4", "edge", 0, ""),
    ("valdras.gauntlet", "#3E3A3A", "blackmetal", 0, "the grip melted into the gauntlet"),
    ("valdras.cloth", "#4A1A1A", "dark", 0, "tabard remnants under the plate"),
    # Breach-blood (monsters.md §2.1, SKEL_Slime)
    ("clot.shell", "#2A0E14", "tar", 0, "black-crimson tar shell (MI_AF_Toon_Slime body, opacity .62); glossy light band"),
    ("clot.rim", BREACH, "glow", .6, "breach-violet fresnel rim (modelled rim strips; the shell material's rim tint)"),
    ("clot.helm", "#4A4D52", "metal", 0, "the dented kettle helm suspended inside"),
    ("clot.spark", BREACH_PIN, "glow", 1.0, "3-4 violet sparks drifting inside"),
    ("clot.marker", BREACH_PIN, "glow", 1.0, "FAMILY MARKER (Breach-blood): = clot.spark"),
    ("clot.core", BREACH_CORE, "void", 0, "= breach.core"),
    ("clot.bolt", "#5C5F64", "metal", 0, "suspended crossbow bolt-heads (= crossbow.iron)"),
    ("clot.inner", "#2A0E14", "flat", 0, "the dither backing bowl (flat, like slime.inner)"),
    ("bloat.crust", "#3A1A12", "dark", 0, "cinderbloat: opaque cracked crust"),
    ("bloat.crack", "#1A0A0A", "void", 0, ""),
    ("bloat.fissure", BREACH, "glow", .8, "breach-violet fissures (never ember)"),
    ("bloat.marker", BREACH, "glow", .8, "FAMILY MARKER (Breach-blood): = bloat.fissure"),
    ("bloat.ash", "#8A8480", "bright_matte", 0, "ash flakes"),
    # the elite marker: W1 recolour of SM_FX_EliteCrown (keys of elite_crown.py) and W2 SM_FX_EliteSigil
    ("elite_crown.gold", "#5E6066", "metal", 0, "W1 crown: tarnished iron (elite_crown.py key 'gold')"),
    ("elite_crown.gold_light", "#8A8C90", "metal_bright", 0, "tarnished iron highlight"),
    ("elite_crown.gold_rule", "#2A2A2E", "line", 0, ""),
    ("elite_crown.gem", "#C0283A", "glow_soft", .3, "the crimson stone"),
    ("elite_crown.gem_rim", "#5A1018", "line", 0, ""),
    ("elite_sigil.iron", "#4A4C52", "metal", 0, "W2: a small broken iron halo (= valdras.plate_edge)"),
    ("elite_sigil.iron_line", "#1E1E22", "line", 0, ""),
    ("elite_sigil.core", "#C0283A", "glow", .9, "its crimson ember (crimson, never orange)"),
]

# ── Props_March (A10, A11 gather node) ───────────────────────────────────────────────────────────────────────
PROPS = [
    # Knellwatch camp kit (camp theme march): charred timber, pews, nailed crusade shields, helmets
    ("camp.charred", "#3A302A", "decor_dark", 0, "charred timber"),
    ("camp.charred_line", "#161210", "line", 0, ""),
    ("camp.timber", "#5A4A3C", "decor", 0, "weathered timber (bell frame, gate posts)"),
    ("camp.timber_end", "#7A6A56", "decor_lit", 0, "cut ends, worn tops"),
    ("camp.timber_line", "#1E1814", "line", 0, ""),
    ("camp.pew", "#4A3426", "decor_dark", 0, "chapel pews (the pew palisade)"),
    ("camp.pew_worn", "#6E5642", "decor_lit", 0, "worn pew edges"),
    ("camp.shield", "#5A2220", "decor", 0, "crusade shields nailed points-out, wagon paint"),
    ("camp.shield_rim", "#4E5156", "decor_metal", 0, ""),
    ("camp.helmet", "#5E6166", "decor_metal", 0, "helmets on the gate posts, chains"),
    ("camp.iron", "#4A4C52", "decor_metal", 0, "bands, braziers, fittings"),
    ("camp.iron_dark", "#26272B", "decor_dark", 0, ""),
    ("camp.nail", "#2E2A2C", "line", 0, ""),
    ("camp.rope", "#8A7E66", "decor_lit", 0, "rope (outline mix .6 like the plains rope)"),
    ("camp.canvas", "#4E473E", "decor", 0, "blanket line, wagon covers (big lit faces: keep it dark)"),
    ("camp.canvas_dark", "#2E2A24", "decor_dark", 0, ""),
    ("camp.canvas_line", "#1C1916", "line", 0, ""),
    ("camp.sack", "#5E5546", "decor", 0, "ration sacks"),
    ("camp.chalk", "#D8D2C4", "chalk", 0, "chalk tallies on the sacks"),
    ("camp.crate", "#4E4032", "decor", 0, "crusade supply crates"),
    ("camp.crate_stencil", "#6B2420", "decor", 0, "the crusade's stencil"),
    ("camp.stone", "#5E5C58", "decor", 0, "hearth ring, well"),
    ("camp.stone_top", "#747068", "decor_lit", 0, ""),
    ("camp.stone_line", "#1E1C1C", "line", 0, ""),
    ("camp.ash", "#4A4542", "decor", 0, ""),
    ("camp.soot", "#26221F", "decor_dark", 0, "soot, cold fire pits (Ashwheat)"),
    ("camp.water", "#0E1012", "line", 0, "well water"),
    ("camp.water_sheen", "#3A3E48", "sheen", 0, "oily sheen"),
    ("bell.bronze", "#6E5A3A", "decor_metal", 0, "the keepers' bell (bell frame), the split Ashwheat bell, handbells"),
    ("bell.bronze_dark", "#3A2E1E", "decor_dark", 0, ""),
    ("bell.patina", "#4E6A5E", "decor", 0, "verdigris"),
    ("bell.crack", "#140F0C", "line", 0, "the crack"),
    ("banner.black", "#1E1C20", "decor_dark", 0, "Knellwatch's banner: a black field …"),
    ("banner.bell", "#B8B0A0", "decor_lit", 0, "… with a pale bell"),
    ("banner.trim", "#4A4642", "decor", 0, ""),
    ("namewall.tag", "#8A6E44", "decor_metal", 0, "bronze name-tags in rows"),
    ("namewall.tag_dark", "#4A3A24", "decor_dark", 0, ""),
    # the refuges: the only warm light in the zone (chapter1.md §15)
    ("hearth.ember", EMBER, "glow", 1.0, "REFUGE: the hearth's embers"),
    ("hearth.ember_hot", WARM_LIGHT, "glow", 1.0, "REFUGE: the warm light #FFB060 (hearth core, bell-frame lantern)"),
    ("hearth.ember_dim", "#B8401C", "glow", .7, "REFUGE: dim embers"),
    ("hearth.log", "#4A3424", "decor", 0, ""),
    ("hearth.log_end", "#A07850", "decor_lit", 0, "log ends lit by the fire"),
    ("hearth.stone", "#5A5650", "decor", 0, "hearth stones, the Boneyard Gate"),
    ("forge.glow", "#FF8A3A", "glow", .85, "REFUGE: Hald's forge"),
    ("forge.coal", "#2A1E1A", "decor_dark", 0, ""),
    ("forge.blade", "#6A6D73", "decor_metal", 0, "the heap of swords waiting to be melted"),
    ("forge.blade_rust", "#5A4A3E", "decor", 0, ""),
    ("lantern.frame", "#2E2A26", "decor_dark", 0, "bell-frame lantern"),
    ("lantern.glow", WARM_LIGHT, "glow", 1.0, "REFUGE: = hearth.ember_hot"),
    ("crowmarket.cookfire", "#C86A3A", "glow", .5, "REFUGE: Crowmarket's cookfire, 'not quite warm'"),
    ("brazier.coals", "#6A2A1A", "glow", .25, "hub braziers smoulder (hue 12°: no light, outside the ember band)"),
    # wagons, the burned bridge, Ashwheat (the goblin-camp kit re-themed), the Boneyard Gate, the sealed Pit
    ("wagon.wood", "#5A4836", "decor", 0, "Sheepfold wagons, the wagon barricade, Crowmarket carts"),
    ("wagon.wood_dark", "#2E241C", "decor_dark", 0, ""),
    ("bridge.burnt", "#1E1816", "decor_dark", 0, "burned bridge planks"),
    ("cottage.thatch", "#3E362C", "decor", 0, "Ashwheat: burnt thatch (big faces: the plain decor light)"),
    ("cottage.thatch_lit", "#5A5040", "decor", 0, ""),
    ("cottage.daub", "#6E665A", "decor", 0, "wattle and daub"),
    ("cottage.door", "#4A3A2C", "decor_dark", 0, "doors barred from inside"),
    ("pit.stone", "#48443F", "decor", 0, "the sealed Pit mouth"),
    ("pit.dark", "#0C0A0C", "line", 0, ""),
    ("pit.seep", "#6E2A86", "glow", .35, "a faint breach seep under the seal"),
    # the Breach Rim (chapter1.md §9.1): the dais (Seal of Veins), ash circles, the banner ring, helmet stakes
    ("dais.stone", "#3E3A42", "decor_dark", 0, "cold dark stone"),
    ("dais.stone_top", "#56525A", "decor_lit", 0, ""),
    ("dais.crack", BREACH_CORE, "line", 0, ""),
    ("dais.glow", BREACH, "glow", .9, "violet uplight from the cracks; breach fire beyond the east barricade"),
    ("dais.vein", BREACH_PIN, "glow", 1.0, ""),
    ("ritual.ash", "#8A847C", "decor_lit", 0, "pale ash circles on the dark ground"),
    ("ritual.char", "#1E1A18", "decor_dark", 0, ""),
    ("bannerring.cloth", "#5A1A1A", "decor", 0, "12 scorched banners"),
    ("bannerring.char", "#1A1210", "decor_dark", 0, ""),
    ("stake.wood", "#4A3C30", "decor", 0, "helmet stakes, spears"),
    # story decor (positions and spriteType kept, chapter1.md §12)
    ("redbanner.cloth", "#9A2020", "interact", 0, "story_rf_red_banner: the brightest crimson in the decor"),
    ("belfry.mud", "#3A322A", "decor_dark", 0, "story_rf_belfry: the split bell lies in the mud"),
    ("altar.stone", "#5A565C", "decor", 0, "story_rf_penitents_altar"),
    ("altar.groove", "#1E1A1E", "line", 0, "the groove toward the breach"),
    ("altar.dried", "#1A1012", "decor_dark", 0, "'something dried black'"),
    # lore / interactive (interaction may glow; §1.5 interactive ≥ decor)
    ("orderpost.wood", "#4E4032", "decor", 0, "lore_rf_01: the Marshal's Last Order"),
    ("orderpost.paper", "#C8BCA0", "interact", 0, "parchment (journal pages too)"),
    ("orderpost.ink", "#2A2024", "line", 0, ""),
    ("orderpost.seal", "#8A1E22", "interact", 0, "wax seal"),
    ("journal.leather", "#4A3428", "interact", 0, "lore_rf_02"),
    ("seal.stone", "#4A4650", "interact", 0, "lore_rf_03: shard of the seal"),
    ("seal.lines", "#FF9A4A", "glow", .8, "ABYSSFIRE: its lines match the hero's brand (warm, as if just held)"),
    ("catechism.stone", "#6A6660", "decor", 0, "lore_rf_04"),
    ("catechism.carve", "#2A2826", "line", 0, ""),
    ("lore.glyph", "#FFB878", "glow", .85, "INTERACTION: a small warm glyph, 'something to read here'"),
    ("echo.body", "#A8B0C4", "glow_soft", .4, "lore_rf_echo_*: cold pale ghost (not violet: echoes are victims)"),
    ("echo.rim", "#DCE4F2", "glow", .7, ""),
    ("strongbox.iron", "#3E4046", "interact_metal", 0, "A Dead Knight's Strongbox (treasure_cache), the Paymaster's"),
    ("strongbox.wood", "#4A3A2E", "interact", 0, ""),
    ("strongbox.brass", "#B08A3A", "interact_metal", 0, "loot-side brass"),
    ("strongbox.seam", "#FFD08A", "glow", .85, "LOOT: the lid seam glow"),
    ("handbell.glow", "#FFC070", "glow", .8, "INTERACTION: the Five Handbells puzzle feedback"),
    ("barrowmoss.moss", "#5E6A4A", "interact", 0, "S2 gather node (苦冢苔): dull grave moss"),
    ("barrowmoss.moss_lit", "#8A9466", "interact", 0, ""),
    ("barrowmoss.glow", "#C8D49A", "glow_soft", .35, "pale gather hint (not ember)"),
    # rocks, boulders, outcrops and their ground blend
    ("rock.stone", "#66625C", "decor", 0, "broken stone with tool marks"),
    ("rock.toolmark", "#4A4642", "decor", 0, "(= banner.trim)"),
    ("boulder.stone", "#5A5650", "decor", 0, "(= hearth.stone)"),
    ("boulder.ash", "#8A8680", "decor_lit", 0, "ash crust on top"),
    ("outcrop.stone", "#5E5A54", "decor", 0, ""),
    ("outcrop.cap", "#7A766E", "decor_lit", 0, ""),
    ("ground.dark", "#3E3B35", "cover_dark", 0, "skirts / bases blending into the march ground"),
    ("ground.light", "#6A665C", "cover_light", 0, ""),
]
PROP_O = {"camp.rope": 0.6}     # outline mix overrides (decor default 1.0)

# ── Foliage_March (A10): the same game ids and footprints, dead ─────────────────────────────────────────────
FOLIAGE = [
    ("deadoak.bark", "#4A4038", "bark", 0, "tree@march: dead oak"),
    ("deadoak.bark_lit", "#6E6256", "bark_lit", 0, ""),
    ("deadoak.split", "#1C1614", "line", 0, "bark split by fire"),
    ("deadoak.char", "#2A2220", "bark_dark", 0, "charred heart"),
    ("deadoak.ash", "#7A746C", "bark_lit", 0, "ash on the limbs"),
    ("deadoak.leaf", "#5E4A36", "leaf", 0, "a few clinging dead leaves"),
    ("charred.bark", "#3A322C", "bark_dark", 0, "tree_round@march: charred orchard tree"),
    ("charred.char", "#1A1614", "line", 0, ""),
    ("charred.ash", "#6E6862", "bark_lit", 0, ""),
    ("charred.apple", "#1E1418", "glossy", 0, "a few black apples"),
    ("briar.stem", "#3A3228", "bark_dark", 0, "bush@march: briar"),
    ("briar.thorn", "#6A5E4C", "bark_lit", 0, ""),
    ("briar.leaf", "#4E4A3A", "leaf", 0, ""),
    ("briar.rag", "#6A6050", "leaf", 0, "rags caught on it"),
    ("briar.rag_crimson", "#5A1E1E", "leaf", 0, "a tabard scrap"),
    ("ashgrass.dark", "#4A473F", "cover_dark", 0, "grass@march: ash-grey dead grass (also the cover tufts' dark)"),
    ("ashgrass.light", "#8A867A", "cover_light", 0, ""),
    ("ashgrass.seed", "#A8A294", "seed", 0, "seed heads"),
    ("poppy.petal", "#7A2A26", "petal", 0, "flower@march: dull poppies, the foliage's only colour"),
    ("poppy.centre", "#1E1A1C", "line", 0, ""),
    ("poppy.ash", "#8E8A82", "petal", 0, "ash"),
    ("ashcap.cap", "#B8B2A6", "petal", 0, "mushroom_red@march: pale ash-cap"),
    ("ashcap.gill", "#6E665E", "leaf", 0, ""),
    ("ashcap.stem", "#9A948A", "leaf", 0, ""),
    ("reed.stem", "#4A4838", "leaf", 0, "lily_pad_plains@march (optional): dead reeds"),
    ("reed.head", "#2E2A24", "bark_dark", 0, ""),
    ("scum.oil", "#2A2C30", "glossy", 0, "oily scum on black water"),
    ("scum.sheen", "#4A4E5A", "sheen", 0, ""),
]

# ── NPC (A9): march looks appended to the NPC registry ──────────────────────────────────────────────────────
# Colour fields of folk.Look per NPC role (chapter1.md §3); the swatches are exactly what ``folk.regions_for`` derives
# from them (shade / line / brow / blush tones included), registered under ``npc.march.<snake>`` — the prefix the A9
# re-dress must build with (``folk.Folk(look, asset, prefix=f"npc.march.{snake}")``): the plains looks keep
# ``npc.<snake>`` and must never be re-registered with march values (that would move their names and free swatches
# the shipped plains NPCs still sample). Shapes (hair style, hat kind, build) are A9's; only colours are fixed here.
NPC_PREFIX = "npc.march"
# NPC capacity (art review A1): the NPC atlas held 175 of 256 swatches, and eleven re-dressed looks with their own
# skins, hair and legs would need ~160 more. So the march looks change the *identity* pieces (the Captain's crusade
# tabard, the chaplain's black cassock, Corran's slate hood and deserter gambeson, the pilgrim's undyed wool, Marl's
# drab tunic, Mag's crow-black collar) and reuse the existing NPC swatch values for everything else (skins, hair,
# legs, boots, belts, the earthy coats; the warm human skins are right: the living are warm, the dead are ash). The
# saturated plains accents (teal, blue, green, purple, straw, red scarf) are gone. A9: add a colour only for a new
# identity piece; anything else picks an existing NPC value (same hex *and* tone params, see ``folk.regions_for``).
NPC_LOOKS = {
    "quest_elder": dict(      # 瑟琳·哈洛队长 Captain Serin Harrow: dented half-plate over the crusade tabard (the sigil
        skin="#C49C78", build="slim", hair=("#B8B4AE", "short"),   # cut out), cropped grey hair, a neck burn scar
        top=("#6E2422", "tunic", "#B08A3A"), sleeves="#5A5A62", belt="#2A1C14", legs="#262A36", boots="#1A140E"),
    "knell_chaplain": dict(   # 奥德温修士 Brother Aldwin: threadbare black cassock, rope belt, ash on the brow, handbell
        skin="#C2A07E", hair=("#4A3D30", "short"), top=("#2C292C", "robe", None), belt="#8A6A4A",
        legs="#262A36", boots="#1A140E"),
    "blacksmith": dict(       # 哈尔德·熔剑 Hald Swordmelter: soot-grey tunic, leather apron (the red braid is gone)
        skin="#C98B62", build="stout", hair=("#3F2D21", "bald"), beard=("#3F2D21", "braided"),
        top=("#5A5A62", "tunic", None), sleeves="#C98B62", apron="#6E4424", belt="#3A2414", legs="#4A3A2C",
        boots="#2E2018"),
    "merchant": dict(         # 军需官薇拉 Quartermaster Vira: drab army vest over linen, a ledger, a ration-tally cord
        skin="#C49A72", build="stout", hair=("#3A2A1A", "bun"), top=("#3A4A30", "vest", "#B08A3A"),
        sleeves="#9A9080", belt="#4A3015", legs="#4A3A2A", boots="#2A1A0A"),   # greyed linen (the plains cream
                                                                                # sleeves were the brightest NPC area)
    "stash_keeper": dict(     # 守冢人塔姆欣 Tamsin the Gravewarden: earth-dark grave-digger's coat, crypt keys
        skin="#C2A07E", hair=("#4A3D30", "short"), top=("#3E3630", "coat", None), belt="#2A1C14",
        legs="#4A4F43", boots="#3A2A1C", keyring=True),
    "plains_herbalist": dict(  # 军医玛勒 Mother Marl: drab tunic, leather apron, rolled sleeves, a lantern
        skin="#C49A72", hair=("#B8B4AE", "bun"), hat=("headband", "#5A4A30", None),
        top=("#5E5A4A", "tunic", "#D8C9A0"), apron="#6E4424", belt="#3A2414", legs="#4A3A2A", boots="#2A1A0A",
        pack="#8A6A3A"),
    "plains_wanderer": dict(  # 逃兵柯兰 Corran the deserter: the deserters' drab gambeson under a slate hood (no crimson:
        skin="#C8966C", height=1.76, hair=("#1E1A1E", "ponytail"), beard=("#1E1A1E", "stubble"),  # he is no enemy)
        hat=("hood", "#4A5058", None), top=("#4E5040", "coat", None), belt="#2A1C14", legs="#262A36",
        boots="#1A140E", pack="#674931"),
    "escort_traveling_merchant": dict(   # 掘墓人伯恩 Old Bern the Gravedigger (escort): earth-stained coat, white hair
        skin="#C49C78", hair=("#B8B4AE", "short"), beard=("#D6D2CC", "long"), top=("#6B3E39", "coat", None),
        belt="#2A1C14", legs="#4A3A2A", boots="#2E2018"),
    "rescue_lost_traveler": dict(        # 被缚的朝圣者 the Bound Pilgrim: undyed wool, a dusty pilgrim's cloak, rope
        skin="#C49C78", hair=("#493326", "short"), top=("#8A8270", "robe", None), mantle="#6E6E36",
        belt="#8A6A4A", legs="#4A4F43", boots="#3A2A1C"),
    "wandering_merchant": dict(          # 圣骨贩子 the Relic Peddler: a tray of bone-white relics
        skin="#C49A72", hair=("#5A4028", "short"), beard=("#5A4028", "short"), hat=("widebrim", "#5A4A30", None),
        top=("#6B3E39", "coat", "#B08A3A"), belt="#3A2414", legs="#4A3A2A", boots="#2A1A0A", pack="#7A5A36"),
    "crow_trader": dict(      # 鸦嫂玛格 Mag Crowe (new, camp 2): a black coat, a crow-feather collar, boots slung over
        skin="#C8966C", hair=("#1E1A1E", "long"), top=("#2C292C", "coat", None), mantle="#1E1C22",  # a shoulder
        belt="#2A1C14", legs="#262A36", boots="#2A1A0A"),
}
# kit parts and props of the re-dress (A9 kit parts: half-plate + gorget, cassock, handbell, barrow), registered
# under ``npckit.march.<part>`` with the NPC ink mix; (hex, shadowAmt, lightAmt) equal to an existing NPC swatch
# wherever one fits (METAL / METAL_DARK / LEATHER / BEDROLL / the prop paper and wood), so only the new materials
# take an index
NPC_KIT = [
    ("halfplate", "#6E7076", .42, .50, 0, "the Captain's dented half-plate"),
    ("halfplate_dark", "#5A606E", .42, .32, 0, "(= folk METAL_DARK)"),
    ("gorget", "#9AA3B2", .42, .50, 0, "(= folk METAL)"),
    ("burn_scar", "#8E5A50", .30, .25, 0, "the Captain's neck scar"),
    ("cassock", "#2C292C", .42, .32, 0, "(= the chaplain's top)"),
    ("ash_brow", "#6E6A66", .30, .25, 0, "ash on the chaplain's brow"),
    ("handbell", "#8A6A3A", .42, .45, 0, "the chaplain's bronze handbell"),
    ("handbell_dark", "#4A3A22", .30, .12, 0, ""),
    ("cord", "#8A6A4A", .42, .32, 0, "handbell cord, tally cord, the pilgrim's wrist rope (= folk BEDROLL)"),
    ("ledger", "#6A4228", .42, .32, 0, "Vira's ledger (= folk LEATHER)"),
    ("ledger_pages", "#EFE3C2", .25, .40, 0, "(= npcprop paper)"),
    ("crypt_keys", "#B08A3A", .42, .40, 0, "Tamsin's crypt keys (= the keyring brass)"),
    ("lantern_frame", "#2E2A26", .42, .32, 0, "Marl's lantern"),
    ("lantern_glow", "#F0A060", 0.0, 0.0, .55, "a dim warm light (Marl's dressing station is a refuge)"),
    ("stain", "#3A2A26", .30, .20, 0, "Marl's stained gloves and apron"),
    ("barrow_wood", "#7A5230", .42, .30, 0, "Bern's barrow (= npcprop wood)"),
    ("shroud", "#B8B0A0", .35, .30, 0, "the shrouded dead on the barrow"),
    ("tray", "#7A5230", .42, .30, 0, "the Relic Peddler's tray (= npcprop wood)"),
    ("relic", "#EFE3C2", .25, .40, 0, "bone-white relics (= npcprop paper)"),
    ("slung_boots", "#2E2018", .42, .32, 0, "boots slung over Mag's shoulder (= Hald's boots)"),
]
NPC_KIT_PREFIX = "npckit.march"

# ── theme variants (kit.theme): base game id → the march asset its slot resolves to (planned names; the package
# shipping it lists ``kit.theme.themed_ids([base], "march")`` in its gameIds). New remake ids (monsters, the two new
# NPCs, quest items) are plain ids and not listed here.
VARIANTS = [
    # (base game id, planned asset, package, note)
    ("tree", "SM_Foliage_March_DeadOak_A/B/C", "A10", "dead oak, bark split by fire"),
    ("tree_round", "SM_Foliage_March_CharredOrchard_A/B", "A10", "charred orchard tree, a few black apples"),
    ("bush", "SM_Foliage_March_Briar_A/B", "A10", "briar with rags caught on it"),
    ("grass", "SM_Foliage_March_AshGrass_A/B/C", "A10", "ash-grey dead grass (ground cover, no ink)"),
    ("flower", "SM_Foliage_March_AshPoppy_A/B", "A10", "ash and dull poppies (ground cover, no ink)"),
    ("mushroom_red", "SM_Foliage_March_AshCap_A/B", "A10", "pale ash-cap fungus"),
    ("lily_pad_plains", "SM_Foliage_March_Reeds_A/B", "A10", "optional: dead reeds / oily scum on black water"),
    ("rock", "SM_Rock_March_BrokenStone_A/B/C", "A10", "broken stone with tool marks"),
    ("boulder", "SM_Rock_March_AshBoulder_A", "A10", "ash-crusted boulder"),
    ("outcrop_plains", "SM_Outcrop_March_V0..V5", "A10", "wall outcrops on the same tile grid"),
    ("palisade_plains", "SM_Palisade_March_Pew_A/B/C", "A10", "pews, cart wheels, shields nailed points-out"),
    ("palisade_gate_plains", "SM_Palisade_March_GatePost", "A10", "helmets on the gate posts"),
    ("camp_tent", "SM_Prop_March_BellFrame", "A10", "the bell frame in the tent slot"),
    ("camp_campfire", "SM_Prop_March_Hearth", "A10", "the hearth: the only warm light (#FFB060)"),
    ("camp_banner", "SM_Prop_March_KnellwatchBanner", "A10", "a black field with a bell"),
    ("camp_crate", "SM_Prop_March_SupplyCrate", "A10", "crusade supply crates"),
    ("camp_barrel", "SM_Prop_March_RationSacks", "A10", "ration sacks counted in chalk"),
    ("camp_well", "SM_Prop_March_Well", "A10", "buckets, a ladle chained to it"),
    ("camp_torch", "SM_Prop_March_Brazier", "A10", "a smouldering brazier (no warm light)"),
    ("decor_ruins", "SM_Prop_March_BrokenBelfry", "A10", "story_rf_belfry (spriteType ruins; \"Ashwheat's Broken "
                    "Belfry\", chapter1.md §19 — renamed from SM_Prop_March_FallenBelfry)"),
    ("decor_war_banner", "SM_Prop_March_RedBanner", "A10", "story_rf_red_banner (spriteType war_banner)"),
    ("decor_broken_altar", "SM_Prop_March_PenitentsAltar", "A10", "story_rf_penitents_altar"),
    ("lore_ancient_tablet", "SM_Prop_March_OrderPost", "A10", "lore_rf_01 (order post falls back to the tablet)"),
    ("lore_torn_journal", "SM_Prop_March_TornJournal", "A10", "lore_rf_02"),
    ("lore_crystal_shard", "SM_Prop_March_SealShard", "A10", "lore_rf_03 (warm brand lines)"),
    ("lore_carved_stone", "SM_Prop_March_CatechismStone", "A10", "lore_rf_04"),
    ("decor_event_puzzle_rune_pillar", "SM_Prop_March_HandbellPost", "A10", "Five Handbells puzzle"),
    ("exit_gate_sealed", "SM_Prop_March_WagonBarricade", "A10", "east road: wagons and shields, breach fire beyond"),
    ("exit_sealed", "SM_Prop_March_WagonBarricade", "A10", "(alias of exit_gate_sealed)"),
    ("treasure_chest", "SK_Prop_March_Strongbox", "A10", "A Dead Knight's Strongbox (treasure_cache)"),
    ("decor_treasure_chest", "SK_Prop_March_Strongbox", "A10", "(alias)"),
    ("gather_herb", "SM_Prop_Gather_Barrowmoss", "A11", "S2 barrowmoss gather node"),
    ("elite_crown", "SM_FX_EliteCrown_March", "A2-A8", "W1: tarnished iron, crimson stone (W2 SM_FX_EliteSigil)"),
    ("npc_quest_elder", "SK_NPC_QuestElder_March", "A9", "Captain Serin Harrow"),
    ("npc_blacksmith", "SK_NPC_Blacksmith_March", "A9", "Hald Swordmelter"),
    ("npc_merchant", "SK_NPC_Merchant_March", "A9", "Quartermaster Vira"),
    ("npc_stash", "SK_NPC_StashKeeper_March", "A9", "Tamsin the Gravewarden"),
    ("npc_plains_herbalist", "SK_NPC_PlainsHerbalist_March", "A9", "Mother Marl"),
    ("npc_plains_wanderer", "SK_NPC_PlainsWanderer_March", "A9", "Corran the deserter"),
    ("npc_escort_traveling_merchant", "SK_NPC_EscortTravelingMerchant_March", "A9", "Old Bern the Gravedigger"),
    ("npc_rescue_lost_traveler", "SK_NPC_RescueLostTraveler_March", "A9", "the Bound Pilgrim"),
    ("npc_wandering_merchant", "SK_NPC_WanderingMerchant_March", "A9", "the Relic Peddler"),
    ("npc_scenery_anvil", "SM_Prop_March_AnvilStump", "A9", "Hald's anvil stump (charred)"),
]
# Plain (unthemed) remake ids the same packages ship, for reference: monster_<id> for every monsters.md §2 id
# (breachclot, cinderbloat, risen_footman, risen_arbalest, risen_shieldbearer, dead_standard_bearer, oath_zealot,
# deserter, deserter_drummer, gravegnawer, bonegorger, miniboss_corpseherd, valdras, sub_pitwarden) + the hunts'
# variants on their base meshes; npc_knell_chaplain, npc_crow_trader; quest_item_ichor / _moss / _tags (A11).

FAMILY_SETS = {"Monsters_March": (MONSTERS, MON_PRESETS, O_MON, {}),
               "Props_March": (PROPS, PROP_PRESETS, O_PROP, PROP_O),
               "Foliage_March": (FOLIAGE, FOL_PRESETS, O_FOL, {})}
PARENTS = {"Foliage_March": "M_AF_Toon_Foliage"}          # manifest palette parent (envkit.FOLIAGE_TOON)
MARKER_ROLES = {   # hostile role → its family-marker swatch names (darkgate: ≥ 2 px on the darkest tile)
    "unburied (footman, arbalester, shieldbearer, standard-bearer, pitwarden)": ("unburied.eye",),
    "oath_zealot": ("zealot.marker",), "deserter / drummer / hunt_dreck": ("deserter.marker",),
    "hunt_harke": ("harke.marker",), "gravegnawer": ("gnawer.eye",), "bonegorger / hunt_hungerbell": ("bonegorger.eye",),
    "miniboss_corpseherd": ("corpseherd.eye",), "valdras": ("valdras.marker",), "breachclot": ("clot.marker",),
    "cinderbloat": ("bloat.marker",),
}
WARM_ALLOWED = {"hearth.", "forge.", "lantern.", "crowmarket.", "seal.", "lore.glyph", "strongbox.", "handbell."}


# ── colour helpers (pure Python) ────────────────────────────────────────────────────────────────────────────
def hex_rgb(h: str) -> tuple:
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def lab_l(rgb) -> float:
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    y = 0.2126 * lin(rgb[0]) + 0.7152 * lin(rgb[1]) + 0.0722 * lin(rgb[2])
    f = y ** (1 / 3) if y > 216 / 24389 else (24389 / 27 * y + 16) / 116
    return 116 * f - 16


def hsv(h: str) -> tuple:
    r, g, b = (c / 255.0 for c in hex_rgb(h))
    hh, s, v = colorsys.rgb_to_hsv(r, g, b)
    return hh * 360.0, s, v


def is_ember(h: str) -> bool:
    hue, s, v = hsv(h)
    return 15.0 <= hue <= 35.0 and s > 0.6 and v >= 0.35


def light_tone(h: str, s: float, l: float) -> tuple:
    c = hex_rgb(h)
    return tuple(x + (w - x) * l for x, w in zip(c, (255.0, 244.0, 214.0)))


# ── regions ────────────────────────────────────────────────────────────────────────────────────────────────
def family_regions(family: str) -> list:
    """[(name, Region, note)] of a march family in registration order."""
    from kit.palette import Region
    rows, presets, o, o_over = FAMILY_SETS[family]
    out = []
    for name, hx, pre, e, note in rows:
        s, l = presets[pre]
        out.append((name, Region(hx, s, l, e, o_over.get(name, o)), note))
    return out


def npc_regions() -> list:
    """[(name, Region, note)] of the march NPC looks (via ``folk.regions_for``) and kit parts, NPC ink class."""
    from kit import outline
    from kit.palette import Region
    import folk
    out = []
    for snake, kw in NPC_LOOKS.items():
        look = folk.Look(**kw)
        for key, reg in folk.regions_for(look).items():
            out.append((f"{NPC_PREFIX}.{snake}.{key}", reg, ""))
    o = outline.INK_MIX[folk.OUTLINE_CLASS]
    for part, hx, s, l, e, note in NPC_KIT:
        out.append((f"{NPC_KIT_PREFIX}.{part}", Region(hx, s, l, e, o), note))
    return out


def regions_of(family: str) -> list:
    return npc_regions() if family == "NPC" else family_regions(family)


# ── checks ─────────────────────────────────────────────────────────────────────────────────────────────────
BODY_PRESETS = {"skin", "hide", "cloth", "dark", "metal", "blackmetal", "trim", "wood", "tar"}


def check_rules() -> tuple[list, list]:
    """(failures, advisories) of the colour rules on the swatch lists (no registry needed)."""
    fails, adv = [], []
    ground_l = lab_l(hex_rgb(DARKEST_GROUND))
    for name, hx, pre, e, _ in MONSTERS:
        eye = name.endswith((".eye", ".eyes", ".sclera", ".pupil", ".marker"))
        if (e > 0 or eye) and is_ember(hx):
            fails.append(f"ember: enemy swatch Monsters_March {name} {hx} is ember orange (DR5)")
        hue, s, v = hsv(hx)
        if (e > 0 or eye) and 10.0 <= hue <= 40.0 and s > 0.55 and v >= 0.35 and not is_ember(hx):
            adv.append(f"near-ember enemy accent {name} {hx} (hue {hue:.0f}°, sat {s:.2f})")
        if pre in BODY_PRESETS:
            s_, l_ = MON_PRESETS[pre]
            dl = lab_l(light_tone(hx, s_, l_)) - ground_l
            if dl < 15.0:
                fails.append(f"lit band: Monsters_March {name} {hx} light tone only {dl:+.1f} L* over the darkest "
                             f"march ground {DARKEST_GROUND} (darkgate lit rule 15)")
    names = {n for n, *_ in MONSTERS}
    for role, ms in MARKER_ROLES.items():
        for m in ms:
            if m not in names:
                fails.append(f"marker: {role} has no marker swatch {m}")
    for name, hx, pre, e, _ in PROPS:
        if e > 0 and is_ember(hx) and not any(name.startswith(p) for p in WARM_ALLOWED):
            fails.append(f"warm light: Props_March {name} {hx} is ember outside the refuges / interaction / Abyssfire")
    for fam_rows, fam in ((PROPS, "Props_March"), (FOLIAGE, "Foliage_March")):
        for name, hx, pre, e, _ in fam_rows:
            hue, s, v = hsv(hx)
            if e > 0 and abs(((hue - 288.0) + 180.0) % 360.0 - 180.0) < 30.0 and not name.startswith(
                    ("dais.", "pit.")):
                adv.append(f"violet: {fam} {name} {hx} glows breach-violet outside the Breach Rim / Pit")
    for snake, kw in NPC_LOOKS.items():
        for k, v in kw.items():
            vals = [v] if isinstance(v, str) else [x for x in (v if isinstance(v, tuple) else ()) if
                                                    isinstance(x, str) and x.startswith("#")]
            for hx in vals:
                if not hx.startswith("#"):
                    continue
                hue, s, val = hsv(hx)
                if s > 0.5 and abs(((hue - 288.0) + 180.0) % 360.0 - 180.0) < 30.0:
                    adv.append(f"violet: NPC {snake}.{k} {hx} reads as breach magic on a friendly")
                if s > 0.55 and (hue < 12.0 or hue > 345.0) and val > 0.45:
                    adv.append(f"crimson: NPC {snake}.{k} {hx} is near the Living's crimson-cloth marker")
    return fails, adv


# ── registration ───────────────────────────────────────────────────────────────────────────────────────────
def register(families, dry: bool) -> dict:
    """Register every swatch of ``families`` (append-only), save registry + atlases, write the manifest palette
    entries (not for a dry run). Returns {family: {"before": n, "after": n, "new": [...], "names": {name: idx}}}."""
    from kit import export
    from kit.palette import GRID, Palette
    out = {}
    for fam in families:
        pal = Palette(fam)
        before = {i for i, sw in enumerate(pal.swatches) if sw is not None}
        idx = {}
        for name, reg, _ in regions_of(fam):
            idx[name] = pal.add(name, reg)
        after = {i for i, sw in enumerate(pal.swatches) if sw is not None}
        moved = [n for n in idx if n in pal._released]
        foreign = [n for n in moved if fam == "NPC" and not n.startswith((NPC_PREFIX + ".", NPC_KIT_PREFIX + "."))]
        if foreign:      # never: a march name is new; only a plains name moving would orphan a shipped NPC's swatch
            raise RuntimeError(f"{fam}: re-registering moved plains names {foreign[:6]} … (append-only violated)")
        for n in moved:  # the colour script changed a march colour: the name moves to a new swatch (its old one is
            print(f"  [moved] {fam} {n} → swatch {idx[n]} (colour script changed; re-ship what samples it)")
        pal.save()
        out[fam] = {"before": len(before), "after": len(after), "new": sorted(after - before), "names": idx,
                    "capacity": GRID * GRID}
        if not dry:
            man = export.Manifest()
            if fam == "NPC" and fam in man.data["palettes"]:
                pass                                      # unchanged entry (same files): keep the disk copy
            else:
                man.set_palette(pal)
                if fam in PARENTS:
                    man.data["palettes"][fam]["parent"] = PARENTS[fam]
                man.save()
    return out


def summary(res: dict) -> None:
    for fam, r in res.items():
        print(f"[{fam}] swatches {r['before']} → {r['after']} / {r['capacity']} (+{len(r['new'])} new: "
              f"{_ranges(r['new'])}); {len(r['names'])} march names")
        groups: dict[str, list] = {}
        for n, i in r["names"].items():
            key = n.rsplit(".", 1)[0]
            groups.setdefault(key, []).append(i)
        for g, ids in groups.items():
            print(f"    {g:40s} {_ranges(sorted(set(ids)))}")


def _ranges(ids) -> str:
    ids = sorted(ids)
    if not ids:
        return "-"
    out, a, b = [], ids[0], ids[0]
    for i in ids[1:]:
        if i == b + 1:
            b = i
            continue
        out.append(f"{a}" if a == b else f"{a}-{b}")
        a = b = i
    out.append(f"{a}" if a == b else f"{a}-{b}")
    return ",".join(out)


def main(argv: list[str]) -> int:
    dry = "--check" in argv
    fams = ["Monsters_March", "Props_March", "Foliage_March", "NPC"]
    for a in argv:
        if a.startswith("--families="):
            fams = [f for f in a.split("=", 1)[1].split(",") if f]
    fails, adv = check_rules()
    for a in adv:
        print(f"  [advisory] {a}")
    for f in fails:
        print(f"  [FAIL] {f}")
    if dry:                                               # a scratch copy of the registries: nothing committed moves
        from kit import paths
        tmp = Path(tempfile.mkdtemp(prefix="march_pal_"))
        shutil.copytree(paths.palette_root(), tmp / "palettes")
        os.environ["AF_PALETTE_ROOT"] = str(tmp / "palettes")
        os.environ["AF_EXPORT_ROOT"] = str(tmp / "export")
        try:
            res = register(fams, dry=True)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    else:
        res = register(fams, dry=False)
    summary(res)
    if "--list" in argv:
        for fam, r in res.items():
            for n, i in sorted(r["names"].items(), key=lambda kv: (kv[1], kv[0])):
                print(f"  {fam}[{i}] {n}")
    full = [f for f, r in res.items() if r["after"] > r["capacity"] - 16]
    for f in full:
        print(f"  [advisory] {f}: {res[f]['capacity'] - res[f]['after']} swatches left")
    print("OK" if not fails else f"{len(fails)} rule failure(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
