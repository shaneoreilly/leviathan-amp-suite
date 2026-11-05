#include "Sessions.h"
#include "Model.h"

namespace lev {

juce::File Sessions::root() { return musicDir().getChildFile("Sessions"); }

Sessions::Sessions(Model& model) : m(model) {
    for (auto& t : tmp) t.resize(1 << 18);
    root().createDirectory();
    refresh();
}

Sessions::~Sessions() {
    alive->store(false);
    m.params.recording.store(false);
    m.params.bkPlaying.store(false);
    m.params.srcPlaying.store(false);
}

const Session* Sessions::current() const {
    for (const auto& s : list) if (s.dir == selected) return &s;
    return nullptr;
}

void Sessions::select(const juce::File& dir) {
    if (dir == selected) return;
    stopPlayback(); stopLiveReamp();
    selected = dir;
    loadWaveform();
    m.changed();
}

void Sessions::refresh() {
    list.clear();
    for (const auto& e : juce::RangedDirectoryIterator(root(), false, "*", juce::File::findDirectories)) {
        const auto d = e.getFile();
        const auto v = juce::JSON::parse(d.getChildFile("session.json").loadFileAsString());
        if (!v.isObject()) continue;
        Session s; s.dir = d;
        s.name = v["name"].toString(); s.preset = v["preset"].toString(); s.source = v["source"].toString();
        s.date = juce::Time::fromISO8601(v["date"].toString());
        s.duration = (double) v["duration"];
        if (v["settings"].isObject()) s.settings = Settings::fromJSON(v["settings"]);
        list.push_back(std::move(s));
    }
    std::sort(list.begin(), list.end(), [](const Session& a, const Session& b) { return a.date > b.date; });
    if (selected != juce::File() && current() == nullptr) selected = juce::File();
    m.changed();
}

void Sessions::save(const Session& s) {
    auto* o = new juce::DynamicObject();
    o->setProperty("name", s.name);
    // same ISO 8601 form Swift's JSONEncoder writes, so the Mac app reads these sessions too
    const juce::Time utc(s.date.toMilliseconds() - (juce::int64) s.date.getUTCOffsetSeconds() * 1000);
    o->setProperty("date", utc.formatted("%Y-%m-%dT%H:%M:%SZ"));
    o->setProperty("duration", s.duration);
    o->setProperty("preset", s.preset);
    if (s.settings) o->setProperty("settings", s.settings->toVar());
    if (s.source.isNotEmpty()) o->setProperty("source", s.source);
    s.dir.getChildFile("session.json").replaceWithText(juce::JSON::toString(juce::var(o)));
}

std::unique_ptr<juce::AudioFormatWriter> Sessions::openWriter(const juce::File& f, double rate, int channels) {
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = f.createOutputStream();
    if (!stream) return nullptr;
    juce::WavAudioFormat wav;   // 32-bit float, like the Mac app's recordings
    return wav.createWriterFor(stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(32)
                                         .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
}

// MARK: - recording

void Sessions::startRecording() {
    if (recording) return;
    if (!m.running || m.engineRate() <= 0) { message = "Turn the amp on to record."; m.changed(); return; }
    stopPlayback();
    const double sr = m.engineRate();
    const auto dir = root().getChildFile(juce::Time::getCurrentTime().formatted("%Y-%m-%d %H.%M.%S"));
    dir.createDirectory();
    dryWriter = openWriter(dir.getChildFile("dry.wav"), sr, 1);
    wetWriter = openWriter(dir.getChildFile("wet.wav"), sr, 2);
    if (!dryWriter || !wetWriter) { dryWriter.reset(); wetWriter.reset(); message = "Could not start recording in " + dir.getFullPathName(); m.changed(); return; }
    recDir = dir; recRate = sr; recStart = juce::Time::getCurrentTime(); recSeconds = 0; recFrames = 0;
    m.params.recDry.reset(); m.params.recL.reset(); m.params.recR.reset();
    m.params.recording.store(true);
    recording = true; message = {};
    m.changed();
}

void Sessions::stopRecording() {
    if (!recording) return;
    m.params.recording.store(false);
    drain();
    const auto frames = (double) recFrames;
    dryWriter.reset(); wetWriter.reset();
    recording = false;
    int n = 0;
    for (const auto& s : list) if (s.name.startsWith("Session ")) n = std::max(n, s.name.fromFirstOccurrenceOf("Session ", false, false).getIntValue());
    Session s; s.dir = recDir; s.name = "Session " + juce::String(n + 1).paddedLeft('0', 2); s.date = recStart;
    s.duration = frames / recRate; s.preset = m.presetName; s.settings = m.s();
    save(s);
    refresh();
    select(s.dir);
}

void Sessions::drain() {
    auto& p = m.params;
    for (;;) {
        const int n = std::min({ p.recDry.available(), p.recL.available(), p.recR.available(), 1 << 18 });
        if (n <= 0) break;
        p.recDry.read(tmp[0].data(), n); p.recL.read(tmp[1].data(), n); p.recR.read(tmp[2].data(), n);
        const float* d[] = { tmp[0].data() }; const float* w[] = { tmp[1].data(), tmp[2].data() };
        if (dryWriter) dryWriter->writeFromFloatArrays(d, 1, n);
        recFrames += n;
        if (wetWriter) wetWriter->writeFromFloatArrays(w, 2, n);
    }
}

void Sessions::tick() {
    if (recording) { drain(); recSeconds = (juce::Time::getCurrentTime() - recStart).inSeconds(); }
    auto& p = m.params;
    if (playing) {
        playPos = p.bkPos.load() / playRate;
        if (!p.bkPlaying.load()) { playing = false; playPos = 0; m.changed(); }
    }
    if (p.srcDone.exchange(false)) { liveReamp = false; m.changed(); }
    if (retireCountdown > 0 && --retireCountdown == 0) retired.clear();
}

/// Keep a buffer alive while the audio thread may read it; swapped-out ones are released a second later.
const float* Sessions::hand(std::vector<float> x) {
    live.push_back(std::move(x));
    return live.back().data();
}

// MARK: - playback (through the engine's backing slot, so it plays on your chosen output)

void Sessions::play(double from) {
    auto* s = current();
    if (!s) return;
    if (!m.running || m.engineRate() <= 0) { message = "Turn the amp on to play recordings."; m.changed(); return; }
    stopLiveReamp();
    const double sr = m.engineRate();
    auto w = loadAudio(s->wet(), sr);
    if (w.channels.empty()) { message = "Could not read the recording."; m.changed(); return; }
    auto& p = m.params;
    p.bkPlaying.store(false);
    for (auto& b : live) retired.push_back(std::move(b));
    live.clear(); retireCountdown = 30;
    const int len = (int) w.channels[0].size();
    const float* l = hand(w.channels[0]);
    const float* r = w.channels.size() > 1 ? hand(w.channels[1]) : l;
    p.bkL.store(l); p.bkR.store(r); p.bkLen.store(len); p.bkLoop.store(false); p.bkVol = 0;
    p.bkPos.store(std::min((int) (from * sr), std::max(len - 1, 0)));
    p.bkPlaying.store(true, std::memory_order_release);
    playLen = len; playRate = sr; playing = true;
    m.changed();
}

void Sessions::seek(double t) {
    if (playing) { m.params.bkPos.store(juce::jlimit(0, std::max(playLen - 1, 0), (int) (t * playRate))); playPos = t; }
    else play(t);
}

void Sessions::stopPlayback() {
    m.params.bkPlaying.store(false);
    if (playing) { playing = false; m.changed(); }
}

// MARK: - live re-amp

void Sessions::startLiveReamp() {
    auto* s = current();
    if (!s) return;
    if (!m.running || m.engineRate() <= 0) { message = "Turn the amp on to re-amp."; m.changed(); return; }
    stopPlayback();
    auto w = loadAudio(s->dry(), m.engineRate());
    if (w.channels.empty()) { message = "This session has no dry take."; m.changed(); return; }
    auto& p = m.params;
    p.srcPlaying.store(false);
    for (auto& b : live) retired.push_back(std::move(b));
    live.clear(); retireCountdown = 30;
    const int len = (int) w.channels[0].size();
    p.srcPtr.store(hand(w.channels[0])); p.srcLen.store(len); p.srcPos.store(0); p.srcLoop.store(true); p.srcDone.store(false);
    p.srcPlaying.store(true, std::memory_order_release);
    liveReamp = true;
    m.changed();
}

void Sessions::stopLiveReamp() {
    m.params.srcPlaying.store(false);
    if (liveReamp) { liveReamp = false; m.changed(); }
}

// MARK: - offline re-amp

void Sessions::reamp() {
    auto* s = current();
    if (!s || rendering) return;
    const double sr = m.engineRate() > 0 ? m.engineRate() : 48000;
    auto w = loadAudio(s->dry(), sr);
    if (w.channels.empty()) { message = "This session has no dry take."; m.changed(); return; }
    stopPlayback(); stopLiveReamp();
    std::shared_ptr<OfflineRig> rig = m.offlineRig(sr);
    const Settings settings = m.s(); const juce::String preset = m.presetName;
    const juce::File wet = s->wet(), backup = s->dir.getChildFile("wet-original.wav"), dir = s->dir;
    rendering = 0; m.changed();
    auto x = std::make_shared<std::vector<float>>(std::move(w.channels[0]));
    auto aliveFlag = alive;
    juce::Thread::launch([this, aliveFlag, rig, x, sr, wet, backup, dir, settings, preset] {
        const int tail = (int) (sr * 2), total = (int) x->size() + tail, block = 512;   // let delay/reverb ring out
        x->resize((size_t) total, 0.0f);
        std::vector<float> l((size_t) total), r((size_t) total);
        for (int i = 0; i < total; i += block) {
            const int n = std::min(block, total - i);
            rig->dsp->process(x->data() + i, l.data() + i, r.data() + i, n);
            if ((i / block) % 400 == 0) {
                const double prog = (double) i / total;
                juce::MessageManager::callAsync([this, aliveFlag, prog] { if (aliveFlag->load()) { rendering = prog; m.changed(); } });
            }
        }
        bool ok = true;
        if (!backup.existsAsFile() && wet.existsAsFile()) ok = wet.moveFileTo(backup);
        if (ok) ok = writeWav(wet, { l.data(), r.data() }, total, sr);
        juce::MessageManager::callAsync([this, aliveFlag, ok, dir, settings, preset] {
            if (!aliveFlag->load()) return;
            rendering.reset();
            if (!ok) { message = "Re-amp failed: couldn't write the new take."; m.changed(); return; }
            for (auto& s2 : list) if (s2.dir == dir) { s2.preset = preset; s2.settings = settings; save(s2); }
            refresh(); loadWaveform();
            message = "Re-amped with \"" + preset + "\". The original wet take is kept as wet-original.wav.";
            m.changed();
        });
    });
}

// MARK: - manage

void Sessions::loadPreset() {
    auto* s = current();
    if (!s || !s->settings) return;
    m.presetName = s->preset;
    m.setSettings(*s->settings);
}

void Sessions::rename(const juce::String& name) {
    if (name.trim().isEmpty()) return;
    for (auto& s : list) if (s.dir == selected) { s.name = name.trim(); save(s); }
    refresh();
}

void Sessions::remove() {
    auto* s = current();
    if (!s) return;
    stopPlayback(); stopLiveReamp();
    s->dir.moveToTrash();
    selected = juce::File();
    refresh();
}

void Sessions::reveal() { if (auto* s = current()) s->wet().revealToUser(); }

void Sessions::importRecordings() {
    auto chooser = std::make_shared<juce::FileChooser>("Choose WAV recordings or folders of them", juce::File(), "*.wav;*.wave;*.aif;*.aiff;*.flac");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                         [this, chooser](const juce::FileChooser& fc) { importFiles(fc.getResults()); });
}

/// Files named as a pair (`Dry_<name>` + `Wet_<name>`, or `<name> dry` + `<name> wet`) become one take; any other
/// file becomes a take of its own, treated as a dry DI recording so it can be re-amped through the rig.
void Sessions::importFiles(const juce::Array<juce::File>& picked) {
    if (picked.isEmpty()) return;
    const juce::StringArray exts { "wav", "wave", "aif", "aiff", "flac" };
    juce::Array<juce::File> files;
    for (const auto& f : picked) {
        if (f.isDirectory()) { for (const auto& e : juce::RangedDirectoryIterator(f, false, "*", juce::File::findFiles)) files.add(e.getFile()); }
        else files.add(f);
    }
    auto split = [](const juce::File& f, juce::String& name, juce::String& tag) {
        const auto n = f.getFileNameWithoutExtension(), low = n.toLowerCase();
        for (const char* t : { "dry", "wet" }) {
            for (const auto& pre : { juce::String(t) + "_", juce::String(t) + " ", juce::String(t) + "-" })
                if (low.startsWith(pre)) { name = n.substring(pre.length()); tag = t; return true; }
            for (const auto& suf : { " " + juce::String(t), "_" + juce::String(t), "-" + juce::String(t) })
                if (low.endsWith(suf)) { name = n.dropLastCharacters(suf.length()); tag = t; return true; }
        }
        return false;
    };
    std::map<juce::String, juce::File> dries, wets; std::vector<std::pair<juce::String, juce::File>> singles;
    for (const auto& f : files) {
        if (!exts.contains(f.getFileExtension().trimCharactersAtStart(".").toLowerCase())) continue;
        juce::String n, t;
        if (split(f, n, t)) (t == "dry" ? dries : wets)[n] = f; else singles.emplace_back(f.getFileNameWithoutExtension(), f);
    }
    for (auto it = dries.begin(); it != dries.end();) { if (!wets.count(it->first)) { singles.emplace_back(it->first, it->second); it = dries.erase(it); } else ++it; }
    for (auto it = wets.begin(); it != wets.end();) { if (!dries.count(it->first)) { singles.emplace_back(it->first, it->second); it = wets.erase(it); } else ++it; }

    juce::StringArray have; for (const auto& s : list) have.add(s.name);
    int added = 0;
    auto add = [&](const juce::String& name, const juce::File& dry, const juce::File& wet) {
        if (have.contains(name)) return;
        const auto date = wet.getLastModificationTime();
        const auto dir = root().getChildFile(date.formatted("%Y-%m-%d %H.%M.%S") + " " + juce::File::createLegalFileName(name));
        if (!dir.createDirectory()) return;
        // stored as WAV whatever came in, so every take in the library is the same format
        auto copy = [](const juce::File& src, const juce::File& dst) {
            if (src.hasFileExtension("wav;wave")) return src.copyFileTo(dst);
            auto w = readAudio(src, (int64_t) 48000 * 60 * 60);
            if (w.channels.empty()) return false;
            std::vector<const float*> ch; for (auto& c : w.channels) ch.push_back(c.data());
            return writeWav(dst, ch, (int) w.channels[0].size(), w.rate);
        };
        if (!copy(dry, dir.getChildFile("dry.wav")) || !copy(wet, dir.getChildFile("wet.wav"))) { dir.deleteRecursively(); return; }
        Session s; s.dir = dir; s.name = name; s.date = date; s.source = "import";
        if (std::unique_ptr<juce::AudioFormatReader> r(juce::WavAudioFormat().createReaderFor(dir.getChildFile("wet.wav").createInputStream().release(), true)); r)
            s.duration = r->lengthInSamples / r->sampleRate;
        save(s);
        added++;
    };
    for (const auto& [n, d] : dries) add(n, d, wets[n]);
    std::sort(singles.begin(), singles.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (const auto& [n, f] : singles) add(n, f, f);
    refresh();
    message = added == 0 ? juce::String("Nothing new to import.") : "Imported " + juce::String(added) + " recording" + (added == 1 ? "" : "s") + ".";
    m.setScreen(Screen::recordings);
}

// MARK: - waveform

void Sessions::loadWaveform() {
    waveform.clear();
    auto* s = current();
    if (!s) return;
    const auto file = s->wet().existsAsFile() ? s->wet() : s->dry();
    const int gen = ++waveGen;
    auto aliveFlag = alive;
    juce::Thread::launch([this, aliveFlag, file, gen] {
        auto w = loadAudio(file, 0);
        if (w.channels.empty()) return;
        const auto& x = w.channels[0];
        const size_t buckets = 1500, per = std::max<size_t>(x.size() / buckets, 1);
        std::vector<float> out; out.reserve(buckets * 2);
        for (size_t i = 0; i < x.size(); i += per) {
            float lo = 0, hi = 0;
            for (size_t k = i; k < std::min(i + per, x.size()); ++k) { lo = std::min(lo, x[k]); hi = std::max(hi, x[k]); }
            out.push_back(lo); out.push_back(hi);
        }
        juce::MessageManager::callAsync([this, aliveFlag, gen, out = std::move(out)]() mutable {
            if (aliveFlag->load() && gen == waveGen.load()) { waveform = std::move(out); m.changed(); }
        });
    });
}

} // namespace lev
