#include "AmpBrowser.h"

namespace lev::amp {

namespace {
struct Item { juce::String id, name, section; };

juce::String currentId(Model& m, bool second) {
    if (second) return m.s().s("ampBNam").isEmpty() ? juce::String() : "nam:" + m.s().s("ampBNam");
    if (m.s().s("nam").isNotEmpty()) return "nam:" + m.s().s("nam");
    if (m.s().s("capture").isNotEmpty()) return "cap:" + m.s().s("capture");
    return {};
}

std::vector<Item> items(Model& m, bool second) {
    std::vector<Item> v { { {}, second ? juce::String("Off") : juce::String::fromUTF8("Leviathan amp (Clean \xc2\xb7 Rust \xc2\xb7 Hot)"), second ? juce::String() : juce::String("Leviathan") } };
    std::vector<Item> nams;
    for (auto& f : m.namFiles) nams.push_back({ "nam:" + f, namTitle(f), "Real amps (NAM)" });
    std::sort(nams.begin(), nams.end(), [](const Item& a, const Item& b) { return a.name.compareIgnoreCase(b.name) < 0; });   // A-Z by shown name
    v.insert(v.end(), nams.begin(), nams.end());
    if (!second) for (auto& [k, c] : m.captures) v.push_back({ "cap:" + k, k, "Your captures" });
    return v;
}

juce::String badge(const juce::String& id, bool second) {
    if (id.startsWith("nam:")) return "NAM";
    if (id.startsWith("cap:")) return "CAPTURE";
    return second ? "OFF" : "BUILT-IN";
}
juce::String name(const juce::String& id, bool second) {
    if (id.startsWith("nam:")) return namTitle(id.substring(4));
    if (id.startsWith("cap:")) return id.substring(4);
    return second ? juce::String::fromUTF8("Choose a second amp\xe2\x80\xa6") : juce::String("Leviathan amp");
}
juce::String detail(Model& m, const juce::String& id, bool second) {
    if (id.startsWith("nam:")) return m.namFiles.contains(id.substring(4)) ? "Neural Amp Modeler capture" : "Missing: rescan or add it again";
    if (id.startsWith("cap:")) return "Your fitted capture";
    return second ? juce::String("Blends with Amp") : juce::String::fromUTF8("Clean \xc2\xb7 Rust \xc2\xb7 Hot");
}

void apply(Model& m, const juce::String& id, bool second) {
    if (second) {
        Settings n = m.s(); n.set("ampBNam", id.startsWith("nam:") ? id.substring(4) : juce::String());
        if (id.isNotEmpty()) n.set("ampBOn", true);
        m.setSettings(n); return;
    }
    if (id.startsWith("nam:")) { m.useNam(id.substring(4)); return; }
    m.useNam({}); m.useCapture(id.startsWith("cap:") ? id.substring(4) : juce::String());
}

/// Search, then the amps grouped by where they come from; clicking one plays it straight away.
class Browser : public juce::Component, private juce::ListBoxModel {
public:
    Browser(Model& m, bool second) : model(m), secondAmp(second) {
        search.setTextToShowWhenEmpty(juce::String::fromUTF8("Search amps: name, model, creator\xe2\x80\xa6"), theme::secondary);
        search.setFont(theme::font(10.5f)); search.setIndents(10, 0); search.setJustification(juce::Justification::centredLeft);
        search.setColour(juce::TextEditor::backgroundColourId, juce::Colours::white.withAlpha(0.06f));
        search.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.onTextChange = [this] { refresh(); };
        addAndMakeVisible(search);
        list.setModel(this); list.setRowHeight(onAndroid ? 40 : 26);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(list);
        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds;
        setSize(std::min(520, (int) area.getWidth() - 40), std::min(480, (int) area.getHeight() - 80));
        refresh();
    }
    void visibilityChanged() override { if (isShowing() && !onAndroid) search.grabKeyboardFocus(); }
    void resized() override { search.setBounds(12, 12, getWidth() - 24, 36); list.setBounds(getLocalBounds().withTrimmedTop(56).reduced(6, 4)); }
    void paint(juce::Graphics& g) override {
        g.setColour(theme::line); g.fillRect(0, 54, getWidth(), 1);
        if (rows.empty()) {
            g.setColour(theme::secondary); g.setFont(theme::font(10.5f));
            g.drawText(juce::String::fromUTF8("No amp matches \xe2\x80\x9c") + search.getText() + juce::String::fromUTF8("\xe2\x80\x9d."),
                       getLocalBounds().withTrimmedTop(80), juce::Justification::centredTop);
        }
    }
private:
    struct Row { bool header; Item item; };
    void refresh() {
        rows.clear();
        juce::StringArray words; words.addTokens(search.getText().toLowerCase(), " ", ""); words.removeEmptyStrings();
        juce::String section = "\x01";
        for (auto& it : items(model, secondAmp)) {
            bool ok = true; for (auto& w : words) ok = ok && (it.name + " " + it.section).toLowerCase().contains(w);
            if (!ok) continue;
            if (it.section != section) { section = it.section; if (section.isNotEmpty()) rows.push_back({ true, it }); }
            rows.push_back({ false, it });
        }
        list.updateContent(); repaint();
        const auto cur = currentId(model, secondAmp);
        for (size_t i = 0; i < rows.size(); ++i) if (!rows[i].header && rows[i].item.id == cur) list.scrollToEnsureRowIsOnscreen((int) i);
    }
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem(int r, juce::Graphics& g, int w, int h, bool) override {
        if (!juce::isPositiveAndBelow(r, (int) rows.size())) return;
        const auto& row = rows[(size_t) r];
        if (row.header) {
            g.setColour(theme::secondary); g.setFont(theme::font(9, true, 1));
            g.drawText(row.item.section.toUpperCase(), 10, 0, w - 20, h, juce::Justification::bottomLeft);
            return;
        }
        const bool on = row.item.id == currentId(model, secondAmp);
        if (on) { g.setColour(model.accent().withAlpha(0.15f)); g.fillRoundedRectangle(0, 1, (float) w, (float) h - 2, 6); }
        g.setColour(on ? model.accent() : theme::text); g.setFont(theme::font(10.5f, on));
        g.drawText(row.item.name, 12, 0, w - 20, h, juce::Justification::centredLeft, true);
    }
    void listBoxItemClicked(int r, const juce::MouseEvent&) override {
        if (!juce::isPositiveAndBelow(r, (int) rows.size()) || rows[(size_t) r].header) return;
        apply(model, rows[(size_t) r].item.id, secondAmp);
        list.repaint();
    }
    Model& model; bool secondAmp;
    juce::TextEditor search; juce::ListBox list;
    std::vector<Row> rows;
};
} // namespace

void step(Model& m, int d, bool second) {
    const auto v = items(m, second);
    const auto cur = currentId(m, second);
    int i = 0; for (size_t k = 0; k < v.size(); ++k) if (v[k].id == cur) i = (int) k;
    apply(m, v[(size_t) ((i + d + (int) v.size()) % (int) v.size())].id, second);
}

std::unique_ptr<juce::Component> makeButton(Model& m, bool second) {
    auto b = std::make_unique<SourceButton>([&m, second] { return badge(currentId(m, second), second); },
                                            [&m, second] { return name(currentId(m, second), second); },
                                            [&m, second] { return detail(m, currentId(m, second), second); },
                                            [&m, second](SourceButton& self) { juce::CallOutBox::launchAsynchronously(std::make_unique<Browser>(m, second), self.getScreenBounds(), nullptr); });
    b->setTooltip(second ? "Choose a second amp" : "Choose an amp");
    return b;
}

}
