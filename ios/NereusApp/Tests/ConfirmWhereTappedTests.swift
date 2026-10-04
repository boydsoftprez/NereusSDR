// NereusSDR for iOS: the Core's question over whatever screen is showing, and the Tuner Genius's antennas on the TX panel
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// D85: the real screens against a fake Core that owns a Tuner Genius. The
/// Core's question (`confirm.request`) arrives while the Tuner Genius page,
/// the TX panel, Modes or Setup is showing, and appears over it once, in a
/// window of its own that takes the touches over the screen; Set ANT1 sends
/// one `confirm.proceed`. A tab change leaves it up. The TX panel's tuner
/// row carries ANT 1 to 3 with the operator's names, lit from the Core's
/// `antennaA`, and sends `setTgxlAntenna`. The taps themselves are the UI
/// tests' (``ConfirmWhereTappedUITests``). With `NEREUS_MAIN_SHOTS` set to a
/// directory (through `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the screens are
/// written there.
@Suite("The Core's question where it was asked", .serialized)
@MainActor
struct ConfirmWhereTappedTests {
    // MARK: The question over each screen

    @Test("over the Tuner Genius page the question shows, takes the touch, and Set ANT1 sends one confirm.proceed")
    func overTunerGeniusPage() async throws {
        try await askOver("confirm-over-tuner-genius", tab: .radio) { model, flow in
            RadioView(app: model, main: model.main, flow: flow, accessoryRoute: [.page(.tunerGenius)])
        }
    }

    @Test("over the TX panel the question shows, takes the touch, and Set ANT1 sends one confirm.proceed")
    func overTxPanel() async throws {
        try await askOver("confirm-over-tx-panel", tab: .panadapter) { model, _ in
            MainScreen(app: model, main: model.main, txPanelOpen: true)
        }
    }

    @Test("over Modes the question shows, takes the touch, and Set ANT1 sends one confirm.proceed")
    func overModes() async throws {
        try await askOver("confirm-over-modes", tab: .modes) { model, flow in
            ModesTab(app: model, main: model.main, flow: flow)
        }
    }

    @Test("over Setup the question shows, takes the touch, and Set ANT1 sends one confirm.proceed")
    func overSetup() async throws {
        try await askOver("confirm-over-setup", tab: .setup) { model, flow in
            SetupTab(app: model, flow: flow, router: SetupRouter())
        }
    }

    @Test("the app's root asks the question over the tab showing, from the tab bar")
    func fromTheRoot() async throws {
        let (model, station) = try await connected()
        let screen = try await Screen(model: model, station: station) { flow in
            RootView(selection: .setup)
                .environmentObject(model)
                .environmentObject(flow)
        }
        defer { screen.close() }
        #expect(!screen.questionShowing)
        await station.deliver(Self.tunerAntennaQuestion(id: 51))
        #expect(await screen.settle { screen.questionShowing })
        #expect(ConfirmationWindows.all(over: screen.window).count == 1)
        model.devices.cancel()
        #expect(await screen.settle { !screen.questionShowing })
        await model.disconnect()
    }

    @Test("a tab change leaves the question up over the new tab, and it is still answered there")
    func survivesATabChange() async throws {
        let (model, station) = try await connected()
        let tabs = Tabs()
        let screen = try await Screen(model: model, station: station) { flow in
            TabsRoot(model: model, flow: flow, tabs: tabs)
        }
        defer { screen.close() }
        await station.deliver(Self.tunerAntennaQuestion(id: 41))
        #expect(await screen.settle { screen.questionShowing })
        let over = try #require(screen.questionWindow)
        for tab in [AppTab.setup, .modes, .radio, .panadapter] {
            tabs.selection = tab
            try await LinkBarrier.roundTrip(model.commands)
            await ShotWait.laidOut(screen.window)
            #expect(model.devices.question?.id == 41)
            #expect(screen.questionShowing, "the question went with the change to \(tab.title)")
            #expect(ConfirmationWindows.all(over: screen.window).count == 1)
            #expect(screen.questionWindow === over)
        }
        try await screen.shoot("confirm-after-tab-change")
        model.devices.cancel()
        let cancel = await station.waitForMessage(within: .seconds(5)) { Self.invoke($0)?.verb == "confirm.cancel" }
        #expect(cancel.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "id", value: .i64(41))])
        #expect(await screen.settle { !screen.questionShowing })
        await model.disconnect()
    }

