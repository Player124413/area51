#!/usr/bin/env bash
#===============================================================================
#
#  run_tests.sh - builds and runs the host unit tests for the native core.
#
#  The tests compile exactly the same .cpp files that go into liba51.so, so this
#  is a real check of the shipped code (not a re-implementation).  Only a C++17
#  compiler is needed - no Android SDK, no CMake.
#
#  Usage:   ./run_tests.sh            (build + run everything)
#           CXX=clang++ ./run_tests.sh
#
#===============================================================================

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE="$HERE/../app/src/main/cpp"
BUILD="$HERE/build"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O2 -g -Wall -Wextra -Wno-unused-parameter -fno-exceptions}"

CORE_SOURCES=(
    "$CORE/a51/crc16.cpp"
    "$CORE/a51/dfs_archive.cpp"
    "$CORE/a51/input_gadgets.cpp"
    "$CORE/a51/input_state.cpp"
    "$CORE/a51/json.cpp"
    "$CORE/a51/perf_governor.cpp"
    "$CORE/a51/touch_layout.cpp"
)

TEST_SUITES=(
    test_dfs
    test_perf
    test_touch
    test_json
)

echo "==> compiler: $("$CXX" --version | head -n 1)"

for SRC in "${CORE_SOURCES[@]}"; do
    if [ ! -f "$SRC" ]; then
        echo "missing core source: $SRC" >&2
        exit 2
    fi
done

mkdir -p "$BUILD"
rm -f "$BUILD"/*.o

FAILED=0

echo "==> compiling core"
CORE_OBJS=()
for SRC in "${CORE_SOURCES[@]}"; do
    OBJ="$BUILD/$(basename "${SRC%.cpp}").o"
    # shellcheck disable=SC2086
    "$CXX" $CXXFLAGS -I"$CORE" -I"$HERE" -c "$SRC" -o "$OBJ"
    CORE_OBJS+=("$OBJ")
done

for SUITE in "${TEST_SUITES[@]}"; do
    echo
    echo "==> building $SUITE"
    # shellcheck disable=SC2086
    "$CXX" $CXXFLAGS -I"$CORE" -I"$HERE" "$HERE/$SUITE.cpp" "${CORE_OBJS[@]}" -o "$BUILD/$SUITE"

    echo "==> running  $SUITE"
    if ! "$BUILD/$SUITE"; then
        FAILED=1
    fi
    echo
done

if [ "$FAILED" -ne 0 ]; then
    echo "NATIVE TESTS FAILED"
    exit 1
fi

echo "NATIVE TESTS OK"
