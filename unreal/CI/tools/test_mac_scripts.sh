#!/usr/bin/env bash
# shellcheck disable=SC2016 # the stub bodies below are literal scripts, expanded when the stubs run
# Logic test of the macOS-runner job scripts on Linux (or a Mac without Unreal): ue-content.sh, ue-package.sh
# (Mac / Android / IOS) and mac-dmg.sh run against a fake engine (RunUAT.sh, Build.sh, UnrealEditor-Cmd) and stub
# Apple tools (security, codesign, xcrun notarytool / stapler, hdiutil, ditto, PlistBuddy, sips, tiffutil, spctl,
# lipo, uname), with every signing secret set to throw-away values. It checks the flow, not Apple's tools:
# outputs and names, version stamping, inside-out signing order, two notarisations + staples, keychain handling
# (non-extractable import, locked outside signing, stale search-list entries dropped, after_script reset), the local
# signing path (an identity of the user's own keychains, a notarytool keychain profile: no temporary keychain), that no
# signing secret reaches the engine, the pak check of the runtime Data/ + Fonts/ files, the Android throw-away key
# and universal APK, the toolchain stamp, ini / keystore / Intermediate/Android clean-up, the pinned DMG helper
# requirements, and that failing steps fail the job.
#
#   unreal/CI/tools/test_mac_scripts.sh [WORKDIR]
# Tools: bash, python3, zip/unzip. Nothing outside WORKDIR is touched (HOME is redirected).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ci="$(cd "$here/.." && pwd)"
W="${1:-$(mktemp -d "${TMPDIR:-/tmp}/af-macjobs-XXXXXX")}"
mkdir -p "$W"
W="$(cd "$W" && pwd)"
rm -rf "${W:?}/unreal" "${W:?}/UE" "${W:?}/bin" "${W:?}/home" "${W:?}/Volumes" "${W:?}/calls.log"
mkdir -p "$W/unreal" "$W/bin" "$W/home" "$W/Volumes"
fails=0
ok() { echo "  ok   $*"; }
bad() {
  echo "  FAIL $*"
  fails=$((fails + 1))
}
check() { # check DESCRIPTION COMMAND... -> ok / FAIL
  local what="$1"
  shift
  if "$@"; then ok "$what"; else bad "$what"; fi
}
has_call() { grep -q -- "$1" "$W/calls.log"; }
# not grep -q: it exits at the first match, and unzip then dies of SIGPIPE under pipefail on a busy machine
in_dmg() { unzip -l "$dmg" | grep -- "$1" >/dev/null; }

# ---------------------------------------------------------------------------------- project skeleton (copy of CI)
mkdir -p "$W/unreal/CI"
# copy the CI sources only (WORKDIR may itself live under unreal/CI/out)
for d in lib scripts installers windows tools version.sh; do cp -R "$ci/$d" "$W/unreal/CI/"; done
mkdir -p "$W/unreal/Config/Android" "$W/unreal/Config/IOS" "$W/unreal/Scripts" "$W/unreal/Data" "$W/unreal/Fonts"
echo '{}' >"$W/unreal/Data/items.json"
echo 'docs, not a runtime file' >"$W/unreal/Data/README.md"
echo 'ttf' >"$W/unreal/Fonts/Test-Regular.ttf"
echo '{ "FileVersion": 3 }' >"$W/unreal/Abyssfire.uproject"
printf '[/Script/EngineSettings.GeneralProjectSettings]\nProjectVersion=0.1.0\n' >"$W/unreal/Config/DefaultGame.ini"
printf '[/Script/Engine.UserInterfaceSettings]\nApplicationScale=1.0\n' >"$W/unreal/Config/Android/AndroidEngine.ini"
printf '[/Script/Engine.UserInterfaceSettings]\nApplicationScale=1.0\n' >"$W/unreal/Config/IOS/IOSEngine.ini"
echo 'print("build_content")' >"$W/unreal/Scripts/build_content.py"
cp "$W/unreal/Config/DefaultGame.ini" "$W/DefaultGame.ini.orig"
cp "$W/unreal/Config/Android/AndroidEngine.ini" "$W/AndroidEngine.ini.orig"
cp "$W/unreal/Config/IOS/IOSEngine.ini" "$W/IOSEngine.ini.orig"

