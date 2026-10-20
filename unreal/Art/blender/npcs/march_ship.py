"""March NPC family ship / review / dev CLI (A9; Docs/dark-remake/implementation-plan.md §5, chapter1.md §3).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/march_ship.py <npc|all> [flags]
      <npc>              QuestElder_March | KnellChaplain | Blacksmith_March | Merchant_March | StashKeeper_March |
                         PlainsHerbalist_March | PlainsWanderer_March | CrowTrader | EscortTravelingMerchant_March |
                         RescueLostTraveler_March | WanderingMerchant_March (or the role / snake / game id)
      (no flag)          ship (``npc_ship.ship`` on the march build): bake every clip, per-frame QA, FBX (SK + LOD1 +
                         A_ per clip), manifest (merged; themed game ids ``npc_<id>@march``), re-import verification,
                         the review set in Art/Previews/npc_<snake>/ on the march ash ground with the ink gate on
                         every game-camera cell, portrait T_UI_Portrait_<Id> (exit 1 on a QA / ink / verify failure)
      --no-previews --no-verify
      --quick [--pose=Clip:frame] [--facing=se]     mesh iteration → $AF_SCRATCH_ROOT/npcs_march/<snake>
      --anims [Clip …] [--facing=se]                bake + contact sheets → scratch
      --game [Clip:frame:facing …]                  gated 1080p game cells → scratch
      --barrow                                      Old Bern's barrow.png (profile: legs / wheel) → scratch
    march_ship.py props      SM_NPCKit_Prop_{Handbell,Lantern,Ledger,Vial,Barrow,Spade,SwordNotched} (Art/Export/NPCs) + the sheet
                             Previews/npc_props_march/props.png
    march_ship.py dressing   SM_Prop_March_AnvilStump (Art/Export/Props; npc_scenery_anvil@march)
    march_ship.py kit        SK_NPCKit_{HalfPlate,Gorget,Top_Cassock,Hat_Hood,FeatherCollar,RelicTray}
                             (Art/Export/NPCs) + Previews/npc_kit_march/kit_parts.png
    march_ship.py lineup     Previews/npc_family_march/ (the eleven + the warrior, march ground, R2 heights)

Ship one NPC at a time (each rewrites the NPC palette atlas; the registry merges under its lock either way).
"""
from __future__ import annotations

import json
import math
import sys
import time
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "monsters", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import anim, export, mesh as M, outline, paths, review, rig as R, scene  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.paths import scratch_root  # noqa: E402

import folk as F  # noqa: E402
import npc_ship as S  # noqa: E402
import march_folk as MF  # noqa: E402
import march_looks as ML  # noqa: E402
import march_props as MPR  # noqa: E402
from march_clips import MarchAnims  # noqa: E402
import march_palettes as MP  # noqa: E402

SCRATCH = scratch_root("npcs_march")
CATEGORY = "NPCs"
STAGE_HEX = MP.MARCH_GROUND["0"][1]          # ash grass #4C4940 (A1 proposal for A10's terrain style `march`)


def march_stage() -> None:
    """Review stage on the march ash grass (every ``kit.review.Review`` built afterwards, as the Wave-1 monster
    previews: ``gnawers.march_stage``); nothing in the kit changes."""
    if getattr(review.stage, "_af_march", False):
        return
    orig = review.stage

    def stage(radius: float = 30.0, blob_radius: float = 0.39, ground_hex: str = STAGE_HEX, grid: bool = True):
        return orig(radius, blob_radius, STAGE_HEX, grid)
    stage._af_march = True
    review.stage = stage


# ── build ───────────────────────────────────────────────────────────────────────────────────────────────
def to_bone_space(obj, rig: R.Rig, bone: str, socks: list | None = None) -> list:
    """Re-express a prop modelled in body space in ``bone``'s rest frame at its head (the UE socket convention of
    ``rig.attach_to_bone``); sockets too."""
    m = rig.obj.data.bones[bone].matrix_local.inverted()
    obj.data.transform(m)
    obj.data.update()
    out = []
    for s in socks or []:
        out.append({**s, "pos": tuple(m @ Vector(s["pos"]))})
    return out


