// The rig screen: tuner, panels on the grid, pedalboard (port of ContentView in mac/Sources/App.swift).
#pragma once
#include "Widgets.h"

namespace lev {

/// Text that is recomputed on every refresh (values, readouts).
class LiveLabel : public juce::Component, public juce::SettableTooltipClient {
public:
    LiveLabel(std::function<juce::String()> fn, float pt = 10, juce::Colour c = theme::secondary, juce::Justification j = juce::Justification::centredLeft, bool bold = false)
        : text(std::move(fn)), size(pt), colour(c), just(j), isBold(bold) { setInterceptsMouseClicks(false, false); fit(); }
    void fit() { setPref(*this, std::max(8, juce::GlyphArrangement::getStringWidthInt(theme::font(size, isBold), text()) + 4), (int) (size * theme::UI * 1.35f)); }
    void paint(juce::Graphics& g) override { g.setColour(colourFn ? colourFn() : colour); g.setFont(theme::font(size, isBold)); const auto t = text(); if (t.containsChar('\n')) g.drawFittedText(t, getLocalBounds(), just, 4, 1.0f); else g.drawText(t, getLocalBounds(), just, true); }
    std::function<juce::String()> text; std::function<juce::Colour()> colourFn;
    float size; juce::Colour colour; juce::Justification just; bool isBold;
};

/// ComboBox bound to a getter/setter; re-reads its value on refresh.
class Picker : public juce::ComboBox {
public:
    Picker(int width, std::function<int()> get, std::function<void(int)> set) : getter(std::move(get)), setter(std::move(set)) {
        setPref(*this, width, 26);
        onChange = [this] { if (!syncing && getSelectedId() != 0) setter(getSelectedId()); };
    }
    void sync() { syncing = true; setSelectedId(getter(), juce::dontSendNotification); syncing = false; }
private:
    std::function<int()> getter; std::function<void(int)> setter; bool syncing = false;
};

class RigView : public juce::Component, private juce::Timer {
public:
    explicit RigView(Model& m);
    void refresh();
    void resized() override;
    /// Space at the top of the scrolling content, for a top bar drawn over it that scrolls away (phones).
    void setTopInset(int px) { if (px != topInset) { topInset = px; layout(); } }
    int scrollY() const { return viewport.getViewPositionY(); }
    std::function<void()> onScroll;
private:
    struct ScrollViewport : juce::Viewport {
        std::function<void()> moved;
        void visibleAreaChanged(const juce::Rectangle<int>&) override { if (moved) moved(); }
    };
    Model& model;
    int topInset = 0;
    ScrollViewport viewport;
    juce::Component canvas;
    std::map<juce::String, std::unique_ptr<UnitBox>> panels;
    std::vector<std::unique_ptr<UnitBox>> pedals;
    std::unique_ptr<juce::Component> tunerView, pedalHeader, emptyPedals, tip;
    juce::String signature;
    std::vector<juce::Component*> live;     // repainted ~15x/s (meters, readouts)
    std::unique_ptr<juce::Component> helpStrip;   // explanation of the control under the pointer

    juce::String currentSignature() const;
    void rebuild();
    void layout();
    void syncAll(juce::Component& c);
    void timerCallback() override;

    std::unique_ptr<UnitBox> makePanel(const juce::String& id);
    juce::String cabSignature() const;
    juce::String cabSig;
    void rebuildPanel(const juce::String& id);
    std::unique_ptr<juce::Component> inputPickers();
    std::unique_ptr<UnitBox> makePedal(PedalKind k);
    std::unique_ptr<juce::Component> makeTuner();
    template <typename T> T* liveReg(T* c) { live.push_back(c); return c; }
};

} // namespace lev
