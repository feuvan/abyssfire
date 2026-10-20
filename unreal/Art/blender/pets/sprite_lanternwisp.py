"""灯魂 Lanternwisp — ``SK_Pet_Lanternwisp[_E1|_E2]``: the dark remake's re-skin of the Chapter-1 companion
``pet_sprite`` (package A11, Docs/dark-remake/chapter1.md S5 ``q_rf_lanternwisp`` and §14; story-bible.md §8-9: "the
lantern of Pell, the youngest of the burial detail, still walking grave to grave").

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/pets/sprite_lanternwisp.py [--stage 0|1|2]
        [--quick [Clip frame]] [--portrait] [--anims Clip …] [--no-previews] [--no-verify]

A **re-skin** of ``pets/sprite.py`` on the same ``SKEL_Pet_Sprite`` (17 bones, same rest joints, same clip set baked
from the same web ``flyPose`` tracks — ``sprite.build_rig`` / ``sprite.SpriteAnims``): only the meshes, the palette
regions, the glow cards and the names change. The brief (chapter1.md S5): *a lantern-gold core, soot wisps, a tiny
bronze bell*; it must read as the opposite of the cold ``false_lantern`` (monsters.md §2.6: cold #CFE8FF and a
mouth vs warm and a bell). Ley-beasts are "re-toned gently, never darkened" (story-bible §8): it stays warm on purpose,
the one warm thing that walks the Weeping Ditch with the hero (DR5: warmth is the hero side's).

* ``body`` — the **lantern-gold core** (the sprite's seed ball, #F4C460), a face of soot-dark eyes with glints, a
  small smile, a warm blush; not emissive (a glowing pale ball's bloom eats its own ink rim, the sprite's lesson):
  the light is the warm aura card **behind** it (``fx.aura``).
* ``sprout`` → the **lantern's bail**: a bronze lantern cap sits on the crown (on ``body``) and its ring handle
  swings on the ``sprout`` bone (the sprite's sway / nod).
* ``wing_*`` → four **soot wisps**: wavering smoke ribbons whose far end hooks back behind the pet (a curl, never a
  point), soft puffs, graded in five bands from a soot root darker than the ash ground to an ash-pale tip over the
  last ~30 % (art review r2: the first puff chain faceted into grey stone shards and its pale half read as a blade at
  1080p) — they flap like the sprite's wings and read as a flickering flame's smoke.
* ``tail_*`` → the **flame**: a tongue leaving the back of the core in a shallow trail behind, S-flickering, graded
  core gold → amber → pale gold → the ember tip, split near its end by a second tongue (art review r2: the smooth
  orange cone read as a duck's foot / beak); the tail card and ``fx_tail`` sit at its tip.
* a **tiny bronze bell** on a short loop under the chin (on ``body``): the Knellwatch bell motif; Pell's lantern
  answers the bell that tolls for the named dead.
* Evolution (``--stage``, ``beast_pet_sprite_e1`` / ``_e2``, pets.json levels 10 / 20): E1 ×1.13 — a lit band round
  the upper core under the cap and a long forked flame (art review r2: its crown petals alone were sub-pixel at
  1080p), five gold flame petals on the cap, two ember motes; E2 ×1.27 — the smoke burns clean: pale ash-gold wisps,
  the lit band and forked flame, taller petals, a gold halo round the cap, brass cap and bell, three motes.
* Ink (art review r2): request 3.2 px (``OUTLINE_PX``), gated at the Chapter-1 character gate 3.0 px — the sprite's
  3.5 px measured a 4.3-4.4 px median with the padded proxies of the first build; the portrait is this module's own
  (``portrait``: the Idle frame with the lowest wisp tips, turned toward profile so the flame shows).

**Game ids**: the remake re-dresses ``pet_sprite`` everywhere (GameData ``data.pet.pet_sprite.name`` = 灯魂 /
Lanternwisp), and a companion follows the hero across presentation themes (the Ember Tower keeps the plains look,
the pet must not change there), so these meshes take the **base** ids ``beast_pet_sprite`` / ``pet_sprite`` /
``beast_pet_sprite_e1`` / ``_e2`` (no ``@march``); the seed-sprite entries ``SK_Pet_Sprite*`` stay in the manifest
with no game ids (``supersededBy``), and ``pets/sprite.py`` no longer claims the ids. The portrait is written under
the runtime's name ``T_UI_Portrait_Pet_Sprite`` (``AbyssUiContext::PetPortrait``: ``pet_sprite`` → ``_Pet_Sprite``).

Previews ``Art/Previews/pet_lanternwisp[_e1|_e2]/`` (the mship set: game, game_1080crop, game_inkgate, closeups,
turnaround, material_ab, anim sheets, game_poses / contacts / facings, poses_close, back sheets, lod1, portrait) on
the march ash grass (``envkit_march.use_march_stage``) — the darkest ground the pet walks in Chapter 1.
"""
from __future__ import annotations

import math
import os
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "monsters", HERE, HERE.parent / "environment",
           HERE.parent / "march"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import sprite as S  # noqa: E402  (reads --stage like this script)
import envkit_march as EM  # noqa: E402,F401  (every review stage of this process on the march ash grass)
from kit import anim, mesh as M, outline, paths, review, scene  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

STAGE = S.STAGE
U = S.U
C0 = S.C0
SUFFIX = S.SUFFIX
ASSET = "SK_Pet_Lanternwisp" + SUFFIX
TOKEN = "Pet_Lanternwisp" + SUFFIX
PREFIX = "lanternwisp" if STAGE == 0 else f"lanternwisp_e{STAGE}"
PREVIEW_DIR = "pet_lanternwisp" if STAGE == 0 else f"pet_lanternwisp_e{STAGE}"
GAME_IDS = ["beast_pet_sprite", "pet_sprite"] if STAGE == 0 else [f"beast_pet_sprite_e{STAGE}"]
SUPERSEDES = "SK_Pet_Sprite" + SUFFIX
DISPLAY = ({"zh-CN": "灯魂", "en": "Lanternwisp"}, {"zh-CN": "灯魂·觉醒", "en": "Lanternwisp (Awakened)"},
           {"zh-CN": "灯魂·长明", "en": "Lanternwisp (Undying)"})[STAGE]
CAP_TOP_U = 3.0                # cap height over the crown (web units)
RING_U = (1.35, 0.40)          # bail ring radius / tube (web units)
HEIGHT = C0.z + S.BODY_R[2] * U + (CAP_TOP_U + 2 * RING_U[0] + 0.6) * U     # rest top: the bail ring

CORE = ("#F4C460", "#F7CC70", "#FAD684")[STAGE]
SMOKE_KEYS = ("smoke", "smoke_1", "smoke_2", "smoke_3", "smoke_tip")
SMOKE = (("#3E3834", "#4E4741", "#645C55", "#7E756C", "#A39A90"),
         ("#3E3834", "#4E4741", "#645C55", "#7E756C", "#A39A90"),
         ("#D8C8A4", "#DDCEAC", "#E3D5B4", "#EADCBC", "#F0E4C4"))
