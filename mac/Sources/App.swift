import SwiftUI
import AVFoundation
import AppKit
import UniformTypeIdentifiers

// MARK: - Settings

let CAB_USER = CABS.count, CAB_LIB = CABS.count + 1, CAB_IRLIB = CABS.count + 2

struct Settings: Codable, Equatable {
    var inGain = 0.0, inputChannel = 0
    var gateOn = true, gateThresh = -60.0, gateRelease = 60.0, gateDepth = -60.0
    var pitchOn = false, pitchSemis = 0.0, pitchCents = 0.0, pitchMix = 1.0
    var odOn = true, odDrive = 0.05, odTone = 0.55, odLevel = 0.8
    var ampOn = true, ampChannel = 2, ampGain = 0.55
    var bass = 0.5, mid = 0.55, treble = 0.55, presence = 0.55, depth = 0.55, ampVolume = 0.5
    var eqOn = true, eq: [Double] = [0, 1, 0, 1, 1.5, 1, -1, -3]
    var cabOn = true, cabModel = CAB_LIB, cabMic = 0.3, cabLowCut = 80.0, cabHighCut = 10000.0, cabLevel = 0.0
    var irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav", irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav", irBlend = 0.35
    var irFile = ""   // with cabModel == CAB_IRLIB: path inside the cab IR library
    var delaySync = 0   // index into DELAY_DIVS; 0 = free time in ms
    var delayOn = false, delayTime = 420.0, delayFeedback = 0.28, delayTone = 0.45, delayMix = 0.2
    var revOn = false, revSize = 0.45, revMix = 0.18
    var doublerOn = true, doublerWidth = 0.6, doublerTime = 20.0
    var masterVol = -6.0

    // pedal chain (pre-amp, in order) and the extra pedals
    var chain: [Int] = [PedalKind.od.rawValue]
    var compOn = true, compSustain = 0.5, compAttack = 10.0, compLevel = 0.0, compBlend = 1.0
    var wahOn = true, wahAuto = false, wahPos = 0.5, wahSens = 0.5, wahQ = 0.5
    var fuzzOn = true, fuzzAmt = 0.6, fuzzTone = 0.5, fuzzLevel = 0.5
    var chorusOn = true, chorusRate = 0.8, chorusDepth = 0.5, chorusMix = 0.5
    var flangerOn = true, flangerRate = 0.25, flangerDepth = 0.7, flangerFb = 0.5, flangerMix = 0.5
    var phaserOn = true, phaserRate = 0.5, phaserDepth = 0.7, phaserFb = 0.4
    var tremOn = true, tremRate = 5.0, tremDepth = 0.6, tremShape = 0.0
    var bladeOn = true, bladeDrive = 0.5, bladeLow = 0.5, bladeMid = 0.5, bladeHigh = 0.5, bladeLevel = 0.5
    var bladeBoost = 0.5, bladeBoostOn = false, bladeShift = false, bladeDiode = 0

    // captured amp: name of a file in Application Support/Leviathan/Captures ("" = use the voicings)
    var capture = "", capDrive = 0.0, capLevel = 0.0

    // NAM capture: file name in Application Support/Leviathan/NAM ("" = none), input/output trims in dB
    var nam = "", namInput = 0.0, namOutput = 0.0

    func apply(to p: Params) {
        p.chainCount = 0
        let ch = chain.filter { PedalKind(rawValue: $0) != nil }.prefix(Params.maxChain)
        for (i, k) in ch.enumerated() { p.chain[i] = Int32(k) }
        p.chainCount = ch.count
        p.compOn = compOn; p.compSustain = Float(compSustain); p.compAttack = Float(compAttack); p.compLevel = Float(compLevel); p.compBlend = Float(compBlend)
        p.wahOn = wahOn; p.wahAuto = wahAuto; p.wahPos = Float(wahPos); p.wahSens = Float(wahSens); p.wahQ = Float(wahQ)
        p.fuzzOn = fuzzOn; p.fuzzAmt = Float(fuzzAmt); p.fuzzTone = Float(fuzzTone); p.fuzzLevel = Float(fuzzLevel)
        p.chorusOn = chorusOn; p.chorusRate = Float(chorusRate); p.chorusDepth = Float(chorusDepth); p.chorusMix = Float(chorusMix)
        p.flangerOn = flangerOn; p.flangerRate = Float(flangerRate); p.flangerDepth = Float(flangerDepth); p.flangerFb = Float(flangerFb); p.flangerMix = Float(flangerMix)
        p.phaserOn = phaserOn; p.phaserRate = Float(phaserRate); p.phaserDepth = Float(phaserDepth); p.phaserFb = Float(phaserFb)
        p.tremOn = tremOn; p.tremRate = Float(tremRate); p.tremDepth = Float(tremDepth); p.tremShape = Float(tremShape)
        p.bladeOn = bladeOn; p.bladeDrive = Float(bladeDrive); p.bladeLow = Float(bladeLow); p.bladeMid = Float(bladeMid); p.bladeHigh = Float(bladeHigh)
        p.bladeLevel = Float(bladeLevel); p.bladeBoost = Float(bladeBoost); p.bladeBoostOn = bladeBoostOn; p.bladeShift = bladeShift; p.bladeDiode = bladeDiode
        p.capDrive = Float(capDrive); p.capLevel = Float(capLevel)
        p.namIn = Float(namInput); p.namOut = Float(namOutput)
        p.inGain = Float(inGain); p.inputChannel = inputChannel
        p.gateOn = gateOn; p.gateThresh = Float(gateThresh); p.gateRelease = Float(gateRelease); p.gateDepth = Float(gateDepth)
        p.pitchOn = pitchOn; p.pitchSemis = Float(pitchSemis); p.pitchCents = Float(pitchCents); p.pitchMix = Float(pitchMix)
        p.odOn = odOn; p.odDrive = Float(odDrive); p.odTone = Float(odTone); p.odLevel = Float(odLevel)
        p.ampOn = ampOn; p.ampChannel = ampChannel; p.ampGain = Float(ampGain)
        p.bass = Float(bass); p.mid = Float(mid); p.treble = Float(treble); p.presence = Float(presence); p.depth = Float(depth); p.ampVolume = Float(ampVolume)
        p.eqOn = eqOn; for i in 0..<8 { p.eq[i] = Float(i < eq.count ? eq[i] : 0) }
        p.cabOn = cabOn; p.cabLowCut = Float(cabLowCut); p.cabHighCut = Float(cabHighCut); p.cabLevel = Float(cabLevel)
        p.delayOn = delayOn; p.delayTime = Float(delayTime); p.delayFeedback = Float(delayFeedback); p.delayTone = Float(delayTone); p.delayMix = Float(delayMix)
        p.revOn = revOn; p.revSize = Float(revSize); p.revMix = Float(revMix)
        p.doublerOn = doublerOn; p.doublerWidth = Float(doublerWidth); p.doublerTime = Float(doublerTime)
        p.masterVol = Float(masterVol)
        p.version &+= 1
    }

    /// Decode, filling any keys missing from older saves with defaults.
    static func lenient(_ data: Data) -> Settings? {
        guard let base = try? JSONSerialization.jsonObject(with: JSONEncoder().encode(Settings())) as? [String: Any],
              let over = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let merged = try? JSONSerialization.data(withJSONObject: base.merging(over) { $1 }) else { return nil }
        return try? JSONDecoder().decode(Settings.self, from: merged)
    }
}

