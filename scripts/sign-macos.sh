#!/usr/bin/env bash
#
# sign-macos.sh — sign, notarize and staple a macOS build.
#
#   ./scripts/sign-macos.sh v5/breakpar
#   ./scripts/sign-macos.sh v5/breakpar --identity "Some Other Org"
#   ./scripts/sign-macos.sh v5/breakpar --dmg
#   ./scripts/sign-macos.sh SomeApp.app --dry-run
#
# Takes a bare Mach-O executable, a .app, a .dmg or a .pkg. A bare executable
# is wrapped into a .app first, because a bare binary cannot be stapled — the
# notarisation ticket has nowhere to live, so every launch needs the machine to
# be online and reach Apple. Wrapping is the difference between "notarised" and
# "actually opens on someone else's Mac".
#
# Credentials come from the login keychain and are never printed.
#
# SIGNING needs a Developer ID Application certificate and its private key. There
# is no way around that — an App Store Connect API key cannot sign code, it can
# only talk to Apple's services.
#
# NOTARISATION takes either, and prefers the API key:
#
#   1. App Store Connect API key (key id + issuer + .p8). No password to rotate,
#      revocable from the ASC console on its own, and the same three values work
#      unchanged in CI. This is the default when a key is available.
#   2. Apple ID + app-specific password, kept as a generic keychain item. The
#      fallback, used when no API key is configured.
#
# The .p8 is read from the keychain if it is there, otherwise from a file:
#
#   security add-generic-password -s asc-mcp -a <KEY_ID> -w "$(cat AuthKey_X.p8)"
#
# For the app-specific password route instead (appleid.apple.com -> Sign-In and
# Security -> App-Specific Passwords):
#
#   security add-generic-password -s spt-notary -a you@example.com -w
#
set -euo pipefail

# ---- account preset --------------------------------------------------
# The default signing account. Anything else is selected by passing the
# certificate name (or a unique part of it) to --identity, so a machine that
# holds Developer IDs for other organisations does not have to name any of them
# in a public repo.
# name       keychain service   apple id                team id      cert match
PRESET_SPT=( "spt-notary" "fterry24v2@gmail.com" "6Y5SZ2K5XY" "Developer ID Application: Forrester Terry" )

IDENTITY_PRESET="spt"
BUNDLE_ID=""
APP_NAME=""
MAKE_DMG=0
NO_BUNDLE=0
DRY_RUN=0
TARGET=""

# explicit overrides — any of these wins over the preset
OVERRIDE_CERT=""
OVERRIDE_APPLE_ID="${APPLE_ID:-}"
OVERRIDE_TEAM_ID="${APPLE_TEAM_ID:-}"
OVERRIDE_KEYCHAIN_SERVICE="${NOTARY_KEYCHAIN_SERVICE:-}"

# App Store Connect API key: the preferred notarisation credential
ASC_KEY_ID="${ASC_KEY_ID:-45S6SMU5GY}"
ASC_ISSUER_ID="${ASC_ISSUER_ID:-08a7930e-9034-41cd-ab35-41afa2b19813}"
ASC_KEY_SERVICE="${ASC_KEY_SERVICE:-asc-mcp}"
ASC_KEY_PATH="${ASC_KEY_PATH:-}"      # a .p8 on disk; keychain is tried first
ASC_KEY_B64="${ASC_KEY_B64:-}"        # base64 of the .p8, for CI
NO_ASC=0

die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
info() { printf '\033[36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarn:\033[0m %s\n' "$*" >&2; }

