import SwiftUI

// Pitch: two rows alike, ‹ [guitar  tuning ▾] › (your guitar) and ‹ [speaker  tuning ▾] › (Play as). ‹ › shift down and up a semitone;
// the button opens a list with search: every tuning the shifter can reach from your guitar's, then bands and the
// tunings they play in (shared/pitch/bands.json, from tab sites). A band in a tuning your guitar can't reach by
// shifting every string the same (a drop tuning from a standard one) is shown, greyed. Windows: UI/PlayAs.cpp.

struct Band { let name: String; let tunings: [(name: String, strings: [Int])] }

/// The band list, read once from the app bundle.
let BANDS: [Band] = {
    guard let u = Bundle.main.url(forResource: "bands", withExtension: "json"), let d = try? Data(contentsOf: u),
          let o = try? JSONSerialization.jsonObject(with: d) as? [String: Any], let all = o["bands"] as? [[String: Any]] else { return [] }
    return all.compactMap { b in
        guard let n = b["name"] as? String, let ts = b["tunings"] as? [[String: Any]] else { return nil }
        let tunings = ts.compactMap { t -> (name: String, strings: [Int])? in
            guard let tn = t["name"] as? String, let s = t["strings"] as? [Int], s.count == 6 else { return nil }
            return (tn, s)
        }
        return tunings.isEmpty ? nil : Band(name: n, tunings: tunings)
    }
}()

extension Model {
    /// The shift that makes your guitar sound in a tuning, if shifting every string the same gets there (within an octave).
    func shift(to strings: [Int]) -> Int? {
        let k = strings[0] - tuning.strings[0]
        guard abs(k) <= 12, zip(strings, tuning.strings).allSatisfy({ $0 - $1 == k }) else { return nil }
        return k
    }
    /// What you sound like now: the shift while the shifter is on, else none.
    var currentShift: Int { s.pitchOn ? Int(s.pitchSemis.rounded()) : 0 }
}

final class PlayAsUI: ObservableObject { @Published var open = false; @Published var query = "" }

/// One Pitch row: ‹ [icon  name ▾] ›, one line. The guitar row and the play-as row look the same.
struct PitchSelector<Pop: View>: View {
    @EnvironmentObject var m: Model
    let guitar: Bool, name: String, help: String, prevHelp: String, nextHelp: String
    @Binding var open: Bool
    let prev: () -> Void, next: () -> Void
    @ViewBuilder let popover: () -> Pop
    var body: some View {
        HStack(spacing: 4) {
            Button(action: prev) { Image(systemName: "chevron.left").frame(width: 18 * Z, height: 28 * Z).contentShape(Rectangle()) }
                .buttonStyle(.borderless).help(prevHelp)
            Button { open.toggle() } label: {
                DropdownBox(text: name, width: 150 * W, open: open) {
                    Group {
                        if guitar { GuitarIcon().stroke(.secondary, style: StrokeStyle(lineWidth: 1.3, lineCap: .round, lineJoin: .round)).frame(width: 16 * Z, height: 16 * Z) }
                        else { Image(systemName: "speaker.wave.2").foregroundStyle(.secondary) }
                    }.frame(width: 18 * Z)
                }
            }
            .buttonStyle(.plain)
            .popover(isPresented: $open, arrowEdge: .bottom, content: popover)
            .help(help)
            Button(action: next) { Image(systemName: "chevron.right").frame(width: 18 * Z, height: 28 * Z).contentShape(Rectangle()) }
                .buttonStyle(.borderless).help(nextHelp)
        }
    }
}

final class GuitarUI: ObservableObject { @Published var open = false }

/// One electric guitar, outlined and tilted like 🎸: a double-cutaway body drawn as one smooth outline, two pickups,
/// neck and an angled headstock. Windows: PlayAs.cpp guitarPath, the same points.
struct GuitarIcon: Shape {
    func path(in r: CGRect) -> Path {
        var path = Path()
        for stroke in guitarStrokes(in: r) {
            guard let first = stroke.points.first else { continue }
            path.move(to: first); stroke.points.dropFirst().forEach { path.addLine(to: $0) }
            if stroke.closed { path.closeSubpath() }
        }
        return path
    }
}

