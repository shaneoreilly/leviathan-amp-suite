// File-based libraries: God's Cab IRs, amp captures, WAV reading, and where Leviathan keeps things on disk.
#pragma once
#include <JuceHeader.h>
#include "Dsp.h"

namespace lev {

/// %APPDATA%\Leviathan on Windows; ~/Library/Application Support/Leviathan on macOS (shared with the Swift app).
juce::File appDataDir();
/// Documents\..\Music\Leviathan on Windows, ~/Music/Leviathan on macOS.
juce::File musicDir();

struct WavData { std::vector<std::vector<float>> channels; double rate = 0; };
/// Reads WAV/AIFF/FLAC as float channels (up to maxFrames). Returns rate 0 on failure.
WavData readAudio(const juce::File& f, int64_t maxFrames = (int64_t) DSP::maxIR * 4);
/// Reads audio converted to `rate` (or kept at its own rate when rate <= 0), all channels, up to maxSeconds.
WavData loadAudio(const juce::File& f, double rate, double maxSeconds = 900);
bool writeWav(const juce::File& f, const std::vector<const float*>& ch, int frames, double rate);

/// One God's Cab impulse, parsed from names like "57_TS_1_inch_edge_pres_3.wav".
struct IRFile { juce::String rel, mic; bool ts = false; juce::String position; int pres = 0; };

class IRLibrary {
public:
    static juce::File root() { return appDataDir().getChildFile("IRs/GodsCab"); }
    void scan(double sampleRate);
    bool isEmpty() const { return files.empty(); }
    juce::StringArray mics() const;
    const IRFile* file(const juce::String& rel) const;
    juce::StringArray positions(const juce::String& mic, bool ts) const;
    bool tsAvailable(const juce::String& mic) const;
    const IRFile* find(const juce::String& mic, bool ts, const juce::String& position, int pres) const;
    juce::String step(const juce::String& rel, int d) const;
    std::optional<std::vector<float>> samples(const juce::String& rel, double sampleRate);
    static std::optional<IRFile> parse(const juce::String& rel);
private:
    std::vector<IRFile> files;
    juce::File dir;
    std::map<juce::String, std::vector<float>> cache;
    double cacheRate = 0;
};

/// A fitted amp model written by mac/Capture/fit.py.
struct Capture {
    juce::String name;
    double sr = 48000, hp = 80, ls = 0, pk_f = 900, pk = 0, hs = 0, k1 = 1, b1 = 0, mid = 6000, k2 = 1, b2 = 0, esr_val = 1;
    std::vector<float> ir;
    static juce::File dir() { return appDataDir().getChildFile("Captures"); }
    static std::map<juce::String, Capture> loadAll();
    void apply(Params& p) const;
};

} // namespace lev
