#include "PlayAs.h"
#include <BinaryData.h>

namespace lev::playas {

namespace {
struct Band { juce::String name; std::vector<std::pair<juce::String, std::array<int, 6>>> tunings; };

/// The band list, read once from the app's resources.
const std::vector<Band>& bands() {
    static const std::vector<Band> all = [] {
        std::vector<Band> v;
        const auto json = juce::JSON::parse(juce::String::fromUTF8(BinaryData::bands_json, BinaryData::bands_jsonSize));
        if (auto* arr = json.getProperty("bands", {}).getArray())
            for (auto& b : *arr) {
                Band band { b.getProperty("name", {}).toString(), {} };
                if (auto* ts = b.getProperty("tunings", {}).getArray())
                    for (auto& t : *ts)
                        if (auto* s = t.getProperty("strings", {}).getArray(); s != nullptr && s->size() == 6) {
                            std::array<int, 6> st {}; for (int i = 0; i < 6; ++i) st[(size_t) i] = (int) (*s)[i];
                            band.tunings.emplace_back(t.getProperty("name", {}).toString(), st);
                        }
                if (band.name.isNotEmpty() && !band.tunings.empty()) v.push_back(std::move(band));
            }
        return v;
    }();
    return all;
}

/// The shift that makes your guitar sound in a tuning, if shifting every string the same gets there (within an octave).
std::optional<int> shiftTo(const Model& m, const std::array<int, 6>& s) {
    const auto& g = m.tuning().strings;
    const int k = s[0] - g[0];
    if (std::abs(k) > 12) return std::nullopt;
    for (size_t i = 0; i < 6; ++i) if (s[i] - g[i] != k) return std::nullopt;
    return k;
}

/// What you sound like now: the shift while the shifter is on, else none.
int currentShift(const Model& m) { return m.s().b("pitchOn") ? (int) std::round(m.s().d("pitchSemis")) : 0; }

juce::String shifted(int k) { return k == 0 ? juce::String("  (no shift)") : juce::String::formatted("  (%+d)", k); }

/// guitar: your guitar's tunings (no search). Otherwise Play as: tunings reachable from it, then bands, with search.
class Browser : public juce::Component, private juce::ListBoxModel {
public:
    Browser(Model& m, bool guitarList) : model(m), guitar(guitarList) {
        search.setTextToShowWhenEmpty(juce::String::fromUTF8("Search bands or tunings\xe2\x80\xa6"), theme::secondary);
        search.setFont(theme::font(10.5f)); search.setIndents(10, 0); search.setJustification(juce::Justification::centredLeft);
        search.setColour(juce::TextEditor::backgroundColourId, theme::tint.withAlpha(0.06f));
        search.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.onTextChange = [this] { refresh(); };
        if (!guitar) addAndMakeVisible(search);
        list.setModel(this); list.setRowHeight(onAndroid ? 40 : 26);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(list);
        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds;
        if (guitar) setSize(std::min(340, (int) area.getWidth() - 40), (int) TUNINGS.size() * list.getRowHeight() + 20);
        else setSize(std::min(440, (int) area.getWidth() - 40), std::min(480, (int) area.getHeight() - 80));
        refresh();
    }
    void visibilityChanged() override { if (isShowing() && !onAndroid && !guitar) search.grabKeyboardFocus(); }
    void resized() override {
        if (guitar) { list.setBounds(getLocalBounds().reduced(6, 10)); return; }
        search.setBounds(12, 12, getWidth() - 24, 36);
        list.setBounds(getLocalBounds().withTrimmedTop(56).withTrimmedBottom(32).reduced(6, 4));
    }
    void paint(juce::Graphics& g) override {
        if (guitar) return;
        g.setColour(theme::line); g.fillRect(0, 54, getWidth(), 1); g.fillRect(0, getHeight() - 31, getWidth(), 1);
        g.setColour(theme::secondary); g.setFont(theme::font(9));
        g.drawText("Your guitar: " + juce::String(model.tuning().name) + ". Click to hear it.", 12, getHeight() - 30, getWidth() - 24, 30, juce::Justification::centredLeft);
        if (rows.empty()) {
            g.setFont(theme::font(10.5f));
            g.drawText(juce::String::fromUTF8("Nothing matches \xe2\x80\x9c") + search.getText() + juce::String::fromUTF8("\xe2\x80\x9d."),
                       getLocalBounds().withTrimmedTop(80), juce::Justification::centredTop);
        }
    }
private:
    struct Row { bool header, band; juce::String title, detail; std::optional<int> shift; };
    void refresh() {
        rows.clear();
        if (guitar) {
            for (int i : tuningOrder()) rows.push_back({ false, false, TUNINGS[(size_t) i].name, tuningNotes(TUNINGS[(size_t) i].strings, TUNINGS[(size_t) i].flats), i });
            list.updateContent(); return;
        }
        juce::StringArray words; words.addTokens(search.getText().toLowerCase(), " ", ""); words.removeEmptyStrings();
        auto match = [&](const juce::String& s) { const auto l = s.toLowerCase(); for (auto& w : words) if (!l.contains(w)) return false; return true; };
        std::vector<Row> tunings, bandRows;
        for (auto& [name, k] : model.playAsOptions()) {
            auto st = model.tuning().strings; for (auto& n : st) n += k;
            const auto notes = tuningNotes(st, model.tuning().flats);
            if (match(name + " " + notes)) tunings.push_back({ false, false, name, notes + shifted(k), k });
        }
        for (auto& b : bands())
            for (auto& [tname, st] : b.tunings) {
                if (!match(b.name + " " + tname)) continue;
                const auto k = shiftTo(model, st);
                bandRows.push_back({ false, true, b.name, k ? tname + shifted(*k) : tname + ": retune your guitar for this one", k });
            }
        if (!tunings.empty()) { rows.push_back({ true, false, "TUNINGS", {}, {} }); rows.insert(rows.end(), tunings.begin(), tunings.end()); }
        if (!bandRows.empty()) { rows.push_back({ true, true, "BANDS", {}, {} }); rows.insert(rows.end(), bandRows.begin(), bandRows.end()); }
        list.updateContent(); repaint();
        if (words.isEmpty()) {
            const int cur = currentShift(model);
            for (size_t i = 0; i < rows.size(); ++i) if (!rows[i].header && !rows[i].band && rows[i].shift == cur) list.scrollToEnsureRowIsOnscreen((int) i + 6);
        }
    }
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem(int r, juce::Graphics& g, int w, int h, bool) override {
        if (!juce::isPositiveAndBelow(r, (int) rows.size())) return;
        const auto& row = rows[(size_t) r];
        if (row.header) {
            g.setColour(theme::secondary); g.setFont(theme::font(9, true, 1));
            g.drawText(row.title, 10, 0, w - 20, h, juce::Justification::bottomLeft);
            return;
        }
        const bool on = guitar ? row.shift == model.tuningIndex : !row.band && row.shift == currentShift(model);
        if (on) { g.setColour(model.accent().withAlpha(0.15f)); g.fillRoundedRectangle(0, 1, (float) w, (float) h - 2, 6); }
        const float alpha = row.shift ? 1.0f : 0.45f;
        g.setFont(theme::font(9.5f));
        const int dw = std::min(w / 2, juce::GlyphArrangement::getStringWidthInt(theme::font(9.5f), row.detail) + 4);
        g.setColour(theme::secondary.withMultipliedAlpha(alpha));
        g.drawText(row.detail, w - 12 - dw, 0, dw, h, juce::Justification::centredRight, true);
        g.setColour((on ? model.accent() : theme::text).withMultipliedAlpha(alpha)); g.setFont(theme::font(10.5f, on));
        g.drawText(row.title, 12, 0, w - 36 - dw, h, juce::Justification::centredLeft, true);
    }
    void listBoxItemClicked(int r, const juce::MouseEvent&) override {
        if (!juce::isPositiveAndBelow(r, (int) rows.size())) return;
        const auto& row = rows[(size_t) r];
        if (row.header || !row.shift) return;
        if (guitar) model.setTuningIndex(*row.shift); else model.playAs(*row.shift);
        list.repaint();
    }
    Model& model; bool guitar;
    juce::TextEditor search; juce::ListBox list;
    std::vector<Row> rows;
};
} // namespace

/// One electric guitar, outlined and tilted like the emoji: a double-cutaway body drawn as one smooth outline, two
/// pickups, neck and an angled headstock, fitted into r (Mac: guitarStrokes, the same points).
juce::Path guitarPath(juce::Rectangle<float> r) {
    using Pts = std::vector<juce::Point<double>>;
    static const Pts ctrl { { 0.7, 0.8 }, { 1.6, -0.4 }, { 2.6, -1.8 }, { 3.5, -1.7 }, { 3.9, -0.4 }, { 3.5, 1.6 }, { 4.8, 3.6 }, { 5.1, 6.2 },
        { 4.2, 8.6 }, { 2.0, 9.9 }, { 0, 10.2 }, { -2.2, 9.9 }, { -4.4, 8.6 }, { -5.2, 6.2 }, { -5.0, 3.6 }, { -3.8, 1.6 }, { -4.4, -0.8 },
        { -4.0, -2.8 }, { -3.0, -3.2 }, { -2.0, -1.8 }, { -0.7, 0.8 } };
    Pts body;   // a closed Catmull-Rom curve through the control points
    const int n = (int) ctrl.size();
    for (int i = 0; i < n; ++i) {
        const auto& p0 = ctrl[(size_t) ((i - 1 + n) % n)]; const auto& p1 = ctrl[(size_t) i]; const auto& p2 = ctrl[(size_t) ((i + 1) % n)]; const auto& p3 = ctrl[(size_t) ((i + 2) % n)];
        for (int s = 0; s < 6; ++s) {
            const double t = s / 6.0, t2 = t * t, t3 = t2 * t;
            auto cr = [&](double a, double b, double c, double d) { return 0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3); };
            body.push_back({ cr(p0.x, p1.x, p2.x, p3.x), cr(p0.y, p1.y, p2.y, p3.y) });
        }
    }
    std::vector<std::pair<Pts, bool>> all { { body, true }, { { { 0, 0.8 }, { 0, -11 } }, false },
        { { { -0.7, -11 }, { -1.0, -13.8 }, { 0, -15 }, { 1.3, -14.4 }, { 0.7, -11 } }, true },
        { { { -1.6, 3.4 }, { 1.6, 3.4 } }, false }, { { { -1.6, 5.6 }, { 1.6, 5.6 } }, false } };
    const double c = std::cos(juce::MathConstants<double>::pi / 4), sn = std::sin(juce::MathConstants<double>::pi / 4);   // headstock top right
    double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
    for (auto& [pts, closed] : all) for (auto& q : pts) { q = { q.x * c - q.y * sn, q.x * sn + q.y * c }; x0 = std::min(x0, q.x); x1 = std::max(x1, q.x); y0 = std::min(y0, q.y); y1 = std::max(y1, q.y); }
    const double k = std::min(r.getWidth() / (x1 - x0), r.getHeight() / (y1 - y0));
    const double ox = r.getCentreX() - (x0 + (x1 - x0) / 2) * k, oy = r.getCentreY() - (y0 + (y1 - y0) / 2) * k;
    juce::Path p;
    for (auto& [pts, closed] : all) {
        for (size_t i = 0; i < pts.size(); ++i) {
            const auto pt = juce::Point<float>((float) (ox + pts[i].x * k), (float) (oy + pts[i].y * k));
            if (i == 0) p.startNewSubPath(pt); else p.lineTo(pt);
        }
        if (closed) p.closeSubPath();
    }
    return p;
}

