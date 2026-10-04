// NereusSDR for iOS: the transmit panadapter while keyed: the keyed view and the fall, the transmit window, DUP, high SWR
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

/// Task 54f (R-IOS-11, R-IOS-13; desktop PR #317 parity): while the Core's
/// radio is keyed on the band's slice, the band asks for the keyed view
/// (centred on the carrier, XIT included, at the phone's own span, 8 kHz at
/// the first key) with this phone's transmit window, holds receive frames
/// until the Core's transmit display comes, draws with the transmit grid,
/// and at the fall goes back to its receive view. DUP keeps the receiver;
/// a Core below 3 greys it with its reason. A Core that sends no transmit
/// display gets the plain line the desktop shows.
@Suite("Transmit display", .serialized)
@MainActor
struct TransmitDisplayTests {
    static let frequency = 7_236_400.0
    static let xitHz: Int64 = 1_500

    @MainActor
    final class Rig {
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let main: MainScreenModel
        let recorded = Recorded()

        @MainActor
        final class Recorded {
            var sent: [DisplaySubscription] = []
            var settingsSent: [LinkMessage] = []
        }

        var sent: [DisplaySubscription] { recorded.sent }
        var band: BandModel { main.band }

        /// A Core at minor 11 with media and `txStateVersion` 2, sending its
        /// transmit display at `txDisplay` (0 for none), with slice A at
        /// 7.236.400 and XIT 1.5 kHz on.
        init(txDisplay: Int64, txState: Bool = true) throws {
            let mirror = MirrorStore(send: { _ in })
            mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
            mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
                .init(name: "remoteMediaVersion", value: .i64(1)),
                .init(name: "spectrumGrantVersion", value: .i64(1)),
                .init(name: "txDisplayVersion", value: .i64(txDisplay)),
                .init(name: "txStateVersion", value: .i64(txState ? 2 : 0)),
            ])))
            mirror.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
            mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(TransmitDisplayTests.frequency)),
                .init(ordinal: 3, name: "filterLow", value: .i64(100)),
                .init(ordinal: 4, name: "filterHigh", value: .i64(2900)),
                .init(ordinal: 11, name: "active", value: .bool(true)),
                .init(ordinal: 13, name: "sliceIndex", value: .i64(0)),
                .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
                .init(ordinal: 56, name: "xitEnabled", value: .bool(true)),
                .init(ordinal: 57, name: "xitHz", value: .i64(TransmitDisplayTests.xitHz)),
            ])))
            if txState {
                mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "txState", className: "TransmitState",
                                                                    properties: [
                    .init(ordinal: 0, name: "keyed", value: .bool(false)),
                    .init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                    .init(ordinal: 29, name: "highSwr", value: .bool(false)),
                    .init(ordinal: 30, name: "swrWindBackLatched", value: .bool(false)),
                ])))
            }
            mirror.apply(.snapshotComplete)
            let defaults = try #require(UserDefaults(suiteName: "TransmitDisplayTests-\(UUID().uuidString)"))
            self.mirror = mirror
            let recorded = recorded
            settings = SettingsProxyClient(send: { message in
                await MainActor.run { recorded.settingsSent.append(message) }
            })
            settings.handle(.stateChanged(.receivingSnapshot))
            settings.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
            settings.apply(.settingsSnapshot(.init(properties: [])))
            settings.handle(.stateChanged(.ready))
            main = MainScreenModel(mirror: mirror, settings: settings, commands: nil,
                                   operations: BandSubscriber.Operations(
                                       subscribe: { recorded.sent.append($0) }, unsubscribe: { _ in }),
                                   displaySettings: BandDisplaySettingsStore(defaults: defaults))
            _ = main.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        }

        func key(_ keyed: Bool, highSwr: Bool = false, windBack: Bool = false) {
            mirror.apply(.delta(LinkMessage.Delta(key: "txState", properties: [
                .init(ordinal: 0, name: "keyed", value: .bool(keyed)),
                .init(ordinal: 29, name: "highSwr", value: .bool(highSwr)),
                .init(ordinal: 30, name: "swrWindBackLatched", value: .bool(windBack)),
            ])))
        }

        func addSlice(_ id: Int, panKey: String) {
            mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:\(id)", className: "SliceModel", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(TransmitDisplayTests.frequency + 10_000)),
                .init(ordinal: 3, name: "filterLow", value: .i64(100)),
                .init(ordinal: 4, name: "filterHigh", value: .i64(2900)),
                .init(ordinal: 11, name: "active", value: .bool(false)),
                .init(ordinal: 13, name: "sliceIndex", value: .i64(Int64(id))),
                .init(ordinal: 27, name: "panKey", value: .utf8(panKey)),
                .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            ])))
        }

        func transmitOn(_ id: Int, highSwr: Bool = false) {
            mirror.apply(.delta(LinkMessage.Delta(key: "txState", properties: [
                .init(ordinal: 0, name: "keyed", value: .bool(true)),
                .init(ordinal: 3, name: "txSliceId", value: .i64(Int64(id))),
                .init(ordinal: 29, name: "highSwr", value: .bool(highSwr)),
            ])))
        }

        /// One of the Core's contexts for the band's endpoint.
        func context(generation: UInt32, transmit: Bool?, limit: MediaControlEvent.Grant.Limit = .none) {
            let endpoint = band.endpointId ?? 1
            let revision = sent.last?.revision ?? 1
            var payload: [String: LinkJSON] = [
                "op": .string("context"), "connectionId": .string("tests"),
                "endpointId": .number(Double(endpoint)), "revision": .number(Double(revision)),
                "contextGeneration": .number(Double(generation)), "sourceStream": .number(0),
                "sourceCentreHz": .number(TransmitDisplayTests.frequency),
                "sampleRateHz": .number(transmit == true ? 96_000 : 192_000),
                "centreHz": .number(TransmitDisplayTests.frequency),
                "spanHz": .number(transmit == true ? 8_000 : 192_000), "wideCentreHz": .number(0),
                "wideSpanHz": .number(0), "traceSamples": .number(64), "waterfallSamples": .number(64),
                "wideSamples": .number(0), "minDbm": .number(-80), "maxDbm": .number(30), "fps": .number(15),
                "framesPerLine": .number(1), "grantedFftSize": .number(32_768), "grantedTier": .string("wide"),
                "requestedPixels": .number(64), "grantedPixels": .number(64), "limit": .string(limit.rawValue),
            ]
            if let transmit {
                payload["transmit"] = .bool(transmit)
            }
            guard let context = MediaControlDecoder.context(payload, wideband: false, grant: true,
                                                            transmit: transmit != nil) else {
                Issue.record("the context did not decode")
                return
            }
            main.receive(.context(context))
        }

        func frame(generation: UInt32, sequence: UInt32, level: Float) {
            main.receive(.displayFrame(DisplayFrame(endpointId: band.endpointId ?? 1, contextGeneration: generation,
                                                    encoderSequence: sequence,
                                                    producerTimestamp: UInt64(sequence) * 66_000_000,
                                                    isKeyframe: sequence == 0, waterfallAdvance: true,
                                                    minDbm: -80, maxDbm: 30,
                                                    traceDbm: Array(repeating: level, count: 64),
                                                    waterfallDbm: Array(repeating: level, count: 64),
                                                    wideDbm: [])))
        }
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func quiet() async {
        for _ in 0..<3_000 {
            await Task.yield()
        }
    }

    // MARK: The keyed view and the fall

    @Test("a key on the band's slice asks for the keyed view about the carrier, and the fall asks for the receive view")
    func keyedViewAndTheFall() async throws {
        let rig = try Rig(txDisplay: 3)
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.last?.spanHz == 192_000 })
        let receive = try #require(rig.sent.last)
        #expect(receive.centreHz == Self.frequency)
        // Every subscribe on a declaring Core carries this phone's window.
        #expect(receive.txWindow == -80...30)
        #expect(receive.duplex == nil)

        rig.key(true)
        let carrier = Self.frequency + Double(Self.xitHz)
        #expect(await settle { rig.band.transmit.keyedHere && rig.sent.last?.spanHz == 8_000 })
        #expect(rig.band.transmit.carrierHz == carrier)
        let keyed = try #require(rig.sent.last)
        #expect(keyed.centreHz == carrier)
        #expect(keyed.txWindow == -80...30)
        // Drawn with the transmit grid while keyed.
        #expect(rig.band.drawnSettings.scaleRange == -80...20)

        rig.key(false)
        #expect(await settle { !rig.band.transmit.keyedHere && rig.sent.last?.spanHz == 192_000 })
        #expect(rig.sent.last?.centreHz == Self.frequency)
        #expect(rig.band.drawnSettings.scaleRange == BandDisplaySettings.desktopDefaults.scaleRange)
    }

    @Test("a zoom while keyed is held to the transmit display's reach and kept for the next key")
    func keyedZoomIsKept() async throws {
        let rig = try Rig(txDisplay: 1)
        rig.main.receive(.mediaState(.connected))
        rig.key(true)
        let carrier = Self.frequency + Double(Self.xitHz)
        #expect(await settle { rig.sent.last?.spanHz == 8_000 })
        rig.band.requestView(TuneGestures.View(centerHz: carrier, spanHz: 20_000))
        #expect(await settle { rig.sent.last?.spanHz == 20_000 })
        #expect(rig.band.settings.txViewSpanHz == 20_000)
        // Past the reach: held to 96 kHz about the carrier.
        rig.band.requestView(TuneGestures.View(centerHz: carrier + 50_000, spanHz: 400_000))
        #expect(await settle { rig.sent.last?.spanHz == 96_000 })
        #expect(rig.sent.last?.centreHz == carrier)
        rig.band.requestView(TuneGestures.View(centerHz: carrier, spanHz: 20_000))
        #expect(await settle { rig.sent.last?.spanHz == 20_000 })
        rig.key(false)
        #expect(await settle { rig.sent.last?.spanHz == 192_000 })
        rig.key(true)
        #expect(await settle { rig.sent.last?.spanHz == 20_000 })
        #expect(rig.sent.last?.centreHz == carrier)
    }

    @Test("XIT moved while keyed moves the keyed view with the carrier")
    func theViewFollowsXit() async throws {
        let rig = try Rig(txDisplay: 1)
        rig.main.receive(.mediaState(.connected))
        rig.key(true)
        #expect(await settle { rig.sent.last?.spanHz == 8_000 })
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 57, name: "xitHz", value: .i64(2_500)),
        ])))
        #expect(await settle { rig.sent.last?.centreHz == Self.frequency + 2_500 })
        #expect(rig.band.transmit.carrierHz == Self.frequency + 2_500)
    }

    @Test("receive frames are held while keyed until the transmit display comes, and the fall brings them back")
    func receiveFramesAreHeldWhileKeyed() async throws {
        let rig = try Rig(txDisplay: 1)
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { !rig.sent.isEmpty })
        rig.context(generation: 1, transmit: false)
        rig.frame(generation: 1, sequence: 0, level: -120)
        #expect(rig.band.state.frame?.traceDbm.first == -120)
        rig.key(true)
        #expect(await settle { rig.band.transmit.keyedHere })
        // The receiver hearing its own transmitter is never drawn.
        rig.frame(generation: 1, sequence: 1, level: -20)
        #expect(rig.band.state.frame?.traceDbm.first == -120)
        // The transmit display is.
        rig.context(generation: 2, transmit: true)
        #expect(rig.band.showsTransmit)
        rig.frame(generation: 2, sequence: 0, level: -30)
        #expect(rig.band.state.frame?.traceDbm.first == -30)
        #expect(rig.band.state.frame?.contextGeneration == 2)
        // The fall: receive frames again.
        rig.key(false)
        rig.context(generation: 3, transmit: false)
        #expect(await settle { !rig.band.transmit.keyedHere })
        #expect(!rig.band.showsTransmit)
        rig.frame(generation: 3, sequence: 0, level: -110)
        #expect(rig.band.state.frame?.traceDbm.first == -110)
    }

    @Test("another Core pan keeps this band's receive frames; selecting the transmit pan while keyed swaps it")
    func anotherPanAndActiveChange() async throws {
        let rig = try Rig(txDisplay: 3)
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-0")),
        ])))
        rig.addSlice(1, panKey: "pan-1")
        rig.mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "transmit", className: "TransmitModel",
                                                                  properties: [
            .init(ordinal: 4, name: "filterLow", value: .i64(100)),
            .init(ordinal: 5, name: "filterHigh", value: .i64(2900)),
        ])))
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.last?.sliceId == 0 })
        rig.context(generation: 1, transmit: false)
        rig.frame(generation: 1, sequence: 0, level: -120)
        rig.transmitOn(1, highSwr: true)
        await quiet()
        #expect(!rig.band.transmit.keyedHere)
        #expect(TransmitDisplayBanner.swrText(rig.band.transmit) == nil)
        #expect(rig.main.transmit.txFilterHz == nil)
        #expect(rig.sent.last?.sliceId == 0 && rig.sent.last?.spanHz == 192_000)
        rig.frame(generation: 1, sequence: 1, level: -20)
        #expect(rig.band.state.frame?.traceDbm.first == -20)

        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 11, name: "active", value: .bool(false)),
        ])))
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 11, name: "active", value: .bool(true)),
        ])))
        #expect(await settle { rig.band.transmit.keyedHere && rig.sent.last?.sliceId == 1
            && rig.sent.last?.spanHz == 8_000 })
        #expect(await settle { rig.band.transmit.highSwr && rig.main.transmit.txFilterHz != nil })
        #expect(TransmitDisplayBanner.swrText(rig.band.transmit) == TransmitDisplayBanner.highSwrText)
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 11, name: "active", value: .bool(false)),
        ])))
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 11, name: "active", value: .bool(true)),
        ])))
        #expect(await settle { !rig.band.transmit.keyedHere && rig.main.transmit.txFilterHz == nil })
        #expect(TransmitDisplayBanner.swrText(rig.band.transmit) == nil)
    }

    @Test("a different slice sharing the displayed Core pan gets its transmit display")
    func sharedCorePan() async throws {
        let rig = try Rig(txDisplay: 3)
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-0")),
        ])))
        rig.addSlice(1, panKey: "pan-0")
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.last?.sliceId == 0 })
        rig.transmitOn(1)
        #expect(await settle { rig.band.transmit.keyedHere && rig.sent.last?.spanHz == 8_000 })
        #expect(rig.sent.last?.sliceId == 0)
        #expect(rig.band.transmit.carrierHz == Self.frequency + 10_000)
        // The Core keeps the display on the pan it recorded at the rise.
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-1")),
        ])))
        await quiet()
        #expect(rig.band.transmit.keyedHere)
    }

    @Test("a shared transmit view says another device sets it")
    func sharedTransmitView() async throws {
        let rig = try Rig(txDisplay: 1)
        rig.main.receive(.mediaState(.connected))
        rig.key(true)
        #expect(await settle { rig.band.transmit.keyedHere })
        rig.context(generation: 2, transmit: true, limit: .shared)
        #expect(rig.band.transmitViewShared)
        #expect(TransmitDisplayBanner.note(missing: rig.band.transmitDisplayMissing,
                                           shared: rig.band.transmitViewShared) == TransmitDisplayBanner.sharedText)
        rig.context(generation: 3, transmit: true, limit: .none)
        #expect(!rig.band.transmitViewShared)
    }

    // MARK: A Core without it

    @Test("a Core that sends no transmit display: today's wire, the picture held, and the desktop's words")
    func aCoreWithoutTheTransmitDisplay() async throws {
        let rig = try Rig(txDisplay: 0)
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { !rig.sent.isEmpty })
        #expect(rig.sent.allSatisfy { $0.txWindow == nil && $0.duplex == nil })
        rig.context(generation: 1, transmit: nil)
        rig.frame(generation: 1, sequence: 0, level: -120)
        let count = rig.sent.count
        rig.key(true)
        #expect(await settle { rig.band.transmit.keyedHere })
        await quiet()
        // The view stays: nothing new is asked for the key.
        #expect(rig.sent.count == count)
        #expect(rig.band.transmitDisplayMissing)
        #expect(TransmitDisplayBanner.note(missing: true, shared: false)
            == "This Core does not send its transmit display. Updating the Core may help.")
        rig.frame(generation: 1, sequence: 1, level: -20)
        #expect(rig.band.state.frame?.traceDbm.first == -120)
    }

    @Test("a Core that sends no txState keys the band from radio.transmitting and the transmit slice")
    func withoutTxState() async throws {
        let rig = try Rig(txDisplay: 1, txState: false)
        rig.mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "radio", className: "RadioModel", properties: [
            .init(ordinal: 0, name: "transmitting", value: .bool(true)),
        ])))
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 20, name: "txSlice", value: .bool(true)),
        ])))
        #expect(await settle { rig.band.transmit.keyedHere })
        #expect(rig.band.transmit.carrierHz == Self.frequency + Double(Self.xitHz))
    }

    @Test("an out-of-range floating transmit slice ID cannot key the band")
    func outOfRangeTxSliceId() async throws {
        let rig = try Rig(txDisplay: 1)
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "txState", properties: [
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
            .init(ordinal: 3, name: "txSliceId", value: .f64(1e100)),
        ])))
        rig.main.transmitDisplay.refresh()
        #expect(!rig.band.transmit.keyedHere)
    }

    // MARK: DUP

    @Test("DUP on a Core at 3 keeps the receiver while keyed and swaps at once")
    func duplexKeepsTheReceiver() async throws {
        let rig = try Rig(txDisplay: 3)
        rig.main.receive(.mediaState(.connected))
        #expect(rig.main.display.duplexAvailable)
        #expect(rig.main.display.duplexNote == DisplaySheetModel.duplexOffNote)
        rig.key(true)
        #expect(await settle { rig.sent.last?.spanHz == 8_000 })
        rig.main.display.setDuplex(true)
        #expect(await settle { rig.sent.last?.duplex == true && rig.sent.last?.spanHz == 192_000 })
        #expect(rig.main.display.duplexNote == DisplaySheetModel.duplexOnNote)
        rig.context(generation: 1, transmit: false)
        rig.frame(generation: 1, sequence: 0, level: -118)
        // DUP on: receive frames are drawn while keyed.
        #expect(rig.band.state.frame?.traceDbm.first == -118)
        rig.main.display.setDuplex(false)
        #expect(await settle { rig.sent.last?.duplex == nil && rig.sent.last?.spanHz == 8_000 })
    }

    @Test("DUP leaves the orange TX filter at the VFO without XIT")
    func duplexFilterLeavesOutXit() async throws {
        let rig = try Rig(txDisplay: 3)
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { !rig.sent.isEmpty })
        rig.mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "transmit", className: "TransmitModel",
                                                                  properties: [
            .init(ordinal: 4, name: "filterLow", value: .i64(100)),
            .init(ordinal: 5, name: "filterHigh", value: .i64(2900)),
        ])))
        rig.key(true)
        #expect(await settle { rig.main.transmit.txFilterHz
            == (Self.frequency + Double(Self.xitHz) + 100)...(Self.frequency + Double(Self.xitHz) + 2900) })
        rig.main.display.setDuplex(true)
        #expect(await settle { rig.band.duplexActive && rig.main.transmit.txFilterHz
            == (Self.frequency + 100)...(Self.frequency + 2900) })
    }

    @Test(arguments: [Int64(0), Int64(1), Int64(2)])
    func duplexIsGreyedWithItsReasonBelowThree(version: Int64) async throws {
        let rig = try Rig(txDisplay: version)
        #expect(!rig.main.display.duplexAvailable)
        #expect(rig.main.display.duplexNote == DisplaySheetModel.duplexUnavailableText)
        rig.main.display.setDuplex(true)
        #expect(!rig.band.settings.displayDuplex)
    }

    // MARK: High SWR

    @Test("the Core's high SWR borders the band, with the power turned down when the Core latched it")
    func highSwr() async throws {
        let rig = try Rig(txDisplay: 0)
        rig.key(true, highSwr: true)
        #expect(await settle { rig.band.transmit.highSwr })
        #expect(TransmitDisplayBanner.swrText(rig.band.transmit) == TransmitDisplayBanner.highSwrText)
        rig.key(true, highSwr: true, windBack: true)
        #expect(await settle { rig.band.transmit.windBackLatched })
        #expect(TransmitDisplayBanner.swrText(rig.band.transmit) == TransmitDisplayBanner.powerTurnedDownText)
        rig.key(false)
        #expect(await settle { !rig.band.transmit.highSwr })
        #expect(TransmitDisplayBanner.swrText(rig.band.transmit) == nil)
    }

    // MARK: Keyed scale

    @Test("while keyed the scale's arrows move the transmit grid, kept on this phone")
    func keyedScaleMovesTheTransmitGrid() async throws {
        let rig = try Rig(txDisplay: 1)
        rig.key(true)
        #expect(await settle { rig.band.transmitOverlay })
        #expect(rig.main.display.top == 20)
        #expect(rig.main.display.canRaiseTop)
        rig.main.display.nudgeTop(up: false)
        #expect(rig.band.settings.txGridTopDbm == 10)
        #expect(rig.band.settings.txGridRangeDb == 90)
        #expect(rig.band.settings.scaleTopDbm == BandDisplaySettings.scaleTopDefaultDbm)
        #expect(rig.band.settings.txDbmWindow == -80...30)
    }

    // MARK: The Core's settings

    @Test(arguments: [(Int64(1), false), (Int64(2), true), (Int64(3), true)])
    func theCoresTransmitDisplaySettingsNeedVersion2(version: Int64, available: Bool) async throws {
        let rig = try Rig(txDisplay: version)
        let core = rig.main.coreTxDisplay
        #expect(core.available == available)
        #expect(core.fftSize == 32_768 && core.window == 4 && core.traceAverageTimeMs == 30)
        #expect(core.waterfallAverageTimeMs == 120 && !core.normalize && !core.normalizeAvailable)
        core.write(CoreTxDisplaySettings.fftSizeKey, "65536")
        await quiet()
        let writes = rig.recorded.settingsSent.compactMap { message -> String? in
            if case .settingsWrite(let write) = message { return write.key } else { return nil }
        }
        #expect(writes == (available ? [CoreTxDisplaySettings.fftSizeKey] : []))
    }
}
