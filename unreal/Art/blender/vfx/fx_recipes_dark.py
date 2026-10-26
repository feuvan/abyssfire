"""Dark-remake VFX recipes (焚境 · The Burning March, work package A12) — registered from ``fx_recipes.build()`` into
``Art/Export/VFX/vfx_recipes.json`` (pure Python, no bpy; ``python3 unreal/Art/blender/vfx/fx_recipes.py``).

Colour script (monsters.md §1.2 DR5, story-bible tone pillars — aftermath not gore, ash not blood-splatter):

* **breach-violet** #B02FD0 around near-black #1A0820 (pinpoints #D08CFF, hot core #F0E0FF) is enemy magic that is
  not a damage hazard: breach fire, raises, the Corpseherd's bell, violet death bursts of the breach-touched;
* **crimson rim #D11F2E + bone fill #FFE8DC** is the enemy damage telegraph (``M_AF_Telegraph``, U2) and nothing else;
* **ash** (#EEEAE2 / #B8B2A6 / #8A8480 / #3A3631) is how things end: the dead crumble, bells shake ash off, ash falls;
* **ember orange** is never drawn by an enemy recipe (the hue audit's ember rule gates it); the living carry no light.

What this module adds (ids by binding; W1 = bound by today's runtime, W2 = forward-compatible, played by the U2 / U3
packages; ``doc_section()`` lists every id with its event and owner, written to the JSON's ``dark`` section):

| Id | Bound to | Wave |
|---|---|---|
| ``ambient.ashfall`` | zone weather ambience ``ashfall`` (``emerald_plains``) — event colour = the weather tint #CFCAC0 | W1 |
| ``ambient.sparks`` (re-palette) | ambience ``sparks``: the old ember motes when no tint (abyss_rift), the weather tint when given (the Breach Rim #B02FD0) | W1 |
| ``elite.<affix>`` ×7 (recoloured) + ``elite.unburied`` | the elite affix loops (Scorched = breach fire, Driven = ash wind, Breachwalker = violet, Brutish = crimson, Dirgecursed = bell ripples, Breach-Drinker = tar + violet, Gravecold = grave-cold grey-blue) | W1 |
| ``teleport.blink`` (recoloured) | EvEntityTeleported EliteBlink: a violet smear | W1 |
| ``notify.FX_Fuse`` / ``notify.FX_Burst`` | the cinderbloat's Death clip (the fuse, then the violet-black burst) | W1 |
| ``notify.FX_BellToll`` / ``notify.FX_bell_ring`` | the Corpseherd's Attack01 bolt release / Cast01 Call the Flock | W1 / W2 |
| ``notify.FX_BellRing`` | the Bonegorger's Attack02 Clapper Slam (an impact: a cracked dust ring; no violet: beasts) | W2 |
| ``notify.FX_Roar`` / ``notify.FX_Brand`` | Valdras' Roar (breach flare up the standard) / Cast01 Oath-Brand slam | W1 / W2 |
| ``notify.FX_Land`` / ``FX_DrumBeat_1..4`` / ``FX_Dig`` / ``FX_BannerSnap`` | breach-blood landings, the drummer's beats (dust, no glow), the burial detail's spade, the banner's ash | W1 / W2 |
| ``death.<family>`` (``unburied``, ``breach``, ``living``, ``gravegnawers``) + ``death.valdras`` / ``death.cinderbloat`` | monster kill: PlayFirst({death.<defId>, death.<family>, death.burst}), Blow = the victim's facing | runtime change |
| ``elite.fire_enhanced.death`` / ``elite.frozen.death`` | W2 on-death affix effects (the Scorched burst at +1 s, the Gravecold frost ring at +600 ms) | W2 |
| ``monster.dead_standard_bearer.aura`` | the standard-bearer's +15 % aura loop (cloth-dark crimson, no rim: DECISIONS_DARK DR5-aura) | W2 |
| ``monster.raise.mark`` / ``monster.raise`` | the raise marker ring (900 ms) / EvMonsterRaised | W2 |
| ``monster.block`` | EvBlock (shield-wall frontal block) | W2 |
| ``monster.ash_flakes`` | per-monster loop (cinderbloat ``fx.ashFlakes``) | runtime change |
| ``monster.valdras.standard_fire`` / ``monster.valdras.feed`` | Valdras P3 standard ignition loop / Feed | W2 |
| ``telegraph.contact`` (+ ``.line``, ``.cone``) | the telegraph decal's contact beat (U2), scaled by the context radius | W2 |
| ``hazard.breach_fire`` / ``hazard.breach_seep`` / ``hazard.banner_ring`` | EvHazard loops: the Oath-Brand brand, the seep lines, the closing banner ring | W2 |
| ``corpse.crumble`` / ``corpse.expire`` / ``corpse.burn`` / ``corpse.consume`` | the ash husk (U3): crumble, fade at TTL, burned, eaten | W2 |
| ``notify.FX_Frenzy`` / ``monster.oath_zealot.frenzy`` | the zealot's frenzy: Roar's chant (breath and dust, no light) / the loop for the rest of the phase (2H) | W2 |
| ``notify.FX_Keen`` | the Corpseherd's Keen (Cast02 @800): violet sound rings, a hairline ripple out to r 4, ash blown outward (2H) | W2 |
| ``status.ashnumb`` (+ ``ASHNUMB`` contract) | 麻灰 Ashnumb on the hero: ash flaking off the limbs; the per-stack grey band is the contract (2H) | W2 |
| ``prop.echo`` | the echo lore look (U1: the figure fades in, holds, fades out, 3 s): cold ash motes | W1 (U1) |
| ``camp.bell_toll`` | the Knellwatch bell frame's ``bell`` socket (story tolls, set-piece audio cue) | W1 (U4 / U7) |
| ``camp.torch@march`` | the march brazier (``camp_torch@march``): a smoulder — smoke, dull sparks, ash, **no light** (env art review: ``camp.torch``'s flame + #FFB060 light at every brazier broke chapter1.md §15) | runtime (U1: themed recipe id) |

And the hue-audit clean-up of today's hero recipes (DR5, ``vfx/hue_audit.py``): the rogue's ``shadow`` palette moves to
a dusk violet-grey under saturation .5 (the breach-violet guard band), ``skill.vanish``'s scorch follows it, and the
four hero ground layers that read as a filling enemy telegraph stop filling (the poison / bleed puddles start nearer
their size, the meteor's incoming shadow grows < 1.5×).

Material contracts the UE content build implements (also written to ``manifest.json → materials`` by
``fx_textures_dark.py``): ``M_AF_Telegraph`` (``TELEGRAPH``) and ``M_AF_AshDissolve`` (``ASH_DISSOLVE``).
"""
from __future__ import annotations

import copy
import math

import fx_recipes as FR
from fx_recipes import R, flat, lay, px, RING_R

# ── palettes (added to the recipe document; the runtime reads them from the JSON) ─────────────────────────
DARK_PAL = {
    "breach":  ("#F0E0FF", "#D08CFF", "#B02FD0", "#1A0820", 3),
    "ash":     ("#EEEAE2", "#B8B2A6", "#8A8480", "#3A3631", 6),
    "hazard":  ("#FFF4EE", "#FFE8DC", "#D11F2E", "#4A0810", 5),
    "echo":    ("#F4F8FF", "#DCE4F2", "#A8B0C4", "#2E3440", 6),
    "crimson": ("#F2B8B4", "#C0283A", "#A3221E", "#3A0A0C", 5),
    "gravecold": ("#F2F6FA", "#C8D8E6", "#7E98B0", "#1E2A36", 1),
    # ambient.sparks' own palette: mid = the old fixed mote colour, so a context without a tint draws exactly what
    # it drew before (abyss_rift's ember storm) and a tinted context (the Breach Rim) draws the tint
    "embers":  ("#FFE0B0", "#FF6A3A", "#E8401A", "#5A1A08", 0),
}
# the rogue's shadow, out of the breach-violet guard band (hue 288° ± 30° at HSV sat > .5): a dusk violet-grey whose
# saturated slots stay under .5 — still the rogue's smoke, never the enemy's magic
SHADOW_PAL = ("#ECE6FA", "#A898D8", "#5E5294", "#1C1828", 3)

VIOLET, VIOLET_DARK, VIOLET_PIN, VIOLET_HOT = "#B02FD0", "#1A0820", "#D08CFF", "#F0E0FF"
VIOLET_DEEP = "#3A0E4A"               # translucent bodies under violet rings (presence on the dark ground)
CRIMSON, BONE = "#D11F2E", "#FFE8DC"  # the hazard pair (telegraphs only)
ASH_LIGHT, ASH_MID, ASH_DARK, ASH_SOOT = "#B8B2A6", "#8A8480", "#5E5A54", "#2E2B28"
TAR = "#2A0E14"                        # breach-blood shell (A1 clot.shell)
ECHO_BODY, ECHO_RIM = "#A8B0C4", "#DCE4F2"   # Props_March echo.* (A10's MI_AF_Echo)
STEEL_SPARK = "#FFF4D6"               # pale steel sparks (hue 44°, sat .16: never ember)
BELL_CREAM = "#E8E0CC"                # bone / cream bell tones (beasts and the keepers' bell: no violet)
SMOULDER_SMOKE = "#3E3833"            # the march brazier's smoke (warm grey-brown, under the ash haze)
SMOULDER_SMOKE_LIT = "#6A645B"        # VFX r5: #3E3833 matched the mud's value and vanished from the game camera
SMOULDER_SPARK = "#7A3420"            # its few sparks: dull red, hue 13°, value .48 — below the ember band

FB = {"tall": 0, "forked": 1, "lick": 2, "wisp": 3}   # T_FX_FlameBreach cells


def ring_size(r_cm: float) -> float:
    """Quad size of a ``Ring`` / ``Ripple`` of radius ``r_cm`` (the ring sits at 0.4375 of the quad)."""
    return round(r_cm / RING_R, 1)


SIGIL_R = 104.0 / 256.0               # T_FX_RaiseSigil ring radius / quad


# ── small builders ─────────────────────────────────────────────────────────────────────────────────────
def ground_ring(at, r0, r1, life, color, *, alpha=0.8, body=VIOLET_DEEP, body_alpha=0.5, sprite="Ring", delay=0,
                pow=2.0, h=3, **kw):
    """A ground ring r0 → r1 (cm) as two layers: a translucent body (presence on the dark ground — additive light
    alone vanishes on #3A332D mud) + the additive line."""
    size = ring_size(r0)
    grow = round(r1 / max(r0, 0.1), 3)
    out = []
    if body:
        out.append(lay(sprite=sprite, orient="ground", blend="translucent", at=at, height=h, sizeCm=size, grow=grow,
                       sizePow=pow, lifeMs=life, alpha=[body_alpha, 0], fadeIn=0.06, alphaPow=1.4, color=body,
                       delayMs=delay, **kw))
    out.append(lay(sprite=sprite, orient="ground", at=at, height=h + 1, sizeCm=size, grow=grow, sizePow=pow,
                   lifeMs=life, alpha=[alpha, 0], fadeIn=0.06, alphaPow=1.4, color=color, intensity=1.2,
                   delayMs=delay, **kw))
    return out


def breach_flames(at, n, *, radius=30.0, rise=(70, 150), life=(380, 560), size=(48, 76), h=0, cells=4, delay=0,
                  on_ring=False, rate=None, duration=None, attach=None, variant=None, **kw):
    """Violet-black flame tongues (``T_FX_FlameBreach``, pre-coloured: white tint), upright, translucent."""
    d = dict(emitter="flames", sprite="FlameBreach", blend="translucent", orient="upright", at=at, height=h,
             spawnRadiusCm=radius, onRing=on_ring or None, upCmS=list(rise), lifeMs=list(life), sizeCm=list(size),
             aspect=0.5, grow=0.7, sizePow=2, alpha=[1, 0], fadeIn=0.12, alphaPow=1.3, color="white",
             intensity=1.0, delayMs=delay, attach=attach)
    if variant is not None:
        d["variant"] = variant
    else:
        d["randomVariants"] = cells
    if rate:
        d.update(rate=rate, durationMs=duration if duration is not None else -1)
    else:
        d["count"] = n
    d.update(kw)
    return lay(**d)


def violet_glow(at, size, life, *, alpha=0.5, h="chest", delay=0, grow=1.3, **kw):
    return lay(emitter="glow", at=at, height=h, sizeCm=size, grow=grow, lifeMs=life, alpha=[alpha, 0],
               color=VIOLET, intensity=1.2, delayMs=delay, **kw)


def ash_flakes(at, n, *, h=80, radius=30.0, speed=(40, 120), up=(60, 160), gravity=320.0, life=(700, 1200),
               size=(7, 11), color=ASH_LIGHT, alpha=0.9, delay=0, dir="random", **kw):
    """Cel ash flakes (``T_FX_AshFlake``, translucent) thrown and settling."""
    return lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at=at, height=h,
               count=n, spawnRadiusCm=radius, speedCmS=list(speed), upCmS=list(up), dir=dir,
               gravityCmS2=gravity, drag=1.6, lifeMs=list(life), sizeCm=list(size), grow=0.9, alpha=[alpha, 0],
               fadeIn=0.05, alphaPow=2.2, spinDegS=[-240, 240], rotationDeg=[0, 360], color=color, delayMs=delay,
               **kw)


def ash_dust(at, n, *, h=8, radius=30.0, color=ASH_DARK, alpha=0.6, size=(40, 64), life=(600, 900), delay=0,
             speed=(20, 60), rise=8.0, **kw):
    """Low ash / dirt dust (``T_FX_Dust``), translucent."""
    return lay(sprite="Dust", orient="billboard", blend="translucent", at=at, height=h, count=n,
               spawnRadiusCm=radius, onRing=True if radius else None, dir="out", speedCmS=list(speed),
               gravityCmS2=-rise, drag=2.4, lifeMs=list(life), sizeCm=list(size), grow=1.45, sizePow=1.5,
               alpha=[alpha, 0], fadeIn=0.1, alphaPow=1.4, color=color, delayMs=delay, **kw)


def soot(at, n, *, h=40, radius=20.0, color="#2E2834", alpha=0.7, size=(36, 56), life=(700, 1100), rise=40.0,
         delay=0, **kw):
    return lay(emitter="smoke", sprite="Smoke", randomVariants=2, at=at, height=h, count=n, spawnRadiusCm=radius,
               speedCmS=[10, 40], lifeMs=list(life), sizeCm=list(size), grow=1.6, gravityCmS2=-rise,
               alpha=[alpha, 0], color=color, delayMs=delay, **kw)


def violet_motes(at, n, *, h="chest", radius=20.0, speed=(30, 90), rise=90.0, life=(500, 900), size=(6, 10),
                 color=VIOLET_PIN, delay=0, alpha=1.0, **kw):
    return lay(emitter="motes", sprite="Ember", at=at, height=h, count=n, spawnRadiusCm=radius,
               speedCmS=list(speed), gravityCmS2=-rise, lifeMs=list(life), sizeCm=list(size), color=color,
               altEvery=3, altColor=VIOLET_HOT, alpha=[alpha, 0], delayMs=delay, intensity=1.4, **kw)


def ripple_ground(at, r0, r1, life, color, *, alpha=0.75, body=None, body_alpha=0.45, delay=0, h=3, pow=1.6, **kw):
    return ground_ring(at, r0, r1, life, color, alpha=alpha, body=body, body_alpha=body_alpha, sprite="Ripple",
                       delay=delay, pow=pow, h=h, **kw)


def ripple_air(at, s0, s1, life, color, *, alpha=0.8, h="head", delay=0, offset=None, blend=None, **kw):
    """A billboard ripple (the bell's tone in the air) growing ``s0`` → ``s1`` cm quad."""
    return lay(sprite="Ripple", orient="billboard", blend=blend, at=at, height=h, sizeCm=s0,
               grow=round(s1 / s0, 3), sizePow=1.6, lifeMs=life, alpha=[alpha, 0], fadeIn=0.05, alphaPow=1.3,
               color=color, intensity=1.2, delayMs=delay, offsetCm=offset, **kw)


def held_ground(sprite, at, size, color, a, P, *, h=9, blend="translucent", grow=1.06, **kw):
    """A ground mark that stays (``fx_recipes.held``: steady + pulse pair)."""
    base = dict(sprite=sprite, orient="ground", blend=blend, at=at, height=h, sizeCm=size, grow=grow, sizePow=1,
                color=color, **kw)
    return FR.held(base, a, P)


# ── ambience ───────────────────────────────────────────────────────────────────────────────────────────
def ambience() -> dict:
    rec = {}
    # falling ash round the hero (a camera volume like ambient.dust_motes): the flakes fall slowly with a drift and
    # a lazy spin, specks between them; event colour = the weather tint (#CFCAC0 in the march data) when the
    # runtime passes it, the ash palette's mid otherwise
    rec["ambient.ashfall"] = R("ash", [
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="camera",
            durationMs=-1, rate=28, spawnRadiusCm=900, height=230, spawnHeightJitterCm=200, dir="blow",
            coneDeg=50, speedCmS=[10, 26], upCmS=[-62, -40], gravityCmS2=0, drag=0, lifeMs=[3600, 4800],
            sizeCm=[10, 16], grow=1.0, alpha=[0.9, 0], fadeIn=0.1, alphaPow=5, spinDegS=[-80, 80],
            rotationDeg=[0, 360], color="event"),
        lay(sprite="Core", orient="billboard", blend="translucent", at="camera", durationMs=-1, rate=30,
            spawnRadiusCm=900, height=230, spawnHeightJitterCm=220, dir="blow", coneDeg=60, speedCmS=[8, 20],
            upCmS=[-54, -34], gravityCmS2=0, drag=0, lifeMs=[4000, 5200], sizeCm=[5, 8], grow=1.0,
            alpha=[0.7, 0], fadeIn=0.1, alphaPow=4, color="event"),
    ], note="falling ash (chapter1.md §15 march mood): 28 flakes + 30 specks per second over a 9 m disc round the "
            "hero, falling 35-60 cm/s with a drift along world +X (~120 + ~140 alive); the weather tint (#CFCAC0) "
            "colours them")
    return rec