# (shadowAmt, lightAmt) per band: the soot keeps a weak light band (at .36 the lit side of #3E3834 turned mid-grey
# and the wisp read as a grey stick at 1080p again); E2's pale smoke keeps the first pass's soft bands
SMOKE_SL = (((.22, .14), (.22, .16), (.22, .20), (.22, .26), (.20, .32)),) * 2 + (((.24, .36),) * 4 + ((.20, .36),),)
WISP_BANDS = (0.68, 0.77, 0.85, 0.92)  # t (fraction of the wisp's length) where each next band begins
REGIONS = outline.ink_regions(dict(
    core=Region(CORE, .28, .50),                       # lantern gold; its light is the aura card behind it
    blush=Region("#F2985A", .20, .30),
    flame=Region(("#F0A040", "#F2AA48", "#F4B456")[STAGE], .30, .46),
    tail_tip=Region("#FFE2A8", .0, .25, e=0.18),       # the ember tip: lit; its light is the tail card behind
    flame_pale=Region("#FCCB76", .26, .44),            # art review r2: the flame tongue's gold → amber → pale grade
    # first-pass wisp tones (a hard two-tone split at half length; retired by art review r2, kept registered: the
    # registry is append-only)
    wisp=Region(("#625A54", "#665E58", "#D8C8A4")[STAGE], .24, .36),
    wisp_tip=Region(("#A39A90", "#AAA096", "#F0E4C4")[STAGE], .20, .36),
    # art review r2 smoke: a soot root clearly darker than the ash ground (#4C4940) graded to an ash-pale tip over the
    # last ~30 % of the length in five bands (``WISP_BANDS``) — the 50 % hard split read as grey stone shards and, at
    # 1080p, its pale half as a blade jutting from the core. E2's smoke burns clean: pale ash-gold → paler.
    **{k: Region(v, s_, l_) for k, v, (s_, l_) in zip(SMOKE_KEYS, SMOKE[STAGE], SMOKE_SL[STAGE])},
    iris=Region("#2E1C10", .30, .12),
    pupil=Region("#140A04", .30, .05),
    glint=Region("#FFFFFF", .0, .10, e=0.6),
    mouth=Region("#6E3418", .30, .10),
    mouth_in=Region("#4A200C", .30, .05),
    bronze=Region("#A27A48" if STAGE < 2 else "#C29A50", .42, .46),
    bronze_dark=Region("#6A4C2C" if STAGE < 2 else "#8A6A34", .42, .34),
    clapper=Region("#3A2818", .30, .10),
    **({} if STAGE == 0 else dict(
        crown_flame=Region("#FFE08C", .22, .40, e=0.32),
        # art review r2: the awakened core burns lighter in a band under the cap (the lantern's lit window) — with
        # the long forked flame, E1's change at the game camera (its crown petals are sub-pixel at 1080p)
        core_band=Region(("#FFF0C4", "#FFF0C4", "#FFF4D2")[STAGE], .16, .40),     # no emission: its bloom ate the ink
        halo=Region("#FFE89C", .0, .20, e=0.45),
    )),
), S.OUTLINE_CLASS)
if STAGE == 2:   # the pale ash-gold wisps: 80 % ink in their line tone (the loot HULL_O rule for pale parts)
    from dataclasses import replace as _replace
    for _k in ("wisp", "wisp_tip") + SMOKE_KEYS:
        REGIONS[_k] = _replace(REGIONS[_k], o=0.2)


@dataclass
class WispSpec(S.SpriteSpec):
    name: str = ASSET


# Art review r2: at the sprite's 3.5 px request the Lanternwisp's 1080p ink measured a 4.3-4.4 px median (the padded
# proxies round the eye stack, the bell's clapper, the crown and the wisps plus the overlapping hulls where four wisps
# and the flame meet the core); the pads were trimmed (eye stack 2-5 mm proud instead of 6-10, the clapper lifted,
# the crown petals lowered, the flame's tongues each on their own unpadded hull) and the request set to 3.2 px so the
# median sits in 3.5-3.9 px (the seed sprite's ~3.9) — still gated at the Chapter-1 character gate, 3.0 px
OUTLINE_PX = float(os.environ.get("AF_LW_OUTLINE_PX", "3.2"))
OUTLINE_PX_NOTE = ("art review r2: request 3.2 px (the sprite's 3.5 px measured a 4.3-4.4 px median on the Lanternwisp: "
                   "padded proxies and overlapping hulls add ~0.5-0.8 px); the median sits in 3.5-3.9 px and the ink "
                   "is gated at the Chapter-1 character gate, 3.0 px (not request - 0.5)")
GATE_PX = 3.0


# ── the re-skin: the sprite module's names, glows and FX point at the Lanternwisp ──────────────────────────
AURA = ("body", C0, 13.0 + 2 * STAGE, "#FFC878", 0.30 + 0.05 * STAGE)    # warm aura card, BEHIND the pet
TAIL_GLOW = ("tail_04", None, 4.5, "#FFC070", 0.55)
FX = {
    "aura": dict(S.FX["aura"], color=AURA[3], alpha=round(AURA[4], 2),
                 note="warm lantern light (DR5: warmth is the hero side's; the false_lantern is cold #CFE8FF)"),
    "tailGlow": dict(S.FX["tailGlow"], color=TAIL_GLOW[3], alpha=TAIL_GLOW[4],
                     radiusCm=round(TAIL_GLOW[2] * U * 100, 1), sparkles={"color": "#FFE6B0", "count": 3,
                                                                          "trail": "behind the tip, fading"}),
    "castHeal": dict(S.FX["castHeal"], ring="#FFE6A8", glow="#FFD078"),
    "attackZap": dict(S.FX["attackZap"], color="#FFD890"),
    "inkColor": "#2A1A10 (warm soot; the 3D hull mixes #120C18 with the local line tone at 60 % ink)",
}
if STAGE > 0:
    FX["motes"] = dict(S.FX["motes"], glow={"color": "#FFC060", "alpha": 0.7, "radiusCm": round(3 * U * 100, 1)},
                       core="#FFF2D0", note="ember motes")
if STAGE == 2:
    FX["haloGlow"] = dict(S.FX["haloGlow"], color="#FFE490", socket="sprout",
                          note="over the emissive halo ring round the lantern cap (mesh part 'halo', no hull)")


def reskin() -> None:
    """Point the sprite module's names at the Lanternwisp (its rig, clips, glows and portrait code read them)."""
    S.ASSET, S.TOKEN, S.PREFIX, S.PREVIEW_DIR, S.GAME_IDS, S.DISPLAY = (ASSET, TOKEN, PREFIX, PREVIEW_DIR, GAME_IDS,
                                                                       DISPLAY)
    S.HEIGHT = HEIGHT
    S.SpriteSpec = WispSpec
    S.AURA, S.TAIL_GLOW, S.FX = AURA, TAIL_GLOW, FX


