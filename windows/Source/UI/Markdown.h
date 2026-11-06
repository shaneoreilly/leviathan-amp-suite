// Minimal Markdown for release notes shown in the app (port of mac/Sources/Markdown.swift).
// Blocks: headings (#, ##, ###), paragraphs, bullet and numbered lists (nested by indent), tables (one row per line),
// fenced code and rules. Inline: bold, italic, code and links.
#pragma once
#include <JuceHeader.h>

namespace lev {

struct MDBlock {
    enum Kind { heading, para, item, row, code, rule } kind = para;
    int level = 0;              // heading level, or list nesting
    juce::String text, marker;  // marker: "•", "◦", "1."
    juce::StringArray cells;    // table row
    bool header = false;
};

std::vector<MDBlock> parseMarkdown(const juce::String& src);

class MarkdownView : public juce::Component {
public:
    void setMarkdown(const juce::String& md) { blocks = parseMarkdown(md); laidOutWidth = -1; }
    /// Lays the blocks out for a width and returns the height they need.
    int layout(int width);
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    juce::Colour accent = juce::Colours::red;

private:
    struct Piece {
        enum Kind { text, fill, line } kind = text;
        juce::Rectangle<float> box;
        juce::TextLayout layout;
        std::vector<std::pair<juce::Range<int>, juce::String>> links;   // character range -> URL
        juce::Colour colour;
    };
    std::vector<MDBlock> blocks;
    std::vector<Piece> pieces;
    int laidOutWidth = -1;
    juce::String linkAt(juce::Point<float> p) const;
};

} // namespace lev
