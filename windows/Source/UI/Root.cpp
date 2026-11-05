#include "Root.h"
#include "../Sessions.h"
#include "../Updater.h"

namespace lev {

namespace {
const juce::String dot = juce::String::fromUTF8(" \xc2\xb7 ");
const juce::String ellipsis = juce::String::fromUTF8("\xe2\x80\xa6");

juce::TextButton* sizeButton(juce::TextButton& b, int extra = 24) {
    setPref(b, juce::GlyphArrangement::getStringWidthInt(theme::font(10), b.getButtonText()) + extra, 28);
    return &b;
}

void askText(const juce::String& title, const juce::String& initial, const juce::String& okText, std::function<void(juce::String)> done) {
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
    explicit Sidebar(Model& m) : model(m), minus(IconButton::minus, [&m] { m.zoomStep(-1); }, 12), plus(IconButton::plus, [&m] { m.zoomStep(1); }, 12) {
        addAndMakeVisible(minus); addAndMakeVisible(plus);
        minus.setTooltip(juce::String::fromUTF8("Zoom out (Ctrl \xe2\x88\x92)")); plus.setTooltip("Zoom in (Ctrl +)");
    }
    struct Item { Screen s; const char* label; };
    const Item items[3] = { { Screen::rig, "Rig" }, { Screen::presets, "Presets" }, { Screen::recordings, "Recordings" } };
    juce::Rectangle<int> itemRect(int i) const { return { 6, 10 + i * 74, getWidth() - 12, 70 }; }
    juce::Rectangle<int> settingsRect() const { return { 6, getHeight() - 80, getWidth() - 12, 70 }; }
    void resized() override { minus.setBounds(getWidth() / 2 - 32, getHeight() - 140, 30, 26); plus.setBounds(getWidth() / 2 + 2, getHeight() - 140, 30, 26); }
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
        g.setColour(theme::secondary); g.setFont(theme::font(9));
        g.drawText(juce::String(juce::roundToInt(model.zoom * 100)) + "%", 0, getHeight() - 110, getWidth(), 18, juce::Justification::centred);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        for (int i = 0; i < 3; ++i) if (itemRect(i).contains(e.getPosition())) model.setScreen(items[i].s);
        if (settingsRect().contains(e.getPosition())) model.setScreen(Screen::settings);
        if (juce::Rectangle<int>(0, getHeight() - 112, getWidth(), 22).contains(e.getPosition())) model.setZoom(1.0);
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

struct TopBar : juce::Component {
    explicit TopBar(Model& m) : model(m),
        power(""), rec(""), tuner("Tuner"), midi("MIDI"), save("Save"), presetMenu(""),
        metro(IconButton::metronome, [&m] { m.setMetro(!m.metroOn); }, 16),
        prev(IconButton::chevronLeft, [&m] { m.stepPreset(-1); }, 13), next(IconButton::chevronRight, [&m] { m.stepPreset(1); }, 13),
        tap(m, "Tap", [&m] { m.tap(); }, MidiArmed::action("@tap", "Tap tempo")), bpm(""), meters(m),
        tunerLearn(m, MidiArmed::action("@tuner", "Tuner"), 6), metroLearn(m, MidiArmed::action("@metro", "Metronome"), 6),
        prevLearn(m, MidiArmed::action("@prev", "Previous preset"), 6), nextLearn(m, MidiArmed::action("@next", "Next preset"), 6) {
        for (auto* c : std::initializer_list<juce::Component*> { &power, &rec, &tuner, &midi, &save, &presetMenu, &metro, &prev, &next, &tap, &bpm, &meters })
            addAndMakeVisible(c);
        for (auto* o : { &tunerLearn, &metroLearn, &prevLearn, &nextLearn }) addAndMakeVisible(o);
        power.onClick = [&m] { m.toggle(); };
        rec.onClick = [&m] { m.sessions->toggleRecord(); }; rec.setTooltip("Record dry + wet (Ctrl+R)");
        tuner.onClick = [&m] { m.setTuner(!m.tunerOn); };
        midi.onClick = [&m] { m.midi->setLearning(!m.midi->learning); if (m.midi->learning) m.setScreen(Screen::rig); };
        midi.setTooltip("MIDI learn: click a knob or switch, then move a control on your MIDI device");
        save.onClick = [&m] { m.saveCurrent(); }; save.setTooltip("Save changes to this preset (Ctrl+S)");
        metro.setTooltip("Metronome"); prev.setTooltip("Previous preset"); next.setTooltip("Next preset");
        bpm.onClick = [this] { showTempo(); }; bpm.setTooltip("Tempo and metronome settings");
        presetMenu.onClick = [this] { showPresetMenu(); };
        sync();
    }
    void sync() {
        auto& m = model;
        power.setButtonText(m.running ? juce::String::fromUTF8("\xe2\x8f\xbb On") : juce::String::fromUTF8("\xe2\x8f\xbb Start"));
        power.setColour(juce::TextButton::buttonOnColourId, m.running ? juce::Colour(0xff34c759) : m.accent());
        power.setToggleState(true, juce::dontSendNotification);
        rec.setButtonText(m.sessions->recording ? juce::String::fromUTF8("\xe2\x97\x8f ") + mmss(m.sessions->recSeconds) : juce::String::fromUTF8("\xe2\x97\x8f Rec"));
        rec.setColour(juce::TextButton::textColourOffId, m.sessions->recording ? juce::Colours::red : theme::text);
        tuner.setToggleState(m.tunerOn, juce::dontSendNotification);
        midi.setToggleState(m.midi->learning, juce::dontSendNotification);
        metro.setToggleState(m.metroOn, juce::dontSendNotification);
        bpm.setButtonText(std::fmod(m.bpm, 1.0) == 0 ? juce::String(juce::roundToInt(m.bpm)) : juce::String(m.bpm, 1));
        presetMenu.setButtonText((m.presetModified() ? juce::String::fromUTF8("\xe2\x97\x8f ") : juce::String()) + m.presetName);
        save.setEnabled(m.presetModified());
        for (auto* o : { &tunerLearn, &metroLearn, &prevLearn, &nextLearn }) o->sync();
        resized(); repaint();
    }
    void resized() override {
        int x = 70;
        auto place = [&](juce::Component& c, int w, int h = 28) { c.setBounds(x, (getHeight() - h) / 2, w, h); x += w + 10; };
        x = 64 + 196;
        place(power, 80); place(meters, 170, 36); x += 100;   // latency text
        int r = getWidth() - 14;
        auto placeR = [&](juce::Component& c, int w, int h = 28) { r -= w; c.setBounds(r, (getHeight() - h) / 2, w, h); r -= 10; };
        placeR(save, 60);
        placeR(next, 26); nextLearn.setBounds(next.getBounds());
        placeR(presetMenu, juce::jlimit(140, 300, juce::GlyphArrangement::getStringWidthInt(theme::font(10), presetMenu.getButtonText()) + 40));
        placeR(prev, 26); prevLearn.setBounds(prev.getBounds());
        placeR(midi, 60); placeR(tuner, 66); tunerLearn.setBounds(tuner.getBounds());
        placeR(tap, 50); placeR(bpm, 54); placeR(metro, 34); metroLearn.setBounds(metro.getBounds());
        placeR(rec, 84);
        latencyX = x - 100;
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(theme::bar);
        g.setColour(theme::line); g.fillRect(0, getHeight() - 1, getWidth(), 1);
        drawLogo(g, juce::Rectangle<float>(14, (float) getHeight() / 2 - 20, 40, 40), model.accent());
        g.setColour(theme::text); g.setFont(theme::font(15, true, 4)); g.drawText("LEVIATHAN", 64, 10, 190, 26, juce::Justification::left);
        g.setColour(theme::secondary); g.setFont(theme::font(9, false, 1.5f)); g.drawText("OPEN GUITAR RIG", 64, 36, 160, 16, juce::Justification::left);
        if (model.running && model.info.sampleRate > 0) {
            const double total = model.info.latencyMs() + model.params.pitchLatencyMs.load();
            g.setColour(total > 15 ? juce::Colours::orange : theme::secondary); g.setFont(theme::font(10));
            g.drawText(juce::String::fromUTF8("~") + juce::String(total, 1) + " ms", latencyX, 0, 96, getHeight(), juce::Justification::centredLeft);
        }
    }
    void showPresetMenu() {
        auto& m = model;
        juce::PopupMenu menu;
        if (!m.favorites.isEmpty()) {
            menu.addSectionHeader("Favorites");
            auto fav = m.favorites; fav.sort(true);
            for (const auto& n : fav) menu.addItem(juce::String::fromUTF8("\xe2\x98\x85 ") + n, [&m, n] { m.loadAny(n); });
        }
        menu.addSectionHeader("Factory");
        for (const auto& f : FACTORY) { const juce::String n = f.name; menu.addItem(m.userPresets.count(n) ? n + " (edited)" : n, [&m, n] { m.loadFactory(n); }); }
        juce::StringArray mine; for (auto& [k, v] : m.userPresets) if (!m.isFactory(k)) mine.add(k);
        mine.sort(false);
        if (!mine.isEmpty()) { menu.addSectionHeader("Yours"); for (const auto& n : mine) menu.addItem(n, [&m, n] { m.loadUser(n); }); }
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
                metroT.setButtonText("Metronome on"); metroT.setToggleState(m.metroOn, juce::dontSendNotification); metroT.onClick = [this] { model.setMetro(metroT.getToggleState()); };
                for (int i = 1; i <= 12; ++i) beats.addItem(juce::String(i), i);
                beats.setSelectedId(m.beatsPerBar, juce::dontSendNotification); beats.onChange = [this] { model.setBeatsPerBar(beats.getSelectedId()); };
                vol.setRange(-40, 0, 0.5); vol.setValue(m.metroVol, juce::dontSendNotification); vol.setTextValueSuffix(" dB");
                vol.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22); vol.onValueChange = [this] { model.setMetroVol(vol.getValue()); };
                clockT.setButtonText("Follow incoming MIDI clock"); clockT.setToggleState(m.midi->followClock, juce::dontSendNotification);
                clockT.onClick = [this] { model.midi->setFollowClock(clockT.getToggleState()); };
                for (auto* c : std::initializer_list<juce::Component*> { &bpmS, &tapB, &metroT, &beats, &vol, &clockT }) addAndMakeVisible(c);
                setSize(400, 230);
            }
            void resized() override {
                bpmS.setBounds(130, 10, 170, 28); tapB.setBounds(310, 10, 70, 28); metroT.setBounds(130, 50, 250, 26);
                beats.setBounds(130, 88, 80, 28); vol.setBounds(130, 128, 250, 28); clockT.setBounds(130, 168, 250, 26);
            }
            void paint(juce::Graphics& g) override {
                g.setColour(theme::secondary); g.setFont(theme::font(10));
                int y = 10; for (auto* t : { "Tempo", "Metronome", "Beats per bar", "Click volume", "MIDI clock" }) { g.drawText(t, 10, y, 115, 28, juce::Justification::centredLeft); y += y == 10 ? 40 : 39; }
            }
            Model& model; juce::Slider bpmS, vol; ActionButton tapB; juce::ToggleButton metroT, clockT; juce::ComboBox beats;
        };
        juce::CallOutBox::launchAsynchronously(std::make_unique<Tempo>(model), bpm.getScreenBounds(), nullptr);
    }
    Model& model;
    juce::TextButton power, rec, tuner, midi, save, presetMenu;
    IconButton metro, prev, next;
    ActionButton tap;
    juce::TextButton bpm;
    Meters meters;
    LearnOverlay tunerLearn, metroLearn, prevLearn, nextLearn;
    int latencyX = 0;
};