def build(npc, lod1: bool = False) -> SimpleNamespace:
    scene.reset()
    pal = Palette(F.FAMILY)
    folk = MF.MarchFolk(npc.look, npc.asset, prefix=npc.prefix)
    rig = R.build_humanoid(folk.spec())
    R.add_socket(rig, "eyes", "head", folk.hp(F.EYE_H, 0.0, 1.0))
    if npc.look.item == "barrow":
        # the barrow's own bone (a ``MarchFolk.spec`` chain): head at the wheel's axle; the socket sits on it
        R.add_socket(rig, "barrow", "barrow_01", tuple(MPR.barrow_axle()))
    body = folk.build(pal, rig)
    body1 = S.build_lod1(folk, pal, rig) if lod1 else None
    items, socks = {}, {}
    for kind, bone in ((npc.look.item, "weapon_r"), (npc.look.off, "weapon_l")):
        if kind == "none":
            continue
        obj, s = MPR.build_prop(kind, pal)
        if kind == "barrow":
            s = to_bone_space(obj, rig, "barrow_01", s)
            bone = "barrow"
        items[obj.name] = (obj, bone)
        socks[obj.name] = s
    scenery = []
    for name, off, yaw in npc.scenery:
        obj, s = MPR.build_anvil(pal)
        scenery.append((obj, Vector(off), yaw, s))
    return SimpleNamespace(npc=npc, pal=pal, folk=folk, rig=rig, body=body, body1=body1, items=items, socks=socks,
                           scenery=scenery)


def attach(B) -> None:
    names = {b.name for b in B.rig.obj.data.bones}
    for name, (obj, bone) in B.items.items():
        if bone in names:
            R.attach_to_bone(obj, B.rig, bone)
        else:
            sk = next(s for s in B.rig.sockets if s["name"] == bone)
            R.attach_to_bone(obj, B.rig, sk["bone"])
    for obj, off, yaw, _ in B.scenery:
        obj.parent = B.rig.obj
        obj.matrix_parent_inverse = Matrix.Identity(4)
        obj.matrix_basis = Matrix.Translation(off) @ Matrix.Rotation(math.radians(yaw), 4, "Z")


def anims(B) -> MarchAnims:
    A = MarchAnims(B.rig, B.folk, B.npc, {n: o for n, (o, _) in B.items.items()})
    if B.npc.scenery:
        A.ANVIL_AT = Vector(B.npc.scenery[0][1])
    B.anims = A
    return A


_plain_glows = S.glows_for


def glows_for(B):
    base = _plain_glows(B)
    fx = B.npc.fx

    def fn(cam, res):
        out = base(cam, res)
        if "lanternGlow" in fx:
            g = fx["lanternGlow"]
            lan = next((o for o, _ in B.items.values() if o.name == g["item"]), None)
            if lan is not None:
                p = lan.matrix_world @ Vector(next(k["pos"] for k in B.socks[lan.name] if k["name"] == g["socket"]))
                v = S.card_visibility(cam, p, g["radiusCm"] / 100, ignore=(lan,))
                if v > 0.0:
                    out.append(S._card(cam, res, p, g["radiusCm"] / 100, g["color"], g["alpha"] * v, g["coreColor"],
                                       g["coreFrac"], g["coreAlpha"] * v))
        return out
    return fn


def game_cells(B, rv, acts: dict, clips: dict, keys, fname: str, cols: int = 4) -> None:
    """``npc_ship.game_cells`` with a wider crop for the barrow (Bern pushes it 1.6 m ahead of him)."""
    rig = B.rig
    cells = []
    review.setup_render((1920, 1080), 8)
    H = B.folk.D.height
    wide = B.npc.look.item == "barrow"
    hw = 0.058 * (1.15 if B.scenery else 1.0) * (1.7 if wide else 1.0)
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
        pts = review.mesh_points([m for m in S.meshes_of(B, scenery=False) if not m.hide_render], step=5)
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


# ── Old Bern's barrow: the lifted loops' wheel keys, the barrow QA ─────────────────────────────────────────
_plain_bake = anim.bake


