"""Set pieces of the march (dark remake 焚境 · The Burning March, package A10 part 2; implementation-plan.md §4 U4 /
§5 A10; chapter1.md §1, §2, §5, §9.1, §10, §12; story-bible.md tone pillars: aftermath not gore, ash not blood,
ember orange only for the hero / loot / refuges / interaction, breach violet for the breach itself).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/setpieces_march.py [names…]
        [--quick] [--sheet] [--scenes] [--echo] [--family] [--no-previews] [--no-verify]

Every asset ships through the kit's environment pipeline (``envkit.ship``: FBX + merged manifest entry with game
ids, footprint, blocking, sockets, palette slots, the review set on the march ground with the ink gate recorded),
plus a LOD1 FBX (``lods[]`` + ``lod1.png``), the reserved-light check (``envkit_march.check_light``) and a re-import
check of both FBX files (:func:`verify_static`). The UE set-piece layer (U4, ``set_pieces.json`` → prop groups at
tiles, non-blocking by default) places them by game id; the ids below are the art side of that contract.

| Asset | game ids | where (chapter1.md) | look |
|---|---|---|---|
| ``SM_Prop_March_AshwheatCottage_A/B`` | ``goblin_hut@march`` (the goblin-camp slot at (40,52) *is* Ashwheat), ``setpiece_ashwheat_cottage`` | Ashwheat (40,52) r8 | low wattle-and-daub cottage, charred corner posts, burnt thatch (A: a hole burned through over the window; B: the thatch gone from half the roof, bare charred rafters; under the holes a charred loft floor, L* ~17, with fallen rafters and ash), the door shut and barred **from inside** (nothing nailed outside), a pale cold candle in the window (``cottage.candle``: not ember — the reserved light), soot over the openings; sockets ``knock`` (the door's knocking emitter), ``candle`` |
| ``SM_Prop_March_BurnedBridge`` | ``setpiece_burned_bridge`` | (38,48), north of Ashwheat | the bridge the Captain burned: the near (−X) dry-stone abutment whole (three courses, wing walls) with the deck stub on its stringers, the far one a low ruin with its stringer fallen diagonally into the dry ditch, charred piling stumps, loose planks, ash; the ditch (``_Ditch``) with broken bank lips |
| ``SM_Prop_March_SheepfoldWagon_A/B`` | ``setpiece_sheepfold_wagon`` | the Sheepfold (60,55), ring r6 | overturned crusade supply wagons (A on its side, the bed open toward the ring; B upside down and rolled onto a side edge, axles and wheels tipped to the front, the front end propped on its broken wheel, the tongue hanging), crimson-painted boards, iron fittings |
| ``SM_Prop_March_BreachDais`` | ``setpiece_breach_dais`` | the Breach Rim (15,95) | the Seal of Veins: a low round dais of dark stone slabs (top 6 cm), a carved seal of rings and veins at its centre, split by the breach crack glowing breach violet from below (``dais.glow`` / ``dais.vein``), heaved slabs, a broken kerb and three pillar stumps; socket ``breach`` |
| ``SM_Prop_March_RitualCircle`` | ``setpiece_ritual_circle`` | (11,91), (19,99) | a scuffed ring of ash (L* ~45) poured on scorched ground, seven uneven spokes (two broken) to an inner arc, ash heaps with burnt stakes and crimson rags; socket ``raise`` (where the standard-bearer rises) |
| ``SM_Prop_March_RingBanner_A/B`` | ``setpiece_banner_ring`` | ring of 12 at r9 round the dais | a scorched crusade banner on a planted spear (cloth sways: ``MI_AF_Toon_Props_March_Wind``), burnt hem and tails, the crusade's open palm faded on both faces; socket ``flame`` (P3: the ring ignites) |
| ``SM_Prop_March_HelmetStake_A/B`` | ``setpiece_helmet_stake`` | the Dead Ranks approach | A: a crusade kettle helm tilted on a planted spear, the blade through its crown, a crimson strap and rag; B: three spears, two helms (one pierced), a fallen helm, a split shield at their foot |
| ``SM_Prop_March_BoneyardGate`` | ``decor_defend_boneyard_gate`` (M5's ``defendTarget.spriteKey``), ``setpiece_boneyard_gate`` | the Boneyard Gate (10,32) | where the burial road leaves Knellwatch: a roofed timber lychgate (four posts, a charred plank roof), shut leaves with crusade shields nailed on, the burial bell on a bracket off the front tie beam below the eave, pew-stake wings; no bones (Chapter 1 has none) |
| ``SM_Prop_March_PitMouthSealed`` | ``setpiece_pit_mouth_sealed`` | the Pit entrance (17,26), sealed until W3 | the burn pit's mouth: a ring of stones and ash round a black opening, a timber collar, charred planks and an iron grate chained over it; faint smoulder (``brazier.coals``, no light) and a breach seep (``pit.seep``) in the gaps; sockets ``smoke``, ``entrance`` (the subDungeonEntrances point) |
| ``SM_Prop_March_WagonBarricade`` | ``exit_gate_sealed@march``, ``exit_sealed@march`` | the east road exit (119,60), W7 | two wagons side-on, crusade shields nailed points-down, tied sacks and tallied crates heaped in their beds, a pew and sharpened stakes; the exit's breach fire plays beyond it at ``fx_vortex`` (same footprint / blocking / socket as the plains sealed gate) |
| ``SM_Prop_March_OrderPost`` | ``lore_ancient_tablet@march``, ``lore_order_post`` | ``lore_rf_01`` (45,35) | the Marshal's Last Order nailed to a post: parchment, three lines, his crimson wax seal; one small warm glyph (``lore.glyph``, interaction) |
| ``SM_Prop_March_NameWall`` | ``setpiece_name_wall`` | Knellwatch, the inner palisade | hundreds of bronze name-tags (``namewall.tag``) nailed in rows on a stretch of the inner palisade, a few hanging on their cords, a shelf of unclaimed bundles below |

The **echo material** (``--echo``): the lore ``echo`` look (U1: a translucent figure frozen in its last pose for 3 s)
is the soul echo's master ``M_AF_Ghost`` with the march's cold ``echo.*`` colours — the manifest section
``materials.MI_AF_Echo`` (parent, tint, timing, the per-echo figure and pose) and ``Previews/setpieces_march/echo_*`` (each of the
three echoes beside its set piece at the game camera, the ghost maths emulated exactly).

Ink: decor 1.4 px (``Props_March``, R14), the barricade and the order post interactive 2.0 px. Reserved light: no
ember on any set piece but the order post's glyph; breach violet only on the dais and the pit seep.
"""
from __future__ import annotations

import json
import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit_march as EM  # noqa: E402  (first: puts the march stage in place)
import envkit as E  # noqa: E402
import foliage as F  # noqa: E402
import story as ST  # noqa: E402
import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import export, outline, paths, pngio, review, scene, theme  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"
FAM = EM.PROPS
FAMILY = "environment.setpieces_march"
SCRATCH = paths.scratch_root("a10sp")


def reg(name: str) -> Region:
    return EM.region(FAM, name)


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


def V(x, y, z) -> Vector:
    return Vector((x, y, z))


# ── regions ─────────────────────────────────────────────────────────────────────────────────────────────
# A1's swatches by role (march_palettes.PROPS), plus the few this generator appends (EXTRA_REGIONS: line tones of
# planks and stones, thin parts' darker outline mix, the cold candle, the ditch bed).
REGIONS = dict(
    wood=reg("wagon.wood"), wood_dark=reg("wagon.wood_dark"), burnt=reg("bridge.burnt"),
    charred=reg("camp.charred"), charred_line=reg("camp.charred_line"), timber=reg("camp.timber"),
    timber_end=reg("camp.timber_end"), timber_line=reg("camp.timber_line"), pew=reg("camp.pew"),
    pew_worn=reg("camp.pew_worn"), shield=reg("camp.shield"), shield_rim=reg("camp.shield_rim"),
    helmet=reg("camp.helmet"), iron=reg("camp.iron"), iron_dark=reg("camp.iron_dark"), nail=reg("camp.nail"),
    rope=reg("camp.rope"), canvas=reg("camp.canvas"), canvas_dark=reg("camp.canvas_dark"),
    sack=reg("camp.sack"), chalk=reg("camp.chalk"), stone=reg("camp.stone"), stone_top=reg("camp.stone_top"),
    stone_line=reg("camp.stone_line"), ash=reg("camp.ash"), soot=reg("camp.soot"),
    bronze=reg("bell.bronze"), bronze_dark=reg("bell.bronze_dark"), patina=reg("bell.patina"),
    hearth_stone=reg("hearth.stone"), coals=reg("brazier.coals"),
    thatch=reg("cottage.thatch"), thatch_lit=reg("cottage.thatch_lit"), daub=reg("cottage.daub"),
    door=reg("cottage.door"),
    pit_stone=reg("pit.stone"), pit_dark=reg("pit.dark"), pit_seep=reg("pit.seep"),
    dais=reg("dais.stone"), dais_top=reg("dais.stone_top"), dais_crack=reg("dais.crack"),
    dais_glow=reg("dais.glow"), dais_vein=reg("dais.vein"),
    ritual_ash=reg("ritual.ash"), ritual_char=reg("ritual.char"),
    ring_cloth=reg("bannerring.cloth"), ring_char=reg("bannerring.char"), stake=reg("stake.wood"),
    tag=reg("namewall.tag"), tag_dark=reg("namewall.tag_dark"),
    post=reg("orderpost.wood"), paper=reg("orderpost.paper"), ink=reg("orderpost.ink"), seal=reg("orderpost.seal"),
    glyph=reg("lore.glyph"),
    ground_dark=reg("ground.dark"), ground_light=reg("ground.light"),
)
EXTRA_REGIONS = dict(
    wood_line=Region("#5A4836").line_region(),
    burnt_line=Region("#1E1816").line_region(),
    daub_shade=Region("#6E665A").shade_region(.30, .12),
    thatch_line=Region("#3E362C").line_region(),
    dais_line=Region("#3E3A42").line_region(),
    pit_line=Region("#48443F").line_region(),
    stake_line=Region("#4A3C30").line_region(),
    pole=Region("#4A3C30", .48, .24, o=.55),          # thin poles / spear shafts: darker outline over the lit edge
    bar=Region("#4A3C30", .48, .18, o=.3),
    chain=Region("#4A4C52", .46, .40, o=.4),
    candle=Region("#D2CCB8", 0.0, 0.0, e=0.55),        # Ashwheat's candles: pale and cold (not the ember band)
    candle_wax=Region("#B8B2A2", .30, .20),
    window=Region("#141012", .30, .08),                # the dark behind the shutters
    ditch=Region("#2A2622", .40, .10),                 # the dry ditch bed under the burned bridge
    ash_pale=Region("#9A948A", .38, .14),              # poured ash (ritual circle spokes, heaps' tops)
    char_black=Region("#241C18", .45, .14),            # burned-through timber: its lit top stays dark
    mud_crack=Region("#1A1614", .30, .08),             # cracks in the dry ditch bed
    dais_slab=Region("#3A363E", .46, .17),             # the dais slabs: cold dark stone, a restrained lit top
    dais_slab_top=Region("#4A4650", .44, .18),         # the seal stone (a touch lighter: the eye goes there)
    sigil_faded=Region("#8E8270", .42, .18),           # the church sigil, faded on the crusade banners
    helm_iron=Region("#46484E", .48, .24),             # dull helmet iron (a restrained lit band: not a pale bowl)
    wood_old=Region("#4E4438", .48, .19),              # weathered, ash-dusted wagon boards (a restrained lit top)
    wood_old_dark=Region("#30281F", .48, .17),
    # art review r1 (appended)
    ash_poured=Region("#7A756E", .38, .12),            # the ash circle's ring and spokes (renders L* ~46 on the
                                                       # L* 31 ground, was 55-62: the Rim's brightest mark, a wheel)
    floor_char=Region("#302824", .45, .12),            # a cottage's burned loft floor seen through the roof (L* 17)
    roof_char=Region("#3A302A", .50, .14),             # the lychgate's roof planks: three tones ±3 L* (they
    roof_char_lo=Region("#352C26", .50, .14),          # alternated near-black / mid-brown, a barcode over the
    roof_char_hi=Region("#40352D", .50, .14),          # shielded leaves)
    sack_crease=Region("#5E5546").shade_region(.30, .12),   # the crease across a slumped sack (camp.sack's shade)
    bronze_dull=Region("#64553C", .48, .08),           # = camp_march's bell body: the lychgate bell joins its swatch
)
ALL_REGIONS = {**REGIONS, **EXTRA_REGIONS}


# Pad gate: a padded hull proxy (``hull_pad``) grows locally round whatever stands proud of it, by pushing the
# nearest proxy face's corners out along their normals — on a coarse proxy (an 8-corner box, a 7-sided tube, a flat
# n-gon) a few cm of trim pushes a whole corner out into a black wedge of ink (the first cottage / wagon / banner
# builds: 18–50 cm). Every padded part of every set piece must grow ≤ PAD_MAX_MM; details inside a face carry no
# cover, structural trims their own ink, and a part whose frame stands proud gets an explicit ``hull=`` proxy.
PAD_MAX_MM = 25.0
BUILT: dict = {}                    # (asset, lod) → its Builder (parts with padMm after build)
LOD = 0                             # the LOD the generator of the moment builds (set by :func:`new`; helpers read it)


class SPBuilder(M.Builder):
    """``kit.mesh.Builder`` with the set pieces' LOD1 rebuilt **part by part** (art review r1: whole-mesh collapse
    decimation turned the flat decals and seal plates into lumpy domes — 13-34 % of their toon loops > 60° off their
    face, a few hundred flipped): parts ``keep(name)`` accepts (decals, flat ground plates, the seal and void plates,
    the breach cores, the pit seep, helm brims) keep their LOD0 topology; every other part is collapse-decimated to
    ``lod_ratio`` and its shading normals re-derived from the decimated geometry (:func:`repair_normals`: a flat
    part stays flat, a smoothed one gets its hard edges re-marked) — the decimator drops the sharp-edge flags, so a
    4 mm plate smoothed its top into its rim and its bottom."""

    def __init__(self, *a, keep=None, **kw):
        super().__init__(*a, **kw)
        self.keep = keep

    def add(self, part, region, bind=None, name=None, outline=True, sub_regions=None, hull=None, hull_pad=False,
            covered_by=None):
        ratio = self.lod_ratio
        if ratio >= 1.0 or (self.lod_skip is not None and name and self.lod_skip(name)):
            return super().add(part, region, bind, name, outline, sub_regions, hull, hull_pad, covered_by)
        if not (self.keep is not None and name and self.keep(name)):
            flat = all(not f.smooth for f in part.bm.faces)
            n0 = part.tris()
            part = M.decimate(part, ratio)
            if part.tris() < n0:
                repair_normals(part, flat)
            if hull is not None:
                hull = M.decimate(hull, ratio)
        self.lod_ratio = 1.0
        try:
            return super().add(part, region, bind, name, outline, sub_regions, hull, hull_pad, covered_by)
        finally:
            self.lod_ratio = ratio


REPAIR_SHARP_DEG = 40.0


def repair_normals(part: M.Part, flat: bool) -> M.Part:
    """Shading normals of a decimated part from its own (decimated) faces: flat parts flat, smoothed parts smooth
    with every edge over ``REPAIR_SHARP_DEG`` hard (the Decimate modifier keeps face smooth flags, not sharp edges);
    face winding made consistent."""
    bmesh.ops.recalc_face_normals(part.bm, faces=part.bm.faces)
    return part.flat() if flat else part.auto_sharp(REPAIR_SHARP_DEG)


# Parts that keep their LOD0 topology in LOD1 (flat decals and plates: a decimated 2-12 mm strip folds its top into
# its rim; the seal, the void and its cores, the seep: the set pieces' key read; helm brims: the ring silhouette)
LOD_KEEP = ("seal", "groove", "underlay", "crack", "fissure", "core", "hole", "seep", "smoulder_dot", "scorch",
            "ash_ring", "spoke", "bank", "ditch", "helm", "sigil", "floor_char", "lip", "shield")


def lod_keep(name: str) -> bool:
    return any(name == s or name.startswith(s + "_") for s in LOD_KEEP)


def new(pal: Palette, name: str, lod: int = 0, decimate: bool = True):
    """A Builder on the set-piece regions (prefix ``setpiece``: names identical to A1's share their swatch).
    ``lod`` 1: the part-by-part LOD1 (:class:`SPBuilder`); ``decimate`` False keeps every part's LOD0 topology
    (the flat ground layers: their LOD1 is their LOD0, nothing to drop)."""
    global LOD
    LOD = lod
    if lod:
        b = SPBuilder(name, pal, prefix="setpiece", lod_ratio=0.5 if decimate else 1.0, lod_skip=lod_skip,
                      keep=lod_keep)
    else:
        b = M.Builder(name, pal, prefix="setpiece")
    b.regions(**ALL_REGIONS)
    BUILT[(name, lod)] = b
    return b, scene.rng(name)


# ── normal sanity (art review r1) ───────────────────────────────────────────────────────────────────────
NORMAL_DOT_MIN = 0.5                # a toon loop normal more than 60° off its face normal counts as bad …
NORMAL_BAD_MAX = 0.02               # … and an LOD fails when more than 2 % of its toon loops are bad


def normal_sanity(me) -> dict:
    """Toon-slot (material 0) loops whose shading normal deviates > 60° from their face normal (loopN·faceN <
    ``NORMAL_DOT_MIN``) and flipped (< 0): what made the decimated ground layers render as boulders while the
    re-import check (triangles, UVs, bounds, sockets) passed them."""
    import verify_march as VM
    return VM.normal_sanity(me, NORMAL_DOT_MIN, NORMAL_BAD_MAX)


def pad_report(name: str) -> dict:
    """{lod0|lod1: largest pads [(part, mm)]} and the parts over ``PAD_MAX_MM``."""
    out = {"maxMm": 0.0, "over": []}
    for lod in (0, 1):
        b = BUILT.get((name, lod))
        if b is None:
            continue
        pads = sorted(((p["name"], p["padMm"]) for p in b.parts if "padMm" in p), key=lambda t: -t[1])
        out[f"lod{lod}"] = pads[:4]
        if pads:
            out["maxMm"] = max(out["maxMm"], pads[0][1])
        out["over"] += [f"lod{lod} {n} {mm:.0f} mm" for n, mm in pads if mm > PAD_MAX_MM]
    out["pass"] = not out["over"]
    return out


LOD_SKIP = ("grain", "nail", "tag", "soot", "seam", "vein_dot", "rivet", "chalk", "rag", "tuft", "chip", "glyph_l",
            "straw", "course", "plank_line")


def lod_skip(name: str) -> bool:
    return any(name.startswith(s) for s in LOD_SKIP)


# ── small geometry helpers ─────────────────────────────────────────────────────────────────────────────
def box(size, center=(0, 0, 0), bevel: float = 0.0, segments: int = 1) -> M.Part:
    """``kit.mesh.box`` with its 45° bevel chamfers kept sharp: the kit's 50° auto-sharp smooths a chamfer into the
    face beside it, and a big flat face whose corner normals lean outward then splits its cel band along the
    triangle diagonal (a dark wedge on every plank of a roof). LOD1 drops the chamfers (12 triangles, not 44: they
    are below the readability floor where LOD1 draws)."""
    return M.box(size, center, bevel=0.0 if LOD else bevel, segments=segments).auto_sharp(30)


def hull_box(size, center=(0, 0, 0)) -> M.Part | None:
    """The plain box round a chamfered :func:`box` as its hull proxy (12 hull triangles, not 44; it encloses the
    chamfers, so it pads 0 mm); None at LOD1 (the LOD1 box is plain already: its own copy is the proxy)."""
    return None if LOD else M.box(size, center)


def beam(a: Vector, c: Vector, w: float, h: float | None = None, up: Vector | None = None, rows: int = 1,
         jitter: float = 0.0, rnd: random.Random | None = None) -> M.Part:
    """A squared timber from ``a`` to ``c`` (``w`` across, ``h`` along ``up``); ``rows`` intermediate rings keep a
    padded hull local; ``jitter`` roughens the section (hewn, charred)."""
    h = w if h is None else h
    a, c = Vector(a), Vector(c)
    pts = [a.lerp(c, k / rows) for k in range(rows + 1)]
    d = (c - a).normalized()
    if up is None:
        up = V(0, 0, 1) if abs(d.z) < 0.9 else V(0, -1, 0)
    prof = M.rect_profile(h, w)
    if jitter and rnd is not None:
        scales = [(1 + rnd.uniform(-jitter, jitter), 1 + rnd.uniform(-jitter, jitter)) for _ in pts]
    else:
        scales = None
    p = M.sweep(pts, prof, scales=scales, up=up, cap0="ngon", cap1="ngon")
    return p.flat().auto_sharp(40)


def charred_top(p: M.Part, z_top: float, depth: float, rnd: random.Random, teeth: int = 5) -> M.Part:
    """Burn a post's top into a jagged, tapered stump (vertices within ``depth`` of ``z_top`` pull in and down)."""
    ph = rnd.uniform(0, math.tau)
    amps = [rnd.uniform(0.3, 1.0) for _ in range(teeth)]
    for v in p.bm.verts:
        dz = z_top - v.co.z
        if dz < depth:
            k = 1.0 - max(dz, 0.0) / depth
            az = math.atan2(v.co.y, v.co.x)
            i = int(((az + math.pi) / math.tau * teeth + ph) % teeth)
            drop = depth * 0.5 * k * amps[i] * (0.5 + 0.5 * math.sin(az * teeth * 2 + ph))
            v.co.z -= drop
            v.co.x *= 1.0 - 0.12 * k
            v.co.y *= 1.0 - 0.12 * k
    return p


def stump(b: M.Builder, at: Vector, h: float, r: float, rnd: random.Random, region: str = "burnt",
          lean: float = 4.0, name: str = "stump") -> int:
    """A burned piling / post: an octagonal log, its top burned to jagged teeth."""
    rings = []
    for k in range(6):
        z = -0.05 + (h + 0.05) * k / 5
        rr = r * (1.0 + rnd.uniform(-0.06, 0.06))
        rings.append((z, rr, rr * rnd.uniform(0.9, 1.0)))
    p = M.loft_z(rings, sides=8, exp=2.0, cap0="ngon", cap1="ngon")
    charred_top(p, h, min(h * 0.45, 0.22), rnd)
    p.transform(_rz(rnd.uniform(0, 360)) @ _rx(rnd.uniform(-lean, lean)) @ _ry(rnd.uniform(-lean, lean)))
    p.translate(at)
    return b.add(p.smooth(False).auto_sharp(35), region, name=name, hull_pad=True)


def board(b: M.Builder, region: str, size, center, rot: Matrix | None, rnd: random.Random, name: str = "board",
          burn: float = 0.0, burn_end: int = 1, hull: bool = True, outline_on: bool = True) -> int:
    """A plank (bevelled box, slightly out of true). ``burn`` > 0 chews that length off one end into an uneven
    charred taper (``burn_end`` ±1 = the +X / −X end)."""
    sx, sy, sz = size
    p = box((sx, sy, sz), (0, 0, 0), bevel=min(0.01, sz * 0.3), segments=1)
    proxy = None
    if burn > 0:
        # extra loops near the burned end: subdivide by moving vertices (the box is 1 segment: taper the end face)
        for v in p.bm.verts:
            if v.co.x * burn_end > sx / 2 - 0.02:
                v.co.x -= burn_end * rnd.uniform(0.0, burn) * (0.4 + 0.6 * (0.5 + v.co.y / max(sy, 1e-6)))
                v.co.z *= rnd.uniform(0.5, 0.9)
    elif hull and outline_on:
        proxy = hull_box((sx, sy, sz))                    # a whole plank: the plain box is its hull proxy
    # (no per-vertex jitter: a non-planar face splits the cel bands along its triangle diagonal)
    m = _rx(rnd.uniform(-1.2, 1.2)) @ _ry(rnd.uniform(-1.2, 1.2))
    if rot is not None:
        m = rot @ m
    m = Matrix.Translation(center) @ m
    p.transform(m)
    if proxy is not None:
        proxy.transform(m)
    return b.add(p, region, name=name, hull_pad=hull and outline_on, outline=outline_on, hull=proxy)


def nail(b: M.Builder, at: Vector, normal: Vector, pid: int | None = None, r: float = 0.012):
    n = Vector(normal).normalized()
    p = M.cylinder(r, r, 0.0, 0.006, sides=4)
    p.transform(Vector((0, 0, 1)).rotation_difference(n).to_matrix().to_4x4())
    p.translate(at)
    b.add(p, "nail", name="nail", outline=False, covered_by=pid)


def paint(b: M.Builder, region: str, pts, normal, width, pid: int | None, lift: float = 0.002, thick: float = 0.003,
          name: str = "seam"):
    """A painted stroke on a flat face (soot, chalk, crack, seam)."""
    pts = [Vector(p) for p in pts]
    n = Vector(normal).normalized()
    b.add(E.ribbon(pts, [n] * len(pts), width, lift=lift, thick=thick), region, name=name, outline=False,
          covered_by=pid)


