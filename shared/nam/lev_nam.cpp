// See lev_nam.h. Built with NAM_SAMPLE_FLOAT so the engine's float buffers go straight in.
#include "lev_nam.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

struct LevNam {
    std::unique_ptr<nam::DSP> dsp;
    std::vector<float> scratch;   // NAM may not support in-place processing
    int maxBlock = 0;
};

static void setErr(char* err, int len, const std::string& msg) {
    if (err && len > 0) { std::strncpy(err, msg.c_str(), (size_t) len - 1); err[len - 1] = 0; }
}

extern "C" LevNam* lev_nam_load(const char* path, char* err, int errLen) {
    try {
        auto dsp = nam::get_dsp(std::filesystem::u8path(path));
        if (!dsp) { setErr(err, errLen, "not a NAM model"); return nullptr; }
        if (dsp->NumInputChannels() != 1 || dsp->NumOutputChannels() != 1) { setErr(err, errLen, "only mono NAM models are supported"); return nullptr; }
        auto* m = new LevNam();
        m->dsp = std::move(dsp);
        return m;
    } catch (const std::exception& e) {
        setErr(err, errLen, e.what());
    } catch (...) {
        setErr(err, errLen, "could not read the model");
    }
    return nullptr;
}

extern "C" void lev_nam_free(LevNam* m) { delete m; }

extern "C" void lev_nam_reset(LevNam* m, double sampleRate, int maxBlock) {
    if (!m) return;
    m->maxBlock = maxBlock;
    m->scratch.assign((size_t) maxBlock, 0.0f);
    m->dsp->Reset(sampleRate, maxBlock);   // also prewarms, so the first block isn't a click
}

extern "C" void lev_nam_process(LevNam* m, const float* in, float* out, int frames) {
    if (!m || frames <= 0) return;
    for (int off = 0; off < frames; off += m->maxBlock) {
        const int n = frames - off < m->maxBlock ? frames - off : m->maxBlock;
        std::memcpy(m->scratch.data(), in + off, (size_t) n * sizeof(float));
        float* ip = m->scratch.data();
        float* op = out + off;
        m->dsp->process(&ip, &op, n);
    }
}

extern "C" double lev_nam_measure_level(LevNam* m, double sampleRate) {
    if (!m || m->maxBlock <= 0) return 0;
    // a decaying low power chord with pick noise at a typical DI level (about -18 dBFS RMS while it rings)
    const int n = (int) sampleRate;
    std::vector<float> x((size_t) n), y((size_t) n);
    uint32_t seed = 12345;
    for (int i = 0; i < n; ++i) {
        const double t = i / sampleRate, env = std::exp(-t * 2.0);
        double v = 0;
        for (double f : { 82.41, 123.47, 164.81 }) v += std::sin(2 * 3.141592653589793 * f * t);
        seed = seed * 1664525u + 1013904223u;
        const double noise = ((seed >> 8) / 16777216.0 - 0.5) * std::exp(-t * 30.0);
        x[(size_t) i] = (float) ((v / 3 * 0.3 + noise * 0.2) * env * 2.45);
    }
    lev_nam_process(m, x.data(), y.data(), n);
    double e = 0;
    for (int i = n / 10; i < n; ++i) e += (double) y[(size_t) i] * y[(size_t) i];
    lev_nam_reset(m, sampleRate, m->maxBlock);   // back to a clean state for real use
    return 10 * std::log10(e / (n - n / 10) + 1e-20);
}

extern "C" int lev_nam_polarity(LevNam* m, double sampleRate) {
    if (!m || m->maxBlock <= 0) return 1;
    // the same guitar-like chord as lev_nam_measure_level; the sign of the strongest input/output correlation over
    // 0-2 ms of lag says whether the capture inverts the signal (some real amps do)
    const int n = (int) (sampleRate * 0.5), maxLag = (int) (sampleRate * 0.002);
    std::vector<float> x((size_t) n), y((size_t) n);
    for (int i = 0; i < n; ++i) {
        const double t = i / sampleRate;
        double v = 0;
        for (double f : { 82.41, 123.47, 164.81 }) v += std::sin(2 * 3.141592653589793 * f * t);
        x[(size_t) i] = (float) (v / 3 * 0.3 * std::exp(-t * 2.0) * 2.45);
    }
    lev_nam_process(m, x.data(), y.data(), n);
    double best = 0;
    for (int lag = 0; lag <= maxLag; ++lag) {
        double c = 0;
        for (int i = n / 10; i + lag < n; ++i) c += (double) x[(size_t) i] * y[(size_t) (i + lag)];
        if (std::abs(c) > std::abs(best)) best = c;
    }
    lev_nam_reset(m, sampleRate, m->maxBlock);   // back to a clean state for real use
    return best < 0 ? -1 : 1;
}

extern "C" double lev_nam_expected_rate(const LevNam* m) { return m ? m->dsp->GetExpectedSampleRate() : 0; }
extern "C" int lev_nam_has_loudness(const LevNam* m) { return m && m->dsp->HasLoudness() ? 1 : 0; }
extern "C" double lev_nam_loudness(const LevNam* m) { return m && m->dsp->HasLoudness() ? m->dsp->GetLoudness() : 0; }
extern "C" int lev_nam_has_input_level(LevNam* m) { return m && m->dsp->HasInputLevel() ? 1 : 0; }
extern "C" double lev_nam_input_level(LevNam* m) { return m && m->dsp->HasInputLevel() ? m->dsp->GetInputLevel() : 0; }
