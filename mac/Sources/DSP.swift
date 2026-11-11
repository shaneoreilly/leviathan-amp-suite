import Foundation
import Accelerate

// MARK: - Primitives

final class Biquad {
    enum Kind { case lp, hp, bp, peak, lowShelf, highShelf }
    private var b0: Float = 1, b1: Float = 0, b2: Float = 0, a1: Float = 0, a2: Float = 0
    private var z1: Float = 0, z2: Float = 0

    init() {}
    init(_ k: Kind, _ f: Double, _ sr: Double, q: Double = 0.7071, db: Double = 0) { set(k, f, sr, q: q, db: db) }

    // RBJ cookbook
    func set(_ k: Kind, _ freq: Double, _ sr: Double, q: Double = 0.7071, db: Double = 0) {
        let f = min(max(freq, 5), sr * 0.48)
        let w = 2 * Double.pi * f / sr, cw = cos(w), sw = sin(w), al = sw / (2 * q), A = pow(10, db / 40)
        var B0 = 1.0, B1 = 0.0, B2 = 0.0, A0 = 1.0, A1 = 0.0, A2 = 0.0
        switch k {
        case .lp: B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al
        case .hp: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al
        case .bp: B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al
        case .peak: B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A
        case .lowShelf:
            let s = 2 * sqrt(A) * al
            B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s)
            A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s
        case .highShelf:
            let s = 2 * sqrt(A) * al
            B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s)
            A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s
        }
        b0 = Float(B0 / A0); b1 = Float(B1 / A0); b2 = Float(B2 / A0); a1 = Float(A1 / A0); a2 = Float(A2 / A0)
    }

    @inline(__always) func run(_ x: Float) -> Float {
        let y = b0 * x + z1
        z1 = b1 * x - a1 * y + z2
        z2 = b2 * x - a2 * y
        return y
    }
}

struct Smooth {
    var v: Float = 0, target: Float = 0
    @inline(__always) mutating func next(_ c: Float) -> Float { v += (target - v) * c; return v }
}

@inline(__always) func dbLin(_ d: Float) -> Float { powf(10, d / 20) }
@inline(__always) func lerp(_ t: Float, _ a: Float, _ b: Float) -> Float { a + (b - a) * t }

// MARK: - Shared parameters (main thread writes, audio thread reads once per buffer)

final class Params: @unchecked Sendable {
    var version = 0
    /// Time spent inside NAM (amp A + B), for the diagnostic log.
    var namSeconds: Double = 0
    var inGain: Float = 0, inputChannel = 0, muted = false
    var gateOn = true, gateThresh: Float = -60, gateRelease: Float = 60, gateDepth: Float = -60
    var pitchOn = false, pitchSemis: Float = 0, pitchCents: Float = 0, pitchMix: Float = 1, pitchLowLag = false
    var odOn = true, odDrive: Float = 0, odTone: Float = 0.6, odLevel: Float = 0.75
    var ampOn = true, ampChannel = 2, ampGain: Float = 0.55
    var bass: Float = 0.5, mid: Float = 0.5, treble: Float = 0.55, presence: Float = 0.55, depth: Float = 0.5, ampVolume: Float = 0.5
    var eqOn = true
    let eq = UnsafeMutablePointer<Float>.allocate(capacity: 8)
    var cabOn = true, cabLowCut: Float = 80, cabHighCut: Float = 7500, cabLevel: Float = 0
    var delayOn = false, delayTime: Float = 420, delayFeedback: Float = 0.28, delayTone: Float = 0.45, delayMix: Float = 0.2
    var revOn = false, revSize: Float = 0.45, revMix: Float = 0.18
    var doublerOn = false, doublerWidth: Float = 0.6, doublerTime: Float = 20
    var masterVol: Float = -6

    // pedal chain (pre-amp, user ordered). Entries are PedalKind raw values.
    static let maxChain = 9
    let chain = UnsafeMutablePointer<Int32>.allocate(capacity: Params.maxChain)
    var chainCount = 0
    var compOn = true, compSustain: Float = 0.5, compAttack: Float = 10, compLevel: Float = 0, compBlend: Float = 1
    var wahOn = true, wahAuto = false, wahPos: Float = 0.5, wahSens: Float = 0.5, wahQ: Float = 0.5
    var fuzzOn = true, fuzzAmt: Float = 0.6, fuzzTone: Float = 0.5, fuzzLevel: Float = 0.5
    var bladeOn = true, bladeDrive: Float = 0.5, bladeLow: Float = 0.5, bladeMid: Float = 0.5, bladeHigh: Float = 0.5, bladeLevel: Float = 0.5
    var bladeBoost: Float = 0.5, bladeBoostOn = false, bladeShift = false, bladeDiode = 0
    var chorusOn = true, chorusRate: Float = 0.8, chorusDepth: Float = 0.5, chorusMix: Float = 0.5
    var flangerOn = true, flangerRate: Float = 0.25, flangerDepth: Float = 0.7, flangerFb: Float = 0.5, flangerMix: Float = 0.5
    var phaserOn = true, phaserRate: Float = 0.5, phaserDepth: Float = 0.7, phaserFb: Float = 0.4
    var tremOn = true, tremRate: Float = 5, tremDepth: Float = 0.6, tremShape: Float = 0

    // captured amp (replaces the voicings when capOn; its IR goes into the cab slot)
    var capOn = false, capHP: Float = 80, capLS: Float = 0, capPkF: Float = 900, capPk: Float = 0, capHS: Float = 0
    var capK1: Float = 1, capB1: Float = 0, capMid: Float = 6000, capK2: Float = 1, capB2: Float = 0
    var capDrive: Float = 0, capLevel: Float = 0

    // NAM capture (replaces the amp when namOn and a model is loaded); input/output trims in dB
    var namOn = false, namIn: Float = 0, namOut: Float = 0
    // Amp B: a second NAM capture fed by the same pedals; Spread pans A left and B right (mid/side, mono-safe)
    var ampBOn = false, ampBLevel: Float = 0, ampBSpread: Float = 1

    // metronome (mixed after the master, never through the amp)
    var metroOn = false, bpm: Float = 120, beatsPerBar = 4, metroVol: Float = -12

    // recorder: raw input + final stereo output, drained by the UI thread
    var recording = false
    let recDry = Ring(1 << 18), recL = Ring(1 << 18), recR = Ring(1 << 18)

    // reamp source: replaces the live input while playing
    var srcPtr: UnsafeMutablePointer<Float>?, srcLen = 0, srcPos = 0, srcPlaying = false, srcDone = false, srcLoop = false

    // backing track: stereo, mixed after the master
    var bkL: UnsafeMutablePointer<Float>?, bkR: UnsafeMutablePointer<Float>?, bkLen = 0, bkPos = 0
    var bkPlaying = false, bkLoop = true, bkVol: Float = -6
    /// Output EQ: corrects the speakers or headphones in use (set per output by the model, not part of presets). It sits
    /// after the recorder tap, so recordings and re-amps never have it. Off = bypassed.
    var outEqOn = false, outEqLow: Float = 0, outEqMid: Float = 0, outEqHigh: Float = 0, outEqLevel: Float = 0
    /// A jump requested by the UI (-1 = none); the audio thread applies it at the start of a block, so it can't be
    /// overwritten by the block's own position update.
    var bkSeek = -1

    init() {
        eq.initialize(repeating: 0, count: 8)
        chain.initialize(repeating: -1, count: Params.maxChain)
    }

    // written by audio thread, read by UI
    var inPeak: Float = 0, outPeak: Float = 0, gateOpen = false, pitchLatencyMs: Float = 0
    var inClips = 0   // raw input samples at full scale (the interface clipping), ever-increasing
    var metroBeat = -1
    let tunerRing = UnsafeMutablePointer<Float>.allocate(capacity: 4096)
    var tunerWrite = 0
}

/// Single-producer / single-consumer float FIFO (audio thread writes, main thread reads).
final class Ring: @unchecked Sendable {
    let cap: Int, mask: Int
    private let buf: UnsafeMutablePointer<Float>
    private var w = 0, r = 0
    private(set) var dropped = 0

    init(_ capacity: Int) {
        cap = capacity; mask = capacity - 1
        buf = .allocate(capacity: capacity); buf.initialize(repeating: 0, count: capacity)
    }

    func write(_ x: UnsafePointer<Float>, _ n: Int) {
        let k = min(n, cap - (w &- r))
        for i in 0..<k { buf[(w &+ i) & mask] = x[i] }
        OSMemoryBarrier()
        w &+= k
        dropped += n - k
    }

    var available: Int { w &- r }

    /// Copy up to `n` samples out. Returns the count copied.
    func read(into dst: UnsafeMutablePointer<Float>, max n: Int) -> Int {
        let k = min(n, w &- r)
        OSMemoryBarrier()
        for i in 0..<k { dst[i] = buf[(r &+ i) & mask] }
        r &+= k
        return k
    }

