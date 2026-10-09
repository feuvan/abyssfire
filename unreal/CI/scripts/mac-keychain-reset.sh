#!/usr/bin/env bash
# after_script of `installer:mac-dmg` and `package:ios` (self-hosted macOS runner). Runs in its own shell after the job
# script, also when that script failed or its EXIT trap never ran (job cancelled, script killed): removes every
# temporary CI signing keychain (af-ci-signing.keychain-db, lib/macos.sh) from the user's keychain search list and
# deletes the ones left in this checkout's work directory. Never fails the job.
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/common.sh
source "$here/../lib/common.sh"
# shellcheck source=../lib/macos.sh
source "$here/../lib/macos.sh"

[[ "$(uname -s)" == "Darwin" ]] || exit 0
command -v security >/dev/null 2>&1 || exit 0

keep=()
stale=0
while IFS= read -r line; do keep+=("$line"); done < <(af_keychain_search_list)
if security list-keychains -d user | grep -q "/$AF_KEYCHAIN_NAME"; then stale=1; fi
if [[ $stale -eq 1 ]]; then
  # `list-keychains -s` without arguments would empty the search list: only rewrite it when something remains
  if [[ ${#keep[@]} -gt 0 ]]; then
    security list-keychains -d user -s "${keep[@]}" && af_log "removed CI signing keychain(s) from the search list"
  else
    af_warn "the search list holds only CI signing keychains; left as is"
  fi
fi
while IFS= read -r -d '' kc; do
  security delete-keychain "$kc" >/dev/null 2>&1 || rm -f "$kc"
  af_log "deleted leftover keychain $kc"
done < <(find "$AF_OUT_DIR/work" -name "$AF_KEYCHAIN_NAME" -print0 2>/dev/null)
exit 0
