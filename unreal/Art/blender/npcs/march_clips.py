"""March NPC clips (A9): ``npc_clips.NpcAnims`` with the march work loops and the anchored hands.

* ``Work_Toll`` 2667 loop (Brother Aldwin) — the bronze handbell held before the belly, hanging below the fist; a
  slow wind back, one stroke forward (``FX_HandbellRing`` at 30 %: the audio cue) and the bell swinging out its ring,
  the head bowed in prayer, the free hand flat on the chest;
* ``Work_Vial`` 2400 loop (Mother Marl) — the lantern raised before her chest (its elbow out at the fist's height),
  the phial brought in front of its panes and turned against the light, turned back, lowered;
* ``Work_Barrow`` 2400 loop (Old Bern) — standing at the barrow's handles, a long breath, the head bowed over the
  load and lifted again;
* ``Work_Spade`` 2667 loop (Tamsin) — leaning on her planted grave spade, both hands stacked on its grip: the weight
  sinks onto it, she looks out over the graves, settles;
* Old Bern's barrow bones: ``barrow_01`` pitched up about the axle in Walk / Run (``barrow_state``: the legs ~10 cm
  off the ground, a small bob per step, the grips in his fists) and resting (pitch 0) in every other clip;
  ``barrow_wheel_01`` spins 1 turn per Walk loop and 1½ per Run loop;
* the Relic Peddler's ``Work_Count`` keeps his free hand on the tray's rim.

**Anchored hands** (``ANCHORS``, applied in every clip's post step before the feet are planted): Bern's fists stay on
the barrow's grips (the barrow hangs on the root socket, so the grips are fixed in body space) — both in the idles,
the work loop, Walk / Run / Hurt, the hold hand in Alert / Talk / the fidget; Aldwin's free hand stays on his chest
(carried by ``spine_03``) except while it greets or talks; the peddler's left hand stays on the tray rim (carried by
``spine_02``, the tray's bone). Bern's ``Death``: hit, the knees buckle, he sinks onto his knees and slumps over the
handles, head bowed between them (a forward fall would land in his own barrow).
"""
from __future__ import annotations

import math

import numpy as np
from mathutils import Vector

from kit import anim

import common as C
from common import BP, K, V
import hero_base as HB
import props as P
from npc_clips import NpcAnims
import march_props as MPR

GRIP_ITEMS = ("hammer", "tongs", "staff", "sword", "sword_notched", "spade", "handbell", "vial")


