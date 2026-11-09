#ifdef __ANDROID__
 #define JUCE_CORE_INCLUDE_JNI_HELPERS 1   // getEnv/getAppContext, for the app's external Music folder
#endif
#include "Library.h"

namespace lev {

juce::File appDataDir() {
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support/Leviathan");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Leviathan");
#endif
}

#if JUCE_ANDROID
/// Context.getExternalFilesDir("Music"): the app's own folder on shared storage. Android's public Music folder
/// only takes audio files, and every take also writes session.json.
static juce::File androidExternalMusicDir() {
    auto* env = juce::getEnv();
    const auto ctx = juce::getAppContext();
    const juce::LocalRef<jclass> ctxClass { env->GetObjectClass(ctx.get()) };
    const auto getDir = env->GetMethodID(ctxClass.get(), "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    const juce::LocalRef<jstring> type { env->NewStringUTF("Music") };
    const juce::LocalRef<jobject> dir { env->CallObjectMethod(ctx.get(), getDir, type.get()) };
    if (dir == nullptr) return {};
    const juce::LocalRef<jclass> fileClass { env->GetObjectClass(dir.get()) };
    const auto getPath = env->GetMethodID(fileClass.get(), "getAbsolutePath", "()Ljava/lang/String;");
    const juce::LocalRef<jstring> path { (jstring) env->CallObjectMethod(dir.get(), getPath) };
    return juce::File(juce::juceString(path.get()));
}
#endif

juce::File musicDir() {
#if JUCE_ANDROID
    static const juce::File dir = [] { const auto d = androidExternalMusicDir(); return d == juce::File() ? appDataDir().getChildFile("Music") : d; }();
    return dir.getChildFile("Leviathan");
#else
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Leviathan");
#endif
}

// MARK: - file pickers

#if JUCE_ANDROID
/// Copies a picked document (a file, or a folder with its whole tree) to `dest`, returning what was written.
static juce::File copyDocument(const juce::AndroidDocument& doc, const juce::File& destDir) {
    const auto info = doc.getInfo();
    const auto name = juce::File::createLegalFileName(info.getName().isNotEmpty() ? info.getName() : juce::String("picked"));
    if (info.isDirectory()) {
        const auto dir = destDir.getChildFile(name);
        dir.createDirectory();
        for (const auto& child : juce::AndroidDocumentIterator::makeNonRecursive(doc)) copyDocument(child, dir);
        return dir;
    }
    const auto out = destDir.getNonexistentChildFile(juce::File(name).getFileNameWithoutExtension(), juce::File(name).getFileExtension(), false);
    if (auto in = doc.createInputStream()) {
        juce::FileOutputStream os(out);
        if (os.openedOk()) { os.writeFromInputStream(*in, -1); os.flush(); }
    }
    return out;
}

/// A fresh folder per pick under the cache; picks older than a day are cleared (by then they've been imported).
static juce::File importStaging() {
    const auto base = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Imports");
    for (const auto& e : juce::RangedDirectoryIterator(base, false, "*", juce::File::findDirectories))
        if (e.getModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days(1)) e.getFile().deleteRecursively();
    const auto dir = base.getChildFile(juce::String(juce::Time::currentTimeMillis()));
    dir.createDirectory();
    return dir;
}
#endif

juce::Array<juce::File> pickedFiles(const juce::FileChooser& fc) {
#if JUCE_ANDROID
    juce::Array<juce::File> out;
    const auto urls = fc.getURLResults();
    if (urls.isEmpty()) return out;
    const auto staging = importStaging();
    for (const auto& u : urls) {
        const bool folder = u.toString(false).contains("/tree/");   // folder picks come back as document-tree URIs
        const auto doc = folder ? juce::AndroidDocument::fromTree(u) : juce::AndroidDocument::fromDocument(u);
        if (doc.hasValue()) out.add(copyDocument(doc, staging));
    }
    return out;
#else
    return fc.getResults();
#endif
}

std::optional<bool> writePicked(const juce::FileChooser& fc, const juce::String& text, const juce::String& ext) {
#if JUCE_ANDROID
    const auto url = fc.getURLResult();
    if (url.isEmpty()) return std::nullopt;
    auto os = juce::AndroidDocument::fromDocument(url).createOutputStream();
    if (os == nullptr) return false;
    const auto ok = os->writeText(text, false, false, nullptr);
    os->flush();
    return ok;
#else
    auto f = fc.getResult();
    if (f == juce::File()) return std::nullopt;
    if (ext.isNotEmpty() && !f.hasFileExtension(ext)) f = f.withFileExtension(ext);
    return f.replaceWithText(text);
#endif
}

std::optional<PickedFolder> pickedFolder(const juce::FileChooser& fc) {
#if JUCE_ANDROID
    const auto url = fc.getURLResult();
    if (url.isEmpty()) return std::nullopt;
    const auto tree = juce::AndroidDocument::fromTree(url);
    if (!tree.hasValue()) return std::nullopt;
    return PickedFolder { tree.getInfo().getName(),
        [tree](const juce::String& fileName, const juce::String& text) {
            const auto doc = tree.createChildDocumentWithTypeAndName("application/octet-stream", fileName);
            auto os = doc.hasValue() ? doc.createOutputStream() : nullptr;
            if (os == nullptr) return false;
            const auto ok = os->writeText(text, false, false, nullptr);
            os->flush();
            return ok;
        },
        [] {} };
#else
    const auto dir = fc.getResult();
    if (dir == juce::File()) return std::nullopt;
    return PickedFolder { dir.getFileName(),
        [dir](const juce::String& fileName, const juce::String& text) { return dir.getChildFile(fileName).replaceWithText(text); },
        [dir] { dir.revealToUser(); } };
#endif
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

bool writeWav(const juce::File& f, const std::vector<const float*>& ch, int frames, double rate, bool bits16) {
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = f.createOutputStream();
    if (!stream) return false;
    juce::WavAudioFormat wav;   // 32-bit float, like the Mac app's recordings
    auto opts = juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels((int) ch.size());
    opts = bits16 ? opts.withBitsPerSample(16)
                  : opts.withBitsPerSample(32).withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto w = wav.createWriterFor(stream, opts);
    return w && w->writeFromFloatArrays(ch.data(), (int) ch.size(), frames);
}

#if JUCE_ANDROID
namespace {
/// A minimal .m4a (MP4 audio) writer laid out like Apple's encoder writes them: ftyp, moov first, then mdat, with
/// the AAC packets in chunks of 24 and 32-bit chunk offsets. Android's MediaMuxer puts every packet in one chunk
/// with 64-bit offsets and the index last, which WhatsApp plays with breaks (verified against an Apple-encoded copy).
struct Mp4 {
    std::vector<uint8_t> d;
    void u8(int v) { d.push_back((uint8_t) v); }
    void u16(int v) { u8(v >> 8); u8(v); }
    void u32(uint32_t v) { u8((int) (v >> 24)); u8((int) (v >> 16)); u8((int) (v >> 8)); u8((int) v); }
    void str(const char* s) { while (*s) u8(*s++); }
    void zeros(int n) { d.insert(d.end(), (size_t) n, 0); }
    void bytes(const std::vector<uint8_t>& b) { d.insert(d.end(), b.begin(), b.end()); }
    size_t open(const char* type) { const auto at = d.size(); u32(0); str(type); return at; }
    void close(size_t at) { const auto n = (uint32_t) (d.size() - at); d[at] = (uint8_t) (n >> 24); d[at + 1] = (uint8_t) (n >> 16); d[at + 2] = (uint8_t) (n >> 8); d[at + 3] = (uint8_t) n; }
    void matrix() { const uint32_t m[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000 }; for (auto v : m) u32(v); }
    void descriptor(int tag, const std::vector<uint8_t>& body) { u8(tag); u8((int) body.size()); bytes(body); }
};

std::vector<uint8_t> buildM4A(const std::vector<std::vector<uint8_t>>& frames, const std::vector<uint8_t>& asc, int rate, int channels, int bitrate) {
    constexpr int perChunk = 24;
    const auto count = (uint32_t) frames.size();
    const uint32_t chunks = (count + perChunk - 1) / perChunk, duration = count * 1024;
    auto moov = [&](uint32_t mdatData) {
        Mp4 m;
        const auto mv = m.open("moov");
        { const auto b = m.open("mvhd"); m.u32(0); m.u32(0); m.u32(0); m.u32(1000); m.u32((uint32_t) ((uint64_t) duration * 1000 / (uint32_t) rate));
          m.u32(0x10000); m.u16(0x100); m.zeros(10); m.matrix(); m.zeros(24); m.u32(2); m.close(b); }
        const auto tr = m.open("trak");
        { const auto b = m.open("tkhd"); m.u32(7); m.u32(0); m.u32(0); m.u32(1); m.u32(0); m.u32((uint32_t) ((uint64_t) duration * 1000 / (uint32_t) rate));
          m.zeros(8); m.u16(0); m.u16(0); m.u16(0x100); m.u16(0); m.matrix(); m.u32(0); m.u32(0); m.close(b); }
        const auto md = m.open("mdia");
        { const auto b = m.open("mdhd"); m.u32(0); m.u32(0); m.u32(0); m.u32((uint32_t) rate); m.u32(duration); m.u16(0x55c4 /* und */); m.u16(0); m.close(b); }
        { const auto b = m.open("hdlr"); m.u32(0); m.u32(0); m.str("soun"); m.zeros(12); m.str("SoundHandler"); m.u8(0); m.close(b); }
        const auto mi = m.open("minf");
        { const auto b = m.open("smhd"); m.u32(0); m.u16(0); m.u16(0); m.close(b); }
        { const auto di = m.open("dinf"); const auto dr = m.open("dref"); m.u32(0); m.u32(1); { const auto u = m.open("url "); m.u32(1); m.close(u); } m.close(dr); m.close(di); }
        const auto st = m.open("stbl");
        { const auto sd = m.open("stsd"); m.u32(0); m.u32(1);
          const auto a = m.open("mp4a"); m.zeros(6); m.u16(1); m.zeros(8); m.u16(channels); m.u16(16); m.u16(0); m.u16(0); m.u32((uint32_t) rate << 16);
          { const auto e = m.open("esds"); m.u32(0);
            Mp4 dsi; dsi.bytes(asc);
            Mp4 dcd; dcd.u8(0x40); dcd.u8(0x15); dcd.u8(0); dcd.u16(0x1800); dcd.u32((uint32_t) bitrate); dcd.u32((uint32_t) bitrate); dcd.descriptor(0x05, dsi.d);
            Mp4 es; es.u16(1); es.u8(0); es.descriptor(0x04, dcd.d); es.descriptor(0x06, { 0x02 });
            m.descriptor(0x03, es.d); m.close(e); }
          m.close(a); m.close(sd); }
        { const auto b = m.open("stts"); m.u32(0); m.u32(1); m.u32(count); m.u32(1024); m.close(b); }
        { const auto b = m.open("stsc"); m.u32(0); const uint32_t rem = count % perChunk;
          m.u32(rem == 0 || chunks == 1 ? 1 : 2); m.u32(1); m.u32(chunks == 1 ? count : (uint32_t) perChunk); m.u32(1);
          if (rem != 0 && chunks > 1) { m.u32(chunks); m.u32(rem); m.u32(1); }
          m.close(b); }
        { const auto b = m.open("stsz"); m.u32(0); m.u32(0); m.u32(count); for (auto& f : frames) m.u32((uint32_t) f.size()); m.close(b); }
        { const auto b = m.open("stco"); m.u32(0); m.u32(chunks); uint32_t off = mdatData;
          for (uint32_t c = 0; c < chunks; ++c) { m.u32(off); for (uint32_t k = c * perChunk; k < std::min(count, (c + 1) * perChunk); ++k) off += (uint32_t) frames[k].size(); }
          m.close(b); }
        m.close(st); m.close(mi); m.close(md); m.close(tr); m.close(mv);
        return m.d;
    };
    Mp4 f;
    { const auto b = f.open("ftyp"); f.str("M4A "); f.u32(0); f.str("M4A "); f.str("mp42"); f.str("isom"); f.close(b); }
    const auto head = (uint32_t) (f.d.size() + moov(0).size() + 8);   // the index doesn't change size with its offsets
    f.bytes(moov(head));
    const auto mdat = f.open("mdat");
    for (auto& fr : frames) f.bytes(fr);
    f.close(mdat);
    return f.d;
}
} // namespace

/// AAC from Android's own encoder (MediaCodec, through JNI), written into an .m4a by buildM4A.
bool writeM4A(const juce::File& f, const std::vector<const float*>& ch, int frames, double rate) {
    auto* env = juce::getEnv();
    if (env == nullptr || ch.empty() || frames <= 0) return false;
    const int nch = std::min((int) ch.size(), 2), bitrate = 192000;
    auto cls = [env](const char* n) { return juce::LocalRef<jclass>(env->FindClass(n)); };
    auto ok = [env] { if (env->ExceptionCheck()) { env->ExceptionClear(); return false; } return true; };
    const auto formatC = cls("android/media/MediaFormat"), codecC = cls("android/media/MediaCodec"), infoC = cls("android/media/MediaCodec$BufferInfo");
    const juce::LocalRef<jstring> mime { env->NewStringUTF("audio/mp4a-latm") };
    const juce::LocalRef<jobject> format { env->CallStaticObjectMethod(formatC.get(), env->GetStaticMethodID(formatC.get(), "createAudioFormat",
        "(Ljava/lang/String;II)Landroid/media/MediaFormat;"), mime.get(), (jint) rate, (jint) nch) };
    if (!ok() || format == nullptr) return false;
    const auto setInt = env->GetMethodID(formatC.get(), "setInteger", "(Ljava/lang/String;I)V");
    auto put = [&](const char* k, int v) { const juce::LocalRef<jstring> key { env->NewStringUTF(k) }; env->CallVoidMethod(format.get(), setInt, key.get(), (jint) v); };
    put("aac-profile", 2);          // AAC LC
    put("bitrate", bitrate);
    put("max-input-size", 16384);
    const juce::LocalRef<jobject> codec { env->CallStaticObjectMethod(codecC.get(), env->GetStaticMethodID(codecC.get(), "createEncoderByType",
        "(Ljava/lang/String;)Landroid/media/MediaCodec;"), mime.get()) };
    if (!ok() || codec == nullptr) return false;
    env->CallVoidMethod(codec.get(), env->GetMethodID(codecC.get(), "configure",
        "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V"), format.get(), nullptr, nullptr, (jint) 1 /* ENCODE */);
    if (!ok()) return false;
    env->CallVoidMethod(codec.get(), env->GetMethodID(codecC.get(), "start", "()V"));
    if (!ok()) return false;

    const auto dequeueIn = env->GetMethodID(codecC.get(), "dequeueInputBuffer", "(J)I"),
               getIn = env->GetMethodID(codecC.get(), "getInputBuffer", "(I)Ljava/nio/ByteBuffer;"),
               queueIn = env->GetMethodID(codecC.get(), "queueInputBuffer", "(IIIJI)V"),
               dequeueOut = env->GetMethodID(codecC.get(), "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I"),
               getOut = env->GetMethodID(codecC.get(), "getOutputBuffer", "(I)Ljava/nio/ByteBuffer;"),
               releaseOut = env->GetMethodID(codecC.get(), "releaseOutputBuffer", "(IZ)V");
    const auto fSize = env->GetFieldID(infoC.get(), "size", "I"), fOffset = env->GetFieldID(infoC.get(), "offset", "I"), fFlags = env->GetFieldID(infoC.get(), "flags", "I");
    const juce::LocalRef<jobject> info { env->NewObject(infoC.get(), env->GetMethodID(infoC.get(), "<init>", "()V")) };

    std::vector<std::vector<uint8_t>> packets;
    std::vector<uint8_t> asc;
    int pos = 0;
    bool inputDone = false, outputDone = false, good = true;
    while (!outputDone && good) {
        if (!inputDone) {
            const int ii = env->CallIntMethod(codec.get(), dequeueIn, (jlong) 10000);
            if (!ok()) { good = false; break; }
            if (ii >= 0) {
                const juce::LocalRef<jobject> buf { env->CallObjectMethod(codec.get(), getIn, (jint) ii) };
                auto* dst = static_cast<int16_t*>(env->GetDirectBufferAddress(buf.get()));
                const int cap = (int) env->GetDirectBufferCapacity(buf.get()) / (2 * nch);
                const int n = std::min({ cap, 1024, frames - pos });
                for (int i = 0; i < n; ++i)
                    for (int c = 0; c < nch; ++c)
                        dst[i * nch + c] = (int16_t) juce::jlimit(-32768, 32767, juce::roundToInt(ch[(size_t) c][pos + i] * 32767.0f));
                const jlong pts = (jlong) ((double) pos * 1000000.0 / rate);
                pos += n;
                inputDone = pos >= frames;
                env->CallVoidMethod(codec.get(), queueIn, (jint) ii, (jint) 0, (jint) (n * 2 * nch), pts, (jint) (inputDone ? 4 : 0) /* END_OF_STREAM */);
                if (!ok()) { good = false; break; }
            }
        }
        const int oi = env->CallIntMethod(codec.get(), dequeueOut, info.get(), (jlong) 10000);
        if (!ok()) { good = false; break; }
        if (oi >= 0) {
            const int flags = env->GetIntField(info.get(), fFlags), size = env->GetIntField(info.get(), fSize), offset = env->GetIntField(info.get(), fOffset);
            if (size > 0) {
                const juce::LocalRef<jobject> buf { env->CallObjectMethod(codec.get(), getOut, (jint) oi) };
                const auto* p = static_cast<const uint8_t*>(env->GetDirectBufferAddress(buf.get())) + offset;
                if ((flags & 2) != 0) asc.assign(p, p + size);   // CODEC_CONFIG: the AudioSpecificConfig
                else packets.emplace_back(p, p + size);
            }
            env->CallVoidMethod(codec.get(), releaseOut, (jint) oi, (jboolean) false);
            if (!ok()) { good = false; break; }
            outputDone = (flags & 4) != 0;
        }
    }
    env->CallVoidMethod(codec.get(), env->GetMethodID(codecC.get(), "stop", "()V")); ok();
    env->CallVoidMethod(codec.get(), env->GetMethodID(codecC.get(), "release", "()V")); ok();
    if (!good || !outputDone || packets.empty() || asc.empty()) return false;
    const auto data = buildM4A(packets, asc, (int) rate, nch, bitrate);
    return f.replaceWithData(data.data(), data.size());
}
#else
bool writeM4A(const juce::File&, const std::vector<const float*>&, int, double) { return false; }
#endif

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

int IRLibrary::importPack(const juce::Array<juce::File>& items) {
    int n = 0;
    std::function<void(const juce::File&)> visit = [&](const juce::File& d) {
        if (!d.isDirectory()) return;
        const auto name = d.getFileName();
        if (name == "44.1" || name == "48") {
            for (const auto& e : juce::RangedDirectoryIterator(d, true, "*.wav", juce::File::findFiles)) {
                const auto to = root().getChildFile(name).getChildFile(e.getFile().getRelativePathFrom(d));
                to.getParentDirectory().createDirectory();
                if (e.getFile().copyFileTo(to)) ++n;
            }
            return;
        }
        for (const auto& e : juce::RangedDirectoryIterator(d, false, "*", juce::File::findDirectories)) visit(e.getFile());
    };
    for (const auto& f : items) visit(f);
    return n;
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
