import SwiftUI
import Foundation
import AVFoundation
import AppKit
import UniformTypeIdentifiers

enum Screen: Hashable { case rig, presets, recordings, settings }

/// What a session folder holds besides its audio: when, which preset, and the full rig used.
struct SessionMeta: Codable {
    var name: String
    var date: Date
    var duration: Double
    var preset: String
    var settings: Settings?
    var source: String? = nil      // "import" for takes brought in from elsewhere
}

extension SessionMeta {
    /// Reads session.json. The stored rig settings are filled from defaults where an older version didn't have a key,
    /// so adding a setting never hides older recordings (as happened in 0.4.3 when irFile was added).
    static func decode(_ data: Data) -> SessionMeta? {
        if let m = try? JSONDecoder.iso.decode(SessionMeta.self, from: data) { return m }
        guard var obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return nil }
        let settings = obj.removeValue(forKey: "settings")
        guard let rest = try? JSONSerialization.data(withJSONObject: obj),
              var m = try? JSONDecoder.iso.decode(SessionMeta.self, from: rest) else { return nil }
        if let settings, let d = try? JSONSerialization.data(withJSONObject: settings) { m.settings = Settings.lenient(d) }
        return m
    }
}

struct Session: Identifiable, Hashable {
    let id: URL                      // the folder
    var meta: SessionMeta
    var dry: URL { id.appendingPathComponent("dry.wav") }
    var wet: URL { id.appendingPathComponent("wet.wav") }
    static func == (a: Session, b: Session) -> Bool { a.id == b.id }
    func hash(into h: inout Hasher) { h.combine(id) }
}

