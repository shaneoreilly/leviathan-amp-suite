import SwiftUI
import CoreMIDI

// MARK: - MIDI control: learn a CC for any knob or on/off switch, Program Change picks presets

/// One learned assignment: a CC on a channel drives one setting.
struct MidiMap: Codable, Identifiable, Equatable {
    var id = UUID()
    var channel: Int          // 0...15
    var cc: Int               // 0...127
    var key: String           // Settings property name, e.g. "odDrive" or "odOn"
    var name: String          // shown in Settings, e.g. "OVERDRIVE · Drive"
    var lo = 0.0, hi = 1.0    // knob range the CC is scaled onto
    var toggle = false        // on/off switch rather than a knob
    var momentary = false     // switch sends press/release: each press flips (otherwise ≥64 = on, <64 = off)
}

/// The control waiting for a CC while learning.
struct MidiArmed: Equatable {
    let key: String, name: String, lo: Double, hi: Double, toggle: Bool
}

/// Last message received, kept apart so a busy expression pedal doesn't redraw every knob.
final class MidiMonitor: ObservableObject { @Published var last = "" }

struct MidiScopeKey: EnvironmentKey { static let defaultValue = "" }
extension EnvironmentValues {
    /// Title of the enclosing unit, so a knob can name itself ("AMP · Gain") when it's learned.
    var midiScope: String { get { self[MidiScopeKey.self] } set { self[MidiScopeKey.self] = newValue } }
}

extension PedalKind {
    /// Name of this pedal's on/off field in Settings.
    var onKey: String { "\(self)On" }
}

@MainActor
final class Midi: ObservableObject {
    unowned let model: Model
    let monitor = MidiMonitor()

    @Published var maps: [MidiMap] = [] { didSet { save() } }
    @Published var learning = false { didSet { if !learning { armed = nil } } }
    @Published var armed: MidiArmed?
    @Published var sources: [String] = []
    @Published var programChange = UserDefaults.standard.object(forKey: "midiPC") as? Bool ?? true {
        didSet { UserDefaults.standard.set(programChange, forKey: "midiPC") }
    }

    private var client = MIDIClientRef()
    private var port = MIDIPortRef()
    private var connected: [MIDIEndpointRef] = []
    private var justLearned: (id: UUID, at: Date)?

    init(model: Model) {
        self.model = model
        if let d = UserDefaults.standard.data(forKey: "midiMaps"), let m = try? JSONDecoder().decode([MidiMap].self, from: d) { maps = m }
    }

    private func save() { UserDefaults.standard.set(try? JSONEncoder().encode(maps), forKey: "midiMaps") }

    func start() {
        guard client == 0 else { return }
        var st = MIDIClientCreateWithBlock("Leviathan" as CFString, &client) { [weak self] n in
            guard n.pointee.messageID == .msgSetupChanged else { return }
            DispatchQueue.main.async { MainActor.assumeIsolated { self?.connectAll() } }
        }
        guard st == noErr else { return }
        st = MIDIInputPortCreateWithProtocol(client, "Leviathan In" as CFString, ._1_0, &port) { [weak self] list, _ in
            let words = Midi.channelVoiceWords(list)
            guard !words.isEmpty else { return }
            DispatchQueue.main.async { MainActor.assumeIsolated { for w in words { self?.handle(w) } } }
        }
        guard st == noErr else { return }
        connectAll()
    }

