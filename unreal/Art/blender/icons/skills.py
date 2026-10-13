"""Skill icons (spec §9.2, DECISIONS R10): the 40 skill emblems of ``src/graphics/icons/SkillIcons.ts`` as Blender
renders — each motif modelled in 3-D (``motifs.py``; the shipped weapon meshes for blades, axes and shields)
under the icon key light with the web ink (2.1 u, motif scale 0.9), on the web's emblem plate.

Two textures per skill (the UI frame — dark iron stroke, element hairline, bevel — is NOT baked; Slate draws it):
* ``T_UI_SkillIcon_<id>``  — the emblem: rounded-square plate (inset 2/96, r 15/96, transparent outside) with the
  damage-type / tree backdrop (radial mid → deep, 12 faint rays, edge vignette) and the inked motif;
* ``T_UI_SkillMotif_<id>`` — the inked motif alone on transparency (round touch buttons, tooltips, VFX cards).

Run:  EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/skills.py [--only id,id]
"""
from __future__ import annotations

import json
import math
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import bpy  # noqa: E402,F401  (bpy first: it provides mathutils)
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import iconkit as K  # noqa: E402
import item_models as IM  # noqa: E402
import motifs as F  # noqa: E402
import shapes as S  # noqa: E402
from item_models import W, Wp, U  # noqa: E402
from kit import color, mesh as M, paths, pngio  # noqa: E402
from motifs import R, at, pill  # noqa: E402

DATA = paths.UNREAL_ROOT / "Data"
GROUP_DIR = "Icons/Skills"
MOTIF_SCALE = 0.9
INK_U = 2.1 * MOTIF_SCALE              # web: inkWidth 2.1 u in motif space, motif scaled 0.9

ELEMENT_BG = {"fire": ("#D0501C", "#3A0A06", "#FFB060"), "ice": ("#2E8ED6", "#081A3A", "#AEEAFF"),
              "lightning": ("#6A52E6", "#120C3A", "#D8D0FF"), "poison": ("#4F9C22", "#0A2008", "#C8FF80"),
              "arcane": ("#9A3AD6", "#1E0838", "#F0B0FF")}
TREE_BG = {"combat_master": ("#B4742E", "#2A1206", "#FFD890"), "guardian": ("#3A6EAA", "#0A1428", "#B8D8FF"),
           "berserker": ("#A82A26", "#280606", "#FFA080"), "assassination": ("#5E3C90", "#120822", "#D8B8FF"),
           "archery": ("#3A8C5A", "#08200E", "#C0F0B0"), "traps": ("#A47C2A", "#221806", "#FFE0A0")}

STEEL, GOLD, BONE, WOOD = "#D0D8E2", "#E8B848", "#ECE2C6", "#9A6A3A"


def U9(x, y):
    """web motif units → final icon units (motif scaled 0.9 about the centre)."""
    return (48 + (x - 48) * MOTIF_SCALE, 48 + (y - 48) * MOTIF_SCALE)


def sparkle(x, y, r=5.0, hex="#FFFFFF", alpha=1.0):
    return K.Accent("sparkle", u=U9(x, y), r_u=r * MOTIF_SCALE, hex=hex, alpha=alpha)


def glow(x, y, r, hex, alpha, under=False, core=0.0):
    return K.Accent("glow", u=U9(x, y), r_u=r * MOTIF_SCALE, hex=hex, alpha=alpha, under=under, core=core)


def move(objs, scale=1.0, at_=(0, 0, 0), rot_deg=0.0):
    """Scale / turn (in the screen plane, + = clockwise) / move already placed objects about the world origin."""
    m = Matrix.Translation(Vector(at_)) @ Matrix.Rotation(math.radians(rot_deg), 4, "Y") @ Matrix.Scale(scale, 4)
    for o in objs:
        o.matrix_world = m @ o.matrix_world
    return objs


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Motifs (web MOTIFS, SkillIcons.ts :283-853)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def m_slash(c):
    F.weapon(c, "sword", "T2", 30, 72, math.pi / 4, 1.25, d=2, widen=1.6)
    F.swoosh(c, 44, 60, 38, math.pi * 1.05, math.pi * 1.95, 13, d=-8)
    c.acc.append(sparkle(70, 24, 5))


def m_whirlwind(c):
    for i in range(3):
        a = i / 3 * math.pi * 2
        F.swoosh(c, 48, 48, 34 - i * 2, a, a + math.pi * .85, 11, "#FFF0C8" if i == 0 else "#E8D8B8", d=-2 - i,
                 tag=f"sw{i}")
    F.weapon(c, "sword", "T4", 40, 62, math.pi / 5, .8, d=-10, widen=1.6)
    c.acc.append(sparkle(70, 24, 5))


def m_war_stomp(c):
    for rx, w in ((30, 3), (40, 2.2), (48, 1.4)):
        F.ring2d(c, 48, 78, rx, rx * .32, w * 1.2, "#FFDC8C", d=6, tag=f"wave{rx}", e=.5)
    c.add(pill(S.circle2d(40, 40, 48, 78, rz=13), depth=5, d=0, edge=.5), "ground", R("#8A6A48", .42, .35))
    for pts in ([(48, 78), (30, 84), (16, 80)], [(48, 78), (66, 86), (80, 82)], [(48, 78), (52, 90)],
                [(48, 78), (40, 70), (26, 70)]):
        c.add(pill(S.ribbon2d(pts, lambda t: .9 * (1 - .6 * t) + .2), depth=.8, d=-5.3, edge=.9), "crack",
              R("#2A1A10", .2, .05))
    for (x, y, r) in ((22, 60, 4), (74, 58, 3.5), (30, 50, 2.6), (68, 46, 2.4)):
        c.add(S.lump((r * U, r * .8 * U, r * U), seed=int(x * 7 + y), noise=.2, center=at(x, y, -3)), "debris",
              R("#8A6A48", .42, .4))
    m = IM.boots("plate")
    for o in m.objs:
        c.obj(o)
    move(m.objs, .62, at(54, 42, -8))


def m_shield_wall(c):
    for x in (24, 72):
        F.weapon(c, "shield", "T2", x, 50, 0.0, .78, d=10)
    F.weapon(c, "shield", "T4", 48, 46, 0.0, 1.05, d=-6)
    c.acc.append(sparkle(34, 20, 5))


