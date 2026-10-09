#!/usr/bin/env bash
# CI job `installer:windows` (GitLab SaaS Linux runner, no engine) — NSIS setup.exe + MSI + portable zip from the
# staged Win64 build of `package:win64`.
#
#   windows-installers.sh [STAGED_DIR]
#
# Input:  STAGED_DIR (default $AF_OUT_DIR/package/win64/Windows: Abyssfire.exe, Abyssfire/Binaries/Win64/...,
#         Abyssfire/Content/Paks/*.pak|.utoc|.ucas, Engine/...)
#         $AF_OUT_DIR/installer-assets (installer-assets job; regenerated with Pillow / stdlib when absent)
# Output: $AF_OUT_DIR/installers/windows/
#           Abyssfire-<version>-win64-setup.exe     NSIS (MUI2, zh-CN / en, Start menu + desktop, uninstaller)
#           Abyssfire-<version>-win64.msi           wixl (MajorUpgrade, Start menu + desktop, ARP entry)
#           Abyssfire-<version>-win64-portable.zip  the staged build as is
#           SHA256SUMS-windows.txt, SIGNING-windows.txt (signed=0|1, read by release.sh)
#
# Optional Authenticode signing (osslsigncode) when AF_WIN_CODESIGN_PFX_B64 + AF_WIN_CODESIGN_PFX_PASSWORD exist:
# the game executables, the NSIS uninstaller and installer (!uninstfinalize / !finalize) and the MSI. Mandatory with
# AF_WIN_REQUIRE_CODESIGN=1 or AF_REQUIRE_SIGNING=1 (on tags it stays optional: exportable .pfx code-signing keys no
# longer exist for new certificates, CI.md 4.2).
# MSI ProductVersion = AF_MSI_VERSION (X.Y.(Z*100+S), so a release candidate never replaces the final release).
# setup.exe and the MSI refuse to install over each other (shared install folder): the NSIS script gets the MSI's
# UpgradeCode from abyssfire.wxs.
# Tools: makensis (nsis >= 3.08), wixl + wixl-heat + msiinfo (msitools), osslsigncode, python3, zip, 7z (checks).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/common.sh
source "$here/../lib/common.sh"
af_require_cmd makensis wixl wixl-heat msiinfo python3

af_load_version
staged_in="${1:-${AF_WIN_STAGED_DIR:-$AF_OUT_DIR/package/win64/Windows}}"
[[ -d "$staged_in" ]] || af_die "staged Win64 build not found at $staged_in (artifact of package:win64)"
[[ -f "$staged_in/Abyssfire.exe" ]] || af_die "$staged_in/Abyssfire.exe missing: not a staged UE Windows build"
compgen -G "$staged_in/Abyssfire/Content/Paks/*.pak" >/dev/null ||
  af_die "$staged_in/Abyssfire/Content/Paks/*.pak missing: was the build cooked and pak'ed?"

work="$AF_OUT_DIR/work/windows-installers"
dst="$AF_OUT_DIR/installers/windows"
assets="${AF_INSTALLER_ASSETS:-$AF_OUT_DIR/installer-assets}"
rm -rf "$work"
mkdir -p "$work" "$dst"
cleanup() { rm -rf "$work/secrets"; }
trap cleanup EXIT

if [[ ! -f "$assets/Abyssfire.ico" || ! -f "$assets/nsis-welcome.bmp" ]]; then
  af_warn "installer-assets missing: generating them"
  python3 "$AF_CI_DIR/installers/assets/make_installer_assets.py" --out "$assets" --version "$AF_VERSION"
fi

# Work on a copy: signing rewrites executables, and the input artifact stays pristine.
af_section_start stage "Copy the staged build"
staged="$work/staged"
cp -a "$staged_in" "$staged"
af_section_end stage

# ------------------------------------------------------------------------------------------------------ signing
signing=0
if af_have AF_WIN_CODESIGN_PFX_B64; then
  af_require_cmd osslsigncode
  af_require_vars AF_WIN_CODESIGN_PFX_PASSWORD
  mkdir -p "$work/secrets"
  chmod 700 "$work/secrets"
  af_secret_to_file AF_WIN_CODESIGN_PFX_B64 "$work/secrets/codesign.pfx"
  (
    umask 077
    af_secret_value AF_WIN_CODESIGN_PFX_PASSWORD >"$work/secrets/pass"
  )
  export AF_WIN_SIGN_PFX="$work/secrets/codesign.pfx" AF_WIN_SIGN_PASSFILE="$work/secrets/pass"
  signing=1
  af_section_start sign_game "Authenticode: game executables"
  while IFS= read -r -d '' exe; do "$here/sign-pe.sh" "$exe"; done < <(
    find "$staged" -maxdepth 1 -name 'Abyssfire*.exe' -print0
    find "$staged/Abyssfire/Binaries" -name 'Abyssfire*.exe' -print0 2>/dev/null
  )
  af_section_end sign_game
elif af_is_true "${AF_WIN_REQUIRE_CODESIGN:-0}" || [[ "${AF_REQUIRE_SIGNING:-auto}" == "1" ]]; then
  af_die "Windows signing is required (AF_WIN_REQUIRE_CODESIGN=1 / AF_REQUIRE_SIGNING=1) but AF_WIN_CODESIGN_PFX_B64 is not set"
else
  af_warn "AF_WIN_CODESIGN_PFX_B64 not set: installers are unsigned (SmartScreen will warn)"
fi

af_section_start lists "File lists"
python3 "$AF_CI_DIR/installers/windows/gen_file_lists.py" "$staged" "$work/lists"
size_kb="$(cat "$work/lists/size-kb.txt")"
af_section_end lists

