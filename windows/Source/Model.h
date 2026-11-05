// App state and the audio engine host: the C++ counterpart of Model in mac/Sources/App.swift.
#pragma once
#include <JuceHeader.h>
#include "Dsp.h"
#include "Settings.h"
#include "Library.h"

namespace lev {

class Sessions;
class Midi;
class Updater;

enum class Screen { rig, presets, recordings, settings };

/// A private copy of the rig (offline re-amping), independent of the live engine (with its own NAM instance).
struct OfflineRig {
    Params p; std::unique_ptr<DSP> dsp; LevNam* nam = nullptr;
    ~OfflineRig() { dsp.reset(); lev_nam_free(nam); }
};


class Model : public juce::ChangeBroadcaster, private juce::AudioIODeviceCallback, private juce::Timer {
public:
    Model();
    ~Model() override;

    Params params;
    juce::AudioDeviceManager devices;
    juce::PropertiesFile& props() { return *propsFile; }

    // MARK: settings
    const Settings& s() const { return settings; }
    void setSettings(const Settings& n);
    void set(const juce::Identifier& key, const juce::var& v);   // one field (knobs, switches, MIDI)
    void changed() { sendChangeMessage(); }

    // MARK: engine
    struct Info {
        double sampleRate = 0; int buffer = 0; double inMs = 0, bufferMs = 0, outMs = 0;
        juce::String inName, outName, type;
        double latencyMs() const { return inMs + bufferMs + outMs; }
        juce::String breakdown() const;
    };
    bool running = false;
    Info info;
    juce::String status;
    int dropouts = 0;
    float cpu = 0;
    void start();
    void stop();
    void toggle() { running ? stop() : start(); }
    void restartAudio();
    bool hasInput() const;

    // MARK: presets
    std::map<juce::String, Settings> userPresets;
    juce::String presetName;
    juce::StringArray favorites;
    bool isFactory(const juce::String& n) const;
    std::optional<Settings> presetSettings(const juce::String& n) const;
    juce::StringArray presetOrder() const;
    void loadFactory(const juce::String& n);
    void loadFactoryOriginal(const juce::String& n);
    void restoreFactory(const juce::String& n);
    void loadAny(const juce::String& n);
    void loadUser(const juce::String& n);
    void toggleFavorite(const juce::String& n);
    void renamePreset(const juce::String& oldName, const juce::String& newName);
    void saveCurrent() { savePreset(presetName); }
    void savePreset(const juce::String& name);
    void deletePreset(const juce::String& n);
    bool presetModified() const;
    void stepPreset(int d);
    void persistPresets();

    // preset files (.leviathan)
    static constexpr const char* presetExt = "leviathan";
    juce::String presetFileData(const juce::String& name) const;
    void exportPreset(const juce::String& name);
    void exportAllPresets();
    void importPresetsDialog();
    void importPresets(const juce::Array<juce::File>& files);

    // MARK: pedals
    bool pedalOn(PedalKind k) const { return settings.b(pedalOnKey(k)); }
    void addPedal(PedalKind k);
    void removePedal(PedalKind k);
    void movePedal(PedalKind k, PedalKind before);

    // MARK: pitch
    const Tuning& tuning() const { return TUNINGS[(size_t) juce::jlimit(0, (int) TUNINGS.size() - 1, tuningIndex)]; }
    juce::String soundsLike() const;
    std::vector<std::pair<juce::String, int>> playAsOptions() const;
    void playAs(int semis);

    // MARK: tuner
    bool tunerOn = false, tunerMute = true;
    int tuningIndex = 1;
    double a4 = 440;
    juce::String tunerNote = juce::String::fromUTF8("\xe2\x80\x93");
    double tunerCents = 0, tunerFreq = 0;
    int tunerString = -1;
    void setTuner(bool on) { tunerOn = on; syncMute(); changed(); }
    void setTunerMute(bool m) { tunerMute = m; syncMute(); changed(); }
    void setTuningIndex(int i);
    void setA4(double v);