def kettle_helm(b: M.Builder, c: Vector, r: float, rot: Matrix, region: str = "helm_iron", name: str = "helm",
                strap: Vector | None = None, rnd: random.Random | None = None) -> int:
    """A crusade kettle hat (chapel de fer): an ogival crown rising to a low point, a raised comb front to back,
    a broad brim turned well down at its edge (art review r1: a flat brim seen from above read as a mushroom cap /
    lamp post) — dull iron, a darker band where crown meets brim. ``strap``: a crimson strap tail hanging from the
    brim, swinging toward that (world) direction. The helm keeps its LOD0 topology in LOD1 (``lod_keep``: the brim
    ring is the silhouette)."""
    prof = [(r * 1.3, -0.36 * r), (r * 1.46, -0.24 * r), (r * 1.54, -0.1 * r), (r * 1.4, -0.0 * r),
            (r * 1.05, 0.04 * r), (r * 1.0, 0.3 * r), (r * 0.93, 0.68 * r), (r * 0.76, 1.02 * r), (r * 0.5, 1.32 * r),
            (r * 0.22, 1.52 * r), (0.0, 1.6 * r)]
    p = M.lathe(prof, sides=14, cap0="ngon", cap1="fan")
    p.transform(rot)
    p.translate(c)
    hid = b.add(p, region, name=name, hull_pad=True)
    band = M.belt(r * 1.02, r * 1.02, 0.06 * r, 0.1 * r, 0.006, sides=14)
    band.transform(rot)
    band.translate(c)
    b.add(band, "iron_dark", name="seam_" + name, outline=False, covered_by=hid)
    comb = [V(0, -r * 0.9 + r * 1.8 * k / 8, 0) for k in range(9)]
    pts = []
    for q in comb:                                         # the comb follows the crown's front-back section
        t = abs(q.y) / r
        z = 1.6 * r * max(0.0, 1.0 - t ** 1.7) ** 0.62 + 0.012
        pts.append(V(0, q.y * 0.98, max(z, 0.3 * r)))
    cb = E.ribbon(pts, [V(1, 0, 0)] * len(pts), 0.012, lift=0.0, thick=0.008)
    cb.transform(rot)
    cb.translate(c)
    b.add(cb, "iron_dark", name="seam_comb_" + name, hull_pad=True)       # on the silhouette: its own ink
    if strap is not None:                                    # a crimson chin-strap tail off the brim's low side
        rnd = rnd or random.Random(0)
        a0 = rot.to_3x3() @ V(r * 1.32, 0.0, -0.3 * r)
        a0 = c + a0
        d = Vector(strap).normalized()
        pts = [a0, a0 + d * 0.04 + V(0, 0, -0.07), a0 + d * 0.07 + V(rnd.uniform(-0.02, 0.02), 0, -0.17),
               a0 + d * 0.09 + V(rnd.uniform(-0.03, 0.03), 0, -0.26)]
        nrm = d.cross(V(0, 0, 1)).normalized()
        st = E.ribbon(pts, [nrm] * 4, [0.03, 0.028, 0.026, 0.02], lift=0.0, thick=0.006)
        b.add(st, "shield", name="strap_" + name, hull_pad=True)
    return hid


def heater_shield(b: M.Builder, c: Vector, w: float, h: float, rot: Matrix, rnd: random.Random,
                  face: str = "shield", split: bool = False, name: str = "shield", bow: float = 0.05) -> int:
    """A crusade heater shield, point down, built facing −Y (its front) before ``rot``: a bowed plate (``bow`` m
    of curvature across) with an iron rim strip (``shield_rim``, covered by the plate's padded proxy). ``split``
    knocks the upper right corner off."""
    ring = [(-w / 2, h / 2)]
    for k in range(1, 9):
        t = k / 8
        ring.append((-(w / 2) * math.sqrt(max(0.0, 1.0 - t ** 1.7)), 0.12 * h - t * 0.62 * h))
    for k in range(7, 0, -1):
        t = k / 8
        ring.append(((w / 2) * math.sqrt(max(0.0, 1.0 - t ** 1.7)), 0.12 * h - t * 0.62 * h))
    ring += [(w / 2, 0.12 * h), (w / 2, h / 2)]
    if split:
        ring = [(x, y) for x, y in ring if not (x > w * 0.2 and y > h * 0.2)]
        ring.insert(len(ring) - 0, (w * 0.2, h / 2))
        ring.insert(len(ring) - 1, (w / 2, h * 0.2))
        ring = [p for p in ring if p != (w / 2, h / 2)]
    if _area(ring) < 0:
        ring = list(reversed(ring))
    # LOD1 unbevelled (68 triangles, kept whole: ``lod_keep``). (The unbevelled plate is no LOD0 hull proxy: its
    # big fan faces padded 26-33 mm round the rim strip — the chamfer ring keeps the pad local)
    p = M.plate(ring, 0.026, bevel=0.0 if LOD else 0.005)
    for v in p.bm.verts:                     # bow round the vertical axis: the centre stands forward (+Z)
        v.co.z += bow * (1.0 - (2.0 * v.co.x / w) ** 2)
    p.transform(_rx(90))                     # plate +Z (its front) → −Y
    p.transform(rot)
    p.translate(c)
    sid = b.add(p, face, name=name, hull_pad=True)
    # iron rim along the top and the sides (a strip just proud of the front face)
    inset = _inset(ring, 0.013)
    rim = [V(x, 0, y) for i, (x, y) in enumerate(inset) if i % 2 == 0 or i in (0, len(inset) - 1, len(inset) - 2)]
    nrm = [V(0, -1, 0)] * len(rim)
    part = E.ribbon(rim + [rim[0]], nrm + [nrm[0]], 0.018, lift=0.012, thick=0.004)
    for v in part.bm.verts:
        v.co.y -= bow * (1.0 - (2.0 * v.co.x / w) ** 2)
    part.transform(rot)
    part.translate(c)
    b.add(part, "shield_rim", name="seam_rim", outline=False, covered_by=sid)
    return sid


def _inset(ring, d: float):
    """The closed CCW outline ``ring`` moved ``d`` inward along each corner's bisector (mitre capped at 3 d)."""
    out = []
    n = len(ring)
    for i in range(n):
        (x0, y0), (x1, y1), (x2, y2) = ring[i - 1], ring[i], ring[(i + 1) % n]
        e0 = V(x1 - x0, y1 - y0, 0)
        e1 = V(x2 - x1, y2 - y1, 0)
        n0 = V(-e0.y, e0.x, 0).normalized() if e0.length > 1e-9 else V(0, 0, 0)
        n1 = V(-e1.y, e1.x, 0).normalized() if e1.length > 1e-9 else V(0, 0, 0)
        bis = n0 + n1
        if bis.length < 1e-9:
            bis = n0 if n0.length else n1
        bis.normalize()
        k = min(d / max(bis.dot(n0 if n0.length else n1), 1e-3), 3.0 * d)
        out.append((x1 + bis.x * k, y1 + bis.y * k))
    return out


def _area(ring) -> float:
    a = 0.0
    for i in range(len(ring)):
        x0, y0 = ring[i]
        x1, y1 = ring[(i + 1) % len(ring)]
        a += x0 * y1 - x1 * y0
    return a / 2


def wheel(b: M.Builder, c: Vector, R: float, rot: Matrix, rnd: random.Random, spokes: int = 8, broken: int = 0,
          rim_region: str = "wood_old_dark", name: str = "wheel", spoke_region: str = "wood_old") -> int:
    """A cart wheel in the XZ plane (axle along Y) before ``rot``: felloe ring, iron tyre, hub, ``spokes``
    (``broken`` of them snapped)."""
    def rim_part(n: int) -> M.Part:
        pts = [V(math.cos(math.tau * k / n) * (R - 0.035), 0, math.sin(math.tau * k / n) * (R - 0.035))
               for k in range(n)]
        p = M.sweep(pts, M.rect_profile(0.07, 0.06), up=[V(0, 1, 0)] * n, closed_path=True)
        p.transform(rot)
        p.translate(c)
        return p

    def hub_part(sides: int) -> M.Part:
        p = M.cylinder(R * 0.16, R * 0.14, -0.07, 0.07, sides=sides)
        p.transform(_rx(90))
        p.transform(rot)
        p.translate(c)
        return p
    # budget (art review r1): LOD0 rim 16 segments (was 20), spokes 5 sides (was 6); LOD1 rim 14 (12 read as a
    # polygon at 1080p), hub 6, spokes 4. No coarser hull proxies: a 14-segment rim / 4-sided spoke proxy padded
    # up to 19 mm round the finer part and thickened the wheels' ink to a 2.1 px baked median (decor 1.4 px)
    wid = b.add(rim_part(14 if LOD else 16), rim_region, name=name, hull_pad=True)
    b.add(hub_part(6 if LOD else 10), rim_region, name=name + "_hub", hull_pad=True)
    a0 = rnd.uniform(0, math.tau)
    snapped = set(rnd.sample(range(spokes), broken)) if broken else set()
    for k in range(spokes):
        a = a0 + math.tau * k / spokes
        d = V(math.cos(a), 0, math.sin(a))
        L = (R - 0.06) * (0.45 if k in snapped else 1.0)
        sp = M.tube([d * R * 0.14, d * L], [0.022, 0.018], sides=4 if LOD else 5)
        sp.transform(rot)
        sp.translate(c)
        b.add(sp, spoke_region, name=name + "_spoke", hull_pad=True)
    return wid


def ash_heap(b: M.Builder, c: Vector, r: float, h: float, rnd: random.Random, region: str = "ash",
             name: str = "heap", z: float = 0.0) -> int:
    """A low heap of ash / earth, flat-footed (bedded in the ground, or on a floor at height ``z``)."""
    shape = E.RockShape((r, r * rnd.uniform(0.75, 0.95), h), (c.x, c.y, rnd.random()), bumps=4, amp=0.05,
                        top_cut=0.0)
    # LOD1: subdiv 1 (a subdiv-1 heap is no LOD0 hull proxy: its bumps differ, the pad grew 3-9 cm round it)
    part = E.rock(shape, center=(c.x, c.y, -h * 0.35), subdiv=1 if LOD else 2, facet=None, yaw=rnd.uniform(0, 360))
    for v in part.bm.verts:
        v.co.z = max(v.co.z, -0.02) + z
    # smooth, its buried flat underside hard (a smooth rim averaged the ground-facing faces into the lit top)
    return b.add(part.auto_sharp(55), region, name=name, hull_pad=True)


def rubble(b: M.Builder, c: Vector, s: float, rnd: random.Random, region: str = "stone", name: str = "rubble") -> int:
    shape = E.RockShape((s, s * rnd.uniform(0.7, 1.0), s * rnd.uniform(0.5, 0.8)), (c.x, c.y, s, rnd.random()),
                        bumps=4, amp=0.1, top_cut=0.3, boxy=2.6)
    part = E.rock(shape, center=(c.x, c.y, c.z), subdiv=1, facet=30.0, yaw=rnd.uniform(0, 360))
    return b.add(part, region, name=name, hull_pad=True)


# ── Ashwheat cottages (the goblin-camp slot re-themed: goblin_hut@march) ─────────────────────────────────
# body 2.3 × 1.85 m (the hut slot's 2 × 2 tiles + eaves), walls 1.7 m to the plate, thatch ridge 3.0 m
CW, CD, CHW, CHG, CEAVE, CGABLE, PLINTH = 2.3, 1.85, 1.7, 2.62, 0.30, 0.18, 0.16
THATCH_T = 0.30
RIDGE_Z = CHG + 0.34              # outer ridge line of the thatch
EAVE_Z = CHW - 0.06               # outer eave line
SAG = 0.07                        # the ridge sags this much mid-span
COURSES = 3                       # thatch courses: each overlaps the one below with a rounded, lit lip
COURSE_STEP = 0.055


def _yz_prism(prof_yz, x0: float, x1: float) -> M.Part:
    """A prism of the (y, z) outline ``prof_yz`` (CCW seen from +X) spanning x0 … x1 (closed)."""
    p = M.plate(prof_yz, x1 - x0, bevel=0.0)                 # outline in XY, thickness along Z
    p.transform(Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1))))   # (u, v, w) → (w, u, v)
    p.translate(V((x0 + x1) / 2, 0, 0))
    return p.flat().auto_sharp(30)


def roof_outer(x: float, t: float, side: int, ph: float = 0.0) -> Vector:
    """A point on the thatch's outer surface: ``t`` 0 = eave … 1 = ridge, ``side`` −1 front / +1 back. The ridge
    sags mid-span, the eave line waves a little (hand-laid thatch)."""
    ye = side * (CD / 2 + CEAVE)
    L = CW / 2 + CGABLE
    sag = SAG * (1.0 - (x / L) ** 2)
    wave = 0.025 * math.sin(x * 4.3 + ph) * (1.0 - t)
    z = EAVE_Z + (RIDGE_Z - EAVE_Z) * t - sag * t ** 0.7 + wave
    bulge = 0.035 * math.sin(math.pi * t)                 # thatch swells between eave and ridge
    u = (t * COURSES) % 1.0 if t < 0.999 else 1.0
    step = COURSE_STEP * (1.0 - u) ** 1.6 if t < 0.999 else 0.0
    y = ye * (1.0 - t)
    n = slope_normal(side)
    return V(x, y, z) + n * (bulge + step)


def _slope_ts(t0: float, t1: float, n: int = 5) -> list[float]:
    """Samples of a slope from t0 to t1: ``n`` even steps plus both sides of every course boundary."""
    ts = {round(t0 + (t1 - t0) * k / n, 5) for k in range(n + 1)}
    for k in range(1, COURSES):
        tb = k / COURSES
        for e in (-0.006, 0.006):
            if t0 + 0.004 < tb + e < t1 - 0.004:
                ts.add(round(tb + e, 5))
    return sorted(ts)


def slope_normal(side: int) -> Vector:
    ye = side * (CD / 2 + CEAVE)
    d = V(0, -ye, RIDGE_Z - EAVE_Z).normalized()
    n = d.cross(V(1, 0, 0)) if side < 0 else V(1, 0, 0).cross(d)
    return n if n.z > 0 else -n


def thatch_slab(x0: float, x1: float, side: int, t0: float, t1: float, rnd: random.Random, rows: int = 6,
                ragged: float = 0.0, ph: float = 0.0) -> M.Part:
    """One slope of the thatch from ``t0`` (eave) to ``t1`` (ridge), x0 … x1, front (``side`` −1) or back (+1): a
    thick slab with a rounded eave lip; ``ragged`` frays the edges that are cut (burned) rather than built."""
    n = slope_normal(side)
    ts = _slope_ts(t0, t1)
    xs = [x0 + (x1 - x0) * k / rows for k in range(rows + 1)]
    L = CW / 2 + CGABLE
    cut0, cut1 = t0 > 0.001, t1 < 0.999
    rings = []
    for x in xs:
        ring = []
        for k, t in enumerate(ts):                      # outer surface, eave → ridge
            p = roof_outer(x, t, side, ph)
            if ragged and ((k == len(ts) - 1 and cut1) or (k == 0 and cut0)):
                p += V(0, 0, 0) + (roof_outer(x, min(1, t + 0.1), side, ph) - p).normalized() * rnd.uniform(-ragged, ragged)
            ring.append(p)
        for k, t in reversed(list(enumerate(ts))):      # inner surface, ridge → eave
            thick = THATCH_T * (0.7 if t > 0.92 else 1.0)
            p = roof_outer(x, t, side, ph) - n * thick
            if ragged and ((k == len(ts) - 1 and cut1) or (k == 0 and cut0)):
                p += (roof_outer(x, min(1, t + 0.1), side, ph) - roof_outer(x, t, side, ph)).normalized() * \
                    rnd.uniform(-ragged, ragged)
            ring.append(p)
        if not cut0:                                      # the rounded eave lip (between inner and outer eave)
            e = roof_outer(x, 0.0, side, ph)
            down = (roof_outer(x, 0.0, side, ph) - roof_outer(x, 0.1, side, ph)).normalized()
            ring.append(e - n * THATCH_T * 0.55 + down * 0.07)
        rings.append(ring)
    for ring, edge in ((rings[0], x0), (rings[-1], x1)):
        if ragged and abs(abs(edge) - L) > 0.01:
            for v in ring:
                v.x += rnd.uniform(-ragged, ragged) * 0.7
        elif abs(abs(edge) - L) <= 0.01:                  # gable edge: the thatch rolls over a little
            for v in ring:
                v.z -= 0.02
    part = E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=True)
    part.recalc_normals()
    return part.auto_sharp(55)


def under_roof(x0: float, x1: float, side: int, t1: float = 1.0, t0: float = 0.0) -> M.Part:
    """The dark under-side of a slope where thatch still lies on it: a thin plate just under the thatch (never under
    a burned hole — art review r1: there it showed as a flat black lid instead of an opening)."""
    n = slope_normal(side)
    pts = []
    for x in (x0, x1):
        for t in (t0, t1):
            pts.append(roof_outer(x, t, side) - n * (THATCH_T + 0.02))
    a, c, e, f = pts[0], pts[1], pts[3], pts[2]
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in (a, f, e, c)]
    bm.faces.new(vs)
    bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.015)
    part = M.Part(bm)
    part.recalc_normals()
    return part


