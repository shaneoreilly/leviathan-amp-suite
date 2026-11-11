#include "Model.h"
#include <BinaryData.h>
#include "Sessions.h"
#include "Midi.h"
#include "Updater.h"
#include "DebugLog.h"
#include "SysInfo.h"

namespace lev {

const std::vector<double> Model::zoomSteps = { 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.25, 1.4, 1.6 };
/// "|" starts a new row.
static const juce::StringArray defaultPanels { "input", "master", "gate", "pitch", "eq", "doubler", "|", "amp", "ampb", "cab", "reverb", "delay" };
static const juce::Colour channelColours[3] = { juce::Colour::fromFloatRGBA(0.35f, 0.7f, 1, 1), juce::Colour::fromFloatRGBA(0.91f, 0.57f, 0.23f, 1),
                                                juce::Colour::fromFloatRGBA(1, 0.3f, 0.3f, 1) };

juce::String mmss(double t) {
    const int s = std::max(0, (int) std::floor(t));
    return juce::String(s / 60) + ":" + juce::String(s % 60).paddedLeft('0', 2);
}

juce::String Model::Info::breakdown() const {
    return juce::String(inMs, 1) + " ms in + " + juce::String(bufferMs, 1) + " ms buffer + " + juce::String(outMs, 1) + " ms out";
}

Model::Model() {
    juce::PropertiesFile::Options o;
    o.applicationName = "Leviathan"; o.filenameSuffix = "settings"; o.folderName = "Leviathan";
    o.osxLibrarySubFolder = "Application Support"; o.storageFormat = juce::PropertiesFile::storeAsXML;
    propsFile = std::make_unique<juce::PropertiesFile>(o);
    auto& p = props();

    if (auto v = juce::JSON::parse(p.getValue("settings")); v.isObject()) settings = Settings::fromJSON(v);
    if (auto v = juce::JSON::parse(p.getValue("presets")); auto* obj = v.getDynamicObject())
        for (const auto& kv : obj->getProperties()) userPresets[kv.name.toString()] = Settings::fromJSON(kv.value);
    presetName = p.getValue("preset", FACTORY[0].name);
    favorites = juce::StringArray::fromLines(p.getValue("favorites")); favorites.removeEmptyStrings();
    if (auto v = juce::JSON::parse(p.getValue("presetMeta")); auto* obj = v.getDynamicObject())
        for (const auto& kv : obj->getProperties())
        {
            auto& mt = presetMeta[kv.name.toString()];
            mt.created = (juce::int64) kv.value.getProperty("created", 0); mt.modified = (juce::int64) kv.value.getProperty("modified", 0);
            mt.played = (juce::int64) kv.value.getProperty("played", 0); mt.imported = (juce::int64) kv.value.getProperty("imported", 0);
            if (auto* t = kv.value.getProperty("tags", {}).getArray()) for (auto& x : *t) mt.tags.add(x.toString());
        }
    for (const auto& [n, s] : userPresets)   // presets saved before dates were kept: date them from now
        if (presetMeta[n].shownModified() == 0) presetMeta[n].imported = juce::Time::currentTimeMillis();
    presetSort = juce::jlimit(0, 4, p.getIntValue("presetSort", 0));
    presetSortReversed = p.getBoolValue("presetSortReversed", false);
    tuningIndex = p.getIntValue("tuning", 1); a4 = p.getDoubleValue("a4", 440);
    namCalibrate = p.getBoolValue("namCalibrate", false); interfaceLevel = p.getDoubleValue("interfaceLevel", 12);
    bpm = p.getDoubleValue("bpm", 120); beatsPerBar = p.getIntValue("beatsPerBar", 4); metroVol = p.getDoubleValue("metroVol", -12);
    if (p.containsKey("accent")) accentPick = juce::Colour::fromString(p.getValue("accent"));
    zoom = p.getDoubleValue("zoom", onAndroid ? 0.8 : 1.0);   // a phone is narrower than the smallest desktop window
    explainOnHover = p.getBoolValue("explainOnHover", true);
    {
        auto saved = juce::StringArray::fromTokens(p.getValue("panelOrder2"), ",", "");
        for (const auto& id : saved) if (defaultPanels.contains(id)) panelOrder.addIfNotAlreadyThere(id);
        // a panel added in a new version goes in after its neighbour in the default order (Doubler after the EQ), not at the end
        for (int k = 0; k < defaultPanels.size(); ++k) {
            const auto& id = defaultPanels[k];
            if (panelOrder.contains(id)) continue;
            int at = panelOrder.size();
            for (int j = k - 1; j >= 0; --j) if (panelOrder.contains(defaultPanels[j])) { at = panelOrder.indexOf(defaultPanels[j]) + 1; break; }
            panelOrder.insert(at, id);
        }
    }
    collapsed = juce::StringArray::fromLines(p.getValue("collapsed")); collapsed.removeEmptyStrings();

    captures = Capture::loadAll();
    rescanNam();
    irLib.scan(48000);
    cabIRs.scan();
    if (!irLib.isEmpty() && !p.getBoolValue("godsCabIntro")) {           // first run with the IR pack: switch to it
        settings.set("cabModel", CAB_LIB); settings.set("cabHighCut", 10000.0); p.setValue("godsCabIntro", true);
    }

    debuglog::setEnabled(p.getBoolValue("debugLog"));
    Sessions::chosen = juce::File(p.getValue("recordingsFolder"));
    sessions = std::make_unique<Sessions>(*this);
    if (Sessions::chosenMissing()) {   // e.g. a cloud drive that isn't running: record locally for now, and say so
        status = "Your recordings folder (" + Sessions::chosen.getFullPathName() + ") isn't available, so new recordings go to "
               + Sessions::defaultRoot().getFullPathName() + " for now. "
               + (onAndroid && !hasAllFilesAccess() ? "Leviathan needs All files access for it." : "Is the drive connected?");
        fixIn("Library");
    }
    midi = std::make_unique<Midi>(*this);
    updater = std::make_unique<Updater>(*this);

    applyAll();

    // audio: restore the last device setup, or pick the lowest-latency driver type Windows offers
    std::unique_ptr<juce::XmlElement> saved = juce::parseXML(p.getValue("audioState"));
    // LEVIATHAN_NO_INPUT=1: output only (UI testing without a microphone permission prompt)
    const int wantIns = juce::SystemStats::getEnvironmentVariable("LEVIATHAN_NO_INPUT", {}).isNotEmpty() ? 0 : 8;
    juce::String err = devices.initialise(wantIns, 2, saved.get(), true);
    if (!saved) {
        juce::OwnedArray<juce::AudioIODeviceType> types;
        for (auto* t : devices.getAvailableDeviceTypes()) {
            t->scanForDevices();
            if (t->getDeviceNames(false).isEmpty()) continue;
            const auto n = t->getTypeName();
            if (n == "ASIO") { devices.setCurrentAudioDeviceType(n, true); break; }
            if (n.contains("Low Latency")) devices.setCurrentAudioDeviceType(n, true);
        }
        auto setup = devices.getAudioDeviceSetup();
        setup.bufferSize = 128;
       #if JUCE_ANDROID
        setup.sampleRate = 48000;   // the bundled captures and God's Cab's main set are 48 kHz
       #endif
        devices.setAudioDeviceSetup(setup, true);
    }
   #if JUCE_ANDROID
    // Phones: the lowest buffer the device offers, until you pick one in Settings > Audio; from then on yours is
    // kept (saveState notes the choice). Leviathan never changes it by itself there.
    if (!p.getBoolValue("bufferChosen"))
        if (auto* d = devices.getCurrentAudioDevice(); d != nullptr && !d->getAvailableBufferSizes().isEmpty()) {
            auto setup = devices.getAudioDeviceSetup();
            setup.bufferSize = d->getAvailableBufferSizes()[0];
            devices.setAudioDeviceSetup(setup, true);
        }
   #endif
    ensureInputs();
    syncMute();
    midi->start();
    updater->checkOnLaunch();
    // LEVIATHAN_SCREEN=presets|recordings|settings: open on that screen (UI testing)
    const auto scr = juce::SystemStats::getEnvironmentVariable("LEVIATHAN_SCREEN", {});
    if (scr == "presets") screen = Screen::presets; else if (scr == "recordings") screen = Screen::recordings; else if (scr == "settings") screen = Screen::settings;
    // Android opens with the amp off (nothing plays until you tap Start) and the device released while it's off;
    // the device setup is kept and reopened by start(). Desktop starts playing straight away.
    if (onAndroid) devices.closeAudioDevice();
    else start();
    startTimerHz(30);
}

Model::~Model() {
    alive->store(false);
    stopTimer();
    stop();
    devices.closeAudioDevice();   // the engine is gone after this, so the model can't be in use
    lev_nam_free(namModel); namModel = nullptr;
    saveState();
    sessions.reset();
    midi.reset();
    updater.reset();
}

// MARK: - settings

void Model::setSettings(const Settings& n) {
    if (n == settings) return;
    const Settings old = settings;
    settings = n;
    settingsChanged(old);
}

void Model::set(const juce::Identifier& key, const juce::var& v) {
    Settings n = settings;
    n.set(key, v);
    setSettings(n);
}

void Model::confirmLoad(std::function<void()> load) {
    if (!presetModified()) { load(); return; }
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
                                     .withTitle(juce::String::fromUTF8("Save changes to \u201c") + presetName + juce::String::fromUTF8("\u201d?"))
                                     .withMessage(juce::String::fromUTF8("You\u2019ve changed this preset since it was last saved."))
                                     .withButton("Save", 1).withButton(juce::String::fromUTF8("Don\u2019t save"), 2).withButton("Cancel", 0),
                                 [this, load](int r) {   // 1 = Save, 2 = Don't save, 0 = Cancel
                                     if (r == 0) return;
                                     if (r == 1) saveCurrent();
                                     load();
                                 });
}

