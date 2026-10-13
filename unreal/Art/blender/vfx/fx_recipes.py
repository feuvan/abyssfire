"""VFX recipes for the UE particle runtime — ``Art/Export/VFX/vfx_recipes.json`` + ``fx_palettes.json``.

    /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_recipes.py        (pure Python, no bpy needed)

Every effect of art-inventory-ch1.md §8.5–§8.7 (combat feedback, the 40 skills, projectiles, ground effects,
statuses (DECISIONS R8), rewards, world) as a recipe in the runtime's schema (WorldContract 5,
``Source/Abyssfire/Public/Vfx/AbyssVfxRecipe.h``): a list of layers, each one emitter of pooled ISM quads
(``T_FX_<sprite>``) or FX meshes (``SM_FX_*``). The runtime merges this file over its built-in defaults (same id
replaces), so every id below is the art pass of that effect.

Sources: the web compositions (``SkillEffectSystem.ts``, ``VFXManager.ts``, ``FxKit.ts`` via the spec tables),
converted with the runtime's factor 100/45 cm per web px (sizes, radii, speeds) — AoE radii in tiles × 100 cm.
Look rules of the art pass:
* solid pieces are toon FX meshes with ink (slash arcs, arrows, hex shields, crystals, debris, stun stars, the
  fireball core); light is additive sprites; smoke, dust, liquids and cel flames are translucent sprites — the
  pre-coloured cel ``Flame`` cells are drawn with a white tint so they keep the web's 3-band flame palettes;
* every ground ring / shock is two layers like the web (a translucent body for presence over the bright plains +
  the additive glow);
* sizes: quad sprites = the quad height (``Ring`` radius = 0.4375 × size, ``Shock`` / decals radius = size / 2,
  ``Slash`` arc centre on the quad's bottom edge); meshes = the bounding diameter drawn (``MESH_DIAMETER``, from
  the manifest ``boundsDiameterCm``).

Runtime notes the recipes work around (``integration`` section of the JSON): ``dir: blow`` adds a random vertical
component (−0.15…0.35), so flight-aligned heads use one long-lived particle per projectile (a stable pitch) and the
multishot volley adds a compensating gravity; mesh layers are always ``M_AF_FX_Mesh`` (tint + toon + ink), never
additive.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ART = HERE.parent.parent
EXPORT = ART / "Export"
OUT = EXPORT / "VFX"

PX = 100.0 / 45.0          # cm per web px (runtime VfxPxToCm)
RING_R = 0.4375            # T_FX_Ring: ring radius / quad size

# ── palettes (FxKit PAL, art-inventory-ch1.md §8.2) ───────────────────────────────────────────────────
PAL = {
    "fire":      ("#FFF3C4", "#FFA532", "#FF5418", "#8A1C08", 0),
    "frost":     ("#FFFFFF", "#A6F0FF", "#46B4F0", "#1E5AA8", 1),
    "lightning": ("#FFFFFF", "#CFE0FF", "#8C8CFF", "#4A3AA8", 1),
    "poison":    ("#F0FFC0", "#9AEC40", "#3FAE2E", "#1C5A1E", 2),
    "shadow":    ("#F0DCFF", "#B478FF", "#7030C0", "#24103E", 3),
    "holy":      ("#FFFBE4", "#FFE27A", "#F4AC28", "#8A5A10", 4),
    "steel":     ("#FFFFFF", "#EEF3FB", "#AABBD4", "#56647C", 6),
    "blood":     ("#FFB0A0", "#E8342C", "#A01420", "#4A0810", 5),
    "rage":      ("#FFE0B0", "#FF5A2A", "#D01E1E", "#5A0A0A", 5),
    "arcane":    ("#FFFFFF", "#D8B4FF", "#8A5CFF", "#34207A", 3),
    "nature":    ("#F6FFE0", "#A8F07A", "#40C060", "#1A5A2A", 2),
    "earth":     ("#FFF0C8", "#E8C47A", "#B08850", "#5A4430", 4),
    "portal":    ("#E8FFF8", "#80F0E0", "#3FE0A0", "#14584A", 1),
    # the game's mana language is blue (HUD MP globe 0x2A5FD6, MP potion liquid #2A5AE8 / glow #4A8AFF, spec §7)
    "mana":      ("#DDEEFF", "#4A8AFF", "#2A5AE8", "#102A72", 1),
}
FLAME = {"fire": 0, "frost": 1, "poison": 2, "shadow": 3, "holy": 4, "rage": 5, "plain": 6, "camp": 7}
SLASH = {"warrior": 0, "rogue": 1, "mage": 2, "heavy": 3}
IMPACT = {"normal": 0, "heavy": 1, "crit": 2, "kill": 3}

# FX mesh bounding diameters (cm) — refreshed from manifest.json (boundsDiameterCm) when present
MESH_DIAMETER = {
    "SM_FX_Arrow": 52.0, "SM_FX_PoisonArrow": 54.0, "SM_FX_IceArrow": 57.0, "SM_FX_IceShard": 26.0,
    "SM_FX_Fireball": 62.0, "SM_FX_SpiritBolt": 44.0, "SM_FX_Rock_A": 14.0, "SM_FX_Rock_B": 15.0,
    "SM_FX_MeteorRock": 70.0, "SM_FX_HexRing": 210.0, "SM_FX_HexDome": 235.0, "SM_FX_FrostNova": 150.0,
    "SM_FX_FrostCage": 160.0, "SM_FX_FrostCrust": 85.0, "SM_FX_StunStar": 20.0, "SM_FX_ChainLink": 13.0,
    "SM_FX_Chain": 58.0, "SM_FX_SlashArc_Warrior": 226.0, "SM_FX_SlashArc_Rogue": 215.0,
    "SM_FX_SlashArc_Mage": 200.0, "SM_FX_WhirlBand": 215.0, "SM_Pickup_GoldCoin": 18.0,
    "SM_FX_HexPlate": 66.0, "SM_FX_Bubble": 225.0, "SM_FX_SlashArc_Warrior_Back": 226.0,
}
# arc / band radius of the ribbon meshes in mesh units (m) — size for a ribbon of radius R = R / ARC_R × diameter
ARC_R = {"SM_FX_SlashArc_Warrior": 1.0, "SM_FX_SlashArc_Warrior_Back": 1.0, "SM_FX_SlashArc_Rogue": 1.0, "SM_FX_SlashArc_Mage": 0.95,
         "SM_FX_WhirlBand": 1.0, "SM_FX_HexRing": 0.8, "SM_FX_HexDome": 0.89, "SM_FX_FrostNova": 0.5}


def load_diameters() -> None:
    p = EXPORT / "manifest.json"
    if not p.exists():
        return
    assets = json.loads(p.read_text()).get("assets", {})
    for name in list(MESH_DIAMETER):
        e = assets.get(name)
        if not e:
            continue
        d = e.get("boundsDiameterCm")
        if d is None and "boundsCm" in e:
            lo, hi = e["boundsCm"]["min"], e["boundsCm"]["max"]
            d = math.dist(lo, hi)
        if d:
            MESH_DIAMETER[name] = round(float(d), 1)


def mesh_size(mesh: str, scale: float = 1.0) -> float:
    return round(MESH_DIAMETER[mesh] * scale, 1)


def ribbon_size(mesh: str, radius_cm: float) -> float:
    return round(MESH_DIAMETER[mesh] * radius_cm / (ARC_R[mesh] * 100.0), 1)


def px(v: float) -> float:
    return round(v * PX, 1)


# ── layer helpers (one web FxKit primitive each) ──────────────────────────────────────────────────────
def clean(d: dict) -> dict:
    return {k: v for k, v in d.items() if v is not None and not (k == "delayMs" and v == 0)}


def lay(**kw) -> dict:
    return clean(kw)


def glow(at, size_cm, life, color, *, grow=2.17, alpha=0.9, h="chest", delay=0, **kw):
    """web glow(): drawn diameter 0.6·size → grow·size (the preset's 0.6 start is folded into ``size_cm``)."""
    return lay(emitter="glow", at=at, height=h, sizeCm=size_cm, grow=grow, lifeMs=life, alpha=[alpha, 0],
               color=color, delayMs=delay, **kw)


def flash(at, size_px, color="core", *, life=160, h="chest", delay=0, glow_color=None, **kw):
    """web flash(): a coloured glow 1.6× size / 1.5× life α .6 + the white-hot core 0.35 → 0.85 × size."""
    return [
        lay(emitter="glow", at=at, height=h, sizeCm=px(size_px * 1.6 * 0.6), grow=1.25 / 0.6, lifeMs=int(life * 1.5),
            alpha=[0.6, 0], color=glow_color or ("mid" if color == "core" else color), delayMs=delay, **kw),
        lay(emitter="flash", at=at, height=h, sizeCm=px(size_px * 0.35), grow=0.85 / 0.35, lifeMs=life,
            color=color, delayMs=delay, **kw),
    ]


def glint(at, size_px, *, life=200, h="chest", delay=0, rot=0, color="white", **kw):
    return lay(emitter="glint", at=at, height=h, sizeCm=px(size_px * 0.3), grow=round(1 / 0.3, 3), lifeMs=life,
               rotationDeg=rot,
               spinDegS=69, color=color, delayMs=delay, **kw)


def sparks(at, n, color, *, speed=(90, 200), life=(180, 340), dir="random", cone=None, h="chest", delay=0,
           gravity=0, drag=3, size=1.0, alt="white", alt_every=3, **kw):
    s0, s1 = speed
    return lay(emitter="sparks", at=at, height=h, count=n, speedCmS=[px(s0), px(s1)], lifeMs=list(life), dir=dir,
               coneDeg=cone, gravityCmS2=px(gravity) if gravity else None, drag=drag,
               sizeCm=[round(9 * size, 1), round(13 * size, 1)], color=color, altEvery=alt_every,
               altColor=alt if alt_every else None, delayMs=delay, **kw)


def motes(at, n, color, *, sprite="Ember", radius_px=0, speed=(20, 70), life=(400, 700), size=(6, 12), rise=0,
          gravity=0, h="chest", delay=0, alt=None, alt_every=0, spin=120, alpha=1.0, **kw):
    return lay(emitter="motes", sprite=sprite, at=at, height=h, count=n, spawnRadiusCm=px(radius_px) or None,
               speedCmS=[px(speed[0]), px(speed[1])], lifeMs=list(life), sizeCm=list(size),
               gravityCmS2=px(gravity - rise) if (gravity or rise) else 0, color=color, altEvery=alt_every or None,
               altColor=alt, spinDegS=[-spin, spin], alpha=[alpha, 0], delayMs=delay, **kw)


def ring(at, r0_cm, r1_cm, life, color, *, alpha=1.0, delay=0, pow=3, h=None, body=True, **kw):
    """web ring(): expanding ground ring r0 → r1 (radius, cm) — a translucent body (α .55) + the additive ring."""
    size = round(r0_cm / RING_R, 1)
    grow = round(r1_cm / max(r0_cm, 0.1), 3)
    out = []
    if body:
        out.append(lay(sprite="Ring", orient="ground", blend="translucent", at=at, height=h if h is not None else 2,
                       sizeCm=size, grow=grow, sizePow=pow, lifeMs=life, alpha=[round(alpha * 0.55, 3), 0],
                       fadeIn=0.05, alphaPow=1.5, color=color, delayMs=delay, **kw))
    out.append(lay(sprite="Ring", orient="ground", at=at, height=h if h is not None else 3, sizeCm=size, grow=grow,
                   sizePow=pow, lifeMs=life, alpha=[alpha, 0], fadeIn=0.05, alphaPow=1.5, color=color,
                   intensity=1.2, delayMs=delay, **kw))
    return out


def shock(at, r0_cm, r1_cm, life, color, *, alpha=0.9, delay=0, **kw):
    """web shock(): filled shockwave disc r0 → r1 (radius cm), translucent α .45 + additive."""
    size = round(2 * r0_cm, 1)
    grow = round(r1_cm / max(r0_cm, 0.1), 3)
    return [
        lay(emitter="shock", blend="translucent", at=at, height=1.5, sizeCm=size, grow=grow, lifeMs=life,
            alpha=[round(alpha * 0.45, 3), 0], fadeIn=0.04, alphaPow=1.2, color=color, delayMs=delay, **kw),
        lay(emitter="shock", at=at, height=2.5, sizeCm=size, grow=grow, lifeMs=life, alpha=[alpha, 0], fadeIn=0.04,
            alphaPow=1.2, color=color, intensity=1.15, delayMs=delay, **kw),
    ]


def decal(at, sprite, r_cm, life, color, *, alpha=1.0, blend="translucent", grow=1.0, spin=0, fade_in=0.06,
          pow=3, delay=0, rot=(0, 360), h=1, **kw):
    """web decal(): ground decal of radius r (size 2r); scale .85 → grow (or 1 → grow when shrinking)."""
    g0 = 1.0 if grow < 1 else 0.85
    return lay(emitter="decal", sprite=sprite, blend=blend, at=at, height=h, sizeCm=round(2 * r_cm * g0, 1),
               grow=round(grow / g0, 3), sizePow=3, lifeMs=life, alpha=[alpha, 0], fadeIn=fade_in, alphaPow=pow,
               spinDegS=round(math.degrees(spin), 1) if spin else None, rotationDeg=list(rot), color=color,
               delayMs=delay, **kw)


def smoke(at, n, color, *, radius_px=8, speed=(15, 45), life=(500, 800), size=(0.35, 0.6), rise=20, alpha=0.8,
          delay=0, h=20, sprite="Smoke", **kw):
    """web smoke(): cartoon puffs (64 px texture) scale .6 → 1.25 × size; ``rate=`` makes it a continuous trail."""
    d = dict(emitter="smoke", sprite=sprite, randomVariants=2 if sprite == "Smoke" else None, at=at, height=h,
             count=n, spawnRadiusCm=px(radius_px), speedCmS=[px(speed[0]), px(speed[1])], lifeMs=list(life),
             sizeCm=[px(64 * size[0] * 0.6), px(64 * size[1] * 0.6)], grow=round(1.25 / 0.6, 3),
             gravityCmS2=-px(rise), alpha=[alpha, 0], color=color, delayMs=delay)
    d.update(kw)
    if d.get("rate"):
        d.pop("count", None)
    return clean(d)


def flames(at, n, cell, *, radius_px=10, rise=(30, 70), life=(300, 520), size=(0.5, 0.9), delay=0, stagger=0,
           on_ring=False, h=0, tint="white", rate=None, duration=None, attach=None, even=False, **kw):
    """web flames(): cel flame tongues (32×52 px) from a ground disc, rising, pivot at the base: the pre-coloured
    ``Flame`` cell ``cell`` (palette flame), translucent (sorted with the world)."""
    base = dict(emitter="flames", sprite="Flame", variant=cell, blend="translucent", orient="upright", at=at,
                height=h, spawnRadiusCm=px(radius_px), onRing=on_ring or None, upCmS=[px(rise[0]), px(rise[1])],
                lifeMs=list(life), sizeCm=[px(52 * size[0] * 0.85), px(52 * size[1] * 0.85)], aspect=0.5,
                grow=0.72, sizePow=2, alpha=[1, 0], fadeIn=0.15, alphaPow=1.2, color=tint, intensity=1.0,
                spinDegS=None, delayMs=delay, attach=attach)
    if even and on_ring and not rate:
        # evenly spaced round the ring: a zero-speed orbit places burst particle i at 360·i/n (random ring angles
        # cluster); a stagger becomes two half bursts on slightly different radii
        r = px(radius_px)
        base.pop("spawnRadiusCm", None)
        base.pop("onRing", None)
        if stagger:
            n1 = (n + 1) // 2
            a1 = dict(base, count=n1, orbit={"r0Cm": r, "r1Cm": r, "degS": 0})
            a2 = dict(base, count=n - n1, orbit={"r0Cm": round(r * 0.88, 1), "r1Cm": round(r * 0.88, 1),
                                                   "degS": 0}, delayMs=delay + stagger * 0.5)
            a1.update(kw)
            a2.update({k: v for k, v in kw.items() if k != "delayMs"})
            return [clean(a1), clean(a2)]
        base.update(count=n, orbit={"r0Cm": r, "r1Cm": r, "degS": 0})
    elif rate:
        base.update(rate=rate, durationMs=duration if duration is not None else -1)
    elif stagger:
        base.update(rate=round(n / (stagger / 1000.0), 2), durationMs=stagger)
    else:
        base.update(count=n)
    base.update(kw)
    return clean(base)


def debris(at, n, *, color="#8A7358", speed=(60, 130), delay=0, spawn_px=0, mesh="SM_FX_Rock_A", **kw):
    return lay(emitter="debris", mesh=mesh, at=at, count=n, speedCmS=[px(speed[0]), px(speed[1])],
               upCmS=[px(110), px(170)], sizeCm=[mesh_size(mesh, 0.7), mesh_size(mesh, 1.15)],
               spawnRadiusCm=px(spawn_px) or None, color=color, delayMs=delay, **kw)


def beam(at, h_px, w_px, life, color, *, alpha=0.9, delay=0, fade_in=0.1, cell=0, h=0, **kw):
    """web beam(): vertical column h × w px, scale .4/.5 → 1 (pow 3); base on the anchor."""
    return lay(emitter="beam", variant=cell, at=at, height=h, sizeCm=px(h_px * 0.4), aspect=round(w_px / h_px * 1.25, 3),
               grow=2.5, sizePow=3, lifeMs=life, alpha=[alpha, 0], fadeIn=fade_in, color=color, delayMs=delay, **kw)


def gather(at, n, color, r_px, life, *, sprite="Glow", size=(10, 16), delay=0, h="chest", **kw):
    return lay(emitter="gather", sprite=sprite, at=at, height=h, count=n,
               orbit={"r0Cm": [px(r_px * 0.8), px(r_px * 1.1)], "r1Cm": 0, "degS": [143, 229]},
               lifeMs=[int(life * 0.8), life], sizeCm=list(size), alpha=[0.2, 1], color=color, delayMs=delay, **kw)


def held(base: dict, a: float, P: int, pulse: float = 0.45) -> list:
    """a sprite that STAYS (the web's yoyo tweens on a persistent circle): one particle per half period P living
    2P with a triangle envelope (fadeIn .5) — two overlap, summing to a constant (1 − pulse)·α — plus one per
    2P (triangle 0 → pulse·α → 0): (1 − pulse)·α ↔ α, period 2P (the loot beam's steady + pulse pair)."""
    return [lay(**base, rate=round(1000.0 / P, 3), attach=True, lifeMs=2 * P, fadeIn=0.5, alphaPow=1,
                alpha=[round((1 - pulse) * a, 3), 0]),
            lay(**base, rate=round(1000.0 / (2 * P), 3), attach=True, lifeMs=2 * P, fadeIn=0.5, alphaPow=1,
                alpha=[round(pulse * a, 3), 0])]


def impact_star(at, kind, size_cm, color, *, life=140, delay=0, h="chest", rot=(0, 360), **kw):
    """Toon hit star (T_FX_Impact cell per hit profile): a white-hot core cell over the event colour."""
    return [
        lay(sprite="Impact", variant=IMPACT[kind], orient="billboard", at=at, height=h, sizeCm=size_cm, grow=1.45,
            sizePow=3, lifeMs=life, alpha=[1, 0], alphaPow=1.6, rotationDeg=list(rot), color=color, intensity=1.35,
            delayMs=delay, **kw),
    ]


def ribbon(mesh, at, radius_cm, life, color, *, h="chest", rot=0.0, spin=0.0, align=True, delay=0, alpha_pow=5.0,
           intensity=1.25, grow=1.08, **kw):
    """A toon ribbon mesh (slash arc / whirl band) round the anchor: ground orient (yaw), aligned to the blow."""
    return lay(mesh=mesh, sprite="Slash", orient="ground", blend="translucent", at=at, height=h,
               sizeCm=ribbon_size(mesh, radius_cm), grow=grow, sizePow=2, lifeMs=life, alpha=[1, 0],
               alphaPow=alpha_pow, rotationDeg=rot, spinDegS=spin or None, alignToBlow=align, color=color,
               intensity=intensity, delayMs=delay, **kw)


def swipe_glow(at, radius_cm, life, color, *, cell=0, h="chest", delay=0, alpha=0.75, **kw):
    """Additive crescent under / over a ribbon (Slash cell on the ground plane, arc centre = the anchor)."""
    size = round(radius_cm / 0.96, 1)
    return lay(emitter="swipe", sprite="Slash", variant=cell, at=at, height=h, offsetCm=[round(size / 2, 1), 0, 0],
               sizeCm=size, aspect=2.0, grow=1.08, lifeMs=life, alpha=[alpha, 0], alphaPow=1.6, color=color,
               intensity=1.25, delayMs=delay, **kw)


def slash_sprite(at, size_cm, life, color, *, cell=0, rot=0, delay=0, h="chest", alpha=1.0, blend=None, **kw):
    return lay(emitter="slash", sprite="Slash", variant=cell, at=at, height=h, sizeCm=size_cm, lifeMs=life,
               rotationDeg=rot, alpha=[alpha, 0], color=color, blend=blend, delayMs=delay, **kw)


def drops(at, n, color, *, speed=(80, 170), dir="blow", cone=80, gravity=420, h="chest", delay=0, size=(6, 9),
          life=(300, 450), **kw):
    return lay(sprite="Drop", orient="billboard", blend="translucent", at=at, height=h, count=n, dir=dir,
               coneDeg=cone, speedCmS=[px(speed[0]), px(speed[1])], upCmS=[40, 120], gravityCmS2=px(gravity),
               sizeCm=list(size), aspect=0.67, grow=1, lifeMs=list(life), alpha=[1, 0], alphaPow=3, color=color,
               delayMs=delay, **kw)


def light(radius_cm, alpha, flicker=True):
    return {"radiusCm": radius_cm, "alpha": alpha, "flicker": flicker}


def flat(*items) -> list:
    out = []
    for it in items:
        if isinstance(it, list):
            out.extend(flat(*it))
        elif it is not None:
            out.append(it)
    return out


def R(palette, layers, *, shake=None, light_=None, arc=None, note=None) -> dict:
    r = {"palette": palette, "layers": flat(*layers)}
    if shake:
        r["shake"] = list(shake)
    if light_:
        r["light"] = light_
    if arc is not None:
        r["arcCm"] = arc
    if note:
        r["note"] = note
    return r


# ════════════════════════════════════════════════════════════════════════════════════════════════════
# combat feedback (§8.5, combat-feel §11)
# ════════════════════════════════════════════════════════════════════════════════════════════════════
def combat() -> dict:
    rec = {}
    # NS_Hit_Impact: core + coloured glow + ground ring + sparks along the blow (big = crit / kill + cross glint)
    rec["hit.impact"] = R("event", [
        impact_star("target", "normal", 1.7, "event", only="small", unit="ring"),
        impact_star("target", "crit", 2.1, "event", only="big", unit="ring", life=180),
        lay(emitter="flash", at="target", height="chest", offsetCm=[6.7, 0, 0], unit="ring", sizeCm=0.5, grow=2.6,
            lifeMs=100, color="white", only="small"),
        lay(emitter="flash", at="target", height="chest", offsetCm=[6.7, 0, 0], unit="ring", sizeCm=0.5, grow=2.6,
            lifeMs=140, color="white", only="big"),
        lay(emitter="glow", at="target", height="chest", unit="ring", sizeCm=1.2, grow=2.17, lifeMs=160,
            alpha=[0.45, 0], color="event", only="small"),
        lay(emitter="glow", at="target", height="chest", unit="ring", sizeCm=1.2, grow=2.17, lifeMs=220,
            alpha=[0.45, 0], color="event", only="big"),
        lay(sprite="Ring", orient="ground", blend="translucent", at="target", height=2, unit="ring",
            sizeCm=round(0.3 / RING_R, 3), grow=4.17, sizePow=3, lifeMs=240, alpha=[0.4, 0], color="event",
            only="small"),
        lay(sprite="Ring", orient="ground", at="target", height=3, unit="ring", sizeCm=round(0.3 / RING_R, 3),
            grow=4.17, sizePow=3, lifeMs=240, alpha=[0.75, 0], color="event", intensity=1.2, only="small"),
        lay(sprite="Ring", orient="ground", blend="translucent", at="target", height=2, unit="ring",
            sizeCm=round(0.3 / RING_R, 3), grow=5.67, sizePow=3, lifeMs=320, alpha=[0.45, 0], color="event",
            only="big"),
        lay(sprite="Ring", orient="ground", at="target", height=3, unit="ring", sizeCm=round(0.3 / RING_R, 3),
            grow=5.67, sizePow=3, lifeMs=320, alpha=[0.85, 0], color="event", intensity=1.2, only="big"),
        lay(emitter="sparks", at="target", height="chest", count="sparks", dir="blow", coneDeg=52,
            speedCmS=[355, 622], lifeMs=[160, 280], drag=4, gravityCmS2=266, altEvery=2, color="event",
            only="small"),
        lay(emitter="sparks", at="target", height="chest", count="sparks", dir="blow", coneDeg=72,
            speedCmS=[488, 932], lifeMs=[160, 280], drag=4, gravityCmS2=266, altEvery=2, color="event",
            only="big"),
        glint("target", 40, life=200, rot=45, only="big"),
    ])
    # per-profile variants (forward compatible: hit.impact.<profile>; the runtime plays hit.impact today)
    for prof, (cell, n) in {"light": ("normal", 4), "normal": ("normal", 6), "heavy": ("heavy", 9),
                            "crit": ("crit", 12), "kill": ("kill", 14)}.items():
        r = json.loads(json.dumps(rec["hit.impact"]))
        big = prof in ("crit", "kill")
        r["layers"] = [l for l in r["layers"] if l.get("only") in (None, "big" if big else "small")]
        for l in r["layers"]:
            l.pop("only", None)
            if l.get("sprite") == "Impact":
                l["variant"] = IMPACT[cell]
        r["note"] = f"hit profile {prof} ({n} sparks): forward-compatible id, hit.impact covers it today"
        rec[f"hit.impact.{prof}"] = r
    # hero basic attack: steel crescent at the target chest + sparks + trail stamp
    rec["hit.basic"] = R("steel", [
        slash_sprite("target", 45, 150, "#AABBD4", cell=SLASH["warrior"], offsetCm=[-13, 0, 0], intensity=1.3),
        slash_sprite("target", 40, 110, "white", cell=SLASH["warrior"], offsetCm=[-13, 0, 0], intensity=1.2,
                     alpha=0.85),
        sparks("target", 5, "#FFF2C0", speed=(90, 170), dir="blow", cone=69, size=0.8, alt_every=0),
        lay(emitter="streak", at="target", height="chest", count=2, sizeCm=60, aspect=0.12, lifeMs=[170, 220],
            orient="billboard", rotationDeg=[-50, -30], alignToBlow=True, color="#FFFFCC", alpha=[0.9, 0]),
    ])
    for cls, mesh, tint, cell in (("warrior", "SM_FX_SlashArc_Warrior", "#FFD98A", "warrior"),
                                  ("mage", "SM_FX_SlashArc_Mage", "#C7A6FF", "mage"),
                                  ("rogue", "SM_FX_SlashArc_Rogue", "#9DFFC8", "rogue")):
        rec[f"hit.basic.{cls}"] = R("steel", [
            ribbon(mesh, "target", 55, 170, tint, rot=-30, spin=300, h="chest", intensity=1.2),
            swipe_glow("target", 55, 150, tint, cell=SLASH[cell], alpha=0.6),
            sparks("target", 5, "#FFF2C0", speed=(90, 170), dir="blow", cone=69, size=0.8, alt_every=0),
        ], note=f"forward-compatible per-class basic swing (class impact colour {tint}); hit.basic is played today")
    rec["hit.claw"] = R("blood", [
        lay(sprite="Claw", orient="billboard", blend="translucent", at="target", height="chest", sizeCm=[80, 80],
            grow=1.2, sizePow=3, lifeMs=220, alpha=[1, 0], rotationDeg=[-20, 20], speedCmS=67, dir="down",
            coneDeg=0, color="#5A0A14"),
        lay(sprite="Claw", orient="billboard", at="target", height="chest", sizeCm=74, grow=1.15, lifeMs=180,
            alpha=[1, 0], rotationDeg=[-20, 20], color="#FF5A3A", intensity=1.4),
        lay(sprite="Claw", orient="billboard", at="target", height="chest", sizeCm=66, grow=1.1, lifeMs=120,
            alpha=[1, 0], color="white", intensity=1.4),
        sparks("target", 5, "#FF6A4A", speed=(60, 130), dir="down", cone=92, size=0.75, alt_every=0),
        glow("target", 40, 160, "#FF3A2A", alpha=0.5, grow=1.6),
    ])
    rec["hit.evade"] = R("holy", [
        glint("target", 34, life=200),
        sparks("target", 6, "#FFE9A0", alt_every=0),
        lay(emitter="streak", at="target", height="chest", count=3, dir="back", coneDeg=40, speedCmS=[200, 300],
            lifeMs=160, sizeCm=26, aspect=0.15, color="#FFF6D0", alpha=[0.8, 0]),
    ])
    # NS_Hit_SkillGroundMark
    rec["hit.mark.fire"] = R("fire", [
        decal("target", "Scorch", px(18) * 0.8, 1800, "#2E1E18", alpha=0.6),
        lay(emitter="glow", orient="ground", at="target", height=4, sizeCm=px(20 * 0.6 * 2), grow=1.4, lifeMs=500,
            alpha=[0.7, 0], color="#FF6A1A"),
    ])
    rec["hit.mark.ice"] = R("frost", [
        decal("target", "Frost", px(20) * 0.9, 1600, "mid", alpha=0.6, blend="additive"),
    ])
    rec["hit.mark.other"] = R("lightning", [
        decal("target", "Scorch", px(20) * 0.7, 1400, "#3A3A54", alpha=0.6),
        decal("target", "CrackGlow", px(20) * 0.8, 360, "#8C8CFF", alpha=0.9, blend="additive", pow=1.5),
    ])
    # NS_Death_Burst (colour = event: monster #FF4444, hero #CC2222, escort #E67E22, soul echo #7FD8FF)
    rec["death.burst"] = R("event", [
        flash("target", 22, "core", life=170),
        smoke("target", 7, "#5A5060", radius_px=12, rise=18, alpha=0.8, h=24),
        motes("target", 8, "event", rise=60, alt="white", alt_every=2, size=(7, 12)),
        ring("target", px(6), px(30), 360, "event", alpha=0.7),
        lay(emitter="glow", sprite="Wisp", orient="billboard", at="target", height="chest", delayMs=80, sizeCm=46,
            grow=1.2, lifeMs=700, upCmS=100, dir="up", coneDeg=0, alpha=[0.85, 0], fadeIn=0.15, color="core",
            intensity=1.2),
    ])
    # NS_Reward_GoldBurst: the coin mesh (gold tint: M_AF_FX_Mesh tints the whole coin)
    rec["reward.gold"] = R("holy", [
        lay(sprite="Coin", mesh="SM_Pickup_GoldCoin", orient="billboard", blend="translucent", at="target",
            height=30, count=6, dir="up", coneDeg=126, speedCmS=[111, 222], upCmS=155, gravityCmS2=933,
            lifeMs=[420, 580], sizeCm=mesh_size("SM_Pickup_GoldCoin", 1.7), grow=0.35, sizePow=0.25, alpha=[1, 1],
            spinDegS=[-540, 540], rotationDeg=[0, 360], color="#FFC93A", intensity=1.1),
        motes("target", 5, "mid", sprite="Spark", rise=30, alt="core", alt_every=2, h=40, size=(8, 12)),
        glow("target", 50, 220, "#FFC93A", h=40, grow=1.6, alpha=0.6),
    ])
    rec["reward.heal"] = R("nature", [
        glow("target", 67, 420, "rim", alpha=0.55, grow=1.4),
        lay(emitter="glow", orient="ground", at="target", height=3, sizeCm=90, grow=1.5, lifeMs=480,
            alpha=[0.55, 0], color="rim"),
        ring("target", px(6), px(28), 420, "mid", alpha=0.85),
        motes("target", 5, "mid", sprite="Plus", radius_px=12, rise=60, size=(20, 28), spin=0, h=60,
              intensity=1.3),
        motes("target", 10, "core", radius_px=12, rise=70, alt="rim", alt_every=2, h=40),
    ])
    rec["reward.mana"] = R("mana", [
        glow("target", 67, 420, "rim", alpha=0.55, grow=1.4),
        lay(emitter="glow", orient="ground", at="target", height=3, sizeCm=90, grow=1.5, lifeMs=480,
            alpha=[0.55, 0], color="rim"),
        ring("target", px(6), px(28), 420, "mid", alpha=0.85),
        motes("target", 12, "core", sprite="Spark", radius_px=12, rise=70, alt="mid", alt_every=2, size=(10, 15),
              h=40),
    ])
    # NS_Hero_LevelUp
    rec["hero.levelup"] = R("holy", [
        decal("target", "Rune", px(40), 1100, "rim", alpha=0.9, blend="additive", grow=1.1, spin=1.0, pow=1.6),
        beam("target", 150, 44, 900, "rim", alpha=0.4, blend="translucent"),
        beam("target", 150, 44, 900, "mid", alpha=0.85, cell=1),
        beam("target", 130, 14, 700, "white", alpha=0.9),
        flash("target", 36, "core", life=200),
        shock("target", px(8), px(70), 480, "mid", alpha=0.6),
        ring("target", px(10), px(80), 520, "rim", delay=120),
        motes("target", 16, "core", sprite="Star", radius_px=20, rise=90, life=(700, 1000), alt="mid", alt_every=2,
              size=(9, 14)),
        motes("target", 10, "mid", radius_px=18, rise=120, life=(600, 900), delay=150, h="ground"),
    ], shake=(100, 0.004))
    return rec


# ════════════════════════════════════════════════════════════════════════════════════════════════════
# hero utility, loot, quests, statuses (§8.7, R8)
# ════════════════════════════════════════════════════════════════════════════════════════════════════
def utility() -> dict:
    rec = {}
    rec["hero.dodge"] = R("steel", [
        lay(emitter="smoke", sprite="Dust", at="origin", height=4, count=5, spawnRadiusCm=25, sizeCm=[26, 38],
            lifeMs=[380, 560], alpha=[0.7, 0], color="#C8B89A", gravityCmS2=-30, speedCmS=[30, 80], dir="back",
            coneDeg=90),
        lay(emitter="streak", at="origin", height="chest", count=4, spawnRadiusCm=20, speedCmS=600, dir="blow",
            coneDeg=6, lifeMs=150, sizeCm=18, color="white", alpha=[0.6, 0]),
    ])
    rec["hero.resonance"] = R("event", [
        flash("target", 36, "event", life=220),
        lay(emitter="glow", orient="ground", at="target", height=3, sizeCm=60, grow=3.5, lifeMs=420,
            alpha=[0.6, 0], color="event"),
        ring("target", 18, 105, 460, "event"),
        shock("target", 15, 105, 380, "event", alpha=0.55),
        motes("target", 12, "event", sprite="Spark", radius_px=18, rise=100, life=(600, 900), alt="white",
              alt_every=2, size=(8, 12)),
    ])
    rec["hero.resonance.aura"] = R("event", [
        lay(emitter="glow", orient="ground", at="target", height=3, rate=1.6, attach=True, sizeCm=120, grow=1.15,
            lifeMs=900, fadeIn=0.3, alpha=[0.35, 0], color="event"),
        lay(emitter="motes", sprite="Spark", at="target", height="chest", rate=7, attach=True, spawnRadiusCm=35,
            spawnHeightJitterCm=50, gravityCmS2=-160, lifeMs=[500, 800], sizeCm=[7, 11], color="event",
            altEvery=3, altColor="white"),
    ])
    rec["hero.dash"] = R("holy", [
        lay(emitter="streak", at="path", height="chest", count=9, spawnRadiusCm=30, speedCmS=930, dir="blow",
            coneDeg=6, lifeMs=180, sizeCm=14, color="mid", altEvery=2, altColor="white"),
        lay(emitter="smoke", sprite="Dust", at="path", height=4, count=5, sizeCm=[26, 36], lifeMs=[380, 560],
            alpha=[0.6, 0], color="#C8B89A", speedCmS=[20, 50], gravityCmS2=-30),
    ])
    rec["teleport.blink"] = R("arcane", [
        lay(emitter="glow", orient="upright", at="origin", height="chest", sizeCm=180, aspect=0.5, grow=0.1,
            lifeMs=200, color="mid"),
        ring("origin", 70, 9, 220, "mid", pow=2),
        flash("point", 30, "core", delay=80),
        sparks("point", 8, "mid", delay=80),
    ])
    # NS_Hero_TownPortal
    rec["portal.channel"] = R("frost", [
        lay(sprite="Ring", orient="ground", at="origin", height=3, rate=2.5, durationMs=1500, attach=True,
            sizeCm=round(px(4) / RING_R, 1), grow=7, sizePow=1.5, lifeMs=1200, alpha=[0.7, 0], color="#4488FF",
            intensity=1.2),
        lay(sprite="Ring", orient="ground", at="origin", height=3, rate=2.5, durationMs=1300, delayMs=200,
            attach=True, sizeCm=round(px(4) / RING_R, 1), grow=5, sizePow=1.5, lifeMs=1200, alpha=[0.5, 0],
            color="#66AAFF", intensity=1.2),
        lay(emitter="glow", orient="ground", at="origin", height=3, rate=1.5, durationMs=1500, attach=True,
            sizeCm=9, grow=15, sizePow=1.5, lifeMs=1200, alpha=[0.3, 0], color="#2266CC"),
        lay(emitter="beam", variant=1, at="origin", height=0, rate=1.2, durationMs=1500, attach=True, sizeCm=210,
            aspect=0.3, grow=1.05, lifeMs=900, fadeIn=0.3, alpha=[0.45, 0], color="#66AAFF"),
        lay(emitter="motes", sprite="Spark", at="origin", rate=14, durationMs=1500, attach=True, spawnRadiusCm=55,
            gravityCmS2=-180, lifeMs=[500, 800], sizeCm=[7, 11], color="#9CC8FF"),
    ])
    rec["portal.arrive"] = R("frost", [
        flash("point", 36, "#B8D8FF", life=220),
        ring("point", 14, 90, 420, "#4488FF"),
        sparks("point", 10, "#66AAFF"),
    ])
    # NS_Loot_Beam_<Quality> (event colour = quality colour)
    rec["loot.drop"] = R("event", [
        lay(emitter="glow", orient="ground", at="point", height=2, sizeCm=60, grow=1.2, lifeMs=1400, fadeIn=0.2,
            alpha=[0.7, 0], color="event"),
        flash("point", 20, "event", life=220, h=20),
        ring("point", px(4), px(24), 420, "event"),
    ])
    rec["loot.drop.rare"] = R("event", [
        lay(emitter="glow", orient="ground", at="point", height=2, sizeCm=72, grow=1.2, lifeMs=1600, fadeIn=0.2,
            alpha=[0.7, 0], color="event"),
        lay(emitter="beam", at="point", height=0, sizeCm=18, aspect=0.2, grow=10, sizePow=4, blend="translucent",
            lifeMs=1400, fadeIn=0.05, alphaPow=2.5, alpha=[0.3, 0], color="event"),
        lay(emitter="beam", variant=1, at="point", height=0, sizeCm=18, aspect=0.16, grow=10, sizePow=4,
            lifeMs=1400, fadeIn=0.05, alphaPow=2.5, alpha=[0.55, 0], color="event", intensity=1.3),
        lay(emitter="beam", at="point", height=0, sizeCm=14, aspect=0.08, grow=10, sizePow=4, lifeMs=1300,
            fadeIn=0.05, alphaPow=2.5, alpha=[0.5, 0], color="white"),
        flash("point", 20, "event", life=220, h=20),
        ring("point", px(4), px(24), 420, "event"),
    ])
    for q, hpx in (("legendary", 110), ("set", 110)):
        rec[f"loot.drop.{q}"] = R("event", [
            lay(emitter="glow", orient="ground", at="point", height=2, sizeCm=96, grow=1.2, lifeMs=1800, fadeIn=0.2,
                alpha=[0.75, 0], color="event"),
            lay(emitter="beam", at="point", height=0, sizeCm=px(hpx) * 0.1, aspect=0.26, grow=10, sizePow=4,
                blend="translucent", lifeMs=1800, fadeIn=0.05, alphaPow=2.5, alpha=[0.4, 0], color="event"),
            lay(emitter="beam", variant=1, at="point", height=0, sizeCm=px(hpx) * 0.1, aspect=0.2, grow=10,
                sizePow=4, lifeMs=1800, fadeIn=0.05, alphaPow=2.5, alpha=[0.75, 0], color="event", intensity=1.4),
            lay(emitter="beam", at="point", height=0, sizeCm=px(hpx) * 0.08, aspect=0.1, grow=10, sizePow=4,
                lifeMs=1600, fadeIn=0.05, alphaPow=2.5, alpha=[0.7, 0], color="white"),
            flash("point", 30, "event", life=220, h=20),
            ring("point", px(4), px(36), 420, "event"),
            motes("point", 10, "event", sprite="Star", radius_px=16, rise=60, life=(700, 1100), alt="white",
                  alt_every=2, size=(8, 12), h=30),
        ])
    # NS_Loot_Beam_<Quality>: the beam STANDS on the bag until pickup (VFXManager.addLootBeam): pool glow (magic+),
    # light pillar + white core (rare+), alpha pulsing ×.55 yoyo every 900 ms (700 legendary / set). Particles
    # cannot loop an alpha curve, so each part is (a) a steady layer — one particle per half period P living 2P with
    # a triangle envelope (fadeIn .5): two overlap at any time and sum to a constant .55·α — plus (b) a pulse layer,
    # one particle per 2P living 2P, triangle 0 → .45·α → 0: together .55·α ↔ α, period 2P. The loot.drop.* burst
    # covers the first P while the steady layer fades in. Played attached to the loot prop (WorldContract 5.3).
    def steady_pulse(base: dict, a: float, P: int, translucent_body: float = 0.0) -> list:
        out = []
        if translucent_body:
            out.append(lay(**base, blend="translucent", rate=round(1000.0 / P, 3), durationMs=-1, attach=True,
                           lifeMs=2 * P, fadeIn=0.5, alphaPow=1, alpha=[round(translucent_body, 3), 0]))
        out.append(lay(**base, rate=round(1000.0 / P, 3), durationMs=-1, attach=True, lifeMs=2 * P, fadeIn=0.5,
                       alphaPow=1, alpha=[round(0.55 * a, 3), 0]))
        out.append(lay(**base, rate=round(1000.0 / (2 * P), 3), durationMs=-1, attach=True, lifeMs=2 * P,
                       fadeIn=0.5, alphaPow=1, alpha=[round(0.45 * a, 3), 0]))
        return out

    # the pool reaches well past the bag (≈ 40 cm wide): smaller pools hid under it and read as a thin rim
    for q, col, pool_cm, pillar_cm, big in (("magic", "#4F8CFF", 120, 0, False), ("rare", "#FFD84A", 128, 180, False),
                                            ("legendary", "#FF8A2A", 156, 280, True),
                                            ("set", "#3ECF6A", 156, 280, True)):
        P = 700 if big else 900
        layers = steady_pulse(dict(sprite="Glow", orient="ground", at="point", height=2, sizeCm=pool_cm, grow=1.0,
                                   color=col, intensity=1.2), 0.75, P, translucent_body=0.5)
        if pillar_cm:
            w = 0.22 if big else 0.2
            layers += steady_pulse(dict(sprite="Beam", variant=1, orient="upright", at="point", height=0,
                                        sizeCm=pillar_cm, aspect=w, grow=1.0, color=col, intensity=1.3),
                                   0.75 if big else 0.55, P, translucent_body=0.22 if big else 0.18)
            layers += steady_pulse(dict(sprite="Beam", orient="upright", at="point", height=0,
                                        sizeCm=round(pillar_cm * 0.8), aspect=round(0.08 / 0.8, 3), grow=1.0,
                                        color="white"), 0.7 if big else 0.5, P)
        if big:
            layers.append(lay(emitter="motes", sprite="Spark", at="point", height=20, rate=2.2, durationMs=-1,
                              attach=True, spawnRadiusCm=18, gravityCmS2=-70, speedCmS=[0, 8], lifeMs=[900, 1300],
                              sizeCm=[7, 10], alpha=[0.9, 0], color=col, altEvery=2, altColor="white",
                              intensity=1.3))
        parts = f"pool + pillar {pillar_cm} cm + core" if pillar_cm else "pool only (web: no pillar below rare)"
        rec[f"loot.beam.{q}"] = R("event", layers, note=f"NS_Loot_Beam_{q.capitalize()}: loop on the loot prop until "
                                                        f"pickup ({parts}), alpha ×.55 yoyo every {P} ms")
    rec["loot.pickup"] = R("holy", [
        glint("point", 18, life=200, h=25),
        sparks("point", 5, "mid", h=25),
    ])
    rec["quest.pop"] = R("holy", [
        lay(emitter="glow", at="point", height=40, sizeCm=45, grow=2.4, lifeMs=260, color="#FFE39A"),
        lay(emitter="sparks", at="point", height=40, count=6, dir="flat", speedCmS=155, color="#FFF1C0",
            altEvery=0),
    ])
    # statuses (DECISIONS R8: small code-driven visuals on top of the StatusTint)
    rec["status.burn"] = R("fire", [
        flames("target", 0, FLAME["fire"], radius_px=9, rise=(25, 45), life=(260, 420), size=(0.4, 0.6),
               rate=9, attach=True, h="chest", spawnHeightJitterCm=40),
        lay(emitter="motes", at="target", height="chest", rate=5, attach=True, spawnRadiusCm=22,
            gravityCmS2=-160, sizeCm=[5, 9], color="rim"),
        lay(emitter="glow", orient="ground", at="target", height=3, rate=1.3, attach=True, sizeCm=80, grow=1.15,
            lifeMs=900, fadeIn=0.3, alpha=[0.3, 0], color="#FF7A3A"),
    ], note="R8 burn embers: 2–3 small cel flames + embers + a warm ground glow; StatusTint #FF7A3A α .35")
    rec["status.poison"] = R("poison", [
        lay(emitter="motes", sprite="Bubble", at="target", height="chest", rate=7, attach=True, spawnRadiusCm=24,
            spawnHeightJitterCm=40, gravityCmS2=-90, speedCmS=[10, 30], sizeCm=[12, 18], lifeMs=[600, 900],
            color="#B6F060", intensity=1.4, spinDegS=0),
        lay(emitter="glow", orient="ground", at="target", height=3, rate=1.3, attach=True, sizeCm=80, grow=1.15,
            lifeMs=900, fadeIn=0.3, alpha=[0.35, 0], color="#6AC83A"),
        lay(sprite="Drop", orient="billboard", blend="translucent", at="target", height="chest", rate=1.6,
            attach=True, spawnRadiusCm=16, gravityCmS2=500, sizeCm=[5, 7], aspect=0.67, grow=1, lifeMs=[350, 500],
            alpha=[1, 0.4], color="#5FC03A"),
    ], note="R8 poison bubbles; StatusTint #8FE04A α .35")
    rec["status.freeze"] = R("frost", [
        lay(mesh="SM_FX_FrostCrust", sprite="ShardFrost", orient="ground", blend="translucent", at="target",
            height=0, rate=1.0, attach=True, sizeCm=mesh_size("SM_FX_FrostCrust"), grow=1, lifeMs=1050,
            fadeIn=0.08, alpha=[1, 1], color="#BFEFFF", intensity=1.1),
        lay(emitter="motes", sprite="Flake", at="target", height="chest", rate=5, attach=True, spawnRadiusCm=35,
            spawnHeightJitterCm=50, gravityCmS2=40, speedCmS=[5, 20], sizeCm=[8, 12], lifeMs=[700, 1100],
            color="core"),
        lay(emitter="glow", orient="ground", at="target", height=3, rate=1.2, attach=True, sizeCm=110, grow=1.1,
            lifeMs=900, fadeIn=0.3, alpha=[0.35, 0], color="#7FD0FF"),
        # a frosted veil over the body (camera-ward, translucent pale ice): the StatusTint multiply alone keeps a
        # green goblin green — the web hue-shifted it to ice blue
        lay(sprite="Glow", orient="billboard", blend="translucent", at="target", height="chest", rate=1.2,
            attach=True, sizeCm=96, grow=1.05, lifeMs=1000, fadeIn=0.35, alpha=[0.34, 0], color="#D6F4FF"),
    ], note="R8 frost crystals at the feet (SM_FX_FrostCrust) + a frosted veil; StatusTint #7FD0FF α .45")
    star = dict(mesh="SM_FX_StunStar", sprite="Star", orient="upright", blend="translucent", at="target",
                height="overhead", attach=True, orbit={"r0Cm": 24, "r1Cm": 24, "degS": 240},
                sizeCm=mesh_size("SM_FX_StunStar", 0.9), grow=1, fadeIn=0.15, alpha=[1, 0], alphaPow=8,
                rotationDeg=[-20, 20], color="#FFE27A", intensity=1.25)
    rec["status.stun"] = R("holy", [
        lay(**star, rate=3, lifeMs=1000),
        # the stun reads from its first frame: two stars already circling (staggered lives), not 333 ms of nothing
        # before the 3 /s loop's first star
        lay(**star, count=2, lifeMs=[600, 1000]),
        lay(emitter="motes", sprite="Spark", at="target", height="overhead", rate=4, attach=True,
            spawnRadiusCm=26, gravityCmS2=0, speedCmS=[5, 15], sizeCm=[6, 9], lifeMs=[300, 500], color="core"),
    ], note="R8 stun stars: toon stars orbiting fx_overhead, facing the W1 camera")
    rec["status.slow"] = R("frost", [
        lay(sprite="Swirl", orient="ground", at="target", height=2, rate=1.2, attach=True, sizeCm=110, grow=1.05,
            lifeMs=900, fadeIn=0.3, alpha=[0.45, 0], spinDegS=-140, rotationDeg=[0, 360], color="mid"),
    ], note="optional R8 slow visual: a frost swirl at the feet")
    rec["status.bleed"] = R("blood", [
        # a held open wound on the body (a dark crimson cut + a wet glint, camera-ward), fat dark-red drops running
        # off it and spatters spreading round the feet past the silhouette (the web has no bleed visual; the first
        # pass — small drops at the chest — vanished into the body)
        *held(dict(sprite="Slash", variant=SLASH["rogue"], orient="billboard", blend="translucent", at="target",
                   height="chest", offsetCm=[0, 4, 4], sizeCm=44, aspect=0.5, rotationDeg=-35, grow=1.0,
                   color="#7A0614"), 0.95, 450, pulse=0.25),
        *held(dict(sprite="Slash", variant=SLASH["rogue"], orient="billboard", at="target", height="chest",
                   offsetCm=[0, 4, 5], sizeCm=36, aspect=0.5, rotationDeg=-35, grow=1.0, color="#FF5A5A",
                   intensity=1.1), 0.45, 450),
        # fat inked drops (22–30 cm) flicked a little sideways off the wound so they fall clear of the silhouette —
        # 18–24 cm drops falling straight down the body read as red smudges on the goblin at 1:1
        lay(sprite="Drop", orient="billboard", blend="translucent", at="target", height="chest", rate=7,
            attach=True, offsetCm=[0, 4, -4], spawnRadiusCm=18, spawnHeightJitterCm=16, dir="flat",
            speedCmS=[25, 55], gravityCmS2=480, sizeCm=[22, 30], aspect=0.67, grow=1, lifeMs=[420, 560],
            alpha=[1, 0.8], color="mid"),
        lay(sprite="Drop", orient="billboard", at="target", height="chest", rate=7, attach=True, offsetCm=[0, 4, -4],
            spawnRadiusCm=18, spawnHeightJitterCm=16, dir="flat", speedCmS=[25, 55], gravityCmS2=480,
            sizeCm=[9, 12], aspect=0.67, grow=1, lifeMs=[420, 560], alpha=[0.55, 0], color="#FF8070"),
        # crimson spatters round the feet (held near opaque, a bright tint: the cel Puddle's value bands average .69 and
        # the Drop's .43, so the palette rim #A01420 came out maroon-brown over tan dirt)
        lay(emitter="decal", sprite="Puddle", at="target", rate=1.6, attach=True, spawnRadiusCm=30, sizeCm=[34, 46],
            grow=1.25, lifeMs=1600, alpha=[1.0, 0], alphaPow=3.5, color="#D41A2C"),
    ], note="optional R8 bleed visual: a held wound, blood drips + spatters round the feet; StatusTint #C8323C α .22")
    return rec


# ════════════════════════════════════════════════════════════════════════════════════════════════════
# world (§8.7, world-map-nav §14)
# ════════════════════════════════════════════════════════════════════════════════════════════════════
def world() -> dict:
    rec = {}
    # NS_Camp_Flame + NS_Camp_Sparks: the camp flame cell (CampProps drawFlame, #FF8800) + inner tongues
    rec["camp.campfire"] = R("fire", [
        # on the log pile: SM_Prop_Camp_Campfire socket `flame` = 12 cm
        flames("origin", 0, FLAME["camp"], radius_px=4, rise=(18, 34), life=(420, 620), size=(1.0, 1.25),
               rate=10, h=10),
        flames("origin", 0, FLAME["fire"], radius_px=3, rise=(25, 45), life=(300, 460), size=(0.5, 0.72),
               rate=9, h=14),
        lay(emitter="sparks", at="origin", height=30, rate=5.5, durationMs=-1, count=1, dir="up", coneDeg=50,
            speedCmS=[33, 100], gravityCmS2=-40, drag=0.5, lifeMs=[400, 900], sizeCm=[6, 9], color="#FFDD44",
            altEvery=2, altColor="#FF8800"),
        lay(emitter="glow", at="origin", height=40, rate=2.5, durationMs=-1, sizeCm=90, grow=1.15, lifeMs=600,
            fadeIn=0.3, alpha=[0.45, 0], color="#FF8800"),
    ], light_=light(300, 0.4))
    rec["camp.torch"] = R("fire", [
        # in the torch cup: SM_Prop_Camp_Torch socket `flame` = 170 cm
        flames("origin", 0, FLAME["camp"], radius_px=2, rise=(15, 30), life=(300, 460), size=(0.6, 0.78), rate=9,
               h=166),
        lay(emitter="motes", at="origin", height=195, rate=2, durationMs=-1, spawnRadiusCm=6, gravityCmS2=-120,
            lifeMs=[500, 900], sizeCm=[5, 8], color="#FFB060"),
        lay(emitter="glow", at="origin", height=182, rate=2, durationMs=-1, sizeCm=60, grow=1.15, lifeMs=600,
            fadeIn=0.3, alpha=[0.4, 0], color="#FF6600"),
    ], light_=light(180, 0.35))
    rec["camp.hearthstone"] = R("fire", [
        lay(sprite="Rune", orient="ground", at="origin", height=2, rate=1.25, durationMs=-1, sizeCm=110, grow=1.06,
            lifeMs=800, fadeIn=0.35, alpha=[0.5, 0], color="#FF8A3A", spinDegS=20),
        lay(emitter="glow", orient="ground", at="origin", height=2, rate=0.9, durationMs=-1, sizeCm=120, grow=1.1,
            lifeMs=1100, fadeIn=0.45, alpha=[0.55, 0.25], color="#FF8A3A"),
        lay(emitter="motes", at="origin", height=60, rate=1.25, durationMs=-1, spawnRadiusCm=10, gravityCmS2=-80,
            lifeMs=[800, 1200], sizeCm=[6, 9], color="#FFB060"),
    ], note="NS_World_Hearthstone (forward-compatible id: the world builder can play it at camp hearthstones)")
    # the exit vortex is the prop's own toon mesh (deep teal interior, bright swirl strokes): the recipe adds only
    # the ground ward (ring + rune stroke at the ring radius, translucent body + additive), rising motes and a small
    # core glint — a big additive upright glow blew the centre out to white and its pool spilled 2.5 m of mint haze
    rec["exit.portal"] = R("portal", [
        lay(sprite="Ring", orient="ground", blend="translucent", at="origin", height=2, rate=1.0, durationMs=-1,
            sizeCm=250, grow=1.04, sizePow=1, lifeMs=1600, fadeIn=0.3, alpha=[0.6, 0.15], color="#14584A"),
        lay(sprite="Ring", orient="ground", at="origin", height=3, rate=1.0, durationMs=-1, sizeCm=250, grow=1.04,
            sizePow=1, lifeMs=1600, fadeIn=0.3, alpha=[0.55, 0], color="#3FE0A0", intensity=1.1),
        lay(sprite="Rune", orient="ground", blend="translucent", at="origin", height=2, rate=0.7, durationMs=-1,
            sizeCm=226, grow=1.0, lifeMs=2000, fadeIn=0.35, alpha=[0.55, 0], spinDegS=14, rotationDeg=[0, 360],
            color="#1E6E5C"),
        lay(sprite="Rune", orient="ground", at="origin", height=3, rate=0.7, durationMs=-1, sizeCm=226, grow=1.0,
            lifeMs=2000, fadeIn=0.35, alpha=[0.45, 0], spinDegS=14, rotationDeg=[0, 360], color="#80F0E0"),
        lay(emitter="motes", sprite="Spark", at="origin", height=20, rate=6, durationMs=-1, spawnRadiusCm=95,
            onRing=True, gravityCmS2=-120, speedCmS=[0, 8], lifeMs=[900, 1400], sizeCm=[7, 11],
            color="#B0FFE0"),
    ], light_=light(120, 0.3, False))
    rec["exit.sealed"] = R("blood", [
        # W7 sealed gate: the crimson seal is the prop's membrane (#6A2230 at α .75); the recipe draws the ward on
        # the ground (dark stroke + red rune glow) and slow embers sinking back into it
        lay(sprite="Ring", orient="ground", blend="translucent", at="origin", height=2, rate=0.8, durationMs=-1,
            sizeCm=250, grow=1.0, sizePow=1, lifeMs=2000, fadeIn=0.4, alpha=[0.6, 0.15], color="#4A0E1A"),
        lay(sprite="Rune", orient="ground", blend="translucent", at="origin", height=2, rate=0.8, durationMs=-1,
            sizeCm=226, grow=1, lifeMs=2000, fadeIn=0.4, alpha=[0.6, 0], color="#4A0E1A"),
        lay(sprite="Rune", orient="ground", at="origin", height=3, rate=0.8, durationMs=-1, sizeCm=226, grow=1,
            lifeMs=2000, fadeIn=0.4, alpha=[0.6, 0], color="#E0405A", intensity=1.1),
        lay(emitter="motes", sprite="Ember", at="origin", height=150, rate=2.5, durationMs=-1, spawnRadiusCm=80,
            spawnHeightJitterCm=120, gravityCmS2=30, speedCmS=[0, 6], lifeMs=[1200, 1800], sizeCm=[6, 9],
            alpha=[0.8, 0], color="#E0405A"),
    ])
    rec["prop.lore"] = R("holy", [
        lay(emitter="glow", orient="ground", at="origin", height=2, rate=1.3, durationMs=-1, attach=True,
            sizeCm=100, grow=1.2, lifeMs=1500, fadeIn=0.45, alpha=[0.6, 0], color="event"),
        lay(emitter="motes", sprite="Spark", at="origin", height=30, rate=2, durationMs=-1, attach=True,
            spawnRadiusCm=30, gravityCmS2=-60, lifeMs=[700, 1100], sizeCm=[6, 9], color="event"),
    ])
    rec["prop.gather"] = R("holy", [
        lay(emitter="glow", orient="ground", at="origin", height=2, rate=1.4, durationMs=-1, attach=True,
            sizeCm=70, grow=1.25, lifeMs=900, fadeIn=0.4, alpha=[0.75, 0], color="#FFD98A"),
        lay(emitter="motes", sprite="Spark", at="origin", height=25, rate=1.5, durationMs=-1, attach=True,
            spawnRadiusCm=20, gravityCmS2=-50, lifeMs=[600, 900], sizeCm=[5, 8], color="#FFF1C0"),
    ])
    rec["prop.clue"] = R("frost", [
        lay(sprite="Ring", orient="ground", at="origin", height=3, rate=0.9, durationMs=-1, attach=True,
            sizeCm=70, grow=1.3, sizePow=1, lifeMs=1100, alpha=[0.7, 0.2], color="#8FE0FF", intensity=1.2),
    ])
    # the glow ring under the hero's ghost STAYS (ZoneScene.ts:1248-1286: α .6 ↔ .25 + widen ×1.15, 1400 ms yoyo):
    # a steady ring at .25 + a widening pulse to .6 (one-shot rings at .7 /s left it empty half the time)
    echo_ring = held(dict(sprite="Ring", orient="ground", at="origin", height=3, sizeCm=90, grow=1.0, sizePow=1,
                          color="#7FD8FF"), 0.6, 700, pulse=0.58)
    echo_ring[1]["grow"] = 1.15
    rec["prop.soul_echo"] = R("frost", [
        *echo_ring,
        lay(emitter="motes", sprite="Wisp", at="origin", height="chest", rate=3, durationMs=-1, attach=True,
            spawnRadiusCm=25, gravityCmS2=-80, sizeCm=[10, 16], spinDegS=0, color="#9FE6FF"),
    ])
    rec["prop.chest"] = R("holy", [
        glint("origin", 12, life=400, h=45, rate=1, durationMs=-1, attach=True, spawnRadiusCm=20,
              color="#FFE080"),
    ])
    # played when the chest opens (the world builder starts the Open clip, 400 ms): every layer waits for the
    # ChestOpen notify (120 ms) — the lid lifts over the next 280 ms, so the shaft rises out of the opening (fx_open,
    # 35 cm) as it widens; the warm glow fades in instead of flashing (a 0.9 additive pop bloomed the whole chest
    # into a white blob)
    OPEN = 120
    rec["prop.chest.open"] = R("holy", [
        beam("origin", 60, 26, 900, "#FFE9A0", alpha=0.3, fade_in=0.3, cell=1, h=36, blend="translucent",
             delay=OPEN),
        beam("origin", 60, 26, 900, "#FFF0B0", alpha=0.5, fade_in=0.3, cell=1, h=36, delay=OPEN),
        glow("origin", 80, 600, "#FFD870", h=44, grow=1.35, alpha=0.4, fadeIn=0.3, delay=OPEN),
        motes("origin", 10, "core", sprite="Star", rise=70, h=46, size=(8, 12), delay=OPEN + 80),
    ], note="chest opened (picked up / collected): timed from the Open clip's start, ChestOpen notify at 120 ms")
    rec["prop.event"] = R("frost", [
        lay(emitter="glow", at="origin", height=110, rate=1.5, durationMs=-1, attach=True, sizeCm=80, grow=1.2,
            lifeMs=1000, fadeIn=0.4, alpha=[0.5, 0], color="#72D8FF"),
    ])
    # camera-attached ambient volumes (WeatherSystem.ts:15-30; emerald_plains = pollen + dust). The volume is a 10 m
    # disc round the camera focus (about half of it on screen at the W1 distance), rates scaled so the screen holds
    # the web's count (pollen ≈ 26 motes, dust ≈ 11). Over the bright plains an additive pale mote vanishes and a soft
    # Glow dot at α ≤ .6 fading linearly over 8–14 s was invisible at 1:1, so pollen / dust / dust motes are crisp
    # translucent specks (the Core sprite's disc, 5–8 cm) that hold their alpha for most of the life (alphaPow 2.5);
    # spawn heights stay above the ground (10–210 cm).
    for amb, kw in (
        ("pollen", dict(sprite="Core", rate=4.7, spawnHeightJitterCm=200, height=110, speedCmS=[2, 11],
                        lifeMs=[8000, 14000], sizeCm=[14, 19], grow=1.25, alpha=[0.9, 0], alphaPow=2.5,
                        blend="translucent", color="#FFF2A0")),
        # dust: the web's every-800 ms layer (≈ 13 on screen); at 2.4 /s and α .55 a quarter-screen crop held 0–1
        # visible mote, so 3.2 /s (≈ 17 on screen) and α .7
        ("dust", dict(sprite="Core", rate=3.2, spawnHeightJitterCm=120, height=70, speedCmS=[4, 18], dir="up",
                      coneDeg=140, lifeMs=[6000, 12000], sizeCm=[16, 22], grow=1.4, alpha=[0.7, 0], alphaPow=2.5,
                      blend="translucent", color="#E4F2CC")),
        ("wisps", dict(rate=2.2, spawnHeightJitterCm=200, height=110, speedCmS=[8, 20], gravityCmS2=-4,
                       lifeMs=[5000, 9000], sizeCm=[8, 14], grow=1.3, alpha=[0.5, 0], color="#9ACBFF",
                       intensity=1.3)),
        ("dust_motes", dict(sprite="Core", rate=4.7, spawnHeightJitterCm=200, height=110, speedCmS=[2, 11],
                            lifeMs=[8000, 14000], sizeCm=[14, 19], grow=1.25, alpha=[0.75, 0], alphaPow=2.5,
                            blend="translucent", color="#EEF4FA")),
        ("sparks", dict(sprite="Ember", rate=5, spawnHeightJitterCm=180, height=100, speedCmS=[10, 30], dir="up",
                        coneDeg=90, gravityCmS2=-20, lifeMs=[3000, 6000], sizeCm=[5, 9], alpha=[0.7, 0],
                        color="#FF6A3A", intensity=1.5)),
    ):
        base = dict(emitter="motes", sprite="Glow", at="camera", durationMs=-1, spawnRadiusCm=1000, gravityCmS2=0,
                    drag=0, fadeIn=0.15, intensity=1.0, spinDegS=0)
        base.update(kw)
        rec[f"ambient.{amb}"] = R("event" if amb != "sparks" else "fire", [clean(base)])
    # NS_NPC_AnvilSparks at the anvil stump's `strike` socket. PlayNotify passes the smith's actor (feet) and its
    # forward as the blow, so the socket is an offset in the actor frame: the stump stands at mesh (12, 52) cm
    # (12 cm to the smith's left, 52 forward; SK_NPC_Blacksmith scenery), its `strike` socket at (0, -3.19, 54.63)
    # → forward 48.8, right -12, up 54.6 (+3 cm over the hot bar)
    strike = [48.8, -12.0, 57.6]
    # the flash sits over the hammer head (head centre ≈ strike + 4 cm, top ≈ +12 cm): centred lower, the hammer and
    # tongs hid it and the strike read as a few specks
    lift = [48.8, -12.0, 76.0]
    rec["npc.FX_HammerStrike"] = R("fire", [
        lay(emitter="glow", at="origin", height="ground", offsetCm=lift, sizeCm=66, grow=1.2, lifeMs=180,
            alpha=[0.9, 0], fadeIn=0.0, alphaPow=1.4, color="#FFA040", intensity=1.5),
        *impact_star("origin", "normal", 40, "#FFD070", life=100, h="ground", offsetCm=lift),
        lay(emitter="flash", at="origin", height="ground", offsetCm=lift, sizeCm=18, grow=1.8, lifeMs=110,
            alpha=[0.95, 0], color="#FFE6A0"),
        # 6 spark dots (web r 1.2 u ≈ 4 cm) in an upward fan (−90° ± 65°), 10–22 cm out (NpcKit.ts:343-352)
        lay(emitter="motes", sprite="Ember", at="origin", height="ground", offsetCm=strike, count=7, dir="up",
            coneDeg=130, speedCmS=[170, 300], gravityCmS2=420, drag=3, lifeMs=[180, 280], sizeCm=[8, 12],
            spinDegS=0, alpha=[1, 0], alphaPow=1.5, color="#FFD070", altEvery=3, altColor="white",
            intensity=1.6),
        lay(emitter="sparks", at="origin", height="ground", offsetCm=strike, count=4, dir="up", coneDeg=110,
            speedCmS=[220, 340], gravityCmS2=500, drag=3, lifeMs=[120, 200], sizeCm=[9, 13], aspect=0.3,
            stretch=0.03, color="#FFD070", altEvery=0, intensity=1.5),
    ], light_=light(90, 0.55, False),
        note="NS_NPC_AnvilSparks: Work_Hammer notify FX_HammerStrike (426.6 ms) at SM_Prop_Camp_AnvilStump `strike`")
    # elite affix auras (§8.7, forward-compatible ids elite.<affix>): the spec's aura ellipse — fill α .25 + stroke
    # α .6 in the affix colour, pulsing scale 1 → 1.2, α .25 → .1 over 1200 ms. Additive alone turns every red into
    # the same orange on dirt and greys purple / blue on grass, so the body is TRANSLUCENT (the Shock disc = a thin
    # fill + a crisp rim: true hue on any ground) and only a faint additive rim glows on top.
    def aura(col, rim, *, body=None, body_alpha=(0.72, 0.28), rim_alpha=(0.5, 0.08)):
        """``body`` (default ``col``) is the translucent Shock disc's tint: a dark affix hue needs a deeper body at
        a higher alpha to hold its hue over the tan dirt (vampiric #CC0000 at α .72 came out the fire ring's
        orange, only darker)."""
        return [
            lay(sprite="Shock", orient="ground", blend="translucent", at="target", height=2, rate=0.85, attach=True,
                sizeCm=120, grow=1.2, sizePow=1, lifeMs=1200, fadeIn=0.25, alpha=list(body_alpha),
                color=body or col),
            lay(sprite="Puff", orient="ground", blend="translucent", at="target", height=1.5, rate=0.85,
                attach=True, sizeCm=118, grow=1.2, sizePow=1, lifeMs=1200, fadeIn=0.25, alpha=[0.3, 0.1],
                color=col),
            lay(sprite="Ring", orient="ground", at="target", height=3, rate=0.85, attach=True,
                sizeCm=round(60 / RING_R), grow=1.2, sizePow=1, lifeMs=1200, fadeIn=0.25, alpha=list(rim_alpha),
                color=rim, intensity=1.1),
        ]

    def glints(col, lit, rate, size, life, radius):
        """an arcane / ice shimmer: a translucent star in the true hue under the additive glint."""
        return [
            lay(emitter="glint", blend="translucent", at="target", height="chest", rate=rate, attach=True,
                spawnRadiusCm=radius, spawnHeightJitterCm=40, sizeCm=size, lifeMs=life, alpha=[0.9, 0],
                color=col, intensity=1.0),
            lay(emitter="glint", at="target", height="chest", rate=rate, attach=True, spawnRadiusCm=radius,
                spawnHeightJitterCm=40, sizeCm=round(size * 0.8, 1), lifeMs=life, color=lit, intensity=1.3),
        ]
    AURA_KW = {
        # blood, not fire: a deep crimson body held near opaque (a ring of drying blood) + a thin cool-red rim
        "vampiric": dict(body="#A80A24", body_alpha=(0.92, 0.45), rim_alpha=(0.42, 0.06)),
    }
    for affix, col, rim, extra in (
        # a fire orb at mid-body rising 8 px (800 ms): a solid orange ball with a hot core + a cel flame tongue
        # the web's fire orb (r 6 px ≈ 27 cm, alpha .4 ↔ .1 yoyo 800 ms) STAYS over the elite: a held crisp 28 cm
        # ball (the Core sprite's disc, translucent in the affix red-orange) + a hot additive core + halo (`held`),
        # a cel flame tongue licking up from it (a soft Puff read as a faint glow, not an orb)
        ("fire_enhanced", "#FF4400", "#FF7A3A", [
            *held(dict(sprite="Core", orient="billboard", blend="translucent", at="target", height="chest",
                       offsetCm=[0, 28, 12], sizeCm=76, grow=1.0, color="#E8400E"), 0.95, 400, pulse=0.25),
            *held(dict(sprite="Glow", orient="billboard", at="target", height="chest", offsetCm=[0, 28, 12],
                       sizeCm=84, grow=1.0, color="#FF4400", intensity=1.2), 0.45, 400),
            *held(dict(sprite="Core", orient="billboard", at="target", height="chest", offsetCm=[0, 28, 13],
                       sizeCm=40, grow=1.0, color="#FFC060", intensity=1.3), 0.85, 400, pulse=0.3),
            flames("target", 0, FLAME["fire"], radius_px=0, rise=(10, 10), life=(560, 640), size=(0.34, 0.38),
                   rate=2.5, attach=True, h="chest", offsetCm=[0, 28, 22], alphaPow=1.6, grow=0.8),
        ]),
        ("swift", "#44FF88", "#8AFFB4", [
            lay(emitter="streak", blend="translucent", at="target", height="chest", rate=1.7, attach=True,
                spawnHeightJitterCm=40, dir="back", coneDeg=10, speedCmS=200, lifeMs=600, sizeCm=44, aspect=0.16,
                color="#1E9A4E", alpha=[0.8, 0]),
            lay(emitter="streak", at="target", height="chest", rate=1.7, attach=True, spawnHeightJitterCm=40,
                dir="back", coneDeg=10, speedCmS=200, lifeMs=600, sizeCm=40, color="#44FF88", alpha=[0.7, 0]),
        ]),
        ("teleporting", "#AA44FF", "#C890FF", glints("#AA44FF", "#E0C0FF", 2, 30, 1000, 30)),
        # red power pulse ×1.8 (700 ms): rim-only (a hollow ring round the body) — a filled additive glow turned
        # the green goblin khaki
        ("extra_strong", "#FF2222", "#FF6A5A", [
            lay(sprite="Ring", orient="billboard", blend="translucent", at="target", height="chest", rate=1.4,
                attach=True, sizeCm=118, grow=1.45, sizePow=1.5, lifeMs=700, fadeIn=0.15, alpha=[0.7, 0],
                alphaPow=1.2, color="#E01818"),
            lay(sprite="Ring", orient="billboard", at="target", height="chest", rate=1.4, attach=True,
                sizeCm=118, grow=1.45, sizePow=1.5, lifeMs=700, fadeIn=0.15, alpha=[0.45, 0], color="#FF4A3A",
                intensity=1.2),
        ]),
        ("curse_aura", "#8844AA", "#B07CD8", [
            lay(sprite="Puff", orient="ground", blend="translucent", at="target", height=1, rate=0.7, attach=True,
                sizeCm=240, grow=1.3, lifeMs=1500, alpha=[0.24, 0], fadeIn=0.4, color="#6A2A90"),
            lay(emitter="smoke", sprite="Smoke", at="target", height=10, rate=1.4, attach=True, spawnRadiusCm=70,
                onRing=True, dir="out", speedCmS=[6, 14], gravityCmS2=-12, sizeCm=[34, 46], grow=1.4,
                lifeMs=[1300, 1700], alpha=[0.32, 0], fadeIn=0.3, color="#5A2A78"),
        ]),
        # a blood orb dripping (web r 5 px, alpha .4 ↔ .15 yoyo 1000 ms): a held cel blood bead (the inked Drop
        # sprite, 48 cm; a flat dark-red disc turned brown on dirt, 36 cm was a speck at 1:1) beside the chest with a wet
        # highlight, fat drops falling from it to the ground
        ("vampiric", "#CC0000", "#FF2A48", [
            *held(dict(sprite="Drop", orient="billboard", blend="translucent", at="target", height="chest",
                       offsetCm=[0, 30, 14], sizeCm=48, aspect=0.67, grow=1.0, color="#E8182A"), 1.0, 500,
                  pulse=0.15),
            *held(dict(sprite="Glow", orient="billboard", at="target", height="chest", offsetCm=[0, 30, 14],
                       sizeCm=80, grow=1.0, color="#CC0000", intensity=1.0), 0.16, 500),
            *held(dict(sprite="Core", orient="billboard", at="target", height="chest", offsetCm=[-8, 23, 10],
                       sizeCm=14, grow=1.0, color="#FF9A8A", intensity=1.0), 0.7, 500, pulse=0.2),
            lay(sprite="Drop", orient="billboard", blend="translucent", at="target", height="chest", rate=2.6,
                attach=True, offsetCm=[0, 30, 0], spawnRadiusCm=6, gravityCmS2=360, sizeCm=[20, 27], aspect=0.67,
                grow=1, lifeMs=[620, 780], alpha=[1, 0.35], alphaPow=1, fadeIn=0.08, color="#A00012"),
        ]),
        ("frozen", "#4488FF", "#9CC8FF", glints("#3A78F0", "#CFE4FF", 1.1, 40, 900, 24)),
    ):
        rec[f"elite.{affix}"] = R("event", [*aura(col, rim, **AURA_KW.get(affix, {})), *extra],
                                  note=f"NS_Elite_Aura_{affix} (forward-compatible id; aura colour {col})")
    return rec


# ════════════════════════════════════════════════════════════════════════════════════════════════════
# projectiles and ground effects
# ════════════════════════════════════════════════════════════════════════════════════════════════════
def head_mesh(mesh, color, life, *, size=1.0, intensity=1.0, rate=None):
    """A flight-aligned mesh head on the projectile anchor. One particle per projectile (``count`` 1, ``life`` =
    the longest flight) keeps a stable orientation: ``dir: blow`` re-rolls a random pitch per particle."""
    d = dict(mesh=mesh, sprite="Arrow", orient="velocity", blend="translucent", at="origin", attach=True,
             dir="blow", coneDeg=0, speedCmS=1, sizeCm=mesh_size(mesh, size), grow=1, lifeMs=life, alpha=[1, 1],
             color=color, intensity=intensity)
    if rate:
        d.update(rate=rate, durationMs=-1)
    else:
        d.update(count=1)
    return clean(d)


def comet_tail(color, length_cm, aspect, *, rate=60):
    """A flight-aligned Comet streak behind a head: translucent body (reads over the bright plains) + additive
    copy; one per frame on the attached anchor, short-lived so it trails the head."""
    base = dict(sprite="Comet", orient="velocity", at="origin", rate=rate, durationMs=-1, attach=True, dir="blow",
                coneDeg=0, speedCmS=1, sizeCm=length_cm, aspect=aspect, grow=0.85, lifeMs=70, color=color,
                offsetCm=[round(-length_cm * 0.38, 1), 0, 0])
    return [lay(**base, blend="translucent", alpha=[0.7, 0]), lay(**base, alpha=[0.9, 0], intensity=1.4)]


def projectiles() -> dict:
    rec = {}
    rec["proj.default"] = R("event", [
        lay(emitter="glow", at="origin", rate=60, durationMs=-1, attach=True, sizeCm=44, grow=0.6, lifeMs=90,
            alpha=[0.9, 0], color="rim"),
        lay(sprite="Comet", orient="velocity", at="origin", rate=60, durationMs=-1, attach=True, dir="blow",
            coneDeg=0, speedCmS=1, sizeCm=34, aspect=0.33, grow=0.8, lifeMs=80, alpha=[0.9, 0], color="mid",
            intensity=1.4),
        lay(emitter="motes", at="origin", rate=30, durationMs=-1, speedCmS=[10, 40], gravityCmS2=-60,
            lifeMs=[200, 380], sizeCm=[5, 9], color="rim"),
    ])
    rec["proj.default.hit"] = R("event", [
        flash("point", 26, "core", life=170),
        sparks("point", 8, "mid"),
        ring("point", px(4), px(22), 320, "rim"),
    ])
    rec["proj.default.fizzle"] = R("event", [glow("point", 40, 200, "mid", alpha=0.6, grow=1.5)])
    # generic skill fallback (SkillEffectSystem.ts:1087-1092): flash / sparks / ring in #F39C12 at the target
    rec["skill.default"] = R("holy", [
        flash("point", 24, "#FFF1C4", glow_color="#F39C12", life=170),
        sparks("point", 8, "#F39C12", speed=(110, 220)),
        ring("point", px(6), px(30), 340, "#F39C12", alpha=0.85),
    ])
    rec["proj.monster_bolt"] = R("event", [
        head_mesh("SM_FX_SpiritBolt", "event", 34, size=1.0, intensity=1.3, rate=60),
        *comet_tail("event", 62, 0.3),
        lay(emitter="glow", at="origin", rate=60, durationMs=-1, attach=True, sizeCm=48, grow=0.6, lifeMs=90,
            alpha=[0.8, 0], color="event"),
        lay(emitter="motes", at="origin", rate=36, durationMs=-1, speedCmS=[10, 40], gravityCmS2=-60,
            lifeMs=[220, 380], sizeCm=[5, 9], color="event"),
    ])
    rec["proj.monster_bolt.hit"] = R("event", [
        flash("point", 28, "core", life=180),
        impact_star("point", "normal", 64, "event", life=160),
        glow("point", 56, 220, "event", alpha=0.45, grow=1.5, blend="translucent"),
        sparks("point", 7, "event"),
        ring("point", px(4), px(20), 300, "event"),
        smoke("point", 2, "#4A3A36", h=40, size=(0.3, 0.45)),
    ])
    rec["proj.pet_bolt"] = R("arcane", [
        head_mesh("SM_FX_SpiritBolt", "event", 34, size=0.85, intensity=1.3, rate=60),
        *comet_tail("event", 50, 0.3),
        lay(emitter="glow", at="origin", rate=60, durationMs=-1, attach=True, sizeCm=34, grow=0.6, lifeMs=90,
            alpha=[0.8, 0], color="event"),
        lay(emitter="motes", sprite="Spark", at="origin", rate=24, durationMs=-1, speedCmS=[5, 25],
            gravityCmS2=0, lifeMs=[200, 320], sizeCm=[5, 8], color="core"),
    ])
    rec["proj.pet_bolt.hit"] = R("arcane", [
        flash("point", 22, "core", life=160),
        impact_star("point", "normal", 48, "event", life=140),
        sparks("point", 6, "event"),
        ring("point", px(3), px(16), 260, "event", alpha=0.8),
    ])
    # fireball (C + flight clamp(1.5 × dist px, 300, 600) ms)
    rec["proj.fireball.launch"] = R("fire", [
        flash("origin", 20, "core", h="hand"),
        sparks("origin", 4, "mid", h="hand", dir="blow", cone=60),
    ])
    rec["proj.fireball"] = R("fire", [
        # a hot toon core (HDR tint → it blooms) inside an orange halo; the comet streak gives the motion read
        head_mesh("SM_FX_Fireball", "#FFB347", 34, size=0.8, intensity=2.0, rate=60),
        lay(emitter="glow", at="origin", rate=60, durationMs=-1, attach=True, sizeCm=96, grow=0.75, lifeMs=100,
            alpha=[0.75, 0], color="rim"),
        lay(sprite="Comet", orient="velocity", at="origin", rate=60, durationMs=-1, attach=True, dir="blow",
            coneDeg=0, speedCmS=1, sizeCm=118, aspect=0.4, offsetCm=[-34, 0, 0], grow=0.85, lifeMs=90,
            alpha=[0.85, 0], color="#FF8A2A", intensity=1.6),
        lay(emitter="flash", at="origin", rate=60, durationMs=-1, attach=True, sizeCm=28, grow=1, lifeMs=50,
            alpha=[1, 0.6], color="core", offsetCm=[12, 0, 4]),
        # short-lived tongues shed behind the head (they shrink as they rise): a flicker, not a burning fuse
        flames("origin", 0, FLAME["fire"], radius_px=4, rise=(30, 60), life=(130, 200), size=(0.26, 0.42),
               rate=48, h=-14),
        lay(emitter="motes", sprite="Ember", at="origin", rate=22, durationMs=-1, speedCmS=[10, 40],
            gravityCmS2=-80, lifeMs=[220, 380], sizeCm=[6, 10], color="rim"),
        smoke("origin", 0, "#4A3A36", size=(0.25, 0.35), life=(380, 560), alpha=0.45, rate=9, durationMs=-1,
              h=0, count=None),
    ])
    rec["proj.fireball.hit"] = R("fire", [
        flash("point", 34, "core", life=170),
        impact_star("point", "heavy", 90, "#FFA532"),
        glow("point", 120, 320, "rim", alpha=0.6, grow=1.67),
        shock("point", px(8), px(46), 380, "mid"),
        flames("point", 9, FLAME["fire"], radius_px=6, rise=(30, 60), life=(300, 420), size=(0.5, 0.75), h=10,
               speedCmS=[200, 311], dir="flat"),
        sparks("point", 10, "mid", gravity=160),
        motes("point", 8, "rim", rise=40),
        smoke("point", 4, "#4A3A36", delay=90, h=30),
        decal("point", "Scorch", px(18), 1400, "#2E1E18", alpha=0.7),
    ], shake=(90, 0.003))
    rec["proj.ice_arrow.launch"] = R("frost", [flash("origin", 16, "core", h="hand")])
    rec["proj.ice_arrow"] = R("frost", [
        head_mesh("SM_FX_IceArrow", "#CFF6FF", 520, intensity=1.2),
        *comet_tail("#46B4F0", 74, 0.26),
        lay(emitter="glow", orient="velocity", at="origin", rate=60, durationMs=-1, attach=True, dir="blow",
            coneDeg=0, speedCmS=1, sizeCm=70, aspect=0.45, grow=0.7, lifeMs=90, alpha=[0.7, 0], color="rim"),
        lay(emitter="motes", sprite="Flake", at="origin", rate=36, durationMs=-1, speedCmS=[5, 30],
            gravityCmS2=30, lifeMs=[250, 420], sizeCm=[6, 10], color="core"),
        smoke("origin", 0, "#CFEFFF", size=(0.2, 0.3), life=(300, 450), alpha=0.35, rate=9, durationMs=-1, h=0,
              count=None),
    ])
    rec["proj.ice_arrow.hit"] = R("frost", [
        flash("point", 24, "core"),
        lay(mesh="SM_FX_IceShard", sprite="ShardFrost", orient="velocity", blend="translucent", at="point",
            height="chest", count=7, dir="back", coneDeg=120, speedCmS=[px(100), px(170)], upCmS=[60, 160],
            gravityCmS2=px(180) * 2, lifeMs=[300, 450], sizeCm=[mesh_size("SM_FX_IceShard", 0.45),
                                                               mesh_size("SM_FX_IceShard", 0.7)],
            grow=1, alpha=[1, 0], alphaPow=7, color="#DDF8FF", intensity=1.1),
        motes("point", 6, "core", sprite="Flake", size=(6, 10)),
        ring("point", px(4), px(24), 320, "mid"),
        decal("point", "Frost", px(16), 1000, "mid", alpha=0.6, blend="additive"),
    ])
    rec["proj.poison_arrow"] = R("poison", [
        head_mesh("SM_FX_PoisonArrow", "#9AEC40", 520),
        *comet_tail("#3FAE2E", 70, 0.26),
        lay(sprite="Drop", orient="billboard", blend="translucent", at="origin", rate=20, durationMs=-1,
            gravityCmS2=500, sizeCm=[5, 8], aspect=0.67, grow=1, lifeMs=[250, 400], alpha=[1, 0], color="rim"),
        lay(emitter="glow", at="origin", rate=14, durationMs=-1, sizeCm=26, grow=1.5, lifeMs=300, alpha=[0.5, 0],
            color="mid"),
    ])
    rec["proj.poison_arrow.hit"] = R("poison", [
        flash("point", 20, "mid"),
        drops("point", 8, "rim", dir="up", cone=120, speed=(70, 120), gravity=380),
        smoke("point", 3, "#7AC84A", alpha=0.6, h=30),
        decal("point", "Puddle", px(14), 1200, "rim", alpha=0.75, grow=1.3),
        motes("point", 4, "mid", sprite="Bubble", rise=30, size=(8, 12), spin=0, h=10),
    ])
    # arrows (multishot / piercing / arrow rain heads)
    rec["proj.arrow"] = R("steel", [
        head_mesh("SM_FX_Arrow", "#EEF3FB", 520),
        lay(sprite="Comet", orient="velocity", at="origin", rate=60, durationMs=-1, attach=True, dir="blow",
            coneDeg=0, speedCmS=1, sizeCm=40, aspect=0.3, grow=0.8, lifeMs=80, alpha=[0.7, 0], color="#EEF3FB",
            offsetCm=[-26, 0, 0]),
    ])
    rec["proj.arrow.hit"] = R("steel", [glint("point", 14, life=160)])
    rec["proj.piercing_arrow"] = R("holy", [
        head_mesh("SM_FX_Arrow", "#FFE27A", 520, size=0.8, intensity=1.2),
        lay(sprite="Comet", orient="velocity", at="origin", rate=60, durationMs=-1, attach=True, dir="blow",
            coneDeg=0, speedCmS=1, sizeCm=60, aspect=0.3, grow=0.8, lifeMs=90, alpha=[0.85, 0], color="mid",
            intensity=1.4, offsetCm=[-24, 0, 0]),
    ])
    rec["proj.piercing_arrow.hit"] = R("holy", [glint("point", 18, life=160), sparks("point", 5, "mid")])
    # ground effects
    rec["ground.default"] = R("element", [
        lay(sprite="Rune", orient="ground", at="origin", height=3, unit="radius", rate=1.2, durationMs=-1,
            attach=True, sizeCm=2, grow=1.05, lifeMs=1200, fadeIn=0.3, alpha=[0.5, 0], spinDegS=25, color="rim"),
        lay(emitter="motes", at="origin", unit="radius", rate=10, durationMs=-1, attach=True, spawnRadiusCm=1,
            gravityCmS2=-90, sizeCm=[0.04, 0.07], color="mid"),
    ])
    rec["ground.default.trigger"] = R("element", [
        flash("origin", 30, "core", h=30),
        lay(emitter="shock", at="origin", unit="radius", sizeCm=0.6, grow=3.3, lifeMs=380, color="mid"),
        sparks("origin", 12, "mid", h=30),
    ])
    rec["ground.default.end"] = R("element", [
        lay(emitter="ring", at="origin", unit="radius", sizeCm=round(1 / RING_R, 3), grow=1.2, lifeMs=400,
            alpha=[0.4, 0], color="mid"),
    ])
    # fire wall: the 3 s persistent ring of flames (C4)
    rec["ground.fire_wall"] = R("fire", [
        lay(emitter="flames", sprite="Flame", variant=FLAME["fire"], blend="translucent", orient="upright",
            at="origin", unit="radius", rate=105, durationMs=-1, attach=True, spawnRadiusCm=1, onRing=True,
            sizeCm=[0.32, 0.46], aspect=0.5, grow=0.75, sizePow=2, upCmS=[40, 80], lifeMs=[420, 640],
            alpha=[1, 0], fadeIn=0.15, alphaPow=1.2, color="white"),
        lay(emitter="flames", sprite="Flame", variant=FLAME["fire"], blend="translucent", orient="upright",
            at="origin", unit="radius", rate=40, durationMs=-1, attach=True, spawnRadiusCm=0.93, onRing=True,
            sizeCm=[0.2, 0.3], aspect=0.5, grow=0.7, upCmS=[60, 110], lifeMs=[300, 480], alpha=[1, 0],
            fadeIn=0.15, color="white", height=25),
        lay(emitter="glow", at="origin", unit="radius", rate=14, durationMs=-1, attach=True, spawnRadiusCm=1,
            onRing=True, height=40, sizeCm=0.42, grow=1.3, lifeMs=700, fadeIn=0.3, alpha=[0.5, 0], color="rim"),
        lay(emitter="motes", at="origin", unit="radius", rate=10, durationMs=-1, attach=True, spawnRadiusCm=1,
            gravityCmS2=-150, sizeCm=[0.03, 0.05], color="rim"),
        lay(emitter="decal", sprite="Scorch", at="origin", unit="radius", rate=0.7, durationMs=-1, attach=True,
            sizeCm=2.3, lifeMs=1700, alpha=[0.45, 0], color="#2E1E18"),
    ], light_=light(260, 0.5))
    # traps armed at the hero's feet (blinking rune), then the trigger burst
    rec["ground.explosive_trap"] = R("fire", [
        lay(sprite="Rune", orient="ground", at="origin", height=3, rate=1.5, durationMs=-1, attach=True,
            sizeCm=px(18) * 2, grow=1, lifeMs=900, fadeIn=0.3, alpha=[0.75, 0], spinDegS=229, color="rim",
            intensity=1.2),
        lay(emitter="glow", orient="ground", at="origin", height=2, rate=1.5, durationMs=-1, attach=True,
            sizeCm=70, grow=1.1, lifeMs=600, fadeIn=0.4, alpha=[0.35, 0], color="#FF7A3A"),
    ])
    rec["ground.explosive_trap.trigger"] = R("fire", [
        flash("origin", 40, "core", h=40, life=200),
        impact_star("origin", "crit", 160, "#FFA532", h=40, life=180),
        lay(emitter="glow", at="origin", height=40, unit="radius", sizeCm=1.6, grow=1.4, lifeMs=360,
            alpha=[0.7, 0], color="rim"),
        lay(emitter="shock", blend="translucent", at="origin", height=1.5, unit="radius", sizeCm=0.16,
            grow=12.5, lifeMs=380, alpha=[0.45, 0], color="mid"),
        lay(emitter="shock", at="origin", height=2.5, unit="radius", sizeCm=0.16, grow=12.5, lifeMs=380,
            alpha=[1, 0], color="mid", intensity=1.15),
        lay(sprite="Ring", orient="ground", at="origin", height=3, unit="radius", sizeCm=round(0.13 / RING_R, 3),
            grow=6.5, sizePow=3, lifeMs=260, alpha=[1, 0], color="white", intensity=1.2),
        lay(emitter="flames", sprite="Flame", variant=FLAME["fire"], blend="translucent", orient="upright",
            at="origin", unit="radius", rate=100, durationMs=120, spawnRadiusCm=0.4, sizeCm=[0.2, 0.3], aspect=0.5,
            grow=0.75, upCmS=[155, 290], lifeMs=[360, 520], alpha=[1, 0], fadeIn=0.15, color="white"),
        debris("origin", 8),
        sparks("origin", 12, "mid", h=30, gravity=220),
        smoke("origin", 7, "#4A3A36", delay=90, h=30),
        lay(emitter="decal", sprite="Scorch", at="origin", unit="radius", sizeCm=1.0, lifeMs=1800,
            alpha=[0.7, 0], color="#2E1E18"),
    ], shake=(200, 0.009))
    rec["ground.slow_trap"] = R("frost", [
        lay(sprite="Rune", orient="ground", at="origin", height=3, rate=1.5, durationMs=-1, attach=True,
            sizeCm=px(16) * 2, grow=1, lifeMs=900, fadeIn=0.3, alpha=[0.75, 0], spinDegS=286, color="rim",
            intensity=1.2),
    ])
    rec["ground.slow_trap.trigger"] = R("frost", [
        flash("origin", 26, "mid", h=30),
        lay(emitter="decal", sprite="Frost", blend="additive", at="origin", unit="radius", sizeCm=1.6,
            lifeMs=1500, alpha=[0.6, 0], color="mid"),
        lay(sprite="Rune", orient="ground", at="origin", height=3, unit="radius", sizeCm=1.4, grow=1,
            lifeMs=900, alpha=[0.7, 0], spinDegS=34, color="mid", intensity=1.2),
        *[lay(sprite="Ring", orient="ground", at="origin", height=3, unit="radius",
              sizeCm=round(px(8) / 100 / RING_R, 3), grow=round(1 / (px(8) / 100), 2), sizePow=2, lifeMs=520,
              alpha=[a, 0], color="rim", intensity=1.2, delayMs=d) for d, a in ((0, 0.85), (150, 0.65), (300, 0.45))],
        motes("origin", 10, "core", sprite="Flake", radius_px=27, h=30, size=(7, 11)),
        smoke("origin", 5, "#CFEFFF", alpha=0.35, h=20),
    ])
    rec["ground.chain_trap"] = R("holy", [
        lay(sprite="Rune", orient="ground", at="origin", height=3, rate=1.5, durationMs=-1, attach=True,
            sizeCm=px(18) * 2, grow=1, lifeMs=900, fadeIn=0.3, alpha=[0.75, 0], spinDegS=180, color="rim",
            intensity=1.2),
    ])
    rec["ground.chain_trap.trigger"] = R("holy", [
        flash("origin", 30, "core", h=30),
        # five chains lashing out at fixed 72° yaws (one count-1 layer each: random `dir: out` angles cluster),
        # links along UE +Y from the pivot, growing out to 0.76 of the trap radius (unit radius), lying 14 cm over the
        # ground; yaws 81 + 72k flank the view axis (a chain heading straight away from the W1 camera seemed to rise
        # through the rogue) and the links are a quarter smaller (T3 review: oversized)
        *[lay(mesh="SM_FX_Chain", sprite="Chain", orient="ground", blend="translucent", at="origin", height=14,
              count=1, unit="radius", rotationDeg=-9 + 72 * k, delayMs=14 * k,
              sizeCm=round(MESH_DIAMETER["SM_FX_Chain"] / 58.0 * 0.095, 3), grow=8.0, sizePow=2, lifeMs=700,
              alpha=[1, 0], alphaPow=6, color="#C9D3E0") for k in range(5)],
        lay(emitter="bolt", at="origin", beamTo="point", height=30, beamToHeight=30, count=3, sizeCm=50,
            color="#FFE27A", delayMs=120),
        sparks("origin", 10, "mid", h=30),
        ring("origin", 20, 140, 420, "rim"),
    ], shake=(120, 0.004))
    rec["ground.arrow_rain"] = R("steel", [
        lay(mesh="SM_FX_Arrow", sprite="Arrow", orient="velocity", blend="translucent", at="origin",
            unit="radius", rate=30, durationMs=-1, attach=True, spawnRadiusCm=0.9, spawnHeightJitterCm=30,
            height=260, speedCmS=1700, dir="down", coneDeg=10, lifeMs=150, sizeCm=0.16, grow=1, alpha=[1, 1],
            color="#EEF3FB"),
        # stuck shafts: the inked arrow mesh, velocity-oriented by a 1 cm/s downward drift (head down, ±12° lean),
        # centred 16 cm up so the head is buried; dissolves late (a flat Arrow quad was nearly invisible)
        lay(mesh="SM_FX_Arrow", sprite="Arrow", orient="velocity", blend="translucent", at="origin", unit="radius",
            rate=30, durationMs=-1, attach=True, spawnRadiusCm=0.9, height=16, delayMs=150, dir="down", coneDeg=24,
            speedCmS=1, sizeCm=0.15, grow=1, lifeMs=650, alpha=[1, 0], alphaPow=8, color="#EEF3FB"),
        lay(emitter="smoke", sprite="Dust", at="origin", unit="radius", rate=10, durationMs=-1, attach=True,
            spawnRadiusCm=0.9, delayMs=150, sizeCm=[0.1, 0.16], lifeMs=[300, 480], alpha=[0.5, 0],
            color="#C8B89A"),
        lay(emitter="glint", at="origin", unit="radius", rate=10, durationMs=-1, attach=True, spawnRadiusCm=0.9,
            height=6, delayMs=150, sizeCm=0.035, lifeMs=160),
    ])
    rec["ground.arrow_rain.end"] = R("steel", [
        lay(emitter="ring", at="origin", unit="radius", sizeCm=round(1 / RING_R, 3), grow=1.15, lifeMs=400,
            alpha=[0.4, 0], color="mid"),
    ])
    return rec


# ════════════════════════════════════════════════════════════════════════════════════════════════════
# skills (§8.6) — skill.<id>, played on EvSkillVfx at the anim beat (A contact / C release)
# ════════════════════════════════════════════════════════════════════════════════════════════════════
def warrior() -> dict:
    rec = {}
    arc_w = "SM_FX_SlashArc_Warrior"
    rec["skill.slash"] = R("holy", [
        ribbon(arc_w, "origin", 115, 230, "rim", rot=-38, spin=260, intensity=1.3),
        swipe_glow("origin", 120, 200, "mid", alpha=0.8),
        swipe_glow("origin", 92, 180, "#EEF3FB", delay=25, alpha=0.6),
        flash("target", 16, "mid", life=120, delay=40),
        impact_star("target", "heavy", 64, "mid", delay=40),
        sparks("target", 8, "mid", speed=(110, 220), dir="blow", cone=80, delay=40),
        glint("target", 26, life=180, delay=40),
    ])
    rec["skill.whirlwind"] = R("steel", [
        # mesh dissolves are a 4×4 screen-door that only TAA resolves: alphaPow 10 keeps the half-dithered window to
        # the last ~2 frames (VFX review 16; integration.meshDissolve)
        ribbon("SM_FX_WhirlBand", "origin", 155, 300, "mid", rot=(0, 360), spin=1500, align=False, alpha_pow=10,
               intensity=1.2, grow=1.0, unit=None),
        ribbon(arc_w + "_Back", "origin", 121, 220, "#F4AC28", rot=(0, 360), spin=-900, align=False, delay=30,
               grow=1.0, alpha_pow=10),
        decal("origin", "Swirl", 160, 420, "rim", alpha=0.55, blend="additive", spin=-10.0, pow=1.6, h=3),
        lay(emitter="streak", at="origin", height="chest", count=10, dir="tangent", speedCmS=400,
            spawnRadiusCm=110, lifeMs=200, sizeCm=26, color="white", alpha=[0.9, 0]),
        ring("origin", 62, 200, 420, "mid", alpha=0.7),
        shock("origin", 46, 230, 480, "mid", alpha=0.4, delay=120),
        lay(emitter="smoke", sprite="Dust", at="origin", count=8, spawnRadiusCm=150, onRing=True, height=4,
            sizeCm=[30, 44], lifeMs=[400, 600], alpha=[0.6, 0], color="#C8B89A", dir="out", speedCmS=[40, 90],
            gravityCmS2=-20),
    ])
    rec["skill.shield_wall"] = R("holy", [
        gather("origin", 10, "mid", 44, 140),
        decal("origin", "Rune", px(40), 900, "rim", alpha=0.8, blend="additive", spin=0.6, delay=60, pow=1.6),
        lay(mesh="SM_FX_HexRing", sprite="Hex", orient="ground", blend="translucent", at="origin",
            height="chest", delayMs=100, sizeCm=mesh_size("SM_FX_HexRing", 0.62), grow=round(1 / 0.62, 3),
            sizePow=5, lifeMs=780, alpha=[1, 0], alphaPow=6, color="mid", intensity=1.1),
        lay(sprite="Hex", orient="billboard", at="origin", height="chest", delayMs=100, count=8,
            orbit={"r0Cm": 50, "r1Cm": 80, "degS": 0}, sizeCm=26, grow=1.75, sizePow=5, lifeMs=780,
            alpha=[0.7, 0], alphaPow=3, color="mid", intensity=1.2),
        flash("origin", 26, "core", delay=110),
        beam("origin", 90, 40, 520, "rim", alpha=0.4, delay=100),
        motes("origin", 12, "core", sprite="Spark", radius_px=20, rise=40, size=(7, 11)),
        ring("origin", px(16), px(56), 420, "mid"),
        shock("origin", px(12), px(50), 400, "mid", alpha=0.6),
    ])
    rec["skill.taunt_roar"] = R("rage", [
        glow("origin", 45, 260, "rim", h="head", grow=2.2),
        *[lay(emitter="slash", sprite="Slash", variant=SLASH["heavy"], orient="ground", blend=bl, at="origin",
              height="chest", count=8, dir="out", coneDeg=0, speedCmS=560, drag=2, sizeCm=15, aspect=2.1, grow=2.0,
              lifeMs=360, alpha=[a, 0], alphaPow=1.4, alignToBlow=False, color=c, intensity=i, delayMs=d)
          for c, d in (("rim", 0), ("mid", 110)) for bl, a, i in (("translucent", 0.85, 1.0), ("additive", 0.9, 1.4))],
        ring("origin", px(14), 300, 480, "rim"),
        ring("origin", px(10), 240, 440, "mid", delay=110, body=False),
        shock("origin", 30, 270, 460, "mid", alpha=0.5),
        sparks("origin", 10, "mid"),
    ])
    rec["skill.charge"] = R("holy", [
        lay(emitter="streak", at="path", height="chest", count=9, spawnRadiusCm=25, speedCmS=930, dir="blow",
            coneDeg=4, lifeMs=200, sizeCm=16, color="mid", altEvery=2, altColor="white"),
        lay(emitter="smoke", sprite="Dust", at="path", height=4, count=5, sizeCm=[28, 40], lifeMs=[380, 560],
            alpha=[0.65, 0], color="#C8B89A", speedCmS=[20, 60], gravityCmS2=-30),
        flash("target", 26, "mid", delay=70),
        impact_star("target", "heavy", 110, "mid", delay=70, life=170),
        sparks("target", 12, "mid", speed=(140, 260), dir="blow", cone=103, delay=70),
        shock("target", px(8), px(46), 400, "#E8C47A", delay=70),
        glint("target", 40, life=200, delay=70),
        debris("target", 4, delay=70),
    ], shake=(150, 0.008))
    rec["skill.iron_fortress"] = R("steel", [
        gather("origin", 10, "mid", 44, 140),
        decal("origin", "Rune", px(38), 900, "rim", alpha=0.8, blend="additive", spin=-0.5, pow=1.6),
        lay(mesh="SM_FX_HexDome", sprite="Hex", orient="ground", blend="translucent", at="origin",
            height="chest", delayMs=100, sizeCm=mesh_size("SM_FX_HexDome", 0.62), grow=round(1 / 0.62, 3),
            sizePow=5, lifeMs=800, alpha=[1, 0], alphaPow=6, color="#9FB4D4", intensity=1.1),
        lay(sprite="Hex", orient="billboard", at="origin", height="chest", delayMs=100, count=6,
            orbit={"r0Cm": 55, "r1Cm": 89, "degS": 0}, sizeCm=26, grow=1.75, sizePow=5, lifeMs=800,
            alpha=[0.6, 0], alphaPow=3, color="#7F97BB", intensity=1.2),
        lay(sprite="Hex", orient="billboard", at="origin", height="head", delayMs=160, count=6,
            orbit={"r0Cm": 40, "r1Cm": 64, "degS": 0}, sizeCm=24, grow=1.75, sizePow=5, lifeMs=800,
            alpha=[0.6, 0], alphaPow=3, color="#9FB4D4", intensity=1.2),
        flash("origin", 26, "mid", delay=170),
        glint("origin", 30, delay=170),
        ring("origin", px(14), px(50), 420, "mid", delay=170),
        sparks("origin", 8, "mid", delay=170),
    ])
    def heartbeat(delay, col):
        return [lay(sprite="Ring", orient="billboard", blend="translucent", at="origin", height="chest", sizeCm=150,
                    grow=1.35, sizePow=2, lifeMs=280, alpha=[0.75, 0], fadeIn=0.1, color=col, delayMs=delay),
                lay(sprite="Ring", orient="billboard", at="origin", height="chest", sizeCm=150, grow=1.35,
                    sizePow=2, lifeMs=280, alpha=[0.6, 0], fadeIn=0.1, color=col, intensity=1.3, delayMs=delay)]
    rec["skill.frenzy"] = R("rage", [
        *heartbeat(0, "rim"),
        ring("origin", px(10), px(46), 320, "rim"),
        *heartbeat(150, "mid"),
        ring("origin", px(10), px(46), 320, "mid", delay=150, body=False),
        flames("origin", 10, FLAME["rage"], radius_px=26, on_ring=True, rise=(60, 100), size=(0.55, 0.85),
               even=True),
        sparks("origin", 10, "mid", dir="up", cone=70),
        motes("origin", 8, "rim", rise=110),
    ])
    rec["skill.bleed_strike"] = R("blood", [
        ribbon(arc_w + "_Back", "origin", 110, 230, "mid", rot=40, spin=-260, intensity=1.2),
        swipe_glow("origin", 112, 200, "rim", alpha=0.75, rotationDeg=8),
        swipe_glow("origin", 88, 180, "#FFB0A0", delay=25, alpha=0.5),
        flash("target", 18, "mid", delay=40),
        drops("target", 10, "mid", delay=40),
        lay(sprite="Drop", orient="billboard", blend="translucent", at="target", height="chest", rate=14,
            durationMs=360, delayMs=160, spawnRadiusCm=14, gravityCmS2=600, sizeCm=[6, 9], aspect=0.67, grow=1,
            lifeMs=[350, 500], alpha=[1, 0.5], color="mid"),
        decal("target", "Puddle", px(11), 1300, "rim", alpha=0.8, grow=1.4, delay=200),
    ])
    rec["skill.unyielding"] = R("holy", [
        decal("origin", "Crack", px(30), 1000, "#3A2A1E", alpha=0.8),
        decal("origin", "CrackGlow", px(28), 420, "#F4AC28", alpha=0.9, blend="additive", pow=1.5),
        shock("origin", px(10), px(44), 400, "#E8C47A"),
        lay(emitter="debris", mesh="SM_FX_Rock_A", at="origin", count=8, upCmS=[100, 140], gravityCmS2=0,
            speedCmS=0, lifeMs=620, orbit={"r0Cm": 53, "r1Cm": 60, "degS": 220},
            sizeCm=[mesh_size("SM_FX_Rock_A", 1.0), mesh_size("SM_FX_Rock_A", 1.4)], color="#B08850"),
        beam("origin", 64, 30, 500, "#F4AC28", alpha=0.45, blend="translucent"),
        beam("origin", 64, 30, 500, "rim", alpha=0.55),
        glow("origin", 85, 400, "mid", grow=1.4),
        glint("origin", 28),
    ])
    rec["skill.life_regen"] = R("nature", [
        lay(emitter="glow", orient="ground", at="origin", height=3, sizeCm=115, grow=1.2, lifeMs=700,
            alpha=[0.6, 0], color="rim"),
        ring("origin", px(8), px(32), 420, "mid"),
        motes("origin", 6, "mid", sprite="Plus", rise=50, size=(14, 20), spin=0),
        motes("origin", 10, "core", rise=70),
    ])
    rec["skill.war_stomp"] = R("earth", [
        decal("origin", "Crack", 110, 1400, "#3A2A1E", alpha=0.75, unit=None),
        lay(emitter="decal", sprite="CrackGlow", blend="additive", at="origin", unit="radius", sizeCm=1.1,
            lifeMs=600, alpha=[0.9, 0], color="#FF9A3A"),
        impact_star("origin", "heavy", 150, "mid", h=30, life=170),
        lay(emitter="shock", at="origin", unit="radius", sizeCm=0.3, grow=6.5, lifeMs=420, color="mid"),
        lay(emitter="ring", at="origin", unit="radius", sizeCm=round(0.2 / RING_R, 3), grow=5, lifeMs=460,
            color="rim"),
        lay(emitter="ring", at="origin", unit="radius", delayMs=100, sizeCm=round(0.15 / RING_R, 3), grow=6.5,
            lifeMs=520, color="mid"),
        debris("origin", 10, spawn_px=27),
        lay(emitter="smoke", sprite="Dust", at="origin", unit="radius", count=10, spawnRadiusCm=0.8, height=4,
            sizeCm=[0.16, 0.26], alpha=[0.6, 0], color="#C8B89A"),
        sparks("origin", 8, "mid", h=30),
    ], shake=(240, 0.011))
    rec["skill.vengeful_wrath"] = R("rage", [
        gather("origin", 12, "rim", 44, 160, sprite="Ember"),
        flames("origin", 14, FLAME["rage"], radius_px=26, on_ring=True, rise=(50, 90), size=(0.55, 0.8),
               even=True),
        flames("origin", 5, FLAME["fire"], radius_px=8, rise=(70, 120), size=(0.9, 1.2)),
        decal("origin", "CrackGlow", px(30), 700, "#FF5A2A", alpha=0.9, blend="additive", pow=1.5),
        glow("origin", 110, 360, "rim", grow=1.6),
        flash("origin", 30, "core"),
        shock("origin", px(10), px(60), 420, "mid"),
        ring("origin", px(12), px(70), 480, "rim"),
        motes("origin", 14, "rim", rise=110),
    ])
    # the tall vertical blood crescent (web r 44 px: the arc stands ~2 m, twice the goblin) — a dark translucent
    # body + the additive edge — then the steel counter-slash mirrored at +70 ms; the crit star stays small so the
    # crescent, not a disc, is the read
    # the overhead cleave (web :347-360, spec "tall vertical blood crescent r 44 + steel counter-slash (+70)"): the
    # Slash cell is aligned to the blow at rotation 0, so the crescent stands as a ")" facing the blow — its span
    # (aspect 2) runs screen-vertical, 2 × 78 cm from over the head to the feet (anchor lifted 24 cm off fx_chest).
    # A rotation of ±90 laid it flat (a wide ∩ the white counter-slash covered) and the 70 cm crit impact star read as
    # an orange rune disc, so the blood is a solid TRANSLUCENT crescent (dark-blood under-stroke + blood-mid body)
    # with only a thin additive hot edge, and the steel counter-slash (+70 ms) crosses it on the diagonal
    rec["skill.lethal_strike"] = R("blood", [
        slash_sprite("target", 84, 300, "#4A0810", cell=SLASH["heavy"], aspect=2.0, blend="translucent",
                     alpha=0.9, grow=1.12, alphaPow=2.2, offsetCm=[0, 0, 24]),
        slash_sprite("target", 78, 300, "mid", cell=SLASH["heavy"], aspect=2.0, blend="translucent", alpha=0.95,
                     grow=1.12, alphaPow=2.2, offsetCm=[0, 0, 24]),
        slash_sprite("target", 74, 220, "#FFB0A0", cell=SLASH["heavy"], aspect=2.0, alpha=0.55, grow=1.12,
                     intensity=1.0, offsetCm=[0, 0, 24]),
        slash_sprite("target", 60, 220, "#56647C", cell=SLASH["warrior"], rot=-62, aspect=2.0, delay=70,
                     blend="translucent", alpha=0.8, offsetCm=[0, 0, 12]),
        slash_sprite("target", 56, 200, "#EEF3FB", cell=SLASH["warrior"], rot=-62, aspect=2.0, delay=70,
                     intensity=1.15, offsetCm=[0, 0, 12]),
        flash("target", 20, "mid", life=150, delay=10),
        glint("target", 50, life=220, rot=45, delay=10),
        glint("target", 36, life=260, color="mid", delay=40),
        sparks("target", 12, "mid", speed=(140, 260), size=1.1, dir="blow", cone=90, delay=10),
        shock("target", px(8), px(42), 320, "rim", alpha=0.7, delay=20),
        drops("target", 7, "mid", size=(8, 12)),
    ], shake=(160, 0.008))
    rec["skill.dual_wield_mastery"] = R("holy", [
        slash_sprite("origin", 70, 220, "rim", cell=SLASH["warrior"], rot=45),
        slash_sprite("origin", 70, 220, "#EEF3FB", cell=SLASH["warrior"], rot=-45, delay=40),
        glint("origin", 30),
        motes("origin", 8, "core", sprite="Spark", rise=40, size=(7, 11)),
    ])
    rec["skill.rampage"] = R("blood", [
        ribbon("SM_FX_WhirlBand", "origin", 160, 340, "mid", rot=(0, 360), spin=1300, align=False, grow=1.0,
               alpha_pow=10),
        ribbon("SM_FX_WhirlBand", "origin", 120, 320, "rim", rot=(0, 360), spin=1500, align=False, delay=60,
               grow=1.0, alpha_pow=10),
        ribbon(arc_w, "origin", 190, 280, "#FF5A2A", rot=(0, 360), spin=1100, align=False, delay=120, grow=1.0,
               alpha_pow=10),
        decal("origin", "Crack", 160, 1400, "#3A2A1E", alpha=0.75),
        decal("origin", "CrackGlow", 160, 600, "#FF5A2A", alpha=0.85, blend="additive", pow=1.5),
        shock("origin", 40, 280, 460, "mid"),
        ring("origin", 50, 260, 520, "rim"),
        flames("origin", 12, FLAME["rage"], radius_px=40, on_ring=True, rise=(50, 90), even=True),
        debris("origin", 10, spawn_px=30),
        sparks("origin", 12, "mid"),
        smoke("origin", 6, "#5A4048", radius_px=40, delay=100),
    ], shake=(280, 0.012))
    return rec


def mage() -> dict:
    rec = {}
    rec["skill.fireball"] = R("fire", [flash("origin", 14, "mid", h="hand", life=140)],
                              note="cast glint only: the flight is proj.fireball(.launch / .hit)")
    rec["skill.ice_arrow"] = R("frost", [flash("origin", 12, "mid", h="hand", life=120)])
    R_bz = 0.85 * 300
    rec["skill.blizzard"] = R("frost", [
        decal("point", "Frost", R_bz, 1300, "rim", alpha=0.55, blend="additive", grow=1.08, fade_in=0.15, pow=2),
        ring("point", 0.4 * R_bz, R_bz, 500, "rim"),
        *[lay(mesh="SM_FX_IceShard", sprite="ShardFrost", orient="velocity", blend="translucent", at="point",
              rate=round(n / 0.16, 1), durationMs=160, delayMs=d, spawnRadiusCm=0.85 * R_bz, height=250,
              speedCmS=1450, dir="down", coneDeg=24, lifeMs=[150, 172], sizeCm=[mesh_size("SM_FX_IceShard", 1.4),
                                                                             mesh_size("SM_FX_IceShard", 1.8)],
              grow=1, alpha=[1, 1], color="#DDF8FF", intensity=1.15) for n, d in ((9, 0), (9, 180), (8, 360))],
        *[lay(emitter="streak", at="point", rate=round(n / 0.16, 1), durationMs=160, delayMs=d,
              spawnRadiusCm=0.85 * R_bz, height=240, speedCmS=1450, dir="down", coneDeg=24, lifeMs=150, sizeCm=60,
              aspect=0.1, stretch=0.02, color="#CFF6FF", alpha=[0.85, 0], intensity=1.3)
          for n, d in ((9, 0), (9, 180), (8, 360))],
        *[lay(emitter="glint", at="point", rate=round(n / 0.16, 1), durationMs=160, delayMs=d + 160,
              spawnRadiusCm=0.85 * R_bz, height=6, sizeCm=14, lifeMs=160, color="white", intensity=1.2)
          for n, d in ((9, 0), (9, 180), (8, 360))],
        *[lay(sprite="ShardFrost", orient="upright", blend="translucent", at="point", rate=round(n / 0.16, 1),
              durationMs=160, delayMs=d + 165, spawnRadiusCm=0.85 * R_bz, height=-8, sizeCm=[36, 48], aspect=0.33,
              grow=1, lifeMs=[260, 460], alpha=[1, 0], alphaPow=4, rotationDeg=[-25, 25], color="white")
          for n, d in ((9, 0), (9, 180), (8, 360))],
        motes("point", 16, "core", sprite="Flake", radius_px=0.9 * R_bz / PX, gravity=60, rise=0, speed=(5, 18),
              life=(700, 1100), h=120, size=(8, 12)),
        smoke("point", 6, "#CFEFFF", radius_px=0.7 * R_bz / PX, alpha=0.35, delay=200, h=30,
              size=(0.6, 0.85)),
    ])
    rec["skill.ice_armor"] = R("frost", [
        decal("origin", "Frost", px(30), 900, "mid", alpha=0.6, blend="additive"),
        lay(mesh="SM_FX_FrostNova", sprite="ShardFrost", orient="ground", blend="translucent", at="origin",
            height=0, sizeCm=mesh_size("SM_FX_FrostNova", 0.5), grow=2.0, sizePow=6, lifeMs=520, alpha=[1, 0],
            alphaPow=12, rotationDeg=[0, 360], color="#CFF6FF", intensity=1.15),
        flash("origin", 26, "mid", delay=60),
        ring("origin", px(10), px(42), 420, "rim"),
        glint("origin", 30, delay=60),
        motes("origin", 8, "core", sprite="Flake", radius_px=22, delay=420, h=30, size=(8, 12)),
        lay(mesh="SM_FX_IceShard", sprite="ShardFrost", orient="velocity", blend="translucent", at="origin",
            height=25, delayMs=430, count=9, spawnRadiusCm=50, onRing=True, dir="out", coneDeg=40,
            speedCmS=[120, 220], upCmS=[60, 140], gravityCmS2=600, lifeMs=[260, 380],
            sizeCm=[mesh_size("SM_FX_IceShard", 0.4), mesh_size("SM_FX_IceShard", 0.6)], grow=1, alpha=[1, 0],
            alphaPow=12, color="#DDF8FF"),
    ])
    rec["skill.chain_lightning"] = R("lightning", [
        flash("origin", 20, "mid", h="hand"),
        lay(emitter="bolt", blend="translucent", at="points", beamTo="points", sizeCm=90, lifeMs=240,
            alpha=[0.8, 0], flicker=0, color="#2A1E66", intensity=1.0),
        lay(emitter="bolt", at="points", beamTo="points", sizeCm=110, color="#CFE0FF", lifeMs=240),
        lay(emitter="bolt", at="points", beamTo="points", sizeCm=70, color="#8C8CFF", lifeMs=200, delayMs=80),
        flash("points", 18, "core"),
        sparks("points", 7, "rim", alt="core", alt_every=2),
        ring("points", px(4), px(22), 300, "mid"),
        glint("points", 26),
    ])
    rec["skill.mana_shield"] = R("arcane", [
        gather("origin", 12, "rim", 44, 300, sprite="Spark", size=(8, 12)),
        decal("origin", "Rune", px(34), 900, "rim", alpha=0.8, blend="additive", spin=0.9, delay=100, pow=1.6),
        lay(sprite="Bubble", orient="billboard", blend="translucent", at="origin", height="chest", delayMs=120,
            sizeCm=52, grow=2.88, sizePow=6, lifeMs=760, alpha=[0.45, 0], alphaPow=3, color="#8A5CFF"),
        # the inked toon shell (rim ring + highlight arc + glint, built facing the W1 camera at rotation 0)
        lay(mesh="SM_FX_Bubble", sprite="Bubble", orient="billboard", blend="translucent", at="origin",
            height="chest", delayMs=120, sizeCm=mesh_size("SM_FX_Bubble", 0.35), grow=round(1 / 0.35, 3),
            sizePow=6, lifeMs=760, alpha=[1, 0], alphaPow=12, color="#A07CFF", intensity=1.9),
        lay(sprite="Bubble", orient="billboard", at="origin", height="chest", delayMs=120, sizeCm=52, grow=2.88,
            sizePow=6, lifeMs=760, alpha=[0.9, 0], alphaPow=2.5, flicker=0.15, color="#9A7CFF", intensity=1.25),
        lay(sprite="Bubble", orient="billboard", at="origin", height="chest", delayMs=120, sizeCm=75, grow=1.7,
            sizePow=6, lifeMs=500, alpha=[0.7, 0], alphaPow=2, color="mid", intensity=1.2),
        flash("origin", 24, "core", delay=120),
        lay(sprite="Spark", orient="billboard", at="origin", height="chest", delayMs=120, count=8,
            orbit={"r0Cm": 62, "r1Cm": 62, "degS": 200}, sizeCm=14, grow=1, lifeMs=640, alpha=[1, 0],
            color="mid", intensity=1.3),
    ])
    R_fw = 0.8 * 200
    rec["skill.fire_wall"] = R("fire", [
        decal("point", "Scorch", 1.15 * R_fw, 1700, "#2E1E18", alpha=0.6),
        shock("point", 0.3 * R_fw, R_fw, 400, "mid"),
        *[flames("point", 14, FLAME["fire"], radius_px=R_fw / PX, on_ring=True, rise=(30, 60), life=(lf, lf + 60),
                 size=(0.85, 1.05), delay=d, h=hh) for lf, d, hh in ((520, 0, 0), (640, 110, 26), (760, 220, 52))],
        lay(emitter="glow", at="point", count=14, spawnRadiusCm=R_fw, onRing=True, height=40, sizeCm=45, grow=1.6,
            lifeMs=700, alpha=[0.6, 0], color="rim"),
        motes("point", 12, "rim", radius_px=R_fw / PX, rise=60, h=40),
        smoke("point", 5, "#4A3A36", radius_px=R_fw / PX, delay=300, h=60),
    ])
    rec["skill.teleport"] = R("arcane", [
        gather("origin", 12, "mid", 34, 180, sprite="Spark", size=(8, 12)),
        lay(emitter="glow", orient="upright", at="origin", height=20, sizeCm=230, aspect=0.45, grow=0.08,
            lifeMs=200, color="mid"),
        ring("origin", px(32), px(4), 220, "mid", pow=2),
        decal("origin", "Rune", px(26), 420, "rim", alpha=0.85, blend="additive", spin=2.0, pow=1.5),
        lay(emitter="streak", at="path", height="chest", count=6, speedCmS=700, dir="blow", coneDeg=4,
            lifeMs=200, sizeCm=18, color="mid"),
        decal("point", "Rune", px(30), 600, "mid", alpha=0.85, blend="additive", spin=-1.5, grow=1.15, delay=100,
              pow=1.5),
        flash("point", 30, "core", delay=100),
        beam("point", 80, 26, 380, "rim", alpha=0.5, delay=100, blend="translucent"),
        beam("point", 80, 26, 380, "mid", alpha=0.85, delay=100, cell=1),
        ring("point", px(6), px(40), 400, "mid", delay=100),
        sparks("point", 10, "mid", delay=100),
        motes("point", 8, "core", rise=80, delay=100),
    ])
    R_mt = 2.5 * 100
    rec["skill.meteor"] = R("fire", [
        decal("point", "Rune", 0.7 * R_mt, 420, "rim", alpha=0.85, blend="additive", spin=1.4, pow=1.2),
        lay(emitter="glow", orient="ground", blend="translucent", at="point", height=2, sizeCm=60, grow=3.2,
            sizePow=1, lifeMs=320, alpha=[0.0, 0.6], color="#1A0E08"),
        # the rock falls along a diagonal from the caster's side (web: from (−90, −260) px): 180 cm back along the
        # blow and 600 cm up, 600 cm/s along the blow + 2000 cm/s down → lands at ≈ +300 ms
        lay(mesh="SM_FX_MeteorRock", sprite="Rock", orient="velocity", blend="translucent", at="point",
            height=600, offsetCm=[-180, 0, 0], speedCmS=600, dir="blow", coneDeg=0, upCmS=-2000, lifeMs=300,
            sizeCm=mesh_size("SM_FX_MeteorRock", 1.15), grow=1, alpha=[1, 1], spinDegS=[-200, 200],
            color="#8A6A5A"),
        lay(sprite="Comet", orient="velocity", at="point", height=600, offsetCm=[-180, 0, 40], speedCmS=600,
            dir="blow", coneDeg=0, upCmS=-2000, lifeMs=300, sizeCm=150, aspect=0.4, grow=1, alpha=[0.9, 0.9],
            color="#FFA532", intensity=1.5),
        lay(emitter="glow", at="point", height=600, offsetCm=[-180, 0, 0], speedCmS=600, dir="blow", coneDeg=0,
            upCmS=-2000, lifeMs=300, sizeCm=140, grow=1, alpha=[0.7, 0.7], color="rim"),
        # the trail: one burst from the sky point spread over fractions of the head's velocity, so at any instant
        # the tongues string out along the fall line behind the head (a fixed emitter cannot follow the head)
        flames("point", 18, FLAME["fire"], radius_px=3, rise=(0, 10), life=(250, 300), size=(0.6, 1.0),
               h=600, offsetCm=[-180, 0, 0], speedCmS=[420, 585], dir="blow", coneDeg=3, upCmS=[-1950, -1400]),
        smoke("point", 10, "#4A3A36", alpha=0.55, h=600, offsetCm=[-180, 0, 0], speedCmS=[210, 390], dir="blow",
              coneDeg=4, upCmS=[-1300, -700], size=(0.4, 0.6), life=(320, 420), rise=0),
        # impact at +300
        flash("point", 50, "core", delay=300, life=200),
        impact_star("point", "heavy", 220, "mid", delay=300, life=200, h=30),
        glow("point", 2 * R_mt, 420, "rim", delay=300, grow=1.4, alpha=0.7),
        shock("point", 30, R_mt, 460, "mid", delay=300),
        ring("point", 40, 1.1 * R_mt, 520, "rim", delay=300),
        ring("point", 30, 0.8 * R_mt, 480, "mid", delay=420, body=False),
        decal("point", "Scorch", 0.8 * R_mt, 2200, "#2E1E18", alpha=0.8, delay=300),
        decal("point", "CrackGlow", 0.7 * R_mt, 900, "#FF7A2A", alpha=0.9, blend="additive", delay=300, pow=1.5),
        flames("point", 16, FLAME["fire"], radius_px=0.5 * R_mt / PX, rise=(50, 100), size=(0.8, 1.1), delay=300),
        flames("point", 12, FLAME["fire"], radius_px=6, rise=(30, 60), size=(0.6, 0.85), delay=300, h=10,
               speedCmS=[260, 420], dir="flat"),
        debris("point", 12, delay=300, color="#6A5048", mesh="SM_FX_Rock_B"),
        sparks("point", 14, "mid", delay=300, gravity=200),
        motes("point", 12, "rim", delay=300, rise=60),
        smoke("point", 10, "#4A3A36", radius_px=40, delay=380, h=40),
    ], shake=(320, 0.015), note="the impact shake fires at +300 (combat-feel: at impact) — the recipe shake starts "
                                "with the cast; the runtime's impact hook may replace it")
    rec["skill.combustion"] = R("fire", [
        gather("target", 8, "rim", 30, 60, sprite="Ember"),
        flash("target", 34, "core", delay=60),
        impact_star("target", "heavy", 120, "mid", delay=60),
        glow("target", 140, 320, "rim", delay=60, grow=1.6, alpha=0.6),
        flames("target", 10, FLAME["fire"], radius_px=18, rise=(50, 90), size=(0.6, 0.9), delay=60),
        flames("target", 10, FLAME["fire"], radius_px=4, rise=(20, 40), size=(0.5, 0.7), delay=60, h=10,
               speedCmS=[220, 330], dir="flat"),
        shock("target", px(8), px(48), 380, "mid", delay=60),
        sparks("target", 10, "mid", delay=60),
        smoke("target", 4, "#4A3A36", delay=120, h=40),
        decal("target", "Scorch", 45, 1400, "#2E1E18", alpha=0.7, delay=60),
    ], shake=(140, 0.006))
    rec["skill.freeze"] = R("frost", [
        lay(sprite="Ring", orient="ground", at="target", height=3, sizeCm=round(110 / RING_R, 1), grow=0.2,
            sizePow=2, lifeMs=170, alpha=[0.9, 0.4], color="rim", intensity=1.2),
        motes("target", 8, "core", sprite="Flake", radius_px=40, speed=(30, 60), h=40, size=(8, 12)),
        flash("target", 26, "core", delay=140),
        decal("target", "Frost", 70, 1200, "mid", alpha=0.65, blend="additive", delay=140),
        lay(mesh="SM_FX_FrostCage", sprite="ShardFrost", orient="ground", blend="translucent", at="target",
            height=0, delayMs=140, sizeCm=mesh_size("SM_FX_FrostCage", 0.68), grow=round(0.8 / 0.68, 3), sizePow=6,
            lifeMs=660, alpha=[1, 1], color="#A9E6FF", intensity=1.05),
        lay(mesh="SM_FX_IceShard", sprite="ShardFrost", orient="velocity", blend="translucent", at="target",
            height=70, delayMs=780, count=10, spawnRadiusCm=35, spawnHeightJitterCm=90, dir="out", coneDeg=60,
            speedCmS=[160, 300], upCmS=[60, 180], gravityCmS2=700, lifeMs=[300, 450],
            sizeCm=[mesh_size("SM_FX_IceShard", 0.5), mesh_size("SM_FX_IceShard", 0.8)], grow=1, alpha=[1, 0],
            alphaPow=7, color="#DDF8FF"),
        glint("target", 36, delay=780),
        smoke("target", 4, "#CFEFFF", alpha=0.35, delay=780, h=40),
    ])
    R_at = 2.5 * 100
    rec["skill.arcane_torrent"] = R("arcane", [
        decal("point", "Rune", 0.9 * R_at, 900, "rim", alpha=0.6, blend="additive", spin=0.7, pow=1.6),
        # arcane missiles: a long violet comet body + a fat bright wisp head (40 cm heads read as droplets)
        lay(sprite="Comet", orient="velocity", at="point", rate=22, durationMs=540, spawnRadiusCm=0.85 * R_at,
            height=330, speedCmS=2200, dir="down", coneDeg=14, lifeMs=150, sizeCm=120, aspect=0.36, grow=1,
            alpha=[0.92, 0.92], color="rim", blend="translucent"),
        lay(sprite="Wisp", orient="velocity", at="point", rate=22, durationMs=540, spawnRadiusCm=0.85 * R_at,
            height=330, speedCmS=2200, dir="down", coneDeg=14, lifeMs=150, sizeCm=66, aspect=0.55, grow=1,
            alpha=[1, 1], color="mid", intensity=1.5),
        lay(emitter="flash", at="point", rate=22, durationMs=540, delayMs=150, spawnRadiusCm=0.85 * R_at,
            height=10, sizeCm=30, grow=2.4, lifeMs=160, color="core"),
        lay(sprite="Ring", orient="ground", at="point", rate=22, durationMs=540, delayMs=150,
            spawnRadiusCm=0.85 * R_at, height=3, sizeCm=round(10 / RING_R, 1), grow=4, sizePow=3, lifeMs=300,
            alpha=[0.9, 0], color="mid", intensity=1.2),
        shock("point", 40, 1.1 * R_at, 420, "rim", alpha=0.65, delay=250),
        motes("point", 12, "core", sprite="Spark", radius_px=0.8 * R_at / PX, rise=55, delay=150, size=(7, 11)),
    ])
    return rec


def rogue() -> dict:
    rec = {}
    arc_r = "SM_FX_SlashArc_Rogue"
    rec["skill.backstab"] = R("shadow", [
        ribbon(arc_r, "target", 70, 200, "rim", rot=-40, spin=320, h="chest", intensity=1.3),
        ribbon(arc_r, "target", 70, 200, "mid", rot=40, spin=-320, h="chest", delay=60, intensity=1.3),
        slash_sprite("target", 71, 200, "rim", cell=SLASH["rogue"], rot=-40),
        slash_sprite("target", 71, 200, "mid", cell=SLASH["rogue"], rot=40, delay=60),
        flash("target", 18, "mid", delay=30),
        glint("target", 44, delay=60, rot=45),
        sparks("target", 6, "mid"),
        drops("target", 6, "#E8342C"),
        smoke("target", 2, "#4A3A5A", h=40),
    ])
    rec["skill.poison_blade"] = R("poison", [
        gather("origin", 8, "mid", 34, 300),
        flames("origin", 12, FLAME["poison"], radius_px=24, on_ring=True, rise=(30, 60), delay=110, stagger=200,
               even=True,
               size=(0.45, 0.7)),
        ring("origin", px(10), px(44), 420, "rim"),
        # web glow 56 px around the body → a venom pool at the feet + glows at the blade hand (a chest-centred
        # glow is swallowed by the body it is anchored in)
        lay(emitter="glow", orient="ground", blend="translucent", at="origin", height=4, sizeCm=130, grow=1.2,
            lifeMs=500, alpha=[0.35, 0], fadeIn=0.1, color="rim"),
        lay(emitter="glow", orient="ground", at="origin", height=5, sizeCm=130, grow=1.2, lifeMs=500,
            alpha=[0.55, 0], fadeIn=0.1, color="mid"),
        glow("origin", 40, 420, "mid", h="hand", grow=1.4, alpha=0.8),
        drops("origin", 7, "rim", dir="down", cone=60, speed=(10, 30), gravity=420, h="hand"),
        motes("origin", 6, "mid", sprite="Bubble", rise=50, size=(8, 12), spin=0, h=10),
        decal("origin", "Puddle", px(16), 900, "rim", alpha=0.75, grow=1.3, delay=250),
    ])
    rec["skill.multishot"] = R("steel", [
        flash("origin", 20, "mid", h="hand"),
        lay(mesh="SM_FX_Arrow", sprite="Arrow", orient="velocity", blend="translucent", at="origin",
            height="hand", count=7, dir="blow", coneDeg=92, fan=True, speedCmS=1500, gravityCmS2=1360, lifeMs=220,
            sizeCm=mesh_size("SM_FX_Arrow"), grow=1, alpha=[1, 1], color="#EEF3FB", intensity=1.1),
        lay(emitter="streak", at="origin", height="hand", count=7, dir="blow", coneDeg=92, fan=True,
            speedCmS=1500, gravityCmS2=1360, lifeMs=200, sizeCm=40, aspect=0.15, stretch=0.02, color="#EEF3FB",
            alpha=[0.85, 0], intensity=1.3),
    ], note="7 arrows fanned evenly over ±46° (±0.8 rad) round the aim (`fan`: particle i of N at yaw "
            "cone·((i + .5) / N − .5); runtimes without it draw the random ±46° cone), 3.3 tiles in 220 ms (the web's "
            "fallback fan); gravity offsets the blow direction's random climb so the volley lands level")
    rec["skill.death_mark"] = R("shadow", [
        decal("target", "Rune", px(22), 1000, "rim", alpha=0.85, blend="additive", spin=-1.2, pow=1.6),
        gather("target", 10, "mid", 34, 300),
        # the skull sigil stamps down 18 cm over fx_overhead (clear of the head): a dark under-stroke (the same
        # skull 12 % larger in shadow dark) + the translucent shadow-mid body + a faint additive glow; the white
        # stamp flash is capped at α .5 so it never blooms into a featureless bulb
        lay(sprite="Skull", orient="billboard", blend="translucent", at="target", height="overhead",
            offsetCm=[0, 0, 18], sizeCm=108, grow=0.5, sizePow=6, lifeMs=1000, alpha=[0.95, 0], alphaPow=3,
            color="dark"),
        lay(sprite="Skull", orient="billboard", blend="translucent", at="target", height="overhead",
            offsetCm=[0, 0, 18], sizeCm=96, grow=0.5, sizePow=6, lifeMs=1000, alpha=[0.92, 0], alphaPow=3,
            color="mid"),
        lay(sprite="Skull", orient="billboard", at="target", height="overhead", offsetCm=[0, 0, 18], sizeCm=96,
            grow=0.5, sizePow=6, lifeMs=1000, alpha=[0.3, 0], alphaPow=3, color="mid", intensity=1.1),
        lay(sprite="Skull", orient="billboard", at="target", height="overhead", offsetCm=[0, 0, 18], sizeCm=60,
            grow=0.85, sizePow=2, lifeMs=200, alpha=[0.5, 0], alphaPow=1.5, color="white", intensity=1.0),
        lay(sprite="Ring", orient="ground", at="target", height=3, sizeCm=round(px(20) * 0.9 / RING_R, 1),
            grow=0.39, sizePow=2, lifeMs=360, alpha=[0.9, 0], color="rim", intensity=1.2),
        glint("target", 12, h="overhead", offsetCm=[0, 22, 46], delay=60),
        lay(emitter="motes", at="target", height="chest", count=6, orbit={"r0Cm": 49, "r1Cm": 49, "degS": 180},
            gravityCmS2=0, sizeCm=[6, 10], lifeMs=[800, 1000], color="mid"),
    ])
    rec["skill.piercing_arrow"] = R("holy", [
        glint("origin", 34, h="hand", color="mid"),
        lay(mesh="SM_FX_Arrow", sprite="Arrow", orient="velocity", blend="translucent", at="origin",
            height="hand", count=1, dir="blow", coneDeg=0, speedCmS=2300, gravityCmS2=1100, lifeMs=420,
            sizeCm=mesh_size("SM_FX_Arrow", 0.8), grow=1, alpha=[1, 0], alphaPow=8, color="#FFE27A",
            intensity=1.2),
        lay(sprite="Comet", orient="velocity", at="origin", height="hand", count=1, dir="blow", coneDeg=0,
            speedCmS=2300, gravityCmS2=1100, lifeMs=420, sizeCm=90, aspect=0.3, grow=1, alpha=[0.9, 0],
            alphaPow=6, color="mid", intensity=1.4),
        lay(emitter="streak", at="path", height="chest", count=14, dir="blow", coneDeg=4, speedCmS=300,
            lifeMs=[160, 240], sizeCm=26, color="mid", alpha=[0.7, 0]),
        *[ring("origin", 20, 70, 300, "rim", delay=int(d), offsetCm=[off, 0, 0], h=4)
          for off, d in ((150, 65), (300, 130), (450, 195))],
    ], note="the line (6 tiles) is drawn along the blow; three shock rings mark ¼ ½ ¾ as the arrow passes")
    rec["skill.poison_arrow"] = R("poison", [flash("origin", 12, "mid", h="hand", life=120)])
    rec["skill.explosive_trap"] = R("fire", [
        glow("origin", 60, 90, "rim", h=6),
        decal("origin", "Rune", px(18), 200, "rim", alpha=0.9, blend="additive", spin=4.0, pow=1.2),
    ])
    R_pc = 0.7 * 250
    rec["skill.poison_cloud"] = R("poison", [
        decal("point", "Puddle", 0.8 * R_pc, 1600, "rim", alpha=0.7, grow=1.2),
        lay(emitter="glow", orient="ground", at="point", height=4, sizeCm=2 * R_pc, grow=1.05, lifeMs=1300,
            alpha=[0.4, 0], color="mid"),
        # over the green plains the cloud needs value contrast, not just hue: a dark-green under-layer (a shadow /
        # outline mass under the billows), a light sickly yellow-green body and a pale chartreuse crown
        smoke("point", 10, "#1F4A18", radius_px=R_pc / PX, life=(900, 1300), alpha=0.62, size=(0.8, 1.1), h=20),
        smoke("point", 12, "#A6CC3E", radius_px=R_pc / PX, life=(900, 1300), alpha=0.86, size=(0.7, 1.0), h=34),
        smoke("point", 8, "#DCF06C", radius_px=0.8 * R_pc / PX, life=(900, 1300), alpha=0.78, size=(0.55, 0.8),
              delay=120, h=58),
        motes("point", 12, "core", sprite="Bubble", radius_px=0.7 * R_pc / PX, rise=40, size=(11, 16), spin=0,
              h=10, intensity=1.3),
        shock("point", 0.3 * R_pc, 1.2 * R_pc, 420, "mid", alpha=0.5),
    ])
    rec["skill.slow_trap"] = R("frost", [
        decal("origin", "Rune", px(16), 200, "rim", alpha=0.9, blend="additive", spin=5.0, pow=1.2),
    ])
    rec["skill.vanish"] = R("shadow", [
        flash("origin", 26, "mid"),
        ring("origin", 100, 10, 260, "rim", pow=2),
        # the smoke bomb ENVELOPS the rogue: puffs on a ring just outside the body (≥ 45 cm, so the camera-side
        # half always draws in front of it whatever the facing) from the boots to over the head, + a dense core
        smoke("origin", 14, "#3E2F52", radius_px=21, onRing=True, alpha=0.92, size=(0.75, 1.05), h=60,
              spawnHeightJitterCm=100, speed=(5, 25), rise=14),
        smoke("origin", 8, "#6A5A80", radius_px=16, onRing=True, alpha=0.85, size=(0.6, 0.85), h=95, delay=50,
              spawnHeightJitterCm=80, speed=(5, 20), rise=18),
        smoke("origin", 5, "#4A3A5E", radius_px=6, alpha=0.9, size=(0.9, 1.1), h=70, delay=20, speed=(0, 10)),
        sparks("origin", 10, "mid", alt="core"),
        decal("origin", "Scorch", 60, 1400, "#24103E", alpha=0.6),
    ])
    R_ar = 3.5 * 100
    rec["skill.arrow_rain"] = R("steel", [
        ring("point", 0.9 * R_ar, R_ar, 420, "rim", alpha=0.6, pow=1),
        decal("point", "Rune", R_ar, 600, "rim", alpha=0.35, blend="additive", spin=0.4),
    ])
    rec["skill.shadow_step"] = R("shadow", [
        smoke("origin", 5, "#4A3A5E", radius_px=10, alpha=0.8, h=40),
        ring("origin", 80, 8, 220, "mid", pow=2),
        lay(emitter="streak", at="path", height="chest", count=5, speedCmS=600, dir="blow", coneDeg=4,
            lifeMs=220, sizeCm=40, aspect=0.25, color="mid", alpha=[0.7, 0]),
        smoke("point", 5, "#4A3A5E", radius_px=10, alpha=0.8, h=40, delay=80),
        flash("point", 24, "mid", delay=80),
        ribbon(arc_r, "point", 65, 200, "mid", rot=-30, spin=300, delay=80),
        motes("point", 8, "mid", sprite="Spark", rise=50, delay=80, size=(7, 10)),
    ])
    rec["skill.chain_trap"] = R("holy", [
        flash("origin", 20, "mid", h=10),
        decal("origin", "Rune", px(18), 300, "rim", alpha=0.9, blend="additive", spin=3.0, pow=1.2),
    ])
    return rec


def sprite_table() -> dict:
    """The sprite table from ``sprites.json`` (written by fx_textures.py next to this file: no bpy needed)."""
    table = json.loads((HERE / "sprites.json").read_text())
    return {name: {"cells": int(s["cells"]), "blend": s["blend"], "texture": s.get("texture", f"T_FX_{name}"),
                   "note": s.get("note", "")} for name, s in sorted(table.items())}


def mesh_table() -> dict:
    p = EXPORT / "manifest.json"
    out = {}
    if p.exists():
        assets = json.loads(p.read_text()).get("assets", {})
        for name in MESH_DIAMETER:
            e = assets.get(name)
            if e and e.get("category") in ("VFX", "Pickups"):
                fx = e.get("fx") or {}
                out[name] = {"fbx": e.get("fbx"), "boundsDiameterCm": MESH_DIAMETER[name],
                             "orient": fx.get("orient", "billboard"), "pivot": fx.get("pivot"),
                             "defaultTint": fx.get("defaultTint"), "intensity": fx.get("intensity"),
                             "usedBy": fx.get("recipes")}
    return out


def palettes_json() -> dict:
    """fx_palettes.json (spec §8.2): element palettes, flame gradient maps (+ the Flame strip cell), impact /
    quality / affix / spirit colours."""
    data_dir = ART.parent / "Data"

    def hexi(v):
        return f"#{int(v):06X}"
    try:
        hf = json.loads((data_dir / "hit_feedback.json").read_text())
        class_impact = {k: hexi(v) for k, v in hf["classImpactColors"].items()}
        fallback = hexi(hf["fallbackImpactColor"])
    except (OSError, KeyError, ValueError):
        class_impact, fallback = {"warrior": "#FFD98A", "mage": "#C7A6FF", "rogue": "#9DFFC8"}, "#FFF2C0"
    try:
        sp = json.loads((data_dir / "spirit_profiles.json").read_text())["profiles"]
        spirit = {k: hexi(v["visualColor"]) for k, v in sp.items()}
    except (OSError, KeyError, ValueError):
        spirit = {}
    try:
        ea = json.loads((data_dir / "elite_affixes.json").read_text())["definitions"]
        affix = {k: hexi(v["vfxColor"]) for k, v in ea.items()}
    except (OSError, KeyError, ValueError):
        affix = {}
    return {
        "schemaVersion": 1,
        "source": "art-inventory-ch1.md §8.2 (src/graphics/vfx/FxKit.ts PAL, FxTextures.ts:241-247)",
        "palettes": {k: {"core": c, "mid": m, "rim": r, "dark": d, "flameCell": f}
                     for k, (c, m, r, d, f) in PAL.items()},
        "flameGradients": {
            "fire": ["#E8401A", "#FF9A2A", "#FFF0B0"], "holy": ["#F0A020", "#FFE070", "#FFFBE8"],
            "poison": ["#2E9A2A", "#8EE83A", "#EAFFB8"], "shadow": ["#5A1A9A", "#A05CF0", "#F0D8FF"],
            "rage": ["#B01818", "#FF4A2A", "#FFC8A0"], "frost": ["#2A86D8", "#8EE6FF", "#FFFFFF"],
            "plain": ["#B8B8B8", "#E8E8E8", "#FFFFFF"],
            "camp": {"base": "#FF8800", "outer": "25% → #B01810", "mid": "55% → #FFB030", "inner": "85% → #FFF0A0",
                     "line": "50% → #400808"},
            "note": "outer → mid → inner; baked as the pre-coloured cells of T_FX_Flame (flameCell)",
        },
        "flameCells": FLAME,
        "classImpactColors": class_impact,
        "fallbackImpactColor": fallback,
        "skillImpactColors": {"fire": "#FF6600", "meteor": "#FF6600", "combustion": "#FF6600", "ice": "#4488FF",
                              "blizzard": "#4488FF", "freeze": "#4488FF", "lightning": "#5DADE2",
                              "poison": "#7ED957", "arcane_torrent": "#B07CFF", "physical": "#F1C40F",
                              "other": "#F39C12"},
        "lootQualityColors": {"normal": "#C8C8C8", "magic": "#4F8CFF", "rare": "#FFD84A", "legendary": "#FF8A2A",
                              "set": "#3ECF6A"},
        "eliteAffixColors": affix,
        "spiritColors": spirit,
        "deathBurstColors": {"monster": "#FF4444", "hero": "#CC2222", "escort": "#E67E22", "soulEcho": "#7FD8FF"},
        "statusTints": {"burn": ["#FF7A3A", 0.35], "freeze": ["#7FD0FF", 0.45], "stun": ["#7FD0FF", 0.45],
                        "poison": ["#8FE04A", 0.35]},
        "storyEmbers": {"embers": "#FF9A3C", "night": "#9FB8FF", "abyss": "#D24CFF", "dawn": "#FFD08A",
                        "forge": "#FFB040", "sand": "#FFD79A", "light": "#FFF1C8"},
    }


STRIKE_CM = -22.0
"""Body-anchored bursts sit at the target's fx_chest socket = the body's centre, where the opaque body hides them
(the sprites are depth-tested; the web drew FX over its sprites). Strike recipes pull their target-side burst
layers 22 cm back along the blow — onto the struck surface, toward the attacker (integration.bodyOcclusion)."""
STRIKE_IDS = ("hit.impact", "hit.basic", "hit.claw", "hit.evade", "skill.slash", "skill.bleed_strike",
              "skill.lethal_strike", "skill.charge", "skill.backstab", "skill.combustion", "skill.freeze",
              "skill.chain_lightning", "skill.death_mark", "skill.shadow_step")
STRIKE_SPRITES = {"Impact", "Core", "Glow", "Spark", "Streak", "Slash", "Claw", "Drop", "Star", "Ember", "Shard",
                  "ShardFrost", "Flake"}


STRIKE_CM_HERO = -42.0
"""Bursts on the HERO (claw rakes, evades): the warrior's armoured bulk is ~40 cm deep, so they sit in front of the
hero's struck side — between the hero and the attacker."""


def strike_offsets(recipes: dict) -> None:
    def is_strike(rid):
        return any(rid == s or rid.startswith(s + ".") for s in STRIKE_IDS) or \
            (rid.startswith("proj.") and rid.endswith(".hit"))
    for rid, r in recipes.items():
        if not is_strike(rid):
            continue
        for L in r["layers"]:
            if L.get("at") not in ("target", "point") or L.get("orient") in ("ground", "upright", "beam"):
                continue
            h = L.get("height", "chest")
            if not (h in ("chest", "head") or (isinstance(h, (int, float)) and h >= 30)):
                continue
            sprite = L.get("sprite") or {"glow": "Glow", "flash": "Core", "glint": "Spark", "sparks": "Streak",
                                         "streak": "Streak", "slash": "Slash", "motes": "Ember"}.get(L.get("emitter"))
            if L.get("mesh") or sprite not in STRIKE_SPRITES or L.get("orbit"):
                continue
            off = list(L.get("offsetCm", [0.0, 0.0, 0.0]))
            d = STRIKE_CM_HERO if rid in ("hit.claw", "hit.evade", "proj.monster_bolt.hit") else STRIKE_CM
            if off[0] > d:
                off[0] = round(min(off[0], 0.0) + d, 1)
                L["offsetCm"] = off


CAM_BIAS_CM = 40.0
"""Blow-less events (level-up, pickups, statuses, loot, portal, resonance, world props: UAbyssVfxSystem leaves
Context.Blow zero → the blow frame is UE +X / +Y) have a FIXED frame, and the W1 camera's yaw is fixed, so an offset
in that frame can point straight down the view ray: d·(−0.455, −0.455, +0.766) moves a quad d cm toward the camera
with the same screen position (2–3 % larger) — it draws over the body it is anchored on instead of inside it."""
BLOWLESS_IDS = ("hero.levelup", "reward.heal", "reward.mana", "hero.resonance", "portal.", "loot.", "quest.pop",
                "status.", "elite.", "prop.", "camp.", "exit.")


def camera_bias(recipes: dict) -> None:
    k = CAM_BIAS_CM
    fx = (round(-0.4548 * k, 1), round(-0.4548 * k, 1), round(0.766 * k, 1))
    for rid, r in recipes.items():
        if not any(rid == s or rid.startswith(s) for s in BLOWLESS_IDS):
            continue
        for L in r["layers"]:
            if L.get("at", "origin") not in ("origin", "target", "point") or \
                    L.get("orient") in ("ground", "beam") or L.get("emitter") in ("ring", "shock", "decal"):
                continue
            h = L.get("height", "ground")
            if not (h in ("chest", "head", "hand", "overhead") or (isinstance(h, (int, float)) and h >= 25)):
                continue
            if L.get("mesh") and L.get("orient") == "upright":
                continue
            off = list(L.get("offsetCm", [0.0, 0.0, 0.0]))
            L["offsetCm"] = [round(off[0] + fx[0], 1), round(off[1] + fx[1], 1), round(off[2] + fx[2], 1)]


GROUND_LIFT_CM = 7.0
GROUND_MIN_CM = 8.0
PRESET_ORIENT = {"ring": "ground", "shock": "ground", "decal": "ground", "swipe": "ground", "flames": "upright",
                 "beam": "upright", "streak": "velocity", "sparks": "velocity", "bolt": "beam"}
PRESET_HEIGHT = {"ring": 3.0, "shock": 2.0, "decal": 1.0, "swipe": "chest", "gather": "chest", "bolt": "chest"}


def ground_lift(recipes: dict) -> None:
    """Sprite quads lying on the ground (orient ``ground``: rings, shocks, decals, runes, pools, auras) sit 8–10 cm
    over the terrain instead of 1–3 cm. M_AF_FX_Sprite fades every quad by DepthFade(8 cm) (Scripts/abyss_content/
    ue/materials.py) and the runtime has no ground special case, so at 1–3 cm they drew at 10–30 % strength; from
    6.2 cm (8 cm of view depth at the W1 pitch, 8 / sin 50° · sin 50°… = h / 0.766 along the ray) they are whole.
    The relative order of stacked ground layers is kept (+7 cm each, at least 8 cm); FX meshes (opaque, no fade)
    and ground-orient quads anchored higher (chest swipes, ribbons) are untouched."""
    for r in recipes.values():
        for L in r["layers"]:
            if L.get("mesh"):
                continue
            orient = L.get("orient", PRESET_ORIENT.get(L.get("emitter"), "billboard"))
            if orient != "ground":
                continue
            h = L.get("height", PRESET_HEIGHT.get(L.get("emitter"), "ground"))
            if h in ("ground", "feet"):
                h = 0.0
            if not isinstance(h, (int, float)) or h >= GROUND_MIN_CM:
                continue
            L["height"] = round(max(GROUND_MIN_CM, float(h) + GROUND_LIFT_CM), 1)


def prewarm(recipes: dict) -> None:
    """Loops start empty: a rate emitter's first particle appears after 1000 / rate ms (a status ring at 1.2 /s
    shows nothing for 0.8 s). Every loop layer slower than 3 /s gets a one-particle burst copy at its start."""
    for r in recipes.values():
        extra = []
        for L in r["layers"]:
            if 0 < L.get("rate", 0) < 3 and L.get("durationMs", -1) < 0 and not L.get("delayMs"):
                b = {k: v for k, v in L.items() if k not in ("rate", "durationMs")}
                b["count"] = 1
                extra.append(b)
        r["layers"].extend(extra)


DITHER_MAX_MS = 34.0
"""M_AF_FX_Mesh fades by a 4 × 4 Bayer screen-door (thresholds (b + .5) / 16: any α < 31/32 drops pixels) that only
TAA / TSR resolves, and mobile low has no TAA. Every mesh layer's dithered window (fade-in + late dissolve) is held to
two 60 Hz frames."""
SHRINK_OUT = ("SM_FX_Rock_A", "SM_FX_Rock_B", "SM_FX_IceShard")
MESH_PRESET_FADE = {"debris": ([1.0, 0.0], 5.0, [420, 560])}     # AbyssVfxRecipe.cpp ApplyPreset


def mesh_dissolve(recipes: dict) -> None:
    """FX meshes are masked, so a dissolve is a screen-door, not a fade (ice armor's shatter, the mana-shield bubble,
    whirlwind / rampage bands showed ~90–190 ms of checker noise at 1:1). Small flying pieces (rock debris, ice
    shards) keep α 1 and shrink out instead (the coins' late shrink: grow × .15, sizePow .3); every other mesh
    layer's alphaPow is raised until α < 31/32 lasts at most ``DITHER_MAX_MS`` of its longest life, and a fade-in
    is cut to the same window (α = α0 − (α0 − α1)·U^pow, U = (T − fadeIn) / (1 − fadeIn), AbyssVfxSystem.cpp)."""
    thr = 31.0 / 32.0          # AF_FxMeshMask keeps a pixel while α > (b + .5) / 16: the first one drops below 15.5/16
    for r in recipes.values():
        for L in r["layers"]:
            mesh = L.get("mesh")
            if not mesh:
                continue
            da, dp, dl = MESH_PRESET_FADE.get(L.get("emitter"), ([1.0, 0.0], 1.0, [300, 300]))
            alpha = L.get("alpha", da)
            a0, a1 = (float(alpha[0]), float(alpha[-1])) if isinstance(alpha, list) else (float(alpha), 0.0)
            life = L.get("lifeMs", dl)
            lmax = float(max(life) if isinstance(life, list) else life)
            fi = float(L.get("fadeIn", 0.0))
            if fi * lmax > DITHER_MAX_MS:
                fi = round(DITHER_MAX_MS / lmax, 3)
                L["fadeIn"] = fi
            if not (a1 < a0 and a0 >= thr):
                continue
            grow, size_pow = float(L.get("grow", 1.0)), L.get("sizePow")
            if mesh in SHRINK_OUT and grow <= 1.0 and size_pow is None:
                L["alpha"] = [a0, a0]
                L.pop("alphaPow", None)
                L["grow"] = round(grow * 0.15, 3)
                L["sizePow"] = 0.3
                if "lifeMs" not in L:
                    L["lifeMs"] = list(dl)
                continue
            pw = float(L.get("alphaPow", dp))
            frac = (a0 - thr) / (a0 - a1)
            span = (1.0 - fi) * lmax
            need = math.log(frac) / math.log(1.0 - min(DITHER_MAX_MS / span, 0.99))
            if need > pw:
                L["alphaPow"] = round(need + 0.05, 1)


def build() -> dict:
    load_diameters()
    recipes = {}
    for group in (combat, utility, world, projectiles, warrior, mage, rogue):
        recipes.update(group())
    strike_offsets(recipes)
    camera_bias(recipes)
    ground_lift(recipes)
    prewarm(recipes)
    mesh_dissolve(recipes)
    doc = {
        "schemaVersion": 1,
        "_doc": "Abyssfire VFX recipes, art pass (Art/blender/vfx/fx_recipes.py). Schema: Source/Abyssfire/Public/"
                "World/WorldContract.md 5 / AbyssVfxRecipe.h; merged over the runtime's built-in defaults (same id "
                "replaces). Textures T_FX_<sprite> are linear RGBA (sRGB off): RGB tints, A coverage; the Flame, "
                "ShardFrost, Arrow, Coin, Rock and Chain cells are pre-coloured (white tint). Meshes SM_FX_* are drawn "
                "by M_AF_FX_Mesh (one tint per particle, toon bands, ink hull, AF_Data.a value channel).",
        "palettes": {k: {"core": c, "mid": m, "rim": r, "dark": d} for k, (c, m, r, d, _) in PAL.items()},
        "sprites": sprite_table(),
        "meshes": mesh_table(),
        "recipes": dict(sorted(recipes.items())),
        "aliases": {"proj.multishot": "proj.arrow", "proj.multishot.hit": "proj.arrow.hit",
                    "loot.drop.magic": "loot.drop"},
        "integration": {
            "groundSoftFade": "M_AF_FX_Sprite fades every quad with DepthFade(8 cm) and the runtime has no ground "
                              "special case, so every ground-oriented sprite quad (rings, shocks, decals, runes, pools, "
                              "auras) is authored 8-10 cm over the terrain (fx_recipes.ground_lift; full strength from "
                              "6.2 cm at the W1 pitch). The previews draw exactly the shipped 8 cm fade on every quad. "
                              "The runtime's fake light pools (UAbyssVfxSystem lights, +2 cm) and the prop pools "
                              "(AAbyssPropActor, M_AF_LightPool) still sit at 2 cm and draw at ~25 %: lift them to 8 "
                              "cm too (or skip DepthFade for ground quads / M_AF_LightPool).",
            "fan": "Layer key `fan` (bool, dir blow / back): particle i of a burst of N flies at yaw cone*((i + 0.5) / "
                   "N - 0.5) round the axis instead of a random yaw in the cone - an even fan (skill.multishot: 7 "
                   "arrows over the web's +-0.8 rad). Needs FAbyssVfxLayer::bFan + ReadBool(\"fan\") + the Serial-"
                   "based yaw in SpawnParticle; until then the runtime ignores the key (random cone).",
            "dirBlowPitch": "EAbyssVfxDir::Blow adds Direction.Z = rand(-0.15, 0.35): flight-aligned heads use one "
                            "particle per projectile (count 1, life = the longest flight) so their pitch is stable; "
                            "a `flat` option (Z = 0) would let heads respawn every frame and fans fly level.",
            "meshMaterial": "mesh layers always draw M_AF_FX_Mesh (opaque masked toon + ink, Bayer fade) — glow, "
                            "smoke and energy stay sprites; ribbons / crystals / plates / debris / heads are meshes.",
            "bodyOcclusion": "Sprites are depth-tested against the opaque characters, and a chest / head anchor is the "
                             "fx_chest / fx_head socket = the middle of the body, so a burst centred there loses "
                             "everything inside the silhouette (the web drew FX over its sprites). The strike recipes "
                             "pull their target-side burst layers 22 cm back along the blow (onto the struck surface, "
                             "fx_recipes.STRIKE_CM); a burst struck from behind the target (attacker farther from the "
                             "camera) is still partly hidden. Recommended runtime fix: billboard / velocity layers "
                             "anchored on an actor (origin / target / point at chest, head, hand or overhead) move "
                             "their quad toward the camera by min(40 cm, 0.5 x the actor's capsule radius + 20 cm) "
                             "(Position -= CameraForward * Bias in UAbyssVfxSystem::Tick, or a camera-ward WPO in "
                             "M_AF_FX_Additive / _Translucent) - at the W1 distance that is <2 % size change.",
            "cameraBias": "Blow-less recipes (level-up, pickups, statuses, loot, portal, resonance, props, camps) "
                          "carry offsetCm d*(-0.455, -0.455, +0.766) with d = 40 cm on their body-anchored quads: "
                          "with the zero blow the frame is world +X/+Y and the W1 yaw is fixed, so that is the "
                          "view ray toward the camera (same screen position, drawn over the body). Re-derive "
                          "(fx_recipes.CAM_BIAS_CM) if the camera yaw ever changes or these events get a blow.",
            "loopPrewarm": "Rate emitters start empty (first particle after 1000 / rate ms). Loop layers slower than "
                           "3 /s carry a one-particle burst copy (count 1) so a status / prop loop shows at once "
                           "(fx_recipes.prewarm).",
            "perClassBasic": "hit.basic.<class> and hit.impact.<profile> are authored for a one-line runtime change "
                             "(PlayFirst({hit.basic.<class>, hit.basic}) / ({hit.impact.<profile>, hit.impact})).",
            "elite": "elite.<affix> and camp.hearthstone are forward-compatible ids (not bound to events yet).",
            "lootBeam": "loot.beam.<magic|rare|legendary|set> are LOOPS for the loot prop (WorldContract 5.3: add a "
                        "'GroundItem prop loop' row): in UAbyssWorldBuilder::SpawnEntity's GroundItem branch set "
                        "LoopRecipe = loot.beam.<quality> for quality >= Magic (Normal: none). The prop's vfx:<handle> "
                        "tag already stops it in RemoveEntity on pickup / despawn. loot.drop.* stay the drop-moment "
                        "burst; the 31 cm PoolRadiusCm light pool can stay (the beam carries its own pool).",
            "clueYaw": "SM_FX_ClueMagnifier's lens is tilted back 40 deg and faces the W1 camera at mesh yaw camera + 90 "
                       "(UE 135 deg; n.v = 0.985); the random Pickup yaw (AbyssWorldBuilder.cpp Setup.YawDeg = TileHash "
                       "% 360) turns it edge-on. For DefId quest_clue set Setup.YawDeg = CameraYawDeg + 180 (= 225; "
                       "AAbyssPropActor adds MeshYawOffsetDeg -90 -> mesh yaw 135) + a hash jitter of at most +-8 deg.",
            "chestOpen": "prop.chest.open is timed from the start of the chest's Open clip (its layers wait for the "
                         "ChestOpen notify, 120 ms); nothing plays it yet - play it where the chest prop starts Open "
                         "(UAbyssWorldBuilder::RemoveEntity, PickedUp / Collected of a Chest-style prop).",
            "meshDissolve": "FX mesh layers fade by a 4x4 Bayer screen-door (M_AF_FX_Mesh opacity mask) that TAA / TSR "
                            "resolves; without TAA (mobile low) any alpha < 31/32 shows as checker noise. The recipes "
                            "now hold every mesh layer's dithered window (fade-in + late dissolve) to <= 34 ms = 2 frames "
                            "at 60 Hz (fx_recipes.mesh_dissolve raises alphaPow / cuts fadeIn; at alphaPow 10-12 the "
                            "shatter / bubble / band dissolves still dithered for 90-190 ms), and rock debris / ice "
                            "shards no longer dissolve at all (alpha 1, late shrink: grow x .15, sizePow .3). Optional "
                            "runtime polish: on tiers without TAA use a plain alpha-test at 0.5 instead of the dither.",
            "launchSocket": "Projectiles and `hand` layers resolve fx_hand_r. The mage holds the staff there: at Cast01's "
                            "release the staff crystal is 73 cm further along the cast. Spec 8.1 launches from the "
                            "crystal: AAbyssCharacterActor::GetAnchorLocation(HandR) returns the main-hand mesh's "
                            "`crystal` socket when it has one (the previews draw it so).",
            "impactShake": "meteor / fireball shakes belong to the impact (combat-feel 11.5); recipe shakes start "
                           "with the recipe, so the impact recipes (proj.fireball.hit) carry them.",
        },
    }
    return doc


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    doc = build()
    (OUT / "vfx_recipes.json").write_text(json.dumps(doc, indent=1, ensure_ascii=True) + "\n")
    (OUT / "fx_palettes.json").write_text(json.dumps(palettes_json(), indent=1, ensure_ascii=True) + "\n")
    n = len(doc["recipes"])
    layers = sum(len(r["layers"]) for r in doc["recipes"].values())
    print(f"vfx_recipes.json: {n} recipes, {layers} layers; fx_palettes.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