# ── elite affix recolours (DR5: no enemy ember; each affix keeps one readable hue on the dark ground) ─────────
def _aura(at, disc, disc_alpha, rim, rim_alpha, *, puff=None, puff_alpha=0.28, P=600):
    """The affix base (as fx_recipes.aura, re-coloured): a translucent Shock disc + a puff + a crisp additive rim,
    pulsing (rate .85 /s, life 1.2 s, α a → .4 a)."""
    return [
        lay(sprite="Shock", orient="ground", blend="translucent", at=at, height=2, rate=0.85, attach=True,
            sizeCm=120, grow=1.2, sizePow=1, lifeMs=1200, fadeIn=0.25, alpha=[disc_alpha, round(disc_alpha * 0.4, 3)],
            color=disc),
        lay(sprite="Puff", orient="ground", blend="translucent", at=at, height=1.5, rate=0.85, attach=True,
            sizeCm=118, grow=1.2, sizePow=1, lifeMs=1200, fadeIn=0.25, alpha=[puff_alpha, round(puff_alpha / 3, 3)],
            color=puff or disc),
        lay(sprite="Ring", orient="ground", at=at, height=3, rate=0.85, attach=True, sizeCm=137, grow=1.2,
            sizePow=1, lifeMs=1200, fadeIn=0.25, alpha=[rim_alpha, round(rim_alpha * 0.16, 3)], color=rim,
            intensity=1.1),
    ]


def elites() -> dict:
    rec = {}
    # 焦灼 Scorched (fire_enhanced): breach fire licking at the feet — black tongues with violet edges on a soot disc
    rec["elite.fire_enhanced"] = R("breach", [
        # the shared affix rim thinned (.55 → .22): read first as "a violet ring at the feet" (VFX r5); the flames lead
        _aura("target", VIOLET_DARK, 0.62, VIOLET, 0.22, puff="#3A0E4A", puff_alpha=0.35),
        breach_flames("target", 0, radius=36, on_ring=True, rate=12, attach=True, rise=(40, 90), life=(520, 760),
                      size=(56, 84), h=0),
        breach_flames("target", 0, radius=18, rate=2.5, attach=True, rise=(50, 90), life=(500, 700),
                      size=(30, 44), h=0, variant=FB["wisp"]),
        violet_motes("target", 0, h=20, radius=38, rate=3, durationMs=-1, attach=True, rise=70,
                     life=(700, 1100), size=(5, 8)),
        lay(emitter="glow", orient="ground", at="target", height=4, rate=0.85, attach=True, sizeCm=150,
            grow=1.1, lifeMs=1200, fadeIn=0.3, alpha=[0.3, 0.06], color=VIOLET),
    ], note="Scorched (焦灼): breach fire at the feet (violet-black tongues on a soot disc), violet motes; never ember")
    # 驱策 Driven (swift): pale ash wind round the legs — orbiting streaks, a dust skirt
    rec["elite.swift"] = R("ash", [
        _aura("target", "#4A4640", 0.5, "#E8E4DC", 0.42, puff="#6E6A62", puff_alpha=0.3),
        held_ground("Swirl", "target", 150, "#2E2B28", 0.5, 500, h=5, spinDegS=-420, rotationDeg=[0, 360]),
        held_ground("Swirl", "target", 146, "#EEEAE2", 0.6, 500, h=6, blend=None, spinDegS=-420,
                    rotationDeg=[0, 360], intensity=1.1),
        lay(sprite="Dust", orient="billboard", blend="translucent", at="target", height=6, rate=1.6,
            attach=True, spawnRadiusCm=40, onRing=True, dir="tangent", speedCmS=[60, 110], lifeMs=[500, 700],
            sizeCm=[36, 50], grow=1.4, alpha=[0.5, 0], fadeIn=0.12, color="#5E5A54"),
    ], note="Driven (驱策): a pale wind swirl spinning at the feet (Swirl, ash white over a dark under-swirl) and dust "
            "whipped round (grey; no colour of its own)")
    # 渊行 Breachwalker (teleporting): violet flicker, wisps that wink out
    rec["elite.teleporting"] = R("breach", [
        # no violet rim line (VFX r5: the shared rim read first as "a violet ring at the feet"): a dark disc only
        _aura("target", VIOLET_DEEP, 0.5, "#C890FF", 0.0, puff="#2A0A36", puff_alpha=0.28)[:2],
        # the body smears through space: horizontal violet tears beside it that flicker out — the affix's own
        # shape, so they must carry it: 1-1.3 m long, ~10 a second, spread over the whole body height
        lay(sprite="Streak", orient="billboard", blend="translucent", at="target", height="chest", rate=10,
            attach=True, spawnRadiusCm=40, onRing=True, spawnHeightJitterCm=120, lifeMs=[280, 420],
            sizeCm=[104, 136], aspect=0.26, grow=1.3, rotationDeg=[80, 100], alpha=[0.9, 0], flicker=0.5,
            color=VIOLET_DARK),
        lay(sprite="Streak", orient="billboard", at="target", height="chest", rate=10, attach=True,
            spawnRadiusCm=40, onRing=True, spawnHeightJitterCm=120, lifeMs=[280, 420], sizeCm=[92, 124],
            aspect=0.2, grow=1.3, rotationDeg=[80, 100], alpha=[1.0, 0], flicker=0.5, color=VIOLET,
            intensity=1.45),
        breach_flames("target", 0, radius=30, rate=1.6, attach=True, rise=(60, 110), life=(420, 600),
                      size=(26, 36), h=20, variant=FB["wisp"], spawnHeightJitterCm=40),
    ], note="Breachwalker (渊行): the body smears through space — big flickering violet tears beside it (no "
            "rim ring: the tears lead) — and winking wisps; the blink itself is teleport.blink")
    # 蛮力 Brutish (extra_strong): weight, not light — the ground strains under it (a held crack mark on an iron-dark
    # disc), a pale ash rim pulsing like a heartbeat, dust shoved out round the feet. Never crimson: crimson rings
    # on the ground are the damage telegraph's language (the first pass read as two targeting reticles)
    rec["elite.extra_strong"] = R("ash", [
        _aura("target", "#1E1C1A", 0.72, "#B8B2A6", 0.5, puff="#2E2B28", puff_alpha=0.32),
        held_ground("Crack", "target", 150, "#0E0C0A", 0.8, 700, h=1.5, rotationDeg=[0, 360]),
        lay(sprite="Dust", orient="billboard", blend="translucent", at="target", height=8, rate=5, attach=True,
            spawnRadiusCm=44, onRing=True, dir="out", speedCmS=[30, 70], gravityCmS2=-10, drag=2.4,
            lifeMs=[600, 900], sizeCm=[34, 50], grow=1.45, sizePow=1.5, alpha=[0.5, 0], fadeIn=0.1, alphaPow=1.4,
            color="#77705F"),
        lay(sprite="Rock", randomVariants=2, orient="billboard", blend="translucent", at="target", height=6,
            rate=1.6, attach=True, spawnRadiusCm=36, dir="random", speedCmS=[30, 70], upCmS=[60, 120],
            gravityCmS2=900, lifeMs=[280, 400], sizeCm=[6, 9], spinDegS=[-300, 300], alpha=[1, 1],
            color="#8A7F70"),
    ], note="Brutish (蛮力): weight — an iron-dark disc with a held crack mark, a pale ash heartbeat rim, dust and "
            "grit shoved off the feet; no crimson (the telegraph's colour), no orange")
    # 哀咒 Dirgecursed (curse_aura): "the r4 aura drawn as a ring of bell ripples on the ground" (monsters.md §3)
    rec["elite.curse_aura"] = R("breach", [
        lay(sprite="Puff", orient="ground", blend="translucent", at="target", height=1.5, rate=0.7, attach=True,
            sizeCm=230, grow=1.2, lifeMs=1500, fadeIn=0.4, alpha=[0.5, 0], color="#1E0A28"),
        # ~1-2 ripples alive (rate .5 x 2.2 s; VFX r5: at .9 /s per elite the floor filled with concentric rings
        # that competed with the crimson telegraph rims), alpha down
        lay(sprite="Ripple", variant=1, orient="ground", blend="translucent", at="target", height=3, rate=0.5,
            attach=True, sizeCm=ring_size(55), grow=round(400 / 55, 3), sizePow=1.3, lifeMs=2200, fadeIn=0.08,
            alpha=[0.4, 0], alphaPow=0.9, color="#2E0E3C"),
        lay(sprite="Ripple", variant=1, orient="ground", at="target", height=4, rate=0.5, attach=True,
            sizeCm=ring_size(55), grow=round(400 / 55, 3), sizePow=1.3, lifeMs=2200, fadeIn=0.08,
            alpha=[0.5, 0], alphaPow=0.9, color="#A070C8", intensity=1.1),
        lay(sprite="Ring", orient="ground", at="target", height=3, rate=0.85, attach=True, sizeCm=137,
            grow=1.2, sizePow=1, lifeMs=1200, fadeIn=0.25, alpha=[0.25, 0.05], color="#B48CD8", intensity=1.0),
        lay(emitter="smoke", sprite="Smoke", at="target", height=10, rate=1.4, attach=True, spawnRadiusCm=70,
            onRing=True, dir="out", speedCmS=[6, 14], gravityCmS2=-12, sizeCm=[34, 46], grow=1.4,
            lifeMs=[1300, 1700], alpha=[0.5, 0], fadeIn=0.3, color="#3A2448"),
    ], note="Dirgecursed (哀咒): bell ripples rolling out to the r4 aura edge (Ripple, violet-grey), a dark haze")
    # 噬渊 Breach-Drinker (vampiric): tar disc, rings contracting, tar drawn up into the body
    rec["elite.vampiric"] = R("breach", [
        lay(sprite="Shock", orient="ground", blend="translucent", at="target", height=2, rate=0.85, attach=True,
            sizeCm=150, grow=1.1, sizePow=1, lifeMs=1200, fadeIn=0.25, alpha=[0.92, 0.45], color=VIOLET_DARK),
        # it drinks: rings contract onto the body (every other aura breathes out) and tar spirals in and up
        lay(sprite="Ripple", variant=1, orient="ground", blend="translucent", at="target", height=3, rate=1.2,
            attach=True, sizeCm=ring_size(115), grow=round(38 / 115, 3), sizePow=1.6, lifeMs=820, fadeIn=0.25,
            alpha=[0.4, 0], alphaPow=1.2, color="#2A0A36"),
        # the contracting line a dull tar-violet hairline (the Ripple's fine cell, not the thick Ring; VFX r5: the
        # neon violet ring led the read); the tar drops spiralling in are the affix's own shape
        lay(sprite="Ripple", variant=1, orient="ground", at="target", height=4, rate=1.2, attach=True,
            sizeCm=ring_size(115),
            grow=round(38 / 115, 3), sizePow=1.6, lifeMs=820, fadeIn=0.25, alpha=[0.5, 0], alphaPow=1.2,
            color="#7A2A96", intensity=1.0),
        lay(sprite="Drop", orient="billboard", blend="translucent", at="target", height=8, rate=8.0,
            attach=True, orbit={"r0Cm": [80, 110], "r1Cm": 8, "degS": [120, 170]}, upCmS=[40, 80],
            lifeMs=[700, 900], sizeCm=[28, 38], aspect=0.67, grow=0.55, alpha=[1, 0.3], alphaPow=1.5,
            color=VIOLET_DARK, altEvery=3, altColor="#7A2A96"),
        lay(emitter="motes", sprite="Ember", at="target", height=8, rate=3.0, attach=True,
            orbit={"r0Cm": [80, 110], "r1Cm": 8, "degS": [120, 170]}, upCmS=[40, 80], lifeMs=[700, 900],
            sizeCm=[7, 10], alpha=[1, 0], color=VIOLET_PIN, intensity=1.4),
    ], note="Breach-Drinker (噬渊): a near-black tar disc (maroon drops read as blood); big tar drops and violet motes "
            "spiral in and up, a dull tar-violet hairline contracts onto the body (the other auras breathe out)")
    # 墓寒 Gravecold (frozen): grave-cold grey-blue rime, desaturated (the hero's ice is the saturated blue)
    rec["elite.frozen"] = R("gravecold", [
        _aura("target", "#3E5266", 0.62, "#C8D8E6", 0.45, puff="#2A3846", puff_alpha=0.28),
        held_ground("Frost", "target", 130, "#7E98B0", 0.6, 700, h=6, blend=None, rotationDeg=[0, 360]),
        lay(emitter="smoke", sprite="Smoke", at="target", height=20, rate=2.2, attach=True, spawnRadiusCm=46,
            onRing=True, speedCmS=[4, 12], gravityCmS2=-18, sizeCm=[34, 48], grow=1.5, lifeMs=[1100, 1500],
            alpha=[0.42, 0], fadeIn=0.3, color="#8C9CAC"),
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="target",
            height=150, rate=2.2, attach=True, spawnRadiusCm=50, spawnHeightJitterCm=40, dir="down", coneDeg=20,
            speedCmS=[20, 40], lifeMs=[1000, 1400], sizeCm=[8, 11], spinDegS=[-90, 90], rotationDeg=[0, 360],
            alpha=[0.9, 0], alphaPow=2, color="#C8D8E6"),
    ], note="Gravecold (墓寒): grey-blue rime crusting at the feet, cold breath rising, frozen ash falling "
            "(desaturated: the hero's ice is the saturated blue)")
    # 不葬 Unburied (new affix, Unburied family only): the breach will not let go — violet cracks under the feet,
    # motes seeping up
    rec["elite.unburied"] = R("breach", [
        held_ground("CrackGlow", "target", 120, VIOLET, 0.55, 700, blend=None, intensity=1.1),
        held_ground("Crack", "target", 124, "#120610", 0.7, 700, h=8),
        violet_motes("target", 0, h=6, radius=40, rate=4, durationMs=-1, attach=True, rise=110,
                     life=(700, 1100), size=(5, 8), speed=(4, 12)),
    ], note="Unburied (不葬, forward-compatible: the affix is W2 data): violet cracks under the feet, motes seeping up")
    # W2 on-death effects (monsters.md §3; played at the kill, point = the corpse, beside death.<family>):
    # 焦灼 Scorched — a breach-fire burst after 1 s at r2 under U2's elite-style crimson decal (the decal is the
    # telegraph; this is what burns): the corpse smoulders with breach fire for the second, then notify.FX_Burst's
    # pieces at r2
    B = 1000
    rec["elite.fire_enhanced.death"] = R("breach", [
        breach_flames("point", 0, radius=36, rate=20, duration=B, rise=(40, 90), life=(360, 540), size=(52, 78)),
        violet_motes("point", 0, h=20, radius=30, rate=8, durationMs=B, rise=80, life=(500, 800), size=(5, 8)),
        lay(emitter="glow", orient="ground", at="point", height=4, sizeCm=150, grow=1.15, sizePow=1, lifeMs=B,
            fadeIn=0.0, alpha=[0.12, 0.6], alphaPow=2.2, color=VIOLET),
        lay(emitter="glow", at="point", height=50, sizeCm=200, grow=1.3, lifeMs=260, alpha=[0.8, 0], color=VIOLET,
            delayMs=B),
        lay(emitter="flash", at="point", height=50, sizeCm=40, lifeMs=170, color=VIOLET_HOT, delayMs=B),
        lay(emitter="shock", blend="translucent", at="point", height=2, sizeCm=40, grow=7.0, lifeMs=420,
            alpha=[0.72, 0], fadeIn=0.04, alphaPow=1.1, color=VIOLET_DARK, delayMs=B),
        ground_ring("point", 30, 200, 460, "#B8B2A6", alpha=0.7, body="#3A3631", body_alpha=0.6, pow=3, delay=B),
        breach_flames("point", 16, radius=70, on_ring=True, rise=(160, 300), life=(420, 620), size=(84, 128),
                      delay=B),
        breach_flames("point", 6, radius=26, rise=(220, 340), life=(440, 640), size=(100, 140), variant=FB["tall"],
                      delay=B + 30),
        lay(sprite="Scorch", orient="ground", blend="translucent", at="point", height=1, sizeCm=200, grow=1.08,
            lifeMs=2600, alpha=[0.78, 0], fadeIn=0.03, alphaPow=2.6, rotationDeg=[0, 360], color="#140A12",
            delayMs=B),
        lay(sprite="CrackGlow", orient="ground", at="point", height=2, sizeCm=190, grow=1.05, lifeMs=1100,
            alpha=[0.95, 0], fadeIn=0.02, alphaPow=1.6, rotationDeg=[0, 360], color=VIOLET, delayMs=B),
        ash_flakes("point", 18, h=40, radius=30, speed=(120, 300), up=(180, 400), gravity=760, life=(700, 1200),
                   size=(8, 13), color=ASH_LIGHT, delay=B),
        soot("point", 5, h=50, radius=40, color="#241A2A", alpha=0.75, life=(800, 1200), size=(48, 70), rise=60,
             delay=B),
    ], note="Scorched (焦灼) on death (W2): the corpse smoulders with breach fire for 1 s (U2's elite-style crimson r2 "
            "decal fills over the same second), then a breach-fire burst at r2 — flames on a ring, an ash shock ring, "
            "violet cracks in a scorch, ash thrown")
    # 墓寒 Gravecold — a frost ring r2.5 after 600 ms, slow 30 % for 2.5 s. A slow, not damage: no crimson decal
    # (DR5), so the recipe carries its own pre-cue in the affix's grey-blue — rime fading in over the zone (in place:
    # it never grows from the centre, so it cannot be mistaken for a filling telegraph), then the ring snaps out at
    # 600 ms and the crust holds for the slow
    F = 600
    rec["elite.frozen.death"] = R("gravecold", [
        lay(sprite="Frost", orient="ground", blend="translucent", at="point", height=5, sizeCm=470, grow=1.06,
            sizePow=1, lifeMs=F, fadeIn=0.9, alpha=[0.0, 0.42], alphaPow=1.0, rotationDeg=[0, 360],
            color="#7E98B0"),
        lay(emitter="smoke", sprite="Smoke", at="point", height=12, rate=10, durationMs=F, spawnRadiusCm=200,
            onRing=True, dir="in", speedCmS=[40, 80], gravityCmS2=-10, sizeCm=[40, 56], grow=1.3,
            lifeMs=[500, 700], alpha=[0.45, 0], fadeIn=0.3, color="#8C9CAC"),
        ground_ring("point", 60, 250, 280, "#E4EEF6", alpha=0.9, body="#1E2A36", body_alpha=0.6, pow=2.6, delay=F),
        lay(emitter="flash", at="point", height=40, sizeCm=60, lifeMs=160, color="#F2F6FA", delayMs=F),
        lay(sprite="Frost", orient="ground", blend="translucent", at="point", height=5, sizeCm=500, grow=1.02,
            sizePow=1, lifeMs=2500, fadeIn=0.03, alpha=[0.6, 0], alphaPow=2.2, rotationDeg=[0, 360],
            color="#7E98B0", delayMs=F),
        lay(sprite="Frost", orient="ground", at="point", height=6, sizeCm=490, grow=1.02, sizePow=1, lifeMs=2500,
            fadeIn=0.03, alpha=[0.5, 0], alphaPow=2.4, rotationDeg=[0, 360], color="#C8D8E6", intensity=1.0,
            delayMs=F),
        lay(emitter="smoke", sprite="Smoke", at="point", height=16, rate=5, durationMs=2200, spawnRadiusCm=200,
            speedCmS=[4, 12], gravityCmS2=-16, sizeCm=[40, 56], grow=1.5, lifeMs=[1100, 1500], alpha=[0.38, 0],
            fadeIn=0.3, color="#8C9CAC", delayMs=F),
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="point", height=40,
            count=16, spawnRadiusCm=180, spawnHeightJitterCm=40, dir="up", coneDeg=40, speedCmS=[40, 120],
            gravityCmS2=160, lifeMs=[900, 1400], sizeCm=[8, 11], spinDegS=[-90, 90], rotationDeg=[0, 360],
            alpha=[0.9, 0], alphaPow=2, color="#C8D8E6", delayMs=F),
    ], note="Gravecold (墓寒) on death (W2): grey-blue rime fades in over the r2.5 zone and cold mist draws in "
            "(the pre-cue), at 600 ms a grave-cold ring snaps out and the frost crust holds for the 2.5 s slow; no "
            "crimson (a slow, not damage: DR5)")
    # the elite blink: a violet smear between the two tiles (EvEntityTeleported EliteBlink: origin / target = tiles)
    rec["teleport.blink"] = R("breach", [
        lay(emitter="streak", at="path", height="chest", count=9, dir="blow", coneDeg=4, speedCmS=240,
            lifeMs=[200, 300], sizeCm=56, aspect=0.22, color=VIOLET, alpha=[0.85, 0], intensity=1.3),
        lay(emitter="streak", blend="translucent", at="path", height=70, count=6, dir="blow", coneDeg=6,
            speedCmS=160, lifeMs=[260, 360], sizeCm=64, aspect=0.3, color=VIOLET_DARK, alpha=[0.7, 0]),
        soot("origin", 4, h=60, radius=24, color="#2A1236", alpha=0.75, life=(420, 620)),
        violet_glow("origin", 120, 220, alpha=0.6, h=70),
        ground_ring("origin", 70, 15, 240, VIOLET, alpha=0.9, pow=2),
        ground_ring("target", 15, 80, 260, VIOLET, alpha=0.9, delay=40),
        breach_flames("target", 6, radius=26, on_ring=True, rise=(90, 150), life=(260, 380), size=(30, 44),
                      variant=FB["wisp"], delay=30),
        lay(emitter="flash", at="target", height="chest", sizeCm=22, lifeMs=150, color=VIOLET_HOT, delayMs=40),
    ], note="the elite blink (渊行 Breachwalker): a violet smear along the path, soot where it left, wisps where it lands")
    return rec