def build_cottage(pal: Palette, variant: str = "A", lod: int = 0):
    """Ashwheat cottage (see the module table). A: a hole burned through the front thatch over the window;
    B: the thatch gone from the right half of the roof, bare charred rafters, one shutter left."""
    name = f"SM_Prop_March_AshwheatCottage_{variant}"
    b, rnd = new(pal, name, lod)
    W, D = CW, CD
    ph = rnd.uniform(0, math.tau)
    # plinth + walls (box to the wall plate) + the two gables (daub triangles)
    pl = box((W + 0.08, D + 0.08, PLINTH + 0.05), (0, 0, (PLINTH - 0.05) / 2), bevel=0.02)
    b.add(pl, "stone", name="plinth", hull_pad=True)
    body = box((W, D, CHW - PLINTH + 0.02), (0, 0, (CHW + PLINTH) / 2 - 0.01))
    # its hull proxy is the box out to the timber frame (posts and rails stand 2.5–3.5 cm proud of the daub and
    # make the silhouette): padding the bare 8-corner box round them pushed whole corners out — black wedges
    frame_box = box((W + 0.055, D + 0.075, CHW - PLINTH + 0.06), (0, 0, (CHW + PLINTH) / 2 + 0.01))
    bid = b.add(body.flat(), "daub", name="walls", hull=frame_box.flat(), hull_pad=True)
    gids = {}
    for sx in (-1, 1):
        g = _yz_prism([(-D / 2 + 0.02, CHW - 0.02), (D / 2 - 0.02, CHW - 0.02), (0.0, CHG)], -0.06, 0.06)
        g.translate(V(sx * (W / 2 - 0.06), 0, 0))
        # hull proxy: out over the gable frame (6 cm proud) and down over the tie beam
        gh = _yz_prism([(-D / 2 + 0.02, CHW - 0.09), (D / 2 - 0.02, CHW - 0.09), (D / 2 - 0.02, CHW + 0.06),
                        (0.0, CHG), (-D / 2 + 0.02, CHW + 0.06)], -0.06 - (0.066 if sx < 0 else 0.0),
                       0.06 + (0.066 if sx > 0 else 0.0))
        gh.translate(V(sx * (W / 2 - 0.06), 0, 0))
        gid = gids[sx] = b.add(g, "daub", name="gable", hull=gh, hull_pad=True)
        xg = sx * (W / 2 + 0.002)
        b.add(beam(V(xg, 0, CHW - 0.02), V(xg, 0, CHG - 0.1), 0.1, rows=2), "charred", name="frame_king",
              outline=False, covered_by=gid)
        for sy in (-1, 1):
            b.add(beam(V(xg, sy * (D / 2 - 0.1), CHW), V(xg, sy * 0.12, CHG - 0.25), 0.08, rows=2), "charred",
                  name="frame_strut", outline=False, covered_by=gid)
        b.add(beam(V(xg, -D / 2 + 0.03, CHW - 0.02), V(xg, D / 2 - 0.03, CHW - 0.02), 0.12, rows=3), "charred",
              name="frame_tie", outline=False, covered_by=gid)
    # the burned loft floor seen through the roof (art review r1: the flat L* 8 plate read as a black box lid; a
    # charred floor at L* ~17 with fallen rafters and ash keeps the "something is in there" dark)
    b.add(box((W - 0.06, D - 0.06, 0.02), (0, 0, CHW + 0.01)), "floor_char", name="floor_char", outline=False)
    # timber frame on the walls: corner posts, sill, wall plate, a mid rail (charred)
    for sx in (-1, 1):
        for sy in (-1, 1):
            p = beam(V(sx * (W / 2 - 0.05), sy * (D / 2 - 0.05), PLINTH - 0.02),
                     V(sx * (W / 2 - 0.05), sy * (D / 2 - 0.05), CHW + 0.02), 0.15, rows=3, jitter=0.05, rnd=rnd)
            b.add(p, "charred", name="frame_post", outline=False, covered_by=bid)
    for sy in (-1, 1):
        for z, w_ in ((PLINTH + 0.05, 0.11), (CHW - 0.05, 0.14), (1.05, 0.09)):
            p = beam(V(-W / 2 + 0.02, sy * (D / 2 - 0.035), z), V(W / 2 - 0.02, sy * (D / 2 - 0.035), z), w_, rows=4,
                     jitter=0.04, rnd=rnd)
            b.add(p, "charred", name="frame_rail", outline=False, covered_by=bid)
    for sx in (-1, 1):                                     # the gable-end walls: sill, rail, a centre post
        xw = sx * (W / 2 - 0.035)
        for z, w_ in ((PLINTH + 0.05, 0.11), (1.05, 0.09)):
            b.add(beam(V(xw, -D / 2 + 0.02, z), V(xw, D / 2 - 0.02, z), w_, rows=3, jitter=0.04, rnd=rnd), "charred",
                  name="frame_rail", outline=False, covered_by=bid)
        b.add(beam(V(xw, 0.32 * sx, PLINTH), V(xw, 0.32 * sx, CHW), 0.11, rows=3, jitter=0.05, rnd=rnd), "charred",
              name="frame_post", outline=False, covered_by=bid)
    # a shuttered window on the −X gable wall (shut: dark crack between the leaves)
    xs_ = -W / 2 - 0.006
    for sy_ in (-1, 1):
        b.add(box((0.03, 0.17, 0.32), (xs_ - 0.012, -0.32 + sy_ * 0.088, 1.28), bevel=0.005), "wood",
              name="shutter_shut", outline=False, covered_by=bid)
    b.add(beam(V(xs_ - 0.03, -0.55, 1.12), V(xs_ - 0.03, -0.09, 1.12), 0.06, rows=1), "timber", name="frame_sill",
          outline=False)
    yf = -D / 2 - 0.005                                    # the front face
    # door: shut, barred from inside (nothing nailed outside). The door, its frame and the window stand inside the
    # front face (never on the silhouette): no ink, no cover (a cover would pad the wall's hull 5–10 cm out)
    dx, dw, dh = -0.45, 0.72, 1.45
    door = box((dw, 0.05, dh), (dx, yf - 0.012, PLINTH + dh / 2), bevel=0.008)
    b.add(door, "door", name="door", outline=False)
    for k in range(1, 4):                                  # plank joints
        x = dx - dw / 2 + dw * k / 4
        paint(b, "wood_line", [V(x, yf - 0.04, PLINTH + 0.06), V(x, yf - 0.04, PLINTH + dh - 0.06)], V(0, -1, 0),
              0.012, None, name="plank_line")
    for z in (PLINTH + 0.28, PLINTH + dh - 0.3):           # strap hinges
        b.add(box((dw * 0.78, 0.012, 0.05), (dx - dw * 0.08, yf - 0.042, z)), "iron_dark", name="seam_hinge",
              outline=False)
    ring = M.tube([V(dx + dw * 0.32 + 0.04 * math.cos(a), yf - 0.045, PLINTH + 0.74 + 0.04 * math.sin(a))
                   for a in [k * math.tau / 8 for k in range(9)]], [0.008] * 9, sides=5)
    b.add(ring, "iron", name="nail_ring", outline=False)
    for sx in (-1, 1):                                     # door jambs and lintel
        b.add(beam(V(dx + sx * (dw / 2 + 0.05), yf - 0.02, PLINTH), V(dx + sx * (dw / 2 + 0.05), yf - 0.02,
                                                                       PLINTH + dh + 0.08), 0.1, rows=2),
              "charred", name="frame_jamb", outline=False)
    b.add(beam(V(dx - dw / 2 - 0.12, yf - 0.025, PLINTH + dh + 0.08), V(dx + dw / 2 + 0.12, yf - 0.025,
                                                                         PLINTH + dh + 0.08), 0.12, rows=2),
          "charred", name="frame_lintel", outline=False)
    b.add(box((dw + 0.2, 0.22, 0.06), (dx, yf - 0.1, 0.03), bevel=0.015), "hearth_stone", name="threshold",
          hull_pad=True)
    # window: the dark behind, a sill, the candle, shutters swung open
    wx, ww, wz0, wh = 0.55, 0.44, 0.86, 0.36
    b.add(box((ww, 0.03, wh), (wx, yf - 0.006, wz0 + wh / 2)), "window", name="window", outline=False)
    b.add(beam(V(wx - ww / 2 - 0.08, yf - 0.05, wz0 - 0.02), V(wx + ww / 2 + 0.08, yf - 0.05, wz0 - 0.02), 0.07,
               rows=2), "timber", name="frame_sill", outline=False)
    b.add(beam(V(wx - ww / 2 - 0.06, yf - 0.02, wz0 + wh + 0.03), V(wx + ww / 2 + 0.06, yf - 0.02, wz0 + wh + 0.03),
               0.08, rows=2), "charred", name="frame_wlintel", outline=False)
    cpos = V(wx + 0.05, yf - 0.03, wz0 + 0.06)
    b.add(M.cylinder(0.022, 0.02, wz0 - 0.005, wz0 + 0.085, sides=6).translate(V(cpos.x, cpos.y, 0)), "candle_wax",
          name="candle_wax", outline=False)
    flame = M.ellipsoid((0.026, 0.018, 0.05), (cpos.x, cpos.y - 0.004, wz0 + 0.14), sides=6, rings=4)
    b.add(flame, "candle", name="candle", outline=False)
    shutters = [(-1, 58.0), (1, -64.0)] if variant == "A" else [(-1, 72.0)]
    for side, ang in shutters:
        hinge = V(wx + side * ww / 2, yf - 0.02, 0)
        p = box((ww / 2, 0.03, wh + 0.02), (side * ww / 4, -0.015, wz0 + wh / 2), bevel=0.006)
        p.transform(_rz(ang))
        p.translate(V(hinge.x, hinge.y, 0))
        b.add(p, "wood", name="shutter", hull_pad=True)
    if variant == "B":                                     # the other shutter lies on the ground
        p = box((0.36, ww / 2, 0.03), (0, 0, 0.0), bevel=0.006)
        p.transform(_rz(20) @ _ry(8))
        p.translate(V(wx + 0.45, yf - 0.42, 0.03))
        b.add(p, "wood", name="shutter_fallen", hull_pad=True)
    # soot licking up from the openings
    for x0, z0, n_, spread in ((wx, wz0 + wh + 0.06, 4, 0.24),):
        for k in range(n_):
            u = (k / max(n_ - 1, 1) - 0.5) * spread
            pts = [V(x0 + u, yf - 0.012, z0), V(x0 + u * 1.25 + rnd.uniform(-0.03, 0.03), yf - 0.012,
                                                 min(z0 + 0.12 + rnd.uniform(0, 0.08), CHW - 0.2)),
                   V(x0 + u * 1.4 + rnd.uniform(-0.05, 0.05), yf - 0.012, min(z0 + 0.3 + rnd.uniform(0, 0.1),
                                                                            CHW - 0.1))]
            b.add(E.ribbon(pts, [V(0, -1, 0)] * 3, [0.07, 0.05, 0.015], lift=0.0, thick=0.002), "soot",
                  name="soot", outline=False)
    for k in range(3):                                     # soot up the right gable end (wall, then gable)
        y = (k - 1) * 0.32
        pts = [V(W / 2 + 0.003, y, CHW - 0.35), V(W / 2 + 0.003, y * 0.85, CHW - 0.06)]
        b.add(E.ribbon(pts, [V(1, 0, 0)] * 2, [0.12, 0.1], lift=0.0, thick=0.002), "soot", name="soot",
              outline=False, covered_by=bid)
        pts = [V(W / 2 + 0.003, y * 0.8, CHW + 0.02), V(W / 2 + 0.003, y * 0.55, CHW + 0.2),
               V(W / 2 + 0.003, y * 0.3, CHG - 0.3)]
        b.add(E.ribbon(pts, [V(1, 0, 0)] * 3, [0.1, 0.07, 0.02], lift=0.0, thick=0.002), "soot", name="soot",
              outline=False, covered_by=gids[1])
    # roof: thatch slabs over dark under-plates, a ridge roll; burned gaps show charred rafters over the dark loft
    X0, X1 = -W / 2 - CGABLE, W / 2 + CGABLE
    xw0, xw1 = -W / 2 + 0.02, W / 2 - 0.02
    holes = []
    if variant == "A":
        hx0, hx1 = 0.2, 0.98                               # the burned hole over the window
        for x0_, x1_, sd, t0_, t1_ in ((xw0, hx0, -1, 0.0, 1.0), (hx0, hx1, -1, 0.0, 0.38),
                                        (hx0 + 0.22, hx1 - 0.1, -1, 0.82, 1.0), (hx1, xw1, -1, 0.0, 1.0),
                                        (xw0, xw1, 1, 0.0, 1.0)):
            b.add(under_roof(x0_, x1_, sd, t1_, t0_), "window", name="underroof", outline=False)
        debris = [(V(0.32, -0.62, 0.0), V(0.86, -0.12, 0.05)), (V(0.95, -0.55, 0.0), V(0.5, -0.05, 0.08))]
        heaps = [(V(0.62, -0.42, 0), 0.17), (V(0.3, -0.18, 0), 0.12)]
        b.add(thatch_slab(X0, hx0, -1, 0.0, 1.0, rnd, rows=5, ph=ph), "thatch", name="thatch", hull_pad=True)
        b.add(thatch_slab(hx0, hx1, -1, 0.0, 0.38, rnd, rows=4, ragged=0.08, ph=ph), "thatch", name="thatch",
              hull_pad=True)
        b.add(thatch_slab(hx0 + 0.22, hx1 - 0.1, -1, 0.82, 1.0, rnd, rows=3, ragged=0.06, ph=ph), "thatch",
              name="thatch", hull_pad=True)
        b.add(thatch_slab(hx1, X1, -1, 0.0, 1.0, rnd, rows=2, ragged=0.06, ph=ph), "thatch", name="thatch",
              hull_pad=True)
        b.add(thatch_slab(X0, X1, 1, 0.0, 1.0, rnd, rows=7, ph=ph), "thatch", name="thatch", hull_pad=True)
        holes.append((-1, hx0, hx1, 0.38, 1.0))
        rafters = [(-1, hx0 + 0.16), (-1, (hx0 + hx1) / 2 + 0.03), (-1, hx1 - 0.14)]
        ridge = [(X0 - 0.03, X1 + 0.03)]
    else:
        cut = -0.12
        for x0_, x1_, sd, t0_, t1_ in ((xw0, cut, -1, 0.0, 1.0), (cut, xw1, -1, 0.0, 0.26),
                                        (xw0, cut + 0.3, 1, 0.0, 1.0), (cut + 0.3, xw1, 1, 0.0, 0.2)):
            b.add(under_roof(x0_, x1_, sd, t1_, t0_), "window", name="underroof", outline=False)
        debris = [(V(0.05, -0.55, 0.0), V(0.95, 0.3, 0.06)), (V(0.7, -0.6, 0.0), V(0.35, 0.55, 0.1)),
                  (V(0.15, 0.45, 0.0), V(1.0, 0.1, 0.04))]
        heaps = [(V(0.55, -0.35, 0), 0.2), (V(0.85, 0.4, 0), 0.15), (V(0.25, 0.05, 0), 0.12)]
        b.add(thatch_slab(X0, cut, -1, 0.0, 1.0, rnd, rows=4, ragged=0.09, ph=ph), "thatch", name="thatch",
              hull_pad=True)
        b.add(thatch_slab(X0, cut + 0.3, 1, 0.0, 1.0, rnd, rows=4, ragged=0.09, ph=ph), "thatch", name="thatch",
              hull_pad=True)
        b.add(thatch_slab(cut, X1, -1, 0.0, 0.26, rnd, rows=4, ragged=0.07, ph=ph), "thatch", name="thatch",
              hull_pad=True)
        b.add(thatch_slab(cut + 0.3, X1 - 0.25, 1, 0.0, 0.2, rnd, rows=3, ragged=0.07, ph=ph), "thatch",
              name="thatch", hull_pad=True)
        holes.append((-1, cut, X1, 0.26, 1.0))
        rafters = [(sd, x) for x in (cut + 0.22, cut + 0.66, cut + 1.1, W / 2 - 0.06) for sd in (-1, 1)]
        ridge = [(X0 - 0.03, cut + 0.05)]
        b.add(beam(V(cut - 0.1, 0, RIDGE_Z - THATCH_T - 0.02), V(X1 - 0.35, 0, RIDGE_Z - THATCH_T - 0.09), 0.13,
                   rows=3, jitter=0.08, rnd=rnd), "charred", name="ridge_beam", hull_pad=True)
        for sd in (-1, 1):                                  # purlins half-way up the bare slope
            b.add(beam(roof_outer(cut + 0.05, 0.55, sd) - slope_normal(sd) * (THATCH_T + 0.05),
                       roof_outer(X1 - 0.5, 0.55, sd) - slope_normal(sd) * (THATCH_T + 0.1), 0.08, rows=3,
                       jitter=0.1, rnd=rnd), "charred", name="purlin", hull_pad=True)
    for xa, xb in ridge:                                   # the ridge roll
        pts = [V(xa + (xb - xa) * k / 6, 0, roof_outer(xa + (xb - xa) * k / 6, 1.0, -1).z + 0.02)
               for k in range(7)]
        b.add(M.tube(pts, [(0.13, 0.1)] * 7, sides=10), "thatch_lit", name="ridge", hull_pad=True)
    for sd, x in rafters:                                  # charred rafters over the gaps
        a = roof_outer(x, 0.0, sd) - slope_normal(sd) * (THATCH_T + 0.04)
        c = roof_outer(x, 1.0, sd) - slope_normal(sd) * (THATCH_T + 0.08) + V(rnd.uniform(-0.03, 0.03), 0, 0)
        if rnd.random() < 0.35:                            # burned short
            c = a.lerp(c, rnd.uniform(0.6, 0.85))
        b.add(beam(a, c, 0.08, rows=2, jitter=0.12, rnd=rnd), "charred", name="rafter", hull_pad=True)
    # on the burned loft floor under the holes: fallen rafters and heaps of ash and burnt thatch
    zf = CHW + 0.02
    for a, c in debris:
        b.add(beam(a + V(0, 0, zf + 0.04), c + V(0, 0, zf + 0.04), 0.07, rows=2, jitter=0.12, rnd=rnd), "char_black",
              name="loft_rafter", hull_pad=True)
    for c, r in heaps:
        ash_heap(b, c, r, 0.06, rnd, region="ash", name="loft_ash", z=zf)
    # charred lips along the burned edges (the slab's cut faces read black)
    for sd, xa, xb, ta, tb in holes:
        for t in (ta,):
            pts = [roof_outer(xa + (xb - xa) * k / 5, t - 0.015, sd, ph) + slope_normal(sd) * 0.006 for k in range(6)]
            b.add(E.ribbon(pts, [slope_normal(sd)] * 6, 0.07, lift=0.0, thick=0.004), "burnt", name="soot_edge",
                  outline=False)
    # thatch texture: straw streaks down each course, a lighter cut-straw band along each course's lip
    for sd in (-1, 1):
        n = slope_normal(sd)
        for k in range(COURSES):
            for q in range(11):
                x = X0 + 0.12 + (X1 - X0 - 0.24) * (q + rnd.uniform(0.15, 0.85)) / 11
                ta = k / COURSES + 0.03 + rnd.uniform(0.0, 0.05)
                tb = (k + 1) / COURSES - 0.025 - rnd.uniform(0.0, 0.12)
                if any(hs == sd and xa - 0.04 < x < xb + 0.04 and tb > ta_ and ta < tb_
                       for hs, xa, xb, ta_, tb_ in holes):
                    continue
                if variant == "B" and sd > 0 and x > 0.1 and tb > 0.2:
                    continue
                pts = [roof_outer(x + (j - 1) * 0.004, ta + (tb - ta) * j / 2, sd, ph) + n * 0.001 for j in range(3)]
                b.add(E.ribbon(pts, [n] * 3, [0.018, 0.014, 0.006], lift=0.0, thick=0.003), "thatch_line",
                      name="straw", outline=False)
        for k in range(COURSES):
            t = k / COURSES + 0.012
            spans = [(X0 + 0.06, X1 - 0.06)]
            for hs, xa, xb, ta, tb in holes:
                if hs == sd and ta < t < tb:
                    spans = [(X0 + 0.06, xa - 0.05), (xb + 0.05, X1 - 0.06)]
            if variant == "B" and sd > 0 and k > 0:
                spans = [(X0 + 0.06, 0.1)]
            if variant == "B" and sd > 0 and k == 0:
                spans = [(X0 + 0.06, X1 - 0.3)]
            for xa, xb in spans:
                if xb - xa < 0.2:
                    continue
                pts = [roof_outer(xa + (xb - xa) * q / 8, t, sd, ph) + n * 0.001 for q in range(9)]
                b.add(E.ribbon(pts, [n] * 9, 0.03, lift=0.0, thick=0.003), "thatch_lit", name="course",
                      outline=False)
    # debris: a fallen charred beam, thatch clumps, a few stones
    b.add(beam(V(W / 2 + 0.28, -0.55, 0.06), V(W / 2 + 0.58, 0.65, 0.09), 0.13, rows=3, jitter=0.1, rnd=rnd),
          "burnt", name="debris_beam", hull_pad=True)
    for c, r in ((V(-W / 2 - 0.32, -0.6, 0), 0.22), (V(W / 2 + 0.22, D / 2 + 0.3, 0), 0.18)):
        ash_heap(b, c, r, 0.1, rnd, region="thatch", name="debris_thatch")
    for k in range(3):
        rubble(b, V(-W / 2 - 0.15 + rnd.uniform(-0.1, 0.3), -D / 2 - 0.32 - rnd.uniform(0, 0.2), 0), 0.07, rnd,
               region="stone", name="debris_stone")
    obj = E.finish(b, pal, "decor", ground_h=2.6, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "knock", "pos": [dx, yf - 0.06, PLINTH + 0.85]},
                                    {"name": "candle", "pos": [cpos.x, cpos.y - 0.004, wz0 + 0.14]}])
    return obj


# ── the burned bridge (38,48) ───────────────────────────────────────────────────────────────────────────
def dry_stone(b: M.Builder, c: Vector, size, rnd: random.Random, region: str = "pit_stone", name: str = "abut_stone"):
    """A rough squared stone of a dry-stone abutment."""
    sx, sy, sz = size
    shape = E.RockShape((sx / 2, sy / 2, sz / 2), (c.x, c.y, c.z, sx), bumps=3, amp=0.05, top_cut=0.25, boxy=3.0,
                        centre=1.0, sink=0.0)
    # LOD1: subdiv 1 (not a LOD0 hull proxy: its bumps differ, the pad grew 7-12 cm round the stone)
    part = E.rock(shape, center=(c.x, c.y, c.z - sz / 2), subdiv=1 if LOD else 2, facet=24.0, yaw=rnd.uniform(-6, 6))
    return b.add(part, region, name=name, hull_pad=True)


def build_bridge(pal: Palette, lod: int = 0, ground: bool = False):
    """The bridge north of Ashwheat, burned to the stumps over a dry ditch (chapter1.md §5 M3). The ditch runs
    along Y (front to back), the bridge spanned it along X. The near (−X) bank keeps its dry-stone abutment whole —
    three courses with wing walls — and on it the charred deck stub still stands on its stringers, its last plank
    hanging into the gap; the far (+X) abutment is a ruin (one low course, a stone tumbled into the bed), its
    stringer fallen diagonally down into the ditch (art review r1: the two banks read alike, a symmetric jumble);
    the pilings are black stumps standing in the dry bed. ``ground``: the ditch bed alone
    (``SM_Prop_March_BurnedBridge_Ditch``, no ink: a flat decal takes no inverted hull — the screen-space ink of a
    hull a few mm thick smears into a black wedge along its far edge; its LOD1 is its LOD0)."""
    name = "SM_Prop_March_BurnedBridge" + ("_Ditch" if ground else "")
    b, rnd = new(pal, name, lod, decimate=not ground)
    rnd = scene.rng("SM_Prop_March_BurnedBridge")            # one layout for both layers

    # the ditch bed: a dark irregular strip with dried-mud cracks (the ground layer); round it broken bank lips
    # (art review r1: an even halo read as a puddle decal)
    def bed_ring(grow, n: int = 56) -> list:
        """The bed's outline: a long irregular trough (along Y), its banks wandering; ``grow`` (m, or a function of
        the angle) widens it (the bank-slope band round the bed)."""
        out = []
        for k in range(n):
            a = math.tau * k / n
            x, y = math.cos(a), math.sin(a)
            wob = 0.09 * math.sin(a * 3 + 1.0) + 0.05 * math.sin(a * 7 + 2.1) + 0.03 * math.sin(a * 13 + 0.3)
            g = grow(a) if callable(grow) else grow
            rx = 0.95 + wob + g
            ry = 2.25 + 0.1 * math.sin(a * 5 + 0.4) + g * 0.6
            out.append((math.copysign(abs(x) ** 0.45, x) * rx, math.copysign(abs(y) ** 0.75, y) * ry))
        return out

    def lip(a: float) -> float:
        """Bank-lip width round the bed: lobes and near-gaps, never an even band."""
        w = 0.5 + 0.5 * math.sin(a * 3 + 1.3)
        w *= 0.55 + 0.45 * math.sin(a * 7 + 0.4)
        return 0.015 + 0.34 * max(0.0, w) ** 1.4
    bank = M.plate(bed_ring(lip), 0.004, bevel=0.0)
    bank.translate(V(0, 0, 0.002))
    bed = M.plate(bed_ring(0.0), 0.006, bevel=0.0)
    bed.translate(V(0, 0, 0.004))
    if ground:
        b.add(bank, "ground_dark", name="bank", outline=False)
        b.add(bed, "ditch", name="ditch", outline=False)
        # crumbled lip edges: short paler strips where the bank broke over the bed
        for a0, a1, w in ((0.35, 0.95, 0.07), (2.25, 2.7, 0.05), (3.55, 4.2, 0.08), (5.3, 5.7, 0.05)):
            ring = bed_ring(0.03, 112)
            ks = [k for k in range(112) if a0 <= math.tau * k / 112 <= a1]
            pts = [V(ring[k][0], ring[k][1], 0.0075) for k in ks]
            ws = [w * math.sin(math.pi * (i + 0.5) / len(pts)) for i in range(len(pts))]
            b.add(E.ribbon(pts, [V(0, 0, 1)] * len(pts), ws, lift=0.0, thick=0.002), "ground_light", name="lip",
                  outline=False)
    else:
        bank.bm.free()
        bed.bm.free()
    for k in range(5):                                     # dried-mud cracks along the bed
        x0, y0 = rnd.uniform(-0.5, 0.5), -1.7 + 3.4 * (k + rnd.uniform(0.2, 0.8)) / 5
        pts = [V(x0, y0, 0.008)]
        a = rnd.uniform(0, math.tau)
        for _ in range(6):
            a += rnd.uniform(-0.9, 0.9)
            pts.append(pts[-1] + V(math.cos(a) * 0.14, math.sin(a) * 0.14, 0))
        pts = [V(max(-0.8, min(0.8, p.x)), max(-1.95, min(1.95, p.y)), 0.008) for p in pts]
        if ground:
            b.add(E.ribbon(pts, [V(0, 0, 1)] * len(pts), [0.012] * len(pts), lift=0.0, thick=0.002), "mud_crack",
                  name="crack", outline=False)
    if ground:
        return E.finish(b, pal, "none", ground_h=0.3, name=name + ("_LOD1" if lod else ""))
    for sx in (-1, 1):
        for k in range(8):
            y = -2.0 + 4.0 * (k + rnd.uniform(0.1, 0.9)) / 8
            if abs(y) < 0.9:
                continue                                    # the abutment sits there
            rubble(b, V(sx * (1.0 + rnd.uniform(-0.05, 0.12)), y, 0), rnd.uniform(0.07, 0.13), rnd,
                   region="pit_stone", name="bank_stone")
    for x, y, s_ in ((-0.55, 2.2, 0.1), (0.6, 2.15, 0.12), (0.2, -2.25, 0.09), (-0.7, -2.1, 0.11)):
        rubble(b, V(x, y, 0), s_, rnd, region="pit_stone", name="bank_stone")      # stones on the rim's ends
    # near (−X) abutment, whole: two full courses, a coping course under the stringers, wing walls back along ±Y
    for k, (yy, w_) in enumerate(((-0.62, 0.5), (-0.1, 0.55), (0.45, 0.6))):
        dry_stone(b, V(-1.2, yy + rnd.uniform(-0.03, 0.03), 0.2), (0.42, w_, 0.4), rnd)
    for yy in (-0.38, 0.22):
        dry_stone(b, V(-1.22, yy, 0.52), (0.36, 0.5, 0.24), rnd)
    for yy in (-0.62, 0.62):                                 # the wing walls
        dry_stone(b, V(-1.62, yy * 1.1, 0.18), (0.42, 0.34, 0.36), rnd)
        dry_stone(b, V(-1.98, yy * 1.12, 0.14), (0.34, 0.3, 0.28), rnd)
    rubble(b, V(-1.55, -0.95, 0), 0.16, rnd, region="pit_stone", name="abut_stone")
    # far (+X) abutment, ruined: one low course, a stone tumbled into the bed, a spill of rubble
    for yy, w_ in ((-0.5, 0.5), (0.3, 0.55)):
        dry_stone(b, V(1.22, yy + rnd.uniform(-0.03, 0.03), 0.17), (0.42, w_, 0.34), rnd)
    dry_stone(b, V(0.82, -0.05, 0.12), (0.36, 0.42, 0.24), rnd)              # tumbled off the top course
    for x, y, s_ in ((0.62, 0.35, 0.09), (0.74, -0.45, 0.08), (1.5, 0.95, 0.12), (1.0, 0.75, 0.07)):
        rubble(b, V(x, y, 0), s_, rnd, region="pit_stone", name="abut_stone")
    # near bank (−X): two stringers broken off over the gap, the deck stub on them, burned planks hanging
    for yy, tip, droop in ((-0.4, -0.35, 0.06), (0.4, -0.7, 0.03)):
        b.add(beam(V(-2.2, yy, 0.62), V(tip, yy + rnd.uniform(-0.04, 0.04), 0.62 - droop), 0.16, rows=3,
                   jitter=0.08, rnd=rnd), "char_black", name="stringer", hull_pad=True)
    for k in range(7):
        x = -2.12 + 0.2 * k
        if k == 5:
            continue                                       # a plank gone
        L = 1.1 - rnd.uniform(0.0, 0.12)
        tilt = 0.0
        z = 0.725
        if k == 6:                                         # the last one hangs off the near stringer into the gap
            board(b, "char_black", (0.17, 0.9, 0.05), V(x + 0.18, -0.3, 0.42), _rz(90) @ _rx(8) @ _ry(-58), rnd,
                  name="deck", burn=0.2, burn_end=-1)
            continue
        board(b, "charred" if k < 3 else "char_black", (0.18, L, 0.05), V(x, rnd.uniform(-0.05, 0.05), z),
              _rz(90 + rnd.uniform(-3, 3)) @ _rx(rnd.uniform(-2, 2) + tilt), rnd, name="deck",
              burn=0.16 if k >= 4 else 0.0)
    for k in (0, 2):                                       # ash on the deck
        ash_heap(b, V(-2.1 + 0.25 * k, 0.15 * (k - 1), 0.75), 0.14, 0.03, rnd, region="ash_pale", name="ash")
    for yy, h in ((-0.64, 1.1), (0.64, 0.78)):             # rail posts on the abutment
        stump(b, V(-1.85, yy, 0.0), h, 0.07, rnd, region="char_black", lean=3.0, name="rail_post")
    b.add(beam(V(-2.0, -0.66, 0.98), V(-1.2, -0.62, 0.7), 0.07, rows=2, jitter=0.1, rnd=rnd), "char_black",
          name="rail", hull_pad=True)
    # pilings: two bents of charred stumps standing in the dry bed (the near bent taller)
    for x, y, h in ((-0.42, -0.42, 0.66), (-0.42, 0.44, 0.48), (0.4, -0.4, 0.3), (0.44, 0.46, 0.4)):
        stump(b, V(x, y, 0.0), h, 0.09, rnd, region="char_black", lean=7.0, name="piling")
        ash_heap(b, V(x + 0.05, y - 0.05, 0), 0.15, 0.05, rnd, region="ash", name="piling_ash")
    b.add(beam(V(-0.42, -0.42, 0.42), V(-0.42, 0.1, 0.3), 0.07, rows=2, jitter=0.1, rnd=rnd), "char_black",
          name="brace", hull_pad=True)
    # far bank (+X): the stringer fell from the ruined abutment diagonally down into the bed; a burned stub on the
    # bank behind it, planks in the bed
    b.add(beam(V(1.75, -0.62, 0.44), V(0.05, 0.85, 0.06), 0.16, rows=4, jitter=0.07, rnd=rnd), "char_black",
          name="stringer_fallen", hull_pad=True)
    b.add(beam(V(2.35, 0.25, 0.08), V(1.6, 0.42, 0.08), 0.15, rows=2, jitter=0.08, rnd=rnd), "char_black",
          name="stringer", hull_pad=True)
    for x, y, yaw, L in ((0.0, -1.0, 30, 0.9), (-0.25, 1.3, -70, 0.7), (0.6, -0.1, 95, 0.8), (1.95, 0.85, 10, 0.6)):
        board(b, "char_black", (0.18, L, 0.045), V(x, y, 0.03), _rz(yaw) @ _ry(rnd.uniform(-6, 6)), rnd,
              name="plank", burn=0.15)
    for c in (V(-0.2, -0.2, 0), V(0.3, 1.05, 0), V(1.9, 0.0, 0)):
        ash_heap(b, c, 0.22, 0.06, rnd, region="ash", name="ash")
    obj = E.finish(b, pal, "decor", ground_h=1.0, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "smoulder", "pos": [-0.42, -0.42, 0.66]},
                                    {"name": "echo", "pos": [-1.9, 0.0, 0.0]}])
    return obj


# ── wagons: the Sheepfold ring, the east-road barricade ─────────────────────────────────────────────────
WL, WW, WZ, WSIDE, WR = 2.3, 1.15, 0.62, 0.42, 0.42      # bed length / width / floor height / side height / wheel


def wagon(b: M.Builder, rnd: random.Random, xf: Matrix, keep_wheels=(0, 1, 2, 3), broken: dict | None = None,
          tongue: float = 1.0, paint: str = "shield", name: str = "wagon") -> int:
    """A crusade supply wagon built upright (bed along X, front at +X) and placed by ``xf``: plank bed floor and
    sides (the middle side board painted crusade crimson), side stakes, iron corner straps, two axles, wheels
    ``keep_wheels`` (0 front-left … 3 rear-right) with ``broken`` {index: spokes snapped}, a tongue cut to
    ``tongue`` × its length. Returns the floor's part id."""
    broken = broken or {}

    def add(part, region, nm, hull=True, cov=None, proxy=None):
        part.transform(xf)
        if cov is not None:
            return b.add(part, region, name=nm, outline=False, covered_by=cov)
        if proxy is not None:
            proxy.transform(xf)
        return b.add(part, region, name=nm, hull_pad=hull, hull=proxy)

    def bx(size, center, bevel, region, nm):
        """A chamfered board / block with the plain box as its hull proxy (budget, art review r1)."""
        return add(box(size, center, bevel=bevel), region, nm, proxy=hull_box(size, center))
    fid = bx((WL, WW, 0.07), (0, 0, WZ - 0.035), 0.01, "wood_old_dark", name + "_floor")
    for k in range(1, 6):                                  # plank seams on both faces of the floor
        y = -WW / 2 + WW * k / 6
        for z, nz in ((WZ + 0.001, 1), (WZ - 0.071, -1)):
            seam = E.ribbon([V(-WL / 2 + 0.03, y, z), V(0, y, z), V(WL / 2 - 0.03, y, z)], [V(0, 0, nz)] * 3, 0.016,
                            lift=0.0, thick=0.003)
            add(seam, "wood_line", "seam_floor", cov=fid)
    for x in (-0.3, 0.55):                                 # burn scars under the bed
        scar = E.ribbon([V(x - 0.25, -0.2, WZ - 0.072), V(x, 0.05, WZ - 0.072), V(x + 0.2, 0.3, WZ - 0.072)],
                        [V(0, 0, -1)] * 3, [0.12, 0.2, 0.08], lift=0.0, thick=0.003)
        add(scar, "char_black", "soot_scar", cov=fid)
    for sy in (-1, 1):                                    # side boards: three planks, the middle painted
        for k, (z, h) in enumerate(((WZ + 0.07, 0.13), (WZ + 0.21, 0.13), (WZ + 0.35, 0.12))):
            reg_ = paint if k == 1 else "wood_old"
            bx((WL - 0.02, 0.045, h - 0.012), (0, sy * (WW / 2 - 0.02), z), 0.008, reg_, name + "_side")
        for x in (-WL / 2 + 0.08, 0.0, WL / 2 - 0.08):   # stakes
            bx((0.07, 0.06, WSIDE + 0.12), (x, sy * (WW / 2 + 0.01), WZ + WSIDE / 2 + 0.02), 0.01, "wood_old_dark",
               name + "_stake")
        for x in (-WL / 2 + 0.03, WL / 2 - 0.03):        # iron corner straps (own ink: they sit on the bed's corners)
            add(box((0.1, 0.012, WSIDE - 0.04), (x - math.copysign(0.03, x), sy * (WW / 2 + 0.003),
                                                       WZ + WSIDE / 2 + 0.03)), "iron_dark", "rivet_strap")
    for sx in (-1, 1):                                    # end boards
        bx((0.045, WW - 0.06, WSIDE - 0.04), (sx * (WL / 2 - 0.02), 0, WZ + WSIDE / 2 + 0.02), 0.008, "wood_old",
           name + "_end")
    for sx in (-1, 1):                                    # axles + bolsters
        x = sx * (WL / 2 - 0.55)
        bx((0.12, WW + 0.36, 0.1), (x, 0, WR), 0.012, "wood_old_dark", name + "_axle")
        bx((0.14, WW - 0.1, WZ - WR - 0.05), (x, 0, (WZ + WR) / 2 - 0.01), 0.012, "wood_old_dark", name + "_bolster")
    k = 0
    for sx in (1, -1):
        for sy in (-1, 1):
            if k in keep_wheels:
                c = V(sx * (WL / 2 - 0.55), sy * (WW / 2 + 0.14), WR)
                rot = xf.to_3x3().to_4x4()
                wheel(b, xf @ c, WR, rot, rnd, broken=broken.get(k, 0), name=name + "_wheel", rim_region="wood_old_dark",
                      spoke_region="wood_old")
            k += 1
    if tongue > 0:
        L = 1.1 * tongue
        add(beam(V(WL / 2 - 0.4, 0, WR + 0.02), V(WL / 2 + L, 0, WR - 0.08), 0.09, rows=2), "wood_old_dark",
            name + "_tongue")
    return fid


