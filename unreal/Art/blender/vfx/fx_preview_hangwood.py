"""Previews of the Chapter 2 VFX recipes (``fx_recipes_hangwood.py``) on the Hanging Wood's ground.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_preview_hangwood.py
        [scenario …] [--scratch] [--list]

The dark-remake preview machinery (``fx_preview_dark``: shipped FBX figures posed by their exported clips, the
recipes played as the runtime plays them, W1 camera, native 1080p) on ``DarkStage("hangwood")`` — root earth in leaf
mould, the darkest tile of the theme. Output: ``Art/Previews/vfx_effects_hangwood/<recipe>/``. 4R-3 appends its
scenes to :func:`scenarios`.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE, HERE.parent, HERE.parent / "coherence", HERE.parent / "environment"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import fx_preview as FP  # noqa: E402
import fx_preview_dark as FD  # noqa: E402
from kit import paths  # noqa: E402

HOUND = "SK_Mon_MoonHound"
OUT_NAME = "vfx_effects_hangwood"


def scenarios() -> dict:
    S, Fig = FD.DScene, FD.Fig
    sc = {}
    sc["notify.FX_PounceLeap"] = S("notify.FX_PounceLeap", [Fig("m", HOUND, (0, 0), (-1, -0.4), clip="Pounce", t0=380.0)],
                                   [(0, "notify", "m", "notify.FX_PounceLeap")], [0, 70, 150, 260], crop=(520, 400),
                                   focus="m", note="Pounce @380 (Leap): earth and leaves kicked back from the hinds")
    sc["notify.FX_PounceLand"] = S("notify.FX_PounceLand", [Fig("m", HOUND, (0, 0), (-1, -0.4), clip="Pounce", t0=650.0)],
                                   [(0, "notify", "m", "notify.FX_PounceLand")], [0, 70, 160, 300, 600],
                                   crop=(520, 400), focus="m",
                                   note="Pounce @650 (Contact): dust, leaf litter, clods and claw scores at the "
                                        "forehand (in game the core has dashed the actor to the line's end)")
    return sc


def main(argv) -> int:
    sc = scenarios()
    if "--list" in argv:
        print("\n".join(sorted(sc)))
        return 0
    sel = [a for a in argv if not a.startswith("--")]
    out = (paths.scratch_root("fx", OUT_NAME) if "--scratch" in argv else paths.preview_root() / OUT_NAME)
    recipes = FP.load_recipes()
    stage = FD.DarkStage("hangwood")
    for k in (sel or sorted(sc)):
        FD.run(stage, recipes, sc[k], out)
    return 0


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(args))
