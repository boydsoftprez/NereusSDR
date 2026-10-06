// NereusSDR for iOS: the dial's haptics while and after the microphone is open, and the microphone closing with its meter
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMedia
@testable import NereusSDR
import Testing
import UIKit

/// TestFlight build 5 (2026-09-27): the dial's haptic ticks stopped once
/// the mic level meter had opened the microphone, and came back only when
/// the app was closed. iOS silences haptics while an app records unless
/// its session allows them. The phone's own microphone (``MicCapture``,
/// here with no engine) allows them before its input opens, and its input
/// stops when the last meter closes, the app goes to the background, the
/// phone locks, or a key ends with no meter up.
@Suite("The dial's haptics and the microphone", .serialized)
@MainActor
struct MicrophoneHapticsTests {
    /// Counts the microphone's calls to allow haptics while recording.
    final class HapticsCounter: @unchecked Sendable {
        private let lock = NSLock()
        private var count = 0

        var allowed: Int { lock.withLock { count } }

        func allow() {
            lock.withLock { count += 1 }
        }
    }

    struct Rig {
        let capture: MicCapture
        let level: LiveMicLevel
        let haptics: HapticsCounter
        let center: NotificationCenter
        let gain: CurrentValueSubject<Double?, Never>
        let allowed: CurrentValueSubject<Bool, Never>
    }

    private func rig() -> Rig {
        let haptics = HapticsCounter()
        let capture = MicCapture(uplink: MediaUplink(), permitted: { true },
                                 ioBuffer: FakeIOBufferDuration(preferred: 0.02),
                                 allowHaptics: { haptics.allow() }, startsEngine: false)
        let gain = CurrentValueSubject<Double?, Never>(0)
        let allowed = CurrentValueSubject<Bool, Never>(true)
        let center = NotificationCenter()
        let level = LiveMicLevel(microphone: capture, gain: gain.eraseToAnyPublisher(),
                                 allowed: allowed.eraseToAnyPublisher(), notificationCenter: center)
        return Rig(capture: capture, level: level, haptics: haptics, center: center, gain: gain, allowed: allowed)
    }

    @Test("the microphone allows haptics before its input opens, for the meter and for a key")
    func allowsHapticsBeforeTheInputOpens() async {
        let rig = rig()
        #expect(rig.haptics.allowed == 0)
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.capture.inputRunning)
        #expect(rig.haptics.allowed == 1)
        rig.level.hide(viewer)
        await rig.level.settle()
        #expect(await rig.capture.start() == .started)
        #expect(rig.haptics.allowed == 2)
        rig.capture.stop()
        rig.capture.settle()
    }

    @Test("a microphone that may not open does not touch the session's haptics")
    func notAllowedLeavesHapticsAlone() async {
        let haptics = HapticsCounter()
        let capture = MicCapture(uplink: MediaUplink(), permitted: { false },
                                 ioBuffer: FakeIOBufferDuration(preferred: 0.02),
                                 allowHaptics: { haptics.allow() }, startsEngine: false)
        #expect(await capture.startLevel({ _ in }, lost: {}) == .failed(reason: MicCapture.notAllowedText))
        #expect(haptics.allowed == 0)
        #expect(!capture.inputRunning)
    }

    @Test("the input stops when the last meter closes")
    func inputStopsWhenTheMeterCloses() async {
        let rig = rig()
        let panel = UUID()
        let section = UUID()
        rig.level.show(panel)
        rig.level.show(section)
        await rig.level.settle()
        #expect(rig.capture.inputRunning)
        rig.level.hide(panel)
        await rig.level.settle()
        #expect(rig.capture.inputRunning, "the Modes tab's meter is still up")
        rig.level.hide(section)
        await rig.level.settle()
        #expect(!rig.capture.inputRunning)
    }

    @Test("the input stops in the background and when the phone locks")
    func inputStopsInTheBackgroundAndWhenLocked() async {
        let rig = rig()
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.capture.inputRunning)
        rig.center.post(name: UIApplication.didEnterBackgroundNotification, object: nil)
        await rig.level.settle()
        #expect(!rig.capture.inputRunning)
        rig.center.post(name: UIApplication.willEnterForegroundNotification, object: nil)
        await rig.level.settle()
        #expect(rig.capture.inputRunning)
        rig.center.post(name: UIApplication.protectedDataWillBecomeUnavailableNotification, object: nil)
        await rig.level.settle()
        #expect(!rig.capture.inputRunning)
        rig.center.post(name: UIApplication.protectedDataDidBecomeAvailableNotification, object: nil)
        await rig.level.settle()
        #expect(rig.capture.inputRunning)
        rig.level.hide(viewer)
        await rig.level.settle()
        #expect(!rig.capture.inputRunning)
    }

    @Test("unkeying stops the input unless a meter is up, and the meter's close stops it then")
    func inputStopsWhenTheKeyEnds() async {
        let rig = rig()
        #expect(await rig.capture.start() == .started)
        #expect(rig.capture.inputRunning)
        rig.capture.stop()
        #expect(!rig.capture.inputRunning)

        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(await rig.capture.start() == .started)
        rig.capture.stop()
        #expect(rig.capture.inputRunning, "the meter is still up")
        rig.level.hide(viewer)
        await rig.level.settle()
        #expect(!rig.capture.inputRunning)
    }

    @Test("a phone that may no longer transmit closes the microphone")
    func inputStopsWhenTransmitIsNoLongerAllowed() async {
        let rig = rig()
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.capture.inputRunning)
        rig.allowed.send(false)
        await rig.level.settle()
        #expect(!rig.capture.inputRunning)
        rig.level.hide(viewer)
        await rig.level.settle()
    }
}
