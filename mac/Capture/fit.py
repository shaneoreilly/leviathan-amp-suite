#!/usr/bin/env python3
"""Fit Leviathan captures from (dry test signal, wet recording) pairs.

Usage:
  python3 fit.py                      # fit every wet/*.wav not yet captured
  python3 fit.py "wet/My Preset.wav"  # fit specific files (refits)
  python3 fit.py --force              # refit everything

Model (must match CaptureAmp in DSP.swift):
  x -> HP(hp) -> lowShelf(250 Hz, ls) -> peak(pk_f, q .7, pk) -> highShelf(3 kHz, hs)
    -> [oversampled]  v = tanh(k1*u + b1) - tanh(b1) -> LP(mid) -> w = tanh(k2*v + b2) - tanh(b2)
    -> FIR ir (tone stack + cab + output level, solved by least squares)

Accuracy is reported as ESR (error-to-signal ratio, pre-emphasised, lower is better) on held-out audio:
  < 0.01 indistinguishable for most players, < 0.05 very close, > 0.15 audibly different.
"""
import glob
import json
import os
import sys
import time

import numpy as np
from scipy.io import wavfile
from scipy.linalg import solve_toeplitz
from scipy.optimize import minimize
from scipy.signal import fftconvolve, lfilter, resample_poly

SR = 44100
BASE = os.path.expanduser("~/Music/Leviathan/Capture")
DRY = os.path.join(BASE, "leviathan_capture_test.wav")
OUTDIR = os.path.expanduser("~/Library/Application Support/Leviathan/Captures")
PRE = 16          # samples of headroom before the IR peak
OS = 2            # oversampling used while fitting (runtime uses 4x)

# name, low, high, log-scaled
SPACE = [
    ("hp", 20, 400, True),
    ("ls", -15, 15, False),
    ("pk_f", 250, 3500, True),
    ("pk", -15, 15, False),
    ("hs", -15, 15, False),
    ("k1", 0.05, 400, True),
    ("b1", -0.6, 0.6, False),
    ("mid", 800, 16000, True),
    ("k2", 0.05, 60, True),
    ("b2", -0.6, 0.6, False),
]


# ---------- filters (RBJ cookbook, identical to Biquad in DSP.swift) ----------

def biquad(kind, f, sr, q=0.7071, gain_db=0.0):
    f = min(max(f, 5), sr * 0.48)
    w = 2 * np.pi * f / sr
    cw, sw = np.cos(w), np.sin(w)
    al = sw / (2 * q)
    A = 10 ** (gain_db / 40)
    if kind == "lp":
        b = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2]; a = [1 + al, -2 * cw, 1 - al]
    elif kind == "hp":
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]; a = [1 + al, -2 * cw, 1 - al]
    elif kind == "peak":
        b = [1 + al * A, -2 * cw, 1 - al * A]; a = [1 + al / A, -2 * cw, 1 - al / A]
    elif kind == "lowShelf":
        s = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) - (A - 1) * cw + s), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - s)]
        a = [(A + 1) + (A - 1) * cw + s, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - s]
    elif kind == "highShelf":
        s = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) + (A - 1) * cw + s), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - s)]
        a = [(A + 1) - (A - 1) * cw + s, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - s]
    else:
        raise ValueError(kind)
    b = np.array(b) / a[0]
    a = np.array(a) / a[0]
    return b, a


def front(x, p):
    """Everything before the FIR, at base rate."""
    for b, a in (biquad("hp", p["hp"], SR, 0.7071), biquad("lowShelf", 250, SR, gain_db=p["ls"]),
                 biquad("peak", p["pk_f"], SR, 0.7, p["pk"]), biquad("highShelf", 3000, SR, gain_db=p["hs"])):
        x = lfilter(b, a, x)
    u = resample_poly(x, OS, 1)
    v = np.tanh(p["k1"] * u + p["b1"]) - np.tanh(p["b1"])
    b, a = biquad("lp", p["mid"], SR * OS)
    v = lfilter(b, a, v)
    w = np.tanh(p["k2"] * v + p["b2"]) - np.tanh(p["b2"])
    return resample_poly(w, 1, OS)


def emph(x):
    return lfilter([1, -0.85], [1], x)


