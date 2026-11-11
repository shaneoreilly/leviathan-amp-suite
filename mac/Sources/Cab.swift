import SwiftUI

// The cabinet picker: one button on the CABINET panel naming the current cab and where it comes from, ‹ › to step
// through its list, and a browser (popover) with search, Built-in / God's Cab / Library / Favourites / Recent.
// Windows: UI/CabBrowser.cpp does the same.
//
// A cab, wherever it comes from, is one id: "b:<index>" built-in, "u" the loaded single file, "l:<rel>" an IR in the
// library, "g:<rel>" a God's Cab IR (as mic A). Favourites and recents keep these per user (UserDefaults, not presets).

final class CabUI: ObservableObject {
    static let shared = CabUI()
    private static let d = UserDefaults.standard
    @Published var open = ProcessInfo.processInfo.environment["LEVIATHAN_SHOW_CAB"] != nil   // UI testing: open on launch
    @Published var query = ""
    @Published var folder = ""   // Library tab: "" = every folder
    @Published var tab = d.string(forKey: "cabTab") ?? "library" { didSet { CabUI.d.set(tab, forKey: "cabTab") } }
    @Published var favs: [String] = d.stringArray(forKey: "cabFavs") ?? [] { didSet { CabUI.d.set(favs, forKey: "cabFavs") } }
    @Published var recent: [String] = d.stringArray(forKey: "cabRecent") ?? [] { didSet { CabUI.d.set(recent, forKey: "cabRecent") } }
    func toggleFav(_ id: String) { if let i = favs.firstIndex(of: id) { favs.remove(at: i) } else { favs.append(id) } }
    func noteRecent(_ id: String) { recent = Array(([id] + recent.filter { $0 != id }).prefix(12)) }
}

struct CabItem: Hashable { let id, name, meta: String }

extension Model {
    var cabID: String {
        switch s.cabModel {
        case CAB_IRLIB: return "l:" + s.irFile
        case CAB_LIB: return "g:" + s.irA
        case CAB_USER: return "u"
        default: return "b:\(s.cabModel)"
        }
    }
    func cabName(_ id: String) -> String {
        let v = String(id.dropFirst(2))
        if id == "u" { return userIRName ?? "IR file" }
        if id.hasPrefix("b:") { return Int(v).flatMap { $0 >= 0 && $0 < CABS.count ? CABS[$0].name : nil } ?? "Cabinet" }
        if id.hasPrefix("l:") { return v.isEmpty ? "Choose an IR…" : CabIRLibrary.title(v) }
        if id.hasPrefix("g:") { return irLib.file(v).map { "\($0.mic) · \($0.position)" } ?? "God's Cab" }
        return "Cabinet"
    }
    func cabSource(_ id: String) -> String {
        if id == "u" { return "FILE" }
        switch id.prefix(2) { case "b:": return "BUILT-IN"; case "l:": return "LIBRARY"; case "g:": return "GOD'S CAB"; default: return "" }
    }
    /// Second line under the name: the folder, the God's Cab extras, or what kind of cab it is.
    func cabDetail(_ id: String) -> String {
        let v = String(id.dropFirst(2))
        if id.hasPrefix("l:") { let f = (v as NSString).deletingLastPathComponent; return f.isEmpty ? "IR library" : f }
        if id.hasPrefix("g:"), let f = irLib.file(v) {
            var t = "Mesa 4x12 · Presence \(f.pres)" + (f.ts ? " · TS" : "")
            if s.cabModel == CAB_LIB, let b = irLib.file(s.irB) { t += " + \(b.mic)" }
            return t
        }
        if id == "u" { return "Single IR file" }
        return "Leviathan cab model"
    }
    func applyCab(_ id: String) {
        var x = s
        let v = String(id.dropFirst(2))
        if id == "u" { x.cabModel = CAB_USER }
        else if id.hasPrefix("b:"), let i = Int(v) { x.cabModel = i }
        else if id.hasPrefix("l:") { x.cabModel = CAB_IRLIB; x.irFile = v }
        else if id.hasPrefix("g:") { x.cabModel = CAB_LIB; x.irA = v }
        x.cabOn = true
        s = x
        CabUI.shared.noteRecent(id)
    }
    /// ‹ ›: the next cab in the current one's own list.
    func stepCab(_ d: Int) {
        switch s.cabModel {
        case CAB_IRLIB: if let r = cabIRs.step(s.irFile, by: d) { applyCab("l:" + r) }
        case CAB_LIB: if let r = irLib.step(s.irA, by: d) { applyCab("g:" + r) }
        case CAB_USER: break
        default: applyCab("b:\((s.cabModel + d + CABS.count) % CABS.count)")
        }
    }
    var builtinCabs: [CabItem] {
        CABS.indices.map { CabItem(id: "b:\($0)", name: CABS[$0].name, meta: "Built-in") }
            + (userIRName.map { [CabItem(id: "u", name: $0, meta: "Single file")] } ?? [])
    }
    var libraryCabs: [CabItem] {
        cabIRs.files.map { r in CabItem(id: "l:" + r, name: CabIRLibrary.title(r), meta: (r as NSString).deletingLastPathComponent) }
    }
    func cabItem(_ id: String) -> CabItem { CabItem(id: id, name: cabName(id), meta: cabSource(id).capitalized) }
}

