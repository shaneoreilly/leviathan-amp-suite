#!/bin/sh
# Builds NeuralAmpModelerCore + Leviathan's bridge as a static library for one macOS architecture.
#   shared/nam/build_lib.sh <arm64|x86_64> <out-dir>      -> <out-dir>/liblevnam.a  (cached per NAM commit)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ARCH="$1"; OUT="$2"
"$HERE/fetch_nam.sh"
N="$HERE/deps/NeuralAmpModelerCore"
STAMP="$OUT/.stamp-$(cd "$N" && git rev-parse HEAD)-$(shasum "$HERE/lev_nam.cpp" | cut -c1-12)"
[ -f "$OUT/liblevnam.a" ] && [ -f "$STAMP" ] && exit 0
rm -rf "$OUT"; mkdir -p "$OUT/obj"
echo "Building NAM core for $ARCH (first build only)..."
for f in "$HERE/lev_nam.cpp" "$N"/NAM/*.cpp "$N"/NAM/*/*.cpp; do
  o="$OUT/obj/$(echo "$f" | sed "s|$N/||; s|$HERE/||; s|/|_|g").o"
  clang++ -std=c++20 -O3 -DNDEBUG -DNAM_SAMPLE_FLOAT -target "$ARCH-apple-macos13.0" \
    -I"$N" -I"$N/Dependencies/eigen" -I"$N/Dependencies/nlohmann" -w -c "$f" -o "$o" &
done
wait
ar rcs "$OUT/liblevnam.a" "$OUT"/obj/*.o
touch "$STAMP"
