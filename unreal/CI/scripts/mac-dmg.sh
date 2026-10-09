#!/usr/bin/env bash
# CI job `installer:mac-dmg` (self-hosted macOS runner) — Developer ID signing, notarisation, stapling and a styled DMG.
#
# Input:  $AF_OUT_DIR/package/mac/Abyssfire.app.zip      (package:mac)
#         $AF_OUT_DIR/installer-assets/                  (installer-assets: background, icons; regenerated if absent)
# Output: $AF_OUT_DIR/installers/mac/Abyssfire-<version>-macos-arm64.dmg (+ SHA256SUMS-mac.txt)
#
# Steps: unpack -> stamp Info.plist (version, icon) -> codesign inside-out (hardened runtime, timestamp) ->
#        notarise + staple the app -> hdiutil RW image (app, /Applications link, .background, volume icon) ->
#        Finder layout (.DS_Store via dmg_style.py, Finder AppleScript fallback) -> UDZO -> sign -> notarise +
#        staple the DMG -> Gatekeeper check.
#
# Secrets (masked + protected CI variables, environment scope mac-signing): AF_MAC_DEVID_P12_B64,
# AF_MAC_DEVID_P12_PASSWORD, AF_ASC_API_KEY_P8_B64, AF_ASC_API_KEY_ID, AF_ASC_API_ISSUER_ID; optional
# AF_MAC_SIGN_IDENTITY. Without them (development pipelines) the app is ad-hoc signed and not notarised; on release
# tags they are mandatory (AF_REQUIRE_SIGNING, common.sh). They are consumed at the start (p12 imported into the
# temporary keychain, .p8 decoded once) and then unset: no later process (pip, the DMG layout helper, Finder) sees them.
# The keychain stays locked except while codesign runs.
#   AF_SKIP_NOTARIZE=1     sign but do not notarise
#   AF_NOTARY_TIMEOUT      notarytool --wait limit per submission (default 1h; the job allows 3h for both)
#   AF_MAC_APP_ICON        auto (default: use the generated icon when unreal/Build/Mac has none) | always | never
#   AF_DMG_FINDER=1        allow the Finder AppleScript fallback (needs a logged-in GUI session)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/common.sh
source "$here/../lib/common.sh"
# shellcheck source=../lib/macos.sh
source "$here/../lib/macos.sh"
af_require_cmd ditto codesign hdiutil xcrun "$AF_PLISTBUDDY"
af_unset_secrets AF_MAC_DEVID_P12_B64 AF_MAC_DEVID_P12_PASSWORD AF_ASC_API_KEY_P8_B64 AF_ASC_API_KEY_ID AF_ASC_API_ISSUER_ID

work="$AF_OUT_DIR/work/dmg"
mnt=""
cleanup() {
  if [[ -n "$mnt" && -d "$mnt" ]]; then hdiutil detach "$mnt" -force >/dev/null 2>&1 || true; fi
  af_keychain_cleanup
  rm -rf "$work/keychain" "$work/secrets" 2>/dev/null || true
}
trap cleanup EXIT

af_load_version
src_zip="$AF_OUT_DIR/package/mac/Abyssfire.app.zip"
[[ -f "$src_zip" ]] || af_die "$src_zip missing (artifact of package:mac)"
assets="$AF_OUT_DIR/installer-assets"
if [[ ! -f "$assets/dmg-background.png" ]]; then
  af_warn "installer-assets artifact missing: generating the artwork locally"
  env -u AF_MAC_DEVID_P12_B64 -u AF_MAC_DEVID_P12_PASSWORD -u AF_ASC_API_KEY_P8_B64 -u AF_ASC_API_KEY_ID \
    -u AF_ASC_API_ISSUER_ID python3 -I "$AF_CI_DIR/installers/assets/make_installer_assets.py" --out "$assets" \
    --version "$AF_VERSION"
fi
dst="$AF_OUT_DIR/installers/mac"
dmg_name="Abyssfire-$AF_VERSION-macos-arm64.dmg"
rm -rf "$work"
mkdir -p "$work/app" "$dst" "$AF_LOG_DIR"

# ------------------------------------------------------------------------------------------------- unpack and stamp
af_section_start unpack "Unpack and stamp Abyssfire.app"
ditto -x -k "$src_zip" "$work/app"
app_src="$(find "$work/app" -maxdepth 1 -type d -name '*.app' | head -1)"
[[ -n "$app_src" ]] || af_die "no .app inside $src_zip"
app="$work/app/Abyssfire.app"
if [[ "$app_src" != "$app" ]]; then mv "$app_src" "$app"; fi
plist="$app/Contents/Info.plist"
pb() { "$AF_PLISTBUDDY" -c "$1" "$plist"; }
pb_set() { # pb_set KEY TYPE VALUE
  pb "Set :$1 $3" 2>/dev/null || pb "Add :$1 $2 $3"
}
pb_set CFBundleShortVersionString string "$AF_VERSION_NUMERIC"
pb_set CFBundleVersion string "$AF_BUILD_NUMBER"
pb_set CFBundleDisplayName string "Abyssfire"
pb_set LSApplicationCategoryType string "public.app-category.role-playing-games"
bundle_id="$(pb 'Print :CFBundleIdentifier' 2>/dev/null || true)"
af_log "bundle id: ${bundle_id:-?} (expected com.feuvan.abyssfire, DECISIONS P12)"

