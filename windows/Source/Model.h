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
    Params p; std::unique_ptr<DSP> dsp; LevNam* nam = nullptr; LevNam* namB = nullptr;
    ~OfflineRig() { dsp.reset(); lev_nam_free(nam); lev_nam_free(namB); }
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
    /// Back to the rig (and preset name) before the last change; changes less than 0.6 s apart are one step.
    void undo();
    bool canUndo() const { return !undoStack.empty(); }
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
    /// Where the current status message gets fixed: a Settings section ("Audio", "Library", "About"), or "amp:on"
    /// to switch the amp on. Only errors have one. It belongs to the message it was set for (statusFixFor), so a
    /// later message never inherits it. The banner shows "Click to resolve" for it.
    juce::String statusFix, statusFixFor;
    void fixIn(const juce::String& where) { statusFix = where; statusFixFor = status; }
    juce::String currentFix() const { return status.isNotEmpty() && status == statusFixFor ? statusFix : juce::String(); }
    /// Goes to a fix: opens that Settings section, or switches the amp on.
    void resolve(const juce::String& where);
    /// Settings opens on this section next time it shows (set by resolve; Settings clears it).
    juce::String settingsJump;
    int dropouts = 0;
    float cpu = 0;
    /// withInput false: for playing a recording back while the amp is off. The device opens with the live input
    /// silenced (on a phone with no interface it's the built-in mic, which would feed back through the speaker),
    /// and Sessions turns audio off again once playback is over. start() with input turns it on from there.
    void start(bool withInput = true);
    void stop();
    bool playbackOnly = false;
    std::atomic<bool> inputOff { false };   // read by the audio callback
    bool live() const { return running && !playbackOnly; }
    /// Android: leaving the app releases the audio device entirely (its exclusive low-latency streams and CPU hint
    /// otherwise starve other apps' playback); coming back reopens it. Not while recording or live re-amping.
    void enterBackground();
    void enterForeground();
    bool pausedForBackground = false, wasRunningBeforeBackground = false;
    /// Diagnostic log (debuglog): the worst callback since the last perf line, and what was last written.
    std::atomic<float> callbackPeak { 0 };
    /// Callbacks since the last perf line, the longest time between two of them (s), and the audio thread's priority:
    /// together they tell a slow CPU (load high, gaps normal) from a driver or system stall (gaps long, load normal).
    std::atomic<int> callbackCount { 0 }, callbackPriority { -1000 };
    std::atomic<float> callbackGapMax { 0 };
    juce::int64 callbackLast = 0;   // audio thread only (reset in start(), before the callback is added)
    juce::String loggedDevice, loggedStatus, loggedMessage, loggedPower;
    int perfTick = 0, perfXRuns = 0; double perfAt = 0, perfNam = 0;
    void setDebugLog(bool on);
    void toggle() { live() ? stop() : start(); }
    void restartAudio();
    /// Theme (Settings > Appearance): "dark" or "light", and your own background colour for each ("" = default). Not in
    /// presets. Root applies it (theme::apply) and rebuilds the window when themeStamp changes (Mac: Model.theme).
    juce::String themeName = "dark", bgDark, bgLight;
    int themeStamp = 0;
    juce::Colour themeBackground() const {
        const auto hex = themeName == "light" ? bgLight : bgDark;
        if (hex.length() == 6) return juce::Colour((juce::uint32) (0xff000000u | (juce::uint32) hex.getHexValue32()));
        return themeName == "light" ? juce::Colour(0xffececee) : juce::Colour(0xff0d0d0d);
    }
    void setTheme(const juce::String& name) { themeName = name; props().setValue("theme", name); themeStamp++; changed(); }
    /// The background colour for the theme in use, as RRGGBB ("" = back to the default).
    void setBackground(const juce::String& hex) {
        (themeName == "light" ? bgLight : bgDark) = hex;
        props().setValue(themeName == "light" ? "bgLight" : "bgDark", hex); themeStamp++; changed();
    }
    /// Messages hidden with "Don't show again" (Settings > Appearance > Messages shows them again). A message's key is
    /// its wording without numbers, quoted names or brackets, as the Mac app's Model.messageKey.
    juce::StringArray hiddenMessages;
    static juce::String messageKey(const juce::String& s);
    bool isHidden(const juce::String& msg) const { return hiddenMessages.contains(messageKey(msg)); }
    void hideForever(const juce::String& msg);
    void showHiddenMessages();
    /// Output EQ for the output in use: each output (headphones, monitors, a TV) keeps its own, switching with it.
    /// Kept in the app properties by output name, not in presets (Mac: Model.OutputEQ, the same keys).
    struct OutputEQ { bool on = false; double low = 0, mid = 0, high = 0, level = 0; };
    OutputEQ outputEQ;
    juce::String outputEQKey;   // the output it belongs to
    void setOutputEQ(const OutputEQ& e);
    void loadOutputEQ();
    /// The chosen device setup (type, devices, rate, buffer, channels) as text: unchanged by opening or closing it.
    juce::String deviceSignature() const;
    bool hasInput() const;

    // MARK: presets
    std::map<juce::String, Settings> userPresets;
    juce::String presetName;
    juce::StringArray favorites;
    bool isFactory(const juce::String& n) const;
    std::optional<Settings> presetSettings(const juce::String& n) const;
    juce::StringArray presetOrder() const;
    /// When each preset was created, last saved and last loaded (ms since 1970, 0 = never), for sorting.
    struct PresetMeta {
        juce::int64 created = 0, modified = 0, played = 0;
        juce::int64 imported = 0;   // from a .leviathan file, or first seen by a version that keeps dates
        juce::int64 shownModified() const { return modified > 0 ? modified : imported; }   // the Modified column
        juce::StringArray tags;   // your own tags (the automatic ones are worked out: autoTags)
    };
    /// Preset tags (the Mac app's Model.autoTags and friends, the same words): Factory or Yours, the amp, style words
    /// in the name; then your own.
    juce::StringArray autoTags(const juce::String& n) const;
    juce::StringArray userTags(const juce::String& n) const;
    void setUserTags(const juce::String& n, const juce::String& commaSeparated);
    juce::StringArray tags(const juce::String& n) const;
    std::map<juce::String, PresetMeta> presetMeta;
    int presetSort = 0;   // 0 list order, 1 recently played, 2 recently modified, 3 recently created, 4 name A-Z
    bool presetSortReversed = false;   // list order Z-A / oldest first
    void setPresetSort(int s);
    /// Click a column header: sort by it, or reverse if it's already the sort.
    void sortPresetsBy(int s) { if (presetSort == s) presetSortReversed = !presetSortReversed; else { presetSort = s; presetSortReversed = false; } setPresetSort(presetSort); }
    juce::StringArray sortedPresets(int sort, bool reversed = false) const;
    static juce::String stamp(juce::int64 ms) { return ms > 0 ? juce::Time(ms).formatted("%Y-%m-%d %H:%M") : juce::String::fromUTF8("\xe2\x80\x94"); }
    juce::StringArray recentlyPlayed() const {
        juce::StringArray out;
        for (auto& n : sortedPresets(1)) if (auto it = presetMeta.find(n); it != presetMeta.end() && it->second.played > 0) out.add(n);
        return out;
    }
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
    /// Loading another preset from the screen asks first when this one has unsaved changes (MIDI never asks).
    void confirmLoad(std::function<void()> load);
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

    // MARK: NAM input calibration (as in the NAM plugin; app-wide, not per preset)
    bool namCalibrate = false;
    double interfaceLevel = 12;   // dBu that reaches 0 dBFS on your input
    std::optional<double> namCalibration() const { return namCalibrate ? std::optional<double>(interfaceLevel) : std::nullopt; }
    void setNamCalibrate(bool on);
    void setInterfaceLevel(double dbu);
    /// Per loaded capture (amp A, amp B): its calibrated input level if the file has one, and the trim applied (dB).
    std::optional<double> namInputLevel, namInputLevelB; double namTrim = 0, namTrimB = 0;
    /// Input calibration gain (dB) for a capture made at `capture` dBu, with your interface at `interface` dBu (nullopt = off).
    static double calibrationTrim(std::optional<double> capture, std::optional<double> interface);
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
    CabIRLibrary cabIRs;
    std::map<juce::String, Capture> captures;
    juce::String userIRName;
    const Capture* activeCapture() const;
    void useCapture(const juce::String& name);
    void reloadCaptures();
    void loadIR();
    void updateIR();
    /// The cab impulse at a rate with Size / Length / Tone applied (not to a fitted capture's IR); rawIR: as its source gives it.
    std::vector<float> currentIR(double sr);
    std::vector<float> rawIR(double sr);
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
    bool namReadyB() const { return namModelB != nullptr && namLoadedB == settings.s("ampBNam") && settings.s("ampBNam").isNotEmpty(); }
    bool namReady() const { return namModel != nullptr && namLoaded == settings.s("nam") && settings.s("nam").isNotEmpty(); }
    void rescanNam();
    void importNamDialog();
    /// Add cab IRs (WAV files or folders of them) to the IR library and switch to the first one.
    void importCabIRsDialog();
    void useNam(const juce::String& name);
    void updateNam();
    void updateNamB();   // Amp B's capture into the running engine
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

    /// Explanations on hover (Settings -> Appearance): shared/help/explanations.json, the same text as the Mac app.
    bool explainOnHover = true;
    void setExplainOnHover(bool on);
    static juce::String explain(const juce::String& key);
    juce::String hoverId, hoverTitle, hoverText;
    void showHover(const juce::String& id, const juce::String& title, const juce::String& text);
    void clearHover(const juce::String& id = {});

    /// Input device and channel, for the Input panel (Settings -> Audio has the full device panel).
    juce::StringArray inputDeviceNames();
    juce::String inputDeviceName() const;
    int inputChannelCount() const;
    void setInputDevice(const juce::String& name);
    /// Output device, for the Master panel.
    juce::StringArray outputDeviceNames();
    juce::String outputDeviceName() const;
    void setOutputDevice(const juce::String& name);

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
    LevNam* namModel = nullptr; double namRate = 0; float namGain = 1, namIn = 1; int namGen = 0; std::optional<double> namCal;
    LevNam* namModelB = nullptr; double namRateB = 0; float namGainB = 1, namInB = 1; int namGenB = 0; juce::String namLoadedB; std::optional<double> namCalB;
    void setEngineNamB(LevNam* m, float gain, float input = 1);
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    struct NamOpen { LevNam* m = nullptr; float gain = 1, input = 1; double expected = 0, trim = 0; std::optional<double> loudness, inputLevel; };
    static NamOpen openNam(const juce::File& f, double rate, std::optional<double> calibration);
    void setEngineNam(LevNam* m, float gain, float input = 1);

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
    std::vector<std::pair<Settings, juce::String>> undoStack;
    juce::int64 lastEditMs = 0; bool undoing = false;
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
