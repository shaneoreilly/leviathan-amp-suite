// Diagnostic log (Settings > About > Debug log): a plain-text record of the setup, the audio device, NAM loads,
// messages and, every 2 s while playing, DSP load, NAM's share of it and dropouts. Off by default; one file per
// launch in <app data>/Logs, the last 5 kept, each capped at 2 MB. Same format as the Mac app's DebugLog.swift.
// Never called from the audio thread: the engine only adds to counters, which the model writes out.
#pragma once
#include <JuceHeader.h>

namespace lev::debuglog {

bool enabled();
/// Turning it on starts a new file with the setup at the top.
void setEnabled(bool on);
juce::File dir();
/// The file being written (or the newest one), or nothing.
juce::File latest();
/// One line: time, topic, text. Thread-safe; does nothing while off.
void write(const juce::String& topic, const juce::String& text);

}
