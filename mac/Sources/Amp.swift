import SwiftUI

// The amp pickers (AMP and AMP B panels), laid out like the cabinet's: ‹ [current amp] › ⋯. The button opens a browser
// with search; ‹ › step through the list without opening it. Windows: UI/AmpBrowser.cpp does the same.
//
// An amp is one id: "" the built-in Leviathan amp, "nam:<file>" a NAM capture, "cap:<name>" a fitted capture.
// Amp B lists "" (off) and NAM captures.

/// A NAM file's display name: no ESR suffix, underscores as spaces, the creator's "Tim R" moved to the end.
func namDisplayTitle(_ file: String) -> String {
    var n = file.replacingOccurrences(of: #"[-_ ]*ESR[-_ ]?[0-9.,]+"#, with: "", options: .regularExpression)
    n = n.replacingOccurrences(of: "_", with: " ")
    let words = n.split(separator: " ", omittingEmptySubsequences: true)
    // community files start with the creator's first name and initial ("Tim R ...")
    if words.count > 2, words[1].count == 1, words[1].first?.isUppercase == true {
        return words.dropFirst(2).joined(separator: " ") + "  (" + words[0] + " " + words[1] + ")"
    }
    return words.joined(separator: " ")
}

/// The height of the row above the knobs in AMP, AMP B, CABINET and DELAY (and the gap REVERB leaves for it), so the
/// first row of knobs lines up across the rig row (Windows: pickerRowH).
var PICKER_ROW_H: CGFloat { 44 * Z }

/// ‹ [badge · name / detail ▾] › — the row shared by the cabinet and amp pickers.
struct SourceButtonRow<Popover: View, MenuItems: View>: View {
    let badge: String, name: String, detail: String, help: String
    var width: CGFloat = 210   // button width (times W); the amp panels use a narrower one
    var showMenu = true
    @Binding var open: Bool
    let prev: () -> Void, next: () -> Void
    @ViewBuilder let popover: () -> Popover
    @ViewBuilder let menu: () -> MenuItems

    var body: some View {
        HStack(spacing: 4) {
            Button(action: prev) { Image(systemName: "chevron.left").frame(width: 18 * Z, height: 30 * Z).contentShape(Rectangle()) }
                .buttonStyle(.borderless).help("Previous")
            Button { open.toggle() } label: {
                HStack(spacing: 8) {
                    Text(badge).font(.inter(size: T(9) * UI, weight: .bold)).foregroundStyle(.secondary)
                        .padding(.horizontal, 5).padding(.vertical, 2).background(RoundedRectangle(cornerRadius: 4).fill(PAL.tint.opacity(0.08)))
                    VStack(alignment: .leading, spacing: 1) {
                        Text(name).font(.inter(size: T(10.5) * UI, weight: .semibold)).lineLimit(1)
                        Text(detail).font(.inter(size: T(9) * UI)).foregroundStyle(.secondary).lineLimit(1)
                    }
                    Spacer(minLength: 4)
                    Image(systemName: "chevron.down").foregroundStyle(.secondary)
                }
                .padding(.horizontal, 8).padding(.vertical, 5)
                .frame(width: width * W)
                .background(RoundedRectangle(cornerRadius: 7).fill(PAL.tint.opacity(0.06)))
                .overlay(RoundedRectangle(cornerRadius: 7).stroke(open ? Color.accentColor : PAL.tint.opacity(0.1)))
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .popover(isPresented: $open, arrowEdge: .bottom, content: popover)
            .help(help)
            Button(action: next) { Image(systemName: "chevron.right").frame(width: 18 * Z, height: 30 * Z).contentShape(Rectangle()) }
                .buttonStyle(.borderless).help("Next")
            if showMenu {
                Menu(content: menu) { Image(systemName: "ellipsis.circle") }
                    .menuStyle(.borderlessButton).menuIndicator(.hidden).fixedSize()
            }
        }
        .frame(height: PICKER_ROW_H)
    }
}

struct AmpItem: Hashable { let id, name, section: String }

extension Model {
    var ampID: String { !s.nam.isEmpty ? "nam:" + s.nam : !s.capture.isEmpty ? "cap:" + s.capture : "" }
    func ampItems(secondAmp: Bool) -> [AmpItem] {
        var v = [AmpItem(id: "", name: secondAmp ? "Off" : "Leviathan amp (Clean · Rust · Hot)", section: secondAmp ? "" : "Leviathan")]
        v += namHost.files.map { AmpItem(id: "nam:" + $0, name: namDisplayTitle($0), section: "Real amps (NAM)") }   // A-Z by shown name
            .sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
        if !secondAmp { v += captures.keys.sorted().map { AmpItem(id: "cap:" + $0, name: $0, section: "Your captures") } }
        return v
    }
    func ampBadge(_ id: String) -> String { id.hasPrefix("nam:") ? "NAM" : id.hasPrefix("cap:") ? "CAPTURE" : "BUILT-IN" }
    func ampName(_ id: String, secondAmp: Bool = false) -> String {
        if id.hasPrefix("nam:") { return namDisplayTitle(String(id.dropFirst(4))) }
        if id.hasPrefix("cap:") { return String(id.dropFirst(4)) }
        return secondAmp ? "Choose a second amp…" : "Leviathan amp"
    }
    func ampDetail(_ id: String, secondAmp: Bool = false) -> String {
        if id.hasPrefix("nam:") { return namHost.files.contains(String(id.dropFirst(4))) ? "Neural Amp Modeler capture" : "Missing: rescan or add it again" }
        if id.hasPrefix("cap:") { return "Your fitted capture" }
        return secondAmp ? "Blends with Amp" : "Clean · Rust · Hot"
    }
    func applyAmp(_ id: String) {
        if id.hasPrefix("nam:") { useNam(String(id.dropFirst(4))) }
        else { var n = s; n.nam = ""; s = n; useCapture(id.hasPrefix("cap:") ? String(id.dropFirst(4)) : "") }
    }
    func applyAmpB(_ id: String) {
        var n = s; n.ampBNam = id.hasPrefix("nam:") ? String(id.dropFirst(4)) : ""
        if !n.ampBNam.isEmpty { n.ampBOn = true }
        s = n
    }
    func stepAmp(_ d: Int, secondAmp: Bool) {
        let items = ampItems(secondAmp: secondAmp)
        let cur = secondAmp ? (s.ampBNam.isEmpty ? "" : "nam:" + s.ampBNam) : ampID
        let i = items.firstIndex { $0.id == cur } ?? 0
        let next = items[(i + d + items.count) % items.count].id
        secondAmp ? applyAmpB(next) : applyAmp(next)
    }
}

final class AmpUI: ObservableObject { @Published var open = false; @Published var query = "" }

struct AmpPicker: View {
    @EnvironmentObject var m: Model
    let secondAmp: Bool
    @StateObject private var ui = AmpUI()
    var body: some View {
        let id = secondAmp ? (m.s.ampBNam.isEmpty ? "" : "nam:" + m.s.ampBNam) : m.ampID
        SourceButtonRow(badge: secondAmp && id.isEmpty ? "OFF" : m.ampBadge(id), name: m.ampName(id, secondAmp: secondAmp),
                        detail: m.ampDetail(id, secondAmp: secondAmp), help: secondAmp ? "Choose a second amp" : "Choose an amp",
                        width: 150, showMenu: false,   // narrow panels; adding captures is in the browser
                        open: $ui.open, prev: { m.stepAmp(-1, secondAmp: secondAmp) }, next: { m.stepAmp(1, secondAmp: secondAmp) },
                        popover: { AmpBrowser(secondAmp: secondAmp, ui: ui).environmentObject(m) },
                        menu: {
                            Button("Add NAM captures (.nam)…") { m.importNam() }
                            Button("Open NAM folder") { NSWorkspace.shared.open(NamHost.dir) }
                            Button("Rescan") { m.reloadCaptures(); m.rescanNam() }
                        })
    }
}

struct AmpBrowser: View {
    @EnvironmentObject var m: Model
    let secondAmp: Bool
    @ObservedObject var ui: AmpUI
    var body: some View {
        let cur = secondAmp ? (m.s.ampBNam.isEmpty ? "" : "nam:" + m.s.ampBNam) : m.ampID
        let words = ui.query.lowercased().split(separator: " ").map(String.init)
        let items = m.ampItems(secondAmp: secondAmp).filter { a in words.allSatisfy { "\(a.name) \(a.section)".lowercased().contains($0) } }
        let sections = items.reduce(into: [String]()) { if !$0.contains($1.section) { $0.append($1.section) } }
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                TextField("Search amps: name, model, creator…", text: $ui.query).textFieldStyle(.plain)
            }
            .padding(.horizontal, 10).padding(.vertical, 8)
            .background(RoundedRectangle(cornerRadius: 8).fill(PAL.tint.opacity(0.06)))
            .padding(12)
            Divider()
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 1, pinnedViews: [.sectionHeaders]) {
                        ForEach(sections, id: \.self) { sec in
                            Section {
                                ForEach(items.filter { $0.section == sec }, id: \.self) { a in
                                    let on = a.id == cur
                                    Button { secondAmp ? m.applyAmpB(a.id) : m.applyAmp(a.id) } label: {
                                        Text(a.name).fontWeight(on ? .semibold : .regular).foregroundStyle(on ? Color.accentColor : .primary)
                                            .lineLimit(1).frame(maxWidth: .infinity, alignment: .leading)
                                            .padding(.horizontal, 12).padding(.vertical, 6)
                                            .background(RoundedRectangle(cornerRadius: 6).fill(on ? Color.accentColor.opacity(0.12) : .clear))
                                            .contentShape(Rectangle())
                                    }.buttonStyle(.plain).id(a.id)
                                }
                            } header: {
                                if !sec.isEmpty {
                                    Text(sec.uppercased()).font(.inter(size: T(9) * UI, weight: .semibold)).tracking(1).foregroundStyle(.secondary)
                                        .frame(maxWidth: .infinity, alignment: .leading).padding(.horizontal, 12).padding(.vertical, 6)
                                        .background(Color(nsColor: .windowBackgroundColor))
                                }
                            }
                        }
                        if items.isEmpty { Text("No amp matches “\(ui.query)”.").foregroundStyle(.secondary).frame(maxWidth: .infinity).padding(30) }
                    }.padding(6)
                }
                .onAppear { proxy.scrollTo(cur, anchor: .center) }
            }
            Divider()
            HStack {
                Text("Click to hear it").foregroundStyle(.secondary)
                Spacer()
                Button("Add NAM captures…") { m.importNam() }
                Button("Open NAM folder") { NSWorkspace.shared.open(NamHost.dir) }
                Button("Rescan") { m.reloadCaptures(); m.rescanNam() }
            }
            .font(.inter(size: T(9) * UI)).buttonStyle(.borderless)
            .padding(.horizontal, 12).padding(.vertical, 7)
        }
        .font(.inter(size: T(10.5) * UI))
        .frame(width: 520 * Z, height: 480 * Z)
    }
}
