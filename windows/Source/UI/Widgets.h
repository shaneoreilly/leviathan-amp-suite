// Shared UI pieces, styled after the SwiftUI app: theme, knobs, switches, faders, panel boxes, grid layout,
// MIDI-learn overlay. Sizes are in SwiftUI points; window zoom is the JUCE global scale factor.
#pragma once
#include <JuceHeader.h>
#include "../Model.h"
#include "../Midi.h"

namespace lev {

namespace theme {
const juce::Colour bg { 0xff0d0d0d }, bar { 0xff121212 }, panel { 0xff161616 }, panel2 { 0xff1f1f1f }, line { 0xff333333 },
                   knobFace { 0xff262626 }, knobTrack { 0xff383838 }, text { 0xffeeeeee }, secondary { 0xff9a9a9a };
/// SwiftUI point size -> JUCE font (SwiftUI's 1.5x text scale, JUCE heights include line spacing).
juce::Font font(float pt, bool bold = false, float tracking = 0);
constexpr float UI = 1.5f, W = 1.4f;
}

class LookAndFeel : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    void setAccent(juce::Colour c);
    juce::Font getComboBoxFont(juce::ComboBox&) override { return theme::font(10.5f); }
    juce::Font getPopupMenuFont() override { return theme::font(10.5f); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return theme::font(10); }
    juce::Font getLabelFont(juce::Label&) override { return theme::font(10); }
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Colour accent = juce::Colours::red;
};

/// Preferred size of a component (layout hint stored on the component).
void setPref(juce::Component& c, int w, int h);
juce::Point<int> pref(const juce::Component& c);

/// Lays children out in a row or column at their preferred sizes, centred across.
class Stack : public juce::Component {
public:
    Stack(bool vertical, int spacing, juce::Justification align = juce::Justification::centred) : vert(vertical), gap(spacing), just(align) {}
    template <typename T> T* add(std::unique_ptr<T> c) { auto* p = c.get(); addAndMakeVisible(*p); owned.push_back(std::move(c)); relayoutPref(); return p; }
    void addRef(juce::Component& c) { addAndMakeVisible(c); relayoutPref(); }
    void relayoutPref();
    void resized() override;
    bool vert; int gap; juce::Justification just;
private:
    std::vector<std::unique_ptr<juce::Component>> owned;
};

/// Click-to-arm overlay for MIDI learn; sits on top of a control and only takes clicks while learning.
class LearnOverlay : public juce::Component {
public:
    LearnOverlay(Model& m, MidiArmed a, float radius = 10) : model(m), armed(std::move(a)), r(radius) { setInterceptsMouseClicks(false, false); }
    void sync();
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    MidiArmed armed;
private:
    Model& model; float r;
};

class Knob : public juce::Component, public juce::SettableTooltipClient {
public:
    using Fmt = std::function<juce::String(double)>;
    Knob(Model& m, juce::String label, juce::String key, double lo, double hi, double def, Fmt fmt = {}, juce::String scope = {});
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void sync() { overlay.sync(); repaint(); }
private:
    Model& model; juce::String label, key, scope; double lo, hi, def; Fmt fmt; double dragStart = 0;
    juce::uint32 enteredAt = 0;
    bool onReset = false;
    juce::Rectangle<float> resetArea() const { return { (float) getWidth() / 2 - 11, 102, 22, 16 }; }
    LearnOverlay overlay;
    double value() const { return model.s().d(key); }
    juce::Rectangle<float> dial() const;
};

class Footswitch : public juce::Component {
public:
    Footswitch(Model& m, juce::String key, juce::String name);
    void paint(juce::Graphics&) override;
    void resized() override { overlay.setBounds(getLocalBounds()); }
    void mouseUp(const juce::MouseEvent&) override;
    void sync() { overlay.sync(); repaint(); }
private:
    Model& model; juce::String key; LearnOverlay overlay;
};

/// Vertical EQ fader: click anywhere on the track to jump there, drag to move, double-click for 0.
class Fader : public juce::Component, public juce::SettableTooltipClient {
public:
    Fader(Model& m, int band);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override { setFrom(e); }
    void mouseDrag(const juce::MouseEvent& e) override { setFrom(e); }
    void mouseDoubleClick(const juce::MouseEvent&) override { model.setSettings([&] { auto s = model.s(); s.setEq(band, 0); return s; }()); }
private:
    Model& model; int band;
    void setFrom(const juce::MouseEvent& e);
};

/// A text button that can also be MIDI-learned as an action ("@next", "@tap", ...).
class ActionButton : public juce::TextButton {
public:
    ActionButton(Model& m, const juce::String& text, std::function<void()> fn, std::optional<MidiArmed> learn = std::nullopt);
    void resized() override { if (overlay) overlay->setBounds(getLocalBounds()); }
    void sync() { if (overlay) overlay->sync(); }
private:
    std::unique_ptr<LearnOverlay> overlay;
};

/// Small icon button drawn from a path (chevrons, x, reset, grip...).
class IconButton : public juce::Button {
public:
    enum Icon { chevronLeft, chevronRight, chevronDown, cross, reset, refresh, plus, minus, star, starFilled, folder, trash, play, pause, prev, next, grip, metronome };
    IconButton(Icon i, std::function<void()> fn = {}, float size = 14);
    void paintButton(juce::Graphics&, bool over, bool down) override;
    Icon icon; juce::Colour colour = theme::secondary; float iconSize;
    static juce::Path pathFor(Icon i, juce::Rectangle<float> r);
};

/// A rig panel: chevron, (drag grip), title, trailing items, ON/OFF; content centred, sitting on the bottom.
class UnitBox : public juce::Component, public juce::DragAndDropTarget {
public:
    /// dragId: "panel:<id>" for rig panels, "pedal:<kind>" for pedals, empty for no drag handle.
    UnitBox(Model& m, juce::String title, std::unique_ptr<juce::Component> content, juce::String onKey = {}, juce::String dragId = {});
    std::vector<std::unique_ptr<juce::Component>> leading, trailing;
    void addLeading(std::unique_ptr<juce::Component> c) { addAndMakeVisible(*c); leading.push_back(std::move(c)); }
    void addTrailing(std::unique_ptr<juce::Component> c) { addAndMakeVisible(*c); trailing.push_back(std::move(c)); }
    juce::Point<int> idealSize() const;
    bool folded() const { return model.collapsed.contains(title); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void paintOverChildren(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    juce::String title, onKey, dragId;
    bool isInterestedInDragSource(const SourceDetails& d) override;
    void itemDragEnter(const SourceDetails& d) override;
    void itemDropped(const SourceDetails&) override {}
    std::unique_ptr<juce::Component> content;
    std::unique_ptr<Footswitch> footswitch;
    bool dimContent() const { return onKey.isNotEmpty() && !model.s().b(onKey); }
private:
    Model& model;
    IconButton chevron;
    std::unique_ptr<juce::Component> gripHandle;
    static constexpr int pad = 12, headerH = 24, gapAfterHeader = 10;
};

/// Panels snap to a 24-column grid; expanded panels in a row share the row's height.
/// A nullptr item starts a new row.
void layoutGrid(std::vector<UnitBox*> items, juce::Rectangle<int> area, int& usedHeight, int columns = 24, int spacing = 12);

/// Draws the Leviathan logo (white whale over the sea), strokes scaled to the size.
void drawLogo(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c);

} // namespace lev
