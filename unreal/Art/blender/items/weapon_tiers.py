"""Weapon tier materials (DECISIONS I6 / R4: one mesh per weapon family, tiered material variants).

Each family mesh paints its parts with a fixed set of *roles* (blade, guard, grip …). A tier = one colour set for
every role of every family, i.e. one recoloured ``Weapons`` atlas. Colours come from the web item-icon variant tables
(``src/graphics/icons/ItemIcons.ts`` :95-255: SWORDS, DAGGERS, BOWS, STAFFS, SHIELDS, drawAxe / drawMace / wandSpec)
— each tier takes the look of the family's base at that level step, so a T4 sword is coloured like the claymore,
a T5 dagger like the shadow blade, a T2 staff like the arcane staff. Higher tiers add the web's glow accents as
emissive roles (sword fuller, dagger edge, staff orb / rune line, bow grain, shield bands, axe edge, mace cap).

Families without bases in ``Data/item_bases.json`` (mace, wand: the core's ``WeaponType`` enum and the web icons have
them) follow the same ladder with the sword / staff metals.
"""
from __future__ import annotations

from kit.palette import Region

# web ItemIcons.ts palette (:95-121)
STEEL, IRON, DARK_IRON = "#C5CED9", "#8C95A3", "#4D5361"
GOLD, BRONZE, COPPER, SILVER = "#E3B44C", "#C4843E", "#CF7A45", "#DDE3EC"
LEATHER, DARK_LEATHER, WOOD, DARK_WOOD = "#8A5332", "#5C3522", "#A06D3C", "#6B4428"
BONE, DEMON, DEMON_BLACK = "#E9DFC2", "#6A1F2C", "#2F2432"
CRIMSON, VIOLET, CYAN = "#FF3A52", "#A968FF", "#56DCFF"
RUBY, SAPPHIRE = "#E0283C", "#2F6CF0"
EMBER = "#FF8A2A"


def blade(h, l=.60):        # polished blades: low shadowAmt so the shade band stays mid-light (warrior review 6)
    return Region(h, .15, l)


def metal(h, l=.50, s=.42):
    return Region(h, s, l)


def wood(h, l=.40):
    return Region(h, .42, l)


def cloth(h, l=.32):
    return Region(h, .42, l)


def glow(h, e=1.0):
    return Region(h, 0.0, 0.0, e=e)


def stone(h, e=0.3):        # cut gem in a fitting: toon-banded, slightly self-lit
    return Region(h, .35, .45, e=e)


FULLER = Region("#4A5468", .30, .20)        # dark fuller line on steel (warrior: rgba(70,80,100,.8))

SWORD = {
    "T1": dict(blade=blade(STEEL), fuller=FULLER, guard=metal(IRON, .45), grip=cloth(LEATHER),
               wrap=cloth(DARK_LEATHER), pommel=metal(IRON, .45), gem=metal(STEEL, .55)),
    # w_rusty_sword: blade #A89684 with rust blotches, guard / pommel #756C66, dark-leather grip
    "Worn": dict(blade=Region("#A89684", .30, .35), fuller=Region("#9A5028", .40, .25), guard=metal("#756C66", .32),
                 grip=cloth(DARK_LEATHER), wrap=cloth("#3A2416"), pommel=metal("#756C66", .32),
                 gem=metal("#5A524E", .25)),
    # w_broad_sword: steel, bronze cross guard and pommel
    "T2": dict(blade=blade(STEEL), fuller=FULLER, guard=metal("#D98A3C", .55), grip=cloth("#7A4A26"),
               wrap=cloth("#4A2A14"), pommel=metal("#D98A3C", .55), gem=metal("#F0B870", .55)),
    # w_militia_sword: bright steel, iron cross, blue grip
    "T3": dict(blade=blade("#D3DBE4"), fuller=FULLER, guard=metal("#9DB0C8", .50), grip=cloth("#2F62C8"),
               wrap=cloth("#1E3F86"), pommel=metal("#9DB0C8", .50), gem=stone("#4A7AD8", .25)),
    # w_claymore: dark-iron cross, gold pommel, ruby
    "T4": dict(blade=blade("#D8E0EA"), fuller=FULLER, guard=metal("#343A48", .45), grip=cloth("#5A1E28"),
               wrap=cloth("#2E0E14"), pommel=metal(GOLD), gem=stone(RUBY, .35)),
    # w_flamberge: warm steel, gold wings, crimson grip, ember gem + glow (the fuller burns)
    "T5": dict(blade=blade("#E6D6C8"), fuller=glow(EMBER), guard=metal(GOLD), grip=cloth("#8A1F24"),
               wrap=cloth("#5A1418"), pommel=metal(GOLD), gem=glow("#FF7A1A")),
    # w_demon_blade: dark blade, demon iron, crimson glow
    "T6": dict(blade=blade("#4A3446", .45), fuller=glow(CRIMSON), guard=metal(DEMON, .40), grip=cloth(DEMON_BLACK),
               wrap=cloth("#1C1520"), pommel=metal(DEMON, .40), gem=glow(CRIMSON)),
}

