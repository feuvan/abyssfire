#!/usr/bin/env bash
# Installs the Android SDK build-tools (zipalign with 16 KB page alignment, apksigner) into ANDROID_HOME for the
# engine-free `installer:android` job (Docker image with a JDK 21). Idempotent: a cached SDK is reused.
#
#   ANDROID_HOME                  default $CI_PROJECT_DIR/.android-sdk (cached by the job)
#   AF_ANDROID_BUILD_TOOLS        build-tools version (default 36.0.0; >= 35 needed for `zipalign -P 16`)
#   AF_ANDROID_CMDLINE_TOOLS_ZIP  command-line tools download URL (Android Studio "Command line tools only")
set -euo pipefail
# shellcheck source=../lib/common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

sdk="${ANDROID_HOME:-${CI_PROJECT_DIR:-$AF_REPO_DIR}/.android-sdk}"
bt="${AF_ANDROID_BUILD_TOOLS:-36.0.0}"
url="${AF_ANDROID_CMDLINE_TOOLS_ZIP:-https://dl.google.com/android/repository/commandlinetools-linux-13114758_latest.zip}"

if [[ -x "$sdk/build-tools/$bt/apksigner" && -x "$sdk/build-tools/$bt/zipalign" ]]; then
  af_log "Android build-tools $bt already in $sdk"
  exit 0
fi
af_require_cmd curl unzip java
mkdir -p "$sdk/cmdline-tools"
if [[ ! -x "$sdk/cmdline-tools/latest/bin/sdkmanager" ]]; then
  af_log "downloading Android command-line tools"
  tmp="$(mktemp -d)"
  curl -fsSL --retry 3 -o "$tmp/clt.zip" "$url"
  unzip -q "$tmp/clt.zip" -d "$tmp"
  rm -rf "$sdk/cmdline-tools/latest"
  mv "$tmp/cmdline-tools" "$sdk/cmdline-tools/latest"
  rm -rf "$tmp"
fi
sdkmanager="$sdk/cmdline-tools/latest/bin/sdkmanager"
# `yes` exits on SIGPIPE once sdkmanager stops reading; that is expected under pipefail
(yes || true) | "$sdkmanager" --sdk_root="$sdk" --licenses >/dev/null
"$sdkmanager" --sdk_root="$sdk" --install "build-tools;$bt" >/dev/null
[[ -x "$sdk/build-tools/$bt/apksigner" ]] || af_die "build-tools $bt did not install"
af_log "Android build-tools $bt installed in $sdk"
