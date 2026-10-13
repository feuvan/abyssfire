"""VFX previews: every recipe of ``Export/VFX/vfx_recipes.json`` played by a Python port of the UE runtime and
drawn at the W1 game camera over the plains, cast by the hero class that owns it (warrior / mage / rogue, each in its
own clip at the beat), with goblin targets and the props / NPCs the world effects sit on.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_preview.py [ids…|groups…]
    #   groups: combat utility status world warrior mage rogue ambient fallback      (default: all)
    #   --scratch       write to the scratchpad instead of Art/Previews/vfx_effects
    #   --cells         also dump every frame to the scratchpad
    #   --soft-ground   draw ground quads with a 0.8 cm depth fade (the proposed runtime fix) instead of the
    #                   shipped M_AF_FX_Sprite DepthFade(8 cm) on every quad (the default: what the game draws)

Port of ``UAbyssVfxSystem`` (Source/Abyssfire/Private/Vfx/AbyssVfxSystem.cpp + AbyssVfxRecipe.cpp ApplyPreset):
the same layer defaults, emission (bursts, rates, delays, point stagger), spawn (anchors, spawn disc, cone
directions incl. the blow direction's random climb), motion (drag, gravity, orbit), size / alpha curves, colours
(palettes, event colours, alternates, intensity) and the per-orientation quad / mesh transforms. Actor anchors are
the shipped sockets of the posed rig (``fx_chest`` / ``fx_head`` / ``fx_overhead`` / ``fx_hand_r`` — the mage's
staff ``crystal`` for ``hand``, spec 8.1 — as ``AAbyssCharacterActor::GetAnchorLocation``), re-evaluated every 60 Hz tick while the cast clip plays. Rendering:

* Blender (Cycles, emission-only toon look) renders the static scene — the composited plains terrain, the hero in
  the clip time of the event (beat + t), goblins, props (the chest in its Open clip), the blacksmith in Work_Hammer
  — and the **FX meshes** (``fxkit.fx_material`` = M_AF_FX_Mesh, incl. the Bayer-dithered fade), colour (linear
  HDR) + a view-depth pass, at native 1080p for the crop;
* numpy composites every sprite quad exactly like the sprite materials: ray / quad intersection per pixel (true
  perspective), bilinear texture cell, additive (``tex.rgb·tint·tex.a·α``) or translucent, depth-tested with the
  shipped 8 cm DepthFade on every quad (``--soft-ground``: 0.8 cm on ground quads); translucent pools before
  additive ones (sort priorities 1 / 2); the runtime's fake light pools at +2 cm with its ×0.5 alpha;
* the character-side composites the runtime adds: the StatusTint multiply on a status target, the dodge
  afterimages (Ghost material: class spirit colour, α .28 / .21 at ⅓ / ⅔ of the path, 260 ms);
* bloom like the zone post (threshold 1, the kit's two lobes, strength 0.8), then sRGB.

Each effect → ``Previews/vfx_effects/<recipe-id>/<ms>[-<ms>]ms.png``: the key frames (ms from the event) as native
1080p crops at scale 1, truecolour, one or two frames per file — a frame that cannot be stored under 200 KB is cut
into stacked native strips (``pngio.write_png_native``), and a frame that still does not fit fails the run (no
resampling, ever).
"""
from __future__ import annotations

import json
import math
import random
import sys
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "environment"))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import outline, paths, pngio, review, scene, shading  # noqa: E402
import fxkit as K  # noqa: E402

RECIPES = paths.export_root() / "VFX" / "vfx_recipes.json"
TEX_DIR = paths.export_root() / "VFX"
OUT_DIR = paths.preview_root() / "vfx_effects"
RES = (1920, 1080)

# ── recipe model (AbyssVfxRecipe.h defaults + ApplyPreset) ───────────────────────────────────────────
BASE = dict(sprite="Glow", mesh=None, blend="additive", orient="billboard", variant=0, randomVariants=0,
            at="origin", height="ground", heightCm=0.0, offset=(0.0, 0.0, 0.0), beamTo="target",
            beamToHeight="chest", spawnRadius=0.0, onRing=False, jitter=0.0, unit="cm", attach=False, delay=0.0,
            count=(1.0, 1.0), countFromContext=False, countByRadius=False, rate=0.0, duration=-1.0, stagger=-1.0,
            life=(300.0, 300.0), speed=(0.0, 0.0), dir="random", cone=360.0, up=(0.0, 0.0), gravity=0.0, drag=0.0,
            orbit=False, orbitR0=(0.0, 0.0), orbitR1=(0.0, 0.0), orbitDeg=(0.0, 0.0), size=(20.0, 20.0), grow=1.0,
            sizePow=1.0, aspect=1.0, stretch=0.0, alpha0=1.0, alpha1=0.0, fadeIn=0.0, alphaPow=1.0, flicker=0.0,
            colorSource="mid", color=(1.0, 1.0, 1.0), altEvery=0, altSource="white", altColor=(1.0, 1.0, 1.0),
            intensity=1.0, rotation=(0.0, 0.0), spin=(0.0, 0.0), align=False, big=0, fan=False)

PRESETS = {
    "glow": dict(sprite="Glow", orient="billboard", blend="additive", life=(300, 300), size=(53, 53), grow=1.3 / 0.6,
                 sizePow=2, alpha0=0.9, alpha1=0, fadeIn=0.08, alphaPow=1.6),
    "flash": dict(sprite="Core", orient="billboard", blend="additive", life=(160, 160), size=(25, 25),
                  grow=0.85 / 0.35, sizePow=2, alpha0=0.9, colorSource="core", intensity=1.5),
    "glint": dict(sprite="Spark", orient="billboard", life=(200, 200), size=(18, 18), grow=1 / 0.3, sizePow=3,
                  alpha0=1, fadeIn=0.15, alphaPow=1.5, colorSource="white", intensity=1.5),
    "sparks": dict(sprite="Streak", orient="velocity", count=(6, 6), speed=(200, 444), dir="random",
                   life=(180, 340), drag=3, size=(8, 12), aspect=0.25, stretch=0.06, grow=0.5, alpha0=1,
                   alphaPow=1.2, colorSource="mid", altEvery=3, altSource="white", intensity=1.4),
    "streak": dict(sprite="Streak", orient="velocity", life=(200, 200), size=(10, 10), aspect=0.2, stretch=0.08,
                   alpha0=1),
    "motes": dict(sprite="Ember", orient="billboard", count=(6, 6), spawnRadius=30, speed=(44, 155), dir="random",
                  life=(400, 700), size=(6, 12), gravity=-90, drag=1.5, alpha0=1, alphaPow=1.5, spin=(-120, 120),
                  intensity=1.3),
    "flames": dict(sprite="Flame", orient="upright", blend="additive", count=(6, 6), spawnRadius=20, up=(67, 155),
                   life=(300, 520), size=(30, 50), aspect=0.62, grow=0.7, alpha0=0.95, fadeIn=0.1, alphaPow=1.4,
                   colorSource="mid", intensity=1.3),
    "smoke": dict(sprite="Smoke", randomVariants=2, orient="billboard", blend="translucent", count=(4, 4),
                  spawnRadius=20, speed=(33, 100), dir="flat", gravity=-44, drag=2.2, life=(500, 900),
                  size=(30, 50), grow=1.25 / 0.6, sizePow=1.5, alpha0=0.8, fadeIn=0.12, alphaPow=1.3,
                  spin=(-60, 60), rotation=(0, 360), colorSource="fixed", color="#5A5060"),
    "debris": dict(mesh="SM_FX_Rock_A", sprite="Rock", randomVariants=2, orient="billboard", blend="translucent",
                   count=(4, 4), speed=(133, 289), dir="flat", up=(244, 377), gravity=1155, life=(420, 560),
                   size=(8, 14), grow=1, alpha0=1, alphaPow=5, spin=(-516, 516), rotation=(0, 360),
                   colorSource="fixed", color="#8A8E98"),
    "ring": dict(sprite="Ring", orient="ground", life=(400, 400), size=(20, 20), grow=5, sizePow=3, alpha0=0.8,
                 fadeIn=0.05, alphaPow=1.5, heightCm=3, height="custom"),
    "shock": dict(sprite="Shock", orient="ground", life=(400, 400), size=(20, 20), grow=5, sizePow=3, alpha0=0.9,
                  alphaPow=1, heightCm=2, height="custom"),
    "decal": dict(sprite="Scorch", orient="ground", blend="translucent", life=(1400, 1400), size=(80, 80),
                  grow=1 / 0.85, sizePow=2, alpha0=0.6, fadeIn=0.1, alphaPow=2.5, rotation=(0, 360), heightCm=1,
                  height="custom", colorSource="fixed", color=(0.08, 0.06, 0.06)),
    "slash": dict(sprite="Slash", orient="billboard", life=(150, 230), size=(45, 45), aspect=2, grow=1.2,
                  sizePow=2, alpha0=1, alphaPow=1.5, align=True, intensity=1.3),
    "swipe": dict(sprite="Slash", orient="ground", life=(230, 230), size=(60, 60), aspect=2, grow=1.1, sizePow=2,
                  alpha0=1, alphaPow=1.5, align=True, height="chest", intensity=1.3),
    "beam": dict(sprite="Beam", orient="upright", life=(700, 700), size=(150, 150), aspect=0.33, grow=2,
                 sizePow=3, alpha0=0.9, fadeIn=0.1, alphaPow=1, intensity=1.3),
    "gather": dict(sprite="Glow", orient="billboard", count=(10, 10), orbit=True, orbitR0=(90, 110),
                   orbitR1=(0, 0), orbitDeg=(143, 229), life=(260, 340), size=(10, 16), alpha0=0.2, alpha1=1,
                   alphaPow=1, height="chest", intensity=1.3),
    "bolt": dict(sprite="Bolt", randomVariants=4, orient="beam", life=(240, 240), size=(18, 18), grow=1, alpha0=1,
                 alphaPow=1.5, flicker=0.5, height="chest", beamToHeight="chest", intensity=1.6),
    "mesh": dict(orient="billboard", blend="translucent", life=(400, 400), size=(100, 100), grow=1, alpha0=1,
                 alpha1=1),
}

PALETTES = {}   # filled from the recipe file (linear)
CELLS = {}


