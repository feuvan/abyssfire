#!/usr/bin/env python3
"""Fetch a generated UE Python API stub (PyPI `unreal-stub`, generated from a UE 5.6 editor) into Scripts/.cache, so
tests/test_api_names.py can check every editor-API name the content build uses without an Unreal install.

    python3 unreal/Scripts/tests/fetch_ue_stub.py          # -> unreal/Scripts/.cache/unreal_stub/unreal.py
    UE_PY_STUB=/path/to/unreal.py python3 -m unittest ...   # or point at the stub your own editor writes
                                                           # (Editor Preferences > Python > Developer Mode:
                                                           #  <project>/Intermediate/PythonStub/unreal.py)

The newest stub of the exact engine (5.8.3) is the one the editor writes itself; prefer it once UE is installed.
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

CACHE = Path(__file__).resolve().parent.parent / ".cache" / "unreal_stub"
PACKAGE = "unreal-stub==0.3"   # "Python stub for Unreal Engine 5 API - latest: 5.6.0"


def main() -> int:
    CACHE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([sys.executable, "-m", "pip", "download", "--no-deps", "--only-binary=:all:", "-d", tmp, PACKAGE],
                       check=True)
        wheels = list(Path(tmp).glob("*.whl"))
        if not wheels:
            print("no wheel downloaded", file=sys.stderr)
            return 1
        with zipfile.ZipFile(wheels[0]) as zf:
            data = zf.read("unreal/unreal.py")
    out = CACHE / "unreal.py"
    out.write_bytes(data)
    print(f"wrote {out} ({len(data) // 1024} KiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
