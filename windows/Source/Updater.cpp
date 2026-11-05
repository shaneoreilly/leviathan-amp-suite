#include "Updater.h"
#include "Model.h"

namespace lev {

static const char* kHeaders = "Accept: application/vnd.github+json\r\nUser-Agent: Leviathan-Updater";

Updater::Updater(Model& model) : m(model) { autoCheck = m.props().getBoolValue("autoUpdateCheck", true); }

Updater::~Updater() { alive->store(false); task.reset(); }

bool Updater::newer(const juce::String& a, const juce::String& b) {
    auto x = juce::StringArray::fromTokens(a, ".", ""), y = juce::StringArray::fromTokens(b, ".", "");
    for (int i = 0; i < std::max(x.size(), y.size()); ++i) {
        const int p = x[i].getIntValue(), q = y[i].getIntValue();
        if (p != q) return p > q;
    }
    return false;
}

void Updater::setAutoCheck(bool on) { autoCheck = on; m.props().setValue("autoUpdateCheck", on); m.changed(); }

/// At launch, at most once every 12 hours.
void Updater::checkOnLaunch() {
    if (!autoCheck) return;
    const auto last = juce::Time(m.props().getValue("lastUpdateCheck", "0").getLargeIntValue());
    if ((juce::Time::getCurrentTime() - last).inHours() < 12) return;
    check(true);
}

void Updater::check(bool quiet) {
    if (busy()) return;
    phase = Phase::checking; error = {}; m.changed();
    auto aliveFlag = alive;
    juce::Thread::launch([this, aliveFlag, quiet] {
        int code = 0;
        const juce::URL url(juce::String("https://api.github.com/repos/") + repo + "/releases/latest");
        auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                .withExtraHeaders(kHeaders).withConnectionTimeoutMs(15000).withStatusCode(&code));
        const auto body = stream ? stream->readEntireStreamAsString() : juce::String();
        juce::MessageManager::callAsync([this, aliveFlag, quiet, code, body] {
            if (!aliveFlag->load()) return;
            const auto v = juce::JSON::parse(body);
            if (code != 200 || !v.isObject()) {
                phase = quiet ? Phase::idle : Phase::failed;
                error = "Couldn't check for updates" + (code ? " (GitHub answered " + juce::String(code) + ")" : juce::String()) + ".";
                m.changed(); return;
            }
            m.props().setValue("lastUpdateCheck", juce::String(juce::Time::getCurrentTime().toMilliseconds()));
            auto tag = v["tag_name"].toString();
            if (tag.startsWith("v")) tag = tag.substring(1);
            if (!newer(tag, current())) { phase = Phase::upToDate; m.changed(); return; }
            Release r; r.version = tag; r.notes = v["body"].toString(); r.page = juce::URL(v["html_url"].toString());
            juce::String installerName;
            if (auto* assets = v["assets"].getArray()) {
                for (const auto& a : *assets) {
                    const auto name = a["name"].toString();
                    if (name.endsWithIgnoreCase(".exe")) { installerName = name; r.installer = juce::URL(a["browser_download_url"].toString()); }
                }
                for (const auto& a : *assets) if (a["name"].toString() == installerName + ".sha256") r.sha = juce::URL(a["browser_download_url"].toString());
            }
            release = r;
            if (installerName.isEmpty()) { phase = quiet ? Phase::idle : Phase::failed; error = "Release " + tag + " has no Windows installer yet."; m.changed(); return; }
            phase = Phase::available;
            m.changed();
        });
    });
}

void Updater::install() {
#if ! JUCE_WINDOWS
    if (release) release->page.launchInDefaultBrowser();   // the macOS app updates itself; this build is for testing
    return;
#else
    if (!release || busy()) return;
    if (release->sha.isEmpty()) { fail("the release has no checksum file"); return; }
    download = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Leviathan-Setup-" + release->version + ".exe");
    download.deleteFile();
    phase = Phase::downloading; progressValue = 0; m.changed();
    task = release->installer.downloadToFile(download, juce::URL::DownloadTaskOptions().withExtraHeaders("User-Agent: Leviathan-Updater").withListener(this));
    if (!task) fail("the download didn't start");
#endif
}

void Updater::progress(juce::URL::DownloadTask*, juce::int64 done, juce::int64 total) {
    auto aliveFlag = alive;
    const double f = total > 0 ? (double) done / (double) total : 0;
    juce::MessageManager::callAsync([this, aliveFlag, f] { if (aliveFlag->load() && phase == Phase::downloading) { progressValue = f; m.changed(); } });
}

void Updater::finished(juce::URL::DownloadTask* t, bool success) {
    const bool ok = success && !t->hadError() && (t->statusCode() == 0 || t->statusCode() == 200);
    auto aliveFlag = alive;
    juce::MessageManager::callAsync([this, aliveFlag, ok] {
        if (!aliveFlag->load()) return;
        if (!ok) { fail("the download failed"); return; }
        phase = Phase::installing; m.changed();
        verifyAndRun();
    });
}

void Updater::verifyAndRun() {
    auto aliveFlag = alive;
    const auto shaURL = release->sha; const auto file = download;
    juce::Thread::launch([this, aliveFlag, shaURL, file] {
        auto stream = shaURL.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                   .withExtraHeaders("User-Agent: Leviathan-Updater").withConnectionTimeoutMs(15000));
        const auto want = stream ? stream->readEntireStreamAsString().trim().upToFirstOccurrenceOf(" ", false, false).toLowerCase() : juce::String();
        const auto got = juce::SHA256(file).toHexString().toLowerCase();
        juce::MessageManager::callAsync([this, aliveFlag, want, got, file] {
            if (!aliveFlag->load()) return;
            if (want.length() != 64 || want != got) { file.deleteFile(); fail("the download didn't match its checksum"); return; }
            // Inno Setup: closes the running app, installs over it, and its [Run] entry reopens Leviathan
            if (!file.startAsProcess("/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS")) { fail("the installer didn't start"); return; }
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        });
    });
}

void Updater::fail(const juce::String& msg) {
    phase = Phase::failed;
    error = "Update failed: " + msg + ". Nothing was changed.";
    m.changed();
}

} // namespace lev
