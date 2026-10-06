// NereusSDR for iOS: the TX zero line follows the desktop: on for any keying source, at the transmit slice's dial
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// The desktop draws the TX zero line whenever its radio is on the air,
/// whatever keyed it (this phone, another device, the Core's own window or
/// the radio's PTT), on the pan that hosts the transmit slice, at that
/// slice's dial frequency: no XIT, no CW pitch. The phone does the same:
/// ``TransmitModel/txZeroLineSliceId`` names the slice the band draws the
/// line at (``BandGestureLayer``, at the slice's `frequencyHz`).
///
/// Slices A and B, A active, the Core transmitting on B.
@Suite("TX zero line", .serialized)
@MainActor
struct TxZeroLineTests {
    private let platform = TestPlatform()
    static let thisDeviceId = "zero-line-phone-id"
    static let bFrequency = 7_190_000.0
    static let xitHz: Int64 = 1_500

    @Test("keyed by this phone, the line is on B, the transmit slice, while A is active")
    func keyedHere() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        #expect(transmit.txZeroLineSliceId == nil)
        transmit.tapPtt()
        #expect(await settle { transmit.ptt.transmitting })
        #expect(await settle { transmit.txZeroLineSliceId == 1 })
        #expect(model.main.slices.activeSliceId == 0)
        // The Core says who holds it: this phone.
        await station.deliver(Self.txState(holder: Self.thisDeviceId, source: "device", keyed: true))
        #expect(await settle { transmit.transmittingHere })
        #expect(transmit.txZeroLineSliceId == 1)
        transmit.tapPtt()
        await station.deliver(Self.txState(holder: "", source: "", keyed: false))
        #expect(await settle { transmit.ptt.state == .idle && transmit.txZeroLineSliceId == nil })
        await model.disconnect()
    }

    /// The keying rule (Core 5cdb9a1ab): the flag on B, which this phone
    /// does not hold, and A this phone's own. The Core admits the key, moves
    /// the unkeyed flag to A and keys there: the PTT keys as for any key, and
    /// the TX badge and the line follow the flag to A. Nothing else is sent.
    @Test("a key whose flag the Core moves to this phone's slice keys there, and the badge and line follow")
    func keyWithTheFlagMovedHere() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state.isKeyed })
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 12, name: "txSlice", value: .bool(false)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 12, name: "txSlice", value: .bool(true)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "txState", properties: [
            .init(ordinal: 3, name: "txSliceId", value: .i64(0)),
        ] + Self.holder(Self.thisDeviceId, source: "device", keyed: true))))
        #expect(await settle { transmit.transmittingHere && transmit.txZeroLineSliceId == 0 })
        #expect(transmit.ptt.state.isKeyed)
        #expect(model.main.slices.entries.first { $0.id == 0 }?.slice.txSlice == true)
        #expect(model.main.slices.entries.first { $0.id == 1 }?.slice.txSlice == false)
        let verbs = station.messages.compactMap(TransmitScreenTests.invoke).map(\.verb)
        #expect(!verbs.contains("tx.unkey") && !verbs.contains("tx.setTxSlice"))
        transmit.tapPtt()
        await station.deliver(Self.txState(holder: "", source: "", keyed: false))
        #expect(await settle { transmit.ptt.state == .idle && transmit.txZeroLineSliceId == nil })
        #expect(!station.keyed)
        await model.disconnect()
    }

    @Test("keyed by another device, the Core's window or the radio's PTT, the line is on B", arguments: [
        ("macbook-device-id", "device"), ("", ""), ("", TransmitStateReport.radioPttSource),
    ])
    func keyedElsewhere(_ holder: String, _ source: String) async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        await station.deliver(Self.txState(holder: holder, source: source, keyed: true))
        #expect(await settle { transmit.txZeroLineSliceId == 1 })
        #expect(!transmit.ptt.transmitting)
        #expect(model.main.slices.activeSliceId == 0)
        await station.deliver(Self.txState(holder: holder, source: source, keyed: false))
        #expect(await settle { transmit.txZeroLineSliceId == nil })
        await model.disconnect()
    }

    @Test("with XIT on B the line stays at B's dial frequency")
    func xitLeavesTheLineAtTheDial() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 56, name: "xitEnabled", value: .bool(true)),
            .init(ordinal: 57, name: "xitHz", value: .i64(Self.xitHz)),
        ])))
        await station.deliver(Self.txState(holder: "macbook-device-id", source: "device", keyed: true))
        #expect(await settle { transmit.txZeroLineSliceId == 1 })
        let entry = try #require(model.main.slices.entries.first { $0.id == transmit.txZeroLineSliceId })
        #expect(entry.slice.frequencyHz == Self.bFrequency)
        await model.disconnect()
    }

    @Test("a transmit slice that is not on this band draws no line here")
    func transmitSliceElsewhere() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        await station.deliver(.delta(LinkMessage.Delta(key: "txState", properties: [
            .init(ordinal: 3, name: "txSliceId", value: .i64(3)),
        ])))
        await station.deliver(Self.txState(holder: "macbook-device-id", source: "device", keyed: true))
        #expect(await settle { transmit.onAirElsewhere != nil })
        #expect(transmit.txZeroLineSliceId == nil)
        await model.disconnect()
    }

    // MARK: Inside

    /// The app connected to a fake Core that lets it transmit, with slice B
    /// beside slice A on A's pan, A active, and the Core's transmit slice B.
    private func connected() async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "TxZeroLineTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults), platform: platform.platform)
        model.main.transmit.thisDeviceId = Self.thisDeviceId
        let station = try FakeStation(additions: [.remoteTx])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected && !model.main.slices.entries.isEmpty })
        guard case .text(let pan)? = model.mirror.object("slice:0")?["panKey"] else {
            Issue.record("slice A has no pan")
            return (model, station)
        }
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 11, name: "active", value: .bool(true)),
        ])))
        await station.deliver(.objectCreate(try FakeStation.objectCreate(
            key: "slice:1", className: "SliceModel",
            values: ["frequency": .f64(Self.bFrequency), "sliceIndex": .i64(1), "active": .bool(false),
                     "panKey": .utf8(pan), "xitEnabled": .bool(false), "xitHz": .i64(0),
                     "txSlice": .bool(true)])))
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "txState", className: "TransmitState",
                                                                    properties: Self.holder("", source: "", keyed: false)
                                                                        + [.init(ordinal: 3, name: "txSliceId", value: .i64(1)),
                                                                           .init(ordinal: 17, name: "stopSerial",
                                                                                 value: .i64(0))])))
        #expect(await settle {
            model.main.transmit.permitted && model.main.slices.entries.count == 2 && model.main.slices.activeSliceId == 0
        })
        return (model, station)
    }

    static func holder(_ id: String, source: String, keyed: Bool) -> [LinkMessage.PropertyEntry] {
        [
            .init(ordinal: 0, name: "keyed", value: .bool(keyed)),
            .init(ordinal: 18, name: "holderDeviceId", value: .utf8(id)),
            .init(ordinal: 19, name: "holderName", value: .utf8(id.isEmpty ? "" : "Holder")),
            .init(ordinal: 20, name: "holderShortName", value: .utf8(id.isEmpty ? "" : "Holder")),
            .init(ordinal: 22, name: "holderSource", value: .utf8(source)),
            .init(ordinal: 24, name: "holderEpoch", value: .i64(keyed ? 3 : 4)),
        ]
    }

    static func txState(holder: String, source: String, keyed: Bool) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "txState", properties: Self.holder(holder, source: source, keyed: keyed)))
    }

    /// Waits for `condition` without sleeping, up to 30 seconds of turns.
    private func settle(_ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(30)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }
}
