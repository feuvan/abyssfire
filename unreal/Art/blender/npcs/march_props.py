"""March NPC props (A9): the held props of the re-dressed looks, Old Bern's barrow and Hald's charred anvil stump.

Held props follow the kit convention of ``props.py`` (weapon-bone space: grip at the origin, **+Y along the item**,
**+Z its leading face**; they attach to ``weapon_r`` / ``weapon_l`` with an identity transform), on the NPC palette
with the NPC ink (``npc_ship.ink_slot``). Colours are the A1 kit swatches (``march_palettes.NPC_KIT``) or the shipped
prop values (wood, paper, leather), so no prop takes a new atlas index but the charred stump's two tones.

* ``SM_NPCKit_Prop_Handbell`` — Brother Aldwin's bronze handbell: a turned wooden handle, the bell below it along +Y
  (mouth at the far end, a dark mouth and the clapper's ball), a cord loop at the handle's end; sockets ``bell``
  (mouth centre: ``FX_BellToll``) and ``tip``;
* ``SM_NPCKit_Prop_Lantern`` — Mother Marl's lantern hanging from the fist: a wire bail, a cone cap, four posts
  round warm panes (``lantern_glow`` #F0A060, emissive .55 — her dressing station is a refuge), socket ``glow``;
* ``SM_NPCKit_Prop_Ledger`` — the quartermaster's / gravewarden's ledger: the stash keeper's open-book shape in
  leather with a tally ribbon;
* ``SM_NPCKit_Prop_Vial`` — Marl's phial of black blood: near-black glass, a paper label, a cork;
* ``SM_NPCKit_Prop_Barrow`` — Old Bern's barrow of shrouded dead, modelled in **body space** and exported in the
  frame of his bone ``barrow_01`` (head at the wheel's axle; socket ``barrow``): two handles whose grips (sockets
  ``handle_l`` / ``handle_r``) sit at his hands, a plank bed on two back legs, the axle on a fork past the bed (the
  wheel itself is in Bern's mesh on ``barrow_wheel_01``: ``wheel_parts``), three shrouded bodies — head, neck,
  shoulders, a taper to the feet — bound with cord and stained with grave earth toward the bed; ``barrow_matrix`` /
  ``lift_pitch`` pitch it about the axle (Walk / Run lift the legs ~10 cm);
* ``SM_NPCKit_Prop_Spade`` — Tamsin's grave spade (held under its T-grip, socket ``tip`` at the blade's point);
* ``SM_NPCKit_Prop_SwordNotched`` — Corran's old sword: the plains sword's length and grip, notched edges, a chipped
  point, a plain iron guard (the Captain keeps the plains ``SM_NPCKit_Prop_Sword``);
* ``SM_Prop_March_AnvilStump`` — the plains anvil stump charred black (same footprint, socket ``strike``).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bmesh  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import asset, mesh as M, outline  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

import common as C  # noqa: E402
import hero_base as HB  # noqa: E402
from hero_base import ell  # noqa: E402
import props as P  # noqa: E402
import march_folk as MF  # noqa: E402

U = P.U
K = MF.KIT

MARCH_PROPS = {
    "handbell": "SM_NPCKit_Prop_Handbell",
    "lantern": "SM_NPCKit_Prop_Lantern",
    "ledger": "SM_NPCKit_Prop_Ledger",
    "vial": "SM_NPCKit_Prop_Vial",
    "barrow": "SM_NPCKit_Prop_Barrow",
    "spade": "SM_NPCKit_Prop_Spade",
    "sword_notched": "SM_NPCKit_Prop_SwordNotched",
}
ANVIL = "SM_Prop_March_AnvilStump"
# held-prop kinds the plains clip / ship code looks up by name (``props.PROPS``): the march kinds join that table in
# this process only (no file of the plains kit changes)
for _k, _v in MARCH_PROPS.items():
    P.PROPS.setdefault(_k, _v)

REGIONS = outline.ink_regions(dict(
    wood=Region("#7A5230", .42, .30),
    wood_dark=Region("#7A5230", .42, .30).shade_region(.30, .12),
    paper=Region("#EFE3C2", .25, .40),
    ink_line=Region("#EFE3C2", .25, .40).line_region(.2),
    leather=Region("#6A4228"),
    leather_dark=Region("#6A4228").shade_region(.30, .12),
    metal_dark=Region("#5A606E"),
    bronze=Region(K["handbell"].hex, K["handbell"].s, K["handbell"].l),
    bronze_dark=Region(K["handbell_dark"].hex, K["handbell_dark"].s, K["handbell_dark"].l),
    cord=Region(K["cord"].hex, K["cord"].s, K["cord"].l),
    frame=Region(K["lantern_frame"].hex, K["lantern_frame"].s, K["lantern_frame"].l),
    glow=Region(K["lantern_glow"].hex, 0.0, 0.0, e=K["lantern_glow"].e),
    shroud=Region(K["shroud"].hex, K["shroud"].s, K["shroud"].l),
    shroud_line=Region(K["shroud"].hex, K["shroud"].s, K["shroud"].l).shade_region(.28, .12),
), "npc")
# the charred stump: bark burnt to the lantern frame's black (an existing swatch), its split top and rings two new
# char tones; the anvil's iron is the plains value. Its face is a worn, soot-dulled iron (``iron_face_worn``, appended
# by the W1 coherence pass): the plains polish (``iron_face`` #C8D2E2, kept registered) read as the brightest value in
# the Knellwatch camp, a near-white block above the smith himself (the march value hierarchy: actors above decor)
ANVIL_REGIONS = outline.ink_regions(dict(
    bark=Region(K["lantern_frame"].hex, K["lantern_frame"].s, K["lantern_frame"].l),
    bark_dark=Region("#1C1916", .30, .12),
    wood_top=Region("#4A3A2E", .42, .26),
    rings=Region("#4A3A2E", .42, .26).shade_region(.30, .12),
    iron=Region("#5C6270", .42, .40),
    iron_face=Region("#C8D2E2", .30, .45),
    iron_face_worn=Region("#868C95", .30, .40),
), "npc")


def _builder(name: str, pal: Palette, regions=REGIONS) -> M.Builder:
    b = M.Builder(name, pal, prefix=f"npcprop.{name.split('_Prop_')[-1].lower()}")
    b.regions(**regions)
    return b


def _finish(b: M.Builder, pal: Palette, name: str) -> bpy.types.Object:
    obj = asset.finish_mesh(b, pal, "npc")
    obj.name = name
    return obj


# ── handbell ────────────────────────────────────────────────────────────────────────────────────────────
BELL_Y0 = 0.050                 # bell crown (m along +Y from the grip)
BELL_H = 0.112                  # crown → lip (the chaplain's identity prop: ~20 px across at 1080p)
BELL_R = 0.064                  # lip radius


def build_handbell(pal: Palette):
    b = _builder(MARCH_PROPS["handbell"], pal)
    # handle: a turned wooden grip through the fist, a knob above it (−Y), a collar into the bell's crown
    ys = [-0.050, -0.040, -0.020, 0.0, 0.020, 0.036, 0.046]
    rs = [0.010, 0.0125, 0.0115, 0.0110, 0.0115, 0.0125, 0.0105]
    hid = b.add(M.tube([(0, y, 0) for y in ys], rs, sides=10, dome0=0.006, dome1=0.0), "wood", name="handle",
                hull_pad=True)
    b.add(ell((0.0145, 0.010, 0.0145), (0, -0.056, 0), sides=10, rings=6), "wood", name="knob", hull_pad=True)
    for y in (-0.032, 0.028):
        b.add(M.tube([(0, y - 0.002, 0), (0, y + 0.002, 0)], [0.0128, 0.0128], sides=10, dome0=0.0, dome1=0.0),
              "wood_dark", name="ring", outline=False, covered_by=hid)
    # bell: lathe along +Y (profile r over y), flared lip
    y0 = BELL_Y0
    prof = [(0.008, y0 - 0.002), (0.024, y0), (0.034, y0 + 0.010), (0.038, y0 + 0.030), (0.040, y0 + 0.058),
            (0.046, y0 + 0.082), (0.056, y0 + 0.100), (BELL_R, y0 + BELL_H), (BELL_R * 0.92, y0 + BELL_H + 0.003)]
    bell = M.lathe([(r, y) for r, y in prof], sides=20, cap0="fan", cap1="fan").rotate("x", -90)
    bid = b.add(bell, "bronze", name="bell", hull_pad=True)
    # the dark mouth (a recessed disc) + the clapper's ball hanging in it
    yl = y0 + BELL_H + 0.0015
    mouth = M.cylinder(BELL_R * 0.86, BELL_R * 0.86, -0.0008, 0.0008, sides=20).rotate("x", -90).translate((0, yl, 0))
    b.add(mouth, "bronze_dark", name="bell_mouth", outline=False, covered_by=bid)
    b.add(ell((0.013, 0.013, 0.013), (0, yl + 0.004, 0), sides=10, rings=6), "bronze_dark", name="clapper",
          hull_pad=True)
    # lip band and a cast band round the waist
    for y, r in ((y0 + 0.034, 0.0385), (y0 + BELL_H - 0.007, BELL_R * 0.975)):
        b.add(M.tube([(0, y - 0.002, 0), (0, y + 0.002, 0)], [r + 0.0012, r + 0.0012], sides=20, dome0=0.0,
                     dome1=0.0), "bronze_dark", name="band", outline=False, covered_by=bid)
    # the cord: a loop from the knob (the brief's "handbell on a cord")
    pts = [Vector((0.006 * math.sin(t), -0.060 - 0.030 * (1 - math.cos(t)) * 0.5, 0.022 * math.sin(t)))
           for t in np.linspace(0.0, 2 * math.pi, 12)]
    b.add(M.tube(pts, [0.0026] * 12, sides=6, dome0=0.0, dome1=0.0), "cord", name="cord", hull_pad=True)
    obj = _finish(b, pal, MARCH_PROPS["handbell"])
    return obj, [{"name": "bell", "pos": (0, y0 + BELL_H, 0)}, {"name": "tip", "pos": (0, y0 + BELL_H, 0)}]


# ── lantern ─────────────────────────────────────────────────────────────────────────────────────────────
def build_lantern(pal: Palette):
    """Hanging along +Y from the fist (the pouch convention): bail → cap → cage of four posts round the panes →
    base. 17 cm tall under the bail, 9 cm across."""
    b = _builder(MARCH_PROPS["lantern"], pal)
    hw = 0.042
    y_cap, y_top, y_bot, y_base = 0.055, 0.085, 0.175, 0.190
    # bail: a wire hoop from the ring in the fist down to the cap's flat sides (its ends sit on the pyramid's faces
    # 2 cm under its top)
    ye = y_cap + 0.020
    arc = [Vector((math.sin(t) * 0.031, 0.004 + (1 - math.cos(t)) * (ye - 0.004), 0.0))
           for t in np.linspace(-math.pi * 0.5, math.pi * 0.5, 11)]
    b.add(M.tube(arc, [0.0022] * len(arc), sides=6, dome0=0.0, dome1=0.0), "frame", name="bail", hull_pad=True)
    b.add(M.tube([(0, -0.006, 0), (0, 0.008, 0)], [0.006, 0.006], sides=8), "frame", name="bail_ring",
          hull_pad=True)
    # cap: a squat cone with a vent knob
    cap = M.lathe([(0.012, 0.0), (hw * 1.15, 0.026), (hw * 1.18, 0.030), (hw * 1.05, 0.032)], sides=4, exp=2.0,
                  cap0="fan", cap1="fan").rotate("x", -90).rotate("y", 45).translate((0, y_cap, 0))
    cid = b.add(cap, "frame", name="cap", hull_pad=True)
    b.add(ell((0.009, 0.009, 0.009), (0, y_cap - 0.002, 0), sides=8, rings=5), "frame", name="vent",
          outline=False, covered_by=cid)
    # panes: a warm box inside the cage (emissive: the glow itself is the runtime card)
    pane = M.box((hw * 1.7, y_bot - y_top, hw * 1.7), (0, (y_top + y_bot) / 2, 0), bevel=0.004)
    gid = b.add(pane, "glow", name="panes", hull_pad=True)
    # posts and rails
    for sx in (-1, 1):
        for sz in (-1, 1):
            b.add(M.box((0.008, y_bot - y_top + 0.012, 0.008), (sx * hw * 0.88, (y_top + y_bot) / 2, sz * hw * 0.88),
                        bevel=0.0015), "frame", name="post", hull_pad=True)
    for y in (y_top, y_bot):
        b.add(M.box((hw * 1.95, 0.008, hw * 1.95), (0, y, 0), bevel=0.002), "frame", name="rail", hull_pad=True)
    # cross wires over the panes (no hull: on the pane surface)
    for y in ((y_top + y_bot) / 2,):
        for ax in ("x", "z"):
            size = (hw * 1.78, 0.003, 0.003) if ax == "x" else (0.003, 0.003, hw * 1.78)
            for s in (-1, 1):
                off = (0, y, s * hw * 0.86) if ax == "x" else (s * hw * 0.86, y, 0)
                b.add(M.box(size, off, bevel=0.0005), "frame", name="wire", outline=False, covered_by=gid)
    b.add(M.box((hw * 2.05, 0.014, hw * 2.05), (0, y_base, 0), bevel=0.003), "frame", name="base", hull_pad=True)
    obj = _finish(b, pal, MARCH_PROPS["lantern"])
    return obj, [{"name": "glow", "pos": (0, (y_top + y_bot) / 2, 0)}, {"name": "tip", "pos": (0, y_base, 0)}]


# ── ledger ──────────────────────────────────────────────────────────────────────────────────────────────
def build_ledger(pal: Palette):
    """``props.build_book`` in ledger leather (the A1 ``ledger`` = folk LEATHER) with a cord tally ribbon."""
    b = _builder(MARCH_PROPS["ledger"], pal)
    h = 4.6 * U
    w = 3.4 * U
    open_deg = 15.0
    for sg in (1.0, -1.0):
        a = math.radians(open_deg) * sg
        d = Vector((sg * math.cos(a), 0, math.sin(abs(a))))
        c = d * (w * 0.5 + 0.2 * U) - Vector((-sg * math.sin(abs(a)), 0, math.cos(a))) * 0.25 * U
        fr = HB.basis(d, Vector((0, 1, 0)))
        fr.translation = c
        cov = M.box((w + 0.4 * U, 2 * h + 0.4 * U, 0.35 * U), (0, 0, 0), bevel=0.12 * U)
        b.add(cov.transform(fr), "leather", name="cover", hull_pad=True)
        pages = M.box((w - 0.1 * U, 2 * h - 0.2 * U, 0.9 * U), (0, 0, 0.5 * U), bevel=0.25 * U)
        pages.deform(lambda co: Vector((co.x, co.y, co.z + 0.35 * U * (1 - abs(co.x) / w) * (co.z > 0.5 * U))))
        b.add(pages.transform(fr), "paper", name="pages", hull_pad=True)
        # tally columns: short strokes in rows (the ration count), not text
        for i in range(5):
            y = -h * 0.62 + i * h * 0.30
            for j in range(4 - (i % 2)):
                x = -w * 0.32 + j * w * 0.17
                p0 = fr @ Vector((x, y - 0.55 * U, 1.0 * U))
                p1 = fr @ Vector((x + 0.1 * U, y + 0.55 * U, 1.0 * U))
                b.add(M.tube([p0, p1], [0.10 * U, 0.10 * U], sides=4, dome0=0.0, dome1=0.0), "ink_line", name="text",
                      outline=False)
    b.add(M.tube([(0, -h - 0.15 * U, -0.25 * U), (0, h + 0.15 * U, -0.25 * U)], [0.55 * U, 0.55 * U], sides=10,
                 dome0=0.1 * U, dome1=0.1 * U), "leather_dark", name="spine", hull_pad=True)
    b.add(M.tube([(0.0, h - 0.4 * U, 0.6 * U), (0.15 * U, h + 0.6 * U, 0.2 * U), (0.3 * U, h + 1.6 * U, -0.6 * U)],
                 [0.16 * U] * 3, sides=6, dome0=0.0, dome1=0.05 * U), "cord", name="ribbon", hull_pad=True)
    obj = _finish(b, pal, MARCH_PROPS["ledger"])
    return obj, [{"name": "tip", "pos": (0, 0, 1.0 * U)}]


# ── vial ────────────────────────────────────────────────────────────────────────────────────────────────
def build_vial(pal: Palette):
    """A stoppered phial held in the fist, neck along +Y: near-black glass (the black blood inside), a paper
    label, a cork."""
    b = _builder(MARCH_PROPS["vial"], pal)
    prof = [(0.004, -0.030), (0.020, -0.026), (0.023, -0.005), (0.023, 0.040), (0.016, 0.054), (0.0085, 0.064),
            (0.0085, 0.080)]
    body = M.lathe([(r, y) for r, y in prof], sides=12, cap0="fan", cap1="fan").rotate("x", -90)
    # pale glass (the shroud swatch) with the black blood filling its lower two thirds: a dark bottle vanished into
    # its own ink at 1080p
    vid = b.add(body, "shroud", name="vial", hull_pad=True)
    b.add(M.tube([(0, -0.027, 0), (0, 0.030, 0)], [0.0236, 0.0236], sides=12, dome0=0.0, dome1=0.0), "frame",
          name="blood", outline=False, covered_by=vid)
    b.add(M.tube([(0, 0.036, 0), (0, 0.046, 0)], [0.0216, 0.0216], sides=12, dome0=0.0, dome1=0.0), "paper",
          name="label", outline=False, covered_by=vid)
    b.add(M.tube([(0, 0.076, 0), (0, 0.093, 0)], [0.0095, 0.0088], sides=8, dome0=0.0, dome1=0.004), "wood",
          name="cork", hull_pad=True)
    obj = _finish(b, pal, MARCH_PROPS["vial"])
    return obj, [{"name": "tip", "pos": (0, 0.093, 0)}]


# ── barrow ──────────────────────────────────────────────────────────────────────────────────────────────
# body space (−Y forward): Bern stands at the origin; grips at his hands, the wheel ahead of him on a fork past the
# bed's front board (its front half shows past the shrouds' head ends: the spin reads from the game camera)
BARROW = dict(grip_x=0.215, grip_y=-0.385, grip_z=0.765, bed_y0=-0.70, bed_y1=-1.55, bed_z=0.46, bed_w=0.56,
              wheel_y=-1.80, wheel_r=0.20, leg_y=-0.76, load_y=-1.13)
WHEEL_HW = 0.028                # the wheel's half width along the axle
LEG_FOOT = (-0.72, 0.006)      # (y, z) of the back legs' feet at rest (on the ground under their 6 mm dome)
# Walk / Run (review: "Bern drags a wheelbarrow on its legs"): the barrow is pitched up about the wheel's axle until
# the legs clear the ground by LIFT_CLEAR (± the stride's bob), the grips riding up with it into his fists
LIFT_CLEAR = 0.10


def barrow_axle() -> Vector:
    """The wheel's axle (body space): the barrow's pivot and the head of its bone ``barrow_01``."""
    return Vector((0.0, BARROW["wheel_y"], BARROW["wheel_r"]))


def barrow_matrix(pitch_deg: float) -> Matrix:
    """The barrow's posed frame in body space: pitched ``pitch_deg`` about the axle (+ lifts the handles)."""
    ax = barrow_axle()
    return Matrix.Translation(ax) @ Matrix.Rotation(math.radians(pitch_deg), 4, "X") @ Matrix.Translation(-ax)