reskin()


# ── geometry ──────────────────────────────────────────────────────────────────────────────────────────────
WISP_W = 1.55                  # × the sprite's wing chord: smoke reads by its mass, a wing by its outline
# art review r2: 14 rings × 8 sides faceted the puff chain into stone shards (and rubble in the back views): 16 × 9,
# smooth puffs (sin², no cusps), a rounder section, a thinner tip that stays rounded and hooks back (a pointed curved
# tongue read as a horn); the hull proxy is the same rings on 7 sides (the ink stays near the 3.5 px request;
# hull ≤ .75 × toon)
WISP_RINGS, WISP_SIDES = 16, 9
WISP_HULL = (16, 7)            # the toon chain's own rings on fewer sides (pads only the polygon chord, ~2 mm)


def _wisp_axis(t: float, W: float) -> tuple[float, float]:
    """(x / L, centre y) of the wisp's spine at ``t``: a wavering S (smoke, not a rigid stick), then the far end
    curls back on itself (+chord = behind the pet: smoke trailing the lantern, not a horn curling up)."""
    c = 0.26 * W * math.sin(1.7 * math.pi * t + 0.2) * (0.35 + 0.65 * t) + 0.62 * W * _smooth(0.52, 1.0, t) ** 1.6
    return t - 0.15 * _smooth(0.70, 1.0, t) ** 2, c


def _wisp_half(t: float, W: float) -> float:
    """Half-width of the smoke at ``t``: three soft puffs (sin², never a cusp) on an envelope that swells off the
    root and thins toward a small **rounded** last puff (dome) — never a point."""
    grow = min(1.0, 0.55 + 2.5 * t)
    thin = 1.0 - 0.58 * max(0.0, t - 0.18) / 0.82
    puff = 0.80 + 0.30 * math.sin(math.pi * 2.4 * t + 0.35) ** 2
    return 0.5 * W * grow * thin * puff


def wisp_outline(L: float, W: float, n: int = 30) -> list[tuple[float, float]]:
    """CCW outline of a smoke wisp from the root to its rounded tip (the half-width of :func:`_wisp_tube`)."""
    up, lo = [], []
    for i in range(n + 1):
        t = i / n
        xr, c = _wisp_axis(t, W)
        h = _wisp_half(t, W)
        up.append((L * xr, c + h))
        lo.append((L * xr, c - h))
    return lo + list(reversed(up[1:-1]))


def _smooth(e0: float, e1: float, x: float) -> float:
    u = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return u * u * (3.0 - 2.0 * u)


def _wisp_tube(pair: int, n: int, sides: int) -> M.Part:
    L, W = S.WING_L[pair - 1] * U, S.WING_W[pair - 1] * U * WISP_W
    pts, rads = [], []
    for i in range(n + 1):
        t = i / n
        xr, c = _wisp_axis(t, W)
        r_in = _wisp_half(t, W)
        r_th = max(0.0012, r_in * (0.56 - 0.08 * t))
        pts.append(Vector((L * xr, c, 0.012 * t * t + 0.008 * math.sin(2.0 * math.pi * t) + 0.012 * max(0.0, t - 0.6))))
        rads.append((r_th, r_in))
    return M.tube(pts, rads, sides=sides, up=(0, 0, 1), dome0=0.002, dome1=0.8 * rads[-1][0])


def wisp_part(pair: int) -> tuple[M.Part, M.Part]:
    """(wisp, hull proxy) in the sprite's wing frame (span x, chord y, normal z): a chain of smoke puffs swept
    along the S-bent spine — a flattened tube whose radius swells and narrows (a flat plate read as a blade or a horn
    edge-on and from the front), graded by ``af_sub`` 0-4 from the soot root to the ash-pale tip over the last ~30 %
    (``WISP_BANDS``; art review r2: a hard split at half length read as stone); the ink bakes on a slightly coarser
    copy of the same chain."""
    L, W = S.WING_L[pair - 1] * U, S.WING_W[pair - 1] * U * WISP_W
    p = _wisp_tube(pair, WISP_RINGS, WISP_SIDES)
    ts = np.linspace(0.0, 1.0, 97)
    spine = np.array([(L * a, c) for a, c in (_wisp_axis(float(t), W) for t in ts)])
    lay = p.bm.faces.layers.int.get("af_sub") or p.bm.faces.layers.int.new("af_sub")
    for f in p.bm.faces:                                  # band by the nearest spine point (the curl folds x back)
        c = sum((v.co for v in f.verts), Vector()) / len(f.verts)
        t = float(ts[int(np.argmin(((spine - (c.x, c.y)) ** 2).sum(1)))])
        f[lay] = sum(1 for e in WISP_BANDS if t >= e)
    return p.smooth(True), _wisp_tube(pair, *WISP_HULL)


def lantern_cap() -> tuple[M.Part, M.Part, Matrix]:
    """(cap, rim band, frame): a bronze lantern top seated on the crown (local z = the crown's normal)."""
    top, n = S.ball_point(S.SPROUT_DIR, 0.0)
    fr = S.frame_z(top, n, Vector((0, -1, 0)))
    u = U
    prof = [(3.25 * u, -1.05 * u), (3.45 * u, -0.40 * u), (3.20 * u, 0.30 * u), (2.30 * u, 1.25 * u),
            (1.35 * u, 2.05 * u), (0.95 * u, 2.55 * u), (1.15 * u, 2.80 * u), (0.75 * u, CAP_TOP_U * u),
            (0.01 * u, CAP_TOP_U * u + 0.05 * u)]
    cap = M.lathe(prof, sides=12, cap0="fan", cap1="fan").transform(fr)
    band = M.lathe([(3.30 * u, -0.70 * u), (3.50 * u, -0.45 * u), (3.50 * u, 0.05 * u), (3.20 * u, 0.30 * u)],
                   sides=12, cap0="fan", cap1="fan").transform(fr)
    return cap, band, fr


def bail_ring(fr: Matrix) -> M.Part:
    """The lantern's ring handle over the cap, upright across the pet's forward axis (it swings on ``sprout``)."""
    r, t = RING_U[0] * U, RING_U[1] * U
    zc = (CAP_TOP_U + RING_U[0] - 0.15) * U
    path = [Vector((r * math.cos(a), 0.0, zc + r * math.sin(a))) for a in (k / 12 * math.tau for k in range(12))]
    ring = M.sweep(path, M.superellipse(t, t, 5), closed_path=True, up=(0, 1, 0))
    return ring.transform(fr)


