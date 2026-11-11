import SwiftUI
import AppKit

// MARK: - Root: sidebar + top bar + screen

struct RootView: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var rec: Sessions
    @EnvironmentObject var updater: Updater
    @EnvironmentObject var midi: Midi

    var ch: Color { m.accent }

    var body: some View {
        HStack(spacing: 0) {
            Sidebar()
            Rectangle().fill(line).frame(width: 1)
            VStack(spacing: 0) {
                TopBar()
                if updater.showBanner, let r = updater.available { UpdateBanner(release: r) } else { UpdateProgressBanner() }
                ForEach([m.status, rec.message].filter { !$0.isEmpty }, id: \.self) { msg in
                    HStack {
                        Text(msg).font(.inter(size: T(12) * UI))
                        Spacer()
                        // an error that a setting fixes gets a button that goes there
                        let fix = msg == rec.message ? rec.currentFix : m.currentFix
                        if !fix.isEmpty { Button("Click to resolve") { m.resolve(fix) }.font(.inter(size: T(12) * UI)) }
                        Button { if msg == rec.message { rec.message = "" } else { m.status = "" } } label: { Image(systemName: "xmark") }
                            .buttonStyle(.borderless)
                    }
                    .padding(10)
                    .background(Color(red: 0.16, green: 0.12, blue: 0.06)).foregroundStyle(Color(red: 1, green: 0.85, blue: 0.6))
                }
                if midi.learning && m.screen == .rig { MidiLearnBanner() }
                switch m.screen {
                case .rig: ContentView()
                case .presets: PresetsView()
                case .recordings: RecordingsView()
                case .settings: SettingsView()
                }
            }
        }
        .id(m.zoom)   // rebuild everything at the new size
        .font(.inter(size: T(13) * UI))
        .controlSize(.large)
        .background(Color(white: 0.05))
        .tint(ch)
        .sheet(isPresented: $updater.showNotes) {
            if let r = updater.available { WhatsNewSheet(updater: updater, release: r).tint(ch) }
        }
        .alert("Save changes to \u{201C}\(m.presetName)\u{201D}?", isPresented: $m.askUnsaved) {
            Button("Save") { m.resolveUnsaved(save: true) }
            Button("Don\u{2019}t save", role: .destructive) { m.resolveUnsaved(save: false) }
            Button("Cancel", role: .cancel) { m.resolveUnsaved(save: nil) }
        } message: { Text("You\u{2019}ve changed this preset since it was last saved.") }
        .alert("Save preset", isPresented: $m.savingPreset) {
            TextField("Name", text: $m.newPresetName)
            Button("Save") { m.savePreset(m.newPresetName) }
            Button("Cancel", role: .cancel) {}
        }
    }
}

struct Sidebar: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var rec: Sessions
    @EnvironmentObject var updater: Updater

    var body: some View {
        VStack(spacing: 4) {
            item(.rig, "slider.vertical.3", "Rig")
            item(.presets, "list.bullet", "Presets")
            item(.recordings, "waveform", "Recordings", badge: rec.recording)
            Spacer()
            zoomControl
            item(.settings, "gearshape", "Settings", badge: updater.available != nil)
        }
        .padding(.vertical, 10)
        .frame(width: 92 * Z)
        .background(Color(white: 0.07))
    }

    var zoomControl: some View {
        VStack(spacing: 2) {
            HStack(spacing: 2) {
                Button { m.zoomStep(-1) } label: { Image(systemName: "minus").frame(width: 30 * Z, height: 26 * Z).contentShape(Rectangle()) }   // the whole box clicks, not just the glyph
                    .keyboardShortcut("-", modifiers: .command).help("Zoom out (⌘−)")
                    .disabled(m.zoom <= Model.zoomSteps.first! + 0.001)
                Button { m.zoomStep(1) } label: { Image(systemName: "plus").frame(width: 30 * Z, height: 26 * Z).contentShape(Rectangle()) }   // the whole box clicks, not just the glyph
                    .keyboardShortcut("=", modifiers: .command).help("Zoom in (⌘+)")
                    .disabled(m.zoom >= Model.zoomSteps.last! - 0.001)
            }
            .buttonStyle(.plain)
            .font(.inter(size: T(13) * Z, weight: .semibold))
            .background(RoundedRectangle(cornerRadius: 7).fill(Color.white.opacity(0.06)))
            Button { m.zoom = 1 } label: {
                Text("\(Int((m.zoom * 100).rounded()))%").font(.inter(size: T(9) * UI).monospacedDigit())
            }
            .buttonStyle(.plain).foregroundStyle(.secondary).keyboardShortcut("0", modifiers: .command).help("Actual size (⌘0)")
        }
        .foregroundStyle(.secondary)
        .padding(.bottom, 6)
    }

    func item(_ s: Screen, _ icon: String, _ label: String, badge: Bool = false) -> some View {
        let on = m.screen == s
        return Button { m.screen = s } label: {
            VStack(spacing: 4) {
                Image(systemName: icon).font(.inter(size: T(20) * UI * 0.8))
                    .overlay(alignment: .topTrailing) { if badge { Circle().fill(.red).frame(width: 9, height: 9).offset(x: 6, y: -4) } }
                Text(label).font(.inter(size: T(9) * UI, weight: .semibold))
            }
            .frame(width: 80 * Z, height: 70 * Z)
            .foregroundStyle(on ? Color.accentColor : .secondary)
            .background(RoundedRectangle(cornerRadius: 10).fill(on ? Color.white.opacity(0.07) : .clear))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .help(label)
    }
}

struct TopBar: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var rec: Sessions
    var ch: Color { m.accent }

    var body: some View {
        HStack(spacing: 14) {
            LogoMark(size: 40 * Z, color: ch)
            VStack(alignment: .leading, spacing: 0) {
                Text("LEVIATHAN").font(.inter(size: T(15) * UI, weight: .heavy)).tracking(4)
                Text("OPEN GUITAR RIG").font(.inter(size: T(9) * UI)).tracking(1.5).foregroundStyle(.secondary)
            }
            .fixedSize()
            Button(m.live ? "⏻ On" : "⏻ Start") { m.toggle() }
                .buttonStyle(.borderedProminent).tint(m.live ? .green : ch)
                .keyboardShortcut(m.screen == .recordings ? nil : KeyboardShortcut(.space, modifiers: []))
                .explain("top:power", "Audio on/off")
            LevelMeters(mt: m.meters).explain("top:meters", "Levels")
            if let i = m.info { LatencyLabel(info: i, mt: m.meters).explain("top:latency", "Latency") }
            if rec.recording {
                LiveWaveView(wave: rec.liveWave, color: .red).frame(width: 240 * W, height: 34 * Z).explain("top:recWave", "Recording")
            }
            Spacer(minLength: 8)
            Button { rec.toggleRecord() } label: {
                HStack(spacing: 6) {
                    Circle().fill(rec.recording ? Color.red : Color(white: 0.5)).frame(width: 12, height: 12)
                    Text(rec.recording ? mmss(rec.recSeconds) : "Rec").monospacedDigit()
                }
            }
            .buttonStyle(TopBarToggle(on: rec.recording))
            .keyboardShortcut("r", modifiers: [.command])
            .help("Record dry + wet (⌘R)")
            .explain("top:rec", "Record")
            TempoControl()
            Button("Tuner") { m.tunerOn.toggle() }.buttonStyle(TopBarToggle(on: m.tunerOn)).keyboardShortcut("t", modifiers: [])
                .overlay { MidiLearnOverlay(armed: .action("@tuner", "Tuner"), cornerRadius: 6) }
                .explain("top:tuner", "Tuner")
            MidiLearnButton().explain("top:midi", "MIDI learn")
            HStack(spacing: 2) {
                Button { m.confirmLoad { m.stepPreset(-1) } } label: { Image(systemName: "chevron.left") }.help("Previous preset")
                    .overlay { MidiLearnOverlay(armed: .action("@prev", "Previous preset"), cornerRadius: 6) }
                    .explain("top:prev", "Previous preset")
                PresetMenu().fixedSize().explain("top:preset", "Preset")   // as wide as its name, so ‹ › sit right beside it
                Button { m.confirmLoad { m.stepPreset(1) } } label: { Image(systemName: "chevron.right") }.help("Next preset")
                    .overlay { MidiLearnOverlay(armed: .action("@next", "Next preset"), cornerRadius: 6) }
                    .explain("top:next", "Next preset")
            }
            Button { m.undo() } label: { Image(systemName: "arrow.uturn.backward") }
                .keyboardShortcut("z").disabled(!m.canUndo).help("Undo the last change to the rig (⌘Z)")
                .explain("top:undo", "Undo")
            Button("Save") { m.saveCurrent() }.keyboardShortcut("s").disabled(!m.presetModified).help("Save changes to this preset (⌘S)")
                .explain("top:save", "Save")
            Button("Save as…") { m.newPresetName = m.presetName.isEmpty ? "" : m.presetName + " copy"; m.savingPreset = true }
                .keyboardShortcut("s", modifiers: [.command, .shift]).help("Save as a new preset, leaving this one unchanged (⇧⌘S)")
                .explain("top:saveAs", "Save as")
        }
        .lineLimit(1)
        .padding(.horizontal, 14).padding(.vertical, 10)
        .background(Color(white: 0.07))
        .overlay(alignment: .bottom) { Rectangle().fill(line).frame(height: 1) }
    }
}