# ── monster notifies (PlayNotify: notify.<name>, anchored on the actor, Blow = the actor's forward) ─────────
def notifies() -> dict:
    rec = {}
    # cinderbloat Death: FX_Fuse at 0 ms (the crust swells along its fissures), FX_Burst at 800 ms (the violet-black
    # burst; monsters.md §2.1: "death.burst recoloured: violet-black flame, ash ring; no gore")
    rec["notify.FX_Fuse"] = R("breach", [
        lay(emitter="glow", orient="ground", at="origin", height=4, sizeCm=150, grow=1.25, sizePow=1,
            lifeMs=820, fadeIn=0.0, alpha=[0.12, 0.7], alphaPow=2.2, color=VIOLET),
        # spits off the crust top (from 40 cm inside the body at 8-11 cm they drew as big neon squiggles)
        lay(emitter="sparks", sprite="Streak", at="origin", height="head", rate=12, durationMs=780, dir="up",
            coneDeg=60, speedCmS=[110, 210], gravityCmS2=520, lifeMs=[140, 220], sizeCm=[5, 7], color=VIOLET,
            altEvery=3, altColor=VIOLET_HOT, drag=2, alpha=[1, 0], intensity=1.3),
        lay(sprite="Ring", orient="ground", blend="translucent", at="origin", height=3, sizeCm=ring_size(130),
            grow=round(55 / 130, 3), sizePow=0.7, lifeMs=800, fadeIn=0.2, alpha=[0.0, 0.55], alphaPow=0.6,
            color=VIOLET_DEEP),
        lay(sprite="Ring", orient="ground", at="origin", height=4, sizeCm=ring_size(130), grow=round(55 / 130, 3),
            sizePow=0.7, lifeMs=800, fadeIn=0.2, alpha=[0.0, 0.8], alphaPow=0.6, color=VIOLET, intensity=1.2),
        ash_flakes("origin", 0, h=50, radius=26, rate=14, durationMs=760, speed=(20, 60), up=(40, 110),
                   gravity=260, life=(500, 800), size=(6, 9), color=ASH_MID),
        soot("origin", 0, h=50, radius=14, rate=4, durationMs=760, alpha=0.5, life=(500, 800), size=(26, 38),
             color="#2A1C30"),
    ], note="cinderbloat Death @0 → 800 ms: the fuse — light swelling from below, violet spits, ash shaken off the "
            "crust, a ring tightening (the crimson r2.2 hazard ring is U2's telegraph, not this recipe)")
    rec["notify.FX_Burst"] = R("breach", [
        lay(emitter="glow", at="origin", height=50, sizeCm=210, grow=1.3, lifeMs=260, alpha=[0.8, 0], color=VIOLET),
        lay(emitter="flash", at="origin", height=50, sizeCm=40, lifeMs=170, color=VIOLET_HOT),
        lay(emitter="shock", blend="translucent", at="origin", height=2, sizeCm=40, grow=8.0, lifeMs=420,
            alpha=[0.72, 0], fadeIn=0.04, alphaPow=1.1, color=VIOLET_DARK),
        # the shock ring is ash (A5's fuse.burst.ashRing #8A8480): the violet is the fire's, not a neon band
        ground_ring("origin", 30, 240, 480, "#B8B2A6", alpha=0.7, body="#3A3631", body_alpha=0.6, pow=3),
        breach_flames("origin", 18, radius=80, on_ring=True, rise=(160, 300), life=(420, 620), size=(90, 140)),
        breach_flames("origin", 8, radius=30, rise=(220, 360), life=(460, 660), size=(110, 150),
                      variant=FB["tall"], delay=30),
        lay(sprite="Scorch", orient="ground", blend="translucent", at="origin", height=1, sizeCm=230, grow=1.08,
            lifeMs=2600, alpha=[0.78, 0], fadeIn=0.03, alphaPow=2.6, rotationDeg=[0, 360], color="#140A12"),
        lay(sprite="CrackGlow", orient="ground", at="origin", height=2, sizeCm=210, grow=1.05, lifeMs=1100,
            alpha=[0.95, 0], fadeIn=0.02, alphaPow=1.6, rotationDeg=[0, 360], color=VIOLET),
        ash_flakes("origin", 22, h=40, radius=30, speed=(120, 320), up=(180, 420), gravity=760,
                   life=(700, 1200), size=(8, 13), color=ASH_LIGHT),
        soot("origin", 6, h=50, radius=40, color="#241A2A", alpha=0.75, life=(800, 1200), size=(48, 70),
             rise=60),
        violet_motes("origin", 10, h=40, radius=40, speed=(80, 200), rise=120, life=(500, 900)),
    ], shake=[140, 0.4], note="cinderbloat Death @800 ms: the burst — violet-black flame ring, a soot blast, an "
                             "ash shock ring, cracks glowing violet in a scorch, ash thrown and settling; no gore")
    # breach-blood hop landings (clot / bloat Walk FX_Land; the old slime too): a dull pat of ash
    rec["notify.FX_Land"] = R("ash", [
        lay(sprite="Ring", orient="ground", blend="translucent", at="origin", height=3, sizeCm=ring_size(16),
            grow=3.4, sizePow=2.5, lifeMs=300, alpha=[0.4, 0], alphaPow=1.3, color="#1E1A18"),
        ash_dust("origin", 4, radius=24, color="#77705F", alpha=0.5, size=(28, 40), life=(380, 520),
                 speed=(30, 70), rise=10),
    ], note="a hop landing: a dull dust pat (no colour: the clot's sparks are its own emissive)")
    # the Corpseherd's bell: Attack01 FX_BellToll (the bolt leaves the bell, 500 ms) and Cast01 FX_bell_ring (Call the
    # Flock, 650 ms) — a breach-violet ripple (monsters.md §2.5); the bell rides the crook at fx_bell
    # forward, right, up of fx_bell on the posed rig (fx_preview_dark --measure on the current export, VFX r5):
    # Attack01 @500 (84, 34, 130) / Cast01 @650 (20, 29, 165)
    bell = [84.0, 34.0, 130.0]
    rec["notify.FX_BellToll"] = R("breach", [
        ripple_air("origin", 30, 110, 320, VIOLET, alpha=0.95, h="custom", offset=bell),
        ripple_air("origin", 30, 110, 320, VIOLET_DARK, alpha=0.6, h="custom", offset=bell, blend="translucent"),
        lay(emitter="flash", at="origin", height="custom", offsetCm=bell, sizeCm=16, lifeMs=110, color=VIOLET_PIN),
        violet_motes("origin", 5, h="custom", offsetCm=bell, radius=8, speed=(30, 80), rise=-40,
                     life=(300, 500), size=(5, 7)),
    ], note="the Corpseherd's bell-toll bolt leaving the bell (the bolt is proj.monster_bolt, #B02FD0)")
    bell = [20.0, 29.0, 165.0]
    rec["notify.FX_bell_ring"] = R("breach", [
        ripple_air("origin", 40, 190, 520, VIOLET, alpha=0.95, h="custom", offset=bell),
        ripple_air("origin", 40, 190, 520, VIOLET_DARK, alpha=0.55, h="custom", offset=bell, blend="translucent"),
        ripple_air("origin", 40, 190, 520, VIOLET, alpha=0.8, h="custom", offset=bell, delay=170),
        # two ground ripples, not three (VFX r5: with the raise marks the floor filled with concentric violet)
        ripple_ground("origin", 45, 380, 950, VIOLET, alpha=0.8, body=VIOLET_DEEP, body_alpha=0.45, variant=1),
        ripple_ground("origin", 45, 380, 950, VIOLET, alpha=0.55, body=None, delay=220, variant=1),
        lay(emitter="flash", at="origin", height="custom", offsetCm=bell, sizeCm=20, lifeMs=140, color=VIOLET_HOT),
        violet_motes("origin", 9, h="custom", offsetCm=bell, radius=10, speed=(40, 110), rise=-60,
                     life=(500, 800), size=(5, 8)),
    ], note="Call the Flock (召羊): the bell's toll as violet ripples — in the air at the bell, two rolling "
            "out over the ground toward the 6-tile call (r 3.8 m shown)")
    # the Hungerbell / Bonegorger Clapper Slam (Attack02 Contact = FX_BellRing): a beast — an impact, not a bell tone.
    # VFX r5: it shared the keepers' bell language (camp.bell_toll: the broken bone-cream Ripple at a similar size),
    # so the hostile slam and the friendly story toll read alike. Now: short, cracked, ground-hugging — a fast dust
    # ring, a split in the ground, more dust and grit; no air ripple, no Ripple sprite, no violet
    slam = [90.0, 48.0, 0.0]             # the clapper's tip at Contact (--measure: (90, 48, 11))
    rec["notify.FX_BellRing"] = R("ash", [
        lay(emitter="shock", blend="translucent", at="origin", height=2, offsetCm=slam, sizeCm=50, grow=4.2,
            lifeMs=320, alpha=[0.7, 0], alphaPow=1.2, color="#241E1A"),
        ground_ring("origin", 26, 190, 300, "#CFC6B0", alpha=0.7, body="#2E2924", body_alpha=0.55, pow=3,
                    offsetCm=slam),
        lay(sprite="Impact", variant=1, orient="billboard", at="origin", height=24, offsetCm=slam,
            sizeCm=round(80 / FR.impact_scale("heavy"), 3), grow=1.4, sizePow=3, lifeMs=130, alpha=[1, 0],
            alphaPow=1.6, rotationDeg=[0, 360], color="#E8E0CC", intensity=1.25),
        ash_dust("origin", 14, radius=50, offsetCm=slam, color="#8A8478", alpha=0.72, size=(54, 82),
                 life=(700, 1000), speed=(110, 220), rise=4),
        ash_dust("origin", 5, radius=16, offsetCm=slam, color="#A39C8E", alpha=0.6, size=(46, 64),
                 life=(500, 760), speed=(20, 60), rise=30),
        FR.debris("origin", 11, offsetCm=slam, color="#5E574E", speed=(80, 190)),
        lay(sprite="Crack", orient="ground", blend="translucent", at="origin", height=1, offsetCm=slam,
            sizeCm=210, grow=1.05, lifeMs=1800, alpha=[0.85, 0], fadeIn=0.03, alphaPow=2.4, rotationDeg=[0, 360],
            color="#120E0C"),
    ], shake=[160, 0.5], note="the Clapper Slam: an impact — a fast dust ring hugging the ground, the ground "
                             "split, thick dust and grit (no bell ripple: that is the keepers' camp.bell_toll; the "
                             "Gravegnawers are beasts, not breach-touched: no violet)")
    # Valdras' Roar (boss intro, phase changes; FX_Roar 450 ms): a dust shock and a pale shout, and the breach flaring
    # along his standard. --measure on the current export (VFX r5): banner (-98, 0, 177), standard_top (-80, 0, 225)
    # @450 and (-101, 0, 180) / (-79, 0, 226) @700 — the cloth hangs ~150-180, the pole top at 225: the emitter sits on
    # the banner with a height jitter over cloth and pole, and a short rise so tongues die on the standard (the old
    # [-80, 0, 236] + 160-280 cm/s floated a crown 30-100 cm over the finial)
    banner = [-94.0, 0.0, 190.0]
    rec["notify.FX_Roar"] = R("breach", [
        ground_ring("origin", 60, 320, 620, "#CFCAC0", alpha=0.6, body="#3A3631", body_alpha=0.5, pow=2.5),
        ash_dust("origin", 9, radius=70, color="#5A564E", alpha=0.65, size=(50, 76), life=(700, 1000),
                 speed=(80, 160)),
        ripple_air("origin", 70, 260, 520, "#E8E4DC", alpha=0.45, h="head", offset=[30.0, 0.0, 0.0]),
        breach_flames("origin", 0, radius=16, rate=48, duration=420, rise=(30, 70), life=(260, 380),
                      size=(56, 86), h="custom", offsetCm=banner, spawnHeightJitterCm=76),
        lay(emitter="streak", at="origin", height="custom", offsetCm=banner, count=8, dir="up", coneDeg=30,
            speedCmS=[150, 240], lifeMs=[140, 220], sizeCm=48, aspect=0.18, color=VIOLET, alpha=[0.85, 0],
            spawnHeightJitterCm=50, intensity=1.3),
        violet_glow("origin", 170, 520, alpha=0.55, h="custom", offsetCm=banner, grow=1.4),
        violet_motes("origin", 12, h="custom", offsetCm=banner, radius=30, speed=(40, 120), rise=140,
                     life=(600, 1000)),
        ash_flakes("origin", 10, h="custom", offsetCm=banner, radius=30, speed=(40, 120), up=(60, 160),
                   gravity=200, life=(900, 1400), size=(7, 11), color=ASH_MID),
    ], shake=[220, 0.45], note="Valdras' Roar: a dust shock at his feet, a pale shout, and breach fire flaring up the "
                              "standard (its P3 burning is monster.valdras.standard_fire). The goblin chief's Roar "
                              "(later chapters) reuses the notify name")
    # Valdras' Oath-Brand slam (Cast01 FX_Brand 900 ms: the sword driven ~32 cm into the ground ~1.1 m ahead)
    brand = [84.0, 9.0, 0.0]              # the oath-sword's tip at FX_Brand (measured: 29 cm into the ground)
    rec["notify.FX_Brand"] = R("breach", [
        lay(emitter="flash", at="origin", height="custom", offsetCm=[brand[0], brand[1], 40.0], sizeCm=40,
            lifeMs=170, color=VIOLET_HOT),
        lay(emitter="shock", blend="translucent", at="origin", height=2, offsetCm=brand, sizeCm=50, grow=7.0,
            lifeMs=420, alpha=[0.75, 0], alphaPow=1.1, color=VIOLET_DARK),
        ripple_ground("origin", 40, 210, 480, VIOLET, alpha=0.95, body="#2A0A36", body_alpha=0.5, pow=3,
                      offsetCm=brand, variant=1),
        lay(sprite="CrackGlow", orient="ground", at="origin", height=2, offsetCm=brand, sizeCm=330, grow=1.04,
            lifeMs=1400, alpha=[1.0, 0], fadeIn=0.02, alphaPow=1.5, rotationDeg=[0, 360], color=VIOLET),
        lay(sprite="Crack", orient="ground", blend="translucent", at="origin", height=1, offsetCm=brand,
            sizeCm=340, grow=1.04, lifeMs=2400, alpha=[0.85, 0], fadeIn=0.02, alphaPow=2.4,
            rotationDeg=[0, 360], color="#0E0810"),
        breach_flames("origin", 12, radius=70, on_ring=True, rise=(180, 320), life=(400, 620), size=(66, 96),
                      offsetCm=brand),
        FR.debris("origin", 8, offsetCm=brand, color="#4A4440", speed=(80, 180)),
        ash_flakes("origin", 14, h=20, radius=40, offsetCm=brand, speed=(80, 220), up=(160, 360), gravity=700,
                   life=(700, 1100), color=ASH_LIGHT),
        soot("origin", 5, h=40, radius=50, offsetCm=brand, color="#221828", alpha=0.7, life=(800, 1200),
             size=(50, 70)),
    ], shake=[260, 0.8], note="the Oath-Brand slam: the ground splits in violet light round the planted blade, breach "
                             "fire erupts on a ring; the 4 s burning brand is hazard.breach_fire")
    # the deserter drummer's beats (Cast01, 4 beats): a puff of dust off the drum head — no glow, the Living carry no
    # light (drum on the belly: ~30 cm ahead, ~95 cm up)
    drum = [36.0, 0.0, 104.0]            # just over the drum skin (inside the drum the body would hide it)
    rec["notify.FX_DrumBeat_1"] = R("ash", [
        lay(sprite="Dust", orient="billboard", blend="translucent", at="origin", height="custom", offsetCm=drum,
            count=3, spawnRadiusCm=10, speedCmS=[40, 80], dir="up", coneDeg=70, lifeMs=[420, 600],
            sizeCm=[38, 54], grow=1.5, alpha=[0.7, 0], fadeIn=0.08, color="#A89E8C"),
        ripple_air("origin", 30, 90, 280, "#E4DED0", alpha=0.45, h="custom", offset=drum),
    ], note="a drum beat: dust off the drum head and a faint pale beat ripple; no light")
    # the burial detail's spade (Idle_Order_Dig FX_Dig): earth tossed
    rec["notify.FX_Dig"] = R("ash", [
        # the shovel's blade bites at (78, 10) cm in the actor frame (blade_tip at FX_Dig 760 ms, --measure)
        lay(sprite="Rock", randomVariants=2, orient="billboard", blend="translucent", at="origin", height=8,
            offsetCm=[78.0, 10.0, 0.0], count=7, dir="back", coneDeg=55, speedCmS=[90, 170], upCmS=[190, 280],
            gravityCmS2=1100, lifeMs=[380, 540], sizeCm=[12, 18], spinDegS=[-400, 400], alpha=[1, 1],
            color="#8A7F70"),
        ash_dust("origin", 5, offsetCm=[78.0, 10.0, 0.0], radius=14, color="#8A8070", alpha=0.62,
                 size=(40, 58), life=(450, 700), rise=12),
    ], note="the burial detail digging a grave that is no longer there: clods and dust")
    # the dead standard-bearer's banner snapping (Idle_Order FX_BannerSnap): ash shaken off the cloth
    rec["notify.FX_BannerSnap"] = R("ash", [
        ash_flakes("origin", 11, h="custom", offsetCm=[5.0, 28.0, 262.0], radius=30, speed=(30, 80),
                   up=(10, 50), gravity=120, life=(1100, 1600), size=(9, 13), color=ASH_LIGHT, dir="flat"),
    ], note="the banner snapping in a wind that isn't there: ash shaken off the cloth")
    return rec