    /// Discard everything (only call while the producer is idle).
    func reset() { r = w; dropped = 0 }
}

// MARK: - Pedals

enum PedalKind: Int, CaseIterable, Codable, Identifiable {
    case comp, wah, fuzz, od, chorus, flanger, phaser, trem, blade
    var id: Int { rawValue }
    var title: String {
        ["COMPRESSOR", "WAH", "FUZZ", "OVERDRIVE", "CHORUS", "FLANGER", "PHASER", "TREMOLO", "BLADE"][rawValue]
    }
}

class Pedal {
    let sr: Float
    var mix = Smooth()
    init(sr: Float) { self.sr = sr }
    func update(_ p: Params) {}
    func tick(_ x: Float) -> Float { x }

    /// Runs the block with a click-free bypass crossfade.
    func process(_ a: UnsafeMutablePointer<Float>, _ n: Int, _ c: Float) {
        if mix.target == 0 && mix.v < 1e-4 { return }
        for i in 0..<n {
            let m = mix.next(c), x = a[i]
            if m < 1e-4 { continue }
            a[i] = x + (tick(x) - x) * m
        }
    }
}

func makePedal(_ k: PedalKind, sr: Float) -> Pedal {
    switch k {
    case .comp: return Compressor(sr: sr)
    case .wah: return Wah(sr: sr)
    case .fuzz: return Fuzz(sr: sr)
    case .od: return Overdrive(sr: sr)
    case .chorus: return Chorus(sr: sr)
    case .flanger: return Flanger(sr: sr)
    case .phaser: return Phaser(sr: sr)
    case .trem: return Tremolo(sr: sr)
    case .blade: return Blade(sr: sr)
    }
}

/// Feed-forward peak compressor; Sustain sets threshold and ratio together, makeup is automatic.
final class Compressor: Pedal {
    private var env: Float = 0, atk: Float = 0, rel: Float = 0, thr: Float = 0, slope: Float = 0, makeup: Float = 1, blend: Float = 1
    override func update(_ p: Params) {
        mix.target = p.compOn ? 1 : 0
        atk = expf(-1 / (max(p.compAttack, 0.5) / 1000 * sr)); rel = expf(-1 / (0.18 * sr))
        thr = -8 - p.compSustain * 40
        slope = 1 - 1 / (3 + p.compSustain * 7)
        makeup = dbLin(-thr * slope * 0.55 + p.compLevel)
        blend = p.compBlend
    }
    override func tick(_ x: Float) -> Float {
        let ax = abs(x)
        env = ax + (env - ax) * (ax > env ? atk : rel)
        let lvl = 20 * log10f(env + 1e-9)
        let y = x * (lvl > thr ? dbLin((thr - lvl) * slope) : 1) * makeup
        return x + (y - x) * blend
    }
}

/// Resonant band-pass sweep (TPT state-variable filter). Manual position or envelope-driven auto-wah.
final class Wah: Pedal {
    private var ic1: Float = 0, ic2: Float = 0, env: Float = 0, k: Float = 0.3, g: Float = 0.1, cnt = 0
    private var auto = false, sens: Float = 0.5, eA: Float = 0, eR: Float = 0
    private var pos = Smooth()
    override func update(_ p: Params) {
        mix.target = p.wahOn ? 1 : 0
        pos.target = p.wahPos; auto = p.wahAuto; sens = p.wahSens
        k = 1 / (2 + p.wahQ * 8)
        eA = expf(-1 / (0.004 * sr)); eR = expf(-1 / (0.12 * sr))
    }
    override func tick(_ x: Float) -> Float {
        let ax = abs(x)
        env = ax + (env - ax) * (ax > env ? eA : eR)
        let pv = pos.next(0.002)
        if cnt == 0 {
            let t = auto ? min(env * (2 + sens * 40), 1) : pv
            g = tanf(Float.pi * min(350 * powf(2200 / 350, t), sr * 0.45) / sr)
        }
        cnt = (cnt + 1) & 15
        let a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2
        let v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2
        return v1 * k * 2.5 + v2 * 0.1
    }
}

/// High-gain asymmetric clipper, 4x oversampled, with a scooped tone control (LP/HP blend).
final class Fuzz: Pedal {
    private let hp = Biquad(), lpT = Biquad(), hpT = Biquad(), dc = Biquad()
    private let up = (0..<4).map { _ in Biquad() }, down = (0..<4).map { _ in Biquad() }
    private var pre: Float = 1, tone: Float = 0.5, lvl: Float = 1
    override init(sr: Float) {
        super.init(sr: sr)
        let os = Double(sr) * 4, aa = min(20000, Double(sr) * 0.45)
        for (i, q) in [0.5098, 0.6013, 0.9000, 2.5629].enumerated() { up[i].set(.lp, aa, os, q: q); down[i].set(.lp, aa, os, q: q) }
    }
    override func update(_ p: Params) {
        let s = Double(sr)
        mix.target = p.fuzzOn ? 1 : 0
        hp.set(.hp, 90, s); lpT.set(.lp, 550, s); hpT.set(.hp, 1100, s); dc.set(.hp, 20, s)
        pre = 4 + p.fuzzAmt * p.fuzzAmt * 300; tone = p.fuzzTone; lvl = lerp(p.fuzzLevel, 0.05, 1.2)
    }
    override func tick(_ x: Float) -> Float {
        let v = hp.run(x) * pre * 4
        var o: Float = 0
        for k in 0..<4 {
            var u = up[3].run(up[2].run(up[1].run(up[0].run(k == 0 ? v : 0))))
            u = u > 0 ? tanhf(u) : tanhf(u * 0.7) * 0.8
            o = down[3].run(down[2].run(down[1].run(down[0].run(u))))
        }
        let y = dc.run(o)
        return (lpT.run(y) * (1 - tone) + hpT.run(y) * tone) * lvl * 1.6
    }
}

final class Overdrive: Pedal {
    private let hp = Biquad(), shelf = Biquad(), hump = Biquad(), lp = Biquad()
    private var pre: Float = 1, lvl: Float = 1
    override func update(_ p: Params) {
        let s = Double(sr)
        mix.target = p.odOn ? 1 : 0
        hp.set(.hp, 160, s, q: 0.5); shelf.set(.lowShelf, 500, s, db: -9); hump.set(.peak, 900, s, q: 0.8, db: 3)
        lp.set(.lp, Double(lerp(p.odTone, 1200, 7000)), s, q: 0.6)
        pre = 1 + p.odDrive * p.odDrive * 60; lvl = lerp(p.odLevel, 0.2, 2.5)
    }
    override func tick(_ x: Float) -> Float {
        lp.run(tanhf(hump.run(shelf.run(hp.run(x))) * pre + 0.05) - 0.04996) * lvl
    }
}

/// Op-amp distortion voiced after the EAE Longsword: switchable front-end mid boost, a high-gain stage with
/// selectable clipping (MOSFET, none = op-amp headroom then the rails, silicon) and an active 3-band EQ whose mid
/// shifts between 650 Hz and 1.5 kHz. 4x oversampled like the fuzz.
final class Blade: Pedal {
    private let hp = Biquad(), bHP = Biquad(), bLP = Biquad(), focus = Biquad(), post = Biquad(), dc = Biquad()
    private let low = Biquad(), mid = Biquad(), high = Biquad()
    private let up = (0..<4).map { _ in Biquad() }, down = (0..<4).map { _ in Biquad() }
    private var gain: Float = 1, boost: Float = 1, boostOn = false, lvl: Float = 1, diode = 0, t: Float = 1, norm: Float = 1
    override init(sr: Float) {
        super.init(sr: sr)
        let os = Double(sr) * 4, aa = min(20000, Double(sr) * 0.45)
        for (i, q) in [0.5098, 0.6013, 0.9000, 2.5629].enumerated() { up[i].set(.lp, aa, os, q: q); down[i].set(.lp, aa, os, q: q) }
    }
    override func update(_ p: Params) {
        let s = Double(sr)
        mix.target = p.bladeOn ? 1 : 0
        hp.set(.hp, 70, s); bHP.set(.hp, 220, s, q: 0.6); bLP.set(.lp, 4500, s, q: 0.6)
        focus.set(.peak, 720, s, q: 0.7, db: 4); post.set(.lp, 6800, s, q: 0.6); dc.set(.hp, 20, s)
        low.set(.lowShelf, 180, s, db: Double((p.bladeLow - 0.5) * 24))
        mid.set(.peak, p.bladeShift ? 1500 : 650, s, q: 0.9, db: Double((p.bladeMid - 0.5) * 24))
        high.set(.highShelf, 2800, s, db: Double((p.bladeHigh - 0.5) * 24))
        gain = 1 + p.bladeDrive * p.bladeDrive * 200
        boostOn = p.bladeBoostOn; boost = dbLin(p.bladeBoost * 20)
        diode = p.bladeDiode
        t = diode == 0 ? 0.8 : (diode == 1 ? 3.0 : 0.45)
        norm = 1 / sqrtf(t)
        lvl = lerp(p.bladeLevel, 0.05, 1.5)
    }
    @inline(__always) private func clip(_ u: Float) -> Float {
        if diode == 0 { return t * (tanhf(u / t + 0.08) - tanhf(0.08)) }        // MOSFET: smooth, slightly asymmetric
        if diode == 1 { return u / powf(1 + powf(abs(u) / t, 6), 1.0 / 6) }      // no diodes: clean headroom, then the rails
        return u / powf(1 + powf(abs(u) / t, 2.5), 1.0 / 2.5)                     // silicon: tight and compressed
    }
    override func tick(_ x: Float) -> Float {
        var y = hp.run(x)
        if boostOn { y += bLP.run(bHP.run(y)) * (boost - 1) }
        let v = focus.run(y) * gain
        var o: Float = 0
        for k in 0..<4 {
            let u = up[3].run(up[2].run(up[1].run(up[0].run(k == 0 ? v * 4 : 0))))
            o = down[3].run(down[2].run(down[1].run(down[0].run(clip(u)))))
        }
        let z = post.run(dc.run(o)) * norm
        return high.run(mid.run(low.run(z))) * lvl
    }
}

