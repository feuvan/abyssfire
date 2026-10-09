#!/usr/bin/env bash
# shellcheck disable=SC2016 # the stub bodies below are literal scripts, expanded when the stubs run
# Logic test of the macOS-runner job scripts on Linux (or a Mac without Unreal): ue-content.sh, ue-package.sh
# (Mac / Android / IOS) and mac-dmg.sh run against a fake engine (RunUAT.sh, Build.sh, UnrealEditor-Cmd) and stub
# Apple tools (security, codesign, xcrun notarytool / stapler, hdiutil, ditto, PlistBuddy, sips, tiffutil, spctl,
# lipo, uname), with every signing secret set to throw-away values. It checks the flow, not Apple's tools:
# outputs and names, version stamping, inside-out signing order, two notarisations + staples, keychain handling
# (non-extractable import, locked outside signing, stale search-list entries dropped, after_script reset), that no
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
in_dmg() { unzip -l "$dmg" | grep -q -- "$1"; }

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

# ------------------------------------------------------------------------------------------------- fake engine
UE="$W/UE"
mkdir -p "$UE/Engine/Build/BatchFiles/Mac" "$UE/Engine/Binaries/Mac"
echo '{ "MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 3, "Changelist": 0 }' >"$UE/Engine/Build/Build.version"
cat >"$UE/Engine/Build/BatchFiles/Mac/Build.sh" <<'EOF'
#!/usr/bin/env bash
echo "Build.sh $*" >>"$AF_TEST_CALLS"
[[ "$1 $2 $3" == "AbyssfireEditor Mac Development" ]] || exit 9
EOF
cat >"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" <<'EOF'
#!/usr/bin/env bash
echo "UnrealEditor-Cmd $*" >>"$AF_TEST_CALLS"
proj="$(dirname "$1")"
mkdir -p "$proj/Content/Abyssfire/Maps" "$proj/Content/Abyssfire/Characters"
echo umap >"$proj/Content/Abyssfire/Maps/L_Main.umap"
echo asset >"$proj/Content/Abyssfire/Characters/SK_Hero_Warrior.uasset"
for a in "$@"; do case "$a" in -abslog=*) echo "LogPython: content built" >"${a#-abslog=}" ;; esac; done
echo '{"ok": true, "assets": 2}' >"$AF_CONTENT_REPORT"
[[ "$(env | grep -c '^UE-LocalDataCachePath=')" == "1" ]] || exit 8
EOF
cat >"$UE/Engine/Build/BatchFiles/RunUAT.sh" <<'EOF'
#!/usr/bin/env bash
echo "RunUAT $*" >>"$AF_TEST_CALLS"
[[ -n "${AF_TEST_UAT_FAIL:-}" ]] && { echo "ERROR: cook failed"; exit 25; }
if env | grep -Eq '^AF_[A-Z_]*(_B64|PASSWORD)='; then echo "a signing secret reached the engine"; exit 19; fi
platform="" archive="" project=""
for a in "$@"; do
  case "$a" in -platform=*) platform="${a#-platform=}" ;; -archivedirectory=*) archive="${a#-archivedirectory=}" ;;
    -project=*) project="${a#-project=}" ;; esac
done
u="$(dirname "$project")"
grep -q "ProjectVersion=$AF_VERSION" "$u/Config/DefaultGame.ini" || { echo "no ProjectVersion overlay"; exit 11; }
# the pak response file UAT writes into uebp_LogFolder (AF_TEST_DROP: leave one runtime file out)
(cd "$u" && find Data Fonts -type f | sort) | while IFS= read -r f; do
  [[ "$f" == "${AF_TEST_DROP:-}" ]] || printf '"%s" "../../../Abyssfire/%s" -compress\n' "$u/$f" "$f"