def m_taunt_roar(c):
    for s in (-1, 1):
        for i in range(3):
            a0, a1 = (153, 207) if s < 0 else (-27, 27)
            F.ring2d(c, 48, 62, 32 + i * 6, 32 + i * 6, 3.4 - i * .6, "#FFC878", d=6, tag=f"snd{i}", a0=a0, a1=a1, e=.5 - i * .1)
    helm = S.chain2d(S.bez([(24, 78), (18, 38), (30, 16), (48, 16)], 12), S.bez([(48, 16), (66, 16), (78, 38), (72, 78)], 12))
    c.add(pill(helm, depth=11, d=0, edge=.55, dome=.4), "helm", R("#B8C2CF", .42, .5))
    tee = [(30, 40), (66, 40), (66, 48), (55, 48), (58, 80), (38, 80), (41, 48), (30, 48)]
    c.add(pill(tee, depth=1.2, d=-10.2, edge=.85), "slot", R("#1A1018", .2, .05))
    c.add(pill(S.circle2d(7, 20, 48, 66, rz=10), depth=1.8, d=-11.3, edge=.7), "mouth", R("#C02030", .35, .35))
    c.add(pill(S.rrect2d(10, 2.4, 1.0, 2, 48, 58.2), depth=1, d=-13, edge=.9), "teeth", R("#FFF4E0", .2, .5))
    for sx in (-1, 1):
        c.add(pill(S.circle2d(3, 14, 48 + sx * 11, 44, rz=1.6), depth=.8, d=-11.4, edge=.9), "eye", R("#FFC040", .2, .5, e=.8))
    for pts in ([(26, 34), (10, 30), (8, 12)], [(70, 34), (86, 30), (88, 12)]):
        path = S.bez(pts, 12)
        c.add(pill(S.ribbon2d(path, lambda t: 5 * (1 - t) + .4), depth=4, d=2, edge=.7, dome=.4), "horn", R(BONE, .42, .4))
    c.acc += [glow(37, 44, 6, "#FFC040", .6), glow(59, 44, 6, "#FFC040", .6)]


def m_vengeful_wrath(c):
    F.flame(c, 30, 64, 50, 12, -.25, d=12, tag="fa")
    F.flame(c, 66, 64, 50, 12, .25, d=12, tag="fb")
    F.flame(c, 48, 56, 56, 14, 0, d=6, tag="fc")
    skin = R("#D89A6A", .42, .38)
    c.add(pill(S.rrect2d(36, 34, 9, 4, 48, 57), depth=9, d=-2, edge=.5), "fist", skin)
    for i in range(4):
        c.add(pill(S.circle2d(5, 16, 35 + i * 8.6, 42, rz=6), depth=6, d=-6, edge=.6), "fist")
    c.add(pill(S.capsule2d((30, 60), (52, 56), 5, 4.4), depth=5, d=-9, edge=.6), "fist")
    c.add(pill(S.rrect2d(32, 16, 3, 3, 48, 80), depth=7, d=0, edge=.5), "cuff", R("#8A3A24", .42, .32))
    c.acc.append(glow(48, 44, 30, "#FF7A2A", .35))


def m_charge(c):
    F.speed_lines(c, [((6, 30), (30, 30)), ((4, 46), (26, 46)), ((8, 62), (30, 62)), ((14, 76), (34, 76))], 1.3)
    for (x, y, r) in ((18, 82, 7), (8, 74, 5), (28, 88, 5)):
        p = M.ellipsoid((r * U, r * .6 * U, r * U), (0, 0, 0), sides=16, rings=8)
        p.translate(at(x, y, -2))
        c.add(p, "dust", R("#C8B090", .42, .4))
    F.weapon(c, "shield", "T3", 58, 48, .18, 1.0, d=0)
    c.add(pill([(74, 44), (92, 48), (74, 52)], depth=3, d=0, edge=.6, dome=.6), "spike", R(STEEL, .4, .55))


def m_lethal_strike(c):
    F.star(c, 48, 78, 10, 16, 6, "#FF3030", d=8, tag="imp", e=.5)
    F.star(c, 48, 78, 10, 10, 4, "#FFE0A0", d=5, tag="imp2", e=.8)
    F.weapon(c, "sword", "T4", 48, 31, math.pi, .94, d=-2, widen=1.6)
    c.acc += [glow(48, 78, 16, "#FF4040", .5), sparkle(58, 40, 6)]


def m_dual_wield_mastery(c):
    F.weapon(c, "sword", "T3", 28, 72, math.pi / 4, 1.2, d=2, widen=1.6)
    F.weapon(c, "sword", "T2", 68, 72, -math.pi / 4, 1.2, d=-4, spin=12, widen=1.6)
    c.acc.append(sparkle(48, 32, 6))


