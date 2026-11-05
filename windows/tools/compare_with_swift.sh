#!/bin/sh
# Renders the same test scenarios through the Swift engine (mac/Sources/DSP.swift) and the C++ engine
# (windows/Source/Dsp.cpp) and reports how far apart they are. Run on macOS from the repo root:
#
#   windows/tools/compare_with_swift.sh
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/leviathan-dsp-compare"
mkdir -p "$OUT"

# both engines link the same NAM library (built by the Mac build), and run NAM's example model
ARCH="$(uname -m)"
"$ROOT/shared/nam/build_lib.sh" "$ARCH" "$ROOT/mac/.build-nam/$ARCH"
NAMLIB="$ROOT/mac/.build-nam/$ARCH/liblevnam.a"
MODEL="$ROOT/shared/nam/deps/NeuralAmpModelerCore/example_models/wavenet.nam"
swiftc -O -parse-as-library -import-objc-header "$ROOT/shared/nam/lev_nam.h" "$ROOT/mac/Sources/DSP.swift" "$ROOT/windows/tools/swift_render.swift" -Xlinker -force_load -Xlinker "$NAMLIB" -lc++ -o "$OUT/swift_render"
c++ -std=c++20 -O2 -ffp-contract=off -DNAM_SAMPLE_FLOAT -I"$ROOT/windows/Source" -I"$ROOT/shared/nam" "$ROOT/windows/tools/dsp_render.cpp" "$ROOT/windows/Source/Dsp.cpp" -Wl,-force_load,"$NAMLIB" -o "$OUT/dsp_render"
"$OUT/swift_render" "$OUT" "$MODEL"
"$OUT/dsp_render" "$OUT" "$MODEL"

python3 - "$OUT" <<'EOF'
import sys, struct, math, glob, os
out = sys.argv[1]
worst = 0.0
for sw in sorted(glob.glob(os.path.join(out, "*.swift.f32"))):
    name = os.path.basename(sw)[:-len(".swift.f32")]
    a = open(sw, "rb").read(); b = open(os.path.join(out, name + ".cpp.f32"), "rb").read()
    n = len(a) // 4
    x = struct.unpack(f"{n}f", a); y = struct.unpack(f"{n}f", b)
    sig = sum(v * v for v in x) / n
    err = sum((p - q) ** 2 for p, q in zip(x, y)) / n
    peak = max(abs(p - q) for p, q in zip(x, y))
    snr = 10 * math.log10(sig / err) if err > 0 else float("inf")
    worst = max(worst, -snr if snr != float("inf") else -999)
    print(f"{name:12s} signal {10 * math.log10(sig + 1e-30):6.1f} dB   difference {snr:6.1f} dB below signal   max sample diff {peak:.2e}")
sys.exit(0 if worst < -60 else 1)
EOF
