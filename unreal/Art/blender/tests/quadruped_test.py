"""Kit test of the quadruped rig family (``kit/skel_quadruped.py`` + ``monsters/qclips.py``), no mesh, no render.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/tests/quadruped_test.py   # ~2 s, exit 0

Checks: the bone set, hierarchy and hinge axes of ``SKEL_Quadruped``; its sockets; ``QuadrupedSpec.scaled`` (Volgan's
2.2); the solver (the neutral pose is the rest pose, a pose's basis is rigid, paws reach their targets inside the
legs' range and are clamped outside it); the gaits (four foot notifies, ref speed, no planted paw slides — at the
hound's size and at 2.2 ×); every library clip samples and bakes with the right frame count; the ground clamp.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "monsters"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402,F401  (mathutils ships inside the bpy module)
from mathutils import Vector  # noqa: E402

from kit import anim, scene, skel_quadruped as SQ  # noqa: E402
from qclips import QP, QuadAnims  # noqa: E402

FAILS: list[str] = []


def check(ok: bool, msg: str) -> None:
    print(("  ok   " if ok else "  FAIL ") + msg)
    if not ok:
        FAILS.append(msg)


def rig_checks(rig, k: float) -> None:
    arm = rig.obj.data
    names = [b.name for b in arm.bones]
    check(set(names) == set(SQ.CORE_BONES) | set(SQ.IK_BONES) and len(names) == 36, f"36 bones, the family's set ({len(names)})")
    par = {b.name: (b.parent.name if b.parent else None) for b in arm.bones}
    want = {"pelvis": "root", "spine_01": "pelvis", "spine_04": "spine_03", "neck_01": "spine_04", "head": "neck_02",
            "jaw": "head", "ear_l": "head", "tail_01": "pelvis", "tail_03": "tail_02", "foreleg_01_l": "spine_04",
            "foretoe_r": "foreleg_03_r", "hindleg_01_r": "pelvis", "hindtoe_l": "hindleg_03_l",
            "ik_foot_root": "root", "ik_foot_bl": "ik_foot_root"}
    check(all(par[b] == p for b, p in want.items()), "hierarchy (trunk chain, legs under spine_04 / pelvis, ik under root)")
    hinge = [abs((b.matrix_local.to_3x3() @ Vector((1, 0, 0))).x) for b in arm.bones
             if b.name not in ("root", "ik_foot_root") and not b.name.startswith("ear")]
    check(min(hinge) > 0.97, f"local X is the lateral hinge on every limb / trunk bone (min |x.X| {min(hinge):.3f})")
    socks = {s["name"]: s for s in rig.sockets}
    need = ("fx_feet", "fx_chest", "fx_head", "fx_overhead", "fx_elite", "fx_eye_l", "fx_eye_r", "mouth", "bite",
            "collar", "tail_tip")
    check(all(n in socks and socks[n]["bone"] in names for n in need), "sockets: " + ", ".join(need))
    check(socks["fx_overhead"]["bone"] == "root" and socks["fx_elite"]["bone"] == "root"
          and socks["fx_elite"]["pos"].z > socks["fx_overhead"]["pos"].z > rig.spec.withers,
          "fx_overhead / fx_elite ride root, above the withers")
    check(socks["mouth"]["bone"] == "head" and socks["bite"]["bone"] == "jaw" and socks["collar"]["bone"] == "neck_01",
          "mouth on head, bite on jaw, collar on neck_01")
    check(abs(rig.spec.crown_height() - max(0.75 * k, rig.joints["ear_tip_l"].z)) < 1e-6
          and abs(rig.joints["nose"].y - (-0.83 * k)) < 1e-6, f"landmarks scale with the spec (x {k:g})")
    ik = rig.obj.pose.bones["ik_foot_fl"].constraints
    check(len(ik) == 1 and ik[0].subtarget == "foretoe_l", "ik_foot_* copy the paws")


def solver_checks(A: QuadAnims, k: float) -> None:
    posed = A.solve(QP())
    worst = max((posed[n].translation - A.rest[n].translation).length for n in posed)
    trunk = max((posed[n] - A.rest[n]).to_3x3().col[1].length for n in SQ.SPINE + SQ.NECK + ("head", "jaw") + SQ.TAIL)
    check(trunk < 1e-6 and worst < 0.012 * k, f"the neutral pose is the rest pose (trunk exact, leg joints within "
                                              f"{worst * 1000:.1f} mm: the IK plane)")
    q = QP(hip=(0.02 * k, 0.05 * k, -0.06 * k), pitch=6.0, yaw=8.0, arch=10.0, bend=6.0, neck=(10.0, 12.0),
           head=(5.0, 9.0, 4.0), jaw=20.0, tail=(10.0, 8.0, 4.0), fl=(0.0, 0.10 * k, 0.05 * k), fold=(30.0, 0, 0, 0))
    b = A.basis(q)
    rigid = max(abs(m.to_3x3().determinant() - 1.0) for m in b.values())
    stretch = max(abs(m.to_scale().x - 1.0) + abs(m.to_scale().y - 1.0) + abs(m.to_scale().z - 1.0) for m in b.values())
    check(rigid < 1e-5 and stretch < 1e-5, "a pose's basis matrices are rigid (no scale, no shear)")
    A.solve(q)
    tgt = A.J["fore_ball_l"] + Vector((0.0, -0.10 * k, 0.05 * k))
    check((A.feet["fl"] - tgt).length < 1e-5 and A.reach_error == 0.0, "a paw inside its range reaches its target")
    conn = max((posed_child.translation - (posed_par @ Vector((0, A.rig.obj.data.bones[p].length, 0)))).length
               for c, p, posed_child, posed_par in ((c, p, A.solve(q)[c], A.solve(q)[p]) for c, p in
                                                    (("foreleg_02_l", "foreleg_01_l"), ("foreleg_03_l", "foreleg_02_l"),
                                                     ("hindleg_02_r", "hindleg_01_r"), ("hindtoe_r", "hindleg_03_r"),
                                                     ("spine_02", "spine_01"), ("head", "neck_02"))))
    check(conn < 1e-5, "connected bones stay connected (bone lengths kept)")
    A.solve(QP(fl=(0.0, 0.9 * k, 0.0)))
    check(A.reach_error > 0.2 * k, "an out-of-reach target is clamped and reported (reach_error)")
    low = A.lowest(QP(hip=(0, 0, -0.62 * k), follow=1.0)) if A.probes else None
    if A.probes:
        g = A.grounded(QP(hip=(0, 0, -0.62 * k), follow=1.0))
        check(low < -0.05 and A.lowest(g) >= 0.0, f"grounded() lifts a sunk body back onto the ground ({low * 100:.0f} cm -> "
                                                 f"{A.lowest(g) * 100:.1f} cm)")


def clip_checks(A: QuadAnims, k: float, token: str) -> None:
    clips = [A.idle(), A.idle_sniff(), A.walk(), A.run(), A.attack(), A.pounce(), A.howl(), A.hurt(), A.death(),
             A.stun()]
    names = [c.name for c in clips]
    check(names == ["Idle", "Idle_Sniff", "Walk", "Run", "Attack01", "Pounce", "Cast01", "Hurt", "Death", "Stun"],
          "the library's clips: " + ", ".join(names))
    for c in clips:
        if getattr(c, "duty_of", None):
            n = {d["name"] for d in c.notify_list()}
            slide = A.slide_report(c)
            check(n == {"FootFL", "FootFR", "FootBL", "FootBR"} and abs(c.ref_speed - c.gait_speed) < 1e-9
                  and slide <= 0.002 * k,
                  f"{c.name}: four foot notifies, ref {c.ref_speed:g} m/s, planted paws slide {slide * 1000:.2f} mm")
    nt = {c.name: {d["name"]: d["ms"] for d in c.notify_list()} for c in clips}
    check(nt["Attack01"]["Contact"] == 320.0 and abs(nt["Attack01"]["WindupEnd"] - 198.4) < 1e-6,
          "Attack01: Contact 320, WindupEnd 62 % of it")
    check(nt["Pounce"] == {"WindupEnd": 403.0, "Leap": 380.0, "FX_PounceLeap": 380.0, "Contact": 650.0,
                           "FX_PounceLand": 650.0}, "Pounce: Leap 380, Contact 650 (+ FX notifies)")
    check(nt["Cast01"]["Contact"] == 1000.0 and nt["Cast01"]["FX_Howl"] == 1000.0, "Cast01 (howl): Contact 1000")
    pounce = next(c for c in clips if c.name == "Pounce")
    A.solve(pounce.pose_fn(520.0))
    air = min(p.z for p in A.feet.values()) - A.J["fore_ball_l"].z
    A.solve(pounce.pose_fn(650.0))
    fore = max(A.feet["fl"].z, A.feet["fr"].z) - A.J["fore_ball_l"].z
    check(air > 0.05 * k and fore < 1e-4, f"Pounce: airborne between Leap and Contact ({air * 100:.0f} cm), the fores "
                                          "down on Contact")
    # review 4R-1: a planted paw is never dragged over the ground in a one-shot — a paw that moves is picked up
    # (the bite's recovery, the pounce's landing); Death is the body going down, not a stand
    z0 = {"fl": A.J["fore_ball_l"].z, "fr": A.J["fore_ball_r"].z, "bl": A.J["hind_ball_l"].z, "br": A.J["hind_ball_r"].z}
    for c in clips:
        if c.loop or c.name == "Death":
            continue
        drag = dict.fromkeys(z0, 0.0)
        prev = None
        for f in range(c.frames + 1):
            A.solve(c.pose_fn(min(scene.frame_to_ms(f), c.length_ms)))
            cur = {g: v.copy() for g, v in A.feet.items()}
            if prev is not None:
                for g in z0:
                    if cur[g].z - z0[g] < 0.002 * k and prev[g].z - z0[g] < 0.002 * k:
                        drag[g] += (cur[g] - prev[g]).to_2d().length
            prev = cur
        worst = max(drag.values())
        check(worst <= 0.015 * k, f"{c.name}: no planted paw is dragged (worst {worst * 100:.1f} cm on the ground)")
    # the gaits again, from the baked action (what the FBX carries), independent of the solver's own report
    toes = {"fl": "foretoe_l", "fr": "foretoe_r", "bl": "hindtoe_l", "br": "hindtoe_r"}
    for c in clips:
        if not getattr(c, "duty_of", None):
            continue
        act = anim.bake_basis(A.rig, c, token, c.basis_fn)
        anim.assign(A.rig, act)
        arm = A.rig.obj
        pos = []
        for f in range(c.frames + 1):
            bpy.context.scene.frame_set(f)
            bpy.context.view_layer.update()
            pos.append({g: (arm.matrix_world @ arm.pose.bones[b].head).copy() for g, b in toes.items()})
        worst = 0.0
        for g, (ph0, duty) in c.duty_of.items():
            for f in range(c.frames):
                t0, t1 = scene.frame_to_ms(f), scene.frame_to_ms(f + 1)
                if ((t0 / c.length_ms) - ph0) % 1.0 + (t1 - t0) / c.length_ms <= duty and t1 <= c.length_ms:
                    d = pos[f + 1][g] - pos[f][g]
                    worst = max(worst, abs(d.y - c.gait_speed * (t1 - t0) / 1000.0), abs(d.z), abs(d.x))
        check(worst <= 0.002 * k, f"{c.name} baked: planted paws move back at the ref speed frame by frame "
                                  f"(worst {worst * 1000:.2f} mm)")
        if c.name == "Run":
            stand = abs(A.J["fore_ball_l"].y - A.J["hind_ball_l"].y)
            ext = max((max(-q["fl"].y, -q["fr"].y) - min(-q["bl"].y, -q["br"].y)) for q in pos
                      if min(v.z - z0[g] for g, v in q.items()) > 0.01 * k)
            check(ext > stand + 0.25 * k, f"Run: an extended flight frame (all four paws up, fore to hind "
                                          f"{ext * 100:.0f} cm against the stand's {stand * 100:.0f} cm)")
        anim.assign(A.rig, None)
    loops = [c for c in clips if c.loop]
    seam = max(max((A.basis(c.pose_fn(0.0))[n] - A.basis(c.pose_fn(c.length_ms))[n]).to_3x3().col[0].length
                   for n in ("pelvis", "head", "foreleg_01_l", "hindleg_02_r")) for c in loops)
    check({c.name for c in loops} == {"Idle", "Walk", "Run", "Stun"} and seam < 1e-6, "Idle / Walk / Run / Stun loop "
                                                                                    "seamlessly")
    for c in (clips[2], clips[5]):
        act = anim.bake_basis(A.rig, c, token, c.basis_fn)
        check(int(act.frame_end) == scene.ms_to_frame(c.length_ms) and len(act.pose_markers) == len(c.notify_list()),
              f"{c.name} bakes: {int(act.frame_end)} frames, {len(act.pose_markers)} markers")


def main() -> int:
    for k, label in ((1.0, "the hound's size"), (2.2, "x 2.2 (Volgan's actor scale as a rig)")):
        print(f"[quadruped] {label}")
        scene.reset()
        spec = SQ.QuadrupedSpec(name=f"SK_Test_Quad_{int(k * 10)}")
        rig = SQ.build_quadruped(spec if k == 1.0 else spec.scaled(k))
        rig_checks(rig, k)
        A = QuadAnims(rig, QP(), run_speed=3.75 * k, walk_speed=1.30 * k)
        if k == 1.0:
            J = rig.joints
            A.probes = tuple((f"{kind}toe_{s}", tuple(J[f"{nm}_{s}"]), 0.03, 0.03)
                             for kind, nm in (("fore", "fore_ball"), ("hind", "hind_ball")) for s in ("l", "r")) + \
                (("spine_02", (0.0, 0.1, 0.62), 0.08, 0.11),)
        solver_checks(A, k)
        clip_checks(A, k, f"Test_Quad_{int(k * 10)}")
    print(f"[quadruped] {'PASS' if not FAILS else 'FAIL'} ({len(FAILS)} failures)")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
