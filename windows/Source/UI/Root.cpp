#include "Root.h"
#include "NativeMenus.h"
#include "../Sessions.h"
#include "../Updater.h"
#include "../DebugLog.h"
#include "Markdown.h"

namespace lev {

namespace {
const juce::String dot = juce::String::fromUTF8(" \xc2\xb7 ");
const juce::String ellipsis = juce::String::fromUTF8("\xe2\x80\xa6");

juce::TextButton* sizeButton(juce::TextButton& b, int extra = 24) {
    setPref(b, juce::GlyphArrangement::getStringWidthInt(theme::font(10), b.getButtonText()) + extra, 28);
    return &b;
}

void askText(const juce::String& title, const juce::String& initial, const juce::String& okText, std::function<void(juce::String)> done) {
    if (nativeAskText(title, initial, okText, done)) return;   // Android: the system's own text prompt
    auto* w = new juce::AlertWindow(title, {}, juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("name", initial, "Name");
    w->addButton(okText, 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([w, done](int r) {
        if (r == 1) done(w->getTextEditorContents("name"));
    }), true);
}

void drawSectionTitle(juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r) {
    g.setColour(theme::secondary); g.setFont(theme::font(10, true, 2)); g.drawText(t, r, juce::Justification::centredLeft);
}

void boxBackground(juce::Graphics& g, juce::Rectangle<float> r, float radius = 12) {
    g.setColour(theme::panel); g.fillRoundedRectangle(r.reduced(0.5f), radius);
    g.setColour(theme::line); g.drawRoundedRectangle(r.reduced(0.5f), radius, 1);
}

// MARK: - sidebar

struct Sidebar : juce::Component {
    explicit Sidebar(Model& m) : model(m), minus(IconButton::minus, [&m] { m.zoomStep(-1); }, onAndroid ? 18 : 12), plus(IconButton::plus, [&m] { m.zoomStep(1); }, onAndroid ? 18 : 12) {
        addAndMakeVisible(minus); addAndMakeVisible(plus);
        minus.setTooltip(juce::String::fromUTF8("Zoom out (Ctrl \xe2\x88\x92)")); plus.setTooltip("Zoom in (Ctrl +)");
    }
    struct Item { Screen s; const char* label; };
    const Item items[3] = { { Screen::rig, "Rig" }, { Screen::presets, "Presets" }, { Screen::recordings, "Recordings" } };
    juce::Rectangle<int> itemRect(int i) const { return { 6, 10 + i * 74, getWidth() - 12, 70 }; }
    juce::Rectangle<int> settingsRect() const { return { 6, getHeight() - 80, getWidth() - 12, 70 }; }
    // Zoom: side by side on a computer; on a phone a column of finger-sized buttons (+ on top, then the %, then -)
    juce::Rectangle<int> zoomLabelRect() const {
        return onAndroid ? juce::Rectangle<int>(0, getHeight() - 88 - 48 - 24, getWidth(), 24) : juce::Rectangle<int>(0, getHeight() - 110, getWidth(), 18);
    }
    void resized() override {
        if (onAndroid) {
            const auto l = zoomLabelRect();
            plus.setBounds(getWidth() / 2 - 26, l.getY() - 48, 52, 48);
            minus.setBounds(getWidth() / 2 - 26, l.getBottom(), 52, 48);
        } else {
            minus.setBounds(getWidth() / 2 - 32, getHeight() - 140, 30, 26); plus.setBounds(getWidth() / 2 + 2, getHeight() - 140, 30, 26);
        }
    }
    void drawItem(juce::Graphics& g, juce::Rectangle<int> r, Screen s, const juce::String& label, bool badge) {
        const bool on = model.screen == s;
        if (on) { g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRoundedRectangle(r.toFloat(), 10); }
        const auto c = on ? model.accent() : theme::secondary;
        auto icon = r.toFloat().removeFromTop(44).withSizeKeepingCentre(22, 22).translated(0, 6);
        g.setColour(c);
        juce::Path p;
        if (s == Screen::rig) for (int i = 0; i < 3; ++i) { const float x = icon.getX() + 4 + (float) i * 7; p.startNewSubPath(x, icon.getY()); p.lineTo(x, icon.getBottom()); p.addEllipse(x - 2.5f, icon.getY() + (float) (i == 1 ? 13 : 5), 5, 5); }
        else if (s == Screen::presets) for (int i = 0; i < 3; ++i) { const float y = icon.getY() + 4 + (float) i * 7; p.addEllipse(icon.getX(), y - 1, 2.5f, 2.5f); p.startNewSubPath(icon.getX() + 6, y); p.lineTo(icon.getRight(), y); }
        else if (s == Screen::recordings) for (int i = 0; i < 7; ++i) { const float x = icon.getX() + 2 + (float) i * 3, h = (float) (std::abs(3 - i) < 2 ? 18 : std::abs(3 - i) < 3 ? 12 : 7); p.startNewSubPath(x, icon.getCentreY() - h / 2); p.lineTo(x, icon.getCentreY() + h / 2); }
        else { p.addEllipse(icon.reduced(4)); p.addEllipse(icon.withSizeKeepingCentre(5, 5)); for (int i = 0; i < 8; ++i) { const float a = (float) i * juce::MathConstants<float>::pi / 4; p.startNewSubPath(icon.getCentreX() + std::cos(a) * 7, icon.getCentreY() + std::sin(a) * 7); p.lineTo(icon.getCentreX() + std::cos(a) * 11, icon.getCentreY() + std::sin(a) * 11); } }
        g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (badge) { g.setColour(juce::Colours::red); g.fillEllipse(icon.getRight() - 2, icon.getY() - 3, 9, 9); }
        g.setColour(c); g.setFont(theme::font(9, true));
        g.drawText(label, r.withTrimmedTop(46).withHeight(20), juce::Justification::centred);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(theme::bar);
        g.setColour(theme::line); g.fillRect(getWidth() - 1, 0, 1, getHeight());
        for (int i = 0; i < 3; ++i) drawItem(g, itemRect(i), items[i].s, items[i].label, items[i].s == Screen::recordings && model.sessions->recording);
        drawItem(g, settingsRect(), Screen::settings, "Settings", model.updater->phase == Updater::Phase::available);
        if (onAndroid) {   // finger-sized buttons need to look like buttons
            g.setColour(theme::panel2);
            for (auto* b : { &plus, &minus }) g.fillRoundedRectangle(b->getBounds().toFloat().reduced(2), 10);
        }
        g.setColour(theme::secondary); g.setFont(theme::font(9));
        g.drawText(juce::String(juce::roundToInt(model.zoom * 100)) + "%", zoomLabelRect(), juce::Justification::centred);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        for (int i = 0; i < 3; ++i) if (itemRect(i).contains(e.getPosition())) model.setScreen(items[i].s);
        if (settingsRect().contains(e.getPosition())) model.setScreen(Screen::settings);
        if (zoomLabelRect().expanded(0, 2).contains(e.getPosition())) model.setZoom(1.0);
    }
    Model& model; IconButton minus, plus;
};

// MARK: - top bar

struct Meters : juce::Component {
    explicit Meters(Model& m) : model(m) { setPref(*this, 170, 36); }
    void paint(juce::Graphics& g) override {
        auto bar = [&](int y, float level, const char* label) {
            g.setColour(theme::secondary); g.setFont(theme::font(9)); g.drawText(label, 0, y, 30, 16, juce::Justification::centredRight);
            auto r = juce::Rectangle<float>(36, (float) y + 4, (float) getWidth() - 40, 8);
            g.setColour(juce::Colours::black); g.fillRoundedRectangle(r, 3);
            const float db = 20 * std::log10(std::max(level, 1e-6f)), t = juce::jlimit(0.f, 1.f, (db + 60) / 60);
            g.setColour(db > -1 ? juce::Colours::red : db > -9 ? juce::Colours::yellow : juce::Colour(0xff4cd964));
            g.fillRoundedRectangle(r.withWidth(r.getWidth() * t), 3);
        };
        bar(0, model.params.inPeak.load(), "IN"); bar(18, model.params.outPeak.load(), "OUT");
    }
    Model& model;
};

/// Puts a component's explanation in the rig's explanation strip while the pointer is over it.
struct HoverExplainer : juce::MouseListener {
    explicit HoverExplainer(Model& m) : model(m) {}
    void add(juce::Component& c, const char* key, const juce::String& title) { items[&c] = { key, title }; c.addMouseListener(this, true); }
    void detach() { for (auto& it : items) it.first->removeMouseListener(this); items.clear(); }
    void mouseEnter(const juce::MouseEvent& e) override {
        for (auto* c = e.eventComponent; c; c = c->getParentComponent())
            if (auto it = items.find(c); it != items.end()) { model.showHover(it->second.first, it->second.second, Model::explain(it->second.first)); return; }
    }
    void mouseExit(const juce::MouseEvent& e) override {
        for (auto* c = e.eventComponent; c; c = c->getParentComponent())
            if (auto it = items.find(c); it != items.end()) { model.clearHover(it->second.first); return; }
    }
    Model& model;
    std::map<juce::Component*, std::pair<juce::String, juce::String>> items;
};

/// The whole take so far, squeezed to fit; shown in the top bar while recording.
struct LiveWave : juce::Component, private juce::Timer {
    explicit LiveWave(Model& m) : model(m) { startTimerHz(15); }
    void timerCallback() override { if (isVisible()) repaint(); }
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(0xff1a1a1a)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 5);
        const auto& w = model.sessions->liveWave;
        const size_t n = w.size() / 2;
        if (n == 0) return;
        const int cols = std::max(getWidth() / 2, 1);
        const double per = std::max((double) n / cols, 1.0);
        const int used = (int) std::min((size_t) cols, n);
        const float mid = (float) getHeight() / 2, amp = (float) getHeight() / 2 - 3;
        g.setColour(juce::Colours::red.withAlpha(0.9f));
        for (int c = 0; c < used; ++c) {
            const size_t a = (size_t) (c * per), b = std::max((size_t) ((c + 1) * per), a + 1);
            float lo = 0, hi = 0;
            for (size_t i = a; i < std::min(b, n); ++i) { lo = std::min(lo, w[2 * i]); hi = std::max(hi, w[2 * i + 1]); }
            g.fillRect((float) c * 2 + 1, mid - hi * amp, 1.5f, std::max((hi - lo) * amp, 1.f));
        }
    }
    Model& model;
};

struct TopBar : juce::Component {
    explicit TopBar(Model& m) : model(m),
        power(""), rec(""), tuner("Tuner"), midi("MIDI"), save("Save"), saveAs("Save as..."), presetMenu(""), undoB(juce::String::fromUTF8("\xe2\x86\xb6")),
        metro(IconButton::metronome, [this] { showTempo(); }, 16),   // every metronome setting in one popup
        prev(IconButton::chevronLeft, [&m] { m.confirmLoad([&m] { m.stepPreset(-1); }); }, 13), next(IconButton::chevronRight, [&m] { m.confirmLoad([&m] { m.stepPreset(1); }); }, 13),
        bpm(""), meters(m),
        tunerLearn(m, MidiArmed::action("@tuner", "Tuner"), 6), metroLearn(m, MidiArmed::action("@metro", "Metronome"), 6),
        prevLearn(m, MidiArmed::action("@prev", "Previous preset"), 6), nextLearn(m, MidiArmed::action("@next", "Next preset"), 6) {
        undoB.onClick = [&m] { m.undo(); }; undoB.setTooltip("Undo the last change to the rig (Ctrl+Z)");
        hover.add(undoB, "top:undo", "Undo");
        for (auto* c : std::initializer_list<juce::Component*> { &power, &rec, &tuner, &midi, &save, &saveAs, &presetMenu, &metro, &prev, &next, &bpm, &meters, &undoB })
            addAndMakeVisible(c);
        for (auto* o : { &tunerLearn, &metroLearn, &prevLearn, &nextLearn }) addAndMakeVisible(o);
        addChildComponent(wave);
        hover.add(power, "top:power", "Audio on/off"); hover.add(meters, "top:meters", "Levels"); hover.add(rec, "top:rec", "Record");
        hover.add(metro, "top:metro", "Metronome"); hover.add(bpm, "top:metro", "Metronome");
        hover.add(tuner, "top:tuner", "Tuner"); hover.add(midi, "top:midi", "MIDI learn"); hover.add(prev, "top:prev", "Previous preset");
        hover.add(next, "top:next", "Next preset"); hover.add(presetMenu, "top:preset", "Preset"); hover.add(save, "top:save", "Save");
        hover.add(saveAs, "top:saveAs", "Save as"); hover.add(wave, "top:recWave", "Recording");
        power.onClick = [&m] { m.toggle(); };
        rec.onClick = [&m] { m.sessions->toggleRecord(); }; rec.setTooltip("Record dry + wet (Ctrl+R)");
        tuner.onClick = [&m] { m.setTuner(!m.tunerOn); };
        midi.onClick = [&m] { m.midi->setLearning(!m.midi->learning); if (m.midi->learning) m.setScreen(Screen::rig); };
        midi.setTooltip("MIDI learn: click a knob or switch, then move a control on your MIDI device");
        save.onClick = [&m] { m.saveCurrent(); }; save.setTooltip("Save changes to this preset (Ctrl+S)");
        saveAs.setButtonText("Save as" + juce::String::fromUTF8("\xe2\x80\xa6"));
        saveAs.onClick = [&m] { askText("Save preset as", m.presetName.isEmpty() ? juce::String() : m.presetName + " copy", "Save", [&m](juce::String n) { m.savePreset(n); }); };
        saveAs.setTooltip("Save as a new preset, leaving this one unchanged (Ctrl+Shift+S)");
        metro.setTooltip("Metronome and tempo"); prev.setTooltip("Previous preset"); next.setTooltip("Next preset");
        bpm.onClick = [this] { showTempo(); }; bpm.setTooltip("Metronome and tempo");
        presetMenu.onClick = [this] { showPresetMenu(); };
        sync();
    }
    void sync() {
        auto& m = model;
        // the power symbol (U+23FB) isn't in Android's fonts: it shows as an empty box there
        const juce::String sym = onAndroid ? juce::String() : juce::String::fromUTF8("\xe2\x8f\xbb ");
        power.setButtonText(sym + (m.live() ? "On" : "Start"));
        power.setColour(juce::TextButton::buttonOnColourId, m.live() ? juce::Colour(0xff34c759) : m.accent());
        power.setToggleState(true, juce::dontSendNotification);
        rec.setButtonText(m.sessions->recording ? juce::String::fromUTF8("\xe2\x97\x8f ") + mmss(m.sessions->recSeconds) : juce::String::fromUTF8("\xe2\x97\x8f Rec"));
        rec.setColour(juce::TextButton::textColourOffId, m.sessions->recording ? juce::Colours::red : theme::text);
        tuner.setToggleState(m.tunerOn, juce::dontSendNotification);
        midi.setToggleState(m.midi->learning, juce::dontSendNotification);
        metro.setToggleState(m.metroOn, juce::dontSendNotification);
        bpm.setClickingTogglesState(false); bpm.setToggleState(m.metroOn, juce::dontSendNotification);   // lit with the icon while it plays
        bpm.setButtonText(std::fmod(m.bpm, 1.0) == 0 ? juce::String(juce::roundToInt(m.bpm)) : juce::String(m.bpm, 1));
        presetMenu.setButtonText((m.presetModified() ? juce::String::fromUTF8("\xe2\x97\x8f ") : juce::String()) + m.presetName);
        save.setEnabled(m.presetModified()); undoB.setEnabled(m.canUndo());
        wave.setVisible(m.sessions->recording);
        for (auto* o : { &tunerLearn, &metroLearn, &prevLearn, &nextLearn }) o->sync();
        // a longer or shorter preset name can switch between one and two rows: the window lays out again
        if (heightFor(getWidth()) != getHeight()) { if (auto* p = getParentComponent()) p->resized(); }
        resized(); repaint();
    }
    static constexpr int rowH = 64, compactRowH = 50;
    int presetW() const { return juce::jlimit(140, 300, juce::GlyphArrangement::getStringWidthInt(theme::font(10), presetMenu.getButtonText()) + 40); }
    /// One row when everything fits, else two (a phone, a narrow window): see resized().
    bool twoRows(int width) const {
        const int left = 64 + 196 + 80 + 10 + 170 + 10 + 100;   // logo and title, power, meters, latency
        const int right = 80 + 60 + 34 + 26 + presetW() + 26 + 60 + 66 + 50 + 54 + 34 + 84 + 12 * 10 + 14;
        return width < left + right;
    }
    int heightFor(int width) const { return twoRows(width) ? 2 * compactRowH : rowH; }
    void resized() override {
        compact = twoRows(getWidth());
        const int h1 = compact ? compactRowH : rowH;
        int x = 70;
        auto place = [&](juce::Component& c, int w, int h = 28, int top = 0, int rowHeight = 0) {
            const int rh = rowHeight > 0 ? rowHeight : h1;
            c.setBounds(x, top + (rh - h) / 2, w, h); x += w + 10;
        };
        int r = getWidth() - 14;
        auto placeR = [&](juce::Component& c, int w, int h = 28) { r -= w; c.setBounds(r, (h1 - h) / 2, w, h); r -= 10; };
        if (!compact) {
            x = 64 + 196;
            place(power, 80); place(meters, 170, 36); x += 100;   // latency text
            wave.setBounds(x, (h1 - 34) / 2, 200, 34);
            placeR(saveAs, 80);
            placeR(save, 60);
            placeR(undoB, 34);
            placeR(next, 26); nextLearn.setBounds(next.getBounds());
            placeR(presetMenu, presetW());
            placeR(prev, 26); prevLearn.setBounds(prev.getBounds());
            placeR(midi, 60); placeR(tuner, 66); tunerLearn.setBounds(tuner.getBounds());
            placeR(bpm, 54); r += 6; placeR(metro, 34); metroLearn.setBounds(metro.getBounds());   // touching: one control
            placeR(rec, 84);
            latencyX = x - 100;
            return;
        }
        // Two rows. 1: logo, power, meters, latency | undo, save, save as. 2: preset picker, then the performance
        // buttons (record, metronome, tempo, tuner, MIDI learn), with the recording's waveform after them.
        x = 64;
        place(power, 80); place(meters, 170, 36); latencyX = x; x += 100;
        placeR(saveAs, 80); placeR(save, 60); placeR(undoB, 34);
        x = 14;
        const int top = compactRowH;
        place(prev, 26, 28, top); prevLearn.setBounds(prev.getBounds());
        place(presetMenu, presetW(), 28, top);
        place(next, 26, 28, top); nextLearn.setBounds(next.getBounds());
        x += 10;
        place(rec, 84, 28, top); place(metro, 34, 28, top); metroLearn.setBounds(metro.getBounds());
        x -= 6; place(bpm, 54, 28, top);   // touching the icon: one control
        place(tuner, 66, 28, top); tunerLearn.setBounds(tuner.getBounds()); place(midi, 60, 28, top);
        wave.setBounds(x, top + (compactRowH - 34) / 2, std::max(0, std::min(200, getWidth() - 14 - x)), 34);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(theme::bar);
        g.setColour(theme::line); g.fillRect(0, getHeight() - 1, getWidth(), 1);
        const int h1 = compact ? compactRowH : rowH;
        const float logo = compact ? 34.0f : 40.0f;
        drawLogo(g, juce::Rectangle<float>(14, (float) h1 / 2 - logo / 2, logo, logo), model.accent());
        if (!compact) {   // two rows: the logo alone, to leave room for the controls
            g.setColour(theme::text); g.setFont(theme::font(15, true, 4)); g.drawText("LEVIATHAN", 64, 10, 190, 26, juce::Justification::left);
            g.setColour(theme::secondary); g.setFont(theme::font(9, false, 1.5f)); g.drawText("OPEN GUITAR RIG", 64, 36, 160, 16, juce::Justification::left);
        }
        if (model.running && model.info.sampleRate > 0) {
            const double total = model.info.latencyMs() + model.params.pitchLatencyMs.load();
            g.setColour(total > 15 ? juce::Colours::orange : theme::secondary); g.setFont(theme::font(10));
            g.drawText(juce::String::fromUTF8("~") + juce::String(total, 1) + " ms", latencyX, 0, 96, h1, juce::Justification::centredLeft);
        }
    }
    void showPresetMenu() {
        auto& m = model;
        juce::PopupMenu menu;
        const auto recent = m.recentlyPlayed();
        if (!recent.isEmpty()) {
            menu.addSectionHeader("Recently played");
            for (int i = 0; i < std::min(5, recent.size()); ++i) { const auto n = recent[i]; menu.addItem(n, [&m, n] { m.confirmLoad([&m, n] { m.loadAny(n); }); }); }
        }
        if (!m.favorites.isEmpty()) {
            menu.addSectionHeader("Favorites");
            auto fav = m.favorites; fav.sort(true);
            for (const auto& n : fav) menu.addItem(juce::String::fromUTF8("\xe2\x98\x85 ") + n, [&m, n] { m.confirmLoad([&m, n] { m.loadAny(n); }); });
        }
        menu.addSectionHeader("Factory");
        for (const auto& f : FACTORY) { const juce::String n = f.name; menu.addItem(m.userPresets.count(n) ? n + " (edited)" : n, [&m, n] { m.confirmLoad([&m, n] { m.loadFactory(n); }); }); }
        juce::StringArray mine; for (auto& [k, v] : m.userPresets) if (!m.isFactory(k)) mine.add(k);
        mine.sort(false);
        if (!mine.isEmpty()) { menu.addSectionHeader("Yours"); for (const auto& n : mine) menu.addItem(n, [&m, n] { m.confirmLoad([&m, n] { m.loadUser(n); }); }); }
        menu.addSeparator();
        menu.addItem("Save \"" + m.presetName + "\"", [&m] { m.saveCurrent(); });
        menu.addItem("Save as new" + ellipsis, [&m] { askText("Save preset", {}, "Save", [&m](juce::String n) { m.savePreset(n); }); });
        menu.addSeparator();
        menu.addItem("Export \"" + m.presetName + "\"" + ellipsis, m.presetSettings(m.presetName).has_value(), false, [&m] { m.exportPreset(m.presetName); });
        menu.addItem("Import presets" + ellipsis, [&m] { m.importPresetsDialog(); });
        if (m.isFactory(m.presetName) && m.userPresets.count(m.presetName))
            menu.addItem("Restore factory \"" + m.presetName + "\"", [&m] { m.restoreFactory(m.presetName); });
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetMenu));
    }
    void showTempo() {
        struct Tempo : juce::Component {
            explicit Tempo(Model& m) : model(m), tapB(m, "Tap", [&m] { m.tap(); }, MidiArmed::action("@tap", "Tap tempo")) {
                bpmS.setSliderStyle(juce::Slider::IncDecButtons); bpmS.setRange(30, 300, 0.1); bpmS.setValue(m.bpm, juce::dontSendNotification);
                bpmS.setTextValueSuffix(" BPM"); bpmS.onValueChange = [this] { model.setBpm(bpmS.getValue()); };
                metroT.setToggleState(m.metroOn, juce::dontSendNotification); metroT.onClick = [this] { model.setMetro(metroT.getToggleState()); };
                for (int i = 1; i <= 12; ++i) beats.addItem(juce::String(i), i);
                beats.setSelectedId(m.beatsPerBar, juce::dontSendNotification); beats.onChange = [this] { model.setBeatsPerBar(beats.getSelectedId()); };
                vol.setRange(-40, 0, 0.5); vol.setValue(m.metroVol, juce::dontSendNotification); vol.setTextValueSuffix(" dB");
                vol.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22); vol.setViewportIgnoreDragFlag(true); vol.onValueChange = [this] { model.setMetroVol(vol.getValue()); };
                clockT.setButtonText("Follow incoming MIDI clock"); clockT.setToggleState(m.midi->followClock, juce::dontSendNotification);
                clockT.onClick = [this] { model.midi->setFollowClock(clockT.getToggleState()); };
                for (auto* c : std::initializer_list<juce::Component*> { &bpmS, &tapB, &metroT, &beats, &vol, &clockT }) addAndMakeVisible(c);
                setSize(400, 230);
            }
            void resized() override {
                metroT.setBounds(130, 10, 44, 28); bpmS.setBounds(130, 50, 170, 28); tapB.setBounds(310, 50, 70, 28);
                beats.setBounds(130, 88, 80, 28); vol.setBounds(130, 128, 250, 28); clockT.setBounds(130, 168, 250, 26);
            }
            void paint(juce::Graphics& g) override {
                g.setColour(theme::secondary); g.setFont(theme::font(10));
                int y = 10; for (auto* t : { "Metronome", "Tempo", "Beats per bar", "Click volume", "MIDI clock" }) { g.drawText(t, 10, y, 115, 28, juce::Justification::centredLeft); y += y == 10 ? 40 : 39; }
            }
            Model& model; juce::Slider bpmS, vol; ActionButton tapB; SwitchButton metroT { [this] { return model.accent(); } }; juce::ToggleButton clockT; juce::ComboBox beats;
        };
        juce::CallOutBox::launchAsynchronously(std::make_unique<Tempo>(model), metro.getScreenBounds().getUnion(bpm.getScreenBounds()), nullptr);
    }
    Model& model;
    juce::TextButton power, rec, tuner, midi, save, saveAs, presetMenu, undoB;
    IconButton metro, prev, next;
    juce::TextButton bpm;
    Meters meters;
    LearnOverlay tunerLearn, metroLearn, prevLearn, nextLearn;
    int latencyX = 0;
    bool compact = false;
    LiveWave wave { model };
    HoverExplainer hover { model };
    ~TopBar() override { hover.detach(); }
};

