"""The Pit crypt kit — 信者之坑 / the Pit of the Faithful (dark remake Wave 3, package A13 / work package 3C;
chapter1.md §9.2, S7 ``q_rf_the_pit``, ``cs_rf_pit``; implementation-plan.md §5 A13, §4 U6).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/crypt_pit.py [names…]
        [--quick] [--family] [--no-previews] [--no-verify]

The sub-dungeon under the burn pit: a hand-authored 40 × 40 tile map (``sub_rf_pit``), the darkest place of the
chapter — the hero's light radius is the main light. Every asset ships through the environment pipeline (``envkit.ship``
on the march layer: FBX + LOD1, manifest entry with game ids / footprint / blocking / sockets, the review set, the ink
gate, the pad gate, normal sanity, the reserved-light check and a re-import of both FBX files).

**Tiling.** A tile is 1 m. Wall tiles (tile type 4) take ``SM_Pit_Wall_A/B/C``: a 1 × 1 × 2.0 m block of coursed
rubble, dressed on all four faces, a black earth top — neighbours abut face to face into one wall, any multiple of
90° of yaw tiles (the runtime's hashed yaw must be **snapped to 90°** for these: manifest ``placement.yawStepDeg``).
Everything that dresses a wall is an **overlay**: it stands on the wall tile's own centre, its front (−Y) on the face
toward the floor, and encloses or sits proud of the plain block, so the UE lane places it without removing the wall
mesh under it (the pier is 1.12 m square and 2.3 m tall; niches, lamps and tag curtains project 12-34 cm from the
face, above knee height).

| Asset | game ids | Room (chapter1.md §9.2) | Look |
|---|---|---|---|
| ``SM_Pit_Wall_A/B/C`` | ``wall_pit``, ``outcrop_plains@pit``, ``setpiece_pit_wall`` | every wall tile | coursed rubble in three cold tones, painted joints, a plinth course, a black earth top; B: a fallen stone and its black socket; C: ash banked at the foot, soot streaks |
| ``SM_Pit_WallPier`` | ``setpiece_pit_wall_pier`` | corners, wall ends, every 4-6 tiles of a long wall | a square pier of big dressed blocks on a plinth under a solid weathered capstone: the kit's "corner" |
| ``SM_Pit_HelmNiche_A/B`` | ``setpiece_pit_helm_niche`` | the Helm Wall (24,8) | loculi with a kettle helm standing in each **crown up, tipped back against the niche** (its face up and out at the roll call: the same dome and brim the Risen in front of it wear), a little ash on the sill, a bronze tag under it; A one niche, B two tiers |
| ``SM_Pit_NicheLamp`` / ``SM_Pit_NicheLamp_Lit`` | ``setpiece_pit_lamp`` / ``setpiece_pit_lamp_lit`` | every room; the cleanse lights them one by one | a small loculus with an iron cresset: cold, sooty, a dead wick / a flame (``hearth.ember`` round ``lantern.glow``), the back of the niche washed warm; sockets ``flame``, ``light`` |
| ``SM_Pit_TagStrand_A/B/C`` | ``setpiece_pit_tag_strand`` | the Tag Cellar (10,24) | one cord (C: a bunch of three) on an iron peg, 10-16 bronze name-tags knotted down it: the instanced strand (no hull: a 1 cm cord would draw as a 3 px bar) |
| ``SM_Pit_TagCurtain`` | ``setpiece_pit_tag_curtain`` | the Tag Cellar's walls | a peg rail and 13 strands, ~150 tags to the wall tile: the thousands |
| ``SM_Pit_TagRack`` | ``setpiece_pit_tag_rack`` | the Tag Cellar's floor (blocking, 2 × 1) | a charred timber rack, 16 strands hanging from its beam |
| ``SM_Pit_AshSteps`` | ``setpiece_pit_ash_steps``, ``exit_portal@pit`` | the Ash Steps (5,5): the entry and the exit | a steep stair rising into a black passage in the wall, choked with ash, a fallen beam, a helm on the second step, a charred frame; sockets ``exit``, ``fx_ring`` (the exit slot's), ``fx_vortex``, ``top`` |
| ``SM_Pit_FlameNiche`` | ``setpiece_pit_flame_niche`` | the Flame Niche (20,35), the S7 turn-in beat | a battered slab of the palest stone in the Pit, an **empty niche in the shape of a flame**, five burn marks round it (the hero's brand: five flames round an empty heart); sockets ``niche``, ``turn_in``, ``mark_1..5`` |
| ``SM_Pit_AshDrift_A/B`` | ``setpiece_pit_ash_drift`` | the Ash Steps, anywhere | drifted ash on the flags, one soft un-inked skin over several lenses in three values (B: rubble and a half-buried helm): ground dressing |
| ``SM_Prop_March_PitMouthOpen`` | ``setpiece_pit_mouth_open`` | the surface entrance (17,26) once W3 unseals it | the burn pit's mouth open: the grate thrown back, the planks heaped aside, a ladder down into the black, two dim smears of breach seep deep on the shaft's far wall; no warm light; sockets ``smoke``, ``entrance``, ``ladder_top`` |

Colour (``Props_March``; the crypt regions are appended, :data:`CRYPT`): cold dark stone below the monsters' lit
values, a black top so the rooms read as cut out of the earth, ash a step over the floor's own (``crypt.drift``),
bronze tags and dull helm iron as the accents. The crypt statics ship on their own toon instance without the kit's
Fresnel rim and with 30 % of the toon's cool shade tint (:data:`CRYPT_SLOT`: a wall turned from the key light is dark
grey, not indigo); the stonework's relief is painted (``masonry``); the look in game — the darkness and
the hero's light radius — is a grade (``vfx_recipes.json`` ``dark.pit``; ``environment/vignette_pit.py`` previews it). Reserved light (chapter1.md §15): the only warm emissive is the **lit** lamp
(``lantern.*`` / ``hearth.ember``: the cleansed Pit is a refuge); breach violet only as the seep in the open mouth.
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
import setpieces_march as SP  # noqa: E402
import bpy  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import paths, scene, theme  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"
FAM = EM.PROPS
FAMILY = "environment.crypt_pit"
THEME = "pit"
SCRATCH = paths.scratch_root("a13pit")
TILE, HALF, WALL_H = 1.0, 0.5, 2.0
V = SP.V
_rx, _ry, _rz = SP._rx, SP._ry, SP._rz


def reg(name: str) -> Region:
    return EM.region(FAM, name)


# ── regions ─────────────────────────────────────────────────────────────────────────────────────────────
# The crypt's own swatches (appended to Props_March under ``crypt.<key>``). Values: the wall faces lit L* 30-36
# (below the monsters' lit sides, L* 50-65, and the hero), the top near black, the shrine the palest stone.
CRYPT = dict(
    # (shade amounts: tuned with CRYPT_SHADE_COOL below — with 30 % of the cool tint a shade band keeps the stone's
    # own hue, and .30-.34 puts its value where the tinted .18-.24 had it: a face turned from the key light is a
    # step under the lit one and well over the black top)
    wall=Region("#4A4642", .30, .16),            # coursed rubble, cold dark stone
    wall_dark=Region("#36332F", .30, .12),       # older, recessed stones
    wall_lit=Region("#5A554F", .32, .16),        # a few paler stones, sills
    joint=Region("#1C1A19", .30, .08),           # joints between the stones
    plinth=Region("#3C3935", .32, .14),
    top=Region("#131110", .30, .06),             # the mass's top: black earth (the rooms are cut out of it)
    cap=Region("#4E4A45", .34, .10),             # pier capstones, lintels
    shrine=Region("#666159", .14, .14),          # the Flame Niche's dressed stone (the palest stone in the Pit; a
                                                 # shallow shade band: turned from the key light the slab is still
                                                 # as pale as a lit wall — art review r1: it went dark indigo)
    shrine_edge=Region("#8A847A", .12, .14),     # its rim round the empty niche
    shrine_joint=Region("#35312E", .30, .10),
    cord=Region("#4A4238", .48, .14),            # tag strings
    oil=Region("#16130F", .30, .30),             # the cresset's cold oil (a glossy light band)
    wick=Region("#0E0C0A", .30, .06),
    step=Region("#48443F", .32, .16),            # the stair's treads
    step_worn=Region("#625D55", .30, .16),       # their worn front edges
    earth=Region("#221E1B", .40, .10),           # the dark behind the open mouth's shaft
    shaft=Region("#14110F", .30, .06),           # the open mouth's throat: the shaft going down (near black)
    seep_dim=Region("#3E2050", 0.0, 0.0, .22),   # the breach seep deep in the open mouth's shaft: a dim smear (the
                                                 # sealed mouth's pit.seep is #6E2A86 at glow .35: in an open
                                                 # black hole five such ovals read as gems)
    seep_halo=Region("#22152B", 0.0, 0.0, .10),  # its soft edge: a step over the shaft's dark
    drift=Region("#57534D", .36, .14),           # ash lying in the Pit: a step over the floor's ash (#3A3733 /
                                                 # lip #4A4640); the march's pale ash (#8A8480) lay on the dark
                                                 # flags like cut paper. A drift's crowns
    drift_low=Region("#4B4742", .34, .10),       # a drift's body: a value under its crowns
    drift_edge=Region("#3E3B37", .30, .06),      # a drift's feathered rim: a step over the flags it lies on
    drift_pale=Region("#69645D", .36, .14),      # banked against a wall, on the stair's treads
)
# A1 / A10 regions the kit reuses (the same swatches: identical values share an index)
_SHARED = ("pit_stone", "pit_dark", "pit_seep", "ash", "ash_pale", "ash_poured", "soot", "helm_iron", "helmet",
           "iron", "iron_dark", "tag", "tag_dark", "rope", "timber", "timber_end", "charred", "char_black", "stone",
           "coals", "chain", "nail", "ground_dark", "shield")
REGIONS = {k: SP.ALL_REGIONS[k] for k in _SHARED}
REGIONS.update(CRYPT)
REGIONS.update(lamp_flame=reg("hearth.ember"), lamp_core=reg("lantern.glow"), lamp_frame=reg("lantern.frame"))
# the lit lamp's wash on the back of its niche: a refuge light (the name carries the allowed ``lantern.`` prefix,
# march_palettes.WARM_ALLOWED / envkit_march.check_light)
LAMP_WASH = ("lantern.wash", Region("#7A4A26", .20, .20, .45))

# The crypt's toon instance: the palette toon with the rim switched off and the shade's cool tint turned down.
# * The kit's banded Fresnel rim (#FFECC8 at .55 on lit faces seen at a grazing angle) is a crisp stroke on a rounded
#   figure, but a wall is one big plane: at the W1 camera every key-lit wall face in the left third of the frame
#   crosses the band at once and turned cream (L* 70 in the darkest place of the chapter).
# * The toon's shade is the base mixed 35 % toward #1E143C: on a figure a cool turn of form, on a wall a whole plane
#   — every face turned from the key light was violet-indigo (kit values 42,37,47 against a 59,56,53 lit wall; hue
#   250 deg, sat .39 under the Pit's grade), a room of one purple wall and one grey one in the place where violet is
#   enemy magic only (art review r1, 3C). ``ShadeCool`` .3 keeps 30 % of the tint: the unlit side falls to dark grey.
# One instance for every crypt static (the content build rejects an instance with two parameter sets); the tag
# strands keep the march cloth instance.
CRYPT_SLOT = "MI_AF_Toon_Props_March_Crypt"
CRYPT_RIM_HEX, CRYPT_RIM_ALPHA = "#4A4642", 0.02       # the wall's own colour at 2 %: none. (Not 0: an unset scalar
                                                       # reads back 0 in the content build's instance test, so a 0
                                                       # override is never written there — Scripts test_editor_compat)
CRYPT_SHADE_COOL = 0.3                                 # M_AF_Toon ShadeCool (default 1)
CRYPT_PARAMS = {"RimOverride": 1.0, "RimOverrideColor": [round(int(CRYPT_RIM_HEX[i:i + 2], 16) / 255.0, 4)
                                                         for i in (1, 3, 5)],
                "RimOverrideAlpha": CRYPT_RIM_ALPHA, "ShadeCool": CRYPT_SHADE_COOL}


def crypt_toon(obj) -> None:
    """Slot 0 of ``obj`` → the preview of :data:`CRYPT_SLOT` (the palette toon, rim overridden, shade cool .3)."""
    from kit import shading
    base = obj.data.materials[0]
    name = "AF_CryptToon_" + base.name
    m = bpy.data.materials.get(name)
    if m is None:
        m = base.copy()
        m.name = name
        grp = next(n for n in m.node_tree.nodes if n.type == "GROUP")
        grp.node_tree = shading.rim_override_group(CRYPT_RIM_HEX, CRYPT_RIM_ALPHA, CRYPT_SHADE_COOL)
        m["af_parent"] = "M_AF_Toon"
        m["af_params"] = json.dumps(CRYPT_PARAMS)
        m["af_toon_slot"] = True
    obj.data.materials[0] = m


def cord_mask(obj, parts=("cords", "nametags")):
    """callable(border) → bool mask of the cords' and tags' coverage, dilated 2 px: the ink gate leaves their edges
    out (``EnvReview.ink_ignore``, as the bow's string in ``items/weapons.py``). Cords and tags carry no hull by
    design — the frame that holds them (rail, posts, beams) is what the gate measures."""
    import numpy as np
    from kit import review
    names = json.loads(obj["af_parts"])
    ids = {i for i, n in enumerate(names) if n in parts}
    me = obj.data.copy()
    bm = bmesh.new()
    bm.from_mesh(me)
    lay = bm.faces.layers.int.get("af_part")
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f[lay] not in ids or f.material_index != 0], context="FACES")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(f"{obj.name}_cords_only", me)
    bpy.context.scene.collection.objects.link(o)
    o.hide_render = True

    def fn(border):
        o.matrix_world = obj.matrix_world.copy()
        if obj.hide_render:
            return None
        prev = {x.name: x.hide_render for x in bpy.data.objects if x.type == "MESH" and x.name != "AF_Stage"}
        for x in bpy.data.objects:
            if x.type == "MESH" and x.name != "AF_Stage":
                x.hide_render = x is not o
        try:
            body, _ = review.coverage_pass([o], border)
        finally:
            for n, h in prev.items():
                if n in bpy.data.objects:
                    bpy.data.objects[n].hide_render = h
        m = body > 0.2
        for _ in range(2):
            pm = np.pad(m, 1, constant_values=False)
            m = m | pm[:-2, 1:-1] | pm[2:, 1:-1] | pm[1:-1, :-2] | pm[1:-1, 2:]
        return m
    return fn


def crypt_slot(name: str) -> bool:
    """Whether ``name`` ships on :data:`CRYPT_SLOT` (every crypt static; not the wind meshes, not the surface)."""
    sp = SPECS[name]
    return not sp.get("wind") and not sp.get("theme")

LOD_KEEP = ("hole", "helm", "bowl", "seep", "flame", "mark", "cords", "nametags", "top", "back", "shaft", "wash",
            "niche", "rim", "smoulder_dot", "step", "passage", "joint_h", "joint_p", "drift", "spill")
LOD_SKIP = SP.LOD_SKIP + ("stonelet", "peg_pin", "crack")


def lod_keep(name: str) -> bool:
    return any(name == s or name.startswith(s) for s in LOD_KEEP)


def lod_skip(name: str) -> bool:
    return any(name.startswith(s) for s in LOD_SKIP)


def new(pal: Palette, name: str, lod: int = 0):
    """A Builder on the crypt regions (prefix ``crypt``); LOD1 = the set pieces' part-by-part rebuild."""
    SP.LOD = lod
    if lod:
        b = SP.SPBuilder(name, pal, prefix="crypt", lod_ratio=0.5, lod_skip=lod_skip, keep=lod_keep)
    else:
        b = M.Builder(name, pal, prefix="crypt")
    b.regions(**REGIONS)
    b.swatch["wash"] = pal.add(*LAMP_WASH)
    SP.BUILT[(name, lod)] = b
    return b, scene.rng(name)


# ── geometry helpers ────────────────────────────────────────────────────────────────────────────────────
class Face:
    """One vertical face of a block: ``P(x, z, out)`` = the point ``x`` along the face (to the right, seen from
    outside), ``z`` up, ``out`` metres proud of it; the face stands ``dist`` from the block's axis, its outward
    normal is −Y turned ``yaw`` degrees about Z."""

    def __init__(self, yaw: float, dist: float = HALF, origin=(0.0, 0.0, 0.0)):
        self.m = Matrix.Translation(Vector(origin)) @ _rz(yaw)
        self.rot = _rz(yaw)
        self.dist = dist
        self.n = (self.rot.to_3x3() @ Vector((0.0, -1.0, 0.0))).normalized()

    def P(self, x: float, z: float, out: float = 0.0) -> Vector:
        return self.m @ Vector((x, -self.dist - out, z))

    def box(self, x0, x1, z0, z1, proud: float, sunk: float = 0.004, bevel: float = 0.0) -> M.Part:
        """A block standing ``proud`` of the face (its back ``sunk`` into it)."""
        d = proud + sunk
        p = SP.box((x1 - x0, d, z1 - z0), (0.5 * (x0 + x1), -self.dist - proud + d / 2, 0.5 * (z0 + z1)), bevel=bevel)
        return p.transform(self.m)

    def quad(self, x0, x1, z0, z1, lift: float = 0.002) -> M.Part:
        """A flat painted rectangle ``lift`` off the face."""
        bm = bmesh.new()
        vs = [bm.verts.new(tuple(self.P(x, z, lift))) for x, z in ((x0, z0), (x1, z0), (x1, z1), (x0, z1))]
        f = bm.faces.new(vs)
        f.normal_update()
        if f.normal.dot(self.n) < 0:
            f.normal_flip()
        bm.normal_update()
        return M.Part(bm).flat()

    def plate(self, outline, lift: float = 0.002, thick: float = 0.004) -> M.Part:
        """A flat plate with outline [(x, z)] (face coordinates) on the face."""
        p = M.plate(SP_ccw(outline), thick)
        # M.plate: outline in XY, extruded along +Z (0 .. thick) → face coords: X stays, Y → world Z, +Z → outward
        p.transform(Matrix(((1, 0, 0, 0), (0, 0, -1, -self.dist - lift), (0, 1, 0, 0), (0, 0, 0, 1))))
        return p.transform(self.m).flat()


def smear(pts, w: float, rnd: random.Random, steps: int = 4) -> list:
    """Outline [(x, y)] of a soft smear ``w`` wide along the polyline ``pts``: tapered to both ends, its edges
    waved (deterministic in ``rnd``)."""
    line = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        for k in range(steps):
            t = k / steps
            line.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t))
    line.append(pts[-1])
    n = len(line) - 1
    ph = rnd.uniform(0, math.tau)
    up, down = [], []
    for k, (x, y) in enumerate(line):
        xa, ya = line[max(k - 1, 0)]
        xb, yb = line[min(k + 1, n)]
        d = math.hypot(xb - xa, yb - ya) or 1.0
        nx, ny = -(yb - ya) / d, (xb - xa) / d
        t = k / n
        half = 0.5 * w * (math.sin(math.pi * t) ** 0.6) * (1.0 + 0.22 * math.sin(5.3 * t * math.pi + ph))
        half = max(half, 0.004)
        up.append((x + nx * half, y + ny * half))
        down.append((x - nx * half * 0.8, y - ny * half * 0.8))
    return up + down[::-1]