void Model::undo() {
    if (undoStack.empty()) return;
    auto [old, name] = undoStack.back(); undoStack.pop_back();
    undoing = true; setSettings(old); presetName = name; undoing = false;
    lastEditMs = 0; changed();
}

void Model::settingsChanged(const Settings& old) {
    if (!undoing) {
        const auto now = juce::Time::currentTimeMillis();
        if (now - lastEditMs > 600 || undoStack.empty()) {
            undoStack.push_back({ old, presetName });
            if (undoStack.size() > 100) undoStack.erase(undoStack.begin());
        }
        lastEditMs = now;
    }
    applyAll();
    if (settings.s("nam") != old.s("nam")) updateNam();
    if (settings.s("ampBNam") != old.s("ampBNam")) updateNamB();
    if (settings.i("cabModel") != old.i("cabModel") || settings.d("cabMic") != old.d("cabMic") || settings.s("irA") != old.s("irA")
        || settings.s("irB") != old.s("irB") || settings.d("irBlend") != old.d("irBlend") || settings.s("capture") != old.s("capture")
        || settings.s("irFile") != old.s("irFile"))
        updateIR();
    saveCountdown = 12;   // ~0.4 s
    changed();
}

void Model::applyAll() {
    if (auto* c = activeCapture()) c->apply(params);
    params.capOn = activeCapture() != nullptr;
    params.namOn = namReady();
    params.bpm = (float) bpm; params.beatsPerBar = beatsPerBar; params.metroVol = (float) metroVol; params.metroOn.store(metroOn);
    settings.apply(params);
    if (auto ms = syncedDelayMs()) { params.delayTime = (float) *ms; params.version.fetch_add(1); }
}

void Model::saveState() {
    auto& p = props();
    p.setValue("settings", settings.toJSON());
    p.setValue("preset", presetName);
    p.setValue("tuning", tuningIndex); p.setValue("a4", a4);
    p.setValue("bpm", bpm); p.setValue("beatsPerBar", beatsPerBar); p.setValue("metroVol", metroVol);
    if (accentPick) p.setValue("accent", accentPick->toString()); else p.removeValue("accent");
    p.setValue("zoom", zoom);
    p.setValue("panelOrder2", panelOrder.joinIntoString(","));
    p.setValue("explainOnHover", explainOnHover);
    p.setValue("collapsed", collapsed.joinIntoString("\n"));
    p.setValue("favorites", favorites.joinIntoString("\n"));
    {
        auto* o = new juce::DynamicObject();
        for (const auto& [k, mt] : presetMeta) {
            auto* e = new juce::DynamicObject();
            e->setProperty("created", mt.created); e->setProperty("modified", mt.modified); e->setProperty("played", mt.played); e->setProperty("imported", mt.imported);
            if (!mt.tags.isEmpty()) { juce::Array<juce::var> t; for (auto& x : mt.tags) t.add(x); e->setProperty("tags", t); }
            o->setProperty(k, juce::var(e));
        }
        p.setValue("presetMeta", juce::JSON::toString(juce::var(o), juce::JSON::FormatOptions{}.withSpacing(juce::JSON::Spacing::none)));
        p.setValue("presetSort", presetSort);
        p.setValue("presetSortReversed", presetSortReversed);
    }
    if (auto xml = devices.createStateXml()) p.setValue("audioState", xml->toString());
    // Android starts on the lowest buffer and never changes it itself, so any other size is one you picked
    if (auto* d = devices.getCurrentAudioDevice(); onAndroid && d != nullptr && !d->getAvailableBufferSizes().isEmpty()
                                                   && d->getCurrentBufferSizeSamples() != d->getAvailableBufferSizes()[0])
        p.setValue("bufferChosen", true);
    p.saveIfNeeded();
}

// MARK: - engine

bool Model::hasInput() const {
    auto* d = devices.getCurrentAudioDevice();
    return d && d->getActiveInputChannels().countNumberOfSetBits() > 0;
}

void Model::ensureInputs() {
    // enable up to 8 inputs so "Input channel" can pick any of them (inputs arrive in this order)
    if (juce::SystemStats::getEnvironmentVariable("LEVIATHAN_NO_INPUT", {}).isNotEmpty()) return;
    auto setup = devices.getAudioDeviceSetup();
    if (auto* d = devices.getCurrentAudioDevice()) {
        const int n = std::min(8, d->getInputChannelNames().size());
        juce::BigInteger want; want.setRange(0, n, true);
        if (setup.inputChannels != want || setup.useDefaultInputChannels) {
            setup.useDefaultInputChannels = false; setup.inputChannels = want;
            setup.useDefaultOutputChannels = true;
            devices.setAudioDeviceSetup(setup, true);
        }
    }
}

void Model::start(bool withInput) {
    if (running) {
        if (withInput && playbackOnly) {   // already playing a recording back: just turn the input on
            playbackOnly = false; inputOff.store(false);
            debuglog::write("audio", "input on");
            changed();
        }
        return;
    }
    if (devices.getCurrentAudioDevice() == nullptr) devices.restartLastAudioDevice();   // closed while off (Android)
    if (devices.getCurrentAudioDevice() == nullptr) {
        const auto err = devices.initialise(8, 2, nullptr, true);
        if (devices.getCurrentAudioDevice() == nullptr) {
            status = err.isNotEmpty() ? err : juce::String("Pick an input and output device in Settings."); fixIn("Audio");
            changed(); return;
        }
        ensureInputs();
    }
    callbackLast = 0;
    playbackOnly = !withInput; inputOff.store(!withInput);   // before the callback runs: never a moment of live input
    devices.addAudioCallback(this);
    running = true;
    debuglog::write("audio", withInput ? "started" : "started for playback (input off)");
    adviseOverloads = true; recentOverloads.clear(); startedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    lastXRuns = devices.getXRunCount();
    perfXRuns = lastXRuns;   // the driver's count restarts with the device; the next perf line counts from here
    perfAt = startedAt; perfNam = params.namSeconds.load(); callbackPeak.store(0);
    callbackCount.store(0); callbackGapMax.store(0); callbackPriority.store(sysinfo::unknownPriority);
    refreshInfo();
    changed();
}

void Model::stop() {
    if (!running) return;
    devices.removeAudioCallback(this);
    running = false; playbackOnly = false; inputOff.store(false);
    debuglog::write("audio", "stopped");
    params.inPeak.store(0); params.outPeak.store(0);
    // Android: off means the audio device is released too (its exclusive streams would hold up other apps' sound)
    if (onAndroid) { saveState(); devices.closeAudioDevice(); }
    changed();
}

