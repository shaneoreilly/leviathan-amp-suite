import SwiftUI
import CoreGraphics

/// Line-art logo: the white whale breaching, jaws open, over a line of sea. Drawn as strokes in a
/// 100 x 100 box so the same geometry gives the in-app mark, the app icon and the README's SVG.
enum Logo {
    enum Cmd { case move(Double, Double), line(Double, Double), curve(Double, Double, Double, Double, Double, Double), close }

    static let paths: [[Cmd]] = {
        var p: [[Cmd]] = []
        // head: broad back rising from the water, blunt brow, then the upper jaw raking down into the sea
        p.append([.move(8, 86), .curve(4, 58, 8, 25, 21, 11), .curve(29, 2, 55, 2, 65, 11), .curve(70, 15, 70, 21, 67, 26), .line(46, 84)])
        // lower jaw: one long blade thrown up and out to the right, hinged where the upper jaw meets the sea
        p.append([.move(48, 86), .line(92, 47)])
        // teeth: upper row points down into the mouth, lower row points up at it
        p += ticks(from: (64, 33), to: (52, 67), count: 5, normal: (0.9, 0.42), len: 4)
        p += ticks(from: (60, 75), to: (85, 53), count: 5, normal: (-0.66, -0.75), len: 4)
        // the sea
        p.append(wave(y: 89, from: 3, to: 97, step: 9.4, amp: 2.2))
        return p
    }()

    static let eye = (x: 25.0, y: 72.0, r: 1.8)

    private static func ticks(from a: (Double, Double), to b: (Double, Double), count: Int, normal n: (Double, Double), len: Double) -> [[Cmd]] {
        (0..<count).map { i in
            let t = Double(i) / Double(count - 1)
            let x = a.0 + (b.0 - a.0) * t, y = a.1 + (b.1 - a.1) * t
            return [.move(x, y), .line(x + n.0 * len, y + n.1 * len)]
        }
    }

    private static func wave(y: Double, from x0: Double, to x1: Double, step: Double, amp: Double) -> [Cmd] {
        var c: [Cmd] = [.move(x0, y)]
        var x = x0, up = true
        while x < x1 - 0.01 {
            let nx = min(x + step, x1), dy = up ? -amp : amp
            c.append(.curve(x + (nx - x) * 0.35, y + dy, x + (nx - x) * 0.65, y + dy, nx, y))
            x = nx; up.toggle()
        }
        return c
    }

    /// The logo scaled into `rect` (keeps the 1:1 aspect, centred).
    static func cgPath(in rect: CGRect) -> CGPath {
        let s = min(rect.width, rect.height) / 100
        let ox = rect.midX - 50 * s, oy = rect.midY - 50 * s
        func pt(_ x: Double, _ y: Double) -> CGPoint { CGPoint(x: ox + x * s, y: oy + y * s) }
        let path = CGMutablePath()
        for cmds in paths {
            for c in cmds {
                switch c {
                case let .move(x, y): path.move(to: pt(x, y))
                case let .line(x, y): path.addLine(to: pt(x, y))
                case let .curve(a, b, c2, d, x, y): path.addCurve(to: pt(x, y), control1: pt(a, b), control2: pt(c2, d))
                case .close: path.closeSubpath()
                }
            }
        }
        path.addEllipse(in: CGRect(x: ox + (eye.x - eye.r) * s, y: oy + (eye.y - eye.r) * s, width: 2 * eye.r * s, height: 2 * eye.r * s))
        return path
    }

    /// SVG path data for the 100 x 100 box.
    static var svgPathData: String {
        func f(_ v: Double) -> String { String(format: "%g", (v * 100).rounded() / 100) }
        var out: [String] = []
        for cmds in paths {
            out.append(cmds.map { c -> String in
                switch c {
                case let .move(x, y): return "M\(f(x)) \(f(y))"
                case let .line(x, y): return "L\(f(x)) \(f(y))"
                case let .curve(a, b, c2, d, x, y): return "C\(f(a)) \(f(b)) \(f(c2)) \(f(d)) \(f(x)) \(f(y))"
                case .close: return "Z"
                }
            }.joined(separator: " "))
        }
        let e = eye
        out.append("M\(f(e.x - e.r)) \(f(e.y)) a\(f(e.r)) \(f(e.r)) 0 1 0 \(f(2 * e.r)) 0 a\(f(e.r)) \(f(e.r)) 0 1 0 \(f(-2 * e.r)) 0")
        return out.joined(separator: " ")
    }
}

struct LogoShape: Shape {
    func path(in rect: CGRect) -> Path { Path(Logo.cgPath(in: rect)) }
}

/// The mark as used in the app: strokes scale with the size.
struct LogoMark: View {
    var size: CGFloat
    var color: Color = .white
    var body: some View {
        LogoShape().stroke(color, style: StrokeStyle(lineWidth: max(1, size * 0.045), lineCap: .round, lineJoin: .round))
            .frame(width: size, height: size)
    }
}
