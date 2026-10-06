// NereusSDR for iOS: the band's sound-only mark drawn beneath the band's controls, on the real main screen
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-11: the mark across the waterfall for a stretch the band was not
/// sent ("sound only") draws beneath the controls on the band (the dial's
/// step box and wheel, zoom, the dBm arrows), never over them. With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each picture is written there as a PNG.
@Suite("The band's marks", .serialized)
@MainActor
struct BandMarksShotTests {
    static let markText = "Locked 19:42 to 20:15 \u{00B7} sound only"
    /// The mark's height, its `awayMarkHeight`, kept here so this test reads
    /// the same wherever the mark is drawn from.
    static let markHeight: CGFloat = 18

    @Test("the sound-only mark scrolls beneath the dial's step box and wheel")
    func markBeneathTheDial() async throws {
        let suite = "BandMarksShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        settings.dialKind = .thumbwheel
        let model = AppModel(phoneSettings: settings,
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 5) { model.main.slices.entries.count == 2 })

        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))

        let host = try Host(model: model)
        defer { host.close() }
        let draw = try await ShotWait.requireBandLaidOut(band, in: host.window)
        BandFlagShotTests.feed(band, width: draw.requestedPixels, lines: 400)
        band.markAway(Self.markText)
        BandFlagShotTests.feed(band, width: draw.requestedPixels, lines: 60)
        try await ShotWait.requireBandShown(band, in: host.window, after: draw)
        #expect(await ShotWait.until { host.markFrame() != nil && host.stepFrame() != nil })

        // The mark in the open waterfall, clear of every control.
        host.write("band-mark-open-waterfall")
        let open = try #require(host.markFrame())
        let step = try #require(host.stepFrame())
        #expect(open.maxY < step.minY)

        // Scroll it down until it sits across the step box's middle.
        let lines = Int(((step.midY - open.midY) * host.scale).rounded())
        #expect(lines > 0)
        BandFlagShotTests.feed(band, width: draw.requestedPixels, lines: lines)
        try await ShotWait.requireBandShown(band, in: host.window, after: draw)
        #expect(await ShotWait.until { host.markFrame().map { abs($0.midY - step.midY) <= 2 } ?? false })
        let mark = try #require(host.markFrame())
        #expect(abs(mark.midY - step.midY) <= 2)
        host.write("band-mark-under-dial")

        // The step box's bright label shows through where the mark crosses
        // it. Drawn over, the mark's near-opaque strip dims every one of
        // those pixels, and its own words are grey, not bright.
        let crossing = mark.intersection(step)
        #expect(!crossing.isNull && crossing.height >= 10)
        let bright = host.brightPixels(in: crossing)
        #expect(bright > 20, "step box pixels showing through the mark: \(bright)")

        await model.disconnect()
    }

    // MARK: Inside

    private func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    /// The real main screen over the tab bar in a window of its own.
    @MainActor
    final class Host {
        let window: UIWindow
        let host: UIHostingController<AnyView>
        var scale: CGFloat { window.screen.scale }

        init(model: AppModel, size: CGSize = CGSize(width: 402, height: 874)) throws {
            let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
            window = UIWindow(windowScene: scene)
            window.frame = CGRect(origin: .zero, size: size)
            window.windowLevel = .alert + 1
            let root = VStack(spacing: 0) {
                MainScreen(app: model, main: model.main)
                TabBar(selection: .constant(.panadapter), sideways: false)
            }
            .background(ChromeColours.bar.ignoresSafeArea())
            .preferredColorScheme(.dark)
            host = UIHostingController(rootView: AnyView(root))
            host.view.frame = CGRect(origin: .zero, size: size)
            window.rootViewController = host
            window.isHidden = false
        }

        func close() {
            window.isHidden = true
            window.rootViewController = nil
        }

        /// Where the dial's step box is: the box its fill (#0F1420,
        /// `DialColours.stepBackground`) covers in the window's lower left.
        /// SwiftUI builds no accessibility tree for a window no assistive
        /// tool is reading, so it is found by its pixels.
        func stepFrame() -> CGRect? {
            pixels { cg, pixelScale, colour in
                var box: (minX: Int, minY: Int, maxX: Int, maxY: Int)?
                for y in cg.height * 2 / 3..<cg.height {
                    for x in 0..<cg.width / 2 where colour(x, y) == (0x0F, 0x14, 0x20) {
                        box = box.map { (min($0.minX, x), min($0.minY, y), max($0.maxX, x), max($0.maxY, y)) }
                            ?? (x, y, x, y)
                    }
                }
                return box.map {
                    CGRect(x: CGFloat($0.minX) / pixelScale, y: CGFloat($0.minY) / pixelScale,
                           width: CGFloat($0.maxX - $0.minX + 1) / pixelScale,
                           height: CGFloat($0.maxY - $0.minY + 1) / pixelScale)
                }
            }
        }

        /// Reads the window's picture a pixel at a time: `colour(x, y)` is
        /// its red, green and blue.
        private func pixels<Result>(_ read: (CGImage, CGFloat, (Int, Int) -> (Int, Int, Int)) -> Result?) -> Result? {
            guard let cg = image().cgImage,
                  let data = cg.dataProvider?.data, let bytes = CFDataGetBytePtr(data) else {
                return nil
            }
            let perPixel = cg.bitsPerPixel / 8
            let bgra = cg.bitmapInfo.contains(.byteOrder32Little)
            let colour = { (x: Int, y: Int) -> (Int, Int, Int) in
                let at = y * cg.bytesPerRow + x * perPixel
                return (Int(bytes[at + (bgra ? 2 : 0)]), Int(bytes[at + 1]), Int(bytes[at + (bgra ? 0 : 2)]))
            }
            return read(cg, CGFloat(cg.width) / window.bounds.width, colour)
        }

        func image() -> UIImage {
            // Eight bits a channel, so the pixel counts below read bytes.
            let format = UIGraphicsImageRendererFormat()
            format.preferredRange = .standard
            format.scale = window.screen.scale
            return UIGraphicsImageRenderer(bounds: window.bounds, format: format).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
        }

        /// Where the mark is, from its two dashed edges (#405060, one point
        /// thick, `markHeight` apart) across the lower two thirds of
        /// the window, found by its pixels as the step box is.
        func markFrame() -> CGRect? {
            pixels { cg, pixelScale, colour in
                // Its dashed edges (#405060, `BandColours.awayMarkEdge`) are
                // the only rows of that exact colour. Beneath the dial only
                // the gaps between PTT, the step box and the wheel show them,
                // so a few dozen pixels in a row are enough.
                func edge(_ y: Int) -> Bool {
                    (0..<cg.width).filter { x in colour(x, y) == (0x40, 0x50, 0x60) }.count > 16
                }
                let rows = (cg.height / 3..<cg.height).filter(edge)
                let apart = BandMarksShotTests.markHeight * pixelScale
                guard let top = rows.first(where: { top in
                    rows.contains { abs(CGFloat($0 - top) - (apart - pixelScale)) <= pixelScale }
                }) else {
                    return nil
                }
                return CGRect(x: 0, y: CGFloat(top) / pixelScale, width: window.bounds.width,
                              height: BandMarksShotTests.markHeight)
            }
        }

        /// Pixels in `rect` (points) as bright as the step box's label
        /// (#C8D8E8), brighter than anything the mark draws (#8090A0 at most).
        func brightPixels(in rect: CGRect) -> Int {
            pixels { cg, pixelScale, colour in
                let area = CGRect(x: rect.minX * pixelScale, y: rect.minY * pixelScale,
                                  width: rect.width * pixelScale, height: rect.height * pixelScale).integral
                    .intersection(CGRect(x: 0, y: 0, width: cg.width, height: cg.height))
                var count = 0
                for y in Int(area.minY)..<Int(area.maxY) {
                    for x in Int(area.minX)..<Int(area.maxX) {
                        let (red, _, blue) = colour(x, y)
                        if red >= 170, blue >= 200 {
                            count += 1
                        }
                    }
                }
                return count
            } ?? 0
        }

        func write(_ name: String) {
            if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
               let data = image().pngData() {
                let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
                try? data.write(to: url)
                print("Wrote \(url.path)")
            }
        }
    }
}
