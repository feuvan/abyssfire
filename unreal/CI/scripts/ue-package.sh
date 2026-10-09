#!/usr/bin/env bash
# CI jobs `package:mac`, `package:android`, `package:ios` (self-hosted macOS runner) — RunUAT BuildCookRun.
#
#   ue-package.sh Mac       Apple Silicon, Shipping, cook/stage/pak/iostore/archive
#                           -> $AF_OUT_DIR/package/mac/Abyssfire.app.zip (unsigned; installer:mac-dmg signs it)
#   ue-package.sh Android   ASTC, arm64, -package: .aab + universal .apk (bundletool), release (non-debuggable) Gradle
#                           build signed with a THROW-AWAY per-job key -> $AF_OUT_DIR/package/android/Abyssfire.apk,
#                           Abyssfire.aab. The upload key never reaches the engine or this runner: installer:android
#                           re-signs both on an ephemeral SaaS runner.
#   ue-package.sh IOS       -distribution, signed with the distribution certificate + provisioning profile
#                           -> $AF_OUT_DIR/package/ios/Abyssfire-<version>-ios.ipa (App Store Connect upload only)
#
# After BuildCookRun every runtime Data/ + Fonts/ file must be in the pak (af_check_runtime_dependencies).
# Expects unreal/Content from the `ue:content` job (artifact). Variables: unreal/Docs/CI.md.
#   AF_ANDROID_DISTRIBUTION  auto (default: distribution build on tags and protected refs in CI) | 1 | 0 (debug build)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/common.sh
source "$here/../lib/common.sh"
# shellcheck source=../lib/ue.sh
source "$here/../lib/ue.sh"
# shellcheck source=../lib/macos.sh
source "$here/../lib/macos.sh"

platform="${1:-}"
case "$platform" in
  Mac | Android | IOS) ;;
  *)
    echo "usage: $0 Mac|Android|IOS" >&2
    exit 2
    ;;
esac

on_exit() {
  af_ue_collect_logs
  af_keychain_cleanup
  # UEDeployAndroid copies Build/Android (keystore included) under Intermediate/Android and writes the signing
  # passwords into its generated gradle.properties: never leave them on the persistent runner. UBT objects live in
  # Intermediate/Build, so only Gradle's incremental state is lost.
  if [[ "$platform" == "Android" ]]; then rm -rf "$AF_UNREAL_DIR/Intermediate/Android"; fi
  af_cleanup_overlays
}
trap on_exit EXIT

# The engine (and its crash dumps) must not inherit signing secrets it does not use; iOS keeps its own until they
# are in the temporary keychain.
if [[ "$platform" == "IOS" ]]; then
  af_unset_secrets AF_IOS_DIST_P12_B64 AF_IOS_DIST_P12_PASSWORD AF_IOS_PROVISION_PROFILE_B64
else
  af_unset_secrets
fi
af_ue_init
af_load_version
if [[ "$platform" != "Android" && "$AF_UE_HOST" != "Mac" ]]; then af_die "$platform packages only on a macOS runner"; fi
[[ -f "$AF_UNREAL_DIR/Content/Abyssfire/Maps/L_Main.umap" ]] ||
  af_die "unreal/Content is missing L_Main: this job needs the ue:content artifact"

work="$AF_OUT_DIR/work/$platform"
archive="$AF_OUT_DIR/work/$platform-archive"
rm -rf "$work" "$archive"
mkdir -p "$work" "$archive"
af_track_tmp "$work"
stamp="$work/.job-start"
touch "$stamp"

# Display version for every platform (Project Settings > Description).
af_overlay_ini "$AF_UNREAL_DIR/Config/DefaultGame.ini" "[/Script/EngineSettings.GeneralProjectSettings]" \
  "ProjectVersion=$AF_VERSION"

newest() { # newest file matching a name pattern under the given directories, written by this job
  local pattern="$1" best="" f
  shift
  while IFS= read -r -d '' f; do
    if [[ -z "$best" || "$f" -nt "$best" ]]; then best="$f"; fi
  done < <(find "$@" -type f -name "$pattern" -newer "$stamp" -print0 2>/dev/null)
  printf '%s' "$best"
}

