#!/bin/bash
# Cross-compile the Windows build on Linux with MinGW-w64, after tools/setup_linux.sh has
# patched PS2Recomp and copied the generated code and overrides into it. From the repo root:
#
#   tools/build_windows.sh
#
# Produces dist/windows/: SubRebellion.exe (static, needs only Windows' own DLLs),
# extract_disc.exe, Setup.cmd, "Play Sub Rebellion.cmd" and README.txt. The game files are not
# included; Setup.cmd unpacks them from the user's own disc image. Takes ~10 minutes on 2 cores.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
command -v x86_64-w64-mingw32-g++-posix >/dev/null || \
  sudo apt-get install -y -q g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix
[ -f PS2Recomp/ps2xRuntime/src/runner/register_functions.cpp ] || { echo "Run tools/setup_linux.sh first"; exit 1; }
cp overrides/*.cpp PS2Recomp/ps2xRuntime/src/runner/

if [ ! -d PS2Recomp/out/win ]; then
  cmake -S PS2Recomp -B PS2Recomp/out/win -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-x86_64.cmake" \
    -DCMAKE_BUILD_TYPE=None -DCMAKE_CXX_FLAGS="-O1 -g0 -msse4.1 -mavx2" -DCMAKE_C_FLAGS="-O2" \
    -DPS2X_BUILD_RECOMP=OFF -DPS2X_BUILD_ANALYZER=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF \
    -DPS2X_BUILD_RUNTIME=ON -DPS2X_ENABLE_FFMPEG=OFF -DPS2X_ENABLE_DEBUG_UI=OFF \
    -DPS2X_ENABLE_AGRESSIVE_LOGS=OFF -DPS2X_RUNTIME_LIB_COMPILE_OPTIONS=-O2
fi
cmake --build PS2Recomp/out/win -j"$(nproc)" --target ps2EntryRunner

mkdir -p dist/windows
x86_64-w64-mingw32-strip -o dist/windows/SubRebellion.exe PS2Recomp/out/win/ps2xRuntime/ps2EntryRunner.exe
x86_64-w64-mingw32-g++-posix -O2 -std=c++17 -static -s -o dist/windows/extract_disc.exe tools/extract_disc.cpp
cp windows/* dist/windows/
ls -la dist/windows
