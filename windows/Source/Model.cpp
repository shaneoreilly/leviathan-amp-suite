#include "Model.h"
#include <BinaryData.h>
#include "Sessions.h"
#include "Midi.h"
#include "Updater.h"

namespace lev {

const std::vector<double> Model::zoomSteps = { 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.25, 1.4, 1.6 };
/// "|" starts a new row.
static const juce::StringArray defaultPanels { "input", "master", "gate", "pitch", "eq", "|", "amp", "cab", "reverb", "delay" };
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
            presetMeta[kv.name.toString()] = { (juce::int64) kv.value.getProperty("created", 0), (juce::int64) kv.value.getProperty("modified", 0),
                                               (juce::int64) kv.value.getProperty("played", 0), (juce::int64) kv.value.getProperty("imported", 0) };
    for (const auto& [n, s] : userPresets)   // presets saved before dates were kept: date them from now
        if (presetMeta[n].shownModified() == 0) presetMeta[n].imported = juce::Time::currentTimeMillis();
    presetSort = juce::jlimit(0, 4, p.getIntValue("presetSort", 0));
    presetSortReversed = p.getBoolValue("presetSortReversed", false);
    tuningIndex = p.getIntValue("tuning", 1); a4 = p.getDoubleValue("a4", 440);
    bpm = p.getDoubleValue("bpm", 120); beatsPerBar = p.getIntValue("beatsPerBar", 4); metroVol = p.getDoubleValue("metroVol", -12);
    if (p.containsKey("accent")) accentPick = juce::Colour::fromString(p.getValue("accent"));
    zoom = p.getDoubleValue("zoom", 1.0);
    explainOnHover = p.getBoolValue("explainOnHover", true);
    {
        auto saved = juce::StringArray::fromTokens(p.getValue("panelOrder2"), ",", "");
        for (const auto& id : saved) if (defaultPanels.contains(id)) panelOrder.addIfNotAlreadyThere(id);
        for (const auto& id : defaultPanels) panelOrder.addIfNotAlreadyThere(id);
    }
    collapsed = juce::StringArray::fromLines(p.getValue("collapsed")); collapsed.removeEmptyStrings();

    captures = Capture::loadAll();
    rescanNam();
    irLib.scan(48000);
    cabIRs.scan();
    if (!irLib.isEmpty() && !p.getBoolValue("godsCabIntro")) {           // first run with the IR pack: switch to it
        settings.set("cabModel", CAB_LIB); settings.set("cabHighCut", 10000.0); p.setValue("godsCabIntro", true);
    }

    sessions = std::make_unique<Sessions>(*this);
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
        devices.setAudioDeviceSetup(setup, true);
    }
    ensureInputs();
    syncMute();
    midi->start();
    updater->checkOnLaunch();
    // LEVIATHAN_SCREEN=presets|recordings|settings: open on that screen (UI testing)
    const auto scr = juce::SystemStats::getEnvironmentVariable("LEVIATHAN_SCREEN", {});
    if (scr == "presets") screen = Screen::presets; else if (scr == "recordings") screen = Screen::recordings; else if (scr == "settings") screen = Screen::settings;
    start();
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

void Model::settingsChanged(const Settings& old) {
    applyAll();
    if (settings.s("nam") != old.s("nam")) updateNam();
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
            o->setProperty(k, juce::var(e));
        }
        p.setValue("presetMeta", juce::JSON::toString(juce::var(o), juce::JSON::FormatOptions{}.withSpacing(juce::JSON::Spacing::none)));
        p.setValue("presetSort", presetSort);
        p.setValue("presetSortReversed", presetSortReversed);
    }
    if (auto xml = devices.createStateXml()) p.setValue("audioState", xml->toString());
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

void Model::start() {
    if (running) return;
    if (devices.getCurrentAudioDevice() == nullptr) {
        const auto err = devices.initialise(8, 2, nullptr, true);
        if (devices.getCurrentAudioDevice() == nullptr) {
            status = err.isNotEmpty() ? err : juce::String("Pick an input and output device in Settings.");
            changed(); return;
        }
        ensureInputs();
    }
    devices.addAudioCallback(this);
    running = true;
    adviseOverloads = true; recentOverloads.clear(); startedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    lastXRuns = devices.getXRunCount();
    refreshInfo();
    changed();
}