// MARK: - presets screen

struct PresetsScreen : Screen_, juce::FileDragAndDropTarget {
    explicit PresetsScreen(Model& m) : model(m), importB("Import" + ellipsis), exportB("Export all" + ellipsis), saveB("Save as new" + ellipsis) {
        filter.addItem("All", 1); filter.addItem(juce::String::fromUTF8("\xe2\x98\x85 Favorites"), 2); filter.setSelectedId(1);
        filter.onChange = [this] { signature.clear(); refresh(); };
        header.onSort = [this](int s) { model.sortPresetsBy(s); signature.clear(); refresh(); };
        addAndMakeVisible(header);
        importB.onClick = [&m] { m.importPresetsDialog(); }; importB.setTooltip("Add .leviathan preset files (or drop them here)");
        exportB.onClick = [&m] { m.exportAllPresets(); }; exportB.setTooltip("Save every preset as a .leviathan file in a folder");
        saveB.onClick = [&m] { askText("Save preset", {}, "Save", [&m](juce::String n) { m.savePreset(n); }); };
        for (auto* c : std::initializer_list<juce::Component*> { &filter, &importB, &exportB, &saveB, &viewport, &search, &tagBar }) addAndMakeVisible(c);
        viewport.setViewedComponent(&list, false); viewport.setScrollBarsShown(true, false);
        search.setTextToShowWhenEmpty("Search presets and tags", theme::secondary);
        search.setFont(theme::font(10.5f)); search.setIndents(10, 0); search.setJustification(juce::Justification::centredLeft);
        search.setColour(juce::TextEditor::backgroundColourId, juce::Colours::white.withAlpha(0.06f));
        search.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.onTextChange = [this] { signature.clear(); refresh(); };
        search.onEscapeKey = [this] { search.clear(); };
        tagBar.onChange = [this] { signature.clear(); refresh(); };
    }
    /// Favourites, the tags picked (all of them), then the search words (each in the name or a tag).
    juce::StringArray names() const {
        auto all = model.sortedPresets(model.presetSort, model.presetSortReversed);
        juce::StringArray words; words.addTokens(search.getText().toLowerCase(), " ", ""); words.removeEmptyStrings();
        juce::StringArray f;
        for (auto& n : all) {
            if (filter.getSelectedId() == 2 && !model.favorites.contains(n)) continue;
            const auto t = model.tags(n);
            bool ok = true;
            for (auto& want : tagBar.picked) ok = ok && t.contains(want);
            const auto hay = (n + " " + t.joinIntoString(" ")).toLowerCase();
            for (auto& w : words) ok = ok && hay.contains(w);
            if (ok) f.add(n);
        }
        return f;
    }
    void refresh() override {
        tagBar.update();
        juce::StringArray sig { names().joinIntoString("|"), model.favorites.joinIntoString("|"), model.presetName, model.presetModified() ? "m" : "", juce::String(model.presetSort) + (model.presetSortReversed ? "r" : "") };
        for (auto& [k, v] : model.userPresets) sig.add(k);
        for (auto& [k, v] : model.presetMeta) sig.add(v.tags.joinIntoString(","));
        if (sig.joinIntoString("/") == signature) return;
        signature = sig.joinIntoString("/");
        rows.clear(); list.removeAllChildren();
        for (const auto& n : names()) { auto r = std::make_unique<Row>(model, n); list.addAndMakeVisible(*r); rows.push_back(std::move(r)); }
        resized();
    }
    void resized() override {
        auto r = getLocalBounds().reduced(14, 0);
        auto head = r.removeFromTop(56).withTrimmedTop(14).withHeight(28);
        filter.setBounds(head.getX() + 110, head.getY(), 200, 28);
        search.setBounds(filter.getRight() + 10, head.getY(), 260, 28);

        saveB.setBounds(head.removeFromRight(120)); head.removeFromRight(8);
        exportB.setBounds(head.removeFromRight(100)); head.removeFromRight(8);
        importB.setBounds(head.removeFromRight(90));
        tagBar.setBounds(r.removeFromTop(tagBar.heightFor(r.getWidth())));
        header.setBounds(r.removeFromTop(24));
        viewport.setBounds(r.withTrimmedBottom(10));
        int y = 0; for (auto& row : rows) { row->setBounds(0, y, viewport.getMaximumVisibleWidth(), 58); y += 58; }
        list.setSize(viewport.getMaximumVisibleWidth(), y);
    }
    void paint(juce::Graphics& g) override { drawSectionTitle(g, "PRESETS", { 14, 14, 100, 28 }); }
    bool isInterestedInFileDrag(const juce::StringArray& files) override {
        for (auto& f : files) if (juce::File(f).hasFileExtension(Model::presetExt)) return true;
        return false;
    }
    void filesDropped(const juce::StringArray& files, int, int) override {
        juce::Array<juce::File> fs; for (auto& f : files) if (juce::File(f).hasFileExtension(Model::presetExt)) fs.add(juce::File(f));
        model.importPresets(fs);
    }
    struct Row : juce::Component {
        Row(Model& m, juce::String n) : model(m), name(std::move(n)),
            star(m.favorites.contains(name) ? IconButton::starFilled : IconButton::star, [this] { model.toggleFavorite(name); }, 16),
            load("Load"), exportB("Export" + ellipsis), rename("Rename"), del("Delete"), restore("Restore factory"), tagsB("Tags" + ellipsis) {
            star.colour = m.favorites.contains(name) ? juce::Colours::yellow : theme::secondary;
            addAndMakeVisible(star);
            load.onClick = [this] { model.confirmLoad([this] { model.loadAny(name); model.setScreen(Screen::rig); }); };
            exportB.onClick = [this] { model.exportPreset(name); };
            rename.onClick = [this] { askText("Rename preset", name, "Rename", [this](juce::String n) { model.renamePreset(name, n); }); };
            del.onClick = [this] { model.deletePreset(name); };
            restore.onClick = [this] { model.restoreFactory(name); };
            tagsB.onClick = [this] {
                askText("Tags for " + name + " (separate with commas)", model.userTags(name).joinIntoString(", "), "Save", [this](juce::String t) { model.setUserTags(name, t); });
            };
            tagsB.setTooltip("Add your own tags. Factory or Yours, the amp and style words in the name are tagged for you.");
            for (auto* b : { &load, &exportB, &tagsB }) addAndMakeVisible(b);
            const bool factory = m.isFactory(name), edited = factory && m.userPresets.count(name);
            if (!factory) { addAndMakeVisible(rename); addAndMakeVisible(del); }
            else if (edited) addAndMakeVisible(restore);
        }
        void resized() override {
            star.setBounds(4, 15, 28, 28);
            int r = getWidth() - 8;
            for (auto* b : { &del, &rename, &restore, &tagsB, &exportB, &load }) {
                if (!b->isVisible()) continue;
                const int w = juce::GlyphArrangement::getStringWidthInt(theme::font(10), b->getButtonText()) + 24;
                r -= w; b->setBounds(r, 15, w, 28); r -= 8;
            }
            statusX = r;
        }
        void paint(juce::Graphics& g) override {
            const bool current = name == model.presetName, factory = model.isFactory(name), edited = factory && model.userPresets.count(name);
            g.setColour(theme::line.withAlpha(0.5f)); g.fillRect(0, getHeight() - 1, getWidth(), 1);
            const auto cols = columns(getWidth());
            g.setColour(theme::text); g.setFont(theme::font(14, current)); g.drawText(name, 44, 8, cols.modified - 54, 24, juce::Justification::centredLeft, true);
            g.setColour(theme::secondary); g.setFont(theme::font(10));
            auto tg = model.tags(name);
            if (edited) tg.set(tg.indexOf("Factory"), juce::String::fromUTF8("Factory \xc2\xb7 edited"));
            juce::String sub = tg.joinIntoString(juce::String::fromUTF8(" \xc2\xb7 "));
            if (current) sub += juce::String::fromUTF8(model.presetModified() ? " \xc2\xb7 loaded \xc2\xb7 unsaved changes" : " \xc2\xb7 loaded");
            g.drawText(sub, 44, 32, cols.modified - 54, 18, juce::Justification::centredLeft, true);
            Model::PresetMeta mt; if (auto it = model.presetMeta.find(name); it != model.presetMeta.end()) mt = it->second;
            g.setFont(theme::font(12));
            g.drawText(Model::stamp(mt.shownModified()), cols.modified, 0, cols.width, getHeight(), juce::Justification::centredLeft);
            g.drawText(Model::stamp(mt.played), cols.played, 0, cols.width, getHeight(), juce::Justification::centredLeft);
        }
        void mouseDoubleClick(const juce::MouseEvent&) override { model.confirmLoad([this] { model.loadAny(name); model.setScreen(Screen::rig); }); }
        Model& model; juce::String name; IconButton star; juce::TextButton load, exportB, rename, del, restore, tagsB; int statusX = 0;
    };
    /// Column positions shared by the header and the rows: dates right of the name, actions at the right edge.
    struct Columns { int modified, played, width; };
    static Columns columns(int w) { const int actions = 450, dw = 170; return { w - actions - 2 * dw, w - actions - dw, dw }; }
    /// Tag chips to narrow the list: tap to filter by a tag (several narrow further), tap again to drop it.
    struct TagBar : juce::Component {
        explicit TagBar(Model& m) : model(m) {}
        juce::StringArray picked; std::function<void()> onChange;
        std::vector<std::pair<juce::String, int>> all;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> chips;
        void update() {
            std::map<juce::String, int> c;
            for (auto& n : model.sortedPresets(4, false)) for (auto& t : model.tags(n)) c[t]++;
            all.assign(c.begin(), c.end());
            std::stable_sort(all.begin(), all.end(), [](auto& a, auto& b) { return a.second > b.second; });
            for (int i = picked.size(); --i >= 0;) if (c.find(picked[i]) == c.end()) picked.remove(i);
            repaint();
        }
        int chipH() const { return onAndroid ? 34 : 24; }
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> layout(int width) const {
            std::vector<std::pair<juce::Rectangle<int>, juce::String>> out;
            int x = 30, y = 0;
            for (auto& [t, n] : all) {
                const int w = juce::GlyphArrangement::getStringWidthInt(theme::font(9), t + "  " + juce::String(n)) + 24;
                if (x > 30 && x + w > width) { x = 30; y += chipH() + 6; }
                out.push_back({ { x, y, w, chipH() }, t }); x += w + 6;
            }
            if (!picked.isEmpty()) out.push_back({ { x, y, 50, chipH() }, {} });   // Clear
            return out;
        }
        int heightFor(int width) const { const auto l = layout(width); return l.empty() ? 0 : l.back().first.getBottom() + 10; }
        void paint(juce::Graphics& g) override {
            chips = layout(getWidth());
            for (auto& [r, t] : chips) {
                if (t.isEmpty()) { g.setColour(theme::secondary); g.setFont(theme::font(9)); g.drawText("Clear", r, juce::Justification::centred); continue; }
                const bool on = picked.contains(t);
                const auto rf = r.toFloat().reduced(0.5f);
                g.setColour(on ? model.accent().withAlpha(0.22f) : juce::Colours::white.withAlpha(0.05f)); g.fillRoundedRectangle(rf, rf.getHeight() / 2);
                g.setColour(on ? model.accent() : juce::Colours::white.withAlpha(0.12f)); g.drawRoundedRectangle(rf, rf.getHeight() / 2, 1);
                int n = 0; for (auto& [tt, c] : all) if (tt == t) n = c;
                g.setFont(theme::font(9));
                g.setColour(theme::text); g.drawText(t, r.withTrimmedLeft(12), juce::Justification::centredLeft);
                g.setColour(theme::secondary); g.drawText(juce::String(n), r.withTrimmedRight(10), juce::Justification::centredRight);
            }
        }
        void mouseUp(const juce::MouseEvent& e) override {
            for (auto& [r, t] : chips) if (r.contains(e.getPosition())) {
                if (t.isEmpty()) picked.clear(); else if (picked.contains(t)) picked.removeString(t); else picked.add(t);
                repaint(); if (onChange) onChange(); return;
            }
        }
        Model& model;
    };
    /// NAME / MODIFIED / LAST PLAYED: click to sort, click again to reverse.
    struct Header : juce::Component {
        explicit Header(Model& m) : model(m) {}
        void paint(juce::Graphics& g) override {
            const auto c = columns(getWidth());
            draw(g, "NAME", 44, 4); draw(g, "MODIFIED", c.modified, 2); draw(g, "LAST PLAYED", c.played, 1);
        }
        void draw(juce::Graphics& g, const juce::String& t, int x, int sort) {
            const bool on = model.presetSort == sort;
            g.setColour(on ? model.accent() : theme::secondary); g.setFont(theme::font(10, true, 1.2f));
            const auto label = t + (on ? (model.presetSortReversed ? juce::String::fromUTF8(" \xe2\x96\xb4") : juce::String::fromUTF8(" \xe2\x96\xbe")) : juce::String());
            g.drawText(label, x, 0, 170, getHeight(), juce::Justification::centredLeft);
        }
        void mouseUp(const juce::MouseEvent& e) override {
            const auto c = columns(getWidth());
            if (e.x >= c.played && e.x < c.played + c.width) onSort(1);
            else if (e.x >= c.modified && e.x < c.modified + c.width) onSort(2);
            else if (e.x < c.modified) onSort(4);
        }
        Model& model; std::function<void(int)> onSort;
    };
    Model& model; Header header { model }; juce::ComboBox filter; juce::TextButton importB, exportB, saveB; juce::Viewport viewport; juce::Component list;
    juce::TextEditor search; TagBar tagBar { model };
    std::vector<std::unique_ptr<Row>> rows; juce::String signature;
};

