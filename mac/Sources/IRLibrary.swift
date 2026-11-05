import Foundation
import AVFoundation

/// One God's Cab impulse, parsed from names like "57_TS_1_inch_edge_pres_3.wav".
struct IRFile: Hashable {
    let rel: String        // path inside the sample-rate folder, e.g. "SM57/NO-TS/57_1_inch_edge_pres_3.wav"
    let mic: String
    let ts: Bool
    let position: String   // "1 inch edge", "grill cone near", "2 feet", ...
    let pres: Int
}

final class IRLibrary {
    static let root = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("Leviathan/IRs/GodsCab")
    static let micOrder = ["SM57", "MD421", "SM7B", "C414", "U87", "NT5"]

    private(set) var files: [IRFile] = []
    private var dir: URL?
    private var cache: [String: [Float]] = [:]
    private var cacheRate = 0.0

    var isEmpty: Bool { files.isEmpty }
    var mics: [String] {
        let have = Set(files.map(\.mic))
        return IRLibrary.micOrder.filter(have.contains) + have.subtracting(IRLibrary.micOrder).sorted()
    }

    /// Index the folder matching the engine rate (the pack ships 44.1k and 48k versions).
    func scan(sampleRate: Double) {
        let d = IRLibrary.root.appendingPathComponent(abs(sampleRate - 44100) < 1 ? "44.1" : "48")
        guard d != dir || files.isEmpty else { return }
        dir = d; cache = [:]
        var out: [IRFile] = []
        let en = FileManager.default.enumerator(at: d, includingPropertiesForKeys: nil)
        while let u = en?.nextObject() as? URL {
            guard u.pathExtension.lowercased() == "wav" else { continue }
            let rel = String(u.standardizedFileURL.path.dropFirst(d.standardizedFileURL.path.count + 1))
            if rel.contains("Legacy") || rel.contains("Stereo") { continue }
            if let f = IRLibrary.parse(rel) { out.append(f) }
        }
        let micRank = { (m: String) in IRLibrary.micOrder.firstIndex(of: m) ?? 99 }
        files = out.sorted { (micRank($0.mic), $0.ts ? 1 : 0, $0.position, $0.pres) < (micRank($1.mic), $1.ts ? 1 : 0, $1.position, $1.pres) }
    }

    static func parse(_ rel: String) -> IRFile? {
        let name = ((rel as NSString).lastPathComponent as NSString).deletingPathExtension
        var t = name.split(separator: "_").map(String.init)
        guard t.count >= 3 else { return nil }
        var mic = t.removeFirst()
        if mic == "57" { mic = "SM57" }
        var ts = false
        if t.first == "TS" { ts = true; t.removeFirst() }
        var pres = 0
        if t.count >= 2, t[t.count - 2] == "pres", let p = Int(t[t.count - 1]) { pres = p; t.removeLast(2) }
        return IRFile(rel: rel, mic: mic, ts: ts, position: t.joined(separator: " "), pres: pres)
    }

    func file(_ rel: String) -> IRFile? { files.first { $0.rel == rel } }

    func positions(mic: String, ts: Bool) -> [String] {
        var seen = Set<String>()
        return files.filter { $0.mic == mic && $0.ts == ts }.map(\.position).filter { seen.insert($0).inserted }
    }

    func tsAvailable(mic: String) -> Bool { files.contains { $0.mic == mic && $0.ts } }

    /// Closest match when one attribute changes, keeping the others where possible.
    func find(mic: String, ts: Bool, position: String, pres: Int) -> IRFile? {
        let c = files.filter { $0.mic == mic && $0.ts == ts }
        let pool = c.isEmpty ? files.filter { $0.mic == mic } : c
        return pool.first { $0.position == position && $0.pres == pres }
            ?? pool.first { $0.position == position }
            ?? pool.first { $0.pres == pres }
            ?? pool.first
    }

    func step(_ rel: String, by d: Int) -> String? {
        guard !files.isEmpty else { return nil }
        guard let i = files.firstIndex(where: { $0.rel == rel }) else { return files.first?.rel }
        return files[(i + d + files.count) % files.count].rel
    }

    /// Energy-normalised samples at the engine rate.
    func samples(_ rel: String, sampleRate: Double) -> [Float]? {
        if cacheRate != sampleRate { cache = [:]; cacheRate = sampleRate }
        if let c = cache[rel] { return c }
        guard let dir, let w = try? readWav(dir.appendingPathComponent(rel)) else { return nil }
        let h = normaliseIR(resample(w.samples, from: w.rate, to: sampleRate))
        cache[rel] = h
        return h
    }
}

func readWav(_ url: URL, maxFrames: Int = DSP.maxIR * 4) throws -> (samples: [Float], rate: Double) {
    let f = try AVAudioFile(forReading: url)
    let n = AVAudioFrameCount(min(f.length, Int64(maxFrames)))
    guard let buf = AVAudioPCMBuffer(pcmFormat: f.processingFormat, frameCapacity: n) else { throw EngineError(msg: "buffer") }
    try f.read(into: buf, frameCount: n)
    guard let ch = buf.floatChannelData?[0] else { throw EngineError(msg: "not float audio") }
    return (Array(UnsafeBufferPointer(start: ch, count: Int(buf.frameLength))), f.processingFormat.sampleRate)
}
