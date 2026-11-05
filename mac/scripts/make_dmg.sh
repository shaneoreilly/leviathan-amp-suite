#!/bin/sh
# Builds a universal Leviathan.app and packages it as dist/Leviathan-<version>.dmg
# (drag-to-Applications window).
#
#   scripts/make_dmg.sh 0.1.0
#
# Optional, for a release that opens without any Gatekeeper warning (needs an Apple Developer account):
#   LEVIATHAN_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)"   sign with Developer ID
#   NOTARY_PROFILE=leviathan       notarize with a profile saved by `xcrun notarytool store-credentials`
set -e
cd "$(dirname "$0")/.."
VERSION="${1:-$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' Info.plist)}"
VERSION="${VERSION#v}"
NAME="Leviathan-$VERSION"
OUT="$(pwd)/dist"
mkdir -p "$OUT"

UNIVERSAL=1 VERSION="$VERSION" ./build.sh

STAGE=$(mktemp -d)
cp -R Leviathan.app "$STAGE/"
ln -s /Applications "$STAGE/Applications"

DMG="$OUT/$NAME.dmg"
rm -f "$DMG"
hdiutil create -volname "Leviathan $VERSION" -srcfolder "$STAGE" -fs HFS+ -format UDZO -imagekey zlib-level=9 -ov "$DMG" >/dev/null
rm -rf "$STAGE"

if [ -n "$LEVIATHAN_SIGN_IDENTITY" ]; then
  codesign --force --timestamp -s "$LEVIATHAN_SIGN_IDENTITY" "$DMG"
  if [ -n "$NOTARY_PROFILE" ]; then
    xcrun notarytool submit "$DMG" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$DMG"
  fi
fi

shasum -a 256 "$DMG" | tee "$DMG.sha256"
echo "Packaged $DMG"