# ── deaths (a runtime change: PlayFirst({death.<defId>, death.<family>, death.burst}) on a monster kill) ─────────
def deaths() -> dict:
    rec = {}
    # the Unburied (and the Corpseherd): the breach leaves the body — a violet breath up from the head, the eyes'
    # pinpoints going out, ash puffing off as the body folds (the husk crumbles later: corpse.crumble)
    rec["death.unburied"] = R("breach", [
        lay(sprite="Wisp", orient="billboard", at="target", height="head", offsetCm=[0, 0, 26], dir="up",
            coneDeg=0, upCmS=110, lifeMs=720, sizeCm=46, grow=1.25, alpha=[0.9, 0], fadeIn=0.12, alphaPow=1.2,
            color=VIOLET, intensity=1.2),
        lay(sprite="Wisp", orient="billboard", blend="translucent", at="target", height="head",
            offsetCm=[0, 0, 22], dir="up", coneDeg=0, upCmS=100, lifeMs=720, sizeCm=50, grow=1.25,
            alpha=[0.6, 0], fadeIn=0.12, alphaPow=1.2, color=VIOLET_DARK),
        lay(emitter="flash", at="target", height="head", offsetCm=[0, 0, 22], sizeCm=14, lifeMs=130,
            color=VIOLET_HOT),
        violet_motes("target", 9, h="head", radius=18, speed=(30, 90), rise=120, life=(500, 900)),
        # the breach letting go: a fine ripple (a thick neon ring under every dying Unburied read as a target ring)
        ripple_ground("target", 25, 125, 420, VIOLET, alpha=0.75, body=VIOLET_DEEP, body_alpha=0.3, pow=2.5,
                      variant=1),
        ash_dust("target", 5, radius=34, color="#5A564E", alpha=0.6, size=(40, 60), life=(600, 900)),
        ash_flakes("target", 12, h=110, radius=26, speed=(40, 140), up=(40, 160), gravity=300,
                   life=(700, 1200), color=ASH_LIGHT),
    ], note="an Unburied falls: a violet breath leaves through the head, the pinpoints go out, ash puffs off the "
            "folding body; no blood")
    # breach-blood: the shell splits — tar flung, violet sparks; the puddle is the runtime's (fx.puddle)
    rec["death.breach"] = R("breach", [
        lay(sprite="Drop", orient="billboard", blend="translucent", at="target", height=24, count=11,
            spawnRadiusCm=12, dir="random", speedCmS=[80, 200], upCmS=[120, 260], gravityCmS2=900,
            lifeMs=[380, 560], sizeCm=[10, 15], aspect=0.67, grow=0.8, alpha=[1, 0.2], alphaPow=2, color=TAR,
            altEvery=4, altColor="#5A1670"),
        violet_motes("target", 12, h=24, radius=14, speed=(80, 220), rise=40, life=(300, 600), size=(6, 9)),
        violet_glow("target", 110, 200, alpha=0.5, h=24),
        ripple_ground("target", 18, 105, 360, VIOLET, alpha=0.85, body="#1E0812", body_alpha=0.45, pow=2.5,
                      variant=1),
        ash_flakes("target", 5, h=30, radius=14, speed=(40, 100), up=(60, 140), gravity=380, life=(500, 800),
                   size=(5, 8), color=ASH_MID),
    ], note="breach-blood bursts: tar flung, violet sparks; then the tar puddle (runtime fx.puddle) dries in 3 s")
    # the Living: no glow — a fall in the mud, a little dust
    rec["death.living"] = R("ash", [
        ash_dust("target", 6, radius=30, color="#7A7266", alpha=0.62, size=(46, 66), life=(650, 950),
                 speed=(40, 90), rise=14),
        # the body hits the mud (~450 ms into the fall clips): a low, wide slap of dirt
        ash_dust("target", 8, radius=54, color="#6E675B", alpha=0.6, size=(52, 76), life=(700, 1000),
                 speed=(50, 110), rise=6, delay=430),
        lay(sprite="Rock", randomVariants=2, orient="billboard", blend="translucent", at="target", height=10,
            count=5, dir="random", speedCmS=[70, 140], upCmS=[110, 190], gravityCmS2=1000, lifeMs=[340, 480],
            sizeCm=[9, 13], spinDegS=[-300, 300], alpha=[1, 1], color="#8A7F70", delayMs=430),
    ], note="a Living enemy falls: dust off the blow, then a slap of mud as the body lands; no light, no blood "
            "(ash mode)")
    rec["death.gravegnawers"] = R("ash", [
        ash_dust("target", 5, radius=24, color="#7A7266", alpha=0.6, size=(38, 54), life=(550, 800),
                 speed=(40, 80), rise=14),
        ash_dust("target", 6, radius=40, color="#6E675B", alpha=0.55, size=(44, 62), life=(600, 900),
                 speed=(40, 90), rise=6, delay=300),
        ash_flakes("target", 9, h=40, radius=16, speed=(50, 120), up=(50, 140), gravity=380, life=(550, 850),
                   size=(6, 9), color=ASH_LIGHT),
    ], note="a Gravegnawer falls: dust and ash flecks off the hide, a puff as it drops (beasts: no violet)")
    # the cinderbloat (death.<defId>): an intentionally quiet kill. Its death is the fuse (notify.FX_Fuse @0 under the
    # crimson r2.2 ring) and the burst (notify.FX_Burst @800); the family's death.breach (tar flung, violet sparks, a
    # violet ripple) at the kill read as burst → 800 ms fuse → second burst and undercut the fuse telegraph
    # (monsters.md §1.3: fuse >= 800 ms with a visible ring). So: a dull crack in the crust and an ash puff, no tar, no
    # violet, no ring
    rec["death.cinderbloat"] = R("ash", [
        # off the crust top (the body is ~0.6 m tall and ~0.8 m wide: anything spawned inside it never draws)
        lay(sprite="Rock", randomVariants=2, orient="billboard", blend="translucent", at="target", height="head",
            offsetCm=[0, 0, 10], count=5, spawnRadiusCm=18, dir="random", speedCmS=[40, 90], upCmS=[60, 130],
            gravityCmS2=900, lifeMs=[280, 400], sizeCm=[8, 12], spinDegS=[-300, 300], alpha=[1, 1],
            color="#5A3A2E"),
        ash_dust("target", 5, h=18, radius=46, color="#8A8480", alpha=0.55, size=(36, 52), life=(500, 760),
                 speed=(20, 50), rise=20),
        ash_flakes("target", 7, h="head", offsetCm=[0, 0, 12], radius=18, speed=(30, 80), up=(40, 110),
                   gravity=320, life=(500, 800), size=(7, 10), color=ASH_MID),
    ], note="a cinderbloat is killed (death.<defId>): a dull crust crack and an ash puff only — the fuse "
            "(notify.FX_Fuse) and the burst (notify.FX_Burst @800) are its death; no tar, no violet")
    # Lord-Marshal Valdras: the fire seized inside him gutters out of the seams and the standard (he kneels at 1100 ms,
    # the standard droops by 1300-1450 ms of the Death clip; cs_rf_marshal_falls follows).
    # Anchored to HIM, not the blow: death recipes play with Context.Blow = the victim's facing (INTEGRATION
    # deathFamily), so the offsets are his actor frame (forward, right, up) whoever killed him from wherever, and the
    # standard plume follows the measured droop path in three delayed layers (fx_preview_dark --measure on the
    # 21:16 export, VFX r5): banner / standard_top
    #   0-900 ms upright   (-47..-59, 0..-4, 179..194) / (-26..-48, 0..-3, 238..254)
    #   900-1250 falling   (-42 → -76, 0 → -16, 179 → 137) / (-29 → -84, 0 → -37, 238 → 191)
    #   1250+ drooped      (-76, -16, 137) / (-84, -37, 191)
    # (the old fixed [40, 0, 215] in the blow frame put the plume 50-80 cm above and beside the drooped pole, and in
    # front of him on a kill from behind)
    up_, fall_, droop_ = [-44.0, -1.0, 214.0], [-60.0, -10.0, 178.0], [-80.0, -26.0, 164.0]

    def plume(off, delay, dur, rate, rise, life, size, alpha, jitter):
        return lay(sprite="Wisp", orient="billboard", at="target", height="custom", offsetCm=off, dir="up",
                   coneDeg=10, upCmS=rise, rate=rate, durationMs=dur, delayMs=delay, spawnRadiusCm=10,
                   spawnHeightJitterCm=jitter, lifeMs=list(life), sizeCm=list(size), grow=1.25,
                   alpha=[alpha, 0], fadeIn=0.15, color=VIOLET, intensity=1.2)
    rec["death.valdras"] = R("breach", [
        plume(up_, 0, 860, 12, 60, (420, 620), (46, 66), 0.85, 60),
        plume(fall_, 860, 400, 10, 50, (360, 520), (42, 58), 0.8, 56),
        plume(droop_, 1260, 460, 8, 36, (400, 600), (36, 52), 0.72, 50),
        # the seams: pinpoints off the plate round the posed chest socket (follows the kneel; no offset)
        violet_motes("target", 0, h="chest", radius=40, rate=18, durationMs=1400, speed=(20, 60), rise=110,
                     life=(800, 1300), spawnHeightJitterCm=110),
        # the breach lets go of him: two fine ripples (a thick neon ring at 3.6 m read as a UI circle)
        ripple_ground("target", 60, 380, 1000, VIOLET, alpha=0.8, body="#2A0A36", body_alpha=0.4, pow=2.2,
                      variant=1),
        ripple_ground("target", 40, 240, 900, VIOLET, alpha=0.6, body=None, pow=2.0, delay=260, variant=1),
        ash_dust("target", 10, radius=80, color="#77705F", alpha=0.62, size=(60, 90), life=(900, 1300),
                 speed=(40, 100), rise=8, delay=1050),
        # ash shaken off the cloth, upright then drooped
        ash_flakes("target", 0, h="custom", offsetCm=[-46.0, -1.0, 192.0], radius=24, rate=11, durationMs=880,
                   speed=(20, 70), up=(20, 80), gravity=140, life=(1100, 1600), color=ASH_MID),
        ash_flakes("target", 0, h="custom", offsetCm=[-74.0, -20.0, 152.0], radius=24, rate=9, durationMs=780,
                   delay=900, speed=(20, 60), up=(10, 60), gravity=140, life=(1000, 1500), color=ASH_MID),
    ], note="Valdras falls (death.<defId>, actor frame: Blow = his facing): violet guttering out of the standard "
            "along its droop (upright → falling → drooped) and out of the seams for ~1.6 s, two fine ripples, dust "
            "as he kneels (Kneel 1100 ms)")
    return rec


