// NereusSDR for iOS: the microphone's 10 ms I/O buffer is held only while transmitting, and the one from before comes back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMedia
@testable import NereusSDR
import Testing

/// A stand-in for the session's I/O buffer duration that records what is
/// asked of it.
final class FakeIOBufferDuration: IOBufferDurationPort, @unchecked Sendable {
    private let lock = NSLock()
    private var preferred: TimeInterval
    private let granted: TimeInterval
    private var asked: [TimeInterval] = []
    private let refuses: Bool

    init(preferred: TimeInterval, granted: TimeInterval = 0.0213, refuses: Bool = false) {
        self.preferred = preferred
        self.granted = granted
        self.refuses = refuses
    }

    struct Refused: Error {}

    var preferredIOBufferDuration: TimeInterval {
        lock.withLock { preferred }
    }

    var ioBufferDuration: TimeInterval {
        granted
    }

    /// Every duration asked for, in order.
    var requests: [TimeInterval] {
        lock.withLock { asked }
    }

    func setPreferredIOBufferDuration(_ duration: TimeInterval) throws {
        if refuses {
            throw Refused()
        }
        lock.withLock {
            asked.append(duration)
            preferred = duration
        }
    }
}

/// TestFlight build 5 (2026-09-27): the band sounded robotic once the mic
/// level meter had opened the microphone, which asked the shared session
/// for a 10 ms I/O buffer and never put the old one back. The microphone
/// here starts no engine (a test stand-in), so only the session's buffer
/// is watched.
@Suite("The microphone's I/O buffer", .serialized)
struct KeyedIOBufferTests {
    static let before: TimeInterval = 0.02

    private func microphone(_ port: FakeIOBufferDuration, permitted: Bool = true) -> MicCapture {
        MicCapture(uplink: MediaUplink(), permitted: { permitted }, ioBuffer: port, startsEngine: false)
    }

    @Test func aKeyHoldsTenMillisecondsAndItsEndPutsTheOldBufferBack() async {
        let port = FakeIOBufferDuration(preferred: Self.before)
        let capture = microphone(port)
        #expect(await capture.start() == .started)
        #expect(port.preferredIOBufferDuration == KeyedIOBuffer.keyedDuration)
        capture.stop()
        capture.settle()
        #expect(port.preferredIOBufferDuration == Self.before)
        #expect(port.requests == [KeyedIOBuffer.keyedDuration, Self.before])
    }

    @Test func theLevelMeterAloneNeverChangesTheBuffer() async {
        let port = FakeIOBufferDuration(preferred: Self.before)
        let capture = microphone(port)
        #expect(await capture.startLevel({ _ in }, lost: {}) == .started)
        #expect(port.preferredIOBufferDuration == Self.before)
        capture.stopLevel()
        capture.settle()
        #expect(port.preferredIOBufferDuration == Self.before)
        #expect(port.requests.isEmpty)
    }

    @Test func unkeyingWithTheMeterStillOpenPutsTheBufferBackAtOnce() async {
        let port = FakeIOBufferDuration(preferred: Self.before)
        let capture = microphone(port)
        #expect(await capture.startLevel({ _ in }, lost: {}) == .started)
        #expect(await capture.start() == .started)
        #expect(port.preferredIOBufferDuration == KeyedIOBuffer.keyedDuration)
        capture.stop()
        capture.settle()
        // The meter still runs the microphone; the band is back on its buffer.
        #expect(port.preferredIOBufferDuration == Self.before)
        capture.stopLevel()
        capture.settle()
        #expect(port.preferredIOBufferDuration == Self.before)
        #expect(port.requests == [KeyedIOBuffer.keyedDuration, Self.before])
    }

    @Test func closingTheMeterWhileKeyedKeepsTheKeysBufferUntilUnkey() async {
        let port = FakeIOBufferDuration(preferred: Self.before)
        let capture = microphone(port)
        #expect(await capture.startLevel({ _ in }, lost: {}) == .started)
        #expect(await capture.start() == .started)
        capture.stopLevel()
        capture.settle()
        #expect(port.preferredIOBufferDuration == KeyedIOBuffer.keyedDuration)
        capture.stop()
        capture.settle()
        #expect(port.preferredIOBufferDuration == Self.before)
    }

    @Test func withNothingAskedBeforeTheBufferIOSRanComesBack() async {
        let port = FakeIOBufferDuration(preferred: 0, granted: 0.0213)
        let capture = microphone(port)
        #expect(await capture.start() == .started)
        capture.stop()
        capture.settle()
        #expect(port.requests == [KeyedIOBuffer.keyedDuration, 0.0213])
    }

    @Test func aStartThatFailsLeavesTheBufferAlone() async {
        let port = FakeIOBufferDuration(preferred: Self.before)
        let capture = microphone(port, permitted: false)
        #expect(await capture.start() == .failed(reason: MicCapture.notAllowedText))
        capture.settle()
        #expect(port.preferredIOBufferDuration == Self.before)
    }

    @Test func aSecondKeyStartRemembersTheBufferFromBeforeTheFirst() async {
        let port = FakeIOBufferDuration(preferred: Self.before)
        let capture = microphone(port)
        #expect(await capture.start() == .started)
        #expect(await capture.start() == .started)
        capture.stop()
        capture.settle()
        #expect(port.preferredIOBufferDuration == Self.before)
        #expect(port.requests == [KeyedIOBuffer.keyedDuration, Self.before])
    }

    @Test func aRefusedBufferIsNeitherHeldNorPutBack() {
        let port = FakeIOBufferDuration(preferred: Self.before, refuses: true)
        let buffer = KeyedIOBuffer(port: port)
        buffer.hold()
        #expect(!buffer.isHeld)
        buffer.release()
        #expect(port.requests.isEmpty)
    }

    @Test func anUnknownBufferIsNotChanged() {
        let port = FakeIOBufferDuration(preferred: 0, granted: 0)
        let buffer = KeyedIOBuffer(port: port)
        buffer.hold()
        #expect(!buffer.isHeld)
        #expect(port.requests.isEmpty)
    }
}
