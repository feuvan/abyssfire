"""Validate ``Art/Export/VFX/vfx_recipes.json`` against what the UE runtime parses and plays.

    python3 unreal/Art/blender/vfx/fx_validate.py            (pure Python; exit 1 on any error)

Mirrors ``FAbyssVfxLibrary::MergeJson / ParseRecipe / ParseLayer`` (Source/Abyssfire/Private/Vfx/AbyssVfxRecipe.cpp)
— a field of the wrong JSON type is *silently ignored* there (ReadFloat / ReadBool / ReadRange), an unknown enum
value skips the whole recipe — so every key, type and enum is checked here instead. Then the content checks:

* every sprite is a ``T_FX_<sprite>.png`` in Export/VFX whose cell count matches the sprite table (strip width =
  cells × height for the square-cell strips), every ``variant`` < cells, every mesh an exported ``SM_*`` FBX
  (``""`` clears a preset mesh);
* coverage: every recipe id the runtime binds (WorldContract 5.3: combat feedback, rewards, utility, world, props,
  statuses) and every one of the 40 skills (``skill.<id>``, the projectile heads / impacts of projectile skills, the
  ground loops / triggers of ground-effect skills, from ``Data/classes.json``);
* runtime limits: count ≤ 200, rate ≤ 400 (clamped there), a rough particle budget per recipe (the web capped the
  whole engine at 900 live particles).
"""
from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
UNREAL = HERE.parent.parent.parent
EXPORT = UNREAL / "Art" / "Export"
DOC = EXPORT / "VFX" / "vfx_recipes.json"
CLASSES = UNREAL / "Data" / "classes.json"

PRESETS = {"glow", "flash", "glint", "sparks", "streak", "motes", "flames", "smoke", "debris", "ring", "shock",
           "decal", "slash", "swipe", "beam", "gather", "bolt", "mesh"}
PRESET_SPRITE = {"glow": "Glow", "flash": "Core", "glint": "Spark", "sparks": "Streak", "streak": "Streak",
                 "motes": "Ember", "flames": "Flame", "smoke": "Smoke", "debris": "Rock", "ring": "Ring",
                 "shock": "Shock", "decal": "Scorch", "slash": "Slash", "swipe": "Slash", "beam": "Beam",
                 "gather": "Glow", "bolt": "Bolt", "mesh": "Glow"}
ORIENT = {"billboard", "ground", "velocity", "upright", "beam"}
ANCHOR = {"origin", "source", "caster", "target", "point", "points", "path", "camera"}
HEIGHT = {"ground", "feet", "chest", "head", "overhead", "hand"}
DIRS = {"random", "up", "down", "blow", "back", "out", "in", "flat", "tangent"}
UNITS = {"cm", "ring", "radius"}
COLOR_SLOTS = {"core", "mid", "rim", "dark", "event", "white"}
BUILTIN_PALETTES = {"fire", "frost", "lightning", "poison", "shadow", "holy", "steel", "blood", "rage", "arcane",
                    "nature", "earth"}
HEX = re.compile(r"^#?[0-9A-Fa-f]{6}$")

NUM = "number"
RANGE = "range"
BOOL = "bool"
LAYER_KEYS = {
    "emitter": "str", "sprite": "str", "mesh": "str", "blend": "str", "orient": "str", "variant": NUM,
    "randomVariants": NUM, "at": "str", "height": "height", "offsetCm": "vec3", "beamTo": "str",
    "beamToHeight": "height", "spawnRadiusCm": NUM, "onRing": BOOL, "spawnHeightJitterCm": NUM, "unit": "str",
    "attach": BOOL, "delayMs": NUM, "count": "count", "countByRadius": BOOL, "rate": NUM, "durationMs": NUM,
    "staggerMs": NUM, "lifeMs": RANGE, "speedCmS": RANGE, "dir": "str", "coneDeg": NUM, "upCmS": RANGE,
    "gravityCmS2": NUM, "drag": NUM, "orbit": "orbit", "sizeCm": RANGE, "grow": NUM, "sizePow": NUM, "aspect": NUM,
    "stretch": NUM, "alpha": "alpha", "fadeIn": NUM, "alphaPow": NUM, "flicker": NUM, "color": "color",
    "altEvery": NUM, "altColor": "color", "intensity": NUM, "rotationDeg": RANGE, "spinDegS": RANGE,
    "alignToBlow": BOOL, "only": "str",
    "fan": BOOL,       # forward-compatible (integration.fan): even yaw fan for dir blow / back bursts
    "flat": BOOL,      # forward-compatible (integration.flat): no random climb for dir blow / back
}
RECIPE_KEYS = {"palette", "layers", "arcCm", "light", "shake", "note"}

