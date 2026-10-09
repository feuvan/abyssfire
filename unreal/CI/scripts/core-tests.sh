#!/usr/bin/env bash
# CI job `core:*` — AbyssCore unit tests through unreal/CoreTests/run.sh (CMake + doctest), one compiler per job.
#
#   core-tests.sh gcc|clang [--sanitize] [--shared]
#
#   --sanitize   ABYSS_SANITIZE=1 (ASan + UBSan + float-cast-overflow, build-<cc>-asan/)
#   --shared     ABYSS_SHARED=1   (hidden-visibility shared core: a missing ABYSS_API fails to link)
#
# Writes a JUnit report to $AF_OUT_DIR/reports/core-<variant>.xml (ctest >= 3.21) for the MR test widget.
# Needs: cmake >= 3.20, ninja (optional), gcc/g++ and/or clang/clang++.
set -euo pipefail
# shellcheck source=../lib/common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

compiler="${1:-}"
case "$compiler" in
  gcc) af_require_cmd gcc g++ ;;
  clang) af_require_cmd clang clang++ ;;
  *)
    echo "usage: $0 gcc|clang [--sanitize] [--shared]" >&2
    exit 2
    ;;
esac
shift

variant="$compiler"
export ABYSS_SANITIZE=0 ABYSS_SHARED=0
for arg in "$@"; do
  case "$arg" in
    --sanitize)
      ABYSS_SANITIZE=1
      variant="$variant-asan"
      ;;
    --shared)
      ABYSS_SHARED=1
      variant="$variant-shared"
      ;;
    *) af_die "unknown option $arg" ;;
  esac
done
af_require_cmd cmake ctest

if [[ "$ABYSS_SANITIZE" == "1" ]]; then
  # Leak checking needs ptrace-like access that some container runtimes deny; AF_ASAN_DETECT_LEAKS=0 turns it off.
  export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=${AF_ASAN_DETECT_LEAKS:-1}:abort_on_error=1:strict_string_checks=1}"
  export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
fi

reports="$AF_OUT_DIR/reports"
mkdir -p "$reports"
junit="$reports/core-$variant.xml"
ctest_version="$(ctest --version | head -1 | awk '{print $3}')"
extra="${CTEST_ARGS:-}"
if printf '%s\n%s\n' "3.21.0" "$ctest_version" | sort -V -C; then
  extra="--output-junit $junit $extra"
else
  af_warn "ctest $ctest_version has no --output-junit (needs 3.21); no JUnit report"
fi

af_section_start core "AbyssCore tests: $variant ($("${compiler/gcc/g++}" --version 2>/dev/null | head -1 || true))"
set +e
CTEST_ARGS="$extra" "$AF_UNREAL_DIR/CoreTests/run.sh" "$compiler"
status=$?
set -e
af_section_end core
if [[ $status -ne 0 ]]; then af_die "AbyssCore tests failed ($variant)"; fi
af_log "AbyssCore tests passed ($variant)"