# ── raises, blocks, monster loops, Valdras (W2) ─────────────────────────────────────────────────────────────
def monsters() -> dict:
    rec = {}
    # the raise marker (Call the Flock / Muster / the Unburied trait): 900 ms under the corpse, contracting
    sig = round(120 / SIGIL_R, 1)
    rec["monster.raise.mark"] = R("breach", [
        lay(sprite="Puff", orient="ground", blend="translucent", at="point", height=1.5, sizeCm=250, grow=0.8,
            sizePow=1, lifeMs=940, fadeIn=0.2, alpha=[0.62, 0.3], alphaPow=2, color="#120614"),
        lay(sprite="RaiseSigil", orient="ground", blend="translucent", at="point", height=2, sizeCm=sig,
            grow=0.62, sizePow=1.4, lifeMs=920, fadeIn=0.18, alpha=[0.6, 0.35], alphaPow=3, spinDegS=-40,
            rotationDeg=[0, 360], color=VIOLET_DEEP),
        lay(sprite="RaiseSigil", orient="ground", at="point", height=3, sizeCm=sig, grow=0.62, sizePow=1.4,
            lifeMs=920, fadeIn=0.18, alpha=[0.95, 0.55], alphaPow=3, spinDegS=-40, rotationDeg=[0, 360],
            color=VIOLET, intensity=1.25),
        lay(emitter="gather", sprite="Ember", at="point", height=6, count=14,
            orbit={"r0Cm": [90, 130], "r1Cm": 4, "degS": [-160, -100]}, upCmS=[10, 40], lifeMs=[700, 900],
            sizeCm=[6, 9], alpha=[0.3, 1], color=VIOLET_PIN, intensity=1.4),
        lay(emitter="glow", orient="ground", at="point", height=4, sizeCm=110, grow=0.7, lifeMs=920,
            fadeIn=0.0, alpha=[0.15, 0.55], alphaPow=1.5, color=VIOLET),
    ], note="the raise marker (W2): a breach-violet ring of grasping fingers closing on the corpse over 900 ms, "
            "violet motes drawn in; then monster.raise as it stands")
    rec["monster.raise"] = R("breach", [
        ash_flakes("target", 18, h=10, radius=36, speed=(40, 120), up=(200, 360), gravity=640,
                   life=(700, 1100), color=ASH_LIGHT),
        ash_dust("target", 7, radius=40, color="#77705F", alpha=0.62, size=(46, 66), life=(700, 1000),
                 speed=(50, 110), rise=10),
        ripple_ground("target", 20, 170, 480, VIOLET, alpha=0.9, body=VIOLET_DEEP, body_alpha=0.4, pow=2.5,
                      variant=1),
        lay(emitter="flash", at="target", height="head", offsetCm=[0, 0, 6], sizeCm=18, lifeMs=160,
            color=VIOLET_HOT, delayMs=260),
        violet_glow("target", 70, 300, alpha=0.6, h="head", delay=260, grow=1.2),
        violet_motes("target", 8, h=10, radius=30, speed=(20, 60), rise=200, life=(500, 800)),
    ], note="EvMonsterRaised (W2): ash bursts off the ground as the body stands (Rise), the eyes ignite violet")
    # the shield wall's frontal block (EvBlock, W2): origin = the attacker, target = the blocker; the strike lands
    # on the shield face 40 cm in front of the blocker's chest (back along the blow)
    face = [-40.0, 0.0, 6.0]
    rec["monster.block"] = R("ash", [
        # a short clang arc on the shield rim (W = size x aspect: a 148 cm crescent read as a pale half-moon)
        lay(sprite="Slash", variant=3, orient="billboard", at="target", height="chest", offsetCm=face,
            sizeCm=32, aspect=2.0, grow=1.2, sizePow=2, lifeMs=100, rotationDeg=90, alignToBlow=True,
            alpha=[0.85, 0], alphaPow=1.8, color="#F4EEE0", intensity=1.3),
        lay(emitter="flash", at="target", height="chest", offsetCm=face, sizeCm=22, lifeMs=70, color="white"),
        lay(sprite="Impact", variant=1, orient="billboard", at="target", height="chest", offsetCm=face,
            sizeCm=round(46 / FR.impact_scale("heavy"), 3), grow=1.4, sizePow=3, lifeMs=120, alpha=[1, 0],
            alphaPow=1.6, rotationDeg=[0, 360], color="#E8E4DC", intensity=1.25),
        lay(emitter="sparks", at="target", height="chest", offsetCm=face, count=12, dir="back", coneDeg=80,
            speedCmS=[260, 520], gravityCmS2=760, drag=1.5, lifeMs=[160, 300], sizeCm=[8, 12], color=STEEL_SPARK,
            altEvery=2, altColor="white", alpha=[1, 0]),
        lay(sprite="Ring", orient="billboard", blend="translucent", at="target", height="chest", offsetCm=face,
            sizeCm=46, grow=2.2, sizePow=2, lifeMs=220, alpha=[0.55, 0], color="#2E2B28"),
        lay(sprite="Ring", orient="billboard", at="target", height="chest", offsetCm=face, sizeCm=46, grow=2.2,
            sizePow=2, lifeMs=220, alpha=[0.6, 0], color="#CFCAC0", intensity=1.1),
    ], note="EvBlock (W2): steel on the tower shield — a bone-white arc across the blow, pale sparks thrown back at "
            "the attacker, a dull clang ring (no orange)")
    # per-monster loop: the cinderbloat's crust sheds ash (manifest fx.ashFlakes: 3 /s, rise 22 cm/s, 1.5 s, fx_vent)
    rec["monster.ash_flakes"] = R("ash", [
        # from the crust top (fx_head = body_top; spawning at a fixed 62 cm put most flakes inside the body)
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="origin",
            height="head", offsetCm=[0, 0, 18], rate=6, durationMs=-1, attach=True, spawnRadiusCm=26,
            dir="up", coneDeg=50, speedCmS=[14, 30], gravityCmS2=-10, lifeMs=[1500, 1900], sizeCm=[11, 15],
            spinDegS=[-120, 120], rotationDeg=[0, 360], alpha=[0.95, 0], fadeIn=0.1, alphaPow=1.5,
            color="#C8C2B6"),
        lay(emitter="smoke", sprite="Smoke", randomVariants=2, at="origin", height="head", offsetCm=[0, 0, 8],
            rate=0.7, durationMs=-1, attach=True, spawnRadiusCm=14, speedCmS=[6, 14], gravityCmS2=-24,
            lifeMs=[1400, 1800], sizeCm=[30, 42], grow=1.6, alpha=[0.32, 0], color="#4A4640"),
        lay(emitter="motes", sprite="Ember", at="origin", height="head", offsetCm=[0, 0, 4], rate=0.8,
            durationMs=-1, attach=True, spawnRadiusCm=14, speedCmS=[6, 14], gravityCmS2=-30, lifeMs=[700, 1000],
            sizeCm=[5, 7], color=VIOLET_PIN, intensity=1.3),
    ], note="a monster loop (cinderbloat fx.ashFlakes): ash drifting off the crust from its vent, a rare violet spit")
    # the dead standard-bearer's aura (W2, monsters.md §2.2: Unburied within 5 tiles deal +15 %; "a crimson ground
    # halo, visible"). DR5 keeps crimson #D11F2E (+ the bone fill) for damage telegraphs only — the decision logged
    # in DECISIONS_DARK['DR5-aura']: the halo is the banner's own cloth-dark crimson (#4A0E12 / #3A0A0C, the
    # crusade's cloth, not the telegraph hue), a soft stain with NO rim line and no fill sweep (it never grows from
    # the centre), breathing slowly, ash shaken off the banner over it
    rec["monster.dead_standard_bearer.aura"] = R("crimson", [
        # (at .24 #4A0E12 the stain vanished on the mud in the first preview: a cloth crimson a step lighter, denser)
        lay(sprite="Puff", orient="ground", blend="translucent", at="origin", height=1.5, rate=0.45, attach=True,
            sizeCm=1000, grow=1.04, sizePow=1, lifeMs=2600, fadeIn=0.45, alpha=[0.42, 0], alphaPow=1.2,
            color="#6A1418"),
        lay(sprite="Puff", orient="ground", blend="translucent", at="origin", height=2, rate=0.6, attach=True,
            sizeCm=320, grow=1.06, sizePow=1, lifeMs=2000, fadeIn=0.4, alpha=[0.5, 0], alphaPow=1.2,
            color="#4A0E12"),
        # (no drifting red haze: its loose puffs read as blood pools on the mud — ash, not blood-splatter)
        ash_flakes("origin", 0, h="custom", offsetCm=[6.0, 28.0, 250.0], radius=30, rate=6, durationMs=-1,
                   attach=True, speed=(10, 30), up=(-10, 10), gravity=60, life=(1800, 2600), size=(9, 13),
                   color=ASH_LIGHT, dir="flat"),
    ], note="the dead standard-bearer's +15 % aura (W2, r 5 tiles, loop on the bearer while it lives): a soft "
            "cloth-dark crimson stain breathing on the ground, ash drifting off the banner over it — no rim line, "
            "no fill sweep, never the telegraph crimson (DR5)")
    # Valdras P3 (No Retreat): the standard on his back burns with breach fire (EvMonsterPhase index 2, loop on the
    # actor until death)
    banner = [-44.0, 0.0, 214.0]
    rec["monster.valdras.standard_fire"] = R("breach", [
        # clinging to the cloth (slower rise, more tongues) rather than a crown over the pole top
        breach_flames("origin", 0, radius=20, rate=14, attach=True, rise=(45, 100), life=(440, 660),
                      size=(56, 84), h="custom", offsetCm=banner, spawnHeightJitterCm=80),
        breach_flames("origin", 0, radius=10, rate=3, attach=True, rise=(120, 180), life=(380, 520),
                      size=(40, 56), h="custom", offsetCm=[-40.0, 0.0, 278.0], variant=FB["wisp"]),
        lay(emitter="glow", at="origin", height="custom", offsetCm=banner, rate=2.5, durationMs=-1, attach=True,
            sizeCm=150, grow=1.15, lifeMs=800, fadeIn=0.5, alpha=[0.38, 0], alphaPow=1, color=VIOLET),
        violet_motes("origin", 0, h="custom", offsetCm=banner, radius=26, rate=6, durationMs=-1, attach=True,
                     speed=(20, 60), rise=140, life=(700, 1100), spawnHeightJitterCm=80),
        soot("origin", 0, h="custom", offsetCm=[-44.0, 0.0, 290.0], radius=12, rate=2.4, durationMs=-1,
             attach=True, alpha=0.5, life=(900, 1300), size=(36, 50), color="#22182A", rise=70),
    ], note="Valdras P3 No Retreat (W2): breach fire burning up the standard, violet motes and soot streaming off")
    # Feed: a Raised that dies near the dais is drawn into the standard (origin = the corpse point, target = Valdras)
    rec["monster.valdras.feed"] = R("breach", [
        lay(emitter="streak", at="path", height="chest", count=14, dir="blow", coneDeg=6, speedCmS=420,
            lifeMs=[240, 360], sizeCm=48, aspect=0.2, color=VIOLET, alpha=[0.85, 0], intensity=1.3),
        lay(emitter="gather", sprite="Ember", at="target", height="custom", count=12,
            orbit={"r0Cm": [70, 100], "r1Cm": 8, "degS": [120, 200]}, lifeMs=[420, 560], sizeCm=[6, 9],
            alpha=[0.3, 1], color=VIOLET_PIN, offsetCm=[0.0, 0.0, 220.0], intensity=1.4),
        ash_dust("origin", 5, radius=24, color="#77705F", alpha=0.6, rise=10),
        ash_flakes("origin", 10, h=40, radius=22, speed=(30, 90), up=(80, 160), gravity=200,
                   life=(600, 900), color=ASH_MID),
    ], note="Feed (W2): the corpse's ash streams into the standard as violet streaks; Valdras heals (the gather "
            "sits over his head: Blow runs corpse -> Valdras, not along his facing, so a back offset would drift)")
    return rec


# ── telegraph contact beats and hazards (W2: U2's ground-decal actor plays these) ───────────────────────────
def telegraphs() -> dict:
    rec = {}
    # the decal's own contact flash is the material's (TELEGRAPH.contact); this is the punctuation in the world:
    # a crimson rim snap and bone dust kicked up at the rim, scaled by the context radius (unit radius)
    rec["telegraph.contact"] = R("hazard", [
        lay(sprite="Ring", orient="ground", blend="translucent", at="point", height=3, unit="radius",
            sizeCm=round(1.0 / RING_R, 3), grow=1.12, sizePow=2, lifeMs=220, alpha=[0.7, 0], alphaPow=1.3,
            color="#5A0A10"),
        lay(sprite="Ring", orient="ground", at="point", height=4, unit="radius", sizeCm=round(1.0 / RING_R, 3),
            grow=1.12, sizePow=2, lifeMs=220, alpha=[0.95, 0], alphaPow=1.3, color="#FF3A48", intensity=1.25),
        lay(emitter="shock", blend="translucent", at="point", height=2, unit="radius", sizeCm=0.4, grow=5.0,
            lifeMs=200, alpha=[0.38, 0], alphaPow=1.1, color=BONE),
        lay(sprite="Dust", orient="billboard", blend="translucent", at="point", height=8, unit="radius",
            countByRadius=True, count=5, spawnRadiusCm=0.95, onRing=True, dir="out", speedCmS=[40, 90],
            drag=2.4, gravityCmS2=-10, lifeMs=[500, 750], sizeCm=[0.2, 0.28], grow=1.4, alpha=[0.5, 0],
            fadeIn=0.08, color="#5A544A"),
    ], note="the telegraph's contact beat (W2, U2): a crimson rim snap and dust at the rim; scaled by the radius")
    rec["telegraph.contact.line"] = R("hazard", [
        lay(sprite="Dust", orient="billboard", blend="translucent", at="path", height=8, count=9, dir="flat",
            speedCmS=[30, 70], drag=2.4, lifeMs=[480, 700], sizeCm=[40, 56], grow=1.4, alpha=[0.55, 0],
            fadeIn=0.08, color="#5A544A"),
        lay(emitter="sparks", at="path", height=12, count=10, dir="up", coneDeg=50, speedCmS=[140, 260],
            gravityCmS2=700, lifeMs=[160, 260], sizeCm=[8, 11], color="#FF3A48", altEvery=3, altColor=BONE),
    ], note="a line telegraph's contact (W2): dust and crimson spits along origin → target")
    rec["telegraph.contact.cone"] = R("hazard", [
        lay(sprite="Dust", orient="billboard", blend="translucent", at="origin", height=8, unit="radius",
            count=10, spawnRadiusCm=0.55, dir="blow", coneDeg=110, speedCmS=[80, 200], drag=2.2,
            lifeMs=[480, 720], sizeCm=[0.17, 0.24], grow=1.4, alpha=[0.55, 0], fadeIn=0.08, color="#5A544A"),
        lay(emitter="sparks", at="origin", height=14, count=12, dir="blow", coneDeg=110, speedCmS=[220, 420],
            gravityCmS2=500, lifeMs=[180, 300], sizeCm=[8, 11], color="#FF3A48", altEvery=3, altColor=BONE),
    ], note="a cone telegraph's contact (W2): a fan of dust and crimson spits along the blow")
    # hazards (EvHazard loops, W2): what burns inside the hazard decal (the decal's rim is the hazard style)
    rec["hazard.breach_fire"] = R("breach", [
        held_ground("Scorch", "point", 1.9, "#120810", 0.7, 800, h=1, unit="radius"),
        held_ground("CrackGlow", "point", 1.7, VIOLET, 0.8, 700, h=2, blend=None, unit="radius", intensity=1.1),
        # a fire, not scattered drops: denser and taller tongues (at r 1.8 m the old 43-68 cm read as violet beads)
        breach_flames("point", 0, radius=0.75, rate=24, unit="radius", rise=(40, 90), life=(520, 760),
                      size=(0.3, 0.5), h=0),
        violet_motes("point", 0, h=10, radius=0.8, unit="radius", rate=6, durationMs=-1, speed=(6, 16),
                     rise=110, life=(700, 1100), size=(0.03, 0.045)),
        lay(emitter="glow", orient="ground", at="point", height=3, unit="radius", rate=1.5, durationMs=-1,
            sizeCm=2.2, grow=1.05, lifeMs=1200, fadeIn=0.5, alpha=[0.28, 0], alphaPow=1, color=VIOLET),
    ], note="a burning brand / breach-fire patch (W2 EvHazard, unit radius): violet cracks in a scorch, breach "
            "fire licking up")
    rec["hazard.breach_seep"] = R("breach", [
        # one fissure along the line: the radial crack cells stretched along it (aligned to the path, aspect .32)
        # and overlapping — at random yaw they read as a row of violet starbursts
        # 18-20 overlapping cells, not 12 (VFX r5: 12 still read as a beaded row of starbursts)
        lay(sprite="Crack", orient="ground", blend="translucent", at="path", height=1, count=18, sizeCm=[150, 190],
            aspect=0.32, grow=1.04, lifeMs=1300, alpha=[0.85, 0], fadeIn=0.05, alphaPow=2.4,
            rotationDeg=[-14, 14], alignToBlow=True, color="#0E0810"),
        lay(sprite="CrackGlow", orient="ground", at="path", height=2, count=20, sizeCm=[140, 180], aspect=0.24,
            grow=1.04, lifeMs=1200, alpha=[0.9, 0], fadeIn=0.08, alphaPow=1.6, rotationDeg=[-8, 8],
            alignToBlow=True, color=VIOLET),
        breach_flames("path", 12, radius=10, rise=(80, 150), life=(400, 600), size=(40, 62), cells=4, delay=60),
        violet_motes("path", 12, h=10, radius=8, speed=(10, 30), rise=120, life=(500, 800)),
    ], note="渗裂 Breach-seep (W2): a crack line (origin → target, 8 × 1 tiles) splitting in violet, breach fire "
            "along it for the two ticks; the crimson line stays on through the ticks (U2: the windup's decal hands "
            "over to the hazard style at contact until the zone ends — TELEGRAPH styles.hazard)")
    rec["hazard.banner_ring"] = R("breach", [
        # a wall of fire, not a dotted line: ~60 tongues alive round the ring (at r 4.2 m: 0.7-1 m tall, one every
        # ~45 cm; the rate does not scale with the radius, the size does)
        # VFX r5: at r 3.2 m the .16-.24 radius tongues (51-77 cm) at 110 /s still read as dotted — bigger and denser
        # (r 3.2 m: 70-100 cm, ~95 alive; r 4.2 m: 92-130 cm) plus a low lick layer closing the gaps at the base
        breach_flames("point", 0, radius=1.0, unit="radius", on_ring=True, rate=240, rise=(60, 130),
                      life=(440, 660), size=(0.23, 0.33), h=0),
        breach_flames("point", 0, radius=1.0, unit="radius", on_ring=True, rate=90, rise=(20, 50),
                      life=(380, 560), size=(0.12, 0.17), h=0, variant=FB["lick"]),
        lay(sprite="Ring", orient="ground", blend="translucent", at="point", height=2, unit="radius",
            sizeCm=round(1.0 / RING_R, 3), grow=1.0, rate=1.2, durationMs=-1, lifeMs=1200, fadeIn=0.4,
            alpha=[0.55, 0], alphaPow=1, color="#1A0820"),
        # the violet line at half strength: the boundary's crimson rim (arena-style decal) must lead there
        lay(sprite="Ring", orient="ground", at="point", height=3, unit="radius", sizeCm=round(1.0 / RING_R, 3),
            grow=1.0, rate=1.2, durationMs=-1, lifeMs=1200, fadeIn=0.4, alpha=[0.3, 0], alphaPow=1, color=VIOLET,
            intensity=1.1),
        # a low smoke band joining the tongues into one wall
        soot("point", 0, h=30, radius=1.0, unit="radius", onRing=True, rate=26, durationMs=-1, alpha=0.55,
             life=(800, 1200), size=(0.1, 0.14), color="#22182A", rise=50),
    ], note="P3's closing banner ring (W2 EvArena): breach fire round the ring's edge (unit radius = the arena's "
            "current radius; re-play as it shrinks); the damage side (outside: 4 % max life / s) is U2's arena-style "
            "crimson decal (TELEGRAPH styles.arena), not this recipe")
    return rec


