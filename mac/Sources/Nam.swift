import Foundation
import AppKit
import UniformTypeIdentifiers

// MARK: - Neural Amp Modeler captures (.nam), via the shared C++ bridge (shared/nam/lev_nam.h)

/// The NAM folder, loading on a background queue, and handing models to the audio thread safely.
@MainActor
final class NamHost {
    static let dir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("Leviathan/NAM")
    /// Captures that ship with the app (shared/nam/captures, GPLv3, see CREDITS.md there).
    static let bundled = Bundle.main.resourceURL?.appendingPathComponent("NAM")

    /// Capture names (file names without .nam), sorted.
    private(set) var files: [String] = []
    /// What's loaded now: the model, its name, the rate it was reset for, and its metadata.
    private(set) var model: OpaquePointer?
    private(set) var loadedName = "", loadedRate = 0.0
    private(set) var expectedRate = 0.0, loudness: Double?
    /// The capture's calibrated input level (dBu at 0 dBFS), if its file has one, and the input gain applied for it.
    private(set) var inputLevel: Double?, inputTrim = 0.0
    private var loadedCal: Double?
    private var generation = 0

    /// 0 = amp A, 1 = amp B: which engine slot this host loads into.
    let slot: Int
    init(slot: Int = 0) { self.slot = slot; scan() }

    private func install(_ m: OpaquePointer?, gain: Float, input: Float, dsp: DSP?) {
        if slot == 0 { dsp?.setNam(m, gain: gain, input: input) } else { dsp?.setNamB(m, gain: gain, input: input) }
    }

    func scan() {
        try? FileManager.default.createDirectory(at: NamHost.dir, withIntermediateDirectories: true)
        var names = Set<String>()
        for d in [NamHost.dir, NamHost.bundled].compactMap({ $0 }) {
            let urls = (try? FileManager.default.contentsOfDirectory(at: d, includingPropertiesForKeys: nil)) ?? []
            for u in urls where u.pathExtension.lowercased() == "nam" { names.insert(u.deletingPathExtension().lastPathComponent) }
        }
        files = names.sorted { $0.localizedStandardCompare($1) == .orderedAscending }
    }

    /// Your own copy wins over the bundled one of the same name.
    func url(_ name: String) -> URL {
        let mine = NamHost.dir.appendingPathComponent(name + ".nam")
        if FileManager.default.fileExists(atPath: mine.path) { return mine }
        return NamHost.bundled?.appendingPathComponent(name + ".nam") ?? mine
    }

    struct Opened { let model: OpaquePointer, gain: Float, input: Float, expected: Double, loudness: Double?, inputLevel: Double?, trim: Double }

    /// Input calibration: the gain (dB) that makes your interface hit the capture the way the original rig did.
    /// `interface` is the level (dBu) that reaches 0 dBFS on your input, or nil when calibration is off.
    nonisolated static func calibrationTrim(capture: Double?, interface: Double?) -> Double {
        guard let capture, let interface else { return 0 }
        return min(max(interface - capture, -24), 24)
    }

    /// Loads (off the main thread) and reset for `rate`; returns the model, its loudness-normalising gain and its
    /// input calibration gain (`calibration`: your interface's dBu at 0 dBFS, nil = off).
    nonisolated static func open(_ url: URL, rate: Double, calibration: Double?) -> Opened? {
        var err = [CChar](repeating: 0, count: 256)
        guard let m = lev_nam_load(url.path, &err, Int32(err.count)) else { return nil }
        lev_nam_reset(m, rate, Int32(DSP.maxN))
        let loud: Double? = lev_nam_has_loudness(m) != 0 ? lev_nam_loudness(m) : nil
        let level: Double? = lev_nam_has_input_level(m) != 0 ? lev_nam_input_level(m) : nil
        let trim = calibrationTrim(capture: level, interface: calibration), input = Float(pow(10, trim / 20))
        // level-matched (measured at the calibrated input, since many files have no loudness metadata), and the
        // right way up (some captures invert, which cancels when two are blended)
        let gain = Float(pow(10, (LEV_NAM_TARGET_LEVEL - lev_nam_measure_level_in(m, rate, input)) / 20)) * Float(lev_nam_polarity(m, rate))
        return Opened(model: m, gain: gain, input: input, expected: lev_nam_expected_rate(m), loudness: loud, inputLevel: level, trim: trim)
    }

    /// Make `name` the engine's capture at `rate` ("" unloads). `done` reports an error message, if any.
    func select(_ name: String, rate: Double, calibration: Double?, dsp: DSP?, done: @escaping (String?) -> Void) {
        generation += 1
        let gen = generation
        if name.isEmpty { swap(nil, name: "", rate: 0, gain: 1, dsp: dsp); done(nil); return }
        if name == loadedName && rate == loadedRate && calibration == loadedCal, model != nil { install(model, gain: currentGain, input: currentInput, dsp: dsp); done(nil); return }
        let url = url(name)
        guard FileManager.default.fileExists(atPath: url.path) else { swap(nil, name: "", rate: 0, gain: 1, dsp: dsp); done("NAM capture \u{201C}\(name)\u{201D} isn't in the NAM folder."); return }
        DispatchQueue.global(qos: .userInitiated).async {
            let r = NamHost.open(url, rate: rate, calibration: calibration)
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    guard gen == self.generation else { if let r { lev_nam_free(r.model) }; return }   // superseded
                    guard let r else { self.swap(nil, name: "", rate: 0, gain: 1, dsp: dsp); done("Couldn't load NAM capture \u{201C}\(name)\u{201D}."); return }
                    self.expectedRate = r.expected; self.loudness = r.loudness; self.inputLevel = r.inputLevel; self.inputTrim = r.trim
                    self.loadedCal = calibration
                    self.swap(r.model, name: name, rate: rate, gain: r.gain, input: r.input, dsp: dsp)
                    done(r.expected > 0 && abs(r.expected - rate) > 1
                         ? String(format: "\u{201C}%@\u{201D} was captured at %.1f kHz but the engine runs at %.1f kHz, so it won't sound quite right. Use a %.0f kHz device setting if you can.", name, r.expected / 1000, rate / 1000, r.expected / 1000)
                         : nil)
                }
            }
        }
    }

    private var currentGain: Float = 1, currentInput: Float = 1

    /// Hand the new model to the audio thread; free the old one once it can no longer be in use.
    private func swap(_ m: OpaquePointer?, name: String, rate: Double, gain: Float, input: Float = 1, dsp: DSP?) {
        let old = model
        model = m; loadedName = name; loadedRate = rate; currentGain = gain; currentInput = input
        if m == nil { inputLevel = nil; inputTrim = 0; loadedCal = nil }
        install(m, gain: gain, input: input, dsp: dsp)
        if let old, old != m {
            DispatchQueue.main.asyncAfter(deadline: .now() + 1) { lev_nam_free(old) }
        }
    }

    /// Copy picked .nam files into the NAM folder. Returns the names added.
    func importFiles() -> [String] {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = true
        panel.allowedContentTypes = [UTType(filenameExtension: "nam") ?? .data]
        panel.message = "Choose NAM captures (.nam), for example from tone3000.com"
        guard panel.runModal() == .OK else { return [] }
        var added: [String] = []
        for u in panel.urls where u.pathExtension.lowercased() == "nam" {
            let dest = NamHost.dir.appendingPathComponent(u.lastPathComponent)
            try? FileManager.default.removeItem(at: dest)
            if (try? FileManager.default.copyItem(at: u, to: dest)) != nil { added.append(u.deletingPathExtension().lastPathComponent) }
        }
        scan()
        return added
    }
}
