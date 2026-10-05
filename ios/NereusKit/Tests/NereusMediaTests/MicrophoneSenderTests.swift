// NereusSDR for iOS: tests for the microphone's paced sends: one frame per 20 ms tick however the samples arrive
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMedia

/// A send clock the test moves by hand: `advance(to:)` runs each piece of
/// scheduled work whose deadline has come, at its deadline plus `lateness`.
final class ManualSendClock: MicrophoneSendClock, @unchecked Sendable {
    private let lock = NSLock()
    private var time: UInt64 = 1_000_000_000
    private var waiting: (deadline: UInt64, work: @Sendable () -> Void)?
    private var closed = false
    /// How late each piece of work runs after its deadline.
    var lateness: UInt64 = 0

    var now: UInt64 { lock.withLock { time } }
    var isClosed: Bool { lock.withLock { closed } }

    func schedule(at deadline: UInt64, _ work: @escaping @Sendable () -> Void) {
        lock.withLock {
            if !closed {
                waiting = (deadline, work)
            }
        }
    }

    func close() {
        lock.withLock {
            closed = true
            waiting = nil
        }
    }

    /// Moves time to `target`, running the work that falls due on the way.
    func advance(to target: UInt64) {
        while true {
            let due: (@Sendable () -> Void)? = lock.withLock {
                guard let next = waiting, next.deadline + lateness <= target else {
                    return nil
                }
                waiting = nil
                time = max(time, next.deadline + lateness)
                return next.work
            }
            guard let due else {
                break
            }
            due()
        }
        lock.withLock { time = max(time, target) }
    }

    func advance(by nanoseconds: UInt64) {
        advance(to: now + nanoseconds)
    }
}

/// What the sender sent, and when by the manual clock.
private final class SendLog: @unchecked Sendable {
    private let lock = NSLock()
    private var entries: [(time: UInt64, frame: Int)] = []
    let clock: ManualSendClock

    init(clock: ManualSendClock) {
        self.clock = clock
    }

    func record(_ data: Data) -> Bool {
        let index = data.withUnsafeBytes { Int($0.loadUnaligned(as: Int64.self)) }
        let time = clock.now
        lock.withLock { entries.append((time, index)) }
        return true
    }

    var frames: [Int] { lock.withLock { entries.map(\.frame) } }
    var times: [UInt64] { lock.withLock { entries.map(\.time) } }

    /// The gaps between sends, in milliseconds.
    var gapsMs: [Double] {
        let times = self.times
        return zip(times.dropFirst(), times).map { Double($0 - $1) / 1_000_000 }
    }
}

@Suite struct MicrophoneSenderTests {
    private static let sampleRate = 48_000
    private static let millisecond: UInt64 = 1_000_000

    /// An encoder that writes the frame's index (its first sample is its
    /// first sample's position in the stream) into the frame's data.
    private static func indexEncoder(_ frame: [Float]) throws -> Data {
        var index = Int64(frame[0]) / Int64(MicrophoneSender.frameSamples)
        return Data(bytes: &index, count: MemoryLayout<Int64>.size)
    }

    /// The microphone: `count` samples whose values are their positions.
    private static func samples(from start: Int, count: Int) -> [Float] {
        (start ..< start + count).map(Float.init)
    }

    /// Runs `seconds` of a microphone that hands over `pieceMs` of sound
    /// at a time, each piece as it ends; returns the log.
    private static func run(pieceMs: Int, seconds: Int, lateness: UInt64 = 0,
                            jitterMs: [Int] = [0]) -> (SendLog, MicrophoneSender, ManualSendClock) {
        let clock = ManualSendClock()
        clock.lateness = lateness
        let log = SendLog(clock: clock)
        let sender = MicrophoneSender(send: { log.record($0) }, makeClock: { clock })
        sender.begin(encode: indexEncoder)
        let piece = sampleRate * pieceMs / 1000
        let start = clock.now
        var position = 0
        var index = 0
        while position < sampleRate * seconds {
            let jitter = UInt64(jitterMs[index % jitterMs.count]) * millisecond
            index += 1
            // This piece ends, and is handed over, at its end plus jitter.
            let handedAt = start + UInt64(position + piece) * 1_000_000_000 / UInt64(sampleRate) + jitter
            clock.advance(to: handedAt)
            sender.take(samples(from: position, count: piece))
            position += piece
        }
        // Let the backlog go out.
        clock.advance(by: 500 * millisecond)
        return (log, sender, clock)
    }

