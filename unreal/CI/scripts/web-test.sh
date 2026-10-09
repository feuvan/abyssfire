#!/usr/bin/env bash
# CI job `web:vitest` (optional) — the original web game's Vitest suite (npm test), JUnit report for the MR widget.
set -euo pipefail
# shellcheck source=../lib/common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"
af_require_cmd node npm

cd "$AF_REPO_DIR"
if [[ ! -x node_modules/.bin/vitest || package-lock.json -nt node_modules/.package-lock.json ]]; then
  npm ci --no-audit --no-fund --prefer-offline
fi
mkdir -p "$AF_OUT_DIR/reports"
node_modules/.bin/vitest run --reporter=default --reporter=junit \
  --outputFile.junit="$AF_OUT_DIR/reports/web-vitest.xml"