// MARK: - recordings screen

/// Trim mode for the Recordings screen: the kept range in seconds.

/// Min/max waveform with a seconds ruler. Click to play from that point; drag across it to select a part
/// (model.sessions->selection), whose yellow edges can then be dragged.
struct Waveform : juce::Component {
    explicit Waveform(Model& m) : model(m) { setViewportIgnoreDragFlag(true); }   // drags select and trim, never scroll
    void paint(juce::Graphics& g) override {
        auto& s = *model.sessions;
        const auto* cur = s.current();
        const float w = (float) getWidth(), h = (float) getHeight(), ruler = 22;
        g.fillAll(juce::Colour(0xff171717));
        const double dur = cur ? cur->duration : 0;
        if (dur > 0) {
            double step = 300; for (double st : { 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0 }) if (w / (float) (dur / st) > 50) { step = st; break; }
            g.setFont(theme::font(7));
            for (double t = 0; t <= dur; t += step) {
                const float x = (float) (t / dur) * w;
                g.setColour(juce::Colours::grey); g.fillRect(x, ruler - 6, 1.f, 6.f); g.drawText(mmss(t), (int) x + 3, 0, 60, 16, juce::Justification::centredLeft);
            }
        }
        const auto& wf = s.waveform;
        const size_t n = wf.size() / 2;
        if (n > 0) {
            const float mid = ruler + (h - ruler) / 2, amp = (h - ruler) / 2 - 4;
            g.setColour(model.accent().withAlpha(0.85f));
            for (size_t i = 0; i < n; ++i) {
                const float x = (float) i / (float) n * w, lo = wf[2 * i], hi = wf[2 * i + 1];
                g.fillRect(x, mid - hi * amp, std::max(w / (float) n, 1.f), std::max((hi - lo) * amp, 1.f));
            }
        } else if (cur) { g.setColour(theme::secondary); g.setFont(theme::font(10)); g.drawText(juce::String("loading") + ellipsis, getLocalBounds(), juce::Justification::centred); }
        if (s.playing && dur > 0) { g.setColour(juce::Colours::white); g.fillRect((float) (s.playPos / dur) * w, 0.f, 2.f, h); }
        if (s.selection && dur > 0) {
            const float xs = (float) (s.selection->first / dur) * w, xe = (float) (s.selection->second / dur) * w;
            g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRect(0.f, 0.f, xs, h); g.fillRect(xe, 0.f, w - xe, h);
            for (float x : { xs, xe }) {
                g.setColour(juce::Colours::yellow); g.fillRect(x - 1, 0.f, 2.f, h);
                g.fillRoundedRectangle(x - 6, h / 2 - 17, 12, 34, 3);
            }
        }
    }
    void mouseMove(const juce::MouseEvent& e) override {
        setMouseCursor(handleAt(e.x) != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
    /// -1 start handle, 1 end handle, 0 none
    int handleAt(int x) const {
        auto& s = *model.sessions; auto* cur = s.current();
        if (!s.selection || !cur || cur->duration <= 0) return 0;
        const double w = getWidth();
        if (std::abs(x - s.selection->first / cur->duration * w) < 9) return -1;
        if (std::abs(x - s.selection->second / cur->duration * w) < 9) return 1;
        return 0;
    }
    void mouseDown(const juce::MouseEvent& e) override { dragging = handleAt(e.x); downX = e.x; selecting = false; if (dragging != 0) dragHandle(e); }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragging != 0) { dragHandle(e); return; }
        auto* cur = model.sessions->current();
        if (!cur || cur->duration <= 0 || (!selecting && std::abs(e.x - downX) <= 4)) return;
        selecting = true;   // a drag of more than a few pixels selects from where it started
        const double a = juce::jlimit(0.0, 1.0, (double) downX / getWidth()) * cur->duration, b = juce::jlimit(0.0, 1.0, (double) e.x / getWidth()) * cur->duration;
        model.sessions->selection = std::make_pair(std::min(a, b), std::max(std::min(a, b) + 0.05, std::max(a, b)));
        changed();
    }
    void mouseUp(const juce::MouseEvent& e) override {   // a plain click seeks (and clears a selection it's outside of)
        if (dragging == 0 && !selecting) {
            auto& s = *model.sessions;
            if (auto* cur = s.current(); cur && s.selection) {
                const double t = juce::jlimit(0.0, 1.0, (double) e.x / getWidth()) * cur->duration;
                if (t < s.selection->first || t > s.selection->second) s.selection.reset();
            }
            seekTo(e); changed();
        }
        dragging = 0; selecting = false;
    }
    void dragHandle(const juce::MouseEvent& e) {
        auto& s = *model.sessions; auto* cur = s.current();
        if (!cur || !s.selection) return;
        const double t = juce::jlimit(0.0, 1.0, (double) e.x / getWidth()) * cur->duration;
        if (dragging < 0) s.selection->first = juce::jlimit(0.0, s.selection->second - 0.05, t);
        else s.selection->second = juce::jlimit(s.selection->first + 0.05, cur->duration, t);
        changed();
    }
    void changed() { repaint(); if (onChange) onChange(); }
    int dragging = 0, downX = 0; bool selecting = false;
    std::function<void()> onChange;
    void seekTo(const juce::MouseEvent& e) {
        if (auto* cur = model.sessions->current()) model.sessions->seek(juce::jlimit(0.0, 1.0, (double) e.x / getWidth()) * cur->duration);
    }
    Model& model;
};

