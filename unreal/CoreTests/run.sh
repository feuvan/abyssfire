#!/usr/bin/env bash
# Builds AbyssCore + tests with GCC and Clang (build-gcc/, build-clang/) and runs ctest for both.
#   ./run.sh            build both and test
#   ./run.sh gcc        only GCC     ./run.sh clang   only Clang
#   ABYSS_SANITIZE=1 ./run.sh       ASan + UBSan builds (build-*-asan/)
#   ABYSS_SHARED=1 ./run.sh         AbyssCore as a hidden-visibility shared library (build-*-shared/): a public symbol
#                                   without ABYSS_API fails to link, like a UE modular (editor / DLL) build would
#   CTEST_ARGS="-R core.base" ./run.sh   pass extra arguments to ctest
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
jobs="$(nproc 2>/dev/null || echo 4)"
generator=()
if command -v ninja >/dev/null 2>&1; then generator=(-G Ninja); fi

if [[ $# -gt 0 ]]; then which_compilers=("$@"); else which_compilers=(gcc clang); fi

suffix=""
sanitize=OFF
shared=OFF
if [[ "${ABYSS_SANITIZE:-0}" == "1" ]]; then suffix="-asan"; sanitize=ON; fi
if [[ "${ABYSS_SHARED:-0}" == "1" ]]; then suffix="$suffix-shared"; shared=ON; fi

status=0
for comp in "${which_compilers[@]}"; do
  case "$comp" in
    gcc) cc=gcc; cxx=g++ ;;
    clang) cc=clang; cxx=clang++ ;;
    *) echo "unknown compiler '$comp' (use gcc or clang)"; exit 2 ;;
  esac
  dir="$here/build-$comp$suffix"
  echo "=== [$comp] configure ($dir)"
  CC="$cc" CXX="$cxx" cmake -S "$here" -B "$dir" "${generator[@]}" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DABYSS_SANITIZE="$sanitize" -DABYSS_SHARED="$shared" >/dev/null
  echo "=== [$comp] build"
  cmake --build "$dir" -j "$jobs"
  echo "=== [$comp] test"
  # shellcheck disable=SC2086
  if ! ctest --test-dir "$dir" --output-on-failure -j "$jobs" ${CTEST_ARGS:-}; then status=1; fi
done
exit $status
