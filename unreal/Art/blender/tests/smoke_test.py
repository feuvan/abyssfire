"""Kit smoke test: build a test character, export FBX, re-import into an empty scene, verify, render previews.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/tests/smoke_test.py [--no-previews] [--keep]

* FBX / manifest / palette registry go to a temporary export root (never the committed ``Art/Export``);
  it is deleted at the end unless ``--keep``.
* Previews go to ``Art/Previews/SK_Test_KitKnight/`` (review them!) — override with ``AF_PREVIEW_ROOT``.
* Exit code 0 = every check passed.

The test knight is a compact cousin of the warrior (spec §3.1): plate + crimson tabard and cape, plumed great
helm with an ember T-visor, broadsword on ``weapon_r``, heater shield on ``weapon_l``; clips Idle / Run /
Attack01 with the warrior's timings (1000 / 700 / 615 ms, Contact 308).
"""
from __future__ import annotations

import json
import math
import os
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

TMP = Path(tempfile.mkdtemp(prefix="af_smoke_"))
os.environ.setdefault("AF_EXPORT_ROOT", str(TMP / "Export"))
os.environ.setdefault("AF_PALETTE_ROOT", str(TMP / "palettes"))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit  # noqa: E402
from kit import anim, asset, export, mesh as M, outline, paths, review, rig as R, scene, shading  # noqa: E402
from kit.anim import Clip, Key, Pose  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

ASSET = "SK_Test_KitKnight"
TOKEN = "Test_KitKnight"
CATEGORY = "Characters"
U = 0.0301            # warrior rig unit → metres (spec §0.1: 3.01 cm / unit)
FAIL: list[str] = []


def check(cond: bool, msg: str) -> None:
    print(("  ok   " if cond else "  FAIL ") + msg)
    if not cond:
        FAIL.append(msg)


