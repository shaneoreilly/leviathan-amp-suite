#include "Rig.h"
#include "CabBrowser.h"
#include "AmpBrowser.h"

namespace lev {

namespace {
using Fmt = Knob::Fmt;
const Fmt pct = [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; };
const Fmt dbSigned = [](double v) { return juce::String::formatted("%+.1f dB", v); };
const Fmt hz = [](double v) { return juce::String::formatted("%.2f Hz", v); };
Fmt intUnit(const char* unit) { return [unit](double v) { return juce::String(juce::roundToInt(v)) + " " + unit; }; }
const juce::String dot = juce::String::fromUTF8(" \xc2\xb7 ");

std::unique_ptr<Stack> row(int gap = 2) { return std::make_unique<Stack>(false, gap); }
std::unique_ptr<Stack> col(int gap = 8, juce::Justification j = juce::Justification::centred) { return std::make_unique<Stack>(true, gap, j); }

/// Holds one component and reports at least `min` as its size: top left aligned, or centred across when `centre`.
/// Keeps a panel whose contents change (the cab type) the same size, so the panels around it don't move.
struct MinSize : juce::Component {
    MinSize(std::unique_ptr<juce::Component> c, juce::Point<int> min, bool centre) : child(std::move(c)), centred(centre) {
        addAndMakeVisible(*child);
        const auto p = pref(*child);
        setPref(*this, std::max(p.x, min.x), std::max(p.y, min.y));
    }
    void resized() override {
        const auto p = pref(*child);
        child->setBounds(centred ? (getWidth() - p.x) / 2 : 0, 0, p.x, p.y);
    }
    std::unique_ptr<juce::Component> child; bool centred;
};

struct GateDot : juce::Component {
    explicit GateDot(Model& m) : model(m) { setPref(*this, 10, 10); }
    void paint(juce::Graphics& g) override {
        const bool open = model.params.gateOpen.load() && model.running;
        g.setColour(open ? juce::Colour(0xff4cd964) : juce::Colour(0xff444444));
        g.fillEllipse(getLocalBounds().toFloat().withSizeKeepingCentre(7, 7));
    }
    Model& model;
};

/// A segmented choice (CLEAN / RUST / HOT, Pedal / Auto).
struct Segments : juce::Component {
    Segments(Model& m, juce::StringArray l, std::function<int()> get, std::function<void(int)> set, std::function<juce::Colour(int)> colourFor, int w, int h, float fontPt)
        : model(m), labels(std::move(l)), getter(std::move(get)), setter(std::move(set)), colourOf(std::move(colourFor)), pt(fontPt) { setPref(*this, w, h); }
    void paint(juce::Graphics& g) override {
        const int n = labels.size(); const float gap = 6, w = ((float) getWidth() - gap * (float) (n - 1)) / (float) n;
        for (int i = 0; i < n; ++i) {
            auto r = juce::Rectangle<float>((float) i * (w + gap), 0, w, (float) getHeight());
            const bool on = getter() == i;
            g.setColour(on ? colourOf(i) : theme::panel2); g.fillRoundedRectangle(r, 7);
            g.setColour(on ? juce::Colours::black : theme::secondary); g.setFont(theme::font(pt, true, pt > 10 ? 2.f : 0.5f));
            g.drawText(labels[i], r, juce::Justification::centred);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override {
        const int n = labels.size(); const int i = juce::jlimit(0, n - 1, e.x * n / std::max(getWidth(), 1));
        setter(i);
    }
    Model& model; juce::StringArray labels; std::function<int()> getter; std::function<void(int)> setter; std::function<juce::Colour(int)> colourOf; float pt;
};

namespace {
/// "Open folder" for a library folder, or no action on Android (nothing there can show a folder).
std::function<void()> openFolder(const juce::File& dir) {
    if (!canRevealFiles) return {};
    return [dir] { dir.createDirectory(); dir.startAsProcess(); };
}

/// A small "..." button that opens a menu of actions.
std::unique_ptr<juce::TextButton> moreButton(std::vector<std::pair<juce::String, std::function<void()>>> items, const juce::String& tip) {
    auto b = std::make_unique<juce::TextButton>(juce::String::fromUTF8("\xe2\x8b\xaf"));
    auto* bp = b.get();
    b->onClick = [bp, items] {
        juce::PopupMenu menu;
        for (auto& [t, fn] : items) if (fn) menu.addItem(t, fn);   // no action: not on this platform
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(bp));
    };
    setPref(*b, 30, 26);
    b->setTooltip(tip);
    return b;
}
} // namespace

std::unique_ptr<juce::TextButton> textButton(const juce::String& t, std::function<void()> fn) {
    auto b = std::make_unique<juce::TextButton>(t);
    b->onClick = std::move(fn);
    setPref(*b, juce::GlyphArrangement::getStringWidthInt(theme::font(10), t) + 24, 26);
    return b;
}
} // namespace

namespace {
} // namespace

RigView::RigView(Model& m) : model(m) {
    helpStrip = std::make_unique<HelpStrip>(m, onAndroid ? juce::String::fromUTF8("Touch any control for an explanation. Drag a dial up or down to change it; \xe2\x86\xba resets it.")
                                                        : juce::String::fromUTF8("Hover over any control for an explanation. Scroll or drag a dial to change it; \xe2\x86\xba resets it."));
    addChildComponent(*helpStrip);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&canvas, false);
    viewport.setScrollBarsShown(true, false);
    viewport.moved = [this] { if (onScroll) onScroll(); };
    startTimerHz(15);
    refresh();
}

void RigView::resized() {
    auto r = getLocalBounds();
    helpStrip->setVisible(model.explainOnHover);
    if (model.explainOnHover) helpStrip->setBounds(r.removeFromBottom(44));
    viewport.setBounds(r);
    layout();
}

void RigView::timerCallback() {
    for (auto* c : live) c->repaint();
    static juce::String shown = "-";
    if (helpStrip->isVisible() && shown != model.hoverId) { shown = model.hoverId; helpStrip->repaint(); }
}

juce::String RigView::currentSignature() const {
    const auto& s = model.s();
    juce::StringArray sig;
    auto chain = s.chain(); std::sort(chain.begin(), chain.end());
    for (int k : chain) sig.add("p" + juce::String(k));
    sig.add(model.collapsed.joinIntoString("|"));
    sig.add(model.explainOnHover ? "help" : "nohelp"); sig.add(model.panelOrder.joinIntoString(","));
    sig.add(model.inputDeviceName() + "/" + juce::String(model.inputChannelCount()));
    sig.add(model.outputDeviceName());
    sig.add(s.s("capture")); sig.add(juce::String((int) model.captures.size()));
    sig.add(s.s("nam")); sig.add(model.namFiles.joinIntoString(",")); sig.add(model.namReady() ? "namok" : "");
    sig.add(s.b("wahAuto") ? "wa" : "wm");
    sig.add(s.i("delaySync") == 0 ? "free" : "sync");
    sig.add(model.tunerOn ? "tuner" : "");
    sig.add(juce::String(model.accent().getARGB()));
    return sig.joinIntoString("/");
}

/// What the cabinet panel's structure depends on: a change here rebuilds that panel alone, so picking a cab or
/// an IR doesn't rebuild (and flash) the whole rig.
juce::String RigView::cabSignature() const {
    const auto& s = model.s();
    juce::StringArray sig;
    sig.add("cab" + juce::String(s.i("cabModel"))); sig.add(model.irLib.isEmpty() ? "nolib" : "lib"); sig.add(model.userIRName);
    sig.add(juce::String(model.cabIRs.all().size()));
    sig.add(s.s("irA")); sig.add(s.s("irB"));
    return sig.joinIntoString("/");
}

void RigView::refresh() {
    const auto sig = currentSignature(), cab = cabSignature();
    if (sig != signature) { signature = sig; cabSig = cab; rebuild(); }
    else if (cab != cabSig) { cabSig = cab; rebuildPanel("cab"); }
    else { syncAll(canvas); layout(); }
}

void RigView::rebuildPanel(const juce::String& id) {
    auto it = panels.find(id);
    if (it == panels.end()) return;
    auto* old = it->second.get();
    live.erase(std::remove_if(live.begin(), live.end(), [old](juce::Component* c) { return old->isParentOf(c); }), live.end());
    canvas.removeChildComponent(old);
    it->second = makePanel(id);
    canvas.addAndMakeVisible(*it->second);
    syncAll(*it->second);
    layout();
}

void RigView::syncAll(juce::Component& c) {
    for (auto* ch : c.getChildren()) {
        if (auto* p = dynamic_cast<Picker*>(ch)) p->sync();
        else if (auto* k = dynamic_cast<Knob*>(ch)) k->sync();
        else if (auto* f = dynamic_cast<Footswitch*>(ch)) f->sync();
        else if (auto* a = dynamic_cast<ActionButton*>(ch)) a->sync();
        else if (auto* o = dynamic_cast<LearnOverlay*>(ch)) o->sync();
        if (auto* l = dynamic_cast<LiveLabel*>(ch)) l->fit();
        ch->repaint();
        syncAll(*ch);
    }
}

void RigView::rebuild() {
    canvas.removeAllChildren();
    live.clear();
    panels.clear(); pedals.clear(); tunerView.reset(); pedalHeader.reset(); emptyPedals.reset(); tip.reset();
    if (model.tunerOn) { tunerView = makeTuner(); canvas.addAndMakeVisible(*tunerView); }
    for (const auto& id : model.panelOrder) {
        if (id == "|") continue;   // row break
        auto p = makePanel(id); canvas.addAndMakeVisible(*p); panels[id] = std::move(p);
    }
    if (helpStrip->isVisible() != model.explainOnHover) resized();

    // pedalboard header: chevron, title, add menu (A-Z)
    struct Header : juce::Component {
        Header(Model& m) : model(m), chevron(IconButton::chevronDown, [this] { model.toggleCollapsed("PEDALS"); }, 11), add("+ Add pedal") {
            addAndMakeVisible(chevron); addAndMakeVisible(add);
            chevron.icon = model.collapsed.contains("PEDALS") ? IconButton::chevronRight : IconButton::chevronDown;
            const auto chain = model.s().chain();
            add.setEnabled((int) chain.size() < Params::maxChain && (int) chain.size() < kPedalCount);
            add.onClick = [this] {
                juce::PopupMenu menu;
                std::vector<int> kinds;
                const auto ch = model.s().chain();
                for (int k = 0; k < kPedalCount; ++k) if (std::find(ch.begin(), ch.end(), k) == ch.end()) kinds.push_back(k);
                std::sort(kinds.begin(), kinds.end(), [](int a, int b) { return juce::String(pedalTitle((PedalKind) a)) < pedalTitle((PedalKind) b); });
                for (int k : kinds) {
                    const auto t = juce::String(pedalTitle((PedalKind) k));
                    menu.addItem(t.substring(0, 1) + t.substring(1).toLowerCase(), [this, k] { model.addPedal((PedalKind) k); });
                }
                menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&add));
            };
        }
        void resized() override { chevron.setBounds(0, 4, 14, 18); add.setBounds(230, 0, 130, 26); }
        void paint(juce::Graphics& g) override {
            g.setColour(theme::secondary); g.setFont(theme::font(10, true, 2));
            g.drawText(juce::String::fromUTF8("PEDALS  \xe2\x86\x92  AMP"), 20, 0, 200, 26, juce::Justification::centredLeft);
        }
        Model& model; IconButton chevron; juce::TextButton add;
    };
    pedalHeader = std::make_unique<Header>(model);
    canvas.addAndMakeVisible(*pedalHeader);
    if (!model.collapsed.contains("PEDALS")) {
        if (model.s().chain().empty()) {
            emptyPedals = std::make_unique<LiveLabel>([] { return juce::String("No pedals. Add one; signal flows left to right into the amp."); }, 12);
            canvas.addAndMakeVisible(*emptyPedals);
        }
        for (int k : model.s().chain()) { auto p = makePedal((PedalKind) k); canvas.addAndMakeVisible(*p); pedals.push_back(std::move(p)); }
    }
    tip = std::make_unique<LiveLabel>([] { return juce::String::fromUTF8("Tip: free cab IRs (God's Cab, Seacow, OwnHammer free packs) sound far more real than the built-in cabs; load them with \xe2\x80\x9cIR\xe2\x80\xa6\xe2\x80\x9d."); }, 10);
    canvas.addAndMakeVisible(*tip);
    syncAll(canvas);
    layout();
}

