"""March palette review: swatch sheets + today's assets re-paletted on a march ground (package A1, direction check).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/march/march_review.py
        [--sheets] [--characters] [--props [camp,story,ashwheat,interactive,foliage]]     (default: everything)

Writes ``Art/Previews/march_palettes/`` (review only, nothing ships):

* ``palette_<family>*.png`` — every march swatch of ``Monsters_March`` / ``Props_March`` / ``Foliage_March`` by index
  on the darkest march ground: index, the toon bands (shade · base · light) and the ink-hull colour, names, hex, E
  for emissive; ``palette_npc_march*.png`` — each march NPC look (the slots ``folk.regions_for`` derives) and the
  kit parts, NEW marking the swatches appended to ``NPC`` (the rest reuse plains NPC values). A legend strip
  carries the reserved colours (DR5) and the reference march ground with its L*.
* ``lineup_characters*.png`` — **today's shipped meshes, march colours only** (shapes are the plains ones): the
  warrior (unchanged: the hero keeps the warm palette) beside the goblin as a gravegnawer, the chief as a bonegorger,
  the shaman as the Corpseherd, the slime as a breachclot, three NPC bodies as palette proxies of the Unburied kit
  (footman), the Living (zealot, deserter); back row every NPC in its march look (+ proxies for the two new NPCs).
  ``lineup_props_<group>*.png`` — the camp kit, Ashwheat / story decor, interactive props, foliage and rocks.
  Each at the W1 game camera, native 1080p, shipped ink, bloom, on the reference march ground (``MARCH_GROUND``:
  ash grass, a mud lane, camp ash), with ``…_grade*.png`` an **approximate** P14 march grade (55 % of the mood:
  gain, saturation, contrast; no fog, tone curve or vignette); the characters also get
  ``…_inkgate*.png`` (the edge map on the dark ground).
* ``lineup_metrics.json`` — per figure: ink gate on the dark ground, median rim, CIELAB chroma p75, and an
  approximate lit-side ΔL* (median L* of the figure's brightest fifth of body pixels − the ground's median L*;
  darkgate.py is the real gate for the new monsters).
"""
from __future__ import annotations

