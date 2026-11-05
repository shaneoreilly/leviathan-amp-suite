// Renders a dry DI take through every factory preset (God's Cab IRs from the user's library) and reports
// loudness and peaks, so presets can be level-matched. Writes a wet WAV per preset for listening.
//
//   preset_check <dry.wav> <out-dir>
#include <JuceHeader.h>
#include "Settings.h"
#include "Library.h"

using namespace lev;

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc == 3 && juce::String(argv[1]) == "--dump") {   // factory presets as JSON (compared with the Mac app's)
        auto* o = new juce::DynamicObject();
        for (const auto& f : FACTORY) { Settings s; f.fn(s); o->setProperty(f.name, s.toVar()); }
        juce::File(juce::String::fromUTF8(argv[2])).replaceWithText(juce::JSON::toString(juce::var(o)));
        return 0;
    }
    if (argc < 3) { std::printf("usage: preset_check <dry.wav> <out-dir> | --dump <file.json>\n"); return 1; }
    const juce::File dryFile(juce::String::fromUTF8(argv[1])), out(juce::String::fromUTF8(argv[2]));
    // NAM captures for presets that use one: LEVIATHAN_NAM_DIR, else the app's NAM folder
    const auto envNam = juce::SystemStats::getEnvironmentVariable("LEVIATHAN_NAM_DIR", {});
    const juce::File namDir = envNam.isNotEmpty() ? juce::File(envNam) : appDataDir().getChildFile("NAM");
    out.createDirectory();
    auto dry = loadAudio(dryFile, 48000);
    if (dry.channels.empty()) { std::printf("can't read %s\n", argv[1]); return 1; }
    const double sr = 48000;
    IRLibrary lib; lib.scan(sr);
    // --settings <file.json>: render one saved rig (as the Mac app stores it) instead of the factory presets
    std::vector<std::pair<juce::String, Settings>> rigs;
    if (argc >= 5 && juce::String(argv[3]) == "--settings") {
        rigs.emplace_back("current", Settings::fromJSON(juce::JSON::parse(juce::File(juce::String::fromUTF8(argv[4])).loadFileAsString())));
        for (int i = 5; i + 1 < argc; i += 2) {   // overrides: key value
            auto s2 = rigs.front().second; s2.set(argv[i], juce::String(argv[i + 1]).getDoubleValue());
            rigs.emplace_back(juce::String(argv[i]) + "=" + juce::String(argv[i + 1]), s2);
        }
    } else for (const auto& f : FACTORY) { Settings s; f.fn(s); rigs.emplace_back(f.name, s); }
    for (const auto& [rigName, rig] : rigs) {
        Settings s = rig;
        Params p;
        s.apply(p);
        DSP d(sr, p);
        std::vector<float> ir;
        if (s.i("cabModel") == CAB_LIB && !lib.isEmpty()) {
            auto a = lib.samples(s.s("irA"), sr);
            auto b = s.s("irB").isNotEmpty() ? lib.samples(s.s("irB"), sr) : std::nullopt;
            if (!a) std::printf("  MISSING IR %s\n", s.s("irA").toRawUTF8());
            if (s.s("irB").isNotEmpty() && !b) std::printf("  MISSING IR %s\n", s.s("irB").toRawUTF8());
            ir = a ? *a : renderCab(CABS[0], 0.3, sr);
            if (a && b && s.d("irBlend") > 0.001) {
                const float t = (float) s.d("irBlend");
                std::vector<float> m(std::max(ir.size(), b->size()), 0.0f);
                for (size_t i = 0; i < m.size(); ++i) m[i] = (i < ir.size() ? ir[i] : 0) * (1 - t) + (i < b->size() ? (*b)[i] : 0) * t;
                ir = m;
            }
            ir = normaliseIR(ir);
        } else {
            ir = renderCab(CABS[juce::jlimit(0, kCabCount - 1, s.i("cabModel"))], s.d("cabMic"), sr);
        }
        d.setIR(ir);
        LevNam* nam = nullptr;
        if (s.s("nam").isNotEmpty()) {
            char err[256] = {};
            nam = lev_nam_load(namDir.getChildFile(s.s("nam") + ".nam").getFullPathName().toRawUTF8(), err, sizeof err);
            if (!nam) { std::printf("  NAM %s: %s\n", s.s("nam").toRawUTF8(), err); }
            else {
                lev_nam_reset(nam, sr, DSP::maxN);
                const double lvl = lev_nam_measure_level(nam, sr);
                p.namOn = true; d.setNam(nam, (float) std::pow(10.0, (LEV_NAM_TARGET_LEVEL - lvl) / 20));
            }
        }
        const auto& x = dry.channels[0];
        const int n = (int) x.size(), block = 256;
        std::vector<float> l((size_t) n), r((size_t) n);
        for (int i = 0; i < n; i += block) d.process(x.data() + i, l.data() + i, r.data() + i, std::min(block, n - i));
        // loudness: RMS over 400 ms windows above -50 dBFS (ignores the gaps between riffs), and the true peak
        double sum = 0; int cnt = 0; float peak = 0; int clipped = 0;
        const int win = (int) (sr * 0.4);
        for (int i = 0; i + win <= n; i += win / 2) {
            double e = 0; for (int k = i; k < i + win; ++k) e += (double) l[(size_t) k] * l[(size_t) k] + (double) r[(size_t) k] * r[(size_t) k];
            e /= 2.0 * win;
            if (10 * std::log10(e + 1e-20) > -50) { sum += e; cnt++; }
        }
        for (int i = 0; i < n; ++i) { peak = std::max(peak, std::abs(l[(size_t) i])); if (std::abs(l[(size_t) i]) >= 0.999f) clipped++; }
        const double loud = cnt ? 10 * std::log10(sum / cnt) : -99;
        std::printf("%-34s loudness %6.1f dB   peak %6.1f dBFS   clipped %5.2f%%\n", rigName.toRawUTF8(), loud, 20 * std::log10(peak + 1e-9), 100.0 * clipped / n);
        writeWav(out.getChildFile(juce::File::createLegalFileName(rigName) + ".wav"), { l.data(), r.data() }, n, sr);
        lev_nam_free(nam);
    }
    return 0;
}
