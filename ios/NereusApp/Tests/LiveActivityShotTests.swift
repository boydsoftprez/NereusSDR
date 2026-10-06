// NereusSDR for iOS: the Live Activity's presentations drawn on the simulator, for comparing with pictures 13, 14, 16 and 02
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// The lock-screen card and the Dynamic Island as the widget draws them
/// (the same views, built into the app for this), each state on a phone
/// drawn generic around it as the board does: the lock screen of
/// `13-lock-screen.jpg` and `16-long-sessions.jpg`, another app in front
/// with the island of `14-other-apps.jpg`, and StandBy of
/// `02-sideways.jpg`. With `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each is written there as a PNG.
@Suite("The Live Activity on screen", .serialized)
@MainActor
struct LiveActivityShotTests {
    static let now = Date()

    static var meter: StationActivityAttributes.Meter {
        StationActivityAttributes.Meter(minDbm: -127, s9Dbm: -73, maxDbm: -13, marks: [
            .init(label: "S1", dbm: -121), .init(label: "3", dbm: -109), .init(label: "5", dbm: -97),
            .init(label: "7", dbm: -85), .init(label: "9", dbm: -73), .init(label: "+20", dbm: -53),
            .init(label: "+40", dbm: -33),
        ])
    }

    static func listening(link: Int = 38, message: String = "", good: Bool = false) -> StationActivityAttributes.ContentState {
        StationActivityAttributes.ContentState(
            stationName: "KG4VCF/shack", link: .up, roundTripMs: link,
            slice: StationActivityAttributes.Slice(letter: "A", colour: "#00D4FF", frequencyHz: 7_236_400,
                                                   mode: "LSB", bandwidth: "2.9K", signalDbm: -81,
                                                   signalText: "-81.0 dBm", signalSpoken: "-81.0 dBm"),
            meter: meter, message: message, messageGood: good)
    }

    static var keyed: StationActivityAttributes.ContentState {
        var state = listening()
        state.keyed = true
        state.keyedSince = now.addingTimeInterval(-47)
        state.forwardWatts = 100
        state.swr = 1.15
        state.timeOutAt = now.addingTimeInterval(133)
        return state
    }

    static func lost(keyed: Bool) -> StationActivityAttributes.ContentState {
        var state = listening()
        state.link = .lost
        state.roundTripMs = nil
        state.lostWhileKeyed = keyed
        state.retry = StationActivityAttributes.Retry(attempt: 4, at: now.addingTimeInterval(8), stopped: false)
        return state
    }

    /// Colours the drawings are checked for, as the board draws them.
    static let frequencyCyan = Pixel(0x00, 0xE5, 0xFF)
    static let unkeyRed = Pixel(0xCC, 0x22, 0x22)
    static let lostRed = Pixel(0xC1, 0x48, 0x48)
    static let amber = Pixel(0xFF, 0xD7, 0x00)
    static let txClockText = Pixel(0xFF, 0xB0, 0xB0)
    static let linkGreen = Pixel(0x5F, 0xFF, 0x8A)

