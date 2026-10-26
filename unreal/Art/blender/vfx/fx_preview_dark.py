"""Dark-remake VFX previews (A12): every march recipe of ``fx_recipes_dark.py`` played by ``fx_preview``'s port of
``UAbyssVfxSystem`` on the **march ground** at the W1 game camera, on the shipped march monsters, NPCs and props
that cast or carry it — plus per-pixel emulations of the two material contracts (``M_AF_Telegraph`` on the ground,
``M_AF_AshDissolve`` on a dead body) and of the echo figure (A10's ``MI_AF_Echo``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_preview_dark.py [ids…|groups…]
    #   groups: ambient elite notify death monster telegraph hazard corpse world status wave2 (default: all)
    #   --scratch   write to the scratchpad instead of Art/Previews/vfx_effects
    #   --list      print the scenario ids and exit
    #   --index     write Previews/vfx_effects/README_dark.md (binding → preview directory) and exit
    #   --measure [asset…]  socket offsets (actor frame) at the notify beats of the MEASURE rows

Each effect → ``Previews/vfx_effects/<recipe-id>/<ms>[-<ms>]ms.png`` exactly like ``fx_preview.py`` (native 1080p crops
at scale 1, truecolour, one or two key frames per file, ≤ 200 KB per file or stacked native strips): the same sprite
compositor (ray / quad intersection, the shipped 8 cm DepthFade), FX meshes in ``M_AF_FX_Mesh``, bloom. What differs:

* the ground is the shipped **march terrain** (``environment/terrain_march``: ash grass with a trampled mud road);
* actors are the shipped march figures re-imported through ``coherence/cohkit`` (every slot, rim overrides,
  attachments, the slime-type shells, the manifest ink) and posed by their exported clips; each actor keeps its own
  clip time (NLA strips offset per actor), anchors are its posed ``fx_*`` sockets re-read every 60 Hz tick;
* ``telegraph`` scenes draw ``M_AF_Telegraph`` per pixel on the ground plane (8 cm over the terrain, depth-tested)
  from the contract in ``fx_recipes_dark.TELEGRAPH`` and the ``T_FX_TelegraphRim`` / ``T_FX_TelegraphFill`` textures;
* ``corpse`` scenes render the body in its Death end pose and apply ``M_AF_AshDissolve`` in screen space (grey to
  ash, crack-first crumble with ``T_FX_AshNoise``) over a plate without the body;
* ``prop.echo`` renders the echo figure with the ``MI_AF_Echo`` maths (additive cold tint, edge lift, the 3 s fade).
"""
from __future__ import annotations

import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE, HERE.parent / "environment", HERE.parent / "coherence"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import paths, pngio, review, scene  # noqa: E402
import fx_preview as FP  # noqa: E402
from fx_preview import Actor, Anchor, Camera, Ctx, Sim, RES, RIGHT, UP, hex_lin, ue_to_bl, ue_cm  # noqa: E402
import fx_recipes_dark as DK  # noqa: E402
import fxkit as K  # noqa: E402

OUT_DIR = FP.OUT_DIR
F0 = 3000.0                                  # scene frame at t = 0 (every actor's clip is offset to it)
FPS = 60.0


