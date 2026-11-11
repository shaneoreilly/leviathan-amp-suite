// The cabinet picker (port of mac/Sources/Cab.swift): one button on the CABINET panel naming the current cab and
// where it comes from, ‹ › to step through its list, and a browser with search and Built-in / God's Cab / Library /
// Favourites / Recent.
//
// A cab, wherever it comes from, is one id: "b:<index>" built-in, "u" the loaded single file, "l:<rel>" an IR in the
// library, "g:<rel>" a God's Cab IR (as mic A). Favourites and recents keep these per user (app properties, not presets).
#pragma once
#include "Widgets.h"
#include "../Model.h"

namespace lev::cab {

juce::String currentId(Model& m);
juce::String name(Model& m, const juce::String& id);
juce::String source(const juce::String& id);   // "LIBRARY", "GOD'S CAB", ...
juce::String detail(Model& m, const juce::String& id);
void apply(Model& m, const juce::String& id);
void step(Model& m, int d);

/// The panel's button: badge, name and detail; opens the browser.
std::unique_ptr<juce::Component> makeButton(Model& m);

}