def m_iron_fortress(c):
    st = R("#8A94A8", .42, .45)
    tower = [(22, 88), (24, 30), (20, 30), (20, 16), (30, 16), (30, 22), (38, 22), (38, 16), (58, 16), (58, 22),
             (66, 22), (66, 16), (76, 16), (76, 30), (72, 30), (74, 88)]
    body = pill(tower, depth=10, d=0, edge=.25, dome=.25)
    c.add(body, "stone", st)
    surf = S.Surface(body)
    for y in range(36, 90, 10):
        c.add(IM.drape_strip(surf, [W(19, y), W(79, y)], .006, .004), "mortar", R("#5A6274", .3, .1))
        x = 24 + ((y // 10) % 2) * 8
        while x < 76:
            c.add(IM.drape_strip(surf, [W(x, y), W(x, y + 10)], .006, .004), "mortar")
            x += 16
    door = S.chain2d([(38, 88), (38, 64)], S.bez([(38, 64), (48, 52), (58, 64)], 10), [(58, 88)])
    p, _ = surf.at(*W(48, 75))
    c.add(pill(door, depth=2, d=p.y / U - .5, edge=.6), "door", R("#4A5060", .42, .3))
    for x in (42, 48, 54):
        c.add(pill(S.capsule2d((x, 62), (x, 88), .6), depth=.6, d=p.y / U - 2.4, edge=.9), "plank", R("#20242C", .2, .05))
    surf.free()
    F.weapon(c, "shield", "T4", 48, 43, 0.0, .40, d=-11)


def m_unyielding(c):
    hp = F.heart_pts(48, 46, 1.25)
    heart = pill(hp, depth=12, d=0, edge=.5, dome=.4)
    c.add(heart, "heart", R("#D02838", .42, .4))
    surf = S.Surface(heart)
    for y in (39.5, 61.5):
        c.add(IM.drape_strip(surf, [W(10, y), W(86, y)], .036, .014), "band", R("#9AA4B4", .42, .5))
        for x in range(22, 80, 12):
            c.add(S.dome_on(surf, *W(x, y), .016, .012, lift=.014), "rivet", R("#E8EEF4", .3, .6))
    crack = [(48, 22), (44, 34), (52, 46), (46, 56), (50, 72)]
    c.add(IM.drape_strip(surf, Wp(crack), .012, .008, step=.006), "seal", R(GOLD, .35, .55, e=.35))
    surf.free()
    c.acc += [glow(48, 50, 44, "#FFD060", .55, under=True)]


def m_life_regen(c):
    c.add(pill(F.heart_pts(44, 48, 1.1), depth=11, d=0, edge=.5, dome=.4), "heart", R("#E0303C", .42, .45))
    g = R("#5AD04A", .35, .5, e=.2)
    c.add(pill(S.rrect2d(8, 26, 2.5, 3, 66, 67), depth=4, d=-10, edge=.6), "plus", g)
    c.add(pill(S.rrect2d(26, 8, 2.5, 3, 66, 67), depth=4, d=-10, edge=.6), "plus")
    for (x, y, r) in ((24, 18, -.6), (70, 20, .6)):
        leaf = F.xform2d(S.blob2d([(0, -8), (4.5, 0), (0, 8), (-4.5, 0)], 6), x, y, r)
        c.add(pill(leaf, depth=3, d=-2, edge=.6), "leaf", R("#5AB04A", .42, .42))
    c.acc += [sparkle(16, 60, 4, "#D8FFC0"), sparkle(82, 40, 5, "#D8FFC0"), sparkle(48, 10, 4, "#D8FFC0")]


def m_frenzy(c):
    F.flame(c, 48, 88, 84, 30, 0, d=14)
    mask = S.chain2d(S.bez([(26, 44), (24, 20), (72, 20), (70, 44)], 14), [(66, 70), (48, 84), (30, 70)])
    c.add(pill(mask, depth=9, d=0, edge=.5, dome=.4), "mask", R("#3A1C24", .42, .35))
    for pts in ([(28, 36), (12, 26), (10, 8)], [(68, 36), (84, 26), (86, 8)]):
        c.add(pill(S.ribbon2d(S.bez(pts, 12), lambda t: 5 * (1 - t) + .4), depth=4, d=1, edge=.7, dome=.4), "horn", R(BONE, .42, .4))
    for eye in ([(30, 44), (44, 50), (42, 55), (32, 51)], [(66, 44), (52, 50), (54, 55), (64, 51)]):
        c.add(pill(eye, depth=1, d=-8.6, edge=.8), "eye", R("#FFE040", .2, .5, e=.9))
    teeth = [(38, 66), (58, 66), (55, 72), (51, 68), (48, 73), (45, 68), (41, 72)]
    c.add(pill(teeth, depth=1, d=-8.2, edge=.8), "teeth", R("#FFF4E0", .2, .5))
    c.acc += [glow(37, 50, 10, "#FFC040", .7), glow(59, 50, 10, "#FFC040", .7)]


def m_bleed_strike(c):
    F.weapon(c, "dagger", "T4", 26, 74, math.pi / 4, 1.05, d=2, widen=1.4)
    red = R("#D01828", .4, .45, e=.15)
    for i in range(3):
        o = i * 11
        path = [(46 + o + t * 22 - 10, 18 + o * .3 + t * 40) for t in [k / 12 for k in range(13)]]
        c.add(pill(S.ribbon2d(path, lambda t: 3.6 * math.sin(math.pi * t) + .15), depth=2.5, d=-6, edge=.7), "slash", red)
    for (x, y, s) in ((62, 76, 1), (76, 70, .8), (52, 86, .7)):
        F.drop(c, x, y, s * 1.2, "#D01828", d=-3, tag="slash")


def m_rampage(c):
    F.star(c, 44, 44, 12, 46, 22, "#E8462E", d=10, tag="burst", e=.25, depth=2)
    F.star(c, 44, 44, 12, 30, 14, "#FFB878", d=8, tag="burst2", e=.45, depth=2)
    o = F.weapon(c, "axe", "T2", 30, 70, math.pi / 4.6, .92, d=-4, spin=-20)


def m_fireball(c):
    bx, by = 58 + 14 * math.sin(math.radians(45)), 38 - 14 * math.cos(math.radians(45))
    F.flame(c, 58 - 14 * math.sin(math.pi * 1.25), 38 + 14 * math.cos(math.pi * 1.25), 66, 17, 0, d=4,
            rot=math.pi * 1.25)
    ball = M.ellipsoid((16 * U, 12 * U, 16 * U), (0, 0, 0), sides=24, rings=12)
    ball.translate(at(58, 38, -6))
    c.add(ball, "ball", R("#FF7A1A", .3, .5, e=.4))
    core = M.ellipsoid((10 * U, 6 * U, 10 * U), (0, 0, 0), sides=20, rings=10)
    core.translate(at(56, 36, -13))
    c.add(core, "core", R("#FFE070", .2, .6, e=.75))
    c.acc += [glow(58, 38, 30, "#FFA040", .5), sparkle(52, 31, 5)]


def m_meteor(c):
    F.flame(c, 40 - 12 * math.sin(math.pi * .25), 60 + 12 * math.cos(math.pi * .25), 74, 22, 0, d=6, rot=math.pi * .25)
    rock = S.lump((19 * U, 14 * U, 20 * U), seed=3, noise=.22, center=at(38, 60, -4), sides=12, rings=8)
    c.add(rock, "rock", R("#5A4040", .42, .35))
    rs = S.Surface(rock)
    for pts in ([(26, 52), (36, 62), (32, 76)], [(36, 62), (52, 58)], [(30, 48), (38, 46)]):
        c.add(IM.drape_strip(rs, Wp(pts), .012, .006, step=.006), "lava", R("#FF8A2A", .2, .5, e=.9))
    rs.free()
    c.acc.append(glow(38, 62, 30, "#FF6020", .45))


def m_blizzard(c):
    F.cloud(c, 48, 32, 1.2, "#DCEAF8", d=0)
    for (x, y) in ((26, 60), (44, 70), (62, 58), (76, 74), (34, 84)):
        F.crystal(c, x + 1.5, y - 10, 18, 3.2, rot=math.pi + .18, d=-2, col="#AEE8FF", tag="shard", e=.2)
    for (x, y, r) in ((18, 74, 6), (56, 86, 5), (82, 56, 5.5)):
        F.snowflake(c, x, y, r, d=-8)


def m_ice_armor(c):
    ice = R("#8AD8FF", .35, .55, e=.12)
    tor = S.pillow([(x * .9, z * .9 - .02) for x, z in IM.torso2d()], depth=.11, back=.03, wall=.03, edge=.4, dome=.4)
    c.add(tor, "ice", ice)
    surf = S.Surface(tor)
    for pts in ([(48, 20), (38, 44), (48, 62), (60, 42), (48, 20)], [(38, 44), (24, 60), (30, 86)],
                [(60, 42), (72, 62), (66, 86)], [(48, 62), (48, 90)]):
        pts9 = [(48 + (x - 48) * .9, 50 + (y - 50) * .9) for x, y in pts]
        c.add(IM.drape_strip(surf, Wp(pts9), .006, .006, step=.008), "facet", R("#F4FCFF", .2, .6, e=.5))
    surf.free()
    for (x, s) in ((20, -1), (76, 1)):
        sp = [(x - 8, 26), (x + s * 6, 2), (x + 8, 26)]
        sp9 = [(48 + (px - 48) * .9, 50 + (py - 50) * .9) for px, py in sp]
        c.add(pill(sp9, depth=5, d=-4, edge=.5, dome=.6), "ice")
    c.acc += [sparkle(36, 30, 6), sparkle(64, 70, 4.5)]


def m_chain_lightning(c):
    nodes = [(20, 20), (48, 54), (78, 30), (70, 80)]
    F.bolt(c, [nodes[0], (34, 30), (30, 40), (48, 54)], 5, d=0, tag="b1")
    F.bolt(c, [nodes[1], (58, 44), (62, 40), nodes[2]], 4, d=0, tag="b1")
    F.bolt(c, [nodes[1], (56, 64), (60, 68), nodes[3]], 3.6, d=0, tag="b1")
    for (x, y) in nodes:
        n = M.ellipsoid((5.5 * U, 4 * U, 5.5 * U), (0, 0, 0), sides=16, rings=8)
        n.translate(at(x, y, -3))
        c.add(n, "node", R("#E8E0FF", .2, .6, e=.6))
        c.acc.append(glow(x, y, 14, "#C0B0FF", .6))


def m_mana_shield(c):
    sph = M.ellipsoid((34 * U, 26 * U, 34 * U), (0, 0, 0), sides=40, rings=20)
    sph.translate(at(48, 48, 6))
    c.add(sph, "orb", R("#9A64F0", .42, .5, e=.15))
    surf = S.Surface(sph)
    for row in range(-4, 5):
        for col in range(-4, 5):
            x = 48 + col * 12 + (6 if row % 2 else 0)
            y = 48 + row * 10.4
            hexp = [(x + 6.9 * math.cos(math.pi / 6 + i * math.pi / 3), y + 6.9 * math.sin(math.pi / 6 + i * math.pi / 3)) for i in range(7)]
            if all((px - 48) ** 2 + (py - 48) ** 2 < 31 ** 2 for px, py in hexp):
                c.add(IM.surf_tube(surf, Wp(hexp), .006), "hex", R("#E6CCFF", .2, .6, e=.45))
    surf.free()
    rim = F.ring3(34, 34, 2.4, 2.4)
    rim.translate(at(48, 48, 6))
    c.add(rim, "rim", R("#E8D0FF", .25, .6, e=.4))
    F.star(c, 48, 48, 4, 13, 4.4, "#9AD0FF", d=-26, tag="star", e=.55, depth=3)
    c.add(pill(S.ribbon2d(S.bez([(22, 44), (26, 28), (40, 18)], 10), lambda t: 1.6 * math.sin(math.pi * t) + .3), depth=1,
               d=-22, edge=.9), "glint", R("#FFFFFF", .1, .6, e=.8))
    c.acc.append(glow(48, 48, 20, "#9AD0FF", .5))


def m_fire_wall(c):
    c.add(pill(S.circle2d(42, 40, 48, 78, rz=9), depth=4, d=4, edge=.5), "ground", R("#5A2A1A", .42, .3))
    F.flame(c, 18, 76, 40, 11, -.1, d=2, tag="fa")
    F.flame(c, 78, 76, 42, 11, .1, d=2, tag="fa")
    F.flame(c, 36, 80, 56, 13, -.05, d=-4, tag="fb")
    F.flame(c, 60, 80, 58, 13, .05, d=-4, tag="fb")
    c.acc.append(glow(48, 60, 40, "#FF8030", .3))


def m_combustion(c):
    F.star(c, 48, 50, 10, 40, 18, "#E8401C", d=4, tag="s1", e=.3, depth=4, rot=-90 + 5.7)
    F.star(c, 48, 50, 10, 28, 13, "#FF9A22", d=-1, tag="s2", e=.45, depth=4, rot=-90 + 23)
    core = M.ellipsoid((12 * U, 7 * U, 12 * U), (0, 0, 0), sides=20, rings=10)
    core.translate(at(48, 50, -7))
    c.add(core, "core", R("#FFF0A0", .1, .6, e=.85))
    for (x, y) in ((14, 20), (82, 18), (86, 80), (12, 78)):
        c.add(S.lump((3.5 * U, 2.5 * U, 3.5 * U), seed=x + y, noise=.25, center=at(x, y, 0)), "debris", R("#5A3028", .42, .3))
    c.acc.append(glow(48, 50, 30, "#FFD060", .6))


def m_ice_arrow(c):
    ice = R("#9AE4FF", .3, .55, e=.2)
    for (x, y, s) in ((20, 76, 1), (30, 84, .7), (12, 64, .7)):
        c.add(pill([(x, y - 6 * s), (x + 4 * s, y), (x, y + 6 * s), (x - 4 * s, y)], depth=3 * s, d=-2, edge=.6, dome=.6), "chip", ice)
    F.arrow(c, (18, 78), (84, 14), w=1.4, head="#9AE4FF", fletch="#DAF4FF", shaft="#7AB8E0", d=0, glow_head=.25)
    c.add(pill([(84, 14), (62, 22), (70, 30), (76, 36)], depth=3.5, d=-3, edge=.6, dome=.5), "crust", ice)
    c.acc += [sparkle(74, 20, 6)]
    F.snowflake(c, 30, 60, 5, d=-6)


def m_freeze(c):
    F.crystal(c, 30, 84, 44, 9, -.4, d=4, tag="cr")
    F.crystal(c, 66, 84, 48, 9, .35, d=4, tag="cr")
    F.crystal(c, 48, 88, 66, 12, 0, d=-4, tag="cr")
    F.snowflake(c, 20, 26, 8, d=-6)
    c.acc += [glow(48, 56, 40, "#AEE8FF", .4, under=True), sparkle(46, 34, 6)]


def m_teleport(c):
    pts = [(48 + math.cos(t * math.pi * 4.2) * (6 + t * 32), 48 + math.sin(t * math.pi * 4.2) * (6 + t * 32) * .9)
           for t in [i / 60 for i in range(61)]]
    F.ribbon3(c, pts, lambda t: 1 + t * 5, depth=3.0, d=0, tag="spiral", reg=R("#D8A0FF", .25, .6, e=.5))
    F.ribbon3(c, pts[10:56], lambda t: .55, depth=1.0, d=-1.8, tag="glint", reg=R("#FFFFFF", .1, .6, e=.8))
    c.acc += [glow(48, 48, 40, "#C070FF", .6, under=True), glow(48, 48, 12, "#FFFFFF", .8), sparkle(18, 22, 5),
              sparkle(80, 76, 4.5), sparkle(82, 20, 3.5)]


def m_arcane_torrent(c):
    for i, (x, y, s) in enumerate(((70, 26, 1), (52, 52, 1.15), (78, 60, .85), (34, 78, .9))):
        tail = [(x - 30 * s, y - 20 * s), (x - 15 * s, y - 10 * s), (x, y)]
        c.add(pill(S.ribbon2d(S.bez(tail, 10), lambda t: .5 + t * 5 * s), depth=2, d=2 - i, edge=.7), "tail",
              R("#D070FF", .25, .5, e=.45))
        orb = M.ellipsoid((7 * s * U, 5 * s * U, 7 * s * U), (0, 0, 0), sides=18, rings=9)
        orb.translate(at(x, y, -2 - i))
        c.add(orb, "orb", R("#F0C0FF", .2, .6, e=.6))
        c.acc.append(glow(x, y, 14, "#E080FF", .6))


def m_backstab(c):
    st = R("#3A2458", .42, .35)
    body = S.chain2d(S.bez([(8, 92), (8, 66), (18, 56), (40, 54)], 10), S.bez([(40, 54), (62, 56), (72, 66), (72, 92)], 10))
    c.add(pill(body, depth=8, d=4, edge=.5), "body", st)
    c.add(pill(S.circle2d(15, 24, 40, 38), depth=8, d=2, edge=.5), "body")
    hood = S.chain2d(S.bez([(24, 42), (22, 18), (58, 16), (56, 42)], 12), S.bez([(56, 42), (48, 58), (40, 60)], 6),
                     S.bez([(40, 60), (30, 58), (24, 42)], 6))
    c.add(pill(hood, depth=9, d=-1, edge=.5, dome=.4), "hood", R("#5A3C86", .42, .38))
    F.weapon(c, "dagger", "T2", 76, 22, math.pi * 1.22, 1.1, d=-10, widen=1.4)
    F.star(c, 52, 62, 8, 12, 4, "#FF4040", d=-6, tag="hit", e=.6, depth=2)
    c.acc += [glow(52, 62, 16, "#FF4040", .5), sparkle(76, 26, 5)]


def m_poison_blade(c):
    F.weapon(c, "dagger", "T4", 30, 72, math.pi / 4, 1.2, d=2, widen=1.4)
    for (x, y, s) in ((60, 50, 1), (70, 66, .8), (52, 70, .7)):
        F.drop(c, x, y, s * 1.3, "#7BE03A", d=-6, tag="venom", e=.25)
    for (x, y, r) in ((80, 30, 3), (74, 20, 2), (84, 44, 2.2)):
        F.ring2d(c, x, y, r, r, 1.0, "#C8FF96", d=-4, tag="bub", e=.5)
    c.acc.append(glow(64, 34, 26, "#7BE03A", .35))


def m_vanish(c):
    c.add(pill(F.hood_pts(), depth=12, d=0, edge=.5, dome=.4), "hood", R("#4A3470", .42, .35))
    c.add(pill(S.circle2d(17, 28, 48, 42, rz=19), depth=1.5, d=-11, edge=.8), "face", R("#0C0814", .2, .03))
    for eye in ([(36, 40), (45, 43), (43, 46), (37, 44)], [(60, 40), (51, 43), (53, 46), (59, 44)]):
        c.add(pill(eye, depth=.8, d=-12.8, edge=.8), "eye", R("#D8B0FF", .2, .6, e=.9))
    for (x, y, s) in ((26, 78, 1), (48, 84, -1), (70, 78, 1)):
        path = S.bez([(x, y - 8), (x + 8 * s, y - 2), (x - 8 * s, y + 4), (x, y + 10)], 12)
        c.add(pill(S.ribbon2d(path, lambda t: 1.6 * math.sin(math.pi * (.15 + .85 * t)) + .3), depth=1.5, d=-6, edge=.8),
              "smoke", R("#C8AAFF", .25, .55, e=.35))
    c.acc += [glow(40, 43, 8, "#D8B0FF", .7), glow(56, 43, 8, "#D8B0FF", .7)]


def m_multishot(c):
    for i, a in enumerate((-.55, -.28, 0, .28, .55)):
        ang = -math.pi / 4 + a
        F.arrow(c, (20, 78), (20 + math.cos(ang) * 70, 78 + math.sin(ang) * 70), w=1.05, d=-i * 1.5, tag="ar")
    c.acc.append(sparkle(22, 76, 6))


def m_arrow_rain(c):
    dx, dy = math.cos(math.pi * .62), math.sin(math.pi * .62)
    for i, (x, y) in enumerate(((30, 44), (56, 36), (80, 48), (42, 70), (68, 78), (20, 84))):
        F.arrow(c, (x - dx * 30, y - dy * 30), (x, y), w=.95, d=-i * 1.2, tag="ar")


def m_shadow_step(c):
    for i, (x, y, rot, a) in enumerate(((22, 80, -.5, .45), (46, 60, -.3, .7), (66, 34, -.5, 1.0))):
        col = color.rgb_to_hex(color.mix(color.hex_to_rgb("#5E3C90"), color.hex_to_rgb("#B890FF"), a))
        sole = F.xform2d(S.circle2d(7, 20, 0, -4, rz=11), x, y, rot)
        heel = F.xform2d(S.circle2d(5, 16, 0, 12, rz=6), x, y, rot)
        reg = R(col, .35, .5, e=.2 * a)
        c.add(pill(sole, depth=3, d=-i, edge=.6), f"fp{i}", reg)
        c.add(pill(heel, depth=3, d=-i, edge=.6), f"fp{i}")
    for o in (-6, 0, 6):
        path = S.bez([(14 + o, 60 + o), (30 + o, 30 + o), (60 + o, 16 + o)], 12)
        c.add(pill(S.ribbon2d(path, lambda t: 1.2 * math.sin(math.pi * t) + .2), depth=1, d=4, edge=.9), "streak",
              R("#D2B4FF", .25, .55, e=.3))
    c.acc.append(glow(66, 34, 16, "#B890FF", .5))


def m_death_mark(c):
    red = R("#FF3040", .3, .5, e=.55)
    for r, w in ((36, 3), (42, 2)):
        ring = F.ring3(r, r, w, 2)
        ring.translate(at(48, 50, 6))
        c.add(ring, "mark", red)
    for k in range(4):
        an = k * math.pi / 2
        c.add(pill(S.capsule2d((48 + math.cos(an) * 30, 50 + math.sin(an) * 30), (48 + math.cos(an) * 46, 50 + math.sin(an) * 46), 1.5),
                   depth=2, d=6, edge=.8), "mark")
    F.skull(c, 48, 50, 1.45, eye="#FF2030", d=0, eye_e=.9)
    c.acc += [glow(38, 49, 9, "#FF2030", .7), glow(58, 49, 9, "#FF2030", .7)]


def m_piercing_arrow(c):
    for (cx, cy, rx, ry, w) in ((52, 44, 9, 20, 3), (64, 32, 6, 13, 2)):
        ring = F.ring3(rx, ry, w, 2)
        ring.rotate("y", 45)
        ring.translate(at(cx, cy, 2))
        c.add(ring, "ring", R("#FFFFFF", .2, .6, e=.6))
    F.arrow(c, (12, 84), (86, 10), w=1.35, head="#E8F0FF", d=-2)
    F.speed_lines(c, [((10, 66), (26, 50)), ((28, 90), (44, 74))], 1.2)


def m_poison_arrow(c):
    trail = S.bez([(14, 82), (40, 64), (70, 28)], 16)
    c.add(pill(S.ribbon2d(trail, lambda t: 5 * math.sin(math.pi * (.1 + .9 * t)) ** .5), depth=2, d=6, edge=.8), "trail",
          R("#8CF050", .3, .5, e=.35))
    F.arrow(c, (14, 82), (80, 16), w=1.35, head="#7BE03A", d=0, glow_head=.3)
    for (x, y, s) in ((70, 36, 1), (62, 48, .75)):
        F.drop(c, x, y, s * 1.2, "#7BE03A", d=-4, tag="venom", e=.25)
    c.acc.append(glow(78, 18, 16, "#9AFF60", .55))


def m_explosive_trap(c):
    c.add(pill(S.circle2d(36, 36, 48, 78, rz=10), depth=4, d=6, edge=.5), "ground", R("#6A5238", .42, .32))
    bomb = M.ellipsoid((24 * U, 20 * U, 24 * U), (0, 0, 0), sides=32, rings=16)
    bomb.translate(at(46, 58, 0))
    c.add(bomb, "bomb", R("#4A4A5A", .42, .4))
    cap = M.cylinder(5.5 * U, 5 * U, 0, 9 * U, sides=16)
    cap.rotate("z", 0)
    cap.translate((0, 0, 0))
    S.align(cap, (0, 0, 1), (0, -1, 0), at=at(58, 39, -2))
    capobj = M.loft_z([(0, 6 * U, 6 * U), (8 * U, 5.5 * U, 5.5 * U), (9 * U, 4 * U, 4 * U)], sides=16)
    capobj.rotate("y", 28)
    capobj.translate(at(56, 38, -1))
    c.add(capobj, "cap", R("#8A8A9A", .42, .5))
    fuse = [at(58, 31, -1), at(62, 25, -1), at(68, 21, -1), at(74, 20, -1)]
    c.add(M.tube(fuse, [1.4 * U] * 4, sides=6), "fuse", R("#C8A870", .42, .4))
    bs = S.Surface(bomb)
    p, _ = bs.at(*W(47, 60))
    bs.free()
    c.add(pill([(46, 46), (38, 62), (46, 62), (42, 74), (56, 56), (48, 56), (52, 46)], depth=2, d=p.y / U - 1.2, edge=.7),
          "zap", R("#FFC030", .3, .55, e=.35))
    F.star(c, 75, 19, 6, 8, 3, "#FFE070", d=-4, tag="spark", e=.9, depth=1.5)
    c.acc.append(glow(75, 19, 14, "#FFA030", .8))


def m_poison_cloud(c):
    F.cloud(c, 48, 60, 1.3, "#7AD040", d=4, tag="c1")
    F.cloud(c, 44, 40, .8, "#9AE060", d=8, tag="c2")
    F.skull(c, 48, 60, .75, eye="#1A3008", col="#E8F8C8", d=-12)
    for (x, y, r) in ((18, 24, 3), (80, 28, 3.6), (74, 84, 2.6), (22, 86, 2.2)):
        F.ring2d(c, x, y, r, r, 1.2, "#D2FFA0", d=-4, tag="bub", e=.5)


def m_slow_trap(c):
    for (a0, a1) in ((36, 162), (216, 342)):
        F.ring2d(c, 48, 48, 40, 40, 2.4, "#96DCFF", d=10, tag="arc", a0=a0, a1=a1, e=.5)
    glass = S.chain2d([(30, 20), (66, 20)], S.bez([(66, 20), (66, 40), (50, 48)], 8), S.bez([(50, 48), (66, 56), (66, 76)], 8),
                      [(30, 76)], S.bez([(30, 76), (30, 56), (46, 48)], 8), S.bez([(46, 48), (30, 40), (30, 20)], 8))
    c.add(pill(glass, depth=8, d=0, edge=.55, dome=.4), "glass", R("#D2ECFA", .3, .6))
    sand = R("#6AC8FF", .35, .5, e=.2)
    c.add(pill(S.chain2d(S.bez([(34, 75), (48, 58), (62, 75)], 10)), depth=2, d=-7, edge=.6), "sand", sand)
    c.add(pill(S.chain2d([(33.5, 26), (62.5, 26)], S.bez([(62.5, 26), (62, 34), (50, 40)], 6), S.bez([(46, 40), (34, 34), (33.5, 26)], 6)),
               depth=2, d=-7, edge=.6), "sand")
    c.add(pill(S.capsule2d((48, 40), (48, 72), 1.2), depth=1.5, d=-6, edge=.8), "sand")
    c.add(pill(S.ribbon2d([(35, 26), (37, 38)], .9), depth=.8, d=-8.5, edge=.9), "glint", R("#FFFFFF", .1, .6, e=.8))
    for y in (17.5, 79.5):
        c.add(pill(S.rrect2d(52, 7, 3, 3, 48, y), depth=8, d=-1, edge=.5), "gold", R(GOLD, .42, .55))
    for x in (25.8, 70.8):
        c.add(pill(S.rrect2d(3.6, 56, 1.5, 2, x, 48), depth=3.6, d=-3, edge=.6), "post", R(WOOD, .42, .4))


def m_chain_trap(c):
    st = R("#9AA4B4", .42, .5)
    chain_pts = [at(11 + i * 3.4, 88 - i * 3.0, -2) for i in range(8)]
    c.add(S.links(chain_pts, 9.5 * U, 1.9 * U, face=(0, -1, 0)), "chain", st)
    c.add(pill(S.circle2d(30, 40, 54, 66, rz=9), depth=4, d=8, edge=.5), "base", R("#4A4A58", .42, .3))
    tooth = R("#E0E8F0", .3, .55)
    for s_ in (-1, 1):
        arc = [(54 + 25 * math.cos(a), 64 + s_ * 25 * math.sin(a)) for a in [math.pi * (1.0 + i / 20) for i in range(21)]]
        F.ribbon3(c, arc, 3.2, depth=5.0, d=-1 if s_ > 0 else 2, tag="jaw", reg=st)
        for i in range(7):
            a = math.pi * (1.12 + i * .127)
            px, py = math.cos(a) * 22, math.sin(a) * 22
            tri = [(54 + px * 1.04 - 2.4, 64 + py * s_ * 1.02), (54 + px * .78, 64 + py * .72 * s_), (54 + px * 1.04 + 2.4, 64 + py * s_ * 1.02)]
            c.add(pill(tri, depth=2.5, d=-2.5 if s_ > 0 else .5, edge=.6, dome=.6), "tooth", tooth)
    for sx in (-1, 1):
        hinge = M.ellipsoid((4 * U, 3 * U, 4 * U), (0, 0, 0), sides=12, rings=6)
        hinge.translate(at(54 + sx * 25, 64, -4))
        c.add(hinge, "hinge", R("#6E7686", .42, .45))


MOTIFS = {k[2:]: v for k, v in globals().items() if k.startswith("m_") and callable(v)}


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Plate (web drawSkillIcon backdrop, frame left to Slate)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def rounded_square_mask(size: int, inset_u: float = 2.0, r_u: float = 15.0, ss: int = 4) -> np.ndarray:
    k = size / K.UNIT
    x0, x1 = inset_u * k, size - inset_u * k
    r = r_u * k
    ys = (np.arange(size * ss) + .5) / ss
    X, Y = np.meshgrid(ys, ys)
    cx = np.clip(X, x0 + r, x1 - r)
    cy = np.clip(Y, x0 + r, x1 - r)
    inside = (np.hypot(X - cx, Y - cy) <= r) & (X >= x0) & (X <= x1) & (Y >= x0) & (Y <= x1)
    return K.down(inside.astype(np.float32), ss)


def plate(bg: tuple[str, str, str], size: int = K.ICON_PX) -> np.ndarray:
    mid, deep, rim = (np.array(color.hex_to_rgb(h), np.float32) / 255.0 for h in bg)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32) + .5
    # radial gradient between circle (0.4, 0.36, r .05) and (0.5, 0.5, r .72) — sampled along the centre line
    t = np.hypot(xx - size * .45, yy - size * .43) / (size * .72)
    t = np.clip((t - .03) / .97, 0, 1)
    c0 = mid + (1 - mid) * .22

    def lerp(a, b, u):
        return a + (b - a) * u[..., None]
    col = np.where((t < .45)[..., None], lerp(c0, mid, t / .45), lerp(mid, deep, (t - .45) / .55))
    # 12 faint additive rays in the rim colour (alpha .045)
    ang = (np.degrees(np.arctan2(yy - size / 2, xx - size / 2)) + 360) % 30
    ray = (np.abs(ang - 15) < 5.1).astype(np.float32)
    ray = K.blur(ray, .6)
    col = np.clip(col + rim * .045 * ray[..., None], 0, 1)
    # vignette to alpha .5 black (r .36 → .74 of the size)
    v = np.clip((np.hypot(xx - size / 2, yy - size / 2) - size * .36) / (size * .38), 0, 1) * .5
    col = col * (1 - v[..., None])
    a = rounded_square_mask(size)
    return np.dstack([col, a]).astype(np.float32)


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def skills() -> list[dict]:
    data = json.loads((DATA / "classes.json").read_text())
    cls = data["classes"]
    by_id = cls if isinstance(cls, dict) else {c["id"]: c for c in cls}
    out = []
    for cid in data["classOrder"]:
        for s in by_id[cid]["skills"]:
            out.append({"id": s["id"], "cls": cid, "damageType": s["damageType"], "tree": s["tree"]})
    return out


def backdrop(sk: dict) -> tuple[str, str, str]:
    if sk["damageType"] != "physical":
        return ELEMENT_BG[sk["damageType"]]
    return TREE_BG.get(sk["tree"], TREE_BG["combat_master"])


def render_motif(cam, sk: dict):
    K.clear_objects()
    pal = IM.pal("Skills")
    c = F.Comp(f"skill_{sk['id']}", pal)
    MOTIFS[sk["id"]](c)
    objs = c.build()
    # web space framing: the 96-unit square scaled 0.9 about its centre
    K.apply_frame(cam, K.Frame(world_per_unit=U / MOTIF_SCALE, cx=0.0, cz=0.0))
    style = K.IconStyle(ink_u=INK_U, line_u=K.LINE_U * MOTIF_SCALE, seam_u=K.SEAM_U * MOTIF_SCALE, **c.style)
    return K.render_icon(style=style, accents=c.acc)


def emblem(motif_rgba: np.ndarray, sk: dict) -> np.ndarray:
    pl = plate(backdrop(sk), motif_rgba.shape[0])
    mo = motif_rgba.astype(np.float32) / 255.0
    # the motif is clipped to the plate (web: drawn inside ctx.clip of the frame)
    mo[..., 3] *= pl[..., 3]
    out = K.over(pl, mo)
    out[..., 3] = pl[..., 3]
    return (np.clip(out, 0, 1) * 255 + .5).astype(np.uint8)


def main(argv: list[str]) -> int:
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    sks = skills()
    missing = [s["id"] for s in sks if s["id"] not in MOTIFS]
    if missing:
        raise RuntimeError(f"skills without a motif: {missing}")
    if only:
        sks = [s for s in sks if s["id"] in only]
    cam = K.studio()
    IM.reset_palettes()
    res = {}
    t0 = time.time()
    for i, sk in enumerate(sks):
        r = render_motif(cam, sk)
        res[sk["id"]] = (sk, r, emblem(r.rgba, sk))
        print(f"[skills] {i + 1:2d}/{len(sks)} {sk['id']:20s} gate {'ok ' if r.gate['pass'] else 'FAIL'} "
              f"span {r.gate['span']:.2f}  {time.time() - t0:5.0f}s" + ("" if r.gate["pass"] else f"  {r.gate}"))
    fails = [k for k, (_, r, _) in res.items() if not r.gate["pass"]]
    if only:
        cells = [(k, e) for k, (_, r, e) in res.items()] + [(k + "_m", r.rgba) for k, (_, r, e) in res.items()]
        pngio.write_png(K.SCRATCH / "skills_only.png", K.atlas(cells, cols=min(8, len(res)) if len(res) > 1 else 2, scale=.5))
        pngio.write_png(K.SCRATCH / "skills_only_ui.png", K.ui_sheet([(k, e) for k, (_, r, e) in res.items()], cols=8))
        print(f"[skills] scratch sheet; gate fails: {fails}")
        return 0
    for sid, (sk, r, e) in res.items():
        K.write_icon(f"{GROUP_DIR}/T_UI_SkillIcon_{sid}.png", e)
        K.write_icon(f"{GROUP_DIR}/T_UI_SkillMotif_{sid}.png", r.rgba)
    previews = []
    for cls in ("warrior", "mage", "rogue"):
        cells = [(sid, e) for sid, (sk, r, e) in res.items() if sk["cls"] == cls]
        mcells = [(sid, r.rgba) for sid, (sk, r, e) in res.items() if sk["cls"] == cls]
        for pi in range(0, len(cells), 8):
            sfx = f"_p{pi // 8 + 1}"
            previews += K.save_preview("icons_skills", f"{cls}{sfx}_1x.png", K.atlas(cells[pi:pi + 8], cols=4))
        previews += K.save_preview("icons_skills", f"{cls}_half.png", K.atlas(cells, cols=8, scale=.5))
        previews += K.save_preview("icons_skills", f"{cls}_motifs_half.png", K.atlas(mcells, cols=8, scale=.5))
        previews += K.save_preview("icons_skills", f"{cls}_ui.png", K.ui_sheet(cells, cols=8, qualities=("normal",)))
    man = K.IconManifest()
    for sid, (sk, r, e) in res.items():
        bg = backdrop(sk)
        common = dict(skillId=sid, cls=sk["cls"], damageType=sk["damageType"], tree=sk["tree"],
                      backdrop={"mid": bg[0], "deep": bg[1], "rim": bg[2]}, gate=r.gate, previews=previews)
        man.data["ui"][f"T_UI_SkillIcon_{sid}"] = K.ui_entry(
            "skillIcon", f"{GROUP_DIR}/T_UI_SkillIcon_{sid}.png", K.ICON_PX, plate=True,
            frame="Slate (dark iron #17121C 3.2u, rim hairline, top-left bevel): not baked", **common)
        man.data["ui"][f"T_UI_SkillMotif_{sid}"] = K.ui_entry(
            "skillMotif", f"{GROUP_DIR}/T_UI_SkillMotif_{sid}.png", K.ICON_PX, plate=False, **common)
    man.save()
    print(f"[skills] shipped {len(res)} skills × 2 textures; gate fails: {fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]))
