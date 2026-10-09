"""Review renders (spec §0.3, ARCHITECTURE §5: every asset is previewed before it ships).

All renders use Cycles CPU with the emission-only toon preview (``kit.shading``), view transform *Standard*,
so 4–8 samples are converged (samples only anti-alias edges). Outputs go to ``Art/Previews/<asset>/``,
each PNG ≤ 200 KB (``pngio.write_png_budget``):

* ``game.png``       — the W1 game camera (yaw 45°, pitch −50°, FOV 35°) at the default distance, 640×360:
                       the asset at true in-game size, plus ``game_1080crop.png`` (same camera at 1920×1080,
                       cropped around the asset = native 1080p pixels).
* ``closeup.png``    — front 3/4 (the web ``se`` view) at a lower pitch, outline at constant screen width.
* ``turnaround.png`` — 8 facings (front, front-right … back …) under the game camera direction.
* ``anim_<Clip>.png`` / ``anims.png`` — contact sheets: N frames per clip, contact/release frames marked.

Facing names (character yaw relative to the camera): ``front`` faces the camera, ``se`` = front 3/4 toward
screen right (the web's main view), ``side``, ``ne`` = back 3/4, ``back``.
"""
from __future__ import annotations

import math
import os
import tempfile
from pathlib import Path
from typing import Sequence

import bpy
import numpy as np
from mathutils import Matrix, Vector

from . import anim as kanim
from . import color, outline, paths, pngio, scene, shading

STUDIO_BG = "#3B3546"
GAME_RES = (640, 360)
TMP_DIR = Path(tempfile.gettempdir()) / "af_review"

# yaw (deg, about Z) applied to an asset that faces −Y so that it faces …
FACINGS = {
    "front": -135.0,   # … the camera
    "se": -90.0,       # … screen lower-right (front 3/4, web main view)
    "side": -45.0,     # … screen right (profile)
    "ne": 0.0,         # … screen upper-right (back 3/4)
    "back": 45.0,      # … away from the camera
    "nw": 90.0,
    "sw": 180.0,       # … screen lower-left (front 3/4 mirrored)
    "side_l": 135.0,
}
TURN_ORDER = ["front", "se", "side", "ne", "back", "nw", "side_l", "sw"]


# ── render setup ────────────────────────────────────────────────────────────────────────────────────────
def setup_render(res: tuple[int, int], samples: int = 8, bg_hex: str = STUDIO_BG) -> None:
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    cy = sc.cycles
    cy.device = "CPU"
    cy.samples = samples
    cy.use_adaptive_sampling = False
    cy.use_denoising = False
    cy.max_bounces = 16            # transparent hull hits count as bounces (diffuse/glossy stay 0)
    cy.diffuse_bounces = 0
    cy.glossy_bounces = 0
    cy.transmission_bounces = 0
    cy.volume_bounces = 0
    cy.transparent_max_bounces = 16
    cy.caustics_reflective = False
    cy.caustics_refractive = False
    cy.pixel_filter_type = "BLACKMAN_HARRIS"
    cy.filter_width = 1.2
    cy.seed = 0
    sc.render.threads_mode = "FIXED"
    sc.render.threads = max(1, min(4, os.cpu_count() or 4))
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = False
    sc.render.use_border = False
    sc.render.use_crop_to_border = False
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    sc.view_settings.exposure = 0.0
    sc.view_settings.gamma = 1.0
    sc.display_settings.display_device = "sRGB"
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGB"
    sc.render.image_settings.color_depth = "8"
    sc.render.image_settings.compression = 0
    w = sc.world or bpy.data.worlds.new("AF_World")
    sc.world = w
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    bg = nt.nodes.new("ShaderNodeBackground")
    bg.inputs["Color"].default_value = color.hex_linear4(bg_hex)
    bg.inputs["Strength"].default_value = 1.0
    out = nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(bg.outputs[0], out.inputs["Surface"])


def _camera(name: str = "AF_ReviewCam") -> bpy.types.Object:
    cam = bpy.data.objects.get(name)
    if cam is None:
        cam = bpy.data.objects.new(name, bpy.data.cameras.new(name))
        scene.link(cam)
    bpy.context.scene.camera = cam
    cam.data.sensor_fit = "HORIZONTAL"
    cam.data.clip_start = 0.05
    cam.data.clip_end = 500.0
    return cam


def cam_dir(yaw_ue: float = shading.CAM_YAW_UE, pitch_ue: float = shading.CAM_PITCH_UE) -> Vector:
    """Camera forward vector in Blender axes for a UE yaw/pitch."""
    f, _, _ = shading.camera_basis_ue(yaw_ue, pitch_ue)
    return shading.ue_dir_to_blender(f)


def place_camera(focus: Sequence[float], distance: float, fov_h: float = shading.CAM_FOV_H,
                 yaw_ue: float = shading.CAM_YAW_UE, pitch_ue: float = shading.CAM_PITCH_UE) -> bpy.types.Object:
    cam = _camera()
    cam.data.type = "PERSP"
    cam.data.angle = math.radians(fov_h)
    f = cam_dir(yaw_ue, pitch_ue)
    eye = Vector(focus) - f * distance
    cam.matrix_world = scene.look_at_matrix(eye, Vector(focus))
    return cam


def game_camera(focus_xy: Sequence[float] = (0, 0), distance: float = shading.CAM_DIST_DEFAULT):
    """The W1 game camera looking at a focus 50 cm above the ground."""
    return place_camera((focus_xy[0], focus_xy[1], shading.CAM_FOCUS_Z), distance)