let FACTORY: [(String, (inout Settings) -> Void)] = [
    ("Tight Rhythm (Gojira-ish)", { s in
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.86; s.presence = 0.72; s.treble = 0.54; s.mid = 0.24; s.eqOn = true
        s.eq = [-0.5, 1, -0.5, -0.5, 1.5, 1.5, -2.5, 0.5]
        s.ampVolume = 0.483   // level-matched
    }),
    ("Sludge Crunch (Mastodon-ish)", { s in
        s.odOn = false; s.ampChannel = 1; s.ampGain = 0.8; s.bass = 0.65; s.mid = 0.6; s.treble = 0.5; s.presence = 0.45; s.depth = 0.65
        s.ampVolume = 0.29   // level-matched (was clipping)
        s.eq = [1, 1, 0, 0, 0, 0, -1, -3]; s.irA = "SM57/NO-TS/57_2_inch_cone_far_pres_2.wav"; s.irBlend = 0.5
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.26; s.presence = 0.75; s.treble = 0.9; s.mid = 0.56; s.eqOn = true
        s.eq = [6, 0, -3, -3, 2, 4.5, -2.5, -6]
        s.ampVolume = 0.418   // level-matched
    }),
    ("Screaming Lead", { s in
        s.gateThresh = -64; s.odDrive = 0.2; s.ampGain = 0.7; s.mid = 0.65; s.treble = 0.5; s.ampVolume = 0.44
        s.eq = [-2, -1, 0, 1, 2, 1, -1, -4]; s.irA = "SM57/TS/57_TS_1_inch_edge_pres_3.wav"; s.delayOn = true; s.delayTime = 450; s.delayMix = 0.22; s.revOn = true; s.revMix = 0.15
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.93; s.presence = 0.9; s.treble = 0.53; s.mid = 0.1; s.eqOn = true
        s.eq = [-6, -0.5, -1.5, -0.5, 2, 2, -2.5, 3.5]
        s.ampVolume = 0.491   // level-matched
    }),
    // Tight modern rhythm: tight gate -> boosted TS-style overdrive -> hot channel with scooped mids
    // (EQ cut from 320 Hz to 2.5 kHz) -> 4x12 with a 57 on the cap.
    ("Modern Metal Rhythm", { s in
        s.gateOn = true; s.gateThresh = -48; s.gateRelease = 90
        s.chain = [PedalKind.od.rawValue]; s.odOn = true; s.odDrive = 0.85; s.odTone = 0.7; s.odLevel = 0.5
        s.ampOn = true; s.ampChannel = 2; s.ampGain = 0.65
        s.bass = 0.57; s.mid = 0.35; s.treble = 0.7; s.presence = 0.8; s.depth = 0.2; s.ampVolume = 0.6
        s.eqOn = true; s.eq = [0, 0, -3.5, -4.9, -4.3, -1.8, 0, 0]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM57/NO-TS/57_1_inch_cap_pres_4.wav"; s.irB = ""; s.irBlend = 0
        s.cabLowCut = 80; s.cabHighCut = 10000
        s.delayOn = false; s.revOn = false
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.78; s.presence = 0.12; s.treble = 0.49; s.mid = 0.46; s.eqOn = true
        s.eq = [2, 0, -3, -4.5, -4, -3, 0.5, -2.5]
        s.ampVolume = 0.686   // level-matched
    }),
    ("Ambient Clean", { s in
        s.gateThresh = -72; s.odOn = false; s.ampChannel = 0; s.ampGain = 0.4; s.bass = 0.5; s.mid = 0.5; s.treble = 0.6; s.ampVolume = 0.44
        s.eq = [0, 0, 0, 0, 0, 1, 1, 0]; s.cabModel = 2; s.cabHighCut = 10000
        s.delayOn = true; s.delayTime = 520; s.delayFeedback = 0.42; s.delayMix = 0.3; s.revOn = true; s.revSize = 0.8; s.revMix = 0.35
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.1; s.presence = 0.9; s.treble = 0.47; s.mid = 0.36; s.eqOn = true
        s.eq = [-6, -6, -6, -6, 1.5, -4.5, 3, 6]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "C414/NO-TS/C414_1_inch_cone_near_pres_3.wav"; s.irB = "SM57/NO-TS/57_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.4; s.cabHighCut = 12000
        s.ampVolume = 0.551   // level-matched
    }),
    // Band-inspired tones: the well-known rigs behind each sound (amp type, boost, EQ shape, mics), level-matched.
    // Mark IIC+ style: no boost, deep V on the graphic EQ, 57 on the cap with a far 421.
    ("Thrash Rhythm (Metallica-ish)", { s in
        s.gateThresh = -50.0; s.gateRelease = 80.0; s.chain = []
        s.ampChannel = 2; s.ampGain = 0.7
        s.bass = 0.65; s.mid = 0.3; s.treble = 0.65; s.presence = 0.6; s.depth = 0.5; s.ampVolume = 0.47
        s.eq = [2, 2, 0, -3, -5, -2, 1, 0]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM57/NO-TS/57_1_inch_cap_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_far_pres_3.wav"; s.irBlend = 0.3
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.24; s.presence = 0.46; s.treble = 0.9; s.mid = 0.63; s.eqOn = true
        s.eq = [6, -1.5, -4.5, -5, -2.5, 0, -1, -5]
        s.ampVolume = 0.6   // level-matched
    }),
    // Hot-rodded Marshall with a clean boost: mids stay in, bright and biting.
    ("Thrash Attack (Slayer-ish)", { s in
        s.gateThresh = -55.0; s.gateRelease = 70.0
        s.chain = [PedalKind.od.rawValue]; s.odOn = true; s.odDrive = 0.1; s.odTone = 0.6; s.odLevel = 0.85
        s.ampChannel = 2; s.ampGain = 0.6
        s.bass = 0.5; s.mid = 0.6; s.treble = 0.7; s.presence = 0.7; s.depth = 0.4; s.ampVolume = 0.5
        s.eq = [0, 0, 0, 1, 2, 2, 0, -2]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_4.wav"; s.irB = ""; s.irBlend = 0.0
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.95; s.presence = 0.57; s.treble = 0.6; s.mid = 0.27; s.eqOn = true
        s.eq = [-2.5, 1, 0.5, 0, 1.5, 2.5, 0, -0.5]
        s.ampVolume = 0.494   // level-matched
    }),
    // Solid-state crunch scooped hard, big low end, very tight gate.
    ("Groove Scoop (Pantera-ish)", { s in
        s.gateThresh = -48.0; s.gateRelease = 50.0; s.chain = []
        s.ampChannel = 2; s.ampGain = 0.75
        s.bass = 0.7; s.mid = 0.15; s.treble = 0.75; s.presence = 0.7; s.depth = 0.6; s.ampVolume = 0.59
        s.eq = [3, 2, -2, -6, -6, -2, 2, 1]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM57/NO-TS/57_1_inch_cap_pres_5.wav"; s.irB = "SM7B/NO-TS/SM7B_1_inch_cap_pres_4.wav"; s.irBlend = 0.2
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.38; s.presence = 0.1; s.treble = 0.87; s.mid = 0.87; s.eqOn = true
        s.eq = [6, -1, -3, -3.5, -3, -2.5, 0.5, -6]
        s.ampVolume = 0.726   // level-matched
    }),
    // Modded JCM800 pushed by a drive pedal: tighter and more mid-forward than the Mark-style scoop.
    ("Speed Metal (Megadeth-ish)", { s in
        s.gateThresh = -55.0; s.gateRelease = 70.0
        s.chain = [PedalKind.od.rawValue]; s.odOn = true; s.odDrive = 0.3; s.odTone = 0.65; s.odLevel = 0.6
        s.ampChannel = 2; s.ampGain = 0.6
        s.bass = 0.5; s.mid = 0.5; s.treble = 0.7; s.presence = 0.65; s.depth = 0.45; s.ampVolume = 0.51
        s.eq = [0, 0, -1, 0, 1, 2, 0, -2]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_4.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.25
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.89; s.presence = 0.55; s.treble = 0.53; s.mid = 0.28; s.eqOn = true
        s.eq = [-5, 1, 0, -0.5, 1, 2, 0, 1.5]
        s.ampVolume = 0.496   // level-matched
    }),
    // Treble booster into a cranked British head: thick mids, loose gate, a little room.
    ("Doom Riff (Sabbath-ish)", { s in
        s.gateThresh = -66.0; s.gateRelease = 150.0
        s.chain = [PedalKind.od.rawValue]; s.odOn = true; s.odDrive = 0.35; s.odTone = 0.75; s.odLevel = 0.7
        s.ampChannel = 1; s.ampGain = 0.8
        s.bass = 0.6; s.mid = 0.65; s.treble = 0.5; s.presence = 0.45; s.depth = 0.65; s.ampVolume = 0.35
        s.eq = [1, 1, 1, 1, 0, 0, -1, -3]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "MD421/NO-TS/MD421_1_inch_cone_near_pres_2.wav"; s.irB = "C414/NO-TS/C414_2_foot_pres_2.wav"; s.irBlend = 0.3
        s.revOn = true; s.revSize = 0.4; s.revMix = 0.1
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.81; s.presence = 0.52; s.treble = 0.9; s.mid = 0.39; s.eqOn = true
        s.eq = [2.5, 1, -1, -4.5, 4.5, 4, -5.5, -0.5]
        s.ampVolume = 0.397   // level-matched
    }),
    // Marshall crunch, mid-forward for twin harmonies, with lead delay and reverb.
    ("Twin Lead (Maiden-ish)", { s in
        s.gateThresh = -66.0; s.gateRelease = 120.0; s.chain = []
        s.ampChannel = 1; s.ampGain = 0.55
        s.bass = 0.45; s.mid = 0.7; s.treble = 0.6; s.presence = 0.55; s.depth = 0.4; s.ampVolume = 0.43
        s.eq = [-1, 0, 1, 2, 2, 1, 0, -2]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.delayOn = true; s.delayTime = 380.0; s.delayFeedback = 0.25; s.delayMix = 0.15
        s.revOn = true; s.revSize = 0.5; s.revMix = 0.12
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.42; s.presence = 0.73; s.treble = 0.85; s.mid = 0.64; s.eqOn = true
        s.eq = [5.5, -1, -2, 0.5, 2.5, 2, -2.5, -2]
        s.ampVolume = 0.47   // level-matched
    }),
    // Clean boost into high gain for an ultra-tight low end, hard gate, clanky upper mids.
    ("Djent Clank (Meshuggah-ish)", { s in
        s.gateThresh = -45.0; s.gateRelease = 35.0
        s.chain = [PedalKind.od.rawValue]; s.odOn = true; s.odDrive = 0.0; s.odTone = 0.6; s.odLevel = 0.9
        s.ampChannel = 2; s.ampGain = 0.6
        s.bass = 0.45; s.mid = 0.45; s.treble = 0.65; s.presence = 0.75; s.depth = 0.7; s.ampVolume = 0.5
        s.eq = [-2, -1, -3, -1, 1, 3, 1, -2]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM57/NO-TS/57_1_inch_cap_pres_4.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.3
        s.cabLowCut = 110.0; s.cabHighCut = 8000.0
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.95; s.presence = 0.52; s.treble = 0.42; s.mid = 0.31; s.eqOn = true
        s.eq = [-2, -1, -2.5, -1.5, -1, 1.5, 0, 2.5]
        s.ampVolume = 0.572   // level-matched
    }),
    // HM-2 style "all knobs up": fuzz into a crunchy amp with the lows and upper mids pushed hard.
    ("Chainsaw Death (Entombed-ish)", { s in
        s.gateThresh = -55.0; s.gateRelease = 80.0
        s.chain = [PedalKind.fuzz.rawValue]; s.fuzzOn = true; s.fuzzAmt = 0.7; s.fuzzTone = 0.7; s.fuzzLevel = 0.55
        s.odOn = false
        s.ampChannel = 1; s.ampGain = 0.4
        s.bass = 0.55; s.mid = 0.6; s.treble = 0.6; s.presence = 0.55; s.depth = 0.5; s.ampVolume = 0.35
        s.eq = [5, 3, 0, 0, 6, 6, 3, -2]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = ""; s.irBlend = 0.0
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.6; s.presence = 0.9; s.treble = 0.1; s.mid = 0.51; s.eqOn = true
        s.eq = [6, -0.5, -3, 3.5, -0.5, 2.5, 2, 6]
        s.ampVolume = 0.47   // level-matched
    }),
    // Compressed and wide: thick low end, a drive pedal for density, gated to silence between hits.
    ("Industrial Wall (Rammstein-ish)", { s in
        s.gateThresh = -48.0; s.gateRelease = 45.0
        s.chain = [PedalKind.comp.rawValue, PedalKind.od.rawValue]
        s.compOn = true; s.compSustain = 0.4; s.compAttack = 5.0; s.compLevel = 0.0; s.compBlend = 0.7
        s.odOn = true; s.odDrive = 0.2; s.odTone = 0.5; s.odLevel = 0.6
        s.ampChannel = 2; s.ampGain = 0.7
        s.bass = 0.65; s.mid = 0.4; s.treble = 0.55; s.presence = 0.5; s.depth = 0.75; s.ampVolume = 0.49
        s.eq = [2, 2, -1, -2, 0, 1, 0, -3]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "SM7B/NO-TS/SM7B_1_inch_cap_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.ampVolume = 0.511   // level-matched
    }),
    // Clean channel with light compression, chorus, delay and a big room: the quiet parts of prog metal.
    ("Prog Clean (Opeth-ish)", { s in
        s.gateThresh = -72.0; s.gateRelease = 120.0
        s.chain = [PedalKind.comp.rawValue, PedalKind.chorus.rawValue]
        s.compOn = true; s.compSustain = 0.35; s.compAttack = 12.0; s.compBlend = 0.6
        s.chorusOn = true; s.chorusRate = 0.6; s.chorusDepth = 0.4; s.chorusMix = 0.35
        s.odOn = false
        s.ampChannel = 0; s.ampGain = 0.35
        s.bass = 0.5; s.mid = 0.5; s.treble = 0.6; s.presence = 0.5; s.depth = 0.5; s.ampVolume = 0.33
        s.eq = [0, 0, 0, 0, 0, 1, 1, 0]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "U87/NO-TS/U87_grill_cone_far_pres_3.wav"; s.irB = ""; s.irBlend = 0.0
        s.delayOn = true; s.delayTime = 450.0; s.delayFeedback = 0.3; s.delayMix = 0.2
        s.revOn = true; s.revSize = 0.6; s.revMix = 0.25
        // tone fitted to the Rammstein-ish balance (real lows, no 2-5 kHz honk), measured on a DI take
        s.depth = 0.1; s.presence = 0.9; s.treble = 0.39; s.mid = 0.28; s.eqOn = true
        s.eq = [-6, -6, -6, -6, 2, -4.5, 4, 6]
        s.cabOn = true; s.cabModel = CAB_LIB; s.irA = "C414/NO-TS/C414_2_inch_cone_near_pres_3.wav"; s.irB = "SM57/NO-TS/57_2_inch_edge_pres_3.wav"; s.irBlend = 0.35; s.cabHighCut = 12000
        s.ampVolume = 0.698   // level-matched
    }),

    // NAM captures of real amps (bundled; see shared/nam/captures/CREDITS.md). Without them the built-in amp plays.
    // Peavey 5150 with a boost: the modern metal rhythm sound.
    ("Modern Metal - 5150 (NAM)", { s in
        s.nam = "Helga B 5150 BlockLetter - Boosted"; s.capture = ""; s.ampOn = true; s.ampChannel = 2
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 90; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -52
        s.gateRelease = 70; s.chain = []; s.delayOn = false; s.revOn = false
        s.namInput = 0; s.namOutput = 2.0
    }),
    // Unboosted 5150 hit by a clean boost, very tight lows and gate.
    ("Djent - 5150 + Boost (NAM)", { s in
        s.nam = "Helga B 5150 BlockLetter - NoBoost"; s.capture = ""; s.ampOn = true; s.ampChannel = 2
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 110; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -45
        s.gateRelease = 35; s.chain = [PedalKind.od.rawValue]; s.delayOn = false; s.revOn = false
        s.namInput = 0; s.namOutput = 4.6; s.odOn = true; s.odDrive = 0
        s.odTone = 0.6; s.odLevel = 0.9
    }),
    // 6505+ red channel with an MXR drive in front.
    ("Groove Chug - 6505+ (NAM)", { s in
        s.nam = "Helga B 6505+ Red ch - MXR Drive"; s.capture = ""; s.ampOn = true; s.ampChannel = 2
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 90; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -48
        s.gateRelease = 60; s.chain = []; s.delayOn = false; s.revOn = false
        s.namInput = 0; s.namOutput = 1.3
    }),
    // Mesa Mark IV lead channel for tight thrash rhythm.
    ("Thrash Rhythm - Mark IV (NAM)", { s in
        s.nam = "Roman A LT_MESA_MARKIV_1"; s.capture = ""; s.ampOn = true; s.ampChannel = 2
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 90; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -50
        s.gateRelease = 80; s.chain = []; s.delayOn = false; s.revOn = false
        s.namInput = 0; s.namOutput = 1.1
    }),
    // Hot-rodded Marshall (Splawn Pro Mod, 3rd gear) with a clean boost.
    ("Thrash Attack - Splawn (NAM)", { s in
        s.nam = "Tim R Splawn Pro Mod 3rd Gear - G12"; s.capture = ""; s.ampOn = true; s.ampChannel = 2
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 90; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -55
        s.gateRelease = 70; s.chain = [PedalKind.od.rawValue]; s.delayOn = false; s.revOn = false
        s.namInput = 0; s.namOutput = 3.1; s.odOn = true; s.odDrive = 0.05
        s.odTone = 0.6; s.odLevel = 0.8
    }),
    // Marshall JVM crunch: classic rock and twin-guitar metal rhythm.
    ("Crunch - JVM (NAM)", { s in
        s.nam = "Phillipe P JVM-CR-OR-NoDrive-ESR0,017"; s.capture = ""; s.ampOn = true; s.ampChannel = 1
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 80; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -66
        s.gateRelease = 120; s.chain = []; s.delayOn = false; s.revOn = false
        s.namInput = 0; s.namOutput = -1.6
    }),
    // Marshall JVM OD2 with lead delay and reverb.
    ("British Lead - JVM (NAM)", { s in
        s.nam = "Phillipe P JVM-OD2-RD-NoDrive-ESR0,003"; s.capture = ""; s.ampOn = true; s.ampChannel = 2
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 90; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -64
        s.gateRelease = 150; s.chain = []; s.delayOn = true; s.revOn = true
        s.namInput = 0; s.namOutput = 2.7; s.delayTime = 380; s.delayFeedback = 0.25
        s.delayMix = 0.15; s.revSize = 0.5; s.revMix = 0.12
    }),
    // Laney GH100S: thick British mids for doom and stoner riffs.
    ("Doom - Laney (NAM)", { s in
        s.nam = "Phillipe P LaneyGH100S-Hi-MidGain-ESR0,003"; s.capture = ""; s.ampOn = true; s.ampChannel = 1
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 80; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -66
        s.gateRelease = 150; s.chain = []; s.delayOn = false; s.revOn = true
        s.namInput = 0; s.namOutput = -0.9; s.revSize = 0.4; s.revMix = 0.1
    }),
    // ENGL with a DS-1: compressed, wide and gated hard.
    ("Industrial - ENGL (NAM)", { s in
        s.nam = "Tom C Engl e530 preamp & 840 power amp with Boss DS1 Overdrive"; s.capture = ""; s.ampOn = true; s.ampChannel = 2
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 90; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -48
        s.gateRelease = 45; s.chain = [PedalKind.comp.rawValue]; s.delayOn = false; s.revOn = false
        s.namInput = 0; s.namOutput = 4.1; s.compOn = true; s.compSustain = 0.4
        s.compAttack = 5; s.compLevel = 0; s.compBlend = 0.7
    }),
    // Soldano SLO-style drive with delay and reverb for leads.
    ("Hot Lead - Soldano (NAM)", { s in
        s.nam = "Jason Z Soldano - Super Lead Overdrive pedal nice crunchy rhythm tone"; s.capture = ""; s.ampOn = true; s.ampChannel = 2
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "SM57/NO-TS/57_1_inch_edge_pres_3.wav"; s.irB = "MD421/NO-TS/MD421_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.35
        s.cabLowCut = 90; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -64
        s.gateRelease = 150; s.chain = []; s.delayOn = true; s.revOn = true
        s.namInput = 0; s.namOutput = 1.3; s.delayTime = 450; s.delayFeedback = 0.3
        s.delayMix = 0.2; s.revSize = 0.55; s.revMix = 0.15
    }),
    // Fender Twin Reverb with light compression, chorus, delay and reverb.
    ("Clean - Twin (NAM)", { s in
        s.nam = "Tim R Fender TwinVerb Norm Bright"; s.capture = ""; s.ampOn = true; s.ampChannel = 0
        s.ampGain = 0.6; s.eqOn = true; s.eq = [0, 0, 0, 0, 0, 0, 0, 0]; s.cabOn = true
        s.cabModel = 4; s.irA = "C414/NO-TS/C414_1_inch_cone_near_pres_3.wav"; s.irB = "SM57/NO-TS/57_1_inch_cone_near_pres_3.wav"; s.irBlend = 0.4
        s.cabLowCut = 80; s.cabHighCut = 10000; s.gateOn = true; s.gateThresh = -72
        s.gateRelease = 120; s.chain = [PedalKind.comp.rawValue, PedalKind.chorus.rawValue]; s.delayOn = true; s.revOn = true
        s.namInput = 0; s.namOutput = -10.7; s.compOn = true; s.compSustain = 0.35
        s.compAttack = 12; s.compBlend = 0.6; s.chorusOn = true; s.chorusRate = 0.6
        s.chorusDepth = 0.4; s.chorusMix = 0.35; s.delayTime = 450; s.delayFeedback = 0.3
        s.delayMix = 0.18; s.revSize = 0.6; s.revMix = 0.22
    }),
]

// MARK: - Captures

/// A fitted amp model written by mac/Capture/fit.py.
struct Capture: Codable {
    let name: String
    let sr: Double
    let hp, ls, pk_f, pk, hs, k1, b1, mid, k2, b2: Double
    let ir: [Float]
    let esr_val: Double

    static let dir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("Leviathan/Captures")

    static func loadAll() -> [String: Capture] {
        let files = (try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil)) ?? []
        var out: [String: Capture] = [:]
        for u in files where u.pathExtension == "json" {
            if let d = try? Data(contentsOf: u), let c = try? JSONDecoder().decode(Capture.self, from: d) {
                out[u.deletingPathExtension().lastPathComponent] = c
            }
        }
        return out
    }

    func apply(to p: Params) {
        p.capHP = Float(hp); p.capLS = Float(ls); p.capPkF = Float(pk_f); p.capPk = Float(pk); p.capHS = Float(hs)
        p.capK1 = Float(k1); p.capB1 = Float(b1); p.capMid = Float(mid); p.capK2 = Float(k2); p.capB2 = Float(b2)
    }
}

