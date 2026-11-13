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
    "pit": {
        "zones": ("sub_rf_pit",),                            # 信者之坑 / the Pit of the Faithful (chapter1.md §9.2)
        "parent": "march",                                   # the march underground: <id>@pit, <id>@march, <id>
        "palettes": {"Monsters": "Monsters_March", "Props": "Props_March", "Foliage": "Foliage_March",
                     "NPC": "NPC", "Heroes": "Heroes", "Pickups": "Pickups", "Weapons": "Weapons", "Pets": "Pets"},
        "note": "A13 / 3C: the crypt under the Red Field. Its own terrain style (terrain.pit), wall tile "
                "(outcrop_plains@pit) and stair (exit_portal@pit); everything else it shows is the march's, so its "
                "lookups fall back to the parent theme before the plain id (implementation-plan.md 3D notes)",
    },
    "hangwood": {
        "zones": ("twilight_forest",),                       # 绳林 / the Hanging Wood (chapter2.md §2, §15)
        "palettes": {"Monsters": "Monsters_Hangwood", "Props": "Props_Hangwood", "Foliage": "Foliage_Hangwood",
                     "NPC": "NPC", "Heroes": "Heroes", "Pickups": "Pickups", "Weapons": "Weapons", "Pets": "Pets"},
        "note": "Chapter 2 (4R-1): its own terrain style (terrain.hangwood: leaf mould, root ridges, wet stone, "
                "black pond water), monsters on Monsters_Hangwood (the Moon-Bitten on SKEL_Quadruped first). Art "
                "parent: the march (PARENTS_PENDING_DATA below; see its comment)",
    },
    "barrow": {
        "zones": ("sub_hw_barrow",),                         # 根冢 / the Root Barrow (chapter2.md §9.2)
        "palettes": {"Monsters": "Monsters_Hangwood", "Props": "Props_Hangwood", "Foliage": "Foliage_Hangwood",
                     "NPC": "NPC", "Heroes": "Heroes", "Pickups": "Pickups", "Weapons": "Weapons", "Pets": "Pets"},
        "note": "Chapter 2 (4R-3 ships its kit): the Hanging Wood's underground, on the crypt blocks with root "
                "overlays. Art parent: hangwood (PARENTS_PENDING_DATA below)",
    },
}
BASE_THEME = "plains"

# Theme parents the art ships along that are kept out of the THEMES rows. The exporter
# (Tools/export-data/src/themeParents.ts) asserts that the data's ``themeParents`` (GameData/world.ts
# REMAKE_OVERRIDES) equals the kit's table. A ``"parent"`` line written into a THEMES row before the data has the
# same row fails the data check of every lane (the reader at 4R-1's start knew only those lines), so Chapter 2's two
# parents wait here: the exporter that knows nothing of this table sees no kit parent for them (and no data row: it
# agrees), the Phase 4 exporter reads this table as part of the kit's chain (one line, ``{"theme": "parent", ...}``:
# keep that shape) and agrees with the data's rows. :func:`chain` walks both, so kit lookups already resolve
# hangwood -> march and barrow -> hangwood -> march (chapter2.md §15). HANDOFF (4G gate, once both lanes are in):
# move each entry into its THEMES row as a ``"parent"`` line and leave this table empty.
PARENTS_PENDING_DATA: dict[str, str] = {"hangwood": "march", "barrow": "hangwood"}


def parent_of(theme: str) -> str | None:
    """The theme's art parent: its THEMES row, else the row waiting for the data lane (PARENTS_PENDING_DATA)."""
    return THEMES[theme].get("parent") or PARENTS_PENDING_DATA.get(theme)


def chain(theme: str | None) -> list[str]:
    """The themes a zone's lookups try, nearest first: ``pit`` → ``["pit", "march"]``; the base theme / ``None`` →
    ``[]`` (plain ids only)."""
    out: list[str] = []
    while theme and theme != BASE_THEME and theme not in out:
        if theme not in THEMES:
            raise ValueError(f"unknown presentation theme {theme!r} (kit.theme.THEMES: {', '.join(THEMES)})")
        out.append(theme)
        theme = parent_of(theme)
    return out


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
    """The runtime lookup (U1): ``<id>@<theme>`` first (then its parent themes': :func:`chain`), then ``<id>``.
    ``game_ids`` = manifest ``gameIds``."""
    base, _ = split(game_id)
    for th in chain(theme):
        hit = game_ids.get(f"{base}{SEP}{th}")
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