void Model::setDebugLog(bool on) {
    props().setValue("debugLog", on);
    debuglog::setEnabled(on);
    if (on) { loggedDevice = {}; refreshInfo(); debuglog::write("audio", running ? "running" : "off"); }
    changed();
}

void Model::enterBackground() {
    if (pausedForBackground || sessions->recording || sessions->liveReamp) return;
    debuglog::write("app", "to the background: audio released");
    pausedForBackground = true;
    wasRunningBeforeBackground = live();   // playback-only audio stays off: the playback stopped with it
    saveState();
    stop();
    devices.closeAudioDevice();
}

void Model::enterForeground() {
    if (!pausedForBackground) return;
    debuglog::write("app", "back in front");
    pausedForBackground = false;
    devices.restartLastAudioDevice();
    if (wasRunningBeforeBackground) start();
    refreshInfo(); changed();
}

void Model::restartAudio() {
    ensureInputs();
    if (running) { stop(); start(); }
    saveState();
}

juce::String Model::deviceSignature() const {
    const auto s = devices.getAudioDeviceSetup();
    return devices.getCurrentAudioDeviceType() + "|" + s.inputDeviceName + "|" + s.outputDeviceName + "|" + juce::String(s.sampleRate)
         + "|" + juce::String(s.bufferSize) + "|" + s.inputChannels.toString(2) + "|" + s.outputChannels.toString(2)
         + "|" + (s.useDefaultInputChannels ? "1" : "0") + (s.useDefaultOutputChannels ? "1" : "0");
}

void Model::refreshInfo() {
    info = {};
    if (auto* d = devices.getCurrentAudioDevice()) {
        const double sr = d->getCurrentSampleRate();
        info.sampleRate = sr; info.buffer = d->getCurrentBufferSizeSamples();
        info.inMs = sr > 0 ? d->getInputLatencyInSamples() * 1000.0 / sr : 0;
        info.outMs = sr > 0 ? d->getOutputLatencyInSamples() * 1000.0 / sr : 0;
        info.bufferMs = sr > 0 ? info.buffer * 1000.0 / sr : 0;
        info.type = d->getTypeName(); info.inName = devices.getAudioDeviceSetup().inputDeviceName;
        info.outName = devices.getAudioDeviceSetup().outputDeviceName;
    }
    const auto line = info.type + ": in \"" + info.inName + "\", out \"" + info.outName + "\", " + juce::String(juce::roundToInt(info.sampleRate)) + " Hz, buffer "
                    + juce::String(info.buffer) + " (" + juce::String(info.bufferMs, 1) + " ms); latency in " + juce::String(info.inMs, 1) + " + out "
                    + juce::String(info.outMs, 1) + " = " + juce::String(info.latencyMs(), 1) + " ms";
    if (line != loggedDevice) { loggedDevice = line; debuglog::write("audio", line); }
}

void Model::audioDeviceAboutToStart(juce::AudioIODevice* d) {
    const double sr = d->getCurrentSampleRate();
    auto fresh = std::make_unique<DSP>(sr, params);
    zeros.assign((size_t) DSP::maxN, 0.0f); scratchL.assign((size_t) DSP::maxN, 0.0f); scratchR.assign((size_t) DSP::maxN, 0.0f);
    {
        const juce::SpinLock::ScopedLockType sl(dspLock);
        dsp = std::move(fresh);
        dspRate = sr;
    }
    juce::MessageManager::callAsync([this] { irLib.scan(dspRate); updateIR(); updateNam(); updateNamB(); refreshInfo(); changed(); });
}

void Model::audioDeviceStopped() {
    const juce::SpinLock::ScopedLockType sl(dspLock);
    dsp.reset();
}

void Model::audioDeviceError(const juce::String& msg) {
    juce::MessageManager::callAsync([this, msg] { status = "Audio device error: " + msg; fixIn("Audio"); changed(); });
}

void Model::audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int n,
                                             const juce::AudioIODeviceCallbackContext&) {
    const auto t0 = juce::Time::getHighResolutionTicks();
    struct Peak {   // this callback's share of its time budget, for the diagnostic log (a max, read and reset by the timer)
        Model& m; juce::int64 t0; int n;
        ~Peak() {
            const double used = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0), budget = n / std::max(m.dspRate, 1.0);
            const float load = (float) (used / budget);
            if (load > m.callbackPeak.load(std::memory_order_relaxed)) m.callbackPeak.store(load, std::memory_order_relaxed);
        }
    } peak { *this, t0, n };
    if (callbackLast != 0) {
        const float gap = (float) juce::Time::highResolutionTicksToSeconds(t0 - callbackLast);
        if (gap > callbackGapMax.load(std::memory_order_relaxed)) callbackGapMax.store(gap, std::memory_order_relaxed);
    }
    callbackLast = t0;
    if (callbackCount.fetch_add(1, std::memory_order_relaxed) == 0 && callbackPriority.load(std::memory_order_relaxed) == sysinfo::unknownPriority)
        callbackPriority.store(sysinfo::currentThreadPriority(), std::memory_order_relaxed);
    DSP* d = dsp.get();
    if (d == nullptr || numOut == 0) { for (int c = 0; c < numOut; ++c) if (out[c]) juce::FloatVectorOperations::clear(out[c], n); return; }
    const int ch = juce::jlimit(0, std::max(numIn - 1, 0), params.inputChannel);
    const float* src = numIn > 0 && in[ch] && !inputOff.load(std::memory_order_relaxed) ? in[ch] : zeros.data();
    for (int off = 0; off < n; off += DSP::maxN) {
        const int k = std::min(DSP::maxN, n - off);
        float* l = out[0] ? out[0] + off : scratchL.data();
        float* r = numOut > 1 && out[1] ? out[1] + off : scratchR.data();
        d->process(src + (src == zeros.data() ? 0 : off), l, r, k);
    }
    for (int c = 2; c < numOut; ++c) if (out[c]) juce::FloatVectorOperations::clear(out[c], n);
}

void Model::resolve(const juce::String& where) {
    if (where == "amp:on") { start(); return; }
    if (where.isEmpty()) return;
    props().setValue("settingsSection", where);
    settingsJump = where;
    setScreen(Screen::settings);
}

void Model::overloaded() {
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    if (now - startedAt < 3) return;   // the first seconds after a start settle on their own
    recentOverloads.erase(std::remove_if(recentOverloads.begin(), recentOverloads.end(), [&](double t) { return now - t > 10; }),
                          recentOverloads.end());
    recentOverloads.push_back(now);
    if (recentOverloads.size() < 4) return;
    recentOverloads.clear();
    juce::String why;
    if (!onAndroid && info.type.isNotEmpty() && info.type != "ASIO")
        why << " An ASIO driver for your interface (or \"Windows Audio (Exclusive Mode)\") is usually much more stable at low latency.";
    if (info.inName.isNotEmpty() && info.outName.isNotEmpty() && info.inName != info.outName)
        why << " Input (" << info.inName << ") and output (" << info.outName << ") are separate devices with separate clocks; one interface for both is most stable.";
    // keep dropouts from going on: step the buffer up to the next size the driver offers, up to 1024 samples.
    // Not on Android: there the buffer only changes when you change it, so say so and point at the setting.
    if (onAndroid) {
        if (adviseOverloads) {
            adviseOverloads = false;
            status = "Audio is dropping out (crackles). A bigger buffer in Settings > Audio stops it, at a little more latency." + why;
            fixIn("Audio"); changed();
        }
        return;
    }
    if (auto* d = devices.getCurrentAudioDevice()) {
        const int cur = d->getCurrentBufferSizeSamples();
        int bigger = 0;
        for (int b : d->getAvailableBufferSizes()) if (b > cur && b <= 1024 && (bigger == 0 || b < bigger)) bigger = b;
        if (bigger > 0) {
            auto setup = devices.getAudioDeviceSetup();
            setup.bufferSize = bigger;
            juce::MessageManager::callAsync([this, setup] { devices.setAudioDeviceSetup(setup, true); startedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0; refreshInfo(); saveState(); changed(); });
            status = "Audio was dropping out (crackles), so the buffer is now " + juce::String(bigger) + " samples (Settings > Audio)." + why;
            fixIn("Audio");
            changed();
            return;
        }
    }
    if (adviseOverloads) { adviseOverloads = false; status = "Audio is still dropping out at the largest buffer." + why; fixIn("Audio"); changed(); }
}