/// Short fractional delay line for modulation effects.
final class ModLine {
    private let size = 8192, mask = 8191
    private let buf: UnsafeMutablePointer<Float>
    private var w = 0
    init() { buf = .allocate(capacity: 8192); buf.initialize(repeating: 0, count: 8192) }
    @inline(__always) func push(_ x: Float) { buf[w] = x; w = (w + 1) & mask }
    /// `d` samples behind the newest sample (d >= 1).
    @inline(__always) func tap(_ d: Float) -> Float {
        let rp = Float(w) - 1 - min(max(d, 1), Float(size - 4)) + Float(size)
        let i0 = Int(rp), f = rp - Float(i0)
        let a = buf[i0 & mask], b = buf[(i0 + 1) & mask]
        return a + (b - a) * f
    }
}

final class Chorus: Pedal {
    private let line = ModLine()
    private var ph: Float = 0, inc: Float = 0, depth: Float = 0, wet: Float = 0.5
    override func update(_ p: Params) {
        mix.target = p.chorusOn ? 1 : 0
        inc = p.chorusRate / sr; depth = p.chorusDepth * 0.006 * sr; wet = p.chorusMix
    }
    override func tick(_ x: Float) -> Float {
        line.push(x)
        ph += inc; if ph >= 1 { ph -= 1 }
        let base = 0.012 * sr
        let t1 = line.tap(base + depth * (0.5 + 0.5 * sinf(2 * .pi * ph)))
        let t2 = line.tap(base * 1.3 + depth * (0.5 + 0.5 * sinf(2 * .pi * (ph + 0.37))))
        return x * (1 - wet * 0.5) + (t1 + t2) * 0.5 * wet
    }
}

final class Flanger: Pedal {
    private let line = ModLine()
    private var ph: Float = 0, inc: Float = 0, depth: Float = 0, fb: Float = 0, wet: Float = 0.5, last: Float = 0
    override func update(_ p: Params) {
        mix.target = p.flangerOn ? 1 : 0
        inc = p.flangerRate / sr; depth = p.flangerDepth * 0.005 * sr; fb = p.flangerFb * 0.85; wet = p.flangerMix
    }
    override func tick(_ x: Float) -> Float {
        ph += inc; if ph >= 1 { ph -= 1 }
        line.push(x + last * fb)
        last = line.tap(0.0003 * sr + depth * (0.5 - 0.5 * cosf(2 * .pi * ph)))
        return (x + last * wet) / (1 + wet * (1 + fb))
    }
}

final class Phaser: Pedal {
    private let z = UnsafeMutablePointer<Float>.allocate(capacity: 6)
    private var ph: Float = 0, inc: Float = 0, depth: Float = 0, fb: Float = 0, a: Float = 0, last: Float = 0, cnt = 0
    override init(sr: Float) { super.init(sr: sr); z.initialize(repeating: 0, count: 6) }
    override func update(_ p: Params) {
        mix.target = p.phaserOn ? 1 : 0
        inc = p.phaserRate / sr; depth = p.phaserDepth; fb = p.phaserFb * 0.7
    }
    override func tick(_ x: Float) -> Float {
        ph += inc; if ph >= 1 { ph -= 1 }
        if cnt == 0 {
            let t = 0.5 - 0.5 * cosf(2 * .pi * ph)
            let f = 250 * powf(10, t * depth)                // 250 Hz up to 2.5 kHz at full depth
            let tn = tanf(.pi * min(f, sr * 0.45) / sr)
            a = (tn - 1) / (tn + 1)
        }
        cnt = (cnt + 1) & 7
        var u = x + last * fb
        for i in 0..<6 { let y = a * u + z[i]; z[i] = u - a * y; u = y }
        last = u
        return (x + u) * 0.5
    }
}

final class Tremolo: Pedal {
    private var ph: Float = 0, inc: Float = 0, depth: Float = 0, k: Float = 1, norm: Float = 1
    override func update(_ p: Params) {
        mix.target = p.tremOn ? 1 : 0
        inc = p.tremRate / sr; depth = p.tremDepth
        k = 1 + p.tremShape * 10; norm = 1 / tanhf(k)
    }
    override func tick(_ x: Float) -> Float {
        ph += inc; if ph >= 1 { ph -= 1 }
        let s = tanhf(sinf(2 * .pi * ph) * k) * norm
        return x * (1 - depth * (0.5 - 0.5 * s))
    }
}

// MARK: - Amp voicings

struct Voicing {
    let name: String
    let hp: Double, tight: Double, emph: Double     // pre-clip shaping
    let preMax: Float, b1: Float                    // stage 1 gain range / bias (even harmonics)
    let cascade: Bool, s2: Float, b2: Float, s3: Float
    let interHP: Double, lp1: Double, lp2: Double, lp3: Double
    let mid: Double, character: Double, power: Float, out: Float
}

let VOICINGS: [Voicing] = [
    Voicing(name: "CLEAN", hp: 60, tight: 0, emph: 1.5, preMax: 5, b1: 0.03, cascade: false, s2: 1, b2: 0, s3: 1,
            interHP: 20, lp1: 15000, lp2: 15000, lp3: 15000, mid: 500, character: 0, power: 1.2, out: 1.8),
    Voicing(name: "RUST", hp: 75, tight: -1.5, emph: 3, preMax: 35, b1: 0.15, cascade: true, s2: 3, b2: 0.05, s3: 1.6,
            interHP: 40, lp1: 12000, lp2: 9000, lp3: 8000, mid: 700, character: 2, power: 1.6, out: 0.95),
    Voicing(name: "HOT", hp: 90, tight: -3, emph: 4, preMax: 90, b1: 0.2, cascade: true, s2: 6, b2: 0.08, s3: 2.5,
            interHP: 70, lp1: 10000, lp2: 7500, lp3: 6500, mid: 800, character: 3, power: 1.6, out: 0.85),
]

// MARK: - Engine

