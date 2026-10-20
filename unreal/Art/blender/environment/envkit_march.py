"""The march layer of the environment family (dark remake 焚境 · The Burning March, package A10;
Docs/dark-remake/implementation-plan.md §5, chapter1.md §2 / §12 / §15, story-bible.md tone pillars).

Every ``environment/*_march.py`` generator builds on :mod:`envkit` exactly like the plains family and ships through
:func:`ship` here, which adds what the theme changes and nothing else (``envkit.py`` and ``kit/`` are untouched):

* **palettes** — ``Props_March`` / ``Foliage_March`` (A1's colour script, ``march/march_palettes.py``). Generators
  take their regions from the registry by name (:func:`regions`: ``regions("Foliage_March", "deadoak")`` →
  ``{"bark": Region(...), …}`` = the swatches A1 registered), so a march mesh samples the very swatches the colour
  script defines; a region a generator adds on top (a darker line tone for a groove, a thin pole's ``o``) is appended
  to the registry (append-only) and listed in :data:`EXTRA_REGIONS` of its generator;
* **ink** — ``Props_March`` decor at 1.4 px like ``Props_Plains`` (``envkit.PX_OVERRIDE``, DECISIONS R14: the same
  thin poles, stakes and banner shafts); foliage decor 1.2 px, interactive 2.0 px (kit classes);
* **wind** — one cloth instance for the whole ``Props_March`` palette, :data:`PROP_WIND_SLOT` with the plains
  parameters (the content build rejects an instance with two parameter sets); ground cover on
  :data:`GROUND_SLOT`;
* **themed game ids** — the asset ships ``kit.theme.themed_ids(base, "march")`` (``tree@march`` …): the runtime
  resolves ``<id>@march`` before ``<id>`` for ``emerald_plains`` (U1), footprints and blocking stay the plains
  slot's (W5);
* **reserved light** (chapter1.md §15, DR5) — :func:`check_light` refuses a mesh that samples an ember emissive
  outside the refuges / interaction / the Abyssfire (``march_palettes.WARM_ALLOWED``) or a breach-violet glow outside
  the Breach Rim / Pit;
* **previews on the march ground** — :func:`use_march_stage` re-points the kit's review stage to the march's ash grass
  (``#4C4940``, ``Data/terrain_styles.json themes.march``, no green grid), so every ``game*.png`` / ``closeup`` /
  family sheet is judged on the ground it will stand on. Family sheets go to ``Previews/env_family_march/``.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Sequence

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parent
for _p in (BLENDER, HERE, BLENDER / "march"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import numpy as np  # noqa: E402

import envkit as E  # noqa: E402
import march_palettes as MP  # noqa: E402
from kit import paths, review, theme  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

THEME = "march"
PROPS = "Props_March"
FOLIAGE = "Foliage_March"
STYLE = json.loads((paths.UNREAL_ROOT / "Data" / "terrain_styles.json").read_text())["themes"]["march"]
GRASS_HEX = "#%06X" % STYLE["ground"]["0"]["base"]       # #4C4940 ash-grey dead grass
DIRT_HEX = "#%06X" % STYLE["ground"]["1"]["base"]        # #3A332D trampled mud (the darkest walkable tile)
STONE_HEX = "#%06X" % STYLE["ground"]["2"]["base"]
CAMP_HEX = "#%06X" % STYLE["ground"]["5"]["base"]

E.PX_OVERRIDE[(PROPS, "decor")] = 1.4
PROP_WIND_SLOT = "MI_AF_Toon_Props_March_Wind"
PROP_WIND_PARAMS = dict(E.PROP_WIND_PARAMS)
GROUND_SLOT = "MI_AF_Toon_Foliage_March_Ground"
GROUND_WIND = {"WindAmplitudeCm": 2.0, "WindHeightCm": 45.0, "WindSpeed": 1.6}     # = the plains ground cover
FAMILY_DIR = "env_family_march"


# ── palettes ────────────────────────────────────────────────────────────────────────────────────────────
def regions(family: str, *prefixes: str, **extra: Region) -> dict[str, Region]:
    """The registered march regions of ``family`` under ``prefixes`` keyed by their part after the prefix
    (``regions("Foliage_March", "deadoak")`` → ``{"bark": …, "split": …}``; with several prefixes the key keeps the
    prefix: ``"deadoak.bark"`` → ``"deadoak_bark"``), plus ``extra`` regions (appended to the registry)."""
    out: dict[str, Region] = {}
    rows = MP.family_regions(family)
    for pre in prefixes:
        for name, reg, _ in rows:
            if name.startswith(pre + "."):
                key = name[len(pre) + 1:] if len(prefixes) == 1 else name.replace(".", "_")
                out[key] = reg
    out.update(extra)
    return out


def region(family: str, name: str) -> Region:
    """One registered march region by its full name (``region("Props_March", "camp.iron")``)."""
    for n, reg, _ in MP.family_regions(family):
        if n == name:
            return reg
    raise KeyError(f"{family} has no march swatch {name!r} (march/march_palettes.py)")


def palette(family: str) -> Palette:
    return Palette(family)


# ── reserved light (chapter1.md §15, story-bible DR5) ──────────────────────────────────────────────────────
def _used_swatches(obj) -> list[int]:
    me = obj.data
    if "af_swatch" not in me.attributes:
        return []
    a = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_swatch"].data.foreach_get("value", a)
    return sorted(set(int(x) for x in a))


def check_light(obj, pal: Palette, allow_violet: bool = False) -> list[str]:
    """Emissive swatches ``obj`` samples that break the march's reserved light: ember (hue 15-35°, sat > .6) outside
    ``march_palettes.WARM_ALLOWED`` and breach-violet glows (within 30° of #B02FD0) outside the Breach Rim / Pit.
    Returns the problems (empty = clean)."""
    out = []
    for i in _used_swatches(obj):
        sw = pal.swatches[i] if i < len(pal.swatches) else None
        if sw is None or sw.get("e", 0.0) <= 0.0:
            continue
        names = sw["names"]
        hx = sw["hex"]
        if MP.is_ember(hx) and not any(n.startswith(p) for n in names for p in MP.WARM_ALLOWED):
            out.append(f"ember emissive {hx} ({', '.join(names)}) outside the refuges / interaction / Abyssfire")
        hue, s, _ = MP.hsv(hx)
        if (not allow_violet and s > 0.5 and abs(((hue - 288.0) + 180.0) % 360.0 - 180.0) < 30.0
                and not any(n.startswith(("dais.", "pit.")) for n in names)):
            out.append(f"breach-violet glow {hx} ({', '.join(names)}) outside the Breach Rim / Pit")
    return out


# ── review ground ───────────────────────────────────────────────────────────────────────────────────────
_KIT_STAGE = review.stage


def _march_stage(radius: float = 30.0, blob_radius: float = 0.39, ground_hex: str | None = None,
                 grid: bool = False):
    """``kit.review.stage`` on the march ground: ash grass, no grid (the plains grid is green)."""
    if ground_hex in (None, "#74A247"):
        ground_hex = GRASS_HEX
    return _KIT_STAGE(radius=radius, blob_radius=blob_radius, ground_hex=ground_hex, grid=grid)


def use_march_stage() -> None:
    """Every review stage of this process (``Review`` / ``EnvReview``, ``envkit.family_sheet``, ``quick``, ``sheet``)
    stands on the march ground. Process-local: plains generators run in their own processes."""
    review.stage = _march_stage


use_march_stage()


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def ship(obj, name: str, category: str, pal: Palette, cls: str, *, base_ids: Sequence[str], family: str,
         extra: dict | None = None, allow_violet: bool = False, plain_ids: Sequence[str] = (), **kw) -> dict:
    """``envkit.ship`` for a march asset: themed game ids (``<base>@march``), theme metadata, the reserved-light
    check (raises), previews on the march ground. ``plain_ids``: remake ids the asset also answers to (a set piece
    the UE set-piece layer places, e.g. ``setpiece_bell_frame``), after the themed ones."""
    problems = check_light(obj, pal, allow_violet)
    if problems:
        raise RuntimeError(f"{name}: " + "; ".join(problems))
    ex = {"displayName": name.split("_", 2)[-1], "family": family, "theme": THEME,
          "themeOf": list(base_ids),
          "themeNote": "presentation theme march (chapter1.md §15): the runtime resolves <id>@march before <id> for "
                       "emerald_plains (U1); same footprint and blocking as the plains slot (W5)"}
    ex.update(extra or {})
    ids = theme.themed_ids(base_ids, THEME) + [g for g in plain_ids if g not in base_ids]
    return E.ship(obj, name, category, pal, cls, game_ids=ids, extra=ex, **kw)


def family_sheet(objs, out_name: str, label: str, **kw) -> Path:
    """``envkit.family_sheet`` into ``Previews/env_family_march/`` on the march ground."""
    out = paths.preview_root() / FAMILY_DIR
    out.mkdir(parents=True, exist_ok=True)
    return E.family_sheet(objs, f"../{FAMILY_DIR}/{out_name}", label, **kw)


def scene_shot(meshes, focus, out: Path, label: str, ground_hex: str = GRASS_HEX, **kw) -> Path:
    return E.scene_shot(meshes, focus, out, label, ground_hex=ground_hex, **kw)
