# shellcheck shell=bash
# Unreal Engine helpers for the self-hosted macOS runner (or a Linux UE runner for Android); sourced after common.sh.
#
#   UE_ROOT_MAC            engine root (default "/Users/Shared/Epic Games/UE_5.8", Epic launcher install)
#   UE_ROOT_LINUX          engine root on a Linux runner (default ~/UnrealEngine, source build)
#   AF_UE_MIN_PATCH        lowest accepted 5.8.x hotfix (default 3; DECISIONS P1)
#   AF_XCODE_DEVELOPER_DIR Xcode to use (exported as DEVELOPER_DIR; default: xcode-select's)
#   AF_PERSISTENT_DDC      absolute path of the runner's persistent local DerivedDataCache, passed to every engine
#                          process as UE-LocalDataCachePath. Mandatory in CI (set it in the runner's config.toml);
#                          outside CI the default is unreal/DerivedDataCache.
#   AF_ZEN_DATA_DIR        optional: data directory of the Zen storage server (UE 5.4+ local DDC), passed as
#                          UE-ZenDataPath [Verify on 5.8.3]; default: the engine's (the runner user's profile)
#   AF_CLEAN_BUILD=1       delete Binaries / Intermediate / Saved before building (also done automatically when the
#                          engine, Xcode or the NDK changed since the last job: Intermediate/.af-toolchain-stamp)
#   AF_UE_EXTRA_ARGS       extra arguments for every RunUAT BuildCookRun call (word-split)

