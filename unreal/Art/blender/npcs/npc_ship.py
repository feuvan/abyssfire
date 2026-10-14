"""NPC family ship / review / dev CLI (spec art-inventory-ch1.md §5, §9.5; DECISIONS R2, R11, I7).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/npc_ship.py <npc|all> [flags]
      <npc>              Blacksmith | Merchant | QuestElder | PlainsHerbalist | PlainsWanderer |
                         EscortTravelingMerchant | RescueLostTraveler | WanderingMerchant | StashKeeper (or snake case)
      (no flag)          ship: bake every clip, per-frame QA, FBX (SK + LOD1 + A_ per clip), manifest (merged),
                         re-import verification, the review set in Art/Previews/npc_<id>/ with the ink gate on every
                         game-camera cell, portrait T_UI_Portrait_<Id> (exit 1 on a QA / ink / verify failure)
      --no-previews --no-verify
      --quick [--pose=Clip:frame] [--facing=se]     mesh iteration → scratch
      --anims [Clip …]                              bake + contact sheets → scratch
    npc_ship.py props      SM_NPCKit_Prop_* (Art/Export/NPCs) + Previews/npc_props/props.png
    npc_ship.py dressing   SM_Prop_Camp_AnvilStump (Art/Export/Props) + SM_UI_QuestMarker_* (Art/Export/NPCs)
    npc_ship.py kit        SK_NPCKit_* modular parts (Art/Export/NPCs/Kit) + Previews/npc_kit/kit_parts.png
                [SK_NPCKit_… …]                     export only these parts (the sheets still show every part)
    npc_ship.py lineup     Previews/npc_family/ lineup sheets (all NPCs + the warrior for scale, R2)
"""
from __future__ import annotations

import json
import math
import sys
import time
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "monsters", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import anim, export, mesh as M, outline, paths, pngio, review, rig as R, scene, shading  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.palette import Palette  # noqa: E402

import common as C  # noqa: E402
import hero_base as HB  # noqa: E402
import folk as F  # noqa: E402
import looks as LK  # noqa: E402
import props as P  # noqa: E402
from npc_clips import NpcAnims  # noqa: E402

SCRATCH = scratch_root("npcs")
CATEGORY = "NPCs"
PX_CLASS = outline.SCREEN_PX_1080["npc"]
PX = 3.5            # per-asset hull request (coherence pass): see PX_NOTE
PX_NOTE = ("coherence pass: at the NPC class's 3.0 px the townsfolk kept a rim >= 3 px on only 61-83 % of their "
           "1080p silhouette edges (hat brims, sleeves, fingers, coat hems, the pack; median 3.1-3.5 px) against the "
           "Chapter-1 character gate (dark rim >= 3 px on >= 90 %), while the goblins beside them ship 3.5 px; 3.5 px, "
           "as the heroes, the boss and the goblin family, holds it (gate 3.0 px); held props and kit parts ink with it")


def ink_slot(slot: dict) -> dict:
    """The per-asset ink request on an NPC outline slot (the UE importer makes an ``M_AF_Outline`` instance with this
    ``OutlinePx1080`` when it differs from the class's); used by the NPCs, their held props and the kit parts."""
    slot["outlinePx1080"] = float(PX)
    slot["outlinePxClass"] = float(PX_CLASS)
    slot["outlinePxNote"] = PX_NOTE
    return slot


def ink_gate() -> dict:
    """``review.ink_gate("npc")`` with the rim threshold taken from the shipped request (PX − ``GATE_SLACK_PX``):
    3.0 px, the Chapter-1 character gate."""
    g = dict(review.ink_gate("npc"))
    g["minPx"] = round(max(0.5, PX - outline.GATE_SLACK_PX), 2)
    return g


def prop_kind(npc, kind: str) -> str:
    return "herbs" if (kind == "pouch" and npc.id == "PlainsHerbalist") else kind


# ── build ───────────────────────────────────────────────────────────────────────────────────────────────
def build(npc, lod1: bool = False) -> SimpleNamespace:
    scene.reset()
    pal = Palette(F.FAMILY)
    folk = F.Folk(npc.look, npc.asset, prefix=f"npc.{npc.snake}")
    rig = R.build_humanoid(folk.spec())
    # body sockets (spec §2.4 + the eyes for the portrait / look-at)
    eye = folk.hp(F.EYE_H, 0.0, 1.0)
    R.add_socket(rig, "eyes", "head", eye)
    body = folk.build(pal, rig)
    body1 = build_lod1(folk, pal, rig) if lod1 else None
    items, socks = {}, {}
    for kind, bone in ((npc.look.item, "weapon_r"), (npc.look.off, "weapon_l")):
        if kind == "none":
            continue
        obj, s = P.build_prop(prop_kind(npc, kind), pal)
        items[obj.name] = (obj, bone)
        socks[obj.name] = s
    scenery = []
    for name, off, yaw in npc.scenery:
        obj, s = P.build_anvil(pal)
        scenery.append((obj, Vector(off), yaw, s))
    return SimpleNamespace(npc=npc, pal=pal, folk=folk, rig=rig, body=body, body1=body1, items=items, socks=socks,
                           scenery=scenery)


LOD1_TRIS = 4950          # spec §1.9 NPC LOD1 5 k (toon)


