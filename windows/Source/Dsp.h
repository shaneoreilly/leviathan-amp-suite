// Leviathan's audio engine: a line-for-line port of mac/Sources/DSP.swift. No JUCE in here, so the offline
// checker (tools/dsp_render.cpp) can render through it and compare against the Swift engine.
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "lev_nam.h"

namespace lev {

constexpr float kPi = 3.14159265358979f;

// MARK: - Primitives

class Biquad {
public:
    enum Kind { lp, hp, bp, peak, lowShelf, highShelf };
    Biquad() = default;
    Biquad(Kind k, double f, double sr, double q = 0.7071, double db = 0) { set(k, f, sr, q, db); }
    void set(Kind k, double freq, double sr, double q = 0.7071, double db = 0);
    inline float run(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
private:
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

struct Smooth {
    float v = 0, target = 0;
    inline float next(float c) { v += (target - v) * c; return v; }
};

inline float dbLin(float d) { return std::pow(10.0f, d / 20.0f); }
inline float lerpf(float t, float a, float b) { return a + (b - a) * t; }

/// Single-producer / single-consumer float FIFO (audio thread writes, UI thread reads).
class Ring {
public:
    explicit Ring(int capacity) : cap(capacity), mask(capacity - 1), buf((size_t) capacity, 0.0f) {}
    void write(const float* x, int n);
    int available() const { return (int) (w.load(std::memory_order_acquire) - r.load(std::memory_order_relaxed)); }
    int read(float* dst, int n);
    void reset() { r.store(w.load()); }
private:
    const int cap, mask;
    std::vector<float> buf;
    std::atomic<int64_t> w { 0 }, r { 0 };
};

// MARK: - Shared parameters (UI thread writes, audio thread reads once per buffer)

enum class PedalKind : int { comp, wah, fuzz, od, chorus, flanger, phaser, trem, blade };
constexpr int kPedalCount = 9;
const char* pedalTitle(PedalKind k);
const char* pedalOnKey(PedalKind k);   // "compOn", "wahOn", ... (the Settings field)

struct Params {
    std::atomic<int> version { 0 };
    float inGain = 0; int inputChannel = 0; std::atomic<bool> muted { false };
    bool gateOn = true; float gateThresh = -60, gateRelease = 60, gateDepth = -60;
    bool pitchOn = false; float pitchSemis = 0, pitchCents = 0, pitchMix = 1;
    bool odOn = true; float odDrive = 0, odTone = 0.6f, odLevel = 0.75f;
    bool ampOn = true; int ampChannel = 2; float ampGain = 0.55f;
    float bass = 0.5f, mid = 0.5f, treble = 0.55f, presence = 0.55f, depth = 0.5f, ampVolume = 0.5f;
    bool eqOn = true; float eq[8] = {};
    bool cabOn = true; float cabLowCut = 80, cabHighCut = 7500, cabLevel = 0;
    bool delayOn = false; float delayTime = 420, delayFeedback = 0.28f, delayTone = 0.45f, delayMix = 0.2f;
    bool revOn = false; float revSize = 0.45f, revMix = 0.18f;
    bool doublerOn = false; float doublerWidth = 0.6f, doublerTime = 20;
    float masterVol = -6;

    static constexpr int maxChain = 9;
    int chain[maxChain] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int chainCount = 0;
    bool compOn = true; float compSustain = 0.5f, compAttack = 10, compLevel = 0, compBlend = 1;
    bool wahOn = true, wahAuto = false; float wahPos = 0.5f, wahSens = 0.5f, wahQ = 0.5f;
    bool fuzzOn = true; float fuzzAmt = 0.6f, fuzzTone = 0.5f, fuzzLevel = 0.5f;
    bool bladeOn = true; float bladeDrive = 0.5f, bladeLow = 0.5f, bladeMid = 0.5f, bladeHigh = 0.5f, bladeLevel = 0.5f;
    float bladeBoost = 0.5f; bool bladeBoostOn = false, bladeShift = false; int bladeDiode = 0;
    bool chorusOn = true; float chorusRate = 0.8f, chorusDepth = 0.5f, chorusMix = 0.5f;
    bool flangerOn = true; float flangerRate = 0.25f, flangerDepth = 0.7f, flangerFb = 0.5f, flangerMix = 0.5f;
    bool phaserOn = true; float phaserRate = 0.5f, phaserDepth = 0.7f, phaserFb = 0.4f;
    bool tremOn = true; float tremRate = 5, tremDepth = 0.6f, tremShape = 0;

    // captured amp (replaces the voicings when capOn; its IR goes into the cab slot)
    bool capOn = false; float capHP = 80, capLS = 0, capPkF = 900, capPk = 0, capHS = 0;
    float capK1 = 1, capB1 = 0, capMid = 6000, capK2 = 1, capB2 = 0, capDrive = 0, capLevel = 0;

    // NAM capture (replaces the amp when namOn and a model is loaded); input/output trims in dB
    bool namOn = false; float namIn = 0, namOut = 0;
    // Amp B: a second NAM capture fed by the same pedals; Spread pans A left and B right (mid/side, mono-safe)
    bool ampBOn = false; float ampBLevel = 0, ampBSpread = 1;

    // metronome (mixed after the master, never through the amp)
    std::atomic<bool> metroOn { false }; float bpm = 120; int beatsPerBar = 4; float metroVol = -12;

    // recorder: raw input + final stereo output, drained by the UI thread
    std::atomic<bool> recording { false };
    Ring recDry { 1 << 18 }, recL { 1 << 18 }, recR { 1 << 18 };

    // reamp source: replaces the live input while playing (buffers owned by the UI side, swapped with care)
    std::atomic<const float*> srcPtr { nullptr }; std::atomic<int> srcLen { 0 }, srcPos { 0 };
    std::atomic<bool> srcPlaying { false }, srcDone { false }, srcLoop { false };

    // backing track: stereo, mixed after the master
    std::atomic<const float*> bkL { nullptr }, bkR { nullptr }; std::atomic<int> bkLen { 0 }, bkPos { 0 };
    std::atomic<int> bkSeek { -1 };   // jump requested by the UI, applied by the audio thread at the start of a block
    std::atomic<bool> bkPlaying { false }, bkLoop { true }; float bkVol = -6;

    // written by audio thread, read by UI
    std::atomic<float> inPeak { 0 }, outPeak { 0 }, pitchLatencyMs { 0 };
    std::atomic<bool> gateOpen { false };
    std::atomic<int> inClips { 0 };   // raw input samples at full scale (the interface clipping), ever-increasing
    std::atomic<int> metroBeat { -1 };
    float tunerRing[4096] = {};
    std::atomic<int> tunerWrite { 0 };
};

// MARK: - Pedals

class Pedal {
public:
    explicit Pedal(float sampleRate) : sr(sampleRate) {}
    virtual ~Pedal() = default;
    virtual void update(const Params&) {}
    virtual float tick(float x) { return x; }
    void process(float* a, int n, float c);
protected:
    const float sr;
    Smooth mix;
};
std::unique_ptr<Pedal> makePedal(PedalKind k, float sr);

// MARK: - Amp voicings

struct Voicing {
    const char* name;
    double hp, tight, emph;
    float preMax, b1;
    bool cascade; float s2, b2, s3;
    double interHP, lp1, lp2, lp3;
    double mid, character; float power, out;
};
extern const Voicing VOICINGS[3];

// MARK: - Pitch shifter (see DSP.swift for the design notes)

class PitchShifter {
public:
    explicit PitchShifter(double sr);
    void setRatio(double r);
    inline void write(float x) { int i = w & mask; buf[(size_t) i] = x; buf[(size_t) (i + size)] = x; w++; }
    float process(float x);
    double ratio = 1.0, avgDelay = 0.0;
private:
    static constexpr int size = 32768, mask = 32767;   // room for the 30 ms search at 192 kHz
    std::vector<float> buf, scratch;
    int w = 0;
    double read = 0, oldRead = 0;
    int fade;
    int fadeLen, L, back, ahead, jMin, jMinChord, jMax, margin, period, estCount = 0, estEvery;
    float periodScore = 0, lastScore = 0;
    inline float at(int i) const { return buf[(size_t) (i & mask)]; }
    float interp(double pos) const;
    int bestJump(int base, bool up, int maxJ, int minJ = 0, bool preferShort = true);
};

// MARK: - Reverb

/// Stereo reverb (mac/Sources/DSP.swift FDNReverb, operation for operation): 12 ms pre-delay, four allpass
/// diffusers, an 8-line feedback delay network mixed by a Householder matrix with damping in each line.
struct FDNReverb {
    static constexpr int lineLens[8] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };   // at 44.1 kHz
    static constexpr int diffLens[4] = { 142, 107, 379, 277 };
    static constexpr float outScale = 3.8f;   // as loud as the Freeverb it replaces at the same Mix
    explicit FDNReverb(double sr) {
        const double scale = sr / 44100;
        for (int i = 0; i < 8; ++i) { lens[i] = std::max((int) ((double) lineLens[i] * scale), 1); lines[i].assign((size_t) lens[i], 0.0f); }
        for (int k = 0; k < 4; ++k) { dlens[k] = std::max((int) ((double) diffLens[k] * scale), 1); dif[k].assign((size_t) dlens[k], 0.0f); }
        preLen = std::max((int) (sr * 0.012), 2); pre.assign((size_t) preLen, 0.0f);
    }
    void set(float size, float sr) {
        const float rt = 0.35f + size * size * 5;
        for (int i = 0; i < 8; ++i) g[i] = std::pow(10.0f, -3 * (float) lens[i] / (rt * sr));
        damp = 0.15f + 0.35f * size;
    }
    inline void process(float x, float& l, float& r) {
        pre[(size_t) prePos] = x;
        int pr = prePos + 1; if (pr == preLen) pr = 0;
        prePos = pr;
        float v = pre[(size_t) pr];
        for (int k = 0; k < 4; ++k) {
            const float b = dif[k][(size_t) dpos[k]], y = b - 0.6f * v;
            dif[k][(size_t) dpos[k]] = v + 0.6f * y;
            if (++dpos[k] == dlens[k]) dpos[k] = 0;
            v = y;
        }
        float sum = 0;
        for (int i = 0; i < 8; ++i) {
            lp[i] += (lines[i][(size_t) pos[i]] - lp[i]) * (1 - damp);
            o[i] = lp[i] * g[i];
            sum += o[i];
        }
        const float h = sum * 0.25f;
        float ll = 0, rr = 0;
        for (int i = 0; i < 8; ++i) {
            const float y = lines[i][(size_t) pos[i]];
            switch (i % 4) { case 0: ll += y; break; case 1: rr += y; break; case 2: ll -= y; break; default: rr -= y; break; }
            lines[i][(size_t) pos[i]] = o[i] - h + v + 1e-20f;   // same input to every line; the output signs decorrelate L and R
            if (++pos[i] == lens[i]) pos[i] = 0;
        }
        l = ll * outScale; r = rr * outScale;
    }
    std::vector<float> lines[8], dif[4], pre;
    int lens[8] {}, pos[8] {}, dlens[4] {}, dpos[4] {}, preLen = 2, prePos = 0;
    float lp[8] {}, g[8] {}, o[8] {}, damp = 0.3f;
};

// MARK: - Engine

class DSP {
public:
    DSP(double sampleRate, Params& params);
    static constexpr int maxN = 4096, maxIR = 4096;
    const float sr;