struct RecordingsScreen : Screen_ {
    explicit RecordingsScreen(Model& m) : model(m), wave(m),
        prevB(IconButton::prev, [this] { step(-1); }, 14), playB(IconButton::play, [&m] { m.sessions->togglePlay(); }, 18), nextB(IconButton::next, [this] { step(1); }, 14),
        folderB(IconButton::folder, [&m] { m.sessions->reveal(); }, 16), trashB(IconButton::trash, [&m] { m.sessions->remove(); }, 16),
        liveB(""), reampB("Re-amp with current rig"), originalB(""), importB("Import recordings (WAV)" + ellipsis), wetB("Wet"), dryB("Dry") {
        for (auto* b : { &wetB, &dryB }) {   // Wet / Dry: which take the player plays
            b->setClickingTogglesState(true); b->setRadioGroupId(4711); addAndMakeVisible(*b);
            b->setTooltip("Wet: the take through the rig. Dry: your guitar as it went in, before the rig.");
        }
        wetB.setConnectedEdges(juce::Button::ConnectedOnRight); dryB.setConnectedEdges(juce::Button::ConnectedOnLeft);
        wetB.onClick = [&m] { m.sessions->setPlayDry(false); };
        dryB.onClick = [&m] { m.sessions->setPlayDry(true); };
        for (auto* c : std::initializer_list<juce::Component*> { &listView, &wave, &prevB, &playB, &nextB, &nameEd, &folderB, &trashB, &liveB, &reampB, &originalB, &progress, &importB })
            addAndMakeVisible(c);
        listView.setViewedComponent(&listInner, false); listView.setScrollBarsShown(true, false);
        prevB.setTooltip("Previous recording"); nextB.setTooltip("Next recording"); folderB.setTooltip("Show in Explorer"); trashB.setTooltip(canRevealFiles ? "Move to Recycle Bin" : "Delete");
        nameEd.setFont(theme::font(20, true)); nameEd.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        nameEd.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        nameEd.onReturnKey = [this] { model.sessions->rename(nameEd.getText()); };
        nameEd.onFocusLost = [this] { if (auto* c = model.sessions->current(); c && c->name != nameEd.getText()) model.sessions->rename(nameEd.getText()); };
        liveB.onClick = [&m] { m.sessions->toggleLiveReamp(); if (m.sessions->liveReamp) m.setScreen(Screen::rig); };
        reampB.onClick = [&m] { m.sessions->reamp(); };
        originalB.onClick = [&m] { m.sessions->loadPreset(); m.setScreen(Screen::rig); };
        importB.onClick = [&m] { m.sessions->importRecordings(); };
        trimB.setButtonText(juce::String::fromUTF8("\xe2\x9c\x82 Trim")); trimB.setTooltip("Select the whole take, then drag the handles in to cut silence or mistakes (or drag on the waveform)");
        trimB.onClick = [this] { if (auto* c = model.sessions->current()) { model.sessions->selection = std::make_pair(0.0, c->duration); refresh(); } };
        restoreB.setButtonText("Restore untrimmed"); restoreB.setTooltip("Put back the take as it was before trimming");
        restoreB.onClick = [&m] { m.sessions->restoreUntrimmed(); };
        cutB.setButtonText("Trim selection"); cutB.setTooltip("Cut the selected part out of this take (the original is kept, so you can restore it)");
        cutB.onClick = [this] { auto& s = *model.sessions; if (s.selection) { const auto sel = *s.selection; s.selection.reset(); s.cut(sel.first, sel.second); } refresh(); };
        shareB.setButtonText(juce::String::fromUTF8("Share\xe2\x80\xa6")); shareB.setTooltip("Send this take to another app");
        shareB.onClick = [this] {
            juce::PopupMenu menu;
            menu.addItem("Amped, for messages (M4A)", [this] { model.sessions->share(Sessions::ShareKind::ampedForMessages); });
            menu.addItem("Amped, full quality (WAV)", [this] { model.sessions->share(Sessions::ShareKind::ampedWav); });
            menu.addItem("Raw guitar, for re-amping (WAV)", [this] { model.sessions->share(Sessions::ShareKind::rawWav); });
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&shareB));
        };
        applyB.setButtonText("Trim to selection"); applyB.setToggleState(true, juce::dontSendNotification);
        applyB.setTooltip("Keep only the selected part of this take (the original is kept, so you can restore it)");
        applyB.onClick = [this] { auto& s = *model.sessions; if (s.selection) { const auto sel = *s.selection; s.selection.reset(); s.trim(sel.first, sel.second); } refresh(); };
        cancelB.setButtonText("Clear"); cancelB.onClick = [this] { model.sessions->selection.reset(); refresh(); };
        playSelB.setButtonText("Play selection"); playSelB.onClick = [this] { auto& s = *model.sessions; if (s.selection) s.play(s.selection->first, s.selection->second); };
        for (auto* c : std::initializer_list<juce::Component*> { &trimB, &restoreB, &applyB, &cutB, &cancelB, &playSelB, &shareB }) addChildComponent(c);
        wave.onChange = [this] { refresh(); };
    }
    juce::TextButton trimB, restoreB, applyB, cutB, cancelB, playSelB, shareB;
    juce::Rectangle<int> trimText;
    bool trimCuts() const {
        auto& s = *model.sessions; auto* c = s.current();
        return c && s.selection && s.selection->second - s.selection->first >= 0.05 && (s.selection->first > 0.001 || s.selection->second < c->duration - 0.001);
    }
    void step(int d) {
        auto& s = *model.sessions;
        int i = -1; for (int k = 0; k < (int) s.list.size(); ++k) if (s.list[(size_t) k].dir == s.selected) i = k;
        if (i < 0) { if (!s.list.empty()) s.select(s.list[0].dir); return; }
        const int j = i + d;
        if (j >= 0 && j < (int) s.list.size()) s.select(s.list[(size_t) j].dir);
    }
    void refresh() override {
        auto& s = *model.sessions;
        juce::StringArray sig; for (auto& x : s.list) sig.add(x.dir.getFullPathName() + x.name + juce::String(x.duration));
        sig.add(s.selected.getFullPathName());
        if (sig.joinIntoString("|") != listSig) {
            listSig = sig.joinIntoString("|");
            listInner.removeAllChildren(); rows.clear();
            for (auto& x : s.list) {
                auto r = std::make_unique<SessionRow>(model, x.dir); listInner.addAndMakeVisible(*r); rows.push_back(std::move(r));
            }
            if (auto* c = s.current()) nameEd.setText(c->name, false);
        }
        const auto* cur = s.current();
        const bool has = cur != nullptr;
        for (auto* c : std::initializer_list<juce::Component*> { &wave, &prevB, &playB, &nextB, &nameEd, &folderB, &trashB, &liveB, &reampB }) c->setVisible(has);
        folderB.setVisible(has && canRevealFiles);
        playB.icon = s.playing ? IconButton::pause : IconButton::play;
        wetB.setToggleState(!s.playDry, juce::dontSendNotification); dryB.setToggleState(s.playDry, juce::dontSendNotification);
        liveB.setButtonText(s.liveReamp ? "Stop live re-amp" : "Live re-amp");
        reampB.setVisible(has && !s.rendering); progress.setVisible(has && s.rendering.has_value());
        progressValue = s.rendering.value_or(0);
        originalB.setVisible(has && cur->settings.has_value());
        if (cur) originalB.setButtonText("Load \"" + cur->preset + "\" settings");
        importB.setVisible(!has);
        const bool sel = has && s.selection.has_value();
        trimB.setVisible(has); trimB.setEnabled(!sel);
        restoreB.setVisible(has && s.canRestoreUntrimmed());
        for (auto* c : std::initializer_list<juce::Component*> { &applyB, &cutB, &cancelB, &playSelB }) c->setVisible(sel);
        applyB.setEnabled(trimCuts()); cutB.setEnabled(trimCuts());
        shareB.setVisible(has && Sessions::canShare);
        resized(); repaint(); wave.repaint();
    }
    juce::Rectangle<int> detail() const { return getLocalBounds().withTrimmedLeft(300); }
    void resized() override {
        listView.setBounds(getLocalBounds().withWidth(300));
        int y = 0; for (auto& r : rows) { r->setBounds(0, y, listView.getMaximumVisibleWidth(), 52); y += 52; }
        listInner.setSize(listView.getMaximumVisibleWidth(), y);
        auto d = detail();
        wave.setBounds(d.removeFromTop(220));
        if (model.sessions->selection) {
            auto bar = d.removeFromTop(44).reduced(14, 7);
            applyB.setBounds(bar.removeFromRight(150)); bar.removeFromRight(8);
            cutB.setBounds(bar.removeFromRight(130)); bar.removeFromRight(8);
            cancelB.setBounds(bar.removeFromRight(70)); bar.removeFromRight(8);
            playSelB.setBounds(bar.removeFromRight(130));
            trimText = bar;
        }
        auto transport = d.removeFromTop(44);
        playB.setBounds(transport.withSizeKeepingCentre(40, 32)); prevB.setBounds(playB.getBounds().translated(-48, 0)); nextB.setBounds(playB.getBounds().translated(48, 0));
        {   auto wd = transport.reduced(14, 8).removeFromRight(120);
            wetB.setBounds(wd.removeFromLeft(60)); dryB.setBounds(wd); }
        d.reduce(18, 18);
        auto nameRow = d.removeFromTop(40);
        trashB.setBounds(nameRow.removeFromRight(32)); if (canRevealFiles) folderB.setBounds(nameRow.removeFromRight(32));
        if (shareB.isVisible()) { shareB.setBounds(nameRow.removeFromRight(90).reduced(0, 5)); nameRow.removeFromRight(6); }
        trimB.setBounds(nameRow.removeFromRight(80).reduced(0, 5)); nameRow.removeFromRight(6);
        if (restoreB.isVisible()) { restoreB.setBounds(nameRow.removeFromRight(150).reduced(0, 5)); nameRow.removeFromRight(6); }
        nameEd.setBounds(nameRow);
        d.removeFromTop(80);   // info grid (painted)
        auto boxRow = [&](juce::Component& b) { auto r = d.removeFromTop(68); boxes.push_back(r); b.setBounds(r.reduced(12).removeFromRight(240).withSizeKeepingCentre(240, 28)); d.removeFromTop(10); };
        boxes.clear();
        boxRow(liveB);
        boxRow(reampB); progress.setBounds(reampB.getBounds());
        if (originalB.isVisible()) boxRow(originalB);
        importB.setBounds(detail().withSizeKeepingCentre(260, 30).translated(0, 60));
    }
    void paint(juce::Graphics& g) override {
        g.setColour(theme::line); g.fillRect(299, 0, 1, getHeight());
        auto& s = *model.sessions;
        const auto* cur = s.current();
        const auto& sel = s.selection;
        if (cur) { g.setColour(juce::Colour(0xff141414)); g.fillRect(detail().withTrimmedTop(sel ? 264 : 220).withHeight(44)); }
        if (cur && sel) {
            g.setColour(juce::Colour(0xff1c1c1c)); g.fillRect(detail().withTrimmedTop(220).withHeight(44));
            auto tt = [](double t) { t = std::max(t, 0.0); return juce::String((int) t / 60) + ":" + juce::String::formatted("%04.1f", std::fmod(t, 60.0)); };
            g.setColour(theme::text); g.setFont(theme::font(12));
            g.drawText("Selected " + tt(sel->first) + juce::String::fromUTF8(" \xe2\x80\x93 ") + tt(sel->second) + " (" + tt(sel->second - sel->first) + ")"
                       + juce::String::fromUTF8("   \xc2\xb7   drag on the waveform to select, or drag the yellow handles"), trimText, juce::Justification::centredLeft, true);
        }
        if (!cur) {
            g.setColour(theme::text); g.setFont(theme::font(16));
            g.drawText(s.list.empty() ? "No recordings yet" : "Select a recording", detail().withSizeKeepingCentre(500, 30).translated(0, -30), juce::Justification::centred);
            g.setColour(theme::secondary); g.setFont(theme::font(11));
            g.drawFittedText("Press Rec (Ctrl+R) to capture your dry guitar and the amp's output together.\nDry takes can be re-amped through any preset later.",
                             detail().withSizeKeepingCentre(760, 50).translated(0, 14), juce::Justification::centred, 2);
            return;
        }
        auto info = detail().reduced(18, 0).withY(220 + (model.sessions->selection ? 44 : 0) + 44 + 18 + 48).withHeight(70);
        g.setFont(theme::font(11));
        auto line = [&](int i, const juce::String& k, const juce::String& v) {
            g.setColour(theme::secondary); g.drawText(k, info.getX(), info.getY() + i * 22, 100, 20, juce::Justification::centredLeft);
            g.setColour(theme::text); g.drawText(v, info.getX() + 110, info.getY() + i * 22, 600, 20, juce::Justification::centredLeft);
        };
        line(0, "Recorded", cur->date.toString(true, true, false));
        line(1, "Duration", mmss(cur->duration));
        line(2, "Preset", cur->preset.isEmpty() ? juce::String::fromUTF8("\xe2\x80\x94") : cur->preset + (cur->source.isNotEmpty() ? " (" + cur->source + ")" : juce::String()));
        const char* titles[] = { "Live re-amp", "Re-amp", "Original rig" };
        const juce::String descs[] = {
            "Play this take's dry guitar through the rig in a loop while you change settings in real time.",
            juce::String::fromUTF8("Render the dry take through the current rig (\xe2\x80\x9c") + model.presetName + juce::String::fromUTF8("\xe2\x80\x9d) into a new wet take. The first original is kept as wet-original.wav."),
            "Load the exact settings this take was recorded with." };
        for (size_t i = 0; i < boxes.size() && i < 3; ++i) {
            boxBackground(g, boxes[i].toFloat(), 10);
            auto r = boxes[i].reduced(12).withTrimmedRight(260);
            g.setColour(theme::text); g.setFont(theme::font(12, true)); g.drawText(titles[i], r.removeFromTop(20), juce::Justification::centredLeft);
            g.setColour(theme::secondary); g.setFont(theme::font(10)); g.drawFittedText(descs[i], r, juce::Justification::topLeft, 2);
        }
    }
    struct SessionRow : juce::Component {
        SessionRow(Model& m, juce::File d) : model(m), dir(std::move(d)) {}
        void paint(juce::Graphics& g) override {
            const Session* s = nullptr; for (auto& x : model.sessions->list) if (x.dir == dir) s = &x;
            if (!s) return;
            if (model.sessions->selected == dir) { g.setColour(model.accent().withAlpha(0.25f)); g.fillRoundedRectangle(getLocalBounds().reduced(6, 2).toFloat(), 6); }
            g.setColour(theme::text); g.setFont(theme::font(13)); g.drawText(s->name, 14, 6, getWidth() - 80, 22, juce::Justification::centredLeft);
            g.setColour(theme::secondary); g.setFont(theme::font(9)); g.drawText(s->date.toString(true, true, false), 14, 28, getWidth() - 80, 16, juce::Justification::centredLeft);
            g.setFont(theme::font(11)); g.drawText(mmss(s->duration), getWidth() - 70, 0, 56, getHeight(), juce::Justification::centredRight);
        }
        void mouseUp(const juce::MouseEvent& e) override {
            if (e.mods.isPopupMenu()) {
                juce::PopupMenu menu;
                if (canRevealFiles) {
                    menu.addItem("Show in Explorer", [this] { dir.getChildFile("wet.wav").revealToUser(); });
                    menu.addSeparator();
                }
                menu.addItem(canRevealFiles ? "Move to Recycle Bin" : "Delete", [this] { model.sessions->remove(dir); });
                menu.showMenuAsync({});
                return;
            }
            model.sessions->select(dir);
        }
        Model& model; juce::File dir;
    };
    Model& model; juce::Viewport listView; juce::Component listInner; std::vector<std::unique_ptr<SessionRow>> rows; juce::String listSig;
    Waveform wave; IconButton prevB, playB, nextB, folderB, trashB; juce::TextEditor nameEd;
    juce::TextButton wetB, dryB;
    juce::TextButton liveB, reampB, originalB, importB;
    double progressValue = 0; juce::ProgressBar progress { progressValue };
    std::vector<juce::Rectangle<int>> boxes;
};

// MARK: - settings screen
//
// Same sections and grid as the Mac app (mac/Sources/Shell.swift SettingsView): one card per section, sub-headings, a
// fixed label column and every control starting at the same x. Hovering a row explains it in the strip at the bottom.

namespace settingsLayout {
constexpr int labelX = 20, labelW = 170, ctrlX = 200, rowH = 30, rowGap = 8, top = 60;
}

/// One settings section: title, sub-headings with rules, row labels; rows explain themselves on hover.
struct Group : juce::Component {
    struct Row { int y, h; juce::String label, key; };
    Group(Model& m, juce::String t) : model(m), title(std::move(t)) { addMouseListener(&hover, true); hover.owner = this; }
    void clearLayout() { rows.clear(); heads.clear(); rules.clear(); }
    void paint(juce::Graphics& g) override {
        using namespace settingsLayout;
        for (auto& r : rows)   // the row a search result pointed at
            if (highlight.isNotEmpty() && r.label == highlight) {
                g.setColour(model.accent().withAlpha(0.18f));
                g.fillRoundedRectangle(juce::Rectangle<int>(labelX - 8, r.y - 3, getWidth() - 2 * labelX + 16, std::min(r.h, rowH) + 6).toFloat(), 7);
            }
        g.setColour(theme::text); g.setFont(theme::font(22, true)); g.drawText(title, labelX, 8, 400, 40, juce::Justification::centredLeft);
        g.setColour(theme::line); for (int y : rules) g.fillRect(labelX, y, getWidth() - 2 * labelX, 1);
        g.setColour(theme::secondary); g.setFont(theme::font(9, true, 1.2f));
        for (auto& [y, t] : heads) g.drawText(t.toUpperCase(), labelX, y, 400, 18, juce::Justification::centredLeft);
        g.setFont(theme::font(10.5f));
        for (auto& r : rows) if (r.label.isNotEmpty()) g.drawText(r.label, labelX, r.y, labelW, std::min(r.h, rowH), juce::Justification::centredLeft);
        if (extra) extra(g);
    }
    /// Lays rows out top to bottom: heading("Devices"), then row("Input", "inputDevice") returns the row's y.
    struct Lay {
        Group& g; int y = settingsLayout::top;
        void heading(const juce::String& t) {
            if (!g.heads.empty() || !g.rows.empty()) { g.rules.push_back(y + 4); y += 18; }
            g.heads.push_back({ y, t }); y += 26;
        }
        int row(const juce::String& label, const juce::String& key, int h = settingsLayout::rowH) {
            g.rows.push_back({ y, h, label, key }); const int r = y; y += h + settingsLayout::rowGap; return r;
        }
        int end() const { return y + 12; }
    };
    /// Places controls left to right from the control column, vertically centred in a row; returns the x after them.
    static int place(int y, std::initializer_list<std::pair<juce::Component*, int>> items, int x = settingsLayout::ctrlX) {
        for (auto& [c, w] : items) { c->setBounds(x, y + (settingsLayout::rowH - 28) / 2, w, 28); x += w + 8; }
        return x;
    }
    using Items = std::vector<std::pair<juce::Component*, int>>;
    static int place(int y, const Items& items, int x = settingsLayout::ctrlX) {
        for (auto& [c, w] : items) { c->setBounds(x, y + (settingsLayout::rowH - 28) / 2, w, 28); x += w + 8; }
        return x;
    }
    /// `before`, then an "Open folder" button where the system can show folders (not Android), then `after`.
    static Items withOpen(Items before, juce::Component* open, int w, const Items& after = {}) {
        if (canRevealFiles) before.push_back({ open, w });
        before.insert(before.end(), after.begin(), after.end());
        return before;
    }
    struct Hover : juce::MouseListener {
        Group* owner = nullptr; juce::String shown;
        void mouseMove(const juce::MouseEvent& e) override {
            const auto p = e.getEventRelativeTo(owner).getPosition();
            juce::String key, label;
            for (auto& r : owner->rows) if (p.y >= r.y - 4 && p.y < r.y + r.h + 4 && r.key.isNotEmpty()) { key = r.key; label = r.label.isNotEmpty() ? r.label : owner->title; }
            if (key == shown) return;
            if (shown.isNotEmpty()) owner->model.clearHover("settings:" + shown);
            shown = key;
            if (key.isNotEmpty()) owner->model.showHover("settings:" + key, label, Model::explain(key));
            if (owner->onHover) owner->onHover();
        }
        void mouseExit(const juce::MouseEvent& e) override {
            if (owner->getLocalBounds().contains(e.getEventRelativeTo(owner).getPosition())) return;   // moved onto a child
            if (shown.isNotEmpty()) owner->model.clearHover("settings:" + shown);
            shown.clear();
            if (owner->onHover) owner->onHover();
        }
    } hover;
    Model& model;
    juce::String title;
    std::vector<Row> rows;
    std::vector<std::pair<int, juce::String>> heads;
    std::vector<int> rules;
    std::function<void(juce::Graphics&)> extra;   // values painted next to the controls
    std::function<void()> onHover;
    juce::String highlight;   // a row's label, briefly, after a search jumped to it
    /// The heading a row sits under ("" before the first heading).
    juce::String headingFor(int y) const {
        juce::String h;
        for (auto& [hy, t] : heads) if (hy <= y) h = t;
        return h;
    }
};