def build_lod1(folk, pal, rig):
    """LOD1 within the §1.9 budget: built at the default ratio, rebuilt once with the ratio scaled to the budget if
    the look is busier (wide brims, packs, long hair)."""
    ratio = folk.LOD1_RATIO
    body1 = folk.build(pal, rig, lod=1)
    n0 = n = M.tri_count(body1, 0)
    for _ in range(8):                  # the exact face / sleeve / hat parts are a fixed cost: iterate on the count
        if n <= LOD1_TRIS:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (LOD1_TRIS - 200) / n
        body1 = folk.build(pal, rig, lod=1, lod_ratio=ratio)
        n = M.tri_count(body1, 0)
    if n != n0:
        print(f"[lod1] {folk.asset}: {n0} toon tris at {folk.LOD1_RATIO} -> {n} at {ratio:.3f}")
    return body1


def attach(B) -> None:
    for name, (obj, bone) in B.items.items():
        R.attach_to_bone(obj, B.rig, bone)
    for obj, off, yaw, _ in B.scenery:
        obj.parent = B.rig.obj
        obj.matrix_parent_inverse = Matrix.Identity(4)
        obj.matrix_basis = Matrix.Translation(off) @ Matrix.Rotation(math.radians(yaw), 4, "Z")


def anims(B) -> NpcAnims:
    A = NpcAnims(B.rig, B.folk, B.npc, {n: o for n, (o, _) in B.items.items()})
    if B.npc.scenery:
        A.ANVIL_AT = Vector(B.npc.scenery[0][1])
    return A


def meshes_of(B, scenery: bool = True) -> list:
    out = [B.body] + [o for o, _ in B.items.values()]
    if scenery:
        out += [o for o, *_ in B.scenery]
    return out


# ── runtime glow cards (previews; the UE module draws them from manifest ``fx``) ────────────────────────
def _clip_time(rig) -> tuple[str | None, float]:
    ad = rig.obj.animation_data
    act = ad.action if ad else None
    if act is None or "af_clip" not in act:
        return None, 0.0
    return json.loads(act["af_clip"])["name"], scene.frame_to_ms(bpy.context.scene.frame_current)


def _world_socket(rig, name: str) -> Vector:
    s = next(x for x in rig.sockets if x["name"] == name)
    b = rig.obj.data.bones[s["bone"]]
    pb = rig.obj.pose.bones[s["bone"]]
    return rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(s["pos"])))


def _card(cam, res, p: Vector, radius_m: float, color: str, alpha: float, core: str | None = None,
          core_frac: float = 0.35, core_alpha: float = 0.0) -> dict:
    px, py = review.project_px(cam, p, res)
    wpp = outline.pixel_world_size(cam, p, res[0])
    return dict(center_px=(px, py), radius_px=radius_m / wpp, color=color, alpha=alpha, core=core,
                core_frac=core_frac, core_alpha=core_alpha)


def coin_fx(t_ms: float, L: float) -> float:
    """Web count track ``fx``: 0 at t0, 1 at the toss top (t .3, ease-out), 0 at the catch (t .6)."""
    t = (t_ms / L) % 1.0
    if t <= 0.3:
        u = t / 0.3
        return 1 - (1 - u) ** 3
    if t <= 0.6:
        u = (t - 0.3) / 0.3
        return 1 - u ** 3
    return 0.0


OCCLUDE_MARGIN = 0.08       # m: geometry this close in front of a card's centre does not hide it (its own hand / prop)
DEPTH_TEST_NOTE = ("runtime card: draw it depth-tested against the characters and props (or fade it by an occlusion "
                   "test, as the previews do: 7 rays to the card's core, hits more than 8 cm in front of it hide "
                   "it) — the body occludes it in the ne / back facings")


def card_visibility(cam, p: Vector, radius_m: float, ignore=()) -> float:
    """Fraction of a glow card's core seen from the camera: rays to the centre and a ring at half the radius (in the
    camera plane); a hit more than ``OCCLUDE_MARGIN`` in front of the card on any mesh but ``ignore`` (the prop that
    carries the glow) hides that sample. The previews fade every card by it — the UE card is depth-tested."""
    dg = bpy.context.evaluated_depsgraph_get()
    sc = bpy.context.scene
    o = cam.matrix_world.translation
    r3 = cam.matrix_world.to_3x3()
    rx, ry = r3 @ Vector((1, 0, 0)), r3 @ Vector((0, 1, 0))
    pts = [p] + [p + (rx * math.cos(a) + ry * math.sin(a)) * (0.5 * radius_m)
                 for a in np.linspace(0.0, 2 * math.pi, 6, endpoint=False)]
    names = {getattr(x, "name", x) for x in ignore}
    vis = 0.0
    for q in pts:
        d = q - o
        L = d.length
        d.normalize()
        start, seen = o.copy(), True
        for _ in range(8):
            reach = L - max(OCCLUDE_MARGIN, 0.5 * radius_m) - (start - o).length
            if reach <= 0.0:
                break
            hit, loc, _n, _i, obj, _m = sc.ray_cast(dg, start, d, distance=reach)
            if not hit:
                break
            if obj is not None and (obj.name in names or obj.original.name in names or obj.type != "MESH"
                                    or obj.name.startswith("AF_")):
                start = loc + d * 1e-4
                continue
            seen = False
            break
        vis += 1.0 if seen else 0.0
    return vis / len(pts)


