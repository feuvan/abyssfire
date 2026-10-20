"""The Living beside the Unburied (art review r1, item 1) — ``Art/Previews/mon_family_march/living_unburied_lineup.png``.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/lineup_living.py
        [--ground-hex=#3A332D] [--theme=march]   (lineup_march's grounds; default the march mud stress colour)
        [--no-family]                            skip re-shooting lineup_march's ``living`` page

One page at the W1 game camera (``lineup_march.shoot``: the shipped FBX re-imported by ``cohkit``, Idle F0, se, native
1080p, ink-gated per figure): warrior · risen footman · risen shieldbearer | oath zealot · deserter · drummer, each
labelled with its **head share** — the head's screen height over the figure's at the W1 camera (vertices weighted
≥ .5 to ``head`` plus rigid head attachments such as helms; held items left out). The r0 Living read as chibi beside
the Unburied (deserter 31 %, drummer 29 %, zealot 30 % against the footman's 22.5 %); r1 rebuilds them on the Unburied
body (``living.LivingDims``). ``living_unburied_lineup.json`` keeps the numbers.

It then re-shoots ``lineup_march``'s ``living`` page from the current exports and updates only that page in
``lineup_march.json`` (the other pages belong to their families' ships). The file names do not match
``lineup_march_*.png``, so a full ``lineup_march.py`` run keeps this page. Exit 1 on an ink failure.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "coherence", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import cohkit as K  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from bpy_extras.object_utils import world_to_camera_view  # noqa: E402
from mathutils import Vector  # noqa: E402

import darkgate as DG  # noqa: E402
import lineup_march as LM  # noqa: E402
from kit import paths, review  # noqa: E402

TAG = "living_unburied"
OUT_NAME = "living_unburied_lineup"
PAGE = [("SK_Hero_Warrior", "WARRIOR"), ("SK_Mon_RisenFootman", "FOOTMAN"),
        ("SK_Mon_RisenShieldbearer", "SHIELDBEARER"), ("SK_Mon_OathZealot", "ZEALOT"),
        ("SK_Mon_Deserter", "DESERTER"), ("SK_Mon_DeserterDrummer", "DRUMMER")]
OUTLINE_SLOT = 1                                 # material slot 1 is the ink hull (M_AF_Outline)


def _points(obj, head_bones=("head",), thr=0.5):
    """World-space toon vertices of ``obj`` (evaluated: posed) and the mask of those weighted ≥ ``thr`` to
    ``head_bones``."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    n = len(me.vertices)
    co = np.zeros(n * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    mw = np.array(obj.matrix_world)
    co = (np.c_[co, np.ones(n)] @ mw.T)[:, :3]
    keep = np.ones(n, bool)
    if len(me.polygons):
        mi = np.zeros(len(me.polygons), np.int32)
        me.polygons.foreach_get("material_index", mi)
        keep = np.zeros(n, bool)
        for f in me.polygons:
            if mi[f.index] != OUTLINE_SLOT:
                keep[list(f.vertices)] = True
    head = np.zeros(n, bool)
    gidx = {g.index for g in obj.vertex_groups if g.name in head_bones}
    if gidx:
        for v in obj.data.vertices:
            head[v.index] = sum(g.weight for g in v.groups if g.group in gidx) >= thr
    ev.to_mesh_clear()
    return co[keep], head[keep]


def head_share(asset: str) -> dict:
    """Head screen height / figure screen height of ``asset`` alone at the W1 camera (Idle F0, se)."""
    K.reset()
    review.setup_render(LM.RES, 4)
    cam = review.game_camera((0.0, 0.0))
    f = K.import_character(asset, (0.0, 0.0, 0.0), review.FACINGS["se"], cam=cam)
    bpy.context.view_layer.update()
    allp, headp = [], []
    for o in f.meshes[:f.n_body]:
        co, hd = _points(o)
        allp.append(co)
        headp.append(co[hd])
    for o in f.meshes[f.n_body:]:
        if getattr(o, "parent_bone", "") == "head":     # helms, hoods, the drummer's rag: part of the head's read
            co, _ = _points(o, head_bones=())
            allp.append(co)
            headp.append(co)
    sc = bpy.context.scene

    def proj(co):
        return np.array([tuple(world_to_camera_view(sc, cam, Vector(p)))[:2] for p in co[::2]])
    A, H = proj(np.concatenate(allp)), proj(np.concatenate(headp))
    fig = float(A[:, 1].max() - A[:, 1].min()) * LM.RES[1]
    hd = float(H[:, 1].max() - H[:, 1].min()) * LM.RES[1]
    return {"headPx": round(hd, 1), "figurePx": round(fig, 1), "headShare": round(hd / fig, 4)}


def main(argv: list[str]) -> int:
    hexc = next((a.split("=", 1)[1] for a in argv if a.startswith("--ground-hex=")), None)
    theme = next((a.split("=", 1)[1] for a in argv if a.startswith("--theme=")), None)
    if hexc is None and theme is None:
        hexc = "#3A332D"
    ground = DG.Ground(theme, "darkest", hexc)
    out = paths.preview_dir(LM.OUT_DIR)
    out.mkdir(parents=True, exist_ok=True)

    shares = {a: head_share(a) for a, _ in PAGE}
    for a, s in shares.items():
        print(f"  [head] {a:28s} {s['headShare'] * 100:5.1f} %  ({s['headPx']:.0f} of {s['figurePx']:.0f} px)")
    page = [(a, f"{lab} H{shares[a]['headShare'] * 100:.0f}%", None) for a, lab in PAGE]

    write0 = DG.write

    def write(path, img, head):                         # the header names the head-share label
        return write0(path, img, head.replace(f"MARCH LINEUP  {TAG.upper()}", "LIVING VS UNBURIED  H = HEAD SHARE"))
    DG.write = write
    try:
        recs = LM.shoot(TAG, page, ground, out)
    finally:
        DG.write = write0
    for src in out.glob(f"lineup_march_{TAG}*.png"):      # the page + its ink map, each maybe split in tiles (_2 …)
        src.replace(out / src.name.replace(f"lineup_march_{TAG}", OUT_NAME, 1))
    for r in recs:
        r.update(shares[r["asset"]])
    ok = all(r["ok"] for r in recs)
    (out / f"{OUT_NAME}.json").write_text(json.dumps(
        {"tool": "Art/blender/monsters/lineup_living.py", "ground": {"key": ground.run_key, **ground.stats},
         "headShare": "head screen height / figure screen height at the W1 camera, Idle F0, se (head-weighted "
                      "toon vertices + rigid head attachments; held items left out)",
         "figures": recs}, indent=1, ensure_ascii=False))
    print(f"[lineup_living] {'PASS' if ok else 'FAIL'} -> {out / (OUT_NAME + '.png')}")

    if "--no-family" not in argv:
        living = next(p for t, p in LM.PAGES if t == "living")
        frecs = LM.shoot("living", living, ground, out)
        jp = out / "lineup_march.json"
        doc = json.loads(jp.read_text()) if jp.is_file() else {"ground": {"key": ground.run_key, **ground.stats}}
        doc.setdefault("pages", {})["living"] = frecs
        jp.write_text(json.dumps(doc, indent=1, ensure_ascii=False))
        fok = all(r["ok"] for r in frecs)
        print(f"[lineup_march living] {'PASS' if fok else 'FAIL'} -> {out / 'lineup_march_living.png'}")
        ok = ok and fok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]))