/// Recorder, session library, playback, live re-amp and offline re-amp.
///
/// Recording taps the engine twice: the raw input (dry, mono) and the final output (wet, stereo).
/// Playback and live re-amp reuse the engine's backing-track and reamp-source slots.
@MainActor
final class Sessions: ObservableObject {
    static let defaultRoot = FileManager.default.urls(for: .musicDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("Leviathan/Sessions")
    /// The folder chosen in Settings > Library ("recordingsFolder"; nil = the default). A cloud drive works:
    /// Google Drive, Dropbox and iCloud Drive mount as folders. Windows: Sessions::chosen.
    static var chosen: URL? {
        get { UserDefaults.standard.string(forKey: "recordingsFolder").flatMap { $0.isEmpty ? nil : URL(fileURLWithPath: $0, isDirectory: true) } }
        set { UserDefaults.standard.set(newValue?.path ?? "", forKey: "recordingsFolder") }
    }
    private static func isFolder(_ u: URL) -> Bool { var d: ObjCBool = false; return FileManager.default.fileExists(atPath: u.path, isDirectory: &d) && d.boolValue }
    /// A chosen folder that isn't there right now (the drive isn't running): takes go to the default meanwhile.
    static var chosenMissing: Bool { chosen.map { !isFolder($0) } ?? false }
    /// Where takes are recorded.
    static var root: URL { if let c = chosen, isFolder(c) { return c }; return defaultRoot }
    @Published var moving = false

    var countTakes: Int {
        let dirs = (try? FileManager.default.contentsOfDirectory(at: Sessions.root, includingPropertiesForKeys: nil)) ?? []
        return dirs.filter { FileManager.default.fileExists(atPath: $0.appendingPathComponent("session.json").path) }.count
    }

    /// Records into `to` (nil: the default) from now on. moveExisting: each take is copied, checked (every file the
    /// same size), then removed from the old folder, and the old folder is deleted once it's empty.
    func changeRoot(to: URL?, moveExisting: Bool) {
        guard !recording, !moving else { message = recording ? "Stop recording before changing the recordings folder." : "Recordings are still being moved."; return }
        let from = Sessions.root, target = to ?? Sessions.defaultRoot
        guard target.standardizedFileURL != from.standardizedFileURL else { return }
        stopPlayback(); stopLiveReamp()
        try? FileManager.default.createDirectory(at: target, withIntermediateDirectories: true)
        let commit = { [weak self] in
            Sessions.chosen = (to == nil || to!.standardizedFileURL == Sessions.defaultRoot.standardizedFileURL) ? nil : to
            self?.selected = nil; self?.loaded = nil
            self?.refresh()
        }
        guard moveExisting else {
            commit()
            message = "New recordings go to \(target.path). Earlier ones stay in \(from.path)."
            return
        }
        let takes = ((try? FileManager.default.contentsOfDirectory(at: from, includingPropertiesForKeys: [.isDirectoryKey])) ?? [])
            .filter { (try? $0.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true }
        moving = true
        message = "Moving recordings\u{2026}"
        Task.detached {
            let fm = FileManager.default
            func sameTree(_ a: URL, _ b: URL) -> Bool {
                guard let e = fm.enumerator(at: a, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey]) else { return false }
                for case let f as URL in e where (try? f.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true {
                    let rel = String(f.path.dropFirst(a.path.count))
                    let s1 = (try? f.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? -1
                    let s2 = (try? b.appendingPathComponent(rel).resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? -2
                    if s1 != s2 { return false }
                }
                return true
            }
            var moved = 0, failed: [String] = []
            for (i, d) in takes.enumerated() {
                await MainActor.run { self.message = "Moving recordings: \(i + 1) of \(takes.count)\u{2026}" }
                var dest = target.appendingPathComponent(d.lastPathComponent)
                var k = 2
                while fm.fileExists(atPath: dest.path) { dest = target.appendingPathComponent("\(d.lastPathComponent) \(k)"); k += 1 }
                let copied = (try? fm.copyItem(at: d, to: dest)) != nil && sameTree(d, dest)
                if copied, (try? fm.removeItem(at: d)) != nil { moved += 1 }
                else { if !copied { try? fm.removeItem(at: dest) }; failed.append(d.lastPathComponent) }
            }
            // clean up: the old folder goes once nothing of Leviathan's is left in it (system clutter aside)
            var removedOld = false
            if failed.isEmpty {
                for junk in [".DS_Store", "desktop.ini", "Thumbs.db"] { try? fm.removeItem(at: from.appendingPathComponent(junk)) }
                let left = (try? fm.contentsOfDirectory(atPath: from.path))?.filter { !$0.hasPrefix(".") } ?? ["?"]
                if left.isEmpty { removedOld = (try? fm.removeItem(at: from)) != nil }
            }
            let summary = "Moved \(moved) recording\(moved == 1 ? "" : "s") to \(target.path)." + (removedOld ? " The old folder was removed." : "")
                + (failed.isEmpty ? "" : " Couldn't move: \(failed.joined(separator: ", ")) (still in \(from.path)).")
            await MainActor.run {
                self.moving = false
                commit()
                self.message = summary
            }
        }
    }

    unowned let m: Model
    @Published var list: [Session] = []
    @Published var selected: URL? { didSet { if selected != oldValue { stopPlayback(); stopLiveReamp(); selection = nil; loadWaveform() } } }
    /// The part of the take selected on the waveform (seconds): Play plays only this, and it's what a trim keeps.
    struct Selection: Equatable { var start: Double, end: Double }
    @Published var selection: Selection?
    @Published var recording = false
    @Published var recSeconds = 0.0
    @Published var playing = false
    @Published var playPos = 0.0
    /// Min/max pairs of the take being recorded (20 per second), for the top bar.
    let liveWave = LiveWave()
    private var liveBin: (lo: Float, hi: Float, n: Int) = (0, 0, 0)
    /// The take last loaded for playback, kept so clicking around the waveform doesn't reload the file.
    private var loaded: (id: URL, rate: Double, l: [Float], r: [Float])?
    @Published var liveReamp = false
    @Published var rendering: Double? = nil      // progress 0...1 while an offline re-amp runs
    @Published var waveform: [Float] = []        // min/max pairs
    @Published var message = ""
    /// As Model.statusFix, for message.
    var messageFix = "", messageFixFor = ""
    func fixIn(_ where_: String) { messageFix = where_; messageFixFor = message }
    var currentFix: String { !message.isEmpty && message == messageFixFor ? messageFix : "" }

    private var dryFile: AVAudioFile?, wetFile: AVAudioFile?, recDir: URL?, recRate = 48000.0, recStart = Date()
    private let tmp = (0..<3).map { _ in UnsafeMutablePointer<Float>.allocate(capacity: 1 << 18) }
    private var playLen = 0, fullLen = 0, playRate = 48000.0

    init(model: Model) {
        m = model
        try? FileManager.default.createDirectory(at: Sessions.root, withIntermediateDirectories: true)
        refresh()
        if Sessions.chosenMissing, let c = Sessions.chosen {   // e.g. a cloud drive that isn't running: record locally for now
            model.status = "Your recordings folder (\(c.path)) isn't available, so new recordings go to \(Sessions.defaultRoot.path) for now. Is the drive connected?"
            model.fixIn("Library")
        }
        // quitting mid-take: finish the recording properly instead of leaving it unfinished
        NotificationCenter.default.addObserver(forName: NSApplication.willTerminateNotification, object: nil, queue: .main) { [weak self] _ in
            MainActor.assumeIsolated { if self?.recording == true { self?.stopRecording() } }
        }
    }

    var current: Session? { list.first { $0.id == selected } }

    func refresh() {
        let dirs = (try? FileManager.default.contentsOfDirectory(at: Sessions.root, includingPropertiesForKeys: nil)) ?? []
        for d in dirs where d != recDir { Sessions.recover(d) }
        list = dirs.compactMap { d in
            guard let data = try? Data(contentsOf: d.appendingPathComponent("session.json")),
                  let meta = SessionMeta.decode(data) else { return nil }
            return Session(id: d, meta: meta)
        }.sorted { $0.meta.date > $1.meta.date }
        if let s = selected, !list.contains(where: { $0.id == s }) { selected = nil }
    }

    /// A date and time written into a file name: 20261104_181844, 20261104_18:18:44, 2026-11-04 18.18.44, ... (local time).
    nonisolated static func dateInName(_ s: String) -> Date? {
        let re = try! NSRegularExpression(pattern: #"(\d{4})[-_.]?(\d{2})[-_.]?(\d{2})[ _T-]+(\d{2})[:.\-/]?(\d{2})(?:[:.\-/]?(\d{2}))?"#)
        guard let m = re.firstMatch(in: s, range: NSRange(s.startIndex..., in: s)) else { return nil }
        func g(_ i: Int) -> Int? { Range(m.range(at: i), in: s).flatMap { Int(s[$0]) } }
        var c = DateComponents()
        c.year = g(1); c.month = g(2); c.day = g(3); c.hour = g(4); c.minute = g(5); c.second = g(6) ?? 0
        guard let y = c.year, (1990...2100).contains(y), let mo = c.month, (1...12).contains(mo), let d = c.day, (1...31).contains(d),
              let h = c.hour, h < 24, let mi = c.minute, mi < 60 else { return nil }
        return Calendar.current.date(from: c)
    }

    /// A take cut off before Stop (the app quit or crashed) has its audio but unfinished WAV headers and no
    /// session.json. Finish the headers and add the session file so it shows up again.
    nonisolated static func recover(_ dir: URL) {
        let fm = FileManager.default
        let meta = dir.appendingPathComponent("session.json"), dry = dir.appendingPathComponent("dry.wav"), wet = dir.appendingPathComponent("wet.wav")
        guard fm.fileExists(atPath: dry.path) || fm.fileExists(atPath: wet.path) else { return }
        let dryDur = fixWavHeader(dry), wetDur = fixWavHeader(wet)
        guard !fm.fileExists(atPath: meta.path) else { return }
        let date = (DateFormatter.folder.date(from: String(dir.lastPathComponent.prefix(19)))) ?? Date()
        let name = "Recovered " + DateFormatter.recovered.string(from: date)
        let m = SessionMeta(name: name, date: date, duration: dryDur ?? wetDur ?? 0, preset: "", settings: nil, source: nil)
        try? JSONEncoder.iso.encode(m).write(to: meta)
    }

    /// Set a WAV file's RIFF and data sizes from its real length (they're 0 if the writer never closed). Returns seconds.
    @discardableResult nonisolated static func fixWavHeader(_ url: URL) -> Double? {
        guard let h = try? FileHandle(forUpdating: url) else { return nil }
        defer { try? h.close() }
        guard let head = try? h.read(upToCount: 65536), head.count > 44, head.prefix(4) == Data("RIFF".utf8),
              let size = try? h.seekToEnd() else { return nil }
        func find(_ tag: String) -> Int? { head.range(of: Data(tag.utf8)).map { $0.lowerBound } }
        guard let d = find("data"), let f = find("fmt "), f + 24 <= head.count else { return nil }
        let u16 = { (o: Int) in head.subdata(in: o..<(o + 2)).withUnsafeBytes { $0.loadUnaligned(as: UInt16.self) } }
        let u32 = { (o: Int) in head.subdata(in: o..<(o + 4)).withUnsafeBytes { $0.loadUnaligned(as: UInt32.self) } }
        let ch = Int(u16(f + 10)), sr = Double(u32(f + 12)), bits = Int(u16(f + 22)), frame = max(ch * bits / 8, 1)
        let actual = (Int(size) - (d + 8)) / frame * frame
        if Int(u32(d + 4)) != actual {
            var riff = UInt32(d + 8 + actual - 8), data = UInt32(actual)
            try? h.seek(toOffset: 4); h.write(Data(bytes: &riff, count: 4))
            try? h.seek(toOffset: UInt64(d + 4)); h.write(Data(bytes: &data, count: 4))
        }
        return sr > 0 ? Double(actual / frame) / sr : nil
    }

    private func save(_ s: Session) {
        try? JSONEncoder.iso.encode(s.meta).write(to: s.id.appendingPathComponent("session.json"))
    }

    // MARK: recording

    func toggleRecord() { recording ? stopRecording() : startRecording() }

    func startRecording() {
        guard !recording else { return }
        guard !moving else { message = "Wait for the recordings to finish moving, then record."; return }
        guard m.live, let dsp = m.engine.dsp else { message = "Turn the amp on to record."; fixIn("amp:on"); return }
        stopPlayback()
        let sr = Double(dsp.sr)
        let stamp = DateFormatter.folder.string(from: Date())
        let dir = Sessions.root.appendingPathComponent(stamp)
        do {
            try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
            dryFile = try AVAudioFile(forWriting: dir.appendingPathComponent("dry.wav"), settings: wavSettings(sr, 1),
                                      commonFormat: .pcmFormatFloat32, interleaved: false)
            wetFile = try AVAudioFile(forWriting: dir.appendingPathComponent("wet.wav"), settings: wavSettings(sr, 2),
                                      commonFormat: .pcmFormatFloat32, interleaved: false)
        } catch {
            message = "Could not start recording: \(error.localizedDescription)"; fixIn("Library"); return
        }
        recDir = dir; recRate = sr; recStart = Date(); recSeconds = 0
        liveWave.reset(); liveBin = (0, 0, 0)
        let p = m.params
        p.recDry.reset(); p.recL.reset(); p.recR.reset()
        p.recording = true
        recording = true
        message = ""
    }

    func stopRecording() {
        guard recording, let dir = recDir else { return }
        m.params.recording = false
        drain()
        let frames = dryFile?.length ?? 0
        dryFile = nil; wetFile = nil; recording = false; recDir = nil
        let n = (list.compactMap { Int($0.meta.name.replacingOccurrences(of: "Session ", with: "")) }.max() ?? 0) + 1
        let s = Session(id: dir, meta: SessionMeta(name: String(format: "Session %02d", n), date: recStart,
                                                   duration: Double(frames) / recRate, preset: m.presetName, settings: m.s))
        save(s)
        refresh()
        selected = s.id
    }

    /// Called ~30x/s: move recorded audio from the engine's FIFOs to disk.
    func tick() {
        if recording { drain(); recSeconds = Date().timeIntervalSince(recStart) }
        let p = m.params
        if playing {
            playPos = Double(p.bkPos) / playRate
            if !p.bkPlaying { playing = false; playPos = 0 }
        }
        if p.srcDone { p.srcDone = false; liveReamp = false }
        // audio started only to play a recording back: turn it off again after 2 s with nothing playing
        // (long enough for play()'s 0.6 s start-up and for pause / play)
        if m.playbackOnly && !playing && !liveReamp && !p.bkPlaying {
            if let t = idleSince { if Date().timeIntervalSince(t) >= 2 { idleSince = nil; m.stop() } } else { idleSince = Date() }
        } else { idleSince = nil }
    }
    private var idleSince: Date?

    private func drain() {
        let p = m.params
        let cap = 1 << 18
        while true {
            let n = min(p.recDry.available, p.recL.available, p.recR.available, cap)
            if n == 0 { break }
            _ = p.recDry.read(into: tmp[0], max: n); _ = p.recL.read(into: tmp[1], max: n); _ = p.recR.read(into: tmp[2], max: n)
            write(dryFile, [tmp[0]], n); write(wetFile, [tmp[1], tmp[2]], n)
            let binLen = max(Int(recRate / 20), 1)
            for i in 0..<n {
                let x = tmp[1][i]
                liveBin.lo = min(liveBin.lo, x); liveBin.hi = max(liveBin.hi, x); liveBin.n += 1
                if liveBin.n >= binLen { liveWave.append(liveBin.lo, liveBin.hi); liveBin = (0, 0, 0) }
            }
        }
    }

    private func write(_ f: AVAudioFile?, _ ch: [UnsafeMutablePointer<Float>], _ n: Int) {
        guard let f, let buf = AVAudioPCMBuffer(pcmFormat: f.processingFormat, frameCapacity: AVAudioFrameCount(n)),
              let d = buf.floatChannelData else { return }
        buf.frameLength = AVAudioFrameCount(n)
        for (i, c) in ch.enumerated() { d[i].update(from: c, count: n) }
        try? f.write(from: buf)
    }

    private func wavSettings(_ sr: Double, _ ch: Int) -> [String: Any] {
        [AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: sr, AVNumberOfChannelsKey: ch,
         AVLinearPCMBitDepthKey: 32, AVLinearPCMIsFloatKey: true, AVLinearPCMIsNonInterleaved: false]
    }

    // MARK: playback (through the engine's backing slot, so it plays on your chosen output)

    func togglePlay() {
        if playing { stopPlayback() } else if let sel = selection { play(from: sel.start, to: sel.end) } else { play() }
    }

    /// Plays from `t`; with `to`, stops there (playing a selection).
    func play(from t: Double = 0, to end: Double? = nil) {
        guard let s = current else { return }
        guard m.running, let dsp = m.engine.dsp else {
            // audio is off: start it, then play from the same point once the engine is up
            if !m.running { m.start(withInput: false); DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) { [weak self] in if self?.m.running == true { self?.play(from: t, to: end) } } }
            return
        }
        stopLiveReamp()
        let sr = Double(dsp.sr)
        let l: [Float], r: [Float]
        if let c = loaded, c.id == s.id, c.rate == sr { l = c.l; r = c.r }
        else {
            guard let ch = try? loadAudio(s.wet, rate: sr), let first = ch.first else { message = "Could not read the recording."; return }
            l = first; r = ch.count > 1 ? ch[1] : first
            loaded = (s.id, sr, l, r)
        }
        let p = m.params
        p.bkPlaying = false
        let oldL = p.bkL, oldR = p.bkR
        let len = end.map { min(max(Int($0 * sr), 1), l.count) } ?? l.count   // the engine stops at bkLen
        p.bkL = copyOut(l); p.bkR = copyOut(r); p.bkLen = len; p.bkLoop = false; p.bkVol = 0
        p.bkPos = min(Int(t * sr), max(len - 1, 0))
        OSMemoryBarrier()
        p.bkPlaying = true
        retire(oldL, oldR)
        playLen = len; fullLen = l.count; playRate = sr; playing = true
    }

    func seek(_ t: Double) {
        // still playing the right range in the engine: just move the playhead; otherwise (stopped, ran off the end, or
        // the selection changed under it) start again from t, up to the end of the selection if there is one
        let want = selection.map { min(max(Int($0.end * playRate), 1), fullLen) } ?? fullLen
        if playing && m.params.bkPlaying && want == playLen { m.params.bkSeek = max(0, min(Int(t * playRate), playLen - 1)); playPos = t }
        else { play(from: t, to: selection?.end) }
    }

    func stopPlayback() {
        m.params.bkPlaying = false
        playing = false
    }

    // MARK: live re-amp: the dry take replaces your guitar, looping, through the live rig

    func toggleLiveReamp() { liveReamp ? stopLiveReamp() : startLiveReamp() }

    func startLiveReamp() {
        guard let s = current else { return }
        guard m.running, let dsp = m.engine.dsp else { message = "Turn the amp on to re-amp."; fixIn("amp:on"); return }
        stopPlayback()
        guard let ch = try? loadAudio(s.dry, rate: Double(dsp.sr)), let x = ch.first else { message = "This session has no dry take."; return }
        let p = m.params
        p.srcPlaying = false
        let old = p.srcPtr
        p.srcPtr = copyOut(x); p.srcLen = x.count; p.srcPos = 0; p.srcLoop = true; p.srcDone = false
        OSMemoryBarrier()
        p.srcPlaying = true
        retire(old, nil)
        liveReamp = true
    }

    func stopLiveReamp() {
        m.params.srcPlaying = false
        liveReamp = false
    }

    // MARK: offline re-amp: render the dry take through the current rig into a new wet take

    func reamp() {
        guard let s = current, rendering == nil else { return }
        let sr = m.engine.dsp.map { Double($0.sr) } ?? 48000
        guard let ch = try? loadAudio(s.dry, rate: sr), let x = ch.first else { message = "This session has no dry take."; return }
        stopPlayback(); stopLiveReamp()
        let rig = m.offlineRig(sr: sr)
        let settings = m.s, preset = m.presetName
        rendering = 0
        let wet = s.wet, backup = s.id.appendingPathComponent("wet-original.wav")
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            let tail = Int(sr * 2)                        // let delay/reverb ring out
            let total = x.count + tail
            var l = [Float](repeating: 0, count: total), r = [Float](repeating: 0, count: total)
            var input = x + [Float](repeating: 0, count: tail)
            let block = 512
            var i = 0
            input.withUnsafeMutableBufferPointer { ip in
                l.withUnsafeMutableBufferPointer { lp in r.withUnsafeMutableBufferPointer { rp in
                    while i < total {
                        let n = min(block, total - i)
                        rig.process(input: ip.baseAddress! + i, outL: lp.baseAddress! + i, outR: rp.baseAddress! + i, frames: n)
                        i += n
                        if i % (block * 400) == 0 {
                            let prog = Double(i) / Double(total)
                            DispatchQueue.main.async { self?.rendering = prog }
                        }
                    }
                }}
            }
            var err: String?
            do {
                let fm = FileManager.default
                if !fm.fileExists(atPath: backup.path), fm.fileExists(atPath: wet.path) { try fm.moveItem(at: wet, to: backup) }
                else if fm.fileExists(atPath: wet.path) { try fm.removeItem(at: wet) }
                let f = try AVAudioFile(forWriting: wet, settings: [AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: sr,
                                                                    AVNumberOfChannelsKey: 2, AVLinearPCMBitDepthKey: 32,
                                                                    AVLinearPCMIsFloatKey: true, AVLinearPCMIsNonInterleaved: false],
                                        commonFormat: .pcmFormatFloat32, interleaved: false)
                let buf = AVAudioPCMBuffer(pcmFormat: f.processingFormat, frameCapacity: AVAudioFrameCount(total))!
                buf.frameLength = AVAudioFrameCount(total)
                buf.floatChannelData![0].update(from: l, count: total)
                buf.floatChannelData![1].update(from: r, count: total)
                try f.write(from: buf)
            } catch { err = error.localizedDescription }
            DispatchQueue.main.async {
                guard let self else { return }
                self.rendering = nil
                if let err { self.message = "Re-amp failed: \(err)"; return }
                if var cur = self.list.first(where: { $0.id == s.id }) {
                    cur.meta.preset = preset; cur.meta.settings = settings
                    self.save(cur); self.refresh()
                }
                self.loadWaveform()
                self.message = "Re-amped with “\(preset)”. The original wet take is kept as wet-original.wav."
            }
        }
    }

    // MARK: manage

    func loadPreset() {
        guard let s = current, let st = s.meta.settings else { return }
        m.s = st; m.presetName = s.meta.preset
    }

    func rename(_ name: String) {
        guard var s = current, !name.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        s.meta.name = name; save(s); refresh()
    }

    /// Move a recording to the Trash (any one, not just the selected).
    func delete(_ id: URL) {
        if id == selected { delete(); return }
        NSWorkspace.shared.recycle([id]) { [weak self] _, _ in DispatchQueue.main.async { self?.refresh() } }
    }

    func delete() {
        guard let s = current else { return }
        stopPlayback(); stopLiveReamp()
        NSWorkspace.shared.recycle([s.id]) { [weak self] _, _ in DispatchQueue.main.async { self?.refresh() } }
        selected = nil
    }

    func reveal() { if let s = current { NSWorkspace.shared.activateFileViewerSelecting([s.wet]) } }

    /// The take to share, named after it ("Riff (amped).wav", not wet.wav): as 16-bit WAV (takes are 32-bit float,
    /// which many players can't play), or for messages as AAC (.m4a), since chat apps like WhatsApp play no WAV.
    /// Made once per take version (cached on the file's modification date). Windows/Android: Sessions::share.
    private var shareCache: [String: URL] = [:]
    func shareURL(_ s: Session, amped: Bool, m4a: Bool = false) -> URL? {
        let src = amped ? s.wet : s.dry
        let fm = FileManager.default
        guard let attrs = try? fm.attributesOfItem(atPath: src.path), let modified = attrs[.modificationDate] as? Date else { return nil }
        let base = s.meta.name.isEmpty ? "Leviathan take" : s.meta.name.replacingOccurrences(of: "/", with: "-").replacingOccurrences(of: ":", with: "-")
        let key = "\(src.path)|\(modified.timeIntervalSince1970)|\(base)|\(m4a)"
        if let u = shareCache[key], fm.fileExists(atPath: u.path) { return u }
        let dir = fm.temporaryDirectory.appendingPathComponent("Share/\(abs(key.hashValue))", isDirectory: true)
        try? fm.createDirectory(at: dir, withIntermediateDirectories: true)
        let out = dir.appendingPathComponent(base + (amped ? " (amped)" : " (raw)") + (m4a ? ".m4a" : ".wav"))
        try? fm.removeItem(at: out)
        do {
            let inF = try AVAudioFile(forReading: src)
            let fmt = inF.processingFormat
            let settings: [String: Any] = m4a
                ? [AVFormatIDKey: kAudioFormatMPEG4AAC, AVSampleRateKey: fmt.sampleRate,
                   AVNumberOfChannelsKey: min(fmt.channelCount, 2), AVEncoderBitRateKey: 192_000]
                : [AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: fmt.sampleRate,
                   AVNumberOfChannelsKey: fmt.channelCount, AVLinearPCMBitDepthKey: 16,
                   AVLinearPCMIsFloatKey: false, AVLinearPCMIsBigEndianKey: false]
            let outF = try AVAudioFile(forWriting: out, settings: settings, commonFormat: fmt.commonFormat, interleaved: fmt.isInterleaved)
            guard let buf = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: 1 << 16) else { return src }
            while inF.framePosition < inF.length {
                try inF.read(into: buf, frameCount: buf.frameCapacity)
                if buf.frameLength == 0 { break }
                try outF.write(from: buf)
            }
        } catch { return src }
        shareCache[key] = out
        return out
    }

    /// Import WAV (or AIFF/CAF) recordings, picked as files or folders. Files named as a pair
    /// (`Dry_<name>` + `Wet_<name>`, or `<name> dry` + `<name> wet`) become one take; any other file becomes a
    /// take of its own, treated as a dry DI recording so it can be re-amped through the rig.
    func importRecordings() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = true; panel.allowsMultipleSelection = true
        // .folder too: with only audio types listed, macOS greys out folders and "Open" can't be pressed
        panel.allowedContentTypes = [.wav, .aiff, .audio, .folder]
        panel.message = "Choose WAV recordings or folders of them"
        panel.prompt = "Import"
        guard panel.runModal() == .OK else { return }
        let audioExt: Set<String> = ["wav", "wave", "aif", "aiff", "caf"]
        var files: [URL] = []
        for u in panel.urls {
            if (try? u.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true {
                files += ((try? FileManager.default.contentsOfDirectory(at: u, includingPropertiesForKeys: nil)) ?? [])
            } else { files.append(u) }
        }
        files = files.filter { audioExt.contains($0.pathExtension.lowercased()) }

        func split(_ u: URL) -> (name: String, tag: String)? {
            let n = u.deletingPathExtension().lastPathComponent, low = n.lowercased()
            for tag in ["dry", "wet"] {
                for pre in ["\(tag)_", "\(tag) ", "\(tag)-"] where low.hasPrefix(pre) { return (String(n.dropFirst(pre.count)), tag) }
                for suf in [" \(tag)", "_\(tag)", "-\(tag)"] where low.hasSuffix(suf) { return (String(n.dropLast(suf.count)), tag) }
            }
            return nil
        }
        var dries: [String: URL] = [:], wets: [String: URL] = [:], singles: [(String, URL)] = []
        for f in files {
            if let (n, t) = split(f) { if t == "dry" { dries[n] = f } else { wets[n] = f } }
            else { singles.append((f.deletingPathExtension().lastPathComponent, f)) }
        }
        // a dry or wet file without its partner is just a single take
        for (n, f) in dries where wets[n] == nil { singles.append((n, f)); dries[n] = nil }
        for (n, f) in wets where dries[n] == nil { singles.append((n, f)); wets[n] = nil }

        let have = Set(list.map { $0.meta.name })
        var added = 0
        var importedFrom: [URL] = []
        func add(_ name: String, dry: URL, wet: URL) {
            guard !have.contains(name) else { return }
            // the date in the file name (e.g. Dry_20261104_18:18:44), else when the file was last changed
            let date = Sessions.dateInName(name) ?? Sessions.dateInName(wet.deletingPathExtension().lastPathComponent)
                ?? (try? wet.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? Date()
            let dir = Sessions.root.appendingPathComponent(DateFormatter.folder.string(from: date) + " " + name.replacingOccurrences(of: "/", with: "-"))
            do {
                try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
                // stored as WAV whatever came in, so every take in the library is the same format
                try writeWav(from: dry, to: dir.appendingPathComponent("dry.wav"))
                try writeWav(from: wet, to: dir.appendingPathComponent("wet.wav"))
                let dur = (try? AVAudioFile(forReading: dir.appendingPathComponent("wet.wav"))).map { Double($0.length) / $0.processingFormat.sampleRate } ?? 0
                save(Session(id: dir, meta: SessionMeta(name: name, date: date, duration: dur, preset: "", settings: nil, source: "import")))
                added += 1
                importedFrom += dry == wet ? [dry] : [dry, wet]
            } catch {}
        }
        for (n, d) in dries.sorted(by: { $0.key < $1.key }) { if let w = wets[n] { add(n, dry: d, wet: w) } }
        for (n, f) in singles.sorted(by: { $0.0 < $1.0 }) { add(n, dry: f, wet: f) }
        refresh()
        message = added == 0 ? "Nothing new to import." : "Imported \(added) recording\(added == 1 ? "" : "s")."
        if !importedFrom.isEmpty { offerToRemoveOriginals(importedFrom) }
    }

    /// After an import, offer to move the original files to the Trash so the takes live only in Leviathan.
    /// Trash (not delete), so they can be put back; another app (AmpKit, a DAW) may still expect them where they were.
    private func offerToRemoveOriginals(_ files: [URL]) {
        let a = NSAlert()
        a.messageText = "Move the \(files.count) original file\(files.count == 1 ? "" : "s") to the Trash?"
        a.informativeText = "Copies are now in Leviathan's recordings. Moving the originals to the Trash leaves them only in Leviathan; you can put them back from the Trash. If another app (such as AmpKit) keeps its recordings in that folder, it may no longer find them."
        a.addButton(withTitle: "Keep Originals")
        a.addButton(withTitle: "Move to Trash")
        guard a.runModal() == .alertSecondButtonReturn else { return }
        NSWorkspace.shared.recycle(files) { [weak self] moved, err in
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    self?.message = err == nil ? "Imported, and moved \(moved.count) original file\(moved.count == 1 ? "" : "s") to the Trash."
                                                : "Imported, but couldn't move the originals: \(err!.localizedDescription)"
                }
            }
        }
    }

    private func writeWav(from src: URL, to dst: URL) throws {
        try? FileManager.default.removeItem(at: dst)
        if src.pathExtension.lowercased().hasPrefix("wav") { try FileManager.default.copyItem(at: src, to: dst); return }
        let inFile = try AVAudioFile(forReading: src)
        let fmt = inFile.processingFormat
        let out = try AVAudioFile(forWriting: dst, settings: [AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: fmt.sampleRate,
                                                               AVNumberOfChannelsKey: fmt.channelCount, AVLinearPCMBitDepthKey: 32,
                                                               AVLinearPCMIsFloatKey: true, AVLinearPCMIsNonInterleaved: false],
                                  commonFormat: .pcmFormatFloat32, interleaved: false)
        guard let buf = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: 65536) else { return }
        while inFile.framePosition < inFile.length { try inFile.read(into: buf); if buf.frameLength == 0 { break }; try out.write(from: buf) }
    }

