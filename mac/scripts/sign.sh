#!/bin/sh
# Signs an app bundle with a stable identity: scripts/sign.sh path/to/Leviathan.app
#
# Why: macOS remembers privacy permissions (microphone) per code signature. An ad-hoc signature
# changes on every build, so macOS forgets the permission and asks again after each rebuild.
# Signing every build with the same certificate keeps the permission.
#
# Order of preference:
#   1. $LEVIATHAN_SIGN_IDENTITY (e.g. "Developer ID Application: Your Name (TEAMID)")
#   2. A self-signed "Leviathan Local Signing" certificate kept in its own keychain at
#      ~/Library/Application Support/Leviathan/Signing/leviathan-signing.keychain-db
#      (created on first use; never touches your login keychain)
# Uses the hardened runtime with the microphone entitlement (required for notarization, harmless otherwise).
set -e
APP="$1"
[ -d "$APP" ] || { echo "usage: $0 path/to/App.app" >&2; exit 1; }
ENT="$(cd "$(dirname "$0")/.." && pwd)/Leviathan.entitlements"

if [ -n "$LEVIATHAN_SIGN_IDENTITY" ]; then
  codesign --force --options runtime --timestamp --entitlements "$ENT" -s "$LEVIATHAN_SIGN_IDENTITY" "$APP"
  echo "Signed with $LEVIATHAN_SIGN_IDENTITY"
  exit 0
fi

# CI without a real identity: a throwaway local certificate would differ on every run anyway, so it buys
# nothing over an ad-hoc signature there.
if [ -n "$CI" ]; then
  codesign --force --options runtime --entitlements "$ENT" -s - "$APP"
  echo "Signed ad-hoc (CI build without a Developer ID)"
  exit 0
fi

NAME="Leviathan Local Signing"
DIR="$HOME/Library/Application Support/Leviathan/Signing"
KC="$DIR/leviathan-signing.keychain-db"
PASS_FILE="$DIR/keychain-password"
OPENSSL=/usr/bin/openssl

mkdir -p "$DIR"
chmod 700 "$DIR"

if [ ! -f "$KC" ]; then
  # random password, only readable by you, used to unlock this one keychain
  head -c 24 /dev/urandom | base64 > "$PASS_FILE"
  chmod 600 "$PASS_FILE"
  PASS=$(cat "$PASS_FILE")
  TMP=$(mktemp -d)
  cat > "$TMP/cert.cnf" <<CNF
[req]
distinguished_name = dn
x509_extensions = ext
prompt = no
[dn]
CN = $NAME
[ext]
basicConstraints = critical,CA:false
keyUsage = critical,digitalSignature
extendedKeyUsage = critical,codeSigning
CNF
  $OPENSSL req -x509 -newkey rsa:2048 -nodes -days 3650 -config "$TMP/cert.cnf" \
    -keyout "$TMP/key.pem" -out "$TMP/cert.pem" >/dev/null
  $OPENSSL pkcs12 -export -inkey "$TMP/key.pem" -in "$TMP/cert.pem" -name "$NAME" \
    -out "$TMP/id.p12" -passout pass:"$PASS" >/dev/null
  security create-keychain -p "$PASS" "$KC" >/dev/null
  security set-keychain-settings "$KC"                      # no auto-lock timeout
  security unlock-keychain -p "$PASS" "$KC"
  security import "$TMP/id.p12" -k "$KC" -P "$PASS" -T /usr/bin/codesign >/dev/null
  security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$PASS" "$KC" >/dev/null
  rm -rf "$TMP"
fi

security unlock-keychain -p "$(cat "$PASS_FILE")" "$KC"
# sign by certificate hash: the self-signed cert isn't "trusted", so lookups by name are refused
HASH=$(security find-identity -p codesigning "$KC" | awk -v n="\"$NAME\"" 'index($0, n) { print $2; exit }')
[ -n "$HASH" ] || { echo "could not find the $NAME identity in $KC" >&2; exit 1; }
codesign --force --options runtime --entitlements "$ENT" --keychain "$KC" -s "$HASH" "$APP"
echo "Signed with $NAME (stable, so macOS keeps the microphone permission between builds)"