void Model::timerCallback() {
    if (saveCountdown > 0 && --saveCountdown == 0) { saveState(); saveCountdown = -1; }
    sessions->tick();
    if (onTick) onTick();
    if (status != loggedStatus) { loggedStatus = status; if (status.isNotEmpty()) debuglog::write("message", status); }
    if (sessions->message != loggedMessage) { loggedMessage = sessions->message; if (loggedMessage.isNotEmpty()) debuglog::write("message", loggedMessage); }
    if (!running) return;
    if (++perfTick % 60 == 0 && debuglog::enabled()) {   // every 2 s at 30 Hz
        const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0, nam = params.namSeconds.load();
        const double span = std::max(now - perfAt, 1e-3);
        const int xr = devices.getXRunCount();
        // callbacks: how many arrived against how many the buffer size implies, and the longest wait between two
        juce::String timing;
        if (auto* dev = devices.getCurrentAudioDevice()) {
            const int buf = dev->getCurrentBufferSizeSamples();
            const double rate = dev->getCurrentSampleRate();
            if (buf > 0 && rate > 0)
                timing << "; callbacks " << callbackCount.exchange(0) << " of ~" << juce::roundToInt(span * rate / buf)
                       << ", longest gap " << juce::roundToInt(callbackGapMax.exchange(0) * 1000) << " ms (buffer "
                       << juce::String(buf / rate * 1000, 1) << " ms)";
        }
        // the whole machine next to Leviathan: a busy system with Leviathan steady points at another program
        const auto use = sysinfo::cpuUse();
        if (use.system >= 0)
            timing << "; CPU all apps " << juce::roundToInt(use.system) << "%, Leviathan " << juce::roundToInt(use.process) << "%";
        debuglog::write("perf", "load avg " + juce::String(juce::roundToInt(devices.getCpuUsage() * 100)) + "%, peak " + juce::String(juce::roundToInt(callbackPeak.exchange(0) * 100))
                                + "%; NAM " + juce::String(juce::roundToInt((nam - perfNam) / span * 100)) + "% of real time; dropouts +" + juce::String(xr - perfXRuns)
                                + " (" + juce::String(xr) + " total)" + timing + "; \"" + presetName + "\", amp " + (settings.s("nam").isNotEmpty() ? "NAM \"" + settings.s("nam") + "\"" : juce::String("built-in")));
        perfAt = now; perfNam = nam; perfXRuns = xr;
        // once per start: the audio thread's priority (drivers set it; a low one lets other programs interrupt audio)
        const int pri = callbackPriority.exchange(sysinfo::unknownPriority - 1);
        if (pri != sysinfo::unknownPriority && pri != sysinfo::unknownPriority - 1)
            debuglog::write("audio", "audio thread priority " + juce::String(sysinfo::priorityName(pri)));
        else if (pri == sysinfo::unknownPriority) callbackPriority.store(pri);   // no callback yet: try again next time
        // power plan / source changes while playing (a laptop unplugged, a power mode switched)
        if (const juce::String pw = sysinfo::power(); pw != loggedPower) {
            if (loggedPower.isNotEmpty()) debuglog::write("power", pw);   // the first is in the log header
            loggedPower = pw;
        }
    }
    const int x = devices.getXRunCount();
    if (x > lastXRuns) { dropouts += x - lastXRuns; lastXRuns = x; refreshInfo(); overloaded(); }
    if (++tunerTick % 15 == 0) cpu = (float) devices.getCpuUsage();
    if (tunerOn && tunerTick % 2 == 0) runTuner();
    checkInputClipping();
}

/// The interface's converter clipping (before Leviathan's input gain) makes every sound harsh and fizzy;
/// it can only be fixed with the interface's own gain control, so say so.
void Model::checkInputClipping() {
    const int c = params.inClips.load();
    if (c == lastClips) return;
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    if (c - lastClips >= 3) {
        clipTimes.erase(std::remove_if(clipTimes.begin(), clipTimes.end(), [&](double t) { return now - t > 10; }), clipTimes.end());
        clipTimes.push_back(now);
    }
    lastClips = c;
    if (clipWarned || clipTimes.size() < 3) return;
    clipWarned = true;
    status = juce::String::fromUTF8("Your interface\xe2\x80\x99s input is clipping (") + (info.inName.isNotEmpty() ? info.inName : juce::String("input"))
             + " hits full scale before Leviathan). That makes every tone harsh and fizzy. Turn the gain knob on the interface down until your hardest playing stays out of the red, then use Input Gain here if you need more level.";
    fixIn("Audio");
    changed();
}