/// One Pitch row's button: [icon  name ▾], one line; the guitar and play-as rows look the same (Mac: PitchSelector).
class RowButton : public juce::Component, public juce::SettableTooltipClient {
public:
    RowButton(Model& m, bool guitarRow) : model(m), guitar(guitarRow) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setPref(*this, (int) (150 * theme::W), 30);
        setTooltip(guitar ? "Your guitar's tuning" : "Play as: what you sound like. Search tunings and bands");
    }
    juce::String name() const {
        if (guitar) return model.tuning().name;
        auto s = model.tuning().strings; const int k = currentShift(model); for (auto& n : s) n += k;
        return tuningName(s);
    }
    void paint(juce::Graphics& g) override {
        auto in = theme::drawDropdownBox(g, getLocalBounds());
        const auto icon = in.removeFromLeft(18).toFloat().withSizeKeepingCentre(16, 16);
        juce::Path p;
        if (guitar) p = guitarPath(icon);
        else {        // cone and two waves
            p.startNewSubPath(icon.getX(), icon.getCentreY() - 3); p.lineTo(icon.getX() + 3, icon.getCentreY() - 3); p.lineTo(icon.getX() + 7, icon.getY() + 2);
            p.lineTo(icon.getX() + 7, icon.getBottom() - 2); p.lineTo(icon.getX() + 3, icon.getCentreY() + 3); p.lineTo(icon.getX(), icon.getCentreY() + 3); p.closeSubPath();
            p.addCentredArc(icon.getX() + 8, icon.getCentreY(), 4, 4, 0, 0.6f, 2.5f, true);
            p.addCentredArc(icon.getX() + 8, icon.getCentreY(), 7.5f, 7.5f, 0, 0.6f, 2.5f, true);
        }
        g.setColour(theme::secondary); g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        in.removeFromLeft(8);
        g.setColour(theme::text); g.setFont(theme::font(10.5f));   // as the other dropdowns (combo boxes)
        g.drawText(name(), in, juce::Justification::centredLeft, true);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if (getLocalBounds().contains(e.getPosition()) && !e.mouseWasDraggedSinceMouseDown())
            juce::CallOutBox::launchAsynchronously(std::make_unique<Browser>(model, guitar), getScreenBounds(), nullptr);
    }
private:
    Model& model; bool guitar;
};

std::unique_ptr<juce::Component> makeGuitarButton(Model& m) { return std::make_unique<RowButton>(m, true); }

std::unique_ptr<juce::Component> makeButton(Model& m) { return std::make_unique<RowButton>(m, false); }

}
