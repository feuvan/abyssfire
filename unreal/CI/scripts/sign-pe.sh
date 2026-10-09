#!/usr/bin/env bash
# Authenticode-signs one PE (.exe / .dll) or .msi file IN PLACE with osslsigncode (Linux).
# Called by windows-installers.sh and by NSIS (!finalize / !uninstfinalize) - hence the in-place contract.
#
#   sign-pe.sh FILE
#
#   AF_WIN_SIGN_PFX        path of the decoded PKCS#12 (.pfx) certificate + key   (set by windows-installers.sh)
#   AF_WIN_SIGN_PASSFILE   path of a file holding its password                     (set by windows-installers.sh)
#   AF_WIN_TIMESTAMP_URL   RFC 3161 time-stamp server (default http://timestamp.digicert.com; "none" = no timestamp,
#                          the signature then expires with the certificate)
set -euo pipefail
file="${1:?usage: sign-pe.sh FILE}"
: "${AF_WIN_SIGN_PFX:?AF_WIN_SIGN_PFX is not set}"
: "${AF_WIN_SIGN_PASSFILE:?AF_WIN_SIGN_PASSFILE is not set}"
ts_url="${AF_WIN_TIMESTAMP_URL:-http://timestamp.digicert.com}"

args=(sign -pkcs12 "$AF_WIN_SIGN_PFX" -readpass "$AF_WIN_SIGN_PASSFILE" -h sha256
  -n "Abyssfire" -i "https://github.com/feuvan/abyssfire")
if [[ "$ts_url" != "none" ]]; then args+=(-ts "$ts_url"); fi

tmp="$file.signing"
rm -f "$tmp"
ok=0
for attempt in 1 2 3; do # time-stamp servers fail transiently
  if osslsigncode "${args[@]}" -in "$file" -out "$tmp" >/dev/null; then
    ok=1
    break
  fi
  rm -f "$tmp"
  echo "sign-pe.sh: attempt $attempt failed for $file" >&2
  sleep $((attempt * 5))
done
[[ $ok -eq 1 ]] || {
  echo "sign-pe.sh: could not sign $file" >&2
  exit 1
}
mv -f "$tmp" "$file"
echo "signed $(basename "$file")" >&2
