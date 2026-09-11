#!/usr/bin/env bash
set -euo pipefail
: "${APPLE_CERT_P12:?Signing certificate missing}" "${APPLE_CERT_PASSWORD:?Certificate password missing}"
SIGNING_KEYCHAIN="$RUNNER_TEMP/maglava-signing.keychain-db"
SIGNING_PASSWORD=$(openssl rand -hex 24)
echo "::add-mask::$SIGNING_PASSWORD"
security create-keychain -p "$SIGNING_PASSWORD" "$SIGNING_KEYCHAIN"
security set-keychain-settings -lut 21600 "$SIGNING_KEYCHAIN"
security unlock-keychain -p "$SIGNING_PASSWORD" "$SIGNING_KEYCHAIN"
printf '%s' "$APPLE_CERT_P12" | base64 --decode > "$RUNNER_TEMP/maglava-cert.p12"
security import "$RUNNER_TEMP/maglava-cert.p12" -k "$SIGNING_KEYCHAIN" -P "$APPLE_CERT_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$SIGNING_PASSWORD" "$SIGNING_KEYCHAIN" >/dev/null
security list-keychains -d user -s "$SIGNING_KEYCHAIN" /Library/Keychains/System.keychain
rm -f "$RUNNER_TEMP/maglava-cert.p12"
echo "MAGLAVA_SIGNING_KEYCHAIN=$SIGNING_KEYCHAIN" >> "$GITHUB_ENV"