def sack(b: M.Builder, c: Vector, size, yaw: float, rnd: random.Random, region: str = "sack", name: str = "sack"):
    sx, sy, sz = size
    p = M.ellipsoid((sx, sy, sz), (0, 0, 0), sides=10, rings=6, exp=2.4)
    for v in p.bm.verts:
        if v.co.z < -sz * 0.3:
            v.co.z = -sz * 0.3 + (v.co.z + sz * 0.3) * 0.25
    p.transform(_rz(yaw))
    p.translate(c + V(0, 0, sz * 0.3))
    return b.add(p, region, name=name, hull_pad=True)


def grain_sack(b: M.Builder, c: Vector, size, rot: Matrix, rnd: random.Random, region: str = "sack",
               name: str = "sack") -> int:
    """A slumped grain sack lying on its side (art review r1: the ellipsoid sacks read as grey boulders, the
    wagons as ore carts): an undyed-tan pillow (``camp.sack``) flattened where it lies, a gathered neck tied with
    rope at one end, a crease across its top. ``size`` = (length, width, height) lying; ``c`` the middle of its
    footprint on the floor; ``rot`` turns it (its neck toward +X before ``rot``)."""
    L, w, h = size
    rings = [(0.0, h * 0.42, w * 0.42), (L * 0.1, h * 0.5, w * 0.5), (L * 0.45, h * 0.52, w * 0.52),
             (L * 0.72, h * 0.46, w * 0.47), (L * 0.86, h * 0.2, w * 0.2), (L * 0.9, h * 0.1, w * 0.1),
             (L * 0.97, h * 0.16, w * 0.15), (L * 1.03, h * 0.06, w * 0.06)]
    p = M.loft_z(rings, sides=8, cap0="ngon", cap1="fan")
    k1 = rnd.uniform(0, 6.3)
    for v in p.bm.verts:
        a = math.atan2(v.co.y, v.co.x)
        v.co.x *= 1 + 0.07 * math.sin(3 * a + k1)
        v.co.y *= 1 + 0.06 * math.sin(2 * a - k1)
    p.smooth(True)
    m = _ry(90)                                               # length along +X, rx vertical
    p.transform(m)
    for v in p.bm.verts:                                      # it slumps: a flat underside, a sagging top
        v.co.z = max(v.co.z, -h * 0.34) if v.co.z < 0 else v.co.z * (0.86 + 0.1 * math.cos(v.co.x * 6.0 / L))
    lo = min(v.co.z for v in p.bm.verts)
    place = Matrix.Translation(c) @ rot @ Matrix.Translation(V(-L * 0.5, 0, -lo))
    p.transform(place)
    from mathutils.bvhtree import BVHTree
    tree = BVHTree.FromBMesh(p.bm)
    pid = b.add(p, region, name=name, hull_pad=True)
    tie = M.belt(h * 0.11, w * 0.11, L * 0.88, 0.03, 0.012, sides=8)
    tie.transform(place @ m)
    b.add(tie, "rope", name="tuft_tie", outline=False, covered_by=pid)
    up = (rot.to_3x3() @ V(0, 0, 1)).normalized()
    pts = []
    for q in range(5):                                        # the crease across the top, laid on the cloth
        y = (q / 4 - 0.5) * w * 0.8
        o = place @ V(L * 0.42 + 0.03 * math.sin(q * 1.7), y, h * 1.5)
        hit = tree.ray_cast(o, -up, h * 3.0)[0]
        if hit is not None:
            pts.append(hit + up * 0.0015)
    if len(pts) >= 3:
        b.add(E.ribbon(pts, [up] * len(pts), [0.012, 0.016, 0.018, 0.015, 0.01][:len(pts)], lift=0.0, thick=0.002),
              "sack_crease", name="seam_crease", outline=False, covered_by=pid)
    return pid


def supply_crate(b: M.Builder, c: Vector, size, rot: Matrix, rnd: random.Random, region: str = "wood_dark",
                 tally: bool = True, name: str = "crate") -> int:
    """A crusade supply crate in a wagon bed (art review r1: plain cubes did not say 'raised with supplies'). Seen
    from the W1 camera mostly from above, over the bed's side boards: a planked lid (two seams), a seam round the
    sides near the top, chalk tallies (four strokes and a slash) on the lid."""
    sx, sy, sz = size
    m = Matrix.Translation(c) @ rot
    p = box(size, (0, 0, sz / 2), bevel=0.012)
    p.transform(m)
    hp = hull_box(size, (0, 0, sz / 2))
    if hp is not None:
        hp.transform(m)
    pid = b.add(p, region, name=name, hull_pad=True, hull=hp)
    n3 = m.to_3x3()
    up = (n3 @ V(0, 0, 1)).normalized()
    for k in (1, 2):                                          # the lid's planks
        x = -sx / 2 + sx * k / 3
        pts = [m @ V(x, y, sz + 0.0012) for y in (-sy / 2 + 0.02, 0.0, sy / 2 - 0.02)]
        b.add(E.ribbon(pts, [up] * 3, 0.011, lift=0.0, thick=0.002), "wood_line", name="seam_plank", outline=False,
              covered_by=pid)
    z = sz * 0.72                                             # a plank joint round the sides, above the bed boards
    for nx, ny in ((0, -1), (1, 0), (0, 1), (-1, 0)):
        t = V(-ny, nx, 0)
        half = (sx if ny else sy) / 2 - 0.02
        off = V(nx * (sx / 2 + 0.0012), ny * (sy / 2 + 0.0012), z)
        pts = [m @ (off + t * u) for u in (-half, half)]
        b.add(E.ribbon(pts, [n3 @ V(nx, ny, 0)] * 2, 0.01, lift=0.0, thick=0.002), "wood_line", name="seam_plank",
              outline=False, covered_by=pid)
    if tally:                                                 # chalk on the lid: four strokes and a slash
        zt = sz + 0.0016
        for q in range(4):
            x = -sx * 0.3 + q * 0.04
            pts = [m @ V(x, -sy * 0.32, zt), m @ V(x + 0.004, -sy * 0.14, zt), m @ V(x + 0.002, sy * 0.04, zt)]
            b.add(E.ribbon(pts, [up] * 3, 0.012, lift=0.0, thick=0.002), "chalk", name="chalk", outline=False,
                  covered_by=pid)
        pts = [m @ V(-sx * 0.34, -sy * 0.26, zt + 0.0006), m @ V(-sx * 0.2, -sy * 0.12, zt + 0.0006),
               m @ V(-sx * 0.02, sy * 0.0, zt + 0.0006)]
        b.add(E.ribbon(pts, [up] * 3, 0.012, lift=0.0, thick=0.002), "chalk", name="chalk", outline=False,
              covered_by=pid)
    return pid


def build_sheepfold_wagon(pal: Palette, variant: str = "A", lod: int = 0):
    """The Corpseherd's ring (r6, gaps N and S): overturned crusade wagons. A on its side, the bed open toward the
    front (−Y: the inside of the ring), the up-side wheels flat on top, a wheel broken off at its foot; B upside
    down, wheels in the air (one gone, propped against it), the tongue snapped."""
    name = f"SM_Prop_March_SheepfoldWagon_{variant}"
    b, rnd = new(pal, name, lod)
    if variant == "A":
        xf = Matrix.Translation(V(0, 0, WW / 2 + 0.06)) @ _rx(90) @ _rz(rnd.uniform(-3, 3))
        wagon(b, rnd, xf, keep_wheels=(1, 3), broken={1: 2}, tongue=0.55)
        # the lower wheels broke off: one lies flat in front, a spill of sacks and a plank from the bed
        wheel(b, V(0.95, -1.05, 0.05), WR, _rx(90) @ _ry(12), rnd, broken=3, name="wheel_loose")
        grain_sack(b, V(-0.5, -0.55, 0.0), (0.52, 0.36, 0.26), _rz(20), rnd)
        grain_sack(b, V(-0.12, -0.8, 0.0), (0.44, 0.32, 0.22), _rz(-35), rnd, region="canvas")
        board(b, "wood_old", (0.9, 0.14, 0.04), V(0.2, -1.25, 0.02), _rz(-18), rnd, name="plank", burn=0.1)
    else:
        # upside down and rolled ~24° onto its back side edge, the underside (axles, bolsters, wheels in the air)
        # tipped toward the front, the rear end propped on its broken-off wheel, the snapped tongue hanging from the
        # front axle to the ground (art review r1: flat upside down it read as an upright cart from the W1 camera)
        base = _ry(-7) @ _rx(180 + 24) @ _rz(rnd.uniform(-2, 2))
        corners = [base @ V(x, y, z) for x in (-WL / 2, WL / 2) for y in (-WW / 2 - 0.05, WW / 2 + 0.05)
                   for z in (WZ - 0.07, WZ + WSIDE + 0.08)]
        lift = -min(c.z for c in corners)
        xf = Matrix.Translation(V(0, 0, lift)) @ base
        wagon(b, rnd, xf, keep_wheels=(0, 1, 3), broken={3: 1}, tongue=0.0)
        front_axle = xf @ V(WL / 2 - 0.55, 0.0, WR)
        b.add(beam(front_axle + V(0.0, 0.0, 0.02), V(front_axle.x + 0.85, front_axle.y - 0.3, 0.05), 0.09, rows=3),
              "wood_old_dark", name="wagon_tongue", hull_pad=True)
        # the missing wheel, broken, lying tilted under the raised front end (its high rim holds the end up)
        front = [xf @ V(WL / 2, y, WZ + WSIDE + 0.08) for y in (-WW / 2, WW / 2)]
        rz = min(c.z for c in front)
        t = math.asin(min(max((rz - 0.03) / (2 * WR), 0.1), 0.5))
        xc = max(c.x for c in front) - 0.08 - WR * math.cos(t)
        wheel(b, V(xc, (front[0].y + front[1].y) / 2 - 0.05, WR * math.sin(t) + 0.02), WR,
              _rz(rnd.uniform(-10, 10)) @ _ry(-math.degrees(t)) @ _rx(90), rnd, broken=3, name="wheel_loose")
        grain_sack(b, V(1.25, -0.95, 0.0), (0.5, 0.34, 0.26), _rz(50), rnd)
    for k in range(2):
        ash_heap(b, V(rnd.uniform(-1.2, 1.2), rnd.uniform(0.6, 0.9), 0), 0.2, 0.05, rnd, region="ash", name="ash")
    return E.finish(b, pal, "decor", ground_h=1.2, name=name + ("_LOD1" if lod else ""), ground_band=0.02)


# ── the Breach Rim: the dais (Seal of Veins), ash circles, the banner ring, helmet stakes ───────────────
def sector(r0: float, r1: float, a0: float, a1: float, z0: float, z1: float, segs: int = 3,
           bevel: float = 0.012) -> M.Part:
    """A slab of a round floor: the annular sector r0 … r1, a0 … a1 (radians), z0 … z1, its top edge chamfered."""
    rings = []
    for i in range(segs + 1):
        a = a0 + (a1 - a0) * i / segs
        c, s_ = math.cos(a), math.sin(a)
        rin, rout = r0 + bevel * 0.6, r1 - bevel * 0.6
        rings.append([V(c * r0, s_ * r0, z0), V(c * r1, s_ * r1, z0), V(c * r1, s_ * r1, z1 - bevel),
                      V(c * rout, s_ * rout, z1), V(c * rin, s_ * rin, z1), V(c * r0, s_ * r0, z1 - bevel)])
    return E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=False).auto_sharp(30)


def broken_pillar(b: M.Builder, at: Vector, h: float, r: float, rnd: random.Random, region: str = "dais",
                  name: str = "pillar") -> int:
    """An octagonal stone column snapped off at ``h`` (a jagged, sloping break), on a square plinth."""
    rings = [(z, r, r) for z in (-0.04, h * 0.35, h * 0.7, h)]
    p = M.loft_z(rings, sides=8, exp=2.0, cap0="ngon", cap1="ngon", phase=math.pi / 8)
    tilt = V(rnd.uniform(-1, 1), rnd.uniform(-1, 1), 0).normalized()
    for v in p.bm.verts:
        if v.co.z > h * 0.95:
            v.co.z = h - 0.18 * h * (0.5 + 0.5 * (v.co.x * tilt.x + v.co.y * tilt.y) / r) + rnd.uniform(-0.03, 0.03)
    p.flat().auto_sharp(30)
    p.translate(at)
    pid = b.add(p, region, name=name, hull_pad=True)
    base = box((r * 2.5, r * 2.5, 0.16), (at.x, at.y, 0.06), bevel=0.02)
    b.add(base, region, name=name + "_plinth", hull_pad=True)
    return pid


def build_dais(pal: Palette, lod: int = 0):
    """The Seal of Veins (chapter1.md §9.1, §11): Valdras's ground. A low round dais (r 2.3 m, its top 5–7 cm so a
    fight walks over it) of dark stone slabs in two rings round the carved seal — concentric grooves and veins —
    split from the centre to the west (−X) by the breach crack: breach violet (``dais.glow``) glows from below
    through the crack and the gap where two slabs are gone, ``dais.vein`` pinpoints along the veins. Heaved
    slabs, a broken kerb, three snapped pillars."""
    name = "SM_Prop_March_BreachDais"
    b, rnd = new(pal, name, lod)
    R0, R1, R2 = 0.85, 1.55, 2.3
    crack_a = math.pi                                       # the breach runs west (−X)
    # violet light under the floor: shows through every gap
    under = M.cylinder(R2 - 0.05, R2 - 0.05, 0.0, 0.006, sides=24)
    b.add(under, "dais_crack", name="underlay", outline=False)
    # through the gap of the missing outer slab: black depth with glowing fissures and two hot cores
    for k in range(4):
        y0 = rnd.uniform(-0.3, 0.3)
        pts = [V(-R1 - 0.02, y0, 0.009)]
        for j in range(4):
            pts.append(pts[-1] + V(-0.2, rnd.uniform(-0.12, 0.12), 0))
        ws = [0.045, 0.036, 0.026, 0.016, 0.006] if k < 2 else [0.022, 0.018, 0.013, 0.009, 0.004]
        b.add(E.ribbon(pts, [V(0, 0, 1)] * 5, ws, lift=0.0, thick=0.002), "dais_glow", name="fissure",
              outline=False)
    for c_ in (V(-1.75, 0.08, 0.012), V(-2.05, -0.12, 0.012)):        # the two hot cores (kept in LOD1)
        b.add(M.ellipsoid((0.07, 0.045, 0.004), c_, sides=8, rings=3), "dais_vein", name="core", outline=False)
    # the seal stone (two halves either side of the crack), ring 1 (8 slabs), ring 2 (12 slabs)
    gap = 0.018
    for k, (a0, a1) in enumerate(((crack_a + 0.08, crack_a + math.pi - 0.02), (crack_a + math.pi + 0.02,
                                                                                 crack_a + math.tau - 0.08))):
        p = sector(0.0, R0, a0, a1, 0.0, 0.075, segs=10, bevel=0.015)
        b.add(p, "dais_slab_top", name="seal", hull_pad=True)
    for k in range(8):
        a0 = math.tau * k / 8 + 0.2
        a1 = a0 + math.tau / 8
        mid = (a0 + a1) / 2
        if abs(math.atan2(math.sin(mid - crack_a), math.cos(mid - crack_a))) < 0.4:
            # cracked through: two pieces, the outer one heaved up
            p = sector(R0 + gap, R1 - gap, a0 + gap / R0, mid - 0.06, 0.0, 0.066, segs=2)
            b.add(p, "dais_slab", name="slab", hull_pad=True)
            p = sector(R0 + gap, R1 - gap, mid + 0.06, a1 - gap / R0, 0.0, 0.066, segs=2)
            p.transform(Matrix.Translation(V(math.cos(mid) * 1.2, math.sin(mid) * 1.2, 0)) @ _ry(-7)
                        @ Matrix.Translation(V(-math.cos(mid) * 1.2, -math.sin(mid) * 1.2, 0)))
            b.add(p, "dais_slab", name="slab", hull_pad=True)
            continue
        p = sector(R0 + gap, R1 - gap, a0 + gap / R0, a1 - gap / R0, 0.0, 0.066 + rnd.uniform(-0.006, 0.004), segs=3)
        b.add(p, "dais_slab", name="slab", hull_pad=True)
    for k in range(12):
        a0 = math.tau * k / 12 + 0.11
        a1 = a0 + math.tau / 12
        mid = (a0 + a1) / 2
        d = abs(math.atan2(math.sin(mid - crack_a), math.cos(mid - crack_a)))
        if d < 0.3:
            continue                                         # gone: the breach glows up through the gap
        z1 = 0.056 + rnd.uniform(-0.008, 0.004)
        p = sector(R1 + gap, R2 - gap, a0 + gap / R1, a1 - gap / R1, 0.0, z1, segs=3)
        if d < 0.75:                                         # heaved by the breach
            ax = V(-math.sin(mid), math.cos(mid), 0)
            c = V(math.cos(mid) * (R1 + 0.05), math.sin(mid) * (R1 + 0.05), 0)
            p.transform(Matrix.Translation(c) @ Matrix.Rotation(math.radians(rnd.uniform(5, 9)), 4, ax)
                        @ Matrix.Translation(-c))
        b.add(p, "dais_slab", name="slab", hull_pad=True)
    # shards of the missing slab, tipped into the gap
    for k in range(3):
        a = crack_a + rnd.uniform(-0.18, 0.18)
        r_ = rnd.uniform(R1 + 0.15, R2 - 0.15)
        sh = box((rnd.uniform(0.2, 0.3), rnd.uniform(0.14, 0.22), 0.05), (0, 0, 0), bevel=0.01)
        sh.transform(Matrix.Translation(V(math.cos(a) * r_, math.sin(a) * r_, 0.05)) @ _rz(rnd.uniform(0, 180))
                     @ _rx(rnd.uniform(-25, 25)) @ _ry(rnd.uniform(15, 35)))
        b.add(sh, "dais_slab", name="shard", hull_pad=True)
    # the crack through the seal and ring 1: a dark lip with the violet core inside it
    pts = [V(0.12, 0.02, 0)]
    for k in range(1, 9):
        t = k / 8
        pts.append(V(-R1 * t * 1.02, 0.07 * math.sin(k * 2.1) + 0.04 * (t - 0.5), 0))
    zt = 0.0765

    def lay(width, region, lift, nm, thick=0.003):
        pp = [V(p.x, p.y, zt if p.x > -R0 else 0.068) for p in pts]
        ws = [width * (0.4 + 0.6 * k / (len(pp) - 1)) for k in range(len(pp))]
        b.add(E.ribbon(pp, [V(0, 0, 1)] * len(pp), ws, lift=lift, thick=thick), region, name=nm, outline=False)
    lay(0.075, "dais_crack", 0.0, "crack")
    lay(0.032, "dais_glow", 0.002, "crack_glow")
    lay(0.01, "dais_vein", 0.003, "core_line")
    # the carved seal: three concentric grooves and seven veins running out to the rim
    for rr in (0.28, 0.52, 0.76):
        n = 30
        pts_ = [V(math.cos(math.tau * k / n) * rr, math.sin(math.tau * k / n) * rr, 0.0755) for k in range(n + 1)]
        segs_ = []
        cur = []
        for p_ in pts_:                                      # break the groove where the crack runs
            a = math.atan2(p_.y, p_.x)
            if abs(math.atan2(math.sin(a - crack_a), math.cos(a - crack_a))) < 0.16:
                if len(cur) > 1:
                    segs_.append(cur)
                cur = []
            else:
                cur.append(p_)
        if len(cur) > 1:
            segs_.append(cur)
        for sg in segs_:
            b.add(E.ribbon(sg, [V(0, 0, 1)] * len(sg), 0.022, lift=0.0, thick=0.002), "dais_line", name="groove",
                  outline=False)
    for k in range(7):
        a = crack_a + math.tau * (k + 0.5) / 7
        pts_ = []
        for j in range(6):
            t = j / 5
            rr = 0.3 + (R2 - 0.45) * t
            aa = a + 0.12 * math.sin(j * 1.7 + k)
            z = 0.0755 if rr < R0 else (0.0665 if rr < R1 else 0.0565)
            pts_.append(V(math.cos(aa) * rr, math.sin(aa) * rr, z))
        b.add(E.ribbon(pts_, [V(0, 0, 1)] * len(pts_), [0.03, 0.026, 0.022, 0.018, 0.014, 0.01], lift=0.0,
                       thick=0.002), "dais_line", name="groove", outline=False)
        for j in (2, 4):                                     # violet pinpoints where a vein crosses a joint
            p_ = pts_[j]
            dot = M.cylinder(0.018, 0.018, p_.z + 0.001, p_.z + 0.004, sides=6).translate(V(p_.x, p_.y, 0))
            b.add(dot, "dais_vein", name="core_pin", outline=False)
    # the kerb: low curved blocks, five missing, two knocked askew
    for k in range(16):
        a0 = math.tau * k / 16 + 0.06
        a1 = a0 + math.tau / 16 - 0.05
        mid = (a0 + a1) / 2
        if k in (7, 8, 11, 2) or abs(math.atan2(math.sin(mid - crack_a), math.cos(mid - crack_a))) < 0.25:
            continue
        p = sector(R2 + 0.01, R2 + 0.24, a0, a1, -0.02, 0.15 + rnd.uniform(-0.02, 0.02), segs=2, bevel=0.02)
        if k in (5, 13):
            c = V(math.cos(mid) * (R2 + 0.12), math.sin(mid) * (R2 + 0.12), 0)
            p.transform(Matrix.Translation(c + V(math.cos(mid) * 0.12, math.sin(mid) * 0.12, 0)) @ _rz(rnd.uniform(8, 18))
                        @ Matrix.Translation(-c))
        b.add(p, "dais", name="kerb", hull_pad=True)
    # three snapped pillars at the rim
    for a, h in ((math.radians(50), 1.05), (math.radians(140), 0.55), (math.radians(292), 0.8)):
        broken_pillar(b, V(math.cos(a) * (R2 + 0.45), math.sin(a) * (R2 + 0.45), 0.13), h, 0.17, rnd)
        rubble(b, V(math.cos(a + 0.2) * (R2 + 0.85), math.sin(a + 0.2) * (R2 + 0.85), 0), 0.12, rnd, region="dais",
               name="rubble")
    # ash drifts against the kerb
    for k in range(4):
        a = rnd.uniform(0, math.tau)
        if abs(math.atan2(math.sin(a - crack_a), math.cos(a - crack_a))) < 0.5:
            continue
        ash_heap(b, V(math.cos(a) * (R2 - 0.2), math.sin(a) * (R2 - 0.2), 0.04), 0.24, 0.05, rnd, region="ash_pale",
                 name="ash")
    obj = E.finish(b, pal, "decor", ground_h=0.5, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "breach", "pos": [-1.5, 0.0, 0.08]},
                                    {"name": "centre", "pos": [0.0, 0.0, 0.076]}])
    return obj


