"""Audio step: runs the audio agent's importer (unreal/Audio/ue/import_audio.py, audio.md 9.7) in-process, so one
content build produces everything. That script owns the SoundWave settings (looping, loading behaviour, compression,
virtualisation) and its own idempotency (source SHA-256 tags).
"""
from __future__ import annotations

import importlib.util
import sys

from .. import paths
from ..report import BuildReport


def import_audio(report: BuildReport, force: bool, prune: bool, extra: list[str]) -> None:
    script = paths.AUDIO_IMPORT_SCRIPT
    if not script.is_file():
        report.warning(f"{script} not found: audio skipped")
        return
    manifest = paths.UNREAL_DIR / "Audio" / "Export" / "audio_manifest.json"
    if not manifest.is_file():
        report.warning(f"{manifest} not found (render the audio, unreal/Audio/README.md): audio skipped")
        return
    spec = importlib.util.spec_from_file_location("abyss_import_audio", script)
    if spec is None or spec.loader is None:
        report.error(f"cannot load {script}")
        return
    module = importlib.util.module_from_spec(spec)
    sys.modules["abyss_import_audio"] = module
    spec.loader.exec_module(module)
    argv = list(extra)
    if force and "--force" not in argv:
        argv.append("--force")
    if prune and "--prune" not in argv:
        argv.append("--prune")
    try:
        module.run(argv)
        report.note(f"import_audio.py {' '.join(argv)}: OK (details: LogPython '[import_audio]' lines)")
    except Exception as e:  # noqa: BLE001 - the importer raises after logging every failed asset
        report.error(f"audio import: {e}")