# ── the march ground ───────────────────────────────────────────────────────────────────────────────────
def make_march_ground():
    """The shipped march terrain composited at 72 px per tile: ash grass with a trampled mud road along the screen
    axis and a mud patch (both ground values the effects must read on)."""
    import terrain as T
    import terrain_march as TM
    C0, R0, C1, R1 = -9, -9, 10, 10
    rows, cols = R1 - R0, C1 - C0
    m = np.zeros((rows, cols), np.int32)
    for r in range(rows):
        for c in range(cols):
            col, row = c + C0, r + R0
            if abs((col + row) - 1) <= 1 and -7 <= col - row <= 7:
                m[r, c] = 1
            if (col - 4) ** 2 + (row + 5) ** 2 <= 4 or (col + 5) ** 2 + (row - 4) ** 2 <= 3:
                m[r, c] = 1
    cache = K.SCRATCH / "preview_ground_march.png"
    K.SCRATCH.mkdir(parents=True, exist_ok=True)
    if not cache.exists():
        texs = TM.load_textures()
        img = TM.composite(m, T.Layers(texs["tex"], texs["details"]), ppt=72, origin=(C0, R0))
        pngio.write_png(cache, img)
    me = bpy.data.meshes.new("fx_ground")
    x0, x1 = C0 - 0.5, C1 - 0.5
    y0, y1 = -(R0 - 0.5), -(R1 - 0.5)
    me.from_pydata([(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new(name="UVMap")
    for li, (u, v) in enumerate(((0, 1), (1, 1), (1, 0), (0, 0))):
        uv.data[li].uv = (u, v)
    ob = bpy.data.objects.new("fx_ground", me)
    scene.link(ob)
    mat = bpy.data.materials.new("fx_ground")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    tx = nt.nodes.new("ShaderNodeTexImage")
    tx.image = bpy.data.images.load(str(cache))
    tx.interpolation = "Linear"
    em = nt.nodes.new("ShaderNodeEmission")
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(tx.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs[0], o.inputs["Surface"])
    me.materials.append(mat)
    return ob


# ── the stage ──────────────────────────────────────────────────────────────────────────────────────────
class DarkStage(FP.Stage):
    """``fx_preview.Stage`` on the march ground with march figures (no goblins, heroes on demand)."""

    def __init__(self):
        import cohkit as CK
        self.CK = CK
        CK.reset()
        bpy.context.scene.view_settings.view_transform = "Standard"
        self.ground = make_march_ground()
        self.heroes = {}
        self.hero = None
        self.gobs = []
        self.smith = None
        self.mesh_src: dict[str, bpy.types.Object] = {}
        self.live: list[bpy.types.Object] = []
        self.depth_mat = review._flat_material("AF_FxDepth", "depth")
        self.depth_hull = self._depth_hull()
        self.white = FP._emit_material("AF_FxMaskW", (1.0, 1.0, 1.0))
        self.black = FP._emit_material("AF_FxMaskK", (0.0, 0.0, 0.0))
        self.black_hole = review._flat_material("AF_FxHole", "transparent")
        self.timed = None
        self.figs: dict[str, dict] = {}        # pool by (asset, slot)
        self.props: dict[str, object] = {}

    # figures ------------------------------------------------------------------------------------------------
    def figure(self, asset: str, slot: int = 0, variant: str | None = None, bare: bool = False,
               look: str | None = None) -> dict:
        """A shipped march figure (``bare``: without its manifest attachments — the echo is the person, not the
        barrow they pushed; ``look``: a manifest ``looks.weapon`` id, e.g. the footman's shovel for the dig)."""
        key = f"{asset}#{slot}#{variant}#{bare}#{look}"
        if key in self.figs:
            return self.figs[key]
        if asset.startswith("hero:"):
            ch = self.load_hero(asset[5:])
            ch["kind"] = "hero"
            ch["entry"] = FP.manifest()[ch["sk"]]
            ch["strips"] = {}
            self.figs[key] = ch
            return ch
        e = self.CK.entry(asset)
        clips = [a["name"] for a in e.get("anims", [])]
        # cohkit attaches by bone name; a manifest socket may sit on another bone (the merchant's ``barrow`` socket
        # is on ``root``): resolve it through the entry's socket list for this import
        socks = {sk["name"]: sk for sk in e.get("sockets", []) if "bone" in sk}
        orig = self.CK.krig.attach_to_bone

        def attach(obj, rig, name):
            bones = rig.obj.data.bones
            sk = socks.get(name)
            return orig(obj, rig, name if name in bones or sk is None or sk["bone"] not in bones else sk["bone"])

        self.CK.krig.attach_to_bone = attach
        atts0 = e.get("attachments")
        if look:
            lk = next(x for x in (e.get("looks") or {}).get("weapon", []) if x["id"] == look)
            socks_l = {x["socket"] for x in lk["attachments"]}
            e["attachments"] = [x for x in (atts0 or []) if x["socket"] not in socks_l] + lk["attachments"]
        try:
            fig = self.CK.import_character(asset, (0.0, 0.0, 0.0), 0.0, clip=clips[0] if clips else None,
                                           variant=variant)
        finally:
            self.CK.krig.attach_to_bone = orig
            if look:
                e["attachments"] = atts0
        if bare and len(fig.meshes) > fig.n_body:
            for o in fig.meshes[fig.n_body:]:
                bpy.data.objects.remove(o, do_unlink=True)
            del fig.meshes[fig.n_body:]
        self.CK.apply_ink(fig.all())
        anims = {}
        root_dir = paths.export_root()
        for a in e.get("anims", []):
            new = self.CK._import(root_dir / a["fbx"])
            a2 = next((o for o in new if o.type == "ARMATURE"), None)
            for o in new:
                o.hide_render = True
            if a2 is not None:
                a2.parent = fig.root
                anims[a["name"]] = a2
        ch = {"root": fig.root, "meshes": fig.all(), "arm": fig.arm, "anims": anims, "sk": asset,
              "cls": fig.cls, "fig": fig, "entry": e, "kind": "char", "strips": {},
              "sockets": {s["name"]: s for s in e.get("sockets", []) if "bone" in s}}
        self.figs[key] = ch
        return ch

    def hide_all(self):
        for ch in self.figs.values():
            for m in ch["meshes"]:
                m.hide_render = True
        for p in self.props.values():
            p.hide_render = True

    def place(self, ch, at_ue, facing_ue, visible=True, scale: float | None = None):
        ch["root"].location = ue_to_bl(at_ue)
        d = np.asarray(facing_ue, float)
        db = Vector((d[0], -d[1], 0))
        ch["root"].rotation_euler = (0, 0, math.atan2(db.x, -db.y))
        for m in ch["meshes"]:
            m.hide_render = not visible
        for k, o in ch.get("swap", {}).items():
            if o is not None and visible:
                o.hide_render = k == "bow"

    def set_clip(self, ch, clip: str, t0_ms: float, hold: bool = False):
        """Pose the figure by ``clip``: every bone copies (pose space) the clip's own imported armature, whose action
        plays on an NLA strip offset so the clip shows ``t0_ms`` at scene frame F0 (``hold``: ``hold_at`` keeps it
        there). This is what UE does with an anim on the skeleton (the clip's local transforms down the chain, the
        mesh skinned against the skeleton's bind pose): the clip FBXs store a bind pose of their own (A2's
        ``weapon_r`` rests upright in the clips, forward in the SK file), so playing their local channels on the SK
        armature turned the standard and the shovel 90°."""
        if clip not in ch["anims"]:
            clip = "Idle" if "Idle" in ch["anims"] else next(iter(ch["anims"]))
        arm, src = ch["arm"], ch["anims"][clip]
        if arm.animation_data is not None:
            arm.animation_data.action = None
            for tr in arm.animation_data.nla_tracks:
                for st0 in tr.strips:
                    st0.mute = True
        for pb in arm.pose.bones:
            c = next((x for x in pb.constraints if x.type == "COPY_TRANSFORMS"), None)
            if c is None:
                c = pb.constraints.new("COPY_TRANSFORMS")
                c.target_space = c.owner_space = "POSE"
            c.target = src
            c.subtarget = pb.name if pb.name in src.data.bones else ""
            c.mute = pb.name not in src.data.bones
        st = ch["strips"].get(clip)
        if st is None:
            sad = src.animation_data
            act = sad.action if sad.action is not None else ch.get("acts", {}).get(clip)
            slot = getattr(sad, "action_slot", None)
            ch.setdefault("acts", {})[clip] = act
            sad.action = None
            tr = sad.nla_tracks.new()
            st = tr.strips.new(clip, int(act.frame_range[0]), act)
            if slot is not None and hasattr(st, "action_slot"):
                st.action_slot = slot
            st.extrapolation = "HOLD"
            ch["strips"][clip] = st
        info = FP.clip_info(ch["sk"], clip)
        L = float(info.get("lengthMs", 1000.0))
        loop = bool(info.get("loop"))
        a0, a1 = st.action_frame_start, st.action_frame_end
        if hold:
            st.repeat = 1.0
            start = F0 - min(t0_ms, L - 1000.0 / FPS) * FPS / 1000.0
        elif loop:
            st.repeat = 40.0
            start = F0 - t0_ms * FPS / 1000.0 - (a1 - a0) * 10
        else:
            st.repeat = 1.0
            start = F0 - t0_ms * FPS / 1000.0
        st.frame_start_ui = start
        ch["clip"] = clip
        ch["t0"] = t0_ms
        ch["hold"] = hold
        ch["loopL"] = L if loop else 0.0

    def hold_at(self, ch, t_ms: float):
        """A held actor shows clip time ch['t0'] at scene time t: shift its strip back by t."""
        st = ch["strips"][ch["clip"]]
        L = float(FP.clip_info(ch["sk"], ch["clip"]).get("lengthMs", 1000.0))
        st.frame_start_ui = F0 + t_ms * FPS / 1000.0 - min(ch["t0"], L - 1000.0 / FPS) * FPS / 1000.0

    def sockets(self, ch) -> dict:
        bpy.context.view_layer.update()
        out = {}
        e = ch["entry"]
        for s in e.get("sockets", []):
            if "bone" in s and s["bone"] in ch["arm"].data.bones:
                out[s["name"]] = ue_cm(FP.socket_world(ch["arm"], s))
        return out

    def prop(self, asset: str):
        if asset not in self.props:
            o = self.CK.import_static(asset)
            self.CK.apply_ink([o])
            self.props[asset] = o
        return self.props[asset]


# ── figures and scenes ─────────────────────────────────────────────────────────────────────────────────
@dataclass
class Fig:
    key: str
    asset: str
    at: tuple = (0.0, 0.0)           # (right, up) cm on the ground from the scene centre (screen axes)
    face: object = (0.0, -1.0)       # a figure key to face, or (right, up) screen direction
    clip: str = "Idle"
    t0: float = 0.0                  # clip time at t = 0
    hold: bool = False               # freeze at t0
    variant: str | None = None
    visible: bool = True
    bare: bool = False               # without the manifest attachments (the echo figure)
    look: str | None = None          # a manifest looks.weapon id (the footman's shovel)


@dataclass
class DScene:
    rid: str
    figs: list
    events: list                     # (t_ms, kind, who, rid)
    frames: list
    crop: tuple = (640, 480)
    focus: object = (0.0, 0.0)       # a figure key or (right, up)
    radius: float = 100.0
    color: str | None = None
    props: list = field(default_factory=list)          # (asset, (right, up), yaw deg)
    telegraph: dict | None = None
    ash: dict | None = None
    ghost: dict | None = None
    tight: bool = True
    note: str = ""


def gpos(at) -> np.ndarray:
    r, u = at
    return RIGHT * r + UP * u


def socket_heights(e: dict) -> dict:
    s = {x["name"]: x for x in e.get("sockets", []) if "restLocCm" in x}
    out = {}
    for h, name in FP.SOCKET_OF.items():
        if name in s:
            out[h] = float(s[name]["restLocCm"][2])
    out.setdefault("hand", out.get("chest", 80.0))
    return {**FP.DEFAULT_H, **out}


# ── material emulations ────────────────────────────────────────────────────────────────────────────────
def ground_points(cam: Camera, W: int, H: int, z_cm: float = 8.0):
    """Per crop pixel: the UE ground-plane point (cm) at height z, and its view depth (m)."""
    gy, gx = np.mgrid[0:H, 0:W].astype(np.float64)
    d = cam.rays(gx, gy)
    zb = z_cm / 100.0
    t = (zb - cam.O[2]) / np.where(np.abs(d[..., 2]) > 1e-9, d[..., 2], -1e-9)
    P = cam.O + d * t[..., None]
    depth = (P - cam.O) @ cam.fwd
    ue = np.stack([P[..., 0] * 100.0, -P[..., 1] * 100.0], axis=-1)
    return ue, depth


def telegraph_layer(spec: dict, ue: np.ndarray, t_ms: float) -> tuple[np.ndarray, np.ndarray] | None:
    """``M_AF_Telegraph`` (fx_recipes_dark.TELEGRAPH) at scene time ``t_ms`` → (linear RGB, alpha) per pixel."""
    C = DK.TELEGRAPH
    p = dict(C["params"])
    style = spec.get("style", "normal")
    sty = C["styles"].get(style, {})
    p.update({k: v for k, v in sty.items() if not isinstance(v, (dict, str))})
    inner = sty.get("innerRim")
    fl, fo = C["contact"]["flashMs"], C["contact"]["fadeMs"]
    lingering = style in ("hazard", "arena")     # zones: rim only, on from start to end, then fade (no flash)
    start = float(spec["start"])
    if lingering:
        end = float(spec.get("end", spec.get("contact", 1e9)))
        if t_ms < start or t_ms > end + fo:
            return None
        contact, after, prog = end, -1.0, 1.0
        linger_k = 1.0 if t_ms <= end else max(0.0, 1.0 - (t_ms - end) / fo)
    else:
        contact = float(spec["contact"])
        after = t_ms - contact
        if t_ms < start or after > fl + fo:
            return None
        prog = min(max((t_ms - start) / max(contact - start, 1.0), 0.0), 1.0)
    o = np.asarray(spec["origin"], float)[:2]
    rel = ue - o
    dirv = np.asarray(spec.get("dir", (1.0, 0.0)), float)
    dirv = dirv / np.linalg.norm(dirv)
    nrm = np.array([-dirv[1], dirv[0]])
    a_ = rel @ dirv
    b_ = rel @ nrm
    r = np.hypot(rel[..., 0], rel[..., 1])
    shape = spec["shape"]
    R = float(spec.get("radius", 100.0))
    if shape == "circle":
        w = R - r                                              # inward distance to the boundary (cm)
        u_edge = np.arctan2(rel[..., 1], rel[..., 0]) * R
        fill_r, fill_max = r, R
    elif shape == "ring":
        Ri = float(spec.get("inner", R * 0.5))
        w = np.minimum(R - r, r - Ri)
        u_edge = np.arctan2(rel[..., 1], rel[..., 0]) * R
        fill_r, fill_max = r - Ri, R - Ri
    elif shape == "cone":
        half = math.radians(float(spec.get("arc", 90.0)) / 2)
        ang = np.arctan2(b_, a_)
        side = np.minimum(np.abs(a_ * math.sin(half) - b_ * math.cos(half)),
                          np.abs(a_ * math.sin(-half) - b_ * math.cos(-half)))
        inside = (np.abs(ang) <= half) & (r <= R)
        w = np.where(inside, np.minimum(R - r, side), -1.0)
        u_edge = np.where(R - r < side, ang * R, r)
        fill_r, fill_max = r, R
    else:   # line
        Lh = float(spec.get("length", 500.0))
        Wd = float(spec.get("width", 120.0)) / 2
        inside = (a_ >= 0) & (a_ <= Lh) & (np.abs(b_) <= Wd)
        ends = np.minimum(a_, Lh - a_)
        sides = Wd - np.abs(b_)
        w = np.where(inside, np.minimum(ends, sides), -1.0)
        u_edge = np.where(sides < ends, a_, b_)
        fill_r, fill_max = a_, Lh
    if style == "arena":                      # the damage is outside: measure the rim outward from the boundary
        w = -w
    rimW = float(p["RimWidthCm"])
    v = np.clip(w / rimW, 0.0, 1.0)
    aa = np.clip(w + 0.5, 0.0, 1.0)
    rim_tex = FP.tex("TelegraphRim")
    u = np.mod(u_edge / float(p["RimTileCm"]), 1.0)
    rs = FP.bilinear(rim_tex, u, np.clip(v, 0.0, 0.999))
    urg_from = float(C["styles"].get(style, {}).get("urgencyFrom", 0.7))
    pulse = 1.0
    if prog >= urg_from and after < 0:
        pulse = 0.82 + 0.18 * math.sin(2 * math.pi * t_ms / 180.0)
    rim_a = rs[..., 3] * float(p["RimAlpha"]) * pulse * aa * (w <= rimW)
    rim_rgb = rs[..., :3] * np.array(hex_lin(p["RimColor"]))
    if inner:
        di = np.abs(w - (1.0 - float(inner["atFraction"])) * (R if shape not in ("line",) else Wd))
        rim_a = np.maximum(rim_a, np.clip(1.0 - (di - float(inner["widthCm"]) / 2) / 1.5, 0, 1) *
                           float(inner["alpha"]) * (w > 0))
    fill_rgb = np.zeros(ue.shape[:2] + (3,))
    fill_a = np.zeros(ue.shape[:2])
    if style == "arena":                      # a faint flat crimson wash on the side that hurts, ramping in
        wash = sty.get("outerWash", {})
        fill_rgb = np.broadcast_to(np.array(hex_lin(wash.get("color", p["RimColor"]))), fill_rgb.shape)
        fill_a = float(wash.get("alpha", 0.1)) * np.clip(w / float(wash.get("fadeInCm", 60.0)), 0.0, 1.0) * (w > 0)
        rim_a = rim_a * (0.85 + 0.15 * math.sin(2 * math.pi * t_ms / 1200.0))
    elif style != "hazard":
        ft = FP.tex("TelegraphFill")
        fu = np.mod(ue[..., 0] / float(p["FillTileCm"]), 1.0)
        fv = np.mod(ue[..., 1] / float(p["FillTileCm"]), 1.0)
        fs = FP.bilinear(ft, fu, fv)[..., :3]
        front = prog * fill_max
        filled = (w > 0) & (fill_r <= front)
        fa = float(p["FillAlphaStart"]) + (float(p["FillAlphaContact"]) - float(p["FillAlphaStart"])) * prog
        g = float(p.get("FillGrain", 1.0))
        fill_rgb = (1.0 - g * (1.0 - fs)) * np.array(hex_lin(p["FillColor"]))
        fill_a = fa * filled
        band = filled & (fill_r >= front - float(p["FrontWidthCm"]))
        fill_rgb = np.where(band[..., None], np.array(hex_lin(p["FrontColor"])), fill_rgb)
        fill_a = np.where(band, float(p["FrontAlpha"]), fill_a)
    else:
        rim_a = rim_a * (0.8 + 0.2 * math.sin(2 * math.pi * t_ms / 1200.0))
    if lingering:
        rim_a = rim_a * linger_k
        fill_a = fill_a * linger_k
    elif after >= 0:                   # the contact flash, then the fade
        k = 1.0 if after <= fl else max(0.0, 1.0 - (after - fl) / fo)
        flash = after <= fl
        if flash:
            fill_rgb = np.broadcast_to(np.array(hex_lin("#FFF4EE")), fill_rgb.shape)
            fill_a = float(C["contact"].get("fillAlpha", 0.55)) * (w > 0)
            rim_rgb = rs[..., :3] * np.array(hex_lin("#FF3A48"))
        fill_a = fill_a * k
        rim_a = rim_a * k
    a = rim_a + fill_a * (1 - rim_a)
    rgb = (rim_rgb * rim_a[..., None] + fill_rgb * (fill_a * (1 - rim_a))[..., None]) / np.maximum(a[..., None], 1e-6)
    return rgb, np.clip(a, 0, 1)


def ash_noise() -> np.ndarray:
    return FP.tex("AshNoise")


# ── the runner ─────────────────────────────────────────────────────────────────────────────────────────
def run(stage: DarkStage, recipes: dict, s: DScene, out_dir: Path) -> list:
    stage.hide_all()
    figs = {}
    used: dict[str, int] = {}
    for f in s.figs:
        n = used.get(f.asset, 0)
        used[f.asset] = n + 1
        ch = stage.figure(f.asset, n, f.variant, bare=f.bare, look=f.look)
        figs[f.key] = (f, ch)
    pos = {k: gpos(f.at) for k, (f, _) in figs.items()}

    def facing(f):
        if isinstance(f.face, str):
            v = pos[f.face] - pos[f.key]
        else:
            v = gpos(f.face)
        return v / max(np.linalg.norm(v[:2]), 1e-6)
    for k, (f, ch) in figs.items():
        stage.place(ch, pos[k], facing(f), visible=f.visible)
        stage.set_clip(ch, f.clip, f.t0, hold=f.hold)
    props = []
    for asset, at, yaw in s.props:
        o = stage.prop(asset)
        o.location = ue_to_bl(gpos(at))
        o.rotation_euler = (0, 0, math.radians(yaw))
        o.hide_render = False
        props.append((asset, gpos(at), o))
    actors = {}
    for k, (f, ch) in figs.items():
        actors[k] = Actor(k, pos[k].copy(), socket_heights(ch["entry"]))

    def set_time(t_ms: float):
        for k, (f, ch) in figs.items():
            if f.hold:
                stage.hold_at(ch, t_ms)
        fr = F0 + t_ms * FPS / 1000.0
        fi = int(math.floor(fr))
        bpy.context.scene.frame_set(fi, subframe=fr - fi)
        bpy.context.view_layer.update()
        for k, (f, ch) in figs.items():
            sk = stage.sockets(ch)
            a = actors[k]
            a.sockets = {h: sk[n] for h, n in FP.SOCKET_OF.items() if n in sk}
            a.extra = sk

    # camera + crop
    focus = pos[s.focus] if isinstance(s.focus, str) else gpos(s.focus)
    review.setup_render(RES, 8)
    fb = ue_to_bl(focus)
    cam_obj = review.game_camera((fb.x, fb.y))
    c = review.project(cam_obj, Vector((fb.x, fb.y, 0.6)))
    cw, chh = s.crop
    x0 = int(round(c[0] * RES[0] - cw / 2))
    y0 = int(round((1 - c[1]) * RES[1] - chh / 2))
    x0, y0 = max(0, min(RES[0] - cw, x0)), max(0, min(RES[1] - chh, y0))
    box = (x0 / RES[0], 1 - (y0 + chh) / RES[1], (x0 + cw) / RES[0], 1 - y0 / RES[1])
    cam = Camera(cam_obj, RES, (x0, y0, cw, chh))
    R_ = np.array(cam_obj.matrix_world)[:3, :3]
    sim = Sim(recipes, (FP.bl_to_ue_dir(R_[:, 0]), FP.bl_to_ue_dir(R_[:, 1]), FP.bl_to_ue_dir(-R_[:, 2])))
    color = hex_lin(s.color) if s.color else None
    base = dict(radius=s.radius, color=color, element=None, big=False, sparks=6, ring=13 * 100 / 45)
    set_time(0.0)

    def where(w):
        if isinstance(w, str):
            return pos[w].copy(), actors.get(w)
        return gpos(w), None
    handles: dict[str, int] = {}
    timed = []
    for t0, kind, who, rid in sorted(s.events, key=lambda e: e[0]):
        if kind == "notify":           # UAbyssVfxSystem::PlayNotify: the actor, Blow = its forward
            def fn(rid=rid, who=who):
                f, ch = figs[who]
                fw = facing(f)
                p = pos[who].copy()
                return sim.play(rid, Ctx(origin=p, target=p, point=p, origin_actor=actors[who],
                                         target_actor=actors[who], **dict(base, blow=fw[:2])))
        elif kind == "hit":            # attacker → victim (kills, blocks)
            def fn(rid=rid, who=who):
                a, b = who
                pa, pb = pos[a].copy(), pos[b].copy()
                bl = (pb - pa)[:2] / max(np.linalg.norm((pb - pa)[:2]), 1e-6)
                return sim.play(rid, Ctx(origin=pa, target=pb, point=pb, origin_actor=actors[a],
                                         target_actor=actors[b], **dict(base, blow=bl)))
        elif kind == "death":          # death.<defId|family>: at the victim, Blow = the VICTIM's facing (INTEGRATION
            def fn(rid=rid, who=who):  # deathFamily: the death ids are anchored to the dying actor, not the blow)
                a, b = who
                fv = facing(figs[b][0])
                return sim.play(rid, Ctx(origin=pos[a].copy(), target=pos[b].copy(), point=pos[b].copy(),
                                         origin_actor=actors[a], target_actor=actors[b], **dict(base, blow=fv[:2])))
        elif kind == "self":           # blow-less (elites, echo): +X frame
            def fn(rid=rid, who=who):
                p, a = where(who)
                return sim.play(rid, Ctx(origin=p, target=p, point=p, origin_actor=a, target_actor=a,
                                         **dict(base, blow=np.array([1.0, 0.0]))))
        elif kind == "point":          # ground events (raise mark, hazards, corpses, contact)
            def fn(rid=rid, who=who):
                p, a = where(who)
                return sim.play(rid, Ctx(origin=p, target=p, point=p, target_actor=a,
                                         **dict(base, blow=np.array([1.0, 0.0]))))
        elif kind == "path":           # origin → target (lines, feed, blink)
            def fn(rid=rid, who=who):
                (pa, aa), (pb, ab) = where(who[0]), where(who[1])
                bl = (pb - pa)[:2] / max(np.linalg.norm((pb - pa)[:2]), 1e-6)
                return sim.play(rid, Ctx(origin=pa, target=pb, point=pb, origin_actor=aa, target_actor=ab,
                                         **dict(base, blow=bl)))
        elif kind == "ambient":
            def fn(rid=rid):
                return sim.play(rid, Ctx(origin=focus.copy(), target=focus.copy(), point=focus.copy(),
                                         **dict(base, blow=np.array([1.0, 0.0]))))
        elif kind == "prop":
            def fn(rid=rid, who=who):
                p = props[who][1].copy()
                return sim.play(rid, Ctx(origin=p, target=p, point=p, **dict(base, blow=np.array([1.0, 0.0]))))
        elif kind == "stop":
            def fn(rid=rid):
                sim.stop(handles.get(rid, 0))
                return 0
        elif kind == "mproj":          # a monster bolt: launch at the source hand, hit the target's chest
            def fn(rid=rid, who=who):
                src, dst, travel = who
                a = FP.actor_point(actors[src], "hand", 0)
                b = FP.actor_point(actors[dst], "chest", 0)

                def moving(t_ms, a=a, b=b, t_start=sim.t):
                    u = min(max((t_ms - t_start) / travel, 0.0), 1.0)
                    return a + (b - a) * u
                bl = (b - a)[:2] / np.linalg.norm((b - a)[:2])
                ctx = Ctx(origin=a, target=b, point=b, origin_actor=actors[src], target_actor=actors[dst],
                          **dict(base, blow=bl))
                sim.play(f"{rid}.launch", ctx)
                hc = Ctx(**{**ctx.__dict__, "origin_actor": None})
                h = sim.play(rid, hc, anchor_override=Anchor(None, "ground", 0, a, moving=moving))
                timed.append((sim.t + travel, lambda h=h, ctx=ctx, dst=dst: (
                    sim.stop(h), sim.play(f"{rid}.hit", Ctx(**{**ctx.__dict__, "point": pos[dst].copy(),
                                                               "target": pos[dst].copy()})))))
                timed.sort(key=lambda e: e[0])
                return h
        else:
            raise ValueError(kind)
        timed.append((t0, (lambda fn=fn, rid=rid: handles.__setitem__(rid, fn() or 0))))
    timed.sort(key=lambda e: e[0])

    cells = []
    dt = 1000.0 / FPS
    t = 0.0
    fi = 0
    frames = sorted(max(f, 16.7) for f in s.frames)
    bg_key, bg_col, bg_dep = None, None, None
    anim = any(not f.hold and (f.clip not in ("Idle",) or f.t0) for f, _ in figs.values())
    ghost = s.ghost
    ash = s.ash
    plate_cache = {}
    ue_g, dep_g = (None, None)
    while fi < len(frames):
        while timed and timed[0][0] <= t + 1e-6:
            timed.pop(0)[1]()
        if t + 1e-6 >= frames[fi]:
            set_time(t)
            sprites, meshes = sim.instances()
            key = tuple(round((ch.get("t0", 0.0) + (0 if f.hold else t)), 1) for f, ch in figs.values()) if anim else 0
            if meshes:
                stage.place_meshes(meshes)
                col, dep = stage.render(box)
                stage.place_meshes([])
                if key != bg_key:
                    bg_key = key
                    bg_col, bg_dep = stage.render(box)
            elif key == bg_key:
                col, dep = bg_col.copy(), bg_dep
            else:
                col, dep = stage.render(box)
                bg_key, bg_col, bg_dep = key, col.copy(), dep
            img = col.copy()
            if ash is not None:
                img, dep = apply_ash(stage, figs, ash, box, img, dep, t, plate_cache, cam)
            if ghost is not None:
                img = apply_ghost(stage, figs, ghost, box, img, t)
            plate = img.copy()
            if s.telegraph is not None:
                if ue_g is None:
                    ue_g, dep_g = ground_points(cam, img.shape[1], img.shape[0], 8.0)
                for spec in (s.telegraph if isinstance(s.telegraph, list) else [s.telegraph]):
                    tl = telegraph_layer({**spec, "origin": gpos(spec["at"]) if "at" in spec else spec["origin"],
                                          "dir": gpos(spec["dir"])[:2] if "dir" in spec else (1.0, 0.0)},
                                         ue_g, t)
                    if tl is not None:
                        rgb, a = tl
                        soft = np.clip((dep - dep_g) / 0.08, 0, 1)
                        a = a * soft
                        img = img * (1 - a[..., None]) + rgb * a[..., None]
            FP.composite_sprites(img, dep, cam, sprites)
            touched = np.abs(img - plate).max(axis=2) > 0.02
            if meshes:
                touched |= np.abs(col - bg_col).max(axis=2) > 0.02
            img = FP.bloom_lin(img)
            cells.append((frames[fi], FP.to_srgb8(img), touched))
            fi += 1
            continue
        sim.update(dt)
        t += dt
    stage.place_meshes([])
    subj = [pos[k] for k, (f, _) in figs.items() if f.visible]
    subj += [p for _, p, _ in props]
    if s.telegraph is not None:
        for spec in (s.telegraph if isinstance(s.telegraph, list) else [s.telegraph]):
            o = gpos(spec["at"])
            Rr = float(spec.get("radius", spec.get("length", 200.0)))
            subj += [o + RIGHT * Rr * 0.7, o - RIGHT * Rr * 0.7]
    box_px = FP.tight_box([c[2] for c in cells], cam, subj, (cw, chh)) if s.tight else (0, 0, cw, chh)
    x0c, y0c, x1c, y1c = box_px
    final = []
    for ms, out, _ in cells:
        o = np.ascontiguousarray(out[y0c:y1c, x0c:x1c])
        FP.label(o, f"{s.rid}  +{int(ms)}MS  1:1 1080P  MARCH")
        final.append((ms, o))
    written = FP.write_effect(s.rid, final, out_dir)
    kb = sum(sz for _, sz in written) // 1024
    print(f"  {s.rid}: {len(cells)} frames {x1c - x0c}x{y1c - y0c} native -> {len(written)} files, {kb} KB")
    stage.hide_all()
    return written


# ── ash dissolve and the echo figure ────────────────────────────────────────────────────────────────────
def lin_lum(img):
    return img[..., 0] * 0.2126 + img[..., 1] * 0.7152 + img[..., 2] * 0.0722


def apply_ash(stage, figs, ash: dict, box, img, dep, t_ms: float, cache: dict, cam=None):
    """``M_AF_AshDissolve`` in screen space on figure ``ash['fig']``: grey from ``grey`` ms over greyMs, crumble
    over crumbleMs to crumbleTo, expire from ``expire`` ms. The plate without the body (rendered once) shows
    through the dropped pixels."""
    C = DK.ASH_DISSOLVE
    tl = C["timeline"]
    f, ch = figs[ash["fig"]]
    g0 = float(ash.get("grey", 0.0))
    gk = min(max((t_ms - g0) / tl["greyMs"], 0.0), 1.0)
    c0 = g0 + tl["greyMs"]
    ck = min(max((t_ms - c0) / tl["crumbleMs"], 0.0), 1.0) * tl["crumbleTo"]
    if "expire" in ash:
        ek = min(max((t_ms - float(ash["expire"])) / tl["expireMs"], 0.0), 1.0)
        ck = ck + (1.0 - ck) * ek if t_ms >= float(ash["expire"]) else ck
    if gk <= 0.0:
        return img, dep
    if "plate" not in cache:
        for m in ch["meshes"]:
            m.hide_render = True
        cache["plate"] = stage.render(box)
        for m in ch["meshes"]:
            m.hide_render = False
    mkey = round(t_ms, 1) if not f.hold else 0
    if cache.get("mkey") != mkey:
        cache["mask"] = stage.render_mask(box, ch["meshes"], slot0_only=False)
        cache["mkey"] = mkey
    mask = cache["mask"]
    plate_col, plate_dep = cache["plate"]
    ashc = np.array(hex_lin(C["params"]["AshColor"]))
    shade = np.array(hex_lin(C["params"]["AshShade"]))
    L = lin_lum(img)
    k = np.clip((L - 0.02) / 0.12, 0, 1)[..., None]
    ashed = shade + (ashc - shade) * k
    ink = (L < 0.012)[..., None]
    ashed = np.where(ink, img * 0.9, ashed)
    out = img + (ashed - img) * (gk * mask[..., None])
    if ck > 0:
        H, W = mask.shape
        nz = ash_noise()
        ys, xs = np.nonzero(mask > 0.5)
        if len(ys):
            top, bot = ys.min(), ys.max()
            hf = np.clip((bot - np.arange(H)) / max(bot - top, 1), 0, 1)[:, None] * np.ones((1, W))
            gy, gx = np.mgrid[0:H, 0:W]
            # the noise tile spans 60 cm on the body (the contract's object-space map) at this camera
            tile = 60.0
            if cam is not None:
                o = ch["root"].matrix_world.translation
                p0, p1 = cam.project(o), cam.project(o + Vector((0.6, 0.0, 0.0)))
                if p0 is not None and p1 is not None:
                    tile = max(8.0, math.hypot(p1[0] - p0[0], p1[1] - p0[1]))
            u = (gx / tile) % 1.0
            v = (gy / tile) % 1.0
            smp = FP.bilinear(nz, u, v)
            score = smp[..., 2] * 0.6 + smp[..., 0] * 0.3 + (1 - smp[..., 1]) * 0.15 + hf * 0.3
            drop = (score < ck * 1.4) & (mask > 0.02)
            edge = (score < ck * 1.4 + 0.05) & ~drop & (mask > 0.5)
            out = np.where(drop[..., None], plate_col, out)
            dep = np.where(drop, plate_dep, dep)
            ec = np.array(hex_lin(C["params"]["EdgeColor"]))
            out = np.where(edge[..., None], out * 0.4 + ec * 0.6, out)
    return out, dep


def ghost_material(tint_hex: str, scale: float):
    name = f"AF_EchoGhost_{tint_hex}"
    m = bpy.data.materials.get(name)
    if m is not None:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    lw = nt.nodes.new("ShaderNodeLayerWeight")
    lw.inputs["Blend"].default_value = 0.5
    inv = nt.nodes.new("ShaderNodeMath")
    inv.operation = "SUBTRACT"
    inv.inputs[0].default_value = 1.0
    nt.links.new(lw.outputs["Facing"], inv.inputs[1])
    mr = nt.nodes.new("ShaderNodeMapRange")          # 0.85 + 0.3 · edge (edge = 1 − facing … as MI_AF_Echo)
    mr.inputs["To Min"].default_value = 1.15
    mr.inputs["To Max"].default_value = 0.85
    nt.links.new(inv.outputs[0], mr.inputs["Value"])
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    lin = hex_lin(tint_hex)
    mul.inputs["A"].default_value = (lin[0] * scale, lin[1] * scale, lin[2] * scale, 1.0)
    nt.links.new(mr.outputs["Result"], mul.inputs["B"])
    em = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(mul.outputs["Result"], em.inputs["Color"])
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m


def apply_ghost(stage, figs, ghost: dict, box, img, t_ms: float):
    """The echo figure (A10 MI_AF_Echo: additive cold tint × (0.85 + 0.3 edge), fade 450 / hold 2100 / 450 ms with
    a 4 Hz ± .08 flicker in the hold) rendered alone over black and added; the figure is hidden in the plate."""
    f, ch = figs[ghost["fig"]]
    tl = {"in": 450.0, "hold": 2100.0, "out": 450.0}
    if t_ms < tl["in"]:
        k = 1 - (1 - t_ms / tl["in"]) ** 2
    elif t_ms < tl["in"] + tl["hold"]:
        k = 1.0 - 0.08 * (0.5 + 0.5 * math.sin(2 * math.pi * 4.0 * t_ms / 1000.0))
    else:
        k = max(0.0, 1.0 - ((t_ms - tl["in"] - tl["hold"]) / tl["out"]) ** 2)
    if k <= 0:
        return img
    mat = ghost_material(ghost.get("tint", DK.ECHO_BODY), ghost.get("scale", 0.5))
    vis = [o for o in bpy.data.objects if o.type == "MESH" and not o.hide_render]
    for o in vis:
        o.hide_render = True
    saved = []
    for o in ch["meshes"]:
        if o.type != "MESH":
            continue
        mats = o.data.materials
        saved.append((o, [mats[i] for i in range(len(mats))]))
        for i in range(len(mats)):
            mats[i] = mat if i != 1 else stage.black_hole
        o.hide_render = False
    try:
        review.setup_render(RES, 8, bg_hex="#000000")
        g = review.render_array(box, exr=True)[..., :3]
    finally:
        for o, mats in saved:
            for i, mm in enumerate(mats):
                o.data.materials[i] = mm
            o.hide_render = True
        for o in vis:
            o.hide_render = False
        for o in ch["meshes"]:
            o.hide_render = True
        review.setup_render(RES, 8)
    return img + g * k


# ── scenarios ──────────────────────────────────────────────────────────────────────────────────────────
FOOT, ARB, SHIELD, BEARER = "SK_Mon_RisenFootman", "SK_Mon_RisenArbalest", "SK_Mon_RisenShieldbearer", \
    "SK_Mon_DeadStandardBearer"
ZEAL, DES, DRUM = "SK_Mon_OathZealot", "SK_Mon_Deserter", "SK_Mon_DeserterDrummer"
GNAW, GORGE = "SK_Mon_Gravegnawer", "SK_Mon_Bonegorger"
CLOT, BLOAT, HERD, VAL = "SK_Mon_Breachclot", "SK_Mon_Cinderbloat", "SK_Mon_Corpseherd", "SK_Mon_Valdras"
HERO = "hero:warrior"


def notify_ms(asset: str, clip: str, name: str) -> float:
    return float(next((n["ms"] for n in FP.clip_info(asset, clip).get("notifies", []) if n["name"] == name), 0.0))


def scenarios() -> dict:
    sc = {}
    S = DScene
    hit_ms = FP.beat_ms("SK_Hero_Warrior", "Attack01")
    # ambience
    sc["ambient.ashfall"] = S("ambient.ashfall", [Fig("h", HERO, (0, 0), (1, -0.4)), Fig("m", FOOT, (260, 60), "h")],
                              [(0, "ambient", None, "ambient.ashfall")], [9000, 11000], crop=(960, 540),
                              focus=(120, 20), tight=False, color="#CFCAC0")
    sc["ambient.sparks"] = S("ambient.sparks", [Fig("m", VAL, (120, 40), (-1, -0.3))],
                             [(0, "ambient", None, "ambient.sparks")], [6000, 8000], crop=(960, 540),
                             focus=(80, 20), tight=False, color="#B02FD0",
                             note="the Breach Rim region: ambient.sparks with the weather tint #B02FD0")
    # elites: one on the mud road, one on the ash grass
    for e, a1, a2 in (("fire_enhanced", FOOT, DES), ("swift", GNAW, DES), ("teleporting", FOOT, ZEAL),
                      ("extra_strong", ZEAL, SHIELD), ("curse_aura", FOOT, DES), ("vampiric", FOOT, GNAW),
                      ("frozen", SHIELD, DES), ("unburied", FOOT, BEARER)):
        sc[f"elite.{e}"] = S(f"elite.{e}", [Fig("a", a1, (0, 0), (-1, -0.3)), Fig("b", a2, (-170, 150), (-1, -0.6))],
                             [(0, "self", "a", f"elite.{e}"), (0, "self", "b", f"elite.{e}")],
                             [400, 800, 1200, 1600], crop=(760, 560), focus=(-85, 75))
    # W2 on-death affix effects: the elite dies at t = 0 (its Death clip), the hero stands at the edge of the zone
    sc["elite.fire_enhanced.death"] = S("elite.fire_enhanced.death",
                                        [Fig("v", FOOT, (0, 0), (-1, -0.3), clip="Death"),
                                         Fig("h", HERO, (-230, -60), "v")],
                                        [(0, "point", "v", "elite.fire_enhanced.death"),
                                         (1000, "point", "v", "telegraph.contact")],
                                        [100, 500, 900, 1010, 1100, 1400], crop=(820, 640), focus=(-60, -10),
                                        radius=200,
                                        telegraph={"shape": "circle", "at": (0, 0), "radius": 200, "start": 0,
                                                   "contact": 1000, "style": "elite"},
                                        note="Scorched on death: the elite-style crimson r2 decal (U2) fills over "
                                             "1 s while the corpse smoulders, then the burst + telegraph.contact")
    sc["elite.frozen.death"] = S("elite.frozen.death",
                                 [Fig("v", SHIELD, (0, 0), (-1, -0.3), clip="Death"),
                                  Fig("h", HERO, (-200, -70), "v")],
                                 [(0, "point", "v", "elite.frozen.death")],
                                 [100, 300, 560, 620, 700, 1200, 2400], crop=(820, 640), focus=(-60, -10),
                                 note="Gravecold on death: rime fades in over r2.5 (the pre-cue, no crimson: a "
                                      "slow), the ring at 600 ms, the crust holds for the 2.5 s slow")
    sc["teleport.blink"] = S("teleport.blink", [Fig("a", ZEAL, (300, 40), (-1, 0))],
                             [(0, "path", ((0, 0), "a"), "teleport.blink")], [0, 60, 120, 200, 320],
                             crop=(820, 460), focus=(150, 20))
    # notifies on their monsters, in the clip that carries them
    sc["notify.FX_Fuse"] = S("notify.FX_Fuse", [Fig("m", BLOAT, (0, 0), (-1, -0.5), clip="Death")],
                             [(0, "notify", "m", "notify.FX_Fuse"), (800, "notify", "m", "notify.FX_Burst")],
                             [100, 300, 500, 700, 790], crop=(560, 480), focus="m")
    sc["notify.FX_Burst"] = S("notify.FX_Burst", [Fig("m", BLOAT, (0, 0), (-1, -0.5), clip="Death", t0=800.0)],
                              [(0, "notify", "m", "notify.FX_Burst")], [0, 50, 110, 200, 320, 500, 800, 1300],
                              crop=(900, 720), focus="m")
    sc["notify.FX_Land"] = S("notify.FX_Land", [Fig("m", CLOT, (0, 0), (-1, -0.5), clip="Walk", t0=510.0)],
                             [(0, "notify", "m", "notify.FX_Land")], [0, 80, 160, 280], crop=(420, 360), focus="m")
    sc["notify.FX_BellToll"] = S("notify.FX_BellToll", [Fig("m", HERD, (0, 0), (-1, -0.4), clip="Attack01",
                                                            t0=500.0)],
                                 [(0, "notify", "m", "notify.FX_BellToll")], [0, 60, 130, 220, 320],
                                 crop=(480, 420), focus="m")
    sc["notify.FX_bell_ring"] = S("notify.FX_bell_ring", [Fig("m", HERD, (0, 0), (-1, -0.4), clip="Cast01", t0=650.0),
                                                          Fig("d", FOOT, (-190, -60), "m", clip="Death", t0=890.0,
                                                              hold=True)],
                                  [(0, "notify", "m", "notify.FX_bell_ring")], [0, 120, 260, 420, 620, 900],
                                  crop=(900, 640), focus="m")
    sc["notify.FX_BellRing"] = S("notify.FX_BellRing", [Fig("m", GORGE, (0, 0), (-1, -0.2), clip="Attack02", t0=800.0),
                                                        Fig("h", HERO, (-230, -30), "m")],
                                 [(0, "notify", "m", "notify.FX_BellRing")], [0, 60, 140, 260, 420, 640],
                                 crop=(760, 560), focus=(-100, -10))
    sc["notify.FX_Roar"] = S("notify.FX_Roar", [Fig("m", VAL, (0, 0), (-1, -0.3), clip="Roar", t0=450.0)],
                             [(0, "notify", "m", "notify.FX_Roar")], [0, 100, 220, 380, 560, 750],
                             crop=(760, 720), focus="m")
    sc["notify.FX_Brand"] = S("notify.FX_Brand", [Fig("m", VAL, (0, 0), (-1, -0.3), clip="Cast01", t0=900.0),
                                                  Fig("h", HERO, (-260, -60), "m")],
                              [(0, "notify", "m", "notify.FX_Brand")], [0, 70, 160, 300, 500, 800, 1200],
                              crop=(820, 720), focus=(-80, -20))
    sc["notify.FX_DrumBeat_1"] = S("notify.FX_DrumBeat_1", [Fig("m", DRUM, (0, 0), (-1, -0.4), clip="Cast01",
                                                                t0=125.0)],
                                   [(0, "notify", "m", "notify.FX_DrumBeat_1"), (250, "notify", "m", "notify.FX_DrumBeat_2")],
                                   [0, 80, 180, 260, 340], crop=(420, 420), focus="m")
    sc["notify.FX_Dig"] = S("notify.FX_Dig", [Fig("m", FOOT, (0, 0), (-1, -0.4), clip="Idle_Order_Dig",
                                                  t0=notify_ms(FOOT, "Idle_Order_Dig", "FX_Dig"), look="shovel")],
                            [(0, "notify", "m", "notify.FX_Dig")], [0, 80, 180, 300], crop=(440, 420), focus="m",
                            note="the notify fires at its clip time; the preview starts the clip there")
    sc["notify.FX_BannerSnap"] = S("notify.FX_BannerSnap", [Fig("m", BEARER, (0, 0), (-1, -0.4), clip="Idle_Order",
                                                                t0=notify_ms(BEARER, "Idle_Order", "FX_BannerSnap"))],
                                   [(0, "notify", "m", "notify.FX_BannerSnap")], [0, 300, 700, 1200],
                                   crop=(480, 560), focus="m")
    # deaths (the kill hit lands from the hero; the victim plays its Death clip; the death recipe is anchored to the
    # victim: Blow = its facing — the "death" event kind)
    for fam, asset in (("unburied", FOOT), ("breach", CLOT), ("living", ZEAL), ("gravegnawers", GNAW)):
        sc[f"death.{fam}"] = S(f"death.{fam}", [Fig("h", HERO, (-200, -40), "v", clip="Attack01", t0=hit_ms),
                                                 Fig("v", asset, (0, 0), "h", clip="Death")],
                               [(0, "hit", ("h", "v"), "hit.impact.kill"), (0, "death", ("h", "v"), f"death.{fam}")],
                               [0, 80, 180, 320, 520, 800, 1200], crop=(640, 520), focus=(-60, -10))
    # Valdras is killed from his side (the hero below him on screen, he faces left): the plume must stay on the
    # standard on his back through the droop, whatever the blow (the old blow-frame offsets put it in front of him)
    sc["death.valdras"] = S("death.valdras", [Fig("h", HERO, (-40, -230), "v", clip="Attack01", t0=hit_ms),
                                              Fig("v", VAL, (0, 0), (-1, -0.3), clip="Death")],
                            [(0, "hit", ("h", "v"), "hit.impact.kill"), (0, "death", ("h", "v"), "death.valdras")],
                            [80, 320, 600, 900, 1100, 1300, 1600, 2000], crop=(760, 820), focus=(0, -60),
                            note="killed from his side: Blow (hero -> Valdras) is screen-up, he faces screen-left; "
                                 "the plume follows the standard on his back (actor frame)")
    # the cinderbloat kill chain (VFX r5): the quiet death.cinderbloat at the kill, the fuse under the crimson r2.2
    # ring (U2, normal style), FX_Burst + telegraph.contact at 800 — one fuse, one burst
    sc["death.cinderbloat"] = S("death.cinderbloat",
                                [Fig("h", HERO, (-170, -60), "v", clip="Attack01", t0=hit_ms),
                                 Fig("v", BLOAT, (0, 0), "h", clip="Death")],
                                [(0, "hit", ("h", "v"), "hit.impact.kill"),
                                 (0, "death", ("h", "v"), "death.cinderbloat"),
                                 (0, "notify", "v", "notify.FX_Fuse"), (800, "notify", "v", "notify.FX_Burst"),
                                 (800, "point", "v", "telegraph.contact")],
                                [80, 300, 600, 790, 810, 900, 1300], crop=(820, 640), focus=(-60, -20),
                                radius=220,
                                telegraph={"shape": "circle", "at": (0, 0), "radius": 220, "start": 0,
                                           "contact": 800, "style": "normal"},
                                note="the whole cinderbloat death: kill (quiet) -> fuse 800 ms under the r2.2 ring "
                                     "-> burst; replaces the telegraph.elite scene")
    # raises, block, loops
    sc["monster.raise.mark"] = S("monster.raise.mark", [Fig("d", FOOT, (0, 0), (-1, 0.2), clip="Death", t0=890.0,
                                                            hold=True),
                                                        Fig("r", HERD, (230, 90), "d", clip="Cast01", t0=650.0)],
                                 [(0, "point", "d", "monster.raise.mark"), (0, "notify", "r", "notify.FX_bell_ring")],
                                 [100, 300, 500, 700, 880], crop=(760, 560), focus=(100, 40))
    sc["monster.raise"] = S("monster.raise", [Fig("d", FOOT, (0, 0), (-1, -0.3), clip="Rise", t0=-900.0)],
                            [(0, "point", "d", "monster.raise.mark"), (900, "hit", ("d", "d"), "monster.raise")],
                            [300, 900, 1000, 1150, 1400, 1700, 2000], crop=(520, 520), focus="d")
    sc["monster.block"] = S("monster.block", [Fig("h", HERO, (-190, -30), "m", clip="Attack01", t0=hit_ms),
                                              Fig("m", SHIELD, (0, 0), "h", clip="Idle")],
                            [(0, "hit", ("h", "m"), "monster.block")], [0, 40, 90, 150, 230], crop=(560, 420),
                            focus=(-90, -10))
    sc["monster.dead_standard_bearer.aura"] = S("monster.dead_standard_bearer.aura",
                                                [Fig("m", BEARER, (0, 0), (-1, -0.4), clip="Idle"),
                                                 Fig("a", FOOT, (190, 130), (-1, -0.5), clip="Idle"),
                                                 Fig("b", FOOT, (-210, 110), (-1, -0.3), clip="Idle"),
                                                 Fig("h", HERO, (-330, -170), "m")],
                                                [(0, "notify", "m", "monster.dead_standard_bearer.aura")],
                                                [1800, 3000], crop=(1180, 760), focus=(-40, 0), tight=False,
                                                note="the +15 % aura (r 5 tiles) round the bearer and two buffed "
                                                     "footmen: cloth-dark crimson, no rim (DR5-aura)")
    sc["monster.ash_flakes"] = S("monster.ash_flakes", [Fig("m", BLOAT, (0, 0), (-1, -0.5), clip="Idle")],
                                 [(0, "self", "m", "monster.ash_flakes")], [1500, 2500, 3500], crop=(420, 420),
                                 focus="m")
    sc["monster.valdras.standard_fire"] = S("monster.valdras.standard_fire",
                                            [Fig("m", VAL, (0, 0), (-1, -0.3), clip="Idle")],
                                            [(0, "notify", "m", "monster.valdras.standard_fire")],
                                            [800, 1400, 2000, 2600], crop=(640, 760), focus="m")
    sc["monster.valdras.feed"] = S("monster.valdras.feed",
                                   [Fig("m", VAL, (0, 0), (-1, -0.3), clip="Idle"),
                                    Fig("d", FOOT, (-260, -90), "m", clip="Death", t0=890.0, hold=True)],
                                   [(0, "path", ("d", "m"), "monster.valdras.feed")], [0, 120, 260, 420, 600],
                                   crop=(860, 720), focus=(-130, -30))
    # telegraphs (the material emulated) + the contact recipes
    sc["telegraph.contact"] = S("telegraph.contact", [Fig("m", GORGE, (60, 120), "h", clip="Attack02", t0=0.0),
                                                     Fig("h", HERO, (-60, -40), "m")],
                               [(800, "point", (-60, -40), "telegraph.contact"),
                                (800, "notify", "m", "notify.FX_BellRing")],
                               [200, 400, 600, 760, 810, 900, 1000], crop=(760, 600), focus=(0, 30), radius=200,
                               telegraph={"shape": "circle", "at": (-60, -40), "radius": 200, "start": 0,
                                          "contact": 800, "style": "normal"})
    sc["telegraph.contact.cone"] = S("telegraph.contact.cone", [Fig("m", VAL, (120, 60), "h", clip="Attack02", t0=0.0),
                                                        Fig("h", HERO, (-90, -40), "m")],
                             [(700, "path", ("m", "h"), "telegraph.contact.cone")],
                             [150, 350, 550, 690, 720, 820], crop=(860, 680), focus=(20, 20), radius=260,
                             telegraph={"shape": "cone", "at": (120, 60), "dir": (-1.0, -0.47), "radius": 260,
                                        "arc": 120, "start": 0, "contact": 700, "style": "boss"})
    sc["telegraph.contact.line"] = S("telegraph.contact.line", [Fig("m", VAL, (260, 60), "h", clip="Idle"),
                                                        Fig("h", HERO, (-160, -40), "m")],
                             [(800, "path", ("m", "h"), "telegraph.contact.line")],
                             [200, 500, 780, 810, 900], crop=(980, 560), focus=(50, 10),
                             telegraph={"shape": "line", "at": (260, 60), "dir": (-1.0, -0.24), "length": 500,
                                        "width": 120, "start": 0, "contact": 800, "style": "boss"})
    sc["hazard.breach_fire"] = S("hazard.breach_fire", [Fig("h", HERO, (-200, -40), (1, 0.2))],
                               [(0, "point", (40, 20), "hazard.breach_fire")], [600, 1400, 2200, 3000],
                               crop=(700, 520), focus=(-40, 0), radius=180,
                               telegraph={"shape": "circle", "at": (40, 20), "radius": 180, "start": -1000,
                                          "contact": 9000, "style": "hazard"},
                               note="the Oath-Brand brand: hazard style rim (U2) + hazard.breach_fire inside")
    sc["hazard.breach_seep"] = S("hazard.breach_seep", [Fig("h", HERO, (-120, 140), (1, -0.2))],
                                 [(0, "path", ((300, -60), (-300, 80)), "hazard.breach_seep")],
                                 [150, 400, 700, 1000, 1300], crop=(980, 520), focus=(0, 20),
                                 telegraph=[{"shape": "line", "at": (300, -60), "dir": (-600, 140), "length": 616,
                                             "width": 100, "start": -700, "contact": 0, "style": "boss"},
                                            {"shape": "line", "at": (300, -60), "dir": (-600, 140), "length": 616,
                                             "width": 100, "start": 0, "end": 1250, "style": "hazard"}],
                                 note="the windup (boss style) hands over to the hazard-style crimson line at "
                                      "contact, on through both ticks (TELEGRAPH styles.hazard.window)")
    sc["hazard.banner_ring"] = S("hazard.banner_ring", [Fig("m", VAL, (0, 0), (-1, -0.3), clip="Idle"),
                                                        Fig("h", HERO, (-240, -40), "m")],
                                 [(0, "point", (0, 0), "hazard.banner_ring")], [1500, 2500, 3500],
                                 crop=(920, 660), focus=(0, 0), radius=320, tight=False,
                                 telegraph={"shape": "circle", "at": (0, 0), "radius": 320, "start": -1000,
                                            "end": 9000, "style": "arena"},
                                 note="the closing banner ring mid-shrink at r 3.2 m with the arena-style decal "
                                      "(crimson rim, hatch outward, a faint crimson wash outside: the side that "
                                      "hurts); U2 re-plays both as the radius shrinks")
    # corpses (U3): the ash dissolve emulated on the body + the recipe
    sc["corpse.crumble"] = S("corpse.crumble", [Fig("d", FOOT, (0, 0), (-1, -0.3), clip="Death", t0=890.0, hold=True)],
                             [(0, "point", "d", "corpse.crumble")], [0, 250, 500, 800, 1100, 1500],
                             crop=(520, 420), focus="d", ash={"fig": "d", "grey": -380.0})
    sc["corpse.expire"] = S("corpse.expire", [Fig("d", FOOT, (0, 0), (-1, -0.3), clip="Death", t0=890.0, hold=True)],
                            [(0, "point", "d", "corpse.expire")], [0, 200, 400, 700, 1000],
                            crop=(560, 420), focus="d", ash={"fig": "d", "grey": -5000.0, "expire": 0.0})
    sc["corpse.burn"] = S("corpse.burn", [Fig("d", FOOT, (0, 0), (-1, -0.3), clip="Death", t0=890.0, hold=True)],
                          [(0, "point", "d", "corpse.burn")], [100, 300, 600, 1000, 1500],
                          crop=(520, 440), focus="d", color="#FF6600", ash={"fig": "d", "grey": 120.0},
                          note="burned by the hero's fire (event colour #FF6600)")
    # Wave 2 (2H): the gnawer plays its real Consume clip (loop, bites at 330 / 830). The footman's husk (manifest
    # corpse.husk: centre (29, 24) forward / right of its root, the body ~1.5 m long) lies here facing (-1, -0.3): its
    # centre is at (-34, 14) on screen, the legs toward screen-right; the gnawer stands past the legs' end, 95 cm
    # from the centre along the body, facing it (screen-left: a profile, its grey clear of the grey husk), so the jaws
    # (~45 cm ahead, 6-25 cm up) work into the husk's legs
    sc["corpse.consume"] = S("corpse.consume", [Fig("d", FOOT, (0, 0), (-1, -0.3), clip="Death", t0=890.0, hold=True),
                                                Fig("g", GNAW, (57, 41), (-0.958, -0.287), clip="Consume", t0=0.0)],
                             [(0, "path", ("g", "d"), "corpse.consume")], [330, 900, 1500], crop=(640, 460),
                             focus=(10, 25), ash={"fig": "d", "grey": -5000.0},
                             note="Consume: the gnawer at the husk's legs tearing at it (its Consume clip, bites at "
                                  "330 / 830; the 2 s recipe once at the channel start)")
    # Wave 2 (2H): the zealot's frenzy, the Corpseherd's Keen, Ashnumb on the hero
    sc["notify.FX_Frenzy"] = S("notify.FX_Frenzy", [Fig("m", ZEAL, (0, 0), (-1, -0.4), clip="Roar", t0=480.0)],
                               [(0, "notify", "m", "notify.FX_Frenzy"), (0, "notify", "m", "monster.oath_zealot.frenzy")],
                               [0, 90, 200, 360, 560], crop=(560, 600), focus="m",
                               note="Roar's chant (FX_Frenzy @480) + the frenzy loop starting with it")
    sc["monster.oath_zealot.frenzy"] = S("monster.oath_zealot.frenzy",
                                         [Fig("m", ZEAL, (0, 0), (-1, -0.4), clip="Idle"),
                                          Fig("h", HERO, (-260, -60), "m")],
                                         [(0, "notify", "m", "monster.oath_zealot.frenzy")], [1600, 2400, 3200],
                                         crop=(720, 520), focus=(-90, -10),
                                         note="the frenzied zealot (loop): panting breath, dust at his feet; no light")
    sc["notify.FX_Keen"] = S("notify.FX_Keen", [Fig("m", HERD, (0, 0), (-1, -0.4), clip="Cast02", t0=0.0),
                                                Fig("h", HERO, (-230, -80), "m")],
                             [(800, "notify", "m", "notify.FX_Keen"), (800, "point", "m", "telegraph.contact")],
                             [400, 790, 880, 1020, 1300], crop=(1000, 760), focus=(-20, -10), radius=400,
                             telegraph={"shape": "circle", "at": (0, 0), "radius": 400, "start": 0, "contact": 800,
                                        "style": "elite"},
                             note="the Keen (Cast02): the crimson r4 ring (U2, elite style: she is an elite mini-boss) "
                                  "fills over the 800 ms windup; at contact the lament's violet ripple sweeps to its "
                                  "rim in 220 ms and flares there")
    sc["status.ashnumb"] = S("status.ashnumb", [Fig("h", HERO, (0, 0), (1, -0.4)),
                                                Fig("c", CLOT, (170, 40), "h", clip="Idle")],
                             [(0, "self", "h", "status.ashnumb")], [1200, 2000, 2800], crop=(560, 520),
                             focus=(40, 10),
                             note="Ashnumb on the hero (status loop): ash flaking off the limbs, dust at the feet; the "
                                  "per-stack grey band is the AshnumbTint contract (not emulated)")
    # world
    sc["prop.echo"] = S("prop.echo", [Fig("e", "SK_NPC_EscortTravelingMerchant_March", (0, 0), (-0.6, -1.0),
                                          clip="Death", t0=22 * 1000.0 / 60.0, hold=True, visible=False, bare=True)],
                        [(0, "self", "e", "prop.echo")], [200, 700, 1500, 2400, 2700, 2950],
                        crop=(520, 520), focus="e", ghost={"fig": "e"},
                        note="the Seventh Man (lore_rf_echo_01): the escort's Death kneel as A10's echo figure")
    sc["camp.bell_toll"] = S("camp.bell_toll", [], [(0, "prop", 0, "camp.bell_toll")], [100, 400, 800, 1300, 1800],
                             crop=(900, 760), focus=(0, 60), props=[("SM_Prop_March_BellFrame", (0, 0), -135.0)])
    sc["camp.torch@march"] = S("camp.torch@march", [], [(0, "prop", 0, "camp.torch@march")], [1400, 2600],
                               crop=(420, 520), focus=(0, 40), props=[("SM_Prop_March_Brazier", (0, 0), 0.0)],
                               note="the march brazier's smoulder (camp_torch@march): smoke, dull sparks, ash; no light")
    for k, v in sc.items():
        v.rid = k
    return sc


GROUPS = {
    "ambient": lambda k: k.startswith("ambient."),
    "elite": lambda k: k.startswith(("elite.", "teleport.")),
    "notify": lambda k: k.startswith("notify."),
    "death": lambda k: k.startswith("death."),
    "monster": lambda k: k.startswith("monster."),
    "telegraph": lambda k: k.startswith("telegraph."),
    "hazard": lambda k: k.startswith("hazard."),
    "corpse": lambda k: k.startswith("corpse."),
    "world": lambda k: k.startswith(("prop.", "camp.")),
    "status": lambda k: k.startswith("status."),
    # Wave 2 (2H): the scenes of the recipes the wave added or re-timed
    "wave2": lambda k: k in ("notify.FX_Frenzy", "monster.oath_zealot.frenzy", "notify.FX_Keen", "status.ashnumb",
                             "corpse.consume"),
}


MEASURE = [   # (asset, clip, ms, body sockets, (item, item sockets))
    (HERD, "Cast01", 650.0, ["fx_bell", "fx_hand_r", "fx_head"], None),
    (HERD, "Attack01", 500.0, ["fx_bell", "fx_hand_r"], None),
    (VAL, "Roar", 450.0, ["banner", "standard_top", "standard_root", "fx_head"], None),
    (VAL, "Roar", 700.0, ["banner", "standard_top", "standard_root", "fx_head"], None),
    *[(VAL, "Death", t, ["banner", "standard_top", "standard_root", "fx_chest", "fx_head"], None)
      for t in (0.0, 300.0, 600.0, 900.0, 1100.0, 1300.0, 1450.0, 1600.0, 2000.0)],
    (VAL, "Idle", 0.0, ["banner", "standard_top", "standard_root", "fx_chest"], None),
    (BLOAT, "Death", 0.0, ["fx_head", "fx_chest", "fx_vent"], None),
    (BLOAT, "Death", 400.0, ["fx_head", "fx_chest", "fx_vent"], None),
    (BLOAT, "Death", 790.0, ["fx_head", "fx_chest", "fx_vent"], None),
    (GNAW, "Attack01", 0.0, ["fx_head", "fx_hand_r", "fx_chest"], None),
    (GNAW, "Attack01", 300.0, ["fx_head", "fx_hand_r", "fx_chest"], None),
    (BEARER, "Idle", 0.0, ["fx_head", "fx_chest", "fx_hand_r"], ("SM_MonWeapon_StandardPole",
                                                                 ["finial", "crossbar_l", "crossbar_r", "mid"])),
    (VAL, "Cast01", 900.0, ["fx_hand_r"], ("SM_MonWeapon_OathSword", ["tip", "mid", "guard"])),
    (GORGE, "Attack02", 800.0, ["fx_hand_r"], ("SM_MonWeapon_BellClapper", ["tip", "mid"])),
    (DRUM, "Cast01", 125.0, ["fx_chest", "fx_drum", "fx_hand_r"], None),
    (FOOT, "Death", 890.0, ["fx_chest", "fx_head", "fx_feet", "fx_hand_r"], None),
    (BEARER, "Idle_Order", 2050.0, ["fx_head", "fx_hand_r"], ("SM_MonWeapon_StandardPole",
                                                             ["finial", "crossbar_l", "crossbar_r", "mid"])),
    (FOOT, "Idle_Order_Dig", 760.0, ["fx_hand_r"], ("SM_MonWeapon_Shovel", ["blade_tip", "blade_mid"]), "shovel"),
    (SHIELD, "Idle", 0.0, ["fx_chest", "fx_hand_l"], None),
    # Wave 2 (2H): the clips the wave added
    (GNAW, "Consume", 0.0, ["fx_head", "fx_hand_l", "fx_chest"], None),
    (GNAW, "Consume", 330.0, ["fx_head", "fx_hand_l", "fx_chest"], None),
    (ZEAL, "Roar", 480.0, ["fx_head", "fx_chest", "fx_hand_l", "fx_feet"], None),
    (ZEAL, "Idle", 0.0, ["fx_head", "fx_chest"], None),
    (HERD, "Cast02", 800.0, ["fx_head", "fx_hand_l", "fx_bell", "fx_chest"], None),
    (HERD, "Rise", 0.0, ["fx_head", "fx_chest"], None),
]


def measure() -> None:
    """Socket positions in the actor frame (forward, right, up cm) at the notify beats — the recipes' offsets."""
    stage = DarkStage()
    only = [a for a in sys.argv[1:] if not a.startswith("--")]
    for asset, clip, ms, socks, item, *look in MEASURE:
        if only and not any(o in asset for o in only):
            continue
        ch = stage.figure(asset, look=look[0] if look else None)
        stage.place(ch, np.zeros(3), np.array([1.0, 0.0, 0.0]))
        stage.set_clip(ch, clip, ms)
        bpy.context.scene.frame_set(int(F0))
        bpy.context.view_layer.update()
        sk = stage.sockets(ch)
        fwd, right = np.array([1.0, 0.0]), np.array([0.0, 1.0])
        row = []
        for n in socks:
            if n in sk:
                v = sk[n]
                row.append(f"{n}=({v[:2] @ fwd:.0f}, {v[:2] @ right:.0f}, {v[2]:.0f})")
        if item:
            name, isocks = item
            obj = next((m for m in ch["meshes"] if name in m.name), None)
            ie = FP.manifest().get(name, {})
            for sname in isocks:
                so = next((x for x in ie.get("sockets", []) if x["name"] == sname), None)
                if obj is not None and so is not None:
                    v = ue_cm(FP.item_socket_world(obj, so))
                    row.append(f"{name}.{sname}=({v[:2] @ fwd:.0f}, {v[:2] @ right:.0f}, {v[2]:.0f})")
        nts = {n["name"]: n["ms"] for n in FP.clip_info(asset, clip).get("notifies", [])}
        print(f"{asset} {clip}@{ms:.0f} {nts}: " + "  ".join(row))
        for m in ch["meshes"]:
            m.hide_render = True


def write_index(sc: dict, out_dir: Path) -> Path:
    """``README_dark.md`` in the preview root: every A12 binding → its preview directory (named after the recipe id)
    and the other scenes that also play it; every scene → what it plays (recipes, telegraph styles)."""
    plays: dict[str, set] = {}
    for k, v in sc.items():
        ids = {rid for _, kind, _, rid in v.events if kind != "stop"}
        plays[k] = ids
    tel = {k: sorted({t.get("style", "normal") for t in (v.telegraph if isinstance(v.telegraph, list) else
                                                           [v.telegraph])}) for k, v in sc.items() if v.telegraph}
    rows = ["# Dark-remake VFX previews (A12) — index", "",
            "Generated by `vfx/fx_preview_dark.py --index` (also after every render run). Each scene directory is "
            "named after the recipe it validates; the table lists every `dark.bindings` id of `vfx_recipes.json`.",
            "", "| Recipe id | Preview directory | Also played in | Bound to |", "|---|---|---|---|"]
    missing = []
    for rid, (event, wave) in sorted(DK.BINDINGS.items()):
        d = rid if rid in sc else None
        also = sorted(k for k, ids in plays.items() if rid in ids and k != rid)
        if d is None and not also:
            missing.append(rid)
        here = f"[`{d}/`]({d}/)" if d and (out_dir / d).is_dir() else (f"`{d}/` (not rendered)" if d else "—")
        rows.append(f"| `{rid}` | {here} | {', '.join(f'`{a}`' for a in also) or '—'} | {event} ({wave}) |")
    rows += ["", "| Scene | Plays | Telegraph styles (M_AF_Telegraph emulated) |", "|---|---|---|"]
    for k in sorted(sc):
        rows.append(f"| `{k}` | {', '.join(f'`{r}`' for r in sorted(plays[k]))} | {', '.join(tel.get(k, [])) or '—'} |")
    rows += ["", f"Bindings without a scene: {', '.join(missing) or 'none'}.", ""]
    if (out_dir / "AshnumbTint").is_dir():
        rows += ["Material contract mocks (not recipe scenes): [`AshnumbTint/`](AshnumbTint/) — "
                 "`dark.materials.AshnumbTint` on the three heroes at 0 / 1 / 3 / 5 stacks (`vfx/ashnumb_band.py`).", ""]
    path = out_dir / "README_dark.md"
    path.write_text("\n".join(rows))
    return path


def main(argv) -> int:
    sel = [a for a in argv if not a.startswith("--")]
    if "--measure" in argv:
        measure()
        return 0
    sc = scenarios()
    if "--list" in argv:
        print("\n".join(sorted(sc)))
        return 0
    if "--index" in argv:
        print(write_index(sc, OUT_DIR))
        return 0
    out_dir = K.SCRATCH / "effects_dark" if "--scratch" in argv else OUT_DIR
    recipes = FP.load_recipes()
    for name, e in FP.manifest().items():
        if name.startswith(("SM_FX_", "SM_Pickup_GoldCoin")):
            d = e.get("boundsDiameterCm")
            if d is None:
                lo, hi = e["boundsCm"]["min"], e["boundsCm"]["max"]
                d = math.dist(lo, hi)
            FP.MESH_D[name] = float(d)
    keys = []
    for a in sel or list(GROUPS):
        if a in GROUPS:
            keys += [k for k in sc if GROUPS[a](k)]
        elif a in sc:
            keys.append(a)
        else:
            print(f"unknown scenario {a}")
    keys = list(dict.fromkeys(keys))
    stage = DarkStage()
    fails = []
    for k in keys:
        try:
            run(stage, recipes, sc[k], out_dir)
        except pngio.NativePreviewError as ex:
            print(f"  FAIL {k}: {ex}")
            fails.append(k)
    if out_dir == OUT_DIR:
        print(f"  index: {write_index(sc, OUT_DIR)}")
    if fails:
        print(f"native preview budget failures: {fails}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
