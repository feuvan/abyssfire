#!/bin/bash
set -e

cd /Users/feuvan/src/abyssfire

# Install dependencies if needed
if [ ! -d "node_modules" ]; then
  npm install
fi