/// Control points of the body outline, upright (neck up), clockwise from the neck: treble horn, lower bout, bass horn.
let GUITAR_BODY: [(Double, Double)] = [(0.7, 0.8), (1.6, -0.4), (2.6, -1.8), (3.5, -1.7), (3.9, -0.4), (3.5, 1.6), (4.8, 3.6), (5.1, 6.2),
    (4.2, 8.6), (2.0, 9.9), (0, 10.2), (-2.2, 9.9), (-4.4, 8.6), (-5.2, 6.2), (-5.0, 3.6), (-3.8, 1.6), (-4.4, -0.8), (-4.0, -2.8),
    (-3.0, -3.2), (-2.0, -1.8), (-0.7, 0.8)]

/// The guitar as strokes (body smoothed through its control points, neck, headstock, pickups), turned 45° and fitted into r.
func guitarStrokes(in r: CGRect) -> [(points: [CGPoint], closed: Bool)] {
    // a closed Catmull-Rom curve through the body's control points
    let n = GUITAR_BODY.count
    var body: [(Double, Double)] = []
    for i in 0..<n {
        let p0 = GUITAR_BODY[(i - 1 + n) % n], p1 = GUITAR_BODY[i], p2 = GUITAR_BODY[(i + 1) % n], p3 = GUITAR_BODY[(i + 2) % n]
        for s in 0..<6 {
            let t = Double(s) / 6, t2 = t * t, t3 = t2 * t
            func cr(_ a: Double, _ b: Double, _ c: Double, _ d: Double) -> Double {
                0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3)
            }
            body.append((cr(p0.0, p1.0, p2.0, p3.0), cr(p0.1, p1.1, p2.1, p3.1)))
        }
    }
    let all: [([(Double, Double)], Bool)] = [
        (body, true),
        ([(0, 0.8), (0, -11)], false),                                             // neck
        ([(-0.7, -11), (-1.0, -13.8), (0, -15), (1.3, -14.4), (0.7, -11)], true),  // headstock
        ([(-1.6, 3.4), (1.6, 3.4)], false), ([(-1.6, 5.6), (1.6, 5.6)], false),    // pickups
    ]
    let c = cos(Double.pi / 4), sn = sin(Double.pi / 4)   // headstock to the top right
    let turned = all.map { ($0.0.map { (x, y) in (x * c - y * sn, x * sn + y * c) }, $0.1) }
    let xs = turned.flatMap { $0.0.map(\.0) }, ys = turned.flatMap { $0.0.map(\.1) }
    let w = xs.max()! - xs.min()!, h = ys.max()! - ys.min()!
    let k = min(Double(r.width) / w, Double(r.height) / h)
    let ox = Double(r.midX) - (xs.min()! + w / 2) * k, oy = Double(r.midY) - (ys.min()! + h / 2) * k
    return turned.map { (points: $0.0.map { CGPoint(x: ox + $0.0 * k, y: oy + $0.1 * k) }, closed: $0.1) }
}

/// 🎸 your guitar's tuning: ‹ › step through them, the button lists them with their notes.
struct GuitarPicker: View {
    @EnvironmentObject var m: Model
    @StateObject private var ui = GuitarUI()
    var body: some View {
        let step = { (d: Int) in m.tuningIndex = stepTuning(m.tuningIndex, d) }
        PitchSelector(guitar: true, name: m.tuning.name, help: "Your guitar's tuning", prevHelp: "Previous tuning", nextHelp: "Next tuning",
                      open: $ui.open, prev: { step(-1) }, next: { step(1) }) {
            VStack(alignment: .leading, spacing: 1) {
                ForEach(TUNING_ORDER, id: \.self) { i in
                    ListRow(title: TUNINGS[i].name, detail: tuningNotes(TUNINGS[i].strings, flats: TUNINGS[i].flats),
                                 on: i == m.tuningIndex, enabled: true) { m.tuningIndex = i }
                }
            }
            .padding(6).font(.inter(size: T(10.5) * UI)).frame(width: 340 * Z).environmentObject(m)
        }
    }
}

