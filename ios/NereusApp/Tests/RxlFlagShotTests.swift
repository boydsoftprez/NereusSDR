// NereusSDR for iOS: the flag's RXL line on the real band, on this phone's slice and on a listened one
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-15: New. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// JJ, 2026-11-15 (his choice 1 of three drawn): while RXL is on in a
/// mode it levels, the flag has a thin line under its header with the
/// desktop's RXL tag at its right end, "RXL +12 dB", on this phone's
/// slices and on a slice another device controls alike; the flag grows by
/// the line. With `NEREUS_BAND_SHOTS` set to a directory, each screen is
/// written there.
@Suite("The flag's RXL line", .serialized)
@MainActor
struct RxlFlagShotTests {
    struct Shot: Sendable, CustomStringConvertible {
        let owned: Bool
        var description: String { "rxl-line\(owned ? "-listened" : "")" }
    }

    nonisolated static let shots = [Shot(owned: false), Shot(owned: true)]
    static let size = CGSize(width: 402, height: 640)

    @Test("RXL's text: whole dB with a sign, a real minus sign for a cut")
    func text() {
        #expect(RxlLiftTag.text(12) == "RXL +12 dB")
        #expect(RxlLiftTag.text(11.6) == "RXL +12 dB")
        #expect(RxlLiftTag.text(-5) == "RXL \u{2212}5 dB")
        #expect(RxlLiftTag.text(0.4) == "RXL 0 dB")
        #expect(RxlLiftTag.text(-0.4) == "RXL 0 dB")
        #expect(RxlLiftTag.text(.nan) == "RXL 0 dB")
        #expect(RxlLiftTag.ink(3) == RxlLiftTag.lift)
        #expect(RxlLiftTag.ink(-3) == RxlLiftTag.cut)
        #expect(RxlLiftTag.ink(0) == RxlLiftTag.level)
    }

    @Test(arguments: shots)
    func theFlagShowsItsRxlLine(_ shot: Shot) async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        var capabilities: [LinkMessage.PropertyEntry] = [
            .init(ordinal: 0, name: BandSlicesModel.remoteTxCapability, value: .i64(1)),
            .init(ordinal: 2, name: "stationCatalogVersion", value: .i64(1)),
        ]
        if shot.owned {
            capabilities.append(.init(ordinal: 1, name: SliceAccess.capability, value: .i64(2)))
        }
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        let json = try #require(ModesTabBindingTests.catalogueJSON("catalog-anan-g2"))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: "StationCatalog",
                                                           properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
        ])))
        if shot.owned {
            mirror.apply(Self.slice(1, hz: 14_251_000, active: true, rxl: true))
        } else {
            mirror.apply(Self.slice(0, hz: 14_251_000, active: true, rxl: true))
        }
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        if shot.owned {
            main.slices.thisDeviceId = "me"
            mirror.apply(FlagButtonsShotTests.access(1, controller: "shack-desktop", revision: 12))
        }
        let band = main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context(centreHz: 14_240_000))
        band.receive(.context(context))
        let count = 1
        #expect(await ShotWait.until { main.slices.entries.count == count && main.slices.catalog != nil })
        let entry = try #require(main.slices.entries.first)
        #expect(entry.rxlLiftDb == 12)
        #expect(entry.listening == shot.owned)
        // The line makes the flag 18 points taller.
        #expect(VfoFlagView.expectedHeight(for: entry)
                == FlagMetrics(large: false).height(rade: false, listening: shot.owned) + FlagMetrics.rxlLine)
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "RxlTagSpotShotTests") ?? .standard)

        let window = try BandFlagShotTests.window(size: Self.size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: main.slices, settings: settings, tuning: main.tuning,
                             foreign: main.foreign, flagControls: main.flagControls)
        }
        .frame(width: Self.size.width, height: Self.size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: Self.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(Self.size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: shot.description)
    }

    @Test("RXL off, or on in a mode it leaves alone: no line")
    func noLineWhenRxlIsOffOrTheModeIsNotVoice() async throws {
        let mirror = MirrorStore(send: { _ in })
        let json = try #require(ModesTabBindingTests.catalogueJSON("catalog-anan-g2"))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: "StationCatalog",
                                                           properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
        ])))
        mirror.apply(Self.slice(0, hz: 14_251_000, active: true, rxl: false))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        #expect(await ShotWait.until { main.slices.entries.count == 1 && main.slices.catalog != nil })
        #expect(main.slices.entries.first?.rxlLiftDb == nil)
        mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 161, name: "rxLevelerMode", value: .enumeration(2)),
            .init(ordinal: 162, name: "rxLevelerGainDb", value: .f64(-4.6)),
        ])))
        #expect(await ShotWait.until { main.slices.entries.first?.rxlLiftDb == -4.6 })
        // CW: RXL leaves the audio alone, so the flag has no line.
        let cw = try #require(main.slices.catalog?.modes.first { $0.label == "CWU" })
        mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 2, name: "dspMode", value: .enumeration(Int64(cw.id))),
        ])))
        #expect(await ShotWait.until { main.slices.entries.first?.rxlLiftDb == nil })
    }

    static func slice(_ index: Int, hz: Double, active: Bool, rxl: Bool) -> LinkMessage {
        var properties: [LinkMessage.PropertyEntry] = [
            .init(ordinal: 1, name: "frequency", value: .f64(hz)),
            .init(ordinal: 2, name: "dspMode", value: .enumeration(1)),
            .init(ordinal: 3, name: "filterLow", value: .i64(100)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(2900)),
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
            .init(ordinal: 9, name: "rxAntenna", value: .utf8("ANT1")),
            .init(ordinal: 10, name: "txAntenna", value: .utf8("ANT1")),
            .init(ordinal: 11, name: "active", value: .bool(active)),
            .init(ordinal: 12, name: "txSlice", value: .bool(index == 0)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(Int64(index))),
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-105)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            .init(ordinal: 35, name: "locked", value: .bool(false)),
        ]
        if rxl {
            properties.append(.init(ordinal: 161, name: "rxLevelerMode", value: .enumeration(1)))
            properties.append(.init(ordinal: 162, name: "rxLevelerGainDb", value: .f64(12)))
        }
        return .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel",
                                                      properties: properties))
    }
}
