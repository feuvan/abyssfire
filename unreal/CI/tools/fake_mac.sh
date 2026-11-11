# shellcheck shell=bash
# shellcheck disable=SC2016 # the stub bodies below are literal scripts, expanded when the stubs run
# The fake Unreal Engine and the stub Apple tools shared by the self-tests of the macOS scripts
# (test_mac_scripts.sh, test_release_mac.sh). Sourced, never executed, with W set to the test's work directory:
#
#   $W/UE    a 5.8.3 engine: Build.sh (expects AbyssfireEditor Mac Development), UnrealEditor-Cmd (writes L_Main and
#            the content report, expects the CI DDC variable), RunUAT.sh (BuildCookRun for Mac / Android / IOS:
#            refuses signing secrets in its environment, wants the ProjectVersion overlay, writes the pak list)
#   $W/bin   uname (Darwin), codesign, spctl, sips, tiffutil, SetFile, lipo, file, security, xcrun, ditto, hdiutil,
#            PlistBuddy; every call is appended to $AF_TEST_CALLS
#
# The caller exports PATH="$W/bin:$PATH", HOME, AF_TEST_CALLS, AF_TEST_VOLUMES and AF_PLISTBUDDY="$W/bin/PlistBuddy".
# Switches read by the fakes: AF_TEST_UAT_FAIL, AF_TEST_DROP, AF_TEST_NO_METAL, AF_TEST_TWO_IDS (`security
# find-identity` lists a second Developer ID certificate of the same name).
: "${W:?fake_mac.sh: set W to the work directory first}"
mkdir -p "$W/bin"

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
                 echo "  2) 1111111111222222222233333333334444444444 \"Apple Distribution: Test (TEAM123456)\""
                 # after a renewal: a second valid certificate of the same name
                 [[ -z "${AF_TEST_TWO_IDS:-}" ]] || echo "  3) FEDCBA9876543210FEDCBA9876543210FEDCBA98 \"Developer ID Application: Test (TEAM123456)\""
                 echo "     2 valid identities found" ;;
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
