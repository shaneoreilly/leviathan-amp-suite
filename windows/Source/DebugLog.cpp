#include "DebugLog.h"
#include "Library.h"
#include "SysInfo.h"

namespace lev::debuglog {

namespace {
juce::CriticalSection lock;
std::unique_ptr<juce::FileOutputStream> out;
juce::File file;
bool full = false;
constexpr juce::int64 maxBytes = 2 * 1024 * 1024;
constexpr int keep = 5;

juce::String now() { return juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S.") + juce::String(juce::Time::getCurrentTime().getMilliseconds()).paddedLeft('0', 3); }

void header() {
    using S = juce::SystemStats;
    out->writeText("Leviathan " + juce::String(LEVIATHAN_VERSION) + " diagnostic log\n", false, false, nullptr);
    out->writeText("System:  " + S::getOperatingSystemName() + (S::isOperatingSystem64Bit() ? " (64-bit)" : "")
                   + (S::getDeviceDescription().isNotEmpty() ? ", " + S::getDeviceManufacturer() + " " + S::getDeviceDescription() : juce::String()) + "\n", false, false, nullptr);
    juce::StringArray features;
    if (S::hasSSE2()) features.add("SSE2");
    if (S::hasSSE41()) features.add("SSE4.1");
    if (S::hasAVX()) features.add("AVX");
    if (S::hasAVX2()) features.add("AVX2");
    if (S::hasNeon()) features.add("NEON");
    out->writeText("CPU:     " + S::getCpuModel() + " (" + S::getCpuVendor() + "), " + juce::String(S::getNumPhysicalCpus()) + " cores / "
                   + juce::String(S::getNumCpus()) + " threads, " + juce::String(S::getCpuSpeedInMegahertz()) + " MHz, " + features.joinIntoString(" ") + "\n", false, false, nullptr);
    out->writeText("Memory:  " + juce::String(S::getMemorySizeInMegabytes()) + " MB\n", false, false, nullptr);
    out->writeText("NAM:     " + juce::String(sysinfo::namEngine()) + "\n", false, false, nullptr);
    if (const auto pw = sysinfo::power(); !pw.empty()) out->writeText("Power:   " + juce::String(pw) + "\n", false, false, nullptr);
    if (const auto pr = sysinfo::processPriority(); !pr.empty()) out->writeText("Process: priority " + juce::String(pr) + "\n", false, false, nullptr);
    out->writeText("\n", false, false, nullptr);
    out->flush();
}
} // namespace

juce::File dir() { return appDataDir().getChildFile("Logs"); }

bool enabled() { const juce::ScopedLock sl(lock); return out != nullptr; }

void setEnabled(bool on) {
    const juce::ScopedLock sl(lock);
    if (on == (out != nullptr)) return;
    if (!on) { out->writeText(now() + "  log    stopped\n", false, false, nullptr); out->flush(); out.reset(); return; }
    dir().createDirectory();
    juce::Array<juce::File> old = dir().findChildFiles(juce::File::findFiles, false, "Leviathan-*.log");
    old.sort();
    for (int i = 0; i < old.size() - (keep - 1); ++i) old[i].deleteFile();   // room for the new one
    file = dir().getChildFile("Leviathan-" + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H.%M.%S") + ".log");
    out = std::make_unique<juce::FileOutputStream>(file);
    if (!out->openedOk()) { out.reset(); return; }
    full = false;
    header();
}

juce::File latest() {
    const juce::ScopedLock sl(lock);
    if (out != nullptr) { out->flush(); return file; }
    auto all = dir().findChildFiles(juce::File::findFiles, false, "Leviathan-*.log");
    all.sort();
    return all.isEmpty() ? juce::File() : all.getLast();
}

void write(const juce::String& topic, const juce::String& text) {
    const juce::ScopedLock sl(lock);
    if (out == nullptr || full) return;
    if (out->getPosition() > maxBytes) { out->writeText(now() + "  log    full (2 MB): turn the log off and on to start a new one\n", false, false, nullptr); full = true; out->flush(); return; }
    out->writeText(now() + "  " + topic.paddedRight(' ', 7) + text + "\n", false, false, nullptr);
    out->flush();
}

} // namespace lev::debuglog
