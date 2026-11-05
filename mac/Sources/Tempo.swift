import SwiftUI

/// Note lengths the delay can lock to, in beats.
let DELAY_DIVS: [(name: String, beats: Double)] = [
    ("Free (ms)", 0), ("1/2", 2), ("1/4", 1), ("1/8 dotted", 0.75), ("1/8", 0.5), ("1/4 triplet", 2.0 / 3), ("1/8 triplet", 1.0 / 3), ("1/16", 0.25),
]

/// Tap tempo button (MIDI-learnable).
struct TapButton: View {
    @EnvironmentObject var m: Model
    var body: some View {
        Button("Tap") { m.tap() }
            .help("Tap the beat to set the tempo")
            .overlay { MidiLearnOverlay(armed: .action("@tap", "Tap tempo"), cornerRadius: 6) }
    }
}

/// Top-bar tempo: metronome switch with beat lights, the tempo (click for options) and tap.
struct TempoControl: View {
    @EnvironmentObject var m: Model
    @StateObject private var ui = TempoUI()

    var body: some View {
        HStack(spacing: 4) {
            Toggle(isOn: $m.metroOn) { Image(systemName: "metronome") }
                .toggleStyle(.button).help("Metronome")
                .overlay { MidiLearnOverlay(armed: .action("@metro", "Metronome"), cornerRadius: 6) }
            Button { ui.open.toggle() } label: {
                HStack(spacing: 6) {
                    Text(String(format: m.bpm.truncatingRemainder(dividingBy: 1) == 0 ? "%.0f" : "%.1f", m.bpm)).monospacedDigit()
                    if m.metroOn { BeatLights(mt: m.meters, beats: m.beatsPerBar) }
                }
            }
            .help("Tempo and metronome settings")
            .popover(isPresented: $ui.open, arrowEdge: .bottom) { TempoPopover().environmentObject(m).padding(16) }
            TapButton()
        }
        .fixedSize()
    }
}

final class TempoUI: ObservableObject { @Published var open = false }

struct BeatLights: View {
    @ObservedObject var mt: Meters
    let beats: Int
    var body: some View {
        HStack(spacing: 3) {
            ForEach(0..<min(beats, 12), id: \.self) { i in
                Circle().fill(mt.beat == i ? (i == 0 ? Color.accentColor : .white) : Color(white: 0.25)).frame(width: 6, height: 6)
            }
        }
    }
}

struct TempoPopover: View {
    @EnvironmentObject var m: Model
    @EnvironmentObject var midi: Midi
    var body: some View {
        Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 10) {
            GridRow {
                Text("Tempo").foregroundStyle(.secondary)
                HStack {
                    TextField("", value: Binding(get: { m.bpm }, set: { m.bpm = min(max($0, 30), 300) }), format: .number.precision(.fractionLength(0...1)))
                        .frame(width: 60).multilineTextAlignment(.trailing)
                    Stepper("BPM", value: Binding(get: { m.bpm }, set: { m.bpm = min(max($0.rounded(), 30), 300) }), in: 30...300, step: 1)
                    TapButton()
                }
            }
            GridRow {
                Text("Metronome").foregroundStyle(.secondary)
                Toggle("On", isOn: $m.metroOn)
            }
            GridRow {
                Text("Beats per bar").foregroundStyle(.secondary)
                Picker("", selection: $m.beatsPerBar) { ForEach(1...12, id: \.self) { Text("\($0)").tag($0) } }.labelsHidden().frame(width: 80)
            }
            GridRow {
                Text("Click volume").foregroundStyle(.secondary)
                HStack {
                    Slider(value: $m.metroVol, in: -40...0).frame(width: 160)
                    Text(String(format: "%.0f dB", m.metroVol)).monospacedDigit().frame(width: 50, alignment: .trailing)
                }
            }
            GridRow {
                Text("MIDI clock").foregroundStyle(.secondary)
                Toggle("Follow incoming clock", isOn: $midi.followClock)
            }
        }
        .frame(minWidth: 360)
    }
}
