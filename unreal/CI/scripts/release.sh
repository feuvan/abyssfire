#!/usr/bin/env bash
# CI job `release` (tags vX.Y.Z[-pre] only; GitLab SaaS runner, glab image registry.gitlab.com/gitlab-org/cli) —
# uploads every installer to the project's generic package registry (package "abyssfire", version = AF_VERSION) and
# creates the GitLab Release with one link per file plus SHA256SUMS.txt.
#
# Input: $AF_OUT_DIR/installers/{mac,windows,android,ios}/ (artifacts of the installer jobs)
# Auth:  CI_JOB_TOKEN only: curl uploads with JOB-TOKEN, glab uses it through GLAB_ENABLE_CI_AUTOLOGIN=true (set by
#        the job; a GITLAB_TOKEN / GITLAB_ACCESS_TOKEN / OAUTH_TOKEN variable would take precedence - do not define
#        one). Re-running the job re-uploads the files; an existing release is left as is (delete it to recreate it).
#        release-cli (deprecated since GitLab 18.0, removed in 20.0) is only a fallback when glab is missing.
#   AF_RELEASE_DRY_RUN=1   print what would be uploaded / created, touch nothing (also usable outside CI)
#   AF_RELEASE_REQUIRE     space-separated file patterns that must exist (default: the five required installers)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/common.sh
source "$here/../lib/common.sh"
af_load_version

dry=0
if af_is_true "${AF_RELEASE_DRY_RUN:-0}"; then dry=1; fi
if [[ $dry -eq 0 ]]; then
  af_require_cmd curl
  af_require_vars CI_COMMIT_TAG CI_JOB_TOKEN CI_API_V4_URL CI_PROJECT_ID CI_PROJECT_PATH
  [[ "${AF_IS_RELEASE:-0}" == "1" ]] || af_die "tag '$CI_COMMIT_TAG' is not vX.Y.Z[-pre]: no release"
fi
tag="${CI_COMMIT_TAG:-v$AF_VERSION}"
inst="$AF_OUT_DIR/installers"
[[ -d "$inst" ]] || af_die "$inst missing (installer job artifacts)"

# Required deliverables: a release never goes out with a platform missing.
required="${AF_RELEASE_REQUIRE:-*-macos-arm64.dmg *-win64-setup.exe *-win64.msi *-android-arm64.apk *-android.aab}"
missing=()
for pattern in $required; do
  if ! find "$inst" -type f -name "$pattern" | grep -q .; then missing+=("$pattern"); fi
done
if [[ ${#missing[@]} -gt 0 ]]; then af_die "missing installers: ${missing[*]}"; fi

# Windows Authenticode signing is optional (CI.md 4.2): installer:windows records whether it signed.
win_signed=1
if grep -q '^signed=0' "$inst/windows/SIGNING-windows.txt" 2>/dev/null; then win_signed=0; fi
win_note=""
if [[ $win_signed -eq 0 ]]; then win_note=" (unsigned: SmartScreen will warn)"; fi

label_for() {
  case "$1" in
    *-macos-arm64.dmg) echo "macOS (Apple Silicon) - .dmg" ;;
    *-win64-setup.exe) echo "Windows x64 installer - .exe$win_note" ;;
    *-win64.msi) echo "Windows x64 installer - .msi$win_note" ;;
    *-win64-portable.zip) echo "Windows x64 portable - .zip$win_note" ;;
    *-android-arm64.apk) echo "Android (arm64) - .apk" ;;
    *-android.aab) echo "Android App Bundle (Google Play) - .aab" ;;
    # App Store distribution profile: installable only through App Store Connect / TestFlight
    *-ios.ipa) echo "iOS - App Store Connect upload (not installable)" ;;
    SHA256SUMS.txt) echo "SHA-256 checksums" ;;
    *) echo "$1" ;;
  esac
}

work="$AF_OUT_DIR/work/release"
rm -rf "$work"
mkdir -p "$work"
files=()
while IFS= read -r -d '' f; do files+=("$f"); done < <(
  find "$inst" -type f \( -name '*.dmg' -o -name '*.exe' -o -name '*.msi' -o -name '*.zip' -o -name '*.apk' \
    -o -name '*.aab' -o -name '*.ipa' \) -print0 | LC_ALL=C sort -z
)
: >"$work/SHA256SUMS.txt"
af_checksum_into "$work/SHA256SUMS.txt" "${files[@]}"
files+=("$work/SHA256SUMS.txt")

