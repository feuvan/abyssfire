#!/usr/bin/env python3
"""Decode the exported OGG renders to 16-bit WAV for engines / tools that do not import OGG Vorbis.

UE 5 imports .ogg directly (unreal/Audio/ue/import_audio.py); this is the fallback:

    /opt/venvs/audio/bin/python unreal/Audio/render/export_wav.py [--out DIR]
    UnrealEditor-Cmd ... -script="<repo>/unreal/Audio/ue/import_audio.py --source-root DIR --source-ext wav"

Writes <DIR>/<Folder>/<Name>.wav (default DIR: unreal/Audio/Intermediate/Wav, git-ignored) with TPDF dither to
16 bit (UE stores imported waves as 16-bit PCM and does not dither).
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
import zlib

import numpy as np
import soundfile as sf

HERE = pathlib.Path(__file__).resolve().parent
EXPORT = HERE.parent / "Export"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=str(HERE.parent / "Intermediate" / "Wav"))
    ap.add_argument("--manifest", default=str(EXPORT / "audio_manifest.json"))
    args = ap.parse_args()
    manifest_path = pathlib.Path(args.manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    out_root = pathlib.Path(args.out)
    count = 0
    for name, entry in sorted(manifest["assets"].items()):
        src = manifest_path.parent / entry["file"]
        x, sr = sf.read(str(src), dtype="float64", always_2d=True)
        # Seeded TPDF dither (+-1 LSB) so the WAVs are reproducible.
        rng = np.random.Generator(np.random.PCG64(zlib.crc32(name.encode("utf-8"))))
        lsb = 1.0 / 32768.0
        y = x + (rng.random(x.shape) - rng.random(x.shape)) * lsb
        dst = out_root / entry["file"].rsplit("/", 1)[0] / f"{name}.wav"
        dst.parent.mkdir(parents=True, exist_ok=True)
        sf.write(str(dst), np.clip(y, -1.0, 1.0 - lsb), sr, subtype="PCM_16")
        count += 1
    print(f"{count} WAV files in {out_root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
