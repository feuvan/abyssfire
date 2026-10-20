#!/usr/bin/env bash
# Builds the pure presentation helpers' tests with GCC and Clang (build-gcc/, build-clang/) and runs them.
#   ./run.sh            build both and test
#   ./run.sh gcc        only GCC     ./run.sh clang   only Clang
#   ABYSS_SANITIZE=1 ./run.sh       ASan + UBSan builds (build-*-asan/)
#   ABYSS_JOBS=4 ./run.sh           parallel build jobs (default 2: the core library is large and shares the machine)
#   CTEST_ARGS="-V" ./run.sh        pass extra arguments to ctest
#   PRESENT_ARGS="-ts=theme" ./run.sh   doctest arguments for the runner (one suite, -s for every assertion)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
jobs="${ABYSS_JOBS:-2}"
generator=()
if command -v ninja >/dev/null 2>&1; then generator=(-G Ninja); fi

if [[ $# -gt 0 ]]; then which_compilers=("$@"); else which_compilers=(gcc clang); fi

suffix=""
sanitize=OFF
if [[ "${ABYSS_SANITIZE:-0}" == "1" ]]; then suffix="-asan"; sanitize=ON; fi

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
    -DABYSS_SANITIZE="$sanitize" >/dev/null
  echo "=== [$comp] build"
  cmake --build "$dir" -j "$jobs"
  echo "=== [$comp] test"
  if [[ -n "${PRESENT_ARGS:-}" ]]; then
    # shellcheck disable=SC2086
    if ! "$dir/present_tests" --no-intro --no-version ${PRESENT_ARGS}; then status=1; fi
  else
    # shellcheck disable=SC2086
    if ! ctest --test-dir "$dir" --output-on-failure ${CTEST_ARGS:-}; then status=1; fi
  fi
done
exit $status
