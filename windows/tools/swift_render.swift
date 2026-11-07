// Swift side of the engine check: the same scenarios as dsp_render.cpp, rendered through mac/Sources/DSP.swift.
// Built by compare_with_swift.sh together with DSP.swift.
import Foundation

func testInput(_ i: Int, _ sr: Double) -> Float {
    let t = Double(i) / sr
    var x = 0.0
    if t < 1.0 {
        for f in [82.41, 123.47, 164.81] { x += sin(2 * Double.pi * f * t) }
        x *= 0.12 * exp(-t * 1.5)
    } else if t < 2.0 {
        let u = t - 1.0
        x = 0.05 * sin(2 * Double.pi * 100 * (pow(40.0, u) - 1) / log(40.0))
    }
    return Float(x)
}

struct Scenario { let name: String; let setup: (Params) -> Void; let ir: ((Double) -> [Float])?; var nam = false }
let builtinCab: (Double) -> [Float] = { sr in renderCab(CABS[0], mic: 0.3, sr: sr) }

let scenarios: [Scenario] = [
    Scenario(name: "hot_full", setup: { p in
        p.inGain = 9.65; p.gateOn = true; p.gateThresh = -48; p.gateRelease = 90
        p.chain[0] = Int32(PedalKind.od.rawValue); p.chainCount = 1; p.odOn = true; p.odDrive = 0.85; p.odTone = 0.7; p.odLevel = 0.5
        p.ampOn = true; p.ampChannel = 2; p.ampGain = 0.65; p.bass = 0.57; p.mid = 0.35; p.treble = 0.7; p.presence = 0.8; p.depth = 0.2; p.ampVolume = 0.6
        p.eqOn = true; for (i, e) in [Float(0), 0, -3.5, -4.9, -4.3, -1.8, 0, 0].enumerated() { p.eq[i] = e }
        p.cabOn = true; p.cabLowCut = 80; p.cabHighCut = 10000
        p.delayOn = true; p.delayTime = 375; p.delayMix = 0.25; p.revOn = true; p.revSize = 0.6; p.revMix = 0.2; p.masterVol = -3
    }, ir: builtinCab),
    Scenario(name: "clean_mod", setup: { p in
        for (i, k) in [0, 1, 2, 4, 5, 6, 7].enumerated() { p.chain[i] = Int32(k) }
        p.chainCount = 7
        p.compOn = true; p.wahOn = true; p.fuzzOn = true; p.chorusOn = true; p.flangerOn = true; p.phaserOn = true; p.tremOn = true
        p.wahAuto = true; p.fuzzAmt = 0.3; p.tremShape = 0.4
        p.ampOn = true; p.ampChannel = 0; p.ampGain = 0.4; p.eqOn = false; p.cabOn = true; p.masterVol = -6
    }, ir: builtinCab),
    Scenario(name: "rust_pitch", setup: { p in
        p.pitchOn = true; p.pitchSemis = -2; p.pitchCents = 10; p.pitchMix = 1
        p.chainCount = 0; p.ampOn = true; p.ampChannel = 1; p.ampGain = 0.7; p.eqOn = false; p.cabOn = true; p.masterVol = -6
    }, ir: builtinCab),
    Scenario(name: "doubler", setup: { p in
        p.chainCount = 0; p.ampOn = true; p.ampChannel = 2; p.ampGain = 0.6; p.eqOn = false; p.cabOn = true; p.masterVol = -6
        p.doublerOn = true; p.doublerWidth = 0.8; p.doublerTime = 24; p.revOn = true; p.revMix = 0.2
    }, ir: builtinCab),
    Scenario(name: "amp_only", setup: { p in
        p.inGain = 9.65; p.gateOn = false; p.chainCount = 0
        p.ampOn = true; p.ampChannel = 2; p.ampGain = 0.65; p.eqOn = false; p.cabOn = false; p.masterVol = -3
    }, ir: nil),
    Scenario(name: "blade_mosfet", setup: { p in
        p.gateOn = false; p.chain[0] = Int32(PedalKind.blade.rawValue); p.chainCount = 1; p.bladeOn = true
        p.bladeDrive = 0.8; p.bladeLow = 0.4; p.bladeMid = 0.7; p.bladeHigh = 0.6; p.bladeLevel = 0.5; p.bladeBoost = 0.6
        p.bladeBoostOn = true; p.bladeShift = false; p.bladeDiode = 0
        p.ampOn = false; p.eqOn = false; p.cabOn = false; p.masterVol = -6
    }, ir: nil),
    Scenario(name: "blade_open", setup: { p in
        p.gateOn = false; p.chain[0] = Int32(PedalKind.blade.rawValue); p.chainCount = 1; p.bladeOn = true
        p.bladeDrive = 0.8; p.bladeLow = 0.4; p.bladeMid = 0.7; p.bladeHigh = 0.6; p.bladeLevel = 0.5; p.bladeBoost = 0.6
        p.bladeBoostOn = false; p.bladeShift = true; p.bladeDiode = 1
        p.ampOn = false; p.eqOn = false; p.cabOn = false; p.masterVol = -6
    }, ir: nil),
    Scenario(name: "blade_silicon", setup: { p in
        p.gateOn = false; p.chain[0] = Int32(PedalKind.blade.rawValue); p.chainCount = 1; p.bladeOn = true
        p.bladeDrive = 0.8; p.bladeLow = 0.4; p.bladeMid = 0.7; p.bladeHigh = 0.6; p.bladeLevel = 0.5; p.bladeBoost = 0.6
        p.bladeBoostOn = true; p.bladeShift = true; p.bladeDiode = 2
        p.ampOn = false; p.eqOn = false; p.cabOn = false; p.masterVol = -6
    }, ir: nil),
    Scenario(name: "od_only", setup: { p in
        p.gateOn = false; p.chain[0] = Int32(PedalKind.od.rawValue); p.chainCount = 1; p.odDrive = 0.85
        p.ampOn = false; p.eqOn = false; p.cabOn = false; p.masterVol = -3
    }, ir: nil),
    Scenario(name: "cab_only", setup: { p in
        p.gateOn = false; p.chainCount = 0; p.ampOn = false; p.eqOn = false; p.cabOn = true; p.masterVol = 0
    }, ir: builtinCab),
    Scenario(name: "gate_only", setup: { p in
        p.inGain = 9.65; p.gateOn = true; p.gateThresh = -48; p.gateRelease = 90
        p.chainCount = 0; p.ampOn = false; p.eqOn = false; p.cabOn = false; p.masterVol = 0
    }, ir: nil),
    Scenario(name: "nam_capture", setup: { p in
        p.inGain = 6; p.gateOn = true; p.gateThresh = -50; p.chain[0] = Int32(PedalKind.od.rawValue); p.chainCount = 1; p.odDrive = 0.3
        p.ampOn = true; p.namOn = true; p.namIn = 3; p.namOut = -2; p.eqOn = true; p.cabOn = true; p.masterVol = -6
    }, ir: builtinCab, nam: true),
    Scenario(name: "metronome", setup: { p in
        p.ampOn = false; p.cabOn = false; p.eqOn = false; p.chainCount = 0; p.gateOn = false
        p.metroOn = true; p.bpm = 133; p.beatsPerBar = 3; p.metroVol = -6
    }, ir: nil),
]

