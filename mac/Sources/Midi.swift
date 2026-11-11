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

/// Turns MIDI clock (24 ticks per beat) into a tempo. Only touched on the MIDI thread.
final class ClockTracker: @unchecked Sendable {
    private var ticks = 0, beatStart: UInt64 = 0, last: UInt64 = 0, smooth = 0.0
    private let toSec: Double = { var tb = mach_timebase_info_data_t(); mach_timebase_info(&tb); return Double(tb.numer) / Double(tb.denom) / 1e9 }()

    func message(_ status: UInt8, at ts: MIDITimeStamp) -> Double? {
        let now = ts != 0 ? ts : mach_absolute_time()
        switch status {
        case 0xFA, 0xFC: ticks = 0; beatStart = 0; smooth = 0; return nil   // start / stop: begin measuring again
        case 0xF8: break
        default: return nil
        }
        // a gap of over a second means the clock stopped and restarted
        if last != 0 && Double(now &- last) * toSec > 1 { ticks = 0; beatStart = 0; smooth = 0 }
        last = now
        if beatStart == 0 { beatStart = now; ticks = 0; return nil }
        ticks += 1
        guard ticks == 24 else { return nil }
        let secs = Double(now &- beatStart) * toSec
        beatStart = now; ticks = 0
        guard secs > 0.15, secs < 3 else { return nil }
        let b = 60 / secs
        smooth = smooth == 0 ? b : smooth * 0.6 + b * 0.4
        return (smooth * 10).rounded() / 10
    }
}

