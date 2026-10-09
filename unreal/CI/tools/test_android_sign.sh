#!/usr/bin/env bash
# Local self-test of scripts/android-sign.sh without Unreal: fake APK (stored native lib) and AAB, both already signed
# with a throw-away key the way package:android leaves them (UE / Gradle sign with a per-job CI key), a test release
# keystore, then the job script in release mode (the outputs must carry ONLY the release signer) and in debug mode.
#
#   unreal/CI/tools/test_android_sign.sh [WORKDIR]
#
# Tools: zipalign, apksigner, jarsigner, keytool, python3. Distro zipalign builds older than build-tools 35 have no
# -P (16 KB) option: the test then sets AF_ANDROID_ALLOW_4K=1 (the CI job installs build-tools 36 instead).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ci="$(cd "$here/.." && pwd)"
work="${1:-$(mktemp -d "${TMPDIR:-/tmp}/af-android-XXXXXX")}"
mkdir -p "$work"
work="$(cd "$work" && pwd)"
rm -rf "$work/in" "$work/out-release" "$work/out-debug"
mkdir -p "$work/in/package/android"

python3 - "$work/in/package/android" <<'PY'
import os, sys, zipfile
d = sys.argv[1]
with zipfile.ZipFile(os.path.join(d, "Abyssfire.apk"), "w") as z:
    z.writestr("AndroidManifest.xml", b"\x03\x00\x08\x00" + b"\x00" * 60, zipfile.ZIP_DEFLATED)
    z.writestr("classes.dex", b"dex\n035\x00" + os.urandom(2000), zipfile.ZIP_DEFLATED)
    z.writestr("lib/arm64-v8a/libUnreal.so", b"\x7fELF" + os.urandom(300000), zipfile.ZIP_STORED)
    z.writestr("assets/main.obb.png", os.urandom(50000), zipfile.ZIP_STORED)
with zipfile.ZipFile(os.path.join(d, "Abyssfire.aab"), "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("BundleConfig.pb", b"\x0a\x00")
    z.writestr("base/manifest/AndroidManifest.xml", b"\x0a\x00" + os.urandom(100))
    z.writestr("base/lib/arm64-v8a/libUnreal.so", b"\x7fELF" + os.urandom(300000))
    z.writestr("base/dex/classes.dex", b"dex\n035\x00" + os.urandom(2000))
    z.writestr("META-INF/OLD.SF", b"stale signature from a previous signer")
PY

# JKS: separate key password (a PKCS12 keystore, the keytool default, always uses the store password)
keytool -genkeypair -storetype JKS -keystore "$work/in/release.keystore" -storepass ci-store-pass -keypass ci-key-pass \
  -alias abyssfire -keyalg RSA -keysize 3072 -validity 365 -dname "CN=Abyssfire CI Test,O=feuvan,C=CN" >/dev/null 2>&1

# what package:android hands over: APK and AAB signed with the job's throw-away key
keytool -genkeypair -keystore "$work/in/throwaway.keystore" -storepass throwaway -keypass throwaway -alias ci \
  -keyalg RSA -keysize 2048 -validity 2 -dname "CN=Abyssfire CI throw-away" >/dev/null 2>&1
apksigner sign --ks "$work/in/throwaway.keystore" --ks-pass pass:throwaway --ks-key-alias ci --min-sdk-version 26 \
  --out "$work/in/signed.apk" "$work/in/package/android/Abyssfire.apk"
mv -f "$work/in/signed.apk" "$work/in/package/android/Abyssfire.apk"
rm -f "$work/in/signed.apk.idsig"
jarsigner -keystore "$work/in/throwaway.keystore" -storepass throwaway "$work/in/package/android/Abyssfire.aab" ci \
  >/dev/null 2>&1

allow4k=0
if ! zipalign 2>&1 | grep -q -- '-P'; then allow4k=1; fi

run() { # run MODE
  local mode="$1" out="$work/out-$1"
  mkdir -p "$out/package"
  cp -R "$work/in/package/android" "$out/package/"
  (
    export AF_OUT_DIR="$out" AF_ANDROID_ALLOW_4K="$allow4k" CI_COMMIT_TAG=v1.2.3 GITLAB_CI=1 CI_PIPELINE_IID=7
    unset AF_VERSION
    if [[ "$mode" == "release" ]]; then
      AF_ANDROID_KEYSTORE_B64="$(base64 <"$work/in/release.keystore" | tr -d '\n')"
      export AF_ANDROID_KEYSTORE_B64 AF_ANDROID_KEYSTORE_PASSWORD=ci-store-pass AF_ANDROID_KEY_ALIAS=abyssfire \
        AF_ANDROID_KEY_PASSWORD=ci-key-pass
    else
      export AF_REQUIRE_SIGNING=0
      unset AF_ANDROID_KEYSTORE_B64
    fi
    "$ci/scripts/android-sign.sh"
  )
  echo "=== [$mode] outputs"
  ls -l "$out/installers/android"
  apksigner verify --min-sdk-version 26 --print-certs "$out/installers/android/"*.apk | grep -E 'Signer #1 certificate DN'
  jarsigner -verify -verbose "$out/installers/android/"*.aab 2>/dev/null | grep -E 'META-INF/.*\.(SF|RSA)|jar verified|unsigned' || true
}
run release
echo "=== [release] only the release signer remains"
signers="$(apksigner verify --min-sdk-version 26 --print-certs "$work/out-release/installers/android/"*.apk |
  grep -E '^Signer #[0-9]+ certificate DN')"
if [[ "$signers" != "Signer #1 certificate DN: CN=Abyssfire CI Test, O=feuvan, C=CN" ]]; then
  echo "FAIL: APK signers after re-signing: $signers" >&2
  exit 1
fi
aab_sigs="$(unzip -Z1 "$work/out-release/installers/android/"*.aab | grep -E '^META-INF/.*\.(SF|RSA|EC|DSA)$' | tr '\n' ' ')"
if [[ "$aab_sigs" != "META-INF/ABYSSFIR.SF META-INF/ABYSSFIR.RSA " ]]; then
  echo "FAIL: AAB signature files after re-signing: $aab_sigs" >&2
  exit 1
fi
echo "ok: APK and AAB carry only the release key (the throw-away signature is gone)"
run debug
echo "=== release mode must fail without a keystore on a tag"
if env -u AF_ANDROID_KEYSTORE_B64 -u AF_VERSION -u AF_REQUIRE_SIGNING AF_OUT_DIR="$work/out-debug" \
  CI_COMMIT_TAG=v1.2.3 GITLAB_CI=1 "$ci/scripts/android-sign.sh" 2>/dev/null; then
  echo "FAIL: unsigned tag build was accepted" >&2
  exit 1
fi
echo "ok: refused"
echo "OK: $work"
