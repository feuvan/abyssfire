"""Lord-Marshal Valdras where he is fought — the Breach Rim mood cell (review round 2, item 13).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/valdras_rim.py [--out=<dir>]

The shipped ``SK_Mon_Valdras`` (re-imported from ``Art/Export`` by ``coherence/cohkit``, posed by his exported
clips) standing at the ``centre`` socket of the shipped ``SM_Prop_March_BreachDais`` on the exported march dirt, at
the W1 game camera (native 1080p, the shipped ink, bloom, the runtime visorGlow card), in three poses — Idle F0,
Roar F27 (FX_Roar) and Death F120 (the cutscene pose) — each shown **neutral** and in the **Breach Rim region
mood** (chapter1.md §15, ``Data/zone_moods.json`` ``breach_rim``):

* the violet uplight from the crack: one toon light slot (``hlsl._TORCH_SLOT``: Emit += base × (1 − e) × C ×
  att² × wrap, att = saturate(1 − d² / r²), the normal wrap 0.35 → 1 over h .40-.50) at the dais' ``breach``
  socket, C = linear(#B02FD0) × alpha .6 × the region's lightScale .40 × ``abyss.CharLightGain`` 3.5, radius 4 m
  (the uplight recipe is not authored yet: these are the assumed values, stated on the sheet);
* the region grade at 55 % (DECISIONS P14; ``march_review.grade``'s approximation): ambient #6A4A8A α .35,
  saturation .60, contrast 1.15, lift / gain, plus the haze #2A1A36 α .14 and the vignette #0A0610 α .62 by the
  crop's place in the 1080p frame.

Measured per cell (the questions of the review: do the violet seams and the violet-lipped visor still separate
under a violet ambient?): ``MK`` the visible marker (``valdras.marker``) block and its median L* against the helm
plate round it; ``SEAM`` the median ΔE (CIE76) between the emissive seam pixels and the body pixels within 3 px of
them; ``LIT`` the darkgate lit-side |ΔL*| against the dais top. → ``Art/Previews/mon_valdras/rim_mood_<pose>.png``
(neutral | mood), ``rim_mood.json``.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "coherence", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import cohkit as K  # noqa: E402  (sets up the kit / environment import paths)
import darkgate as DG  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import color, paths, pngio, review, shading  # noqa: E402

ASSET = "SK_Mon_Valdras"
DAIS = "SM_Prop_March_BreachDais"
POSES = (("Idle", 0), ("Roar", 27), ("Death", 120))
RES = (1920, 1080)
# Breach Rim region mood (Data/zone_moods.json breach_rim; chapter1.md §15)
MOOD = dict(ambient="#6A4A8A", alpha=0.35, sat=0.60, con=1.15, lift=(-.012, -.014, .010), gain=(.010, 0.0, .030),
            haze="#2A1A36", haze_alpha=0.14, vignette="#0A0610", vignette_alpha=0.62)
P14 = 0.55                                     # the grade applies 55 % of the mood
UPLIGHT = dict(color="#B02FD0", alpha=0.6, light_scale=0.40, gain=3.5, radius=4.0)
WRAP = (0.35, 0.40, 0.50)                      # hlsl.TORCH_WRAP


def srgb_lin(a):
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def lin_srgb(a):
    a = np.clip(a, 0, None)
    return np.where(a <= 0.0031308, a * 12.92, 1.055 * np.power(a, 1 / 2.4) - 0.055)


def grade(img: np.ndarray, box, res=RES) -> np.ndarray:
    """The region grade at ``P14`` of the mood on a crop ``img`` taken at ``box`` of the ``res`` frame."""
    lin = srgb_lin(img.astype(np.float32) / 255.0)
    amb = srgb_lin(np.array(color.hex_to_rgb(MOOD["ambient"]), np.float32) / 255.0)
    lin = lin * (1.0 + (amb - 1.0) * MOOD["alpha"] * P14)
    H, W = img.shape[:2]
    x0, y0 = review.border_origin(box, res)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    u, v = (xx + x0 + 0.5) / res[0], (yy + y0 + 0.5) / res[1]
    hz = np.clip(1.0 - np.hypot(u - 0.5, v - 0.5) / 0.625, 0, 1)[..., None]
    haze = srgb_lin(np.array(color.hex_to_rgb(MOOD["haze"]), np.float32) / 255.0)
    lin = lin + haze * MOOD["haze_alpha"] * P14 * hz
    sat = 1.0 + (MOOD["sat"] - 1.0) * P14
    y = lin @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    lin = y[..., None] + (lin - y[..., None]) * sat
    con = 1.0 + (MOOD["con"] - 1.0) * P14
    lin = 0.18 * np.power(np.clip(lin, 0, None) / 0.18, con)
    y = np.clip(lin @ np.array([0.2126, 0.7152, 0.0722], np.float32), 0, 1)[..., None]
    lin = lin + np.array(MOOD["lift"], np.float32) * P14 * (1 - y) + np.array(MOOD["gain"], np.float32) * P14 * y
    r = np.hypot((u - 0.5) * 1.6, v - 0.5)[..., None]
    vg = np.clip((r - 0.35) / 0.55, 0, 1) ** 1.5 * MOOD["vignette_alpha"] * P14
    vc = srgb_lin(np.array(color.hex_to_rgb(MOOD["vignette"]), np.float32) / 255.0)
    lin = lin * (1 - vg) + vc * vg
    return (lin_srgb(lin).clip(0, 1) * 255 + 0.5).astype(np.uint8)


def add_uplight(meshes, pos: Vector) -> list:
    """The violet uplight as one toon light slot on every toon slot of ``meshes`` (copies of their materials):
    Emit += linear(base) × (1 − e) × C × att² × wrap. Returns the (mesh, index, old material) list to restore."""
    c = np.array(shading.color.hex_linear(UPLIGHT["color"])) * UPLIGHT["alpha"] * UPLIGHT["light_scale"] * \
        UPLIGHT["gain"]
    r2 = UPLIGHT["radius"] ** 2
    saved, done = [], {}
    for m in meshes:
        mats = m.data.materials
        for i in range(len(mats)):
            mt = mats[i]
            if mt is None or i == 1 or not mt.use_nodes:
                continue
            grp = next((n for n in mt.node_tree.nodes if n.type == "GROUP" and "Base" in n.inputs), None)
            if grp is None or not grp.inputs["Base"].links:
                continue
            if mt.name not in done:
                nm = mt.copy()
                nm.name = mt.name + "_Uplit"
                nt = nm.node_tree
                b = shading.NB(nt)
                g = next(n for n in nt.nodes if n.type == "GROUP" and "Base" in n.inputs)
                base = g.inputs["Base"].links[0].from_socket
                _, _, e = b.sep(g.inputs["Params"].links[0].from_socket)
                geo = b.node("ShaderNodeNewGeometry")
                td = b.vmath("SUBTRACT", b.const_vec(tuple(pos)), geo.outputs["Position"])
                d2 = b.vmath("DOT_PRODUCT", td, td)
                att = b.math("MAXIMUM", b.math("SUBTRACT", 1.0, b.math("DIVIDE", d2, r2)), 0.0)
                att2 = b.math("MULTIPLY", att, att)
                tn = b.math("MULTIPLY_ADD", b.vmath("DOT_PRODUCT", geo.outputs["Normal"], b.vmath("NORMALIZE", td)),
                            0.5, 0.5)
                wrap = b.math("MULTIPLY_ADD", b.smooth(tn, WRAP[1], WRAP[2]), 1.0 - WRAP[0], WRAP[0])
                k = b.math("MULTIPLY", b.math("MULTIPLY", att2, wrap), b.math("SUBTRACT", 1.0, e))
                add = b.vmath("MULTIPLY", b.srgb_to_linear(base), b.const_vec(tuple(c)))
                add = b.vmath("SCALE", add, scale=k)
                em2 = b.node("ShaderNodeEmission")
                b._set(em2.inputs["Color"], add)
                out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
                prev = out.inputs["Surface"].links[0].from_socket
                addsh = b.node("ShaderNodeAddShader")
                nt.links.new(prev, addsh.inputs[0])
                nt.links.new(em2.outputs[0], addsh.inputs[1])
                nt.links.new(addsh.outputs[0], out.inputs["Surface"])
                done[mt.name] = nm
            saved.append((m, i, mt))
            mats[i] = done[mt.name]
    return saved


def lab(rgb: np.ndarray) -> np.ndarray:
    a = srgb_lin(rgb.astype(np.float32) / 255.0)
    M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]], np.float32)
    xyz = a @ M.T / np.array([0.95047, 1.0, 1.08883], np.float32)
    f = np.where(xyz > 216 / 24389, np.cbrt(xyz), (24389 / 27 * xyz + 16) / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)


def dilate(m: np.ndarray, n: int) -> np.ndarray:
    out = m.copy()
    for _ in range(n):
        p = np.pad(out, 1)
        out = out | p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]
    return out


def _cov(objs, hide, box) -> np.ndarray:
    """Body coverage of ``objs`` alone (``hide`` and the ground out of the render: a coverage pass only swaps the
    listed meshes' materials, anything else would draw its own colour into the coverage channels)."""
    stage = [o for o in bpy.data.objects if o.name.startswith("AF_Stage") or o.name.startswith("rim_")]
    keep = [(o, o.hide_render) for o in list(hide) + stage]
    for o, _ in keep:
        o.hide_render = True
    try:
        b, _ = review.coverage_pass(objs, box, ground=False)
    finally:
        for o, h in keep:
            o.hide_render = h
    return b


def measure(img, fig, every, box, dais_mask, body, spec) -> dict:
    """MK / SEAM / LIT of one frame (see the module docstring)."""
    L = lab(img)
    mk = DG._fit(DG.marker_pass(fig, every, spec, box), body.shape) >= 0.5
    em = K.LAST_EMISSIVE.get("em")
    glow = DG._fit(em[:, :, :3].max(-1), body.shape) > K.EMISSIVE_EDGE_MIN if em is not None else np.zeros_like(mk)
    solid = body > 0.98
    rec = {}
    ring = dilate(mk, 3) & solid & ~mk & ~glow
    rec["marker"] = {"blockPx": DG.largest_block(mk), "L": round(float(np.median(L[..., 0][mk])), 1) if mk.any()
                     else None,
                     "dLvsPlate": round(float(np.median(L[..., 0][mk]) - np.median(L[..., 0][ring])), 1)
                     if mk.any() and ring.any() else None}
    seam = glow & solid & ~mk
    near = dilate(seam, 3) & solid & ~glow & ~mk
    if seam.any() and near.any():
        ls, ln = np.median(L[seam], 0), np.median(L[near], 0)
        rec["seams"] = {"pixels": int(seam.sum()), "L": round(float(ls[0]), 1), "dE": round(float(np.linalg.norm(ls - ln)), 1),
                        "dL": round(float(ls[0] - ln[0]), 1)}
    h, rimw = DG.light_pass(fig, every, box)
    h, rimw = DG._fit(h, body.shape), DG._fit(rimw, body.shape)
    lit = solid & ~mk & ~glow & ((h >= shading.T_LIGHT) | (rimw >= DG.RIM_LIGHT_ON))
    top = dais_mask & ~(body > 0.02)
    if lit.any() and top.any():
        gl = float(np.median(L[..., 0][top]))
        rec["lit"] = {"daisL": round(gl, 1), "litL": round(float(np.median(L[..., 0][lit])), 1),
                      "deltaL": round(float(np.median(np.abs(L[..., 0][lit] - gl))), 1)}
    return rec


def main(argv) -> int:
    out = Path(next((a.split("=", 1)[1] for a in argv if a.startswith("--out=")), paths.preview_dir("mon_valdras")))
    cells, report = [], {"tool": "monsters/valdras_rim.py", "mood": MOOD, "p14": P14, "uplight": UPLIGHT, "cells": []}
    for clip, frame in POSES:
        K.reset()
        review.setup_render(RES, 8)
        cam = review.game_camera((0.0, 0.0))
        dais = K.import_static(DAIS)
        de = K.entry(DAIS)
        cs = next(s for s in de["sockets"] if s["name"] == "centre")
        bs = next(s for s in de["sockets"] if s["name"] == "breach")
        at = Vector((cs["locCm"][0] / 100.0, -cs["locCm"][1] / 100.0, cs["locCm"][2] / 100.0))
        fig = K.import_character(ASSET, tuple(at), review.FACINGS["se"], clip=clip, frame=frame, cam=cam)
        every = [dais] + fig.all()
        pts = review.mesh_points(every, step=8)
        c0, c1 = int(pts[:, 0].min()) - 6, int(pts[:, 0].max()) + 7
        r0, r1 = int(-pts[:, 1].max()) - 6, int(-pts[:, 1].min()) + 7
        ground = DG.Ground("march", "dirt", None)
        DG.ground_plane(ground.image(r1 - r0, c1 - c0, (c0, r0), 48), (c0, r0), (r1 - r0, c1 - c0), 48,
                        [(at.x, -at.y, 0.46, 0.45)], name=f"rim_{clip}")
        bpy.context.view_layer.update()
        box = K.projected_box(cam, fig.all(), RES, pad_px=(70, 50))     # the figure and the dais round him
        spec = DG.marker_spec(fig)
        body = None
        for mood in (False, True):
            saved = []
            if mood:
                bp = Vector((bs["locCm"][0] / 100.0, -bs["locCm"][1] / 100.0, bs["locCm"][2] / 100.0))
                saved = add_uplight(every, bp)
            img, raw = K.render(every, box)
            if mood:
                img = grade(img, box)
            if body is None:
                body = DG._fit(_cov(fig.meshes, [dais] + fig.extra, box), img.shape[:2])
                dais_mask = DG._fit(_cov([dais], fig.all(), box), img.shape[:2]) > 0.98
            rec = measure(img, fig, every, box, dais_mask, body, spec)
            for m, i, mt in saved:
                m.data.materials[i] = mt
            tag = f"{clip} F{frame} {'BREACH RIM MOOD' if mood else 'NEUTRAL'}"
            img = np.ascontiguousarray(img)
            review.draw_text(img, 4, 4, tag)
            mkr, sm, lt = rec.get("marker", {}), rec.get("seams", {}), rec.get("lit", {})
            review.draw_text(img, 4, 14, f"MK {mkr.get('blockPx')}PX  DL VS PLATE {mkr.get('dLvsPlate')}",
                             rgb=(160, 230, 160) if (mkr.get("blockPx") or 0) >= 2 else (240, 90, 90))
            review.draw_text(img, 4, 24, f"SEAM DE {sm.get('dE')}  DL {sm.get('dL')}", rgb=(255, 220, 140))
            review.draw_text(img, 4, 34, f"LIT DL {lt.get('deltaL')} VS DAIS L {lt.get('daisL')}",
                             rgb=(160, 230, 160) if (lt.get("deltaL") or 0) >= DG.LIT_DL else (240, 90, 90))
            if mood:
                review.draw_text(img, 4, img.shape[0] - 10, "UPLIGHT #B02FD0 A.6 X LS .40 X GAIN 3.5 R 4M (ASSUMED)",
                                 rgb=(200, 180, 230))
            cells.append(img)
            report["cells"].append({"clip": clip, "frame": frame, "mood": "breach_rim" if mood else "neutral", **rec})
            print(f"[rim] {tag}: {json.dumps(rec)}")
    out.mkdir(parents=True, exist_ok=True)
    for k, (clip, frame) in enumerate(POSES):           # one sheet per pose: neutral | Breach Rim mood
        pair = cells[2 * k:2 * k + 2]
        h = max(c.shape[0] for c in pair)
        pair = [np.pad(c, ((0, h - c.shape[0]), (0, 0), (0, 0)), constant_values=24) for c in pair]
        for q, sz in pngio.write_png_native(out / f"rim_mood_{clip.lower()}.png", review.grid(pair, 2)):
            print(f"  {q.name} {sz // 1024} KB")
    (out / "rim_mood.json").write_text(json.dumps(report, indent=1) + "\n")
    return 0


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(args))
