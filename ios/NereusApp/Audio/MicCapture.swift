// NereusSDR for iOS: the phone's microphone, captured, encoded as Opus and sent on the media connection's microphone line, or measured alone for the mic level meter
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import NereusLink
import NereusMedia
import os

/// The microphone while this phone transmits (R-IOS-20; spec section 5.4
/// items 6 to 8; the media control document, "Microphone line").
///
/// It runs its own `AVAudioEngine`, input only, on the audio session the
/// band already plays on (`.playAndRecord`, mode `.default`, A2DP only, so
/// the microphone stays on the iPhone while AirPods play, unless the
/// operator chose theirs). iOS voice processing stays off, so the Core's
/// PROC, EQ and leveler shape the voice. The input is converted to 48 kHz
/// mono, cut into 20 ms frames, encoded with the Opus microphone profile
/// (48 kbit/s full band, 24 kbit/s under Save data, in-band FEC) and sent
/// as RTP on the line through ``MediaUplink``; with Lossless chosen and a
/// line that carries it, each frame goes as five 4 ms L16 packets, as the
/// desktop sends them (R-IOS-09). With no line open the frames go nowhere.
///
/// The sound comes in small pieces: while sending, the session is asked
/// for a 10 ms I/O buffer (``KeyedIOBuffer``, which puts the duration from
/// before back when sending ends, since the band's playback shares it) and
/// the input feeds an `AVAudioSinkNode`, whose block only
/// copies each piece into a lock-free ``MicrophoneRing`` and wakes the
/// drain with a flag and a signal (``MicrophoneInputWake``; a tap, which
/// iOS hands over about 100 ms at a time, is the fallback when the sink
/// cannot start). The drain thread empties the ring in order on the
/// encoder's queue, and ``MicrophoneSender``
/// sends each frame as it fills, one every 20 ms by a steady clock, so the
/// Core's microphone buffer gets an even stream and never five frames at
/// once. Each key's spacing of sends is logged when it ends.
///
/// The mic level meter's level (``startLevel(_:lost:)``) runs on the same
/// engine: the peak of each 100 ms of sound goes to the level's handler,
/// and nothing is encoded or sent unless sending is on too. The level alone
/// never changes the session's I/O buffer. The engine runs while sending
/// or the level is on, and stops when both are off.
///
/// Before the input opens, haptics and system sounds are allowed while
/// recording, so the dial's ticks still play while the microphone is open
/// and after it closes.
///
/// It never asks for the microphone permission (that is the question after
/// pairing, spec section 5.3 item 4): without it, a start fails with words
/// that say where to allow it. Starting and stopping run in order on its
/// own queue; a stop ends a start still on its way.
final class MicCapture: MicrophoneSource, @unchecked Sendable {
    /// Why a start failed when the microphone is not allowed.
    static let notAllowedText =
        "NereusSDR can't use this phone's microphone. Allow it in the Settings app, under NereusSDR."
    /// Why transmitting stopped when the input could not start again after
    /// the sound moved.
    static let lostText =
        "This phone's microphone stopped when the sound moved, so transmitting stopped. Key again to transmit."

    private static let logger = Logger(subsystem: "NereusSDR", category: "audio.microphone")
    private static let sampleRate = 48_000.0
    /// The level's piece: 100 ms at 48 kHz, as often as the meter moved before.
    private static let levelSamples = 4_800

    private let uplink: MediaUplink
    /// The paced sends; on while sending is on. Thread-safe.
    private let sender: MicrophoneSender
    private let permitted: @Sendable () -> Bool
    /// Allows haptics and system sounds while recording, before the input opens.
    private let allowHaptics: @Sendable () -> Void
    /// False only in tests: the input is taken as started, with no engine.
    private let startsEngine: Bool
    /// Starting and stopping, in order.
    private let queue = DispatchQueue(label: "NereusSDR.audio.microphone")
    /// Encoding, sending and the level, in order, off the tap's thread.
    private let encodeQueue = DispatchQueue(label: "NereusSDR.audio.microphone.encode")

    // Read and written on `queue` only.
    /// Told when sending stops by itself (``onLost(_:)``).
    private var lost: (@Sendable (String) -> Void)?
    private var engine: AVAudioEngine?
    /// The input runs: the engine, or its stand-in in tests.
    private var running = false
    /// The 10 ms I/O buffer, held while sending.
    private let keyedBuffer: KeyedIOBuffer
    private var observer: (any NSObjectProtocol)?
    /// Sending is on: a key or VOX.
    private var sending = false
    /// The level is on: the mic level meter is on screen.
    private var metering = false
    /// Told when the level stops by itself; present while the level is on.
    private var levelLost: (@Sendable () -> Void)?