def srgb_lin(c):
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def hex_lin(h: str):
    s = h.lstrip("#")
    return tuple(srgb_lin([int(s[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]))


def rng_range(v):
    if isinstance(v, (int, float)):
        return (float(v), float(v))
    a = float(v[0])
    b = float(v[1]) if len(v) > 1 else a
    return (min(a, b), max(a, b))


def parse_color(v):
    if isinstance(v, (int, float)):
        return "fixed", hex_lin("%06X" % int(v))
    if v in ("core", "mid", "rim", "dark", "event", "white"):
        return v, (1.0, 1.0, 1.0)
    return "fixed", hex_lin(v)


def parse_layer(j: dict) -> dict:
    L = dict(BASE)
    if "emitter" in j:
        pre = dict(PRESETS[j["emitter"]])
        if isinstance(pre.get("color"), str):
            pre["color"] = hex_lin(pre["color"])
        L.update(pre)
    if j.get("sprite"):
        L["sprite"] = j["sprite"]
    if "mesh" in j:
        L["mesh"] = j["mesh"] or None
    if "blend" in j:
        L["blend"] = "translucent" if j["blend"] in ("translucent", "normal") else "additive"
    for k in ("orient", "at", "beamTo", "dir", "unit"):
        if k in j:
            L[k] = j[k]
    for k, d in (("variant", "variant"), ("randomVariants", "randomVariants"), ("altEvery", "altEvery")):
        if k in j:
            L[d] = int(j[k])
    if "height" in j:
        h = j["height"]
        if isinstance(h, (int, float)):
            L["height"], L["heightCm"] = "custom", float(h)
        else:
            L["height"] = {"feet": "ground"}.get(h, h)
    if "beamToHeight" in j:
        L["beamToHeight"] = j["beamToHeight"] if isinstance(j["beamToHeight"], str) else "custom"
    if "offsetCm" in j:
        L["offset"] = tuple(float(x) for x in j["offsetCm"])
    for k, d in (("spawnRadiusCm", "spawnRadius"), ("spawnHeightJitterCm", "jitter"), ("delayMs", "delay"),
                 ("rate", "rate"), ("durationMs", "duration"), ("staggerMs", "stagger"), ("coneDeg", "cone"),
                 ("gravityCmS2", "gravity"), ("drag", "drag"), ("grow", "grow"), ("sizePow", "sizePow"),
                 ("aspect", "aspect"), ("stretch", "stretch"), ("fadeIn", "fadeIn"), ("alphaPow", "alphaPow"),
                 ("flicker", "flicker"), ("intensity", "intensity")):
        if k in j:
            L[d] = float(j[k])
    for k, d in (("onRing", "onRing"), ("attach", "attach"), ("countByRadius", "countByRadius"),
                 ("alignToBlow", "align"), ("fan", "fan")):
        if k in j:
            L[d] = bool(j[k])
    if "count" in j:
        if isinstance(j["count"], str):
            L["countFromContext"] = j["count"] == "sparks"
        else:
            L["count"] = rng_range(j["count"])
    for k, d in (("lifeMs", "life"), ("speedCmS", "speed"), ("upCmS", "up"), ("sizeCm", "size"),
                 ("rotationDeg", "rotation"), ("spinDegS", "spin")):
        if k in j:
            L[d] = rng_range(j[k])
    if "orbit" in j:
        o = j["orbit"]
        if isinstance(o, dict):
            L["orbit"] = True
            L["orbitR0"] = rng_range(o.get("r0Cm", 0))
            L["orbitR1"] = rng_range(o.get("r1Cm", 0))
            L["orbitDeg"] = rng_range(o.get("degS", 0))
        else:
            L["orbit"] = bool(o)
    if "alpha" in j:
        a = j["alpha"]
        if isinstance(a, list):
            L["alpha0"], L["alpha1"] = float(a[0]), float(a[1])
        else:
            L["alpha0"] = float(a)
    if "color" in j:
        L["colorSource"], L["color"] = parse_color(j["color"])
    if "altColor" in j:
        L["altSource"], L["altColor"] = parse_color(j["altColor"])
    if "only" in j:
        L["big"] = 1 if j["only"] == "big" else (-1 if j["only"] == "small" else 0)
    if isinstance(L["color"], str):
        L["color"] = hex_lin(L["color"])
    L["count"] = (min(max(L["count"][0], 0), 200), min(max(L["count"][1], L["count"][0]), 200))
    L["life"] = (max(1.0, L["life"][0]), max(L["life"][0], L["life"][1]))
    L["fadeIn"] = min(max(L["fadeIn"], 0), 0.95)
    return L


def load_recipes():
    doc = json.loads(RECIPES.read_text())
    for name, p in doc["palettes"].items():
        PALETTES[name] = {k: hex_lin(v) for k, v in p.items()}
    for name, s in doc.get("sprites", {}).items():
        CELLS[name] = int(s.get("cells", 1))
    recipes = {}
    for rid, r in doc["recipes"].items():
        recipes[rid] = {"palette": r.get("palette", "event"), "layers": [parse_layer(l) for l in r["layers"]],
                        "light": r.get("light"), "shake": r.get("shake"), "arc": r.get("arcCm", 0.0)}
    for alias, target in doc.get("aliases", {}).items():
        if target in recipes:
            recipes[alias] = recipes[target]
    return recipes


def palette_from_color(c):
    lerp = lambda a, b, t: tuple(x + (y - x) * t for x, y in zip(a, b))  # noqa: E731
    return {"core": lerp(c, (1, 1, 1), 0.7), "mid": lerp(c, (1, 1, 1), 0.25), "rim": tuple(c),
            "dark": tuple(x * 0.3 for x in c)}


ELEMENT_PAL = {"fire": "fire", "ice": "frost", "lightning": "lightning", "poison": "poison", "arcane": "arcane",
               "physical": "steel"}


# ── context / anchors ──────────────────────────────────────────────────────────────────────────────────
@dataclass
class Actor:
    name: str
    pos: np.ndarray                      # UE cm (feet)
    heights: dict                        # chest / head / overhead / hand → cm (actors without a posed rig)
    hand_off: np.ndarray = field(default_factory=lambda: np.zeros(3))
    sockets: dict = field(default_factory=dict)   # chest / head / overhead / hand → UE cm (posed rig sockets)


@dataclass
class Ctx:
    origin: np.ndarray
    target: np.ndarray
    point: np.ndarray
    origin_actor: Actor | None = None
    target_actor: Actor | None = None
    points: list = field(default_factory=list)
    stagger: float = 0.0
    radius: float = 100.0
    ring: float = 13 * 100 / 45
    sparks: int = 6
    big: bool = False
    color: tuple | None = None
    element: str | None = None
    blow: np.ndarray = field(default_factory=lambda: np.array([1.0, 0.0]))


def actor_point(a: Actor, height: str, cm: float) -> np.ndarray:
    if height in ("ground",):
        return a.pos.copy()
    if height == "custom":
        return a.pos + np.array([0, 0, cm])
    if height in a.sockets:
        return np.array(a.sockets[height], float)
    if height == "hand":
        return a.pos + a.hand_off + np.array([0, 0, a.heights["hand"]])
    return a.pos + np.array([0, 0, a.heights[height]])


DEFAULT_H = {"chest": 80.0, "head": 140.0, "overhead": 200.0, "hand": 95.0, "ground": 0.0}


class Anchor:
    def __init__(self, actor: Actor | None, height: str, cm: float, fixed: np.ndarray, moving=None):
        self.actor, self.height, self.cm, self.fixed, self.moving = actor, height, cm, np.asarray(fixed, float), moving

    def pos(self, t_ms: float) -> np.ndarray:
        if self.moving is not None:
            return self.moving(t_ms)
        if self.actor is not None:
            return actor_point(self.actor, self.height, self.cm)
        h = self.cm if self.height == "custom" else DEFAULT_H.get(self.height, 0.0)
        return self.fixed + np.array([0, 0, h])


# ── simulation ─────────────────────────────────────────────────────────────────────────────────────────
@dataclass(eq=False)
class Particle:
    L: dict
    handle: int
    anchor: Anchor | None
    beam_end: Anchor | None
    pos: np.ndarray
    vel: np.ndarray
    life: float
    size0: float
    size1: float
    rot: float
    spin: float
    variant: int
    color: tuple
    orbit: tuple | None
    age: float = 0.0
    mesh_d: float = 100.0


class Emitter:
    def __init__(self, handle, L, ctx, pal, start, anchor, beam_end=None, point_index=0):
        self.handle, self.L, self.ctx, self.pal = handle, L, ctx, pal
        self.start, self.anchor, self.beam_end = start, anchor, beam_end
        self.elapsed, self.acc, self.burst_done, self.stopped, self.serial = 0.0, 0.0, False, False, 0
        self.point_index = point_index


def cone_dir(rnd: random.Random, axis: np.ndarray, cone: float) -> np.ndarray:
    if cone >= 359:
        z = rnd.uniform(-1, 1)
        a = rnd.uniform(0, 2 * math.pi)
        r = math.sqrt(max(0, 1 - z * z))
        return np.array([r * math.cos(a), r * math.sin(a), z])
    if cone <= 0:
        return axis.copy()
    half = math.radians(cone * 0.5)
    cz = rnd.uniform(math.cos(half), 1.0)
    a = rnd.uniform(0, 2 * math.pi)
    sz = math.sqrt(max(0, 1 - cz * cz))
    t = np.cross(axis, [0, 0, 1]) if abs(axis[2]) < 0.99 else np.cross(axis, [1, 0, 0])
    t /= np.linalg.norm(t)
    b = np.cross(axis, t)
    return axis * cz + (t * math.cos(a) + b * math.sin(a)) * sz


def flat_cone(rnd, d2, cone):
    half = min(cone, 360) * 0.5
    yaw = math.radians(rnd.uniform(-half, half))
    c, s = math.cos(yaw), math.sin(yaw)
    return np.array([d2[0] * c - d2[1] * s, d2[0] * s + d2[1] * c, 0.0])


class Sim:
    def __init__(self, recipes, cam_vecs, seed=7):
        self.recipes = recipes
        self.rnd = random.Random(seed)
        self.emitters: list[Emitter] = []
        self.particles: list[Particle] = []
        self.lights = []
        self.next = 1
        self.cam_right, self.cam_up, self.cam_fwd = cam_vecs
        self.t = 0.0

    # Play (UAbyssVfxSystem::Play)
    def play(self, rid: str, ctx: Ctx, anchor_override: Anchor | None = None) -> int:
        r = self.recipes.get(rid)
        if r is None:
            print(f"  [preview] no recipe {rid}")
            return 0
        h = self.next
        self.next += 1
        pal = self.palette(r["palette"], ctx)
        for L in r["layers"]:
            if (L["big"] > 0 and not ctx.big) or (L["big"] < 0 and ctx.big):
                continue
            n = len(ctx.points) if L["at"] == "points" else 1
            for i in range(n):
                stagger = L["stagger"] if L["stagger"] >= 0 else ctx.stagger
                anchor = anchor_override or self.spawn_anchor(ctx, L["at"], L["height"], L["heightCm"], i)
                beam_end = None
                if L["orient"] == "beam":
                    if L["at"] == "points":
                        beam_end = anchor
                        anchor = Anchor(ctx.origin_actor, "hand", 0, ctx.origin) if i == 0 else \
                            self.spawn_anchor(ctx, "points", L["height"], L["heightCm"], i - 1)
                    else:
                        beam_end = self.spawn_anchor(ctx, L["beamTo"], L["beamToHeight"], 0, i)
                self.emitters.append(Emitter(h, L, ctx, pal, L["delay"] + stagger * i, anchor, beam_end, i))
        if r.get("light"):
            lt = r["light"]
            self.lights.append((h, Anchor(ctx.origin_actor, "ground", 0, ctx.origin), lt["radiusCm"], lt["alpha"],
                                lt.get("flicker", False), ctx.color or pal["mid"]))
        return h

    def stop(self, h):
        for e in self.emitters:
            if e.handle == h:
                e.stopped = True
        self.lights = [l for l in self.lights if l[0] != h]

    def palette(self, name, ctx: Ctx):
        if name == "element":
            if ctx.element:
                return PALETTES[ELEMENT_PAL.get(ctx.element, "steel")]
            return palette_from_color(ctx.color) if ctx.color else PALETTES["steel"]
        if name != "event" and name in PALETTES:
            return PALETTES[name]
        if ctx.color:
            return palette_from_color(ctx.color)
        return PALETTES[ELEMENT_PAL.get(ctx.element or "physical", "steel")]

    def spawn_anchor(self, ctx, at, height, cm, i) -> Anchor:
        if at == "origin":
            return Anchor(ctx.origin_actor, height, cm, ctx.origin)
        if at == "target":
            return Anchor(ctx.target_actor, height, cm, ctx.target)
        if at == "point":
            near = ctx.target_actor
            if near is not None and np.hypot(*(near.pos[:2] - ctx.point[:2])) > 150:
                near = None
            return Anchor(None if height in ("ground", "custom") else near, height, cm, ctx.point)
        if at == "points":
            p = ctx.points[i] if i < len(ctx.points) else ctx.point
            return Anchor(None, height, cm, p)
        return Anchor(None, height, cm, ctx.origin)

    def update(self, dt_ms: float):
        self.t += dt_ms
        for e in list(self.emitters):
            L = e.L
            e.elapsed += dt_ms
            active = e.elapsed - e.start
            done = False
            if active >= 0:
                if L["rate"] <= 0:
                    if not e.burst_done and not e.stopped:
                        c = e.ctx.sparks if L["countFromContext"] else self.rnd.uniform(*L["count"])
                        if L["countByRadius"]:
                            c *= max(1.0, e.ctx.radius / 100.0)
                        for _ in range(max(0, int(round(c)))):
                            self.spawn(e)
                        e.burst_done = True
                    done = True
                else:
                    if e.stopped or (L["duration"] >= 0 and active >= L["duration"]):
                        done = True
                    else:
                        e.acc += min(dt_ms, 250) * L["rate"] / 1000.0
                        n = int(math.floor(e.acc))
                        if n > 0:
                            e.acc -= n
                            for _ in range(n):
                                self.spawn(e)
            elif e.stopped and L["rate"] > 0:
                done = True
            if done:
                self.emitters.remove(e)
        dts = dt_ms / 1000.0
        alive = []
        for p in self.particles:
            p.age += dt_ms
            if p.age >= p.life:
                continue
            alive.append(p)
            if p.L["drag"] > 0:
                p.vel = p.vel * math.exp(-p.L["drag"] * dts)
            p.vel = p.vel + np.array([0, 0, -p.L["gravity"] * dts])
            p.pos = p.pos + p.vel * dts
            p.rot += p.spin * dts
            if p.orbit is not None:
                r0, r1, deg, ang = p.orbit
                p.orbit = (r0, r1, deg, ang + deg * dts)
        self.particles = alive

    def spawn(self, e: Emitter):
        L, ctx, rnd = e.L, e.ctx, self.rnd
        unit = {"ring": ctx.ring, "radius": ctx.radius}.get(L["unit"], 1.0)
        serial = e.serial
        e.serial += 1
        life = max(1.0, rnd.uniform(*L["life"]))
        size0 = max(0.1, rnd.uniform(*L["size"]) * unit)
        alt = L["altEvery"] > 0 and serial % L["altEvery"] == L["altEvery"] - 1
        src = L["altSource"] if alt else L["colorSource"]
        if src == "fixed":
            col = L["altColor"] if alt else L["color"]
        elif src == "white":
            col = (1.0, 1.0, 1.0)
        elif src == "event":
            col = ctx.color if ctx.color else e.pal["mid"]
        else:
            col = e.pal[src]
        col = tuple(c * L["intensity"] for c in col)
        spin = rnd.uniform(*L["spin"])
        rot = rnd.uniform(*L["rotation"])
        variant = rnd.randrange(L["randomVariants"]) if L["randomVariants"] > 1 else L["variant"]
        blow = ctx.blow / max(np.linalg.norm(ctx.blow), 1e-6)
        fwd = np.array([blow[0], blow[1], 0.0])
        right = np.array([-blow[1], blow[0], 0.0])
        if L["align"]:
            if L["orient"] == "ground":
                rot += math.degrees(math.atan2(-blow[0], blow[1]))
            else:
                sx, sy = float(fwd @ self.cam_right), float(fwd @ self.cam_up)
                rot += math.degrees(math.atan2(-sx, sy))
        anchor_pos = e.anchor.pos(self.t)
        if L["at"] == "path":
            o = actor_point(ctx.origin_actor, L["height"], L["heightCm"]) if ctx.origin_actor else \
                ctx.origin + np.array([0, 0, DEFAULT_H.get(L["height"], L["heightCm"])])
            t_ = actor_point(ctx.target_actor, L["height"], L["heightCm"]) if ctx.target_actor else \
                ctx.target + np.array([0, 0, DEFAULT_H.get(L["height"], L["heightCm"])])
            anchor_pos = o + (t_ - o) * rnd.random()
        off = fwd * L["offset"][0] + right * L["offset"][1] + np.array([0, 0, L["offset"][2]])
        disc = np.zeros(3)
        sr = L["spawnRadius"] * unit
        if sr > 0:
            a = rnd.uniform(0, 2 * math.pi)
            rr = sr if L["onRing"] else sr * math.sqrt(rnd.random())
            disc = np.array([math.cos(a) * rr, math.sin(a) * rr, 0.0])
        if L["jitter"] > 0:
            disc[2] = rnd.uniform(-L["jitter"] * 0.5, L["jitter"] * 0.5)
        off = off + disc
        d = L["dir"]
        if d == "random":
            direction = cone_dir(rnd, np.array([0, 0, 1.0]), 360 if L["cone"] >= 359 else L["cone"])
        elif d == "up":
            direction = cone_dir(rnd, np.array([0, 0, 1.0]), 0 if L["cone"] >= 359 else L["cone"])
        elif d == "down":
            direction = cone_dir(rnd, np.array([0, 0, -1.0]), 0 if L["cone"] >= 359 else L["cone"])
        elif d in ("blow", "back"):
            axis = blow if d == "blow" else -blow
            cone = 0 if L["cone"] >= 359 else L["cone"]
            if L["fan"] and L["rate"] <= 0:          # integration.fan: particle i of N at an even yaw
                n = max(1.0, L["count"][1])
                yaw = math.radians(cone * ((serial + 0.5) / n - 0.5))
                c_, s_ = math.cos(yaw), math.sin(yaw)
                direction = np.array([axis[0] * c_ - axis[1] * s_, axis[0] * s_ + axis[1] * c_, 0.0])
            else:
                direction = flat_cone(rnd, axis, cone)
            direction[2] = rnd.uniform(-0.15, 0.35)
            direction /= np.linalg.norm(direction)
        elif d in ("out", "in", "tangent"):
            radial = disc[:2].copy()
            if np.linalg.norm(radial) < 1e-6:
                a = rnd.uniform(0, 2 * math.pi)
                radial = np.array([math.cos(a), math.sin(a)])
            radial /= np.linalg.norm(radial)
            if d == "in":
                radial = -radial
            elif d == "tangent":
                radial = np.array([-radial[1], radial[0]])
            direction = flat_cone(rnd, radial, 0 if L["cone"] >= 359 else L["cone"])
        else:   # flat
            a = rnd.uniform(0, 2 * math.pi)
            direction = np.array([math.cos(a), math.sin(a), 0.0])
        vel = direction * rnd.uniform(*L["speed"]) + np.array([0, 0, rnd.uniform(*L["up"])])
        if L["orient"] == "ground" and d == "out" and np.linalg.norm(vel) > 1e-6:
            rot += math.degrees(math.atan2(-vel[0], vel[1]))
        orbit = None
        if L["orbit"]:
            cnt = max(1.0, L["count"][1])
            ang = rnd.uniform(0, 360) if L["rate"] > 0 else 360.0 * serial / cnt
            orbit = (rnd.uniform(*L["orbitR0"]) * unit, rnd.uniform(*L["orbitR1"]) * unit, rnd.uniform(*L["orbitDeg"]),
                     ang)
        attached = L["attach"] or orbit is not None
        p = Particle(L, e.handle, e.anchor if attached or L["orient"] == "beam" else None, e.beam_end,
                     off if attached else anchor_pos + off, vel, life, size0, size0 * L["grow"], rot, spin, variant,
                     col, orbit)
        if L["mesh"]:
            p.mesh_d = MESH_D.get(L["mesh"], 100.0)
        if L["orient"] == "beam":
            p.anchor = e.anchor
            p.pos = np.zeros(3)
        self.particles.append(p)

    # Render (transforms)
    def instances(self):
        sprites, meshes = [], []
        cr, cu, cf = self.cam_right, self.cam_up, self.cam_fwd
        for p in self.particles:
            L = p.L
            T = min(max(p.age / p.life, 0.0), 1.0)
            st = 1.0 - (1.0 - T) ** L["sizePow"]
            size = p.size0 + (p.size1 - p.size0) * st
            if L["fadeIn"] > 0 and T < L["fadeIn"]:
                alpha = L["alpha0"] * (T / L["fadeIn"])
            else:
                u = (T - L["fadeIn"]) / (1.0 - L["fadeIn"]) if L["fadeIn"] < 1 else 1.0
                alpha = L["alpha0"] + (L["alpha1"] - L["alpha0"]) * min(max(u, 0), 1) ** L["alphaPow"]
            if L["flicker"] > 0:
                alpha *= 1.0 - L["flicker"] * self.rnd.random()
            if alpha <= 0.002 or size <= 0.01:
                continue
            pos = p.pos.copy()
            if p.anchor is not None and L["orient"] != "beam":
                ap = p.anchor.pos(self.t)
                if p.orbit is not None:
                    r0, r1, deg, ang = p.orbit
                    rad = r0 + (r1 - r0) * T
                    a = math.radians(ang)
                    pos = ap + np.array([math.cos(a) * rad, math.sin(a) * rad, 0.0]) + p.pos
                else:
                    pos = ap + p.pos
            rot = math.radians(p.rot)
            c, s = math.cos(rot), math.sin(rot)
            if L["mesh"]:
                scale = size / max(1.0, p.mesh_d)
                if L["orient"] == "velocity" and np.linalg.norm(p.vel) > 1e-6:
                    y = p.vel / np.linalg.norm(p.vel)
                    z = np.array([0, 0, 1.0])
                    if abs(abs(y @ z) - 1) < 1e-4:
                        z = np.array([0, 0, 1.0]) if abs(y[2]) < 1 - 1e-4 else np.array([1.0, 0, 0])
                    x = np.cross(y, z)
                    x /= np.linalg.norm(x)
                    z = np.cross(x, y)
                    R = np.stack([x, y, z], axis=1)
                elif L["orient"] in ("ground", "upright"):
                    R = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])
                else:
                    R = rotator(p.rot * 0.7, p.rot, p.rot * 1.3)
                meshes.append((L["mesh"], pos, R, scale, p.color, alpha))
                continue
            H = size
            W = size * L["aspect"]
            o = L["orient"]
            if o == "billboard":
                X = cr * c + cu * s
                Y = cu * c - cr * s
                center = pos
            elif o == "ground":
                X = np.array([c, s, 0.0])
                Y = np.array([-s, c, 0.0])
                center = pos
            elif o == "upright":
                fr = np.array([cr[0], cr[1], 0.0])
                fr = fr / max(np.linalg.norm(fr), 1e-6)
                upv = np.array([0, 0, 1.0])
                X = fr * c + upv * s
                Y = upv * c - fr * s
                center = pos + Y * (size * 0.5)
            elif o == "velocity":
                spd = float(np.linalg.norm(p.vel))
                sx, sy = float(p.vel @ cr), float(p.vel @ cu)
                Y = cu.copy()
                if abs(sx) + abs(sy) > 1e-4:
                    Y = cr * sx + cu * sy
                    Y /= np.linalg.norm(Y)
                X = np.cross(Y, cf)
                X /= max(np.linalg.norm(X), 1e-6)
                H = size + L["stretch"] * spd
                center = pos
            else:   # beam
                A = p.anchor.pos(self.t) if p.anchor else pos
                B = p.beam_end.pos(self.t) if p.beam_end else A + np.array([0, 0, 100.0])
                span = B - A
                ln = float(np.linalg.norm(span))
                if ln < 1:
                    continue
                Y = span / ln
                X = np.cross(Y, cf)
                X = X / np.linalg.norm(X) if np.linalg.norm(X) > 1e-6 else cr
                W, H = size, ln
                center = (A + B) * 0.5
            sprites.append((L["sprite"], L["blend"], p.variant, center, X * (W * 0.5), Y * (H * 0.5), p.color,
                            alpha, o))
        for h, anchor, radius, a, flick, col in self.lights:
            if not any(p.handle == h for p in self.particles) and not any(e.handle == h for e in self.emitters):
                continue
            k = a + (0.05 * math.sin(self.t * 0.007) + 0.03 * math.sin(self.t * 0.013 + 2.3)) if flick else a
            center = anchor.pos(self.t) + np.array([0, 0, 2.0])
            sprites.append(("LightPool", "additive", 0, center, np.array([radius, 0, 0.0]), np.array([0, radius, 0.0]),
                            col, min(max(k, 0.0), 1.0) * 0.5, "ground"))
        return sprites, meshes