def barrow_grips(pitch_deg: float = 0.0) -> dict:
    B = BARROW
    m = barrow_matrix(pitch_deg)
    return {s: m @ Vector((sg * B["grip_x"], B["grip_y"], B["grip_z"])) for s, sg in (("l", 1.0), ("r", -1.0))}


def leg_clearance(pitch_deg: float) -> float:
    """Height of the back legs' feet over the ground with the barrow pitched ``pitch_deg``."""
    p = barrow_matrix(pitch_deg) @ Vector((0.0, LEG_FOOT[0], LEG_FOOT[1]))
    return p.z - LEG_FOOT[1]


def lift_pitch(clear: float = LIFT_CLEAR) -> float:
    """The pitch (deg) that lifts the legs ``clear`` metres off the ground."""
    lo, hi = 0.0, 20.0
    for _ in range(40):
        mid = (lo + hi) / 2
        if leg_clearance(mid) < clear:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2


def wheel_parts(lod: int = 0) -> list:
    """The barrow's solid wheel about the origin, axle along X: ``[(name, part, region, carries_hull)]``.

    A plank disc in an iron tyre with a batten cross and an iron hub on both faces: a four-fold pattern, so the spin
    reads at the game camera and the loops' quarter-turn multiples wrap without a pop (Walk 1 turn, Run 1½). The
    wheel is not part of ``SM_NPCKit_Prop_Barrow``: it is built into Bern's skeletal mesh, rigid on the spinning bone
    ``barrow_wheel_01`` (the runtime attaches one rigid prop; a spinning wheel needs a bone)."""
    r = BARROW["wheel_r"]
    hw = WHEEL_HW
    sides = 16 if lod else 24
    out = []
    tyre = M.cylinder(r, r, -hw * 0.82, hw * 0.82, sides=sides).rotate("y", 90)
    out.append(("wheel_tyre", tyre, "iron", True))
    disc = M.cylinder(r * 0.93, r * 0.93, -hw, hw, sides=sides).rotate("y", 90)
    out.append(("wheel", disc, "wood", True))
    for sx in (-1.0, 1.0):
        x = sx * (hw + 0.0025)
        for ang in (0.0, 90.0):
            bat = M.box((0.005, 2 * r * 0.86, 0.036), (0, 0, 0), bevel=0.0012).rotate("x", ang).translate((x, 0, 0))
            out.append(("wheel_batten", bat, "wood_dark", False))
        hub = M.cylinder(0.036, 0.030, 0.0, 0.014, sides=12 if lod else 16).rotate("y", 90 * sx).translate(
            (sx * hw, 0, 0))
        out.append(("wheel_hub", hub, "iron", False))
    return out