def glows_for(B):
    npc, rig = B.npc, B.rig
    fx = npc.fx

    def fn(cam, res):
        out = []
        clip, t = _clip_time(rig)

        def add(p, radius_m, color, alpha, core=None, core_frac=0.35, core_alpha=0.0, ignore=()):
            v = card_visibility(cam, p, radius_m, ignore)
            if v > 0.0:
                out.append(_card(cam, res, p, radius_m, color, alpha * v, core, core_frac, core_alpha * v))
        if "orbGlow" in fx:
            g = fx["orbGlow"]
            staff = next(o for o, _ in B.items.values() if o.name == P.PROPS["staff"])
            p = staff.matrix_world @ Vector((0, P.STAFF_UP, 0))
            add(p, g["radiusCm"] / 100, g["color"], g["alpha"], g["coreColor"], g["coreRadiusCm"] / g["radiusCm"],
                g["coreAlpha"], ignore=(staff,))
        if "herbGlow" in fx:
            g = fx["herbGlow"]
            p = _world_socket(rig, "fx_hand_r") + Vector((0, 0, -0.05))
            k = 1.0 + g["pulse"] * math.sin(2 * math.pi * t / g["pulseMs"])
            add(p, g["radiusCm"] / 100 * k, g["color"], g["alpha"],
                ignore=[o for o, _ in B.items.values()])
        if "coinGlint" in fx and clip == "Work_Count":
            g = fx["coinGlint"]
            f = coin_fx(t, npc.work_ms)
            if f > 0.05:
                p = _world_socket(rig, "fx_hand_r") + Vector((0, 0, 0.04 + g["riseCm"] / 100 * f))
                add(p, g["radiusCm"] / 100, g["color"], g["alpha"] * min(1.0, f * 1.6), "#FFF6D0", 0.35, 0.9 * f)
        if "hotBarGlow" in fx:
            g = fx["hotBarGlow"]
            tong = next(o for o, _ in B.items.values() if o.name == g["item"])
            p = tong.matrix_world @ Vector(next(k["pos"] for k in B.socks[tong.name] if k["name"] == g["socket"]))
            add(p, g["radiusCm"] / 100, g["color"], g["alpha"], g["coreColor"], g["coreFrac"], g["coreAlpha"],
                ignore=(tong,))
            s = fx.get("anvilSparks")
            if s and clip == "Work_Hammer":
                dt = t - s["atMs"]
                if 0.0 <= dt <= s["flashMs"]:
                    k = 1.0 - dt / s["flashMs"]
                    add(p + Vector((0, 0, 0.03)), s["flashRadiusCm"] / 100, s["glow"], s["flashAlpha"] * k,
                        s["sparks"], 0.3, 0.9 * k, ignore=(tong,))
        return out
    return fn


# ── manifest pieces ─────────────────────────────────────────────────────────────────────────────────────
def npc_states(npc, clips) -> dict:
    names = [c.name for c in clips]
    st = {"working": f"Work_{npc.work}", "alert": "Alert", "idle": "IdleNPC", "talking": "Talk",
          "idleVariants": ["IdleNPC_Fidget"],
          "note": "web NPC presentation (quests-story-ch1 §6.4): the default loop is `working`; `alert` (one-shot, "
                  "hold the last frame) when the hero is within 3 tiles; `talking` while its dialogue / shop is open; "
                  "IdleNPC_Fidget may replace one working loop now and then (starts / ends on IdleNPC F0)"}
    if "Cower" in names:
        st["threatened"] = "Cower"
    if "Run" in names:
        st["locomotion"] = ["Walk", "Run"]
    return st


def budget(B) -> dict:
    t = M.tri_count
    body, body1 = B.body, B.body1
    items = [o for o, _ in B.items.values()]
    return {
        "spec": "art-inventory-ch1 §1.9 NPC (merged look): LOD0 8-12k, LOD1 5k triangles, <= 75 bones per section",
        "lod0": {"toon": t(body, 0), "outline": t(body, 1)},
        "lod1": {"toon": t(body1, 0), "outline": t(body1, 1)} if body1 is not None else None,
        "items": {"toon": sum(t(o, 0) for o in items), "outline": sum(t(o, 1) for o in items)},
        "weightedBones": len({g.name for g in body.vertex_groups}),
        "decision": "the §1.9 figures budget the toon surface; the baked hull (P7) is a second unlit one-colour draw "
                    "kept <= ~0.6x the toon triangles by low-poly hull proxies and no-hull trims",
    }


# ── QA ──────────────────────────────────────────────────────────────────────────────────────────────────
HAND_PARTS = ("hand_l", "hand_r", "thumb_l", "thumb_r", "wrist_l", "wrist_r")


