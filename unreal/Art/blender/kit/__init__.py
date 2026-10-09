"""Abyssfire Blender art kit (bpy 5.0) — shared by every generator under ``unreal/Art/blender/``.

Modules:
    paths    repo / export / preview locations (env overrides for tests)
    scene    factory reset, units (1 BU = 1 m), deterministic seeds, object helpers, Blender→UE axes
    color    exact port of the web tone() (sRGB maths)
    palette  Region / Palette: swatch registry, palette atlases T_AF_Palette_<Family>_BC/_P, UV collapse
    shading  toon constants (key light, bands, rim, grounding) + Cycles preview materials = M_AF_Toon
    mesh     low-poly modelling helpers + Builder (regions, part binds, AF_Data grounding)
    rig      SK_Humanoid builder (UE mannequin names, weapon sockets, IK helpers), skin weights
    outline  inverted-hull baking (P7) + preview width control
    anim     Pose (FK/aim/IK), Clip (web easing), per-frame bake to actions, PoseLib, gait
    export   FBX (SK / A_ / SM) for UE 5.8 + manifest.json
    review   game-camera / close-up / turnaround / contact-sheet renders (≤ 200 KB PNG)
    asset    finish_mesh (skin + materials + hull), ship_character / ship_static (FBX + manifest + previews)
    pngio    exact PNG writer (+ quantising size budget) / reader

Run generators with the Blender Python module:
    /opt/venvs/blender/bin/python unreal/Art/blender/<script>.py
"""
from __future__ import annotations

import sys
from pathlib import Path

_here = Path(__file__).resolve().parent.parent
if str(_here) not in sys.path:
    sys.path.insert(0, str(_here))

from . import paths, scene, color, pngio, palette, shading, mesh, rig, outline, anim, export, review, asset  # noqa: E402
from .palette import Palette, Region  # noqa: E402
from .rig import Bind, HumanoidSpec, Rig, build_humanoid, frame  # noqa: E402
from .anim import Clip, Key, Pose, PoseLib  # noqa: E402

__all__ = [
    "paths", "scene", "color", "pngio", "palette", "shading", "mesh", "rig", "outline", "anim", "export",
    "review", "asset", "Palette", "Region", "Bind", "HumanoidSpec", "Rig", "build_humanoid", "frame", "Clip", "Key",
    "Pose", "PoseLib",
]