# ── corpses and ash (W2: U3) ────────────────────────────────────────────────────────────────────────────────
def corpses() -> dict:
    rec = {}
    rec["corpse.crumble"] = R("ash", [
        ash_dust("point", 6, radius=34, color="#7A7368", alpha=0.6, size=(46, 68), life=(800, 1200),
                 speed=(20, 50), rise=6),
        # flakes coming off the body's top (a lying body is ~40 cm high), big enough to read at the game camera
        ash_flakes("point", 0, h=30, radius=40, rate=30, durationMs=700, speed=(10, 40), up=(-10, 40),
                   gravity=200, life=(700, 1100), size=(9, 14), color=ASH_LIGHT, spawnHeightJitterCm=30),
        soot("point", 3, h=30, radius=30, color="#4A4640", alpha=0.45, life=(1000, 1400), size=(40, 56),
             rise=30),
    ], note="U3: the body greys and slumps into an ash husk — ash dust spreading low, flakes falling off it")
    rec["corpse.expire"] = R("ash", [
        ash_flakes("point", 0, h=20, radius=40, rate=24, durationMs=650, dir="blow", speed=(50, 120),
                   up=(20, 70), gravity=-20, life=(1200, 1800), size=(6, 10), color=ASH_LIGHT, alpha=0.8),
        ash_dust("point", 3, radius=30, color="#6E6A62", alpha=0.45, size=(44, 60), life=(900, 1300),
                 speed=(30, 60)),
    ], note="U3: the husk's TTL ends — the ash lifts and blows away along world +X")
    rec["corpse.burn"] = R("ash", [
        lay(emitter="flames", sprite="Flame", variant=6, blend="translucent", orient="upright", at="point",
            height=0, spawnRadiusCm=40, rate=16, durationMs=700, upCmS=[60, 120], lifeMs=[380, 560],
            sizeCm=[50, 76], aspect=0.5, grow=0.72, sizePow=2, alpha=[1, 0], fadeIn=0.15, alphaPow=1.2,
            color="event", intensity=1.1),
        lay(emitter="glow", at="point", height=40, rate=4, durationMs=700, sizeCm=120, grow=1.2, lifeMs=500,
            alpha=[0.45, 0], color="event"),
        soot("point", 6, h=50, radius=30, color="#1E1A18", alpha=0.75, life=(900, 1300), size=(44, 64),
             rise=70, delay=200),
        ash_flakes("point", 12, h=40, radius=30, speed=(30, 90), up=(120, 240), gravity=200, life=(900, 1400),
                   color=ASH_MID, delay=300),
        lay(sprite="Scorch", orient="ground", blend="translucent", at="point", height=1, sizeCm=150, grow=1.08,
            lifeMs=3000, alpha=[0.7, 0], fadeIn=0.1, alphaPow=2.5, rotationDeg=[0, 360], color="#14100E"),
    ], note="U3: a fire killing blow / fire over a corpse burns it (unraisable): a flare in the fire's own colour "
            "(event: the hero's fire orange, breach fire violet; grey without one), soot, ash")
    # VFX r5: at 7-10 cm flakes, #77705F dust at .55 and 5-8 cm grit nothing read on the mud — bigger, paler, denser,
    # tossed higher so it clears the husk; the gnawer stands beside the husk (origin) and bites into it (point)
    # the gnawer's jaws reach ~44 cm along its facing at Attack01 Contact (--measure: head (44, -11, 48) @300), i.e.
    # into the husk's surface: spawn a little short of them, above the body, and throw it back toward the gnawer
    # (spawned at the jaws, inside the slumped husk, the grit never drew)
    # Wave 2 (2H): re-timed for the real Consume clip (it used Attack01 as a stand-in, the head at 48 cm): crouched
    # over the husk the head works at (40, 1, 29) and the jaw tip at ~45-52 cm forward, 6-25 cm up (--measure)
    bite = [36.0, 0.0, 0.0]
    rec["corpse.consume"] = R("ash", [
        ash_dust("origin", 0, radius=30, rate=5, durationMs=2000, offsetCm=bite, color="#A39C8E", alpha=0.7,
                 size=(46, 66), life=(600, 900), speed=(30, 80), rise=14, h=10),
        ash_flakes("origin", 0, h=34, offsetCm=bite, radius=14, rate=18, durationMs=2000, speed=(60, 140),
                   up=(100, 220), gravity=620, life=(500, 800), size=(11, 16), color="#CFCAC0", alpha=1.0,
                   dir="back", coneDeg=150),
        lay(sprite="Rock", randomVariants=2, orient="billboard", blend="translucent", at="origin", height=30,
            offsetCm=bite, rate=6, durationMs=2000, spawnRadiusCm=12, dir="back", coneDeg=140,
            speedCmS=[70, 140], upCmS=[120, 220], gravityCmS2=1000, lifeMs=[320, 460], sizeCm=[9, 13],
            spinDegS=[-400, 400], alpha=[1, 1], color="#B8B2A6"),
    ], note="U3: a gravegnawer tearing at a husk for 2 s (Consume; origin = the gnawer beside it, Blow = gnawer -> "
            "husk): pale ash, flakes and grit tossed up and back at its jaws, in front of the body (spawned inside "
            "the slumped husk they never drew)")
    return rec


# ── Wave 2 (dark remake 2H): the clips the wave added, the frenzy, the Keen, Ashnumb ──────────────────────────
# offsets from fx_preview_dark --measure on the Wave 2 exports (actor frame: forward, right, up cm):
# OathZealot Roar @480 fx_head (-10, -2, 158), Idle @0 fx_head (14, 1, 156); Corpseherd Cast02 @800 fx_head
# (-7, 2, 108), fx_hand_l (19, -44, 120); Gravegnawer Consume @330 fx_head (40, 1, 29), the jaw tip ~(45-52, 0, 6-25)
ZEALOT_SHOUT = [10.0, 0.0, 162.0]     # in front of the hood's mouth as he shouts, chin up
ZEALOT_BREATH = [32.0, 0.0, 150.0]    # in front of the hood at READY (frenzied crouch)
HERD_KEEN = [6.0, 0.0, 112.0]         # in front of the veil as she keens, the hood thrown back
KEEN_SWEEP_MS = 220                   # the Keen's ground ripple reaches r4 this soon after contact (review: <= 250)
ASHNUMB_ASH = "#8A8480"               # the ash on the hero's limbs (ash mid; StatusTint, see ASHNUMB)
ASHNUMB_FLAKE_DARK = "#4C4944"        # dark ash flakes (= AshShade): the ones that read on steel


def wave2() -> dict:
    rec = {}
    # the zealot's frenzy (K4 enterAction Roar; FX_Frenzy @480): the shouted chant — the Living carry no light, so the
    # frenzy is breath and dust: a pale burst of breath out of the hood, dust shoved off his feet as he rears, ash
    # shaken off the whipping robe; the scar's flare is the material (manifest fx.frenzy), not this recipe
    rec["notify.FX_Frenzy"] = R("ash", [
        lay(sprite="Dust", orient="billboard", blend="translucent", at="origin", height="custom",
            offsetCm=ZEALOT_SHOUT, count=5, spawnRadiusCm=6, dir="blow", coneDeg=36, speedCmS=[110, 190],
            upCmS=[20, 60], drag=2.6, lifeMs=[380, 560], sizeCm=[18, 28], grow=1.9, sizePow=1.4, alpha=[0.46, 0],
            fadeIn=0.08, alphaPow=1.4, color="#D4CEC2"),
        ripple_air("origin", 36, 130, 320, "#E8E4DC", alpha=0.3, h="custom", offset=ZEALOT_SHOUT),
        ground_ring("origin", 34, 150, 400, "#B8B2A6", alpha=0.4, body="#3A3631", body_alpha=0.42, pow=2.6),
        ash_dust("origin", 9, radius=42, color="#8A8478", alpha=0.64, size=(46, 68), life=(600, 900),
                 speed=(90, 180), rise=6),
        ash_flakes("origin", 10, h=90, radius=30, speed=(80, 180), up=(40, 120), gravity=300, life=(700, 1100),
                   size=(8, 12), color=ASH_MID, dir="flat"),
    ], shake=[90, 0.2], note="the zealot's frenzy (W2, Roar FX_Frenzy @480): the chant breaks out of the hood as a "
                             "pale breath, dust shoved off his feet, ash shaken off the whipping robe — no light (the "
                             "scar flare is fx.frenzy's material parameter)")
    # the frenzy for the rest of the phase (loop on the zealot, origin + attach, Blow = his facing): panting breath
    # out of the hood and dust kicked at his feet — he is alive and spent, not glowing. Art review r1: the oath-scar
    # at its frenzy emissive is a 1 px block at 1080p (oath_zealot.py --frenzy-read: 3-11 px of it in se / sw), so
    # the breath is the persistent read: a pant every ~0.3 s, each puff living ~0.8 s (two or three always in the
    # air), large and pale enough to hold a ~25 px cloud before the hood on the darkest ground
    rec["monster.oath_zealot.frenzy"] = R("ash", [
        lay(sprite="Dust", orient="billboard", blend="translucent", at="origin", height="custom",
            offsetCm=ZEALOT_BREATH, rate=3.4, durationMs=-1, attach=True, spawnRadiusCm=5, dir="blow",
            coneDeg=34, speedCmS=[30, 60], upCmS=[14, 34], drag=2.2, lifeMs=[700, 950], sizeCm=[24, 34],
            grow=1.7, sizePow=1.3, alpha=[0.62, 0], fadeIn=0.1, alphaPow=1.1, color="#DCD6CA"),
        ash_dust("origin", 0, rate=2.6, durationMs=-1, attach=True, radius=26, color="#8A8478", alpha=0.5,
                 size=(34, 48), life=(560, 780), speed=(30, 70), rise=6),
    ], note="the zealot's frenzy loop (W2: EvMonsterPhase zealot index 1 until death; Blow = his facing): panting "
            "breath out of the hood — a pale cloud always before the hood, the frenzy's persistent read (the scar "
            "is ~1 px at 1080p) — and dust kicked at his feet; no light (the Living carry none)")
    # the Corpseherd's Keen (Cast02 FX_Keen @800; chapter1.md §10 stretch: crimson ring telegraph r4, slow 25 % 3 s):
    # her voice, not her bell — the crimson ring is U2's decal (it fills to its rim at 800); this is the lament
    # breaking out at contact: violet sound rings in the air before the veil (the slow part: the voice), a hairline
    # violet ripple that sweeps the ground to the 4-tile rim in KEEN_SWEEP_MS and flares there (art review: the slow
    # lands on everyone inside r4 at 800, so the ground read must arrive at once — a 700 ms linear ripple reached a
    # hero at the rim ~650 ms after the hit), the ash on the ground blown outward by it
    rec["notify.FX_Keen"] = R("breach", [
        ripple_air("origin", 30, 170, 460, VIOLET, alpha=0.85, h="custom", offset=HERD_KEEN),
        ripple_air("origin", 30, 170, 460, VIOLET_DARK, alpha=0.5, h="custom", offset=HERD_KEEN,
                   blend="translucent"),
        ripple_air("origin", 30, 170, 460, VIOLET, alpha=0.6, h="custom", offset=HERD_KEEN, delay=150),
        ripple_ground("origin", 40, 400, KEEN_SWEEP_MS, VIOLET, alpha=0.85, body=VIOLET_DEEP, body_alpha=0.4,
                      pow=1.0, variant=1),
        ripple_ground("origin", 388, 412, 480, VIOLET, alpha=0.7, body=VIOLET_DEEP, body_alpha=0.32, pow=1.0,
                      delay=KEEN_SWEEP_MS - 40, variant=1),
        ash_dust("origin", 14, radius=90, color="#8A8478", alpha=0.6, size=(44, 64), life=(700, 1000),
                 speed=(220, 360), rise=4),
        ash_flakes("origin", 16, h=10, radius=120, speed=(140, 260), up=(40, 120), gravity=220, life=(700, 1100),
                   size=(7, 11), color=ASH_LIGHT, dir="out"),
        violet_motes("origin", 8, h="custom", offsetCm=HERD_KEEN, radius=12, speed=(40, 100), rise=60,
                     life=(500, 800), size=(5, 7)),
    ], note="the Corpseherd's Keen (W2 stretch, Cast02 FX_Keen @800): the lament breaks out — violet sound rings "
            "before the veil (the voice, slower), a hairline ripple sweeping the ground to r 4 m in 220 ms and "
            "flaring at the rim (everyone inside is slowed at 800), the ash blown outward; the crimson ring is U2's "
            "telegraph decal (filled to its rim at 800), the slow is status.slow")
    # 麻灰 Ashnumb (K6 StatusType::Ashnumb, monsters.md §4: -10 % healing received per stack up to 5, slowed 20 % at
    # 5, 6 s): grey ash creeping up the hero's limbs — the band is the StatusTint contract (ASHNUMB: the runtime raises
    # it per stack), this loop is the ash flaking off the limbs and settling round the feet (neutral: a status loop
    # on the hero; never violet, never a filling ground shape)
    # art review r1: the r0 flakes (12-16 cm, pale only) all but vanished at 1080p — pale ash on the warrior's pale
    # steel has no contrast, the flakes were too small to separate from the dark ground and, spawned on a 30 cm ring,
    # most were inside or behind the body. Now larger, more, slower to fade, spawned just outside the silhouette
    # (ring 40-42 cm), in two tones: pale flakes (read on cloth, leather and the ground) and dark ash (reads on steel
    # and the mage's light trims)
    rec["status.ashnumb"] = R("ash", [
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="target",
            height="chest", rate=12, durationMs=-1, attach=True, spawnRadiusCm=42, onRing=True,
            spawnHeightJitterCm=70, dir="out", speedCmS=[8, 22], gravityCmS2=140, drag=1.4, lifeMs=[900, 1250],
            sizeCm=[20, 28], spinDegS=[-160, 160],
            rotationDeg=[0, 360], alpha=[1.0, 0], fadeIn=0.08, alphaPow=2.2, color=ASH_LIGHT),
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="target",
            height="chest", rate=8, durationMs=-1, attach=True, spawnRadiusCm=40, onRing=True,
            spawnHeightJitterCm=70, dir="out", speedCmS=[8, 22], gravityCmS2=140, drag=1.4, lifeMs=[900, 1250],
            sizeCm=[18, 26], spinDegS=[-160, 160],
            rotationDeg=[0, 360], alpha=[1.0, 0], fadeIn=0.08, alphaPow=2.2, color=ASHNUMB_FLAKE_DARK),
        lay(sprite="Dust", orient="billboard", blend="translucent", at="target", height=12, rate=1.8, durationMs=-1,
            attach=True, spawnRadiusCm=22, dir="out", speedCmS=[8, 20], gravityCmS2=-6, drag=2.0,
            lifeMs=[800, 1100], sizeCm=[34, 46], grow=1.3, alpha=[0.5, 0], fadeIn=0.2, color=ASHNUMB_ASH),
        lay(sprite="Core", orient="billboard", blend="translucent", at="target", height="chest", rate=5,
            durationMs=-1, attach=True, spawnRadiusCm=26, spawnHeightJitterCm=70, gravityCmS2=60, speedCmS=[4, 12],
            lifeMs=[600, 900], sizeCm=[4, 6], alpha=[0.8, 0], color="#6E6A62"),
    ], note="麻灰 Ashnumb (W2, K6): ash flaking off the hero's limbs and settling at the feet; the grey band "
            "creeping up the limbs per stack is the StatusTint contract dark.materials.AshnumbTint (runtime)")
    return rec


