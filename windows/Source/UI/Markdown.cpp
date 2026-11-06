#include "Markdown.h"
#include "Widgets.h"

namespace lev {

std::vector<MDBlock> parseMarkdown(const juce::String& src) {
    std::vector<MDBlock> out;
    juce::StringArray para, code;
    bool inCode = false;
    auto flush = [&] {
        if (para.isEmpty()) return;
        MDBlock b; b.kind = MDBlock::para; b.text = para.joinIntoString(" "); out.push_back(b); para.clear();
    };
    for (auto raw : juce::StringArray::fromLines(src)) {
        const auto line = raw.replace("\t", "    ");
        const auto t = line.trim();
        if (t.startsWith("```")) {
            if (inCode) { MDBlock b; b.kind = MDBlock::code; b.text = code.joinIntoString("\n"); out.push_back(b); code.clear(); }
            else flush();
            inCode = !inCode; continue;
        }
        if (inCode) { code.add(line); continue; }
        if (t.isEmpty()) { flush(); continue; }
        int indent = 0; while (indent < line.length() && line[indent] == ' ') ++indent;
        const int level = std::min(indent / 2, 3);
        if (t.startsWithChar('#')) {
            int hashes = 0; while (hashes < t.length() && t[hashes] == '#') ++hashes;
            if (hashes <= 6 && t[hashes] == ' ') {
                flush(); MDBlock b; b.kind = MDBlock::heading; b.level = std::min(hashes, 3); b.text = t.substring(hashes + 1).trim(); out.push_back(b); continue;
            }
        }
        if (t == "---" || t == "***" || t == "___") { flush(); MDBlock b; b.kind = MDBlock::rule; out.push_back(b); continue; }
        if (t.startsWithChar('|')) {
            flush();
            auto cells = juce::StringArray::fromTokens(t, "|", "");
            for (auto& c : cells) c = c.trim();
            if (cells.size() > 0 && cells[0].isEmpty()) cells.remove(0);
            if (cells.size() > 0 && cells[cells.size() - 1].isEmpty()) cells.remove(cells.size() - 1);
            // |---|:--:| marks the row above as the header
            bool sep = cells.size() > 0;
            for (auto& c : cells) if (c.isEmpty() || !c.containsOnly("-: ")) sep = false;
            if (sep) { if (!out.empty() && out.back().kind == MDBlock::row) out.back().header = true; continue; }
            bool any = false; for (auto& c : cells) any |= c.isNotEmpty();
            if (any) { MDBlock b; b.kind = MDBlock::row; b.cells = cells; out.push_back(b); }
            continue;
        }
        if (t.startsWith("- ") || t.startsWith("* ") || t.startsWith("+ ")) {
            flush(); MDBlock b; b.kind = MDBlock::item; b.level = level; b.marker = juce::String::fromUTF8(level == 0 ? "\xe2\x80\xa2" : "\xe2\x97\xa6");
            b.text = t.substring(2); out.push_back(b); continue;
        }
        int digits = 0; while (digits < t.length() && juce::CharacterFunctions::isDigit(t[digits])) ++digits;
        if (digits > 0 && digits <= 3 && t.substring(digits).startsWith(". ")) {
            flush(); MDBlock b; b.kind = MDBlock::item; b.level = level; b.marker = t.substring(0, digits) + "."; b.text = t.substring(digits + 2); out.push_back(b); continue;
        }
        // indented text right after a list item continues it
        if (indent >= 2 && para.isEmpty() && !out.empty() && out.back().kind == MDBlock::item) { out.back().text << " " << t; continue; }
        para.add(t);
    }
    flush();
    if (inCode) { MDBlock b; b.kind = MDBlock::code; b.text = code.joinIntoString("\n"); out.push_back(b); }
    return out;
}

namespace {
juce::Font mono(float pt) {
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), pt * theme::UI * 1.18f, juce::Font::plain));
}

/// Inline Markdown into an AttributedString: **bold**, *italic* / _italic_, `code`, [text](url), \escapes.
void appendInline(juce::AttributedString& as, std::vector<std::pair<juce::Range<int>, juce::String>>& links, const juce::String& s,
                  float pt, bool baseBold, juce::Colour colour, juce::Colour linkColour) {
    bool bold = baseBold, italic = false;
    juce::String run;
    auto fontNow = [&] { auto f = theme::font(pt, bold); return italic ? f.italicised() : f; };
    auto flush = [&] { if (run.isNotEmpty()) { as.append(run, fontNow(), colour); run.clear(); } };
    const int n = s.length();
    for (int k = 0; k < n;) {
        const auto c = s[k];
        if (c == '\\' && k + 1 < n) { run << s[k + 1]; k += 2; continue; }
        if (c == '`') {
            const int e = s.indexOfChar(k + 1, '`');
            if (e > k) { flush(); as.append(s.substring(k + 1, e), mono(pt * 0.92f), colour); k = e + 1; continue; }
        }
        if ((c == '*' || c == '_') && k + 1 < n && s[k + 1] == c) { flush(); bold = !bold; k += 2; continue; }
        if (c == '*' || c == '_') {
            const bool inWord = k > 0 && juce::CharacterFunctions::isLetterOrDigit(s[k - 1]) && k + 1 < n && juce::CharacterFunctions::isLetterOrDigit(s[k + 1]);
            const bool opens = !italic && k + 1 < n && s[k + 1] != ' ';
            if (!inWord && (italic || opens)) { flush(); italic = !italic; ++k; continue; }
        }
        if (c == '[') {
            const int close = s.indexOf(k, "](");
            const int end = close > k ? s.indexOfChar(close + 2, ')') : -1;
            if (close > k && end > close) {
                flush();
                const int start = as.getText().length();
                auto f = fontNow(); f.setUnderline(true);
                as.append(s.substring(k + 1, close), f, linkColour);
                links.push_back({ { start, as.getText().length() }, s.substring(close + 2, end) });
                k = end + 1; continue;
            }
        }
        run << c; ++k;
    }
    flush();
}
} // namespace