struct PresetMenu: View {
    @EnvironmentObject var m: Model
    var body: some View {
        Menu((m.presetModified ? "● " : "") + m.presetName) {
            let recent = Array(m.recentlyPlayed.prefix(5))
            if !recent.isEmpty { Section("Recently played") { ForEach(recent, id: \.self) { n in Button(n) { m.confirmLoad { m.loadAny(n) } } } } }
            if !m.favorites.isEmpty {
                Section("Favorites") { ForEach(m.favorites.sorted(), id: \.self) { n in Button("★ \(n)") { m.confirmLoad { m.loadAny(n) } } } }
            }
            Section("Factory") {
                ForEach(FACTORY.map(\.0), id: \.self) { n in Button(m.userPresets[n] != nil ? "\(n) (edited)" : n) { m.confirmLoad { m.loadFactory(n) } } }
            }
            let mine = m.userPresets.keys.filter { !m.isFactory($0) }.sorted()
            if !mine.isEmpty { Section("Yours") { ForEach(mine, id: \.self) { n in Button(n) { m.confirmLoad { m.loadUser(n) } } } } }
            Divider()
            Button("Save “\(m.presetName)”") { m.saveCurrent() }
            Button("Save as new…") { m.newPresetName = ""; m.savingPreset = true }
            Divider()
            Button("Export “\(m.presetName)”…") { m.exportPreset(m.presetName) }.disabled(m.presetSettings(m.presetName) == nil)
            Button("Import presets…") { m.importPresetsPanel() }
            if m.isFactory(m.presetName) && m.userPresets[m.presetName] != nil {
                Button("Restore factory “\(m.presetName)”") { m.restoreFactory(m.presetName) }
            }
        }
    }
}

// MARK: - Presets screen

struct PresetsView: View {
    @EnvironmentObject var m: Model
    @StateObject private var ui = PresetsUI()

    /// Favourites, the tags picked (all of them), then the search words (each in the name or a tag).
    var names: [String] {
        let all = m.sortedPresets(m.presetSort, reversed: m.presetSortReversed)
        let words = ui.query.lowercased().split(separator: " ").map(String.init)
        return all.filter { n in
            if ui.favOnly && !m.favorites.contains(n) { return false }
            let tags = m.tags(n)
            if !ui.tagFilter.isSubset(of: Set(tags)) { return false }
            let hay = ([n] + tags).joined(separator: " ").lowercased()
            return words.allSatisfy { hay.contains($0) }
        }
    }
    /// Every tag in use, most common first.
    var allTags: [(tag: String, count: Int)] {
        var c: [String: Int] = [:]
        for n in m.sortedPresets(.name, reversed: false) { for t in m.tags(n) { c[t, default: 0] += 1 } }
        return c.map { (tag: $0.key, count: $0.value) }.sorted { $0.count != $1.count ? $0.count > $1.count : $0.tag < $1.tag }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Text("PRESETS").font(.inter(size: T(10) * UI, weight: .bold)).tracking(2).foregroundStyle(.secondary)
                Picker("", selection: $ui.favOnly) { Text("All").tag(false); Text("★ Favorites").tag(true) }
                    .pickerStyle(.segmented).labelsHidden().frame(width: 260 * Z)
                HStack(spacing: 6) {
                    Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                    TextField("Search presets and tags", text: $ui.query).textFieldStyle(.plain)
                    if !ui.query.isEmpty { Button { ui.query = "" } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(.secondary) }.buttonStyle(.plain) }
                }
                .padding(.horizontal, 10).padding(.vertical, 6).frame(width: 280 * Z)
                .background(RoundedRectangle(cornerRadius: 8).fill(Color.white.opacity(0.06)))
                Spacer()
                Button("Import…") { m.importPresetsPanel() }.help("Add .leviathan preset files (or drop them here)")
                Button("Export all…") { m.exportAllPresets() }.help("Save every preset as a .leviathan file in a folder")
                Button("Save as new…") { m.newPresetName = ""; m.savingPreset = true }
            }
            .padding(14)
            tagBar
            columnHeaders
            List {
                ForEach(names, id: \.self) { n in row(n) }
            }
            .listStyle(.inset)
            .scrollContentBackground(.hidden)
        }
        .onDrop(of: [.fileURL], isTargeted: nil) { providers in
            for p in providers {
                _ = p.loadObject(ofClass: URL.self) { url, _ in
                    guard let url, url.pathExtension.lowercased() == Model.presetExt else { return }
                    DispatchQueue.main.async { MainActor.assumeIsolated { m.importPresets([url]) } }
                }
            }
            return true
        }
        .alert("Tags for \(ui.tagging ?? "")", isPresented: Binding(get: { ui.tagging != nil }, set: { if !$0 { ui.tagging = nil } })) {
            TextField("e.g. live, drop C, favourite solo", text: $ui.tagText)
            Button("Save") { if let n = ui.tagging { m.setUserTags(n, ui.tagText) }; ui.tagging = nil }
            Button("Cancel", role: .cancel) { ui.tagging = nil }
        } message: { Text("Separate tags with commas. Factory or Yours, the amp and style words in the name are tagged for you.") }
        .alert("Rename preset", isPresented: Binding(get: { ui.renaming != nil }, set: { if !$0 { ui.renaming = nil } })) {
            TextField("Name", text: $ui.newName)
            Button("Rename") { if let r = ui.renaming { m.renamePreset(r, to: ui.newName) }; ui.renaming = nil }
            Button("Cancel", role: .cancel) { ui.renaming = nil }
        }
    }

    func subtitle(_ n: String, factory: Bool, edited: Bool) -> String {
        var t = m.tags(n)
        if edited, let i = t.firstIndex(of: "Factory") { t[i] = "Factory · edited" }
        return t.joined(separator: " · ")
    }

    /// Tag chips to narrow the list: click to filter by a tag (several narrow further), click again to drop it.
    var tagBar: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 6) {
                ForEach(allTags, id: \.tag) { t in
                    let on = ui.tagFilter.contains(t.tag)
                    Button { if on { ui.tagFilter.remove(t.tag) } else { ui.tagFilter.insert(t.tag) } } label: {
                        HStack(spacing: 4) { Text(t.tag); Text("\(t.count)").foregroundStyle(.secondary) }
                            .font(.inter(size: T(9) * UI))
                            .padding(.horizontal, 9).padding(.vertical, 4)
                            .background(Capsule().fill(on ? Color.accentColor.opacity(0.22) : Color.white.opacity(0.05)))
                            .overlay(Capsule().stroke(on ? Color.accentColor : Color.white.opacity(0.12)))
                            .contentShape(Capsule())
                    }.buttonStyle(.plain)
                }
                if !ui.tagFilter.isEmpty { Button("Clear") { ui.tagFilter = [] }.buttonStyle(.borderless).font(.inter(size: T(9) * UI)) }
            }
            .padding(.horizontal, 24).padding(.bottom, 10)
        }
    }

    static let dateColumn: CGFloat = 170, actionsColumn: CGFloat = 410
    func stamp(_ d: Date?) -> String { d.map { DateFormatter.presetStamp.string(from: $0) } ?? "—" }

    /// Click a header to sort by it; click again to reverse.
    var columnHeaders: some View {
        HStack(spacing: 12) {
            Color.clear.frame(width: 20, height: 1)
            header("NAME", .name).frame(maxWidth: .infinity, alignment: .leading)
            header("MODIFIED", .modified).frame(width: PresetsView.dateColumn * Z, alignment: .leading)
            header("LAST PLAYED", .played).frame(width: PresetsView.dateColumn * Z, alignment: .leading)
            Color.clear.frame(width: PresetsView.actionsColumn * Z, height: 1)
        }
        .fixedSize(horizontal: false, vertical: true)
        .padding(.horizontal, 24).padding(.bottom, 4)
    }
    func header(_ title: String, _ sort: PresetSort) -> some View {
        let on = m.presetSort == sort
        return Button { m.sortPresets(by: sort) } label: {
            HStack(spacing: 4) {
                Text(title).font(.inter(size: T(10) * UI, weight: .bold)).tracking(1.2)
                if on { Image(systemName: m.presetSortReversed ? "chevron.up" : "chevron.down").font(.inter(size: T(8) * UI, weight: .bold)) }
            }
            .foregroundStyle(on ? Color.accentColor : .secondary)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .help(sort == .name ? "Sort A to Z (click again for Z to A)" : "Sort newest first (click again for oldest first)")
    }

    func row(_ n: String) -> some View {
        let current = n == m.presetName, factory = m.isFactory(n), edited = factory && m.userPresets[n] != nil
        return HStack(spacing: 12) {
            Button { m.toggleFavorite(n) } label: { Image(systemName: m.favorites.contains(n) ? "star.fill" : "star") }
                .buttonStyle(.borderless).foregroundStyle(m.favorites.contains(n) ? Color.yellow : .secondary)
            VStack(alignment: .leading, spacing: 2) {
                Text(n).font(.inter(size: T(14) * UI, weight: current ? .bold : .regular))
                Text(subtitle(n, factory: factory, edited: edited)).font(.inter(size: T(10) * UI)).foregroundStyle(.secondary)
            }
            Spacer()
            Text(stamp(m.presetMeta[n]?.shownModified)).font(.inter(size: T(12) * UI).monospacedDigit()).foregroundStyle(.secondary)
                .lineLimit(1).fixedSize().frame(width: PresetsView.dateColumn * Z, alignment: .leading)
            Text(stamp(m.presetMeta[n]?.played)).font(.inter(size: T(12) * UI).monospacedDigit()).foregroundStyle(.secondary)
                .lineLimit(1).fixedSize().frame(width: PresetsView.dateColumn * Z, alignment: .leading)
            HStack(spacing: 8) {
            if current { Text(m.presetModified ? "Loaded · unsaved" : "Loaded").font(.inter(size: T(10) * UI)).foregroundStyle(.secondary).lineLimit(1).fixedSize() }
            Button("Load") { m.confirmLoad { m.loadAny(n); m.screen = .rig } }
            Button("Export…") { m.exportPreset(n) }
            Button("Tags…") { ui.tagText = m.userTags(n).joined(separator: ", "); ui.tagging = n }.help("Add your own tags to this preset")
            if !factory {
                Button("Rename") { ui.newName = n; ui.renaming = n }
                Button("Delete", role: .destructive) { m.deletePreset(n) }
            } else if edited {
                Button("Restore factory") { m.restoreFactory(n) }
            }
            }
            .frame(width: PresetsView.actionsColumn * Z, alignment: .trailing)
        }
        .padding(.vertical, 6)
        .contentShape(Rectangle())
        .onTapGesture(count: 2) { m.confirmLoad { m.loadAny(n); m.screen = .rig } }
    }
}

