"""VFX hue audit (dark remake DR5: implementation-plan.md A0 / work package 0C, monsters.md §1.2).

    python3 unreal/Art/blender/vfx/hue_audit.py [--recipes=<json>] [--out=<dir>] [--report=<json>] [--no-report]
                                                [--report-only] [--quiet]

Pure Python (stdlib only). Reads what the game ships — ``Art/Export/VFX/vfx_recipes.json`` (palettes, sprites,
recipes, aliases), the ``T_FX_<sprite>`` textures (pre-coloured cells: flames, ice shards, arrows …), the monster
palettes (``palettes/Monsters_*.json``) and the monster manifest ``fx`` colours — and resolves every colour each recipe
layer can draw: its colour source (fixed, the recipe palette's core / mid / rim / dark slot, the event colour, white;
the alternate colour), times the texel colours of a pre-coloured sprite cell, through the runtime's palette rules
(``element`` = every element palette, ``event`` = derived from the event colour as ``FAbyssVfxPalette::FromColor``
does; layer presets as ``FAbyssVfxLibrary::ApplyPreset``). Colours are judged in display sRGB (HSV), like the hexes of
the colour rules.

Event colours (what ``UAbyssVfxSystem`` puts in ``Context.Color``) are read from ``unreal/Data``:

| Recipes | Event colours |
|---|---|
| ``elite.<affix>`` | ``elite_affixes.json`` ``vfxColor`` |
| ``proj.monster_bolt*`` | ``monsters.json`` ``derived.projectileColor`` of the ranged monsters (gating for Chapter-1 zones, ``CH1_ZONES``; the rest advisory) |
| ``death.burst`` | the monster kill #FF4444 (hero #CC2222 / escort #E67E22 deaths reuse it with non-enemy colours) |
| ``hit.impact*`` | ``hit_feedback.json`` class impact colours + fallback, every active skill's ``impactColor`` (``classes.json``) but death_mark / slow_trap (no impact burst) |
| ``hero.resonance*`` | ``spirit_profiles.json`` ``visualColor`` per class + the #FFB45C fallback |
| ``proj.<vfxId>*`` (hero) | the ``impactColor`` of the skills with that ``vfxId`` |
| ``proj.pet_bolt*`` | ``pets.json``: a ranged pet's ``combat.color``, bolt abilities' fire #FF7A2A / arcane #CC44CC / own colour (gating for pets met in Chapter 1) |

Rules (gating — exit 1 when one fails):

| Rule | Applies to | Fails when |
|---|---|---|
| ``violet`` | hero recipes | a drawn colour with HSV saturation > .5 lies within 30° of hue of breach-violet #B02FD0 (the mage's mana shield #8A5CFF sits 31° away) |
| ``fill`` | hero recipes | a layer draws a filling ground shape: a ground-oriented filled sprite (not a ring / rune / crack / slash) whose size grows ≥ 1.5 × while its alpha, when 95 % of the growth is done, is still ≥ half its peak — the enemy telegraph language |
| ``ember`` | enemy recipes, monster emissive / eye swatches, monster manifest ``fx`` colours | a colour is ember orange: hue 15-35°, saturation > .6 (reserved for the hero, the Abyssfire, loot and refuges) and HSV value ≥ .35 (a darker colour reads as black / brown: a pupil) |

The value floor and the Chapter-1 scope are relaxations of DR5's wording, pending confirmation (Docs/DECISIONS.md R22):
the report's ``relaxations`` block lists both with their reasons and everything they exempt today.

Advisory (reported, never gating): violet / fill hits in friendly (loot, rewards, refuges, props) and neutral (status
loops, ambience, NPC notifies, fallbacks) recipes; violet / ember in event colours of later chapters (monster bolts
outside ``CH1_ZONES``, pets met after Chapter 1); enemy colours within 5° outside the ember hue band (``nearEmber``: the
goblin chief's "eyeEmbers" sit at 14°); enemy filling ground shapes and whether they keep the hazard colours
(crimson #D11F2E rim, bone #FFE8DC fill). Event colours the audit cannot know offline (loot quality, lore props, the
``proj.default`` fallback) are listed as unresolved.

Sides come from the recipe id (``SIDES``, first match): ``elite.*``, ``proj.monster_bolt*``, ``hit.claw``,
``teleport.blink`` (the elite blink), ``death.burst`` and the dark-remake prefixes ``breach.`` / ``telegraph.`` /
``hazard.`` / ``enemy.`` / ``monster.`` are enemy; ``skill.*``, ``hero.*``, ``hit.*``, ``ground.*`` and the hero
projectiles (pet bolts included) are hero; ``loot.*``, ``reward.*``, ``camp.*``, ``portal.*``, ``exit.portal``,
``prop.*``, ``quest.*`` are friendly; ``notify.<N>`` / ``npc.<N>`` follow the manifest assets whose clips carry notify
``N`` (monsters → enemy, heroes → hero, NPCs → neutral); everything else is neutral.

Writes ``Art/Previews/ch1_dark/hue_audit_report.json`` (the baseline on today's assets: every rule, every hit with
recipe / layer / colour source / hue / saturation, the advisories, the unresolved event usages) and ``hue_audit.png``
(every resolved colour on a hue × saturation strip per side, the violet guard band and the ember band shaded, failing
colours ringed red). ``--report-only`` writes the report and exits 0 even when a rule fails.
"""
from __future__ import annotations

import colorsys
import json
import os
import re
import struct
import sys
import time
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parent
ART = BLENDER.parent
UNREAL = ART.parent


def _env_path(name: str, default: Path) -> Path:
    """``kit.paths``' overrides (``AF_EXPORT_ROOT`` / ``AF_PREVIEW_ROOT`` / ``AF_PALETTE_ROOT``) without bpy."""
    v = os.environ.get(name)
    return Path(v).resolve() if v else default


EXPORT = _env_path("AF_EXPORT_ROOT", ART / "Export")
PALETTES = _env_path("AF_PALETTE_ROOT", BLENDER / "palettes")
RECIPES = EXPORT / "VFX" / "vfx_recipes.json"
OUT = _env_path("AF_PREVIEW_ROOT", ART / "Previews") / "ch1_dark"
REPORT = "hue_audit_report.json"
CHART = "hue_audit.png"

