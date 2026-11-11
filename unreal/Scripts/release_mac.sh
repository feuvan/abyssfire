#!/usr/bin/env bash
# Builds the macOS version of the Unreal remake on this Mac and publishes it as a GitHub pre-release.
#
#   unreal/Scripts/release_mac.sh                 # content -> package -> dmg -> publish (asks before publishing)
#   unreal/Scripts/release_mac.sh --no-publish    # build everything, change nothing on GitHub
#   unreal/Scripts/release_mac.sh --from dmg      # resume: reuse what the earlier steps left in unreal/CI/out
#
# Needs once: UE 5.8.3, Xcode with the Metal toolchain, `brew install gh` + `gh auth login`, and the commit pushed
# (unreal/Scripts/README.md 4.1). Everything is checked before the first long step.
#
# Steps (each is an existing script, run as a child process; the elapsed time is printed after each):
#   content   unreal/Scripts/build_content.sh          editor target + unreal/Content
#   package   unreal/CI/scripts/ue-package.sh Mac      BuildCookRun, Apple Silicon -> package/mac/Abyssfire.app.zip
#   dmg       unreal/CI/scripts/mac-dmg.sh             version stamp, signature, styled DMG -> installers/mac/
#   publish   this script                              release notes, SHA256SUMS.txt, gh release create --prerelease
#
# Options:
#   --version X.Y.Z[-pre]  version of the build. Default: <ProjectVersion of Config/DefaultGame.ini>-pre.<commit
#                          date yyyymmdd>.<first 8 hex of HEAD> - the same for every run on the same commit
#   --tag NAME             tag GitHub creates on the commit. Default: unreal-v<version>. It must not start with "v":
#                          .github/workflows/release.yml builds the WEB game's desktop app for every pushed v* tag
#   --repo OWNER/NAME      GitHub repository. Default: the origin remote (needed when origin is not on github.com)
#   --config NAME          Shipping (default) | Development | Test. A build that is not Shipping says so in its file
#                          name, the release title and the notes
#   --from STEP            content (default) | package | dmg | publish: start there and reuse the earlier steps'
#                          output. Leftovers of another version, configuration or commit are refused (--from package
#                          wants the content this script built from this commit)
#   --no-publish           do everything except GitHub (gh is not needed): the notes are written and the
#                          `gh release create` command is printed, not run
#   --draft                publish as a draft pre-release (visible to you only until you publish it on github.com)
#   --replace              delete an existing draft or pre-release / tag of that name first (shown in the summary,
#                          after your confirmation; a full release is never deleted). Without it they stop the run
#                          before anything is built
#   --notes-file FILE      your own text (Markdown), placed at the top of the release notes
#   --sign-identity NAME   sign with this "Developer ID Application: ..." identity (name or SHA-1) from your keychain
#   --notary-profile NAME  notarise with this notarytool keychain profile (`xcrun notarytool store-credentials NAME`);
#                          needs --sign-identity. Default without both: ad-hoc signature, not notarised - recipients
#                          must allow the app once (the release notes say how)
#   --shaders              passed to build_content.sh: compile every material and fail on shader errors
#   --allow-dirty          build with uncommitted changes to tracked files; only together with --no-publish
#   --yes                  publish without asking (required when stdin is not a terminal)
#   -h, --help             this text
#
# Environment: UE_ROOT (or UE_ROOT_MAC; default "/Users/Shared/Epic Games/UE_5.8"), AF_OUT_DIR (default
# unreal/CI/out), AF_CLEAN_BUILD=1 (delete unreal/Binaries, Intermediate and Saved first; also done on its own when
# the engine or Xcode changed since the last build), AF_PERSISTENT_DDC (the cook's DerivedDataCache; default
# unreal/DerivedDataCache), AF_UE_EXTRA_ARGS, AF_XCODE_DEVELOPER_DIR, AF_NOTARY_TIMEOUT (unreal/Docs/CI.md 4.1),
# AF_RELEASE_MAX_ASSET_BYTES (GitHub's per-file limit, 2 GiB). The CI signing variables (AF_MAC_DEVID_*, AF_ASC_API_*,
# AF_MAC_SIGN_IDENTITY, AF_NOTARY_KEYCHAIN_PROFILE, AF_REQUIRE_SIGNING, AF_SKIP_NOTARIZE) are ignored: the two
# signing options above decide.
#
# Output (all under $AF_OUT_DIR, git-ignored): release/mac/ (the DMG under its published name, SHA256SUMS.txt,
# release-notes.md), logs/release-mac/<step>.log (+ the step's own logs in logs/release-mac/<step>/; the content
# build's are in unreal/Saved/ContentBuild/). A failing step ends the run with its status and names its log.
# Do not commit or edit tracked files while it runs: the build must be the commit it is published as (checked
# after every step).
# ---------------------------------------------------------------------------------------------------- end of help
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# A local run is never steered by GitLab's variables: CI_COMMIT_TAG would make signing mandatory
# (af_signing_required) and GITLAB_CI a persistent DDC; the version is this script's (set_version).
unset CI_COMMIT_TAG CI_COMMIT_SHORT_SHA CI_PIPELINE_IID CI_JOB_NAME_SLUG GITLAB_CI
unset AF_VERSION AF_VERSION_NUMERIC AF_MSI_VERSION AF_VERSION_QUAD AF_BUILD_NUMBER AF_ANDROID_VERSION_CODE
unset AF_IS_RELEASE AF_IS_PRERELEASE AF_GIT_SHA AF_TAG AF_LOG_DIR
# The release goes to github.com, the host of the links in its notes: never to a GitHub Enterprise host or another
# repository picked up from the shell.
unset GH_HOST GH_REPO
# shellcheck source=../CI/lib/common.sh
source "$here/../CI/lib/common.sh"
# shellcheck source=../CI/lib/ue.sh
source "$here/../CI/lib/ue.sh"
# shellcheck source=../CI/lib/macos.sh
source "$here/../CI/lib/macos.sh"
# No child gets a signing secret (the kit's rule); the local signing names go to mac-dmg.sh only, from the options.
# shellcheck disable=SC2119 # no secret to keep
af_unset_secrets
unset AF_MAC_SIGN_IDENTITY AF_NOTARY_KEYCHAIN_PROFILE AF_REQUIRE_SIGNING AF_SKIP_NOTARIZE AF_UE_CLIENT_CONFIG