    /// Listen to every MIDI source (rerun when devices come and go).
    private func connectAll() {
        for e in connected { MIDIPortDisconnectSource(port, e) }
        connected = []
        var names: [String] = []
        for i in 0..<MIDIGetNumberOfSources() {
            let e = MIDIGetSource(i)
            guard e != 0, MIDIPortConnectSource(port, e, nil) == noErr else { continue }
            connected.append(e)
            var cf: Unmanaged<CFString>?
            if MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &cf) == noErr, let s = cf?.takeRetainedValue() { names.append(s as String) }
        }
        sources = names
    }

    /// MIDI 1.0 channel-voice messages (UMP type 2) from an event list; everything else is skipped.
    nonisolated static func channelVoiceWords(_ list: UnsafePointer<MIDIEventList>) -> [UInt32] {
        let sizes = [1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4]   // UMP words per message type
        var out: [UInt32] = []
        for p in list.unsafeSequence() {
            let n = Int(p.pointee.wordCount)
            withUnsafeBytes(of: p.pointee.words) { raw in
                let w = raw.bindMemory(to: UInt32.self)
                var i = 0
                while i < min(n, w.count) {
                    let t = Int(w[i] >> 28)
                    if t == 2 { out.append(w[i]) }
                    i += sizes[t]
                }
            }
        }
        return out
    }

    private func handle(_ w: UInt32) {
        let status = Int((w >> 16) & 0xF0), ch = Int((w >> 16) & 0x0F), d1 = Int((w >> 8) & 0x7F), d2 = Int(w & 0x7F)
        switch status {
        case 0xB0:
            monitor.last = "CC \(d1) = \(d2) · ch \(ch + 1)"
            controlChange(ch, d1, d2)
        case 0xC0:
            monitor.last = "Program \(d1 + 1) · ch \(ch + 1)"
            if programChange && !learning {
                let order = model.presetOrder
                if d1 < order.count { model.loadAny(order[d1]) }
            }
        default: break
        }
    }

    private func controlChange(_ ch: Int, _ cc: Int, _ v: Int) {
        if learning, let a = armed {
            maps.removeAll { $0.key == a.key }
            let m = MidiMap(channel: ch, cc: cc, key: a.key, name: a.name, lo: a.lo, hi: a.hi, toggle: a.toggle)
            maps.append(m)
            justLearned = (m.id, Date())
            armed = nil
            return
        }
        for i in maps.indices where maps[i].channel == ch && maps[i].cc == cc {
            let m = maps[i]
            if m.toggle {
                // A release arriving right after the learn press means the switch is momentary.
                if !m.momentary, v < 64, let j = justLearned, j.id == m.id, -j.at.timeIntervalSinceNow < 1.5 {
                    maps[i].momentary = true; justLearned = nil; continue
                }
                if m.momentary {
                    if v >= 64, let cur = model.setting(m.key) as? Bool { model.setSetting(m.key, !cur) }
                } else {
                    model.setSetting(m.key, v >= 64)
                }
            } else {
                model.setSetting(m.key, m.lo + (m.hi - m.lo) * Double(v) / 127)
            }
        }
    }

    // MARK: learn

    func arm(_ a: MidiArmed) { armed = armed == a ? nil : a }
    func map(for key: String) -> MidiMap? { maps.first { $0.key == key } }
    func forget(_ key: String) { maps.removeAll { $0.key == key } }
}

/// Learn-mode overlay for a knob dial or an on/off switch: click to arm, shows the assigned CC.
struct MidiLearnOverlay: View {
    @EnvironmentObject var midi: Midi
    let armed: MidiArmed
    var cornerRadius: CGFloat = 10

    var body: some View {
        let isArmed = midi.armed == armed, mapped = midi.map(for: armed.key)
        if midi.learning {
            RoundedRectangle(cornerRadius: cornerRadius)
                .fill(isArmed ? Color.accentColor.opacity(0.35) : mapped != nil ? Color.accentColor.opacity(0.15) : Color.white.opacity(0.04))
                .overlay(RoundedRectangle(cornerRadius: cornerRadius)
                    .stroke(isArmed ? Color.accentColor : Color.white.opacity(0.35), style: StrokeStyle(lineWidth: isArmed ? 2 : 1, dash: isArmed ? [] : [3, 3])))
                .overlay {
                    Text(isArmed ? "move a\ncontrol" : mapped.map { "CC \($0.cc)" } ?? "")
                        .font(.system(size: 9 * UI, weight: .bold)).multilineTextAlignment(.center)
                        .padding(2).background(Color.black.opacity(0.6), in: RoundedRectangle(cornerRadius: 3))
                        .opacity(isArmed || mapped != nil ? 1 : 0)
                }
                .contentShape(Rectangle())
                .onTapGesture { midi.arm(armed) }
                .contextMenu { if mapped != nil { Button("Forget MIDI") { midi.forget(armed.key) } } }
                .help(mapped.map { "\(armed.name): CC \($0.cc), channel \($0.channel + 1). Click, then move another control to reassign." }
                      ?? "Click, then move a control on your MIDI device")
        }
    }
}

