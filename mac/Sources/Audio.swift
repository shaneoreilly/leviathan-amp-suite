import Foundation
import CoreAudio
import AudioToolbox

struct AudioDevice: Identifiable, Hashable {
    let id: AudioDeviceID
    let uid: String
    let name: String
    let ins: Int
    let outs: Int
    let transport: UInt32
    /// Delay the device itself adds on each side (converters, driver safety margin, the OS's own processing), in ms.
    let inLatencyMs: Double
    let outLatencyMs: Double
    var isVirtual: Bool { transport == kAudioDeviceTransportTypeVirtual || transport == kAudioDeviceTransportTypeAggregate || isRouter }
    var isBuiltIn: Bool { transport == kAudioDeviceTransportTypeBuiltIn }
    /// System-wide EQ / routing apps (eqMac, SoundSource, Loopback...) that sit in front of the real device and buffer audio.
    var isRouter: Bool { ["eqmac", "soundsource", "loopback", "boom", "audio hijack", "blackhole", "soundflower"].contains { name.lowercased().contains($0) } }
    var isWireless: Bool { [kAudioDeviceTransportTypeBluetooth, kAudioDeviceTransportTypeBluetoothLE, kAudioDeviceTransportTypeAirPlay].contains(transport) }
    /// Game pads with a headphone jack (DualSense, Xbox...) show up as outputs but are never what you want to play through.
    var isGamepad: Bool { ["controller", "dualsense", "dualshock", "xbox"].contains { name.lowercased().contains($0) } }
    /// Good enough to pick or suggest for playing through.
    var goodOutput: Bool { outputWarning == nil && !isGamepad }
    var isDisplay: Bool { [kAudioDeviceTransportTypeHDMI, kAudioDeviceTransportTypeDisplayPort].contains(transport) }

    /// Why this device is a poor choice for live playing, if it is.
    var outputWarning: String? {
        if isWireless { return "wireless: adds 100 ms or more" }
        if isRouter { return "routed through an EQ/routing app: adds its own buffer" }
        if isVirtual { return "virtual device: adds a buffer" }
        if isDisplay { return "monitor speakers: most screens add 20–50 ms that macOS can't report" }
        if outLatencyMs > 8 { return String(format: "slow output (+%.0f ms)", outLatencyMs) }
        return nil
    }
}

struct EngineError: LocalizedError {
    let msg: String
    var errorDescription: String? { msg }
}

private let aggregateName = "Leviathan I/O"

// MARK: - Property helpers

private func addr(_ sel: AudioObjectPropertySelector, _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal) -> AudioObjectPropertyAddress {
    AudioObjectPropertyAddress(mSelector: sel, mScope: scope, mElement: kAudioObjectPropertyElementMain)
}

private func getValue<T>(_ obj: AudioObjectID, _ sel: AudioObjectPropertySelector, _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal, _ fallback: T) -> T {
    var a = addr(sel, scope), v = fallback, size = UInt32(MemoryLayout<T>.size)
    return AudioObjectGetPropertyData(obj, &a, 0, nil, &size, &v) == noErr ? v : fallback
}

@discardableResult
private func setValue<T>(_ obj: AudioObjectID, _ sel: AudioObjectPropertySelector, _ value: T, _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal) -> OSStatus {
    var a = addr(sel, scope), v = value
    return AudioObjectSetPropertyData(obj, &a, 0, nil, UInt32(MemoryLayout<T>.size), &v)
}

private func getString(_ obj: AudioObjectID, _ sel: AudioObjectPropertySelector) -> String {
    var a = addr(sel), size = UInt32(MemoryLayout<Unmanaged<CFString>?>.size)
    var s: Unmanaged<CFString>? = nil
    guard AudioObjectGetPropertyData(obj, &a, 0, nil, &size, &s) == noErr, let str = s else { return "" }
    return str.takeRetainedValue() as String
}

private func getArray<T>(_ obj: AudioObjectID, _ sel: AudioObjectPropertySelector, _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal, _ zero: T) -> [T] {
    var a = addr(sel, scope), size: UInt32 = 0
    guard AudioObjectGetPropertyDataSize(obj, &a, 0, nil, &size) == noErr, size > 0 else { return [] }
    var arr = [T](repeating: zero, count: Int(size) / MemoryLayout<T>.stride)
    guard AudioObjectGetPropertyData(obj, &a, 0, nil, &size, &arr) == noErr else { return [] }
    return arr
}

