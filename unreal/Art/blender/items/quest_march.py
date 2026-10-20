"""March quest items (dark remake 焚境 · The Burning March, package A11; Docs/dark-remake/chapter1.md §6 S1-S3,
implementation-plan.md §5): the three Chapter-1 quest pickups and the barrowmoss gather node.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/quest_march.py   # ship all (~4 min)
        --only ichor,moss,tags,node     ship a subset (the family sheets need all four)
        --quick [ichor,moss,tags,node]  iteration: studio sheet + W1 1080p cells → $AF_SCRATCH_ROOT/items/quest_march
        --no-previews · --no-verify

| Asset | Game ids | Quest (chapter1.md) |
|---|---|---|
| ``SM_Pickup_Quest_Ichor`` | ``quest_item_ichor`` | S1 ``q_rf_black_blood``: ``mat_breach_ichor`` 渊血样本, dropped by ``breachclot`` |
| ``SM_Pickup_Quest_Moss`` | ``quest_item_moss`` | S2 ``q_rf_barrowmoss``: ``mat_barrowmoss`` 苦冢苔 (the picked bundle) |
| ``SM_Pickup_Quest_Tags`` | ``quest_item_tags`` | S3 ``q_rf_tag_thief``: ``mat_tag_pouch`` 名牌袋, dropped by ``hunt_dreck`` |
| ``SM_Prop_Gather_Barrowmoss`` | ``gather_moss``, ``gather_herb@march`` | S2 gather node (7 spots round (22,26) r9) |

The runtime resolves a quest node ``quest_node:<itemKind>`` through ``gather_<kind>`` → ``quest_item_<kind>`` →
``<kind>`` → ``gather_node`` (``AbyssWorldBuilder``): the S2 spots (itemKind ``moss``) find the growing node under
``gather_moss``; the drops (``ichor``, ``tags``) find their pickups. ``gather_herb@march`` keeps A1's planned slot
(``march_palettes.VARIANTS``): any herb-kind gather in a march zone shows barrowmoss, never an emerald herb.

**Pickups** follow the ground-loot conventions of ``loot.py`` (palette ``Pickups``, appended; category ``Pickups``;
interactive ink 2.0 px line tone; pivot at the ground contact; readable from any spawn yaw; ``loot`` record with
the ``beam`` / ``label`` / ``glint`` sockets) and keep the loot side's warmth rules: no ember is needed, the breach
violet appears only where the item *is* breach-blood (the ichor's meniscus and sparks: the enemy's colour on its
blood). Every pickup is judged on the march ground (``envkit_march.use_march_stage``: ash grass #4C4940) and on
the darkest walkable tile (mud #3A332D) — ``Previews/quest_march/march_tiles.png``.

* **Ichor** — Mother Marl's sample phial (「别用手碰。玛勒会教你。」): a squat flat-bottomed phial of near-black
  ichor with a violet meniscus and drifting violet sparks (story-bible: "cold black ichor ... violet sheen and
  drifting sparks"), the lower half wrapped in undyed linen tied with twine (handled through cloth), a cork, a
  parchment label at the neck, a tar drip down the shoulder.
* **Moss** — a handful of grave moss bound at one end like a sheaf (the rite burns it over a grave): a ragged moss
  head (dull barrowmoss with outlined tufts breaking its outline, lighter tufts on the crown and six large pale spore
  caps, the A1 gather hint #C8D49A e .35), one binding of two turns of pale twine at the neck, and a wide fan of torn
  root out of the bound end. Art review r2: the first roll (an even cocoon with two twine bands and pin-head caps)
  read as a grub or larva at game scale, in a zone full of gnawers and corpses.
* **Tags** — Dreck's pouch: a dark, soot-stained leather drawstring pouch on its side, its mouth open, tarnished
  silver name-tags spilled in front of it on pale cords, stamped, two blackened with soot tarnish. Art review r2: the
  first pass (tan pouch, pale-gold "bronze" tags with a #D2AE68 rim) read as a coin purse spilling gold next to the
  gold piles (#E8C060) and SM_Loot_Bag; the tags now match Dreck the Silverpicker's own bandolier (Monsters_March
  ``dreck.tags`` #A8ACB0: a mid-grey face, a pale cool rim, dark stamps) and S3's melted silver.
* **Gather node** (``Props_March``, A1's ``barrowmoss.*`` swatches + ``camp.ash``): bumpy barrowmoss cushions
  (lighter tufts on their crowns, A1's pale ``barrowmoss.glow`` spore caps as the gather hint, e .35, not ember) over
  a low clod of ash earth — one self-contained clump, because it bobs and rocks like every quest node
  (``quest_node:<kind>`` spawns with ``EAbyssPropStyle::Pickup``: 6 px / 900 ms yoyo, rock ±8°). Art review r2: the
  broken grave marker the first pass leaned it against floated and wobbled with it (a haunted headstone in the
  Burial Trenches): it is gone from the node — a marker belongs to the static Burial Trenches decor. Each cushion
  carries its own padded ink and the baked hull hugs the ground (``envkit.ground_hull``) so the ink makes no dark
  tray under the cushions; the thin spore stalks were dropped: a 3 px stalk under a 2 px line is all ink.
"""
from __future__ import annotations

