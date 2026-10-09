#!/usr/bin/env bash
# CI job `art:blender-smoke` — the Blender art kit smoke test (unreal/Art/blender/tests/smoke_test.py): builds a test
# knight, FBX round trip, toon-band pixel checks, preview renders. Engine-free (bpy 5.0 from PyPI, Python 3.11).
#
#   AF_BLENDER_PYTHON   interpreter with `bpy` (default: /opt/venvs/blender/bin/python if present, else python3)
#   AF_BLENDER_ARGS     extra arguments for smoke_test.py (e.g. --no-previews)
# Previews land in $AF_OUT_DIR/blender-previews (job artifact) instead of the committed Art/Previews.
set -euo pipefail
# shellcheck source=../lib/common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

py="${AF_BLENDER_PYTHON:-}"
if [[ -z "$py" ]]; then
  if [[ -x /opt/venvs/blender/bin/python ]]; then py=/opt/venvs/blender/bin/python; else py=python3; fi
fi
af_require_cmd "$py"
"$py" -c 'import sys, bpy; print("bpy", bpy.app.version_string, "python", sys.version.split()[0])' ||
  af_die "$py cannot import bpy (pip install bpy==5.0.* into a Python 3.11 environment)"

previews="$AF_OUT_DIR/blender-previews"
mkdir -p "$previews"
# shellcheck disable=SC2086 # AF_BLENDER_ARGS is a word list on purpose
EGL_PLATFORM="${EGL_PLATFORM:-surfaceless}" AF_PREVIEW_ROOT="$previews" \
  "$py" "$AF_UNREAL_DIR/Art/blender/tests/smoke_test.py" ${AF_BLENDER_ARGS:-}
af_log "Blender kit smoke test passed; previews in $previews"
