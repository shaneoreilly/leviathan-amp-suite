#ifdef __ANDROID__
 #define JUCE_CORE_INCLUDE_JNI_HELPERS 1   // the APK install intent
#endif
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

juce::String Updater::released(const juce::String& iso) {
    // "2026-11-06T22:36:27Z"; JUCE has no time zones, so New York time comes from the US rules: daylight time (UTC-4)
    // from 2:00 local on the second Sunday of March to 2:00 local on the first Sunday of November, else UTC-5.
    if (iso.length() < 16 || iso[4] != '-' || iso[10] != 'T') return {};
    const int Y = iso.substring(0, 4).getIntValue(), M = iso.substring(5, 7).getIntValue(), D = iso.substring(8, 10).getIntValue();
    const int h = iso.substring(11, 13).getIntValue(), mi = iso.substring(14, 16).getIntValue();
    if (Y < 2000 || M < 1 || M > 12 || D < 1 || D > 31) return {};
    // days since 1970-01-01 for a civil date (Howard Hinnant's algorithm)
    auto days = [](int y, int m, int d) {
        y -= m <= 2; const int era = (y >= 0 ? y : y - 399) / 400; const int yoe = y - era * 400;
        const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return (juce::int64) era * 146097 + doe - 719468;
    };
    auto sunday = [&](int y, int m, int nth) {   // day number of the nth Sunday of a month
        const auto first = days(y, m, 1); const int wd = (int) ((first + 4) % 7);   // 0 = Sunday (1970-01-01 was a Thursday)
        return first + (7 - wd) % 7 + 7 * (nth - 1);
    };
    const juce::int64 utcMin = days(Y, M, D) * 1440 + h * 60 + mi;
    const juce::int64 dstStart = sunday(Y, 3, 2) * 1440 + 7 * 60, dstEnd = sunday(Y, 11, 1) * 1440 + 6 * 60;   // 2:00 local in UTC
    const juce::int64 local = utcMin + (utcMin >= dstStart && utcMin < dstEnd ? -4 : -5) * 60;
    // back to a civil date
    const juce::int64 z = (local >= 0 ? local : local - 1439) / 1440 + 719468;
    const juce::int64 era = (z >= 0 ? z : z - 146096) / 146097; const int doe = (int) (z - era * 146097);
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    const int y = (int) (yoe + era * 400) + (m <= 2);
    const int minOfDay = (int) (local - ((local >= 0 ? local : local - 1439) / 1440) * 1440);
    const int hh = minOfDay / 60, mm = minOfDay % 60, h12 = hh % 12 == 0 ? 12 : hh % 12;
    return "> Released on " + juce::String(y) + "-" + juce::String(m).paddedLeft('0', 2) + "-" + juce::String(d).paddedLeft('0', 2) + " "
         + juce::String(h12).paddedLeft('0', 2) + ":" + juce::String(mm).paddedLeft('0', 2) + (hh < 12 ? " AM" : " PM") + " NYC\n\n";
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
        if (Updater::newer(tag, current) && !Updater::newer(tag, latest))
            found.push_back({ tag, Updater::released(r["published_at"].toString()) + Updater::playerNotes(r["body"].toString()) });
    }
    if (found.size() < 2) return {};
    std::sort(found.begin(), found.end(), [](auto& a, auto& b) { return Updater::newer(a.first, b.first); });
    juce::StringArray parts;
    for (auto& [tag, notes] : found) parts.add("# Leviathan " + tag + "\n\n" + notes);
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
            Release r; r.version = tag; r.notes = since.isNotEmpty() ? since : released(v["published_at"].toString()) + playerNotes(v["body"].toString()); r.page = juce::URL(v["html_url"].toString());
            juce::String installerName;
            if (auto* assets = v["assets"].getArray()) {
                for (const auto& a : *assets) {
                    const auto name = a["name"].toString();
                    if (name.endsWithIgnoreCase(onAndroid ? ".apk" : ".exe")) { installerName = name; r.installer = juce::URL(a["browser_download_url"].toString()); }
                }
                for (const auto& a : *assets) if (a["name"].toString() == installerName + ".sha256") r.sha = juce::URL(a["browser_download_url"].toString());
            }
            release = r;
            if (installerName.isEmpty()) { phase = quiet ? Phase::idle : Phase::failed; error = "Release " + tag + (onAndroid ? " has no Android app yet." : " has no Windows installer yet."); m.changed(); return; }
            phase = Phase::available;
            // LEVIATHAN_SHOW_NOTES=1 opens What's new once an update is found (UI testing)
            if (juce::SystemStats::getEnvironmentVariable("LEVIATHAN_SHOW_NOTES", {}).isNotEmpty()) showNotes = true;
            m.changed();
        });
    });
}