    /// The engine's input runs through the sink node, not the tap (the log).
    private var usesSink = false
    /// The engine's ring (its overflow count, for the log).
    private var ring: MicrophoneRing?
    /// The engine's drain thread, woken by the input thread.
    private var wake: MicrophoneInputWake?

    // Read and written on `encodeQueue` only.
    /// Present while the level is on.
    private var levelHandler: (@Sendable (Float) -> Void)?
    /// The loudest sample of the level's current piece, and how many it holds.
    private var levelPeak: Float = 0
    private var levelCount = 0

    /// How a key's microphone is encoded.
    enum Encoding: Equatable {
        /// L16, five 4 ms packets a frame, on a line that carries lossless.
        case l16
        case opus(OpusEncoder.Profile)
    }

    /// Lossless where it is chosen, not fallen back and carried by the
    /// line; otherwise Opus at the choice's rate.
    static func encoding(for uplink: MediaUplink) -> Encoding {
        encoding(sendsLossless: uplink.microphoneSendsLossless, profile: uplink.microphoneOpusProfile)
    }

    static func encoding(sendsLossless: Bool, profile: OpusEncoder.Profile) -> Encoding {
        sendsLossless ? .l16 : .opus(profile)
    }

    /// The current key's Opus encoder, made when the key starts so a frame
    /// can go as Opus at any turn; read by the sender's thread.
    final class KeyEncoder: @unchecked Sendable {
        private let lock = NSLock()
        private var opus: OpusEncoder?

        func set(_ encoder: OpusEncoder?) {
            lock.withLock { opus = encoder }
        }

        /// The configuration of the actual stored encoder, not the uplink's
        /// requested next profile (read-only regression observation).
        var profile: OpusEncoder.Profile? {
            lock.withLock { opus?.profile }
        }

        /// Tests read libopus's actual configuration, rather than a request.
        var bitrate: Int? {
            lock.withLock { opus?.bitrate }
        }

        var identity: ObjectIdentifier? {
            lock.withLock { opus.map(ObjectIdentifier.init) }
        }

        func encode(_ samples: [Float], bitrate: Int) throws -> Data {
            try lock.withLock {
                guard let opus else { throw CancellationError() }
                try opus.setBitrate(bitrate)
                return try opus.encode(samples)
            }
        }
    }

    private let keyEncoder = KeyEncoder()

    /// A frame as the sender holds it until its turn: its samples as they are.
    static func heldFrame(_ samples: [Float]) -> Data {
        samples.withUnsafeBufferPointer { Data(buffer: $0) }
    }

    static func heldSamples(_ frame: Data) -> [Float] {
        let count = frame.count / MemoryLayout<Float>.size
        return [Float](unsafeUninitializedCapacity: count) { buffer, initialised in
            _ = frame.copyBytes(to: buffer)
            initialised = count
        }
    }

    /// Sends one held frame in the format the line takes at its turn, not
    /// the one the key began with (the desktop's sendMicAudio decides on
    /// each pump, RemoteMediaController.cpp): L16 while lossless is carried,
    /// so a fall back mid-key leaves it at the next frame, else Opus.
    static func sendFrame(_ frame: Data, lossless: Bool, opus: ([Float]) throws -> Data,
                          sendL16: (Data) -> Bool, sendOpus: (Data) -> Bool) -> Bool {
        let samples = heldSamples(frame)
        if lossless {
            return sendL16(L16Audio.microphoneFrame(mono: samples))
        }
        guard let packet = try? opus(samples) else {
            return false
        }
        return sendOpus(packet)
    }

    init(uplink: MediaUplink,
         permitted: @escaping @Sendable () -> Bool = { AVAudioApplication.shared.recordPermission == .granted },
         ioBuffer: any IOBufferDurationPort = SessionIOBufferDuration(),
         allowHaptics: @escaping @Sendable () -> Void = { SystemAudioSession.allowHapticsWhileRecording() },
         startsEngine: Bool = true,
         makeClock: @escaping @Sendable () -> any MicrophoneSendClock = { HostSendClock() }) {
        self.uplink = uplink
        self.permitted = permitted
        self.allowHaptics = allowHaptics
        self.startsEngine = startsEngine
        keyedBuffer = KeyedIOBuffer(port: ioBuffer)
        let keyEncoder = keyEncoder
        sender = MicrophoneSender(send: { [uplink] frame in
            // R-IOS-09: the current choice also applies to a frame's Opus
            // rate at its turn, using the same codec state throughout a key.
            MicCapture.sendFrame(frame, lossless: uplink.microphoneSendsLossless,
                                 opus: { try keyEncoder.encode($0, bitrate: uplink.microphoneOpusProfile.bitrate) },
                                 sendL16: { uplink.sendMicrophoneL16($0) }, sendOpus: { uplink.sendMicrophone($0) })
        }, makeClock: makeClock)
    }