// MARK: - Recordings screen

struct RecordingsView: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var rec: Sessions
    @StateObject private var ui = RecordingsUI()

    var body: some View {
        HStack(spacing: 0) {
            sessionList.frame(width: 300 * Z)
            Rectangle().fill(line).frame(width: 1)
            if let s = rec.current { detail(s) } else {
                VStack(spacing: 12) {
                    Text(rec.list.isEmpty ? "No recordings yet" : "Select a recording").font(.inter(size: T(16) * UI))
                    Text("Press Rec (⌘R) to capture your dry guitar and the amp's output together.\nDry takes can be re-amped through any preset later.")
                        .multilineTextAlignment(.center).foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
    }

    var sessionList: some View {
        List(selection: $rec.selected) {
            ForEach(rec.list) { s in
                HStack {
                    VStack(alignment: .leading, spacing: 2) {
                        Text(s.meta.name).font(.inter(size: T(13) * UI)).lineLimit(1)
                        Text(DateFormatter.nice.string(from: s.meta.date)).font(.inter(size: T(9) * UI)).foregroundStyle(.secondary)
                    }
                    Spacer()
                    Text(mmss(s.meta.duration)).font(.inter(size: T(11) * UI).monospacedDigit()).foregroundStyle(.secondary)
                }
                .padding(.vertical, 3)
                .tag(s.id)
                .contextMenu {
                    Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting([s.wet]) }
                    Divider()
                    Button("Move to Trash", role: .destructive) { rec.delete(s.id) }
                }
            }
        }
        .listStyle(.sidebar)
        .onDeleteCommand { rec.delete() }   // Delete key on the selected recording
    }

    func detail(_ s: Session) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            WaveformView(samples: rec.waveform, duration: s.meta.duration, position: rec.playing ? rec.playPos : nil,
                         seek: { rec.seek($0) }, select: { a, b in rec.selection = .init(start: a, end: b) },
                         clear: { t in if let sel = rec.selection, t < sel.start || t > sel.end { rec.selection = nil } })
                .overlay {
                    if let sel = rec.selection {
                        TrimOverlay(duration: s.meta.duration,
                                    start: Binding(get: { sel.start }, set: { rec.selection?.start = $0 }),
                                    end: Binding(get: { sel.end }, set: { rec.selection?.end = $0 }))
                    }
                }
                .frame(height: 220 * Z)
            if let sel = rec.selection {
                HStack(spacing: 10) {
                    Image(systemName: "selection.pin.in.out").foregroundStyle(.secondary)
                    Text("Selected \(mmssTenths(sel.start)) – \(mmssTenths(sel.end)) (\(mmssTenths(sel.end - sel.start)))").monospacedDigit()
                    Text("Drag on the waveform to select, or drag the yellow handles.").foregroundStyle(.secondary).lineLimit(1)
                    Spacer()
                    Button("Play selection") { rec.play(from: sel.start, to: sel.end) }
                    Button("Clear") { rec.selection = nil }
                    Button("Trim selection") { rec.cut(sel.start, sel.end); rec.selection = nil }
                        .help("Cut the selected part out of this take (the original is kept, so you can restore it)")
                        .disabled(sel.end - sel.start < 0.05 || (sel.start <= 0.001 && sel.end >= s.meta.duration - 0.001))
                    Button("Trim to selection") { rec.trim(sel.start, sel.end); rec.selection = nil }
                        .buttonStyle(.borderedProminent)
                        .help("Keep only the selected part of this take (the original is kept, so you can restore it)")
                        .disabled(sel.end - sel.start < 0.05 || (sel.start <= 0.001 && sel.end >= s.meta.duration - 0.001))
                }
                .padding(.horizontal, 14).padding(.vertical, 8)
                .background(Color(white: 0.11))
            }
            HStack(spacing: 10) {
                Spacer()
                Button { step(-1) } label: { Image(systemName: "backward.end.fill") }.help("Previous recording")
                Button { rec.togglePlay() } label: { Image(systemName: rec.playing ? "pause.fill" : "play.fill").frame(width: 30) }
                    .keyboardShortcut(.return, modifiers: [])
                Button("") { rec.togglePlay() }.keyboardShortcut(.space, modifiers: []).frame(width: 0, height: 0).opacity(0)   // Space plays/pauses here
                Button { step(1) } label: { Image(systemName: "forward.end.fill") }.help("Next recording")
                Spacer()
            }
            .padding(.vertical, 8)
            .background(Color(white: 0.08))

            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    HStack(alignment: .firstTextBaseline) {
                        TextField("Name", text: $ui.name, onCommit: { rec.rename(ui.name) })
                            .textFieldStyle(.plain).font(.inter(size: T(22) * UI, weight: .semibold))
                        Spacer()
                        Button { rec.selection = .init(start: 0, end: s.meta.duration) } label: { Label("Trim", systemImage: "scissors") }
                            .help("Select the whole take, then drag the handles in to cut silence or mistakes (or drag on the waveform)")
                            .disabled(rec.selection != nil || s.meta.duration <= 0)
                        if rec.canRestoreUntrimmed {
                            Button("Restore untrimmed") { rec.restoreUntrimmed() }.help("Put back the take as it was before trimming")
                        }
                        Menu {
                            if let u = rec.shareURL(s, amped: true, m4a: true) { ShareLink("Amped, for messages (M4A)", item: u) }
                            if let u = rec.shareURL(s, amped: true) { ShareLink("Amped, full quality (WAV)", item: u) }
                            if let u = rec.shareURL(s, amped: false) { ShareLink("Raw guitar, for re-amping (WAV)", item: u) }
                        } label: { Label("Share", systemImage: "square.and.arrow.up") }
                            .fixedSize().help("Send this take to another app")
                        Button { rec.reveal() } label: { Image(systemName: "folder") }.help("Show in Finder")
                        Button(role: .destructive) { rec.delete() } label: { Image(systemName: "trash") }.help("Move to Trash")
                    }
                    Grid(alignment: .leading, horizontalSpacing: 14, verticalSpacing: 4) {
                        GridRow { Text("Recorded").foregroundStyle(.secondary); Text(DateFormatter.nice.string(from: s.meta.date)) }
                        GridRow { Text("Duration").foregroundStyle(.secondary); Text(mmss(s.meta.duration)) }
                        GridRow { Text("Preset").foregroundStyle(.secondary)
                            Text(s.meta.preset.isEmpty ? "—" : s.meta.preset + (s.meta.source.map { " (\($0))" } ?? "")) }
                    }
                    .font(.inter(size: T(11) * UI))

                    box("Live re-amp", "Play this take's dry guitar through the rig in a loop while you change settings in real time.") {
                        Button(rec.liveReamp ? "Stop live re-amp" : "Live re-amp") {
                            rec.toggleLiveReamp(); if rec.liveReamp { m.screen = .rig }
                        }
                    }
                    box("Re-amp", "Render the dry take through the current rig (“\(m.presetName)”) into a new wet take. The first original is kept as wet-original.wav.") {
                        if let p = rec.rendering { ProgressView(value: p).frame(width: 140) }
                        else { Button("Re-amp with current rig") { rec.reamp() } }
                    }
                    if s.meta.settings != nil {
                        box("Original rig", "Load the exact settings this take was recorded with.") {
                            Button("Load “\(s.meta.preset)” settings") { rec.loadPreset(); m.screen = .rig }
                        }
                    }
                }
                .padding(18)
            }
        }
        .onAppear { ui.name = s.meta.name }
        .onChange(of: s.id) { _ in ui.name = s.meta.name }
    }

    func box<C: View>(_ title: String, _ desc: String, @ViewBuilder _ action: () -> C) -> some View {
        HStack(alignment: .center, spacing: 14) {
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.inter(size: T(12) * UI, weight: .semibold))
                Text(desc).font(.inter(size: T(10) * UI)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
            Spacer()
            action()
        }
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 10).fill(panel))
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(line))
    }

    func step(_ d: Int) {
        guard let i = rec.list.firstIndex(where: { $0.id == rec.selected }) else { rec.selected = rec.list.first?.id; return }
        let j = i + d
        if j >= 0 && j < rec.list.count { rec.selected = rec.list[j].id }
    }
}

