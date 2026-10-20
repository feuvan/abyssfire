"""Darkest-tile readability check of the Lanternwisp (A11, art review r2): ``coherence/darkgate.py`` on the shipped
``SK_Pet_Lanternwisp`` / ``_E1`` / ``_E2`` instead of the monster roster.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/pets/lanternwisp_darkgate.py
        [darkgate flags: --tile=darkest|grass|… · --ground-hex=#3A332D · --facing=se · --clip=Idle …]

The monsters' measurement (the shipped FBX re-imported and posed by its exported clip, the Pets palette atlas, the UE
ink width of its slot, the W1 camera, native 1080p): ``rim`` (dark ink ≥ 3 px on ≥ 90 % of the silhouette edge) and
``lit`` (lit-side ΔL* ≥ 15 against the ground) are what a companion must hold on the darkest march mud — the
Lanternwisp is the one warm thing that walks the Weeping Ditch with the hero, and its soot wisps (art review r2: a soot
root clearly darker than the ash ground) must still part from it by their ink. ``marker`` is a hostile family's accent
(monsters.md §1.2): a friendly pet carries none, so the column is **N/A** (``marker.na``) and every figure's verdict is
rim + lit, as for the march NPCs (``npcs/march_darkgate.py``). Output: ``Art/Previews/pet_lanternwisp/darkgate_*.png``
and ``darkgate_pet_report.json`` beside them (never the monsters' ``ch1_dark`` report).
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "coherence"))

import darkgate as DG  # noqa: E402
from kit import paths  # noqa: E402

OUT = paths.preview_root() / "pet_lanternwisp"
REPORT = OUT / "darkgate_pet_report.json"
STAGES = (("SK_Pet_Lanternwisp", "LANTERNWISP"), ("SK_Pet_Lanternwisp_E1", "LANTERNWISP E1"),
          ("SK_Pet_Lanternwisp_E2", "LANTERNWISP E2"))


def roster(only: list, variants: bool = True) -> list:
    man = DG.K.manifest()["assets"]
    return [(a, None, lbl) for a, lbl in STAGES if a in man and (not only or a in only)]


def _pet_marker(measure):
    """The marker column is N/A for a friendly pet: the verdict is rim + lit."""
    def f(st, mk, light, glow, spec, ground_l):
        rec = measure(st, mk, light, glow, spec, ground_l)
        rec["marker"] = {"ok": True, "na": True, "blockPx": 0, "pixels": 0,
                         "note": "friendly pet: no family marker (monsters.md 1.2) — not gated"}
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
    DG.measure = _pet_marker(DG.measure)
    DG.review.draw_text = _label_na(DG.review.draw_text)
    args = list(argv)
    if not any(a.startswith("--out=") for a in args):
        args.append(f"--out={OUT}")
    if not any(a.startswith("--report=") for a in args):
        args.append(f"--report={REPORT}")
    DG.main(args)
    doc = json.loads(REPORT.read_text())
    bad = 0
    for key, run in doc.get("runs", {}).items():
        ok = [f for f in run["figures"] if f["rim"]["ok"] and f["lit"]["ok"]]
        bad += len(run["figures"]) - len(ok)
        run["petVerdict"] = {"pass": len(ok) == len(run["figures"]), "rimAndLit": f"{len(ok)}/{len(run['figures'])}",
                             "note": "a friendly pet carries no family marker (monsters.md 1.2): the marker column is "
                                     "N/A (marker.na) and every figure's ok / PASS is rim + lit"}
        print(f"  {key}: rim + lit {len(ok)}/{len(run['figures'])}")
        for f in run["figures"]:
            print(f"    {f['label']:20s} rim {f['rim']['fracRim'] * 100:5.1f} %  lit dL* {f['lit'].get('deltaL', 0):5.1f}")
    REPORT.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