# ── character ───────────────────────────────────────────────────────────────────────────────────────────
def build_knight():
    pal = Palette("Test")
    cape_pts = None
    spec = HumanoidSpec(name=ASSET, skeleton="SKEL_Human", head=0.44).fit_height(1.76)
    # cape chain hangs from the upper back (secondary-motion chain cape_c_01..03, spec §2.4)
    nz = spec.ankle + spec.shin + spec.thigh + spec.pelvis_offset + spec.torso
    cape_pts = [Vector((0, 0.12, nz - 0.03)), Vector((0, 0.17, nz - 0.30)), Vector((0, 0.21, nz - 0.56)),
                Vector((0, 0.25, nz - 0.80))]
    spec.chains = [("cape_c", "spine_03", cape_pts)]
    rig = R.build_humanoid(spec)
    J = rig.joints
    b = M.Builder(ASSET, pal, prefix="knight")
    b.regions(
        steel=Region("#9AA6BA", .42, .45), iron=Region("#4B5366"), gold=Region("#D9A640", .42, .50),
        crimson=Region("#A82230"), crimson_in=Region("#62131D", .42, .15), leather=Region("#5E3A22"),
        visor=Region("#0C0A12", .30, .10), ember=Region("#FF8A2A", e=1.0), sigil=Region("#FF8A2A", e=0.85),
    )
    pz, nzr = J["pelvis"].z, J["neck"].z
    hip_z = J["hip_l"].z
    hc = J["head_center"]

    # torso: cuirass (steel) with a slight forward chest
    b.add(M.loft_z([(pz - 0.05, 0.150, 0.112, 0, 0.0), (pz + 0.07, 0.143, 0.106, 0, -0.004),
                    (pz + 0.21, 0.172, 0.126, 0, -0.012), (pz + 0.35, 0.190, 0.136, 0, -0.016),
                    (nzr - 0.05, 0.170, 0.122, 0, -0.008), (nzr + 0.015, 0.085, 0.075, 0, 0.0)],
                   sides=18, exp=2.3, dome1=0.0), "steel", Bind.blend("pelvis", "spine_01", "spine_02", "spine_03"),
          name="cuirass")
    # breastplate ridge (light crease) + ember sigil
    b.add(M.strap([(0, -0.118, pz + 0.10), (0, -0.140, pz + 0.24), (0, -0.150, pz + 0.36)], 0.012, 0.012,
                  up=(0, -1, 0)), "steel", Bind.blend("spine_01", "spine_02", "spine_03"), name="ridge")
    flame = [(0.0, -0.045), (0.022, -0.02), (0.026, 0.01), (0.012, 0.03), (0.008, 0.012), (0.0, 0.045),
             (-0.008, 0.012), (-0.012, 0.03), (-0.026, 0.01), (-0.022, -0.02)]
    b.add(M.plate(flame, 0.008).rotate("x", 90).translate((0.0, -0.149, pz + 0.29)), "sigil",
          Bind.rigid("spine_02"), name="sigil", outline=False)
    # gorget + mail skirt + belt + buckle
    b.add(M.belt(0.088, 0.078, nzr - 0.02, 0.05, 0.022, sides=16), "iron", Bind.blend("spine_03", "neck_01"),
          name="gorget")
    b.add(M.loft_z([(hip_z - 0.16, 0.20, 0.155), (hip_z - 0.06, 0.183, 0.138), (pz - 0.03, 0.158, 0.118)],
                   sides=18, exp=2.2, cap0=None), "iron", Bind.blend("pelvis", "thigh_l", "thigh_r", smooth=0.05),
          name="mailskirt")
    b.add(M.belt(0.153, 0.115, pz - 0.035, 0.05, 0.02, sides=18), "leather", Bind.rigid("pelvis"), name="belt")
    b.add(M.box((0.06, 0.02, 0.05), (0, -0.138, pz - 0.035), bevel=0.006), "gold", Bind.rigid("pelvis"),
          name="buckle")
    # tabard (front, crimson) with a gold hem
    tab = M.sheet(0.20, 0.25, 0.34, thickness=0.012, cols=6, rows=5, wrap_radius=0.30, flare=0.04)
    b.add(tab.translate((0, -0.133, pz - 0.06)), "crimson", Bind.blend("pelvis", "thigh_l", "thigh_r", smooth=0.08),
          name="tabard")
    # cape: one two-sided sheet — crimson outside, crimson_in lining on the body side (af_sub 1 = front face)
    cape = M.sheet(0.34, 0.50, 0.80, thickness=0.016, cols=7, rows=8, wrap_radius=-0.36, flare=-0.10,
                   hem_wave=0.015, hem_waves=2.5)
    cape.translate((0, 0.128, nzr - 0.02))
    b.add(cape, "crimson", Bind.blend("spine_03", "cape_c_01", "cape_c_02", "cape_c_03", falloff=3.0), name="cape",
          sub_regions={1: "crimson_in"})
    # great helm (steel) + gold brow band + T visor + ember + plume
    hz = hc.z
    b.add(M.loft_z([(hz - 0.17, 0.140, 0.150), (hz - 0.10, 0.168, 0.188), (hz - 0.01, 0.178, 0.198),
                    (hz + 0.09, 0.170, 0.190), (hz + 0.16, 0.135, 0.150), (hz + 0.205, 0.075, 0.085)],
                   sides=18, exp=2.15, cap0="fan", dome0=-0.02, dome1=0.018), "steel", Bind.rigid("head"),
          name="helm")
    b.add(M.belt(0.177, 0.197, hz + 0.045, 0.035, 0.012, sides=18, exp=2.15), "gold", Bind.rigid("head"),
          name="browband")
    b.add(M.box((0.19, 0.03, 0.028), (0, -0.192, hz - 0.005), bevel=0.006), "visor", Bind.rigid("head"),
          name="visor_h", outline=False)
    b.add(M.box((0.032, 0.03, 0.10), (0, -0.194, hz - 0.06), bevel=0.006), "visor", Bind.rigid("head"),
          name="visor_v", outline=False)
    b.add(M.box((0.13, 0.012, 0.011), (0, -0.206, hz - 0.005), bevel=0.003), "ember", Bind.rigid("head"),
          name="ember", outline=False)
    crown = hz + 0.22
    b.add(M.tube([(0, -0.05, crown - 0.05), (0, 0.02, crown + 0.02), (0, 0.13, crown + 0.03),
                  (0, 0.23, crown - 0.03), (0, 0.30, crown - 0.14)],
                 [(0.03, 0.02), (0.05, 0.028), (0.048, 0.026), (0.036, 0.02), (0.012, 0.008)], sides=8,
                 up=(0, 0, 1)), "crimson", Bind.rigid("head"), name="plume")
    # shoulders: pauldrons (steel top lame, iron lower lame, gold edge)
    for side, sx in (("l", 1), ("r", -1)):
        S = J[f"shoulder_{side}"]
        top = M.ellipsoid((0.115, 0.125, 0.075), sides=14, rings=7).rotate("y", -24 * sx)
        b.add(top.translate(S + Vector((0.03 * sx, 0, 0.035))), "steel", Bind.rigid(f"upperarm_{side}"),
              name=f"pauldron_{side}")
        low = M.ellipsoid((0.10, 0.11, 0.05), sides=12, rings=6).rotate("y", -40 * sx)
        b.add(low.translate(S + Vector((0.07 * sx, 0, -0.035))), "iron", Bind.rigid(f"upperarm_{side}"),
              name=f"lame_{side}")
        E, W = J[f"elbow_{side}"], J[f"wrist_{side}"]
        b.add(M.tube([S, E], [0.062, 0.055], sides=10), "iron", Bind.blend(f"upperarm_{side}", f"lowerarm_{side}"),
              name=f"upperarm_{side}")
        b.add(M.ellipsoid((0.052, 0.052, 0.052), E, sides=10, rings=6), "steel",
              Bind.blend(f"upperarm_{side}", f"lowerarm_{side}"), name=f"elbow_{side}")
        d = (W - E).normalized()
        b.add(M.tube([E + d * 0.03, W - d * 0.02, W + d * 0.012], [0.054, 0.058, 0.064], sides=10, dome1=0.0),
              "steel", Bind.blend(f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"), name=f"vambrace_{side}")
        hand = M.mitten(0.13, 0.085, 0.062, side=side, curl_deg=35)
        b.add(hand.transform(R.frame(W, J[f"handtip_{side}"] - W, (0, -1, 0))), "iron",
              Bind.blend(f"hand_{side}", f"fingers_01_{side}"), name=f"gauntlet_{side}")
        # legs
        Hp, K, A = J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"]
        b.add(M.tube([Hp + Vector((0, 0, 0.04)), K], [0.086, 0.066], sides=11), "iron",
              Bind.blend("pelvis", f"thigh_{side}", f"calf_{side}"), name=f"thigh_{side}")
        b.add(M.tube([K, A + Vector((0, 0, 0.05))], [0.066, 0.052], sides=11), "steel",
              Bind.blend(f"thigh_{side}", f"calf_{side}", f"foot_{side}"), name=f"greave_{side}")
        kc = M.ellipsoid((0.058, 0.05, 0.062), K + Vector((0, -0.035, 0)), sides=10, rings=6)
        b.add(kc, "steel", Bind.blend(f"thigh_{side}", f"calf_{side}"), name=f"kneecop_{side}")
        b.add(M.ellipsoid((0.012, 0.01, 0.012), K + Vector((0, -0.088, 0.004)), sides=6, rings=4), "gold",
              Bind.rigid(f"calf_{side}"), name=f"rivet_{side}", outline=False)
        boot = M.boot(spec.foot_len, 0.118, 0.105, shaft=0.035)
        b.add(boot.translate((A.x, 0, 0)), "iron", Bind.blend(f"calf_{side}", f"foot_{side}", f"ball_{side}"),
              name=f"sabaton_{side}")
    body = asset.finish_mesh(b, pal, "hero", rig=rig, grounding_height=spec.crown_height())
    return pal, rig, body


def build_sword(pal):
    b = M.Builder("SM_Test_KitKnight_Sword", pal, prefix="knight_sword")
    b.regions(blade=Region("#C9D3E2", .30, .60), fuller=Region("#465064", .30, .2), gold=Region("#D9A640", .42, .5),
              leather=Region("#5E3A22"), gem=Region("#FF7A26", e=1.0))
    blade = [(-0.03, 0.0), (0.03, 0.0), (0.026, 0.66), (0.0, 0.80), (-0.026, 0.66)]
    b.add(M.plate(blade, 0.011, bevel=0.003).rotate("y", 90).translate((0, 0.08, 0)), "blade")
    b.add(M.plate([(-0.004, 0.0), (0.004, 0.0), (0.004, 0.52), (-0.004, 0.52)], 0.0125).rotate("y", 90)
          .translate((0, 0.13, 0)), "fuller", outline=False)
    b.add(M.tube([(0, 0.10, -0.15), (0, 0.072, -0.07), (0, 0.068, 0), (0, 0.072, 0.07), (0, 0.10, 0.15)],
                 [0.012, 0.016, 0.019, 0.016, 0.012], sides=8), "gold")
    b.add(M.tube([(0, -0.065, 0), (0, 0.06, 0)], [0.018, 0.017], sides=8), "leather")
    b.add(M.ellipsoid((0.028, 0.028, 0.028), (0, -0.088, 0), sides=10, rings=6), "gold")
    b.add(M.ellipsoid((0.012, 0.01, 0.012), (0.024, -0.088, 0), sides=6, rings=4), "gem", outline=False)
    b.add(M.ellipsoid((0.012, 0.01, 0.012), (-0.024, -0.088, 0), sides=6, rings=4), "gem", outline=False)
    obj = asset.finish_mesh(b, pal, "weapon")
    return obj, [{"name": "tip", "pos": (0, 0.88, 0)}, {"name": "mid", "pos": (0, 0.48, 0)}]


def build_shield(pal):
    b = M.Builder("SM_Test_KitKnight_Shield", pal, prefix="knight_shield")
    b.regions(field=Region("#A82230"), gold=Region("#D9A640", .42, .5), core=Region("#FFCF6B", .3, .5, e=0.5),
              back=Region("#363C4B"))
    heater = [(-0.18, -0.30), (0.18, -0.30), (0.18, -0.05), (0.13, 0.12), (0.06, 0.24), (0.0, 0.30),
              (-0.06, 0.24), (-0.13, 0.12), (-0.18, -0.05)]
    field = M.plate(heater, 0.024, bevel=0.004).bend(-28, 0.18, along="x", toward="z")
    rim = M.plate([(x * 1.07, y * 1.06 + 0.002) for x, y in heater], 0.018, bevel=0.004)
    rim.bend(-28, 0.19, along="x", toward="z").translate((0, 0, -0.008))
    flame = [(0.0, -0.13), (0.06, -0.07), (0.07, 0.0), (0.04, 0.07), (0.025, 0.02), (0.0, 0.12),
             (-0.025, 0.02), (-0.04, 0.07), (-0.07, 0.0), (-0.06, -0.07)]
    emb = M.plate([(x, -y) for x, y in reversed(flame)], 0.008).translate((0, 0.0, 0.016))
    core = M.plate([(x * 0.4, -y * 0.4 + 0.02) for x, y in reversed(flame)], 0.006).translate((0, 0.0, 0.022))
    for p, reg, ol in ((field, "field", True), (rim, "gold", True), (emb, "gold", False), (core, "core", False)):
        p.rotate("x", -90).translate((0, 0.075, 0.02))
        b.add(p, reg, outline=ol)
    b.add(M.box((0.05, 0.03, 0.16), (0, 0.045, 0.02), bevel=0.008), "back")
    obj = asset.finish_mesh(b, pal, "weapon")
    return obj


# ── poses and clips ─────────────────────────────────────────────────────────────────────────────────────
def wpn_dir(deg: float, lateral: float = 0.0) -> Vector:
    """Web weapon angle (0 = up, + = forward) → character-space direction, ``lateral`` toward −X (right)."""
    a = math.radians(deg)
    return Vector((-lateral, -math.sin(a), math.cos(a))).normalized()


def make_poses(rig):
    J = rig.joints
    lib = anim.PoseLib(rig)

    def ready() -> Pose:
        p = Pose(rig)
        p.move("pelvis", (0, 0.01, -0.035))
        p.rot("spine_01", pitch=2).rot("spine_02", pitch=1.5).rot("neck_01", pitch=-1).rot("head", pitch=-2)
        p.rot("pelvis", yaw=-6)
        p.foot("l", offset=(0.02, -0.15, 0.0)).foot("r", offset=(-0.03, 0.14, 0.0))
        p.hand("r", at=(-0.27, -0.20, 0.90))
        p.aim("weapon_r", wpn_dir(112, 0.25))
        p.hand("l", at=(0.08, -0.30, 1.08), pole=(1.0, 0.6, -0.4))
        p.aim("weapon_l", (-0.29, -0.956, 0.0))
        p.rot("clavicle_l", yaw=-6)
        p.rot("cape_c_01", pitch=-3).rot("cape_c_02", pitch=-2)
        return p
    lib.add("READY", ready())
    br = ready()
    br.move("pelvis", (0, 0, -0.016))
    br.rot("spine_02", pitch=-1.2).rot("spine_03", pitch=-0.8).rot("head", pitch=1.7)
    br.hand("r", at=(-0.27, -0.20, 0.885)).aim("weapon_r", wpn_dir(114, 0.25))
    br.hand("l", at=(0.08, -0.30, 1.068), pole=(1.0, 0.6, -0.4))
    br.rot("cape_c_02", pitch=-2).rot("cape_c_03", pitch=-2)
    lib.add("READY_BREATH", br)

    def lean(p: Pose, deg: float) -> Pose:
        p.rot("spine_01", pitch=deg * 0.35).rot("spine_02", pitch=deg * 0.35).rot("spine_03", pitch=deg * 0.3)
        return p
    w = ready()
    w.move("pelvis", (0, 3 * U, -2 * U))
    lean(w, -14).rot("head", pitch=6)
    w.foot("l", offset=(0.02, -0.13, 0.0)).foot("r", offset=(-0.03, 0.17, 0.0))
    w.hand("r", at=(-0.17, 0.10, 1.80), pole=(-0.6, 0.3, 0.2)).aim("weapon_r", wpn_dir(-52, 0.0))
    w.hand("l", at=(0.20, -0.18, 1.10), pole=(1.0, 0.5, -0.4)).aim("weapon_l", (-0.1, -1.0, 0.0))
    w.rot("cape_c_01", pitch=4)
    lib.add("ATK_WINDUP", w)
    s = ready()
    s.move("pelvis", (0, 0.03, -0.05))
    lean(s, 2)
    s.foot("l", offset=(0.02, -0.20, 0.03)).foot("r", offset=(-0.03, 0.16, 0.0))
    s.hand("r", at=(-0.10, -0.30, 1.68), pole=(-0.7, 0.4, 0.0)).aim("weapon_r", wpn_dir(43, 0.1))
    s.hand("l", at=(0.22, -0.12, 1.05), pole=(1.0, 0.5, -0.4))
    lib.add("ATK_SWING", s)
    c = ready()
    c.move("pelvis", (0, -4 * U, -0.075))
    lean(c, 23).rot("head", pitch=-12)
    c.foot("l", offset=(0.02, -0.15 - 13 * U, 0.0)).foot("r", offset=(-0.03, 0.15, 0.0))
    c.hand("r", at=(-0.08, -0.58, 1.12), pole=(-0.6, 0.6, -0.2)).aim("weapon_r", wpn_dir(129, 0.25))
    c.hand("l", at=(0.28, -0.02, 0.98), pole=(1.0, 0.6, -0.3)).aim("weapon_l", (-0.5, -0.86, 0.0))
    c.rot("cape_c_01", pitch=-12).rot("cape_c_02", pitch=-10).rot("cape_c_03", pitch=-8)
    lib.add("ATK_CONTACT", c)
    f = c.copy()
    lean(f, 2)
    f.hand("r", at=(-0.16, -0.48, 0.86), pole=(-0.6, 0.6, -0.2)).aim("weapon_r", wpn_dir(143, 0.35))
    lib.add("ATK_FOLLOW", f)
    r = ready()
    r.move("pelvis", (0, -0.03, -0.05))
    lean(r, 11)
    r.foot("l", offset=(0.02, -0.15 - 8 * U, 0.0))
    r.hand("r", at=(-0.24, -0.32, 0.90)).aim("weapon_r", wpn_dir(118, 0.3))
    lib.add("ATK_RECOVER", r)
    return lib


def make_clips(rig, lib):
    idle = Clip("Idle", 1000.0, loop=True, keys=[
        Key(lib["READY"], t=0.0), Key(lib["READY_BREATH"], t=0.5), Key(lib["READY"], t=1.0)])
    atk = Clip("Attack01", 615.0, key_span_ms=538.0, notifies={"Contact": 308.0}, keys=[
        Key(lib["READY"], t=0.0),
        Key(lib["ATK_WINDUP"], t=0.28, ease="out"),
        Key(lib["ATK_SWING"], t=0.43, ease="in"),
        Key(lib["ATK_CONTACT"], t=0.571, ease="linear"),
        Key(lib["ATK_FOLLOW"], t=0.71, ease="out"),
        Key(lib["ATK_RECOVER"], t=0.86),
        Key(lib["READY"], t=1.0)])
    cycle = 700.0

    def run_pose(t_ms: float) -> Pose:
        t = (t_ms / cycle) % 1.0
        fl, fr, dz, swing = anim.run_gait(t, 3.35, cycle, duty=0.28, lift=0.17, bob=0.03)
        p = Pose(rig)
        p.move("pelvis", (0, -0.03, -0.065 + dz))
        p.rot("pelvis", yaw=-7 * swing)
        p.rot("spine_01", pitch=4, yaw=4 * swing).rot("spine_02", pitch=3, yaw=4 * swing).rot("spine_03", pitch=2)
        p.rot("head", pitch=-5)
        p.foot("l", offset=fl + Vector((0.01, 0, 0))).foot("r", offset=fr + Vector((-0.01, 0, 0)))
        # sword hand counter-swings (web: ±3.2 u), shield stays across the chest
        p.hand("r", at=(-0.26, -0.10 - 0.16 * swing, 0.95 + 0.04 * abs(swing)))
        p.aim("weapon_r", wpn_dir(118 + 10 * swing, 0.3))
        p.hand("l", at=(0.09, -0.30, 1.10 + 0.012 * math.sin(4 * math.pi * t)), pole=(1.0, 0.6, -0.4))
        p.aim("weapon_l", (-0.29, -0.956, 0.0))
        flow = 0.38
        p.rot("cape_c_01", pitch=-14 * flow * 2).rot("cape_c_02", pitch=-10 * flow * 2 + 4 * math.sin(2 * math.pi * t))
        p.rot("cape_c_03", pitch=-8 * flow * 2 + 6 * math.sin(2 * math.pi * t + 1.0))
        return p
    run = Clip("Run", cycle, loop=True, sampler=run_pose, notifies=anim.run_contacts_ms(cycle), ref_speed=3.35)
    return [idle, run, atk]


# ── verification ────────────────────────────────────────────────────────────────────────────────────────
def record_source_pose(rig, clips) -> dict:
    """Armature-space bone heads/tails of the source rig at a few frames of every clip."""
    rec = {}
    for c in clips:
        act = bpy.data.actions[f"A_{TOKEN}_{c.name}"]
        anim.assign(rig, act)
        frames = sorted({0, c.frames // 3, c.frames // 2, c.frames})
        for f in frames:
            anim.set_frame(f)
            rec[(c.name, f)] = {pb.name: pb.matrix.copy() for pb in rig.obj.pose.bones}
    anim.assign(rig, None)
    anim.clear_pose(rig)
    return rec


def verify_reimport(sk_fbx: Path, anim_fbx: dict, sm_fbx: list, expect_bones: set, expect_frames: dict,
                    expect_tris: int, source: dict):
    print("\n[reimport] empty scene")
    scene.reset()
    bpy.ops.import_scene.fbx(filepath=str(sk_fbx))
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    check(len(arms) == 1 and len(meshes) == 1, f"one armature + one mesh ({len(arms)}, {len(meshes)})")
    arm, me = arms[0], meshes[0]
    bones = {b.name for b in arm.data.bones}
    check(bones == expect_bones, f"bones survive ({len(bones)} == {len(expect_bones)}; missing "
                                 f"{sorted(expect_bones - bones)[:5]} extra {sorted(bones - expect_bones)[:5]})")
    check("Armature" not in bones, "no extra 'Armature' root bone")
    for need in ("root", "pelvis", "head", "weapon_r", "weapon_l", "hand_r", "ik_foot_l", "cape_c_03"):
        check(need in bones, f"bone {need}")
    check(abs(arm.scale[0] - 1.0) < 1e-4, f"armature scale 1 (got {tuple(round(v, 4) for v in arm.scale)})")
    mats = [m.name for m in me.data.materials]
    check(mats == ["MI_AF_Toon_Test", "M_AF_Outline"], f"material slots {mats}")
    tris = sum(p.loop_total - 2 for p in me.data.polygons)
    check(tris == expect_tris, f"triangles {tris} == {expect_tris}")
    hull = sum(1 for p in me.data.polygons if p.material_index == 1)
    check(hull > 0, f"outline hull faces in slot 1 ({hull})")
    check(me.data.uv_layers.active is not None, "UV map present")
    check("AF_Data" in me.data.color_attributes, f"AF_Data colours ({[c.name for c in me.data.color_attributes]})")
    st = R.weight_stats(me)
    check(st["unweighted"] == 0, f"all vertices weighted ({st['unweighted']} unweighted)")
    check(st["maxInfluences"] <= 4, f"<= 4 influences ({st['maxInfluences']})")
    check(me.data.has_custom_normals, "custom normals imported")
    # hull normals point outward (custom normal · (hull vertex − body centroid) > 0 for most corners)
    import numpy as np
    cn = np.zeros(len(me.data.loops) * 3, np.float32)
    me.data.corner_normals.foreach_get("vector", cn)
    cn = cn.reshape(-1, 3)
    mi = np.zeros(len(me.data.polygons), np.int32)
    me.data.polygons.foreach_get("material_index", mi)
    lt = np.zeros(len(me.data.polygons), np.int32)
    me.data.polygons.foreach_get("loop_total", lt)
    hull_loops = np.repeat(mi, lt) == 1
    pn = np.zeros(len(me.data.polygons) * 3, np.float32)
    me.data.polygons.foreach_get("normal", pn)
    pn = np.repeat(pn.reshape(-1, 3), lt, axis=0)
    frac = float(((cn[hull_loops] * pn[hull_loops]).sum(1) < 0).mean())
    check(frac > 0.9, f"hull corners: custom normal outward while the face winding points inward ({frac:.1%})")
    lo, hi = scene.world_bounds([me])
    check(1.70 < hi.z < 1.90 and lo.z > -0.05, f"height {hi.z:.3f} m (cm units round-trip)")
    check(abs((lo.x + hi.x) / 2) < 0.05, "centred on the origin")
    imported = {}
    for clip, f in anim_fbx.items():
        before = set(bpy.data.actions)
        objs_before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(f))
        new = [a for a in bpy.data.actions if a not in before]
        check(len(new) == 1, f"{f.name}: one action ({[a.name for a in new]})")
        if not new:
            continue
        a = new[0]
        n = int(round(a.frame_range[1] - a.frame_range[0]))
        check(n == expect_frames[clip], f"{f.name}: {n} frames == {expect_frames[clip]} (take '{a.name}')")
        check(f.stem in a.name, f"{f.name}: take named after the file")
        arm2 = next(o for o in bpy.data.objects if o not in objs_before and o.type == "ARMATURE")
        imported[clip] = (arm2, a)
        # world-space comparison with the source rig at the recorded frames
        # (FBX stores no bone length, so compare posed head positions and bone orientations, not tails)
        err, aerr = 0.0, 0.0
        for (cname, fr), mats_ in source.items():
            if cname != clip:
                continue
            bpy.context.scene.frame_set(int(a.frame_range[0]) + fr)
            for bn, m in mats_.items():
                pb = arm2.pose.bones.get(bn)
                if pb is None:
                    continue
                err = max(err, (pb.matrix.translation - m.translation).length)
                q0, q1 = m.to_quaternion(), pb.matrix.to_quaternion()
                ang = q0.rotation_difference(q1).angle
                aerr = max(aerr, math.degrees(min(ang, 2 * math.pi - ang)))      # q ≡ −q
        check(err < 1e-3 and aerr < 0.1, f"{f.name}: posed bones match the source (max {err * 1000:.3f} mm, "
                                         f"{aerr:.4f} deg)")
    sm = {}
    for f in sm_fbx:
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(f))
        new = [o for o in bpy.data.objects if o not in before]
        names = sorted(o.name for o in new)
        mesh = next(o for o in new if o.type == "MESH")
        sm[f.stem] = mesh
        if "Sword" in f.stem:
            check(any(n.startswith("SOCKET_tip") for n in names) and any(n.startswith("SOCKET_mid") for n in names),
                  f"static mesh sockets {names}")
        mnames = [m.name.split(".")[0] for m in mesh.data.materials]
        check(mnames == ["MI_AF_Toon_Test", "M_AF_Outline"], f"{f.name}: material slots {mnames}")
    return arm, me, imported, sm


def roundtrip_render(arm, me, imported, sm, pal, out_dir: Path, clip: str, frame: int):
    """What UE will get: re-imported mesh + weapons on their sockets, driven by the re-imported clip."""
    mt, mo = shading.toon_material(pal), shading.outline_material(pal)
    arm2, act = imported[clip]
    for pb in arm.pose.bones:
        c = pb.constraints.new("COPY_TRANSFORMS")
        c.target, c.subtarget = arm2, pb.name
        c.target_space = c.owner_space = "POSE"
    for o, bone in ((me, None), (sm.get("SM_Test_KitKnight_Sword"), "weapon_r"),
                    (sm.get("SM_Test_KitKnight_Shield"), "weapon_l")):
        if o is None:
            continue
        o.data.materials[0], o.data.materials[1] = mt, mo
        o["af_outline_width"] = outline.WIDTH_CM["hero" if bone is None else "weapon"] / 100.0
        if bone:
            for ch in list(o.children):
                bpy.data.objects.remove(ch)
            R.attach_to_bone(o, R.Rig(arm, None, {}, []), bone)
    arm2.hide_render = True
    bpy.context.scene.frame_set(int(act.frame_range[0]) + frame)
    meshes = [o for o in (me, sm.get("SM_Test_KitKnight_Sword"), sm.get("SM_Test_KitKnight_Shield")) if o]
    rv = review.Review(ASSET, arm, meshes, 1.76, out_dir=out_dir)
    rv.facing("se")
    res = (360, 420)
    review.setup_render(res, 8)
    pts = review.mesh_points(meshes)
    review.frame_points(pts, res, fov_h=26, pitch_ue=-35, fill=0.92)
    review.set_outline_px(meshes, 2.2, Vector(pts.mean(0)))
    img = review.render_array()
    review.draw_text(img, 4, 4, f"REIMPORTED FBX  {clip.upper()} F{frame}")
    return img


def shader_conformance() -> None:
    """Render flat swatches head-on (no rim) in each band and compare with shading.toon_reference."""
    import numpy as np
    import bmesh
    print("\n[shader conformance] Cycles preview vs M_AF_Toon reference")
    scene.reset()
    pal = Palette("Test")
    cases = [("crimson", Region("#A82230")), ("steel", Region("#9AA6BA", .42, .45)),
             ("skin", Region("#E6B791", .30, .25)), ("ember", Region("#FF8A2A", e=1.0))]
    b = M.Builder("Conf", pal, prefix="conf")
    for k, r in cases:
        b.region(k, r)
    me = bpy.data.meshes.new("conf")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=1.0)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new("conf", me)
    scene.link(obj)
    mat = shading.toon_material(pal)
    me.materials.append(mat)
    me.attributes.new("af_swatch", "INT", "FACE")
    L = shading.L_KEY
    worst = 0.0
    for k, r in cases:
        me.attributes["af_swatch"].data[0].value = b.swatch[k]
        pal.bake_uvs(me)
        from kit.mesh import set_af_data
        set_af_data(me, None)
        for label, n in (("light", L), ("base", L.orthogonal()), ("shade", -L)):
            n = n.normalized()
            obj.matrix_world = R.frame((0, 0, 0), n.orthogonal(), n)   # quad's +Z = n
            cam = review._camera()
            cam.data.type = "ORTHO"
            cam.data.ortho_scale = 0.5
            cam.matrix_world = scene.look_at_matrix(n * 3.0, Vector((0, 0, 0)))
            review.setup_render((8, 8), 4)
            px = review.render_array()[4, 4].astype(float)
            ref = np.array(shading.toon_reference(r.hex, r.s, r.l, r.e, n, n))
            d = float(np.abs(px - ref).max())
            worst = max(worst, d)
            print(f"    {k:8s} {label:5s} render {px.astype(int).tolist()} reference {np.round(ref).astype(int).tolist()}")
    check(worst <= 1.5, f"preview pixels equal the M_AF_Toon reference (max error {worst:.2f} / 255)")


def main() -> int:
    t0 = time.time()
    previews = "--no-previews" not in sys.argv
    print(f"export root {paths.export_root()}  previews {paths.preview_root()}")
    scene.reset()
    pal, rig, body = build_knight()
    sword, sword_sockets = build_sword(pal)
    shield = build_shield(pal)
    print(f"[build] body tris {M.tri_count(body, 0)} + hull {M.tri_count(body, 1)}; sword {M.tri_count(sword)}; "
          f"shield {M.tri_count(shield)}; bones {len(rig.obj.data.bones)}")
    st = R.weight_stats(body)
    check(st["unweighted"] == 0 and st["maxInfluences"] <= 4, f"weights {st['maxInfluences']} max inf, "
                                                               f"{st['unweighted']} unweighted")
    check(M.tri_count(body, 0) <= 18000, "toon triangles within the hero budget (≤ 18 k)")
    lib = make_poses(rig)
    clips = make_clips(rig, lib)
    sword.name, shield.name = "SM_Test_KitKnight_Sword", "SM_Test_KitKnight_Shield"
    # static weapon meshes first (they are attached to the rig for previews afterwards)
    sm = export.export_static_mesh(sword, "SM_Test_KitKnight_Sword", "Weapons", sword_sockets)
    shp = export.export_static_mesh(shield, "SM_Test_KitKnight_Shield", "Weapons")
    man = export.Manifest()
    man.set_palette(pal)
    man.set_asset("SM_Test_KitKnight_Sword", export.static_entry(sword, sm, "Weapons", pal, "weapon",
                                                                  sockets=sword_sockets,
                                                                  extra={"attachSocket": "weapon_r"}))
    man.set_asset("SM_Test_KitKnight_Shield", export.static_entry(shield, shp, "Weapons", pal, "weapon",
                                                                   extra={"attachSocket": "weapon_l"}))
    man.save()
    res = asset.ship_character(ASSET, TOKEN, CATEGORY, rig, body, pal, clips, "hero",
                               game_ids=["test_kit_knight"], attachments=[(sword, "weapon_r"), (shield, "weapon_l")],
                               blob_radius=0.39, previews=previews)
    man = json.loads(Path(res["manifest"]).read_text())
    e = man["assets"][ASSET]
    print("\n[manifest]", json.dumps({k: e[k] for k in ("skeleton", "heightCm", "bones", "triangles")}))
    check(e["materialSlots"][1]["name"] == "M_AF_Outline", "manifest outline slot")
    atk = next(a for a in e["anims"] if a["name"] == "Attack01")
    check(atk["contactMs"] == 308.0 and atk["lengthMs"] == 615.0 and atk["frames"] == 37,
          f"Attack01 615 ms / 37 frames / contact 308 ({atk['lengthMs']}, {atk['frames']}, {atk['contactMs']})")
    check(any(s["name"] == "fx_overhead" for s in e["sockets"]), "fx sockets in the manifest")
    check("shading" in man and abs(man["shading"]["tShade"] - 0.42) < 1e-9, "shading block")
    check("Test" in man["palettes"], "palette entry")
    check(man["assets"]["SM_Test_KitKnight_Sword"]["sockets"][0]["name"] == "tip", "weapon SM entry + sockets")
    expect_bones = {b.name for b in rig.obj.data.bones if b.use_deform}
    frames = {c.name: c.frames for c in clips}
    source = record_source_pose(rig, clips)
    sk = paths.category_dir(CATEGORY) / f"{ASSET}.fbx"
    anims = {c.name: paths.category_dir(CATEGORY) / f"A_{TOKEN}_{c.name}.fbx" for c in clips}
    sms = [sm, paths.category_dir("Weapons") / "SM_Test_KitKnight_Shield.fbx"]
    tris = sum(p.loop_total - 2 for p in body.data.polygons)
    arm, me, imported, smo = verify_reimport(sk, anims, sms, expect_bones, frames, tris, source)
    if previews:
        out = paths.preview_dir(ASSET)
        from kit import pngio
        img = roundtrip_render(arm, me, imported, smo, pal, out, "Attack01", 18)
        pngio.write_png_budget(out / "roundtrip_reimport.png", img)
        print("\n[previews]")
        for p in sorted(out.glob("*.png")):
            print(f"  {p}  {p.stat().st_size // 1024} KB")
            check(p.stat().st_size <= 200 * 1024, f"{p.name} <= 200 KB")
    shader_conformance()
    print(f"\n{'PASS' if not FAIL else 'FAIL'}  ({len(FAIL)} failures, {time.time() - t0:.1f} s)")
    for f in FAIL:
        print("  -", f)
    if "--keep" in sys.argv:
        print(f"kept {TMP}")
    else:
        import shutil
        shutil.rmtree(TMP, ignore_errors=True)
    return 0 if not FAIL else 1


if __name__ == "__main__":
    sys.exit(main())
