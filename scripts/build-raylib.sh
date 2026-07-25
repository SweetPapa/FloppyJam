#!/usr/bin/env bash
#
# build-raylib.sh — clone raylib, strip out everything BREAK PAR does not use,
# and build a static library.
#
#   ./scripts/build-raylib.sh --out build/raylib-host
#   ./scripts/build-raylib.sh --out build/raylib-arm64  --arch arm64
#   ./scripts/build-raylib.sh --out build/raylib-mingw  --mingw
#
# Why this exists: a full static raylib pushes the Windows executable to
# 1,837,056 bytes, which is over the 1,474,560-byte floppy ceiling the game is
# designed to. Section 13 of the spec calls for exactly this — "compile raylib
# from source with unused modules disabled" — and the biggest wins are the file
# format decoders. BREAK PAR loads no models, no fonts and no audio files: every
# sound is synthesised at runtime, so dr_mp3, stb_vorbis, jar_xm, dr_flac, cgltf
# and m3d are all dead weight linked into the binary.
#
# PNG stays because TakeScreenshot needs it for `--tour`, the renderer smoke test.
#
set -euo pipefail

TAG="${RAYLIB_TAG:-5.5}"
OUT=""
ARCH=""
MINGW=0
JOBS="$( (getconf _NPROCESSORS_ONLN || sysctl -n hw.ncpu || echo 4) 2>/dev/null )"

while [ $# -gt 0 ]; do
    case "$1" in
        --out)   OUT="${2:?--out needs a path}"; shift 2 ;;
        --arch)  ARCH="${2:?--arch needs a value}"; shift 2 ;;
        --mingw) MINGW=1; shift ;;
        --tag)   TAG="${2:?}"; shift 2 ;;
        -h|--help) sed -n '2,20p' "$0" | sed 's/^#//;s/^ //'; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 1 ;;
    esac
done
[ -n "$OUT" ] || { echo "error: --out is required" >&2; exit 1; }

mkdir -p "$OUT/lib" "$OUT/include"
OUT="$(cd "$OUT" && pwd)"
SRCDIR="$(mktemp -d)/raylib"

git clone --depth 1 --branch "$TAG" https://github.com/raysan5/raylib.git "$SRCDIR" 2>&1 | tail -1

# ---- trim config.h --------------------------------------------------
# Flipping the defines in place rather than going through EXTERNAL_CONFIG_FLAGS:
# that macro means supplying the ENTIRE config yourself, and any raylib default
# that changes between versions silently becomes our problem.
CFG="$SRCDIR/src/config.h"
off() {
    for f in "$@"; do
        sed -i.bak -E "s|^([[:space:]]*)#define[[:space:]]+$f[[:space:]]+1|\\1//#define $f 1|" "$CFG"
    done
    rm -f "$CFG.bak"
}

# every sound in the game is generated; no audio file is ever opened
off SUPPORT_FILEFORMAT_WAV SUPPORT_FILEFORMAT_OGG SUPPORT_FILEFORMAT_MP3 \
    SUPPORT_FILEFORMAT_QOA SUPPORT_FILEFORMAT_FLAC SUPPORT_FILEFORMAT_XM \
    SUPPORT_FILEFORMAT_MOD
# every mesh is drawn immediate-mode; no model is ever loaded
off SUPPORT_FILEFORMAT_OBJ SUPPORT_FILEFORMAT_MTL SUPPORT_FILEFORMAT_IQM \
    SUPPORT_FILEFORMAT_GLTF SUPPORT_FILEFORMAT_VOX SUPPORT_FILEFORMAT_M3D
# raylib's built-in default font is embedded, not parsed from a file
off SUPPORT_FILEFORMAT_TTF SUPPORT_FILEFORMAT_FNT
# Image decoders: PNG has to stay. raylib 5.5's LoadImageFromMemory calls into
# stb_image unconditionally, so disabling every stb format leaves an undefined
# reference and raylib itself will not compile. PNG is also what TakeScreenshot
# needs for `--tour`. The rest go.
off SUPPORT_FILEFORMAT_BMP SUPPORT_FILEFORMAT_TGA SUPPORT_FILEFORMAT_JPG \
    SUPPORT_FILEFORMAT_GIF SUPPORT_FILEFORMAT_QOI SUPPORT_FILEFORMAT_PSD \
    SUPPORT_FILEFORMAT_DDS SUPPORT_FILEFORMAT_HDR SUPPORT_FILEFORMAT_PIC \
    SUPPORT_FILEFORMAT_PNM SUPPORT_FILEFORMAT_KTX SUPPORT_FILEFORMAT_ASTC \
    SUPPORT_FILEFORMAT_PKM SUPPORT_FILEFORMAT_PVR
# the game owns its own camera and never asks raylib for gestures or the
# clipboard (clipboard images would drag SUPPORT_FILEFORMAT_BMP back in)
off SUPPORT_CAMERA_SYSTEM SUPPORT_GESTURES_SYSTEM SUPPORT_MOUSE_GESTURES \
    SUPPORT_RPRAND_GENERATOR SUPPORT_CLIPBOARD_IMAGE

echo "--- disabled in config.h ---"
grep -c '^//#define SUPPORT' "$CFG" || true

# ---- build ----------------------------------------------------------
MAKE_ARGS=(PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC RAYLIB_BUILD_MODE=RELEASE)
CUSTOM=""
if [ "$MINGW" = 1 ]; then
    MAKE_ARGS+=(OS=Windows_NT CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar)
elif [ -n "$ARCH" ]; then
    CUSTOM="-arch $ARCH -mmacosx-version-min=11.0"
fi
[ -n "$CUSTOM" ] && MAKE_ARGS+=(CUSTOM_CFLAGS="$CUSTOM")

make -C "$SRCDIR/src" "${MAKE_ARGS[@]}" -j"$JOBS" >/dev/null

cp "$SRCDIR/src/libraylib.a" "$OUT/lib/"
cp "$SRCDIR/src/raylib.h" "$SRCDIR/src/raymath.h" "$SRCDIR/src/rlgl.h" "$OUT/include/"
rm -rf "$(dirname "$SRCDIR")"

printf 'raylib %s -> %s/lib/libraylib.a (%s bytes)\n' \
    "$TAG" "$OUT" "$(wc -c < "$OUT/lib/libraylib.a")"
