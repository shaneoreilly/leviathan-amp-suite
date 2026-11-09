// Self-update from GitHub releases. Windows: finds the installer on the latest release, checks it against the
// release's .sha256, runs it silently (it closes Leviathan, installs over it and reopens it). Android: the same for
// the release's .apk, handed to Android's installer, which asks you to confirm the update.
#pragma once
#include <JuceHeader.h>

namespace lev {

class Model;

class Updater : private juce::URL::DownloadTaskListener, private juce::Timer {
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
    /// The What's new popup is open.
    bool showNotes = false;

    static juce::String current() { return LEVIATHAN_VERSION; }
    static bool newer(const juce::String& a, const juce::String& b);
    /// Release notes as players see them in the app: without the Downloads section, the "click Update" hint and the
    /// GitHub changelog link, which only make sense on the release page.
    static juce::String playerNotes(const juce::String& body);
    /// "> Released on 2026-11-06 06:36 PM NYC" and a blank line, from GitHub's published_at (UTC); empty without one.
    static juce::String released(const juce::String& isoUtc);
    bool busy() const { return phase == Phase::checking || phase == Phase::downloading || phase == Phase::installing; }

    void checkOnLaunch();
    void check(bool quiet = false);
    void install();
    void setAutoCheck(bool on);
    /// Hide the update banner until the next version.
    void dismiss();
    bool showBanner() const { return phase == Phase::available && release && release->version != dismissedVersion; }
    juce::String dismissedVersion;

private:
    Model& m;
    std::unique_ptr<juce::URL::DownloadTask> task;
    juce::File download;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    void timerCallback() override;
    void finished(juce::URL::DownloadTask*, bool success) override;
    void progress(juce::URL::DownloadTask*, juce::int64 done, juce::int64 total) override;
    void fail(const juce::String& msg);
    void verifyAndRun();
};

} // namespace lev