/// JUCE's device panel draws its own labels right-aligned beside each control; this hides them so the section paints
/// them in its label column like every other row.
struct SelectorLookAndFeel : LookAndFeel {
    void drawLabel(juce::Graphics& g, juce::Label& l) override { if (l.getAttachedComponent() == nullptr) LookAndFeel::drawLabel(g, l); }
};

/// A colour dot; ringed when it's the picked accent.
struct Swatch : juce::Component {
    Swatch(Model& m, juce::Colour c) : model(m), colour(c) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(2);
        g.setColour(colour); g.fillEllipse(r);
        if (model.accentPick && *model.accentPick == colour) { g.setColour(juce::Colours::white.withAlpha(0.9f)); g.drawEllipse(r.reduced(1), 2); }
    }
    void mouseUp(const juce::MouseEvent&) override { model.setAccent(colour); }
    Model& model; juce::Colour colour;
};

/// Bug reports and feature requests: GitHub issue forms, prefilled (you see and edit them before submitting).
void openFeedback(Model& m, const juce::String& templ) {
    juce::URL u(juce::String("https://github.com/") + Updater::repo + "/issues/new");
    u = u.withParameter("template", templ).withParameter("version", Updater::current())
         .withParameter("os", juce::SystemStats::getOperatingSystemName());
    if (templ == "bug_report.yml" && m.info.sampleRate > 0)
        u = u.withParameter("audio", m.info.inName + " in, " + m.info.outName + " out, " + m.info.type + ", "
                                         + juce::String(m.info.sampleRate / 1000, 1) + " kHz, " + juce::String(m.info.buffer) + " samples");
    u.launchInDefaultBrowser();
}

struct SettingsScreen : Screen_ {
    explicit SettingsScreen(Model& m) : model(m),
        audioSel(m.devices, 0, 8, 2, 2, false, false, false, false),
        audio(m, "Audio"), mid(m, "MIDI"), lib(m, "Library"), look(m, "Appearance"), about(m, "About"),
        strip(m, "Hover over a setting for an explanation.") {
        addAndMakeVisible(viewport); viewport.setViewedComponent(&inner, false); viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(nav); addChildComponent(strip);
        section = current(m.props().getValue("settingsSection", "Audio"));
        for (auto* g : groups()) { inner.addAndMakeVisible(g); g->onHover = [this] { strip.repaint(); }; }
        const juce::String site = juce::String("https://github.com/") + Updater::repo;

        // audio
        audioSel.setLookAndFeel(&selLnf);
        audio.addAndMakeVisible(audioSel);
        audio.addAndMakeVisible(inputCh); audio.addAndMakeVisible(audioInfo);
        inputCh.onChange = [this] { if (inputCh.getSelectedId() > 0) model.set("inputChannel", inputCh.getSelectedId() - 1); };
        m.devices.addChangeListener(&audioWatch);
        // Restart only when the setup itself changed (picked in the selector above). The manager also broadcasts
        // when the device opens or closes, which start()/stop() do on Android; restarting on those looped forever.
        deviceSetup = model.deviceSignature();
        audioWatch.fn = [this] {
            if (const auto now = model.deviceSignature(); now != deviceSetup) { deviceSetup = now; model.restartAudio(); }
            signature.clear(); refresh();
        };
        // output EQ: the output in use keeps its own (Model::OutputEQ)
        outEqT.onClick = [this] { auto e = model.outputEQ; e.on = outEqT.getToggleState(); model.setOutputEQ(e); resized(); };
        audio.addAndMakeVisible(outEqT);
        lowLagT.onClick = [this] { model.set("pitchLowLag", lowLagT.getToggleState()); };
        audio.addAndMakeVisible(lowLagT);
        for (int i = 0; i < 4; ++i) {
            auto& sl = outEqS[(size_t) i];
            sl.setSliderStyle(juce::Slider::LinearHorizontal); sl.setRange(-12, i == 3 ? 6 : 12, 0.5);
            sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22); sl.setTextValueSuffix(" dB");
            sl.setDoubleClickReturnValue(true, 0); sl.setViewportIgnoreDragFlag(true);
            sl.onValueChange = [this, i] {
                auto e = model.outputEQ; const double v = outEqS[(size_t) i].getValue();
                (i == 0 ? e.low : i == 1 ? e.mid : i == 2 ? e.high : e.level) = v;
                model.setOutputEQ(e);
            };
            audio.addAndMakeVisible(sl);
        }
        a4.setSliderStyle(juce::Slider::IncDecButtons); a4.setRange(430, 450, 1); a4.setTextValueSuffix(" Hz");
        a4.onValueChange = [this] { model.setA4(a4.getValue()); };
        for (size_t i = 0; i < TUNINGS.size(); ++i) tuning.addItem(tuningLabel(TUNINGS[i]), (int) i + 1);
        tuning.onChange = [this] { model.setTuningIndex(tuning.getSelectedId() - 1); };
        audio.addAndMakeVisible(a4); audio.addAndMakeVisible(tuning);
        namCalT.setButtonText("Match each capture's input level");
        namCalT.onClick = [this] { model.setNamCalibrate(namCalT.getToggleState()); };
        ifaceLvl.setSliderStyle(juce::Slider::IncDecButtons); ifaceLvl.setRange(-10, 30, 0.5); ifaceLvl.setTextValueSuffix(" dBu");
        ifaceLvl.setNumDecimalPlacesToDisplay(1);
        ifaceLvl.onValueChange = [this] { model.setInterfaceLevel(ifaceLvl.getValue()); };
        audio.addAndMakeVisible(namCalT); audio.addChildComponent(ifaceLvl);

        // midi
        pcT.setButtonText("Program changes load presets");
        pcT.onClick = [this] { model.midi->setProgramChange(pcT.getToggleState()); };
        learnB.setButtonText("Learn on the rig" + ellipsis); learnB.onClick = [this] { model.setScreen(Screen::rig); model.midi->setLearning(true); };
        clearB.setButtonText("Clear all"); clearB.onClick = [this] { model.midi->clear(); };
        for (auto* c : std::initializer_list<juce::Component*> { &pcT, &learnB, &clearB }) mid.addAndMakeVisible(c);