AXE = {
    "T1": dict(haft=wood(WOOD), wrap=cloth(DARK_LEATHER), butt=metal(IRON, .45), head=Region(STEEL, .25, .50),
               edge=blade("#EEF2F6"), socket=metal(DARK_IRON, .40), rivet=metal(IRON, .45)),
    "T2": dict(haft=wood(WOOD), wrap=cloth("#2F5A3A"), butt=metal(BRONZE), head=Region(STEEL, .25, .50),
               edge=blade("#EEF2F6"), socket=metal(BRONZE), rivet=metal(GOLD)),               # web drawAxe rivets
    "T3": dict(haft=wood("#7A4A2E"), wrap=cloth("#2F62C8"), butt=metal("#9DB0C8", .50), head=Region("#D3DBE4", .25, .50),
               edge=blade("#F4F7FA"), socket=metal("#9DB0C8", .50), rivet=metal(STEEL)),
    "T4": dict(haft=wood(DARK_WOOD), wrap=cloth("#5A1E28"), butt=metal(GOLD), head=Region("#B9C3D0", .25, .50),
               edge=blade("#F6F8FB"), socket=metal(GOLD), rivet=stone(RUBY, .35)),
    "T5": dict(haft=wood("#5A3020"), wrap=cloth("#8A1F24"), butt=metal(GOLD), head=Region("#E6D6C8", .25, .50),
               edge=glow(EMBER), socket=metal(GOLD), rivet=glow("#FF7A1A")),
    "T6": dict(haft=cloth(DEMON_BLACK), wrap=cloth(DEMON), butt=metal(DEMON, .40), head=Region("#4A3446", .20, .45),
               edge=glow(CRIMSON), socket=metal(DEMON, .40), rivet=glow(CRIMSON)),
}

MACE = {   # web drawMace: dark-wood haft, leather wraps, iron flanged head, gold cap
    "T1": dict(haft=wood(DARK_WOOD), wrap=cloth(LEATHER), butt=metal(IRON, .45), head=metal(IRON, .50),
               flange=metal("#A2AAB6", .50), cap=metal(GOLD, .55), collar=metal(DARK_IRON, .40)),
    "T2": dict(haft=wood(DARK_WOOD), wrap=cloth("#2F5A3A"), butt=metal(BRONZE), head=metal(STEEL, .50),
               flange=metal(STEEL, .55), cap=metal(BRONZE, .55), collar=metal(BRONZE)),
    "T3": dict(haft=wood("#7A4A2E"), wrap=cloth("#2F62C8"), butt=metal("#9DB0C8", .50), head=metal("#D3DBE4", .45),
               flange=metal("#9DB0C8", .50), cap=metal("#9DB0C8", .50), collar=metal("#6C7488", .45)),
    "T4": dict(haft=wood(DARK_WOOD), wrap=cloth("#5A1E28"), butt=metal(GOLD), head=metal("#5A6070", .45),
               flange=metal(GOLD), cap=stone(RUBY, .35), collar=metal(GOLD)),
    "T5": dict(haft=wood("#5A3020"), wrap=cloth("#8A1F24"), butt=metal(GOLD), head=metal(GOLD),
               flange=metal("#F0CE78"), cap=glow(EMBER), collar=metal("#8A1F24", .35)),
    "T6": dict(haft=cloth(DEMON_BLACK), wrap=cloth("#1C1520"), butt=metal(DEMON, .40), head=metal("#4A3446", .40),
               flange=metal(DEMON, .40), cap=glow(CRIMSON), collar=metal(DEMON_BLACK, .35)),
}