icon_mode="${AF_MAC_APP_ICON:-auto}"
if [[ "$icon_mode" == "auto" ]]; then
  if find "$AF_UNREAL_DIR/Build/Mac" \( -name '*.icns' -o -name 'AppIcon.appiconset' \) 2>/dev/null | grep -q .; then
    icon_mode=never
  else
    icon_mode=always
  fi
fi
if [[ "$icon_mode" == "always" && -f "$assets/Abyssfire.icns" ]]; then
  cp -f "$assets/Abyssfire.icns" "$app/Contents/Resources/Abyssfire.icns"
  pb_set CFBundleIconFile string "Abyssfire"
  pb "Delete :CFBundleIconName" 2>/dev/null || true # an asset-catalog icon would win over CFBundleIconFile
  af_log "app icon: generated Abyssfire.icns ($(cat "$assets/source.txt" 2>/dev/null || echo '?'))"
fi
af_section_end unpack

# -------------------------------------------------------------------------------------------------------- signing
signed=0
identity="-"
if af_have AF_MAC_DEVID_P12_B64; then
  af_require_vars AF_MAC_DEVID_P12_PASSWORD
  af_keychain_create "$work/keychain"
  af_keychain_import_p12 AF_MAC_DEVID_P12_B64 AF_MAC_DEVID_P12_PASSWORD
  if af_have AF_MAC_SIGN_IDENTITY; then
    identity="$AF_MAC_SIGN_IDENTITY"
  else
    identity="$(af_keychain_identity "Developer ID Application")" ||
      af_die "AF_MAC_DEVID_P12_B64 holds no 'Developer ID Application' identity"
  fi
  signed=1
elif af_signing_required; then
  af_die "release builds need AF_MAC_DEVID_P12_B64 / AF_MAC_DEVID_P12_PASSWORD (or AF_REQUIRE_SIGNING=0)"
else
  af_warn "no Developer ID certificate: ad-hoc signature, no notarisation (Gatekeeper will block the download)"
fi

notarize=0
if [[ $signed -eq 1 ]] && ! af_is_true "${AF_SKIP_NOTARIZE:-0}"; then
  if af_have AF_ASC_API_KEY_P8_B64; then
    notarize=1
    af_notary_prepare "$work/secrets"
  elif af_signing_required; then
    af_die "release builds need the App Store Connect API key (AF_ASC_API_KEY_P8_B64, _ID, _ISSUER_ID) to notarise"
  else
    af_warn "no App Store Connect API key: signed but not notarised"
  fi
fi
# everything secret is now in the keychain / $work/secrets: nothing started from here on inherits the variables
af_unset_secrets

af_section_start sign "codesign ($(if [[ $signed -eq 1 ]]; then echo Developer ID; else echo ad-hoc; fi))"
af_codesign_bundle "$app" "$identity" "$AF_CI_DIR/installers/mac/entitlements.plist"
af_keychain_lock
af_section_end sign

if [[ $notarize -eq 1 ]]; then
  af_section_start notarize_app "Notarise + staple the app"
  ditto -c -k --sequesterRsrc --keepParent "$app" "$work/Abyssfire-notarize.zip"
  af_notarize "$work/Abyssfire-notarize.zip"
  xcrun stapler staple "$app"
  rm -f "$work/Abyssfire-notarize.zip"
  af_section_end notarize_app
fi

# ------------------------------------------------------------------------------------------------------------- DMG
af_section_start dmg "Build the DMG"
stage="$work/stage"
mkdir -p "$stage/.background"
ditto "$app" "$stage/Abyssfire.app"
ln -s /Applications "$stage/Applications"
bg="$stage/.background/background.tiff"
if [[ -f "$assets/dmg-background@2x.png" ]] && command -v tiffutil >/dev/null 2>&1; then
  # one multi-resolution TIFF: Finder picks the @2x page on Retina displays
  tiffutil -cathidpicheck "$assets/dmg-background.png" "$assets/dmg-background@2x.png" -out "$bg" >/dev/null
else
  sips -s format tiff "$assets/dmg-background.png" --out "$bg" >/dev/null
fi
if [[ -f "$assets/Abyssfire.icns" ]]; then cp -f "$assets/Abyssfire.icns" "$stage/.VolumeIcon.icns"; fi