        // library
        importRec.setButtonText("Import WAV" + ellipsis); importRec.onClick = [this] { model.sessions->importRecordings(); };
        openRec.setButtonText("Open folder"); openRec.onClick = [] { Sessions::root().createDirectory(); Sessions::root().startAsProcess(); };
        // where takes are recorded: any folder, a cloud drive's included (Google Drive, Dropbox and iCloud mount as one)
        changeRec.setButtonText("Change folder" + ellipsis); changeRec.onClick = [this] { chooseRecordingsFolder(); };
        resetRec.setButtonText("Use default"); resetRec.onClick = [this] { askMoveRecordings(Sessions::defaultRoot()); };
        addNam.setButtonText("Add" + ellipsis); addNam.onClick = [this] { model.importNamDialog(); };
        openNam.setButtonText("Open folder"); openNam.onClick = [] { Model::namDir().createDirectory(); Model::namDir().startAsProcess(); };
        rescanNam.setButtonText("Rescan"); rescanNam.onClick = [this] { model.rescanNam(); };
        addIRs.setButtonText("Add" + ellipsis); addIRs.onClick = [this] { model.importCabIRsDialog(); };
        openIRs.setButtonText("Open folder"); openIRs.onClick = [] { CabIRLibrary::root().createDirectory(); CabIRLibrary::root().startAsProcess(); };
        rescanIRs.setButtonText("Rescan"); rescanIRs.onClick = [this] { model.cabIRs.scan(); model.changed(); };
        openCap.setButtonText("Open folder"); openCap.onClick = [] { Capture::dir().createDirectory(); Capture::dir().startAsProcess(); };
        rescan.setButtonText("Rescan"); rescan.onClick = [this] { model.reloadCaptures(); };
        openIR.setButtonText("Open folder"); openIR.onClick = [] { IRLibrary::root().createDirectory(); IRLibrary::root().startAsProcess(); };
        if (!canRevealFiles) {   // Android: no folder to open, so God's Cab is added from a picked folder instead
            openIR.setButtonText("Add" + ellipsis);
            openIR.onClick = [this] {
                auto chooser = std::make_shared<juce::FileChooser>("Choose the God's Cab folder (the one holding 44.1 and 48)", juce::File());
                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, chooser](const juce::FileChooser& fc) {
                    const auto picked = pickedFiles(fc);
                    if (picked.isEmpty()) return;
                    const int n = IRLibrary::importPack(picked);
                    model.irLib = IRLibrary(); model.irLib.scan(model.engineRate()); model.updateIR();
                    model.status = n == 0 ? juce::String("No God's Cab \"44.1\" or \"48\" folders found there.")
                                          : "Added " + juce::String(n) + " God's Cab IRs.";
                    model.changed();
                });
            };
        }
        for (auto* c : std::initializer_list<juce::Component*> { &importRec, &openRec, &changeRec, &resetRec, &addNam, &openNam, &rescanNam, &addIRs, &openIRs, &rescanIRs, &openCap, &rescan, &openIR })
            lib.addAndMakeVisible(c);
        if (!canRevealFiles) for (auto* c : { &openRec, &openNam, &openIRs, &openCap }) c->setVisible(false);

        // appearance
        static const juce::Colour swatches[] = { juce::Colour::fromFloatRGBA(1, 0.3f, 0.3f, 1), juce::Colour::fromFloatRGBA(0.91f, 0.57f, 0.23f, 1),
            juce::Colour::fromFloatRGBA(0.95f, 0.8f, 0.25f, 1), juce::Colour::fromFloatRGBA(0.35f, 0.8f, 0.45f, 1), juce::Colour::fromFloatRGBA(0.35f, 0.7f, 1, 1),
            juce::Colour::fromFloatRGBA(0.6f, 0.45f, 1, 1), juce::Colour::fromFloatRGBA(1, 0.45f, 0.75f, 1), juce::Colour::fromFloatRGBA(0.85f, 0.85f, 0.85f, 1) };
        for (auto c : swatches) { auto b = std::make_unique<Swatch>(m, c); look.addAndMakeVisible(*b); swatchButtons.push_back(std::move(b)); }
        followAmp.setButtonText("Follow amp channel"); followAmp.onClick = [this] { model.setAccent(std::nullopt); };
        zoomMinus.setButtonText(juce::String::fromUTF8("\xe2\x88\x92")); zoomMinus.onClick = [this] { model.zoomStep(-1); };
        zoomPlus.setButtonText("+"); zoomPlus.onClick = [this] { model.zoomStep(1); };
        explain.setButtonText("Explain controls on hover");
        explain.onClick = [this] { model.setExplainOnHover(explain.getToggleState()); };
        for (auto* c : std::initializer_list<juce::Component*> { &followAmp, &zoomMinus, &zoomPlus, &explain }) look.addAndMakeVisible(c);

        // about: updates, feedback, license
        checkNow.setButtonText("Check now"); checkNow.onClick = [this] { model.updater->check(); };
        autoUpd.setButtonText("Check for updates when Leviathan opens"); autoUpd.onClick = [this] { model.updater->setAutoCheck(autoUpd.getToggleState()); };
        newsB.setButtonText(juce::String::fromUTF8("What\xe2\x80\x99s new")); newsB.onClick = [this] { model.updater->showNotes = true; model.changed(); };
        installB.setButtonText("Update now"); installB.setToggleState(true, juce::dontSendNotification); installB.onClick = [this] { model.updater->install(); };
        pageB.setButtonText("View on GitHub"); pageB.onClick = [this] { if (model.updater->release) model.updater->release->page.launchInDefaultBrowser(); };
        bugB.setButtonText("Report a bug" + ellipsis); bugB.onClick = [this] { openFeedback(model, "bug_report.yml"); };
        featB.setButtonText("Request a feature" + ellipsis); featB.onClick = [this] { openFeedback(model, "feature_request.yml"); };
        licB.setButtonText("License"); licB.onClick = [site] { juce::URL(site + "/blob/main/LICENSE").launchInDefaultBrowser(); };
        noticeB.setButtonText("Notice & credits"); noticeB.onClick = [site] { juce::URL(site + "/blob/main/NOTICE.md").launchInDefaultBrowser(); };
        srcB.setButtonText("Source code"); srcB.onClick = [site] { juce::URL(site).launchInDefaultBrowser(); };
        // diagnostic log: off by default; shared to attach to a bug report
        debugT.setButtonText("Record a diagnostic log");
        debugT.onClick = [this] { model.setDebugLog(debugT.getToggleState()); };
        shareLogB.setButtonText(onAndroid ? "Share log" + ellipsis : juce::String("Show log"));
        shareLogB.onClick = [this] {
            const auto f = debuglog::latest();
            if (f == juce::File()) { model.status = "There's no diagnostic log yet: turn on \"Record a diagnostic log\" first."; model.changed(); return; }
            if (onAndroid) logShare = juce::ContentSharer::shareFilesScoped({ juce::URL(f) }, [](bool, const juce::String&) {});
            else f.revealToUser();
        };
        copyLogB.setButtonText("Copy log");
        copyLogB.onClick = [this] {
            const auto f = debuglog::latest();
            if (f == juce::File()) return;
            juce::SystemClipboard::copyTextToClipboard(f.loadFileAsString());
            model.status = "The diagnostic log is on the clipboard: paste it into a bug report or a message."; model.changed();
        };
        for (auto* c : std::initializer_list<juce::Component*> { &checkNow, &updProgress, &autoUpd, &newsB, &installB, &pageB, &bugB, &featB, &licB, &noticeB, &srcB, &debugT, &shareLogB, &copyLogB })
            about.addAndMakeVisible(c);
        refresh();
    }
    ~SettingsScreen() override { model.devices.removeChangeListener(&audioWatch); audioSel.setLookAndFeel(nullptr); }

    std::array<Group*, 5> groups() { return { &audio, &mid, &lib, &look, &about }; }   // an array: an initializer_list would dangle

    /// Sections from before 0.4.9 and where they live now.
    static juce::String current(const juce::String& saved) {
        if (saved == "Tuner") return "Audio";
        if (saved == "Updates" || saved == "Help & feedback") return "About";
        for (auto* n : Nav::items) if (saved == n) return saved;
        return "Audio";
    }

    juce::String deviceSetup;   // model.deviceSignature() when audioWatch last restarted audio
    struct Watch : juce::ChangeListener { std::function<void()> fn; void changeListenerCallback(juce::ChangeBroadcaster*) override { if (fn) fn(); } } audioWatch;

    void refresh() override {
        if (model.settingsJump.isNotEmpty()) {   // "Click to resolve" on a banner: open the section that fixes it
            section = current(model.settingsJump); model.settingsJump = {};
            nav.repaint(); resized(); viewport.setViewPosition(0, 0);
        }
        auto& m = model;
        juce::StringArray sig;
        if (auto* d = m.devices.getCurrentAudioDevice()) sig.add(d->getName() + juce::String(d->getInputChannelNames().size()));
        sig.add(juce::String((int) m.midi->maps.size())); for (auto& mp : m.midi->maps) sig.add(mp.id + (mp.momentary ? "m" : ""));
        sig.add(juce::String((int) m.updater->phase)); sig.add(m.updater->release ? m.updater->release->version : juce::String());
        if (sig.joinIntoString("|") != signature) {
            signature = sig.joinIntoString("|");
            inputCh.clear(juce::dontSendNotification);
            if (auto* d = m.devices.getCurrentAudioDevice()) {
                const auto names = d->getInputChannelNames();
                for (int i = 0; i < std::min(8, names.size()); ++i) inputCh.addItem("Channel " + juce::String(i + 1) + (names[i].isNotEmpty() ? " (" + names[i] + ")" : juce::String()), i + 1);
            }
            mapRows.clear();
            for (auto& mp : m.midi->maps) { auto r = std::make_unique<MapRow>(m, mp); mid.addAndMakeVisible(*r); mapRows.push_back(std::move(r)); }
        }
        selLnf.setAccent(m.accent());
        nav.query.setColour(juce::TextEditor::focusedOutlineColourId, m.accent());
        inputCh.setSelectedId(m.s().i("inputChannel") + 1, juce::dontSendNotification);
        a4.setValue(m.a4, juce::dontSendNotification);
        outEqT.setToggleState(m.outputEQ.on, juce::dontSendNotification);
        lowLagT.setToggleState(m.s().b("pitchLowLag"), juce::dontSendNotification);
        { const double v[] = { m.outputEQ.low, m.outputEQ.mid, m.outputEQ.high, m.outputEQ.level };
          for (int i = 0; i < 4; ++i) outEqS[(size_t) i].setValue(v[i], juce::dontSendNotification); }
        namCalT.setToggleState(m.namCalibrate, juce::dontSendNotification);
        ifaceLvl.setValue(m.interfaceLevel, juce::dontSendNotification);
        tuning.setSelectedId(m.tuningIndex + 1, juce::dontSendNotification);
        followAmp.setEnabled(m.accentPick.has_value());
        autoUpd.setToggleState(m.updater->autoCheck, juce::dontSendNotification);
        explain.setToggleState(m.explainOnHover, juce::dontSendNotification);
        checkNow.setEnabled(!m.updater->busy());
        progressValue = m.updater->progressValue;
        pcT.setToggleState(m.midi->programChange, juce::dontSendNotification);
        clearB.setEnabled(!m.midi->maps.empty());
        resized(); repaint();
        for (auto* g : groups()) g->repaint();
    }
    /// Nav and content as one block, centred on wide windows instead of hugging the sidebar.
    /// The section menu sits against the app's sidebar, a quarter of the width; the section fills the rest.
    juce::Rectangle<int> block() const { return { 0, 0, getWidth(), getHeight() - (strip.isVisible() ? 44 : 0) }; }
    int navWidth() const { return juce::jlimit(220, 320, getWidth() / 4); }
    void resized() override {
        using namespace settingsLayout;
        strip.setVisible(model.explainOnHover);
        if (strip.isVisible()) strip.setBounds(getLocalBounds().removeFromBottom(44));
        const auto b = block();
        nav.setBounds(b.getX(), b.getY(), navWidth(), b.getHeight());
        viewport.setBounds(nav.getRight(), b.getY(), b.getWidth() - nav.getWidth(), b.getHeight());
        const int w = std::min(viewport.getMaximumVisibleWidth() - 56, 1000), x = 28;
        for (auto* g : groups()) { g->clearLayout(); g->setVisible(g->title == section); }
        auto fit = [&](Group& g, const Group::Lay& lay) { g.setBounds(x, 24, w, lay.end()); };

        // AUDIO: JUCE's device panel puts its controls at 35% of its width, so this width lines them up with ctrlX
        {
            Group::Lay lay { audio };
            lay.heading("Devices");
            const int selW = (int) std::round((ctrlX - labelX) / 0.35);
            audioSel.setBounds(labelX, lay.y - 15, selW, 800);   // it shrinks itself to fit its controls; JUCE starts them 15 px down
            lay.y += audioSel.getHeight() - 15 - 24 + rowGap;   // JUCE ends its panel with an empty row
            const bool channels = inputCh.getNumItems() > 1;
            inputCh.setVisible(channels);
            if (channels) Group::place(lay.row("Input channel", "inputChannel"), { { &inputCh, 320 } });
            lay.heading("Output EQ");
            outEqT.setButtonText("Correct " + (model.outputEQKey.isNotEmpty() ? model.outputEQKey : juce::String("this output")));
            Group::place(lay.row("Output EQ", "settings:outputEQ"), { { &outEqT, 420 } });
            static const char* eqLabels[] = { "Low", "Mid", "High", "Level" };
            for (int i = 0; i < 4; ++i) {
                outEqS[(size_t) i].setVisible(model.outputEQ.on);
                if (model.outputEQ.on) Group::place(lay.row(eqLabels[i], "settings:outputEQ"), { { &outEqS[(size_t) i], 320 } });
            }
            lay.heading("Speed");
            const int ly = lay.row("Latency", "settings:latency", 46);
            const int ey = lay.row("Engine", "settings:engine");
            audioInfo.setBounds(ctrlX, ly, w - ctrlX - labelX, ey + rowH - ly);
            audioInfo.engineY = ey - ly;
            lay.heading("Amp captures");
            Group::place(lay.row("Calibrate input", "settings:namCalibrate"), { { &namCalT, 420 } });
            ifaceLvl.setVisible(model.namCalibrate);
            int nowY = -1;
            if (model.namCalibrate) {
                Group::place(lay.row("Interface level", "settings:interfaceLevel"), { { &ifaceLvl, 160 } });
                nowY = lay.row("Now", "settings:namCalibrate", model.s().b("ampBOn") && model.s().s("ampBNam").isNotEmpty() ? 46 : rowH);
            }
            lay.heading("Pitch shifter");
            Group::place(lay.row("Low lag on chords", "pitchLowLag"), { { &lowLagT, 420 } });
            lay.heading("Tuner");
            Group::place(lay.row("Reference", "settings:a4"), { { &a4, 160 } });
            Group::place(lay.row("Guitar tuning", "settings:tuning"), { { &tuning, 320 } });
            audio.extra = [this, nowY](juce::Graphics& g) {   // JUCE's own labels for its device panel, in our label column
                if (nowY >= 0) {
                    g.setColour(theme::secondary); g.setFont(theme::font(11));
                    auto& m = model;
                    juce::StringArray lines { calibrationLine("Amp", m.s().s("nam"), m.namReady(), m.namInputLevel, m.namTrim) };
                    if (m.s().b("ampBOn") && m.s().s("ampBNam").isNotEmpty())
                        lines.add(calibrationLine("Amp B", m.s().s("ampBNam"), m.namReadyB(), m.namInputLevelB, m.namTrimB));
                    for (int i = 0; i < lines.size(); ++i) g.drawText(lines[i], ctrlX, nowY + 2 + i * 20, 700, 18, juce::Justification::centredLeft);
                }
                std::function<void(juce::Component&)> scan = [&](juce::Component& c) {
                    for (auto* ch : c.getChildren()) {
                        if (auto* l = dynamic_cast<juce::Label*>(ch); l && l->getAttachedComponent() && l->getAttachedComponent()->isVisible()) {
                            auto* at = l->getAttachedComponent();
                            const auto r = audio.getLocalArea(at->getParentComponent(), at->getBounds());
                            g.drawText(l->getText().trimCharactersAtEnd(":"), labelX, r.getY(), labelW, std::min(r.getHeight(), rowH), juce::Justification::centredLeft);
                        }
                        scan(*ch);
                    }
                };
                g.setColour(theme::secondary); g.setFont(theme::font(11));
                scan(audioSel);
            };
            fit(audio, lay);
        }
        // MIDI
        {
            Group::Lay lay { mid };
            lay.heading("Connection");
            const int dy = lay.row("Devices", "settings:midiDevices");
            const int my = lay.row("Last message", "settings:midiLast");
            lay.heading("Presets");
            Group::place(lay.row("Program change", "settings:programChange"), { { &pcT, 420 } });
            lay.heading("Assignments");
            const int ay = lay.row("Assign", "settings:midiAssign");
            const int after = Group::place(ay, { { &learnB, 160 }, { &clearB, 100 } });
            for (auto& r : mapRows) r->setBounds(labelX, lay.row("", "settings:midiAssign"), w - 2 * labelX, rowH);
            mid.extra = [this, dy, my, ay, after](juce::Graphics& g) {
                auto& md = *model.midi;
                g.setFont(theme::font(11));
                g.setColour(md.sources.isEmpty() ? theme::secondary : theme::text);
                g.drawFittedText(md.sources.isEmpty() ? juce::String("None connected") : md.sources.joinIntoString(", "), ctrlX, dy, 700, rowH, juce::Justification::centredLeft, 1);
                g.setColour(theme::secondary);
                g.drawText(md.last.isEmpty() ? juce::String::fromUTF8("\xe2\x80\x94") : md.last, ctrlX, my, 400, rowH, juce::Justification::centredLeft);
                if (md.maps.empty()) g.drawText("Nothing assigned yet", after + 4, ay, 300, rowH, juce::Justification::centredLeft);
            };
            fit(mid, lay);
        }
        // LIBRARY
        {
            Group::Lay lay { lib };
            Group::place(lay.row("Recordings", "settings:recordings"), Group::withOpen({ { &importRec, 140 } }, &openRec, 110));
            const int fy = lay.row("Recordings folder", "settings:recordingsFolder");
            const int fx = Group::place(fy, Sessions::chosen != juce::File() ? Group::Items { { &changeRec, 140 }, { &resetRec, 110 } } : Group::Items { { &changeRec, 140 } });
            resetRec.setVisible(Sessions::chosen != juce::File());
            const int ny = lay.row("NAM captures", "settings:nam"), nx = Group::place(ny, Group::withOpen({ { &addNam, 80 } }, &openNam, 110, { { &rescanNam, 80 } }));
            const int iy = lay.row("Cab IRs", "settings:cabIRs"), ix = Group::place(iy, Group::withOpen({ { &addIRs, 80 } }, &openIRs, 110, { { &rescanIRs, 80 } }));
            const int cy = lay.row("Your captures", "settings:captures"), cx = Group::place(cy, Group::withOpen({}, &openCap, 110, { { &rescan, 80 } }));
            const int gy = lay.row("God's Cab", "settings:godsCab"), gx = Group::place(gy, { { &openIR, 110 } });
            lib.extra = [this, ny, nx, iy, ix, cy, cx, gy, gx, fy, fx](juce::Graphics& g) {
                g.setColour(theme::secondary); g.setFont(theme::font(11));
                if (fy >= 0) {
                    const auto path = Sessions::chosenMissing() ? Sessions::chosen.getFullPathName() + " (not available: using the default for now)"
                                                                : Sessions::root().getFullPathName();
                    g.setColour(Sessions::chosenMissing() ? juce::Colours::orange : theme::secondary);
                    g.drawFittedText(path, fx + 4, fy, std::max(100, lib.getWidth() - fx - 24), rowH, juce::Justification::centredLeft, 1, 0.8f);
                    g.setColour(theme::secondary);
                }
                g.drawText(juce::String(model.namFiles.size()) + " loaded", nx + 4, ny, 300, rowH, juce::Justification::centredLeft);
                g.drawText(juce::String(model.cabIRs.all().size()) + " in the library", ix + 4, iy, 300, rowH, juce::Justification::centredLeft);
                g.drawText(juce::String((int) model.captures.size()) + " loaded", cx + 4, cy, 300, rowH, juce::Justification::centredLeft);
                g.drawText(model.irLib.isEmpty() ? juce::String("Not found") : juce::String("Found"), gx + 4, gy, 300, rowH, juce::Justification::centredLeft);
            };
            fit(lib, lay);
        }
        // APPEARANCE
        {
            Group::Lay lay { look };
            const int ay = lay.row("Accent colour", "settings:accent");
            { int sx = ctrlX; for (auto& s : swatchButtons) { s->setBounds(sx, ay + 3, 24, 24); sx += 30; } Group::place(ay, { { &followAmp, 170 } }, sx + 6); }
            const int zy = lay.row("Zoom", "settings:zoom");
            Group::place(zy, { { &zoomMinus, 30 } }); Group::place(zy, { { &zoomPlus, 30 } }, ctrlX + 30 + 68);
            Group::place(lay.row("Explanations", "settings:explain"), { { &explain, 320 } });
            look.extra = [this, zy](juce::Graphics& g) {
                g.setColour(theme::text); g.setFont(theme::font(11));
                g.drawText(juce::String(juce::roundToInt(model.zoom * 100)) + "%", ctrlX + 30, zy, 68, rowH, juce::Justification::centred);
            };
            fit(look, lay);
        }
        // ABOUT
        {
            auto& u = *model.updater;
            Group::Lay lay { about };
            lay.heading("Updates");
            const int vy = lay.row("Version", "settings:version");
            const int vx = Group::place(vy, { { &checkNow, 110 } }, ctrlX + 70);
            updProgress.setVisible(u.phase == Updater::Phase::downloading);
            if (updProgress.isVisible()) Group::place(vy, { { &updProgress, 200 } }, vx);
            Group::place(lay.row("Automatic", "settings:autoUpdate"), { { &autoUpd, 420 } });
            const bool avail = u.phase == Updater::Phase::available && u.release;
            int ry = -1;
            for (auto* c : { &newsB, &installB, &pageB }) c->setVisible(avail);
            if (avail) { ry = lay.row("Available", "settings:version"); Group::place(ry, { { &newsB, 120 }, { &installB, 120 }, { &pageB, 140 } }, ctrlX + 150); }
            lay.heading("Feedback");
            Group::place(lay.row("GitHub", "settings:feedback"), { { &bugB, 150 }, { &featB, 170 } });
            debugT.setToggleState(debuglog::enabled(), juce::dontSendNotification);
            Group::place(lay.row("Debug log", "settings:debugLog"),
                         onAndroid ? Group::Items { { &debugT, 230 }, { &shareLogB, 110 } } : Group::Items { { &debugT, 230 }, { &shareLogB, 100 }, { &copyLogB, 100 } });
            copyLogB.setVisible(!onAndroid);
            lay.heading("License");
            const int ly = lay.row("Leviathan", "settings:license");
            Group::place(lay.row("", "settings:license"), { { &licB, 90 }, { &noticeB, 140 }, { &srcB, 120 } });
            const int cy = lay.row("", "settings:license", 40);
            about.extra = [this, vy, vx, ry, ly, cy, w](juce::Graphics& g) {
                auto& up = *model.updater;
                g.setColour(theme::text); g.setFont(theme::font(11, true));
                g.drawText(Updater::current(), ctrlX, vy, 66, rowH, juce::Justification::centredLeft);
                if (ry >= 0) g.drawText("Leviathan " + up.release->version, ctrlX, ry, 146, rowH, juce::Justification::centredLeft);
                g.drawText("by Shane O'Reilly", ctrlX, ly, 170, rowH, juce::Justification::centredLeft);
                juce::String st; juce::Colour c = theme::secondary;
                switch (up.phase) {
                case Updater::Phase::checking: st = "Checking" + ellipsis; break;
                case Updater::Phase::upToDate: st = "Up to date"; break;
                case Updater::Phase::available: st = "Leviathan " + up.release->version + " is available"; c = theme::text; break;
                case Updater::Phase::installing: st = "Installing" + ellipsis; break;
                case Updater::Phase::failed: st = up.error; c = juce::Colours::orange; break;
                default: break;
                }
                g.setFont(theme::font(11));
                if (up.phase != Updater::Phase::downloading) { g.setColour(c); g.drawFittedText(st, vx + 4, vy, w - vx - 24, rowH, juce::Justification::centredLeft, 1); }
                g.setColour(theme::secondary);
                g.drawText("Free software under the GNU AGPLv3, with no warranty.", ctrlX + 176, ly, 520, rowH, juce::Justification::centredLeft);
                g.setFont(theme::font(10));
                g.drawFittedText(juce::String::fromUTF8("Built with JUCE (AGPLv3, \xc2\xa9 Raw Material Software) and the Steinberg ASIO SDK (GPLv3). "
                                                        "ASIO is a trademark of Steinberg Media Technologies GmbH."),
                                 ctrlX, cy, w - ctrlX - labelX, 40, juce::Justification::topLeft, 2);
            };
            fit(about, lay);
        }
        int shownBottom = 0;
        for (auto* g : groups()) if (g->isVisible()) shownBottom = g->getBottom();
        inner.setSize(viewport.getMaximumVisibleWidth(), shownBottom + 24);
    }
    void paint(juce::Graphics& g) override {
        g.setColour(theme::line); g.fillRect(nav.getRight() - 1, nav.getY(), 1, nav.getHeight());
    }
    /// Search: every laid-out row (all sections are laid out, only one shown), matched on its label, heading, section
    /// and hover explanation, label matches first. Same index and ranking as the Mac app's searchSettings.
    struct Hit { juce::String section, heading, label; };
    std::vector<Hit> search(const juce::String& query) {
        juce::StringArray words; words.addTokens(query.toLowerCase(), " ", ""); words.removeEmptyStrings();
        std::vector<Hit> first, rest;
        if (words.isEmpty()) return first;
        for (auto* g : groups())
            for (auto& r : g->rows) {
                if (r.label.isEmpty() || r.key.isEmpty() || r.label == "Now") continue;
                const auto head = g->headingFor(r.y);
                const auto all = (r.label + " " + head + " " + g->title + " " + Model::explain(r.key)).toLowerCase();
                bool hit = true, inLabel = true;
                for (auto& w : words) { hit = hit && all.contains(w); inLabel = inLabel && r.label.toLowerCase().contains(w); }
                if (!hit) continue;
                bool dup = false;   // one result per label in a section (e.g. two "Now" rows)
                for (auto* v : { &first, &rest }) for (auto& h : *v) dup = dup || (h.section == g->title && h.label == r.label);
                if (!dup) (inLabel ? first : rest).push_back({ g->title, head, r.label });
            }
        first.insert(first.end(), rest.begin(), rest.end());
        return first;
    }
    void go(const Hit& h) {
        section = h.section; model.props().setValue("settingsSection", h.section);
        nav.query.clear(); nav.refreshResults();
        resized(); repaint(); nav.repaint();
        for (auto* g : groups())
            if (g->title == h.section) {
                g->highlight = h.label; g->repaint();
                for (auto& r : g->rows) if (r.label == h.label) viewport.setViewPosition(0, std::max(0, g->getY() + r.y - viewport.getHeight() / 3));
                juce::Component::SafePointer<Group> sg(g); const auto label = h.label;
                juce::Timer::callAfterDelay(2500, [sg, label] { if (sg && sg->highlight == label) { sg->highlight.clear(); sg->repaint(); } });
            }
    }

    void chooseRecordingsFolder() {
        if (!hasAllFilesAccess()) {   // Android 11+: recording into a folder on the phone's storage needs All files access
            auto opts = juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("Allow access to your phone's storage")
                .withMessage("To record into a folder on your phone's storage, Leviathan needs Android's \"All files access\". "
                             "Turn it on for Leviathan on the next screen, come back, and tap Change folder again.")
                .withButton("Open settings", 1).withButton("Cancel", 0);
            juce::AlertWindow::showAsync(opts, [](int r) { if (r == 1) requestAllFilesAccess(); });
            return;
        }
        auto chooser = std::make_shared<juce::FileChooser>(onAndroid ? juce::String("Choose a folder on your phone for your recordings")
                                                                     : juce::String("Choose where to keep your recordings (any folder, Google Drive or Dropbox included)"),
                                                            Sessions::root());
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, chooser](const juce::FileChooser& fc) {
            if (fc.getURLResults().isEmpty() && fc.getResults().isEmpty()) return;   // cancelled
            const auto dir = pickedDirectoryPath(fc);
            if (dir == juce::File()) {
                model.status = "Leviathan can only record into a folder on the phone's own storage (or SD card). Pick one there; "
                               "to get takes into Google Drive, use Share, or a sync app on that folder.";
                model.changed(); return;
            }
            askMoveRecordings(dir);
        });
    }
    /// Asks whether to bring the existing takes along, then switches folders (Sessions::changeRoot).
    void askMoveRecordings(const juce::File& to) {
        auto& s = *model.sessions;
        if (to == Sessions::root()) return;
        const int n = s.countTakes();
        if (n == 0) { s.changeRoot(to, false); return; }
        auto opts = juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Move your " + juce::String(n) + " recording" + (n == 1 ? "" : "s") + "?")
            .withMessage("New recordings will go to " + to.getFullPathName() + ".\n\nMoving copies each take there, checks the copy, then removes it from "
                         + Sessions::root().getFullPathName() + " (and that folder once it's empty). Takes you leave behind stay where they are, "
                         "but Leviathan only lists the new folder.")
            .withButton("Move recordings", 1).withButton("Leave them", 2).withButton("Cancel", 0);
        juce::AlertWindow::showAsync(opts, [this, to](int r) { if (r != 0) model.sessions->changeRoot(to, r == 1); });
    }

    /// Left-hand navigation (same sections as the Mac app); the last one opened is remembered.
    juce::String section;
    struct Nav : juce::Component {
        explicit Nav(SettingsScreen& o) : owner(o) {
            addAndMakeVisible(query);
            query.setTextToShowWhenEmpty("Search settings", theme::secondary);
            query.setFont(theme::font(10.5f)); query.setIndents(30, 0); query.setJustification(juce::Justification::centredLeft);
            query.setColour(juce::TextEditor::backgroundColourId, juce::Colours::white.withAlpha(0.06f));
            query.setColour(juce::TextEditor::outlineColourId, theme::line);
            query.onTextChange = [this] { refreshResults(); };
            query.onReturnKey = [this] { if (!results.empty()) owner.go(results.front()); };
            query.onEscapeKey = [this] { query.clear(); refreshResults(); };
        }
        static constexpr const char* items[] = { "Audio", "MIDI", "Library", "Appearance", "About" };
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> rows;
        juce::TextEditor query;
        std::vector<SettingsScreen::Hit> results;
        std::vector<juce::Rectangle<int>> resultRects;
        void refreshResults() { results = owner.search(query.getText()); repaint(); }
        void resized() override { query.setBounds(14, 46, getWidth() - 28, 34); }
        void paint(juce::Graphics& g) override {
            g.fillAll(theme::bar);
            drawSectionTitle(g, "SETTINGS", { 24, 10, 180, 28 });
            {   // magnifier inside the search box
                const auto q = query.getBounds().toFloat();
                const float cx = q.getX() + 15, cy = q.getCentreY() - 1;
                juce::Path p; p.addEllipse(cx - 5, cy - 5, 10, 10); p.startNewSubPath(cx + 3.5f, cy + 3.5f); p.lineTo(cx + 7, cy + 7);
                g.setColour(theme::secondary); g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            rows.clear(); resultRects.clear();
            int y = query.getBottom() + 14;
            if (query.getText().isNotEmpty()) {
                if (results.empty()) { g.setColour(theme::secondary); g.setFont(theme::font(10.5f)); g.drawText("No settings match.", 26, y, getWidth() - 40, 24, juce::Justification::centredLeft); }
                for (auto& h : results) {
                    const juce::Rectangle<int> r(14, y, getWidth() - 28, 40);
                    g.setColour(theme::text); g.setFont(theme::font(10.5f)); g.drawText(h.label, r.withTrimmedLeft(12).removeFromTop(22), juce::Justification::bottomLeft);
                    g.setColour(theme::secondary); g.setFont(theme::font(9));
                    g.drawText(h.heading.isEmpty() ? h.section : h.section + juce::String::fromUTF8(" \xe2\x80\xba ") + h.heading,
                               r.withTrimmedLeft(12).withTrimmedTop(22), juce::Justification::topLeft);
                    resultRects.push_back(r); y += 42;
                }
                return;
            }
            for (const auto* it : items) {
                const juce::Rectangle<int> r(14, y, getWidth() - 28, 32);
                const bool on = owner.section == it;
                if (on) { g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(r.toFloat(), 7); }
                g.setColour(on ? owner.model.accent() : theme::text); g.setFont(theme::font(10.5f));
                g.drawText(it, r.withTrimmedLeft(12), juce::Justification::centredLeft);
                if (juce::String(it) == "About" && owner.model.updater->phase == Updater::Phase::available) {
                    g.setColour(juce::Colours::red); g.fillEllipse((float) r.getRight() - 18, (float) r.getCentreY() - 4, 8, 8);
                }
                rows.emplace_back(r, it); y += 34;
            }
        }
        void mouseUp(const juce::MouseEvent& e) override {
            for (size_t i = 0; i < resultRects.size() && i < results.size(); ++i)
                if (resultRects[i].contains(e.getPosition())) { const auto h = results[i]; owner.go(h); return; }
            for (auto& [r, n] : rows) if (r.contains(e.getPosition())) {
                owner.section = n; owner.model.props().setValue("settingsSection", n);
                owner.resized(); owner.repaint(); repaint();
            }
        }
        SettingsScreen& owner;
    };
    Nav nav { *this };

    /// One MIDI assignment: "CC 7 · ch 1" in the label column, the target at the control column, momentary, remove.
    struct MapRow : juce::Component {
        MapRow(Model& m, const MidiMap& mp) : model(m), id(mp.id), x(IconButton::cross, [this] { model.midi->removeMap(id); }, 11) {
            label = "CC " + juce::String(mp.cc) + dot + "ch " + juce::String(mp.channel + 1); name = mp.name;
            addAndMakeVisible(x);
            if (mp.toggle) {
                momentary.setButtonText("Momentary switch"); momentary.setToggleState(mp.momentary, juce::dontSendNotification);
                momentary.setTooltip("On: each press flips it. Off: the switch's value sets it (127 = on, 0 = off).");
                momentary.onClick = [this] { model.midi->setMomentary(id, momentary.getToggleState()); };
                addAndMakeVisible(momentary);
            }
        }
        void resized() override {
            using namespace settingsLayout;
            momentary.setBounds(ctrlX - labelX + 330, 2, 180, 26); x.setBounds(getWidth() - 26, 4, 22, 22);
        }
        void paint(juce::Graphics& g) override {
            using namespace settingsLayout;
            g.setFont(theme::font(11)); g.setColour(theme::secondary); g.drawText(label, 0, 0, labelW, getHeight(), juce::Justification::centredLeft);
            g.setColour(theme::text); g.drawText(name, ctrlX - labelX, 0, 320, getHeight(), juce::Justification::centredLeft);
        }
        Model& model; juce::String id, label, name; IconButton x; juce::ToggleButton momentary;
    };

    /// Latency summary and breakdown, then the engine's load; repainted a few times a second.
    struct AudioInfo : juce::Component, private juce::Timer {
        explicit AudioInfo(Model& m) : model(m) { startTimerHz(4); }
        void timerCallback() override { if (isShowing()) repaint(); }
        void paint(juce::Graphics& g) override {
            const auto& i = model.info;
            g.setFont(theme::font(11));
            if (i.sampleRate <= 0) { g.setColour(theme::secondary); g.drawText("Not running.", 0, 0, getWidth(), 30, juce::Justification::centredLeft); return; }
            const std::pair<const char*, double> parts[] = { { "your interface's input", i.inMs }, { "the buffers", i.bufferMs }, { "the output device", i.outMs } };
            const auto* big = &parts[0]; for (auto& p : parts) if (p.second > big->second) big = &p;
            g.setColour(theme::text);
            g.drawText("About " + juce::String(i.latencyMs(), 1) + " ms from string to ear"
                           + (big->second > i.latencyMs() * 0.5 ? ", mostly " + juce::String(big->first) : juce::String()),
                       0, 0, getWidth(), 22, juce::Justification::centredLeft);
            g.setColour(theme::secondary); g.setFont(theme::font(10));
            g.drawText("Input " + juce::String(i.inMs, 1) + dot + "buffers " + juce::String(i.bufferMs, 1) + dot + "output " + juce::String(i.outMs, 1) + " ms"
                           + dot + juce::String(i.sampleRate / 1000, 1) + " kHz" + dot + juce::String(i.buffer) + " samples" + dot + i.type,
                       0, 22, getWidth(), 20, juce::Justification::centredLeft);
            g.setFont(theme::font(11));
            g.setColour(model.dropouts > 0 || model.cpu > 0.6f ? juce::Colours::orange : theme::secondary);
            g.drawText("DSP load " + juce::String(juce::roundToInt(model.cpu * 100)) + "%" + dot + juce::String(model.dropouts) + " dropout" + (model.dropouts == 1 ? "" : "s"),
                       0, engineY, getWidth(), 30, juce::Justification::centredLeft);
        }
        Model& model; int engineY = 54;
    };

    Model& model;
    juce::Viewport viewport; juce::Component inner;
    SelectorLookAndFeel selLnf;
    juce::AudioDeviceSelectorComponent audioSel;
    Group audio, mid, lib, look, about;
    HelpStrip strip;
    juce::ComboBox inputCh; AudioInfo audioInfo { model };
    juce::Slider a4; juce::ComboBox tuning;
    juce::ToggleButton outEqT; std::array<juce::Slider, 4> outEqS;
    juce::ToggleButton lowLagT { "Less delay on low chords (they may shimmer slightly)" };
    juce::ToggleButton namCalT; juce::Slider ifaceLvl;
    /// "Amp: +3.0 dB (captured at 9.0 dBu)": what input calibration is doing to a loaded capture.
    static juce::String calibrationLine(const juce::String& label, const juce::String& name, bool loaded, std::optional<double> level, double trim) {
        if (name.isEmpty()) return label + ": no capture (the built-in amp isn't calibrated)";
        if (!loaded) return label + ": loads when audio starts";
        if (!level) return label + ": this capture has no input level, so it's left as it is";
        return label + ": " + (trim >= 0 ? "+" : "") + juce::String(trim, 1) + " dB (captured at " + juce::String(*level, 1) + " dBu)";
    }
    juce::ToggleButton pcT; juce::TextButton learnB, clearB;
    std::vector<std::unique_ptr<MapRow>> mapRows;
    juce::ToggleButton debugT; juce::TextButton shareLogB, copyLogB; juce::ScopedMessageBox logShare;
    juce::TextButton importRec, openRec, changeRec, resetRec, addNam, openNam, rescanNam, addIRs, openIRs, rescanIRs, openCap, rescan, openIR;
    std::vector<std::unique_ptr<Swatch>> swatchButtons;
    juce::TextButton followAmp, zoomMinus, zoomPlus;
    juce::ToggleButton explain;
    juce::TextButton checkNow, newsB, installB, pageB, bugB, featB, licB, noticeB, srcB; juce::ToggleButton autoUpd;
    double progressValue = 0; juce::ProgressBar updProgress { progressValue };
    juce::String signature;
};

