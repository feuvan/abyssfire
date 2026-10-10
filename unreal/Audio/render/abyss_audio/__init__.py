"""Offline port of the web game's Web Audio synth (unreal/Docs/spec/audio.md 2, 4, 6, 9).

The package emulates exactly the Web Audio nodes the web recipes use (AudioParam automation, band-limited
oscillators, RBJ biquads as specified by Web Audio, wave shapers, buffer sources, gains, the convolver reverb and
Chromium's DynamicsCompressor), plus the procedural score (Composer / ScorePlayer), and masters the result for UE
(BS.1770 loudness, true peak, OGG Vorbis). Everything is deterministic: every random source is seeded.
"""

SAMPLE_RATE = 48000

__all__ = ["SAMPLE_RATE"]