import json
import math
import re
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parent
for _p in (BLENDER, BLENDER / "coherence", BLENDER / "environment", BLENDER / "npcs", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

import cohkit as K  # noqa: E402
import darkgate as DG  # noqa: E402
import envkit as E  # noqa: E402
import lineup as LU  # noqa: E402
import march_palettes as MP  # noqa: E402
from kit import color, outline, paths, pngio, review, shading  # noqa: E402
from kit.palette import GRID, SWATCH_PX, Palette  # noqa: E402

OUT = paths.preview_root() / "march_palettes"
RES = (1920, 1080)
SCRATCH = paths.scratch_root("march_review")
MOOD_MARCH = dict(ambient="#A8AEBB", alpha=0.30, sat=0.68, con=1.12, lift=(-.010, -.006, .004), gain=(0, .004, .020))


# ── march regions (what the registries hold now) ──────────────────────────────────────────────────────────
_PALS: dict[str, Palette] = {}


def pal(fam: str) -> Palette:
    if fam not in _PALS:
        _PALS[fam] = Palette(fam)
    return _PALS[fam]


def march_region(ref: str):
    """``Family:name`` (Family defaults by the name: npc.* / npckit.* → NPC) → Region, or None."""
    fam, _, name = ref.rpartition(":")
    if not fam:
        fam = "NPC" if name.startswith(("npc.", "npckit.")) else None
    fams = [fam] if fam else ["Monsters_March", "Props_March", "Foliage_March"]
    for f in fams:
        try:
            return pal(f).region(name)
        except KeyError:
            continue
    return None


def band_rgb(reg) -> dict:
    """Toon band colours of a region as the shader outputs them (emissive mixes toward the base)."""
    t = color.tone(reg.hex, reg.s, reg.l)
    c = t["base"]
    out = {}
    for k in ("shade", "base", "light"):
        out[k] = color.mix(t[k], c, reg.e)
    out["hull"] = outline.hull_rgb(reg.hex, reg.o)
    return out


# ── swatch sheets ─────────────────────────────────────────────────────────────────────────────────────────
BG = MP.hex_rgb(MP.DARKEST_GROUND)
PAD = tuple(max(0, c - 10) for c in BG)
TXT = (232, 226, 210)
DIM = (150, 144, 132)


def _chip(img, x, y, rgb, w=14, h=14):
    img[y:y + h, x:x + w] = [int(round(v)) for v in rgb[:3]]


def legend() -> np.ndarray:
    """Reserved colours + the reference march ground."""
    W, H = 1200, 96
    img = np.zeros((H, W, 3), np.uint8)
    img[:] = BG
    review.draw_text(img, 8, 6, "RESERVED COLOURS (DR5)", rgb=TXT)
    items = [("EMBER", MP.EMBER, "HERO LOOT REFUGE"), ("WARM", MP.WARM_LIGHT, "HEARTH FORGE LANTERN"),
             ("HALO", MP.HERO_HALO, "HERO LIGHT"), ("BREACH", MP.BREACH, "ENEMY MAGIC"),
             ("CORE", MP.BREACH_CORE, "BREACH CORE"), ("PIN", MP.BREACH_PIN, "VIOLET EYES"),
             ("BONE", MP.BONE_YELLOW, "GNAWER EYES"), ("CRIMSON", MP.CRIMSON[0], "LIVING CLOTH"),
             ("TELEGRAPH", MP.TELEGRAPH_RIM, "DECAL ONLY"), ("FILL", MP.TELEGRAPH_FILL, "DECAL ONLY"),
             ("MANA", MP.MANA_SHIELD, "HERO (30 DEG OFF)")]
    for i, (lab, hx, what) in enumerate(items):
        x = 8 + i * 108
        _chip(img, x, 20, MP.hex_rgb(hx), 20, 14)
        review.draw_text(img, x + 24, 21, lab, rgb=TXT)
        review.draw_text(img, x, 38, what[:17], rgb=DIM)
    review.draw_text(img, 8, 58, "MARCH GROUND (A10 PROPOSAL)", rgb=TXT)
    for i, (k, (nm, hx, _)) in enumerate(sorted(MP.MARCH_GROUND.items())):
        x = 8 + i * 230
        _chip(img, x, 72, MP.hex_rgb(hx), 30, 16)
        review.draw_text(img, x + 36, 76, f"{nm} {hx} L{MP.lab_l(MP.hex_rgb(hx)):.0f}", rgb=TXT)
    return img


def family_sheet(fam: str) -> np.ndarray:
    p = pal(fam)
    rows_src = {name: note for name, *_, note in MP.FAMILY_SETS[fam][0]}
    cells = []
    for sw in p.swatches:
        if sw is None:
            continue
        cells.append(sw)
    cw, ch, cols = 300, 20, 4
    nrows = (len(cells) + cols - 1) // cols
    head = legend()
    W = cw * cols
    body = np.zeros((nrows * ch, W, 3), np.uint8)
    body[:] = BG
    for k, sw in enumerate(cells):
        r, c = divmod(k, cols)
        x, y = c * cw + 6, r * ch + 3
        reg = p.region(sw["names"][0])
        b = band_rgb(reg)
        review.draw_text(body, x, y + 4, f"{sw['i']:3d}", rgb=DIM)
        for j, key in enumerate(("shade", "base", "light", "hull")):
            _chip(body, x + 22 + j * 15, y, b[key])
        nm = sw["names"][0] + (f" +{len(sw['names']) - 1}" if len(sw["names"]) > 1 else "")
        review.draw_text(body, x + 86, y + 4, nm[:26], rgb=TXT)
        tag = sw["hex"][1:] + (" E" if sw.get("e", 0) > 0 else "")
        review.draw_text(body, x + 86 + 27 * 6, y + 4, tag, rgb=(255, 200, 120) if sw.get("e", 0) > 0 else DIM)
        if r < nrows - 1:
            body[(r + 1) * ch - 1, c * cw:(c + 1) * cw] = PAD
    title = np.zeros((22, W, 3), np.uint8)
    title[:] = PAD
    review.draw_text(title, 8, 8, f"{fam.upper()}  {len(cells)} SWATCHES / 256  (IDX  SHADE BASE LIGHT HULL  NAME  HEX)  "
                                  f"ON THE DARKEST MARCH GROUND {MP.DARKEST_GROUND}", rgb=TXT)
    sep = np.zeros((4, W, 3), np.uint8)
    sep[:] = PAD
    return np.concatenate([title, head[:, :W] if head.shape[1] >= W else np.pad(head, ((0, 0), (0, W - head.shape[1]),
                                                                                       (0, 0)), mode="edge"),
                           sep, body], 0)


def npc_sheet(new_ids: set) -> np.ndarray:
    p = pal("NPC")
    slots = ["skin", "skin_shade", "hair", "beard", "top", "top_shade", "sleeve", "trim", "apron", "mantle", "hat",
             "legs", "boots", "belt", "pack"]
    W, rh = 1200, 34
    rows = list(MP.NPC_LOOKS) + ["(kit)"]
    img = np.zeros((len(rows) * rh + 30, W, 3), np.uint8)
    img[:] = BG
    review.draw_text(img, 8, 8, "NPC MARCH LOOKS (APPENDED TO NPC; WHITE TICK = NEW SWATCH, THE REST REUSE PLAINS "
                                "NPC VALUES)", rgb=TXT)
    for i, snake in enumerate(rows):
        y = 30 + i * rh
        img[y - 2, :] = PAD
        review.draw_text(img, 8, y + 10, snake.upper()[:22], rgb=TXT)
        if snake == "(kit)":
            items = [(f"{MP.NPC_KIT_PREFIX}.{k}", k) for k, *_ in MP.NPC_KIT]
        else:
            items = [(f"{MP.NPC_PREFIX}.{snake}.{s}", s) for s in slots]
        x = 150
        for name, lab in items:
            try:
                idx = p.index(name)
            except KeyError:
                continue
            b = band_rgb(p.region(name))
            _chip(img, x, y, b["light"], 18, 6)
            _chip(img, x, y + 6, b["base"], 18, 10)
            _chip(img, x, y + 16, b["shade"], 18, 6)
            if idx in new_ids:
                img[y:y + 3, x + 15:x + 18] = (255, 255, 255)
            review.draw_text(img, x - 1, y + 24, lab.replace("_", "")[:8], rgb=DIM)
            x += 52 if snake == "(kit)" else 68
    return img


def sheets() -> None:
    for fam in ("Monsters_March", "Props_March", "Foliage_March"):
        img = family_sheet(fam)
        out = pngio.write_png_native(OUT / f"palette_{fam.lower()}.png", img)
        for pth, size in out:
            print(f"  {pth}  {size // 1024} KB")
    before = 176                      # NPC.json held 0..175 (index 30 free) before A1
    p = pal("NPC")
    new_ids = {i for i, sw in enumerate(p.swatches) if sw is not None and (i >= before or i == 30)}
    out = pngio.write_png_native(OUT / "palette_npc_march.png", npc_sheet(new_ids))
    for pth, size in out:
        print(f"  {pth}  {size // 1024} KB")


# ── re-palette (today's meshes, march colours) ────────────────────────────────────────────────────────────
GNAWER = {
    "goblin.skin": "gnawer.hide", "goblin.skin_dark": "gnawer.hide_dark", "goblin.skin_line": "gnawer.hide_line",
    "goblin.belly": "gnawer.belly", "goblin.ear_in": "gnawer.ear_stub", "goblin.cloth": "gnawer.trophy",
    "goblin.cloth_in": "unburied.tabard_char", "goblin.strap": "gnawer.rope", "goblin.rope": "gnawer.rope",
    "goblin.rope_line": "gnawer.hide_line", "goblin.bone": "gnawer.claw", "goblin.bone_dark": "gnawer.hide_dark",
    "goblin.eye": "gnawer.eye", "goblin.pupil": "gnawer.pupil", "goblin.mouth": "gnawer.maw",
    "goblin_spear.wood": "gnawer.stick", "goblin_spear.wood_dark": "unburied.leather_dark",
    "goblin_spear.stone": "gnawer.spearhead", "goblin_spear.stone_dark": "unburied.helm_dent",
    "goblin_spear.rope": "gnawer.rope", "goblin_spear.rope_line": "gnawer.hide_line",
}
BONEGORGER = {
    "chief.skin": "bonegorger.hide", "chief.skin_dark": "bonegorger.hide_dark", "chief.skin_line": "gnawer.hide_line",
    "chief.belly": "bonegorger.belly", "chief.ear_in": "gnawer.ear_stub", "chief.cloth": "bonegorger.tuft",
    "chief.cloth_in": "bonegorger.hide_dark", "chief.strap": "gnawer.rope", "chief.rope": "gnawer.rope",
    "chief.rope_line": "gnawer.hide_line", "chief.bone": "gnawer.claw", "chief.bone_dark": "bonegorger.hide_dark",
    "chief.eye": "bonegorger.eye", "chief.pupil": "gnawer.pupil", "chief.mouth": "gnawer.maw",
    "chief.iron": "unburied.helm_dent", "chief.iron_dark": "valdras.plate_line", "chief.iron_light": "unburied.steel",
    "chief.horn": "gnawer.claw", "chief.horn_band": "clapper.bronze_dark", "chief.fur": "bonegorger.tuft",
    "chief.fur_dark": "bonegorger.hide_dark", "chief.leather": "deserter.leather",
    "chief.leather_dark": "unburied.leather_dark", "chief.socket": "gnawer.maw", "chief.paint": "bonegorger.hide_dark",
    "chief_cleaver.iron": "clapper.bronze", "chief_cleaver.iron_dark": "clapper.bronze_dark",
    "chief_cleaver.edge": "clapper.bronze", "chief_cleaver.rust": "clapper.patina",
    "chief_cleaver.hole": "clapper.bronze_dark", "chief_cleaver.leather": "deserter.leather",
    "chief_cleaver.leather_line": "unburied.leather_dark",
}
CORPSEHERD = {
    "shaman.skin": "corpseherd.hands", "shaman.skin_dark": "corpseherd.chin", "shaman.skin_line": "crook.wood_line",
    "shaman.belly": "corpseherd.hands", "shaman.ear_in": "corpseherd.face_void", "shaman.cloth": "corpseherd.skirt",
    "shaman.cloth_in": "corpseherd.face_void", "shaman.strap": "crook.cord", "shaman.rope": "crook.cord",
    "shaman.rope_line": "crook.wood_line", "shaman.fur": "corpseherd.rags", "shaman.fur_dark": "corpseherd.hood",
    "shaman.hide": "corpseherd.veil", "shaman.hide_dark": "corpseherd.hood", "shaman.hide_seam": "unburied.skin_line",
    "shaman.feather_red": "corpseherd.skirt", "shaman.feather_teal": "corpseherd.rags",
    "shaman.feather_cream": "corpseherd.veil", "shaman.feather_tip": "corpseherd.hood",
    "shaman.bead_red": "crook.bell", "shaman.bead_teal": "crook.bell", "shaman.socket": "corpseherd.face_void",
    "shaman.ember": "corpseherd.eye", "shaman.gem": "breach.pinpoint", "shaman.paint": "corpseherd.skirt",
    "shaman.spiral": "crook.wood", "shaman.bone": "corpseherd.hood", "shaman.bone_dark": "corpseherd.face_void",
    "shaman.bone_crack": "corpseherd.face_void", "shaman.eye": "corpseherd.eye", "shaman.pupil": "corpseherd.face_void",
    "shaman.mouth": "corpseherd.face_void",
    "shaman_totem.wood": "crook.wood", "shaman_totem.wood_dark": "crook.wood_line",
    "shaman_totem.hide_dark": "corpseherd.rags", "shaman_totem.hide_line": "crook.wood_line",
    "shaman_totem.socket": "corpseherd.face_void", "shaman_totem.glint": "breach.pinpoint",
    "shaman_totem.fire": "breach.glow", "shaman_totem.fire_hot": "breach.hot", "shaman_totem.bone": "crook.bell",
}
CLOT = {
    "slime.body": "clot.shell", "slime.core": "clot.core", "slime.bubble": "clot.spark", "slime.sclera": "clot.shell",
    "slime.brow": "clot.shell", "slime.eye_rim": "clot.shell", "slime.pupil": "clot.shell", "slime.glint": "clot.shell",
    "slime.xeye": "clot.shell", "slime.sheen": "clot.rim", "slime.bone": "clot.helm", "slime.inner": "clot.inner",
}
# NPC body slots → a kit (proxy figures of the Unburied / Living on today's NPC bodies)
FOOTMAN = {"skin": "unburied.skin", "skin_shade": "unburied.skin_shadow", "blush": "unburied.skin",
           "eye_white": "unburied.socket", "pupil": "unburied.eye", "eye_hi": "unburied.eye",
           "mouth": "unburied.mouth", "top": "unburied.tabard", "top_shade": "unburied.tabard_char",
           "sleeve": "unburied.mail", "legs": "unburied.gambeson", "boots": "unburied.leather_dark",
           "boot_line": "unburied.leather_dark", "belt": "unburied.leather", "gold": "unburied.trim",
           "leather": "unburied.leather", "trim": "unburied.trim", "hair": "unburied.hood",
           "hair_line": "unburied.socket", "beard": "unburied.skin_shadow", "beard_line": "unburied.skin_line",
           "stubble": "unburied.skin_shadow", "brow": "unburied.skin_line", "hat": "unburied.helm",
           "hat_line": "unburied.helm_dent", "hat_accent": "unburied.tabard", "mantle": "unburied.tabard_char",
           "mantle_line": "unburied.sigil", "pack": "unburied.leather", "pack_line": "unburied.leather_dark",
           "bedroll": "unburied.gambeson", "metal_dark": "unburied.helm_dent"}
ZEALOT = {"skin": "living.skin", "skin_shade": "living.skin_shade", "blush": "living.skin",
          "eye_white": "living.skin_shade", "pupil": "living.eye_dark", "eye_hi": "living.eye_dark",
          "mouth": "living.mouth", "top": "zealot.robe", "top_shade": "zealot.robe_dark", "sleeve": "living.skin",
          "legs": "zealot.robe_dark", "boots": "unburied.leather_dark", "boot_line": "unburied.leather_dark",
          "belt": "zealot.rope", "gold": "zealot.rope", "leather": "unburied.leather", "trim": "zealot.rope",
          "hair": "zealot.robe", "hair_line": "zealot.robe_dark", "brow": "living.skin_shade",
          "beard": "living.hair", "beard_line": "living.hair", "hat": "zealot.robe", "hat_line": "zealot.robe_dark",
          "mantle": "zealot.robe", "mantle_line": "zealot.robe_dark", "pack": "zealot.robe_dark",
          "pack_line": "unburied.leather_dark", "bedroll": "unburied.mail", "metal_dark": "unburied.mail"}
DESERTER = {"skin": "living.skin_b", "skin_shade": "living.skin_shade", "blush": "living.skin_b",
            "eye_white": "living.skin_shade", "pupil": "living.eye_dark", "eye_hi": "living.eye_dark",
            "mouth": "living.mouth", "top": "deserter.gambeson", "top_shade": "deserter.gambeson_line",
            "sleeve": "deserter.gambeson", "legs": "deserter.hood", "boots": "unburied.leather_dark",
            "boot_line": "unburied.leather_dark", "belt": "deserter.leather", "gold": "deserter.leather",
            "leather": "deserter.leather", "trim": "deserter.marker", "hair": "living.hair",
            "hair_line": "unburied.leather_dark", "brow": "living.hair", "beard": "living.hair",
            "beard_line": "unburied.leather_dark", "hat": "deserter.hood", "hat_line": "unburied.leather_dark",
            "hat_accent": "deserter.marker", "mantle": "deserter.hood", "mantle_line": "unburied.leather_dark",
            "pack": "deserter.pack", "pack_line": "unburied.leather_dark", "bedroll": "deserter.insignia_scar",
            "metal_dark": "unburied.helm_dent"}
NPC_SLOT_FALLBACK = {
    "hat": ["hair", "top"], "hat_line": ["hair_line", "top_shade"], "hat_accent": ["trim", "belt"],
    "trim": ["belt"], "specs": ["eye_white"], "brass": ["gold"], "brass_dark": ["belt"], "mantle": ["top"],
    "mantle_line": ["top_shade"], "apron": ["top"], "apron_line": ["top_shade"], "pack": ["belt"],
    "pack_line": ["boot_line"], "beard": ["hair", "skin_shade"], "beard_line": ["hair_line", "skin_shade"],
    "stubble": ["skin_shade"], "hair": ["skin"], "hair_line": ["skin_shade"], "bedroll": ["legs"],
    "metal_dark": ["belt"], "sleeve": ["top"],
}
SLOT_RANK = {"top": 0, "mantle": 1, "apron": 2, "skin": 3, "legs": 4, "boots": 5, "belt": 6, "pack": 7,
             "hair": 8, "beard": 9, "trim": 10, "hat": 60, "hat_accent": 61, "sleeve": 70, "blush": 71, "brow": 72}
NPC_PROP = {   # npcprop.<kind>.<slot> → march (the props carried by today's NPC meshes)
    "orb": "npckit.march.handbell", "orb_hot": "npckit.march.handbell", "herb": "npckit.march.stain",
    "herb_light": "npckit.march.shroud", "book": "npckit.march.ledger", "book_dark": "npckit.march.handbell_dark",
    "bark": "Props_March:camp.charred", "bark_dark": "Props_March:camp.charred_line",
    "wood_top": "Props_March:camp.timber_end", "rings": "Props_March:camp.timber_line",
}
PROPS_MAP = {
    "camp.well_stone": "camp.stone", "rock.rock": "rock.stone", "totem.stone": "camp.stone",
    "camp.stone_line": "camp.stone_line", "lore.boulder_line": "camp.stone_line", "rock.rock_line": "camp.stone_line",
    "boulder.rock": "boulder.stone", "boulder.rock_line": "camp.stone_line", "boulder.moss": "boulder.ash",
    "drywell.moss": "boulder.ash", "lore.moss": "boulder.ash", "outcrop.rock": "outcrop.stone",
    "skirt.rock": "outcrop.stone", "outcrop.rock_line": "camp.stone_line", "outcrop.cap": "outcrop.cap",
    "palisade.ivy": "camp.charred", "outcrop.hedge": "ground.dark", "outcrop.hedge_back": "ground.dark",
    "skirt.hedge": "ground.dark", "skirt.hedge_back": "ground.dark", "outcrop.fleck": "outcrop.cap",
    "skirt.fleck": "outcrop.cap", "outcrop.flower": "ground.light", "skirt.grass": "ground.dark",
    "skirt.grass_dark": "ground.dark",
    "palisade.wood": "camp.pew", "palisade.wood_dk": "camp.charred", "palisade.wood_line": "camp.charred_line",
    "palisade.band": "camp.iron", "palisade.pennant": "camp.shield", "palisade.cap": "camp.pew_worn",
    "camp.stone": "camp.stone", "interact.stone": "camp.stone", "camp.stone_top": "camp.stone_top",
    "camp.log": "hearth.log", "camp.log_end": "hearth.log_end", "camp.log_line": "camp.charred_line",
    "camp.ash": "camp.ash", "camp.ember": "hearth.ember", "camp.ember_hot": "hearth.ember_hot",
    "camp.ember_dim": "hearth.ember_dim", "totem.wood": "camp.timber", "camp.post_line": "camp.timber_line",
    "totem.wood_line": "camp.timber_line", "camp.roof": "camp.charred", "camp.roof_line": "camp.charred_line",
    "camp.bucket": "camp.crate", "camp.rope": "camp.rope", "camp.water": "camp.water",
    "camp.water_hi": "camp.water_sheen", "camp.iron": "camp.iron", "gobcamp.iron": "camp.iron",
    "camp.canvas": "camp.canvas", "camp.canvas_in": "camp.canvas_dark", "camp.canvas_line": "camp.canvas_line",
    "camp.canvas_trim": "camp.sack", "camp.gold": "banner.bell", "camp.peg": "camp.timber",
    "gobcamp.pole": "camp.charred", "camp.cloth": "banner.black", "camp.cloth_trim": "banner.trim",
    "camp.stave": "camp.crate", "camp.stave_line": "camp.timber_line", "camp.lid": "camp.timber_end",
    "camp.plank": "camp.crate", "camp.plank_line": "camp.timber_line", "camp.frame": "camp.timber",
    "camp.cup": "camp.helmet", "camp.rim": "camp.iron_dark", "camp.post": "camp.timber", "camp.pole": "camp.timber",
    "camp.bar": "camp.charred",
    "ruins.stone": "altar.stone", "ruins.stone_lt": "catechism.stone", "ruins.arch": "catechism.stone",
    "ruins.stone_line": "camp.stone_line", "ruins.relief": "camp.stone_top", "ruins.ivy": "ground.dark",
    "ruins.ivy_lt": "ground.light", "ruins.ivy_dk": "ground.dark",
    "totem.cloth": "redbanner.cloth", "totem.cloth_dk": "bannerring.cloth", "totem.paint": "banner.bell",
    "totem.bone": "camp.shield_rim", "totem.bone_dk": "camp.charred", "totem.rope": "camp.rope",
    "totem.hide": "camp.shield", "totem.hide_lt": "camp.pew_worn", "totem.stitch": "camp.canvas_line",
    "totem.feather": "camp.charred_line",
    "gobcamp.hide": "cottage.thatch", "gobcamp.hide_lt": "cottage.thatch_lit", "gobcamp.hide_dk": "cottage.door",
    "gobcamp.strap": "wagon.wood_dark", "gobcamp.bone": "cottage.daub", "gobcamp.bone_dk": "camp.charred",
    "gobcamp.char": "camp.charred", "gobcamp.cord": "camp.rope", "gobcamp.stitch": "camp.canvas_line",
    "gobcamp.feather": "camp.charred_line", "gobcamp.inside": "pit.dark", "gobcamp.soot": "camp.soot",
    "gobcamp.ash": "camp.ash", "gobcamp.ember": "camp.soot", "gobcamp.ember_hot": "camp.ash",
    "gobcamp.plank": "wagon.wood", "gobcamp.plank_line": "camp.timber_line", "gobcamp.batten": "camp.charred",
    "gobcamp.sack": "camp.sack", "gobcamp.feather_red": "camp.crate_stencil", "gobcamp.red": "camp.crate_stencil",
    "altar.slab": "altar.stone", "altar.body": "altar.stone", "altar.body_line": "altar.groove",
    "altar.top": "catechism.stone", "altar.rune": "altar.dried", "altar.rune_core": "altar.dried",
    "altar.ember": "altar.dried", "altar.drip": "altar.dried",
    "drywell.stone": "pit.stone", "drywell.stone_lt": "catechism.stone", "drywell.inner": "pit.dark",
    "drywell.leaves": "ground.dark", "drywell.script": "pit.seep",
    "lore.sandstone": "orderpost.wood", "lore.sandstone_line": "camp.stone_line", "lore.sandstone_sh": "camp.charred",
    "lore.script": "lore.glyph", "lore.script_core": "lore.glyph", "lore.leather": "journal.leather",
    "lore.leather_dk": "camp.charred", "lore.paper": "orderpost.paper", "lore.ink": "orderpost.ink",
    "lore.quill": "camp.charred_line", "lore.quill_dk": "camp.charred_line", "lore.crystal": "seal.stone",
    "lore.crystal_lt": "seal.stone", "lore.crystal_core": "seal.lines", "lore.rock_a": "rock.stone",
    "lore.rock_b": "rock.stone", "lore.boulder": "rock.stone", "lore.panel": "catechism.stone",
    "lore.glyph": "lore.glyph", "lore.glyph_core": "lore.glyph", "lore.lichen": "boulder.ash",
    "interact.wood": "strongbox.wood", "interact.wood_line": "camp.charred_line", "interact.wood_in": "pit.dark",
    "interact.brass": "strongbox.brass", "interact.lock": "strongbox.brass", "interact.keyhole": "pit.dark",
    "interact.seam": "strongbox.seam", "interact.pillar_base": "camp.stone", "interact.pillar": "camp.charred",
    "interact.rune_blue": "handbell.glow", "interact.rune_core": "handbell.glow", "interact.frame": "strongbox.iron",
    "interact.frame_lt": "camp.helmet", "interact.frame_line": "camp.stone_line", "interact.runestone": "bell.bronze",
    "interact.plank": "wagon.wood", "interact.plank_line": "camp.timber_line", "interact.iron": "camp.iron",
    "interact.nail": "camp.nail", "interact.sign_paint": "camp.crate_stencil", "interact.leaf": "barrowmoss.moss",
    "interact.vein": "barrowmoss.glow", "interact.stem": "barrowmoss.moss", "interact.soil": "ground.dark",
    "interact.bud": "barrowmoss.moss_lit", "interact.sealed_deep": "dais.crack", "interact.sealed": "dais.glow",
    "interact.sealed_glyph": "dais.vein", "interact.sealed_rim": "dais.crack", "interact.brass_rim": "strongbox.brass",
}
PROPS_SUFFIX = {"dark": "ground.dark", "light": "ground.light"}    # <any>.dark / <any>.light: ground blend
FOLIAGE_MAP = {
    "oak.bark": "deadoak.bark", "oak.bark_groove": "deadoak.split", "oak.back": "deadoak.split",
    "oak.front": "deadoak.char", "oak.flick": "deadoak.leaf", "fruittree.bark": "charred.bark",
    "fruittree.bark_groove": "charred.char", "fruittree.back": "charred.char", "fruittree.front": "charred.bark",
    "fruittree.flick": "charred.ash", "fruittree.fruit": "charred.apple", "fruittree.fruit_hi": "charred.apple",
    "bush.back": "briar.stem", "bush.front": "briar.leaf", "bush.flick": "briar.rag", "bush.berry": "briar.rag_crimson",
    "bush.stem": "briar.stem", "flowers.yellow": "poppy.petal", "flowers.white": "poppy.ash",
    "flowers.pink": "poppy.petal", "flowers.violet": "poppy.ash", "flowers.centre": "poppy.centre",
    "mushroom.cap": "ashcap.cap", "mushroom.spot": "ashcap.stem", "mushroom.stem": "ashcap.stem",
    "mushroom.gill": "ashcap.gill", "lily.pad": "scum.oil", "lily.pad_vein": "reed.head", "lily.lily": "reed.stem",
    "lily.lily_core": "reed.head", "grass.tipflower": "ashgrass.seed", "grass.dark": "ashgrass.dark",
    "flowers.dark": "ashgrass.dark", "mushroom.dark": "ashgrass.dark", "grass.light": "ashgrass.light",
    "flowers.light": "ashgrass.light", "mushroom.light": "ashgrass.light",
}
STATS: dict = {"fallback": {}}


def _fallback(sw) -> tuple:
    """An unmapped plains swatch, greyed down: 60 % toward its luminance, value × .75 (logged)."""
    c = np.array(MP.hex_rgb(sw["hex"]), np.float32)
    lum = float(c @ np.array([0.299, 0.587, 0.114], np.float32))
    c = (c + (lum - c) * 0.6) * 0.75
    return tuple(c), sw["s"], sw["l"], sw["e"], sw["o"]


def remap_atlas(family: str, pick, tag: str):
    """(BC, P) atlas arrays of ``family`` with every swatch ``pick(names) → Region | None`` repainted; ``None``
    keeps the swatch (a hero-side or loot colour) unless it is ``"grey"`` → the greyed fallback."""
    src = pal(family)
    bc, pp = src.atlas_arrays()
    fb = STATS["fallback"].setdefault(tag, [])
    for sw in src.swatches:
        if sw is None:
            continue
        reg = pick(sw["names"])
        if reg is None:
            continue
        if reg == "grey":
            rgb, s, l, e, o = _fallback(sw)
            fb.append(sw["names"][0])
        else:
            rgb, s, l, e, o = MP.hex_rgb(reg.hex), reg.s, reg.l, reg.e, reg.o
        i = sw["i"]
        col, row = i % GRID, i // GRID
        ys, xs = slice(row * SWATCH_PX, (row + 1) * SWATCH_PX), slice(col * SWATCH_PX, (col + 1) * SWATCH_PX)
        bc[ys, xs] = [int(round(v)) for v in rgb]
        pp[ys, xs] = [int(round(max(0.0, min(1.0, v)) * 255)) for v in (s, l, e, o)]
    return bc, pp


def files_materials(tag: str, bc: np.ndarray, pp: np.ndarray):
    """(toon, outline) preview materials over a remapped atlas (envkit.toon_material_files' graph)."""
    shading.ensure_groups()
    pbc, ppp = SCRATCH / f"T_Remap_{tag}_BC.png", SCRATCH / f"T_Remap_{tag}_P.png"
    pngio.write_png(pbc, bc)
    pngio.write_png(ppp, pp)
    ibc = E._image(pbc, f"T_Remap_{tag}_BC")
    ipp = E._image(ppp, f"T_Remap_{tag}_P")
    out = []
    for kind in ("toon", "outline"):
        m = bpy.data.materials.new(f"AF_Remap_{tag}_{kind}")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        b = shading.NB(nt)
        t_bc, t_p = shading._palette_samplers(b, ibc, ipp)
        o = b.node("ShaderNodeOutputMaterial")
        em = b.node("ShaderNodeEmission")
        em.inputs["Strength"].default_value = 1.0
        if kind == "toon":
            attr = b.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name="AF_Data")
            grp = b.node("ShaderNodeGroup", node_tree=bpy.data.node_groups[shading.TOON_GROUP])
            b.l.new(t_bc.outputs["Color"], grp.inputs["Base"])
            b.l.new(t_p.outputs["Color"], grp.inputs["Params"])
            g, _, _ = b.sep(attr.outputs["Vector"])
            b.l.new(g, grp.inputs["Grounding"])
            b.l.new(grp.outputs["Color"], em.inputs["Color"])
            b.l.new(em.outputs[0], o.inputs["Surface"])
        else:
            grp = b.node("ShaderNodeGroup", node_tree=bpy.data.node_groups[shading.OUTLINE_GROUP])
            b.l.new(t_bc.outputs["Color"], grp.inputs["Base"])
            b.l.new(t_p.outputs["Alpha"], grp.inputs["OutlineMix"])
            b.l.new(grp.outputs["Color"], em.inputs["Color"])
            tr = b.node("ShaderNodeBsdfTransparent")
            geo = b.node("ShaderNodeNewGeometry")
            mx = b.node("ShaderNodeMixShader")
            b.l.new(geo.outputs["Backfacing"], mx.inputs["Fac"])
            b.l.new(em.outputs[0], mx.inputs[1])
            b.l.new(tr.outputs[0], mx.inputs[2])
            b.l.new(mx.outputs[0], o.inputs["Surface"])
        out.append(m)
    return out[0], out[1]