# Stated by the project (Docs/spec/ue58-platform.md 3; the same figure as the GitLab release notes, CI/scripts/release.sh).
MIN_MACOS="14.5"
MAX_CHANGES=30

usage() { sed -n '2,/end of help$/p' "${BASH_SOURCE[0]}" | sed -e '$d' -e 's/^# \{0,1\}//'; }
die_usage() {
  printf 'release_mac.sh: %s (--help lists the options)\n' "$*" >&2
  exit 2
}

# ------------------------------------------------------------------------------------------------------- options
version="" tag="" repo="" config="Shipping" from="content" notes_file="" sign_identity="" notary_profile=""
publish=1 draft=0 replace=0 shaders=0 allow_dirty=0 yes=0
args=()
for a in "$@"; do
  case "$a" in
    --*=*) args+=("${a%%=*}" "${a#*=}") ;;
    *) args+=("$a") ;;
  esac
done
set -- ${args[@]+"${args[@]}"}
resume_args=() # the options as given, without --from / --replace: the resume hint after a failed upload
while [[ $# -gt 0 ]]; do
  case "$1" in
    --version | --tag | --repo | --config | --from | --notes-file | --sign-identity | --notary-profile)
      [[ $# -ge 2 && -n "$2" ]] || die_usage "$1 needs a value"
      case "$1" in
        --version) version="$2" ;;
        --tag) tag="$2" ;;
        --repo) repo="$2" ;;
        --config) config="$2" ;;
        --from) from="$2" ;;
        --notes-file) notes_file="$2" ;;
        --sign-identity) sign_identity="$2" ;;
        --notary-profile) notary_profile="$2" ;;
      esac
      if [[ "$1" != "--from" ]]; then resume_args+=("$1" "$2"); fi
      shift 2
      continue
      ;;
    --no-publish) publish=0 ;;
    --draft) draft=1 ;;
    --replace) replace=1 ;;
    --shaders) shaders=1 ;;
    --allow-dirty) allow_dirty=1 ;;
    --yes) yes=1 ;;
    -h | --help)
      usage
      exit 0
      ;;
    *) die_usage "unknown argument '$1'" ;;
  esac
  if [[ "$1" != "--replace" ]]; then resume_args+=("$1"); fi
  shift
done

case "$config" in
  Shipping | Development | Test) ;;
  *) die_usage "--config is Shipping, Development or Test (got '$config')" ;;
esac
case "$from" in
  content) first=1 ;;
  package) first=2 ;;
  dmg) first=3 ;;
  publish) first=4 ;;
  *) die_usage "--from is content, package, dmg or publish (got '$from')" ;;
esac
if [[ $allow_dirty -eq 1 && $publish -eq 1 ]]; then
  die_usage "--allow-dirty only works with --no-publish: a published build must be a commit"
fi
if [[ -n "$notary_profile" && -z "$sign_identity" ]]; then
  die_usage "--notary-profile needs --sign-identity: Apple notarises only Developer ID signed software"
fi
if [[ -n "$notes_file" ]]; then
  [[ -f "$notes_file" && -r "$notes_file" ]] || die_usage "--notes-file '$notes_file' is not a readable file"
  notes_file="$(cd "$(dirname "$notes_file")" && pwd)/$(basename "$notes_file")"
fi

# ------------------------------------------------------------------------------------- commit, version, tag, repo
command -v git >/dev/null 2>&1 || af_die "git not found: xcode-select --install"
git_repo() { git -C "$AF_REPO_DIR" "$@"; }
sha="$(git_repo rev-parse --verify --quiet 'HEAD^{commit}')" || af_die "$AF_REPO_DIR is not a git checkout with a commit"

version_re='^([0-9]+)\.([0-9]+)\.([0-9]+)(-([0-9A-Za-z.-]+))?$'
if [[ -z "$version" ]]; then
  # A pure function of the commit (the committed ini, the commit's own date and hash): a resumed run gets the same
  # names. Read from HEAD, not from the working copy, which carries the version overlay while a package step runs.
  base="$(git -C "$AF_UNREAL_DIR" show 'HEAD:./Config/DefaultGame.ini' 2>/dev/null |
    sed -n 's/^ProjectVersion=\([0-9][0-9.]*\).*/\1/p' | head -1 || true)"
  [[ "$base" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] ||
    af_die "no ProjectVersion=X.Y.Z in the committed unreal/Config/DefaultGame.ini: pass --version X.Y.Z[-pre]"
  version="$base-pre.$(git_repo show -s --format=%cd --date=format:%Y%m%d HEAD).${sha:0:8}"
fi
[[ "$version" =~ $version_re ]] ||
  die_usage "--version '$version' is not X.Y.Z or X.Y.Z-pre (digits, letters, dots and dashes after the dash)"

# The kit takes its version from a tag vX.Y.Z[-pre] (unreal/CI/version.sh): hand it ours in that one command's
# environment and export what it prints, so every child (af_load_version) stamps the same version. AF_TAG is the
# kit's notion of a GitLab tag, not the GitHub tag of this release: left out.
set_version() {
  local out line
  out="$(CI_COMMIT_TAG="v$version" "$AF_CI_DIR/version.sh")" ||
    af_die "version '$version' is not usable (unreal/CI/version.sh, message above): pass another --version"
  while IFS= read -r line; do
    case "$line" in
      AF_TAG=*) ;;
      AF_*=*) export "${line?}" ;;
    esac
  done <<<"$out"
  [[ "${AF_VERSION:-}" == "$version" ]] || af_die "unreal/CI/version.sh did not accept version '$version'"
}
set_version

if [[ -z "$tag" ]]; then tag="unreal-v$version"; fi
if [[ "$tag" == v* ]]; then
  die_usage "--tag '$tag' starts with \"v\": .github/workflows/release.yml builds and drafts a release of the WEB game" \
    "for every pushed v* tag. Use the default (unreal-v$version) or another prefix"
fi
if ! [[ "$tag" =~ ^[A-Za-z0-9][A-Za-z0-9._/-]*$ ]] || ! git check-ref-format "refs/tags/$tag"; then
  die_usage "--tag '$tag' is not a usable tag name (letters, digits, . _ / - ; see git check-ref-format)"
