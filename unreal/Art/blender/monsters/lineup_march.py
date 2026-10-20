"""March family lineup (A7 / A8 family pass) — ``Art/Previews/mon_family_march/lineup_march_<page>.png``.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/lineup_march.py
        [--ground-hex=#3A332D]        flat march ground (default: the mud stress colour; ``--theme=march`` once A10
                                      exports the march terrain set)

The Chapter-1 march roster re-imported from ``Art/Export`` (``coherence/cohkit``: shipped FBX posed by its Idle F0,
family palette atlases, the UE screen-constant ink of its slot, emissive bloom) beside the shipped warrior for R2
scale, at the W1 game camera (yaw 45°, pitch −50°, FOV 35°, default distance), native 1080p, se facing, web
contact shadows on the ground (``coherence/darkgate`` ground plane). Three pages:

1. the Unburied and their marshal: warrior · risen footman · shieldbearer · arbalester · dead standard-bearer ·
   Lord-Marshal Valdras (boss);
2. the living and the gnawers: warrior · oath zealot · deserter · drummer · gravegnawer · bonegorger;
3. the breach, the Corpseherd and the elite markers: warrior · breachclot · cinderbloat · the Corpseherd ·
   a footman with ``SM_FX_EliteCrown_March`` (W1, ``elite_crown@march``) · a gravegnawer with
   ``SM_FX_EliteSigil`` (W2, ``elite_sigil``), both on ``fx_overhead`` as the runtime places them.

Each figure is ink-gated on its own coverage (``cohkit.ink_gate_group``: its request − ``outline.GATE_SLACK_PX``,
3.0 px for the characters, 2.5 px for a figure carrying a 3.0 px elite marker) and labelled with its
manifest height and its posed top; ``lineup_march.json`` keeps the numbers. Exit 1 on an ink failure.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "coherence", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import cohkit as K  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix  # noqa: E402

import darkgate as DG  # noqa: E402
from kit import outline, paths, review  # noqa: E402

OUT_DIR = "mon_family_march"
RES = (1920, 1080)
SPACING = 1.25
PAGES = [
    ("unburied", [("SK_Hero_Warrior", "WARRIOR", None), ("SK_Mon_RisenFootman", None, None),
                  ("SK_Mon_RisenShieldbearer", None, None), ("SK_Mon_RisenArbalest", None, None),
                  ("SK_Mon_DeadStandardBearer", None, None), ("SK_Mon_Valdras", None, None)]),
    ("living", [("SK_Hero_Warrior", "WARRIOR", None), ("SK_Mon_OathZealot", None, None),
                ("SK_Mon_Deserter", None, None), ("SK_Mon_DeserterDrummer", None, None),
                ("SK_Mon_Gravegnawer", None, None), ("SK_Mon_Bonegorger", None, None)]),
    ("breach_elites", [("SK_Hero_Warrior", "WARRIOR", None), ("SK_Mon_Breachclot", None, None),
                       ("SK_Mon_Cinderbloat", None, None), ("SK_Mon_Corpseherd", None, None),
                       ("SK_Mon_RisenFootman", "ELITE W1 CROWN", "SM_FX_EliteCrown_March"),
                       ("SK_Mon_Gravegnawer", "ELITE W2 SIGIL", "SM_FX_EliteSigil")]),
]


def label_of(asset: str, override: str | None) -> str:
    if override:
        return override
    e = K.entry(asset)
    return DG._label((e.get("displayName") or {}).get("en") or asset.replace("SK_Mon_", ""))


def place_marker(name: str, fig, cam):
    """The elite marker on ``fx_overhead``: the crown yawed to the camera (as the runtime's W1 crown), the sigil
    level (its manifest ``fx.motion`` spins it about Z)."""
    mk = K.import_static(name)
    wp = K._socket_world(K.entry(fig.asset), fig.arm, "fx_overhead")
    if name == "SM_FX_EliteSigil":
        mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.radians(-20.0), 4, "Z")
    else:
        dv = cam.matrix_world.translation - wp
        mk.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(math.atan2(dv.x, -dv.y), 4, "Z")
    mk["af_px1080"] = K.slot_info(K.entry(name))[2]
    fig.extra.append(mk)
    return mk


def shoot(tag: str, page: list, ground: DG.Ground, out: Path) -> list[dict]:
    K.reset()
    review.setup_render(RES, 8)
    cam = review.game_camera((0.0, 0.0))
    r, _ = DG.screen_axes(cam)
    yaw = review.FACINGS["se"]
    figs = []
    for i, (asset, lab, marker) in enumerate(page):
        at = r * ((i - (len(page) - 1) / 2) * SPACING)
        f = K.import_character(asset, at, yaw, label=label_of(asset, lab), cam=cam)
        bpy.context.view_layer.update()
        if marker:
            place_marker(marker, f, cam)
        figs.append(f)
    bpy.context.view_layer.update()
    every = [m for f in figs for m in f.all()]
    pts = review.mesh_points(every, step=8)
    c0, c1 = int(pts[:, 0].min()) - 10, int(pts[:, 0].max()) + 11
    r0, r1 = int(-pts[:, 1].max()) - 10, int(-pts[:, 1].min()) + 11
    ppt = 48
    shadows = [(f.root.location.x, -f.root.location.y,
                K.entry(f.asset).get("blobShadowRadiusCm", 30) / 100.0 * f.root.scale[0], 0.45) for f in figs]
    DG.ground_plane(ground.image(r1 - r0, c1 - c0, (c0, r0), ppt), (c0, r0), (r1 - r0, c1 - c0), ppt, shadows,
                    name=f"lm_{tag}")
    bpy.context.view_layer.update()
    box = K.projected_box(cam, every, RES, pad_px=(40, 80))
    img, raw = K.render(every, box)
    gmap = img.copy()
    recs = []
    for f in figs:
        # the kit's gate: the request − GATE_SLACK_PX (the characters' 3.5 px → 3.0 = CHAR_GATE_PX); a figure
        # carrying an elite marker is gated at its marker's 3.0 px request − slack (the markers' own ship gate)
        req = min([f.px] + [float(m.get("af_px1080", f.px)) for m in f.extra])
        gate = min(K.CHAR_GATE_PX, req - outline.GATE_SLACK_PX)
        st = K.ink_gate_group(f.all(), every, box, img, raw, f.cls, cam, min_px=gate,
                              emissive=K.LAST_EMISSIVE.get("em"))
        ox, oy = st["offset"]
        ys, xs = st["edge_yx"]
        ok = st["edge_ok"]
        gmap[ys[ok] + oy, xs[ok] + ox] = (60, 230, 90)
        gmap[ys[~ok] + oy, xs[~ok] + ox] = (240, 60, 60)
        frac = float(st.get("fracRim", ok.mean() if len(ok) else 0.0))
        H = st["body"].shape[0]
        rows = np.nonzero(st["body"].max(1) > 0.5)[0]
        recs.append({"asset": f.asset, "label": f.label, "class": f.cls, "outlinePx1080": f.px, "gatePx": gate,
                     "heightCm": K.entry(f.asset).get("heightCm"), "posedTopCm": round(f.top_cm(), 1),
                     "fracRim": round(frac, 4), "ok": frac >= DG.RIM_FRAC,
                     "markers": [m.name.replace("COH_", "") for m in f.extra],
                     "_anchor": f.root.location.copy(), "_bottom": oy + (int(rows.max()) if len(rows) else H)})
        print(f"  [{tag}] {f.label:24s} {f.cls:13s} h {recs[-1]['heightCm']} cm  top {recs[-1]['posedTopCm']} cm  "
              f"rim {frac * 100:5.1f}% (>= {gate:.1f} px)  {'PASS' if recs[-1]['ok'] else 'FAIL'}")
    x0, y0 = review.border_origin(box, RES)
    for im in (img, gmap):
        for rec in recs:
            p = review.project_px(cam, rec["_anchor"], RES)
            lines = [(rec["label"], (240, 236, 220)),
                     (f"{rec['heightCm']:.0f} CM" if rec["heightCm"] else "", (200, 200, 200)),
                     (f"RIM {rec['fracRim'] * 100:.0f}%", (160, 230, 160) if rec["ok"] else (240, 90, 90))]
            for k, (txt, col) in enumerate(lines):
                tx, ty = int(p[0] - x0 - len(txt) * 3), max(int(p[1] - y0 + 14), rec["_bottom"] + 8) + 9 * k
                review.draw_text(im, max(2, tx), min(im.shape[0] - 9, max(2, ty)), txt, rgb=col)
    head = f"MARCH LINEUP  {tag.upper()}  {ground.run_key.upper()} L {ground.stats['L50']:.0f}  W1 CAMERA 1080P SE"
    DG.write(out / f"lineup_march_{tag}.png", img, head)
    DG.write(out / f"lineup_march_{tag}_inkgate.png", gmap, "INK GATE: EDGE GREEN GE GATE (3PX, 2.5 WITH A MARKER) / RED THINNER")
    for rec in recs:
        rec.pop("_anchor")
        rec.pop("_bottom")
    return recs


def main(argv: list[str]) -> int:
    hexc = next((a.split("=", 1)[1] for a in argv if a.startswith("--ground-hex=")), None)
    theme = next((a.split("=", 1)[1] for a in argv if a.startswith("--theme=")), None)
    if hexc is None and theme is None:
        hexc = "#3A332D"
    ground = DG.Ground(theme, "darkest", hexc)
    out = paths.preview_dir(OUT_DIR)
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("lineup_march_*.png"):
        p.unlink()
    res = {"ground": {"key": ground.run_key, **ground.stats}, "pages": {}}
    ok = True
    for tag, page in PAGES:
        recs = shoot(tag, page, ground, out)
        res["pages"][tag] = recs
        ok = ok and all(r["ok"] for r in recs)
    (out / "lineup_march.json").write_text(json.dumps(res, indent=1, ensure_ascii=False))
    print(f"[lineup] {'PASS' if ok else 'FAIL'} -> {out}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]))