def frame_bounds(lo: Vector, hi: Vector, res: tuple[int, int], fov_h: float = 30.0,
                 yaw_ue: float = shading.CAM_YAW_UE, pitch_ue: float = -25.0, fill: float = 0.86,
                 offset_v: float = 0.0) -> bpy.types.Object:
    """Perspective camera at a UE yaw/pitch, distance chosen so the box fills ``fill`` of the frame."""
    cam = _camera()
    cam.data.type = "PERSP"
    cam.data.angle = math.radians(fov_h)
    f = cam_dir(yaw_ue, pitch_ue)
    center = (lo + hi) * 0.5
    corners = [Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
    up = Vector((0, 0, 1))
    r = f.cross(up).normalized()
    u = r.cross(f).normalized()
    tan_h = math.tan(math.radians(fov_h) / 2)
    tan_v = tan_h * res[1] / res[0]
    dist = 0.5
    for c in corners:
        d = c - center
        x, y, z = d.dot(r), d.dot(u), d.dot(f)
        dist = max(dist, abs(x) / (tan_h * fill) - z, abs(y) / (tan_v * fill) - z)
    center = center + u * offset_v
    cam.matrix_world = scene.look_at_matrix(center - f * dist, center)
    return cam


def mesh_points(objs: Sequence[bpy.types.Object], step: int = 1) -> np.ndarray:
    """World-space posed vertex positions of mesh objects (every ``step``-th vertex)."""
    dg = bpy.context.evaluated_depsgraph_get()
    out = []
    for o in objs:
        if o.type != "MESH" or o.hide_render:
            continue
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        n = len(me.vertices)
        co = np.zeros(n * 3, np.float32)
        me.vertices.foreach_get("co", co)
        ev.to_mesh_clear()
        co = co.reshape(n, 3)[::step]
        mw = np.array(ev.matrix_world)
        out.append(co @ mw[:3, :3].T + mw[:3, 3])
    return np.concatenate(out) if out else np.zeros((1, 3), np.float32)


def frame_points(pts: np.ndarray, res: tuple[int, int], fov_h: float = 28.0,
                 yaw_ue: float = shading.CAM_YAW_UE, pitch_ue: float = -25.0, fill: float = 0.9,
                 include_ground: bool = True) -> bpy.types.Object:
    """Perspective camera at a UE yaw/pitch, placed so every point fits in ``fill`` of the frame (centred)."""
    cam = _camera()
    cam.data.type = "PERSP"
    cam.data.angle = math.radians(fov_h)
    f = np.array(cam_dir(yaw_ue, pitch_ue))
    r = np.cross(f, (0, 0, 1.0))
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    pts = np.asarray(pts, np.float64)
    if include_ground:   # keep the contact shadow in frame
        pts = np.vstack([pts, [[pts[:, 0].mean(), pts[:, 1].mean(), 0.0]]])
    tan_h = math.tan(math.radians(fov_h) / 2)
    tan_v = tan_h * res[1] / res[0]
    c = (pts.min(0) + pts.max(0)) / 2
    for _ in range(3):
        rel = pts - c
        x, y, z = rel @ r, rel @ u, rel @ f
        dist = float(max(0.3, np.max(np.abs(x) / (tan_h * fill) - z), np.max(np.abs(y) / (tan_v * fill) - z)))
        # recentre on the perspective-projected extents
        px, py = x / (dist + z) / tan_h, y / (dist + z) / tan_v
        dx, dy = (px.min() + px.max()) / 2, (py.min() + py.max()) / 2
        c = c + r * dx * dist * tan_h + u * dy * dist * tan_v
    eye = Vector(c - f * dist)
    cam.matrix_world = scene.look_at_matrix(eye, Vector(c))
    return cam


def stage(radius: float = 30.0, blob_radius: float = 0.39, ground_hex: str = "#74A247",
          grid: bool = True) -> bpy.types.Object:
    """Plains review ground with the web contact shadow under the origin."""
    ob = bpy.data.objects.get("AF_Stage")
    if ob is None:
        import bmesh
        me = bpy.data.meshes.new("AF_Stage")
        bm = bmesh.new()
        bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=radius)
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new("AF_Stage", me)
        scene.link(ob)
    mat = shading.stage_ground_material(blob_radius=blob_radius, ground_hex=ground_hex, grid=grid)
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    return ob


def hide_stage(hidden: bool) -> None:
    ob = bpy.data.objects.get("AF_Stage")
    if ob is not None:
        ob.hide_render = hidden


# ── core render ─────────────────────────────────────────────────────────────────────────────────────────
def set_outline_px(meshes: Sequence[bpy.types.Object], px: float | None, at: Vector) -> None:
    """Make the hull ``px`` pixels wide at point ``at`` for the active camera (None = baked world width)."""
    sc = bpy.context.scene
    for m in meshes:
        if m.type != "MESH" or "af_outline_width" not in m:
            continue
        if px is None:
            outline.set_preview_width(m, float(m["af_outline_width"]))
        else:
            wpp = outline.pixel_world_size(sc.camera, at, sc.render.resolution_x)
            outline.set_preview_width(m, px * wpp)


def render_array(border: tuple[float, float, float, float] | None = None, exr: bool = False) -> np.ndarray:
    """Render the active camera → HxWx3 uint8 (top row first); ``exr`` → HxWx3 float32 scene-linear values
    (data passes: coverage, emission)."""
    sc = bpy.context.scene
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    st = sc.render.image_settings
    if exr:
        st.file_format, st.color_mode, st.color_depth = "OPEN_EXR", "RGB", "32"
        tmp = TMP_DIR / f"r_{os.getpid()}.exr"
    else:
        tmp = TMP_DIR / f"r_{os.getpid()}.png"
    if border is not None:
        sc.render.use_border = True
        sc.render.use_crop_to_border = True
        sc.render.border_min_x, sc.render.border_min_y, sc.render.border_max_x, sc.render.border_max_y = border
    sc.render.filepath = str(tmp)
    bpy.ops.render.render(write_still=True)
    sc.render.use_border = False
    sc.render.use_crop_to_border = False
    if exr:
        img = bpy.data.images.load(str(tmp), check_existing=False)
        w, h = img.size
        arr = np.array(img.pixels[:], np.float32).reshape(h, w, 4)[::-1, :, :3].copy()
        bpy.data.images.remove(img)
        st.file_format, st.color_mode, st.color_depth = "PNG", "RGB", "8"
    else:
        arr = pngio.read_png(tmp)[:, :, :3]
    tmp.unlink(missing_ok=True)
    return arr


def border_origin(border: tuple[float, float, float, float] | None, res: tuple[int, int]) -> tuple[int, int]:
    """Top-left pixel (x, y down) of a cropped border render inside the full ``res`` frame."""
    if border is None:
        return 0, 0
    return int(border[0] * res[0]), res[1] - int(border[3] * res[1])


class _World:
    """Temporarily set the world background colour (data passes render over black)."""

    def __init__(self, hex_or_rgb):
        self.c = hex_or_rgb

    def __enter__(self):
        bg = bpy.context.scene.world.node_tree.nodes.get("Background")
        self.saved = tuple(bg.inputs["Color"].default_value)
        c = self.c if not isinstance(self.c, str) else color.hex_linear(self.c)
        bg.inputs["Color"].default_value = (c[0], c[1], c[2], 1.0)
        return self

    def __exit__(self, *exc):
        bpy.context.scene.world.node_tree.nodes.get("Background").inputs["Color"].default_value = self.saved


def project(cam: bpy.types.Object, p: Vector) -> tuple[float, float]:
    """World point → normalised frame coords (0..1, y up) for the scene's render aspect."""
    from bpy_extras.object_utils import world_to_camera_view
    co = world_to_camera_view(bpy.context.scene, cam, Vector(p))
    return co.x, co.y


# ── offline ink (portraits / icons): screen-space line art instead of the hull ─────────────────────────
def _flat_material(name: str, kind: str) -> bpy.types.Material:
    """``kind`` 'transparent' (hides a slot) or 'depth' (emission = camera Z depth, for the ink pass)."""
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    if kind == "transparent":
        nt.links.new(nt.nodes.new("ShaderNodeBsdfTransparent").outputs[0], out.inputs["Surface"])
    else:
        cam = nt.nodes.new("ShaderNodeCameraData")
        em = nt.nodes.new("ShaderNodeEmission")
        nt.links.new(cam.outputs["View Z Depth"], em.inputs["Color"])
        em.inputs["Strength"].default_value = 1.0
        nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m