    @Test("with no question up, the question's window is hidden, and a question it does not ask leaves it hidden")
    func hiddenWithoutAQuestion() async throws {
        let (model, station) = try await connected()
        let screen = try await Screen(model: model, station: station) { flow in
            RootView(selection: .panadapter)
                .environmentObject(model)
                .environmentObject(flow)
        }
        defer { screen.close() }
        let over = try #require(screen.questionWindow)
        #expect(over.isHidden)
        #expect(over.windowLevel.rawValue > screen.window.windowLevel.rawValue)
        #expect(over.frame == screen.window.frame)
        // A kind of question this phone does not know is never this window's.
        await station.deliver(.confirmRequest(LinkMessage.ConfirmRequest(
            id: 42, kind: "somethingNew", reason: SeveralDevices.waitingReason, affected: [], expiresInMs: 60_000,
            forCommandId: 900)))
        #expect(await screen.settle { model.devices.question?.id == 42 })
        try await LinkBarrier.roundTrip(model.commands)
        await ShotWait.laidOut(screen.window)
        #expect(over.isHidden)
        await model.disconnect()
    }

    // MARK: The TX panel's antennas

    @Test("the TX panel's tuner row lights the Core's antenna, with the operator's names, and sends setTgxlAntenna")
    func txPanelAntennas() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        let screen = try await Screen(model: model, station: station) { _ in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, txPanelOpen: true)
                TabBar(selection: .constant(.panadapter), sideways: false)
            }
        }
        defer { screen.close() }

        // The Core reports ANT 3 as 2: only ANT 3 is lit.
        await station.deliver(FakeStation.accessoryDelta("tuner", "TunerModel", [("antennaA", .i64(2))]))
        #expect(await settle { accessories.tunerGenius?.antenna == 3 })
        let three = try #require(accessories.tunerGenius)
        #expect((1...3).filter { TunerAntennaRow.antennaLit(three, port: $0) } == [3])
        // The operator's names, as on the Tuner Genius page.
        #expect(accessories.records?.tunerLabels == ["Beam", "Vertical", "Dipole"])
        #expect(accessories.tunerAntennaReason == nil)
        #expect(try Self.antennaColours(accessories) == [false, false, true])
        // The TX panel's tuner row, with ANT 3 lit.
        try await screen.shoot("tx-panel-antenna-row")

        // ANT 1 sends setTgxlAntenna with port 1 through the page's call.
        accessories.setTunerAntenna(1)
        let sent = await station.waitForMessage(within: .seconds(5)) { Self.invoke($0)?.verb == "setTgxlAntenna" }
        #expect(sent.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "port", value: .i64(1))])
        #expect(await settle { accessories.tunerGenius?.antenna == 1 })
        #expect(await screen.settle { (try? Self.antennaColours(accessories)) == [true, false, false] })

        // A tuner with no antenna switch: greyed, with the page's reason, and nothing sent.
        await station.deliver(FakeStation.accessoryDelta("tuner", "TunerModel", [("hasAntennaSwitch", .bool(false))]))
        #expect(await settle { accessories.tunerAntennaReason == AccessoriesModel.tunerNoAntennaSwitch })
        #expect(try Self.antennaColours(accessories) == [false, false, false])
        let before = Self.sent(station, "setTgxlAntenna")
        accessories.setTunerAntenna(2)
        try await LinkBarrier.roundTrip(model.commands)
        #expect(Self.sent(station, "setTgxlAntenna") == before)
        try await screen.shoot("tx-panel-antenna-row-no-switch")
        await model.disconnect()
    }

    // MARK: Inside

    /// Shows `content` over the tab bar as the app's root lays it out, then
    /// has the Core ask about the tuner's antenna: the question shows over
    /// the screen once, in its own window, takes the touches over the screen
    /// and lets the tab bar's through, and Set ANT1 sends one
    /// `confirm.proceed` however often it is pressed before the Core answers.
    private func askOver<Content: View>(_ name: String, tab: AppTab,
                                        @ViewBuilder content: @escaping (AppModel, ConnectionFlow) -> Content)
        async throws {
        let (model, station) = try await connected()
        let screen = try await Screen(model: model, station: station) { flow in
            VStack(spacing: 0) {
                content(model, flow)
                TabBar(selection: .constant(tab), sideways: false)
                    .background(ConfirmationWindowAnchor(app: model))
            }
            .background(ChromeColours.page)
        }
        defer { screen.close() }
        #expect(!screen.questionShowing)

        await station.deliver(Self.tunerAntennaQuestion(id: 31))
        #expect(await screen.settle { screen.questionShowing })
        // Once, over the screen, in the Core's words.
        #expect(ConfirmationWindows.all(over: screen.window).count == 1)
        let question = try #require(model.devices.question)
        #expect(SharedChangeSheet.kicker(question) == "This changes what the MacBook hears")
        #expect(question.change?.to == "ANT1")
        // Touches over the screen reach the question, the sheet's among them;
        // those on the tab bar go through to the app.
        let over = try #require(screen.questionWindow)
        #expect(over.hitTest(CGPoint(x: over.bounds.midX, y: over.coveredHeight - 12), with: nil) != nil)
        #expect(over.hitTest(CGPoint(x: over.bounds.midX, y: 60), with: nil) != nil)
        #expect(over.hitTest(CGPoint(x: 20, y: over.bounds.height - 10), with: nil) == nil)
        try await screen.shoot(name)

        // Set ANT1, pressed twice before the Core answers: one confirm.proceed.
        let first = Task { await model.devices.proceed() }
        let second = Task { await model.devices.proceed() }
        let proceed = await station.waitForMessage(within: .seconds(5)) { Self.invoke($0)?.verb == "confirm.proceed" }
        let invoke = try #require(proceed.flatMap(Self.invoke))
        #expect(invoke.args == [LinkMessage.PropertyEntry(name: "id", value: .i64(31)),
                                LinkMessage.PropertyEntry(name: "choice", value: .i64(SeveralDevices.noChoice))])
        await second.value
        #expect(Self.sent(station, "confirm.proceed") == 1)
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: "confirm.proceed", id: invoke.id, accepted: true, reason: "", affected: [])))
        await first.value
        #expect(await screen.settle { !screen.questionShowing })
        #expect(model.devices.question == nil)
        #expect(Self.sent(station, "confirm.proceed") == 1)
        await model.disconnect()
    }

    /// Which of ANT 1 to 3 the row draws lit (the accent blue), read from
    /// the row drawn at the TX panel's width.
    static func antennaColours(_ accessories: AccessoriesModel) throws -> [Bool] {
        let tuner = try #require(accessories.tunerGenius)
        let row = TunerAntennaRow(model: accessories, tuner: tuner, identifierPrefix: "txTunerAntenna")
            .frame(width: 280)
            .background(ChromeColours.panel)
            .environment(\.colorScheme, .dark)
        let renderer = ImageRenderer(content: row)
        renderer.scale = 1
        let image = try #require(renderer.uiImage)
        // Each button's top-left corner, clear of its words: 280 points, three
        // buttons 4 points apart, under the "Antenna:" caption.
        return (0..<3).map { index in
            Pixels.colour(image, at: CGPoint(x: CGFloat(index) * (272.0 / 3 + 4) + 5, y: 24)).blue > 0.5
        }
    }

    /// The Core's question when ANT 1 is tapped while the MacBook listens:
    /// the tuner's antenna from ANT3 to ANT1 changes what it hears.
    static func tunerAntennaQuestion(id: Int64) -> LinkMessage {
        .confirmRequest(LinkMessage.ConfirmRequest(
            id: id, kind: "sharedSetting", reason: SeveralDevices.waitingReason,
            affected: [SeveralDevicesScreenTests.affectedMacBook(effect: "changes")], expiresInMs: 60_000,
            change: ["label": .string("Tuner antenna"), "from": .string("ANT3"), "to": .string("ANT1")],
            forCommandId: 900))
    }

    nonisolated static func invoke(_ message: LinkMessage) -> LinkMessage.CommandInvoke? {
        TransmitScreenTests.invoke(message)
    }

    /// How many `verb` invokes the app has sent.
    static func sent(_ station: FakeStation, _ verb: String) -> Int {
        station.messages.filter { invoke($0)?.verb == verb }.count
    }

    /// The app connected to a fake Core that owns a Power Genius and a
    /// Tuner Genius and lets this phone transmit.
    private func connected() async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "ConfirmWhereTappedTests-\(UUID().uuidString)"))
        UIApplication.shared.isIdleTimerDisabled = false
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: [.remoteTx, .accessories])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "txState", className: "TransmitState",
                                                                    properties: TransmitScreenTests.holder(
                                                                        "", short: "", keyed: false)
                                                                        + [.init(ordinal: 17, name: "stopSerial",
                                                                                 value: .i64(0))])))
        await TransmitScreenTests.fillTransmit(station)
        await station.deliverAccessories()
        let accessories = model.main.accessories
        #expect(await settle {
            model.main.transmit.permitted && model.connection == .connected && accessories.tunerGenius != nil
                && accessories.records != nil && model.main.transmit.tuner != nil
        })
        return (model, station)
    }

    private func settle(seconds: Double = 5, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    /// A screen in a window of its own on an upright phone.
    @MainActor
    final class Screen {
        let window: UIWindow

        init<Content: View>(model: AppModel, station: FakeStation,
                            @ViewBuilder content: (ConnectionFlow) -> Content) async throws {
            let flow = ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
                keyStore: KeychainKeyStore(item: InMemorySecretItem()),
                stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
                transportFactory: station.transportFactory, clock: TestLinkClock(),
                microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
                appMajors: [1], now: Date.init, browser: nil))
            flow.show(.band)
            window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 874))
            let host = UIHostingController(rootView: content(flow).preferredColorScheme(.dark))
            host.view.frame = window.bounds
            window.rootViewController = host
            window.isHidden = false
            try await Task.sleep(for: .milliseconds(600))
        }

        func close() {
            window.isHidden = true
            window.rootViewController = nil
        }

        /// The question's window over this one.
        var questionWindow: ConfirmationWindow? {
            ConfirmationWindows.all(over: window).first
        }

        /// The question's window is up, above this one, and draws the sheet:
        /// its dark fill just above the tab bar, the screen dimmed above it,
        /// and nothing over the tab bar.
        var questionShowing: Bool {
            guard let over = questionWindow, !over.isHidden,
                  over.windowLevel.rawValue > window.windowLevel.rawValue else {
                return false
            }
            let image = UIGraphicsImageRenderer(bounds: over.bounds).image { _ in
                over.drawHierarchy(in: over.bounds, afterScreenUpdates: true)
            }
            let sheet = Pixels.colour(image, at: CGPoint(x: over.bounds.midX, y: over.coveredHeight - 12))
            let dim = Pixels.colour(image, at: CGPoint(x: over.bounds.midX, y: 60))
            let tabBar = Pixels.colour(image, at: CGPoint(x: over.bounds.midX, y: over.bounds.height - 10))
            return sheet.alpha > 0.95 && sheet.red < 0.1 && sheet.blue < 0.15
                && dim.alpha > 0.3 && dim.alpha < 0.6 && tabBar.alpha < 0.05
        }

        func settle(seconds: Double = 5, _ condition: () -> Bool) async -> Bool {
            let deadline = Date().addingTimeInterval(seconds)
            while Date() < deadline {
                if condition() {
                    return true
                }
                try? await Task.sleep(for: .milliseconds(50))
            }
            return condition()
        }

        /// Draws the app's window and the question over it and, with
        /// `NEREUS_MAIN_SHOTS` set, writes it there.
        func shoot(_ name: String) async throws {
            try await Task.sleep(for: .milliseconds(500))
            let image = ConfirmationWindows.draw(window)
            if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
               let data = image.pngData() {
                let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
                try data.write(to: url)
                print("Wrote \(url.path)")
            }
        }
    }
}