/// Where an Android update is downloaded: files/Updates, the folder FileProvider shares with the installer
/// (android/res/xml/lev_update_paths.xml).
[[maybe_unused]] static juce::File androidUpdatesDir() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("files/Updates"); }

#if JUCE_ANDROID
/// Opens Android's installer on the downloaded APK (ACTION_VIEW on a FileProvider link). Android asks to confirm,
/// and once to allow installs from Leviathan. Returns false if it couldn't be opened.
static bool launchApkInstaller(const juce::File& apk) {
    auto* env = juce::getEnv();
    const auto ctx = juce::getAppContext();
    auto ok = [env] { if (env->ExceptionCheck()) { env->ExceptionClear(); return false; } return true; };
    const juce::LocalRef<jclass> fileC { env->FindClass("java/io/File") };
    const juce::LocalRef<jobject> file { env->NewObject(fileC.get(), env->GetMethodID(fileC.get(), "<init>", "(Ljava/lang/String;)V"), juce::javaString(apk.getFullPathName()).get()) };
    // androidx isn't on the system class path: load FileProvider through the app's class loader
    const juce::LocalRef<jclass> ctxC { env->GetObjectClass(ctx.get()) };
    const juce::LocalRef<jobject> loader { env->CallObjectMethod(ctx.get(), env->GetMethodID(ctxC.get(), "getClassLoader", "()Ljava/lang/ClassLoader;")) };
    const juce::LocalRef<jclass> loaderC { env->GetObjectClass(loader.get()) };
    const juce::LocalRef<jclass> providerC { (jclass) env->CallObjectMethod(loader.get(), env->GetMethodID(loaderC.get(), "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"),
                                                                            juce::javaString("androidx.core.content.FileProvider").get()) };
    if (!ok() || providerC == nullptr) return false;
    const juce::LocalRef<jobject> uri { env->CallStaticObjectMethod(providerC.get(), env->GetStaticMethodID(providerC.get(), "getUriForFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;"), ctx.get(), juce::javaString("local.leviathan.ampsuite.updates").get(), file.get()) };
    if (!ok() || uri == nullptr) return false;
    const juce::LocalRef<jclass> intentC { env->FindClass("android/content/Intent") };
    const juce::LocalRef<jobject> intent { env->NewObject(intentC.get(), env->GetMethodID(intentC.get(), "<init>", "(Ljava/lang/String;)V"), juce::javaString("android.intent.action.VIEW").get()) };
    const juce::LocalRef<jobject> r1 { env->CallObjectMethod(intent.get(), env->GetMethodID(intentC.get(), "setDataAndType", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;"),
                                                              uri.get(), juce::javaString("application/vnd.android.package-archive").get()) };
    const juce::LocalRef<jobject> r2 { env->CallObjectMethod(intent.get(), env->GetMethodID(intentC.get(), "addFlags", "(I)Landroid/content/Intent;"),
                                                              (jint) (0x00000001 /* GRANT_READ_URI_PERMISSION */ | 0x10000000 /* NEW_TASK */)) };
    env->CallVoidMethod(ctx.get(), env->GetMethodID(ctxC.get(), "startActivity", "(Landroid/content/Intent;)V"), intent.get());
    return ok();
}
#endif

void Updater::install() {
#if ! JUCE_WINDOWS && ! JUCE_ANDROID
    if (release) release->page.launchInDefaultBrowser();   // the macOS app updates itself; this build is for testing
    return;
#else
    if (!release || busy()) return;
    if (release->sha.isEmpty()) { fail("the release has no checksum file"); return; }
    if (onAndroid) { androidUpdatesDir().deleteRecursively(); androidUpdatesDir().createDirectory(); }
    download = onAndroid ? androidUpdatesDir().getChildFile("Leviathan-" + release->version + ".apk")
                         : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Leviathan-Setup-" + release->version + ".exe");
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
           #if JUCE_ANDROID
            // Android's installer takes it from here: it asks to confirm, replaces the app and closes this one
            if (!launchApkInstaller(file)) { fail("Android's installer didn't open"); return; }
            phase = Phase::available; m.changed();   // back to the banner if the update is cancelled
            return;
           #endif
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