class _SlotSwap:
    """Temporarily replace material slots of meshes (``{slot: material}``), restored on exit."""

    def __init__(self, meshes, slots: dict):
        self.meshes, self.slots, self.saved = [m for m in meshes if m.type == "MESH"], slots, []

    def __enter__(self):
        for m in self.meshes:
            mats = m.data.materials
            self.saved.append([mats[i] for i in range(len(mats))])
            for i, mat in self.slots.items():
                if i < len(mats):
                    mats[i] = mat
        return self

    def __exit__(self, *exc):
        for m, old in zip(self.meshes, self.saved):
            for i, mat in enumerate(old):
                m.data.materials[i] = mat


def _render_rgba(res: tuple[int, int], samples: int, exr: bool = False, sharp: bool = False) -> np.ndarray:
    """Render the active camera with a transparent film → HxWx4 (top row first): uint8 PNG or float32 EXR."""
    sc = bpy.context.scene
    setup_render(res, samples)
    sc.render.film_transparent = True
    st = sc.render.image_settings
    if sharp:                      # one centred sample per pixel: unfiltered depth / coverage
        sc.cycles.pixel_filter_type = "BOX"
        sc.cycles.filter_width = 0.01
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    if exr:
        st.file_format, st.color_mode, st.color_depth = "OPEN_EXR", "RGBA", "32"
        tmp = TMP_DIR / f"ink_{os.getpid()}.exr"
    else:
        st.file_format, st.color_mode, st.color_depth = "PNG", "RGBA", "8"
        tmp = TMP_DIR / f"ink_{os.getpid()}.png"
    sc.render.filepath = str(tmp)
    bpy.ops.render.render(write_still=True)
    if exr:
        img = bpy.data.images.load(str(tmp), check_existing=False)
        w, h = img.size
        arr = np.array(img.pixels[:], np.float32).reshape(h, w, 4)[::-1].copy()
        bpy.data.images.remove(img)
    else:
        arr = pngio.read_png(tmp)
    tmp.unlink(missing_ok=True)
    sc.render.film_transparent = False
    st.file_format, st.color_mode, st.color_depth = "PNG", "RGB", "8"
    return arr


def _disk(r: float) -> list[tuple[int, int]]:
    ri = int(math.ceil(r))
    return [(dy, dx) for dy in range(-ri, ri + 1) for dx in range(-ri, ri + 1)
            if (dx or dy) and dx * dx + dy * dy <= r * r + 1e-6]


def _shift(a: np.ndarray, dy: int, dx: int, fill) -> np.ndarray:
    """out[y, x] = a[y + dy, x + dx] (``fill`` outside)."""
    out = np.full_like(a, fill)
    h, w = a.shape[:2]
    ys, yd = (slice(dy, h), slice(0, h - dy)) if dy >= 0 else (slice(0, h + dy), slice(-dy, h))
    xs, xd = (slice(dx, w), slice(0, w - dx)) if dx >= 0 else (slice(0, w + dx), slice(-dx, w))
    out[yd, xd] = a[ys, xs]
    return out


def ink_render(meshes: Sequence[bpy.types.Object], res: tuple[int, int], silhouette_px: float = 4.5,
               interior_px: float = 2.0, depth_step: float = 0.03, ink_mix: float = 0.4, samples: int = 16,
               ss: int = 2) -> np.ndarray:
    """Offline toon render with **screen-space line art** (portraits, icons): the baked hulls are hidden and
    the web's ink is drawn instead — a solid ``#120C18`` silhouette of constant ``silhouette_px`` (the web
    stamps the sprite in 8 directions) and ``interior_px`` contour lines wherever a nearer surface overlaps a
    farther one by more than ``depth_step`` metres, coloured ``mix(ink, line(c_near), ink_mix)`` like the hull.
    No hull artefacts (saw-teeth along overlapping plates, slivers at grazing angles), width exact at any size.
    Lines are found on a ``ss``× supersampled depth pass and box-filtered down (anti-aliased).
    Returns HxWx4 uint8, straight alpha, transparent background."""
    hide = _flat_material("AF_Ink_Hide", "transparent")
    with _SlotSwap(meshes, {1: hide}):
        col = _render_rgba(res, samples).astype(np.float32) / 255.0
        with _SlotSwap(meshes, {0: _flat_material("AF_Ink_Depth", "depth")}):
            dz = _render_rgba((res[0] * ss, res[1] * ss), 1, exr=True, sharp=True)
    cov = dz[:, :, 3] > 0.5
    depth = np.where(cov, dz[:, :, 0], np.inf).astype(np.float32)
    H, W = depth.shape
    # nearer-neighbour colour lookup in the supersampled grid (nearest upsample of the colour pass)
    col_up = col.repeat(ss, 0).repeat(ss, 1)[:H, :W]
    sil = cov.copy()
    for dy, dx in _disk(silhouette_px * ss):
        sil |= _shift(cov, dy, dx, False)
    line = np.zeros((H, W), bool)
    best = np.full((H, W), np.inf, np.float32)
    line_col = np.zeros((H, W, 3), np.float32)
    thr = np.where(cov, np.maximum(depth_step, depth * 0.004), 0.0).astype(np.float32)
    lim = np.where(cov, depth - thr, -np.inf).astype(np.float32)
    for dy, dx in _disk(interior_px * ss):
        dn = _shift(depth, dy, dx, np.inf)
        nearer = (dn < lim) & (dn < best)
        if nearer.any():
            cn = _shift(col_up[:, :, :3], dy, dx, 0.0)
            line_col[nearer] = cn[nearer]
            best[nearer] = dn[nearer]
            line |= nearer

    def down(a: np.ndarray) -> np.ndarray:
        h, w = a.shape[0] // ss, a.shape[1] // ss
        return a[:h * ss, :w * ss].reshape(h, ss, w, ss, *a.shape[2:]).mean(axis=(1, 3))
    ink = np.array(color.hex_to_rgb(shading.INK_HEX), np.float32) / 255.0
    lc = down(line_col * line[:, :, None]) / np.maximum(down(line.astype(np.float32))[:, :, None], 1e-6)
    lc = lc * 255.0
    tone = (lc + np.array((20, 10, 30), np.float32)) * 0.5 * 0.45 / 255.0       # web line(c)
    lrgb = ink + (tone - ink) * ink_mix
    la = down(line.astype(np.float32))[:, :, None]
    sa = down(sil.astype(np.float32))[:, :, None]
    rgb, a = col[:, :, :3], col[:, :, 3:4]
    rgb = rgb + (lrgb - rgb) * la * (a > 0)
    out_a = a + sa * (1 - a)
    out_rgb = (rgb * a + ink * sa * (1 - a)) / np.maximum(out_a, 1e-6)
    return (np.dstack([out_rgb, out_a]).clip(0, 1) * 255.0 + 0.5).astype(np.uint8)


