#!/bin/bash
# Run the recompiled game on a virtual X display (Linux), for testing without a monitor.
#
#   tools/run_headless.sh <seconds> <out-dir> [ps2EntryRunner] [game-dir]
#
# Writes <out-dir>/game.log and, because PS2X_FRAME_DUMP is set (a diagnostic added by
# patches/ps2recomp-sub-rebellion-fixes.patch), every 30th presented frame as raw RGBA in
# <out-dir>/frames/. Convert those with tools/frames_to_png.py. Screenshots of the Xvfb
# root window come out black (it doesn't capture the OpenGL window), so use the frame dumps.
#
# Optional environment:
#   KEYS="15:x 30:x 45:Return"  press keys at those seconds after start (xdotool). PS2Recomp's
#                               default keyboard map: X/Space = Cross, Enter = Start,
#                               C = Circle, V = Triangle, arrows = D-pad.
#   PS2X_IDLE_DUMP=1            every guest thread's wait state when the game idles
#   PS2X_SPRITE_LOG=1           one line per distinct textured sprite setup and GS transfer
#   PS2X_SEQ_LOG=1              GIF packets and GS draws/transfers in a fixed window of frames
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
timeout "$SECS" "$EXE" "$GAME/SLUS_205.48" > "$OUT/game.log" 2>&1 &
GPID=$!
if [ -n "${KEYS:-}" ]; then
  (
    start=$(date +%s)
    for item in $KEYS; do
      t=${item%%:*}; k=${item#*:}
      while [ $(( $(date +%s) - start )) -lt "$t" ]; do sleep 0.2; done
      w=$(xdotool search --name "." 2>/dev/null | tail -1)
      [ -n "$w" ] && xdotool windowfocus "$w" 2>/dev/null
      xdotool keydown "$k"; sleep 0.25; xdotool keyup "$k"
      echo "key $k at ${t}s" >> "$OUT/keys.log"
    done
  ) &
fi
wait $GPID
echo "game exit: $?" >> "$OUT/game.log"
kill $XPID 2>/dev/null
