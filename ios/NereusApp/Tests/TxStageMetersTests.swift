// NereusSDR for iOS: the TX panel's transmit stage meters and the Core's mic mute against a fake Core, with pictures for the board
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// JJ, 2026-09-28: the seven transmit stage readings as one strip under Mic
/// level (`txState` at `txReadingsVersion` 3), with a recent peak the phone
/// keeps; and Mic level greyed while the Core's mic is muted
/// (`transmit.micMuted` at `transmitSettingsVersion` 10). With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the strip's screens are written there
/// for comparing with the board's `stagemeters-*.png`.
@Suite("Transmit stage meters and mic mute", .serialized)
@MainActor
struct TxStageMetersTests {
    private let platform = TestPlatform()

    /// The board's example readings and recent peaks, in ``TxStage/all``'s order.
    static let keyedReadings = [-4.2, -2.6, 5.8, -3.1, 2.4, 4.1, -1.9]
    static let keyedPeaks = [-1.8, -0.9, 7.2, -1.2, 3.9, 5.0, 0.6]
    static let idleReadings = [-18.5, -12.0, 9.6, -20.3, 0.0, 3.0, -27.0]

    // MARK: The bars

    @Test("seven bars in the sound's order, on the desktop bars' scales, red above their mid mark")
    func scales() {
        #expect(TxStage.all.map(\.name)
                == ["EQ", "Leveler", "Leveler Gain", "CFC", "CFC Gain", "ALC Gain", "ALC Group"])
        #expect(TxStage.all.map(\.property)
                == ["eqDb", "levelerDb", "levelerGainDb", "cfcDb", "cfcGainDb", "alcGainDb", "alcGroupDb"])
        #expect(TxStage.all.map { [$0.low, $0.mid, $0.high] }
                == [[-30, 0, 12], [-30, 0, 12], [0, 10, 30], [-30, 0, 12], [0, 10, 30], [0, 10, 30], [-30, 0, 25]])
        let eq = TxStage.all[0]
        #expect(eq.position(-30) == 0)
        #expect(abs(eq.position(0) - 0.665) < 1e-9)
        #expect(abs(eq.position(12) - 0.99) < 1e-9)
        #expect(eq.position(-400) == 0)
        #expect(abs(eq.position(40) - 0.99) < 1e-9)
        #expect(abs(TxStage.all[6].position(0) - 0.545) < 1e-9)
        #expect(abs(TxStage.all[2].position(10) - 0.333) < 1e-9)
        #expect(TxStage.readout(-4.24) == "-4.2 dB")
        #expect(TxStage.readout(5.8) == "5.8 dB")
        #expect(TxStage.readout(-0.02) == "0.0 dB")
        #expect(TxStage.readout(nil) == "--")
        #expect(TxStage.reading(-400) == nil)
        #expect(TxStage.reading(-195) == -195)
        #expect([TxStage.mark(-30), TxStage.mark(0), TxStage.mark(12)] == ["-30", "0", "+12"])
        #expect(TxStageMeters.olderCoreText == "This Core does not send these readings. Updating the Core may help.")
        #expect(TxStageMeters.noReadingsText == "The Core has no transmit readings right now.")
        #expect(TxStageMeters.offAirText
                == "Not transmitting. These are the last readings the Core sent; they move again when the radio transmits.")
    }

    // MARK: Against a fake Core

    @Test("keyed readings move with their peaks; a peak rises at once and falls a tenth of the way per reading")
    func keyedReadingsAndPeaks() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .txStageReadings])
        let transmit = model.main.transmit
        #expect(transmit.txReadingsVersion == 3)
        // Nothing from the Core so far: no reading anywhere, never a 0.
        #expect(TxStageMeters.look(transmit) == .noReadings)

        await station.deliver(TransmitScreenTests.txStateDelta(
            [.init(ordinal: 0, name: "keyed", value: .bool(true))] + Self.stages(Self.keyedReadings)))
        #expect(await settle { transmit.stageReadings == Self.keyedReadings.map(Optional.some) })
        #expect(TxStageMeters.look(transmit) == .onAir)
        #expect(TxStageMeters.line(.onAir) == nil)
        #expect(transmit.stagePeaks == transmit.stageReadings)

        // EQ rises: its peak follows at once.
        var next = Self.keyedReadings
        next[0] = -1.8
        await station.deliver(TransmitScreenTests.txStateDelta(Self.stages(next)))
        #expect(await settle { transmit.stageReadings[0] == -1.8 })
        #expect(transmit.stagePeaks[0] == -1.8)
        // EQ falls: its peak falls a tenth of the way to it with each reading.
        next[0] = -10
        await station.deliver(TransmitScreenTests.txStateDelta(Self.stages(next)))
        #expect(await settle { transmit.stageReadings[0] == -10 })
        let once = try #require(transmit.stagePeaks[0])
        #expect(abs(once - (-2.62)) < 1e-9)
        next[0] = -10.5
        await station.deliver(TransmitScreenTests.txStateDelta(Self.stages(next)))
        #expect(await settle { transmit.stageReadings[0] == -10.5 })
        let twice = try #require(transmit.stagePeaks[0])
        #expect(abs(twice - (once + (-10.5 - once) * 0.1)) < 1e-9)
        #expect(twice < once && twice > -10.5)
        // The other stages kept their readings, so their peaks sit on them.
        #expect(transmit.stagePeaks[1] == -2.6)

        // Unkeyed, the Core sends only what changes: the rest stay as last
        // sent, dimmed with the line, and no peak is shown.
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 0, name: "keyed", value: .bool(false)),
            .init(ordinal: 38, name: "levelerDb", value: .f64(-12)),
        ]))
        #expect(await settle { transmit.stageReadings[1] == -12 && !transmit.stagesOnAir })
        #expect(transmit.stageReadings[0] == -10.5)
        #expect(transmit.stageReadings[2] == 5.8)
        #expect(transmit.stagePeaks == TxStage.empty)
        #expect(TxStageMeters.look(transmit) == .offAir)
        #expect(TxStageMeters.line(.offAir) == TxStageMeters.offAirText)

        // Keyed again: the peaks start from the readings.
        await station.deliver(TransmitScreenTests.txStateDelta([.init(ordinal: 0, name: "keyed", value: .bool(true))]))
        #expect(await settle { transmit.stagesOnAir })
        #expect(transmit.stagePeaks == transmit.stageReadings)

        // The Core has no transmit channel: -400 everywhere reads "--", never 0.
        await station.deliver(TransmitScreenTests.txStateDelta(Self.stages(Array(repeating: -400, count: 7))))
        #expect(await settle { transmit.stageReadings == TxStage.empty })
        #expect(TxStageMeters.look(transmit) == .noReadings)
        #expect(TxStageMeters.line(.noReadings) == TxStageMeters.noReadingsText)
        #expect(transmit.stagePeaks == TxStage.empty)
        #expect(transmit.stageReadings.map(TxStage.readout) == Array(repeating: "--", count: 7))
        // One stage back: only it reads.
        await station.deliver(TransmitScreenTests.txStateDelta([.init(ordinal: 42, name: "alcGainDb", value: .f64(4.1))]))
        #expect(await settle { transmit.stageReadings[5] == 4.1 })
        #expect(TxStageMeters.look(transmit) == .onAir)
        #expect(transmit.stageReadings.filter { $0 != nil }.count == 1)
        await model.disconnect()
    }

    @Test("an older Core, without txReadingsVersion 3, shows the reason once over seven greyed bars")
    func olderCore() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let transmit = model.main.transmit
        #expect(transmit.txReadingsVersion == 0)
        // Even a stray reading is not shown without the version.
        await station.deliver(TransmitScreenTests.txStateDelta(
            [.init(ordinal: 0, name: "keyed", value: .bool(true))] + Self.stages(Self.keyedReadings)))
        #expect(await settle { transmit.stagesOnAir })
        #expect(transmit.stageReadings == TxStage.empty)
        #expect(transmit.stagePeaks == TxStage.empty)
        #expect(TxStageMeters.look(transmit) == .olderCore)
        #expect(TxStageMeters.line(.olderCore) == TxStageMeters.olderCoreText)
        await model.disconnect()
    }

    @Test("Mic level greys while another device mutes the Core's mic, ungreys when it unmutes, and the phone writes nothing")
    func micMute() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let transmit = model.main.transmit
        await TransmitScreenTests.fillTransmit(station)
        #expect(await settle { transmit.rfPower == 100 })
        #expect(transmit.settingsVersion >= 10)
        #expect(!transmit.micMuted)
        #expect(MicLevelGauge.mutedReason(transmit) == nil)

        await station.deliver(Self.micMuted(true))
        #expect(await settle { transmit.micMuted })
        #expect(MicLevelGauge.mutedReason(transmit) == "The Core's mic is muted.")
        #expect(TransmitModel.micMutedText == "The Core's mic is muted.")

        await station.deliver(Self.micMuted(false))
        #expect(await settle { !transmit.micMuted })
        #expect(MicLevelGauge.mutedReason(transmit) == nil)
        #expect(!station.messages.contains { message in
            guard case .propertyWrite(let write) = message else {
                return false
            }
            return write.properties.contains { $0.name == "micMuted" }
        })
        await model.disconnect()

        // A Core before transmitSettingsVersion 10 sends no mute to read.
        let (older, olderStation) = try await connected(additions: [.remoteTx],
                                                         without: ["transmitSettingsVersion"])
        await TransmitScreenTests.fillTransmit(olderStation)
        #expect(await settle { older.main.transmit.rfPower == 100 })
        await olderStation.deliver(Self.micMuted(true))
        try await Task.sleep(for: .milliseconds(300))
        #expect(!older.main.transmit.micMuted)
        await older.disconnect()
    }

    // MARK: Pictures

    @Test("the strip keyed, off the air, with no readings and on an older Core; large type, sideways, the iPad column and a muted mic")
    func stripShots() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .accessoryOperate, .txStageReadings])
        let transmit = model.main.transmit
        try await MainScreenShotTests.fill(station)
        #expect(await settle { model.main.slices.entries.count == 2 })
        // The board's frames are in USB.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 2, name: "dspMode", value: .enumeration(1)),
        ])))
        await TransmitScreenTests.fillTransmit(station)
        #expect(await settle { transmit.amp != nil && transmit.rfPower == 100 })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))

        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state.isKeyed })
        await station.deliver(TransmitScreenTests.txStateDelta(
            [.init(ordinal: 0, name: "keyed", value: .bool(true))] + Self.stages(Self.keyedPeaks)))
        #expect(await settle { transmit.stagePeaks[0] == -1.8 })
        await station.deliver(TransmitScreenTests.txStateDelta(Self.stages(Self.keyedReadings)))
        #expect(await settle { transmit.stageReadings[0] == -4.2 })
        #expect(TxStageMeters.look(transmit) == .onAir)
        try await shoot("stagemeters-portrait-keyed-usb", model: model)
        try await shoot("stagemeters-portrait-large-type", model: model, largeText: true)
        try await shoot("stagemeters-landscape", model: model, sideways: true)
        try await shoot("stagemeters-landscape-large-type", model: model, sideways: true, largeText: true)
        try await shootColumn("stagemeters-ipad-column", model: model)

        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state == .idle })
        await station.deliver(TransmitScreenTests.txStateDelta(
            [.init(ordinal: 0, name: "keyed", value: .bool(false))] + Self.stages(Self.idleReadings)))
        #expect(await settle { !transmit.stagesOnAir && transmit.stageReadings[0] == -18.5 })
        #expect(TxStageMeters.look(transmit) == .offAir)
        try await shoot("stagemeters-portrait-not-transmitting", model: model)

        await station.deliver(TransmitScreenTests.txStateDelta(Self.stages(Array(repeating: -400, count: 7))))
        #expect(await settle { TxStageMeters.look(transmit) == .noReadings })
        try await shoot("stagemeters-portrait-no-readings", model: model)

        await station.deliver(Self.micMuted(true))
        #expect(await settle { transmit.micMuted })
        try await shoot("mic-muted-portrait", model: model)
        await model.disconnect()

        let (older, olderStation) = try await connected(additions: [.remoteTx, .accessoryOperate])
        try await MainScreenShotTests.fill(olderStation)
        await TransmitScreenTests.fillTransmit(olderStation)
        #expect(await settle { older.main.slices.entries.count == 2 && older.main.transmit.amp != nil })
        older.main.band.endpointId = 1
        older.main.band.receive(.context(try #require(BandFlagShotTests.context())))
        #expect(TxStageMeters.look(older.main.transmit) == .olderCore)
        try await shoot("stagemeters-portrait-older-core", model: older, find: .reason)
        await older.disconnect()
    }

    // MARK: The fake Core

    static func stages(_ values: [Double]) -> [LinkMessage.PropertyEntry] {
        zip(TxStage.all, values).enumerated().map { index, pair in
            LinkMessage.PropertyEntry(ordinal: UInt16(37 + index), name: pair.0.property, value: .f64(pair.1))
        }
    }

    static func micMuted(_ muted: Bool) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "transmit", properties: [.init(ordinal: 86, name: "micMuted", value: .bool(muted))]))
    }

    private func connected(additions: FakeStation.Additions,
                           without: Set<String> = []) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "TxStageMetersTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: additions, withoutCapabilities: without)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await settle { model.main.transmit.permitted && model.connection == .connected })
        return (model, station)
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }

    // MARK: Drawing

    /// What marks the strip in a picture: a bar's red ground, or the older
    /// Core's reason box.
    enum Mark {
        case redGround
        case reason

        var colour: (Int, Int, Int) {
            switch self {
            case .redGround: (0x1E, 0x0E, 0x12)
            case .reason: (0x14, 0x1C, 0x28)
            }
        }
    }

    /// The main screen with the TX panel open, the panel scrolled so the
    /// strip sits under Mic level as the board's frames show it.
    private func shoot(_ name: String, model: AppModel, sideways: Bool = false, largeText: Bool = false,
                       find mark: Mark = .redGround) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let window = try BandFlagShotTests.window(size: size)
        if sideways {
            window.frame.origin.y = 120
        }
        let root = StageShotRoot(model: model)
            .environment(\.dynamicTypeSize, largeText ? .accessibility1 : .large)
            .preferredColorScheme(.dark)
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
        BandFlagShotTests.feed(model.main.band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(model.main.band, in: window, after: bandDraw)
        let image = try await scrolledToStrip(window, panelWidth: TxPanel.width, mark: mark,
                                              lead: sideways ? 40 : largeText ? 70 : 110)
        try write(name, image)
    }

    /// The iPad's applet column, as the column draws it on its side, scrolled to the strip.
    private func shootColumn(_ name: String, model: AppModel) async throws {
        let size = CGSize(width: AppletColumn.width, height: 834)
        let window = try BandFlagShotTests.window(size: size)
        let root = AppletColumn(main: model.main)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = try await scrolledToStrip(window, panelWidth: AppletColumn.width, mark: .redGround, lead: 110)
        try write(name, image)
    }

    /// Scrolls the panel's scroll view until `mark` shows, then so that it
    /// sits `lead` points below the scroll view's top, and draws the window.
    private func scrolledToStrip(_ window: UIWindow, panelWidth: CGFloat, mark: Mark,
                                 lead: CGFloat) async throws -> UIImage {
        await ShotWait.laidOut(window)
        let scrolls = Self.scrollViews(in: window).filter {
            $0.contentSize.height > $0.bounds.height + 1 && abs($0.contentSize.width - panelWidth) < 1
        }
        let all = Self.scrollViews(in: window).map { "\($0.bounds.size) in \($0.contentSize)" }
        #expect(scrolls.count == 1, "expected one scrolling panel \(panelWidth) wide among \(all)")
        let scroll = try #require(scrolls.first, "no scrolling panel \(panelWidth) wide among \(all)")
        let frame = scroll.convert(scroll.bounds, to: window)
        let furthest = max(scroll.contentSize.height - scroll.bounds.height, 0)
        var found: CGFloat?
        var offset: CGFloat = 0
        while found == nil {
            scroll.setContentOffset(CGPoint(x: 0, y: min(offset, furthest)), animated: false)
            await ShotWait.laidOut(window)
            found = Self.firstRow(of: mark.colour, in: frame, image: render(window))
            if offset >= furthest {
                break
            }
            offset += scroll.bounds.height / 2
        }
        let row = try #require(found, "the strip was not found in the panel")
        let target = min(max(scroll.contentOffset.y + row - (frame.minY + lead), 0), furthest)
        scroll.setContentOffset(CGPoint(x: 0, y: target), animated: false)
        await ShotWait.laidOut(window)
        let image = render(window)
        #expect(abs(scroll.contentOffset.y - target) < 1,
                "scrolled to \(scroll.contentOffset.y), wanted \(target) of \(furthest), row \(row)")
        return image
    }

    private func render(_ window: UIWindow) -> UIImage {
        UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
    }

    private func write(_ name: String, _ image: UIImage) throws {
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    private static func scrollViews(in view: UIView) -> [UIScrollView] {
        var found: [UIScrollView] = []
        if let scroll = view as? UIScrollView {
            found.append(scroll)
        }
        for child in view.subviews {
            found += scrollViews(in: child)
        }
        return found
    }

    /// The topmost row, in points from the window's top, inside `rect`
    /// where a run of pixels has `colour` (within a few steps), or nil.
    private static func firstRow(of colour: (Int, Int, Int), in rect: CGRect, image: UIImage) -> CGFloat? {
        guard let cgImage = image.cgImage else {
            return nil
        }
        let width = cgImage.width
        let height = cgImage.height
        var pixels = [UInt8](repeating: 0, count: width * height * 4)
        guard let space = CGColorSpace(name: CGColorSpace.sRGB),
              let context = CGContext(data: &pixels, width: width, height: height, bitsPerComponent: 8,
                                      bytesPerRow: width * 4, space: space,
                                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
            return nil
        }
        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: width, height: height))
        let scale = CGFloat(width) / image.size.width
        let left = max(Int(rect.minX * scale), 0)
        let right = min(Int(rect.maxX * scale), width)
        let top = max(Int(rect.minY * scale), 0)
        let bottom = min(Int(rect.maxY * scale), height)
        guard left < right, top < bottom else {
            return nil
        }
        // A run of the colour at least 20 points long across a row: a bar's
        // red ground, or the reason box, never a stray pixel of it.
        let run = Int(20 * scale)
        for y in top..<bottom {
            var length = 0
            for x in left..<right {
                let index = (y * width + x) * 4
                if abs(Int(pixels[index]) - colour.0) <= 4, abs(Int(pixels[index + 1]) - colour.1) <= 4,
                   abs(Int(pixels[index + 2]) - colour.2) <= 4 {
                    length += 1
                    if length >= run {
                        return CGFloat(y) / scale
                    }
                } else {
                    length = 0
                }
            }
        }
        return nil
    }
}

/// The app's root as `RootView` lays it out, with the TX panel open.
private struct StageShotRoot: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, txPanelOpen: true)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
