#!/bin/bash
# Run the recompiled game on a virtual X display (Linux), for testing without a monitor.
#
#   tools/run_headless.sh <seconds> <out-dir> [ps2EntryRunner] [game-dir]
#
# Writes <out-dir>/game.log and, because PS2X_FRAME_DUMP is set (a diagnostic added by
# patches/ps2recomp-sub-rebellion-fixes.patch), every 30th presented frame as raw RGBA in
# <out-dir>/frames/. Convert those with tools/frames_to_png.py. Screenshots of the Xvfb
# root window come out black (it doesn't capture the OpenGL window), so use the frame dumps.
# PS2X_IDLE_DUMP=1 additionally prints every guest thread's wait state when the game idles.
SECS=${1:-60}
OUT=$(realpath -m "${2:-run}")
EXE=${3:-PS2Recomp/out/rt/ps2xRuntime/ps2EntryRunner}
GAME=$(realpath "${4:-game}")
EXE=$(realpath "$EXE")
mkdir -p "$OUT/frames"
Xvfb :77 -screen 0 1280x960x24 >/dev/null 2>&1 &
XPID=$!
sleep 1
export DISPLAY=:77 LIBGL_ALWAYS_SOFTWARE=1 PS2X_FRAME_DUMP="$OUT/frames"
cd "$GAME"
timeout "$SECS" "$EXE" "$GAME/SLUS_205.48" > "$OUT/game.log" 2>&1
echo "game exit: $?" >> "$OUT/game.log"
kill $XPID 2>/dev/null