import argparse
import math
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parent
for _p in (HERE, BLENDER, BLENDER / "environment", BLENDER / "march"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import itemkit as IK  # noqa: E402  (kit / heroes on sys.path)
import loot as L  # noqa: E402

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import envkit as E  # noqa: E402
import envkit_march as EM  # noqa: E402  (every review stage of this process on the march ground)
from kit import asset, export, mesh as M, outline, paths, review, scene, theme  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

TAU = math.tau
CLS = L.CLS
CATEGORY = L.CATEGORY
FAMILY = L.FAMILY
PREFIX = "qmarch"                      # Pickups swatch names qmarch.<region> (appended to the registry)
R = L.R
OUT = "quest_march"                    # Previews/quest_march/: family sheets


# ── regions (Pickups, appended) ──────────────────────────────────────────────────────────────────────────
GLINT = R("#FFFFFF", 0.0, 0.0, e=0.7)
REGIONS = dict(
    # ichor phial: near-black breach-blood (violet-tinted, a generous light band so its lit side shows on the mud),
    # pale smoky glass above it, a violet meniscus band and violet sparks (the enemy's colour on its own blood)
    ichor=R("#150F17", .40, .42),
    ichor_glass=R("#AFA8B8", .30, .50),
    ichor_sheen=R("#A63CC8", 0.0, 0.0, e=0.45),
    ichor_spark=R("#D08CFF", 0.0, 0.0, e=1.0),
    glint=GLINT,
    linen=R("#D9CFB6", .36, .42),
    linen_fold=R("#A79A80", .38, .30),
    twine=R("#E6D7B4", .32, .45),
    cork=R("#B07A44", .42, .30),
    cork_line=R("#7A4E26", .35, .2),
    label=R("#ECE2C8", .33, .40),
    label_ink=R("#3A2E2E", .30, .10),
    # barrowmoss bundle (A1 barrowmoss.* values: dull grave moss, a lit green, the pale gather hint)
    moss=R("#5E6A4A", .42, .36),
    moss_lit=R("#8A9466", .40, .36),
    moss_tip=R("#C8D49A", .20, .30, e=0.35),
    moss_root=R("#A89A7C", .40, .34),
    # first-pass tag pouch (tan leather, pale-gold bronze tags; retired by art review r2, kept registered: the
    # registry is append-only)
    leather=R("#76503A", .44, .34),
    leather_lit=R("#9C7052", .42, .36),
    leather_in=R("#24180F", .30, .10),
    drawcord=R("#D2C09C", .36, .40),
    bronze=R("#A07C3C", .46, .40),
    bronze_rim=R("#D2AE68", .36, .50),
    stamp=R("#3E2E1A", .30, .12),
    patina=R("#6A9A80", .40, .32),
    thong=R("#BBA27A", .38, .36),          # the tags' cords (pale: tags on strings, not coins)
    # tag pouch, art review r2 (appended): dark soot-brown leather, not the loot bag's tan / cream; tarnished silver
    # tags in the dreck.tags #A8ACB0 family (Dreck the Silverpicker's bandolier): a mid-grey face, a pale cool rim (no
    # gold), dark cool stamps, soot tarnish spots
    pouch=R("#4A3D35", .44, .34),
    pouch_lit=R("#66574B", .42, .36),
    silver=R("#7E8389", .40, .42),
    silver_rim=R("#B4BCC4", .30, .46),
    silver_stamp=R("#26282C", .30, .10),
    tarnish=R("#3E3A38", .40, .22),
)
# darker hull line tones where a light part meets the silhouette (the loot.HULL_O rule: hull p95 lum ≤ 64)
HULL_O = {"linen": 0.6, "twine": 0.6, "label": 0.6, "moss_tip": 0.5, "moss_lit": 0.8, "moss_root": 0.7,
          "drawcord": 0.7, "thong": 0.7, "bronze": 0.7, "patina": 0.7, "silver": 0.7, "ichor_glass": 0.6,
          "ichor_sheen": 0.5, "ichor_spark": 0.5, "glint": 0.5}
# thin layered parts (cords, root fibres): thinner baked hulls (loot.HULL_SCALE rule)
HULL_SCALE = {"SM_Pickup_Quest_Tags": {"thong_hull": 0.75, "cordend_hull": 0.75},
              "SM_Pickup_Quest_Moss": {"root_hull": 0.75}}


def class_regions() -> dict:
    from dataclasses import replace
    out = outline.ink_regions(REGIONS, CLS)
    for k, o in HULL_O.items():
        out[k] = replace(out[k], o=o)
    return out


def palette() -> Palette:
    pal = L.palette()                   # every shipped Pickups name stays where it is (append-only)
    for k, r in class_regions().items():
        pal.add(f"{PREFIX}.{k}", r)
    return pal


def builder(name: str, pal: Palette) -> M.Builder:
    b = M.Builder(name, pal, prefix=PREFIX)
    b.regions(**class_regions())
    return b


# ── ichor phial ───────────────────────────────────────────────────────────────────────────────────────────
PHIAL = [(0.046, 0.0), (0.056, 0.006), (0.061, 0.022), (0.062, 0.070), (0.060, 0.112), (0.054, 0.130),
         (0.040, 0.150), (0.027, 0.163), (0.023, 0.176), (0.023, 0.198)]
PHIAL_LEVEL, PHIAL_LIP, PHIAL_CORK = 0.138, 0.198, 0.044
WRAP_Z = (0.008, 0.056)


def _rad(prof, z):
    return float(np.interp(z, [p[1] for p in prof], [p[0] for p in prof]))


def build_ichor(pal: Palette):
    """Marl's sample phial of breach-blood (S1 ``mat_breach_ichor``)."""
    name = "SM_Pickup_Quest_Ichor"
    b = builder(name, pal)
    lo, hi, rl = L._split_profile(PHIAL, PHIAL_LEVEL)
    full = M.lathe(PHIAL + [(0.0, PHIAL_LIP + 0.002)], sides=20, cap0="fan", cap1=None)
    lid = b.add(M.lathe(lo, sides=20, cap0="fan", cap1="ngon"), "ichor", name="ichor", hull=full, hull_pad=True)
    b.add(M.lathe(hi, sides=20, cap0=None, cap1="fan"), "ichor_glass", name="glass", outline=False, covered_by=lid)
    # violet meniscus band (the breach sheen) at the level line
    b.add(M.lathe([(rl - 0.002, PHIAL_LEVEL - 0.0045), (rl + 0.0016, PHIAL_LEVEL - 0.004),
                   (rl + 0.0016, PHIAL_LEVEL + 0.002), (rl - 0.002, PHIAL_LEVEL + 0.0025)], sides=20, cap0="fan",
                  cap1="fan"), "ichor_sheen", name="meniscus", outline=False, covered_by=lid)
    # drifting violet sparks just under the glass, all round (any spawn yaw shows two or three)
    rng = scene.rng(name, "sparks")
    for k in range(5):
        a = TAU * k / 5 + rng.uniform(-0.3, 0.3)
        z = rng.uniform(0.072, 0.120)
        r_ = _rad(PHIAL, z) + 0.0002
        rr = rng.uniform(0.0030, 0.0042)
        b.add(IK.ell((rr, rr, rr * 1.15), (r_ * math.cos(a), r_ * math.sin(a), z), sides=8, rings=5),
              "ichor_spark", name="spark", outline=False, covered_by=lid)
    # white glass glints on the black (the potion streaks, on the band between the wrap and the level)
    for ph in L.GLINT_PHASES:
        pts, nrm = [], []
        for i in range(6):
            t = i / 5
            z = 0.066 + (0.126 - 0.066) * t
            a = ph - 0.15 * math.sin(math.pi * t)
            r_ = _rad(PHIAL, z)
            pts.append(Vector((r_ * math.cos(a), r_ * math.sin(a), z)))
            nrm.append(Vector((math.cos(a), math.sin(a), 0.0)))
        b.add(IK.surface_strip(pts, nrm, 0.0075, lift=0.0010, depth=0.003), "glint", name="glint", outline=False,
              covered_by=lid)
        z = 0.146
        a = ph + 0.25
        r_ = _rad(PHIAL, z)
        b.add(IK.ell((0.005, 0.005, 0.005), (r_ * math.cos(a), r_ * math.sin(a), z), sides=8, rings=5), "glint",
              name="glint_dot", outline=False, covered_by=lid)
    # undyed linen wrap round the lower half (handled through cloth), its overlap fold and two turns of twine
    n = 24
    rings = []
    for z in np.linspace(WRAP_Z[0], WRAP_Z[1], 5):
        ring = []
        for i in range(n):
            a = TAU * i / n
            top = (z - WRAP_Z[0]) / (WRAP_Z[1] - WRAP_Z[0])
            zz = z + top * (0.005 * math.sin(3 * a + 1.0) + 0.003 * math.sin(7 * a))
            r_ = _rad(PHIAL, zz) + 0.0042 + 0.0012 * math.sin(5 * a + 2.0 * top)
            ring.append((r_ * math.cos(a), r_ * math.sin(a), zz))
        rings.append(ring)
    wrap = IK.loft_rings(rings, cap0=None, cap1=None)
    b.add(wrap, "linen", name="wrap", outline=False, covered_by=lid)
    fold_a = 2.2
    pts = [Vector(((_rad(PHIAL, z) + 0.0056) * math.cos(fold_a + 0.02 * z), (_rad(PHIAL, z) + 0.0056)
                   * math.sin(fold_a + 0.02 * z), z)) for z in np.linspace(WRAP_Z[0] + 0.004, WRAP_Z[1] - 0.004, 5)]
    nrm = [Vector((math.cos(fold_a), math.sin(fold_a), 0.0))] * len(pts)
    b.add(IK.surface_strip(pts, nrm, 0.010, lift=0.0006, depth=0.003), "linen_fold", name="fold", outline=False,
          covered_by=lid)
    for zt in (0.034,):
        b.add(L.closed_tube(Vector((0, 0, zt)), _rad(PHIAL, zt) + 0.0058, 0.0030, n=22, sides=6,
                          z_wave=lambda a: 0.0025 * math.sin(a + 0.7)), "twine", name="twine", outline=False,
              covered_by=lid)
    # neck: lip ring, cork, a twine binding over the cork
    b.add(M.lathe([(0.021, PHIAL_LIP - 0.008), (0.031, PHIAL_LIP - 0.005), (0.032, PHIAL_LIP + 0.004),
                   (0.021, PHIAL_LIP + 0.008)], sides=14, cap0="fan", cap1="fan").auto_sharp(50), "ichor_glass",
          name="lip", outline=False, covered_by=lid)
    ck = M.lathe([(0.0195, PHIAL_LIP - 0.004), (0.0215, PHIAL_LIP + 0.010), (0.0222, PHIAL_LIP + PHIAL_CORK * 0.62),
                  (0.0200, PHIAL_LIP + PHIAL_CORK * 0.92), (0.0090, PHIAL_LIP + PHIAL_CORK),
                  (0.0006, PHIAL_LIP + PHIAL_CORK + 0.002)], sides=12, cap0="fan", cap1="fan")
    cid = b.add(ck, "cork", name="cork", hull_pad=True)
    b.add(IK.ring_y(0.0, 0.0215, 0.0055, 0.004, sides=12).rotate("x", 90)
          .translate((0, 0, PHIAL_LIP + PHIAL_CORK * 0.45)), "cork_line", name="cork_line", outline=False,
          covered_by=cid)
    b.add(L.closed_tube(Vector((0, 0, PHIAL_LIP - 0.0125)), 0.0255, 0.0032, n=16, sides=6), "twine",
          name="neck_twine", outline=False, covered_by=lid)
    # parchment label hanging from the neck twine against the shoulder (Marl's mark: two ink strokes)
    la = -0.55
    zc = 0.148
    rc = _rad(PHIAL, zc) + 0.004
    d = Vector((math.cos(la), math.sin(la), 0.0))
    up = Vector((0.0, 0.0, 1.0))
    # the shoulder slopes ~40° here: lay the label on it
    nrm = (d + up * 0.85).normalized()
    side = up.cross(d).normalized()
    tang = nrm.cross(side).normalized()
    fr = Matrix((side, tang, nrm)).transposed().to_4x4()
    fr.translation = Vector((d.x * rc, d.y * rc, zc))
    lab = M.box((0.044, 0.030, 0.0035), (0, 0, 0.0018), bevel=0.0012).transform(fr)
    b.add(lab, "label", name="label", outline=False, covered_by=lid)
    for k, (w, dy) in enumerate(((0.028, 0.006), (0.020, -0.005))):
        pts = [fr @ Vector((-w / 2 + w * t, dy + 0.0015 * math.sin(9 * t + k), 0.0038)) for t in np.linspace(0, 1, 5)]
        b.add(IK.surface_strip(pts, [nrm] * len(pts), 0.0028, lift=0.0004, depth=0.0015), "label_ink",
              name="label_ink", outline=False, covered_by=lid)
    # tar drip from the lip down the shoulder (it seeps through the cork)
    da = 0.95
    dpts = []
    for z in (PHIAL_LIP - 0.004, 0.172, 0.158, 0.146, 0.136):
        r_ = _rad(PHIAL, z) + (0.0072 if z > 0.17 else 0.0028)
        dpts.append(Vector((r_ * math.cos(da), r_ * math.sin(da), z)))
    b.add(M.tube(dpts, [0.0034, 0.0040, 0.0048, 0.0058, 0.0066], sides=8, dome0=0.002, dome1=0.006), "ichor",
          name="drip", outline=False, covered_by=lid)
    obj = asset.finish_mesh(b, pal, CLS)
    tz = L.top_z(obj)
    return obj, L.sockets(tz, (0.0, 0.0, 0.10)), {
        "kind": "quest", "itemKind": "ichor", "questItem": "mat_breach_ichor",
        "look": "Mother Marl's sample phial of breach-blood: black ichor, violet meniscus and sparks, linen wrap"}


# ── barrowmoss bundle ─────────────────────────────────────────────────────────────────────────────────────
def _noise(a: float, x: float, seed: float) -> float:
    return (0.55 * math.sin(3 * a + 7.1 * x + seed) + 0.30 * math.sin(5 * a - 11.3 * x + 2 * seed)
            + 0.15 * math.sin(9 * a + 23.0 * x + 3 * seed))


# The bundle's envelope along its length (u 0 → 1, −X → +X): a fat ragged head, the twine-bound neck, a short
# root stub flaring a little before the torn roots (art review r2: the even cocoon with two bands read as a grub)
MOSS_ENV = ((0.0, 0.32), (0.05, 0.80), (0.20, 1.0), (0.44, 0.97), (0.62, 0.78), (0.74, 0.56), (0.86, 0.60),
            (1.0, 0.42))
MOSS_LEN, MOSS_RY, MOSS_RZ, MOSS_Z0 = 0.250, 0.084, 0.060, 0.060
MOSS_BIND = 0.060                      # x of the twine binding (u ≈ .74)


def _moss_env(u: float) -> float:
    return float(np.interp(u, [p[0] for p in MOSS_ENV], [p[1] for p in MOSS_ENV]))


def _moss_amp(u: float) -> float:
    """Lumpy noise along the bundle: ragged over the head, smooth at the bound neck (the twine sits on it)."""
    return 0.18 - 0.15 * math.exp(-((u - 0.74) / 0.08) ** 2)


def _moss_top(x: float, y: float) -> float:
    """Height of the bundle's crown at (x, y) (the section ellipse without the noise)."""
    k = _moss_env((x + MOSS_LEN / 2) / MOSS_LEN)
    t = min(1.0, abs(y) / max(MOSS_RY * k, 1e-6))
    return MOSS_Z0 + MOSS_RZ * k * math.sqrt(max(0.0, 1.0 - t * t))


def build_moss(pal: Palette):
    """A handful of barrowmoss bound at one end (S2 ``mat_barrowmoss``)."""
    name = "SM_Pickup_Quest_Moss"
    b = builder(name, pal)
    rng = scene.rng(name, "tufts")
    roll = _roll_xz(MOSS_LEN, MOSS_RY, MOSS_RZ, MOSS_Z0, env=_moss_env, amp=_moss_amp, seed=1.3)
    proxy = _roll_xz(MOSS_LEN, MOSS_RY, MOSS_RZ, MOSS_Z0, env=_moss_env, amp=_moss_amp, seed=1.3, n=12, steps=11)
    rid = b.add(roll, "moss", name="moss", hull=proxy, hull_pad=True)
    # ragged tufts along the head's upper flanks and its end, each with its own ink: they break the outline into
    # scallops (a smooth cocoon read as a larva); high on the flank, never at the ground line (low lumps read as feet)
    edge = [(-0.112, 0.044, 0.030), (-0.064, 0.070, 0.030), (-0.010, 0.068, 0.027),
            (-0.088, -0.060, 0.031), (-0.034, -0.072, 0.029), (0.018, -0.058, 0.025), (-0.128, -0.008, 0.030)]
    for k, (x, y, r) in enumerate(edge):
        z = MOSS_Z0 + 0.45 * MOSS_RZ * _moss_env((x + MOSS_LEN / 2) / MOSS_LEN)
        lp = E.lump((r, r * 0.92, r * 0.66), ("qmoss_edge", k), center=(x, y, z), lobes=6, amp=0.36, subdiv=2)
        b.add(lp, "moss", name="edge_tuft", hull_pad=True)
    # lighter tufts on the crown, large pale spore caps nestled among them (the gather hint: 2.4-2.8 cm, ≥ 2 px at
    # 1080p; the first pass's 1.4 cm caps were sub-pixel)
    tufts = [(-0.094, 0.020, 0.030), (-0.060, -0.034, 0.028), (-0.020, 0.030, 0.030), (0.006, -0.028, 0.027),
             (-0.040, 0.000, 0.026), (0.036, 0.010, 0.024)]
    for k, (x, y, r) in enumerate(tufts):
        z = _moss_top(x, y) - r * 0.30
        lp = E.lump((r, r * 0.9, r * 0.62), ("qmoss", k), center=(x, y, z), lobes=6, amp=0.30, subdiv=2)
        b.add(lp, "moss_lit", name="tuft", outline=False, covered_by=rid)
    caps = [(-0.106, -0.012, 0.0135), (-0.072, 0.040, 0.0125), (-0.040, -0.040, 0.0130), (-0.004, 0.006, 0.0140),
            (-0.064, -0.006, 0.0120), (0.026, -0.022, 0.0120)]
    for x, y, r in caps:
        z = _moss_top(x, y) + 0.010
        b.add(IK.ell((r, r, r * 0.80), (x, y, z), sides=7, rings=4), "moss_tip", name="spore", outline=False,
              covered_by=rid)
    # one binding of two turns of pale twine at the neck
    k = _moss_env((MOSS_BIND + MOSS_LEN / 2) / MOSS_LEN)
    ub = (MOSS_BIND + MOSS_LEN / 2) / MOSS_LEN
    b.add(_twine_band(MOSS_BIND, MOSS_RY * k, MOSS_RZ * k, MOSS_Z0, amp=_moss_amp(ub)), "twine", name="twine",
          outline=False, covered_by=rid)
    # a wide fan of torn root out of the bound end (±70°, pale fibres splayed on the ground, two forked)
    xe = MOSS_LEN / 2 - 0.010
    for k in range(8):
        a = (k - 3.5) / 3.5 * math.radians(70) + rng.uniform(-0.07, 0.07)
        ln = rng.uniform(0.050, 0.074)
        p0 = Vector((xe, 0.016 * (k - 3.5) / 3.5, 0.040 + 0.006 * (k % 2)))
        d = Vector((math.cos(a), math.sin(a), 0.0))
        pts = [p0, p0 + d * ln * 0.36 + Vector((0, 0, -0.016)), p0 + d * ln * 0.72 + Vector((0, 0, -0.030)),
               p0 + d * ln + Vector((0, 0, -0.036))]
        for q in pts:
            q.z = max(q.z, 0.0040)
        b.add(M.tube(pts, [0.0052, 0.0042, 0.0032, 0.0024], sides=5, dome0=0.001, dome1=0.002), "moss_root",
              name="root", hull_pad=True)
        if k in (1, 5):
            q0 = pts[2]
            d2 = Vector((math.cos(a + 0.5), math.sin(a + 0.5), 0.0))
            fork = [q0, q0 + d2 * 0.018, q0 + d2 * 0.030]
            for q in fork:
                q.z = max(q.z - 0.002, 0.0040)
            b.add(M.tube(fork, [0.0030, 0.0026, 0.0020], sides=5, dome0=0.001, dome1=0.0015), "moss_root",
                  name="root", hull_pad=True)
    obj = asset.finish_mesh(b, pal, CLS)
    L.thin_hull(obj, HULL_SCALE[name])
    tz = L.top_z(obj)
    return obj, L.sockets(tz, (0.0, 0.0, tz)), {
        "kind": "quest", "itemKind": "moss", "questItem": "mat_barrowmoss", "hullScale": HULL_SCALE[name],
        "look": "a ragged handful of grave moss bound at one end with twine, large pale spore caps, a fan of torn "
                "roots"}


def _roll_xz(length: float, ry: float, rz: float, z0: float, pinch=(), seed: float = 0.0, n: int = 18,
             steps: int = 15, env=None, amp=0.10) -> M.Part:
    """A lumpy roll along X (moss pressed into a handful): elliptic sections ``ry`` x ``rz`` centred ``z0`` up, rounded
    ends, waists at ``pinch`` x positions (where the twine binds it), flat where it meets the ground. ``env(u)``:
    the section scale along the length (default a symmetric sin^.55); ``amp``: the lumpy noise (or ``amp(u)``)."""
    import bmesh
    bm = bmesh.new()
    xs = [-length / 2 + length * (0.5 - 0.5 * math.cos(math.pi * i / (steps - 1))) for i in range(steps)]
    rings = []
    for x in xs[1:-1]:
        u = (x + length / 2) / length
        e = env(u) if env is not None else math.sin(math.pi * u) ** 0.55
        w = 1.0
        for px in pinch:
            w -= 0.18 * math.exp(-((x - px) / 0.016) ** 2)
        ring = []
        for i in range(n):
            a = TAU * i / n
            k = e * w * (1.0 + (amp(u) if callable(amp) else amp) * _noise(a, x, seed))
            y = ry * k * math.cos(a)
            z = z0 + rz * k * math.sin(a)
            z = max(z, 0.0012)                           # flattened on the ground
            ring.append(bm.verts.new((x, y, z)))
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    for ring, x, sgn in ((rings[0], xs[0], -1), (rings[-1], xs[-1], 1)):
        c = sum((v.co for v in ring), Vector()) / len(ring)
        cv = bm.verts.new(Vector((x * 0.98, c.y, c.z)))
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((ring[i], ring[j], cv) if sgn > 0 else (ring[j], ring[i], cv))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def _twine_band(x: float, ry: float, rz: float, z0: float, seed: float = 0.0, amp: float = 0.10) -> M.Part:
    """Two turns of twine round the roll's waist at ``x`` (a tube on the section ellipse riding the roll's own
    ``amp`` noise, ends tucked in)."""
    pts = []
    for i in range(29):
        t = i / 28
        a = -math.pi / 2 + TAU * 1.95 * t
        k = 1.0 + amp * _noise(a, x, 1.3) + 0.05
        pts.append(Vector((x + 0.008 * (t - 0.5), ry * k * math.cos(a), max(0.006, z0 + rz * k * math.sin(a)))))
    return M.tube(pts, [0.0034] * len(pts), sides=5, dome0=0.001, dome1=0.001)


# ── tag pouch ─────────────────────────────────────────────────────────────────────────────────────────────
def pouch_section(x: float) -> tuple[float, float, float]:
    """(half-width y, half-height z, centre z) of the pouch body at ``x`` (closed end −X, mouth +X)."""
    xs = [-0.100, -0.088, -0.060, -0.020, 0.020, 0.044, 0.056, 0.068, 0.080]
    ys = [0.004, 0.040, 0.060, 0.064, 0.054, 0.030, 0.024, 0.030, 0.038]
    zs = [0.004, 0.032, 0.046, 0.050, 0.044, 0.026, 0.021, 0.026, 0.032]
    hy = float(np.interp(x, xs, ys))
    hz = float(np.interp(x, xs, zs))
    return hy, hz, hz + 0.002


# Art review r2 (README §13 budget: pickups 0.7-3 k toon + ≤ 1.5 k hull; the first pass was 3674 + 1866): a 14-sided
# pouch on 11 rings, 10-sided tags with a 4-point rim, two stamps a tag, 5-sided cords
POUCH_SIDES, POUCH_RINGS = 16, 11
TAG_SIDES = 10


def build_tags(pal: Palette):
    """Dreck's pouch of cut name-tags (S3 ``mat_tag_pouch``): dark leather, tarnished silver tags on pale cords."""
    name = "SM_Pickup_Quest_Tags"
    b = builder(name, pal)
    rng = scene.rng(name, "tags")
    n = POUCH_SIDES
    rings = []
    xs = list(np.linspace(-0.098, 0.080, POUCH_RINGS))
    for x in xs:
        hy, hz, cz = pouch_section(x)
        ring = []
        for i in range(n):
            a = TAU * i / n
            wob = 1.0 + 0.05 * math.sin(4 * a + 9 * x) + (0.06 * math.sin(6 * a) if x > 0.06 else 0.0)
            y = hy * wob * math.cos(a)
            z = cz + hz * wob * math.sin(a)
            ring.append((x, y, max(z, 0.003)))
        rings.append(ring)
    import bmesh
    bm = bmesh.new()
    vr = [[bm.verts.new(p) for p in r] for r in rings]
    for r0, r1 in zip(vr, vr[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    c0 = bm.verts.new((xs[0] - 0.006, 0.0, rings[0][0][2]))
    for i in range(n):
        bm.faces.new((vr[0][(i + 1) % n], vr[0][i], c0))
    # the open mouth: a rim turned in to a dark interior a few cm deep
    inner = []
    for i in range(n):
        x, y, z = rings[-1][i]
        cz = pouch_section(0.080)[2]
        inner.append(bm.verts.new((x - 0.004, y * 0.80, cz + (z - cz) * 0.80)))
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((vr[-1][i], vr[-1][j], inner[j], inner[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    body = M.Part(bm).smooth(True)
    pid = b.add(body, "pouch", name="pouch", hull_pad=True)
    hy, hz, cz = pouch_section(0.080)
    hole = M.ellipsoid((0.004, hy * 0.80, hz * 0.80), (0.070, 0.0, cz), sides=12, rings=4)
    b.add(hole, "leather_in", name="mouth", outline=False, covered_by=pid)
    # worn lighter leather on the upper flanks (the lit side shows on the darkest mud)
    for side in (1, -1):
        pts, nrm = [], []
        for x in np.linspace(-0.086, 0.040, 6):
            hy_, hz_, cz_ = pouch_section(float(x))
            a = math.radians(48) if side > 0 else math.radians(132)
            pts.append(Vector((float(x), hy_ * math.cos(a), cz_ + hz_ * math.sin(a))))
            nrm.append(Vector((0.0, math.cos(a), math.sin(a))))
        b.add(IK.surface_strip(pts, nrm, 0.016, lift=0.0010, depth=0.003), "pouch_lit", name="worn",
              outline=False, covered_by=pid)
    # drawstring round the neck + its two loose ends on the ground
    hy, hz, cz = pouch_section(0.056)
    ring = []
    for i in range(16):
        a = TAU * i / 16
        ring.append(Vector((0.056 + 0.003 * math.sin(2 * a), (hy + 0.003) * math.cos(a),
                            max(0.0040, cz + (hz + 0.003) * math.sin(a)))))
    b.add(M.sweep(ring, M.superellipse(0.0036, 0.0036, 5), closed_path=True, up=(1, 0, 0)), "drawcord",
          name="drawcord", outline=False, covered_by=pid)
    for sgn in (1, -1):
        p0 = Vector((0.058, sgn * (hy + 0.002), cz + 0.004))
        pts = [p0, p0 + Vector((0.010, sgn * 0.020, -0.012)), p0 + Vector((0.006, sgn * 0.046, -0.024)),
               p0 + Vector((-0.006, sgn * 0.068, -0.030))]
        for p in pts:
            p.z = max(p.z, 0.0042)
        b.add(M.tube(pts, [0.0038, 0.0036, 0.0036, 0.0034], sides=5, dome0=0.002, dome1=0.003), "drawcord",
              name="cordend", hull_pad=True)
        b.add(IK.ell((0.0068, 0.0068, 0.0052), tuple(pts[-1] + Vector((0, 0, 0.0012))), sides=6, rings=3), "drawcord",
              name="cordend", hull_pad=True)
    # the name-tags: tarnished silver ovals on cords spilled from the mouth (two half out of the mouth, four on the
    # ground), two blackened with soot tarnish
    tags = [  # (centre, yaw°, tilt about the radial axis°, tarnished)
        (Vector((0.122, -0.054, 0.0)), 18.0, 0.0, False),
        (Vector((0.148, 0.040, 0.0)), -32.0, 0.0, True),
        (Vector((0.182, -0.016, 0.0)), 64.0, 0.0, False),
        (Vector((0.160, 0.096, 0.0)), 8.0, 0.0, True),
        (Vector((0.104, 0.030, 0.0)), -14.0, 52.0, False),
        (Vector((0.100, -0.026, 0.0)), 16.0, 44.0, False),
    ]
    for k, (c, yaw, tilt, tarn) in enumerate(tags):
        _add_tag(b, c, yaw, tilt, tarn, rng, mouth=Vector((0.070, 0.0, cz)))
    obj = asset.finish_mesh(b, pal, CLS)
    L.thin_hull(obj, HULL_SCALE[name])
    tz = L.top_z(obj)
    return obj, L.sockets(tz, (0.12, 0.0, 0.01)), {
        "kind": "quest", "itemKind": "tags", "questItem": "mat_tag_pouch", "hullScale": HULL_SCALE[name],
        "look": "Dreck's dark leather drawstring pouch, mouth open, tarnished silver name-tags spilled on pale cords"}


TAG_R = (0.030, 0.022)          # an oval disc (story-bible: "discs on cords"), longer than a coin is round
TAG_T = 0.0055


def _add_tag(b: M.Builder, c: Vector, yaw: float, tilt: float, tarnish: bool, rng, mouth: Vector) -> None:
    """One silver name-tag: an oval disc (bevelled pale rim, flat stamped face, punched hole at its +X end) resting on
    the ground at ``c`` (xy), turned ``yaw``; ``tilt`` > 0 leans it with the hole end raised toward the pouch mouth
    (half pulled out by its cord). The cord runs from the hole back into the mouth."""
    rx, ry = TAG_R
    rim = IK.revolve([(0.80, -TAG_T / 2), (1.0, -TAG_T * 0.22), (1.0, TAG_T * 0.22), (0.80, TAG_T / 2)],
                     sides=TAG_SIDES).auto_sharp(40)
    face = M.cylinder(0.79, 0.80, -TAG_T * 0.58, TAG_T * 0.58, sides=TAG_SIDES)
    for p in (rim, face):
        p.deform(lambda co: Vector((co.x * rx, co.y * ry, co.z)))
    if tilt:
        d = mouth - c
        yaw = math.degrees(math.atan2(d.y, d.x)) + yaw
    rot = Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Rotation(math.radians(-tilt), 4, "Y")
    lift = rx * math.sin(math.radians(tilt)) + TAG_T * 0.5 * math.cos(math.radians(tilt)) + 0.0012
    m = Matrix.Translation(Vector((c.x, c.y, lift))) @ rot
    tid = b.add(face.transform(m), "silver", name="tag", hull=rim.copy().transform(m), hull_pad=True)
    b.add(rim.transform(m), "silver_rim", name="tag_rim", outline=False, covered_by=tid)
    # a stamped name: two short strokes toward the −X end (a name and a number, not a coin's legend); the punched
    # hole at the top (+X edge)
    nz = (m.to_3x3() @ Vector((0, 0, 1))).normalized()
    for j, (w, dy, x0) in enumerate(((0.016, 0.0045, -0.012), (0.010, -0.0050, -0.015))):
        pts = [m @ Vector((x0 + w * t, dy, TAG_T * 0.6)) for t in (0.0, 1.0)]
        b.add(IK.surface_strip(pts, [nz] * len(pts), 0.0030, lift=0.0004, depth=0.0015), "silver_stamp",
              name="stamp", outline=False, covered_by=tid)
    hole_c = m @ Vector((rx * 0.70, 0.0, 0.0))
    b.add(IK.ell((0.0045, 0.0045, TAG_T * 0.62), (0, 0, 0), sides=6, rings=3).transform(
        Matrix.Translation(hole_c) @ m.to_3x3().to_4x4()), "silver_stamp", name="hole", outline=False,
        covered_by=tid)
    if tarnish:
        for q in range(2):
            a = rng.uniform(0, TAU)
            rr = rng.uniform(0.004, 0.011)
            p = m @ Vector((rr * math.cos(a) * 1.4, rr * math.sin(a), TAG_T * 0.6))
            b.add(IK.ell((0.0095, 0.0075, 0.0012), (0, 0, 0), sides=7, rings=3).transform(
                Matrix.Translation(p) @ m.to_3x3().to_4x4()), "tarnish", name="tarnish", outline=False,
                covered_by=tid)
    # cord: from the hole back toward the pouch mouth, lying on the ground
    p0 = hole_c + nz * 0.002
    toward = (mouth - p0)
    toward.z = 0.0
    dist = toward.length
    toward.normalize()
    side = Vector((-toward.y, toward.x, 0.0))
    wig = rng.uniform(-1, 1)
    pts = [p0]
    for t in (0.25, 0.5, 0.75, 0.95):
        p = p0 + toward * dist * t + side * (0.010 * math.sin(math.pi * t * 2 + wig))
        p.z = max(0.0042, p0.z * (1 - t) + (mouth.z - 0.008) * t * t)
        pts.append(p)
    b.add(M.tube(pts, [0.0034] * len(pts), sides=5, dome0=0.0015, dome1=0.0015), "thong", name="thong",
          hull_pad=True)


# ── gather node (Props_March) ─────────────────────────────────────────────────────────────────────────────
NODE = "SM_Prop_Gather_Barrowmoss"
NODE_IDS = ["gather_moss", "gather_herb@march"]
NODE_PREFIX = "barrowmoss"


def node_regions() -> dict:
    regs = EM.regions(EM.PROPS, "barrowmoss")
    regs["earth"] = EM.region(EM.PROPS, "camp.ash")
    regs["stone"] = EM.region(EM.PROPS, "rock.stone")
    regs["stone_line"] = EM.region(EM.PROPS, "catechism.carve")
    regs["lichen"] = EM.region(EM.PROPS, "boulder.ash")
    return regs


def build_node(pal: Palette):
    """Barrowmoss cushions growing over a low clod of grave earth (S2 gather node): one clump that can bob and rock
    (art review r2: no grave marker — a headstone that floats and wobbles is a haunting, not a gather point)."""
    b = M.Builder(NODE, pal, prefix=NODE_PREFIX)
    b.regions(**node_regions())
    rnd = scene.rng(NODE, "node")
    # a low clod: it shows as a thin earth lip past the cushions, not a dark tray under them
    clod = M.ellipsoid((0.122, 0.104, 0.024), (0.0, 0.0, 0.002), sides=16, rings=6)
    clod.deform(lambda co: Vector((co.x, co.y, max(co.z, 0.0005) + 0.004 * math.sin(9 * co.x + 5 * co.y))))
    b.add(clod, "earth", name="earth", hull_pad=True)
    # moss cushions over the clod (the back one fills where the first pass's marker stood); seated low (squash
    # .35: their lower lobes sit on the clod instead of hanging past it in jagged hull wedges)
    cush = [((0.050, -0.042, 0.044), (0.092, 0.078, 0.056)), ((-0.072, -0.034, 0.042), (0.080, 0.070, 0.052)),
            ((-0.008, 0.022, 0.062), (0.082, 0.064, 0.058)), ((0.100, 0.036, 0.034), (0.058, 0.054, 0.042)),
            ((-0.108, 0.046, 0.034), (0.054, 0.050, 0.040)), ((-0.010, -0.082, 0.026), (0.070, 0.042, 0.034)),
            ((0.022, 0.082, 0.032), (0.064, 0.044, 0.038))]
    # each cushion carries its own padded ink (one proxy padded round all of them tented out past the clod);
    # small lighter tufts and the pale spore caps (A1 barrowmoss.glow, the gather hint) sit on their crowns
    for k, (c, r) in enumerate(cush):
        lp = E.Lump(r, ("bm_cush", k), center=c, lobes=10, amp=0.38, subdiv=2, squash_bottom=0.35)
        kid = b.add(lp.part(), "moss", name="moss", hull_pad=True)
        top = Vector(c) + Vector((0, 0, r[2] * 0.62))
        nt = 7 if k < 3 else 5
        for j in range(nt):
            a = TAU * j / nt + rnd.uniform(-0.3, 0.3)
            d = rnd.uniform(0.20, 0.62) if j else 0.0
            p = top + Vector((math.cos(a) * r[0] * d, math.sin(a) * r[1] * d, -r[2] * 0.55 * d * d + 0.004))
            rr = rnd.uniform(0.018, 0.026)
            b.add(E.lump((rr, rr, rr * 0.62), ("bm_tuft", k, j), center=tuple(p), lobes=5, amp=0.30, subdiv=1),
                  "moss_lit", name="tuft", outline=False, covered_by=kid)
        for j in range(2):
            a = rnd.uniform(0, TAU)
            d = rnd.uniform(0.10, 0.45)
            p = top + Vector((math.cos(a) * r[0] * d, math.sin(a) * r[1] * d, r[2] * 0.14 - r[2] * 0.35 * d * d))
            b.add(IK.ell((0.0115, 0.0115, 0.0135), tuple(p), sides=7, rings=4), "glow", name="capsule",
                  outline=False, covered_by=kid)
    # the baked hull hugs the ground contact (envkit.ground_hull): the cushions' and the clod's lower hull no longer
    # fans down-and-out into a dark band at the base
    obj = E.finish(b, pal, "interactive", ground_h=0.25, name=NODE, ground_band=0.02)
    return obj


NODE_NOTE = ("S2 q_rf_barrowmoss gather node (苦冢苔, itemKind moss → gather_moss): moss cushions over a low clod of "
             "ash earth, one self-contained clump; bob + glow ring are runtime FX (prop.gather, "
             "EAbyssPropStyle::Pickup bob 6 px / 900 ms, rock ±8°, so no grave marker on it — art review r2: a "
             "floating, wobbling headstone; a broken marker belongs to the static Burial Trenches decor); the pale "
             "spore caps are A1's barrowmoss.glow gather hint (not ember)")


# ── catalogue / ship ──────────────────────────────────────────────────────────────────────────────────────
PICKUPS = [("ichor", "SM_Pickup_Quest_Ichor", build_ichor, ["quest_item_ichor"]),
           ("moss", "SM_Pickup_Quest_Moss", build_moss, ["quest_item_moss"]),
           ("tags", "SM_Pickup_Quest_Tags", build_tags, ["quest_item_tags"])]


def build_pickups(groups=None):
    pal = palette()
    pal.save()
    objs = {}
    for grp, name, fn, ids in PICKUPS:
        if groups and grp not in groups:
            continue
        obj, socks, meta = fn(pal)
        obj.name = name
        obj.hide_render = True
        low = L.lowest_z(obj)
        if abs(low) > 0.0015:
            print(f"[quest] WARNING {name}: lowest point {low * 100:.2f} cm (pivot should be on the ground)")
        objs[name] = (obj, socks, meta, ids, grp)
        print(f"[quest] {name}: toon {M.tri_count(obj, 0)} hull {M.tri_count(obj, 1)}  lowest {low * 100:.2f} cm  "
              f"top {L.top_z(obj) * 100:.1f} cm  footprint r {L.footprint_r(obj) * 100:.1f} cm")
    return pal, objs


def ship_pickups(pal, objs, previews: bool, verify: bool) -> list[str]:
    fails = []
    for name, (obj, socks, meta, ids, grp) in objs.items():
        fbx = export.export_static_mesh(obj, name, CATEGORY, socks)
        man = export.Manifest()
        man.set_palette(pal)
        loot = L.loot_entry(meta, tint=False)
        extra = {"loot": loot, "pickupGroup": "quest", "remake": "dark remake A11 (焚境 · The Burning March)",
                 "family": "items.quest_march"}
        e = export.static_entry(obj, fbx, CATEGORY, pal, CLS, ids, socks, footprint_tiles=[1, 1], blocking=False,
                                extra=extra)
        if len(e["materialSlots"]) > 1:
            e["materialSlots"][1]["outlineMix"] = outline.INK_MIX[CLS]
        if previews:
            files, v = L.review_asset(obj, name, meta=meta)
            files.append(tile_cells(obj, name, march=True))
            e["previews"] = [str(p.relative_to(paths.preview_root())) for p in files]
            e["inkGate"] = IK.ink_entry(v)
            e["inkGate"]["ground"] = f"march ash grass {EM.GRASS_HEX} (envkit_march stage)"
            ok = v["pass"]
            print(f"[ink] {name}: {v['cells']} cells, min rim {v['minFracRim'] * 100:.1f} %, dark "
                  f"{v['minFracDark'] * 100:.1f} %, median {v['medianRimPx']} px -> {'PASS' if ok else 'FAIL'}")
            if not ok:
                fails.append(f"ink {name}: {v['failures']}")
        man.set_asset(name, e)
        man.save()
    if verify:
        for name, (obj, socks, meta, ids, grp) in objs.items():
            fails += IK.verify_fbx(name, CATEGORY, sum(p.loop_total - 2 for p in obj.data.polygons),
                                   [s["name"] for s in socks], FAMILY)
    return fails


TILES = (("GRASS", EM.GRASS_HEX), ("MUD (DARKEST)", EM.DIRT_HEX), ("STONE", EM.STONE_HEX), ("CAMP", EM.CAMP_HEX))


def tile_cells(obj, name: str, march: bool = True, out_dir: Path | None = None, rv=None) -> Path:
    """``march_tiles.png``: the shipped look (2 px line ink, bloom) at W1 1080p 1:1 on each march ground value —
    ash grass, the darkest walkable mud, stone, camp ash — each ink-gated (the pickup must read on all four).
    ``rv``: render through that review (the gather node's ``EnvReview``: its cells join its ink verdict)."""
    obj.hide_render = False
    if rv is None:
        rv = L.LootReview(name, obj, [obj], max(L.top_z(obj), 0.05),
                          blob_radius=max(0.08, L.footprint_r(obj) * 1.15),
                          game_outline_px=outline.SCREEN_PX_1080[CLS], outline_class=CLS, out_dir=out_dir)
        rv.ground_occludes = True
    cells = []
    for lbl, hx in TILES:
        review.stage(blob_radius=rv.blob_radius, ground_hex=hx, grid=False)
        for f in ("front", "sw"):
            rv.facing(f)
            review.setup_render((1920, 1080), rv.samples)
            cam = review.game_camera()
            box = rv.game_crop_box(cam, pad=0.03)
            rv.game_outline(1080)
            img, st = rv.game_shot(box, measure=True, tag=f"tile {lbl} {f}")
            cells.append((f"{lbl} {f.upper()}", img, st))
    review.stage(blob_radius=rv.blob_radius)
    rv.facing("front")
    h = max(max(c[1].shape[0] for c in cells), 120)
    w = max(max(c[1].shape[1] for c in cells), 170)
    out = []
    for lbl, img, st in cells:
        c = IK.pad_to(img, h, w)
        review.draw_text(c, 3, 3, lbl)
        review.draw_text(c, 3, 12, f"RIM {st['fracRim'] * 100:.0f}% D {st['fracDark'] * 100:.0f}%", rgb=(255, 220, 140))
        out.append(c)
    obj.hide_render = True
    return rv._save("march_tiles.png", review.grid(out, 4, pad=2))


def ship_node(previews: bool, verify: bool) -> list[str]:
    pal = EM.palette(EM.PROPS)
    obj = build_node(pal)
    pal.save()
    problems = EM.check_light(obj, pal)
    if problems:
        raise RuntimeError(f"{NODE}: " + "; ".join(problems))
    print(f"[node] {NODE}: toon {E.tris(obj, 0)} hull {E.tris(obj, 1)}")
    extra = {"displayName": "Gather_Barrowmoss", "family": "items.quest_march", "note": NODE_NOTE,
             "theme": EM.THEME, "themeOf": ["gather_herb"],
             "themeNote": "gather_moss is the S2 spots' plain id (itemKind moss); gather_herb@march is A1's planned "
                          "march slot for the herb gather (the runtime resolves <id>@march before <id>, U1)",
             "remake": "dark remake A11 (焚境 · The Burning March)"}
    # march_tiles.png (art review r2): the same front / sw gate on the four march grounds as the pickups — the
    # node stands in the Burial Trenches, on the darkest mud too; its cells join the node's ink verdict
    res = E.ship(obj, NODE, "Props", pal, "interactive", game_ids=NODE_IDS, footprint=[1, 1], blocking=False,
                 extra=extra, previews=previews, blob=0.20, close_facing="front",
                 review_extra=lambda rv: tile_cells(obj, NODE, rv=rv))
    fails = []
    if previews and res.get("ink") and not res["ink"]["pass"]:
        fails.append(f"ink {NODE}: {res.get('inkFailures')}")
    if verify:
        fails += IK.verify_fbx(NODE, "Props", sum(p.loop_total - 2 for p in obj.data.polygons), [], EM.PROPS)
    return fails, obj


# ── family sheets ─────────────────────────────────────────────────────────────────────────────────────────
def lineup(objs: dict, node=None) -> np.ndarray:
    """The three pickups (and the gather node) round the warrior on the march ash grass at W1 1080p 1:1."""
    arm, body = IK.import_hero("Warrior")
    arm.rotation_euler = (0, 0, math.radians(review.FACINGS["se"]))
    fwd = Vector((1.0, -1.0, 0.0)).normalized()
    right = Vector((-1.0, -1.0, 0.0)).normalized()
    keep = [body]
    items = [o[0] for o in objs.values()] + ([node] if node is not None else [])
    for i, o in enumerate(items):
        off = (i - (len(items) - 1) / 2) * 0.55
        o.matrix_world = Matrix.Translation(-fwd * 0.95 + right * off)
        o.hide_render = False
        keep.append(o)
    bpy.context.view_layer.update()
    with IK.only_visible(keep):
        review.hide_stage(False)
        review.setup_render((1920, 1080), 8)
        review.stage(blob_radius=0.36)
        cam = review.game_camera((-0.4, 0.4))
        for o in keep:
            if o is body:
                review.set_outline_px([o], outline.SCREEN_PX_1080["hero"], Vector((0, 0, 0.9)))
            else:
                review.set_outline_px([o], outline.SCREEN_PX_1080["interactive"], Vector((0, 0, 0.1)))
        pts = review.mesh_points(keep, step=4)
        xy = np.array([review.project_px(cam, Vector(p), (1920, 1080)) for p in pts[::7]])
        x0, x1 = xy[:, 0].min() - 50, xy[:, 0].max() + 50
        y0, y1 = xy[:, 1].min() - 40, xy[:, 1].max() + 40
        box = L.px_border(x0, y0, x1, y1)
        img = review.render_array(box)
        img = review.bloom(img, review.emissive_pass(keep, box), 1080)
    review.draw_text(img, 4, 4, "MARCH QUEST ITEMS ROUND SK_HERO_WARRIOR  W1 1080P 1:1  ASH GRASS #4C4940  SHIPPED INK + BLOOM")
    for o in items:
        o.hide_render = True
        o.matrix_world = Matrix.Identity(4)
    IK.remove_hero(arm)
    return img


def loot_compare(tags) -> np.ndarray:
    """``tags_vs_loot.png`` (art review r2: the first tag pouch read as a coin purse spilling gold): SM_Loot_Bag, the
    gold piles S / M / L and SM_Pickup_Quest_Tags side by side at W1 1080p 1:1 with the shipped ink, on the march ash
    grass and on the darkest mud. The loot meshes are rebuilt from ``loot.py`` in memory (nothing re-shipped)."""
    pal = L.palette()
    others = [L.build_bag(pal)[0]] + [L.build_gold(pal, s)[0] for s in ("S", "M", "L")]
    items = others + [tags]
    names = ["BAG", "GOLD S", "GOLD M", "GOLD L", "TAGS"]
    right = Vector((-1.0, -1.0, 0.0)).normalized()
    for i, o in enumerate(items):
        o.matrix_world = Matrix.Translation(right * (i - (len(items) - 1) / 2) * 0.42)
        o.hide_render = False
    bpy.context.view_layer.update()
    rows = []
    with IK.only_visible(items):
        review.hide_stage(False)
        for lbl, hx in (("ASH GRASS", EM.GRASS_HEX), ("MUD (DARKEST)", EM.DIRT_HEX)):
            review.setup_render((1920, 1080), 8)
            review.stage(blob_radius=0.30, ground_hex=hx, grid=False)
            cam = review.game_camera()
            for o in items:
                review.set_outline_px([o], outline.SCREEN_PX_1080[CLS], Vector((0, 0, 0.05)))
            pts = review.mesh_points(items, step=4)
            xy = np.array([review.project_px(cam, Vector(p), (1920, 1080)) for p in pts[::5]])
            box = L.px_border(xy[:, 0].min() - 40, xy[:, 1].min() - 26, xy[:, 0].max() + 40, xy[:, 1].max() + 26)
            img = review.render_array(box)
            img = review.bloom(img, review.emissive_pass(items, box), 1080)
            for o, nm in zip(items, names):
                x, _y = review.project_px(cam, o.matrix_world.translation, (1920, 1080))
                review.draw_text(img, max(0, int(x - box[0] * 1920) - 3 * len(nm)), img.shape[0] - 10, nm,
                                 rgb=(230, 220, 200))
            review.draw_text(img, 3, 3, f"{lbl} {hx}  1080P 1:1")
            rows.append(img)
    review.stage(blob_radius=0.36)
    for o in others:
        bpy.data.objects.remove(o, do_unlink=True)
    tags.hide_render = True
    tags.matrix_world = Matrix.Identity(4)
    w = max(r.shape[1] for r in rows)
    return review.grid([IK.pad_to(r, r.shape[0], w) for r in rows], 1, pad=2)


def studio(objs: list) -> np.ndarray:
    return L.studio_sheet(objs, lambda o: o.name.replace("SM_", ""), len(objs), cell=(300, 300))


def quick(groups) -> None:
    out = IK.SCRATCH / "quest_march"
    scene.reset()
    objs = {}
    if not groups or any(g in groups for g in ("ichor", "moss", "tags")):
        pal, objs = build_pickups(groups)
        for name, o in objs.items():
            files, v = L.review_asset(o[0], name, out_dir=out / name, meta=o[2])
            tile_cells(o[0], name, out_dir=out / name)
            print(f"[quick] {name} ink {'PASS' if v['pass'] else 'FAIL'} rim {v['minFracRim']:.3f} dark "
                  f"{v['minFracDark']:.3f} median {v['medianRimPx']}")
    node = None
    if not groups or "node" in groups:
        palp = EM.palette(EM.PROPS)
        node = build_node(palp)
        palp.save()
        print(f"[quick] node toon {E.tris(node, 0)} hull {E.tris(node, 1)}; light check {EM.check_light(node, palp)}")
        rv = E.EnvReview(NODE, node, [node], 0.3, blob_radius=0.2, game_outline_px=E.ink_px(EM.PROPS, "interactive"),
                         outline_class="interactive", out_dir=out / NODE)
        rv.ground_occludes = True
        rv.game_view("front")
        rv.closeup("front")
        rv.turnaround()
        tile_cells(node, NODE, rv=rv)
        v = rv.ink_verdict()
        print(f"[quick] node ink {'PASS' if v['pass'] else 'FAIL'} rim {v['minFracRim']:.3f} median {v['medianRimPx']}")
        node.hide_render = True
    sel = [o[0] for o in objs.values()] + ([node] if node is not None else [])
    if sel:
        IK.save(out, "studio.png", studio(sel))
    if objs:
        IK.save(out, "lineup.png", lineup(objs, node))
    if "SM_Pickup_Quest_Tags" in objs:
        IK.save(out, "tags_vs_loot.png", loot_compare(objs["SM_Pickup_Quest_Tags"][0]))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--quick", nargs="?", const="all", default=None)
    ap.add_argument("--no-previews", action="store_true")
    ap.add_argument("--no-verify", action="store_true")
    a = ap.parse_args(argv)
    if a.quick is not None:
        quick(None if a.quick == "all" else [g for g in a.quick.split(",") if g])
        return 0
    t0 = time.time()
    groups = [g for g in a.only.split(",") if g] or None
    previews, verify = not a.no_previews, not a.no_verify
    scene.reset()
    fails = []
    objs = {}
    if not groups or any(g in groups for g in ("ichor", "moss", "tags")):
        pal, objs = build_pickups(groups)
        fails += ship_pickups(pal, objs, previews, verify)
    node = None
    if not groups or "node" in groups:
        f, node = ship_node(previews, verify)
        fails += f
        node.hide_render = True
    if previews and groups is None:
        fam = paths.preview_root() / OUT
        sel = [o[0] for o in objs.values()] + [node]
        IK.save(fam, "studio.png", studio(sel))
        IK.save(fam, "game_lineup.png", lineup(objs, node))
        IK.save(fam, "tags_vs_loot.png", loot_compare(objs["SM_Pickup_Quest_Tags"][0]))
    print("[verify] " + ("OK" if not fails else "\n  ".join(["FAIL"] + fails)))
    print(f"[quest_march] done in {time.time() - t0:.0f}s")
    return 1 if fails else 0


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(argv))
