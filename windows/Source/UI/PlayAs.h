// Pitch: two rows alike, ‹ [guitar  tuning ▾] › (your guitar) and ‹ [speaker  tuning ▾] › (Play as) (port of mac/Sources/PlayAs.swift).
// ‹ › shift down and up a semitone; the button opens a list with search: every tuning the shifter can reach from your
// guitar's, then bands and the tunings they play in (shared/pitch/bands.json). A band in a tuning your guitar can't
// reach by shifting every string the same is shown, greyed.
#pragma once
#include "Widgets.h"

namespace lev::playas {

/// The two Pitch rows' buttons, alike: [icon  tuning ▾]. The guitar's lists your guitar's tunings; Play as's (what you
/// sound like) lists the tunings it can reach, then bands, with search.
std::unique_ptr<juce::Component> makeGuitarButton(Model& m);
std::unique_ptr<juce::Component> makeButton(Model& m);

}
