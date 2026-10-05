// NereusSDR for iOS: AM, SAM and DSB transmit against a fake Core, and the Core's words for the modes it will not key
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing
import UIKit

/// R-IOS-13, R-IOS-18 (Task 57a, desktop PR #321 parity): the Core lets a
/// remote holder key AM, SAM and DSB, and the phone has no mode rule of its
/// own. In each of them PTT (and MOX) starts the microphone and keys, TUNE
/// keys, AM Carrier stays where it is, and the band's orange TX filter
/// spans both sides of the carrier. The modes the Core will not key are
/// refused by it alone, with the code `bandPlan`, and the phone shows the
/// Core's words as sent.
@Suite("AM, SAM and DSB transmit", .serialized)
@MainActor
struct AmFamilyTransmitTests {
    /// The fake Core's slice A: 7.2364 MHz.
    static let carrierHz = 7_236_400.0

    private let center = NotificationCenter()

    /// The Core's refusals (BandPlanGuard), each with the mode it refuses.
    nonisolated static let refusals: [(mode: String, words: String)] = [
        ("CWL", "CW transmit is not available on this Core"),
        ("CWU", "CW transmit is not available on this Core"),
        ("FM", "FM transmit is not available on this Core"),
        ("DRM", "DRM transmit is not available on this Core"),
        ("SPEC", "Mode not supported for TX"),
    ]

