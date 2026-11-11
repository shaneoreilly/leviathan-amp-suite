// The amp pickers (AMP and AMP B panels), laid out like the cabinet's: ‹ [current amp] › ⋯ (port of mac/Sources/Amp.swift).
// An amp is one id: "" the built-in Leviathan amp, "nam:<file>" a NAM capture, "cap:<name>" a fitted capture.
// Amp B lists "" (off) and NAM captures.
#pragma once
#include "Widgets.h"

namespace lev::amp {

void step(Model& m, int d, bool secondAmp);
/// The panel's button (badge, name, detail); opens a searchable list of amps.
std::unique_ptr<juce::Component> makeButton(Model& m, bool secondAmp);

}