// MARK: - Tunings

struct Tuning: Hashable {
    let name: String
    let strings: [Int]   // MIDI notes, low to high
    let flats: Bool
}

let TUNINGS: [Tuning] = [
    Tuning(name: "E Standard", strings: [40, 45, 50, 55, 59, 64], flats: false),
    Tuning(name: "Eb Standard", strings: [39, 44, 49, 54, 58, 63], flats: true),
    Tuning(name: "D Standard", strings: [38, 43, 48, 53, 57, 62], flats: false),
    Tuning(name: "Drop D", strings: [38, 45, 50, 55, 59, 64], flats: false),
    Tuning(name: "Drop Db", strings: [37, 44, 49, 54, 58, 63], flats: true),
    Tuning(name: "Drop C", strings: [36, 43, 48, 53, 57, 62], flats: false),
    Tuning(name: "C Standard", strings: [36, 41, 46, 51, 55, 60], flats: true),
    Tuning(name: "Drop B", strings: [35, 42, 47, 52, 56, 61], flats: false),
    Tuning(name: "Drop A", strings: [33, 40, 45, 50, 54, 59], flats: false),
]

/// Name any six-string tuning: "E Standard", "Drop C", or the notes if it's neither.
func tuningName(_ strings: [Int]) -> String {
    let names = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
    let n = { (m: Int) in names[((m % 12) + 12) % 12] }
    let iv = zip(strings.dropFirst(), strings).map { $0 - $1 }
    if iv == [5, 5, 5, 4, 5] { return "\(n(strings[0])) Standard" }
    if iv == [7, 5, 5, 4, 5] { return "Drop \(n(strings[0]))" }
    return strings.map(n).joined(separator: " ")
}

/// The open-string notes, low to high: "D A D G B E".
func tuningNotes(_ strings: [Int], flats: Bool) -> String { strings.map { noteName($0, flats: flats) }.joined(separator: " ") }

extension Tuning {
    /// Name with its notes, for pickers: "Drop D · D A D G B E".
    var label: String { name + " \u{00B7} " + tuningNotes(strings, flats: flats) }
}

func noteName(_ midi: Int, flats: Bool) -> String {
    let sharp = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    let flat = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
    return (flats ? flat : sharp)[((midi % 12) + 12) % 12]
}

// MARK: - Model

@MainActor
final class Model: ObservableObject {
    let params = Params()
    lazy var engine = HALEngine(params: params)

    @Published var s: Settings { didSet { if s != oldValue { settingsChanged(oldValue) } } }
    @Published var inputs: [AudioDevice] = []
    @Published var outputs: [AudioDevice] = []
    @Published var inputUID = UserDefaults.standard.string(forKey: "inputUID") ?? "" { didSet { saveDevice(); restartIfRunning() } }
    @Published var outputUID = UserDefaults.standard.string(forKey: "outputUID") ?? "" { didSet { saveDevice(); restartIfRunning() } }
    @Published var bufferSize = UserDefaults.standard.object(forKey: "buffer") as? Int ?? 32 {
        didSet { UserDefaults.standard.set(bufferSize, forKey: "buffer"); restartIfRunning() }
    }
    @Published var running = false
    @Published var status = ""
    @Published var info: HALEngine.Info?
    let meters = Meters()   // fast-changing values live apart so they don't redraw the whole rig
    @Published var savingPreset = false
    @Published var newPresetName = ""

    @Published var tunerOn = false { didSet { syncMute() } }
    @Published var tunerMute = true { didSet { syncMute() } }
    @Published var tuningIndex = UserDefaults.standard.object(forKey: "tuning") as? Int ?? 1 {
        didSet { UserDefaults.standard.set(tuningIndex, forKey: "tuning") }
    }
    @Published var a4 = UserDefaults.standard.object(forKey: "a4") as? Double ?? 440 { didSet { UserDefaults.standard.set(a4, forKey: "a4") } }
    @Published var tunerNote = "–"
    @Published var tunerCents = 0.0
    @Published var tunerFreq = 0.0
    @Published var tunerString: Int? = nil

    @Published var userPresets: [String: Settings] = [:]
    private var unreadablePresets: [String: Any] = [:]
    /// When each preset was created, last saved and last loaded (for sorting the Presets screen).
    @Published var presetMeta: [String: PresetMeta] = (UserDefaults.standard.data(forKey: "presetMeta"))
        .flatMap { try? JSONDecoder().decode([String: PresetMeta].self, from: $0) } ?? [:] {
        didSet { UserDefaults.standard.set(try? JSONEncoder().encode(presetMeta), forKey: "presetMeta") }
    }
    @Published var presetSort = PresetSort(rawValue: UserDefaults.standard.string(forKey: "presetSort") ?? "") ?? .list {
        didSet { UserDefaults.standard.set(presetSort.rawValue, forKey: "presetSort") }
    }
    /// false: list order A→Z / newest first; true: reversed.
    @Published var presetSortReversed = UserDefaults.standard.bool(forKey: "presetSortReversed") {
        didSet { UserDefaults.standard.set(presetSortReversed, forKey: "presetSortReversed") }
    }
    /// Click a column header: sort by it, or reverse if it's already the sort.
    func sortPresets(by sort: PresetSort) {
        if presetSort == sort { presetSortReversed.toggle() } else { presetSort = sort; presetSortReversed = false }
    }
    /// All presets in the chosen order (the list order is factory first, then yours A-Z; it's what ‹ › and MIDI use).
    func sortedPresets(_ sort: PresetSort, reversed: Bool = false) -> [String] {
        let list = presetOrder
        guard sort != .list else { return reversed ? list.reversed() : list }
        if sort == .name {
            let az = list.sorted { $0.localizedStandardCompare($1) == .orderedAscending }
            return reversed ? az.reversed() : az
        }
        func date(_ n: String) -> Date? {
            let m = presetMeta[n]
            switch sort { case .played: return m?.played; case .modified: return m?.shownModified; case .created: return m?.created ?? m?.imported; case .list, .name: return nil }
        }
        let dated = list.filter { date($0) != nil }.sorted { reversed ? date($0)! < date($1)! : date($0)! > date($1)! }
        return dated + list.filter { date($0) == nil }   // never saved / played: always after, in list order
    }
    var recentlyPlayed: [String] {
        presetOrder.filter { presetMeta[$0]?.played != nil }.sorted { presetMeta[$0]!.played! > presetMeta[$1]!.played! }
    }
    @Published var presetName = UserDefaults.standard.string(forKey: "preset") ?? FACTORY[0].0
    @Published var userIRName: String?
    @Published var captures: [String: Capture] = Capture.loadAll()
    let namHost = NamHost()
    let cabIRs: CabIRLibrary = { let l = CabIRLibrary(); l.scan(); return l }()