// MARK: - presets screen

struct PresetsScreen : Screen_, juce::FileDragAndDropTarget {
    explicit PresetsScreen(Model& m) : model(m), importB("Import" + ellipsis), exportB("Export all" + ellipsis), saveB("Save as new" + ellipsis) {
        filter.addItem("All", 1); filter.addItem(juce::String::fromUTF8("\xe2\x98\x85 Favorites"), 2); filter.setSelectedId(1);
        filter.onChange = [this] { signature.clear(); refresh(); };
        importB.onClick = [&m] { m.importPresetsDialog(); }; importB.setTooltip("Add .leviathan preset files (or drop them here)");
        exportB.onClick = [&m] { m.exportAllPresets(); }; exportB.setTooltip("Save every preset as a .leviathan file in a folder");
        saveB.onClick = [&m] { askText("Save preset", {}, "Save", [&m](juce::String n) { m.savePreset(n); }); };
        for (auto* c : std::initializer_list<juce::Component*> { &filter, &importB, &exportB, &saveB, &viewport }) addAndMakeVisible(c);
        viewport.setViewedComponent(&list, false); viewport.setScrollBarsShown(true, false);
    }
    juce::StringArray names() const {
        auto all = model.presetOrder();
        if (filter.getSelectedId() == 2) { juce::StringArray f; for (auto& n : all) if (model.favorites.contains(n)) f.add(n); return f; }
        return all;
    }
    void refresh() override {
        juce::StringArray sig { names().joinIntoString("|"), model.favorites.joinIntoString("|"), model.presetName, model.presetModified() ? "m" : "" };
        for (auto& [k, v] : model.userPresets) sig.add(k);
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
        saveB.setBounds(head.removeFromRight(120)); head.removeFromRight(8);
        exportB.setBounds(head.removeFromRight(100)); head.removeFromRight(8);
        importB.setBounds(head.removeFromRight(90));
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
            load("Load"), exportB("Export" + ellipsis), rename("Rename"), del("Delete"), restore("Restore factory") {
            star.colour = m.favorites.contains(name) ? juce::Colours::yellow : theme::secondary;
            addAndMakeVisible(star);
            load.onClick = [this] { model.loadAny(name); model.setScreen(Screen::rig); };
            exportB.onClick = [this] { model.exportPreset(name); };
            rename.onClick = [this] { askText("Rename preset", name, "Rename", [this](juce::String n) { model.renamePreset(name, n); }); };
            del.onClick = [this] { model.deletePreset(name); };
            restore.onClick = [this] { model.restoreFactory(name); };
            for (auto* b : { &load, &exportB }) addAndMakeVisible(b);
            const bool factory = m.isFactory(name), edited = factory && m.userPresets.count(name);
            if (!factory) { addAndMakeVisible(rename); addAndMakeVisible(del); }
            else if (edited) addAndMakeVisible(restore);
        }
        void resized() override {
            star.setBounds(4, 15, 28, 28);
            int r = getWidth() - 8;
            for (auto* b : { &del, &rename, &restore, &exportB, &load }) {
                if (!b->isVisible()) continue;
                const int w = juce::GlyphArrangement::getStringWidthInt(theme::font(10), b->getButtonText()) + 24;
                r -= w; b->setBounds(r, 15, w, 28); r -= 8;
            }
            statusX = r;
        }
        void paint(juce::Graphics& g) override {
            const bool current = name == model.presetName, factory = model.isFactory(name), edited = factory && model.userPresets.count(name);
            g.setColour(theme::line.withAlpha(0.5f)); g.fillRect(0, getHeight() - 1, getWidth(), 1);
            g.setColour(theme::text); g.setFont(theme::font(14, current)); g.drawText(name, 44, 8, 600, 24, juce::Justification::centredLeft);
            g.setColour(theme::secondary); g.setFont(theme::font(10));
            g.drawText(factory ? (edited ? juce::String::fromUTF8("Factory \xc2\xb7 edited") : juce::String("Factory")) : juce::String("Yours"), 44, 32, 300, 18, juce::Justification::centredLeft);
            if (current) g.drawText(model.presetModified() ? juce::String::fromUTF8("Loaded \xc2\xb7 unsaved changes") : juce::String("Loaded"), statusX - 260, 0, 250, getHeight(), juce::Justification::centredRight);
        }
        void mouseDoubleClick(const juce::MouseEvent&) override { model.loadAny(name); model.setScreen(Screen::rig); }
        Model& model; juce::String name; IconButton star; juce::TextButton load, exportB, rename, del, restore; int statusX = 0;
    };
    Model& model; juce::ComboBox filter; juce::TextButton importB, exportB, saveB; juce::Viewport viewport; juce::Component list;
    std::vector<std::unique_ptr<Row>> rows; juce::String signature;
};