def bell_parts() -> tuple[M.Part, M.Part, M.Part]:
    """(bell, clapper, loop): a tiny bronze bell, mouth down, on a short loop from the lower front of the core —
    forward of the belly so the high W1 camera sees it under the face (under the chin it hid behind the core)."""
    anchor, n = S.ball_point(S.ball_dir(0.86, 0.50, 0.0), 0.0)
    u = U
    top = anchor + Vector((0.0, -0.95 * u, -0.85 * u))
    h = 2.9 * u
    prof = [(1.55 * u, 0.0), (1.46 * u, 0.32 * u), (1.10 * u, 0.95 * u), (0.95 * u, 1.75 * u), (0.86 * u, 2.35 * u),
            (0.48 * u, 2.80 * u), (0.02 * u, h)]
    bell = M.lathe(prof, sides=10, cap0="fan", cap1="fan")
    bell.transform(Matrix.Translation(top - Vector((0, 0, h))))
    clap = M.ellipsoid((0.50 * u, 0.50 * u, 0.42 * u), (top.x, top.y, top.z - h + 0.14 * u), sides=8, rings=4)
    loop = M.sweep([anchor - n * 0.004, anchor + (top - anchor) * 0.5, top], M.superellipse(0.32 * u, 0.32 * u, 6),
                   cap0="fan", cap1="fan")
    return bell, clap, loop


# ── the flame (tail) ──────────────────────────────────────────────────────────────────────────────────────
# Art review r2: a narrow root tucked into the core, an S-flicker sideways, the far end swept back and lifted (a
# flame streaming behind the lantern, not a cone hanging under it), graded gold → amber → pale gold → the ember tip
# along its length, and split: a second, shorter tongue leaves the first near the tip (a flame's flickering split
# tip, not a duck's foot). E1 / E2: the flame is longer and forks early into two long tongues — E1's silhouette change
# at the game camera (its crown petals are sub-pixel at 1080p). Each tongue carries its own padded ink (a lick
# covered by the main tongue's ink tented it 7 cm out).
TAIL_LEN = (1.0, 1.45, 1.36)[STAGE]          # × the sprite's tail line (S.tail_rest, extrapolated past tail_04)
TAIL_BANDS = (0.20, 0.56, 0.80)              # u where amber, pale gold, the ember tip begin (af_sub 1..3)
FLAME_SUBS = {1: "flame", 2: "flame_pale", 3: "tail_tip"}
FORK_U = (0.70, 0.46, 0.48)[STAGE]           # where the second tongue leaves the first (fraction of the length)
FORK_END = (0.94, 0.92, 0.92)[STAGE]         # its tip, as a fraction of the first tongue's length
FORK_SPREAD = (1.5, 2.6, 2.4)[STAGE]         # web units the two tips part sideways


def flame_point(t: float, lat: float = 0.0) -> Vector:
    """Spine of the flame at ``t`` (0 = root, ``TAIL_LEN`` = tip, in the sprite's tail parameter): the tail line
    leaving the **back** of the core (not from under the face: from the front it read as a beak), raised into a
    shallow trail streaming behind (hanging under the core its split tip read as a foot), a lateral flicker (+ ``lat``
    web units)."""
    u = t / TAIL_LEN
    back = 1.4 * t * t + 3.0 * t + 2.6 * (1.0 - min(1.0, u / 0.6)) ** 1.5
    up = 1.6 * (1.0 - min(1.0, u / 0.5)) ** 2
    lift = 5.5 * t                                # a comet trail streaming back, not a foot hanging under the core
    side = 0.9 * math.sin(math.pi * 1.5 * u) + lat
    return S.tail_rest(t) + S.w2b(-back, -lift - up, side)


TAIL_TIP = None                              # set below: the flame's rest tip (glow card + fx_tail socket)


def flame_radius(u: float, r0: float = 2.6) -> float:
    """Half-width (m) at ``u`` ∈ [0, 1]: a swell, then a flickering taper to a point."""
    u = min(1.0, max(0.0, u))
    w = r0 * (1.0 + 0.22 * math.sin(math.pi * min(1.0, u / 0.45))) * (1.0 - u) ** 0.78
    return U * (w * (1.0 + 0.10 * math.sin(3.4 * math.pi * u + 0.5)) + 0.05)


def _graded_tube(pts, rads, us, sides: int) -> M.Part:
    tube = M.tube(pts, rads, sides=sides, dome0=0.002, dome1=0.0005)
    lay = tube.bm.faces.layers.int.get("af_sub") or tube.bm.faces.layers.int.new("af_sub")
    P = [Vector(p) for p in pts]
    for f in tube.bm.faces:
        c = sum((v.co for v in f.verts), Vector()) / len(f.verts)
        i = min(range(len(P)), key=lambda k: (P[k] - c).length)
        f[lay] = sum(1 for e in TAIL_BANDS if us[i] >= e)
    return tube


TAIL_TIP = flame_point(TAIL_LEN)
# the tail glow card and the ``fx_tail`` socket follow the flame's tip (the sprite's sat at tail_rest(1), under the
# core: the comet-trail flame left the card floating below the pet)
TAIL_GLOW = ("tail_04", TAIL_TIP, 4.5, "#FFC070", 0.55)
S.TAIL_GLOW = TAIL_GLOW


def add_flame(b: M.Builder) -> None:
    bind = Bind.blend("tail_01", "tail_02", "tail_03", "tail_04", falloff=4, smooth=0.01)
    n = 13
    t0 = -0.10                                   # the root starts inside the core
    ts = [t0 + (TAIL_LEN - t0) * i / n for i in range(n + 1)]
    us = [max(0.0, t / TAIL_LEN) for t in ts]
    lean = -0.45 * FORK_SPREAD                   # the first tongue leans one way past the fork, the second the other
    pts = [flame_point(t, lean * _smooth(FORK_U, 1.0, u)) for t, u in zip(ts, us)]
    rads = [flame_radius(u) for u in us]
    b.add(_graded_tube(pts, rads, us, 10), "core", bind, name="tail", hull_pad=True, sub_regions=FLAME_SUBS)
    # the second tongue: leaves the first at FORK_U (inside it), leans the other way, a little shorter
    m = 8 if STAGE else 5
    tf0, tf1 = (FORK_U - 0.06) * TAIL_LEN, FORK_END * TAIL_LEN
    ft = [tf0 + (tf1 - tf0) * i / m for i in range(m + 1)]
    fu = [t / TAIL_LEN for t in ft]
    k0 = fu[0]
    fp = [flame_point(t, 0.55 * FORK_SPREAD * _smooth(k0, FORK_END, u)) for t, u in zip(ft, fu)]
    fr = [flame_radius(u) * (0.80 - 0.12 * i / m) for i, u in enumerate(fu)]
    b.add(_graded_tube(fp, fr, fu, 8), "flame", bind, name="tail_fork", hull_pad=True, sub_regions=FLAME_SUBS)


CORE_BAND = (0.36, 0.74)        # E1 / E2 lit band: height on the core as a fraction of its up radius (eyes ~0)