def SP_ccw(pts):
    """Counter-clockwise copy of a 2-D outline."""
    a = sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]))
    return list(pts) if a > 0 else list(reversed(pts))


RIM_Z = 0.003                      # an ash skin's rim lies on the floor (a rim under it is "buried" for the ink
                                   # gate: every edge of a flat drift was left out and the cell had none)


def _close_under(bm, rim: list, centre) -> None:
    """Close an ash skin under its rim with a fan from ``centre`` (the rim is star-shaped round it) and turn every
    normal outward. The underside is what the hull is seen by: an open skin looked at from above has only front
    faces, so its inverted hull drew no ink at all (ink gate: 0 % rim)."""
    cv = bm.verts.new(centre)
    n = len(rim)
    for k in range(n):
        bm.faces.new((cv, rim[(k + 1) % n], rim[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    top = max(bm.verts, key=lambda v: v.co.z)
    if sum(f.normal.z for f in top.link_faces) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)


def _lens(d2: float, h: float) -> float:
    return h * (1.0 - d2) ** 1.4 if d2 < 1.0 else 0.0


def ash_mound(b, c, r: float, h: float, rnd: random.Random, region: str = "drift", name: str = "heap",
              z: float = 0.0, **kw) -> int:
    """A small bank of ash: a low lens with a ragged rim (``setpieces_march.ash_heap``'s lumps read as inked
    boulders on a crypt floor — ash lies; it does not stand). Smooth, flat underneath, its rim on the ground
    (or a tread at ``z``), radius ``r`` along x and ~0.8 r along y, turned at random."""
    segs, rings = (8, 2) if SP.LOD else (12, 3)
    yaw = rnd.uniform(0, math.tau)
    ph = [rnd.uniform(0, math.tau) for _ in range(3)]
    ky = rnd.uniform(0.72, 0.9)
    bm = bmesh.new()
    apex = bm.verts.new((c.x, c.y, z + h))
    prev = None
    for j in range(1, rings + 1):
        t = j / rings
        ring = []
        for k in range(segs):
            a = math.tau * k / segs
            rag = 1.0 + (0.16 * math.sin(2 * a + ph[0]) + 0.10 * math.sin(3 * a + ph[1])
                         + 0.06 * math.sin(5 * a + ph[2])) * t
            x, y = math.cos(a) * r * t * rag, math.sin(a) * r * ky * t * rag
            zz = _lens(t * t, h) if j < rings else RIM_Z
            ring.append(bm.verts.new((c.x + x * math.cos(yaw) - y * math.sin(yaw),
                                      c.y + x * math.sin(yaw) + y * math.cos(yaw), z + zz)))
        for k in range(segs):
            k1 = (k + 1) % segs
            if prev is None:
                bm.faces.new((apex, ring[k], ring[k1]))
            else:
                bm.faces.new((prev[k], ring[k], ring[k1], prev[k1]))
        prev = ring
    _close_under(bm, prev, (c.x, c.y, z + RIM_Z))
    kw.setdefault("hull_pad", True)
    return b.add(M.Part(bm).auto_sharp(70), region, name=name, **kw)


def ash_drift(b, lenses, rnd: random.Random, region: str = "drift", name: str = "drift", z: float = 0.0) -> int:
    """A drift of ash as **one** skin over several overlapping lenses ``(cx, cy, r, h)``: soft swells inside one
    outline (separate heaps drew a stack of inked pancakes). The lenses must overlap round their weighted centre
    (the skin is a polar grid from it). The skin carries **no hull** and three values — a feathered rim a step over
    the flags (``drift_edge``), the body (``drift_low``), the crowns of the swells (``region``) — so it lies in the
    floor like ground cover (art review r1: one pale value inside a hull drew a heavy ink line along the far edge
    only, a cut-out paper blob on the dark flags). An asset that is only a drift ships with outline class
    ``none``; beside inked parts the ink gate leaves its edges out (``SPECS[...]["ink_ignore"]``)."""
    segs, rings = (12, 4) if SP.LOD else (26, 7)
    ph = [(rnd.uniform(0, math.tau), rnd.uniform(0.75, 0.95)) for _ in lenses]
    wsum = sum(r * r for _, _, r, _ in lenses)
    cx = sum(x * r * r for x, _, r, _ in lenses) / wsum
    cy = sum(y * r * r for _, y, r, _ in lenses) / wsum

    def height(x: float, y: float) -> float:
        acc = 0.0
        for (lx, ly, r, h), (a0, ky) in zip(lenses, ph):
            dx, dy = x - lx, y - ly
            u, v = dx * math.cos(a0) + dy * math.sin(a0), -dx * math.sin(a0) + dy * math.cos(a0)
            acc += _lens((u * u + (v / ky) ** 2) / (r * r), h) ** 2
        return acc ** 0.5                                   # a soft union: the lobes run into each other
    rmax = max(math.hypot(x - cx, y - cy) + r for x, y, r, _ in lenses)
    hmax = max(h for *_, h in lenses)
    bm = bmesh.new()
    sub = bm.faces.layers.int.new("af_sub")
    apex = bm.verts.new((cx, cy, z + max(height(cx, cy), 0.01)))
    grid = []
    for k in range(segs):
        a = math.tau * k / segs
        ca, sa = math.cos(a), math.sin(a)
        edge = 0.05
        t = 0.02
        while t < rmax:                                      # the last radius where the drift still stands
            if height(cx + ca * t, cy + sa * t) > 0.004:
                edge = t
            t += 0.02
        edge += 0.03
        col = []
        for j in range(1, rings + 1):
            t = edge * j / rings
            x, y = cx + ca * t, cy + sa * t
            col.append(bm.verts.new((x, y, z + (max(height(x, y), 0.014 * (1.0 - j / rings) + RIM_Z)
                                               if j < rings else RIM_Z))))
        grid.append(col)

    def tone(f, outer: bool) -> None:
        c = f.calc_center_median()
        f[sub] = 2 if outer else (0 if c.z - z > 0.45 * hmax else 1)
    for k in range(segs):
        k1 = (k + 1) % segs
        tone(bm.faces.new((apex, grid[k][0], grid[k1][0])), False)
        for j in range(rings - 1):
            tone(bm.faces.new((grid[k][j], grid[k][j + 1], grid[k1][j + 1], grid[k1][j])), j == rings - 2)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if sum(f.normal.z for f in bm.faces) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    return b.add(M.Part(bm).auto_sharp(70), region, name=name, outline=False,
                 sub_regions={1: "drift_low", 2: "drift_edge"})


STONE_LIFT = 0.0026                # a dressed stone is painted on its face (see masonry)


def masonry(b, rnd: random.Random, face: Face, cid: int, z0: float, z1: float, x0: float = -HALF, x1: float = HALF,
            course: float = 0.32, proud_p: float = 0.22, dark_p: float = 0.18, name: str = "") -> None:
    """Coursed rubble on a face between z0 and z1: painted joints, a few paler stones with a painted cast edge under
    and beside them (``wall_lit`` / ``wall``), a few dark ones (``wall_dark``) — value, not relief, carries the
    stonework at the W1 camera. (Stones modelled 1.5-3 cm proud padded the block's hull proxy: a 4.5 px baked rim on
    a 1.4 px class, the ink gate's baked bound R14.) LOD1 keeps the horizontal bed joints (``joint_h*``: one quad a
    course) and leaves the perpends and the stones off (``course_*`` / ``stonelet*``) — art review r1: LOD1 is the
    mobile-low mesh too, and without any joint the Pit's dominant surface was a flat untextured block there."""
    n = max(1, round((z1 - z0) / course))
    zs = [z0 + (z1 - z0) * k / n for k in range(n + 1)]
    w = x1 - x0
    for k in range(n):
        za, zb = zs[k], zs[k + 1]
        if k:
            b.add(face.quad(x0 + 0.004, x1 - 0.004, za - 0.009, za + 0.009), "joint", name="joint_h" + name,
                  outline=False, covered_by=cid)
        m = 2 if (k % 2) else 3
        cuts = [x0] + sorted(x0 + w * (j + (0.5 if k % 2 else 0.0) + rnd.uniform(-0.18, 0.18)) / (m + (0 if k % 2 else 0))
                             for j in range(1, m)) + [x1]
        cuts = [min(max(c, x0), x1) for c in cuts]
        for xa, xb in zip(cuts, cuts[1:]):
            if xb - xa < 0.08:
                continue
            if xa > x0 + 1e-4:
                b.add(face.quad(xa - 0.008, xa + 0.008, za + 0.006, zb - 0.006), "joint", name="course_v" + name,
                      outline=False, covered_by=cid)
            r = rnd.random()
            if r < proud_p:
                b.add(face.quad(xa + 0.020, xb - 0.020, za + 0.020, zb - 0.020, STONE_LIFT),
                      "wall_lit" if rnd.random() < 0.55 else "wall", name="stonelet" + name, outline=False,
                      covered_by=cid)
                # its cast edge: under it and on its right (the key light comes from the upper left)
                b.add(face.quad(xa + 0.026, xb - 0.010, za + 0.009, za + 0.020, STONE_LIFT - 0.0008), "joint",
                      name="course_s" + name, outline=False, covered_by=cid)
                b.add(face.quad(xb - 0.020, xb - 0.010, za + 0.009, zb - 0.026, STONE_LIFT - 0.0008), "joint",
                      name="course_s" + name, outline=False, covered_by=cid)
            elif r < proud_p + dark_p:
                b.add(face.quad(xa + 0.012, xb - 0.012, za + 0.012, zb - 0.012, 0.0015), "wall_dark",
                      name="stonelet_d" + name, outline=False, covered_by=cid)


def soot_stain(rnd: random.Random, x0: float, z_top: float, w: float, length: float, blunt: float = 0.3) -> list:
    """Outline [(x, z)] of a soot stain on a face: ``w`` wide under ``z_top``, narrowing raggedly over ``length``
    to a blunt, off-centre end (``blunt`` of its width: a stain fades out, a point is a crack)."""
    n = 4
    tip = x0 + w * rnd.uniform(0.35, 0.65)
    left, right = [(x0, z_top)], [(x0 + w, z_top)]
    for k in range(1, n + 1):
        t = k / n
        z = z_top - length * t * (rnd.uniform(0.92, 1.08) if k < n else 1.0)
        half = 0.5 * w * (blunt + (1.0 - blunt) * (1.0 - t) ** 0.9)
        c = x0 + w / 2 + (tip - x0 - w / 2) * t
        left.append((c - half * rnd.uniform(0.8, 1.15), z + rnd.uniform(-0.03, 0.03)))
        right.append((c + half * rnd.uniform(0.8, 1.15), z + rnd.uniform(-0.03, 0.03)))
    return left + right[::-1]


def top_plate(b, cid: int, size: float, z: float, name: str = "top") -> None:
    """The black earth top of a block (a plate 3 mm over it)."""
    bm = bmesh.new()
    h = size / 2
    f = bm.faces.new([bm.verts.new((x, y, z + 0.003)) for x, y in ((-h, -h), (h, -h), (h, h), (-h, h))])
    f.normal_update()
    if f.normal.z < 0:
        f.normal_flip()
    b.add(M.Part(bm).flat(), "top", name=name, outline=False, covered_by=cid)


# ── walls ───────────────────────────────────────────────────────────────────────────────────────────────
def build_wall(pal: Palette, variant: str = "A", lod: int = 0):
    """One wall tile: a 1 × 1 × 2.0 m block of coursed rubble on a plinth course, dressed on all four faces (each
    seeded differently: any 90° yaw tiles and no two neighbours repeat), a black earth top. B: one face has lost a
    stone (a black socket, the stone at its foot); C: ash banked against two faces, soot streaks down them."""
    name = f"SM_Pit_Wall_{variant}"
    b, rnd = new(pal, name, lod)
    core = SP.box((TILE, TILE, WALL_H), (0, 0, WALL_H / 2))
    cid = b.add(core, "wall", name="core", hull_pad=True)
    pl = SP.box((TILE + 0.05, TILE + 0.05, 0.24), (0, 0, 0.12))
    plid = b.add(pl, "plinth", name="plinth", hull_pad=True)
    top_plate(b, cid, TILE, WALL_H)
    for k in range(4):
        f = Face(90.0 * k)
        masonry(b, rnd, f, cid, 0.24, WALL_H - 0.02, name=f"_{k}")
        b.add(f.quad(-HALF + 0.004, HALF - 0.004, 0.232, 0.250, 0.027), "joint", name=f"joint_p_{k}", outline=False,
              covered_by=plid)
        if variant == "B" and k in (0, 2):
            xa = rnd.uniform(-0.30, 0.02)
            za = rnd.choice((0.56, 0.88, 1.20))
            b.add(f.quad(xa, xa + 0.30, za, za + 0.26, 0.004), "pit_dark", name=f"hole_{k}", outline=False,
                  covered_by=cid)
            SP.rubble(b, f.P(xa + 0.1, 0.0, 0.16), 0.11, rnd, region="wall", name="fallen")
            for dx in (-0.2, 0.22):
                SP.rubble(b, f.P(xa + 0.1 + dx, 0.0, 0.1), 0.05, rnd, region="wall_dark", name="fallen_s")
        if variant == "C" and k in (0, 1):
            for j in range(2):
                c = f.P(rnd.uniform(-0.3, 0.3), 0.0, 0.07)
                ash_mound(b, c, rnd.uniform(0.2, 0.3), rnd.uniform(0.07, 0.12), rnd, region="drift_pale",
                            name="bank")
            # one soot stain down the face from under the top: a broad haze a value under the stone where the
            # smoke stood, a darker heart in it, ragged, fading out at an uneven height (art review r1: two straight
            # 5-9 cm streaks to a face read as arrow slits and repeated along a wall)
            xs, w_, len_ = rnd.uniform(-0.40, 0.02), rnd.uniform(0.32, 0.44), rnd.uniform(0.55, 1.10)
            b.add(f.plate(soot_stain(rnd, xs, WALL_H - 0.03, w_, len_), 0.0026, 0.0015), "wall_dark",
                  name=f"soot_haze_{k}", outline=False, covered_by=cid)
            b.add(f.plate(soot_stain(rnd, xs + w_ * rnd.uniform(0.25, 0.4), WALL_H - 0.03, w_ * 0.34, len_ * 0.5,
                                     0.6), 0.0045, 0.0012), "soot", name=f"soot_{k}", outline=False, covered_by=cid)
    obj = E.finish(b, pal, "decor", ground_h=WALL_H, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = "[]"
    return obj


def build_pier(pal: Palette, lod: int = 0):
    """The pier (the kit's corner piece): 1.12 m square, 2.37 m tall, big dressed blocks on a stepped plinth, a
    solid weathered capstone (the one top in the Pit that is stone, not black earth: a pier is built, the wall mass
    is what was left). It stands on a wall tile and encloses the plain block."""
    name = "SM_Pit_WallPier"
    b, rnd = new(pal, name, lod)
    S, H = 1.12, 2.30
    core = SP.box((S, S, H - 0.16), (0, 0, (H - 0.16) / 2))
    cid = b.add(core, "wall", name="core", hull_pad=True)    # (plain: at yaw 45° the silhouette is the block's corners,
                                                             # and a chamfer under a plain hull box left a 3 px rim)
    b.add(SP.box((S + 0.10, S + 0.10, 0.20), (0, 0, 0.10)), "plinth", name="plinth", hull_pad=True)
    b.add(SP.box((S + 0.05, S + 0.05, 0.14), (0, 0, 0.27)), "plinth", name="plinth_2", hull_pad=True)
    cap = SP.box((S + 0.10, S + 0.10, 0.16), (0, 0, H - 0.08))
    kid = b.add(cap, "cap", name="cap", hull_pad=True)
    # the top closed in stone: a weathering on the capstone (a shallow four-sided chamfer up to a flat, all in the
    # cap's own stone: the flat catches the light band, so the top is the pier's palest plane — a darker flat inside
    # a paler chamfer still hinted at a recess) — art review r1: a capstone ring round a black earth top read from
    # the W1 pitch as a hollow shaft, a chimney
    bm = bmesh.new()
    a, c_, zt = (S + 0.10) / 2 - 0.02, (S + 0.10) / 2 - 0.22, H + 0.07
    lo = [bm.verts.new((sx * a, sy * a, H)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    hi = [bm.verts.new((sx * c_, sy * c_, zt)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for k in range(4):
        bm.faces.new((lo[k], lo[(k + 1) % 4], hi[(k + 1) % 4], hi[k]))
    bm.faces.new(hi)
    bm.faces.new(lo[::-1])                                   # closed underneath (inside the cap): a sound hull
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    b.add(M.Part(bm).flat(), "cap", name="top_weathering", hull_pad=True)
    del kid
    for k in range(4):
        f = Face(90.0 * k, S / 2)
        masonry(b, rnd, f, cid, 0.34, H - 0.16, -S / 2 + 0.03, S / 2 - 0.03, course=0.58, proud_p=0.12, dark_p=0.25,
                name=f"_{k}")
    obj = E.finish(b, pal, "decor", ground_h=H, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = "[]"
    return obj


# ── loculi: the helm niches and the lamps ───────────────────────────────────────────────────────────────
def loculus(b, rnd: random.Random, face: Face, z0: float, w: float, h: float, depth: float, tag: str = "",
            back: str = "pit_dark") -> dict:
    """A burial niche as an overlay on a wall face: a black back, two jambs, a lintel and a sill slab standing
    ``depth`` proud (what stands in it sits on the sill; its back half is in the dark). Returns the part ids and
    the sill's top centre."""
    bid = b.add(face.box(-w / 2 + 0.07, w / 2 - 0.07, z0 + 0.05, z0 + h - 0.07, 0.012), back, name="back" + tag,
                outline=False)
    sill = face.box(-w / 2, w / 2, z0 - 0.02, z0 + 0.06, depth, bevel=0.012)
    sid = b.add(sill, "wall_lit", name="sill" + tag, hull_pad=True,
                hull=None if SP.LOD else face.box(-w / 2, w / 2, z0 - 0.02, z0 + 0.06, depth))
    for sg in (-1, 1):
        xa, xb = sorted((sg * (w / 2 - 0.10), sg * w / 2))
        b.add(face.box(xa, xb, z0 + 0.06, z0 + h - 0.09, 0.115), "wall", name="jamb" + tag, hull_pad=True)
    lid = b.add(face.box(-w / 2 - 0.03, w / 2 + 0.03, z0 + h - 0.09, z0 + h + 0.02, 0.145, bevel=0.012), "cap",
                name="lintel" + tag, hull_pad=True,
                hull=None if SP.LOD else face.box(-w / 2 - 0.03, w / 2 + 0.03, z0 + h - 0.09, z0 + h + 0.02, 0.145))
    return {"back": bid, "sill": sid, "lintel": lid, "sill_top": face.P(0.0, z0 + 0.06, depth * 0.52)}


HELM_TILT = 40.0                   # degrees the niche helms are tipped back (crown toward the wall)


def helm_extent(r: float, rot: Matrix) -> tuple[float, float, float]:
    """(lowest z, furthest toward the wall +y, furthest toward the room -y) of ``SP.kettle_helm``'s outline of radius
    ``r`` turned by ``rot`` about its origin (the brim's three rings and the crown's)."""
    lo = back = front = 0.0
    m = rot.to_3x3()
    for rho, z in ((1.30, -0.36), (1.46, -0.24), (1.54, -0.10), (1.0, 0.30), (0.93, 0.68), (0.76, 1.02),
                   (0.5, 1.32), (0.0, 1.6)):
        for k in range(24):
            a = math.tau * k / 24
            q = m @ Vector((math.cos(a) * rho * r, math.sin(a) * rho * r, z * r))
            lo, back, front = min(lo, q.z), max(back, q.y), min(front, q.y)
    return lo, back, front


def niche_helm(b, face: Face, x: float, z_sill: float, depth: float, r: float, rnd: random.Random,
               name: str = "helm") -> Vector:
    """A crusade kettle helm standing in a loculus **crown up** (``setpieces_march.kettle_helm``: the same dome,
    comb and turned-down brim the Risen in front of the wall wear), tipped back :data:`HELM_TILT` degrees against
    the niche's back so its face looks up and out at the room — "every one facing up, as if waiting for roll call" —
    and the W1 camera sees the dome's outside over the brim's ellipse. (Art review r1, 3C: the first helms were set
    down upturned, crown down with ash in the bowl; at the game camera they read as iron basins on shelves, the same
    family as the unlit cresset in the lamp niche next to them.) Returns the point over the helm's crown."""
    rot = _rz(rnd.uniform(-14, 14)) @ _rx(-(HELM_TILT + rnd.uniform(-5, 5))) @ _ry(rnd.uniform(-6, 6))
    lo, back, front = helm_extent(r, rot)
    out = min(0.018 + back, depth - 0.012 + front)          # the crown against the back; the brim stays on the sill
    c = face.P(x, z_sill - lo + 0.002, out)
    # (face.P's "out" runs toward the room = the mesh's -y for the kit's front face: rot is in that frame)
    SP.kettle_helm(b, c, r, face.rot @ rot, region="helmet", name=name)
    return c + (face.rot @ rot).to_3x3() @ Vector((0.0, 0.0, 1.6 * r))


def name_tag(b, face: Face, x: float, z: float, out: float, pid: int, rnd: random.Random) -> None:
    """A bronze name-tag nailed to the stone (4.5 × 7 cm, hanging a little askew)."""
    t = SP.box((0.07, 0.006, 0.046), (0, 0, 0))
    t.transform(face.m @ Matrix.Translation((x, -face.dist - out - 0.002, z)) @ _ry(rnd.uniform(-9, 9)))
    b.add(t, "tag" if rnd.random() < 0.7 else "tag_dark", name="plate_tag", outline=False)
    del pid                                   # (no cover: a covered tag below a sill padded its proxy by 17 cm)


def build_helm_niche(pal: Palette, variant: str = "A", lod: int = 0):
    """The Helm Wall's niches (overlay on a wall tile, front −Y): A — one loculus at chest height, a kettle helm
    standing on its sill crown up, tipped back against the niche (:func:`niche_helm`); B — two tiers, a helm in each
    (the lower sill deeper: both show from the W1 camera). A bronze tag under every sill, a little ash drifted on
    the sill beside the helm (never in it: a pale disc in a dark ring is a bowl). Sockets ``helm_<n>`` over each
    crown."""
    name = f"SM_Pit_HelmNiche_{variant}"
    b, rnd = new(pal, name, lod)
    f = Face(0.0)
    tiers = [(0.80, 0.80, 0.60, 0.30)] if variant == "A" else [(0.38, 0.78, 0.56, 0.34), (1.14, 0.74, 0.54, 0.27)]
    socks = []
    for k, (z0, w, h, depth) in enumerate(tiers):
        L = loculus(b, rnd, f, z0, w, h, depth, tag=f"_{k}")
        r = 0.100 if variant == "A" else 0.094
        x = rnd.uniform(-0.05, 0.05)
        crown = niche_helm(b, f, x, z0 + 0.06, depth, r, rnd, name=f"helm_{k}")
        # ash on the sill, banked against the jamb the helm does not stand by
        sg = -1.0 if x > 0 else 1.0
        ash = M.lathe([(0.0, 0.016), (0.045, 0.012), (0.085, 0.0015)], sides=6 if lod else 9, cap0="fan", cap1=None)
        ash.transform(Matrix.Translation(f.P(sg * (w / 2 - 0.16), z0 + 0.06, depth * 0.62)) @ _rz(rnd.uniform(0, 360))
                      @ Matrix.Diagonal((1.0, 0.72, 1.0, 1.0)))
        b.add(ash.auto_sharp(60), "drift_pale", name=f"sill_ash_{k}", outline=False)     # (no cover: it would
        #                                                           pad the sill's hull proxy by the lens's height)
        name_tag(b, f, rnd.uniform(-0.2, 0.2), z0 + 0.018, depth, L["sill"], rnd)      # on the sill's front edge
        socks.append({"name": f"helm_{k + 1}", "pos": [round(v, 4) for v in (crown + V(0, 0, 0.03))]})
    obj = E.finish(b, pal, "decor", ground_h=WALL_H, name=name + ("_LOD1" if lod else ""))
    obj["af_sockets"] = json.dumps(socks)
    return obj


FLAME_PTS = [(0.0, 0.0), (0.30, 0.10), (0.42, 0.34), (0.36, 0.60), (0.20, 0.84), (0.16, 1.04), (0.26, 1.24),
             (0.10, 1.12), (-0.04, 0.92), (-0.14, 1.06), (-0.10, 1.34), (0.02, 1.56), (-0.20, 1.40), (-0.36, 1.10),
             (-0.44, 0.74), (-0.42, 0.40), (-0.28, 0.12)]


def flame_outline(height: float, cx: float = 0.0, z0: float = 0.0, grow: float = 0.0, lean: float = 0.0) -> list:
    """The flame's outline [(x, z)]: a teardrop with a forked, leaning tongue, ``height`` tall from ``z0``;
    ``grow`` metres pushed outward (a rim round it)."""
    s = height / 1.56
    pts = [(x * s, z * s) for x, z in FLAME_PTS]
    if grow:
        cx0 = sum(p[0] for p in pts) / len(pts)
        cz0 = sum(p[1] for p in pts) / len(pts)
        out = []
        for x, z in pts:
            d = math.hypot(x - cx0, z - cz0) or 1.0
            out.append((x + (x - cx0) / d * grow, z + (z - cz0) / d * grow))
        pts = out
    return [(cx + x + lean * z, z0 + z) for x, z in pts]


def build_lamp(pal: Palette, lit: bool = False, lod: int = 0):
    """A niche lamp (overlay on a wall tile, front −Y): a small loculus at shoulder height with an iron cresset on
    its sill. Unlit: cold oil, a dead wick, soot up the back. Lit (the cleanse, one by one): a flame — an ember
    tongue round a warm core — and the back of the niche washed warm. Same pivot, bounds and sockets in both."""
    name = "SM_Pit_NicheLamp" + ("_Lit" if lit else "")
    b, rnd = new(pal, "SM_Pit_NicheLamp", lod)               # one seed: the two states are the same stones
    SP.BUILT[(name, lod)] = SP.BUILT.pop(("SM_Pit_NicheLamp", lod))
    f = Face(0.0)
    z0, w, h, depth = 1.04, 0.50, 0.50, 0.22
    L = loculus(b, rnd, f, z0, w, h, depth, back="wash" if lit else "pit_dark")
    c = f.P(0.0, z0 + 0.06, depth - 0.115)
    # the cresset: a foot, a stem, a shallow iron bowl with a pinched spout toward the room
    prof = [(0.052, 0.0), (0.056, 0.012), (0.020, 0.024), (0.016, 0.062), (0.050, 0.078), (0.086, 0.104),
            (0.094, 0.128), (0.086, 0.132)]
    bowl = M.lathe(prof, sides=10, cap0="ngon", cap1=None)
    hid = b.add(bowl.translate(c), "lamp_frame", name="lamp", hull_pad=True)
    oil = M.lathe([(0.0, 0.120), (0.084, 0.118)], sides=10, cap0="fan", cap1=None)
    b.add(oil.translate(c), "wash" if lit else "oil", name="bowl_oil", outline=False, covered_by=hid)
    wick = M.tube([c + V(0.0, -0.03, 0.116), c + V(0.0, -0.05, 0.150)], [0.008, 0.005], sides=5)
    b.add(wick, "wick", name="wick", outline=False, covered_by=hid)
    tip = c + V(0.0, -0.05, 0.150)
    if lit:
        # the flame: an ember tongue (two crossed plates: it reads from every side) round a warm core
        for yaw in (0.0, 90.0):
            for reg_, hgt, lift in (("lamp_flame", 0.150, 0.0), ("lamp_core", 0.086, 0.0015)):
                pl = M.plate(SP_ccw(flame_outline(hgt)), 0.003)
                pl.transform(Matrix.Translation(tip + V(0, 0, -0.012)) @ _rz(yaw) @
                             Matrix(((1, 0, 0, 0), (0, 0, -1, lift), (0, 1, 0, 0), (0, 0, 0, 1))))
                b.add(pl.flat(), reg_, name="flame", outline=False)
    else:
        b.add(f.quad(-0.07, 0.07, z0 + 0.20, z0 + h - 0.075, 0.014), "soot", name="soot_back", outline=False)
    obj = E.finish(b, pal, "decor", ground_h=WALL_H, name=name + ("_LOD1" if lod else ""))
    obj["af_sockets"] = json.dumps([{"name": "flame", "pos": [round(v, 4) for v in (tip + V(0, 0, 0.05))]},
                                    {"name": "light", "pos": [0.0, -HALF - 0.75, 0.9]}])
    return obj


# ── the Tag Cellar: strands, curtain, rack ──────────────────────────────────────────────────────────────
class Strands:
    """Cords and name-tags of one asset gathered into two parts (``cords``: crossed ribbons; ``nametags``: little
    plates, ``af_sub`` 1 bronze / 2 dark bronze), with what the wind needs: each vertex's depth below its hang
    point and its strand's phase."""

    def __init__(self, rnd: random.Random, lod: int = 0, two_sided: bool = True):
        self.rnd, self.lod, self.two = rnd, lod, two_sided
        self.cords, self.tags = bmesh.new(), bmesh.new()
        self.sub = self.tags.faces.layers.int.new("af_sub")
        self.sway: list = []                                 # (top point, length, phase)
        self.n_tags = 0

    def _quad(self, bm, pts, n: Vector, sub=None, both: bool = False):
        vs = [bm.verts.new(tuple(p)) for p in pts]
        f = bm.faces.new(vs)
        f.normal_update()
        if f.normal.dot(n) < 0:
            f.normal_flip()
        if sub is not None:
            f[self.sub] = sub
        if both:
            g = bm.faces.new([bm.verts.new(tuple(p)) for p in reversed(pts)])
            g.normal_update()
            if g.normal.dot(n) > 0:
                g.normal_flip()
            if sub is not None:
                g[self.sub] = sub

    def strand(self, top: Vector, length: float, tags: int, front: Vector = Vector((0, -1, 0)), drift: float = 0.03,
               gap0: float = 0.10) -> None:
        """One cord from ``top`` hanging ``length``, ``tags`` plates knotted down it from ``gap0`` below the top."""
        rnd = self.rnd
        front = Vector(front).normalized()
        side = front.cross(Vector((0, 0, 1))).normalized()
        dx, dy = rnd.uniform(-drift, drift), rnd.uniform(-drift * 0.5, drift * 0.5)
        segs = 3

        def at(t: float) -> Vector:
            return top + side * (dx * t * t) + front * (dy * t) + Vector((0, 0, -length * t))
        w = 0.011
        for k in range(segs):
            a, c = at(k / segs), at((k + 1) / segs)
            self._quad(self.cords, [a - side * w / 2, a + side * w / 2, c + side * w / 2, c - side * w / 2], front,
                       both=self.two and not self.lod)
            if not self.lod:
                self._quad(self.cords, [a - front * w / 2, a + front * w / 2, c + front * w / 2, c - front * w / 2],
                           side, both=True)
        for k in range(tags):
            t = (gap0 + (length - gap0 - 0.03) * (k + rnd.uniform(0.15, 0.85)) / tags) / length
            c = at(t)
            yaw = rnd.uniform(-70.0, 70.0)
            tilt = rnd.uniform(-22.0, 22.0)
            r = _rz(yaw).to_3x3()
            tw, th = rnd.uniform(0.038, 0.050), rnd.uniform(0.056, 0.070)
            ax = r @ side
            down = (Matrix.Rotation(math.radians(tilt), 3, r @ front) @ Vector((0, 0, -1)))
            hang = c + (r @ front) * (0.006 * (1 if k % 2 else -1))
            pts = [hang - ax * tw / 2, hang + ax * tw / 2, hang + ax * tw / 2 + down * th, hang - ax * tw / 2 + down * th]
            self._quad(self.tags, pts, r @ front, sub=1 if rnd.random() < 0.68 else 2,
                       both=self.two or abs(yaw) > 40.0)
            self.n_tags += 1
        self.sway.append((top.copy(), length, rnd.random()))

    def add(self, b) -> set:
        self.cords.normal_update()
        self.tags.normal_update()
        ids = {b.add(M.Part(self.cords).flat(), "cord", name="cords", outline=False),
               b.add(M.Part(self.tags).flat(), "tag", name="nametags", outline=False,
                     sub_regions={1: "tag", 2: "tag_dark"})}
        return ids

    def wind(self, ids: set, amp: float = 0.55):
        """``finish(wind=…)``: each strand vertex sways with its depth below its own hang point (0 at the peg →
        ``amp`` at the cord's end: an underground draught, never a banner's flap); the rail, pegs and posts rigid."""
        sway = self.sway

        def fn(co: Vector, pid: int):
            if pid not in ids:
                return 0.0, 0.0
            best = min(sway, key=lambda s: (co.x - s[0].x) ** 2 + (co.y - s[0].y) ** 2)
            t = E.smoothstep(0.0, best[1], best[0].z - co.z)
            return amp * t, best[2]
        return fn


def peg(b, face: Face, x: float, z: float, out: float = 0.085) -> Vector:
    """An iron peg driven into the wall; returns its tip (where a cord hangs)."""
    a, c = face.P(x, z, -0.01), face.P(x, z + 0.012, out)
    b.add(M.tube([a, c], [0.011, 0.008], sides=5), "iron", name="peg", hull_pad=True)
    b.add(M.ellipsoid((0.016, 0.016, 0.016), tuple(c), sides=5, rings=3), "iron_dark", name="peg_pin", outline=False)
    return c


def build_tag_strand(pal: Palette, variant: str = "A", lod: int = 0):
    """One instanced strand of the Tag Cellar (overlay on a wall tile, front −Y, standing on the tile's centre line:
    the UE lane offsets instances along the wall): an iron peg near the wall's top and a cord of name-tags. A:
    1.50 m, 16 tags; B: 1.12 m, 11 tags; C: a bunch of three cords on one peg (34 tags)."""
    name = f"SM_Pit_TagStrand_{variant}"
    b, rnd = new(pal, name, lod)
    f = Face(0.0)
    tip = peg(b, f, 0.0, 1.90)
    st = Strands(rnd, lod, two_sided=False)
    if variant == "A":
        st.strand(tip + V(0, 0, -0.008), 1.50, 16)
    elif variant == "B":
        st.strand(tip + V(0, 0, -0.008), 1.12, 11)
    else:
        for k, (L, n) in enumerate(((1.42, 14), (1.05, 10), (1.26, 10))):
            st.strand(tip + V((k - 1) * 0.012, -0.004 * k, -0.008), L, n, drift=0.07)
    ids = st.add(b)
    # no hull: a cord is 1 cm wide (a hull would draw it as a 3 px black bar) and the peg is a nail's head
    obj = E.finish(b, pal, "none", ground_h=WALL_H, wind=st.wind(ids), name=name + ("_LOD1" if lod else ""))
    obj["af_sockets"] = json.dumps([{"name": "peg", "pos": [round(v, 4) for v in tip]}])
    obj["af_tags"] = st.n_tags
    return obj


def build_tag_curtain(pal: Palette, lod: int = 0):
    """A wall tile's curtain of name-tags (overlay, front −Y): a charred peg rail under the wall's top and thirteen
    cords, each a different length, ~150 tags — ten such tiles are the Cellar's thousands."""
    name = "SM_Pit_TagCurtain"
    b, rnd = new(pal, name, lod)
    f = Face(0.0)
    rail = SP.beam(f.P(-0.47, 1.88, 0.045), f.P(0.47, 1.90, 0.045), 0.07, rows=3, jitter=0.05, rnd=rnd)
    b.add(rail, "charred", name="rail", hull_pad=True)
    for x in (-0.36, 0.36):
        b.add(M.tube([f.P(x, 1.89, -0.01), f.P(x, 1.89, 0.09)], [0.014, 0.012], sides=5), "iron", name="rail_pin",
              outline=False)                                # (no cover: a pin 1 cm proud padded the rail's proxy 55 mm)
    st = Strands(rnd, lod, two_sided=False)
    for k in range(13):
        x = -0.43 + 0.86 * k / 12 + rnd.uniform(-0.012, 0.012)
        L = rnd.uniform(0.95, 1.50) if k % 3 else rnd.uniform(1.30, 1.58)
        top = f.P(x, 1.845, 0.045 + (0.022 if k % 2 else -0.018))
        st.strand(top, L, max(8, int(L * 8.4) + rnd.randint(-1, 1)), drift=0.035)
    ids = st.add(b)
    obj = E.finish(b, pal, "decor", ground_h=WALL_H, wind=st.wind(ids), name=name + ("_LOD1" if lod else ""))
    obj["af_sockets"] = "[]"
    obj["af_tags"] = st.n_tags
    return obj


def build_tag_rack(pal: Palette, lod: int = 0):
    """A free-standing rack of the Tag Cellar (2 × 1 tiles, blocking): two charred posts on stone feet, a beam at
    2.0 m with sixteen cords of name-tags, a lower rail that keeps them from tangling; read from both sides."""
    name = "SM_Pit_TagRack"
    b, rnd = new(pal, name, lod)
    X = 0.90
    for sg in (-1, 1):
        post = SP.beam(V(sg * X, 0, 0.0), V(sg * X + rnd.uniform(-0.02, 0.02), 0, 2.08), 0.10, rows=5, jitter=0.05,
                       rnd=rnd)
        b.add(post, "charred", name="post", hull_pad=True)
        SP.dry_stone(b, V(sg * X, 0, 0.11), (0.30, 0.30, 0.22), rnd, region="pit_stone", name="foot")
        brace = SP.beam(V(sg * X, 0, 1.62), V(sg * (X - 0.34), 0, 1.98), 0.06, rows=2, jitter=0.04, rnd=rnd)
        b.add(brace, "charred", name="brace", hull_pad=True)
    beam = SP.beam(V(-X - 0.14, 0, 2.02), V(X + 0.14, 0, 2.00), 0.10, 0.09, rows=6, jitter=0.05, rnd=rnd)
    b.add(beam, "char_black", name="beam", hull_pad=True)
    low = SP.beam(V(-X, 0, 0.52), V(X, 0, 0.50), 0.05, rows=4, jitter=0.04, rnd=rnd)
    b.add(low, "charred", name="low_rail", hull_pad=True)
    st = Strands(rnd, lod, two_sided=True)
    for k in range(16):
        x = -X + 0.10 + (2 * X - 0.20) * k / 15 + rnd.uniform(-0.015, 0.015)
        L = rnd.uniform(1.0, 1.42)
        st.strand(V(x, 0.03 if k % 2 else -0.03, 1.955), L, max(8, int(L * 8.0) + rnd.randint(-1, 1)),
                  front=V(0, -1 if k % 2 == 0 else 1, 0), drift=0.03)
    ids = st.add(b)
    obj = E.finish(b, pal, "decor", ground_h=2.1, wind=st.wind(ids), name=name + ("_LOD1" if lod else ""),
                   ground_band=0.02)
    obj["af_sockets"] = "[]"
    obj["af_tags"] = st.n_tags
    return obj


# ── the Ash Steps (entry / exit) ────────────────────────────────────────────────────────────────────────
def build_ash_steps(pal: Palette, lod: int = 0):
    """The Ash Steps: the stair the hero comes down and the way back up. The pivot is the **exit tile** (floor, in
    front of a straight wall row); the piece replaces the three wall tiles of the row behind it (y +0.5 .. +1.5,
    which the runtime leaves bare round an exit): two wall masses and, between them, a steep stair rising into a
    black passage roofed by the mass. Ash chokes it: banked on every tread, spilled out over the exit tile; a burnt
    beam has come down across the lower steps, a kettle helm lies on the second; a charred timber frame stands at
    its foot."""
    name = "SM_Pit_AshSteps"
    b, rnd = new(pal, name, lod)
    W = 0.64                                                 # half width of the passage
    Y0, Y1 = HALF, HALF + TILE                               # the wall row behind the exit tile
    # the wall masses either side and the lintel mass over the passage
    for sg in (-1, 1):
        xa, xb = sorted((sg * W, sg * 1.5))
        core = SP.box((xb - xa, TILE, WALL_H), ((xa + xb) / 2, (Y0 + Y1) / 2, WALL_H / 2))
        cid = b.add(core, "wall", name="core", hull_pad=True)
        f = Face(0.0, -Y0)                                   # the front face of the row (at y = +0.5)
        masonry(b, rnd, f, cid, 0.24, WALL_H - 0.02, xa, xb, name=f"_f{sg}")
        b.add(f.box(xa, xb, 0.0, 0.24, 0.025), "plinth", name="plinth", hull_pad=True)
        fe = Face(90.0 * sg, 1.5, origin=(0.0, (Y0 + Y1) / 2, 0.0))    # the row's end (seen where the row ends)
        masonry(b, rnd, fe, cid, 0.24, WALL_H - 0.02, name=f"_e{sg}")
        b.add(fe.box(-HALF, HALF, 0.0, 0.24, 0.025), "plinth", name="plinth", hull_pad=True)
        side = Face(90.0 * sg, W if sg > 0 else W)           # the face toward the stair
        side.m = Matrix.Translation(V(sg * W, (Y0 + Y1) / 2, 0)) @ _rz(-90.0 * sg)
        side.dist = 0.0
        side.n = (side.m.to_3x3() @ Vector((0, -1, 0))).normalized()
        b.add(side.quad(-HALF + 0.01, HALF - 0.01, 0.0, WALL_H - 0.43, 0.002), "earth", name=f"passage_side{sg}",
              outline=False, covered_by=cid)                # the passage's walls: in the dark
        top = M.Part(bmesh.new())
        f_ = top.bm.faces.new([top.bm.verts.new((x, y, WALL_H + 0.003)) for x, y in
                               ((xa, Y0), (xb, Y0), (xb, Y1), (xa, Y1))])
        f_.normal_update()
        if f_.normal.z < 0:
            f_.normal_flip()
        b.add(top.flat(), "top", name="top", outline=False, covered_by=cid)
    lint = SP.box((2 * W + 0.04, TILE, 0.42), (0, (Y0 + Y1) / 2, WALL_H - 0.21))
    lid = b.add(lint, "wall", name="core_lintel", hull_pad=True)
    b.add(Face(0.0, -Y0).quad(-W, W, WALL_H - 0.42, WALL_H - 0.02, 0.003), "wall_dark", name="back_lintel",
          outline=False, covered_by=lid)
    top = M.Part(bmesh.new())
    f_ = top.bm.faces.new([top.bm.verts.new((x, y, WALL_H + 0.003)) for x, y in ((-W, Y0), (W, Y0), (W, Y1), (-W, Y1))])
    f_.normal_update()
    if f_.normal.z < 0:
        f_.normal_flip()
    b.add(top.flat(), "top", name="top", outline=False, covered_by=lid)
    # the passage: black behind and above the steps
    psg = bmesh.new()
    for pts in ([(-W, Y1 - 0.01, 0.0), (W, Y1 - 0.01, 0.0), (W, Y1 - 0.01, WALL_H - 0.42), (-W, Y1 - 0.01, WALL_H - 0.42)],
                [(-W, Y0 + 0.02, WALL_H - 0.425), (W, Y0 + 0.02, WALL_H - 0.425), (W, Y1, WALL_H - 0.425),
                 (-W, Y1, WALL_H - 0.425)]):
        psg.faces.new([psg.verts.new(p) for p in pts])
    bmesh.ops.recalc_face_normals(psg, faces=psg.faces)
    for fc in psg.faces:
        c = fc.calc_center_median()
        if fc.normal.dot(V(0, Y0 - 0.2, 0.6) - c) < 0:
            fc.normal_flip()
    b.add(M.Part(psg).flat(), "pit_dark", name="passage", outline=False)
    # the steps: five steep treads from the exit tile's back edge up into the dark
    n, rise, tread = 5, 0.30, 0.25
    ya = Y0 - 0.14
    for k in range(n):
        y0 = ya + tread * k
        st = SP.box((2 * W, Y1 - y0, rise), (0, (y0 + Y1) / 2, rise * (k + 0.5)))
        sid = b.add(st, "step", name=f"step_{k}", hull_pad=True)
        edge = bmesh.new()
        z = rise * (k + 1) + 0.002
        fe = edge.faces.new([edge.verts.new(p) for p in ((-W + 0.02, y0, z), (W - 0.02, y0, z),
                                                         (W - 0.02, y0 + 0.07, z), (-W + 0.02, y0 + 0.07, z))])
        fe.normal_update()
        if fe.normal.z < 0:
            fe.normal_flip()
        b.add(M.Part(edge).flat(), "step_worn", name=f"step_edge_{k}", outline=False, covered_by=sid)
        if k < 4:                                            # ash banked on the tread, against a side wall
            sgn = -1 if k % 2 else 1
            ash_mound(b, V(sgn * rnd.uniform(0.25, 0.42), y0 + 0.14, 0), rnd.uniform(0.18, 0.26),
                        rnd.uniform(0.045, 0.065), rnd, region="drift_pale", name="tread_ash",
                        z=rise * (k + 1) - 0.006)
    # ash spilled out of the stair over the exit tile: one drift
    ash_drift(b, [(0.02, ya - 0.20, 0.62, 0.085), (-0.46, ya - 0.02, 0.38, 0.06), (0.52, ya - 0.06, 0.34, 0.055),
                  (0.12, ya - 0.62, 0.44, 0.04), (-0.30, ya - 0.50, 0.30, 0.035)], rnd, name="spill")
    # a burnt beam come down across the lower steps; a helm on the second
    bm_ = SP.beam(V(-0.50, ya - 0.20, 0.12), V(0.44, ya + 0.62, 0.86), 0.11, rows=5, jitter=0.07, rnd=rnd)
    b.add(bm_, "char_black", name="fallen_beam", hull_pad=True)
    SP.kettle_helm(b, V(-0.30, ya + tread * 1.5, rise * 2 + 0.035), 0.10, _rx(-64) @ _rz(30), region="helm_iron",
                   name="helm_step")
    # the charred frame at the stair's foot
    for sg in (-1, 1):
        post = SP.beam(V(sg * (W + 0.06), Y0 - 0.10, 0.0), V(sg * (W + 0.05), Y0 - 0.08, 2.16), 0.13, rows=5,
                       jitter=0.06, rnd=rnd)
        b.add(post, "charred", name="frame_post", hull_pad=True)
    head = SP.beam(V(-W - 0.22, Y0 - 0.09, 2.22), V(W + 0.22, Y0 - 0.09, 2.20), 0.14, 0.13, rows=5, jitter=0.06,
                   rnd=rnd)
    b.add(head, "char_black", name="frame_head", hull_pad=True)
    for k in range(4):
        SP.rubble(b, V(rnd.uniform(-1.3, 1.3), Y0 - rnd.uniform(0.08, 0.30), 0), rnd.uniform(0.05, 0.10), rnd,
                  region="wall", name="stone")
    obj = E.finish(b, pal, "decor", ground_h=WALL_H, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "exit", "pos": [0.0, 0.0, 0.0]},
                                    {"name": "fx_ring", "pos": [0.0, 0.0, 0.02]},      # the exit_portal slot's
                                    {"name": "fx_vortex", "pos": [0.0, Y0 + 0.45, 1.25]},
                                    {"name": "top", "pos": [0.0, Y1 - 0.1, 1.5]}])
    return obj


# ── the Flame Niche ─────────────────────────────────────────────────────────────────────────────────────
MARK_R, MARK_H = 0.80, 0.36                                 # the five burn marks: ring radius, each mark's height
NICHE_H, NICHE_ZC = 1.06, 1.36                              # the empty niche: its height, its centre on the slab


def build_flame_niche(pal: Palette, lod: int = 0):
    """火龛 the Flame Niche (the S7 turn-in beat): a slab of pale dressed stone leaning back against the wall on two
    worn steps, an **empty niche cut in the shape of a flame** (black, a pale chamfered rim round it) and five burn
    marks ringed round it, each a small scorched flame pointing away from the empty heart — the hero's brand. Ash
    lies at its foot; nothing else has been here for a thousand years. Overlay on three wall tiles (pivot on the
    middle one's centre, front −Y), projecting 0.9 m over the floor row in front."""
    name = "SM_Pit_FlameNiche"
    b, rnd = new(pal, name, lod)
    SW, SH, ST = 2.20, 2.46, 0.36                            # the slab: width, height, thickness at the foot
    lean = -11.0                                             # degrees: the face leans back (reads from the W1 pitch)
    base_y = -HALF - 0.50                                    # the slab's front face at its foot
    m = Matrix.Translation(V(0, base_y, 0.30)) @ _rx(lean)
    slab = SP.box((SW, ST, SH), (0, ST / 2, SH / 2), bevel=0.03)
    sid = b.add(slab.transform(m), "shrine", name="slab", hull_pad=True,
                hull=None if lod else SP.box((SW, ST, SH), (0, ST / 2, SH / 2)).transform(m))
    f = Face(0.0, 0.0)
    f.m = m
    f.n = (m.to_3x3() @ Vector((0, -1, 0))).normalized()
    # ashlar joints: three courses, staggered
    for z in (0.62, 1.24, 1.86):
        b.add(f.quad(-SW / 2 + 0.03, SW / 2 - 0.03, z - 0.008, z + 0.008), "shrine_joint", name="course_h",
              outline=False, covered_by=sid)
    for z0, z1, xs in ((0.0, 0.62, (-0.55, 0.55)), (0.62, 1.24, (-0.86, 0.86)), (1.24, 1.86, (-0.55, 0.55)),
                       (1.86, SH, (-0.86, 0.0, 0.86))):
        for x in xs:
            b.add(f.quad(x - 0.007, x + 0.007, z0 + 0.01, z1 - 0.01), "shrine_joint", name="course_v", outline=False,
                  covered_by=sid)
    # the empty niche: a pale rim, the black flame inside it (its own inner shadow a step darker on the lit side)
    z_n = NICHE_ZC - NICHE_H / 2
    b.add(f.plate(flame_outline(NICHE_H + 0.02, 0.0, z_n - 0.01, grow=0.075), 0.003, 0.004), "shrine_edge",
          name="rim", outline=False, covered_by=sid)
    b.add(f.plate(flame_outline(NICHE_H, 0.0, z_n), 0.008, 0.004), "pit_dark", name="niche", outline=False,
          covered_by=sid)
    # five burn marks round it (72° apart, the first straight above): soot, then the charred flame
    marks = []
    for k in range(5):
        a = math.radians(90.0 - 72.0 * k)
        cx, cz = math.cos(a) * MARK_R, NICHE_ZC + math.sin(a) * MARK_R * 0.94
        rot = math.degrees(a) - 90.0
        for reg_, hgt, grow, lift in (("soot", MARK_H * 1.25, 0.03, 0.0025), ("char_black", MARK_H, 0.0, 0.006)):
            ol = flame_outline(hgt, 0.0, -hgt * 0.42, grow=grow)
            c_, s_ = math.cos(math.radians(rot)), math.sin(math.radians(rot))
            ol = [(cx + x * c_ - z * s_, cz + x * s_ + z * c_) for x, z in ol]
            b.add(f.plate(ol, lift, 0.003), reg_, name=f"mark_{k + 1}", outline=False, covered_by=sid)
        marks.append(f.P(cx, cz, 0.01))
    # the capstone and the two worn steps
    cap = SP.box((SW + 0.12, ST + 0.08, 0.14), (0, ST / 2, SH + 0.07), bevel=0.02).transform(m)
    b.add(cap, "cap", name="cap", hull_pad=True)
    for k, (w, d, h, z) in enumerate(((2.74, 0.92, 0.16, 0.0), (2.44, 0.70, 0.15, 0.16))):
        st = SP.box((w, d, h), (0, -HALF - d / 2 + 0.02, z + h / 2), bevel=0.015)
        stid = b.add(st, "step", name=f"step_{k}", hull_pad=True,
                     hull=None if lod else SP.box((w, d, h), (0, -HALF - d / 2 + 0.02, z + h / 2)))
        edge = bmesh.new()
        ze, yf = z + h + 0.002, -HALF - d + 0.02
        fe = edge.faces.new([edge.verts.new(p) for p in ((-w / 2 + 0.02, yf, ze), (w / 2 - 0.02, yf, ze),
                                                         (w / 2 - 0.02, yf + 0.08, ze), (-w / 2 + 0.02, yf + 0.08, ze))])
        fe.normal_update()
        if fe.normal.z < 0:
            fe.normal_flip()
        b.add(M.Part(edge).flat(), "step_worn", name=f"step_edge_{k}", outline=False, covered_by=stid)
    # ash at its foot
    for c, r, h in ((V(-0.86, -HALF - 0.62, 0.31), 0.28, 0.045), (V(0.70, -HALF - 0.56, 0.31), 0.24, 0.04),
                    (V(-1.34, -HALF - 0.50, 0.0), 0.34, 0.05), (V(1.38, -HALF - 0.62, 0.0), 0.30, 0.045),
                    (V(0.10, -HALF - 0.98, 0.0), 0.36, 0.035)):
        ash_mound(b, V(c.x, c.y, 0), r, h, rnd, region="drift", name="foot_ash", z=c.z)
    obj = E.finish(b, pal, "interactive", ground_h=SH, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    niche_c = f.P(0.0, NICHE_ZC, 0.02)
    socks = [{"name": "niche", "pos": [round(v, 4) for v in niche_c]},
             {"name": "turn_in", "pos": [0.0, -HALF - 1.45, 0.0]}]
    socks += [{"name": f"mark_{k + 1}", "pos": [round(v, 4) for v in p]} for k, p in enumerate(marks)]
    obj["af_sockets"] = json.dumps(socks)
    return obj


# ── ground dressing ─────────────────────────────────────────────────────────────────────────────────────
def build_ash_drift(pal: Palette, variant: str = "A", lod: int = 0):
    """Drifted ash on the flags (the Ash Steps room, anywhere the Pit burned): A — a long low drift; B — a drift
    against rubble, a kettle helm half buried in it. The drift itself is un-inked and feathered (:func:`ash_drift`)."""
    name = f"SM_Pit_AshDrift_{variant}"
    b, rnd = new(pal, name, lod)
    if variant == "A":
        ash_drift(b, [(-0.30, 0.05, 0.52, 0.07), (0.36, -0.10, 0.40, 0.055), (0.05, 0.38, 0.34, 0.045),
                      (-0.62, -0.26, 0.26, 0.035)], rnd)
    else:
        ash_drift(b, [(0.0, 0.0, 0.56, 0.08), (-0.46, 0.22, 0.34, 0.05), (0.40, 0.28, 0.30, 0.045)], rnd)
        for k in range(4):
            a = rnd.uniform(0, math.tau)
            SP.rubble(b, V(math.cos(a) * 0.5, math.sin(a) * 0.42, 0), rnd.uniform(0.07, 0.13), rnd, region="wall",
                      name="stone")
        SP.kettle_helm(b, V(0.30, -0.24, 0.05), 0.10, _rx(38) @ _rz(-40), region="helm_iron", name="helm_drift")
    # A is a drift and nothing else: no hull (class ``none``, as ground cover); B's stones and helm ink as decor
    obj = E.finish(b, pal, "none" if variant == "A" else "decor", ground_h=0.3,
                   name=name + ("_LOD1" if lod else ""), ground_band=None if variant == "A" else 0.02)
    obj["af_sockets"] = "[]"
    return obj


# ── the surface: the pit mouth unsealed ─────────────────────────────────────────────────────────────────
def build_pit_mouth_open(pal: Palette, lod: int = 0):
    """The mouth of the Pit at (17,26) once Wave 3 unseals it (``SM_Prop_March_PitMouthSealed`` opened: the same
    ring of stones, banked ash and timber collar, the same footprint): the iron grate thrown back on its chain
    against two stakes, the charred planks heaped to one side, a ladder standing in the opening, the shaft's far
    wall going down into black. No warm light; the breach seep still shows in the depth, dimly (two smears low on
    the far wall, ``crypt.seep_dim`` in a ``seep_halo``). Sockets ``smoke``, ``entrance`` (the subDungeonEntrances
    point), ``ladder_top``."""
    name = "SM_Prop_March_PitMouthOpen"
    b, rnd = new(pal, name, lod)
    RX, RY = 1.15, 0.9
    hole = [(math.cos(math.tau * k / 24) * RX * (1 + 0.05 * math.sin(k * 2.2)),
             math.sin(math.tau * k / 24) * RY * (1 + 0.05 * math.sin(k * 1.7 + 1))) for k in range(24)]
    b.add(M.plate(hole, 0.006).translate(V(0, 0, 0.005)), "pit_dark", name="hole", outline=False)
    # the shaft's far wall (the +Y side, away from the approach / the camera): two courses stepping down into the
    # dark, both well under the collar's value — dark earth, then the near-black throat (art review r1: a first
    # course in pit.stone was a pale rim round a black disc: a dish, not a depth)
    for k, (rg, s0, s1) in enumerate((("earth", 0.97, 0.82), ("shaft", 0.82, 0.62))):
        ring = []
        for j in range(13):
            a = math.radians(18.0 + 144.0 * j / 12)
            ring.append((math.cos(a) * RX * s0, math.sin(a) * RY * s0 - 0.10 * k))
        for j in range(12, -1, -1):
            a = math.radians(18.0 + 144.0 * j / 12)
            ring.append((math.cos(a) * RX * s1, math.sin(a) * RY * s1 - 0.10 * (k + 1)))
        b.add(M.plate(SP_ccw(ring), 0.004).translate(V(0, 0, 0.011 + 0.001 * k)), rg, name=f"shaft_{k}", outline=False)
    # the breach seep, deep in it: two dim smears low on the far wall (a halo a step over the dark, a dimmer-than-
    # the-seal core), the nearer one behind the ladder — a first scatter of five saturated ovals read as gems or
    # berries lying in the hole
    for k, (pts, w) in enumerate((([(-0.36, 0.23), (-0.12, 0.31), (0.14, 0.30), (0.34, 0.21)], 0.12),
                                  ([(-0.46, 0.05), (-0.30, 0.01), (-0.14, 0.00)], 0.075))):
        for rg, grow, lift in (("seep_halo", 1.9, 0.0165), ("seep_dim", 1.0, 0.0185)):
            b.add(M.plate(SP_ccw(smear(pts, w * grow, rnd)), 0.0015).translate(V(0, 0, lift)), rg,
                  name=f"seep_{k}", outline=False)
    # the rim: stones and banked ash all round
    for k in range(16):
        a = math.tau * (k + rnd.uniform(-0.2, 0.2)) / 16
        c = V(math.cos(a) * (RX + 0.18), math.sin(a) * (RY + 0.18), 0)
        SP.rubble(b, c, rnd.uniform(0.11, 0.17), rnd, region="pit_stone", name="rim_stone")
        if k % 2:
            ash_mound(b, c * 1.12, 0.2, 0.07, rnd, region="ash", name="rim_ash")
    # the timber collar
    for sy in (-1, 1):
        b.add(SP.beam(V(-RX - 0.15, sy * (RY - 0.05), 0.09), V(RX + 0.15, sy * (RY - 0.05), 0.09), 0.18, rows=5,
                      jitter=0.06, rnd=rnd), "char_black", name="collar", hull_pad=True)
    for sx in (-1, 1):
        b.add(SP.beam(V(sx * (RX - 0.1), -RY - 0.12, 0.2), V(sx * (RX - 0.1), RY + 0.12, 0.2), 0.17, rows=4,
                      jitter=0.06, rnd=rnd), "char_black", name="collar", hull_pad=True)
    # the seal broken: the planks heaped on the +X side
    for k in range(5):
        c = V(RX + 0.62 + rnd.uniform(-0.08, 0.08), -0.25 + 0.16 * k + rnd.uniform(-0.05, 0.05), 0.06 + 0.055 * k)
        SP.board(b, "char_black" if k % 3 else "charred", (0.2, 1.7 - 0.12 * k, 0.05), c,
                 _rz(rnd.uniform(-16, 16)) @ _ry(rnd.uniform(-5, 5)), rnd, name="seal_plank")
    # the grate thrown back: leaning on two stakes at the −X side, a chain still on it
    gw, gd = 1.1, 0.9
    gm = Matrix.Translation(V(-RX - 0.50, 0.05, 0.50)) @ _ry(-62.0) @ _rz(6.0)
    for k in range(6):
        y = -gd / 2 + gd * k / 5
        b.add(SP.box((gw, 0.035, 0.035), (0, y, 0), bevel=0.006).transform(gm), "iron", name="grate", hull_pad=True)
    for k in range(6):
        x = -gw / 2 + gw * k / 5
        b.add(SP.box((0.035, gd, 0.035), (x, 0, 0.03), bevel=0.006).transform(gm), "iron", name="grate",
              hull_pad=True)
    for sy in (-1, 1):
        at = V(-RX - 0.78, sy * 0.42, 0)
        SP.stump(b, at, 0.62, 0.05, rnd, region="timber", lean=5.0, name="chain_stake")
    corner = gm @ V(gw / 2, gd / 2, 0.02)
    end = V(-RX - 0.10, RY + 0.36, 0.03)
    b.add(M.tube([corner, corner.lerp(end, 0.5) - V(0, 0, 0.16), end], [0.016, 0.016, 0.016], sides=5), "chain",
          name="chain", hull_pad=True)
    # the ladder standing in the opening, on the approach (−Y) side: its rails stand a hero's chest above the rim
    foot = V(0.0, -RY + 0.56, 0.02)
    top = V(0.0, -RY + 0.06, 1.28)
    for sx in (-1, 1):
        rail = SP.beam(foot + V(sx * 0.25, 0, 0), top + V(sx * 0.27, 0, 0), 0.07, rows=4, jitter=0.04, rnd=rnd)
        b.add(rail, "timber", name="ladder_rail", hull_pad=True)
    for k in range(4):
        t = 0.16 + 0.22 * k
        c = foot.lerp(top, t)
        b.add(M.tube([c + V(-0.27, 0, 0), c + V(0.27, 0, 0)], [0.028, 0.028], sides=6), "timber_end",
              name="ladder_rung", hull_pad=True)
    obj = E.finish(b, pal, "decor", ground_h=1.3, name=name + ("_LOD1" if lod else ""), ground_band=0.02)
    obj["af_sockets"] = json.dumps([{"name": "smoke", "pos": [0.0, 0.1, 0.3]},
                                    {"name": "entrance", "pos": [0.0, 0.0, 0.0]},
                                    {"name": "ladder_top", "pos": [round(v, 3) for v in top]}])
    return obj


# ── registry ────────────────────────────────────────────────────────────────────────────────────────────
def _builders() -> dict:
    out = {f"SM_Pit_Wall_{v}": (lambda pal, lod=0, v=v: build_wall(pal, v, lod)) for v in "ABC"}
    out["SM_Pit_WallPier"] = build_pier
    out.update({f"SM_Pit_HelmNiche_{v}": (lambda pal, lod=0, v=v: build_helm_niche(pal, v, lod)) for v in "AB"})
    out["SM_Pit_NicheLamp"] = lambda pal, lod=0: build_lamp(pal, False, lod)
    out["SM_Pit_NicheLamp_Lit"] = lambda pal, lod=0: build_lamp(pal, True, lod)
    out.update({f"SM_Pit_TagStrand_{v}": (lambda pal, lod=0, v=v: build_tag_strand(pal, v, lod)) for v in "ABC"})
    out["SM_Pit_TagCurtain"] = build_tag_curtain
    out["SM_Pit_TagRack"] = build_tag_rack
    out["SM_Pit_AshSteps"] = build_ash_steps
    out["SM_Pit_FlameNiche"] = build_flame_niche
    out.update({f"SM_Pit_AshDrift_{v}": (lambda pal, lod=0, v=v: build_ash_drift(pal, v, lod)) for v in "AB"})
    out["SM_Prop_March_PitMouthOpen"] = build_pit_mouth_open
    return out


WALL_OVERLAY = ("an overlay on a wall tile: placed on the wall tile's own centre, its front (the mesh's −Y) turned to "
                "the face that looks onto the floor (yaw in 90° steps); it sits proud of the plain wall block, so the "
                "wall mesh under it stays")
LAYER = ("U6 (implementation-plan.md §4: the Pit's dressing from its map data) through the U4 set-piece layer "
         "(set_pieces.json zone sub_rf_pit, tile anchors) or the map's decorations")
# per asset: plain ids, themed ids (kit.theme: <id>@pit), footprint, blocking, where, note
SPECS = {
    **{f"SM_Pit_Wall_{v}": dict(
        ids=["wall_pit", "setpiece_pit_wall"], themed=["outcrop_plains"], fp=[1, 1], blocking=True, blob=0.01,
        where="every wall tile (tile type 4) of sub_rf_pit",
        placement={"yawStepDeg": 90, "variants": "A/B/C by tileHash(c, r, 3) % 3 as the outcrops",
                   "why": "the block is dressed on four faces and abuts its neighbours face to face: a hashed 0-359° "
                          "yaw (AAbyssZoneActor::BuildWalls) must be snapped to (hash >> 8) % 4 * 90 for these ids"},
        note="1 × 1 × 2.0 m; the black top is the unexcavated earth: wall masses read as cut-out darkness; LOD1 (also "
             "the mobile-low mesh) keeps the bed joints and the plinth joint (66 toon triangles), the perpends and "
             "the single stones are LOD0 only"
             + {"A": "", "B": "; variant B: a fallen stone and its socket on two faces",
                "C": "; variant C: ash banked at the foot of two faces (reaches 0.15 m over the floor), soot streaks"}[v])
       for v in "ABC"},
    "SM_Pit_WallPier": dict(ids=["setpiece_pit_wall_pier"], fp=[1, 1], blocking=True, blob=0.01,
                            where="wall corners (inner and outer), wall ends, door jambs, every 4-6 tiles of a long wall",
                            note=WALL_OVERLAY + "; 1.12 m square and 2.37 m tall: it encloses the plain block "
                                 "(the kit's corner piece); a solid weathered capstone (the one stone top in the "
                                 "Pit)"),
    **{f"SM_Pit_HelmNiche_{v}": dict(
        ids=["setpiece_pit_helm_niche"], fp=[1, 1], blocking=False, blob=0.01,
        where="the Helm Wall (24,8): the wall tiles facing the six dormant footmen, A and B alternating",
        note=WALL_OVERLAY + ("; one loculus, one kettle helm" if v == "A" else "; two tiers, two kettle helms")
             + " standing crown up, tipped back against the niche; sockets helm_<n> over each crown (the cleanse's "
               "embers may rise from them)") for v in "AB"},
    "SM_Pit_NicheLamp": dict(ids=["setpiece_pit_lamp"], fp=[1, 1], blocking=False, blob=0.01,
                             where="every room: one every 3-5 wall tiles, two by the Flame Niche",
                             note=WALL_OVERLAY + "; the unlit state (until rf_pit_cleansed). Sockets flame (the wick), "
                                  "light (the pool's centre on the floor, 0.75 m in front)"),
    "SM_Pit_NicheLamp_Lit": dict(ids=["setpiece_pit_lamp_lit"], fp=[1, 1], blocking=False, blob=0.01, warm=True,
                                 where="replaces setpiece_pit_lamp at the same transform when its turn in the "
                                       "cleanse sequence comes (and at once on a zone entry with rf_pit_cleansed)",
                                 note=WALL_OVERLAY + "; the lit state: prop.pit_lamp.light once at the swap, "
                                      "prop.pit_lamp.flame looping on socket flame, the light pool on socket light "
                                      "(vfx_recipes.json pit section: sequence and timing)"),
    **{f"SM_Pit_TagStrand_{v}": dict(
        ids=["setpiece_pit_tag_strand"], fp=[1, 1], blocking=False, blob=0.01, wind=True,
        where="the Tag Cellar (10,24): instanced along its walls",
        note=WALL_OVERLAY + "; one strand on the tile's centre line — instance it with an offset of −0.45 .. +0.45 m "
             "along the wall (6-10 to a tile, variants by hash); the cords sway (MI_AF_Toon_Props_March_Wind, "
             "weights 0 at the peg to 0.55 at the end)") for v in "ABC"},
    "SM_Pit_TagCurtain": dict(ids=["setpiece_pit_tag_curtain"], fp=[1, 1], blocking=False, blob=0.01, wind=True,
                              where="the Tag Cellar (10,24): every wall tile that faces the room",
                              note=WALL_OVERLAY + "; a peg rail and 13 cords in one mesh (the cheap way to the "
                                   "thousands: ~150 tags a tile)"),
    "SM_Pit_TagRack": dict(ids=["setpiece_pit_tag_rack"], fp=[2, 1], blocking=True, blob=0.5, wind=True,
                           where="the Tag Cellar (10,24): rows of racks on the floor, aisles between them",
                           note="free-standing, 2 × 1 tiles along the mesh's X; BLOCKING: mark its two tiles in the "
                                "map's collisions (set pieces do not block by themselves, DECISIONS DR15)"),
    "SM_Pit_AshSteps": dict(ids=["setpiece_pit_ash_steps"], themed=["exit_portal"], fp=[1, 1], blocking=False,
                            blob=0.01, ink_ignore=("spill",), where="the Ash Steps (5,5): the sub-dungeon's exit tile (and where the hero "
                                             "arrives)",
                            note="pivot = the exit tile (floor) in front of a straight wall row; the mesh replaces the "
                                 "three wall tiles of the row behind it (the runtime leaves the 3 × 3 round an exit "
                                 "bare: BuildWalls NearExit) — if the map recesses the exit tile into the wall, put "
                                 "setpiece_pit_wall on the tiles left and right of it. Same footprint / blocking as "
                                 "the exit_portal slot; the exit recipe is exit.portal@pit (ash sifting down the "
                                 "stair, no vortex) at socket fx_vortex"),
    "SM_Pit_FlameNiche": dict(ids=["setpiece_pit_flame_niche"], fp=[3, 1], blocking=False, blob=0.01,
                              placement={"facing": "key-lit", "yawDeg": 180, "floorNeighbour": [0, -1],
                                         "why": "the slab is the palest thing in the room and the beat is the black "
                                                "flame cut in it: stand it on a wall whose floor lies toward -row "
                                                "(the floor tile it faces is (c, r - 1): UE yaw 180, the room's far "
                                                "wall in the upper right of the frame, as in the vignette) — the "
                                                "one facing that is both seen and key-lit at the W1 camera. Facing "
                                                "-col (yaw 90) it is seen in the shade band: still paler than its "
                                                "wall (crypt.shrine has a shallow shade), but the niche and the five "
                                                "marks lose contrast; facing +row / +col it is seen from behind"},
                              where="the Flame Niche (20,35): against the room's far wall, facing the door",
                              note="pivot on the middle one of three wall tiles (front −Y); it projects 0.9 m over the "
                                   "floor row in front (the two steps: walk-over, 0.31 m). Sockets niche (the empty "
                                   "flame's centre: the S7 turn-in beat and prop.pit_cleanse.niche), turn_in (where "
                                   "the hero stands), mark_1..5 (the burn marks, clockwise from the top)"),
    "SM_Pit_AshDrift_A": dict(ids=["setpiece_pit_ash_drift"], fp=[1, 1], blocking=False, blob=0.01,
                              where="the Ash Steps (5,5) and wherever ash has drifted", note="walk-over ground dressing"),
    "SM_Pit_AshDrift_B": dict(ids=["setpiece_pit_ash_drift"], fp=[1, 1], blocking=False, blob=0.01,
                              ink_ignore=("drift",),
                              where="the Ash Steps (5,5) and wherever ash has drifted",
                              note="variant B: rubble and a half-buried kettle helm"),
    "SM_Prop_March_PitMouthOpen": dict(ids=["setpiece_pit_mouth_open"], fp=[3, 3], blocking=False, blob=0.5,
                                       violet=True, theme="march",
                                       where="the Pit entrance (17,26) of emerald_plains once W3 unseals it "
                                             "(entrance_rf_pit: sealed false)",
                                       note="replaces setpiece_pit_mouth_sealed on the same anchor, yaw and footprint "
                                            "(set_pieces.json pit_mouth_sealed -> gameId setpiece_pit_mouth_open, or "
                                            "fallbackIds); socket entrance = the subDungeonEntrances point, smoke for "
                                            "camp.torch@march's smoulder (no light), ladder_top where the hero "
                                            "climbs in"),
}
ROOMS = {
    "ash_steps": ("the Ash Steps 灰阶 (5,5)", ["SM_Pit_AshSteps", "SM_Pit_AshDrift_A", "SM_Pit_AshDrift_B"]),
    "helm_wall": ("the Helm Wall 盔墙 (24,8)", ["SM_Pit_HelmNiche_A", "SM_Pit_HelmNiche_B", "SM_Pit_WallPier"]),
    "tag_cellar": ("the Tag Cellar 名牌窖 (10,24)", ["SM_Pit_TagCurtain", "SM_Pit_TagRack", "SM_Pit_TagStrand_A",
                                                 "SM_Pit_TagStrand_B", "SM_Pit_TagStrand_C"]),
    "pitwardens_cut": ("the Pitwarden's Cut 掘坑官之室 (28,26)", ["SM_Pit_Wall_B", "SM_Pit_Wall_C", "SM_Pit_NicheLamp"]),
    "flame_niche": ("the Flame Niche 火龛 (20,35)", ["SM_Pit_FlameNiche", "SM_Pit_NicheLamp", "SM_Pit_NicheLamp_Lit"]),
}


def game_ids_of(name: str) -> list[str]:
    sp = SPECS[name]
    return list(sp.get("ids", [])) + theme.themed_ids(sp.get("themed", []), THEME)


def ship_one(name: str, previews: bool = True, verify: bool = True) -> dict:
    """Build (LOD0 + LOD1), check reserved light, export, manifest, previews, ink gate, pads, normals, re-import."""
    from kit import export
    builders = _builders()
    sp = SPECS[name]
    scene.reset()
    pal = Palette(FAM)
    lod_obj = builders[name](pal, 1)
    extra = {"displayName": name.split("_", 2)[-1], "family": FAMILY, "theme": sp.get("theme", "march"),
             "dressing": THEME, "zone": "emerald_plains" if sp.get("theme") else "sub_rf_pit",
             "placement": dict(sp.get("placement") or {}, layer=LAYER, where=sp["where"]), "note": sp["note"]}
    if sp.get("themed"):
        extra["themeOf"] = list(sp["themed"])
        extra["themeNote"] = ("resolves when sub_rf_pit's presentation theme is 'pit' (<id>@pit before <id>); the "
                              "plain ids need no theme (implementation-plan.md 3D notes)")
    slot = EM.PROP_WIND_SLOT if sp.get("wind") else (CRYPT_SLOT if crypt_slot(name) else None)
    if crypt_slot(name):
        crypt_toon(lod_obj)
    if slot:                                                 # the LOD1 FBX carries the LOD0's slot name
        orig = lod_obj.data.materials[0]
        m1 = orig.copy()
        m1.name = slot
        lod_obj.data.materials[0] = m1
        extra["lods"] = [E.export_lod1(lod_obj, name, CAT)]
        lod_obj.data.materials[0] = orig
        bpy.data.materials.remove(m1)
    else:
        extra["lods"] = [E.export_lod1(lod_obj, name, CAT)]
    lod_obj.hide_render = True
    obj = builders[name](pal)
    if crypt_slot(name):
        crypt_toon(obj)
    pal.save()
    if "af_tags" in obj:
        extra["nameTags"] = int(obj["af_tags"])
    pads = SP.pad_report(name)
    print(f"[pad] {name}: largest hull pad {pads['maxMm']:.1f} mm (gate {SP.PAD_MAX_MM:.0f} mm)"
          + ("" if pads["pass"] else "  FAIL " + "; ".join(pads["over"])))
    problems = EM.check_light(obj, pal, allow_violet=sp.get("violet", False))
    problems += EM.check_light(lod_obj, pal, allow_violet=sp.get("violet", False))
    if problems:
        raise RuntimeError(f"{name}: " + "; ".join(problems))
    normals = {f"lod{k}": SP.normal_sanity(o.data) for k, o in ((0, obj), (1, lod_obj))}
    normals["pass"] = all(v["pass"] for v in normals.values())
    print(f"[normals] {name}: " + "  ".join(f"{k} {v['badFrac'] * 100:.1f} % bad, {v['flipped']} flipped"
                                            for k, v in normals.items() if k != "pass")
          + ("" if normals["pass"] else f"  FAIL (gate {SP.NORMAL_BAD_MAX * 100:.0f} %)"))
    sockets = json.loads(obj.get("af_sockets", "[]"))
    extra["budget"] = {"lod0": {"toon": E.tris(obj, 0), "outline": E.tris(obj, 1)},
                       "lod1": dict(extra["lods"][0]["triangles"])}
    if sp.get("warm"):
        extra["light"] = {"reserved": "warm (lantern.* / hearth.ember): the lit lamps of the cleansed Pit are a "
                                      "refuge light (chapter1.md §9.2 cleanse; §15 reserved light)",
                          "sockets": {"flame": "prop.pit_lamp.flame", "light": "the fake light pool (R18)"}}
    kw = {}
    if sp.get("wind"):
        kw.update(wind_slot=EM.PROP_WIND_SLOT, toon_params=EM.PROP_WIND_PARAMS)
    elif crypt_slot(name):
        kw.update(toon_slot=CRYPT_SLOT, toon_params=CRYPT_PARAMS)
        extra["materialNote"] = (f"slot 0 {CRYPT_SLOT}: the palette toon (M_AF_Toon) with RimOverride 1, "
                                 f"RimOverrideColor the wall's own {CRYPT_RIM_HEX} at RimOverrideAlpha "
                                 f"{CRYPT_RIM_ALPHA:g} — no Fresnel rim on the crypt (a key-lit wall plane crossed "
                                 "the rim band whole and turned cream in the left third of the frame) — and "
                                 f"ShadeCool {CRYPT_SHADE_COOL:g} (the master's scalar, default 1: the share of the "
                                 "cool shade tint; a wall plane turned from the key light was indigo, the hue "
                                 "reserved for enemy magic in the Pit)")
    rx = (lambda rv, l0=obj, l1=lod_obj: E.lod_preview(rv, l0, l1))
    review_cls = E.EnvReview
    unlined = ("cords", "nametags") if sp.get("wind") else tuple(sp.get("ink_ignore", ()))
    if unlined and obj["af_outline_class"] != "none" and previews:
        mask = cord_mask(obj, unlined)

        class CordReview(review_cls):                        # the gate measures the frame, not the cords / drift
            def __init__(self, *a, **k):
                super().__init__(*a, **k)
                self.ink_ignore = mask
        E.EnvReview = CordReview
        extra["inkNote"] = (
            "the cords and name-tags carry no hull (1 cm cords): the ink gate measures the frame (rail / posts / "
            "beams) and leaves their edges out" if sp.get("wind") else
            "the ash drift (" + ", ".join(unlined) + ") carries no hull — it is feathered into the floor in three "
            "values, as ground cover — and the ink gate leaves its edges out")
    elif name == "SM_Pit_AshDrift_A":
        extra["inkNote"] = ("outline class none: the asset is one feathered ash drift (three values, no hull), "
                            "ground cover on the flags")
    try:
        res = E.ship(obj, name, CAT, pal, obj["af_outline_class"], game_ids=game_ids_of(name), footprint=sp["fp"],
                     blocking=sp["blocking"], sockets=sockets, extra=extra, previews=previews,
                     blob=sp.get("blob", 0.3), review_extra=rx if previews else None, close_facing="se",
                     facing="se", **kw)
    finally:
        E.EnvReview = review_cls
    res["pad"] = pads
    res["normals"] = normals
    if verify:
        res["verify"] = SP.verify_static(name)
    del export
    return res


def quick(names, tag: str = "pit_quick") -> None:
    """Iteration sheet (scratch): each asset close-up + its 1080p W1 crop with the warrior."""
    scene.reset()
    pal = Palette(FAM)
    builders = _builders()
    objs = [builders[n](pal) for n in names]
    for o, n in zip(objs, names):
        if crypt_slot(n):
            crypt_toon(o)
        print(f"  {n}: toon {E.tris(o, 0)} hull {E.tris(o, 1)}")
    SP.iter_sheet(objs, tag, facing="se")


def main(argv: list[str]) -> int:
    flags = {a for a in argv if a.startswith("--")}
    sel = [a for a in argv if not a.startswith("--")]
    names = [n for n in SPECS if not sel or n in sel or any(s.lower() in n.lower() for s in sel)]
    if "--quick" in flags:
        quick(names, "pit_" + "_".join(n.split("_")[-2][:6] + n.split("_")[-1][:3] for n in names)[:70])
        return 0
    results = {}
    fails = 0
    for n in ([] if "--family" in flags else names):
        res = ship_one(n, previews="--no-previews" not in flags, verify="--no-verify" not in flags)
        results[n] = res
        ok = (res.get("ink") or {}).get("pass", True) and res["pad"]["pass"] and res["normals"]["pass"] and \
            (res.get("verify") or {"pass": True})["pass"]
        fails += not ok
    if "--no-previews" not in flags and (len(names) > 3 or "--family" in flags):
        scene.reset()
        pal = Palette(FAM)
        builders = _builders()
        for tag, group in (("pit_walls", [n for n in SPECS if "Wall" in n or "Niche" in n or "Lamp" in n]),
                           ("pit_cellar_steps", [n for n in SPECS if n not in
                                                 [m for m in SPECS if "Wall" in m or "Niche" in m or "Lamp" in m]])):
            objs = [builders[n](pal) for n in group]
            for o, n in zip(objs, group):
                if crypt_slot(n):
                    crypt_toon(o)
            EM.family_sheet(objs, f"{tag}.png", "THE PIT CRYPT KIT " + tag.upper().replace("_", " "), cols=4)
            for o in objs:
                bpy.data.objects.remove(o, do_unlink=True)
    rc = E.summary()
    return 1 if (fails or rc) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