// MARK: - recordings screen

struct Waveform : juce::Component {
    explicit Waveform(Model& m) : model(m) {}
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
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if (auto* cur = model.sessions->current()) model.sessions->seek(juce::jlimit(0.0, 1.0, (double) e.x / getWidth()) * cur->duration);
    }
    Model& model;
};

struct RecordingsScreen : Screen_ {
    explicit RecordingsScreen(Model& m) : model(m), wave(m),
        prevB(IconButton::prev, [this] { step(-1); }, 14), playB(IconButton::play, [&m] { m.sessions->togglePlay(); }, 18), nextB(IconButton::next, [this] { step(1); }, 14),
        folderB(IconButton::folder, [&m] { m.sessions->reveal(); }, 16), trashB(IconButton::trash, [&m] { m.sessions->remove(); }, 16),
        liveB(""), reampB("Re-amp with current rig"), originalB(""), importB("Import recordings (WAV)" + ellipsis) {
        for (auto* c : std::initializer_list<juce::Component*> { &listView, &wave, &prevB, &playB, &nextB, &nameEd, &folderB, &trashB, &liveB, &reampB, &originalB, &progress, &importB })
            addAndMakeVisible(c);
        listView.setViewedComponent(&listInner, false); listView.setScrollBarsShown(true, false);
        prevB.setTooltip("Previous recording"); nextB.setTooltip("Next recording"); folderB.setTooltip("Show in Explorer"); trashB.setTooltip("Move to Recycle Bin");
        nameEd.setFont(theme::font(20, true)); nameEd.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        nameEd.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        nameEd.onReturnKey = [this] { model.sessions->rename(nameEd.getText()); };
        nameEd.onFocusLost = [this] { if (auto* c = model.sessions->current(); c && c->name != nameEd.getText()) model.sessions->rename(nameEd.getText()); };
        liveB.onClick = [&m] { m.sessions->toggleLiveReamp(); if (m.sessions->liveReamp) m.setScreen(Screen::rig); };
        reampB.onClick = [&m] { m.sessions->reamp(); };
        originalB.onClick = [&m] { m.sessions->loadPreset(); m.setScreen(Screen::rig); };
        importB.onClick = [&m] { m.sessions->importRecordings(); };
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
        playB.icon = s.playing ? IconButton::pause : IconButton::play;
        liveB.setButtonText(s.liveReamp ? "Stop live re-amp" : "Live re-amp");
        reampB.setVisible(has && !s.rendering); progress.setVisible(has && s.rendering.has_value());
        progressValue = s.rendering.value_or(0);
        originalB.setVisible(has && cur->settings.has_value());
        if (cur) originalB.setButtonText("Load \"" + cur->preset + "\" settings");
        importB.setVisible(!has);
        resized(); repaint(); wave.repaint();
    }
    juce::Rectangle<int> detail() const { return getLocalBounds().withTrimmedLeft(300); }
    void resized() override {
        listView.setBounds(getLocalBounds().withWidth(300));
        int y = 0; for (auto& r : rows) { r->setBounds(0, y, listView.getMaximumVisibleWidth(), 52); y += 52; }
        listInner.setSize(listView.getMaximumVisibleWidth(), y);
        auto d = detail();
        wave.setBounds(d.removeFromTop(220));
        auto transport = d.removeFromTop(44);
        playB.setBounds(transport.withSizeKeepingCentre(40, 32)); prevB.setBounds(playB.getBounds().translated(-48, 0)); nextB.setBounds(playB.getBounds().translated(48, 0));
        d.reduce(18, 18);
        auto nameRow = d.removeFromTop(40);
        trashB.setBounds(nameRow.removeFromRight(32)); folderB.setBounds(nameRow.removeFromRight(32)); nameEd.setBounds(nameRow);
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
        if (cur) { g.setColour(juce::Colour(0xff141414)); g.fillRect(detail().withTrimmedTop(220).withHeight(44)); }
        if (!cur) {
            g.setColour(theme::text); g.setFont(theme::font(16));
            g.drawText(s.list.empty() ? "No recordings yet" : "Select a recording", detail().withSizeKeepingCentre(500, 30).translated(0, -30), juce::Justification::centred);
            g.setColour(theme::secondary); g.setFont(theme::font(11));
            g.drawFittedText("Press Rec (Ctrl+R) to capture your dry guitar and the amp's output together.\nDry takes can be re-amped through any preset later.",
                             detail().withSizeKeepingCentre(760, 50).translated(0, 14), juce::Justification::centred, 2);
            return;
        }
        auto info = detail().reduced(18, 0).withY(220 + 44 + 18 + 48).withHeight(70);
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
        void mouseUp(const juce::MouseEvent&) override { model.sessions->select(dir); }
        Model& model; juce::File dir;
    };
    Model& model; juce::Viewport listView; juce::Component listInner; std::vector<std::unique_ptr<SessionRow>> rows; juce::String listSig;
    Waveform wave; IconButton prevB, playB, nextB, folderB, trashB; juce::TextEditor nameEd;
    juce::TextButton liveB, reampB, originalB, importB;
    double progressValue = 0; juce::ProgressBar progress { progressValue };
    std::vector<juce::Rectangle<int>> boxes;
};

