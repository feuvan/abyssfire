#!/usr/bin/env bash
# CI job `release:ios` (manual, tags only) — uploads the .ipa of `package:ios` to the generic package registry and
# adds it to the existing GitLab Release of the tag. The iOS build is manual, so it is not part of `release`.
#
# The .ipa is signed with an App Store distribution profile: it can only be uploaded to App Store Connect /
# TestFlight (Transporter, `xcrun altool`), not installed on a device, and the release link says so.
# Auth: CI_JOB_TOKEN only (the package registry and the Release links API both accept it). Re-runs are idempotent:
# an existing link ("has already been taken") counts as success.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/common.sh
source "$here/../lib/common.sh"
af_require_cmd curl
af_require_vars CI_COMMIT_TAG CI_JOB_TOKEN CI_API_V4_URL CI_PROJECT_ID
af_load_version

ipa="$(find "$AF_OUT_DIR/package/ios" -name '*.ipa' -type f | head -1)"
[[ -n "$ipa" ]] || af_die "no .ipa in $AF_OUT_DIR/package/ios (artifact of package:ios)"
name="$(basename "$ipa")"
url="$CI_API_V4_URL/projects/$CI_PROJECT_ID/packages/generic/abyssfire/$AF_VERSION/$name"
af_log "upload $name"
curl --fail-with-body --silent --show-error --retry 3 --header "JOB-TOKEN: $CI_JOB_TOKEN" \
  --upload-file "$ipa" "$url" >/dev/null

tag_enc="$(printf '%s' "$CI_COMMIT_TAG" | sed 's/+/%2B/g')"
api="$CI_API_V4_URL/projects/$CI_PROJECT_ID/releases/$tag_enc/assets/links"
body="{\"name\":\"iOS - App Store Connect upload (not installable)\",\"url\":\"$url\",\"link_type\":\"package\",\"direct_asset_path\":\"/$name\"}"
resp="$AF_OUT_DIR/work/release-ios-link.json"
mkdir -p "$(dirname "$resp")"
code="$(curl --silent --show-error --retry 3 --output "$resp" --write-out '%{http_code}' \
  --header "JOB-TOKEN: $CI_JOB_TOKEN" --header "Content-Type: application/json" --data "$body" "$api" || true)"
case "$code" in
  2??) af_log "linked $name to release $CI_COMMIT_TAG" ;;
  400 | 409)
    if grep -q 'already been taken' "$resp" 2>/dev/null; then
      af_log "link already present on release $CI_COMMIT_TAG"
    else
      cat "$resp" >&2 2>/dev/null || true
      af_die "adding the release link failed (HTTP $code)"
    fi
    ;;
  404) af_die "release $CI_COMMIT_TAG not found: run the release job first" ;;
  *)
    cat "$resp" >&2 2>/dev/null || true
    af_die "adding the release link failed (HTTP ${code:-none}); add it by hand: $url"
    ;;
esac