    func importCabIRs() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = true; panel.allowsMultipleSelection = true
        panel.allowedContentTypes = [.wav, .aiff, .audio, .folder]
        panel.prompt = "Add"
        panel.message = "Choose cabinet IRs (WAV) or folders of them, for example a tone pack from tone3000.com"
        guard panel.runModal() == .OK else { return }
        let n = cabIRs.importItems(panel.urls)
        status = n == 0 ? "No WAV impulse responses found there." : "Added \(n) cab IR\(n == 1 ? "" : "s") to the IR library."
        objectWillChange.send()
        if n > 0, s.cabModel != CAB_IRLIB || !cabIRs.files.contains(s.irFile), let first = cabIRs.files.first {
            var x = s; x.cabModel = CAB_IRLIB; x.irFile = first; s = x
        }
    }
    private var userIR: (samples: [Float], rate: Double)?
    let irLib = IRLibrary()
    lazy var sessions = Sessions(model: self)
    lazy var midi = Midi(model: self)

    // Rig layout (not part of presets): panel order and collapsed sections.
    /// "|" starts a new row.
    static let defaultPanels = ["input", "master", "gate", "pitch", "eq", "doubler", "|", "amp", "cab", "reverb", "delay"]
    /// Each panel's UnitBox title (also its collapse key).
    static let panelTitles = ["input": "INPUT", "gate": "GATE", "pitch": "PITCH", "amp": "AMP", "master": "MASTER",
                              "cab": "CABINET", "eq": "GRAPHIC EQ", "delay": "DELAY", "reverb": "REVERB", "doubler": "DOUBLER"]
    @Published var panelOrder: [String] = {
        var order = (UserDefaults.standard.stringArray(forKey: "panelOrder2") ?? []).filter(Model.defaultPanels.contains)
        // a panel added in a new version goes in after its neighbour in the default order (Doubler after the EQ), not at the end
        for (k, id) in Model.defaultPanels.enumerated() where !order.contains(id) {
            let after = Model.defaultPanels[..<k].last(where: order.contains).flatMap { order.firstIndex(of: $0) }
            order.insert(id, at: after.map { $0 + 1 } ?? order.count)
        }
        return order
    }() { didSet { UserDefaults.standard.set(panelOrder, forKey: "panelOrder2") } }
    /// Show an explanation of the control under the pointer, in a strip at the bottom of the rig.
    @Published var explainOnHover = UserDefaults.standard.object(forKey: "explainOnHover") as? Bool ?? true {
        didSet { UserDefaults.standard.set(explainOnHover, forKey: "explainOnHover"); if !explainOnHover { HoverHelp.shared.clear() } }
    }
    @Published var collapsed = Set(UserDefaults.standard.stringArray(forKey: "collapsed") ?? []) {
        didSet { UserDefaults.standard.set(Array(collapsed), forKey: "collapsed") }
    }
    func toggleCollapsed(_ key: String) { if collapsed.contains(key) { collapsed.remove(key) } else { collapsed.insert(key) } }
    let updater = Updater()

    // Tempo: shared by the metronome and synced delay; set by typing, tap tempo or MIDI clock. Not part of presets.
    @Published var bpm = UserDefaults.standard.object(forKey: "bpm") as? Double ?? 120 {
        didSet { if bpm != oldValue { UserDefaults.standard.set(bpm, forKey: "bpm"); applyAll() } }
    }
    @Published var metroOn = false { didSet { params.metroOn = metroOn } }
    @Published var beatsPerBar = UserDefaults.standard.object(forKey: "beatsPerBar") as? Int ?? 4 {
        didSet { UserDefaults.standard.set(beatsPerBar, forKey: "beatsPerBar"); applyAll() }
    }
    @Published var metroVol = UserDefaults.standard.object(forKey: "metroVol") as? Double ?? -12 {
        didSet { UserDefaults.standard.set(metroVol, forKey: "metroVol"); applyAll() }
    }
    private var taps: [Date] = []
    @Published var screen: Screen = {   // LEVIATHAN_SCREEN=settings|presets|recordings opens there (UI testing)
        switch ProcessInfo.processInfo.environment["LEVIATHAN_SCREEN"] {
        case "settings": return .settings
        case "presets": return .presets
        case "recordings": return .recordings
        default: return .rig
        }
    }()
    /// Accent colour: a picked colour, or (when none is set) the colour of the active amp channel.
    @Published var accentRGB: [Double]? = UserDefaults.standard.array(forKey: "accent") as? [Double] {
        didSet { UserDefaults.standard.set(accentRGB, forKey: "accent") }
    }
    var accent: Color {
        if let c = accentRGB, c.count == 3 { return Color(red: c[0], green: c[1], blue: c[2]) }
        return channelColors[min(max(s.ampChannel, 0), channelColors.count - 1)]
    }
    var accentBinding: Binding<Color> {
        Binding(get: { self.accent }, set: { c in
            let n = NSColor(c).usingColorSpace(.sRGB) ?? .red
            self.accentRGB = [Double(n.redComponent), Double(n.greenComponent), Double(n.blueComponent)]
        })
    }
    @Published var zoom = Double(Z) { didSet { Z = CGFloat(zoom); UserDefaults.standard.set(zoom, forKey: "zoom") } }
    static let zoomSteps = [0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.25, 1.4, 1.6]
    func zoomStep(_ d: Int) {
        let i = Model.zoomSteps.firstIndex { $0 >= zoom - 0.001 } ?? 4
        zoom = Model.zoomSteps[min(max(i + d, 0), Model.zoomSteps.count - 1)]
    }
    @Published var favorites = Set(UserDefaults.standard.stringArray(forKey: "favorites") ?? []) {
        didSet { UserDefaults.standard.set(Array(favorites), forKey: "favorites") }
    }

    private var timer: Timer?
    private var saveWork: DispatchWorkItem?
    private var tunerTick = 0

    init() {
        let saved = UserDefaults.standard.data(forKey: "settings").flatMap(Settings.lenient)
        s = saved ?? Settings()
        // Lenient: presets saved before a setting existed get its default (a strict decode dropped them all, and the
        // next save then overwrote them). Anything still unreadable is kept as-is so a save never loses it.
        if let d = UserDefaults.standard.data(forKey: "presets"), let all = (try? JSONSerialization.jsonObject(with: d)) as? [String: Any] {
            for (name, v) in all {
                if let data = try? JSONSerialization.data(withJSONObject: v), let p = Settings.lenient(data) { userPresets[name] = p }
                else { unreadablePresets[name] = v }
            }
        }
        // Presets saved before dates were kept: date them from now so Modified isn't blank
        let now = Date()
        for n in userPresets.keys where presetMeta[n]?.shownModified == nil { presetMeta[n, default: PresetMeta()].imported = now }
        irLib.scan(sampleRate: 48000)
        if !irLib.isEmpty && !UserDefaults.standard.bool(forKey: "godsCabIntro") {   // first run with the IR pack: switch to it
            s.cabModel = CAB_LIB; s.cabHighCut = 10000; UserDefaults.standard.set(true, forKey: "godsCabIntro")
        }
        applyAll()
        engine.onRestartNeeded = { [weak self] in self?.rateChanged() }
        engine.onOverload = { [weak self] in self?.overloaded() }
        refreshDevices()
        onDeviceListChange { [weak self] in self?.refreshDevices() }
        midi.start()
        updater.checkOnLaunch()
        AppDelegate.model = self
        if !AppDelegate.pending.isEmpty {   // preset files that opened the app
            let urls = AppDelegate.pending; AppDelegate.pending = []
            DispatchQueue.main.async { [weak self] in self?.importPresets(urls); self?.screen = .presets }
        }
        // open ready to play; retry once in case the previous instance is still releasing the device
        DispatchQueue.main.async { [weak self] in self?.start() }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak self] in
            if let self, !self.running { self.start() }
        }
        timer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.tick() }
        }
    }

    var tuning: Tuning { TUNINGS[min(max(tuningIndex, 0), TUNINGS.count - 1)] }

    /// What the guitar sounds like after the shifter.
    var soundsLike: String {
        let shift = s.pitchOn ? Int(s.pitchSemis.rounded()) : 0
        return tuningName(tuning.strings.map { $0 + shift })
    }

    /// Targets reachable by shifting the whole guitar (same string intervals), within an octave.
    var playAsOptions: [(name: String, semis: Int)] {
        (-12...12).reversed().map { k in (tuningName(tuning.strings.map { $0 + k }), k) }
    }

    func playAs(_ semis: Int) {
        s.pitchSemis = Double(semis); s.pitchCents = 0; s.pitchOn = semis != 0
    }
    /// inputUID "none": no input (play recordings, metronome) - the rig gets silence.
    static let noInput = "none"
    var input: AudioDevice? { inputs.first { $0.uid == inputUID } }
    var output: AudioDevice? { outputs.first { $0.uid == outputUID } }

    func refreshDevices() {
        let all = listDevices()
        inputs = all.filter { $0.ins > 0 }
        outputs = all.filter { $0.outs > 0 }
        // Prefer an external interface for input. For output, the interface itself when it has one (one clock, no
        // bridging), otherwise the fastest output that isn't wireless, virtual or routed through an EQ app.
        // Never fall back to the built-in mic: through a high-gain amp it's just static and feedback. With no
        // interface, run without input (a saved interface that's unplugged stays chosen for when it's back).
        if inputUID.isEmpty { inputUID = inputs.first(where: { !$0.isBuiltIn && !$0.isVirtual })?.uid ?? Model.noInput }
        if output == nil {
            let good = outputs.filter(\.goodOutput).sorted { $0.outLatencyMs < $1.outLatencyMs }
            if let i = input, i.outs > 0, !i.isVirtual, let d = outputs.first(where: { $0.uid == i.uid }) { outputUID = d.uid }
            else if let d = good.first ?? outputs.first(where: { !$0.isVirtual }) ?? outputs.first { outputUID = d.uid }
        }
    }

    func toggle() { running ? stop() : start() }

    func start() {
        if ProcessInfo.processInfo.environment["LEVIATHAN_NO_AUDIO"] != nil { return }   // UI testing: never open audio devices
        switch input == nil ? .authorized : AVCaptureDevice.authorizationStatus(for: .audio) {   // no input: no mic needed
        case .authorized: break
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .audio) { ok in DispatchQueue.main.async { ok ? self.start() : self.denied() } }
            return
        default: denied(); return
        }
        guard let o = output else { status = "Pick an output device in Settings → Audio."; return }
        let i = input
        do {
            info = try engine.start(input: i, output: o, buffer: bufferSize)
            running = true
            adviseOverloads = true; recentOverloads = []; startedAt = Date()
            status = i.map { latencyAdvice($0, o) }
                ?? (inputUID == Model.noInput ? "" : "Your input isn\u{2019}t connected, so Leviathan is running without one. Plug it in, or pick another in the Input panel.")
            updateIR(); updateNam()
        } catch {
            engine.stop(); running = false; info = nil
            status = startFailure(error, input: i, output: o)
        }
    }

    /// Says which device refused to start: if the output alone starts, the input is the problem. A USB interface
    /// that stops responding (CoreAudio's 'what' / 'nope') usually comes back after unplugging it.
    private func startFailure(_ error: Error, input i: AudioDevice?, output o: AudioDevice) -> String {
        let detail = " (\(error.localizedDescription))"
        guard let i else { return "Leviathan couldn\u{2019}t start \(o.name). Pick another output, or unplug it and plug it back in." + detail }
        let outputWorks = (try? engine.start(input: nil, output: o, buffer: bufferSize)) != nil
        engine.stop()
        return outputWorks
            ? "\(i.name) isn\u{2019}t responding. Unplug it, plug it back in, then press Start." + detail
            : "Leviathan couldn\u{2019}t start \(o.name). Pick another output, or unplug it and plug it back in." + detail
    }

    func stop() { engine.stop(); running = false; info = nil }

    /// Hardware advice: most of the round trip is the devices, not the app.
    func latencyAdvice(_ i: AudioDevice, _ o: AudioDevice) -> String {
        if let w = o.outputWarning {
            let better = outputs.filter(\.goodOutput).min { $0.outLatencyMs < $1.outLatencyMs }
            return "Output \(o.name): \(w)." + (better.map { " Try \($0.name) in Settings." } ?? "")
        }
        if i.uid != o.uid, i.outs > 0, !i.isVirtual {
            return "Tip: \(i.name) has its own output. Using it for output too skips clock bridging between two devices."
        }
        return ""
    }

    private func denied() {
        status = "Microphone access is off. Enable Leviathan in System Settings → Privacy & Security → Microphone."
    }

    private func restartIfRunning() { if running { start() } }

    private var lastOverloadInfo = Date.distantPast, recentOverloads: [Date] = [], adviseOverloads = true, startedAt = Date.distantPast

    /// Dropouts (heard as crackles). The engine backs off on its own; here we refresh the latency readout at most once a
    /// second (each refresh asks CoreAudio several questions) and, if they keep coming, say what will fix it.
    private func overloaded() {
        meters.dropouts = engine.overloads
        let now = Date()
        if now.timeIntervalSince(lastOverloadInfo) > 1, let o = output, let prev = info {
            let i = input
            lastOverloadInfo = now
            info = engine.currentInfo(input: i, output: o, previous: prev)   // cycle usage went up
        }
        // ignore the first seconds after a start: the engine backs off its timing margin on its own then
        guard now.timeIntervalSince(startedAt) > 3, let o = output else { return }
        let i = input ?? o
        recentOverloads = recentOverloads.filter { now.timeIntervalSince($0) < 10 } + [now]
        guard recentOverloads.count >= 4 else { return }
        recentOverloads = []
        let separate = i.uid != o.uid
        let why = separate ? " Input (\(i.name)) and output (\(o.name)) are separate devices with separate clocks; one interface for both, or the Mac's own speakers or headphones, is the most stable." : ""
        // keep dropouts from going on: step the buffer up (it restarts the engine), up to 256 samples
        if let bigger = [16, 32, 64, 128, 256].first(where: { $0 > bufferSize }) {
            status = "Audio was dropping out (crackles), so the buffer is now \(bigger) samples (Settings → Audio)." + why
            DispatchQueue.main.async { [weak self] in self?.bufferSize = bigger }
        } else if adviseOverloads {
            adviseOverloads = false
            status = "Audio is still dropping out at the largest buffer." + why
        }
    }

    private var rateRestarts: [Date] = []
    private var rateRestartPending = false
    /// Something else changed a device's sample rate. Rebuild at the new rate (avoids a hidden resampler), but
    /// debounced, and at most 3 times in 10 s: if another app keeps changing it back we stop fighting over it.
    private func rateChanged() {
        guard running, !rateRestartPending else { return }
        rateRestarts = rateRestarts.filter { $0.timeIntervalSinceNow > -10 }
        guard rateRestarts.count < 3 else {
            status = "Another app keeps changing the audio device's sample rate. Leviathan will keep running, possibly with a little extra latency."
            return
        }
        rateRestartPending = true
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
            guard let self else { return }
            self.rateRestartPending = false
            self.rateRestarts.append(Date())
            self.restartIfRunning()
        }
    }

    private func saveDevice() {
        UserDefaults.standard.set(inputUID, forKey: "inputUID")
        UserDefaults.standard.set(outputUID, forKey: "outputUID")
    }

    private func syncMute() { params.muted = tunerOn && tunerMute }

    var activeCapture: Capture? { s.capture.isEmpty ? nil : captures[s.capture] }

    /// Push settings (plus the selected capture's model) to the audio thread.
    private func applyAll() {
        if let c = activeCapture { c.apply(to: params) }
        params.capOn = activeCapture != nil
        params.namOn = !s.nam.isEmpty && namHost.model != nil && namHost.loadedName == s.nam
        s.apply(to: params)
        params.bpm = Float(bpm); params.beatsPerBar = beatsPerBar; params.metroVol = Float(metroVol); params.metroOn = metroOn
        if let ms = syncedDelayMs { params.delayTime = Float(ms) }
        params.version &+= 1
    }

    /// Delay time from the tempo when the delay is synced, nil when it's free.
    var syncedDelayMs: Double? {
        guard s.delaySync > 0, s.delaySync < DELAY_DIVS.count else { return nil }
        return min(max(DELAY_DIVS[s.delaySync].beats * 60000 / bpm, 40), 2000)
    }

    /// Tap tempo: average of the last few taps; a 2 s pause starts over. A free (unsynced) delay follows the taps too.
    func tap() {
        let now = Date()
        if let l = taps.last, now.timeIntervalSince(l) > 2 { taps = [] }
        taps.append(now); taps = Array(taps.suffix(5))
        guard taps.count >= 2, let first = taps.first else { return }
        let beat = now.timeIntervalSince(first) / Double(taps.count - 1)
        bpm = min(max((60 / beat * 10).rounded() / 10, 30), 300)
        if s.delaySync == 0 { s.delayTime = min(max((beat * 1000).rounded(), 40), 1500) }
    }

    func stepPreset(_ d: Int) {
        let order = presetOrder
        guard !order.isEmpty else { return }
        let i = order.firstIndex(of: presetName) ?? (d > 0 ? -1 : 0)
        loadAny(order[((i + d) % order.count + order.count) % order.count])
    }

    /// Buttons a MIDI footswitch can press (mapping keys starting with "@").
    func midiAction(_ key: String) {
        switch key {
        case "@next": stepPreset(1)
        case "@prev": stepPreset(-1)
        case "@tuner": tunerOn.toggle()
        case "@tap": tap()
        case "@metro": metroOn.toggle()
        default: break
        }
    }

    func reloadCaptures() {
        captures = Capture.loadAll()
        applyAll(); updateIR()
    }

    /// Load the selected NAM capture into the running engine (at its rate); unloads when none is selected.
    func updateNam() {
        guard let dsp = engine.dsp else { return }
        namHost.select(s.nam, rate: Double(dsp.sr), dsp: dsp) { [weak self] err in
            guard let self else { return }
            if let err { self.status = err }
            self.applyAll(); self.objectWillChange.send()
        }
    }

    /// Use a NAM capture in place of the amp (the rest of the rig, cab included, stays as it is).
    func useNam(_ name: String) {
        var n = s
        n.nam = name; n.capture = ""
        if !name.isEmpty { n.ampOn = true }
        s = n
    }

    func importNam() {
        let added = namHost.importFiles()
        objectWillChange.send()
        if let first = added.first { useNam(first) }
    }

    func rescanNam() { namHost.scan(); objectWillChange.send() }

    /// Select a capture and make the rest of the rig neutral, so it sounds like the original.
    func useCapture(_ name: String) {
        var n = s
        n.capture = name
        if !name.isEmpty { n.nam = "" }
        if !name.isEmpty {
            n.ampOn = true; n.cabOn = true; n.eqOn = false
            n.capDrive = 0; n.capLevel = 0
            for k in PedalKind.allCases { setPedal(k, on: false, in: &n) }
        }
        s = n
    }

    private func settingsChanged(_ old: Settings) {
        applyAll()
        if s.cabModel != old.cabModel || s.cabMic != old.cabMic || s.irA != old.irA || s.irB != old.irB || s.irBlend != old.irBlend
            || s.capture != old.capture || s.irFile != old.irFile { updateIR() }
        if s.nam != old.nam { updateNam() }
        saveWork?.cancel()
        let w = DispatchWorkItem { [s] in UserDefaults.standard.set(try? JSONEncoder().encode(s), forKey: "settings") }
        saveWork = w
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.4, execute: w)
    }

    func updateIR() {
        guard let dsp = engine.dsp else { return }
        dsp.setIR(currentIR(sr: Double(dsp.sr)))
    }

    /// The cab impulse for the current settings at a given rate.
    func currentIR(sr: Double) -> [Float] {
        irLib.scan(sampleRate: sr)
        if let c = activeCapture {
            // not normalised: the fitted IR carries the captured output level
            return resample(c.ir, from: c.sr, to: sr).map { $0 * Float(c.sr / sr) }
        } else if s.cabModel == CAB_LIB, var h = irLib.samples(s.irA, sampleRate: sr) {
            // God's Cab IRs are phase-aligned, so a straight sum blends cleanly
            if s.irBlend > 0.001, !s.irB.isEmpty, let b = irLib.samples(s.irB, sampleRate: sr) {
                let t = Float(s.irBlend)
                let a = h
                h = (0..<max(a.count, b.count)).map { (i: Int) -> Float in
                    let x: Float = i < a.count ? a[i] : 0
                    let y: Float = i < b.count ? b[i] : 0
                    return x * (1 - t) + y * t
                }
            }
            return normaliseIR(h)
        } else if s.cabModel == CAB_IRLIB, let h = cabIRs.samples(s.irFile, sampleRate: sr) {
            return h
        } else if s.cabModel == CAB_USER, let ir = userIR {
            return normaliseIR(resample(ir.samples, from: ir.rate, to: sr))
        }
        return renderCab(CABS[min(s.cabModel, CABS.count - 1)], mic: s.cabMic, sr: sr)
    }

    /// A private copy of the whole rig (for offline re-amping), independent of the live engine.
    func offlineRig(sr: Double) -> DSP {
        let p = Params()
        if let c = activeCapture { c.apply(to: p) }
        p.capOn = activeCapture != nil
        s.apply(to: p)
        let d = DSP(sampleRate: sr, params: p)
        d.setIR(currentIR(sr: sr))
        if !s.nam.isEmpty, let r = NamHost.open(namHost.url(s.nam), rate: sr) {   // its own instance, freed with the rig
            p.namOn = true
            d.setNam(r.model, gain: r.gain, owned: true)
        }
        return d
    }

    func loadIR() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.wav, .aiff, .audio]
        panel.message = "Choose a cabinet impulse response (.wav)"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do {
            userIR = try readWav(url)
            userIRName = url.deletingPathExtension().lastPathComponent
            s.cabModel = CAB_USER
            updateIR()
        } catch {
            status = "Could not read IR: \(error.localizedDescription)"
        }
    }

    // MARK: pedals

    func pedalOn(_ k: PedalKind) -> Bool {
        switch k {
        case .comp: s.compOn
        case .wah: s.wahOn
        case .fuzz: s.fuzzOn
        case .od: s.odOn
        case .chorus: s.chorusOn
        case .flanger: s.flangerOn
        case .phaser: s.phaserOn
        case .trem: s.tremOn
        case .blade: s.bladeOn
        }
    }

    func setPedal(_ k: PedalKind, on: Bool, in n: inout Settings) {
        switch k {
        case .comp: n.compOn = on
        case .wah: n.wahOn = on
        case .fuzz: n.fuzzOn = on
        case .od: n.odOn = on
        case .chorus: n.chorusOn = on
        case .flanger: n.flangerOn = on
        case .phaser: n.phaserOn = on
        case .trem: n.tremOn = on
        case .blade: n.bladeOn = on
        }
    }

    func addPedal(_ k: PedalKind) {
        guard !s.chain.contains(k.rawValue), s.chain.count < Params.maxChain else { return }
        var n = s; n.chain.append(k.rawValue); setPedal(k, on: true, in: &n); s = n
    }

    func removePedal(_ k: PedalKind) { s.chain.removeAll { $0 == k.rawValue } }


    // MARK: presets

    /// Factory presets you've re-saved load your version; "Restore factory" drops it.
    func loadFactory(_ name: String) {
        if userPresets[name] != nil { loadUser(name); return }
        loadFactoryOriginal(name)
    }

    func loadFactoryOriginal(_ name: String) {
        guard let f = FACTORY.first(where: { $0.0 == name }) else { return }
        var n = Settings(); f.1(&n); applyPreset(n, name)
    }

    func isFactory(_ name: String) -> Bool { FACTORY.contains { $0.0 == name } }

    /// Presets in the order the Presets screen lists them (MIDI Program Change counts through this).
    var presetOrder: [String] { FACTORY.map(\.0) + userPresets.keys.filter { !isFactory($0) }.sorted() }

    /// Read or write one setting by its property name (for MIDI). Unknown names are ignored.
    func setting(_ key: String) -> Any? {
        (try? JSONSerialization.jsonObject(with: JSONEncoder().encode(s)) as? [String: Any])?[key]
    }

    func setSetting(_ key: String, _ value: Any) {
        guard var d = (try? JSONSerialization.jsonObject(with: JSONEncoder().encode(s))) as? [String: Any], d[key] != nil else { return }
        d[key] = value
        if let data = try? JSONSerialization.data(withJSONObject: d), let n = try? JSONDecoder().decode(Settings.self, from: data) { s = n }
    }

    func restoreFactory(_ name: String) {
        userPresets[name] = nil; persistPresets(); loadFactoryOriginal(name)
    }

    func loadAny(_ name: String) { isFactory(name) ? loadFactory(name) : loadUser(name) }

    func toggleFavorite(_ name: String) {
        if favorites.contains(name) { favorites.remove(name) } else { favorites.insert(name) }
    }

    func renamePreset(_ old: String, to new: String) {
        let n = new.trimmingCharacters(in: .whitespaces)
        guard !n.isEmpty, n != old, let p = userPresets[old], !isFactory(n) else { return }
        userPresets[n] = p; userPresets[old] = nil
        presetMeta[n] = presetMeta[old]; presetMeta[old] = nil
        if favorites.contains(old) { favorites.remove(old); favorites.insert(n) }
        if presetName == old { presetName = n }
        persistPresets()
    }

    /// Overwrite the current preset with the current settings.
    func saveCurrent() { savePreset(presetName) }

    /// Unsaved changes relative to the stored preset (ignores levels and tuning, which presets don't carry).
    var presetModified: Bool {
        var stored: Settings
        if let u = userPresets[presetName] { stored = u }
        else if let f = FACTORY.first(where: { $0.0 == presetName }) { stored = Settings(); f.1(&stored) }
        else { return false }
        var cur = s
        cur.inGain = stored.inGain; cur.masterVol = stored.masterVol; cur.inputChannel = stored.inputChannel
        cur.pitchOn = stored.pitchOn; cur.pitchSemis = stored.pitchSemis; cur.pitchCents = stored.pitchCents; cur.pitchMix = stored.pitchMix
        cur.cabModel = stored.cabModel
        return cur != stored
    }

    func loadUser(_ name: String) { if let p = userPresets[name] { applyPreset(p, name) } }

    private func applyPreset(_ p: Settings, _ name: String) {
        var n = p
        n.inGain = s.inGain; n.masterVol = s.masterVol; n.inputChannel = s.inputChannel   // presets don't change your levels
        n.pitchOn = s.pitchOn; n.pitchSemis = s.pitchSemis; n.pitchCents = s.pitchCents; n.pitchMix = s.pitchMix   // ...or your tuning
        if n.cabModel == CAB_USER && userIR == nil { n.cabModel = irLib.isEmpty ? 0 : CAB_LIB }
        if n.cabModel == CAB_IRLIB && !cabIRs.files.contains(n.irFile) { n.cabModel = irLib.isEmpty ? 0 : CAB_LIB }
        if n.cabModel == CAB_LIB && irLib.isEmpty { n.cabModel = 0 }
        s = n; presetName = name
        presetMeta[name, default: PresetMeta()].played = Date()
        UserDefaults.standard.set(name, forKey: "preset")
    }

    func savePreset(_ name: String) {
        let n = name.trimmingCharacters(in: .whitespaces)
        guard !n.isEmpty else { return }
        let now = Date()
        var meta = presetMeta[n] ?? PresetMeta()
        if userPresets[n] == nil && meta.created == nil && !isFactory(n) { meta.created = now }
        meta.modified = now; meta.played = now
        presetMeta[n] = meta
        userPresets[n] = s; presetName = n; unreadablePresets[n] = nil
        persistPresets()
    }

    func deletePreset(_ name: String) { userPresets[name] = nil; unreadablePresets[name] = nil; presetMeta[name] = nil; persistPresets() }

    func persistPresets() {
        var all = unreadablePresets
        if let d = try? JSONEncoder().encode(userPresets), let mine = (try? JSONSerialization.jsonObject(with: d)) as? [String: Any] {
            all.merge(mine) { $1 }
        }
        UserDefaults.standard.set(try? JSONSerialization.data(withJSONObject: all), forKey: "presets")
        UserDefaults.standard.set(presetName, forKey: "preset")
    }

    // MARK: meters + tuner

    private func tick() {
        sessions.tick()
        guard running else { meters.set(0, 0, false, 0); return }
        meters.set(params.inPeak, params.outPeak, params.gateOpen, params.pitchLatencyMs)
        checkInputClipping()
        if meters.beat != params.metroBeat { meters.beat = params.metroBeat }
        tunerTick += 1
        if tunerTick % 15 == 0 { meters.load = engine.loadPeak; engine.loadPeak = 0 }
        if tunerOn && tunerTick % 2 == 0 { runTuner() }
    }

    private var lastClips = 0, clipTimes: [Date] = [], clipWarned = false
    /// The interface's converter clipping (before Leviathan's input gain) makes every sound harsh and fizzy;
    /// it can only be fixed with the interface's own gain control, so say so.
    private func checkInputClipping() {
        let c = params.inClips
        guard c != lastClips else { return }
        if c - lastClips >= 3 { clipTimes = clipTimes.filter { $0.timeIntervalSinceNow > -10 } + [Date()] }
        lastClips = c
        guard !clipWarned, clipTimes.count >= 3 else { return }
        clipWarned = true
        status = "Your interface\u{2019}s input is clipping (\(input?.name ?? "input") hits full scale before Leviathan). That makes every tone harsh and fizzy. Turn the gain knob on the interface down until your hardest playing stays out of the red, then use Input Gain here if you need more level."
    }

    private func runTuner() {
        guard let dsp = engine.dsp else { return }
        let n = 2048, w = params.tunerWrite
        var x = [Float](repeating: 0, count: n)
        for i in 0..<n { x[i] = params.tunerRing[(w - n + i + 4096) & 4095] }
        guard let f = detectPitch(x, sr: Double(dsp.sr)) else { return }
        let m = 69 + 12 * log2(f / a4)
        let t = tuning
        // Snap to the nearest string of the chosen tuning if close, otherwise chromatic.
        let nearest = t.strings.enumerated().min { abs(Double($0.element) - m) < abs(Double($1.element) - m) }!
        let target: Int
        if abs(Double(nearest.element) - m) < 1.5 { target = nearest.element; tunerString = nearest.offset }
        else { target = Int(m.rounded()); tunerString = nil }
        tunerFreq = f
        tunerCents = (m - Double(target)) * 100
        tunerNote = noteName(target, flats: t.flats) + "\(target / 12 - 1)"
    }
}