    @Test func aMicrophoneInHundredMillisecondLumpsStillSendsEveryTwentyMilliseconds() {
        let (log, sender, _) = Self.run(pieceMs: 100, seconds: 3)
        let gaps = log.gapsMs
        #expect(log.frames.count == 150)
        #expect(gaps.allSatisfy { abs($0 - 20) < 0.5 }, "gaps \(gaps.prefix(12))")
        #expect(!gaps.contains { $0 < 10 }, "never two back to back")
        let stats = sender.end()
        #expect(stats.frames == 150)
        #expect(stats.minGapMs.map { abs($0 - 20) < 0.5 } == true)
        #expect(stats.maxGapMs.map { abs($0 - 20) < 0.5 } == true)
        #expect(stats.meanGapMs.map { abs($0 - 20) < 0.5 } == true)
        #expect(stats.maxBacklog == 5)
        #expect(stats.dropped == 0)
    }

    @Test func lumpsThatArriveUnevenlyStaySpacedAtLeastTwentyMilliseconds() {
        // 87 to 116 ms between lumps, as the Core logged from build 4.
        let (log, _, _) = Self.run(pieceMs: 100, seconds: 3, jitterMs: [0, 13, 0, 16, 3])
        let gaps = log.gapsMs
        #expect(log.frames == Array(0 ..< 150))
        #expect(gaps.allSatisfy { $0 >= 19.5 }, "gaps \(gaps)")
        #expect(gaps.filter { $0 > 20.5 }.count <= 10, "late lumps stretch a gap now and then, never bunch")
    }

    @Test func aMicrophoneInTenMillisecondPiecesSendsEveryTwentyMilliseconds() {
        let (log, sender, _) = Self.run(pieceMs: 10, seconds: 3)
        #expect(log.frames.count == 150)
        #expect(log.gapsMs.allSatisfy { abs($0 - 20) < 0.5 }, "gaps \(log.gapsMs.prefix(12))")
        let stats = sender.end()
        #expect(stats.maxBacklog <= 1, "a frame goes as soon as it fills")
    }

    @Test func noFrameIsLostOrSentTwice() {
        for pieceMs in [5, 10, 23, 100, 107] {
            let (log, _, _) = Self.run(pieceMs: pieceMs, seconds: 2)
            let piece = Self.sampleRate * pieceMs / 1000
            let fed = (Self.sampleRate * 2 + piece - 1) / piece * piece
            let expected = fed / MicrophoneSender.frameSamples
            #expect(log.frames == Array(0 ..< expected), "pieces of \(pieceMs) ms")
        }
    }

    @Test func aClockThatAlwaysWakesLateKeepsTheFullRate() {
        // Every tick runs 12 ms late: the grid holds, 20 ms apart.
        let (log, _, _) = Self.run(pieceMs: 100, seconds: 2, lateness: 12 * Self.millisecond)
        #expect(log.frames == Array(0 ..< 100))
        #expect(log.gapsMs.dropFirst().allSatisfy { abs($0 - 20) < 0.5 }, "gaps \(log.gapsMs)")
    }

