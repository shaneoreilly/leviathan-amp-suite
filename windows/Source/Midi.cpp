#include "Midi.h"
#include "Model.h"

namespace lev {

Midi::Midi(Model& model) : m(model) {
    auto& p = m.props();
    programChange = p.getBoolValue("midiPC", true);
    followClock = p.getBoolValue("midiClock", true);
    if (auto* a = juce::JSON::parse(p.getValue("midiMaps")).getArray()) {
        for (const auto& v : *a) {
            MidiMap mp;
            if (v["id"].isString()) mp.id = v["id"].toString();
            mp.channel = juce::jlimit(0, 15, (int) v["channel"]); mp.cc = juce::jlimit(0, 127, (int) v["cc"]);
            mp.key = v["key"].toString(); mp.name = v["name"].toString();
            mp.lo = (double) v["lo"]; mp.hi = (double) v["hi"];
            mp.toggle = (bool) v["toggle"]; mp.momentary = (bool) v["momentary"];
            if (mp.key.isNotEmpty()) maps.push_back(mp);
        }
    }
}

Midi::~Midi() {
    alive->store(false);
    for (auto& in : inputs) in->stop();
}

void Midi::start() {
    deviceWatch = juce::MidiDeviceListConnection::make([this] { connectAll(); });
    connectAll();
}

void Midi::save() {
    juce::Array<juce::var> a;
    for (const auto& mp : maps) {
        auto* o = new juce::DynamicObject();
        o->setProperty("id", mp.id); o->setProperty("channel", mp.channel); o->setProperty("cc", mp.cc);
        o->setProperty("key", mp.key); o->setProperty("name", mp.name); o->setProperty("lo", mp.lo); o->setProperty("hi", mp.hi);
        o->setProperty("toggle", mp.toggle); o->setProperty("momentary", mp.momentary);
        a.add(juce::var(o));
    }
    auto& p = m.props();
    p.setValue("midiMaps", juce::JSON::toString(a, juce::JSON::FormatOptions{}.withSpacing(juce::JSON::Spacing::none)));
    p.setValue("midiPC", programChange); p.setValue("midiClock", followClock);
    p.saveIfNeeded();
    m.changed();
}

/// Listen to every MIDI input (rerun when devices come and go).
void Midi::connectAll() {
    for (auto& in : inputs) in->stop();
    inputs.clear();
    sources.clear();
    for (const auto& d : juce::MidiInput::getAvailableDevices()) {
        if (auto in = juce::MidiInput::openDevice(d.identifier, this)) {
            in->start();
            sources.add(d.name);
            inputs.push_back(std::move(in));
        }
    }
    m.changed();
}

void Midi::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg) {
    // MIDI thread: clock is measured here (timing matters), everything else goes to the message thread
    if (msg.isMidiClock() || msg.isMidiStart() || msg.isMidiStop()) {
        if (auto bpm = clockMessage(msg)) {
            auto aliveFlag = alive;
            juce::MessageManager::callAsync([this, aliveFlag, b = *bpm] {
                if (aliveFlag->load() && followClock && std::abs(m.bpm - b) >= 0.3) m.setBpm(b);
            });
        }
        return;
    }
    if (!msg.isController() && !msg.isProgramChange()) return;
    auto aliveFlag = alive;
    juce::MessageManager::callAsync([this, aliveFlag, msg] { if (aliveFlag->load()) handle(msg); });
}

/// MIDI clock (24 ticks per beat) to a tempo, smoothed.
std::optional<double> Midi::clockMessage(const juce::MidiMessage& msg) {
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    if (msg.isMidiStart() || msg.isMidiStop()) { ticks = 0; beatStart = 0; smooth = 0; return std::nullopt; }
    if (lastTick != 0 && now - lastTick > 1) { ticks = 0; beatStart = 0; smooth = 0; }   // the clock stopped and restarted
    lastTick = now;
    if (beatStart == 0) { beatStart = now; ticks = 0; return std::nullopt; }
    if (++ticks != 24) return std::nullopt;
    const double secs = now - beatStart;
    beatStart = now; ticks = 0;
    if (secs <= 0.15 || secs >= 3) return std::nullopt;
    const double b = 60 / secs;
    smooth = smooth == 0 ? b : smooth * 0.6 + b * 0.4;
    return std::round(smooth * 10) / 10;
}