# ------------------------------------------------------------------- fake engine + Apple stubs ($W/UE, $W/bin)
# shellcheck source=fake_mac.sh
source "$here/fake_mac.sh"

# ------------------------------------------------------------------------------------------------- environment
b64() { printf '%s' "$1" | base64 | tr -d '\n'; }
export PATH="$W/bin:$PATH" HOME="$W/home" AF_TEST_CALLS="$W/calls.log" AF_TEST_VOLUMES="$W/Volumes"
export AF_PLISTBUDDY="$W/bin/PlistBuddy" UE_ROOT_MAC="$UE" GITLAB_CI=1 AF_PERSISTENT_DDC="$W/ddc"
export CI_COMMIT_TAG=v1.2.3 CI_PIPELINE_IID=42 CI_COMMIT_SHORT_SHA=abcdef12 CI_COMMIT_REF_PROTECTED=true
AF_MAC_DEVID_P12_B64="$(b64 P12DATA)"
AF_ASC_API_KEY_P8_B64="$(b64 P8KEY)"
AF_ANDROID_KEYSTORE_B64="$(b64 KEYSTORE)"
AF_IOS_DIST_P12_B64="$(b64 P12IOS)"
AF_IOS_PROVISION_PROFILE_B64="$(b64 PROFILE)"
export AF_MAC_DEVID_P12_B64 AF_ASC_API_KEY_P8_B64 AF_ANDROID_KEYSTORE_B64 AF_IOS_DIST_P12_B64 AF_IOS_PROVISION_PROFILE_B64
export AF_MAC_DEVID_P12_PASSWORD=p12-password AF_ASC_API_KEY_ID=KEYID12345 AF_ASC_API_ISSUER_ID=issuer-uuid-0000
export AF_ANDROID_KEYSTORE_PASSWORD=store-pass AF_ANDROID_KEY_ALIAS=abyssfire
export AF_APPLE_TEAM_ID=TEAM123456 AF_IOS_DIST_P12_PASSWORD=ios-pass
unset AF_VERSION AF_OUT_DIR AF_LOG_DIR
OUT="$W/unreal/CI/out"
run_job() { # run_job SLUG SCRIPT ARGS...
  local slug="$1"
  shift
  CI_JOB_NAME_SLUG="$slug" bash "$W/unreal/CI/scripts/$1" "${@:2}" >"$W/$slug.log" 2>&1
}

# installer-assets (the SaaS job) - the DMG and Android steps consume it
python3 "$W/unreal/CI/installers/assets/make_installer_assets.py" --out "$OUT/installer-assets" --version 1.2.3 >/dev/null

echo "=== ue-content.sh"
if run_job ue-content ue-content.sh; then ok "exit 0"; else bad "exit $? (see $W/ue-content.log)"; fi
check "report + summary" test -f "$OUT/content/report.json" -a -f "$OUT/content/summary.txt"
check "editor target built first" has_call "Build.sh AbyssfireEditor Mac Development"
check "toolchain stamp written" test -s "$W/unreal/Intermediate/.af-toolchain-stamp"
check "Metal toolchain checked" has_call "xcrun -sdk macosx metal --version"

echo "=== ue-package.sh Mac"
if run_job package-mac ue-package.sh Mac; then ok "exit 0"; else bad "exit $? (see $W/package-mac.log)"; fi
check "Abyssfire.app.zip" test -f "$OUT/package/mac/Abyssfire.app.zip"
check "arm64, -NoCodeSign" has_call "-specifiedarchitecture=arm64 -NoCodeSign"
check "Shipping cook/stage/pak/iostore/package/archive" \
  has_call "-clientconfig=Shipping -build -cook -stage -pak -iostore -compressed -package -archive"
check "DefaultGame.ini restored" cmp -s "$W/unreal/Config/DefaultGame.ini" "$W/DefaultGame.ini.orig"