def _need(ref: str):
    r = march_region(ref)
    if r is None:
        raise KeyError(f"re-palette target {ref} is not registered (run march_palettes.py)")
    return r


def table_pick(table: dict, fam_default: str, suffix: dict | None = None, keep_unmapped: tuple = ()):
    """Swatch names → the march swatch of ``table``; a swatch none of whose names carries one of the table's
    prefixes is not on this figure (kept); one that does but has no entry is greyed down and logged."""
    prefixes = tuple(sorted({k.split(".", 1)[0] + "." for k in table}))

    def pick(names):
        if not any(n.startswith(prefixes) for n in names):
            return None
        for n in names:
            if n in table:
                return _need(table[n] if ":" in table[n] else f"{fam_default}:{table[n]}")
        if suffix:
            for n in names:
                tail = n.rsplit(".", 1)[-1]
                if tail in suffix:
                    return _need(f"{fam_default}:{suffix[tail]}")
        if any(n.startswith(keep_unmapped) for n in names):
            return None
        return "grey"
    return pick


def npc_pick(target: str | dict, src_snake: str):
    """NPC swatch → the march look ``target`` (a snake in NPC_LOOKS) or a slot table (FOOTMAN …) for the NPC mesh
    ``src_snake``; props carried by NPC meshes through NPC_PROP; other NPCs' swatches stay (not on this mesh)."""
    def slot_region(slot):
        if isinstance(target, dict):
            t = target.get(slot)
            return _need(f"Monsters_March:{t}") if t else None
        for s in [slot] + NPC_SLOT_FALLBACK.get(slot, []):
            r = march_region(f"NPC:{MP.NPC_PREFIX}.{target}.{s}")
            if r is not None:
                return r
        return None

    def pick(names):
        # a swatch shared by two slots of this mesh (the elder's robe: sleeve = top) takes the main slot's colour
        mine = sorted((n for n in names if n.startswith(f"npc.{src_snake}.")),
                      key=lambda n: (SLOT_RANK.get(n.rsplit(".", 1)[1], 50), n))
        for n in mine:
            r = slot_region(n.rsplit(".", 1)[1])
            if r is not None:
                return r
        props = [n for n in names if n.startswith("npcprop.")]
        for n in props:
            t = NPC_PROP.get(n.rsplit(".", 1)[1])
            if t:
                return _need(t)
        if mine:
            return "grey"
        return None                       # metal, paper, quest markers, hot iron …: as shipped
    return pick


