# shellcheck shell=bash
# macOS signing helpers (temporary keychain, codesign, notarytool) for the self-hosted Mac runner.
# Sourced after common.sh. Secrets come only from masked / protected CI variables (unreal/Docs/CI.md):
#
#   AF_MAC_DEVID_P12_B64 / AF_MAC_DEVID_P12_PASSWORD   Developer ID Application certificate + key (.p12, base64)
#   AF_MAC_SIGN_IDENTITY                               optional: identity name or SHA-1 (default: first
#                                                      "Developer ID Application" in the temporary keychain)
#   AF_ASC_API_KEY_P8_B64 / AF_ASC_API_KEY_ID / AF_ASC_API_ISSUER_ID   App Store Connect API key for notarytool
#
# The temporary keychain lives in the job's work directory, is added to the user search list for the duration of
# the job (xcodebuild has no --keychain option) and is deleted with the original search list restored on exit; the
# keys are imported non-extractable, the keychain is locked whenever nothing signs and auto-locks when idle, and the
# jobs' after_script (scripts/mac-keychain-reset.sh) removes what a killed job left behind. AF_KEYCHAIN_PASSWORD is
# a plain (never exported) shell variable, random per job.

AF_KEYCHAIN=""
AF_KEYCHAIN_PASSWORD=""
AF_KEYCHAIN_SAVED_LIST=()
AF_KEYCHAIN_NAME="af-ci-signing.keychain-db"

# af_keychain_search_list -> the user keychain search list, one path per line, without any CI signing keychain
# (a stale one left by a killed job must not be restored into the list)
af_keychain_search_list() {
  local line
  while IFS= read -r line; do
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line#\"}"
    line="${line%\"}"
    case "$line" in
      "" | */"$AF_KEYCHAIN_NAME") ;;
      *) printf '%s\n' "$line" ;;
    esac
  done < <(security list-keychains -d user)
}