func resample(_ x: [Float], from: Double, to: Double) -> [Float] {
    if abs(from - to) < 1 || x.isEmpty { return x }
    let ratio = from / to, n = Int(Double(x.count) / ratio)
    return (0..<n).map { i in
        let p = Double(i) * ratio, i0 = Int(p), fr = Float(p - Double(i0))
        let a = x[min(i0, x.count - 1)], b = x[min(i0 + 1, x.count - 1)]
        return a + (b - a) * fr
    }
}

// MARK: - Views

/// Global UI scale: text size, and widths of fixed-size controls.
/// Window zoom, set by the sidebar −/+ (⌘= / ⌘− / ⌘0). Views read it while they build and RootView rebuilds
/// the whole tree when it changes, so everything reflows at the new size instead of being bitmap-scaled.
var Z: CGFloat = CGFloat(UserDefaults.standard.object(forKey: "zoom") as? Double ?? 1)
var UI: CGFloat { 1.5 * Z }
var W: CGFloat { 1.4 * Z }

let channelColors: [Color] = [Color(red: 0.35, green: 0.7, blue: 1), Color(red: 0.91, green: 0.57, blue: 0.23), Color(red: 1, green: 0.3, blue: 0.3)]
let panel = Color(white: 0.085), panel2 = Color(white: 0.12), line = Color(white: 0.2)

final class DragStart: ObservableObject { var start: Double? }

struct Knob: View {
    let label: String
    @Binding var value: Double
    /// Settings property this knob edits; makes it MIDI-learnable.
    var key: String? = nil
    let range: ClosedRange<Double>
    let def: Double
    var fmt: (Double) -> String = { String(format: "%.1f", $0 * 10) }
    var color: Color = .accentColor
    @StateObject private var drag = DragStart()
    @Environment(\.midiScope) private var scope

    var body: some View {
        let span = range.upperBound - range.lowerBound
        let t = (value - range.lowerBound) / span
        VStack(spacing: 2) {
            ZStack {
                Circle().fill(Color(white: 0.15)).frame(width: 44 * Z, height: 44 * Z)
                    .shadow(color: .black.opacity(0.6), radius: 2, y: 1)
                Circle().trim(from: 0, to: 0.75).stroke(Color(white: 0.22), style: .init(lineWidth: 5 * Z, lineCap: .round))
                    .rotationEffect(.degrees(135)).frame(width: 60 * Z, height: 60 * Z)
                Circle().trim(from: 0, to: 0.75 * t).stroke(color, style: .init(lineWidth: 5 * Z, lineCap: .round))
                    .rotationEffect(.degrees(135)).frame(width: 60 * Z, height: 60 * Z)
                Capsule().fill(.white).frame(width: 3 * Z, height: 14 * Z).offset(y: -12 * Z).rotationEffect(.degrees(-135 + 270 * t))
            }
            .frame(width: 66 * Z, height: 66 * Z)
            .contentShape(Rectangle())
            .gesture(DragGesture(minimumDistance: 0).onChanged { g in
                if drag.start == nil { drag.start = value }
                let fine = NSEvent.modifierFlags.contains(.shift)
                value = min(max(drag.start! - g.translation.height / (fine ? 800 : 180) * span, range.lowerBound), range.upperBound)
            }.onEnded { _ in drag.start = nil })
            .simultaneousGesture(TapGesture(count: 2).onEnded { value = def })
            .background(ScrollCatcher { d, fine in
                value = min(max(value + Double(d) / (fine ? 2400 : 480) * span, range.lowerBound), range.upperBound)
            })
            .onHover { inside in
                if inside { HoverHelp.shared.show(id: "\(scope)·\(label)", title: scope.isEmpty ? label : "\(scope) · \(label)", text: key.flatMap { EXPLAIN[$0] }) }
                else { HoverHelp.shared.clear(id: "\(scope)·\(label)") }
            }
            .help("Drag or scroll · Shift = fine · double-click resets")
            .overlay { if let key { MidiLearnOverlay(armed: MidiArmed(key: key, name: "\(scope) · \(label)", lo: range.lowerBound, hi: range.upperBound, toggle: false)) } }
            Text(fmt(value)).font(.system(size: 11 * UI).monospacedDigit())
            Text(label.uppercased()).font(.system(size: 9 * UI, weight: .semibold)).foregroundStyle(.secondary).tracking(0.6)
            Button { value = def } label: {
                Image(systemName: "arrow.counterclockwise").font(.system(size: 9 * UI, weight: .semibold))
                    .frame(width: 22 * Z, height: 14 * Z).contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .foregroundStyle(value == def ? Color(white: 0.3) : color)
            .disabled(value == def)
            .help("Reset to \(fmt(def))")
        }
        .frame(width: 60 * W)
    }
}

struct Footswitch: View {
    @Binding var on: Bool
    var color: Color = .accentColor
    var key: String? = nil
    var name = ""
    var body: some View {
        Button { on.toggle() } label: {
            HStack(spacing: 5) {
                Circle().fill(on ? color : Color(white: 0.2)).frame(width: 8, height: 8).shadow(color: on ? color : .clear, radius: 4)
                // always as wide as "OFF", so switching never changes the panel's width (and reflows the rig)
                ZStack(alignment: .leading) {
                    Text("OFF").hidden()
                    Text(on ? "ON" : "OFF")
                }
                .font(.system(size: 9 * UI, weight: .bold)).tracking(1)
            }
            .fixedSize()
            .padding(.horizontal, 8).padding(.vertical, 3)
            .overlay(Capsule().stroke(line))
        }
        .buttonStyle(.plain)
        .overlay { if let key { MidiLearnOverlay(armed: MidiArmed(key: key, name: "\(name) · On/Off", lo: 0, hi: 1, toggle: true), cornerRadius: 20) } }
    }
}

/// Which panel a UnitBox is (set by the rig so the box shows a drag handle), and the drag in progress.
struct PanelIDKey: EnvironmentKey { static let defaultValue: String? = nil }
extension EnvironmentValues { var panelID: String? { get { self[PanelIDKey.self] } set { self[PanelIDKey.self] = newValue } } }

final class DragState: ObservableObject { var pedal: PedalKind?; var panel: String? }

struct UnitBox<Content: View>: View {
    @EnvironmentObject private var m: Model
    @EnvironmentObject private var drag: DragState
    @Environment(\.panelID) private var panelID
    let title: String
    var on: Binding<Bool>? = nil
    var onKey: String? = nil
    var color: Color = .accentColor
    var trailing: AnyView? = nil
    var leading: AnyView? = nil
    @ViewBuilder let content: Content

    var body: some View {
        let folded = m.collapsed.contains(title)
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                CollapseChevron(key: title)
                if let panelID, panelID != "|" {
                    Grip().onDrag { drag.panel = panelID; return NSItemProvider(object: panelID as NSString) }
                }
                leading
                Text(title).font(.system(size: 10 * UI, weight: .bold)).tracking(2).foregroundStyle(.secondary)
                trailing
                Spacer(minLength: 12)
                if let on { Footswitch(on: on, color: color, key: onKey, name: title) }
            }
            .contentShape(Rectangle())
            .onHover { inside in
                let k = "panel:" + (panelID ?? (title == "PEDALS" ? "pedals" : ""))
                if inside { HoverHelp.shared.show(id: "panel·" + title, title: title.capitalized, text: EXPLAIN[k]) } else { HoverHelp.shared.clear(id: "panel·" + title) }
            }
            if !folded {
                content.opacity(on?.wrappedValue == false ? 0.4 : 1).environment(\.midiScope, title)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottom)   // centred across, sitting on the bottom so knob rows line up
                    .padding(.top, 10)
            } else {
                // Folded: the content stays in the layout, hidden and squeezable to zero height. GridFlow measures the
                // expanded size (unspecified proposal) and the folded one (height 0), so a collapse never moves its neighbours.
                content.padding(.top, 10).hidden()
                    .frame(minHeight: 0, maxHeight: .infinity, alignment: .top).clipped().allowsHitTesting(false)
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(RoundedRectangle(cornerRadius: 12).fill(panel))
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(line))
    }
}