def build_ritual_circle(pal: Palette, lod: int = 0, ground: bool = False):
    """An ash circle of the Breach Rim (chapter1.md §9.1, §11 P2: a standard-bearer rises at one): a ring of pale
    ash poured on scorched ground, broken where it was walked through, seven spokes to an inner ring, ash heaps
    round it with burnt stakes and crimson rags (the Red-Oath's), a heap at the centre. ``ground``: the poured
    ash and the scorch alone (``SM_Prop_March_RitualCircle_Ash``, no ink, like the bridge's ditch)."""
    name = "SM_Prop_March_RitualCircle" + ("_Ash" if ground else "")
    b, rnd = new(pal, name, lod, decimate=not ground)
    rnd = scene.rng("SM_Prop_March_RitualCircle")
    n = 26
    scorch = [(math.cos(math.tau * k / n) * 1.45 * (1 + 0.07 * math.sin(k * 2.7)),
               math.sin(math.tau * k / n) * 1.45 * (1 + 0.07 * math.sin(k * 1.9 + 1))) for k in range(n)]
    halo = [(x * 1.16, y * 1.16) for x, y in scorch]
    flat = []                                                # (part, region, name): the no-ink ground layer
    flat.append((M.plate(halo, 0.003).translate(V(0, 0, 0.002)), "ground_dark", "scorch"))
    flat.append((M.plate(scorch, 0.004).translate(V(0, 0, 0.003)), "ritual_char", "scorch"))
    # the poured ring (two arcs: a gap where it was walked through). Art review r1: at L* 55-62 with seven even
    # spokes to a hub it was the brightest ground mark of the Rim and read as a cart wheel / ship's helm — now ash
    # at L* ~46 (``ash_poured``), the ring scuffed thin in places, the spokes uneven, two of them broken and smeared
    for a0, a1 in ((0.35, 2.9), (3.25, math.tau - 0.12)):
        m_ = 20
        pts = [V(math.cos(a0 + (a1 - a0) * k / m_) * (1.18 + 0.03 * math.sin(k * 3.1)),
                 math.sin(a0 + (a1 - a0) * k / m_) * (1.18 + 0.03 * math.sin(k * 3.1)), 0.006) for k in range(m_ + 1)]
        ws = [0.15 * (0.55 + 0.45 * abs(math.sin(k * 1.3 + a0))) for k in range(m_ + 1)]
        ws[0] *= 0.4
        ws[-1] *= 0.4
        flat.append((E.ribbon(pts, [V(0, 0, 1)] * len(pts), ws, lift=0.0, thick=0.012), "ash_poured", "ash_ring"))
    m_ = 24                                                  # the inner ring: an arc, scuffed open on one side
    pts = [V(math.cos(math.tau * k / m_ * 0.82 + 1.1) * 0.55, math.sin(math.tau * k / m_ * 0.82 + 1.1) * 0.55, 0.006)
           for k in range(m_ + 1)]
    ws = [0.065 * (0.6 + 0.4 * abs(math.sin(k * 0.9))) for k in range(m_ + 1)]
    ws[0] *= 0.3
    ws[-1] *= 0.3
    flat.append((E.ribbon(pts, [V(0, 0, 1)] * len(pts), ws, lift=0.0, thick=0.008), "ash_poured", "ash_ring"))
    srnd = random.Random(7091)
    for k in range(7):                                       # spokes (radial only: no star, no cross), uneven
        a = math.tau * k / 7 + 0.2 + srnd.uniform(-0.12, 0.12)
        r0, r1 = 0.6 + srnd.uniform(-0.03, 0.08), srnd.uniform(0.86, 1.12)
        wk = srnd.uniform(0.6, 1.25)
        segs = [(r0, r1)]
        if k in (2, 5):                                      # broken: a scuffed gap, the outer half smeared aside
            rm = (r0 + r1) / 2
            segs = [(r0, rm - 0.06), (rm + 0.05, r1 - 0.06)]
        for j, (ra, rb) in enumerate(segs):
            da = srnd.uniform(0.06, 0.14) * (1 if k == 2 else -1) if j else 0.0
            pts = [V(math.cos(a + da * t) * r_, math.sin(a + da * t) * r_, 0.006)
                   for t, r_ in ((0.0, ra), (0.5, (ra + rb) / 2), (1.0, rb))]
            ws = [0.06 * wk, 0.05 * wk * srnd.uniform(0.7, 1.1), 0.026 * wk]
            flat.append((E.ribbon(pts, [V(0, 0, 1)] * 3, ws, lift=0.0, thick=0.008), "ash_poured", "spoke"))
    for k in range(9):                                       # scattered ash: smudges round the ring
        a = srnd.uniform(0, math.tau)
        r_ = srnd.uniform(0.7, 1.45)
        c = V(math.cos(a) * r_, math.sin(a) * r_, 0.0055)
        sm = M.plate([(math.cos(math.tau * q / 7) * srnd.uniform(0.03, 0.06), math.sin(math.tau * q / 7) *
                       srnd.uniform(0.02, 0.04)) for q in range(7)], 0.004)
        sm.transform(Matrix.Translation(c) @ _rz(srnd.uniform(0, 180)))
        flat.append((sm, "ash_poured", "scorch_smudge"))
    if ground:
        for part, reg_, nm in flat:
            b.add(part, reg_, name=nm, outline=False)
        return E.finish(b, pal, "none", ground_h=0.3, name=name + ("_LOD1" if lod else ""))
    for part, _, _ in flat:
        part.bm.free()
    for k in range(5):                                       # ash heaps with burnt stakes and rags
        a = math.tau * k / 5 + 0.55
        c = V(math.cos(a) * 1.32, math.sin(a) * 1.32, 0)
        ash_heap(b, c, 0.17, 0.1, rnd, region="ritual_ash", name="heap")
        h = rnd.uniform(0.42, 0.62)
        stump(b, c + V(0, 0, 0.0), h, 0.035, rnd, region="char_black", lean=10.0, name="stake")
        if k % 2 == 0:                                       # a crimson rag tied below the burnt top
            rag = M.sheet(0.07, 0.05, 0.16, thickness=0.008, cols=2, rows=3, hem_wave=0.01)
            rag.transform(_rz(math.degrees(a) + rnd.uniform(-30, 30)) @ _rx(rnd.uniform(-10, 10)))
            rag.translate(c + V(0, 0, h - 0.12))
            b.add(rag, "ring_cloth", name="rag", hull_pad=True)
    ash_heap(b, V(0.05, -0.04, 0), 0.22, 0.07, rnd, region="ash_pale", name="heap")
    obj = E.finish(b, pal, "decor", ground_h=0.4, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "raise", "pos": [0.0, 0.0, 0.02]}])
    return obj


def strip_plate(shape, thick: float, rows: int = 4, cols: int = 4) -> M.Part:
    """A thin plate of the convex outline ``shape`` (XY, thickness along Z) meshed as a rows × cols strip grid
    (scanlines across Y), so it can be laid on a curved surface vertex by vertex without its faces chording
    through it."""
    ys = [y for _, y in shape]
    y0, y1 = min(ys) + 1e-3, max(ys) - 1e-3

    def span(y):
        xs = []
        n = len(shape)
        for i in range(n):
            (ax, ay), (bx, by) = shape[i], shape[(i + 1) % n]
            if (ay - y) * (by - y) <= 0 and ay != by:
                xs.append(ax + (bx - ax) * (y - ay) / (by - ay))
        return min(xs), max(xs)
    bm = bmesh.new()
    grid = {+1: [], -1: []}
    for j in range(rows + 1):
        y = y0 + (y1 - y0) * j / rows
        xl, xr = span(y)
        for side in (1, -1):
            grid[side].append([bm.verts.new((xl + (xr - xl) * i / cols, y, side * thick / 2))
                               for i in range(cols + 1)])
    for j in range(rows):
        for i in range(cols):
            for side in (1, -1):
                g = grid[side]
                q = [g[j][i], g[j][i + 1], g[j + 1][i + 1], g[j + 1][i]]
                bm.faces.new(q if side > 0 else list(reversed(q)))
    ring = ([(0, i) for i in range(cols + 1)] + [(j, cols) for j in range(1, rows + 1)]
            + [(rows, i) for i in range(cols - 1, -1, -1)] + [(j, 0) for j in range(rows - 1, 0, -1)])
    for k in range(len(ring)):
        (j0, i0), (j1, i1) = ring[k], ring[(k + 1) % len(ring)]
        bm.faces.new([grid[1][j0][i0], grid[-1][j0][i0], grid[-1][j1][i1], grid[1][j1][i1]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).auto_sharp(60)


def palm_plates(s: float, thick: float = 0.0015) -> list:
    """The crusade's open palm (``camp_march.palm_sigil``'s proportions: a solid palm, four fingers fanning up so
    their tips part, the thumb out to the side) as convex :func:`strip_plate` pieces in sigil coordinates (x across,
    y up from the wrist, z through), ``s`` = wrist to the middle fingertip — gridded so each piece can be laid
    vertex by vertex on a curved cloth."""
    out = []

    def piece(w0: float, w1: float, length: float, base: tuple, ang: float, rows: int, cols: int, tip: float = 0.0):
        shape = [(-w0 / 2, 0.0), (w0 / 2, 0.0), (w1 / 2, length - tip), (0.0, length), (-w1 / 2, length - tip)] \
            if tip else [(-w0 / 2, 0.0), (w0 / 2, 0.0), (w1 / 2, length), (-w1 / 2, length)]
        pl = strip_plate(shape, thick, rows=rows, cols=cols)
        ca, sa = math.cos(ang), math.sin(ang)
        for v in pl.bm.verts:
            x, y = v.co.x, v.co.y
            v.co.x, v.co.y = base[0] + x * ca - y * sa, base[1] + x * sa + y * ca
        out.append(pl)
    piece(0.31 * s, 0.43 * s, 0.45 * s, (0.0, 0.0), 0.0, 3, 2)                          # the palm
    for du0, du1, ln in ((-0.135, -0.22, 0.40), (-0.045, -0.075, 0.52), (0.045, 0.075, 0.55), (0.135, 0.215, 0.44)):
        ang = math.atan2(-(du1 - du0), ln)                                                 # fan: tip at du1
        piece(0.085 * s, 0.062 * s, math.hypot(du1 - du0, ln) * s, (du0 * s, 0.40 * s), ang, 3, 1, tip=0.03 * s)
    ang = math.atan2(-(0.33 - 0.16), 0.33 - 0.12)                                          # the thumb, out to the side
    piece(0.095 * s, 0.07 * s, math.hypot(0.17, 0.21) * s, (0.16 * s, 0.12 * s), ang, 2, 1, tip=0.025 * s)
    return out


def ragged_cloth(hem, w0: float, L: float, hang: Vector, T: float = 0.016, stripe=None, band: float = 0.12):
    """A hanging banner cloth as a closed grid sheet: hung at ``hang`` (local y 0), ``w0`` wide, its hem the
    polyline ``hem`` [(x, depth)] (+X → −X, burnt tails), bellied toward −Y and rippling near the hem; ``T``
    thick. Faces within ``band`` of the hem get ``af_sub`` 1 (the char), faces between the ``stripe`` depths 2.
    Returns (part, point(x, y, side, lift)): the world point on the front (side +1, −Y) or back (−1) face at
    local (x, −depth) lifted ``lift`` off it — decals follow the true surface (grid chords ≤ 1 mm)."""
    xs = [x for x, _ in hem]
    ds = [d for _, d in hem]

    def depth_at(x):
        if x >= xs[0]:
            return ds[0]
        for i in range(len(xs) - 1):
            if xs[i] >= x >= xs[i + 1]:
                f = (xs[i] - x) / max(xs[i] - xs[i + 1], 1e-9)
                return ds[i] + (ds[i + 1] - ds[i]) * f
        return ds[-1]

    def belly(x, y):                                          # local y ≤ 0 (down); + = toward −Y (front)
        u = min(max((x + w0 / 2) / w0, 0.0), 1.0)
        zz = -y
        return (0.045 * math.sin(math.pi * u) * min(1.0, zz / 0.4)
                + 0.03 * math.sin(x * 9 + 1.3) * max(0.0, (zz - L * 0.6) / (L * 0.4)))

    def world(x, y, off):
        return V(hang.x + x, hang.y - (belly(x, y) + off), hang.z + y)

    dmin = min(ds)
    cols = []
    for i in range(len(xs) - 1):                              # a column at every hem point and between them
        cols += [xs[i], (xs[i] + xs[i + 1]) / 2]
    cols.append(xs[-1])
    s_h = 1.0 - band / L
    # rows: level at the top (the stripe's edges, and 0.4 m where the belly stops growing — a row on that kink
    # keeps the grid on the analytic surface), then ~14 cm apart down to the hem band, the hem
    srows = [0.0] + ([stripe[0] / dmin, stripe[1] / dmin] if stripe else []) + [z / dmin for z in (0.3, 0.4)]
    srows = sorted(x for x in srows if x < s_h - 0.05)
    n_mid = max(1, int(round((s_h - srows[-1]) * L / 0.14)))
    srows += [srows[-1] + (s_h - srows[-1]) * k / n_mid for k in range(1, n_mid + 1)] + [1.0]

    def ws(s_):                                               # rows run level at the top, follow the hem below
        t = min(max((s_ - 0.3) / 0.7, 0.0), 1.0)
        return t * t * (3 - 2 * t)

    def local(x, s_):
        y = -s_ * (dmin + ws(s_) * (depth_at(x) - dmin))
        flare = 0.02 * min(-y / (0.82 * L), max(0.0, (depth_at(x) + y) / (0.18 * L)), 1.0)
        return x * (1.0 + 2.0 * flare / w0), y
    bm = bmesh.new()
    front, back = [], []
    for s_ in srows:
        rf, rb = [], []
        for x in cols:
            lx, ly = local(x, s_)
            rf.append(bm.verts.new(world(lx, ly, T / 2)))
            rb.append(bm.verts.new(world(lx, ly, -T / 2)))
        front.append(rf)
        back.append(rb)
    R, C = len(srows) - 1, len(cols) - 1
    sub = bm.faces.layers.int.new("af_sub")

    def tag(f, ys):
        yc = sum(ys) / len(ys) - hang.z
        xc = sum(v.co.x for v in f.verts) / len(f.verts) - hang.x
        if yc < -depth_at(xc) + band:
            f[sub] = 1
        elif stripe and -stripe[1] < yc < -stripe[0]:
            f[sub] = 2
    for r in range(R):
        for c in range(C):
            for g, rev in ((front, False), (back, True)):
                q = [g[r][c], g[r][c + 1], g[r + 1][c + 1], g[r + 1][c]]
                f = bm.faces.new(list(reversed(q)) if rev else q)
                tag(f, [v.co.z for v in q])
    rim = ([(0, c) for c in range(C + 1)] + [(r, C) for r in range(1, R + 1)]
           + [(R, c) for c in range(C - 1, -1, -1)] + [(r, 0) for r in range(R - 1, 0, -1)])
    for k in range(len(rim)):
        (r0, c0), (r1, c1) = rim[k], rim[(k + 1) % len(rim)]
        f = bm.faces.new([front[r0][c0], back[r0][c0], back[r1][c1], front[r1][c1]])
        tag(f, [front[r0][c0].co.z, front[r1][c1].co.z])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    part = M.Part(bm).auto_sharp(60)

    def point(x, y, side, lift):
        return world(x, y, side * (T / 2 + lift))
    return part, point


def spear(b: M.Builder, base: Vector, top: Vector, rnd: random.Random, head: bool = True, name: str = "spear") -> int:
    """A crusade spear planted butt-down: ash shaft (``pole``: darker outline mix), an iron leaf head."""
    d = (top - base).normalized()
    pid = b.add(M.tube([base - d * 0.05, base.lerp(top, 0.5), top], [0.028, 0.026, 0.024], sides=7), "pole",
                name=name, hull_pad=True)
    if head:
        side = d.cross(V(0, 0, 1)).normalized() if abs(d.z) < 0.99 else V(1, 0, 0)
        out = d.cross(side).normalized()
        blade_pts = [(0.0, 0.0), (0.035, 0.07), (0.028, 0.16), (0.0, 0.27), (-0.028, 0.16), (-0.035, 0.07)]
        p = M.plate(blade_pts, 0.012, bevel=0.003)
        m = Matrix((side.to_4d(), d.to_4d(), out.to_4d(), (0, 0, 0, 1))).transposed()
        m.translation = top
        p.transform(m)
        b.add(p, "iron", name=name + "_head", hull_pad=True)
        b.add(M.tube([top - d * 0.06, top + d * 0.01], [0.03, 0.026], sides=7), "iron_dark", name=name + "_socket",
              hull_pad=True)
    return pid


def build_ring_banner(pal: Palette, variant: str = "A", lod: int = 0):
    """One of the twelve scorched banners round the dais (radius 9; P3: the ring ignites, socket ``flame``): a
    crusade banner on a planted spear, crossbar lashed on, the cloth (``bannerring.cloth``) burned ragged at the
    hem (``bannerring.char``). A: whole cloth with three burnt tails; B: torn to its upper half, the spear leaning."""
    name = f"SM_Prop_March_RingBanner_{variant}"
    b, rnd = new(pal, name, lod)
    H = 2.7 if variant == "A" else 2.55
    lean = V(0.0, 0.0, 1.0) if variant == "A" else V(0.12, -0.05, 1.0).normalized()
    base = V(0, 0, 0)
    top = base + lean * H
    spear(b, base, top, rnd)
    zb = H - 0.28
    bar_c = base + lean * zb
    side = V(1, 0, 0)
    bar = M.tube([bar_c - side * 0.42, bar_c + side * 0.42], [0.022, 0.022], sides=7)
    b.add(bar, "bar", name="bar", hull_pad=True)
    b.add(E.rope_band((0.036, 0.036), 0, height=0.06, thick=0.01, sides=8).translate(bar_c - V(0, 0, 0.03)), "rope",
          name="tuft_lash", hull_pad=True)
    # the cloth: hangs from the bar, its front −Y, bellied, burnt tails — a real grid sheet (the hem band and the
    # stripe are faces of it) so the faded sigil sits on its true surface on both faces
    L = 1.15 if variant == "A" else 0.68
    w0 = 0.72
    tails = 3 if variant == "A" else 4
    hem = []                                                  # (x, depth) from +X to −X, tips at even k
    for k in range(tails * 2 + 1):
        t = 1.0 - k / (tails * 2)
        dip = L * (1.0 if k % 2 == 0 else 0.78) + rnd.uniform(-0.06, 0.04)
        hem.append((-w0 / 2 + w0 * t, dip))
    hang = bar_c + V(0, -0.03, -0.02)
    cloth, cloth_pt = ragged_cloth(hem, w0, L, hang, T=0.016, stripe=(0.15, 0.21) if variant == "A" else None,
                                   band=0.12)
    cid = b.add(cloth, "ring_cloth", name="cloth", hull_pad=True, sub_regions={1: "ring_char", 2: "ring_char"})
    sway = {cid}
    # the church sigil, faded (art review r1): the crusade's **open palm, fingers up** — the same mark as
    # camp_march.palm_sigil on the pew shields and supply crates, the Unburied tabards and Valdras's standard (the
    # old filled bowl under a teardrop read as a half-moon smile with a drop at 1080p: story-bible §12 forbids
    # crescents). Solid strip-gridded plates (:func:`palm_plates`) laid on the cloth's true surface on both faces
    sz, s_palm = (-0.66, 0.42) if variant == "A" else (-0.44, 0.28)  # wrist below the hang line, palm size
    for side in (1, -1):                                      # +1 front (−Y), −1 back (mirrored: reads the same)
        for pl in palm_plates(s_palm):
            for v in pl.bm.verts:
                v.co = cloth_pt(v.co.x * side, sz + v.co.y, side, 0.002 + v.co.z + 0.00075)
            pl.recalc_normals()
            sway.add(b.add(pl, "sigil_faded", name="sigil", outline=False, covered_by=cid))
    for k in range(3):                                        # stones and ash at the foot
        a = math.tau * k / 3 + 0.3
        rubble(b, V(math.cos(a) * 0.13, math.sin(a) * 0.13, 0), 0.07, rnd, region="stone", name="foot")
    ash_heap(b, V(0, 0, 0), 0.16, 0.05, rnd, region="ash", name="foot_ash")
    hull_ids = {k for k, p in enumerate(b.parts) if p.get("hullOnly") and p["name"].startswith("cloth")}
    wind = E.cloth_wind({p_: (bar_c.z - 0.02, L, 0.2 if variant == "A" else 0.55) for p_ in sway | hull_ids})
    obj = E.finish(b, pal, "decor", ground_h=1.0, wind=wind, name=name + ("_LOD1" if lod else ""))
    obj["af_sockets"] = json.dumps([{"name": "flame", "pos": [bar_c.x, bar_c.y - 0.05, bar_c.z - L * 0.5]}])
    return obj


def build_helmet_stake(pal: Palette, variant: str = "A", lod: int = 0):
    """Helmets on spears along the Dead Ranks approach. A: one kettle helm jammed on a planted spear; B: three
    spears at angles, two helms (one fallen at their foot), a split crusade shield leaning on them."""
    name = f"SM_Prop_March_HelmetStake_{variant}"
    b, rnd = new(pal, name, lod)
    if variant == "A":
        # the helm jammed down over the spear and tilted ~24° on it, the blade through its crown (art review r1)
        base = V(0, 0, 0)
        hc = V(0.07, -0.025, 1.5)                             # the helm's crown base
        hrot = _rx(-20) @ _ry(14)
        apex = hc + hrot.to_3x3() @ V(0, 0, 1.6 * 0.13)
        d = (apex - base).normalized()
        spear(b, base, apex + d * 0.03, rnd, head=True)
        kettle_helm(b, hc, 0.13, hrot, name="helm", strap=V(-0.6, -1.0, 0.0), rnd=rnd)
        ash_heap(b, V(0, 0, 0), 0.15, 0.05, rnd, region="ash", name="foot_ash")
        rubble(b, V(0.14, -0.06, 0), 0.07, rnd, region="stone", name="foot")
        # a strip of tabard tied below the helm
        rag = M.sheet(0.07, 0.05, 0.3, thickness=0.008, cols=2, rows=4, hem_wave=0.015)
        rag.transform(_rz(-20) @ _rx(-6))
        rag.translate(base.lerp(apex, 0.83) + V(-0.01, -0.03, 0.0))
        b.add(rag, "shield", name="rag", hull_pad=True)
    else:
        tops = [V(-0.25, 0.05, 1.5), V(0.12, -0.08, 1.7), V(0.42, 0.12, 1.35)]
        bases = [V(-0.12, 0.0, 0), V(0.1, 0.02, 0), V(0.3, 0.05, 0)]
        h1c, h1r = tops[1] + V(0, 0, -0.09), _rx(-18) @ _ry(14)
        apex1 = h1c + h1r.to_3x3() @ V(0, 0, 1.6 * 0.13)
        for k, (bs, tp) in enumerate(zip(bases, tops)):
            if k == 1:                                        # this one's blade through the helm's crown
                tp = apex1 + (apex1 - bs).normalized() * 0.03
            spear(b, bs, tp, rnd, head=(k != 0))
        kettle_helm(b, tops[0] + V(0, 0, -0.08), 0.12, _rx(22) @ _ry(-16), name="helm", strap=V(-1.0, -0.4, 0.0),
                    rnd=rnd)
        kettle_helm(b, h1c, 0.13, h1r, name="helm")
        kettle_helm(b, V(-0.35, -0.28, 0.06), 0.12, _rx(100) @ _rz(30), name="helm_fallen")
        heater_shield(b, V(0.18, -0.22, 0.3), 0.42, 0.56, _rz(8) @ _rx(-18), rnd, split=True)
        rag = M.sheet(0.07, 0.05, 0.26, thickness=0.008, cols=2, rows=4, hem_wave=0.015)   # a crimson rag
        rag.transform(_rz(25) @ _rx(-8))
        rag.translate(bases[2].lerp(tops[2], 0.82) + V(0.0, -0.03, 0.0))
        b.add(rag, "shield", name="rag", hull_pad=True)
        ash_heap(b, V(0.1, 0.0, 0), 0.26, 0.06, rnd, region="ash", name="foot_ash")
        for k in range(3):
            rubble(b, V(rnd.uniform(-0.3, 0.45), rnd.uniform(0.1, 0.3), 0), 0.07, rnd, region="stone", name="foot")
    return E.finish(b, pal, "decor", ground_h=1.0, name=name + ("_LOD1" if lod else ""), ground_band=0.02)


# ── Knellwatch: the Boneyard Gate, the Name Wall; the sealed Pit mouth ────────────────────────────────────
def flat_stake(w: float, d: float, h: float, tip: float, rows: int = 6) -> M.Part:
    """A squared stake ``w`` × ``d`` rising to ``h`` with a chisel point (``tip`` tall) — every face planar (the
    kit's ``spike`` jitters its rings, and its non-planar quads split the cel bands along their diagonals)."""
    rings = []
    for k in range(rows + 1):
        z = -0.04 + (h - tip + 0.04) * k / rows
        rings.append([V(-w / 2, -d / 2, z), V(w / 2, -d / 2, z), V(w / 2, d / 2, z), V(-w / 2, d / 2, z)])
    rings.append([V(-w * 0.12, -d / 2, h), V(w * 0.12, -d / 2, h), V(w * 0.12, d / 2, h), V(-w * 0.12, d / 2, h)])
    p = E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=False)
    return p.flat().auto_sharp(30)


def pew_stake(b: M.Builder, at: Vector, h: float, rnd: random.Random, yaw: float = 0.0, name: str = "stake",
              region: str = "charred") -> int:
    """A sharpened palisade stake of the march (charred timber, an iron band)."""
    sp = flat_stake(0.15, 0.13, h, 0.16)
    m = _rz(yaw) @ _rx(rnd.uniform(-2, 2)) @ _ry(rnd.uniform(-3, 3))
    sp.transform(m)
    sp.translate(at)
    pid = b.add(sp, region, name=name, hull_pad=True)
    band = E.rope_band((0.078, 0.068), h * 0.62, height=0.05, thick=0.005, sides=12, exp=10.0)
    band.transform(m).translate(at)                       # the band leans with the stake
    b.add(band, "iron_dark", name="seam_band", outline=False, covered_by=pid)
    return pid


