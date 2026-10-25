// NereusSDR for iOS: tests of the iPad's Roundtable applet: its place in the column, the pinned box, the narrow columns, the swipes' iPad defaults and its pictures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 19, the iPad applet's tests.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// The iPad's Roundtable applet (spec 11.4). With `NEREUS_MAIN_SHOTS` set
/// (through `TEST_RUNNER_NEREUS_MAIN_SHOTS`), its pictures are written
/// there at the iPad Pro 13-inch's size on its side and upright.
@MainActor
@Suite("Roundtable applet", .serialized)
struct RoundtableAppletTests {
    private let platform = TestPlatform()

    private func defaults() throws -> UserDefaults {
        try #require(UserDefaults(suiteName: "RoundtableAppletTests-\(UUID().uuidString)"))
    }

    // MARK: Swipes

    @Test("on an iPad both edges start Off and the tab bar widens Roundtable; a phone keeps its panels")
    func iPadSwipeDefaults() throws {
        let pad = PhoneSettings(defaults: try defaults(), isPad: true)
        #expect(pad.swipeTarget(.leftEdge) == .off)
        #expect(pad.swipeTarget(.rightEdge) == .off)
        #expect(pad.swipeTarget(.tabBar) == .roundtableMine)
        let phone = PhoneSettings(defaults: try defaults(), isPad: false)
        #expect(phone.swipeTarget(.leftEdge) == .rxPanel)
        #expect(phone.swipeTarget(.rightEdge) == .txPanel)
        #expect(phone.swipeTarget(.tabBar) == .roundtableMine)
        #expect(SwipeShortcuts.defaultTarget(.leftEdge, iPad: true) == .off)
        #expect(SwipeShortcuts.defaultTarget(.leftEdge) == .rxPanel)
    }

    @Test("an iPad's Setup choice turns an edge on, saved on that device, and Reset puts it back Off")
    func iPadSwipeChoicesPersist() throws {
        let store = try defaults()
        let pad = PhoneSettings(defaults: store, isPad: true)
        pad.setSwipeTarget(.rxPanel, for: .leftEdge)
        #expect(PhoneSettings(defaults: store, isPad: true).swipeTarget(.leftEdge) == .rxPanel)
        #expect(pad.swipesChanged)
        pad.resetSwipes()
        #expect(PhoneSettings(defaults: store, isPad: true).swipeTarget(.leftEdge) == .off)
        #expect(!pad.swipesChanged)
    }

    @Test("Setup's Reset row names each device's own defaults")
    func swipeDefaultsWords() throws {
        let pad = PhoneSettings(defaults: try defaults(), isPad: true)
        let phone = PhoneSettings(defaults: try defaults(), isPad: false)
        #expect(pad.swipeDefaultsText.hasPrefix(SwipeTarget.off.title))
        #expect(phone.swipeDefaultsText.hasPrefix(SwipeTarget.rxPanel.title))
        #expect(pad.swipeDefaultsText != phone.swipeDefaultsText)
    }

    // MARK: The column's order

    @Test("Roundtable sits second after the S-Meter, moves anywhere, and the others keep their order")
    func appletOrder() {
        #expect(AppletOrder.order(roundtableAt: AppletOrder.roundtableDefault) == [.sMeter, .roundtable, .rx, .tx])
        #expect(AppletOrder.order(roundtableAt: 0) == [.roundtable, .sMeter, .rx, .tx])
        #expect(AppletOrder.order(roundtableAt: 3) == [.sMeter, .rx, .tx, .roundtable])
        #expect(AppletOrder.order(roundtableAt: -4) == [.roundtable, .sMeter, .rx, .tx])
        #expect(AppletOrder.order(roundtableAt: 9) == [.sMeter, .rx, .tx, .roundtable])
    }

