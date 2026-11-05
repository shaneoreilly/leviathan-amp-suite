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
# bundled NAM captures (GPLv3, credits in shared/nam/captures/CREDITS.md)
mkdir -p "$APP/Contents/Resources/NAM" && cp ../shared/nam/captures/*.nam ../shared/nam/captures/CREDITS.md ../shared/nam/captures/COPYING "$APP/Contents/Resources/NAM/"
if [ -n "$VERSION" ]; then
  /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $VERSION" "$APP/Contents/Info.plist"
  /usr/libexec/PlistBuddy -c "Set :CFBundleVersion $VERSION" "$APP/Contents/Info.plist"
fi

FLAGS="-O -enforce-exclusivity=unchecked -parse-as-library -swift-version 5 \
  -framework SwiftUI -framework AVFoundation -framework CoreAudio -framework AudioToolbox -framework Accelerate"
# Neural Amp Modeler (C++), shared with the Windows app; built once per architecture and cached.
# Force-loaded: NAM registers its model types with static objects that nothing references directly.
NAM_LIB=.build-nam
FLAGS="$FLAGS -import-objc-header ../shared/nam/lev_nam.h -lc++"
if [ -n "$UNIVERSAL" ]; then
  ../shared/nam/build_lib.sh arm64 "$NAM_LIB/arm64"
  ../shared/nam/build_lib.sh x86_64 "$NAM_LIB/x86_64"
  TMP=$(mktemp -d)
  swiftc $FLAGS -target arm64-apple-macos13.0 Sources/*.swift -Xlinker -force_load -Xlinker "$NAM_LIB/arm64/liblevnam.a" -o "$TMP/arm64"
  swiftc $FLAGS -target x86_64-apple-macos13.0 Sources/*.swift -Xlinker -force_load -Xlinker "$NAM_LIB/x86_64/liblevnam.a" -o "$TMP/x86_64"
  lipo -create "$TMP/arm64" "$TMP/x86_64" -output "$APP/Contents/MacOS/Leviathan"
  rm -rf "$TMP"
else
  ../shared/nam/build_lib.sh "$(uname -m)" "$NAM_LIB/$(uname -m)"
  swiftc $FLAGS -target "$(uname -m)-apple-macos13.0" Sources/*.swift -Xlinker -force_load -Xlinker "$NAM_LIB/$(uname -m)/liblevnam.a" -o "$APP/Contents/MacOS/Leviathan"
fi

./scripts/sign.sh "$APP"
echo "Built $(pwd)/$APP"