usage() {
    sed -n '2,30p' "$0" | sed 's/^#//;s/^ //'
    cat <<'EOF'

Options:
  --identity <spt|NAME>          signing account, or any Developer ID name
                                 or unique substring of one (default: spt)
  --bundle-id <id>               CFBundleIdentifier (default: com.sweetpapa.<name>)
  --app-name <Name>              .app display name (default: derived from the binary)
  --dmg                          also build and notarize a .dmg for distribution
  --no-bundle                    sign the bare executable, do not wrap it in a .app
                                 (cannot be stapled — online check on every launch)
  --dry-run                      sign and verify locally, do not submit to Apple
  --no-asc-key                   ignore the API key, use Apple ID + app password
  -h, --help                     this text

Notarisation uses an App Store Connect API key when one is available (no
password, revocable on its own) and falls back to an Apple ID app-specific
password otherwise.

Environment overrides:
  APPLE_ID, APPLE_TEAM_ID, NOTARY_KEYCHAIN_SERVICE, SIGN_IDENTITY
  ASC_KEY_ID, ASC_ISSUER_ID, ASC_KEY_SERVICE, ASC_KEY_PATH, ASC_KEY_B64
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --identity)  IDENTITY_PRESET="${2:?--identity needs a value}"; shift 2 ;;
        --bundle-id) BUNDLE_ID="${2:?--bundle-id needs a value}"; shift 2 ;;
        --app-name)  APP_NAME="${2:?--app-name needs a value}"; shift 2 ;;
        --dmg)       MAKE_DMG=1; shift ;;
        --no-bundle) NO_BUNDLE=1; shift ;;
        --dry-run)   DRY_RUN=1; shift ;;
        --no-asc-key) NO_ASC=1; shift ;;
        -h|--help)   usage; exit 0 ;;
        -*)          die "unknown option: $1" ;;
        *)           [ -z "$TARGET" ] || die "only one target, got '$TARGET' and '$1'"
                     TARGET="$1"; shift ;;
    esac
done

[ -n "$TARGET" ] || { usage; exit 1; }
[ -e "$TARGET" ] || die "no such file: $TARGET"
[ "$(uname -s)" = "Darwin" ] || die "this script only runs on macOS"
command -v codesign >/dev/null || die "codesign not found — install the Xcode command line tools"
command -v xcrun    >/dev/null || die "xcrun not found — install the Xcode command line tools"

# ---- resolve the account --------------------------------------------
case "$IDENTITY_PRESET" in
    spt) P=( "${PRESET_SPT[@]}" ) ;;
    # anything else is matched against the certificate name in the keychain;
    # the team id is then read out of the certificate rather than hardcoded
    *)   P=( "" "" "" "$IDENTITY_PRESET" ) ;;
esac
KEYCHAIN_SERVICE="${OVERRIDE_KEYCHAIN_SERVICE:-${P[0]}}"
APPLE_ID_USED="${OVERRIDE_APPLE_ID:-${P[1]}}"
TEAM_ID="${OVERRIDE_TEAM_ID:-${P[2]}}"
CERT_MATCH="${SIGN_IDENTITY:-${OVERRIDE_CERT:-${P[3]}}}"

# Resolve the cert to a SHA-1 hash. Matching on the hash rather than the name
# means a machine with two certs from the same team cannot silently pick the
# wrong one, and the failure when a cert is missing is immediate and legible.
CERT_LINE="$(security find-identity -v -p codesigning 2>/dev/null | grep -F "$CERT_MATCH" | head -1 || true)"
[ -n "$CERT_LINE" ] || die "no codesigning identity matching '$CERT_MATCH'
  available identities:
$(security find-identity -v -p codesigning | sed 's/^/  /')"
CERT_HASH="$(printf '%s' "$CERT_LINE" | awk '{print $2}')"
CERT_NAME="$(printf '%s' "$CERT_LINE" | sed 's/.*"\(.*\)".*/\1/')"

if [ -z "$TEAM_ID" ]; then
    # Developer ID names end in "(TEAMID)" — pull it out rather than ask.
    TEAM_ID="$(printf '%s' "$CERT_NAME" | sed -n 's/.*(\([A-Z0-9]\{10\}\))$/\1/p')"
fi

info "identity   $CERT_NAME"
info "team       ${TEAM_ID:-<unknown>}"
[ "$DRY_RUN" = 1 ] || info "apple id   ${APPLE_ID_USED:-<unset>}"

# ---- wrap a bare executable into a .app ------------------------------
TARGET="${TARGET%/}"
BASENAME="$(basename "$TARGET")"
WORKDIR="$(cd "$(dirname "$TARGET")" && pwd)"
ABS="$WORKDIR/$BASENAME"

is_macho() { file -b "$1" 2>/dev/null | grep -q "Mach-O"; }