/// Text painted inside the settings groups (row labels and values that change).
struct GroupText : juce::Component {};

// MARK: - banners

/// The button on an error banner that goes to the setting that fixes it ("Tap" on touch screens).
const juce::String resolveLabel = onAndroid ? "Tap to resolve" : "Click to resolve";

struct Banner : juce::Component {
    using Actions = std::vector<std::pair<juce::String, std::function<void()>>>;
    /// close: the ✕ (none when empty). actions: buttons at the right, the last one highlighted.
    Banner(juce::String t, juce::Colour bgc, juce::Colour fg, std::function<void()> close, Actions actions = {})
        : text(std::move(t)), bgColour(bgc), fgColour(fg), x(IconButton::cross, close ? close : [] {}, 11) {
        if (close) addAndMakeVisible(x);
        for (size_t i = 0; i < actions.size(); ++i) {
            auto* b = buttons.add(new juce::TextButton(actions[i].first));
            b->onClick = std::move(actions[i].second);
            if (i + 1 == actions.size() && actions.size() > 1) b->setToggleState(true, juce::dontSendNotification);   // primary
            addAndMakeVisible(b);
        }
    }
    int buttonsWidth() const { int w = 0; for (auto* b : buttons) w += b->getBestWidthForHeight(26) + 24 + 8; return w; }
    void resized() override {
        int right = getWidth() - (x.isVisible() ? 40 : 10);
        x.setBounds(getWidth() - 34, (getHeight() - 24) / 2, 24, 24);
        for (int i = buttons.size(); --i >= 0;) {
            const int w = buttons[i]->getBestWidthForHeight(26) + 24;
            buttons[i]->setBounds(right - w, (getHeight() - 26) / 2, w, 26); right -= w + 8;
        }
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(bgColour); g.setColour(fgColour); g.setFont(theme::font(12));
        g.drawFittedText(text, getLocalBounds().reduced(10, 0).withTrimmedRight(buttonsWidth() + (x.isVisible() ? 40 : 10)), juce::Justification::centredLeft, 2);
        if (progress >= 0) {
            const auto r = juce::Rectangle<float>((float) getWidth() - 220.f, (float) getHeight() / 2 - 3, 180, 6);
            g.setColour(theme::line); g.fillRoundedRectangle(r, 3);
            g.setColour(fgColour.withAlpha(0.9f)); g.fillRoundedRectangle(r.withWidth(r.getWidth() * (float) progress), 3);
        }
    }
    juce::String text; juce::Colour bgColour, fgColour; IconButton x; juce::OwnedArray<juce::TextButton> buttons;
    double progress = -1;
};