// MARK: - settings screen

struct Group : juce::Component {
    Group(juce::String t) : title(std::move(t)) {}
    void paint(juce::Graphics& g) override {
        boxBackground(g, getLocalBounds().toFloat());
        g.setColour(theme::text); g.setFont(theme::font(14, true)); g.drawText(title, 16, 12, 400, 24, juce::Justification::centredLeft);
        g.setColour(theme::secondary); g.setFont(theme::font(10));
        for (auto& [y, l] : labels) g.drawText(l, 16, y, 170, 28, juce::Justification::centredLeft);
        if (extra) extra(g);
    }
    juce::String title;
    std::vector<std::pair<int, juce::String>> labels;   // row labels, left column
    std::function<void(juce::Graphics&)> extra;         // values painted next to the controls
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
        audio("Audio"), look("Appearance"), tun("Tuner"), upd("Updates"), mid("MIDI"), lib("Library"), fb("Help & feedback"), about("About") {
        addAndMakeVisible(viewport); viewport.setViewedComponent(&inner, false); viewport.setScrollBarsShown(true, false);
        for (auto* g : { &audio, &look, &tun, &upd, &mid, &lib, &fb, &about }) inner.addAndMakeVisible(g);
        const juce::String site = juce::String("https://github.com/") + Updater::repo;
        licB.setButtonText("License"); licB.onClick = [site] { juce::URL(site + "/blob/main/LICENSE").launchInDefaultBrowser(); };
        noticeB.setButtonText("Notice & credits"); noticeB.onClick = [site] { juce::URL(site + "/blob/main/NOTICE.md").launchInDefaultBrowser(); };
        srcB.setButtonText("Source code"); srcB.onClick = [site] { juce::URL(site).launchInDefaultBrowser(); };
        for (auto* b : { &licB, &noticeB, &srcB }) about.addAndMakeVisible(b);
        bugB.setButtonText("Report a bug" + ellipsis); bugB.onClick = [this] { openFeedback(model, "bug_report.yml"); };
        featB.setButtonText("Request a feature" + ellipsis); featB.onClick = [this] { openFeedback(model, "feature_request.yml"); };
        fb.addAndMakeVisible(bugB); fb.addAndMakeVisible(featB);
        audio.addAndMakeVisible(audioSel);
        audio.addAndMakeVisible(inputCh); audio.addAndMakeVisible(audioInfo);
        inputCh.onChange = [this] { if (inputCh.getSelectedId() > 0) model.set("inputChannel", inputCh.getSelectedId() - 1); };
        m.devices.addChangeListener(&audioWatch);
        audioWatch.fn = [this] { model.restartAudio(); signature.clear(); refresh(); };

        // appearance
        static const juce::Colour swatches[] = { juce::Colour::fromFloatRGBA(1, 0.3f, 0.3f, 1), juce::Colour::fromFloatRGBA(0.91f, 0.57f, 0.23f, 1),
            juce::Colour::fromFloatRGBA(0.95f, 0.8f, 0.25f, 1), juce::Colour::fromFloatRGBA(0.35f, 0.8f, 0.45f, 1), juce::Colour::fromFloatRGBA(0.35f, 0.7f, 1, 1),
            juce::Colour::fromFloatRGBA(0.6f, 0.45f, 1, 1), juce::Colour::fromFloatRGBA(1, 0.45f, 0.75f, 1), juce::Colour::fromFloatRGBA(0.85f, 0.85f, 0.85f, 1) };
        for (auto c : swatches) { auto b = std::make_unique<Swatch>(m, c); look.addAndMakeVisible(*b); swatchButtons.push_back(std::move(b)); }
        followAmp.setButtonText("Follow amp channel"); followAmp.onClick = [this] { model.setAccent(std::nullopt); };
        zoomMinus.setButtonText(juce::String::fromUTF8("\xe2\x88\x92")); zoomMinus.onClick = [this] { model.zoomStep(-1); };
        zoomPlus.setButtonText("+"); zoomPlus.onClick = [this] { model.zoomStep(1); };
        for (auto* c : std::initializer_list<juce::Component*> { &followAmp, &zoomMinus, &zoomPlus }) look.addAndMakeVisible(c);

        // tuner
        a4.setSliderStyle(juce::Slider::IncDecButtons); a4.setRange(430, 450, 1); a4.setTextValueSuffix(" Hz");
        a4.onValueChange = [this] { model.setA4(a4.getValue()); };
        for (size_t i = 0; i < TUNINGS.size(); ++i) tuning.addItem(TUNINGS[i].name, (int) i + 1);
        tuning.onChange = [this] { model.setTuningIndex(tuning.getSelectedId() - 1); };
        tun.addAndMakeVisible(a4); tun.addAndMakeVisible(tuning);

        // updates
        checkNow.setButtonText("Check now"); checkNow.onClick = [this] { model.updater->check(); };
        autoUpd.setButtonText("Check for updates when Leviathan opens"); autoUpd.onClick = [this] { model.updater->setAutoCheck(autoUpd.getToggleState()); };
        installB.setButtonText("Update and relaunch"); installB.setToggleState(true, juce::dontSendNotification); installB.onClick = [this] { model.updater->install(); };
        pageB.setButtonText("View on GitHub"); pageB.onClick = [this] { if (model.updater->release) model.updater->release->page.launchInDefaultBrowser(); };
        notes.setMultiLine(true); notes.setReadOnly(true); notes.setFont(theme::font(10));
        for (auto* c : std::initializer_list<juce::Component*> { &checkNow, &autoUpd, &installB, &pageB, &notes, &updProgress }) upd.addAndMakeVisible(c);

        // midi
        pcT.setButtonText(juce::String::fromUTF8("Program 1, 2, 3\xe2\x80\xa6 loads the 1st, 2nd, 3rd\xe2\x80\xa6 preset in the Presets list"));
        pcT.onClick = [this] { model.midi->setProgramChange(pcT.getToggleState()); };
        learnB.setButtonText(juce::String::fromUTF8("Learn on the rig\xe2\x80\xa6")); learnB.onClick = [this] { model.setScreen(Screen::rig); model.midi->setLearning(true); };
        clearB.setButtonText("Clear all"); clearB.onClick = [this] { model.midi->clear(); };
        for (auto* c : std::initializer_list<juce::Component*> { &pcT, &learnB, &clearB }) mid.addAndMakeVisible(c);

        // library
        importRec.setButtonText("Import recordings (WAV)" + ellipsis); importRec.onClick = [this] { model.sessions->importRecordings(); };
        openRec.setButtonText("Open folder"); openRec.onClick = [] { Sessions::root().createDirectory(); Sessions::root().startAsProcess(); };
        openCap.setButtonText("Open folder"); openCap.onClick = [] { Capture::dir().createDirectory(); Capture::dir().startAsProcess(); };
        rescan.setButtonText("Rescan"); rescan.onClick = [this] { model.reloadCaptures(); };
        openIR.setButtonText("Open IR folder"); openIR.onClick = [] { IRLibrary::root().createDirectory(); IRLibrary::root().startAsProcess(); };
        for (auto* c : std::initializer_list<juce::Component*> { &importRec, &openRec, &openCap, &rescan, &openIR }) lib.addAndMakeVisible(c);
        refresh();
    }
    ~SettingsScreen() override { model.devices.removeChangeListener(&audioWatch); }