void RigView::layout() {
    const int w = std::max(viewport.getMaximumVisibleWidth(), 300), padding = 14;
    int y = topInset + padding;
    const juce::Rectangle<int> full(padding, 0, w - 2 * padding, 0);
    if (tunerView) { tunerView->setBounds(full.getX(), y, full.getWidth(), 300); y += 312; }
    std::vector<UnitBox*> items;
    for (const auto& id : model.panelOrder) {
        if (id == "|") items.push_back(nullptr);
        else if (auto it = panels.find(id); it != panels.end()) items.push_back(it->second.get());
    }
    int used = 0;
    layoutGrid(items, { full.getX(), y, full.getWidth(), 0 }, used, 48, 12, true);
    y += used + 12;
    if (pedalHeader) { pedalHeader->setBounds(full.getX(), y, full.getWidth(), 26); y += 32; }
    if (emptyPedals) { emptyPedals->setBounds(full.getX(), y, full.getWidth(), 40); y += 52; }
    // pedals in chain order (the chain can be reordered without a rebuild)
    std::vector<UnitBox*> ped;
    for (int k : model.s().chain()) for (auto& p : pedals) if (p->dragId == "pedal:" + juce::String(k)) ped.push_back(p.get());
    if (!ped.empty()) { layoutGrid(ped, { full.getX(), y, full.getWidth(), 0 }, used); y += used + 12; }
    if (tip) { tip->setBounds(full.getX(), y, full.getWidth(), 20); y += 28; }
    canvas.setSize(w, y + padding);
}