def qa_run(B, clips, actions) -> dict:
    names = json.loads(B.body["af_parts"])
    items = {n: o for n, (o, _) in B.items.items()}
    groups = {"body": "*", "nohands": ("!", HAND_PARTS + ("arm_l", "arm_r", "cuff_l", "cuff_r"))}
    pairs = {f"x:{n[len('SM_NPCKit_Prop_'):]}": ("nohands", f"item:{n}") for n in items}

    def grip(name):
        # faces inside the fist: within 7 cm of the grip (the origin) along the item axis
        return lambda co: abs(co.y) < 0.075 and math.hypot(co.x, co.z) < 0.06
    q = ToonGroundQA(B.rig, B.body, items, names, groups, pairs, {n: grip(n) for n in items})
    if B.folk.skirted:
        q.coat = CoatLegQA(B.rig, B.body, names, B.folk.skirt_z0, B.folk.skirt_z1)
    return q.run(clips, actions)


class CoatLegQA:
    """Per-frame check that no trouser / boot vertex shows through a robe or coat skirt (art review: the escort's
    Run punched the legs through the coat's front panels). Each leg vertex is joined to its own core — the nearest
    point on the posed thigh / calf bone of its side — and that segment must not cross the skirt shell: a leg that
    leaves the coat under the hem never crosses it, a knee or thigh pushed through a panel does. Metrics:
    ``coatLegs`` = leg vertices beyond the cloth, ``coatLegsMm`` = the deepest one's distance past it (mm)."""

    LEG_PARTS = ("leg_l", "leg_r", "boot_shaft_l", "boot_shaft_r", "boot_cuff_l", "boot_cuff_r")

    def __init__(self, rig, body, part_names, z0: float = 0.0, z1: float = 0.0):
        from qa import _face_parts
        self.rig, self.body = rig, body
        me = body.data
        nf = len(me.polygons)
        mi = np.zeros(nf, np.int32)
        me.polygons.foreach_get("material_index", mi)
        names = _face_parts(body, part_names)
        toon = mi == 0
        self.coat_faces = np.nonzero(toon & (names == "skirt"))[0]
        lt = np.zeros(nf, np.int32)
        me.polygons.foreach_get("loop_total", lt)
        ls = np.zeros(nf, np.int32)
        me.polygons.foreach_get("loop_start", ls)
        lv = np.zeros(len(me.loops), np.int32)
        me.loops.foreach_get("vertex_index", lv)
        self.leg_verts, self.ink_verts = {}, {}
        for s in ("l", "r"):
            legs = np.isin(names, [p for p in self.LEG_PARTS if p.endswith("_" + s)])
            for out, sel in ((self.leg_verts, toon & legs), (self.ink_verts, (mi == 1) & legs)):
                f_ = np.nonzero(sel)[0]
                if len(f_):
                    out[s] = np.unique(np.concatenate([lv[ls[f]:ls[f] + lt[f]] for f in f_]))

    def frame(self, geom=None) -> dict:
        from mathutils.bvhtree import BVHTree
        from qa import Checker
        if not self.leg_verts or not len(self.coat_faces):
            return {}
        co, lt, ls, lv, co_list = geom or Checker._geom(self.body)
        polys = [lv[ls[f]:ls[f] + lt[f]].tolist() for f in self.coat_faces]
        tree = BVHTree.FromPolygons(co_list, polys, all_triangles=False)
        mw = self.rig.obj.matrix_world
        out = {}
        for key, verts in (("coatLegs", self.leg_verts), ("coatLegInk", self.ink_verts)):
            n_out, deep = 0, 0.0
            for s, vids in verts.items():
                segs = []
                for bn in (f"thigh_{s}", f"calf_{s}"):
                    pb = self.rig.obj.pose.bones[bn]
                    segs.append((np.array(mw @ pb.head), np.array(mw @ pb.tail)))
                P = co[vids]
                best = None
                for a, b in segs:
                    ab = b - a
                    t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-12), 0.0, 1.0)
                    c = a + t[:, None] * ab
                    dd = np.linalg.norm(P - c, axis=1)
                    best = (c, dd) if best is None else (np.where((dd < best[1])[:, None], c, best[0]),
                                                         np.minimum(dd, best[1]))
                for p, c in zip(P, best[0]):
                    d = p - c
                    ln = float(np.linalg.norm(d))
                    if ln < 1e-5:
                        continue
                    hit = tree.ray_cast(Vector(c), Vector(d / ln), ln)
                    if hit[0] is not None:
                        n_out += 1
                        deep = max(deep, ln - hit[3])
            out[key] = float(n_out)
            out[key + "Mm"] = deep * 1000.0
        return out


class ToonGroundQA(HB.PairQA):
    """``PairQA`` with the items' ground measured on their toon surface: a planted staff foot / sword point rests on
    the floor, and its baked hull (the ink, 1.6 cm) reaching under the floor there is expected — the ground hides it
    as it hides the soles' ink. Skirted looks add :class:`CoatLegQA` (legs never in front of the coat / robe)."""

    coat: "CoatLegQA | None" = None

    def frame(self) -> dict:
        from qa import Checker
        out = super().frame()
        for nm, ob in self.items.items():
            if ob.hide_render or ob.parent is None:
                continue
            me = ob.data
            mi = np.zeros(len(me.polygons), np.int32)
            me.polygons.foreach_get("material_index", mi)
            out["ground:" + nm] = Checker._tree(Checker._geom(ob), np.nonzero(mi == 0)[0])[1] * 100.0
        if self.coat is not None:
            out.update(self.coat.frame())
        return out