    struct Watch : juce::ChangeListener { std::function<void()> fn; void changeListenerCallback(juce::ChangeBroadcaster*) override { if (fn) fn(); } } audioWatch;

    void refresh() override {
        auto& m = model;
        // input channels
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
            for (auto& mp : m.midi->maps) {
                auto r = std::make_unique<MapRow>(m, mp); mid.addAndMakeVisible(*r); mapRows.push_back(std::move(r));
            }
            notes.setText(m.updater->release ? m.updater->release->notes : juce::String(), false);
        }
        inputCh.setSelectedId(m.s().i("inputChannel") + 1, juce::dontSendNotification);
        a4.setValue(m.a4, juce::dontSendNotification);
        tuning.setSelectedId(m.tuningIndex + 1, juce::dontSendNotification);
        followAmp.setEnabled(m.accentPick.has_value());
        autoUpd.setToggleState(m.updater->autoCheck, juce::dontSendNotification);
        checkNow.setEnabled(!m.updater->busy());
        const bool avail = m.updater->phase == Updater::Phase::available;
        installB.setVisible(avail); pageB.setVisible(m.updater->release.has_value()); notes.setVisible(avail && notes.getText().isNotEmpty());
        updProgress.setVisible(m.updater->phase == Updater::Phase::downloading); progressValue = m.updater->progressValue;
        pcT.setToggleState(m.midi->programChange, juce::dontSendNotification);
        clearB.setEnabled(!m.midi->maps.empty());
        resized(); repaint();
        for (auto* g : { &audio, &look, &tun, &upd, &mid, &lib, &fb, &about }) g->repaint();
    }
    void resized() override {
        viewport.setBounds(getLocalBounds());
        const int w = std::min(viewport.getMaximumVisibleWidth() - 48, 1000), x = 24;
        int y = 48;
        auto group = [&](Group& g, int h) { g.setBounds(x, y, w, h); y += h + 18; };
        // audio
        audioSel.setBounds(16, 44, std::min(w - 32, 700), 800);   // it shrinks itself to fit its controls
        const int selH = audioSel.getHeight();
        inputCh.setBounds(16 + 176, 44 + selH + 8, 360, 28);
        audioInfo.setBounds(16, 44 + selH + 44, w - 32, 70);
        audio.labels = { { 44 + selH + 8, "Input channel" } };
        group(audio, 44 + selH + 44 + 80);
        // appearance
        { int sx = 192; for (auto& b : swatchButtons) { b->setBounds(sx, 48, 24, 24); sx += 30; } followAmp.setBounds(sx + 10, 46, 170, 28); }
        zoomMinus.setBounds(192, 86, 30, 28); zoomPlus.setBounds(290, 86, 30, 28);
        look.labels = { { 46, "Accent colour" }, { 86, "Zoom" } };
        look.extra = [this](juce::Graphics& g) { g.setColour(theme::text); g.drawText(juce::String(juce::roundToInt(model.zoom * 100)) + "%", 222, 86, 68, 28, juce::Justification::centred); };
        group(look, 130);
        a4.setBounds(192, 44, 160, 28); tuning.setBounds(192, 80, 220, 28);
        tun.labels = { { 44, "Reference (A4)" }, { 80, "Guitar tuning" } };
        group(tun, 124);
        checkNow.setBounds(260, 44, 110, 28); autoUpd.setBounds(192, 80, 400, 26);
        int uy = 116;
        if (notes.isVisible()) { notes.setBounds(16, uy, w - 32, 130); uy += 140; }
        installB.setBounds(16, uy, 180, 30); pageB.setBounds(installB.isVisible() ? 206 : 16, uy, 140, 30); updProgress.setBounds(380, 44, 200, 24);
        upd.labels = { { 44, "Version" }, { 80, "Automatic" } };
        upd.extra = [this](juce::Graphics& g) {
            auto& u = *model.updater;
            g.setColour(theme::text); g.drawText(Updater::current(), 192, 44, 60, 28, juce::Justification::centredLeft);
            juce::String st; juce::Colour c = theme::secondary;
            switch (u.phase) {
            case Updater::Phase::checking: st = "Checking" + ellipsis; break;
            case Updater::Phase::upToDate: st = "Up to date"; break;
            case Updater::Phase::available: st = "Leviathan " + u.release->version + " is available"; c = theme::text; break;
            case Updater::Phase::downloading: st = "Downloading" + ellipsis; break;
            case Updater::Phase::installing: st = "Installing" + ellipsis; break;
            case Updater::Phase::failed: st = u.error; c = juce::Colours::orange; break;
            default: break;
            }
            g.setColour(c); g.drawFittedText(st, u.phase == Updater::Phase::downloading ? 590 : 380, 44, 560, 28, juce::Justification::centredLeft, 1);
        };
        group(upd, uy + (installB.isVisible() || pageB.isVisible() ? 46 : 0));
        int my = 120; pcT.setBounds(192, 80 + 36, w - 210, 26); my += 36;
        learnB.setBounds(192, my, 150, 28); clearB.setBounds(350, my, 90, 28); my += 44;
        if (mapRows.empty()) my += 40;
        for (auto& r : mapRows) { r->setBounds(16, my, w - 32, 30); my += 32; }
        mid.labels = { { 44, "Devices" }, { 80, "Last message" }, { 116, "Program change" }, { 156, "Assignments" } };
        mid.extra = [this, my](juce::Graphics& g) {
            auto& md = *model.midi;
            g.setColour(md.sources.isEmpty() ? theme::secondary : theme::text);
            g.drawFittedText(md.sources.isEmpty() ? juce::String("None connected") : md.sources.joinIntoString(", "), 192, 44, 760, 28, juce::Justification::centredLeft, 1);
            g.setColour(theme::secondary);
            g.drawText(md.last.isEmpty() ? juce::String::fromUTF8("\xe2\x80\x94") : md.last, 192, 80, 400, 28, juce::Justification::centredLeft);
            if (md.maps.empty())
                g.drawFittedText("Nothing assigned. Use Learn, click a knob or switch, then move a control on your MIDI device. An expression pedal on Wah Position makes a real wah.",
                                 16, my - 40, 900, 34, juce::Justification::topLeft, 2);
        };
        group(mid, my + 10);
        importRec.setBounds(192, 44, 230, 28); openRec.setBounds(430, 44, 110, 28);
        openCap.setBounds(192, 80, 110, 28); rescan.setBounds(310, 80, 80, 28);
        openIR.setBounds(192, 116, 140, 28);
        lib.labels = { { 44, "Recordings" }, { 80, "Captures" }, { 116, "IR library" } };
        lib.extra = [this](juce::Graphics& g) {
            g.setColour(theme::secondary);
            g.drawText(juce::String((int) model.captures.size()) + " loaded", 400, 80, 200, 28, juce::Justification::centredLeft);
            g.drawText(model.irLib.isEmpty() ? juce::String("No God's Cab pack found in ") + IRLibrary::root().getFullPathName() : juce::String("God's Cab pack found"),
                       340, 116, 700, 28, juce::Justification::centredLeft);
        };
        group(lib, 160);
        bugB.setBounds(192, 44, 150, 28); featB.setBounds(350, 44, 170, 28);
        fb.labels = { { 44, "GitHub" } };
        fb.extra = [](juce::Graphics& g) {
            g.setColour(theme::secondary);
            g.drawFittedText("Opens a form on GitHub (a free account is needed) with your Leviathan version, Windows version and audio devices filled in. "
                             "You can see and change everything before you submit.", 16, 80, 900, 34, juce::Justification::topLeft, 2);
        };
        group(fb, 124);
        licB.setBounds(192, 80, 90, 28); noticeB.setBounds(290, 80, 140, 28); srcB.setBounds(438, 80, 120, 28);
        about.labels = { { 44, "Leviathan" }, { 80, "License" } };
        about.extra = [](juce::Graphics& g) {
            g.setColour(theme::text); g.setFont(theme::font(10, true));
            g.drawText("by Shane O'Reilly", 192, 44, 200, 28, juce::Justification::centredLeft);
            g.setColour(theme::secondary); g.setFont(theme::font(10));
            g.drawText(juce::String::fromUTF8("\xc2\xb7 version ") + Updater::current(), 372, 44, 200, 28, juce::Justification::centredLeft);
            g.drawText("Free software under the GNU AGPLv3, with no warranty.", 570, 80, 420, 28, juce::Justification::centredLeft);
            g.drawFittedText(juce::String::fromUTF8("You're free to use, share and change Leviathan. Anything made from it must also be open source under the AGPLv3 and keep the credit "
                                                    "\xe2\x80\x9cLeviathan by Shane O'Reilly\xe2\x80\x9d.\n"
                                                    "Built with JUCE (AGPLv3, \xc2\xa9 Raw Material Software) and the Steinberg ASIO SDK (GPLv3). ASIO is a trademark of Steinberg Media Technologies GmbH."),
                             16, 118, 960, 76, juce::Justification::topLeft, 4);
        };
        group(about, 206);
        inner.setSize(viewport.getMaximumVisibleWidth(), y + 20);
    }
    void paint(juce::Graphics& g) override { drawSectionTitle(g, "SETTINGS", { 24, 12, 200, 28 }); }

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
        void resized() override { momentary.setBounds(500, 2, 180, 26); x.setBounds(700, 4, 22, 22); }
        void paint(juce::Graphics& g) override {
            g.setFont(theme::font(10)); g.setColour(theme::secondary); g.drawText(label, 0, 0, 170, getHeight(), juce::Justification::centredLeft);
            g.setColour(theme::text); g.drawText(name, 176, 0, 310, getHeight(), juce::Justification::centredLeft);
        }
        Model& model; juce::String id, label, name; IconButton x; juce::ToggleButton momentary;
    };

    struct AudioInfo : juce::Component {
        explicit AudioInfo(Model& m) : model(m) {}
        void paint(juce::Graphics& g) override {
            g.setFont(theme::font(10)); g.setColour(theme::secondary);
            g.drawText("Input channel", 0, -36, 170, 28, juce::Justification::centredLeft);
            const auto& i = model.info;
            if (i.sampleRate <= 0) { g.drawText("Not running.", 0, 0, getWidth(), 20, juce::Justification::centredLeft); return; }
            g.setColour(theme::text);
            g.drawText("Latency: ~" + juce::String(i.latencyMs(), 1) + " ms round trip (" + i.breakdown() + ")" + dot
                           + juce::String(i.sampleRate / 1000, 1) + " kHz" + dot + juce::String(i.buffer) + " samples" + dot + i.type,
                       0, 0, getWidth(), 20, juce::Justification::centredLeft);
            g.setColour(model.dropouts > 0 || model.cpu > 0.6f ? juce::Colours::orange : theme::secondary);
            g.drawText("DSP load " + juce::String(juce::roundToInt(model.cpu * 100)) + "%" + dot + juce::String(model.dropouts) + " dropout" + (model.dropouts == 1 ? "" : "s"),
                       0, 22, getWidth(), 20, juce::Justification::centredLeft);
            g.setColour(theme::secondary);
            g.drawFittedText("Smallest buffer that never crackles is best. ASIO (your interface's own driver) is fastest; \"Windows Audio (Exclusive Mode)\" is next. "
                             "Bluetooth headphones and monitor speakers add delay you can't see here.", 0, 44, getWidth(), 30, juce::Justification::topLeft, 2);
        }
        Model& model;
    };

    struct LabelsPainter;
    void paintOverChildren(juce::Graphics&) override {}

    Model& model;
    juce::Viewport viewport; juce::Component inner;
    juce::AudioDeviceSelectorComponent audioSel;
    Group audio, look, tun, upd, mid, lib, fb, about;
    juce::TextButton bugB, featB, licB, noticeB, srcB;
    juce::ComboBox inputCh; AudioInfo audioInfo { model };
    std::vector<std::unique_ptr<Swatch>> swatchButtons;
    juce::TextButton followAmp, zoomMinus, zoomPlus;
    juce::Slider a4; juce::ComboBox tuning;
    juce::TextButton checkNow, installB, pageB; juce::ToggleButton autoUpd; juce::TextEditor notes;
    double progressValue = 0; juce::ProgressBar updProgress { progressValue };
    juce::ToggleButton pcT; juce::TextButton learnB, clearB;
    std::vector<std::unique_ptr<MapRow>> mapRows;
    juce::TextButton importRec, openRec, openCap, rescan, openIR;
    juce::String signature;
};