_TAGS = [0]


def remap_for(spec) -> dict:
    """{family: (toon, outline)} preview materials of one figure's re-palette ``spec`` = {family: pick}."""
    out = {}
    for fam, pick in spec.items():
        _TAGS[0] += 1
        tag = f"{fam}_{_TAGS[0]}"
        out[fam] = files_materials(tag, *remap_atlas(fam, pick, tag))
    return out


def import_remapped(asset: str, at, yaw: float, spec: dict, label: str, **kw):
    """``cohkit.import_character`` with the figure's families drawn from remapped atlases."""
    mats = remap_for(spec)
    saved = {f: K._MATS.get(f) for f in mats}
    K._MATS.update(mats)
    try:
        return K.import_character(asset, at, yaw, label=label, **kw)
    finally:
        for f, v in saved.items():
            if v is None:
                K._MATS.pop(f, None)
            else:
                K._MATS[f] = v


def import_static_remapped(asset: str, spec: dict):
    mats = remap_for(spec)
    saved = {f: K._MATS.get(f) for f in mats}
    K._MATS.update(mats)
    try:
        return K.import_static(asset)
    finally:
        for f, v in saved.items():
            if v is None:
                K._MATS.pop(f, None)
            else:
                K._MATS[f] = v


# ── ground, grade, shoot ──────────────────────────────────────────────────────────────────────────────────
def march_ground(c0, r0, c1, r1, lanes=(), camp=(), ppt: int = 48, shadows=(), name="march_ground"):
    """The reference march ground (MARCH_GROUND): ash grass with mud lanes and camp-ash tiles, a little value noise
    per tile and per texel (deterministic), the web contact shadows; the ``AF_Stage`` plane the review passes use."""
    rows, cols = r1 - r0, c1 - c0
    rng = np.random.default_rng(20261019)
    img = np.zeros((rows * ppt, cols * ppt, 3), np.float32)
    keys = np.zeros((rows, cols), np.int32)
    for (c, r) in lanes:
        if c0 <= c < c1 and r0 <= r < r1:
            keys[r - r0, c - c0] = 1
    for (c, r) in camp:
        if c0 <= c < c1 and r0 <= r < r1:
            keys[r - r0, c - c0] = 5
    for r in range(rows):
        for c in range(cols):
            base = np.array(MP.hex_rgb(MP.MARCH_GROUND[str(keys[r, c])][1]), np.float32)
            img[r * ppt:(r + 1) * ppt, c * ppt:(c + 1) * ppt] = base * (1.0 + rng.uniform(-0.035, 0.035))
    # (per-tile value jitter only: per-texel noise doubled the native PNGs and split them into narrow tiles)
    return DG.ground_plane(img.clip(0, 255), (c0, r0), (rows, cols), ppt, shadows=shadows, name=name), keys