def solve_ir(z, y, n):
    """Least-squares FIR h (length n) minimising |y - z*h|^2."""
    m = len(z)
    L = 1 << int(np.ceil(np.log2(2 * m)))
    Z = np.fft.rfft(z, L)
    rzz = np.fft.irfft(Z * np.conj(Z), L)[:n]
    rzy = np.fft.irfft(np.fft.rfft(y, L) * np.conj(Z), L)[:n]
    rzz[0] *= 1 + 1e-5
    rzz[0] += 1e-12
    return solve_toeplitz(rzz, rzy)


def esr(y, yh):
    e = emph(y) - emph(yh)
    return float(np.sum(e ** 2) / (np.sum(emph(y) ** 2) + 1e-20))


# ---------- parameter vector helpers ----------

def unpack(v):
    p = {}
    for x, (name, lo, hi, lg) in zip(v, SPACE):
        t = min(max(x, 0.0), 1.0)
        p[name] = float(lo * (hi / lo) ** t if lg else lo + (hi - lo) * t)
    return p


def pack(p):
    v = []
    for name, lo, hi, lg in SPACE:
        x = p[name]
        v.append(np.log(x / lo) / np.log(hi / lo) if lg else (x - lo) / (hi - lo))
    return np.array(v)


# ---------- io ----------