def core_band() -> M.Part:
    """A lighter band round the upper core, under the cap's rim, 1.5 mm proud of the ball (clear of
    its facets; a shell sunk 3 mm inside it, closed by fans inside the ball)."""
    rx, rz = S.BODY_R[0] * U, S.BODY_R[2] * U
    zs = [rz * (CORE_BAND[0] + (CORE_BAND[1] - CORE_BAND[0]) * i / 2) for i in range(3)]

    def r_at(z: float) -> float:
        return rx * math.sqrt(max(0.0, 1.0 - (z / rz) ** 2))
    prof = [(r_at(zs[0]) - 0.003, zs[0])] + [(r_at(z) + 0.0015, z) for z in zs] + [(r_at(zs[-1]) - 0.003, zs[-1])]
    return M.lathe(prof, sides=20, cap0="fan", cap1="fan").translate(tuple(C0))


def build_body(pal: Palette, rig, lod: int = 0):
    from kit import asset
    if lod:
        # LOD1 <= 2k toon (the sprite budget); the crown petals are sub-pixel at 1080p (art review r2) and go first
        skip = ("blush", "glint", "clapper", "cap_band", "crown")
        b = M.Builder(ASSET, pal, prefix=PREFIX, lod_ratio=0.5, lod_skip=lambda nm: nm in skip)
    else:
        b = M.Builder(ASSET, pal, prefix=PREFIX)
    b.regions(**REGIONS)
    rx, ry, rz = S.BODY_R[0] * U, S.BODY_R[1] * U, S.BODY_R[2] * U
    ball = M.ellipsoid((rx, ry, rz), C0, sides=20, rings=11)       # (art review r2 budget: 22 × 12 before)
    bid = b.add(ball, "core", Bind.rigid("body"), name="ball", hull_pad=True)
    if STAGE > 0:
        b.add(core_band(), "core_band", Bind.rigid("body"), name="core_band", outline=False, covered_by=bid)
    # the flame streaming behind (the sprite's wisp tail): a flame tongue (art review r2: the smooth orange cone read
    # as a duck's foot / beak) — see add_flame
    add_flame(b)
    # soot wisps in the wing slots (modelled at the rest spread / sweep)
    for nm, (anc, side, pair) in S.WINGS.items():
        a = C0 + S.w2b(*anc)
        f = S.wing_frame(*S.wing_rest(pair), side).to_4x4()
        f.translation = a
        wp, wh = wisp_part(pair)
        b.add(wp.transform(f), "smoke", Bind.rigid(nm), name="wisp", hull=wh.transform(f), hull_pad=True,
              sub_regions={i: k for i, k in enumerate(SMOKE_KEYS) if i})
    # tiny arms: two warm nubs reaching forward
    for s in ("l", "r"):
        sh, hand = S.arm_points(s)
        arm = M.capsule(C0 + sh, C0 + hand, 1.15 * U, 0.95 * U, sides=7, cap_rings=2)
        b.add(arm, "core", Bind.rigid(f"arm_{s}"), name=f"arm_{s}", hull_pad=True)
    # the lantern cap on the crown (body) and its bail ring swinging on the sprout bone
    cap, band, fr = lantern_cap()
    cid = b.add(cap, "bronze", Bind.rigid("body"), name="cap", hull_pad=True)
    b.add(band, "bronze_dark", Bind.rigid("body"), name="cap_band", outline=False, covered_by=cid)
    b.add(bail_ring(fr), "bronze", Bind.rigid("sprout"), name="bail", hull_pad=True)
    if STAGE > 0:
        add_crown(b, cid, fr)
    # the tiny bronze bell under the chin
    bell, clap, loop = bell_parts()
    belid = b.add(bell, "bronze", Bind.rigid("body"), name="bell", hull_pad=True)
    b.add(clap, "clapper", Bind.rigid("body"), name="clapper", outline=False, covered_by=belid)
    b.add(loop, "bronze_dark", Bind.rigid("body"), name="bell_loop", hull_pad=True)
    # face: soot-dark eyes with glints (the sprite's eye rig), a small smile, the "o" mouth, a warm blush
    for s in ("l", "r"):
        e, nrm = S.ball_point(S.EYE[s], -0.0025)
        m = S.frame_z(e, nrm, Vector((0, 0, 1)))
        er = S.EYE_R
        # a flat eye stack, 2-5 mm proud of the core (art review r2: the first stack stood 6-10 mm proud and the
        # core's padded ink ballooned 15 mm round the face — black halos in the close-ups, a heavier 1080p median)
        iris = M.ellipsoid((er, er * 1.12, 0.0035), (0, 0, 0.0010), sides=12, rings=5).transform(m)
        b.add(iris, "iris", Bind.rigid(f"eye_{s}"), name=f"iris_{s}", outline=False, covered_by=bid)
        sg = 1.0 if s == "l" else -1.0
        pup = M.ellipsoid((er * 0.52, er * 0.6, 0.0025), (-sg * er * 0.12, -er * 0.08, 0.0030), sides=8,
                          rings=4).transform(m)
        b.add(pup, "pupil", Bind.rigid(f"eye_{s}"), name=f"pupil_{s}", outline=False, covered_by=bid)
        gl = M.ellipsoid((er * 0.32, er * 0.32, 0.0020), (sg * er * 0.30, er * 0.40, 0.0052), sides=6,
                         rings=3).transform(m)
        b.add(gl, "glint", Bind.rigid(f"eye_{s}"), name="glint", outline=False, covered_by=bid)
        ck, cn = S.ball_point(S.CHEEK[s], 0.0005)
        cm = S.frame_z(ck, cn, Vector((0, 0, 1)))
        cheek = M.ellipsoid((1.25 * U, 0.70 * U, 0.0022), (0, 0, 0), sides=8, rings=3).transform(cm)
        b.add(cheek, "blush", Bind.rigid("body"), name="blush", outline=False, covered_by=bid)
    mo, mn = S.ball_point(S.MOUTH_DIR, 0.0)
    mm = S.frame_z(mo, mn, Vector((0, 0, 1)))
    arc = [mm @ Vector((x, -0.004 * (1 - (x / 0.016) ** 2) + 0.002, 0.0015)) for x in (-0.016, -0.008, 0.0, 0.008,
                                                                                         0.016)]
    smile = M.strap(arc, 0.0045, 0.003, up=[mm.to_3x3() @ Vector((0, 0, 1))] * len(arc))
    b.add(smile, "mouth", Bind.rigid("mouth"), name="smile", outline=False, covered_by=bid)
    o_m = M.ellipsoid((0.0085, 0.010, 0.004), (0, -0.002, 0.001), sides=8, rings=3).transform(mm)
    b.cover(bid, o_m)
    o_m.transform(S.collapsed(rig.joints["mouth_o"]))
    b.add(o_m, "mouth_in", Bind.rigid("mouth_o"), name="mouth_o", outline=False)
    body = asset.finish_mesh(b, pal, S.OUTLINE_CLASS, rig=rig, grounding_height=HEIGHT,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    return body


def add_crown(b: M.Builder, cid: int, fr: Matrix) -> None:
    """Evolved looks: five gold flame petals lying up the lantern cap's slope (E1; longer in E2), and in E2 an
    emissive gold halo round the cap's foot — on the cap, inside its padded ink (``covered_by``): standing tongues
    tented the cap's ink into a dark block behind them, and five separately inked tongues read as a black comb at
    1080p (the sprite's crown lesson)."""
    ln = (0.0, 1.9, 2.5)[STAGE] * U
    p0 = Vector((3.05, 0.42))                           # (r, z) on the cap profile, the slope toward (1.35, 2.05)
    d = Vector((-1.70, 1.63)).normalized()
    nrm = Vector((d.y, -d.x))                           # outward normal of the slope in (r, z)
    for i in range(5):
        a = math.tau * i / 5 + 0.3
        ca, sa = math.cos(a), math.sin(a)

        def at(t: float, lift: float) -> Vector:
            q = p0 * U + d * (ln * t) + nrm * lift
            return Vector((ca * q.x, sa * q.x, q.y))
        pts = [at(t, 0.14 * U + 0.06 * U * math.sin(math.pi * t)) for t in (0.0, 0.35, 0.70, 1.0)]
        flat = [(0.20 * U, 0.85 * U), (0.18 * U, 0.75 * U), (0.13 * U, 0.42 * U), (0.05 * U, 0.08 * U)]
        tongue = M.tube(pts, flat, sides=6, up=tuple(Vector((ca * nrm.x, sa * nrm.x, nrm.y))), dome0=0.002,
                        dome1=0.001)
        b.add(tongue.transform(fr), "crown_flame", Bind.rigid("body"), name="crown", outline=False, covered_by=cid)
    if STAGE == 2:
        # up the cap's slope, inside the silhouette: at the cap's widest ring its bloom washed the side ink out
        # (E2 ne cell: 13 % of the rim without a dark pixel)
        ring = M.sweep([Vector((math.cos(t) * 2.66 * U, math.sin(t) * 2.66 * U, 1.0 * U))
                        for t in (k / 18 * math.tau for k in range(18))],
                       M.superellipse(0.24 * U, 0.20 * U, 5), closed_path=True, up=(0, 0, 1))
        b.add(ring.transform(fr), "halo", Bind.rigid("body"), name="halo", outline=False, covered_by=cid)


# ── march tiles (the pet on every ground it walks in Chapter 1) ──────────────────────────────────────────────
TILES = (("GRASS", EM.GRASS_HEX), ("MUD (DARKEST)", EM.DIRT_HEX), ("STONE", EM.STONE_HEX), ("CAMP", EM.CAMP_HEX))


def march_tiles(ms, rv, actions) -> None:
    """``march_tiles.png``: Idle F0 se and Walk F9 sw at W1 1080p 1:1 with the shipped look on each march ground
    value (ash grass, the darkest walkable mud, stone, camp ash) — every cell ink-gated with the rest of the set."""
    import mship
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    cells = []
    review.setup_render((1920, 1080), 8)
    hw = mship._crop_hw(ms)
    zc = ms.height * 0.45
    for lbl, hx in TILES:
        review.stage(blob_radius=ms.blob_radius, ground_hex=hx, grid=False)
        for name, fr, fac in (("Idle", 0, "se"), ("Walk", 9, "sw")):
            rv.facing(fac)
            anim.assign(rig, acts[name])
            anim.set_frame(fr)
            cam = review.game_camera()
            rv.game_outline(1080, Vector((0, 0, zc)))
            c = review.project(cam, Vector((0, 0, zc)))
            hh = hw * 1920 / 1080
            box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
            img, st = rv.game_shot(box, measure=True, tag=f"march_tiles {lbl} {name} F{fr} {fac}")
            review.draw_text(img, 3, 3, f"{lbl} {name} F{fr} {fac}")
            review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
            cells.append(img)
    review.stage(blob_radius=ms.blob_radius)
    rv._save("march_tiles.png", review.grid(cells, 4))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)


