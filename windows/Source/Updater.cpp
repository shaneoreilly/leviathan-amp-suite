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

juce::String Updater::playerNotes(const juce::String& body) {
    juce::StringArray out;
    bool skipping = false;
    for (auto& line : juce::StringArray::fromLines(body)) {
        const auto t = line.trim();
        if (t.startsWithChar('#')) {
            skipping = t.trimCharactersAtStart("#").trim().equalsIgnoreCase("downloads");
            if (skipping) continue;
        }
        if (skipping || t.startsWith("**Already on") || t.startsWith("**Full Changelog")) continue;
        out.add(line);
    }
    return out.joinIntoString("\n").trim();
}

/// Notes for every release after `current` up to `latest`, newest first, each under its version; empty when GitHub
/// can't be reached or only one release is new (then the latest release's own notes are used). Runs off the message thread.
static juce::String notesSince(const juce::String& current, const juce::String& latest) {
    int code = 0;
    const juce::URL url(juce::String("https://api.github.com/repos/") + Updater::repo + "/releases?per_page=30");
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                            .withExtraHeaders(kHeaders).withConnectionTimeoutMs(15000).withStatusCode(&code));
    if (!stream || code != 200) return {};
    const auto v = juce::JSON::parse(stream->readEntireStreamAsString());
    auto* arr = v.getArray();
    if (arr == nullptr) return {};
    std::vector<std::pair<juce::String, juce::String>> found;
    for (auto& r : *arr) {
        if ((bool) r["draft"] || (bool) r["prerelease"]) continue;
        auto tag = r["tag_name"].toString(); if (tag.startsWith("v")) tag = tag.substring(1);
        if (Updater::newer(tag, current) && !Updater::newer(tag, latest)) found.push_back({ tag, r["body"].toString() });
    }
    if (found.size() < 2) return {};
    std::sort(found.begin(), found.end(), [](auto& a, auto& b) { return Updater::newer(a.first, b.first); });
    juce::StringArray parts;
    for (auto& [tag, body] : found) parts.add("# Leviathan " + tag + "\n\n" + Updater::playerNotes(body));
    return parts.joinIntoString("\n\n");
}

void Updater::setAutoCheck(bool on) { autoCheck = on; m.props().setValue("autoUpdateCheck", on); m.changed(); }

/// Check at every launch, then every 6 hours while running.
void Updater::checkOnLaunch() {
    dismissedVersion = m.props().getValue("updateDismissed");
    if (!autoCheck) return;
    check(true);
    startTimer(6 * 3600 * 1000);
}

void Updater::timerCallback() {
    if (autoCheck && phase != Phase::available && !busy()) check(true);
}

void Updater::dismiss() {
    if (release) { dismissedVersion = release->version; m.props().setValue("updateDismissed", dismissedVersion); }
    m.changed();
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
        // a second request when an update exists: the notes of every version since this one
        juce::String since;
        if (code == 200) {
            auto tag = juce::JSON::parse(body)["tag_name"].toString();
            if (tag.startsWith("v")) tag = tag.substring(1);
            if (newer(tag, current())) since = notesSince(current(), tag);
        }
        juce::MessageManager::callAsync([this, aliveFlag, quiet, code, body, since] {
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
            Release r; r.version = tag; r.notes = since.isNotEmpty() ? since : playerNotes(v["body"].toString()); r.page = juce::URL(v["html_url"].toString());
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
            // LEVIATHAN_SHOW_NOTES=1 opens What's new once an update is found (UI testing)
            if (juce::SystemStats::getEnvironmentVariable("LEVIATHAN_SHOW_NOTES", {}).isNotEmpty()) showNotes = true;
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