/// Banner shown on the rig while learning.
struct MidiLearnBanner: View {
    @EnvironmentObject var midi: Midi
    var body: some View {
        HStack {
            Image(systemName: "pianokeys")
            Text(midi.armed.map { "Move a knob, pedal or switch on your MIDI device to assign it to \($0.name)." }
                 ?? "MIDI learn: click a knob or an ON/OFF switch, then move a control on your MIDI device.")
            Spacer()
            if midi.sources.isEmpty { Text("No MIDI devices connected").foregroundStyle(.orange) }
            Button("Done") { midi.learning = false }.keyboardShortcut(.escape, modifiers: [])
        }
        .font(.system(size: 12 * UI))
        .padding(10)
        .background(Color.accentColor.opacity(0.18))
    }
}

/// The MIDI group on the Settings screen.
struct MidiSettings: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var midi: Midi
    let row: (String, AnyView) -> AnyView

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            row("Devices", AnyView(
                Text(midi.sources.isEmpty ? "None connected" : midi.sources.joined(separator: ", "))
                    .foregroundStyle(midi.sources.isEmpty ? .secondary : .primary)))
            row("Last message", AnyView(MonitorText(mon: midi.monitor)))
            row("Program change", AnyView(
                Toggle("Program 1, 2, 3… loads the 1st, 2nd, 3rd… preset in the Presets list", isOn: $midi.programChange)))
            row("Assignments", AnyView(
                HStack {
                    Button("Learn on the rig…") { m.screen = .rig; midi.learning = true }
                    Button("Clear all", role: .destructive) { midi.maps = [] }.disabled(midi.maps.isEmpty)
                }))
            if midi.maps.isEmpty {
                Text("Nothing assigned. Use Learn, click a knob or switch, then move a control on your MIDI device. An expression pedal on Wah Position makes a real wah.")
                    .font(.system(size: 11 * UI)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            } else {
                Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 6) {
                    ForEach($midi.maps) { $mp in
                        GridRow {
                            Text("CC \(mp.cc) · ch \(mp.channel + 1)").monospacedDigit().foregroundStyle(.secondary)
                            Text(mp.name)
                            if mp.toggle {
                                Toggle("Momentary switch", isOn: $mp.momentary)
                                    .help("On: each press flips it. Off: the switch's value sets it (127 = on, 0 = off).")
                            } else { Text("") }
                            Button { midi.maps.removeAll { $0.id == mp.id } } label: { Image(systemName: "xmark") }.buttonStyle(.borderless)
                        }
                    }
                }
            }
        }
    }

    struct MonitorText: View {
        @ObservedObject var mon: MidiMonitor
        var body: some View { Text(mon.last.isEmpty ? "—" : mon.last).monospacedDigit().foregroundStyle(.secondary) }
    }
}

/// Top-bar toggle for learn mode.
struct MidiLearnButton: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var midi: Midi
    var body: some View {
        Toggle("MIDI", isOn: Binding(get: { midi.learning }, set: { midi.learning = $0; if $0 { m.screen = .rig } }))
            .toggleStyle(.button)
            .help("MIDI learn: click a knob or switch, then move a control on your MIDI device")
    }
}