# ---------------------------------------------------------------------------------------------------------------- Mac
package_mac() {
  local dst="$AF_OUT_DIR/package/mac" app
  af_section_start bcr "BuildCookRun Mac ${AF_UE_CLIENT_CONFIG:-Shipping} (${AF_MAC_ARCH:-arm64})"
  af_buildcookrun Mac "$archive" -specifiedarchitecture="${AF_MAC_ARCH:-arm64}" -NoCodeSign
  af_section_end bcr
  af_check_runtime_dependencies "$archive" "$AF_UNREAL_DIR/Saved/StagedBuilds"
  app="$(find "$archive" -maxdepth 3 -type d -name '*.app' | head -1)"
  [[ -n "$app" ]] || af_die "no .app in $archive"
  af_log "packaged $app"
  local exe
  for exe in "$app/Contents/MacOS/"*; do af_log "$(basename "$exe"): $(lipo -archs "$exe" 2>/dev/null || echo '?')"; done
  mkdir -p "$dst"
  rm -f "$dst/Abyssfire.app.zip"
  # ditto keeps symlinks, permissions and extended attributes inside the bundle
  ditto -c -k --sequesterRsrc --keepParent "$app" "$dst/Abyssfire.app.zip"
  printf 'source: %s\nversion: %s\n' "$(basename "$app")" "$AF_VERSION" >"$dst/package-info.txt"
}

# ------------------------------------------------------------------------------------------------------------ Android
android_distribution() {
  case "${AF_ANDROID_DISTRIBUTION:-auto}" in
    auto) af_signing_required || [[ "${CI_COMMIT_REF_PROTECTED:-}" == "true" ]] ;;
    *) af_is_true "$AF_ANDROID_DISTRIBUTION" ;;
  esac
}

