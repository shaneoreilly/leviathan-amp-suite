// Renders fixed test scenarios through the C++ engine and writes raw float32 (L block then R block) per scenario.
// tools/compare_with_swift.sh renders the same scenarios through mac/Sources/DSP.swift and compares them.
//
//   dsp_render <out-dir>
#include "Dsp.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace lev;

static float testInput(int i, double sr) {
    const double t = i / sr;
    // a decaying low E power chord, then a chirp, then silence (gate tail)
    double x = 0;
    if (t < 1.0) {
        for (double f : { 82.41, 123.47, 164.81 }) x += std::sin(2 * 3.141592653589793 * f * t);
        x *= 0.12 * std::exp(-t * 1.5);
    } else if (t < 2.0) {
        const double u = t - 1.0, f = 100 * std::pow(40.0, u);   // 100 Hz .. 4 kHz
        x = 0.05 * std::sin(2 * 3.141592653589793 * 100 * (std::pow(40.0, u) - 1) / std::log(40.0));
        (void) f;
    }
    return (float) x;
}

struct Scenario { const char* name; void (*setup)(Params&); std::vector<float> (*ir)(double sr); bool nam = false; };

static std::vector<float> builtinCab(double sr) { return renderCab(CABS[0], 0.3, sr); }

static const Scenario scenarios[] = {
    { "hot_full", [](Params& p) {
        p.inGain = 9.65f; p.gateOn = true; p.gateThresh = -48; p.gateRelease = 90;
        p.chain[0] = (int) PedalKind::od; p.chainCount = 1; p.odOn = true; p.odDrive = 0.85f; p.odTone = 0.7f; p.odLevel = 0.5f;
        p.ampOn = true; p.ampChannel = 2; p.ampGain = 0.65f; p.bass = 0.57f; p.mid = 0.35f; p.treble = 0.7f; p.presence = 0.8f; p.depth = 0.2f; p.ampVolume = 0.6f;
        p.eqOn = true; { float e[8] = { 0, 0, -3.5f, -4.9f, -4.3f, -1.8f, 0, 0 }; for (int i = 0; i < 8; ++i) p.eq[i] = e[i]; }
        p.cabOn = true; p.cabLowCut = 80; p.cabHighCut = 10000;
        p.delayOn = true; p.delayTime = 375; p.delayMix = 0.25f; p.revOn = true; p.revSize = 0.6f; p.revMix = 0.2f; p.masterVol = -3;
    }, builtinCab },
    { "clean_mod", [](Params& p) {
        int ch[] = { 0, 1, 2, 4, 5, 6, 7 };
        for (int i = 0; i < 7; ++i) p.chain[i] = ch[i];
        p.chainCount = 7;
        p.compOn = p.wahOn = p.fuzzOn = p.chorusOn = p.flangerOn = p.phaserOn = p.tremOn = true;
        p.wahAuto = true; p.fuzzAmt = 0.3f; p.tremShape = 0.4f;
        p.ampOn = true; p.ampChannel = 0; p.ampGain = 0.4f; p.eqOn = false; p.cabOn = true; p.masterVol = -6;
    }, builtinCab },
    { "rust_pitch", [](Params& p) {
        p.pitchOn = true; p.pitchSemis = -2; p.pitchCents = 10; p.pitchMix = 1;
        p.chainCount = 0; p.ampOn = true; p.ampChannel = 1; p.ampGain = 0.7f; p.eqOn = false; p.cabOn = true; p.masterVol = -6;
    }, builtinCab },
    { "amp_only", [](Params& p) {
        p.inGain = 9.65f; p.gateOn = false; p.chainCount = 0;
        p.ampOn = true; p.ampChannel = 2; p.ampGain = 0.65f; p.eqOn = false; p.cabOn = false; p.masterVol = -3;
    }, nullptr },
    { "od_only", [](Params& p) {
        p.gateOn = false; p.chain[0] = (int) PedalKind::od; p.chainCount = 1; p.odDrive = 0.85f;
        p.ampOn = false; p.eqOn = false; p.cabOn = false; p.masterVol = -3;
    }, nullptr },
    { "cab_only", [](Params& p) {
        p.gateOn = false; p.chainCount = 0; p.ampOn = false; p.eqOn = false; p.cabOn = true; p.masterVol = 0;
    }, builtinCab },
    { "gate_only", [](Params& p) {
        p.inGain = 9.65f; p.gateOn = true; p.gateThresh = -48; p.gateRelease = 90;
        p.chainCount = 0; p.ampOn = false; p.eqOn = false; p.cabOn = false; p.masterVol = 0;
    }, nullptr },
    { "nam_capture", [](Params& p) {
        p.inGain = 6; p.gateOn = true; p.gateThresh = -50; p.chain[0] = (int) PedalKind::od; p.chainCount = 1; p.odDrive = 0.3f;
        p.ampOn = true; p.namOn = true; p.namIn = 3; p.namOut = -2; p.eqOn = true; p.cabOn = true; p.masterVol = -6;
    }, builtinCab, true },
    { "metronome", [](Params& p) {
        p.ampOn = false; p.cabOn = false; p.eqOn = false; p.chainCount = 0; p.gateOn = false;
        p.metroOn = true; p.bpm = 133; p.beatsPerBar = 3; p.metroVol = -6;
    }, nullptr },
};

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    const double sr = 48000;
    const int total = (int) (sr * 3), block = 128;
    for (const auto& sc : scenarios) {
        Params p;
        sc.setup(p);
        p.version++;
        DSP d(sr, p);
        if (sc.ir) d.setIR(sc.ir(sr));
        LevNam* nm = nullptr;
        if (sc.nam) {   // NAM's example model, path passed as argv[2]
            char err[256] = {};
            nm = lev_nam_load(argv[2], err, sizeof err);
            if (!nm) { std::fprintf(stderr, "nam: %s\n", err); return 1; }
            lev_nam_reset(nm, sr, DSP::maxN);
            d.setNam(nm, 0.5f);
        }
        std::vector<float> in((size_t) block), l((size_t) total), r((size_t) total);
        for (int t = 0; t < total; t += block) {
            for (int i = 0; i < block; ++i) in[(size_t) i] = testInput(t + i, sr);
            d.process(in.data(), l.data() + t, r.data() + t, block);
        }
        FILE* f = std::fopen((dir + "/" + sc.name + ".cpp.f32").c_str(), "wb");
        if (!f) return 1;
        std::fwrite(l.data(), sizeof(float), l.size(), f);
        std::fwrite(r.data(), sizeof(float), r.size(), f);
        std::fclose(f);
        lev_nam_free(nm);
    }
    return 0;
}
