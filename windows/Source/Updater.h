// Self-update from GitHub releases (Windows): finds the installer on the latest release, checks it against the
// release's .sha256, runs it silently (it closes Leviathan, installs over it and reopens it).
#pragma once
#include <JuceHeader.h>

namespace lev {

class Model;

class Updater : private juce::URL::DownloadTaskListener {
public:
    static constexpr const char* repo = "shaneoreilly/leviathan-amp-suite";
    explicit Updater(Model& m);
    ~Updater() override;

    struct Release { juce::String version, notes; juce::URL installer, sha, page; };
    enum class Phase { idle, checking, upToDate, available, downloading, installing, failed };
    Phase phase = Phase::idle;
    std::optional<Release> release;
    double progressValue = 0;
    juce::String error;
    bool autoCheck = true;

    static juce::String current() { return LEVIATHAN_VERSION; }
    static bool newer(const juce::String& a, const juce::String& b);
    bool busy() const { return phase == Phase::checking || phase == Phase::downloading || phase == Phase::installing; }

    void checkOnLaunch();
    void check(bool quiet = false);
    void install();
    void setAutoCheck(bool on);

private:
    Model& m;
    std::unique_ptr<juce::URL::DownloadTask> task;
    juce::File download;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    void finished(juce::URL::DownloadTask*, bool success) override;
    void progress(juce::URL::DownloadTask*, juce::int64 done, juce::int64 total) override;
    void fail(const juce::String& msg);
    void verifyAndRun();
};

} // namespace lev
