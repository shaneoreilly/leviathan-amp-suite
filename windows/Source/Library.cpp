#include "Library.h"

namespace lev {

juce::File appDataDir() {
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support/Leviathan");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Leviathan");
#endif
}

juce::File musicDir() {
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Leviathan");
}

static juce::AudioFormatManager& formats() {
    static juce::AudioFormatManager* fm = [] { auto* m = new juce::AudioFormatManager(); m->registerBasicFormats(); return m; }();
    return *fm;
}

WavData readAudio(const juce::File& f, int64_t maxFrames) {
    WavData out;
    std::unique_ptr<juce::AudioFormatReader> r(formats().createReaderFor(f));
    if (!r || r->numChannels < 1 || r->sampleRate <= 0) return out;
    const int n = (int) std::min<int64_t>(r->lengthInSamples, maxFrames);
    juce::AudioBuffer<float> buf((int) r->numChannels, std::max(n, 1));
    buf.clear();
    if (n > 0) r->read(&buf, 0, n, 0, true, true);
    for (int c = 0; c < buf.getNumChannels(); ++c) out.channels.emplace_back(buf.getReadPointer(c), buf.getReadPointer(c) + n);
    out.rate = r->sampleRate;
    return out;
}

WavData loadAudio(const juce::File& f, double rate, double maxSeconds) {
    std::unique_ptr<juce::AudioFormatReader> r(formats().createReaderFor(f));
    if (!r || r->sampleRate <= 0) return {};
    auto w = readAudio(f, (int64_t) (r->sampleRate * maxSeconds));
    if (rate > 0 && std::abs(rate - w.rate) > 0.5) {
        // windowed-sinc conversion for recordings (the linear resample() is only for short IRs)
        for (auto& ch : w.channels) {
            const double ratio = w.rate / rate;
            std::vector<float> out((size_t) ((double) ch.size() / ratio) + 1);
            juce::LagrangeInterpolator li;
            const int made = li.process(ratio, ch.data(), out.data(), (int) out.size(), (int) ch.size(), 0);
            juce::ignoreUnused(made);
            ch = std::move(out);
        }
        w.rate = rate;
    }
    return w;
}