def srgb_lin(a):
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def lin_srgb(a):
    a = np.clip(a, 0, None)
    return np.where(a <= 0.0031308, a * 12.92, 1.055 * np.power(a, 1 / 2.4) - 0.055)


def grade(img: np.ndarray, mood: dict = MOOD_MARCH) -> np.ndarray:
    """Approximate P14 grade at 55 % of the zone mood (DECISIONS P14): ColorGain = lerp(1, ambient, α·.5), saturation
    × 1.06 × (1 + (sat − 1)·.55), contrast 1 + (con − 1)·.55 about 0.18, lift / gain × .55 as shadow / highlight
    offsets. No fog, tone curve, exposure or vignette: a direction check, not the UE frame."""
    lin = srgb_lin(img.astype(np.float32) / 255.0)
    amb = srgb_lin(np.array(MP.hex_rgb(mood["ambient"]), np.float32) / 255.0)
    lin = lin * (1.0 + (amb - 1.0) * mood["alpha"] * 0.5)
    sat = 1.06 * (1.0 + (mood["sat"] - 1.0) * 0.55)
    y = lin @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    lin = y[..., None] + (lin - y[..., None]) * sat
    con = 1.0 + (mood["con"] - 1.0) * 0.55
    lin = 0.18 * np.power(np.clip(lin, 0, None) / 0.18, con)
    y = np.clip(lin @ np.array([0.2126, 0.7152, 0.0722], np.float32), 0, 1)[..., None]
    lin = lin + np.array(mood["lift"], np.float32) * 0.55 * (1 - y) + np.array(mood["gain"], np.float32) * 0.55 * y
    return (lin_srgb(lin).clip(0, 1) * 255 + 0.5).astype(np.uint8)