func mmssTenths(_ t: Double) -> String {
    let t = max(t, 0)
    return String(format: "%d:%04.1f", Int(t) / 60, t.truncatingRemainder(dividingBy: 60))
}

/// Trim handles over the waveform: the parts that will be cut are dimmed; drag either handle.
struct TrimOverlay: View {
    let duration: Double
    @Binding var start: Double
    @Binding var end: Double
    var body: some View {
        GeometryReader { g in
            let w = g.size.width, h = g.size.height
            let xs = CGFloat(start / max(duration, 0.001)) * w, xe = CGFloat(end / max(duration, 0.001)) * w
            ZStack(alignment: .topLeading) {
                Rectangle().fill(Color.black.opacity(0.6)).frame(width: max(xs, 0), height: h).allowsHitTesting(false)
                Rectangle().fill(Color.black.opacity(0.6)).frame(width: max(w - xe, 0), height: h).offset(x: xe).allowsHitTesting(false)
                handle(h).offset(x: xs - 7)
                    .gesture(DragGesture(minimumDistance: 0, coordinateSpace: .named("trim")).onChanged { v in
                        start = min(max(Double(v.location.x / w) * duration, 0), end - 0.05)
                    })
                handle(h).offset(x: xe - 7)
                    .gesture(DragGesture(minimumDistance: 0, coordinateSpace: .named("trim")).onChanged { v in
                        end = max(min(Double(v.location.x / w) * duration, duration), start + 0.05)
                    })
            }
            .coordinateSpace(name: "trim")
        }
    }
    func handle(_ h: CGFloat) -> some View {
        ZStack {
            Rectangle().fill(Color.yellow).frame(width: 2, height: h)
            RoundedRectangle(cornerRadius: 3).fill(Color.yellow).frame(width: 12, height: 34)
            Image(systemName: "line.3.horizontal").rotationEffect(.degrees(90)).font(.inter(size: 8, weight: .bold)).foregroundStyle(.black)
        }
        .frame(width: 14, height: h)
        .contentShape(Rectangle())
        .onHover { inside in if inside { NSCursor.resizeLeftRight.push() } else { NSCursor.pop() } }
    }
}

/// Whether the press on the waveform has turned into a drag (a selection) rather than a click.
final class DragFlag: ObservableObject { var on = false }

/// Min/max waveform with a seconds ruler. Click to play from that point; drag across it to select a part.
struct WaveformView: View {
    let samples: [Float]
    let duration: Double
    let position: Double?
    let seek: (Double) -> Void
    var select: (Double, Double) -> Void = { _, _ in }
    /// A plain click at time t (the screen clears the selection unless t is inside it).
    var clear: (Double) -> Void = { _ in }
    @StateObject private var drag = DragFlag()

    var body: some View {
        GeometryReader { g in
            let w = g.size.width, h = g.size.height, ruler: CGFloat = 22
            ZStack(alignment: .topLeading) {
                Rectangle().fill(Color(white: 0.09))
                Canvas { ctx, size in
                    // ruler
                    if duration > 0 {
                        let stepS = [1.0, 2, 5, 10, 15, 30, 60, 120].first { w / CGFloat(duration / $0) > 50 } ?? 300
                        var t = 0.0
                        while t <= duration {
                            let x = CGFloat(t / duration) * size.width
                            ctx.fill(Path(CGRect(x: x, y: ruler - 6, width: 1, height: 6)), with: .color(.gray))
                            ctx.draw(Text(mmss(t)).font(.inter(size: 10)).foregroundColor(.gray), at: CGPoint(x: x + 3, y: 8), anchor: .leading)
                            t += stepS
                        }
                    }
                    // wave
                    let n = samples.count / 2
                    guard n > 0 else { return }
                    let mid = ruler + (size.height - ruler) / 2, amp = (size.height - ruler) / 2 - 4
                    var path = Path()
                    for i in 0..<n {
                        let x = CGFloat(i) / CGFloat(n) * size.width
                        let lo = CGFloat(samples[2 * i]), hi = CGFloat(samples[2 * i + 1])
                        path.addRect(CGRect(x: x, y: mid - hi * amp, width: max(size.width / CGFloat(n), 1), height: max((hi - lo) * amp, 1)))
                    }
                    ctx.fill(path, with: .color(Color.accentColor.opacity(0.85)))
                }
                if let position, duration > 0 {
                    Rectangle().fill(Color.white).frame(width: 2, height: h).offset(x: CGFloat(position / duration) * w)
                }
                if samples.isEmpty {
                    Text("loading…").foregroundStyle(.secondary).frame(width: w, height: h)
                }
            }
            .contentShape(Rectangle())
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { v in   // a drag of more than a few points selects from where it started
                    guard duration > 0, drag.on || abs(v.translation.width) > 4 else { return }
                    drag.on = true
                    let a = max(0, min(Double(v.startLocation.x / w), 1)) * duration, b = max(0, min(Double(v.location.x / w), 1)) * duration
                    select(min(a, b), max(min(a, b) + 0.05, max(a, b)))
                }
                .onEnded { v in     // a plain click seeks (and clears a selection it's outside of)
                    if !drag.on { let t = max(0, min(Double(v.location.x / w), 1)) * duration; clear(t); seek(t) }
                    drag.on = false
                })
        }
    }
}

// MARK: - Settings screen

/// Which settings section is showing (remembered between launches).
final class SettingsUI: ObservableObject {
    /// One for the app, so "Click to resolve" on a banner can open a section.
    static let shared = SettingsUI()
    @Published var section = UserDefaults.standard.string(forKey: "settingsSection") ?? "Audio" {
        didSet { UserDefaults.standard.set(section, forKey: "settingsSection") }
    }
    /// Search box text, and the row a search result pointed at (highlighted for a moment, then cleared).
    @Published var query = ""
    @Published var highlight: String?
    func go(_ e: SettingsEntry) {
        section = e.section; query = ""; highlight = e.label
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.5) { [weak self] in if self?.highlight == e.label { self?.highlight = nil } }
    }
}