    // MARK: helpers

    // MARK: trim

    /// Where the first trim keeps the untouched audio, so a trim can always be undone.
    func untrimmedDir(_ s: Session) -> URL { s.id.appendingPathComponent("untrimmed") }
    var canRestoreUntrimmed: Bool { current.map { FileManager.default.fileExists(atPath: untrimmedDir($0).path) } ?? false }

    /// Cuts the take (dry, wet and any wet-original) to `start...end` seconds ("Trim to selection").
    func trim(_ start: Double, _ end: Double) { trimTake(start, end, keep: true) }
    /// Cuts `start...end` out of the take, joining what's either side with a 5 ms crossfade ("Trim selection").
    func cut(_ start: Double, _ end: Double) { trimTake(start, end, keep: false) }

    private func trimTake(_ start: Double, _ end: Double, keep: Bool) {
        guard var s = current, end - start >= 0.05 else { return }
        stopPlayback(); stopLiveReamp()
        do {
            if let length = try Sessions.trimFiles(in: s.id, start: start, end: end, keep: keep) { s.meta.duration = length }
            save(s)
        } catch {
            message = "Couldn't trim this recording: \(error.localizedDescription)"
            return
        }
        loaded = nil
        refresh(); loadWaveform()
    }

    /// Cuts a session folder's dry.wav, wet.wav and wet-original.wav to `start...end` seconds, keeping the untouched
    /// audio in untrimmed/ on the first trim. Returns the new length (from wet.wav when there is one).
    /// keep: leave only `start...end`; otherwise remove it (Windows: Sessions::trimFiles, same order of operations).
    nonisolated static func trimFiles(in dir: URL, start: Double, end: Double, keep: Bool = true) throws -> Double? {
        let fm = FileManager.default, backup = dir.appendingPathComponent("untrimmed")
        let files = ["dry.wav", "wet.wav", "wet-original.wav"].map { dir.appendingPathComponent($0) }.filter { fm.fileExists(atPath: $0.path) }
        if !fm.fileExists(atPath: backup.path) {
            try fm.createDirectory(at: backup, withIntermediateDirectories: true)
            for f in files { try fm.copyItem(at: f, to: backup.appendingPathComponent(f.lastPathComponent)) }
        }
        var length: Double?
        for f in files {
            let sr: Double, frames: AVAudioFramePosition
            let tmpURL = dir.appendingPathComponent(".trim-" + f.lastPathComponent)
            try? fm.removeItem(at: tmpURL)
            do {
                let inF = try AVAudioFile(forReading: f)
                sr = inF.processingFormat.sampleRate
                let total = inF.length
                let a = min(max(AVAudioFramePosition(start * sr), 0), total), b = min(max(AVAudioFramePosition(end * sr), a), total)
                guard b > a else { continue }
                let out = try AVAudioFile(forWriting: tmpURL, settings: inF.fileFormat.settings,
                                          commonFormat: inF.processingFormat.commonFormat, interleaved: inF.processingFormat.isInterleaved)
                if keep {
                    frames = b - a
                    guard let buf = AVAudioPCMBuffer(pcmFormat: inF.processingFormat, frameCapacity: AVAudioFrameCount(frames)) else { continue }
                    inF.framePosition = a
                    try inF.read(into: buf, frameCount: AVAudioFrameCount(frames))
                    try out.write(from: buf)
                } else {
                    // before a, then after b, crossfaded over xf frames so the join doesn't click
                    let xf = min(AVAudioFramePosition(0.005 * sr), a, total - b)
                    frames = a + (total - b) - xf
                    guard frames > 0,
                          let all = AVAudioPCMBuffer(pcmFormat: inF.processingFormat, frameCapacity: AVAudioFrameCount(total)),
                          let buf = AVAudioPCMBuffer(pcmFormat: inF.processingFormat, frameCapacity: AVAudioFrameCount(frames)) else { continue }
                    try inF.read(into: all, frameCount: AVAudioFrameCount(total))
                    guard let src = all.floatChannelData, let dst = buf.floatChannelData else { continue }
                    let A = Int(a), B = Int(b), X = Int(xf), N = Int(total)
                    for c in 0..<Int(inF.processingFormat.channelCount) {
                        let i = src[c], o = dst[c]
                        for k in 0..<(A - X) { o[k] = i[k] }
                        for k in 0..<X { let t = Float(k + 1) / Float(X + 1); o[A - X + k] = i[A - X + k] * (1 - t) + i[B + k] * t }
                        for k in (B + X)..<N { o[A + k - B - X] = i[k] }
                    }
                    buf.frameLength = AVAudioFrameCount(frames)
                    try out.write(from: buf)
                }
            }   // both files close here
            _ = try fm.replaceItemAt(f, withItemAt: tmpURL)
            if f.lastPathComponent == "wet.wav" || length == nil { length = Double(frames) / sr }
        }
        return length
    }