echo "=== mac-dmg.sh"
if run_job installer-mac-dmg mac-dmg.sh; then ok "exit 0"; else bad "exit $? (see $W/installer-mac-dmg.log)"; fi
dmg="$OUT/installers/mac/Abyssfire-1.2.3-macos-arm64.dmg"
check "$(basename "$dmg")" test -f "$dmg"
check "SHA256SUMS-mac.txt" test -f "$OUT/installers/mac/SHA256SUMS-mac.txt"
check "notarised twice (app, dmg)" test "$(grep -c 'notarytool submit' "$W/calls.log")" = 2
check "stapled twice" test "$(grep -c 'stapler staple' "$W/calls.log")" = 2
grep "^codesign --force" "$W/calls.log" >"$W/sign-order.txt"
first_lib="$(grep -n 'libtbb.dylib\|libfoo' "$W/sign-order.txt" | head -1 | cut -d: -f1)"
app_line="$(grep -n 'entitlements' "$W/sign-order.txt" | head -1 | cut -d: -f1)"
check "inside-out signing (libs before the app)" test "${first_lib:-999}" -lt "${app_line:-0}"
check "hardened runtime + timestamp + temp keychain" has_call "--options runtime --timestamp --keychain"
check "DMG signed with the Developer ID identity" \
  grep -q "codesign --force --timestamp --keychain .* --sign ABCDEF0123456789ABCDEF0123456789ABCDEF01 .*\.dmg" "$W/calls.log"
check "p12 imported non-extractable, codesign / productbuild only" \
  grep -q "security import .* -x .*-T /usr/bin/codesign -T /usr/bin/productbuild" "$W/calls.log"
check "no -T /usr/bin/security" bash -c "! grep -q -- '-T /usr/bin/security' '$W/calls.log'"
check "keychain auto-locks after 30 min" has_call "security set-keychain-settings -lut 1800"
dmg_lock_order() { # lock after the app signature, unlock -> sign the DMG -> lock
  grep -E '^security (lock|unlock)-keychain|^codesign --force .*\.dmg' "$W/calls.log" | sed 's/ .*//; s/^codesign/sign/' |
    tr '\n' ' ' | grep -q 'security security sign security'
}
check "keychain locked except while signing" dmg_lock_order
check "notarytool uses the key decoded once" grep -q "notarytool submit .* --key $OUT/work/dmg/secrets/AuthKey_ci.p8" "$W/calls.log"
check "DMG holds Abyssfire.app (renamed)" in_dmg "Abyssfire.app/Contents/Info.plist"
check "DMG has the Applications link" in_dmg " Applications$"
check "DMG background" in_dmg ".background/background.tiff"
plist_ok() {
  unzip -p "$dmg" Abyssfire.app/Contents/Info.plist | python3 -c '
import plistlib, sys
d = plistlib.loads(sys.stdin.buffer.read())
assert d["CFBundleShortVersionString"] == "1.2.3" and d["CFBundleVersion"] == "42", d
assert d["CFBundleIconFile"] == "Abyssfire" and "CFBundleIconName" not in d, d'
}
check "Info.plist version 1.2.3 (42) + icon" plist_ok
check "temporary keychain deleted" has_call "security delete-keychain"
check "no key files left" test -z "$(find "$OUT/work" \( -name '*.p8' -o -name '*.p12' \) 2>/dev/null)"

echo "=== ue-package.sh Android (distribution build, throw-away key)"
if run_job package-android ue-package.sh Android; then ok "exit 0"; else bad "exit $? (see $W/package-android.log)"; fi
check "apk (universal) + aab" test -f "$OUT/package/android/Abyssfire.apk" -a -f "$OUT/package/android/Abyssfire.aab"
check "ASTC, -distribution" grep -q -- "-platform=Android .* -cookflavor=ASTC -distribution" "$W/calls.log"
check "AndroidEngine.ini restored" cmp -s "$W/unreal/Config/Android/AndroidEngine.ini" "$W/AndroidEngine.ini.orig"
check "throw-away keystore removed" test ! -e "$W/unreal/Build/Android/abyssfire-ci-throwaway.keystore"
check "Intermediate/Android removed" test ! -e "$W/unreal/Intermediate/Android"
check "temporary launcher icons removed" test ! -e "$W/unreal/Build/Android/res"
check "pak check passed" grep -q "pak holds all 2 runtime" "$W/package-android.log"

