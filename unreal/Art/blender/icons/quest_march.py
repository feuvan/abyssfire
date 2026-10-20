"""Quest-item icons of the march (dark remake 焚境 · The Burning March, package A11): ``T_UI_QuestItem_ichor``,
``T_UI_QuestItem_moss``, ``T_UI_QuestItem_tags`` — the inventory / quest-card icons of chapter1.md S1-S3
(``AbyssUiContext::QuestItemIcon``: ``T_UI_QuestItem_<itemKind>``; the quest log draws them at 20 px, the HUD tracker
at 16 px, the inventory at 36-48 px).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/quest_march.py   # ship (~1 min)
        --only ichor,moss,tags     scratch sheets only (iteration), nothing shipped

Rendered like the Chapter-1 pendant (``glyphs.py``) from the **shipped** ground meshes (``items/quest_march.py``:
``SM_Pickup_Quest_Ichor / _Moss / _Tags``; re-run this after they re-ship), turned for the icon camera, through the
icon kit's look (``iconkit.render_icon``: screen-space overlap lines + seams, ink #140C1C 2 u, rim, drop shadow, the
icon gate). Accents keep the reserved colours: the ichor's soft violet under-glow is its own breach sheen (the enemy's
colour on its blood), the moss's pale gather-hint glow, a cool silver glint on the tags — no ember, no gold (art review
r2: the first tag icon, pale-gold discs by a tan pouch, read as a coin purse beside the gold-pile icons).

Outputs: ``Art/Export/Icons/QuestItems/T_UI_QuestItem_<kind>.png`` (256², straight alpha), manifest
``ui.T_UI_QuestItem_<kind>`` (kind ``questItemIcon``, ``questItemKind``, ``source``, ``gate``, ``previews``), previews
``Art/Previews/icons_quest/quest_items_march_{1x,half,ui}.png``.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import bpy  # noqa: E402,F401
from mathutils import Vector  # noqa: E402

import iconkit as K  # noqa: E402
from kit import pngio  # noqa: E402

KINDS = {"ichor": "SM_Pickup_Quest_Ichor", "moss": "SM_Pickup_Quest_Moss", "tags": "SM_Pickup_Quest_Tags"}
SOURCE = {
    "ichor": {"mesh": "SM_Pickup_Quest_Ichor", "inWorld": "SM_Pickup_Quest_Ichor",
              "look": "Mother Marl's sample phial: black breach-blood, violet meniscus, linen wrap, cork, label",
              "accents": "violet under-glow (its own breach sheen, α .34), glass sparkle"},
    "moss": {"mesh": "SM_Pickup_Quest_Moss", "inWorld": "SM_Pickup_Quest_Moss (node: SM_Prop_Gather_Barrowmoss)",
             "look": "a ragged handful of barrowmoss bound at one end with twine, large pale spore caps, a fan of "
                     "torn roots",
             "accents": "pale gather-hint under-glow #C8D49A α .22"},
    "tags": {"mesh": "SM_Pickup_Quest_Tags", "inWorld": "SM_Pickup_Quest_Tags",
             "look": "Dreck's dark leather pouch, tarnished silver name-tags spilled on pale cords",
             "accents": "cool silver glint"},
}


def build(kind: str):
    """(objects, accents) of one icon, placed for the studio camera (viewer −Y, Z up)."""
    o = K.import_fbx("Pickups", KINDS[kind])
    if kind == "ichor":
        # upright, a little turned and tipped toward the viewer so the cork and the meniscus ring read
        K.place(o, toward=(0, -1, 0), up=(0, 0, 1), roll=-9, pitch=18, spin=-62)
        acc = [K.Accent("webglow", at=tuple(o.matrix_world @ Vector((0, 0, 0.10))), r_u=38, hex="#B02FD0", alpha=.34,
                        under=True),
               K.Accent("sparkle", u=(34, 52), r_u=5.0, hex="#FFFFFF", alpha=.95)]
    elif kind == "moss":
        # the bundle on the diagonal seen from above-front (art review r2: crown-on, the old roll read as stones on a
        # green melon and the roots were lost): the ragged head low-left, the twine-bound neck, the torn-root fan
        # opening up-right
        K.place(o, toward=(0, -0.55, 0.84), up=(0, 0, 1), roll=-32)
        acc = [K.Accent("webglow", at=tuple(o.matrix_world @ Vector((-0.03, 0, 0.06))), r_u=40, hex="#C8D49A",
                        alpha=.22, under=True),
               K.Accent("sparkle", u=(36, 44), r_u=4.0, hex="#F2F8D8", alpha=.9)]
    else:
        # seen from above-front: the pouch on the upper left, the stamped tags fanned down-right
        K.place(o, toward=(0, 0, 1), up=(-1, 0.25, 0), roll=-32, pitch=-36)
        acc = [K.Accent("sparkle", u=(66, 58), r_u=4.6, hex="#F2F6FF", alpha=.95)]
    return [o], acc


def main(argv: list[str]) -> int:
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    cam = K.studio()
    out = {}
    t0 = time.time()
    for kind in KINDS:
        if only and kind not in only:
            continue
        K.clear_objects()
        objs, acc = build(kind)
        K.apply_frame(cam, K.fit(objs, fill=.84))
        r = K.render_icon(accents=acc)
        out[f"T_UI_QuestItem_{kind}"] = (kind, r)
        print(f"[quest_march] {kind:6s} gate {'ok ' if r.gate['pass'] else 'FAIL'} span {r.gate['span']:.2f} "
              f"{time.time() - t0:4.0f}s" + ("" if r.gate["pass"] else f" {r.gate}"))
    cells = [(k.replace("T_UI_QuestItem_", ""), v[1].rgba) for k, v in out.items()]
    if only:
        pngio.write_png(K.SCRATCH / "quest_march_1x.png", K.atlas(cells, cols=3))
        pngio.write_png(K.SCRATCH / "quest_march_ui.png", K.ui_sheet(cells, cols=3, sizes=(48, 36),
                                                                     qualities=("normal",)))
        print(f"[quest_march] scratch sheets in {K.SCRATCH}")
        return 0
    return ship(out, cells)


def ship(out: dict, cells) -> int:
    files = {}
    for name, (kind, r) in out.items():
        rel = f"Icons/QuestItems/{name}.png"
        K.write_icon(rel, r.rgba)
        files[name] = rel
    names = list(out)
    previews: dict[str, list[str]] = {n: [] for n in names}
    px = cells[0][1].shape[0]
    jobs = [("quest_items_march_1x.png", K.atlas(cells, cols=3), K.atlas_boxes(len(cells), px, 3)),
            ("quest_items_march_half.png", K.atlas(cells, cols=3, scale=.5), K.atlas_boxes(len(cells), px // 2, 3)),
            ("quest_items_march_ui.png", K.ui_sheet(cells, cols=3, sizes=(48, 36), qualities=("normal",)),
             K.ui_sheet_boxes(len(cells), 3, (48, 36)))]
    for file, img, boxes in jobs:
        for n, fl in zip(names, K.save_preview_cells("icons_quest", file, img, boxes)):
            previews[n].extend(fl)
    man = K.IconManifest()
    for name, (kind, r) in out.items():
        man.data["ui"][name] = K.ui_entry("questItemIcon", files[name], r.rgba.shape[0], gate=r.gate,
                                          previews=previews[name], questItemKind=kind, source=SOURCE[kind],
                                          remake="dark remake A11 (焚境 · The Burning March)")
    man.save()
    fails = [n for n, v in out.items() if not v[1].gate["pass"]]
    print(f"[quest_march] shipped {len(out)} icons; gate fails: {fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]))