void Midi::handle(const juce::MidiMessage& msg) {
    const int ch = msg.getChannel() - 1;
    if (msg.isController()) {
        last = "CC " + juce::String(msg.getControllerNumber()) + " = " + juce::String(msg.getControllerValue()) + juce::String::fromUTF8(" \xc2\xb7 ch ") + juce::String(ch + 1);
        controlChange(ch, msg.getControllerNumber(), msg.getControllerValue());
    } else if (msg.isProgramChange()) {
        const int pc = msg.getProgramChangeNumber();
        last = "Program " + juce::String(pc + 1) + juce::String::fromUTF8(" \xc2\xb7 ch ") + juce::String(ch + 1);
        if (programChange && !learning) {
            const auto order = m.presetOrder();
            if (pc < order.size()) m.loadAny(order[pc]);
        }
    }
    m.changed();
}

void Midi::controlChange(int ch, int cc, int v) {
    if (learning && armed) {
        const auto a = *armed;
        maps.erase(std::remove_if(maps.begin(), maps.end(), [&](const MidiMap& x) { return x.key == a.key; }), maps.end());
        MidiMap mp; mp.channel = ch; mp.cc = cc; mp.key = a.key; mp.name = a.name; mp.lo = a.lo; mp.hi = a.hi; mp.toggle = a.toggle;
        maps.push_back(mp);
        justLearned = std::make_pair(mp.id, juce::Time::getMillisecondCounterHiRes());
        armed.reset();
        save();
        return;
    }
    for (auto& mp : maps) {
        if (mp.channel != ch || mp.cc != cc) continue;
        if (mp.toggle) {
            // a release arriving right after the learn press means the switch is momentary
            if (!mp.momentary && v < 64 && justLearned && justLearned->first == mp.id
                && juce::Time::getMillisecondCounterHiRes() - justLearned->second < 1500) {
                mp.momentary = true; justLearned.reset(); save(); continue;
            }
            if (mp.key.startsWith("@")) { if (!mp.momentary || v >= 64) m.midiAction(mp.key); }
            else if (mp.momentary) { if (v >= 64) m.set(mp.key, !m.s().b(mp.key)); }
            else m.set(mp.key, v >= 64);
        } else {
            m.set(mp.key, mp.lo + (mp.hi - mp.lo) * v / 127.0);
        }
    }
}

void Midi::setLearning(bool on) { learning = on; if (!on) armed.reset(); m.changed(); }
void Midi::arm(const MidiArmed& a) { if (armed && *armed == a) armed.reset(); else armed = a; m.changed(); }
const MidiMap* Midi::mapFor(const juce::String& key) const { for (const auto& mp : maps) if (mp.key == key) return &mp; return nullptr; }
void Midi::forget(const juce::String& key) { maps.erase(std::remove_if(maps.begin(), maps.end(), [&](auto& x) { return x.key == key; }), maps.end()); save(); }
void Midi::removeMap(const juce::String& id) { maps.erase(std::remove_if(maps.begin(), maps.end(), [&](auto& x) { return x.id == id; }), maps.end()); save(); }
void Midi::clear() { maps.clear(); save(); }
void Midi::setMomentary(const juce::String& id, bool on) { for (auto& mp : maps) if (mp.id == id) mp.momentary = on; save(); }
void Midi::setProgramChange(bool on) { programChange = on; save(); }
void Midi::setFollowClock(bool on) { followClock = on; save(); }

} // namespace lev