def composite_glow(img: np.ndarray, center_px: tuple[float, float], radius_px: float, hex_color: str,
                   alpha: float, core: float = 0.0) -> None:
    """Soft radial glow (screen blend, quadratic falloff) — a runtime FX sprite (e.g. ``visorGlow``) baked
    into an offline render that gets no bloom. ``img`` RGB or RGBA uint8 (alpha grows to cover the glow);
    ``core`` adds a tighter hot centre (fraction of the radius)."""
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot(xx + 0.5 - center_px[0], yy + 0.5 - center_px[1]) / max(radius_px, 1e-3)
    g = alpha * np.clip(1.0 - r, 0.0, 1.0) ** 2
    if core > 0:
        g = g + (1.0 - alpha) * np.clip(1.0 - r / core, 0.0, 1.0) ** 2
    g = np.clip(g, 0, 1)[:, :, None]
    c = np.array(color.hex_to_rgb(hex_color), np.float32) / 255.0
    rgb = img[:, :, :3].astype(np.float32) / 255.0
    if img.shape[2] == 4:
        a = img[:, :, 3:4].astype(np.float32) / 255.0
        lit = 1 - (1 - rgb) * (1 - c * g)                  # screen over the opaque part
        out_a = a + g * (1 - a)
        rgb = (lit * a + c * g * (1 - a)) / np.maximum(out_a, 1e-6)
        img[:, :, 3] = (out_a[:, :, 0] * 255 + 0.5).astype(np.uint8)
    else:
        rgb = 1 - (1 - rgb) * (1 - c * g)
    img[:, :, :3] = (rgb.clip(0, 1) * 255 + 0.5).astype(np.uint8)


# ── game-look post (bloom approximation + runtime glow cards) and the ink gate ──────────────────────────
# UE desktop bloom on the emissive palette regions (spec §1.7: palette P.b; world-map-nav §14.1 "mild bloom",
# web bloom strength 0.8): two Gaussian lobes on the emissive-only pass, added in linear light. σ in 1080p pixels.
BLOOM_LOBES = ((3.0, 0.55), (10.0, 0.45))
BLOOM_STRENGTH = 0.8
# web point-light / glow sprite falloff stops (world-map-nav §14.1): 1, .9@.15, .6@.4, .25@.7, 0@1
GLOW_STOPS = ((0.0, 1.0), (0.15, 0.9), (0.4, 0.6), (0.7, 0.25), (1.0, 0.0))


def _srgb_to_lin(a: np.ndarray) -> np.ndarray:
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def _lin_to_srgb(a: np.ndarray) -> np.ndarray:
    a = np.clip(a, 0.0, 1.0)
    return np.where(a <= 0.0031308, a * 12.92, 1.055 * a ** (1 / 2.4) - 0.055)


def _gauss_matrix(n: int, sigma: float) -> np.ndarray:
    x = np.arange(n, dtype=np.float32)
    k = np.exp(-0.5 * ((x[:, None] - x[None, :]) / max(sigma, 1e-3)) ** 2)
    return (k / np.sqrt(2 * np.pi) / max(sigma, 1e-3)).astype(np.float32)


def blur(a: np.ndarray, sigma: float) -> np.ndarray:
    """Separable Gaussian blur of an HxW(xC) float image (zero outside the frame)."""
    gy, gx = _gauss_matrix(a.shape[0], sigma), _gauss_matrix(a.shape[1], sigma)
    if a.ndim == 2:
        return gy @ a @ gx.T
    return np.stack([gy @ a[:, :, c] @ gx.T for c in range(a.shape[2])], axis=2)


def _flat_emission(name: str, rgb, hull: bool = False) -> bpy.types.Material:
    """Constant linear emission; ``hull`` = the outline slot's rule (the flipped near side is transparent)."""
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    em.inputs["Strength"].default_value = 1.0
    if hull:
        tr = nt.nodes.new("ShaderNodeBsdfTransparent")
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        mx = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(geo.outputs["Backfacing"], mx.inputs["Fac"])
        nt.links.new(em.outputs[0], mx.inputs[1])
        nt.links.new(tr.outputs[0], mx.inputs[2])
        nt.links.new(mx.outputs[0], out.inputs["Surface"])
    else:
        nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m


def _visible(meshes) -> list:
    return [m for m in meshes if m.type == "MESH" and not m.hide_render]


def coverage_pass(meshes, border=None, samples: int = 8) -> tuple[np.ndarray, np.ndarray]:
    """(body, hull) coverage fractions per pixel for the active camera: toon slots emit red, the visible (far)
    side of the outline hull green, over black, stage hidden — exact anti-aliased coverages."""
    stage_ob = bpy.data.objects.get("AF_Stage")
    was = stage_ob.hide_render if stage_ob else True
    hide_stage(True)
    sc = bpy.context.scene
    keep = sc.cycles.samples
    sc.cycles.samples = samples
    body, hull = _flat_emission("AF_Cov_Body", (1, 0, 0)), _flat_emission("AF_Cov_Hull", (0, 1, 0), hull=True)
    with _SlotSwap(_visible(meshes), {0: body, 1: hull}), _World((0.0, 0.0, 0.0)):
        arr = render_array(border, exr=True)
    sc.cycles.samples = keep
    hide_stage(was)
    return np.clip(arr[:, :, 0], 0, 1), np.clip(arr[:, :, 1], 0, 1)


def emissive_pass(meshes, border=None, samples: int = 4) -> np.ndarray:
    """Linear emission of the emissive palette regions only (bloom source), over black, stage hidden."""
    stage_ob = bpy.data.objects.get("AF_Stage")
    was = stage_ob.hide_render if stage_ob else True
    hide_stage(True)
    sc = bpy.context.scene
    keep = sc.cycles.samples
    sc.cycles.samples = samples
    vis = _visible(meshes)
    black = _flat_emission("AF_Hull_Black", (0, 0, 0), hull=True)
    saved = []
    for m in vis:
        mats = m.data.materials
        saved.append([mats[i] for i in range(len(mats))])
        if len(mats) and mats[0] is not None:
            mats[0] = shading.emissive_pass_material(saved[-1][0])
        if len(mats) > 1:
            mats[1] = black
    try:
        with _World((0.0, 0.0, 0.0)):
            arr = render_array(border, exr=True)
    finally:
        for m, old in zip(vis, saved):
            for i, mat in enumerate(old):
                m.data.materials[i] = mat
        sc.cycles.samples = keep
        hide_stage(was)
    return np.maximum(arr, 0.0)


def bloom(img: np.ndarray, emis: np.ndarray, res_y: int = 1080, strength: float = BLOOM_STRENGTH) -> np.ndarray:
    """UE-style bloom approximation: blurred emissive pass (``BLOOM_LOBES``, σ scaled from 1080p) added to the
    image in linear light. ``img`` uint8 RGB → uint8 RGB."""
    lin = _srgb_to_lin(img.astype(np.float32) / 255.0)
    k = res_y / 1080.0
    add = sum(w * blur(emis, s * k) for s, w in BLOOM_LOBES)
    return (_lin_to_srgb(lin + strength * add) * 255.0 + 0.5).astype(np.uint8)


