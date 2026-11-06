import SwiftUI

// MARK: - Minimal Markdown, for release notes shown in the app
//
// Blocks: headings (#, ##, ###), paragraphs, bullet and numbered lists (nested by indent), tables (one row per line),
// fenced code and rules. Inline: bold, italic, code and links (Foundation's inline Markdown).
// windows/Source/UI/Markdown.cpp parses the same subset the same way.

enum MDBlock: Hashable {
    case heading(Int, String)
    case para(String)
    case item(Int, String, String)        // nesting level, marker ("•", "◦", "1."), text
    case row([String], Bool)              // cells, header row
    case code(String)
    case rule
}

enum Markdown {
    static func blocks(_ src: String) -> [MDBlock] {
        var out: [MDBlock] = [], para: [String] = [], code: [String] = [], inCode = false
        func flush() { if !para.isEmpty { out.append(.para(para.joined(separator: " "))); para = [] } }
        for raw in src.replacingOccurrences(of: "\r\n", with: "\n").components(separatedBy: "\n") {
            let line = raw.replacingOccurrences(of: "\t", with: "    ")
            let t = line.trimmingCharacters(in: .whitespaces)
            if t.hasPrefix("```") {
                if inCode { out.append(.code(code.joined(separator: "\n"))); code = [] } else { flush() }
                inCode.toggle(); continue
            }
            if inCode { code.append(line); continue }
            if t.isEmpty { flush(); continue }
            let indent = line.prefix(while: { $0 == " " }).count, level = min(indent / 2, 3)
            if t.hasPrefix("#") {
                let hashes = t.prefix(while: { $0 == "#" }).count
                if hashes <= 6, t.dropFirst(hashes).first == " " {
                    flush(); out.append(.heading(min(hashes, 3), String(t.dropFirst(hashes + 1)).trimmingCharacters(in: .whitespaces))); continue
                }
            }
            if t == "---" || t == "***" || t == "___" { flush(); out.append(.rule); continue }
            if t.hasPrefix("|") {
                flush()
                var cells = t.split(separator: "|", omittingEmptySubsequences: false).map { $0.trimmingCharacters(in: .whitespaces) }
                if cells.first == "" { cells.removeFirst() }
                if cells.last == "" { cells.removeLast() }
                // |---|:--:| marks the row above as the header
                if !cells.isEmpty, cells.allSatisfy({ !$0.isEmpty && $0.allSatisfy { "-: ".contains($0) } }) {
                    if case .row(let c, _)? = out.last { out[out.count - 1] = .row(c, true) }
                    continue
                }
                if cells.contains(where: { !$0.isEmpty }) { out.append(.row(cells, false)) }
                continue
            }
            if t.hasPrefix("- ") || t.hasPrefix("* ") || t.hasPrefix("+ ") {
                flush(); out.append(.item(level, level == 0 ? "•" : "◦", String(t.dropFirst(2)))); continue
            }
            let digits = t.prefix(while: \.isNumber)
            if !digits.isEmpty, digits.count <= 3, t.dropFirst(digits.count).hasPrefix(". ") {
                flush(); out.append(.item(level, digits + ".", String(t.dropFirst(digits.count + 2)))); continue
            }
            // indented text right after a list item continues it
            if indent >= 2, para.isEmpty, case .item(let l, let m, let s)? = out.last { out[out.count - 1] = .item(l, m, s + " " + t); continue }
            para.append(t)
        }
        flush()
        if inCode { out.append(.code(code.joined(separator: "\n"))) }
        return out
    }

    static func inline(_ s: String) -> AttributedString {
        (try? AttributedString(markdown: s, options: .init(interpretedSyntax: .inlineOnlyPreservingWhitespace))) ?? AttributedString(s)
    }
}

struct MarkdownView: View {
    let blocks: [MDBlock]
    init(_ text: String) { blocks = Markdown.blocks(text) }

    var body: some View {
        VStack(alignment: .leading, spacing: 8 * Z) {
            ForEach(Array(blocks.enumerated()), id: \.offset) { i, b in block(b, first: i == 0) }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .textSelection(.enabled)
    }

    private var body13: Font { .system(size: 13 * UI) }

    @ViewBuilder private func block(_ b: MDBlock, first: Bool) -> some View {
        switch b {
        case .heading(let level, let s):
            Text(Markdown.inline(s))
                .font(.system(size: (level == 1 ? 18 : level == 2 ? 15 : 13) * UI, weight: .semibold))
                .padding(.top, first ? 0 : (level == 1 ? 16 : 8) * Z)
                .fixedSize(horizontal: false, vertical: true)
        case .para(let s):
            Text(Markdown.inline(s)).font(body13).foregroundStyle(Color(white: 0.85)).fixedSize(horizontal: false, vertical: true)
        case .item(let level, let marker, let s):
            HStack(alignment: .firstTextBaseline, spacing: 6 * Z) {
                Text(marker).font(body13).foregroundStyle(.secondary).frame(minWidth: 12 * Z, alignment: .leading)
                Text(Markdown.inline(s)).font(body13).foregroundStyle(Color(white: 0.85)).fixedSize(horizontal: false, vertical: true)
            }
            .padding(.leading, CGFloat(level) * 18 * Z)
        case .row(let cells, let header):
            HStack(alignment: .firstTextBaseline, spacing: 12 * Z) {
                ForEach(cells.indices, id: \.self) { i in
                    Text(Markdown.inline(cells[i])).font(.system(size: 13 * UI, weight: header ? .semibold : .regular))
                        .frame(maxWidth: .infinity, alignment: .leading).fixedSize(horizontal: false, vertical: true)
                }
            }
        case .code(let s):
            Text(s).font(.system(size: 12 * UI, design: .monospaced))
                .padding(8 * Z).frame(maxWidth: .infinity, alignment: .leading)
                .background(RoundedRectangle(cornerRadius: 6).fill(panel2))
        case .rule:
            Rectangle().fill(line).frame(height: 1).padding(.vertical, 4 * Z)
        }
    }
}
