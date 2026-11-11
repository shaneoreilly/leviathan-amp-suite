import SwiftUI

// Light and dark themes, with your own background colour for each (Settings > Appearance). Every colour in the app
// comes from the background: panels, lines, knob faces and text are mixes of it, so any background you pick stays
// readable. Whether text is dark or light follows how bright the background actually is.
// Windows / Android: theme::apply in UI/Widgets.cpp, the same formula.

struct RGB: Equatable {
    var r, g, b: Double
    init(_ r: Double, _ g: Double, _ b: Double) { self.r = r; self.g = g; self.b = b }
    init?(hex: String) {
        let h = hex.trimmingCharacters(in: CharacterSet(charactersIn: "# "))
        guard h.count == 6, let v = UInt32(h, radix: 16) else { return nil }
        self.init(Double((v >> 16) & 255) / 255, Double((v >> 8) & 255) / 255, Double(v & 255) / 255)
    }
    init(_ c: Color) {
        let n = NSColor(c).usingColorSpace(.sRGB) ?? .black
        self.init(Double(n.redComponent), Double(n.greenComponent), Double(n.blueComponent))
    }
    var hex: String { String(format: "%02X%02X%02X", Int((r * 255).rounded()), Int((g * 255).rounded()), Int((b * 255).rounded())) }
    var color: Color { Color(.sRGB, red: r, green: g, blue: b) }
    /// Perceived brightness, 0...1.
    var luma: Double { 0.2126 * r + 0.7152 * g + 0.0722 * b }
    func mix(_ o: RGB, _ t: Double) -> RGB { RGB(r + (o.r - r) * t, g + (o.g - g) * t, b + (o.b - b) * t) }
}

struct Palette {
    let isLight: Bool
    let bg, bar, panel, panel2, line, knobFace, knobTrack, off, dim, text, secondary, tint, warnBg, warnFg: Color

    static let defaultDark = RGB(hex: "0D0D0D")!, defaultLight = RGB(hex: "ECECEE")!

    static func make(_ bg: RGB) -> Palette {
        let light = bg.luma > 0.45
        let fg = light ? RGB(hex: "141414")! : RGB(hex: "EEEEEE")!, white = RGB(1, 1, 1)
        func m(_ t: Double) -> Color { bg.mix(fg, t).color }
        return Palette(
            isLight: light, bg: bg.color,
            bar: m(light ? 0.03 : 0.025),
            panel: light ? bg.mix(white, 0.7).color : m(0.04),        // light: cards lift towards white
            panel2: m(light ? 0.06 : 0.08),
            line: m(light ? 0.16 : 0.17),
            knobFace: light ? bg.mix(white, 0.95).color : m(0.11),
            knobTrack: m(light ? 0.16 : 0.19),
            off: m(light ? 0.18 : 0.15),                               // unlit lights, empty tracks
            dim: m(light ? 0.38 : 0.3),                                // disabled marks
            text: fg.color, secondary: fg.mix(bg, 0.42).color,
            tint: light ? .black : .white,                             // the base of see-through overlays
            warnBg: light ? RGB(hex: "FFF0D4")!.color : RGB(hex: "291F0F")!.color,
            warnFg: light ? RGB(hex: "6A4600")!.color : RGB(hex: "FFD999")!.color)
    }
}

/// The palette in use. Model sets it (theme and background colours); RootView rebuilds the window when it changes.
var PAL = Palette.make(Palette.defaultDark)

/// The background colour picker. The system colour panel talks to this long-lived object, not to a view: picking a
/// colour rebuilds the window (every view reads PAL), which would cut a view's picker off after the first pick. So
/// the panel keeps working until you close it; changes apply once the colour stops moving for a moment.
@MainActor final class BackgroundColourPanel: NSObject {
    static let shared = BackgroundColourPanel()
    private weak var model: Model?
    private var pending: DispatchWorkItem?

    func open(_ m: Model) {
        model = m
        let p = NSColorPanel.shared
        p.showsAlpha = false
        p.isContinuous = true
        p.setTarget(nil); p.setAction(nil)   // don't let setting the colour below call back
        p.color = NSColor(m.themeBackground.color)
        p.setTarget(self); p.setAction(#selector(changed(_:)))
        p.title = m.theme == "light" ? "Background (light theme)" : "Background (dark theme)"
        p.orderFront(nil)
    }

    @objc private func changed(_ p: NSColorPanel) {
        let hex = RGB(Color(nsColor: p.color)).hex
        pending?.cancel()
        let w = DispatchWorkItem { [weak self] in self?.model?.setBackground(hex) }
        pending = w
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.15, execute: w)
    }
}