    // MARK: tempo
    double bpm = 120;
    bool metroOn = false;
    int beatsPerBar = 4;
    double metroVol = -12;
    void setBpm(double v);
    void setMetro(bool on) { metroOn = on; applyAll(); changed(); }
    void setBeatsPerBar(int n);
    void setMetroVol(double v);
    void tap();
    std::optional<double> syncedDelayMs() const;
    void midiAction(const juce::String& key);

    // MARK: cab, captures
    IRLibrary irLib;
    std::map<juce::String, Capture> captures;
    juce::String userIRName;
    const Capture* activeCapture() const;
    void useCapture(const juce::String& name);
    void reloadCaptures();
    void loadIR();
    void updateIR();
    std::vector<float> currentIR(double sr);
    std::unique_ptr<OfflineRig> offlineRig(double sr);

    // MARK: NAM captures (.nam, in appDataDir()/NAM)
    static juce::File namDir() { return appDataDir().getChildFile("NAM"); }
    /// Captures that ship with the app (next to Leviathan.exe; inside the bundle on macOS test builds).
    static juce::File namBundledDir();
    /// Your own copy wins over the bundled one of the same name.
    static juce::File namFile(const juce::String& name);
    juce::StringArray namFiles;
    juce::String namLoaded;                 // name of the model in the engine now
    double namExpectedRate = 0; std::optional<double> namLoudness;
    bool namReady() const { return namModel != nullptr && namLoaded == settings.s("nam") && settings.s("nam").isNotEmpty(); }
    void rescanNam();
    void importNamDialog();
    void useNam(const juce::String& name);
    void updateNam();
    double engineRate() const { return dspRate; }

    // MARK: appearance + layout (not part of presets)
    std::optional<juce::Colour> accentPick;
    juce::Colour accent() const;
    void setAccent(std::optional<juce::Colour> c);
    double zoom = 1.0;
    static const std::vector<double> zoomSteps;
    void setZoom(double z);
    void zoomStep(int d);
    juce::StringArray panelOrder;
    juce::StringArray collapsed;
    void toggleCollapsed(const juce::String& key);
    void setPanelOrder(const juce::StringArray& o);

    Screen screen = Screen::rig;
    void setScreen(Screen s) { screen = s; changed(); }

    std::unique_ptr<Sessions> sessions;
    std::unique_ptr<Midi> midi;
    std::unique_ptr<Updater> updater;

    std::function<void()> onTick;   // UI hook, ~30 Hz

private:
    Settings settings;
    std::unique_ptr<juce::PropertiesFile> propsFile;
    std::optional<std::pair<std::vector<float>, double>> userIR;
    LevNam* namModel = nullptr; double namRate = 0; float namGain = 1; int namGen = 0;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    struct NamOpen { LevNam* m = nullptr; float gain = 1; double expected = 0; std::optional<double> loudness; };
    static NamOpen openNam(const juce::File& f, double rate);
    void setEngineNam(LevNam* m, float gain);

    // engine (the DSP is created when the device starts, at its rate)
    juce::SpinLock dspLock;
    std::unique_ptr<DSP> dsp;
    double dspRate = 48000;
    std::vector<float> zeros, scratchL, scratchR;

    int saveCountdown = -1, tunerTick = 0, lastXRuns = 0;
    std::vector<double> recentOverloads;
    bool adviseOverloads = true;
    double startedAt = 0;
    std::vector<double> taps;

    void applyAll();
    void settingsChanged(const Settings& old);
    void applyPreset(const Settings& p, const juce::String& name);
    void syncMute() { params.muted.store(tunerOn && tunerMute); }
    void saveState();
    void timerCallback() override;
    void runTuner();
    void checkInputClipping();
    int lastClips = 0; std::vector<double> clipTimes; bool clipWarned = false;
    void refreshInfo();
    void overloaded();
    void ensureInputs();

    void audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int n,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* d) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& msg) override;
};

/// "1:05"
juce::String mmss(double seconds);

} // namespace lev
