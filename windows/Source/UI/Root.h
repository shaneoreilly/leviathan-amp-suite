// Window content: sidebar, top bar, status banners and the four screens (port of mac/Sources/Shell.swift).
#pragma once
#include "Rig.h"

namespace lev {

/// A screen that rebuilds itself only when what it shows has changed.
class Screen_ : public juce::Component {
public:
    virtual void refresh() = 0;
};

class Root : public juce::Component, public juce::DragAndDropContainer, private juce::ChangeListener, private juce::Timer {
public:
    explicit Root(Model& m);
    ~Root() override;
    void resized() override;
    void paint(juce::Graphics& g) override { g.fillAll(theme::bg); }
    bool keyPressed(const juce::KeyPress& k) override;

private:
    Model& model;
    LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::Component> sidebar, topbar;
    std::unique_ptr<RigView> rig;
    std::unique_ptr<Screen_> presets, recordings, settings;
    juce::OwnedArray<juce::Component> banners;
    juce::String bannerSig;
    std::unique_ptr<juce::Component> whatsNew;   // the What's new popup, over everything
    Screen shown = Screen::rig;
    int themeShown = -1;
    void buildScreens();   // every screen, at the current theme (rebuilt when it changes)

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuildBanners();
};

} // namespace lev
