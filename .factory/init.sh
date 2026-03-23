#!/bin/bash
set -e

cd "$(dirname "$0")/.."

# Install dependencies (idempotent)
if [ ! -d "node_modules" ] || [ "package.json" -nt "node_modules/.package-lock.json" ]; then
  npm install
fi

# Ensure vitest is available (will be added in foundation milestone)
if ! npx vitest --version >/dev/null 2>&1; then
  echo "Warning: vitest not yet installed. Will be set up in foundation milestone."
fi