// MARK: - panels

std::unique_ptr<UnitBox> RigView::makePanel(const juce::String& id) {
    auto& m = model;
    const auto& s = m.s();
    auto knob = [&](const juce::String& label, const char* key, double lo, double hi, double def, Fmt f, const char* scope) {
        return std::make_unique<Knob>(m, label, key, lo, hi, def, f, scope);
    };
    std::unique_ptr<UnitBox> box;

    if (id == "input") {
        auto c = col(8);
        c->add(inputPickers());
        auto r = row(); r->add(knob("Gain", "inGain", -24, 24, 0, dbSigned, "INPUT"));
        c->add(std::move(r));
        box = std::make_unique<UnitBox>(m, "INPUT", std::move(c), "", "panel:input");
    } else if (id == "gate") {
        auto r = row();
        r->add(knob("Thresh", "gateThresh", -90, -20, -60, intUnit("dB"), "GATE"));
        r->add(knob("Release", "gateRelease", 10, 2100, 60, [](double v) { return v >= 2050 ? juce::String::fromUTF8("\xe2\x88\x9e") : juce::String(juce::roundToInt(std::min(v, 2000.0))) + " ms"; }, "GATE"));
        r->add(knob("Depth", "gateDepth", -60, 0, -60, [](double v) { return v <= -60 ? juce::String("Full") : juce::String(juce::roundToInt(v)) + " dB"; }, "GATE"));
        box = std::make_unique<UnitBox>(m, "GATE", std::move(r), "gateOn", "panel:gate");
        auto dotC = std::make_unique<GateDot>(m); liveReg(dotC.get()); box->addTrailing(std::move(dotC));
    } else if (id == "pitch") {
        auto c = col(8);
        auto r1 = row(8);
        r1->add(std::make_unique<LiveLabel>([] { return juce::String("Guitar"); }, 12, theme::secondary));
        auto tune = std::make_unique<Picker>((int) (140 * theme::W), [&m] { return m.tuningIndex + 1; }, [&m](int i) { m.setTuningIndex(i - 1); });
        for (size_t i = 0; i < TUNINGS.size(); ++i) tune->addItem(tuningLabel(TUNINGS[i]), (int) i + 1);
        r1->add(std::move(tune));
        c->add(std::move(r1));
        auto r2 = row(4);
        r2->add(std::make_unique<IconButton>(IconButton::minus, [&m] { m.playAs(std::max((int) std::round(m.s().d("pitchSemis")) - 1, -12)); }, 12))->setTooltip("Down a semitone");
        auto playAs = std::make_unique<Picker>((int) (155 * theme::W), [&m] { return 13 - (int) std::round(m.s().d("pitchSemis")); }, [&m](int i) { m.playAs(13 - i); });
        for (auto& [name, semis] : m.playAsOptions()) {
            auto st = m.tuning().strings; for (auto& n : st) n += semis;
            const auto label = name + juce::String::fromUTF8(" \xc2\xb7 ") + tuningNotes(st, m.tuning().flats);
            playAs->addItem(semis == 0 ? label + " (no shift)" : label + juce::String::formatted("  (%+d)", semis), 13 - semis);
        }
        playAs->setTextWhenNothingSelected("Play as");
        r2->add(std::move(playAs));
        r2->add(std::make_unique<IconButton>(IconButton::plus, [&m] { m.playAs(std::min((int) std::round(m.s().d("pitchSemis")) + 1, 12)); }, 12))->setTooltip("Up a semitone");
        c->add(std::move(r2));
        auto r3 = row();
        r3->add(knob("Semis", "pitchSemis", -12, 12, 0, [](double v) { return juce::String::formatted("%+d", (int) std::round(v)); }, "PITCH"));
        r3->add(knob("Fine", "pitchCents", -50, 50, 0, [](double v) { return juce::String::formatted("%+d", (int) std::round(v)) + juce::String::fromUTF8("\xc2\xa2"); }, "PITCH"));
        r3->add(knob("Mix", "pitchMix", 0, 1, 1, pct, "PITCH"));
        c->add(std::move(r3));
        box = std::make_unique<UnitBox>(m, "PITCH", std::move(c), "pitchOn", "panel:pitch");
        auto lat = std::make_unique<LiveLabel>([&m] { const float l = m.params.pitchLatencyMs.load(); return l > 0 ? juce::String::formatted("+%.0f ms", l) : juce::String(); }, 10);
        setPref(*lat, 56, 20); liveReg(lat.get()); box->addTrailing(std::move(lat));
    } else if (id == "amp") {
        auto c = col(10);
        auto r1 = row(6);
        // ‹ [current amp] › ⋯, like the cabinet (UI/AmpBrowser.cpp)
        r1->add(std::make_unique<IconButton>(IconButton::chevronLeft, [&m] { amp::step(m, -1, false); }, 12))->setTooltip("Previous amp");
        r1->add(amp::makeButton(m, false));
        r1->add(std::make_unique<IconButton>(IconButton::chevronRight, [&m] { amp::step(m, 1, false); }, 12))->setTooltip("Next amp");
        c->add(std::move(r1));
        if (m.s().s("nam").isNotEmpty()) {
            auto r = row();
            r->add(knob("Input", "namInput", -24, 24, 0, dbSigned, "AMP"));
            r->add(knob("Output", "namOutput", -24, 24, 0, dbSigned, "AMP"));
            c->add(std::move(r));
        } else if (m.activeCapture()) {
            auto r = row();
            r->add(knob("Drive", "capDrive", -18, 18, 0, dbSigned, "AMP"));
            r->add(knob("Level", "capLevel", -24, 12, 0, dbSigned, "AMP"));
            c->add(std::move(r));
        } else {
            const juce::StringArray names { VOICINGS[0].name, VOICINGS[1].name, VOICINGS[2].name };
            static const juce::Colour chans[3] = { juce::Colour::fromFloatRGBA(0.35f, 0.7f, 1, 1), juce::Colour::fromFloatRGBA(0.91f, 0.57f, 0.23f, 1), juce::Colour::fromFloatRGBA(1, 0.3f, 0.3f, 1) };
            c->add(std::make_unique<Segments>(m, names, [&m] { return m.s().i("ampChannel"); }, [&m](int i) { m.set("ampChannel", i); },
                                              [](int i) { return chans[i]; }, 7 * (int) (60 * theme::W) + 12, 30, 11));
            auto r = row();
            r->add(knob("Gain", "ampGain", 0, 1, 0.55, {}, "AMP"));
            r->add(knob("Bass", "bass", 0, 1, 0.5, {}, "AMP"));
            r->add(knob("Mid", "mid", 0, 1, 0.5, {}, "AMP"));
            r->add(knob("Treble", "treble", 0, 1, 0.55, {}, "AMP"));
            r->add(knob("Presence", "presence", 0, 1, 0.55, {}, "AMP"));
            r->add(knob("Depth", "depth", 0, 1, 0.5, {}, "AMP"));
            r->add(knob("Volume", "ampVolume", 0, 1, 0.5, {}, "AMP"));
            c->add(std::move(r));
        }
        box = std::make_unique<UnitBox>(m, "AMP", std::move(c), "ampOn", "panel:amp");
        box->alignTop = true;
    } else if (id == "master") {
        auto c = col(8);
        const auto outs = m.outputDeviceNames();
        auto dev = std::make_unique<Picker>((int) (120 * theme::W), [&m, outs] { return outs.indexOf(m.outputDeviceName()) + 1; },
                                            [&m, outs](int i) { if (juce::isPositiveAndBelow(i - 1, outs.size())) m.setOutputDevice(outs[i - 1]); });
        for (int i = 0; i < outs.size(); ++i) dev->addItem(outs[i], i + 1);
        dev->setTextWhenNothingSelected(outs.isEmpty() ? "No output device" : "Choose an output");
        dev->setTooltip(Model::explain("outputDevice"));
        // same column as INPUT's pickers; when INPUT shows a channel picker, a blank of its size keeps the device pickers level
        auto pk = col(6);
        const auto ph = pref(*dev).y;
        pk->add(std::move(dev));
        if (std::min(m.inputChannelCount(), 8) > 1) {
            auto gap = std::make_unique<juce::Component>();
            setPref(*gap, (int) (120 * theme::W), ph);
            pk->add(std::move(gap));
        }
        c->add(std::move(pk));
        auto out = knob("Output", "masterVol", -40, 12, 0, dbSigned, "MASTER");
        out->setMid(0);   // 0 dB at 12 o'clock
        auto r = row(); r->add(std::move(out));
        c->add(std::move(r));
        box = std::make_unique<UnitBox>(m, "MASTER", std::move(c), "", "panel:master");
    } else if (id == "cab") {
        // The rows and knobs depend on the cab type. Each part is sized to its largest variant (of the types on
        // offer), so switching types never resizes the panel and the panels around it stay where they are.
        std::vector<int> kinds { 0 };
        if (!m.cabIRs.isEmpty()) kinds.push_back(CAB_IRLIB);
        if (!m.irLib.isEmpty()) kinds.push_back(CAB_LIB);
        if (m.userIRName.isNotEmpty()) kinds.push_back(CAB_USER);
        auto block = [&](int) {   // the cab picker: current cab (opens the browser), previous / next, add IRs
            auto r1 = row(6);
            r1->add(std::make_unique<IconButton>(IconButton::chevronLeft, [&m] { cab::step(m, -1); }, 12))->setTooltip("Previous cabinet");
            r1->add(cab::makeButton(m));
            r1->add(std::make_unique<IconButton>(IconButton::chevronRight, [&m] { cab::step(m, 1); }, 12))->setTooltip("Next cabinet");
            r1->add(moreButton({ { juce::String::fromUTF8("Add IRs to the library (files or a folder)\xe2\x80\xa6"), [&m] { m.importCabIRsDialog(); } },
                                 { juce::String::fromUTF8("Load a single IR file\xe2\x80\xa6"), [&m] { m.loadIR(); } },
                                 { "Open IR library folder", openFolder(CabIRLibrary::root()) } }, "Add cabinet IRs"));
            return r1;
        };
        auto knobs = [&](int kind) {
            auto r = row();
            if (kind == CAB_LIB) r->add(knob(juce::String::fromUTF8("A \xe2\x86\x94 B"), "irBlend", 0, 1, 0.35, pct, "CABINET"));
            else if (kind < kCabCount)
                r->add(knob("Mic Pos", "cabMic", 0, 1, 0.3, [](double v) { return v < 0.12 ? juce::String("center") : v > 0.88 ? juce::String("edge") : juce::String(v * 10, 1); }, "CABINET"));
            r->add(knob("Low Cut", "cabLowCut", 20, 300, 80, intUnit("Hz"), "CABINET"));
            r->add(knob("High Cut", "cabHighCut", 2500, 16000, 10000, [](double v) { return juce::String(v / 1000, 1) + "k"; }, "CABINET"));
            r->add(knob("Level", "cabLevel", -12, 12, 0, [](double v) { return juce::String::formatted("%+.1f", v); }, "CABINET"));
            // two rows (cut and level, then the shape of the cab) keep the panel narrow
            auto r2 = row();
            auto pctOf = [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; };
            auto size = knob("Size", "cabSize", 0.8, 1.25, 1, pctOf, "CABINET"); size->setMid(1); r2->add(std::move(size));
            r2->add(knob("Length", "cabLength", 0.2, 1, 1, pctOf, "CABINET"));
            r2->add(knob("Tone", "cabTone", -6, 6, 0, [](double v) { return std::abs(v) < 0.05 ? juce::String("Flat") : juce::String::formatted("%+.1f", v); }, "CABINET"));
            auto both = col(6);
            both->add(std::move(r)); both->add(std::move(r2));
            return both;
        };
        juce::Point<int> blockMax, knobsMax;
        for (int k : kinds) {
            const auto bp = pref(*block(k)), kp = pref(*knobs(k));
            blockMax = { std::max(blockMax.x, bp.x), std::max(blockMax.y, bp.y) };
            knobsMax = { std::max(knobsMax.x, kp.x), std::max(knobsMax.y, kp.y) };
        }
        const int kind = s.i("cabModel");
        auto c = col(8);
        c->add(std::make_unique<MinSize>(block(kind), blockMax, false));
        c->add(std::make_unique<MinSize>(knobs(kind), knobsMax, true));
        box = std::make_unique<UnitBox>(m, "CABINET", std::move(c), "cabOn", "panel:cab");
        box->alignTop = true;
    } else if (id == "eq") {
        static const char* freqs[] = { "80", "160", "320", "640", "1.2k", "2.5k", "5k", "10k" };
        auto r = row(6);
        for (int i = 0; i < 8; ++i) {
            auto band = col(4);
            band->add(std::make_unique<LiveLabel>([&m, i] { return juce::String::formatted("%+.0f", m.s().eq()[(size_t) i]); }, 9, theme::secondary, juce::Justification::centred));
            band->add(std::make_unique<Fader>(m, i));
            band->add(std::make_unique<LiveLabel>([i] { return juce::String(freqs[i]); }, 9, theme::secondary, juce::Justification::centred));
            r->add(std::move(band));
        }
        box = std::make_unique<UnitBox>(m, "GRAPHIC EQ", std::move(r), "eqOn", "panel:eq");
    } else if (id == "delay") {
        auto c = col(8);
        auto r1 = row(6);
        auto sync = std::make_unique<Picker>((int) (110 * theme::W), [&m] { return m.s().i("delaySync") + 1; }, [&m](int i) { m.set("delaySync", i - 1); });
        for (size_t i = 0; i < DELAY_DIVS.size(); ++i) sync->addItem(DELAY_DIVS[i].name, (int) i + 1);
        sync->setTooltip("Free time, or a note length at the tempo");
        r1->add(std::move(sync));
        r1->add(std::make_unique<ActionButton>(m, "Tap", [&m] { m.tap(); }, MidiArmed::action("@tap", "Tap tempo")))->setTooltip("Tap the beat to set the tempo");
        auto info = std::make_unique<LiveLabel>([&m] {
            if (auto ms = m.syncedDelayMs()) return juce::String::formatted("%.0f BPM", m.bpm) + dot + juce::String::formatted("%.0f ms", *ms);
            return juce::String::formatted("%.0f BPM", m.bpm);
        }, 10);
        setPref(*info, 130, 20); r1->add(std::move(info));
        c->add(std::move(r1));
        auto r = row();
        if (s.i("delaySync") == 0) r->add(knob("Time", "delayTime", 40, 1500, 420, intUnit("ms"), "DELAY"));
        r->add(knob("Feedback", "delayFeedback", 0, 0.92, 0.28, pct, "DELAY"));
        r->add(knob("Tone", "delayTone", 0, 1, 0.45, {}, "DELAY"));
        r->add(knob("Mix", "delayMix", 0, 1, 0.2, pct, "DELAY"));
        c->add(std::move(r));
        box = std::make_unique<UnitBox>(m, "DELAY", std::move(c), "delayOn", "panel:delay");
    } else if (id == "ampb") {
        // a second amp (any NAM capture) fed by the same pedals; Spread puts amp A left and amp B right
        auto c = col(8);
        // same row as the AMP panel (‹ [amp] › ⋯), so the two panels are the same width
        auto r1 = row(6);
        r1->add(std::make_unique<IconButton>(IconButton::chevronLeft, [&m] { amp::step(m, -1, true); }, 12))->setTooltip("Previous amp");
        r1->add(amp::makeButton(m, true));
        r1->add(std::make_unique<IconButton>(IconButton::chevronRight, [&m] { amp::step(m, 1, true); }, 12))->setTooltip("Next amp");
        c->add(std::move(r1));
        auto r = row();
        r->add(knob("Level", "ampBLevel", -12, 12, 0, dbSigned, "AMP B"));
        r->add(knob("Spread", "ampBSpread", 0, 1, 1, pct, "AMP B"));
        c->add(std::move(r));
        box = std::make_unique<UnitBox>(m, "AMP B", std::move(c), "ampBOn", "panel:ampb");
        box->alignTop = true;
    } else if (id == "doubler") {
        auto r = row();
        r->add(knob("Width", "doublerWidth", 0, 1, 0.6, pct, "DOUBLER"));
        r->add(knob("Time", "doublerTime", 8, 40, 20, intUnit("ms"), "DOUBLER"));
        box = std::make_unique<UnitBox>(m, "DOUBLER", std::move(r), "doublerOn", "panel:doubler");
    } else if (id == "reverb") {
        auto r = row();
        r->add(knob("Size", "revSize", 0, 1, 0.45, {}, "REVERB"));
        r->add(knob("Mix", "revMix", 0, 1, 0.18, pct, "REVERB"));
        box = std::make_unique<UnitBox>(m, "REVERB", std::move(r), "revOn", "panel:reverb");
    } else {
        box = std::make_unique<UnitBox>(m, id.toUpperCase(), nullptr);
    }
    return box;
}