def _shroud_k(s: float) -> tuple[float, float]:
    """Width / height factors of a shrouded body at ``s`` (0 = the crown, 1 = the toes): head knob, neck pinch,
    shoulders, a taper to the ankles, the feet standing up under the cloth."""
    table = [(0.0, 0.34, 0.44), (0.05, 0.52, 0.64), (0.10, 0.50, 0.62), (0.155, 0.33, 0.46), (0.21, 0.88, 0.78),
             (0.31, 1.00, 0.90), (0.47, 0.84, 0.80), (0.57, 0.88, 0.78), (0.74, 0.64, 0.60), (0.89, 0.42, 0.46),
             (0.94, 0.44, 0.60), (1.0, 0.30, 0.54)]
    for (s0, w0, h0), (s1, w1, h1) in zip(table, table[1:]):
        if s <= s1:
            t = (s - s0) / max(s1 - s0, 1e-6)
            t = t * t * (3 - 2 * t)
            return w0 + (w1 - w0) * t, h0 + (h1 - h0) * t
    return table[-1][1], table[-1][2]


SHROUD_EXP = 2.6
SHROUD_HEX = "#9A9080"         # the barrow's shrouds (an existing NPC swatch; the vial keeps the pale #B8B0A0)


def _se(t: float) -> tuple[float, float]:
    """Superellipse (SHROUD_EXP) point at angle ``t``: (side, up)."""
    c, s = math.cos(t), math.sin(t)
    e = 2.0 / SHROUD_EXP
    return math.copysign(abs(c) ** e, c), math.copysign(abs(s) ** e, s)


