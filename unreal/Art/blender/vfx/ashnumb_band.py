"""The Ashnumb band on the heroes (dark remake Wave 2 art, 2H; art review r1): a Blender mock of the
``dark.materials.AshnumbTint`` contract (``vfx/fx_recipes_dark.ASHNUMB``) so the UE status presentation (2G) has a
visual target.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/ashnumb_band.py [--scratch]

Each hero's toon slot gets the contract's maths as material nodes after the toon group (nothing else changes; the ink
hull and the held items are untouched, as the contract says): per pixel the height fraction ``h`` = the posed
surface's height over the ground / the hero's ``heightCm``; ``top`` = min(``BandTopBase`` + stacks x
``BandTopPerStack``, ``MaxBandTop``); ``band`` = 1 - smoothstep(top - ``BandSoftness``, top, h); the toon colour
lerps by band x ``Strength`` toward ``AshColor`` where the toon's key light is in its lit bands and ``AshShade`` in
its shade band (h_light = N.L x .5 + .5 < ``T_SHADE``, the toon group's own split); an ``EdgeColor`` line
``EdgeWidth`` tall sits on the band's top edge (``top - EdgeWidth`` .. ``top``, full strength).

Output ``Previews/vfx_effects/AshnumbTint/band_stacks*.png``: the warrior, the mage and the rogue (rows) at 0, 1,
3 and 5 stacks (columns) at the W1 camera, native 1080p, se, on the darkest march tile (coherence/darkgate.py's
ground: march dirt), bloom on; Idle frame 0.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "coherence", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import cohkit as K  # noqa: E402  (sets up the kit / environment import paths)
import bpy  # noqa: E402
import numpy as np  # noqa: E402

import darkgate as DG  # noqa: E402
import fx_recipes_dark as DK  # noqa: E402
from kit import color, paths, pngio, review, shading  # noqa: E402

HEROES = [("SK_Hero_Warrior", "WARRIOR"), ("SK_Hero_Mage", "MAGE"), ("SK_Hero_Rogue", "ROGUE")]
STACKS = (0, 1, 3, 5)
RES = (1920, 1080)
SPACING = 1.25                   # m between the figures along screen-right
OUT = "vfx_effects/AshnumbTint"        # not status.ashnumb/: fx_preview_dark clears that folder


def band_top(stacks: int) -> float:
    P = DK.ASHNUMB["params"]
    return min(P["BandTopBase"] + stacks * P["BandTopPerStack"], P["MaxBandTop"])


def band_material(base: bpy.types.Material, stacks: int, height_m: float) -> bpy.types.Material:
    """A copy of the toon material ``base`` with the AshnumbTint band at ``stacks`` (0 = the copy unchanged)."""
    P = DK.ASHNUMB["params"]
    m = base.copy()
    m.name = f"AF_Ashnumb_{base.name}_{stacks}"
    if stacks <= 0:
        return m
    nt = m.node_tree
    b = shading.NB(nt)
    grp = next(n for n in nt.nodes if n.type == "GROUP")
    em = next(n for n in nt.nodes if n.type == "EMISSION")
    toon = grp.outputs["Color"]
    top = band_top(stacks)
    geo = b.node("ShaderNodeNewGeometry")
    _, _, z = b.sep(geo.outputs["Position"])
    h = b.math("DIVIDE", z, height_m)
    band = b.math("SUBTRACT", 1.0, b.smooth(h, top - P["BandSoftness"], top))
    ndl = b.vmath("DOT_PRODUCT", geo.outputs["Normal"], b.const_vec(shading.L_KEY))
    lit = b.math("GREATER_THAN", b.math("MULTIPLY_ADD", ndl, 0.5, 0.5), shading.T_SHADE)
    ash = b.node("ShaderNodeMix", data_type="RGBA")
    b.l.new(lit, ash.inputs["Factor"])
    ash.inputs[6].default_value = (*color.hex_linear(P["AshShade"]), 1.0)
    ash.inputs[7].default_value = (*color.hex_linear(P["AshColor"]), 1.0)
    mix = b.node("ShaderNodeMix", data_type="RGBA")
    b.l.new(b.math("MULTIPLY", band, P["Strength"]), mix.inputs["Factor"])
    b.l.new(toon, mix.inputs[6])
    b.l.new(ash.outputs[2], mix.inputs[7])
    edge = b.math("MULTIPLY", b.math("GREATER_THAN", h, top - P["EdgeWidth"]), b.math("LESS_THAN", h, top))
    line = b.node("ShaderNodeMix", data_type="RGBA")
    b.l.new(edge, line.inputs["Factor"])
    b.l.new(mix.outputs[2], line.inputs[6])
    line.inputs[7].default_value = (*color.hex_linear(P["EdgeColor"]), 1.0)
    for lk in list(em.inputs["Color"].links):
        nt.links.remove(lk)
    b.l.new(line.outputs[2], em.inputs["Color"])
    return m


def row(asset: str, label: str, ground, tag: str) -> np.ndarray:
    K.reset()
    review.setup_render(RES, 8)
    cam = review.game_camera((0.0, 0.0))
    r, _ = DG.screen_axes(cam)
    yaw = review.FACINGS["se"]
    e = K.entry(asset)
    hm = float(e["heightCm"]) / 100.0
    figs = []
    for i, n in enumerate(STACKS):
        at = r * ((i - (len(STACKS) - 1) / 2) * SPACING)
        f = K.import_character(asset, at, yaw, cam=cam, label=f"{n}")
        for m in f.meshes[:f.n_body]:            # the hero's skeletal mesh (its toon slot), not the held items
            if len(m.data.materials):
                m.data.materials[0] = band_material(m.data.materials[0], n, hm)
        figs.append((n, f))
    every = [m for _, f in figs for m in f.all()]
    pts = review.mesh_points(every, step=8)
    c0, c1 = int(pts[:, 0].min()) - 10, int(pts[:, 0].max()) + 11
    r0, r1 = int(-pts[:, 1].max()) - 10, int(-pts[:, 1].min()) + 11
    ppt = 48
    shadows = [(f.root.location.x, -f.root.location.y, e.get("blobShadowRadiusCm", 30) / 100.0, 0.45)
               for _, f in figs]
    DG.ground_plane(ground.image(r1 - r0, c1 - c0, (c0, r0), ppt), (c0, r0), (r1 - r0, c1 - c0), ppt, shadows,
                    name=f"ash_{tag}")
    bpy.context.view_layer.update()
    box = K.projected_box(cam, every, RES, pad_px=(30, 40))
    img, _ = K.render(every, box)
    img = np.ascontiguousarray(img)
    x0, y0 = review.border_origin(box, RES)
    for n, f in figs:
        p = review.project_px(cam, f.root.location, RES)
        top = band_top(n)
        txt = f"{n} STACK{'S' if n != 1 else ''}" + (f"  TOP {top:.2f}" if n else "  (NONE)")
        review.draw_text(img, max(2, int(p[0] - x0 - len(txt) * 3)), min(img.shape[0] - 10, int(p[1] - y0 + 12)),
                         txt, rgb=(240, 236, 220))
    review.draw_text(img, 4, 4, f"{label}  ASHNUMB BAND (AshnumbTint mock)  {ground.run_key.upper()}  W1 CAMERA "
                                f"1080P SE")
    return img


def main(argv: list[str]) -> int:
    ground = DG.Ground(None, "darkest", None)
    K.set_theme(ground.theme)
    rows = [row(a, lbl, ground, a.replace("SK_Hero_", "").lower()) for a, lbl in HEROES]
    w = max(x.shape[1] for x in rows)
    rows = [np.pad(x, ((0, 4), (0, w - x.shape[1]), (0, 0)), constant_values=24) for x in rows]
    sheet = np.concatenate(rows, axis=0)
    out = (paths.scratch_root("w2", "ashnumb") if "--scratch" in argv else paths.preview_root() / OUT)
    out.mkdir(parents=True, exist_ok=True)
    for q, sz in pngio.write_png_native(out / "band_stacks.png", sheet):
        print(f"  {q} {sz // 1024} KB")
    return 0


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(args))