def write(name: str, img: np.ndarray, label: str) -> None:
    def ann(t, i, k):
        review.draw_text(t, 6, 6, label + (f"  TILE {i + 1}/{k}" if k > 1 else ""))
    for p, size in pngio.write_png_native(OUT / name, img, annotate=ann):
        print(f"  {p}  {size // 1024} KB")


def shoot(groups, cam, name: str, label: str, lanes=(), camp=(), shadows=(), inkmap: bool = False) -> list[dict]:
    every = [m for _, ms, _, _ in groups for m in ms]
    pts = review.mesh_points(every, step=8)
    c0, c1 = int(pts[:, 0].min()) - 10, int(pts[:, 0].max()) + 11
    r0, r1 = int(-pts[:, 1].max()) - 10, int(-pts[:, 1].min()) + 11
    march_ground(c0, r0, c1, r1, lanes, camp, shadows=shadows, name=f"g_{name}")
    bpy.context.view_layer.update()
    box = K.projected_box(cam, every, RES, pad_px=(36, 44))
    if box[0] <= 0.0 or box[1] <= 0.0 or box[2] >= 1.0 or box[3] >= 1.0:
        raise RuntimeError(f"{name}: the lineup does not fit the 1080p frame {box}")
    t0 = time.time()
    img, raw = K.render(every, box)
    print(f"  [{name}] render {time.time() - t0:.1f} s  {img.shape[1]}x{img.shape[0]}")
    recs, ink = [], img.copy()
    for lab, ms, cls, extra in groups:
        gate_cls = "decor" if cls == "none" else cls
        st = K.ink_gate_group(ms, every, box, img, raw, gate_cls, cam,
                              min_px=extra.get("gatePx"), emissive=K.LAST_EMISSIVE.get("em"))
        rec = LU.stats(st, lab, cls)
        if cls == "none":
            rec["inkOk"] = True
        else:
            ys, xs = st["edge_yx"]
            ox, oy = st["offset"]
            ok = st["edge_ok"]
            ink[ys[ok] + oy, xs[ok] + ox] = (60, 230, 90)
            ink[ys[~ok] + oy, xs[~ok] + ox] = (240, 60, 60)
        body, rgb = st["body"], st["rgb"]
        solid = body > 0.98
        if solid.sum() > 30:
            L = DG.lab_l(rgb[solid].reshape(-1, 3))
            top = L[L >= np.percentile(L, 80)]
            gl = DG.lab_l(rgb[(body + st["hull"]) < 0.02].reshape(-1, 3)) if ((body + st["hull"]) < 0.02).sum() > 50 \
                else np.array([MP.lab_l(MP.hex_rgb(MP.MARCH_GROUND["0"][1]))])
            rec["litL"] = round(float(np.median(top)), 1)
            rec["groundL"] = round(float(np.median(gl)), 1)
            rec["litDeltaL"] = round(rec["litL"] - rec["groundL"], 1)
        rec.update({k: v for k, v in extra.items() if k not in ("anchor", "nolabel", "gatePx")})
        recs.append(rec)
        print(f"  [fig] {lab:16s} {cls:11s} rim {rec.get('fracRim', 1) * 100:5.1f}%  C*p75 "
              f"{rec.get('chromaP75', 0):5.1f}  lit L* {rec.get('litL', 0):5.1f} (ground {rec.get('groundL', 0):4.1f}, "
              f"dL {rec.get('litDeltaL', 0):+5.1f})  {'PASS' if rec['inkOk'] else 'FAIL'}")
    x0, y0 = review.border_origin(box, RES)
    labelled = img.copy()
    for (lab, ms, cls, extra), rec in zip(groups, recs):
        if extra.get("nolabel"):
            continue
        anchor = extra.get("anchor")
        p = review.project_px(cam, anchor if anchor is not None else Vector(review.mesh_points(ms, step=8).mean(0)), RES)
        tx, ty = int(p[0] - x0 - len(lab) * 3), int(p[1] - y0 + 12)
        review.draw_text(labelled, max(2, tx), min(labelled.shape[0] - 20, max(2, ty)), lab)
        if "litDeltaL" in rec and cls != "none":
            tag = f"RIM {rec.get('fracRim', 1) * 100:.0f} DL {rec['litDeltaL']:+.0f}"
            review.draw_text(labelled, max(2, int(p[0] - x0 - len(tag) * 3)), min(labelled.shape[0] - 10, max(12, ty + 9)),
                             tag, rgb=(160, 230, 160) if rec["inkOk"] and rec["litDeltaL"] >= 15 else (240, 150, 90))
    write(f"{name}.png", labelled, label)
    g = grade(img)
    for (lab, ms, cls, extra) in groups:
        if extra.get("nolabel"):
            continue
        anchor = extra.get("anchor")
        p = review.project_px(cam, anchor if anchor is not None else Vector(review.mesh_points(ms, step=8).mean(0)), RES)
        review.draw_text(g, max(2, int(p[0] - x0 - len(lab) * 3)), min(g.shape[0] - 20, max(2, int(p[1] - y0 + 12))), lab)
    write(f"{name}_grade.png", g, label + "  APPROX P14 MARCH GRADE (GAIN SAT CONTRAST ONLY)")
    if inkmap:                         # the characters only (the props' gate results are in lineup_metrics.json)
        write(f"{name}_inkgate.png", ink, "INK GATE ON THE MARCH GROUND (GREEN = RIM >= GATE, RED = THINNER)")
    return recs


