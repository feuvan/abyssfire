"""Ash-husk corpse hooks of the march roster (dark remake Wave 2 art, 2H; monsters.md §1.5, DECISIONS DR6,
implementation-plan.md K3 / U3).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/corpse_husks.py
        [assets…] [--no-previews] [--dry-run] [--scratch]

For every Chapter 1 monster asset the manifest gets a ``corpse`` block — what the UE U3 package (``AbyssCorpses``)
reads to turn a kill into what monsters.md §1.5 describes: in ash mode (the default in every locale) the body holds
its Death pose, greys and crumbles into an **ash husk** that keeps its place for the corpse TTL, then blows away;
breach-blood leaves a tar puddle instead; the Gravegnawers a carcass the other gnawers ignore. The block is
presentation only (the core's corpse registry, TTL, raise / consume / burn rules are K3's data on the monster):

* ``look`` — ``husk`` (the Unburied, the Living, the Corpseherd, Valdras), ``carcass`` (the Gravegnawers: the same
  ash timeline, a beast's body), ``puddle`` (breach-blood: the manifest ``fx.puddle`` decal, no body left);
* ``deathClip`` / ``holdFromMs`` — the Death clip and the time its last pose is held from (the clip's length);
  ``timeline`` — the ``M_AF_AshDissolve`` contract (``dark.materials`` / ``manifest.materials``) by reference, with
  ``holdUntil: "story"`` for the two bodies a cutscene looks at (the Corpseherd's ``cs_rf_herd``, Valdras'
  ``cs_rf_marshal_falls``: the grey starts when the beat ends);
* ``husk`` — the fallen body measured on the shipped FBX at the Death clip's last frame (body + held items, actor
  frame: forward, right, up cm, the convention of the VFX recipes' offsets): ``centerCm``, ``boundsCm``,
  ``radiusCm`` (ground footprint round the centre), ``topCm``, and ``anchors`` (``fx_chest`` / ``fx_head`` sockets,
  the pelvis): where ``corpse.crumble`` / ``expire`` / ``burn`` (point = the centre on the ground), the raise marker
  (``monster.raise.mark`` under the pelvis) and the Consume target (the centre) go;
* ``rise`` — the asset's own ``Rise`` clip, for the **self-rise only** (the Unburied trait / affix re-raises the
  monster's own corpse: Death reversed, ``monster.raise`` + the dissolve reversed over its first 60 %);
  ``raisedAs`` — what a raise by **another** monster (the Corpseherd, a standard-bearer, Valdras) stands up:
  always a ``risen_footman`` (monsters.md 2.0; U3 removes this husk on ``EvCorpseRemoved{raised}`` and the footman
  spawns at the corpse with its own Rise) — on every husk that can be raised, the Unburied with their own Rise too;
* ``raisable`` / ``consumable`` — false on the gnawers' carcasses (monsters.md 1.5) and on the two story bodies
  (``STORY_HOLD``: the Corpseherd and Valdras; a cutscene looks at them, so nothing raises or eats the mini-boss or
  the act boss; the guard is permanent, not only while ``holdUntil: story`` holds);
* ``recipes`` — the A12 ids of the husk timeline; ``items`` — held items dissolve with the body (no weapon lingers);
* ``previews`` — ``Previews/corpse_husks/<asset>/``: the death pose (+16 ms), the grey done (+830), the crumble
  (+1280, ``corpse.crumble`` on the husk's centre) and the husk (+2130) at the W1 camera on the march ground
  (``M_AF_AshDissolve`` emulated per pixel as ``vfx/fx_preview_dark.py`` does; breach-blood: the death end only).
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE.parent / "coherence",
           HERE.parent / "environment", HERE.parent / "vfx", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402

from kit import export, paths, review  # noqa: E402

PREVIEW_DIR = "corpse_husks"
TIMELINE = {"material": "M_AF_AshDissolve", "contract": "manifest.materials.M_AF_AshDissolve (timeline)",
            "holdMs": 450, "greyMs": 380, "crumbleMs": 900, "crumbleTo": 0.38, "expireMs": 700, "ttlMs": 20000}
RECIPES = {"crumble": "corpse.crumble", "expire": "corpse.expire", "burn": "corpse.burn",
           "consume": "corpse.consume", "raiseMark": "monster.raise.mark", "raise": "monster.raise"}
CARCASS_RECIPES = {"crumble": "corpse.crumble", "expire": "corpse.expire", "burn": "corpse.burn"}

# asset → (look, family, rise, note)
ROSTER = {
    "SK_Mon_RisenFootman": ("husk", "unburied", "Rise", "folded forward over the knees, face down (the kneel-fold "
                            "the kit calls the ash-husk pose); the sword / shovel beside him"),
    "SK_Mon_RisenArbalest": ("husk", "unburied", "Rise", "folded and rolled onto the flank, the pavise on its edge "
                             "beside him, the crossbow under the arm"),
    "SK_Mon_RisenShieldbearer": ("husk", "unburied", "Rise", "folded forward, the tower shield laid on the ground "
                                 "beside him"),
    "SK_Mon_DeadStandardBearer": ("husk", "unburied", "Rise", "kneeling folded at the foot of the planted standard, "
                                  "the hand still on it (the standard dissolves with him)"),
    "SK_Mon_OathZealot": ("husk", "living", None, "face down, the flail flung flat"),
    "SK_Mon_Deserter": ("husk", "living", None, "face down, the axe beside him (Dreck / Harke: their attachments "
                        "dissolve with the body)"),
    "SK_Mon_DeserterDrummer": ("husk", "living", None, "on his side, the drum showing"),
    "SK_Mon_Gravegnawer": ("carcass", "gnawers", None, "the carcass on its side, knees drawn up (the other gnawers "
                           "ignore it: no Consume, no raise)"),
    "SK_Mon_Bonegorger": ("carcass", "gnawers", None, "on the belly, rolled onto its side (Hungerbell at x1.25)"),
    "SK_Mon_Breachclot": ("puddle", "breach", None, "melts into the tar puddle (fx.puddle: dries in 3 s), the helm "
                          "lying in it; no corpse entry"),
    "SK_Mon_Cinderbloat": ("puddle", "breach", None, "the fuse bursts (FX_Burst 800): the crust collapses, six char "
                           "shards on the scorched puddle; no corpse entry"),
    "SK_Mon_Corpseherd": ("husk", "unburied", "Rise", "on her knees, folded over them, the hood bowed over her right "
                          "hand on the bell (cs_rf_herd looks at it: the grey waits for the beat)"),
    "SK_Mon_Valdras": ("husk", "unburied", "Rise", "kneeling on the dais, sword point-down, the standard drooped "
                       "(cs_rf_marshal_falls looks at it: the grey waits for the beat)"),
}
STORY_HOLD = {"SK_Mon_Corpseherd": "cs_rf_herd", "SK_Mon_Valdras": "cs_rf_marshal_falls"}
RAISED_AS = "monster_risen_footman"     # monsters.md 2.0: a raised Unburied or Living corpse stands as the footman


def _clip(e: dict, name: str) -> dict | None:
    return next((c for c in e.get("anims", []) if c["name"] == name), None)


def measure(stage, asset: str) -> dict:
    """The fallen body at the Death clip's last frame: bounds / footprint of the posed body + held items and the
    anchor sockets, in the actor frame (forward, right, up cm) — the actor at the origin facing UE +X."""
    import fx_preview as FP
    import fx_preview_dark as FD
    ch = stage.figure(asset)
    stage.place(ch, np.zeros(3), np.array([1.0, 0.0, 0.0]))
    death = _clip(ch["entry"], "Death")
    t_end = float(death["lengthMs"]) - 1000.0 / 60.0 * 0.25
    stage.set_clip(ch, "Death", t_end, hold=True)
    bpy.context.scene.frame_set(int(FD.F0))
    bpy.context.view_layer.update()
    pts = review.mesh_points([m for m in ch["meshes"] if not m.hide_render], step=1)
    ue = np.stack([pts[:, 0] * 100.0, -pts[:, 1] * 100.0, pts[:, 2] * 100.0], axis=1)   # UE cm: x fwd, y right
    lo, hi = ue.min(axis=0), ue.max(axis=0)
    c = (lo + hi) * 0.5
    c[2] = max(0.0, lo[2])
    rad = float(np.percentile(np.hypot(ue[:, 0] - c[0], ue[:, 1] - c[1]), 97))
    sk = stage.sockets(ch)
    pelvis = ch["arm"].matrix_world @ ch["arm"].pose.bones["pelvis"].head if "pelvis" in ch["arm"].pose.bones else None
    r1 = lambda v: [round(float(x), 1) for x in v]          # noqa: E731
    anchors = {n: r1(sk[k]) for n, k in (("chest", "fx_chest"), ("head", "fx_head")) if k in sk}
    if pelvis is not None:
        anchors["pelvis"] = r1(FP.ue_cm(pelvis))
    for m in ch["meshes"]:
        m.hide_render = True
    return {"centerCm": r1(c), "boundsCm": {"min": r1(lo), "max": r1(hi)}, "radiusCm": round(rad, 1),
            "topCm": round(float(hi[2]), 1), "anchors": anchors,
            "frame": "actor frame at the Death clip's last frame: forward, right, up cm (the VFX offset convention; "
                     "UE: +X forward, +Y right of the actor's facing)"}


def block(asset: str, e: dict, husk: dict, previews: list[str]) -> dict:
    look, family, rise, note = ROSTER[asset]
    death = _clip(e, "Death")
    out = {"look": look, "family": family, "deathClip": "Death", "holdFromMs": float(death["lengthMs"]),
           "note": note, "spec": "Docs/dark-remake/monsters.md 1.5 (DR6 ash mode); implementation-plan.md U3"}
    if look == "puddle":
        out.update({"registry": False, "puddle": "manifest fx.puddle" if "puddle" in (e.get("fx") or {}) else
                    "the scorched puddle under the burst (notify.FX_Burst's scorch + the Death clip's shards)",
                    "timeline": None, "recipes": {}})
    else:
        tl = dict(TIMELINE)
        if asset in STORY_HOLD:
            tl["holdUntil"] = "story"
            tl["story"] = STORY_HOLD[asset]
            tl["holdNote"] = ("the death pose holds (no grey, no crumble) while the beat that looks at it plays; the "
                              "timeline starts when it ends")
        out.update({"registry": True, "timeline": tl,
                    "recipes": dict(CARCASS_RECIPES if look == "carcass" else RECIPES),
                    "items": "every held item / attachment takes the same dissolve (no weapon lingers on the ground)",
                    "shadow": "the blob shadow follows the pelvis (fx.blobShadow deathFollowBone where set) and fades "
                              "with the crumble to 0.35, gone at expire"})
        if look == "carcass":
            out["consumable"] = False
            out["raisable"] = False
            out["carcassNote"] = "monsters.md 1.5: the other gnawers ignore it (no Consume); not Unburied / Living (no raise)"
        if asset in STORY_HOLD:
            # art review: a later gnawer or raiser must not eat or raise the act boss / the mini-boss
            out["consumable"] = False
            out["raisable"] = False
            out["storyGuard"] = (f"{STORY_HOLD[asset]} looks at this body: never Consumed, never raised by another "
                                 "monster (permanent, not only while holdUntil story holds); its own rise (the "
                                 "Unburied trait) is the core's rule")
        if rise and _clip(e, rise):
            r = _clip(e, rise)
            out["rise"] = {"clip": rise, "lengthMs": float(r["lengthMs"]),
                           "riseUpMs": next((n["ms"] for n in r.get("notifies", []) if n["name"] == "RiseUp"), None),
                           "dissolve": "reverse the grey + dissolve over the clip's first 60 % (contract 'rise')",
                           "use": "the self-rise only (the Unburied trait / affix re-raises this monster's own "
                                  "corpse); a raise by another monster stands it up as raisedAs"}
        if look == "husk":
            out["raisedAs"] = RAISED_AS
            out["raisedNote"] = ("a raise by another monster (the Corpseherd, a standard-bearer, Valdras) stands this "
                                 "corpse up as a risen footman (monsters.md 2.0): U3 removes the husk on "
                                 "EvCorpseRemoved{raised} and the footman spawns at the corpse with its own Rise"
                                 + ("; raisable false wins here (storyGuard)" if asset in STORY_HOLD else ""))
    out["husk"] = husk
    if previews:
        out["previews"] = previews
    return out


FACE = (-1.0, -0.3)                     # the preview figure's facing (screen right, up), as the A12 corpse scenes


def render(stage, recipes: dict, asset: str, out_dir: Path, husk: dict) -> list[str]:
    """``Previews/corpse_husks/<asset>/``: the Death end held, the grey (+450 → +830 ms), the crumble (corpse.crumble
    at +830, on the husk's centre) and the husk at +2130 ms (puddles: the Death end only); the crop is sized and
    centred on the measured husk (a planted standard or a kneeling boss stands tall)."""
    import fx_preview_dark as FD
    look = ROSTER[asset][0]
    e = stage.figure(asset)["entry"]
    L = float(_clip(e, "Death")["lengthMs"])
    t0 = L - 1000.0 / 60.0 * 0.25
    fig = FD.Fig("d", asset, (0, 0), FACE, clip="Death", t0=t0, hold=True)
    f = np.array(FACE) / np.hypot(*FACE)
    rt = np.array([f[1], -f[0]])                            # the figure's right, in screen (right, up) axes
    cf, cr = husk["centerCm"][0], husk["centerCm"][1]
    ctr = f * cf + rt * cr
    r, top = float(husk["radiusCm"]), float(husk["topCm"])
    crop = (int(min(1000, max(420, (2.0 * r + 160.0) * 1.2))),
            int(min(1000, max(360, (0.66 * top + 1.6 * r + 160.0) * 1.2))))
    focus = (float(ctr[0]), float(ctr[1]) + 0.3 * max(0.0, top - 110.0))
    if look == "puddle":
        s = FD.DScene(f"{asset}", [fig], [], [10.0], crop=crop, focus=focus, tight=False,
                      note="breach-blood: no husk (the tar puddle is the runtime fx.puddle decal)")
    else:
        tl = TIMELINE
        g0 = float(tl["holdMs"])
        c0 = g0 + tl["greyMs"]
        s = FD.DScene(f"{asset}", [fig], [(c0, "point", (float(ctr[0]), float(ctr[1])), "corpse.crumble")],
                      [10.0, c0, c0 + 450.0, c0 + tl["crumbleMs"] + 400.0], crop=crop, focus=focus, tight=False,
                      ash={"fig": "d", "grey": g0}, note="the Death end held, grey, crumble, the husk")
    written = FD.run(stage, recipes, s, out_dir)
    return [f"{PREVIEW_DIR}/{asset}/{p.name}" for p, _ in written]


def main(argv: list[str]) -> int:
    import fx_preview as FP
    import fx_preview_dark as FD
    sel = [a for a in argv if not a.startswith("--")]
    assets = [a for a in ROSTER if not sel or a in sel or any(s.lower() in a.lower() for s in sel)]
    out_dir = (paths.scratch_root("w2", PREVIEW_DIR) if "--scratch" in argv else paths.preview_root() / PREVIEW_DIR)
    stage = FD.DarkStage()
    recipes = FP.load_recipes()
    results = {}
    for asset in assets:
        e = stage.figure(asset)["entry"]
        husk = measure(stage, asset)
        prev = [] if "--no-previews" in argv else render(stage, recipes, asset, out_dir, husk)
        results[asset] = block(asset, e, husk, prev)
        print(f"[corpse] {asset}: {results[asset]['look']} centre {husk['centerCm']} r {husk['radiusCm']} "
              f"top {husk['topCm']}  previews {len(prev)}", flush=True)
    if "--dry-run" in argv:
        print(json.dumps(results, indent=1, ensure_ascii=False)[:6000])
        return 0
    man = export.Manifest()
    for asset, b in results.items():
        if "previews" not in b:
            old = (man.data["assets"][asset].get("corpse") or {}).get("previews")
            if old:
                b["previews"] = old
        man.data["assets"][asset]["corpse"] = b
    man.save()
    print(f"[corpse] manifest: {len(results)} corpse blocks")
    return 0


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(args))
