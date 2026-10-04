// NereusSDR for iOS: the phone's microphone as transmit uses it (started for a key or VOX, stopped after), and its level alone for the mic level meter
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// What ``TransmitModel`` starts at the PTT tap and while VOX is armed, and
/// stops at the unkey, when VOX is disarmed and when an interruption
/// begins. The app's is ``MicCapture``; tests use a stand-in that records
/// no sound.
///
/// Its level alone (``startLevel(_:lost:)``) is what ``LiveMicLevel`` listens
/// to while the mic level meter is on screen and this phone is not keyed:
/// the microphone runs and each piece of sound's peak comes back, with
/// nothing sent. Sending and the level are independent; the microphone
/// runs while either is on.
protocol MicrophoneSource: AnyObject, Sendable {
    /// Starts sending the microphone on the media connection's microphone
    /// line; returns once it is running, or why it could not. Starting
    /// while running changes nothing. Never asks for the permission.
    func start() async -> MicrophoneStart
    /// Stops sending, at once. Stopping while stopped changes nothing; a
    /// start still on its way ends stopped. The level, if on, carries on.
    func stop()
    /// Starts the level alone: `level` is called, on any thread, with the
    /// peak of each piece of sound (0 for silence, 1 for full scale).
    /// Sends nothing. Returns once the microphone is running, or why it
    /// could not; a second start replaces `level` and `lost`. Never asks
    /// for the permission. `lost` runs, on any thread, if the level stops
    /// by itself (the input could not be built again after a route
    /// change): no more peaks come until the level is started again.
    func startLevel(_ level: @escaping @Sendable (Float) -> Void,
                    lost: @escaping @Sendable () -> Void) async -> MicrophoneStart
    /// Stops the level, at once; a start of it still on its way ends
    /// stopped. Sending, if on, carries on.
    func stopLevel()
    /// The phone's media services reset: every audio object is gone, the
    /// input's too. Drops the input and builds it again if sending or the
    /// level still wants it, after the stops queued so far.
    func servicesReset()
    /// `lost` runs, on any thread, with the words for the operator when
    /// sending stops by itself (the input could not be built again after
    /// a route change). The key must end then: nothing is being sent.
    func onLost(_ lost: @escaping @Sendable (String) -> Void)
}