# Restores every file registered with af_overlay_ini / af_track_tmp when the script exits.
AF_RESTORE_FILES=()
AF_TMP_PATHS=()
af_cleanup_overlays() {
  local f
  if [[ ${#AF_RESTORE_FILES[@]} -gt 0 ]]; then
    for f in "${AF_RESTORE_FILES[@]}"; do
      if [[ -f "$f.af-ci-backup" ]]; then
        mv -f "$f.af-ci-backup" "$f"
      elif [[ -f "$f.af-ci-created" ]]; then
        rm -f "$f" "$f.af-ci-created"
      fi
    done
  fi
  if [[ ${#AF_TMP_PATHS[@]} -gt 0 ]]; then
    for f in "${AF_TMP_PATHS[@]}"; do rm -rf "$f"; done
  fi
  AF_RESTORE_FILES=()
  AF_TMP_PATHS=()
}
af_track_tmp() { AF_TMP_PATHS+=("$1"); }

# af_overlay_ini FILE SECTION KEY=VALUE...
# Appends a section to an ini of the CI working copy only (backed up first and restored on exit; never committed).
# UE merges a repeated [Section] and the later key wins.
af_overlay_ini() {
  local file="$1" section="$2" kv
  shift 2
  if [[ ! -f "$file.af-ci-backup" && ! -f "$file.af-ci-created" ]]; then
    if [[ -f "$file" ]]; then cp -p "$file" "$file.af-ci-backup"; else
      mkdir -p "$(dirname "$file")"
      : >"$file"
      : >"$file.af-ci-created"
    fi
    AF_RESTORE_FILES+=("$file")
  fi
  {
    printf '\n; --- added by unreal/CI for this build only (restored afterwards) ---\n%s\n' "$section"
    for kv in "$@"; do printf '%s\n' "$kv"; done
  } >>"$file"
}

af_ue_init() {
  case "$(uname -s)" in
    Darwin)
      AF_UE_HOST=Mac
      UE_ROOT="${UE_ROOT_MAC:-/Users/Shared/Epic Games/UE_5.8}"
      ;;
    Linux) # a self-hosted Linux UE runner can take over Android packaging (UE_ANDROID_RUNNER_TAG)
      AF_UE_HOST=Linux
      UE_ROOT="${UE_ROOT_LINUX:-$HOME/UnrealEngine}"
      ;;
    *) af_die "unsupported UE host $(uname -s) (Windows runners use unreal/CI/windows/ue-package.ps1)" ;;
  esac
  [[ -d "$UE_ROOT/Engine" ]] ||
    af_die "Unreal Engine not found at '$UE_ROOT' (UE_ROOT_MAC / UE_ROOT_LINUX; install UE 5.8.3, see unreal/Docs/CI.md)"
  AF_UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
  AF_UBT_BUILD="$UE_ROOT/Engine/Build/BatchFiles/$AF_UE_HOST/Build.sh"
  [[ -x "$AF_UAT" ]] || af_die "$AF_UAT missing or not executable"
  local bin="$UE_ROOT/Engine/Binaries/$AF_UE_HOST"
  if [[ -x "$bin/UnrealEditor-Cmd" ]]; then
    AF_EDITOR_CMD="$bin/UnrealEditor-Cmd"
  elif [[ "$AF_UE_HOST" == "Mac" ]]; then
    AF_EDITOR_CMD="$bin/UnrealEditor.app/Contents/MacOS/UnrealEditor"
  else
    AF_EDITOR_CMD="$bin/UnrealEditor"
  fi

  local bv="$UE_ROOT/Engine/Build/Build.version" major minor patch
  if [[ -f "$bv" ]]; then
    major="$(sed -n 's/.*"MajorVersion"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' "$bv")"
    minor="$(sed -n 's/.*"MinorVersion"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' "$bv")"
    patch="$(sed -n 's/.*"PatchVersion"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' "$bv")"
    af_log "Unreal Engine $major.$minor.$patch at $UE_ROOT"
    if [[ "$major.$minor" != "5.8" ]]; then af_die "the project pins UE 5.8 (DECISIONS P1); found $major.$minor"; fi
    if [[ -n "$patch" ]] && ((patch < ${AF_UE_MIN_PATCH:-3})); then
      af_warn "UE 5.8.$patch is older than the tested 5.8.${AF_UE_MIN_PATCH:-3} hotfix"
    fi
  else
    af_warn "$bv not found; engine version not checked"
  fi

  if [[ -n "${AF_XCODE_DEVELOPER_DIR:-}" ]]; then export DEVELOPER_DIR="$AF_XCODE_DEVELOPER_DIR"; fi
  if command -v xcodebuild >/dev/null 2>&1; then
    af_log "$(xcodebuild -version 2>/dev/null | tr '\n' ' ')($(xcode-select -p 2>/dev/null || true))"
  fi
  if [[ "$AF_UE_HOST" == "Mac" ]]; then
    # Xcode 26 no longer bundles the Metal toolchain; the cook compiles Metal shaders / metallibs with it.
    xcrun -sdk macosx metal --version >/dev/null 2>&1 ||
      af_die "Metal Toolchain missing: xcodebuild -downloadComponent MetalToolchain (unreal/Docs/CI.md 3.1)"
  fi

  af_ue_toolchain_stamp

  # UAT / UBT logs go next to the job's artifacts.
  export uebp_LogFolder="$AF_LOG_DIR/uat"
  mkdir -p "$uebp_LogFolder"
  if [[ -n "${GITLAB_CI:-}" && -z "${AF_PERSISTENT_DDC:-}" ]]; then
    af_die "AF_PERSISTENT_DDC is not set: give the runner a persistent DerivedDataCache directory in its config.toml" \
      "environment (unreal/Docs/CI.md 3.1); the DDC is not a GitLab cache"
  fi
  AF_DDC_DIR="${AF_PERSISTENT_DDC:-$AF_UNREAL_DIR/DerivedDataCache}"
  mkdir -p "$AF_DDC_DIR"
  af_log "local DerivedDataCache: $AF_DDC_DIR${AF_ZEN_DATA_DIR:+ (Zen data: $AF_ZEN_DATA_DIR)}"
  export UE_ROOT AF_UE_HOST AF_UAT AF_UBT_BUILD AF_EDITOR_CMD AF_DDC_DIR
}

# Incremental builds on a persistent runner reuse Binaries / Intermediate; after an engine hotfix or a new Xcode / NDK
# their PCHs, UHT output, generated projects and Gradle intermediates are stale (ue58-platform.md 12.1: clean after
# an engine upgrade). The stamp records what built them; a change (or AF_CLEAN_BUILD=1) wipes them first.
af_ue_toolchain_stamp() {
  local stamp_file="$AF_UNREAL_DIR/Intermediate/.af-toolchain-stamp" stamp
  stamp="$(
    cat "$UE_ROOT/Engine/Build/Build.version" 2>/dev/null
    if command -v xcodebuild >/dev/null 2>&1; then xcodebuild -version 2>/dev/null; fi
    echo "host=$AF_UE_HOST ndk=${NDKROOT:-}"
  )"
  if af_is_true "${AF_CLEAN_BUILD:-0}" || [[ -f "$stamp_file" && "$(cat "$stamp_file")" != "$stamp" ]]; then
    af_warn "engine / toolchain changed (or AF_CLEAN_BUILD=1): clean build, deleting Binaries, Intermediate, Saved"
    rm -rf "$AF_UNREAL_DIR/Binaries" "$AF_UNREAL_DIR/Intermediate" "$AF_UNREAL_DIR/Saved"
  fi
  mkdir -p "$AF_UNREAL_DIR/Intermediate"
  printf '%s' "$stamp" >"$stamp_file"
}

# af_ue_run CMD ARGS... -> runs an engine tool with the CI DDC location (an env var name with a dash cannot be
# exported by bash, so it is passed through env(1)) and the console output mirrored to $AF_LOG_DIR.
af_ue_run() {
  local name zen=()
  name="$(basename "$1" | tr -c 'A-Za-z0-9._-' '_')"
  mkdir -p "$AF_LOG_DIR"
  if [[ -n "${AF_ZEN_DATA_DIR:-}" ]]; then zen=("UE-ZenDataPath=$AF_ZEN_DATA_DIR"); fi
  env "UE-LocalDataCachePath=$AF_DDC_DIR" ${zen[@]+"${zen[@]}"} "$@" 2>&1 | tee -a "$AF_LOG_DIR/$name.console.log"
  return "${PIPESTATUS[0]}"
}

# Collects UE's own logs (project Saved/Logs, crash reports) into the artifacts; never fails.
af_ue_collect_logs() {
  local dst="$AF_LOG_DIR/project"
  mkdir -p "$dst"
  if [[ -d "$AF_UNREAL_DIR/Saved/Logs" ]]; then cp -R "$AF_UNREAL_DIR/Saved/Logs/." "$dst/" 2>/dev/null || true; fi
  if [[ -d "$AF_UNREAL_DIR/Saved/Crashes" ]]; then cp -R "$AF_UNREAL_DIR/Saved/Crashes" "$dst/" 2>/dev/null || true; fi
  return 0
}

# af_buildcookrun PLATFORM ARCHIVE_DIR EXTRA_ARGS... -> RunUAT BuildCookRun with the project's release flags.
af_buildcookrun() {
  local platform="$1" archive="$2"
  shift 2
  local args=(
    BuildCookRun
    -project="$AF_UPROJECT"
    -platform="$platform"
    -clientconfig="${AF_UE_CLIENT_CONFIG:-Shipping}"
    -build -cook -stage -pak -iostore -compressed -package -archive
    -archivedirectory="$archive"
    -nop4 -utf8output -unattended
  )
  args+=("$@")
  if [[ -n "${AF_UE_EXTRA_ARGS:-}" ]]; then
    # shellcheck disable=SC2206 # deliberate word splitting of a user-provided argument list
    args+=(${AF_UE_EXTRA_ARGS})
  fi
  af_log "RunUAT ${args[*]}"
  af_ue_run "$AF_UAT" "${args[@]}"
}

# The runtime files Abyssfire.Build.cs stages as RuntimeDependencies (Data/..., Fonts/..., StagedFileType.UFS): the
# data tables and fonts the game reads (docs such as Data/README.md or the font licences are not checked). Relative to
# unreal/; tracked files when unreal/ is this repository's work tree, else the files on disk (self-tests).
af_runtime_dependency_files() {
  local top
  top="$(git -C "$AF_UNREAL_DIR" rev-parse --show-toplevel 2>/dev/null || true)"
  {
    if [[ -n "$top" && "$(cd "$top" && pwd -P)" == "$(cd "$AF_REPO_DIR" && pwd -P)" ]]; then
      git -C "$AF_UNREAL_DIR" ls-files -- Data Fonts
    else
      (cd "$AF_UNREAL_DIR" && find Data Fonts -type f 2>/dev/null)
    fi
  } | grep -E '\.(json|ttf|otf|ttc)$' | LC_ALL=C sort || true
}

# af_check_runtime_dependencies PAK_SEARCH_DIR... -> fails unless every runtime Data/ + Fonts/ file is in the pak.
# Shipping only logs a data-load error (ue58-platform.md 10.2), and the RuntimeDependencies wildcard is expanded into
# the UBT receipt kept in Intermediate, so a missing file would otherwise ship silently. Source: the pak response
# files UAT writes to uebp_LogFolder (PakList_*.txt); without them, `UnrealPak <pakchunk0> -List`.
af_check_runtime_dependencies() {
  local lists=() f n=0 miss=0 pak unrealpak listing
  while IFS= read -r -d '' f; do lists+=("$f"); done < <(find "$uebp_LogFolder" -type f -name 'PakList_*.txt' -print0 2>/dev/null)
  if [[ ${#lists[@]} -eq 0 ]]; then
    unrealpak="$UE_ROOT/Engine/Binaries/$AF_UE_HOST/UnrealPak"
    pak="$(find "$@" -type f -name 'pakchunk0*.pak' 2>/dev/null | head -1)"
    [[ -n "$pak" && -x "$unrealpak" ]] ||
      af_die "no PakList_*.txt in $uebp_LogFolder and no pakchunk0*.pak to list: cannot verify the staged Data/ + Fonts/"
    listing="$AF_LOG_DIR/pak-contents.txt"
    "$unrealpak" "$pak" -List >"$listing" 2>&1 || af_die "UnrealPak $pak -List failed"
    lists=("$listing")
  fi
  while IFS= read -r f; do
    [[ -n "$f" ]] || continue
    n=$((n + 1))
    if ! grep -qF "Abyssfire/$f\"" "${lists[@]}"; then
      af_warn "not staged: $f"
      miss=1
    fi
  done < <(af_runtime_dependency_files)
  [[ $n -gt 0 ]] || af_die "no Data/*.json or Fonts/ files under $AF_UNREAL_DIR (RuntimeDependencies)"
  [[ $miss -eq 0 ]] ||
    af_die "RuntimeDependencies (Data/, Fonts/) missing from the pak: check Abyssfire.Build.cs (ue58-platform.md 10.1); AF_CLEAN_BUILD=1 rebuilds a stale receipt"
  af_log "pak holds all $n runtime Data/ + Fonts/ files (${#lists[@]} pak list(s))"
}