echo "=== ue-package.sh IOS"
if run_job package-ios ue-package.sh IOS; then ok "exit 0"; else bad "exit $? (see $W/package-ios.log)"; fi
check "Abyssfire-1.2.3-ios.ipa" test -f "$OUT/package/ios/Abyssfire-1.2.3-ios.ipa"
check "IOSEngine.ini restored" cmp -s "$W/unreal/Config/IOS/IOSEngine.ini" "$W/IOSEngine.ini.orig"
check "provisioning profile removed" test -z "$(find "$W/home/Library" -name '*.mobileprovision' 2>/dev/null)"
check "iOS keychain stays unlocked for the build" has_call "security set-keychain-settings -lut 15000"
check "iOS platform checked" has_call "xcrun --sdk iphoneos --show-sdk-path"

echo "=== failure paths"
if AF_TEST_UAT_FAIL=1 run_job package-mac-fail ue-package.sh Mac; then bad "failing RunUAT did not fail the job"; else
  ok "failing RunUAT fails the job"
fi
check "ini restored after failure" cmp -s "$W/unreal/Config/DefaultGame.ini" "$W/DefaultGame.ini.orig"
if AF_TEST_DROP=Fonts/Test-Regular.ttf run_job package-mac-nodata ue-package.sh Mac; then
  bad "a pak without Fonts/Test-Regular.ttf was accepted"
else ok "a runtime file missing from the pak fails the job"; fi
if env -u AF_PERSISTENT_DDC CI_JOB_NAME_SLUG=no-ddc bash "$W/unreal/CI/scripts/ue-package.sh" Mac >"$W/no-ddc.log" 2>&1; then
  bad "CI job without AF_PERSISTENT_DDC accepted"
else ok "AF_PERSISTENT_DDC is mandatory in CI"; fi
if AF_TEST_NO_METAL=1 run_job no-metal ue-package.sh Mac; then bad "missing Metal toolchain not detected"; else
  ok "missing Metal toolchain fails early"
fi
echo "an older engine" >"$W/unreal/Intermediate/.af-toolchain-stamp"
echo stale >"$W/unreal/Intermediate/stale.marker"
if run_job package-mac-stamp ue-package.sh Mac; then ok "exit 0 after an engine change"; else bad "rebuild after an engine change failed"; fi
check "stale Intermediate wiped after an engine change" test ! -e "$W/unreal/Intermediate/stale.marker"
if env -u AF_MAC_DEVID_P12_B64 CI_JOB_NAME_SLUG=dmg-unsigned bash "$W/unreal/CI/scripts/mac-dmg.sh" >"$W/dmg-unsigned.log" 2>&1; then
  bad "unsigned DMG accepted on a release tag"
else ok "release tag without Developer ID is refused"; fi
if env -u AF_MAC_DEVID_P12_B64 -u CI_COMMIT_TAG CI_JOB_NAME_SLUG=dmg-dev bash "$W/unreal/CI/scripts/mac-dmg.sh" >"$W/dmg-dev.log" 2>&1; then
  ok "development build: ad-hoc signed, unnotarised DMG"
else bad "development DMG failed (see $W/dmg-dev.log)"; fi
check "development DMG named 0.0.0-<sha>" test -f "$OUT/installers/mac/Abyssfire-0.0.0-abcdef12-macos-arm64.dmg"
check "ad-hoc identity '-' used" has_call "--sign - "
check "ad-hoc signatures without the hardened runtime" bash -c "! grep -- '--sign - ' '$W/calls.log' | grep -q -- '--options runtime'"

echo "=== mac-dmg.sh, local signing (an identity of the user's keychains + a notarytool keychain profile)"
# what unreal/Scripts/release_mac.sh --sign-identity / --notary-profile does: no CI secret, no tag, not a GitLab job
cp "$W/calls.log" "$W/calls-ci.log"
: >"$W/calls.log"
local_dmg() { # local_dmg SLUG VAR=VALUE... -> mac-dmg.sh without any CI signing secret, outside GitLab
  local slug="$1"
  shift
  env -u AF_MAC_DEVID_P12_B64 -u AF_MAC_DEVID_P12_PASSWORD -u AF_ASC_API_KEY_P8_B64 -u AF_ASC_API_KEY_ID \
    -u AF_ASC_API_ISSUER_ID -u CI_COMMIT_TAG -u GITLAB_CI CI_JOB_NAME_SLUG="$slug" "$@" \
    bash "$W/unreal/CI/scripts/mac-dmg.sh" >"$W/$slug.log" 2>&1
}
local_id="Developer ID Application: Test (TEAM123456)"
if local_dmg dmg-local AF_MAC_SIGN_IDENTITY="$local_id" AF_NOTARY_KEYCHAIN_PROFILE=af-notary; then ok "exit 0"; else
  bad "exit $? (see $W/dmg-local.log)"
