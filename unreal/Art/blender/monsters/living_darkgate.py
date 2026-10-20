"""darkgate on the Living with the march crown (art review r1, item 4) — a wrapper round ``coherence/darkgate.py``.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/living_darkgate.py
        [darkgate options …]          default: SK_Mon_OathZealot SK_Mon_Deserter SK_Mon_DeserterDrummer
                                      --ground-hex=#3A332D (pass --theme=march --tile=dirt|grass for the terrain runs)

``cohkit.import_character`` stands every hunt variant's elite crown up as the plains ``SM_FX_EliteCrown`` (it names
that mesh; the manifest variant does not say which crown), so a march hunt (Dreck, Harke) is gated under the wrong
crown. The march runtime places ``SM_FX_EliteCrown_March`` (``elite_crown@march``, A8), so this wrapper swaps the
name for the run — the ground, camera, metrics and report are darkgate's own. The fallback itself belongs to the
coherence tooling (A0) / the elite marker (A8): it is flagged there, not patched (``cohkit`` / ``darkgate`` are
shared). Exit status = darkgate's.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "coherence", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import cohkit as K  # noqa: E402
import darkgate as DG  # noqa: E402

PLAINS_CROWN = "SM_FX_EliteCrown"
MARCH_CROWN = "SM_FX_EliteCrown_March"
LIVING = ["SK_Mon_OathZealot", "SK_Mon_Deserter", "SK_Mon_DeserterDrummer"]


def march_crown() -> None:
    """Route cohkit's hard-coded plains crown to the march one (module globals: ``import_character`` looks them up
    at call time)."""
    entry, imp = K.entry, K.import_static

    def _entry(name, *a, **k):
        return entry(MARCH_CROWN if name == PLAINS_CROWN else name, *a, **k)

    def _imp(name, *a, **k):
        return imp(MARCH_CROWN if name == PLAINS_CROWN else name, *a, **k)

    K.entry, K.import_static = _entry, _imp


def main(argv: list[str]) -> int:
    if not any(not a.startswith("--") for a in argv):
        argv = argv + LIVING
    if not any(a.startswith(("--ground-hex=", "--theme=")) for a in argv):
        argv = argv + ["--ground-hex=#3A332D"]
    march_crown()
    print(f"  [living_darkgate] hunt crowns: {MARCH_CROWN} (cohkit's {PLAINS_CROWN} fallback swapped for the run)")
    return DG.main(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]))