    @Test("PTT starts the microphone and keys, with the TX filter on both sides of the carrier",
          arguments: ["AM", "SAM", "DSB"])
    func pttKeys(_ mode: String) async throws {
        let rig = try await connected(mode: mode)
        let transmit = rig.transmit
        #expect(transmit.amCarrier == 25)
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-0")),
        ])))
        transmit.tapPtt()
        #expect(await settle(seconds: 5) { transmit.ptt.state.isKeyed })
        #expect(rig.microphone.starts == 1)
        #expect(rig.microphone.isRunning)
        #expect(rig.station.keyed)
        // The fake accepts tx.key, then mirrors the Core's separate on-air
        // state. A command acknowledgement alone does not key the band.
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "txState", properties: [
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
        ])))
        #expect(await settle(seconds: 5) { rig.model.main.band.transmit.keyedHere })
        let keys = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }
        #expect(keys.count == 3)
        // The whole passband either side of the carrier, out to the high
        // edge (2900 Hz), whatever the low edge.
        #expect(await settle(seconds: 5) {
            transmit.txFilterHz == (Self.carrierHz - 2900)...(Self.carrierHz + 2900)
        })
        // Core holds the pan from the key rise. A local PTT must not draw its
        // filter on the active slice after that slice is moved to another pan.
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-1")),
        ])))
        #expect(await settle(seconds: 5) { transmit.txFilterHz == nil })
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-0")),
        ])))
        #expect(await settle(seconds: 5) { transmit.txFilterHz != nil })

        transmit.tapPtt()
        #expect(await settle(seconds: 5) { transmit.ptt.state == .idle })
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "txState", properties: [
            .init(ordinal: 0, name: "keyed", value: .bool(false)),
        ])))
        #expect(!rig.station.keyed)
        #expect(!rig.microphone.isRunning)
        await rig.model.disconnect()
    }

    @Test("TUNE keys", arguments: ["AM", "SAM", "DSB"])
    func tuneKeys(_ mode: String) async throws {
        let rig = try await connected(mode: mode)
        let transmit = rig.transmit
        transmit.toggleTune()
        #expect(await settle(seconds: 5) { transmit.ptt.state.isKeyed && transmit.ptt.tuning })
        #expect(rig.station.keyed)
        transmit.toggleTune()
        #expect(await settle(seconds: 5) { transmit.ptt.state == .idle })
        #expect(!rig.station.keyed)
        await rig.model.disconnect()
    }

    @Test("the Core's refusal of a mode shows its words as sent, and nothing keys",
          arguments: refusals.indices)
    func refusedMode(_ index: Int) async throws {
        let (mode, words) = Self.refusals[index]
        let rig = try await connected(mode: mode)
        let transmit = rig.transmit
        rig.station.refuseNext("tx.key", reason: words, code: "bandPlan")
        transmit.tapPtt()
        let refusal = TxRefusalInfo(reason: words, code: "bandPlan")
        #expect(await settle(seconds: 5) { transmit.ptt.state == .refused(refusal) })
        #expect(TxNoticeCard.content(ptt: transmit.ptt, heldNote: nil) == .refusal(refusal))
        #expect(!transmit.ptt.transmitting)
        #expect(!rig.station.keyed)
        #expect(await settle(seconds: 5) { !rig.microphone.isRunning })
        #expect(transmit.txFilterHz == nil)
        await rig.model.disconnect()
    }

    @Test("the TX filter's span on the band: both sides in AM, SAM, DSB, FM and DRM, one side otherwise")
    func txFilterSpan() {
        let passband = (Self.carrierHz - 4000)...(Self.carrierHz + 4000)
        for label in ["AM", "SAM", "DSB", "FM", "DRM"] {
            #expect(TransmitModel.txFilter(carrierHz: Self.carrierHz, passband: passband, lowHz: 100, highHz: 2900,
                                           bothSides: TxFilterMatch.fromZeroModes.contains(label))
                    == (Self.carrierHz - 2900)...(Self.carrierHz + 2900))
        }
        // A low edge of 0, as Match RX leaves it in AM, changes nothing.
        #expect(TransmitModel.txFilter(carrierHz: Self.carrierHz, passband: passband, lowHz: 0, highHz: 4000,
                                       bothSides: true) == (Self.carrierHz - 4000)...(Self.carrierHz + 4000))
        #expect(TransmitModel.txFilter(carrierHz: Self.carrierHz, passband: passband, lowHz: 0, highHz: 0,
                                       bothSides: true) == nil)
        #expect(!TxFilterMatch.fromZeroModes.contains("USB"))
        #expect(!TxFilterMatch.fromZeroModes.contains("SPEC"))
    }

    // MARK: The fake Core

    @MainActor
    struct Rig {
        let model: AppModel
        let station: FakeStation
        let microphone: FakeMicrophone

        var transmit: TransmitModel { model.main.transmit }
    }

    /// The app connected to a fake Core that lets it transmit and carries
    /// its microphone line, with the board's TX panel (the TX filter 100 to
    /// 2900 Hz, AM Carrier 25 percent) and slice A in `mode`, its receive
    /// filter 4 kHz either side of the carrier.
    private func connected(mode: String) async throws -> Rig {
        let suite = "AmFamilyTransmitTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        UIApplication.shared.isIdleTimerDisabled = false
        let microphone = FakeMicrophone()
        let station = try FakeStation(additions: [.remoteTx, .wideband])
        let audio = AudioSessionController(session: FakeAudioSession(), output: FakePlaybackOutput(),
                                           notificationCenter: center)
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             mediaPeerFactory: station.mediaPeerFactory, audio: audio,
                             microphone: { _ in microphone },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        try await MainScreenShotTests.fill(station)
        await TransmitScreenTests.fillTransmit(station)
        #expect(await settle(seconds: 5) { model.main.slices.catalog != nil })
        let id = try #require(model.main.slices.catalog?.modes.first { $0.label == mode }?.id)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 2, name: "dspMode", value: .enumeration(Int64(id))),
            .init(ordinal: 3, name: "filterLow", value: .i64(-4000)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(4000)),
        ])))
        #expect(await settle(seconds: 5) {
            model.main.transmit.permitted && model.connection == .connected && model.main.transmit.microphoneLine
                && model.main.modes.modeLabel == mode
                && model.main.slices.entries.first(where: { $0.slice.id == 0 })?.modeLabel == mode
        })
        await audio.settle()
        return Rig(model: model, station: station, microphone: microphone)
    }

    private func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }
}