    @Test("the lock-screen card: listening, locked while keyed, keyed, link lost, back on the air, eight hours, stale")
    func lockScreen() throws {
        let size = CGSize(width: 402, height: 874)
        // The card sits in the lower part of the lock screen.
        let card = CGRect(x: 0, y: 560, width: 402, height: 314)
        // The card's head row (the Core, the TX clock, the link), above UNKEY.
        let head = CGRect(x: 0, y: 610, width: 402, height: 60)
        let listening = try render("activity-lock-listening", LockScreen(state: Self.listening()), size: size)
        #expect(listening.count(Self.frequencyCyan, in: card) > 400, "the frequency is drawn")
        #expect(listening.count(Self.linkGreen, in: card) > 20, "the link dot is drawn")
        #expect(listening.count(Self.unkeyRed, in: card) == 0, "no UNKEY while listening")

        let unkeyed = try render("activity-lock-unkeyed",
                                 LockScreen(state: Self.listening(message: "Unkeyed when you locked the phone. TX ran 0:42.")),
                                 size: size)
        #expect(unkeyed.count(Self.frequencyCyan, in: card) > 400)
        #expect(unkeyed.count(Self.unkeyRed, in: card) == 0)

        let keyed = try render("activity-lock-keyed", LockScreen(state: Self.keyed), size: size)
        #expect(keyed.count(Self.unkeyRed, in: card) > 2_000, "UNKEY is drawn")
        #expect(keyed.count(Self.amber, in: card) > 30, "the time-out is drawn")
        #expect(keyed.count(Self.txClockText, in: head) > 30, "the TX clock is drawn")

        let lost = try render("activity-lock-link-lost", LockScreen(state: Self.lost(keyed: false)), size: size)
        #expect(lost.count(Self.lostRed, in: card) > 300, "LINK LOST is drawn")
        #expect(lost.count(Self.frequencyCyan, in: card) == 0, "no frequency while lost")

        let back = try render("activity-lock-back-on-air",
                              LockScreen(state: Self.listening(message: "Back on the air.", good: true)), size: size)
        #expect(back.count(Self.frequencyCyan, in: card) > 400)
        let eight = try render("activity-lock-eight-hours",
                               LockScreen(state: Self.listening(message: ActivityWords.eightHours)), size: size)
        #expect(eight.count(Self.frequencyCyan, in: card) > 400)

        // Gone stale while keyed: UNKEY stays, the words are amber, and no clock or link time.
        let stale = try render("activity-lock-stale", LockScreen(state: Self.keyed.shown(stale: true)), size: size)
        #expect(stale.count(Self.unkeyRed, in: card) > 2_000, "UNKEY stays on a stale keyed card")
        #expect(stale.count(Self.amber, in: card) > 30, "the no-news words are drawn")
        #expect(stale.count(Self.txClockText, in: head) == 0, "no TX clock")
        #expect(stale.count(Self.linkGreen, in: card) == 0, "no link time")
    }

    @Test("the lock-screen card stays under iOS's 160-point limit in every state")
    func cardHeight() {
        let states = [Self.listening(), Self.keyed, Self.lost(keyed: true),
                      Self.listening(message: ActivityWords.eightHours), Self.keyed.shown(stale: true)]
        for state in states {
            let host = UIHostingController(rootView: ActivityCard(state: state).frame(width: 374))
            let height = host.sizeThatFits(in: CGSize(width: 374, height: 1000)).height
            #expect(height <= 160, "the card is \(height) points tall")
        }
    }

    @Test("the Dynamic Island: listening, keyed, opened while keyed, opened with the link lost, and the smallest")
    func island() throws {
        let size = CGSize(width: 402, height: 874)
        let compact = CGRect(x: 80, y: 0, width: 242, height: 60)
        let opened = CGRect(x: 0, y: 0, width: 402, height: 220)
        let listening = try render("activity-island-listening", OtherApp(state: Self.listening(), opened: false),
                                   size: size)
        #expect(listening.count(Self.frequencyCyan, in: compact) > 40, "the frequency beside the camera")
        let keyed = try render("activity-island-keyed", OtherApp(state: Self.keyed, opened: false), size: size)
        #expect(keyed.count(Self.frequencyCyan, in: compact) == 0)
        #expect(keyed.count(Pixel(0xFF, 0x80, 0x80), in: compact) > 40, "TX and its clock in red")
        let openedKeyed = try render("activity-island-opened-keyed", OtherApp(state: Self.keyed, opened: true),
                                     size: size)
        #expect(openedKeyed.count(Self.unkeyRed, in: opened) > 2_000, "UNKEY in the opened island")
        #expect(openedKeyed.count(Self.amber, in: opened) > 30, "the time-out in the opened island")
        let openedLost = try render("activity-island-opened-lost",
                                    OtherApp(state: Self.lost(keyed: true), opened: true), size: size)
        #expect(openedLost.count(Self.lostRed, in: opened) > 300)
        #expect(openedLost.count(Self.unkeyRed, in: opened) == 0)
        let openedListening = try render("activity-island-opened-listening",
                                         OtherApp(state: Self.listening(), opened: true), size: size)
        #expect(openedListening.count(Self.frequencyCyan, in: opened) > 400)
        let minimal = try render("activity-island-minimal", Minimal(states: [Self.listening(), Self.keyed,
                                                                             Self.lost(keyed: false)]),
                                 size: CGSize(width: 240, height: 80))
        #expect(minimal.count(Pixel(0x00, 0xD4, 0xFF), in: CGRect(x: 0, y: 0, width: 240, height: 80)) > 40)
    }