    /// Puts back the audio from before the first trim.
    func restoreUntrimmed() {
        guard var s = current else { return }
        stopPlayback(); stopLiveReamp()
        let fm = FileManager.default, backup = untrimmedDir(s)
        guard let saved = try? fm.contentsOfDirectory(at: backup, includingPropertiesForKeys: nil), !saved.isEmpty else { return }
        for f in saved where f.pathExtension.lowercased() == "wav" {
            _ = try? fm.replaceItemAt(s.id.appendingPathComponent(f.lastPathComponent), withItemAt: f)
        }
        try? fm.removeItem(at: backup)
        let main = fm.fileExists(atPath: s.wet.path) ? s.wet : s.dry
        if let f = try? AVAudioFile(forReading: main) { s.meta.duration = Double(f.length) / f.processingFormat.sampleRate }
        save(s)
        loaded = nil
        refresh(); loadWaveform()
    }

    private func loadWaveform() {
        waveform = []
        guard let s = current else { return }
        let url = FileManager.default.fileExists(atPath: s.wet.path) ? s.wet : s.dry
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let ch = try? loadAudio(url, rate: nil), let x = ch.first else { return }
            let buckets = 1500, per = max(x.count / buckets, 1)
            var w = [Float](); w.reserveCapacity(buckets * 2)
            var i = 0
            while i < x.count {
                let e = min(i + per, x.count)
                var lo: Float = 0, hi: Float = 0
                for k in i..<e { lo = min(lo, x[k]); hi = max(hi, x[k]) }
                w.append(lo); w.append(hi); i = e
            }
            DispatchQueue.main.async { if self?.current?.id == s.id { self?.waveform = w } }
        }
    }

    private func copyOut(_ x: [Float]) -> UnsafeMutablePointer<Float> {
        let p = UnsafeMutablePointer<Float>.allocate(capacity: max(x.count, 1))
        x.withUnsafeBufferPointer { p.update(from: $0.baseAddress!, count: x.count) }
        return p
    }

    /// Free swapped-out engine buffers once the audio thread can no longer be reading them.
    private func retire(_ a: UnsafeMutablePointer<Float>?, _ b: UnsafeMutablePointer<Float>?) {
        let pa = a.map { UInt(bitPattern: $0) }, pb = b.map { UInt(bitPattern: $0) }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
            pa.flatMap { UnsafeMutablePointer<Float>(bitPattern: $0) }?.deallocate()
            pb.flatMap { UnsafeMutablePointer<Float>(bitPattern: $0) }?.deallocate()
        }
    }
}

