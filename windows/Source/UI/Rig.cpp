#include "Rig.h"

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

std::unique_ptr<juce::TextButton> textButton(const juce::String& t, std::function<void()> fn) {
    auto b = std::make_unique<juce::TextButton>(t);
    b->onClick = std::move(fn);
    setPref(*b, juce::GlyphArrangement::getStringWidthInt(theme::font(10), t) + 24, 26);
    return b;
}
} // namespace

namespace {
struct HelpStrip : juce::Component {
    explicit HelpStrip(Model& m) : model(m) {}
    void paint(juce::Graphics& g) override {
        g.fillAll(theme::bar);
        g.setColour(theme::line); g.fillRect(0, 0, getWidth(), 1);
        auto r = getLocalBounds().reduced(14, 6);
        g.setColour(theme::secondary);
        g.drawEllipse(juce::Rectangle<float>((float) r.getX(), (float) r.getCentreY() - 7, 14, 14), 1.2f);
        g.setFont(theme::font(10, true)); g.drawText("i", r.getX(), r.getCentreY() - 7, 14, 14, juce::Justification::centred);
        r.removeFromLeft(24);
        if (model.hoverText.isEmpty()) {
            g.setFont(theme::font(12));
            g.drawText(juce::String::fromUTF8("Hover over any control for an explanation. Scroll or drag a dial to change it; \xe2\x86\xba resets it."), r, juce::Justification::centredLeft, true);
            return;
        }
        g.setColour(theme::text); g.setFont(theme::font(12, true));
        const int tw = juce::GlyphArrangement::getStringWidthInt(theme::font(12, true), model.hoverTitle) + 12;
        g.drawText(model.hoverTitle, r.removeFromLeft(tw), juce::Justification::centredLeft, true);
        g.setColour(theme::secondary); g.setFont(theme::font(12));
        g.drawFittedText(model.hoverText, r, juce::Justification::centredLeft, 2);
    }
    Model& model;
};
} // namespace

RigView::RigView(Model& m) : model(m) {
    helpStrip = std::make_unique<HelpStrip>(m);
    addChildComponent(*helpStrip);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&canvas, false);
    viewport.setScrollBarsShown(true, false);
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
    sig.add(s.s("capture")); sig.add(juce::String((int) model.captures.size()));
    sig.add(s.s("nam")); sig.add(model.namFiles.joinIntoString(",")); sig.add(model.namReady() ? "namok" : "");
    sig.add(s.b("wahAuto") ? "wa" : "wm");
    sig.add("cab" + juce::String(s.i("cabModel"))); sig.add(model.irLib.isEmpty() ? "nolib" : "lib"); sig.add(model.userIRName);
    sig.add(s.s("irA")); sig.add(s.s("irB"));
    sig.add(s.i("delaySync") == 0 ? "free" : "sync");
    sig.add(model.tunerOn ? "tuner" : "");
    sig.add(juce::String(model.accent().getARGB()));
    return sig.joinIntoString("/");
}

