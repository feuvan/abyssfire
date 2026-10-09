#!/usr/bin/env bash
# CI job `data:check` — the committed unreal/Data/*.json (and CoreTests golden maps) must equal a fresh export of the
# web game's TypeScript tables (unreal/Tools/export-data/README.md). Exit 1 = stale data: run the exporter and commit.
#
#   data-check.sh            npm ci (when node_modules is missing or stale) + run.mjs --check
#   AF_DATA_TYPECHECK=1      also type-check the exporter (tsc -p unreal/Tools/export-data/tsconfig.json)
set -euo pipefail
# shellcheck source=../lib/common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"
af_require_cmd node npm

cd "$AF_REPO_DIR"
af_section_start npm "npm ci (root, Node $(node --version))"
if [[ ! -x node_modules/.bin/vite || package-lock.json -nt node_modules/.package-lock.json ]]; then
  npm ci --no-audit --no-fund --prefer-offline
else
  af_log "node_modules is up to date"
fi
af_section_end npm

af_section_start export "export-data --check"
node unreal/Tools/export-data/run.mjs --check
af_section_end export

if af_is_true "${AF_DATA_TYPECHECK:-0}"; then
  af_section_start tsc "type-check the exporter"
  node_modules/.bin/tsc -p unreal/Tools/export-data/tsconfig.json
  af_section_end tsc
fi
af_log "unreal/Data is up to date with the TypeScript source"
