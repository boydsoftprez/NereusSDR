// NereusSDR for iOS: the S-meter's seven faces drawn on the Core's scale, for comparing with the desktop's
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// D86, D83: each face drawn at the desktop fixture's 300 by 150 and
/// 600 by 300 sizes, receiving at -61 dBm, on the air at 87 W, and with
/// no reading. With
/// `NEREUS_METER_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_METER_SHOTS`), each is written there as a PNG for
/// comparing with the desktop's render of the same readings.
@Suite("S-meter faces", .serialized)
@MainActor
struct SMeterFaceShotTests {
    static let small = CGSize(width: 300, height: 150)
    static let large = CGSize(width: 600, height: 300)

    @Test("each face draws, and the faces differ from one another")
    func faces() async throws {
        let defaults = try #require(UserDefaults(suiteName: "SMeterFaceShotTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             meterSettings: SMeterSettingsStore(defaults: defaults))
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        let deadline = Date().addingTimeInterval(5)
        while model.main.sMeter.meters == nil, Date() < deadline {
            try await Task.sleep(for: .milliseconds(50))
        }
        let meters = try #require(model.main.sMeter.meters)
        await model.disconnect()

        var samples: [UInt32] = []
        for (index, face) in SMeterFace.allCases.enumerated() {
            var state = SMeterState(settings: SMeterSettings(face: face))
            state.apply(SMeterReadings(peakDbm: -61), at: 0)
            let rx = state.display(at: 0, meters: meters)
            let image = try render(face: face, display: rx, size: Self.small)
            samples.append(Self.pixel(image, at: CGPoint(x: Self.small.width / 2, y: Self.small.height * 0.3)))
            try write(image, name: "phone-face-\(index)-rx")
            try write(try render(face: face, display: rx, size: Self.large), name: "phone-face-\(index)-rx-large")

            state.apply(SMeterReadings(transmitting: true, transmitSent: true, forwardWatts: 87, swr: 1.3), at: 1)
            try write(try render(face: face, display: state.display(at: 1, meters: meters), size: Self.small),
                      name: "phone-face-\(index)-tx")
            state.apply(SMeterReadings(peakDbm: -400), at: 2)
            let empty = state.display(at: 2, meters: meters)
            #expect(empty.spoken == "No signal reading")
            try write(try render(face: face, display: empty, size: Self.small),
                      name: "phone-face-\(index)-noreading")
        }
        #expect(Set(samples).count >= 5)
    }

    @Test("the readouts in each unit, with and without the decimal point: the faces' text and the flag's level")
    func readouts() async throws {
        let defaults = try #require(UserDefaults(suiteName: "SMeterFaceShotTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             meterSettings: SMeterSettingsStore(defaults: defaults))
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        let deadline = Date().addingTimeInterval(5)
        while model.main.sMeter.meters == nil, Date() < deadline {
            try await Task.sleep(for: .milliseconds(50))
        }
        let meters = try #require(model.main.sMeter.meters)
        await model.disconnect()

        var rights: Set<String> = []
        for unit in SMeterUnit.allCases {
            for decimal in [true, false] {
                let name = "phone-readout-\(unit.rawValue)-\(decimal ? "decimal" : "whole")"
                var state = SMeterState(settings: SMeterSettings(unit: unit, showDecimal: decimal))
                state.apply(SMeterReadings(peakDbm: -73.4), at: 0)
                let shown = state.display(at: 0, meters: meters)
                rights.insert(shown.left + "|" + shown.right)
                try write(try render(face: .classic, display: shown, size: Self.small), name: "\(name)-classic")
                try write(try render(face: .agedCream, display: shown, size: Self.small), name: "\(name)-aged-cream")
                // The flag's level bar, a weak and a strong signal.
                let readout = SMeterReadout(unit: unit, showDecimal: decimal)
                for (label, dbm) in [("weak", -103.4), ("strong", -13.4)] {
                    try write(try renderFlag(dbm: dbm, meter: meters.sMeter, readout: readout),
                              name: "\(name)-flag-\(label)")
                }
            }
        }
        // Six different pairs of readouts: each unit and decimal setting prints differently.
        #expect(rights.count == 6)
    }

    // MARK: Inside

    private func renderFlag(dbm: Double, meter: StationCatalog.Meters.SMeter,
                            readout: SMeterReadout) throws -> UIImage {
        let view = FlagLevelBar(dbm: dbm, meter: meter, readout: readout)
            .frame(width: 190, height: 26)
            .padding(5)
            .background(BandColours.flagBackground)
            .environment(\.colorScheme, .dark)
        let renderer = ImageRenderer(content: view)
        renderer.scale = 3
        return try #require(renderer.uiImage)
    }

    private func render(face: SMeterFace, display: SMeterDisplay, size: CGSize) throws -> UIImage {
        let view = Group {
            if let theme = face.theme {
                VintageMeterFace(display: display, needle: display.needle, theme: theme)
            } else {
                ClassicMeterFace(display: display, needle: display.needle)
            }
        }
        .frame(width: size.width, height: size.height)
        .clipped()
        .environment(\.colorScheme, .dark)
        let renderer = ImageRenderer(content: view)
        renderer.scale = 2
        return try #require(renderer.uiImage)
    }

    private func write(_ image: UIImage, name: String) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_METER_SHOTS"], !directory.isEmpty,
              let data = image.pngData() else {
            return
        }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try data.write(to: url)
        print("Wrote \(url.path)")
    }

    /// The colour at `point` (in points), as `0xRRGGBB`.
    private static func pixel(_ image: UIImage, at point: CGPoint) -> UInt32 {
        guard let cgImage = image.cgImage else {
            return 0
        }
        var bytes = [UInt8](repeating: 0, count: 4)
        let space = CGColorSpaceCreateDeviceRGB()
        guard let context = CGContext(data: &bytes, width: 1, height: 1, bitsPerComponent: 8, bytesPerRow: 4,
                                      space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
            return 0
        }
        let scale = image.scale
        context.draw(cgImage, in: CGRect(x: -point.x * scale, y: -(CGFloat(cgImage.height) - point.y * scale),
                                         width: CGFloat(cgImage.width), height: CGFloat(cgImage.height)))
        return UInt32(bytes[0]) << 16 | UInt32(bytes[1]) << 8 | UInt32(bytes[2])
    }
}
