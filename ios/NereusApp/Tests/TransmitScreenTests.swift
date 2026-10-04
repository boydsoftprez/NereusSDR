// NereusSDR for iOS: transmit on the main screen against a fake Core: the PTT, its keepalive, refusals, stops and the keyed view
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-11, R-IOS-13, R-IOS-17: the real app keying a fake Core, which
/// reaches no radio. What the PTT sends and never sends, the screen kept
/// awake while keyed, the Core's refusals and stops on the band, and
/// transmit held by another device. With `NEREUS_MAIN_SHOTS` set to a
/// directory (through `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the keyed screens
/// are written there for comparing with `01-on-the-band.jpg`,
/// `02-sideways.jpg` and `17-transmit-time-out.jpg`.
@Suite("Transmit on screen", .serialized)
@MainActor
struct TransmitScreenTests {
    /// This test's own lock, background time and idle timer.
    private let platform = TestPlatform()
    static let otherDeviceId = "macbook-device-id"
    static let thisDeviceId = "this-phone-device-id"
    static let timeOutText = "Transmit stopped after 3:00, the Core's time-out for phones and tablets."

    // MARK: The PTT

    @Test("one tap keys with tx.key three times, the keepalive runs, a second tap unkeys and it stops")
    func pttKeysAndUnkeys() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        let keys = station.messages.compactMap(Self.invoke).filter { $0.verb == "tx.key" }
        #expect(keys.count == 3)
        #expect(Set(keys.map(\.id)).count == 1)
        #expect(keys.first?.args == [LinkMessage.PropertyEntry(name: "trigger", value: .utf8("screen"))])
        #expect(station.keyed)
        // The screen stays awake while keyed.
        #expect(platform.screenAwake)
        #expect(await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == "tx.keepalive" } != nil)

        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        let unkeys = station.messages.compactMap(Self.invoke).filter { $0.verb == "tx.unkey" }
        #expect(unkeys.count == 3)
        #expect(unkeys.first?.args == [LinkMessage.PropertyEntry(name: "epoch", value: .i64(1))])
        #expect(!station.keyed)
        #expect(!platform.screenAwake)
        // The controller has retired its keepalive timer after unkeying.
        #expect(await settle(seconds: 30) { !transmit.ptt.keepaliveRunning })
        await model.disconnect()
    }

    /// The Core's words for a key refused without this device's
    /// microphone, as its session fixture sends them; nil outside the
    /// checkout.
    static func microphoneRefusalWords() -> String? {
        let file = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
            .appendingPathComponent("tests/data/link/v1/sessions/key-without-microphone.json")
        guard let data = try? Data(contentsOf: file),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return nil
        }
        for step in object["steps"] as? [[String: Any]] ?? [] {
            guard let message = step["message"] as? [String: Any],
                  message["type"] as? String == "command.result",
                  let values = message["values"] as? [[String: Any]],
                  values.contains(where: {
                      $0["name"] as? String == "refusalCode"
                          && $0["value"] as? String == PttController.microphoneRefusalCode
                  }),
                  let reason = message["reason"] as? String else {
                continue
            }
            return reason
        }
        return nil
    }

    @Test("the disabled VOX's words are the Core's own for a key without the microphone")
    func voxWordsMatchTheCores() throws {
        let words = try #require(Self.microphoneRefusalWords())
        #expect(TransmitModel.voxNeedsMicrophone == words)
    }

    @Test("a refused key shows the Core's words and keys nothing")
    func refusedKey() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        let words = try #require(Self.microphoneRefusalWords())
        station.refuseNext("tx.key", reason: words)
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .refused(TxRefusalInfo(reason: words)) })
        #expect(TxNoticeCard.content(ptt: transmit.ptt, heldNote: nil) == .refusal(TxRefusalInfo(reason: words)))
        #expect(!transmit.ptt.transmitting)
        #expect(!station.keyed)
        // A keepalive may go while the key is on its way (a slow answer
        // can take longer than one 100 ms beat); none goes once refused.
        #expect(!transmit.ptt.keepaliveRunning)
        await model.disconnect()
    }

    /// The Core's words for `noTransmitSlice` (link section 18.3).
    static let noTransmitSliceWords = "There is no slice to transmit on. Add a slice first."

    /// The keying rule (Core 5cdb9a1ab): a key whose transmit flag would land
    /// on another device's slice, with no slice of this phone's to move it
    /// to, is refused with `noTransmitSlice`, and the Core sends no transmit
    /// change. PTT stays unkeyed and shows the Core's words as sent.
    @Test("a key refused for no transmit slice stays unkeyed, shows the Core's words and sends no release")
    func refusedForNoTransmitSlice() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        station.refuseNext("tx.key", reason: Self.noTransmitSliceWords, code: "noTransmitSlice")
        transmit.tapPtt()
        let refusal = TxRefusalInfo(reason: Self.noTransmitSliceWords, code: "noTransmitSlice")
        #expect(await settle(seconds: 30) { transmit.ptt.state == .refused(refusal) })
        #expect(TxNoticeCard.content(ptt: transmit.ptt, heldNote: nil) == .refusal(refusal))
        #expect(!transmit.ptt.transmitting && !transmit.transmittingHere)
        #expect(!station.keyed)
        #expect(await settle(seconds: 30) { !transmit.ptt.keepaliveRunning })
        // A round trip after everything the refusal set off: an unkey sent
        // on its way would be at the station before the barrier is.
        await Self.barrier(model, station)
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb == "tx.unkey" })
        #expect(transmit.ptt.state == .refused(refusal))
        await model.disconnect()
    }

    @Test("the Core's time-out stop ends the key without an unkey, says why, and PTT reads Tap")
    func timeOutStop() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        station.stopKeyOnItsOwn()
        await station.deliver(Self.txStateDelta([
            .init(ordinal: 0, name: "keyed", value: .bool(false)),
            .init(ordinal: 15, name: "stopReason", value: .utf8("timeOut")),
            .init(ordinal: 16, name: "stopText", value: .utf8(Self.timeOutText)),
            .init(ordinal: 17, name: "stopSerial", value: .i64(1)),
            .init(ordinal: 28, name: "stopEpoch", value: .i64(1)),
        ]))
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(transmit.ptt.stop?.text == Self.timeOutText)
        #expect(PttButton.words(transmit.ptt, clock: "0:00") == PttButton.Words(label: "PTT", sub: "Tap"))
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb == "tx.unkey" })
        await model.disconnect()
    }

    @Test("with transmit held by another device a tap sends nothing to key, and PTT names the holder")
    func heldElsewhere() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        // The fake Core refuses this phone's releases while the MacBook holds transmit.
        station.otherDeviceTakes("MacBook Pro")
        await station.deliver(Self.txStateDelta(Self.holder(Self.otherDeviceId, short: "MacBook", keyed: true)))
        #expect(await settle(seconds: 30) {
            transmit.ptt.state == .heldElsewhere(device: "MacBook", onAir: true)
        })
        #expect(PttButton.words(transmit.ptt, clock: "") == PttButton.Words(label: "TX", sub: "MacBook"))
        #expect(PttButton.look(transmit.ptt) == .elsewhere(onAir: true))
        transmit.tapPtt()
        transmit.toggleTune()
        await transmit.lastTuneOperationForTesting?.value
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb.hasPrefix("tx.") })
        // Transmit comes free: PTT reads Tap and one tap keys.
        station.otherDeviceReleases()
        await station.deliver(Self.txStateDelta(Self.holder("", short: "", keyed: false)))
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        transmit.tapPtt()
        #expect(await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == "tx.key" } != nil)
        await model.disconnect()
    }

    @Test("a PTT tap during TUNE ends TUNE and never keys: the Core keeps one key per device")
    func pttDuringTune() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        transmit.toggleTune()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed && transmit.ptt.tuning })
        #expect(station.keyed)
        #expect(PttButton.words(transmit.ptt, clock: "0:01") == PttButton.Words(label: "TUNE", sub: "0:01"))
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!station.keyed)
        let verbs = station.messages.compactMap(Self.invoke).map(\.verb).filter { $0 != "tx.keepalive" }
        #expect(verbs == ["tx.tune", "tx.tune", "tx.tune", "tx.tune", "tx.tune", "tx.tune"])
        await model.disconnect()
    }

    @Test("locking the phone while keyed unkeys")
    func lockWhileKeyed() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        platform.lock()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!station.keyed)
        #expect(station.messages.compactMap(Self.invoke).contains { $0.verb == "tx.unkey" })
        await model.disconnect()
    }

    @Test("on a Core without remote transmit PTT says why and sends nothing")
    func olderCore() async throws {
        let suite = "TransmitScreenTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle(seconds: 30) { model.connection == .connected })
        let transmit = model.main.transmit
        #expect(!transmit.offered)
        transmit.tapPtt()
        #expect(transmit.heldNote == TransmitModel.noRemoteTransmitText)
        #expect(TransmitModel.noRemoteTransmitText
                == "This Core can't take transmit from a phone. Updating the Core may help.")
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb.hasPrefix("tx.") })
        await model.disconnect()
    }

    @Test("the link lost while keyed says the Core stops on its own, and back on the air PTT reads Tap")
    func linkLostWhileKeyed() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        await station.dropLink()
        #expect(!station.keyed)
        #expect(await settle(seconds: 30) { transmit.ptt.state == .linkLost })
        // The phone itself: no keepalive, not transmitting, PTT ready.
        #expect(!transmit.ptt.keepaliveRunning)
        #expect(!transmit.ptt.transmitting)
        #expect(!transmit.transmittingHere)
        #expect(PttButton.look(transmit.ptt) == .ready)
        #expect(TxNoticeCard.content(ptt: transmit.ptt, heldNote: nil) == .linkLost)
        #expect(!platform.screenAwake)
        #expect(await settle(seconds: 30) { model.connection == .connected })
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(PttButton.words(transmit.ptt, clock: "") == PttButton.Words(label: "PTT", sub: "Tap"))
        #expect(!transmit.ptt.keepaliveRunning)
        // Nothing keyed again by itself.
        #expect(station.messages.compactMap(Self.invoke).filter { $0.verb == "tx.key" }.count == 3)
        await model.disconnect()
    }

    @Test("the TX panel's settings come from the Core, and a verb the Core lacks is greyed with its reason")
    func txPanel() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        await Self.fillTransmit(station)
        // The amplifier, the settings and the tuner arrive as separate objects.
        #expect(await settle(seconds: 30) { transmit.amp != nil && transmit.rfPower == 100 && transmit.tuner != nil })
        #expect(transmit.amp == TransmitModel.Accessory(kind: .powerGenius, name: "Power Genius XL", operate: true))
        #expect(transmit.tuner?.kind == .tunerGenius)
        #expect(transmit.tunePower == 10)
        // This fake's Core is at remoteTxVersion 1: the tuner's TUNE is
        // greyed with the reason and sends nothing.
        #expect(transmit.tunerTuneReason == TransmitModel.tunerTuneOlderCoreText)
        #expect(TransmitModel.tunerTuneOlderCoreText
                == "This Core can't start the tuner's tune from a phone. Updating the Core may help.")
        transmit.toggleTunerTune()
        await transmit.lastTuneOperationForTesting?.value
        // This fake's Core predates the amp and tuner verbs (remotePgxlControlVersion
        // and remoteTgxlControlVersion 0): OPERATE is greyed with the reason and
        // sends nothing; its tune power verb (transmitSettingsVersion 8) is there.
        #expect(!transmit.ampOperateAvailable)
        #expect(!transmit.tunerOperateAvailable)
        #expect(transmit.tunePowerAvailable)
        transmit.setAmpOperate(false)
        #expect(transmit.note == TransmitModel.ampOlderCoreText)
        transmit.setTunePower(20)
        let tunePower = await station.waitForMessage(within: .seconds(30)) {
            Self.invoke($0)?.verb == "setTunePowerForTxBand"
        }
        #expect(tunePower.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "watts", value: .i64(20))])
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb == "setPgxlOperate" })
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb == "tx.tunerTune" })
        await model.disconnect()
    }

    @Test("at remoteTxVersion 2 the tuner's TUNE keys tx.tunerTune through the PTT's queue, and a second tap ends it")
    func tunerTune() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .tunerTune])
        let transmit = model.main.transmit
        await Self.fillTransmit(station)
        #expect(await settle(seconds: 30) { transmit.tuner != nil && transmit.tunerTuneReason == nil })
        transmit.toggleTunerTune()
        await transmit.lastTuneOperationForTesting?.value
        #expect(await settle(seconds: 30) { transmit.ptt.tunerTuning && transmit.ptt.state.isKeyed })
        #expect(station.keyedVerb == "tx.tunerTune")
        let on = station.messages.compactMap(Self.invoke).first { $0.verb == "tx.tunerTune" }
        #expect(on?.args == [LinkMessage.PropertyEntry(name: "on", value: .bool(true))])
        #expect(PttButton.words(transmit.ptt, clock: "0:01") == PttButton.Words(label: "TUNE", sub: "0:01"))
        // On the air for this phone's own tune: TUNE stays pressable, to end it.
        #expect(transmit.tunerTuneReason == nil)
        transmit.toggleTunerTune()
        await transmit.lastTuneOperationForTesting?.value
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!station.keyed)
        let verbs = station.messages.compactMap(Self.invoke).filter { $0.verb.hasPrefix("tx.") && $0.verb != "tx.keepalive" }
        #expect(verbs.map(\.verb) == ["tx.tunerTune", "tx.tunerTune", "tx.tunerTune",
                                      "tx.tunerTune", "tx.tunerTune", "tx.tunerTune"])
        #expect(verbs.last?.args == [LinkMessage.PropertyEntry(name: "on", value: .bool(false))])
        // While the PTT's key is on, the radio is on the air: the tuner's
        // TUNE is greyed with the Core's words and starts nothing.
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.pttKeyed && transmit.ptt.state.isKeyed })
        #expect(transmit.tunerTuneReason == TransmitModel.onAirText)
        transmit.toggleTunerTune()
        await transmit.lastTuneOperationForTesting?.value
        #expect(station.keyedVerb == "tx.key")
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        await model.disconnect()
    }

    @Test("the Core's tuneEnded notice ends the tuner's tune with its own off and is shown in the Core's words")
    func tunerTuneEnded() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .tunerTune])
        let transmit = model.main.transmit
        transmit.toggleTunerTune()
        await transmit.lastTuneOperationForTesting?.value
        #expect(await settle(seconds: 30) { transmit.ptt.tunerTuning && transmit.ptt.state.isKeyed })
        station.stopKeyOnItsOwn()
        let words = "The amplifier did not go to standby for tuning. Put it in standby or disconnect it in Setup, "
            + "then tune again."
        await station.deliver(.notice(LinkMessage.Notice(id: 41, kind: "tuneEnded", reason: words, secondsAgo: 0,
                                                                  takeBack: false)))
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!transmit.ptt.keepaliveRunning)
        #expect(model.devices.notices.last?.notice.reason == words)
        // Ended through its own off (three copies), which ends nothing more at the Core.
        #expect(station.messages.compactMap(Self.invoke).filter { $0.verb == "tx.tunerTune" }.count == 6)
        await model.disconnect()
    }

    @Test("2-Tone lit by another's two-tone test is greyed with the Core's words and starts nothing; free, a tap keys it")
    func twoToneLitElsewhere() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        // The fake's Core has its PureSignal object from the snapshot on.
        await station.deliver(.delta(LinkMessage.Delta(key: "pureSignal", properties: [
            .init(ordinal: 2, name: "twoToneOn", value: .bool(true)),
        ])))
        #expect(await settle(seconds: 30) { transmit.twoToneOn })
        #expect(!transmit.ptt.twoTone)
        #expect(transmit.twoToneReason == TransmitModel.onAirText)
        transmit.toggleTwoTone()
        await transmit.lastTuneOperationForTesting?.value
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb == "tx.twoTone" },
                "a tap on another's lit 2-Tone sends nothing")
        #expect(!station.keyed)

        await station.deliver(.delta(LinkMessage.Delta(key: "pureSignal", properties: [
            .init(ordinal: 2, name: "twoToneOn", value: .bool(false)),
        ])))
        #expect(await settle(seconds: 30) { !transmit.twoToneOn })
        #expect(transmit.twoToneReason == nil)
        transmit.toggleTwoTone()
        await transmit.lastTuneOperationForTesting?.value
        #expect(await settle(seconds: 30) { transmit.ptt.twoTone && transmit.ptt.state.isKeyed })
        // This phone's own two-tone on the air: 2-Tone stays pressable, to end it.
        #expect(transmit.twoToneReason == nil)
        transmit.toggleTwoTone()
        await transmit.lastTuneOperationForTesting?.value
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!station.keyed)
        await model.disconnect()
    }

    @Test("the radio's transmit inhibit greys TUNE and MOX with the Core's reason, or the inhibit input's words")
    func transmitInhibit() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        #expect(transmit.inhibitReason == nil)
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "radio", className: "RadioModel", properties: [
            .init(ordinal: 28, name: "txInhibitReason", value: .utf8("I/O Board: Fault Code 5")),
            .init(ordinal: 20, name: "txInhibited", value: .bool(true)),
        ])))
        #expect(await settle(seconds: 30) { transmit.inhibitReason == "I/O Board: Fault Code 5" })
        // The External TX Inhibit line sends no words: the phone shows the Core's refusal words.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 28, name: "txInhibitReason", value: .utf8("")),
        ])))
        #expect(await settle(seconds: 30) { transmit.inhibitReason == TransmitModel.inhibitInputText })
        #expect(TransmitModel.inhibitInputText == "The radio's transmit inhibit input is holding transmit off.")
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 20, name: "txInhibited", value: .bool(false)),
        ])))
        #expect(await settle(seconds: 30) { transmit.inhibitReason == nil })
        await model.disconnect()
    }

    @Test("the TX filter sits on the slice's side of the carrier")
    func txFilterSide() {
        let lower = TransmitModel.txFilter(carrierHz: 7_236_400, passband: 7_233_400...7_236_300, lowHz: 100,
                                           highHz: 2900)
        #expect(lower == 7_233_500...7_236_300)
        let upper = TransmitModel.txFilter(carrierHz: 14_200_000, passband: 14_200_100...14_203_000, lowHz: 100,
                                           highHz: 2900)
        #expect(upper == 14_200_100...14_202_900)
    }

    @Test("the Sharing chip's note names who keeps a full band, in the board's words")
    func sharingNote() {
        #expect(SharingChip.note(reason: .sharedConnection, holder: "MacBook")
                == "The Core's connection is full. MacBook has transmit, so it keeps its full band and sound.")
        #expect(SharingChip.note(reason: .sharedProcessing, holder: nil) == "The Core is busy. The devices share it.")
        #expect(SharingChip.note(reason: .coreBusy, holder: nil) == "The Core is busy. The devices share it.")
    }

    // MARK: Pictures

    @Test("listening and keyed, the TX panel, transmit held elsewhere and the time-out, upright and sideways")
    func transmitShots() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .accessoryOperate])
        let transmit = model.main.transmit
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        // No amplifier or tuner at the Core: their rows greyed, not set up (I14).
        try await shoot("tx-panel-no-accessories-upright", sideways: false, txPanelOpen: true, model: model)
        await Self.fillTransmit(station)
        #expect(await settle(seconds: 30) { transmit.amp != nil })

        try await shoot("tx-listening-upright", sideways: false, model: model)
        try await shoot("tx-listening-sideways", sideways: true, model: model)
        try await shoot("tx-panel-upright", sideways: false, txPanelOpen: true, model: model)
        try await shoot("tx-panel-sideways", sideways: true, txPanelOpen: true, model: model)
        // A typed TX filter edge: the number pad over the TX panel (C1).
        transmit.openTxFilterPad(low: false)
        try await shoot("tx-panel-filter-pad-upright", sideways: false, txPanelOpen: true, model: model)
        transmit.closePad()

        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        await station.deliver(Self.txStateDelta([
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
            .init(ordinal: 9, name: "forwardPowerWatts", value: .f64(67)),
            .init(ordinal: 11, name: "swr", value: .f64(1.3)),
            .init(ordinal: 13, name: "micLevelDb", value: .f64(-14)),
        ]))
        #expect(await settle(seconds: 30) { transmit.forwardWatts == 67 && transmit.txFilterHz != nil })
        try await shoot("tx-keyed-upright", sideways: false, model: model)
        try await shoot("tx-keyed-sideways", sideways: true, model: model)
        try await shoot("tx-keyed-panel-upright", sideways: false, txPanelOpen: true, model: model)

        await station.deliver(Self.txStateDelta([
            .init(ordinal: 0, name: "keyed", value: .bool(false)),
            .init(ordinal: 9, name: "forwardPowerWatts", value: .f64(0)),
            .init(ordinal: 15, name: "stopReason", value: .utf8("timeOut")),
            .init(ordinal: 16, name: "stopText", value: .utf8(Self.timeOutText)),
            .init(ordinal: 17, name: "stopSerial", value: .i64(1)),
            .init(ordinal: 28, name: "stopEpoch", value: .i64(1)),
        ]))
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle && transmit.ptt.stop != nil })
        try await shoot("tx-time-out-upright", sideways: false, model: model)

        transmit.dismissNotice()
        await station.deliver(Self.txStateDelta(Self.holder(Self.otherDeviceId, short: "MacBook", keyed: false)))
        #expect(await settle(seconds: 30) { transmit.ptt.state == .heldElsewhere(device: "MacBook", onAir: false) })
        try await shoot("tx-held-elsewhere-upright", sideways: false, model: model)
        await station.deliver(Self.txStateDelta(Self.holder(Self.otherDeviceId, short: "MacBook", keyed: true)))
        #expect(await settle(seconds: 30) { transmit.ptt.state == .heldElsewhere(device: "MacBook", onAir: true) })
        try await shoot("tx-held-elsewhere-on-air-upright", sideways: false, model: model)
        await model.disconnect()
    }

    // MARK: The fake Core

    /// The app connected to a fake Core that lets it transmit, with an
    /// empty transmit state, and this phone's own device id set.
    private func connected(additions: FakeStation.Additions = [.remoteTx]) async throws -> (AppModel, FakeStation) {
        let suite = "TransmitScreenTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = Self.thisDeviceId
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "txState", className: "TransmitState",
                                                                    properties: Self.holder("", short: "", keyed: false)
                                                                        + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                                                                           .init(ordinal: 17, name: "stopSerial",
                                                                                 value: .i64(0))])))
        #expect(await settle(seconds: 30) { model.main.transmit.permitted && model.connection == .connected })
        return (model, station)
    }

    static func holder(_ id: String, short: String, keyed: Bool) -> [LinkMessage.PropertyEntry] {
        [
            .init(ordinal: 0, name: "keyed", value: .bool(keyed)),
            .init(ordinal: 18, name: "holderDeviceId", value: .utf8(id)),
            .init(ordinal: 19, name: "holderName", value: .utf8(short.isEmpty ? "" : short + " Pro")),
            .init(ordinal: 20, name: "holderShortName", value: .utf8(short)),
            .init(ordinal: 21, name: "holderKind", value: .utf8(id.isEmpty ? "" : "computer")),
            .init(ordinal: 22, name: "holderSource", value: .utf8(id.isEmpty ? "" : "device")),
            .init(ordinal: 24, name: "holderEpoch", value: .i64(id.isEmpty ? 2 : 1)),
        ]
    }

    static func txStateDelta(_ properties: [LinkMessage.PropertyEntry]) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "txState", properties: properties))
    }

    /// The board's TX panel: 100 W, tune power 10, PROC on, a Power Genius
    /// operating and a Tuner Genius, and the TX filter 100 to 2900 Hz on
    /// slice A, the transmit slice.
    static func fillTransmit(_ station: FakeStation) async {
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "transmit", className: "TransmitModel",
                                                                    properties: [
            .init(ordinal: 2, name: "power", value: .i64(100)),
            .init(ordinal: 4, name: "filterLow", value: .i64(100)),
            .init(ordinal: 5, name: "filterHigh", value: .i64(2900)),
            .init(ordinal: 23, name: "cpdrOn", value: .bool(true)),
            .init(ordinal: 18, name: "monEnabled", value: .bool(false)),
            .init(ordinal: 97, name: "voxEnabled", value: .bool(false)),
            .init(ordinal: 28, name: "tunePowerForTxBand", value: .i64(10)),
            .init(ordinal: 16, name: "voxThresholdDb", value: .i64(-40)),
            .init(ordinal: 17, name: "voxHangTimeMs", value: .i64(500)),
            .init(ordinal: 25, name: "amCarrierLevel", value: .i64(25)),
            .init(ordinal: 26, name: "dexpEnabled", value: .bool(false)),
            .init(ordinal: 37, name: "activeTxProfile", value: .utf8("Default")),
            .init(ordinal: 38, name: "txProfilesJson", value: .utf8("[\"AM\",\"Default\",\"Default DX\"]")),
        ])))
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "amplifier", className: "AmplifierModel",
                                                                    properties: [
            .init(ordinal: 8, name: "present", value: .bool(true)),
            .init(ordinal: 11, name: "operate", value: .bool(true)),
        ])))
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "tuner", className: "TunerModel",
                                                                    properties: [
            .init(ordinal: 16, name: "isPresent", value: .bool(true)),
            .init(ordinal: 11, name: "isOperate", value: .bool(true)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 12, name: "txSlice", value: .bool(true)),
        ])))
    }

    nonisolated static func invoke(_ message: LinkMessage) -> LinkMessage.CommandInvoke? {
        if case .commandInvoke(let invoke) = message {
            return invoke
        }
        return nil
    }

    /// Everything the app sent before reaches the fake Core: the tasks
    /// the screen started run, the PTT's queue settles, then a fresh
    /// command goes on the same link and the Core receives it.
    static func barrier(_ model: AppModel, _ station: FakeStation) async {
        await Task { @MainActor in }.value
        await model.main.transmit.controller.settle()
        let verb = "test.barrier.\(UUID().uuidString)"
        let barrier = await model.commands.start(verb, arguments: [], copies: 1, timeout: .seconds(1))
        await barrier.sent()
        #expect(await station.waitForMessage { invoke($0)?.verb == verb } != nil)
    }

    static func keepalives(_ station: FakeStation) -> Int {
        station.messages.compactMap(invoke).filter { $0.verb == "tx.keepalive" }.count
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

    private func shoot(_ name: String, sideways: Bool, txPanelOpen: Bool = false, model: AppModel) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = TransmitShotRoot(model: model, txPanelOpen: txPanelOpen).preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        if sideways {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out, with the TX panel open or not.
private struct TransmitShotRoot: View {
    @ObservedObject var model: AppModel
    let txPanelOpen: Bool

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, txPanelOpen: txPanelOpen)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