/// Read an audio file as float channels, converted to `rate` (or left at its own rate when nil).
func loadAudio(_ url: URL, rate: Double?, maxSeconds: Double = 900) throws -> [[Float]] {
    let f = try AVAudioFile(forReading: url)
    let inFmt = f.processingFormat
    let frames = AVAudioFrameCount(min(f.length, Int64(inFmt.sampleRate * maxSeconds)))
    guard frames > 0, let src = AVAudioPCMBuffer(pcmFormat: inFmt, frameCapacity: frames) else { return [] }
    try f.read(into: src, frameCount: frames)
    let ch = Int(inFmt.channelCount)
    let target = rate ?? inFmt.sampleRate
    var buf = src
    if abs(target - inFmt.sampleRate) > 0.5 {
        guard let outFmt = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: target, channels: AVAudioChannelCount(ch), interleaved: false),
              let conv = AVAudioConverter(from: inFmt, to: outFmt),
              let dst = AVAudioPCMBuffer(pcmFormat: outFmt, frameCapacity: AVAudioFrameCount(Double(frames) * target / inFmt.sampleRate) + 4096)
        else { throw EngineError(msg: "converter") }
        var fed = false
        var err: NSError?
        _ = conv.convert(to: dst, error: &err) { _, status in
            if fed { status.pointee = .endOfStream; return nil }
            fed = true; status.pointee = .haveData; return src
        }
        if let err { throw err }
        buf = dst
    }
    guard let d = buf.floatChannelData else { throw EngineError(msg: "not float audio") }
    return (0..<ch).map { Array(UnsafeBufferPointer(start: d[$0], count: Int(buf.frameLength))) }
}

