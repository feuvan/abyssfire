"""Ship + review helpers shared by the monster generators (bake, FBX, manifest, previews, ink gate)."""
from __future__ import annotations

import math
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

from kit import anim, export, outline, paths, pngio, review, rig as R, scene, shading

import common as C


def apply_bp(A, bp) -> None:
    anim.assign(A.rig, None)
    anim.apply_pose(A.solve(bp))


def quick_review(asset, rig, meshes, attachments, A, out: Path, height: float, outline_class: str,
                 blob_radius: float = 0.22, glows=None, pose=None) -> None:
    """Mesh iteration: READY pose close-up (se / front / back), turnaround and the 1080p game crop → ``out``."""
    out.mkdir(parents=True, exist_ok=True)
    for o, bone in attachments:
        R.attach_to_bone(o, rig, bone)
    apply_bp(A, pose if pose is not None else A.ready())
    objs = list(meshes) + [o for o, _ in attachments]
    rv = review.Review(asset, rig.obj, objs, height, blob_radius=blob_radius, out_dir=out,
                       game_outline_px=outline.SCREEN_PX_1080[outline_class], outline_class=outline_class,
                       glows=glows)
    rv.closeup("se")
    (out / "closeup.png").rename(out / "closeup_se.png")
    rv.closeup("front", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    rv.closeup("ne", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_ne.png")
    rv.turnaround()
    rv.game_view()
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")