/// The tab being shown, which a test changes as a tap on the tab bar would.
@MainActor
final class Tabs: ObservableObject {
    @Published var selection: AppTab = .panadapter
}

/// The app's root laid out as ``RootView`` lays it out, every tab's screen
/// alive and only the chosen one shown, with the tab a test chooses.
private struct TabsRoot: View {
    let model: AppModel
    let flow: ConnectionFlow
    @ObservedObject var tabs: Tabs

    var body: some View {
        VStack(spacing: 0) {
            ZStack {
                screen(.panadapter) { MainScreen(app: model, main: model.main) }
                screen(.modes) { ModesTab(app: model, main: model.main, flow: flow) }
                screen(.radio) { RadioView(app: model, main: model.main, flow: flow) }
                screen(.setup) { SetupTab(app: model, flow: flow, router: SetupRouter()) }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            TabBar(selection: $tabs.selection, sideways: false)
                .background(ConfirmationWindowAnchor(app: model))
        }
        .background(ChromeColours.bar)
    }

    private func screen(_ tab: AppTab, @ViewBuilder content: () -> some View) -> some View {
        content()
            .opacity(tab == tabs.selection ? 1 : 0)
            .allowsHitTesting(tab == tabs.selection)
    }
}

/// The question's window over a test's window, and a picture of both.
@MainActor
enum ConfirmationWindows {
    /// Every question window over `host` that has not been taken down.
    static func all(over host: UIWindow) -> [ConfirmationWindow] {
        (host.windowScene?.windows ?? []).compactMap { $0 as? ConfirmationWindow }
            .filter { $0.host === host && !$0.isDismantled }
    }