fi
check "app signed with the named identity, hardened runtime + timestamp" \
  has_call "codesign --force --options runtime --timestamp --sign $local_id .*Abyssfire.app\$"
check "DMG signed with the named identity" has_call "codesign --force --timestamp --sign $local_id .*\.dmg\$"
check "no --keychain argument" bash -c "! grep -q -- '--keychain ' '$W/calls.log'"
check "no temporary keychain, no lock / unlock, search list untouched" bash -c \
  "! grep -Eq '^security (create-keychain|import|unlock-keychain|lock-keychain|set-keychain-settings|delete-keychain|list-keychains .* -s)' '$W/calls.log'"
check "identity looked up in the user's search list" has_call "^security find-identity -v -p codesigning\$"
check "notarised twice with the keychain profile" \
  test "$(grep -c 'notarytool submit .* --keychain-profile af-notary --wait' "$W/calls.log")" = 2
check "no API key handed to notarytool" bash -c "! grep -q -- 'notarytool .* --key ' '$W/calls.log'"
check "stapled twice" test "$(grep -c 'stapler staple' "$W/calls.log")" = 2
check "no key files left" test -z "$(find "$OUT/work" \( -name '*.p8' -o -name '*.p12' \) 2>/dev/null)"
: >"$W/calls.log"
if local_dmg dmg-local-signed AF_MAC_SIGN_IDENTITY="$local_id"; then ok "identity without a profile: signed, not notarised"; else
  bad "identity without a profile failed (see $W/dmg-local-signed.log)"
fi
check "no notarytool call without a profile" bash -c "! grep -q 'notarytool' '$W/calls.log'"
if local_dmg dmg-local-unknown AF_MAC_SIGN_IDENTITY="Developer ID Application: Nobody (XXXXXXXXXX)"; then
  bad "an identity that is in no keychain was accepted"
else ok "an identity that is in no keychain is refused"; fi
if local_dmg dmg-local-noid AF_NOTARY_KEYCHAIN_PROFILE=af-notary; then bad "a notary profile without a signature was accepted"; else
  ok "a notary profile without a signing identity is refused"
fi
if local_dmg dmg-local-dist AF_MAC_SIGN_IDENTITY="Apple Distribution: Test (TEAM123456)" AF_NOTARY_KEYCHAIN_PROFILE=af-notary; then
  bad "notarisation with an identity that is not Developer ID was accepted"
else ok "notarisation with an identity that is not Developer ID is refused"; fi
if AF_MAC_SIGN_IDENTITY="$local_id" run_job dmg-ci-named mac-dmg.sh; then ok "CI path with AF_MAC_SIGN_IDENTITY (p12 present)"; else
  bad "CI path with AF_MAC_SIGN_IDENTITY failed (see $W/dmg-ci-named.log)"
fi
check "the p12 still goes into the temporary keychain" has_call "security import .* -x "
check "and signing uses that keychain" has_call "--timestamp --keychain .* --sign $local_id "
# A GitLab job that does not get the protected p12 (an unprotected branch) but sees the identity's name, a plain
# variable: ad-hoc as before the local path existed, whatever the runner's own keychain holds.
: >"$W/calls.log"
if env -u AF_MAC_DEVID_P12_B64 -u CI_COMMIT_TAG CI_JOB_NAME_SLUG=dmg-ci-nop12 AF_MAC_SIGN_IDENTITY="$local_id" \
  AF_NOTARY_KEYCHAIN_PROFILE=af-notary bash "$W/unreal/CI/scripts/mac-dmg.sh" >"$W/dmg-ci-nop12.log" 2>&1; then
  ok "GitLab job with the identity's name but no p12: exit 0"
