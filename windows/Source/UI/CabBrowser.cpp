#include "CabBrowser.h"

namespace lev::cab {

namespace {
const juce::String dotSep = juce::String::fromUTF8(" \xc2\xb7 ");

juce::StringArray stored(Model& m, const char* key) {
    juce::StringArray a; a.addLines(m.props().getValue(key)); a.removeEmptyStrings(); return a;
}
void store(Model& m, const char* key, const juce::StringArray& a) { m.props().setValue(key, a.joinIntoString("\n")); }
bool isFav(Model& m, const juce::String& id) { return stored(m, "cabFavs").contains(id); }
void toggleFav(Model& m, const juce::String& id) {
    auto a = stored(m, "cabFavs");
    if (a.contains(id)) a.removeString(id); else a.add(id);
    store(m, "cabFavs", a);
}
void noteRecent(Model& m, const juce::String& id) {
    auto a = stored(m, "cabRecent");
    a.removeString(id); a.insert(0, id);
    while (a.size() > 12) a.remove(a.size() - 1);
    store(m, "cabRecent", a);
}
juce::String folderOf(const juce::String& rel) { return rel.containsChar('/') ? rel.upToLastOccurrenceOf("/", false, false) : juce::String(); }

struct Item { juce::String id, name, meta; };
std::vector<Item> builtins(Model& m) {
    std::vector<Item> v;
    for (int i = 0; i < kCabCount; ++i) v.push_back({ "b:" + juce::String(i), CABS[i].name, "Built-in" });
    if (m.userIRName.isNotEmpty()) v.push_back({ "u", m.userIRName, "Single file" });
    return v;
}
std::vector<Item> library(Model& m, const juce::String& folder, bool withMeta) {
    std::vector<Item> v;
    for (const auto& rel : m.cabIRs.all())
        if (folder.isEmpty() || folderOf(rel) == folder) v.push_back({ "l:" + rel, CabIRLibrary::title(rel), withMeta ? folderOf(rel) : juce::String() });
    return v;
}
Item itemFor(Model& m, const juce::String& id) {
    const auto src = source(id);
    return { id, name(m, id), src.substring(0, 1) + src.substring(1).toLowerCase() };
}
} // namespace

juce::String currentId(Model& m) {
    const auto& s = m.s();
    const int k = s.i("cabModel");
    if (k == CAB_IRLIB) return "l:" + s.s("irFile");
    if (k == CAB_LIB) return "g:" + s.s("irA");
    if (k == CAB_USER) return "u";
    return "b:" + juce::String(k);
}

juce::String name(Model& m, const juce::String& id) {
    const auto v = id.substring(2);
    if (id == "u") return m.userIRName.isNotEmpty() ? m.userIRName : juce::String("IR file");
    if (id.startsWith("b:")) { const int i = v.getIntValue(); return juce::isPositiveAndBelow(i, kCabCount) ? juce::String(CABS[i].name) : juce::String("Cabinet"); }
    if (id.startsWith("l:")) return v.isEmpty() ? juce::String::fromUTF8("Choose an IR\xe2\x80\xa6") : CabIRLibrary::title(v);
    if (id.startsWith("g:")) { if (auto* f = m.irLib.file(v)) return f->mic + dotSep + f->position; return "God's Cab"; }
    return "Cabinet";
}

juce::String source(const juce::String& id) {
    if (id == "u") return "FILE";
    if (id.startsWith("b:")) return "BUILT-IN";
    if (id.startsWith("l:")) return "LIBRARY";
    if (id.startsWith("g:")) return "GOD'S CAB";
    return {};
}

juce::String detail(Model& m, const juce::String& id) {
    const auto v = id.substring(2);
    if (id.startsWith("l:")) { const auto f = folderOf(v); return f.isEmpty() ? juce::String("IR library") : f; }
    if (id.startsWith("g:"))
        if (auto* f = m.irLib.file(v)) {
            auto t = "Mesa 4x12" + dotSep + "Presence " + juce::String(f->pres) + (f->ts ? dotSep + "TS" : juce::String());
            if (m.s().i("cabModel") == CAB_LIB) if (auto* b = m.irLib.file(m.s().s("irB"))) t << " + " << b->mic;
            return t;
        }
    if (id == "u") return "Single IR file";
    return "Leviathan cab model";
}

void apply(Model& m, const juce::String& id) {
    Settings x = m.s();
    const auto v = id.substring(2);
    if (id == "u") x.set("cabModel", CAB_USER);
    else if (id.startsWith("b:")) x.set("cabModel", v.getIntValue());
    else if (id.startsWith("l:")) { x.set("cabModel", CAB_IRLIB); x.set("irFile", v); }
    else if (id.startsWith("g:")) { x.set("cabModel", CAB_LIB); x.set("irA", v); }
    x.set("cabOn", true);
    m.setSettings(x);
    noteRecent(m, id);
}

void step(Model& m, int d) {
    const int k = m.s().i("cabModel");
    if (k == CAB_IRLIB) { if (!m.cabIRs.isEmpty()) apply(m, "l:" + m.cabIRs.step(m.s().s("irFile"), d)); }
    else if (k == CAB_LIB) { if (!m.irLib.isEmpty()) apply(m, "g:" + m.irLib.step(m.s().s("irA"), d)); }
    else if (k != CAB_USER) apply(m, "b:" + juce::String((k + d + kCabCount) % kCabCount));
}

namespace {

/// God's Cab: the IR built from its mic and position, as rows of choices; an optional second mic blends in.
class GodsChips : public juce::Component {
public:
    explicit GodsChips(Model& m) : model(m) {}
    struct Chip { juce::Rectangle<int> r; juce::String label; bool on; std::function<void()> pick; };
    std::vector<std::pair<int, juce::String>> titles;
    std::vector<Chip> chips;
    void rebuild(int width) {
        titles.clear(); chips.clear();
        auto& lib = model.irLib;
        const IRFile* a = lib.file(model.s().s("irA"));
        int y = 10;
        auto group = [&](const juce::String& title, const juce::StringArray& opts, const juce::String& cur, std::function<void(const juce::String&)> choose) {
            titles.push_back({ y, title }); y += 22;
            int x = 0;
            const int h = onAndroid ? 40 : 30;
            for (auto& o : opts) {
                const int w = std::max(70, juce::GlyphArrangement::getStringWidthInt(theme::font(10.5f), o) + 28);
                if (x > 0 && x + w > width) { x = 0; y += h + 6; }
                chips.push_back({ { x, y, w, h }, o, o == cur, [choose, o] { choose(o); } });
                x += w + 6;
            }
            y += h + 16;
        };
        if (a == nullptr) { setSize(width, 60); return; }
        const IRFile A = *a;
        auto pickA = [this](const juce::String& mic, bool ts, const juce::String& pos, int pres) { set("irA", mic, ts, pos, pres); };
        group("MICROPHONE", lib.mics(), A.mic, [=](const juce::String& o) { pickA(o, A.ts, A.position, A.pres); });
        group("POSITION", lib.positions(A.mic, A.ts), A.position, [=](const juce::String& o) { pickA(A.mic, A.ts, o, A.pres); });
        group("PRESENCE", juce::StringArray { "1", "2", "3", "4", "5" }, juce::String(A.pres), [=](const juce::String& o) { pickA(A.mic, A.ts, A.position, o.getIntValue()); });
        if (lib.tsAvailable(A.mic))
            group("BOOST IN FRONT", juce::StringArray { "None", "Tube Screamer" }, A.ts ? "Tube Screamer" : "None",
                  [=](const juce::String& o) { pickA(A.mic, o != "None", A.position, A.pres); });
        const IRFile* b = lib.file(model.s().s("irB"));
        juce::StringArray second { "Off" }; second.addArray(lib.mics());
        const auto B = b ? std::optional<IRFile>(*b) : std::nullopt;
        group("SECOND MIC", second, B ? B->mic : juce::String("Off"), [this, A, B](const juce::String& o) {
            if (o == "Off") { model.set("irB", juce::String()); return; }
            set("irB", o, B ? B->ts : A.ts, B ? B->position : A.position, B ? B->pres : A.pres);
        });
        if (B) group("SECOND MIC POSITION", lib.positions(B->mic, B->ts), B->position, [this, B](const juce::String& o) { set("irB", B->mic, B->ts, o, B->pres); });
        setSize(width, y);
    }
    void set(const juce::String& key, const juce::String& mic, bool ts, const juce::String& pos, int pres) {
        if (auto* f = model.irLib.find(mic, ts, pos, pres)) {
            Settings x = model.s(); x.set(key, f->rel); x.set("cabModel", CAB_LIB); x.set("cabOn", true); model.setSettings(x);
            noteRecent(model, "g:" + x.s("irA"));
        }
        rebuild(getWidth()); repaint();
    }
    void paint(juce::Graphics& g) override {
        g.setFont(theme::font(9, true, 1)); g.setColour(theme::secondary);
        for (auto& [y, t] : titles) g.drawText(t, 0, y, getWidth(), 18, juce::Justification::centredLeft);
        g.setFont(theme::font(10.5f));
        for (auto& c : chips) {
            const auto r = c.r.toFloat().reduced(0.5f);
            g.setColour(c.on ? model.accent().withAlpha(0.22f) : theme::tint.withAlpha(0.05f)); g.fillRoundedRectangle(r, r.getHeight() / 2);
            g.setColour(c.on ? model.accent() : theme::tint.withAlpha(0.12f)); g.drawRoundedRectangle(r, r.getHeight() / 2, 1);
            g.setColour(c.on ? theme::text : theme::secondary); g.drawText(c.label, c.r, juce::Justification::centred);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override {
        for (auto& c : chips) if (c.r.contains(e.getPosition())) { auto f = c.pick; f(); return; }
    }
    Model& model;
};

class Browser : public juce::Component {
public:
    explicit Browser(Model& m) : model(m), gods(m) {
        tab = m.props().getValue("cabTab", "library");
        search.setTextToShowWhenEmpty(juce::String::fromUTF8("Search every cab: name, band, folder\xe2\x80\xa6"), theme::secondary);
        search.setFont(theme::font(10.5f)); search.setIndents(30, 0); search.setJustification(juce::Justification::centredLeft);
        search.setColour(juce::TextEditor::backgroundColourId, theme::tint.withAlpha(0.06f));
        search.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.onTextChange = [this] { refresh(); };
        addAndMakeVisible(search);
        list.setModel(&itemRows); folders.setModel(&folderRows);
        for (auto* l : { &list, &folders }) {
            l->setRowHeight(onAndroid ? 40 : 26);
            l->setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
            addAndMakeVisible(*l);
        }
        godsView.setViewedComponent(&gods, false); godsView.setScrollBarsShown(true, false);
        addAndMakeVisible(godsView);
        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds;
        setSize(std::min(720, (int) area.getWidth() - 40), std::min(onAndroid ? 600 : 520, (int) area.getHeight() - 80));
        refresh();
    }
    void visibilityChanged() override { if (isShowing() && !onAndroid) search.grabKeyboardFocus(); }

    struct Tab { juce::String id, label; int count; };
    std::vector<Tab> tabs() {
        std::vector<Tab> t { { "builtin", "Built-in", (int) builtins(model).size() } };
        if (!model.irLib.isEmpty()) t.push_back({ "gods", "God's Cab", model.irLib.size() });
        t.push_back({ "library", "Library", model.cabIRs.all().size() });
        t.push_back({ "favs", "Favourites", stored(model, "cabFavs").size() });
        t.push_back({ "recent", "Recent", stored(model, "cabRecent").size() });
        return t;
    }
    juce::String activeTab() { for (auto& t : tabs()) if (t.id == tab) return tab; return "builtin"; }
    bool searching() const { return search.getText().trim().isNotEmpty(); }
    bool showFolders() { return !searching() && activeTab() == "library" && folderNames.size() > 1; }

    void refresh() {
        folderNames.clear();
        for (const auto& rel : model.cabIRs.all()) folderNames.addIfNotAlreadyThere(folderOf(rel));
        folderNames.sort(true);
        if (folderNames.size() > 1) folderNames.insert(0, "\x01all");   // "All" row
        items.clear();
        const auto t = activeTab();
        if (searching()) {
            juce::StringArray words; words.addTokens(search.getText().toLowerCase(), " ", ""); words.removeEmptyStrings();
            auto all = builtins(model); for (auto& i : library(model, {}, true)) all.push_back(i);
            for (auto& i : all) {
                const auto hay = (i.name + " " + i.meta).toLowerCase();
                bool ok = true; for (auto& w : words) ok = ok && hay.contains(w);
                if (ok) items.push_back(i);
            }
        } else if (t == "builtin") items = builtins(model);
        else if (t == "library") items = library(model, folder, false);
        else if (t == "favs") for (auto& id : stored(model, "cabFavs")) items.push_back(itemFor(model, id));
        else if (t == "recent") for (auto& id : stored(model, "cabRecent")) items.push_back(itemFor(model, id));
        const bool g = !searching() && t == "gods";
        godsView.setVisible(g); list.setVisible(!g); folders.setVisible(showFolders());
        if (g) gods.rebuild(std::max(200, godsView.getWidth() - 40));
        list.updateContent(); folders.updateContent(); repaint(); resized();
        const auto cur = currentId(model);
        for (size_t i = 0; i < items.size(); ++i) if (items[i].id == cur) list.scrollToEnsureRowIsOnscreen((int) i);
    }

    juce::Rectangle<int> tabStrip() const { return { 12, 58, getWidth() - 24, 34 }; }
    juce::Rectangle<int> body() const { return getLocalBounds().withTrimmedTop(searching() ? 60 : 96).withTrimmedBottom(30); }
    void resized() override {
        search.setBounds(12, 12, getWidth() - 24, 36);
        auto b = body();
        if (showFolders()) { folders.setBounds(b.removeFromLeft(190).reduced(8, 4)); b.removeFromLeft(1); }
        list.setBounds(b.reduced(6, 4));
        godsView.setBounds(b.reduced(16, 6));
        if (godsView.isVisible()) gods.rebuild(std::max(200, godsView.getWidth() - 14));
    }
    void paint(juce::Graphics& g) override {
        {   // magnifier
            const auto q = search.getBounds().toFloat(); const float cx = q.getX() + 15, cy = q.getCentreY() - 1;
            juce::Path p; p.addEllipse(cx - 5, cy - 5, 10, 10); p.startNewSubPath(cx + 3.5f, cy + 3.5f); p.lineTo(cx + 7, cy + 7);
            g.setColour(theme::secondary); g.strokePath(p, juce::PathStrokeType(1.5f));
        }
        tabRects.clear();
        if (!searching()) {
            int x = tabStrip().getX();
            const auto cur = activeTab();
            for (auto& t : tabs()) {
                const auto label = t.label, count = juce::String(t.count);
                const int w = juce::GlyphArrangement::getStringWidthInt(theme::font(10.5f, true), label) + juce::GlyphArrangement::getStringWidthInt(theme::font(9), count) + 26;
                const juce::Rectangle<int> r(x, tabStrip().getY(), w, tabStrip().getHeight());
                const bool on = t.id == cur;
                g.setColour(on ? theme::text : theme::secondary); g.setFont(theme::font(10.5f, true));
                g.drawText(label, r.withTrimmedLeft(8), juce::Justification::centredLeft);
                g.setColour(theme::secondary); g.setFont(theme::font(9));
                g.drawText(count, r.withTrimmedRight(8), juce::Justification::centredRight);
                if (on) { g.setColour(model.accent()); g.fillRect(r.getX(), r.getBottom() - 2, r.getWidth(), 2); }
                tabRects.push_back({ r, t.id }); x += w + 4;
            }
            g.setColour(theme::line); g.fillRect(0, tabStrip().getBottom(), getWidth(), 1);
        }
        if (showFolders()) { g.setColour(theme::line); g.fillRect(folders.getRight() + 8, body().getY(), 1, body().getHeight()); }
        if (list.isVisible() && items.empty()) {
            g.setColour(theme::secondary); g.setFont(theme::font(10.5f));
            const auto t = activeTab();
            g.drawText(searching() ? juce::String::fromUTF8("No cab matches \xe2\x80\x9c") + search.getText() + juce::String::fromUTF8("\xe2\x80\x9d.")
                       : t == "favs" ? juce::String::fromUTF8("Nothing here yet. Tap \xe2\x98\x86 on any cab to keep it here.")
                       : t == "recent" ? juce::String("Cabs you pick show up here.")
                       : juce::String::fromUTF8("No IRs in the library yet. Add some with \xe2\x80\xa6 on the panel."),
                       body().reduced(20), juce::Justification::centredTop);
        }
        g.setColour(theme::line); g.fillRect(0, getHeight() - 30, getWidth(), 1);
        g.setColour(theme::secondary); g.setFont(theme::font(9));
        g.drawText(onAndroid ? juce::String::fromUTF8("Tap to hear it \xc2\xb7 \xe2\x98\x85 adds to Favourites")
                             : juce::String::fromUTF8("Click to hear it \xc2\xb7 \xe2\x98\x85 adds to Favourites \xc2\xb7 Add IRs: \xe2\x80\xa6 on the panel"),
                   12, getHeight() - 30, getWidth() - 24, 30, juce::Justification::centredLeft);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        for (auto& [r, id] : tabRects) if (r.contains(e.getPosition())) { tab = id; model.props().setValue("cabTab", id); refresh(); return; }
    }

private:
    /// The cab list: ☆ / ★ at the left (toggles Favourites), the name, the folder or source at the right.
    struct ItemRows : juce::ListBoxModel {
        Browser& b; explicit ItemRows(Browser& o) : b(o) {}
        int getNumRows() override { return (int) b.items.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool) override {
            if (!juce::isPositiveAndBelow(row, (int) b.items.size())) return;
            const auto& it = b.items[(size_t) row];
            const bool on = it.id == currentId(b.model), fav = isFav(b.model, it.id);
            if (on) { g.setColour(b.model.accent().withAlpha(0.15f)); g.fillRoundedRectangle(0, 1, (float) w, (float) h - 2, 6); }
            g.setColour(fav ? juce::Colour(0xffffb020) : theme::secondary.withAlpha(0.6f));
            const auto star = IconButton::pathFor(fav ? IconButton::starFilled : IconButton::star, juce::Rectangle<float>(8, (float) h / 2 - 7, 14, 14));
            if (fav) g.fillPath(star); else g.strokePath(star, juce::PathStrokeType(1.3f));
            const int metaW = it.meta.isEmpty() ? 0 : std::min(200, w / 3);
            g.setColour(on ? b.model.accent() : theme::text); g.setFont(theme::font(10.5f, on));
            g.drawText(it.name, 32, 0, w - 40 - metaW, h, juce::Justification::centredLeft, true);
            if (metaW > 0) { g.setColour(theme::secondary); g.setFont(theme::font(9)); g.drawText(it.meta, w - metaW - 8, 0, metaW, h, juce::Justification::centredRight, true); }
        }
        void listBoxItemClicked(int row, const juce::MouseEvent& e) override {
            if (!juce::isPositiveAndBelow(row, (int) b.items.size())) return;
            const auto id = b.items[(size_t) row].id;
            if (e.x < 30) toggleFav(b.model, id); else apply(b.model, id);
            b.list.repaint(); b.repaint();
        }
    };
    struct FolderRows : juce::ListBoxModel {
        Browser& b; explicit FolderRows(Browser& o) : b(o) {}
        int getNumRows() override { return b.folderNames.size(); }
        juce::String key(int row) const { const auto f = b.folderNames[row]; return f == "\x01all" ? juce::String() : f; }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool) override {
            if (!juce::isPositiveAndBelow(row, b.folderNames.size())) return;
            const auto f = key(row);
            if (f == b.folder) { g.setColour(theme::tint.withAlpha(0.08f)); g.fillRoundedRectangle(0, 1, (float) w, (float) h - 2, 6); }
            int n = 0; for (const auto& rel : b.model.cabIRs.all()) if (f.isEmpty() || folderOf(rel) == f) ++n;
            g.setColour(theme::text); g.setFont(theme::font(10.5f));
            g.drawText(b.folderNames[row] == "\x01all" ? juce::String("All") : f.isEmpty() ? juce::String("(top level)") : f, 8, 0, w - 50, h, juce::Justification::centredLeft, true);
            g.setColour(theme::secondary); g.setFont(theme::font(9)); g.drawText(juce::String(n), w - 44, 0, 36, h, juce::Justification::centredRight);
        }
        void listBoxItemClicked(int row, const juce::MouseEvent&) override { if (juce::isPositiveAndBelow(row, b.folderNames.size())) { b.folder = key(row); b.refresh(); } }
    };

    Model& model;
    juce::TextEditor search;
    juce::ListBox list, folders;
    juce::Viewport godsView;
    GodsChips gods;
    juce::String tab, folder;
    juce::StringArray folderNames;
    std::vector<Item> items;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> tabRects;
    ItemRows itemRows { *this };
    FolderRows folderRows { *this };
};

} // namespace

std::unique_ptr<juce::Component> makeButton(Model& m) {
    auto b = std::make_unique<SourceButton>([&m] { return source(currentId(m)); }, [&m] { return name(m, currentId(m)); },
                                            [&m] { return detail(m, currentId(m)); },
                                            [&m](SourceButton& self) { juce::CallOutBox::launchAsynchronously(std::make_unique<Browser>(m), self.getScreenBounds(), nullptr); });
    b->key = [&m] { return currentId(m) + "|" + m.s().s("irB") + "|" + juce::String(m.s().i("cabModel")); };
    b->setTooltip("Choose a cabinet");
    return b;
}

} // namespace lev::cab