final class DSP {
    let sr: Float
    let p: Params
    private var lastVersion = -1
    static let maxN = 4096
    private let a = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN)
    private let b = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN)
    private let wl = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN)
    private let wr = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN)
    private let gBuf: UnsafeMutablePointer<Float> = { let g = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN); g.initialize(repeating: 1, count: DSP.maxN); return g }()   // gate gain per sample, for the post-cab stage
    private let fade: Float

    // input + gate
    private var inG = Smooth()
    private var env: Float = 0, gGain: Float = 1, gTarget: Float = 1, gHold = 0
    private var gThr: Float = 0, gRelC: Float = 0, gEnvR: Float = 0, gAttC: Float = 0, gFloor: Float = 0

    // pitch
    private let shifter: PitchShifter
    private var pMix = Smooth()

    // pedals, one instance per kind, run in the order of p.chain
    private let pedals: [Pedal]

    // metronome
    private var mCount = 0, mBeat = -1, mClick = Int.max, mAccent = false, mWas = false

    // amp
    private let aHP = Biquad(), aTight = Biquad(), aEmph = Biquad()
    private let up = (0..<4).map { _ in Biquad() }, down = (0..<4).map { _ in Biquad() }
    private let aLP1 = Biquad(), aIHP = Biquad(), aLP2 = Biquad(), aLP3 = Biquad()
    private let aChar = Biquad(), aBass = Biquad(), aMid = Biquad(), aTreb = Biquad(), aDepth = Biquad(), aPres = Biquad(), aDC = Biquad()
    private var ampMix = Smooth(), aPre = Smooth(), aVol = Smooth()
    private var v = VOICINGS[2], tb1: Float = 0, tb2: Float = 0

    // captured amp (model fitted by Capture/fit.py)
    private let kHP = Biquad(), kLS = Biquad(), kPk = Biquad(), kHS = Biquad(), kMid = Biquad()
    private var capOn = false, kK1: Float = 1, kB1: Float = 0, kK2: Float = 1, kB2: Float = 0, kT1: Float = 0, kT2: Float = 0
    private var kLvl = Smooth()

    // NAM capture: model pointer swapped in by the main thread (shared C++ bridge, same code as the Windows app)
    private var nam: OpaquePointer?, namGain: Float = 1, namCal: Float = 1   // namCal: input calibration gain
    private var namInG = Smooth(), namOutG = Smooth()
    private let namBuf = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN)

    // eq
    private let eqF: [Double] = [80, 160, 320, 640, 1250, 2500, 5000, 10000]
    private let eqB = (0..<8).map { _ in Biquad() }
    private let oeqL = (0..<3).map { _ in Biquad() }, oeqR = (0..<3).map { _ in Biquad() }
    private var oeqOn = false, oeqG: Float = 1
    private var eqMix = Smooth()

    // cab: FIR via vDSP_conv, double-buffered so the UI can swap IRs
    static let maxIR = 4096
    private let irs = [UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxIR), UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxIR)]
    private var irLen = [0, 0]
    private var irActive = 0
    private let hist = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxIR + DSP.maxN)
    // Amp B: its capture, the pre-amp copy it runs on, and the side signal (A - B) with its own EQ, cab and filter state
    private var namB: OpaquePointer?, namBGain: Float = 1, namBCal: Float = 1, namBOwned = false
    private var bMix = Smooth(), bLvl = Smooth(), bSpread = Smooth(), sideWasOn = false
    private let bBuf: UnsafeMutablePointer<Float> = { let x = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN); x.initialize(repeating: 0, count: DSP.maxN); return x }()
    private let sBuf: UnsafeMutablePointer<Float> = { let x = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN); x.initialize(repeating: 0, count: DSP.maxN); return x }()
    private let sConv: UnsafeMutablePointer<Float> = { let x = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN); x.initialize(repeating: 0, count: DSP.maxN); return x }()
    private let hist2: UnsafeMutablePointer<Float> = { let x = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxIR + DSP.maxN); x.initialize(repeating: 0, count: DSP.maxIR + DSP.maxN); return x }()
    private let eqS = (0..<8).map { _ in Biquad() }
    private let cLoS = Biquad(), cHi1S = Biquad(), cHi2S = Biquad()
    private let cLo = Biquad(), cHi1 = Biquad(), cHi2 = Biquad()
    private var cabMix = Smooth(), cabLvl = Smooth()

    // delay
    private let dBuf: UnsafeMutablePointer<Float>, dSize: Int
    private var dW = 0, dTime = Smooth(), dSend = Smooth(), dFb: Float = 0, dWet: Float = 0
    private let dLP = Biquad()

    // reverb (feedback delay network)
    private let reverb: FDNReverb
    private var rSend = Smooth(), rWet: Float = 0
    // doubler: the side signal is the difference of two slowly drifting delayed copies (mid stays dry, mono-safe)
    private static let dbSize = 16384, dbMask = 16383   // 40 ms + drift fits at 192 kHz
    private let dbBuf: UnsafeMutablePointer<Float> = { let b = UnsafeMutablePointer<Float>.allocate(capacity: 16384); b.initialize(repeating: 0, count: 16384); return b }()
    private var dbW = 0, dbPh1: Float = 0, dbPh2: Float = 0, dbInc1: Float = 0, dbInc2: Float = 0
    private var dbWidth = Smooth(), dbTime = Smooth()

    // output limiter: stops a hot Master from hard-clipping (which sounds like crackle)
    private var limG: Float = 1, limRel: Float = 0
    static let limCeiling: Float = 0.891   // -1 dBFS

    init(sampleRate: Double, params: Params) {
        sr = Float(sampleRate); p = params
        shifter = PitchShifter(sr: sampleRate)
        pedals = PedalKind.allCases.map { makePedal($0, sr: Float(sampleRate)) }
        fade = 1 - expf(-1 / (0.006 * Float(sampleRate)))
        limRel = 1 - expf(-1 / (0.08 * Float(sampleRate)))
        dSize = Int(sampleRate * 2.1)
        dBuf = .allocate(capacity: dSize); dBuf.initialize(repeating: 0, count: dSize)
        hist.initialize(repeating: 0, count: DSP.maxIR + DSP.maxN)
        irs[0].initialize(repeating: 0, count: DSP.maxIR); irs[1].initialize(repeating: 0, count: DSP.maxIR)
        irs[0][0] = 1; irLen[0] = 1
        reverb = FDNReverb(sr: sampleRate)
        let os = sampleRate * 4, aa = min(20000, sampleRate * 0.45)
        for (i, q) in [0.5098, 0.6013, 0.9000, 2.5629].enumerated() {
            up[i].set(.lp, aa, os, q: q); down[i].set(.lp, aa, os, q: q)
        }
        aDC.set(.hp, 15, sampleRate)
        update(force: true)
    }

    /// Swap in a NAM model (already reset for this rate and maxN) and its loudness-normalising gain.
    /// The caller keeps ownership and frees the previous model once the audio thread has moved on.
    func setNam(_ m: OpaquePointer?, gain: Float, input: Float = 1, owned: Bool = false) { namGain = gain; namCal = input; namOwned = owned; OSMemoryBarrier(); nam = m }
    func setNamB(_ m: OpaquePointer?, gain: Float, input: Float = 1, owned: Bool = false) { namBGain = gain; namBCal = input; namBOwned = owned; OSMemoryBarrier(); namB = m }
    private var namOwned = false
    deinit { if namOwned, let nam { lev_nam_free(nam) }; if namBOwned, let namB { lev_nam_free(namB) } }

    /// Swap in a new cabinet impulse response (called from the main thread).
    func setIR(_ h: [Float]) {
        let slot = 1 - irActive, n = min(h.count, DSP.maxIR)
        h.withUnsafeBufferPointer { irs[slot].update(from: $0.baseAddress!, count: n) }
        irLen[slot] = n
        irActive = slot
    }

    private func update(force: Bool = false) {
        guard force || p.version != lastVersion else { return }
        lastVersion = p.version
        let s = Double(sr), os = s * 4
        inG.target = dbLin(p.inGain)
        gThr = dbLin(p.gateThresh)
        // Depth: how far a closed gate turns things down; -60 dB means fully closed. Half of it at each of the two stages.
        gFloor = p.gateDepth <= -60 ? 0 : dbLin(p.gateDepth / 2)
        // Release = time to fall 60 dB (closed), not one time constant: after a high-gain amp a gate that is only
        // ~9 dB down still sounds open, so the old curve left a long audible tail. The gain is applied twice, before the
        // pedals and again after the cab, so each stage falls 30 dB over Release: a clean tone fades exactly as before,
        // and a high-gain rig (which holds a fading input at full level until it is ~40 dB down) still fades evenly
        // instead of hanging at full level and then dropping all at once.
        // the last stretch of the dial (2050 ms and up) is infinite: once open the gate never fades
        gRelC = p.gateRelease >= 2050 ? 1 : expf(logf(0.001) / 2 / (p.gateRelease / 1000 * sr))
        gEnvR = expf(-1 / (0.015 * sr)); gAttC = expf(-1 / (0.0003 * sr))

        shifter.setRatio(pow(2, Double(p.pitchSemis.rounded() + p.pitchCents / 100) / 12))
        shifter.lowLag = p.pitchLowLag
        pMix.target = p.pitchOn ? p.pitchMix : 0

        for pd in pedals { pd.update(p) }

        v = VOICINGS[min(max(p.ampChannel, 0), VOICINGS.count - 1)]
        ampMix.target = p.ampOn ? 1 : 0
        aHP.set(.hp, v.hp, s, q: 0.7); aTight.set(.lowShelf, 250, s, db: v.tight); aEmph.set(.peak, 1800, s, q: 0.6, db: v.emph)
        aPre.target = 1 + (v.preMax - 1) * powf(p.ampGain, 1.5)
        tb1 = tanhf(v.b1); tb2 = tanhf(v.b2)
        aLP1.set(.lp, v.lp1, os); aIHP.set(.hp, v.interHP, os); aLP2.set(.lp, v.lp2, os); aLP3.set(.lp, v.lp3, os)
        aChar.set(.peak, 800, s, q: 0.8, db: v.character)
        aBass.set(.lowShelf, 110, s, db: Double((p.bass - 0.5) * 24))
        aMid.set(.peak, v.mid, s, q: 0.6, db: Double((p.mid - 0.5) * 24))
        aTreb.set(.highShelf, 2200, s, db: Double((p.treble - 0.5) * 24))
        aDepth.set(.peak, 90, s, q: 0.9, db: Double((p.depth - 0.5) * 20))
        aPres.set(.highShelf, 3500, s, db: Double((p.presence - 0.5) * 20))
        aVol.target = dbLin(-30 + p.ampVolume * 36) * v.out

        capOn = p.capOn
        kHP.set(.hp, Double(p.capHP), s, q: 0.7071); kLS.set(.lowShelf, 250, s, db: Double(p.capLS))
        kPk.set(.peak, Double(p.capPkF), s, q: 0.7, db: Double(p.capPk)); kHS.set(.highShelf, 3000, s, db: Double(p.capHS))
        kMid.set(.lp, Double(p.capMid), os)
        kK1 = p.capK1 * dbLin(p.capDrive); kB1 = p.capB1; kK2 = p.capK2; kB2 = p.capB2
        kT1 = tanhf(kB1); kT2 = tanhf(kB2)
        kLvl.target = dbLin(p.capLevel)
        namInG.target = dbLin(p.namIn); namOutG.target = dbLin(p.namOut)
        if force { namInG.v = namInG.target; namOutG.v = namOutG.target }

        eqMix.target = p.eqOn ? 1 : 0
        for i in 0..<8 {
            let kind: Biquad.Kind = i == 0 ? .lowShelf : i == 7 ? .highShelf : .peak
            eqB[i].set(kind, eqF[i], s, q: 1.4, db: Double(p.eq[i]))
            eqS[i].set(kind, eqF[i], s, q: 1.4, db: Double(p.eq[i]))
        }

        oeqOn = p.outEqOn; oeqG = dbLin(p.outEqLevel)
        for f in [oeqL, oeqR] {
            f[0].set(.lowShelf, 100, s, db: Double(p.outEqLow))
            f[1].set(.peak, 1000, s, q: 0.7, db: Double(p.outEqMid))
            f[2].set(.highShelf, 6000, s, db: Double(p.outEqHigh))
        }

        cabMix.target = p.cabOn ? 1 : 0; cabLvl.target = dbLin(p.cabLevel)
        // a capture's IR already is the cab: leave it unfiltered so it stays faithful
        let lowCut = p.capOn ? 10 : Double(p.cabLowCut), highCut = p.capOn ? s * 0.47 : Double(p.cabHighCut)
        cLo.set(.hp, lowCut, s); cHi1.set(.lp, highCut, s, q: 0.54); cHi2.set(.lp, highCut, s, q: 1.31)
        cLoS.set(.hp, lowCut, s); cHi1S.set(.lp, highCut, s, q: 0.54); cHi2S.set(.lp, highCut, s, q: 1.31)
        bMix.target = p.ampBOn ? 1 : 0; bLvl.target = dbLin(p.ampBLevel); bSpread.target = p.ampBSpread
        if force { bLvl.v = bLvl.target; bSpread.v = bSpread.target }

        dSend.target = p.delayOn ? 1 : 0; dTime.target = p.delayTime / 1000 * sr
        dFb = p.delayFeedback; dWet = p.delayMix * 1.4; dLP.set(.lp, Double(lerp(p.delayTone, 1200, 9000)), s)
        if force { dTime.v = dTime.target }

        rSend.target = p.revOn ? 1 : 0; rWet = p.revMix * 3
        dbWidth.target = p.doublerOn ? p.doublerWidth : 0
        dbTime.target = min(max(p.doublerTime, 5), 40) / 1000 * sr
        dbInc1 = 0.11 / sr; dbInc2 = 0.17 / sr
        if dbTime.v == 0 { dbTime.v = dbTime.target }
        reverb.set(size: p.revSize, sr: sr)
    }

    func process(input: UnsafePointer<Float>, outL: UnsafeMutablePointer<Float>, outR: UnsafeMutablePointer<Float>, frames: Int) {
        update()
        let n = min(frames, DSP.maxN), c = fade
        var inPk: Float = 0, w = p.tunerWrite
        let ring = p.tunerRing
        let recording = p.recording

        // reamp: a stored dry take replaces the live input
        var src = input
        if p.srcPlaying, let sp = p.srcPtr {
            let len = p.srcLen
            var pos = p.srcPos
            let loop = p.srcLoop
            for i in 0..<n {
                if pos >= len && loop { pos = 0 }
                b[i] = pos < len ? sp[pos] : 0; pos += 1
            }
            p.srcPos = pos
            if pos >= len && !loop { p.srcPlaying = false; p.srcDone = true }
            src = UnsafePointer(b)
        }
        if recording { p.recDry.write(src, n) }
        var clips = 0
        for i in 0..<n where abs(src[i]) >= 0.989 { clips += 1 }   // >= -0.1 dBFS
        if clips > 0 { p.inClips &+= clips }

        // input gain, tuner tap, gate
        let close = gThr * 0.5, holdN = Int(0.012 * sr), gateOn = p.gateOn
        for i in 0..<n {
            let x = src[i] * inG.next(c)
            inPk = max(inPk, abs(x))
            ring[w] = x; w = (w + 1) & 4095
            let ax = abs(x)
            env = ax > env ? ax : env * gEnvR + ax * (1 - gEnvR)
            if env > gThr { gTarget = 1; gHold = holdN } else if env < close { if gHold > 0 { gHold -= 1 } else { gTarget = 0 } }
            gGain = gTarget + (gGain - gTarget) * (gTarget > gGain ? gAttC : gRelC)
            let g = gFloor + (1 - gFloor) * gGain
            gBuf[i] = g
            a[i] = gateOn ? x * g : x
        }
        p.tunerWrite = w; p.inPeak = max(inPk, p.inPeak * 0.9); p.gateOpen = gTarget > 0 || !gateOn

        // pitch shift
        if p.pitchOn || pMix.v > 1e-4 {
            for i in 0..<n {
                let m = pMix.next(c), x = a[i]
                a[i] = x + (shifter.process(x) - x) * m
            }
            p.pitchLatencyMs = p.pitchOn && shifter.ratio != 1 ? Float(shifter.shownDelay) / sr * 1000 : 0
        } else {
            for i in 0..<n { shifter.write(a[i]) }
            p.pitchLatencyMs = 0
        }

        // pedal chain, in user order
        for k in 0..<min(p.chainCount, Params.maxChain) {
            let id = Int(p.chain[k])
            if id >= 0 && id < pedals.count { pedals[id].process(a, n, c) }
        }

        // Amp B runs on the same pedal output: keep a copy before amp A changes it
        let nb = namB, sideOn = nb != nil && (bMix.target > 0 || bMix.v > 1e-4)
        if sideOn { let k = namBCal; for i in 0..<n { bBuf[i] = a[i] * k }; if !sideWasOn { hist2.update(repeating: 0, count: DSP.maxIR + DSP.maxN) } }

        // NAM capture in place of the amp (block-processed; same bridge as the Windows app)
        let nm = p.namOn ? nam : nil
        if let nm {
            let k = namCal
            for i in 0..<n { namBuf[i] = a[i] * namInG.next(c) * k }
            let t0 = DispatchTime.now().uptimeNanoseconds
            lev_nam_process(nm, namBuf, namBuf, Int32(n))
            p.namSeconds += Double(DispatchTime.now().uptimeNanoseconds - t0) / 1e9
            let g = namGain
            for i in 0..<n {
                let m = ampMix.next(c), x = a[i]
                _ = aPre.next(c); _ = aVol.next(c)
                a[i] = x + (namBuf[i] * g * namOutG.next(c) - x) * m
            }
        }
        // amp, 4x oversampled through the clipping stages
        for i in 0..<(nm == nil ? n : 0) {
            let m = ampMix.next(c), x = a[i]
            if m < 1e-4 { continue }
            let pre = aPre.next(c), vol = aVol.next(c)
            if capOn {
                // same structure as fit.py: filters -> tanh -> LP -> tanh, oversampled; the FIR runs in the cab slot
                let u0 = kHS.run(kPk.run(kLS.run(kHP.run(x)))) * 4
                var o: Float = 0
                for k in 0..<4 {
                    var u = up[3].run(up[2].run(up[1].run(up[0].run(k == 0 ? u0 : 0))))
                    u = tanhf(kK1 * u + kB1) - kT1
                    u = kMid.run(u)
                    u = tanhf(kK2 * u + kB2) - kT2
                    o = down[3].run(down[2].run(down[1].run(down[0].run(u))))
                }
                a[i] = x + (o * kLvl.next(c) - x) * m
                continue
            }
            let vin = aEmph.run(aTight.run(aHP.run(x))) * pre * 4
            var o: Float = 0
            for k in 0..<4 {
                var u = up[3].run(up[2].run(up[1].run(up[0].run(k == 0 ? vin : 0))))
                u = tanhf(u + v.b1) - tb1
                u = aIHP.run(aLP1.run(u))
                if v.cascade {
                    u = aLP2.run(tanhf(u * v.s2 + v.b2) - tb2)
                    u = aLP3.run(tanhf(u * v.s3))
                }
                o = down[3].run(down[2].run(down[1].run(down[0].run(u))))
            }
            var y = aTreb.run(aMid.run(aBass.run(aChar.run(o))))
            y = tanhf(y * v.power)
            y = aDC.run(aPres.run(aDepth.run(y))) * vol
            // its odd number of inverting stages flip the signal; turn it back so it lines up with NAM captures
            // (blended as amp A and B they'd otherwise partly cancel) and with the dry signal while switching
            a[i] = x + (-y - x) * m
        }

        // Amp B: mid = blend of A and B, side = (A - B) / 2 x Spread; side goes through its own EQ and cab
        if sideOn, let nb {
            let t0 = DispatchTime.now().uptimeNanoseconds
            lev_nam_process(nb, bBuf, bBuf, Int32(n))
            p.namSeconds += Double(DispatchTime.now().uptimeNanoseconds - t0) / 1e9
            let g = namBGain
            for i in 0..<n {
                let m = bMix.next(c), A = a[i], B = bBuf[i] * g * bLvl.next(c)
                a[i] = A + ((A + B) * 0.5 - A) * m
                sBuf[i] = (A - B) * 0.5 * bSpread.next(c) * m
            }
        }
        sideWasOn = sideOn

        // graphic eq
        for i in 0..<n {
            let m = eqMix.next(c), x = a[i]
            if m < 1e-4 { continue }
            var y = x
            for bq in eqB { y = bq.run(y) }
            a[i] = x + (y - x) * m
            if sideOn {
                let xs = sBuf[i]
                var ys = xs
                for bq in eqS { ys = bq.run(ys) }
                sBuf[i] = xs + (ys - xs) * m
            }
        }

        // cabinet
        let slot = irActive, L = irLen[slot]
        let tail = DSP.maxIR - 1
        memmove(hist, hist + n, tail * MemoryLayout<Float>.size)   // shift history left by n
        (hist + tail).update(from: a, count: n)
        vDSP_conv(hist + tail - (L - 1), 1, irs[slot] + (L - 1), -1, b, 1, vDSP_Length(n), vDSP_Length(L))
        if sideOn {
            memmove(hist2, hist2 + n, tail * MemoryLayout<Float>.size)
            (hist2 + tail).update(from: sBuf, count: n)
            vDSP_conv(hist2 + tail - (L - 1), 1, irs[slot] + (L - 1), -1, sConv, 1, vDSP_Length(n), vDSP_Length(L))
        }
        for i in 0..<n {
            let m = cabMix.next(c), lvl = cabLvl.next(c)
            let y = cHi2.run(cHi1.run(cLo.run(b[i]))) * lvl
            a[i] += (y - a[i]) * m
            if sideOn {
                let ys = cHi2S.run(cHi1S.run(cLoS.run(sConv[i]))) * lvl
                sBuf[i] += (ys - sBuf[i]) * m
            }
        }

        // gate, second stage: the same fade after the amp and cab (delay and reverb tails still ring out)
        if gateOn { for i in 0..<n { a[i] *= gBuf[i] } }
        if gateOn && sideOn { for i in 0..<n { sBuf[i] *= gBuf[i] } }

        // delay (send stays live so tails ring out when switched off)
        for i in 0..<n {
            let d = min(max(dTime.next(0.0005), 1), Float(dSize - 2))
            var rp = Float(dW) - d; if rp < 0 { rp += Float(dSize) }
            if rp >= Float(dSize) { rp -= Float(dSize) }   // a tiny negative rp + dSize can round up to exactly dSize
            let i0 = Int(rp), fr = rp - Float(i0), i1 = (i0 + 1) % dSize
            let wet = dLP.run(dBuf[i0] + (dBuf[i1] - dBuf[i0]) * fr)
            dBuf[dW] = a[i] * dSend.next(c) + wet * dFb + 1e-20
            dW = (dW + 1) % dSize
            a[i] += wet * dWet
        }

        // reverb + master
        let vol = dbLin(p.masterVol), mute = p.muted
        var outPk: Float = 0
        for i in 0..<n {
            let x = a[i], rin = x * rSend.next(c) * 0.015
            var l: Float = 0, r: Float = 0
            reverb.process(rin, &l, &r)
            // doubler: two copies ~T and ~0.62 T behind, each drifting ±6-8 % at its own slow rate (about ±1.5 cents)
            dbBuf[dbW] = x
            dbPh1 += dbInc1; if dbPh1 >= 1 { dbPh1 -= 1 }
            dbPh2 += dbInc2; if dbPh2 >= 1 { dbPh2 -= 1 }
            let tb = dbTime.next(0.0005), w = dbWidth.next(c)
            var side: Float = 0
            if w > 1e-4 {
                let t1 = tb * (1 + 0.06 * sinf(2 * .pi * dbPh1)), t2 = tb * 0.62 * (1 + 0.08 * sinf(2 * .pi * dbPh2))
                let r1 = Float(dbW) - t1 + Float(DSP.dbSize), r2 = Float(dbW) - t2 + Float(DSP.dbSize)
                let i1 = Int(r1), f1 = r1 - Float(i1), i2 = Int(r2), f2 = r2 - Float(i2)
                let d1 = dbBuf[i1 & DSP.dbMask] + (dbBuf[(i1 + 1) & DSP.dbMask] - dbBuf[i1 & DSP.dbMask]) * f1
                let d2 = dbBuf[i2 & DSP.dbMask] + (dbBuf[(i2 + 1) & DSP.dbMask] - dbBuf[i2 & DSP.dbMask]) * f2
                side = (d1 - d2) * 0.5 * w
            }
            dbW = (dbW + 1) & DSP.dbMask
            let sb: Float = sideOn ? sBuf[i] : 0
            var yl = (x + side + sb + l * rWet) * vol, yr = (x - side - sb + r * rWet) * vol
            // peak limiter: gain drops at once to keep the peak at the ceiling, then recovers over ~80 ms
            let pk = max(abs(yl), abs(yr)), target = pk > DSP.limCeiling ? DSP.limCeiling / pk : 1
            limG = target < limG ? target : limG + (target - limG) * limRel
            yl *= limG; yr *= limG
            yl = min(max(yl, -1), 1); yr = min(max(yr, -1), 1)
            outPk = max(outPk, abs(yl))
            wl[i] = yl; wr[i] = yr
        }
        p.outPeak = max(outPk, p.outPeak * 0.9)
        if recording { p.recL.write(wl, n); p.recR.write(wr, n) }

        // backing track + metronome ride on top of the rig, after the recorder tap
        let bkOn = p.bkPlaying, bl = p.bkL, br = p.bkR, bLen = p.bkLen, bg = dbLin(p.bkVol)
        var bPos = p.bkPos
        if bkOn, p.bkSeek >= 0 { bPos = min(p.bkSeek, max(bLen - 1, 0)); p.bkSeek = -1 }
        let metro = p.metroOn, mg = dbLin(p.metroVol)
        if metro && !mWas { mCount = 0; mBeat = -1 }
        mWas = metro
        let beatLen = max(Int(60 / max(p.bpm, 20) * sr), 1), beats = max(p.beatsPerBar, 1)
        let clickLen = Int(0.08 * sr), clickDecay = 1 / (0.022 * sr)
        for i in 0..<n {
            var yl = wl[i], yr = wr[i]
            if bkOn, let bl, let br, bLen > 0 {
                if bPos >= bLen { if p.bkLoop { bPos = 0 } else { p.bkPlaying = false } }
                if bPos < bLen { yl += bl[bPos] * bg; yr += br[bPos] * bg; bPos += 1 }
            }
            if metro {
                if mCount == 0 { mBeat = (mBeat + 1) % beats; mClick = 0; mAccent = mBeat == 0 }
                mCount += 1; if mCount >= beatLen { mCount = 0 }
                if mClick < clickLen {
                    let t = Float(mClick)
                    // woodblock-like: a pitched tone plus its fifth below for body; accent is higher and louder
                    let f: Float = mAccent ? 1500 : 1000, env = expf(-t * clickDecay) * mg * (mAccent ? 1 : 0.75)
                    let s = (sinf(2 * .pi * f * t / sr) * 0.7 + sinf(2 * .pi * f * 0.667 * t / sr) * 0.3) * env
                    yl += s; yr += s; mClick += 1
                }
            }
            if oeqOn {
                yl = oeqL[2].run(oeqL[1].run(oeqL[0].run(yl))) * oeqG
                yr = oeqR[2].run(oeqR[1].run(oeqR[0].run(yr))) * oeqG
            }
            outL[i] = mute ? 0 : min(max(yl, -1), 1); outR[i] = mute ? 0 : min(max(yr, -1), 1)
        }
        if bkOn { p.bkPos = bPos }
        p.metroBeat = metro ? mBeat : -1
        if n < frames { for i in n..<frames { outL[i] = 0; outR[i] = 0 } }
    }
}

