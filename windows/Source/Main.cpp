#include <JuceHeader.h>
#include "Model.h"
#include "UI/Root.h"
#include "UI/NativeMenus.h"

namespace lev {

class MainWindow : public juce::DocumentWindow {
public:
    explicit MainWindow(Model& model)
        : DocumentWindow(juce::String("Leviathan - v") + LEVIATHAN_VERSION, juce::Colour(0xff0d0d0d), DocumentWindow::allButtons), m(model) {
        setUsingNativeTitleBar(true);
        setContentOwned(new Root(m), true);
       #if JUCE_ANDROID
        // a phone has one window: the whole screen, with the status and navigation bars hidden (swipe to show them)
        juce::Desktop::getInstance().setKioskModeComponent(this, false);
        setFullScreen(true);
       #else
        setResizable(true, false);
        setResizeLimits(900, 600, 10000, 10000);
        // the window reopens where and how big you left it
        const auto saved = m.props().getValue("windowFrame");
        // first open: big enough for the whole rig at 100% without scrolling (as the Mac app), never bigger than the screen
        if (saved.isEmpty() || !restoreWindowStateFromString(saved)) {
            auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                      ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea : juce::Rectangle<int>(0, 0, 1600, 1000);
            centreWithSize(std::min(2000, area.getWidth()), std::min(1140, area.getHeight() - 40));   // 40: the title bar
        }
       #endif
        setVisible(true);
    }
    ~MainWindow() override {
       #if ! JUCE_ANDROID
        m.props().setValue("windowFrame", getWindowStateAsString());
       #endif
        m.props().saveIfNeeded();
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
private:
    Model& m;
};

class App : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Leviathan"; }
    const juce::String getApplicationVersion() override { return LEVIATHAN_VERSION; }
    // Desktop: one Leviathan at a time. Android already keeps one (singleTask), and JUCE's check there can mistake
    // the copy an update just killed for a running one and quit the new one.
    bool moreThanOneInstanceAllowed() override { return onAndroid; }

    void initialise(const juce::String& commandLine) override {
       #if JUCE_ANDROID
        // Keep the screen on and the phone awake while Leviathan is open: hands are on the guitar, not the screen.
        juce::Desktop::setScreenSaverEnabled(false);
        installNativeMenus();   // drop-downs and menus as native Android pickers
        // Android asks for the microphone (any audio input) at runtime; the audio devices open after the answer.
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, [this, commandLine](bool) { open(commandLine); });
       #else
        open(commandLine);
       #endif
    }
    void open(const juce::String& commandLine) {
        model = std::make_unique<Model>();
        juce::Desktop::getInstance().setGlobalScaleFactor((float) model->zoom);
        window = std::make_unique<MainWindow>(*model);
        openFiles(commandLine);
    }
    void shutdown() override { window.reset(); model.reset(); }
    void systemRequestedQuit() override { quit(); }
    // Android: release the audio device while another app is in front (see Model::enterBackground)
    void suspended() override { if (onAndroid && model) model->enterBackground(); }
    void resumed() override { if (onAndroid && model) model->enterForeground(); }
    // Android's back gesture would close the app and cut the sound; a stray swipe mid-song mustn't do that.
    // It goes back to the rig instead, and does nothing on the rig.
    bool backButtonPressed() override {
        if (model && model->screen != Screen::rig) model->setScreen(Screen::rig);
        return true;
    }
    // a .leviathan file double-clicked while Leviathan is already open
    void anotherInstanceStarted(const juce::String& commandLine) override { openFiles(commandLine); }

private:
    std::unique_ptr<Model> model;
    std::unique_ptr<MainWindow> window;

    void openFiles(const juce::String& commandLine) {
        juce::Array<juce::File> files;
        for (const auto& t : juce::StringArray::fromTokens(commandLine, true)) {
            const juce::File f(t.unquoted());
            if (f.existsAsFile() && f.hasFileExtension(Model::presetExt)) files.add(f);
        }
        if (files.isEmpty() || !model) return;
        model->importPresets(files);
        model->setScreen(Screen::presets);
        if (window) window->toFront(true);
    }
};

} // namespace lev

START_JUCE_APPLICATION(lev::App)