/// Vertical fader: click anywhere on the track to jump there, drag to move, double-click for 0.
struct Fader: View {
    @Binding var value: Double
    let range: ClosedRange<Double>
    var step = 0.5
    var color: Color = .accentColor
    var height: CGFloat { 165 * Z }; var thumb: CGFloat { 20 * Z }

    var body: some View {
        let span = range.upperBound - range.lowerBound
        let travel = height - thumb
        let y = { (v: Double) in thumb / 2 + travel * CGFloat(1 - (v - range.lowerBound) / span) }
        let zeroY = y(min(max(0, range.lowerBound), range.upperBound)), vy = y(value)
        ZStack(alignment: .top) {
            Capsule().fill(Color(white: 0.2)).frame(width: 6, height: height)
            Rectangle().fill(Color(white: 0.4)).frame(width: 16, height: 1).offset(y: zeroY)
            Capsule().fill(color).frame(width: 6, height: abs(vy - zeroY)).offset(y: min(vy, zeroY))
            Circle().fill(.white).frame(width: thumb, height: thumb).shadow(color: .black.opacity(0.6), radius: 2, y: 1)
                .offset(y: vy - thumb / 2)
        }
        .frame(width: 30, height: height, alignment: .top)
        .contentShape(Rectangle())
        .gesture(DragGesture(minimumDistance: 0).onChanged { g in
            let t = 1 - Double((g.location.y - thumb / 2) / travel)
            let v = range.lowerBound + min(max(t, 0), 1) * span
            value = (v / step).rounded() * step
        })
        .simultaneousGesture(TapGesture(count: 2).onEnded { value = 0 })
        .help("Click or drag · double-click resets to 0 dB")
    }
}

final class Meters: ObservableObject {
    @Published private(set) var inLevel: Float = 0
    @Published private(set) var outLevel: Float = 0
    @Published private(set) var gateOpen = false
    @Published private(set) var pitchLatency: Float = 0
    @Published var load: Float = 0          // worst render time / buffer time over the last half second
    @Published var dropouts = 0
    @Published var beat = -1                // metronome beat in the bar, -1 when off
    private var plTime: CFTimeInterval = 0
    func set(_ i: Float, _ o: Float, _ g: Bool, _ pl: Float) {
        // publish only real changes (meters round to ~0.5 dB steps visually anyway)
        if abs(i - inLevel) > 1e-3 { inLevel = i }
        if abs(o - outLevel) > 1e-3 { outLevel = o }
        if g != gateOpen { gateOpen = g }
        // latency readouts refresh once a second (immediately when pitch turns on/off) so the numbers don't flicker
        let now = CACurrentMediaTime()
        if (pl == 0) != (pitchLatency == 0) || (now - plTime >= 1 && abs(pl - pitchLatency) >= 0.5) {
            pitchLatency = pl.rounded(); plTime = now
        }
    }
}

struct LevelMeters: View {
    @ObservedObject var mt: Meters
    var body: some View {
        VStack(alignment: .trailing, spacing: 3) {
            HStack(spacing: 4) { Text("IN").font(.system(size: 9 * UI)).foregroundStyle(.secondary); Meter(level: mt.inLevel) }
            HStack(spacing: 4) { Text("OUT").font(.system(size: 9 * UI)).foregroundStyle(.secondary); Meter(level: mt.outLevel) }
        }
    }
}

struct GateDot: View {
    @ObservedObject var mt: Meters
    var body: some View { Circle().fill(mt.gateOpen ? Color.green : Color(white: 0.2)).frame(width: 7, height: 7) }
}

struct PitchLatency: View {
    @ObservedObject var mt: Meters
    var body: some View {
        Text(mt.pitchLatency > 0 ? String(format: "+%.0f ms", mt.pitchLatency) : "")
            .font(.system(size: 10 * UI).monospacedDigit()).foregroundStyle(.secondary)
    }
}

struct Meter: View {
    let level: Float
    var body: some View {
        let db = 20 * log10(max(Double(level), 1e-6)), t = min(max((db + 60) / 60, 0), 1)
        GeometryReader { g in
            ZStack(alignment: .leading) {
                RoundedRectangle(cornerRadius: 3).fill(.black)
                RoundedRectangle(cornerRadius: 3).fill(db > -1 ? Color.red : db > -9 ? Color.yellow : Color.green).frame(width: g.size.width * t)
            }
        }
        .frame(width: 80, height: 8)
        .overlay(RoundedRectangle(cornerRadius: 3).stroke(line))
    }
}

/// Reorders live as a dragged pedal passes over another one.
struct PedalDrop: DropDelegate {
    let target: PedalKind
    let m: Model
    let drag: DragState

    func dropEntered(info: DropInfo) {
        guard let d = drag.pedal, d != target,
              let from = m.s.chain.firstIndex(of: d.rawValue), let to = m.s.chain.firstIndex(of: target.rawValue) else { return }
        withAnimation(.easeOut(duration: 0.15)) { m.s.chain.move(fromOffsets: IndexSet(integer: from), toOffset: to > from ? to + 1 : to) }
    }
    func dropUpdated(info: DropInfo) -> DropProposal? { DropProposal(operation: .move) }
    func performDrop(info: DropInfo) -> Bool { drag.pedal = nil; return true }
}

/// Reorders live as a dragged panel passes over another one.
struct PanelDrop: DropDelegate {
    let target: String
    let m: Model
    let drag: DragState

    func dropEntered(info: DropInfo) {
        guard let d = drag.panel, d != target,
              let from = m.panelOrder.firstIndex(of: d), let to = m.panelOrder.firstIndex(of: target) else { return }
        withAnimation(.easeOut(duration: 0.15)) { m.panelOrder.move(fromOffsets: IndexSet(integer: from), toOffset: to > from ? to + 1 : to) }
    }
    func dropUpdated(info: DropInfo) -> DropProposal? { DropProposal(operation: .move) }
    func performDrop(info: DropInfo) -> Bool { drag.panel = nil; return true }
}

/// ≡ drag handle.
struct Grip: View {
    var body: some View {
        Image(systemName: "line.3.horizontal").foregroundStyle(.secondary).padding(.trailing, 2)
            .contentShape(Rectangle()).help("Drag to reorder")
    }
}

/// Collapse / expand a section; remembered between launches.
struct CollapseChevron: View {
    @EnvironmentObject var m: Model
    let key: String
    var body: some View {
        let folded = m.collapsed.contains(key)
        Button { withAnimation(.easeOut(duration: 0.15)) { m.toggleCollapsed(key) } } label: {
            Image(systemName: "chevron.right").rotationEffect(.degrees(folded ? 0 : 90))
                .font(.system(size: 9 * UI, weight: .bold)).foregroundStyle(.secondary).frame(width: 12 * Z, height: 12 * Z).contentShape(Rectangle())
        }
        .buttonStyle(.plain).help(folded ? "Expand" : "Collapse")
    }
}

struct ContentView: View {
    @EnvironmentObject var m: Model
    @StateObject private var drag = DragState()

