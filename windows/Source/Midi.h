// MIDI control (port of mac/Sources/Midi.swift): learn a CC for any knob or on/off switch, footswitch actions
// (keys starting with "@"), Program Change picks presets, MIDI clock sets the tempo.
#pragma once
#include <JuceHeader.h>

namespace lev {

class Model;

struct MidiMap {
    juce::String id = juce::Uuid().toString();
    int channel = 0, cc = 0;          // 0...15, 0...127
    juce::String key, name;           // Settings field ("odDrive") or action ("@next"); shown name
    double lo = 0, hi = 1;
    bool toggle = false, momentary = false;
};

/// The control waiting for a CC while learning.
struct MidiArmed {
    juce::String key, name; double lo = 0, hi = 1; bool toggle = false;
    bool operator==(const MidiArmed& o) const { return key == o.key; }
    static MidiArmed action(const juce::String& k, const juce::String& n) { return { k, n, 0, 1, true }; }
};

class Midi : private juce::MidiInputCallback {
public:
    explicit Midi(Model& m);
    ~Midi() override;
    void start();

    std::vector<MidiMap> maps;
    bool learning = false;
    std::optional<MidiArmed> armed;
    juce::StringArray sources;
    bool programChange = true, followClock = true;
    juce::String last;                // last message, for Settings

    void setLearning(bool on);
    void arm(const MidiArmed& a);
    const MidiMap* mapFor(const juce::String& key) const;
    void forget(const juce::String& key);
    void removeMap(const juce::String& id);
    void clear();
    void setMomentary(const juce::String& id, bool on);
    void setProgramChange(bool on);
    void setFollowClock(bool on);
    void save();

private:
    Model& m;
    std::vector<std::unique_ptr<juce::MidiInput>> inputs;
    juce::MidiDeviceListConnection deviceWatch;
    std::optional<std::pair<juce::String, double>> justLearned;
    // clock tracking (MIDI thread)
    int ticks = 0; double beatStart = 0, lastTick = 0, smooth = 0;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    void connectAll();
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    std::optional<double> clockMessage(const juce::MidiMessage& msg);
    void controlChange(int ch, int cc, int v);
    void handle(const juce::MidiMessage& msg);
};

} // namespace lev
