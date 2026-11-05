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

/// Cab IRs you add (any WAV/AIFF, e.g. tone packs from tone3000.com), in IRs/Library (subfolders allowed).
/// Presets refer to an IR by its path inside the folder, with forward slashes, the same on Mac and Windows.
class CabIRLibrary {
public:
    static juce::File root() { return appDataDir().getChildFile("IRs/Library"); }
    void scan();
    bool isEmpty() const { return files.isEmpty(); }
    const juce::StringArray& all() const { return files; }
    bool contains(const juce::String& rel) const { return files.contains(rel); }
    juce::String step(const juce::String& rel, int d) const;
    std::optional<std::vector<float>> samples(const juce::String& rel, double sampleRate);
    /// Copies IR files, or whole folders of them, into the library. Returns how many were added.
    int importItems(const juce::Array<juce::File>& items);
    /// Display name: the file name without "IR", extension or extra spaces.
    static juce::String title(const juce::String& rel);
private:
    juce::StringArray files;
    std::map<juce::String, std::vector<float>> cache;
    double cacheRate = 0;
};

/// Cuts an IR to what the engine convolves (DSP::maxIR samples), fading the last 5 ms so the cut never clicks.
std::vector<float> fitIR(std::vector<float> h, double sampleRate = 48000);

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
