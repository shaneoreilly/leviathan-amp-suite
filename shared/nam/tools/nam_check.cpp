// Renders a WAV through a .nam model via Leviathan's bridge (lev_nam), in 64-sample blocks like NAM's render tool,
// so the two outputs can be compared sample for sample.   nam_check <model.nam> <in.f32> <rate> <out.f32>
#include "../lev_nam.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 5) return 1;
    char err[256] = {};
    LevNam* m = lev_nam_load(argv[1], err, sizeof err);
    if (!m) { std::fprintf(stderr, "load failed: %s\n", err); return 1; }
    std::vector<float> x; const double sr = std::atof(argv[3]);
    if (FILE* in = std::fopen(argv[2], "rb")) { float v; while (std::fread(&v, sizeof v, 1, in) == 1) x.push_back(v); std::fclose(in); }
    lev_nam_reset(m, lev_nam_expected_rate(m) > 0 ? lev_nam_expected_rate(m) : sr, 64);
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); i += 64) lev_nam_process(m, x.data() + i, y.data() + i, (int) std::min<size_t>(64, x.size() - i));
    FILE* f = std::fopen(argv[4], "wb"); std::fwrite(y.data(), sizeof(float), y.size(), f); std::fclose(f);
    std::printf("rate %.0f loudness %s%.2f\n", lev_nam_expected_rate(m), lev_nam_has_loudness(m) ? "" : "(none) ", lev_nam_loudness(m));
    lev_nam_free(m);
    return 0;
}
