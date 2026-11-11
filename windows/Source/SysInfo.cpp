#include "SysInfo.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <powrprof.h>
#include <cstdio>
#include <cwchar>
#pragma comment(lib, "powrprof.lib")
#endif

#ifdef _MSC_VER
extern "C" const char* levnam_engine_description();   // windows/nam/dispatch.cpp
#endif

namespace lev::sysinfo {

std::string namEngine() {
#ifdef _MSC_VER
    return levnam_engine_description();
#elif defined(__aarch64__) || defined(__arm64__)
    return "single build (ARM64, NEON)";
#elif defined(__x86_64__)
    return "single build (x86-64)";
#else
    return "single build";
#endif
}

#ifdef _WIN32
namespace {
std::string narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s((size_t) n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

/// Windows 10/11's power-mode slider (Settings > System > Power), which overrides the plan's processor settings.
/// Not in the SDK headers, so looked up by name; absent on older Windows.
std::string powerMode() {
    HMODULE pp = GetModuleHandleW(L"powrprof.dll");
    if (pp == nullptr) return {};
    using Fn = DWORD (WINAPI*)(GUID*);
    auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(pp, "PowerGetActualOverlayScheme")));
    if (fn == nullptr) return {};
    GUID g {};
    if (fn(&g) != ERROR_SUCCESS) return {};
    static const GUID efficiency  = { 0x961cc777, 0x2547, 0x4f9d, { 0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a } };
    static const GUID better      = { 0x3af9b8d9, 0x7c97, 0x431d, { 0xad, 0x78, 0x34, 0xa8, 0xbf, 0xea, 0x43, 0x9f } };
    static const GUID performance = { 0xded574b5, 0x45a0, 0x4f42, { 0x87, 0x37, 0x46, 0x34, 0x5c, 0x09, 0xc2, 0x38 } };
    static const GUID none {};
    if (IsEqualGUID(g, efficiency)) return "Best power efficiency";
    if (IsEqualGUID(g, better)) return "Balanced (better performance)";
    if (IsEqualGUID(g, performance)) return "Best performance";
    if (IsEqualGUID(g, none)) return "Balanced";
    return "custom";
}

unsigned long long ticks(const FILETIME& f) { return ((unsigned long long) f.dwHighDateTime << 32) | f.dwLowDateTime; }
} // namespace
#endif

std::string power() {
#ifdef _WIN32
    std::string s;
    SYSTEM_POWER_STATUS ps {};
    if (GetSystemPowerStatus(&ps)) {
        if (ps.BatteryFlag == 128 || ps.BatteryFlag == 255) s = ps.ACLineStatus == 0 ? "battery" : "AC power";
        else s = ps.ACLineStatus == 1 ? "AC power (battery " + std::to_string(ps.BatteryLifePercent) + "%)"
                                      : "on battery (" + std::to_string(ps.BatteryLifePercent) + "%)";
        if (ps.SystemStatusFlag & 1) s += ", battery saver on";
    }
    GUID* scheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &scheme) == ERROR_SUCCESS && scheme != nullptr) {
        wchar_t name[256] {};
        DWORD size = sizeof name;
        if (PowerReadFriendlyName(nullptr, scheme, nullptr, nullptr, (UCHAR*) name, &size) == ERROR_SUCCESS)
            s += (s.empty() ? "" : ", ") + std::string("plan \"") + narrow(name) + "\"";
        LocalFree(scheme);
    }
    const std::string mode = powerMode();
    if (!mode.empty()) s += (s.empty() ? "" : ", ") + std::string("power mode ") + mode;
    return s;
#else
    return {};
#endif
}

std::string processPriority() {
#ifdef _WIN32
    switch (GetPriorityClass(GetCurrentProcess())) {
        case IDLE_PRIORITY_CLASS: return "idle";
        case BELOW_NORMAL_PRIORITY_CLASS: return "below normal";
        case NORMAL_PRIORITY_CLASS: return "normal";
        case ABOVE_NORMAL_PRIORITY_CLASS: return "above normal";
        case HIGH_PRIORITY_CLASS: return "high";
        case REALTIME_PRIORITY_CLASS: return "realtime";
        default: return {};
    }
#else
    return {};
#endif
}

CpuUse cpuUse() {
    CpuUse u;
#ifdef _WIN32
    static unsigned long long lastIdle = 0, lastBusyTotal = 0, lastProc = 0, lastWall = 0;
    FILETIME idle, kernel, user, created, exited, pk, pu, now;
    if (!GetSystemTimes(&idle, &kernel, &user) || !GetProcessTimes(GetCurrentProcess(), &created, &exited, &pk, &pu)) return u;
    GetSystemTimeAsFileTime(&now);
    const unsigned long long i = ticks(idle), total = ticks(kernel) + ticks(user);   // kernel time includes idle
    const unsigned long long proc = ticks(pk) + ticks(pu), wall = ticks(now);
    if (lastWall != 0 && total > lastBusyTotal && wall > lastWall) {
        const double dt = (double) (total - lastBusyTotal);
        u.system = 100.0 * (1.0 - (double) (i - lastIdle) / dt);
        SYSTEM_INFO si; GetSystemInfo(&si);
        u.process = 100.0 * (double) (proc - lastProc) / ((double) (wall - lastWall) * (double) si.dwNumberOfProcessors);
    }
    lastIdle = i; lastBusyTotal = total; lastProc = proc; lastWall = wall;
#endif
    return u;
}

int currentThreadPriority() {
#ifdef _WIN32
    const int p = GetThreadPriority(GetCurrentThread());
    return p == THREAD_PRIORITY_ERROR_RETURN ? unknownPriority : p;
#else
    return unknownPriority;
#endif
}

std::string priorityName(int p) {
    switch (p) {
        case 15: return "time critical";
        case 2: return "highest";
        case 1: return "above normal";
        case 0: return "normal";
        case -1: return "below normal";
        case -2: return "lowest";
        case -15: return "idle";
        case unknownPriority: return {};
        default: return std::to_string(p);   // real-time class values (16..31 range offsets)
    }
}

}
