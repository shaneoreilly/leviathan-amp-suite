// Recorder, session library, playback, live re-amp and offline re-amp (port of mac/Sources/Sessions.swift).
// Session folders are the same format as the Mac app's: dry.wav, wet.wav, session.json.
#pragma once
#include <JuceHeader.h>
#include "Settings.h"
#include "Library.h"

namespace lev {

class Model;

struct Session {
    juce::File dir;
    juce::String name, preset, source;
    juce::Time date;
    double duration = 0;
    std::optional<Settings> settings;
    juce::File dry() const { return dir.getChildFile("dry.wav"); }
    juce::File wet() const { return dir.getChildFile("wet.wav"); }
};

class Sessions {
public:
    explicit Sessions(Model& m);
    ~Sessions();
    static juce::File root();

    std::vector<Session> list;
    juce::File selected;
    bool recording = false, playing = false, liveReamp = false;
    double recSeconds = 0, playPos = 0;
    std::optional<double> rendering;     // offline re-amp progress 0...1
    std::vector<float> waveform;         // min/max pairs
    std::vector<float> liveWave;         // min/max pairs of the take being recorded, 20 per second (top bar)
    juce::String message;
    /// As Model::statusFix, for message.
    juce::String messageFix, messageFixFor;
    void fixIn(const juce::String& where) { messageFix = where; messageFixFor = message; }
    juce::String currentFix() const { return message.isNotEmpty() && message == messageFixFor ? messageFix : juce::String(); }

    const Session* current() const;
    void select(const juce::File& dir);
    void refresh();

    void toggleRecord() { recording ? stopRecording() : startRecording(); }
    void startRecording();
    void stopRecording();
    void tick();                          // ~30 Hz from the model's timer

    /// The part of the take selected on the waveform (seconds): Play plays only this, and it's what a trim keeps.
    std::optional<std::pair<double, double>> selection;
    void togglePlay() { if (playing) stopPlayback(); else if (selection) play(selection->first, selection->second); else play(); }
    /// Plays from `from`; with `to` >= 0, stops there (playing a selection).
    void play(double from = 0, double to = -1);
    void seek(double t);
    void stopPlayback();

    void toggleLiveReamp() { liveReamp ? stopLiveReamp() : startLiveReamp(); }
    void startLiveReamp();
    void stopLiveReamp();

    void reamp();
    void loadPreset();
    void rename(const juce::String& name);
    /// Cuts the selected take (dry, wet, wet-original) to start...end seconds; the first trim keeps the untouched
    /// audio in untrimmed/ so it can be restored.
    void trim(double start, double end) { trimTake(start, end, true); }
    /// Cuts start...end out of the take, joining what's either side with a 5 ms crossfade (no click).
    void cut(double start, double end) { trimTake(start, end, false); }
    void restoreUntrimmed();
    bool canRestoreUntrimmed() const;
    /// keep: leave only start...end; otherwise remove start...end. Returns the new length.
    static std::optional<double> trimFiles(const juce::File& dir, double start, double end, bool keep = true);
    /// The system share sheet for the selected take: the raw guitar (dry) or the amped sound (wet). Android
    /// (and the Mac app); Windows has no share sheet JUCE can open.
    static constexpr bool canShare = onAndroid;
    enum class ShareKind { ampedForMessages, ampedWav, rawWav };
    void share(ShareKind kind);
    void remove();
    void remove(const juce::File& dir);
    /// Finish a take that was cut off before Stop (unfinished WAV headers, no session.json) so it shows up again.
    static void recover(const juce::File& dir);
    /// Set a WAV file's RIFF and data sizes from its real length; returns seconds (or -1).
    static double fixWavHeader(const juce::File& f);
    static juce::Time dateInName(const juce::String& s);
    void reveal();
private:
    void trimTake(double start, double end, bool keep);
    juce::ScopedMessageBox shareBox;   // the share sheet; closing it early would cancel the share
public:
    void importRecordings();

private:
    Model& m;
    std::unique_ptr<juce::AudioFormatWriter> dryWriter, wetWriter;
    juce::File recDir; double recRate = 48000; juce::Time recStart; juce::int64 recFrames = 0;
    std::vector<float> tmp[3];
    int playLen = 0, fullLen = 0; double playRate = 48000;
    float liveLo = 0, liveHi = 0; int liveN = 0;
    /// The take last loaded for playback, kept so clicking around the waveform doesn't reload the file.
    juce::File loadedFile; double loadedRate = 0; std::vector<std::vector<float>> loadedCh;
    // buffers handed to the audio thread; swapped-out ones are freed a second later
    std::vector<std::vector<float>> live, retired;
    int retireCountdown = 0;
    std::atomic<int> waveGen { 0 };

    void save(const Session& s);
    void drain();
    const float* hand(std::vector<float> x);
    void loadWaveform();
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    static std::unique_ptr<juce::AudioFormatWriter> openWriter(const juce::File& f, double rate, int channels);
    void importFiles(const juce::Array<juce::File>& picked);
};

} // namespace lev