def build_boneyard_gate(pal: Palette, lod: int = 0):
    """埋骨门 / the Boneyard Gate (10,32), M5's defend target: where the burial road leaves Knellwatch. A roofed
    timber lychgate — four squared posts on stone pads, tie beams, a steep charred plank roof — over two shut
    leaves braced and faced with crusade shields nailed points-out; the burial bell hangs under the ridge
    (``bell`` socket: the self-ringing siege bell); pew-stake palisade wings either side. No bones (Chapter 1
    has none). Front (−Y) faces the road outside."""
    name = "SM_Prop_March_BoneyardGate"
    b, rnd = new(pal, name, lod)
    HW, DY, PH = 1.35, 0.42, 2.25                            # half width between posts, half depth, post height
    for sx in (-1, 1):
        for sy in (-1, 1):
            at = V(sx * HW, sy * DY, 0)
            b.add(box((0.34, 0.34, 0.16), (at.x, at.y, 0.06), bevel=0.02), "hearth_stone", name="pad",
                  hull_pad=True)
            pid = b.add(beam(at + V(0, 0, 0.1), at + V(0, 0, PH), 0.22, rows=6, jitter=0.03, rnd=rnd), "timber",
                        name="post", hull_pad=True)
            for z in (0.55, 1.75):                           # iron bands
                band = E.rope_band((0.113, 0.113), z, height=0.06, thick=0.006, sides=12, exp=10.0)
                band.translate(at)
                b.add(band, "iron_dark", name="seam_band", outline=False, covered_by=pid)
        # side tie beams (front-back) and knee braces
        b.add(beam(V(sx * HW, -DY - 0.18, PH - 0.08), V(sx * HW, DY + 0.18, PH - 0.08), 0.18, rows=3), "timber",
              name="tie", hull_pad=True)
    for sy in (-1, 1):                                        # long beams across the opening
        b.add(beam(V(-HW - 0.25, sy * DY, PH + 0.06), V(HW + 0.25, sy * DY, PH + 0.06), 0.2, rows=6, jitter=0.03,
                   rnd=rnd), "timber", name="plate", hull_pad=True)
        for sx in (-1, 1):
            b.add(beam(V(sx * HW, sy * DY, PH - 0.55), V(sx * (HW - 0.45), sy * DY, PH - 0.02), 0.1, rows=2),
                  "timber", name="brace", hull_pad=True)
    # the roof: two steep slopes of charred planks over rafters, a ridge board. Art review r1: planks alternating
    # near-black / mid-brown made a barcode that out-read the shielded leaves — now three charred tones within ±3
    # L*, picked at random, two planks burned through (gaps to the rafters) and two burned short
    ridge_z, eave_z, eave_y = PH + 0.82, PH + 0.14, DY + 0.45
    L = HW + 0.45
    tones = ("roof_char", "roof_char_lo", "roof_char_hi")
    gaps = {(-1, 2), (1, 5)}
    for sy in (-1, 1):
        a_ = V(0, sy * eave_y, eave_z)
        c_ = V(0, 0, ridge_z)
        d = (c_ - a_).normalized()
        n = V(1, 0, 0).cross(d) if sy > 0 else d.cross(V(1, 0, 0))
        if n.z < 0:
            n = -n
        L_slope = (c_ - a_).length
        for k in range(9):                                    # planks running down the slope
            x = -L + (2 * L) * (k + 0.5) / 9
            c = (a_ + c_) / 2 + V(x, 0, 0) + n * 0.03
            reg_ = tones[rnd.randrange(3)]
            if (sy, k) in gaps:
                continue                                      # burned through: a gap down to the rafters
            short = 0.0 if not (sy < 0 and k in (6, 7)) else 0.35     # two planks burned short on the front
            m = Matrix((V(1, 0, 0), d, n)).transposed().to_4x4()       # columns: across, down-slope, out
            size = (2 * L / 9 - 0.012, L_slope - short, 0.05)
            pb = box(size, (0, -short / 2, 0), bevel=0.008)
            hp = hull_box(size, (0, -short / 2, 0))
            mm = Matrix.Translation(c) @ m @ _ry(rnd.uniform(-1.5, 1.5))
            pb.transform(mm)
            if hp is not None:
                hp.transform(mm)
            b.add(pb, reg_, name="roof_plank", hull_pad=True, hull=hp)
        for x in (-L + 0.1, 0.0, L - 0.1):                    # rafters (seen under the eaves)
            b.add(beam(a_ + V(x, 0, 0) - n * 0.05, c_ + V(x, 0, 0) - n * 0.05, 0.08, rows=2), "timber",
                  name="rafter", hull_pad=True)
    b.add(beam(V(-L - 0.06, 0, ridge_z + 0.06), V(L + 0.06, 0, ridge_z + 0.06), 0.14, h=0.1, rows=4), "timber",
          name="ridge", hull_pad=True)
    # the burial bell on an iron bracket off the front tie beam, below the eave line (art review r1: hung under the
    # ridge, the roof hid it from the W1 camera): the self-ringing siege bell hangs over the shut leaves
    bell_c = V(0, -DY - 0.3, PH - 0.44)
    b.add(beam(V(0, -DY - 0.08, PH + 0.02), V(0, -DY - 0.34, PH + 0.02), 0.035, rows=1), "iron_dark", name="bracket",
          hull_pad=True)
    b.add(M.tube([V(0, -DY - 0.3, PH + 0.0), bell_c + V(0, 0, 0.32)], [0.012, 0.012], sides=5), "chain",
          name="chain", hull_pad=True)
    bell = M.lathe([(0.2, 0.0), (0.21, 0.02), (0.16, 0.08), (0.12, 0.2), (0.11, 0.28), (0.06, 0.32), (0.0, 0.33)],
                   sides=14, cap0=None, cap1="fan")
    bell.translate(bell_c)
    # LOD0: a closed lathe proxy round the open bell; LOD1 pads the decimated bell itself (a decimated proxy
    # shrinks faster than the bell and padded 34 mm)
    bell_hull = None if lod else M.lathe([(0.21, 0.0), (0.215, 0.02), (0.16, 0.08), (0.12, 0.2), (0.112, 0.28),
                                          (0.062, 0.325), (0.0, 0.34)], sides=14, cap0="ngon", cap1="fan")
    # the camp's bell bronze (env art review: bell.bronze alone lit a sandy L* 60 pear): a dull dark body, the lip
    # the one lit rim
    bid_ = b.add(bell, "bronze_dull", name="bell", hull=bell_hull.translate(bell_c) if bell_hull else None,
                 hull_pad=True)
    b.add(M.belt(0.2, 0.2, 0.004, 0.03, 0.012, sides=14).translate(bell_c), "bronze", name="seam_lip",
          outline=False, covered_by=bid_)
    b.add(M.ellipsoid((0.04, 0.04, 0.05), bell_c + V(0, 0, 0.02), sides=6, rings=3), "bronze_dark", name="clapper",
          outline=False)
    # the shut leaves: vertical planks, three battens and a diagonal brace each, crusade shields nailed on
    for sx in (-1, 1):
        x0, x1 = sx * 0.02, sx * (HW - 0.13)
        xc, w_ = (x0 + x1) / 2, abs(x1 - x0)
        leaf = box((w_, 0.07, 1.9), (xc, -DY + 0.02, 1.04), bevel=0.01)
        lid = b.add(leaf, "wood", name="leaf", hull_pad=True)
        for k in range(1, 5):
            x = min(x0, x1) + w_ * k / 5
            paint(b, "wood_line", [V(x, -DY - 0.017, 0.12), V(x, -DY - 0.017, 1.97)], V(0, -1, 0), 0.014, lid,
                  name="plank_line")
        for z in (0.32, 1.04, 1.76):
            b.add(box((w_ - 0.04, 0.04, 0.13), (xc, -DY - 0.035, z), bevel=0.008), "wood_dark", name="batten",
                  hull_pad=True)
        b.add(beam(V(x1 - sx * 0.06, -DY - 0.04, 0.38), V(x0 + sx * 0.06, -DY - 0.04, 1.7), 0.11, h=0.04, rows=2,
                   up=V(0, -1, 0)), "wood_dark", name="brace_leaf", hull_pad=True)
        heater_shield(b, V(xc, -DY - 0.1, 1.15), 0.5, 0.66, _ry(sx * 4), rnd, split=(sx > 0))
        for z in (0.32, 1.76):                                # iron strap hinges toward the posts
            b.add(box((w_ * 0.55, 0.008, 0.05), (x1 - sx * w_ * 0.27, -DY - 0.058, z)), "iron_dark",
                  name="seam_hinge", outline=False)
    b.add(box((0.1, 0.05, 0.08), (0, -DY - 0.06, 1.0)), "iron_dark", name="seam_latch", outline=False)
    # palisade wings: three charred stakes each side, a helmet on the outer post tops
    for sx in (-1, 1):
        for k, h in enumerate((1.5, 1.42, 1.32)):
            pew_stake(b, V(sx * (HW + 0.45 + 0.19 * k), rnd.uniform(-0.04, 0.04), 0), h, rnd, yaw=rnd.uniform(-8, 8))
    obj = E.finish(b, pal, "decor", ground_h=2.4, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "bell", "pos": [0.0, -DY - 0.3, PH - 0.28]},
                                    {"name": "fx_hit", "pos": [0.0, -DY - 0.1, 1.1]}])
    return obj


def build_name_wall(pal: Palette, lod: int = 0):
    """名墙 / the Name Wall (chapter1.md §2): a stretch of Knellwatch's inner palisade where the bronze name-tags
    of the dead are nailed in rows — hundreds of them (``namewall.tag`` / ``tag_dark``), some hanging on their
    cords; a shelf below holds the unclaimed bundles. Front (−Y) faces the camp."""
    name = "SM_Prop_March_NameWall"
    b, rnd = new(pal, name, lod)
    Wd = 3.0
    n_st = 14
    pids = []
    for k in range(n_st):                                    # the palisade: squared posts, alternate pew boards
        x = -Wd / 2 + Wd * (k + 0.5) / n_st
        h = 2.35 + rnd.uniform(-0.12, 0.1)
        sp = flat_stake(Wd / n_st + 0.008, 0.16, h, 0.2)
        sp.translate(V(x, 0.0, 0))
        pids.append(b.add(sp, "charred" if k % 3 else "pew", name="stake", hull_pad=True))
    for z in (0.62, 1.95):                                  # two rails across the front
        b.add(beam(V(-Wd / 2 - 0.05, -0.13, z), V(Wd / 2 + 0.05, -0.13, z), 0.12, h=0.1, rows=6), "timber",
              name="rail", hull_pad=True)
    # the tags: 8 rows between the rails and below the top rail
    y_face = -0.11
    rows_z = [0.78 + 0.145 * r_ for r_ in range(8)]
    for r_, z in enumerate(rows_z):
        n_t = 30
        for k in range(n_t):
            if rnd.random() < 0.06:
                continue                                    # a gap: a tag taken down, or never nailed
            x = -Wd / 2 + 0.08 + (Wd - 0.16) * (k + 0.5 + rnd.uniform(-0.12, 0.12)) / n_t
            zz = z + rnd.uniform(-0.008, 0.008)
            hang = rnd.random() < 0.1
            rr = rnd.uniform(0.026, 0.031)
            tag = M.cylinder(rr, rr, 0.0, 0.004, sides=7)
            tag.transform(_rx(90) @ _ry(rnd.uniform(-20, 20)))
            if hang:                                        # on its cord, hanging from the nail
                zz -= 0.05
                b.add(M.tube([V(x, y_face - 0.006, zz + 0.05), V(x + rnd.uniform(-0.01, 0.01), y_face - 0.008,
                                                                    zz + rr)], [0.003, 0.003], sides=4),
                      "rope", name="tag_cord", outline=False)
            tag.translate(V(x, y_face - 0.008, zz))
            reg_ = "tag" if rnd.random() < 0.55 else "tag_dark"
            b.add(tag, reg_, name="tag", outline=False)
    # chalk counts on the lower rail, a shelf with bundles of unclaimed tags and two candle stubs (unlit)
    shelf = box((1.2, 0.22, 0.045), (0.35, -0.22, 0.5), bevel=0.008)
    sid = b.add(shelf, "timber", name="shelf", hull_pad=True)
    for x in (-0.12, 0.82):
        b.add(beam(V(x, -0.12, 0.47), V(x, -0.3, 0.3), 0.05, rows=1), "timber", name="shelf_bracket",
              hull_pad=True)
    for x in (0.0, 0.25, 0.55):
        sack(b, V(x, -0.24, 0.52), (0.08, 0.06, 0.05), rnd.uniform(0, 90), rnd, region="canvas", name="bundle")
    for x in (0.72, 0.84):
        b.add(M.cylinder(0.022, 0.02, 0.52, 0.6 + rnd.uniform(0, 0.03), sides=6).translate(V(x, -0.24, 0)),
              "candle_wax", name="candle_stub", hull_pad=True)
    for k in range(4):
        x = -1.35 + 0.09 * k
        paint(b, "ash_pale", [V(x, -0.185, 0.57), V(x + 0.01, -0.185, 0.67)], V(0, -1, 0), 0.012, None, name="chalk")
    paint(b, "ash_pale", [V(-1.38, -0.185, 0.6), V(-1.0, -0.185, 0.64)], V(0, -1, 0), 0.012, None, name="chalk")
    for x in (-1.1, 0.9):                                    # ash at the foot
        ash_heap(b, V(x, -0.3, 0), 0.2, 0.05, rnd, region="ash", name="ash")
    obj = E.finish(b, pal, "decor", ground_h=2.4, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "nail_tag", "pos": [0.9, -0.13, 1.5]}])
    return obj


def build_pit_mouth(pal: Palette, lod: int = 0):
    """The sealed mouth of 信者之坑 / the Pit of the Faithful at (17,26) — the burn pit the hero climbed out of:
    a black opening ringed with stones and banked ash, a squared timber collar, charred planks laid across and
    an iron grate chained down to stakes. Faint smoulder (``brazier.coals``: no light) and a breach seep
    (``pit.seep``) show in the gaps; ``smoke`` socket for the "still smoking" recipe (chapter1.md §6 S7)."""
    name = "SM_Prop_March_PitMouthSealed"
    b, rnd = new(pal, name, lod)
    RX, RY = 1.15, 0.9
    hole = [(math.cos(math.tau * k / 24) * RX * (1 + 0.05 * math.sin(k * 2.2)),
             math.sin(math.tau * k / 24) * RY * (1 + 0.05 * math.sin(k * 1.7 + 1))) for k in range(24)]
    b.add(M.plate(hole, 0.006).translate(V(0, 0, 0.005)), "pit_dark", name="hole", outline=False)
    for k in range(7):                                       # the smoulder and the seep in the depth
        a = rnd.uniform(0, math.tau)
        r_ = rnd.uniform(0.15, 0.75)
        c = V(math.cos(a) * r_ * RX, math.sin(a) * r_ * RY, 0.01)
        reg_ = "coals" if k % 3 else "pit_seep"
        b.add(M.ellipsoid((rnd.uniform(0.05, 0.1), rnd.uniform(0.04, 0.08), 0.004), c, sides=7, rings=3), reg_,
              name="smoulder_dot" if k % 3 else "seep", outline=False)       # (kept in LOD1: the seep is the read)
    # the rim: stones and banked ash all round
    for k in range(16):
        a = math.tau * (k + rnd.uniform(-0.2, 0.2)) / 16
        c = V(math.cos(a) * (RX + 0.18), math.sin(a) * (RY + 0.18), 0)
        rubble(b, c, rnd.uniform(0.11, 0.17), rnd, region="pit_stone", name="rim_stone")
        if k % 2:
            ash_heap(b, c * 1.12, 0.2, 0.07, rnd, region="ash", name="rim_ash")
    # the timber collar: four squared logs framing the opening
    for sy in (-1, 1):
        b.add(beam(V(-RX - 0.15, sy * (RY - 0.05), 0.09), V(RX + 0.15, sy * (RY - 0.05), 0.09), 0.18, rows=5,
                   jitter=0.06, rnd=rnd), "char_black", name="collar", hull_pad=True)
    for sx in (-1, 1):
        b.add(beam(V(sx * (RX - 0.1), -RY - 0.12, 0.2), V(sx * (RX - 0.1), RY + 0.12, 0.2), 0.17, rows=4,
                   jitter=0.06, rnd=rnd), "char_black", name="collar", hull_pad=True)
    # the seal: charred planks across (two broken), the iron grate over the middle, chained to stakes
    for k in range(7):
        x = -RX + 0.12 + (2 * RX - 0.24) * k / 6
        if k in (2,):
            continue
        L = 2 * RY + 0.2 - (0.55 if k == 5 else 0.0)
        yoff = -0.27 if k == 5 else 0.0
        board(b, "char_black" if k % 3 else "charred", (0.2, L, 0.05), V(x, yoff, 0.33 + 0.01 * (k % 2)),
              _rz(rnd.uniform(-4, 4)), rnd, name="seal_plank", burn=0.18 if k in (1, 5) else 0.0)
    grate_c = V(0.05, 0.0, 0.4)
    gw, gd = 1.1, 0.9
    gid = None
    for k in range(6):                                       # bars along X and Y
        y = -gd / 2 + gd * k / 5
        b.add(box((gw, 0.035, 0.035), grate_c + V(0, y, 0), bevel=0.006), "iron", name="grate", hull_pad=True)
    for k in range(6):
        x = -gw / 2 + gw * k / 5
        b.add(box((0.035, gd, 0.035), grate_c + V(x, 0, 0.03), bevel=0.006), "iron", name="grate", hull_pad=True)
    for sx, sy in ((-1, -1), (1, 1), (1, -1)):
        stake_at = V(sx * (RX + 0.42), sy * (RY + 0.3), 0)
        stump(b, stake_at, 0.38, 0.045, rnd, region="timber", lean=4.0, name="chain_stake")
        corner = grate_c + V(sx * gw / 2, sy * gd / 2, 0.02)
        b.add(M.tube([corner, corner.lerp(stake_at + V(0, 0, 0.3), 0.5) - V(0, 0, 0.06), stake_at + V(0, 0, 0.3)],
                     [0.016, 0.016, 0.016], sides=5), "chain", name="chain", hull_pad=True)
    obj = E.finish(b, pal, "decor", ground_h=0.6, name=name + ("_LOD1" if lod else ""), ground_band=0.02,)
    obj["af_sockets"] = json.dumps([{"name": "smoke", "pos": [0.0, 0.0, 0.4]},
                                    {"name": "entrance", "pos": [0.0, 0.0, 0.0]}])
    return obj


# ── the east-road barricade (W7 sealed exit) and the Marshal's order post (lore_rf_01) ───────────────────
def build_wagon_barricade(pal: Palette, lod: int = 0):
    """The east road (119,60), sealed (W7): two crusade supply wagons drawn up side-on end to end in a shallow
    chevron, their sides toward the road (−Y) faced with crusade shields nailed points-down, sacks and crates
    heaped in their beds to raise the wall, a chapel pew jammed in the gap, a rail of sharpened stakes angled out
    in front. The exit's ``exit.sealed`` recipe (breach fire beyond) plays at ``fx_vortex``. Same footprint,
    blocking and socket as the plains sealed gate (``exit_gate_sealed``)."""
    name = "SM_Prop_March_WagonBarricade"
    b, rnd = new(pal, name, lod)
    for k, (x0, yaw) in enumerate(((-1.22, 7.0), (1.22, -7.0))):
        xf = Matrix.Translation(V(x0, 0.35, 0)) @ _rz(yaw)
        wagon(b, rnd, xf, keep_wheels=(0, 1, 2, 3) if k == 0 else (0, 2, 3), broken={2: 2}, tongue=0.0,
              name="wagon")
        side_y = 0.35 - WW / 2 - 0.05                       # the road side of the bed
        for j, xs in enumerate((-0.85, 0.0, 0.85)):          # shields on the side boards, between the wheels
            c = Matrix.Translation(V(x0, 0.35, 0)) @ _rz(yaw) @ V(xs, -WW / 2 - 0.06, WZ + 0.25)
            heater_shield(b, c, 0.46, 0.6, _rz(yaw) @ _ry(rnd.uniform(-7, 7)), rnd, split=(k == 1 and j == 0))
        for j in range(3):                                  # the load heaped in the bed: tied sacks, tallied crates
            c = Matrix.Translation(V(x0, 0.35, 0)) @ _rz(yaw) @ V(-0.7 + 0.7 * j, rnd.uniform(-0.12, 0.12), WZ + 0.0)
            if (j + k) % 2:                                 # two sacks: one lying, one slumped across it
                grain_sack(b, c, (0.62, 0.42, 0.3), _rz(yaw + rnd.uniform(-25, 25) + (180 if j == 2 else 0)), rnd)
                grain_sack(b, c + V(rnd.uniform(-0.05, 0.05), rnd.uniform(-0.05, 0.05), 0.2), (0.52, 0.36, 0.26),
                           _rz(yaw + 90 + rnd.uniform(-30, 30)) @ _rx(rnd.uniform(-8, 8)), rnd)
            else:                                           # a crate standing proud of the side boards
                cr = _rz(yaw + rnd.uniform(-4, 4))
                supply_crate(b, c, (0.5, 0.42, 0.56), cr, rnd, tally=(j == 0))
                if j == 0 and k == 0:                       # and a smaller one stacked on it
                    supply_crate(b, c + V(0.03, 0.02, 0.56), (0.36, 0.32, 0.3), _rz(yaw + 17), rnd, tally=False,
                                 name="crate_top")
    # the pew jammed upright in the gap
    pew = box((0.5, 0.07, 1.3), (0.0, 0.0, 0.0), bevel=0.012)
    pew.transform(Matrix.Translation(V(0.02, -0.05, 0.64)) @ _ry(-8) @ _rz(4))
    b.add(pew, "pew", name="pew", hull_pad=True)
    b.add(box((0.56, 0.36, 0.06), (0.04, -0.22, 0.48), bevel=0.01), "pew_worn", name="pew_seat", hull_pad=True)
    # sharpened stakes angled out toward the road on a ground rail
    for k in range(8):
        x = -2.3 + 4.6 * (k + 0.5) / 8
        base = V(x + rnd.uniform(-0.06, 0.06), -0.62, 0.0)
        tip = base + V(rnd.uniform(-0.08, 0.08), -0.55, 0.8 + rnd.uniform(-0.1, 0.1))
        d = (tip - base)
        sp = flat_stake(0.09, 0.08, d.length, 0.14, rows=1)        # (planar, nothing covered: one row is enough)
        sp.transform(Matrix.Translation(base) @ V(0, 0, 1).rotation_difference(d.normalized()).to_matrix().to_4x4())
        b.add(sp, "charred", name="stake", hull_pad=True)
    b.add(beam(V(-2.4, -0.78, 0.3), V(2.4, -0.76, 0.28), 0.08, rows=6, jitter=0.05, rnd=rnd), "timber",
          name="stake_rail", hull_pad=True)
    for k in range(5):
        rubble(b, V(rnd.uniform(-2.3, 2.3), rnd.uniform(-1.05, -0.85), 0), rnd.uniform(0.07, 0.12), rnd,
               region="stone", name="stone")
    obj = E.finish(b, pal, "interactive", ground_h=1.6, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "fx_vortex", "pos": [0.0, 1.4, 1.25]}])
    return obj


def build_order_post(pal: Palette, lod: int = 0):
    """lore_rf_01 元帅的最后军令 / the Marshal's Last Order (45,35): an order nailed to a post on the Gnawed Field.
    A squared, leaning post on a heap of stones; the parchment (``orderpost.paper``) nailed at the top corners,
    its lower corner curling, three lines of the Marshal's hand and his crimson seal (``orderpost.seal``); a
    second, older notice torn away below. The lore glyph (``lore.glyph``, interaction) is a small warm mark at the
    order's head."""
    name = "SM_Prop_March_OrderPost"
    b, rnd = new(pal, name, lod)
    H = 1.75
    lean = _ry(3.5) @ _rx(-2.0)
    post = beam(V(0, 0, -0.05), V(0, 0, H), 0.15, rows=6, jitter=0.03, rnd=rnd)
    charred_top(post, H, 0.1, rnd)
    post.transform(lean)
    pid = b.add(post, "post", name="post", hull_pad=True)
    yf = -0.076                                               # the post's front face (−Y), in post space
    # parchment: a slightly bowed sheet, the lower right corner curling forward
    W, Hh = 0.34, 0.46
    sheet_ = M.sheet(W, W * 0.98, Hh, thickness=0.006, cols=4, rows=5, sag=0.006)
    for v in sheet_.bm.verts:
        u, t = (v.co.x / W + 0.5), -v.co.z / Hh
        if u > 0.6 and t > 0.7:
            v.co.y -= 0.12 * ((u - 0.6) / 0.4) * ((t - 0.7) / 0.3)
            v.co.z += 0.03 * ((u - 0.6) / 0.4) * ((t - 0.7) / 0.3)
    top_z = 1.48
    sheet_.translate(V(0.0, yf - 0.006, top_z))
    sheet_.transform(lean)
    sid = b.add(sheet_, "paper", name="order", hull_pad=True, sub_regions={1: "paper", 2: "paper"})

    def on_sheet(x, z, lift=0.0025):
        """The parchment's front face (its sag included) at (x, z), ``lift`` in front of it. The marks stand
        inside its outline, never on the silhouette: no ink, no cover."""
        u, t = x / W, min(max((top_z - z) / Hh, 0.0), 1.0)
        sag_y = -0.006 * math.sin(math.pi * t) * (1.0 - (2.0 * u) ** 2)
        return lean @ V(x, yf - 0.006 + sag_y - lift, z)
    nrm = (lean.to_3x3() @ V(0, -1, 0)).normalized()
    for k, (x0, x1) in enumerate(((-0.12, 0.13), (-0.13, 0.1), (-0.13, 0.06), (-0.11, 0.08))):
        z = top_z - 0.1 - 0.075 * k
        pts = [on_sheet(x0 + (x1 - x0) * q / 6, z + 0.006 * math.sin(q * 2.1 + k)) for q in range(7)]
        b.add(E.ribbon(pts, [nrm] * 7, 0.016, lift=0.0, thick=0.002), "ink", name="seam_text", outline=False)
    seal = M.cylinder(0.04, 0.04, -0.0005, 0.005, sides=10).transform(_rx(90)).transform(lean.to_3x3().to_4x4())
    b.add(seal.translate(on_sheet(-0.03, top_z - 0.37, 0.0)), "seal", name="seal", outline=False)
    for x in (-0.14, 0.14):                                   # nails at the top corners
        nail(b, on_sheet(x, top_z - 0.03, 0.001), nrm, None, r=0.012)
    glyph = [on_sheet(-0.02 + 0.03 * math.cos(a), top_z - 0.045 + 0.016 * math.sin(a), 0.003) for a in
             [math.pi * k / 4 for k in range(5)]]
    b.add(E.ribbon(glyph, [nrm] * 5, 0.012, lift=0.0, thick=0.002), "glyph", name="glyph_mark", outline=False)
    # the torn older notice below: a scrap under a nail
    scrap = M.plate([(-0.09, 0.0), (0.1, 0.0), (0.08, -0.07), (0.02, -0.1), (-0.04, -0.06), (-0.09, -0.09)], 0.005)
    scrap.transform(_rx(90))
    scrap.translate(V(0.01, yf - 0.006, 0.86))
    scrap.transform(lean)
    scid = b.add(scrap, "paper", name="scrap", hull_pad=True)
    nail(b, lean @ V(0.0, yf - 0.006 - 0.003, 0.84), nrm, None, r=0.011)
    for k in range(5):                                       # the stone heap at its foot
        a = math.tau * k / 5 + 0.3
        rubble(b, V(math.cos(a) * 0.16, math.sin(a) * 0.16, 0), rnd.uniform(0.08, 0.11), rnd, region="stone",
               name="foot")
    ash_heap(b, V(0, 0, 0), 0.2, 0.06, rnd, region="ash", name="foot_ash")
    obj = E.finish(b, pal, "interactive", ground_h=0.8, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "glyph", "pos": list(on_sheet(-0.02, top_z - 0.045, 0.01))}])
    return obj