def glow_card(img: np.ndarray, center_px, radius_px: float, hex_color: str, alpha: float,
              core_hex: str | None = None, core_frac: float = 0.35, core_alpha: float = 0.0) -> np.ndarray:
    """A runtime glow sprite (UE: additive camera-facing card at a socket) with the web light falloff
    ``GLOW_STOPS``, optional hot core; added in linear light. ``img`` uint8 RGB → uint8 RGB."""
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot(xx + 0.5 - center_px[0], yy + 0.5 - center_px[1]) / max(radius_px, 1e-3)
    xs, ys = zip(*GLOW_STOPS)
    f = np.interp(r, xs, ys).astype(np.float32)[:, :, None]
    lin = _srgb_to_lin(img.astype(np.float32) / 255.0)
    lin = lin + alpha * f * np.array(color.hex_linear(hex_color), np.float32)
    if core_hex and core_alpha > 0:
        fc = np.interp(r / core_frac, xs, ys).astype(np.float32)[:, :, None]
        lin = lin + core_alpha * fc * np.array(color.hex_linear(core_hex), np.float32)
    return (_lin_to_srgb(lin) * 255.0 + 0.5).astype(np.uint8)


def _bilinear(a: np.ndarray, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    h, w = a.shape
    x = np.clip(x, 0, w - 1.001)
    y = np.clip(y, 0, h - 1.001)
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - x0, y - y0
    return (a[y0, x0] * (1 - fx) * (1 - fy) + a[y0, x0 + 1] * fx * (1 - fy) + a[y0 + 1, x0] * (1 - fx) * fy
            + a[y0 + 1, x0 + 1] * fx * fy)


INK_GATE = {"minPx": 3.0, "minFrac": 0.90, "darkLum": 40.0, "darkWithinPx": 3, "maxHullLumP95": 40.0}


def ink_stats(body: np.ndarray, hull: np.ndarray, rgb: np.ndarray | None = None, min_px: float = INK_GATE["minPx"],
              dark_lum: float = INK_GATE["darkLum"], within: int = INK_GATE["darkWithinPx"]) -> dict:
    """Measure the ink rim on a render: for every silhouette edge pixel (coverage ≥ .5 next to background),
    walk inward along the coverage gradient and integrate the hull coverage until the body is reached = the
    visible ink width in pixels (sub-pixel exact, AA-independent). Also the review's colour check on ``rgb``:
    a pixel darker than ``dark_lum`` within ``within`` px of the edge, and the luminance of solid-ink pixels.
    Returns stats + ``edge_ok`` / ``edge_yx`` arrays for the diagnostic map."""
    A = np.clip(body + hull, 0, 1)
    inside = A >= 0.5
    pad = np.pad(inside, 1, constant_values=False)
    nb_out = ~pad[:-2, 1:-1] | ~pad[2:, 1:-1] | ~pad[1:-1, :-2] | ~pad[1:-1, 2:]
    edge = inside & nb_out
    ys, xs = np.nonzero(edge)
    if len(ys) == 0:
        return {"edges": 0, "fracRim": 0.0, "fracDark": 0.0, "rimPxMedian": 0.0, "rimPxP10": 0.0,
                "hullLumP95": 0.0, "edge_yx": (ys, xs), "edge_ok": np.zeros(0, bool)}
    sm = blur(A, 1.0)
    gy, gx = np.gradient(sm)
    nx, ny = gx[ys, xs], gy[ys, xs]
    nn = np.maximum(np.hypot(nx, ny), 1e-6)
    nx, ny = nx / nn, ny / nn
    ds = 0.25
    s = np.arange(-2.0, 9.0, ds, dtype=np.float32)
    px = xs[:, None] + 0.0 + nx[:, None] * s[None]
    py = ys[:, None] + 0.0 + ny[:, None] * s[None]
    Hs, Bs = _bilinear(hull, px, py), _bilinear(body, px, py)
    reached = np.cumsum(Bs >= 0.98, axis=1) > 0        # through the body's AA ramp (H = 1 − B there)
    width = (Hs * ~reached).sum(1) * ds
    ok = width >= min_px
    out = {"edges": int(len(ys)), "fracRim": round(float(ok.mean()), 4),
           "rimPxMedian": round(float(np.median(width)), 2), "rimPxP10": round(float(np.percentile(width, 10)), 2),
           "edge_yx": (ys, xs), "edge_ok": ok, "minPx": min_px}
    if rgb is not None:
        lum = rgb[:, :, :3].astype(np.float32) @ np.array([0.299, 0.587, 0.114], np.float32)
        lp = np.pad(lum, within, constant_values=255.0)
        win = np.lib.stride_tricks.sliding_window_view(lp, (2 * within + 1, 2 * within + 1))
        dark = win.min(axis=(2, 3)) < dark_lum
        out["fracDark"] = round(float(dark[ys, xs].mean()), 4)
        solid = (hull > 0.98) & (body < 0.02)
        out["hullLumP95"] = round(float(np.percentile(lum[solid], 95)), 1) if solid.any() else 0.0
    return out


def ink_gate_ok(st: dict) -> bool:
    return (st.get("edges", 0) > 0 and st["fracRim"] >= INK_GATE["minFrac"]
            and st.get("fracDark", 1.0) >= INK_GATE["minFrac"] and st.get("hullLumP95", 0.0) <= INK_GATE["maxHullLumP95"])


def ink_map(rgb: np.ndarray, st: dict, zoom: int = 3) -> np.ndarray:
    """Diagnostic: the render zoomed ``zoom``× (nearest) with each silhouette edge pixel marked green (ink ≥
    the gate width) or red (thinner)."""
    img = rgb.copy()
    ys, xs = st["edge_yx"]
    ok = st["edge_ok"]
    img[ys[ok], xs[ok]] = (60, 230, 90)
    img[ys[~ok], xs[~ok]] = (255, 40, 40)
    return img.repeat(zoom, 0).repeat(zoom, 1)


def project_px(cam: bpy.types.Object, p: Vector, res: tuple[int, int]) -> tuple[float, float]:
    """World point → pixel coordinates (x right, y down) in a ``res`` render of ``cam``."""
    x, y = project(cam, p)
    return x * res[0], (1.0 - y) * res[1]


# ── labels (tiny 5×7 bitmap font) ──────────────────────────────────────────────────────────────────────
_FONT = {
    "A": "01110 10001 10001 11111 10001 10001 10001", "B": "11110 10001 11110 10001 10001 10001 11110",
    "C": "01111 10000 10000 10000 10000 10000 01111", "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 11110 10000 10000 10000 11111", "F": "11111 10000 11110 10000 10000 10000 10000",
    "G": "01111 10000 10000 10011 10001 10001 01111", "H": "10001 10001 11111 10001 10001 10001 10001",
    "I": "11111 00100 00100 00100 00100 00100 11111", "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 11100 10010 10001 10001 10001", "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10001 10001 10001 10001", "N": "10001 11001 10101 10011 10001 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110", "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101", "R": "11110 10001 10001 11110 10010 10001 10001",
    "S": "01111 10000 01110 00001 00001 00001 11110", "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110", "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 11011 10001", "X": "10001 01010 00100 00100 01010 10001 10001",
    "Y": "10001 01010 00100 00100 00100 00100 00100", "Z": "11111 00010 00100 01000 10000 10000 11111",
    "0": "01110 10011 10101 10101 11001 10001 01110", "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00110 01000 10000 11111", "3": "11110 00001 00110 00001 00001 00001 11110",
    "4": "00010 00110 01010 10010 11111 00010 00010", "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "01110 10000 11110 10001 10001 10001 01110", "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 01110 10001 10001 10001 01110", "9": "01110 10001 10001 01111 00001 00001 01110",
    " ": "00000 00000 00000 00000 00000 00000 00000", ".": "00000 00000 00000 00000 00000 01100 01100",
    "-": "00000 00000 00000 11111 00000 00000 00000", "_": "00000 00000 00000 00000 00000 00000 11111",
    ":": "00000 01100 01100 00000 01100 01100 00000", "/": "00001 00010 00010 00100 01000 01000 10000",
    "(": "00010 00100 01000 01000 01000 00100 00010", ")": "01000 00100 00010 00010 00010 00100 01000",
    "+": "00000 00100 00100 11111 00100 00100 00000", "#": "01010 11111 01010 01010 11111 01010 00000",
    "=": "00000 00000 11111 00000 11111 00000 00000", "%": "11001 11010 00010 00100 01011 01011 10011",
}


def draw_text(img: np.ndarray, x: int, y: int, text: str, rgb=(240, 236, 220), scale: int = 1,
              shadow: bool = True) -> None:
    for ox, oy, col in (((1, 1, (0, 0, 0)),) if shadow else ()) + ((0, 0, rgb),):
        cx = x + ox
        for ch in text.upper():
            g = _FONT.get(ch, _FONT[" "])
            rows = g.split()
            for ry, row in enumerate(rows):
                for rx, bit in enumerate(row):
                    if bit == "1":
                        y0, x0 = y + oy + ry * scale, cx + rx * scale
                        img[max(0, y0):max(0, y0 + scale), max(0, x0):max(0, x0 + scale)] = col
            cx += 6 * scale


def border(img: np.ndarray, rgb=(255, 138, 42), w: int = 2) -> None:
    img[:w, :] = rgb
    img[-w:, :] = rgb
    img[:, :w] = rgb
    img[:, -w:] = rgb


def grid(cells: list[np.ndarray], cols: int, pad: int = 4, bg=(24, 20, 30)) -> np.ndarray:
    h = max(c.shape[0] for c in cells)
    w = max(c.shape[1] for c in cells)
    rows = (len(cells) + cols - 1) // cols
    out = np.zeros((rows * (h + pad) + pad, cols * (w + pad) + pad, 3), np.uint8)
    out[:, :] = bg
    for i, c in enumerate(cells):
        r, k = divmod(i, cols)
        y, x = pad + r * (h + pad), pad + k * (w + pad)
        out[y:y + c.shape[0], x:x + c.shape[1]] = c
    return out


# ── high-level review of one asset ──────────────────────────────────────────────────────────────────────
def ink_world(px_1080: float) -> float:
    """World width (m) of a screen-constant ink of ``px_1080`` pixels at 1080p seen from the default W1 camera
    distance — what the UE outline (``OutlinePx × PixelWorldSize``) draws at default zoom. Close-ups and sheets use
    it so a magnified preview shows the shipped ink weight, magnified."""
    wpp = 2.0 * shading.CAM_DIST_DEFAULT * math.tan(math.radians(shading.CAM_FOV_H / 2.0)) / 1920.0
    return px_1080 * wpp


class Review:
    """Renders the standard preview set for a (skeletal or static) asset into ``Art/Previews/<asset>/``.

    ``root``: the object that is yawed for facings (armature for characters, the mesh for props);
    ``meshes``: the meshes with baked hulls; ``height`` (m) for framing; ``blob_radius`` (m) for the stage.
    """

    def __init__(self, asset: str, root: bpy.types.Object, meshes: Sequence[bpy.types.Object], height: float,
                 blob_radius: float = 0.39, outline_px: float = 2.5, out_dir: Path | None = None,
                 samples: int = 8, extra_objects: Sequence[bpy.types.Object] = (),
                 game_outline_px: float | None = None, glows=None, bloom: bool = True):
        """``game_outline_px``: render the game views with the screen-constant ink width UE draws (the
        manifest ``outlinePx1080``, scaled to the render height) instead of the bare baked hull; close-ups,
        turnarounds and sheets then use the same ink in world units (``ink_world``) — the shipped weight,
        magnified. ``glows(cam, res) → [dict(center_px, radius_px, color, alpha, core, core_alpha, core_frac)]``:
        runtime glow cards (full-frame pixels) composited into game views; ``bloom``: the emissive bloom pass."""
        self.asset = asset
        self.root = root
        self.meshes = list(meshes)
        self.extra = list(extra_objects)
        self.height = height
        self.blob_radius = blob_radius
        self.outline_px = outline_px
        self.game_outline_px = game_outline_px
        self.samples = samples
        self.glows = glows
        self.glow_scale = 1.0          # × every glow card's alpha (e.g. 0 once a death clip has put the ember out)
        self.bloom = bloom
        self.out = Path(out_dir) if out_dir else paths.preview_dir(asset)
        self.out.mkdir(parents=True, exist_ok=True)
        self.written: list[tuple[Path, int]] = []
        self.ink_reports: list[dict] = []
        stage(blob_radius=blob_radius)

    def _save(self, name: str, img: np.ndarray) -> Path:
        p = self.out / name
        size = pngio.write_png_budget(p, img)
        self.written.append((p, size))
        return p

    def facing(self, name_or_deg) -> None:
        deg = FACINGS[name_or_deg] if isinstance(name_or_deg, str) else float(name_or_deg)
        self.root.rotation_mode = "XYZ"
        self.root.rotation_euler = (0, 0, math.radians(deg))
        bpy.context.view_layer.update()

    def _bounds(self) -> tuple[Vector, Vector]:
        lo, hi = scene.world_bounds(self.meshes + self.extra)
        lo.z = min(lo.z, 0.0)
        return lo, hi

    # ── ink widths ──────────────────────────────────────────────────────────────────────────────────────
    def ink(self, scale: float = 1.0, px: float | None = None, at: Vector | None = None) -> None:
        """Hull width for a non-game render: the shipped ink in world units (``ink_world``) when
        ``game_outline_px`` is set, else ``px`` (default ``outline_px``) screen pixels at ``at``."""
        if self.game_outline_px:
            for m in self.meshes:
                if m.type == "MESH" and "af_outline_width" in m:
                    outline.set_preview_width(m, ink_world(self.game_outline_px) * scale)
        else:
            set_outline_px(self.meshes, (px or self.outline_px) * scale, at if at is not None else Vector((0, 0, 0.9)))

    def game_outline(self, res_y: int, at: Vector | None = None, baked: bool = False) -> None:
        """Hull width for a game-camera render: the UE screen-constant width when ``game_outline_px`` is set
        (scaled from 1080p to ``res_y``), else (or with ``baked``) the baked world width (mobile low: no WPO)."""
        if self.game_outline_px and not baked:
            set_outline_px(self.meshes, self.game_outline_px * res_y / 1080.0,
                           at if at is not None else Vector((0, 0, self.height * 0.5)))
        else:
            set_outline_px(self.meshes, None, Vector((0, 0, 0)))

    # ── game look ───────────────────────────────────────────────────────────────────────────────────────
    def post(self, img: np.ndarray, cam, res: tuple[int, int], border=None, glow: bool = True,
             bloom_on: bool | None = None) -> np.ndarray:
        """Bloom (emissive pass) + runtime glow cards over a game-camera render (cropped by ``border``)."""
        bloom_on = self.bloom if bloom_on is None else bloom_on
        if bloom_on:
            img = bloom(img, emissive_pass(self.meshes, border), res[1])
        if glow and self.glows is not None:
            x0, y0 = border_origin(border, res)
            for g in self.glows(cam, res):
                a = g.get("alpha", 0.0) * self.glow_scale
                if a <= 0.0:
                    continue
                c = (g["center_px"][0] - x0, g["center_px"][1] - y0)
                img = glow_card(img, c, g["radius_px"], g["color"], a, g.get("core"),
                                g.get("core_frac", 0.35), g.get("core_alpha", 0.0) * self.glow_scale)
        return img

    def game_shot(self, border=None, res: tuple[int, int] = (1920, 1080), measure: bool = False,
                  tag: str = "", glow: bool = True, bloom_on: bool | None = None):
        """One game-camera render (active camera, current hull width) → (final image, ink stats or None).
        ``measure``: coverage pass + ``ink_stats`` (measured before the glow/bloom post, colour check after)."""
        cam = bpy.context.scene.camera
        img = render_array(border)
        st = None
        if measure:
            body, hull = coverage_pass(self.meshes, border)
        img = self.post(img, cam, res, border, glow=glow, bloom_on=bloom_on)
        if measure:
            st = ink_stats(body, hull, img)
            st["tag"] = tag
            self.ink_reports.append(st)
        return img, st

    @staticmethod
    def ink_label(st: dict | None) -> str:
        if not st:
            return ""
        return f"RIM>={st['minPx']:g}PX {st['fracRim'] * 100:.0f}%  DARK {st.get('fracDark', 0) * 100:.0f}%"

    def game_crop_box(self, cam, pad: float = 0.03) -> tuple[float, float, float, float]:
        lo, hi = self._bounds()
        pts = [project(cam, Vector((x, y, z))) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
        hw = max(max(xs) - min(xs), (max(ys) - min(ys)) * 1080 / 1920) * 0.5 + pad
        hh = hw * 1920 / 1080
        return (max(0, cx - hw), max(0, cy - hh), min(1, cx + hw), min(1, cy + hh))

    # game camera at default distance
    def game_view(self, facing: str = "se") -> list[Path]:
        """``game.png`` (640×360, the W1 frame) and ``game_1080crop.png``: native 1080p pixels — the shipped look
        (UE ink width, bloom, glow cards) next to the baked hull alone (mobile low: no outline WPO, no bloom),
        each with its measured ink rim. ``game_inkgate.png``: the shipped crop zoomed 3× with every silhouette
        edge pixel marked green (ink ≥ 3 px) or red."""
        self.facing(facing)
        setup_render(GAME_RES, self.samples)
        hide_stage(False)
        cam = game_camera()
        self.game_outline(GAME_RES[1])
        img, _ = self.game_shot(None, GAME_RES)
        draw_text(img, 6, 6, f"{self.asset}  GAME W1 YAW45 PITCH-50 FOV35  {GAME_RES[0]}X{GAME_RES[1]}")
        paths_ = [self._save("game.png", img)]
        setup_render((1920, 1080), self.samples)
        box = self.game_crop_box(cam)
        self.game_outline(1080)
        ship, st = self.game_shot(box, measure=True, tag=f"game_1080crop {facing} shipped")
        cells = [ship.copy()]
        labels = [f"1080P 1:1 SHIPPED  INK {self.game_outline_px or 0:g}PX + BLOOM", self.ink_label(st)]
        if self.game_outline_px:
            self.game_outline(1080, baked=True)
            base, st_b = self.game_shot(box, measure=True, tag=f"game_1080crop {facing} baked", glow=False,
                                        bloom_on=False)
            st_b["gated"] = False
            w_cm = float(self.meshes[0].get("af_outline_width", 0.0)) * 100.0
            cells.append(base)
            labels += [f"BAKED {w_cm:.1f}CM ONLY (MOBILE LOW)", f"RIM MEDIAN {st_b['rimPxMedian']:.1f}PX "
                                                              f"P10 {st_b['rimPxP10']:.1f}PX"]
            self.game_outline(1080)
        for k, c in enumerate(cells):
            draw_text(c, 4, 4, labels[2 * k])
            draw_text(c, 4, 14, labels[2 * k + 1], rgb=(255, 220, 140))
        paths_.append(self._save("game_1080crop.png", grid(cells, len(cells), pad=2)))
        ys, xs = st["edge_yx"]
        y0, y1 = max(0, int(ys.min()) - 8), min(ship.shape[0], int(ys.max()) + 9)
        x0, x1 = max(0, int(xs.min()) - 8), min(ship.shape[1], int(xs.max()) + 9)
        z = ink_map(ship, st, zoom=3)[y0 * 3:y1 * 3, x0 * 3:x1 * 3]
        plain = ship.repeat(3, 0).repeat(3, 1)[y0 * 3:y1 * 3, x0 * 3:x1 * 3]
        draw_text(plain, 4, 4, "SHIPPED 1080P  3X NEAREST")
        draw_text(z, 4, 4, f"INK GATE  GREEN >= {st['minPx']:g}PX  RED THINNER", rgb=(255, 255, 255))
        draw_text(z, 4, 14, self.ink_label(st), rgb=(255, 220, 140))
        paths_.append(self._save("game_inkgate.png", grid([plain, z], 2, pad=2)))
        return paths_

    def closeup(self, facing: str = "se", res: tuple[int, int] = (480, 600), pitch: float = -22.0) -> Path:
        self.facing(facing)
        setup_render(res, self.samples)
        pts = mesh_points(self.meshes + self.extra)
        frame_points(pts, res, fov_h=28.0, pitch_ue=pitch, fill=0.9)
        self.ink(at=Vector(pts.mean(0)))
        img = render_array()
        ink = f"  INK {ink_world(self.game_outline_px) * 100:.1f}CM" if self.game_outline_px else ""
        draw_text(img, 6, 6, f"{self.asset}  {facing.upper()} 3/4{ink}")
        return self._save("closeup.png", img)

    def turnaround(self, cell: tuple[int, int] = (210, 270), pitch: float = shading.CAM_PITCH_UE,
                   names: Sequence[str] = TURN_ORDER) -> Path:
        # framing from the posed vertices of all facings (one scale for the whole sheet)
        pts = []
        for n in names:
            self.facing(n)
            pts.append(mesh_points(self.meshes + self.extra, step=3))
        pts = np.concatenate(pts)
        setup_render(cell, self.samples)
        frame_points(pts, cell, fov_h=24.0, pitch_ue=pitch, fill=0.94)
        cells = []
        for n in names:
            self.facing(n)
            self.ink(scale=1.0 if self.game_outline_px else 0.8, at=Vector(pts.mean(0)))
            img = render_array()
            draw_text(img, 4, 4, n)
            cells.append(img)
        self.facing("se")
        return self._save("turnaround.png", grid(cells, 4))

    def material_ab(self, facing: str = "se", res: tuple[int, int] = (300, 380), amp: float = 4.0) -> Path:
        """``material_ab.png``: the full M_AF_Toon stack vs. the same render without the §1.4 rim and grounding
        terms, and their difference (×``amp``, rim in warm, grounding in cool) — close-up (top row) and native
        1080p game crop (bottom row), so both terms can be judged before shipping."""
        self.facing(facing)
        rows = []
        for kind in ("closeup", "game"):
            imgs = {}
            for terms in ((True, True), (False, False)):
                shading.build_toon_group(rim=terms[0], grounding=terms[1])
                if kind == "closeup":
                    setup_render(res, self.samples)
                    pts = mesh_points(self.meshes + self.extra)
                    frame_points(pts, res, fov_h=28.0, pitch_ue=-22.0, fill=0.9)
                    self.ink(at=Vector(pts.mean(0)))
                    img = render_array()
                else:
                    setup_render((1920, 1080), self.samples)
                    cam = game_camera()
                    self.game_outline(1080)
                    c = project(cam, Vector((0, 0, 0.85)))
                    hw = res[0] / 1920.0 / 2
                    hh = res[1] / 1080.0 / 2
                    img = render_array((c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh))[:res[1], :res[0]]
                imgs[terms] = img
            shading.build_toon_group()
            on, off = imgs[(True, True)].astype(np.float32), imgs[(False, False)].astype(np.float32)
            d = on - off
            lum = d @ np.array([0.299, 0.587, 0.114], np.float32)
            diff = np.zeros_like(on)
            diff[:, :] = (24, 20, 30)
            diff += np.clip(lum, 0, None)[:, :, None] * amp * np.array([1.0, 0.85, 0.55], np.float32)
            diff += np.clip(-lum, 0, None)[:, :, None] * amp * np.array([0.45, 0.6, 1.0], np.float32)
            cells = [imgs[(True, True)].copy(), imgs[(False, False)].copy(), diff.clip(0, 255).astype(np.uint8)]
            tag = "CLOSEUP" if kind == "closeup" else "1080P 1:1"
            for c, lab in zip(cells, (f"{tag} FULL STACK", f"{tag} NO RIM NO GROUNDING",
                                      f"DIFF X{amp:g} WARM=RIM COOL=GROUND")):
                draw_text(c, 3, 3, lab)
            rows.append(grid(cells, 3, pad=2))
        w = max(r.shape[1] for r in rows)
        rows = [np.pad(r, ((0, 0), (0, w - r.shape[1]), (0, 0))) for r in rows]
        self.facing("se")
        return self._save("material_ab.png", np.vstack(rows))

    def contact_sheet(self, rig, clips: Sequence, frames: int = 8, cell: tuple[int, int] = (168, 204),
                      facing: str = "se", per_clip_files: bool = False) -> list[Path]:
        """``clips``: [(Clip, Action)]. One row per clip; notify frames get an orange border."""
        self.facing(facing)
        rows, outs = [], []
        # framing: posed vertices over the sampled frames of all clips
        allpts = []
        plan = []
        for clip, act in clips:
            kanim.assign(rig, act)
            n = clip.frames
            picks = sorted(set(int(round(x)) for x in np.linspace(0, n if not clip.loop else n - 1, frames)))
            marks = {}
            for d in clip.notify_list():
                fr = scene.ms_to_frame(d["ms"])
                marks.setdefault(fr, []).append(d)
                if fr not in picks:
                    nearest = min(picks, key=lambda p: abs(p - fr))
                    picks[picks.index(nearest)] = fr
            picks = sorted(set(picks))
            plan.append((clip, act, picks, marks))
            for fr in picks:
                kanim.set_frame(fr)
                allpts.append(mesh_points(self.meshes + self.extra, step=4))
        pts = np.concatenate(allpts)
        setup_render(cell, max(4, self.samples // 2))
        frame_points(pts, cell, fov_h=24.0, pitch_ue=shading.CAM_PITCH_UE, fill=0.95)
        self.ink(scale=1.0 if self.game_outline_px else 0.7, at=Vector(pts.mean(0)))
        width = max(len(p[2]) for p in plan)
        for clip, act, picks, marks in plan:
            kanim.assign(rig, act)
            cells = []
            for fr in picks:
                kanim.set_frame(fr)
                img = render_array()
                ms = scene.frame_to_ms(fr)
                draw_text(img, 3, cell[1] - 10, f"F{fr} {ms:.0f}MS")
                if fr in marks:
                    border(img)
                    for i, d in enumerate(marks[fr]):
                        draw_text(img, 3, 14 + i * 9, f"{d['name']} {d['ms']:.0f}", rgb=(255, 170, 70))
                cells.append(img)
            while len(cells) < width:
                cells.append(np.full_like(cells[0], 24))
            row = grid(cells, width, pad=2)
            label = np.zeros((14, row.shape[1], 3), np.uint8)
            label[:, :] = (24, 20, 30)
            loop = " LOOP" if clip.loop else ""
            draw_text(label, 4, 4, f"{clip.name}  {clip.length_ms:.0f}MS  {clip.frames}F{loop}")
            rowimg = np.vstack([label, row])
            rows.append(rowimg)
            if per_clip_files:
                outs.append(self._save(f"anim_{clip.name}.png", rowimg))
        kanim.set_frame(0)
        sheet = np.vstack(rows)
        outs.insert(0, self._save("anims.png", sheet))
        return outs

    def report(self) -> list[dict]:
        return [{"file": str(p), "bytes": s} for p, s in self.written]