setup="$dst/Abyssfire-$AF_VERSION-win64-setup.exe"
msi="$dst/Abyssfire-$AF_VERSION-win64.msi"
zip="$dst/Abyssfire-$AF_VERSION-win64-portable.zip"
rm -f "$setup" "$msi" "$zip"

wxs="$AF_CI_DIR/installers/windows/abyssfire.wxs"
upgrade_code="$(sed -n 's/.*UpgradeCode="\({[0-9A-Fa-f-]*}\)".*/\1/p' "$wxs" | head -1)"
[[ -n "$upgrade_code" ]] || af_die "no UpgradeCode in $wxs"

# ---------------------------------------------------------------------------------------------------------- NSIS
af_section_start nsis "NSIS installer ($(makensis -VERSION))"
nsis_args=(-V2 -INPUTCHARSET UTF8
  "-DVERSION=$AF_VERSION" "-DVERSION_QUAD=$AF_VERSION_QUAD" "-DMSI_UPGRADE_CODE=$upgrade_code"
  "-DSOURCE_DIR=$staged" "-DLISTS_DIR=$work/lists" "-DASSETS_DIR=$assets"
  "-DOUTFILE=$setup" "-DSIZE_KB=$size_kb")
if [[ $signing -eq 1 ]]; then nsis_args+=("-DSIGN_CMD=$here/sign-pe.sh"); fi
makensis "${nsis_args[@]}" "$AF_CI_DIR/installers/windows/abyssfire.nsi"
af_section_end nsis

# ----------------------------------------------------------------------------------------------------------- MSI
af_section_start msi "MSI ($(wixl --version 2>/dev/null || echo wixl))"
(cd "$staged" && find . -type f | LC_ALL=C sort | sed "s|^\./|$staged/|") |
  wixl-heat -p "$staged/" --directory-ref INSTALLDIR --component-group CG.GameFiles --var var.SourceDir --win64 \
    >"$work/game-files.wxs"
wixl -a x64 -D "Version=$AF_MSI_VERSION" -D "DisplayVersion=$AF_VERSION" -D Win64=yes -D "SourceDir=$staged" \
  -D "AssetsDir=$assets" -o "$msi" "$wxs" "$work/game-files.wxs"
if [[ $signing -eq 1 ]]; then "$here/sign-pe.sh" "$msi"; fi
af_section_end msi

# ---------------------------------------------------------------------------------------------------- portable zip
af_section_start zip "Portable zip"
if command -v zip >/dev/null 2>&1; then
  (cd "$staged" && zip -q -r -9 -X "$zip" .)
else
  python3 - "$staged" "$zip" <<'PY'
import os, sys, zipfile
src, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, dirs, files in os.walk(src):
        dirs.sort()
        for f in sorted(files):
            p = os.path.join(root, f)
            z.write(p, os.path.relpath(p, src))
PY
fi
af_section_end zip

# ---------------------------------------------------------------------------------------------------------- checks
af_section_start verify "Verify the installers"
nfiles="$(wc -l <"$work/lists/file-list.txt" | tr -d ' ')"
msi_table() { msiinfo export "$msi" "$1" | tr -d '\r'; } # IDT export: CRLF lines, 3 header rows
msi_files="$(msi_table File | tail -n +4 | wc -l | tr -d ' ')"
[[ "$msi_files" == "$nfiles" ]] || af_die "MSI carries $msi_files files, the staged build has $nfiles"
msi_table Property | grep -q "^ProductVersion[[:space:]]$AF_MSI_VERSION\$" ||
  af_die "MSI ProductVersion is not $AF_MSI_VERSION"
msi_table LaunchCondition | grep -q 'NSISUNINSTALL' || af_die "MSI does not refuse an existing setup.exe install"
msi_table Shortcut | grep -q 'Abyssfire.exe' || af_die "MSI has no game shortcut"
msi_table Upgrade | grep -q 'WIX_UPGRADE_DETECTED' || af_die "MSI has no MajorUpgrade rows"
msiinfo suminfo "$msi" | grep -q 'Template: x64' || af_die "MSI is not an x64 package"
if command -v 7z >/dev/null 2>&1; then
  listing="$(7z l "$setup")"
  printf '%s\n' "$listing" | grep -q 'Abyssfire.exe' || af_die "setup.exe does not contain Abyssfire.exe"
  printf '%s\n' "$listing" | grep -q 'uninstall.exe' || af_die "setup.exe does not contain the uninstaller"
  printf '%s\n' "$listing" | grep -Eq '\.pak$' || af_die "setup.exe does not contain the .pak"
else
  af_warn "7z not installed: setup.exe contents not listed"
fi
if [[ $signing -eq 1 ]]; then
  verify_args=()
  # AF_WIN_VERIFY_CAFILE: trust anchor for test certificates (default: the system CA bundle)
  if [[ -n "${AF_WIN_VERIFY_CAFILE:-}" ]]; then verify_args=(-CAfile "$AF_WIN_VERIFY_CAFILE"); fi
  for f in "$setup" "$msi" "$staged/Abyssfire.exe"; do
    osslsigncode verify ${verify_args[@]+"${verify_args[@]}"} -in "$f" >"$work/verify.txt" 2>&1 ||
      grep -q 'Signature verification: ok' "$work/verify.txt" ||
      {
        cat "$work/verify.txt" >&2
        af_die "signature of $(basename "$f") does not verify"
      }
  done
fi
af_section_end verify

rm -f "$dst/SHA256SUMS-windows.txt"
(cd "$dst" && af_checksum_into SHA256SUMS-windows.txt "$(basename "$setup")" "$(basename "$msi")" "$(basename "$zip")")
printf 'signed=%s\n' "$signing" >"$dst/SIGNING-windows.txt"
af_log "Windows installers (signed=$signing):"
ls -l "$dst" >&2