def load(path):
    sr, x = wavfile.read(path)
    if x.dtype == np.int16:
        x = x / 32768.0
    elif x.dtype == np.int32:
        x = x / 2147483648.0
    elif x.dtype == np.uint8:
        x = (x - 128) / 128.0
    x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if sr != SR:
        from math import gcd
        g = gcd(sr, SR)
        x = resample_poly(x, SR // g, sr // g)
    return x


def _env(s, hop=441):
    e = np.sqrt(np.convolve(s ** 2, np.ones(hop) / hop, "same")[::hop])
    return np.log10(e + 1e-6)


def align(x, y):
    """Where dry sample 0 lands in the wet recording (recording may start anywhere; gates may blank parts).

    Coarse: correlate 10 ms log-loudness envelopes (robust to distortion and gating).
    Fine: cross-correlate the loud part of the signal within +-20 ms of the coarse lag.
    """
    hop = 441
    a, b = _env(x, hop), _env(y, hop)
    a = a - a.mean(); b = b - b.mean()
    L = 1 << int(np.ceil(np.log2(len(a) + len(b))))
    c = np.fft.irfft(np.fft.rfft(b, L) * np.conj(np.fft.rfft(a, L)), L)
    lags = np.r_[np.arange(0, len(b)), np.arange(-len(a) + 1, 0)]
    vals = np.r_[c[: len(b)], c[L - len(a) + 1:]]
    coarse = int(lags[np.argmax(vals)]) * hop

    # fine, on the loud sweep + noise + guitar region (starts ~17.5 s), which survives any gate
    s0, s1 = int(17.5 * SR), min(len(x), int(60 * SR))
    xs = x[s0:s1]
    w = 2 * int(0.02 * SR)
    lo = max(s0 + coarse - w // 2, 0)
    ys = y[lo: lo + len(xs) + w]
    if len(ys) < len(xs) // 2:
        return coarse
    n = len(xs) + len(ys)
    L = 1 << int(np.ceil(np.log2(n)))
    X, Y = np.fft.rfft(xs, L), np.fft.rfft(ys, L)
    cc = np.abs(np.fft.irfft(Y * np.conj(X) / (np.abs(X) + 1e-9 * np.abs(X).max()), L))[: w + 1]
    return lo + int(np.argmax(cc)) - s0


# ---------- fitting ----------

def fit(dry, wet, name, log=print):
    lat = align(dry, wet)
    shift = lat - PRE
    y = np.zeros_like(dry)
    lo = max(shift, 0)
    seg = wet[lo: shift + len(dry)]
    y[lo - shift: lo - shift + len(seg)] = seg
    if np.max(np.abs(y)) < 1e-4:
        raise RuntimeError("wet file is silent after alignment; is it a recording of the test file playing?")
    if shift < 0:
        log(f"  warning: recording started {-shift / SR:.2f} s after the test file; start recording first next time")
    if shift + len(dry) > len(wet):
        log(f"  warning: recording stopped {(shift + len(dry) - len(wet)) / SR:.2f} s early")
    log(f"  test signal starts at {lat / SR:.3f} s in the recording")

    # gate mask: where the rig output is silent but the input is not, the rig had a gate closed.
    # A static model can't (and shouldn't) learn that; Leviathan has its own gate. Drop those stretches.
    hop = 441
    ed, ew = _env(dry, hop), _env(y, hop)
    gated = (ew < -4.5) & (ed > -3.5)          # wet below ~-90 dBFS while dry above ~-70 dBFS
    gated = np.convolve(gated.astype(float), np.ones(5), "same") > 0   # widen by 20 ms each side
    mask = np.repeat(~gated, hop)[: len(dry)].astype(float)
    if len(mask) < len(dry):
        mask = np.r_[mask, np.ones(len(dry) - len(mask))]
    if gated.mean() > 0.02:
        log(f"  {gated.mean() * 100:.0f}% of the recording was gated by the rig; excluded "
            f"(turn off the gate / Noise & Feedback Filter for a cleaner capture)")
    y = y * mask

    n = len(dry)
    split = int(n * 0.85)                  # last 15% (strummed chords) is held out
    # fast search subset: chunks spread over the training region
    idx = np.concatenate([np.arange(s, s + int(1.5 * SR)) for s in np.linspace(int(SR), split - int(1.6 * SR), 10).astype(int)])

    def loss_for(p, x, yy, taps):
        z = front(x, p)
        h = solve_ir(emph(z), emph(yy), taps)
        return esr(yy, fftconvolve(z, h)[: len(yy)]), h

    # the front filters have memory, so run the full signal and score only the subset
    def objective(v):
        p = unpack(v)
        z = front(dry[:split], p)
        zs, ys = z[idx], y[idx]
        h = solve_ir(emph(zs), emph(ys), 512)
        return esr(ys, fftconvolve(zs, h)[: len(ys)])

    starts = []
    for k1 in (0.5, 5, 40, 200):
        for k2 in (0.3, 3):
            starts.append(pack(dict(hp=80, ls=0, pk_f=900, pk=0, hs=0, k1=k1, b1=0.1, mid=6000, k2=k2, b2=0.05)))
    t0 = time.time()
    scored = sorted((objective(s), i) for i, s in enumerate(starts))
    log(f"  coarse search best ESR {scored[0][0]:.4f}")
    best_v, best_l = None, np.inf
    for _, i in scored[:2]:
        r = minimize(objective, starts[i], method="Powell", bounds=[(0, 1)] * len(SPACE),
                     options=dict(maxfev=900, xtol=1e-3, ftol=1e-4))
        log(f"  refine from start {i}: ESR {r.fun:.4f} ({r.nfev} evals)")
        if r.fun < best_l:
            best_v, best_l = r.x, r.fun
    p = unpack(best_v)

    z = front(dry, p)
    h = solve_ir(emph(z[:split]), emph(y[:split]), 2048)
    f = 256
    h[-f:] *= np.linspace(1, 0, f)
    yh = fftconvolve(z, h)[:n]
    e_tr, e_val = esr(y[:split], yh[:split]), esr(y[split:], yh[split:])
    log(f"  ESR train {e_tr:.4f}  held-out {e_val:.4f}  ({time.time() - t0:.0f} s)")
    return dict(version=1, name=name, sr=SR, **p, ir=[float(v) for v in h],
                esr_train=e_tr, esr_val=e_val, latency=lat)


def main(argv):
    if not os.path.exists(DRY):
        print(f"missing {DRY}; run make_test.py first")
        return 1
    force = "--force" in argv
    files = [a for a in argv if not a.startswith("--")] or sorted(glob.glob(os.path.join(BASE, "wet", "*.wav")))
    if not files:
        print(f"no wet recordings in {os.path.join(BASE, 'wet')}")
        return 1
    os.makedirs(OUTDIR, exist_ok=True)
    dry = load(DRY)
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]
        out = os.path.join(OUTDIR, name + ".json")
        if os.path.exists(out) and not force and len(argv) == 0:
            print(f"skip {name} (already captured; --force to refit)")
            continue
        print(f"fitting {name}")
        try:
            cap = fit(dry, load(path), name)
        except Exception as e:  # keep going with the other presets
            print(f"  FAILED: {e}")
            continue
        with open(out, "w") as fh:
            json.dump(cap, fh)
        verdict = "excellent" if cap["esr_val"] < 0.01 else "very close" if cap["esr_val"] < 0.05 else \
            "close" if cap["esr_val"] < 0.15 else "rough (time-based FX or gate still on?)"
        print(f"  -> {out}  [{verdict}]")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