# WorldContract 5.3 bindings (first id of each chain must exist in the art file or the built-in defaults; the art
# pass authors every one of them)
EVENT_IDS = [
    "hit.impact", "hit.basic", "hit.claw", "hit.evade", "hit.mark.fire", "hit.mark.ice", "hit.mark.other",
    "death.burst", "reward.gold", "reward.heal", "reward.mana", "hero.levelup", "hero.resonance",
    "hero.resonance.aura", "hero.dodge", "hero.dash", "teleport.blink", "portal.channel", "portal.arrive",
    "loot.drop", "loot.drop.rare", "loot.drop.legendary", "loot.drop.set", "loot.pickup", "quest.pop",
    "loot.beam.magic", "loot.beam.rare", "loot.beam.legendary", "loot.beam.set",     # loot prop loops (5.3)
    "skill.default", "proj.default", "proj.default.hit", "proj.default.fizzle", "proj.monster_bolt",
    "proj.monster_bolt.hit", "proj.pet_bolt", "proj.pet_bolt.hit", "ground.default", "ground.default.trigger",
    "ground.default.end", "status.burn", "status.freeze", "status.poison", "status.bleed", "status.slow",
    "status.stun", "npc.FX_HammerStrike", "camp.campfire", "camp.torch", "exit.portal", "exit.sealed",
    "ambient.pollen", "ambient.wisps", "ambient.dust_motes", "ambient.sparks", "ambient.dust", "prop.soul_echo",
    "prop.clue", "prop.gather", "prop.lore", "prop.chest", "prop.chest.open", "prop.event",
]
PARTICLE_BUDGET = 320


class Report:
    def __init__(self):
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def err(self, msg: str) -> None:
        self.errors.append(msg)

    def warn(self, msg: str) -> None:
        self.warnings.append(msg)