# ── lineups ───────────────────────────────────────────────────────────────────────────────────────────────
NPC_MESH = {  # snake → shipped NPC asset
    "blacksmith": "SK_NPC_Blacksmith", "merchant": "SK_NPC_Merchant", "quest_elder": "SK_NPC_QuestElder",
    "stash_keeper": "SK_NPC_StashKeeper", "plains_herbalist": "SK_NPC_PlainsHerbalist",
    "plains_wanderer": "SK_NPC_PlainsWanderer", "escort_traveling_merchant": "SK_NPC_EscortTravelingMerchant",
    "rescue_lost_traveler": "SK_NPC_RescueLostTraveler", "wandering_merchant": "SK_NPC_WanderingMerchant",
}
NPC_LABEL = {"quest_elder": "CAPTAIN", "knell_chaplain": "ALDWIN*", "blacksmith": "HALD", "merchant": "VIRA",
             "stash_keeper": "TAMSIN", "plains_herbalist": "MARL", "plains_wanderer": "CORRAN",
             "escort_traveling_merchant": "BERN", "rescue_lost_traveler": "PILGRIM",
             "wandering_merchant": "PEDDLER", "crow_trader": "MAG*"}
NPC_PROXY = {"knell_chaplain": "quest_elder", "crow_trader": "wandering_merchant"}   # new NPCs: today's mesh


def characters(results: dict) -> None:
    K.reset()
    review.setup_render(RES, 8)
    cam = review.game_camera((0.0, 0.0))
    r, u = LU.screen_axes(cam)
    se = review.FACINGS["se"]
    MON = "Monsters_Plains"
    front = [
        ("SK_Hero_Warrior", "WARRIOR", {}, "hero (unchanged)"),
        ("SK_Mon_Goblin", "GNAWER", {MON: table_pick(GNAWER, "Monsters_March")}, "goblin mesh as gravegnawer"),
        ("SK_Mon_GoblinChief", "BONEGORGER", {MON: table_pick(BONEGORGER, "Monsters_March")}, "chief as bonegorger"),
        ("SK_Mon_GoblinShaman", "CORPSEHERD", {MON: table_pick(CORPSEHERD, "Monsters_March")}, "shaman as Corpseherd"),
        ("SK_Mon_Slime", "CLOT", {MON: table_pick(CLOT, "Monsters_March")}, "slime as breachclot"),
        ("SK_NPC_PlainsWanderer", "FOOTMAN*", {"NPC": npc_pick(FOOTMAN, "plains_wanderer")}, "Unburied palette proxy"),
        ("SK_NPC_RescueLostTraveler", "ZEALOT*", {"NPC": npc_pick(ZEALOT, "rescue_lost_traveler")},
         "Living (zealot) palette proxy"),
        ("SK_NPC_EscortTravelingMerchant", "DESERTER*", {"NPC": npc_pick(DESERTER, "escort_traveling_merchant")},
         "Living (deserter) palette proxy"),
    ]
    figs = []
    for i, (asset, label, spec, note) in enumerate(front):
        at = r * ((i - (len(front) - 1) / 2) * 1.25) - u * 1.55
        f = import_remapped(asset, at, se, spec, label, cam=cam) if spec else \
            K.import_character(asset, at, se, label=label, cam=cam)
        figs.append((f, note))
    back = list(MP.NPC_LOOKS)
    for i, snake in enumerate(back):
        src = NPC_PROXY.get(snake, snake)
        at = r * ((i - (len(back) - 1) / 2) * 1.12) + u * 1.75
        f = import_remapped(NPC_MESH[src], at, se, {"NPC": npc_pick(snake, src)}, NPC_LABEL[snake],
                            scenery=(src == "blacksmith" and snake == "blacksmith"))
        figs.append((f, f"march look {snake}" + (f" on the {src} mesh" if src != snake else "")))
    lanes = [(int(round(f.root.location.x)) + dc, int(round(-f.root.location.y)) + dr)
             for f, _ in figs[:len(front)] for dc in (-1, 0, 1) for dr in (-1, 0, 1)]
    camp = [(int(round(f.root.location.x)) + dc, int(round(-f.root.location.y)) + dr)
            for f, _ in figs[len(front):] for dc in (-1, 0, 1) for dr in (-1, 0, 1)]
    shadows = [(f.root.location.x, -f.root.location.y,
                K.entry(f.asset).get("blobShadowRadiusCm", 30) / 100.0 * f.root.scale[0], 0.45) for f, _ in figs]
    groups = []
    for f, note in figs:
        groups.append((f.label, f.meshes, f.cls, {"asset": f.asset, "note": note, "anchor": f.root.location.copy(),
                                                  "gatePx": K.CHAR_GATE_PX}))
        for x in f.extra:
            if "AnvilStump" in x.name:
                groups.append((f.label + " ANVIL", [x], K.slot_info(K.entry("SM_Prop_Camp_AnvilStump"))[1],
                               {"asset": "SM_Prop_Camp_AnvilStump", "nolabel": True}))
    results["characters"] = shoot(groups, cam, "lineup_characters",
                                  "MARCH PALETTES ON TODAY'S MESHES (COLOUR ONLY, * = PROXY MESH)  W1 CAMERA 1080P 1:1",
                                  lanes=lanes, camp=camp, shadows=shadows, inkmap=True)


