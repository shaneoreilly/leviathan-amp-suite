// Windows (MSVC) only: picks the fastest NAM build this CPU can run, once, before the first capture loads.
//
// NAM spends almost all its time in Eigen's matrix maths. MSVC's default target is SSE2, which leaves AVX2 and FMA
// unused on nearly every PC from the last ten years, and NAM then costs about twice what it should (issue #1).
// Building the whole app for AVX2 would crash on CPUs without it (older ones, and Pentium / Celeron parts up to
// about 2020), so:
//   - the app links an SSE2 copy of NAM, compiled with its lev_nam_* functions renamed to levnam_sse2_*;
//   - levnam_avx2.dll, next to the exe, holds a second copy built with /arch:AVX2;
//   - the lev_nam_* functions the app calls (below) forward to the DLL when the CPU and OS support AVX2 and FMA
//     and the DLL loads, and to the SSE2 copy otherwise.
// The two copies live in different modules, so none of their code (Eigen's templates included) can mix.
// A LevNam from one copy is never passed to the other: the choice is made once and kept for the whole run.
// LEVIATHAN_NAM=sse2 in the environment forces the SSE2 copy, to compare the two on the same machine.
#ifdef _MSC_VER
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "lev_nam.h"

extern "C" {
LevNam* levnam_sse2_load(const char*, char*, int);
void levnam_sse2_free(LevNam*);
void levnam_sse2_reset(LevNam*, double, int);
void levnam_sse2_process(LevNam*, const float*, float*, int);
double levnam_sse2_expected_rate(const LevNam*);
int levnam_sse2_has_loudness(const LevNam*);
double levnam_sse2_loudness(const LevNam*);
double levnam_sse2_measure_level(LevNam*, double);
double levnam_sse2_measure_level_in(LevNam*, double, float);
int levnam_sse2_polarity(LevNam*, double);
int levnam_sse2_has_input_level(LevNam*);
double levnam_sse2_input_level(LevNam*);
}

namespace {

struct Api {
    decltype(&levnam_sse2_load) load = levnam_sse2_load;
    decltype(&levnam_sse2_free) release = levnam_sse2_free;
    decltype(&levnam_sse2_reset) reset = levnam_sse2_reset;
    decltype(&levnam_sse2_process) process = levnam_sse2_process;
    decltype(&levnam_sse2_expected_rate) expected_rate = levnam_sse2_expected_rate;
    decltype(&levnam_sse2_has_loudness) has_loudness = levnam_sse2_has_loudness;
    decltype(&levnam_sse2_loudness) loudness = levnam_sse2_loudness;
    decltype(&levnam_sse2_measure_level) measure_level = levnam_sse2_measure_level;
    decltype(&levnam_sse2_measure_level_in) measure_level_in = levnam_sse2_measure_level_in;
    decltype(&levnam_sse2_polarity) polarity = levnam_sse2_polarity;
    decltype(&levnam_sse2_has_input_level) has_input_level = levnam_sse2_has_input_level;
    decltype(&levnam_sse2_input_level) input_level = levnam_sse2_input_level;
    char description[200] = "SSE2";
};

/// AVX2 and FMA in the CPU, and the OS saving the 256-bit registers on a thread switch (without that, AVX faults).
bool cpuHasAvx2Fma() {
    int r[4];
    __cpuid(r, 0);
    if (r[0] < 7) return false;
    __cpuid(r, 1);
    const bool fma = (r[2] >> 12) & 1, osxsave = (r[2] >> 27) & 1, avx = (r[2] >> 28) & 1;
    if (!(fma && osxsave && avx)) return false;
    if ((_xgetbv(0) & 6) != 6) return false;   // XMM and YMM state enabled by the OS
    __cpuidex(r, 7, 0);
    return (r[1] >> 5) & 1;                     // AVX2
}

template <typename F> bool find(HMODULE dll, const char* name, F& slot) {
    auto p = reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(dll, name)));
    if (p == nullptr) return false;
    slot = p;
    return true;
}

