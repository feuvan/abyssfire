"""绳林 · The Hanging Wood: palette families of the ``hangwood`` theme (Chapter 2, package 4R-1; Docs/dark-remake/
chapter2.md §4.1 colour discipline, §4.4 the Moon-Bitten, §15 zone mood, §17 art list, §22 O4).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/hangwood/hangwood_palettes.py
        (no flag)            register every swatch below (append-only), write the atlases + manifest palette entries
        --check              rules + capacity on a scratch copy of the registries (writes nothing)
        --families=Monsters_Hangwood,Props_Hangwood   only these families · --list   every name → swatch index

The Chapter 2 colour script written down as swatches, the way ``march/march_palettes.py`` holds Chapter 1's (its
presets, helpers and reserved colours are imported, not copied). Three families: ``Monsters_Hangwood`` (the roster of
§4; this package ships its first user, the moon-hound), ``Props_Hangwood`` and ``Foliage_Hangwood`` (**seed rows
only**: the reserved lights and the handful of materials §15 names, so that 4R-3 starts from one registry and the hue
audit sees the two new accents before any other asset is drawn; 4R-3 appends the rest).

**What Chapter 2 adds to DR5** (§4.1; ``check`` gates the swatch side, ``vfx/hue_audit.py`` the recipes):

| Colour | Hex | Who | Never |
|---|---|---|---|
| moon-tarnish | #9FE0D4 (hue 169°, sat .29; the spec's #9FE0C8 at 158° fails the hue audit: O4, see ``MOON_TARNISH``) round near-black #0C1614 | the fouled Moon Seal: Wood's Dead eye pinpoints, the light between Volgan's teeth, the Cantor's raise marks, the Barrow's root glow | a damage telegraph; a hero recipe within 30° of hue at sat > .5 |
| sallow lamplight | #E6D36A (hue 51°, sat .54) | Lamp-Bearer lanterns and the Chantry's seven poles: light that shows and does not warm | the ember band (hue 15-35°, sat > .6); a refuge light |
| silver-white eye-shine | #E6ECF5, **not emissive** | the Moon-Bitten (hounds, the Moon-Sick, Volgan's eyes): beasts and the turned | a glow, a bloom |
| true moonlight | #E6ECF5 top light | only after ``hw_barrow_cleansed`` and in the cleansed Barrow | before the cleanse |

Family markers (one per hostile, named ``*.eye`` / ``*.marker`` so darkgate and the hue audit find them): Wood's Dead =
moon-tarnish pinpoints (``wooddead.eye``), Moon-Bitten = silver-white eye-shine (``moonbitten.eye``), the Lamp-Bearers
= a crimson lamp-cord on black (``lampbearer.marker``, no glow: the Living's crimson cloth rule).

**The hound** (§4.4): silver-grey coat gone to slate #6E7478, shadow #32363A, belly #9AA0A0, a torn leather temple
collar with a tarnished Well-Ring #8A9088. A low, dark body on dark ground is this chapter's hardest readability case:
the coat's light band has to clear the darkest walkable tile (:data:`HANGWOOD_GROUND` ``1``, the root earth) by ≥ 15
L* (the darkgate lit rule) and the eye-shine has to survive the shade band without an emissive term (preset ``flat``:
no shade mix beyond the toon's fixed 35 %).
"""
from __future__ import annotations