done >"$uebp_LogFolder/PakList_pakchunk0-$platform.txt"
case "$platform" in
  Mac)
    app="$archive/Mac/Abyssfire-Mac-Shipping.app/Contents"
    mkdir -p "$app/MacOS" "$app/Resources" "$app/Frameworks/libfoo.framework" "$app/UE/Engine/Binaries"
    printf 'MACHO' >"$app/MacOS/Abyssfire-Mac-Shipping"; chmod +x "$app/MacOS/Abyssfire-Mac-Shipping"
    printf 'MACHO' >"$app/UE/Engine/Binaries/libtbb.dylib"
    printf 'MACHO' >"$app/Frameworks/libfoo.framework/libfoo"; chmod +x "$app/Frameworks/libfoo.framework/libfoo"
    python3 -c 'import plistlib,sys; plistlib.dump({"CFBundleIdentifier":"com.feuvan.abyssfire","CFBundleExecutable":"Abyssfire-Mac-Shipping","CFBundleIconName":"AppIcon"}, open(sys.argv[1],"wb"))' "$app/Info.plist"
    ;;
  Android)
    ini="$u/Config/Android/AndroidEngine.ini"
    grep -q "StoreVersion=$AF_ANDROID_VERSION_CODE" "$ini" || { echo "no StoreVersion"; exit 12; }
    if [[ " $* " == *" -distribution "* ]]; then
      grep -q "KeyStore=abyssfire-ci-throwaway.keystore" "$ini" || exit 13
      grep -q "KeyAlias=ci" "$ini" || exit 13
      [[ -s "$u/Build/Android/abyssfire-ci-throwaway.keystore" ]] || exit 14
    fi
    [[ -f "$u/Build/Android/res/drawable-xxxhdpi/icon.png" ]] || { echo "no generated icons"; exit 15; }
    # what UEDeployAndroid leaves behind: a copy of the keystore, gradle.properties with the passwords
    mkdir -p "$archive/Android_ASTC" "$u/Binaries/Android" "$u/Intermediate/Android/arm64/gradle"
    echo "STORE_PASSWORD=..." >"$u/Intermediate/Android/arm64/gradle/gradle.properties"
    # like UE in bundle mode: an .apk only through bundletool's universal APK
    if grep -q "bEnableUniversalAPK=True" "$ini"; then echo apk >"$archive/Android_ASTC/Abyssfire-arm64-universal.apk"; fi
    echo aab >"$u/Binaries/Android/Abyssfire-Android-Shipping-arm64.aab"
    ;;
  IOS)
    ini="$u/Config/IOS/IOSEngine.ini"
    grep -q "IOSSigningIdentity=Apple Distribution: Test (TEAM123456)" "$ini" || { echo "no identity"; exit 16; }
    grep -q "CodeSigningTeam=TEAM123456" "$ini" || exit 17
    mkdir -p "$W/ipa/Payload/Abyssfire.app" "$archive/IOS"
    (cd "$W/ipa" && zip -qr "$archive/IOS/Abyssfire.ipa" Payload)
    ;;
esac
echo "BUILD SUCCESSFUL"
EOF
chmod +x "$UE/Engine/Build/BatchFiles/Mac/Build.sh" "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$UE/Engine/Build/BatchFiles/RunUAT.sh"