BREACH_VIOLET = "#B02FD0"
MANA_SHIELD = "#8A5CFF"
VIOLET_GUARD_DEG = 30.0           # no hero colour within this of breach-violet's hue …
VIOLET_SAT = 0.5                  # … at HSV saturation above this
EMBER_HUE = (15.0, 35.0)          # ember orange family (#FF7A2A): hue 15-35° …
EMBER_SAT = 0.6                   # … at saturation above this
EMBER_MIN_VAL = 0.35              # … and HSV value from this (a darker colour reads black / brown: a pupil, soot)
HAZARD_RIM, HAZARD_FILL = "#D11F2E", "#FFE8DC"
FILL_GROW = 1.5                   # a filling ground shape grows at least this much over its life …
FILL_KEEP = 0.5                   # … and keeps ≥ this share of its peak alpha when 95 % of the growth is done
FILL_MIN_ALPHA = 0.15             # (layers that never get this opaque are ignored)
MIN_ALPHA = 0.05                  # layers whose alpha never reaches this draw nothing
CLUSTER_MIN = 0.04                # a texel colour cluster must hold ≥ 4 % of a cell's coverage to count
CHROMA_SAT = 0.12                 # a sprite cell is pre-coloured when a cluster is more saturated than this
NEAR_EMBER_DEG = 5.0              # advisory: an enemy colour this close outside the ember hue band (sat / value in)
CH1_ZONES = ("emerald_plains",)   # monster projectile colours that gate (later chapters: advisory)
CH1_PET_CHAPTERS = (1,)           # pet bolt colours that gate (pets met in later chapters: advisory)
MONSTER_KILL_COLOR = "#FF4444"    # UAbyssVfxSystem::HandleHit: death.burst on a monster kill (WorldContract 5.3)
SPIRIT_FALLBACK = "#FFB45C"       # UAbyssVfxSystem::HeroSpiritColor without a profile colour
NO_IMPACT_BURST = ("death_mark", "slow_trap")   # Combat.cpp: skills whose hits never play hit.impact
PET_FIRE, PET_ARCANE = "#FF7A2A", "#CC44CC"     # PetCompanion.cpp PcAbilityColor (fire / arcane ability colours)

# Sprites that draw an outline or a glyph, never a filled disc (a ground ring / rune / crack is not a telegraph fill).
OUTLINE_SPRITES = {"Ring", "Rune", "Crack", "CrackGlow", "Slash", "Swirl", "Chain", "Arrow", "Streak", "Bolt", "Beam",
                   "Claw", "Skull", "Spark", "Star", "Flake", "Ember", "Wisp", "Comet", "Drop", "Rock", "Coin", "Shard",
                   "ShardFrost", "Flame", "Impact", "Plus"}

# Layer defaults of the runtime presets (AbyssVfxRecipe.cpp ApplyPreset, as vfx/fx_preview.PRESETS) — only the fields
# the audit reads.
BASE = dict(sprite="Glow", orient="billboard", blend="additive", grow=1.0, sizePow=1.0, alpha0=1.0, alpha1=0.0,
            fadeIn=0.0, alphaPow=1.0, color="mid", variant=0, randomVariants=0)
PRESETS = {
    "glow": dict(sprite="Glow", grow=1.3 / 0.6, sizePow=2, alpha0=0.9, alpha1=0, fadeIn=0.08, alphaPow=1.6),
    "flash": dict(sprite="Core", grow=0.85 / 0.35, sizePow=2, alpha0=0.9, color="core"),
    "glint": dict(sprite="Spark", grow=1 / 0.3, sizePow=3, alpha0=1, fadeIn=0.15, alphaPow=1.5, color="white"),
    "sparks": dict(sprite="Streak", orient="velocity", grow=0.5, alpha0=1, alphaPow=1.2, color="mid", altColor="white"),
    "streak": dict(sprite="Streak", orient="velocity", alpha0=1),
    "motes": dict(sprite="Ember", alpha0=1, alphaPow=1.5),
    "flames": dict(sprite="Flame", orient="upright", grow=0.7, alpha0=0.95, fadeIn=0.1, alphaPow=1.4, color="mid"),
    "smoke": dict(sprite="Smoke", blend="translucent", grow=1.25 / 0.6, sizePow=1.5, alpha0=0.8, fadeIn=0.12,
                  alphaPow=1.3, color="#5A5060", randomVariants=2),
    "debris": dict(sprite="Rock", mesh="SM_FX_Rock_A", blend="translucent", alpha0=1, alphaPow=5, color="#8A8E98",
                   randomVariants=2),
    "ring": dict(sprite="Ring", orient="ground", grow=5, sizePow=3, alpha0=0.8, fadeIn=0.05, alphaPow=1.5),
    "shock": dict(sprite="Shock", orient="ground", grow=5, sizePow=3, alpha0=0.9, alphaPow=1),
    "decal": dict(sprite="Scorch", orient="ground", blend="translucent", grow=1 / 0.85, sizePow=2, alpha0=0.6,
                  fadeIn=0.1, alphaPow=2.5, color="lin:0.08,0.06,0.06"),
    "slash": dict(sprite="Slash", grow=1.2, sizePow=2, alpha0=1, alphaPow=1.5),
    "swipe": dict(sprite="Slash", orient="ground", grow=1.1, sizePow=2, alpha0=1, alphaPow=1.5),
    "beam": dict(sprite="Beam", orient="upright", grow=2, sizePow=3, alpha0=0.9, fadeIn=0.1, alphaPow=1),
    "gather": dict(sprite="Glow", alpha0=0.2, alpha1=1, alphaPow=1),
    "bolt": dict(sprite="Bolt", orient="beam", alpha0=1, alphaPow=1.5, randomVariants=4),
    "mesh": dict(blend="translucent", alpha0=1, alpha1=1),
}

# (id prefix or exact id, side) — first match wins; ``notify.`` / ``npc.`` are resolved from the manifest first.
SIDES = [
    ("proj.monster_bolt", "enemy"), ("proj.pet_bolt", "hero"), ("proj.default", "neutral"),
    ("hit.claw", "enemy"), ("teleport.blink", "enemy"), ("death.burst", "enemy"), ("exit.portal", "friendly"),
    ("exit.", "neutral"),
    ("elite.", "enemy"), ("breach.", "enemy"), ("telegraph.", "enemy"), ("hazard.", "enemy"), ("enemy.", "enemy"),
    ("monster.", "enemy"),
    ("skill.", "hero"), ("hero.", "hero"), ("hit.", "hero"), ("ground.", "hero"), ("proj.", "hero"),
    ("loot.", "friendly"), ("reward.", "friendly"), ("camp.", "friendly"), ("portal.", "friendly"),
    ("prop.", "friendly"), ("quest.", "friendly"),
    ("status.", "neutral"), ("ambient.", "neutral"), ("npc.", "neutral"), ("notify.", "neutral"),
]
SIDE_ORDER = ("hero", "friendly", "neutral", "enemy")


