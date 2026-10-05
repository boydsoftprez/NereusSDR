// NereusSDR for iOS: the mic level meter live from this phone's microphone before transmitting, and the Core's reading while keyed
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// JJ, 2026-09-26: the mic level meter moves before transmitting, so Mic
/// Gain can be set by it. A stand-in microphone hands the meter peaks; the
/// meter shows them with the Mic Gain added, sends nothing to the Core,
/// closes the microphone when it leaves the screen, the app goes to the
/// background or the phone locks, and shows the Core's reading while keyed.
@Suite("The mic level before transmitting", .serialized)
@MainActor
struct LiveMicLevelTests {
    /// This test's own lock, background time and idle timer.
    private let platform = TestPlatform()
    /// A meter on its own, fed by subjects in place of the Core.
    @MainActor
    struct Rig {
        let level: LiveMicLevel
        let microphone: FakeMicrophone
        let gain: CurrentValueSubject<Double?, Never>
        let allowed: CurrentValueSubject<Bool, Never>
        let center: NotificationCenter
    }

    private func rig(gain: Double? = 0, allowed: Bool = true) -> Rig {
        let microphone = FakeMicrophone()
        let gainSubject = CurrentValueSubject<Double?, Never>(gain)
        let allowedSubject = CurrentValueSubject<Bool, Never>(allowed)
        let center = NotificationCenter()
        let level = LiveMicLevel(microphone: microphone, gain: gainSubject.eraseToAnyPublisher(),
                                 allowed: allowedSubject.eraseToAnyPublisher(), notificationCenter: center)
        return Rig(level: level, microphone: microphone, gain: gainSubject, allowed: allowedSubject, center: center)
    }

    // MARK: The level

    @Test("a peak reads in dB full scale; silence and nonsense read as the floor")
    func decibels() {
        #expect(LiveMicLevel.decibels(peak: 1) == 0)
        #expect(Self.near(LiveMicLevel.decibels(peak: 0.1), -20))
        #expect(LiveMicLevel.decibels(peak: 0) == LiveMicLevel.silenceDb)
        #expect(LiveMicLevel.decibels(peak: .nan) == LiveMicLevel.silenceDb)
        #expect(LiveMicLevel.decibels(peak: -1) == LiveMicLevel.silenceDb)
        // Held to the meter's scale, -40 to +10.
        #expect(LiveMicLevel.shown(peakDb: -20, gainDb: 6) == -14)
        #expect(LiveMicLevel.shown(peakDb: -80, gainDb: 0) == -40)
        #expect(LiveMicLevel.shown(peakDb: -3, gainDb: 20) == 10)
    }

    @Test("the meter moves with the microphone while on screen, and nothing is sent")
    func movesWithTheMicrophone() async {
        let rig = rig()
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.microphone.isMetering)
        #expect(rig.level.listening)
        #expect(rig.level.levelDb == LinearGauge.Scale.micLevel.min, "the floor until the first sound")

