"""Chapter 2 VFX recipes (绳林 · the Hanging Wood; Docs/dark-remake/chapter2.md §17.3) — registered from
``fx_recipes.build()`` into ``Art/Export/VFX/vfx_recipes.json`` after the dark-remake set (pure Python, no bpy;
``python3 unreal/Art/blender/vfx/fx_recipes.py``).

Package 4R-1 adds only what the first member of ``SKEL_Quadruped`` plays (``monsters/moon_hound.py``); 4R-3 appends
the chapter's own list here (``monster.hanged.drop``, ``monster.fenbound.puddle``, ``notify.net_throw``,
``hazard.lamplight``, the Cantor's moon-tarnish marks, ``notify.howl``, Volgan's, the Barrow's cleanse, the two
ambiences, ``exit.sealed@hangwood``).

Colour script (chapter2.md §4.1 on top of DR5; ``vfx/hue_audit.py`` gates it):

* **moon-tarnish** (:data:`MOON_TARNISH`, the hue-audit constant) round near-black #0C1614 is the fouled seal's light:
  enemy magic that is not a damage hazard — never a ``telegraph.*`` layer, never a hero colour's neighbour;
* **sallow lamplight** #E6D36A is a Lamp-Bearer lantern: it shows, it does not warm; never in the ember band;
* the **Moon-Bitten carry no light**: beasts kick up what they run on — root earth, leaf litter, dust. No glow.

| Id | Bound to | Package |
|---|---|---|
| ``notify.FX_PounceLeap`` | ``SK_Mon_MoonHound`` ``Pounce`` @380 (``Leap``: the paws leave the ground): earth and leaves kicked back from under the hinds | 4R-1 |
| ``notify.FX_PounceLand`` | ``Pounce`` @650 (``Contact``: the fores strike at the dash line's end; the core has moved the actor): a low dust burst and leaf litter under the forehand, claw scores | 4R-1 |
"""
from __future__ import annotations

import copy

from fx_recipes import R, lay
import fx_recipes_dark as FD

MOON_TARNISH = "#9FE0D4"               # = hue_audit.MOON_TARNISH (O4: provisional, see vfx/hue_audit.py)
TARNISH_CORE = "#0C1614"
SALLOW = "#E6D36A"
ROOT_EARTH = "#5C5146"                 # root earth, lit (clods read on the root tile, L* 19.8)
ROOT_DUST = "#6E6456"
LEAF_LITTER = "#5E5236"                # dead leaves (hue 42, sat .43: under the ember band's saturation)
LEAF_LITTER_DARK = "#463A2A"
HIND_AT = [-38.0, 0.0, 0.0]            # under the hound's hinds at the spring (actor frame: forward, right, up cm)
FORE_AT = [44.0, 0.0, 0.0]             # under its forehand at the strike