    @Test("StandBy: the card at twice the size, edge to edge")
    func standBy() throws {
        let standBy = try render("activity-standby", StandBy(state: Self.listening()),
                                 size: CGSize(width: 874, height: 402))
        let listening = try render("activity-lock-listening", LockScreen(state: Self.listening()),
                                   size: CGSize(width: 402, height: 874))
        let big = standBy.count(Self.frequencyCyan, in: CGRect(x: 0, y: 0, width: 874, height: 402))
        let small = listening.count(Self.frequencyCyan, in: CGRect(x: 0, y: 560, width: 402, height: 314))
        // Twice the size is four times the area; allow for the edges of the glyphs.
        #expect(Double(big) > 3 * Double(small), "StandBy's frequency is \(big) pixels, the card's \(small)")
    }

    // MARK: Drawing

    private func render<V: View>(_ name: String, _ view: V, size: CGSize) throws -> Picture {
        let renderer = ImageRenderer(content: view.frame(width: size.width, height: size.height)
            .environment(\.colorScheme, .dark))
        renderer.scale = 2
        let image = try #require(renderer.uiImage)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        return try #require(Picture(image))
    }

    /// A colour to look for.
    struct Pixel {
        let red: UInt8
        let green: UInt8
        let blue: UInt8

        init(_ red: UInt8, _ green: UInt8, _ blue: UInt8) {
            self.red = red
            self.green = green
            self.blue = blue
        }
    }

    /// A drawing's pixels, in points at scale 2, to count colours in.
    struct Picture {
        let width: Int
        let height: Int
        let bytes: [UInt8]

        init?(_ image: UIImage) {
            guard let cg = image.cgImage else {
                return nil
            }
            width = cg.width
            height = cg.height
            var data = [UInt8](repeating: 0, count: width * height * 4)
            let drawn = data.withUnsafeMutableBytes { buffer -> Bool in
                guard let context = CGContext(data: buffer.baseAddress, width: cg.width, height: cg.height,
                                              bitsPerComponent: 8, bytesPerRow: cg.width * 4,
                                              space: CGColorSpace(name: CGColorSpace.sRGB)!,
                                              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
                    return false
                }
                context.draw(cg, in: CGRect(x: 0, y: 0, width: cg.width, height: cg.height))
                return true
            }
            guard drawn else {
                return nil
            }
            bytes = data
        }

        /// Pixels within `tolerance` of `pixel` in `rect` (points).
        func count(_ pixel: Pixel, in rect: CGRect, tolerance: Int = 24) -> Int {
            let scale = 2
            let x0 = max(0, Int(rect.minX) * scale)
            let y0 = max(0, Int(rect.minY) * scale)
            let x1 = min(width, Int(rect.maxX) * scale)
            let y1 = min(height, Int(rect.maxY) * scale)
            var found = 0
            guard x1 > x0, y1 > y0 else {
                return 0
            }
            for y in y0..<y1 {
                for x in x0..<x1 {
                    let i = (y * width + x) * 4
                    if abs(Int(bytes[i]) - Int(pixel.red)) <= tolerance
                        && abs(Int(bytes[i + 1]) - Int(pixel.green)) <= tolerance
                        && abs(Int(bytes[i + 2]) - Int(pixel.blue)) <= tolerance {
                        found += 1
                    }
                }
            }
            return found
        }
    }

    /// iOS's lock screen, drawn generic: the date, the clock, the card near the foot.
    private struct LockScreen: View {
        let state: StationActivityAttributes.ContentState

