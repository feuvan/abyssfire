"""Import the rendered audio into the UE project as SoundWave assets (audio.md 9.7; DECISIONS A1).

Runs inside the Unreal Editor's Python (CPython 3.11, standard library only). Reads unreal/Audio/Export/
audio_manifest.json (written by unreal/Audio/render/render_all.py) and, for every asset, imports Export/<file> into
/Game/Abyssfire/Audio/<Folder>/<Name> and applies the playback settings UAbyssAudioSystem relies on:

    kind                    looping        loading behaviour   compression                virtualisation
    sfx / vocal / footstep  no             RETAIN_ON_LOAD      ADPCM (instant start)      engine default
    stinger                 from manifest  PRIME_ON_LOAD       PLATFORM_SPECIFIC q80      PLAY_WHEN_SILENT
    music / ambience        from manifest  LOAD_ON_DEMAND      PLATFORM_SPECIFIC q80/q70  PLAY_WHEN_SILENT

Music, ambience and the whisper bed must keep running while silent (music volume 0, ducking, fade-in from 0):
UAbyssAudioSystem follows their play position in real time to resume a track where it was left (A2) and to drive the
soundtrack's progress bar.

Idempotent: an asset whose source file is unchanged (SHA-256 kept in the asset's metadata tag AbyssSourceSha256) is not
re-imported, only its settings are re-applied. UE imports .ogg directly (decoded to 16-bit PCM inside the asset); if
an engine build refuses it, convert with unreal/Audio/render/export_wav.py and pass --source-root / --source-ext wav.

Headless (after building AbyssfireEditor):
    UnrealEditor-Cmd <repo>/unreal/Abyssfire.uproject -run=pythonscript \
        -script="<repo>/unreal/Audio/ue/import_audio.py" -unattended -nosplash -stdout -FullStdOutLogOutput
    (Mac: .../UnrealEditor.app/Contents/MacOS/UnrealEditor with the same arguments.)
Arguments go inside the -script string: -script="<path>/import_audio.py --force --prune".
From another editor script (Scripts/build_content.py):
    import importlib.util, sys; spec = importlib.util.spec_from_file_location("import_audio", "<path>/import_audio.py")
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod); mod.run()

Options:
    --manifest PATH      another audio_manifest.json (default: <project>/Audio/Export/audio_manifest.json)
    --only KINDS         comma list of kinds: sfx, vocal, footstep, ambience, stinger, music
    --force              re-import every asset even when its source is unchanged
    --prune              delete SoundWaves under the audio root that the manifest no longer lists
    --source-root DIR    import <DIR>/<Folder>/<Name>.<ext> instead of the manifest's Export files
    --source-ext EXT     extension used with --source-root (default ogg; wav for export_wav.py output)
    --dry-run            report what would be imported, change nothing
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys

import unreal

SHA_TAG = "AbyssSourceSha256"
LENGTH_TOLERANCE_SEC = 0.03

# kind -> (loading behaviour, compression type, compression quality, play when silent)
KIND_SETTINGS = {
    "sfx": ("RETAIN_ON_LOAD", "ADPCM", None, False),
    "vocal": ("RETAIN_ON_LOAD", "ADPCM", None, False),
    "footstep": ("RETAIN_ON_LOAD", "ADPCM", None, False),
    "stinger": ("PRIME_ON_LOAD", "PLATFORM_SPECIFIC", 80, True),
    "music": ("LOAD_ON_DEMAND", "PLATFORM_SPECIFIC", 80, True),
    "ambience": ("LOAD_ON_DEMAND", "PLATFORM_SPECIFIC", 70, True),
}


class ImportError_(RuntimeError):
    pass


# ---------------------------------------------------------------------------------------------------------------------
# Editor asset API (EditorAssetSubsystem on 5.x, EditorAssetLibrary as the fallback)
# ---------------------------------------------------------------------------------------------------------------------
class Assets:
    def __init__(self) -> None:
        sub = None
        if hasattr(unreal, "EditorAssetSubsystem"):
            try:
                sub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
            except Exception:  # noqa: BLE001 - older builds / commandlet without the subsystem
                sub = None
        self.sub = sub
        self.lib = unreal.EditorAssetLibrary

    def exists(self, path: str) -> bool:
        return bool((self.sub or self.lib).does_asset_exist(path))

    def load(self, path: str):
        return (self.sub or self.lib).load_asset(path)

    def get_tag(self, obj, tag: str) -> str:
        try:
            return str((self.sub or self.lib).get_metadata_tag(obj, tag) or "")
        except Exception:  # noqa: BLE001
            return ""

    def set_tag(self, obj, tag: str, value: str) -> None:
        (self.sub or self.lib).set_metadata_tag(obj, tag, value)

    def save(self, obj) -> bool:
        return bool((self.sub or self.lib).save_loaded_asset(obj, True))

    def list(self, directory: str) -> list[str]:
        if not (self.sub or self.lib).does_directory_exist(directory):
            return []
        return list((self.sub or self.lib).list_assets(directory, True, False))

    def delete(self, path: str) -> bool:
        return bool((self.sub or self.lib).delete_asset(path))


# ---------------------------------------------------------------------------------------------------------------------
def project_dir() -> str:
    try:
        d = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
        if d and os.path.isdir(d):
            return d
    except Exception:  # noqa: BLE001
        pass
    # unreal/Audio/ue/import_audio.py -> unreal/
    return os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir, os.pardir))


def file_sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def split_object_path(ue_path: str) -> tuple[str, str]:
    """'/Game/Abyssfire/Audio/SFX/SW_X.SW_X' -> ('/Game/Abyssfire/Audio/SFX', 'SW_X')."""
    package = ue_path.split(".", 1)[0]
    folder, name = package.rsplit("/", 1)
    return folder, name


def enum_value(enum_name: str, member: str):
    enum = getattr(unreal, enum_name, None)
    if enum is None or not hasattr(enum, member):
        return None
    return getattr(enum, member)


def set_prop(asset, name: str, value, required: bool, notes: list[str]) -> bool:
    """set_editor_property (fires PostEditChangeProperty) only when the value differs; True when it changed."""
    try:
        current = asset.get_editor_property(name)
        if current == value:
            return False
        asset.set_editor_property(name, value)
        return True
    except Exception as e:  # noqa: BLE001
        if required:
            raise ImportError_(f"{asset.get_name()}: cannot set {name} = {value}: {e}") from e
        notes.append(f"{asset.get_name()}: {name} not set ({e})")
        return False


def apply_settings(asset, entry: dict, notes: list[str]) -> bool:
    kind = entry.get("kind", "sfx")
    loading, compression, quality, play_when_silent = KIND_SETTINGS.get(kind, KIND_SETTINGS["sfx"])
    changed = False
    changed |= set_prop(asset, "looping", bool(entry.get("loop", False)), True, notes)
    lb = enum_value("SoundWaveLoadingBehavior", loading)
    if lb is None:
        notes.append(f"{asset.get_name()}: SoundWaveLoadingBehavior.{loading} unknown in this engine")
    else:
        changed |= set_prop(asset, "loading_behavior", lb, True, notes)
    ct = enum_value("SoundAssetCompressionType", compression)
    if ct is None:
        notes.append(f"{asset.get_name()}: SoundAssetCompressionType.{compression} unknown in this engine")
    else:
        changed |= set_prop(asset, "sound_asset_compression_type", ct, False, notes)
    if quality is not None:
        changed |= set_prop(asset, "compression_quality", int(quality), False, notes)
    if play_when_silent:
        vm = enum_value("VirtualizationMode", "PLAY_WHEN_SILENT")
        if vm is None:
            notes.append(f"{asset.get_name()}: VirtualizationMode.PLAY_WHEN_SILENT unknown in this engine")
        else:
            changed |= set_prop(asset, "virtualization_mode", vm, False, notes)
    return changed


def verify(asset, entry: dict, notes: list[str]) -> None:
    """The imported wave must match the render: length (loop = whole file) and channel count."""
    name = asset.get_name()
    try:
        duration = float(asset.get_editor_property("duration"))
        want = float(entry.get("lengthSec", 0.0))
        if want > 0 and abs(duration - want) > LENGTH_TOLERANCE_SEC:
            notes.append(f"{name}: duration {duration:.3f} s, manifest {want:.3f} s")
    except Exception as e:  # noqa: BLE001
        notes.append(f"{name}: duration not readable ({e})")
    try:
        channels = int(asset.get_editor_property("num_channels"))
        if channels != int(entry.get("channels", channels)):
            notes.append(f"{name}: {channels} channel(s), manifest {entry.get('channels')}")
    except Exception:  # noqa: BLE001 - informational only
        pass


def import_one(src: str, folder: str, name: str) -> str:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    if hasattr(unreal, "SoundFactory"):
        # The legacy sound factory handles wav / ogg / flac / aif / opus; Interchange does not import audio.
        task.set_editor_property("factory", unreal.SoundFactory())
    for prop, value in (("replace_existing_settings", False), ("async_", False)):
        try:
            task.set_editor_property(prop, value)
        except Exception:  # noqa: BLE001 - property absent in this engine version
            pass
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths") or [])
    if not paths:
        raise ImportError_(f"import failed: {src} -> {folder}/{name}")
    return str(paths[0])


# ---------------------------------------------------------------------------------------------------------------------
def run(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="import_audio.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--manifest", default="")
    ap.add_argument("--only", default="")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--prune", action="store_true")
    ap.add_argument("--source-root", default="")
    ap.add_argument("--source-ext", default="ogg")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv if argv is not None else [])

    root = project_dir()
    manifest_path = args.manifest or os.path.join(root, "Audio", "Export", "audio_manifest.json")
    if not os.path.isfile(manifest_path):
        raise ImportError_(f"no audio manifest at {manifest_path} (run unreal/Audio/render/render_all.py)")
    with open(manifest_path, encoding="utf-8") as f:
        manifest = json.load(f)
    if manifest.get("schemaVersion") != 1:
        raise ImportError_(f"{manifest_path}: unsupported schemaVersion {manifest.get('schemaVersion')}")
    export_dir = os.path.dirname(os.path.abspath(manifest_path))
    ue_root = manifest.get("ueRoot", "/Game/Abyssfire/Audio")
    only = {k.strip() for k in args.only.split(",") if k.strip()}

    assets = Assets()
    imported, skipped, resaved, errors, notes = [], [], [], [], []
    entries = manifest.get("assets", {})
    with unreal.ScopedSlowTask(len(entries), "Importing Abyssfire audio") as slow:
        slow.make_dialog(True)
        for asset_name, entry in sorted(entries.items()):
            if slow.should_cancel():
                raise ImportError_("cancelled")
            slow.enter_progress_frame(1, asset_name)
            if only and entry.get("kind") not in only:
                continue
            try:
                folder, name = split_object_path(entry["ue"])
                if args.source_root:
                    sub = entry["file"].rsplit("/", 1)[0]
                    src = os.path.join(args.source_root, sub, f"{name}.{args.source_ext}")
                    want_sha = file_sha256(src) if os.path.isfile(src) else ""
                else:
                    src = os.path.join(export_dir, entry["file"])
                    want_sha = entry.get("sha256", "")
                if not os.path.isfile(src):
                    raise ImportError_(f"source missing: {src}")
                object_path = f"{folder}/{name}.{name}"
                exists = assets.exists(object_path)
                asset = assets.load(object_path) if exists else None
                unchanged = asset is not None and want_sha and assets.get_tag(asset, SHA_TAG) == want_sha
                if args.dry_run:
                    (skipped if unchanged and not args.force else imported).append(name)
                    continue
                if not unchanged or args.force:
                    import_one(src, folder, name)
                    asset = assets.load(object_path)
                    if asset is None:
                        raise ImportError_(f"imported but not loadable: {object_path}")
                    if want_sha:
                        assets.set_tag(asset, SHA_TAG, want_sha)
                    apply_settings(asset, entry, notes)
                    verify(asset, entry, notes)
                    assets.save(asset)
                    imported.append(name)
                else:
                    if apply_settings(asset, entry, notes):
                        assets.save(asset)
                        resaved.append(name)
                    else:
                        skipped.append(name)
            except Exception as e:  # noqa: BLE001 - collect, report, fail at the end
                errors.append(f"{asset_name}: {e}")

    pruned = []
    if args.prune and not only:
        listed = {e["ue"].split(".", 1)[0] for e in entries.values()}
        for path in assets.list(ue_root):
            package = path.split(".", 1)[0]
            if package not in listed:
                obj = assets.load(path)
                if obj is not None and isinstance(obj, unreal.SoundWave):
                    if args.dry_run or assets.delete(package):
                        pruned.append(package)

    for line in notes:
        unreal.log_warning(f"[import_audio] {line}")
    unreal.log(f"[import_audio] {manifest_path}: imported {len(imported)}, settings updated {len(resaved)}, "
               f"unchanged {len(skipped)}, pruned {len(pruned)}, errors {len(errors)}"
               + (" (dry run)" if args.dry_run else ""))
    for line in errors:
        unreal.log_error(f"[import_audio] {line}")
    if errors:
        raise ImportError_(f"{len(errors)} audio asset(s) failed; see the log")
    return 0


if __name__ == "__main__":
    # -run=pythonscript -script="import_audio.py --force" puts the words after the script path in sys.argv[1:].
    run(sys.argv[1:])
