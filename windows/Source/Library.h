// File-based libraries: God's Cab IRs, amp captures, WAV reading, and where Leviathan keeps things on disk.
#pragma once
#include <JuceHeader.h>
#include "Dsp.h"

namespace lev {

/// %APPDATA%\Leviathan on Windows; ~/Library/Application Support/Leviathan on macOS (shared with the Swift app);
/// the app's private files folder on Android.
juce::File appDataDir();
/// Documents\..\Music\Leviathan on Windows, ~/Music/Leviathan on macOS; on Android the app's own Music folder on
/// shared storage (Android/data/<app>/files/Music/Leviathan, reachable over USB file transfer).
juce::File musicDir();

/// Whether the system can show a file or folder (Explorer, Finder). Android can't, so those buttons are hidden there.
#if JUCE_ANDROID
constexpr bool onAndroid = true;
#else
constexpr bool onAndroid = false;
#endif
constexpr bool canRevealFiles = !onAndroid;

/// The files and folders picked in a FileChooser. On Android the picker hands back content:// links instead of
/// paths; those are copied (folders with their whole tree) into a cache folder first, so callers always get
/// ordinary files.
juce::Array<juce::File> pickedFiles(const juce::FileChooser& fc);
/// Writes text to the file picked in a save dialog: nullopt if nothing was picked, else whether it worked.
/// `ext` is added on desktop when the name lacks it (Android's picker keeps the suggested name).
std::optional<bool> writePicked(const juce::FileChooser& fc, const juce::String& text, const juce::String& ext);
/// The folder picked in a folder dialog, for writing files into: nullopt if none was picked.
struct PickedFolder {
    juce::String name;
    std::function<bool(const juce::String& fileName, const juce::String& text)> write;
    std::function<void()> reveal;
};
std::optional<PickedFolder> pickedFolder(const juce::FileChooser& fc);
/// A folder picked in a folder dialog, as an ordinary folder to work in directly. Desktop: the folder. Android: the
/// picked folder's path on the phone's own storage (internal or SD card); nothing for other places (e.g. Google
/// Drive), which only exist behind Android's picker.
juce::File pickedDirectoryPath(const juce::FileChooser& fc);
/// Android 11+: "All files access", which recording into a folder on the phone's storage needs. Always true on desktop
/// and on older Android.
bool hasAllFilesAccess();
/// Android: opens the system page where you turn All files access on for Leviathan.
void requestAllFilesAccess();

struct WavData { std::vector<std::vector<float>> channels; double rate = 0; };
/// Reads WAV/AIFF/FLAC as float channels (up to maxFrames). Returns rate 0 on failure.
WavData readAudio(const juce::File& f, int64_t maxFrames = (int64_t) DSP::maxIR * 4);
/// Reads audio converted to `rate` (or kept at its own rate when rate <= 0), all channels, up to maxSeconds.
WavData loadAudio(const juce::File& f, double rate, double maxSeconds = 900);
/// AAC in an .m4a file (192 kbps), the format chat apps play (WhatsApp plays no WAV). Android only (its built-in
/// encoder); returns false elsewhere.
bool writeM4A(const juce::File& f, const std::vector<const float*>& ch, int frames, double rate);
/// 32-bit float (recordings); bits16: 16-bit integer, the WAV every player and chat app takes (shared copies).
bool writeWav(const juce::File& f, const std::vector<const float*>& ch, int frames, double rate, bool bits16 = false);

/// One God's Cab impulse, parsed from names like "57_TS_1_inch_edge_pres_3.wav".
struct IRFile { juce::String rel, mic; bool ts = false; juce::String position; int pres = 0; };

class IRLibrary {
public:
    static juce::File root() { return appDataDir().getChildFile("IRs/GodsCab"); }
    void scan(double sampleRate);
    bool isEmpty() const { return files.empty(); }
    int size() const { return (int) files.size(); }
    juce::StringArray mics() const;
    const IRFile* file(const juce::String& rel) const;
    juce::StringArray positions(const juce::String& mic, bool ts) const;
    bool tsAvailable(const juce::String& mic) const;
    const IRFile* find(const juce::String& mic, bool ts, const juce::String& position, int pres) const;
    juce::String step(const juce::String& rel, int d) const;
    std::optional<std::vector<float>> samples(const juce::String& rel, double sampleRate);
    static std::optional<IRFile> parse(const juce::String& rel);
    /// Copies the pack's "44.1" and "48" folders, found anywhere inside the picked items, into root(). Used where
    /// there's no folder to open (Android). Returns how many WAV files were copied.
    static int importPack(const juce::Array<juce::File>& items);
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