def build_barrow(pal: Palette, wheel: bool = False):
    """Old Bern's barrow of shrouded dead: a plank bed (wood) with low sides on two back legs, two long handles rising
    to his grips, the axle between the handle ends (the wheel is Bern's: ``wheel_parts``; ``wheel=True`` adds it for
    the props sheet); three bodies wrapped in shrouds bound with cord at the neck, waist and ankles — a head, a neck,
    shoulders, a taper to the feet (aftermath, not gore: no limb shows) — stained with grave earth toward the bed."""
    b = _builder(MARCH_PROPS["barrow"], pal)
    # the shroud a value step down (review 4: the pale #B8B0A0 cloth on a 1.9 m bed was the brightest, largest mass in
    # Bern's frame): an existing greige linen (#9A9080, Vira's sleeve swatch), the fold line kept
    b.regions(**outline.ink_regions(dict(earth=Region(K["cord"].hex, K["cord"].s, K["cord"].l),
                                         shroud=Region(SHROUD_HEX, .42, .32)), "npc"))
    B = BARROW
    w = B["bed_w"]
    y0, y1, zb = B["bed_y0"], B["bed_y1"], B["bed_z"]
    L = y0 - y1
    # handles: from the grips forward along the bed's sides to the wheel fork
    for sg in (1.0, -1.0):
        g = Vector((sg * B["grip_x"], B["grip_y"], B["grip_z"]))
        p1 = Vector((sg * (w / 2 - 0.01), y0 + 0.02, zb + 0.01))
        p2 = Vector((sg * (w / 2 - 0.02), y1 + 0.05, zb - 0.02))
        p3 = Vector((sg * 0.07, B["wheel_y"], B["wheel_r"]))
        path = HB.catmull([g + Vector((0, 0.07, 0.01)), g, p1, p2, p3], 4)
        hid = b.add(M.tube(path, [0.019] * len(path), sides=8, dome0=0.01, dome1=0.0), "wood", name="handle",
                    hull_pad=True)
        b.add(M.tube([g + Vector((0, 0.055, 0.008)), g - Vector((0, 0.055, 0.008))], [0.0215, 0.0215], sides=8,
                     dome0=0.0, dome1=0.0), "leather", name="grip_wrap", outline=False, covered_by=hid)
        # back leg under the bed's rear corner
        top = Vector((sg * (w / 2 - 0.03), B["leg_y"], zb - 0.01))
        b.add(M.tube([top, Vector((sg * (w / 2 + 0.02), LEG_FOOT[0], LEG_FOOT[1]))], [0.020, 0.017], sides=8,
                     dome0=0.0, dome1=0.006), "wood", name="leg", hull_pad=True)
    # bed: planks with a gap, low side boards
    bed = M.box((w, L, 0.030), (0, (y0 + y1) / 2, zb), bevel=0.006)
    bid = b.add(bed, "wood", name="bed", hull_pad=True)
    for x in np.linspace(-w / 2 + w / 4, w / 2 - w / 4, 3):
        b.add(M.box((0.004, L - 0.02, 0.004), (x, (y0 + y1) / 2, zb + 0.016), bevel=0.0008), "wood_dark",
              name="seam", outline=False, covered_by=bid)
    for sg in (1.0, -1.0):
        b.add(M.box((0.020, L, 0.090), (sg * (w / 2 + 0.002), (y0 + y1) / 2, zb + 0.050), bevel=0.005), "wood",
              name="side_board", hull_pad=True)
    b.add(M.box((w, 0.020, 0.11), (0, y1 + 0.01, zb + 0.06), bevel=0.005), "wood", name="front_board",
          hull_pad=True)
    # the axle through the handle ends (the wheel turns on it)
    r = B["wheel_r"]
    b.add(M.tube([(-0.078, B["wheel_y"], r), (0.078, B["wheel_y"], r)], [0.013, 0.013], sides=8, dome0=0.004,
                 dome1=0.004), "metal_dark", name="axle", hull_pad=True)
    if wheel:
        for nm, part, reg, hull in wheel_parts():
            part.translate(barrow_axle())
            b.add(part, {"iron": "metal_dark", "wood": "wood", "wood_dark": "wood_dark"}[reg], name=nm,
                  **({"hull_pad": True} if hull else {"outline": False}))
    # three shrouded bodies, alternating head to foot on the bed (the bottom two side by side, the third on them)
    # (the load sits back toward Bern: the wheel's front half shows past the head ends, its spin reads)
    bodies = [(-0.135, 0.0, 1.26, 3.0, False), (0.135, 0.0, 1.20, -2.0, True), (0.0, 0.155, 1.30, 1.5, False)]
    hw, hh = 0.13, 0.085
    for i, (x, dz, ln, yaw, flip) in enumerate(bodies):
        cy = B["load_y"] - 0.03 * i
        z_bed = zb + 0.015 + dz
        dv = Vector((math.sin(math.radians(yaw)), -math.cos(math.radians(yaw)), 0.0))     # crown → toes
        if flip:
            dv = -dv
        c = Vector((x, cy, z_bed))
        a = c - dv * ln / 2                              # the crown
        side = dv.cross(Vector((0, 0, 1))).normalized()
        n = 26

        def centre(s, a=a, dv=dv, ln=ln, z_bed=z_bed):
            wk, hk = _shroud_k(s)
            p = a + dv * (ln * s)
            return Vector((p.x, p.y, z_bed + hh * hk))
        ss = [0.0, 0.03, 0.06, 0.10, 0.135, 0.155, 0.175, 0.21, 0.26, 0.31, 0.39, 0.47, 0.52, 0.57, 0.66, 0.74, 0.82,
              0.89, 0.92, 0.95, 0.98, 1.0]
        path = [centre(s) for s in ss]
        scales = [(hh * _shroud_k(s)[1], hw * _shroud_k(s)[0]) for s in ss]
        prof = M.superellipse(1, 1, 12, SHROUD_EXP)
        sh = M.sweep(path, prof, scales=scales, up=(0, 0, 1), dome0=0.035, dome1=0.03)
        sid = b.add(sh, "shroud", name="shroud", hull_pad=True)

        def surf(s, t, d, centre=centre, side=side):
            wk, hk = _shroud_k(s)
            u, v = _se(t)
            return centre(s) + side * (u * (hw * wk + d)) + Vector((0, 0, v * (hh * hk + d)))
        # cords at the neck, the waist and the ankles
        for s in (0.155, 0.50, 0.885):
            ring = [surf(s, t, 0.0025) for t in np.linspace(0, 2 * math.pi, 12, endpoint=False)]
            b.add(M.sweep(ring, M.superellipse(1, 1, 4, 2.0), scales=[(0.0045, 0.0045)] * 12, closed_path=True,
                          up=tuple(dv), cap0=None, cap1=None), "cord", name="shroud_cord", outline=False,
                  covered_by=sid)
        # a fold line along the top, from the chest to the shins
        top = [surf(f, math.pi / 2 + 0.25 * math.sin(f * 9.0 + i), 0.0012) for f in np.linspace(0.24, 0.80, 7)]
        b.add(M.tube(top, [0.0035] * 7, sides=4, dome0=0.0, dome1=0.0), "shroud_line", name="fold", outline=False,
              covered_by=sid)
        # grave earth soaked up from the bed: the lower third of the cloth all along, rising ragged up the sides
        # toward the feet (the pale cloth no longer the largest bright mass in Bern's frame)
        rows, cols = 8, 6           # (budget: the barrow stays ~4k toon triangles with the bodies' shapes)
        outer, inner = [], []
        for rr in range(rows + 1):
            s = 0.02 + 0.96 * rr / rows
            reach = 1.20 + 0.32 * s + 0.16 * math.sin(s * 23.0 + 2.1 * i) + 0.08 * math.sin(s * 57.0 + i)
            ro, ri = [], []
            for cc in range(cols + 1):
                u = cc / cols * 2 - 1                      # −1 … 1 round the underside
                t = -math.pi / 2 + u * (math.pi / 2) * min(1.62, reach)
                ro.append(surf(s, t, 0.0022))
                ri.append(surf(s, t, 0.0008))
            outer.append(ro)
            inner.append(ri)
        b.add(C.thick_patch(outer, inner), "earth", name="earth_stain", outline=False, covered_by=sid)
    obj = _finish(b, pal, MARCH_PROPS["barrow"])
    g = barrow_grips()
    return obj, [{"name": "handle_l", "pos": tuple(g["l"])}, {"name": "handle_r", "pos": tuple(g["r"])},
                 {"name": "wheel", "pos": tuple(barrow_axle())}, {"name": "tip", "pos": (0.0, B["wheel_y"], 0.0)}]


