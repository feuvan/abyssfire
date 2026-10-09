#!/usr/bin/env bash
# CI job `ue:content` (self-hosted macOS runner) — builds the editor target, then runs the content build
# (unreal/Scripts/build_content.py: textures -> meshes -> animations -> materials -> audio -> L_Main, ue58-platform.md
# §11) headless. Output: unreal/Content/ (job artifact reused by every package job) + $AF_OUT_DIR/content/.
#
#   AF_CONTENT_ARGS        extra arguments for build_content.py (e.g. "--family heroes")
#   AF_EDITOR_EXTRA_ARGS   extra editor arguments (e.g. -nullrhi)
#   AF_CONTENT_REPORT      exported to the script: where build_content.py should write its JSON report
#                          ($AF_OUT_DIR/content/report.json); a report with "ok": false fails the job
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/common.sh
source "$here/../lib/common.sh"
# shellcheck source=../lib/ue.sh
source "$here/../lib/ue.sh"
trap 'af_ue_collect_logs; af_cleanup_overlays' EXIT

# the editor, the project's Python and their crash dumps get no signing secret
# shellcheck disable=SC2119 # no secret to keep
af_unset_secrets
af_ue_init
af_load_version
out="$AF_OUT_DIR/content"
mkdir -p "$out"

script="$AF_UNREAL_DIR/Scripts/build_content.py"
[[ -f "$script" ]] || af_die "$script does not exist yet (the content build script, ue58-platform.md §11)"

af_section_start editor "Build AbyssfireEditor ($AF_UE_HOST Development)"
af_ue_run "$AF_UBT_BUILD" AbyssfireEditor "$AF_UE_HOST" Development -project="$AF_UPROJECT" -waitmutex
af_section_end editor

af_section_start content "build_content.py"
export AF_CONTENT_REPORT="$out/report.json" AF_CI=1
rm -f "$AF_CONTENT_REPORT"
log="$out/build_content.log"
editor_args=(
  "$AF_UPROJECT"
  -run=pythonscript
  "-script=$script${AF_CONTENT_ARGS:+ $AF_CONTENT_ARGS}"
  -unattended -nosplash -nop4 -NoSound -stdout -FullStdOutLogOutput -NoLogTimes
  "-abslog=$log"
)
if [[ -n "${AF_EDITOR_EXTRA_ARGS:-}" ]]; then
  # shellcheck disable=SC2206 # deliberate word splitting of a user-provided argument list
  editor_args+=(${AF_EDITOR_EXTRA_ARGS})
fi
set +e
af_ue_run "$AF_EDITOR_CMD" "${editor_args[@]}"
status=$?
set -e
af_section_end content

# The pythonscript commandlet can exit 0 after a Python exception: check the log and the script's report too.
failed=0
if [[ $status -ne 0 ]]; then
  af_warn "editor exited with $status"
  failed=1
fi
if [[ -f "$log" ]] && grep -Eq 'LogPython: Error|Traceback \(most recent call last\)' "$log"; then
  grep -E -A12 'LogPython: Error|Traceback \(most recent call last\)' "$log" | head -80 >&2 || true
  af_warn "Python errors in $log"
  failed=1
fi
if [[ -f "$AF_CONTENT_REPORT" ]]; then
  if grep -Eq '"(ok|success)"[[:space:]]*:[[:space:]]*false' "$AF_CONTENT_REPORT"; then
    af_warn "build_content.py reported failure ($AF_CONTENT_REPORT)"
    failed=1
  fi
else
  af_warn "build_content.py wrote no report at \$AF_CONTENT_REPORT; relying on the exit code and the log"
fi
map="$AF_UNREAL_DIR/Content/Abyssfire/Maps/L_Main.umap"
if [[ ! -f "$map" ]]; then
  af_warn "$map was not created"
  failed=1
fi
if [[ $failed -ne 0 ]]; then af_die "content build failed"; fi

{
  echo "version: $AF_VERSION"
  echo "engine: $UE_ROOT"
  echo "assets: $(find "$AF_UNREAL_DIR/Content" -name '*.uasset' | wc -l | tr -d ' ') .uasset, $(find "$AF_UNREAL_DIR/Content" -name '*.umap' | wc -l | tr -d ' ') .umap"
  echo "size: $(du -sh "$AF_UNREAL_DIR/Content" | awk '{print $1}')"
} | tee "$out/summary.txt"
af_log "content build OK"