    deinit {
        if let observer {
            NotificationCenter.default.removeObserver(observer)
        }
    }

    func start() async -> MicrophoneStart {
        await withCheckedContinuation { continuation in
            queue.async { [self] in
                continuation.resume(returning: startSendingOnQueue())
            }
        }
    }

    func stop() {
        queue.async { [self] in
            if sending {
                logSends(sender.end())
            }
            sending = false
            // The band plays on this session too: its I/O buffer goes back
            // at once, whether or not the level keeps the engine running.
            keyedBuffer.release()
            tearDownIfIdle()
        }
    }

    func servicesReset() {
        queue.async { [self] in
            // The old engine is gone with the media services; whatever
            // still wants the input gets a new one.
            tearDown()
            rebuild()
        }
    }

    func onLost(_ lost: @escaping @Sendable (String) -> Void) {
        queue.async { [self] in
            self.lost = lost
        }
    }

    /// Waits for the starts and stops queued so far (tests).
    func settle() {
        queue.sync {}
    }

    /// The input runs now, after the starts and stops queued so far (tests).
    var inputRunning: Bool {
        queue.sync { running }
    }

    /// Feeds already converted 48 kHz mono input through the real sender,
    /// only with the existing no-engine test stand-in. No input device opens.
    func takeSamplesWithoutEngine(_ samples: [Float]) {
        precondition(!startsEngine)
        encodeQueue.sync { handle(samples) }
    }

    /// The actual key encoder's current configuration (tests). Reading
    /// it does not reconfigure, encode, restart or arm a microphone key.
    var activeOpusProfile: OpusEncoder.Profile? {
        keyEncoder.profile
    }

    var activeOpusBitrate: Int? { keyEncoder.bitrate }
    var activeOpusIdentity: ObjectIdentifier? { keyEncoder.identity }

    func startLevel(_ level: @escaping @Sendable (Float) -> Void,
                    lost: @escaping @Sendable () -> Void) async -> MicrophoneStart {
        await withCheckedContinuation { continuation in
            queue.async { [self] in
                continuation.resume(returning: startLevelOnQueue(level, lost: lost))
            }
        }
    }

    func stopLevel() {
        queue.async { [self] in
            metering = false
            levelLost = nil
            encodeQueue.async { [self] in
                levelHandler = nil
            }
            tearDownIfIdle()
        }
    }

    /// One key's sends, for the next test on the air to read.
    private func logSends(_ stats: MicrophoneSendStats) {
        func milliseconds(_ value: Double?) -> String {
            value.map { String(format: "%.1f", $0) } ?? "-"
        }
        let granted = String(format: "%.1f", AVAudioSession.sharedInstance().ioBufferDuration * 1000)
        let capture = usesSink ? "sink" : "tap"
        let overflowed = ring?.overflowed ?? 0
        let line = "microphone sends: frames=\(stats.frames) unsent=\(stats.unsent) dropped=\(stats.dropped) "
            + "maxBacklog=\(stats.maxBacklog) gapMs min=\(milliseconds(stats.minGapMs)) "
            + "max=\(milliseconds(stats.maxGapMs)) mean=\(milliseconds(stats.meanGapMs)) "
            + "capture=\(capture) ioBufferMs=\(granted) ringOverflow=\(overflowed)"
        Self.logger.notice("\(line, privacy: .public)")
    }

    // MARK: On the queue

    private func startSendingOnQueue() -> MicrophoneStart {
        if sending {
            return .started
        }
        // Before the engine starts, so it starts on the small buffer.
        keyedBuffer.hold()
        if let failure = ensureEngine() {
            keyedBuffer.release()
            return failure
        }
        // Each frame's format is decided at its turn (R-IOS-09, sendFrame);
        // the key's Opus encoder is ready from the start for any frame.
        let encoder: OpusEncoder
        do {
            encoder = try OpusEncoder(profile: uplink.microphoneOpusProfile)
        } catch {
            Self.logger.warning("the microphone's encoder did not start: \(error.localizedDescription)")
            keyedBuffer.release()
            tearDownIfIdle()
            return .failed(reason: PttController.microphoneDidNotStartText)
        }
        // After everything already queued from the input: a new key starts
        // with no sound from before it.
        encodeQueue.sync {
            keyEncoder.set(encoder)
            sender.begin(encode: { MicCapture.heldFrame($0) })
        }
        sending = true
        return .started
    }