package_android() {
  local dst="$AF_OUT_DIR/package/android" ini="$AF_UNREAL_DIR/Config/Android/AndroidEngine.ini"
  local section="[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]" extra=() apk aab dist=no
  for v in ANDROID_HOME NDKROOT JAVA_HOME; do
    af_have "$v" || af_warn "$v is not set on this runner (UE's Turnkey may not find the Android toolchain)"
  done
  # Bundle mode builds only the .aab through Gradle; the .apk comes from `bundletool build-apks --mode=universal`,
  # which UE runs only with bEnableUniversalAPK=True.
  af_overlay_ini "$ini" "$section" "StoreVersion=$AF_ANDROID_VERSION_CODE" "VersionDisplayName=$AF_VERSION" \
    "bEnableBundle=True" "bEnableUniversalAPK=True"
  # launcher icon: the generated set (installer-assets) when the project commits none under Build/Android/res
  if [[ ! -d "$AF_UNREAL_DIR/Build/Android/res" && -d "$AF_OUT_DIR/installer-assets/ue/Android/res" ]]; then
    mkdir -p "$AF_UNREAL_DIR/Build/Android"
    cp -R "$AF_OUT_DIR/installer-assets/ue/Android/res" "$AF_UNREAL_DIR/Build/Android/res"
    af_track_tmp "$AF_UNREAL_DIR/Build/Android/res"
    af_log "Android: generated launcher icons ($(cat "$AF_OUT_DIR/installer-assets/source.txt" 2>/dev/null || echo '?'))"
  fi

  if android_distribution; then
    # -distribution = release Gradle build (not debuggable; Play rejects debuggable bundles). It needs a keystore:
    # a throw-away one generated for this job, never the upload key (installer:android re-signs with that).
    af_require_cmd keytool
    local ks_dir="$AF_UNREAL_DIR/Build/Android" ks_name="abyssfire-ci-throwaway.keystore" pass
    pass="$(af_random_password)"
    [[ -n "$pass" ]] || af_die "could not generate a keystore password"
    mkdir -p "$ks_dir"
    rm -f "$ks_dir/$ks_name"
    af_track_tmp "$ks_dir/$ks_name"
    AF__CI_PASS="$pass" keytool -genkeypair -keystore "$ks_dir/$ks_name" -storetype PKCS12 \
      -storepass:env AF__CI_PASS -keypass:env AF__CI_PASS -alias ci -keyalg RSA -keysize 2048 -validity 2 \
      -dname "CN=Abyssfire CI throw-away" >/dev/null 2>&1 || af_die "keytool could not create the throw-away keystore"
    af_overlay_ini "$ini" "$section" "KeyStore=$ks_name" "KeyAlias=ci" "KeyStorePassword=$pass" "KeyPassword=$pass"
    extra+=(-distribution)
    dist=yes
    af_log "Android: distribution build, signed with a throw-away key (installer:android applies the upload key)"
  else
    af_warn "Android: development build (debug key, debuggable); AF_ANDROID_DISTRIBUTION=1 for a release build"
  fi

  af_section_start bcr "BuildCookRun Android ${AF_ANDROID_COOK_FLAVOR:-ASTC} ${AF_UE_CLIENT_CONFIG:-Shipping}"
  af_buildcookrun Android "$archive" -cookflavor="${AF_ANDROID_COOK_FLAVOR:-ASTC}" ${extra[@]+"${extra[@]}"}
  af_section_end bcr
  af_check_runtime_dependencies "$archive" "$AF_UNREAL_DIR/Saved/StagedBuilds"

  apk="$(newest '*.apk' "$archive" "$AF_UNREAL_DIR/Binaries/Android")"
  aab="$(newest '*.aab' "$archive" "$AF_UNREAL_DIR/Binaries/Android" "$AF_UNREAL_DIR/Intermediate/Android")"
  [[ -n "$apk" ]] || af_die "no .apk produced (archive $archive): the universal APK needs bEnableUniversalAPK=True (overlaid by this script; see logs/<job>/uat for bundletool errors)"
  [[ -n "$aab" ]] || af_die "no .aab produced: check bEnableBundle=True (DefaultEngine.ini, overlaid by this script) and the Gradle log"
  mkdir -p "$dst"
  cp -f "$apk" "$dst/Abyssfire.apk"
  cp -f "$aab" "$dst/Abyssfire.aab"
  printf 'apk: %s\naab: %s\nversion: %s\nversionCode: %s\ndistribution: %s\nsigned-with: %s\n' \
    "$(basename "$apk")" "$(basename "$aab")" "$AF_VERSION" "$AF_ANDROID_VERSION_CODE" "$dist" \
    "$(if [[ $dist == yes ]]; then echo 'throw-away CI key (re-signed by installer:android)'; else echo 'UE debug key'; fi)" \
    >"$dst/package-info.txt"
  af_log "Android: $(basename "$apk"), $(basename "$aab")"
}

# ---------------------------------------------------------------------------------------------------------------- iOS
# install_profile FILE -> sets AF_PROFILE_UUID. Not called as $(...): the clean-up registration (af_track_tmp) must
# happen in this shell, not in a command-substitution subshell.
install_profile() {
  local file="$1" plist dir
  plist="$work/profile.plist"
  security cms -D -i "$file" >"$plist" || af_die "AF_IOS_PROVISION_PROFILE_B64 is not a provisioning profile"
  AF_PROFILE_UUID="$("$AF_PLISTBUDDY" -c 'Print :UUID' "$plist")"
  [[ -n "$AF_PROFILE_UUID" ]] || af_die "the provisioning profile has no UUID"
  for dir in "$HOME/Library/MobileDevice/Provisioning Profiles" \
    "$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles"; do
    mkdir -p "$dir"
    cp -f "$file" "$dir/$AF_PROFILE_UUID.mobileprovision"
    af_track_tmp "$dir/$AF_PROFILE_UUID.mobileprovision"
  done
}