DAGGER = {
    "T1": dict(blade=blade(STEEL), edge=blade("#EEF2F6", .55), guard=metal(IRON, .45), grip=cloth(LEATHER),
               wrap=cloth(DARK_LEATHER), pommel=metal(IRON, .45), gem=metal(STEEL, .55)),
    # w_stiletto: silver needle, gold disc guard and pommel, violet-black grip
    "T2": dict(blade=blade(SILVER), edge=blade("#F6F9FC", .55), guard=metal(GOLD), grip=cloth("#3A2F4A"),
               wrap=cloth("#262030"), pommel=metal(GOLD), gem=stone(SAPPHIRE, .30)),
    # w_kris: pale blade, bronze, wooden grip
    "T3": dict(blade=blade("#CFC4B0"), edge=blade("#F2EADC", .55), guard=metal(BRONZE), grip=wood(WOOD),
               wrap=cloth(DARK_WOOD), pommel=metal(BRONZE), gem=metal("#E8B070", .55)),
    # w_assassin_blade: green-steel, dark iron, poison drip #7BE03A
    "T4": dict(blade=blade("#BFD0C2"), edge=glow("#7BE03A", .9), guard=metal(DARK_IRON, .40), grip=cloth("#2C3A2C"),
               wrap=cloth("#1C261C"), pommel=metal(DARK_IRON, .40), gem=stone("#7BE03A", .45)),
    # w_shadow_blade: violet steel, violet glow
    "T5": dict(blade=blade("#5A4A82", .50), edge=glow(VIOLET), guard=metal(DARK_IRON, .40), grip=cloth(DEMON_BLACK),
               wrap=cloth("#1C1520"), pommel=metal(DARK_IRON, .40), gem=glow(VIOLET)),
    # w_abyssal_dagger: near-black blade, demon iron, crimson glow
    "T6": dict(blade=blade("#3A2838", .45), edge=glow(CRIMSON), guard=metal(DEMON, .40), grip=cloth(DEMON_BLACK),
               wrap=cloth("#1C1520"), pommel=metal(DEMON, .40), gem=glow(CRIMSON)),
}

BOW = {
    "T1": dict(wood=wood("#B67C45"), grain=wood("#DCB894", .30), wrap=cloth("#6A3F24"), band=cloth("#4E2E1A"),
               tip=wood("#8A5A30", .35), string=Region("#F4ECD4", .30, .30)),
    "T2": dict(wood=wood("#8C5530"), grain=wood("#C8A080", .30), wrap=cloth("#2F5A3A"), band=cloth("#234430"),
               tip=Region("#D8C8A0", .35, .45), string=Region("#F4ECD4", .30, .30)),             # long bow, horn tips
    "T3": dict(wood=wood("#A06A3A"), grain=wood("#D2AE86", .30), wrap=cloth("#7A2A24"), band=metal("#E0CFA0"),
               tip=cloth("#3A2A24"), string=Region("#F4ECD4", .30, .30)),                         # composite
    "T4": dict(wood=wood("#5C3A28"), grain=wood("#9A7A62", .30), wrap=cloth("#8A2A24"), band=metal(IRON, .45),
               tip=metal(STEEL, .45), string=Region("#E8E0CC", .30, .30)),                         # war bow
    "T5": dict(wood=Region("#EEE4CC", .38, .35), grain=Region("#FBF6EA", .30, .30), wrap=cloth("#2F5AA0"),
               band=metal(GOLD), tip=metal(GOLD), string=Region("#FFF6DC", .30, .30)),            # eagle bow
    "T6": dict(wood=wood("#2E2640", .35), grain=glow(VIOLET), wrap=cloth("#5A1F2C"), band=metal("#7A4AC0", .45),
               tip=metal("#6A3AA0", .45), string=Region("#C8B8FF", .30, .30)),                    # abyssal bow
}

STAFF = {   # heads: oak · arcane · rune · elder · lich · abyss (web staffSpec haft / orb colours, grip wrap #3E5A8A)
    "T1": dict(shaft=wood(WOOD), grain=wood("#C9A682", .30), wrap=cloth("#3E5A8A"), butt=metal(IRON, .45),
               collar=wood(DARK_WOOD), claw=wood("#8A5A30"), orb=stone("#6EE05A", .45)),
    "T2": dict(shaft=wood("#7A4A2E"), grain=wood("#B08A70", .30), wrap=cloth("#3E5A8A"), butt=metal(IRON, .45),
               collar=metal(GOLD), claw=metal(GOLD), orb=glow("#4F8CFF")),
    "T3": dict(shaft=metal("#6C7488", .40), grain=glow(CYAN), wrap=cloth("#3E5A8A"), butt=metal("#8A94A8", .45),
               collar=metal("#8A94A8", .45), claw=metal("#8A94A8", .45), orb=glow(CYAN)),
    "T4": dict(shaft=wood("#D8CCB0"), grain=wood("#F2EADA", .30), wrap=cloth("#3E5A8A"), butt=metal(GOLD),
               collar=metal(GOLD), claw=wood("#C8BA98"), orb=glow("#FFB84A")),
    "T5": dict(shaft=Region(BONE, .40, .35), grain=Region("#B8AB88", .30, .20), wrap=cloth("#3A4A3A"),
               butt=metal("#8A7A60", .40), collar=metal("#8A7A60", .40), claw=metal("#8A7A60", .40),
               orb=glow("#7DFF9A")),
    "T6": dict(shaft=cloth(DEMON_BLACK), grain=glow(CRIMSON), wrap=cloth(DEMON), butt=metal(DEMON, .40),
               collar=metal(DEMON, .40), claw=metal("#40324E", .40), orb=glow(CRIMSON)),
}