void Model::runTuner() {
    if (dsp == nullptr) return;
    const int n = 2048, w = params.tunerWrite.load(std::memory_order_acquire);
    std::vector<float> x((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = params.tunerRing[(w - n + i + 4096) & 4095];
    auto f = detectPitch(x, dspRate);
    if (!f) return;
    const double m = 69 + 12 * std::log2(*f / a4);
    const auto& t = tuning();
    int nearest = 0;
    for (int k = 1; k < 6; ++k) if (std::abs(t.strings[(size_t) k] - m) < std::abs(t.strings[(size_t) nearest] - m)) nearest = k;
    int target;
    if (std::abs(t.strings[(size_t) nearest] - m) < 1.5) { target = t.strings[(size_t) nearest]; tunerString = nearest; }
    else { target = (int) std::round(m); tunerString = -1; }
    tunerFreq = *f;
    tunerCents = (m - target) * 100;
    tunerNote = noteName(target, t.flats) + juce::String(target / 12 - 1);
    changed();
}

// MARK: - presets

bool Model::isFactory(const juce::String& n) const {
    for (const auto& f : FACTORY) if (n == f.name) return true;
    return false;
}

std::optional<Settings> Model::presetSettings(const juce::String& n) const {
    if (auto it = userPresets.find(n); it != userPresets.end()) return it->second;
    for (const auto& f : FACTORY) if (n == f.name) { Settings s; f.fn(s); return s; }
    return std::nullopt;
}

juce::StringArray Model::presetOrder() const {
    juce::StringArray out;
    for (const auto& f : FACTORY) out.add(f.name);
    juce::StringArray mine; for (const auto& [k, v] : userPresets) if (!isFactory(k)) mine.add(k);
    mine.sort(false);
    out.addArray(mine);
    return out;
}

void Model::loadFactory(const juce::String& n) { if (userPresets.count(n)) loadUser(n); else loadFactoryOriginal(n); }
void Model::loadFactoryOriginal(const juce::String& n) {
    for (const auto& f : FACTORY) if (n == f.name) { Settings s; f.fn(s); applyPreset(s, n); return; }
}
void Model::restoreFactory(const juce::String& n) { userPresets.erase(n); persistPresets(); loadFactoryOriginal(n); }
void Model::loadAny(const juce::String& n) { if (isFactory(n)) loadFactory(n); else loadUser(n); }
void Model::loadUser(const juce::String& n) { if (auto it = userPresets.find(n); it != userPresets.end()) applyPreset(it->second, n); }

void Model::applyPreset(const Settings& p, const juce::String& name) {
    Settings n = p;
    // presets don't change your levels, input channel or tuning
    for (const char* k : { "inGain", "masterVol", "inputChannel", "pitchOn", "pitchSemis", "pitchCents", "pitchMix" }) n.set(k, settings.get(k));
    if (n.i("cabModel") == CAB_USER && !userIR) n.set("cabModel", irLib.isEmpty() ? 0 : CAB_LIB);
    if (n.i("cabModel") == CAB_LIB && irLib.isEmpty()) n.set("cabModel", 0);
    if (n.i("cabModel") == CAB_IRLIB && !cabIRs.contains(n.s("irFile"))) n.set("cabModel", irLib.isEmpty() ? 0 : CAB_LIB);
    setSettings(n);   // first, so undo records the preset name from before
    presetName = name;
    presetMeta[name].played = juce::Time::currentTimeMillis();
    props().setValue("preset", name);
    saveState();
    changed();
}

void Model::toggleFavorite(const juce::String& n) {
    if (favorites.contains(n)) favorites.removeString(n); else favorites.add(n);
    saveState(); changed();
}

void Model::renamePreset(const juce::String& oldName, const juce::String& newName) {
    const auto n = newName.trim();
    auto it = userPresets.find(oldName);
    if (n.isEmpty() || n == oldName || it == userPresets.end() || isFactory(n)) return;
    userPresets[n] = it->second; userPresets.erase(oldName);
    if (auto m = presetMeta.find(oldName); m != presetMeta.end()) { presetMeta[n] = m->second; presetMeta.erase(oldName); }
    if (favorites.contains(oldName)) { favorites.removeString(oldName); favorites.add(n); }
    if (presetName == oldName) presetName = n;
    persistPresets();
}

void Model::savePreset(const juce::String& name) {
    const auto n = name.trim();
    if (n.isEmpty()) return;
    const auto now = juce::Time::currentTimeMillis();
    auto& mt = presetMeta[n];
    if (!userPresets.count(n) && mt.created == 0 && !isFactory(n)) mt.created = now;
    mt.modified = now; mt.played = now;
    userPresets[n] = settings; presetName = n;
    persistPresets();
}

void Model::deletePreset(const juce::String& n) { userPresets.erase(n); presetMeta.erase(n); persistPresets(); }

void Model::setPresetSort(int s) { presetSort = juce::jlimit(0, 4, s); saveState(); changed(); }

juce::StringArray Model::sortedPresets(int sort, bool reversed) const {
    const auto list = presetOrder();
    if (sort <= 0 || sort == 4) {
        auto l = list;
        if (sort == 4) l.sortNatural();
        if (!reversed) return l;
        juce::StringArray r; for (int i = l.size(); --i >= 0;) r.add(l[i]); return r;
    }
    auto date = [&](const juce::String& n) -> juce::int64 {
        auto it = presetMeta.find(n);
        if (it == presetMeta.end()) return 0;
        return sort == 1 ? it->second.played : sort == 2 ? it->second.shownModified() : (it->second.created > 0 ? it->second.created : it->second.imported);
    };
    std::vector<juce::String> dated, rest;
    for (auto& n : list) (date(n) > 0 ? dated : rest).push_back(n);
    std::stable_sort(dated.begin(), dated.end(), [&](const juce::String& a, const juce::String& b) { return reversed ? date(a) < date(b) : date(a) > date(b); });
    juce::StringArray out;
    for (auto& n : dated) out.add(n);
    for (auto& n : rest) out.add(n);   // never played/saved: after the dated ones, in list order
    return out;
}

void Model::persistPresets() {
    auto* o = new juce::DynamicObject();
    for (const auto& [k, v] : userPresets) o->setProperty(k, v.toVar());
    props().setValue("presets", juce::JSON::toString(juce::var(o), juce::JSON::FormatOptions{}.withSpacing(juce::JSON::Spacing::none)));
    saveState();
    changed();
}

bool Model::presetModified() const {
    auto stored = presetSettings(presetName);
    if (!stored) return false;
    Settings cur = settings;
    for (const char* k : { "inGain", "masterVol", "inputChannel", "pitchOn", "pitchSemis", "pitchCents", "pitchMix", "cabModel" }) cur.set(k, stored->get(k));
    return cur != *stored;
}

void Model::stepPreset(int d) {
    const auto order = presetOrder();
    if (order.isEmpty()) return;
    int i = order.indexOf(presetName);
    if (i < 0) i = d > 0 ? -1 : 0;
    loadAny(order[((i + d) % order.size() + order.size()) % order.size()]);
}

// MARK: - preset files

juce::String Model::presetFileData(const juce::String& name) const {
    auto s = presetSettings(name);
    if (!s) return {};
    auto* o = new juce::DynamicObject();
    o->setProperty("format", "leviathan-preset"); o->setProperty("version", 1);
    o->setProperty("name", name); o->setProperty("app", LEVIATHAN_VERSION); o->setProperty("settings", s->toVar());
    return juce::JSON::toString(juce::var(o));
}

static juce::String fileNameFor(const juce::String& name) {
    return juce::File::createLegalFileName(name.replaceCharacter('/', '-').replaceCharacter(':', '-')) + "." + Model::presetExt;
}

void Model::exportPreset(const juce::String& name) {
    auto chooser = std::make_shared<juce::FileChooser>("Export \"" + name + "\"",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(fileNameFor(name)), "*.leviathan");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, chooser, name](const juce::FileChooser& fc) {
            const auto ok = writePicked(fc, presetFileData(name), presetExt);
            if (ok.has_value() && !*ok) { status = "Couldn't export \"" + name + "\"."; changed(); }
        });
}

void Model::exportAllPresets() {
    auto chooser = std::make_shared<juce::FileChooser>("Choose a folder for the preset files",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc) {
            const auto dir = pickedFolder(fc);
            if (!dir) return;
            juce::StringArray failed;
            const auto order = presetOrder();
            for (const auto& n : order) if (!dir->write(fileNameFor(n), presetFileData(n))) failed.add(n);
            status = failed.isEmpty() ? "Exported " + juce::String(order.size()) + " presets to " + dir->name + "."
                                      : "Couldn't export: " + failed.joinIntoString(", ");
            if (failed.isEmpty()) dir->reveal();
            changed();
        });
}

void Model::importPresetsDialog() {
    auto chooser = std::make_shared<juce::FileChooser>("Choose .leviathan preset files",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.leviathan;*.json");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
        [this, chooser](const juce::FileChooser& fc) { const auto files = pickedFiles(fc); if (!files.isEmpty()) importPresets(files); });
}

void Model::importPresets(const juce::Array<juce::File>& files) {
    juce::StringArray added, same, bad;
    for (const auto& f : files) {
        if (f.getSize() > 2 * 1024 * 1024) { bad.add(f.getFileName()); continue; }   // presets are a few KB
        const auto v = juce::JSON::parse(f.loadFileAsString());
        if (!v.isObject() || v["format"].toString() != "leviathan-preset" || !v["settings"].isObject()) { bad.add(f.getFileName()); continue; }
        const Settings s = Settings::fromJSON(v["settings"]);
        auto wanted = (v["name"].isString() ? v["name"].toString() : f.getFileNameWithoutExtension()).trim().substring(0, 120);
        if (wanted.isEmpty()) wanted = "Imported";
        if (auto existing = presetSettings(wanted); existing && *existing == s) { same.add(wanted); continue; }
        auto name = wanted; int i = 2;
        while (isFactory(name) || userPresets.count(name)) name = wanted + " " + juce::String(i++);
        userPresets[name] = s;
        presetMeta[name].imported = juce::Time::currentTimeMillis();
        added.add(name);
    }
    if (!added.isEmpty()) { persistPresets(); loadUser(added[added.size() - 1]); }
    juce::StringArray parts;
    if (!added.isEmpty()) parts.add(added.size() == 1 ? "Imported \"" + added[0] + "\"." : "Imported " + juce::String(added.size()) + " presets.");
    if (!same.isEmpty()) parts.add("Already had " + same.joinIntoString(", ") + ".");
    if (!bad.isEmpty()) parts.add("Not a Leviathan preset: " + bad.joinIntoString(", ") + ".");
    status = parts.joinIntoString(" ");
    changed();
}

// MARK: - pedals

void Model::addPedal(PedalKind k) {
    auto c = settings.chain();
    if (std::find(c.begin(), c.end(), (int) k) != c.end() || (int) c.size() >= Params::maxChain) return;
    Settings n = settings; c.push_back((int) k); n.setChain(c); n.set(pedalOnKey(k), true);
    setSettings(n);
}

void Model::removePedal(PedalKind k) {
    auto c = settings.chain();
    c.erase(std::remove(c.begin(), c.end(), (int) k), c.end());
    Settings n = settings; n.setChain(c); setSettings(n);
}

