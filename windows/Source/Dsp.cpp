// Port of mac/Sources/DSP.swift. Keep the two in step: tools/dsp_render.cpp checks they render the same.
#include "Dsp.h"
#include <algorithm>
#include <cstring>

namespace lev {

// MARK: - Primitives

void Biquad::set(Kind k, double freq, double sr, double q, double db) {
    const double f = std::min(std::max(freq, 5.0), sr * 0.48);
    const double w = 2 * 3.141592653589793 * f / sr, cw = std::cos(w), sw = std::sin(w), al = sw / (2 * q), A = std::pow(10.0, db / 40);
    double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
    switch (k) {
    case lp: B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
    case hp: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
    case bp: B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
    case peak: B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break;
    case lowShelf: {
        const double s = 2 * std::sqrt(A) * al;
        B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
        A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s;
        break;
    }
    case highShelf: {
        const double s = 2 * std::sqrt(A) * al;
        B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
        A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s;
        break;
    }
    }
    b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
}

void Ring::write(const float* x, int n) {
    const int64_t wv = w.load(std::memory_order_relaxed), rv = r.load(std::memory_order_acquire);
    const int k = (int) std::min<int64_t>(n, cap - (wv - rv));
    for (int i = 0; i < k; ++i) buf[(size_t) ((wv + i) & mask)] = x[i];
    w.store(wv + k, std::memory_order_release);
}

int Ring::read(float* dst, int n) {
    const int64_t rv = r.load(std::memory_order_relaxed), wv = w.load(std::memory_order_acquire);
    const int k = (int) std::min<int64_t>(n, wv - rv);
    for (int i = 0; i < k; ++i) dst[i] = buf[(size_t) ((rv + i) & mask)];
    r.store(rv + k, std::memory_order_release);
    return k;
}

// MARK: - Pedals

const char* pedalTitle(PedalKind k) {
    static const char* t[] = { "COMPRESSOR", "WAH", "FUZZ", "OVERDRIVE", "CHORUS", "FLANGER", "PHASER", "TREMOLO", "BLADE" };
    return t[(int) k];
}
const char* pedalOnKey(PedalKind k) {
    static const char* t[] = { "compOn", "wahOn", "fuzzOn", "odOn", "chorusOn", "flangerOn", "phaserOn", "tremOn", "bladeOn" };
    return t[(int) k];
}

void Pedal::process(float* a, int n, float c) {
    if (mix.target == 0 && mix.v < 1e-4f) return;
    for (int i = 0; i < n; ++i) {
        const float m = mix.next(c), x = a[i];
        if (m < 1e-4f) continue;
        a[i] = x + (tick(x) - x) * m;
    }
}

namespace {

/// Feed-forward peak compressor; Sustain sets threshold and ratio together, makeup is automatic.
class Compressor : public Pedal {
public:
    using Pedal::Pedal;
    void update(const Params& p) override {
        mix.target = p.compOn ? 1.f : 0.f;
        atk = std::exp(-1 / (std::max(p.compAttack, 0.5f) / 1000 * sr)); rel = std::exp(-1 / (0.18f * sr));
        thr = -8 - p.compSustain * 40;
        slope = 1 - 1 / (3 + p.compSustain * 7);
        makeup = dbLin(-thr * slope * 0.55f + p.compLevel);
        blend = p.compBlend;
    }
    float tick(float x) override {
        const float ax = std::abs(x);
        env = ax + (env - ax) * (ax > env ? atk : rel);
        const float lvl = 20 * std::log10(env + 1e-9f);
        const float y = x * (lvl > thr ? dbLin((thr - lvl) * slope) : 1) * makeup;
        return x + (y - x) * blend;
    }
private:
    float env = 0, atk = 0, rel = 0, thr = 0, slope = 0, makeup = 1, blend = 1;
};

/// Resonant band-pass sweep (TPT state-variable filter). Manual position or envelope-driven auto-wah.
class Wah : public Pedal {
public:
    using Pedal::Pedal;
    void update(const Params& p) override {
        mix.target = p.wahOn ? 1.f : 0.f;
        pos.target = p.wahPos; autoMode = p.wahAuto; sens = p.wahSens;
        k = 1 / (2 + p.wahQ * 8);
        eA = std::exp(-1 / (0.004f * sr)); eR = std::exp(-1 / (0.12f * sr));
    }
    float tick(float x) override {
        const float ax = std::abs(x);
        env = ax + (env - ax) * (ax > env ? eA : eR);
        const float pv = pos.next(0.002f);
        if (cnt == 0) {
            const float t = autoMode ? std::min(env * (2 + sens * 40), 1.0f) : pv;
            g = std::tan(kPi * std::min(350 * std::pow(2200.0f / 350.0f, t), sr * 0.45f) / sr);
        }
        cnt = (cnt + 1) & 15;
        const float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v1 * k * 2.5f + v2 * 0.1f;
    }
private:
    float ic1 = 0, ic2 = 0, env = 0, k = 0.3f, g = 0.1f; int cnt = 0;
    bool autoMode = false; float sens = 0.5f, eA = 0, eR = 0;
    Smooth pos;
};

const double kAAQ[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };

/// High-gain asymmetric clipper, 4x oversampled, with a scooped tone control (LP/HP blend).
class Fuzz : public Pedal {
public:
    explicit Fuzz(float s) : Pedal(s) {
        const double os = (double) s * 4, aa = std::min(20000.0, (double) s * 0.45);
        for (int i = 0; i < 4; ++i) { up[i].set(Biquad::lp, aa, os, kAAQ[i]); down[i].set(Biquad::lp, aa, os, kAAQ[i]); }
    }
    void update(const Params& p) override {
        const double s = sr;
        mix.target = p.fuzzOn ? 1.f : 0.f;
        hp.set(Biquad::hp, 90, s); lpT.set(Biquad::lp, 550, s); hpT.set(Biquad::hp, 1100, s); dc.set(Biquad::hp, 20, s);
        pre = 4 + p.fuzzAmt * p.fuzzAmt * 300; tone = p.fuzzTone; lvl = lerpf(p.fuzzLevel, 0.05f, 1.2f);
    }
    float tick(float x) override {
        const float v = hp.run(x) * pre * 4;
        float o = 0;
        for (int k = 0; k < 4; ++k) {
            float u = up[3].run(up[2].run(up[1].run(up[0].run(k == 0 ? v : 0))));
            u = u > 0 ? std::tanh(u) : std::tanh(u * 0.7f) * 0.8f;
            o = down[3].run(down[2].run(down[1].run(down[0].run(u))));
        }
        const float y = dc.run(o);
        return (lpT.run(y) * (1 - tone) + hpT.run(y) * tone) * lvl * 1.6f;
    }
private:
    Biquad hp, lpT, hpT, dc, up[4], down[4];
    float pre = 1, tone = 0.5f, lvl = 1;
};

/// Op-amp distortion voiced after the EAE Longsword (see Blade in DSP.swift): boost, selectable clipping, 3-band EQ.
class Blade : public Pedal {
public:
    explicit Blade(float s) : Pedal(s) {
        const double os = (double) s * 4, aa = std::min(20000.0, (double) s * 0.45);
        for (int i = 0; i < 4; ++i) { up[i].set(Biquad::lp, aa, os, kAAQ[i]); down[i].set(Biquad::lp, aa, os, kAAQ[i]); }
    }
    void update(const Params& p) override {
        const double s = sr;
        mix.target = p.bladeOn ? 1.f : 0.f;
        hp.set(Biquad::hp, 70, s); bHP.set(Biquad::hp, 220, s, 0.6); bLP.set(Biquad::lp, 4500, s, 0.6);
        focus.set(Biquad::peak, 720, s, 0.7, 4); post.set(Biquad::lp, 6800, s, 0.6); dc.set(Biquad::hp, 20, s);
        low.set(Biquad::lowShelf, 180, s, 0.7071, (double) ((p.bladeLow - 0.5f) * 24));
        mid.set(Biquad::peak, p.bladeShift ? 1500 : 650, s, 0.9, (double) ((p.bladeMid - 0.5f) * 24));
        high.set(Biquad::highShelf, 2800, s, 0.7071, (double) ((p.bladeHigh - 0.5f) * 24));
        gain = 1 + p.bladeDrive * p.bladeDrive * 200;
        boostOn = p.bladeBoostOn; boost = dbLin(p.bladeBoost * 20);
        diode = p.bladeDiode;
        t = diode == 0 ? 0.8f : (diode == 1 ? 3.0f : 0.45f);
        norm = 1 / std::sqrt(t);
        lvl = lerpf(p.bladeLevel, 0.05f, 1.5f);
    }
    float clip(float u) const {
        if (diode == 0) return t * (std::tanh(u / t + 0.08f) - std::tanh(0.08f));
        if (diode == 1) return u / std::pow(1 + std::pow(std::abs(u) / t, 6.0f), 1.0f / 6.0f);
        return u / std::pow(1 + std::pow(std::abs(u) / t, 2.5f), 1.0f / 2.5f);
    }
    float tick(float x) override {
        float y = hp.run(x);
        if (boostOn) y += bLP.run(bHP.run(y)) * (boost - 1);
        const float v = focus.run(y) * gain;
        float o = 0;
        for (int k = 0; k < 4; ++k) {
            const float u = up[3].run(up[2].run(up[1].run(up[0].run(k == 0 ? v * 4 : 0))));
            o = down[3].run(down[2].run(down[1].run(down[0].run(clip(u)))));
        }
        const float z = post.run(dc.run(o)) * norm;
        return high.run(mid.run(low.run(z))) * lvl;
    }
private:
    Biquad hp, bHP, bLP, focus, post, dc, low, mid, high, up[4], down[4];
    float gain = 1, boost = 1, lvl = 1, t = 1, norm = 1; bool boostOn = false; int diode = 0;
};

class Overdrive : public Pedal {
public:
    using Pedal::Pedal;
    void update(const Params& p) override {
        const double s = sr;
        mix.target = p.odOn ? 1.f : 0.f;
        hp.set(Biquad::hp, 160, s, 0.5); shelf.set(Biquad::lowShelf, 500, s, 0.7071, -9); hump.set(Biquad::peak, 900, s, 0.8, 3);
        lp.set(Biquad::lp, (double) lerpf(p.odTone, 1200, 7000), s, 0.6);
        pre = 1 + p.odDrive * p.odDrive * 60; lvl = lerpf(p.odLevel, 0.2f, 2.5f);
    }
    float tick(float x) override { return lp.run(std::tanh(hump.run(shelf.run(hp.run(x))) * pre + 0.05f) - 0.04996f) * lvl; }
private:
    Biquad hp, shelf, hump, lp;
    float pre = 1, lvl = 1;
};

/// Short fractional delay line for modulation effects.
class ModLine {
public:
    ModLine() : buf(8192, 0.0f) {}
    inline void push(float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float tap(float d) const {
        const float rp = (float) w - 1 - std::min(std::max(d, 1.0f), (float) (size - 4)) + (float) size;
        const int i0 = (int) rp; const float f = rp - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * f;
    }
private:
    static constexpr int size = 8192, mask = 8191;
    std::vector<float> buf; int w = 0;
};

class Chorus : public Pedal {
public:
    using Pedal::Pedal;
    void update(const Params& p) override {
        mix.target = p.chorusOn ? 1.f : 0.f;
        inc = p.chorusRate / sr; depth = p.chorusDepth * 0.006f * sr; wet = p.chorusMix;
    }
    float tick(float x) override {
        line.push(x);
        ph += inc; if (ph >= 1) ph -= 1;
        const float base = 0.012f * sr;
        const float t1 = line.tap(base + depth * (0.5f + 0.5f * std::sin(2 * kPi * ph)));
        const float t2 = line.tap(base * 1.3f + depth * (0.5f + 0.5f * std::sin(2 * kPi * (ph + 0.37f))));
        return x * (1 - wet * 0.5f) + (t1 + t2) * 0.5f * wet;
    }
private:
    ModLine line; float ph = 0, inc = 0, depth = 0, wet = 0.5f;
};

class Flanger : public Pedal {
public:
    using Pedal::Pedal;
    void update(const Params& p) override {
        mix.target = p.flangerOn ? 1.f : 0.f;
        inc = p.flangerRate / sr; depth = p.flangerDepth * 0.005f * sr; fb = p.flangerFb * 0.85f; wet = p.flangerMix;
    }
    float tick(float x) override {
        ph += inc; if (ph >= 1) ph -= 1;
        line.push(x + last * fb);
        last = line.tap(0.0003f * sr + depth * (0.5f - 0.5f * std::cos(2 * kPi * ph)));
        return (x + last * wet) / (1 + wet * (1 + fb));
    }
private:
    ModLine line; float ph = 0, inc = 0, depth = 0, fb = 0, wet = 0.5f, last = 0;
};

class Phaser : public Pedal {
public:
    using Pedal::Pedal;
    void update(const Params& p) override {
        mix.target = p.phaserOn ? 1.f : 0.f;
        inc = p.phaserRate / sr; depth = p.phaserDepth; fb = p.phaserFb * 0.7f;
    }
    float tick(float x) override {
        ph += inc; if (ph >= 1) ph -= 1;
        if (cnt == 0) {
            const float t = 0.5f - 0.5f * std::cos(2 * kPi * ph);
            const float f = 250 * std::pow(10.0f, t * depth);              // 250 Hz up to 2.5 kHz at full depth
            const float tn = std::tan(kPi * std::min(f, sr * 0.45f) / sr);
            a = (tn - 1) / (tn + 1);
        }
        cnt = (cnt + 1) & 7;
        float u = x + last * fb;
        for (int i = 0; i < 6; ++i) { const float y = a * u + z[i]; z[i] = u - a * y; u = y; }
        last = u;
        return (x + u) * 0.5f;
    }
private:
    float z[6] = {}; float ph = 0, inc = 0, depth = 0, fb = 0, a = 0, last = 0; int cnt = 0;
};

class Tremolo : public Pedal {
public:
    using Pedal::Pedal;
    void update(const Params& p) override {
        mix.target = p.tremOn ? 1.f : 0.f;
        inc = p.tremRate / sr; depth = p.tremDepth;
        k = 1 + p.tremShape * 10; norm = 1 / std::tanh(k);
    }
    float tick(float x) override {
        ph += inc; if (ph >= 1) ph -= 1;
        const float s = std::tanh(std::sin(2 * kPi * ph) * k) * norm;
        return x * (1 - depth * (0.5f - 0.5f * s));
    }
private:
    float ph = 0, inc = 0, depth = 0, k = 1, norm = 1;
};

} // namespace

std::unique_ptr<Pedal> makePedal(PedalKind k, float sr) {
    switch (k) {
    case PedalKind::comp: return std::make_unique<Compressor>(sr);
    case PedalKind::wah: return std::make_unique<Wah>(sr);
    case PedalKind::fuzz: return std::make_unique<Fuzz>(sr);
    case PedalKind::blade: return std::make_unique<Blade>(sr);
    case PedalKind::od: return std::make_unique<Overdrive>(sr);
    case PedalKind::chorus: return std::make_unique<Chorus>(sr);
    case PedalKind::flanger: return std::make_unique<Flanger>(sr);
    case PedalKind::phaser: return std::make_unique<Phaser>(sr);
    case PedalKind::trem: return std::make_unique<Tremolo>(sr);
    }
    return std::make_unique<Pedal>(sr);
}

// MARK: - Amp voicings

const Voicing VOICINGS[3] = {
    { "CLEAN", 60, 0, 1.5, 5, 0.03f, false, 1, 0, 1, 20, 15000, 15000, 15000, 500, 0, 1.2f, 1.8f },
    { "RUST", 75, -1.5, 3, 35, 0.15f, true, 3, 0.05f, 1.6f, 40, 12000, 9000, 8000, 700, 2, 1.6f, 0.95f },
    { "HOT", 90, -3, 4, 90, 0.2f, true, 6, 0.08f, 2.5f, 70, 10000, 7500, 6500, 800, 3, 1.6f, 0.85f },
};

// MARK: - Engine

DSP::DSP(double sampleRate, Params& params)
    : sr((float) sampleRate), p(params),
      a(maxN), b(maxN), wl(maxN), wr(maxN), gBuf(maxN, 1.0f),
      fade(1 - std::exp(-1 / (0.006f * (float) sampleRate))),
      shifter(sampleRate),
      namBuf(maxN, 0.0f),
      hist(maxIR + maxN, 0.0f),
      dSize((int) (sampleRate * 2.1)), reverb(sampleRate) {
    for (int k = 0; k < kPedalCount; ++k) pedals.push_back(makePedal((PedalKind) k, (float) sampleRate));
    limRel = 1 - std::exp(-1 / (0.08f * (float) sampleRate));
    dBuf.assign((size_t) dSize, 0.0f);
    irs[0].assign(maxIR, 0.0f); irs[1].assign(maxIR, 0.0f);
    irs[0][0] = 1; irLen[0] = 1;
    const double os = sampleRate * 4, aa = std::min(20000.0, sampleRate * 0.45);
    for (int i = 0; i < 4; ++i) { up[i].set(Biquad::lp, aa, os, kAAQ[i]); down[i].set(Biquad::lp, aa, os, kAAQ[i]); }
    aDC.set(Biquad::hp, 15, sampleRate);
    update(true);
}

void DSP::setIR(const std::vector<float>& h) {
    const int slot = 1 - irActive.load(), n = (int) std::min<size_t>(h.size(), maxIR);
    // stored reversed so the convolution is a forward dot product of two contiguous arrays
    for (int i = 0; i < n; ++i) irs[slot][(size_t) i] = h[(size_t) (n - 1 - i)];
    irLen[slot] = std::max(n, 1);
    if (n == 0) irs[slot][0] = 0;
    irActive.store(slot);
}

void DSP::update(bool force) {
    const int ver = p.version.load(std::memory_order_acquire);
    if (!force && ver == lastVersion) return;
    lastVersion = ver;
    const double s = sr, os = s * 4;
    inG.target = dbLin(p.inGain);
    gThr = dbLin(p.gateThresh);
    // Depth: how far a closed gate turns things down; -60 dB means fully closed. Half of it at each of the two stages.
    gFloor = p.gateDepth <= -60 ? 0.f : dbLin(p.gateDepth / 2);
    // Release = time to fall 60 dB (closed), not one time constant: after a high-gain amp a gate that is only
    // ~9 dB down still sounds open, so a one-time-constant release leaves a long audible tail. The gain is applied twice,
    // before the pedals and again after the cab, so each stage falls 30 dB over Release: a clean tone fades exactly as
    // before, and a high-gain rig (which holds a fading input at full level until it is ~40 dB down) still fades evenly
    // instead of hanging at full level and then dropping all at once.
    // the last stretch of the dial (2050 ms and up) is infinite: once open the gate never fades
    gRelC = p.gateRelease >= 2050 ? 1.f : std::exp(std::log(0.001f) / 2 / (p.gateRelease / 1000 * sr));
    gEnvR = std::exp(-1 / (0.015f * sr)); gAttC = std::exp(-1 / (0.0003f * sr));

    shifter.setRatio(std::pow(2.0, (double) (std::round(p.pitchSemis) + p.pitchCents / 100) / 12));
    pMix.target = p.pitchOn ? p.pitchMix : 0;

    for (auto& pd : pedals) pd->update(p);

    v = VOICINGS[std::min(std::max(p.ampChannel, 0), 2)];
    ampMix.target = p.ampOn ? 1.f : 0.f;
    aHP.set(Biquad::hp, v.hp, s, 0.7); aTight.set(Biquad::lowShelf, 250, s, 0.7071, v.tight); aEmph.set(Biquad::peak, 1800, s, 0.6, v.emph);
    aPre.target = 1 + (v.preMax - 1) * std::pow(p.ampGain, 1.5f);
    tb1 = std::tanh(v.b1); tb2 = std::tanh(v.b2);
    aLP1.set(Biquad::lp, v.lp1, os); aIHP.set(Biquad::hp, v.interHP, os); aLP2.set(Biquad::lp, v.lp2, os); aLP3.set(Biquad::lp, v.lp3, os);
    aChar.set(Biquad::peak, 800, s, 0.8, v.character);
    aBass.set(Biquad::lowShelf, 110, s, 0.7071, (double) ((p.bass - 0.5f) * 24));
    aMid.set(Biquad::peak, v.mid, s, 0.6, (double) ((p.mid - 0.5f) * 24));
    aTreb.set(Biquad::highShelf, 2200, s, 0.7071, (double) ((p.treble - 0.5f) * 24));
    aDepth.set(Biquad::peak, 90, s, 0.9, (double) ((p.depth - 0.5f) * 20));
    aPres.set(Biquad::highShelf, 3500, s, 0.7071, (double) ((p.presence - 0.5f) * 20));
    aVol.target = dbLin(-30 + p.ampVolume * 36) * v.out;

    capOn = p.capOn;
    kHP.set(Biquad::hp, p.capHP, s, 0.7071); kLS.set(Biquad::lowShelf, 250, s, 0.7071, p.capLS);
    kPk.set(Biquad::peak, p.capPkF, s, 0.7, p.capPk); kHS.set(Biquad::highShelf, 3000, s, 0.7071, p.capHS);
    kMid.set(Biquad::lp, p.capMid, os);
    kK1 = p.capK1 * dbLin(p.capDrive); kB1 = p.capB1; kK2 = p.capK2; kB2 = p.capB2;
    kT1 = std::tanh(kB1); kT2 = std::tanh(kB2);
    kLvl.target = dbLin(p.capLevel);
    namInG.target = dbLin(p.namIn); namOutG.target = dbLin(p.namOut);
    if (force) { namInG.v = namInG.target; namOutG.v = namOutG.target; }

    eqMix.target = p.eqOn ? 1.f : 0.f;
    for (int i = 0; i < 8; ++i) {
        const auto kind = i == 0 ? Biquad::lowShelf : i == 7 ? Biquad::highShelf : Biquad::peak;
        eqB[i].set(kind, eqF[i], s, 1.4, p.eq[i]);
        eqS[i].set(kind, eqF[i], s, 1.4, p.eq[i]);
    }

    cabMix.target = p.cabOn ? 1.f : 0.f; cabLvl.target = dbLin(p.cabLevel);
    // a capture's IR already is the cab: leave it unfiltered so it stays faithful
    const double lowCut = p.capOn ? 10 : (double) p.cabLowCut, highCut = p.capOn ? s * 0.47 : (double) p.cabHighCut;
    cLo.set(Biquad::hp, lowCut, s); cHi1.set(Biquad::lp, highCut, s, 0.54); cHi2.set(Biquad::lp, highCut, s, 1.31);
    cLoS.set(Biquad::hp, lowCut, s); cHi1S.set(Biquad::lp, highCut, s, 0.54); cHi2S.set(Biquad::lp, highCut, s, 1.31);
    bMix.target = p.ampBOn ? 1.f : 0.f; bLvl.target = dbLin(p.ampBLevel); bSpread.target = p.ampBSpread;
    if (force) { bLvl.v = bLvl.target; bSpread.v = bSpread.target; }

    dSend.target = p.delayOn ? 1.f : 0.f; dTime.target = p.delayTime / 1000 * sr;
    dFb = p.delayFeedback; dWet = p.delayMix * 1.4f; dLP.set(Biquad::lp, (double) lerpf(p.delayTone, 1200, 9000), s);
    if (force) dTime.v = dTime.target;

    rSend.target = p.revOn ? 1.f : 0.f; rWet = p.revMix * 3;
    dbWidth.target = p.doublerOn ? p.doublerWidth : 0.f;
    dbTime.target = std::min(std::max((float) p.doublerTime, 5.f), 40.f) / 1000 * sr;
    dbInc1 = 0.11f / sr; dbInc2 = 0.17f / sr;
    if (dbTime.v == 0) dbTime.v = dbTime.target;
    reverb.set(p.revSize, sr);
}

void DSP::process(const float* input, float* outL, float* outR, int frames) {
    update();
    const int n = std::min(frames, maxN);
    const float c = fade;
    float inPk = 0;
    int w = p.tunerWrite.load(std::memory_order_relaxed);
    const bool recording = p.recording.load();

    // reamp: a stored dry take replaces the live input
    const float* src = input;
    if (p.srcPlaying.load(std::memory_order_acquire)) {
        if (const float* sp = p.srcPtr.load(std::memory_order_acquire)) {
            const int len = p.srcLen.load(); int pos = p.srcPos.load(); const bool loop = p.srcLoop.load();
            for (int i = 0; i < n; ++i) {
                if (pos >= len && loop) pos = 0;
                b[(size_t) i] = pos < len ? sp[pos] : 0; pos++;
            }
            p.srcPos.store(pos);
            if (pos >= len && !loop) { p.srcPlaying.store(false); p.srcDone.store(true); }
            src = b.data();
        }
    }
    if (recording) p.recDry.write(src, n);
    { int clips = 0; for (int i = 0; i < n; ++i) if (std::abs(src[i]) >= 0.989f) clips++; if (clips) p.inClips.fetch_add(clips); }   // >= -0.1 dBFS

    // input gain, tuner tap, gate
    const float close = gThr * 0.5f; const int holdN = (int) (0.012f * sr); const bool gateOn = p.gateOn;
    for (int i = 0; i < n; ++i) {
        const float x = src[i] * inG.next(c);
        inPk = std::max(inPk, std::abs(x));
        p.tunerRing[w] = x; w = (w + 1) & 4095;
        const float ax = std::abs(x);
        env = ax > env ? ax : env * gEnvR + ax * (1 - gEnvR);
        if (env > gThr) { gTarget = 1; gHold = holdN; }
        else if (env < close) { if (gHold > 0) gHold--; else gTarget = 0; }
        gGain = gTarget + (gGain - gTarget) * (gTarget > gGain ? gAttC : gRelC);
        const float g = gFloor + (1 - gFloor) * gGain;
        gBuf[(size_t) i] = g;
        a[(size_t) i] = gateOn ? x * g : x;
    }
    p.tunerWrite.store(w, std::memory_order_release);
    p.inPeak.store(std::max(inPk, p.inPeak.load() * 0.9f));
    p.gateOpen.store(gTarget > 0 || !gateOn);

    // pitch shift
    if (p.pitchOn || pMix.v > 1e-4f) {
        for (int i = 0; i < n; ++i) {
            const float m = pMix.next(c), x = a[(size_t) i];
            a[(size_t) i] = x + (shifter.process(x) - x) * m;
        }
        p.pitchLatencyMs.store(p.pitchOn && shifter.ratio != 1 ? (float) shifter.avgDelay / sr * 1000 : 0);
    } else {
        for (int i = 0; i < n; ++i) shifter.write(a[(size_t) i]);
        p.pitchLatencyMs.store(0);
    }

    // pedal chain, in user order
    for (int k = 0; k < std::min(p.chainCount, Params::maxChain); ++k) {
        const int id = p.chain[k];
        if (id >= 0 && id < (int) pedals.size()) pedals[(size_t) id]->process(a.data(), n, c);
    }

    // NAM capture in place of the amp (block-processed; same bridge as the Mac app)
    // Amp B runs on the same pedal output: keep a copy before amp A changes it
    LevNam* nb = namB.load(std::memory_order_acquire);
    const bool sideOn = nb != nullptr && (bMix.target > 0 || bMix.v > 1e-4f);
    if (sideOn) {
        const float k = namBCal.load();
        for (int i = 0; i < n; ++i) bBuf[(size_t) i] = a[(size_t) i] * k;
        if (!sideWasOn) std::fill(hist2.begin(), hist2.end(), 0.0f);
    }

    LevNam* nm = p.namOn ? nam.load(std::memory_order_acquire) : nullptr;
    if (nm) {
        const float k = namCal.load();
        for (int i = 0; i < n; ++i) namBuf[(size_t) i] = a[(size_t) i] * namInG.next(c) * k;
        lev_nam_process(nm, namBuf.data(), namBuf.data(), n);
        const float g = namGain.load();
        for (int i = 0; i < n; ++i) {
            const float m = ampMix.next(c), x = a[(size_t) i];
            aPre.next(c); aVol.next(c);
            a[(size_t) i] = x + (namBuf[(size_t) i] * g * namOutG.next(c) - x) * m;
        }
    }
    // amp, 4x oversampled through the clipping stages
    for (int i = 0; i < (nm ? 0 : n); ++i) {
        const float m = ampMix.next(c), x = a[(size_t) i];
        if (m < 1e-4f) continue;
        const float pre = aPre.next(c), vol = aVol.next(c);
        if (capOn) {
            // same structure as fit.py: filters -> tanh -> LP -> tanh, oversampled; the FIR runs in the cab slot
            const float u0 = kHS.run(kPk.run(kLS.run(kHP.run(x)))) * 4;
            float o = 0;
            for (int k = 0; k < 4; ++k) {
                float u = up[3].run(up[2].run(up[1].run(up[0].run(k == 0 ? u0 : 0))));
                u = std::tanh(kK1 * u + kB1) - kT1;
                u = kMid.run(u);
                u = std::tanh(kK2 * u + kB2) - kT2;
                o = down[3].run(down[2].run(down[1].run(down[0].run(u))));
            }
            a[(size_t) i] = x + (o * kLvl.next(c) - x) * m;
            continue;
        }
        const float vin = aEmph.run(aTight.run(aHP.run(x))) * pre * 4;
        float o = 0;
        for (int k = 0; k < 4; ++k) {
            float u = up[3].run(up[2].run(up[1].run(up[0].run(k == 0 ? vin : 0))));
            u = std::tanh(u + v.b1) - tb1;
            u = aIHP.run(aLP1.run(u));
            if (v.cascade) {
                u = aLP2.run(std::tanh(u * v.s2 + v.b2) - tb2);
                u = aLP3.run(std::tanh(u * v.s3));
            }
            o = down[3].run(down[2].run(down[1].run(down[0].run(u))));
        }
        float y = aTreb.run(aMid.run(aBass.run(aChar.run(o))));
        y = std::tanh(y * v.power);
        y = aDC.run(aPres.run(aDepth.run(y))) * vol;
        // its odd number of inverting stages flip the signal; turn it back so it lines up with NAM captures
        // (blended as amp A and B they'd otherwise partly cancel) and with the dry signal while switching
        a[(size_t) i] = x + (-y - x) * m;
    }

    // Amp B: mid = blend of A and B, side = (A - B) / 2 x Spread; side goes through its own EQ and cab
    if (sideOn) {
        lev_nam_process(nb, bBuf.data(), bBuf.data(), n);
        const float g = namBGain.load();
        for (int i = 0; i < n; ++i) {
            const float m = bMix.next(c), A = a[(size_t) i], B = bBuf[(size_t) i] * g * bLvl.next(c);
            a[(size_t) i] = A + ((A + B) * 0.5f - A) * m;
            sBuf[(size_t) i] = (A - B) * 0.5f * bSpread.next(c) * m;
        }
    }
    sideWasOn = sideOn;

    // graphic eq
    for (int i = 0; i < n; ++i) {
        const float m = eqMix.next(c), x = a[(size_t) i];
        if (m < 1e-4f) continue;
        float y = x;
        for (auto& bq : eqB) y = bq.run(y);
        a[(size_t) i] = x + (y - x) * m;
        if (sideOn) {
            const float xs = sBuf[(size_t) i];
            float ys = xs;
            for (auto& bq : eqS) ys = bq.run(ys);
            sBuf[(size_t) i] = xs + (ys - xs) * m;
        }
    }

    // cabinet
    const int slot = irActive.load(), L = irLen[slot], tail = maxIR - 1;
    std::memmove(hist.data(), hist.data() + n, (size_t) tail * sizeof(float));   // shift history left by n
    std::memcpy(hist.data() + tail, a.data(), (size_t) n * sizeof(float));
    const float* h = irs[slot].data();
    for (int i = 0; i < n; ++i) {
        const float* xs = hist.data() + tail - (L - 1) + i;
        float acc = 0;
        for (int k = 0; k < L; ++k) acc += xs[k] * h[k];
        b[(size_t) i] = acc;
    }
    if (sideOn) {
        std::memmove(hist2.data(), hist2.data() + n, (size_t) tail * sizeof(float));
        std::memcpy(hist2.data() + tail, sBuf.data(), (size_t) n * sizeof(float));
        for (int i = 0; i < n; ++i) {
            const float* xs = hist2.data() + tail - (L - 1) + i;
            float acc = 0;
            for (int k = 0; k < L; ++k) acc += xs[k] * h[k];
            sConv[(size_t) i] = acc;
        }
    }
    for (int i = 0; i < n; ++i) {
        const float m = cabMix.next(c), lvl = cabLvl.next(c);
        const float y = cHi2.run(cHi1.run(cLo.run(b[(size_t) i]))) * lvl;
        a[(size_t) i] += (y - a[(size_t) i]) * m;
        if (sideOn) {
            const float ys = cHi2S.run(cHi1S.run(cLoS.run(sConv[(size_t) i]))) * lvl;
            sBuf[(size_t) i] += (ys - sBuf[(size_t) i]) * m;
        }
    }

    // gate, second stage: the same fade after the amp and cab (delay and reverb tails still ring out)
    if (gateOn) for (int i = 0; i < n; ++i) a[(size_t) i] *= gBuf[(size_t) i];
    if (gateOn && sideOn) for (int i = 0; i < n; ++i) sBuf[(size_t) i] *= gBuf[(size_t) i];

    // delay (send stays live so tails ring out when switched off)
    for (int i = 0; i < n; ++i) {
        const float d = std::min(std::max(dTime.next(0.0005f), 1.0f), (float) (dSize - 2));
        float rp = (float) dW - d; if (rp < 0) rp += (float) dSize;
        const int i0 = (int) rp, i1 = (i0 + 1) % dSize; const float fr = rp - (float) i0;
        const float wet = dLP.run(dBuf[(size_t) i0] + (dBuf[(size_t) i1] - dBuf[(size_t) i0]) * fr);
        dBuf[(size_t) dW] = a[(size_t) i] * dSend.next(c) + wet * dFb + 1e-20f;
        dW = (dW + 1) % dSize;
        a[(size_t) i] += wet * dWet;
    }

    // reverb + master
    const float vol = dbLin(p.masterVol); const bool mute = p.muted.load();
    float outPk = 0;
    for (int i = 0; i < n; ++i) {
        const float x = a[(size_t) i], rin = x * rSend.next(c) * 0.015f;
        float l = 0, r = 0;
        reverb.process(rin, l, r);
        // doubler: two copies ~T and ~0.62 T behind, each drifting ±6-8 % at its own slow rate (about ±1.5 cents)
        dbBuf[(size_t) dbW] = x;
        dbPh1 += dbInc1; if (dbPh1 >= 1) dbPh1 -= 1;
        dbPh2 += dbInc2; if (dbPh2 >= 1) dbPh2 -= 1;
        const float tb = dbTime.next(0.0005f), w = dbWidth.next(c);
        float side = 0;
        if (w > 1e-4f) {
            constexpr float twoPi = 6.283185307f;
            const float t1 = tb * (1 + 0.06f * std::sin(twoPi * dbPh1)), t2 = tb * 0.62f * (1 + 0.08f * std::sin(twoPi * dbPh2));
            const float r1 = (float) dbW - t1 + (float) dbSize, r2 = (float) dbW - t2 + (float) dbSize;
            const int i1 = (int) r1, i2 = (int) r2; const float f1 = r1 - (float) i1, f2 = r2 - (float) i2;
            const float d1 = dbBuf[(size_t) (i1 & dbMask)] + (dbBuf[(size_t) ((i1 + 1) & dbMask)] - dbBuf[(size_t) (i1 & dbMask)]) * f1;
            const float d2 = dbBuf[(size_t) (i2 & dbMask)] + (dbBuf[(size_t) ((i2 + 1) & dbMask)] - dbBuf[(size_t) (i2 & dbMask)]) * f2;
            side = (d1 - d2) * 0.5f * w;
        }
        dbW = (dbW + 1) & dbMask;
        const float sb = sideOn ? sBuf[(size_t) i] : 0.0f;
        float yl = (x + side + sb + l * rWet) * vol, yr = (x - side - sb + r * rWet) * vol;
        // peak limiter: gain drops at once to keep the peak at the ceiling, then recovers over ~80 ms
        const float pk = std::max(std::abs(yl), std::abs(yr)), target = pk > limCeiling ? limCeiling / pk : 1.0f;
        limG = target < limG ? target : limG + (target - limG) * limRel;
        yl *= limG; yr *= limG;
        yl = std::min(std::max(yl, -1.0f), 1.0f); yr = std::min(std::max(yr, -1.0f), 1.0f);
        outPk = std::max(outPk, std::abs(yl));
        wl[(size_t) i] = yl; wr[(size_t) i] = yr;
    }
    p.outPeak.store(std::max(outPk, p.outPeak.load() * 0.9f));
    if (recording) { p.recL.write(wl.data(), n); p.recR.write(wr.data(), n); }

    // backing track + metronome ride on top of the rig, after the recorder tap
    const bool bkOn = p.bkPlaying.load(std::memory_order_acquire);
    const float* bl = p.bkL.load(); const float* br = p.bkR.load();
    const int bLen = p.bkLen.load(); const float bg = dbLin(p.bkVol);
    int bPos = p.bkPos.load();
    if (bkOn) { const int sk = p.bkSeek.exchange(-1); if (sk >= 0) bPos = std::min(sk, std::max(bLen - 1, 0)); }
    const bool metro = p.metroOn.load(); const float mg = dbLin(p.metroVol);
    if (metro && !mWas) { mCount = 0; mBeat = -1; }
    mWas = metro;
    const int beatLen = std::max((int) (60 / std::max(p.bpm, 20.0f) * sr), 1), beats = std::max(p.beatsPerBar, 1);
    const int clickLen = (int) (0.08f * sr); const float clickDecay = 1 / (0.022f * sr);
    for (int i = 0; i < n; ++i) {
        float yl = wl[(size_t) i], yr = wr[(size_t) i];
        if (bkOn && bl && br && bLen > 0) {
            if (bPos >= bLen) { if (p.bkLoop.load()) bPos = 0; else p.bkPlaying.store(false); }
            if (bPos < bLen) { yl += bl[bPos] * bg; yr += br[bPos] * bg; bPos++; }
        }
        if (metro) {
            if (mCount == 0) { mBeat = (mBeat + 1) % beats; mClick = 0; mAccent = mBeat == 0; }
            if (++mCount >= beatLen) mCount = 0;
            if (mClick < clickLen) {
                const float t = (float) mClick;
                // woodblock-like: a pitched tone plus its fifth below for body; accent is higher and louder
                const float f = mAccent ? 1500.f : 1000.f, env = std::exp(-t * clickDecay) * mg * (mAccent ? 1.f : 0.75f);
                const float s = (std::sin(2 * kPi * f * t / sr) * 0.7f + std::sin(2 * kPi * f * 0.667f * t / sr) * 0.3f) * env;
                yl += s; yr += s; mClick++;
            }
        }
        outL[i] = mute ? 0 : std::min(std::max(yl, -1.0f), 1.0f);
        outR[i] = mute ? 0 : std::min(std::max(yr, -1.0f), 1.0f);
    }
    if (bkOn) p.bkPos.store(bPos);
    p.metroBeat.store(metro ? mBeat : -1);
    for (int i = n; i < frames; ++i) { outL[i] = 0; outR[i] = 0; }
}

// MARK: - Pitch shifter

PitchShifter::PitchShifter(double sr) {
    L = (int) (sr * 0.010); back = L / 2; ahead = L - back; fadeLen = (int) (sr * 0.006); jMin = (int) (sr * 0.0025); jMinChord = (int) (sr * 0.004);
    jMax = (int) (sr * 0.030); margin = (int) (sr * 0.0008); estEvery = (int) (sr * 0.006);
    period = (int) (sr * 0.008);
    fade = fadeLen;
    buf.assign(size * 2, 0.0f);
    scratch.assign((size_t) jMax + 1, 0.0f);
}

void PitchShifter::setRatio(double r) {
    if (r == ratio) return;
    if (ratio == 1) { read = (double) w - (double) (ahead + period + margin + 8); fade = fadeLen; avgDelay = (double) (ahead + period); }
    ratio = std::abs(r - 1) < 1e-6 ? 1 : r;
}

float PitchShifter::interp(double pos) const {
    const int i = (int) std::floor(pos); const float f = (float) (pos - (double) i);
    const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2 * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

float PitchShifter::process(float x) {
    write(x);
    if (ratio == 1) return x;
    if (++estCount >= estEvery) {
        estCount = 0;
        period = bestJump(w - ahead, true, jMax);
        periodScore = lastScore;
    }
    double d = (double) w - read;
    if (d < 4 || d > (double) (size - 2 * jMax)) {
        read = (double) w - (double) (ahead + period + margin + 8); fade = fadeLen; d = (double) w - read;
    }
    if (fade >= fadeLen) {
        const bool periodic = periodScore > 0.96f;
        if (ratio > 1) {
            if (d <= (double) (std::max(ahead, (int) ((double) fadeLen * (ratio - 1))) + 8)) {
                const int j = bestJump((int) read, true, jMax, (int) ((double) fadeLen * (ratio - 1)) + 2, periodic);
                oldRead = read; read -= (double) j; fade = 0;
            }
        } else if (d >= (double) ((periodic ? std::min(period + margin, jMax) : jMax) + ahead + 8)) {
            const int j = bestJump((int) read, false, std::min(jMax, (int) d - ahead - 8), 0, periodic);
            oldRead = read; read += (double) j; fade = 0;
        }
    }
    float y = interp(read);
    if (fade < fadeLen) {
        const float g = (float) (0.5 - 0.5 * std::cos(3.141592653589793 * (double) fade / (double) fadeLen));
        y = y * g + interp(oldRead) * (1 - g);
        oldRead += ratio; fade++;
    }
    read += ratio;
    avgDelay += (d - avgDelay) * 0.0005;
    return y;
}

/// Best splice distance: the shortest lag whose normalised correlation is a peak within 3% of the best one.
int PitchShifter::bestJump(int base, bool up, int maxJ, int minJ, bool preferShort) {
    const int lo = std::min(std::max(preferShort ? jMin : jMinChord, minJ), jMax);
    if (maxJ < lo) return std::max(maxJ, 1);
    // the L-sample match window is centred on the splice point: back samples before it, ahead after
    const float* ref = buf.data() + ((base - back) & mask);
    float re = 0;
    for (int k = 0; k < L; ++k) re += ref[k] * ref[k];
    if (re < 1e-10f) { lastScore = 0; return std::min(std::max(period, lo), maxJ); }
    int best = lo, firstGood = -1; float bestScore = -1e30f;
    const int count = maxJ - lo + 1;
    for (int k = 0; k < count; ++k) {
        const int j = lo + k;
        const float* cand = buf.data() + (((up ? base - j : base + j) - back) & mask);
        float dot = 0, e = 0;
        for (int t = 0; t < L; ++t) { dot += ref[t] * cand[t]; e += cand[t] * cand[t]; }
        const float s = dot / std::sqrt(e * re + 1e-12f);
        scratch[(size_t) k] = s;
        if (s > bestScore) { bestScore = s; best = j; }
    }
    const float thr = bestScore - 0.03f * std::abs(bestScore);
    if (preferShort && count >= 3) {
        for (int k = 1; k < count - 1; ++k)
            if (scratch[(size_t) k] >= thr && scratch[(size_t) k] >= scratch[(size_t) k - 1] && scratch[(size_t) k] >= scratch[(size_t) k + 1]) {
                firstGood = lo + k; break;
            }
    }
    const int pick = firstGood > 0 ? firstGood : best;
    lastScore = scratch[(size_t) (pick - lo)];
    return pick;
}

// MARK: - Cabinet impulse responses

namespace {
using BQ = Biquad;
std::vector<Biquad> cab4x12Modern(double mic, double sr) {
    const double lp = 7200 - mic * 3400;
    return { BQ(BQ::hp, 70, sr, 1.0), BQ(BQ::peak, 110, sr, 1.2, 4), BQ(BQ::peak, 220, sr, 1.0, 1.5),
             BQ(BQ::peak, 1400, sr, 1.2, -1), BQ(BQ::peak, 2600, sr, 1.8, 4), BQ(BQ::peak, 3900, sr, 3, -2.5),
             BQ(BQ::peak, 5000, sr, 4, 2), BQ(BQ::lp, lp, sr, 0.54), BQ(BQ::lp, lp, sr, 1.31), BQ(BQ::lp, 9500, sr) };
}
std::vector<Biquad> cab4x12Vintage(double mic, double sr) {
    const double lp = 6000 - mic * 2800;
    return { BQ(BQ::hp, 70, sr, 1.0), BQ(BQ::peak, 120, sr, 1.2, 4), BQ(BQ::peak, 800, sr, 0.8, 1),
             BQ(BQ::peak, 2000, sr, 1.5, 3.5), BQ(BQ::peak, 3400, sr, 2.5, -3), BQ(BQ::lp, lp, sr, 0.54), BQ(BQ::lp, lp, sr, 1.31) };
}
std::vector<Biquad> cab2x12Open(double mic, double sr) {
    const double lp = 7800 - mic * 3500;
    return { BQ(BQ::hp, 90, sr, 0.7), BQ(BQ::peak, 140, sr, 1.0, 2), BQ(BQ::peak, 600, sr, 1.0, -1.5),
             BQ(BQ::peak, 2500, sr, 1.4, 4), BQ(BQ::lp, lp, sr, 0.54), BQ(BQ::lp, lp, sr, 1.31) };
}
} // namespace

const CabModel CABS[3] = { { "4x12 Modern", cab4x12Modern }, { "4x12 Vintage", cab4x12Vintage }, { "2x12 Open Back", cab2x12Open } };

std::vector<float> normaliseIR(std::vector<float> h) {
    float e = 0;
    for (float x : h) e += x * x;
    const float g = e > 0 ? 1 / std::sqrt(e) : 1;
    for (float& x : h) x *= g;
    return h;
}

std::vector<float> renderCab(const CabModel& model, double mic, double sr, int length) {
    auto chain = model.build(mic, sr);
    std::vector<float> h((size_t) length, 0.0f);
    for (int i = 0; i < length; ++i) {
        float x = i == 0 ? 1.f : 0.f;
        for (auto& bq : chain) x = bq.run(x);
        h[(size_t) i] = x;
    }
    const int f = length / 8;   // gentle fade so the truncated tail doesn't click
    for (int i = 0; i < f; ++i) h[(size_t) (length - 1 - i)] *= (float) i / (float) f;
    return normaliseIR(h);
}

std::vector<float> resample(const std::vector<float>& x, double from, double to) {
    if (std::abs(from - to) < 1 || x.empty()) return x;
    const double ratio = from / to; const int n = (int) ((double) x.size() / ratio);
    std::vector<float> out((size_t) std::max(n, 0));
    for (int i = 0; i < n; ++i) {
        const double pp = (double) i * ratio; const int i0 = (int) pp; const float fr = (float) (pp - (double) i0);
        const float a = x[(size_t) std::min(i0, (int) x.size() - 1)], b = x[(size_t) std::min(i0 + 1, (int) x.size() - 1)];
        out[(size_t) i] = a + (b - a) * fr;
    }
    return out;
}

// MARK: - Pitch detection (McLeod NSDF)

std::optional<double> detectPitch(const std::vector<float>& x, double sr) {
    const int n = (int) x.size();
    float ms = 0;
    for (float v : x) ms += v * v;
    ms /= (float) std::max(n, 1);
    if (std::sqrt(ms) < 0.003f) return std::nullopt;
    const int minT = (int) (sr / 1100), maxT = std::min((int) (sr / 50), n / 2);
    std::vector<float> sq((size_t) n + 1, 0.0f), nsdf((size_t) maxT + 2, 0.0f);
    for (int i = 0; i < n; ++i) sq[(size_t) i + 1] = sq[(size_t) i] + x[(size_t) i] * x[(size_t) i];
    for (int t = minT; t <= maxT + 1; ++t) {
        float ac = 0;
        for (int i = 0; i < n - t; ++i) ac += x[(size_t) i] * x[(size_t) (i + t)];
        const float m = sq[(size_t) (n - t)] + (sq[(size_t) n] - sq[(size_t) t]);
        nsdf[(size_t) t] = m > 0 ? 2 * ac / m : 0;
    }
    int t = minT;
    while (t <= maxT && nsdf[(size_t) t] > 0) t++;
    std::vector<int> peaks;
    for (; t <= maxT; ++t)
        if (nsdf[(size_t) t] > 0 && nsdf[(size_t) t] > nsdf[(size_t) t - 1] && nsdf[(size_t) t] >= nsdf[(size_t) t + 1]) peaks.push_back(t);
    float top = 0;
    for (int pk : peaks) top = std::max(top, nsdf[(size_t) pk]);
    if (peaks.empty() || top <= 0.6f) return std::nullopt;
    int best = -1;
    for (int pk : peaks) if (nsdf[(size_t) pk] >= top * 0.9f) { best = pk; break; }
    if (best < 0) return std::nullopt;
    const float a = nsdf[(size_t) best - 1], b = nsdf[(size_t) best], c = nsdf[(size_t) best + 1], d = a - 2 * b + c;
    const float shift = d != 0 ? 0.5f * (a - c) / d : 0;
    return sr / ((double) best + (double) shift);
}

} // namespace lev