/// Interface and channel pickers for the Input panel (the same choices as Settings -> Audio).
std::unique_ptr<juce::Component> RigView::inputPickers() {
    auto& m = model;
    auto c = col(6);
    const auto names = m.inputDeviceNames();
    // id 1 = no input (play recordings without an interface); devices from 2
    auto dev = std::make_unique<Picker>((int) (120 * theme::W),
        [&m, names] { const auto n = m.inputDeviceName(); return n.isEmpty() ? 1 : names.indexOf(n) + 2; },
        [&m, names](int i) { if (i == 1) m.setInputDevice({}); else if (juce::isPositiveAndBelow(i - 2, names.size())) m.setInputDevice(names[i - 2]); });
    dev->addItem("No input (playback only)", 1);
    for (int i = 0; i < names.size(); ++i) dev->addItem(names[i], i + 2);
    dev->setTextWhenNothingSelected(names.isEmpty() ? "No input device" : "Choose an interface");
    dev->setTooltip(Model::explain("inputDevice"));
    c->add(std::move(dev));
    const int n = std::min(m.inputChannelCount(), 8);
    if (n > 1) {
        auto ch = std::make_unique<Picker>((int) (120 * theme::W), [&m] { return m.s().i("inputChannel") + 1; }, [&m](int i) { m.set("inputChannel", i - 1); });
        for (int i = 0; i < n; ++i) ch->addItem("Channel " + juce::String(i + 1), i + 1);
        ch->setTooltip(Model::explain("inputChannel"));
        c->add(std::move(ch));
    }
    return c;
}