QA_LIMITS = {"ground": -0.8, "coatLegs": 0.0}
# face down (Death 22-28) the knees press the front panel into the floor under the body — out of every view; the
# legs may cross it there by ≤ 12 mm
QA_CLIP_LIMITS = {"Death": {"coatLegs": 1e9, "coatLegsMm": 12.0}}
QA_ITEM_LIMIT = 12          # prop x body (hands / arms excluded) intersecting triangle pairs per frame: a graze


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
def save_native(path: Path, img: np.ndarray) -> list:
    """Every NPC review PNG (art review 9 / NPC review 11): truecolour at scale 1 (``pngio.write_png_native``,
    ``<stem>_2.png`` … tiles along the sheet's pads when over 200 KB) — the 256-colour budget fallback turned the
    elder's small green orb metal grey on the props and lineup sheets and quantised the measured 1080p cells."""
    out = pngio.write_png_native(path, img)
    if len(out) > 1:
        print(f"  {Path(path).name}: {len(out)} truecolour tiles")
    return out


class NpcReview(review.Review):
    """The kit review set with every PNG written by :func:`save_native`; ``save_as`` renames the next kit-named
    file (``closeup.png`` → ``closeup_front.png``, ``anims.png`` → ``anim_<Clip>.png``) before it is written, so
    its tiles carry the final name."""

    save_as: str | None = None

    def _save(self, name: str, img: np.ndarray) -> Path:
        if self.save_as:
            name, self.save_as = self.save_as, None
        out = save_native(self.out / name, img)
        self.written.extend(out)
        return out[0][0]


def make_review(B, out: Path) -> review.Review:
    rv = NpcReview(B.npc.asset, B.rig.obj, meshes_of(B), B.folk.D.height, blob_radius=B.npc.blob_radius,
                   out_dir=out, game_outline_px=PX, outline_class="npc", glows=glows_for(B))
    rv.gate = ink_gate()
    rv.ground_occludes = True
    return rv


def game_cells(B, rv, acts: dict, clips: dict, keys, fname: str, cols: int = 4) -> None:
    """Native 1080p crops at the W1 camera with the shipped look (UE ink width, bloom, glow cards); every cell's ink
    rim is measured (``rv.ink_reports``: the ship's ink gate) and printed under its label."""
    rig = B.rig
    cells = []
    review.setup_render((1920, 1080), 8)
    H = B.folk.D.height
    hw = 0.058 * (1.15 if B.scenery else 1.0)
    zc = H * 0.45
    for name, fr, fac in keys:
        c = clips[name]
        fr = c.frames // 2 if fr == "mid" else int(fr)
        rv.facing(fac)
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        cam = review.game_camera()
        rv.game_outline(1080, Vector((0, 0, zc)))
        cc = review.project(cam, Vector((0, 0, zc)))
        # poses that leave the origin (a fall): centre the crop on the figure so the measured silhouette is whole
        pts = review.mesh_points([m for m in meshes_of(B, scenery=False) if not m.hide_render], step=5)
        lo_, hi_ = pts.min(0), pts.max(0)
        if max(abs(lo_[0]), abs(hi_[0]), abs(lo_[1]), abs(hi_[1])) > 0.5:
            corners = [Vector((x, y, z)) for x in (lo_[0], hi_[0]) for y in (lo_[1], hi_[1]) for z in (lo_[2], hi_[2])]
            pr = np.array([review.project(cam, c) for c in corners])
            cc = tuple((pr.min(0) + pr.max(0)) * 0.5)
        hh = hw * 1920 / 1080
        img, st = rv.game_shot((cc[0] - hw, cc[1] - hh, cc[0] + hw, cc[1] + hh), measure=True,
                               tag=f"{fname} {name} F{fr} {fac}")
        review.draw_text(img, 3, 3, f"{name} F{fr} {fac}")
        review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
        cells.append(img)
    rv._save(fname, review.grid(cells, cols))


def closeups(B, rv, acts, clips, keys, fname: str, res=(240, 300)) -> None:
    rig = B.rig
    cells = []
    for name, fr, fac in keys:
        c = clips[name]
        fr = c.frames // 2 if fr == "mid" else int(fr)
        rv.facing(fac)
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        review.setup_render(res, 8)
        vis = [m for m in rv.meshes if not m.hide_render]
        pts = review.mesh_points(vis)
        review.frame_points(pts, res, fov_h=26, pitch_ue=-35, fill=0.92)
        rv.ink(at=Vector(pts.mean(0)))
        img = review.render_array()
        mark = ""
        for d in c.notify_list():
            if scene.ms_to_frame(d["ms"]) == fr:
                mark = f" {d['name'].upper()}"
        review.draw_text(img, 3, 3, f"{name} F{fr}{mark} {fac}")
        cells.append(img)
    rv._save(fname, review.grid(cells, min(5, len(cells))))


PORTRAIT_INK_PX = 4.5


