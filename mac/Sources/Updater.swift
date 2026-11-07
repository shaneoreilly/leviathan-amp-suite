import SwiftUI
import AppKit
import CryptoKit

// MARK: - Self-update from GitHub releases
//
// Checks the latest release, downloads its DMG, checks it against the release's .sha256, copies the app out,
// checks its code signature (same bundle id; same Team ID when this copy is Developer ID signed), swaps the
// bundle in place and relaunches. No new DMG to open, nothing to drag.

@MainActor
final class Updater: ObservableObject {
    static let repo = "shaneoreilly/leviathan-amp-suite"

    struct Release: Equatable {
        let version: String, notes: String, dmg: URL, sha: URL?, page: URL
    }

    enum Phase: Equatable {
        case idle, checking, upToDate, available(Release), downloading(Double), installing, failed(String)
    }

    @Published var phase: Phase = .idle
    /// The What's new popup is open.
    @Published var showNotes = false
    @Published var autoCheck = UserDefaults.standard.object(forKey: "autoUpdateCheck") as? Bool ?? true {
        didSet { UserDefaults.standard.set(autoCheck, forKey: "autoUpdateCheck") }
    }
    private var progressObs: NSKeyValueObservation?

    let current = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0"
    var available: Release? { if case .available(let r) = phase { return r }; return nil }
    var isBusy: Bool {
        switch phase { case .checking, .downloading, .installing: return true; default: return false }
    }

    /// At launch, at most once every 12 hours.
    /// The update banner was closed for this version (it comes back for the next one).
    @Published var dismissedVersion = UserDefaults.standard.string(forKey: "updateDismissed") ?? "" {
        didSet { UserDefaults.standard.set(dismissedVersion, forKey: "updateDismissed") }
    }
    var showBanner: Bool { if let r = available { return r.version != dismissedVersion }; return false }
    private var periodic: Timer?

    /// Check at every launch, then every 6 hours while running (each copy checks for itself).
    func checkOnLaunch() {
        guard autoCheck else { return }
        Task {
            await check(quiet: true)
            // LEVIATHAN_SHOW_NOTES=1 opens What's new once an update is found (UI testing)
            if ProcessInfo.processInfo.environment["LEVIATHAN_SHOW_NOTES"] != nil, available != nil { showNotes = true }
        }
        periodic = Timer.scheduledTimer(withTimeInterval: 6 * 3600, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated {
                // not while an update is already known, downloading or installing
                guard let self, self.autoCheck, self.available == nil, !self.isBusy else { return }
                Task { await self.check(quiet: true) }
            }
        }
    }

    func check(quiet: Bool = false) async {
        phase = .checking
        do {
            var req = URLRequest(url: URL(string: "https://api.github.com/repos/\(Updater.repo)/releases/latest")!)
            req.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
            req.setValue("Leviathan-Updater", forHTTPHeaderField: "User-Agent")
            let (data, resp) = try await URLSession.shared.data(for: req)
            guard (resp as? HTTPURLResponse)?.statusCode == 200 else { throw Fail("GitHub answered \((resp as? HTTPURLResponse)?.statusCode ?? 0)") }
            let r = try JSONDecoder().decode(GHRelease.self, from: data)
            UserDefaults.standard.set(Date(), forKey: "lastUpdateCheck")
            let v = r.tag_name.hasPrefix("v") ? String(r.tag_name.dropFirst()) : r.tag_name
            guard Updater.newer(v, than: current) else { phase = .upToDate; return }
            guard let dmg = r.assets.first(where: { $0.name.hasSuffix(".dmg") }) else { throw Fail("Release \(v) has no DMG") }
            let sha = r.assets.first { $0.name == dmg.name + ".sha256" }
            let notes = await Updater.notesSince(current, upTo: v) ?? Updater.released(r.published_at) + Updater.playerNotes(r.body ?? "")
            phase = .available(Release(version: v, notes: notes, dmg: dmg.browser_download_url, sha: sha?.browser_download_url,
                                       page: r.html_url))
        } catch {
            phase = quiet ? .idle : .failed("Couldn't check for updates: \(error.localizedDescription)")
        }
    }

