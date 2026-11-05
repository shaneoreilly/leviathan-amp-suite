// Renders the app icon (.icns), a README logo (SVG) and a preview PNG from Sources/Logo.swift.
// Build + run: tools/make_icon.sh
import AppKit

let args = CommandLine.arguments
let outDir = URL(fileURLWithPath: args.count > 1 ? args[1] : ".")

func render(_ px: Int, icon: Bool) -> Data {
    let cs = CGColorSpace(name: CGColorSpace.sRGB)!
    let ctx = CGContext(data: nil, width: px, height: px, bitsPerComponent: 8, bytesPerRow: 0, space: cs,
                        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    let S = CGFloat(px)
    ctx.translateBy(x: 0, y: S); ctx.scaleBy(x: 1, y: -1)        // y down, like the logo coordinates
    var art = CGRect(x: 0, y: 0, width: S, height: S)
    if icon {
        // macOS icon grid: 824/1024 rounded square, with a soft drop shadow
        let inset = S * 100 / 1024, r = CGRect(x: inset, y: inset, width: S - 2 * inset, height: S - 2 * inset)
        let shape = CGPath(roundedRect: r, cornerWidth: r.width * 0.225, cornerHeight: r.width * 0.225, transform: nil)
        ctx.saveGState()
        ctx.setShadow(offset: CGSize(width: 0, height: S * 0.01), blur: S * 0.02, color: CGColor(gray: 0, alpha: 0.5))
        ctx.addPath(shape); ctx.setFillColor(CGColor(srgbRed: 0.07, green: 0.07, blue: 0.08, alpha: 1)); ctx.fillPath()
        ctx.restoreGState()
        ctx.saveGState(); ctx.addPath(shape); ctx.clip()
        let grad = CGGradient(colorsSpace: cs, colors: [CGColor(srgbRed: 0.13, green: 0.13, blue: 0.15, alpha: 1),
                                                        CGColor(srgbRed: 0.04, green: 0.04, blue: 0.05, alpha: 1)] as CFArray, locations: [0, 1])!
        ctx.drawLinearGradient(grad, start: CGPoint(x: 0, y: r.minY), end: CGPoint(x: 0, y: r.maxY), options: [])
        ctx.restoreGState()
        art = r.insetBy(dx: r.width * 0.13, dy: r.width * 0.13)
    }
    let lw = art.width * (icon ? 0.04 : 0.035)
    ctx.addPath(Logo.cgPath(in: art))
    ctx.setLineWidth(lw); ctx.setLineCap(.round); ctx.setLineJoin(.round)
    ctx.setStrokeColor(icon ? CGColor(srgbRed: 0.96, green: 0.95, blue: 0.92, alpha: 1) : CGColor(srgbRed: 1, green: 0.3, blue: 0.3, alpha: 1))
    ctx.strokePath()
    if icon {   // the sea line in the accent red, drawn over
        let s = art.width / 100
        ctx.setStrokeColor(CGColor(srgbRed: 1, green: 0.3, blue: 0.3, alpha: 1)); ctx.setLineWidth(lw)
        let wave = Logo.paths.last!
        let p = CGMutablePath()
        for c in wave {
            switch c {
            case let .move(x, y): p.move(to: CGPoint(x: art.minX + x * s, y: art.minY + y * s))
            case let .curve(a, b, c2, d, x, y):
                p.addCurve(to: CGPoint(x: art.minX + x * s, y: art.minY + y * s), control1: CGPoint(x: art.minX + a * s, y: art.minY + b * s),
                           control2: CGPoint(x: art.minX + c2 * s, y: art.minY + d * s))
            default: break
            }
        }
        ctx.addPath(p); ctx.strokePath()
    }
    let rep = NSBitmapImageRep(cgImage: ctx.makeImage()!)
    return rep.representation(using: .png, properties: [:])!
}

let iconset = outDir.appendingPathComponent("AppIcon.iconset")
try? FileManager.default.removeItem(at: iconset)
try! FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
for base in [16, 32, 128, 256, 512] {
    try! render(base, icon: true).write(to: iconset.appendingPathComponent("icon_\(base)x\(base).png"))
    try! render(base * 2, icon: true).write(to: iconset.appendingPathComponent("icon_\(base)x\(base)@2x.png"))
}
try! render(1024, icon: true).write(to: outDir.appendingPathComponent("icon-1024.png"))

let svg = """
<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -4 108 108" width="160" height="160">
  <path d="\(Logo.svgPathData)" fill="none" stroke="#ff4d4d" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>

"""
try! svg.write(to: outDir.appendingPathComponent("logo.svg"), atomically: true, encoding: .utf8)
print("wrote AppIcon.iconset, icon-1024.png, logo.svg to \(outDir.path)")
