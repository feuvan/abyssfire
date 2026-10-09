#!/usr/bin/env python3
"""Writes the Finder window layout (.DS_Store) of a mounted read-write DMG without driving Finder.

    python3 dmg_style.py --volume /Volumes/Abyssfire [--app Abyssfire.app] [--background .background/background.tiff]

Needs the `ds_store` and `mac_alias` packages (pip, pure Python; the background alias must be created on macOS
against the mounted volume, the way dmgbuild does it). Called by unreal/CI/scripts/mac-dmg.sh, which falls back to a
Finder AppleScript (GUI session only) or an unstyled image when this fails.

Layout (matches make_installer_assets.py): 660x420 window, 128 px icons, the app at (170, 205), the Applications
link at (490, 205), no toolbar / sidebar / status bar, background picture from the volume.
"""
from __future__ import annotations

import argparse
import os
import sys


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--volume", required=True, help="mount point of the read-write image")
    ap.add_argument("--app", default="Abyssfire.app")
    ap.add_argument("--applications", default="Applications")
    ap.add_argument("--background", default=".background/background.tiff", help="path inside the volume")
    ap.add_argument("--window", default="660x420")
    ap.add_argument("--origin", default="200,120", help="window position on screen")
    ap.add_argument("--icon-size", type=int, default=128)
    ap.add_argument("--app-pos", default="170,205")
    ap.add_argument("--apps-pos", default="490,205")
    ap.add_argument("--no-alias", action="store_true", help="tests only: skip the background alias (non-macOS)")
    a = ap.parse_args()

    from ds_store import DSStore

    w, h = (int(v) for v in a.window.split("x"))
    ox, oy = (int(v) for v in a.origin.split(","))
    app_pos = tuple(int(v) for v in a.app_pos.split(","))
    apps_pos = tuple(int(v) for v in a.apps_pos.split(","))

    bg = os.path.join(a.volume, a.background)
    if not os.path.isfile(bg):
        print(f"dmg_style: background {bg} not found", file=sys.stderr)
        return 1

    icvp = {
        "viewOptionsVersion": 1,
        "backgroundType": 2,  # picture
        "backgroundColorRed": 1.0,
        "backgroundColorGreen": 1.0,
        "backgroundColorBlue": 1.0,
        "gridOffsetX": 0.0,
        "gridOffsetY": 0.0,
        "gridSpacing": 100.0,
        "arrangeBy": "none",
        "showIconPreview": False,
        "showItemInfo": False,
        "labelOnBottom": True,
        "textSize": 13.0,
        "iconSize": float(a.icon_size),
        "scrollPositionX": 0.0,
        "scrollPositionY": 0.0,
    }
    if not a.no_alias:
        from mac_alias import Alias

        icvp["backgroundImageAlias"] = Alias.for_file(bg).to_bytes()

    bwsp = {
        "ShowStatusBar": False,
        "WindowBounds": "{{%d, %d}, {%d, %d}}" % (ox, oy, w, h),
        "ContainerShowSidebar": False,
        "PreviewPaneVisibility": False,
        "SidebarWidth": 0,
        "ShowTabView": False,
        "ShowToolbar": False,
        "ShowPathbar": False,
        "ShowSidebar": False,
    }

    path = os.path.join(a.volume, ".DS_Store")
    if os.path.exists(path):
        os.remove(path)
    with DSStore.open(path, "w+") as d:
        d["."]["vSrn"] = ("long", 1)
        d["."]["bwsp"] = bwsp
        d["."]["icvp"] = icvp
        d["."]["icvl"] = ("type", b"icnv")
        d["."]["vstl"] = ("type", b"icnv")
        d[a.app]["Iloc"] = app_pos
        d[a.applications]["Iloc"] = apps_pos
    print(f"dmg_style: wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