PROP_GROUPS = {   # group → [(asset, label)] (lineup.PROP_GROUPS order); family re-palette from the asset's palette
    "camp": [("SM_Prop_Camp_Tent", "BELL FRAME"), ("SM_Prop_Camp_Campfire", "HEARTH"), ("SM_Prop_Camp_Well", "WELL"),
             ("SM_Prop_Camp_Banner", "BANNER"), ("SM_Prop_Camp_Torch", "BRAZIER"), ("SM_Prop_Camp_Barrel", "SACKS"),
             ("SM_Prop_Camp_Crate", "CRATE"), ("SM_Prop_Camp_AnvilStump", "ANVIL"),
             ("SM_Palisade_Plains_Stake_A", "PEW STAKE"), ("SM_Palisade_Plains_GatePost", "GATEPOST")],
    "story": [("SM_Prop_Story_ElvenRuins", "BELFRY"), ("SM_Prop_Story_GoblinTotem", "RED BANNER"),
              ("SM_Prop_Story_BrokenAltar", "PENITENTS"), ("SM_Prop_Story_DryWell", "PIT MOUTH"),
              ("SM_Prop_ExitGate_Sealed", "BARRICADE")],
    "ashwheat": [("SM_Prop_Goblin_Hut_A", "COTTAGE A"), ("SM_Prop_Goblin_Hut_B", "COTTAGE B"),
                 ("SM_Prop_Goblin_FirePit", "COLD PIT"), ("SM_Prop_Goblin_BoneTotem", "POST"),
                 ("SM_Prop_Goblin_SpearRack", "RACK"), ("SM_Prop_Goblin_Crates", "CRATES")],
    "interactive": [("SK_Prop_TreasureChest", "STRONGBOX"), ("SM_Prop_Lore_AncientTablet", "ORDER POST"),
                    ("SM_Prop_Lore_TornJournal", "JOURNAL"), ("SM_Prop_Lore_CrystalShard", "SEAL SHARD"),
                    ("SM_Prop_Lore_CarvedStone", "CATECHISM"), ("SM_Prop_Event_RunePillar", "HANDBELLS"),
                    ("SM_Prop_Gather_EmeraldHerb", "BARROWMOSS"), ("SM_Pickup_Gold_M", "GOLD (LOOT)"),
                    ("SM_Pickup_PotionHP", "POTION (LOOT)")],
    "foliage": [("SM_Foliage_Plains_Oak_A", "DEAD OAK"), ("SM_Foliage_Plains_FruitTree_A", "CHARRED"),
                ("SM_Foliage_Plains_BerryBush_A", "BRIAR"), ("SM_Foliage_Plains_RedMushrooms_A", "ASHCAP"),
                ("SM_Foliage_Plains_FlowerPatch_A", "POPPY"), ("SM_Foliage_Plains_GrassTuft_A", "ASH GRASS"),
                ("SM_Rock_Plains_Cluster_A", "STONE"), ("SM_Rock_Plains_MossBoulder_A", "BOULDER"),
                ("SM_Outcrop_Plains_V0", "OUTCROP")],
}
KEEP = ("interact.coin", "interact.gold", "interact.gem", "interact.vortex", "interact.swirl", "interact.ring_mark",
        "interact.lens", "interact.glint", "interact.handle", "interact.arrow", "camp.rune", "camp.gem")


PER_ASSET = {   # one asset's own reading of a shared plains swatch (the torch shares the campfire's embers)
    "SM_Prop_Camp_Torch": {"camp.ember": "brazier.coals", "camp.ember_hot": "brazier.coals",
                           "camp.ember_dim": "brazier.coals"},
}


def prop_spec(fam: str, asset: str = ""):
    if fam == "Props_Plains":
        return {fam: table_pick({**PROPS_MAP, **PER_ASSET.get(asset, {})}, "Props_March", PROPS_SUFFIX,
                                keep_unmapped=KEEP)}
    if fam == "Foliage_Plains":
        return {fam: table_pick(FOLIAGE_MAP, "Foliage_March")}
    if fam == "NPC":
        return {fam: npc_pick("blacksmith", "blacksmith")}
    return {}                          # Pickups (loot keeps its colours), Heroes


def props(results: dict, only=None) -> None:
    results.setdefault("props", [])
    for gname, items in PROP_GROUPS.items():
        if only and gname not in only:
            continue
        results["props"] = [p for p in results["props"] if p.get("group") != gname]
        K.reset()
        review.setup_render(RES, 8)
        cam = review.game_camera((0.0, 0.0))
        r, u = LU.screen_axes(cam)
        face = review.FACINGS["front"] + 15.0
        items = [x for x in items if x[0] in K.manifest()["assets"]]
        half = []
        for asset, _ in items:
            b = K.entry(asset)["boundsCm"]
            ext = max(b["max"][0] - b["min"][0], b["max"][1] - b["min"][1]) / 100.0
            half.append(max(0.18, 0.5 * ext * 0.85))
        xs, x = [], 0.0
        for i, hw in enumerate(half):
            if i:
                x += half[i - 1] + hw + 0.22
            xs.append(x)
        mid = (xs[0] + xs[-1]) / 2
        xs = [v - mid for v in xs]
        groups, shadows = [], []
        for (asset, label), x in zip(items, xs):
            at = r * x - u * 0.2
            e = K.entry(asset)
            fam = K.slot_info(e)[0]
            spec = prop_spec(fam, asset)
            if e["kind"] == "SkeletalMesh":
                f = import_remapped(asset, at, face, spec, label) if spec else K.import_character(asset, at, face,
                                                                                                    label=label)
                ms, root = f.meshes, f.root
            else:
                o = import_static_remapped(asset, spec) if spec else K.import_static(asset)
                o.location = at
                o.rotation_euler = (0, 0, math.radians(face))
                ms, root = [o], o
            _, cls, px, _ = K.slot_info(e)
            groups.append((label, ms, cls, {"asset": asset, "group": gname, "anchor": root.location.copy()}))
            if cls != "none":
                rad = e.get("blobShadowRadiusCm")
                if rad is None:
                    b = e["boundsCm"]
                    rad = 0.42 * max(b["max"][0] - b["min"][0], b["max"][1] - b["min"][1])
                shadows.append((root.location.x, -root.location.y, min(rad / 100.0, 1.3), 0.40))
        w = K.import_character("SK_Hero_Warrior", r * (xs[0] - half[0] - 0.75) + u * 0.35, review.FACINGS["se"],
                               label="WARRIOR")
        groups.append(("WARRIOR", w.meshes, w.cls, {"asset": w.asset, "group": gname, "scaleRef": True,
                                                     "gatePx": K.CHAR_GATE_PX, "anchor": w.root.location.copy()}))
        shadows.append((w.root.location.x, -w.root.location.y, 0.39, 0.45))
        camp = []
        if gname == "camp":
            camp = [(cx, cy) for cx in range(-12, 13) for cy in range(-8, 9)]
        recs = shoot(groups, cam, f"lineup_props_{gname}",
                     f"MARCH PALETTES ON TODAY'S {gname.upper()} MESHES (COLOUR ONLY)  W1 CAMERA 1080P 1:1",
                     camp=camp, shadows=shadows)
        results["props"] += [x for x in recs if not x.get("scaleRef")]


def main(argv: list[str]) -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    flags = {a.split("=")[0] for a in argv if a.startswith("--")}
    want = flags & {"--sheets", "--characters", "--props"} or {"--sheets", "--characters", "--props"}
    only = [a for a in argv if not a.startswith("--")]
    only = only[0].split(",") if only else None
    metrics = OUT / "lineup_metrics.json"
    results = json.loads(metrics.read_text()).get("results", {}) if metrics.is_file() else {}
    if "--sheets" in want:
        sheets()
    if "--characters" in want:
        characters(results)
    if "--props" in want:
        props(results, only)
    if "--characters" in want or "--props" in want:
        fb = {k: sorted(set(v)) for k, v in STATS["fallback"].items() if v}
        n = sum(len(v) for v in fb.values())
        print(f"  unmapped plains swatches greyed down: {n}")
        for k, v in fb.items():
            print(f"    {k}: {', '.join(v)[:200]}")
        metrics.write_text(json.dumps({"note": "approximate direction metrics (march_review.py); darkgate.py gates "
                                               "the shipped monsters", "ground": MP.MARCH_GROUND,
                                       "results": results}, indent=1, ensure_ascii=False) + "\n")
        print(f"  {metrics}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