        rig.microphone.speak(peak: 0.1)
        #expect(await settle { Self.near(rig.level.levelDb, -20) })
        rig.microphone.speak(peak: 0.5)
        #expect(await settle { Self.near(rig.level.levelDb, -6.0206) })
        rig.microphone.speak(peak: 0.01)
        #expect(await settle { Self.near(rig.level.levelDb, -40) })
        // The level alone: sending never started.
        #expect(rig.microphone.starts == 0)
        #expect(!rig.microphone.isRunning)
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    /// fix-tx concern 5 (M3): the microphone's level stopping by itself
    /// (the input could not be built again after the sound moved) tells
    /// the meter, so it shows no level rather than the last peak, frozen.
    @Test("a level the microphone drops by itself shows no level, not the last peak frozen")
    func aLostLevelShowsNoLevel() async {
        let rig = rig()
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        rig.microphone.speak(peak: 0.1)
        #expect(await settle { Self.near(rig.level.levelDb, -20) })
        #expect(rig.level.listening)

        #expect(rig.microphone.loseLevel())
        #expect(await settle { !rig.level.listening })
        #expect(rig.level.levelDb == LinearGauge.Scale.micLevel.min)
        #expect(rig.level.reason == nil)
        // Opening the meter again starts the level again.
        let starts = rig.microphone.levelStarts
        rig.level.hide(viewer)
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.microphone.levelStarts == starts + 1)
        #expect(rig.level.listening)
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    @Test("a change of Mic Gain moves the level by as many dB")
    func micGainShiftsTheLevel() async {
        let rig = rig(gain: nil)
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        rig.microphone.speak(peak: 0.1)
        #expect(await settle { Self.near(rig.level.levelDb, -20) }, "no Mic Gain from the Core reads as 0 dB")
        rig.gain.send(6)
        #expect(Self.near(rig.level.levelDb, -14))
        rig.gain.send(-10)
        #expect(Self.near(rig.level.levelDb, -30))
        rig.gain.send(0)
        #expect(Self.near(rig.level.levelDb, -20))
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    @Test("the microphone opens only while a meter is on screen")
    func closesWhenThePanelCloses() async {
        let rig = rig()
        #expect(!rig.microphone.isMetering)
        let panel = UUID()
        let section = UUID()
        rig.level.show(panel)
        rig.level.show(section)
        await rig.level.settle()
        #expect(rig.microphone.isMetering)
        #expect(rig.microphone.levelStarts == 1, "one start for two meters")
        rig.level.hide(panel)
        await rig.level.settle()
        #expect(rig.microphone.isMetering, "the other meter is still up")
        rig.microphone.speak(peak: 0.1)
        #expect(await settle { Self.near(rig.level.levelDb, -20) })
        rig.level.hide(section)
        #expect(!rig.level.listening)
        #expect(rig.level.levelDb == LinearGauge.Scale.micLevel.min)
        await rig.level.settle()
        #expect(!rig.microphone.isMetering)
        // A peak already on its way when it closed changes nothing: the
        // check follows the main queue's run of it.
        rig.microphone.speak(peak: 1)
        await MainQueue.drained()
        #expect(rig.level.levelDb == LinearGauge.Scale.micLevel.min)
    }

    @Test("the microphone closes in the background and opens again in the foreground")
    func closesInTheBackground() async {
        let rig = rig()
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.microphone.isMetering)
        rig.center.post(name: UIApplication.didEnterBackgroundNotification, object: nil)
        await rig.level.settle()
        #expect(!rig.microphone.isMetering)
        #expect(!rig.level.listening)
        rig.center.post(name: UIApplication.willEnterForegroundNotification, object: nil)
        await rig.level.settle()
        #expect(rig.microphone.isMetering)
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    @Test("the microphone closes when the phone locks and opens again once unlocked")
    func closesWhenThePhoneLocks() async {
        let rig = rig()
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.microphone.isMetering)
        rig.center.post(name: UIApplication.protectedDataWillBecomeUnavailableNotification, object: nil)
        await rig.level.settle()
        #expect(!rig.microphone.isMetering)
        rig.center.post(name: UIApplication.protectedDataDidBecomeAvailableNotification, object: nil)
        await rig.level.settle()
        #expect(rig.microphone.isMetering)
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    @Test("a call or Siri closes the microphone until a meter opens again")
    func closesOnAnInterruption() async {
        let rig = rig()
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        rig.level.interrupted()
        await rig.level.settle()
        #expect(!rig.microphone.isMetering)
        rig.level.hide(viewer)
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.microphone.isMetering)
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    @Test("a phone that may not transmit does not open the microphone")
    func onlyWhileThisPhoneMayTransmit() async {
        let rig = rig(allowed: false)
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(!rig.microphone.isMetering)
        #expect(rig.microphone.levelStarts == 0)
        rig.allowed.send(true)
        await rig.level.settle()
        #expect(rig.microphone.isMetering)
        rig.allowed.send(false)
        await rig.level.settle()
        #expect(!rig.microphone.isMetering)
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    @Test("without the microphone permission the meter says why, in the existing words")
    func notAllowed() async {
        let rig = rig()
        rig.microphone.levelResult = .failed(reason: MicCapture.notAllowedText)
        let viewer = UUID()
        rig.level.show(viewer)
        await rig.level.settle()
        #expect(rig.level.reason == MicCapture.notAllowedText)
        #expect(!rig.level.listening)
        #expect(!rig.microphone.isMetering)
        // Allowed in Settings, then back: it tries again.
        rig.microphone.levelResult = .started
        rig.center.post(name: UIApplication.didEnterBackgroundNotification, object: nil)
        rig.center.post(name: UIApplication.willEnterForegroundNotification, object: nil)
        await rig.level.settle()
        #expect(rig.level.reason == nil)
        #expect(rig.level.listening)
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    @Test("the words under the meter are plain and promise nothing")
    func words() {
        for text in [LiveMicLevel.liveText, LiveMicLevel.liveInTxPanelText] {
            #expect(text.hasPrefix("Live from this phone's microphone."))
            #expect(text.contains("so the peaks reach the yellow"))
            #expect(!text.lowercased().contains("yet"))
            #expect(!text.contains("\u{2014}"))
        }
        // The TX panel's Mic Gain is the row right under its meter; the
        // Modes tab's words stay its own.
        #expect(LiveMicLevel.liveInTxPanelText
            == "Live from this phone's microphone. Speak normally and set Mic Gain below so the peaks reach the yellow.")
        #expect(!LiveMicLevel.liveInTxPanelText.contains("Modes tab"))
        #expect(LiveMicLevel.liveText
            == "Live from this phone's microphone. Speak normally and set Mic Gain so the peaks reach the yellow.")
    }

    @Test("the meter shows the live level unkeyed and the Core's reading while keyed")
    func whichReading() async {
        let rig = rig()
        let viewer = UUID()
        #expect(MicLevelGauge.value(coreReading: false, coreDb: -400, live: rig.level) == -400,
                "not listening: the Core's reading, as before")
        rig.level.show(viewer)
        await rig.level.settle()
        rig.microphone.speak(peak: 0.1)
        #expect(await settle { Self.near(rig.level.levelDb, -20) })
        #expect(Self.near(MicLevelGauge.value(coreReading: false, coreDb: -400, live: rig.level), -20))
        #expect(MicLevelGauge.value(coreReading: true, coreDb: -14, live: rig.level) == -14)
        rig.level.hide(viewer)
        await rig.level.settle()
    }

