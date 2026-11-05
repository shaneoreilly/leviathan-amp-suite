import SwiftUI
import AppKit

// MARK: - Root: sidebar + top bar + screen

struct RootView: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var rec: Sessions
    @EnvironmentObject var midi: Midi

    var ch: Color { m.accent }

    var body: some View {
        HStack(spacing: 0) {
            Sidebar()
            Rectangle().fill(line).frame(width: 1)
            VStack(spacing: 0) {
                TopBar()
                ForEach([m.status, rec.message].filter { !$0.isEmpty }, id: \.self) { msg in
                    HStack {
                        Text(msg).font(.system(size: 12 * UI))
                        Spacer()
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
        .font(.system(size: 13 * UI))
        .controlSize(.large)
        .background(Color(white: 0.05))
        .tint(ch)
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

    var body: some View {
        VStack(spacing: 4) {
            item(.rig, "slider.vertical.3", "Rig")
            item(.presets, "list.bullet", "Presets")
            item(.recordings, "waveform", "Recordings", badge: rec.recording)
            Spacer()
            zoomControl
            item(.settings, "gearshape", "Settings")
        }
        .padding(.vertical, 10)
        .frame(width: 92 * Z)
        .background(Color(white: 0.07))
    }

    var zoomControl: some View {
        VStack(spacing: 2) {
            HStack(spacing: 2) {
                Button { m.zoomStep(-1) } label: { Image(systemName: "minus").frame(width: 30 * Z, height: 26 * Z) }
                    .keyboardShortcut("-", modifiers: .command).help("Zoom out (⌘−)")
                    .disabled(m.zoom <= Model.zoomSteps.first! + 0.001)
                Button { m.zoomStep(1) } label: { Image(systemName: "plus").frame(width: 30 * Z, height: 26 * Z) }
                    .keyboardShortcut("=", modifiers: .command).help("Zoom in (⌘+)")
                    .disabled(m.zoom >= Model.zoomSteps.last! - 0.001)
            }
            .buttonStyle(.plain)
            .font(.system(size: 13 * Z, weight: .semibold))
            .background(RoundedRectangle(cornerRadius: 7).fill(Color.white.opacity(0.06)))
            Button { m.zoom = 1 } label: {
                Text("\(Int((m.zoom * 100).rounded()))%").font(.system(size: 9 * UI).monospacedDigit())
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
                Image(systemName: icon).font(.system(size: 20 * UI * 0.8))
                    .overlay(alignment: .topTrailing) { if badge { Circle().fill(.red).frame(width: 9, height: 9).offset(x: 6, y: -4) } }
                Text(label).font(.system(size: 9 * UI, weight: .semibold))
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
                Text("LEVIATHAN").font(.system(size: 15 * UI, weight: .heavy)).tracking(4)
                Text("OPEN GUITAR RIG").font(.system(size: 9 * UI)).tracking(1.5).foregroundStyle(.secondary)
            }
            .fixedSize()
            Button(m.running ? "⏻ On" : "⏻ Start") { m.toggle() }
                .buttonStyle(.borderedProminent).tint(m.running ? .green : ch).keyboardShortcut(.space, modifiers: [])
            LevelMeters(mt: m.meters)
            if let i = m.info { LatencyLabel(info: i, mt: m.meters) }
            Spacer(minLength: 8)
            Button { rec.toggleRecord() } label: {
                HStack(spacing: 6) {
                    Circle().fill(rec.recording ? Color.red : Color(white: 0.5)).frame(width: 12, height: 12)
                    Text(rec.recording ? mmss(rec.recSeconds) : "Rec").monospacedDigit()
                }
            }
            .keyboardShortcut("r", modifiers: [.command])
            .help("Record dry + wet (⌘R)")
            Toggle("Tuner", isOn: $m.tunerOn).toggleStyle(.button).keyboardShortcut("t", modifiers: [])
            MidiLearnButton()
            PresetMenu().frame(maxWidth: 300 * W)
            Button("Save") { m.saveCurrent() }.keyboardShortcut("s").disabled(!m.presetModified).help("Save changes to this preset (⌘S)")
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
            if !m.favorites.isEmpty {
                Section("Favorites") { ForEach(m.favorites.sorted(), id: \.self) { n in Button("★ \(n)") { m.loadAny(n) } } }
            }
            Section("Factory") {
                ForEach(FACTORY.map(\.0), id: \.self) { n in Button(m.userPresets[n] != nil ? "\(n) (edited)" : n) { m.loadFactory(n) } }
            }
            let mine = m.userPresets.keys.filter { !m.isFactory($0) }.sorted()
            if !mine.isEmpty { Section("Yours") { ForEach(mine, id: \.self) { n in Button(n) { m.loadUser(n) } } } }
            Divider()
            Button("Save “\(m.presetName)”") { m.saveCurrent() }
            Button("Save as new…") { m.newPresetName = ""; m.savingPreset = true }
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

    var names: [String] {
        let all = FACTORY.map(\.0) + m.userPresets.keys.filter { !m.isFactory($0) }.sorted()
        return ui.favOnly ? all.filter(m.favorites.contains) : all
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Text("PRESETS").font(.system(size: 10 * UI, weight: .bold)).tracking(2).foregroundStyle(.secondary)
                Picker("", selection: $ui.favOnly) { Text("All").tag(false); Text("★ Favorites").tag(true) }
                    .pickerStyle(.segmented).labelsHidden().frame(width: 260 * Z)
                Spacer()
                Button("Save as new…") { m.newPresetName = ""; m.savingPreset = true }
            }
            .padding(14)
            List {
                ForEach(names, id: \.self) { n in row(n) }
            }
            .listStyle(.inset)
            .scrollContentBackground(.hidden)
        }
        .alert("Rename preset", isPresented: Binding(get: { ui.renaming != nil }, set: { if !$0 { ui.renaming = nil } })) {
            TextField("Name", text: $ui.newName)
            Button("Rename") { if let r = ui.renaming { m.renamePreset(r, to: ui.newName) }; ui.renaming = nil }
            Button("Cancel", role: .cancel) { ui.renaming = nil }
        }
    }

    func row(_ n: String) -> some View {
        let current = n == m.presetName, factory = m.isFactory(n), edited = factory && m.userPresets[n] != nil
        return HStack(spacing: 12) {
            Button { m.toggleFavorite(n) } label: { Image(systemName: m.favorites.contains(n) ? "star.fill" : "star") }
                .buttonStyle(.borderless).foregroundStyle(m.favorites.contains(n) ? Color.yellow : .secondary)
            VStack(alignment: .leading, spacing: 2) {
                Text(n).font(.system(size: 14 * UI, weight: current ? .bold : .regular))
                Text(factory ? (edited ? "Factory · edited" : "Factory") : "Yours").font(.system(size: 10 * UI)).foregroundStyle(.secondary)
            }
            Spacer()
            if current { Text(m.presetModified ? "Loaded · unsaved changes" : "Loaded").font(.system(size: 10 * UI)).foregroundStyle(.secondary) }
            Button("Load") { m.loadAny(n); m.screen = .rig }
            if !factory {
                Button("Rename") { ui.newName = n; ui.renaming = n }
                Button("Delete", role: .destructive) { m.deletePreset(n) }
            } else if edited {
                Button("Restore factory") { m.restoreFactory(n) }
            }
        }
        .padding(.vertical, 6)
        .contentShape(Rectangle())
        .onTapGesture(count: 2) { m.loadAny(n); m.screen = .rig }
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
                    Text(rec.list.isEmpty ? "No recordings yet" : "Select a recording").font(.system(size: 16 * UI))
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
                        Text(s.meta.name).font(.system(size: 13 * UI)).lineLimit(1)
                        Text(DateFormatter.nice.string(from: s.meta.date)).font(.system(size: 9 * UI)).foregroundStyle(.secondary)
                    }
                    Spacer()
                    Text(mmss(s.meta.duration)).font(.system(size: 11 * UI).monospacedDigit()).foregroundStyle(.secondary)
                }
                .padding(.vertical, 3)
                .tag(s.id)
            }
        }
        .listStyle(.sidebar)
    }

    func detail(_ s: Session) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            WaveformView(samples: rec.waveform, duration: s.meta.duration, position: rec.playing ? rec.playPos : nil) { rec.seek($0) }
                .frame(height: 220 * Z)
            HStack(spacing: 10) {
                Spacer()
                Button { step(-1) } label: { Image(systemName: "backward.end.fill") }.help("Previous recording")
                Button { rec.togglePlay() } label: { Image(systemName: rec.playing ? "pause.fill" : "play.fill").frame(width: 30) }
                    .keyboardShortcut(.return, modifiers: [])
                Button { step(1) } label: { Image(systemName: "forward.end.fill") }.help("Next recording")
                Spacer()
            }
            .padding(.vertical, 8)
            .background(Color(white: 0.08))

            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    HStack(alignment: .firstTextBaseline) {
                        TextField("Name", text: $ui.name, onCommit: { rec.rename(ui.name) })
                            .textFieldStyle(.plain).font(.system(size: 22 * UI, weight: .semibold))
                        Spacer()
                        Button { rec.reveal() } label: { Image(systemName: "folder") }.help("Show in Finder")
                        Button(role: .destructive) { rec.delete() } label: { Image(systemName: "trash") }.help("Move to Trash")
                    }
                    Grid(alignment: .leading, horizontalSpacing: 14, verticalSpacing: 4) {
                        GridRow { Text("Recorded").foregroundStyle(.secondary); Text(DateFormatter.nice.string(from: s.meta.date)) }
                        GridRow { Text("Duration").foregroundStyle(.secondary); Text(mmss(s.meta.duration)) }
                        GridRow { Text("Preset").foregroundStyle(.secondary)
                            Text(s.meta.preset.isEmpty ? "—" : s.meta.preset + (s.meta.source.map { " (\($0))" } ?? "")) }
                    }
                    .font(.system(size: 11 * UI))

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
                Text(title).font(.system(size: 12 * UI, weight: .semibold))
                Text(desc).font(.system(size: 10 * UI)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
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

/// Min/max waveform with a seconds ruler; click to play from that point.
struct WaveformView: View {
    let samples: [Float]
    let duration: Double
    let position: Double?
    let seek: (Double) -> Void

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
                            ctx.draw(Text(mmss(t)).font(.system(size: 10)).foregroundColor(.gray), at: CGPoint(x: x + 3, y: 8), anchor: .leading)
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
            .gesture(DragGesture(minimumDistance: 0).onEnded { v in
                seek(max(0, min(Double(v.location.x / w), 1)) * duration)
            })
        }
    }
}