    var ch: Color { m.accent }
    let pct: (Double) -> String = { "\(Int(($0 * 100).rounded()))%" }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                if m.tunerOn { tuner }
                GridFlow(fill: true) {
                    ForEach(m.panelOrder, id: \.self) { id in
                        panelView(id).environment(\.panelID, id)
                            .onDrop(of: [.text], delegate: PanelDrop(target: id, m: m, drag: drag))
                            .layoutValue(key: FoldedKey.self, value: m.collapsed.contains(Model.panelTitles[id] ?? ""))
                            .layoutValue(key: RowBreakKey.self, value: id == "|")
                    }
                }
                pedalboard
            }
            .padding(14)
        }
        .safeAreaInset(edge: .bottom, spacing: 0) { if m.explainOnHover { HoverHelpStrip() } }
        .environmentObject(drag)
    }

    func panelView(_ id: String) -> AnyView {
        switch id {
        case "input": AnyView(inputUnit)
        case "gate": AnyView(gate)
        case "pitch": AnyView(pitch)
        case "amp": AnyView(amp)
        case "master": AnyView(master)
        case "cab": AnyView(cab)
        case "eq": AnyView(eq)
        case "delay": AnyView(delay)
        case "reverb": AnyView(reverb)
        case "doubler": AnyView(doubler)
        case "|": AnyView(Color.clear.frame(width: 0, height: 0))
        default: AnyView(EmptyView())
        }
    }

    // MARK: tuner
    var tuner: some View {
        let t = m.tuning, inTune = abs(m.tunerCents) < 3
        return VStack(spacing: 10) {
            HStack {
                Picker("Tuning", selection: $m.tuningIndex) { ForEach(TUNINGS.indices, id: \.self) { Text(TUNINGS[$0].label).tag($0) } }.frame(width: 300 * W)
                Stepper(value: $m.a4, in: 430...450, step: 1) { Text("A4 = \(Int(m.a4)) Hz").monospacedDigit() }
                Spacer()
                Toggle("Mute while tuning", isOn: $m.tunerMute)
            }
            Text(m.tunerNote).font(.system(size: 64 * UI, weight: .heavy)).foregroundStyle(inTune ? Color.green : .primary)
            GeometryReader { g in
                ZStack {
                    LinearGradient(colors: [.red.opacity(0.4), Color(white: 0.2), .green.opacity(0.5), Color(white: 0.2), .red.opacity(0.4)],
                                   startPoint: .leading, endPoint: .trailing).clipShape(Capsule())
                    Rectangle().fill(.white.opacity(0.4)).frame(width: 1)
                    Capsule().fill(inTune ? Color.green : .white).frame(width: 4, height: 26)
                        .offset(x: CGFloat(min(max(m.tunerCents, -50), 50) / 50) * g.size.width / 2)
                        .shadow(color: inTune ? .green : .clear, radius: 6)
                        .animation(.easeOut(duration: 0.08), value: m.tunerCents)
                }
            }
            .frame(width: 420 * Z, height: 18 * Z)
            Text(m.tunerFreq > 0 ? String(format: "%.1f Hz · %+.0f ¢", m.tunerFreq, m.tunerCents) : "play one string")
                .font(.system(size: 12 * UI).monospacedDigit()).foregroundStyle(.secondary)
            HStack(spacing: 8) {
                ForEach(Array(t.strings.enumerated()), id: \.offset) { i, n in
                    Text(noteName(n, flats: t.flats) + "\(n / 12 - 1)")
                        .font(.system(size: 13 * UI, weight: .bold).monospacedDigit())
                        .frame(width: 48 * Z, height: 30 * Z)
                        .background(RoundedRectangle(cornerRadius: 6).fill(m.tunerString == i ? (inTune ? Color.green : ch).opacity(0.85) : panel2))
                        .foregroundStyle(m.tunerString == i ? .black : .primary)
                }
            }
        }
        .padding(16).frame(maxWidth: .infinity)
        .background(RoundedRectangle(cornerRadius: 12).fill(panel))
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(line))
    }

    // MARK: units
    var inputUnit: some View {
        UnitBox(title: "INPUT") {
            VStack(spacing: 8) {
                Picker("", selection: $m.inputUID) {
                    Text("No input (playback only)").tag(Model.noInput)
                    ForEach(m.inputs) { Text($0.name).tag($0.uid) }
                    if m.input == nil && m.inputUID != Model.noInput { Text("Not connected").tag(m.inputUID) }
                }
                    .labelsHidden().frame(width: 120 * W)
                    .onHover { inside in
                        if inside { HoverHelp.shared.show(id: "inputDevice", title: "Input · Interface", text: EXPLAIN["inputDevice"]) } else { HoverHelp.shared.clear(id: "inputDevice") }
                    }
                if (m.input?.ins ?? 1) > 1 {
                    Picker("", selection: $m.s.inputChannel) {
                        ForEach(0..<min(m.input?.ins ?? 1, 8), id: \.self) { Text("Channel \($0 + 1)").tag($0) }
                    }
                    .labelsHidden().frame(width: 120 * W)
                    .onHover { inside in
                        if inside { HoverHelp.shared.show(id: "inputChannel", title: "Input · Channel", text: EXPLAIN["inputChannel"]) } else { HoverHelp.shared.clear(id: "inputChannel") }
                    }
                }
                Knob(label: "Gain", value: $m.s.inGain, key: "inGain", range: -24...24, def: 0, fmt: { String(format: "%+.1f dB", $0) }, color: ch)
            }
        }
    }

    var gate: some View {
        UnitBox(title: "GATE", on: $m.s.gateOn, onKey: "gateOn", color: ch,
                trailing: AnyView(GateDot(mt: m.meters))) {
            HStack {
                Knob(label: "Thresh", value: $m.s.gateThresh, key: "gateThresh", range: -90...(-20), def: -60, fmt: { "\(Int($0)) dB" }, color: ch)
                Knob(label: "Release", value: $m.s.gateRelease, key: "gateRelease", range: 10...2000, def: 60, fmt: { "\(Int($0)) ms" }, color: ch)
                Knob(label: "Depth", value: $m.s.gateDepth, key: "gateDepth", range: -60...0, def: -60, fmt: { $0 <= -60 ? "Full" : "\(Int($0)) dB" }, color: ch)
            }
        }
    }

    var pitch: some View {
        UnitBox(title: "PITCH", on: $m.s.pitchOn, onKey: "pitchOn", color: ch, trailing: AnyView(PitchLatency(mt: m.meters))) {
            VStack(alignment: .center, spacing: 8) {
                // the button shows just the tuning's name, the menu shows the notes too (keeps the panel narrow)
                Menu("Guitar: \(m.tuning.name)") {
                    ForEach(TUNINGS.indices, id: \.self) { i in Button(TUNINGS[i].label) { m.tuningIndex = i } }
                }
                .frame(width: 155 * W)
                HStack(spacing: 4) {
                    Button { m.playAs(max(Int(m.s.pitchSemis.rounded()) - 1, -12)) } label: { Image(systemName: "minus") }
                        .help("Down a semitone")
                    Menu("Play as: \(m.soundsLike)") {
                        ForEach(m.playAsOptions, id: \.semis) { o in
                            let notes = tuningNotes(m.tuning.strings.map { $0 + o.semis }, flats: m.tuning.flats)
                            Button(o.semis == 0 ? "\(o.name) \u{00B7} \(notes) (no shift)" : String(format: "%@ \u{00B7} %@  (%+d)", o.name, notes, o.semis)) { m.playAs(o.semis) }
                        }
                    }
                    .frame(width: 155 * W)
                    Button { m.playAs(min(Int(m.s.pitchSemis.rounded()) + 1, 12)) } label: { Image(systemName: "plus") }
                        .help("Up a semitone")
                }
                HStack(spacing: 2) {
                    Knob(label: "Semis", value: $m.s.pitchSemis, key: "pitchSemis", range: -12...12, def: 0, fmt: { String(format: "%+d", Int($0.rounded())) }, color: ch)
                    Knob(label: "Fine", value: $m.s.pitchCents, key: "pitchCents", range: -50...50, def: 0, fmt: { String(format: "%+d¢", Int($0.rounded())) }, color: ch)
                    Knob(label: "Mix", value: $m.s.pitchMix, key: "pitchMix", range: 0...1, def: 1, fmt: pct, color: ch)
                }
            }
        }
    }

    // MARK: pedals
    var pedalboard: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                CollapseChevron(key: "PEDALS")
                Text("PEDALS  →  AMP").font(.system(size: 10 * UI, weight: .bold)).tracking(2).foregroundStyle(.secondary)
                Menu("+ Add pedal") {
                    ForEach(PedalKind.allCases.filter { !m.s.chain.contains($0.rawValue) }.sorted { $0.title < $1.title }) { k in
                        Button(k.title.capitalized) { m.addPedal(k) }
                    }
                }.frame(width: 130 * W).disabled(m.s.chain.count >= Params.maxChain || m.s.chain.count == PedalKind.allCases.count)
            }
            if m.collapsed.contains("PEDALS") {
                EmptyView()
            } else if m.s.chain.isEmpty {
                Text("No pedals. Add one; signal flows left to right into the amp.")
                    .font(.system(size: 12 * UI)).foregroundStyle(.secondary).padding(.vertical, 20)
            } else {
                GridFlow {
                    ForEach(m.s.chain.compactMap(PedalKind.init(rawValue:))) { k in
                        pedal(k).layoutValue(key: FoldedKey.self, value: m.collapsed.contains(k.title))
                    }
                }
            }
        }
    }

    func pedal(_ k: PedalKind) -> some View {
        let on = Binding(get: { m.pedalOn(k) }, set: { v in var n = m.s; m.setPedal(k, on: v, in: &n); m.s = n })
        let grip = Grip().onDrag { drag.pedal = k; return NSItemProvider(object: "\(k.rawValue)" as NSString) }
        let tools = HStack(spacing: 2) {
            Button { m.removePedal(k) } label: { Image(systemName: "xmark") }.help("Remove")
        }.buttonStyle(.borderless).controlSize(.small)
        return UnitBox(title: k.title, on: on, onKey: k.onKey, color: ch, trailing: AnyView(tools), leading: AnyView(grip)) { pedalKnobs(k) }
            .onDrop(of: [.text], delegate: PedalDrop(target: k, m: m, drag: drag))
    }

    @ViewBuilder func pedalKnobs(_ k: PedalKind) -> some View {
        let hz = { (v: Double) in String(format: "%.2f Hz", v) }
        switch k {
        case .comp:
            HStack(spacing: 2) {
                Knob(label: "Sustain", value: $m.s.compSustain, key: "compSustain", range: 0...1, def: 0.5, color: ch)
                Knob(label: "Attack", value: $m.s.compAttack, key: "compAttack", range: 1...50, def: 10, fmt: { "\(Int($0)) ms" }, color: ch)
                Knob(label: "Level", value: $m.s.compLevel, key: "compLevel", range: -12...12, def: 0, fmt: { String(format: "%+.1f", $0) }, color: ch)
                Knob(label: "Blend", value: $m.s.compBlend, key: "compBlend", range: 0...1, def: 1, fmt: pct, color: ch)
            }
        case .wah:
            VStack(alignment: .center, spacing: 6) {
                Picker("", selection: $m.s.wahAuto) { Text("Pedal").tag(false); Text("Auto").tag(true) }
                    .pickerStyle(.segmented).labelsHidden().frame(width: 150 * W)
                HStack(spacing: 2) {
                    if m.s.wahAuto {
                        Knob(label: "Sens", value: $m.s.wahSens, key: "wahSens", range: 0...1, def: 0.5, color: ch)
                    } else {
                        Knob(label: "Position", value: $m.s.wahPos, key: "wahPos", range: 0...1, def: 0.5, fmt: pct, color: ch)
                    }
                    Knob(label: "Q", value: $m.s.wahQ, key: "wahQ", range: 0...1, def: 0.5, color: ch)
                }
            }
        case .fuzz:
            HStack(spacing: 2) {
                Knob(label: "Fuzz", value: $m.s.fuzzAmt, key: "fuzzAmt", range: 0...1, def: 0.6, color: ch)
                Knob(label: "Tone", value: $m.s.fuzzTone, key: "fuzzTone", range: 0...1, def: 0.5, color: ch)
                Knob(label: "Level", value: $m.s.fuzzLevel, key: "fuzzLevel", range: 0...1, def: 0.5, color: ch)
            }
        case .blade:
            VStack(alignment: .center, spacing: 6) {
                HStack(spacing: 6) {
                    Picker("", selection: $m.s.bladeDiode) { Text("MOSFET").tag(0); Text("Off").tag(1); Text("Silicon").tag(2) }
                        .pickerStyle(.segmented).labelsHidden().frame(width: 190 * W).explain("bladeDiode", "Blade · Diode")
                    Picker("", selection: $m.s.bladeShift) { Text("650").tag(false); Text("1.5k").tag(true) }
                        .pickerStyle(.segmented).labelsHidden().frame(width: 100 * W).explain("bladeShift", "Blade · Mid shift")
                    Toggle("Boost", isOn: $m.s.bladeBoostOn).toggleStyle(.button).explain("bladeBoostOn", "Blade · Boost")
                }
                HStack(spacing: 2) {
                    Knob(label: "Drive", value: $m.s.bladeDrive, key: "bladeDrive", range: 0...1, def: 0.5, color: ch)
                    Knob(label: "Low", value: $m.s.bladeLow, key: "bladeLow", range: 0...1, def: 0.5, fmt: { String(format: "%+.0f", ($0 - 0.5) * 24) }, color: ch)
                    Knob(label: "Mid", value: $m.s.bladeMid, key: "bladeMid", range: 0...1, def: 0.5, fmt: { String(format: "%+.0f", ($0 - 0.5) * 24) }, color: ch)
                    Knob(label: "High", value: $m.s.bladeHigh, key: "bladeHigh", range: 0...1, def: 0.5, fmt: { String(format: "%+.0f", ($0 - 0.5) * 24) }, color: ch)
                    Knob(label: "Boost", value: $m.s.bladeBoost, key: "bladeBoost", range: 0...1, def: 0.5, fmt: { String(format: "+%.0f dB", $0 * 20) }, color: ch)
                    Knob(label: "Level", value: $m.s.bladeLevel, key: "bladeLevel", range: 0...1, def: 0.5, color: ch)
                }
            }
        case .od:
            HStack(spacing: 2) {
                Knob(label: "Drive", value: $m.s.odDrive, key: "odDrive", range: 0...1, def: 0, color: ch)
                Knob(label: "Tone", value: $m.s.odTone, key: "odTone", range: 0...1, def: 0.6, color: ch)
                Knob(label: "Level", value: $m.s.odLevel, key: "odLevel", range: 0...1, def: 0.75, color: ch)
            }
        case .chorus:
            HStack(spacing: 2) {
                Knob(label: "Rate", value: $m.s.chorusRate, key: "chorusRate", range: 0.1...5, def: 0.8, fmt: hz, color: ch)
                Knob(label: "Depth", value: $m.s.chorusDepth, key: "chorusDepth", range: 0...1, def: 0.5, color: ch)
                Knob(label: "Mix", value: $m.s.chorusMix, key: "chorusMix", range: 0...1, def: 0.5, fmt: pct, color: ch)
            }
        case .flanger:
            HStack(spacing: 2) {
                Knob(label: "Rate", value: $m.s.flangerRate, key: "flangerRate", range: 0.05...3, def: 0.25, fmt: hz, color: ch)
                Knob(label: "Depth", value: $m.s.flangerDepth, key: "flangerDepth", range: 0...1, def: 0.7, color: ch)
                Knob(label: "Feedback", value: $m.s.flangerFb, key: "flangerFb", range: 0...1, def: 0.5, fmt: pct, color: ch)
                Knob(label: "Mix", value: $m.s.flangerMix, key: "flangerMix", range: 0...1, def: 0.5, fmt: pct, color: ch)
            }
        case .phaser:
            HStack(spacing: 2) {
                Knob(label: "Rate", value: $m.s.phaserRate, key: "phaserRate", range: 0.05...5, def: 0.5, fmt: hz, color: ch)
                Knob(label: "Depth", value: $m.s.phaserDepth, key: "phaserDepth", range: 0...1, def: 0.7, color: ch)
                Knob(label: "Feedback", value: $m.s.phaserFb, key: "phaserFb", range: 0...1, def: 0.4, fmt: pct, color: ch)
            }
        case .trem:
            HStack(spacing: 2) {
                Knob(label: "Rate", value: $m.s.tremRate, key: "tremRate", range: 0.5...12, def: 5, fmt: hz, color: ch)
                Knob(label: "Depth", value: $m.s.tremDepth, key: "tremDepth", range: 0...1, def: 0.6, fmt: pct, color: ch)
                Knob(label: "Shape", value: $m.s.tremShape, key: "tremShape", range: 0...1, def: 0, fmt: { $0 < 0.1 ? "sine" : $0 > 0.9 ? "square" : String(format: "%.1f", $0 * 10) }, color: ch)
            }
        }
    }

    var amp: some View {
        UnitBox(title: "AMP", on: $m.s.ampOn, onKey: "ampOn", color: ch) {
            VStack(alignment: .center, spacing: 10) {
                HStack(spacing: 6) {
                    // tags: "" = built-in voicings, "cap:<name>" = fitted capture, "nam:<name>" = NAM capture
                    Picker("", selection: Binding(get: { m.s.nam.isEmpty ? (m.s.capture.isEmpty ? "" : "cap:" + m.s.capture) : "nam:" + m.s.nam },
                                                  set: { v in
                                                      if v.hasPrefix("nam:") { m.useNam(String(v.dropFirst(4))) }
                                                      else { var n = m.s; n.nam = ""; m.s = n; m.useCapture(v.hasPrefix("cap:") ? String(v.dropFirst(4)) : "") }
                                                  })) {
                        Section("Leviathan") { Text("Leviathan amp (Clean · Rust · Hot)").tag("") }
                        if !m.namHost.files.isEmpty || !m.s.nam.isEmpty {
                            Section("Real amps (NAM)") {
                                ForEach(m.namHost.files, id: \.self) { n in Text(namTitle(n)).tag("nam:" + n) }
                                if !m.s.nam.isEmpty && !m.namHost.files.contains(m.s.nam) { Text(namTitle(m.s.nam) + " (missing)").tag("nam:" + m.s.nam) }
                            }
                        }
                        if !m.captures.isEmpty {
                            Section("Your captures") { ForEach(m.captures.keys.sorted(), id: \.self) { n in Text(n).tag("cap:" + n) } }
                        }
                    }.labelsHidden().frame(width: 250 * W)
                    Menu {
                        Button("Add NAM captures (.nam)…") { m.importNam() }
                        Button("Open NAM folder") { NSWorkspace.shared.open(NamHost.dir) }
                        Button("Rescan") { m.reloadCaptures(); m.rescanNam() }
                    } label: { Image(systemName: "ellipsis.circle") }
                        .menuStyle(.borderlessButton).menuIndicator(.hidden).fixedSize().help("Add or rescan amp captures")
                }
                if !m.s.nam.isEmpty {
                    HStack(spacing: 2) {
                        Knob(label: "Input", value: $m.s.namInput, key: "namInput", range: -24...24, def: 0, fmt: { String(format: "%+.1f dB", $0) }, color: ch)
                        Knob(label: "Output", value: $m.s.namOutput, key: "namOutput", range: -24...24, def: 0, fmt: { String(format: "%+.1f dB", $0) }, color: ch)
                    }
                    if m.running && !m.params.namOn { Text("Loading…").font(.system(size: 10 * UI)).foregroundStyle(.secondary) }
                } else if m.activeCapture != nil {
                    HStack(spacing: 2) {
                        Knob(label: "Drive", value: $m.s.capDrive, key: "capDrive", range: -18...18, def: 0, fmt: { String(format: "%+.1f dB", $0) }, color: ch)
                        Knob(label: "Level", value: $m.s.capLevel, key: "capLevel", range: -24...12, def: 0, fmt: { String(format: "%+.1f dB", $0) }, color: ch)
                    }
                } else {
                HStack(spacing: 6) {
                    ForEach(VOICINGS.indices, id: \.self) { i in
                        Button { m.s.ampChannel = i } label: {
                            Text(VOICINGS[i].name).font(.system(size: 11 * UI, weight: .heavy)).tracking(2)
                                .frame(maxWidth: .infinity).padding(.vertical, 6)
                                .background(RoundedRectangle(cornerRadius: 7).fill(m.s.ampChannel == i ? channelColors[i] : panel2))
                                .foregroundStyle(m.s.ampChannel == i ? .black : .secondary)
                        }.buttonStyle(.plain)
                    }
                }
                HStack(spacing: 2) {
                    Knob(label: "Gain", value: $m.s.ampGain, key: "ampGain", range: 0...1, def: 0.55, color: ch)
                    Knob(label: "Bass", value: $m.s.bass, key: "bass", range: 0...1, def: 0.5, color: ch)
                    Knob(label: "Mid", value: $m.s.mid, key: "mid", range: 0...1, def: 0.5, color: ch)
                    Knob(label: "Treble", value: $m.s.treble, key: "treble", range: 0...1, def: 0.55, color: ch)
                    Knob(label: "Presence", value: $m.s.presence, key: "presence", range: 0...1, def: 0.55, color: ch)
                    Knob(label: "Depth", value: $m.s.depth, key: "depth", range: 0...1, def: 0.5, color: ch)
                    Knob(label: "Volume", value: $m.s.ampVolume, key: "ampVolume", range: 0...1, def: 0.5, color: ch)
                }
                }
            }
        }
    }

    /// A NAM file name as a readable amp name: "Helga B 5150 BlockLetter - NoBoost" -> "5150 BlockLetter - NoBoost  (Helga B)".
    func namTitle(_ file: String) -> String {
        var n = file.replacingOccurrences(of: #"[-_ ]*ESR[-_ ]?[0-9.,]+"#, with: "", options: .regularExpression)
        n = n.replacingOccurrences(of: "_", with: " ")
        let words = n.split(separator: " ", omittingEmptySubsequences: true)
        // community files start with the creator's first name and initial ("Tim R ...")
        if words.count > 2, words[1].count == 1, words[1].first?.isUppercase == true {
            return words.dropFirst(2).joined(separator: " ") + "  (" + words[0] + " " + words[1] + ")"
        }
        return words.joined(separator: " ")
    }

    /// Rate and loudness of the loaded NAM capture, and what the knobs do.
    var namDetail: String {
        let h = m.namHost
        var parts: [String] = []
        if h.expectedRate > 0 { parts.append(String(format: "%.1f kHz", h.expectedRate / 1000)) }
        if let l = h.loudness { parts.append(String(format: "loudness %.1f dB (normalised)", l)) }
        parts.append("Input = how hard you hit it; Output = level. Keep the cab on unless the capture includes one.")
        return parts.joined(separator: " · ")
    }

    var eq: some View {
        let freqs = ["80", "160", "320", "640", "1.2k", "2.5k", "5k", "10k"]
        return UnitBox(title: "GRAPHIC EQ", on: $m.s.eqOn, onKey: "eqOn", color: ch) {
            HStack(alignment: .bottom, spacing: 6) {
                ForEach(0..<8, id: \.self) { i in
                    VStack(spacing: 4) {
                        Text(String(format: "%+.0f", m.s.eq[i])).font(.system(size: 9 * UI).monospacedDigit()).foregroundStyle(.secondary)
                        Fader(value: $m.s.eq[i], range: -12...12, color: ch)
                        Text(freqs[i]).font(.system(size: 9 * UI)).foregroundStyle(.secondary)
                    }
                    .contentShape(Rectangle())
                    .explain("eq\(i)", "Graphic EQ · \(freqs[i])\(freqs[i].hasSuffix("k") ? "Hz" : " Hz")")
                }
            }
        }
    }

    var cab: some View {
        let lib = m.s.cabModel == CAB_LIB
        return UnitBox(title: "CABINET", on: $m.s.cabOn, onKey: "cabOn", color: ch) {
            VStack(alignment: .center, spacing: 8) {
              VStack(alignment: .leading, spacing: 8) {
                HStack(spacing: 6) {
                    Picker("", selection: $m.s.cabModel) {
                        if !m.cabIRs.isEmpty { Section("Your IRs") { Text("IR library (\(m.cabIRs.files.count))").tag(CAB_IRLIB) } }
                        if !m.irLib.isEmpty { Section("Multi-mic") { Text("God's Cab · Mesa 4x12").tag(CAB_LIB) } }
                        Section("Built-in") { ForEach(CABS.indices, id: \.self) { Text(CABS[$0].name).tag($0) } }
                        if let n = m.userIRName { Section("File") { Text(n).tag(CAB_USER) } }
                    }.labelsHidden().frame(width: 200 * W)
                    Menu {
                        Button("Add IRs to the library (files or a folder)…") { m.importCabIRs() }
                        Button("Load a single IR file…") { m.loadIR() }
                        Button("Open IR library folder") { NSWorkspace.shared.open(CabIRLibrary.root) }
                    } label: { Image(systemName: "ellipsis.circle") }
                        .menuStyle(.borderlessButton).menuIndicator(.hidden).fixedSize().help("Add cabinet IRs")
                }
                if m.s.cabModel == CAB_IRLIB { IRLibraryRow() }
                if lib {
                    irRow("A", $m.s.irA, optional: false)
                    irRow("B", $m.s.irB, optional: true)
                }
              }
                HStack(spacing: 2) {
                    if lib {
                        Knob(label: "A ↔ B", value: $m.s.irBlend, key: "irBlend", range: 0...1, def: 0.35, fmt: pct, color: ch)
                    } else if m.s.cabModel < CABS.count {
                        Knob(label: "Mic Pos", value: $m.s.cabMic, key: "cabMic", range: 0...1, def: 0.3,
                             fmt: { $0 < 0.12 ? "center" : $0 > 0.88 ? "edge" : String(format: "%.1f", $0 * 10) }, color: ch)
                    }
                    Knob(label: "Low Cut", value: $m.s.cabLowCut, key: "cabLowCut", range: 20...300, def: 80, fmt: { "\(Int($0)) Hz" }, color: ch)
                    Knob(label: "High Cut", value: $m.s.cabHighCut, key: "cabHighCut", range: 2500...16000, def: 10000, fmt: { String(format: "%.1fk", $0 / 1000) }, color: ch)
                    Knob(label: "Level", value: $m.s.cabLevel, key: "cabLevel", range: -12...12, def: 0, fmt: { String(format: "%+.1f", $0) }, color: ch)
                }
            }
        }
    }

    /// Mic / position / presence / TS pickers for one God's Cab slot, plus prev/next for quick auditioning.
    @ViewBuilder func irRow(_ label: String, _ rel: Binding<String>, optional: Bool) -> some View {
        let lib = m.irLib, f = lib.file(rel.wrappedValue)
        let pick = { (mic: String, ts: Bool, pos: String, pres: Int) in
            rel.wrappedValue = lib.find(mic: mic, ts: ts, position: pos, pres: pres)?.rel ?? rel.wrappedValue
        }
        HStack(spacing: 6) {
            Text(label).font(.system(size: 10 * UI, weight: .bold)).foregroundStyle(.secondary).frame(width: 12)
            Picker("", selection: Binding(get: { f?.mic ?? "" }, set: { mic in
                if mic.isEmpty { rel.wrappedValue = "" } else { pick(mic, f?.ts ?? false, f?.position ?? "1 inch edge", f?.pres ?? 3) }
            })) {
                if optional || f == nil { Text("Off").tag("") }
                ForEach(lib.mics, id: \.self) { Text($0).tag($0) }
            }.labelsHidden().frame(width: 90 * W)
            if let f {
                Picker("", selection: Binding(get: { f.position }, set: { pick(f.mic, f.ts, $0, f.pres) })) {
                    ForEach(lib.positions(mic: f.mic, ts: f.ts), id: \.self) { Text($0).tag($0) }
                }.labelsHidden().frame(width: 140 * W)
                Menu {
                    Picker("Presence", selection: Binding(get: { f.pres }, set: { pick(f.mic, f.ts, f.position, $0) })) {
                        ForEach(1...5, id: \.self) { Text("Presence \($0)").tag($0) }
                    }
                    Toggle("Tube Screamer in front", isOn: Binding(get: { f.ts }, set: { pick(f.mic, $0, f.position, f.pres) }))
                        .disabled(!lib.tsAvailable(mic: f.mic))
                } label: { Text("P\(f.pres)" + (f.ts ? " · TS" : "")).monospacedDigit() }
                    .fixedSize().help("Amp presence when captured, and a Tube Screamer mid bump baked into the IR")
                Button { rel.wrappedValue = lib.step(f.rel, by: -1) ?? f.rel } label: { Image(systemName: "chevron.left") }.buttonStyle(.borderless)
                Button { rel.wrappedValue = lib.step(f.rel, by: 1) ?? f.rel } label: { Image(systemName: "chevron.right") }.buttonStyle(.borderless)
            }
        }
    }

    var doubler: some View {
        UnitBox(title: "DOUBLER", on: $m.s.doublerOn, onKey: "doublerOn", color: ch) {
            HStack(spacing: 2) {
                Knob(label: "Width", value: $m.s.doublerWidth, key: "doublerWidth", range: 0...1, def: 0.6, fmt: pct, color: ch)
                Knob(label: "Time", value: $m.s.doublerTime, key: "doublerTime", range: 8...40, def: 20, fmt: { "\(Int($0)) ms" }, color: ch)
            }
        }
    }

    var delay: some View {
        UnitBox(title: "DELAY", on: $m.s.delayOn, onKey: "delayOn", color: ch) {
          VStack(alignment: .center, spacing: 8) {
            HStack(spacing: 6) {
                Picker("", selection: $m.s.delaySync) {
                    ForEach(DELAY_DIVS.indices, id: \.self) { Text(DELAY_DIVS[$0].name).tag($0) }
                }.labelsHidden().frame(width: 110 * W).help("Free time, or a note length at the tempo")
                TapButton()
                Text(m.syncedDelayMs.map { String(format: "%.0f BPM · %.0f ms", m.bpm, $0) } ?? String(format: "%.0f BPM", m.bpm))
                    .font(.system(size: 10 * UI).monospacedDigit()).foregroundStyle(.secondary)
            }
            HStack(spacing: 2) {
                if m.s.delaySync == 0 {
                    Knob(label: "Time", value: $m.s.delayTime, key: "delayTime", range: 40...1500, def: 420, fmt: { "\(Int($0)) ms" }, color: ch)
                }
                Knob(label: "Feedback", value: $m.s.delayFeedback, key: "delayFeedback", range: 0...0.92, def: 0.28, fmt: pct, color: ch)
                Knob(label: "Tone", value: $m.s.delayTone, key: "delayTone", range: 0...1, def: 0.45, color: ch)
                Knob(label: "Mix", value: $m.s.delayMix, key: "delayMix", range: 0...1, def: 0.2, fmt: pct, color: ch)
            }
          }
        }
    }

    var reverb: some View {
        UnitBox(title: "REVERB", on: $m.s.revOn, onKey: "revOn", color: ch) {
            HStack(spacing: 2) {
                Knob(label: "Size", value: $m.s.revSize, key: "revSize", range: 0...1, def: 0.45, color: ch)
                Knob(label: "Mix", value: $m.s.revMix, key: "revMix", range: 0...1, def: 0.18, fmt: pct, color: ch)
            }
        }
    }

    var master: some View {
        UnitBox(title: "MASTER") {
            VStack(spacing: 8) {
                Picker("", selection: $m.outputUID) {
                    ForEach(m.outputs) { d in Text(d.outputWarning != nil ? "\(d.name) ⚠︎" : d.name).tag(d.uid) }
                }
                .labelsHidden().frame(width: 120 * W)
                .explain("outputDevice", "Master · Output device")
                if (m.input?.ins ?? 1) > 1 {
                    // stands in for INPUT's channel picker, so the two device pickers sit level
                    Picker("", selection: .constant(0)) { Text("").tag(0) }.labelsHidden().frame(width: 120 * W).hidden()
                }
                Knob(label: "Output", value: $m.s.masterVol, key: "masterVol", range: -40...12, def: -6, fmt: { String(format: "%+.1f dB", $0) }, color: ch)
            }
        }
    }
}