# ── colour maths ────────────────────────────────────────────────────────────────────────────────────────
def hex01(h) -> tuple[float, float, float]:
    if isinstance(h, (int, float)):
        v = int(h)
        return ((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0
    s = str(h).strip().lstrip("#")
    return tuple(int(s[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def to_lin(c):
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


def to_srgb(c):
    return tuple(12.92 * x if x <= 0.0031308 else 1.055 * x ** (1 / 2.4) - 0.055 for x in (max(0.0, min(1.0, v))
                                                                                         for v in c))


def hexs(c) -> str:
    return "#%02X%02X%02X" % tuple(int(round(max(0.0, min(1.0, x)) * 255)) for x in c)


def hsv(c) -> tuple[float, float, float]:
    h, s, v = colorsys.rgb_to_hsv(*c)
    return h * 360.0, s, v


def hue_dist(a: float, b: float) -> float:
    d = abs(a - b) % 360.0
    return min(d, 360.0 - d)


VIOLET_HUE = hsv(hex01(BREACH_VIOLET))[0]
MANA_HUE = hsv(hex01(MANA_SHIELD))[0]


def is_violet(c) -> bool:
    h, s, _ = hsv(c)
    return s > VIOLET_SAT and hue_dist(h, VIOLET_HUE) < VIOLET_GUARD_DEG


def is_ember(c) -> bool:
    h, s, v = hsv(c)
    return s > EMBER_SAT and v >= EMBER_MIN_VAL and EMBER_HUE[0] <= h <= EMBER_HUE[1]


def floor_exempt(c) -> bool:
    """In the ember hue / saturation band but darker than ``EMBER_MIN_VAL``: exempted by the value floor (reported)."""
    h, s, v = hsv(c)
    return s > EMBER_SAT and v < EMBER_MIN_VAL and EMBER_HUE[0] <= h <= EMBER_HUE[1]


def near_ember(c) -> bool:
    """Outside the ember band by at most ``NEAR_EMBER_DEG`` of hue, saturation and value inside (advisory)."""
    h, s, v = hsv(c)
    lo, hi = EMBER_HUE
    return (s > EMBER_SAT and v >= EMBER_MIN_VAL and not lo <= h <= hi
            and (lo - NEAR_EMBER_DEG <= h < lo or hi < h <= hi + NEAR_EMBER_DEG))


def palette_from_color(lin):
    """The runtime's ``event`` palette (fx_preview.palette_from_color): linear lerps of the event colour."""
    lerp = lambda a, b, t: tuple(x + (y - x) * t for x, y in zip(a, b))  # noqa: E731
    return {"core": lerp(lin, (1, 1, 1), 0.7), "mid": lerp(lin, (1, 1, 1), 0.25), "rim": tuple(lin),
            "dark": tuple(x * 0.3 for x in lin)}


# ── PNG (stdlib reader for the T_FX_* textures: 8-bit, non-interlaced) ─────────────────────────────────
def read_png(path: Path) -> tuple[int, int, int, bytearray]:
    """(width, height, channels, raw bytes row-major) of an 8-bit non-interlaced PNG."""
    b = path.read_bytes()
    if b[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    i, idat = 8, bytearray()
    w = h = ct = bd = il = 0
    while i < len(b):
        n = struct.unpack(">I", b[i:i + 4])[0]
        tag = b[i + 4:i + 8]
        if tag == b"IHDR":
            w, h, bd, ct, _, _, il = struct.unpack(">IIBBBBB", b[i + 8:i + 21])
        elif tag == b"IDAT":
            idat += b[i + 8:i + 8 + n]
        i += 12 + n
    if bd != 8 or il != 0 or ct not in (0, 2, 4, 6):
        raise ValueError(f"{path}: unsupported PNG (bit depth {bd}, colour type {ct}, interlace {il})")
    ch = {0: 1, 2: 3, 4: 2, 6: 4}[ct]
    raw = zlib.decompress(bytes(idat))
    stride = w * ch
    out = bytearray(h * stride)
    prev = bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        row = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if f == 1:
            for x in range(ch, stride):
                row[x] = (row[x] + row[x - ch]) & 255
        elif f == 2:
            row = bytearray((a + c) & 255 for a, c in zip(row, prev))
        elif f == 3:
            for x in range(stride):
                left = row[x - ch] if x >= ch else 0
                row[x] = (row[x] + ((left + prev[x]) >> 1)) & 255
        elif f == 4:
            for x in range(stride):
                a = row[x - ch] if x >= ch else 0
                up = prev[x]
                c = prev[x - ch] if x >= ch else 0
                p = a + up - c
                pa, pb, pc = abs(p - a), abs(p - up), abs(p - c)
                row[x] = (row[x] + (a if pa <= pb and pa <= pc else up if pb <= pc else c)) & 255
        out[y * stride:(y + 1) * stride] = row
        prev = row
    return w, h, ch, out


def write_png(path: Path, w: int, h: int, rgb: bytearray) -> None:
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


class Textures:
    """Texel colour clusters of every sprite cell (``T_FX_<sprite>.png``: linear RGBA, a strip of ``cells``)."""

    def __init__(self, root: Path, cells: dict[str, int]):
        self.root, self.cells, self._cache = root, cells, {}

    def clusters(self, sprite: str, cell: int) -> list[tuple[tuple[float, float, float], float]]:
        """[(linear rgb, share of the cell's coverage)] of the cell's colours (≥ CLUSTER_MIN), or [] when the
        texture is missing."""
        key = (sprite, cell)
        if key in self._cache:
            return self._cache[key]
        p = self.root / f"T_FX_{sprite}.png"
        if not p.is_file():
            self._cache[key] = []
            return []
        if (sprite, "_img") not in self._cache:
            self._cache[(sprite, "_img")] = read_png(p)
        w, h, ch, px = self._cache[(sprite, "_img")]
        n = max(1, self.cells.get(sprite, 1))
        cw = w // n
        x0 = min(cell, n - 1) * cw
        bins: dict[tuple, list] = {}
        total = 0.0
        step = 2 if cw * h > 8192 else 1
        for y in range(0, h, step):
            base = y * w * ch
            for x in range(x0, x0 + cw, step):
                o = base + x * ch
                if ch >= 3:
                    r, g, b_ = px[o], px[o + 1], px[o + 2]
                    a = px[o + 3] / 255.0 if ch == 4 else 1.0
                else:
                    r = g = b_ = px[o]
                    a = px[o + 1] / 255.0 if ch == 2 else 1.0
                if a < 0.5:
                    continue
                k = (r >> 4, g >> 4, b_ >> 4)
                acc = bins.setdefault(k, [0.0, 0.0, 0.0, 0.0])
                acc[0] += r * a
                acc[1] += g * a
                acc[2] += b_ * a
                acc[3] += a
                total += a
        out = []
        for acc in sorted(bins.values(), key=lambda v: -v[3]):
            if total <= 0 or acc[3] / total < CLUSTER_MIN:
                continue
            out.append(((acc[0] / acc[3] / 255.0, acc[1] / acc[3] / 255.0, acc[2] / acc[3] / 255.0), acc[3] / total))
        self._cache[key] = out
        return out

    def chromatic(self, sprite: str, cell: int) -> list[tuple[tuple[float, float, float], float]]:
        """The cell's clusters when it is pre-coloured (a cluster more saturated than CHROMA_SAT), else []."""
        cl = self.clusters(sprite, cell)
        return cl if any(hsv(to_srgb(c))[1] > CHROMA_SAT for c, _ in cl) else []


# ── recipe model ────────────────────────────────────────────────────────────────────────────────────────
def layer_model(j: dict) -> dict:
    L = dict(BASE)
    if j.get("emitter") in PRESETS:
        L.update(PRESETS[j["emitter"]])
    if j.get("sprite"):
        L["sprite"] = j["sprite"]
    if "mesh" in j:
        L["mesh"] = j["mesh"] or None
    for k in ("orient", "blend"):
        if k in j:
            L[k] = j[k]
    for k in ("grow", "sizePow", "fadeIn", "alphaPow"):
        if k in j:
            L[k] = float(j[k])
    if "alpha" in j:
        a = j["alpha"]
        if isinstance(a, list):
            L["alpha0"], L["alpha1"] = float(a[0]), float(a[1])
        else:
            L["alpha0"] = float(a)
    if "color" in j:
        L["color"] = j["color"]
    if "altColor" in j:
        L["altColor"] = j["altColor"]
    L["fadeIn"] = min(max(L["fadeIn"], 0.0), 0.95)
    L["sizePow"] = max(0.01, L["sizePow"])
    L["alphaPow"] = max(0.01, L["alphaPow"])
    L["variant"] = int(j.get("variant", L["variant"]))
    L["randomVariants"] = int(j.get("randomVariants", L["randomVariants"]))
    return L


def alpha_at(L: dict, t: float) -> float:
    """``UAbyssVfxSystem`` alpha over life (fade-in, then ease to alpha1 with alphaPow)."""
    if L["fadeIn"] > 0 and t < L["fadeIn"]:
        return L["alpha0"] * t / L["fadeIn"]
    u = (t - L["fadeIn"]) / (1.0 - L["fadeIn"]) if L["fadeIn"] < 1.0 else 1.0
    return L["alpha0"] + (L["alpha1"] - L["alpha0"]) * max(0.0, min(1.0, u)) ** L["alphaPow"]


def peak_alpha(L: dict) -> float:
    return max(alpha_at(L, i / 200.0) for i in range(201))


def fill_shape(L: dict) -> dict | None:
    """The telegraph test of one layer: ground-oriented filled sprite growing ≥ FILL_GROW× while it keeps ≥ FILL_KEEP
    of its peak alpha at 95 % of the growth. Returns the measured numbers when it fills, else None."""
    if L["orient"] != "ground" or L.get("mesh") or L["sprite"] in OUTLINE_SPRITES or L["grow"] < FILL_GROW:
        return None
    peak = peak_alpha(L)
    if peak < FILL_MIN_ALPHA:
        return None
    t95 = 1.0 - 0.05 ** (1.0 / L["sizePow"])
    a95 = alpha_at(L, t95)
    if a95 < FILL_KEEP * peak:
        return None
    return {"grow": round(L["grow"], 3), "peakAlpha": round(peak, 3), "alphaAt95": round(a95, 3),
            "t95": round(t95, 3)}


# ── context: sides, event colours ───────────────────────────────────────────────────────────────────────
class Context:
    def __init__(self, doc: dict):
        self.doc = doc
        self.palettes = {k: {s: to_lin(hex01(v)) for s, v in p.items()} for k, p in doc.get("palettes", {}).items()}
        self.cells = {k: int(v.get("cells", 1)) for k, v in doc.get("sprites", {}).items()}
        self.tex = Textures(EXPORT / "VFX", self.cells)
        man = EXPORT / "manifest.json"
        self.manifest = json.loads(man.read_text()) if man.is_file() else {"assets": {}}
        self.notify_owner: dict[str, set] = {}
        for name, e in self.manifest.get("assets", {}).items():
            cat = e.get("category", "")
            side = {"Monsters": "enemy", "Characters": "hero", "NPCs": "neutral", "Pets": "hero"}.get(cat)
            if side is None:
                continue
            for a in e.get("anims", []) or []:
                for n in a.get("notifies", []) or []:
                    nm = n.get("name") if isinstance(n, dict) else n
                    if nm:
                        self.notify_owner.setdefault(nm, set()).add(side)
        data = UNREAL / "Data"

        def table(name: str) -> dict:
            q = data / name
            return json.loads(q.read_text()) if q.is_file() else {}
        self.data_read = sorted(n for n in ("elite_affixes.json", "monsters.json", "hit_feedback.json",
                                            "classes.json", "spirit_profiles.json", "pets.json")
                                if (data / n).is_file())
        # enemy: elite affix colours, ranged monsters' bolt colours (derived.projectileColor: the keyword rule, or
        # the authored colour once the remake exports it; only ranged monsters launch a bolt)
        self.elite = {k: d["vfxColor"] for k, d in table("elite_affixes.json").get("definitions", {}).items()
                      if "vfxColor" in d}
        md = table("monsters.json")
        zone_of = {mid: z for z, ids in md.get("byZone", {}).items() for mid in ids}
        self.projectiles = []                 # (monster id, zone, colour)
        for mid, d in sorted(md.get("defs", {}).items()):
            der = d.get("derived", {})
            col = d.get("projectileColor", der.get("projectileColor"))
            if col is not None and der.get("isRanged", d.get("attackRange", 0) > 2.5):
                self.projectiles.append((mid, der.get("homeZone") or zone_of.get(mid) or "dungeon", col))
        # hero: hit.impact (class basic-impact colours, every active skill's impactColor but the skills without an
        # impact burst), hero.resonance (class spirit colours), proj.<vfxId> (the skill's impactColor), pet bolts
        hf = table("hit_feedback.json")
        self.impact = [(f"hit_feedback.classImpactColors.{c}", v) for c, v in hf.get("classImpactColors", {}).items()]
        if "fallbackImpactColor" in hf:
            self.impact.append(("hit_feedback.fallbackImpactColor", hf["fallbackImpactColor"]))
        self.skill_colors: dict[str, list] = {}       # vfxId -> [(label, colour)]
        for cl in table("classes.json").get("classes", []):
            for sk in cl.get("skills", []):
                der = sk.get("derived", {})
                col = der.get("impactColor")
                if col is None or der.get("passive"):
                    continue
                lab = f"classes.{cl.get('id')}.{sk.get('id')}.impactColor"
                if der.get("execKind") not in NO_IMPACT_BURST:
                    self.impact.append((lab, col))
                self.skill_colors.setdefault(der.get("vfxId") or sk.get("id"), []).append((lab, col))
        self.spirit = [(f"spirit_profiles.{c}.visualColor", d["visualColor"])
                       for c, d in table("spirit_profiles.json").get("profiles", {}).items() if d.get("visualColor")]
        self.spirit.append(("HeroSpiritColor fallback", SPIRIT_FALLBACK))
        self.pet_bolts = []                   # (label, colour, gating)
        for pet in table("pets.json").get("pets", []):
            c = pet.get("combat", {})
            gate = pet.get("chapter", 1) in CH1_PET_CHAPTERS
            ch = f"chapter {pet.get('chapter', '?')}"
            if c.get("style") == "ranged" and "color" in c:
                self.pet_bolts.append((f"pets.{pet['id']}.combat.color ({ch})", c["color"], gate))
            for a in pet.get("abilities", []):
                if a.get("kind") != "bolt":
                    continue
                el = a.get("element", c.get("element"))
                col = PET_FIRE if el == "fire" else PET_ARCANE if el == "arcane" else c.get("color")
                if col is not None:
                    self.pet_bolts.append((f"pets.{pet['id']}.{a['id']} ({el}, {ch})", col, gate))

    def side(self, rid: str) -> str:
        for pre in ("notify.", "npc."):
            if rid.startswith(pre):
                owners = self.notify_owner.get(rid[len(pre):], set())
                for s in ("enemy", "hero"):
                    if s in owners:
                        return s
        for pre, s in SIDES:
            if rid == pre or (pre.endswith(".") and rid.startswith(pre)) or rid.startswith(pre + "."):
                return s
        return "neutral"

    def event_colors(self, rid: str, side: str) -> list[tuple[str, tuple, bool]]:
        """[(label, linear colour, gating)] the runtime passes as the event colour of a recipe (what
        ``UAbyssVfxSystem`` sets ``Context.Color`` from), for the enemy and hero recipes the audit can resolve
        offline; [] for the rest (listed as unresolved when a layer needs it)."""
        def lin(c):
            return to_lin(hex01(c))
        if side == "enemy":
            if rid.startswith("elite."):
                c = self.elite.get(rid[len("elite."):])
                return [(f"elite_affixes.{rid[6:]}.vfxColor", lin(c), True)] if c is not None else []
            if rid.startswith("proj.monster_bolt"):
                return [(f"monsters.{m}.projectileColor ({z})", lin(c), z in CH1_ZONES) for m, z, c in self.projectiles]
            if rid == "death.burst":            # hero (#CC2222) / escort (#E67E22) deaths reuse it: not enemy colours
                return [("monster kill (UAbyssVfxSystem::HandleHit)", lin(MONSTER_KILL_COLOR), True)]
            return []
        if side != "hero":
            return []
        if rid == "hit.impact" or rid.startswith("hit.impact."):
            return [(lab, lin(c), True) for lab, c in self.impact]
        if rid == "hero.resonance" or rid.startswith("hero.resonance."):
            return [(lab, lin(c), True) for lab, c in self.spirit]
        if rid.startswith("proj.pet_bolt"):
            return [(lab, lin(c), g) for lab, c, g in self.pet_bolts]
        m = re.fullmatch(r"proj\.(.+?)(\.(?:launch|hit|fizzle))?", rid)
        if m:
            return [(lab, lin(c), True) for lab, c in self.skill_colors.get(m.group(1), [])]
        return []


# ── colour resolution ───────────────────────────────────────────────────────────────────────────────────
def resolve(ctx: Context, rid: str, recipe: dict, side: str) -> tuple[list[dict], list[dict]]:
    """Every colour the recipe can draw: ([hit], [unresolved]); a hit = {layer, sprite, source, hex, hue, sat, val,
    blend, gating}."""
    pal_name = recipe.get("palette", "event")
    events = ctx.event_colors(rid, side)
    if pal_name == "element":
        pals = [(f"element:{n}", ctx.palettes[n], True) for n in ("fire", "frost", "lightning", "poison", "arcane",
                                                                    "steel") if n in ctx.palettes]
    elif pal_name == "event":
        pals = [(f"event:{lab}", palette_from_color(c), g) for lab, c, g in events]
    else:
        pals = [(pal_name, ctx.palettes.get(pal_name), True)] if pal_name in ctx.palettes else []
    hits, unresolved = [], []

    def tints(spec):
        """[(source label, linear tint, gating)] for one colour spec."""
        if isinstance(spec, (int, float)) or (isinstance(spec, str) and spec.startswith("#")):
            return [(f"fixed {hexs(hex01(spec))}", to_lin(hex01(spec)), True)]
        if isinstance(spec, str) and spec.startswith("lin:"):
            return [("preset fixed", tuple(float(x) for x in spec[4:].split(",")), True)]
        if spec == "white":
            return [("white", (1.0, 1.0, 1.0), True)]
        if spec == "event":                   # the event colour (runtime) - else the palette mid, which only
            return [(f"event {lab}", c, g) for lab, c, g in events]   # applies when the binding passes none
        if spec in ("core", "mid", "rim", "dark"):
            return [(f"{pn}.{spec}", p[spec], g) for pn, p, g in pals if p]
        return []

    layers = [("layer", i, layer_model(j)) for i, j in enumerate(recipe.get("layers", []))]
    if recipe.get("light"):
        layers.append(("light", -1, dict(BASE, sprite="LightPool", orient="ground", color="event" if events else "mid",
                                         alpha0=1.0, alpha1=1.0, variant=0, randomVariants=0)))
    for kind, i, L in layers:
        if kind == "layer" and peak_alpha(L) < MIN_ALPHA:
            continue
        what = L.get("mesh") or L["sprite"]
        cells = list(range(L["randomVariants"])) if L["randomVariants"] > 1 else [L["variant"]]
        specs = [("color", L["color"])] + ([("altColor", L["altColor"])] if "altColor" in L else [])
        for slot, spec in specs:
            ts = tints(spec)
            if not ts:
                unresolved.append({"recipe": rid, "layer": i, "what": what, "color": spec, "palette": pal_name,
                                   "why": "event colour set at runtime" if spec == "event" or pal_name == "event"
                                   else "unknown colour / palette"})
                continue
            for src, tint, gating in ts:
                tex = [] if L.get("mesh") or kind == "light" else [cl for c in cells
                                                                   for cl in ctx.tex.chromatic(L["sprite"], c)]
                cols = [(tuple(t * x for t, x in zip(tint, texel)), f"{src} x T_FX_{L['sprite']} texel")
                        for texel, _ in tex] if tex else [(tint, src)]
                for lin, label in cols:
                    c = to_srgb(lin)
                    h, s, v = hsv(c)
                    hits.append({"recipe": rid, "layer": i, "what": what, "slot": slot, "source": label,
                                 "hex": hexs(c), "hue": round(h, 1), "sat": round(s, 3), "val": round(v, 3),
                                 "blend": L["blend"], "orient": L["orient"], "gating": gating})
    return hits, unresolved


# ── enemy art colours (outside the recipes) ─────────────────────────────────────────────────────────────
def enemy_art(ctx: Context) -> list[dict]:
    """Monster palette swatches that glow or are eyes, and the colours of the monster manifest ``fx`` blocks."""
    out = []
    fams = set()
    for name, e in ctx.manifest.get("assets", {}).items():
        if e.get("category") != "Monsters":
            continue
        for s in e.get("materialSlots", []) or []:
            if s.get("palette"):
                fams.add(s["palette"])

        def walk(v, path):
            if isinstance(v, str) and re.fullmatch(r"#[0-9A-Fa-f]{6}", v):
                out.append({"source": f"manifest {name}.fx.{path}", "hex": v.upper()})
            elif isinstance(v, dict):
                for k, x in v.items():
                    walk(x, f"{path}.{k}" if path else k)
            elif isinstance(v, list):
                for k, x in enumerate(v):
                    walk(x, f"{path}[{k}]")
        walk(e.get("fx", {}), "")
    for fam in sorted(f for f in fams if f.startswith("Monsters")):
        p = PALETTES / f"{fam}.json"
        if not p.is_file():
            continue
        for sw in json.loads(p.read_text()).get("swatches", []):
            if sw.get("free"):
                continue
            eye = any(re.search(r"\.(eye|eyes|sclera|pupil|marker)$", n) for n in sw["names"])
            if sw.get("e", 0) > 0 or eye:
                out.append({"source": f"palette {fam}[{sw['i']}] {'/'.join(sw['names'][:3])}"
                                      f"{' (emissive %.2f)' % sw['e'] if sw.get('e', 0) > 0 else ' (eye)'}",
                            "hex": sw["hex"].upper()})
    for o in out:
        h, s, v = hsv(hex01(o["hex"]))
        o.update(hue=round(h, 1), sat=round(s, 3), val=round(v, 3), ember=is_ember(hex01(o["hex"])))
    return out


# ── chart ───────────────────────────────────────────────────────────────────────────────────────────────
_GLYPHS = {   # 3 x 5 digits / letters for the axis labels
    "0": "111101101101111", "1": "010110010010111", "2": "111001111100111", "3": "111001111001111",
    "4": "101101111001001", "5": "111100111001111", "6": "111100111101111", "7": "111001001001001",
    "8": "111101111101111", "9": "111101111001111", "H": "101101111101101", "E": "111100111100111",
    "R": "110101110101101", "O": "111101101101111", "F": "111100111100100", "I": "111010010010111",
    "N": "110101101101101", "D": "110101101101110", "L": "100100100100111", "Y": "101101010010010",
    "U": "101101101101111", "T": "111010010010010", "A": "010101111101101", "M": "101111111101101",
    "V": "101101101101010", "B": "110101110101110", "S": "111100111001111", "C": "111100100100111",
    "G": "111100101101111", "K": "101110100110101", "P": "111101111100100", "W": "101101111111101",
    " ": "000000000000000", ".": "000000000000010", "-": "000000111000000",
}


def chart(groups: dict[str, list[dict]], path: Path) -> None:
    """Hue (x, 2 px per degree) × saturation (y, per side row) dot chart of every resolved colour."""
    LEFT, RH, PAD = 76, 90, 14
    W = LEFT + 720 + 24
    H = PAD + len(SIDE_ORDER) * (RH + PAD) + 16
    img = bytearray(bytes((24, 22, 28)) * (W * H))

    def put(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            o = (y * W + x) * 3
            img[o:o + 3] = bytes(c)

    def text(x, y, s, c=(220, 216, 205)):
        for ch in s.upper():
            g = _GLYPHS.get(ch, _GLYPHS[" "])
            for k, bit in enumerate(g):
                if bit == "1":
                    for dy in range(2):
                        for dx in range(2):
                            put(x + (k % 3) * 2 + dx, y + (k // 3) * 2 + dy, c)
            x += 8
    for r, side in enumerate(SIDE_ORDER):
        y0 = PAD + r * (RH + PAD)
        text(4, y0 + RH // 2 - 5, side)
        for x in range(LEFT, LEFT + 720):
            hue = (x - LEFT) / 2.0
            band = None
            if side == "hero" and hue_dist(hue, VIOLET_HUE) < VIOLET_GUARD_DEG:
                band = (70, 40, 80)
            if side == "enemy" and EMBER_HUE[0] <= hue <= EMBER_HUE[1]:
                band = (80, 52, 30)
            for y in range(y0, y0 + RH):
                sat = 1.0 - (y - y0) / (RH - 1)
                lim = VIOLET_SAT if side == "hero" else EMBER_SAT
                if band and sat > lim:
                    put(x, y, band)
                elif (x - LEFT) % 60 == 0 or y in (y0, y0 + RH - 1):
                    put(x, y, (48, 46, 54))
            if abs(hue - VIOLET_HUE) < 0.26 or (side == "hero" and abs(hue - MANA_HUE) < 0.26):
                for y in range(y0, y0 + RH, 2):     # breach-violet (every row) and the mana shield (hero row)
                    put(x, y, (176, 47, 208) if abs(hue - VIOLET_HUE) < 0.26 else (138, 92, 255))
        for d in groups.get(side, []):
            x = LEFT + int(round(d["hue"] * 2)) % 720
            y = y0 + int(round((1.0 - d["sat"]) * (RH - 1)))
            c = hex01(d["hex"])
            col = tuple(int(round(v * 255)) for v in c)
            ring = (255, 60, 60) if d.get("bad") else (210, 210, 210)
            for dy in range(-3, 4):
                for dx in range(-3, 4):
                    rr = dx * dx + dy * dy
                    if rr <= 4:
                        put(x + dx, y + dy, col)
                    elif rr <= 10 and d.get("bad"):
                        put(x + dx, y + dy, ring)
    yb = H - 14
    for deg in range(0, 361, 30):
        text(LEFT + deg * 2 - 6, yb, str(deg))
    write_png(path, W, H, img)


# ── main ────────────────────────────────────────────────────────────────────────────────────────────────
def audit(doc: dict) -> dict:
    ctx = Context(doc)
    recipes = dict(doc.get("recipes", {}))
    for alias, target in doc.get("aliases", {}).items():
        if target in recipes and alias not in recipes:
            recipes[alias] = recipes[target]
    sides: dict[str, list[str]] = {s: [] for s in SIDE_ORDER}
    hits, unresolved, fills = [], [], []
    for rid in sorted(recipes):
        side = ctx.side(rid)
        sides[side].append(rid)
        h, u = resolve(ctx, rid, recipes[rid], side)
        for x in h:
            x["side"] = side
        hits += h
        unresolved += [dict(x, side=side) for x in u]
        for i, j in enumerate(recipes[rid].get("layers", [])):
            f = fill_shape(layer_model(j))
            if f:
                L = layer_model(j)
                fills.append({"recipe": rid, "layer": i, "side": side, "sprite": L["sprite"], **f,
                              "colors": sorted({x["hex"] for x in h if x["layer"] == i})})
    violet = [x for x in hits if x["side"] == "hero" and x["gating"] and is_violet(hex01(x["hex"]))]
    violet_late = [x for x in hits if x["side"] == "hero" and not x["gating"] and is_violet(hex01(x["hex"]))]
    violet_adv = [x for x in hits if x["side"] in ("friendly", "neutral") and is_violet(hex01(x["hex"]))]
    ember = [x for x in hits if x["side"] == "enemy" and x["gating"] and is_ember(hex01(x["hex"]))]
    ember_adv = [x for x in hits if x["side"] == "enemy" and not x["gating"] and is_ember(hex01(x["hex"]))]
    near = [x for x in hits if x["side"] == "enemy" and x["gating"] and near_ember(hex01(x["hex"]))]
    art = enemy_art(ctx)
    art_bad = [a for a in art if a["ember"]]
    art_near = [a for a in art if near_ember(hex01(a["hex"]))]
    hero_fill = [f for f in fills if f["side"] == "hero"]
    fill_adv = [f for f in fills if f["side"] in ("friendly", "neutral")]
    enemy_fill = [f for f in fills if f["side"] == "enemy"]
    for f in enemy_fill:
        f["hazardColours"] = all(hue_dist(hsv(hex01(c))[0], hsv(hex01(HAZARD_RIM))[0]) < 20.0
                                 or hsv(hex01(c))[1] <= 0.2 for c in f["colors"])
    for x in violet + ember:
        x["bad"] = True

    def per_recipe(xs):
        d: dict[str, list] = {}
        for x in xs:
            d.setdefault(x["recipe"], []).append({k: x[k] for k in ("layer", "what", "slot", "source", "hex", "hue",
                                                                       "sat", "blend")})
        return d
    rules = [
        {"rule": "violet", "subject": "hero recipes", "ok": not violet,
         "detail": f"no hero colour with HSV sat > {VIOLET_SAT} within {VIOLET_GUARD_DEG:g} deg of breach-violet "
                   f"{BREACH_VIOLET} (hue {VIOLET_HUE:.1f}); {len(violet)} hits in "
                   f"{len({x['recipe'] for x in violet})} recipes"},
        {"rule": "fill", "subject": "hero recipes", "ok": not hero_fill,
         "detail": f"no hero layer draws a filling ground shape (ground filled sprite growing >= {FILL_GROW:g}x, "
                   f">= {FILL_KEEP:g} of its peak alpha at 95 % of the growth); {len(hero_fill)} layers"},
        {"rule": "ember", "subject": "enemy recipes", "ok": not ember,
         "detail": f"no enemy colour in the ember family (hue {EMBER_HUE[0]:g}-{EMBER_HUE[1]:g}, sat > {EMBER_SAT}); "
                   f"{len(ember)} hits in {len({x['recipe'] for x in ember})} recipes"},
        {"rule": "ember", "subject": "enemy art (monster emissive / eye swatches, manifest fx)", "ok": not art_bad,
         "detail": f"{len(art)} colours checked; {len(art_bad)} ember"},
    ]
    sep = hue_dist(VIOLET_HUE, MANA_HUE)
    # The two relaxations of DR5's wording, listed with what they exempt today (pending confirmation: DECISIONS R22).
    floor_hits = [x for x in hits if x["side"] == "enemy" and floor_exempt(hex01(x["hex"]))]
    floor_art = [a for a in art if floor_exempt(hex01(a["hex"]))]
    relaxations = {
        "status": "pending confirmation (Docs/DECISIONS.md R22); every exemption below is reported, none is silent",
        "emberValueFloor": {
            "rule": "ember", "minValue": EMBER_MIN_VAL,
            "reason": "DR5 reserves ember orange (#FF7A2A family) for the hero; a colour in the hue / saturation band "
                      "but darker than this reads as black / brown (pupils, soot, leather), not as ember",
            "exempted": {"recipes": per_recipe(floor_hits), "art": floor_art}},
        "chapterScope": {
            "rules": ["ember: monster bolt colours (proj.monster_bolt*)", "violet: pet bolt colours (proj.pet_bolt*)"],
            "gatingZones": list(CH1_ZONES), "gatingPetChapters": list(CH1_PET_CHAPTERS),
            "reason": "event colours of later-chapter monsters and pets are web data until each chapter's dark pass "
                      "(DR11) and stay out of the Chapter 1 build (the W7 sealed gate); their dark pass re-runs the "
                      "audit with that chapter added to the gating set",
            "exempted": {"ember": sorted(per_recipe(ember_adv)), "violet": sorted(per_recipe(violet_late))}},
    }
    groups = {s: [x for x in hits if x["side"] == s] for s in SIDE_ORDER}
    groups["enemy"] = groups["enemy"] + [dict(a, bad=a["ember"]) for a in art]
    return {
        "ok": all(r["ok"] for r in rules),
        "constants": {"breachViolet": BREACH_VIOLET, "breachVioletHue": round(VIOLET_HUE, 1),
                      "manaShield": MANA_SHIELD, "manaShieldHue": round(MANA_HUE, 1),
                      "violetToManaDeg": round(sep, 1), "violetToManaOk": sep >= VIOLET_GUARD_DEG,
                      "violetGuardDeg": VIOLET_GUARD_DEG, "violetSat": VIOLET_SAT, "emberHue": list(EMBER_HUE),
                      "emberSat": EMBER_SAT, "emberMinVal": EMBER_MIN_VAL, "nearEmberDeg": NEAR_EMBER_DEG,
                      "hazard": {"rim": HAZARD_RIM, "fill": HAZARD_FILL},
                      "fill": {"grow": FILL_GROW, "keep": FILL_KEEP, "minAlpha": FILL_MIN_ALPHA},
                      "colourSpace": "display sRGB, HSV hue / saturation"},
        "rules": rules,
        "sides": {s: {"recipes": len(v), "colours": len(groups[s]) if s != "enemy" else len(
            [x for x in hits if x["side"] == "enemy"])} for s, v in sides.items()},
        "violations": {"violet": per_recipe(violet), "fill": hero_fill, "ember": per_recipe(ember),
                       "emberArt": art_bad},
        "advisory": {"violetFriendlyNeutral": per_recipe(violet_adv), "fillFriendlyNeutral": fill_adv,
                     "enemyFills": enemy_fill, "emberLaterChapters": per_recipe(ember_adv),
                     "violetLaterChapters": per_recipe(violet_late),
                     "nearEmber": {"recipes": per_recipe(near), "art": art_near,
                                   "note": f"enemy colours within {NEAR_EMBER_DEG:g} deg outside the ember hue band "
                                           f"(sat > {EMBER_SAT}, value >= {EMBER_MIN_VAL}): ember in all but the "
                                           f"number"}},
        "relaxations": relaxations,
        "unresolved": unresolved,
        "recipesBySide": sides,
        "enemyArt": art,
        "_groups": groups,
        "_dataRead": ctx.data_read,
    }


def main(argv: list[str]) -> int:
    opt = {"recipes": str(RECIPES), "out": str(OUT), "report": None}
    flags = set()
    for a in argv:
        if a.startswith("--") and "=" in a:
            k, v = a[2:].split("=", 1)
            if k not in opt:
                raise SystemExit(f"unknown option --{k}")
            opt[k] = v
        elif a.startswith("--"):
            flags.add(a[2:])
        else:
            raise SystemExit(f"unexpected argument {a!r}")
    bad = flags - {"no-report", "report-only", "quiet"}
    if bad:
        raise SystemExit(f"unknown flag(s): {', '.join(sorted(bad))}")
    t0 = time.time()
    src = Path(opt["recipes"]).resolve()
    doc = json.loads(src.read_text())
    res = audit(doc)
    groups = res.pop("_groups")
    out = Path(opt["out"]).resolve()
    q = "quiet" in flags
    c = res["constants"]
    print(f"  breach-violet {c['breachViolet']} hue {c['breachVioletHue']}  mana shield {c['manaShield']} hue "
          f"{c['manaShieldHue']}  separation {c['violetToManaDeg']} deg "
          f"({'ok' if c['violetToManaOk'] else 'BELOW ' + str(VIOLET_GUARD_DEG)})")
    print("  sides: " + ", ".join(f"{s} {v['recipes']} recipes / {v['colours']} colours" for s, v in res["sides"].items()))
    for r in res["rules"]:
        print(f"  [rule] {'OK  ' if r['ok'] else 'FAIL'} {r['rule']:7s} {r['subject']:58s} {r['detail']}")
    if not q:
        for rid, xs in res["violations"]["violet"].items():
            print(f"    violet  {rid:28s} " + ", ".join(sorted({f"{x['hex']} (h {x['hue']:.0f} s {x['sat']:.2f})"
                                                                  for x in xs})))
        for f in res["violations"]["fill"]:
            print(f"    fill    {f['recipe']:28s} layer {f['layer']} {f['sprite']} grow {f['grow']} alpha@95% "
                  f"{f['alphaAt95']} / peak {f['peakAlpha']}")
        for rid, xs in res["violations"]["ember"].items():
            print(f"    ember   {rid:28s} " + ", ".join(sorted({f"{x['hex']} (h {x['hue']:.0f} s {x['sat']:.2f})"
                                                                  for x in xs})))
        for a in res["violations"]["emberArt"]:
            print(f"    ember   {a['source']} {a['hex']}")
        adv = res["advisory"]
        for rid, xs in adv["nearEmber"]["recipes"].items():
            print(f"    near-ember (advisory) {rid:28s} " + ", ".join(sorted({f"{x['hex']} (h {x['hue']:.0f})"
                                                                           for x in xs})))
        for a in adv["nearEmber"]["art"]:
            print(f"    near-ember (advisory) {a['source']} {a['hex']} (h {a['hue']:.0f})")
        print(f"  advisory: violet in {len(adv['violetFriendlyNeutral'])} friendly / neutral recipes and "
              f"{len(adv['violetLaterChapters'])} hero recipes of later chapters, ember in "
              f"{len(adv['emberLaterChapters'])} enemy recipes of later chapters, "
              f"{len(adv['fillFriendlyNeutral'])} friendly / neutral fills, {len(adv['enemyFills'])} enemy fills, "
              f"{len(adv['nearEmber']['recipes']) + len(adv['nearEmber']['art'])} near-ember enemy colours; "
              f"{len(res['unresolved'])} unresolved colour uses")
        rx = res["relaxations"]
        fl = rx["emberValueFloor"]["exempted"]
        print(f"  relaxations (pending, DECISIONS R22): value floor {EMBER_MIN_VAL} exempts "
              f"{len(fl['recipes'])} enemy recipes / {len(fl['art'])} art colours; chapter scope exempts "
              f"{len(rx['chapterScope']['exempted']['ember'])} ember / {len(rx['chapterScope']['exempted']['violet'])} "
              f"violet event-colour recipes of later chapters")
    if "no-report" not in flags:
        out.mkdir(parents=True, exist_ok=True)
        rp = Path(opt["report"]).resolve() if opt["report"] else out / REPORT
        try:
            shown = src.relative_to(UNREAL).as_posix()
        except ValueError:
            shown = str(src)
        doc_out = {"tool": "Art/blender/vfx/hue_audit.py",
                   "spec": "Docs/dark-remake/implementation-plan.md A0 / 0C; README.md DR5; monsters.md 1.2",
                   "source": shown, "eventColourTables": [f"Data/{n}" for n in res.pop("_dataRead")], **res}
        rp.write_text(json.dumps(doc_out, indent=1, ensure_ascii=False) + "\n")
        chart(groups, out / CHART)
        print(f"  {rp}\n  {out / CHART}")
    print(f"  hue audit: {'PASS' if res['ok'] else 'FAIL'} ({time.time() - t0:.1f} s)")
    return 0 if res["ok"] or "report-only" in flags else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