def rotator(pitch, yaw, roll):
    """FRotator → matrix with the axes as columns (UE FRotationTranslationMatrix)."""
    P, Y, Rr = (math.radians(v) for v in (pitch, yaw, roll))
    SP, CP, SY, CY, SR, CR = math.sin(P), math.cos(P), math.sin(Y), math.cos(Y), math.sin(Rr), math.cos(Rr)
    x = [CP * CY, CP * SY, SP]
    y = [SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP]
    z = [-(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP]
    return np.array([x, y, z]).T


MESH_D: dict[str, float] = {}
SOFT_GROUND = 0.08          # shipped: DepthFade(8 cm) on every quad; --soft-ground → 0.008 (proposed runtime fix)
CELL_DIR: Path | None = None


# ── UE ↔ Blender ───────────────────────────────────────────────────────────────────────────────────────
S = np.diag([1.0, -1.0, 1.0])


def ue_to_bl(p):
    return Vector((p[0] / 100.0, -p[1] / 100.0, p[2] / 100.0))


def bl_to_ue_dir(v):
    return np.array([v[0], -v[1], v[2]])


# ── sprite textures ────────────────────────────────────────────────────────────────────────────────────
_TEX: dict[str, np.ndarray] = {}


def tex(name: str) -> np.ndarray:
    if name not in _TEX:
        p = TEX_DIR / f"T_FX_{name}.png"
        if not p.exists():
            p = TEX_DIR / "T_FX_Glow.png"
        _TEX[name] = pngio.read_png(p).astype(np.float32) / 255.0
    return _TEX[name]


def bilinear(img, u, v):
    h, w = img.shape[:2]
    x = np.clip(u * w - 0.5, 0, w - 1.001)
    y = np.clip(v * h - 0.5, 0, h - 1.001)
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = (x - x0)[..., None], (y - y0)[..., None]
    a = img[y0, x0]
    b = img[y0, x0 + 1]
    c = img[y0 + 1, x0]
    d = img[y0 + 1, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


class Camera:
    def __init__(self, cam_obj, res, box_px):
        self.M = np.array(cam_obj.matrix_world)
        self.O = self.M[:3, 3]
        self.R = self.M[:3, :3]
        self.tan = math.tan(cam_obj.data.angle / 2)
        self.res = res
        self.x0, self.y0, self.w, self.h = box_px
        self.fwd = -self.R[:, 2]

    def project(self, p):
        """Blender world point → (x, y) pixel in the crop (y down) and view depth."""
        q = self.R.T @ (np.asarray(p) - self.O)
        z = -q[2]
        if z <= 1e-4:
            return None
        nx = q[0] / z / self.tan
        ny = q[1] / z / (self.tan * self.res[1] / self.res[0])
        return ((nx + 1) * 0.5 * self.res[0] - self.x0, (1 - ny) * 0.5 * self.res[1] - self.y0, z)

    def rays(self, xs, ys):
        nx = ((xs + self.x0 + 0.5) / self.res[0]) * 2 - 1
        ny = 1 - ((ys + self.y0 + 0.5) / self.res[1]) * 2
        d = np.stack([nx * self.tan, ny * self.tan * self.res[1] / self.res[0], -np.ones_like(nx)], axis=-1)
        return d @ self.R.T


def composite_sprites(img, depth, cam: Camera, sprites):
    """Draw the sprite quads into ``img`` (linear HxWx3) with depth ``depth`` (view z, m)."""
    H, W = img.shape[:2]
    order = [s for s in sprites if s[1] == "translucent"] + [s for s in sprites if s[1] != "translucent"]
    for name, blend, variant, center, hx, hy, col, alpha, orient in order:
        C = ue_to_bl(center)
        Ax = Vector((hx[0] / 100, -hx[1] / 100, hx[2] / 100))
        Ay = Vector((hy[0] / 100, -hy[1] / 100, hy[2] / 100))
        corners = [C + Ax * sx + Ay * sy for sx in (-1, 1) for sy in (-1, 1)]
        proj = [cam.project(np.array(c)) for c in corners]
        if any(p is None for p in proj):
            continue
        xs = [p[0] for p in proj]
        ys = [p[1] for p in proj]
        x0, x1 = max(0, int(math.floor(min(xs)))), min(W, int(math.ceil(max(xs))) + 1)
        y0, y1 = max(0, int(math.floor(min(ys)))), min(H, int(math.ceil(max(ys))) + 1)
        if x0 >= x1 or y0 >= y1:
            continue
        gy, gx = np.mgrid[y0:y1, x0:x1].astype(np.float64)
        d = cam.rays(gx, gy)
        n = np.cross(np.array(Ax), np.array(Ay))
        nn = np.linalg.norm(n)
        if nn < 1e-9:
            continue
        n /= nn
        denom = d @ n
        ok = np.abs(denom) > 1e-9
        t = np.where(ok, (np.array(C) - cam.O) @ n / np.where(ok, denom, 1), -1)
        P = cam.O + d * t[..., None]
        rel = P - np.array(C)
        ax2, ay2 = np.array(Ax) @ np.array(Ax), np.array(Ay) @ np.array(Ay)
        sx = rel @ np.array(Ax) / ax2
        sy = rel @ np.array(Ay) / ay2
        inside = ok & (t > 0) & (np.abs(sx) <= 1) & (np.abs(sy) <= 1)
        if not inside.any():
            continue
        u = (sx + 1) * 0.5
        v = (1 - sy) * 0.5
        T = tex(name)
        cells = CELLS.get(name, 1)
        if cells > 1:
            cw = T.shape[1] // cells
            vi = int(min(max(variant, 0), cells - 1))
            T = T[:, vi * cw:(vi + 1) * cw]
        smp = bilinear(T, u, v)
        pz = (rel @ cam.fwd) + (np.array(C) - cam.O) @ cam.fwd
        # UE DepthFade(8 cm) on every quad (M_AF_FX_Sprite); the recipes lift ground quads to 8-11 cm so they are
        # whole under it (fx_recipes.ground_lift). --soft-ground draws the proposed 0.8 cm ground fade instead.
        fade = SOFT_GROUND if orient == "ground" else 0.08
        soft = np.clip((depth[y0:y1, x0:x1] - pz) / fade, 0, 1)
        a = smp[..., 3] * min(alpha, 1.0) * soft * inside
        rgb = smp[..., :3] * np.array(col)
        reg = img[y0:y1, x0:x1]
        if blend == "translucent":
            reg[:] = reg * (1 - a[..., None]) + rgb * a[..., None]
        else:
            reg[:] = reg + rgb * a[..., None]


def bloom_lin(lin: np.ndarray, strength: float = 0.8) -> np.ndarray:
    bright = np.clip(lin - 1.0, 0, None)
    if bright.max() <= 0:
        return lin
    add = sum(w * review.blur(bright, s) for s, w in review.BLOOM_LOBES)
    return lin + strength * add


def to_srgb8(lin):
    return (review._lin_to_srgb(np.clip(lin, 0, 1)) * 255 + 0.5).astype(np.uint8)


# ── the stage: terrain, characters, FX mesh library ───────────────────────────────────────────────────
# the shipped world props the world / prop recipes sit on: (asset, category, palette family, outline px, yaw°)
CLUE_YAW = -135.0   # Blender yaw (UE +135°): the magnifier lens (normal (0, -.766, .643), tilted back 40°) faces the W1 camera (n·v = .985)
WORLD_PROPS = {
    "camp.campfire": ("SM_Prop_Camp_Campfire", "Props", "Props_Plains", 1.4, 0.0),
    "camp.torch": ("SM_Prop_Camp_Torch", "Props", "Props_Plains", 1.4, 0.0),
    "camp.hearthstone": ("SM_Prop_Camp_Hearthstone", "Props", "Props_Plains", 1.4, -30.0),
    # the gates face into the map (mesh −Y): Blender yaw −135° turns that side to the W1 camera (a yaw of 45° showed
    # the back of the sealed gate — no planks, chain or padlock)
    "exit.portal": ("SM_Prop_ExitPortal", "Props", "Props_Plains", 2.0, -135.0),
    "exit.sealed": ("SM_Prop_ExitGate_Sealed", "Props", "Props_Plains", 2.0, -135.0),
    "prop.lore": ("SM_Prop_Lore_AncientTablet", "Props", "Props_Plains", 2.0, -30.0),
    "prop.gather": ("SM_Prop_Gather_EmeraldHerb", "Props", "Props_Plains", 2.0, 0.0),
    "prop.clue": ("SM_FX_ClueMagnifier", "VFX", "Props_Plains", 2.0, CLUE_YAW),
    "prop.chest": ("SK_Prop_TreasureChest", "Props", "Props_Plains", 2.0, 150.0),
    "prop.chest.open": ("SK_Prop_TreasureChest", "Props", "Props_Plains", 2.0, 150.0),
    "prop.event": ("SM_Prop_Event_RunePillar", "Props", "Props_Plains", 2.0, -30.0),
    "loot.bag": ("SM_Loot_Bag", "Pickups", "Pickups", 2.0, -20.0),
}

# the three heroes as shipped: (SK, held items [(SM, bone)], clips)
HERO_KIT = {
    "warrior": ("SK_Hero_Warrior", [("SM_Hero_Warrior_Sword", "weapon_r"), ("SM_Hero_Warrior_Shield", "weapon_l")],
                ["Idle", "Attack01", "Cast01", "Cast_Whirlwind", "Cast_Charge", "Dodge"]),
    "mage": ("SK_Hero_Mage", [("SM_Hero_Mage_Staff", "weapon_r")],
             ["Idle", "Attack01", "Cast01", "Cast_Blizzard", "Cast_Meteor", "Dodge"]),
    "rogue": ("SK_Hero_Rogue", [("SM_Hero_Rogue_Dagger", "weapon_r"), ("SM_Hero_Rogue_Dagger_L", "weapon_l")],
              ["Idle", "Attack01", "Cast01", "Cast_Multishot", "Dodge"]),
}
SOCKET_OF = {"chest": "fx_chest", "head": "fx_head", "overhead": "fx_overhead", "hand": "fx_hand_r"}
_MAN: dict | None = None


def manifest() -> dict:
    global _MAN
    if _MAN is None:
        _MAN = json.loads(paths.manifest_path().read_text())["assets"]
    return _MAN


def clip_info(sk: str, clip: str) -> dict:
    an = manifest()[sk]["anims"]
    an = an if isinstance(an, list) else list(an.values())
    return next((a for a in an if a["name"] == clip), {"lengthMs": 1000.0, "loop": True, "notifies": []})


def beat_ms(sk: str, clip: str) -> float:
    """The clip's event beat: Contact (attacks), Release (casts), the first notify otherwise, else 0."""
    nts = clip_info(sk, clip).get("notifies", [])
    for want in ("Contact", "Release"):
        for n in nts:
            if n["name"] == want:
                return float(n["ms"])
    return 0.0


def ue_cm(v) -> np.ndarray:
    return np.array([v[0] * 100.0, -v[1] * 100.0, v[2] * 100.0])


def socket_world(arm, sock: dict) -> Vector:
    """A manifest socket on the posed armature (rest location UE cm → Blender m, carried by its bone)."""
    rest = Vector((sock["restLocCm"][0] / 100.0, -sock["restLocCm"][1] / 100.0, sock["restLocCm"][2] / 100.0))
    b = arm.data.bones[sock["bone"]]
    pb = arm.pose.bones[sock["bone"]]
    return arm.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ rest))


