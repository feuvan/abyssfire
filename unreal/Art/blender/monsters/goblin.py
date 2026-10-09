"""哥布林 Goblin — ``SK_Mon_Goblin`` (spec art-inventory-ch1.md §4.2, §4.5, §10; DECISIONS R2, R12, M5, A14).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/goblin.py [--quick] [--no-previews]

Hunched, big-headed raider (0.92 m): swept-back ears, hooked nose, toothy grin, bone-tooth necklace, rope belt,
sash, ragged loincloth and a crude knapped-stone spear (``SM_Mon_Goblin_Spear`` on ``weapon_r``). Clips: Idle 667,
Walk 600 (1.65 m/s, no slide), Attack01 thrust 500 (WindupEnd 155, Contact 250), Hurt 200, Death 500, Stun 600.
The quest-hunt leaders (§4.5) are this mesh at scale 1.25 with their accents as attachments:
``SM_Mon_Goblin_Att_RedCap`` (红帽格鲁克 Gruk Redcap) and ``SM_Mon_Goblin_Att_LootSack`` + ``_Att_Pendant``
(小贼斯尼克 Sneek the Thief) — manifest ``variants``.

Web source: ``src/graphics/sprites/monsters/Goblin.ts`` (palette :45-57, spear :284-294, proportions :296-302,
READY :328-338, walk :347), ``rig/MonsterKit.ts`` (thrust track).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "heroes"))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, V  # noqa: E402
import goblins as GB  # noqa: E402
import mgeo as G  # noqa: E402
from mclips import GoblinAnims  # noqa: E402

ASSET = "SK_Mon_Goblin"
TOKEN = "Mon_Goblin"
SPEAR = "SM_Mon_Goblin_Spear"
PREFIX = "goblin"
WEB = GB.Web(U=0.0216)
LOOK = GB.Look()
OUTLINE_CLASS = "monster"
REF_SPEED = 1.65
WALK = dict(cycle=600.0, duty=0.40, lift=0.075, bob=0.018, lean=4.0, arm=0.07, crouch=-0.012)

SPEAR_REGIONS = outline.ink_regions(dict(
    wood=Region("#7B5330", .42, .30),
    wood_dark=Region("#7B5330", .42, .30).shade_region(.30, .12),
    rope=Region("#B89A62"),
    rope_line=Region("#B89A62").shade_region(.30, .12),
    stone=Region("#9A948A", .42, .45),
    stone_dark=Region("#9A948A", .42, .45).shade_region(.30, .15),
), OUTLINE_CLASS)


def ready(A: GoblinAnims) -> BP:
    """Web READY (Goblin.ts :328-338): crouched, hunched (the rest lean), spear levelled forward at 69°."""
    wd, wu = C.blade(70.0, lat=-0.10)
    return BP(
        pelvis=A.web_pelvis(47.0, 74.0), hip_yaw=-6.0, twist=4.0,
        foot_r=A.web_foot(53.0, -0.072), foot_l=A.web_foot(42.5, 0.078),
        foot_r_rot=(0.0, 0.0, 12.0), foot_l_rot=(0.0, 0.0, 24.0),
        knee_r=V(-0.45, 1, 0), knee_l=V(0.45, 1, 0),
        hand_r=A.web_hand(55.0, 70.0, -0.085), elbow_r=V(-0.8, 0.5, -0.2),
        wpn=wd, wpn_up=wu,
        hand_l=A.web_hand(51.5, 70.5, 0.060), elbow_l=V(0.9, 0.4, -0.2), wrist_l=(10.0, 0.0, 0.0),
        flow=0.1,
    )


# ── spear ──────────────────────────────────────────────────────────────────────────────────────────────
def build_spear(pal: Palette) -> bpy.types.Object:
    """Crude spear (web spear :284-294, ≈ 60 cm): gnarled shaft from 9 u behind the grip to 12 u ahead, rope
    binding, knapped flint head to 18.4 u. Grip at the origin, tip along +Y (weapon_r socket)."""
    u = WEB.U
    b = M.Builder(SPEAR, pal, prefix="goblin_spear")
    b.regions(**SPEAR_REGIONS)
    rng = scene.rng(SPEAR, "shaft")
    ys = np.linspace(-9.0, 12.6, 9)
    pts = [Vector((u * 0.18 * math.sin(y * 0.55) + u * rng.uniform(-0.08, 0.08), y * u,
                   u * 0.15 * math.cos(y * 0.4))) for y in ys]
    rad = [u * (0.62 if y < -8.0 else 0.72 - 0.06 * (y + 9) / 21.6) for y in ys]
    shaft = M.tube(pts, rad, sides=8, up=(0, 0, 1), dome0=u * 0.35, dome1=0.0)
    sid = b.add(shaft, "wood", Bind.rigid("root"), name="shaft", hull_pad=True)
    for y in (-3.5, 4.5):                                             # knots
        k = M.ellipsoid((u * 0.55, u * 0.6, u * 0.5), (u * 0.62, y * u, 0.0), sides=8, rings=6)
        b.add(k, "wood_dark", Bind.rigid("root"), name="knot", outline=False, covered_by=sid)
    # flint head: a faceted leaf (flat-shaded diamond section, jittered knapping along the edges)
    head = []
    L0, L1 = 11.2 * u, 18.6 * u
    for i in range(9):
        t = i / 8
        y = L0 + (L1 - L0) * t
        w = 1.85 * u * math.sin(math.pi * (0.18 + 0.82 * t) ** 0.85) * (1 - t) ** 0.35 + 0.08 * u
        jit = 0.18 * u * math.sin(i * 2.7)
        head.append((y, w + jit, 0.55 * u * (1 - 0.7 * t) + 0.12 * u))
    import bmesh
    bm = bmesh.new()
    rows = []
    for y, w, th in head:
        rows.append([bm.verts.new((w, y, 0.0)), bm.verts.new((0.0, y, th)), bm.verts.new((-w, y, 0.0)),
                     bm.verts.new((0.0, y, -th))])
    for r0, r1 in zip(rows, rows[1:]):
        for j in range(4):
            bm.faces.new((r0[j], r0[(j + 1) % 4], r1[(j + 1) % 4], r1[j]))
    c0 = bm.verts.new((0.0, L0 - 0.6 * u, 0.0))
    c1 = bm.verts.new((0.0, L1 + 0.5 * u, 0.0))
    for j in range(4):
        bm.faces.new((rows[0][(j + 1) % 4], rows[0][j], c0))
        bm.faces.new((rows[-1][j], rows[-1][(j + 1) % 4], c1))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    stone = M.Part(bm).flat()
    hid = b.add(stone, "stone", Bind.rigid("root"), name="flint", hull_pad=True)
    # knapping scars: darker facets painted along the edges (both faces)
    for sgz in (1, -1):
        for i in range(4):
            t = 0.15 + i * 0.2
            y = L0 + (L1 - L0) * t
            sc = M.ellipsoid((0.5 * u, 0.75 * u, 0.12 * u), (0.55 * u * (1 - t) * (1 if i % 2 else -1), y,
                                                             sgz * (0.45 * u * (1 - 0.7 * t) + 0.08 * u)),
                             sides=6, rings=4)
            b.add(sc.flat(), "stone_dark", Bind.rigid("root"), name="scar", outline=False, covered_by=hid)
    # rope binding over the head's tang
    for k in range(5):
        y = (10.6 + k * 0.48) * u
        ring = M.tube([(0, y - 0.22 * u, 0), (0, y + 0.22 * u, 0)], [0.9 * u, 0.9 * u], sides=10, dome0=0.0,
                      dome1=0.0)
        b.add(ring, "rope" if k % 2 == 0 else "rope_line", Bind.rigid("root"), name="binding", outline=False,
              covered_by=sid)
    obj = b.build(SPEAR)
    from kit import shading
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for m in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {SPEAR}: {m}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


# ── body ───────────────────────────────────────────────────────────────────────────────────────────────
def build_body(pal: Palette, kit: GB.GoblinKit) -> bpy.types.Object:
    from kit import asset
    b = M.Builder(ASSET, pal, prefix=PREFIX)
    b.regions(**GB.regions_for(LOOK))
    kit.build_parts(b)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=kit.rig, grounding_height=0.92)
    return body


def build_all():
    scene.reset()
    pal = Palette(GB.FAMILY)
    kit = GB.GoblinKit(ASSET, PREFIX, WEB, LOOK)
    body = build_body(pal, kit)
    spear = build_spear(pal)
    return pal, kit, body, spear


def quick(out: Path) -> None:
    import mship
    pal, kit, body, spear = build_all()
    A = GoblinAnims(kit, REF_SPEED, WALK, ready)
    mship.quick_review(ASSET, kit.rig, [body], [(spear, "weapon_r")], A, out, 0.92, OUTLINE_CLASS)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if "--quick" in args:
        quick(Path(args[args.index("--quick") + 1]) if len(args) > args.index("--quick") + 1 else
              Path("/tmp/af_goblin_quick"))
    else:
        import mship
        import goblin_ship
        goblin_ship.ship(previews="--no-previews" not in args)
