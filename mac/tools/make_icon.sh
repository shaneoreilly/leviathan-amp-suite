#!/bin/sh
# Regenerates Resources/AppIcon.icns and docs/logo.svg from Sources/Logo.swift.
set -e
cd "$(dirname "$0")/.."
TMP=$(mktemp -d)
cp tools/make_icon.swift "$TMP/main.swift"
swiftc -O -target arm64-apple-macos13.0 Sources/Logo.swift "$TMP/main.swift" -o "$TMP/make_icon"
"$TMP/make_icon" "$TMP"
mkdir -p Resources ../docs
iconutil -c icns "$TMP/AppIcon.iconset" -o Resources/AppIcon.icns
cp "$TMP/logo.svg" ../docs/logo.svg
cp "$TMP/icon-1024.png" ../docs/icon.png
rm -rf "$TMP"
echo "Updated Resources/AppIcon.icns, docs/logo.svg, docs/icon.png"
