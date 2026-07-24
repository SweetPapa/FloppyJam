#!/usr/bin/env bash
#
# playthrough_screenshots.sh — auto-play a few MagLava levels and capture
# screenshots as the run progresses, saving them to ~/code/game_screenshots.
#
# It runs the game in headless-friendly demo mode: a built-in bot climbs each
# level while the app writes a numbered PNG every N frames, advancing to the
# next level automatically. A window does appear (raylib needs a GL context),
# so run it on a machine with a display.
#
# Usage:
#   tools/playthrough_screenshots.sh [levels] [every] [outdir]
#     levels  comma-separated campaign level ids   (default: 1,2,3,7)
#     every   capture one screenshot per N frames  (default: 45  ~0.75s)
#     outdir  destination directory                (default: ~/code/game_screenshots)
#
# Examples:
#   tools/playthrough_screenshots.sh
#   tools/playthrough_screenshots.sh 1,4,8 30
#   tools/playthrough_screenshots.sh 5,12,25 60 ~/Desktop/shots
set -euo pipefail

LEVELS="${1:-1,2,3,7}"
EVERY="${2:-45}"
OUTDIR="${3:-$HOME/code/game_screenshots}"

# locate the repo root (this script lives in <root>/tools)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"

# build the game if it isn't built yet
BIN="$ROOT/build/maglava"
if [ ! -x "$BIN" ]; then
  echo "Building MagLava (release) ..."
  cmake -S "$ROOT" -B "$ROOT/build" >/dev/null
  cmake --build "$ROOT/build" >/dev/null
fi

mkdir -p "$OUTDIR"
# start clean so the folder only holds this run's shots
rm -f "$OUTDIR"/shot_*.png

echo "Playing levels [$LEVELS], capturing every $EVERY frames -> $OUTDIR"

MAGLAVA_DEMO=1 \
MAGLAVA_LEVELS="$LEVELS" \
MAGLAVA_SHOTS=1 \
MAGLAVA_SHOTEVERY="$EVERY" \
MAGLAVA_SHOTDIR="$OUTDIR" \
"$BIN"

COUNT="$(ls "$OUTDIR"/shot_*.png 2>/dev/null | wc -l | tr -d ' ')"
echo "Done. Captured $COUNT screenshot(s) in $OUTDIR"
ls -1 "$OUTDIR"/shot_*.png 2>/dev/null | sed 's|.*/|  |' | head -40