/// Every settings row, for search (Windows: built from the laid-out rows, same labels). Matches the label, its
/// section and heading, and its hover explanation, so "buffer" also finds Latency.
struct SettingsEntry: Hashable { let section, heading, label, key: String }
let SETTINGS_INDEX: [SettingsEntry] = [
    ("Audio", "Devices", "Input", "inputDevice"), ("Audio", "Devices", "Input channel", "inputChannel"),
    ("Audio", "Devices", "Output", "outputDevice"), ("Audio", "Output EQ", "Output EQ", "settings:outputEQ"), ("Audio", "Speed", "Buffer", "settings:buffer"),
    ("Audio", "Speed", "Latency", "settings:latency"), ("Audio", "Speed", "Engine", "settings:engine"),
    ("Audio", "Amp captures", "Calibrate input", "settings:namCalibrate"), ("Audio", "Amp captures", "Interface level", "settings:interfaceLevel"),
    ("Audio", "Tuner", "Reference", "settings:a4"), ("Audio", "Tuner", "Guitar tuning", "settings:tuning"),
    ("MIDI", "Connection", "Devices", "settings:midiDevices"), ("MIDI", "Connection", "Last message", "settings:midiLast"),
    ("MIDI", "Presets", "Program change", "settings:programChange"), ("MIDI", "Assignments", "Assign", "settings:midiAssign"),
    ("Library", "", "Recordings", "settings:recordings"), ("Library", "", "Recordings folder", "settings:recordingsFolder"),
    ("Library", "", "NAM captures", "settings:nam"), ("Library", "", "Cab IRs", "settings:cabIRs"), ("Library", "", "Your captures", "settings:captures"),
    ("Appearance", "", "Accent colour", "settings:accent"), ("Appearance", "", "Zoom", "settings:zoom"), ("Appearance", "", "Explanations", "settings:explain"),
    ("About", "Updates", "Version", "settings:version"), ("About", "Updates", "Automatic", "settings:autoUpdate"),
    ("About", "Feedback", "GitHub", "settings:feedback"), ("About", "Feedback", "Debug log", "settings:debugLog"),
    ("About", "License", "Leviathan", "settings:license"),
].map { SettingsEntry(section: $0.0, heading: $0.1, label: $0.2, key: $0.3) }

func searchSettings(_ query: String) -> [SettingsEntry] {
    let words = query.lowercased().split(separator: " ").map(String.init)
    guard !words.isEmpty else { return [] }
    func text(_ e: SettingsEntry) -> String { "\(e.label) \(e.heading) \(e.section) \(EXPLAIN[e.key] ?? "")".lowercased() }
    let hits = SETTINGS_INDEX.filter { e in let t = text(e); return words.allSatisfy { t.contains($0) } }
    // label matches first, then the rest in their usual order
    return hits.filter { e in words.allSatisfy { e.label.lowercased().contains($0) } } + hits.filter { e in !words.allSatisfy { e.label.lowercased().contains($0) } }
}