class MarchAnims(NpcAnims):
    def __init__(self, rig, folk, npc, items: dict | None = None):
        super().__init__(rig, folk, npc, items)
        lk = self.look
        self.GRIPS = tuple(s for s, it in (("r", lk.item), ("l", lk.off)) if it in GRIP_ITEMS)
        self.barrow = lk.item == "barrow"
        self.tray = folk.has("relic_tray")
        self.spade = lk.item == "spade"
        if self.barrow:
            # the slim old man's coat over a hurried step: a touch more room between the shins and the cloth (the
            # stout plains escort's 2.2 cm let two shin vertices 2 mm through the front panel in the run)
            self.SKIRT_CLEAR = 0.028
            self.GRIPS = ()
            self.ITEMS = ()                      # the barrow rests on its own wheel and legs (QA: ground:<barrow>)
            self.grip_pts = MPR.barrow_grips()
            self.lift_deg = MPR.lift_pitch()
            # the wheel (in his mesh) is not cloth and never a ground contact of the body
            self.CLOTH_PARTS = tuple(self.CLOTH_PARTS) + ("wheel",)

    # ── anchored hands ──────────────────────────────────────────────────────────────────────────────────
    def anchored(self, clip: str) -> tuple:
        """Sides whose hand is anchored in ``clip`` (module docstring)."""
        s_free = self.free_hand()
        if self.barrow:
            if clip in ("Alert", "Talk", "IdleNPC_Fidget"):
                return tuple(s for s in ("l", "r") if s != s_free)
            if clip == "Death":
                return ()
            return ("l", "r")
        if self.npc.work == "Toll":
            return () if clip in ("Alert", "Talk") else ("l",)
        if self.spade:
            # the spade stays planted (its hand pinned); the stacked left hand leaves it to greet and talk
            return ("r",) if clip in ("Alert", "Talk") else ("r", "l")
        if self.tray:
            return ("l",)
        return ()

    def bone_pt(self, bp: BP, bone: str, rest: Vector) -> Vector:
        p = C.to_pose(self.rig, bp.but(hand_l=None, hand_r=None), None, grips=())
        m = anim.evaluate(p, return_matrices=True)[bone] @ self.bones[bone].matrix_local.inverted()
        return m @ rest

    def anchor(self, bp: BP, sides, pitch: float = 0.0) -> BP:
        grips = MPR.barrow_grips(pitch) if self.barrow else None
        for s in sides:
            if self.barrow:
                # the fist (weapon bone head = the grip) on the handle's grip (pitched with the barrow)
                bp = self.fit_item(bp, s, Vector((0.0, 0.0, 0.0)), grips[s], iters=3)
            elif self.spade and s == "r":
                R0 = self.ready()
                bp = bp.but(hand_r=R0.hand_r, wpn=R0.wpn, wpn_up=R0.wpn_up, elbow_r=R0.elbow_r)
            elif self.spade and s == "l":
                R0 = self.ready()
                bp = bp.but(hand_l=R0.hand_l, elbow_l=R0.elbow_l, wrist_l=R0.wrist_l)
            elif self.npc.work == "Toll" and s == "l":
                bp = bp.but(hand_l=self.bone_pt(bp, "spine_03", self.CHEST_HAND))
            elif self.tray and s == "l":
                bp = bp.but(hand_l=self.bone_pt(bp, "spine_02", self.tray_hand()))
        return bp

    def _wrap_post(self, name: str, post):
        sides = self.anchored(name)
        if not sides and not self.barrow:
            return post
        base = post or self.post

        def fn(bp: BP, t: float) -> BP:
            if not self.barrow:
                return base(self.anchor(bp, sides), t)
            st = self.barrow_state(name, t)
            out = base(self.anchor(bp, sides, st[0]), t)
            out.c["_barrow"] = st                 # read by ``cloth`` (the solve): the barrow / wheel bones
            return out
        return fn

    # ── Old Bern's barrow: pitched onto its wheel in Walk / Run, the wheel spinning ──────────────────────
    LIFT_CLIPS = {"Walk": (1000.0, 1.0), "Run": (700.0, 1.5)}   # loop ms, wheel turns per loop
    LIFT_BOB_DEG = 0.6          # the barrow rides each step (≈ ±1.3 cm at the grips, ±0.9 cm at the legs)

    def barrow_state(self, clip: str, t: float) -> tuple[float, float]:
        """(pitch, wheel spin) in degrees at ``t`` ms of ``clip``. Walk / Run: lifted ``lift_deg`` (the legs ~10 cm
        off the ground) bobbing with the steps, the wheel rolling (Walk 1.2 m / loop ≈ 0.95 turns of the 0.20 m
        wheel → 1 turn, Run 2.0 m ≈ 1.59 → 1½: quarter-turn multiples, the wheel's four-fold face wraps without a
        pop); every other clip: resting on its legs, the wheel still. The runtime's clip blend (Idle ↔ Walk / Run)
        lifts and sets it down."""
        if clip not in self.LIFT_CLIPS:
            return 0.0, 0.0
        L, turns = self.LIFT_CLIPS[clip]
        ph = (t / L) % 1.0
        return self.lift_deg - self.LIFT_BOB_DEG * math.cos(4 * math.pi * ph), 360.0 * turns * ph

    def wheel_total(self, clip: str) -> float | None:
        """Total spin of a lifted loop (the bake keys its last frame there instead of the first frame's 0)."""
        if not self.barrow or clip not in self.LIFT_CLIPS:
            return None
        return 360.0 * self.LIFT_CLIPS[clip][1]

    def cloth(self, p, bp: BP) -> None:
        super().cloth(p, bp)
        if self.barrow:
            pitch, spin = bp.c.get("_barrow", (0.0, 0.0))
            p.rot("barrow_01", pitch=pitch)
            p.rot("barrow_wheel_01", pitch=spin)

    def walk(self):
        if not self.barrow:
            return super().walk()
        from npc_clips import WALK_SPEED
        # leaning into the lifted barrow, no arm swing (both fists on its grips)
        return self.locomotion("Walk", 1000.0, WALK_SPEED, duty=0.62, lift=0.06, bob=0.012, lean=8.0, arm=0.0,
                               flow=0.18, crouch=-0.025, hip_pitch=2.0)

    def clip(self, name, length, keys, span, loop=False, notifies=None, ref_speed=None, additive=False,
             layer=None, post=None, key_poses=None):
        return super().clip(name, length, keys, span, loop=loop, notifies=notifies, ref_speed=ref_speed,
                            additive=additive, layer=layer, post=self._wrap_post(name, post), key_poses=key_poses)

    def proc_clip(self, name, length, sampler, loop=False, notifies=None, ref_speed=None, post=None,
                  key_poses=None):
        return super().proc_clip(name, length, sampler, loop=loop, notifies=notifies, ref_speed=ref_speed,
                                 post=self._wrap_post(name, post), key_poses=key_poses)

    # ── READY ───────────────────────────────────────────────────────────────────────────────────────────
    def _front(self, zc: float) -> float:
        rx, ry, cy, cx = self.F.torso.at(zc)
        return -(ry - cy)

    @property
    def CHEST_HAND(self) -> Vector:
        """Aldwin's free hand flat on his sternum, rest body space (review: 8 cm higher and out by the shoulder it
        read at the game camera as scratching his ear)."""
        F = self.F
        zc = F.th(F.L - 3.6) - 0.08
        return F.torso.point(zc, 9.0, 0.05) + Vector((0.0, -0.03, 0.0))

    def _ready_toll(self, bp: BP) -> BP:
        F = self.F
        zc = F.th(F.L - 7.5)
        front = self._front(zc)
        hb = self.CHEST_HAND
        d, e = V(0.0, 0.18, -1.0).normalized(), V(0.0, 1.0, 0.18).normalized()
        # the free arm hangs from the shoulder, the elbow tucked in at his side, the forearm across to the sternum
        return bp.but(lean=3.0, head_pitch=10.0, hand_r=V(-0.13, -front + 0.17, zc + 0.03), wpn=d, wpn_up=e,
                      elbow_r=V(-0.9, -0.4, -0.3), hand_l=hb, elbow_l=V(0.35, -0.15, -1.0),
                      wrist_l=(-60.0, 0.0, -80.0))

    # per-look guard stance (sword tip, pommel hand: lateral, forward m); the plains values otherwise. The Captain's
    # blade stands a little further out: at the plains spot its edge ran beside her leg in the back view and the gap
    # between them was too narrow for both inks (rim 89.6 %)
    GUARD = {"QuestElder_March": ((-0.20, 0.34), (-0.17, 0.23))}

    def _ready_guard(self, bp: BP) -> BP:
        g = self.GUARD.get(self.npc.id)
        if g is None:
            return super()._ready_guard(bp)
        J = self.J
        tip = V(g[0][0], g[0][1], 0.004)
        hand = V(g[1][0], g[1][1], 0.0)
        d = (tip - hand)
        d.z = -P.SWORD_TIP_Y
        d.normalize()
        up = V(0, 1, 0) - d * V(0, 1, 0).dot(d)
        bp = bp.but(lean=5.0, hip_roll=-3.0, side=2.0, hip_yaw=-6.0, twist=4.0, pelvis=V(0.01, 0.0, -0.016),
                    foot_r=J["ankle_r"] + V(-0.012, 0.035, 0), foot_l=J["ankle_l"] + V(0.012, -0.03, 0),
                    hand_r=tip - d * P.SWORD_TIP_Y, wpn=d, wpn_up=up.normalized(), elbow_r=V(-0.8, -0.5, -0.1),
                    hand_l=self.J["hip_l"] + V(0.12, 0.05, 0.05), elbow_l=V(1.0, -0.2, 0.0), wrist_l=(10.0, 0.0, -60.0))
        return self.fit_item(bp, "r", self.SWORD_TIP, tip)

    # Tamsin leans on her grave spade: the blade planted before her right foot, the right fist on the shaft under
    # the T-grip at the belly, the left hand stacked over the grip
    SPADE_TIP = V(-0.10, 0.40, 0.004)
    SPADE_HAND = V(-0.05, 0.19, 0.0)

    def _ready_spade(self, bp: BP) -> BP:
        tip = self.SPADE_TIP
        d = self.SPADE_TIP - self.SPADE_HAND
        d.z = -MPR.SPADE_TIP_Y
        d.normalize()
        up = (V(0, 1, 0) - d * V(0, 1, 0).dot(d)).normalized()
        bp = bp.but(lean=7.0, hip_roll=-2.0, side=1.5, hip_yaw=-4.0, twist=3.0, pelvis=V(0.006, 0.0, -0.016),
                    head_pitch=9.0, hand_r=tip - d * MPR.SPADE_TIP_Y, wpn=d, wpn_up=up, elbow_r=V(-0.7, -0.6, -0.3))
        bp = self.fit_item(bp, "r", (0.0, MPR.SPADE_TIP_Y, 0.0), tip)
        top = self.item_pt(bp, "r", (0.0, -0.062, 0.0))
        bp = bp.but(hand_l=top + V(0.02, -0.055, 0.045), elbow_l=V(0.7, -0.5, -0.4), wrist_l=(-35.0, 0.0, 20.0))
        # the left fist rests on the T-grip (its grip point a fist's half-height over the bar: at the wrist target
        # above it floated 8 cm high, by the chin at the game camera)
        return self.fit_item(bp, "l", (0.0, 0.0, 0.0), top + V(0.0, 0.006, 0.032), iters=4)

    def _work_spade(self) -> list:
        """A weary loop at the graves: the weight sinks onto the spade with a breath, the head lifts and turns to
        look over the rows to her right, settles."""
        R0 = self.ready()
        U = self.U
        k0 = R0.but(head_pitch=R0.head_pitch)
        sink = R0.but(lean=R0.lean + 2.0, pelvis=R0.pelvis + V(0, 0, -0.5 * U), head_pitch=R0.head_pitch + 7.0,
                      clav_l=(-3.0, 0.0), clav_r=(-3.0, 0.0))
        look = R0.but(lean=R0.lean - 1.0, pelvis=R0.pelvis + V(0, 0, 0.2 * U), head_pitch=1.0,
                      head_yaw=R0.head_yaw - 20.0, twist=R0.twist - 4.0)
        return [K(0.0, k0), K(0.32, sink), K(0.46, sink), K(0.68, look, "out"), K(0.84, look), K(1.0, k0)]

    def _ready_vial(self, bp: BP) -> BP:
        F = self.F
        zc = F.th(F.L - 6.0)
        front = self._front(zc)
        d = V(0.05, 0.1, 1.0).normalized()
        e = V(0.0, 1.0, -0.1)
        e = (e - d * e.dot(d)).normalized()
        return bp.but(hand_r=V(-0.11, -front + 0.15, zc + 0.02), wpn=d, wpn_up=e, elbow_r=V(-0.9, -0.3, -0.4),
                      hand_l=self.hang_hand("l", out=0.12, fwd=0.12, k=0.82), **self._hanging("l"),
                      elbow_l=V(0.8, -0.4, -0.3))

    def _ready_barrow(self, bp: BP) -> BP:
        # (QA gripMm: at lean 7 the resting grips were 4-6 cm out of his reach — the fists hung above and behind
        # them) a deeper stoop over the handles, the knees a touch bent
        g = self.grip_pts
        bp = bp.but(lean=10.0, hip_pitch=2.0, head_pitch=1.0, pelvis=bp.pelvis + V(0.0, 0.012, -0.012),
                    hand_l=g["l"], hand_r=g["r"],
                    elbow_l=V(0.7, -0.5, -0.4), elbow_r=V(-0.7, -0.5, -0.4), wrist_l=(-10.0, 0.0, 70.0),
                    wrist_r=(-10.0, 0.0, -70.0))
        for s in ("l", "r"):
            bp = self.fit_item(bp, s, Vector((0.0, 0.0, 0.0)), g[s], iters=4)
        return bp

    def tray_hand(self) -> Vector:
        """The peddler's free (left) wrist target: the palm on the tray's front-left rim (rest body space)."""
        c, _ = self.F.tray_frame()
        T = self.F.TRAY
        return self.F.tray_tilt() @ (c + Vector((T["w"] / 2 - 0.035, -T["d"] / 2 + 0.05, 0.012 + T["h"] + 0.035)))

    def _ready_count(self, bp: BP) -> BP:
        bp = super()._ready_count(bp)
        if not self.tray:
            return bp
        F = self.F
        zb = F.th(F.L * 0.42)
        front = self._front(zb)
        return bp.but(hand_l=self.tray_hand(), elbow_l=V(0.9, -0.2, -0.4), wrist_l=(-60.0, 0.0, -30.0),
                      hand_r=V(-0.12, -front + 0.15, zb + 0.10))

    # ── work loops ──────────────────────────────────────────────────────────────────────────────────────
    TOLL_AT = 0.30

    def work(self):
        if self.npc.work != "Toll":
            return super().work()
        keys = self.work_keys()
        L = self.npc.work_ms
        c = self.clip("Work_Toll", L, keys, L, loop=True, notifies={"FX_HandbellRing": round(L * self.TOLL_AT, 1)})
        c.key_poses_ms = sorted({round(k.t * L, 1) for k in keys if k.t < 1.0})
        return c

    def _work_toll(self) -> list:
        """One slow stroke: wind back (the bell's mouth toward him), swing forward (the clapper strikes at 30 %),
        the ring swinging out with the wrist; head bowed, a small nod on the stroke."""
        R0 = self.ready()
        hr = R0.hand_r

        def bell(fwd_deg: float) -> dict:
            a = math.radians(fwd_deg)
            d = V(0.0, math.sin(a) + 0.18, -math.cos(a)).normalized()
            e = V(0.0, math.cos(a), math.sin(a))
            e = (e - d * e.dot(d)).normalized()
            return {"wpn": d, "wpn_up": e}
        k0 = R0.but(head_pitch=12.0)
        back = R0.but(head_pitch=11.0, hand_r=hr + V(0.0, -0.03, 0.03), **bell(-28.0))
        stroke = R0.but(head_pitch=16.0, lean=R0.lean + 1.5, hand_r=hr + V(0.0, 0.05, 0.05), **bell(34.0))
        ring1 = R0.but(head_pitch=15.0, hand_r=hr + V(0.0, 0.02, 0.02), **bell(-12.0))
        ring2 = R0.but(head_pitch=13.0, hand_r=hr + V(0.0, 0.01, 0.01), **bell(8.0))
        return [K(0.0, k0), K(0.16, back, "out"), K(self.TOLL_AT, stroke, "in"), K(0.48, ring1), K(0.66, ring2),
                K(1.0, k0)]

    def _work_vial(self) -> list:
        """The phial held up to her lantern's light (review: the phial beside her face and the lantern at her hip
        never met): the lantern rises before her chest, hanging plumb, and the phial comes to the panes; at the
        'turned' key (48 %) it is turned against the light in front of them (5-10 cm before the glow socket: the
        card sits behind it), turned the other way, lowered."""
        R0 = self.ready()
        Sl, Sr = self.shoulder("l"), self.shoulder("r")
        up_d = V(0.05, 0.1, 1.0).normalized()

        def phial(d):
            d = d.normalized()
            e = (V(0.0, 1.0, 0.0) - d * V(0.0, 1.0, 0.0).dot(d)).normalized()
            return {"wpn": d, "wpn_up": e}
        # (review 5, round 2: with the fist at mid-chest the lantern hung under the phial's forearm — hidden from the
        # se camera at 'turned') the lantern fist rises to just above the shoulder, the elbow out at its height (a
        # forearm from below would run up through the hanging lantern), so the lit panes sit at shoulder height above
        # the phial arm; the phial comes up before their lower half.
        lan = {"elbow_l": V(1.0, -0.15, 0.25), **self._hanging("l")}
        k0 = R0.but(head_pitch=R0.head_pitch + 6.0, head_yaw=-4.0)
        lifted = R0.but(head_pitch=10.0, head_yaw=-3.0, lean=R0.lean + 1.0,
                        hand_l=Sl + V(-0.06, 0.28, 0.02), **lan,
                        hand_r=Sr + V(0.14, 0.38, -0.20), elbow_r=V(-0.8, -0.3, -0.6), **phial(up_d))
        turned = lifted.but(head_pitch=11.0, head_yaw=-5.0, head_roll=-3.0,
                            hand_l=Sl + V(-0.08, 0.30, 0.06), hand_r=Sr + V(0.20, 0.42, -0.12),
                            **phial(V(-0.55, 0.10, 0.8)))
        turned2 = turned.but(head_yaw=-2.0, head_roll=2.0, hand_r=Sr + V(0.19, 0.42, -0.11),
                             **phial(V(0.40, 0.05, 0.9)))
        return [K(0.0, k0), K(0.28, lifted, "out"), K(0.48, turned), K(0.62, turned2), K(0.85, k0, "in"),
                K(1.0, k0)]

    def _work_barrow(self) -> list:
        R0 = self.ready()
        U = self.U
        k0 = R0.but(head_pitch=6.0)
        bowed = R0.but(head_pitch=20.0, lean=R0.lean + 3.0, pelvis=R0.pelvis + V(0, 0, -0.4 * U), head_yaw=-5.0)
        breath = R0.but(head_pitch=-2.0, lean=R0.lean - 0.5, pelvis=R0.pelvis + V(0, 0, 0.12 * U), head_yaw=4.0,
                        clav_l=(4.0, 0.0), clav_r=(4.0, 0.0))
        return [K(0.0, k0), K(0.35, bowed), K(0.62, bowed), K(0.82, breath, "out"), K(1.0, k0)]

    # ── Mother Marl greets with her lantern: raised forward to see who comes, kept hanging plumb ─────────
    def _lantern_gesture(self) -> bool:
        return self.look.off == "lantern" and self.free_hand() == "l"

    def _lantern_at(self, out: float, fwd: float, down: float, elbow=None) -> dict:
        """The lantern fist out / forward / below the left shoulder, the lantern hanging plumb from it (a raised
        wave would turn it into the forearm), the elbow out and down behind it — or, for a fist raised above the
        chest, out to the side at the fist's height (``elbow``): a forearm rising from below runs up through the
        lantern hanging under the fist (QA x:Lantern)."""
        return {"hand_l": self.shoulder("l") + V(out, fwd, -down), "elbow_l": elbow or V(0.8, -0.3, -0.6),
                **self._hanging("l")}

    def alert(self):
        if not self._lantern_gesture():
            return super().alert()
        R0 = self.ready()
        W0 = self.work_keys()[0].bp
        # (review: the end pose read as the lantern held out sideways at the waist) the fist rises forward to above
        # the shoulder so the lantern itself, hanging plumb, is at shoulder height before her
        # (W1, pitch −50°: a forward reach drops down the screen by ~0.77 × its length, so the fist rises well above
        # the shoulder to put the lantern at shoulder height on screen)
        mid = R0.but(head_pitch=R0.head_pitch - 5.0, lean=R0.lean - 1.0, pelvis=R0.pelvis + V(0, 0, 0.006),
                     **self._lantern_at(0.10, 0.22, 0.06, elbow=V(1.0, -0.2, -0.1)))
        up = R0.but(head_pitch=R0.head_pitch - 9.0, head_yaw=R0.head_yaw + 5.0, twist=R0.twist - 4.0,
                    lean=R0.lean - 2.0, pelvis=R0.pelvis + V(0, 0, 0.008),
                    **self._lantern_at(0.10, 0.26, -0.24, elbow=V(1.0, -0.1, 0.3)))
        keys = [K(0.0, W0), K(0.4, mid), K(1.0, up, "out")]
        c = self.clip("Alert", 667.0, keys, 500.0)
        c.key_poses_ms = [0.0, 200.0, 350.0, 500.0, 667.0]
        return c

    def talk(self):
        if not self._lantern_gesture():
            return super().talk()
        R0 = self.ready()
        k0 = R0.but(head_pitch=R0.head_pitch + 1.0, **self._lantern_at(0.14, 0.27, 0.30))
        k1 = R0.but(lean=R0.lean + 2.0, head_pitch=R0.head_pitch - 3.0, head_yaw=R0.head_yaw + 3.0,
                    twist=R0.twist + 3.0, **self._lantern_at(0.16, 0.31, 0.20))
        k2 = R0.but(head_pitch=R0.head_pitch + 3.0, head_yaw=R0.head_yaw - 2.0, **self._lantern_at(0.13, 0.28, 0.26))
        keys = [K(0.0, k0), K(0.33, k1), K(0.66, k2), K(1.0, k0)]
        c = self.clip("Talk", 1200.0, keys, 1200.0, loop=True)
        c.key_poses_ms = [0.0, 300.0, 400.0, 600.0, 800.0, 1000.0]
        return c

    # ── fidgets ─────────────────────────────────────────────────────────────────────────────────────────
    def _fidget_look(self, R0):
        keys = super()._fidget_look(R0)
        if not (self.look.hat and self.look.hat[0] == "hood"):
            return keys
        # the hood's cowl is carried by the neck: a smaller turn keeps the face in the opening
        return [K(k.t, k.bp.but(head_yaw=k.bp.head_yaw * 0.7, twist=k.bp.twist * 1.3), k.ease) if k.t not in (0.0, 1.0)
                else k for k in keys]

    def hurt(self):
        """Bern flinches over his handles (the plains recoil leaned him back 11°, out of reach of the grips)."""
        if not self.barrow:
            return super().hurt()
        R0 = self.ready()
        hit = R0.but(lean=R0.lean - 3.0, head_pitch=-12.0, head_yaw=R0.head_yaw + 6.0, head_roll=4.0,
                     pelvis=R0.pelvis + V(0, -0.004, -0.016), clav_l=(6.0, -2.0), clav_r=(6.0, -2.0), flow=0.3)
        keys = [K(0.0, R0), K(0.25, hit, "out"), K(1.0, R0)]
        c = self.clip("Hurt", 300.0, keys, 300.0)
        c.key_poses_ms = [0.0, 75.0, 150.0, 300.0]
        return c

    # ── escort: the barrow run ──────────────────────────────────────────────────────────────────────────
    def run(self):
        """Old Bern does not sprint behind a loaded barrow: at the escort's run speed (2.85 m/s, the core's) he
        hurries with shorter, lower steps and less lean than the plains escort's run — high knees under the long
        coat met the handles and pushed the shins through the coat's front panels."""
        if not self.barrow:
            return super().run()
        from npc_clips import ESCORT_RUN_SPEED
        return self.locomotion("Run", 700.0, ESCORT_RUN_SPEED, duty=0.36, lift=0.09, bob=0.030, lean=9.0,
                               arm=0.0, flow=0.32, crouch=-0.04, hip_pitch=3.0, flow_step=0.12, head_up=12.0)

    # ── escort: Death at the barrow ─────────────────────────────────────────────────────────────────────
    def death(self):
        if not self.barrow:
            return super().death()
        R0 = self.ready()
        J = self.J
        cw = self.COWER
        g = self.grip_pts
        hit = R0.but(lean=-6.0, head_pitch=-12.0, pelvis=R0.pelvis + V(0, -0.02, 0.0), flow=0.3,
                     hand_l=g["l"] + Vector((0.0, 0.03, 0.02)), hand_r=g["r"] + Vector((0.0, 0.03, 0.02)))
        buckle = R0.but(pelvis=V(0, 0.03, -0.22), lean=16.0, hip_pitch=6.0, head_pitch=10.0,
                        foot_l=J["ankle_l"] + V(0.03, -0.06, 0.0), foot_r=J["ankle_r"] + V(-0.03, -0.02, 0.0),
                        knee_l=V(0.2, 1, -0.2), knee_r=V(-0.2, 1, -0.2), flow=0.25,
                        hand_l=g["l"] + Vector((0.0, 0.02, -0.03)), hand_r=g["r"] + Vector((0.0, 0.02, -0.04)))
        # slumped on his knees behind the barrow, the hands slid a little down the handles, the head bowed on the
        # chest between them (a deeper lean laid the head on the shrouds)
        bed = Vector((0.0, MPR.BARROW["bed_y0"], MPR.BARROW["bed_z"]))
        hl = g["l"].lerp(Vector((g["l"].x, bed.y, bed.z)), 0.16)
        hr = g["r"].lerp(Vector((g["r"].x, bed.y, bed.z)), 0.20)
        slump = R0.but(pelvis=V(*cw["pelvis"]) + V(0, 0.05, 0.0), lean=30.0, hip_pitch=10.0, head_pitch=40.0,
                       head_yaw=-10.0, head_roll=8.0,
                       foot_l=J["ankle_l"] + V(cw["foot_out"], -cw["foot_back"], cw["foot_up"]),
                       foot_r=J["ankle_r"] + V(-cw["foot_out"], -cw["foot_back"] + 0.02, cw["foot_up"]),
                       foot_l_rot=(cw["toe"], 0, 6), foot_r_rot=(cw["toe"] - 4.0, 0, 4),
                       knee_l=V(*cw["knee_pole"]), knee_r=V(-cw["knee_pole"][0], *cw["knee_pole"][1:]),
                       hand_l=hl, hand_r=hr, elbow_l=V(0.9, -0.3, -0.3), elbow_r=V(-0.9, -0.3, -0.3),
                       wrist_l=(-30.0, 0.0, 50.0), wrist_r=(-30.0, 0.0, -50.0), clav_l=(2.0, 8.0), clav_r=(2.0, 8.0),
                       flow=0.1)
        keys = [K(0.0, R0), K(0.14, hit, "out"), K(0.42, buckle, "in"), K(0.80, slump, "smooth"), K(1.0, slump)]
        ground = self.grounded(snap=False)
        c = self.clip("Death", 900.0, keys, 900.0, post=ground)
        c.key_poses_ms = [0.0, 126.0, 378.0, 560.0, 720.0, 900.0]
        return c
