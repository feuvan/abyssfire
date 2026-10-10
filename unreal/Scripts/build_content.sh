#!/usr/bin/env bash
# Builds the Abyssfire editor target and runs the content build headless (macOS; Linux with a source-built engine).
#
#   unreal/Scripts/build_content.sh                       # everything (first run: ~10-30 min, mostly shader compiles)
#   unreal/Scripts/build_content.sh --only materials      # any build_content.py option is passed through
#   SKIP_EDITOR_BUILD=1 unreal/Scripts/build_content.sh   # editor target already built
#   UE_ROOT=/path/to/UE_5.8 unreal/Scripts/build_content.sh
#
# Output: unreal/Content/Abyssfire/**, the report unreal/Saved/ContentBuild/build_report.{json,md} and the editor log
# unreal/Saved/ContentBuild/build_content.log. Exit status 1 when the report says "ok": false, the log has a Python
# error, or L_Main was not written (the pythonscript commandlet itself can exit 0 after a Python exception).
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
unreal_dir="$(cd "$here/.." && pwd)"
project="$unreal_dir/Abyssfire.uproject"

case "$(uname -s)" in
  Darwin) host=Mac; ue_default="/Users/Shared/Epic Games/UE_5.8" ;;
  Linux) host=Linux; ue_default="$HOME/UnrealEngine" ;;
  *) echo "build_content.sh: unsupported host $(uname -s) (on Windows run UnrealEditor-Cmd.exe, see Scripts/README.md)" >&2
     exit 2 ;;
esac
ue_root="${UE_ROOT:-$ue_default}"
if [[ ! -d "$ue_root/Engine" ]]; then
  echo "build_content.sh: Unreal Engine not found at '$ue_root' (set UE_ROOT; install UE 5.8.3, Scripts/README.md)" >&2
  exit 2
fi
bin="$ue_root/Engine/Binaries/$host"
if [[ -x "$bin/UnrealEditor-Cmd" ]]; then
  editor="$bin/UnrealEditor-Cmd"
elif [[ "$host" == Mac ]]; then
  editor="$bin/UnrealEditor.app/Contents/MacOS/UnrealEditor"
else
  editor="$bin/UnrealEditor"
fi

if [[ -z "${SKIP_EDITOR_BUILD:-}" ]]; then
  echo "== building AbyssfireEditor ($host Development)"
  "$ue_root/Engine/Build/BatchFiles/$host/Build.sh" AbyssfireEditor "$host" Development -project="$project" -waitmutex
fi

work="$unreal_dir/Saved/ContentBuild"
mkdir -p "$work"
report="$work/build_report.json"
log="$work/build_content.log"
rm -f "$report"
export AF_CONTENT_REPORT="$report"

script_args="$here/build_content.py"
for a in "$@"; do script_args+=" $a"; done

echo "== running build_content.py ($editor)"
set +e
"$editor" "$project" -run=pythonscript "-script=$script_args" \
  -unattended -nosplash -nop4 -NoSound -stdout -FullStdOutLogOutput "-abslog=$log"
status=$?
set -e

failed=0
[[ $status -eq 0 ]] || { echo "editor exited with status $status" >&2; failed=1; }
if grep -Eq 'LogPython: Error|Traceback \(most recent call last\)' "$log" 2>/dev/null; then
  grep -E -A12 'LogPython: Error|Traceback \(most recent call last\)' "$log" | head -60 >&2
  failed=1
fi
if [[ -f "$report" ]]; then
  if grep -Eq '"ok"[[:space:]]*:[[:space:]]*false' "$report"; then failed=1; fi
  echo "report: $report (${report%.json}.md)"
else
  echo "no report written ($report)" >&2
  failed=1
fi
case " $* " in
  *" --plan "*|*" --only "*) ;;   # partial runs need not (re)write the map
  *) [[ -f "$unreal_dir/Content/Abyssfire/Maps/L_Main.umap" ]] || { echo "L_Main.umap missing" >&2; failed=1; } ;;
esac
if [[ $failed -ne 0 ]]; then
  echo "content build FAILED (log: $log)" >&2
  exit 1
fi
echo "content build OK (log: $log)"
