"""End-to-end helpers: finish a built mesh (skin, materials, hull) and ship an asset (FBX, manifest, previews).

Typical character generator::

    sc = kit.scene.reset()
    pal = kit.Palette("Heroes")
    rig = kit.build_humanoid(kit.HumanoidSpec(name="SK_Hero_Warrior").fit_height(1.76))
    b = kit.mesh.Builder("SK_Hero_Warrior", pal, prefix="warrior"); b.regions(...); b.add(...)
    body = asset.finish_mesh(b, pal, "hero", rig=rig, grounding_height=1.76)
    asset.ship_character("SK_Hero_Warrior", "Hero_Warrior", "Characters", rig, body, pal, clips, "hero",
                         game_ids=["player_warrior"], attachments=[(sword, "weapon_r")], blob_radius=0.39)
"""
from __future__ import annotations

from pathlib import Path
from typing import Sequence

import bpy

from . import anim, export, outline, review, rig as krig, shading
from .mesh import Builder


def finish_mesh(builder: Builder, palette, outline_class: str, rig: krig.Rig | None = None,
                grounding_height: float | None = None, name: str | None = None) -> bpy.types.Object:
    """Build → (skin) → toon material + baked outline hull (class width from ``outline.WIDTH_CM``)."""
    binds = builder.binds()
    obj = builder.build(name, grounding_height)
    if rig is not None:
        krig.skin(obj, rig, binds)
    mt = shading.toon_material(palette)
    width = outline.WIDTH_CM[outline_class] / 100.0
    if width > 0:
        mo = shading.outline_material(palette)
        outline.bake_hull(obj, width, mt, mo)
    else:
        obj.data.materials.clear()
        obj.data.materials.append(mt)
    obj["af_outline_class"] = outline_class
    return obj


def ship_character(asset: str, token: str, category: str, rig: krig.Rig, body: bpy.types.Object, palette,
                   clips: Sequence[anim.Clip], outline_class: str, game_ids: Sequence[str] = (),
                   attachments: Sequence[tuple[bpy.types.Object, str]] = (), blob_radius: float = 0.39,
                   previews: bool = True, preview_clips: Sequence[str] | None = None,
                   extra_manifest: dict | None = None) -> dict:
    """Bake clips, export SK + one A_ file per clip, update manifest.json, render the review set."""
    actions = [anim.bake(rig, c, token) for c in clips]
    sk = export.export_skeletal_mesh(rig, [body], asset, category)
    entries = []
    for c, a in zip(clips, actions):
        fbx = export.export_animation(rig, a, category)
        entries.append(export.clip_entry(c, a, fbx))
    man = export.Manifest()
    man.set_palette(palette)
    entry = export.skeletal_entry(rig, body, sk, category, palette, outline_class, entries, game_ids,
                                  blob_radius_cm=round(blob_radius * 100.0, 1), extra=extra_manifest)
    entry["attachments"] = [{"object": o.name, "socket": b} for o, b in attachments]
    result = {"fbx": str(sk), "anims": [e["fbx"] for e in entries], "previews": []}
    if previews:
        for o, b in attachments:
            krig.attach_to_bone(o, rig, b)
        rv = review.Review(asset, rig.obj, [body, *[o for o, _ in attachments]], rig.spec.crown_height(),
                           blob_radius=blob_radius)
        anim.assign(rig, None)
        anim.clear_pose(rig)
        sel = [(c, a) for c, a in zip(clips, actions) if preview_clips is None or c.name in preview_clips]
        if sel:   # stills in the first clip's first frame (the ready / idle pose), not the A-pose
            anim.assign(rig, sel[0][1])
            anim.set_frame(0)
        rv.game_view()
        rv.closeup()
        rv.turnaround()
        if sel:
            rv.contact_sheet(rig, sel)
        entry["previews"] = [str(Path(p).relative_to(rv.out.parent.parent)) for p, _ in rv.written]
        result["previews"] = rv.report()
    man.set_asset(asset, entry)
    man.save()
    result["manifest"] = str(man.path)
    return result


def ship_static(asset: str, category: str, obj: bpy.types.Object, palette, outline_class: str,
                game_ids: Sequence[str] = (), sockets: Sequence[dict] = (), footprint_tiles=None,
                blocking: bool | None = None, previews: bool = True, height: float | None = None,
                blob_radius: float = 0.3, extra_manifest: dict | None = None) -> dict:
    fbx = export.export_static_mesh(obj, asset, category, sockets)
    man = export.Manifest()
    man.set_palette(palette)
    entry = export.static_entry(obj, fbx, category, palette, outline_class, game_ids, sockets, footprint_tiles,
                                blocking, extra_manifest)
    result = {"fbx": str(fbx), "previews": []}
    if previews:
        lo, hi = obj.bound_box[0], obj.bound_box[6]
        rv = review.Review(asset, obj, [obj], height or (hi[2] - lo[2]), blob_radius=blob_radius)
        rv.game_view()
        rv.closeup()
        rv.turnaround()
        entry["previews"] = [str(Path(p).relative_to(rv.out.parent.parent)) for p, _ in rv.written]
        result["previews"] = rv.report()
    man.set_asset(asset, entry)
    man.save()
    result["manifest"] = str(man.path)
    return result