    /// Swap in a new cabinet impulse response (UI thread).
    void setIR(const std::vector<float>& h);
    /// Swap in a NAM model (already reset for this rate and maxN), with its loudness-normalising gain.
    /// The caller keeps ownership and frees the previous model once the audio thread has moved on.
    void setNam(LevNam* m, float gain) { namGain.store(gain); nam.store(m, std::memory_order_release); }
    void setNamB(LevNam* m, float gain) { namBGain.store(gain); namB.store(m, std::memory_order_release); }
    void process(const float* input, float* outL, float* outR, int frames);

private:
    Params& p;
    int lastVersion = -1;
    std::vector<float> a, b, wl, wr, gBuf;   // gBuf: gate gain per sample, for the post-cab stage
    const float fade;

    Smooth inG; float env = 0, gGain = 1, gTarget = 1; int gHold = 0;
    float gThr = 0, gRelC = 0, gEnvR = 0, gAttC = 0, gFloor = 0;

    PitchShifter shifter; Smooth pMix;
    std::vector<std::unique_ptr<Pedal>> pedals;

    int mCount = 0, mBeat = -1, mClick = 1 << 30; bool mAccent = false, mWas = false;

    Biquad aHP, aTight, aEmph, up[4], down[4], aLP1, aIHP, aLP2, aLP3, aChar, aBass, aMid, aTreb, aDepth, aPres, aDC;
    Smooth ampMix, aPre, aVol;
    Voicing v = VOICINGS[2]; float tb1 = 0, tb2 = 0;