def item_socket_world(obj, sock: dict) -> Vector:
    """A static mesh's manifest socket (``locCm`` in its UE mesh space) in Blender world."""
    loc = sock["locCm"]
    return obj.matrix_world @ Vector((loc[0] / 100.0, -loc[1] / 100.0, loc[2] / 100.0))


class Stage:
    def __init__(self, classes=("warrior", "mage", "rogue")):
        scene.reset()
        bpy.context.scene.view_settings.view_transform = "Standard"
        self.ground = self.make_ground()
        self.heroes = {c: self.load_hero(c) for c in classes}
        self.hero = self.heroes.get("warrior") or next(iter(self.heroes.values()))
        self.gobs = [self.load_goblin(i) for i in range(3)]
        self.smith = None
        self.mesh_src: dict[str, bpy.types.Object] = {}
        self.live: list[bpy.types.Object] = []
        self.depth_mat = review._flat_material("AF_FxDepth", "depth")
        self.depth_hull = self._depth_hull()
        self.white = _emit_material("AF_FxMaskW", (1.0, 1.0, 1.0))
        self.black = _emit_material("AF_FxMaskK", (0.0, 0.0, 0.0))
        self.black_hole = review._flat_material("AF_FxHole", "transparent")
        self.timed = None                 # (character dict, clip, clip-time ms) driving the scene frame

    def _depth_hull(self):
        m = bpy.data.materials.new("AF_FxDepthHull")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        cam = nt.nodes.new("ShaderNodeCameraData")
        em = nt.nodes.new("ShaderNodeEmission")
        nt.links.new(cam.outputs["View Z Depth"], em.inputs["Color"])
        tr = nt.nodes.new("ShaderNodeBsdfTransparent")
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        mx = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(geo.outputs["Backfacing"], mx.inputs["Fac"])
        nt.links.new(em.outputs[0], mx.inputs[1])
        nt.links.new(tr.outputs[0], mx.inputs[2])
        nt.links.new(mx.outputs[0], out.inputs["Surface"])
        return m

    def make_ground(self):
        import terrain as T
        C0, R0, C1, R1 = -8, -8, 9, 9
        rows, cols = R1 - R0, C1 - C0
        m = np.zeros((rows, cols), np.int32)
        for r in range(rows):
            for c in range(cols):
                col, row = c + C0, r + R0
                if abs((col + row) - 1) <= 1 and -6 <= col - row <= 6:      # a dirt path along the screen axis
                    m[r, c] = 1
                if (col - 4) ** 2 + (row + 5) ** 2 <= 3:
                    m[r, c] = 1
        cache = K.SCRATCH / "preview_ground.png"
        K.SCRATCH.mkdir(parents=True, exist_ok=True)
        if not cache.exists():
            texs = T.load_textures()
            img = T.composite(m, T.Layers(texs["tex"], texs["details"]), ppt=72, origin=(C0, R0))
            pngio.write_png(cache, img)
        me = bpy.data.meshes.new("fx_ground")
        x0, x1 = C0 - 0.5, C1 - 0.5
        y0, y1 = -(R0 - 0.5), -(R1 - 0.5)
        me.from_pydata([(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], [], [(0, 1, 2, 3)])
        uv = me.uv_layers.new(name="UVMap")
        for li, (u, v) in enumerate(((0, 1), (1, 1), (1, 0), (0, 0))):
            uv.data[li].uv = (u, v)
        ob = bpy.data.objects.new("fx_ground", me)
        scene.link(ob)
        mat = bpy.data.materials.new("fx_ground")
        mat.use_nodes = True
        nt = mat.node_tree
        nt.nodes.clear()
        tx = nt.nodes.new("ShaderNodeTexImage")
        tx.image = bpy.data.images.load(str(cache))
        tx.interpolation = "Linear"
        em = nt.nodes.new("ShaderNodeEmission")
        o = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(tx.outputs["Color"], em.inputs["Color"])
        nt.links.new(em.outputs[0], o.inputs["Surface"])
        me.materials.append(mat)
        return ob

    def _clip_armatures(self, arm, token, category, clips):
        out = {}
        for clip in clips:
            f = paths.export_root() / category / f"A_{token}_{clip}.fbx"
            if not f.exists():
                continue
            before = set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=str(f))
            new = [o for o in bpy.data.objects if o not in before]
            a2 = next((o for o in new if o.type == "ARMATURE"), None)
            for o in new:
                o.hide_render = True
            if a2 is not None:
                out[clip] = a2
        for pb in arm.pose.bones:          # import_character added COPY_TRANSFORMS (→ Idle); keep one per bone
            if not any(c.type == "COPY_TRANSFORMS" for c in pb.constraints) and out:
                c = pb.constraints.new("COPY_TRANSFORMS")
                c.target, c.subtarget = next(iter(out.values())), pb.name
                c.target_space = c.owner_space = "POSE"
        return out

    # ── loading ────────────────────────────────────────────────────────────────────────────────────────
    def _static_toon(self, name: str, category: str, family: str, cls: str):
        """A shipped static mesh in its family's toon + outline look (screen-constant preview ink)."""
        import envkit as E
        f = paths.export_root() / category / f"{name}.fbx"
        if not f.exists():
            return None
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(f))
        new = [o for o in bpy.data.objects if o not in before]
        m = next(o for o in new if o.type == "MESH")
        for o in new:
            if o is not m:
                bpy.data.objects.remove(o)
        m.parent = None
        m.matrix_world.identity()
        fam = K._manifest_palette(name) or family
        E._set_slots(m, *E.toon_material_files(fam))
        m["af_outline_width"] = outline.WIDTH_CM[cls] / 100.0
        m["af_outline_class"] = cls
        review.set_outline_px([m], outline.SCREEN_PX_1080[cls])
        return m

    def _attach_at(self, obj, arm, bone: str, rest_loc_cm) -> None:
        """Parent ``obj`` to ``bone`` with its frame = the bone's frame moved to a socket's rest location."""
        from types import SimpleNamespace
        from kit import rig as krig
        krig.attach_to_bone(obj, SimpleNamespace(obj=arm), bone)
        q = Vector((rest_loc_cm[0] / 100.0, -rest_loc_cm[1] / 100.0, rest_loc_cm[2] / 100.0))
        obj.matrix_basis = Matrix.Translation(arm.data.bones[bone].matrix_local.inverted() @ q)

    def _clip_armatures(self, arm, token, category, clips):
        out = {}
        for clip in clips:
            f = paths.export_root() / category / f"A_{token}_{clip}.fbx"
            if not f.exists():
                continue
            before = set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=str(f))
            new = [o for o in bpy.data.objects if o not in before]
            a2 = next((o for o in new if o.type == "ARMATURE"), None)
            for o in new:
                o.hide_render = True
            if a2 is not None:
                out[clip] = a2
        for pb in arm.pose.bones:          # one COPY_TRANSFORMS per bone, retargeted per scenario
            if not any(c.type == "COPY_TRANSFORMS" for c in pb.constraints) and out:
                c = pb.constraints.new("COPY_TRANSFORMS")
                c.target, c.subtarget = next(iter(out.values())), pb.name
                c.target_space = c.owner_space = "POSE"
        return out

    def load_hero(self, cls: str):
        sk, items, clips = HERO_KIT[cls]
        root, meshes, arm = K.import_character(sk, "Characters", "Heroes", None, 0, (0.0, 0.0, 0.0), 0.0,
                                               [(n, "Weapons", bone) for n, bone in items], outline_class="hero")
        anims = self._clip_armatures(arm, sk[3:], "Characters", clips)
        for m in meshes:
            review.set_outline_px([m], outline.SCREEN_PX_1080["hero"] if m["af_outline_class"] == "hero" else
                                  outline.SCREEN_PX_1080["weapon"])
        ch = {"root": root, "meshes": meshes, "arm": arm, "anims": anims, "sk": sk, "cls": cls, "swap": {},
              "items": {n: next((m for m in meshes if m.name.startswith(n) and
                                 not m.name.startswith(n + "_L")), None) for n, _ in items}}
        man = manifest()[sk]
        ch["sockets"] = {s["name"]: s for s in man.get("sockets", [])}
        fx = man.get("fx") or {}
        ch["spirit"] = fx.get("spiritColor", "#FFB45C")
        if cls == "mage":                  # the crystal floats on the staff's `crystal` socket
            staff = ch["items"].get("SM_Hero_Mage_Staff")
            cry = self._static_toon("SM_Hero_Mage_Crystal", "Weapons", "Heroes", "weapon")
            if staff is not None and cry is not None:
                sock = next(x for x in manifest()["SM_Hero_Mage_Staff"]["sockets"] if x["name"] == "crystal")
                cry.parent = staff
                cry.matrix_parent_inverse = Matrix.Identity(4)
                cry.matrix_basis = Matrix.Translation(Vector((sock["locCm"][0] / 100.0, -sock["locCm"][1] / 100.0,
                                                              sock["locCm"][2] / 100.0)))
                meshes.append(cry)
                ch["crystal"] = cry
        if cls == "rogue":                 # hip quiver (body socket) + the flash-bow swapped in by Cast_Multishot
            q = self._static_toon("SM_Hero_Rogue_Quiver", "Weapons", "Heroes", "weapon")
            if q is not None:
                s_ = ch["sockets"]["quiver"]
                self._attach_at(q, arm, s_["bone"], s_["restLocCm"])
                meshes.append(q)
            bow = self._static_toon("SM_Hero_Rogue_Bow", "Weapons", "Heroes", "weapon")
            if bow is not None:
                from types import SimpleNamespace
                from kit import rig as krig
                krig.attach_to_bone(bow, SimpleNamespace(obj=arm), "weapon_l")
                bow.hide_render = True
                ch["swap"] = {"bow": bow, "dagger_l": next((m for m in meshes
                                                            if m.name.startswith("SM_Hero_Rogue_Dagger_L")), None)}
        return ch

    def load_goblin(self, i):
        r = K.import_goblin("Idle", 0)
        root, meshes, arm = r
        anims = self._clip_armatures(arm, "Mon_Goblin", "Monsters", ["Idle", "Hurt"])
        for m in meshes:
            review.set_outline_px([m], outline.SCREEN_PX_1080["monster"])
        return {"root": root, "meshes": meshes, "arm": arm, "anims": anims, "sk": "SK_Mon_Goblin", "cls": "goblin"}

    def load_smith(self):
        """The blacksmith (Work_Hammer, hammer + tongs) with the anvil stump at its scenery offset."""
        if self.smith is not None:
            return self.smith
        r = K.import_character("SK_NPC_Blacksmith", "NPCs", "NPC", None, 0, (0.0, 0.0, 0.0), 0.0,
                               [("SM_NPCKit_Prop_Hammer", "NPCs", "weapon_r"),
                                ("SM_NPCKit_Prop_Tongs", "NPCs", "weapon_l")], outline_class="npc")
        root, meshes, arm = r
        anims = self._clip_armatures(arm, "NPC_Blacksmith", "NPCs", ["Work_Hammer"])
        for m in meshes:
            review.set_outline_px([m], outline.SCREEN_PX_1080["npc"] if m["af_outline_class"] == "npc" else
                                  outline.SCREEN_PX_1080["weapon"])
        sc = manifest()["SK_NPC_Blacksmith"]["scenery"][0]
        stump = self._static_toon(sc["asset"], "Props", "Props_Plains", "interactive")
        if stump is not None:
            stump.parent = root
            stump.matrix_parent_inverse = Matrix.Identity(4)
            o = sc["offsetCm"]
            stump.matrix_basis = Matrix.Translation(Vector((o[0] / 100.0, -o[1] / 100.0, o[2] / 100.0))) @ \
                Matrix.Rotation(math.radians(sc.get("yawDeg", 0.0)), 4, "Z")
            meshes.append(stump)
        hammer = next((m for m in meshes if m.name.startswith("SM_NPCKit_Prop_Hammer")), None)
        self.smith = {"root": root, "meshes": meshes, "arm": arm, "anims": anims, "sk": "SK_NPC_Blacksmith",
                      "cls": "smith", "stump": stump, "hammer": hammer}
        for m in meshes:
            m.hide_render = True
        return self.smith

    # ── posing / time ───────────────────────────────────────────────────────────────────────────────────
    def pose(self, ch, clip, at_ue, facing_ue, visible=True):
        arm = ch["arm"]
        target = ch["anims"].get(clip) or ch["anims"].get("Idle") or next(iter(ch["anims"].values()))
        for pb in arm.pose.bones:
            for c in pb.constraints:
                if c.type == "COPY_TRANSFORMS":
                    c.target = target
        ch["clip"] = clip if clip in ch["anims"] else "Idle"
        ch["root"].location = ue_to_bl(at_ue)
        d = np.asarray(facing_ue, float)
        db = Vector((d[0], -d[1], 0))
        ch["root"].rotation_euler = (0, 0, math.atan2(db.x, -db.y))
        for m in ch["meshes"]:
            m.hide_render = not visible
        for k, o in ch.get("swap", {}).items():
            if o is not None and visible:
                o.hide_render = k == "bow"

    def clip_time(self, ch, ms: float) -> None:
        """Drive the scene frame so ``ch``'s current clip shows clip time ``ms`` (clamped / wrapped)."""
        info = clip_info(ch["sk"], ch["clip"])
        L = float(info.get("lengthMs", 1000.0))
        ms = ms % L if info.get("loop") else min(max(ms, 0.0), L - 1000.0 / 60.0)
        a2 = ch["anims"][ch["clip"]]
        act = a2.animation_data.action if a2.animation_data else None
        f0 = act.frame_range[0] if act is not None else 0.0
        f = f0 + ms * 0.06
        fi = int(math.floor(f))
        bpy.context.scene.frame_set(fi, subframe=f - fi)
        # the rogue's flash-bow is in the off hand between FX_BowShow and FX_BowHide of Cast_Multishot
        sw = ch.get("swap") or {}
        if sw.get("bow") is not None and not sw["bow"].hide_viewport:
            nts = {n["name"]: n["ms"] for n in info.get("notifies", [])}
            on = "FX_BowShow" in nts and nts["FX_BowShow"] <= ms < nts.get("FX_BowHide", 1e9)
            vis = not ch["meshes"][0].hide_render
            sw["bow"].hide_render = not (on and vis)
            if sw.get("dagger_l") is not None:
                sw["dagger_l"].hide_render = on or not vis
        ch["ms"] = ms

    def sockets_ue(self, ch) -> dict:
        """fx anchor heights of a posed character → UE cm world positions (AAbyssCharacterActor anchors)."""
        bpy.context.view_layer.update()
        out = {}
        for h, name in SOCKET_OF.items():
            sk = ch.get("sockets", {}).get(name)
            if sk is not None:
                out[h] = ue_cm(socket_world(ch["arm"], sk))
        if ch.get("crystal") is not None and "hand" in out:
            # spec 8.1: the mage casts from the staff crystal — `hand` resolves to the main-hand weapon's `crystal`
            # socket when it has one (runtime patch: AAbyssCharacterActor::GetAnchorLocation, HandR)
            out["hand_r"] = out["hand"]
            out["hand"] = ue_cm(ch["crystal"].matrix_world.translation)
        return out

    def show_prop(self, key: str | None, at_ue, yaw: float | None = None, clip: str | None = None):
        """Show the world prop of ``key`` (imported once from its shipped FBX, toon + outline from the family
        palette PNGs, screen-constant line-tone ink) at ``at_ue``; hide every other prop. Skeletal props (the
        chest) are posed by their clip armature (``clip``) or drawn in the rest pose."""
        import envkit as E
        props = getattr(self, "_props", {})
        self._props = props
        for k, objs in props.items():
            for o in objs.get("objs", []):
                o.hide_render = True
        if key is None or key not in WORLD_PROPS:
            return None
        asset, cat, fam, px, yaw0 = WORLD_PROPS[key]
        if asset not in props:
            f = paths.export_root() / cat / f"{asset}.fbx"
            if not f.exists():
                props[asset] = {"objs": []}
                return None
            before = set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=str(f))
            new = [o for o in bpy.data.objects if o not in before]
            mt, mo = E.toon_material_files(fam)
            meshes = [o for o in new if o.type == "MESH"]
            keep = [o for o in new if o.type in ("MESH", "ARMATURE")]
            for o in [o for o in new if o.type not in ("MESH", "ARMATURE")]:
                for c in list(o.children):
                    mw = c.matrix_world.copy()
                    c.parent = None
                    c.matrix_world = mw
                bpy.data.objects.remove(o)
            for m in meshes:
                E._set_slots(m, mt, mo)
            root = bpy.data.objects.new(f"{asset}_Root", None)
            scene.link(root)
            for o in keep:
                if o.parent is None:
                    o.parent = root
            for m in meshes:
                if "af_outline_width" not in m:
                    m["af_outline_width"] = outline.WIDTH_CM["interactive"] / 100.0
                review.set_outline_px([m], px)
            arm = next((o for o in keep if o.type == "ARMATURE"), None)
            anims = {}
            if arm is not None:
                anims = self._clip_armatures(arm, asset[3:], cat, ["Open"])
                for a2 in anims.values():
                    a2.parent = root
            props[asset] = {"objs": [root] + meshes, "root": root, "arm": arm, "anims": anims, "meshes": meshes,
                            "sk": asset, "cls": "prop"}
        P = props[asset]
        if not P["objs"]:
            return None
        root = P["root"]
        root.location = ue_to_bl(at_ue)
        root.rotation_euler = (0, 0, math.radians(yaw0 if yaw is None else yaw))
        if P.get("arm") is not None:
            for pb in P["arm"].pose.bones:
                for c in pb.constraints:
                    if c.type == "COPY_TRANSFORMS":
                        c.mute = clip is None
                        if clip and clip in P["anims"]:
                            c.target = P["anims"][clip]
            P["clip"] = clip
        for o in P["objs"][1:]:
            o.hide_render = False
        return P

    def set_frame(self, frame):
        bpy.context.scene.frame_set(int(frame))

    def mesh_obj(self, name):
        if name not in self.mesh_src:
            cat = "Pickups" if name.startswith("SM_Pickup") else "VFX"
            o = K.static_import(name, cat)
            o.hide_render = True
            o.name = f"src_{name}"
            self.mesh_src[name] = o
        return self.mesh_src[name]

    def place_meshes(self, meshes):
        for o in self.live:
            bpy.data.objects.remove(o)
        self.live = []
        for name, pos, R, scale, col, alpha in meshes:
            src = self.mesh_obj(name)
            o = src.copy()
            scene.link(o)
            o.hide_render = False
            Rb = S @ R @ S
            m = Matrix.Translation(ue_to_bl(pos)) @ Matrix([list(Rb[0]) + [0], list(Rb[1]) + [0],
                                                           list(Rb[2]) + [0], [0, 0, 0, 1]]) @ \
                Matrix.Diagonal((scale, scale, scale, 1.0))
            o.matrix_world = m
            a = min(1.0, max(0.0, round(alpha * 16) / 16))
            mat = K.fx_material(col, a)
            o.data = src.data.copy()
            for i in range(len(o.data.materials)):
                o.data.materials[i] = mat
            if not len(o.data.materials):
                o.data.materials.append(mat)
            self.live.append(o)

    def render(self, box):
        """(linear HDR colour, view depth) of the crop."""
        sc = bpy.context.scene
        review.setup_render(RES, 8)
        col = review.render_array(box, exr=True)
        # depth: swap every material to view-depth (hulls keep the back-face rule)
        saved = []
        for o in bpy.data.objects:
            if o.type != "MESH" or o.hide_render:
                continue
            mats = o.data.materials
            saved.append((o, [mats[i] for i in range(len(mats))]))
            for i in range(len(mats)):
                if o.name.startswith("fx_ground"):
                    mats[i] = self.depth_mat
                else:
                    mats[i] = self.depth_hull
        try:
            sc.cycles.samples = 1
            sc.cycles.pixel_filter_type = "BOX"
            sc.cycles.filter_width = 0.01
            dep = review.render_array(box, exr=True)[..., 0]
        finally:
            for o, mats in saved:
                for i, m in enumerate(mats):
                    o.data.materials[i] = m
            sc.cycles.pixel_filter_type = "BLACKMAN_HARRIS"
            sc.cycles.filter_width = 1.5
        return col, dep

    def render_mask(self, box, objs, slot0_only=True) -> np.ndarray:
        """Coverage (0..1) of ``objs``' toon surfaces (the slot the StatusTint multiplies), everything else
        black and occluding."""
        sc = bpy.context.scene
        want = set(o.name for o in objs)
        saved = []
        for o in bpy.data.objects:
            if o.type != "MESH" or o.hide_render:
                continue
            mats = o.data.materials
            saved.append((o, [mats[i] for i in range(len(mats))]))
            for i in range(len(mats)):
                on = o.name in want and (i == 0 or not slot0_only)
                # hull slots (1) drop out: an emission hull would enclose and hide the body it outlines
                mats[i] = self.white if on else (self.black_hole if i == 1 else self.black)
        try:
            review.setup_render(RES, 4, bg_hex="#000000")
            m = review.render_array(box, exr=True)[..., 0]
        finally:
            for o, mats in saved:
                for i, mm in enumerate(mats):
                    o.data.materials[i] = mm
        return np.clip(m, 0.0, 1.0)

    def render_ghosts(self, box, ch, ghosts, tint_hex) -> np.ndarray:
        """The dodge afterimages (M_AF_Ghost look: class spirit tint × (0.35 + 0.9 fresnel), additive): the hero's
        toon surfaces re-posed at each ghost's capture time and place, rendered alone over black; returns a list
        of linear RGB layers (one per ghost)."""
        sc = bpy.context.scene
        g = self._ghost_material(tint_hex)
        layers = []
        hidden = [o for o in bpy.data.objects if o.type == "MESH" and not o.hide_render]
        for o in hidden:
            o.hide_render = True
        saved_loc = ch["root"].location.copy()
        try:
            for at_ue, ms in ghosts:
                ch["root"].location = ue_to_bl(at_ue)
                self.clip_time(ch, ms)
                saved = []
                for o in ch["meshes"]:
                    if o.get("af_outline_class") in ("hero", "weapon") and not o.name.startswith("SM_Hero_Rogue_Bow"):
                        o.hide_render = False
                        mats = o.data.materials
                        saved.append((o, [mats[i] for i in range(len(mats))]))
                        for i in range(len(mats)):
                            mats[i] = g if i == 0 else self.black_hole
                review.setup_render(RES, 8, bg_hex="#000000")
                img = review.render_array(box, exr=True)[..., :3]
                for o, mats in saved:
                    for i, mm in enumerate(mats):
                        o.data.materials[i] = mm
                    o.hide_render = True
                layers.append(img)
        finally:
            ch["root"].location = saved_loc
            for o in hidden:
                o.hide_render = False
        return layers

    def _ghost_material(self, tint_hex: str):
        name = f"AF_FxGhost_{tint_hex}"
        m = bpy.data.materials.get(name)
        if m is not None:
            return m
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        lw = nt.nodes.new("ShaderNodeLayerWeight")
        lw.inputs["Blend"].default_value = 0.5
        mr = nt.nodes.new("ShaderNodeMapRange")       # 0.35 + 0.9 · fresnel
        mr.inputs["To Min"].default_value = 0.35
        mr.inputs["To Max"].default_value = 1.25
        nt.links.new(lw.outputs["Facing"], mr.inputs["Value"])
        mul = nt.nodes.new("ShaderNodeMix")
        mul.data_type = "RGBA"
        mul.blend_type = "MULTIPLY"
        mul.inputs["Factor"].default_value = 1.0
        lin = hex_lin(tint_hex)
        mul.inputs["A"].default_value = (lin[0], lin[1], lin[2], 1.0)
        nt.links.new(mr.outputs["Result"], mul.inputs["B"])
        em = nt.nodes.new("ShaderNodeEmission")
        nt.links.new(mul.outputs["Result"], em.inputs["Color"])
        nt.links.new(em.outputs[0], out.inputs["Surface"])
        return m