/// The CABINET panel's picker: current cab (opens the browser), ‹ ›, and the add-IRs menu.
struct CabPicker: View {
    @EnvironmentObject var m: Model
    @ObservedObject private var ui = CabUI.shared
    var body: some View {
        let id = m.cabID
        SourceButtonRow(badge: m.cabSource(id), name: m.cabName(id), detail: m.cabDetail(id), help: "Choose a cabinet",
                        open: $ui.open, prev: { m.stepCab(-1) }, next: { m.stepCab(1) },
                        popover: { CabBrowser().environmentObject(m) },
                        menu: {
                            Button("Add IRs to the library (files or a folder)…") { m.importCabIRs() }
                            Button("Load a single IR file…") { m.loadIR() }
                            Button("Open IR library folder") { NSWorkspace.shared.open(CabIRLibrary.root) }
                        })
    }
}

struct CabBrowser: View {
    @EnvironmentObject var m: Model
    @ObservedObject private var ui = CabUI.shared

    var tabs: [(id: String, label: String, count: Int)] {
        var t: [(String, String, Int)] = [("builtin", "Built-in", m.builtinCabs.count)]
        if !m.irLib.isEmpty { t.append(("gods", "God's Cab", m.irLib.files.count)) }
        t.append(("library", "Library", m.cabIRs.files.count))
        t.append(("favs", "Favourites", ui.favs.count))
        t.append(("recent", "Recent", ui.recent.count))
        return t.map { (id: $0.0, label: $0.1, count: $0.2) }
    }

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                TextField("Search every cab: name, band, folder…", text: $ui.query).textFieldStyle(.plain)
                if !ui.query.isEmpty { Button { ui.query = "" } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(.secondary) }.buttonStyle(.plain) }
            }
            .padding(.horizontal, 10).padding(.vertical, 8)
            .background(RoundedRectangle(cornerRadius: 8).fill(PAL.tint.opacity(0.06)))
            .padding(12)
            if ui.query.isEmpty {
                HStack(spacing: 2) {
                    ForEach(tabs, id: \.id) { t in
                        Button { ui.tab = t.id } label: {
                            HStack(spacing: 5) {
                                Text(t.label).font(.inter(size: T(10.5) * UI, weight: .semibold))
                                Text("\(t.count)").font(.inter(size: T(9) * UI)).foregroundStyle(.secondary)
                            }
                            .foregroundStyle(ui.tab == t.id ? .primary : .secondary)
                            .padding(.horizontal, 10).padding(.vertical, 7)
                            .overlay(alignment: .bottom) { Rectangle().fill(ui.tab == t.id ? Color.accentColor : .clear).frame(height: 2) }
                            .contentShape(Rectangle())
                        }.buttonStyle(.plain)
                    }
                    Spacer()
                }
                .padding(.horizontal, 12)
                Divider()
            }
            content.frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            Divider()
            HStack {
                Text("Click to hear it · ★ adds to Favourites")
                Spacer()
                Text("Add IRs: ⋯ on the panel, or Settings → Library")
            }
            .font(.inter(size: T(9) * UI)).foregroundStyle(.secondary).padding(.horizontal, 12).padding(.vertical, 7)
        }
        .font(.inter(size: T(10.5) * UI))
        .frame(width: 720 * Z, height: 520 * Z)
    }

    @ViewBuilder var content: some View {
        let tab = tabs.contains { $0.id == ui.tab } ? ui.tab : "builtin"
        if !ui.query.isEmpty {
            let words = ui.query.lowercased().split(separator: " ").map(String.init)
            let match = { (c: CabItem) in let t = "\(c.name) \(c.meta)".lowercased(); return words.allSatisfy { t.contains($0) } }
            let hits = (m.builtinCabs + m.libraryCabs).filter(match)
            list(hits, empty: "No cab matches “\(ui.query)”.")
        } else if tab == "gods" {
            GodsCabPicker()
        } else if tab == "library" {
            let folders = Array(Set(m.cabIRs.files.map { ($0 as NSString).deletingLastPathComponent })).sorted()
            HStack(spacing: 0) {
                if folders.count > 1 || folders.first != "" {
                    ScrollView {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("FOLDERS").font(.inter(size: T(9) * UI, weight: .semibold)).tracking(1).foregroundStyle(.secondary).padding(.horizontal, 8).padding(.bottom, 4)
                            folderRow("", "All", m.cabIRs.files.count)
                            ForEach(folders, id: \.self) { f in folderRow(f, f.isEmpty ? "(top level)" : f, m.cabIRs.files.filter { ($0 as NSString).deletingLastPathComponent == f }.count) }
                        }.padding(8)
                    }
                    .frame(width: 180 * Z)
                    Divider()
                }
                let items = m.libraryCabs.filter { ui.folder.isEmpty || $0.meta == ui.folder }.map { CabItem(id: $0.id, name: $0.name, meta: "") }   // the folder is in the sidebar
                list(items, empty: "No IRs in the library yet. Add some with ⋯ on the panel.")
            }
        } else if tab == "favs" {
            list(ui.favs.map { m.cabItem($0) }, empty: "Nothing here yet. Click ☆ on any cab to keep it here.")
        } else if tab == "recent" {
            list(ui.recent.map { m.cabItem($0) }, empty: "Cabs you pick show up here.")
        } else {
            list(m.builtinCabs, empty: "")
        }
    }

    func folderRow(_ f: String, _ label: String, _ n: Int) -> some View {
        Button { ui.folder = f } label: {
            HStack { Text(label).lineLimit(1); Spacer(); Text("\(n)").foregroundStyle(.secondary) }
                .padding(.horizontal, 8).padding(.vertical, 5)
                .background(RoundedRectangle(cornerRadius: 6).fill(ui.folder == f ? PAL.tint.opacity(0.08) : .clear))
                .contentShape(Rectangle())
        }.buttonStyle(.plain)
    }

    @ViewBuilder func list(_ items: [CabItem], empty: String) -> some View {
        if items.isEmpty {
            Text(empty).foregroundStyle(.secondary).frame(maxWidth: .infinity).padding(30)
        } else {
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(spacing: 1) {
                        ForEach(items, id: \.self) { c in
                            let on = c.id == m.cabID, fav = ui.favs.contains(c.id)
                            HStack(spacing: 8) {
                                Button { ui.toggleFav(c.id) } label: {
                                    Image(systemName: fav ? "star.fill" : "star").foregroundStyle(fav ? Color.yellow : Color.secondary)
                                }.buttonStyle(.plain).help(fav ? "Remove from Favourites" : "Add to Favourites")
                                Button { m.applyCab(c.id) } label: {
                                    HStack {
                                        Text(c.name).fontWeight(on ? .semibold : .regular).foregroundStyle(on ? Color.accentColor : .primary).lineLimit(1)
                                        Spacer()
                                        Text(c.meta).font(.inter(size: T(9) * UI)).foregroundStyle(.secondary).lineLimit(1)
                                    }.contentShape(Rectangle())
                                }.buttonStyle(.plain)
                            }
                            .padding(.horizontal, 10).padding(.vertical, 6)
                            .background(RoundedRectangle(cornerRadius: 6).fill(on ? Color.accentColor.opacity(0.12) : .clear))
                            .id(c.id)
                        }
                    }.padding(6)
                }
                .onAppear { proxy.scrollTo(m.cabID, anchor: .center) }
            }
        }
    }
}

