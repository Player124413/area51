#!/usr/bin/env bash
#===============================================================================
#
#  check_android_sources.sh
#
#  Type checks the Android-only sources (the JNI bridge and the GLES renderer)
#  against stub NDK headers.  This is NOT a real NDK build - it is a cheap way
#  to catch typos, wrong argument types and missing includes on any machine and
#  in CI, before the APK build itself runs.
#
#===============================================================================

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE="$HERE/../app/src/main/cpp"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -fno-exceptions}"

echo "==> type checking android-only sources with $("$CXX" --version | head -n 1)"

"$CXX" $CXXFLAGS -fsyntax-only \
    -I"$CORE" \
    -I"$HERE/stubs" \
    "$CORE/jni_bridge.cpp"

"$CXX" $CXXFLAGS -fsyntax-only \
    -I"$CORE" \
    -I"$HERE/stubs" \
    "$CORE/renderer/gl_runtime.cpp"

"$CXX" $CXXFLAGS -fsyntax-only \
    -I"$CORE" \
    -I"$HERE/stubs" \
    "$CORE/a51/runtime.cpp"

echo "ANDROID SOURCES OK"