WAND = {   # web wandSpec (T1): violet wood, gold bands, cyan marquise crystal
    # T1 (art review, fifth look): the violet shaft had the hue and value of the mage's robe, and on the warrior /
    # rogue it read as a dark stick with a cyan speck — the rod is a light honey wood now (a value above the robe,
    # the greaves' grey and the rogue's leather, warm against all three) and the web's violet moves to the three
    # rings (``ring``); gold cap, collar and prongs (``band``), the crystal faintly self-lit (e .6)
    "T1": dict(shaft=wood("#C8955C", .50), band=metal(GOLD), ring=wood("#8A44A6", .45), crystal=stone(CYAN, .60),
               cap=metal(GOLD)),
    "T2": dict(shaft=wood("#4A2A5A"), band=metal(SILVER, .55), ring=metal(SILVER, .55), crystal=glow(CYAN),
               cap=metal(SILVER, .55)),
    "T3": dict(shaft=metal("#6C7488", .40), band=metal("#8A94A8", .45), ring=metal("#8A94A8", .45),
               crystal=glow("#4F8CFF"), cap=metal("#8A94A8", .45)),
    "T4": dict(shaft=wood("#D8CCB0"), band=metal(GOLD), ring=metal(GOLD), crystal=glow("#FFB84A"), cap=metal(GOLD)),
    "T5": dict(shaft=Region(BONE, .40, .35), band=metal("#8A7A60", .40), ring=metal("#8A7A60", .40),
               crystal=glow("#7DFF9A"), cap=metal("#8A7A60", .40)),
    "T6": dict(shaft=cloth(DEMON_BLACK), band=metal(DEMON, .40), ring=metal(DEMON, .40), crystal=glow(CRIMSON),
               cap=metal(DEMON, .40)),
}


def _shield(face, seam, rim, band_v, band_h, boss, rivet, back=wood(DARK_WOOD, .30), strap=cloth(DARK_LEATHER)):
    return dict(face=face, seam=seam, rim=rim, band_v=band_v, band_h=band_h, boss=boss, rivet=rivet, back=back,
                strap=strap)


SHIELD = {   # web shieldSpec kinds: wood · iron · tower · kite · rune · abyss
    "T1": _shield(wood("#A8703C", .35), wood("#6E4422", .20), metal(IRON, .45), wood("#9A6434", .35),
                  metal(IRON, .45), metal(IRON, .50), metal(STEEL, .45)),
    "T2": _shield(metal("#A4AEBB", .45), metal("#A4AEBB", .45), metal(DARK_IRON, .40), cloth("#3E5A8A"),
                  metal("#A4AEBB", .45), metal(STEEL, .50), metal(STEEL, .45)),
    "T3": _shield(cloth("#A8323A"), cloth("#A8323A"), metal(DARK_IRON, .40), metal(GOLD), metal(GOLD), metal(GOLD),
                  metal(STEEL, .45)),
    "T4": _shield(cloth("#2F5AA8"), cloth("#2F5AA8"), metal(GOLD), Region("#F0EAD8", .35, .40),
                  Region("#F0EAD8", .35, .40), metal(GOLD), metal(GOLD)),
    "T5": _shield(metal("#4A5468", .40), metal("#4A5468", .40), metal("#8A94A8", .45), glow(CYAN), glow(CYAN),
                  glow(CYAN), metal("#8A94A8", .45)),
    "T6": _shield(cloth("#3A2A4A"), cloth("#3A2A4A"), metal(DEMON, .40), metal(DEMON, .40), metal(DEMON, .40),
                  glow(CRIMSON), metal(DEMON_BLACK, .35), back=cloth(DEMON_BLACK)),
}

FAMILIES = {"sword": SWORD, "axe": AXE, "mace": MACE, "dagger": DAGGER, "bow": BOW, "staff": STAFF, "wand": WAND,
            "shield": SHIELD}

TIER_NAMES = {   # display (previews / manifest)
    "T1": "common", "Worn": "rusty", "T2": "fine", "T3": "tempered", "T4": "masterwork", "T5": "relic",
    "T6": "abyssal",
}


def tables() -> dict[str, dict[str, dict[str, Region]]]:
    """``{family: {variant: {role: Region}}}`` with the Worn atlas = T1 for every family but the sword."""
    out = {}
    for fam, t in FAMILIES.items():
        v = dict(t)
        v.setdefault("Worn", dict(t["T1"]))
        out[fam] = v
    return out