/// God's Cab: the IR built from its mic and position, as rows of choices; an optional second mic blends in.
struct GodsCabPicker: View {
    @EnvironmentObject var m: Model

    var body: some View {
        let lib = m.irLib
        let a = lib.file(m.s.irA) ?? lib.files.first
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text("Mesa 4x12, recorded with \(lib.mics.count) mics in several positions. Pick a mic and a spot on the speaker; a second mic blends in with A ↔ B on the panel.")
                    .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                if let a {
                    chips("MICROPHONE", lib.mics, a.mic) { pick(\.irA, mic: $0, ts: a.ts, pos: a.position, pres: a.pres) }
                    chips("POSITION", lib.positions(mic: a.mic, ts: a.ts), a.position) { pick(\.irA, mic: a.mic, ts: a.ts, pos: $0, pres: a.pres) }
                    chips("PRESENCE", (1...5).map(String.init), String(a.pres)) { pick(\.irA, mic: a.mic, ts: a.ts, pos: a.position, pres: Int($0) ?? 3) }
                    if lib.tsAvailable(mic: a.mic) {
                        chips("BOOST IN FRONT", ["None", "Tube Screamer"], a.ts ? "Tube Screamer" : "None") { pick(\.irA, mic: a.mic, ts: $0 != "None", pos: a.position, pres: a.pres) }
                    }
                    let b = lib.file(m.s.irB)
                    chips("SECOND MIC", ["Off"] + lib.mics, b?.mic ?? "Off") { mic in
                        if mic == "Off" { m.s.irB = "" } else { pick(\.irB, mic: mic, ts: b?.ts ?? a.ts, pos: b?.position ?? a.position, pres: b?.pres ?? a.pres) }
                    }
                    if let b {
                        chips("SECOND MIC POSITION", lib.positions(mic: b.mic, ts: b.ts), b.position) { pick(\.irB, mic: b.mic, ts: b.ts, pos: $0, pres: b.pres) }
                    }
                }
            }.padding(16)
        }
    }

    func pick(_ key: WritableKeyPath<Settings, String>, mic: String, ts: Bool, pos: String, pres: Int) {
        guard let f = m.irLib.find(mic: mic, ts: ts, position: pos, pres: pres) else { return }
        var x = m.s
        x[keyPath: key] = f.rel; x.cabModel = CAB_LIB; x.cabOn = true
        m.s = x
        CabUI.shared.noteRecent("g:" + x.irA)
    }

    func chips(_ title: String, _ options: [String], _ current: String, _ choose: @escaping (String) -> Void) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title).font(.inter(size: T(9) * UI, weight: .semibold)).tracking(1).foregroundStyle(.secondary)
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 92 * Z), spacing: 6)], alignment: .leading, spacing: 6) {
                ForEach(options, id: \.self) { o in
                    let on = o == current
                    Button { choose(o) } label: {
                        Text(o).lineLimit(1).frame(maxWidth: .infinity)
                            .padding(.horizontal, 10).padding(.vertical, 6)
                            .foregroundStyle(on ? Color.primary : .secondary)
                            .background(Capsule().fill(on ? Color.accentColor.opacity(0.22) : PAL.tint.opacity(0.05)))
                            .overlay(Capsule().stroke(on ? Color.accentColor : PAL.tint.opacity(0.12)))
                            .contentShape(Capsule())
                    }.buttonStyle(.plain)
                }
            }
        }
    }
}