case "$BASENAME" in
    *.app|*.dmg|*.pkg) SIGN_TARGET="$ABS" ;;
    *)
        if [ "$NO_BUNDLE" = 1 ] || ! is_macho "$ABS"; then
            SIGN_TARGET="$ABS"
        else
            APP_NAME="${APP_NAME:-$BASENAME}"
            APP="$WORKDIR/$APP_NAME.app"
            # bundle ids are alphanumerics, dots and hyphens only
            BUNDLE_ID="${BUNDLE_ID:-com.sweetpapa.$(printf '%s' "$BASENAME" \
                | tr '[:upper:]' '[:lower:]' | tr -c 'a-z0-9.\n' '-' | sed 's/-\{2,\}/-/g;s/^-//;s/-$//')}"
            info "wrapping   $BASENAME -> $APP_NAME.app ($BUNDLE_ID)"
            rm -rf "$APP"
            mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
            cp "$ABS" "$APP/Contents/MacOS/$APP_NAME"
            chmod +x "$APP/Contents/MacOS/$APP_NAME"
            # The icon lives in the bundle, so it has to be copied in before
            # signing — dropping a file into Resources afterwards invalidates
            # the signature and Gatekeeper rejects the whole thing.
            ICON_SRC="${ICON:-$(cd "$(dirname "$0")/.." && pwd)/art/BreakPar.icns}"
            if [ -f "$ICON_SRC" ]; then
                cp "$ICON_SRC" "$APP/Contents/Resources/AppIcon.icns"
                ICON_PLIST='    <key>CFBundleIconFile</key>           <string>AppIcon</string>'
                info "icon       $(basename "$ICON_SRC")"
            else
                ICON_PLIST=''
                warn "no icon at $ICON_SRC — the bundle will use the generic one"
            fi
            cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>               <string>$APP_NAME</string>
    <key>CFBundleDisplayName</key>        <string>$APP_NAME</string>
    <key>CFBundleExecutable</key>         <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>         <string>$BUNDLE_ID</string>
$ICON_PLIST
    <key>CFBundlePackageType</key>        <string>APPL</string>
    <key>CFBundleShortVersionString</key> <string>${VERSION:-1.0}</string>
    <key>CFBundleVersion</key>            <string>${VERSION:-1.0}</string>
    <key>LSMinimumSystemVersion</key>     <string>11.0</string>
    <key>NSHighResolutionCapable</key>    <true/>
    <key>NSHumanReadableCopyright</key>   <string>© $(date +%Y) Forrester Terry</string>
</dict>
</plist>
PLIST
            printf 'APPL????' > "$APP/Contents/PkgInfo"
            SIGN_TARGET="$APP"
        fi
        ;;
esac

# ---- sign ------------------------------------------------------------
# Hardened runtime and a secure timestamp are both hard requirements for
# notarisation; without either, Apple rejects the submission rather than
# warning about it.
info "signing    $(basename "$SIGN_TARGET")"
codesign --force --deep --options runtime --timestamp \
         --sign "$CERT_HASH" "$SIGN_TARGET"
codesign --verify --deep --strict --verbose=2 "$SIGN_TARGET" 2>&1 | sed 's/^/           /'

if [ "$DRY_RUN" = 1 ]; then
    info "dry run — signed and verified, nothing submitted to Apple"
    codesign -dv --verbose=4 "$SIGN_TARGET" 2>&1 | grep -E "Authority|TeamIdentifier|Timestamp|flags" | sed 's/^/           /' || true
    exit 0
fi

# ---- notarize --------------------------------------------------------
[ -n "$TEAM_ID" ] || die "could not determine the team id — pass APPLE_TEAM_ID"

# Resolve one notarisation credential. The API key wins when we can find one.
NOTARY_ARGS=()
ASC_TMP=""
cleanup_key() { [ -n "$ASC_TMP" ] && rm -rf "$(dirname "$ASC_TMP")"; }
trap cleanup_key EXIT

if [ "$NO_ASC" = 0 ]; then
    ASC_TMP="$(mktemp -d)/AuthKey.p8"
    ( umask 077; : > "$ASC_TMP" )
    if [ -n "$ASC_KEY_B64" ]; then
        printf '%s' "$ASC_KEY_B64" | base64 --decode > "$ASC_TMP" 2>/dev/null || : > "$ASC_TMP"
    elif [ -n "$ASC_KEY_PATH" ] && [ -f "$ASC_KEY_PATH" ]; then
        cat "$ASC_KEY_PATH" > "$ASC_TMP"
    else
        # `security -w` hands back hex whenever the blob is not clean UTF-8, and a
        # PEM stored as raw data usually is not, so decode when it looks like hex.
        RAW="$(security find-generic-password -s "$ASC_KEY_SERVICE" -a "$ASC_KEY_ID" -w 2>/dev/null || true)"
        if [ -n "$RAW" ]; then
            case "$RAW" in
                *BEGIN*) printf '%s\n' "$RAW" > "$ASC_TMP" ;;
                *)       printf '%s' "$RAW" | xxd -r -p > "$ASC_TMP" 2>/dev/null || : > "$ASC_TMP" ;;
            esac
        fi
    fi
    if grep -q "BEGIN PRIVATE KEY" "$ASC_TMP" 2>/dev/null; then
        info "notary     App Store Connect API key $ASC_KEY_ID"
        NOTARY_ARGS=( --key "$ASC_TMP" --key-id "$ASC_KEY_ID" --issuer "$ASC_ISSUER_ID" )
    else
        cleanup_key; ASC_TMP=""
    fi
