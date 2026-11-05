#include <JuceHeader.h>
#include "Model.h"
#include "UI/Root.h"

namespace lev {

class MainWindow : public juce::DocumentWindow {
public:
    explicit MainWindow(Model& model)
        : DocumentWindow("Leviathan", juce::Colour(0xff0d0d0d), DocumentWindow::allButtons), m(model) {
        setUsingNativeTitleBar(true);
        setContentOwned(new Root(m), true);
        setResizable(true, false);
        setResizeLimits(900, 600, 10000, 10000);
        // the window reopens where and how big you left it
        const auto saved = m.props().getValue("windowFrame");
        if (saved.isEmpty() || !restoreWindowStateFromString(saved)) centreWithSize(1600, 1000);
        setVisible(true);
    }
    ~MainWindow() override { m.props().setValue("windowFrame", getWindowStateAsString()); m.props().saveIfNeeded(); }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
private:
    Model& m;
};

class App : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Leviathan"; }
    const juce::String getApplicationVersion() override { return LEVIATHAN_VERSION; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override {
        model = std::make_unique<Model>();
        juce::Desktop::getInstance().setGlobalScaleFactor((float) model->zoom);
        window = std::make_unique<MainWindow>(*model);
        openFiles(commandLine);
    }
    void shutdown() override { window.reset(); model.reset(); }
    void systemRequestedQuit() override { quit(); }
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