bool writeWav(const juce::File& f, const std::vector<const float*>& ch, int frames, double rate) {
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = f.createOutputStream();
    if (!stream) return false;
    juce::WavAudioFormat wav;   // 32-bit float, like the Mac app's recordings
    auto w = wav.createWriterFor(stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels((int) ch.size()).withBitsPerSample(32)
                                         .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    return w && w->writeFromFloatArrays(ch.data(), (int) ch.size(), frames);
}

// MARK: - IR library

static const juce::StringArray micOrder { "SM57", "MD421", "SM7B", "C414", "U87", "NT5" };

std::optional<IRFile> IRLibrary::parse(const juce::String& rel) {
    const auto name = juce::File::createFileWithoutCheckingPath(rel.replaceCharacter('\\', '/')).getFileNameWithoutExtension();
    auto t = juce::StringArray::fromTokens(name, "_", "");
    if (t.size() < 3) return std::nullopt;
    IRFile f; f.rel = rel;
    f.mic = t[0]; t.remove(0);
    if (f.mic == "57") f.mic = "SM57";
    if (t[0] == "TS") { f.ts = true; t.remove(0); }
    if (t.size() >= 2 && t[t.size() - 2] == "pres" && t[t.size() - 1].containsOnly("0123456789")) {
        f.pres = t[t.size() - 1].getIntValue(); t.removeRange(t.size() - 2, 2);
    }
    f.position = t.joinIntoString(" ");
    return f;
}

void IRLibrary::scan(double sampleRate) {
    const auto d = root().getChildFile(std::abs(sampleRate - 44100) < 1 ? "44.1" : "48");
    if (d == dir && !files.empty()) return;
    dir = d; cache.clear();
    std::vector<IRFile> out;
    for (const auto& e : juce::RangedDirectoryIterator(d, true, "*.wav", juce::File::findFiles)) {
        const auto rel = e.getFile().getRelativePathFrom(d).replaceCharacter('\\', '/');
        if (rel.contains("Legacy") || rel.contains("Stereo")) continue;
        if (auto f = parse(rel)) out.push_back(*f);
    }
    auto rank = [](const juce::String& m) { const int i = micOrder.indexOf(m); return i < 0 ? 99 : i; };
    std::sort(out.begin(), out.end(), [&](const IRFile& a, const IRFile& b) {
        return std::make_tuple(rank(a.mic), a.ts ? 1 : 0, a.position, a.pres) < std::make_tuple(rank(b.mic), b.ts ? 1 : 0, b.position, b.pres);
    });
    files = std::move(out);
}

juce::StringArray IRLibrary::mics() const {
    juce::StringArray have;
    for (const auto& f : files) have.addIfNotAlreadyThere(f.mic);
    juce::StringArray out;
    for (const auto& m : micOrder) if (have.contains(m)) out.add(m);
    juce::StringArray rest; for (const auto& m : have) if (!micOrder.contains(m)) rest.add(m);
    rest.sort(false); out.addArray(rest);
    return out;
}

const IRFile* IRLibrary::file(const juce::String& rel) const {
    for (const auto& f : files) if (f.rel == rel) return &f;
    return nullptr;
}

juce::StringArray IRLibrary::positions(const juce::String& mic, bool ts) const {
    juce::StringArray out;
    for (const auto& f : files) if (f.mic == mic && f.ts == ts) out.addIfNotAlreadyThere(f.position);
    return out;
}

bool IRLibrary::tsAvailable(const juce::String& mic) const {
    for (const auto& f : files) if (f.mic == mic && f.ts) return true;
    return false;
}

const IRFile* IRLibrary::find(const juce::String& mic, bool ts, const juce::String& position, int pres) const {
    std::vector<const IRFile*> pool;
    for (const auto& f : files) if (f.mic == mic && f.ts == ts) pool.push_back(&f);
    if (pool.empty()) for (const auto& f : files) if (f.mic == mic) pool.push_back(&f);
    for (auto* f : pool) if (f->position == position && f->pres == pres) return f;
    for (auto* f : pool) if (f->position == position) return f;
    for (auto* f : pool) if (f->pres == pres) return f;
    return pool.empty() ? nullptr : pool.front();
}

juce::String IRLibrary::step(const juce::String& rel, int d) const {
    if (files.empty()) return {};
    for (size_t i = 0; i < files.size(); ++i)
        if (files[i].rel == rel) return files[(i + (size_t) (d + (int) files.size())) % files.size()].rel;
    return files.front().rel;
}

std::optional<std::vector<float>> IRLibrary::samples(const juce::String& rel, double sampleRate) {
    if (cacheRate != sampleRate) { cache.clear(); cacheRate = sampleRate; }
    if (auto it = cache.find(rel); it != cache.end()) return it->second;
    if (rel.isEmpty() || dir == juce::File()) return std::nullopt;
    const auto f = dir.getChildFile(rel);
    if (!f.isAChildOf(dir)) return std::nullopt;                // no "../" escapes from a preset's IR name
    auto w = readAudio(f);
    if (w.rate <= 0 || w.channels.empty()) return std::nullopt;
    auto h = normaliseIR(resample(w.channels[0], w.rate, sampleRate));
    cache[rel] = h;
    return h;
}

// MARK: - Cab IR library

namespace {
bool isIRFile(const juce::File& f) { return f.hasFileExtension("wav;wave;aif;aiff"); }
}

void CabIRLibrary::scan() {
    root().createDirectory();
    files.clear();
    for (const auto& e : juce::RangedDirectoryIterator(root(), true, "*", juce::File::findFiles))
        if (isIRFile(e.getFile())) files.add(e.getFile().getRelativePathFrom(root()).replaceCharacter('\\', '/'));
    files.sortNatural();
    cache.clear();
}

juce::String CabIRLibrary::step(const juce::String& rel, int d) const {
    if (files.isEmpty()) return rel;
    const int i = files.indexOf(rel);
    if (i < 0) return files[0];
    return files[(i + d + files.size()) % files.size()];
}

std::optional<std::vector<float>> CabIRLibrary::samples(const juce::String& rel, double sampleRate) {
    if (cacheRate != sampleRate) { cache.clear(); cacheRate = sampleRate; }
    if (auto it = cache.find(rel); it != cache.end()) return it->second;
    if (rel.isEmpty()) return std::nullopt;
    const auto f = root().getChildFile(rel);
    if (!f.isAChildOf(root())) return std::nullopt;             // no "../" escapes from a preset's IR name
    auto w = readAudio(f);
    if (w.rate <= 0 || w.channels.empty()) return std::nullopt;
    auto h = normaliseIR(fitIR(resample(w.channels[0], w.rate, sampleRate), sampleRate));
    cache[rel] = h;
    return h;
}

int CabIRLibrary::importItems(const juce::Array<juce::File>& items) {
    int n = 0;
    for (const auto& u : items) {
        const auto dest = root().getChildFile(u.getFileName());
        if (u.isDirectory()) {
            for (const auto& e : juce::RangedDirectoryIterator(u, true, "*", juce::File::findFiles)) {
                const auto f = e.getFile();
                if (!isIRFile(f)) continue;
                const auto to = dest.getChildFile(f.getRelativePathFrom(u));
                to.getParentDirectory().createDirectory();
                if (f.copyFileTo(to)) ++n;
            }
        } else if (isIRFile(u)) {
            root().createDirectory();
            if (u.copyFileTo(dest)) ++n;
        }
    }
    scan();
    return n;
}

juce::String CabIRLibrary::title(const juce::String& rel) {
    auto n = rel.fromLastOccurrenceOf("/", false, false).upToLastOccurrenceOf(".", false, false);
    juce::StringArray w; w.addTokens(n, " ", ""); w.removeEmptyStrings();
    // drop a trailing "IR" (keeping a number after it: "IR 2" -> "2")
    if (w.size() >= 2 && w[w.size() - 1].containsOnly("0123456789") && w[w.size() - 2] == "IR") w.remove(w.size() - 2);
    else if (w.size() >= 2 && w[w.size() - 1] == "IR") w.remove(w.size() - 1);
    return w.joinIntoString(" ");
}

std::vector<float> fitIR(std::vector<float> h, double sampleRate) {
    if ((int) h.size() <= DSP::maxIR) return h;
    h.resize((size_t) DSP::maxIR);
    const int f = std::min((int) (sampleRate * 0.005), DSP::maxIR / 4);
    for (int i = 0; i < f; ++i) h[(size_t) (DSP::maxIR - 1 - i)] *= (float) i / (float) f;
    return h;
}

// MARK: - Captures

std::map<juce::String, Capture> Capture::loadAll() {
    std::map<juce::String, Capture> out;
    for (const auto& e : juce::RangedDirectoryIterator(dir(), false, "*.json", juce::File::findFiles)) {
        const auto v = juce::JSON::parse(e.getFile().loadFileAsString());
        if (!v.isObject()) continue;
        Capture c;
        c.name = v["name"].toString();
        auto num = [&](const char* k, double& dst) { const auto& x = v[k]; if (x.isDouble() || x.isInt() || x.isInt64()) dst = (double) x; };
        num("sr", c.sr); num("hp", c.hp); num("ls", c.ls); num("pk_f", c.pk_f); num("pk", c.pk); num("hs", c.hs);
        num("k1", c.k1); num("b1", c.b1); num("mid", c.mid); num("k2", c.k2); num("b2", c.b2); num("esr_val", c.esr_val);
        if (auto* a = v["ir"].getArray()) for (const auto& x : *a) c.ir.push_back((float) (double) x);
        if (c.ir.empty() || c.sr <= 0) continue;
        out[e.getFile().getFileNameWithoutExtension()] = std::move(c);
    }
    return out;
}

void Capture::apply(Params& p) const {
    p.capHP = (float) hp; p.capLS = (float) ls; p.capPkF = (float) pk_f; p.capPk = (float) pk; p.capHS = (float) hs;
    p.capK1 = (float) k1; p.capB1 = (float) b1; p.capMid = (float) mid; p.capK2 = (float) k2; p.capB2 = (float) b2;
}

} // namespace lev
