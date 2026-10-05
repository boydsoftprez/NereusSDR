// NereusSDR for iOS: the band's WIDE chip: shown only while the first input is not Filtered, clear of the flags, its tap opens the More menu
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

/// R-IOS-18, R-IOS-11 (JJ's board `#widechip-review`, approved 2026-09-30):
/// the amber WIDE chip shows on the band only while the Core reports the
/// first receiver input as not Filtered; it sits at the top right, left of
/// the dBm scale, and moves below a flag that hangs there, clear of every
/// flag, the band-plan strip and the frequency scale, on the iPhone 17's
/// and the Pro Max's upright bands in regular and large type; VoiceOver
/// finds it as a button with the board's words, and a tap opens the active
/// slice's More menu under it, with the Filter policy lines. With
/// `NEREUS_BAND_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_BAND_SHOTS`), each board frame is written there as
/// `widechip-<state>-<device>-<look>.png`, on the band of the simulator the
/// tests run on.
@Suite("The WIDE chip on the band", .serialized)
@MainActor
struct WideChipTests {
    nonisolated static let wideReason = "BYPASS (slice B on WWV has no filter pins)"
    nonisolated static let highPassWords = "The broadcast-band high-pass filter is off because slice A on 160m needs it off."
    nonisolated static let centreHz = 7_245_000.0
    /// The iPhone 17's and the Pro Max's upright bands, between the toolbar and the tab bar.
    nonisolated static let iPhone17Band = CGSize(width: 402, height: 640)
    nonisolated static let proMaxBand = CGSize(width: 440, height: 738)
    /// The iPhone 17's band sideways.
    nonisolated static let sidewaysBand = CGSize(width: 874, height: 330)

    /// The board's states.
    enum State: String, CaseIterable, Sendable {
        case filtered = "filtered-no-chip"
        case oneFlag = "one-flag"
        case topRight = "flag-top-right"
        case menu = "menu-from-chip"

        var wide: Bool { self != .filtered }
        var flagAtTopRight: Bool { self == .topRight }
    }

    struct Look: Sendable, CustomStringConvertible {
        let light: Bool
        let large: Bool
        var description: String { (light ? "light" : "dark") + (large ? "-large" : "-regular") }
    }

    struct Frame: Sendable, CustomStringConvertible {
        let state: State
        let look: Look
        var description: String { "\(state.rawValue)-\(look)" }
    }

    nonisolated static let looks = [false, true].flatMap { light in [false, true].map { Look(light: light, large: $0) } }
    nonisolated static let frames = State.allCases.flatMap { state in looks.map { Frame(state: state, look: $0) } }

    // MARK: When it shows

    @Test("it shows only while the Core reports the first input as not Filtered, and nothing without a state")
    func showsOnlyWhileNotFiltered() {
        #expect(!WideChip.shows(nil))
        #expect(!WideChip.shows(ReceiveFilterState(effective: .filtered, reason: "40m")))
        #expect(WideChip.shows(ReceiveFilterState(effective: .bypass, reason: Self.wideReason)))
        #expect(WideChip.shows(ReceiveFilterState(effective: .widebandLocked, reason: "BYPASS (wideband active)")))
    }

    @Test("on the band it comes and goes with the Core's filter state")
    func comesAndGoesWithTheFilterState() async throws {
        try await withBand(size: Self.proMaxBand, large: false, effective: 0, aHz: Self.hz(0.18, width: 440)) { rig in
            #expect(rig.controls.wideChipRect == nil)
            rig.mirror.apply(Self.effective(1))
            #expect(await ShotWait.until { rig.controls.wideChipRect != nil })
            rig.mirror.apply(Self.effective(0))
            #expect(await ShotWait.until { rig.controls.wideChipRect == nil })
            rig.mirror.apply(Self.effective(2))
            #expect(await ShotWait.until { rig.controls.wideChipRect != nil })
        }
    }

    // MARK: Where it sits

    @Test("one flag low in the band: the chip at the top right, left of the dBm scale",
          arguments: [WideChipTests.iPhone17Band, WideChipTests.proMaxBand], [false, true])
    func topRightBesideTheScale(_ size: CGSize, _ large: Bool) async throws {
        try await withBand(size: size, large: large, effective: 1, aHz: Self.hz(0.18, width: size.width)) { rig in
            let chip = try #require(await rig.chip())
            let target = WideChip.targetSize(large: large)
            #expect(chip.size == target)
            #expect(chip.width >= 44 && chip.height >= 44)
            #expect(chip.minY == WideChip.gap)
            #expect(chip.maxX == rig.layout.dbmScale.minX - WideChip.gap)
            try rig.expectClear(chip)
        }
    }

