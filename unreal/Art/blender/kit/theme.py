"""Presentation themes and themed game ids (dark remake, implementation-plan.md U1 / A1).

A zone's **presentation theme** picks its art without touching any map-generator input: game ids, footprints and
blocking stay the same, the meshes change. In the art manifest a themed variant is the game id with the theme
appended after ``@``: ``tree@march`` → ``SM_Foliage_March_DeadOak_C`` beside ``tree`` → ``SM_Foliage_Plains_Oak_C``.
The runtime resolves ``<id>@<zone theme>`` first and falls back to ``<id>`` (U1), so a themed id never needs every
asset of the base set and a zone without a theme keeps today's look (``ember_tower`` stays plains).

Shipping a themed asset is the ordinary kit path: the entry lists its themed game ids and
``export.Manifest.set_asset`` maps each of them, e.g.::

    from kit import theme
    game_ids = theme.themed_ids(["tree"], "march")          # ["tree@march"]
    asset.ship_static(..., game_ids=game_ids)                  # or envkit.ship(..., game_ids=game_ids)

Ids that are new in the remake (``monster_risen_footman``, ``npc_knell_chaplain``) are plain ids: a theme suffix is
only for a slot that already has a base asset. Pure Python (no bpy): the content scripts and the checklist can load
this file by path.
"""
from __future__ import annotations

SEP = "@"

# theme → what it re-points. ``zones``: zones whose presentation theme it is (GameData maps ``presentationTheme``);
# ``palettes``: the palette family each asset family ships on under the theme (kit.palette.FAMILIES).
THEMES: dict[str, dict] = {
    "plains": {
        "zones": (),            # the default (the base ids): every zone without a presentationTheme; ember_tower
                                # keeps it on purpose (the warm homestead), the web-exported chapters 2-5 too
        "palettes": {"Monsters": "Monsters_Plains", "Props": "Props_Plains", "Foliage": "Foliage_Plains",
                     "NPC": "NPC", "Heroes": "Heroes", "Pickups": "Pickups", "Weapons": "Weapons", "Pets": "Pets"},
    },
    "march": {
        "zones": ("emerald_plains",),                        # 赤幡原 / The Red Field (chapter1.md §15)
        "palettes": {"Monsters": "Monsters_March", "Props": "Props_March", "Foliage": "Foliage_March",
                     "NPC": "NPC",                           # NPC swatches append to the shared registry
                     "Heroes": "Heroes", "Pickups": "Pickups", "Weapons": "Weapons", "Pets": "Pets"},
        "note": "the dark remake 焚境 · The Burning March, Chapter 1; heroes, loot, weapons and pets keep their "
                "families (ember orange is theirs: DR5)",
    },
}
BASE_THEME = "plains"


def themed(game_id: str, theme: str | None) -> str:
    """``tree`` + ``march`` → ``tree@march`` (the base id for the base theme / ``None``)."""
    base, _ = split(game_id)
    if not theme or theme == BASE_THEME:
        return base
    if theme not in THEMES:
        raise ValueError(f"unknown presentation theme {theme!r} (kit.theme.THEMES: {', '.join(THEMES)})")
    return f"{base}{SEP}{theme}"


def split(game_id: str) -> tuple[str, str | None]:
    """``tree@march`` → (``tree``, ``march``); ``tree`` → (``tree``, None)."""
    base, sep, th = game_id.partition(SEP)
    return base, (th if sep else None)


def themed_ids(base_ids, theme: str | None) -> list[str]:
    """The ids a themed asset ships with (order kept, duplicates dropped)."""
    out: list[str] = []
    for g in base_ids:
        t = themed(g, theme)
        if t not in out:
            out.append(t)
    return out


def resolve(game_ids: dict, game_id: str, theme: str | None):
    """The runtime lookup (U1): ``<id>@<theme>`` first, then ``<id>``. ``game_ids`` = manifest ``gameIds``."""
    base, _ = split(game_id)
    if theme and theme != BASE_THEME:
        hit = game_ids.get(f"{base}{SEP}{theme}")
        if hit:
            return hit
    return game_ids.get(base)


def family(kind: str, theme: str | None) -> str:
    """Palette family of an asset family under a theme: ``family("Props", "march")`` → ``Props_March``."""
    return THEMES[theme or BASE_THEME]["palettes"][kind]


def audit(manifest: dict) -> list[str]:
    """Problems with the themed ids of a manifest dict: unknown theme, a themed id without its base id, a themed id
    that points at a missing asset or at an asset whose entry does not list it."""
    out = []
    gids = manifest.get("gameIds", {}) or {}
    assets = manifest.get("assets", {}) or {}
    for gid, name in sorted(gids.items()):
        base, th = split(gid)
        if th is None:
            continue
        if th not in THEMES:
            out.append(f"{gid}: unknown theme {th!r}")
        if base not in gids:
            out.append(f"{gid}: no base id {base!r} (a theme re-points an existing slot; new remake ids are plain)")
        e = assets.get(name)
        if e is None:
            out.append(f"{gid}: asset {name} not in the manifest")
        elif gid not in (e.get("gameIds") or []):
            out.append(f"{gid}: {name}'s entry does not list it")
    return out