    /// Download, verify, replace this app, relaunch.
    func install(_ r: Release) {
        let app = Bundle.main.bundleURL
        if app.path.contains("/AppTranslocation/") {
            phase = .failed("macOS is running Leviathan from a temporary location. Move Leviathan to Applications, open it from there, then update.")
            return
        }
        guard FileManager.default.isWritableFile(atPath: app.deletingLastPathComponent().path) else {
            phase = .failed("Can't write to \(app.deletingLastPathComponent().path). Download the update from GitHub instead.")
            return
        }
        phase = .downloading(0)
        let task = URLSession.shared.downloadTask(with: r.dmg) { [weak self] tmp, resp, err in
            // the temp file is deleted when this returns: move it somewhere we own first
            var kept: URL?
            if let tmp, (resp as? HTTPURLResponse)?.statusCode == 200 {
                let dest = FileManager.default.temporaryDirectory.appendingPathComponent("Leviathan-update-\(UUID().uuidString).dmg")
                if (try? FileManager.default.moveItem(at: tmp, to: dest)) != nil { kept = dest }
            }
            let message = err?.localizedDescription ?? "download failed"
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    guard let self else { return }
                    self.progressObs = nil
                    guard let kept else { self.phase = .failed("Update \(message)."); return }
                    self.phase = .installing
                    Task.detached { await self.finish(r, dmg: kept, app: app) }
                }
            }
        }
        progressObs = task.progress.observe(\.fractionCompleted) { [weak self] p, _ in
            let f = p.fractionCompleted
            DispatchQueue.main.async { MainActor.assumeIsolated { if case .downloading = self?.phase { self?.phase = .downloading(f) } } }
        }
        task.resume()
    }

    /// Off the main thread: verify and swap. On success the app quits and the new one opens.
    nonisolated private func finish(_ r: Release, dmg: URL, app: URL) async {
        let fm = FileManager.default
        var mount: URL?
        // runs before the relaunch too: the app quits right after, so a defer alone would never get there
        func cleanUp() {
            if let m = mount { _ = try? Updater.run("/usr/bin/hdiutil", ["detach", m.path, "-quiet", "-force"]); try? fm.removeItem(at: m) }
            mount = nil
            try? fm.removeItem(at: dmg)
        }
        defer { cleanUp() }
        do {
            if let shaURL = r.sha {
                let (d, _) = try await URLSession.shared.data(from: shaURL)
                let want = String(decoding: d, as: UTF8.self).split(whereSeparator: \.isWhitespace).first.map(String.init)?.lowercased() ?? ""
                let got = SHA256.hash(data: try Data(contentsOf: dmg, options: .mappedIfSafe)).map { String(format: "%02x", $0) }.joined()
                guard want.count == 64, want == got else { throw Fail("The download didn't match its checksum") }
            }
            // mount, copy the app next to the installed one (same volume, so the swap is a rename)
            let mnt = fm.temporaryDirectory.appendingPathComponent("leviathan-mnt-\(UUID().uuidString)")
            try fm.createDirectory(at: mnt, withIntermediateDirectories: true)
            try Updater.run("/usr/bin/hdiutil", ["attach", dmg.path, "-nobrowse", "-readonly", "-noautoopen", "-quiet", "-mountpoint", mnt.path])
            mount = mnt
            guard let name = try fm.contentsOfDirectory(atPath: mnt.path).first(where: { $0.hasSuffix(".app") }) else { throw Fail("No app in the DMG") }
            let stage = try fm.url(for: .itemReplacementDirectory, in: .userDomainMask, appropriateFor: app, create: true)
            let fresh = stage.appendingPathComponent(name)
            try Updater.run("/usr/bin/ditto", [mnt.appendingPathComponent(name).path, fresh.path])
            _ = try? Updater.run("/usr/bin/xattr", ["-dr", "com.apple.quarantine", fresh.path])

            // same app, intact signature, same developer
            guard Bundle(url: fresh)?.bundleIdentifier == Bundle.main.bundleIdentifier else { throw Fail("The download is a different app") }
            try Updater.run("/usr/bin/codesign", ["--verify", "--deep", "--strict", fresh.path])
            if let team = Updater.teamID(app), Updater.teamID(fresh) != team { throw Fail("The download isn't signed by the same developer") }

            _ = try fm.replaceItemAt(app, withItemAt: fresh)
            try? fm.removeItem(at: stage)
            cleanUp()
            await MainActor.run { Updater.relaunch(app) }
        } catch {
            let msg = (error as? Fail)?.message ?? error.localizedDescription
            await MainActor.run { self.phase = .failed("Update failed: \(msg). Nothing was changed.") }
        }
    }

    private static func relaunch(_ app: URL) {
        let pid = ProcessInfo.processInfo.processIdentifier
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/bin/sh")
        p.arguments = ["-c", "while kill -0 \(pid) 2>/dev/null; do sleep 0.2; done; /usr/bin/open \"$0\"", app.path]
        try? p.run()
        NSApp.terminate(nil)
    }

    // MARK: helpers

    struct Fail: Error { let message: String; init(_ m: String) { message = m } }

    private struct GHRelease: Decodable {
        let tag_name: String, body: String?, html_url: URL, assets: [Asset]
        let draft: Bool?, prerelease: Bool?, published_at: String?
        struct Asset: Decodable { let name: String, browser_download_url: URL }
        var version: String { tag_name.hasPrefix("v") ? String(tag_name.dropFirst()) : tag_name }
    }

    /// Notes for every release after `current` up to `latest`, newest first, each under its version. nil when GitHub
    /// can't be reached or only one release is new (then the latest release's own notes are used).
    private static func notesSince(_ current: String, upTo latest: String) async -> String? {
        var req = URLRequest(url: URL(string: "https://api.github.com/repos/\(repo)/releases?per_page=30")!)
        req.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        req.setValue("Leviathan-Updater", forHTTPHeaderField: "User-Agent")
        guard let (data, resp) = try? await URLSession.shared.data(for: req), (resp as? HTTPURLResponse)?.statusCode == 200,
              let all = try? JSONDecoder().decode([GHRelease].self, from: data) else { return nil }
        let new = all.filter { $0.draft != true && $0.prerelease != true && newer($0.version, than: current) && !newer($0.version, than: latest) }
            .sorted { newer($0.version, than: $1.version) }
        guard new.count > 1 else { return nil }
        return new.map { "# Leviathan \($0.version)\n\n" + released($0.published_at) + playerNotes($0.body ?? "") }.joined(separator: "\n\n")
    }

    /// "> Released on 2026-11-06 06:36 PM NYC" and a blank line (a grey note in the popup); empty without a date.
    nonisolated static func released(_ iso: String?) -> String {
        guard let iso, let d = ISO8601DateFormatter().date(from: iso), let ny = TimeZone(identifier: "America/New_York") else { return "" }
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX"); f.timeZone = ny; f.dateFormat = "yyyy-MM-dd hh:mm a"
        return "> Released on \(f.string(from: d)) NYC\n\n"
    }

    /// Release notes as players see them in the app: without the Downloads section, the "click Update" hint
    /// and the GitHub changelog link, which only make sense on the release page.
    nonisolated static func playerNotes(_ body: String) -> String {
        var out: [String] = [], skipping = false
        for line in body.replacingOccurrences(of: "\r\n", with: "\n").components(separatedBy: "\n") {
            let t = line.trimmingCharacters(in: .whitespaces)
            if t.hasPrefix("#") {
                skipping = t.drop(while: { $0 == "#" }).trimmingCharacters(in: .whitespaces).lowercased() == "downloads"
                if skipping { continue }
            }
            if skipping || t.hasPrefix("**Already on") || t.hasPrefix("**Full Changelog") { continue }
            out.append(line)
        }
        return out.joined(separator: "\n").trimmingCharacters(in: .whitespacesAndNewlines)
    }

    /// "0.1.10" > "0.1.9"
    nonisolated static func newer(_ a: String, than b: String) -> Bool {
        let x = a.split(separator: ".").map { Int($0) ?? 0 }, y = b.split(separator: ".").map { Int($0) ?? 0 }
        for i in 0..<max(x.count, y.count) {
            let p = i < x.count ? x[i] : 0, q = i < y.count ? y[i] : 0
            if p != q { return p > q }
        }
        return false
    }

    /// Developer ID Team ID of a signed bundle, nil for ad-hoc / local signatures.
    nonisolated static func teamID(_ app: URL) -> String? {
        guard let out = try? run("/usr/bin/codesign", ["-dv", app.path]) else { return nil }
        let line = out.split(separator: "\n").first { $0.hasPrefix("TeamIdentifier=") }
        let id = line.map { String($0.dropFirst("TeamIdentifier=".count)) }
        return id == nil || id == "not set" ? nil : id
    }

    /// Runs a tool, returns stdout+stderr, throws on a non-zero exit.
    @discardableResult nonisolated static func run(_ exe: String, _ args: [String]) throws -> String {
        let p = Process(), pipe = Pipe()
        p.executableURL = URL(fileURLWithPath: exe); p.arguments = args
        p.standardOutput = pipe; p.standardError = pipe
        try p.run()
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        p.waitUntilExit()
        let out = String(decoding: data, as: UTF8.self)
        guard p.terminationStatus == 0 else { throw Fail("\((exe as NSString).lastPathComponent) failed: \(out.trimmingCharacters(in: .whitespacesAndNewlines))") }
        return out
    }
}