// MARK: - pedals

std::unique_ptr<UnitBox> RigView::makePedal(PedalKind k) {
    auto& m = model;
    const juce::String scope = pedalTitle(k);
    auto knob = [&](const char* label, const char* key, double lo, double hi, double def, Fmt f = {}) {
        return std::make_unique<Knob>(m, label, key, lo, hi, def, f, scope);
    };
    std::unique_ptr<juce::Component> content;
    switch (k) {
    case PedalKind::comp: {
        auto r = row();
        r->add(knob("Sustain", "compSustain", 0, 1, 0.5)); r->add(knob("Attack", "compAttack", 1, 50, 10, intUnit("ms")));
        r->add(knob("Level", "compLevel", -12, 12, 0, [](double v) { return juce::String::formatted("%+.1f", v); })); r->add(knob("Blend", "compBlend", 0, 1, 1, pct));
        content = std::move(r); break;
    }
    case PedalKind::wah: {
        auto c = col(6);
        c->add(std::make_unique<Segments>(m, juce::StringArray { "Pedal", "Auto" }, [&m] { return m.s().b("wahAuto") ? 1 : 0; },
                                          [&m](int i) { m.set("wahAuto", i == 1); }, [&m](int) { return m.accent(); }, (int) (150 * theme::W), 26, 10));
        auto r = row();
        if (m.s().b("wahAuto")) r->add(knob("Sens", "wahSens", 0, 1, 0.5));
        else r->add(knob("Position", "wahPos", 0, 1, 0.5, pct));
        r->add(knob("Q", "wahQ", 0, 1, 0.5));
        c->add(std::move(r));
        content = std::move(c); break;
    }
    case PedalKind::fuzz: {
        auto r = row(); r->add(knob("Fuzz", "fuzzAmt", 0, 1, 0.6)); r->add(knob("Tone", "fuzzTone", 0, 1, 0.5)); r->add(knob("Level", "fuzzLevel", 0, 1, 0.5));
        content = std::move(r); break;
    }
    case PedalKind::blade: {
        auto c = col(6);
        auto sw = row(6);
        auto diode = std::make_unique<Segments>(m, juce::StringArray { "MOSFET", "Off", "Silicon" }, [&m] { return m.s().i("bladeDiode"); },
                                                [&m](int i) { m.set("bladeDiode", i); }, [&m](int) { return m.accent(); }, (int) (190 * theme::W), 26, 10);
        sw->add(std::move(diode));
        auto shift = std::make_unique<Segments>(m, juce::StringArray { "650", "1.5k" }, [&m] { return m.s().b("bladeShift") ? 1 : 0; },
                                                [&m](int i) { m.set("bladeShift", i == 1); }, [&m](int) { return m.accent(); }, (int) (100 * theme::W), 26, 10);
        sw->add(std::move(shift));
        auto boost = std::make_unique<Segments>(m, juce::StringArray { "Boost" }, [&m] { return m.s().b("bladeBoostOn") ? 0 : -1; },
                                                [&m](int) { m.set("bladeBoostOn", !m.s().b("bladeBoostOn")); }, [&m](int) { return m.accent(); }, 70, 26, 10);
        sw->add(std::move(boost));
        c->add(std::move(sw));
        auto db24 = [](double v) { return juce::String::formatted("%+.0f", (v - 0.5) * 24); };
        auto r = row();
        r->add(knob("Drive", "bladeDrive", 0, 1, 0.5)); r->add(knob("Low", "bladeLow", 0, 1, 0.5, db24)); r->add(knob("Mid", "bladeMid", 0, 1, 0.5, db24));
        r->add(knob("High", "bladeHigh", 0, 1, 0.5, db24));
        r->add(knob("Boost", "bladeBoost", 0, 1, 0.5, [](double v) { return juce::String::formatted("+%.0f dB", v * 20); })); r->add(knob("Level", "bladeLevel", 0, 1, 0.5));
        c->add(std::move(r));
        content = std::move(c); break;
    }
    case PedalKind::od: {
        auto r = row(); r->add(knob("Drive", "odDrive", 0, 1, 0)); r->add(knob("Tone", "odTone", 0, 1, 0.6)); r->add(knob("Level", "odLevel", 0, 1, 0.75));
        content = std::move(r); break;
    }
    case PedalKind::chorus: {
        auto r = row(); r->add(knob("Rate", "chorusRate", 0.1, 5, 0.8, hz)); r->add(knob("Depth", "chorusDepth", 0, 1, 0.5)); r->add(knob("Mix", "chorusMix", 0, 1, 0.5, pct));
        content = std::move(r); break;
    }
    case PedalKind::flanger: {
        auto r = row(); r->add(knob("Rate", "flangerRate", 0.05, 3, 0.25, hz)); r->add(knob("Depth", "flangerDepth", 0, 1, 0.7));
        r->add(knob("Feedback", "flangerFb", 0, 1, 0.5, pct)); r->add(knob("Mix", "flangerMix", 0, 1, 0.5, pct));
        content = std::move(r); break;
    }
    case PedalKind::phaser: {
        auto r = row(); r->add(knob("Rate", "phaserRate", 0.05, 5, 0.5, hz)); r->add(knob("Depth", "phaserDepth", 0, 1, 0.7)); r->add(knob("Feedback", "phaserFb", 0, 1, 0.4, pct));
        content = std::move(r); break;
    }
    case PedalKind::trem: {
        auto r = row(); r->add(knob("Rate", "tremRate", 0.5, 12, 5, hz)); r->add(knob("Depth", "tremDepth", 0, 1, 0.6, pct));
        r->add(knob("Shape", "tremShape", 0, 1, 0, [](double v) { return v < 0.1 ? juce::String("sine") : v > 0.9 ? juce::String("square") : juce::String(v * 10, 1); }));
        content = std::move(r); break;
    }
    }
    auto box = std::make_unique<UnitBox>(m, scope, std::move(content), pedalOnKey(k), "pedal:" + juce::String((int) k));
    auto x = std::make_unique<IconButton>(IconButton::cross, [&m, k] { m.removePedal(k); }, 12);
    x->colour = m.accent(); x->setTooltip("Remove");
    box->addTrailing(std::move(x));
    return box;
}

