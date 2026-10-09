#!/usr/bin/env bash
# CI job `installer:android` (GitLab SaaS Linux runner, no engine) — zipalign (16 KB pages) + apksigner for the APK,
# jarsigner for the AAB, verification of both.
#
# Input:  $AF_OUT_DIR/package/android/Abyssfire.apk, Abyssfire.aab   (package:android)
# Output: $AF_OUT_DIR/installers/android/Abyssfire-<version>-android-arm64.apk   sideload / testers
#         $AF_OUT_DIR/installers/android/Abyssfire-<version>-android.aab         Google Play (upload key)
#         SHA256SUMS-android.txt, apk-certs.txt
#
# Secrets (masked + protected): AF_ANDROID_KEYSTORE_B64, AF_ANDROID_KEYSTORE_PASSWORD, AF_ANDROID_KEY_ALIAS,
# AF_ANDROID_KEY_PASSWORD (defaults to the store password). Without a keystore (development pipelines) the APK is
# re-signed with a throw-away debug key and the AAB is passed through as UE produced it.
#   AF_ANDROID_BUILD_TOOLS   build-tools version under $ANDROID_HOME (default 36.0.0); else zipalign/apksigner on PATH
#   AF_ANDROID_MIN_SDK       minSdkVersion for apksigner (default 26, DefaultEngine.ini MinSDKVersion)
#   AF_ANDROID_ALLOW_4K=1    accept a zipalign without -P (16 KB page alignment, build-tools >= 35) - local tests only
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/common.sh
source "$here/../lib/common.sh"

bt_dir="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}/build-tools/${AF_ANDROID_BUILD_TOOLS:-36.0.0}"
if [[ -x "$bt_dir/apksigner" ]]; then export PATH="$bt_dir:$PATH"; fi
af_require_cmd zipalign apksigner jarsigner keytool
min_sdk="${AF_ANDROID_MIN_SDK:-26}"

af_load_version
src="$AF_OUT_DIR/package/android"
[[ -f "$src/Abyssfire.apk" ]] || af_die "$src/Abyssfire.apk missing (artifact of package:android)"
[[ -f "$src/Abyssfire.aab" ]] || af_die "$src/Abyssfire.aab missing (artifact of package:android)"
work="$AF_OUT_DIR/work/android-sign"
dst="$AF_OUT_DIR/installers/android"
rm -rf "$work"
mkdir -p "$work/secrets" "$dst"
chmod 700 "$work/secrets"
trap 'rm -rf "$work/secrets"' EXIT
apk_out="$dst/Abyssfire-$AF_VERSION-android-arm64.apk"
aab_out="$dst/Abyssfire-$AF_VERSION-android.aab"
rm -f "$apk_out" "$aab_out"

# ------------------------------------------------------------------------------------------------------- the key
release=0
if af_have AF_ANDROID_KEYSTORE_B64; then
  af_require_vars AF_ANDROID_KEYSTORE_PASSWORD AF_ANDROID_KEY_ALIAS
  ks="$work/secrets/release.keystore"
  af_secret_to_file AF_ANDROID_KEYSTORE_B64 "$ks"
  AF__KS_PASS="$(af_secret_value AF_ANDROID_KEYSTORE_PASSWORD)"
  AF__KEY_PASS="$(af_secret_value AF_ANDROID_KEY_PASSWORD)"
  [[ -n "$AF__KEY_PASS" ]] || AF__KEY_PASS="$AF__KS_PASS"
  alias_name="$(af_secret_value AF_ANDROID_KEY_ALIAS)"
  export AF__KS_PASS AF__KEY_PASS
  keytool -list -keystore "$ks" -storepass:env AF__KS_PASS -alias "$alias_name" >/dev/null ||
    af_die "the keystore has no key '$alias_name' or AF_ANDROID_KEYSTORE_PASSWORD is wrong"
  release=1
elif af_signing_required; then
  af_die "release builds need AF_ANDROID_KEYSTORE_B64 (+ _PASSWORD, _KEY_ALIAS); AF_REQUIRE_SIGNING=0 allows debug keys"
