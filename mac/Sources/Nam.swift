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
    private var generation = 0

    init() { scan() }

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

    /// Loads (off the main thread) and reset for `rate`; returns the model and its loudness-normalising gain.
    nonisolated static func open(_ url: URL, rate: Double) -> (model: OpaquePointer, gain: Float, expected: Double, loudness: Double?)? {
        var err = [CChar](repeating: 0, count: 256)
        guard let m = lev_nam_load(url.path, &err, Int32(err.count)) else { return nil }
        lev_nam_reset(m, rate, Int32(DSP.maxN))
        let loud: Double? = lev_nam_has_loudness(m) != 0 ? lev_nam_loudness(m) : nil
        // level-match every capture by measuring it (many files have no loudness metadata)
        let gain = Float(pow(10, (LEV_NAM_TARGET_LEVEL - lev_nam_measure_level(m, rate)) / 20))
        return (m, gain, lev_nam_expected_rate(m), loud)
    }

    /// Make `name` the engine's capture at `rate` ("" unloads). `done` reports an error message, if any.
    func select(_ name: String, rate: Double, dsp: DSP?, done: @escaping (String?) -> Void) {
        generation += 1
        let gen = generation
        if name.isEmpty { swap(nil, name: "", rate: 0, gain: 1, dsp: dsp); done(nil); return }
        if name == loadedName && rate == loadedRate, model != nil { dsp?.setNam(model, gain: currentGain); done(nil); return }
        let url = url(name)
        guard FileManager.default.fileExists(atPath: url.path) else { swap(nil, name: "", rate: 0, gain: 1, dsp: dsp); done("NAM capture \u{201C}\(name)\u{201D} isn't in the NAM folder."); return }
        DispatchQueue.global(qos: .userInitiated).async {
            let r = NamHost.open(url, rate: rate)
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    guard gen == self.generation else { if let r { lev_nam_free(r.model) }; return }   // superseded
                    guard let r else { self.swap(nil, name: "", rate: 0, gain: 1, dsp: dsp); done("Couldn't load NAM capture \u{201C}\(name)\u{201D}."); return }
                    self.expectedRate = r.expected; self.loudness = r.loudness
                    self.swap(r.model, name: name, rate: rate, gain: r.gain, dsp: dsp)
                    done(r.expected > 0 && abs(r.expected - rate) > 1
                         ? String(format: "\u{201C}%@\u{201D} was captured at %.1f kHz but the engine runs at %.1f kHz, so it won't sound quite right. Use a %.0f kHz device setting if you can.", name, r.expected / 1000, rate / 1000, r.expected / 1000)
                         : nil)
                }
            }
        }
    }

    private var currentGain: Float = 1

    /// Hand the new model to the audio thread; free the old one once it can no longer be in use.
    private func swap(_ m: OpaquePointer?, name: String, rate: Double, gain: Float, dsp: DSP?) {
        let old = model
        model = m; loadedName = name; loadedRate = rate; currentGain = gain
        dsp?.setNam(m, gain: gain)
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