void Model::movePedal(PedalKind k, PedalKind target) {
    auto c = settings.chain();
    auto from = std::find(c.begin(), c.end(), (int) k), to = std::find(c.begin(), c.end(), (int) target);
    if (from == c.end() || to == c.end() || from == to) return;
    const int fi = (int) (from - c.begin()), ti = (int) (to - c.begin());
    c.erase(c.begin() + fi);
    c.insert(c.begin() + ti, (int) k);
    Settings n = settings; n.setChain(c); setSettings(n);
}

// MARK: - preset tags

juce::StringArray Model::autoTags(const juce::String& n) const {
    static const char* style[] = { "Clean", "Crunch", "Blues", "Rock", "Metal", "Thrash", "Doom", "Djent", "Lead", "Rhythm", "Ambient",
                                   "Funk", "Jazz", "Punk", "Fuzz", "Stoner", "Sludge", "Grunge", "Country", "Shoegaze", "Acoustic", "Bass", "Death", "Black" };
    juce::StringArray t { isFactory(n) ? "Factory" : "Yours" };
    if (auto st = presetSettings(n)) t.add(st->s("nam").isEmpty() ? "Built-in amp" : "NAM");
    juce::StringArray words; words.addTokens(n.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz "), " ", ""); words.removeEmptyStrings();
    for (auto* w : style) if (words.contains(juce::String(w).toLowerCase())) t.add(w);
    return t;
}

juce::StringArray Model::userTags(const juce::String& n) const {
    if (auto it = presetMeta.find(n); it != presetMeta.end()) return it->second.tags;
    return {};
}

void Model::setUserTags(const juce::String& n, const juce::String& commaSeparated) {
    juce::StringArray in; in.addTokens(commaSeparated, ",", ""); in.trim(); in.removeEmptyStrings();
    juce::StringArray out;
    for (auto& t : in) { bool dup = false; for (auto& o : out) dup = dup || o.equalsIgnoreCase(t); if (!dup) out.add(t); }
    presetMeta[n].tags = out;
    saveState(); changed();
}

juce::StringArray Model::tags(const juce::String& n) const {
    auto t = autoTags(n);
    for (auto& u : userTags(n)) { bool dup = false; for (auto& a : t) dup = dup || a.equalsIgnoreCase(u); if (!dup) t.add(u); }
    return t;
}

// MARK: - pitch

juce::String Model::soundsLike() const {
    const int shift = settings.b("pitchOn") ? (int) std::round(settings.d("pitchSemis")) : 0;
    auto s = tuning().strings; for (auto& m : s) m += shift;
    return tuningName(s);
}

std::vector<std::pair<juce::String, int>> Model::playAsOptions() const {
    std::vector<std::pair<juce::String, int>> out;
    for (int k = 12; k >= -12; --k) { auto s = tuning().strings; for (auto& m : s) m += k; out.emplace_back(tuningName(s), k); }
    return out;
}

void Model::playAs(int semis) {
    Settings n = settings;
    n.set("pitchSemis", (double) semis); n.set("pitchCents", 0.0); n.set("pitchOn", settings.b("pitchOn") || semis != 0);   // on when needed; stepping back to your tuning leaves it as it is
    setSettings(n);
}

void Model::setTuningIndex(int i) { tuningIndex = juce::jlimit(0, (int) TUNINGS.size() - 1, i); props().setValue("tuning", tuningIndex); changed(); }
void Model::setA4(double v) { a4 = juce::jlimit(430.0, 450.0, v); props().setValue("a4", a4); changed(); }

void Model::setNamCalibrate(bool on) { namCalibrate = on; props().setValue("namCalibrate", on); updateNam(); updateNamB(); }
void Model::setInterfaceLevel(double dbu) {
    interfaceLevel = juce::jlimit(-10.0, 30.0, dbu); props().setValue("interfaceLevel", interfaceLevel);
    if (namCalibrate) { updateNam(); updateNamB(); } else changed();
}
double Model::calibrationTrim(std::optional<double> capture, std::optional<double> interface) {
    if (!capture || !interface) return 0;
    return juce::jlimit(-24.0, 24.0, *interface - *capture);
}

// MARK: - tempo

void Model::setBpm(double v) {
    v = juce::jlimit(30.0, 300.0, v);
    if (std::abs(v - bpm) < 1e-9) return;
    bpm = v; props().setValue("bpm", bpm); applyAll(); changed();
}
void Model::setBeatsPerBar(int n) { beatsPerBar = juce::jlimit(1, 12, n); props().setValue("beatsPerBar", beatsPerBar); applyAll(); changed(); }
void Model::setMetroVol(double v) { metroVol = juce::jlimit(-40.0, 0.0, v); props().setValue("metroVol", metroVol); applyAll(); changed(); }

std::optional<double> Model::syncedDelayMs() const {
    const int k = settings.i("delaySync");
    if (k <= 0 || k >= (int) DELAY_DIVS.size()) return std::nullopt;
    return juce::jlimit(40.0, 2000.0, DELAY_DIVS[(size_t) k].beats * 60000 / bpm);
}

void Model::tap() {
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    if (!taps.empty() && now - taps.back() > 2) taps.clear();
    taps.push_back(now);
    if (taps.size() > 5) taps.erase(taps.begin());
    if (taps.size() < 2) return;
    const double beat = (now - taps.front()) / (double) (taps.size() - 1);
    setBpm(std::round(60 / beat * 10) / 10);
    if (settings.i("delaySync") == 0) set("delayTime", juce::jlimit(40.0, 1500.0, std::round(beat * 1000)));
}

void Model::midiAction(const juce::String& key) {
    if (key == "@next") stepPreset(1);
    else if (key == "@prev") stepPreset(-1);
    else if (key == "@tuner") setTuner(!tunerOn);
    else if (key == "@tap") tap();
    else if (key == "@metro") setMetro(!metroOn);
}

// MARK: - cab, captures

const Capture* Model::activeCapture() const {
    const auto n = settings.s("capture");
    if (n.isEmpty()) return nullptr;
    auto it = captures.find(n);
    return it == captures.end() ? nullptr : &it->second;
}

void Model::useCapture(const juce::String& name) {
    Settings n = settings;
    n.set("capture", name);
    if (name.isNotEmpty()) n.set("nam", juce::String());
    if (name.isNotEmpty()) {   // make the rest of the rig neutral, so it sounds like the original
        n.set("ampOn", true); n.set("cabOn", true); n.set("eqOn", false); n.set("capDrive", 0.0); n.set("capLevel", 0.0);
        for (int k = 0; k < kPedalCount; ++k) n.set(pedalOnKey((PedalKind) k), false);
    }
    setSettings(n);
}

void Model::reloadCaptures() { captures = Capture::loadAll(); applyAll(); updateIR(); changed(); }

void Model::loadIR() {
    auto chooser = std::make_shared<juce::FileChooser>("Choose a cabinet impulse response (.wav)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser& fc) {
        const auto picked = pickedFiles(fc);
        if (picked.isEmpty()) return;
        const auto f = picked[0];
        auto w = readAudio(f);
        if (w.rate <= 0 || w.channels.empty()) { status = "Could not read IR: " + f.getFileName(); fixIn("Library"); changed(); return; }
        userIR = std::make_pair(w.channels[0], w.rate);
        userIRName = f.getFileNameWithoutExtension();
        if (settings.i("cabModel") == CAB_USER) updateIR(); else set("cabModel", CAB_USER);
        changed();
    });
}