int MarkdownView::layout(int width) {
    if (width == laidOutWidth) return getHeight();
    laidOutWidth = width;
    pieces.clear();
    const float W = (float) width, gap = 8, z = theme::UI / 1.5f;
    const juce::Colour body(0xffd9d9d9);
    float y = 0;
    auto addText = [&](const juce::String& md, float x, float w, float pt, bool bold, juce::Colour col) {
        Piece p; p.kind = Piece::text;
        juce::AttributedString as; as.setWordWrap(juce::AttributedString::byWord);
        appendInline(as, p.links, md, pt, bold, col, accent);
        p.layout.createLayout(as, w);
        p.box = { x, y, w, p.layout.getHeight() };
        const float h = p.layout.getHeight();
        pieces.push_back(std::move(p));
        return h;
    };
    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& b = blocks[i];
        if (i > 0) y += gap * z;
        switch (b.kind) {
        case MDBlock::heading: {
            if (i > 0) y += (b.level == 1 ? 16.f : 8.f) * z;
            y += addText(b.text, 0, W, b.level == 1 ? 18.f : b.level == 2 ? 15.f : 13.f, true, theme::text);
            break;
        }
        case MDBlock::para: y += addText(b.text, 0, W, 13, false, body); break;
        case MDBlock::item: {
            const float indent = (float) b.level * 18 * z * 1.5f, markW = 18 * z * 1.5f;
            const float top = y;
            addText(b.marker, indent, markW, 13, false, theme::secondary);
            y = top;
            y += addText(b.text, indent + markW, W - indent - markW, 13, false, body);
            break;
        }
        case MDBlock::row: {
            const int n = std::max(1, b.cells.size());
            const float colGap = 12 * z, cw = (W - colGap * (float) (n - 1)) / (float) n;
            const float top = y; float h = 0;
            for (int c = 0; c < b.cells.size(); ++c) { y = top; h = std::max(h, addText(b.cells[c], (cw + colGap) * (float) c, cw, 13, b.header, b.header ? theme::text : body)); }
            y = top + h;
            break;
        }
        case MDBlock::code: {
            const float pad = 8 * z;
            Piece bg; bg.kind = Piece::fill; bg.colour = theme::panel2;
            const size_t bgIndex = pieces.size(); pieces.push_back(std::move(bg));
            Piece p; p.kind = Piece::text;
            juce::AttributedString as; as.setWordWrap(juce::AttributedString::byChar);
            as.append(b.text, mono(12), body);
            p.layout.createLayout(as, W - 2 * pad);
            p.box = { pad, y + pad, W - 2 * pad, p.layout.getHeight() };
            const float h = p.layout.getHeight() + 2 * pad;
            pieces[bgIndex].box = { 0, y, W, h };
            pieces.push_back(std::move(p));
            y += h;
            break;
        }
        case MDBlock::rule: {
            Piece p; p.kind = Piece::line; p.colour = theme::line; p.box = { 0, y + 4 * z, W, 1 }; pieces.push_back(std::move(p));
            y += 9 * z;
            break;
        }
        }
    }
    const int h = (int) std::ceil(y);
    setSize(width, h);
    return h;
}

void MarkdownView::paint(juce::Graphics& g) {
    for (auto& p : pieces) {
        switch (p.kind) {
        case Piece::fill: g.setColour(p.colour); g.fillRoundedRectangle(p.box, 6); break;
        case Piece::line: g.setColour(p.colour); g.fillRect(p.box); break;
        case Piece::text: p.layout.draw(g, p.box); break;
        }
    }
}

juce::String MarkdownView::linkAt(juce::Point<float> pt) const {
    for (auto& p : pieces) {
        if (p.kind != Piece::text || p.links.empty() || !p.box.contains(pt)) continue;
        const auto local = pt - p.box.getPosition();
        for (int i = 0; i < p.layout.getNumLines(); ++i) {
            const auto& line = p.layout.getLine(i);
            if (local.y < line.lineOrigin.y - line.ascent || local.y > line.lineOrigin.y + line.descent) continue;
            for (auto* run : line.runs) {
                if (!(run->getRunBoundsX() + line.lineOrigin.x).contains(local.x)) continue;
                for (auto& [range, url] : p.links) if (range.contains(run->stringRange.getStart())) return url;
            }
        }
    }
    return {};
}

void MarkdownView::mouseMove(const juce::MouseEvent& e) {
    setMouseCursor(linkAt(e.position).isNotEmpty() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void MarkdownView::mouseUp(const juce::MouseEvent& e) {
    const auto url = linkAt(e.position);
    if (url.startsWith("https://") || url.startsWith("http://")) juce::URL(url).launchInDefaultBrowser();
}

} // namespace lev