@main struct SwiftRender {
    static func main() {
        let dir = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "."
        let sr = 48000.0, total = Int(sr * 3), block = 128
        for sc in scenarios {
            let p = Params()
            sc.setup(p)
            p.version &+= 1
            let d = DSP(sampleRate: sr, params: p)
            if let ir = sc.ir { d.setIR(ir(sr)) }
            var nm: OpaquePointer?
            if sc.nam {   // NAM's example model, path passed as argv[2]
                nm = lev_nam_load(CommandLine.arguments[2], nil, 0)
                guard let nm else { fatalError("nam load failed") }
                lev_nam_reset(nm, sr, Int32(DSP.maxN))
                d.setNam(nm, gain: 0.5)
            }
            defer { lev_nam_free(nm) }
            var inp = [Float](repeating: 0, count: block)
            var l = [Float](repeating: 0, count: total), r = [Float](repeating: 0, count: total)
            var t = 0
            while t < total {
                for i in 0..<block { inp[i] = testInput(t + i, sr) }
                inp.withUnsafeBufferPointer { ip in l.withUnsafeMutableBufferPointer { lp in r.withUnsafeMutableBufferPointer { rp in
                    d.process(input: ip.baseAddress!, outL: lp.baseAddress! + t, outR: rp.baseAddress! + t, frames: block) } } }
                t += block
            }
            let data = l.withUnsafeBufferPointer { Data(buffer: $0) } + r.withUnsafeBufferPointer { Data(buffer: $0) }
            try! data.write(to: URL(fileURLWithPath: dir).appendingPathComponent("\(sc.name).swift.f32"))
        }
    }
}