    // MARK: Against a fake Core

    @Test("connected: the level follows the Core's Mic Gain, sends nothing, and keyed shows the Core's reading")
    func againstAFakeCore() async throws {
        let (model, station, microphone) = try await connected()
        let transmit = model.main.transmit
        let level = model.main.micLevel
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 27, name: "micGainDb", value: .i64(-6)),
        ])))
        #expect(await settle { model.main.modes.micGainDb == -6 })
        let sentBefore = Self.sent(station)
        let viewer = UUID()
        level.show(viewer)
        await level.settle()
        #expect(microphone.isMetering)
        microphone.speak(peak: 0.1)
        #expect(await settle { Self.near(level.levelDb, -26) })
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 27, name: "micGainDb", value: .i64(4)),
        ])))
        #expect(await settle { Self.near(level.levelDb, -16) })
        for _ in 0..<20 {
            microphone.speak(peak: 0.3)
        }
        // The peak callbacks already queued on the main dispatch queue have run.
        await MainQueue.drained()
        // Nothing reached the Core: no write, no command, no media request,
        // no microphone packet; sending never started.
        #expect(Self.sent(station) == sentBefore)
        #expect(station.mediaPeers.last?.microphonePackets.isEmpty ?? true)
        #expect(microphone.starts == 0)
        #expect(!MicLevelGauge.showsCoreReading(transmit))
        #expect(MicLevelGauge.value(coreReading: false, coreDb: transmit.micLevelDb, live: level) == level.levelDb)

        // Keyed: the Core's reading.
        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state.isKeyed })
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
            .init(ordinal: 13, name: "micLevelDb", value: .f64(-14)),
        ]))
        #expect(await settle { transmit.micLevelDb == -14 })
        #expect(MicLevelGauge.showsCoreReading(transmit))
        #expect(MicLevelGauge.value(coreReading: MicLevelGauge.showsCoreReading(transmit), coreDb: transmit.micLevelDb,
                                    live: level) == -14)
        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state == .idle })
        level.hide(viewer)
        await level.settle()
        #expect(!microphone.isMetering)
        await model.disconnect()
    }

    // MARK: Pictures

    @Test("the TX panel opens the microphone, shows the live level unkeyed, and closes it when it closes")
    func txPanelShot() async throws {
        let (model, station, microphone) = try await connected()
        try await MainScreenShotTests.fill(station)
        await TransmitScreenTests.fillTransmit(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 27, name: "micGainDb", value: .i64(3)),
        ])))
        #expect(await settle { model.main.slices.entries.count == 2 && model.main.modes.micGainDb == 3 })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        #expect(!microphone.isMetering)

        let size = CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let host = UIHostingController(rootView: LiveMicShotRoot(model: model).preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        #expect(await settle { microphone.isMetering }, "the TX panel on screen opens the microphone")
        #expect(await settle { model.main.micLevel.listening })
        // Speech peaking at -8 dB full scale, with Mic Gain at 3 dB: -5 on the meter.
        microphone.speak(peak: 0.4)
        #expect(await settle { abs(model.main.micLevel.levelDb - -4.96) < 0.01 })
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        microphone.speak(peak: 0.4)
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        await MainQueue.drained()
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("tx-panel-live-mic-level-upright.png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        #expect(microphone.starts == 0, "nothing keyed, nothing sent")

        // The panel closes: the microphone closes.
        window.rootViewController = nil
        window.isHidden = true
        #expect(await settle { !microphone.isMetering }, "closing the TX panel closes the microphone")
        await model.disconnect()
    }

    // MARK: Inside

    /// Equal to within a thousandth of a dB: a Float peak is not exact.
    nonisolated static func near(_ value: Double, _ expected: Double) -> Bool {
        abs(value - expected) < 0.001
    }

    /// Everything the app sent the Core that is not the link's own upkeep.
    nonisolated static func sent(_ station: FakeStation) -> Int {
        station.messages.filter { message in
            switch message {
            case .propertyWrite, .mediaControl:
                return true
            case .commandInvoke:
                return true
            default:
                return false
            }
        }.count
    }

    private func connected() async throws -> (AppModel, FakeStation, FakeMicrophone) {
        let suite = "LiveMicLevelTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let microphone = FakeMicrophone()
        let station = try FakeStation(additions: [.remoteTx, .wideband])
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             mediaPeerFactory: station.mediaPeerFactory,
                             microphone: { _ in microphone },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        await TransmitScreenTests.fillTransmit(station)
        // Finish the initial media requests before a caller measures what the meter sends.
        #expect(await settle(seconds: 30) {
            let startupOps = Set(station.messages.compactMap { message -> String? in
                guard case .mediaControl(let control) = message,
                      case .string(let op)? = control.payload["op"] else {
                    return nil
                }
                return op
            })
            return model.main.transmit.permitted && model.connection == .connected
                && startupOps.isSuperset(of: ["start", "audio", "subscribe", "keyframe"])
        })
        return (model, station, microphone)
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }
}

/// The app's root as `RootView` lays it out, with the TX panel open.
private struct LiveMicShotRoot: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(spacing: 0) {
            MainScreen(app: model, main: model.main, txPanelOpen: true)
            TabBar(selection: .constant(.panadapter), sideways: false)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