    std::atomic<LevNam*> nam { nullptr }; std::atomic<float> namGain { 1 };
    // Amp B: its capture, the pre-amp copy it runs on, and the side signal (A - B) with its own EQ, cab and filter state
    std::atomic<LevNam*> namB { nullptr }; std::atomic<float> namBGain { 1 };
    Smooth bMix, bLvl, bSpread; bool sideWasOn = false;
    std::vector<float> bBuf = std::vector<float>(maxN, 0.0f), sBuf = std::vector<float>(maxN, 0.0f), sConv = std::vector<float>(maxN, 0.0f),
                       hist2 = std::vector<float>(maxIR + maxN, 0.0f);
    Biquad eqS[8], cLoS, cHi1S, cHi2S;
    Smooth namInG, namOutG; std::vector<float> namBuf;

    Biquad kHP, kLS, kPk, kHS, kMid;
    bool capOn = false; float kK1 = 1, kB1 = 0, kK2 = 1, kB2 = 0, kT1 = 0, kT2 = 0; Smooth kLvl;

    static constexpr double eqF[8] = { 80, 160, 320, 640, 1250, 2500, 5000, 10000 };
    Biquad eqB[8]; Smooth eqMix;

    // cab: direct-form FIR over a history buffer; IRs stored reversed and double-buffered
    std::vector<float> irs[2]; int irLen[2] = { 1, 0 }; std::atomic<int> irActive { 0 };
    std::vector<float> hist;
    Biquad cLo, cHi1, cHi2; Smooth cabMix, cabLvl;

