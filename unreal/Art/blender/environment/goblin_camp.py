"""Goblin camp dressing at (40,52) — NEW (spec art-inventory-ch1.md §6.9, §12 A13; DECISIONS R7).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/goblin_camp.py [names…]
        [--sheet] [--no-previews]

The explore objective ``zone_goblin_camp`` and the ``cs_ep_whisper`` camera focus target an area the web leaves
empty; this kit dresses it in the goblins' war-banner language (hides ``#7C4F2D`` / ``#6A3B24``, straps
``#4A2F1C``, bone ``#ECE2C6``, war red ``#B8322E``). Placed by the level script, non-blocking (spec §6.9).

| Asset | game ids | look |
|---|---|---|
| ``SM_Prop_Goblin_Hut_A`` | ``goblin_hut`` | dome of bent poles under stitched hide panels, a dark doorway with a tied-back flap, a horned skull over the door, red paint |
| ``SM_Prop_Goblin_Hut_B`` | ``goblin_hut`` | conical pole tent wrapped in hides and straps, poles crossing out of the top, feathers |
| ``SM_Prop_Goblin_FirePit`` | ``goblin_fire_pit`` | sooty stone ring, ash heap with faintly glowing coals ``#FF8A2A``, charred logs, a spit with a bone |
| ``SM_Prop_Goblin_BoneTotem`` | ``goblin_bone_totem`` | crooked pole with a horned beast skull, red-painted hide strips, bone strings, feathers |
| ``SM_Prop_Goblin_SpearRack`` | ``goblin_spear_rack`` | lashed A-frame rack holding four goblin spears (iron tips, cord wraps) and a red rag |
| ``SM_Prop_Goblin_Crates`` | ``goblin_crates`` | two crude lashed crates + a loot sack, a red claw mark |
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit as E  # noqa: E402
import foliage as F  # noqa: E402
import story as ST  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


REGIONS = dict(
    hide=Region("#7C4F2D", .42, .32),
    hide_dk=Region("#6A3B24", .42, .30),
    hide_lt=Region("#9A6A42", .40, .30),
    stitch=Region("#E0C890", .35, .30),
    strap=Region("#4A2F1C", .42, .28),
    pole=Region("#6E4A30", .42, .30),
    bone=Region("#ECE2C6", .40, .30),
    bone_dk=Region("#3A2A24", .40, .20),
    red=Region("#B8322E", .42, .35),
    inside=Region("#1E1418", .30, .10),
    feather=Region("#2A2430", .40, .25),
    feather_red=Region("#B8322E", .42, .35),
    soot=Region("#5A5458", .42, .30),
    ash=Region("#7A7270", .40, .25),
    char=Region("#3A2A24", .40, .20),
    ember=Region("#FF8A2A", .0, .0, e=0.55),
    ember_hot=Region("#FFB060", .0, .0, e=0.7),
    iron=Region("#5A5E6A", .42, .45),
    cord=Region("#C8A870", .38, .30),
    plank=Region("#7E5E3C", .42, .30),
    plank_line=Region("#7E5E3C").shade_region(.30, .12),
    batten=Region("#5A3E26", .42, .28),
    sack=Region("#B89A6A", .42, .32),
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def new(pal, name):
    b = M.Builder(name, pal, prefix="gobcamp")
    b.regions(**REGIONS)
    return b, scene.rng(name)


def horned_skull(b, c: Vector, s: float, yaw: float = 0.0):
    sid = ST.skull(b, c, s, yaw=yaw, region="bone", dark="bone_dk", name="beast_skull")
    for side in (-1, 1):
        h = M.horn(s * 0.32, s * 1.6, bend_deg=70, sides=7)
        h.transform(_rz(yaw) @ Matrix.Translation((side * s * 0.75, 0, s * 0.45)) @ _rz(side * -70) @ _rx(15))
        h.translate(c)
        b.add(h, "bone", name="horn")
    return sid


def dome(b, rnd, R: float, H: float, rows: int = 7, cols: int = 20, door_az: float = -math.pi / 2,
         door_w: float = 0.55, region="hide"):
    """A hide-covered dome shell with lumpy panels; the doorway sector is left open (the dark interior shows)."""
    bm = bmesh.new()
    sub = bm.faces.layers.int.new("af_sub")
    ph = [rnd.uniform(0, math.tau) for _ in range(3)]
    grid = []
    for j in range(rows + 1):
        t = j / rows
        el = t * math.pi / 2
        row = []
        for i in range(cols):
            az = math.tau * i / cols
            bump = 1 + 0.04 * math.sin(az * 5 + ph[0]) * math.sin(el * 3) + 0.03 * math.sin(az * 9 + ph[1])
            r = R * math.cos(el) * bump
            z = H * math.sin(el) ** 0.9
            row.append(bm.verts.new((math.cos(az) * r, math.sin(az) * r, z - 0.03 * (j == 0))))
        grid.append(row)
    top = bm.verts.new((0, 0, H))

    def in_door(i, j):
        az = math.tau * (i + 0.5) / cols
        d = math.atan2(math.sin(az - door_az), math.cos(az - door_az))
        return abs(d) < door_w / R and j < rows * 0.55
    for j in range(rows):
        for i in range(cols):
            if in_door(i, j):
                continue
            k = (i + 1) % cols
            f = bm.faces.new((grid[j][i], grid[j][k], grid[j + 1][k], grid[j + 1][i]))
    for i in range(cols):
        bm.faces.new((grid[rows][i], grid[rows][(i + 1) % cols], top))
    bm.normal_update()
    bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.04)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def build_hut_a(pal: Palette):
    name = "SM_Prop_Goblin_Hut_A"
    b, rnd = new(pal, name)
    R, H = 1.05, 1.75
    door_az = -math.pi / 2
    shell = dome(b, rnd, R, H, door_az=door_az)
    # ink from a closed smooth proxy: the solidified shell's open door edges and lip split its own hull
    proxy = M.lathe([((R + 0.01) * math.cos(el), H * math.sin(el) ** 0.9 + 0.01 * math.sin(el) - 0.03)
                     for el in (k / 8 * math.pi / 2 for k in range(9))][:-1] + [(0.02, H + 0.01)], sides=24,
                    cap0="ngon", cap1="fan")
    sid = b.add(shell, "hide", name="shell", hull=proxy, hull_pad=True)
    # dark interior disc + back wall so the doorway reads
    b.add(M.cylinder(R * 0.92, R * 0.92, 0.0, 0.02, sides=18), "inside", name="floor", outline=False)
    inner = M.ellipsoid((R * 0.9, R * 0.9, H * 0.92), (0, 0, 0), sides=16, rings=8)
    for v in inner.bm.verts:
        v.co.z = max(v.co.z, 0.0)
    b.add(inner, "inside", name="interior", outline=False)
    # hide panels in two tones + stitch rows along panel seams
    for k in range(6):
        az = math.tau * k / 6 + 0.3
        if abs(math.atan2(math.sin(az - door_az), math.cos(az - door_az))) < 0.6:
            continue
        pts, nrm = [], []
        for j in range(6):
            el = 0.12 + j * 0.2
            r = R * math.cos(el) + 0.022
            p = Vector((math.cos(az) * r, math.sin(az) * r, H * math.sin(el) ** 0.9))
            pts.append(p)
            nrm.append(Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el))).normalized())
        b.add(E.ribbon(pts, nrm, 0.022, lift=0.004, thick=0.004), "stitch", name="stitch", outline=False,
              covered_by=sid)
    for k in range(3):
        az = math.tau * k / 3 + 1.4
        el0 = 0.3 + 0.15 * k
        pts = []
        for i in range(4):
            a = az + i * 0.16
            el = el0 + 0.05 * math.sin(i)
            pts.append(Vector((math.cos(a) * (R * math.cos(el) + 0.03), math.sin(a) * (R * math.cos(el) + 0.03),
                               H * math.sin(el) ** 0.9)))
        nrm = [p.normalized() for p in pts]
        b.add(E.ribbon(pts, nrm, 0.3, lift=0.0, thick=0.03), "hide_dk", name="patch", hull_pad=False)
    # poles arching over the dome and crossing out of the top
    for k in range(4):
        az = math.pi / 4 + k * math.pi / 2
        pts = []
        for j in range(7):
            el = j / 6 * math.pi / 2
            r = (R + 0.05) * math.cos(el)
            pts.append(Vector((math.cos(az) * r, math.sin(az) * r, (H + 0.04) * math.sin(el) ** 0.9)))
        pts.append(Vector((-math.cos(az) * 0.12, -math.sin(az) * 0.12, H + 0.32)))
        b.add(M.tube(pts, [0.04] * len(pts), sides=6), "pole", name="pole")
    # doorway: frame poles + tied-back flap + horned skull above
    dx, dy = math.cos(door_az), math.sin(door_az)
    for s in (-1, 1):
        a = door_az + s * (0.55 / R)
        p0 = Vector((math.cos(a) * R, math.sin(a) * R, -0.02))
        b.add(M.tube([p0, p0 + Vector((0, 0, 0.98))], [0.045, 0.04], sides=6), "pole", name="door_pole")
    flap = M.plate([(0.0, 0.0), (0.42, 0.0), (0.34, -0.78), (0.1, -0.9), (0.0, -0.7)], 0.03, bevel=0.006)
    flap.transform(_rx(90) @ _rz(0)).translate((0.38, -R - 0.06, 1.0))
    flap.transform(_rz(-18))
    b.add(flap, "hide_lt", name="flap", hull_pad=True)
    horned_skull(b, Vector((dx * (R - 0.05), dy * (R - 0.05) - 0.06, 1.18)), 0.12, yaw=0.0)
    # red war paint: claw stripes beside the door
    for q in range(3):
        a = door_az - 0.95 + q * 0.09
        el0 = 0.35
        pts = []
        for i in range(3):
            el = el0 + i * 0.15
            pts.append(Vector((math.cos(a) * (R * math.cos(el) + 0.026), math.sin(a) * (R * math.cos(el) + 0.026),
                               H * math.sin(el) ** 0.9)))
        b.add(E.ribbon(pts, [p.normalized() for p in pts], 0.05, lift=0.004, thick=0.003), "red", name="paint",
              outline=False, covered_by=sid)
    # feathers on the top
    for q in range(3):
        b.add(E.blade(Vector((0.0, 0.0, H + 0.28)), (math.cos(q * 2.1) * 0.6, math.sin(q * 2.1) * 0.6, 0.7), 0.28,
                      0.07, bend=0.3), "feather_red" if q == 1 else "feather", name="feather")
    F.tuft(b, Vector((-1.15, -0.55, 0)), 0.26, 8, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=1.3, name=name)


def build_hut_b(pal: Palette):
    name = "SM_Prop_Goblin_Hut_B"
    b, rnd = new(pal, name)
    R, H = 0.95, 2.05
    # conical hide wrap with an open door wedge at the front
    rings = []
    cols = 18
    door = (-math.pi / 2, 0.5)
    bm = bmesh.new()
    grid = []
    for j in range(6):
        t = j / 5
        row = []
        for i in range(cols):
            az = math.tau * i / cols
            r = R * (1 - t) * (1 + 0.03 * math.sin(az * 7 + j))
            row.append(bm.verts.new((math.cos(az) * r, math.sin(az) * r, H * 0.86 * t - 0.03 * (j == 0))))
        grid.append(row)
    for j in range(5):
        for i in range(cols):
            az = math.tau * (i + 0.5) / cols
            dd = abs(math.atan2(math.sin(az - door[0]), math.cos(az - door[0])))
            if dd < door[1] * (1 - j / 5) and j < 3:
                continue
            k = (i + 1) % cols
            bm.faces.new((grid[j][i], grid[j][k], grid[j + 1][k], grid[j + 1][i]))
    bm.normal_update()
    bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.035)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    shell = M.Part(bm).smooth(True)
    proxy = M.loft_z([(-0.03, R + 0.01, R + 0.01), (H * 0.43, R * 0.5 + 0.01, R * 0.5 + 0.01),
                      (H * 0.86 + 0.02, 0.05, 0.05)], sides=20, cap0="ngon", cap1="fan")
    sid = b.add(shell, "hide_dk", name="cone", hull=proxy, hull_pad=True)
    inner = M.loft_z([(0.0, R * 0.85, R * 0.85), (H * 0.7, 0.08, 0.08)], sides=14, cap0="ngon", cap1="fan")
    b.add(inner, "inside", name="interior", outline=False)
    # poles crossing out of the top
    for k in range(6):
        az = math.tau * k / 6 + 0.2
        p0 = Vector((math.cos(az) * (R + 0.04), math.sin(az) * (R + 0.04), -0.02))
        p1 = Vector((-math.cos(az) * 0.22, -math.sin(az) * 0.22, H + 0.25))
        b.add(M.tube([p0, p0.lerp(p1, 0.5), p1], [0.04, 0.035, 0.028], sides=6), "pole", name="pole")
    # straps round the cone + a lighter hide band
    for z in (0.55, 1.15):
        r = R * (1 - z / (H * 0.86)) + 0.03
        pts = [Vector((math.cos(a) * r, math.sin(a) * r, z)) for a in (math.tau * i / 16 for i in range(17))]
        nrm = [Vector((math.cos(a), math.sin(a), 0.4)).normalized() for a in (math.tau * i / 16 for i in range(17))]
        keep = [i for i, a in enumerate(math.tau * i / 16 for i in range(17))
                if abs(math.atan2(math.sin(a - door[0]), math.cos(a - door[0]))) > 0.55 or z > 1.0]
        segs = [[]]
        for i in range(17):
            if i in keep:
                segs[-1].append(i)
            elif segs[-1]:
                segs.append([])
        for sg in segs:
            if len(sg) >= 3:
                b.add(E.ribbon([pts[i] for i in sg], [nrm[i] for i in sg], 0.06, lift=0.006, thick=0.008),
                      "strap", name="strap", outline=False, covered_by=sid)
    # red painted eye/mask motif on the side
    az = 0.5
    c = Vector((math.cos(az) * (R * 0.55 + 0.04), math.sin(az) * (R * 0.55 + 0.04), 0.95))
    n = Vector((math.cos(az), math.sin(az), 0.45)).normalized()
    side = n.cross(Vector((0, 0, 1))).normalized()
    for sgn in (-1, 1):
        pts = [c + side * sgn * 0.06, c + side * sgn * 0.17 + Vector((0, 0, 0.05)), c + side * sgn * 0.26]
        b.add(E.ribbon(pts, [n] * 3, 0.05, lift=0.004, thick=0.003), "red", name="paint", outline=False,
              covered_by=sid)
    b.add(E.ribbon([c - Vector((0, 0, 0.05)), c - Vector((0, 0, 0.2)), c - Vector((0, 0, 0.32))], [n] * 3, 0.045,
                   lift=0.004, thick=0.003), "red", name="paint", outline=False, covered_by=sid)
    for q in range(2):
        b.add(E.blade(Vector((math.cos(q * 3) * 0.1, math.sin(q * 3) * 0.1, H + 0.2)),
                      (math.cos(q * 3), math.sin(q * 3), 0.9), 0.3, 0.07, bend=0.3),
              "feather" if q else "feather_red", name="feather")
    # bone string at the door
    for q in range(4):
        b.add(M.ellipsoid((0.03, 0.03, 0.04), (0.42, -0.62, 0.95 - q * 0.09), sides=7, rings=4),
              "bone" if q % 2 == 0 else "bone_dk", name="bead")
    F.tuft(b, Vector((0.95, -0.4, 0)), 0.24, 7, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=1.4, name=name)


def build_firepit(pal: Palette):
    name = "SM_Prop_Goblin_FirePit"
    b, rnd = new(pal, name)
    b.add(M.cylinder(0.42, 0.38, -0.02, 0.03, sides=16), "ash", name="ash_bed")
    heap = M.ellipsoid((0.26, 0.24, 0.07), (0, 0, 0.03), sides=12, rings=5)
    b.add(heap, "ash", name="ash")
    for q in range(10):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0, 0.17)
        s = rnd.uniform(0.025, 0.04)
        zt = 0.03 + 0.07 * math.sqrt(max(0.0, 1 - (rr / 0.26) ** 2))
        b.add(M.ellipsoid((s, s * 0.8, s * 0.5), (math.cos(a) * rr, math.sin(a) * rr, zt + 0.004), sides=6,
                          rings=3), "ember" if q % 3 else "ember_hot", name="coal", outline=False)
    for q in range(3):
        a = q * 2.1 + 0.4
        p0 = Vector((math.cos(a) * 0.38, math.sin(a) * 0.38, 0.05))
        p1 = Vector((math.cos(a + 2.6) * 0.1, math.sin(a + 2.6) * 0.1, 0.1))
        b.add(M.tube([p0, p0.lerp(p1, 0.5), p1], [0.05, 0.05, 0.035], sides=7), "char", name="log", hull_pad=True)
    for i in range(9):
        a = math.tau * i / 9 + rnd.uniform(-0.1, 0.1)
        c = Vector((math.cos(a) * 0.5, math.sin(a) * 0.5, 0))
        r = (rnd.uniform(0.11, 0.15), rnd.uniform(0.09, 0.12), rnd.uniform(0.08, 0.12))
        shape = E.RockShape(r, (name, i), bumps=4, amp=0.1, top_cut=0.25)
        b.add(E.rock(shape, subdiv=1, facet=24.0, yaw=math.degrees(a)).translate(c), "soot", name="stone",
              hull_pad=True)
    # spit: two forked sticks + a crossbar with a roasting bone
    for s in (-1, 1):
        p0 = Vector((s * 0.62, 0.05, -0.02))
        b.add(M.tube([p0, p0 + Vector((0, 0, 0.62))], [0.03, 0.025], sides=6), "pole", name="spit_post")
        for f in (-1, 1):
            b.add(M.tube([p0 + Vector((0, 0, 0.58)), p0 + Vector((f * 0.06 * -s, 0, 0.72))], [0.02, 0.014], sides=5),
                  "pole", name="fork")
    b.add(M.tube([Vector((-0.72, 0.05, 0.62)), Vector((0.72, 0.05, 0.62))], [0.018, 0.018], sides=6), "pole",
          name="spit")
    ST.bone(b, Vector((-0.12, 0.05, 0.6)), Vector((0.14, 0.05, 0.6)), 0.03)
    b.add(M.ellipsoid((0.11, 0.07, 0.07), (0.0, 0.05, 0.6), sides=10, rings=5), "red", name="meat")
    ST.bone(b, Vector((0.55, -0.55, 0.02)), Vector((0.75, -0.4, 0.02)), 0.022)
    return E.finish(b, pal, "decor", ground_h=0.4, name=name)


def build_bonetotem(pal: Palette):
    name = "SM_Prop_Goblin_BoneTotem"
    b, rnd = new(pal, name)
    H = 1.55
    pts = [Vector((0, 0, -0.05)), Vector((0.03, 0.0, 0.6)), Vector((-0.03, 0.01, 1.1)), Vector((0.02, 0.0, H))]
    pid = b.add(M.tube(pts, [0.06, 0.055, 0.05, 0.045], sides=8), "pole", name="pole", hull_pad=True)
    horned_skull(b, Vector((0.02, -0.03, H + 0.08)), 0.15)
    for z in (0.95, 1.02, 1.38):
        b.add(M.belt(0.055, 0.055, z, 0.035, 0.012, sides=10), "cord", name="lash", outline=False, covered_by=pid)
    # red-painted hide strips hanging from a short crossbar
    b.add(M.tube([Vector((-0.3, -0.04, 1.3)), Vector((0.3, -0.04, 1.32))], [0.03, 0.03], sides=6), "pole",
          name="bar")
    for q, x in enumerate((-0.22, -0.07, 0.08, 0.23)):
        L = rnd.uniform(0.4, 0.6)
        st = M.plate([(-0.05, 0.0), (0.05, 0.0), (0.04, -L), (0.0, -L - 0.06), (-0.04, -L)], 0.014, bevel=0.003)
        st.transform(_rx(90)).translate((x, -0.06, 1.3))
        b.add(st, "red" if q % 2 == 0 else "hide", name="strip", hull_pad=True)
    for side in (-1, 1):
        b.add(M.tube([Vector((side * 0.3, -0.04, 1.3)), Vector((side * 0.32, -0.04, 1.05))], [0.007, 0.007], sides=5),
              "cord", name="string")
        for q in range(3):
            b.add(M.ellipsoid((0.028, 0.028, 0.034), (side * 0.32, -0.04, 1.02 - q * 0.075), sides=7, rings=4),
                  "bone" if q % 2 == 0 else "bone_dk", name="bead")
    for q in range(3):
        b.add(E.blade(Vector((0.0, 0.0, 1.05)), (math.cos(q * 2.0 + 1) * 0.7, math.sin(q * 2.0 + 1) * 0.7, -0.6), 0.24,
                      0.06, bend=0.1), "feather" if q % 2 else "feather_red", name="feather")
    for q, (x, y, r) in enumerate(((-0.13, -0.05, 0.1), (0.12, -0.07, 0.09), (0.0, 0.13, 0.1))):
        shape = E.RockShape((r, r * 0.9, r * 0.8), (name, q), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=26.0).translate((x, y, 0)), "soot", name="stone", hull_pad=True)
    ST.bone(b, Vector((0.25, -0.25, 0.02)), Vector((0.42, -0.12, 0.02)), 0.02)
    return E.finish(b, pal, "decor", ground_h=1.0, name=name)


def goblin_spear(b, base: Vector, top: Vector, rnd):
    d = (top - base).normalized()
    shaft = [base, base.lerp(top, 0.5) + Vector((rnd.uniform(-0.01, 0.01), rnd.uniform(-0.01, 0.01), 0)), top]
    b.add(M.tube(shaft, [0.017, 0.016, 0.014], sides=6), "pole", name="spear_shaft")
    # cord wrap under the head + iron leaf tip
    b.add(M.tube([top - d * 0.08, top], [0.02, 0.02], sides=6), "cord", name="spear_wrap")
    tip = M.gem(0.035, 0.2, 0.03, sides=4)
    tip.scale((1.0, 0.35, 1.0))
    tip.transform(Vector((0, 0, 1)).rotation_difference(d).to_matrix().to_4x4())
    tip.translate(top + d * 0.01)
    b.add(tip, "iron", name="spear_tip")


def build_spearrack(pal: Palette):
    name = "SM_Prop_Goblin_SpearRack"
    b, rnd = new(pal, name)
    W = 0.6
    for s in (-1, 1):
        for f in (-1, 1):
            p0 = Vector((s * W, f * 0.28, -0.02))
            p1 = Vector((s * W * 0.95, -f * 0.05, 1.05))
            b.add(M.tube([p0, p1], [0.035, 0.03], sides=6), "pole", name="leg")
        b.add(M.belt(0.04, 0.04, 0.0, 0.04, 0.012, sides=8).translate((s * W * 0.96, 0.0, 0.92)), "cord",
              name="lash", outline=False)
    b.add(M.tube([Vector((-W - 0.08, 0.0, 0.95)), Vector((W + 0.08, 0.0, 0.95))], [0.035, 0.035], sides=7), "pole",
          name="bar")
    b.add(M.tube([Vector((-W, 0.22, 0.25)), Vector((W, 0.22, 0.25))], [0.03, 0.03], sides=6), "pole", name="rail")
    for q in range(4):
        x = -0.42 + q * 0.28
        base = Vector((x + rnd.uniform(-0.03, 0.03), 0.28, 0.0))
        top = Vector((x + rnd.uniform(-0.06, 0.06), -0.12, 1.72 + rnd.uniform(-0.08, 0.06)))
        goblin_spear(b, base, top, rnd)
    rag = M.plate([(0.0, 0.0), (0.16, 0.0), (0.13, -0.32), (0.07, -0.26), (0.02, -0.36)], 0.014, bevel=0.003)
    rag.transform(_rx(90)).translate((0.35, -0.05, 0.94))
    b.add(rag, "red", name="rag", hull_pad=True)
    horned_skull(b, Vector((0.0, -0.06, 1.06)), 0.08)
    return E.finish(b, pal, "decor", ground_h=1.0, name=name)


def crude_crate(b, c: Vector, s: float, yaw: float, rnd, name="crate"):
    bx = M.box((s, s * 0.9, s * 0.8), (0, 0, 0), bevel=0.012)
    bx.transform(_rz(yaw)).translate(c + Vector((0, 0, s * 0.4)))
    cid = b.add(bx, "plank", name=name, hull_pad=True)
    rot = _rz(yaw)
    for f in (-1, 1):
        n = (rot.to_3x3() @ Vector((0, f, 0))).normalized()
        for z in (s * 0.27, s * 0.54):
            p0 = rot.to_3x3() @ Vector((-s / 2 + 0.03, f * s * 0.45, z)) + c
            p1 = rot.to_3x3() @ Vector((s / 2 - 0.03, f * s * 0.45, z)) + c
            b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [n] * 3, 0.012, lift=0.002, thick=0.002), "plank_line",
                  name="seam", outline=False, covered_by=cid)
    # dark corner battens (crude, a little proud)
    for sx in (-1, 1):
        for sy in (-1, 1):
            bt = M.box((0.05, 0.05, s * 0.82), (sx * (s / 2 - 0.02), sy * (s * 0.45 - 0.02), s * 0.4), bevel=0.008)
            bt.transform(_rz(yaw)).translate(c)
            b.add(bt, "batten", name="batten")
    # rope lashing round the middle
    band = M.belt(s / 2 + 0.002, s * 0.45 + 0.002, s * 0.36, 0.05, 0.012, sides=16, exp=8)
    band.transform(_rz(yaw)).translate(c)
    b.add(band, "cord", name="rope", outline=False, covered_by=cid)
    return cid


def build_crates(pal: Palette):
    name = "SM_Prop_Goblin_Crates"
    b, rnd = new(pal, name)
    c1 = crude_crate(b, Vector((-0.18, 0.05, 0)), 0.6, 12, rnd)
    crude_crate(b, Vector((-0.12, 0.08, 0.48)), 0.42, -18, rnd, name="crate2")
    # red claw mark on the front of the big crate
    for q in range(3):
        x = -0.3 + q * 0.08
        p0 = Vector((x, -0.235, 0.36))
        p1 = Vector((x + 0.06, -0.235, 0.12))
        n = Vector((0, -1, 0))
        b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [n] * 3, [0.03, 0.035, 0.015], lift=0.004, thick=0.003), "red",
              name="claw", outline=False, covered_by=c1)
    # loot sack slumped against the crates
    sack = M.ellipsoid((0.24, 0.2, 0.26), (0.36, -0.12, 0.24), sides=12, rings=7)
    for v in sack.bm.verts:
        v.co.z = max(v.co.z, 0.0)
        if v.co.z > 0.38:
            v.co.x += (v.co.z - 0.38) * 0.4
    sid = b.add(sack, "sack", name="sack", hull_pad=True)
    neck = M.tube([Vector((0.4, -0.12, 0.44)), Vector((0.47, -0.13, 0.56))], [0.07, 0.05], sides=8)
    b.add(neck, "sack", name="neck")
    b.add(M.belt(0.065, 0.065, 0.0, 0.03, 0.012, sides=10).transform(_ry(20)).translate((0.42, -0.12, 0.47)), "cord",
          name="tie", outline=False)
    ST.bone(b, Vector((0.2, -0.45, 0.02)), Vector((0.42, -0.5, 0.02)), 0.02)
    F.tuft(b, Vector((-0.55, -0.3, 0)), 0.22, 7, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.6, name=name)


ASSETS: dict = {
    "SM_Prop_Goblin_Hut_A": (build_hut_a, dict(game_ids=["goblin_hut"], footprint=[2, 2], blocking=True, blob=1.2)),
    "SM_Prop_Goblin_Hut_B": (build_hut_b, dict(game_ids=["goblin_hut"], footprint=[2, 2], blocking=True, blob=1.1)),
    "SM_Prop_Goblin_FirePit": (build_firepit, dict(game_ids=["goblin_fire_pit"], footprint=[1, 1], blocking=False,
                                                   blob=0.65)),
    "SM_Prop_Goblin_BoneTotem": (build_bonetotem, dict(game_ids=["goblin_bone_totem"], footprint=[1, 1],
                                                       blocking=True, blob=0.3)),
    "SM_Prop_Goblin_SpearRack": (build_spearrack, dict(game_ids=["goblin_spear_rack"], footprint=[1, 1],
                                                       blocking=True, blob=0.6)),
    "SM_Prop_Goblin_Crates": (build_crates, dict(game_ids=["goblin_crates"], footprint=[1, 1], blocking=True,
                                                 blob=0.55)),
}
NOTE = ("goblin camp dressing at (40,52) (spec 6.9 NEW, DECISIONS R7): placed by the level script round the "
        "explore objective zone_goblin_camp / cs_ep_whisper focus. Blocking follows DECISIONS W5 (tall decorations "
        "block: huts 2x2, totem, spear rack, crates) over spec 6.9's 'non-blocking' note; the fire pit stays walkable")


def main(argv: list[str]) -> int:
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or list(ASSETS)
    names = [n for n in ASSETS if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(E.PROPS)
    if sheet:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        E.sheet(objs, "sheet_goblin_" + "_".join(n.split("_")[-1][:6] for n in names)[:80], cols=3)
        return 0
    shipped = []
    for n in (names if "--family" not in argv else []):
        scene.reset()
        pal = Palette(E.PROPS)
        fn, kw = ASSETS[n]
        obj = fn(pal)
        pal.save()
        extra = {"displayName": n.split("_", 2)[-1], "family": "environment.goblin_camp", "note": NOTE}
        E.ship(obj, n, CAT, pal, obj["af_outline_class"], extra=extra, previews=previews, **kw)
        shipped.append(n)
    if "--family" in argv:
        shipped = names
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(E.PROPS)
        objs = [ASSETS[n][0](pal) for n in shipped]
        E.family_sheet(objs, "goblin_camp.png", "GOBLIN CAMP DRESSING (SPEC 6.9, R7)", cols=4, focus_z=0.8,
                       cell_px=(300, 320))
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
