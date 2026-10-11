#!/usr/bin/env bash
# Builds the Abyssfire editor target and runs the content build headless (macOS; Linux with a source-built engine).
#
#   unreal/Scripts/build_content.sh                       # everything (first run: ~10-30 min, mostly shader compiles)
#   unreal/Scripts/build_content.sh --only materials      # any build_content.py option is passed through
#   unreal/Scripts/build_content.sh --shaders             # also compile every material (-AllowCommandletRendering)
#   SKIP_EDITOR_BUILD=1 unreal/Scripts/build_content.sh   # editor target already built (still built when older than
#                                                         # unreal/Source or missing; if that build fails, the content
#                                                         # build runs on the existing binary, with a warning)
#   SKIP_EDITOR_BUILD=force unreal/Scripts/build_content.sh  # never build it (a stale binary: STALE EDITOR BINARY)
#   UE_ROOT=/path/to/UE_5.8 unreal/Scripts/build_content.sh
#
# Output: unreal/Content/Abyssfire/**, the report unreal/Saved/ContentBuild/build_report.{json,md} and the editor log
# unreal/Saved/ContentBuild/build_content.log. Exit status 1 when the report says "ok": false, the log has a Python
# error, or L_Main was not written (the pythonscript commandlet itself can exit 0 after a Python exception); with
# --shaders also when the log has shader compile errors (printed either way).
#
# A -run=pythonscript commandlet renders with the NullRHI: no shader is compiled and material statistics read 0, so the
# report lists the masters as "not compiled" unless --shaders adds -AllowCommandletRendering (slower: every master and
# its permutations compile for the editor's shader platform).
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

shaders=0
for a in "$@"; do
  if [[ "$a" == "--shaders" ]]; then shaders=1; fi
done

# The content build calls C++ helpers (Source/Abyssfire/*/ContentBuild/AbyssContentBuildLibrary.*): an editor module
# binary older than the sources lacks the newest ones, so SKIP_EDITOR_BUILD=1 builds it anyway then; should that build
# fail, an existing binary is still used (the Python side reports the helpers it lacks once), and SKIP_EDITOR_BUILD=force
# skips the build without asking. The stamp marks a build this script ran (UBT leaves an up-to-date binary untouched, so
# a source file it did not need to recompile for would otherwise make every run look stale).
binaries="$unreal_dir/Binaries/$host"
stamp="$binaries/.abyss_editor_build_stamp"
editor_binary_stale() {
  local ref="" f newer
  for f in "$binaries"/UnrealEditor-Abyssfire*.dylib "$binaries"/libUnrealEditor-Abyssfire*.so \
           "$binaries"/UnrealEditor-Abyssfire*.so "$binaries"/UnrealEditor.modules; do
    if [[ -f "$f" ]] && { [[ -z "$ref" ]] || [[ "$f" -nt "$ref" ]]; }; then ref="$f"; fi
  done
  if [[ -z "$ref" ]]; then
    echo "no editor module binary in $binaries"
    return 0
  fi
  if [[ -f "$stamp" && "$stamp" -nt "$ref" ]]; then ref="$stamp"; fi
  newer="$(find "$unreal_dir/Source" -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.inl' -o -name '*.cs' \) \
           -newer "$ref" -print 2>/dev/null | head -n 1 || true)"
  if [[ -n "$newer" ]]; then
    echo "${ref#"$unreal_dir"/} is older than ${newer#"$unreal_dir"/}"
  fi
  return 0
}

build_editor=1
fallback_binary=0      # 1: a failed build falls back to the existing (stale) binary instead of ending the run
skip="${SKIP_EDITOR_BUILD:-}"
if [[ -n "$skip" ]]; then
  build_editor=0
  why="$(editor_binary_stale)"
  if [[ -n "$why" && "$skip" == force ]]; then
    echo "build_content.sh: WARNING: SKIP_EDITOR_BUILD=force: not building AbyssfireEditor although the editor binary" \
         "is stale ($why); the content build reports the AbyssContentBuildLibrary helpers it lacks" \
         "(STALE EDITOR BINARY)" >&2
  elif [[ -n "$why" ]]; then
    echo "build_content.sh: WARNING: SKIP_EDITOR_BUILD is set but the editor binary is stale ($why):" \
         "building AbyssfireEditor anyway (the content build needs the current AbyssContentBuildLibrary helpers;" \
         "SKIP_EDITOR_BUILD=force skips it)" >&2
    build_editor=1
    case "$why" in "no editor module binary"*) ;; *) fallback_binary=1 ;; esac
  fi
fi
if [[ $build_editor -eq 1 ]]; then
  echo "== building AbyssfireEditor ($host Development)"
  build_status=0
  "$ue_root/Engine/Build/BatchFiles/$host/Build.sh" AbyssfireEditor "$host" Development -project="$project" \
    -waitmutex || build_status=$?
  if [[ $build_status -eq 0 ]]; then
    mkdir -p "$binaries"
    touch "$stamp"
  elif [[ $fallback_binary -eq 1 ]]; then
    echo "build_content.sh: WARNING: the AbyssfireEditor build failed (status $build_status, UBT output above);" \
         "SKIP_EDITOR_BUILD is set, so the content build runs on the existing stale editor binary (it reports the" \
         "AbyssContentBuildLibrary helpers that binary lacks: STALE EDITOR BINARY)" >&2
  else
    echo "build_content.sh: the AbyssfireEditor build failed (status $build_status)" >&2
    exit "$build_status"
  fi
fi

work="$unreal_dir/Saved/ContentBuild"
mkdir -p "$work"
report="$work/build_report.json"
log="$work/build_content.log"
rm -f "$report"
export AF_CONTENT_REPORT="$report"

script_args="$here/build_content.py"
for a in "$@"; do script_args+=" $a"; done

editor_args=(-unattended -nosplash -nop4 -NoSound -stdout -FullStdOutLogOutput "-abslog=$log")
if [[ $shaders -eq 1 ]]; then
  editor_args+=(-AllowCommandletRendering)   # a real RHI: shaders compile, statistics and compile errors are real
fi

echo "== running build_content.py ($editor)"
set +e
"$editor" "$project" -run=pythonscript "-script=$script_args" "${editor_args[@]}"
status=$?
set -e

failed=0
[[ $status -eq 0 ]] || { echo "editor exited with status $status" >&2; failed=1; }
if grep -Eq 'LogPython: Error|Traceback \(most recent call last\)' "$log" 2>/dev/null; then
  grep -E -A12 'LogPython: Error|Traceback \(most recent call last\)' "$log" | head -60 >&2
  failed=1
fi
shader_errors="$(grep -E 'Failed to compile Material|\[SM[56]\].*error|error: .*\.usf|Material .* had errors' "$log" \
                 2>/dev/null | head -n 40 || true)"
if [[ -n "$shader_errors" ]]; then
  echo "== shader compile errors in the log (first 40 lines):" >&2
  printf '%s\n' "$shader_errors" >&2
  if [[ $shaders -eq 1 ]]; then failed=1; fi
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
