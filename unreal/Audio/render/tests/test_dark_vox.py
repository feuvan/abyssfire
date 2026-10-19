"""AU1 voices (recipes/dark_vox.py): family coverage (every Chapter 1 monster has a voice family or is silent; the
non-vocal sounds are planned cue ids), manifest naming, envelopes that end before their sources stop (no truncation
clicks), first-frame automation, determinism, no looping SFX cue, and tails trimmed to the audible sound."""

import json
import pathlib
import re

import numpy as np
import pytest

from abyss_audio import sfx
from abyss_audio.params import Param
from abyss_audio.recipes import dark_vox, port_sfx

KINDS = ("aggro", "hurt", "death")


def _all_recipes():
    for (kind, family), fn in sorted(dark_vox.VOCALS.items()):
        for v in (0, 1):
            yield f"{kind}:{family}:{v}", fn(v)
    for cue, (fn, n, _rel, _loop, _conc, _use) in sorted(dark_vox.NAMED_CUES.items()):
        for v in range(n):
            yield f"{cue}:{v}", fn(v)


RECIPES = dict(_all_recipes())


def test_every_voice_family_has_aggro_hurt_death():
    assert port_sfx.FAMILIES == ("humanoid", "slime", "undead", "human", "gnawer", "breach")
    for family in port_sfx.FAMILIES:
        for kind in KINDS:
            assert (kind, family) in port_sfx.VOCALS


def test_family_maps_point_at_known_families_and_cues():
    assert set(dark_vox.MONSTER_FAMILY.values()) <= set(dark_vox.FAMILIES)
    assert not set(dark_vox.SILENT_MONSTERS) & set(dark_vox.MONSTER_FAMILY)
    assert set(dark_vox.FAMILY_ALIASES.values()) <= set(dark_vox.FAMILIES)
    for overrides in dark_vox.NAMED_OVERRIDES.values():
        assert set(overrides.values()) <= set(dark_vox.NAMED_CUES)
        assert set(overrides) <= {"monster_aggro", "monster_hurt", "monster_death.vocal"}


def test_manifest_asset_names_are_family_major():
    import render_all

    row = {"id": "monster_aggro", "variants": 2, "families": ["humanoid", "slime"],
           "assets": ["SW_SFX_MonsterAggro_Humanoid_01", "SW_SFX_MonsterAggro_Humanoid_02",
                      "SW_SFX_MonsterAggro_Slime_01", "SW_SFX_MonsterAggro_Slime_02"]}
    families, assets = render_all.family_assets(row)
    assert families == list(port_sfx.FAMILIES)
    assert len(assets) == 2 * len(families)
    for i, family in enumerate(families):
        for v in (1, 2):
            assert assets[2 * i + v - 1] == f"SW_SFX_MonsterAggro_{render_all.pascal(family)}_{v:02d}"
    # The roar has one take per use (UE picks variants at random): the intro / P2 take and the P3 take.
    assert render_all.named_assets("vox_valdras_roar") == ["SW_SFX_VoxValdrasRoar"]
    assert render_all.named_assets("vox_valdras_roar_p3") == ["SW_SFX_VoxValdrasRoarP3"]
    assert render_all.named_assets("vox_valdras_fire") == ["SW_SFX_VoxValdrasFire"]
    for cue, (_fn, n, _rel, _loop, _conc, use) in dark_vox.NAMED_CUES.items():
        # A cue whose variants mean different things cannot be expressed (PickVariant is random): no "(v1)" in a use.
        assert n == 1 or not re.search(r"\(v\d\)", use), cue


@pytest.mark.parametrize("key", sorted(RECIPES))
def test_recipe_is_data_and_rebuilds_identically(key):
    r = RECIPES[key]
    d = r.to_json()
    assert json.loads(json.dumps(d)) == d
    assert d["layers"] and r.length > 0
    kind, *rest = key.split(":")
    v = int(rest[-1])
    again = (dark_vox.VOCALS[(kind, rest[0])](v) if (kind, rest[0]) in dark_vox.VOCALS
             else dark_vox.NAMED_CUES[kind][0](v))
    assert again.to_json() == d


def _gain_product(chain: list, t: np.ndarray) -> np.ndarray:
    g = np.ones_like(t)
    for proc in chain:
        if proc["type"] == "gain":
            g = g * Param.from_spec(proc["gain"], 1.0).values_at(t)
    return g


@pytest.mark.parametrize("key", sorted(RECIPES))
def test_layers_are_silent_when_their_sources_stop(key):
    """A source cut while its gain is still up is a click: every layer's envelope must have decayed by the time its
    last source stops (to 0.5 % of its own peak, or to createADSR's 0.001 floor that the re-used goblin / slime layers
    end on), and its first automation event must not come after the source's first frame (else that frame reads the
    param default)."""
    for i, lay in enumerate(RECIPES[key].layers):
        start = min(s["start"] for s in lay["sources"])
        stop = max(s["stop"] for s in lay["sources"])
        t = np.linspace(start, stop, 4000)
        g = np.abs(_gain_product(lay["chain"], t))
        at_stop = float(np.abs(_gain_product(lay["chain"], np.array([stop - 1e-7])))[0])
        assert at_stop <= max(5e-3 * float(g.max()), 1.001e-3), f"layer {i}: gain {at_stop:.2e} at its stop {stop}"
        for proc in lay["chain"]:
            a = proc.get("gain") if proc["type"] == "gain" else proc.get("freq")
            if isinstance(a, list):
                assert a[0][2] <= start + 1e-12, f"layer {i}: first event at {a[0][2]} after the start {start}"


