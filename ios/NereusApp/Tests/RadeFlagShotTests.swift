// NereusSDR for iOS: the RADE row on screen: receiving, not locked on, an older Core, and large type
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-11, D9, spec section 5.1 item 4: the real flag with its RADE row
/// on the simulator, and the real band with a RADE slice's taller flag. The
/// lock state and offset are fixtures built here, since the Core does not
/// send them. With `NEREUS_BAND_SHOTS` set to a directory, each screen is
/// also written there as a PNG file, for comparing with the board.
@Suite("RADE row on screen", .serialized)
@MainActor
struct RadeFlagShotTests {
    struct FlagShot: Sendable, CustomStringConvertible {
        let name: String
        let callsign: String
        let snr: Double?
        let synced: Bool?
        let offset: Double?
        let large: Bool
        var reason: String? = nil
        var description: String { "rade-flag-\(name)" }
    }

    nonisolated static let flagShots = [
        FlagShot(name: "receiving", callsign: "K1ABC", snr: 12, synced: true, offset: 38, large: false),
        FlagShot(name: "weak", callsign: "K1ABC", snr: 3, synced: true, offset: 52, large: false),
        FlagShot(name: "not-locked", callsign: "", snr: nil, synced: false, offset: nil, large: false),
        FlagShot(name: "older-core", callsign: "K1ABC", snr: 12, synced: nil, offset: nil, large: false),
        FlagShot(name: "large-type", callsign: "K1ABC", snr: 12, synced: true, offset: 38, large: true),
        FlagShot(name: "older-core-large-type", callsign: "", snr: nil, synced: nil, offset: nil, large: true),
        FlagShot(name: "reason", callsign: "", snr: nil, synced: false, offset: 0, large: false,
                 reason: RadeRowMirrorTests.modelFileWords),
        FlagShot(name: "reason-callsign", callsign: "K1ABC", snr: 12, synced: true, offset: 38, large: false,
                 reason: "RADE is not decoding on slice A. Change slice A to another mode and back to RADE to start it."),
        FlagShot(name: "reason-large-type", callsign: "", snr: nil, synced: false, offset: 0, large: true,
                 reason: RadeRowMirrorTests.modelFileWords),
    ]

    /// The phone's text size the board's large-type flag stands for.
    static let largeType = DynamicTypeSize.accessibility1

    final class Heights {
        var byId: [Int: CGFloat] = [:]
    }

    static func entry(_ shot: FlagShot) -> BandSlicesModel.Entry {
        let slice = BandSlice(id: 0, frequencyHz: 7_236_400, filterLowHz: -2_350, filterHighHz: -650,
                              colour: "#00D0FF", lowerSideband: true, txSlice: true)
        return BandSlicesModel.Entry(slice: slice, rxAntenna: "ANT1", txAntenna: "ANT1", modeLabel: "RADE-L",
                                     panKey: nil, signalDbm: -73, signalPeakDbm: nil, signalAverageDbm: nil,
                                     stepHz: 100, sampleRateHz: 192_000, locked: false, muted: false, band: nil,
                                     mode: 13,
                                     rade: RadeReception(callsign: shot.callsign, snrDb: shot.snr, synced: shot.synced,
                                                         offsetHz: shot.offset, reason: shot.reason))
    }

    @Test(arguments: flagShots)
    func theFlagShowsItsRow(_ shot: FlagShot) async throws {
        let size = CGSize(width: 240, height: 320)
        let heights = Heights()
        let entry = Self.entry(shot)
        let window = try BandFlagShotTests.window(size: size)
        let root = VfoFlagView(entry: entry, meter: BandFlagShotTests.catalogue()?.meters.sMeter,
                               heightChanged: { heights.byId[0] = $0 })
            .environment(\.dynamicTypeSize, shot.large ? Self.largeType : .large)
            .padding(20)
            .frame(width: size.width, height: size.height, alignment: .topLeading)
            .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        #expect(await ShotWait.until { heights.byId[0] != nil })
        let height = try #require(heights.byId[0])
        // The row makes the flag taller; the flag keeps its width.
        #expect(height > FlagLayout.flagSize.height)
        // The redrawn flag's rows are fixed: the RADE row's 2 and 16 points at every text size,
        // and an older Core's line allowed its wrap.
        if entry.rade?.wraps == true {
            // The older Core's line, or the Core's reason, wraps as it needs;
            // the flag grows with it.
            #expect(height >= VfoFlagView.expectedHeight(for: entry, large: shot.large) - 3)
        } else {
            #expect(abs(height - VfoFlagView.expectedHeight(for: entry, large: shot.large)) <= 3)
        }
        try Self.write(window, name: shot.description)
    }

