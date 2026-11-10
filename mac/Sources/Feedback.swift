import SwiftUI
import AppKit

// MARK: - Bug reports and feature requests (GitHub issue forms, prefilled; you see and edit them before submitting)

@MainActor
enum Feedback {
    static func open(_ template: String, model m: Model) {
        let v = ProcessInfo.processInfo.operatingSystemVersion
        var audio = ""
        if let i = m.input, let o = m.output {
            audio = "\(i.name) in, \(o.name) out"
            if let info = m.info { audio += String(format: ", %.0f kHz, %d samples, ~%.1f ms", info.sampleRate / 1000, info.buffer, info.latencyMs) }
        }
        var c = URLComponents(string: "https://github.com/\(Updater.repo)/issues/new")!
        c.queryItems = [
            URLQueryItem(name: "template", value: template),
            URLQueryItem(name: "version", value: Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? ""),
            URLQueryItem(name: "os", value: "macOS \(v.majorVersion).\(v.minorVersion).\(v.patchVersion)"),
        ] + (template == "bug_report.yml" && !audio.isEmpty ? [URLQueryItem(name: "audio", value: audio)] : [])
        if let u = c.url { NSWorkspace.shared.open(u) }
    }
}

/// Settings > About: reporting bugs and asking for features.
struct FeedbackSettings: View {
    @EnvironmentObject var m: Model
    var body: some View {
        SettingsHeading("Feedback")
        SettingsRow("GitHub", explain: "settings:feedback") {
            Button("Report a bug\u{2026}") { Feedback.open("bug_report.yml", model: m) }
            Button("Request a feature\u{2026}") { Feedback.open("feature_request.yml", model: m) }
        }
        SettingsRow("Debug log", explain: "settings:debugLog") {
            Toggle("Record a diagnostic log", isOn: Binding(get: { DebugLog.enabled }, set: { m.setDebugLog($0) }))
            if let u = DebugLog.latest() {
                ShareLink("Share log\u{2026}", item: u)
                Button("Show log") { NSWorkspace.shared.activateFileViewerSelecting([u]) }
                Button("Copy log") {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString((try? String(contentsOf: u, encoding: .utf8)) ?? "", forType: .string)
                    m.status = "The diagnostic log is on the clipboard: paste it into a bug report or a message."
                }
            }
        }
    }
}

/// Settings > About: author credit and license (the AGPLv3 "Appropriate Legal Notices", kept visible; see NOTICE.md).
struct AboutSettings: View {
    static let site = URL(string: "https://github.com/\(Updater.repo)")!
    var body: some View {
        SettingsHeading("License")
        SettingsRow("Leviathan", explain: "settings:license") {
            Text("by Shane O'Reilly").fontWeight(.semibold)
            Text("Free software under the GNU AGPLv3, with no warranty.").foregroundStyle(.secondary)
        }
        SettingsRow("", explain: "settings:license") {
            Button("License") { NSWorkspace.shared.open(AboutSettings.site.appendingPathComponent("blob/main/LICENSE")) }
            Button("Notice & credits") { NSWorkspace.shared.open(AboutSettings.site.appendingPathComponent("blob/main/NOTICE.md")) }
            Button("Source code") { NSWorkspace.shared.open(AboutSettings.site) }
        }
    }
}
