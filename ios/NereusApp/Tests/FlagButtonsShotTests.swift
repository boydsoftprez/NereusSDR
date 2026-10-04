// NereusSDR for iOS: the finger-sized flag, its menus and panels on screen, upright and sideways, for comparing with the board
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-11, R-IOS-42: the board's "Flag controls at finger size" frames,
/// drawn by the real band, flags, side buttons, menus and panels on the
/// main screen's models, fed a mirror with the Core's catalogue from the
/// link's conformance suite. With `NEREUS_BAND_SHOTS` set to a directory
/// (through `TEST_RUNNER_NEREUS_BAND_SHOTS`), each frame is written there
/// as `flagbtn-<name>.png`, named as the board's frames are.
@Suite("Flag buttons on screen", .serialized)
@MainActor
struct FlagButtonsShotTests {
    enum Frame: String, CaseIterable, Sendable, CustomStringConvertible {
        case portraitMain = "portrait-main"
        case portraitAntenna = "portrait-antenna"
        case portraitMore = "portrait-more"
        case portraitStep = "portrait-step"
        case portraitPanelAudio = "portrait-panel-audio"
        case portraitPanelDsp = "portrait-panel-dsp"
        case portraitPanelMode = "portrait-panel-mode"
        case portraitPanelXrit = "portrait-panel-xrit"
        case portraitOwnedAudio = "portrait-owned-audio"
        case portraitSliceA = "portrait-slice-a"
        case portraitReceiveOnly = "portrait-receive-only"
        case portraitOwned = "portrait-owned"
        case portraitLargeType = "portrait-large-type"
        case portraitLargeTypeModePanel = "portrait-large-type-mode-panel"
        case landscapeBothFull = "landscape-both-full"
        case landscapeFolds = "landscape-folds"
        case landscapeLargeType = "landscape-large-type"
        case landscapeXritPanel = "landscape-xrit-panel"

        var description: String { "flagbtn-\(rawValue)" }
        var sideways: Bool { rawValue.hasPrefix("landscape") }
        var large: Bool { rawValue.contains("large-type") }
        /// B is controlled by the shack's desktop; this phone listens to it.
        var owned: Bool { rawValue.hasPrefix("portrait-owned") }
        /// The band's area on an iPhone 17, between the toolbar and the tab bar.
        var size: CGSize { sideways ? CGSize(width: 874, height: 330) : CGSize(width: 402, height: 640) }
        /// Slice B's frequency: beside A upright (the board's B), past a
        /// flag and its buttons sideways, or close enough to fold.
        var bHz: Double {
            switch self {
            case .landscapeBothFull, .landscapeLargeType, .landscapeXritPanel:
                return 7_256_000
            default:
                return 7_249_000
            }
        }
        /// The slice active when the frame opens: B upright (the board's
        /// "Slice B's flag"), A sideways and on the slice A frame.
        var activeB: Bool { !sideways && self != .portraitSliceA }
    }

    @Test(arguments: Frame.allCases)
    func theFrameShowsTheFlagsButtons(_ frame: Frame) async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        var capabilities: [LinkMessage.PropertyEntry] = [
            .init(ordinal: 0, name: BandSlicesModel.remoteTxCapability, value: .i64(1)),
            .init(ordinal: 2, name: "stationCatalogVersion", value: .i64(1)),
        ]
        if frame.owned {
            capabilities.append(.init(ordinal: 1, name: SliceAccess.capability, value: .i64(2)))
        }
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        let json = try #require(ModesTabBindingTests.catalogueJSON("catalog-anan-g2"))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: "StationCatalog",
                                                           properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
        ])))
        mirror.apply(Self.slice(0, hz: 7_236_400, active: !frame.activeB))
        mirror.apply(Self.slice(1, hz: frame.bHz, active: frame.activeB))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        if frame.owned {
            main.slices.thisDeviceId = "me"
            mirror.apply(Self.access(0, controller: "me", revision: 1))
            mirror.apply(Self.access(1, controller: "shack-desktop", revision: 12))
        }
        let band = main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        #expect(await ShotWait.until { main.slices.entries.count == 2 && main.slices.catalog != nil })

        let controls = main.flagControls
        let b = 1
        switch frame {
        case .portraitAntenna:
            controls.toggle(.antennas(b))
        case .portraitMore:
            controls.toggle(.more(b))
        case .portraitStep:
            controls.openStep(b)
        case .portraitPanelAudio:
            controls.toggle(.panel(b, .audio))
        case .portraitPanelDsp:
            controls.toggle(.panel(b, .dsp))
        case .portraitPanelMode:
            controls.toggle(.panel(b, .mode))
        case .portraitPanelXrit:
            controls.toggle(.panel(b, .xrit))
        case .portraitOwnedAudio:
            // A listened slice's speaker panel: AF Gain, MUTE, the note and Stop listening live.
            controls.toggle(.panel(b, .audio))
        case .portraitLargeTypeModePanel:
            controls.toggle(.panel(b, .mode))
        case .landscapeXritPanel:
            // B's tab sideways: B, not active, becomes active first.
            controls.toggle(.panel(b, .xrit))
            mirror.apply(Self.active(0, false))
            mirror.apply(Self.active(1, true))
            #expect(await ShotWait.until { controls.showsControls(for: b) })
        default:
            break
        }
        let reason: String? = frame == .portraitReceiveOnly ? TransmitModel.noRemoteTransmitText : nil
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "FlagButtonsShotTests") ?? .standard)

        let window = try BandFlagShotTests.window(size: frame.size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: main.slices, settings: settings, tuning: main.tuning,
                             foreign: main.foreign, sideways: frame.sideways, flagControls: controls,
                             transmitReason: reason)
        }
        .environment(\.dynamicTypeSize, frame.large ? RadeFlagShotTests.largeType : .large)
        .frame(width: frame.size.width, height: frame.size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: frame.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(frame.size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)

        // Both flags full sideways past 246 points; B folded closer.
        #expect(await ShotWait.until { main.slices.flagRects.count == (frame.bHz == 7_256_000 ? 2 : 1) })
        if frame.owned {
            #expect(main.slices.entries.first { $0.id == 1 }?.listening == true)
        }
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: frame.description)
    }

    // MARK: Inside

    static func slice(_ index: Int, hz: Double, active: Bool) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(hz)),
            .init(ordinal: 2, name: "dspMode", value: .enumeration(0)),
            .init(ordinal: 3, name: "filterLow", value: .i64(-3000)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
            .init(ordinal: 9, name: "rxAntenna", value: .utf8("ANT1")),
            .init(ordinal: 10, name: "txAntenna", value: .utf8("ANT1")),
            .init(ordinal: 11, name: "active", value: .bool(active)),
            .init(ordinal: 12, name: "txSlice", value: .bool(index == 0)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(Int64(index))),
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-72 - Double(index) * 20)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            .init(ordinal: 35, name: "locked", value: .bool(false)),
        ]))
    }

    static func active(_ index: Int, _ active: Bool) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "slice:\(index)", properties: [
            .init(ordinal: 11, name: "active", value: .bool(active)),
        ]))
    }

    static func access(_ id: Int64, controller: String, revision: Int64) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "access:\(id)", className: SliceAccess.accessClass, properties: [
            .init(ordinal: 0, name: "sliceId", value: .i64(id)),
            .init(ordinal: 1, name: "incarnation", value: .i64(40 + id)),
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(controller)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(revision)),
            .init(ordinal: 4, name: "listenerDeviceIds", value: .utf8("[]")),
        ]))
    }
}
