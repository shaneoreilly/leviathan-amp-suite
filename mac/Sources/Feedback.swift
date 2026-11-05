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

/// The Help & feedback group on the Settings screen.
struct FeedbackSettings: View {
    @EnvironmentObject var m: Model
    let row: (String, AnyView) -> AnyView
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            row("GitHub", AnyView(HStack {
                Button("Report a bug…") { Feedback.open("bug_report.yml", model: m) }
                Button("Request a feature…") { Feedback.open("feature_request.yml", model: m) }
            }))
            Text("Opens a form on GitHub (a free account is needed) with your Leviathan version, macOS version and audio devices filled in. You can see and change everything before you submit.")
                .font(.system(size: 11 * UI)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
    }
}

/// The About group: author credit, version and license (the AGPLv3 "Appropriate Legal Notices"; see NOTICE.md).
struct AboutSettings: View {
    let row: (String, AnyView) -> AnyView
    static let site = URL(string: "https://github.com/\(Updater.repo)")!
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            row("Leviathan", AnyView(HStack(spacing: 6) {
                Text("by Shane O'Reilly").fontWeight(.semibold)
                Text("· version \(Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "")").foregroundStyle(.secondary)
            }))
            row("License", AnyView(HStack {
                Text("Free software under the GNU AGPLv3, with no warranty.")
                Button("License") { NSWorkspace.shared.open(AboutSettings.site.appendingPathComponent("blob/main/LICENSE")) }
                Button("Notice & credits") { NSWorkspace.shared.open(AboutSettings.site.appendingPathComponent("blob/main/NOTICE.md")) }
                Button("Source code") { NSWorkspace.shared.open(AboutSettings.site) }
            }))
            Text("You're free to use, share and change Leviathan. Anything made from it must also be open source under the AGPLv3 and keep the credit \u{201C}Leviathan by Shane O'Reilly\u{201D}.")
                .font(.system(size: 11 * UI)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
    }
}
