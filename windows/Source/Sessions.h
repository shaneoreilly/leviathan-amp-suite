// Recorder, session library, playback, live re-amp and offline re-amp (port of mac/Sources/Sessions.swift).
// Session folders are the same format as the Mac app's: dry.wav, wet.wav, session.json.
#pragma once
#include <JuceHeader.h>
#include "Settings.h"

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

    const Session* current() const;
    void select(const juce::File& dir);
    void refresh();

    void toggleRecord() { recording ? stopRecording() : startRecording(); }
    void startRecording();
    void stopRecording();
    void tick();                          // ~30 Hz from the model's timer

    void togglePlay() { playing ? stopPlayback() : play(); }
    void play(double from = 0);
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
    void trim(double start, double end);
    void restoreUntrimmed();
    bool canRestoreUntrimmed() const;
    static std::optional<double> trimFiles(const juce::File& dir, double start, double end);
    void remove();
    void remove(const juce::File& dir);
    /// Finish a take that was cut off before Stop (unfinished WAV headers, no session.json) so it shows up again.
    static void recover(const juce::File& dir);
    /// Set a WAV file's RIFF and data sizes from its real length; returns seconds (or -1).
    static double fixWavHeader(const juce::File& f);
    static juce::Time dateInName(const juce::String& s);
    void reveal();
    void importRecordings();

private:
    Model& m;
    std::unique_ptr<juce::AudioFormatWriter> dryWriter, wetWriter;
    juce::File recDir; double recRate = 48000; juce::Time recStart; juce::int64 recFrames = 0;
    std::vector<float> tmp[3];
    int playLen = 0; double playRate = 48000;
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