/// The control waiting for a CC while learning.
struct MidiArmed: Equatable {
    let key: String, name: String, lo: Double, hi: Double, toggle: Bool
    /// A button rather than a setting (key starts with "@", see Model.midiAction).
    static func action(_ key: String, _ name: String) -> MidiArmed { MidiArmed(key: key, name: name, lo: 0, hi: 1, toggle: true) }
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
    @Published var followClock = UserDefaults.standard.object(forKey: "midiClock") as? Bool ?? true {
        didSet { UserDefaults.standard.set(followClock, forKey: "midiClock") }
    }
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
        let clock = ClockTracker()
        st = MIDIInputPortCreateWithProtocol(client, "Leviathan In" as CFString, ._1_0, &port) { [weak self] list, _ in
            let (words, bpm) = Midi.parse(list, clock)
            guard !words.isEmpty || bpm != nil else { return }
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    guard let self else { return }
                    for w in words { self.handle(w) }
                    if let bpm, self.followClock, abs(self.model.bpm - bpm) >= 0.3 { self.model.bpm = bpm }
                }
            }
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

    /// MIDI 1.0 channel-voice messages (UMP type 2) from an event list, plus a new tempo when clock ticks
    /// (UMP type 1, 0xF8) complete a beat. Everything else is skipped. Runs on the MIDI thread.
    nonisolated static func parse(_ list: UnsafePointer<MIDIEventList>, _ clock: ClockTracker) -> ([UInt32], Double?) {
        let sizes = [1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4]   // UMP words per message type
        var out: [UInt32] = [], bpm: Double?
        for p in list.unsafeSequence() {
            let n = Int(p.pointee.wordCount), ts = p.pointee.timeStamp
            withUnsafeBytes(of: p.pointee.words) { raw in
                let w = raw.bindMemory(to: UInt32.self)
                var i = 0
                while i < min(n, w.count) {
                    let t = Int(w[i] >> 28)
                    if t == 2 { out.append(w[i]) }
                    if t == 1, let b = clock.message(UInt8((w[i] >> 16) & 0xFF), at: ts) { bpm = b }
                    i += sizes[t]
                }
            }
        }
        return (out, bpm)
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
                if m.key.hasPrefix("@") {
                    if !m.momentary || v >= 64 { model.midiAction(m.key) }
                } else if m.momentary {
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
/// Whether the pointer is over a control in MIDI learn mode.
final class HoverFlag: ObservableObject { @Published var on = false }

struct MidiLearnOverlay: View {
    @EnvironmentObject var midi: Midi
    let armed: MidiArmed
    var cornerRadius: CGFloat = 10
    @StateObject private var hover = HoverFlag()

    var body: some View {
        if midi.learning { box } else { EmptyView() }
    }

    // Split into small typed pieces: long chained ternaries here time out older Swift type checkers (CI).
    private var isArmed: Bool { midi.armed == armed }
    private var mapped: MidiMap? { midi.map(for: armed.key) }

    private var fillColor: Color {
        if isArmed { return Color.accentColor.opacity(0.35) }
        if mapped != nil { return Color.accentColor.opacity(0.15) }
        return PAL.tint.opacity(0.04)
    }

    private var label: String {
        if isArmed { return "move a\ncontrol" }
        if let m = mapped { return "CC \(m.cc)" }
        return ""
    }

    private var tip: String {
        guard let m = mapped else { return "Click, then move a control on your MIDI device" }
        return "\(armed.name): CC \(m.cc), channel \(m.channel + 1). Click, then move another control to reassign."
    }

    private var box: some View {
        let shape = RoundedRectangle(cornerRadius: cornerRadius)
        // a light tint on every assignable control; a solid outline only on the one you're pointing at or arming
        // (dashed outlines on every control looked like a rendering glitch)
        let stroke: Color = isArmed || hover.on ? Color.accentColor : .clear
        let style = StrokeStyle(lineWidth: isArmed ? 2 : 1.5)
        let showLabel = isArmed || mapped != nil
        return shape.fill(fillColor)
            .overlay(shape.stroke(stroke, style: style))
            .overlay {
                Text(label)
                    .font(.inter(size: T(9) * UI, weight: .bold)).multilineTextAlignment(.center)
                    .padding(2).background(Color.black.opacity(0.6), in: RoundedRectangle(cornerRadius: 3))
                    .opacity(showLabel ? 1 : 0)
            }
            .contentShape(Rectangle())
            .onHover { hover.on = $0 }
            .onTapGesture { midi.arm(armed) }
            .contextMenu { if mapped != nil { Button("Forget MIDI") { midi.forget(armed.key) } } }
            .help(tip)
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
        .font(.inter(size: T(12) * UI))
        .padding(10)
        .background(Color.accentColor.opacity(0.18))
    }
}

/// The MIDI group on the Settings screen.
struct MidiSettings: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var midi: Midi

    var body: some View {
        SettingsHeading("Connection", first: true)
        SettingsRow("Devices", explain: "settings:midiDevices") {
            Text(midi.sources.isEmpty ? "None connected" : midi.sources.joined(separator: ", "))
                .foregroundStyle(midi.sources.isEmpty ? .secondary : .primary)
        }
        SettingsRow("Last message", explain: "settings:midiLast") { MonitorText(mon: midi.monitor) }
        SettingsHeading("Presets")
        SettingsRow("Program change", explain: "settings:programChange") {
            Toggle("Program changes load presets", isOn: $midi.programChange)
        }
        SettingsHeading("Assignments")
        SettingsRow("Assign", explain: "settings:midiAssign") {
            Button("Learn on the rig\u{2026}") { m.screen = .rig; midi.learning = true }
            Button("Clear all", role: .destructive) { midi.maps = [] }.disabled(midi.maps.isEmpty)
            if midi.maps.isEmpty { SettingsCount("Nothing assigned yet") }
        }
        ForEach($midi.maps) { $mp in
            SettingsRow("CC \(mp.cc) \u{00B7} ch \(mp.channel + 1)") {
                Text(mp.name).frame(width: SettingsLayout.wide, alignment: .leading)
                if mp.toggle {
                    Toggle("Momentary switch", isOn: $mp.momentary)
                        .help("On: each press flips it. Off: the switch's value sets it (127 = on, 0 = off).")
                }
                Spacer()
                Button { midi.maps.removeAll { $0.id == mp.id } } label: { Image(systemName: "xmark") }.buttonStyle(.borderless).help("Remove")
            }
        }
    }

    struct MonitorText: View {
        @ObservedObject var mon: MidiMonitor
        var body: some View { Text(mon.last.isEmpty ? "—" : mon.last).monospacedDigit().foregroundStyle(.secondary) }
    }
}

/// Top-bar toggle for learn mode.
/// Top-bar buttons that switch something on (Rec, metronome, Tuner, MIDI): plain text while off, the accent colour
/// only while on. (macOS would otherwise tint every bordered button's text with the accent, on or off.)
struct TopBarToggle: ButtonStyle {
    let on: Bool
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.inter(size: T(9) * UI))   // the size of the bar's other buttons (Save, the preset menu)
            .foregroundStyle(on ? Color.accentColor : Color.primary)
            .padding(.horizontal, 10).padding(.vertical, 5)
            .background(RoundedRectangle(cornerRadius: 6).fill(on ? Color.accentColor.opacity(0.2) : PAL.tint.opacity(configuration.isPressed ? 0.16 : 0.09)))
            .contentShape(RoundedRectangle(cornerRadius: 6))
    }
}

struct MidiLearnButton: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var midi: Midi
    var body: some View {
        Button("MIDI") { midi.learning.toggle(); if midi.learning { m.screen = .rig } }
            .buttonStyle(TopBarToggle(on: midi.learning))
            .help("MIDI learn: click a knob or switch, then move a control on your MIDI device")
    }
}