    @Test func aStalledClockRestartsTheCadenceInsteadOfBursting() {
        let clock = ManualSendClock()
        let log = SendLog(clock: clock)
        let sender = MicrophoneSender(send: { log.record($0) }, makeClock: { clock })
        sender.begin(encode: Self.indexEncoder)
        var position = 0
        for lump in 0 ..< 20 {
            // The clock's thread stalls for 70 ms during the fifth lump.
            clock.lateness = lump == 4 ? 70 * Self.millisecond : 0
            clock.advance(by: 100 * Self.millisecond)
            clock.lateness = 0
            sender.take(Self.samples(from: position, count: 4800))
            position += 4800
        }
        clock.advance(by: 1_000 * Self.millisecond)
        #expect(log.frames == Array(0 ..< 100))
        let gaps = log.gapsMs
        #expect(gaps.contains { $0 >= 80 }, "the stall shows as one long gap")
        #expect(gaps.allSatisfy { $0 >= 19.5 }, "and never as a burst: \(gaps)")
    }

    @Test func theKeyEndsAtOnceAndDropsWhatWaits() {
        let clock = ManualSendClock()
        let log = SendLog(clock: clock)
        let sender = MicrophoneSender(send: { log.record($0) }, makeClock: { clock })
        sender.begin(encode: Self.indexEncoder)
        sender.take(Self.samples(from: 0, count: 4800))
        #expect(log.frames == [0], "the first frame goes at once")
        #expect(sender.waitingFrames == 4)
        let stats = sender.end()
        #expect(stats.frames == 1)
        #expect(stats.dropped == 4)
        #expect(clock.isClosed)
        clock.advance(by: 200 * Self.millisecond)
        sender.take(Self.samples(from: 4800, count: 4800))
        #expect(log.frames == [0], "nothing after the key ends")
        #expect(!sender.isOn)
    }

    @Test func aNewKeyStartsFresh() {
        let clock = ManualSendClock()
        let log = SendLog(clock: clock)
        let sender = MicrophoneSender(send: { log.record($0) }, makeClock: { clock })
        sender.begin(encode: Self.indexEncoder)
        sender.take(Self.samples(from: 0, count: 500))
        sender.end()
        sender.begin(encode: Self.indexEncoder)
        // The 500 samples of the last key are gone: this frame is whole.
        sender.take(Self.samples(from: 960, count: 960))
        #expect(log.frames == [1])
    }

    /// Through the real uplink: RTP timestamps step by 960 and sequences
    /// by one, one packet per 20 ms tick.
    @Test func throughTheUplinkTimestampsStepByNineHundredSixty() throws {
        let peer = ScriptedMediaPeer()
        peer.enableMicrophoneLine(ssrc: 0x1234_5678)
        peer.openMicrophoneLine()
        let uplink = MediaUplink()
        uplink.attach(peer, microphoneSsrc: 0x1234_5678)
        let clock = ManualSendClock()
        let sender = MicrophoneSender(send: { uplink.sendMicrophone($0) }, makeClock: { clock })
        let encoder = try OpusEncoder(profile: .microphone)
        sender.begin(encode: { try encoder.encode($0) })
        for lump in 0 ..< 5 {
            clock.advance(by: 100 * Self.millisecond)
            sender.take((0 ..< 4800).map { Float(0.3 * sin(Double(lump * 4800 + $0) * 0.0576)) })
            #expect(peer.microphonePackets.count == lump * 5 + 1, "one frame at the lump, the rest on the ticks")
        }
        clock.advance(by: 100 * Self.millisecond)
        let packets = peer.microphonePackets
        #expect(packets.count == 25)
        for (next, previous) in zip(packets.dropFirst(), packets) {
            #expect(next.timestamp == previous.timestamp &+ 960)
            #expect(next.sequence == previous.sequence &+ 1)
        }
        #expect(sender.end().frames == 25)
    }

