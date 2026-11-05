#!/usr/bin/env python3
"""Generate the capture test signal.

Import the result into the amp you want to capture as a *dry* recording, reamp it,
and export the wet result. fit.py then learns a model from the (dry, wet) pair.

Layout (44.1 kHz mono, 16-bit, the most widely importable format):
  0      1 s silence
  1      log sweeps 20 Hz - 20 kHz at -42/-30/-18/-6 dBFS, each 4 s + 1.5 s gap
  ~23    band-limited noise with stepped levels (12 s)
  ~36    synthetic plucked guitar: single notes, power chords, open chords (~30 s)
  end    1 s silence
"""
import os
import sys
import numpy as np
from scipy.signal import butter, sosfilt
from scipy.io import wavfile

SR = 44100
OUT = os.path.expanduser("~/Music/Leviathan/Capture")
rng = np.random.default_rng(1234)   # fixed seed: the fitter regenerates nothing, but keeps files reproducible


def db(x):
    return 10 ** (x / 20)


def sweep(dur, f0=20.0, f1=20000.0):
    t = np.arange(int(dur * SR)) / SR
    k = np.log(f1 / f0)
    s = np.sin(2 * np.pi * f0 * dur / k * (np.exp(t / dur * k) - 1))
    f = int(0.01 * SR)
    s[:f] *= np.linspace(0, 1, f)
    s[-f:] *= np.linspace(1, 0, f)
    return s


def noise_section(dur=12.0):
    n = int(dur * SR)
    x = rng.standard_normal(n)
    x = sosfilt(butter(4, [80, 6000], "bandpass", fs=SR, output="sos"), x)
    x /= np.max(np.abs(x))
    seg = int(0.5 * SR)
    env = np.repeat(db(rng.uniform(-45, -3, n // seg + 1)), seg)[:n]
    env = sosfilt(butter(1, 30, fs=SR, output="sos"), env)   # no clicks between steps
    return x * env


def pluck(freq, dur, vel, pos=0.18):
    """Karplus-Strong string with a pickup-position comb and a body-ish low-pass."""
    n = int(dur * SR)
    period = SR / freq
    L = int(period)
    frac = period - L
    buf = rng.uniform(-1, 1, L) * vel
    buf = sosfilt(butter(1, 1500 + 6000 * vel, fs=SR, output="sos"), buf)
    out = np.zeros(n)
    decay = 0.996 + 0.003 * min(freq / 600, 1)
    idx = 0
    prev = 0.0
    for i in range(n):
        cur = buf[idx]
        nxt = buf[(idx + 1) % L]
        v = cur + (nxt - cur) * frac
        out[i] = v
        buf[idx] = decay * 0.5 * (v + prev)
        prev = v
        idx = (idx + 1) % L
    d = max(1, int(pos * period))
    out[d:] -= out[:-d]
    return out


def guitar_section():
    m2f = lambda m: 440 * 2 ** ((m - 69) / 12)
    parts = []
    # single notes across the neck, varied dynamics
    for m in [40, 43, 45, 47, 50, 52, 55, 57, 59, 62, 64, 67, 69, 72, 76]:
        parts.append(pluck(m2f(m), 0.7, rng.uniform(0.3, 1.0)))
    # power chords (root + fifth + octave), palm-mute-ish short and long
    for root in [40, 38, 43, 45, 36, 41]:
        for dur in (0.25, 0.25, 1.2):
            v = rng.uniform(0.5, 1.0)
            ch = sum(pluck(m2f(root + i), dur, v) for i in (0, 7, 12))
            parts.append(ch)
    # open chords, strummed (slight spread between strings)
    shapes = [[40, 47, 52, 56, 59, 64], [45, 52, 57, 61, 64], [43, 47, 50, 55, 59, 67], [50, 57, 62, 66]]
    for shape in shapes:
        for v in (0.35, 0.9):
            dur = 1.8
            n = int(dur * SR)
            ch = np.zeros(n)
            for i, m in enumerate(shape):
                off = int(i * 0.012 * SR)
                ch[off:] += pluck(m2f(m), dur, v)[: n - off]
            parts.append(ch)
    x = np.concatenate(parts)
    return x / np.max(np.abs(x)) * db(-4)


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(os.path.join(OUT, "wet"), exist_ok=True)
    sil = lambda s: np.zeros(int(s * SR))
    parts = [sil(1.0)]
    for lvl in (-42, -30, -18, -6):
        parts += [sweep(4.0) * db(lvl), sil(1.5)]
    parts += [noise_section(), sil(1.0), guitar_section(), sil(1.0)]
    x = np.concatenate(parts)
    path = os.path.join(OUT, "leviathan_capture_test.wav")
    wavfile.write(path, SR, np.round(np.clip(x, -1, 1) * 32767).astype(np.int16))
    print(f"wrote {path} ({len(x) / SR:.1f} s)")
    print(f"export each wet result into {os.path.join(OUT, 'wet')}/<preset name>.wav")


if __name__ == "__main__":
    sys.exit(main())