# ── Tamsin's grave spade ────────────────────────────────────────────────────────────────────────────────
SPADE_TIP_Y = 1.06              # grip → blade tip (m): planted before her, the grip at her chest as she leans on it


def build_spade(pal: Palette):
    """The gravewarden's spade (weapon-bone space, held at the top of the shaft under its T-grip, +Y down the shaft,
    +Z the blade's face): ash-wood shaft, an iron collar and a squared iron blade caked with grave earth."""
    b = _builder(MARCH_PROPS["spade"], pal)
    b.regions(**outline.ink_regions(dict(earth=Region(K["stain"].hex, K["stain"].s, K["stain"].l)), "npc"))
    sid = b.add(M.tube([(0, -0.050, 0), (0, 0.40, 0), (0, 0.80, 0)], [0.0165, 0.0160, 0.0170], sides=10,
                       dome0=0.0, dome1=0.0), "wood", name="shaft", hull_pad=True)
    b.add(M.capsule((-0.060, -0.062, 0), (0.060, -0.062, 0), 0.018, 0.018, sides=8, cap_rings=1, mid=1), "wood",
          name="t_grip", hull_pad=True)
    b.add(M.tube([(0, -0.052, 0), (0, -0.040, 0)], [0.020, 0.020], sides=10, dome0=0.0, dome1=0.0), "wood_dark",
          name="ring", outline=False, covered_by=sid)
    b.add(M.tube([(0, 0.74, 0), (0, 0.80, 0), (0, 0.85, 0)], [0.021, 0.024, 0.030], sides=10, dome0=0.0,
                 dome1=0.0), "metal_dark", name="collar", hull_pad=True)
    # blade: a squared spade, the top edge turned for the foot, a worn edge
    hw, yt, yb = 0.095, 0.845, SPADE_TIP_Y
    outline2 = [(-hw, yt), (hw, yt), (hw * 0.98, yb - 0.05), (hw * 0.80, yb - 0.008), (0.0, yb),
                (-hw * 0.80, yb - 0.008), (-hw * 0.98, yb - 0.05)]
    def dish(x):
        return 0.010 * (1 - min(1.0, (x / hw) ** 2))
    blade = M.plate(outline2, 0.008, bevel=0.0015)
    blade.deform(lambda co: Vector((co.x, co.y, co.z + dish(co.x))))
    bl = b.add(blade, "metal_dark", name="blade", hull_pad=True)
    b.add(M.box((2 * hw, 0.016, 0.014), (0, yt + 0.006, 0.004), bevel=0.003), "metal_dark", name="tread",
          outline=False, covered_by=bl)
    # earth caked on both faces of the blade's lower half, ragged at its top
    for sz in (1.0, -1.0):
        pts = [(-hw * 0.92, yb - 0.06), (-hw * 0.5, yb - 0.115), (-hw * 0.1, yb - 0.085), (hw * 0.3, yb - 0.13),
               (hw * 0.7, yb - 0.095), (hw * 0.92, yb - 0.11), (hw * 0.90, yb - 0.04), (hw * 0.72, yb - 0.012),
               (0.0, yb - 0.004), (-hw * 0.72, yb - 0.012)]
        mud = M.plate(pts, 0.002)
        mud.deform(lambda co, sz=sz: Vector((co.x, co.y, co.z + sz * 0.0052 + dish(co.x))))
        b.add(mud, "earth", name="mud", outline=False, covered_by=bl)
    obj = _finish(b, pal, MARCH_PROPS["spade"])
    return obj, [{"name": "tip", "pos": (0, SPADE_TIP_Y, 0)}, {"name": "grip_top", "pos": (0, -0.062, 0)}]