/// Settings > About: version and updates.
struct UpdateSettings: View {
    @EnvironmentObject var up: Updater

    var body: some View {
        SettingsHeading("Updates", first: true)
        SettingsRow("Version", explain: "settings:version") {
            Text(up.current).monospacedDigit().fontWeight(.semibold)
            Button("Check now") { Task { await up.check() } }.disabled(busy)
            status
        }
        SettingsRow("Automatic", explain: "settings:autoUpdate") {
            Toggle("Check for updates when Leviathan opens", isOn: $up.autoCheck)
        }
        if let r = up.available {
            SettingsRow("Available", explain: "settings:version") {
                Text("Leviathan \(r.version)").fontWeight(.semibold)
                Button("What\u{2019}s new") { up.showNotes = true }
                Button("Update now") { up.install(r) }.buttonStyle(.borderedProminent)
                Button("View on GitHub") { NSWorkspace.shared.open(r.page) }
            }
        }
    }

    var busy: Bool {
        switch up.phase { case .checking, .downloading, .installing: true; default: false }
    }

    @ViewBuilder var status: some View {
        switch up.phase {
        case .checking: ProgressView().controlSize(.small)
        case .upToDate: Text("Up to date").foregroundStyle(.secondary)
        case .downloading(let f): ProgressView(value: f).frame(width: 140); Text("Downloading…").foregroundStyle(.secondary)
        case .installing: ProgressView().controlSize(.small); Text("Installing…").foregroundStyle(.secondary)
        case .failed(let m): Text(m).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
        default: EmptyView()
        }
    }
}

