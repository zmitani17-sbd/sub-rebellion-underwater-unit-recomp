#!/bin/bash
# Rebuild the Sub Rebellion recomp from scratch on Linux (Ubuntu 24.04), from the repo root:
#
#   tools/setup_linux.sh <path to "Sub Rebellion (USA).bin">
#
# Steps: install packages, clone PS2Recomp at the tested commit and apply our patch, build the
# analyzer and recompiler, unpack the disc into game/, recompile SLUS_205.48 into output/, copy
# the generated code into the runtime, and build ps2EntryRunner. Each step is skipped when its
# output exists. Takes ~15 minutes on 2 cores, mostly the final compile.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
BIN=${1:-}
PS2RECOMP_COMMIT=e563698   # ran-j/PS2Recomp, 2026-11-01; the patch applies to this commit

if ! command -v Xvfb >/dev/null; then
  sudo apt-get update -q
  sudo apt-get install -y -q build-essential cmake git python3-pip libx11-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libgl1-mesa-dri libasound2-dev \
    xvfb x11-apps imagemagick gdb
fi
python3 -c "import PIL, capstone" 2>/dev/null || pip install --break-system-packages -q pillow capstone

if [ ! -d PS2Recomp ]; then
  git clone --recurse-submodules https://github.com/ran-j/PS2Recomp.git
  git -C PS2Recomp checkout -q "$PS2RECOMP_COMMIT"
  git -C PS2Recomp submodule update --init --recursive -q
  git -C PS2Recomp apply "$ROOT/patches/ps2recomp-sub-rebellion-fixes.patch"
fi

if [ ! -x PS2Recomp/out/build/ps2xRecomp/ps2_recomp ]; then
  cmake -S PS2Recomp -B PS2Recomp/out/build -DCMAKE_BUILD_TYPE=Release \
    -DPS2X_BUILD_STUDIO=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_RUNTIME=OFF
  cmake --build PS2Recomp/out/build -j"$(nproc)"
fi

if [ ! -f game/SLUS_205.48 ]; then
  [ -n "$BIN" ] || { echo "Pass the path to your Sub Rebellion (USA).bin"; exit 1; }
  python3 -I tools/iso9660.py "$BIN" game '*'
fi

if [ ! -f output/register_functions.cpp ]; then
  PS2Recomp/out/build/ps2xRecomp/ps2_recomp config/sub-rebellion.toml
fi

cp output/*.cpp PS2Recomp/ps2xRuntime/src/runner/
cp output/*.h PS2Recomp/ps2xRuntime/include/

if [ ! -d PS2Recomp/out/rt ]; then
  cmake -S PS2Recomp -B PS2Recomp/out/rt -DCMAKE_BUILD_TYPE=None \
    -DCMAKE_CXX_FLAGS="-O1 -g0 -msse4.1 -mavx2" \
    -DPS2X_BUILD_RECOMP=OFF -DPS2X_BUILD_ANALYZER=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF \
    -DPS2X_BUILD_RUNTIME=ON -DPS2X_ENABLE_FFMPEG=OFF -DPS2X_ENABLE_DEBUG_UI=OFF \
    -DPS2X_ENABLE_AGRESSIVE_LOGS=OFF
fi
cmake --build PS2Recomp/out/rt -j"$(nproc)" --target ps2EntryRunner
echo "Built PS2Recomp/out/rt/ps2xRuntime/ps2EntryRunner. Try: tools/run_headless.sh 60 run"