/// Text painted inside the settings groups (row labels and values that change).
struct GroupText : juce::Component {};

// MARK: - banners

struct Banner : juce::Component {
    Banner(juce::String t, juce::Colour bgc, juce::Colour fg, std::function<void()> close, juce::String action = {}, std::function<void()> act = {})
        : text(std::move(t)), bgColour(bgc), fgColour(fg), x(IconButton::cross, std::move(close), 11) {
        addAndMakeVisible(x);
        if (act) { button.setButtonText(action); button.onClick = std::move(act); addAndMakeVisible(button); }
    }
    void resized() override { x.setBounds(getWidth() - 34, (getHeight() - 24) / 2, 24, 24); if (button.isVisible()) button.setBounds(getWidth() - 110, (getHeight() - 26) / 2, 66, 26); }
    void paint(juce::Graphics& g) override {
        g.fillAll(bgColour); g.setColour(fgColour); g.setFont(theme::font(12));
        g.drawFittedText(text, getLocalBounds().reduced(10, 0).withTrimmedRight(button.isVisible() ? 120 : 40), juce::Justification::centredLeft, 2);
    }
    juce::String text; juce::Colour bgColour, fgColour; IconButton x; juce::TextButton button;
};

} // namespace

// MARK: - root

Root::Root(Model& m) : model(m) {
    setLookAndFeel(&lnf);
    lnf.setAccent(m.accent());
    sidebar = std::make_unique<Sidebar>(m); addAndMakeVisible(*sidebar);
    topbar = std::make_unique<TopBar>(m); addAndMakeVisible(*topbar);
    rig = std::make_unique<RigView>(m); addChildComponent(*rig);
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
    juce::StringArray sig { m.status, m.sessions->message, (m.midi->learning && m.screen == Screen::rig) ? "learn" + (m.midi->armed ? m.midi->armed->key : juce::String()) : juce::String() };
    if (sig.joinIntoString("|") == bannerSig) return;
    bannerSig = sig.joinIntoString("|");
    banners.clear();
    const juce::Colour warnBg(0xff291f0f), warnFg(0xffffd999);
    if (m.status.isNotEmpty()) banners.add(new Banner(m.status, warnBg, warnFg, [&m] { m.status = {}; m.changed(); }));
    if (m.sessions->message.isNotEmpty()) banners.add(new Banner(m.sessions->message, warnBg, warnFg, [&m] { m.sessions->message = {}; m.changed(); }));
    if (m.midi->learning && m.screen == Screen::rig) {
        const auto t = m.midi->armed ? "Move a knob, pedal or switch on your MIDI device to assign it to " + m.midi->armed->name + "."
                                     : juce::String("MIDI learn: click a knob or an ON/OFF switch, then move a control on your MIDI device.")
                                           + (m.midi->sources.isEmpty() ? "   No MIDI devices connected." : "");
        banners.add(new Banner(t, m.accent().withAlpha(0.18f).overlaidWith(theme::bg.withAlpha(0.0f)), theme::text, [&m] { m.midi->setLearning(false); }, "Done",
                               [&m] { m.midi->setLearning(false); }));
    }
    for (auto* b : banners) addAndMakeVisible(b);
}