def bake(rig, clip, asset_token, bones=None):
    """``kit.anim.bake`` + the barrow wheel's loop end: a loop's last frame is keyed as its first (the spin's 0°);
    a lifted loop (``MarchAnims.wheel_total``) keys it at the total spin instead (a quarter-turn multiple: the wheel's
    four-fold face wraps to frame 0 without a pop), so the last interval turns forward, not back."""
    act = _plain_bake(rig, clip, asset_token, bones)
    total = getattr(clip, "af_wheel_total", None)
    if total is None or "barrow_wheel_01" not in rig.obj.data.bones:
        return act
    p = anim.Pose(rig)
    p.rot("barrow_wheel_01", pitch=total)
    q = anim.evaluate(p)["barrow_wheel_01"][1]
    fcs = [next((fc for fc in _fcurves(act) if fc.data_path == 'pose.bones["barrow_wheel_01"].rotation_quaternion'
                 and fc.array_index == i), None) for i in range(4)]
    if any(fc is None for fc in fcs):
        return act
    n = len(fcs[0].keyframe_points)
    prev = [fc.keyframe_points[n - 2].co[1] for fc in fcs]
    if sum(a * b for a, b in zip(prev, q)) < 0:
        q = -q
    for i, fc in enumerate(fcs):
        fc.keyframe_points[n - 1].co[1] = q[i]
        fc.update()
    return act


def _fcurves(act):
    for layer in act.layers:
        for strip in layer.strips:
            for cb in strip.channelbags:
                yield from cb.fcurves


_plain_all_clips = MarchAnims.all_clips


def _all_clips(self):
    clips = _plain_all_clips(self)
    for c in clips:
        t = self.wheel_total(c.name)
        if t is not None:
            c.af_wheel_total = t
    return clips


MarchAnims.all_clips = _all_clips

WHEEL_PARTS = ("wheel", "wheel_tyre", "wheel_batten", "wheel_hub")
GRIP_MM = 8.0               # fist (weapon bone head) to the barrow's handle socket, anchored sides
LIFT_RANGE = (7.5, 13.0)    # cm the back legs clear the ground through Walk / Run (review: about 8-12)


class BarrowQA(S.ToonGroundQA):
    """``npc_ship.ToonGroundQA`` + Old Bern's barrow per frame: ``gripMm`` = the farthest anchored fist from its
    handle socket (``handle_l`` / ``handle_r`` posed with the barrow), ``legLiftCm`` = the back legs' lowest toon
    point over the ground (Walk / Run: lifted ``LIFT_RANGE``; every other clip resting on them, ±0.8 cm)."""

    anims: MarchAnims = None
    barrow = None
    socks: dict = {}

    def run(self, clips, actions, names=None, before=None):
        def b4(name):
            self._clip = name
            if before is not None:
                before(name)
        return super().run(clips, actions, names, before=b4)

    def frame(self) -> dict:
        out = super().frame()
        if self.barrow is None:
            return out
        clip = getattr(self, "_clip", "")
        bo = self.barrow
        rig = self.rig.obj
        mw = bo.matrix_world
        worst = 0.0
        for s in self.anims.anchored(clip):
            h = mw @ Vector(self.socks[f"handle_{s}"])
            pb = rig.pose.bones[f"weapon_{s}"]
            f = rig.matrix_world @ pb.head
            worst = max(worst, (h - f).length * 1000.0)
        out["gripMm"] = worst
        me = bo.data
        if not hasattr(self, "_leg_verts"):
            names = json.loads(bo["af_parts"]) if "af_parts" in bo else []
            pa = np.zeros(len(me.polygons), np.int32)
            if "af_part" in me.attributes:
                me.attributes["af_part"].data.foreach_get("value", pa)
            mi = np.zeros(len(me.polygons), np.int32)
            me.polygons.foreach_get("material_index", mi)
            legs = {i for i, n in enumerate(names) if n == "leg"}
            vs = set()
            for poly, pid, m in zip(me.polygons, pa, mi):
                if pid in legs and m == 0:
                    vs.update(poly.vertices)
            self._leg_verts = np.array(sorted(vs), np.int64)
        if len(self._leg_verts):
            co = np.zeros(len(me.vertices) * 3)
            me.vertices.foreach_get("co", co)
            co = co.reshape(-1, 3)[self._leg_verts]
            m = np.array(mw)
            z = co @ m[:3, :3].T + m[:3, 3]
            low = float(z[:, 2].min()) * 100.0
            lifted = clip in MarchAnims.LIFT_CLIPS
            out["legLiftShortCm"] = max(0.0, LIFT_RANGE[0] - low) if lifted else max(0.0, low - 0.8)
            out["legLiftOverCm"] = max(0.0, low - LIFT_RANGE[1]) if lifted else max(0.0, -0.8 - low)
            out["legLiftCm"] = low
        return out


