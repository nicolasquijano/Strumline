#!/usr/bin/env bash
# Builds Strumline on Linux and macOS (build.cmd does it on Windows).
#   ./build.sh                 configure + build everything
#   ./build.sh target <name>   build one target (e.g. Strumline_VST3, strumline_check)
#
# Needs: CMake 3.25+, Ninja (or any generator CMake finds), a C++20 compiler, and JUCE's system libraries (Linux: see .github/workflows/build.yml for the list).
# The chord converter also needs ONNX Runtime:  cmake -P tools/fetch_onnxruntime.cmake third_party/onnxruntime   and then
#   STRUMLINE_ORT_DIR=$PWD/third_party/onnxruntime ./build.sh
set -euo pipefail
cd "$(dirname "$0")"

args=(-S . -B build -DCMAKE_BUILD_TYPE=Release)
if command -v ninja >/dev/null 2>&1; then args+=(-G Ninja); fi
if [ -n "${STRUMLINE_ORT_DIR:-}" ]; then args+=("-DSTRUMLINE_ORT_DIR=${STRUMLINE_ORT_DIR}"); fi
cmake "${args[@]}"

if [ "${1:-}" = "target" ]; then
  cmake --build build --target "$2" --parallel
else
  cmake --build build --parallel
fi