# ── iteration renders (scratch) ─────────────────────────────────────────────────────────────────────────
def _px(o) -> float:
    return E.ink_px(o.get("af_family", ""), o.get("af_outline_class", "decor"))


def iter_sheet(objs, tag: str, facing: str = "front", close_res=(640, 480), warrior: bool = True) -> Path:
    """Per asset: a close 3/4 (pitch −30°, magnified shipped ink) and the native-1080p W1 game crop (shipped ink,
    bloom) with the warrior beside it for scale → ``SCRATCH/<tag>.png``."""
    SCRATCH.mkdir(parents=True, exist_ok=True)
    review.stage(blob_radius=0.01)
    w = E.import_warrior(at=(0, 0, 0), yaw_deg=-90) if warrior else None
    wm = w[1] if w else []
    for m in wm:
        m.hide_render = True
    for o in objs:
        o.hide_render = True
    rows = []
    f = Vector(review.cam_dir())
    right = f.cross(V(0, 0, 1)).normalized()
    for o in objs:
        o.hide_render = False
        o.location = (0, 0, 0)
        o.rotation_euler = (0, 0, math.radians(review.FACINGS[facing]))
        bpy.context.view_layer.update()
        px = _px(o)
        pts = review.mesh_points([o])
        review.setup_render(close_res, 8)
        review.frame_points(pts, close_res, fov_h=28, pitch_ue=-30, fill=0.92)
        if px:
            outline.set_preview_screen(o, world=review.ink_world(px) * 1.5)
        close = review.render_array()
        review.draw_text(close, 4, 4, o.name.replace("SM_Prop_March_", "") + f"  {E.tris(o, 0)} / {E.tris(o, 1)}")
        lo, hi = Vector(pts.min(0)), Vector(pts.max(0))
        ext = max(hi.x - lo.x, hi.y - lo.y) / 2
        ctr = (lo + hi) / 2
        if w:
            wp = V(ctr.x, ctr.y, 0) - right * (ext + 0.55)
            w[0].location = wp
            for m in wm:
                m.hide_render = False
        bpy.context.view_layer.update()
        review.setup_render((1920, 1080), 8)
        cam = review.game_camera((ctr.x, ctr.y))
        if px:
            review.set_outline_px([o], px)
        for m in wm:
            review.set_outline_px([m], outline.SCREEN_PX_1080[m.get("af_outline_class", "hero")])
        bx = E.projected_box(cam, [o] + wm)
        m_ = 20
        box = (max(0.0, (bx[0] - m_) / 1920), max(0.0, 1 - (bx[3] + m_) / 1080), min(1.0, (bx[2] + m_) / 1920),
               min(1.0, 1 - (bx[1] - m_) / 1080))
        game = review.render_array(box)
        em = review.emissive_pass([o] + wm, box)
        game = review.bloom(game, em, 1080)
        review.draw_text(game, 4, 4, "1080P 1:1 W1  SHIPPED INK + BLOOM")
        rows.append(review.grid([close, game], 2, pad=2))
        o.hide_render = True
        for m in wm:
            m.hide_render = True
    for o in objs:
        o.hide_render = False
    W_ = max(r.shape[1] for r in rows)
    sheet = review.grid(rows, 1, pad=2) if len(rows) > 1 else rows[0]
    p = SCRATCH / f"{tag}.png"
    pngio.write_png(p, sheet)
    print(f"  {p}")
    return p


# ── registry / CLI ──────────────────────────────────────────────────────────────────────────────────────
def _builders():
    return {
        "SM_Prop_March_AshwheatCottage_A": (lambda pal, lod=0: build_cottage(pal, "A", lod)),
        "SM_Prop_March_AshwheatCottage_B": (lambda pal, lod=0: build_cottage(pal, "B", lod)),
        "SM_Prop_March_BurnedBridge": build_bridge,
        "SM_Prop_March_BurnedBridge_Ditch": (lambda pal, lod=0: build_bridge(pal, lod, ground=True)),
        "SM_Prop_March_SheepfoldWagon_A": (lambda pal, lod=0: build_sheepfold_wagon(pal, "A", lod)),
        "SM_Prop_March_SheepfoldWagon_B": (lambda pal, lod=0: build_sheepfold_wagon(pal, "B", lod)),
        "SM_Prop_March_BreachDais": build_dais,
        "SM_Prop_March_RitualCircle": build_ritual_circle,
        "SM_Prop_March_RitualCircle_Ash": (lambda pal, lod=0: build_ritual_circle(pal, lod, ground=True)),
        "SM_Prop_March_RingBanner_A": (lambda pal, lod=0: build_ring_banner(pal, "A", lod)),
        "SM_Prop_March_RingBanner_B": (lambda pal, lod=0: build_ring_banner(pal, "B", lod)),
        "SM_Prop_March_HelmetStake_A": (lambda pal, lod=0: build_helmet_stake(pal, "A", lod)),
        "SM_Prop_March_HelmetStake_B": (lambda pal, lod=0: build_helmet_stake(pal, "B", lod)),
        "SM_Prop_March_BoneyardGate": build_boneyard_gate,
        "SM_Prop_March_NameWall": build_name_wall,
        "SM_Prop_March_PitMouthSealed": build_pit_mouth,
        "SM_Prop_March_WagonBarricade": build_wagon_barricade,
        "SM_Prop_March_OrderPost": build_order_post,
    }


LAYER = ("UE set-piece layer (U4, set_pieces.json → prop groups at tiles; non-blocking by default; "
         "Private/World/AbyssSetPieces.cpp)")
# per asset: game ids (plain set-piece ids; ``@march`` themed slots via kit.theme), footprint, blocking, where, notes
SPECS = {
    "SM_Prop_March_AshwheatCottage_A": dict(ids=["setpiece_ashwheat_cottage"], themed=["goblin_hut"], fp=[2, 2],
                                            blob=1.2, where="Ashwheat (40,52) r8",
                                            note="the goblin-camp hut slot re-themed (the camp at (40,52) is Ashwheat); "
                                                 "door shut and barred from inside, knocking emitter at socket knock, "
                                                 "a cold candle at socket candle (not a light: chapter1.md §15)"),
    "SM_Prop_March_AshwheatCottage_B": dict(ids=["setpiece_ashwheat_cottage"], themed=["goblin_hut"], fp=[2, 2],
                                            blob=1.2, where="Ashwheat (40,52) r8", note="variant B: half the thatch gone"),
    "SM_Prop_March_BurnedBridge": dict(ids=["setpiece_burned_bridge"], fp=[5, 4], blob=1.0, where="(38,48)",
                                       note="the ditch runs along the mesh's Y (front to back), the bridge spanned X; "
                                            "lore_rf_echo_02 (38,47) sits at the near bridgehead (socket echo)"),
    "SM_Prop_March_BurnedBridge_Ditch": dict(ids=["setpiece_burned_bridge_ditch"], fp=[3, 5], blob=0.01,
                                             where="(38,48), under SM_Prop_March_BurnedBridge (same tile and yaw)",
                                             note="the dry ditch bed: a flat ground layer without ink (ground cover "
                                                  "rule), placed with the bridge"),
    "SM_Prop_March_SheepfoldWagon_A": dict(ids=["setpiece_sheepfold_wagon"], fp=[3, 2], blob=1.0,
                                           where="the Sheepfold (60,55): ring r6, gaps N and S",
                                           note="front (−Y, the open bed) faces the ring's centre"),
    "SM_Prop_March_SheepfoldWagon_B": dict(ids=["setpiece_sheepfold_wagon"], fp=[3, 2], blob=1.0,
                                           where="the Sheepfold (60,55): ring r6, gaps N and S",
                                           note="variant B: upside down"),
    "SM_Prop_March_BreachDais": dict(ids=["setpiece_breach_dais"], fp=[5, 5], blob=0.4, violet=True,
                                     where="the Breach Rim (15,95), centre of the banner ring",
                                     note="walkable (top 5-7 cm); the crack and the missing slab face −X (west, the "
                                          "chasm decal's side): yaw 0 at the tile; violet uplight / sparks recipe at "
                                          "socket breach; Valdras kneels at socket centre"),
    "SM_Prop_March_RitualCircle": dict(ids=["setpiece_ritual_circle"], fp=[3, 3], blob=0.3,
                                       where="the Breach Rim (11,91), (19,99)",
                                       note="walkable; P2's dead_standard_bearer rises at socket raise"),
    "SM_Prop_March_RitualCircle_Ash": dict(ids=["setpiece_ritual_circle_ash"], fp=[3, 3], blob=0.01,
                                           where="the Breach Rim (11,91), (19,99), under SM_Prop_March_RitualCircle",
                                           note="the poured ash ring and scorch: a flat ground layer without ink, "
                                                "placed with the circle (same tile and yaw)"),
    "SM_Prop_March_RingBanner_A": dict(ids=["setpiece_banner_ring"], fp=[1, 1], blob=0.3, wind=True,
                                       where="12 at radius 9 round the dais (15,95)",
                                       note="front (−Y) faces the dais; P3 ignition recipe at socket flame"),
    "SM_Prop_March_RingBanner_B": dict(ids=["setpiece_banner_ring"], fp=[1, 1], blob=0.3, wind=True,
                                       where="12 at radius 9 round the dais (15,95)", note="variant B: torn, leaning"),
    "SM_Prop_March_HelmetStake_A": dict(ids=["setpiece_helmet_stake"], fp=[1, 1], blob=0.25,
                                        where="along the Dead Ranks approach (30,80) → the Breach Rim", note=""),
    "SM_Prop_March_HelmetStake_B": dict(ids=["setpiece_helmet_stake"], fp=[1, 1], blob=0.35,
                                        where="along the Dead Ranks approach (30,80) → the Breach Rim",
                                        note="variant B: three spears"),
    "SM_Prop_March_BoneyardGate": dict(ids=["decor_defend_boneyard_gate", "setpiece_boneyard_gate"], fp=[4, 1],
                                       blob=1.0, where="the Boneyard Gate (10,32)",
                                       note="M5's defendTarget spriteKey (GameData quests: decor_defend_boneyard_gate): "
                                            "front (−Y) faces the road outside, i.e. the waves' approachDeg; the "
                                            "self-ringing siege bell at socket bell, hit sparks at fx_hit"),
    "SM_Prop_March_NameWall": dict(ids=["setpiece_name_wall"], fp=[3, 1], blob=0.6,
                                   where="Knellwatch (15,15), along the inner palisade",
                                   note="front (−Y) faces the camp; Hald's recovered tag (q_rf_tag_thief) at "
                                        "socket nail_tag"),
    "SM_Prop_March_PitMouthSealed": dict(ids=["setpiece_pit_mouth_sealed"], fp=[3, 3], blob=0.5,
                                         where="the Pit entrance (17,26) (entrance_rf_pit, sealed until W3)",
                                         note="walk-over seal; the smoke recipe at socket smoke; socket entrance = "
                                              "the pit's centre on the ground (z 0, the mesh's axes, −Y the "
                                              "approach side): the subDungeonEntrances point (17,26) where the "
                                              "runtime anchors the sealed-entrance interaction (inert until W3) and, "
                                              "once the seal breaks, the descent"),
    "SM_Prop_March_WagonBarricade": dict(ids=[], themed=["exit_gate_sealed", "exit_sealed"], fp=[1, 1], blob=1.0,
                                         where="the east road exit (119,60), W7",
                                         note="same footprint / blocking / socket as SM_Prop_ExitGate_Sealed: the hero "
                                              "walks onto the exit tile for the message; exit.sealed (breach fire "
                                              "beyond) at fx_vortex"),
    "SM_Prop_March_OrderPost": dict(ids=["lore_order_post"], themed=["lore_ancient_tablet"], fp=[1, 1], blob=0.3,
                                    where="lore_rf_01 (45,35)",
                                    note="lore collectible: interactive 2 px ink, the warm lore glyph is the focal "
                                         "accent; lore_order_post = the story bible's order_post look"),
}


def game_ids_of(name: str) -> list[str]:
    sp = SPECS[name]
    return list(sp.get("ids", [])) + theme.themed_ids(sp.get("themed", []), EM.THEME)


def ship_one(name: str, previews: bool = True, verify: bool = True) -> dict:
    """Build (LOD0 + LOD1), check reserved light, export, manifest, previews, ink gate, LOD1, re-import check."""
    builders = _builders()
    sp = SPECS[name]
    scene.reset()
    pal = Palette(FAM)
    lod_obj = builders[name](pal, 1)
    extra = {"displayName": name.split("_", 3)[-1], "family": FAMILY, "theme": EM.THEME,
             "placement": f"{LAYER}: {sp['where']}", "note": sp["note"]}
    if sp.get("themed"):
        extra["themeOf"] = list(sp["themed"])
        extra["themeNote"] = ("presentation theme march (chapter1.md §15): the runtime resolves <id>@march before "
                              "<id> for emerald_plains (U1)")
    if sp.get("wind"):                                       # the LOD1 carries the same toon slot name as LOD0
        orig = lod_obj.data.materials[0]
        m1 = orig.copy()
        m1.name = EM.PROP_WIND_SLOT
        lod_obj.data.materials[0] = m1
        extra["lods"] = [E.export_lod1(lod_obj, name, CAT)]
        lod_obj.data.materials[0] = orig
        bpy.data.materials.remove(m1)
    else:
        extra["lods"] = [E.export_lod1(lod_obj, name, CAT)]
    lod_obj.hide_render = True
    obj = builders[name](pal)
    pal.save()
    pads = pad_report(name)
    print(f"[pad] {name}: largest hull pad {pads['maxMm']:.1f} mm (gate {PAD_MAX_MM:.0f} mm)"
          + ("" if pads["pass"] else "  FAIL " + "; ".join(pads["over"])))
    problems = EM.check_light(obj, pal, allow_violet=sp.get("violet", False))
    problems += EM.check_light(lod_obj, pal, allow_violet=sp.get("violet", False))
    if problems:
        raise RuntimeError(f"{name}: " + "; ".join(problems))
    normals = {f"lod{k}": normal_sanity(o.data) for k, o in ((0, obj), (1, lod_obj))}
    normals["pass"] = all(v["pass"] for v in normals.values())
    print(f"[normals] {name}: " + "  ".join(f"{k} {v['badFrac'] * 100:.1f} % bad, {v['flipped']} flipped"
                                            for k, v in normals.items() if k != "pass")
          + ("" if normals["pass"] else f"  FAIL (gate {NORMAL_BAD_MAX * 100:.0f} %)"))
    sockets = json.loads(obj.get("af_sockets", "[]"))
    extra["budget"] = {"lod0": {"toon": E.tris(obj, 0), "outline": E.tris(obj, 1)},
                       "lod1": dict(extra["lods"][0]["triangles"])}
    kw = {}
    if sp.get("wind"):
        kw.update(wind_slot=EM.PROP_WIND_SLOT, toon_params=EM.PROP_WIND_PARAMS)
    rx = (lambda rv, l0=obj, l1=lod_obj: E.lod_preview(rv, l0, l1))
    res = E.ship(obj, name, CAT, pal, obj["af_outline_class"], game_ids=game_ids_of(name), footprint=sp["fp"],
                 blocking=False, sockets=sockets, extra=extra, previews=previews, blob=sp.get("blob", 0.3),
                 review_extra=rx if previews else None, close_facing="se", **kw)
    res["pad"] = pads
    res["normals"] = normals
    if verify:
        res["verify"] = verify_static(name)
    return res


def verify_static(name: str) -> dict:
    """Re-import ``SM_<name>.fbx`` and its ``_LOD1`` into an empty scene and check them against the manifest entry:
    one mesh, the material slot names, the triangles per slot, the bounds, AF_Data + custom normals + UVMap (+
    UVWind for cloth), every socket empty at its manifest location, the node transform identity."""
    entry = export.Manifest().data["assets"][name]
    fails = []

    def check(ok, msg):
        print(("  ok   " if ok else "  FAIL ") + msg)
        if not ok:
            fails.append(msg)
    print(f"\n[verify] {name}: re-import into an empty scene")
    for fbx_rel, tri_want, label in ((entry["fbx"], entry["triangles"], "LOD0"),
                                     (entry["lods"][0]["fbx"], entry["lods"][0]["triangles"], "LOD1")):
        scene.reset()
        bpy.ops.import_scene.fbx(filepath=str(paths.export_root() / fbx_rel))
        objs = list(bpy.data.objects)
        meshes = [o for o in objs if o.type == "MESH"]
        check(len(meshes) == 1, f"{label}: one mesh ({len(meshes)})")
        if not meshes:
            continue
        me = meshes[0]
        mats = [m.name.split(".")[0] for m in me.data.materials]
        want = [s_["name"] for s_ in entry["materialSlots"]]
        check(mats == want, f"{label}: material slots {mats}")
        got = {"toon": M.tri_count(me, 0), "outline": M.tri_count(me, 1)}
        check(got == {"toon": tri_want["toon"], "outline": tri_want["outline"]}, f"{label}: triangles {got}")
        check("AF_Data" in me.data.color_attributes and me.data.has_custom_normals and "UVMap" in me.data.uv_layers,
              f"{label}: AF_Data + custom normals + UVMap")
        ns = normal_sanity(me.data)
        check(ns["pass"], f"{label}: normals {ns['badFrac'] * 100:.1f} % of the toon loops > 60° off their face, "
                          f"{ns['flipped']} flipped (gate {NORMAL_BAD_MAX * 100:.0f} %)")
        if "wind" in entry:
            check("UVWind" in me.data.uv_layers, f"{label}: UVWind (cloth sway)")
        mw = me.matrix_world
        check(all(abs(mw[i][j] - (i == j)) < 1e-5 for i in range(4) for j in range(4)),
              f"{label}: node transform identity")
        if label == "LOD0":
            lo, hi = scene.world_bounds([me])
            hb = entry["boundsCm"]
            check(abs(hi.z * 100 - hb["max"][2]) < 1.0 and abs(lo.z * 100 - hb["min"][2]) < 1.0,
                  f"bounds z {lo.z:.3f} .. {hi.z:.3f} m == manifest")
            for sk in entry.get("sockets", []):
                e_ = next((o for o in objs if o.name == f"SOCKET_{sk['name']}"), None)
                if e_ is None:
                    check(False, f"SOCKET_{sk['name']} present")
                    continue
                d = (Vector(scene.blender_to_ue_cm(e_.matrix_world.translation)) - Vector(sk["locCm"])).length
                check(d < 0.5, f"SOCKET_{sk['name']} at the manifest location ({d:.2f} cm)")
    print(f"[verify] {'PASS' if not fails else 'FAIL'} ({len(fails)} failures)")
    return {"pass": not fails, "failures": fails}


# ── report, family sheets, composed scenes, the echo ──────────────────────────────────────────────────────
PREV_DIR = "setpieces_march"


def write_report(results: dict) -> Path:
    """``Previews/setpieces_march/report.json``: per shipped asset its triangles, ink gate and re-import check
    (merged with the assets already in the report)."""
    out = paths.preview_root() / PREV_DIR / "report.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    data = json.loads(out.read_text()) if out.exists() else {}
    for n, r in results.items():
        data[n] = {"gameIds": game_ids_of(n), "toonTris": r["toonTris"], "hullTris": r["hullTris"],
                   "ink": r.get("ink"), "inkFailures": r.get("inkFailures", []), "verify": r.get("verify"),
                   "pad": r.get("pad"), "normals": r.get("normals")}
    out.write_text(json.dumps(data, indent=1, sort_keys=True) + "\n")
    return out


def _slot_mats(m, mt, mo) -> None:
    mats = m.data.materials
    for i in range(len(mats)):
        nm = mats[i].name if mats[i] is not None else ""
        mats[i] = mo if nm.startswith("M_AF_Outline") else mt


def import_asset(name: str, at=(0, 0, 0), yaw_deg: float = 0.0, clip: str | None = None, frame: int = 0,
                 hide_attach: bool = False, clip_from: str | None = None):
    """A shipped manifest asset (static or skeletal, with its clip posed at ``frame`` and its attachments) in its
    own palette's toon look, from the exported FBX + palette PNGs. ``clip_from``: the asset whose clip list holds
    ``clip`` (another mesh on the same skeleton). Returns (root, [meshes])."""
    man = export.Manifest().data["assets"]
    e = man[name]
    fam = e["materialSlots"][0]["palette"]
    mt, mo = E.toon_material_files(fam)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(paths.export_root() / e["fbx"]))
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    arm = next((o for o in new if o.type == "ARMATURE"), None)
    cls = (e["materialSlots"][1].get("class", "decor") if len(e["materialSlots"]) > 1 else "decor")
    for m in meshes:
        _slot_mats(m, mt, mo)
        m["af_outline_width"] = (e["materialSlots"][1].get("widthCm", 1.2) if len(e["materialSlots"]) > 1 else 0) / 100.0
        m["af_outline_class"] = cls
        m["af_family"] = fam
    for o in new:
        if o.type == "EMPTY":
            o.hide_render = True
    root = bpy.data.objects.new(f"Root_{name}", None)
    scene.link(root)
    for o in new:
        if o.parent is None:
            o.parent = root
    if arm is not None and clip:
        clip_e = next((c for c in man[clip_from or name].get("anims", []) if c["name"] == clip), None)
        if clip_e is not None:
            b2 = set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=str(paths.export_root() / clip_e["fbx"]))
            new2 = [o for o in bpy.data.objects if o not in b2]
            arm2 = next((o for o in new2 if o.type == "ARMATURE"), None)
            if arm2 is not None:
                for pb in arm.pose.bones:
                    c = pb.constraints.new("COPY_TRANSFORMS")
                    c.target, c.subtarget = arm2, pb.name
                    c.target_space = c.owner_space = "POSE"
                act = arm2.animation_data.action if arm2.animation_data else None
                if act is not None:
                    bpy.context.scene.frame_set(int(act.frame_range[0]) + frame)
            for o in new2:
                o.hide_render = True
    if arm is not None and not hide_attach:
        from types import SimpleNamespace
        from kit import rig as krig
        for att in e.get("attachments", []) or []:
            an = att.get("object") or att.get("asset")
            ae = man.get(an)
            if not ae:
                continue
            b3 = set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=str(paths.export_root() / ae["fbx"]))
            new3 = [o for o in bpy.data.objects if o not in b3]
            wm = next((o for o in new3 if o.type == "MESH"), None)
            for o in new3:
                if o is not wm:
                    bpy.data.objects.remove(o)
            if wm is None:
                continue
            fam2 = ae["materialSlots"][0]["palette"]
            mt2, mo2 = E.toon_material_files(fam2)
            _slot_mats(wm, mt2, mo2)
            wm["af_outline_width"] = (ae["materialSlots"][1].get("widthCm", 1.3) if len(ae["materialSlots"]) > 1
                                      else 0) / 100.0
            wm["af_outline_class"] = ae["materialSlots"][1].get("class", "weapon") if len(ae["materialSlots"]) > 1 \
                else "weapon"
            krig.attach_to_bone(wm, SimpleNamespace(obj=arm), att.get("socket", "weapon_r"))
            meshes.append(wm)
    root.location = Vector(at)
    root.rotation_euler = (0, 0, math.radians(yaw_deg))
    bpy.context.view_layer.update()
    return root, meshes


def pose_arms_behind(root) -> None:
    """Preview stand-in for the A9 ``Kneel_Bound`` pose (art review r1, lore_rf_echo_03: "kneeling shade, hands
    still bound behind"): on the posed SKEL_Human armature under ``root``, drop the clip's arm constraints and lay
    each upper arm down and back from the shoulder, the forearms in to the small of the back, wrists crossed there
    (the Bound Pilgrim's rope rides on the wrists)."""
    arm = next((o for o in root.children if o.type == "ARMATURE"), None)
    if arm is None:
        return
    pb = arm.pose.bones
    for side in ("l", "r"):
        for bn in (f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}", f"fingers_01_{side}", f"thumb_01_{side}"):
            if bn in pb:
                for c in list(pb[bn].constraints):
                    pb[bn].constraints.remove(c)
    bpy.context.view_layer.update()
    pel, sp3 = pb["pelvis"].head.copy(), pb["spine_03"].tail.copy()
    up = (sp3 - pel).normalized()
    lat = (pb["clavicle_l"].head - pb["clavicle_r"].head).normalized()      # toward the left shoulder
    back = lat.cross(up).normalized()
    if back.dot(Vector((0, 1, 0))) < 0:                       # the rig's forward is −Y: its back +Y
        back = -back
    small = pel + up * 0.2 + back * 0.17                      # the small of the back

    def aim(bone, head: Vector, d: Vector) -> Vector:
        d = d.normalized()
        x = d.cross(back).normalized() if abs(d.dot(back)) < 0.95 else d.cross(up).normalized()
        z = x.cross(d)
        m = Matrix((x, d, z)).transposed().to_4x4()
        m.translation = head
        bone.matrix = m
        bpy.context.view_layer.update()
        return head + d * bone.length
    for side, sg in (("l", 1.0), ("r", -1.0)):
        sh = pb[f"upperarm_{side}"].head.copy()
        elbow = aim(pb[f"upperarm_{side}"], sh, -up * 0.72 + back * 0.62 + lat * sg * 0.12)
        wrist_goal = small + lat * sg * -0.035                # the wrists cross over the spine
        wrist = aim(pb[f"lowerarm_{side}"], elbow, wrist_goal - elbow)
        aim(pb[f"hand_{side}"], wrist, (wrist_goal - elbow).normalized() * 0.7 - up * 0.3)


def _place(obj, x: float, y: float, yaw_deg: float = 0.0):
    obj.location = (x, y, 0.0)
    obj.rotation_euler = (0, 0, math.radians(yaw_deg))
    return obj


def _face(x: float, y: float, tx: float, ty: float) -> float:
    """Yaw (deg) turning a −Y-facing asset at (x, y) to face (tx, ty)."""
    return math.degrees(math.atan2(ty - y, tx - x)) + 90.0