    @Test("the applet's place is saved on the device and read back clamped")
    func appletPlaceSaved() throws {
        let store = try defaults()
        let settings = PhoneSettings(defaults: store, isPad: true)
        #expect(settings.roundtableAppletPosition == 1)
        settings.roundtableAppletPosition = 3
        #expect(PhoneSettings(defaults: store, isPad: true).roundtableAppletPosition == 3)
        settings.roundtableAppletPosition = 12
        #expect(PhoneSettings(defaults: store, isPad: true).roundtableAppletPosition == 3)
        store.set(-2, forKey: PhoneSettings.keyPrefix + PhoneSettings.roundtableAppletPositionKey)
        #expect(PhoneSettings(defaults: store, isPad: true).roundtableAppletPosition == 0)
    }

    // MARK: The pinned box

    @Test("the box is pinned while 60 points or more of the applet are in view")
    func boxPinned() {
        let height: CGFloat = 900
        func pinned(_ minY: CGFloat, _ maxY: CGFloat) -> Bool {
            RoundtableAppletFoot.boxPinned(applet: CGRect(x: 0, y: minY, width: 320, height: maxY - minY),
                                           viewportHeight: height)
        }
        #expect(pinned(260, 1_400))
        #expect(pinned(-1_000, 300))
        #expect(pinned(840, 1_400))
        #expect(!pinned(860, 1_400))
        #expect(!pinned(-1_400, 50))
        #expect(!pinned(1_000, 1_600))
        #expect(!RoundtableAppletFoot.boxPinned(applet: .zero, viewportHeight: height))
        #expect(!RoundtableAppletFoot.boxPinned(applet: CGRect(x: 0, y: 0, width: 320, height: 600),
                                                viewportHeight: 0))
    }

    @Test("the new pill counts arrivals below a reader up off the end, and reads the end only once something waits")
    func newPillCount() {
        var endRead = 0
        func atEnd(_ value: Bool) -> Bool {
            endRead += 1
            return value
        }
        #expect(RoundtableRoomPills.newCount(waiting: 3, atEnd: atEnd(false), full: true) == 3)
        #expect(RoundtableRoomPills.newCount(waiting: 3, atEnd: atEnd(true), full: true) == nil)
        #expect(endRead == 2)
        // Nothing waits, or half height: the end marker's place is not read, so a room
        // body does not lay its scroll out again each time the marker comes and goes.
        #expect(RoundtableRoomPills.newCount(waiting: 0, atEnd: atEnd(false), full: true) == nil)
        #expect(RoundtableRoomPills.newCount(waiting: 3, atEnd: atEnd(false), full: false) == nil)
        #expect(endRead == 2)
    }

    // MARK: The narrow columns

    @Test("the narrow readings fit the 320-point column: every column holds its widest value")
    func narrowColumnsFit() {
        let columns = RoundtableReadingColumns.narrow
        #expect(!columns.bar)
        let station = columns.stationWidth(rowWidth: AppletColumn.width)
        func width(_ text: String, _ font: UIFont) -> CGFloat {
            ceil((text as NSString).size(withAttributes: [.font: font]).width)
        }
        let digits = UIFont.monospacedDigitSystemFont(ofSize: 12, weight: .regular)
        let bold = UIFont.monospacedDigitSystemFont(ofSize: 12, weight: .bold)
        let call = UIFont.monospacedSystemFont(ofSize: 12, weight: .bold)
        let age = UIFont.monospacedDigitSystemFont(ofSize: 11, weight: .bold)
        let tag = UIFont.systemFont(ofSize: 11, weight: .bold)
        #expect(width("12,345", digits) <= columns.km)
        #expect(width(RoundtableText.offsetText(-1_200, unit: columns.hzUnit), digits) <= columns.offset)
        #expect(width(RoundtableText.offsetText(1_200, unit: columns.hzUnit), digits) <= columns.offset)
        #expect(RoundtableText.offsetText(-1_200, unit: false) == "-1200")
        #expect(RoundtableText.offsetText(-1_200) == "-1200 Hz")
        let header = UIFont.systemFont(ofSize: 11)
        #expect(width("km", header) <= columns.km && width("Offset", header) <= columns.offset)
        #expect(width("Reading", header) <= columns.reading && width("Age", header) <= columns.age)
        #expect(width(RoundtableText.sText(-20), bold) <= 46 && columns.reading >= 46)
        #expect(width(RoundtableText.ageText(59_000), age) <= columns.age)
        #expect(width(RoundtableText.ageText(3_540_000), age) <= columns.age)
        // A six-character call and its AWAY tag.
        #expect(width("AA5ABC", call) + 4 + width("AWAY", tag) + 6 <= station)
        // The phone's columns still fit its own room.
        #expect(RoundtableReadingColumns.regular.stationWidth(rowWidth: 402) >= station)
    }

    // MARK: Pictures

    @Test("shots: the applet, widened, keyed, the first-run line, upright and with the column hidden")
    func shots() async throws {
        let side = CGSize(width: 1376, height: 1032)
        let upright = CGSize(width: 1032, height: 1376)
        do {
            let (model, station) = try await shotStation()
            model.phoneSettings.roundtableFirstRunNoteSeen = true
            try await station.deliverRoundtable(RoundtableSheetTests.boardScene())
            #expect(await settle { model.roundtable.room != nil && model.roundtable.available })
            try await shoot("ipad-01-applet", model: model, size: side)
            try await shoot("ipad-02-widened", model: model, size: side, wide: true)
            model.main.transmit.tapPtt()
            #expect(await settle { model.main.transmit.ptt.transmitting })
            try await shoot("ipad-03-keyed", model: model, size: side)
            model.main.transmit.tapPtt()
            #expect(await settle { model.main.transmit.ptt.state == .idle })
            try await shoot("ipad-04-column-hidden-sheet", model: model, size: side, columnShown: false,
                            detent: .half)
            try await shoot("ipad-05-upright-sheet", model: model, size: upright, detent: .half)
            await model.disconnect()
        }
        do {
            let (model, station) = try await shotStation()
            var scene = RoundtableSheetTests.boardScene()
            scene.firstRunAnswered = false
            try await station.deliverRoundtable(scene)
            #expect(await settle { model.roundtable.room != nil && model.roundtable.askingFirstRun })
            try await shoot("ipad-06-first-run-line", model: model, size: side)
            await model.disconnect()
        }
    }

    // MARK: Helpers

    private func shotStation() async throws -> (AppModel, FakeStation) {
        let store = try defaults()
        let model = AppModel(phoneSettings: PhoneSettings(defaults: store, isPad: true),
                             displaySettings: BandDisplaySettingsStore(defaults: store),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: [.remoteTx, .roundtable])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await settle { model.main.transmit.permitted && model.connection == .connected })
        try await MainScreenShotTests.fill(station)
        #expect(await settle { model.main.slices.entries.count == 2 })
        model.main.band.endpointId = 1
        model.main.band.receive(.context(try #require(BandFlagShotTests.context())))
        return (model, station)
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    private func shoot(_ name: String, model: AppModel, size: CGSize, columnShown: Bool = true,
                       wide: Bool = false, detent: RoundtableSheet.Detent? = nil) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = AppletShotRoot(model: model, columnShown: columnShown, wide: wide, detent: detent)
        let host = ShotHosting.host(root.preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 2))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        try await Task.sleep(for: .milliseconds(400))
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("roundtable-\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root on an iPad, as `RootView` lays it out.
private struct AppletShotRoot: View {
    @ObservedObject var model: AppModel
    let columnShown: Bool
    let wide: Bool
    let detent: RoundtableSheet.Detent?

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, columnShown: columnShown, roundtableSheet: detent,
                           roundtableWide: wide)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height, iPad: true)
            }
        }
        .environment(\.horizontalSizeClass, .regular)
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