std::vector<float> Model::currentIR(double sr) {
    irLib.scan(sr);
    if (auto* c = activeCapture()) {
        // not normalised: the fitted IR carries the captured output level
        auto h = resample(c->ir, c->sr, sr);
        for (auto& x : h) x *= (float) (c->sr / sr);
        return h;
    }
    const int model = settings.i("cabModel");
    if (model == CAB_LIB) {
        if (auto a = irLib.samples(settings.s("irA"), sr)) {
            auto h = *a;
            const auto bName = settings.s("irB");
            if (settings.d("irBlend") > 0.001 && bName.isNotEmpty()) {
                if (auto b = irLib.samples(bName, sr)) {   // God's Cab IRs are phase-aligned, so a straight sum blends cleanly
                    const float t = (float) settings.d("irBlend");
                    std::vector<float> mix(std::max(h.size(), b->size()), 0.0f);
                    for (size_t i = 0; i < mix.size(); ++i) mix[i] = (i < h.size() ? h[i] : 0) * (1 - t) + (i < b->size() ? (*b)[i] : 0) * t;
                    h = std::move(mix);
                }
            }
            return normaliseIR(h);
        }
    } else if (model == CAB_IRLIB) {
        if (auto h = cabIRs.samples(settings.s("irFile"), sr)) return *h;
    } else if (model == CAB_USER && userIR) {
        return normaliseIR(resample(userIR->first, userIR->second, sr));
    }
    return renderCab(CABS[juce::jlimit(0, kCabCount - 1, model)], settings.d("cabMic"), sr);
}

void Model::updateIR() {
    const auto h = currentIR(dspRate);
    const juce::SpinLock::ScopedLockType sl(dspLock);
    if (dsp) dsp->setIR(h);
}

std::unique_ptr<OfflineRig> Model::offlineRig(double sr) {
    auto r = std::make_unique<OfflineRig>();
    if (auto* c = activeCapture()) c->apply(r->p);
    r->p.capOn = activeCapture() != nullptr;
    settings.apply(r->p);
    if (auto ms = syncedDelayMs()) r->p.delayTime = (float) *ms;
    r->dsp = std::make_unique<DSP>(sr, r->p);
    r->dsp->setIR(currentIR(sr));
    if (settings.s("nam").isNotEmpty()) {   // its own instance, freed with the rig
        auto o = openNam(namFile(settings.s("nam")), sr, namCalibration());
        if (o.m) { r->nam = o.m; r->p.namOn = true; r->dsp->setNam(o.m, o.gain, o.input); }
    }
    if (r->p.ampBOn) {
        auto o = openNam(namFile(settings.s("ampBNam")), sr, namCalibration());
        if (o.m) { r->namB = o.m; r->dsp->setNamB(o.m, o.gain, o.input); }
    }
    if (std::abs(sr - dspRate) > 0.5) irLib.scan(dspRate);
    return r;
}

// MARK: - NAM captures

juce::File Model::namBundledDir() {
#if JUCE_ANDROID
    // No folder ships next to an Android app: the captures are packed in as binary data and unpacked once per version
    static const juce::File dir = [] {
        const auto d = appDataDir().getChildFile("NAM-bundled");
        const auto stamp = d.getChildFile(".version");
        if (stamp.loadFileAsString() != LEVIATHAN_VERSION) {
            d.deleteRecursively(); d.createDirectory();
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
                int size = 0;
                const char* res = BinaryData::namedResourceList[i];
                const char* data = BinaryData::getNamedResource(res, size);
                const juce::String file = BinaryData::getNamedResourceOriginalFilename(res);
                if (data != nullptr && file.endsWithIgnoreCase(".nam")) d.getChildFile(file).replaceWithData(data, (size_t) size);
            }
            stamp.replaceWithText(LEVIATHAN_VERSION);
        }
        return d;
    }();
    return dir;
#elif JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::currentApplicationFile).getChildFile("Contents/Resources/NAM");
#else
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("NAM");
#endif
}

juce::File Model::namFile(const juce::String& name) {
    const auto mine = namDir().getChildFile(name + ".nam");
    return mine.existsAsFile() ? mine : namBundledDir().getChildFile(name + ".nam");
}

void Model::rescanNam() {
    namDir().createDirectory();
    namFiles.clear();
    for (const auto& d : { namDir(), namBundledDir() })
        for (const auto& e : juce::RangedDirectoryIterator(d, false, "*.nam", juce::File::findFiles)) namFiles.addIfNotAlreadyThere(e.getFile().getFileNameWithoutExtension());
    namFiles.sortNatural();
    changed();
}

Model::NamOpen Model::openNam(const juce::File& f, double rate, std::optional<double> calibration) {
    NamOpen o;
    char err[256] = {};
    o.m = lev_nam_load(f.getFullPathName().toRawUTF8(), err, sizeof err);
    if (!o.m) return o;
    lev_nam_reset(o.m, rate, DSP::maxN);
    o.expected = lev_nam_expected_rate(o.m);
    if (lev_nam_has_loudness(o.m)) o.loudness = lev_nam_loudness(o.m);
    if (lev_nam_has_input_level(o.m)) o.inputLevel = lev_nam_input_level(o.m);
    o.trim = calibrationTrim(o.inputLevel, calibration);
    o.input = (float) std::pow(10.0, o.trim / 20);
    // level-matched (measured at the calibrated input, since many files have no loudness metadata), and the
    // right way up (some captures invert, which cancels when two are blended as amp A and B)
    o.gain = (float) std::pow(10.0, (LEV_NAM_TARGET_LEVEL - lev_nam_measure_level_in(o.m, rate, o.input)) / 20) * (float) lev_nam_polarity(o.m, rate);
    return o;
}

/// Hand a model to the audio thread; free the previous one once it can no longer be in use.
void Model::setEngineNam(LevNam* m, float gain, float input) {
    LevNam* old = namModel;
    namModel = m; namGain = gain; namIn = input;
    if (!m) { namInputLevel.reset(); namTrim = 0; namCal.reset(); }
    {
        const juce::SpinLock::ScopedLockType sl(dspLock);
        if (dsp) dsp->setNam(m, gain, input);
    }
    if (old && old != m) juce::Timer::callAfterDelay(1000, [old] { lev_nam_free(old); });
}

/// Load the selected NAM capture into the running engine (at its rate); unloads when none is selected.
void Model::updateNam() {
    const auto name = settings.s("nam");
    const int gen = ++namGen;
    if (name.isEmpty() || dsp == nullptr) {
        if (name.isEmpty()) { setEngineNam(nullptr, 1); namLoaded = {}; }
        applyAll(); changed(); return;
    }
    const auto cal = namCalibration();
    if (name == namLoaded && namRate == dspRate && cal == namCal && namModel) { setEngineNam(namModel, namGain, namIn); applyAll(); changed(); return; }
    const auto file = namFile(name);
    if (!file.existsAsFile()) {
        setEngineNam(nullptr, 1); namLoaded = {};
        status = "NAM capture \"" + name + "\" isn't in the NAM folder."; fixIn("Library"); applyAll(); changed(); return;
    }
    const double rate = dspRate;
    auto aliveFlag = alive;
    juce::Thread::launch([this, aliveFlag, file, name, rate, gen, cal] {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto o = openNam(file, rate, cal);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        juce::MessageManager::callAsync([this, aliveFlag, o, name, rate, gen, cal, ms, file] {
            if (!aliveFlag->load() || gen != namGen) { lev_nam_free(o.m); return; }   // superseded
            if (!o.m) { setEngineNam(nullptr, 1); namLoaded = {}; status = "Couldn't load NAM capture \"" + name + "\"."; fixIn("Library"); applyAll(); changed(); return; }
            debuglog::write("nam", "\"" + name + "\" loaded in " + juce::String(juce::roundToInt(ms)) + " ms (" + juce::String(file.getSize() / 1024) + " KB, made at "
                                   + (o.expected > 0 ? juce::String(juce::roundToInt(o.expected)) + " Hz" : juce::String("unknown rate")) + ", engine " + juce::String(juce::roundToInt(rate)) + " Hz)");
            namLoaded = name; namRate = rate; namExpectedRate = o.expected; namLoudness = o.loudness;
            setEngineNam(o.m, o.gain, o.input);
            namInputLevel = o.inputLevel; namTrim = o.trim; namCal = cal;
            if (o.expected > 0 && std::abs(o.expected - rate) > 1) {
                status = "\"" + name + "\" was captured at " + juce::String(o.expected / 1000, 1) + " kHz but the engine runs at "
                         + juce::String(rate / 1000, 1) + " kHz, so it won't sound quite right.";
                fixIn("Audio");   // the device's sample rate
            }
            applyAll(); changed();
        });
    });
}