base="${CI_API_V4_URL:-https://gitlab.example/api/v4}/projects/${CI_PROJECT_ID:-0}/packages/generic/abyssfire/$AF_VERSION"
links=() # one JSON object per release link
table=""
for f in "${files[@]}"; do
  name="$(basename "$f")"
  url="$base/$name"
  label="$(label_for "$name")"
  size="$(af_file_size "$f")"
  if [[ $dry -eq 1 ]]; then
    af_log "[dry run] upload $name ($size bytes) -> $url"
  else
    af_log "upload $name ($size bytes)"
    curl --fail-with-body --silent --show-error --retry 3 --retry-all-errors \
      --header "JOB-TOKEN: $CI_JOB_TOKEN" --upload-file "$f" "$url" >/dev/null
  fi
  links+=("{\"name\":\"$label\",\"url\":\"$url\",\"link_type\":\"package\",\"direct_asset_path\":\"/$name\"}")
  table="$table| $label | [\`$name\`]($url) | $(awk -v b="$size" 'BEGIN { if (b >= 1048576) printf "%.1f MiB", b / 1048576; else printf "%.0f KiB", b / 1024 }') |
"
done

notes="${CI_COMMIT_TAG_MESSAGE:-}"
pre=""
if [[ "${AF_IS_PRERELEASE:-0}" == "1" ]]; then pre="> **Pre-release / 预发布版本** — for testing.

"; fi
description="$(
  cat <<EOF
${pre}${notes:+$notes

}## 下载 / Downloads

| Platform | File | Size |
|---|---|---|
${table}
## 安装说明 / Install notes

* **macOS 14.5+ (Apple Silicon)**: open the .dmg and drag Abyssfire to Applications (signed with Developer ID and notarised).
* **Windows 10/11 x64**: run the setup .exe (Chinese / English UI) or the .msi (\`msiexec /i ... /qn\` for silent installs) - not both: each refuses to install over the other; the portable .zip needs no installation. Saves stay in %LOCALAPPDATA%\\Abyssfire.$(if [[ $win_signed -eq 0 ]]; then printf ' This release is not Authenticode-signed: SmartScreen shows "Windows protected your PC" (More info > Run anyway).'; fi)
* **Android 8.0+ (arm64, Vulkan)**: sideload the .apk; the .aab is the Google Play upload.
* Verify downloads with \`sha256sum -c SHA256SUMS.txt\`.

Built from \`${CI_COMMIT_SHA:-$AF_GIT_SHA}\` by pipeline ${CI_PIPELINE_URL:-(local)}.
EOF
)"
printf '%s\n' "$description" >"$AF_OUT_DIR/release-notes.md"

json="["
for l in "${links[@]}"; do json="$json$l,"; done
json="${json%,}]"
if [[ $dry -eq 1 ]]; then
  af_log "[dry run] release '$tag' with ${#files[@]} links; notes in $AF_OUT_DIR/release-notes.md"
  printf '%s\n' "$json" >&2
  exit 0
fi

name="Abyssfire $AF_VERSION"
if command -v glab >/dev/null 2>&1; then
  # GLAB_ENABLE_CI_AUTOLOGIN=true (job variable): glab sends CI_JOB_TOKEN as JOB-TOKEN to the Releases API
  export GITLAB_HOST="${GITLAB_HOST:-${CI_SERVER_URL:-}}"
  if glab release view "$tag" -R "$CI_PROJECT_PATH" >/dev/null 2>&1; then
    af_warn "release $tag already exists: files were re-uploaded, the release was left unchanged"
  else
    glab release create "$tag" -R "$CI_PROJECT_PATH" --name "$name" --notes "$description" \
      --ref "${CI_COMMIT_SHA:-$tag}" --assets-links "$json" || af_die "glab release create failed"
  fi
elif command -v release-cli >/dev/null 2>&1; then
  af_warn "glab not found: falling back to the deprecated release-cli"
  rc_links=()
  for l in "${links[@]}"; do rc_links+=(--assets-link "$l"); done
  if ! release-cli create --name "$name" --tag-name "$tag" --ref "${CI_COMMIT_SHA:-$tag}" \
    --description "$description" "${rc_links[@]}"; then
    if release-cli get --tag-name "$tag" >/dev/null 2>&1; then
      af_warn "release $tag already exists: files were re-uploaded, the release was left unchanged"
    else
      af_die "release-cli create failed"
    fi
  fi
else
  af_die "neither glab nor release-cli is installed in this image (AF_IMAGE_RELEASE)"
fi
af_log "release $tag created with ${#files[@]} assets"