# ── build / ship ──────────────────────────────────────────────────────────────────────────────────────────
class _A:
    def __init__(self, rig):
        self.rig = rig


def build_all():
    scene.reset()
    pal = Palette(S.FAMILY)
    rig = S.build_rig()
    for sk in rig.sockets:                       # fx_tail at the flame's tip (see TAIL_GLOW)
        if sk["name"] == "fx_tail":
            sk["pos"] = TAIL_TIP.copy()
    body = build_body(pal, rig)
    return pal, rig, body


PORTRAIT = "T_UI_Portrait_Pet_Sprite"      # the runtime's name for pet_sprite (AbyssUiContext::PetPortrait)
# Art review r2: the seed sprite's portrait (Idle F10, yaw −110°) caught the wisps mid-flap — two grey blades and a
# wisp tip standing above the cap like a horn — and hid the flame behind the core. The Lanternwisp's own: the Idle
# frame whose wisp tips sit lowest under the cap's crown, turned toward profile so the flame streams out behind the
# core, the whole creature framed (core, cap, bell, flame).
PORTRAIT_FACING = -95.0
PORTRAIT_PITCH = -20.0
PORTRAIT_FILL = 0.86


def _bone_point(rig, bone: str, rest: Vector) -> Vector:
    b = rig.obj.data.bones[bone]
    pb = rig.obj.pose.bones[bone]
    return rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(rest)))


def wisp_tips_rest() -> dict[str, Vector]:
    """Rest-pose tip of each wisp (its bone's rigid frame), for the portrait's frame choice."""
    out = {}
    for nm, (anc, side, pair) in S.WINGS.items():
        a = C0 + S.w2b(*anc)
        f = S.wing_frame(*S.wing_rest(pair), side)
        L, W = S.WING_L[pair - 1] * U, S.WING_W[pair - 1] * U * WISP_W
        xr, c = _wisp_axis(1.0, W)
        out[nm] = a + f @ Vector((L * xr, c, 0.026))
    return out


def cap_crown_rest() -> Vector:
    top, n = S.ball_point(S.SPROUT_DIR, 0.0)
    return top + n * CAP_TOP_U * U


def portrait_frame(rig, act, frames) -> tuple[int, float]:
    """(frame, margin m) of the Idle frame whose highest wisp tip sits furthest under the cap's crown."""
    tips = wisp_tips_rest()
    crown = cap_crown_rest()
    anim.assign(rig, act)
    best = None
    for fr in frames:
        anim.set_frame(fr)
        bpy.context.view_layer.update()
        top = max(_bone_point(rig, nm, p).z for nm, p in tips.items())
        cz = _bone_point(rig, "body", crown).z
        if best is None or cz - top > best[1]:
            best = (fr, cz - top)
    return best


