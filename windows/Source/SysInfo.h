// System details for the diagnostic log that JUCE doesn't offer: which NAM build runs, the power plan, how busy the
// whole machine is next to Leviathan, and the audio thread's priority. Windows APIs stay in SysInfo.cpp (so
// <windows.h> never meets JUCE's names); elsewhere these return nothing and the log leaves them out.
#pragma once
#include <string>

namespace lev::sysinfo {

/// Which NAM build is running, e.g. "AVX2+FMA (levnam_avx2.dll)" or "SSE2 (this CPU has no AVX2/FMA)".
std::string namEngine();

/// Power source and plan, e.g. "AC power, plan \"Balanced\", power mode Best performance". Empty where unknown.
std::string power();

/// Process priority class, e.g. "normal". Empty where unknown.
std::string processPriority();

/// CPU use since the previous call, in percent of all cores: the whole machine, and Leviathan alone.
/// Negative where unknown (and on the first call). Message thread only.
struct CpuUse { double system = -1, process = -1; };
CpuUse cpuUse();

/// The calling thread's scheduling priority (Windows' THREAD_PRIORITY_*), or `unknownPriority`.
/// Real-time safe: one cheap system call, no allocation.
constexpr int unknownPriority = -1000;
int currentThreadPriority();
/// "time critical", "highest", ... for a value from currentThreadPriority().
std::string priorityName(int p);

}