# af_keychain_create DIR [AUTO_LOCK_SECONDS] -> temporary keychain, unlocked, first in the search list. It locks
# itself after AUTO_LOCK_SECONDS without use (default 1800; package:ios passes the length of its build, because
# xcodebuild signs at the end of it).
af_keychain_create() {
  local dir="$1" lock_after="${2:-1800}"
  af_require_cmd security
  mkdir -p "$dir"
  AF_KEYCHAIN="$dir/$AF_KEYCHAIN_NAME"
  AF_KEYCHAIN_PASSWORD="$(af_random_password)"
  [[ -n "$AF_KEYCHAIN_PASSWORD" ]] || af_die "could not generate a keychain password"
  rm -f "$AF_KEYCHAIN"
  security create-keychain -p "$AF_KEYCHAIN_PASSWORD" "$AF_KEYCHAIN"
  security set-keychain-settings -lut "$lock_after" "$AF_KEYCHAIN"
  security unlock-keychain -p "$AF_KEYCHAIN_PASSWORD" "$AF_KEYCHAIN"
  local line
  AF_KEYCHAIN_SAVED_LIST=()
  while IFS= read -r line; do AF_KEYCHAIN_SAVED_LIST+=("$line"); done < <(af_keychain_search_list)
  if [[ ${#AF_KEYCHAIN_SAVED_LIST[@]} -gt 0 ]]; then
    security list-keychains -d user -s "$AF_KEYCHAIN" "${AF_KEYCHAIN_SAVED_LIST[@]}"
  else
    security list-keychains -d user -s "$AF_KEYCHAIN"
  fi
  af_log "temporary keychain $AF_KEYCHAIN"
}

# af_keychain_import_p12 B64_VAR PASSWORD_VAR
af_keychain_import_p12() {
  local b64var="$1" pwvar="$2" p12
  [[ -n "$AF_KEYCHAIN" ]] || af_die "af_keychain_create first"
  p12="$(dirname "$AF_KEYCHAIN")/import-$$.p12"
  af_secret_to_file "$b64var" "$p12"
  # -x: the private key can never be exported again (not even by `security export` while the keychain is unlocked);
  # only codesign / productbuild get access without a prompt
  security import "$p12" -k "$AF_KEYCHAIN" -f pkcs12 -x -P "$(af_secret_value "$pwvar")" \
    -T /usr/bin/codesign -T /usr/bin/productbuild >/dev/null ||
    {
      rm -f "$p12"
      af_die "importing $b64var failed (wrong $pwvar, or not a PKCS#12 file)"
    }
  rm -f "$p12"
  security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$AF_KEYCHAIN_PASSWORD" "$AF_KEYCHAIN" >/dev/null
}

# af_keychain_identity PATTERN -> prints the SHA-1 of the first valid code-signing identity whose name matches
af_keychain_identity() {
  local pattern="$1" line
  line="$(security find-identity -v -p codesigning "$AF_KEYCHAIN" | grep -F "$pattern" | head -1 || true)"
  [[ -n "$line" ]] || return 1
  printf '%s\n' "$line" | awk '{print $2}'
}

af_keychain_lock() {
  if [[ -n "$AF_KEYCHAIN" ]]; then security lock-keychain "$AF_KEYCHAIN" 2>/dev/null || true; fi
}
af_keychain_unlock() {
  [[ -n "$AF_KEYCHAIN" ]] || af_die "af_keychain_create first"
  security unlock-keychain -p "$AF_KEYCHAIN_PASSWORD" "$AF_KEYCHAIN"
}

af_keychain_cleanup() {
  if [[ -n "$AF_KEYCHAIN" ]]; then
    if [[ ${#AF_KEYCHAIN_SAVED_LIST[@]} -gt 0 ]]; then
      security list-keychains -d user -s "${AF_KEYCHAIN_SAVED_LIST[@]}" 2>/dev/null || true
    fi
    security delete-keychain "$AF_KEYCHAIN" 2>/dev/null || true
    rm -f "$AF_KEYCHAIN"
    AF_KEYCHAIN=""
  fi
}

# af_codesign_bundle APP IDENTITY ENTITLEMENTS
# Signs every Mach-O inside the bundle inside-out (libraries, helpers, nested bundles), then the bundle with the
# hardened runtime and a secure timestamp (both required by notarisation). IDENTITY "-" = ad-hoc: no timestamp and
# NO hardened runtime - its library validation refuses ad-hoc dylibs (no Team ID), the app would not start.
af_codesign_bundle() {
  local app="$1" identity="$2" entitlements="$3"
  local ts=(--timestamp) rt=(--options runtime) kc=() f
  if [[ "$identity" == "-" ]]; then
    ts=(--timestamp=none)
    rt=()
  fi
  if [[ -n "$AF_KEYCHAIN" ]]; then kc=(--keychain "$AF_KEYCHAIN"); fi
  # ${kc[@]+...}: bash 3.2 (macOS /bin/bash) treats an empty array as unbound under `set -u`
  local sign=(codesign --force ${rt[@]+"${rt[@]}"} "${ts[@]}" ${kc[@]+"${kc[@]}"} --sign "$identity")

  # 1. loose Mach-O files (dylibs, executables, plugins), deepest first
  while IFS= read -r -d '' f; do
    if file -b "$f" | grep -q 'Mach-O'; then "${sign[@]}" "$f" >/dev/null; fi
  done < <(find "$app/Contents" -depth -type f \( -perm -u+x -o -name '*.dylib' -o -name '*.so' \) -print0)
  # 2. nested bundles, deepest first
  while IFS= read -r -d '' f; do
    "${sign[@]}" "$f" >/dev/null
  done < <(find "$app/Contents" -depth -type d \( -name '*.framework' -o -name '*.bundle' -o -name '*.app' \
    -o -name '*.xpc' -o -name '*.appex' \) -print0)
  # 3. the app itself, with entitlements
  "${sign[@]}" --entitlements "$entitlements" "$app"
  codesign --verify --deep --strict --verbose=2 "$app"
}

# af_notary_prepare DIR -> decodes the App Store Connect API key once into DIR (mode 700) and keeps its id / issuer
# in plain shell variables, so the secret variables can be unset before anything else runs (af_unset_secrets).
AF_NOTARY_KEY=""
AF_NOTARY_KEY_ID=""
AF_NOTARY_ISSUER=""
af_notary_prepare() {
  local dir="$1"
  af_require_vars AF_ASC_API_KEY_P8_B64 AF_ASC_API_KEY_ID AF_ASC_API_ISSUER_ID
  mkdir -p "$dir"
  chmod 700 "$dir"
  AF_NOTARY_KEY="$dir/AuthKey_ci.p8"
  af_secret_to_file AF_ASC_API_KEY_P8_B64 "$AF_NOTARY_KEY"
  AF_NOTARY_KEY_ID="$(af_secret_value AF_ASC_API_KEY_ID)"
  AF_NOTARY_ISSUER="$(af_secret_value AF_ASC_API_ISSUER_ID)"
}

# af_notarize FILE -> submits FILE (zip / dmg / pkg) with the key from af_notary_prepare and waits
# (AF_NOTARY_TIMEOUT, default 1h: installer:mac-dmg notarises twice within its 3h job timeout).
af_notarize() {
  local file="$1" out status id auth
  af_require_cmd xcrun
  [[ -n "$AF_NOTARY_KEY" && -s "$AF_NOTARY_KEY" ]] || af_die "af_notary_prepare first"
  auth=(--key "$AF_NOTARY_KEY" --key-id "$AF_NOTARY_KEY_ID" --issuer "$AF_NOTARY_ISSUER")
  mkdir -p "$AF_LOG_DIR"
  af_log "notarytool submit $(basename "$file") (waits up to ${AF_NOTARY_TIMEOUT:-1h})"
  out="$(xcrun notarytool submit "$file" "${auth[@]}" --wait --timeout "${AF_NOTARY_TIMEOUT:-1h}" \
    --output-format json 2>&1)" || true
  printf '%s\n' "$out" | tee "$AF_LOG_DIR/notary-$(basename "$file").json" >&2
  status="$(printf '%s' "$out" | sed -n 's/.*"status"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)"
  id="$(printf '%s' "$out" | sed -n 's/.*"id"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)"
  if [[ "$status" != "Accepted" ]]; then
    if [[ -n "$id" ]]; then
      xcrun notarytool log "$id" "${auth[@]}" "$AF_LOG_DIR/notary-$id-log.json" || true
      cat "$AF_LOG_DIR/notary-$id-log.json" >&2 2>/dev/null || true
    fi
    af_die "notarisation of $(basename "$file") failed (status '${status:-none}', submission ${id:-?})"
  fi
  af_log "notarised $(basename "$file") (submission $id)"
}