extension DateFormatter {
    static let folder: DateFormatter = { let f = DateFormatter(); f.dateFormat = "yyyy-MM-dd HH.mm.ss"; return f }()
    static let nice: DateFormatter = { let f = DateFormatter(); f.dateStyle = .medium; f.timeStyle = .short; return f }()
    static let recovered: DateFormatter = { let f = DateFormatter(); f.dateFormat = "MMM d HH:mm"; return f }()
}

extension JSONEncoder { static let iso: JSONEncoder = { let e = JSONEncoder(); e.dateEncodingStrategy = .iso8601; e.outputFormatting = .prettyPrinted; return e }() }
extension JSONDecoder { static let iso: JSONDecoder = { let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601; return d }() }

func mmss(_ t: Double) -> String {
    let s = max(0, Int(t.rounded(.down)))
    return String(format: "%d:%02d", s / 60, s % 60)
}


/// Waveform of the take being recorded, for the top bar.
@MainActor
final class LiveWave: ObservableObject {
    @Published private(set) var pairs: [Float] = []
    func reset() { pairs = [] }
    func append(_ lo: Float, _ hi: Float) { pairs.append(lo); pairs.append(hi) }
}

/// The whole take so far, squeezed to fit, growing from the left; the newest audio is always at the right edge.
struct LiveWaveView: View {
    @ObservedObject var wave: LiveWave
    var color: Color
    var body: some View {
        Canvas { ctx, size in
            ctx.fill(Path(roundedRect: CGRect(origin: .zero, size: size), cornerRadius: 5), with: .color(Color(white: 0.1)))
            let n = wave.pairs.count / 2
            guard n > 0 else { return }
            let cols = max(Int(size.width / 2), 1), per = max(Double(n) / Double(cols), 1)
            let used = min(cols, n), mid = size.height / 2, amp = size.height / 2 - 3
            var path = Path()
            for c in 0..<used {
                let a = Int(Double(c) * per), b = max(Int(Double(c + 1) * per), a + 1)
                var lo: Float = 0, hi: Float = 0
                for i in a..<min(b, n) { lo = min(lo, wave.pairs[2 * i]); hi = max(hi, wave.pairs[2 * i + 1]) }
                let x = CGFloat(c) * 2 + 1
                path.addRect(CGRect(x: x, y: mid - CGFloat(hi) * amp, width: 1.5, height: max(CGFloat(hi - lo) * amp, 1)))
            }
            ctx.fill(path, with: .color(color.opacity(0.9)))
        }
    }
}
