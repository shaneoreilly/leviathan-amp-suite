#!/bin/sh
# Renders the same test scenarios through the Swift engine (mac/Sources/DSP.swift) and the C++ engine
# (windows/Source/Dsp.cpp) and reports how far apart they are. Run on macOS from the repo root:
#
#   windows/tools/compare_with_swift.sh
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/leviathan-dsp-compare"
mkdir -p "$OUT"

swiftc -O -parse-as-library "$ROOT/mac/Sources/DSP.swift" "$ROOT/windows/tools/swift_render.swift" -o "$OUT/swift_render"
c++ -std=c++20 -O2 -ffp-contract=off -I"$ROOT/windows/Source" "$ROOT/windows/tools/dsp_render.cpp" "$ROOT/windows/Source/Dsp.cpp" -o "$OUT/dsp_render"
"$OUT/swift_render" "$OUT"
"$OUT/dsp_render" "$OUT"

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