# ── Corran's old sword (a worn, notched variant of the plains sword) ────────────────────────────────────
def build_sword_notched(pal: Palette):
    """``props.build_sword`` worn by a deserter's war: the same length and grip (Work_Guard leans on its point), the
    blade's edges hacked with notches and the point chipped, a plain iron cross-guard and pommel (the gilt long
    gone), the grip rebound with cord."""
    b = M.Builder(MARCH_PROPS["sword_notched"], pal, prefix="npcprop.swordnotched")
    b.regions(**P.PROP_REGIONS)
    b.regions(**outline.ink_regions(dict(cord=Region(K["cord"].hex, K["cord"].s, K["cord"].l)), "npc"))
    U_ = P.U
    gy = 2.3 * U_
    L = P.SWORD_BLADE
    w0, w1 = 1.15 * U_, 0.95 * U_
    t = 0.30 * U_
    rows = 44
    # notches: (position along the blade 0..1, edge +1 / −1, depth as a share of the half width, half length)
    notches = [(0.22, 1, 0.45, 0.022), (0.37, -1, 0.55, 0.026), (0.48, 1, 0.35, 0.018), (0.60, -1, 0.40, 0.020),
               (0.69, 1, 0.60, 0.024), (0.80, -1, 0.30, 0.016)]

    def cut(f, sg):
        k = 1.0
        for fn, s, dep, hl in notches:
            if s == sg:
                x = abs(f - fn) / hl
                if x < 1.0:
                    k = min(k, 1.0 - dep * (1.0 - x))
        return k
    bm = bmesh.new()
    ring = []
    for i in range(rows + 1):
        f = i / rows
        y = gy + 0.3 * U_ + L * f
        w = w0 + (w1 - w0) * f
        if f > 0.84:
            w *= max(0.05, 1 - ((f - 0.84) / 0.16) ** 1.2)
        th = t * (1 - 0.6 * max(0.0, f - 0.84) / 0.16)
        ring.append([bm.verts.new((w * cut(f, 1), y, 0)), bm.verts.new((0, y, th)),
                     bm.verts.new((-w * cut(f, -1), y, 0)), bm.verts.new((0, y, -th))])
    for r0, r1 in zip(ring, ring[1:]):
        for j in range(4):
            bm.faces.new((r0[j], r0[(j + 1) % 4], r1[(j + 1) % 4], r1[j]))
    bm.faces.new(list(reversed(ring[0])))
    # the chipped point: blunt and off-centre
    tipv = bm.verts.new((0.25 * U_, gy + 0.3 * U_ + L + 0.15 * U_, 0))
    for j in range(4):
        bm.faces.new((ring[-1][j], ring[-1][(j + 1) % 4], tipv))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    blid = b.add(M.Part(bm).flat(), "blade", name="blade", hull_pad=True)
    for sg in (1.0, -1.0):
        b.add(M.tube([(0, gy + 1.0 * U_, sg * t * 0.55), (0, gy + L * 0.75, sg * t * 0.45)], [0.18 * U_, 0.12 * U_],
                     sides=4, dome0=0.0, dome1=0.0), "blade_edge", name="fuller", outline=False, covered_by=blid)
    # a few dark nicks and scratches on the flats
    for f, x, ln, ang in ((0.30, 0.3, 0.030, 20.0), (0.52, -0.2, 0.024, -25.0), (0.66, 0.35, 0.020, 35.0)):
        y = gy + 0.3 * U_ + L * f
        for sz in (1.0, -1.0):
            d = Vector((math.sin(math.radians(ang)), math.cos(math.radians(ang)), 0.0)) * ln / 2
            c = Vector((x * w0, y, sz * t * 0.62))
            b.add(M.tube([c - d, c + d], [0.0012, 0.0012], sides=4, dome0=0.0, dome1=0.0), "metal_dark", name="scratch",
                  outline=False, covered_by=blid)
    # plain iron cross-guard, bent a touch
    b.add(M.tube([(-3.2 * U_, gy + 0.35 * U_, 0), (0, gy, 0), (3.2 * U_, gy - 0.25 * U_, 0)],
                 [0.50 * U_, 0.55 * U_, 0.50 * U_], sides=8, dome0=0.4 * U_, dome1=0.4 * U_), "metal_dark", name="guard",
          hull_pad=True)
    gid = b.add(M.tube([(0, -2.4 * U_, 0), (0, gy - 0.3 * U_, 0)], [0.62 * U_, 0.58 * U_], sides=10, dome0=0.0,
                       dome1=0.0), "leather", name="grip", hull_pad=True)
    for y in np.linspace(-2.0 * U_, gy - 0.6 * U_, 6):
        b.add(M.tube([(0, y - 0.09 * U_, 0), (0, y + 0.09 * U_, 0)], [0.67 * U_, 0.67 * U_], sides=10, dome0=0.0,
                     dome1=0.0), "cord", name="wrap_line", outline=False, covered_by=gid)
    b.add(ell((0.90 * U_, 0.80 * U_, 0.90 * U_), (0, -2.9 * U_, 0), sides=12, rings=8), "metal_dark", name="pommel",
          hull_pad=True)
    obj = _finish(b, pal, MARCH_PROPS["sword_notched"])
    return obj, [{"name": "tip", "pos": (0, gy + L + 0.9 * U_, 0)}, {"name": "mid", "pos": (0, gy + L * 0.5, 0)},
                 {"name": "pommel", "pos": (0, -2.9 * U_, 0)}]


