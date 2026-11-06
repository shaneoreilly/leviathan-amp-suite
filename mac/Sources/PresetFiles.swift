import SwiftUI
import AppKit
import UniformTypeIdentifiers

// MARK: - Preset files (.leviathan): export, import, open from Finder

extension UTType {
    /// Declared in Info.plist (UTExportedTypeDeclarations).
    static let leviathanPreset = UTType(exportedAs: "local.leviathan.ampsuite.preset", conformingTo: .json)
}

/// On disk: a small JSON wrapper so the format can grow.
struct PresetFile: Codable {
    var format = "leviathan-preset"
    var version = 1
    var name: String
    var app: String?
    var settings: Settings
}

extension Model {
    static let presetExt = "leviathan"

    /// Stored settings for a preset (your saved version of a factory preset wins).
    func presetSettings(_ name: String) -> Settings? {
        if let u = userPresets[name] { return u }
        guard let f = FACTORY.first(where: { $0.0 == name }) else { return nil }
        var s = Settings(); f.1(&s); return s
    }

    func presetData(_ name: String) throws -> Data {
        guard let s = presetSettings(name) else { throw CocoaError(.fileNoSuchFile) }
        let e = JSONEncoder(); e.outputFormatting = [.prettyPrinted, .sortedKeys]
        return try e.encode(PresetFile(name: name, app: Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String, settings: s))
    }

    /// Characters Finder won't take in a file name.
    static func fileName(_ name: String) -> String {
        name.replacingOccurrences(of: "/", with: "-").replacingOccurrences(of: ":", with: "-") + ".\(presetExt)"
    }

    func exportPreset(_ name: String) {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.leviathanPreset]
        panel.nameFieldStringValue = Model.fileName(name)
        panel.message = "Export “\(name)”"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do { try presetData(name).write(to: url, options: .atomic) }
        catch { status = "Couldn't export “\(name)”: \(error.localizedDescription)" }
    }

    /// Every preset, one file each, into a folder you pick.
    func exportAllPresets() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.canCreateDirectories = true
        panel.prompt = "Export Here"
        panel.message = "Choose a folder for \(presetOrder.count) preset files"
        guard panel.runModal() == .OK, let dir = panel.url else { return }
        var failed: [String] = []
        for n in presetOrder {
            do { try presetData(n).write(to: dir.appendingPathComponent(Model.fileName(n)), options: .atomic) } catch { failed.append(n) }
        }
        status = failed.isEmpty ? "Exported \(presetOrder.count) presets to \(dir.lastPathComponent)." : "Couldn't export: \(failed.joined(separator: ", "))"
        if failed.isEmpty { NSWorkspace.shared.activateFileViewerSelecting([dir]) }
    }

    func importPresetsPanel() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.leviathanPreset, .json]
        panel.allowsMultipleSelection = true
        panel.message = "Choose .\(Model.presetExt) preset files"
        guard panel.runModal() == .OK else { return }
        importPresets(panel.urls)
    }

    /// Adds each file as one of your presets (renamed if the name is taken by a different preset) and loads the last one.
    func importPresets(_ urls: [URL]) {
        var added: [String] = [], same: [String] = [], bad: [String] = []
        for url in urls {
            guard let data = try? Data(contentsOf: url),
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  obj["format"] as? String == "leviathan-preset",
                  let sObj = obj["settings"], let sData = try? JSONSerialization.data(withJSONObject: sObj),
                  let settings = Settings.lenient(sData) else { bad.append(url.lastPathComponent); continue }
            let base = ((obj["name"] as? String) ?? url.deletingPathExtension().lastPathComponent).trimmingCharacters(in: .whitespaces)
            let wanted = base.isEmpty ? "Imported" : base
            if presetSettings(wanted) == settings { same.append(wanted); continue }
            var name = wanted, i = 2
            while isFactory(name) || userPresets[name] != nil { name = "\(wanted) \(i)"; i += 1 }
            userPresets[name] = settings
            presetMeta[name, default: PresetMeta()].imported = Date()
            added.append(name)
        }
        if !added.isEmpty { persistPresets(); loadUser(added.last!) }
        var parts: [String] = []
        if !added.isEmpty { parts.append(added.count == 1 ? "Imported “\(added[0])”." : "Imported \(added.count) presets.") }
        if !same.isEmpty { parts.append("Already had \(same.joined(separator: ", ")).") }
        if !bad.isEmpty { parts.append("Not a Leviathan preset: \(bad.joined(separator: ", ")).") }
        status = parts.joined(separator: " ")
    }
}

/// Files opened from Finder (double-click, or dropped on the Dock icon).
final class AppDelegate: NSObject, NSApplicationDelegate {
    @MainActor static weak var model: Model?
    @MainActor static var pending: [URL] = []

    func application(_ application: NSApplication, open urls: [URL]) {
        MainActor.assumeIsolated {
            let presets = urls.filter { $0.pathExtension.lowercased() == Model.presetExt }
            if let m = AppDelegate.model { m.importPresets(presets); m.screen = .presets } else { AppDelegate.pending += presets }
        }
    }
}
