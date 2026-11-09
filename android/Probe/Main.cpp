// Leviathan Probe: an Android feasibility test, not the app. It runs the real engine (windows/Source/Dsp.cpp)
// and the NAM bridge (shared/nam) on the phone and measures what decides whether a port is worth it:
//   - round-trip latency (a click out, detected back on the input: speaker -> mic, or a cable out -> in)
//   - audio callback load and dropouts (xruns) while playing through the rig
//   - offline CPU cost of every bundled NAM capture through the full chain
// "Copy report" puts everything on the clipboard to paste back.
#include <JuceHeader.h>
#include "Dsp.h"
#include "lev_nam.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

using namespace lev;

namespace juce { extern std::atomic<int64_t> levInputDropped; }   // android/build.sh patches this into JUCE's Oboe device

namespace {

struct Capture { juce::String name; const char* data; int size; };

std::vector<Capture> bundledCaptures() {
    std::vector<Capture> v;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        int size = 0;
        const char* res = BinaryData::namedResourceList[i];
        const char* data = BinaryData::getNamedResource(res, size);
        const juce::String file = BinaryData::getNamedResourceOriginalFilename(res);
        if (data && file.endsWithIgnoreCase(".nam")) v.push_back({ file.upToLastOccurrenceOf(".nam", false, true), data, size });
    }
    std::sort(v.begin(), v.end(), [](const Capture& a, const Capture& b) { return a.name < b.name; });
    return v;
}

/// lev_nam_load takes a path, so captures are unpacked to the cache folder once.
juce::File unpack(const Capture& c) {
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("probe-" + juce::String::toHexString(c.name.hashCode64()) + ".nam");
    if (f.getSize() != c.size) f.replaceWithData(c.data, (size_t) c.size);
    return f;
}

/// The rig the probe plays through: gate -> amp (built-in or NAM) -> cab, no time effects.
void setRig(Params& p, bool nam, bool cab) {
    p.inGain = 6; p.gateOn = true; p.gateThresh = -60; p.chainCount = 0;
    p.ampOn = true; p.ampChannel = 2; p.ampGain = 0.6f; p.namOn = nam;
    p.eqOn = false; p.cabOn = cab; p.delayOn = false; p.revOn = false; p.masterVol = -6;
    p.version++;
}

/// A guitar-ish test signal for the offline benchmark: decaying power chords every second.
float testInput(long i, double sr) {
    const double t = std::fmod((double) i / sr, 1.0);
    double x = 0;
    for (double f : { 82.41, 123.47, 164.81 }) x += std::sin(2 * 3.141592653589793 * f * t);
    return (float) (x * 0.12 * std::exp(-t * 2.0));
}

struct Loaded { LevNam* m = nullptr; float gain = 1; double expected = 0; juce::String error; };

Loaded loadCapture(const Capture& c, double rate) {
    Loaded o;
    char err[256] = {};
    o.m = lev_nam_load(unpack(c).getFullPathName().toRawUTF8(), err, sizeof err);
    if (!o.m) { o.error = err; return o; }
    lev_nam_reset(o.m, rate, DSP::maxN);
    o.expected = lev_nam_expected_rate(o.m);
    o.gain = (float) std::pow(10.0, (LEV_NAM_TARGET_LEVEL - lev_nam_measure_level(o.m, rate)) / 20) * (float) lev_nam_polarity(o.m, rate);
    return o;
}

juce::String pct(double x) { return juce::String(x * 100, 1) + "%"; }

} // namespace