// MARK: - tuner

std::unique_ptr<juce::Component> RigView::makeTuner() {
    struct Tuner : juce::Component {
        explicit Tuner(Model& m) : model(m) {
            tuning.addItemList([] { juce::StringArray a; for (auto& t : TUNINGS) a.add(tuningLabel(t)); return a; }(), 1);
            tuning.setSelectedId(m.tuningIndex + 1, juce::dontSendNotification);
            tuning.onChange = [this] { model.setTuningIndex(tuning.getSelectedId() - 1); };
            addAndMakeVisible(tuning);
            a4.setSliderStyle(juce::Slider::IncDecButtons); a4.setRange(430, 450, 1); a4.setValue(m.a4, juce::dontSendNotification);
            a4.setTextValueSuffix(" Hz"); a4.onValueChange = [this] { model.setA4(a4.getValue()); };
            addAndMakeVisible(a4);
            mute.setButtonText("Mute while tuning"); mute.setToggleState(m.tunerMute, juce::dontSendNotification);
            mute.onClick = [this] { model.setTunerMute(mute.getToggleState()); };
            addAndMakeVisible(mute);
        }
        void resized() override {
            tuning.setBounds(16, 16, 320, 28); a4.setBounds(350, 16, 160, 28); mute.setBounds(getWidth() - 200, 16, 184, 28);
        }
        void paint(juce::Graphics& g) override {
            auto r = getLocalBounds().toFloat().reduced(0.5f);
            g.setColour(theme::panel); g.fillRoundedRectangle(r, 12); g.setColour(theme::line); g.drawRoundedRectangle(r, 12, 1);
            const auto& t = model.tuning();
            const bool inTune = std::abs(model.tunerCents) < 3;
            const auto acc = model.accent();
            g.setColour(inTune ? juce::Colour(0xff4cd964) : theme::text); g.setFont(theme::font(64, true));
            g.drawText(model.tunerNote, getLocalBounds().withY(50).withHeight(110), juce::Justification::centred);
            const float bw = 420, bx = ((float) getWidth() - bw) / 2, by = 170;
            juce::ColourGradient grad(juce::Colours::red.withAlpha(0.4f), bx, 0, juce::Colours::red.withAlpha(0.4f), bx + bw, 0, false);
            grad.addColour(0.25, juce::Colour(0xff333333)); grad.addColour(0.5, juce::Colours::green.withAlpha(0.5f)); grad.addColour(0.75, juce::Colour(0xff333333));
            g.setGradientFill(grad); g.fillRoundedRectangle(bx, by, bw, 18, 9);
            g.setColour(juce::Colours::white.withAlpha(0.4f)); g.fillRect(bx + bw / 2, by, 1.f, 18.f);
            const float nx = bx + bw / 2 + (float) (juce::jlimit(-50.0, 50.0, model.tunerCents) / 50) * bw / 2;
            g.setColour(inTune ? juce::Colour(0xff4cd964) : juce::Colours::white); g.fillRoundedRectangle(nx - 2, by - 4, 4, 26, 2);
            g.setColour(theme::secondary); g.setFont(theme::font(12));
            g.drawText(model.tunerFreq > 0 ? juce::String::formatted("%.1f Hz", model.tunerFreq) + juce::String::fromUTF8(" \xc2\xb7 ")
                                                 + juce::String::formatted("%+.0f", model.tunerCents) + juce::String::fromUTF8(" \xc2\xa2") : juce::String("play one string"),
                       getLocalBounds().withY(196).withHeight(22), juce::Justification::centred);
            const float sw = 48, gap = 8, total = 6 * sw + 5 * gap, sx = ((float) getWidth() - total) / 2;
            for (int i = 0; i < 6; ++i) {
                auto rr = juce::Rectangle<float>(sx + (float) i * (sw + gap), 232, sw, 30);
                const bool on = model.tunerString == i;
                g.setColour(on ? (inTune ? juce::Colour(0xff4cd964) : acc).withAlpha(0.85f) : theme::panel2); g.fillRoundedRectangle(rr, 6);
                g.setColour(on ? juce::Colours::black : theme::text); g.setFont(theme::font(13, true));
                const int n = t.strings[(size_t) i];
                g.drawText(noteName(n, t.flats) + juce::String(n / 12 - 1), rr, juce::Justification::centred);
            }
        }
        Model& model; juce::ComboBox tuning; juce::Slider a4; juce::ToggleButton mute;
    };
    auto t = std::make_unique<Tuner>(model);
    liveReg(t.get());
    return t;
}

} // namespace lev