    /// The ring hands the reader what the writer put in, in order, and
    /// counts what did not fit.
    @Test func theRingCarriesSamplesInOrderAndCountsOverflow() throws {
        let ring = try #require(MicrophoneRing(capacity: 1000))
        let first = (0 ..< 600).map(Float.init)
        let second = (600 ..< 1200).map(Float.init)
        #expect(first.withUnsafeBufferPointer { ring.write($0.baseAddress!, count: 600) } == 600)
        #expect(second.withUnsafeBufferPointer { ring.write($0.baseAddress!, count: 600) } == 400)
        #expect(ring.overflowed == 200)
        #expect(ring.readAll() == Array((0 ..< 1000).map(Float.init)))
        #expect(ring.readAll().isEmpty)
    }

    /// The phone's own clock, on its own thread, with time moved by hand:
    /// it runs work no earlier than its deadline, runs only the latest
    /// schedule, and nothing after it closes. Nothing here depends on how
    /// busy the computer is: each check waits for the thread to reach a
    /// known point.
    @Test func theHostClockWakesAtItsDeadline() throws {
        let time = VirtualTime()
        let clock = HostSendClock(now: { time.now }, waitUntil: { time.wait(until: $0) })
        defer { clock.close() }
        let ran = RanAt()
        let deadline = clock.now + 30 * Self.millisecond
        clock.schedule(at: deadline - 10 * Self.millisecond) { ran.mark(0) }
        clock.schedule(at: deadline) { ran.mark(clock.now) }
        // Past the first deadline, short of the second: the thread goes on
        // to wait for the second, and nothing has run.
        time.advance(to: deadline - 5 * Self.millisecond)
        #expect(time.waits(for: deadline), "the thread waits for the latest schedule's deadline")
        #expect(ran.times.isEmpty, "nothing runs before its deadline, and the replaced work never")
        time.advance(to: deadline)
        #expect(ran.wait(forCount: 1))
        #expect(ran.times == [deadline], "the second schedule replaced the first and ran at its deadline")
        clock.close()
        // Refused as it is made: nothing can run it later.
        clock.schedule(at: clock.now) { ran.mark(1) }
        time.advance(to: deadline + 100 * Self.millisecond)
        #expect(ran.times == [deadline], "nothing runs once closed")
    }
}

private final class RanAt: @unchecked Sendable {
    private let condition = NSCondition()
    private var marks: [UInt64] = []

    func mark(_ time: UInt64) {
        condition.lock()
        marks.append(time)
        condition.broadcast()
        condition.unlock()
    }

    var times: [UInt64] {
        condition.lock()
        defer { condition.unlock() }
        return marks
    }

    /// Waits, up to 30 s, until `count` marks are in.
    func wait(forCount count: Int) -> Bool {
        let limit = Date().addingTimeInterval(30)
        condition.lock()
        defer { condition.unlock() }
        while marks.count < count {
            if !condition.wait(until: limit) {
                return marks.count >= count
            }
        }
        return true
    }
}

/// Time for ``HostSendClock``'s thread that only the test moves: the
/// thread's wait returns once the test has moved time to its deadline, and
/// the test can see which deadline the thread waits for.
private final class VirtualTime: @unchecked Sendable {
    private let condition = NSCondition()
    private var current: UInt64 = 1_000_000_000
    private var waitingFor: UInt64?

    var now: UInt64 {
        condition.lock()
        defer { condition.unlock() }
        return current
    }

    func wait(until deadline: UInt64) {
        condition.lock()
        waitingFor = deadline
        condition.broadcast()
        while current < deadline {
            condition.wait()
        }
        waitingFor = nil
        condition.broadcast()
        condition.unlock()
    }

    func advance(to time: UInt64) {
        condition.lock()
        current = max(current, time)
        condition.broadcast()
        condition.unlock()
    }

    /// Whether the thread comes to wait for `deadline` within 30 s.
    func waits(for deadline: UInt64) -> Bool {
        let limit = Date().addingTimeInterval(30)
        condition.lock()
        defer { condition.unlock() }
        while waitingFor != deadline {
            if !condition.wait(until: limit) {
                return waitingFor == deadline
            }
        }
        return true
    }
}