def render_portrait(B, rv, acts, clips, out: Path) -> Path:
    """Spec §9.5 / R11: 512² RGBA head-and-shoulders, 3/4 front, transparent, toon + screen-space ink
    (``review.ink_render``), the runtime glow composited (no bloom on a UI texture)."""
    npc, rig = B.npc, B.rig
    name = "IdleNPC"
    anim.assign(rig, acts[name])
    anim.set_frame(0)
    rv.facing(-110.0)
    res = (512, 512)
    review.hide_stage(True)
    review.setup_render(res, 16)
    vis = [m for m in rv.meshes if not m.hide_render and m not in [o for o, *_ in B.scenery]]
    for o, *_ in B.scenery:
        o.hide_render = True
    pts = review.mesh_points(vis)
    hc = _world_socket(rig, "fx_head")
    bust_z = hc.z - 0.50
    sel = pts[(pts[:, 2] > bust_z) & (np.linalg.norm(pts[:, :2] - np.array(hc[:2]), axis=1) < 0.40)]
    cam = review.frame_points(sel, res, fov_h=24, pitch_ue=-9, fill=0.98, include_ground=False)
    for m in rv.meshes:
        outline.remove_preview(m)
    rgba = review.ink_render(vis, res, silhouette_px=PORTRAIT_INK_PX, interior_px=2.0, depth_step=0.018)
    if "orbGlow" in npc.fx:
        g = npc.fx["orbGlow"]
        staff = next(o for o, _ in B.items.values() if o.name == P.PROPS["staff"])
        p = staff.matrix_world @ Vector((0, P.STAFF_UP, 0))
        cx, cy = review.project_px(cam, p, res)
        if 0 <= cx < res[0] and 0 <= cy < res[1]:
            rad = g["radiusCm"] / 100.0 / outline.pixel_world_size(cam, p, res[0])
            review.composite_glow(rgba, (cx, cy), rad, g["color"], g["alpha"], core=0.3)
    outp = paths.export_root() / "Portraits" / f"T_UI_Portrait_{npc.id}.png"
    outp.parent.mkdir(parents=True, exist_ok=True)
    pngio.write_png(outp, rgba)
    review.hide_stage(False)
    for o, *_ in B.scenery:
        o.hide_render = False
    bg = np.array([59, 53, 70], np.float32)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    comp = (rgba[:, :, :3].astype(np.float32) * a + bg * (1 - a) + 0.5).astype(np.uint8)
    review.draw_text(comp, 4, 4, f"T_UI_PORTRAIT_{npc.id.upper()}")
    rv._save("portrait.png", comp)
    rv.facing("se")
    return outp


def standard_keys(B, clips: dict) -> tuple[list, list]:
    npc = B.npc
    game = [("IdleNPC", 0, "se"), (f"Work_{npc.work}", 0, "se"),
            (f"Work_{npc.work}", scene.ms_to_frame(npc.work_ms * 0.32), "se"),
            (f"Work_{npc.work}", scene.ms_to_frame(npc.work_ms * 0.32), "sw"),
            ("Alert", clips["Alert"].frames, "se"), ("Talk", scene.ms_to_frame(400.0), "se"),
            ("IdleNPC_Fidget", scene.ms_to_frame(clips["IdleNPC_Fidget"].length_ms * 0.45), "se"),
            ("Alert", clips["Alert"].frames, "sw")]
    facings = [("IdleNPC", 0, "front"), ("IdleNPC", 0, "sw"), ("IdleNPC", 0, "ne"), ("IdleNPC", 0, "back")]
    for x in npc.extra_clips:
        if x in ("Walk", "Run"):
            game.append((x, scene.ms_to_frame(clips[x].length_ms * 0.25), "se"))
        elif x in ("Hurt", "Cower"):
            game.append((x, "mid", "se"))
        elif x == "Death":
            game.append((x, clips[x].frames, "se"))
    facings += game[8:]
    game = game[:8]
    close = [("IdleNPC", 0, "se"), (f"Work_{npc.work}", scene.ms_to_frame(npc.work_ms * 0.32), "se"),
             ("Alert", clips["Alert"].frames, "se"), ("Talk", scene.ms_to_frame(400.0), "se"),
             ("IdleNPC_Fidget", scene.ms_to_frame(clips["IdleNPC_Fidget"].length_ms * 0.45), "se")]
    return game, facings, close


def render_previews(B, clips, actions, out: Path) -> tuple[list, dict]:
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("*.png"):
        p.unlink()
    rig = B.rig
    acts = dict(zip([c.name for c in clips], actions))
    cl = {c.name: c for c in clips}
    rv = make_review(B, out)
    wk = f"Work_{B.npc.work}"
    anim.assign(rig, acts[wk])
    anim.set_frame(0)
    rv.game_view()
    rv.save_as = "closeup_front.png"
    rv.closeup("front", pitch=-30.0)
    rv.closeup("se")
    anim.assign(rig, acts["IdleNPC"])
    anim.set_frame(0)
    rv.turnaround()
    for c, a in zip(clips, actions):
        rv.save_as = f"anim_{c.name}.png"
        rv.contact_sheet(rig, [(c, a)], frames=8)
    game, facings, close = standard_keys(B, cl)
    game_cells(B, rv, acts, cl, game, "game_poses.png")
    game_cells(B, rv, acts, cl, facings, "game_facings.png")
    closeups(B, rv, acts, cl, close, "poses_close.png")
    render_portrait(B, rv, acts, cl, out)
    if B.body1 is not None:
        lod_sheet(B, rv, acts)
    rv.facing("se")
    for p, size in rv.written:
        print(f"  {p.name:28s} {size // 1024:4d} KB")
    ink = review.ink_verdict(rv.ink_reports)
    print(f"\n[ink] {ink['cells']} gated 1080p cells: min rim {ink['minFracRim'] * 100:.1f} %, "
          f"min dark {ink['minFracDark'] * 100:.1f} %, median rim {ink['medianRimPx']} px")
    for f in ink["failures"]:
        print("  INK FAIL " + f)
    return [str(p) for p, _ in rv.written], ink


