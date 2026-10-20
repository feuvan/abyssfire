"""Re-import check of the shipped march environment (dark remake A10): every ``theme: march`` static mesh of the
environment families is re-imported from its FBX (what UE receives) and compared with its manifest entry (a themed
skeletal prop — the strongbox — by :func:`check_skeletal`).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/verify_march.py
        [--families=environment.foliage,environment.rocks,environment.camp,environment.story]   (exit 1 on a failure)

Per asset: the FBX holds one mesh; its material slots are the entry's (count and the toon slot's name: Interchange
names the UE slot after the FBX material); triangles per slot match ``triangles``; ``UVMap`` (palette UVs, every UV in
a swatch centre), ``AF_Data`` and — for wind assets — ``UVWind`` survive; the bounds match ``boundsCm`` (±0.5 cm, UE
axes); every manifest socket has its ``SOCKET_<name>`` empty at ``locCm`` (±0.5 cm); the LOD1 FBX re-imports with its
recorded triangles; each themed game id resolves to an asset that lists it and has a base id (``kit.theme.audit``);
footprint and blocking equal the plains base slot's (W5: the theme changes the mesh, never the slot); a plain
``setpiece_*`` id (``setpiece_bell_frame``, ``setpiece_hearth_dressing``: the UE set-piece layer places it; every plain
id of the set-piece family, e.g. ``decor_defend_boneyard_gate``) only has to be mapped to the asset; **normal sanity**
(art review r1 of the set pieces): every LOD fails with more than 2 % of its toon loops > 60° off their face normal
(:func:`normal_sanity`; families with authored normals — spherised foliage, faceted rocks — and the assets in
:data:`AUTHORED_NORMALS_ASSETS` gate only their LOD1, against LOD0's fraction + 2 points). Writes
``Art/Previews/env_family_march/reimport_check.json``.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import bpy  # noqa: E402
import numpy as np  # noqa: E402

from kit import paths, scene, theme  # noqa: E402
from kit.palette import GRID  # noqa: E402

FAMILIES = ("environment.foliage", "environment.rocks", "environment.camp", "environment.story",
            "environment.setpieces_march", "environment.lore_march")


def _import(fbx: Path) -> list:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(fbx))
    return [o for o in bpy.data.objects if o not in before]


def _tris(me, slot: int) -> int:
    n = 0
    for p in me.polygons:
        if p.material_index == slot:
            n += len(p.vertices) - 2
    return n


def _bounds_ue(obj) -> tuple[list, list]:
    co = np.array([obj.matrix_world @ v.co for v in obj.data.vertices])
    lo, hi = co.min(0), co.max(0)
    a = scene.blender_to_ue_cm(lo)
    b = scene.blender_to_ue_cm(hi)
    return [min(a[i], b[i]) for i in range(3)], [max(a[i], b[i]) for i in range(3)]


NORMAL_DOT_MIN = 0.5            # a toon loop normal > 60° off its face normal is bad …
NORMAL_BAD_MAX = 0.02           # … and an LOD fails with more than 2 % bad toon loops (art review r1 of the set pieces)
# families whose shading normals are authored on purpose (spherised foliage clumps, faceted / spherised rocks): their
# LOD0 is reported, not gated; their LOD1 must stay within LOD0's fraction + NORMAL_BAD_MAX
AUTHORED_NORMALS = ("environment.foliage", "environment.rocks")
# single assets of the other families carrying such parts (measured per part on the built mesh)
AUTHORED_NORMALS_ASSETS = {
    "SM_Prop_March_PenitentsAltar": "its grass tuft (foliage blades, normals bent up): 34 of the 38 bad loops, LOD0 2.2 %",
}


def normal_sanity(me, dot_min: float = NORMAL_DOT_MIN, bad_max: float = NORMAL_BAD_MAX) -> dict:
    """Toon-slot (material 0) loops of mesh ``me`` whose shading normal deviates from their face normal by more than
    acos(``dot_min``) (``bad``) or points behind it (``flipped``). Art review r1 of the set pieces: whole-mesh
    collapse decimation dropped the sharp-edge flags of the flat ground decals, so their LOD1 smoothed each 4 mm
    plate's top into its rim — 27-34 % of the toon loops > 60° off, the decals rendering as boulders — and the
    re-import check (triangles, UVs, bounds, sockets) passed them."""
    nl = len(me.loops)
    if not nl:
        return {"badFrac": 0.0, "flipped": 0, "loops": 0, "pass": True}
    ln = np.zeros(nl * 3, np.float32)
    me.corner_normals.foreach_get("vector", ln)
    ln = ln.reshape(-1, 3)
    npoly = len(me.polygons)
    fn = np.zeros(npoly * 3, np.float32)
    me.polygons.foreach_get("normal", fn)
    fn = fn.reshape(-1, 3)
    mi = np.zeros(npoly, np.int32)
    me.polygons.foreach_get("material_index", mi)
    lt = np.zeros(npoly, np.int32)
    me.polygons.foreach_get("loop_total", lt)
    lf = np.repeat(np.arange(npoly), lt)
    sel = mi[lf] == 0
    d = (ln[sel] * fn[lf[sel]]).sum(1)
    frac = float((d < dot_min).mean()) if len(d) else 0.0
    return {"badFrac": round(frac, 4), "flipped": int((d < 0.0).sum()), "loops": int(len(d)),
            "pass": frac <= bad_max}


def check_skeletal(name: str, e: dict) -> list[str]:
    """A themed skeletal prop (the strongbox, ``environment/lore_march.py``): one mesh skinned to one armature with
    the manifest's bone count, its triangles per slot, palette UVs and AF_Data; every clip FBX re-imports as an
    armature-only take whose frame range spans ``frames``. Bone sockets are created in UE from the manifest."""
    errs = []
    root = paths.export_root()
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    new = _import(root / e["fbx"])
    meshes = [o for o in new if o.type == "MESH"]
    arms = [o for o in new if o.type == "ARMATURE"]
    if len(meshes) != 1 or len(arms) != 1:
        return [f"{len(meshes)} meshes / {len(arms)} armatures in the FBX (expected 1 / 1)"]
    me, arm = meshes[0].data, arms[0]
    if len(arm.data.bones) != e.get("bones"):
        errs.append(f"{len(arm.data.bones)} bones, manifest {e.get('bones')}")
    t = e["triangles"]
    if _tris(me, 0) != t["toon"] or _tris(me, 1) != t["outline"]:
        errs.append(f"triangles toon {_tris(me, 0)} / outline {_tris(me, 1)} vs manifest {t['toon']} / {t['outline']}")
    if "UVMap" not in me.uv_layers:
        errs.append("no UVMap")
    if "AF_Data" not in me.color_attributes:
        errs.append("no AF_Data colour attribute")
    for a in e.get("anims", []):
        for o in list(bpy.data.objects):
            bpy.data.objects.remove(o, do_unlink=True)
        got = _import(root / a["fbx"])
        arm2 = next((o for o in got if o.type == "ARMATURE"), None)
        act = arm2.animation_data.action if arm2 is not None and arm2.animation_data else None
        if act is None:
            errs.append(f"clip {a['name']}: no action in {a['fbx']}")
            continue
        n = int(round(act.frame_range[1] - act.frame_range[0])) + 1
        if abs(n - int(a["frames"])) > 1:
            errs.append(f"clip {a['name']}: {n} frames, manifest {a['frames']}")
    return errs


def check_asset(name: str, e: dict, man: dict) -> list[str]:
    if e.get("kind") == "SkeletalMesh":
        return check_skeletal(name, e)
    errs = []
    root = paths.export_root()
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    new = _import(root / e["fbx"])
    meshes = [o for o in new if o.type == "MESH"]
    if len(meshes) != 1:
        return [f"{len(meshes)} meshes in the FBX (expected 1)"]
    m = meshes[0]
    me = m.data
    slots = e["materialSlots"]
    if len(me.materials) != len(slots):
        errs.append(f"{len(me.materials)} material slots, manifest {len(slots)}")
    elif me.materials[0] is None or not me.materials[0].name.startswith(slots[0]["name"]):
        errs.append(f"toon slot material {me.materials[0].name if me.materials[0] else None!r} != {slots[0]['name']!r}")
    t = e["triangles"]
    if _tris(me, 0) != t["toon"] or (len(slots) > 1 and _tris(me, 1) != t["outline"]):
        errs.append(f"triangles toon {_tris(me, 0)} / outline {_tris(me, 1)} vs manifest {t['toon']} / {t['outline']}")
    if "UVMap" not in me.uv_layers:
        errs.append("no UVMap")
    else:
        uv = np.zeros(len(me.loops) * 2, np.float32)
        me.uv_layers["UVMap"].data.foreach_get("uv", uv)
        cell = (uv.reshape(-1, 2) * GRID) % 1.0
        off = np.abs(cell - 0.5).max()
        if off > 0.02:
            errs.append(f"palette UVs off their swatch centres by {off:.3f} of a swatch")
    if "AF_Data" not in me.color_attributes:
        errs.append("no AF_Data colour attribute")
    if "wind" in e and "UVWind" not in me.uv_layers:
        errs.append("wind entry without the UVWind channel")
    authored = e.get("family") in AUTHORED_NORMALS or name in AUTHORED_NORMALS_ASSETS
    ns0 = normal_sanity(me)
    if not ns0["pass"] and not authored:
        errs.append(f"LOD0 normals: {ns0['badFrac'] * 100:.1f} % of the toon loops > 60° off their face "
                    f"({ns0['flipped']} flipped; gate {NORMAL_BAD_MAX * 100:.0f} %)")
    lo, hi = _bounds_ue(m)
    b = e["boundsCm"]
    d = max(max(abs(lo[i] - b["min"][i]), abs(hi[i] - b["max"][i])) for i in range(3))
    if d > 0.5:
        errs.append(f"bounds differ from boundsCm by {d:.2f} cm")
    empties = {o.name.split(".")[0]: o for o in new if o.type == "EMPTY"}
    for s in e.get("sockets", []):
        o = empties.get(f"SOCKET_{s['name']}")
        if o is None:
            errs.append(f"socket {s['name']} missing")
            continue
        loc = scene.blender_to_ue_cm(o.matrix_world.translation)
        dd = max(abs(loc[i] - s["locCm"][i]) for i in range(3))
        if dd > 0.5:
            errs.append(f"socket {s['name']} at {[round(x, 1) for x in loc]} vs {s['locCm']}")
    for lod in e.get("lods", []):
        fbx = root / lod["fbx"]
        if not fbx.exists():
            errs.append(f"LOD1 {lod['fbx']} missing")
            continue
        n2 = _import(fbx)
        lm = [o for o in n2 if o.type == "MESH"]
        if len(lm) != 1:
            errs.append(f"LOD1: {len(lm)} meshes")
        else:
            lt = lod["triangles"]
            if _tris(lm[0].data, 0) != lt["toon"] or _tris(lm[0].data, 1) != lt["outline"]:
                errs.append(f"LOD1 triangles {_tris(lm[0].data, 0)}/{_tris(lm[0].data, 1)} vs {lt['toon']}/{lt['outline']}")
            ns1 = normal_sanity(lm[0].data)
            cap = NORMAL_BAD_MAX + (ns0["badFrac"] if authored else 0.0)
            if ns1["badFrac"] > cap:
                errs.append(f"LOD1 normals: {ns1['badFrac'] * 100:.1f} % of the toon loops > 60° off their face "
                            f"({ns1['flipped']} flipped; gate {cap * 100:.1f} %)")
    gids = man.get("gameIds", {})
    for gid in e.get("gameIds", []):
        base, th = theme.split(gid)
        listed = sorted(n for n, x in man["assets"].items() if gid in x.get("gameIds", []))
        if th is None and (gid.startswith("setpiece_") or e.get("family") == "environment.setpieces_march"):
            # a plain set-piece id (the UE set-piece layer places it: setpiece_bell_frame, setpiece_hearth_dressing,
            # the set pieces' setpiece_* / lore_order_post / M5's decor_defend_boneyard_gate)
            if name not in listed or gid not in gids:
                errs.append(f"{gid} not mapped")
            continue
        if th != "march":
            errs.append(f"game id {gid} is not a march id")
            continue
        if name not in listed or gid not in gids:
            errs.append(f"{gid} not mapped")
        base_assets = sorted(n for n, x in man["assets"].items() if base in x.get("gameIds", []))
        if not base_assets:
            errs.append(f"{gid}: no plains asset lists the base id {base}")
            continue
        bx = man["assets"][base_assets[0]]
        if bx.get("footprintTiles") != e.get("footprintTiles") or bx.get("blocking") != e.get("blocking"):
            errs.append(f"{gid}: footprint {e.get('footprintTiles')} / blocking {e.get('blocking')} differ from the "
                        f"plains slot ({base_assets[0]}: {bx.get('footprintTiles')} / {bx.get('blocking')})")
    return errs


def main(argv: list[str]) -> int:
    fams = FAMILIES
    for a in argv:
        if a.startswith("--families="):
            fams = tuple(a.split("=", 1)[1].split(","))
    scene.reset()
    man = json.loads(paths.manifest_path().read_text())
    names = sorted(n for n, e in man["assets"].items() if e.get("theme") == "march" and e.get("family") in fams)
    report = {"assets": {}, "themeAudit": [p for p in theme.audit(man)
                                           if any(p.startswith(g) for n in names
                                                  for g in man["assets"][n].get("gameIds", []))]}
    fails = 0
    for n in names:
        errs = check_asset(n, man["assets"][n], man)
        report["assets"][n] = {"pass": not errs, "errors": errs}
        fails += bool(errs)
        print(f"  {'PASS' if not errs else 'FAIL'} {n}" + ("" if not errs else ": " + "; ".join(errs)))
    for p in report["themeAudit"]:
        print(f"  theme audit: {p}")
    report["summary"] = {"assets": len(names), "failures": fails, "themeAuditProblems": len(report["themeAudit"])}
    out = paths.preview_root() / "env_family_march" / "reimport_check.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=1, ensure_ascii=False) + "\n")
    print(f"[verify] {len(names)} march environment assets re-imported, {fails} failures, "
          f"{len(report['themeAudit'])} theme-audit problems -> {out}")
    return 1 if fails or report["themeAudit"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
