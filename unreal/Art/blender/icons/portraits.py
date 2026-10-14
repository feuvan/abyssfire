"""Portrait coverage + verification (spec §9.5, quests-story §8.5–§8.6, DECISIONS R11).

The portraits themselves are rendered by their characters' generators (``review.ink_render`` at 512², Portrait
pose): heroes (``heroes/*_ship.py``), NPCs (``npcs/npc_ship.py``), the goblin chief (``monsters/goblin_chief.py``);
the story emblems by ``icons/glyphs.py``. This script

* lists every Chapter-1 speaker the UI can ask a portrait for — hero classes, every NPC id / sprite id of the
  zone's NPCs and quest / event NPCs (``Data/npcs.json``, ``quests.json``, ``random_events.json``), the story's
  ``{npc}`` / ``{monster}`` speakers (``Data/story.json``) — and resolves each exactly like
  ``FAbyssUiContext::Portrait`` / ``HeroPortrait`` (strip ``player_`` / ``npc_`` / ``monster_`` / ``beast_``, PascalCase);
* checks each resolved file: 512², straight-alpha transparent background, the head inside the medallion;
* where the runtime name differs from the shipped file (the stash keeper: NPC id ``stash`` → ``T_UI_Portrait_Stash``,
  art ``T_UI_Portrait_StashKeeper``) writes the alias texture (a byte copy) so the lookup does not fall back to the
  generic emblem;
* renders ``Previews/portraits/medallions_*.png`` — every portrait as the UI draws it: the 64 px conversation
  medallion (square brush in a circle), the story ``say`` medallion (masked to r 53), the 66 px class well and the
  170 px class card;
* records ``manifest.json → ui.T_UI_Portrait_*`` (kind ``portrait`` / ``portraitAlias``, owner asset, check).

Run:  EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/portraits.py
"""
from __future__ import annotations

import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import bpy  # noqa: E402,F401
import numpy as np  # noqa: E402

import iconkit as K  # noqa: E402
from kit import paths, pngio, review  # noqa: E402

DATA = paths.UNREAL_ROOT / "Data"
PORTRAITS = paths.export_root() / "Portraits"
CH1_ZONE = "emerald_plains"


def pascal(s: str) -> str:
    return "".join(p[:1].upper() + p[1:] for p in re.split(r"[_\- ]+", s) if p)


def lookup_name(art_id: str) -> str:
    """``FAbyssUiContext::Portrait`` (first try): strip the type prefix, PascalCase; a ``pet_<id>`` resolves like
    ``FAbyssUiContext::PetPortrait`` (``T_UI_Portrait_Pet_<Id>``)."""
    if art_id.startswith("pet_"):
        return f"T_UI_Portrait_Pet_{pascal(art_id[len('pet_'):])}"
    for pre in ("player_", "npc_", "monster_", "beast_"):
        if art_id.startswith(pre):
            art_id = art_id[len(pre):]
            break
    return f"T_UI_Portrait_{pascal(art_id)}"


def speakers() -> list[tuple[str, str]]:
    """(art id, why) of every Chapter-1 portrait request."""
    out = [("warrior", "hero class"), ("mage", "hero class"), ("rogue", "hero class")]
    npcs = json.loads((DATA / "npcs.json").read_text())["npcs"]
    zone = json.loads((DATA / "maps.json").read_text())["maps"][CH1_ZONE]
    ids = [n for camp in zone.get("camps", []) for n in camp.get("npcs", [])]
    ids += [f["npcId"] for f in zone.get("fieldNpcs", []) or []]
    for nid in dict.fromkeys(ids):
        out.append((npcs.get(nid, {}).get("spriteId") or nid, f"npc {nid}"))
    for qid in ("npc_escort_traveling_merchant",):
        out.append((qid, "escort quest NPC"))
    for eid in ("npc_rescue_lost_traveler", "wandering_merchant"):
        out.append((eid, "random-event NPC"))
    for pid in ("pet_sprite",):  # DECISIONS Q3: the Chapter-1 companion (pets panel + HUD pet frame)
        out.append((pid, "pet panel / HUD (PetPortrait)"))
    story = json.loads((DATA / "story.json").read_text())
    for cid, cs in story["cutscenes"].items():
        if not (cid.startswith("cs_ep_") or cid == "cs_boss_goblin_chief"):
            continue
        for npc in set(re.findall(r'"npc": "([a-z_]+)"', json.dumps(cs))):
            out.append((npcs.get(npc, {}).get("spriteId") or npc, f"story {cid}"))
        for mon in set(re.findall(r'"monster": "([a-z_]+)"', json.dumps(cs))):
            out.append((f"monster_{mon}", f"story {cid}"))
    seen, uniq = set(), []
    for a, why in out:
        if a not in seen:
            seen.add(a)
            uniq.append((a, why))
    return uniq