# ------------------------------------------------------------------------------------------------- Apple stubs
stub() {
  printf '#!/usr/bin/env bash\necho "%s $*" >>"$AF_TEST_CALLS"\n%s\n' "$1" "$2" >"$W/bin/$1"
  chmod +x "$W/bin/$1"
}
stub uname 'echo Darwin'
stub codesign 'exit 0'
stub spctl 'exit 0'
stub sips 'out=""; while [[ $# -gt 0 ]]; do [[ "$1" == "--out" ]] && out="$2"; shift; done; echo tiff >"$out"'
stub tiffutil 'out=""; while [[ $# -gt 0 ]]; do [[ "$1" == "-out" ]] && out="$2"; shift; done; echo tiff >"$out"'
stub SetFile 'exit 0'
stub lipo 'echo arm64'
stub file 'if [[ "$(head -c 5 "${@: -1}" 2>/dev/null)" == "MACHO" ]]; then echo "Mach-O 64-bit executable arm64"; else echo data; fi'
stub security '
state="$HOME/keychain-search-list"
[[ -f "$state" ]] || echo "$HOME/Library/Keychains/login.keychain-db" >"$state"
case "$1" in
  create-keychain) touch "${@: -1}" ;;
  list-keychains) if [[ $# -eq 3 ]]; then sed "s/.*/    \"&\"/" "$state"; else shift 4; printf "%s\n" "$@" >"$state"; fi ;;
  find-identity) echo "  1) ABCDEF0123456789ABCDEF0123456789ABCDEF01 \"Developer ID Application: Test (TEAM123456)\""
                 echo "  2) 1111111111222222222233333333334444444444 \"Apple Distribution: Test (TEAM123456)\"" ;;
  cms) python3 -c "import plistlib,sys; sys.stdout.buffer.write(plistlib.dumps({\"UUID\":\"1234-PROFILE\",\"Name\":\"AF\"}))" ;;
  delete-keychain) rm -f "$2" ;;
esac
exit 0'
stub xcrun '
[[ -n "${AF_TEST_NO_METAL:-}" && "$*" == *metal* ]] && { echo "error: cannot execute tool metal due to missing Metal Toolchain" >&2; exit 72; }
case "$1 $2" in
  "notarytool submit") echo "{\"id\":\"sub-$RANDOM\",\"status\":\"Accepted\",\"message\":\"ok\"}" ;;
  "notarytool log") echo "{}" >"${@: -1}" ;;
esac
exit 0'
stub ditto '
if [[ "$1" == "-c" ]]; then src="${@: -2:1}"; dst="${@: -1}"; (cd "$(dirname "$src")" && zip -qry "$dst" "$(basename "$src")")
elif [[ "$1" == "-x" ]]; then unzip -qo "$3" -d "$4"
else cp -R "$1" "$2"; fi'
stub hdiutil '
cmd="$1"; shift
case "$cmd" in
  create) src=""; while [[ $# -gt 1 ]]; do [[ "$1" == "-srcfolder" ]] && src="$2"; shift; done
          mkdir -p "$1.d" && cp -R "$src/." "$1.d/" && echo img >"$1" ;;
  attach) img="$1"; m="$AF_TEST_VOLUMES/Abyssfire"; rm -rf "$m"; cp -R "$img.d" "$m"
          printf "/dev/disk4\tGUID_partition_scheme\t\n/dev/disk4s1\tApple_HFS\t%s\n" "$m"; echo "$img" >"$m/.af-image" ;;
  detach) m="$1"; img="$(cat "$m/.af-image")"; rm -f "$m/.af-image"; rm -rf "$img.d"; cp -R "$m" "$img.d"; rm -rf "$m" ;;
  convert) src="$1"; out=""; while [[ $# -gt 0 ]]; do [[ "$1" == "-o" ]] && out="$2"; shift; done
           (cd "$src.d" && zip -qry "$out" .) ;;
  verify) exit 0 ;;
esac'
cat >"$W/bin/PlistBuddy" <<'EOF'
#!/usr/bin/env python3
import plistlib, sys
cmd, path = sys.argv[2], sys.argv[3]
with open(path, "rb") as f:
    d = plistlib.load(f)
op, rest = cmd.split(" ", 1)
key = rest.split(" ")[0].lstrip(":")
if op == "Print":
    if key not in d: sys.exit(1)
    print(d[key]); sys.exit(0)
if op == "Delete":
    if key not in d: sys.exit(1)
    del d[key]
elif op == "Set":
    if key not in d: sys.exit(1)
    d[key] = rest.split(" ", 1)[1]
elif op == "Add":
    d[key] = rest.split(" ", 2)[2]
with open(path, "wb") as f:
    plistlib.dump(d, f)
EOF
chmod +x "$W/bin/PlistBuddy"

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