def portrait_rgba(ms, rv, act, frame: int, facing: float, pitch: float, fill: float):
    import mship
    rig = ms.A.rig
    anim.assign(rig, act)
    anim.set_frame(frame)
    rv.facing(facing)
    bpy.context.view_layer.update()
    res = (512, 512)
    review.hide_stage(True)
    review.setup_render(res, 16)
    vis = [ms.body]
    pts = review.mesh_points(vis)
    cam = review.frame_points(pts, res, fov_h=24, pitch_ue=pitch, fill=fill, include_ground=False)
    for m in vis:
        outline.remove_preview(m)
    rgba = review.ink_render(vis, res, silhouette_px=mship.PORTRAIT_INK_PX, interior_px=2.0, depth_step=0.012)
    cards = S.glows(rig, aura=True)(cam, res)
    halo = np.zeros_like(rgba)                       # the aura under the creature (a halo card behind the body)
    for g in cards:
        if g.get("behind"):
            review.composite_glow(halo, g["center_px"], g["radius_px"], g["color"], g["alpha"], core=0.25)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    ha = halo[:, :, 3:4].astype(np.float32) / 255.0
    out_a = a + ha * (1 - a)
    rgb = (rgba[:, :, :3] * a + halo[:, :, :3] * ha * (1 - a)) / np.maximum(out_a, 1e-6)
    review.hide_stage(False)
    return np.concatenate([rgb, out_a * 255.0], axis=2).clip(0, 255).astype(np.uint8)


def _comp(rgba, label: str):
    bg = np.array([59, 53, 70], np.float32)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    comp = (rgba[:, :, :3].astype(np.float32) * a + bg * (1 - a) + 0.5).astype(np.uint8)
    review.draw_text(comp, 4, 4, label)
    return comp


def portrait(ms, rv, actions) -> str:
    """``T_UI_Portrait_Pet_Sprite`` (pets panel, ``AbyssUiContext::PetPortrait``): 512² RGBA, toon + screen-space
    ink (``review.ink_render``), the body aura composited under the creature (no bloom on a UI texture); the
    Idle frame with the lowest wisp tips (``portrait_frame``), turned toward profile; ``portrait.png`` /
    ``portrait_zoom2x.png`` / ``portrait_medallion.png`` (the UI medallions of ``icons/portraits.py`` for this one
    texture) in the preview set."""
    from kit import pngio
    acts = dict(zip([c.name for c in ms.clips], actions))
    idle = acts["Idle"]
    n = int(idle.frame_range[1] - idle.frame_range[0])
    fr, margin = portrait_frame(ms.A.rig, idle, range(0, n, 2))
    rgba = portrait_rgba(ms, rv, idle, fr, PORTRAIT_FACING, PORTRAIT_PITCH, PORTRAIT_FILL)
    outp = paths.export_root() / "Portraits" / f"{PORTRAIT}.png"
    outp.parent.mkdir(parents=True, exist_ok=True)
    pngio.write_png(outp, rgba)
    rv._save("portrait.png", _comp(rgba, f"{PORTRAIT.upper()}  IDLE F{fr}  WISP TIPS {margin * 100:.1f}CM UNDER THE CAP"))
    comp = _comp(rgba, "")
    ys, xs = np.nonzero(rgba[:, :, 3] > 128)
    cy, cx = int((ys.min() + ys.max()) / 2), int((xs.min() + xs.max()) / 2)
    y0, x0 = max(0, min(512 - 256, cy - 128)), max(0, min(512 - 256, cx - 128))
    z = comp[y0:y0 + 256, x0:x0 + 256].repeat(2, 0).repeat(2, 1)
    review.draw_text(z, 4, 4, "PORTRAIT 2X ZOOM")
    rv._save("portrait_zoom2x.png", z)
    sys.path.insert(0, str(HERE.parent / "icons"))
    import portraits as PT                 # the medallion renderer of the UI sheets (one row: this texture)
    rv._save("portrait_medallion.png", PT.medallion_sheet([(PORTRAIT, rgba)]))
    ms.portrait_check = PT.check(rgba)
    ms.portrait_frame = fr
    anim.assign(ms.A.rig, idle)
    anim.set_frame(0)
    rv.facing("se")
    print(f"[portrait] {outp}  Idle F{fr}, wisp tips {margin * 100:.1f} cm under the cap crown, check {ms.portrait_check}")
    return f"Portraits/{PORTRAIT}.png"


def supersede_seed_sprite(check: dict | None = None) -> None:
    """The seed-sprite entries keep their files but no longer claim the ids; the portrait's ui entry names its new
    owner (and carries ``icons/portraits.check`` of the new texture)."""
    import iconkit as K                    # the manifest writer that also merges the ``ui`` section
    man = K.IconManifest()
    e = man.data["assets"].get(SUPERSEDES)
    if e is not None:
        e = dict(e)
        e["gameIds"] = []
        e["supersededBy"] = ASSET
        e["supersededNote"] = ("dark remake A11: pet_sprite is re-dressed as 灯魂 / the Lanternwisp everywhere "
                               "(chapter1.md §14); the seed sprite's files stay, unreferenced")
        e.pop("portrait", None)
        man.data["assets"][SUPERSEDES] = e
    for gid in GAME_IDS:
        man.data["gameIds"][gid] = ASSET
    if STAGE == 0:
        ui = dict(man.data.get("ui", {}).get(PORTRAIT, {}))
        if ui:
            ui["owner"] = ASSET
            ui["artIds"] = ["pet_sprite"]
            ui["previews"] = [f"{PREVIEW_DIR}/portrait.png", f"{PREVIEW_DIR}/portrait_zoom2x.png",
                              f"{PREVIEW_DIR}/portrait_medallion.png"]
            ui["note"] = ("the Lanternwisp (dark remake A11) under the runtime name for pet_sprite; "
                          f"{PREVIEW_DIR}/portrait_medallion.png draws this texture as the UI medallions "
                          "(icons/portraits.medallion_sheet: conversation, say, well, card); the shared sheet "
                          "portraits/medallions_*.png still shows the seed sprite until icons/portraits.py runs "
                          "(it rewrites every portrait entry: schedule it before UI sign-off)")
            if check:
                ui["check"] = check
            else:
                ui.pop("check", None)
            man.data["ui"][PORTRAIT] = ui
    man.save()


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    sys.path.insert(0, str(HERE.parent / "icons"))
    pal, rig, body = build_all()
    body1 = build_body(pal, rig, lod=1)
    A = S.SpriteAnims(rig)
    cs = A.all_clips()
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW_DIR, pal=pal, A=_A(rig), body=body, items=[], clips=cs,
        outline_class=S.OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=round(8 * U * 1.05, 3),
        height=HEIGHT, display=DISPLAY, fx=FX, glows=S.glows(rig),
        qa_limits={"ground": -0.5}, category=S.CATEGORY,
        game_keys=[("Idle", 0, "se"), ("Idle", 24, "se"), ("Walk", 9, "se"), ("Walk", 27, "se"),
                   ("Attack01", 8, "se"), ("Attack01", 15, "se"), ("Cast01", 12, "se"), ("Hurt", 0, "se")],
        contact_keys=[("Attack01", 15, "se"), ("Attack01", 15, "side"), ("Cast01", 12, "se"), ("Cast01", 12, "front")],
        close_keys=[("Idle", 0, "front"), ("Idle", 45, "se"), ("Walk", 9, "side"), ("Attack01", 5, "se"),
                    ("Attack01", 15, "se"), ("Cast01", 12, "se"), ("Cast01", 12, "back"), ("Hurt", 0, "se"),
                    ("Hurt", 0, "ne"), ("Walk", 20, "back")],
        back_clips=("Attack01", "Cast01"),
        skeleton_bones_required=("root", "body", "tail_01", "tail_02", "tail_03", "tail_04", "wing_l_01", "wing_l_02",
                                 "wing_r_01", "wing_r_02", "arm_l", "arm_r", "sprout", "eye_l", "eye_r", "mouth",
                                 "mouth_o"),
        extra={"refSpeedCmS": S.REF_SPEED * 100.0, "hoverCm": round(C0.z * 100, 1), "flyer": True, "stage": STAGE,
               "reskinOf": SUPERSEDES, "family": "pets.lanternwisp",
               "remake": "dark remake A11 (焚境 · The Burning March): pet_sprite re-dressed as 灯魂 / the Lanternwisp",
               **({"evolvedLooks": {"beast_pet_sprite_e1": "SK_Pet_Lanternwisp_E1",
                                    "beast_pet_sprite_e2": "SK_Pet_Lanternwisp_E2"}} if STAGE == 0 else
                  {"evolvedFrom": "SK_Pet_Lanternwisp" if STAGE == 1 else "SK_Pet_Lanternwisp_E1",
                   "sharesClipsWith": "SK_Pet_Sprite / SK_Pet_Lanternwisp (same SKEL_Pet_Sprite bones and clip set)"}),
               "notes": "SKEL_Pet_Sprite re-skin: the sprite's rig and clips (sprite.SpriteAnims) unchanged; body = the "
                        "lantern-gold core, wing_* = soot wisps, tail_* = the flame, sprout = the lantern's bail "
                        "ring (the cap sits on body), the bell under the chin on body. No Death: the exhausted pet "
                        "idles at 0.6x. Contact 250 / Release 200 = PET_ATTACK_CONTACT / PET_CAST_RELEASE_FRAME."},
        lod_note="Pet companion budget (the sprite's): LOD0 <= 4k toon tris, LOD1 <= 2k, 17 bones.",
        lod1=body1, portrait=portrait if STAGE == 0 else None, extra_previews=march_tiles,
        outline_px=OUTLINE_PX, outline_px_note=OUTLINE_PX_NOTE,
    )
    if not issubclass(mship.MReview, S.BehindGlow):
        mship.MReview = type("PetReview", (S.BehindGlow, mship.MReview), {})
    _gate_for = mship.gate_for
    mship.gate_for = lambda cls, px: dict(_gate_for(cls, px), minPx=max(GATE_PX, _gate_for(cls, px)["minPx"]))
    res = mship.ship(ms, previews=previews, verify=verify)
    supersede_seed_sprite(getattr(ms, "portrait_check", None))
    return mship.main_result(res)