void Model::setEngineNamB(LevNam* m, float gain, float input) {
    LevNam* old = namModelB;
    namModelB = m; namGainB = gain; namInB = input;
    if (!m) { namInputLevelB.reset(); namTrimB = 0; namCalB.reset(); }
    {
        const juce::SpinLock::ScopedLockType sl(dspLock);
        if (dsp) dsp->setNamB(m, gain, input);
    }
    if (old && old != m) juce::Timer::callAfterDelay(1000, [old] { lev_nam_free(old); });
}

/// Amp B: same as updateNam, for the second capture.
void Model::updateNamB() {
    const auto name = settings.s("ampBNam");
    const int gen = ++namGenB;
    if (name.isEmpty() || dsp == nullptr) {
        if (name.isEmpty()) { setEngineNamB(nullptr, 1); namLoadedB = {}; }
        changed(); return;
    }
    const auto cal = namCalibration();
    if (name == namLoadedB && namRateB == dspRate && cal == namCalB && namModelB) { setEngineNamB(namModelB, namGainB, namInB); changed(); return; }
    const auto file = namFile(name);
    if (!file.existsAsFile()) { setEngineNamB(nullptr, 1); namLoadedB = {}; status = "Amp B: NAM capture \"" + name + "\" isn't in the NAM folder."; fixIn("Library"); changed(); return; }
    const double rate = dspRate;
    auto aliveFlag = alive;
    juce::Thread::launch([this, aliveFlag, file, name, rate, gen, cal] {
        auto o = openNam(file, rate, cal);
        juce::MessageManager::callAsync([this, aliveFlag, o, name, rate, gen, cal] {
            if (!aliveFlag->load() || gen != namGenB) { lev_nam_free(o.m); return; }   // superseded
            if (!o.m) { setEngineNamB(nullptr, 1); namLoadedB = {}; status = "Amp B: couldn't load NAM capture \"" + name + "\"."; fixIn("Library"); changed(); return; }
            namLoadedB = name; namRateB = rate;
            setEngineNamB(o.m, o.gain, o.input);
            namInputLevelB = o.inputLevel; namTrimB = o.trim; namCalB = cal;
            changed();
        });
    });
}

/// Use a NAM capture in place of the amp (the rest of the rig, cab included, stays as it is).
void Model::useNam(const juce::String& name) {
    Settings n = settings;
    n.set("nam", name); n.set("capture", juce::String());
    if (name.isNotEmpty()) n.set("ampOn", true);
    setSettings(n);
}

void Model::importNamDialog() {
    auto chooser = std::make_shared<juce::FileChooser>("Choose NAM captures (.nam), for example from tone3000.com", juce::File(), "*.nam");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
        [this, chooser](const juce::FileChooser& fc) {
            juce::String first;
            for (const auto& f : pickedFiles(fc)) {
                if (!f.hasFileExtension("nam")) continue;
                if (f.copyFileTo(namDir().getChildFile(f.getFileName())) && first.isEmpty()) first = f.getFileNameWithoutExtension();
            }
            rescanNam();
            if (first.isNotEmpty()) useNam(first);
        });
}

void Model::importCabIRsDialog() {
    auto chooser = std::make_shared<juce::FileChooser>("Choose cab IRs (WAV) or a folder of them, for example a tone pack from tone3000.com", juce::File(), "*.wav;*.aif;*.aiff");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories
                             | juce::FileBrowserComponent::canSelectMultipleItems,
        [this, chooser](const juce::FileChooser& fc) {
            const auto items = pickedFiles(fc);
            if (items.isEmpty()) return;
            const int n = cabIRs.importItems(items);
            status = n == 0 ? juce::String("No WAV impulse responses found there.")
                            : "Added " + juce::String(n) + " cab IR" + (n == 1 ? "" : "s") + " to the IR library.";
            if (n > 0 && (settings.i("cabModel") != CAB_IRLIB || !cabIRs.contains(settings.s("irFile")))) {
                Settings x = settings; x.set("cabModel", CAB_IRLIB); x.set("irFile", cabIRs.all()[0]); x.set("cabOn", true); setSettings(x);
            }
            changed();
        });
}

// MARK: - appearance, layout

juce::Colour Model::accent() const {
    if (accentPick) return *accentPick;
    return channelColours[juce::jlimit(0, 2, settings.i("ampChannel"))];
}

void Model::setAccent(std::optional<juce::Colour> c) { accentPick = c; saveState(); changed(); }

void Model::setZoom(double z) {
    zoom = juce::jlimit(zoomSteps.front(), zoomSteps.back(), z);
    juce::Desktop::getInstance().setGlobalScaleFactor((float) zoom);
    saveState(); changed();
}

void Model::zoomStep(int d) {
    int i = 4;
    for (int k = 0; k < (int) zoomSteps.size(); ++k) if (zoomSteps[(size_t) k] >= zoom - 0.001) { i = k; break; }
    setZoom(zoomSteps[(size_t) juce::jlimit(0, (int) zoomSteps.size() - 1, i + d)]);
}

void Model::toggleCollapsed(const juce::String& key) {
    if (collapsed.contains(key)) collapsed.removeString(key); else collapsed.add(key);
    saveState(); changed();
}

void Model::setPanelOrder(const juce::StringArray& o) { panelOrder = o; saveState(); changed(); }

// MARK: - explanations on hover

juce::String Model::explain(const juce::String& key) {
    static const juce::var all = juce::JSON::parse(juce::String::fromUTF8(BinaryData::explanations_json, BinaryData::explanations_jsonSize));
    return all.getProperty(juce::Identifier(key.isEmpty() ? juce::String("_") : key), {}).toString();
}

void Model::showHover(const juce::String& id, const juce::String& title, const juce::String& text) {
    if (!explainOnHover || text.isEmpty()) return;
    hoverId = id; hoverTitle = title; hoverText = text;
}

void Model::clearHover(const juce::String& id) {
    if (id.isNotEmpty() && id != hoverId) return;
    hoverId.clear(); hoverTitle.clear(); hoverText.clear();
}

void Model::setExplainOnHover(bool on) { explainOnHover = on; clearHover(); saveState(); changed(); }

// MARK: - output device (Master panel)

juce::StringArray Model::outputDeviceNames() {
    auto* t = devices.getCurrentDeviceTypeObject();
    return t ? t->getDeviceNames(false) : juce::StringArray();
}

juce::String Model::outputDeviceName() const { return devices.getAudioDeviceSetup().outputDeviceName; }

void Model::setOutputDevice(const juce::String& name) {
    auto setup = devices.getAudioDeviceSetup();
    if (setup.outputDeviceName == name) return;
    setup.outputDeviceName = name;
    if (auto* t = devices.getCurrentDeviceTypeObject(); t && !t->hasSeparateInputsAndOutputs()) setup.inputDeviceName = name;   // ASIO: one device
    setup.useDefaultOutputChannels = true;
    const auto err = devices.setAudioDeviceSetup(setup, true);
    if (err.isNotEmpty()) { status = "Couldn't open " + name + ": " + err; fixIn("Audio"); }
    restartAudio();
    changed();
}

// MARK: - input device (Input panel)

juce::StringArray Model::inputDeviceNames() {
    auto* t = devices.getCurrentDeviceTypeObject();
    return t ? t->getDeviceNames(true) : juce::StringArray();
}

juce::String Model::inputDeviceName() const { return devices.getAudioDeviceSetup().inputDeviceName; }

int Model::inputChannelCount() const {
    if (auto* d = devices.getCurrentAudioDevice()) return d->getInputChannelNames().size();
    return 0;
}

void Model::setInputDevice(const juce::String& name) {
    auto setup = devices.getAudioDeviceSetup();
    if (setup.inputDeviceName == name) return;
    setup.inputDeviceName = name;
    if (auto* t = devices.getCurrentDeviceTypeObject(); t && !t->hasSeparateInputsAndOutputs()) setup.outputDeviceName = name;   // ASIO: one device
    setup.useDefaultInputChannels = true;
    const auto err = devices.setAudioDeviceSetup(setup, true);
    if (err.isNotEmpty()) { status = "Couldn't open " + name + ": " + err; fixIn("Audio"); }
    restartAudio();
    changed();
}

} // namespace lev