fi

if [ ${#NOTARY_ARGS[@]} -eq 0 ]; then
    [ -n "$KEYCHAIN_SERVICE" ] || die "no keychain service for '$IDENTITY_PRESET' — pass NOTARY_KEYCHAIN_SERVICE"
    [ -n "$APPLE_ID_USED" ]    || die "no Apple ID for '$IDENTITY_PRESET' — pass APPLE_ID"
    NOTARY_PASSWORD="$(security find-generic-password -s "$KEYCHAIN_SERVICE" -a "$APPLE_ID_USED" -w 2>/dev/null || true)"
    [ -n "$NOTARY_PASSWORD" ] || die "no notarisation credential found.
  either store an App Store Connect API key:
    security add-generic-password -s $ASC_KEY_SERVICE -a <KEY_ID> -w \"\$(cat AuthKey_<KEY_ID>.p8)\"
  or an app-specific password for $APPLE_ID_USED:
    security add-generic-password -s $KEYCHAIN_SERVICE -a $APPLE_ID_USED -w"
    info "notary     Apple ID $APPLE_ID_USED (app-specific password)"
    NOTARY_ARGS=( --apple-id "$APPLE_ID_USED" --team-id "$TEAM_ID" --password "$NOTARY_PASSWORD" )
fi

submit() {
    # $1 = path to submit, $2 = human label
    info "notarizing $2 (this usually takes a minute or two)"
    xcrun notarytool submit "$1" "${NOTARY_ARGS[@]}" --wait 2>&1 | sed 's/^/           /'
}

case "$SIGN_TARGET" in
    *.dmg|*.pkg)
        submit "$SIGN_TARGET" "$(basename "$SIGN_TARGET")"
        ;;
    *)
        # notarytool will not take a .app or a bare binary directly; it wants a
        # container. ditto --keepParent preserves the bundle structure.
        ZIP="$(mktemp -d)/$(basename "$SIGN_TARGET").zip"
        ditto -c -k --keepParent "$SIGN_TARGET" "$ZIP"
        submit "$ZIP" "$(basename "$SIGN_TARGET")"
        rm -rf "$(dirname "$ZIP")"
        ;;
esac

# ---- staple ----------------------------------------------------------
case "$SIGN_TARGET" in
    *.app|*.dmg|*.pkg)
        info "stapling   $(basename "$SIGN_TARGET")"
        xcrun stapler staple "$SIGN_TARGET" 2>&1 | sed 's/^/           /'
        xcrun stapler validate "$SIGN_TARGET" 2>&1 | sed 's/^/           /'
        ;;
    *)
        warn "a bare executable cannot be stapled; the ticket is only available online.
      re-run without --no-bundle to get a stapled .app."
        ;;
esac

# ---- optional dmg ----------------------------------------------------
if [ "$MAKE_DMG" = 1 ]; then
    case "$SIGN_TARGET" in
        *.app)
            DMG="${SIGN_TARGET%.app}.dmg"
            info "building   $(basename "$DMG")"
            STAGE="$(mktemp -d)"
            cp -R "$SIGN_TARGET" "$STAGE/"
            ln -s /Applications "$STAGE/Applications"
            rm -f "$DMG"
            hdiutil create -volname "$(basename "${SIGN_TARGET%.app}")" \
                -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
            rm -rf "$STAGE"
            codesign --force --timestamp --sign "$CERT_HASH" "$DMG"
            submit "$DMG" "$(basename "$DMG")"
            xcrun stapler staple "$DMG" 2>&1 | sed 's/^/           /'
            info "dmg        $DMG"
            ;;
        *) warn "--dmg only applies to a .app; skipping" ;;
    esac
fi

# ---- final verdict ---------------------------------------------------
info "gatekeeper assessment"
spctl --assess --type execute --verbose=4 "$SIGN_TARGET" 2>&1 | sed 's/^/           /' || \
    warn "spctl was unhappy — check the output above"

info "done: $SIGN_TARGET"