fi

# OWNER/NAME of a github.com remote: https (with or without credentials), scp-like and ssh:// forms, with or without
# .git. The URL itself is never printed: it can hold a token.
repo_from_origin() {
  local url path
  url="$(git_repo remote get-url origin 2>/dev/null || true)"
  case "$url" in
    https://github.com/*) path="${url#https://github.com/}" ;;
    https://*@github.com/*) path="${url#https://*@github.com/}" ;;
    git@github.com:*) path="${url#git@github.com:}" ;;
    ssh://git@github.com/*) path="${url#ssh://git@github.com/}" ;;
    *) return 1 ;;
  esac
  path="${path%/}"
  printf '%s\n' "${path%.git}"
}
if [[ -z "$repo" ]]; then
  repo="$(repo_from_origin)" ||
    af_die "the origin remote is not a github.com repository: pass --repo OWNER/NAME"
fi
[[ "$repo" =~ ^[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9._-]+$ ]] || die_usage "--repo '$repo' is not OWNER/NAME"

# ----------------------------------------------------------------------------------------------- names and places
config_lc="$(printf '%s' "$config" | tr '[:upper:]' '[:lower:]')"
dmg_built="$AF_OUT_DIR/installers/mac/Abyssfire-$version-macos-arm64.dmg" # what mac-dmg.sh writes
if [[ "$config" == "Shipping" ]]; then
  asset_name="Abyssfire-$version-macos-arm64.dmg"
  asset_label="macOS (Apple Silicon) - .dmg"
  title="Abyssfire Unreal $version (macOS, pre-release)"
else
  asset_name="Abyssfire-$version-$config_lc-macos-arm64.dmg"
  asset_label="macOS (Apple Silicon) - .dmg ($config build)"
  title="Abyssfire Unreal $version (macOS, $config build, pre-release)"