package_ios() {
  local dst="$AF_OUT_DIR/package/ios" ini="$AF_UNREAL_DIR/Config/IOS/IOSEngine.ini" identity uuid ipa
  af_require_vars AF_APPLE_TEAM_ID AF_IOS_DIST_P12_B64 AF_IOS_DIST_P12_PASSWORD AF_IOS_PROVISION_PROFILE_B64
  # The iOS platform is an on-demand Xcode download since Xcode 15.
  xcrun --sdk iphoneos --show-sdk-path >/dev/null 2>&1 ||
    af_die "iOS platform not installed: xcodebuild -downloadPlatform iOS (unreal/Docs/CI.md 3.1)"
  # xcodebuild signs at the end of a build that can take hours: auto-lock only after the job timeout (4h)
  af_keychain_create "$work/keychain" 15000
  af_keychain_import_p12 AF_IOS_DIST_P12_B64 AF_IOS_DIST_P12_PASSWORD
  identity="$(af_keychain_identity "Apple Distribution" || af_keychain_identity "iPhone Distribution" || true)"
  [[ -n "$identity" ]] || af_die "AF_IOS_DIST_P12_B64 holds no Apple Distribution identity"
  local name
  name="$(security find-identity -v -p codesigning "$AF_KEYCHAIN" | grep -F "$identity" | sed -n 's/.*"\(.*\)".*/\1/p' | head -1)"
  af_secret_to_file AF_IOS_PROVISION_PROFILE_B64 "$work/profile.mobileprovision"
  install_profile "$work/profile.mobileprovision"
  uuid="$AF_PROFILE_UUID"
  af_unset_secrets # the certificate is in the keychain, the profile installed: the engine needs no secret variable
  af_log "iOS: identity '$name', provisioning profile $uuid"
  local team
  team="$(af_secret_value AF_APPLE_TEAM_ID)"
  # [Verify] key names on 5.8.3 (ue58-platform.md §5.1: Modern-Xcode signing keys); both the modern and the legacy
  # keys are written so either code path finds the manual signing setup.
  af_overlay_ini "$ini" "[/Script/MacTargetPlatform.XcodeProjectSettings]" \
    "bUseAutomaticCodeSigning=False" \
    "CodeSigningTeam=$team" \
    "IOSSigningIdentity=$name" \
    "IOSProvisioningProfile=(FilePath=\"$work/profile.mobileprovision\")"
  af_overlay_ini "$ini" "[/Script/IOSRuntimeSettings.IOSRuntimeSettings]" \
    "bAutomaticSigning=False" \
    "IOSTeamID=$team" \
    "MobileProvision=$uuid.mobileprovision" \
    "SigningCertificate=$name" \
    "VersionInfo=$AF_VERSION_NUMERIC"

  af_section_start bcr "BuildCookRun IOS ${AF_UE_CLIENT_CONFIG:-Shipping} (-distribution)"
  af_buildcookrun IOS "$archive" -distribution
  af_keychain_lock
  af_section_end bcr
  af_check_runtime_dependencies "$archive" "$AF_UNREAL_DIR/Saved/StagedBuilds"
  ipa="$(newest '*.ipa' "$archive" "$AF_UNREAL_DIR/Binaries/IOS")"
  [[ -n "$ipa" ]] || af_die "no .ipa produced"
  mkdir -p "$dst"
  cp -f "$ipa" "$dst/Abyssfire-$AF_VERSION-ios.ipa"
  # verify the signature of the app inside the ipa
  rm -rf "$work/ipa" && mkdir -p "$work/ipa"
  ditto -x -k "$dst/Abyssfire-$AF_VERSION-ios.ipa" "$work/ipa"
  codesign --verify --deep --strict --verbose=2 "$work"/ipa/Payload/*.app
  (cd "$dst" && af_checksum_into SHA256SUMS-ios.txt "Abyssfire-$AF_VERSION-ios.ipa")
  af_log "iOS: $dst/Abyssfire-$AF_VERSION-ios.ipa"
}

case "$platform" in
  Mac) package_mac ;;
  Android) package_android ;;
  IOS) package_ios ;;
esac