/// 🔈 what you sound like: ‹ › a semitone, the button searches tunings and bands.
struct PlayAsPicker: View {
    @EnvironmentObject var m: Model
    @StateObject private var ui = PlayAsUI()
    var body: some View {
        PitchSelector(guitar: false, name: m.soundsLike, help: "Play as: what you sound like. Search tunings and bands",
                      prevHelp: "Down a semitone", nextHelp: "Up a semitone", open: $ui.open,
                      prev: { m.playAs(max(Int(m.s.pitchSemis.rounded()) - 1, -12)) },
                      next: { m.playAs(min(Int(m.s.pitchSemis.rounded()) + 1, 12)) }) {
            PlayAsBrowser(ui: ui).environmentObject(m)
        }
    }
}

struct PlayAsBrowser: View {
    @EnvironmentObject var m: Model
    @ObservedObject var ui: PlayAsUI

    private struct Row: Identifiable { let id: String; let title, detail: String; let shift: Int? }

    var body: some View {
        let words = ui.query.lowercased().split(separator: " ").map(String.init)
        let match = { (s: String) in words.allSatisfy { s.lowercased().contains($0) } }
        let cur = m.currentShift
        let tunings = m.playAsOptions.compactMap { o -> Row? in
            let notes = tuningNotes(m.tuning.strings.map { $0 + o.semis }, flats: m.tuning.flats)
            guard match("\(o.name) \(notes)") else { return nil }
            return Row(id: "t\(o.semis)", title: o.name, detail: o.semis == 0 ? "\(notes)  (no shift)" : String(format: "%@  (%+d)", notes, o.semis), shift: o.semis)
        }
        let bands = BANDS.flatMap { b in
            b.tunings.compactMap { t -> Row? in
                guard match("\(b.name) \(t.name)") else { return nil }
                let k = m.shift(to: t.strings)
                return Row(id: "b\(b.name)|\(t.name)", title: b.name,
                           detail: k.map { t.name + ($0 == 0 ? "  (no shift)" : String(format: "  (%+d)", $0)) } ?? "\(t.name): retune your guitar for this one",
                           shift: k)
            }
        }
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                TextField("Search bands or tunings…", text: $ui.query).textFieldStyle(.plain)
            }
            .padding(.horizontal, 10).padding(.vertical, 8)
            .background(RoundedRectangle(cornerRadius: 8).fill(PAL.tint.opacity(0.06)))
            .padding(12)
            Divider()
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 1, pinnedViews: [.sectionHeaders]) {
                        section("Tunings", tunings, current: cur, bands: false)
                        section("Bands", bands, current: cur, bands: true)
                        if tunings.isEmpty && bands.isEmpty {
                            Text("Nothing matches “\(ui.query)”.").foregroundStyle(.secondary).frame(maxWidth: .infinity).padding(30)
                        }
                    }.padding(6)
                }
                .onAppear { if ui.query.isEmpty { proxy.scrollTo("t\(cur)", anchor: .center) } }
            }
            Divider()
            Text("Your guitar: \(m.tuning.name). Click to hear it.").foregroundStyle(.secondary)
                .font(.inter(size: T(9) * UI)).frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 12).padding(.vertical, 7)
        }
        .font(.inter(size: T(10.5) * UI))
        .frame(width: 440 * Z, height: 480 * Z)
    }

    @ViewBuilder private func section(_ title: String, _ rows: [Row], current: Int, bands: Bool) -> some View {
        if !rows.isEmpty {
            Section {
                ForEach(rows) { r in
                    ListRow(title: r.title, detail: r.detail, on: !bands && r.shift == current, enabled: r.shift != nil) {
                        if let k = r.shift { m.playAs(k) }
                    }.id(r.id)
                }
            } header: {
                Text(title.uppercased()).font(.inter(size: T(9) * UI, weight: .semibold)).tracking(1).foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, alignment: .leading).padding(.horizontal, 12).padding(.vertical, 6)
                    .background(Color(nsColor: .windowBackgroundColor))
            }
        }
    }
}