def lod_sheet(B, rv, acts) -> None:
    anim.assign(B.rig, acts["IdleNPC"])
    anim.set_frame(0)
    cells = []
    res = (240, 300)
    for body, tag in ((B.body, "LOD0"), (B.body1, "LOD1")):
        B.body.hide_render = body is not B.body
        B.body1.hide_render = body is not B.body1
        rv.facing("se")
        review.setup_render(res, 8)
        vis = [body] + [o for o, _ in B.items.values()]
        pts = review.mesh_points(vis)
        review.frame_points(pts, res, fov_h=26, pitch_ue=-35, fill=0.9)
        rv.meshes[0] = body
        rv.ink(at=Vector(pts.mean(0)))
        img = review.render_array()
        review.draw_text(img, 3, 3, f"{tag} {M.tri_count(body, 0)}+{M.tri_count(body, 1)}")
        cells.append(img)
    rv.meshes[0] = B.body
    B.body.hide_render = False
    B.body1.hide_render = True
    rv._save("lod1.png", review.grid(cells, 2))


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def ship(npc, previews: bool = True, verify: bool = True) -> dict:
    import mship
    t0 = time.time()
    B = build(npc, lod1=True)
    rig, body, pal = B.rig, B.body, B.pal
    print(f"[build] {npc.asset}: body {M.tri_count(body, 0)} toon + {M.tri_count(body, 1)} hull tris, LOD1 "
          f"{M.tri_count(B.body1, 0)} + {M.tri_count(B.body1, 1)}, bones {len(rig.obj.data.bones)}")
    for b_ in (body, B.body1):
        st = R.weight_stats(b_)
        print(f"[weights] {b_.name}: max {st['maxInfluences']} influences, {st['unweighted']} unweighted")
        assert st["unweighted"] == 0 and st["maxInfluences"] <= 4
    pal.save()
    B.body1.hide_render = True
    A = anims(B)
    attach(B)
    clips = A.all_clips()
    actions = []
    for c in clips:
        t1 = time.time()
        actions.append(anim.bake(rig, c, npc.token))
        print(f"[bake] {c.name} {c.frames}f {time.time() - t1:.1f}s")
    anim.assign(rig, None)
    anim.clear_pose(rig)
    sk = export.export_skeletal_mesh(rig, [body], npc.asset, CATEGORY)
    sk1 = export.export_skeletal_mesh(rig, [B.body1], f"{npc.asset}_LOD1", CATEGORY)
    entries = []
    for c, a in zip(clips, actions):
        fbx = export.export_animation(rig, a, CATEGORY)
        e = export.clip_entry(c, a, fbx)
        if c.name == "IdleNPC_Fidget":
            e["variantOf"] = "IdleNPC"
        if c.name.startswith("Work_"):
            e["workStyle"] = npc.work
        entries.append(e)
    anim.assign(rig, None)
    anim.clear_pose(rig)
    bpy.context.view_layer.update()
    man = export.Manifest()
    man.set_palette(pal)
    fx = {k: {**v, "depthTest": DEPTH_TEST_NOTE} for k, v in npc.fx.items()}
    extra = {"displayName": npc.display, "previews": [], "fx": fx, "npcStates": npc_states(npc, clips),
             "build": npc.look.build, "budget": budget(B), "notes": npc.notes,
             "portrait": f"Portraits/T_UI_Portrait_{npc.id}.png"}
    if npc.scenery:
        extra["scenery"] = [{"asset": a, "offsetCm": scene.blender_to_ue_cm(off), "yawDeg": yaw,
                             "space": "character mesh space (before the actor's -90 deg mesh yaw), like sockets",
                             **({"note": npc.scenery_note} if npc.scenery_note else {})}
                            for a, off, yaw in npc.scenery]
    entry = export.skeletal_entry(rig, body, sk, CATEGORY, pal, "npc", entries, list(npc.game_ids),
                                  blob_radius_cm=round(npc.blob_radius * 100.0, 1), extra=extra)
    entry["attachments"] = [{"object": n, "socket": bone} for n, (o, bone) in B.items.items()]
    entry["materialSlots"][1]["outlineMix"] = outline.INK_MIX["npc"]
    ink_slot(entry["materialSlots"][1])
    entry["lods"] = [{"index": 1, "asset": f"{npc.asset}_LOD1", "fbx": paths.rel_to_export(sk1), "screenSize": 0.05,
                      "triangles": {"toon": M.tri_count(B.body1, 0), "outline": M.tri_count(B.body1, 1)},
                      "import": f"same SKEL_Human armature and bind pose; import as LOD index 1 of {npc.asset}",
                      "use": "mobile-low gameplay mesh (skeletal LOD bias 1, baked hull only); an NPC is 0.10-0.20 "
                             "of the screen at every W1 zoom, so desktop always draws LOD0 (as the monsters)"}]
    man.set_asset(npc.asset, entry)
    man.save()
    print(f"[export] {sk.name} + LOD1 + {len(entries)} clips ({time.time() - t0:.1f}s)")
    result = {"sk": str(sk), "lod1": str(sk1), "anims": [e["fbx"] for e in entries], "manifest": str(man.path)}
    rep = qa_run(B, clips, actions)
    print("\n[qa] per-clip worst values (ground cm; overlaps = intersecting triangle pairs)")
    print(HB.PairQA.table(rep))
    lim = dict(QA_LIMITS)
    lim.update({f"x:{n[len('SM_NPCKit_Prop_'):]}": QA_ITEM_LIMIT for n in B.items})
    fails = HB.PairQA.verdict(rep, lim, QA_CLIP_LIMITS)
    for f in fails:
        print("  QA FAIL " + f)
    result["qa"] = {"pass": not fails, "failures": fails, "worst": {k: v["worst"] for k, v in rep.items()}}
    if verify:
        src = mship.record(rig, clips, actions)
    if previews:
        out = paths.preview_root() / npc.preview_dir
        result["previews"], result["ink"] = render_previews(B, clips, actions, out)
        man = export.Manifest()
        e2 = man.data["assets"][npc.asset]
        e2["previews"] = [f"{npc.preview_dir}/{p.name}" for p in sorted(out.glob("*.png"))]
        ink = result["ink"]
        e2["inkGate"] = {k: ink[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx",
                                             "maxHullLumP95", "gate")}
        e2["qa"] = {"pass": result["qa"]["pass"], "failures": result["qa"]["failures"]}
        man.save()
    if verify:
        ms = SimpleNamespace(asset=npc.asset, skeleton_bones_required=("weapon_r", "weapon_l", "head", "pelvis"),
                             clips=clips, category=CATEGORY, token=npc.token, items=[], pal=pal)
        result["verify"] = mship.verify_export(ms, Path(sk), src, entry)
    print(f"[ship] {npc.asset} done in {time.time() - t0:.1f}s")
    return result