class ProbeComponent : public juce::Component, private juce::AudioIODeviceCallback, private juce::Timer {
public:
    ProbeComponent() {
        caps = bundledCaptures();

        amp.addItem("Built-in amp (no NAM)", 1);
        for (int i = 0; i < (int) caps.size(); ++i) amp.addItem(caps[(size_t) i].name, i + 2);
        amp.setSelectedId(1, juce::dontSendNotification);
        amp.onChange = [this] { chooseAmp(amp.getSelectedId() - 2); };

        monitor.setToggleState(false, juce::dontSendNotification);   // off: a phone speaker next to its mic howls
        monitor.onClick = [this] { monitorOn.store(monitor.getToggleState()); };
        cab.setToggleState(true, juce::dontSendNotification);
        cab.onClick = [this] { setRig(params, curNam != nullptr, cab.getToggleState()); };

        buffer.onChange = [this] { setBuffer(buffer.getText().getIntValue()); };
        measure.onClick = [this] { startMeasure(); };
        bench.onClick = [this] { runBenchmark(); };
        copy.onClick = [this] { juce::SystemClipboard::copyTextToClipboard(report()); note("Report copied to the clipboard."); };

        log.setMultiLine(true); log.setReadOnly(true); log.setScrollbarsShown(true);
        log.setFont(juce::FontOptions(14.0f));
        status.setJustificationType(juce::Justification::topLeft);
        status.setFont(juce::FontOptions(15.0f));

        for (juce::Component* c : std::initializer_list<juce::Component*> { &status, &amp, &monitor, &cab, &bufferLabel, &buffer, &measure, &bench, &copy, &log })
            addAndMakeVisible(c);

        setRig(params, false, true);
        setSize(420, 800);

        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, [safe = SafePointer(this)](bool granted) {
            if (safe) safe->openAudio(granted);
        });
        startTimerHz(10);
    }

    ~ProbeComponent() override {
        alive->store(false);
        devices.removeAudioCallback(this);
        devices.closeAudioDevice();
        lev_nam_free(curNam);
        for (auto* m : retired) lev_nam_free(m);
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff111111)); }

    void resized() override {
        auto r = getLocalBounds().reduced(12);
        r.removeFromTop(juce::jmax(0, safeTop()));
        status.setBounds(r.removeFromTop(190));
        auto row = [&r](int h) { auto x = r.removeFromTop(h); r.removeFromTop(8); return x; };
        amp.setBounds(row(44));
        { auto x = row(44); monitor.setBounds(x.removeFromLeft(x.getWidth() / 2)); cab.setBounds(x); }
        { auto x = row(44); bufferLabel.setBounds(x.removeFromLeft(130)); buffer.setBounds(x); }
        measure.setBounds(row(48));
        { auto x = row(48); bench.setBounds(x.removeFromLeft(x.getWidth() / 2 - 4)); x.removeFromLeft(8); copy.setBounds(x); }
        log.setBounds(r);
    }