    private func startLevelOnQueue(_ level: @escaping @Sendable (Float) -> Void,
                                   lost: @escaping @Sendable () -> Void) -> MicrophoneStart {
        if let failure = ensureEngine() {
            return failure
        }
        levelLost = lost
        encodeQueue.sync {
            levelHandler = level
            levelPeak = 0
            levelCount = 0
        }
        metering = true
        return .started
    }

    /// Builds and starts the engine unless it runs already; nil once it
    /// runs, or why it could not.
    private func ensureEngine() -> MicrophoneStart? {
        if running {
            return nil
        }
        guard permitted() else {
            return .failed(reason: Self.notAllowedText)
        }
        // Before the input opens: iOS silences the dial's haptic ticks
        // while an app records unless this is on (TestFlight build 5).
        allowHaptics()
        guard startsEngine else {
            running = true
            return nil
        }
        do {
            try build()
            running = true
            return nil
        } catch {
            Self.logger.warning("the microphone did not start: \(error.localizedDescription)")
            tearDown()
            return .failed(reason: PttController.microphoneDidNotStartText)
        }
    }

    /// Builds and starts the engine: the sink node first, the tap if the
    /// sink will not start.
    ///
    /// The I/O buffer is left as it is: ``keyedBuffer`` asks for 10 ms
    /// while sending (a preference: iOS may grant more, and the sends are
    /// paced anyway), and the level alone runs on the session's own.
    private func build() throws {
        do {
            try build(sink: true)
        } catch {
            Self.logger.warning("the microphone's sink did not start, using a tap: \(error.localizedDescription)")
            tearDown()
            try build(sink: false)
        }
    }