def _emit_material(name: str, rgb) -> bpy.types.Material:
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m


# ── scenarios ──────────────────────────────────────────────────────────────────────────────────────────
RIGHT = np.array([-0.70711, 0.70711, 0.0])       # screen-right on the ground (UE)
UP = np.array([0.70711, 0.70711, 0.0])           # screen-up on the ground (UE)
HERO_H = {"chest": 96.8, "head": 155.0, "overhead": 201.0, "hand": 105.0}
GOB_H = {"chest": 50.6, "head": 77.8, "overhead": 117.0, "hand": 35.0}


STATUS_TINT = {"status.freeze": ("#7FD0FF", 0.45), "status.stun": ("#7FD0FF", 0.45), "status.burn": ("#FF7A3A", 0.35),
               "status.poison": ("#8FE04A", 0.35), "status.bleed": ("#C8323C", 0.22), "status.slow": ("#9FD8FF", 0.18)}
DODGE_TILES = {"warrior": 1.8, "mage": 2.25, "rogue": 2.6}      # Data/combat_input.json dodge.distanceTiles


@dataclass
class Scene_:
    rid: str
    events: list                         # (t_ms, kind, data)
    frames: list
    clip: str = "Cast01"                 # the hero's clip; the event fires at its beat (Contact / Release)
    cls: str = "warrior"                 # the hero class casting it
    targets: int = 1
    target_dist: float = 170.0
    crop: tuple = (600, 400)
    focus: str = "mid"                   # mid | hero | target | prop | gobs
    radius: float = 100.0
    element: str | None = None
    color: str | None = None
    big: bool = False
    sparks: int = 6
    ring_px: float = 13.0
    hero: bool = True
    target_clip: str = "Idle"
    prop: str | None = None              # world prop under the effect (WORLD_PROPS)
    prop_clip: str | None = None         # the prop's clip (chest Open), timed from prop_beat
    prop_beat: float = 0.0
    bag: str | None = None               # "drop" (bag on the point) | "pickup" (bag flies to the hero)
    gob_pos: list | None = None          # goblin positions (UE cm, relative to the target spot)
    tint: tuple | None = None            # StatusTint on goblin 0 (hex, alpha)
    smith: bool = False                  # the blacksmith at the anvil (Work_Hammer, timed from its notify)
    dodge: bool = False                  # the hero dodges (Dodge clip, path, afterimages)
    tight: bool = True                   # crop to what the effect touched (+ its actors); False = the whole crop
    note: str = ""


def skill_radius(sid: str) -> float:
    try:
        c = json.loads((paths.UNREAL_ROOT / "Data" / "classes.json").read_text())
        for cls in c["classes"]:
            for s in cls["skills"]:
                if s["id"] == sid:
                    return float(s.get("aoeRadius") or 1.0) * 100.0
    except (OSError, ValueError, KeyError):
        pass
    return 100.0


def skill_element(sid: str) -> str | None:
    try:
        c = json.loads((paths.UNREAL_ROOT / "Data" / "classes.json").read_text())
        for cls in c["classes"]:
            for s in cls["skills"]:
                if s["id"] == sid:
                    return s.get("damageType")
    except (OSError, ValueError, KeyError):
        pass
    return None