def _sp(r: float, t: float) -> tuple[float, float]:
    """Blender XY of a point ``r`` m to the screen right and ``t`` m toward the W1 camera (yaw 45°: screen right is
    (−1, −1)/√2, toward the camera (−1, 1)/√2). A −Y-facing asset at yaw −135° faces the camera."""
    k = math.sqrt(0.5)
    return (-k * r - k * t, -k * r + k * t)


def build_scene(tag: str, pal: Palette):
    """The composed set pieces (Blender metres, the focus at the origin, laid out in screen terms with :func:`_sp`):
    returns (meshes, focus, size_px, warrior (x, y, yaw))."""
    B = _builders()
    exp_ = paths.export_root()
    if tag == "ashwheat":
        ms = []
        for n, (r, t), yaw in (("SM_Prop_March_AshwheatCottage_B", (0.6, -2.6), -150),
                               ("SM_Prop_March_AshwheatCottage_A", (-3.6, 0.2), -112),
                               ("SM_Prop_March_AshwheatCottage_A", (3.6, -0.4), -162)):
            ms.append(_place(B[n](pal), *_sp(r, t), yaw))
        for n in ("SM_Prop_March_BurnedBridge", "SM_Prop_March_BurnedBridge_Ditch"):
            ms.append(_place(B[n](pal), *_sp(-1.6, 3.6), 45))
        # story_rf_belfry (story_march: renamed from SM_Prop_March_FallenBelfry — story-bible §12 never-use list)
        if (exp_ / "Props" / "SM_Prop_March_BrokenBelfry.fbx").exists():
            _, mm = import_asset("SM_Prop_March_BrokenBelfry", at=(*_sp(4.4, 2.8), 0), yaw_deg=-135)
            ms += mm
        return ms, (0.0, 0.0, 0.0), (1920, 1080), (*_sp(0.6, 1.4), -45)
    if tag == "breach_rim":
        ms = [_place(B["SM_Prop_March_BreachDais"](pal), 0, 0, 0)]
        for (x, y) in ((-4.0, 4.0), (4.0, -4.0)):
            ms.append(_place(B["SM_Prop_March_RitualCircle"](pal), x, y, 0))
            ms.append(_place(B["SM_Prop_March_RitualCircle_Ash"](pal), x, y, 0))
        for k in range(12):
            a = math.tau * k / 12 + 0.2
            x, y = math.cos(a) * 9.0, math.sin(a) * 9.0
            n = "SM_Prop_March_RingBanner_A" if k % 3 else "SM_Prop_March_RingBanner_B"
            ms.append(_place(B[n](pal), x, y, _face(x, y, 0, 0)))
        for k, (r, t) in enumerate(((-5.6, 0.6), (-6.5, -1.3), (-4.9, 2.3))):     # the Dead Ranks approach (west)
            n = "SM_Prop_March_HelmetStake_A" if k % 2 else "SM_Prop_March_HelmetStake_B"
            ms.append(_place(B[n](pal), *_sp(r, t), -135 + 25 * (k - 1)))
        if (exp_ / "Monsters" / "SK_Mon_Valdras.fbx").exists():
            _, mm = import_asset("SK_Mon_Valdras", at=(0.3, 0.2, 0.07), yaw_deg=-135, clip="Idle")
            ms += mm
        return ms, (0.6, -0.8, 0.0), (1920, 1080), (*_sp(3.6, 1.0), -45)
    if tag == "sheepfold":
        ms = []
        for k in range(10):
            a = math.tau * k / 10 + math.pi / 10
            if abs(math.sin(a)) > 0.93:
                continue                                    # the gaps north and south
            x, y = math.cos(a) * 6.0, math.sin(a) * 6.0
            n = "SM_Prop_March_SheepfoldWagon_A" if k % 2 else "SM_Prop_March_SheepfoldWagon_B"
            ms.append(_place(B[n](pal), x, y, _face(x, y, 0, 0)))
        if (exp_ / "Monsters" / "SK_Mon_Corpseherd.fbx").exists():
            _, mm = import_asset("SK_Mon_Corpseherd", at=(0.0, 0.0, 0.0), yaw_deg=-135, clip="Idle")
            ms += mm
        return ms, (0.0, -0.5, 0.0), (1920, 1080), (*_sp(2.4, 1.6), -45)
    if tag == "knellwatch_gate":
        # tile-aligned like the runtime places it (the burial road runs along a grid axis): yaw 180, its front (+Y)
        # a 3/4 view toward the camera; the camp wall continues the gate's line either side
        ms = [_place(B["SM_Prop_March_BoneyardGate"](pal), 0, 0, 180)]
        # the camp wall either side, laid like the runtime's BuildWalls (a stake every 0.2 m along a wall line —
        # tile centre ± 0.2 / 0.4 toward each wall neighbour — random yaw, the variants mixed)
        for side in (-1, 1):
            for k in range(18):
                nm = f"SM_Palisade_March_Pew_{'ABC'[(k * 7 + (side > 0)) % 3]}"
                if (exp_ / "Props" / f"{nm}.fbx").exists():
                    _, mm = import_asset(nm, at=(side * (2.35 + 0.2 * k), 0.0, 0),
                                         yaw_deg=(k * 137 + 61 * (side > 0)) % 360)
                    ms += mm
        for x in (-1.9, 2.1):                                # helmets on the road outside
            ms.append(_place(B["SM_Prop_March_HelmetStake_A"](pal), x, 2.3, 180))
        return ms, (0.0, 0.7, 0.0), (1920, 1080), (0.6, 2.4, -45)
    if tag == "knellwatch_camp":
        ms = [_place(B["SM_Prop_March_NameWall"](pal), *_sp(0.0, -1.6), 180),      # on the grid-aligned palisade
              _place(B["SM_Prop_March_PitMouthSealed"](pal), *_sp(4.6, 1.2), -135)]
        for nm, (r, t), yaw in (("SM_Prop_March_Hearth", (0.6, 1.4), -135), ("SM_Prop_March_BellFrame", (-4.4, -0.6), -135),
                                ("SM_Prop_March_AnvilStump", (-2.2, 1.6), -100)):
            if (exp_ / "Props" / f"{nm}.fbx").exists():
                _, mm = import_asset(nm, at=(*_sp(r, t), 0), yaw_deg=yaw)
                ms += mm
        return ms, (0.0, 0.0, 0.0), (1920, 1080), (*_sp(-1.0, 2.6), -45)
    if tag == "east_road":
        ms = [_place(B["SM_Prop_March_WagonBarricade"](pal), 0, 0, 180),            # across the road: tile-aligned
              _place(B["SM_Prop_March_OrderPost"](pal), *_sp(-3.6, 1.6), 180)]
        return ms, (*_sp(-0.6, 0.8), 0.0), (1280, 720), (*_sp(0.8, 2.4), -45)
    raise KeyError(tag)


SCENES = ("ashwheat", "breach_rim", "sheepfold", "knellwatch_gate", "knellwatch_camp", "east_road")


def extras(flags: set, opts: dict) -> int:
    out_dir = paths.preview_root() / PREV_DIR
    out_dir.mkdir(parents=True, exist_ok=True)
    if "--family" in flags:
        scene.reset()
        pal = Palette(FAM)
        B = _builders()
        objs = [B[n](pal) for n in B]
        pal.save()
        groups = (("setpieces_ashwheat.png", "ASHWHEAT: COTTAGES, THE BURNED BRIDGE", ("Cottage", "Bridge"), 2),
                  ("setpieces_breach_rim.png", "THE BREACH RIM: DAIS, ASH CIRCLE", ("Dais", "Ritual"), 3),
                  ("setpieces_wagons.png", "THE SHEEPFOLD WAGONS, THE EAST-ROAD BARRICADE", ("Wagon",), 3),
                  ("setpieces_knellwatch.png", "KNELLWATCH: BONEYARD GATE, NAME WALL, PIT MOUTH",
                   ("Gate", "NameWall", "PitMouth"), 3),
                  ("setpieces_small.png", "BANNER RING, HELMET STAKES, ORDER POST",
                   ("RingBanner", "HelmetStake", "OrderPost"), 5))
        for fname, label, keys, cols in groups:
            sel_ = [o for o in objs if any(k in o.name for k in keys)]
            for o in objs:
                o.hide_render = o not in sel_
            EM.family_sheet(sel_, fname, label, cols=cols)
        for o in objs:
            o.hide_render = False
    if "--scenes" in flags:
        tags = [t for t in SCENES if not opts.get("scene") or t in opts["scene"].split(",")]
        for t in tags:
            scene.reset()
            pal = Palette(FAM)
            ms, focus, size, w = build_scene(t, pal)
            pal.save()
            EM.scene_shot(ms, focus, out_dir / f"scene_{t}.png", f"{t.upper()}  W1 CAMERA  1080P 1:1", size_px=size,
                          warrior=w)
    if "--echo" in flags:
        echo_previews(out_dir, opts)
    return 0


# ── the echo material (lore spriteType ``echo``: U1 draws a translucent figure frozen in its last pose, 3 s) ──
ECHO_TINT = "#A8B0C4"              # Props_March echo.body: cold pale (echoes are victims: never violet, never ember)
ECHO_RIM = "#DCE4F2"               # echo.rim (the edge term brightens toward it)
ECHO_TINT_SCALE = 0.5              # additive strength of the tint (M_AF_Ghost CPD Tint = linear(echo.body) × this)
ECHO_FIGURES = {
    "lore_rf_echo_01": dict(title="第七个人 / The Seventh Man", at=(29, 15), set_piece=None,
                            figures=[dict(asset="SK_NPC_EscortTravelingMerchant_March", clip="Death", frame=22,
                                          xy=(0.0, 0.0), yaw=-150.0)],
                            props=[],
                            pose="kneeling by the trench (the escort rig's Death clip at its kneel, frame 22 of 54); "
                                 "the dropped lantern at (29,15) is the real prop beside him, not part of the echo"),
    "lore_rf_echo_02": dict(title="桥那边 / The Far Bank", at=(38, 47), set_piece="SM_Prop_March_BurnedBridge",
                            figures=[dict(asset="SK_NPC_RescueLostTraveler_March", clip="Cower", frame=0,
                                          xy=(-2.6, -0.55), yaw=95.0),
                                     dict(asset="SK_NPC_EscortTravelingMerchant_March", clip="Death", frame=22,
                                          xy=(-2.75, 0.35), yaw=80.0)],
                            props=[],
                            pose="two adults waiting at the near bridgehead, turned to the far bank (Cower frame 0 "
                                 "and the Death kneel); no children on screen (story-bible §11)"),
    "lore_rf_echo_03": dict(title="石台边 / By the Dais", at=(20, 92), set_piece="SM_Prop_March_BreachDais",
                            # art review r1: at (-0.4, 2.75) the figure straddled the kerb (the ghost a pale cloud cut
                            # by the kerb slabs) and Cower 30 held its head in its hands. Now on open ground outside
                            # the kerb where it is broken at the breach (west), facing the dais, its back to the W1
                            # camera; the escort rig's Death kneel (same SKEL_Human) with the arms laid behind
                            figures=[dict(asset="SK_NPC_RescueLostTraveler_March", clip="Death", frame=22,
                                          clip_from="SK_NPC_EscortTravelingMerchant_March", arms_behind=True,
                                          xy=(-3.05, 0.27), yaw=66.0)],
                            props=[],
                            pose="a deserter kneeling on open ground at the west breach of the dais kerb, facing the "
                                 "seal, back to the camera, hands bound behind (the Bound Pilgrim mesh; the escort "
                                 "rig's Death clip at its kneel, frame 22, played on the shared SKEL_Human). The arms "
                                 "behind the back are a preview stand-in posed on the rig: the dedicated Kneel_Bound "
                                 "clip (kneel, hands bound behind, head bowed) is the A9 follow-up U1 switches to"),
}


def echo_contract() -> dict:
    lin = [round(float(v), 4) for v in review._srgb_to_lin(np.array(
        [int(ECHO_TINT[i:i + 2], 16) / 255.0 for i in (1, 3, 5)]))]
    return {
        "kind": "MaterialInstance",
        "parent": "M_AF_Ghost",
        "use": "lore objects of spriteType echo (GameData loreCollectibles lore_rf_echo_01..03; U1 'the echo lore "
               "look'): every material slot of the figure (toon and ink) draws with this instance; additive, unlit, "
               "no shadow, never lights anything",
        "params": {
            "Tint": {"cpd": [9, 10, 11], "hex": ECHO_TINT, "scale": ECHO_TINT_SCALE,
                     "linear": [round(v * ECHO_TINT_SCALE, 4) for v in lin],
                     "note": "Props_March echo.body; the M_AF_Ghost edge term (0.85 + 0.3 edge) lifts the silhouette "
                             "toward echo.rim " + ECHO_RIM},
            "Ghost": {"cpd": 14, "value": 1.0},
            "Fade": {"cpd": 13, "timeline": "1 → 0 over fadeInMs (ease out), 0 + flicker for holdMs, 0 → 1 over "
                                            "fadeOutMs (ease in)"},
        },
        "timeline": {"fadeInMs": 450, "holdMs": 2100, "fadeOutMs": 450, "totalMs": 3000,
                     "flicker": {"hz": 4.0, "amplitude": 0.08, "note": "added to Fade during the hold"}},
        "trigger": "the lore interaction (E / tap) on the echo's tile: the figure fades in at the lore spot, holds "
                   "while the line shows, fades out; the lore prop itself stays invisible (no mesh) except a faint "
                   "ground mark (the prop.lore light pool decal in echo.body)",
        "vfx": "A12 recipe for the echo ghost (cold ash motes rising off the figure, echo.rim) at the figure's fx_chest",
        "rules": "cold pale only: no breach violet (echoes are the breach's victims, not its servants), no ember; the "
                 "ink slot reads the vertex Body mask 0 → 0.55 of the tint: a soft cold rim, never a dark line",
        "figures": {k: {"title": v["title"], "tile": list(v["at"]), "nextTo": v["set_piece"],
                        "figures": [{kk: f[kk] for kk in ("asset", "clip", "frame")} | {"offsetM": list(f["xy"]),
                                                                                      "yawDeg": f["yaw"]}
                                    | ({"clipAsset": f["clip_from"]} if f.get("clip_from") else {})
                                    | ({"poseOverride": "arms behind the back, wrists crossed at the small of the back "
                                                        "(preview stand-in; A9 Kneel_Bound replaces it)",
                                        "followUp": "A9 Kneel_Bound"} if f.get("arms_behind") else {})
                                    for f in v["figures"]],
                        "props": [p_["asset"] for p_ in v["props"]], "pose": v["pose"]}
                    for k, v in ECHO_FIGURES.items()},
        "previews": [f"Previews/{PREV_DIR}/echo_{k}.png" for k in ECHO_FIGURES],
        "owner": "A10 (setpieces_march.py --echo); the figure poses are recommendations for U1",
    }


class MaterialsManifest(E.EnvManifest):
    """The kit manifest writer merging the top-level ``materials`` section per key as well (art-side material
    contracts the content build / UE layer implement, e.g. ``MI_AF_Echo``)."""

    MERGED = ("assets", "palettes", "gameIds", "terrain", "materials")


def ghost_material() -> "bpy.types.Material":
    """The M_AF_Ghost maths in Cycles (additive emission over black): k = Ghost (1 - Fade)(0.85 + 0.3 edge)
    lerp(0.55, 1, Body), Body = AF_Data.a; the inverted hull (Body 0) counts only on its back faces."""
    from kit import shading
    m = bpy.data.materials.new("AF_Preview_Echo")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    b = shading.NB(nt)
    out = b.node("ShaderNodeOutputMaterial")
    em = b.node("ShaderNodeEmission")
    geo = b.node("ShaderNodeNewGeometry")
    attr = b.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name="AF_Data")
    dot = b.node("ShaderNodeVectorMath", operation="DOT_PRODUCT")
    b.l.new(geo.outputs["Normal"], dot.inputs[0])
    b.l.new(geo.outputs["Incoming"], dot.inputs[1])
    absn = b.node("ShaderNodeMath", operation="ABSOLUTE")
    b.l.new(dot.outputs["Value"], absn.inputs[0])
    inv = b.node("ShaderNodeMath", operation="SUBTRACT")
    inv.inputs[0].default_value = 1.0
    b.l.new(absn.outputs[0], inv.inputs[1])
    pw = b.node("ShaderNodeMath", operation="POWER")
    pw.inputs[1].default_value = 1.5
    b.l.new(inv.outputs[0], pw.inputs[0])
    edge = b.node("ShaderNodeMath", operation="MULTIPLY_ADD")     # 0.3 edge + 0.85
    edge.inputs[1].default_value = 0.3
    edge.inputs[2].default_value = 0.85
    b.l.new(pw.outputs[0], edge.inputs[0])
    body = b.node("ShaderNodeMath", operation="MULTIPLY_ADD")     # 0.45 Body + 0.55
    body.inputs[1].default_value = 0.45
    body.inputs[2].default_value = 0.55
    b.l.new(attr.outputs["Alpha"], body.inputs[0])
    # visibility: body faces when front-facing, hull faces when back-facing
    bf = geo.outputs["Backfacing"]
    one_m_bf = b.node("ShaderNodeMath", operation="SUBTRACT")
    one_m_bf.inputs[0].default_value = 1.0
    b.l.new(bf, one_m_bf.inputs[1])
    a1 = b.node("ShaderNodeMath", operation="MULTIPLY")
    b.l.new(attr.outputs["Alpha"], a1.inputs[0])
    b.l.new(one_m_bf.outputs[0], a1.inputs[1])
    one_m_body = b.node("ShaderNodeMath", operation="SUBTRACT")
    one_m_body.inputs[0].default_value = 1.0
    b.l.new(attr.outputs["Alpha"], one_m_body.inputs[1])
    a2 = b.node("ShaderNodeMath", operation="MULTIPLY")
    b.l.new(one_m_body.outputs[0], a2.inputs[0])
    b.l.new(bf, a2.inputs[1])
    vis = b.node("ShaderNodeMath", operation="ADD")
    b.l.new(a1.outputs[0], vis.inputs[0])
    b.l.new(a2.outputs[0], vis.inputs[1])
    k1 = b.node("ShaderNodeMath", operation="MULTIPLY")
    b.l.new(edge.outputs[0], k1.inputs[0])
    b.l.new(body.outputs[0], k1.inputs[1])
    k2 = b.node("ShaderNodeMath", operation="MULTIPLY")
    b.l.new(k1.outputs[0], k2.inputs[0])
    b.l.new(vis.outputs[0], k2.inputs[1])
    lin = review._srgb_to_lin(np.array([int(ECHO_TINT[i:i + 2], 16) / 255.0 for i in (1, 3, 5)])) * ECHO_TINT_SCALE
    em.inputs["Color"].default_value = (float(lin[0]), float(lin[1]), float(lin[2]), 1.0)
    b.l.new(k2.outputs[0], em.inputs["Strength"])
    tr = b.node("ShaderNodeBsdfTransparent")
    mx = b.node("ShaderNodeMixShader")
    b.l.new(vis.outputs[0], mx.inputs["Fac"])
    b.l.new(tr.outputs[0], mx.inputs[1])
    b.l.new(em.outputs[0], mx.inputs[2])
    b.l.new(mx.outputs[0], out.inputs["Surface"])
    return m


def echo_previews(out_dir: Path, opts: dict) -> None:
    """``Previews/setpieces_march/echo_<lore id>.png``: each echo at the W1 camera (native 1080p) beside its set
    piece — the scene, the same figure as the solid shipped NPC (what U1 poses), and the shipped look (the figure
    through the M_AF_Ghost maths added in linear light). Writes ``materials.MI_AF_Echo`` into the manifest."""
    for key, spec in ECHO_FIGURES.items():
        if opts.get("lore") and key not in opts["lore"].split(","):
            continue                                         # --lore=lore_rf_echo_03: just that one
        scene.reset()
        pal = Palette(FAM)
        env = []
        if spec["set_piece"]:
            env.append(_builders()[spec["set_piece"]](pal))
        pal.save()
        figs = []
        for f in spec["figures"]:
            root, mm = import_asset(f["asset"], at=(f["xy"][0], f["xy"][1], 0.0), yaw_deg=f["yaw"], clip=f["clip"],
                                    frame=f["frame"], hide_attach=True, clip_from=f.get("clip_from"))
            if f.get("arms_behind"):
                pose_arms_behind(root)
            figs += mm
        for p_ in spec["props"]:
            root, mm = import_asset(p_["asset"], at=(p_["xy"][0], p_["xy"][1], 0.06), yaw_deg=p_["yaw"])
            root.rotation_euler = (math.radians(p_.get("tilt", 0.0)), 0, math.radians(p_["yaw"]))
            figs += mm
        bpy.context.view_layer.update()
        review.stage(radius=40.0, blob_radius=0.01)
        review.setup_render((1920, 1080), 8)
        pts = review.mesh_points(figs)
        ctr = Vector(pts.mean(0))
        focus = ctr if not env else (ctr * 0.6 + Vector(review.mesh_points(env).mean(0)) * 0.4)
        cam = review.game_camera((focus.x, focus.y))
        for m in env + figs:
            if "af_outline_width" in m:
                cls = m.get("af_outline_class", "decor")
                px = outline.SCREEN_PX_1080.get(cls, 1.2) if cls not in ("decor", "interactive") else _px(m)
                review.set_outline_px([m], px)
        bx = list(E.projected_box(cam, env + figs))
        for i_, mn in ((0, 560), (1, 360)):                  # at least 560 × 360 px of the game frame
            span = bx[i_ + 2] - bx[i_]
            if span < mn:
                bx[i_] -= (mn - span) / 2
                bx[i_ + 2] += (mn - span) / 2
        mg = 30
        box = (max(0.0, (bx[0] - mg) / 1920), max(0.0, 1 - (bx[3] + mg) / 1080), min(1.0, (bx[2] + mg) / 1920),
               min(1.0, 1 - (bx[1] - mg) / 1080))
        # 1) the solid figure (what U1 poses); 2) the scene without it + the ghost pass added in linear light
        solid = review.render_array(box)
        for m in figs:
            m.hide_render = True
        base = review.render_array(box)
        em = review.emissive_pass(env, box) if env else None
        if em is not None:
            base = review.bloom(base, em, 1080)
            solid = review.bloom(solid, em, 1080)
        for m in figs:
            m.hide_render = False
        gm = ghost_material()
        black = review._flat_emission("AF_Echo_Occluder", (0, 0, 0))
        black_hull = review._flat_emission("AF_Echo_Occluder_Hull", (0, 0, 0), hull=True)
        saved = {}
        for m in env:
            saved[m.name] = [m.data.materials[i] for i in range(len(m.data.materials))]
            for i in range(len(m.data.materials)):
                m.data.materials[i] = black_hull if i == 1 else black
        fsaved = {}
        for m in figs:
            fsaved[m.name] = [m.data.materials[i] for i in range(len(m.data.materials))]
            for i in range(len(m.data.materials)):
                m.data.materials[i] = gm
            review.set_outline_px([m], outline.SCREEN_PX_1080.get("npc", 3.0))
        review.hide_stage(True)
        with review._World((0.0, 0.0, 0.0)):
            ghost = review.render_array(box, exr=True)
        review.hide_stage(False)
        for m in env:
            for i, mat in enumerate(saved[m.name]):
                m.data.materials[i] = mat
        for m in figs:
            for i, mat in enumerate(fsaved[m.name]):
                m.data.materials[i] = mat
        lin = review._srgb_to_lin(base.astype(np.float32) / 255.0) + np.maximum(ghost[:, :, :3], 0.0)
        shipped = (review._lin_to_srgb(lin) * 255.0 + 0.5).astype(np.uint8)
        review.draw_text(solid, 4, 4, f"{key}  THE FIGURE U1 POSES (SOLID)")
        review.draw_text(shipped, 4, 4, f"{key}  ECHO: M_AF_GHOST  TINT {ECHO_TINT} X{ECHO_TINT_SCALE}")
        review.draw_text(shipped, 4, 14, spec["title"].split(" / ")[-1].upper() + "  1080P 1:1 W1", rgb=(200, 210, 230))
        img = review.grid([solid, shipped], 2, pad=2)
        pngio.write_png_native(out_dir / f"echo_{key}.png", img)
        print(f"  {out_dir / f'echo_{key}.png'}")
    man = MaterialsManifest()
    man.data.setdefault("materials", {})["MI_AF_Echo"] = echo_contract()
    man.save()


def main(argv: list[str]) -> int:
    flags = {a.split("=")[0] for a in argv if a.startswith("--")}
    opts = dict(a[2:].split("=", 1) for a in argv if a.startswith("--") and "=" in a)
    names = [a for a in argv if not a.startswith("--")]
    builders = _builders()
    sel = [n for n in builders if not names or any(a == n or a in n for a in names)]
    if "--quick" in flags:
        scene.reset()
        pal = Palette(FAM)
        objs = [builders[n](pal) for n in sel]
        pal.save()
        iter_sheet(objs, "quick_" + "_".join(n.split("_", 3)[-1][:10] for n in sel)[:90] + "_" +
                   opts.get("facing", "front"), facing=opts.get("facing", "front"))
        return 0
    if "--family" in flags or "--scenes" in flags or "--echo" in flags:
        return extras(flags, opts)
    previews = "--no-previews" not in flags
    verify = "--no-verify" not in flags
    results = {}
    for n in sel:
        results[n] = ship_one(n, previews=previews, verify=verify)
    report = write_report(results)
    rc = E.summary()
    bad = [n for n, r in results.items() if r.get("verify") and not r["verify"]["pass"]]
    for n in bad:
        print(f"  VERIFY FAIL {n}: " + "; ".join(results[n]["verify"]["failures"]))
    pad_bad = [n for n, r in results.items() if not r["pad"]["pass"]]
    for n in pad_bad:
        print(f"  PAD FAIL {n}: " + "; ".join(results[n]["pad"]["over"]))
    bad += pad_bad
    nrm_bad = [n for n, r in results.items() if not r["normals"]["pass"]]
    for n in nrm_bad:
        print(f"  NORMALS FAIL {n}: " + "; ".join(f"{k} {v['badFrac'] * 100:.1f} %" for k, v in
                                                 results[n]["normals"].items() if k != "pass"))
    bad += nrm_bad
    if previews and "--no-family" not in flags and len(sel) == len(builders):
        extras({"--family"}, opts)
    print(f"  report: {report}")
    return 1 if (rc or bad) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