# the Ashnumb body band (K6 / monsters.md §4: "grey ash creeping up the hero's limbs, a CPD tint band"): the contract
# the UE status presentation implements next to the existing StatusTint (a multiply tint over the whole body)
ASHNUMB = {
    "kind": "StatusPresentation",
    "name": "AshnumbTint",
    "use": "K6 StatusType::Ashnumb on the hero (stacks 1-5, 6 s, refreshed on hit; the antidote cleanses it): the "
           "hero's toon slots grey from the feet up — a band whose top rises with the stacks — plus the "
           "status.ashnumb loop (ash flaking off, dust at the feet). Never violet, never a ground decal",
    "params": {"AshColor": "#8A8480", "AshShade": "#4C4944", "EdgeColor": "#B8B2A6", "EdgeWidth": 0.04,
               "BandTopBase": 0.12, "BandTopPerStack": 0.10, "BandSoftness": 0.06, "MaxBandTop": 0.62,
               "Strength": 0.85},
    "map": "per vertex the height fraction h = z / heroHeight (object space, the feet at 0); band = 1 - "
           "smoothstep(top - BandSoftness, top, h) with top = min(BandTopBase + stacks x BandTopPerStack, "
           "MaxBandTop) (1 stack: over the boots to mid-shin; 3: the thighs; 5: the hips); toon colour lerps to "
           "AshColor where the toon's key light is in its lit bands, AshShade in its shade band (N.L x .5 + .5 < "
           "T_SHADE), by band x Strength; an EdgeColor line EdgeWidth tall on the band's top edge (top - EdgeWidth .. "
           "top, full strength); the ink hull and held items are untouched. On a renderer without the per-stack "
           "band (mobile low) use the StatusTint path: #8A8480 multiply at .25 + .05 x stacks",
    "preview": "vfx_effects/AshnumbTint/band_stacks*.png (vfx/ashnumb_band.py: this contract as material nodes on "
               "the three heroes at 0 / 1 / 3 / 5 stacks, W1 camera, 1080p, march dirt)",
    "readNote": "art review r1: with the r0 top (stacks x 0.12) one stack was the boots alone, hidden under the "
                "warrior's shield and legs at the W1 camera; the base lifts one stack to mid-shin. Reads clearly on "
                "the mage's robe and the rogue's leather; on the warrior's pale steel the ash reads darker and "
                "warmer (a dull band) and the EdgeColor line marks its top",
    "slowAt5": "at 5 stacks the hero is also slowed 20 %: status.slow is NOT added (the band at its top is the read; "
               "the slow's own frost look is the wrong element)",
    "owner": "A12 W2 (vfx/fx_recipes_dark.py ASHNUMB); built by the UE status presentation (2G)",
}


# ── the echo ghost and the keepers' bell (friendly / neutral) ──────────────────────────────────────────────
def world() -> dict:
    rec = {}
    # the echo lore look (A10's MI_AF_Echo, 3000 ms: 450 fade in, 2100 hold, 450 fade out) — cold ash motes rising
    # off the figure (echo.rim), a faint cold ground ring while it stands; never violet, never ember
    rec["prop.echo"] = R("echo", [
        lay(emitter="glow", at="origin", height="chest", sizeCm=150, grow=1.2, lifeMs=600, fadeIn=0.4,
            alpha=[0.35, 0], color=ECHO_BODY),
        lay(sprite="Ring", orient="ground", blend="translucent", at="origin", height=3, sizeCm=ring_size(70),
            grow=1.06, sizePow=1, lifeMs=3000, fadeIn=0.15, alpha=[0.3, 0], alphaPow=4, color="#5A6274"),
        lay(sprite="Ring", orient="ground", at="origin", height=4, sizeCm=ring_size(70), grow=1.06, sizePow=1,
            lifeMs=3000, fadeIn=0.15, alpha=[0.4, 0], alphaPow=4, color=ECHO_BODY),
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="origin",
            height="chest", rate=9, durationMs=2600, spawnRadiusCm=34, spawnHeightJitterCm=110, dir="up",
            coneDeg=30, speedCmS=[16, 34], gravityCmS2=-10, lifeMs=[1200, 1800], sizeCm=[6, 9], grow=0.8,
            spinDegS=[-60, 60], rotationDeg=[0, 360], alpha=[0.85, 0], fadeIn=0.2, alphaPow=1.6, color=ECHO_RIM),
        lay(sprite="Core", orient="billboard", at="origin", height="chest", rate=7, durationMs=2600,
            spawnRadiusCm=30, spawnHeightJitterCm=110, dir="up", coneDeg=20, speedCmS=[20, 40], gravityCmS2=-8,
            lifeMs=[900, 1400], sizeCm=[5, 7], alpha=[0.7, 0], fadeIn=0.2, color=ECHO_RIM, intensity=1.1),
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="origin",
            height="chest", count=16, spawnRadiusCm=30, spawnHeightJitterCm=110, dir="random", speedCmS=[30, 80],
            upCmS=[20, 60], gravityCmS2=-20, drag=1.2, lifeMs=[900, 1400], sizeCm=[6, 9], alpha=[0.8, 0],
            alphaPow=1.5, spinDegS=[-90, 90], rotationDeg=[0, 360], color=ECHO_RIM, delayMs=2550),
    ], note="the echo ghost (U1 echo lore look, 3 s): cold ash motes rising off the figure, a faint cold ring, a "
            "dispersal of cold ash as it fades (echo.body #A8B0C4 / echo.rim #DCE4F2; never violet or ember)")
    # the Knellwatch toll (bell frame socket ``bell``, ~2.3 m up): pale tones rolling out over the camp, ash shaken off
    bell = [0.0, 0.0, 232.0]
    # VFX r5: kept apart from the Bonegorger's hostile Clapper Slam (notify.FX_BellRing, now a fast cracked dust ring
    # on the ground): the keepers' toll is slow and soft and lives in the air at the bell — three tone ripples
    # 350 ms apart, a faint long swell on the ground only
    rec["camp.bell_toll"] = R("ash", [
        ripple_air("origin", 80, 320, 1300, "#EDE6D8", alpha=0.5, h="custom", offset=bell),
        ripple_air("origin", 80, 320, 1300, "#EDE6D8", alpha=0.38, h="custom", offset=bell, delay=350),
        ripple_air("origin", 80, 320, 1300, "#EDE6D8", alpha=0.26, h="custom", offset=bell, delay=700),
        ripple_ground("origin", 120, 560, 2000, "#D8D0BE", alpha=0.14, body=None, pow=1.0, delay=150),
        ash_flakes("origin", 14, h="custom", offsetCm=[0.0, 0.0, 250.0], radius=60, speed=(10, 40),
                   up=(0, 30), gravity=90, life=(1600, 2400), size=(6, 10), color=ASH_LIGHT, dir="flat"),
    ], note="the keepers' bell tolls (story beats; U4 / U7 bell cue at the bell frame's `bell` socket): slow, soft "
            "pale tones in the air at the bell, a faint swell over the ground, ash shaken off the frame — no violet "
            "(the bell is the living's), no warm light (the lantern is the frame's own); the hostile clapper slam "
            "(notify.FX_BellRing) is a fast cracked dust ring instead")
    # the march brazier (camp_torch@march = SM_Prop_March_Brazier, socket flame ≈ 98 cm): a smoulder, NO light —
    # chapter1.md §15 keeps warm light for the hearth, the forge, the bell-frame lantern and Crowmarket's cookfire;
    # camp.torch (a flame + a #FFB060 light) at every brazier gave each march camp eight extra warm lights
    rec["camp.torch@march"] = R("ash", [
        lay(emitter="smoke", sprite="Smoke", randomVariants=2, at="origin", height=98, rate=1.6, durationMs=-1,
            spawnRadiusCm=10, dir="up", coneDeg=18, speedCmS=[8, 18], gravityCmS2=-24, drag=0.4,
            lifeMs=[1800, 2600], sizeCm=[30, 44], grow=2.0, alpha=[0.5, 0], fadeIn=0.25, alphaPow=1.3,
            color=SMOULDER_SMOKE_LIT),
        lay(emitter="motes", sprite="Ember", at="origin", height=100, rate=1.4, durationMs=-1, spawnRadiusCm=12,
            dir="up", coneDeg=35, speedCmS=[10, 30], gravityCmS2=-40, lifeMs=[500, 900], sizeCm=[4, 6],
            alpha=[0.8, 0], color=SMOULDER_SPARK, intensity=0.6),
        lay(sprite="AshFlake", randomVariants=4, orient="billboard", blend="translucent", at="origin", height=104,
            rate=1.6, durationMs=-1, spawnRadiusCm=14, dir="up", coneDeg=40, speedCmS=[8, 20], gravityCmS2=-6,
            drag=1.0, lifeMs=[1400, 2200], sizeCm=[6, 9], spinDegS=[-90, 90], rotationDeg=[0, 360],
            alpha=[0.7, 0], fadeIn=0.15, alphaPow=1.5, color=ASH_LIGHT),
    ], note="the march brazier's smoulder (SM_Prop_March_Brazier, camp_torch@march): grey-brown smoke curling up, "
            "a few dull red sparks (#7A3420, below the ember band) and ash flakes off the bed; NO light (chapter1.md "
            "§15 reserved light). Runtime: EffectSpots must resolve camp.torch@<zone theme> before camp.torch (U1)")
    return rec


# ── hue-audit fixes on today's hero recipes ───────────────────────────────────────────────────────────────
FILL_FIXES = {
    # (recipe, sprite): new decal growth (fx_recipes.decal's g0 .85 start → grow/.85 < 1.5: no longer a fill)
    ("proj.poison_arrow.hit", "Puddle"): 1.2,
    ("skill.bleed_strike", "Puddle"): 1.22,
    ("skill.poison_blade", "Puddle"): 1.2,
}


def patch_heroes(recipes: dict) -> list[str]:
    changed = []
    v = recipes.get("skill.vanish")
    if v:
        for L in v["layers"]:
            if L.get("color") == "#24103E":
                L["color"] = "dark"
                changed.append("skill.vanish: scorch #24103E → shadow.dark")
    for (rid, sprite), g in FILL_FIXES.items():
        r = recipes.get(rid)
        if not r:
            continue
        for L in r["layers"]:
            if L.get("sprite") == sprite and L.get("orient", "ground") == "ground" and L.get("grow", 1) >= 1.5:
                L["grow"] = round(g / 0.85, 3)
                changed.append(f"{rid}: {sprite} grow → {L['grow']}")
    m = recipes.get("skill.meteor")
    if m:
        for L in m["layers"]:
            # the incoming shadow: it no longer fills from nothing (60 → 192 cm at α → .6) — it starts at 130 cm and
            # darkens as it widens to 190 (< 1.5×), so the hero's meteor never speaks the enemy telegraph language
            if L.get("emitter") == "glow" and L.get("orient") == "ground" and L.get("color") == "#1A0E08":
                L["sizeCm"] = 130
                L["grow"] = 1.46
                changed.append("skill.meteor: ground shadow 130 → 190 cm (grow 1.46)")
    return changed


def patch_ambient(recipes: dict) -> None:
    s = recipes.get("ambient.sparks")
    if s:
        s["palette"] = "embers"
        for L in s["layers"]:
            if L.get("color") == "#FF6A3A":
                L["color"] = "event"
            # 14-22 cm motes at intensity 2.2 bloom into soft violet balloons over the Breach Rim: sparks are points
            if L.get("sprite") == "Ember" and L.get("sizeCm") == [14, 22]:
                L["sizeCm"] = [8, 13]
        s["note"] = ("motes rising: the embers palette's mid (#FF6A3A, the abyss_rift ember storm) without a context "
                     "colour; the weather tint when the runtime passes it (the Breach Rim region: #B02FD0); 8-13 cm "
                     "points, not blobs")


# ── material contracts (U2 / U3 build them; fx_textures_dark.py writes them to manifest.json → materials) ─────
TELEGRAPH = {
    "kind": "Material",
    "name": "M_AF_Telegraph",
    "use": "the enemy damage telegraph ground decal (U2 AbyssTelegraphs: EvTelegraph shape / start / contact / style; "
           "EvHazard zones in the hazard style, EvArena boundaries in the arena style). One pooled ground quad per telegraph, 8 cm over the terrain (the "
           "ground-lift rule of the sprite quads), translucent, unlit, no depth fade, sorted over the ground decals",
    "textures": {
        "Rim": {"texture": "T_FX_TelegraphRim", "srgb": False, "address": "wrap U, clamp V",
                "map": "U = arc length along the boundary / RimTileCm; V = (inward distance to the boundary) / "
                       "RimWidthCm; colour = RimColor x tex.rgb, opacity = tex.a x RimAlpha"},
        "Fill": {"texture": "T_FX_TelegraphFill", "srgb": False, "address": "wrap",
                 "map": "world XY / FillTileCm (never the shape's UV, so the grain does not swim with progress); "
                        "colour = FillColor x (1 - FillGrain x (1 - tex.rgb)) (at full strength the plate edges "
                        "read as flagstone paving; .3 keeps a grain, not plates)"},
    },
    "shapes": {
        "circle": "d = |p - origin| - radius",
        "ring": "d = max(|p - origin| - radius, innerRadius - |p - origin|)",
        "cone": "the circle of radius clipped to |angle(p - origin, dir)| <= arcDeg / 2 (the edges are rim too)",
        "line": "the box origin -> origin + dir x length, half-width width / 2 (rounded far end)",
    },
    "params": {
        "RimColor": CRIMSON, "FillColor": BONE, "RimAlpha": 0.95, "RimWidthCm": 26.0, "RimTileCm": 120.0,
        "FillTileCm": 160.0, "FillGrain": 0.3, "FillAlphaStart": 0.07, "FillAlphaContact": 0.22,
        "FrontWidthCm": 5.0, "FrontAlpha": 0.85, "FrontColor": "#FFF4EE",
    },
    "progress": "Progress = (now - startMs) / (contactMs - startMs), clamped: the fill grows from the origin (circle, "
                "cone, ring: radially to Progress x radius; line: along the length to Progress x length) with a bright "
                "front band FrontWidthCm wide at its edge; fill alpha lerps FillAlphaStart -> FillAlphaContact "
                "(low on purpose: translucency blends in linear light, so bone at alpha .5 over the #3A332D mud "
                "already reads as an opaque pale paving that hides the ground). The fill reaches the rim exactly at "
                "the contact beat (monsters.md 1.3)",
    "urgency": "over the last 30 % of the windup the rim alpha pulses x (0.82 + 0.18 sin(2 pi t / 0.18 s))",
    "contact": {"flashMs": 110, "fadeMs": 170, "fillAlpha": 0.35,
                "look": "at Progress 1 the fill jumps to FrontColor alpha fillAlpha and the rim to #FF3A48, then everything "
                        "fades out over fadeMs; the recipe telegraph.contact (or .line / .cone) plays at the same "
                        "instant for the dust and the rim snap. The fill stays translucent (VFX r5: bone at .26 rising "
                        "to a .45 flash read as a near-opaque white paving over the ground and the hero's feet at "
                        "760-900 ms); the bright front band and the urgency pulse carry the read"},
    "styles": {
        "normal": {},
        "elite": {"RimWidthCm": 30.0, "innerRim": {"atFraction": 0.82, "alpha": 0.6, "widthCm": 10.0},
                  "note": "a second, thinner rim inside: elites telegraph the same hue with more ink"},
        "boss": {"RimWidthCm": 36.0, "FillAlphaContact": 0.27, "innerRim": {"atFraction": 0.82, "alpha": 0.7,
                 "widthCm": 12.0}, "urgencyFrom": 0.5,
                 "note": "bosses: the widest rim, the inner rim, urgency from half the windup"},
        "hazard": {"fill": False, "RimAlpha": 0.75, "pulse": "x (0.8 + 0.2 sin(2 pi t / 1.2 s))",
                   "window": "on for the zone's whole damage window: a hazard that follows a windup (the breach-seep's "
                             "line) keeps its crimson line from the windup's contact until the zone ends (its last "
                             "tick), fading out over fadeMs — the decal hands over from its windup style, it never "
                             "drops to violet only while it still hurts",
                   "note": "a lingering zone (EvHazard): rim only; what burns inside is the hazard.* recipe"},
        "arena": {"fill": False, "RimAlpha": 0.9, "RimWidthCm": 34.0, "rimSide": "outside",
                  "outerWash": {"color": CRIMSON, "alpha": 0.12, "fadeInCm": 60.0},
                  "pulse": "x (0.85 + 0.15 sin(2 pi t / 1.2 s))",
                  "map": "the damage is OUTSIDE the boundary (EvArena, e.g. Valdras P3's closing banner ring: 4 % max "
                         "life per second outside it): Rim V = the OUTWARD distance from the boundary / RimWidthCm, so "
                         "the scorched line sits on the boundary and the 45-degree hatch band fades outward, into the "
                         "side that hurts; outside the rim a faint flat wash of RimColor at outerWash.alpha (ramping "
                         "in over fadeInCm) — no fill sweep, no bone; inside stays clean",
                  "note": "an arena boundary (EvArena): which side hurts must read without the recipe — crimson rim, "
                          "hatch facing outward, a faint crimson wash outside; the fire wall on the edge is "
                          "hazard.banner_ring"},
    },
    "rules": "crimson + bone only — no hero recipe draws a filling ground shape (hue audit fill rule); enemy magic "
             "that is not a damage hazard stays breach-violet",
    "previews": ["Previews/vfx_effects/telegraph.contact*/*.png, hazard.*/*.png, death.cinderbloat/*.png, "
                 "elite.fire_enhanced.death/*.png (the material emulated per pixel on the march ground; styles "
                 "normal / boss / hazard / arena / elite; index: Previews/vfx_effects/README_dark.md)"],
    "owner": "A12 (vfx/fx_recipes_dark.py TELEGRAPH, textures by vfx/fx_textures_dark.py); built by U2",
}

