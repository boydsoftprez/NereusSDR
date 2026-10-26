// NereusSDR for iOS: the band's waterfall lines go in on the row clock, driven by the screen, and the 3D glides by it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusMedia
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// The band's lines go in on the row clock's steady grid while a view on
/// screen drives them (JJ's build 7950 recording, 2026-10-25: the waterfall
/// and the 3D moved in 16.7 and 50 ms pairs as each line went in on arrival).
@Suite("Band lines on the row clock", .serialized)
@MainActor
struct BandRowClockTests {
    /// A view that can drive the band's lines, without a screen.
    final class Driver: BandLineDriver {
        var drivesLines = true
    }

    static let frame = 1.0 / 60

    /// A band on the Core's context (30 frames a second, a line a frame),
    /// 3D at four lines a row when `stacked`.
    static func band(stacked: Bool) throws -> BandModel {
        var settings = BandDisplaySettings.desktopDefaults
        settings.spectrumView = stacked ? .stacked : .flat
        settings.threeDSpeed = 4
        let band = BandModel(settings: settings)
        band.stackOffered = true
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        _ = band.prepareToDraw(size: CGSize(width: 400, height: 800), scale: 1)
        return band
    }

    @Test("the model's lines go in every second frame, and the 3D glide reads the clock")
    func glideAndFoldReadTheClock() throws {
        let band = try Self.band(stacked: true)
        #expect(band.drawsStack)
        let driver = Driver()
        band.lineDriver = driver
        var now = 500.0
        band.lineClock = { now }
        // Lines every 33.3 ms, up to 8 ms early or late.
        let arrivals = (0 ..< 240).map { k in
            500.2 + Double(k) / 30 + Double(WaterfallRowClockTestsJitter.ns(k)) / 1e9
        }.sorted()
        var next = 0
        var putFrames: [Int] = []
        var positions: [Double] = []
        var number = 0
        while 500 + Double(number) * Self.frame <= arrivals[arrivals.count - 1] + 0.5 {
            defer { number += 1 }
            let frameTime = 500 + Double(number) * Self.frame
            while next < arrivals.count && arrivals[next] <= frameTime {
                now = arrivals[next]
                band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(next + 1))))
                next += 1
            }
            now = frameTime
            let lines = band.state.history.linesAppended
            band.advanceLines(at: frameTime, frameSeconds: Self.frame)
            for _ in 0 ..< Int(band.state.history.linesAppended - lines) {
                putFrames.append(number)
            }
            // The glide the renderer is given is the clock's, whatever the time.
            let glide = band.stackGlide(at: Date().timeIntervalSince1970)
            if band.state.stack.count > 0 {
                #expect(glide == (band.state.pacedStackGlide ?? -1))
            }
            positions.append(Double(band.state.stack.rowsAppended) + glide)
        }
        #expect(band.pacingLines)
        #expect(putFrames.count == arrivals.count)
        let steady = zip(putFrames, putFrames.dropFirst()).filter { $0.0 >= 120 }.map { $1 - $0 }
        #expect(steady.allSatisfy { $0 == 2 }, "\(steady)")
        // The stack moves on every frame while lines flow, an eighth of a row each.
        let flowing = positions.dropFirst(120).prefix(putFrames.count * 2 - 240)
        let steps = zip(flowing, flowing.dropFirst()).map { $1 - $0 }
        #expect(steps.allSatisfy { abs($0 - 0.125) < 0.04 }, "\(steps)")
    }

    @Test("looking back stays on the same lines as the clock puts lines in; a mark and a hold put the waiting lines in first")
    func consumersKeepWorking() throws {
        let band = try Self.band(stacked: false)
        let driver = Driver()
        band.lineDriver = driver
        var now = 100.0
        band.lineClock = { now }
        for sequence in 1 ... 40 {
            now = 100 + Double(sequence) / 30
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
            band.advanceLines(at: now, frameSeconds: Self.frame)
        }
        band.lookBack(lines: 10, visibleLines: 5)
        #expect(band.lookBackLines == 10)
        let before = band.state.history.linesAppended
        for sequence in 41 ... 50 {
            now = 100 + Double(sequence) / 30
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
            band.advanceLines(at: now, frameSeconds: Self.frame)
            band.advanceLines(at: now + Self.frame, frameSeconds: Self.frame)
        }
        let put = Int(band.state.history.linesAppended - before)
        #expect(put > 0)
        #expect(band.lookBackLines == 10 + put)
        // A mark goes above every line that came before it.
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: 51)))
        #expect(band.state.linesWaiting > 0)
        band.markAway("Sound only")
        #expect(band.state.linesWaiting == 0)
        // Stop on TX: the lines from before the key go in, then the waterfall holds.
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: 52)))
        #expect(band.state.linesWaiting == 1)
        band.settings.waterfallStopOnTx = true
        band.keyed = true
        #expect(band.state.linesWaiting == 0 && band.state.waterfallHeld)
        band.keyed = false
        // The view gone: lines go in as they come.
        driver.drivesLines = false
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: 53)))
        #expect(!band.pacingLines && band.state.linesWaiting == 0)
    }

    @Test("on screen, the display link puts the lines in and the band draws them; in 3D the stack glides between lines")
    func linesFlowOnScreen() async throws {
        for stacked in [false, true] {
            let band = try Self.band(stacked: stacked)
            let window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 874))
            window.rootViewController = ShotHosting.host(BandView(model: band).ignoresSafeArea())
            window.isHidden = false
            defer {
                window.isHidden = true
                window.rootViewController = nil
            }
            let ready = try await ShotWait.requireBandLaidOut(band, in: window)
            var glides = Set<Double>()
            var waited = false
            // Lines as the Core sends them, one every 33 ms of real time.
            for sequence in 1 ... 60 {
                band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
                waited = waited || band.state.linesWaiting > 0
                for _ in 0 ..< 3 {
                    try await Task.sleep(for: .milliseconds(11))
                    glides.insert(band.stackGlide(at: Date().timeIntervalSince1970))
                }
            }
            #expect(band.pacingLines, "3D \(stacked)")
            #expect(waited, "the lines waited for the clock")
            #expect(await ShotWait.until { band.state.linesWaiting == 0 && band.state.history.linesAppended >= 60 })
            try await ShotWait.requireBandShown(band, in: window, after: ready)
            if stacked {
                #expect(band.state.stack.rowsAppended >= 14)
                #expect(glides.count > 10, "the stack glided between lines: \(glides.count) values")
            }
        }
    }
}

/// The Kit tests' jitter, ±8 ms, for the app's own runs.
enum WaterfallRowClockTestsJitter {
    static func ns(_ k: Int) -> Int64 {
        let span: Int64 = 8_000_000
        var seed = UInt32(12_345) &+ UInt32(truncatingIfNeeded: k) &* 2_654_435_761
        seed = seed &* 1_664_525 &+ 1_013_904_223
        seed ^= seed >> 13
        return Int64(seed % UInt32(2 * span + 1)) - span
    }
}