def is_num(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def is_range(v) -> bool:
    return is_num(v) or (isinstance(v, list) and 1 <= len(v) <= 2 and all(is_num(x) for x in v))


def rng(v, default=(0.0, 0.0)):
    if is_num(v):
        return float(v), float(v)
    if isinstance(v, list) and v:
        a = float(v[0])
        b = float(v[1]) if len(v) > 1 else a
        return min(a, b), max(a, b)
    return default


def color_ok(v) -> bool:
    if is_num(v):
        return True
    return isinstance(v, str) and (v in COLOR_SLOTS or bool(HEX.match(v)))


def png_size(p: Path) -> tuple[int, int]:
    with open(p, "rb") as f:
        head = f.read(24)
    return struct.unpack(">II", head[16:24])


def check_layer(rid: str, i: int, L: dict, doc: dict, rep: Report) -> float:
    """Returns a rough peak particle estimate of the layer."""
    where = f"{rid} layer {i}"
    if not isinstance(L, dict):
        rep.err(f"{where}: not an object")
        return 0.0
    for k, v in L.items():
        kind = LAYER_KEYS.get(k)
        if kind is None:
            rep.err(f"{where}: unknown key {k!r} (ignored by the runtime)")
            continue
        ok = True
        if kind == "str":
            ok = isinstance(v, str)
        elif kind == NUM:
            ok = is_num(v)
        elif kind == RANGE:
            ok = is_range(v)
        elif kind == BOOL:
            ok = isinstance(v, bool)
        elif kind == "vec3":
            ok = isinstance(v, list) and len(v) >= 3 and all(is_num(x) for x in v[:3])
        elif kind == "height":
            ok = is_num(v) or (isinstance(v, str) and v in HEIGHT)
        elif kind == "count":
            ok = is_range(v) or v == "sparks"
        elif kind == "orbit":
            ok = isinstance(v, bool) or (isinstance(v, dict) and set(v) <= {"r0Cm", "r1Cm", "degS"}
                                         and all(is_range(x) for x in v.values()))
        elif kind == "alpha":
            ok = is_num(v) or (isinstance(v, list) and len(v) >= 2 and all(is_num(x) for x in v[:2]))
        elif kind == "color":
            ok = color_ok(v)
        if not ok:
            rep.err(f"{where}: {k} = {v!r} has the wrong type/value ({kind})")
    em = L.get("emitter")
    if em is not None and em not in PRESETS:
        rep.err(f"{where}: unknown emitter {em!r} (the recipe is skipped)")
    for key, allowed in (("orient", ORIENT), ("at", ANCHOR), ("beamTo", ANCHOR), ("dir", DIRS), ("unit", UNITS)):
        if key in L and L[key] not in allowed:
            rep.err(f"{where}: bad {key} {L[key]!r} (the recipe is skipped)")
    if "blend" in L and L["blend"] not in ("additive", "translucent", "normal"):
        rep.err(f"{where}: blend {L['blend']!r} reads as additive")
    if "only" in L and L["only"] not in ("big", "small"):
        rep.err(f"{where}: only {L['only']!r} is neither big nor small")
    sprite = L.get("sprite") or PRESET_SPRITE.get(em or "", "Glow")
    sprites = doc.get("sprites", {})
    if sprite not in sprites:
        rep.err(f"{where}: sprite {sprite!r} not in the sprite table")
    else:
        cells = int(sprites[sprite].get("cells", 1))
        var = L.get("variant", 0)
        if is_num(var) and not 0 <= int(var) < cells:
            rep.err(f"{where}: variant {var} out of range for {sprite} ({cells} cells)")
        rv = L.get("randomVariants", 0)
        if is_num(rv) and int(rv) > cells:
            rep.err(f"{where}: randomVariants {rv} > {cells} cells of {sprite}")
    mesh = L.get("mesh", "SM_FX_Rock_A" if em == "debris" else "")
    if mesh:
        cat = "Pickups" if mesh.startswith("SM_Pickup") else "VFX"
        if not (EXPORT / cat / f"{mesh}.fbx").exists():
            rep.err(f"{where}: mesh {mesh} has no FBX in Export/{cat}")
        if mesh not in doc.get("meshes", {}) and not mesh.startswith("SM_Pickup"):
            rep.warn(f"{where}: mesh {mesh} missing from the meshes table")
    if L.get("orient") == "beam" and L.get("at") != "points" and "beamTo" not in L and em != "bolt":
        rep.warn(f"{where}: beam without beamTo (defaults to the target)")
    if L.get("unit") in ("radius", "ring") and is_range(L.get("sizeCm", 0)) and rng(L.get("sizeCm", 0))[1] > 4.0:
        rep.err(f"{where}: sizeCm {L['sizeCm']} with unit {L['unit']} (sizes scale by the unit too: a cm value "
                f"becomes metres)")
    if L.get("unit") in ("radius", "ring") and is_num(L.get("spawnRadiusCm", 0)) and L.get("spawnRadiusCm", 0) > 3.0:
        rep.err(f"{where}: spawnRadiusCm {L['spawnRadiusCm']} with unit {L['unit']} (a cm value scaled by the unit)")
    cnt = L.get("count", 1 if em not in ("sparks", "motes", "smoke", "debris", "gather", "flames") else 6)
    c = rng(cnt, (6.0, 6.0))[1] if cnt != "sparks" else 14.0
    if c > 200:
        rep.err(f"{where}: count {c} > 200 (clamped by the runtime)")
    rate = L.get("rate", 0.0)
    if is_num(rate) and rate > 400:
        rep.err(f"{where}: rate {rate} > 400 (clamped by the runtime)")
    life = rng(L.get("lifeMs", 300))[1]
    if is_num(rate) and rate > 0:
        per = 1.0
        if L.get("countByRadius"):
            per = 3.0
        return rate * per * life / 1000.0
    if L.get("countByRadius"):
        c *= 3.0
    if L.get("at") == "points":
        c *= 4
    return c


def coverage(doc: dict, rep: Report) -> None:
    ids = set(doc["recipes"]) | set(doc.get("aliases", {}))
    for rid in EVENT_IDS:
        if rid not in ids:
            rep.err(f"coverage: no recipe {rid}")
    try:
        classes = json.loads(CLASSES.read_text())["classes"]
    except (OSError, ValueError, KeyError):
        rep.warn("coverage: Data/classes.json not readable, skill coverage skipped")
        return
    for cl in classes:
        for s in cl["skills"]:
            sid = s["id"]
            vid = s.get("vfxId") or sid
            if f"skill.{vid}" not in ids and f"skill.{sid}" not in ids:
                rep.err(f"coverage: skill {sid} has no skill.{vid}")
            port = s.get("port") or {}
            if s.get("projectile"):
                if f"proj.{vid}" not in ids:
                    rep.err(f"coverage: projectile skill {sid} has no proj.{vid} head")
                if f"proj.{vid}.hit" not in ids:
                    rep.warn(f"coverage: projectile skill {sid} has no proj.{vid}.hit (falls back)")
            if port.get("persistentGround"):
                if f"ground.{vid}" not in ids:
                    rep.err(f"coverage: ground skill {sid} has no ground.{vid}")
                if port.get("groundTrigger") == "armed" and f"ground.{vid}.trigger" not in ids:
                    rep.err(f"coverage: armed trap {sid} has no ground.{vid}.trigger")


def validate(doc: dict) -> Report:
    rep = Report()
    if doc.get("schemaVersion") != 1:
        rep.err("schemaVersion must be 1")
    pals = set(BUILTIN_PALETTES)
    for name, p in doc.get("palettes", {}).items():
        pals.add(name)
        for slot in ("core", "mid", "rim", "dark"):
            if not (isinstance(p.get(slot), str) and HEX.match(p[slot])):
                rep.err(f"palette {name}: {slot} is not #RRGGBB")
    for name, s in doc.get("sprites", {}).items():
        cells = s.get("cells", 1)
        if not (isinstance(cells, int) and cells >= 1):
            rep.err(f"sprite {name}: bad cells {cells!r}")
            continue
        tex = EXPORT / "VFX" / f"T_FX_{name}.png"
        if not tex.exists():
            rep.err(f"sprite {name}: {tex.name} missing")
            continue
        w, h = png_size(tex)
        if w % cells:
            rep.err(f"sprite {name}: {tex.name} width {w} not divisible by {cells} cells")
    for name, m in doc.get("meshes", {}).items():
        cat = "Pickups" if name.startswith("SM_Pickup") else "VFX"
        if not (EXPORT / cat / f"{name}.fbx").exists():
            rep.err(f"mesh table {name}: no FBX")
    for rid, r in doc.get("recipes", {}).items():
        if not isinstance(r, dict):
            rep.err(f"{rid}: not an object")
            continue
        for k in r:
            if k not in RECIPE_KEYS:
                rep.err(f"{rid}: unknown recipe key {k!r}")
        pal = r.get("palette", "event")
        if pal not in pals and pal not in ("element", "event"):
            rep.err(f"{rid}: unknown palette {pal!r}")
        if "light" in r:
            lt = r["light"]
            if not (isinstance(lt, dict) and is_num(lt.get("radiusCm")) and is_num(lt.get("alpha"))):
                rep.err(f"{rid}: light needs radiusCm + alpha numbers")
        if "shake" in r and not (isinstance(r["shake"], list) and len(r["shake"]) >= 2
                                 and all(is_num(x) for x in r["shake"][:2])):
            rep.err(f"{rid}: shake must be [ms, intensity]")
        layers = r.get("layers", [])
        if not layers and not r.get("light") and not r.get("shake"):
            rep.err(f"{rid}: no layers (the runtime skips it)")
        est = sum(check_layer(rid, i, L, doc, rep) for i, L in enumerate(layers))
        if est > PARTICLE_BUDGET:
            rep.warn(f"{rid}: ~{est:.0f} particles at peak (budget {PARTICLE_BUDGET})")
    for a, t in doc.get("aliases", {}).items():
        if t not in doc.get("recipes", {}):
            rep.err(f"alias {a} -> {t}: unknown target")
    coverage(doc, rep)
    return rep


def main(argv) -> int:
    doc = json.loads((Path(argv[0]) if argv else DOC).read_text())
    rep = validate(doc)
    for w in rep.warnings:
        print("warning:", w)
    for e in rep.errors:
        print("ERROR:", e)
    n = len(doc.get("recipes", {}))
    print(f"fx_validate: {n} recipes, {len(rep.errors)} errors, {len(rep.warnings)} warnings")
    return 1 if rep.errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
