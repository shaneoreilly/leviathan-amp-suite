#include "Settings.h"

namespace lev {

namespace {
juce::var arr(std::initializer_list<double> xs) { juce::Array<juce::var> a; for (double x : xs) a.add(x); return a; }
juce::var arrI(std::initializer_list<int> xs) { juce::Array<juce::var> a; for (int x : xs) a.add(x); return a; }

/// Defaults, in the same order and with the same names as the Swift Settings struct.
void fillDefaults(juce::NamedValueSet& v) {
    auto put = [&](const char* k, juce::var x) { v.set(k, x); };
    put("inGain", 0.0); put("inputChannel", 0);
    put("gateOn", true); put("gateThresh", -60.0); put("gateRelease", 60.0); put("gateDepth", -60.0);
    put("pitchOn", false); put("pitchSemis", 0.0); put("pitchCents", 0.0); put("pitchMix", 1.0);
    put("odOn", true); put("odDrive", 0.05); put("odTone", 0.55); put("odLevel", 0.8);
    put("ampOn", true); put("ampChannel", 2); put("ampGain", 0.55);
    put("bass", 0.5); put("mid", 0.55); put("treble", 0.55); put("presence", 0.55); put("depth", 0.55); put("ampVolume", 0.5);
    put("eqOn", true); put("eq", arr({ 0, 1, 0, 1, 1.5, 1, -1, -3 }));
    put("cabOn", true); put("cabModel", CAB_LIB); put("cabMic", 0.3); put("cabLowCut", 80.0); put("cabHighCut", 10000.0); put("cabLevel", 0.0);
    put("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); put("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); put("irBlend", 0.35);
    put("irFile", "");   // with cabModel == CAB_IRLIB: path inside the cab IR library
    put("delaySync", 0);
    put("delayOn", false); put("delayTime", 420.0); put("delayFeedback", 0.28); put("delayTone", 0.45); put("delayMix", 0.2);
    put("revOn", false); put("revSize", 0.45); put("revMix", 0.18);
    put("doublerOn", true); put("doublerWidth", 0.6); put("doublerTime", 20.0);
    put("masterVol", -6.0);
    put("chain", arrI({ (int) PedalKind::od }));
    put("compOn", true); put("compSustain", 0.5); put("compAttack", 10.0); put("compLevel", 0.0); put("compBlend", 1.0);
    put("wahOn", true); put("wahAuto", false); put("wahPos", 0.5); put("wahSens", 0.5); put("wahQ", 0.5);
    put("fuzzOn", true); put("fuzzAmt", 0.6); put("fuzzTone", 0.5); put("fuzzLevel", 0.5);
    put("chorusOn", true); put("chorusRate", 0.8); put("chorusDepth", 0.5); put("chorusMix", 0.5);
    put("flangerOn", true); put("flangerRate", 0.25); put("flangerDepth", 0.7); put("flangerFb", 0.5); put("flangerMix", 0.5);
    put("phaserOn", true); put("phaserRate", 0.5); put("phaserDepth", 0.7); put("phaserFb", 0.4);
    put("tremOn", true); put("tremRate", 5.0); put("tremDepth", 0.6); put("tremShape", 0.0);
    put("bladeOn", true); put("bladeDrive", 0.5); put("bladeLow", 0.5); put("bladeMid", 0.5); put("bladeHigh", 0.5); put("bladeLevel", 0.5);
    put("bladeBoost", 0.5); put("bladeBoostOn", false); put("bladeShift", false); put("bladeDiode", 0);
    put("capture", ""); put("capDrive", 0.0); put("capLevel", 0.0);
    put("nam", ""); put("namInput", 0.0); put("namOutput", 0.0);
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
    p.bladeOn = b("bladeOn"); p.bladeDrive = f("bladeDrive"); p.bladeLow = f("bladeLow"); p.bladeMid = f("bladeMid"); p.bladeHigh = f("bladeHigh");
    p.bladeLevel = f("bladeLevel"); p.bladeBoost = f("bladeBoost"); p.bladeBoostOn = b("bladeBoostOn"); p.bladeShift = b("bladeShift"); p.bladeDiode = i("bladeDiode");
    p.capDrive = f("capDrive"); p.capLevel = f("capLevel");
    p.namIn = f("namInput"); p.namOut = f("namOutput");
    p.inGain = f("inGain"); p.inputChannel = i("inputChannel");
    p.gateOn = b("gateOn"); p.gateThresh = f("gateThresh"); p.gateRelease = f("gateRelease"); p.gateDepth = f("gateDepth");
    p.pitchOn = b("pitchOn"); p.pitchSemis = f("pitchSemis"); p.pitchCents = f("pitchCents"); p.pitchMix = f("pitchMix");
    p.odOn = b("odOn"); p.odDrive = f("odDrive"); p.odTone = f("odTone"); p.odLevel = f("odLevel");
    p.ampOn = b("ampOn"); p.ampChannel = i("ampChannel"); p.ampGain = f("ampGain");
    p.bass = f("bass"); p.mid = f("mid"); p.treble = f("treble"); p.presence = f("presence"); p.depth = f("depth"); p.ampVolume = f("ampVolume");
    p.eqOn = b("eqOn"); { const auto e = eq(); for (int k = 0; k < 8; ++k) p.eq[k] = (float) e[(size_t) k]; }
    p.cabOn = b("cabOn"); p.cabLowCut = f("cabLowCut"); p.cabHighCut = f("cabHighCut"); p.cabLevel = f("cabLevel");
    p.delayOn = b("delayOn"); p.delayTime = f("delayTime"); p.delayFeedback = f("delayFeedback"); p.delayTone = f("delayTone"); p.delayMix = f("delayMix");
    p.revOn = b("revOn"); p.revSize = f("revSize"); p.revMix = f("revMix");
    p.doublerOn = b("doublerOn"); p.doublerWidth = f("doublerWidth"); p.doublerTime = f("doublerTime");
    p.masterVol = f("masterVol");
    p.version.fetch_add(1, std::memory_order_release);
}

// MARK: - Factory presets (same as FACTORY in App.swift)

namespace {
void eqs(Settings& s, std::initializer_list<double> e) { int k = 0; for (double x : e) s.setEq(k++, x); }
void cab(Settings& s, const char* a, const char* b, double blend) {
    s.set("cabOn", true); s.set("cabModel", CAB_LIB); s.set("irA", a); s.set("irB", b); s.set("irBlend", blend);
}
} // namespace

const std::vector<Factory> FACTORY = {
    { "Tight Rhythm (Gojira-ish)", [](Settings& s) {
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.86); s.set("presence", 0.72); s.set("treble", 0.54); s.set("mid", 0.24); s.set("eqOn", true);
        eqs(s, { -0.5, 1, -0.5, -0.5, 1.5, 1.5, -2.5, 0.5 });
        s.set("ampVolume", 0.483);   // level-matched
    } },
    { "Sludge Crunch (Mastodon-ish)", [](Settings& s) {
        s.set("odOn", false); s.set("ampChannel", 1); s.set("ampGain", 0.8); s.set("bass", 0.65); s.set("mid", 0.6); s.set("treble", 0.5);
        s.set("presence", 0.45); s.set("depth", 0.65); s.set("ampVolume", 0.29);   // level-matched (was clipping)
        double e[] = { 1, 1, 0, 0, 0, 0, -1, -3 }; for (int k = 0; k < 8; ++k) s.setEq(k, e[k]);
        s.set("irA", "SM57/NO-TS/57_2_inch_cone_far_pres_2.wav"); s.set("irBlend", 0.5);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.26); s.set("presence", 0.75); s.set("treble", 0.9); s.set("mid", 0.56); s.set("eqOn", true);
        eqs(s, { 6, 0, -3, -3, 2, 4.5, -2.5, -6 });
        s.set("ampVolume", 0.418);   // level-matched
    } },
    { "Screaming Lead", [](Settings& s) {
        s.set("gateThresh", -64.0); s.set("odDrive", 0.2); s.set("ampGain", 0.7); s.set("mid", 0.65); s.set("treble", 0.5); s.set("ampVolume", 0.44);
        double e[] = { -2, -1, 0, 1, 2, 1, -1, -4 }; for (int k = 0; k < 8; ++k) s.setEq(k, e[k]);
        s.set("irA", "SM57/TS/57_TS_1_inch_edge_pres_3.wav"); s.set("delayOn", true); s.set("delayTime", 450.0); s.set("delayMix", 0.22);
        s.set("revOn", true); s.set("revMix", 0.15);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.93); s.set("presence", 0.9); s.set("treble", 0.53); s.set("mid", 0.1); s.set("eqOn", true);
        eqs(s, { -6, -0.5, -1.5, -0.5, 2, 2, -2.5, 3.5 });
        s.set("ampVolume", 0.491);   // level-matched
    } },
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
        s.set("delayOn", false); s.set("revOn", false);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.78); s.set("presence", 0.12); s.set("treble", 0.49); s.set("mid", 0.46); s.set("eqOn", true);
        eqs(s, { 2, 0, -3, -4.5, -4, -3, 0.5, -2.5 });
        s.set("ampVolume", 0.686);   // level-matched
    } },
    { "Ambient Clean", [](Settings& s) {
        s.set("gateThresh", -72.0); s.set("odOn", false); s.set("ampChannel", 0); s.set("ampGain", 0.4); s.set("bass", 0.5); s.set("mid", 0.5);
        s.set("treble", 0.6); s.set("ampVolume", 0.44);
        double e[] = { 0, 0, 0, 0, 0, 1, 1, 0 }; for (int k = 0; k < 8; ++k) s.setEq(k, e[k]);
        s.set("cabModel", 2); s.set("cabHighCut", 10000.0);
        s.set("delayOn", true); s.set("delayTime", 520.0); s.set("delayFeedback", 0.42); s.set("delayMix", 0.3);
        s.set("revOn", true); s.set("revSize", 0.8); s.set("revMix", 0.35);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.1); s.set("presence", 0.9); s.set("treble", 0.47); s.set("mid", 0.36); s.set("eqOn", true);
        eqs(s, { -6, -6, -6, -6, 1.5, -4.5, 3, 6 });
        cab(s, "C414/NO-TS/C414_1_inch_cone_near_pres_3.wav", "SM57/NO-TS/57_1_inch_cone_near_pres_3.wav", 0.4); s.set("cabHighCut", 12000);
        s.set("ampVolume", 0.551);   // level-matched
    } },

    // Band-inspired tones: the well-known rigs behind each sound (amp type, boost, EQ shape, mics), level-matched.
    // Mark IIC+ style: no boost, deep V on the graphic EQ, 57 on the cap with a far 421.
    { "Thrash Rhythm (Metallica-ish)", [](Settings& s) {
        s.set("gateThresh", -50.0); s.set("gateRelease", 80.0); s.setChain({});
        s.set("ampChannel", 2); s.set("ampGain", 0.7);
        s.set("bass", 0.65); s.set("mid", 0.3); s.set("treble", 0.65); s.set("presence", 0.6); s.set("depth", 0.5); s.set("ampVolume", 0.47);
        eqs(s, { 2, 2, 0, -3, -5, -2, 1, 0 });
        cab(s, "SM57/NO-TS/57_1_inch_cap_pres_3.wav", "MD421/NO-TS/MD421_1_inch_cone_far_pres_3.wav", 0.3);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.24); s.set("presence", 0.46); s.set("treble", 0.9); s.set("mid", 0.63); s.set("eqOn", true);
        eqs(s, { 6, -1.5, -4.5, -5, -2.5, 0, -1, -5 });
        s.set("ampVolume", 0.6);   // level-matched
    } },
    // Hot-rodded Marshall with a clean boost: mids stay in, bright and biting.
    { "Thrash Attack (Slayer-ish)", [](Settings& s) {
        s.set("gateThresh", -55.0); s.set("gateRelease", 70.0);
        s.setChain({ (int) PedalKind::od }); s.set("odOn", true); s.set("odDrive", 0.1); s.set("odTone", 0.6); s.set("odLevel", 0.85);
        s.set("ampChannel", 2); s.set("ampGain", 0.6);
        s.set("bass", 0.5); s.set("mid", 0.6); s.set("treble", 0.7); s.set("presence", 0.7); s.set("depth", 0.4); s.set("ampVolume", 0.5);
        eqs(s, { 0, 0, 0, 1, 2, 2, 0, -2 });
        cab(s, "SM57/NO-TS/57_1_inch_edge_pres_4.wav", "", 0.0);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.95); s.set("presence", 0.57); s.set("treble", 0.6); s.set("mid", 0.27); s.set("eqOn", true);
        eqs(s, { -2.5, 1, 0.5, 0, 1.5, 2.5, 0, -0.5 });
        s.set("ampVolume", 0.494);   // level-matched
    } },
    // Solid-state crunch scooped hard, big low end, very tight gate.
    { "Groove Scoop (Pantera-ish)", [](Settings& s) {
        s.set("gateThresh", -48.0); s.set("gateRelease", 50.0); s.setChain({});
        s.set("ampChannel", 2); s.set("ampGain", 0.75);
        s.set("bass", 0.7); s.set("mid", 0.15); s.set("treble", 0.75); s.set("presence", 0.7); s.set("depth", 0.6); s.set("ampVolume", 0.59);
        eqs(s, { 3, 2, -2, -6, -6, -2, 2, 1 });
        cab(s, "SM57/NO-TS/57_1_inch_cap_pres_5.wav", "SM7B/NO-TS/SM7B_1_inch_cap_pres_4.wav", 0.2);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.38); s.set("presence", 0.1); s.set("treble", 0.87); s.set("mid", 0.87); s.set("eqOn", true);
        eqs(s, { 6, -1, -3, -3.5, -3, -2.5, 0.5, -6 });
        s.set("ampVolume", 0.726);   // level-matched
    } },
    // Modded JCM800 pushed by a drive pedal: tighter and more mid-forward than the Mark-style scoop.
    { "Speed Metal (Megadeth-ish)", [](Settings& s) {
        s.set("gateThresh", -55.0); s.set("gateRelease", 70.0);
        s.setChain({ (int) PedalKind::od }); s.set("odOn", true); s.set("odDrive", 0.3); s.set("odTone", 0.65); s.set("odLevel", 0.6);
        s.set("ampChannel", 2); s.set("ampGain", 0.6);
        s.set("bass", 0.5); s.set("mid", 0.5); s.set("treble", 0.7); s.set("presence", 0.65); s.set("depth", 0.45); s.set("ampVolume", 0.51);
        eqs(s, { 0, 0, -1, 0, 1, 2, 0, -2 });
        cab(s, "SM57/NO-TS/57_1_inch_edge_pres_4.wav", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav", 0.25);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.89); s.set("presence", 0.55); s.set("treble", 0.53); s.set("mid", 0.28); s.set("eqOn", true);
        eqs(s, { -5, 1, 0, -0.5, 1, 2, 0, 1.5 });
        s.set("ampVolume", 0.496);   // level-matched
    } },
    // Treble booster into a cranked British head: thick mids, loose gate, a little room.
    { "Doom Riff (Sabbath-ish)", [](Settings& s) {
        s.set("gateThresh", -66.0); s.set("gateRelease", 150.0);
        s.setChain({ (int) PedalKind::od }); s.set("odOn", true); s.set("odDrive", 0.35); s.set("odTone", 0.75); s.set("odLevel", 0.7);
        s.set("ampChannel", 1); s.set("ampGain", 0.8);
        s.set("bass", 0.6); s.set("mid", 0.65); s.set("treble", 0.5); s.set("presence", 0.45); s.set("depth", 0.65); s.set("ampVolume", 0.35);
        eqs(s, { 1, 1, 1, 1, 0, 0, -1, -3 });
        cab(s, "MD421/NO-TS/MD421_1_inch_cone_near_pres_2.wav", "C414/NO-TS/C414_2_foot_pres_2.wav", 0.3);
        s.set("revOn", true); s.set("revSize", 0.4); s.set("revMix", 0.1);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.81); s.set("presence", 0.52); s.set("treble", 0.9); s.set("mid", 0.39); s.set("eqOn", true);
        eqs(s, { 2.5, 1, -1, -4.5, 4.5, 4, -5.5, -0.5 });
        s.set("ampVolume", 0.397);   // level-matched
    } },
    // Marshall crunch, mid-forward for twin harmonies, with lead delay and reverb.
    { "Twin Lead (Maiden-ish)", [](Settings& s) {
        s.set("gateThresh", -66.0); s.set("gateRelease", 120.0); s.setChain({});
        s.set("ampChannel", 1); s.set("ampGain", 0.55);
        s.set("bass", 0.45); s.set("mid", 0.7); s.set("treble", 0.6); s.set("presence", 0.55); s.set("depth", 0.4); s.set("ampVolume", 0.43);
        eqs(s, { -1, 0, 1, 2, 2, 1, 0, -2 });
        cab(s, "SM57/NO-TS/57_1_inch_edge_pres_3.wav", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav", 0.35);
        s.set("delayOn", true); s.set("delayTime", 380.0); s.set("delayFeedback", 0.25); s.set("delayMix", 0.15);
        s.set("revOn", true); s.set("revSize", 0.5); s.set("revMix", 0.12);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.42); s.set("presence", 0.73); s.set("treble", 0.85); s.set("mid", 0.64); s.set("eqOn", true);
        eqs(s, { 5.5, -1, -2, 0.5, 2.5, 2, -2.5, -2 });
        s.set("ampVolume", 0.47);   // level-matched
    } },
    // Clean boost into high gain for an ultra-tight low end, hard gate, clanky upper mids.
    { "Djent Clank (Meshuggah-ish)", [](Settings& s) {
        s.set("gateThresh", -45.0); s.set("gateRelease", 35.0);
        s.setChain({ (int) PedalKind::od }); s.set("odOn", true); s.set("odDrive", 0.0); s.set("odTone", 0.6); s.set("odLevel", 0.9);
        s.set("ampChannel", 2); s.set("ampGain", 0.6);
        s.set("bass", 0.45); s.set("mid", 0.45); s.set("treble", 0.65); s.set("presence", 0.75); s.set("depth", 0.7); s.set("ampVolume", 0.5);
        eqs(s, { -2, -1, -3, -1, 1, 3, 1, -2 });
        cab(s, "SM57/NO-TS/57_1_inch_cap_pres_4.wav", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav", 0.3);
        s.set("cabLowCut", 110.0); s.set("cabHighCut", 8000.0);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.95); s.set("presence", 0.52); s.set("treble", 0.42); s.set("mid", 0.31); s.set("eqOn", true);
        eqs(s, { -2, -1, -2.5, -1.5, -1, 1.5, 0, 2.5 });
        s.set("ampVolume", 0.572);   // level-matched
    } },
    // HM-2 style "all knobs up": fuzz into a crunchy amp with the lows and upper mids pushed hard.
    { "Chainsaw Death (Entombed-ish)", [](Settings& s) {
        s.set("gateThresh", -55.0); s.set("gateRelease", 80.0);
        s.setChain({ (int) PedalKind::fuzz }); s.set("fuzzOn", true); s.set("fuzzAmt", 0.7); s.set("fuzzTone", 0.7); s.set("fuzzLevel", 0.55);
        s.set("odOn", false);
        s.set("ampChannel", 1); s.set("ampGain", 0.4);
        s.set("bass", 0.55); s.set("mid", 0.6); s.set("treble", 0.6); s.set("presence", 0.55); s.set("depth", 0.5); s.set("ampVolume", 0.35);
        eqs(s, { 5, 3, 0, 0, 6, 6, 3, -2 });
        cab(s, "SM57/NO-TS/57_1_inch_edge_pres_3.wav", "", 0.0);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.6); s.set("presence", 0.9); s.set("treble", 0.1); s.set("mid", 0.51); s.set("eqOn", true);
        eqs(s, { 6, -0.5, -3, 3.5, -0.5, 2.5, 2, 6 });
        s.set("ampVolume", 0.47);   // level-matched
    } },
    // Compressed and wide: thick low end, a drive pedal for density, gated to silence between hits.
    { "Industrial Wall (Rammstein-ish)", [](Settings& s) {
        s.set("gateThresh", -48.0); s.set("gateRelease", 45.0);
        s.setChain({ (int) PedalKind::comp, (int) PedalKind::od });
        s.set("compOn", true); s.set("compSustain", 0.4); s.set("compAttack", 5.0); s.set("compLevel", 0.0); s.set("compBlend", 0.7);
        s.set("odOn", true); s.set("odDrive", 0.2); s.set("odTone", 0.5); s.set("odLevel", 0.6);
        s.set("ampChannel", 2); s.set("ampGain", 0.7);
        s.set("bass", 0.65); s.set("mid", 0.4); s.set("treble", 0.55); s.set("presence", 0.5); s.set("depth", 0.75); s.set("ampVolume", 0.49);
        eqs(s, { 2, 2, -1, -2, 0, 1, 0, -3 });
        cab(s, "SM7B/NO-TS/SM7B_1_inch_cap_pres_3.wav", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav", 0.35);
        s.set("ampVolume", 0.511);   // level-matched
    } },
    // Clean channel with light compression, chorus, delay and a big room: the quiet parts of prog metal.
    { "Prog Clean (Opeth-ish)", [](Settings& s) {
        s.set("gateThresh", -72.0); s.set("gateRelease", 120.0);
        s.setChain({ (int) PedalKind::comp, (int) PedalKind::chorus });
        s.set("compOn", true); s.set("compSustain", 0.35); s.set("compAttack", 12.0); s.set("compBlend", 0.6);
        s.set("chorusOn", true); s.set("chorusRate", 0.6); s.set("chorusDepth", 0.4); s.set("chorusMix", 0.35);
        s.set("odOn", false);
        s.set("ampChannel", 0); s.set("ampGain", 0.35);
        s.set("bass", 0.5); s.set("mid", 0.5); s.set("treble", 0.6); s.set("presence", 0.5); s.set("depth", 0.5); s.set("ampVolume", 0.33);
        eqs(s, { 0, 0, 0, 0, 0, 1, 1, 0 });
        cab(s, "U87/NO-TS/U87_grill_cone_far_pres_3.wav", "", 0.0);
        s.set("delayOn", true); s.set("delayTime", 450.0); s.set("delayFeedback", 0.3); s.set("delayMix", 0.2);
        s.set("revOn", true); s.set("revSize", 0.6); s.set("revMix", 0.25);
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.set("depth", 0.1); s.set("presence", 0.9); s.set("treble", 0.39); s.set("mid", 0.28); s.set("eqOn", true);
        eqs(s, { -6, -6, -6, -6, 2, -4.5, 4, 6 });
        cab(s, "C414/NO-TS/C414_2_inch_cone_near_pres_3.wav", "SM57/NO-TS/57_2_inch_edge_pres_3.wav", 0.35); s.set("cabHighCut", 12000);
        s.set("ampVolume", 0.698);   // level-matched
    } },

    // NAM captures of real amps (bundled; see shared/nam/captures/CREDITS.md). Without them the built-in amp plays.
    // Peavey 5150 with a boost: the modern metal rhythm sound.
    { "Modern Metal - 5150 (NAM)", [](Settings& s) {
        s.set("nam", "Helga B 5150 BlockLetter - Boosted"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 2);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 90); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -52);
        s.set("gateRelease", 70); s.setChain({}); s.set("delayOn", false); s.set("revOn", false);
        s.set("namInput", 0); s.set("namOutput", 2.0);
    } },
    // Unboosted 5150 hit by a clean boost, very tight lows and gate.
    { "Djent - 5150 + Boost (NAM)", [](Settings& s) {
        s.set("nam", "Helga B 5150 BlockLetter - NoBoost"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 2);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 110); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -45);
        s.set("gateRelease", 35); s.setChain({ (int) PedalKind::od }); s.set("delayOn", false); s.set("revOn", false);
        s.set("namInput", 0); s.set("namOutput", 4.6); s.set("odOn", true); s.set("odDrive", 0);
        s.set("odTone", 0.6); s.set("odLevel", 0.9);
    } },
    // 6505+ red channel with an MXR drive in front.
    { "Groove Chug - 6505+ (NAM)", [](Settings& s) {
        s.set("nam", "Helga B 6505+ Red ch - MXR Drive"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 2);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 90); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -48);
        s.set("gateRelease", 60); s.setChain({}); s.set("delayOn", false); s.set("revOn", false);
        s.set("namInput", 0); s.set("namOutput", 1.3);
    } },
    // Mesa Mark IV lead channel for tight thrash rhythm.
    { "Thrash Rhythm - Mark IV (NAM)", [](Settings& s) {
        s.set("nam", "Roman A LT_MESA_MARKIV_1"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 2);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 90); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -50);
        s.set("gateRelease", 80); s.setChain({}); s.set("delayOn", false); s.set("revOn", false);
        s.set("namInput", 0); s.set("namOutput", 1.1);
    } },
    // Hot-rodded Marshall (Splawn Pro Mod, 3rd gear) with a clean boost.
    { "Thrash Attack - Splawn (NAM)", [](Settings& s) {
        s.set("nam", "Tim R Splawn Pro Mod 3rd Gear - G12"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 2);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 90); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -55);
        s.set("gateRelease", 70); s.setChain({ (int) PedalKind::od }); s.set("delayOn", false); s.set("revOn", false);
        s.set("namInput", 0); s.set("namOutput", 3.1); s.set("odOn", true); s.set("odDrive", 0.05);
        s.set("odTone", 0.6); s.set("odLevel", 0.8);
    } },
    // Marshall JVM crunch: classic rock and twin-guitar metal rhythm.
    { "Crunch - JVM (NAM)", [](Settings& s) {
        s.set("nam", "Phillipe P JVM-CR-OR-NoDrive-ESR0,017"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 1);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 80); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -66);
        s.set("gateRelease", 120); s.setChain({}); s.set("delayOn", false); s.set("revOn", false);
        s.set("namInput", 0); s.set("namOutput", -1.6);
    } },
    // Marshall JVM OD2 with lead delay and reverb.
    { "British Lead - JVM (NAM)", [](Settings& s) {
        s.set("nam", "Phillipe P JVM-OD2-RD-NoDrive-ESR0,003"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 2);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 90); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -64);
        s.set("gateRelease", 150); s.setChain({}); s.set("delayOn", true); s.set("revOn", true);
        s.set("namInput", 0); s.set("namOutput", 2.7); s.set("delayTime", 380); s.set("delayFeedback", 0.25);
        s.set("delayMix", 0.15); s.set("revSize", 0.5); s.set("revMix", 0.12);
    } },
    // Laney GH100S: thick British mids for doom and stoner riffs.
    { "Doom - Laney (NAM)", [](Settings& s) {
        s.set("nam", "Phillipe P LaneyGH100S-Hi-MidGain-ESR0,003"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 1);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 80); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -66);
        s.set("gateRelease", 150); s.setChain({}); s.set("delayOn", false); s.set("revOn", true);
        s.set("namInput", 0); s.set("namOutput", -0.9); s.set("revSize", 0.4); s.set("revMix", 0.1);
    } },
    // ENGL with a DS-1: compressed, wide and gated hard.
    { "Industrial - ENGL (NAM)", [](Settings& s) {
        s.set("nam", "Tom C Engl e530 preamp & 840 power amp with Boss DS1 Overdrive"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 2);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 90); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -48);
        s.set("gateRelease", 45); s.setChain({ (int) PedalKind::comp }); s.set("delayOn", false); s.set("revOn", false);
        s.set("namInput", 0); s.set("namOutput", 4.1); s.set("compOn", true); s.set("compSustain", 0.4);
        s.set("compAttack", 5); s.set("compLevel", 0); s.set("compBlend", 0.7);
    } },
    // Soldano SLO-style drive with delay and reverb for leads.
    { "Hot Lead - Soldano (NAM)", [](Settings& s) {
        s.set("nam", "Jason Z Soldano - Super Lead Overdrive pedal nice crunchy rhythm tone"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 2);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "SM57/NO-TS/57_1_inch_edge_pres_3.wav"); s.set("irB", "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.35);
        s.set("cabLowCut", 90); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -64);
        s.set("gateRelease", 150); s.setChain({}); s.set("delayOn", true); s.set("revOn", true);
        s.set("namInput", 0); s.set("namOutput", 1.3); s.set("delayTime", 450); s.set("delayFeedback", 0.3);
        s.set("delayMix", 0.2); s.set("revSize", 0.55); s.set("revMix", 0.15);
    } },
    // Fender Twin Reverb with light compression, chorus, delay and reverb.
    { "Clean - Twin (NAM)", [](Settings& s) {
        s.set("nam", "Tim R Fender TwinVerb Norm Bright"); s.set("capture", juce::String()); s.set("ampOn", true); s.set("ampChannel", 0);
        s.set("ampGain", 0.6); s.set("eqOn", true); eqs(s, { 0, 0, 0, 0, 0, 0, 0, 0 }); s.set("cabOn", true);
        s.set("cabModel", 4); s.set("irA", "C414/NO-TS/C414_1_inch_cone_near_pres_3.wav"); s.set("irB", "SM57/NO-TS/57_1_inch_cone_near_pres_3.wav"); s.set("irBlend", 0.4);
        s.set("cabLowCut", 80); s.set("cabHighCut", 10000); s.set("gateOn", true); s.set("gateThresh", -72);
        s.set("gateRelease", 120); s.setChain({ (int) PedalKind::comp, (int) PedalKind::chorus }); s.set("delayOn", true); s.set("revOn", true);
        s.set("namInput", 0); s.set("namOutput", -10.7); s.set("compOn", true); s.set("compSustain", 0.35);
        s.set("compAttack", 12); s.set("compBlend", 0.6); s.set("chorusOn", true); s.set("chorusRate", 0.6);
        s.set("chorusDepth", 0.4); s.set("chorusMix", 0.35); s.set("delayTime", 450); s.set("delayFeedback", 0.3);
        s.set("delayMix", 0.18); s.set("revSize", 0.6); s.set("revMix", 0.22);
    } },
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

juce::String tuningNotes(const std::array<int, 6>& strings, bool flats) {
    juce::StringArray a; for (int m : strings) a.add(noteName(m, flats));
    return a.joinIntoString(" ");
}

juce::String tuningLabel(const Tuning& t) { return juce::String(t.name) + juce::String::fromUTF8(" \xc2\xb7 ") + tuningNotes(t.strings, t.flats); }

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