/// Stereo reverb: a 12 ms pre-delay, four allpass diffusers, then an 8-line feedback delay network mixed by a
/// Householder matrix, with a one-pole damping filter in each line so highs die away first. Size sets the decay
/// (RT60 from 0.35 s to 5.35 s) and darkens bigger rooms. Replaces Freeverb, which rang and sounded metallic.
/// windows/Source/Dsp.h FDNReverb is the same, operation for operation.
final class FDNReverb {
    static let lineLens = [1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116]   // at 44.1 kHz
    static let diffLens = [142, 107, 379, 277]
    static let outScale: Float = 3.8   // as loud as the Freeverb it replaces at the same Mix
    private let lines: [UnsafeMutablePointer<Float>], lens: [Int]
    private var pos = [Int](repeating: 0, count: 8), lp = [Float](repeating: 0, count: 8), g = [Float](repeating: 0, count: 8)
    private let o = UnsafeMutablePointer<Float>.allocate(capacity: 8)
    private let dif: [UnsafeMutablePointer<Float>], dlens: [Int]
    private var dpos = [Int](repeating: 0, count: 4)
    private let pre: UnsafeMutablePointer<Float>, preLen: Int
    private var prePos = 0, damp: Float = 0.3

    init(sr: Double) {
        let scale = sr / 44100
        lens = FDNReverb.lineLens.map { max(Int(Double($0) * scale), 1) }
        lines = lens.map { n in let b = UnsafeMutablePointer<Float>.allocate(capacity: n); b.initialize(repeating: 0, count: n); return b }
        dlens = FDNReverb.diffLens.map { max(Int(Double($0) * scale), 1) }
        dif = dlens.map { n in let b = UnsafeMutablePointer<Float>.allocate(capacity: n); b.initialize(repeating: 0, count: n); return b }
        preLen = max(Int(sr * 0.012), 2)
        pre = .allocate(capacity: preLen); pre.initialize(repeating: 0, count: preLen)
        o.initialize(repeating: 0, count: 8)
    }