def quick(out: Path, clip: str | None = None, frame: int = 0) -> None:
    pal, rig, body = build_all()
    A = S.SpriteAnims(rig)
    out.mkdir(parents=True, exist_ok=True)
    if clip:
        c = next(c for c in A.all_clips() if c.name == clip)
        anim.bake_basis(rig, c, TOKEN, c.basis_fn)
        anim.set_frame(frame)
    rv = type("PetQuickReview", (S.BehindGlow, review.Review), {})(
        ASSET, rig.obj, [body], HEIGHT, blob_radius=8 * U, out_dir=out, game_outline_px=OUTLINE_PX,
        outline_class=S.OUTLINE_CLASS, glows=S.glows(rig))
    rv.gate = dict(rv.gate, minPx=GATE_PX)
    rv.closeup("se", pitch=-25.0)
    (out / "closeup.png").rename(out / "closeup_se.png")
    rv.closeup("front", pitch=-20.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    for fac in ("ne", "side"):
        rv.closeup(fac, pitch=-25.0)
        (out / "closeup.png").rename(out / f"closeup_{fac}.png")
    rv.turnaround()
    rv.game_view()
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")
    v = rv.ink_verdict()
    print(f"  ink {'PASS' if v['pass'] else 'FAIL'} rim {v['minFracRim']:.3f} dark {v['minFracDark']:.3f} median "
          f"{v['medianRimPx']} px")
    print(f"  tris toon {M.tri_count(body, 0)} hull {M.tri_count(body, 1)}  height {HEIGHT * 100:.1f} cm")


def quick_portrait(out: Path, facings=(-110.0, -95.0, -80.0, -68.0)) -> None:
    """Portrait candidates (scratch only, nothing shipped): the chosen Idle frame at a few yaws."""
    from types import SimpleNamespace
    pal, rig, body = build_all()
    A = S.SpriteAnims(rig)
    c = next(c for c in A.all_clips() if c.name == "Idle")
    act = anim.bake_basis(rig, c, TOKEN, c.basis_fn)
    out.mkdir(parents=True, exist_ok=True)
    rv = type("PetQuickReview", (S.BehindGlow, review.Review), {})(
        ASSET, rig.obj, [body], HEIGHT, blob_radius=8 * U, out_dir=out, game_outline_px=OUTLINE_PX,
        outline_class=S.OUTLINE_CLASS, glows=S.glows(rig))
    ms = SimpleNamespace(A=_A(rig), body=body, clips=[c])
    n = int(act.frame_range[1] - act.frame_range[0])
    fr, margin = portrait_frame(rig, act, range(0, n, 2))
    print(f"  portrait frame Idle F{fr}: wisp tips {margin * 100:.1f} cm under the cap crown")
    cells = [_comp(portrait_rgba(ms, rv, act, fr, f, PORTRAIT_PITCH, PORTRAIT_FILL), f"YAW {f:.0f}") for f in facings]
    rv._save("portrait_candidates.png", review.grid(cells, 2))
    sys.path.insert(0, str(HERE.parent / "icons"))
    import portraits as PT
    rgba = portrait_rgba(ms, rv, act, fr, PORTRAIT_FACING, PORTRAIT_PITCH, PORTRAIT_FILL)
    rv._save("portrait_medallion.png", PT.medallion_sheet([(PORTRAIT, rgba)]))
    print(f"  check {PT.check(rgba)}")


def anim_quick(out: Path, names) -> None:
    import mship
    pal, rig, body = build_all()
    A = S.SpriteAnims(rig)
    cs = [c for c in A.all_clips() if not names or c.name in names]
    mship.anim_review(rig, [body], [], cs, TOKEN, out, HEIGHT, S.OUTLINE_CLASS, blob_radius=8 * U)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = Path(os.environ.get("AF_SCRATCH_ROOT", tempfile.gettempdir())) / "abyss_art_pet" / PREVIEW_DIR
    if "--portrait" in args:
        quick_portrait(scratch / "portrait")
    elif "--quick" in args:
        k = args.index("--quick")
        rest = [a for a in args[k + 1:] if not a.startswith("--")]
        quick(scratch, rest[0] if rest else None, int(rest[1]) if len(rest) > 1 else 0)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        anim_quick(scratch / "anims", names)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))