_plain_qa_run = S.qa_run


# the march arms' own part names (Marl's rolled sleeves and gloves) are arms too: the prop pairs leave every arm out,
# as the plains ``arm_*`` / ``cuff_*``
MARCH_ARM_PARTS = ("forearm_l", "forearm_r", "sleeve_roll_l", "sleeve_roll_r", "roll_fold_l", "roll_fold_r",
                   "glove_cuff_l", "glove_cuff_r")


def qa_run(B, clips, actions) -> dict:
    """``npc_ship.qa_run`` on the march build: the prop pairs leave out the march arm parts too
    (``MARCH_ARM_PARTS``); Bern's wheel (in his mesh, on the barrow's axle) is not body for them, and his barrow adds
    ``BarrowQA``'s grip / leg-lift metrics."""
    names = json.loads(B.body["af_parts"])
    items = {n: o for n, (o, _) in B.items.items()}
    groups = {"body": "*", "nohands": ("!", S.HAND_PARTS + ("arm_l", "arm_r", "cuff_l", "cuff_r") + MARCH_ARM_PARTS
                                        + WHEEL_PARTS)}
    pairs = {f"x:{n[len('SM_NPCKit_Prop_'):]}": ("nohands", f"item:{n}") for n in items}

    def grip(name):
        return lambda co: abs(co.y) < 0.075 and math.hypot(co.x, co.z) < 0.06
    barrow = B.npc.look.item == "barrow"
    q = (BarrowQA if barrow else S.ToonGroundQA)(B.rig, B.body, items, names, groups, pairs,
                                                 {n: grip(n) for n in items})
    if barrow:
        q.anims = B.anims
        q.barrow = next(iter(items.values()))
        q.socks = {k["name"]: k["pos"] for k in B.socks[q.barrow.name]}
    if B.folk.skirted:
        q.coat = S.CoatLegQA(B.rig, B.body, names, B.folk.skirt_z0, B.folk.skirt_z1)
    return q.run(clips, actions)


def barrow_leg_low(bo) -> float:
    """The barrow's back legs' lowest toon point over the ground (cm), posed."""
    me = bo.data
    names = json.loads(bo["af_parts"]) if "af_parts" in bo else []
    pa = np.zeros(len(me.polygons), np.int32)
    if "af_part" in me.attributes:
        me.attributes["af_part"].data.foreach_get("value", pa)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    legs = {i for i, n in enumerate(names) if n == "leg"}
    vs = sorted({v for poly, pid, m in zip(me.polygons, pa, mi) if pid in legs and m == 0 for v in poly.vertices})
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)[np.array(vs, np.int64)]
    m = np.array(bo.matrix_world)
    return float((co @ m[:3, :3].T + m[:3, 3])[:, 2].min()) * 100.0


# loop phases picked so the wheel's four-fold batten cross shows a different angle in every cell (quarter-turn
# multiples look alike): Walk (1 turn / loop) 0 / 115 / 230 / 345 deg, Run (1.5 turns) 0 / 75 / 205 / 410 deg
BARROW_SHEET_KEYS = (("IdleNPC", 0.0), ("Walk", 0.0), ("Walk", 0.32), ("Walk", 0.64), ("Walk", 0.96),
                     ("Work_Barrow", 0.0), ("Run", 0.0), ("Run", 0.139), ("Run", 0.38), ("Run", 0.759))


