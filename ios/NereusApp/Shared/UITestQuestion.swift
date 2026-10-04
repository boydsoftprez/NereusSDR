// NereusSDR for iOS: a debug build's stand-in for the Core's question, for the UI tests of where it appears
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if DEBUG
import Foundation
import NereusLink
import NereusMirror

/// With ``argument`` on a debug build's command line, the app listens for
/// ``signal``, a system-wide notification the UI tests post from their own
/// process, and each time hands the several-devices client the question the
/// Core asks when ANT 1 is chosen on the Tuner Genius while the MacBook
/// listens. The UI tests use it with no Core to show the question over any
/// screen and tap its buttons there. Nothing on screen reaches it, and a
/// release build has none of it.
enum UITestQuestion {
    static let argument = "-NereusAskOnSignal"
    static let signal = "com.boydsoftprez.NereusSDR.uitest.ask"

    @MainActor private static weak var devices: SeveralDevicesClient?
    @MainActor private static var nextId: Int64 = 1

    /// Listens for ``signal`` when the launch arguments carry ``argument``.
    @MainActor
    static func listen(_ devices: SeveralDevicesClient, arguments: [String]) {
        guard arguments.contains(argument) else {
            return
        }
        self.devices = devices
        CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), nil, { _, _, _, _, _ in
            Task { @MainActor in
                UITestQuestion.ask()
            }
        }, signal as CFString, nil, .deliverImmediately)
    }

    /// The Core's words for the tuner's antenna from ANT3 to ANT1, which
    /// the MacBook hears.
    @MainActor
    static func ask() {
        let id = nextId
        nextId += 1
        let slice = LinkJSON.object(["sliceId": .number(1), "letter": .string("B"), "frequencyHz": .number(7_249_000),
                                     "band": .number(3), "mode": .number(0), "adc": .number(0),
                                     "streamIndex": .number(0), "effect": .string("changes")])
        let macBook = LinkJSON.object(["deviceId": .string("uitest-macbook"), "deviceName": .string("MacBook Pro"),
                                       "deviceShortName": .string("MacBook"), "state": .string("listening"),
                                       "holdsTransmit": .bool(false), "slices": .array([slice])])
        devices?.receive(.confirmRequest(LinkMessage.ConfirmRequest(
            id: id, kind: "sharedSetting", reason: SeveralDevices.waitingReason, affected: [macBook],
            expiresInMs: 60_000,
            change: ["label": .string("Tuner antenna"), "from": .string("ANT3"), "to": .string("ANT1")],
            forCommandId: 900 + id)))
    }
}
#endif