/// "What's new": the release notes, then Update Now or Ignore (hides this version until the next one). Covers the
/// window, dimming it; ✕, Esc or a click outside closes it without ignoring.
struct WhatsNew : juce::Component {
    WhatsNew(Model& m) : model(m), close(IconButton::cross, [this] { done(); }, 11) {
        auto& r = *m.updater->release;
        title = juce::String::fromUTF8("What\xe2\x80\x99s new in Leviathan ") + r.version;
        sub = "You have " + Updater::current() + ". Updating replaces this copy and reopens Leviathan.";
        notes.accent = m.accent();
        notes.setMarkdown(r.notes.isNotEmpty() ? r.notes : juce::String("No notes for this release."), true);
        view.setViewedComponent(&notes, false); view.setScrollBarsShown(true, false);
        addAndMakeVisible(view); addAndMakeVisible(close);
        ignore.setButtonText("Ignore"); ignore.setTooltip("Hide this update until the next version (Settings, About still has it)");
        ignore.onClick = [this] { model.updater->dismiss(); done(); };
        update.setButtonText("Update Now"); update.setToggleState(true, juce::dontSendNotification);
        update.onClick = [this] { done(); model.updater->install(); };
        addAndMakeVisible(ignore); addAndMakeVisible(update);
        setWantsKeyboardFocus(true);
    }
    void done() { model.updater->showNotes = false; model.changed(); }
    juce::Rectangle<int> card() const {
        const int w = std::min(900, getWidth() - 40), h = std::min(940, getHeight() - 40);
        return getLocalBounds().withSizeKeepingCentre(w, h);
    }
    void resized() override {
        const auto c = card();
        close.setBounds(c.getRight() - 40, c.getY() + 16, 24, 24);
        const int bh = 34, uw = update.getBestWidthForHeight(bh) + 32, iw = ignore.getBestWidthForHeight(bh) + 32;
        update.setBounds(c.getRight() - 20 - uw, c.getBottom() - 16 - bh, uw, bh);
        ignore.setBounds(update.getX() - 10 - iw, update.getY(), iw, bh);
        const auto body = c.withTrimmedTop(headH).withTrimmedBottom(footH).reduced(28, 18);
        view.setBounds(body);
        notes.layout(body.getWidth() - view.getScrollBarThickness() - 6);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));
        const auto c = card().toFloat();
        g.setColour(juce::Colour(0xff121212)); g.fillRoundedRectangle(c, 12);
        g.setColour(theme::line); g.drawRoundedRectangle(c.reduced(0.5f), 12, 1);
        g.fillRect(c.getX(), c.getY() + (float) headH, c.getWidth(), 1.f);
        g.fillRect(c.getX(), c.getBottom() - (float) footH, c.getWidth(), 1.f);
        // header: icon, title, subtitle
        const auto ic = juce::Rectangle<float>(c.getX() + 28, c.getY() + 26, 34, 34);
        g.setColour(model.accent()); g.fillEllipse(ic);
        g.setColour(juce::Colours::black);
        juce::Path arrow; const auto cx = ic.getCentreX(), cy = ic.getCentreY();
        arrow.startNewSubPath(cx, cy - 9); arrow.lineTo(cx, cy + 8); arrow.startNewSubPath(cx - 7, cy + 1); arrow.lineTo(cx, cy + 8); arrow.lineTo(cx + 7, cy + 1);
        g.strokePath(arrow, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(theme::text); g.setFont(theme::font(17, true));
        g.drawText(title, (int) ic.getRight() + 16, (int) c.getY() + 18, (int) c.getWidth() - 160, 34, juce::Justification::centredLeft);
        g.setColour(theme::secondary); g.setFont(theme::font(11));
        g.drawText(sub, (int) ic.getRight() + 16, (int) c.getY() + 52, (int) c.getWidth() - 160, 24, juce::Justification::centredLeft);
    }
    void mouseUp(const juce::MouseEvent& e) override { if (!card().contains(e.getPosition())) done(); }
    bool keyPressed(const juce::KeyPress& k) override {
        if (k == juce::KeyPress::escapeKey) { done(); return true; }
        if (k == juce::KeyPress::returnKey) { update.triggerClick(); return true; }
        return true;   // the popup is modal: keep shortcuts (Space plays, etc.) away from the rig
    }
    Model& model;
    juce::String title, sub;
    MarkdownView notes; juce::Viewport view;
    IconButton close; juce::TextButton ignore, update;
    static constexpr int headH = 96, footH = 66;
};

} // namespace

// MARK: - root

Root::Root(Model& m) : model(m) {
    setLookAndFeel(&lnf);
    lnf.setAccent(m.accent());
    sidebar = std::make_unique<Sidebar>(m); addAndMakeVisible(*sidebar);
    topbar = std::make_unique<TopBar>(m); addAndMakeVisible(*topbar);
    rig = std::make_unique<RigView>(m); addChildComponent(*rig);
    rig->onScroll = [this] { if (onAndroid && model.screen == Screen::rig) resized(); };   // the top bar follows the scroll
    presets = std::make_unique<PresetsScreen>(m); addChildComponent(*presets);
    recordings = std::make_unique<RecordingsScreen>(m); addChildComponent(*recordings);
    settings = std::make_unique<SettingsScreen>(m); addChildComponent(*settings);
    m.addChangeListener(this);
    setWantsKeyboardFocus(true);
    startTimerHz(15);
    changeListenerCallback(nullptr);
}

Root::~Root() { model.removeChangeListener(this); setLookAndFeel(nullptr); }

void Root::timerCallback() {
    if (auto* t = dynamic_cast<TopBar*>(topbar.get())) { t->meters.repaint(); if (model.sessions->recording) t->sync(); t->repaint(); }
    if (model.sessions->playing && model.screen == Screen::recordings) recordings->repaint();
}

void Root::rebuildBanners() {
    auto& m = model;
    const auto ph = m.updater->phase;
    juce::String updSig = m.updater->showBanner() ? "upd" + m.updater->release->version : juce::String();
    if (ph == Updater::Phase::downloading) updSig = "dl" + juce::String((int) (m.updater->progressValue * 100));
    if (ph == Updater::Phase::installing) updSig = "inst";
    if (ph == Updater::Phase::failed) updSig = "fail" + m.updater->error;
    juce::StringArray sig { updSig, m.status, m.currentFix(), m.sessions->message, m.sessions->currentFix(), (m.midi->learning && m.screen == Screen::rig) ? "learn" + (m.midi->armed ? m.midi->armed->key : juce::String()) : juce::String() };
    if (sig.joinIntoString("|") == bannerSig) return;
    bannerSig = sig.joinIntoString("|");
    banners.clear();
    const juce::Colour warnBg(0xff291f0f), warnFg(0xffffd999);
    const auto updBg = m.accent().withAlpha(0.18f).overlaidWith(theme::bg.withAlpha(0.0f));
    if (m.updater->showBanner())
        banners.add(new Banner("Leviathan " + m.updater->release->version + " is available (you have " + Updater::current() + ").", updBg, theme::text,
                               [&m] { m.updater->dismiss(); },
                               { { juce::String::fromUTF8("What\xe2\x80\x99s new"), [&m] { m.updater->showNotes = true; m.changed(); } },
                                 { "Update now", [&m] { m.updater->install(); } } }));
    else if (ph == Updater::Phase::downloading) {
        auto* b = new Banner(juce::String::fromUTF8("Downloading the update\xe2\x80\xa6 ") + juce::String((int) (m.updater->progressValue * 100)) + "%",
                                         updBg, theme::text, {});
        b->progress = m.updater->progressValue;
        banners.add(b);
    } else if (ph == Updater::Phase::installing)
        banners.add(new Banner(juce::String::fromUTF8("Installing the update. Leviathan will reopen in a moment\xe2\x80\xa6"), updBg, theme::text, {}));
    else if (ph == Updater::Phase::failed && m.updater->error.isNotEmpty())
        banners.add(new Banner(m.updater->error, warnBg, warnFg, [&m] { m.updater->phase = Updater::Phase::idle; m.changed(); },
                               { { resolveLabel, [&m] { m.resolve("About"); } } }));
    // an error that a setting fixes gets a button that goes there
    auto resolveIn = [&m](const juce::String& where) {
        return where.isEmpty() ? Banner::Actions() : Banner::Actions { { resolveLabel, [&m, where] { m.resolve(where); } } };
    };
    if (m.status.isNotEmpty()) banners.add(new Banner(m.status, warnBg, warnFg, [&m] { m.status = {}; m.changed(); }, resolveIn(m.currentFix())));
    if (m.sessions->message.isNotEmpty())
        banners.add(new Banner(m.sessions->message, warnBg, warnFg, [&m] { m.sessions->message = {}; m.changed(); }, resolveIn(m.sessions->currentFix())));
    if (m.midi->learning && m.screen == Screen::rig) {
        const auto t = m.midi->armed ? "Move a knob, pedal or switch on your MIDI device to assign it to " + m.midi->armed->name + "."
                                     : juce::String("MIDI learn: click a knob or an ON/OFF switch, then move a control on your MIDI device.")
                                           + (m.midi->sources.isEmpty() ? "   No MIDI devices connected." : "");
        banners.add(new Banner(t, m.accent().withAlpha(0.18f).overlaidWith(theme::bg.withAlpha(0.0f)), theme::text, [&m] { m.midi->setLearning(false); },
                               { { "Done", [&m] { m.midi->setLearning(false); } } }));
    }
    for (auto* b : banners) addAndMakeVisible(b);
}

void Root::changeListenerCallback(juce::ChangeBroadcaster*) {
    lnf.setAccent(model.accent());
    rebuildBanners();
    const bool notesOpen = model.updater->showNotes && model.updater->release.has_value();
    if (notesOpen && !whatsNew) { whatsNew = std::make_unique<WhatsNew>(model); addAndMakeVisible(*whatsNew); whatsNew->grabKeyboardFocus(); }
    if (!notesOpen && whatsNew) whatsNew.reset();
    if (auto* t = dynamic_cast<TopBar*>(topbar.get())) t->sync();
    sidebar->repaint();
    rig->setVisible(model.screen == Screen::rig);
    presets->setVisible(model.screen == Screen::presets);
    recordings->setVisible(model.screen == Screen::recordings);
    settings->setVisible(model.screen == Screen::settings);
    switch (model.screen) {
    case Screen::rig: rig->refresh(); break;
    case Screen::presets: presets->refresh(); break;
    case Screen::recordings: recordings->refresh(); break;
    case Screen::settings: settings->refresh(); break;
    }
    if (shown != model.screen) { shown = model.screen; }
    resized();
    repaint();
}

void Root::resized() {
    auto r = getLocalBounds();
   #if JUCE_ANDROID
    // keep clear of the camera cutout and rounded corners (the system bars are hidden)
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) r = d->safeAreaInsets.subtractedFrom(r);
   #endif
    sidebar->setBounds(r.removeFromLeft(92));
    const int topH = static_cast<TopBar*>(topbar.get())->heightFor(r.getWidth()), headH = topH + 40 * banners.size();
    // Phones: on the rig the bar (and any banners) sit over the top of the scrolling rig and scroll away with it,
    // leaving the whole screen to the rig; scrolling back to the top brings them back. Elsewhere they stay put.
    const bool scrollAway = onAndroid && model.screen == Screen::rig;
    const int off = scrollAway ? std::min(rig->scrollY(), headH) : 0;
    auto head = r.withHeight(headH).translated(0, -off);
    topbar->setBounds(head.removeFromTop(topH));
    for (auto* b : banners) b->setBounds(head.removeFromTop(40));
    if (scrollAway) {
        rig->setTopInset(headH);
        rig->setBounds(r);
        topbar->toFront(false); for (auto* b : banners) b->toFront(false);
    } else {
        rig->setTopInset(0);
        r.removeFromTop(headH);
        rig->setBounds(r);
    }
    for (auto* s : std::initializer_list<juce::Component*> { presets.get(), recordings.get(), settings.get() }) s->setBounds(r.withTop(r.getY() + (scrollAway ? headH : 0)));
    if (whatsNew) { whatsNew->setBounds(getLocalBounds()); whatsNew->toFront(false); }
}

bool Root::keyPressed(const juce::KeyPress& k) {
    const bool cmd = k.getModifiers().isCommandDown();
    if (k == juce::KeyPress::spaceKey && !cmd) { model.toggle(); return true; }
    if (cmd && k.getKeyCode() == 'R') { model.sessions->toggleRecord(); return true; }
    if (cmd && k.getKeyCode() == 'S' && k.getModifiers().isShiftDown()) {
        askText("Save preset as", model.presetName.isEmpty() ? juce::String() : model.presetName + " copy", "Save", [this](juce::String n) { model.savePreset(n); });
        return true;
    }
    if (cmd && k.getKeyCode() == 'S') { model.saveCurrent(); return true; }
    if (cmd && k.getKeyCode() == 'Z') { model.undo(); return true; }
    if (cmd && (k.getKeyCode() == '=' || k.getKeyCode() == '+')) { model.zoomStep(1); return true; }
    if (cmd && k.getKeyCode() == '-') { model.zoomStep(-1); return true; }
    if (cmd && k.getKeyCode() == '0') { model.setZoom(1.0); return true; }
    if (!cmd && (k.getKeyCode() == 'T' || k.getKeyCode() == 't')) { model.setTuner(!model.tunerOn); return true; }
    if (k == juce::KeyPress::escapeKey && model.midi->learning) { model.midi->setLearning(false); return true; }
    if (k == juce::KeyPress::returnKey && model.screen == Screen::recordings) { model.sessions->togglePlay(); return true; }
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && model.screen == Screen::recordings && model.sessions->current()) { model.sessions->remove(); return true; }
    return false;
}

} // namespace lev
