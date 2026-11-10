import Foundation

/// Diagnostic log (Settings > About > Debug log): a plain-text record of the setup, the audio device, NAM loads,
/// messages and, every 2 s while playing, DSP load, NAM's share of it and dropouts. Off by default; one file per
/// launch in ~/Library/Application Support/Leviathan/Logs, the last 5 kept, each capped at 2 MB. Same format as the
/// Windows/Android app's DebugLog.cpp. Never written from the audio thread: the engine only adds to counters.
enum DebugLog {
    static let dir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("Leviathan/Logs")
    private static let queue = DispatchQueue(label: "leviathan.debuglog")
    nonisolated(unsafe) private static var handle: FileHandle?
    nonisolated(unsafe) private static var file: URL?
    nonisolated(unsafe) private static var full = false
    private static let maxBytes: UInt64 = 2 * 1024 * 1024, keep = 5

    static var enabled: Bool { queue.sync { handle != nil } }

    private static func stamp() -> String {
        let f = DateFormatter(); f.dateFormat = "yyyy-MM-dd HH:mm:ss.SSS"; return f.string(from: Date())
    }

    private static func sysctl(_ name: String) -> String {
        var size = 0
        guard sysctlbyname(name, nil, &size, nil, 0) == 0, size > 0 else { return "" }
        var buf = [CChar](repeating: 0, count: size)
        guard sysctlbyname(name, &buf, &size, nil, 0) == 0 else { return "" }
        return String(cString: buf)
    }

    private static func sysctlInt(_ name: String) -> Int {
        var v: Int64 = 0, size = MemoryLayout<Int64>.size
        return sysctlbyname(name, &v, &size, nil, 0) == 0 ? Int(v) : 0
    }

    /// Turning it on starts a new file with the setup at the top.
    static func setEnabled(_ on: Bool) {
        queue.sync {
            if on == (handle != nil) { return }
            if !on {
                handle?.write(Data("\(stamp())  log    stopped\n".utf8)); try? handle?.close(); handle = nil; return
            }
            let fm = FileManager.default
            try? fm.createDirectory(at: dir, withIntermediateDirectories: true)
            let old = ((try? fm.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil)) ?? [])
                .filter { $0.lastPathComponent.hasPrefix("Leviathan-") && $0.pathExtension == "log" }
                .sorted { $0.lastPathComponent < $1.lastPathComponent }
            for f in old.dropLast(keep - 1) { try? fm.removeItem(at: f) }   // room for the new one
            let df = DateFormatter(); df.dateFormat = "yyyy-MM-dd HH.mm.ss"
            let url = dir.appendingPathComponent("Leviathan-\(df.string(from: Date())).log")
            fm.createFile(atPath: url.path, contents: nil)
            guard let h = try? FileHandle(forWritingTo: url) else { return }
            handle = h; file = url; full = false
            let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "?"
            let p = ProcessInfo.processInfo
            var features: [String] = []
            #if arch(arm64)
            features.append("NEON")
            #else
            if sysctlInt("hw.optional.avx2_0") != 0 { features.append("AVX2") }
            #endif
            let head = """
                Leviathan \(version) diagnostic log
                System:  macOS \(p.operatingSystemVersionString), \(sysctl("hw.model"))
                CPU:     \(sysctl("machdep.cpu.brand_string")), \(sysctlInt("hw.physicalcpu")) cores / \(p.activeProcessorCount) threads, \(features.joined(separator: " "))
                Memory:  \(p.physicalMemory / 1_048_576) MB


                """
            h.write(Data(head.utf8))
        }
    }

    /// The file being written, or the newest one.
    static func latest() -> URL? {
        queue.sync {
            if let file, handle != nil { return file }
            let all = ((try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil)) ?? [])
                .filter { $0.lastPathComponent.hasPrefix("Leviathan-") && $0.pathExtension == "log" }
                .sorted { $0.lastPathComponent < $1.lastPathComponent }
            return all.last
        }
    }

    /// One line: time, topic, text. Thread-safe; does nothing while off.
    static func write(_ topic: String, _ text: String) {
        queue.async {
            guard let h = handle, !full else { return }
            if (try? h.offset()) ?? 0 > maxBytes {
                h.write(Data("\(stamp())  log    full (2 MB): turn the log off and on to start a new one\n".utf8)); full = true; return
            }
            h.write(Data("\(stamp())  \(topic.padding(toLength: 7, withPad: " ", startingAt: 0))\(text)\n".utf8))
        }
    }
}