struct SettingsView: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var rec: Sessions
    @EnvironmentObject var updater: Updater
    @ObservedObject private var ui = SettingsUI.shared

    /// The sections, in order (Windows: SettingsScreen::Nav lists the same).
    static let nav: [(name: String, icon: String)] = [
        ("Audio", "speaker.wave.2"), ("MIDI", "pianokeys"), ("Library", "folder"), ("Appearance", "paintpalette"), ("About", "info.circle"),
    ]
    /// Sections from before 0.4.9 and where they live now.
    static func current(_ saved: String) -> String {
        switch saved {
        case "Tuner": return "Audio"
        case "Updates", "Help & feedback": return "About"
        default: return nav.contains { $0.name == saved } ? saved : "Audio"
        }
    }

    /// The section menu sits against the app's sidebar, a quarter of the width; the section fills the rest.
    var body: some View {
        GeometryReader { geo in
            HStack(alignment: .top, spacing: 0) {
                navigation
                    .frame(width: min(max(geo.size.width * 0.25, 220 * Z), 320 * Z))
                    .background(Color(white: 0.07))
                    .overlay(alignment: .trailing) { Rectangle().fill(line).frame(width: 1) }
                ScrollViewReader { scroll in
                    ScrollView {
                        section.font(.inter(size: T(10.5) * UI))   // one body size for labels and controls' text
                            .padding(.horizontal, 36 * Z).padding(.vertical, 28 * Z)
                            .frame(maxWidth: 1000 * Z, alignment: .leading).frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .onChange(of: ui.highlight) { h in if let h { withAnimation { scroll.scrollTo(h, anchor: .center) } } }
                }
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .safeAreaInset(edge: .bottom, spacing: 0) { if m.explainOnHover { HoverHelpStrip(idle: "Hover over a setting for an explanation.") } }
    }

    var navigation: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text("SETTINGS").font(.inter(size: T(10) * UI, weight: .bold)).tracking(2).foregroundStyle(.secondary)
                .padding(.horizontal, 10).padding(.bottom, 10)
            HStack(spacing: 6 * Z) {
                Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                TextField("Search settings", text: $ui.query).textFieldStyle(.plain)
                    .onSubmit { if let first = searchSettings(ui.query).first { ui.go(first) } }
                if !ui.query.isEmpty {
                    Button { ui.query = "" } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(.secondary) }.buttonStyle(.plain)
                }
            }
            .font(.inter(size: T(10.5) * UI))
            .padding(.horizontal, 10).padding(.vertical, 7)
            .background(RoundedRectangle(cornerRadius: 8).fill(Color.white.opacity(0.06)))
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(line))
            .padding(.bottom, 12)
            if !ui.query.isEmpty {
                let hits = searchSettings(ui.query)
                if hits.isEmpty { Text("No settings match.").foregroundStyle(.secondary).padding(.horizontal, 10) }
                ForEach(hits, id: \.self) { e in
                    Button { ui.go(e) } label: {
                        VStack(alignment: .leading, spacing: 1) {
                            Text(e.label).font(.inter(size: T(10.5) * UI))
                            Text(e.heading.isEmpty ? e.section : "\(e.section) \u{203A} \(e.heading)").font(.inter(size: T(9) * UI)).foregroundStyle(.secondary)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.horizontal, 10).padding(.vertical, 5)
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                }
            } else {
            ForEach(SettingsView.nav, id: \.name) { item in
                let on = SettingsView.current(ui.section) == item.name
                Button { ui.section = item.name } label: {
                    HStack(spacing: 8) {
                        Image(systemName: item.icon).frame(width: 18 * Z)
                        Text(item.name).lineLimit(1).fixedSize()
                        Spacer()
                        if item.name == "About" && updater.available != nil { Circle().fill(.red).frame(width: 8, height: 8) }
                    }
                    .font(.inter(size: T(10.5) * UI))
                    .foregroundStyle(on ? Color.accentColor : .primary)
                    .padding(.horizontal, 10).padding(.vertical, 7)
                    .background(RoundedRectangle(cornerRadius: 7).fill(on ? Color.white.opacity(0.08) : .clear))
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
            }
            Spacer()
        }
        .padding(14)
        .frame(maxHeight: .infinity, alignment: .top)
    }

    @ViewBuilder var section: some View {
        switch SettingsView.current(ui.section) {
        case "MIDI": SettingsCard("MIDI") { MidiSettings() }
        case "Library": SettingsCard("Library") { library }
        case "Appearance": SettingsCard("Appearance") { appearance }
        case "About": SettingsCard("About") { UpdateSettings(); FeedbackSettings(); AboutSettings() }
        default: SettingsCard("Audio") { audio }
        }
    }

    // MARK: Audio

    func outputEQRow(_ label: String, _ key: WritableKeyPath<Model.OutputEQ, Double>, _ range: ClosedRange<Double>, _ note: String) -> some View {
        SettingsRow(label, explain: "settings:outputEQ") {
            Slider(value: Binding(get: { m.outputEQ[keyPath: key] }, set: { m.outputEQ[keyPath: key] = ($0 * 2).rounded() / 2 }), in: range)
                .frame(width: 220 * Z)
            Text(String(format: "%+.1f dB", m.outputEQ[keyPath: key])).monospacedDigit().frame(width: 70 * Z, alignment: .trailing)
            if !note.isEmpty { Text(note).foregroundStyle(.secondary) }
            Button("Reset") { m.outputEQ[keyPath: key] = 0 }.buttonStyle(.borderless).disabled(m.outputEQ[keyPath: key] == 0)
        }
    }

    @ViewBuilder var audio: some View {
        SettingsHeading("Devices", first: true)
        SettingsRow("Input", explain: "inputDevice") {
            Picker("", selection: $m.inputUID) {
                Text("No input (playback only)").tag(Model.noInput)
                ForEach(m.inputs) { Text($0.name).tag($0.uid) }
                if m.input == nil && m.inputUID != Model.noInput { Text("Not connected").tag(m.inputUID) }
            }.labelsHidden().frame(width: SettingsLayout.wide, alignment: .leading)
        }
        if (m.input?.ins ?? 1) > 1 {
            SettingsRow("Input channel", explain: "inputChannel") {
                Picker("", selection: $m.s.inputChannel) {
                    ForEach(0..<min(m.input?.ins ?? 1, 8), id: \.self) { Text("Channel \($0 + 1)").tag($0) }
                }.labelsHidden().frame(width: SettingsLayout.narrow, alignment: .leading)
            }
        }
        SettingsRow("Output", explain: "outputDevice") {
            Picker("", selection: $m.outputUID) {
                ForEach(m.outputs) { d in Text(d.outputWarning != nil ? "\(d.name) ⚠︎" : d.name).tag(d.uid) }
            }.labelsHidden().frame(width: SettingsLayout.wide, alignment: .leading)
            if let w = m.output?.outputWarning { Text(w).foregroundStyle(.orange).lineLimit(2) }
        }

        SettingsHeading("Output EQ")
        SettingsRow("Output EQ", explain: "settings:outputEQ") {
            Toggle("Correct \(m.output?.name ?? "this output")", isOn: $m.outputEQ.on)
        }
        if m.outputEQ.on {
            outputEQRow("Low", \.low, -12...12, "100 Hz shelf")
            outputEQRow("Mid", \.mid, -12...12, "1 kHz")
            outputEQRow("High", \.high, -12...12, "6 kHz shelf")
            outputEQRow("Level", \.level, -12...6, "")
        }

        SettingsHeading("Speed")
        SettingsRow("Buffer", explain: "settings:buffer") {
            Picker("", selection: $m.bufferSize) {
                ForEach([16, 32, 64, 128, 256], id: \.self) { n in Text(bufferLabel(n)).tag(n) }
            }.labelsHidden().frame(width: SettingsLayout.wide, alignment: .leading)
        }
        if let i = m.info {
            SettingsRow("Latency", explain: "settings:latency") {
                VStack(alignment: .leading, spacing: 2) {
                    Text(latencySummary(i)).monospacedDigit()
                    Text(String(format: "Input %.1f · buffers %.1f · output %.1f ms · %.1f kHz", i.inMs, i.bufferMs, i.outMs, i.sampleRate / 1000))
                        .font(.inter(size: T(11) * UI)).foregroundStyle(.secondary).monospacedDigit()
                }
            }
            SettingsRow("Engine", explain: "settings:engine") { EngineLoad(mt: m.meters) }
        }

        SettingsHeading("Amp captures")
        SettingsRow("Calibrate input", explain: "settings:namCalibrate") {
            Toggle("Match each capture's input level", isOn: $m.namCalibrate)
        }
        if m.namCalibrate {
            SettingsRow("Interface level", explain: "settings:interfaceLevel") {
                Stepper(value: $m.interfaceLevel, in: -10...30, step: 0.5) { Text(String(format: "%.1f dBu", m.interfaceLevel)).monospacedDigit() }
            }
            SettingsRow("Now", explain: "settings:namCalibrate") {
                VStack(alignment: .leading, spacing: 2) {
                    Text(calibrationLine("Amp", m.s.nam, m.namHost))
                    if m.s.ampBOn && !m.s.ampBNam.isEmpty { Text(calibrationLine("Amp B", m.s.ampBNam, m.namHostB)) }
                }.font(.inter(size: T(11) * UI)).foregroundStyle(.secondary).monospacedDigit()
            }
        }

        SettingsHeading("Tuner")
        SettingsRow("Reference", explain: "settings:a4") {
            Stepper(value: $m.a4, in: 430...450, step: 1) { Text("A4 = \(Int(m.a4)) Hz").monospacedDigit() }
        }
        SettingsRow("Guitar tuning", explain: "settings:tuning") {
            Picker("", selection: $m.tuningIndex) { ForEach(TUNINGS.indices, id: \.self) { Text(TUNINGS[$0].label).tag($0) } }
                .labelsHidden().frame(width: SettingsLayout.wide, alignment: .leading)
        }
    }

    /// "Amp: +3.0 dB (captured at 9.0 dBu)": what input calibration is doing to a loaded capture.
    func calibrationLine(_ label: String, _ name: String, _ h: NamHost) -> String {
        if name.isEmpty { return "\(label): no capture (the built-in amp isn't calibrated)" }
        guard h.loadedName == name, h.model != nil else { return "\(label): loads when audio starts" }
        guard let lvl = h.inputLevel else { return "\(label): this capture has no input level, so it's left as it is" }
        return String(format: "%@: %+.1f dB (captured at %.1f dBu)", label, h.inputTrim, lvl)
    }

    /// "64 samples · ~9.8 ms": the round trip this buffer size would give with the current devices.
    func bufferLabel(_ n: Int) -> String {
        guard let i = m.info, i.buffer > 0 else { return "\(n) samples" }
        let ms = i.inMs + i.outMs + i.bufferMs * Double(n) / Double(i.buffer)
        return String(format: "%d samples · ~%.1f ms", n, ms)
    }

    /// "About 21.4 ms from string to ear, mostly the output device"
    func latencySummary(_ i: HALEngine.Info) -> String {
        let parts = [("your interface's input", i.inMs), ("the buffers", i.bufferMs), ("the output device", i.outMs)]
        let big = parts.max { $0.1 < $1.1 }!
        let most = big.1 > i.latencyMs * 0.5 ? ", mostly \(big.0)" : ""
        return String(format: "About %.1f ms from string to ear%@", i.latencyMs, most)
    }

    // MARK: Library

    /// Where takes are recorded: any folder, a cloud drive's included (Google Drive, Dropbox, iCloud Drive).
    func chooseRecordingsFolder() {
        let p = NSOpenPanel()
        p.canChooseDirectories = true; p.canChooseFiles = false; p.canCreateDirectories = true; p.allowsMultipleSelection = false
        p.directoryURL = Sessions.root
        p.message = "Choose where to keep your recordings (any folder, Google Drive or Dropbox included)"
        p.prompt = "Use This Folder"
        if p.runModal() == .OK, let u = p.url { askMoveRecordings(to: u) }
    }

    /// Asks whether to bring the existing takes along, then switches folders (Sessions.changeRoot).
    func askMoveRecordings(to: URL?) {
        let target = to ?? Sessions.defaultRoot
        guard target.standardizedFileURL != Sessions.root.standardizedFileURL else { return }
        let n = rec.countTakes
        guard n > 0 else { rec.changeRoot(to: to, moveExisting: false); return }
        let a = NSAlert()
        a.messageText = "Move your \(n) recording\(n == 1 ? "" : "s")?"
        a.informativeText = "New recordings will go to \(target.path).\n\nMoving copies each take there, checks the copy, then removes it from \(Sessions.root.path) (and that folder once it's empty). Takes you leave behind stay where they are, but Leviathan only lists the new folder."
        a.addButton(withTitle: "Move Recordings"); a.addButton(withTitle: "Leave Them"); a.addButton(withTitle: "Cancel")
        switch a.runModal() {
        case .alertFirstButtonReturn: rec.changeRoot(to: to, moveExisting: true)
        case .alertSecondButtonReturn: rec.changeRoot(to: to, moveExisting: false)
        default: break
        }
    }

    @ViewBuilder var library: some View {
        SettingsRow("Recordings", explain: "settings:recordings") {
            Button("Import WAV\u{2026}") { rec.importRecordings(); m.screen = .recordings }
            Button("Open folder") { NSWorkspace.shared.open(Sessions.root) }
        }
        SettingsRow("Recordings folder", explain: "settings:recordingsFolder") {
            Button("Change folder\u{2026}") { chooseRecordingsFolder() }.disabled(rec.moving || rec.recording)
            if Sessions.chosen != nil { Button("Use default") { askMoveRecordings(to: nil) }.disabled(rec.moving || rec.recording) }
            Text(Sessions.chosenMissing ? "\(Sessions.chosen?.path ?? "") (not available: using the default for now)" : Sessions.root.path)
                .foregroundStyle(Sessions.chosenMissing ? Color.orange : Color.secondary).lineLimit(1).truncationMode(.middle)
        }
        SettingsRow("NAM captures", explain: "settings:nam") {
            Button("Add\u{2026}") { m.importNam() }
            Button("Open folder") { NSWorkspace.shared.open(NamHost.dir) }
            Button("Rescan") { m.rescanNam() }
            SettingsCount("\(m.namHost.files.count) loaded")
        }
        SettingsRow("Cab IRs", explain: "settings:cabIRs") {
            Button("Add\u{2026}") { m.importCabIRs() }
            Button("Open folder") { NSWorkspace.shared.open(CabIRLibrary.root) }
            Button("Rescan") { m.cabIRs.scan(); m.objectWillChange.send() }
            SettingsCount("\(m.cabIRs.files.count) in the library")
        }
        SettingsRow("Your captures", explain: "settings:captures") {
            Button("Open folder") {
                try? FileManager.default.createDirectory(at: Capture.dir, withIntermediateDirectories: true)
                NSWorkspace.shared.open(Capture.dir)
            }
            Button("Rescan") { m.reloadCaptures() }
            SettingsCount("\(m.captures.count) loaded")
        }
    }

    // MARK: Appearance

    @ViewBuilder var appearance: some View {
        SettingsRow("Accent colour", explain: "settings:accent") {
            ColorPicker("", selection: m.accentBinding, supportsOpacity: false).labelsHidden()
            ForEach(Array(accentSwatches.enumerated()), id: \.offset) { _, c in
                Button { m.accentRGB = c } label: {
                    Circle().fill(Color(red: c[0], green: c[1], blue: c[2])).frame(width: 22 * Z, height: 22 * Z)
                        .overlay(Circle().stroke(Color.white.opacity(m.accentRGB == c ? 0.9 : 0), lineWidth: 2))
                }.buttonStyle(.plain)
            }
            Button("Follow amp channel") { m.accentRGB = nil }.disabled(m.accentRGB == nil)
        }
        SettingsRow("Zoom", explain: "settings:zoom") {
            Button { m.zoomStep(-1) } label: { Image(systemName: "minus") }
            Text("\(Int((m.zoom * 100).rounded()))%").monospacedDigit().frame(width: 56 * Z)
            Button { m.zoomStep(1) } label: { Image(systemName: "plus") }
        }
        SettingsRow("Explanations", explain: "settings:explain") {
            Toggle("Explain controls on hover", isOn: $m.explainOnHover)
        }
    }
}

// MARK: - Settings layout: one card per section, sub-headings, and rows on a shared grid

/// A section: its title, then rows. Every section uses the same card, spacing and label column.
struct SettingsCard<C: View>: View {
    let title: String
    @ViewBuilder let content: C
    init(_ title: String, @ViewBuilder content: () -> C) { self.title = title; self.content = content() }
    var body: some View {
        VStack(alignment: .leading, spacing: 10 * Z) {
            Text(title).font(.inter(size: T(22) * UI, weight: .semibold)).padding(.bottom, 8 * Z)
            content
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

/// A small heading inside a section ("DEVICES"), with a rule above it except at the top.
struct SettingsHeading: View {
    let text: String, first: Bool
    init(_ text: String, first: Bool = false) { self.text = text; self.first = first }
    var body: some View {
        VStack(alignment: .leading, spacing: 8 * Z) {
            if !first { Rectangle().fill(line).frame(height: 1).padding(.top, 8 * Z) }
            Text(text.uppercased()).font(.inter(size: T(9) * UI, weight: .semibold)).tracking(1.2).foregroundStyle(.secondary)
        }
    }
}

/// Label in a fixed left column, controls from the same x on every row, one minimum height; hovering anywhere on
/// the row explains it in the strip at the bottom.
enum SettingsLayout {
    static var labelWidth: CGFloat { 170 * Z }   // the label column
    static var wide: CGFloat { 320 * Z }         // device and tuning pickers
    static var narrow: CGFloat { 180 * Z }       // channel picker
}

struct SettingsRow<C: View>: View {
    @ObservedObject private var ui = SettingsUI.shared
    let label: String, key: String?
    @ViewBuilder let content: C
    init(_ label: String, explain key: String? = nil, @ViewBuilder content: () -> C) { self.label = label; self.key = key; self.content = content() }
    var body: some View {
        HStack(alignment: .center, spacing: 10 * Z) {
            Text(label).foregroundStyle(.secondary).frame(width: SettingsLayout.labelWidth, alignment: .leading)
            content
            Spacer(minLength: 0)
        }
        .frame(minHeight: 30 * Z)
        .padding(.horizontal, 8 * Z).padding(.vertical, 2 * Z)
        .background(RoundedRectangle(cornerRadius: 7).fill(Color.accentColor.opacity(ui.highlight == label && !label.isEmpty ? 0.18 : 0)))
        .animation(.easeOut(duration: 0.3), value: ui.highlight)
        .padding(.horizontal, -8 * Z)
        .id(label)
        .contentShape(Rectangle())
        .onHover { inside in
            guard let key else { return }
            if inside { HoverHelp.shared.show(id: "settings·" + label, title: label, text: EXPLAIN[key]) } else { HoverHelp.shared.clear(id: "settings·" + label) }
        }
    }
}

/// A count after a row's buttons ("34 loaded").
struct SettingsCount: View {
    let text: String
    init(_ text: String) { self.text = text }
    var body: some View { Text(text).foregroundStyle(.secondary).monospacedDigit().padding(.leading, 4 * Z) }
}

// MARK: - Grid layout: panels snap to a 48-column grid so edges line up across rows

/// Collapsed panels show only their header, but keep their expanded width and (while the row has an open panel)
/// their expanded height in the grid, so collapsing never moves anything else.
struct FoldedKey: LayoutValueKey { static let defaultValue = false }
/// A zero-size item that starts a new row ("|" in the panel order).
struct RowBreakKey: LayoutValueKey { static let defaultValue = false }

/// Each item spans as many columns as its natural width needs; items wrap to a new row when the columns run out.
/// Expanded items in a row share the row's height, so tops and bottoms line up too.
/// Item sizes are always the expanded ones (a folded UnitBox keeps its content in the layout, hidden), so a collapse
/// changes neither the column spans nor, unless the whole row folds, the row height.
struct GridFlow: Layout {
    var columns = 48   // fine enough that rounding each panel up to whole columns wastes little width
    var spacing: CGFloat = 12
    /// Spread each row's spare columns over its panels so every row ends at the same right edge.
    var fill = false

    private struct Item { let index: Int, col: Int, span: Int, size: CGSize }

    private func unit(_ width: CGFloat) -> CGFloat { max((width - spacing * CGFloat(columns - 1)) / CGFloat(columns), 1) }

    private func rows(_ width: CGFloat, _ subviews: Subviews) -> [[Item]] {
        let u = unit(width)
        var rows: [[Item]] = [[]], col = 0
        for (i, v) in subviews.enumerated() {
            if v[RowBreakKey.self] { if col > 0 { rows.append([]); col = 0 }; continue }
            let s = v.sizeThatFits(.unspecified)
            let span = min(columns, max(1, Int(((s.width + spacing) / (u + spacing) - 0.001).rounded(.up))))
            if col > 0 && col + span > columns { rows.append([]); col = 0 }
            rows[rows.count - 1].append(Item(index: i, col: col, span: span, size: s))
            col += span
        }
        rows = rows.filter { !$0.isEmpty }
        guard fill else { return rows }
        return rows.map { row in
            let used = row.reduce(0) { $0 + $1.span }, spare = columns - used
            guard spare > 0 else { return row }
            var out: [Item] = [], col = 0, given = 0
            for (k, it) in row.enumerated() {
                let extra = k == row.count - 1 ? spare - given : spare * it.span / used
                given += extra
                out.append(Item(index: it.index, col: col, span: it.span + extra, size: it.size))
                col += it.span + extra
            }
            return out
        }
    }

    /// Height of a folded panel: its header, from a zero-height proposal.
    private func foldedHeight(_ v: LayoutSubview) -> CGFloat { v.sizeThatFits(ProposedViewSize(width: nil, height: 0)).height }

    /// While any panel in the row is open the row keeps every panel's expanded height; a fully folded row shrinks to its headers.
    private func rowHeight(_ row: [Item], _ subviews: Subviews) -> CGFloat {
        if row.contains(where: { !subviews[$0.index][FoldedKey.self] }) { return row.map(\.size.height).max() ?? 0 }
        return row.map { foldedHeight(subviews[$0.index]) }.max() ?? 0
    }

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let width = proposal.width ?? 1200
        let r = rows(width, subviews)
        let h = r.reduce(0) { $0 + rowHeight($1, subviews) } + spacing * CGFloat(max(r.count - 1, 0))
        return CGSize(width: width, height: h)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        let u = unit(bounds.width)
        for v in subviews where v[RowBreakKey.self] { v.place(at: bounds.origin, proposal: .zero) }
        var y = bounds.minY
        for row in rows(bounds.width, subviews) {
            let rh = rowHeight(row, subviews)
            for it in row {
                let w = CGFloat(it.span) * u + CGFloat(it.span - 1) * spacing
                let h = subviews[it.index][FoldedKey.self] ? foldedHeight(subviews[it.index]) : rh
                subviews[it.index].place(at: CGPoint(x: bounds.minX + CGFloat(it.col) * (u + spacing), y: y),
                                         proposal: ProposedViewSize(width: w, height: h))
            }
            y += rh + spacing
        }
    }
}

// MARK: - Flow layout: units wrap onto new rows instead of squashing

struct FlowLayout: Layout {
    var spacing: CGFloat = 12
    /// stretch each row to the full width; false packs items left at their natural width
    var stretch = true

    private func rows(_ maxW: CGFloat, _ subviews: Subviews) -> [[(Int, CGSize)]] {
        var rows: [[(Int, CGSize)]] = [[]]
        var x: CGFloat = 0
        for (i, v) in subviews.enumerated() {
            let s = v.sizeThatFits(.unspecified)
            if x > 0 && x + s.width > maxW { rows.append([]); x = 0 }
            rows[rows.count - 1].append((i, s))
            x += s.width + spacing
        }
        return rows
    }

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let maxW = proposal.width ?? .infinity
        let r = rows(maxW, subviews)
        var h: CGFloat = spacing * CGFloat(max(r.count - 1, 0)), w: CGFloat = 0
        for row in r {
            var rowW: CGFloat = spacing * CGFloat(max(row.count - 1, 0)), rowH: CGFloat = 0
            for item in row { rowW += item.1.width; rowH = max(rowH, item.1.height) }
            h += rowH; w = max(w, rowW)
        }
        // take the whole offered width, so rows always span the window (and stretch to fill)
        return CGSize(width: proposal.width ?? w, height: h)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var y = bounds.minY
        for row in rows(bounds.width, subviews) {
            let rh = row.map(\.1.height).max() ?? 0
            // stretch the row to the full width so panels line up edge to edge
            let natural = row.reduce(0) { $0 + $1.1.width } + spacing * CGFloat(max(row.count - 1, 0))
            let extra = stretch && row.count > 0 ? max(0, bounds.width - natural) / CGFloat(row.count) : 0
            var x = bounds.minX
            for (i, s) in row {
                let w = s.width + extra
                subviews[i].place(at: CGPoint(x: x, y: y), proposal: ProposedViewSize(width: w, height: stretch ? rh : s.height))
                x += w + spacing
            }
            y += rh + spacing
        }
    }
}

final class PresetsUI: ObservableObject {
    @Published var favOnly = false; @Published var renaming: String? = nil; @Published var newName = ""
    @Published var query = ""; @Published var tagFilter: Set<String> = []
    @Published var tagging: String? = nil; @Published var tagText = ""
}
final class RecordingsUI: ObservableObject {
    @Published var name = ""
    /// Trim mode: the kept range in seconds.
}


struct LatencyLabel: View {
    let info: HALEngine.Info
    @ObservedObject var mt: Meters
    var body: some View {
        let total = info.latencyMs + Double(mt.pitchLatency)
        Text(String(format: "~%.1f ms", total)).font(.inter(size: T(10) * UI).monospacedDigit())
            .foregroundStyle(total > 15 ? Color.orange : Color.secondary)
            .help(info.breakdown + (mt.pitchLatency > 0 ? String(format: " + pitch %.0f ms", mt.pitchLatency) : ""))
    }
}

struct EngineLoad: View {
    @ObservedObject var mt: Meters
    var body: some View {
        Text(String(format: "DSP peak %.0f%% of each buffer · %d dropout%@", mt.load * 100, mt.dropouts, mt.dropouts == 1 ? "" : "s"))
            .foregroundStyle(mt.dropouts > 0 || mt.load > 0.6 ? Color.orange : Color.secondary).monospacedDigit()
    }
}

let accentSwatches: [[Double]] = [[1, 0.3, 0.3], [0.91, 0.57, 0.23], [0.95, 0.8, 0.25], [0.35, 0.8, 0.45],
                                  [0.35, 0.7, 1], [0.6, 0.45, 1], [1, 0.45, 0.75], [0.85, 0.85, 0.85]]

/// Remembers the main window's size and position between launches. SwiftUI's own autosave key is built from the
/// view hierarchy's type name, so it changes with every code change (and every update); this one is fixed.
struct WindowFrameSaver: NSViewRepresentable {
    static let key = "windowFrame"

    final class Coordinator {
        var observers: [NSObjectProtocol] = []
        deinit { observers.forEach(NotificationCenter.default.removeObserver) }
    }
    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeNSView(context: Context) -> NSView {
        let v = NSView()
        DispatchQueue.main.async {
            guard let w = v.window, context.coordinator.observers.isEmpty else { return }
            if let str = UserDefaults.standard.string(forKey: WindowFrameSaver.key) {
                let f = NSRectFromString(str)
                // only if it still lands on a connected screen
                if f.width >= 900, f.height >= 600, NSScreen.screens.contains(where: { $0.visibleFrame.intersects(f) }) {
                    w.setFrame(f, display: true)
                }
            }
            let save = { (_: Notification) in UserDefaults.standard.set(NSStringFromRect(w.frame), forKey: WindowFrameSaver.key) }
            for n in [NSWindow.didEndLiveResizeNotification, NSWindow.didMoveNotification, NSWindow.didResizeNotification, NSWindow.willCloseNotification] {
                context.coordinator.observers.append(NotificationCenter.default.addObserver(forName: n, object: w, queue: .main, using: save))
            }
        }
        return v
    }
    func updateNSView(_ nsView: NSView, context: Context) {}
}

/// "A new version is available" across the top of the window, until you update or close it.
struct UpdateBanner: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var updater: Updater
    let release: Updater.Release
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "arrow.down.circle.fill").foregroundStyle(Color.accentColor)
            Text("Leviathan \(release.version) is available (you have \(updater.current)).").font(.inter(size: T(12) * UI))
            Spacer()
            Button("What\u{2019}s new") { updater.showNotes = true }
            Button("Update now") { updater.install(release) }.buttonStyle(.borderedProminent)
            Button { updater.dismissedVersion = release.version } label: { Image(systemName: "xmark") }.buttonStyle(.borderless).help("Hide until the next version")
        }
        .padding(10)
        .background(Color.accentColor.opacity(0.15))
    }
}


/// While an update downloads or installs, and if it fails: the update banner's place, so progress shows on every screen.
struct UpdateProgressBanner: View {
    @EnvironmentObject var updater: Updater
    @EnvironmentObject var m: Model
    var body: some View {
        switch updater.phase {
        case .downloading(let f):
            row { Text("Downloading the update\u{2026}"); ProgressView(value: f).frame(width: 160); Text("\(Int(f * 100))%").monospacedDigit().foregroundStyle(.secondary) }
        case .installing:
            row { Text("Installing the update. Leviathan will reopen in a moment\u{2026}"); ProgressView().controlSize(.small) }
        case .failed(let msg):
            row {
                Text(msg).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
                Spacer()
                Button("Click to resolve") { m.resolve("About") }
                Button { updater.phase = .idle } label: { Image(systemName: "xmark") }.buttonStyle(.borderless)
            }
        default: EmptyView()
        }
    }
    private func row<C: View>(@ViewBuilder _ c: () -> C) -> some View {
        HStack(spacing: 12) { Image(systemName: "arrow.down.circle.fill").foregroundStyle(Color.accentColor); c(); Spacer(minLength: 0) }
            .font(.inter(size: T(12) * UI)).padding(10).background(Color.accentColor.opacity(0.15))
    }
}