# ── dev modes ───────────────────────────────────────────────────────────────────────────────────────────
def quick(npc, out: Path, pose: str | None = None, facing: str = "se") -> None:
    out.mkdir(parents=True, exist_ok=True)
    B = build(npc)
    A = anims(B)
    attach(B)
    if pose:
        name, fr = pose.split(":")
        clip = next(c for c in A.all_clips() if c.name == name)
        act = anim.bake(B.rig, clip, npc.token)
        anim.assign(B.rig, act)
        anim.set_frame(int(fr))
    else:
        anim.apply_pose(A.solve(A.ready()))
    rv = make_review(B, out)
    rv.save_as = f"closeup_{facing}.png"
    rv.closeup(facing)
    rv.save_as = "closeup_front.png"
    rv.closeup("front", pitch=-30.0)
    rv.turnaround()
    rv.game_view()
    print(f"[quick] {npc.asset}: {M.tri_count(B.body, 0)} toon + {M.tri_count(B.body, 1)} hull tris")
    for p, s in rv.written:
        print(f"  {p} {s // 1024} KB")


def anim_dev(npc, out: Path, names: list, facing: str = "se") -> None:
    out.mkdir(parents=True, exist_ok=True)
    B = build(npc)
    A = anims(B)
    attach(B)
    rv = make_review(B, out)
    for c in A.all_clips():
        if names and c.name not in names:
            continue
        t1 = time.time()
        a = anim.bake(B.rig, c, npc.token)
        print(f"[bake] {c.name} {c.frames}f {time.time() - t1:.1f}s")
        tag = "" if facing == "se" else f"_{facing}"
        rv.save_as = f"anim_{c.name}{tag}.png"
        rv.contact_sheet(B.rig, [(c, a)], frames=8, facing=facing)
        print(f"  {out / f'anim_{c.name}{tag}.png'}")


def main(argv: list) -> int:
    args = [a for a in argv[1:] if not a.startswith("--")]
    flags = [a for a in argv[1:] if a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    what = args[0]
    if what == "props":
        import npc_dressing
        return npc_dressing.ship_props()
    if what == "dressing":
        import npc_dressing
        return npc_dressing.ship_dressing()
    if what == "kit":
        import npc_kit_parts
        return npc_kit_parts.ship_kit(args[1:])       # kit [SK_NPCKit_… …]: only those parts are exported
    if what == "lineup":
        import npc_dressing
        return npc_dressing.lineup()
    npcs = LK.NPCS if what == "all" else [LK.find(what)]
    fl = {f.split("=")[0]: (f.split("=", 1)[1] if "=" in f else True) for f in flags}
    if "--quick" in fl:
        for n in npcs:
            quick(n, SCRATCH / n.snake, pose=fl.get("--pose"), facing=fl.get("--facing", "se"))
        return 0
    if "--anims" in fl:
        for n in npcs:
            anim_dev(n, SCRATCH / n.snake, args[1:], facing=fl.get("--facing", "se"))
        return 0
    import mship
    rc = 0
    for n in npcs:
        res = ship(n, previews="--no-previews" not in fl, verify="--no-verify" not in fl)
        rc = max(rc, mship.main_result(res))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