# ── charred anvil stump ─────────────────────────────────────────────────────────────────────────────────
def build_anvil(pal: Palette):
    """``props.build_anvil``'s stump and anvil (same footprint, the strike socket at the same place) with the stump
    charred: black bark split by fire, a burnt cut top; deeper fire cracks."""
    b = M.Builder(ANVIL, pal, prefix="npcprop.march_anvil")
    b.regions(**ANVIL_REGIONS)
    u = 0.0319
    r = 4.0 * u
    top = 12.2 * u
    prof = []
    for i in range(9):
        z = top * i / 8
        rr = r * (1.0 + 0.16 * (1 - i / 8) ** 3) + 0.004 * math.sin(i * 1.7)
        prof.append((z, rr, rr * 0.94))
    stump = M.loft_z([(z, rx, ry) for z, rx, ry in prof], sides=18, exp=2.0, cap0="ngon", cap1="ngon")
    sid = b.add(stump, "bark", name="stump", hull_pad=True)
    for k in range(11):
        a = math.radians(14 + k * 33)
        pts = [Vector((math.cos(a) * r * (1.17 - 0.16 * t), math.sin(a) * r * 0.94 * (1.17 - 0.16 * t), top * t))
               for t in np.linspace(0.03, 0.92, 6)]
        pts = [p * (1.0 + 0.012) for p in pts]
        b.add(M.tube(pts, [0.0045] * 6, sides=4, dome0=0.0, dome1=0.0), "bark_dark", name="crack", outline=False,
              covered_by=sid)
    for k in range(4):
        a = math.radians(40 + k * 90)
        d = Vector((math.cos(a), math.sin(a), 0))
        b.add(M.tube([d * r * 0.9 + Vector((0, 0, 0.06)), d * r * 1.35 + Vector((0, 0, 0.012)),
                      d * r * 1.6 + Vector((0, 0, -0.004))], [0.032, 0.022, 0.012], sides=8, dome0=0.0, dome1=0.008),
              "bark", name="root", hull_pad=True)
    top_disc = M.cylinder(r * 0.97, r * 0.97, top - 0.004, top + 0.006, sides=18)
    b.add(top_disc, "wood_top", name="cut_top", outline=False, covered_by=sid)
    for rr in (0.35, 0.65):
        ring = M.tube([Vector((math.cos(t) * r * rr, math.sin(t) * r * rr, top + 0.0065))
                       for t in np.linspace(0, 2 * math.pi, 20)], [0.0022] * 20, sides=4, dome0=0.0, dome1=0.0)
        b.add(ring, "rings", name="tree_ring", outline=False, covered_by=sid)
    # a burnt split across the top
    b.add(M.tube([Vector((-r * 0.8, -r * 0.1, top + 0.0068)), Vector((0.0, 0.0, top + 0.0068)),
                  Vector((r * 0.75, r * 0.2, top + 0.0068))], [0.004, 0.0035, 0.002], sides=4, dome0=0.0, dome1=0.0),
          "bark_dark", name="crack", outline=False, covered_by=sid)
    hw = 2.3 * u
    at = 17.0 * u
    prof2 = [(-6.0, 0.0), (3.4, 0.0), (7.5, 1.0), (3.2, 2.4), (2.2, 5.0), (-2.4, 5.0), (-3.0, 2.4), (-5.6, 1.6)]
    outline2 = [(-px * u, at - py * u) for px, py in prof2]
    outline2 = C.ccw(C.smooth_closed(outline2, 3))
    body = C.plate2d(outline2, 2 * hw, bevel=0.004)
    body.transform(Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
    body.deform(lambda co: Vector((co.x * (1.0 - 0.75 * max(0.0, (-co.y / u - 3.2) / 4.3)), co.y, co.z)))
    aid = b.add(body, "iron", name="anvil", hull_pad=True)
    face = M.box((hw * 1.9, 9.2 * u, 0.006), (0, 1.3 * u, at + 0.001), bevel=0.0015)
    b.add(face, "iron_face_worn", name="anvil_face", outline=False, covered_by=aid)
    b.add(M.box((hw * 2.3, 6.4 * u, 0.8 * u), (0, 0.2 * u, top + 0.4 * u), bevel=0.003), "iron", name="anvil_base",
          hull_pad=True)
    obj = asset.finish_mesh(b, pal, "npc")
    obj.name = ANVIL
    return obj, [{"name": "strike", "pos": (0.0, 1.0 * u, at + 0.004)}]


BUILDERS = {"handbell": build_handbell, "lantern": build_lantern, "ledger": build_ledger, "vial": build_vial,
            "barrow": build_barrow, "spade": build_spade, "sword_notched": build_sword_notched}


def build_prop(kind: str, pal: Palette):
    """→ (object, sockets): a march prop, else the plains one (``props.build_prop``)."""
    if kind in BUILDERS:
        return BUILDERS[kind](pal)
    return P.build_prop(kind, pal)