    func set(size: Float, sr: Float) {
        let rt = 0.35 + size * size * 5
        for i in 0..<8 { g[i] = powf(10, -3 * Float(lens[i]) / (rt * sr)) }
        damp = 0.15 + 0.35 * size
    }

    @inline(__always) func process(_ x: Float, _ l: inout Float, _ r: inout Float) {
        pre[prePos] = x
        var pr = prePos + 1; if pr == preLen { pr = 0 }
        prePos = pr
        var v = pre[pr]
        for k in 0..<4 {
            let b = dif[k][dpos[k]], y = b - 0.6 * v
            dif[k][dpos[k]] = v + 0.6 * y
            dpos[k] += 1; if dpos[k] == dlens[k] { dpos[k] = 0 }
            v = y
        }
        var sum: Float = 0
        for i in 0..<8 {
            lp[i] += (lines[i][pos[i]] - lp[i]) * (1 - damp)
            o[i] = lp[i] * g[i]
            sum += o[i]
        }
        let h = sum * 0.25
        var ll: Float = 0, rr: Float = 0
        for i in 0..<8 {
            let y = lines[i][pos[i]]
            switch i % 4 { case 0: ll += y; case 1: rr += y; case 2: ll -= y; default: rr -= y }
            lines[i][pos[i]] = o[i] - h + v + 1e-20   // same input to every line; the output signs decorrelate L and R
            pos[i] += 1; if pos[i] == lens[i] { pos[i] = 0 }
        }
        l = ll * FDNReverb.outScale; r = rr * FDNReverb.outScale
    }
}