else bad "GitLab job with the identity's name but no p12 failed (see $W/dmg-ci-nop12.log)"; fi
check "... ad-hoc signed, never with the runner's own Developer ID" bash -c \
  "grep -q -- '--sign - ' '$W/calls.log' && ! grep -q -- '--sign $local_id' '$W/calls.log' && ! grep -q '^security find-identity' '$W/calls.log'"
check "... not notarised, and the log says the names were ignored" bash -c \
  "! grep -q notarytool '$W/calls.log' && grep -q 'ignored in a GitLab job' '$W/dmg-ci-nop12.log'"
if env -u AF_MAC_DEVID_P12_B64 CI_JOB_NAME_SLUG=dmg-ci-nop12-tag AF_MAC_SIGN_IDENTITY="$local_id" \
  bash "$W/unreal/CI/scripts/mac-dmg.sh" >"$W/dmg-ci-nop12-tag.log" 2>&1; then
  bad "release tag in GitLab without the p12 was signed from the runner's keychain"
else ok "... and on a release tag the p12 stays mandatory"; fi

echo "=== make_installer_assets.py: a Pillow that passes the probe and fails while drawing"
if python3 - "$ci/installers/assets/make_installer_assets.py" "$W/assets-fallback" >"$W/assets-fallback.log" 2>&1 <<'EOF'; then
import importlib.util, sys
spec = importlib.util.spec_from_file_location("mia", sys.argv[1])
mia = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mia)
def no_freetype(*args):
    raise OSError("cannot open resource")  # what ImageFont.truetype says without FreeType
def stdlib(out, version):
    (out / "dmg-background.png").write_bytes(b"png")
    return "procedural emblem (standard-library renderer)"
mia.pillow_unusable = lambda: ""
mia.build_pillow = no_freetype
mia.build_stdlib = stdlib
sys.argv = [sys.argv[1], "--out", sys.argv[2], "--version", "1.2.3"]
sys.exit(mia.main())
EOF
  ok "exit 0"
else bad "the artwork step died with its Pillow renderer (see $W/assets-fallback.log)"; fi
check "the standard-library renderer took over, and the log says why" bash -c \
  "grep -q 'standard-library' '$W/assets-fallback/source.txt' && grep -q 'Pillow renderer failed (OSError' '$W/assets-fallback.log'"

echo "=== mac-keychain-reset.sh (after_script)"
printf '%s\n' "$HOME/Library/Keychains/login.keychain-db" "$OUT/work/dmg/keychain/af-ci-signing.keychain-db" \
  >"$HOME/keychain-search-list"
mkdir -p "$OUT/work/dmg/keychain" && touch "$OUT/work/dmg/keychain/af-ci-signing.keychain-db"
if CI_JOB_NAME_SLUG=reset bash "$W/unreal/CI/scripts/mac-keychain-reset.sh" >"$W/reset.log" 2>&1; then ok "exit 0"; else bad "exit $?"; fi
check "stale keychain dropped from the search list" bash -c "! grep -q af-ci-signing '$HOME/keychain-search-list'"
check "login keychain kept" grep -q login.keychain-db "$HOME/keychain-search-list"
check "leftover keychain deleted" has_call "security delete-keychain $OUT/work/dmg/keychain/af-ci-signing.keychain-db"

echo "=== dmg-requirements.txt (hash-pinned DMG helper wheels)"
if python3 -m venv "$W/req-venv" >/dev/null 2>&1; then
  if "$W/req-venv/bin/pip" install --quiet --disable-pip-version-check --require-hashes --no-deps --only-binary :all: \
    -r "$ci/installers/mac/dmg-requirements.txt" >"$W/req.log" 2>&1; then
    ok "pinned wheels install with --require-hashes"
  elif grep -qi 'do not match the hashes' "$W/req.log"; then
    bad "dmg-requirements.txt hashes do not match PyPI (see $W/req.log)"
  else
    echo "  skip pip install failed for another reason (offline?): $W/req.log"
  fi
else
  echo "  skip python3 -m venv unavailable (apt: python3-venv)"
fi

echo
if [[ $fails -gt 0 ]]; then
  echo "FAILED: $fails (logs in $W)"
  exit 1
fi
echo "OK: $W"