def scenarios() -> dict:
    sc = {}
    A, C = "Attack01", "Cast01"
    # warrior
    for sid, clip, fr, extra in (
        ("slash", A, [0, 40, 80, 130, 180, 240], {}),
        ("whirlwind", "Cast_Whirlwind", [0, 50, 100, 160, 230, 320, 420, 520], {"crop": (720, 480), "focus": "hero"}),
        ("shield_wall", C, [0, 80, 140, 220, 340, 480, 640, 860], {"focus": "hero"}),
        ("taunt_roar", C, [0, 80, 160, 260, 360, 480], {"focus": "hero", "crop": (760, 500)}),
        ("charge", "Cast_Charge", [0, 40, 80, 120, 200, 300], {"target_dist": 300}),
        ("iron_fortress", C, [0, 100, 160, 260, 400, 560, 760, 900], {"focus": "hero"}),
        ("frenzy", C, [0, 80, 160, 260, 380, 520], {"focus": "hero"}),
        ("bleed_strike", A, [0, 40, 80, 160, 300, 600, 1000, 1400], {}),
        ("unyielding", C, [0, 80, 160, 300, 450, 620], {"focus": "hero"}),
        ("life_regen", C, [0, 100, 200, 350, 500, 700], {"focus": "hero"}),
        ("war_stomp", C, [0, 60, 140, 260, 420, 650], {"focus": "hero", "crop": (720, 480)}),
        ("vengeful_wrath", C, [0, 100, 200, 320, 460, 640], {"focus": "hero"}),
        ("lethal_strike", A, [0, 40, 90, 150, 230, 340], {}),
        ("dual_wield_mastery", C, [0, 60, 120, 200, 300], {"focus": "hero"}),
        ("rampage", "Cast_Whirlwind", [0, 60, 130, 220, 330, 480, 650, 850], {"focus": "hero", "crop": (760, 500)}),
    ):
        sc[f"skill.{sid}"] = Scene_(f"skill.{sid}", [(0, "skill", sid)], fr, clip=clip, cls="warrior",
                                    radius=skill_radius(sid), element=skill_element(sid), **extra)
    # mage (cast by the mage, in its own cast clips)
    for sid, clip, fr, extra in (
        ("blizzard", "Cast_Blizzard", [0, 100, 190, 280, 380, 520, 760, 1100],
         {"crop": (900, 560), "focus": "target", "target_dist": 260}),
        ("ice_armor", C, [0, 60, 140, 250, 400, 480, 600, 800], {"focus": "hero"}),
        ("chain_lightning", C, [0, 40, 80, 130, 190, 260, 340], {"targets": 3, "crop": (820, 480)}),
        ("mana_shield", C, [0, 120, 180, 260, 400, 560, 760], {"focus": "hero"}),
        ("fire_wall", C, [0, 80, 180, 300, 450, 650, 900], {"focus": "target", "target_dist": 260,
                                                            "crop": (760, 500)}),
        ("teleport", C, [0, 60, 120, 180, 260, 400, 600], {"target_dist": 300}),
        ("meteor", "Cast_Meteor", [0, 100, 200, 280, 320, 380, 500, 700, 1000],
         {"focus": "target", "target_dist": 260, "crop": (820, 620)}),
        ("combustion", C, [0, 40, 70, 120, 200, 320, 500], {}),
        ("freeze", C, [0, 80, 170, 260, 500, 760, 820, 950], {}),
        ("arcane_torrent", C, [0, 100, 200, 300, 400, 550, 750], {"focus": "target", "target_dist": 260,
                                                                  "crop": (820, 560)}),
    ):
        sc[f"skill.{sid}"] = Scene_(f"skill.{sid}", [(0, "skill", sid)], fr, clip=clip, cls="mage",
                                    radius=skill_radius(sid), element=skill_element(sid), **extra)
    # rogue
    for sid, clip, fr, extra in (
        ("backstab", A, [0, 40, 80, 130, 200, 300], {}),
        ("poison_blade", C, [0, 110, 200, 300, 450, 650, 900], {"focus": "hero"}),
        ("multishot", "Cast_Multishot", [0, 40, 80, 120, 170, 220], {"crop": (760, 520), "target_dist": 260}),
        ("death_mark", C, [0, 100, 200, 350, 550, 800], {}),
        ("piercing_arrow", C, [0, 60, 120, 180, 240, 320], {"crop": (900, 520), "target_dist": 300}),
        ("poison_cloud", C, [0, 120, 260, 450, 700, 1000, 1300], {"focus": "target", "target_dist": 260,
                                                                  "crop": (760, 500)}),
        ("vanish", C, [0, 60, 150, 280, 450, 700], {"focus": "hero"}),
        ("shadow_step", C, [0, 40, 80, 120, 200, 320], {"target_dist": 300}),
    ):
        sc[f"skill.{sid}"] = Scene_(f"skill.{sid}", [(0, "skill", sid)], fr, clip=clip, cls="rogue",
                                    radius=skill_radius(sid), element=skill_element(sid), **extra)
    # skills played as projectiles / ground effects (launched at the owning class's hand on its release beat)
    for sid, travel, cls in (("fireball", 360, "mage"), ("ice_arrow", 300, "mage"), ("poison_arrow", 300, "rogue")):
        sc[f"proj.{sid}"] = Scene_(f"proj.{sid}", [(0, "skill", sid), (0, "proj", (sid, travel))],
                                   [40, 120, 200, 280, travel + 20, travel + 80, travel + 180, travel + 400],
                                   clip="Cast01", cls=cls, target_dist=330, crop=(820, 460),
                                   element=skill_element(sid))
    sc["proj.arrow"] = Scene_("proj.arrow", [(0, "proj", ("arrow", 220))], [30, 90, 160, 240, 280, 340],
                              clip="Cast_Multishot", cls="rogue", target_dist=330, crop=(820, 460),
                              note="per-target multishot arrow (alias proj.multishot)")
    sc["proj.piercing_arrow"] = Scene_("proj.piercing_arrow", [(0, "proj", ("piercing_arrow", 260))],
                                       [30, 100, 180, 280, 320, 400], clip="Cast01", cls="rogue",
                                       target_dist=330, crop=(820, 460))
    sc["proj.monster_bolt"] = Scene_("proj.monster_bolt", [(0, "mproj", ("monster_bolt", 320, "#B478FF"))],
                                     [60, 160, 260, 340, 400, 520], clip="Idle", target_dist=330,
                                     crop=(820, 460), color="#B478FF")
    sc["proj.pet_bolt"] = Scene_("proj.pet_bolt", [(0, "pproj", ("pet_bolt", 320, "#9CE0FF"))],
                                 [60, 160, 260, 340, 400, 520], clip="Idle", target_dist=330, crop=(820, 460),
                                 color="#9CE0FF", note="a pet behind the hero shoots the goblin")
    sc["ground.fire_wall"] = Scene_("ground.fire_wall", [(0, "skill", "fire_wall"),
                                                         (0, "ground", ("fire_wall", 3000, None))],
                                    [100, 400, 800, 1400, 2200, 2900], cls="mage", focus="target", target_dist=260,
                                    crop=(760, 500), radius=200, element="fire")
    for sid, trig in (("explosive_trap", 900), ("slow_trap", 900), ("chain_trap", 900)):
        sc[f"ground.{sid}"] = Scene_(f"ground.{sid}", [(0, "skill", sid), (0, "ground", (sid, trig, trig))],
                                     [100, 500, trig, trig + 60, trig + 140, trig + 260, trig + 450, trig + 800],
                                     cls="rogue", focus="hero", crop=(760, 520), radius=skill_radius(sid),
                                     element=skill_element(sid))
    sc["ground.arrow_rain"] = Scene_("ground.arrow_rain", [(0, "skill", "arrow_rain"),
                                                           (0, "ground", ("arrow_rain", 3000, None))],
                                     [100, 300, 600, 1000, 1600, 2400], cls="rogue", focus="target",
                                     target_dist=260, crop=(900, 600), radius=350)
    # fallbacks (unknown skill / projectile / ground effect ids)
    sc["skill.default"] = Scene_("skill.default", [(0, "skill", "default")], [0, 60, 140, 240, 360],
                                 crop=(560, 400), color="#F39C12")
    sc["proj.default"] = Scene_("proj.default", [(0, "proj", ("default", 300))], [60, 160, 260, 320, 380, 480],
                                clip="Cast01", cls="mage", target_dist=330, crop=(820, 460), color="#F39C12")
    sc["proj.default.fizzle"] = Scene_("proj.default.fizzle", [(0, "proj", ("default", 300, "fizzle"))],
                                       [200, 320, 380, 480], clip="Cast01", cls="mage", target_dist=330,
                                       crop=(820, 460), color="#F39C12", targets=0)
    sc["ground.default"] = Scene_("ground.default", [(0, "ground", ("default", 900, 1500))],
                                  [200, 700, 900, 980, 1100, 1500, 1700], focus="target", target_dist=260,
                                  crop=(640, 460), radius=150, element="fire")
    # combat feedback
    for prof, sp, rp in (("light", 4, 10), ("normal", 6, 13), ("heavy", 9, 17), ("crit", 12, 22), ("kill", 14, 24)):
        big = prof in ("crit", "kill")
        sc[f"hit.impact.{prof}"] = Scene_(f"hit.impact.{prof}", [(0, "hit", "hit.impact")],
                                          [0, 30, 60, 100, 160, 240], clip="Attack01", big=big,
                                          sparks=sp, ring_px=rp, color="#FFD98A", crop=(520, 360))
    sc["hit.basic"] = Scene_("hit.basic", [(0, "hit", "hit.basic"), (0, "hit", "hit.impact")],
                             [0, 30, 60, 100, 150, 220], clip="Attack01", color="#FFD98A", crop=(520, 360))
    for cls in ("warrior", "mage", "rogue"):
        sc[f"hit.basic.{cls}"] = Scene_(f"hit.basic.{cls}", [(0, "hit", f"hit.basic.{cls}"), (0, "hit", "hit.impact")],
                                        [0, 40, 80, 120, 170], clip="Attack01", cls=cls,
                                        color={"warrior": "#FFD98A", "mage": "#C7A6FF", "rogue": "#9DFFC8"}[cls],
                                        crop=(520, 360))
    sc["hit.claw"] = Scene_("hit.claw", [(0, "hitme", "hit.claw")], [0, 30, 70, 120, 180, 230], clip="Idle",
                            crop=(520, 360))
    sc["hit.evade"] = Scene_("hit.evade", [(0, "hitme", "hit.evade")], [0, 40, 90, 150, 220], clip="Idle",
                             crop=(520, 360))
    for m in ("fire", "ice", "other"):
        sc[f"hit.mark.{m}"] = Scene_(f"hit.mark.{m}", [(0, "hit", f"hit.mark.{m}")], [0, 100, 300, 700, 1200, 1700],
                                     crop=(520, 360))
    sc["death.burst"] = Scene_("death.burst", [(0, "hit", "death.burst"), (0, "hit", "reward.gold")],
                               [0, 60, 140, 240, 380, 560, 800], clip="Attack01", color="#FF4444",
                               crop=(560, 400), target_clip="Hurt")
    sc["reward.heal"] = Scene_("reward.heal", [(0, "self", "reward.heal")], [0, 80, 180, 320, 480, 680],
                               clip="Idle", focus="hero", crop=(520, 400))
    sc["reward.mana"] = Scene_("reward.mana", [(0, "self", "reward.mana")], [0, 80, 180, 320, 480, 680],
                               clip="Idle", cls="mage", focus="hero", crop=(520, 400))
    sc["hero.levelup"] = Scene_("hero.levelup", [(0, "self", "hero.levelup")], [0, 80, 160, 280, 420, 600, 850, 1100],
                                clip="Idle", focus="hero", crop=(620, 480))
    # loot: the drop burst + the standing beam loop on the bag (as the world builder plays them), then the beam
    # alone later on, and the pickup (bag flight to the hero)
    for q, col in (("magic", "#4F8CFF"), ("rare", "#FFD84A"), ("legendary", "#FF8A2A"), ("set", "#3ECF6A")):
        drop = "loot.drop" if q == "magic" else f"loot.drop.{q}"
        sc[drop] = Scene_(drop, [(0, "loot", drop), (0, "lootloop", f"loot.beam.{q}")],
                          [0, 60, 140, 260, 500, 900, 1300], clip="Idle", color=col, crop=(560, 560),
                          focus="beam", targets=0, bag="drop")
        sc[f"loot.beam.{q}"] = Scene_(f"loot.beam.{q}", [(0, "lootloop", f"loot.beam.{q}")],
                                      [2100, 2600, 3050, 3500], clip="Idle", color=col, crop=(560, 560),
                                      focus="beam", targets=0, bag="drop",
                                      note="standing beam (pulse ×.55 yoyo); frames span one pulse period")
    sc["loot.pickup"] = Scene_("loot.pickup", [(0, "loot", "loot.pickup"), (0, "pickup", "#FFD84A")],
                               [0, 60, 120, 200, 300, 420], clip="Idle", color="#FFD84A", crop=(620, 460),
                               targets=0, target_dist=150, bag="pickup")
    sc["reward.gold"] = Scene_("reward.gold", [(0, "hit", "reward.gold")], [0, 80, 160, 260, 380, 500],
                               crop=(520, 360), targets=1)
    sc["quest.pop"] = Scene_("quest.pop", [(0, "loot", "quest.pop")], [0, 60, 130, 200, 260], clip="Idle",
                             targets=0, focus="target", crop=(420, 320))
    for st in ("burn", "poison", "freeze", "stun", "slow", "bleed"):
        sc[f"status.{st}"] = Scene_(f"status.{st}", [(0, "status", f"status.{st}")], [300, 700, 1100, 1500],
                                    clip="Idle", focus="target", crop=(420, 340), tint=STATUS_TINT[f"status.{st}"])
    sc["hero.dodge"] = Scene_("hero.dodge", [(0, "dodge", "hero.dodge")], [0, 80, 160, 240, 340, 460],
                              clip="Dodge", cls="rogue", focus="hero", crop=(720, 440), dodge=True, targets=0)
    sc["hero.dash"] = Scene_("hero.dash", [(0, "skillctx", "hero.dash")], [0, 60, 120, 200, 320, 480],
                             clip="Cast_Charge", target_dist=300, crop=(760, 420))
    sc["hero.resonance"] = Scene_("hero.resonance", [(0, "self", "hero.resonance"),
                                                     (0, "selfloop", "hero.resonance.aura")],
                                  [0, 100, 220, 400, 800, 1300], clip="Idle", focus="hero",
                                  color="#FFB55C", crop=(560, 420))
    sc["teleport.blink"] = Scene_("teleport.blink", [(0, "skillctx", "teleport.blink")], [0, 60, 120, 200, 320],
                                  clip="Cast01", target_dist=300, crop=(760, 420), targets=0)
    sc["portal.channel"] = Scene_("portal.channel", [(0, "selfloop", "portal.channel"), (1500, "self", "portal.arrive")],
                                  [200, 600, 1000, 1400, 1560, 1750], clip="Idle", focus="hero", crop=(560, 440))
    for w, cl in (("camp.campfire", 2000), ("camp.torch", 2000), ("camp.hearthstone", 2000), ("exit.portal", 2500),
                  ("exit.sealed", 2500), ("prop.lore", 2000), ("prop.gather", 2000), ("prop.clue", 2000),
                  ("prop.soul_echo", 2000), ("prop.chest", 2000), ("prop.event", 2000)):
        big = w.startswith("exit.")
        sc[w] = Scene_(w, [(0, "world", w)], [cl - 900, cl - 600, cl - 300, cl], clip="Idle",
                       targets=0, focus="prop", crop=(680, 780) if big else (480, 420),
                       color="#FFD86A" if w == "prop.lore" else None, prop=w)
    # chest open: played as the Open clip starts (its layers wait for the ChestOpen notify, 120 ms); the lid keeps
    # lifting until 400 ms
    sc["prop.chest.open"] = Scene_("prop.chest.open", [(0, "world", "prop.chest.open")],
                                   [60, 136, 180, 220, 300, 400, 570, 820], clip="Idle", targets=0, focus="prop",
                                   crop=(480, 420), prop="prop.chest.open", prop_clip="Open", prop_beat=0.0)
    # elites: one goblin on the dirt path, one on the grass — every affix hue on both grounds
    for e in ("fire_enhanced", "swift", "teleporting", "extra_strong", "curse_aura", "vampiric", "frozen"):
        sc[f"elite.{e}"] = Scene_(f"elite.{e}", [(0, "status", f"elite.{e}"), (0, "status1", f"elite.{e}")],
                                  [400, 800, 1200, 1600], clip="Idle", focus="gobs", crop=(560, 470), targets=2,
                                  gob_pos=[(0.0, 0.0), (-150.0, 60.0)])
    # the blacksmith's anvil sparks at 427 ms of Work_Hammer (FX_HammerStrike), on the stump's strike point
    sc["npc.FX_HammerStrike"] = Scene_("npc.FX_HammerStrike", [(0, "notify", "npc.FX_HammerStrike")],
                                       [0, 33, 66, 110, 160, 230], clip="Idle", targets=0, hero=False,
                                       focus="smith", crop=(420, 380), smith=True)
    # ambient camera volumes (emerald_plains: pollen + dust; wisps / dust_motes / sparks for later moods)
    for amb, fr in (("pollen", [14000, 16000]), ("dust", [12000, 14000]), ("wisps", [9000, 11000]),
                    ("dust_motes", [14000, 16000]), ("sparks", [6000, 8000])):
        sc[f"ambient.{amb}"] = Scene_(f"ambient.{amb}", [(0, "ambient", f"ambient.{amb}")], fr, clip="Idle",
                                      targets=1, focus="mid", crop=(960, 540), tight=False,
                                      note="camera-attached volume, steady state")
    return sc