void Model::stop() {
    if (!running) return;
    devices.removeAudioCallback(this);
    running = false;
    params.inPeak.store(0); params.outPeak.store(0);
    changed();
}

void Model::restartAudio() {
    ensureInputs();
    if (running) { stop(); start(); }
    saveState();
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
    juce::MessageManager::callAsync([this] { irLib.scan(dspRate); updateIR(); updateNam(); refreshInfo(); changed(); });
}

void Model::audioDeviceStopped() {
    const juce::SpinLock::ScopedLockType sl(dspLock);
    dsp.reset();
}

void Model::audioDeviceError(const juce::String& msg) {
    juce::MessageManager::callAsync([this, msg] { status = "Audio device error: " + msg; changed(); });
}

void Model::audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int n,
                                             const juce::AudioIODeviceCallbackContext&) {
    DSP* d = dsp.get();
    if (d == nullptr || numOut == 0) { for (int c = 0; c < numOut; ++c) if (out[c]) juce::FloatVectorOperations::clear(out[c], n); return; }
    const int ch = juce::jlimit(0, std::max(numIn - 1, 0), params.inputChannel);
    const float* src = numIn > 0 && in[ch] ? in[ch] : zeros.data();
    for (int off = 0; off < n; off += DSP::maxN) {
        const int k = std::min(DSP::maxN, n - off);
        float* l = out[0] ? out[0] + off : scratchL.data();
        float* r = numOut > 1 && out[1] ? out[1] + off : scratchR.data();
        d->process(src + (src == zeros.data() ? 0 : off), l, r, k);
    }
    for (int c = 2; c < numOut; ++c) if (out[c]) juce::FloatVectorOperations::clear(out[c], n);
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
    if (info.type.isNotEmpty() && info.type != "ASIO")
        why << " An ASIO driver for your interface (or \"Windows Audio (Exclusive Mode)\") is usually much more stable at low latency.";
    if (info.inName.isNotEmpty() && info.outName.isNotEmpty() && info.inName != info.outName)
        why << " Input (" << info.inName << ") and output (" << info.outName << ") are separate devices with separate clocks; one interface for both is most stable.";
    // keep dropouts from going on: step the buffer up to the next size the driver offers, up to 1024 samples
    if (auto* d = devices.getCurrentAudioDevice()) {
        const int cur = d->getCurrentBufferSizeSamples();
        int bigger = 0;
        for (int b : d->getAvailableBufferSizes()) if (b > cur && b <= 1024 && (bigger == 0 || b < bigger)) bigger = b;
        if (bigger > 0) {
            auto setup = devices.getAudioDeviceSetup();
            setup.bufferSize = bigger;
            juce::MessageManager::callAsync([this, setup] { devices.setAudioDeviceSetup(setup, true); startedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0; refreshInfo(); saveState(); changed(); });
            status = "Audio was dropping out (crackles), so the buffer is now " + juce::String(bigger) + " samples (Settings > Audio)." + why;
            changed();
            return;
        }
    }
    if (adviseOverloads) { adviseOverloads = false; status = "Audio is still dropping out at the largest buffer." + why; changed(); }
}

void Model::timerCallback() {
    if (saveCountdown > 0 && --saveCountdown == 0) { saveState(); saveCountdown = -1; }
    sessions->tick();
    if (onTick) onTick();
    if (!running) return;
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
    presetName = name;
    presetMeta[name].played = juce::Time::currentTimeMillis();
    props().setValue("preset", name);
    setSettings(n);
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
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (!f.hasFileExtension(presetExt)) f = f.withFileExtension(presetExt);
            if (!f.replaceWithText(presetFileData(name))) { status = "Couldn't export \"" + name + "\"."; changed(); }
        });
}

void Model::exportAllPresets() {
    auto chooser = std::make_shared<juce::FileChooser>("Choose a folder for the preset files",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc) {
            const auto dir = fc.getResult();
            if (dir == juce::File()) return;
            juce::StringArray failed;
            const auto order = presetOrder();
            for (const auto& n : order) if (!dir.getChildFile(fileNameFor(n)).replaceWithText(presetFileData(n))) failed.add(n);
            status = failed.isEmpty() ? "Exported " + juce::String(order.size()) + " presets to " + dir.getFileName() + "."
                                      : "Couldn't export: " + failed.joinIntoString(", ");
            if (failed.isEmpty()) dir.revealToUser();
            changed();
        });
}