private:
    // MARK: audio

    juce::AudioDeviceManager devices;
    Params params;
    std::unique_ptr<DSP> dsp;
    juce::CriticalSection dspLock;   // UI thread vs device restarts; never taken on the audio thread
    double sr = 48000;
    std::vector<float> silence = std::vector<float>(DSP::maxN, 0.0f), spareR = std::vector<float>(DSP::maxN, 0.0f);

    std::atomic<bool> monitorOn { false };
    std::atomic<float> loadAvg { 0 }, loadPeak { 0 }, inPeak { 0 };
    std::atomic<int> lastBlock { 0 };

    void openAudio(bool granted) {
        if (!granted) { note("Microphone permission denied: no audio input, only the offline benchmark works."); }
        auto err = devices.initialise(granted ? 1 : 0, 2, nullptr, true);
        if (err.isNotEmpty()) { note("Audio device: " + err); return; }
        auto setup = devices.getAudioDeviceSetup();
        setup.sampleRate = 48000;   // the bundled captures are 48 kHz
        if (auto* d = devices.getCurrentAudioDevice()) {
            auto sizes = d->getAvailableBufferSizes();
            buffer.clear(juce::dontSendNotification);
            for (int i = 0; i < sizes.size(); ++i) buffer.addItem(juce::String(sizes[i]), i + 1);
            if (!sizes.isEmpty()) setup.bufferSize = sizes[0];   // smallest
        }
        devices.setAudioDeviceSetup(setup, true);
        devices.addAudioCallback(this);
        if (auto* d = devices.getCurrentAudioDevice()) buffer.setText(juce::String(d->getCurrentBufferSizeSamples()), juce::dontSendNotification);
        note("Audio open: " + devices.getCurrentAudioDeviceType() + ". Turn on Monitor only with headphones or an interface.");
    }

    void setBuffer(int n) {
        if (n <= 0) return;
        auto setup = devices.getAudioDeviceSetup();
        if (setup.bufferSize == n) return;
        setup.bufferSize = n;
        auto err = devices.setAudioDeviceSetup(setup, true);
        loadPeak.store(0);
        note(err.isEmpty() ? "Buffer set to " + juce::String(n) : "Buffer " + juce::String(n) + ": " + err);
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* d) override {
        const juce::ScopedLock sl(dspLock);
        sr = d->getCurrentSampleRate();
        dsp = std::make_unique<DSP>(sr, params);
        dsp->setIR(renderCab(CABS[0], 0.3, sr));
        if (curNam) { lev_nam_reset(curNam, sr, DSP::maxN); dsp->setNam(curNam, curGain); }
        loadAvg.store(0); loadPeak.store(0);
    }

    void audioDeviceStopped() override { const juce::ScopedLock sl(dspLock); dsp.reset(); }

    void audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int n,
                                          const juce::AudioIODeviceCallbackContext&) override {
        const auto t0 = juce::Time::getHighResolutionTicks();
        const float* x = (numIn > 0 && in[0] != nullptr) ? in[0] : silence.data();
        float* L = numOut > 0 ? out[0] : nullptr;
        float* R = numOut > 1 ? out[1] : nullptr;

        float pk = 0;
        for (int i = 0; i < n; ++i) pk = std::max(pk, std::abs(x[i]));
        if (pk > inPeak.load(std::memory_order_relaxed)) inPeak.store(pk, std::memory_order_relaxed);

        if (L) {
            if (measuring.load(std::memory_order_acquire)) {
                measureBlock(x, L, n);
                if (R) std::copy(L, L + n, R);
            } else if (monitorOn.load(std::memory_order_relaxed) && dsp) {
                for (int done = 0; done < n;) {   // DSP takes at most maxN frames at a time
                    const int k = std::min(n - done, DSP::maxN);
                    dsp->process(x + done, L + done, (R ? R + done : spareR.data()), k);
                    done += k;
                }
            } else {
                std::fill(L, L + n, 0.0f);
                if (R) std::fill(R, R + n, 0.0f);
            }
        }
        for (int c = 2; c < numOut; ++c) if (out[c]) std::fill(out[c], out[c] + n, 0.0f);

        const double used = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0);
        const float load = (float) (used / (n / sr));
        loadAvg.store(loadAvg.load(std::memory_order_relaxed) * 0.97f + load * 0.03f, std::memory_order_relaxed);
        if (load > loadPeak.load(std::memory_order_relaxed)) loadPeak.store(load, std::memory_order_relaxed);
        lastBlock.store(n, std::memory_order_relaxed);
    }

    // MARK: round-trip measurement (audio thread state machine; the UI only starts it and reads results when done)

    // Each trial plays a short windowed chirp and records the input; the UI thread then finds the chirp in each
    // recording by cross-correlation, which works far below the noise (speaker -> guitar pickup is a weak path).
    static constexpr int trials = 7;
    std::atomic<bool> measuring { false }, measureDone { false };
    int mPhase = 0, mTrial = 0; long mClock = 0, mEmit = 0, mPhaseStart = 0;
    std::vector<float> chirp, mRec;   // allocated in startMeasure (UI thread) before measuring is set
    int mListen = 0;

    void startMeasure() {
        if (measuring.load()) return;
        const int T = (int) (0.020 * sr);   // 20 ms, 300 Hz -> 3 kHz, Hann windowed
        chirp.assign((size_t) T, 0.0f);
        for (int k = 0; k < T; ++k) {
            const double t = k / sr, dur = T / sr, f0 = 300, f1 = 3000;
            const double ph = 2 * 3.141592653589793 * (f0 * t + (f1 - f0) * t * t / (2 * dur));
            chirp[(size_t) k] = (float) (0.9 * std::sin(ph) * (0.5 - 0.5 * std::cos(2 * 3.141592653589793 * k / (T - 1))));
        }
        mListen = (int) (0.5 * sr);
        mRec.assign((size_t) (mListen * trials), 0.0f);
        mPhase = 0; mTrial = 0; mClock = 0; mPhaseStart = 0;
        measureDone.store(false);
        measuring.store(true, std::memory_order_release);
        note("Measuring round trip: 7 short chirps. Hold still; speaker against the mic, a guitar pickup, or a cable from output to input.");
    }

    void measureBlock(const float* x, float* L, int n) {
        const long quiet = (long) (0.3 * sr), T = (long) chirp.size();
        for (int i = 0; i < n; ++i, ++mClock) {
            float y = 0;
            if (mPhase == 0) {   // gap so the previous chirp has died away
                if (mClock - mPhaseStart >= quiet) { mPhase = 1; mEmit = mClock; }
            }
            if (mPhase == 1) {   // play the chirp and record from the moment it starts
                const long k = mClock - mEmit;
                if (k < T) y = chirp[(size_t) k];
                mRec[(size_t) (mTrial * mListen + k)] = x[i];
                if (k + 1 >= mListen) {
                    mPhase = 0; mPhaseStart = mClock + 1;
                    if (++mTrial >= trials) { measuring.store(false, std::memory_order_release); measureDone.store(true, std::memory_order_release); }
                }
            }
            L[i] = y;
        }
    }

    void finishMeasure() {
        const int T = (int) chirp.size(), lags = mListen - T;
        std::vector<int> got; juce::String all; float peak = 0;
        std::vector<float> c((size_t) lags);
        for (int t = 0; t < trials; ++t) {
            const float* r = mRec.data() + (size_t) t * (size_t) mListen;
            for (int i = 0; i < mListen; ++i) peak = std::max(peak, std::abs(r[i]));
            double sumSq = 0; int best = 0; float bestV = 0;
            for (int lag = 0; lag < lags; ++lag) {
                float acc = 0;
                for (int k = 0; k < T; ++k) acc += r[lag + k] * chirp[(size_t) k];
                c[(size_t) lag] = acc; sumSq += (double) acc * acc;
                if (std::abs(acc) > bestV) { bestV = std::abs(acc); best = lag; }
            }
            const double score = bestV / std::sqrt(sumSq / lags + 1e-30);   // peak over the correlation's RMS
            all << juce::String(best / sr * 1000, 1) << "ms(x" << juce::String(score, 0) << ") ";
            if (score >= 8) got.push_back(best);
        }
        const juce::String detail = " Per chirp: " + all.trim() + ". Input peak " + juce::String(juce::Decibels::gainToDecibels(peak), 1) + " dBFS.";
        if (got.empty()) { note("Round trip: chirp not found (needs a peak score of 8 or more)." + detail); return; }
        std::sort(got.begin(), got.end());
        rtMs = got[got.size() / 2] / sr * 1000;
        note("Round trip: " + juce::String(rtMs, 1) + " ms median, " + juce::String((int) got.size()) + "/" + juce::String(trials) + " found." + detail);
    }

    // MARK: NAM

    std::vector<Capture> caps;
    LevNam* curNam = nullptr; float curGain = 1;
    std::vector<LevNam*> retired;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    int loadGen = 0;

    void chooseAmp(int index) {
        const int gen = ++loadGen;
        if (index < 0 || index >= (int) caps.size()) { swapNam(nullptr, 1); return; }
        const Capture c = caps[(size_t) index];
        const double rate = sr;
        note("Loading " + c.name + "...");
        juce::Thread::launch([this, c, rate, gen, flag = alive] {
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            Loaded o = loadCapture(c, rate);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            juce::MessageManager::callAsync([this, o, ms, gen, flag, name = c.name] {
                if (!flag->load() || gen != loadGen) { lev_nam_free(o.m); return; }
                if (!o.m) { note("Couldn't load " + name + ": " + o.error); return; }
                swapNam(o.m, o.gain);
                note("Loaded " + name + " in " + juce::String(ms, 0) + " ms"
                     + (o.expected > 0 && std::abs(o.expected - sr) > 1 ? " (made at " + juce::String(o.expected, 0) + " Hz, device runs " + juce::String(sr, 0) + ")" : ""));
            });
        });
    }

    void swapNam(LevNam* m, float gain) {
        LevNam* old = curNam;
        {
            const juce::ScopedLock sl(dspLock);
            curNam = m; curGain = gain;
            if (dsp) dsp->setNam(m, gain);
        }
        setRig(params, m != nullptr, cab.getToggleState());
        if (old) juce::Timer::callAfterDelay(1000, [old] { lev_nam_free(old); });   // after the audio thread has moved on
    }

    // MARK: offline benchmark

    std::atomic<bool> benching { false };
    juce::StringArray benchLines;

    void runBenchmark() {
        if (benching.exchange(true)) return;
        bench.setEnabled(false);
        benchLines.clear();
        note("Benchmark: each capture through gate, NAM and cab for 5 s of audio at 48 kHz, 128-frame blocks. Takes a while.");
        juce::Thread::launch([this, flag = alive] {
            const double rate = 48000; const int block = 128; const long total = (long) (rate * 5);
            std::vector<float> in((size_t) block), l((size_t) block), r((size_t) block);
            const auto cabIR = renderCab(CABS[0], 0.3, rate);
            auto run = [&](LevNam* m) {
                Params p; setRig(p, m != nullptr, true);
                DSP d(rate, p); d.setIR(cabIR);
                if (m) d.setNam(m, 1);
                const auto t0 = juce::Time::getHighResolutionTicks();
                for (long t = 0; t < total; t += block) {
                    for (int i = 0; i < block; ++i) in[(size_t) i] = testInput(t + i, rate);
                    d.process(in.data(), l.data(), r.data(), block);
                }
                return juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0) / (total / rate);
            };
            auto post = [this, flag](juce::String line) {
                juce::MessageManager::callAsync([this, flag, line] { if (flag->load()) { benchLines.add(line); note(line); } });
            };
            post("Built-in amp + cab: " + pct(run(nullptr)) + " of one core");
            for (const auto& c : caps) {
                if (!flag->load()) return;
                char err[256] = {};
                LevNam* m = lev_nam_load(unpack(c).getFullPathName().toRawUTF8(), err, sizeof err);
                if (!m) { post(c.name + ": couldn't load (" + juce::String(err) + ")"); continue; }
                lev_nam_reset(m, rate, DSP::maxN);
                const double cost = run(m);
                lev_nam_free(m);
                post((cost < 0.4 ? "ok    " : cost < 0.7 ? "tight " : "HEAVY ") + pct(cost) + "  " + c.name);
            }
            juce::MessageManager::callAsync([this, flag] {
                if (!flag->load()) return;
                benching.store(false); bench.setEnabled(true);
                note("Benchmark done. ok < 40% of one core, tight < 70%, HEAVY above (likely to drop out live).");
            });
        });
    }

    // MARK: UI

    juce::Label status;
    juce::ComboBox amp, buffer;
    juce::Label bufferLabel { {}, "Buffer (frames)" };
    juce::ToggleButton monitor { "Monitor" }, cab { "Cab" };
    juce::TextButton measure { "Measure round trip" }, bench { "Benchmark captures" }, copy { "Copy report" };
    juce::TextEditor log;
    double rtMs = -1;

    int safeTop() const {
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) return (int) d->safeAreaInsets.getTop();
        return 0;
    }

    void note(const juce::String& s) {
        log.moveCaretToEnd(); log.insertTextAtCaret(s + "\n");
        juce::Logger::writeToLog("LevProbe: " + s);   // logcat, so results can be read over adb without the screen
    }

    juce::String deviceSummary() const {
        juce::String s;
        auto* d = devices.getCurrentAudioDevice();
        if (!d) return "No audio device";
        const int n = d->getCurrentBufferSizeSamples();
        const double rate = d->getCurrentSampleRate();
        const int inL = d->getInputLatencyInSamples(), outL = d->getOutputLatencyInSamples();
        s << devices.getCurrentAudioDeviceType() << ": " << d->getName() << "\n"
          << juce::String(rate, 0) << " Hz, buffer " << n << " (" << juce::String(n / rate * 1000, 2) << " ms), callback blocks " << lastBlock.load() << "\n"
          << "Reported latency: in " << juce::String(inL / rate * 1000, 1) << " ms + out " << juce::String(outL / rate * 1000, 1) << " ms = "
          << juce::String((inL + outL) / rate * 1000, 1) << " ms\n"
          << "Callback load: avg " << pct(loadAvg.load()) << ", peak " << pct(loadPeak.load()) << "\n"
          << "Dropouts (xruns): " << d->getXRunCount() << ", input backlog dropped: "
          << juce::String(juce::levInputDropped.load() / rate * 1000, 0) << " ms\n";
        return s;
    }

    juce::String report() const {
        juce::String s;
        s << "Leviathan Probe report\n"
          << juce::SystemStats::getDeviceManufacturer() << " " << juce::SystemStats::getDeviceDescription()
          << ", " << juce::SystemStats::getOperatingSystemName() << ", " << juce::SystemStats::getNumCpus() << " CPUs\n"
          << deviceSummary()
          << "Measured round trip: " << (rtMs > 0 ? juce::String(rtMs, 1) + " ms" : juce::String("not measured")) << "\n"
          << "Amp: " << amp.getText() << ", cab " << (cab.getToggleState() ? "on" : "off") << ", monitor " << (monitor.getToggleState() ? "on" : "off") << "\n";
        if (!benchLines.isEmpty()) s << "\nBenchmark (% of one core, 48 kHz, 128-frame blocks):\n" << benchLines.joinIntoString("\n") << "\n";
        s << "\nLog:\n" << log.getText();
        return s;
    }

    void timerCallback() override {
        if (measureDone.exchange(false)) finishMeasure();
        const float pk = inPeak.exchange(0);
        status.setText(deviceSummary() + "Input peak: " + (pk > 0 ? juce::String(juce::Decibels::gainToDecibels(pk), 1) + " dBFS" : juce::String("silent"))
                       + (measuring.load() ? "\nMeasuring..." : ""), juce::dontSendNotification);
        measure.setEnabled(!measuring.load());
        if (++ticks % 50 == 0)   // every 5 s, so a session can be followed over adb without the screen
            juce::Logger::writeToLog("LevProbe status: " + deviceSummary().replace("\n", " | ") + "amp " + amp.getText()
                                     + ", monitor " + (monitor.getToggleState() ? "on" : "off") + ", input peak "
                                     + juce::String(juce::Decibels::gainToDecibels(pk), 1) + " dBFS");
    }
    int ticks = 0;
};

class ProbeApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Leviathan Probe"; }
    const juce::String getApplicationVersion() override { return "0.1"; }
    void initialise(const juce::String&) override {
        juce::Desktop::setScreenSaverEnabled(false);   // keep the screen on: a player mid-song can't tap to wake it
        window = std::make_unique<Window>();
    }
    void shutdown() override { window.reset(); }

private:
    struct Window : juce::DocumentWindow {
        Window() : juce::DocumentWindow("Leviathan Probe", juce::Colours::black, 0) {
            setUsingNativeTitleBar(true);
            setContentOwned(new ProbeComponent(), true);
           #if JUCE_ANDROID || JUCE_IOS
            setFullScreen(true);
           #else
            centreWithSize(getWidth(), getHeight());
           #endif
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::quit(); }
    };
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION(ProbeApp)