# the shipped art per lookup name (owner asset → its manifest ``portrait``)
def shipped() -> dict[str, tuple[str, str]]:
    man = K.manifest()
    out = {}
    for asset, e in man["assets"].items():
        p = e.get("portrait")
        if p:
            out[Path(p).stem] = (asset, p)
    return out


ALIASES = {"T_UI_Portrait_Stash": "T_UI_Portrait_StashKeeper"}
HERO = {"warrior": "T_UI_Portrait_Hero_Warrior", "mage": "T_UI_Portrait_Hero_Mage", "rogue": "T_UI_Portrait_Hero_Rogue"}


def check(img: np.ndarray) -> dict:
    h, w = img.shape[:2]
    a = img[:, :, 3].astype(np.float32) / 255.0
    corners = float(max(a[:8, :8].max(), a[:8, -8:].max()))
    ys, xs = np.nonzero(a > .5)
    top = int(ys.min()) if len(ys) else h
    yy, xx = np.mgrid[0:h, 0:w]
    outside = (np.hypot(xx - w / 2 + .5, yy - h / 2 + .5) > w / 2) & (a > .5)
    frac_out = float(outside.sum() / max((a > .5).sum(), 1))
    # the head: the opaque rows' centroid in the upper half sits inside the inner 70 % circle
    upper = (a > .5) & (yy < h * .55)
    cy, cx = (float(np.nonzero(upper)[0].mean()), float(np.nonzero(upper)[1].mean())) if upper.any() else (h / 2, w / 2)
    head_ok = np.hypot(cx - w / 2, cy - h / 2) < w * .35
    ok = (w == 512 and h == 512 and corners < .02 and head_ok)
    return {"pass": bool(ok), "px": [w, h], "cornerAlphaMax": round(corners, 3), "topMarginPx": top,
            "opaqueOutsideCircle": round(frac_out, 3), "headCentroidPx": [round(cx), round(cy)]}