        var body: some View {
            ZStack(alignment: .top) {
                RadialGradient(colors: [Color(red: 0x1D / 255, green: 0x3D / 255, blue: 0x5C / 255),
                                        Color(red: 0x0D / 255, green: 0x1B / 255, blue: 0x2C / 255),
                                        Color(red: 0x05 / 255, green: 0x07 / 255, blue: 0x0C / 255)],
                               center: UnitPoint(x: 0.25, y: 0), startRadius: 0, endRadius: 800)
                VStack(spacing: 4) {
                    Image(systemName: "lock.fill").font(.system(size: 15)).foregroundStyle(.white.opacity(0.86))
                        .padding(.top, 58)
                    Text("Tuesday, September 22").font(.system(size: 19, weight: .semibold))
                        .foregroundStyle(.white.opacity(0.86))
                    Text("7:32").font(.system(size: 104, weight: .bold)).foregroundStyle(.white.opacity(0.94))
                    Spacer()
                    ActivityCard(state: state)
                        .background(ActivityCard.background(state),
                                    in: RoundedRectangle(cornerRadius: 24, style: .continuous))
                        .padding(.horizontal, 14)
                        .padding(.bottom, 116)
                }
            }
        }
    }

    /// Another app in front (a plain skeleton), with NereusSDR in the island.
    private struct OtherApp: View {
        let state: StationActivityAttributes.ContentState
        let opened: Bool

        var body: some View {
            ZStack(alignment: .top) {
                Color(red: 0xF2 / 255, green: 0xF2 / 255, blue: 0xF7 / 255)
                VStack(alignment: .leading, spacing: 14) {
                    RoundedRectangle(cornerRadius: 8).fill(Color(white: 0.84)).frame(width: 170, height: 28)
                        .padding(.top, 94)
                    RoundedRectangle(cornerRadius: 10).fill(Color(white: 0.89)).frame(height: 36)
                    ForEach(0..<4, id: \.self) { _ in
                        RoundedRectangle(cornerRadius: 14).fill(Color.white).frame(height: 74)
                    }
                    Spacer()
                }
                .padding(.horizontal, 18)
                if opened {
                    VStack(alignment: .leading, spacing: 7) {
                        HStack {
                            ActivityIsland(part: .expandedLeading, state: state)
                            Spacer()
                            ActivityIsland(part: .expandedTrailing, state: state)
                        }
                        ActivityIsland(part: .expandedBottom, state: state)
                    }
                    .padding(.top, 12)
                    .padding(.horizontal, 20)
                    .padding(.bottom, 16)
                    .background(Color.black, in: RoundedRectangle(cornerRadius: 44, style: .continuous))
                    .padding(.horizontal, 10)
                    .padding(.top, 10)
                } else {
                    HStack(spacing: 6) {
                        ActivityIsland(part: .compactLeading, state: state)
                        Spacer()
                        ActivityIsland(part: .compactTrailing, state: state)
                    }
                    .padding(.leading, 10)
                    .padding(.trailing, 11)
                    .frame(width: 222, height: 36)
                    .background(Color.black, in: Capsule())
                    .padding(.top, 11)
                }
            }
        }
    }

    /// The smallest island, beside another app's.
    private struct Minimal: View {
        let states: [StationActivityAttributes.ContentState]

        var body: some View {
            HStack(spacing: 20) {
                ForEach(Array(states.enumerated()), id: \.offset) { _, state in
                    ActivityIsland(part: .minimal, state: state)
                        .frame(width: 36, height: 36)
                        .background(Color.black, in: Circle())
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color(red: 0xF2 / 255, green: 0xF2 / 255, blue: 0xF7 / 255))
        }
    }

    /// StandBy: locked on a charger on its side; iOS shows the card at twice the size, edge to edge.
    private struct StandBy: View {
        let state: StationActivityAttributes.ContentState

        var body: some View {
            ZStack {
                ActivityColours.standBy
                ActivityCard(state: state)
                    .frame(width: 344)
                    .scaleEffect(2)
            }
        }
    }
}