void RigView::refresh() {
    const auto sig = currentSignature();
    if (sig != signature) { signature = sig; rebuild(); }
    else { syncAll(canvas); layout(); }
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
    int y = padding;
    const juce::Rectangle<int> full(padding, 0, w - 2 * padding, 0);
    if (tunerView) { tunerView->setBounds(full.getX(), y, full.getWidth(), 300); y += 312; }
    std::vector<UnitBox*> items;
    for (const auto& id : model.panelOrder) {
        if (id == "|") items.push_back(nullptr);
        else if (auto it = panels.find(id); it != panels.end()) items.push_back(it->second.get());
    }
    int used = 0;
    layoutGrid(items, { full.getX(), y, full.getWidth(), 0 }, used);
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
        r->add(knob("Release", "gateRelease", 10, 400, 60, intUnit("ms"), "GATE"));
        box = std::make_unique<UnitBox>(m, "GATE", std::move(r), "gateOn", "panel:gate");
        auto dotC = std::make_unique<GateDot>(m); liveReg(dotC.get()); box->addTrailing(std::move(dotC));
    } else if (id == "pitch") {
        auto c = col(8);
        auto r1 = row(8);
        r1->add(std::make_unique<LiveLabel>([] { return juce::String("Guitar"); }, 12, theme::secondary));
        auto tune = std::make_unique<Picker>((int) (215 * theme::W), [&m] { return m.tuningIndex + 1; }, [&m](int i) { m.setTuningIndex(i - 1); });
        for (size_t i = 0; i < TUNINGS.size(); ++i) tune->addItem(tuningLabel(TUNINGS[i]), (int) i + 1);
        r1->add(std::move(tune));
        c->add(std::move(r1));
        auto r2 = row(4);
        r2->add(std::make_unique<IconButton>(IconButton::minus, [&m] { m.playAs(std::max((int) std::round(m.s().d("pitchSemis")) - 1, -12)); }, 12))->setTooltip("Down a semitone");
        auto playAs = std::make_unique<Picker>((int) (235 * theme::W), [&m] { return 13 - (int) std::round(m.s().d("pitchSemis")); }, [&m](int i) { m.playAs(13 - i); });
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
        // ids: 1 = built-in voicings, 2.. = fitted captures, 1000.. = NAM captures
        auto cap = std::make_unique<Picker>((int) (190 * theme::W), [&m] {
            if (const auto nm = m.s().s("nam"); nm.isNotEmpty()) { const int k = m.namFiles.indexOf(nm); return k >= 0 ? 1000 + k : 1; }
            const auto cur = m.s().s("capture"); if (cur.isEmpty()) return 1;
            int i = 2; for (auto& [k, v] : m.captures) { if (k == cur) return i; i++; } return 1;
        }, [&m](int i) {
            if (i >= 1000) { m.useNam(m.namFiles[i - 1000]); return; }
            if (i == 1) { m.useNam(""); m.useCapture(""); return; }
            int k = 2; for (auto& [name, v] : m.captures) { if (k == i) { m.useCapture(name); return; } k++; }
        });
        cap->addItem("Leviathan voicings", 1);
        if (!m.namFiles.isEmpty()) { cap->addSeparator(); for (int i = 0; i < m.namFiles.size(); ++i) cap->addItem(juce::String::fromUTF8("NAM \xc2\xb7 ") + m.namFiles[i], 1000 + i); }
        if (!m.captures.empty()) { cap->addSeparator(); int i = 2; for (auto& [k, v] : m.captures) cap->addItem(juce::String::fromUTF8("Capture \xc2\xb7 ") + k, i++); }
        r1->add(std::move(cap));
        r1->add(std::make_unique<IconButton>(IconButton::refresh, [&m] { m.reloadCaptures(); m.rescanNam(); }, 13))->setTooltip("Rescan captures and NAM models");
        r1->add(textButton(juce::String::fromUTF8("Add .nam\xe2\x80\xa6"), [&m] { m.importNamDialog(); }))->setTooltip("Add Neural Amp Modeler captures (free at tone3000.com)");
        if (auto* ac = m.activeCapture())
            r1->add(std::make_unique<LiveLabel>([ac] { return juce::String::formatted("accuracy %.1f%%", std::max(0.0, 1 - ac->esr_val) * 100); }, 10))
                ->setTooltip(juce::String::fromUTF8("1 \xe2\x88\x92 ESR on held-out audio from the capture"));
        c->add(std::move(r1));
        if (m.s().s("nam").isNotEmpty()) {
            auto r = row();
            r->add(knob("Input", "namInput", -24, 24, 0, dbSigned, "AMP"));
            r->add(knob("Output", "namOutput", -24, 24, 0, dbSigned, "AMP"));
            auto note = std::make_unique<LiveLabel>([&m] {
                juce::String t = m.namReady() ? "Neural Amp Modeler capture" : juce::String::fromUTF8("Loading NAM capture\xe2\x80\xa6");
                if (m.namReady() && m.namExpectedRate > 0) t << "\n" << juce::String(m.namExpectedRate / 1000, 1) << " kHz";
                if (m.namReady() && m.namLoudness) t << juce::String::fromUTF8(" \xc2\xb7 loudness ") << juce::String(*m.namLoudness, 1) << " dB (normalised)";
                return t + "\nInput = how hard you hit it; Output = level.";
            }, 10);
            setPref(*note, (int) (230 * theme::W), 60); r->add(std::move(note));
            c->add(std::move(r));
        } else if (m.activeCapture()) {
            auto r = row();
            r->add(knob("Drive", "capDrive", -18, 18, 0, dbSigned, "AMP"));
            r->add(knob("Level", "capLevel", -24, 12, 0, dbSigned, "AMP"));
            auto note = std::make_unique<LiveLabel>([] { return juce::String("Drive 0 / Level 0 = as captured.\nCab is part of the capture."); }, 10);
            setPref(*note, (int) (180 * theme::W), 40); r->add(std::move(note));
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
    } else if (id == "master") {
        auto r = row(); r->add(knob("Output", "masterVol", -40, 12, -6, dbSigned, "MASTER"));
        box = std::make_unique<UnitBox>(m, "MASTER", std::move(r), "", "panel:master");
        auto reset = std::make_unique<IconButton>(IconButton::reset, [&m] { m.set("masterVol", -6.0); }, 14);
        reset->colour = m.accent(); reset->setTooltip("Reset output to -6 dB (or double-click the knob)");
        box->addTrailing(std::move(reset));
    } else if (id == "cab") {
        const bool lib = s.i("cabModel") == CAB_LIB;
        auto c = col(8);
        auto block = col(8, juce::Justification::left);
        auto r1 = row(8);
        auto pick = std::make_unique<Picker>((int) (230 * theme::W), [&m] { return m.s().i("cabModel") + 1; }, [&m](int i) { m.set("cabModel", i - 1); });
        if (!m.irLib.isEmpty()) pick->addItem(juce::String::fromUTF8("God's Cab \xc2\xb7 Mesa OS 4x12"), CAB_LIB + 1);
        for (int i = 0; i < kCabCount; ++i) pick->addItem(juce::String::fromUTF8("Built-in \xc2\xb7 ") + CABS[i].name, i + 1);
        if (!m.cabIRs.isEmpty()) pick->addItem(juce::String::fromUTF8("IR library \xc2\xb7 ") + juce::String(m.cabIRs.all().size()) + " IRs", CAB_IRLIB + 1);
        if (m.userIRName.isNotEmpty()) pick->addItem(juce::String::fromUTF8("File \xc2\xb7 ") + m.userIRName, CAB_USER + 1);
        r1->add(std::move(pick));
        r1->add(textButton(juce::String::fromUTF8("IR file\xe2\x80\xa6"), [&m] { m.loadIR(); }));
        r1->add(textButton(juce::String::fromUTF8("Add IRs\xe2\x80\xa6"), [&m] { m.importCabIRsDialog(); }))
            ->setTooltip("Add a folder of cab IRs (WAV) to the IR library, e.g. a tone pack from tone3000.com");
        block->add(std::move(r1));
        if (s.i("cabModel") == CAB_IRLIB) block->add(irLibraryRow());
        if (lib) { block->add(irRow("A", "irA", false)); block->add(irRow("B", "irB", true)); }
        c->add(std::move(block));
        auto r = row();
        if (lib) r->add(knob(juce::String::fromUTF8("A \xe2\x86\x94 B"), "irBlend", 0, 1, 0.35, pct, "CABINET"));
        else if (s.i("cabModel") < kCabCount)
            r->add(knob("Mic Pos", "cabMic", 0, 1, 0.3, [](double v) { return v < 0.12 ? juce::String("center") : v > 0.88 ? juce::String("edge") : juce::String(v * 10, 1); }, "CABINET"));
        r->add(knob("Low Cut", "cabLowCut", 20, 300, 80, intUnit("Hz"), "CABINET"));
        r->add(knob("High Cut", "cabHighCut", 2500, 16000, 10000, [](double v) { return juce::String(v / 1000, 1) + "k"; }, "CABINET"));
        r->add(knob("Level", "cabLevel", -12, 12, 0, [](double v) { return juce::String::formatted("%+.1f", v); }, "CABINET"));
        c->add(std::move(r));
        box = std::make_unique<UnitBox>(m, "CABINET", std::move(c), "cabOn", "panel:cab");
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

/// Mic / position / presence / TS pickers for one God's Cab slot, plus prev/next for quick auditioning.
namespace {
/// Searchable list of every IR in the library. Clicking one plays it straight away; the box stays open to compare.
class IRBrowser : public juce::Component, private juce::ListBoxModel {
public:
    explicit IRBrowser(Model& m) : model(m) {
        search.setTextToShowWhenEmpty("Search " + juce::String(m.cabIRs.all().size()) + juce::String::fromUTF8(" IRs (band, song, amp\xe2\x80\xa6)"), theme::secondary);
        search.onTextChange = [this] { filter(); };
        addAndMakeVisible(search);
        list.setModel(this); list.setRowHeight(24);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(list);
        filter();
        setSize(440, 440);
    }
    void resized() override { auto r = getLocalBounds().reduced(8); search.setBounds(r.removeFromTop(28)); r.removeFromTop(6); list.setBounds(r); }
    void visibilityChanged() override { if (isShowing()) search.grabKeyboardFocus(); }
private:
    void filter() {
        hits.clear();
        const auto q = search.getText().toLowerCase();
        for (const auto& f : model.cabIRs.all()) if (q.isEmpty() || f.toLowerCase().contains(q)) hits.add(f);
        list.updateContent(); list.repaint();
        const int cur = hits.indexOf(model.s().s("irFile"));
        if (cur >= 0) list.scrollToEnsureRowIsOnscreen(cur);
    }
    int getNumRows() override { return hits.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool) override {
        if (!juce::isPositiveAndBelow(row, hits.size())) return;
        const auto& rel = hits[row];
        const bool on = rel == model.s().s("irFile");
        if (on) { g.setColour(model.accent().withAlpha(0.2f)); g.fillRoundedRectangle(0, 1, (float) w, (float) h - 2, 4); }
        const auto folder = rel.containsChar('/') ? rel.upToLastOccurrenceOf("/", false, false) : juce::String();
        g.setColour(on ? model.accent() : theme::text); g.setFont(13.0f);
        g.drawText(CabIRLibrary::title(rel), 8, 0, w - 160, h, juce::Justification::centredLeft, true);
        g.setColour(theme::secondary); g.setFont(11.0f);
        g.drawText(folder, w - 150, 0, 142, h, juce::Justification::centredRight, true);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override {
        if (!juce::isPositiveAndBelow(row, hits.size())) return;
        Settings x = model.s(); x.set("irFile", hits[row]); x.set("cabModel", CAB_IRLIB); x.set("cabOn", true); model.setSettings(x);
        list.repaint();
    }
    Model& model;
    juce::TextEditor search;
    juce::ListBox list;
    juce::StringArray hits;
};
} // namespace

/// Interface and channel pickers for the Input panel (the same choices as Settings -> Audio).
std::unique_ptr<juce::Component> RigView::inputPickers() {
    auto& m = model;
    auto c = col(6);
    const auto names = m.inputDeviceNames();
    // id 1 = no input (play recordings without an interface); devices from 2
    auto dev = std::make_unique<Picker>((int) (190 * theme::W),
        [&m, names] { const auto n = m.inputDeviceName(); return n.isEmpty() ? 1 : names.indexOf(n) + 2; },
        [&m, names](int i) { if (i == 1) m.setInputDevice({}); else if (juce::isPositiveAndBelow(i - 2, names.size())) m.setInputDevice(names[i - 2]); });
    dev->addItem("No input (playback only)", 1);
    for (int i = 0; i < names.size(); ++i) dev->addItem(names[i], i + 2);
    dev->setTextWhenNothingSelected(names.isEmpty() ? "No input device" : "Choose an interface");
    dev->setTooltip(Model::explain("inputDevice"));
    c->add(std::move(dev));
    const int n = std::min(m.inputChannelCount(), 8);
    if (n > 1) {
        auto ch = std::make_unique<Picker>((int) (190 * theme::W), [&m] { return m.s().i("inputChannel") + 1; }, [&m](int i) { m.set("inputChannel", i - 1); });
        for (int i = 0; i < n; ++i) ch->addItem("Channel " + juce::String(i + 1), i + 1);
        ch->setTooltip(Model::explain("inputChannel"));
        c->add(std::move(ch));
    }
    return c;
}

/// The IR library's current IR (click to search and pick), plus prev/next for quick auditioning.
std::unique_ptr<juce::Component> RigView::irLibraryRow() {
    auto& m = model;
    auto r = row(6);
    const auto rel = m.s().s("irFile");
    auto b = textButton(rel.isEmpty() ? juce::String::fromUTF8("Choose an IR\xe2\x80\xa6") : CabIRLibrary::title(rel), {});
    auto* bp = b.get();
    b->onClick = [&m, bp] {
        juce::CallOutBox::launchAsynchronously(std::make_unique<IRBrowser>(m), bp->getScreenBounds(), nullptr);
    };
    setPref(*b, (int) (300 * theme::W), 26);
    b->setTooltip(rel);
    r->add(std::move(b));
    r->add(std::make_unique<IconButton>(IconButton::chevronLeft, [&m] { m.set("irFile", m.cabIRs.step(m.s().s("irFile"), -1)); }, 12))->setTooltip("Previous IR");
    r->add(std::make_unique<IconButton>(IconButton::chevronRight, [&m] { m.set("irFile", m.cabIRs.step(m.s().s("irFile"), 1)); }, 12))->setTooltip("Next IR");
    return r;
}

std::unique_ptr<juce::Component> RigView::irRow(const juce::String& label, const juce::String& key, bool optional) {
    auto& m = model;
    auto r = row(6);
    r->add(std::make_unique<LiveLabel>([label] { return label; }, 10, theme::secondary, juce::Justification::centredLeft, true))->setSize(12, 20);
    const auto rel = m.s().s(key);
    const IRFile* f = m.irLib.file(rel);
    auto pick = [&m, key](const juce::String& mic, bool ts, const juce::String& pos, int pres) {
        if (auto* x = m.irLib.find(mic, ts, pos, pres)) m.set(key, x->rel);
    };
    const auto mics = m.irLib.mics();
    auto micPick = std::make_unique<Picker>((int) (82 * theme::W), [&m, key, mics] {
        const auto* cur = m.irLib.file(m.s().s(key)); return cur ? mics.indexOf(cur->mic) + 2 : 1;
    }, [&m, key, mics, pick, f](int i) {
        if (i == 1) { m.set(key, juce::String()); return; }
        pick(mics[i - 2], f ? f->ts : false, f ? f->position : juce::String("1 inch edge"), f ? f->pres : 3);
    });
    if (optional || f == nullptr) micPick->addItem("Off", 1);
    for (int i = 0; i < mics.size(); ++i) micPick->addItem(mics[i], i + 2);
    r->add(std::move(micPick));
    if (f) {
        const auto positions = m.irLib.positions(f->mic, f->ts);
        const IRFile cur = *f;
        auto pos = std::make_unique<Picker>((int) (150 * theme::W), [positions, cur] { return positions.indexOf(cur.position) + 1; },
                                            [positions, cur, pick](int i) { pick(cur.mic, cur.ts, positions[i - 1], cur.pres); });
        for (int i = 0; i < positions.size(); ++i) pos->addItem(positions[i], i + 1);
        r->add(std::move(pos));
        auto pres = std::make_unique<Picker>((int) (76 * theme::W), [cur] { return cur.pres; }, [cur, pick](int i) { pick(cur.mic, cur.ts, cur.position, i); });
        for (int i = 1; i <= 5; ++i) pres->addItem("Pres " + juce::String(i), i);
        pres->setTooltip("Amp presence when captured (higher = brighter)");
        r->add(std::move(pres));
        auto ts = textButton("TS", [cur, pick] { pick(cur.mic, !cur.ts, cur.position, cur.pres); });
        ts->setToggleState(cur.ts, juce::dontSendNotification); ts->setEnabled(m.irLib.tsAvailable(cur.mic));
        ts->setTooltip("Tube Screamer mid bump baked into the IR");
        r->add(std::move(ts));
        r->add(std::make_unique<IconButton>(IconButton::chevronLeft, [&m, key, cur] { m.set(key, m.irLib.step(cur.rel, -1)); }, 12));
        r->add(std::make_unique<IconButton>(IconButton::chevronRight, [&m, key, cur] { m.set(key, m.irLib.step(cur.rel, 1)); }, 12));
    }
    return r;
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
