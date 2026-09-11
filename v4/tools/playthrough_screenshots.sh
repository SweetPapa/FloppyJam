#!/usr/bin/env bash
# Native demo captures. Requires a display; never writes player progress.
# Usage: playthrough_screenshots.sh [comma-separated stages] [frame interval] [output directory]
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
LEVELS="${1:-1,7,15,19,23,27,29,35,39,40}"
EVERY="${2:-240}"
OUTDIR="${3:-$ROOT/artifacts/playthrough-$(date +%Y%m%d-%H%M%S)}"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" -j
mkdir -p "$OUTDIR"
OUTDIR="$(cd "$OUTDIR" && pwd)"
MAGLAVA_DEMO=1 \
MAGLAVA_LEVELS="$LEVELS" \
MAGLAVA_PROFILE=1 \
MAGLAVA_SHOTS=1 \
MAGLAVA_SHOTEVERY="$EVERY" \
MAGLAVA_SHOTPERLEVEL=3 \
MAGLAVA_SHOTDIR="$OUTDIR" \
"$ROOT/build/maglava" | tee "$OUTDIR/playthrough.log"