Api choose() {
    Api a;
    const char* force = std::getenv("LEVIATHAN_NAM");
    if (force != nullptr && _stricmp(force, "sse2") == 0) { std::snprintf(a.description, sizeof a.description, "SSE2 (LEVIATHAN_NAM=sse2)"); return a; }
    if (!cpuHasAvx2Fma()) { std::snprintf(a.description, sizeof a.description, "SSE2 (this CPU has no AVX2/FMA)"); return a; }

    wchar_t path[MAX_PATH * 2];
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH * 2);
    if (len == 0 || len >= MAX_PATH * 2) { std::snprintf(a.description, sizeof a.description, "SSE2 (app folder not found)"); return a; }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash == nullptr) { std::snprintf(a.description, sizeof a.description, "SSE2 (app folder not found)"); return a; }
    wcscpy_s(slash + 1, MAX_PATH * 2 - (slash + 1 - path), L"levnam_avx2.dll");

    HMODULE dll = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (dll == nullptr) {
        std::snprintf(a.description, sizeof a.description, "SSE2 (levnam_avx2.dll did not load: error %lu)", GetLastError());
        return a;
    }
    Api d;
    const bool ok = find(dll, "lev_nam_load", d.load) && find(dll, "lev_nam_free", d.release) && find(dll, "lev_nam_reset", d.reset)
        && find(dll, "lev_nam_process", d.process) && find(dll, "lev_nam_expected_rate", d.expected_rate)
        && find(dll, "lev_nam_has_loudness", d.has_loudness) && find(dll, "lev_nam_loudness", d.loudness)
        && find(dll, "lev_nam_measure_level", d.measure_level) && find(dll, "lev_nam_measure_level_in", d.measure_level_in)
        && find(dll, "lev_nam_polarity", d.polarity) && find(dll, "lev_nam_has_input_level", d.has_input_level)
        && find(dll, "lev_nam_input_level", d.input_level);
    if (!ok) {
        FreeLibrary(dll);
        std::snprintf(a.description, sizeof a.description, "SSE2 (levnam_avx2.dll is missing functions)");
        return a;
    }
    std::snprintf(d.description, sizeof d.description, "AVX2+FMA (levnam_avx2.dll)");
    return d;   // the DLL stays loaded for the life of the process
}

/// Chosen on first use (a capture load, or the diagnostic log asking), then fixed. A function-local static, so
/// the audio thread's lev_nam_process only reads it after the load that preceded it has initialised it.
const Api& api() {
    static const Api a = choose();
    return a;
}

} // namespace

extern "C" {
LevNam* lev_nam_load(const char* path, char* err, int errLen) { return api().load(path, err, errLen); }
void lev_nam_free(LevNam* m) { if (m) api().release(m); }
void lev_nam_reset(LevNam* m, double sampleRate, int maxBlock) { api().reset(m, sampleRate, maxBlock); }
void lev_nam_process(LevNam* m, const float* in, float* out, int frames) { api().process(m, in, out, frames); }
double lev_nam_expected_rate(const LevNam* m) { return api().expected_rate(m); }
int lev_nam_has_loudness(const LevNam* m) { return api().has_loudness(m); }
double lev_nam_loudness(const LevNam* m) { return api().loudness(m); }
double lev_nam_measure_level(LevNam* m, double sampleRate) { return api().measure_level(m, sampleRate); }
double lev_nam_measure_level_in(LevNam* m, double sampleRate, float inputGain) { return api().measure_level_in(m, sampleRate, inputGain); }
int lev_nam_polarity(LevNam* m, double sampleRate) { return api().polarity(m, sampleRate); }
int lev_nam_has_input_level(LevNam* m) { return api().has_input_level(m); }
double lev_nam_input_level(LevNam* m) { return api().input_level(m); }

/// Which NAM build is running, for the diagnostic log (Windows-only; not part of lev_nam.h).
const char* levnam_engine_description() { return api().description; }
}
#endif
