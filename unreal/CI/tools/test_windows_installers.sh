#!/usr/bin/env bash
# Local / CI self-test of the Windows installer pipeline without Unreal: builds a fake staged Win64 build (real PE
# executables made with makensis, dummy .pak / .utoc / .ucas, the UE prerequisites path), runs
# scripts/windows-installers.sh exactly as the `installer:windows` job does, and inspects the results
# (7z l, msiinfo, msiextract, osslsigncode verify): MSI ProductVersion X.Y.(Z*100+S), the setup.exe / MSI mutual
# refusal (MSI LaunchCondition + AppSearch, NSIS System plugin), SIGNING-windows.txt, and that AF_REQUIRE_SIGNING=1
# without a certificate fails the job.
#
#   unreal/CI/tools/test_windows_installers.sh [--sign] [WORKDIR]
#
#   --sign    also create a throw-away self-signed code-signing certificate (openssl) and exercise Authenticode
#             signing of the game exe, the NSIS uninstaller / installer and the MSI (no time-stamp server).
# Tools: makensis, wixl, wixl-heat, msiinfo, msiextract, 7z, python3, openssl + osslsigncode (for --sign).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ci="$(cd "$here/.." && pwd)"

sign=0
if [[ "${1:-}" == "--sign" ]]; then
  sign=1
  shift
fi
work="${1:-$(mktemp -d "${TMPDIR:-/tmp}/af-wininst-XXXXXX")}"
mkdir -p "$work"
work="$(cd "$work" && pwd)"
staged="$work/staged/Windows"
rm -rf "$work/staged" "$work/out"
mkdir -p "$staged/Abyssfire/Binaries/Win64" "$staged/Abyssfire/Content/Paks" "$staged/Engine/Extras/Redist/en-us" \
  "$staged/Engine/Binaries/ThirdParty/DbgHelp" "$work/pe"

# Real (tiny) PE files so osslsigncode and 7z treat them like the UE executables.
cat >"$work/pe/dummy.nsi" <<'EOF'
Unicode true
SilentInstall silent
RequestExecutionLevel user
OutFile "${OUT}"
Section
SectionEnd
EOF
makensis -V1 "-DOUT=$staged/Abyssfire.exe" "$work/pe/dummy.nsi"
makensis -V1 "-DOUT=$staged/Abyssfire/Binaries/Win64/Abyssfire-Win64-Shipping.exe" "$work/pe/dummy.nsi"
makensis -V1 "-DOUT=$staged/Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe" "$work/pe/dummy.nsi"
head -c 6000000 /dev/urandom >"$staged/Abyssfire/Content/Paks/Abyssfire-Windows.pak"
head -c 2000000 /dev/urandom >"$staged/Abyssfire/Content/Paks/Abyssfire-Windows.ucas"
head -c 40000 /dev/urandom >"$staged/Abyssfire/Content/Paks/Abyssfire-Windows.utoc"
head -c 100000 /dev/urandom >"$staged/Engine/Binaries/ThirdParty/DbgHelp/dbghelp.dll"
printf 'Abyssfire test build\n' >"$staged/Manifest_NonUFSFiles_Win64.txt"
# a file name with a space and a '$' (NSIS / MSI escaping)
printf 'x' >"$staged/Abyssfire/Content/Paks/read me \$1.txt"

if [[ $sign -eq 1 ]]; then
  openssl req -x509 -newkey rsa:3072 -sha256 -days 7 -nodes -subj "/CN=Abyssfire CI Test Code Signing" \
    -addext "extendedKeyUsage=codeSigning" -addext "keyUsage=digitalSignature" \
    -keyout "$work/pe/key.pem" -out "$work/pe/cert.pem" 2>/dev/null
  openssl pkcs12 -export -inkey "$work/pe/key.pem" -in "$work/pe/cert.pem" -passout pass:ci-test-pass \
    -out "$work/pe/test.pfx"
  AF_WIN_CODESIGN_PFX_B64="$(base64 <"$work/pe/test.pfx" | tr -d '\n')"
  export AF_WIN_CODESIGN_PFX_B64
  export AF_WIN_CODESIGN_PFX_PASSWORD=ci-test-pass AF_WIN_TIMESTAMP_URL=none AF_WIN_VERIFY_CAFILE="$work/pe/cert.pem"