GROUPS = {
    "warrior": lambda k: k.startswith("skill.") and k[6:] in {
        "slash", "whirlwind", "shield_wall", "taunt_roar", "charge", "iron_fortress", "frenzy", "bleed_strike",
        "unyielding", "life_regen", "war_stomp", "vengeful_wrath", "lethal_strike", "dual_wield_mastery", "rampage"},
    "mage": lambda k: (k.startswith("skill.") and k[6:] in {
        "blizzard", "ice_armor", "chain_lightning", "mana_shield", "fire_wall", "teleport", "meteor", "combustion",
        "freeze", "arcane_torrent"}) or k in ("proj.fireball", "proj.ice_arrow", "ground.fire_wall",
                                              "hit.basic.mage", "reward.mana"),
    "rogue": lambda k: (k.startswith("skill.") and k[6:] in {
        "backstab", "poison_blade", "multishot", "death_mark", "piercing_arrow", "poison_cloud", "vanish",
        "shadow_step"}) or k in ("proj.poison_arrow", "proj.arrow", "proj.piercing_arrow", "ground.explosive_trap",
                                 "ground.slow_trap", "ground.chain_trap", "ground.arrow_rain", "hit.basic.rogue"),
    "combat": lambda k: (k.startswith("hit.") and k not in ("hit.basic.mage", "hit.basic.rogue")) or
    k in ("death.burst", "reward.gold", "proj.monster_bolt", "proj.pet_bolt"),
    "utility": lambda k: k.startswith(("reward.", "hero.", "loot.", "quest.", "teleport.", "portal.")) and
    k != "reward.mana",
    "status": lambda k: k.startswith(("status.", "elite.")),
    "world": lambda k: k.startswith(("camp.", "exit.", "prop.", "npc.")),
    "ambient": lambda k: k.startswith("ambient."),
    "fallback": lambda k: k in ("skill.default", "proj.default", "proj.default.fizzle", "ground.default"),
}


# ── output: native 1:1 frames, one or two per file, never resampled ──────────────────────────────────────
def write_effect(rid: str, cells: list, out_dir: Path) -> list:
    """``cells`` = [(ms, uint8 HxWx3)] → ``out_dir/<rid>/<ms>[-<ms>]ms.png``: two frames side by side when the pair
    fits the budget truecolour, else one frame per file, else the frame in stacked native strips
    (``<ms>ms.png``, ``<ms>ms_2.png`` …). Raises ``pngio.NativePreviewError`` when a frame cannot be stored at
    scale 1 under the budget (no resampling)."""
    d = out_dir / rid
    d.mkdir(parents=True, exist_ok=True)
    for old in d.glob("*.png"):
        old.unlink()
    flat = out_dir / f"{rid}.png"              # the earlier downscaled contact sheet
    if flat.exists():
        flat.unlink()
    written = []
    i = 0
    while i < len(cells):
        if i + 1 < len(cells):
            (ma, ca), (mb, cb) = cells[i], cells[i + 1]
            pair = review.grid([ca, cb], 2, pad=2)
            path = d / f"{int(ma):05d}-{int(mb):05d}ms.png"
            size = pngio.write_png(path, pair)
            if size <= pngio.PREVIEW_MAX_BYTES:
                written.append((path, size))
                i += 2
                continue
            path.unlink()
        ms, c = cells[i]
        written += pngio.write_png_native(d / f"{int(ms):05d}ms.png", c, axis=0, max_tiles=4)
        i += 1
    for path, size in written:
        inf = pngio.png_info(path)
        if not inf["truecolor"] or size > pngio.PREVIEW_MAX_BYTES:
            raise pngio.NativePreviewError(f"{path}: not a truecolour native frame under the budget")
    return written


def label(img: np.ndarray, text: str) -> None:
    review.draw_text(img, 4, 4, text, rgb=(255, 244, 220))


