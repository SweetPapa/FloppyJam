#!/usr/bin/env bash
#
# make-icons.sh — turn art/icon-1024.png into the platform icon formats.
#
#   ./scripts/make-icons.sh
#
# Produces:
#   art/BreakPar.icns      macOS app bundle icon   (needs iconutil, macOS only)
#   art/breakpar.ico       Windows executable icon (needs ImageMagick)
#   promo-site/public/games/breakpar/icon-512.png
#
# The generated files are committed. They are small, they never change unless the
# source art does, and committing them means neither CI runner needs iconutil or
# ImageMagick installed just to produce a release.
#
# Note on the game's "no asset files" rule: an icon is linked into the executable
# or lives in the .app bundle, so nothing is loaded from disk at runtime. The
# rule is about the game shipping data files, and this does not break it.
#
set -euo pipefail

SRC="${1:-art/icon-1024.png}"
cd "$(dirname "$0")/.."
[ -f "$SRC" ] || { echo "error: no source art at $SRC" >&2; exit 1; }

have() { command -v "$1" >/dev/null 2>&1; }

# ---- macOS .icns -----------------------------------------------------
if have iconutil && have sips; then
    SET="$(mktemp -d)/BreakPar.iconset"
    mkdir -p "$SET"
    # Stops at 512 (256x256@2x) rather than going to 1024. The full Apple set
    # with an unoptimised 1024 frame produced a 1.8 MB .icns — larger than the
    # floppy ceiling the whole game is built to, spent entirely on an icon.
    # 512 is what Finder shows at any realistic zoom.
    for spec in "16:16x16" "32:16x16@2x" "32:32x32" "64:32x32@2x" \
                "128:128x128" "256:128x128@2x" "256:256x256" "512:256x256@2x"; do
        px="${spec%%:*}"; name="${spec##*:}"
        if have magick; then
            # quantised + max compression; sips writes these several times bigger
            magick "$SRC" -resize "${px}x${px}" -colors 128 -depth 8 \
                -define png:compression-level=9 "$SET/icon_$name.png"
        else
            sips -z "$px" "$px" "$SRC" --out "$SET/icon_$name.png" >/dev/null
        fi
    done
    iconutil -c icns "$SET" -o art/BreakPar.icns
    rm -rf "$(dirname "$SET")"
    echo "art/BreakPar.icns          $(wc -c < art/BreakPar.icns) bytes"
else
    echo "skip: .icns needs iconutil + sips (macOS)" >&2
fi

# ---- Windows .ico ----------------------------------------------------
# Deliberately capped at 128px. windres links this straight into the .exe and
# the whole binary has a 1,474,560-byte ceiling to live under, so a 256px frame
# is a fifth of a floppy spent on something Explorer rarely shows.
if have magick; then
    magick "$SRC" -background none \
        \( -clone 0 -resize 16x16 \) \( -clone 0 -resize 32x32 \) \
        \( -clone 0 -resize 48x48 \) \( -clone 0 -resize 64x64 \) \
        \( -clone 0 -resize 128x128 \) -delete 0 \
        -colors 256 art/breakpar.ico
    echo "art/breakpar.ico           $(wc -c < art/breakpar.ico) bytes"
else
    echo "skip: .ico needs ImageMagick (brew install imagemagick)" >&2
fi

# ---- site icon -------------------------------------------------------
mkdir -p promo-site/public/games/breakpar
if have magick; then
    magick "$SRC" -resize 512x512 -colors 192 -depth 8 \
        -define png:compression-level=9 promo-site/public/games/breakpar/icon-512.png
elif have sips; then
    sips -z 512 512 "$SRC" --out promo-site/public/games/breakpar/icon-512.png >/dev/null
fi
[ -f promo-site/public/games/breakpar/icon-512.png ] &&
    echo "promo-site icon-512.png    $(wc -c < promo-site/public/games/breakpar/icon-512.png) bytes"