def barrow_sheet(B, rv, acts: dict, clips: dict, res=(300, 200), pitch: float = -10.0) -> None:
    """``barrow.png`` (review 1): Old Bern in profile from a low camera, one fixed framing for every cell — the
    resting clips with the back legs on the ground, Walk / Run at four phases with the barrow pitched onto its wheel
    (the legs' clearance and the wheel's spin printed per cell: the batten cross turns with the stride)."""
    rig = B.rig
    bo = next(o for o, _ in B.items.values())
    vis = [m for m in rv.meshes if not m.hide_render]
    rv.facing("side")
    plan, allpts = [], []
    for name, ph in BARROW_SHEET_KEYS:
        c = clips[name]
        fr = int(round(ph * (c.frames - 1 if c.loop else c.frames)))
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        allpts.append(review.mesh_points(vis, step=4))
        plan.append((name, fr))
    pts = np.concatenate(allpts)
    review.setup_render(res, 8)
    review.frame_points(pts, res, fov_h=22, pitch_ue=pitch, fill=0.94)
    rv.ink(at=Vector(pts.mean(0)))
    cells = []
    for name, fr in plan:
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        img = review.render_array()
        low = barrow_leg_low(bo)
        pitch_deg, spin = B.anims.barrow_state(name, scene.frame_to_ms(fr))
        review.draw_text(img, 3, 3, f"{name} F{fr}")
        review.draw_text(img, 3, img.shape[0] - 20, f"LEGS {low:+.1f}CM  PITCH {pitch_deg:.1f}",
                         rgb=(255, 220, 140))
        review.draw_text(img, 3, img.shape[0] - 10, f"WHEEL {spin % 360.0:.0f} DEG", rgb=(200, 200, 255))
        cells.append(img)
    anim.set_frame(0)
    rv.facing("se")
    rv._save("barrow.png", review.grid(cells, 5))


_plain_render_previews = S.render_previews


def render_previews(B, clips, actions, out):
    """``npc_ship.render_previews`` + Old Bern's ``barrow.png`` (the lift and the wheel, in profile)."""
    files, ink = _plain_render_previews(B, clips, actions, out)
    if B.npc.look.item == "barrow":
        rv = S.make_review(B, out)
        acts = dict(zip([c.name for c in clips], actions))
        barrow_sheet(B, rv, acts, {c.name: c for c in clips})
        files += [str(p) for p, _ in rv.written]
        print(f"  barrow.png (profile: legs / wheel per Walk / Run phase)")
    return files, ink


def patch_npc_ship() -> None:
    """Point ``npc_ship``'s pipeline (ship, previews, QA, verification) at the march build in this process."""
    march_stage()
    S.build = build
    S.attach = attach
    S.anims = anims
    S.glows_for = glows_for
    S.game_cells = game_cells
    S.qa_run = qa_run
    S.render_previews = render_previews
    S.QA_LIMITS.update({"gripMm": GRIP_MM, "legLiftShortCm": 0.0, "legLiftOverCm": 0.0})
    anim.bake = bake


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def ship(npc, previews: bool = True, verify: bool = True) -> dict:
    patch_npc_ship()
    res = S.ship(npc, previews=previews, verify=verify)
    man = export.Manifest()
    e = man.data["assets"].get(npc.asset)
    if e is not None:
        e["march"] = {
            "theme": "march", "paletteNames": f"{npc.prefix}.*", "dress": list(npc.look.dress),
            "redresses": f"SK_NPC_{npc.base_id}" if npc.base_id else None,
            "note": "A9 re-dress (Docs/dark-remake/chapter1.md §3): colours from march_palettes.NPC_LOOKS / "
                    "NPC_KIT; a themed game id resolves before the plains one in a `march` zone (kit.theme, U1); the "
                    "portrait of a re-dressed role is T_UI_Portrait_<Role>_March (the UI looks portraits up by the "
                    "speaker id: U1 resolves the theme suffix first)",
        }
        if npc.look.item == "barrow":
            e["attachmentNote"] = ("SM_NPCKit_Prop_Barrow hangs on socket `barrow` on bone barrow_01 (head at the "
                                   "wheel's axle, identity; the prop is modelled in that bone's frame): every clip "
                                   "keys barrow_01 — resting on its two legs (pitch 0) in IdleNPC / Idle / "
                                   "Work_Barrow / Alert / Talk / IdleNPC_Fidget / Hurt / Death, pitched up "
                                   f"{MPR.lift_pitch():.1f} deg about the axle in Walk / Run (the legs ~10 cm off the "
                                   "ground, a 0.6 deg bob per step) with the grips (sockets handle_l / handle_r) in "
                                   "his fists; the runtime's clip blend between them (AnimTiming, ~150 ms) lifts and "
                                   "sets it down. The wheel is part of this skeletal mesh, rigid on barrow_wheel_01 "
                                   "(child of barrow_01, along the axle), which spins at stride speed in Walk (1 turn "
                                   "per 1.2 m loop) and Run (1.5 turns per 2.0 m loop) — no runtime spin needed; the "
                                   "blob shadow covers only Bern (a second contact under the barrow is the "
                                   "runtime's)")
            e["barrowRig"] = {"bone": "barrow_01", "wheelBone": "barrow_wheel_01", "liftDeg": round(MPR.lift_pitch(), 2),
                              "liftClips": sorted(MarchAnims.LIFT_CLIPS), "wheelRadiusCm": MPR.BARROW["wheel_r"] * 100,
                              "wheelTurnsPerLoop": {k: v[1] for k, v in MarchAnims.LIFT_CLIPS.items()},
                              "qa": "BarrowQA: gripMm <= %.0f (fists on the handle sockets), Walk / Run legs %.1f-%.1f cm "
                                    "off the ground, other clips on it (+-0.8 cm)" % (GRIP_MM, *LIFT_RANGE)}
        man.set_asset(npc.asset, e)
        man.save()
    return res


