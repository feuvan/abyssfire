"""Darkest-tile readability check of the march NPCs (A9): ``coherence/darkgate.py`` on the shipped
``SK_NPC_*_March`` / ``SK_NPC_KnellChaplain`` / ``SK_NPC_CrowTrader`` instead of the monster roster.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/march_darkgate.py
        [darkgate flags: --ground-hex=#3A332D · --tile=… · --facing=se · --clip=IdleNPC …]

The same measurement as the monsters' gate (the shipped FBX re-imported and posed, the NPC palette, the UE ink width,
the W1 camera, native 1080p): ``rim`` (dark ink ≥ 3 px on ≥ 90 % of the silhouette edge) and ``lit`` (lit-side ΔL* ≥
15 against the ground) are the readability numbers that matter for a friendly NPC. ``marker`` is a hostile family's
accent (monsters.md §1.2) — NPCs carry none, so the marker column is **N/A** for the class ``npc``: the cell prints
``MK N/A`` and the figure's verdict (``ok``, PASS / FAIL) is rim + lit only (``marker.na`` in the report).
Old Bern stands last on his page, in profile (as in the lineup): his 1.9 m barrow runs out to the right of the row
instead of over a neighbour. Output: ``Art/Previews/npc_family_march/darkgate_*.png`` and ``darkgate_npc_report.json``
beside them (never the monsters' ``ch1_dark`` report).
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "coherence"))
sys.path.insert(0, str(HERE))

import darkgate as DG  # noqa: E402
import march_looks as ML  # noqa: E402
from kit import paths, review  # noqa: E402

OUT = paths.preview_root() / "npc_family_march"
REPORT = OUT / "darkgate_npc_report.json"
WIDE = "SK_NPC_EscortTravelingMerchant_March"      # the barrow: last on its page, in profile


def roster(only: list, variants: bool = True) -> list:
    man = DG.K.manifest()["assets"]
    out = []
    for n in sorted(ML.NPCS, key=lambda n: n.asset == WIDE):
        if n.asset not in man or (only and n.asset not in only):
            continue
        out.append((n.asset, None, DG._label(n.display["en"])))
    return out


def _attach_to_socket_bone(attach):
    """The re-import attaches by bone name; a manifest socket that is not a bone (Bern's ``barrow``, on his bone
    ``barrow_01``) resolves through the NPC entries' socket lists."""
    socks = {}
    for e in DG.K.manifest()["assets"].values():
        for sk in e.get("sockets", []) or []:
            if isinstance(sk, dict) and sk.get("bone"):
                socks.setdefault(sk["name"], sk["bone"])

    def f(obj, rig, bone, *a, **kw):
        bones = rig.obj.data.bones
        if bone not in bones and socks.get(bone) in bones:
            bone = socks[bone]
        return attach(obj, rig, bone, *a, **kw)
    return f


def _profile_wide(imp):
    """Old Bern in profile (``review.FACINGS['side']``: facing screen right, the barrow running out of the row)."""
    def f(asset, at, yaw_deg, *a, **kw):
        if asset == WIDE:
            yaw_deg = review.FACINGS["side"]
        return imp(asset, at, yaw_deg, *a, **kw)
    return f


def _npc_marker(measure):
    """The marker column is N/A for a friendly NPC: the verdict is rim + lit."""
    def f(st, mk, light, glow, spec, ground_l):
        rec = measure(st, mk, light, glow, spec, ground_l)
        rec["marker"] = {"ok": True, "na": True, "blockPx": 0, "pixels": 0,
                         "note": "friendly NPC: no family marker (monsters.md 1.2) — not gated"}
        if rec.get("_marker") is not None:
            rec["_marker"] = rec["_marker"] & False         # nothing painted cyan on the gate map
        rec["ok"] = rec["rim"]["ok"] and rec["lit"]["ok"]
        rec["atEdge"] = [e for e in rec.get("atEdge", []) if e != "marker"]
        return rec
    return f


def _label_na(draw):
    def f(img, x, y, txt, *a, **kw):
        if isinstance(txt, str) and txt.startswith("MK ") and txt.endswith("PX"):
            txt = "MK N/A"
            kw["rgb"] = (170, 170, 170)
        return draw(img, x, y, txt, *a, **kw)
    return f


def main(argv: list) -> int:
    DG.roster = roster
    DG.K.krig.attach_to_bone = _attach_to_socket_bone(DG.K.krig.attach_to_bone)
    DG.K.import_character = _profile_wide(DG.K.import_character)
    DG.measure = _npc_marker(DG.measure)
    DG.review.draw_text = _label_na(DG.review.draw_text)
    args = list(argv)
    if not any(a.startswith("--clip=") for a in args):
        args.append("--clip=IdleNPC")
    if not any(a.startswith("--out=") for a in args):
        args.append(f"--out={OUT}")
    if not any(a.startswith("--report=") for a in args):
        args.append(f"--report={REPORT}")
    if not any(a.startswith("--per-page=") for a in args):
        args.append("--per-page=6")
    DG.main(args)
    doc = json.loads(REPORT.read_text())
    for key, run in doc.get("runs", {}).items():
        ok = [f for f in run["figures"] if f["rim"]["ok"] and f["lit"]["ok"]]
        run["npcVerdict"] = {"pass": len(ok) == len(run["figures"]), "rimAndLit": f"{len(ok)}/{len(run['figures'])}",
                             "note": "friendly NPCs carry no family marker (monsters.md 1.2): the marker column is "
                                     "N/A (marker.na) and every figure's ok / PASS is rim + lit"}
        print(f"  {key}: rim + lit {len(ok)}/{len(run['figures'])}")
        for f in run["figures"]:
            print(f"    {f['label']:28s} rim {f['rim']['fracRim'] * 100:5.1f} %  lit dL* {f['lit'].get('deltaL', 0):5.1f}")
    REPORT.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