private func channelCount(_ id: AudioDeviceID, _ scope: AudioObjectPropertyScope) -> Int {
    var a = addr(kAudioDevicePropertyStreamConfiguration, scope), size: UInt32 = 0
    guard AudioObjectGetPropertyDataSize(id, &a, 0, nil, &size) == noErr, size > 0 else { return 0 }
    let raw = UnsafeMutableRawPointer.allocate(byteCount: Int(size), alignment: 16)
    defer { raw.deallocate() }
    guard AudioObjectGetPropertyData(id, &a, 0, nil, &size, raw) == noErr else { return 0 }
    let list = UnsafeMutableAudioBufferListPointer(raw.assumingMemoryBound(to: AudioBufferList.self))
    return list.reduce(0) { $0 + Int($1.mNumberChannels) }
}

func listDevices() -> [AudioDevice] {
    getArray(AudioObjectID(kAudioObjectSystemObject), kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, AudioDeviceID(0))
        .map { id in
            let rate = max(nominalRate(id), 1)
            return AudioDevice(id: id, uid: getString(id, kAudioDevicePropertyDeviceUID), name: getString(id, kAudioObjectPropertyName),
                        ins: channelCount(id, kAudioDevicePropertyScopeInput), outs: channelCount(id, kAudioDevicePropertyScopeOutput),
                        transport: getValue(id, kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeGlobal, UInt32(0)),
                        inLatencyMs: Double(deviceLatency(id, kAudioDevicePropertyScopeInput)) / rate * 1000,
                        outLatencyMs: Double(deviceLatency(id, kAudioDevicePropertyScopeOutput)) / rate * 1000)
        }
        .filter { $0.name != aggregateName }
}

func onDeviceListChange(_ block: @escaping () -> Void) {
    var a = addr(kAudioHardwarePropertyDevices)
    AudioObjectAddPropertyListenerBlock(AudioObjectID(kAudioObjectSystemObject), &a, halListenerQueue) { _, _ in
        DispatchQueue.main.async { block() }
    }
}

/// CoreAudio runs property listeners *synchronously* on the queue they're registered with. If that were the
/// main queue, a notification arriving while the main thread is stopping the engine (which waits on CoreAudio)
/// would deadlock both. So listeners run on this private queue and only hop to main asynchronously.
let halListenerQueue = DispatchQueue(label: "leviathan.hal-listeners")

private func sampleRates(_ id: AudioDeviceID) -> [AudioValueRange] {
    getArray(id, kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeGlobal, AudioValueRange())
}

private func supports(_ id: AudioDeviceID, _ rate: Double) -> Bool {
    sampleRates(id).contains { rate >= $0.mMinimum - 0.5 && rate <= $0.mMaximum + 0.5 }
}

private func nominalRate(_ id: AudioDeviceID) -> Double {
    getValue(id, kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, Float64(0))
}

private func setRate(_ id: AudioDeviceID, _ rate: Double) {
    if abs(nominalRate(id) - rate) < 0.5 { return }
    setValue(id, kAudioDevicePropertyNominalSampleRate, Float64(rate))
    for _ in 0..<40 where abs(nominalRate(id) - rate) > 0.5 { Thread.sleep(forTimeInterval: 0.025) }
}

/// Everything the device adds on one side, in frames: its own latency, the driver's safety offset and the latency
/// of the stream itself (converters, and the OS's speaker processing on built-in outputs, which can be 600+ frames).
private func deviceLatency(_ id: AudioDeviceID, _ scope: AudioObjectPropertyScope) -> UInt32 {
    let streams = getArray(id, kAudioDevicePropertyStreams, scope, AudioStreamID(0))
    let streamLat = streams.first.map { getValue($0, kAudioStreamPropertyLatency, kAudioObjectPropertyScopeGlobal, UInt32(0)) } ?? 0
    return getValue(id, kAudioDevicePropertyLatency, scope, UInt32(0)) + getValue(id, kAudioDevicePropertySafetyOffset, scope, UInt32(0)) + streamLat
}

// MARK: - Engine

