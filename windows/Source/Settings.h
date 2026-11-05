// Rig settings, factory presets, tunings. Field names and defaults match mac/Sources/App.swift (Settings) exactly,
// so .leviathan preset files and recordings' saved rigs move between the Mac and Windows apps unchanged.
#pragma once
#include <JuceHeader.h>
#include "Dsp.h"

namespace lev {

/// A rig. Stored as named values (the same JSON the Mac app writes); typed accessors on top.
class Settings {
public:
    Settings();                                   // defaults
    static Settings fromJSON(const juce::var& v); // lenient: missing keys keep defaults, unknown keys are ignored
    juce::var toVar() const;
    juce::String toJSON(bool pretty = false) const;

    double d(const juce::Identifier& k) const;
    bool b(const juce::Identifier& k) const;
    int i(const juce::Identifier& k) const;
    juce::String s(const juce::Identifier& k) const;
    void set(const juce::Identifier& k, const juce::var& v);
    bool has(const juce::Identifier& k) const { return values.contains(k); }
    juce::var get(const juce::Identifier& k) const { return values[k]; }

    std::vector<double> eq() const;
    void setEq(int band, double db);
    std::vector<int> chain() const;
    void setChain(const std::vector<int>& c);

    bool operator==(const Settings& o) const { return toJSON() == o.toJSON(); }
    bool operator!=(const Settings& o) const { return !(*this == o); }

    void apply(Params& p) const;

private:
    juce::NamedValueSet values;
};

using PresetFn = void (*)(Settings&);
struct Factory { const char* name; PresetFn fn; };
extern const std::vector<Factory> FACTORY;

struct Tuning { const char* name; std::array<int, 6> strings; bool flats; };
extern const std::vector<Tuning> TUNINGS;
juce::String tuningName(const std::array<int, 6>& strings);
juce::String noteName(int midi, bool flats);
/// The open-string notes, low to high: "D A D G B E".
juce::String tuningNotes(const std::array<int, 6>& strings, bool flats);
/// Name with its notes, for pickers: "Drop D · D A D G B E".
juce::String tuningLabel(const Tuning& t);

struct DelayDiv { const char* name; double beats; };
extern const std::vector<DelayDiv> DELAY_DIVS;

} // namespace lev