def medallion_sheet(items: list[tuple[str, np.ndarray]]) -> np.ndarray:
    """Every portrait as the UI draws it: conversation (64: square brush in a circle), story say (106: masked
    circle r 53), class well (66) and class card (170)."""
    cell_w, cell_h = 64 + 112 + 72 + 176 + 40, 200
    img = np.zeros((len(items) * cell_h + 8, cell_w, 3), np.uint8)
    img[:] = K.SHEET_BG
    for i, (name, rgba) in enumerate(items):
        y0 = 8 + i * cell_h
        x = 8
        for kind, d in (("conv", 64), ("say", 106), ("well", 66), ("card", 170)):
            cy = y0 + 4 + 85
            cx = x + d // 2
            yy, xx = np.mgrid[0:d, 0:d].astype(np.float32) + .5
            r = d / 2
            circle = np.clip(r - np.hypot(xx - r, yy - r) + .5, 0, 1)
            bg = np.zeros((d, d, 3), np.float32) + np.array((12, 11, 14), np.float32)
            rim = np.clip(1 - np.abs(np.hypot(xx - r, yy - r) - (r - 1)) / 1.2, 0, 1)
            tile = img[cy - d // 2:cy - d // 2 + d, x:x + d].astype(np.float32)
            if kind != "card":
                tile = tile * (1 - circle[..., None]) + bg * circle[..., None]
            inner = d - (6 if kind in ("conv",) else 0)
            pr = K.resize(rgba, inner).astype(np.float32) / 255.0
            off = (d - inner) // 2
            pa = pr[..., 3:4]
            if kind == "say":
                pa = pa * circle[off:off + inner, off:off + inner, None]
            sub = tile[off:off + inner, off:off + inner]
            tile[off:off + inner, off:off + inner] = sub * (1 - pa) + pr[..., :3] * 255 * pa
            if kind != "card":
                gold = np.array((201, 164, 90), np.float32)
                tile = tile * (1 - rim[..., None] * .9) + gold * rim[..., None] * .9
            img[cy - d // 2:cy - d // 2 + d, x:x + d] = np.clip(tile, 0, 255).astype(np.uint8)
            x += d + 12
        # the label sits under the small medallions, inside the card's rows, so a split tile never orphans it
        review.draw_text(img, 8, y0 + 4 + 85 + 60, name.replace("T_UI_Portrait_", "").replace("T_UI_Emblem_", "emblem ")[:40],
                         rgb=(200, 196, 210))
    return img


def main() -> int:
    K.studio()
    have = shipped()
    rows = []
    for art, why in speakers():
        name = HERO.get(art) or lookup_name(art)
        rows.append((art, why, name))
    man = K.IconManifest()
    problems = []
    previews_items = []
    written_alias = []
    for art, why, name in rows:
        src_name = ALIASES.get(name, name)
        f = PORTRAITS / f"{src_name}.png"
        if not f.exists():
            problems.append(f"{art} ({why}): {name} missing")
            continue
        if src_name != name:
            dst = PORTRAITS / f"{name}.png"
            shutil.copyfile(f, dst)
            written_alias.append(name)
        img = pngio.read_png(f)
        chk = check(img)
        if not chk["pass"]:
            problems.append(f"{name}: {chk}")
        owner = have.get(src_name, (None, None))[0]
        entry = K.ui_entry("portrait" if src_name == name else "portraitAlias", f"Portraits/{name}.png", img.shape[0],
                           artIds=[art], speaker=why, owner=owner, check=chk)
        if src_name != name:
            entry["aliasOf"] = src_name
            entry["note"] = ("byte copy written by icons/portraits.py: the runtime asks for the NPC id's portrait "
                             f"({art} → {name}); re-run after the owner re-ships {src_name}")
        old = man.data["ui"].get(name, {})
        if old.get("artIds"):
            entry["artIds"] = sorted(set(old["artIds"]) | {art})
        man.data["ui"][name] = entry
        previews_items.append((src_name, img))     # an alias shows its source art (the stash keeper's bust)
    for e in ("T_UI_Emblem_Villain", "T_UI_Emblem_Generic"):
        f = PORTRAITS / f"{e}.png"
        if f.exists():
            previews_items.append((e, pngio.read_png(f)))
        else:
            problems.append(f"{e} missing (run icons/glyphs.py)")
    seen = set()
    uniq = []
    for n, im in previews_items:
        if n not in seen:
            seen.add(n)
            uniq.append((n, im))
    shown: dict[str, list[str]] = {}  # texture → the medallion sheet tile(s) its row lands in
    cell_h = 200
    for pi in range(0, len(uniq), 5):
        chunk = uniq[pi:pi + 5]
        sheet = medallion_sheet(chunk)
        ink = np.any(sheet != np.array(K.SHEET_BG, np.uint8), axis=2).any(axis=1)   # rows with any content
        boxes = []
        for i in range(len(chunk)):
            ys = np.nonzero(ink[8 + i * cell_h:8 + (i + 1) * cell_h])[0] + 8 + i * cell_h
            boxes.append((int(ys.min()), int(ys.max()) + 1, 0, sheet.shape[1]))
        for (n, _), fs in zip(chunk, K.save_preview_cells("portraits", f"medallions_{pi // 5 + 1}.png", sheet, boxes)):
            shown[n] = fs
    for name in man.data["ui"]:
        if name.startswith("T_UI_Portrait_"):
            src = man.data["ui"][name].get("aliasOf", name)
            man.data["ui"][name]["previews"] = shown.get(src, shown.get(name, []))
    man.save()
    print(f"[portraits] {len(rows)} speakers → {len({r[2] for r in rows})} textures; aliases written: {written_alias}")
    for art, why, name in rows:
        print(f"[portraits]   {art:32s} {why:28s} → {name}")
    if problems:
        print("[portraits] PROBLEMS:\n  " + "\n  ".join(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