// MARK: - Settings screen

struct SettingsView: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var rec: Sessions

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                Text("SETTINGS").font(.system(size: 10 * UI, weight: .bold)).tracking(2).foregroundStyle(.secondary)
                group("Audio") {
                    row("Input") {
                        Picker("", selection: $m.inputUID) { ForEach(m.inputs) { Text($0.name).tag($0.uid) } }.labelsHidden().frame(width: 320 * Z)
                        if (m.input?.ins ?? 1) > 1 {
                            Picker("", selection: $m.s.inputChannel) {
                                ForEach(0..<min(m.input?.ins ?? 1, 8), id: \.self) { Text("Channel \($0 + 1)").tag($0) }
                            }.labelsHidden().frame(width: 160 * Z)
                        }
                    }
                    row("Output") {
                        Picker("", selection: $m.outputUID) {
                            ForEach(m.outputs) { d in
                                Text(d.outputWarning != nil ? "\(d.name) ⚠︎ slow" : String(format: "%@ · %.1f ms", d.name, d.outLatencyMs)).tag(d.uid)
                            }
                        }.labelsHidden().frame(width: 320 * Z)
                        if let w = m.output?.outputWarning { Text(w).foregroundStyle(.orange) }
                    }
                    row("Buffer") {
                        Picker("", selection: $m.bufferSize) { ForEach([16, 32, 64, 128, 256], id: \.self) { Text("\($0) samples").tag($0) } }
                            .labelsHidden().frame(width: 200 * Z)
                        if let i = m.info {
                            Text(String(format: "%.1f kHz · %d samples", i.sampleRate / 1000, i.buffer)).foregroundStyle(.secondary).monospacedDigit()
                        }
                    }
                    if let i = m.info {
                        row("Latency") {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(String(format: "~%.1f ms round trip", i.latencyMs)).monospacedDigit()
                                Text(i.breakdown).foregroundStyle(.secondary).monospacedDigit()
                                Text("Smallest buffer that never crackles is best. Most of the delay is usually the output device: wired headphones or your interface's own output are fastest; Bluetooth, monitor speakers and EQ apps (eqMac) are slowest.")
                                    .font(.system(size: 11 * UI)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true).frame(maxWidth: 560, alignment: .leading)
                            }
                        }
                        row("Engine") { EngineLoad(mt: m.meters) }
                    }
                }
                group("Appearance") {
                    row("Accent colour") {
                        ColorPicker("", selection: m.accentBinding, supportsOpacity: false).labelsHidden()
                        ForEach(Array(accentSwatches.enumerated()), id: \.offset) { _, c in
                            Button { m.accentRGB = c } label: {
                                Circle().fill(Color(red: c[0], green: c[1], blue: c[2])).frame(width: 22 * Z, height: 22 * Z)
                                    .overlay(Circle().stroke(Color.white.opacity(m.accentRGB == c ? 0.9 : 0), lineWidth: 2))
                            }.buttonStyle(.plain)
                        }
                        Button("Follow amp channel") { m.accentRGB = nil }.disabled(m.accentRGB == nil)
                    }
                    row("Zoom") {
                        Button { m.zoomStep(-1) } label: { Image(systemName: "minus") }
                        Text("\(Int((m.zoom * 100).rounded()))%").monospacedDigit().frame(width: 60 * Z)
                        Button { m.zoomStep(1) } label: { Image(systemName: "plus") }
                    }
                }
                group("Tuner") {
                    row("Reference") { Stepper(value: $m.a4, in: 430...450, step: 1) { Text("A4 = \(Int(m.a4)) Hz").monospacedDigit() } }
                    row("Guitar tuning") {
                        Picker("", selection: $m.tuningIndex) { ForEach(TUNINGS.indices, id: \.self) { Text(TUNINGS[$0].name).tag($0) } }
                            .labelsHidden().frame(width: 220 * Z)
                    }
                }
                group("MIDI") {
                    MidiSettings(row: { label, c in AnyView(row(label) { c }) })
                }
                group("Library") {
                    row("Recordings") {
                        Button("Import recordings (WAV)…") { rec.importRecordings(); m.screen = .recordings }
                        Button("Open folder") { NSWorkspace.shared.open(Sessions.root) }
                    }
                    row("Captures") {
                        Button("Open folder") {
                            try? FileManager.default.createDirectory(at: Capture.dir, withIntermediateDirectories: true)
                            NSWorkspace.shared.open(Capture.dir)
                        }
                        Button("Rescan") { m.reloadCaptures() }
                        Text("\(m.captures.count) loaded").foregroundStyle(.secondary)
                    }
                }
            }
            .padding(24)
            .frame(maxWidth: 900, alignment: .leading)
        }
    }

    func group<C: View>(_ title: String, @ViewBuilder _ c: () -> C) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title).font(.system(size: 14 * UI, weight: .semibold))
            c()
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 12).fill(panel))
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(line))
    }

    func row<C: View>(_ label: String, @ViewBuilder _ c: () -> C) -> some View {
        HStack(spacing: 12) {
            Text(label).foregroundStyle(.secondary).frame(width: 160 * Z, alignment: .leading)
            c()
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
                subviews[i].place(at: CGPoint(x: x, y: y), proposal: ProposedViewSize(width: w, height: rh))
                x += w + spacing
            }
            y += rh + spacing
        }
    }
}

final class PresetsUI: ObservableObject { @Published var favOnly = false; @Published var renaming: String? = nil; @Published var newName = "" }
final class RecordingsUI: ObservableObject { @Published var name = "" }


struct LatencyLabel: View {
    let info: HALEngine.Info
    @ObservedObject var mt: Meters
    var body: some View {
        let total = info.latencyMs + Double(mt.pitchLatency)
        Text(String(format: "~%.1f ms", total)).font(.system(size: 10 * UI).monospacedDigit())
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