/// "What's new": the release notes, then Update Now or Ignore (hides this version until the next one).
struct WhatsNewSheet: View {
    @ObservedObject var updater: Updater
    let release: Updater.Release

    var body: some View {
        VStack(spacing: 0) {
            HStack(alignment: .top, spacing: 12) {
                Image(systemName: "arrow.down.circle.fill").font(.system(size: 26 * Z)).foregroundStyle(.tint)
                VStack(alignment: .leading, spacing: 2) {
                    Text("What\u{2019}s new in Leviathan \(release.version)").font(.system(size: 17 * UI, weight: .semibold))
                    Text("You have \(updater.current). Updating replaces this copy and reopens Leviathan.")
                        .font(.system(size: 11 * UI)).foregroundStyle(.secondary)
                }
                Spacer()
                Button { updater.showNotes = false } label: { Image(systemName: "xmark") }
                    .buttonStyle(.borderless).keyboardShortcut(.cancelAction).help("Close")
            }
            .padding(20 * Z)
            Rectangle().fill(line).frame(height: 1)
            ScrollView {
                if release.notes.isEmpty {
                    Text("No notes for this release.").foregroundStyle(.secondary).frame(maxWidth: .infinity, alignment: .leading)
                } else {
                    MarkdownView(release.notes, paragraphsAsBullets: true)
                }
            }
            .padding(.horizontal, 20 * Z).padding(.vertical, 14 * Z)
            .frame(minHeight: 240 * Z, idealHeight: 460 * Z, maxHeight: 620 * Z)
            Rectangle().fill(line).frame(height: 1)
            HStack {
                Spacer()
                Button("Ignore") { updater.dismissedVersion = release.version; updater.showNotes = false }
                    .help("Hide this update until the next version (Settings → About still has it)")
                Button("Update Now") { updater.showNotes = false; updater.install(release) }
                    .buttonStyle(.borderedProminent).keyboardShortcut(.defaultAction)
            }
            .padding(16 * Z)
        }
        .frame(width: 600 * Z)
        .controlSize(.large)
        .background(Color(white: 0.07))
    }
}