    /// The board's two slices upright with A in RADE on an older Core: A's
    /// flag grows with its greyed row, and B's tag drops below the taller
    /// flag, at the usual text size and at large type.
    @Test(arguments: [false, true])
    func theBandStacksBelowTheTallerFlag(_ large: Bool) async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeL = try #require(catalog.modes.first { $0.label == "RADE-L" }.map { Int64($0.id) })
        let size = CGSize(width: 402, height: 640)
        let store = MirrorStore(send: { _ in })
        store.apply(Self.slice(0, hz: 7_236_400, mode: radeL, low: -2_350, high: -650, active: true))
        store.apply(Self.slice(1, hz: 7_249_000, mode: 0, low: -3_000, high: -100, active: false))
        let slices = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        let band = BandModel()
        band.catalog = catalog
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "RadeFlagShotTests") ?? .standard)

        let window = try BandFlagShotTests.window(size: size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: slices, settings: settings,
                             foreign: ForeignSlicesModel(store: MirrorStore(send: { _ in })))
        }
        .environment(\.dynamicTypeSize, large ? Self.largeType : .large)
        .frame(width: size.width, height: size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)

        #expect(slices.entries.first?.rade?.row == .olderCore)
        #expect(slices.entries.last?.rade == nil)
        // B folds under A's flag, which is taller than a plain flag: B's
        // triangle hangs from its tag, below the taller flag.
        let plainTagFoot = FlagLayout.flagSize.height + FlagLayout.stackGap + FlagLayout.foldedHeight
        #expect(await ShotWait.until {
            (band.markers.first { $0.sliceId == 1 }?.triangleTopPoints ?? 0) > plainTagFoot
        })
        await ShotWait.laidOut(window)
        try Self.write(window, name: "rade-band-older-core\(large ? "-large-type" : "")")
    }

    /// A RADE slice the Core says has no working decoder, on the phone's
    /// band upright, dark and light and at large type: A's flag reads
    /// `RADE ○ off` with the Core's reason under it and grows for it, and
    /// B's tag drops below the taller flag.
    @Test(arguments: [("dark", ColorScheme.dark, false), ("light", ColorScheme.light, false),
                      ("large-type", ColorScheme.dark, true)])
    func theBandShowsTheReason(_ name: String, _ scheme: ColorScheme, _ large: Bool) async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeL = try #require(catalog.modes.first { $0.label == "RADE-L" }.map { Int64($0.id) })
        let size = CGSize(width: 402, height: 640)
        let store = MirrorStore(send: { _ in })
        RadeRowMirrorTests.core(store, radeStatus: 1, radeReason: 1)
        store.apply(Self.slice(0, hz: 7_236_400, mode: radeL, low: -2_350, high: -650, active: true))
        store.apply(RadeRowMirrorTests.delta(0, [
            LinkMessage.PropertyEntry(ordinal: 151, name: "radeSynced", value: .bool(false)),
            RadeRowMirrorTests.reason(RadeRowMirrorTests.modelFileWords),
        ]))
        store.apply(Self.slice(1, hz: 7_249_000, mode: 0, low: -3_000, high: -100, active: false))
        let slices = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        let band = BandModel()
        band.catalog = catalog
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "RadeFlagShotTests") ?? .standard)

        let window = try BandFlagShotTests.window(size: size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: slices, settings: settings,
                             foreign: ForeignSlicesModel(store: MirrorStore(send: { _ in })))
        }
        .environment(\.dynamicTypeSize, large ? Self.largeType : .large)
        .preferredColorScheme(scheme)
        .frame(width: size.width, height: size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = scheme == .dark ? .dark : .light
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)

        #expect(await ShotWait.until {
            slices.entries.first?.rade?.row == .off(prefix: "K1ABC", reason: RadeRowMirrorTests.modelFileWords)
        })
        let plainTagFoot = FlagLayout.flagSize.height + FlagLayout.stackGap + FlagLayout.foldedHeight
        #expect(await ShotWait.until {
            (band.markers.first { $0.sliceId == 1 }?.triangleTopPoints ?? 0) > plainTagFoot
        })
        await ShotWait.laidOut(window)
        try Self.write(window, name: "rade-band-reason-\(name)")
    }

    // MARK: Inside

    static func slice(_ index: Int, hz: Double, mode: Int64, low: Int64, high: Int64, active: Bool) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(hz)),
            LinkMessage.PropertyEntry(ordinal: 2, name: "dspMode", value: .enumeration(mode)),
            LinkMessage.PropertyEntry(ordinal: 3, name: "filterLow", value: .i64(low)),
            LinkMessage.PropertyEntry(ordinal: 4, name: "filterHigh", value: .i64(high)),
            LinkMessage.PropertyEntry(ordinal: 6, name: "stepHz", value: .i64(100)),
            LinkMessage.PropertyEntry(ordinal: 9, name: "rxAntenna", value: .utf8("ANT1")),
            LinkMessage.PropertyEntry(ordinal: 10, name: "txAntenna", value: .utf8("ANT1")),
            LinkMessage.PropertyEntry(ordinal: 11, name: "active", value: .bool(active)),
            LinkMessage.PropertyEntry(ordinal: 12, name: "txSlice", value: .bool(index == 0)),
            LinkMessage.PropertyEntry(ordinal: 13, name: "sliceIndex", value: .i64(Int64(index))),
            LinkMessage.PropertyEntry(ordinal: 15, name: "signalStrengthDbm", value: .f64(-73 - Double(index) * 12)),
            LinkMessage.PropertyEntry(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            LinkMessage.PropertyEntry(ordinal: 142, name: "snrDb", value: .f64(12)),
            LinkMessage.PropertyEntry(ordinal: 143, name: "lastRadeRxCallsign", value: .utf8("K1ABC")),
        ]))
    }

    static func write(_ window: UIWindow, name: String) throws {
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_BAND_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