// MARK: - Pitch shifter
//
// Delay-line shifter with a single moving read tap. When the tap drifts too close to (shifting up) or too far
// from (shifting down) the write head it jumps by one or more "periods", choosing the jump that best matches the
// waveform it's leaving (WSOLA-style), then crossfades. Matching splices keep chords from warbling, and the
// tap tracks the input's period, so it sits only about one period (plus the 2 ms of the 10 ms match window that lies
// after the splice point; the other 8 ms are before it) behind: measured 4.5 ms (E4), 9 ms (E2), 15 ms (E2 power chord).
// The match window and the 30 ms search must both exceed a low power chord's common period (root and fifth realign
// every two root cycles: 24 ms on E2, 26 ms on Eb2); shorter ones spliced at false matches and smeared those chords.

final class PitchShifter {
    private let size = 32768, mask = 32767   // room for the 30 ms search at 192 kHz
    private let buf: UnsafeMutablePointer<Float>          // mirrored so any L-sample window is contiguous
    private var w = 0
    private var read = 0.0, oldRead = 0.0
    private var fade: Int
    private let fadeLen: Int, L: Int, back: Int, ahead: Int, jMin: Int, jMinChord: Int, jMax: Int, margin: Int
    private(set) var ratio = 1.0
    private(set) var avgDelay = 0.0
    /// Low lag: treat everything as one note, so chords splice at the root's period instead of waiting for the whole
    /// chord to repeat (much less lag on low chords, at the cost of a slight shimmer on them).
    var lowLag = false
    private(set) var shownDelay = 0.0   // ~1 s average of the lag, for the readout (avgDelay follows it within each splice)
    // running estimate of the input's period: when the signal is clearly periodic (single notes, riffs) a
    // down-shift jumps as soon as one period has built up instead of always waiting for the longest possible
    // period (about 40 ms behind the dry signal). Chords have no short common period, so they keep the long search.
    private var period: Int, periodScore: Float = 0, lastScore: Float = 0, estCount = 0
    private let estEvery: Int
    private let scratch: UnsafeMutablePointer<Float>

    init(sr: Double) {
        L = Int(sr * 0.010); back = L * 4 / 5; ahead = L - back; fadeLen = Int(sr * 0.006); jMin = Int(sr * 0.0025); jMinChord = Int(sr * 0.004); jMax = Int(sr * 0.030)
        margin = Int(sr * 0.0008); estEvery = Int(sr * 0.006)
        period = Int(sr * 0.008)
        fade = fadeLen
        buf = .allocate(capacity: size * 2); buf.initialize(repeating: 0, count: size * 2)
        scratch = .allocate(capacity: jMax + 1); scratch.initialize(repeating: 0, count: jMax + 1)
    }

    func setRatio(_ r: Double) {
        guard r != ratio else { return }
        if ratio == 1 { read = Double(w) - Double(ahead + period + margin + 8); fade = fadeLen; avgDelay = Double(ahead + period) }
        ratio = abs(r - 1) < 1e-6 ? 1 : r
    }

    @inline(__always) func write(_ x: Float) {
        let i = w & mask; buf[i] = x; buf[i + size] = x; w += 1
    }

    @inline(__always) private func at(_ i: Int) -> Float { buf[i & mask] }