@main
struct LeviathanApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    @StateObject private var model = Model()

    init() {
        // Only one copy at a time (two would fight over the audio interface): bring the running one forward and quit.
        // Runs before the model exists, so this copy never opens the audio device.
        let me = NSRunningApplication.current
        if let id = Bundle.main.bundleIdentifier,
           let other = NSRunningApplication.runningApplications(withBundleIdentifier: id).first(where: { $0.processIdentifier != me.processIdentifier }) {
            other.activate()
            exit(0)
        }
    }
    var body: some Scene {
        WindowGroup("Leviathan - v\(Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "")") {
            RootView().environmentObject(model).environmentObject(model.sessions).environmentObject(model.midi).environmentObject(model.updater)
                .preferredColorScheme(.dark).frame(minWidth: 900, minHeight: 600)
                .background(WindowFrameSaver())
        }
        .windowResizability(.contentMinSize)
    }
}



// MARK: - Explanations on hover (Settings → Appearance), text shared with the Windows app

let EXPLAIN: [String: String] = {
    guard let u = Bundle.main.url(forResource: "explanations", withExtension: "json"), let d = try? Data(contentsOf: u),
          let j = try? JSONSerialization.jsonObject(with: d) as? [String: String] else { return [:] }
    return j
}()

/// What the pointer is over. Kept out of Model so hovering redraws only the strip.
@MainActor
final class HoverHelp: ObservableObject {
    static let shared = HoverHelp()
    @Published private(set) var title = ""
    @Published private(set) var text = ""
    private var id = ""
    func show(id: String, title: String, text: String?) {
        guard let text, !text.isEmpty else { return }
        self.id = id; self.title = title; self.text = text
    }
    /// Clears only if `id` is still what's shown (moving from one control straight onto another keeps the new one).
    func clear(id: String? = nil) {
        if let id, id != self.id { return }
        self.id = ""; title = ""; text = ""
    }
}

struct HoverHelpStrip: View {
    @ObservedObject private var h = HoverHelp.shared
    var idle = "Hover over any control for an explanation. Scroll or drag a dial to change it; ↺ resets it."
    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Image(systemName: "info.circle").foregroundStyle(.secondary)
            if h.text.isEmpty {
                Text(idle).foregroundStyle(.secondary)
            } else {
                Text(h.title).fontWeight(.semibold)
                Text(h.text).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 0)
        }
        .font(.system(size: 12 * UI))
        .lineLimit(2)
        .padding(.horizontal, 14).padding(.vertical, 8)
        .frame(minHeight: 44 * Z, alignment: .leading)
        .background(Color(white: 0.07))
        .overlay(alignment: .top) { Rectangle().fill(line).frame(height: 1) }
    }
}

// MARK: - Scroll wheel on dials

/// Turns scroll-wheel / trackpad scrolling over a view into knob changes. A scroll gesture that started elsewhere
/// (for example scrolling the page) passes over dials without turning them, and momentum never turns them.
struct ScrollCatcher: NSViewRepresentable {
    let onScroll: (CGFloat, Bool) -> Void
    func makeNSView(context: Context) -> CatcherView { let v = CatcherView(); v.onScroll = onScroll; return v }
    func updateNSView(_ v: CatcherView, context: Context) { v.onScroll = onScroll }

    final class CatcherView: NSView {
        var onScroll: ((CGFloat, Bool) -> Void)?
        private var monitor: Any?
        private static var owner: ObjectIdentifier?
        private static var ownerStamp: TimeInterval = -1

        override func hitTest(_ point: NSPoint) -> NSView? { nil }   // clicks and drags go to the knob

        override func viewDidMoveToWindow() {
            super.viewDidMoveToWindow()
            if let monitor { NSEvent.removeMonitor(monitor); self.monitor = nil }
            guard window != nil else { return }
            monitor = NSEvent.addLocalMonitorForEvents(matching: .scrollWheel) { [weak self] e in
                guard let self, let w = self.window, e.window === w, !self.isHiddenOrHasHiddenAncestor else { return e }
                let inside = self.bounds.contains(self.convert(e.locationInWindow, from: nil))
                if e.momentumPhase != [] { return e }
                if e.phase == .began || e.phase == .mayBegin {
                    if inside { Self.owner = ObjectIdentifier(self); Self.ownerStamp = e.timestamp }
                    else if Self.ownerStamp != e.timestamp { Self.owner = nil; Self.ownerStamp = e.timestamp }
                    return inside ? nil : e
                }
                let mine = e.phase == [] ? inside : Self.owner == ObjectIdentifier(self)
                guard mine else { return e }
                var d = e.scrollingDeltaY != 0 ? e.scrollingDeltaY : -e.scrollingDeltaX   // Shift + mouse wheel scrolls sideways
                if !e.hasPreciseScrollingDeltas { d *= 8 }
                if e.isDirectionInvertedFromDevice { d = -d }   // now > 0 = fingers or wheel moving up/away: turn the dial up
                self.onScroll?(d, e.modifierFlags.contains(.shift))
                return nil
            }
        }
        deinit { if let monitor { NSEvent.removeMonitor(monitor) } }
    }
}


extension View {
    /// Show `EXPLAIN[key]` in the rig's explanation strip while the pointer is over this view.
    func explain(_ key: String, _ title: String) -> some View {
        onHover { inside in
            if inside { HoverHelp.shared.show(id: key, title: title, text: EXPLAIN[key]) } else { HoverHelp.shared.clear(id: key) }
        }
    }
}


struct PresetMeta: Codable {
    var created: Date?, modified: Date?, played: Date?
    /// When it came in from a .leviathan file (or, for presets saved before dates were kept, when this version first saw it).
    var imported: Date?
    /// What the Modified column shows: the last save, or the import date if it was never changed.
    var shownModified: Date? { modified ?? imported }
}

enum PresetSort: String, CaseIterable, Identifiable {
    case list, played, modified, created, name
    var id: String { rawValue }
    var title: String {
        switch self { case .list: "List order"; case .played: "Recently played"; case .modified: "Recently modified"; case .created: "Recently created"; case .name: "Name" }
    }
}

extension DateFormatter {
    /// 2026-11-06 14:05
    static let presetStamp: DateFormatter = { let f = DateFormatter(); f.locale = Locale(identifier: "en_US_POSIX"); f.dateFormat = "yyyy-MM-dd HH:mm"; return f }()
}