    private func build(sink useSink: Bool) throws {
        let engine = AVAudioEngine()
        let input = engine.inputNode
        // Voice processing off: the Core's own processing shapes the voice.
        if input.isVoiceProcessingEnabled {
            try input.setVoiceProcessingEnabled(false)
        }
        let format = input.outputFormat(forBus: 0)
        // The ring carries the first channel alone, at the input's rate.
        guard format.sampleRate > 0, format.channelCount > 0, format.commonFormat == .pcmFormatFloat32,
              !format.isInterleaved || format.channelCount == 1,
              let source = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: format.sampleRate, channels: 1,
                                         interleaved: false),
              let target = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: Self.sampleRate, channels: 1,
                                         interleaved: false),
              let converter = AVAudioConverter(from: source, to: target),
              let ring = MicrophoneRing(capacity: Int(format.sampleRate)) else {
            throw MicCaptureError.noInput
        }
        let capture = Capture(ring: ring, converter: converter, source: source, target: target)
        let encodeQueue = self.encodeQueue
        // The drain runs on the wake's own thread, in order with the rest of
        // the encoder's queue; the input thread never queues a block.
        guard let wake = MicrophoneInputWake({ [weak self] in
            encodeQueue.sync {
                self?.drain(capture)
            }
        }) else {
            throw MicCaptureError.noInput
        }
        self.wake = wake
        if useSink {
            // The input thread only copies, sets a flag and signals.
            let sink = AVAudioSinkNode { _, frameCount, bufferList in
                let buffers = UnsafeMutableAudioBufferListPointer(UnsafeMutablePointer(mutating: bufferList))
                if let first = buffers.first, let data = first.mData {
                    ring.write(data.assumingMemoryBound(to: Float.self), count: Int(frameCount))
                    wake.inputArrived()
                }
                return noErr
            }
            engine.attach(sink)
            engine.connect(input, to: sink, format: format)
        } else {
            input.installTap(onBus: 0, bufferSize: AVAudioFrameCount(format.sampleRate / 50), format: format) {
                buffer, _ in
                if let channel = buffer.floatChannelData?[0] {
                    ring.write(channel, count: Int(buffer.frameLength))
                    wake.inputArrived()
                }
            }
        }
        engine.prepare()
        try engine.start()
        self.engine = engine
        self.ring = ring
        usesSink = useSink
        // A route change stops the engine; build it again for the new input.
        observer = NotificationCenter.default.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine,
                                                          queue: nil) { [weak self] _ in
            guard let self else {
                return
            }
            queue.async { [weak self] in
                self?.rebuild()
            }
        }
    }

    private func rebuild() {
        guard sending || metering else {
            return
        }
        tearDown()
        do {
            try build()
            running = true
        } catch {
            Self.logger.warning("the microphone did not restart after a route change: \(error.localizedDescription)")
            let wasSending = sending
            let levelGone = metering ? levelLost : nil
            levelLost = nil
            if sending {
                logSends(sender.end())
            }
            sending = false
            metering = false
            keyedBuffer.release()
            tearDown()
            encodeQueue.async { [self] in
                levelHandler = nil
            }
            // Nothing is sent any more: the key must not stay on silently.
            if wasSending {
                lost?(Self.lostText)
            }
            // The meter's level stops too: it is told, so it shows no level
            // rather than the last one.
            levelGone?()
        }
    }

    /// Stops the engine once neither sending nor the level wants it.
    private func tearDownIfIdle() {
        if !sending, !metering {
            tearDown()
        }
    }

    private func tearDown() {
        if let observer {
            NotificationCenter.default.removeObserver(observer)
            self.observer = nil
        }
        if let engine {
            if !usesSink {
                engine.inputNode.removeTap(onBus: 0)
            }
            engine.stop()
            self.engine = nil
        }
        // After the input has stopped: the drain thread ends, and nothing
        // drains this engine's ring after it.
        wake?.close()
        wake = nil
        running = false
        ring = nil
    }

    // MARK: The encoder's queue

    /// One engine's input: its ring, and the conversion to 48 kHz mono.
    private final class Capture: @unchecked Sendable {
        let ring: MicrophoneRing
        let converter: AVAudioConverter
        let source: AVAudioFormat
        let target: AVAudioFormat

        init(ring: MicrophoneRing, converter: AVAudioConverter, source: AVAudioFormat, target: AVAudioFormat) {
            self.ring = ring
            self.converter = converter
            self.source = source
            self.target = target
        }
    }

    /// Takes what waits in the ring, converted to 48 kHz mono, to the
    /// level and the sender.
    private func drain(_ capture: Capture) {
        let raw = capture.ring.readAll()
        guard !raw.isEmpty, let samples = Self.convert(raw, with: capture) else {
            return
        }
        handle(samples)
    }

    /// `raw`, one channel at the input's rate, as 48 kHz; nil if it would not convert.
    private static func convert(_ raw: [Float], with capture: Capture) -> [Float]? {
        if capture.source.sampleRate == capture.target.sampleRate {
            return raw
        }
        guard let buffer = AVAudioPCMBuffer(pcmFormat: capture.source, frameCapacity: AVAudioFrameCount(raw.count)),
              let channel = buffer.floatChannelData?[0] else {
            return nil
        }
        raw.withUnsafeBufferPointer { samples in
            if let base = samples.baseAddress {
                channel.update(from: base, count: samples.count)
            }
        }
        buffer.frameLength = AVAudioFrameCount(raw.count)
        let ratio = capture.target.sampleRate / capture.source.sampleRate
        let capacity = AVAudioFrameCount((Double(raw.count) * ratio).rounded(.up)) + 32
        guard let converted = AVAudioPCMBuffer(pcmFormat: capture.target, frameCapacity: capacity) else {
            return nil
        }
        var given = false
        var error: NSError?
        capture.converter.convert(to: converted, error: &error) { _, status in
            if given {
                status.pointee = .noDataNow
                return nil
            }
            given = true
            status.pointee = .haveData
            return buffer
        }
        guard error == nil, let output = converted.floatChannelData?[0], converted.frameLength > 0 else {
            return nil
        }
        return Array(UnsafeBufferPointer(start: output, count: Int(converted.frameLength)))
    }

    /// The level's peak for each 100 ms, then, while sending, the frames.
    private func handle(_ samples: [Float]) {
        if let levelHandler {
            levelPeak = max(levelPeak, Self.peak(samples))
            levelCount += samples.count
            if levelCount >= Self.levelSamples {
                levelHandler(levelPeak)
                levelPeak = 0
                levelCount = 0
            }
        }
        sender.take(samples)
    }

    /// The largest magnitude among `samples`, 0 for none.
    static func peak(_ samples: [Float]) -> Float {
        var largest: Float = 0
        for sample in samples {
            let magnitude = abs(sample)
            if magnitude > largest {
                largest = magnitude
            }
        }
        return largest
    }
}