else
  af_warn "no release keystore: the APK gets a throw-away debug key (cannot update a Play / release install)"
  ks="$work/secrets/debug.keystore"
  export AF__KS_PASS=android AF__KEY_PASS=android
  alias_name=androiddebugkey
  keytool -genkeypair -keystore "$ks" -storepass:env AF__KS_PASS -keypass:env AF__KEY_PASS -alias "$alias_name" \
    -keyalg RSA -keysize 2048 -validity 3650 -dname "CN=Android Debug,O=Android,C=US" >/dev/null 2>&1
fi

# ----------------------------------------------------------------------------------------------------------- APK
af_section_start apk "APK: zipalign + apksigner"
page=(-P 16)
if ! zipalign 2>&1 | grep -q -- '-P'; then
  if af_is_true "${AF_ANDROID_ALLOW_4K:-0}"; then
    af_warn "this zipalign has no -P (16 KB pages); using -p (4 KB) - NOT acceptable for Play (build-tools >= 35)"
    page=(-p)
  else
    af_die "zipalign without -P support: install build-tools >= 35 (AF_ANDROID_BUILD_TOOLS)"
  fi
fi
# zipalign must run before apksigner (a v2/v3 signature covers the alignment padding)
zipalign -f "${page[@]}" 4 "$src/Abyssfire.apk" "$work/aligned.apk"
apksigner sign --ks "$ks" --ks-key-alias "$alias_name" --ks-pass env:AF__KS_PASS --key-pass env:AF__KEY_PASS \
  --min-sdk-version "$min_sdk" --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true \
  --out "$apk_out" "$work/aligned.apk"
apksigner verify --verbose --print-certs --min-sdk-version "$min_sdk" "$apk_out" | tee "$dst/apk-certs.txt"
grep -Eq 'Verified using v(2|3) scheme.*: true' "$dst/apk-certs.txt" || af_die "APK has no v2/v3 signature"
zipalign -c "${page[@]}" 4 "$apk_out" || af_die "APK alignment check failed"
rm -f "$apk_out.idsig"
af_section_end apk

# ----------------------------------------------------------------------------------------------------------- AAB
af_section_start aab "AAB: jarsigner"
cp -f "$src/Abyssfire.aab" "$work/in.aab"
if [[ $release -eq 1 ]]; then
  # Replace whatever signature UE / Gradle wrote with the upload key (Play App Signing re-signs for devices).
  python3 - "$work/in.aab" "$work/unsigned.aab" <<'PY'
import re, sys, zipfile
src, out = sys.argv[1], sys.argv[2]
sig = re.compile(r"^META-INF/([^/]+\.(SF|RSA|DSA|EC)|MANIFEST\.MF)$", re.I)
with zipfile.ZipFile(src) as zi, zipfile.ZipFile(out, "w") as zo:
    for info in zi.infolist():
        if not sig.match(info.filename):
            zo.writestr(info, zi.read(info.filename))
PY
  jarsigner -keystore "$ks" -storepass:env AF__KS_PASS -keypass:env AF__KEY_PASS -sigalg SHA256withRSA \
    -digestalg SHA-256 -signedjar "$aab_out" "$work/unsigned.aab" "$alias_name" >/dev/null
  jarsigner -verify -strict "$aab_out" >"$work/aab-verify.txt" 2>&1 || true
  grep -q 'jar verified' "$work/aab-verify.txt" || {
    cat "$work/aab-verify.txt" >&2
    af_die "AAB signature does not verify"
  }
else
  cp -f "$work/in.aab" "$aab_out"
  if jarsigner -verify "$aab_out" 2>/dev/null | grep -q 'jar verified'; then
    af_log "AAB keeps UE's (debug) signature"
  else
    af_warn "AAB is unsigned (Play needs the upload key: set AF_ANDROID_KEYSTORE_B64)"
  fi
fi
af_section_end aab

rm -f "$dst/SHA256SUMS-android.txt"
(cd "$dst" && af_checksum_into SHA256SUMS-android.txt "$(basename "$apk_out")" "$(basename "$aab_out")")
af_log "Android (release-signed=$release):"
ls -l "$dst" >&2