ASH_DISSOLVE = {
    "kind": "MaterialFunction",
    "name": "M_AF_AshDissolve",
    "use": "U3 corpses in ash mode: the death pose holds, the body turns to ash and crumbles into a husk that keeps its "
           "place for the corpse TTL (20 s), then blows away; Rise reverses it. Applied to every toon slot of the "
           "character (the ink hull follows the same mask so no ink floats over dissolved parts)",
    "textures": {"Noise": {"texture": "T_FX_AshNoise", "srgb": False, "address": "wrap",
                           "map": "object-space XZ / 60 cm (R fBm front, G cracks, B per-plate random)"}},
    "params": {"AshColor": "#6E6A62", "AshShade": "#3E3B37", "EdgeColor": "#B8B2A6", "EdgeWidth": 0.06,
               "EmissiveOff": True},
    "timeline": {
        "holdMs": 450, "greyMs": 380, "crumbleMs": 900, "crumbleTo": 0.38, "expireMs": 700,
        "grey": "after the Death clip ends + holdMs: toon colours lerp to AshColor (lit) / AshShade (shade band) over "
                "greyMs; emissive markers (violet eyes, seams) go out at the start of the grey",
        "crumble": "then Dissolve 0 -> crumbleTo over crumbleMs: a pixel drops when (B x 0.6 + R x 0.3 + (1 - G) "
                   "x 0.15 + heightFraction x 0.3) < Dissolve x 1.4 — whole plates (B dominates: ~12 cm flakes, "
                   "never per-pixel grain), cracks first, upper parts before the lower; the surviving edge band "
                   "EdgeWidth glows EdgeColor (pale ash, never warm). The recipe corpse.crumble plays at the start "
                   "of the crumble",
        "husk": "the remaining ~62 % stays (ash-coloured, no ink fade) until the TTL",
        "expire": "at the TTL: Dissolve crumbleTo -> 1 over expireMs + corpse.expire",
        "burn": "corpse.burn (a fire kill / fire over a corpse): the grey starts at once (no holdMs) under the "
                "flare and the crumble follows as usual; the husk is unraisable",
        "rise": "monster.raise: Dissolve and grey reverse over the Rise clip's first 60 %",
    },
    "owner": "A12 (vfx/fx_recipes_dark.py ASH_DISSOLVE, T_FX_AshNoise by vfx/fx_textures_dark.py); built by U3",
}


# ── bindings (the JSON's ``dark`` section) ─────────────────────────────────────────────────────────────────
BINDINGS = {
    "ambient.ashfall": ("zone weather ambience ashfall (emerald_plains); Context.Color = the weather tint", "W1"),
    "ambient.sparks": ("ambience sparks; pass the weather tint as Context.Color (Breach Rim #B02FD0)", "W1"),
    "elite.fire_enhanced": ("elite affix loop", "W1"), "elite.swift": ("elite affix loop", "W1"),
    "elite.teleporting": ("elite affix loop", "W1"), "elite.extra_strong": ("elite affix loop", "W1"),
    "elite.curse_aura": ("elite affix loop", "W1"), "elite.vampiric": ("elite affix loop", "W1"),
    "elite.frozen": ("elite affix loop", "W1"), "elite.unburied": ("elite affix loop (affix data is W2)", "W2"),
    "teleport.blink": ("EvEntityTeleported EliteBlink", "W1"),
    "notify.FX_Fuse": ("SK_Mon_Cinderbloat Death @0", "W1"), "notify.FX_Burst": ("SK_Mon_Cinderbloat Death @800", "W1"),
    "notify.FX_Land": ("Breachclot / Cinderbloat / Slime Walk", "W1"),
    "notify.FX_BellToll": ("SK_Mon_Corpseherd Attack01 @500", "W1"),
    "notify.FX_bell_ring": ("SK_Mon_Corpseherd Cast01 @650 (Call the Flock)", "W2"),
    "notify.FX_BellRing": ("SK_Mon_Bonegorger Attack02 @800 (Clapper Slam)", "W2"),
    "notify.FX_Roar": ("SK_Mon_Valdras Roar @450 (intro, phase changes); SK_Mon_GoblinChief Roar", "W1"),
    "notify.FX_Brand": ("SK_Mon_Valdras Cast01 @900 (Oath-Brand)", "W2"),
    "notify.FX_DrumBeat_1": ("SK_Mon_DeserterDrummer Cast01 beats 1-4 (aliases _2.._4)", "W2"),
    "notify.FX_Dig": ("SK_Mon_RisenFootman Idle_Order_Dig", "W2"),
    "notify.FX_BannerSnap": ("SK_Mon_DeadStandardBearer Idle_Order", "W2"),
    "death.unburied": ("monster kill, family unburied", "runtime"), "death.breach": ("monster kill, family breach", "runtime"),
    "death.living": ("monster kill, family living", "runtime"),
    "death.gravegnawers": ("monster kill, family gravegnawers", "runtime"),
    "death.valdras": ("monster kill, defId valdras (actor frame: Blow = his facing)", "runtime"),
    "death.cinderbloat": ("monster kill, defId cinderbloat (quiet: the fuse + burst notifies are its death)",
                          "runtime"),
    "elite.fire_enhanced.death": ("W2 on-death, elite fire_enhanced: at the kill, point = the corpse; the burst "
                                  "lands at +1000 ms under U2's elite-style r2 decal", "W2"),
    "elite.frozen.death": ("W2 on-death, elite frozen: at the kill, point = the corpse; the ring at +600 ms, "
                           "the slow 2.5 s", "W2"),
    "monster.dead_standard_bearer.aura": ("W2 the standard-bearer's +15 % aura (r 5 tiles): loop on the bearer "
                                          "(origin, attach; Blow = its facing, as PlayNotify) from spawn to death", "W2"),
    "monster.raise.mark": ("the raise ability's marker (900 ms) under the chosen corpse", "W2"),
    "monster.raise": ("EvMonsterRaised", "W2"), "monster.block": ("EvBlock", "W2"),
    "monster.ash_flakes": ("a monster loop from the manifest fx.ashFlakes (cinderbloat)", "runtime"),
    "monster.valdras.standard_fire": ("EvMonsterPhase valdras index 2, loop until death; Blow = his facing "
                                      "(as PlayNotify: the offset is the standard on his back)", "W2"),
    "monster.valdras.feed": ("Feed: origin = the corpse, target = Valdras", "W2"),
    "telegraph.contact": ("U2 telegraph contact (circle / ring), Context.RadiusCm", "W2"),
    "telegraph.contact.line": ("U2 telegraph contact (line: origin -> target)", "W2"),
    "telegraph.contact.cone": ("U2 telegraph contact (cone: origin + blow, RadiusCm)", "W2"),
    "hazard.breach_fire": ("EvHazard breach fire / Oath-Brand brand, RadiusCm", "W2"),
    "hazard.breach_seep": ("EvHazard seep line (origin -> target, Blow along it; the hazard-style decal stays on "
                           "through the ticks)", "W2"),
    "hazard.banner_ring": ("EvArena ring, RadiusCm = the current arena radius (with the arena-style decal)", "W2"),
    "corpse.crumble": ("U3 husk crumble", "W2"), "corpse.expire": ("U3 husk TTL end", "W2"),
    "corpse.burn": ("U3 corpse burned, Context.Color = the fire's colour", "W2"),
    "corpse.consume": ("U3 Consume channel (2 s): origin = the gnawer, target / point = the husk, Blow = gnawer -> "
                       "husk (the jaws ~44 cm along it; the grit spawns just short of them)", "W2"),
    "notify.FX_Frenzy": ("SK_Mon_OathZealot Roar @480 (the frenzy's enter action, K4 phase enterAction)", "W2"),
    "monster.oath_zealot.frenzy": ("EvMonsterPhase oath_zealot index 1, loop until death (origin, attach; Blow = "
                                   "his facing)", "W2"),
    "notify.FX_Keen": ("SK_Mon_Corpseherd Cast02 @800 (the Keen: U2 draws the crimson r4 ring decal to 800)", "W2"),
    "status.ashnumb": ("K6 StatusType::Ashnumb on the hero (status loop, as status.<type>) + the AshnumbTint "
                       "band (dark.materials.AshnumbTint)", "W2"),
    "prop.echo": ("U1 echo lore look: at the figure, with MI_AF_Echo's 3 s", "W1"),
    "camp.bell_toll": ("U4 / U7 bell cue at SM_Prop_March_BellFrame socket bell", "W1"),
    "camp.torch@march": ("U1: AAbyssZoneActor::BuildCamps' EffectSpot at a torch prop resolves camp.torch@<zone "
                         "presentation theme> before camp.torch (like the decor <id>@march lookup); the march "
                         "brazier's smoulder, no light", "runtime"),
}

INTEGRATION = {
    "deathFamily": "Monster kills play PlayFirst({death.<defId>, death.<family>, death.burst}) instead of death.burst "
                   "(UAbyssVfxSystem::HandleHit): the march families get their own ends (unburied / breach: violet; "
                   "living / gravegnawers: dust and ash, no light); keep the event colour #FF4444 for the fallback only. "
                   "The death ids play anchored to the dying actor: Context.Blow = the victim's facing at death (as "
                   "PlayNotify), not killer -> victim, so death.valdras' offsets stay on the standard on his back "
                   "whatever side the kill came from (hit.impact.kill and the death.burst fallback keep the hit's "
                   "blow). A def whose manifest fx has a fuse (the cinderbloat) has its own quiet death.<defId> "
                   "(death.cinderbloat): its Death notifies FX_Fuse @0 / FX_Burst @800 are the death, and the family "
                   "burst at the kill would read as burst -> fuse -> second burst (monsters.md 1.3)",
    "ambienceTint": "Play ambient.<ambience> with Context.Color = the zone weather's tint when it has one (ashfall "
                    "#CFCAC0; the Breach Rim's sparks #B02FD0): both recipes draw the event colour, and without a tint "
                    "ambient.sparks keeps its ember motes (abyss_rift)",
    "notifyPerMonster": "notify.FX_Roar is shared by Valdras and the old goblin chief; a per-asset chain "
                        "PlayFirst({notify.<SK name>.<N>, notify.<N>, npc.<N>}) would let later chapters give the "
                        "chief its own",
    "monsterLoops": "Start monster.ash_flakes on spawn for monsters whose manifest fx has ashFlakes (the "
                    "cinderbloat), stop it on death",
    "telegraph": "M_AF_Telegraph (dark.materials.M_AF_Telegraph): circle / cone / line / ring, normal / elite / boss / "
                 "hazard styles; play telegraph.contact[.line|.cone] at contact",
    "ashDissolve": "M_AF_AshDissolve (dark.materials.M_AF_AshDissolve) + corpse.* recipes on the husk timeline",
    "eliteDeath": "W2 on-death affix effects: play elite.<affix>.death (elite.fire_enhanced.death, "
                  "elite.frozen.death) at the kill beside death.<family>, point = the corpse; the recipe carries the "
                  "delay (burst +1000 ms, ring +600 ms), the core resolves the damage / slow at the same beat, and U2 "
                  "draws the Scorched burst's r2 decal in the elite style",
    "monsterAuras": "W2 aura loops: monster.dead_standard_bearer.aura on the bearer (origin, attach) while it lives "
                    "(stop on death); no telegraph decal (it is a buff, not a damage zone: DECISIONS_DARK DR5-aura)",
    "frenzy": "W2 (2H): the zealot's phase 1 (K4 enterAction Roar) plays Roar; at its FX_Frenzy notify the scar "
              "swatch's emissive ramps to the manifest fx.frenzy.scarEmissive and monster.oath_zealot.frenzy loops on "
              "the zealot until death (stop it on death; a leash reset that returns to phase 0 stops it too)",
    "ashnumb": "W2 (2H): status.ashnumb is the K6 Ashnumb status loop (play / stop as the other status.<type> loops); "
               "the per-stack band is dark.materials.AshnumbTint (the StatusTint path is its fallback)",
    "consumeClip": "W2 (2H): corpse.consume is timed for SK_Mon_Gravegnawer's Consume clip (loop 1000 ms, two bites; "
                   "the 2 s channel plays it twice): play the recipe once at the channel start with origin = the "
                   "gnawer, Blow = gnawer -> corpse",
    "monsterProjectiles": "proj.monster_bolt serves the arbalester (bone-white bolt, violet trail) and the "
                          "Corpseherd's bell toll alike (#B02FD0): a per-monster projectile vfxId would let the toll "
                          "fly as a ripple",
}


DECISIONS_DARK = {
    "DR5-aura": "(A12 VFX r5, pending: for DECISIONS DR5, core lane) monsters.md §2.2 asks for the standard-bearer's "
                "aura as 'a crimson ground halo, visible', but DR5 keeps warning crimson (#D11F2E rim + bone fill) for "
                "enemy damage telegraphs only. The aura (monster.dead_standard_bearer.aura) is drawn in the banner's "
                "cloth-dark crimson (#4A0E12 / #3A0A0C / #5E1012: DR5's 'crimson accent' cloth family, never #D11F2E), "
                "as a soft stain with no rim line and no fill sweep, so it reads as the banner's presence, not as a "
                "zone that is about to hurt",
    "DR5-slow": "(A12 VFX r5, pending) a slow zone that does not damage (the Gravecold elite's on-death frost ring) "
                "gets no crimson decal: its recipe (elite.frozen.death) carries its own 600 ms pre-cue in the "
                "affix's grey-blue",
    "DR5-arena": "(A12 VFX r5, pending) an EvArena boundary (damage outside) uses M_AF_Telegraph's arena style: "
                 "crimson rim with the hatch facing outward and a faint crimson wash outside; EvHazard zones keep "
                 "the hazard-style rim on through every damage tick",
}


# ── registration (called by fx_recipes.build) ─────────────────────────────────────────────────────────────
def normalise(recs: dict) -> None:
    """Authoring shorthands → the runtime schema: ``height: "custom"`` + a 3-D ``offsetCm`` becomes a numeric height
    (the offset's z) with a flat offset; a rate emitter drops its burst ``count``."""
    for r in recs.values():
        for L in r["layers"]:
            if L.get("height") == "custom":
                off = list(L.get("offsetCm", [0.0, 0.0, 0.0]))
                L["height"] = round(float(off[2]), 1)
                off[2] = 0.0
                if any(abs(v) > 1e-6 for v in off):
                    L["offsetCm"] = [round(float(v), 1) for v in off]
                else:
                    L.pop("offsetCm", None)
            if L.get("rate", 0) > 0:
                L.pop("count", None)
            elif L.get("count") == 0:
                raise ValueError(f"a burst layer with count 0: {L}")


def recipes() -> dict:
    FR.load_diameters()
    out = {}
    for group in (ambience, elites, notifies, deaths, monsters, telegraphs, corpses, wave2, world):
        out.update(group())
    normalise(out)
    return out


ALIASES = {"notify.FX_DrumBeat_2": "notify.FX_DrumBeat_1", "notify.FX_DrumBeat_3": "notify.FX_DrumBeat_1",
           "notify.FX_DrumBeat_4": "notify.FX_DrumBeat_1"}


def apply(recs: dict, pal: dict) -> dict:
    """Register the dark recipes and palettes into ``fx_recipes.build()``'s working set (before its post passes:
    strike offsets, camera bias, ground lift, prewarm and the mesh dissolve apply to them too). ``pal`` is the
    caller's ``PAL`` (the shadow palette is re-coloured in place). Returns a report."""
    for k, v in DARK_PAL.items():
        pal[k] = v
    old_shadow = pal.get("shadow")
    pal["shadow"] = SHADOW_PAL
    new = recipes()
    replaced = sorted(k for k in new if k in recs)
    recs.update(copy.deepcopy(new))
    fixes = patch_heroes(recs)
    patch_ambient(recs)
    return {"added": sorted(k for k in new if k not in replaced), "replaced": replaced, "heroFixes": fixes,
            "shadow": {"from": list(old_shadow[:4]) if old_shadow else None, "to": list(SHADOW_PAL[:4])}}


def doc_section(report: dict) -> dict:
    """The ``dark`` section of vfx_recipes.json: bindings, the material contracts, what changed."""
    return {
        "_doc": "Dark remake A12 (vfx/fx_recipes_dark.py): bindings of the march recipes (W1 = bound today, W2 = the "
                "wave-2 packages play them, runtime = a one-line UAbyssVfxSystem change), the material contracts and "
                "the recolours of shipped recipes",
        "bindings": {k: {"event": e, "wave": w} for k, (e, w) in sorted(BINDINGS.items())},
        "aliases": ALIASES,
        "materials": {"M_AF_Telegraph": TELEGRAPH, "M_AF_AshDissolve": ASH_DISSOLVE, "AshnumbTint": ASHNUMB},
        "decisions": DECISIONS_DARK,
        "recoloured": report.get("replaced", []),
        "heroFixes": report.get("heroFixes", []),
        "shadowPalette": report.get("shadow"),
        "palettes": {k: {"core": c, "mid": m, "rim": r, "dark": d} for k, (c, m, r, d, _) in DARK_PAL.items()},
    }