# ── run one scenario ───────────────────────────────────────────────────────────────────────────────────
def run(stage: Stage, recipes, s: Scene_, out_dir: Path) -> list:
    H = np.array([0.0, 0.0, 0.0])
    T = H + RIGHT * s.target_dist + UP * 12.0
    blow3 = T - H
    blow = blow3[:2] / np.linalg.norm(blow3[:2])
    ch = stage.heroes.get(s.cls) or stage.hero
    for c_, h_ in stage.heroes.items():
        if h_ is not ch:
            stage.pose(h_, "Idle", H + np.array([0, 0, -5000.0]), blow3, visible=False)
    hero = Actor("hero", H, HERO_H, hand_off=np.array([blow[0], blow[1], 0]) * 32)
    if s.gob_pos is not None:
        gob_pos = [T + UP * u + RIGHT * r for u, r in s.gob_pos] + [T + UP * 150 + RIGHT * 60] * 3
    else:
        gob_pos = [T, T + UP * 150 + RIGHT * 60, T - UP * 140 + RIGHT * 110]
    gobs = [Actor(f"gob{i}", gob_pos[i], GOB_H) for i in range(3)]
    hero_clip = s.clip if s.clip in ch["anims"] else ("Cast01" if s.clip.startswith("Cast") else "Idle")
    dodge_dir = np.array([-UP[0], -UP[1], 0.0]) * 0.6 + np.array([-RIGHT[0], -RIGHT[1], 0.0]) * 0.8
    dodge_dir /= np.linalg.norm(dodge_dir)
    dodge_len = DODGE_TILES.get(s.cls, 1.8) * 100.0
    stage.pose(ch, hero_clip, H, blow3 if not s.dodge else dodge_dir, visible=s.hero)
    beat = beat_ms(ch["sk"], hero_clip)
    for i, g in enumerate(stage.gobs):
        vis = i < max(s.targets, 0)
        stage.pose(g, s.target_clip if i == 0 else "Idle", gob_pos[i], H - gob_pos[i], visible=vis)
    target_actor = gobs[0] if s.targets >= 1 else None
    prop_at = H + RIGHT * (260 if s.rid.startswith("exit.") else 120)
    prop = stage.show_prop(s.prop, prop_at, clip=s.prop_clip)
    bag = None
    if s.bag:
        bag = stage.show_prop("loot.bag", T)
    smith = None
    if s.smith:
        smith = stage.load_smith()
        stage.pose(smith, "Work_Hammer", H + RIGHT * 60, -UP + RIGHT * 0.25, visible=True)
    elif stage.smith is not None:
        for m in stage.smith["meshes"]:
            m.hide_render = True

    # the timed actor (its clip time drives the scene frame) and the event's clip time at t = 0
    if smith is not None:
        timed, t0_ms = smith, beat_ms("SK_NPC_Blacksmith", "Work_Hammer") or 426.6
    elif prop is not None and s.prop_clip:
        timed, t0_ms = prop, s.prop_beat
    else:
        timed, t0_ms = ch, (beat if hero_clip not in ("Idle", "Dodge") else 0.0)
    clip_len = float(clip_info(timed["sk"], timed["clip"]).get("lengthMs", 1000.0)) if timed.get("clip") else 0.0
    static_idle = timed is ch and hero_clip == "Idle"      # an idle hero holds its first frame (one background)
    live_until = -1.0 if static_idle or not timed.get("clip") else clip_len - t0_ms

    def hero_at(t_ms: float) -> np.ndarray:
        if not s.dodge:
            return H
        u = min(max(t_ms / 300.0, 0.0), 1.0)
        u = 1.0 - (1.0 - u) ** 2
        return H + dodge_dir * dodge_len * u

    def set_time(t_ms: float, sockets=True) -> None:
        if timed.get("clip"):
            stage.clip_time(timed, t0_ms + (0.0 if static_idle else t_ms))
        if s.dodge:
            ch["root"].location = ue_to_bl(hero_at(t_ms))
            hero.pos = hero_at(t_ms)
        if sockets and timed is ch:
            hero.sockets = stage.sockets_ue(ch)

    # camera + crop
    focus = {"hero": H, "target": T, "prop": prop_at, "gobs": (gob_pos[0] + gob_pos[1]) * 0.5,
             "smith": H + RIGHT * 60 - UP * 25, "beam": T + UP * 110}.get(s.focus, (H + T) * 0.5)
    review.setup_render(RES, 8)
    fb = ue_to_bl(focus)
    cam_obj = review.game_camera((fb.x, fb.y))
    c = review.project(cam_obj, Vector((fb.x, fb.y, 0.6)))
    cw, chh = s.crop
    x0 = int(round(c[0] * RES[0] - cw / 2))
    y0 = int(round((1 - c[1]) * RES[1] - chh / 2))
    x0, y0 = max(0, min(RES[0] - cw, x0)), max(0, min(RES[1] - chh, y0))
    box = (x0 / RES[0], 1 - (y0 + chh) / RES[1], (x0 + cw) / RES[0], 1 - y0 / RES[1])
    cam = Camera(cam_obj, RES, (x0, y0, cw, chh))
    R_ = np.array(cam_obj.matrix_world)[:3, :3]
    cam_vecs = (bl_to_ue_dir(R_[:, 0]), bl_to_ue_dir(R_[:, 1]), bl_to_ue_dir(-R_[:, 2]))
    sim = Sim(recipes, cam_vecs)
    color = hex_lin(s.color) if s.color else None
    ring_cm = s.ring_px * 100 / 45
    base_ctx = dict(blow=blow, radius=s.radius, color=color, element=s.element, big=s.big, sparks=s.sparks,
                    ring=ring_cm)
    # events whose runtime context sets no blow (level-up, pickups, statuses, loot, world, portal): the blow frame
    # falls back to UE +X (UAbyssVfxSystem::SpawnParticle)
    nob_ctx = dict(base_ctx, blow=np.array([1.0, 0.0]))
    set_time(0.0)
    if timed is not ch:
        hero.sockets = stage.sockets_ue(ch)
    if ch.get("crystal") is not None and any(k == "proj" for _, k, _ in s.events):
        cw_ = ue_cm(ch["crystal"].matrix_world.translation)
        hr = hero.sockets.get("hand_r", H)
        print(f"  {s.cls} release: fx_hand_r {np.round(hr, 1)}  launch (staff crystal) {np.round(cw_, 1)}  "
              f"({np.linalg.norm(cw_ - hr):.0f} cm apart)")

    def ctx_skill():
        pts = [g.pos for g in gobs[:max(1, s.targets)]] if s.targets >= 1 else [T]
        return Ctx(origin=H, target=T, point=T, origin_actor=hero, target_actor=target_actor, points=pts,
                   stagger=55.0 if s.targets > 1 else 0.0, **base_ctx)
    timed_ev: list = []          # (t, fn)
    flight = {}
    for t0, kind, data in sorted(s.events, key=lambda e: e[0]):
        if kind in ("skill", "skillctx"):
            rid = f"skill.{data}" if kind == "skill" else data
            timed_ev.append((t0, lambda rid=rid: sim.play(rid, ctx_skill())))
        elif kind == "hit":
            timed_ev.append((t0, lambda rid=data: sim.play(rid, Ctx(origin=H, target=T, point=T, origin_actor=hero,
                                                                    target_actor=target_actor, **base_ctx))))
        elif kind == "hitme":
            back = dict(base_ctx, blow=-blow)
            timed_ev.append((t0, lambda rid=data: sim.play(rid, Ctx(origin=T, target=H, point=H, origin_actor=gobs[0],
                                                                    target_actor=hero, **back))))
        elif kind in ("self", "selfloop"):
            timed_ev.append((t0, lambda rid=data: sim.play(rid, Ctx(origin=H, target=H, point=H, origin_actor=hero,
                                                                    target_actor=hero, **nob_ctx))))
        elif kind == "dodge":
            dctx = dict(base_ctx, blow=dodge_dir[:2])
            timed_ev.append((t0, lambda rid=data: sim.play(rid, Ctx(origin=H, target=H, point=H, origin_actor=hero,
                                                                    target_actor=hero, **dctx))))
        elif kind in ("status", "status1"):
            g = gobs[0] if kind == "status" else gobs[1]
            timed_ev.append((t0, lambda rid=data, g=g: sim.play(rid, Ctx(origin=g.pos, target=g.pos, point=g.pos,
                                                                         origin_actor=g, target_actor=g,
                                                                         **nob_ctx))))
        elif kind in ("loot", "lootloop"):
            timed_ev.append((t0, lambda rid=data: sim.play(rid, Ctx(origin=T, target=T, point=T, **nob_ctx))))
        elif kind == "pickup":
            col = hex_lin(data)
            timed_ev.append((t0, lambda col=col: sim.play("reward.gold", Ctx(origin=H, target=H, point=H,
                                                                             target_actor=hero,
                                                                             **dict(nob_ctx, color=col)))))
        elif kind == "world":
            timed_ev.append((t0, lambda rid=data: sim.play(rid, Ctx(origin=prop_at, target=prop_at, point=prop_at,
                                                                    **nob_ctx))))
        elif kind == "notify":
            sp = ue_cm(smith["root"].matrix_world.translation)
            fwd = smith["root"].matrix_world.to_3x3() @ Vector((0.0, -1.0, 0.0))
            nb = np.array([fwd.x, -fwd.y])
            nb /= np.linalg.norm(nb)
            sa = Actor("smith", sp, {"chest": 94.6, "head": 150.8, "overhead": 197.0, "hand": 77.7})
            timed_ev.append((t0, lambda rid=data, sp=sp, nb=nb, sa=sa: sim.play(
                rid, Ctx(origin=sp, target=sp, point=sp, origin_actor=sa, target_actor=sa,
                         **dict(base_ctx, blow=nb)))))
        elif kind == "ambient":
            timed_ev.append((t0, lambda rid=data: sim.play(rid, Ctx(origin=focus.copy(), target=focus.copy(),
                                                                    point=focus.copy(), **nob_ctx))))
        elif kind in ("proj", "mproj", "pproj"):
            vid, travel = data[0], data[1]
            ending = data[2] if kind == "proj" and len(data) > 2 else "hit"
            if kind == "proj":
                src, dst = hero, (gobs[0] if s.targets >= 1 else None)
            elif kind == "mproj":
                src, dst = gobs[0], hero
            else:
                pet = Actor("pet", H - UP * 70 - RIGHT * 50, {"chest": 30.0, "head": 45.0, "overhead": 70.0,
                                                               "hand": 35.0})
                src, dst = pet, gobs[0]
            dst_pt = (lambda: actor_point(dst, "chest", 0)) if dst is not None else \
                (lambda: T + np.array([0, 0, 90.0]))
            arc = recipes.get(f"proj.{vid}", {}).get("arc", 0.0)
            state = {}

            def launch(vid=vid, src=src, dst=dst, dst_pt=dst_pt, travel=travel, arc=arc, state=state):
                a = actor_point(src, "hand", 0)          # the launch socket at the release beat
                b = dst_pt()

                def moving(t_ms, a=a, b=b, t_start=sim.t, travel=travel, arc=arc):
                    u = min(max((t_ms - t_start) / travel, 0.0), 1.0)
                    p = a + (b - a) * u
                    p[2] += arc * 4 * u * (1 - u)
                    return p
                pblow = (b - a)[:2] / np.linalg.norm((b - a)[:2])
                ctxp = Ctx(origin=a, target=b, point=b, origin_actor=src, target_actor=dst,
                           **dict(base_ctx, blow=pblow))
                state["ctx"] = ctxp
                state["b"] = b
                sim.play(f"proj.{vid}.launch", ctxp)
                hc = Ctx(**{**ctxp.__dict__, "origin_actor": None})
                state["h"] = sim.play(f"proj.{vid}", hc,
                                      anchor_override=Anchor(None, "ground", 0, a, moving=moving))

            def end(vid=vid, state=state, dst=dst, ending=ending):
                sim.stop(state.get("h", 0))
                ctxp = state["ctx"]
                end_pt = dst.pos.copy() if dst is not None else state["b"] - np.array([0, 0, 90.0])
                ec = Ctx(**{**ctxp.__dict__, "point": end_pt, "target": end_pt})
                rid = f"proj.{vid}.{ending}"
                sim.play(rid if rid in recipes else f"proj.default.{ending}", ec)
            timed_ev.append((t0, launch))
            timed_ev.append((t0 + travel, end))
        elif kind == "ground":
            gid, trig, end_t = data
            gc = Ctx(origin=H if gid in ("explosive_trap", "slow_trap", "chain_trap") else T,
                     target=T, point=T, **base_ctx)
            state = {}

            def gstart(gid=gid, gc=gc, state=state):
                state["h"] = sim.play(f"ground.{gid}", gc)
            timed_ev.append((t0, gstart))
            if trig is not None and trig != end_t:
                timed_ev.append((trig, lambda gid=gid, gc=gc: sim.play(f"ground.{gid}.trigger", gc)))
            if end_t is not None:
                def gend(gid=gid, gc=gc, state=state, trig=trig, end_t=end_t):
                    sim.stop(state.get("h", 0))
                    if f"ground.{gid}.trigger" in recipes and end_t == trig:
                        sim.play(f"ground.{gid}.trigger", gc)
                    if f"ground.{gid}.end" in recipes:
                        sim.play(f"ground.{gid}.end", gc)
                timed_ev.append((end_t, gend))
    timed_ev.sort(key=lambda e: e[0])

    # bag flight (WorldBuilder Leaving: 300 ms, EaseInCubic toward the hero's chest − half the bag, 18 px arc)
    def bag_at(t_ms: float) -> np.ndarray:
        if s.bag != "pickup":
            return T
        u = min(max(t_ms / 300.0, 0.0), 1.0)
        e = u ** 3
        chest = actor_point(hero, "chest", 0) - np.array([0, 0, 30.0])
        p = T + (chest - T) * e
        p[2] += 18.0 * PX_CM_WEB * math.sin(math.pi * u)
        return p

    cells = []
    dt = 1000.0 / 60.0
    t = 0.0
    fi = 0
    frames = sorted(max(f, 16.7) for f in s.frames)     # the beat's first tick: particles exist after one update
    bg_key, bg_col, bg_dep = None, None, None
    tag = {"warrior": "WARRIOR 1.76M", "mage": "MAGE", "rogue": "ROGUE"}.get(s.cls, "")
    ghosts_spec = []
    if s.dodge:
        for k, (frac, a0) in enumerate(((1 / 3, 0.28), (2 / 3, 0.21))):
            tc = 300.0 * (1 - math.sqrt(1 - frac))          # when the eased path passes ⅓ / ⅔
            ghosts_spec.append((hero_at(tc), tc, a0, 1.0 + 0.04 * k))
    while fi < len(frames):
        while timed_ev and timed_ev[0][0] <= t + 1e-6:
            timed_ev.pop(0)[1]()
        if t + 1e-6 >= frames[fi]:
            set_time(t)
            if bag is not None:
                bag["root"].location = ue_to_bl(bag_at(t))
                for o in bag["objs"][1:]:
                    o.hide_render = s.bag == "pickup" and t >= 300.0
            sprites, meshes = sim.instances()
            key = (round(timed.get("ms", 0.0), 1), tuple(hero.pos), round(t) if s.bag == "pickup" else 0)
            if meshes:
                stage.place_meshes(meshes)
                col, dep = stage.render(box)
                stage.place_meshes([])
                if key != bg_key:                 # the plate without FX meshes (the touched-pixel crop)
                    bg_key = key
                    bg_col, bg_dep = stage.render(box)
            elif key == bg_key:
                col, dep = bg_col.copy(), bg_dep
            else:
                col, dep = stage.render(box)
                bg_key, bg_col, bg_dep = key, col.copy(), dep
            img = col.copy()
            if s.tint is not None:
                mask = stage.render_mask(box, stage.gobs[0]["meshes"])
                tl = np.array(hex_lin(s.tint[0]))
                img = img * (1.0 + (tl - 1.0) * s.tint[1] * mask[..., None])
            if ghosts_spec:
                live = [(p, tc, a0, sc_) for p, tc, a0, sc_ in ghosts_spec if tc <= t <= tc + 260.0]
                if live:
                    layers = stage.render_ghosts(box, ch, [(p, tc) for p, tc, _, _ in live], ch["spirit"])
                    set_time(t)
                    for lay_, (p, tc, a0, sc_) in zip(layers, live):
                        a = a0 * (1.0 - min((t - tc) / 260.0, 1.0)) ** 1.3
                        img = img + lay_ * a
            plate = img.copy()
            composite_sprites(img, dep, cam, sprites)
            # what the effect touched (sprites, FX meshes) → the tight crop
            touched = np.abs(img - plate).max(axis=2) > 0.02
            if meshes:
                touched |= np.abs(col - bg_col).max(axis=2) > 0.02
            img = bloom_lin(img)
            out = to_srgb8(img)
            cells.append((frames[fi], out, touched))
            fi += 1
            continue
        if (0 <= t <= live_until + dt) or (s.dodge and t <= 320.0):
            set_time(t)                       # the cast clip moves the launch / anchor sockets
        sim.update(dt)
        t += dt
    stage.place_meshes([])
    # one tight crop for every frame of the effect: the union of what it touched + the actors it plays on (for
    # scale), padded, inside the scenario crop — native pixels, only less of the empty plains
    subj = [hero_at(t)] if s.hero else []
    subj += [g.pos for g in gobs[:max(s.targets, 0)]]
    if s.prop:
        subj.append(prop_at)
        if s.prop.startswith("exit."):                 # the whole 3.3 m gate, not a 1.9 m actor box
            subj.append(prop_at + np.array([0.0, 0.0, 140.0]))
    if s.bag:
        subj.append(T)
    if smith is not None:
        subj.append(ue_cm(smith["root"].matrix_world.translation))
    box_px = tight_box([c[2] for c in cells], cam, subj, (cw, chh)) if s.tight else (0, 0, cw, chh)
    x0c, y0c, x1c, y1c = box_px
    final = []
    for ms, out, _ in cells:
        o = np.ascontiguousarray(out[y0c:y1c, x0c:x1c])
        label(o, f"{s.rid}  +{int(ms)}MS  1:1 1080P  {tag if s.hero else ''}")
        final.append((ms, o))
        if CELL_DIR is not None:
            CELL_DIR.mkdir(parents=True, exist_ok=True)
            pngio.write_png(CELL_DIR / f"{s.rid}_{int(ms):05d}.png", o)
    cw, chh = x1c - x0c, y1c - y0c
    stage.show_prop(None, prop_at)
    written = write_effect(s.rid, final, out_dir)
    kb = sum(sz for _, sz in written) // 1024
    print(f"  {s.rid}: {len(cells)} frames {cw}x{chh} native -> {len(written)} files, {kb} KB "
          f"(max {max(sz for _, sz in written) // 1024} KB)")
    if smith is not None:
        hm = smith.get("hammer")
        if hm is not None:
            stage.clip_time(smith, t0_ms)
            bpy.context.view_layer.update()
            hs = next((x for x in manifest()["SM_NPCKit_Prop_Hammer"]["sockets"] if x["name"] == "head"), None)
            sp = ue_cm(smith["root"].matrix_world.translation)
            fwd = smith["root"].matrix_world.to_3x3() @ Vector((0.0, -1.0, 0.0))
            f2 = np.array([fwd.x, -fwd.y])
            f2 /= np.linalg.norm(f2)
            r2 = np.array([-f2[1], f2[0]])
            strike = next(L["offset"] for L in recipes["npc.FX_HammerStrike"]["layers"] if L["sprite"] == "Ember")
            sp_world = sp + np.array([*(f2 * strike[0] + r2 * strike[1]), strike[2]])
            if hs is not None:
                head = ue_cm(item_socket_world(hm, hs))
                print(f"  hammer head at the strike notify: {np.round(head, 1)}  burst at {np.round(sp_world, 1)}  "
                      f"gap {np.linalg.norm(head - sp_world):.1f} cm")
            st = stage.smith.get("stump")
            if st is not None:
                ss = next(x for x in manifest()["SM_Prop_Camp_AnvilStump"]["sockets"] if x["name"] == "strike")
                sw = ue_cm(item_socket_world(st, ss))
                print(f"  anvil `strike` socket {np.round(sw, 1)} vs recipe anchor {np.round(sp_world, 1)} "
                      f"(+3 cm over the bar): {np.linalg.norm(sw + np.array([0, 0, 3.0]) - sp_world):.1f} cm")
        for m in smith["meshes"]:
            m.hide_render = True
    return written


PX_CM_WEB = 100.0 / 45.0
def tight_box(touched: list, cam: Camera, subjects: list, crop: tuple, pad: int = 26, min_wh=(300, 240)):
    """(x0, y0, x1, y1) in the scenario crop: the union of the effect's touched pixels over all frames and the
    screen boxes of its subject actors (feet → 1.9 m, ±45 cm), padded, at least ``min_wh``."""
    W, H_ = crop
    ys, xs = [], []
    for m in touched:
        yy, xx = np.nonzero(m)
        if len(xx):
            xs += [int(xx.min()), int(xx.max())]
            ys += [int(yy.min()), int(yy.max())]
    for p in subjects:
        for dz in (0.0, 190.0):
            for dx in (-60.0, 60.0):
                q = np.asarray(p, float) + np.array([0.0, 0.0, dz]) + RIGHT * dx
                pr = cam.project(np.array(ue_to_bl(q)))
                if pr is not None:
                    xs.append(int(pr[0]))
                    ys.append(int(pr[1]))
    if not xs:
        return 0, 0, W, H_
    x0, x1 = max(0, min(xs) - pad), min(W, max(xs) + pad + 1)
    y0, y1 = max(0, min(ys) - pad), min(H_, max(ys) + pad + 1)
    for lo, hi, n, mn in ((0, 2, W, min_wh[0]), (1, 3, H_, min_wh[1])):
        b = [x0, y0, x1, y1]
        if b[hi] - b[lo] < mn:
            c = (b[hi] + b[lo]) // 2
            a = max(0, min(n - mn, c - mn // 2))
            b[lo], b[hi] = a, min(n, a + mn)
        x0, y0, x1, y1 = b
    return x0, y0, x1, y1


def main(argv) -> int:
    global CELL_DIR, SOFT_GROUND
    if "--soft-ground" in argv:
        SOFT_GROUND = 0.008
    out_dir = K.SCRATCH / "effects" if "--scratch" in argv else OUT_DIR
    if "--cells" in argv:
        CELL_DIR = K.SCRATCH / "cells"
    sel = [a for a in argv if not a.startswith("--")]
    recipes = load_recipes()
    man = manifest()
    for name, e in man.items():
        if name.startswith(("SM_FX_", "SM_Pickup_GoldCoin")):
            d = e.get("boundsDiameterCm")
            if d is None:
                lo, hi = e["boundsCm"]["min"], e["boundsCm"]["max"]
                d = math.dist(lo, hi)
            MESH_D[name] = float(d)
    sc = scenarios()
    keys = []
    for a in sel or list(GROUPS):
        if a in GROUPS:
            keys += [k for k in sc if GROUPS[a](k)]
        elif a in sc:
            keys.append(a)
        else:
            print(f"unknown scenario {a}")
    keys = list(dict.fromkeys(keys))
    classes = sorted({sc[k].cls for k in keys} | {"warrior"})
    stage = Stage(tuple(classes))
    fails = []
    for k in keys:
        try:
            run(stage, recipes, sc[k], out_dir)
        except pngio.NativePreviewError as ex:
            print(f"  FAIL {k}: {ex}")
            fails.append(k)
    if fails:
        print(f"native preview budget failures: {fails}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