size_mb=$(($(du -sm "$stage" | awk '{print $1}') * 12 / 10 + 64))
rw="$work/rw.dmg"
hdiutil create -srcfolder "$stage" -volname "Abyssfire" -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW \
  -size "${size_mb}m" "$rw" >/dev/null
attach_rw() { # prints the mount point (/Volumes/Abyssfire, or "Abyssfire 1" if that name is busy)
  hdiutil attach "$rw" -readwrite -noverify -noautoopen "$@" | awk -F '\t' '/\/Volumes\// {print $NF}' | tail -1
}
mnt="$(attach_rw -nobrowse -owners off)"
[[ -n "$mnt" && -d "$mnt" ]] || af_die "could not mount $rw"

styled=0
# A fresh venv per job (never a shared one other jobs could have modified), hash-pinned wheels only; the helper runs
# isolated (-I) with a minimal environment.
venv="$work/dmg-venv"
if python3 -m venv "$venv" >/dev/null 2>&1; then
  "$venv/bin/pip" install --quiet --disable-pip-version-check --no-input --require-hashes --no-deps \
    --only-binary :all: -r "$AF_CI_DIR/installers/mac/dmg-requirements.txt" >"$AF_LOG_DIR/dmg-pip.log" 2>&1 ||
    {
      af_warn "pip install of dmg-requirements.txt failed (see logs/dmg-pip.log)"
      rm -rf "$venv"
    }
fi
if [[ -x "$venv/bin/python" ]] &&
  env -i HOME="$HOME" PATH="/usr/bin:/bin:/usr/sbin:/sbin" TMPDIR="${TMPDIR:-/tmp}" LANG=en_US.UTF-8 \
    "$venv/bin/python" -I "$AF_CI_DIR/installers/mac/dmg_style.py" --volume "$mnt" \
    --background .background/background.tiff; then
  styled=1
elif af_is_true "${AF_DMG_FINDER:-0}"; then
  # Finder only sees browsable volumes: remount without -nobrowse and let Finder write .DS_Store.
  hdiutil detach "$mnt" >/dev/null
  mnt="$(attach_rw)"
  vol="$(basename "$mnt")"
  if osascript <<EOF; then styled=1; fi
tell application "Finder"
  tell disk "$vol"
    open
    set current view of container window to icon view
    set toolbar visible of container window to false
    set statusbar visible of container window to false
    set the bounds of container window to {200, 120, 860, 540}
    set opts to the icon view options of container window
    set arrangement of opts to not arranged
    set icon size of opts to 128
    set background picture of opts to file ".background:background.tiff"
    set position of item "Abyssfire.app" of container window to {170, 205}
    set position of item "Applications" of container window to {490, 205}
    close
    open
    update without registering applications
    delay 2
    close
  end tell
end tell
EOF
fi
if [[ $styled -eq 0 ]]; then
  af_warn "DMG window layout not applied (dmg_style.py failed or its pinned wheels did not install, see above /" \
    "logs/dmg-pip.log; no AF_DMG_FINDER fallback)"
fi
if [[ -f "$mnt/.VolumeIcon.icns" ]] && command -v SetFile >/dev/null 2>&1; then SetFile -a C "$mnt" || true; fi
chmod -Rf go-w "$mnt" 2>/dev/null || true
sync
for i in 1 2 3 4 5; do
  if hdiutil detach "$mnt" >/dev/null 2>&1; then break; fi
  sleep $((i * 2))
  if [[ $i -eq 5 ]]; then hdiutil detach "$mnt" -force >/dev/null; fi
done
mnt=""
rm -f "$dst/$dmg_name"
hdiutil convert "$rw" -format UDZO -imagekey zlib-level=9 -o "$dst/$dmg_name" >/dev/null
rm -f "$rw"
af_section_end dmg

if [[ $signed -eq 1 ]]; then
  af_keychain_unlock
  codesign --force --timestamp --keychain "$AF_KEYCHAIN" --sign "$identity" "$dst/$dmg_name"
  af_keychain_lock
  codesign --verify --verbose=2 "$dst/$dmg_name"
fi
if [[ $notarize -eq 1 ]]; then
  af_section_start notarize_dmg "Notarise + staple the DMG"
  af_notarize "$dst/$dmg_name"
  xcrun stapler staple "$dst/$dmg_name"
  xcrun stapler validate "$dst/$dmg_name"
  spctl --assess --type open --context context:primary-signature --verbose=2 "$dst/$dmg_name"
  af_section_end notarize_dmg
fi
hdiutil verify "$dst/$dmg_name" >/dev/null

rm -f "$dst/SHA256SUMS-mac.txt"
(cd "$dst" && af_checksum_into SHA256SUMS-mac.txt "$dmg_name")
af_log "DMG: $dst/$dmg_name ($(du -h "$dst/$dmg_name" | awk '{print $1}'); signed=$signed notarised=$notarize styled=$styled)"