fi
content_info="$AF_OUT_DIR/content-info.txt"
assets_dir="$AF_OUT_DIR/installer-assets" # mac-dmg.sh draws the DMG artwork there when it is absent
package_zip="$AF_OUT_DIR/package/mac/Abyssfire.app.zip"
package_info="$AF_OUT_DIR/package/mac/release-info.txt"
dmg_info="$AF_OUT_DIR/installers/mac/release-info.txt"
rel_dir="$AF_OUT_DIR/release/mac"
notes="$rel_dir/release-notes.md"
sums="$rel_dir/SHA256SUMS.txt"
log_root="$AF_OUT_DIR/logs/release-mac"
max_bytes="${AF_RELEASE_MAX_ASSET_BYTES:-2147483648}"
[[ "$max_bytes" =~ ^[0-9]+$ ]] || af_die "AF_RELEASE_MAX_ASSET_BYTES must be a number of bytes"
# gh splits "file#label" at the '#'
case "$rel_dir" in *"#"*) af_die "AF_OUT_DIR must not contain '#' ($AF_OUT_DIR)" ;; esac
case "$notes_file" in
  "$rel_dir"/*) die_usage "--notes-file must not be inside $rel_dir (that directory is rebuilt on every run): move the file" ;;
esac
export GH_PROMPT_DISABLED=1
# The engine, its Python and the Apple tools have no use for a GitHub token either.
child_env=(env -u GH_TOKEN -u GITHUB_TOKEN -u GH_ENTERPRISE_TOKEN -u GITHUB_ENTERPRISE_TOKEN -u GH_PROMPT_DISABLED)

info_get() { sed -n "s/^$2: //p" "$1" 2>/dev/null | head -1; } # info_get FILE KEY
# check_info FILE WHAT STEP -> the leftover WHAT must be this version, configuration and commit, else re-run STEP
check_info() {
  local file="$1" what="$2" step="$3" key want have
  [[ -f "$file" ]] ||
    af_die "$what in $AF_OUT_DIR was not made by this script for this build (no $(basename "$file")): re-run with --from $step"
  for key in version config commit; do
    case "$key" in
      version) want="$version" ;;
      config) want="$config" ;;
      *) want="$sha" ;;
    esac
    have="$(info_get "$file" "$key")"
    [[ "$have" == "$want" ]] ||
      af_die "stale $what in $AF_OUT_DIR: its $key is '$have', this run's is '$want': re-run with --from $step"
  done
  if [[ $publish -eq 1 && "$(info_get "$file" tree)" != "clean" ]]; then
    af_die "$what in $AF_OUT_DIR was built with uncommitted changes (--allow-dirty): re-run with --from $step"
  fi
}
human_size() { awk -v b="$1" 'BEGIN { if (b >= 1048576) printf "%.1f MiB", b / 1048576; else printf "%.0f KiB", b / 1024 }'; }
elapsed() { # elapsed SECONDS -> 1h 02m 03s
  local s="$1"
  if ((s >= 3600)); then
    printf '%dh %02dm %02ds' $((s / 3600)) $((s % 3600 / 60)) $((s % 60))
  else
    printf '%dm %02ds' $((s / 60)) $((s % 60))
  fi
}

# -------------------------------------------------------------------------------------------------------- GitHub
# 0: the release (a draft too) exists; 1: GitHub says there is none; any other answer ends the run.
gh_release_exists() {
  local err
  if err="$(gh release view "$tag" --repo "$repo" --json tagName 2>&1 >/dev/null)"; then return 0; fi
  case "$err" in *"not found"* | *"Not Found"*) return 1 ;; esac
  af_die "could not ask GitHub for release '$tag' of $repo: $err"
}
gh_tag_exists() {
  local err
  if err="$(gh api "repos/$repo/git/ref/tags/$tag" 2>&1 >/dev/null)"; then return 0; fi
  case "$err" in *"HTTP 404"*) return 1 ;; esac
  af_die "could not ask GitHub for tag '$tag' of $repo: $err"
}

# --replace deletes: find out what (replace_target, the summary line; empty when there is nothing of that name), and
# never a release that is neither a draft nor a pre-release - one somebody promoted on github.com is not ours to wipe.
replace_target=""
replace_inspect() {
  local view is_draft is_pre files name kind
  replace_target=""
  if gh_release_exists; then
    view="$(gh release view "$tag" --repo "$repo" --json isDraft,isPrerelease,assets,name \
      --jq '[.isDraft, .isPrerelease, (.assets | length), .name] | @tsv' 2>&1)" ||
      af_die "could not read release '$tag' of $repo: $view"
    IFS=$'\t' read -r is_draft is_pre files name <<<"$view" || true
    if [[ "$is_draft" != "true" && "$is_pre" != "true" ]]; then
      af_die "release '$tag' of $repo is a full release (neither a draft nor a pre-release): --replace does not delete" \
        "those. Delete it yourself on github.com, or pass another --tag / --version"
    fi
    kind="pre-release"
    if [[ "$is_draft" == "true" ]]; then kind="draft"; fi
    name="$(printf '%s' "$name" | tr -d '[:cntrl:]')"
    replace_target="the existing $kind '${name:-$tag}' ($files files, their download counts) and its tag are DELETED first"
  elif gh_tag_exists; then
    replace_target="the existing tag '$tag' (it has no release) is DELETED first"
  fi
}

# The build is published as commit $sha: HEAD and the tracked files must still be that commit after every step. A
# commit or an edit made while a step ran would otherwise ship under the wrong name.
dirty_tree_stop() { # dirty_tree_stop TREE MESSAGE...
  printf '%s\n' "$1" | sed -n '1,20p' >&2
  shift
  if [[ -f "$AF_UNREAL_DIR/Config/DefaultGame.ini.af-ci-backup" ]]; then
    af_die "a killed package step left its version overlay behind: mv unreal/Config/DefaultGame.ini.af-ci-backup" \
      "unreal/Config/DefaultGame.ini"
  fi
  af_die "$@"
}
check_unchanged() { # check_unchanged WHEN ("during the package step")
  local head tree
  head="$(git_repo rev-parse --verify --quiet 'HEAD^{commit}' || true)"
  [[ "$head" == "$sha" ]] ||
    af_die "HEAD moved from ${sha:0:8} to ${head:0:8} $1: what was built is not one commit. Re-run from the start" \
      "(without --from) on the commit to release"
  tree="$(git_repo status --porcelain --untracked-files=no)" || af_die "git status failed in $AF_REPO_DIR"
  if [[ -z "$tree" ]]; then return 0; fi
  tree_state=dirty
  if [[ $allow_dirty -eq 1 ]]; then return 0; fi
  dirty_tree_stop "$tree" "tracked files changed $1 (above): what was built is not commit ${sha:0:8}. Commit or" \
    "revert, then re-run from the start (without --from)"
}

# ------------------------------------------------------------------------------------------------------ preflight
# Everything that can stop the run is checked here, before the first long step; each message names the fix.
need_cmd() { # need_cmd COMMAND FIX
  command -v "$1" >/dev/null 2>&1 || af_die "'$1' not found: $2"
}
preflight() {
  local err line push tree ids
  [[ "$(uname -s)" == "Darwin" ]] ||
    af_die "this script builds on macOS only (this is $(uname -s)): run it on the Mac with Unreal Engine 5.8"
  [[ -z "${AF_MAC_ARCH:-}" || "$AF_MAC_ARCH" == "arm64" ]] ||
    af_die "AF_MAC_ARCH=$AF_MAC_ARCH: the release is Apple Silicon only (the DMG is named arm64): unset AF_MAC_ARCH"

  tree="$(git_repo status --porcelain --untracked-files=no)" || af_die "git status failed in $AF_REPO_DIR"
  tree_state=clean
  if [[ -n "$tree" ]]; then
    tree_state=dirty
    if [[ $allow_dirty -eq 0 ]]; then
      dirty_tree_stop "$tree" "tracked files have uncommitted changes (above): commit them (a release is a commit)," \
        "or try the build with --no-publish --allow-dirty"
    fi
    af_warn "building with uncommitted changes (--allow-dirty): this build cannot be published"
  fi

  if [[ $first -le 2 ]]; then
    ue_root="${UE_ROOT:-${UE_ROOT_MAC:-/Users/Shared/Epic Games/UE_5.8}}"
    if [[ -n "${UE_ROOT:-}" && -n "${UE_ROOT_MAC:-}" && "$UE_ROOT" != "$UE_ROOT_MAC" ]]; then
      af_die "UE_ROOT and UE_ROOT_MAC name different engines: set one (build_content.sh reads UE_ROOT, unreal/CI UE_ROOT_MAC)"
    fi
    [[ -d "$ue_root/Engine" && -x "$ue_root/Engine/Build/BatchFiles/RunUAT.sh" ]] ||
      af_die "Unreal Engine not found at '$ue_root': install UE 5.8.3 with the Epic Games Launcher, or set UE_ROOT"
    ue_version="$(af_ue_build_version "$ue_root")" ||
      af_die "'$ue_root' has no Engine/Build/Build.version: not an Unreal Engine installation (UE_ROOT)"
    [[ "${ue_version%.*}" == "5.8" ]] ||
      af_die "the project pins UE 5.8 (DECISIONS P1); '$ue_root' is $ue_version: install 5.8.3 or set UE_ROOT"
    # both families of scripts get the same engine and the same Xcode
    export UE_ROOT="$ue_root" UE_ROOT_MAC="$ue_root"
    if [[ -n "${AF_XCODE_DEVELOPER_DIR:-}" ]]; then export DEVELOPER_DIR="$AF_XCODE_DEVELOPER_DIR"; fi
    need_cmd xcrun "install Xcode and run: sudo xcode-select -s /Applications/Xcode.app"
    af_ue_require_metal
    need_cmd ditto "it is part of macOS (/usr/bin/ditto): check PATH"
  fi
  if [[ $first -le 3 ]]; then
    for line in ditto codesign hdiutil sips; do need_cmd "$line" "it is part of macOS: check PATH"; done
    need_cmd "$AF_PLISTBUDDY" "it is part of macOS (/usr/libexec/PlistBuddy)"
    need_cmd xcrun "install Xcode and run: sudo xcode-select -s /Applications/Xcode.app"
    need_cmd python3 "xcode-select --install (the DMG artwork and window layout use it)"
  fi
  if ! command -v shasum >/dev/null 2>&1; then need_cmd sha256sum "shasum is part of macOS (/usr/bin/shasum): check PATH"; fi

  # leftovers of the steps this run skips
  if [[ $first -eq 2 ]]; then
    [[ -f "$AF_UNREAL_DIR/Content/Abyssfire/Maps/L_Main.umap" ]] ||
      af_die "unreal/Content has no L_Main (the content build's output): run without --from, or with --from content"
    # unreal/Content is reused as it is: it must be what this script's content step built from this commit (a failed
    # or interrupted content step leaves no record)
    [[ -f "$content_info" ]] ||
      af_die "unreal/Content was not built by a finished content step of this script (no $content_info): run" \
        "without --from (a content build skips what did not change)"
    line="$(info_get "$content_info" commit)"
    [[ "$line" == "$sha" ]] ||
      af_die "unreal/Content was built from commit ${line:0:8}, this run is ${sha:0:8}: run without --from (a content" \
        "build skips what did not change)"
    if [[ $publish -eq 1 && "$(info_get "$content_info" tree)" != "clean" ]]; then
      af_die "unreal/Content was built with uncommitted changes (--allow-dirty): run without --from"
    fi
  fi
  if [[ $first -ge 3 ]]; then
    [[ -f "$package_zip" ]] || af_die "$package_zip missing: re-run with --from package"
    check_info "$package_info" "the packaged app" package
  fi
  if [[ $first -ge 4 ]]; then
    [[ -f "$dmg_built" ]] || af_die "no DMG of version $version in $AF_OUT_DIR/installers/mac: re-run with --from dmg"
    check_info "$dmg_info" "the DMG" dmg
    [[ "$(info_get "$dmg_info" sha256)" == "$(af_sha256 "$dmg_built")" ]] ||
      af_die "$dmg_built is not the DMG this script built (checksum differs from release-info.txt): re-run with --from dmg"
    if [[ -n "$sign_identity" && "$(info_get "$dmg_info" signing)" != "developer-id" ]]; then
      af_die "the DMG in $AF_OUT_DIR is ad-hoc signed, --sign-identity was given: re-run with --from dmg"
    fi
    if [[ -n "$notary_profile" && "$(info_get "$dmg_info" notarised)" != "yes" ]]; then
      af_die "the DMG in $AF_OUT_DIR is not notarised, --notary-profile was given: re-run with --from dmg"
    fi
    if [[ "$(info_get "$dmg_info" notarised)" == "yes" ]]; then
      need_cmd xcrun "install Xcode and run: sudo xcode-select -s /Applications/Xcode.app (the notarisation ticket is checked)"
    fi
  fi

  if [[ -n "$sign_identity" && $first -le 3 ]]; then
    need_cmd security "it is part of macOS (/usr/bin/security): check PATH"
    ids="$(af_local_identities "$sign_identity")" ||
      af_die "no valid code-signing identity '$sign_identity' in your keychains: list them with" \
        "security find-identity -v -p codesigning"
    # codesign refuses a name that fits two certificates (usual for a while after a Developer ID renewal): find that
    # out now, not after the cook, and sign with the one certificate's SHA-1
    if [[ "$(printf '%s\n' "$ids" | awk '{ print $2 }' | sort -u | wc -l | tr -d ' ')" != "1" ]]; then
      printf '%s\n' "$ids" >&2
      af_die "--sign-identity '$sign_identity' fits more than one identity (above): pass the SHA-1 (the 40 hex" \
        "digits) of the one to sign with"
    fi
    line="$(printf '%s\n' "$ids" | sed -n 1p)"
    [[ "$line" == *"Developer ID Application"* ]] ||
      af_die "'$sign_identity' is not a \"Developer ID Application\" identity (the only kind for software distributed" \
        "outside the App Store): security find-identity -v -p codesigning"
    sign_sha1="$(printf '%s\n' "$line" | awk '{ print $2 }')"
    if [[ -n "$notary_profile" ]]; then
      xcrun notarytool history --keychain-profile "$notary_profile" >/dev/null 2>&1 ||
        af_die "notarytool cannot use the keychain profile '$notary_profile': create it with" \
          "xcrun notarytool store-credentials $notary_profile (README 4.1), or check the network"
    fi
  fi

  if [[ $publish -eq 1 ]]; then
    need_cmd gh "brew install gh, then gh auth login (or build only: --no-publish)"
    # `gh auth token`, not `gh auth status`: the latter fails when ANY stored github.com account has a bad token, also
    # one that is not in use. Whether the token works is what the next request shows. (The token is never printed.)
    gh auth token --hostname github.com >/dev/null 2>&1 ||
      af_die "gh is not logged in to github.com: gh auth login"
    if [[ -n "${GH_TOKEN:-}${GITHUB_TOKEN:-}" ]]; then
      af_warn "GH_TOKEN / GITHUB_TOKEN is set: gh uses that token, not your gh auth login. It needs write access to" \
        "the repository's contents (fine-grained: Contents read and write), which cannot be checked before the upload"
    fi
    push="$(gh api "repos/$repo" --jq '.permissions.push' 2>/dev/null)" ||
      af_die "GitHub repository $repo is not reachable with this login: check --repo, gh auth status and the network"
    [[ "$push" != "false" ]] ||
      af_die "this GitHub login cannot write to $repo (releases need write access): gh auth login as a maintainer"
    if ! err="$(gh api "repos/$repo/commits/$sha" --jq '.sha' 2>&1 >/dev/null)"; then
      case "$err" in
        *"HTTP 404"* | *"HTTP 422"*) af_die "commit ${sha:0:8} is not on GitHub ($repo): push first (git push origin HEAD)" ;;
        *) af_die "could not ask GitHub for commit ${sha:0:8} of $repo: $err" ;;
      esac
    fi
    if [[ $replace -eq 1 ]]; then
      replace_inspect
      if [[ -n "$replace_target" ]]; then af_warn "--replace: $replace_target (after your confirmation)"; fi
    else
      if gh_release_exists; then
        af_die "release '$tag' already exists in $repo: pass --replace to delete it first, or another --tag / --version"
      fi
      if gh_tag_exists; then
        af_die "tag '$tag' already exists in $repo (without a release): pass --replace to delete it first, or another --tag"
      fi
    fi
    if [[ $yes -eq 0 && ! -t 0 ]]; then
      af_die "stdin is not a terminal, so the question before publishing cannot be asked: pass --yes (or --no-publish)"
    fi
  fi
}

# ---------------------------------------------------------------------------------------------------------- steps
step_no=0
step_total=$((5 - first))
step_times=()
step_t0=0
step_begin() { # step_begin NAME WHAT
  step_no=$((step_no + 1))
  step_t0=$SECONDS
  af_log "step $step_no/$step_total $1: $2"
}
step_end() { # step_end NAME
  local took
  took="$(elapsed $((SECONDS - step_t0)))"
  step_times+=("$1 $took")
  af_log "step $1 done in $took"
}
# run_child NAME WHAT COMMAND... -> one child step with its console mirrored to logs/release-mac/NAME.log; a
# failing child ends the run with its status.
run_child() {
  local name="$1" what="$2" log status
  shift 2
  log="$log_root/$name.log"
  rm -rf "${log_root:?}/$name"
  mkdir -p "$log_root/$name"
  step_begin "$name" "$what"
  set +e
  "${child_env[@]}" AF_LOG_DIR="$log_root/$name" "$@" 2>&1 | tee "$log"
  status=${PIPESTATUS[0]}
  set -e
  if [[ $status -ne 0 ]]; then
    printf '\033[1;31m[af] ERROR:\033[0m %s\n' \
      "step $name failed with status $status after $(elapsed $((SECONDS - step_t0))); log: $log" >&2
    printf '     fix the cause, then re-run with --from %s (the earlier steps are not repeated)\n' "$name" >&2
    exit "$status"
  fi
  step_end "$name"
}

step_content() {
  local extra=()
  if [[ $shaders -eq 1 ]]; then extra=(--shaders); fi
  mkdir -p "$AF_OUT_DIR"
  rm -f "$content_info"
  run_child content "unreal/Scripts/build_content.sh (editor target + unreal/Content)" \
    bash "$here/build_content.sh" ${extra[@]+"${extra[@]}"}
  check_unchanged "during the content step"
  printf 'commit: %s\ntree: %s\n' "$sha" "$tree_state" >"$content_info"
}

step_package() {
  rm -f "$package_info" "$dmg_info"
  run_child package "unreal/CI/scripts/ue-package.sh Mac ($config, arm64)" \
    AF_UE_CLIENT_CONFIG="$config" bash "$AF_CI_DIR/scripts/ue-package.sh" Mac
  check_unchanged "during the package step"
  printf 'version: %s\nconfig: %s\ncommit: %s\ntree: %s\nengine: %s\n' \
    "$version" "$config" "$sha" "$tree_state" "$ue_version" >"$package_info"
}

step_dmg() {
  local sign=() signing=ad-hoc notarised=no
  rm -f "$dmg_info"
  # mac-dmg.sh keeps the artwork it finds, and the artwork can carry the version (and the project icon of its day):
  # in CI it is a per-pipeline artifact, here it would be the first release's for ever.
  if [[ -d "$assets_dir" && "$(cat "$assets_dir/.release-version" 2>/dev/null || true)" != "$version" ]]; then
    rm -rf "$assets_dir"
  fi
  if [[ -n "$sign_identity" ]]; then
    sign+=("AF_MAC_SIGN_IDENTITY=$sign_sha1")
    signing=developer-id
  fi
  if [[ -n "$notary_profile" ]]; then
    sign+=("AF_NOTARY_KEYCHAIN_PROFILE=$notary_profile")
    notarised=yes
  fi
  run_child dmg "unreal/CI/scripts/mac-dmg.sh ($signing signature, notarised: $notarised)" \
    ${sign[@]+"${sign[@]}"} bash "$AF_CI_DIR/scripts/mac-dmg.sh"
  [[ -f "$dmg_built" ]] || af_die "mac-dmg.sh did not write $dmg_built (log: $log_root/dmg.log)"
  if [[ -d "$assets_dir" ]]; then printf '%s\n' "$version" >"$assets_dir/.release-version"; fi
  check_unchanged "during the dmg step"
  {
    cat "$package_info"
    printf 'signing: %s\nnotarised: %s\nsha256: %s\n' "$signing" "$notarised" "$(af_sha256 "$dmg_built")"
  } >"$dmg_info"
}

# write_notes FILE [USER_NOTES] -> the release notes in the house style of unreal/CI/scripts/release.sh. Only facts
# of the build and of the repository go in: no path, user or host name of this machine.
write_notes() {
  local out="$1" user_notes="${2:-}" with_changes="${3:-1}" size sum signing notarised engine prev total subjects home
  size="$(af_file_size "$rel_dir/$asset_name")"
  sum="$(af_sha256 "$rel_dir/$asset_name")"
  signing="$(info_get "$dmg_info" signing)"
  notarised="$(info_get "$dmg_info" notarised)"
  engine="$(info_get "$dmg_info" engine)"
  {
    printf '> **Pre-release / 预发布版本** — for testing. The Unreal Engine remake, macOS (Apple Silicon) only.\n\n'
    if [[ -n "$user_notes" ]]; then
      cat "$user_notes"
      printf '\n'
    fi
    cat <<EOF
## 下载 / Downloads

| Platform | File | Size | SHA-256 |
|---|---|---|---|
| $asset_label | [\`$asset_name\`](https://github.com/$repo/releases/download/$tag/$asset_name) | $(human_size "$size") | \`$sum\` |

## 安装说明 / Install notes

EOF
    if [[ "$notarised" == "yes" ]]; then
      echo "* **macOS $MIN_MACOS+ (Apple Silicon)**: open the .dmg and drag Abyssfire to Applications (signed with Developer ID and notarised)."
    else
      if [[ "$signing" == "developer-id" ]]; then
        echo "* **macOS $MIN_MACOS+ (Apple Silicon)**: open the .dmg and drag Abyssfire to Applications. This test build is signed with Developer ID but **not notarised**, so macOS refuses to open it the first time. That is expected for a test build; allow it once:"
      else
        echo "* **macOS $MIN_MACOS+ (Apple Silicon)**: open the .dmg and drag Abyssfire to Applications. This test build is ad-hoc signed and **not notarised**, so macOS refuses to open it the first time. That is expected for a test build; allow it once:"
      fi
      cat <<'EOF'
  * open Abyssfire once (macOS blocks it), then **System Settings > Privacy & Security**, scroll down to *Security* and click **Open Anyway**;
  * macOS 14 and earlier only: Control-click the app, choose **Open**, then **Open** again;
  * or in Terminal: `xattr -dr com.apple.quarantine /Applications/Abyssfire.app`.
EOF
    fi
    case "$config" in
      Shipping) ;;
      Development)
        cat <<'EOF'
* **Development build** (not Shipping): it keeps the log, the console and the in-game autotest, which plays the game unattended and reports (`report.json` and screenshots in the output directory): `open -n -W /Applications/Abyssfire.app --args -abyssautotest=smoke -abyssclass=warrior -abyssautotestdir=/tmp/abyss-autotest` (`-abyssautotest=smoke|chapter`, `-abyssclass=warrior|mage|rogue`). Shipping builds never run it.
EOF
        ;;
      *) echo "* **$config build** (not Shipping): a test configuration, not the build players get." ;;
    esac
    # shellcheck disable=SC2016 # literal backticks
    printf '* Verify the download with `shasum -a 256 -c SHA256SUMS.txt`.\n\n'

    # Commit subjects touching unreal/ since the nearest earlier unreal-v* tag reachable from HEAD (local tags only:
    # nothing is fetched or created to find it). As a code block: no @mention, no Markdown from a subject. The
    # subjects are the repository's public history, not this machine's; e-mail addresses and this machine's own
    # directories are masked all the same.
    prev=""
    if [[ "$with_changes" == "1" ]]; then
      prev="$(git_repo describe --tags --abbrev=0 --match 'unreal-v*' --exclude "$tag" HEAD 2>/dev/null || true)"
      if [[ -z "$prev" ]]; then
        af_log "no earlier unreal-v* tag in this clone: the notes get no list of changes (git fetch --tags origin" \
          "brings the tags of earlier releases)"
      fi
    fi
    if [[ -n "$prev" ]]; then
      subjects="$(git -C "$AF_UNREAL_DIR" log --no-merges --format='%s' "$prev..HEAD" -- . |
        sed -E -e 's/[[:alnum:]._%+-]+@[[:alnum:].-]+\.[[:alpha:]]{2,}/<email>/g' -e 's/```/'"'''"'/g')"
      # (assignments without outer quotes: bash 3.2 keeps the inner quotes of a quoted ${var//"$a"/b} in some forms)
      subjects=${subjects//"$AF_REPO_DIR"/<repo>}
      home="${HOME:-}"
      if [[ ${#home} -gt 1 ]]; then subjects=${subjects//"$home"/<home>}; fi
      if [[ -n "$subjects" ]]; then
        total="$(printf '%s\n' "$subjects" | wc -l | tr -d ' ')"
        # shellcheck disable=SC2016 # literal backticks
        printf '## 更新内容 / Changes since `%s`\n\n```text\n' "$prev"
        printf '%s\n' "$subjects" | sed -n "1,${MAX_CHANGES}p" # not head: it would stop reading a long list
        if ((total > MAX_CHANGES)); then printf '... and %d more\n' $((total - MAX_CHANGES)); fi
        printf '```\n\n'
      fi
    fi
    # shellcheck disable=SC2016 # literal backticks
    printf 'Built from `%s` with Unreal Engine %s (%s, arm64) by `unreal/Scripts/release_mac.sh`.\n' \
      "$sha" "${engine:-5.8}" "$config"
  } >"$out"
}

step_publish() {
  local size signing notarised state create=() answer created view url is_draft is_pre n_assets resume local_paths home kind
  local replace_line=""
  if [[ $publish -eq 1 ]]; then
    step_begin publish "release notes, checksums, GitHub pre-release $tag"
  else
    step_begin publish "release notes, checksums (--no-publish: GitHub is not touched)"
  fi
  check_unchanged "since this run started"
  check_info "$package_info" "the packaged app" package
  check_info "$dmg_info" "the DMG" dmg
  [[ -f "$dmg_built" && "$(info_get "$dmg_info" sha256)" == "$(af_sha256 "$dmg_built")" ]] ||
    af_die "$dmg_built is not the DMG this script built: re-run with --from dmg"
  signing="$(info_get "$dmg_info" signing)"
  notarised="$(info_get "$dmg_info" notarised)"
  case "$signing/$notarised" in
    developer-id/yes)
      state="signed with Developer ID, notarised"
      xcrun stapler validate "$dmg_built" >/dev/null 2>&1 ||
        af_die "$dmg_built carries no notarisation ticket (xcrun stapler validate): re-run with --from dmg"
      ;;
    developer-id/*) state="signed with Developer ID, NOT notarised (recipients must allow it once)" ;;
    *) state="ad-hoc signed, NOT notarised (recipients must allow it once)" ;;
  esac

  if [[ -n "$notes_file" && "$notes_file" -ef "$rel_dir/$(basename "$notes_file")" ]]; then
    af_die "--notes-file is inside $rel_dir, which is rebuilt now: move the file elsewhere"
  fi
  rm -rf "$rel_dir"
  mkdir -p "$rel_dir"
  cp -f "$dmg_built" "$rel_dir/$asset_name"
  (cd "$rel_dir" && af_checksum_into SHA256SUMS.txt "$asset_name")
  # What this script itself writes is rendered once on its own - without the user's text and without the commit
  # subjects, which are the repository's and may well name a path - and searched for anything of this machine.
  write_notes "$notes" "" 0
  local_paths=(-e "$AF_REPO_DIR" -e "/Users/")
  home="${HOME:-}"
  if [[ ${#home} -gt 1 ]]; then local_paths+=(-e "$home"); fi
  if grep -qF "${local_paths[@]}" "$notes"; then
    af_die "internal error: the generated release notes name a local path ($notes)"
  fi
  write_notes "$notes" "$notes_file"
  if [[ $publish -eq 1 && $replace -eq 1 ]]; then
    replace_inspect # again: hours may have passed since the preflight
    if [[ -n "$replace_target" ]]; then replace_line=$'\n'"  replaces    $replace_target"; fi
  fi

  size="$(af_file_size "$rel_dir/$asset_name")"
  kind="pre-release (public as soon as it is created)"
  if [[ $draft -eq 1 ]]; then kind="draft pre-release"; fi
  cat <<EOF

  repository  $repo
  tag         $tag
  commit      $sha
  title       $title
  kind        $kind$replace_line
  signature   $state
  notes       $notes
  assets      $asset_name  $(human_size "$size")
                sha256 $(af_sha256 "$rel_dir/$asset_name")
              SHA256SUMS.txt  $(af_file_size "$sums") bytes
                sha256 $(af_sha256 "$sums")

EOF
  create=(gh release create "$tag" --repo "$repo" --target "$sha" --title "$title" --notes-file "$notes" --prerelease)
  if [[ $draft -eq 1 ]]; then create+=(--draft); fi
  create+=("$rel_dir/$asset_name#$asset_label" "$sums#SHA-256 checksums")

  if ((size >= max_bytes)); then
    if [[ $publish -eq 1 ]]; then
      af_die "$asset_name is $(human_size "$size"): GitHub refuses release files of 2 GiB or more (limit here: $max_bytes bytes)"
    fi
    af_warn "$asset_name is $(human_size "$size"): GitHub refuses release files of 2 GiB or more"
  fi
  if [[ $publish -eq 0 ]]; then
    af_log "--no-publish: nothing on GitHub was changed. To publish this build:"
    if [[ $replace -eq 1 ]]; then echo "  (--replace would first delete an existing release / tag '$tag')"; fi
    printf '  '
    printf '%q ' "${create[@]}"
    printf '\n'
    echo "  (or, with every check and the confirmation: unreal/Scripts/release_mac.sh --from publish)"
    step_end publish
    return 0
  fi

  if [[ $yes -eq 0 ]]; then
    [[ -t 0 ]] || af_die "stdin is not a terminal: pass --yes to publish (everything is built: --from publish)"
    printf 'Publish this to https://github.com/%s/releases ? [y/N] ' "$repo" >&2
    read -r answer || answer=""
    case "$answer" in
      y | Y | yes | YES) ;;
      *) af_die "not published (everything is built: re-run with --from publish when ready)" ;;
    esac
  fi

  if [[ $replace -eq 1 ]]; then
    if gh_release_exists; then
      af_log "--replace: deleting release '$tag' and its tag"
      # (a failed delete of a release that is gone by now is no failure: a draft has no tag for --cleanup-tag to
      # remove; a tag that is left is deleted below)
      gh release delete "$tag" --repo "$repo" --yes --cleanup-tag || ! gh_release_exists ||
        af_die "could not delete release '$tag' of $repo"
    fi
    if gh_tag_exists; then
      af_log "--replace: deleting tag '$tag'"
      # (a failed delete of a tag that is gone by now is no failure)
      gh api --method DELETE "repos/$repo/git/refs/tags/$tag" >/dev/null || ! gh_tag_exists ||
        af_die "could not delete tag '$tag' of $repo"
    fi
  fi

  af_log "gh release create $tag (uploading $(human_size "$size"))"
  if ! created="$("${create[@]}")"; then
    resume=(unreal/Scripts/release_mac.sh ${resume_args[@]+"${resume_args[@]}"} --from publish)
    if gh release view "$tag" --repo "$repo" --json tagName >/dev/null 2>&1; then
      resume+=(--replace)
      af_warn "gh release create failed and left a release '$tag' behind (possibly a draft without all its files)"
    else
      af_warn "gh release create failed; no release '$tag' was left on GitHub"
      if [[ -n "$replace_target" ]]; then af_warn "(--replace had already deleted the earlier one)"; fi
    fi
    af_die "not published. Everything is built; resume with: $(printf '%q ' "${resume[@]}")"
  fi

  # read it back: what GitHub now shows
  if view="$(gh release view "$tag" --repo "$repo" --json url,isDraft,isPrerelease,assets \
    --jq '[.url, .isDraft, .isPrerelease, (.assets | length)] | @tsv')"; then
    IFS=$'\t' read -r url is_draft is_pre n_assets <<<"$view"
    [[ "$is_pre" == "true" ]] || af_warn "GitHub does not show '$tag' as a pre-release: check it on the release page"
    [[ "$n_assets" == "2" ]] || af_warn "GitHub shows $n_assets files for '$tag', 2 were sent: check the release page"
    if [[ "$is_draft" == "true" ]]; then af_log "the release is a DRAFT: publish it from its page when ready"; fi
  else
    af_warn "the release was created but could not be read back (gh release view $tag --repo $repo)"
    url="$created"
  fi
  step_end publish
  af_log "released: ${url:-https://github.com/$repo/releases}"
  if [[ $draft -eq 0 ]]; then
    af_log "GitHub created the tag '$tag'; this clone does not have it yet. Run: git fetch --tags origin" \
      "(the next pre-release lists its changes since the nearest unreal-v* tag it finds here)"
  fi
}

# ------------------------------------------------------------------------------------------------------------ run
ue_root="" ue_version="" tree_state=clean sign_sha1=""
target="local build only (--no-publish)"
if [[ $publish -eq 1 ]]; then
  target="GitHub $repo, tag $tag"
  if [[ $draft -eq 1 ]]; then target="$target (draft)"; fi
fi
af_log "Abyssfire $version ($config) from ${sha:0:8} -> $target"
preflight
af_log "preflight ok; starting at step '$from'; output in $AF_OUT_DIR"
mkdir -p "$log_root"

if [[ $first -le 2 ]]; then
  # The kit deletes unreal/Binaries, Intermediate and Saved when the engine or Xcode changed since the last build (or
  # AF_CLEAN_BUILD=1). Do that now, once: left to the package step it would delete the editor binaries the content
  # step has just built.
  AF_UE_HOST=Mac
  af_ue_toolchain_stamp
  unset AF_CLEAN_BUILD
fi
if [[ $first -le 1 ]]; then step_content; fi
if [[ $first -le 2 ]]; then step_package; fi
if [[ $first -le 3 ]]; then step_dmg; fi
step_publish

times=""
for t in ${step_times[@]+"${step_times[@]}"}; do times="$times; $t"; done
af_log "total $(elapsed "$SECONDS")$times"