def moonbitten() -> dict:
    rec = {}
    rec["notify.FX_PounceLeap"] = R("ash", [
        lay(sprite="Rock", randomVariants=2, orient="billboard", blend="translucent", at="origin", height=5,
            offsetCm=HIND_AT, count=5, dir="back", coneDeg=46, speedCmS=[90, 190], upCmS=[110, 210],
            gravityCmS2=1100, lifeMs=[300, 460], sizeCm=[7, 12], spinDegS=[-420, 420], alpha=[1, 1],
            color=ROOT_EARTH),
        lay(sprite="Core", orient="billboard", blend="translucent", at="origin", height=6, offsetCm=HIND_AT,
            count=6, spawnRadiusCm=14, dir="back", coneDeg=70, speedCmS=[60, 150], upCmS=[90, 190],
            gravityCmS2=520, drag=1.6, lifeMs=[420, 700], sizeCm=[4, 7], aspect=0.45, spinDegS=[-520, 520],
            rotationDeg=[0, 360], alpha=[0.95, 0], alphaPow=2.4, altEvery=2, altColor=LEAF_LITTER_DARK,
            color=LEAF_LITTER),
        FD.ash_dust("origin", 3, offsetCm=HIND_AT, radius=12, color=ROOT_DUST, alpha=0.5, size=(30, 46),
                    life=(380, 560), rise=10),
    ], note="the hound's spring (Pounce Leap @380): root earth and dead leaves kicked back from under the hinds "
            "(actor frame: 38 cm behind the root; Blow = its facing). No glow: the Moon-Bitten carry no light")
    rec["notify.FX_PounceLand"] = R("ash", [
        FD.ash_dust("origin", 6, offsetCm=FORE_AT, radius=26, color=ROOT_DUST, alpha=0.58, size=(40, 60),
                    life=(460, 700), rise=12, speed=(40, 110)),
        lay(sprite="Core", orient="billboard", blend="translucent", at="origin", height=6, offsetCm=FORE_AT,
            count=8, spawnRadiusCm=20, dir="out", speedCmS=[70, 170], upCmS=[80, 180], gravityCmS2=560, drag=1.6,
            lifeMs=[420, 720], sizeCm=[4, 7], aspect=0.45, spinDegS=[-520, 520], rotationDeg=[0, 360],
            alpha=[0.95, 0], alphaPow=2.4, altEvery=2, altColor=LEAF_LITTER_DARK, color=LEAF_LITTER),
        lay(sprite="Rock", randomVariants=2, orient="billboard", blend="translucent", at="origin", height=5,
            offsetCm=FORE_AT, count=4, spawnRadiusCm=16, dir="out", speedCmS=[60, 140], upCmS=[120, 200],
            gravityCmS2=1150, lifeMs=[280, 420], sizeCm=[6, 10], spinDegS=[-400, 400], alpha=[1, 1],
            color=ROOT_EARTH),
        lay(sprite="Crack", orient="ground", blend="translucent", at="origin", height=3, offsetCm=FORE_AT,
            sizeCm=74, grow=1.04, lifeMs=1300, fadeIn=0.04, alpha=[0.6, 0], alphaPow=2.2, alignToBlow=True,
            color="#17130F"),
    ], shake=[2, 90],
        note="the hound lands on its target (Pounce Contact @650, at the dash line's end): a low dust burst, leaf "
             "litter and clods under the forehand, claw scores in the earth for 1.3 s (actor frame: 44 cm ahead "
             "of the root; Blow = its facing; the crimson line decal is the core's telegraph: this is the "
             "contact). No glow")
    return rec


BINDINGS = {
    "notify.FX_PounceLeap": ("SK_Mon_MoonHound Pounce @380 (Leap; Blow = its facing)", "4R-1"),
    "notify.FX_PounceLand": ("SK_Mon_MoonHound Pounce @650 (Contact, after the core's dash)", "4R-1"),
}
ACCENTS = {
    "moonTarnish": {"hex": MOON_TARNISH, "core": TARNISH_CORE, "spec": "#9FE0C8",
                    "use": "the fouled Moon Seal: Wood's Dead eyes, the Cantor's raise marks, Volgan's maw, the "
                           "Barrow's root glow; never a telegraph, never within 30 deg of a saturated hero colour",
                    "o4": "provisional: the spec hex fails the hue audit's tarnish rule against the hero greens "
                          "(hue_audit report 'o4'); this is the nearest hue that passes"},
    "sallowLamplight": {"hex": SALLOW, "use": "Lamp-Bearer lanterns and the Chantry's poles; never ember, never a "
                                              "refuge light"},
    "moonBitten": "no light: silver-white eye-shine is a palette swatch without emissive; their recipes draw earth, "
                  "leaves and dust only",
}


def recipes() -> dict:
    out = {}
    for group in (moonbitten,):
        out.update(group())
    FD.normalise(out)
    return out


def apply(recs: dict) -> dict:
    """Register the Chapter 2 recipes into ``fx_recipes.build()``'s working set (before its post passes). Returns a
    report."""
    new = recipes()
    replaced = sorted(k for k in new if k in recs)
    recs.update(copy.deepcopy(new))
    return {"added": sorted(k for k in new if k not in replaced), "replaced": replaced}


def doc_section(report: dict) -> dict:
    """The ``hangwood`` section of vfx_recipes.json: bindings and the chapter's accent contract."""
    return {
        "_doc": "Chapter 2, the Hanging Wood (vfx/fx_recipes_hangwood.py; chapter2.md 17.3): bindings of the "
                "chapter's recipes by package and the accent colours the hue audit gates",
        "bindings": {k: {"event": e, "package": w} for k, (e, w) in sorted(BINDINGS.items())},
        "accents": ACCENTS,
        "added": report.get("added", []),
        "replaced": report.get("replaced", []),
    }