    @Test("a flag at the top right: the chip moves below it, above the band-plan strip",
          arguments: [WideChipTests.iPhone17Band, WideChipTests.proMaxBand], [false, true])
    func belowATopRightFlag(_ size: CGSize, _ large: Bool) async throws {
        try await withBand(size: size, large: large, effective: 1, aHz: Self.topRightHz(width: size.width)) { rig in
            let chip = try #require(await rig.chip())
            let flags = rig.main.slices.flagRects
            let flag = try #require(flags.first)
            // The flag hangs in the top right, over where the chip starts.
            #expect(WideChip.overlaps(flag, CGRect(origin: CGPoint(x: rig.layout.dbmScale.minX - 2 - chip.width, y: 2),
                                                   size: chip.size)))
            #expect(chip.minY >= flag.maxY + WideChip.gap)
            #expect(chip.width >= 44 && chip.height >= 44)
            try rig.expectClear(chip)
        }
    }

    @Test("with the frames a second shown: the chip moves down below the readout, clear of it",
          arguments: [WideChipTests.iPhone17Band, WideChipTests.proMaxBand], [false, true])
    func belowTheFramesASecond(_ size: CGSize, _ large: Bool) async throws {
        let wasOn = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(wasOn) }
        try await withBand(size: size, large: large, effective: 1, aHz: Self.hz(0.18, width: size.width),
                           fps: true) { rig in
            #expect(await ShotWait.until { rig.main.band.framesPerSecond != nil })
            let readout = BandReadouts.fpsRect(layout: rig.layout)
            // The readout as drawn lies inside the room kept for it at its widest.
            var found: NSObject?
            #expect(await ShotWait.until {
                found = SetupTypedEntryTests.element("fpsReadout", in: rig.window)
                return found != nil
            })
            let drawn = try #require(found).accessibilityFrame
            #expect(readout.insetBy(dx: -0.5, dy: -0.5).contains(drawn), "readout \(drawn) outside \(readout)")
            #expect(await ShotWait.until {
                rig.controls.wideChipRect.map { !WideChip.overlaps($0, readout) } == true
            })
            let chip = try #require(rig.controls.wideChipRect)
            #expect(!WideChip.overlaps(chip, readout), "chip \(chip) over the readout \(readout)")
            // Down the right edge, just below it, as the board folds it below a flag.
            #expect(chip.minY >= readout.maxY + WideChip.gap)
            #expect(chip.maxX == rig.layout.dbmScale.minX - WideChip.gap)
            #expect(chip.width >= 44 && chip.height >= 44)
            try rig.expectClear(chip)
            await ShotWait.laidOut(rig.window)
            let device = size == Self.proMaxBand ? "18promax" : "iphone17"
            try RadeFlagShotTests.write(rig.window,
                                        name: "cleanup-widechip-fps-\(device)-dark-\(large ? "large" : "regular")")
        }
    }

    @Test("on another slice's band: clear of the Back to your band bar, the flag under it and an edge marker")
    func clearOfTheBackBarAndMarkers() {
        let layout = BandLayout(size: Self.iPhone17Band, scale: 1, stripPoints: 20)
        let bar = JumpBar.rect(sideways: false, large: true)
        let flag = CGRect(x: 150, y: JumpBar.clearance, width: 200, height: 160)
        let column = CGRect(x: 354, y: JumpBar.clearance, width: 46, height: 140)
        let marker = CGRect(x: 218, y: layout.spectrum.maxY - 52, width: 180, height: 48)
        let avoid = [bar, flag, column, marker]
        let chip = WideChip.rect(layout: layout, avoid: avoid, sideways: false, trailingInset: 0, large: true)
        for other in avoid {
            #expect(!WideChip.overlaps(chip, other), "chip \(chip) over \(other)")
        }
        #expect(chip.maxY <= layout.strip.minY)
        #expect(chip.maxX <= layout.dbmScale.minX)
        #expect(chip.minX >= 0)
    }

    // MARK: What a tap does

    @Test("VoiceOver finds a button with the board's words; a tap opens the active slice's More menu under it",
          arguments: [WideChipTests.iPhone17Band, WideChipTests.proMaxBand])
    func aTapOpensTheMoreMenu(_ size: CGSize) async throws {
        let wasOn = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(wasOn) }
        try await withBand(size: size, large: false, effective: 1, aHz: Self.hz(0.18, width: size.width)) { rig in
            let chip = try #require(await rig.chip())
            var found: NSObject?
            #expect(await ShotWait.until {
                found = SetupTypedEntryTests.element(WideChip.identifier, in: rig.window)
                return found != nil
            })
            let element = try #require(found)
            #expect(element.accessibilityLabel == WideChip.accessibilityLabel)
            #expect(element.accessibilityHint == WideChip.accessibilityHint)
            #expect(element.accessibilityTraits.contains(.button))
            let frame = element.accessibilityFrame
            #expect(frame.width >= 44 && frame.height >= 44)
            #expect(abs(frame.minX - chip.minX) < 1 && abs(frame.minY - chip.minY) < 1)

            #expect(element.accessibilityActivate())
            let controls = rig.controls
            #expect(await ShotWait.until { controls.isOpen(.more(0)) && controls.moreFromWideChip })
            var menu: NSObject?
            #expect(await ShotWait.until {
                menu = SetupTypedEntryTests.element("flagMenu", in: rig.window)
                return menu != nil
            })
            let menuFrame = try #require(menu).accessibilityFrame
            // Under the chip, its right edge on the chip's, as the board draws it.
            #expect(abs(menuFrame.minY - (chip.maxY + FlagPopoverLayer.belowButton)) < 1)
            #expect(abs(menuFrame.maxX - chip.maxX) < 1)
            let lines = SetupTypedEntryTests.element("flagFilterState", in: rig.window)?.accessibilityLabel ?? ""
            #expect(lines.contains("Effective: BYPASS"))
            #expect(lines.contains("Reason: " + Self.wideReason))
            #expect(lines.contains("Shared input: " + Self.highPassWords))

            // A second tap closes it.
            controls.toggleFromWideChip()
            #expect(controls.open == nil && !controls.moreFromWideChip)
        }
    }

    @Test("the chip and the flag's more button share one menu: each moves it under itself, a second tap closes")
    func chipAndMoreButtonShareTheMenu() async throws {
        try await withBand(size: Self.iPhone17Band, large: false, effective: 1,
                           aHz: Self.hz(0.18, width: 402)) { rig in
            let controls = rig.controls
            controls.toggle(.more(0))
            #expect(controls.isOpen(.more(0)) && !controls.moreFromWideChip)
            controls.toggleFromWideChip()
            #expect(controls.isOpen(.more(0)) && controls.moreFromWideChip)
            controls.toggle(.more(0))
            #expect(controls.isOpen(.more(0)) && !controls.moreFromWideChip)
            controls.toggle(.more(0))
            #expect(controls.open == nil)
            controls.toggleFromWideChip()
            #expect(controls.isOpen(.more(0)) && controls.moreFromWideChip)
            controls.close()
            #expect(controls.open == nil && !controls.moreFromWideChip)
        }
    }

    // MARK: The board's frames

    @Test(arguments: frames)
    func boardFrames(_ frame: Frame) async throws {
        let device = Self.deviceBand()
        let width = device.size.width
        let aHz = frame.state.flagAtTopRight ? Self.topRightHz(width: width) : Self.hz(0.18, width: width)
        try await withBand(size: device.size, large: frame.look.large, light: frame.look.light,
                           effective: frame.state.wide ? 1 : 0, aHz: aHz) { rig in
            if frame.state.wide {
                let chip = try #require(await rig.chip())
                try rig.expectClear(chip)
            } else {
                #expect(rig.controls.wideChipRect == nil)
            }
            if frame.state == .menu {
                rig.controls.toggleFromWideChip()
                #expect(await ShotWait.until { rig.controls.isOpen(.more(0)) && rig.controls.moreFromWideChip })
            }
            await ShotWait.laidOut(rig.window)
            try RadeFlagShotTests.write(rig.window, name: "widechip-\(frame.state.rawValue)-\(device.name)-\(frame.look)")
        }
    }

    @Test("sideways, a flag in the top right: the chip beside it, clear of it")
    func sidewaysFrame() async throws {
        let size = Self.sidewaysBand
        try await withBand(size: size, large: false, sideways: true, effective: 1,
                           aHz: Self.topRightHz(width: size.width)) { rig in
            let chip = try #require(await rig.chip())
            try rig.expectClear(chip)
            #expect(chip.minX >= EdgeMarkers.sidewaysInset)
            await ShotWait.laidOut(rig.window)
            try RadeFlagShotTests.write(rig.window, name: "widechip-sideways-iphone17-dark-regular")
        }
    }

    // MARK: The band

    @MainActor
    struct Rig {
        let mirror: MirrorStore
        let main: MainScreenModel
        let window: UIWindow
        let layout: BandLayout
        var controls: FlagControls { main.flagControls }

        /// The chip's target once it is on the band.
        func chip() async -> CGRect? {
            var rect: CGRect?
            _ = await ShotWait.until {
                rect = controls.wideChipRect
                return rect != nil
            }
            return rect
        }

        /// Clear of every flag with its buttons, the dBm scale and the band-plan strip, inside the band.
        func expectClear(_ chip: CGRect) throws {
            for flag in main.slices.flagRects {
                #expect(!WideChip.overlaps(chip, flag), "chip \(chip) over flag \(flag)")
            }
            #expect(chip.maxX <= layout.dbmScale.minX)
            #expect(chip.maxY <= layout.strip.minY)
            #expect(chip.minX >= 0 && chip.minY >= 0)
        }
    }

    /// The band with slice A at `aHz` and the Core's filter state for the
    /// first input, hosted on screen, through `body`.
    private func withBand(size: CGSize, large: Bool, light: Bool = false, sideways: Bool = false, effective: Int64,
                          aHz: Double, fps: Bool = false, _ body: (Rig) async throws -> Void) async throws {
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
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "radio", className: "RadioModel", properties: [
            .init(ordinal: 11, name: "rxFilter0Mode", value: .i64(0)),
            .init(ordinal: 12, name: "rxFilter0Effective", value: .i64(effective)),
            .init(ordinal: 13, name: "rxFilter0Band", value: .i64(5)),
            .init(ordinal: 14, name: "rxFilter0Reason", value: .utf8(effective == 0 ? "40m" : Self.wideReason)),
            .init(ordinal: 35, name: "rxFilter0LowPassReason", value: .utf8(effective == 0 ? "" : Self.highPassWords)),
            .init(ordinal: 36, name: "rxFilter0LowPassSlice", value: .i64(-1)),
        ])))
        mirror.apply(FlagButtonsShotTests.slice(0, hz: aHz, active: true))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        let band = main.band
        band.endpointId = 1
        if fps {
            // The frames a second, shown and counted on a clock a frame's
            // time on at every reading: never the wall clock.
            band.settings.showFps = true
            let clock = SteppedClock()
            band.clock = { clock.next() }
        }
        let context = try #require(FilterReasonShotTests.context(centreHz: Self.centreHz))
        band.receive(.context(context))
        #expect(await ShotWait.until { main.slices.entries.count == 1 && main.slices.catalog != nil })

        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "WideChipTests") ?? .standard)
        let window = try BandFlagShotTests.window(size: size)
        let root = ZStack(alignment: .topLeading) {
            BandView(model: band)
            BandGestureLayer(band: band, slices: main.slices, settings: settings, tuning: main.tuning,
                             foreign: main.foreign, sideways: sideways, flagControls: main.flagControls,
                             transmitReason: nil)
            DbmScaleArrows(display: main.display, band: band)
            if fps {
                BandReadouts(band: band)
            }
        }
        .environment(\.dynamicTypeSize, large ? RadeFlagShotTests.largeType : .large)
        .frame(width: size.width, height: size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = light ? .light : .dark
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
        #expect(await ShotWait.until { !main.slices.flagRects.isEmpty })
        await ShotWait.laidOut(window)
        let layout = try #require(band.pointGeometry(size: size)?.layout)
        try await body(Rig(mirror: mirror, main: main, window: window, layout: layout))
    }

    /// A frequency `share` of the way across a band `width` points wide.
    static func hz(_ share: Double, width: CGFloat) -> Double {
        centreHz - FilterReasonShotTests.span / 2 + FilterReasonShotTests.span * share
    }

    /// Slice A's line 50 points from the band's right edge, so its flag
    /// hangs in the top right (the board's).
    static func topRightHz(width: CGFloat) -> Double {
        hz(1 - 50 / Double(width), width: width)
    }

    /// A band clock that moves on a thirtieth of a second at every reading.
    final class SteppedClock {
        private var now = 100.0

        func next() -> Double {
            now += 1.0 / 30
            return now
        }
    }

    static func effective(_ value: Int64) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 12, name: "rxFilter0Effective", value: .i64(value)),
        ]))
    }

    /// The band of the simulator the tests run on, upright: the Pro Max's
    /// on a 440-point-wide screen, the iPhone 17's otherwise.
    static func deviceBand() -> (name: String, size: CGSize) {
        let width = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first?.screen.bounds.width
        if let width, width >= 440 {
            return ("18promax", proMaxBand)
        }
        return ("iphone17", iPhone17Band)
    }
}