    std::vector<float> dBuf; int dSize, dW = 0; Smooth dTime, dSend; float dFb = 0, dWet = 0; Biquad dLP;

    FDNReverb reverb; Smooth rSend; float rWet = 0;
    // doubler: the side signal is the difference of two slowly drifting delayed copies (mid stays dry, mono-safe)
    static constexpr int dbSize = 16384, dbMask = 16383;   // 40 ms + drift fits at 192 kHz
    std::vector<float> dbBuf = std::vector<float>(dbSize, 0.0f);
    int dbW = 0; float dbPh1 = 0, dbPh2 = 0, dbInc1 = 0, dbInc2 = 0; Smooth dbWidth, dbTime;

    // output limiter: stops a hot Master from hard-clipping (which sounds like crackle)
    float limG = 1, limRel = 0;
    static constexpr float limCeiling = 0.891f;   // -1 dBFS

    void update(bool force = false);
};

// MARK: - Cabinets, IR helpers, pitch detection

struct CabModel { const char* name; std::vector<Biquad> (*build)(double mic, double sr); };
extern const CabModel CABS[3];
constexpr int kCabCount = 3;
constexpr int CAB_USER = kCabCount, CAB_LIB = kCabCount + 1, CAB_IRLIB = kCabCount + 2;

std::vector<float> normaliseIR(std::vector<float> h);
std::vector<float> renderCab(const CabModel& model, double mic, double sr, int length = 2048);
std::vector<float> resample(const std::vector<float>& x, double from, double to);
std::optional<double> detectPitch(const std::vector<float>& x, double sr);

} // namespace lev