else
  unset AF_WIN_CODESIGN_PFX_B64 AF_WIN_CODESIGN_PFX_PASSWORD
fi

export AF_OUT_DIR="$work/out"
export CI_COMMIT_TAG="${CI_COMMIT_TAG:-v1.2.3}" GITLAB_CI="${GITLAB_CI:-}" CI_PIPELINE_IID="${CI_PIPELINE_IID:-42}"
unset AF_VERSION AF_MSI_VERSION AF_REQUIRE_SIGNING AF_WIN_REQUIRE_CODESIGN
if [[ $sign -eq 0 ]]; then
  echo "=== AF_REQUIRE_SIGNING=1 without a certificate must fail"
  if AF_REQUIRE_SIGNING=1 AF_OUT_DIR="$work/out-refused" "$ci/scripts/windows-installers.sh" "$staged" \
    >"$work/refused.log" 2>&1; then
    echo "FAIL: unsigned installers accepted with AF_REQUIRE_SIGNING=1" >&2
    exit 1
  fi
  echo "ok: refused"
fi
"$ci/scripts/windows-installers.sh" "$staged"

out="$work/out/installers/windows"
setup="$(ls "$out"/*-setup.exe)"
msi="$(ls "$out"/*.msi)"
echo "=== 7z l $(basename "$setup")"
7z l "$setup" | sed -n '/^   Date/,$p'
echo "=== msiinfo suminfo $(basename "$msi")"
msiinfo suminfo "$msi"
for t in Property Upgrade Shortcut Feature File AppSearch RegLocator LaunchCondition; do
  echo "=== msiinfo export $t"
  msiinfo export "$msi" "$t"
done
expect_msi="$(bash "$ci/version.sh" | sed -n 's/^AF_MSI_VERSION=//p')"
msiinfo export "$msi" Property | tr -d '\r' | grep -q "^ProductVersion[[:space:]]$expect_msi\$" ||
  { echo "FAIL: MSI ProductVersion is not $expect_msi" >&2; exit 1; }
msiinfo export "$msi" LaunchCondition | tr -d '\r' | grep -q 'Installed OR NOT NSISUNINSTALL' ||
  { echo "FAIL: the MSI does not refuse a setup.exe install" >&2; exit 1; }
msiinfo export "$msi" RegLocator | tr -d '\r' | grep -q 'CurrentVersion\\Uninstall\\Abyssfire[[:space:]]UninstallString[[:space:]]18' ||
  { echo "FAIL: RegLocator does not read the 64-bit NSIS uninstall key" >&2; exit 1; }
7z l "$setup" | grep -q 'PLUGINSDIR/System.dll' ||
  { echo "FAIL: setup.exe lacks the System plugin (MSI detection)" >&2; exit 1; }
grep -qx "signed=$sign" "$out/SIGNING-windows.txt" || { echo "FAIL: SIGNING-windows.txt is not signed=$sign" >&2; exit 1; }
echo "ok: ProductVersion $expect_msi, MSI / setup.exe refuse each other, SIGNING-windows.txt signed=$sign"
echo "=== msiextract"
rm -rf "$work/msiextract"
mkdir -p "$work/msiextract"
msiextract -C "$work/msiextract" "$msi"
if diff -r "$staged" "$work/msiextract/Abyssfire" >/dev/null; then
  echo "msiextract: identical to the staged build"
elif [[ $sign -eq 1 ]]; then
  # only the Authenticode-signed executables may differ
  changed="$(diff -rq "$staged" "$work/msiextract/Abyssfire" | grep -vcE 'Abyssfire(-Win64-Shipping)?\.exe differ' || true)"
  if [[ "$changed" != "0" ]]; then
    echo "msiextract: DIFFERS from the staged build beyond the signed executables" >&2
    exit 1
  fi
  echo "msiextract: identical except the two signed executables"
else
  echo "msiextract: DIFFERS from the staged build" >&2
  exit 1
fi
if [[ $sign -eq 1 ]]; then
  for f in "$setup" "$msi"; do
    echo "=== osslsigncode verify $(basename "$f")"
    osslsigncode verify -CAfile "$work/pe/cert.pem" -in "$f" | grep -E 'Signature verification|Subject|Message digest|Number of'
  done
fi
echo "=== outputs"
ls -l "$out"
cat "$out/SHA256SUMS-windows.txt"
echo "OK: $work"