import os
import shutil
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parent
for _p in (BLENDER, BLENDER / "march", BLENDER / "npcs"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import march_palettes as MP  # noqa: E402  (presets, colour helpers; pure Python until register())

# ── reserved colours of Chapter 2 (chapter2.md §4.1, §15) ───────────────────────────────────────────────────────
MOON_TARNISH = "#9FE0D4"     # the fouled seal's light: enemy magic that is not a damage hazard. O4: the spec's
#                              #9FE0C8 (hue 158°) fails the hue audit's tarnish rule against the hero greens (137°,
#                              135°) and a shift toward 150° fails worse; hue 169° is the nearest that clears both
#                              the greens and frost (201°). Provisional (vfx/hue_audit.py MOON_TARNISH: one value)
MOON_TARNISH_SPEC = "#9FE0C8"
TARNISH_CORE = "#0C1614"     # … round near-black cores
SALLOW = "#E6D36A"           # Lamp-Bearer lanterns, the Chantry's poles (hue 51°: outside the ember band)
MOON_SILVER = "#E6ECF5"      # the Moon-Bitten's eye-shine (non-emissive) and, after the cleanse, true moonlight
WARM_LIGHT = MP.WARM_LIGHT   # #FFB060: the mill fire, Harl's forge, the Crow Carts' cookfire, one candle (Bran's hut)
TARNISH_GUARD_DEG = 30.0     # no hero colour at sat > .5 within this of moon-tarnish's hue (vfx/hue_audit.py)

# Reference ground of the theme (``environment/terrain_hangwood.py`` paints the textures from these bases; terrain
# keys of the map: 0 grass, 1 dirt, 2 stone, 3 water, 5 camp). Used by ``check`` (lit-band rule) and the stages.
HANGWOOD_GROUND = {
    "0": ("leafmould", "#3F3E32", "leaf mould: wet dead leaves, pale fungus specks"),
    "1": ("rootearth", "#35302A", "root-ribbed black earth (the darkest walkable tile: the 'root tile' of §4.4)"),
    "2": ("wetstone", "#3D4242", "wet flagstone and outcrop, moss in the joints"),
    "3": ("pond", "#0E1414", "black pond water with a silver-green sheen #4A6A60"),
    "5": ("yard", "#443D33", "the mill yard: trodden earth, chaff"),
}
DARKEST_GROUND = HANGWOOD_GROUND["1"][1]
STAGE_GROUND_HEX = HANGWOOD_GROUND["1"][1]      # review stages stand the roster on the root earth

MON_PRESETS = MP.MON_PRESETS
PROP_PRESETS = MP.PROP_PRESETS
FOL_PRESETS = MP.FOL_PRESETS
O_MON, O_PROP, O_FOL = MP.O_MON, MP.O_PROP, MP.O_FOL

# ── Monsters_Hangwood ───────────────────────────────────────────────────────────────────────────────────────
# (name, hex, preset, emissive, note). Names: <kit or member>.<region>; generators take their Regions from here
# (``region(name)``), so a build never appends or moves a swatch.
MONSTERS = [
    # shared accents of the fouled seal (4R-2: Wood's Dead eyes, the Cantor's marks, Volgan's maw)
    ("tarnish.glow", MOON_TARNISH, "glow", .85, "moon-tarnish on bodies: seams, the light between Volgan's teeth"),
    ("tarnish.core", TARNISH_CORE, "void", 0, "the near-black core round every moon-tarnish glow"),
    ("tarnish.pinpoint", MOON_TARNISH, "glow", 1.0, "moon-tarnish pinpoints (Wood's Dead eyes behind the shroud)"),
    ("sallow.lamp", SALLOW, "glow", .80, "Lamp-Bearer lantern panes (a held prop light; shows, does not warm)"),
    # the Moon-Bitten kit (§4.4): beasts and the long-turned; silver-white eye-shine, no emissive
    ("moonbitten.eye", MOON_SILVER, "flat", 0, "FAMILY MARKER: silver-white eye-shine (non-emissive)"),
    ("moonbitten.eye_rim", "#14161A", "void", 0, "the dark socket round the eye-shine"),
    ("moonbitten.maw", "#15171A", "void", 0, "maw, lips, nose leather (near-black: no gore, no red)"),
    ("moonbitten.teeth", "#C9C8BA", "bright_matte", 0, "teeth, fangs (dull bone)"),
    ("moonbitten.claw", "#2B2D31", "dark", 0, "claws, pads"),
    # moon_hound 食月犬 (first member of SKEL_Quadruped)
    ("moonhound.coat", "#6E7478", "hide", 0, "silver-grey coat gone to slate"),
    ("moonhound.coat_shadow", "#32363A", "skin_dark", 0, "painted shadow: rib strokes, flank hollow, hock backs"),
    ("moonhound.belly", "#9AA0A0", "hide", 0, "belly, throat, inner legs, muzzle underside"),
    ("moonhound.back", "#565C62", "hide", 0, "the slate saddle along the spine, ear backs, tail top"),
    ("moonhound.ear_inner", "#4A4648", "skin_dark", 0, "inner ear"),
    ("moonhound.collar", "#3C2E25", "dark", 0, "the torn leather temple collar"),
    ("moonhound.collar_edge", "#231A15", "line", 0, "its torn edge, the buckle's shadow"),
    ("moonhound.ring", "#8A9088", "metal_dull", 0, "the tarnished Well-Ring on the collar"),
]

# ── Props_Hangwood (seed rows: 4R-3 appends the camp kit, set pieces and the Barrow kit) ─────────────────────
PROPS = [
    ("lantern.sallow", SALLOW, "glow_soft", .80, "Lamp-Bearer lanterns, the Chantry's seven poles"),
    ("lantern.frame", "#3A3A34", "decor_dark", 0, "lantern cages, pole irons"),
    ("seal.tarnish", MOON_TARNISH, "glow", .85, "the fouled seal from below: the Barrow's root glow, the dark Moon Well"),
    ("seal.core", TARNISH_CORE, "decor_dark", 0, "its near-black core"),
    ("moon.true", MOON_SILVER, "glow_soft", .60, "true moonlight: only after hw_barrow_cleansed / the cleansed Barrow"),
    ("hearth.glow", WARM_LIGHT, "glow_soft", .80, "the reserved warm light (mill fire, forge, cookfire, one candle)"),
    ("root.bark", "#3E362E", "decor", 0, "root knuckles, the Gallows-Oak's trunk"),
    ("root.bark_lit", "#5A5044", "decor_lit", 0, "their ridges"),
    ("root.wet", "#2A2622", "decor_dark", 0, "root hollows, wet wood"),
    ("rope.hemp", "#8A7E62", "decor", 0, "hanging rope, cut rope-ends"),
    ("rope.dark", "#4A4234", "decor_dark", 0, "old rope, knots"),
    ("shroud.linen", "#A8A294", "decor_lit", 0, "hung bundles, shroud lines (wrapped shapes: never a body)"),
    ("shroud.shadow", "#5E5A52", "decor", 0, "their folds"),
    ("stone.wet", "#4A4E4E", "decor", 0, "wet stone: the Chantry's flags, the Writ Stone"),
    ("stone.moss", "#3E4A38", "decor", 0, "moss on stone"),
]

# ── Foliage_Hangwood (seed rows) ─────────────────────────────────────────────────────────────────────────────
FOLIAGE = [
    ("moonoak.bark", "#3A342E", "bark", 0, "moon-oak trunks and the low horizontal boughs"),
    ("moonoak.bark_lit", "#5C5348", "bark_lit", 0, "bough tops"),
    ("moonoak.leaf", "#2E3A34", "leaf", 0, "the thin dark canopy (blue-green, low chroma)"),
    ("moonoak.leaf_lit", "#44544A", "leaf", 0, "canopy lit side"),
    ("fungus.pale", "#B8B8A4", "petal", 0, "pale fungus"),
    ("fern.dark", "#2A3A30", "leaf", 0, "thinned undergrowth (a low dark hound has to stay readable)"),
]

FAMILY_SETS = {"Monsters_Hangwood": (MONSTERS, MON_PRESETS, O_MON, {}),
               "Props_Hangwood": (PROPS, PROP_PRESETS, O_PROP, {}),
               "Foliage_Hangwood": (FOLIAGE, FOL_PRESETS, O_FOL, {})}
PARENTS = {"Foliage_Hangwood": "M_AF_Toon_Foliage"}          # manifest palette parent (envkit.FOLIAGE_TOON)
MARKER_ROLES = {"moon_hound": ["moonbitten.eye"], "hound_packleader": ["moonbitten.eye"]}
# who may glow in which reserved hue (prefixes of swatch names)
TARNISH_ALLOWED = ("tarnish.", "wooddead.", "cantor.", "volgan.", "rootbound.", "seal.", "barrow.")
SALLOW_ALLOWED = ("sallow.", "lantern.", "lampbearer.", "netter.", "warden.", "chantry.")
WARM_ALLOWED = ("hearth.", "forge.", "cookfire.", "candle.")
BODY_PRESETS = MP.BODY_PRESETS
hex_rgb, lab_l, hsv, is_ember, light_tone = MP.hex_rgb, MP.lab_l, MP.hsv, MP.is_ember, MP.light_tone


def hue_dist(a: float, b: float) -> float:
    d = abs(a - b) % 360.0
    return min(d, 360.0 - d)


def region(name: str):
    """The registered Region of a Monsters_Hangwood swatch name (what a generator builds with)."""
    from kit.palette import Region
    for n, hx, pre, e, _ in MONSTERS:
        if n == name:
            s, l = MON_PRESETS[pre]
            return Region(hx, s, l, e, O_MON)
    raise KeyError(f"no Monsters_Hangwood swatch {name!r} (hangwood_palettes.MONSTERS)")


def family_regions(family: str) -> list:
    """[(name, Region, note)] of a family in registration order."""
    from kit.palette import Region
    rows, presets, o, o_over = FAMILY_SETS[family]
    out = []
    for name, hx, pre, e, note in rows:
        s, l = presets[pre]
        out.append((name, Region(hx, s, l, e, o_over.get(name, o)), note))
    return out


def check_rules() -> tuple[list, list]:
    """(failures, advisories) of the colour rules on the swatch lists (no registry needed)."""
    fails, adv = [], []
    ground_l = lab_l(hex_rgb(DARKEST_GROUND))
    t_hue = hsv(MOON_TARNISH)[0]
    s_hue, s_sat, _ = hsv(SALLOW)
    if is_ember(SALLOW) or (15.0 <= s_hue <= 35.0):
        fails.append(f"sallow lamplight {SALLOW} (hue {s_hue:.0f}) lies in the ember hue band 15-35")
    if hue_dist(t_hue, 288.1) < 30.0:
        fails.append(f"moon-tarnish {MOON_TARNISH} (hue {t_hue:.0f}) lies in the breach-violet guard band")
    for fam, rows, presets in (("Monsters_Hangwood", MONSTERS, MON_PRESETS), ("Props_Hangwood", PROPS, PROP_PRESETS),
                               ("Foliage_Hangwood", FOLIAGE, FOL_PRESETS)):
        seen = set()
        for name, hx, pre, e, _ in rows:
            if name in seen:
                fails.append(f"{fam}: swatch name {name} listed twice")
            seen.add(name)
            if pre not in presets:
                fails.append(f"{fam} {name}: unknown preset {pre}")
            hue, s, v = hsv(hx)
            eye = name.endswith((".eye", ".eyes", ".sclera", ".pupil", ".marker"))
            if fam == "Monsters_Hangwood" and (e > 0 or eye) and is_ember(hx):
                fails.append(f"ember: enemy swatch {fam} {name} {hx} is ember orange (DR5)")
            if e > 0 and is_ember(hx) and not name.startswith(WARM_ALLOWED):
                fails.append(f"warm light: {fam} {name} {hx} glows ember outside the reserved warm lights (§15)")
            if e > 0 and s > 0.15 and hue_dist(hue, t_hue) < 20.0 and not name.startswith(TARNISH_ALLOWED):
                fails.append(f"moon-tarnish: {fam} {name} {hx} glows in the seal's hue but is not the seal's "
                             f"({', '.join(TARNISH_ALLOWED)})")
            if e > 0 and s > 0.35 and hue_dist(hue, s_hue) < 12.0 and not name.startswith(SALLOW_ALLOWED):
                fails.append(f"sallow: {fam} {name} {hx} glows in the lamplight hue but is not a Lamp-Bearer light")
            if name.startswith("moonbitten.eye") and name.endswith(".eye") and e > 0:
                fails.append(f"{name}: the Moon-Bitten's eye-shine is not emissive (§4.1)")
            if fam == "Monsters_Hangwood" and pre in BODY_PRESETS:
                s_, l_ = presets[pre]
                dl = lab_l(light_tone(hx, s_, l_)) - ground_l
                if dl < 15.0:
                    fails.append(f"lit band: {fam} {name} {hx} light tone only {dl:+.1f} L* over the darkest "
                                 f"hangwood ground {DARKEST_GROUND} (darkgate lit rule 15)")
    names = {n for n, *_ in MONSTERS}
    for role, ms in MARKER_ROLES.items():
        for m in ms:
            if m not in names:
                fails.append(f"marker: {role} has no marker swatch {m}")
    eye_l = lab_l(hex_rgb(MOON_SILVER))
    coat = next(hx for n, hx, *_ in MONSTERS if n == "moonhound.coat")
    if eye_l - lab_l(light_tone(coat, *MON_PRESETS["hide"])) < 20.0:
        adv.append("the eye-shine is less than 20 L* over the coat's light band (it has to read on the head)")
    return fails, adv


def register(families, dry: bool) -> dict:
    """Register every swatch of ``families`` (append-only), save registry + atlases, write the manifest palette
    entries (not for a dry run). Returns {family: {before, after, new, names, capacity}}."""
    from kit import export
    from kit.palette import GRID, Palette
    out = {}
    for fam in families:
        pal = Palette(fam)
        before = {i for i, sw in enumerate(pal.swatches) if sw is not None}
        idx = {}
        for name, reg, _ in family_regions(fam):
            idx[name] = pal.add(name, reg)
        after = {i for i, sw in enumerate(pal.swatches) if sw is not None}
        for n in [n for n in idx if n in pal._released]:
            print(f"  [moved] {fam} {n} → swatch {idx[n]} (colour script changed; re-ship what samples it)")
        pal.save()
        out[fam] = {"before": len(before), "after": len(after), "new": sorted(after - before), "names": idx,
                    "capacity": GRID * GRID}
        if not dry:
            man = export.Manifest()
            man.set_palette(pal)
            if fam in PARENTS:
                man.data["palettes"][fam]["parent"] = PARENTS[fam]
            man.save()
    return out


def main(argv: list[str]) -> int:
    dry = "--check" in argv
    fams = list(FAMILY_SETS)
    for a in argv:
        if a.startswith("--families="):
            fams = [f for f in a.split("=", 1)[1].split(",") if f]
    fails, adv = check_rules()
    for a in adv:
        print(f"  [advisory] {a}")
    for f in fails:
        print(f"  [FAIL] {f}")
    if dry:                                               # a scratch copy of the registries: nothing committed moves
        from kit import paths
        tmp = Path(tempfile.mkdtemp(prefix="hangwood_pal_"))
        shutil.copytree(paths.palette_root(), tmp / "palettes")
        os.environ["AF_PALETTE_ROOT"] = str(tmp / "palettes")
        os.environ["AF_EXPORT_ROOT"] = str(tmp / "export")
        try:
            res = register(fams, dry=True)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    else:
        res = register(fams, dry=False)
    MP.summary(res)
    if "--list" in argv:
        for fam, r in res.items():
            for n, i in sorted(r["names"].items(), key=lambda kv: (kv[1], kv[0])):
                print(f"  {fam}[{i}] {n}")
    g = lab_l(hex_rgb(DARKEST_GROUND))
    print(f"  darkest reference ground {DARKEST_GROUND} L* {g:.1f}; moon-tarnish hue {hsv(MOON_TARNISH)[0]:.1f} sat "
          f"{hsv(MOON_TARNISH)[1]:.2f}; sallow hue {hsv(SALLOW)[0]:.1f} sat {hsv(SALLOW)[1]:.2f}")
    print("OK" if not fails else f"{len(fails)} rule failure(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