    @inline(__always) private func interp(_ pos: Double) -> Float {
        let i = Int(pos.rounded(.down)), f = Float(pos - Double(i))
        let xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2)
        let c1 = 0.5 * (x1 - xm1), c2 = xm1 - 2.5 * x0 + 2 * x1 - 0.5 * x2, c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1)
        return ((c3 * f + c2) * f + c1) * f + x0
    }

    func process(_ x: Float) -> Float {
        write(x)
        if ratio == 1 { return x }
        estCount += 1
        if estCount >= estEvery {
            estCount = 0
            period = bestJump(w - ahead, up: true, maxJ: jMax)
            periodScore = lastScore
        }
        var d = Double(w) - read
        if d < 4 || d > Double(size - 2 * jMax) {
            read = Double(w) - Double(ahead + period + margin + 8); fade = fadeLen; d = Double(w) - read
        }
        if fade >= fadeLen {
            let periodic = lowLag || periodScore > 0.96
            if ratio > 1 {
                if d <= Double(max(ahead, Int(Double(fadeLen) * (ratio - 1))) + 8) {
                    let j = bestJump(Int(read), up: true, maxJ: jMax, minJ: Int(Double(fadeLen) * (ratio - 1)) + 2, preferShort: periodic)
                    oldRead = read; read -= Double(j); fade = 0
                }
            } else if d >= Double((periodic ? min(period + margin, jMax) : jMax) + ahead + 8) {
                let j = bestJump(Int(read), up: false, maxJ: min(jMax, Int(d) - ahead - 8), preferShort: periodic)
                oldRead = read; read += Double(j); fade = 0
            }
        }
        var y = interp(read)
        if fade < fadeLen {
            let g = Float(0.5 - 0.5 * cos(Double.pi * Double(fade) / Double(fadeLen)))
            y = y * g + interp(oldRead) * (1 - g)
            oldRead += ratio; fade += 1
        }
        read += ratio
        avgDelay += (d - avgDelay) * 0.0005
        shownDelay += (d - shownDelay) * 0.00002
        return y
    }

    /// Best splice distance: the shortest lag whose normalised correlation is a peak within 3% of the best one.
    /// Preferring the shortest good lag (one period, not two) keeps the read tap close to the write head.
    private func bestJump(_ base: Int, up: Bool, maxJ: Int, minJ: Int = 0, preferShort: Bool = true) -> Int {
        let jMin = min(max(preferShort ? self.jMin : jMinChord, minJ), jMax)
        guard maxJ >= jMin else { return max(maxJ, 1) }
        // the L-sample match window around the splice point: back samples before it (most of it), ahead after
        let ref = buf + ((base - back) & mask)
        var re: Float = 0
        vDSP_svesq(ref, 1, &re, vDSP_Length(L))
        if re < 1e-10 { lastScore = 0; return min(max(period, jMin), maxJ) }
        var best = jMin, bestScore = -Float.infinity
        var firstGood = -1
        let count = maxJ - jMin + 1
        let scores = scratch
        for k in 0..<count {
            let j = jMin + k
            let cand = buf + (((up ? base - j : base + j) - back) & mask)
            var dot: Float = 0, e: Float = 0
            vDSP_dotpr(ref, 1, cand, 1, &dot, vDSP_Length(L))
            vDSP_svesq(cand, 1, &e, vDSP_Length(L))
            let s = dot / sqrtf(e * re + 1e-12)
            scores[k] = s
            if s > bestScore { bestScore = s; best = j }
        }
        // shortest lag that is a local peak and close to the best score
        // Low lag accepts a much rougher match (a chord's root period rather than the whole chord repeating)
        let thr = bestScore - (lowLag ? 0.30 : 0.03) * abs(bestScore)
        if preferShort && count >= 3 {
            for k in 1..<(count - 1) where scores[k] >= thr && scores[k] >= scores[k - 1] && scores[k] >= scores[k + 1] {
                firstGood = jMin + k; break
            }
        }
        let pick = firstGood > 0 ? firstGood : best
        lastScore = scores[pick - jMin]
        return pick
    }
}

// MARK: - Cabinet impulse responses

struct CabModel { let name: String; let build: (_ mic: Double, _ sr: Double) -> [Biquad] }

let CABS: [CabModel] = [
    CabModel(name: "4x12 Modern") { mic, sr in
        let lp = 7200 - mic * 3400
        return [Biquad(.hp, 70, sr, q: 1.0), Biquad(.peak, 110, sr, q: 1.2, db: 4), Biquad(.peak, 220, sr, q: 1.0, db: 1.5),
                Biquad(.peak, 1400, sr, q: 1.2, db: -1), Biquad(.peak, 2600, sr, q: 1.8, db: 4), Biquad(.peak, 3900, sr, q: 3, db: -2.5),
                Biquad(.peak, 5000, sr, q: 4, db: 2), Biquad(.lp, lp, sr, q: 0.54), Biquad(.lp, lp, sr, q: 1.31), Biquad(.lp, 9500, sr)]
    },
    CabModel(name: "4x12 Vintage") { mic, sr in
        let lp = 6000 - mic * 2800
        return [Biquad(.hp, 70, sr, q: 1.0), Biquad(.peak, 120, sr, q: 1.2, db: 4), Biquad(.peak, 800, sr, q: 0.8, db: 1),
                Biquad(.peak, 2000, sr, q: 1.5, db: 3.5), Biquad(.peak, 3400, sr, q: 2.5, db: -3),
                Biquad(.lp, lp, sr, q: 0.54), Biquad(.lp, lp, sr, q: 1.31)]
    },
    CabModel(name: "2x12 Open Back") { mic, sr in
        let lp = 7800 - mic * 3500
        return [Biquad(.hp, 90, sr, q: 0.7), Biquad(.peak, 140, sr, q: 1.0, db: 2), Biquad(.peak, 600, sr, q: 1.0, db: -1.5),
                Biquad(.peak, 2500, sr, q: 1.4, db: 4), Biquad(.lp, lp, sr, q: 0.54), Biquad(.lp, lp, sr, q: 1.31)]
    },
]

/// Cabinet Size / Length / Tone (the CABINET panel), applied to whatever IR the cab gives, once when it changes
/// (never per sample). Size stretches it in time (bigger cab: longer, lower resonances), Tone tilts it around the mids,
/// Length keeps that share of its tail with a fade; the energy is then put back, so the knobs don't change loudness.
/// size 0.8...1.25, length 0.2...1, tone -6...+6 dB. Windows: shapeIR in Dsp.cpp, the same steps in the same order.
func shapeIR(_ h: [Float], sr: Double, size: Double, length: Double, tone: Double) -> [Float] {
    if h.isEmpty || (abs(size - 1) < 1e-6 && length >= 0.9999 && abs(tone) < 1e-6) { return h }
    var e0 = 0.0
    for x in h { e0 += Double(x) * Double(x) }
    var y = h
    if abs(size - 1) >= 1e-6 {
        let n = max(1, Int((Double(h.count) * size).rounded()))
        y = [Float](repeating: 0, count: n)
        for i in 0..<n {
            let p = Double(i) / size, j = Int(p), f = Float(p - Double(j))
            let a: Float = j < h.count ? h[j] : 0, b: Float = j + 1 < h.count ? h[j + 1] : 0
            y[i] = a + (b - a) * f
        }
    }
    if abs(tone) >= 1e-6 {
        let lo = Biquad(.lowShelf, 500, sr, db: -tone / 2), hi = Biquad(.highShelf, 2000, sr, db: tone / 2)
        for i in 0..<y.count { y[i] = hi.run(lo.run(y[i])) }
    }
    let keep = min(max(64, Int((Double(y.count) * min(length, 1)).rounded())), DSP.maxIR, y.count)
    if keep < y.count {
        y.removeLast(y.count - keep)
        let fade = max(16, keep / 5)
        for k in 0..<fade { y[keep - fade + k] *= Float(0.5 * (1 + cos(Double.pi * Double(k + 1) / Double(fade)))) }
    }
    var e1 = 0.0
    for x in y { e1 += Double(x) * Double(x) }
    if e0 > 0, e1 > 0 { let g = Float((e0 / e1).squareRoot()); for i in 0..<y.count { y[i] *= g } }
    return y
}

/// Energy-normalised IR so switching cabs doesn't jump in volume.
func normaliseIR(_ h: [Float]) -> [Float] {
    var e: Float = 0
    vDSP_svesq(h, 1, &e, vDSP_Length(h.count))
    let g = e > 0 ? 1 / sqrtf(e) : 1
    return h.map { $0 * g }
}

func renderCab(_ model: CabModel, mic: Double, sr: Double, length: Int = 2048) -> [Float] {
    let chain = model.build(mic, sr)
    var h = [Float](repeating: 0, count: length)
    for i in 0..<length {
        var x: Float = i == 0 ? 1 : 0
        for bq in chain { x = bq.run(x) }
        h[i] = x
    }
    // gentle fade so the truncated tail doesn't click
    let f = length / 8
    for i in 0..<f { h[length - 1 - i] *= Float(i) / Float(f) }
    return normaliseIR(h)
}

// MARK: - Pitch detection (McLeod NSDF)

func detectPitch(_ x: [Float], sr: Double) -> Double? {
    let n = x.count
    var ms: Float = 0
    vDSP_measqv(x, 1, &ms, vDSP_Length(n))
    if sqrtf(ms) < 0.003 { return nil }
    let minT = Int(sr / 1100), maxT = min(Int(sr / 50), n / 2)
    var sq = [Float](repeating: 0, count: n + 1)
    for i in 0..<n { sq[i + 1] = sq[i] + x[i] * x[i] }
    var nsdf = [Float](repeating: 0, count: maxT + 2)
    x.withUnsafeBufferPointer { px in
        for t in minT...(maxT + 1) {
            var ac: Float = 0
            vDSP_dotpr(px.baseAddress!, 1, px.baseAddress! + t, 1, &ac, vDSP_Length(n - t))
            let m = sq[n - t] + (sq[n] - sq[t])
            nsdf[t] = m > 0 ? 2 * ac / m : 0
        }
    }
    var t = minT
    while t <= maxT && nsdf[t] > 0 { t += 1 }
    var peaks: [Int] = []
    while t <= maxT {
        if nsdf[t] > 0 && nsdf[t] > nsdf[t - 1] && nsdf[t] >= nsdf[t + 1] { peaks.append(t) }
        t += 1
    }
    guard let top = peaks.map({ nsdf[$0] }).max(), top > 0.6,
          let best = peaks.first(where: { nsdf[$0] >= top * 0.9 }) else { return nil }
    let a = nsdf[best - 1], b = nsdf[best], c = nsdf[best + 1], d = a - 2 * b + c
    let shift = d != 0 ? 0.5 * (a - c) / d : 0
    return sr / (Double(best) + Double(shift))
}
