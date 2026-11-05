#!/bin/sh
# Builds Leviathan.app with the command line tools (no Xcode project needed).
#   ./build.sh               native architecture, for development
#   UNIVERSAL=1 ./build.sh   Apple silicon + Intel, for releases
#   VERSION=1.2.0 ./build.sh sets the version shown in Finder / About
set -e
cd "$(dirname "$0")"
APP=Leviathan.app
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp Info.plist "$APP/Contents/Info.plist"
cp Resources/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"
if [ -n "$VERSION" ]; then
  /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $VERSION" "$APP/Contents/Info.plist"
  /usr/libexec/PlistBuddy -c "Set :CFBundleVersion $VERSION" "$APP/Contents/Info.plist"
fi

FLAGS="-O -enforce-exclusivity=unchecked -parse-as-library -swift-version 5 \
  -framework SwiftUI -framework AVFoundation -framework CoreAudio -framework AudioToolbox -framework Accelerate"
if [ -n "$UNIVERSAL" ]; then
  TMP=$(mktemp -d)
  swiftc $FLAGS -target arm64-apple-macos13.0 Sources/*.swift -o "$TMP/arm64"
  swiftc $FLAGS -target x86_64-apple-macos13.0 Sources/*.swift -o "$TMP/x86_64"
  lipo -create "$TMP/arm64" "$TMP/x86_64" -output "$APP/Contents/MacOS/Leviathan"
  rm -rf "$TMP"
else
  swiftc $FLAGS -target "$(uname -m)-apple-macos13.0" Sources/*.swift -o "$APP/Contents/MacOS/Leviathan"
fi

./scripts/sign.sh "$APP"
echo "Built $(pwd)/$APP"