final class HALEngine {
    struct Info {
        let sampleRate: Double, buffer: Int, inChannels: Int
        let inMs: Double, bufferMs: Double, outMs: Double
        var latencyMs: Double { inMs + bufferMs + outMs }
        var breakdown: String { String(format: "input %.1f + buffers %.1f + output %.1f ms", inMs, bufferMs, outMs) }
    }

    let params: Params
    private(set) var dsp: DSP?
    private var unit: AudioUnit?
    private var aggregate: AudioDeviceID = 0
    private var inBufs: [UnsafeMutablePointer<Float>] = []
    private var inList: UnsafeMutableAudioBufferListPointer?
    private var inCount = 0
    private let silence = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN)
    private var device: AudioDeviceID = 0
    private var listeners: [(AudioObjectID, AudioObjectPropertyAddress, AudioObjectPropertyListenerBlock)] = []
    /// Share of each IO cycle the render may use. Below 1 the HAL wakes us later in the cycle, so output leaves
    /// sooner (saves (1 - usage) x buffer). Raised automatically if the machine ever misses a deadline.
    private(set) var cycleUsage: Float32 = 1
    /// Dropouts reported by the HAL since start.
    private(set) var overloads = 0
    /// Render cycles, and those whose input read failed, since start (written by the audio thread, read by the UI's
    /// stall check). An interface that hangs (USB firmware, 'what') stops one or the other moving.
    private(set) var cycles = 0, inputFails = 0
    /// Called on the main queue when the engine must be rebuilt (device rate changed under us) or a dropout happened.
    var onRestartNeeded: (() -> Void)?
    var onOverload: (() -> Void)?
    private var timebase = mach_timebase_info_data_t()
    /// Worst render time in the last stretch, as a share of the buffer duration (read and reset by the UI).
    var loadPeak: Float = 0
    private var tickPerFrame: Double = 1

    init(params: Params) {
        self.params = params
        silence.initialize(repeating: 0, count: DSP.maxN)
    }

    /// `input` nil: output only (playing recordings without an interface); the rig then gets silence.
    func start(input: AudioDevice?, output: AudioDevice, buffer: Int) throws -> Info {
        stop()

        // Both sides must run at the same rate or CoreAudio has to resample (extra latency). Prefer 48k.
        let rate = [48000.0, 44100.0].first { r in (input.map { supports($0.id, r) } ?? true) && supports(output.id, r) }
            ?? nominalRate(input?.id ?? output.id)
        if let input { setRate(input.id, rate) }
        setRate(output.id, rate)

        let device: AudioDeviceID
        if input == nil {
            device = output.id
        } else if let input, input.id == output.id {
            device = input.id
        } else {
            let desc: [String: Any] = [
                "name": aggregateName,                              // kAudioAggregateDeviceNameKey
                "uid": "local.leviathan.aggregate.\(getpid())",      // kAudioAggregateDeviceUIDKey
                "private": 1,                                       // kAudioAggregateDeviceIsPrivateKey
                "master": output.uid,                               // clock follows the output device
                "subdevices": [                                     // kAudioAggregateDeviceSubDeviceListKey
                    // the input is resampled to follow the output's clock; the low-quality resampler has the
                    // shortest filter (least added delay) and is still clean for a guitar signal
                    ["uid": input!.uid, "drift": 1, "drift quality": 0x20],   // kAudioAggregateDriftCompensationLowQuality
                    ["uid": output.uid, "drift": 0],
                ],
            ]
            var id = AudioDeviceID(0)
            let st = AudioHardwareCreateAggregateDevice(desc as CFDictionary, &id)
            guard st == noErr, id != 0 else { throw EngineError(msg: "Could not combine \(input!.name) and \(output.name) (error \(st)).") }
            aggregate = id; device = id
            Thread.sleep(forTimeInterval: 0.15)   // let the HAL publish the new device
        }

        var range = getValue(device, kAudioDevicePropertyBufferFrameSizeRange, kAudioObjectPropertyScopeGlobal, AudioValueRange(mMinimum: 16, mMaximum: 4096))
        if range.mMinimum <= 0 { range.mMinimum = 16 }
        let frames = UInt32(min(max(Double(buffer), range.mMinimum), min(range.mMaximum, Double(DSP.maxN))))
        setValue(device, kAudioDevicePropertyBufferFrameSize, frames)
        if aggregate != 0, let input {
            setValue(input.id, kAudioDevicePropertyBufferFrameSize, frames)
            setValue(output.id, kAudioDevicePropertyBufferFrameSize, frames)
        }
        let actualFrames = Int(getValue(device, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, frames))
        let sr = nominalRate(device) > 0 ? nominalRate(device) : rate
        self.device = device
        mach_timebase_info(&timebase)
        tickPerFrame = 1e9 / sr * Double(timebase.denom) / Double(timebase.numer)
        overloads = 0; loadPeak = 0; cycles = 0; inputFails = 0
        // render late in the cycle: our DSP needs a few % of it, so most of the cycle is wasted waiting
        setCycleUsage(actualFrames <= 32 ? 0.7 : 0.5)

        var cd = AudioComponentDescription(componentType: kAudioUnitType_Output, componentSubType: kAudioUnitSubType_HALOutput,
                                           componentManufacturer: kAudioUnitManufacturer_Apple, componentFlags: 0, componentFlagsMask: 0)
        guard let comp = AudioComponentFindNext(nil, &cd) else { throw EngineError(msg: "HAL output unit missing") }
        var au: AudioUnit?
        try check(AudioComponentInstanceNew(comp, &au), "create audio unit")
        guard let u = au else { throw EngineError(msg: "audio unit nil") }
        unit = u

        var one: UInt32 = 1, inOn: UInt32 = input == nil ? 0 : 1
        try check(AudioUnitSetProperty(u, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &inOn, 4), "enable input")
        try check(AudioUnitSetProperty(u, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &one, 4), "enable output")
        var dev = device
        try check(AudioUnitSetProperty(u, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &dev, 4), "select device")

        inCount = input.map { max(1, min($0.ins, 8)) } ?? 0
        var inFmt = floatFormat(sr, max(inCount, 1)), outFmt = floatFormat(sr, 2)
        let fsz = UInt32(MemoryLayout<AudioStreamBasicDescription>.size)
        if inCount > 0 { try check(AudioUnitSetProperty(u, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &inFmt, fsz), "input format") }
        try check(AudioUnitSetProperty(u, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outFmt, fsz), "output format")
        var maxFrames = UInt32(DSP.maxN)
        AudioUnitSetProperty(u, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, 4)

        inBufs = (0..<inCount).map { _ in
            let p = UnsafeMutablePointer<Float>.allocate(capacity: DSP.maxN); p.initialize(repeating: 0, count: DSP.maxN); return p
        }
        let list = AudioBufferList.allocate(maximumBuffers: max(inCount, 1))
        for i in 0..<inCount { list[i] = AudioBuffer(mNumberChannels: 1, mDataByteSize: UInt32(DSP.maxN * 4), mData: inBufs[i]) }
        inList = list

        dsp = DSP(sampleRate: sr, params: params)

        var cb = AURenderCallbackStruct(inputProc: { refCon, flags, ts, _, frames, ioData in
            Unmanaged<HALEngine>.fromOpaque(refCon).takeUnretainedValue().render(flags, ts, frames, ioData)
        }, inputProcRefCon: Unmanaged.passUnretained(self).toOpaque())
        try check(AudioUnitSetProperty(u, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb,
                                       UInt32(MemoryLayout<AURenderCallbackStruct>.size)), "render callback")
        try check(AudioUnitInitialize(u), "initialise audio")
        try check(AudioOutputUnitStart(u), "start audio")

        // dropouts: back off the cycle usage step by step until they stop
        listen(device, kAudioDeviceProcessorOverload) { [weak self] in
            guard let self else { return }
            self.overloads += 1
            if self.cycleUsage < 1 { self.setCycleUsage(min(1, self.cycleUsage + 0.15)) }
            self.onOverload?()
        }
        // if anything changes the rate under us the HAL unit would quietly insert a resampler (more delay): rebuild
        for id in Set([device, input?.id ?? device, output.id]) {
            listen(id, kAudioDevicePropertyNominalSampleRate) { [weak self] in
                guard let self, self.unit != nil, abs(nominalRate(id) - sr) > 0.5 else { return }
                self.onRestartNeeded?()
            }
        }
        return info(input: input, output: output, sr: sr, frames: actualFrames)
    }

    private func info(input: AudioDevice?, output: AudioDevice, sr: Double, frames: Int) -> Info {
        let ms = 1000 / sr
        // input: the device + the whole input buffer; output: the device + the part of the cycle we leave unused
        return Info(sampleRate: sr, buffer: frames, inChannels: inCount,
                    inMs: input.map { Double(deviceLatency($0.id, kAudioDevicePropertyScopeInput)) * ms } ?? 0,
                    bufferMs: Double(frames) * (1 + Double(cycleUsage)) * ms,
                    outMs: Double(deviceLatency(output.id, kAudioDevicePropertyScopeOutput)) * ms)
    }

    func currentInfo(input: AudioDevice?, output: AudioDevice, previous: Info) -> Info {
        info(input: input, output: output, sr: previous.sampleRate, frames: previous.buffer)
    }

    private func setCycleUsage(_ u: Float32) {
        cycleUsage = u
        if device != 0 { setValue(device, kAudioDevicePropertyIOCycleUsage, u) }
    }

    private func listen(_ id: AudioObjectID, _ sel: AudioObjectPropertySelector, _ body: @escaping () -> Void) {
        var a = addr(sel)
        let block: AudioObjectPropertyListenerBlock = { _, _ in DispatchQueue.main.async { body() } }
        if AudioObjectAddPropertyListenerBlock(id, &a, halListenerQueue, block) == noErr { listeners.append((id, a, block)) }
    }

    func stop() {
        for (id, a, block) in listeners { var a = a; AudioObjectRemovePropertyListenerBlock(id, &a, halListenerQueue, block) }
        listeners = []
        if device != 0 && aggregate == 0 { setValue(device, kAudioDevicePropertyIOCycleUsage, Float32(1)) }
        device = 0
        if let u = unit {
            AudioOutputUnitStop(u)
            AudioUnitUninitialize(u)
            AudioComponentInstanceDispose(u)
        }
        unit = nil
        if aggregate != 0 { AudioHardwareDestroyAggregateDevice(aggregate); aggregate = 0 }
        inList?.unsafeMutablePointer.deallocate(); inList = nil
        inBufs.forEach { $0.deallocate() }; inBufs = []
        dsp = nil
    }

    private func render(_ flags: UnsafeMutablePointer<AudioUnitRenderActionFlags>, _ ts: UnsafePointer<AudioTimeStamp>,
                        _ frames: UInt32, _ ioData: UnsafeMutablePointer<AudioBufferList>?) -> OSStatus {
        guard let io = ioData, let u = unit, let list = inList, let dsp else { return noErr }
        let t0 = mach_absolute_time()
        defer {
            let load = Float(Double(mach_absolute_time() - t0) / (tickPerFrame * Double(max(frames, 1))))
            if load > loadPeak { loadPeak = load }
        }
        let n = Int(frames)
        cycles &+= 1
        let input: UnsafePointer<Float>
        if inCount > 0 {
            for i in 0..<inCount { list[i].mDataByteSize = frames * 4 }
            let st = AudioUnitRender(u, flags, ts, 1, frames, list.unsafeMutablePointer)
            if st != noErr { inputFails &+= 1 }
            let ch = min(max(params.inputChannel, 0), inCount - 1)
            input = st == noErr ? UnsafePointer(inBufs[ch]) : UnsafePointer(silence)
        } else {
            input = UnsafePointer(silence)   // output only
        }

        let out = UnsafeMutableAudioBufferListPointer(io)
        guard let l = out[0].mData?.assumingMemoryBound(to: Float.self) else { return noErr }
        let r = out.count > 1 ? out[1].mData?.assumingMemoryBound(to: Float.self) ?? l : l
        dsp.process(input: input, outL: l, outR: r, frames: n)
        return noErr
    }

    private func floatFormat(_ sr: Double, _ ch: Int) -> AudioStreamBasicDescription {
        AudioStreamBasicDescription(mSampleRate: sr, mFormatID: kAudioFormatLinearPCM,
                                    mFormatFlags: kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved,
                                    mBytesPerPacket: 4, mFramesPerPacket: 1, mBytesPerFrame: 4, mChannelsPerFrame: UInt32(ch),
                                    mBitsPerChannel: 32, mReserved: 0)
    }

    private func check(_ st: OSStatus, _ what: String) throws {
        if st != noErr { throw EngineError(msg: "Audio error while trying to \(what) (\(st)).") }
    }
}