def tri_report(body, top: int = 40) -> None:
    """Toon / hull triangles per part name (budget work)."""
    me = body.data
    names = json.loads(body["af_parts"])
    nf = len(me.polygons)
    part = np.zeros(nf, np.int32)
    me.attributes["af_part"].data.foreach_get("value", part)
    mi = np.zeros(nf, np.int32)
    me.polygons.foreach_get("material_index", mi)
    agg: dict = {}
    for pid, m in zip(part, mi):
        nm = names[pid] if pid < len(names) else "?"
        nm = nm[:-5] if nm.endswith("_hull") else nm
        a = agg.setdefault(nm, [0, 0])
        a[m] += 1
    rows = sorted(agg.items(), key=lambda kv: -(kv[1][0] + kv[1][1]))[:top]
    print("  parts (toon + hull): " + ", ".join(f"{k} {v[0]}+{v[1]}" for k, v in rows))


# ── dev modes ───────────────────────────────────────────────────────────────────────────────────────────
def quick(npc, out: Path, pose: str | None = None, facing: str = "se") -> None:
    patch_npc_ship()
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
    rv = S.make_review(B, out)
    rv.save_as = f"closeup_{facing}.png"
    rv.closeup(facing)
    rv.save_as = "closeup_front.png"
    rv.closeup("front", pitch=-30.0)
    rv.turnaround()
    rv.game_view()
    print(f"[quick] {npc.asset}: {M.tri_count(B.body, 0)} toon + {M.tri_count(B.body, 1)} hull tris, "
          f"bones {len(B.rig.obj.data.bones)}")
    tri_report(B.body)
    for p, s in rv.written:
        print(f"  {p} {s // 1024} KB")