    /// The host window, with the question's window over it when it shows.
    static func draw(_ host: UIWindow) -> UIImage {
        UIGraphicsImageRenderer(bounds: host.bounds).image { _ in
            host.drawHierarchy(in: host.bounds, afterScreenUpdates: true)
            if let over = all(over: host).first, !over.isHidden {
                over.drawHierarchy(in: host.bounds, afterScreenUpdates: true)
            }
        }
    }
}

/// A pixel of a picture, as red, green, blue and alpha from 0 to 1.
enum Pixels {
    struct Colour {
        var red: CGFloat
        var green: CGFloat
        var blue: CGFloat
        var alpha: CGFloat
    }

    static func colour(_ image: UIImage, at point: CGPoint) -> Colour {
        guard let cg = image.cgImage else {
            return Colour(red: 0, green: 0, blue: 0, alpha: 0)
        }
        let scale = image.scale
        var pixel: [UInt8] = [0, 0, 0, 0]
        let space = CGColorSpaceCreateDeviceRGB()
        pixel.withUnsafeMutableBytes { bytes in
            guard let context = CGContext(data: bytes.baseAddress, width: 1, height: 1, bitsPerComponent: 8,
                                          bytesPerRow: 4, space: space,
                                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
                return
            }
            context.translateBy(x: -point.x * scale, y: -(CGFloat(cg.height) - point.y * scale - 1))
            context.draw(cg, in: CGRect(x: 0, y: 0, width: cg.width, height: cg.height))
        }
        let alpha = CGFloat(pixel[3]) / 255
        func straight(_ value: UInt8) -> CGFloat {
            alpha > 0 ? CGFloat(value) / 255 / alpha : 0
        }
        return Colour(red: straight(pixel[0]), green: straight(pixel[1]), blue: straight(pixel[2]), alpha: alpha)
    }
}