void Model::importPresetsDialog() {
    auto chooser = std::make_shared<juce::FileChooser>("Choose .leviathan preset files",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.leviathan;*.json");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
        [this, chooser](const juce::FileChooser& fc) { if (!fc.getResults().isEmpty()) importPresets(fc.getResults()); });
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
    n.set("pitchSemis", (double) semis); n.set("pitchCents", 0.0); n.set("pitchOn", semis != 0);
    setSettings(n);
}

void Model::setTuningIndex(int i) { tuningIndex = juce::jlimit(0, (int) TUNINGS.size() - 1, i); props().setValue("tuning", tuningIndex); changed(); }
void Model::setA4(double v) { a4 = juce::jlimit(430.0, 450.0, v); props().setValue("a4", a4); changed(); }

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
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        auto w = readAudio(f);
        if (w.rate <= 0 || w.channels.empty()) { status = "Could not read IR: " + f.getFileName(); changed(); return; }
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
        auto o = openNam(namFile(settings.s("nam")), sr);
        if (o.m) { r->nam = o.m; r->p.namOn = true; r->dsp->setNam(o.m, o.gain); }
    }
    if (std::abs(sr - dspRate) > 0.5) irLib.scan(dspRate);
    return r;
}

// MARK: - NAM captures

juce::File Model::namBundledDir() {
#if JUCE_MAC
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

Model::NamOpen Model::openNam(const juce::File& f, double rate) {
    NamOpen o;
    char err[256] = {};
    o.m = lev_nam_load(f.getFullPathName().toRawUTF8(), err, sizeof err);
    if (!o.m) return o;
    lev_nam_reset(o.m, rate, DSP::maxN);
    o.expected = lev_nam_expected_rate(o.m);
    if (lev_nam_has_loudness(o.m)) o.loudness = lev_nam_loudness(o.m);
    // level-match every capture by measuring it (many files have no loudness metadata)
    o.gain = (float) std::pow(10.0, (LEV_NAM_TARGET_LEVEL - lev_nam_measure_level(o.m, rate)) / 20);
    return o;
}

/// Hand a model to the audio thread; free the previous one once it can no longer be in use.
void Model::setEngineNam(LevNam* m, float gain) {
    LevNam* old = namModel;
    namModel = m; namGain = gain;
    {
        const juce::SpinLock::ScopedLockType sl(dspLock);
        if (dsp) dsp->setNam(m, gain);
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
    if (name == namLoaded && namRate == dspRate && namModel) { setEngineNam(namModel, namGain); applyAll(); changed(); return; }
    const auto file = namFile(name);
    if (!file.existsAsFile()) {
        setEngineNam(nullptr, 1); namLoaded = {};
        status = "NAM capture \"" + name + "\" isn't in the NAM folder."; applyAll(); changed(); return;
    }
    const double rate = dspRate;
    auto aliveFlag = alive;
    juce::Thread::launch([this, aliveFlag, file, name, rate, gen] {
        auto o = openNam(file, rate);
        juce::MessageManager::callAsync([this, aliveFlag, o, name, rate, gen] {
            if (!aliveFlag->load() || gen != namGen) { lev_nam_free(o.m); return; }   // superseded
            if (!o.m) { setEngineNam(nullptr, 1); namLoaded = {}; status = "Couldn't load NAM capture \"" + name + "\"."; applyAll(); changed(); return; }
            namLoaded = name; namRate = rate; namExpectedRate = o.expected; namLoudness = o.loudness;
            setEngineNam(o.m, o.gain);
            if (o.expected > 0 && std::abs(o.expected - rate) > 1)
                status = "\"" + name + "\" was captured at " + juce::String(o.expected / 1000, 1) + " kHz but the engine runs at "
                         + juce::String(rate / 1000, 1) + " kHz, so it won't sound quite right.";
            applyAll(); changed();
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
            for (const auto& f : fc.getResults()) {
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
            const auto items = fc.getResults();
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
    if (err.isNotEmpty()) status = "Couldn't open " + name + ": " + err;
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
    if (err.isNotEmpty()) status = "Couldn't open " + name + ": " + err;
    restartAudio();
    changed();
}

} // namespace lev