def anim_dev(npc, out: Path, names: list, facing: str = "se", game: bool = False) -> None:
    patch_npc_ship()
    out.mkdir(parents=True, exist_ok=True)
    B = build(npc)
    A = anims(B)
    attach(B)
    rv = S.make_review(B, out)
    acts, cl = {}, {}
    for c in A.all_clips():
        if names and c.name not in names:
            continue
        t1 = time.time()
        a = anim.bake(B.rig, c, npc.token)
        acts[c.name], cl[c.name] = a, c
        print(f"[bake] {c.name} {c.frames}f {time.time() - t1:.1f}s")
        tag = "" if facing == "se" else f"_{facing}"
        if not game:
            rv.save_as = f"anim_{c.name}{tag}.png"
            rv.contact_sheet(B.rig, [(c, a)], frames=8, facing=facing)
            print(f"  {out / f'anim_{c.name}{tag}.png'}")
    if game:
        keys = []
        for c in cl.values():
            for f in (0, c.frames // 3, (2 * c.frames) // 3):
                keys.append((c.name, f, facing))
        game_cells(B, rv, acts, cl, keys, f"game_{facing}.png")
        print(f"  {out / f'game_{facing}.png'}")


def barrow_dev(npc, out: Path) -> None:
    patch_npc_ship()
    out.mkdir(parents=True, exist_ok=True)
    B = build(npc)
    A = anims(B)
    attach(B)
    rv = S.make_review(B, out)
    clips = A.all_clips()
    acts = {c.name: anim.bake(B.rig, c, npc.token) for c in clips}
    barrow_sheet(B, rv, acts, {c.name: c for c in clips})
    print(f"  {out / 'barrow.png'}")


def inkmaps(npc, out: Path, keys: list) -> None:
    """Gated 1080p cells with their ink maps (every silhouette edge pixel green ≥ 3 px / red thinner), 3× zoom."""
    from kit import pngio
    patch_npc_ship()
    out.mkdir(parents=True, exist_ok=True)
    B = build(npc)
    A = anims(B)
    attach(B)
    rv = S.make_review(B, out)
    cl = {c.name: c for c in A.all_clips()}
    acts = {}
    for k in keys:
        name, fr, fac = k.split(":")
        if name not in acts:
            acts[name] = anim.bake(B.rig, cl[name], npc.token)
        game_cells(B, rv, acts, cl, [(name, int(fr), fac)], f"ink_{name}_{fr}_{fac}.png", cols=1)
        st = rv.ink_reports[-1]
        raw = review.render_array()
        del raw
        img = np.array(pngio.read_png(out / f"ink_{name}_{fr}_{fac}.png")) if hasattr(pngio, "read_png") else None
        if img is not None:
            m = review.ink_map(img[..., :3], st, 3)
            pngio.write_png(out / f"inkmap_{name}_{fr}_{fac}.png", m)
        print(f"  {k}: rim {st['fracRim'] * 100:.1f} %")


def qa_dev(npc, names: list) -> int:
    """Bake (the listed) clips and run the ship's per-frame QA (``npc_ship.qa_run``) with its limits."""
    import hero_base as HB
    patch_npc_ship()
    B = build(npc)
    A = anims(B)
    attach(B)
    clips = [c for c in A.all_clips() if not names or c.name in names]
    acts = [anim.bake(B.rig, c, npc.token) for c in clips]
    rep = S.qa_run(B, clips, acts)
    print(HB.PairQA.table(rep))
    lim = dict(S.QA_LIMITS)
    lim.update({f"x:{n[len('SM_NPCKit_Prop_'):]}": S.QA_ITEM_LIMIT for n in B.items})
    fails = HB.PairQA.verdict(rep, lim, S.QA_CLIP_LIMITS)
    for f in fails:
        print("  QA FAIL " + f)
    return 1 if fails else 0


def main(argv: list) -> int:
    args = [a for a in argv[1:] if not a.startswith("--")]
    flags = [a for a in argv[1:] if a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    what = args[0]
    if what in ("props", "dressing", "kit", "lineup"):
        import march_dressing as MD
        patch_npc_ship()
        return {"props": MD.ship_props, "dressing": MD.ship_dressing, "kit": MD.ship_kit,
                "lineup": MD.lineup}[what]()
    npcs = ML.NPCS if what == "all" else [ML.find(what)]
    fl = {f.split("=")[0]: (f.split("=", 1)[1] if "=" in f else True) for f in flags}
    if "--quick" in fl:
        for n in npcs:
            quick(n, SCRATCH / n.snake, pose=fl.get("--pose"), facing=fl.get("--facing", "se"))
        return 0
    if "--qa" in fl:
        return max(qa_dev(n, args[1:]) for n in npcs)
    if "--inkmap" in fl:
        for n in npcs:
            inkmaps(n, SCRATCH / n.snake, args[1:])
        return 0
    if "--barrow" in fl:
        for n in npcs:
            barrow_dev(n, SCRATCH / n.snake)
        return 0
    if "--anims" in fl or "--game" in fl:
        for n in npcs:
            anim_dev(n, SCRATCH / n.snake, args[1:], facing=fl.get("--facing", "se"), game="--game" in fl)
        return 0
    import mship
    rc = 0
    for n in npcs:
        res = ship(n, previews="--no-previews" not in fl, verify="--no-verify" not in fl)
        rc = max(rc, mship.main_result(res))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
