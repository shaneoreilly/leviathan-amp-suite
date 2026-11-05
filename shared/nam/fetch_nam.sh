#!/bin/sh
# Fetches NeuralAmpModelerCore (MIT) at a pinned release, with Eigen (MPL-2.0), into shared/nam/deps (not in git).
set -e
cd "$(dirname "$0")"
NAM_TAG=v0.6.0
NAM_COMMIT=0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842
if [ -f deps/NeuralAmpModelerCore/.leviathan-$NAM_COMMIT ]; then exit 0; fi
rm -rf deps/NeuralAmpModelerCore
mkdir -p deps
git -c advice.detachedHead=false clone -q --depth 1 --branch "$NAM_TAG" https://github.com/sdatkinson/NeuralAmpModelerCore.git deps/NeuralAmpModelerCore
cd deps/NeuralAmpModelerCore
[ "$(git rev-parse HEAD)" = "$NAM_COMMIT" ] || { echo "NAM $NAM_TAG is not the expected commit"; exit 1; }
git submodule update -q --init --depth 1 Dependencies/eigen
touch .leviathan-$NAM_COMMIT