def test_render_is_deterministic_and_clean():
    sr = 48000
    for key in ("hurt:undead:0", "aggro:gnawer:1", "vox_corpseherd_bell:0"):
        r = RECIPES[key]
        a = sfx.render(r, sr, sfx.seed_for("SW_TEST_" + key, 1))
        b = sfx.render(r, sr, sfx.seed_for("SW_TEST_" + key, 1))
        assert np.array_equal(a, b)
        assert np.all(np.isfinite(a))
        pk = float(np.max(np.abs(a)))
        assert pk > 1e-3
        assert abs(float(np.mean(a))) < 0.01 * pk  # no DC
        tail = a[0, -int(0.005 * sr):]
        assert float(np.sqrt(np.mean(tail ** 2))) < 1e-4 * pk  # ends in silence


def test_no_named_cue_loops():
    """EvSfx carries no stop and UAbyssAudioSystem::PlayCue ends a voice only by stealing it: a looping SFX cue would
    never end. The lullaby is hummed once."""
    assert not [cue for cue, row in dark_vox.NAMED_CUES.items() if row[3]]
    sr = 48000
    y = dark_vox.trim_tail(sfx.render(dark_vox.corpseherd_lullaby(), sr, sfx.seed_for("SW_SFX_VoxCorpseherdLullaby", 1)),
                           sr)
    assert 15.0 < y.shape[1] / sr < dark_vox.LULLABY_SEC
    pk = float(np.max(np.abs(y)))
    assert float(np.max(np.abs(y[:, :int(0.015 * sr)]))) < 1e-3 * pk  # no click on the first frame
    assert y[0, -1] == 0.0


def test_trim_tail_cuts_inaudible_padding():
    sr = 48000
    t = np.arange(int(0.6 * sr)) / sr
    y = (np.sin(2 * np.pi * 220 * t) * np.exp(-t / 0.02) * (t < 0.2))[None, :]  # 0.2 s of sound, 0.4 s of nothing
    z = dark_vox.trim_tail(y, sr)
    pk = float(np.max(np.abs(y)))
    last = int(np.nonzero(np.abs(y[0]) > pk * 10 ** (dark_vox.TRIM_DB / 20))[0][-1])
    assert z.shape[1] == last + 1 + int(dark_vox.TRIM_FADE_SEC * sr)
    assert np.array_equal(z[:, :last - int(dark_vox.TRIM_FADE_SEC * sr)], y[:, :last - int(dark_vox.TRIM_FADE_SEC * sr)])
    assert z[0, -1] == 0.0
    for key in ("hurt:undead:0", "vox_valdras_hurt:0", "vox_valdras_standard:1"):
        y = sfx.render(RECIPES[key], sr, 7)
        z = dark_vox.trim_tail(y, sr)
        tail = np.max(np.abs(z[:, -int(0.03 * sr):]))
        assert z.shape[1] < y.shape[1] and tail > 1e-6 * float(np.max(np.abs(z)))  # cut, but right after the sound


MONSTERS_MD = pathlib.Path(__file__).resolve().parents[3] / "Docs" / "dark-remake" / "monsters.md"


def _chapter1_monster_ids() -> set[str]:
    """Every monster id monsters.md section 2 defines (#### headings and the stretch table)."""
    text = MONSTERS_MD.read_text(encoding="utf-8")
    sec = text[text.index("## 2. Chapter 1 roster"):text.index("## 3.")]
    ids = set(re.findall(r"^#### `([a-z_0-9]+)`", sec, re.M))
    ids |= set(re.findall(r"^### 2\.\d+ [^`\n]*`([a-z_0-9]+)`", sec, re.M))  # the mini-boss and the act boss
    stretch = sec[sec.index("### 2.7"):]
    ids |= set(re.findall(r"^\| `([a-z_0-9]+)` \|", stretch, re.M))
    return ids


@pytest.mark.skipif(not MONSTERS_MD.exists(), reason="design docs not in this checkout")
def test_every_chapter1_monster_has_a_voice_or_is_silent():
    ids = _chapter1_monster_ids()
    assert {"risen_footman", "valdras", "miniboss_corpseherd", "false_lantern", "ash_outrider"} <= ids
    voiced = set(dark_vox.MONSTER_FAMILY) | set(dark_vox.SILENT_MONSTERS)
    assert ids <= voiced, sorted(ids - voiced)


def test_planned_cues_are_reserved_for_known_monsters():
    known = set(dark_vox.MONSTER_FAMILY) | set(dark_vox.SILENT_MONSTERS)
    assert not set(dark_vox.PLANNED_CUES) & set(dark_vox.NAMED_CUES)
    for cue, (monsters, sound, wave, spatial) in dark_vox.PLANNED_CUES.items():
        assert re.fullmatch(r"sfx_[a-z_]+", cue) and sound and spatial in ("2d", "3d")
        assert wave in ("W1", "W2", "W3", "stretch") and set(monsters) <= known, cue
    covered = {m for monsters, *_ in dark_vox.PLANNED_CUES.values() for m in monsters}
    # monsters.md gives these a non-vocal sound (crank, clang, horn, flail, flee hiss, drum, bite, clapper, fuse...).
    assert {"risen_footman", "risen_arbalest", "risen_shieldbearer", "dead_standard_bearer", "oath_zealot", "deserter",
            "deserter_drummer", "gravegnawer", "bonegorger", "cinderbloat", "false_lantern"} <= covered