void Root::changeListenerCallback(juce::ChangeBroadcaster*) {
    lnf.setAccent(model.accent());
    rebuildBanners();
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
    sidebar->setBounds(r.removeFromLeft(92));
    topbar->setBounds(r.removeFromTop(64));
    for (auto* b : banners) b->setBounds(r.removeFromTop(40));
    for (auto* s : std::initializer_list<juce::Component*> { rig.get(), presets.get(), recordings.get(), settings.get() }) s->setBounds(r);
}

bool Root::keyPressed(const juce::KeyPress& k) {
    const bool cmd = k.getModifiers().isCommandDown();
    if (k == juce::KeyPress::spaceKey && !cmd) { model.toggle(); return true; }
    if (cmd && k.getKeyCode() == 'R') { model.sessions->toggleRecord(); return true; }
    if (cmd && k.getKeyCode() == 'S') { model.saveCurrent(); return true; }
    if (cmd && (k.getKeyCode() == '=' || k.getKeyCode() == '+')) { model.zoomStep(1); return true; }
    if (cmd && k.getKeyCode() == '-') { model.zoomStep(-1); return true; }
    if (cmd && k.getKeyCode() == '0') { model.setZoom(1.0); return true; }
    if (!cmd && (k.getKeyCode() == 'T' || k.getKeyCode() == 't')) { model.setTuner(!model.tunerOn); return true; }
    if (k == juce::KeyPress::escapeKey && model.midi->learning) { model.midi->setLearning(false); return true; }
    if (k == juce::KeyPress::returnKey && model.screen == Screen::recordings) { model.sessions->togglePlay(); return true; }
    return false;
}

} // namespace lev
