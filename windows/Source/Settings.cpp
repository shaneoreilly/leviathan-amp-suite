#include "Settings.h"

namespace lev {

namespace {
juce::var arr(std::initializer_list<double> xs) { juce::Array<juce::var> a; for (double x : xs) a.add(x); return a; }
juce::var arrI(std::initializer_list<int> xs) { juce::Array<juce::var> a; for (int x : xs) a.add(x); return a; }

/// Defaults, in the same order and with the same names as the Swift Settings struct.
void fillDefaults(juce::NamedValueSet& v) {
    auto put = [&](const char* k, juce::var x) { v.set(k, x); };
    put("inGain", 0.0); put("inputChannel", 0);
    put("gateOn", true); put("gateThresh", -60.0); put("gateRelease", 60.0);
    put("pitchOn", false); put("pitchSemis", 0.0); put("pitchCents", 0.0); put("pitchMix", 1.0);
    put("odOn", true); put("odDrive", 0.05); put("odTone", 0.55); put("odLevel", 0.8);
    put("ampOn", true); put("ampChannel", 2); put("ampGain", 0.55);
    put("bass", 0.5); put("mid", 0.55); put("treble", 0.55); put("presence", 0.55); put("depth", 0.55); put("ampVolume", 0.5);
    put("eqOn", true); put("eq", arr({ 0, 1, 0, 1, 1.5, 1, -1, -3 }));
    put("cabOn", true); put("cabModel", CAB_LIB); put("cabMic", 0.3); put("cabLowCut", 80.0); put("cabHighCut", 10000.0); put("cabLevel", 0.0);
    put("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); put("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); put("irBlend", 0.35);
    put("delaySync", 0);
    put("delayOn", false); put("delayTime", 420.0); put("delayFeedback", 0.28); put("delayTone", 0.45); put("delayMix", 0.2);
    put("revOn", false); put("revSize", 0.45); put("revMix", 0.18);
    put("masterVol", -6.0);
    put("chain", arrI({ (int) PedalKind::od }));
    put("compOn", true); put("compSustain", 0.5); put("compAttack", 10.0); put("compLevel", 0.0); put("compBlend", 1.0);
    put("wahOn", true); put("wahAuto", false); put("wahPos", 0.5); put("wahSens", 0.5); put("wahQ", 0.5);
    put("fuzzOn", true); put("fuzzAmt", 0.6); put("fuzzTone", 0.5); put("fuzzLevel", 0.5);
    put("chorusOn", true); put("chorusRate", 0.8); put("chorusDepth", 0.5); put("chorusMix", 0.5);
    put("flangerOn", true); put("flangerRate", 0.25); put("flangerDepth", 0.7); put("flangerFb", 0.5); put("flangerMix", 0.5);
    put("phaserOn", true); put("phaserRate", 0.5); put("phaserDepth", 0.7); put("phaserFb", 0.4);
    put("tremOn", true); put("tremRate", 5.0); put("tremDepth", 0.6); put("tremShape", 0.0);
    put("capture", ""); put("capDrive", 0.0); put("capLevel", 0.0);
}
} // namespace

Settings::Settings() { fillDefaults(values); }

Settings Settings::fromJSON(const juce::var& v) {
    Settings s;
    if (auto* o = v.getDynamicObject()) {
        for (const auto& p : o->getProperties()) {
            if (!s.values.contains(p.name)) continue;               // unknown keys (newer versions) are ignored
            const auto& def = s.values[p.name];
            // keep the default's type so a malformed file can't turn a number into a string or an array into a number
            if (def.isArray() != p.value.isArray()) continue;
            if (def.isString() != p.value.isString()) continue;
            if (def.isArray()) {
                juce::Array<juce::var> a;
                for (const auto& x : *p.value.getArray()) if (x.isInt() || x.isInt64() || x.isDouble() || x.isBool()) a.add(x);
                s.values.set(p.name, a);
            } else if (def.isBool()) {
                s.values.set(p.name, (bool) p.value);
            } else if (def.isInt()) {
                s.values.set(p.name, (int) p.value);
            } else if (def.isDouble()) {
                const double x = (double) p.value;
                if (std::isfinite(x)) s.values.set(p.name, x);
            } else {
                s.values.set(p.name, p.value);
            }
        }
    }
    return s;
}

juce::var Settings::toVar() const {
    auto* o = new juce::DynamicObject();
    for (const auto& p : values) o->setProperty(p.name, p.value);
    return juce::var(o);
}

juce::String Settings::toJSON(bool pretty) const {
    return juce::JSON::toString(toVar(), pretty ? juce::JSON::FormatOptions{} : juce::JSON::FormatOptions{}.withSpacing(juce::JSON::Spacing::none));
}

double Settings::d(const juce::Identifier& k) const { return (double) values[k]; }
bool Settings::b(const juce::Identifier& k) const { return (bool) values[k]; }
int Settings::i(const juce::Identifier& k) const { return (int) values[k]; }
juce::String Settings::s(const juce::Identifier& k) const { return values[k].toString(); }

void Settings::set(const juce::Identifier& k, const juce::var& v) {
    if (!values.contains(k)) return;
    const auto& def = values[k];
    if (def.isBool()) values.set(k, (bool) v);
    else if (def.isInt()) values.set(k, (int) v);
    else if (def.isDouble()) values.set(k, (double) v);
    else values.set(k, v);
}

std::vector<double> Settings::eq() const {
    std::vector<double> out(8, 0.0);
    if (auto* a = values["eq"].getArray()) for (int i = 0; i < std::min(8, a->size()); ++i) out[(size_t) i] = (double) (*a)[i];
    return out;
}

void Settings::setEq(int band, double db) {
    auto e = eq(); if (band < 0 || band >= 8) return;
    e[(size_t) band] = db;
    juce::Array<juce::var> a; for (double x : e) a.add(x);
    values.set("eq", a);
}

std::vector<int> Settings::chain() const {
    std::vector<int> out;
    if (auto* a = values["chain"].getArray()) for (const auto& x : *a) {
        const int k = (int) x;
        if (k >= 0 && k < kPedalCount && std::find(out.begin(), out.end(), k) == out.end()) out.push_back(k);
    }
    return out;
}

void Settings::setChain(const std::vector<int>& c) {
    juce::Array<juce::var> a; for (int x : c) a.add(x);
    values.set("chain", a);
}

void Settings::apply(Params& p) const {
    const auto ch = chain();
    p.chainCount = 0;
    for (size_t k = 0; k < std::min<size_t>(ch.size(), Params::maxChain); ++k) p.chain[k] = ch[k];
    p.chainCount = (int) std::min<size_t>(ch.size(), Params::maxChain);
    auto f = [&](const char* k) { return (float) d(k); };
    p.compOn = b("compOn"); p.compSustain = f("compSustain"); p.compAttack = f("compAttack"); p.compLevel = f("compLevel"); p.compBlend = f("compBlend");
    p.wahOn = b("wahOn"); p.wahAuto = b("wahAuto"); p.wahPos = f("wahPos"); p.wahSens = f("wahSens"); p.wahQ = f("wahQ");
    p.fuzzOn = b("fuzzOn"); p.fuzzAmt = f("fuzzAmt"); p.fuzzTone = f("fuzzTone"); p.fuzzLevel = f("fuzzLevel");
    p.chorusOn = b("chorusOn"); p.chorusRate = f("chorusRate"); p.chorusDepth = f("chorusDepth"); p.chorusMix = f("chorusMix");
    p.flangerOn = b("flangerOn"); p.flangerRate = f("flangerRate"); p.flangerDepth = f("flangerDepth"); p.flangerFb = f("flangerFb"); p.flangerMix = f("flangerMix");
    p.phaserOn = b("phaserOn"); p.phaserRate = f("phaserRate"); p.phaserDepth = f("phaserDepth"); p.phaserFb = f("phaserFb");
    p.tremOn = b("tremOn"); p.tremRate = f("tremRate"); p.tremDepth = f("tremDepth"); p.tremShape = f("tremShape");
    p.capDrive = f("capDrive"); p.capLevel = f("capLevel");
    p.inGain = f("inGain"); p.inputChannel = i("inputChannel");
    p.gateOn = b("gateOn"); p.gateThresh = f("gateThresh"); p.gateRelease = f("gateRelease");
    p.pitchOn = b("pitchOn"); p.pitchSemis = f("pitchSemis"); p.pitchCents = f("pitchCents"); p.pitchMix = f("pitchMix");
    p.odOn = b("odOn"); p.odDrive = f("odDrive"); p.odTone = f("odTone"); p.odLevel = f("odLevel");
    p.ampOn = b("ampOn"); p.ampChannel = i("ampChannel"); p.ampGain = f("ampGain");
    p.bass = f("bass"); p.mid = f("mid"); p.treble = f("treble"); p.presence = f("presence"); p.depth = f("depth"); p.ampVolume = f("ampVolume");
    p.eqOn = b("eqOn"); { const auto e = eq(); for (int k = 0; k < 8; ++k) p.eq[k] = (float) e[(size_t) k]; }
    p.cabOn = b("cabOn"); p.cabLowCut = f("cabLowCut"); p.cabHighCut = f("cabHighCut"); p.cabLevel = f("cabLevel");
    p.delayOn = b("delayOn"); p.delayTime = f("delayTime"); p.delayFeedback = f("delayFeedback"); p.delayTone = f("delayTone"); p.delayMix = f("delayMix");
    p.revOn = b("revOn"); p.revSize = f("revSize"); p.revMix = f("revMix");
    p.masterVol = f("masterVol");
    p.version.fetch_add(1, std::memory_order_release);
}

// MARK: - Factory presets (same as FACTORY in App.swift)

const std::vector<Factory> FACTORY = {
    { "Tight Rhythm (Gojira-ish)", [](Settings&) {} },
    { "Sludge Crunch (Mastodon-ish)", [](Settings& s) {
        s.set("odOn", false); s.set("ampChannel", 1); s.set("ampGain", 0.8); s.set("bass", 0.65); s.set("mid", 0.6); s.set("treble", 0.5);
        s.set("presence", 0.45); s.set("depth", 0.65);
        double e[] = { 1, 1, 0, 0, 0, 0, -1, -3 }; for (int k = 0; k < 8; ++k) s.setEq(k, e[k]);
        s.set("irA", "SM57/NO-TS/57_2_inch_cone_far_pres_2.wav"); s.set("irBlend", 0.5); } },
    { "Screaming Lead", [](Settings& s) {
        s.set("gateThresh", -64.0); s.set("odDrive", 0.2); s.set("ampGain", 0.7); s.set("mid", 0.65); s.set("treble", 0.5);
        double e[] = { -2, -1, 0, 1, 2, 1, -1, -4 }; for (int k = 0; k < 8; ++k) s.setEq(k, e[k]);
        s.set("irA", "SM57/TS/57_TS_1_inch_edge_pres_3.wav"); s.set("delayOn", true); s.set("delayTime", 450.0); s.set("delayMix", 0.22);
        s.set("revOn", true); s.set("revMix", 0.15); } },
    // Tight modern rhythm: tight gate -> boosted TS-style overdrive -> hot channel with scooped mids
    // (EQ cut from 320 Hz to 2.5 kHz) -> 4x12 with a 57 on the cap.
    { "Modern Metal Rhythm", [](Settings& s) {
        s.set("gateOn", true); s.set("gateThresh", -48.0); s.set("gateRelease", 90.0);
        s.setChain({ (int) PedalKind::od }); s.set("odOn", true); s.set("odDrive", 0.85); s.set("odTone", 0.7); s.set("odLevel", 0.5);
        s.set("ampOn", true); s.set("ampChannel", 2); s.set("ampGain", 0.65);
        s.set("bass", 0.57); s.set("mid", 0.35); s.set("treble", 0.7); s.set("presence", 0.8); s.set("depth", 0.2); s.set("ampVolume", 0.6);
        s.set("eqOn", true); double e[] = { 0, 0, -3.5, -4.9, -4.3, -1.8, 0, 0 }; for (int k = 0; k < 8; ++k) s.setEq(k, e[k]);
        s.set("cabOn", true); s.set("cabModel", CAB_LIB); s.set("irA", "SM57/NO-TS/57_1_inch_cap_pres_4.wav"); s.set("irB", ""); s.set("irBlend", 0.0);
        s.set("cabLowCut", 80.0); s.set("cabHighCut", 10000.0);
        s.set("delayOn", false); s.set("revOn", false); } },
    { "Ambient Clean", [](Settings& s) {
        s.set("gateThresh", -72.0); s.set("odOn", false); s.set("ampChannel", 0); s.set("ampGain", 0.4); s.set("bass", 0.5); s.set("mid", 0.5);
        s.set("treble", 0.6); s.set("ampVolume", 0.55);
        double e[] = { 0, 0, 0, 0, 0, 1, 1, 0 }; for (int k = 0; k < 8; ++k) s.setEq(k, e[k]);
        s.set("cabModel", 2); s.set("cabHighCut", 10000.0);
        s.set("delayOn", true); s.set("delayTime", 520.0); s.set("delayFeedback", 0.42); s.set("delayMix", 0.3);
        s.set("revOn", true); s.set("revSize", 0.8); s.set("revMix", 0.35); } },
};

// MARK: - Tunings

const std::vector<Tuning> TUNINGS = {
    { "E Standard", { 40, 45, 50, 55, 59, 64 }, false },
    { "Eb Standard", { 39, 44, 49, 54, 58, 63 }, true },
    { "D Standard", { 38, 43, 48, 53, 57, 62 }, false },
    { "Drop D", { 38, 45, 50, 55, 59, 64 }, false },
    { "Drop Db", { 37, 44, 49, 54, 58, 63 }, true },
    { "Drop C", { 36, 43, 48, 53, 57, 62 }, false },
    { "C Standard", { 36, 41, 46, 51, 55, 60 }, true },
    { "Drop B", { 35, 42, 47, 52, 56, 61 }, false },
    { "Drop A", { 33, 40, 45, 50, 54, 59 }, false },
};

juce::String tuningName(const std::array<int, 6>& s) {
    static const char* names[] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    auto n = [](int m) { return juce::String(names[((m % 12) + 12) % 12]); };
    std::array<int, 5> iv {};
    for (int k = 0; k < 5; ++k) iv[(size_t) k] = s[(size_t) k + 1] - s[(size_t) k];
    if (iv == std::array<int, 5> { 5, 5, 5, 4, 5 }) return n(s[0]) + " Standard";
    if (iv == std::array<int, 5> { 7, 5, 5, 4, 5 }) return "Drop " + n(s[0]);
    juce::StringArray parts; for (int m : s) parts.add(n(m));
    return parts.joinIntoString(" ");
}

juce::String noteName(int midi, bool flats) {
    static const char* sharp[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flat[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return (flats ? flat : sharp)[((midi % 12) + 12) % 12];
}

const std::vector<DelayDiv> DELAY_DIVS = {
    { "Free (ms)", 0 }, { "1/2", 2 }, { "1/4", 1 }, { "1/8 dotted", 0.75 }, { "1/8", 0.5 },
    { "1/4 triplet", 2.0 / 3 }, { "1/8 triplet", 1.0 / 3 }, { "1/16", 0.25 },
};

} // namespace lev
