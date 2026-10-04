// NereusSDR for iOS: the flag's Filter policy lines on screen: the Core's WIDE reason, a low-pass held for a slice, the HL2 high-pass sentence
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-18, R-IOS-11 (shared-input filters, JJ's ruling of 2026-09-30):
/// the more menu's Filter policy lines show the first input's state as the
/// desktop's Filter Policy dialog does, with the Core's words as sent, on
/// an iPhone 18 Pro Max's upright band, dark, light and in large type.
/// With `NEREUS_BAND_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_BAND_SHOTS`), each frame is written there as
/// `filters-lp-<name>.png`.
@Suite("Filter reasons on screen", .serialized)
@MainActor
struct FilterReasonShotTests {
    nonisolated static let wideWords = "BYPASS (slice B on WWV has no filter pins)"
    nonisolated static let multiBandWords = "BYPASS (multi-band: 80m + 20m)"
    nonisolated static let lowPassWords = "The receive low-pass filter is set for slice B on 20m, the highest band on this "
        + "receiver input. Slice A on 80m shares the input, so it has less protection from strong signals on "
        + "higher bands."
    nonisolated static let highPassWords = "The broadcast-band high-pass filter is off because slice A on 30m needs it off."

    /// What the Core reports, and where slices A and B sit.
    enum Case: String, CaseIterable, Sendable {
        /// The HL2's board off: the top slice's band has no pins; no low-pass reason.
        case wide
        /// Slices on 80 m and 20 m through one input: the low-pass held for B.
        case lowPass = "low-pass"
        /// The HL2's broadcast-band high-pass off for A; no slice held.
        case highPass = "high-pass"

        var aHz: Double {
            switch self {
            case .wide: return 7_200_000
            case .lowPass: return 3_800_000
            case .highPass: return 10_120_000
            }
        }
        var bHz: Double {
            switch self {
            case .wide: return 10_000_000
            case .lowPass, .highPass: return 14_200_000
            }
        }
        var effective: Int64 { self == .highPass ? 0 : 1 }
        var reason: String {
            switch self {
            case .wide: return FilterReasonShotTests.wideWords
            case .lowPass: return FilterReasonShotTests.multiBandWords
            case .highPass: return "20m"
            }
        }
        var lowPass: String {
            switch self {
            case .wide: return ""
            case .lowPass: return FilterReasonShotTests.lowPassWords
            case .highPass: return FilterReasonShotTests.highPassWords
            }
        }
        var lowPassSlice: Int64 { self == .lowPass ? 1 : -1 }
        /// The lines the menu shows, in the desktop dialog's words.
        var lines: [String] {
            switch self {
            case .wide:
                return ["Effective: BYPASS", "Reason: " + FilterReasonShotTests.wideWords]
            case .lowPass:
                return ["Effective: BYPASS", "Reason: " + FilterReasonShotTests.multiBandWords,
                        "Shared input: " + FilterReasonShotTests.lowPassWords]
            case .highPass:
                return ["Effective: Filtered", "Reason: 20m", "Shared input: " + FilterReasonShotTests.highPassWords]
            }
        }
    }

    enum Look: String, CaseIterable, Sendable {
        case dark
        case light
        case largeType = "large-type"
    }

    struct Frame: Sendable, CustomStringConvertible {
        let reported: Case
        let look: Look
        var description: String { "filters-lp-\(reported.rawValue)-portrait-\(look.rawValue)" }
    }

    nonisolated static let frames = Case.allCases.flatMap { reported in Look.allCases.map { Frame(reported: reported, look: $0) } }
    /// The band between the toolbar and the tab bar on an iPhone 18 Pro Max, upright.
    nonisolated static let size = CGSize(width: 440, height: 738)
    nonisolated static let span = 48_000.0

    @Test(arguments: frames)
    func theMoreMenuShowsTheCoresFilterWords(_ frame: Frame) async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(ordinal: 0, name: BandSlicesModel.remoteTxCapability, value: .i64(1)),
            .init(ordinal: 2, name: "stationCatalogVersion", value: .i64(1)),
            .init(ordinal: 103, name: "rxFilterLowPassVersion", value: .i64(1)),
        ])))
        let json = try #require(ModesTabBindingTests.catalogueJSON("catalog-anan-g2"))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: "StationCatalog",
                                                           properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
        ])))
        let reported = frame.reported
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "radio", className: "RadioModel", properties: [
            .init(ordinal: 11, name: "rxFilter0Mode", value: .i64(0)),
            .init(ordinal: 12, name: "rxFilter0Effective", value: .i64(reported.effective)),
            .init(ordinal: 13, name: "rxFilter0Band", value: .i64(5)),
            .init(ordinal: 14, name: "rxFilter0Reason", value: .utf8(reported.reason)),
            .init(ordinal: 35, name: "rxFilter0LowPassReason", value: .utf8(reported.lowPass)),
            .init(ordinal: 36, name: "rxFilter0LowPassSlice", value: .i64(reported.lowPassSlice)),
        ])))
        mirror.apply(FlagButtonsShotTests.slice(0, hz: reported.aHz, active: false))
        mirror.apply(FlagButtonsShotTests.slice(1, hz: reported.bHz, active: true))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        let band = main.band
        band.endpointId = 1
        let context = try #require(Self.context(centreHz: reported.bHz))
        band.receive(.context(context))
        #expect(await ShotWait.until { main.slices.entries.count == 2 && main.slices.catalog != nil })

        let radio = try #require(mirror.object(FlagControls.radioKey))
        #expect(FlagControls.filterStateLines(ReceiveFilterState(radio: radio.values)) == reported.lines)

        let controls = main.flagControls
        controls.toggle(.more(1))
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "FilterReasonShotTests") ?? .standard)
        let window = try BandFlagShotTests.window(size: Self.size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: main.slices, settings: settings, tuning: main.tuning,
                             foreign: main.foreign, sideways: false, flagControls: controls, transmitReason: nil)
        }
        .environment(\.dynamicTypeSize, frame.look == .largeType ? RadeFlagShotTests.largeType : .large)
        .frame(width: Self.size.width, height: Self.size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = frame.look == .light ? .light : .dark
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
        #expect(await ShotWait.until { controls.isOpen(.more(1)) && !main.slices.flagRects.isEmpty })
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: frame.description)
    }

    @Test("the lines follow the desktop dialog: wideband named, and its own sentence before the Core's state")
    func theLinesFollowTheDesktopDialog() {
        #expect(FlagControls.filterStateLines(nil) == ["Core filter state is not available."])
        let wideband = ReceiveFilterState(effective: .widebandLocked, reason: "BYPASS (wideband active)")
        #expect(FlagControls.filterStateLines(wideband) == ["Effective: BYPASS (wideband)",
                                                           "Reason: BYPASS (wideband active)"])
    }

    /// The band's display context, centred on `centreHz`.
    static func context(centreHz: Double) -> MediaControlEvent.DisplayContext? {
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("shots"), "endpointId": .number(1),
            "revision": .number(1), "contextGeneration": .number(1), "sourceStream": .number(0),
            "sourceCentreHz": .number(centreHz), "sampleRateHz": .number(192_000), "centreHz": .number(centreHz),
            "spanHz": .number(span), "wideCentreHz": .number(0), "wideSpanHz": .number(0),
            "traceSamples": .number(1_206), "waterfallSamples": .number(1_206), "wideSamples": .number(0),
            "minDbm": .number(-160), "maxDbm": .number(0), "fps": .number(30), "framesPerLine": .number(1),
        ]
        return MediaControlDecoder.context(payload, wideband: false, grant: false)
    }
}
