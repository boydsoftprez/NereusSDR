// NereusSDR for iOS: the band menu's hold, where it opens, its Display Range arithmetic and its notch at the finger
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// R-IOS-57, DC6: a half-second hold on empty band that stays within the
/// drag threshold opens the desktop's band menu at the finger; its Ref
/// Level and Dyn Range move the band's dB Max and dB Min as the desktop's
/// do, and "Add notch here" sends `notch.add` at the finger's frequency.
@Suite("Band menu", .serialized)
@MainActor
struct BandMenuTests {
    // Waits by the clock, not by a count of yields: a loaded simulator can
    // spend a fixed count of yields before the fake Core's reply lands.
    private func settle(_ condition: () -> Bool) async -> Bool {
        let deadline = ContinuousClock.now + .seconds(10)
        while ContinuousClock.now < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(1))
        }
        return condition()
    }

    // At least the 5,000 yields it always gave, and at least a quarter of a
    // second by the clock, so late work lands before what follows is checked.
    private func quiet() async {
        let until = ContinuousClock.now + .milliseconds(250)
        for _ in 0..<5_000 {
            await Task.yield()
        }
        while ContinuousClock.now < until {
            try? await Task.sleep(for: .milliseconds(5))
        }
    }

    private static let layout = BandLayout(size: CGSize(width: 402, height: 500), scale: 1,
                                           settings: BandDisplaySettings())

    // MARK: The hold

    @Test("the hold is half a second and ends at the drag threshold")
    func holdRule() {
        #expect(BandMenuHit.holdSeconds == 0.5)
        #expect(BandMenuHit.moveLimitPoints == 6)
    }

    @Test("empty band is inside the band, off the dBm scale and clear of flags, spots and other devices' labels")
    func emptyBand() {
        let layout = Self.layout
        let flag = CGRect(x: 40, y: 20, width: 120, height: 90)
        let spot = CGRect(x: 220, y: 30, width: 60, height: 16)
        let station = CGRect(x: 200, y: 140, width: 80, height: 24)
        func empty(_ point: CGPoint) -> Bool {
            BandMenuHit.isEmptyBand(at: point, flags: [flag], spots: [spot], stations: [station], layout: layout)
        }
        #expect(empty(CGPoint(x: 120, y: 300)))
        #expect(!empty(CGPoint(x: flag.midX, y: flag.midY)))
        #expect(!empty(CGPoint(x: spot.midX, y: spot.midY)))
        #expect(!empty(CGPoint(x: station.midX, y: station.midY)))
        #expect(!empty(CGPoint(x: layout.dbmScale.midX, y: layout.dbmScale.midY)))
        #expect(!empty(CGPoint(x: -1, y: 300)))
        #expect(!empty(CGPoint(x: 120, y: layout.size.height + 1)))
    }

    // MARK: Where it opens

    @Test("upright the menu opens at the finger, kept inside the band")
    func uprightOrigin() {
        let band = CGSize(width: 402, height: 500)
        let size = CGSize(width: BandMenu.width, height: 260)
        #expect(BandMenu.origin(finger: CGPoint(x: 40, y: 60), size: size, bandSize: band, sideways: false)
                == CGPoint(x: 40, y: 60))
        // Near the far corner it moves in until it fits.
        let corner = BandMenu.origin(finger: CGPoint(x: 380, y: 480), size: size, bandSize: band, sideways: false)
        #expect(corner == CGPoint(x: band.width - BandMenu.edgeInset - size.width,
                                  y: band.height - BandMenu.edgeInset - size.height))
    }

    @Test("sideways the menu opens beside the finger, on its right while it fits, and never over PTT")
    func sidewaysOrigin() {
        let band = CGSize(width: 874, height: 340)
        let size = CGSize(width: BandMenu.width, height: 260)
        let ptt = PttButton.frame(bandSize: band, leadingInset: 0)
        let finger = CGPoint(x: 300, y: 200)
        let right = BandMenu.origin(finger: finger, size: size, bandSize: band, sideways: true, avoid: ptt)
        #expect(right.x == finger.x + BandMenu.besideGap)
        #expect(right.y == finger.y - size.height / 2)
        #expect(!CGRect(origin: right, size: size).intersects(ptt))
        // Near the right edge it opens on the finger's left.
        let far = CGPoint(x: 800, y: 200)
        let left = BandMenu.origin(finger: far, size: size, bandSize: band, sideways: true, avoid: ptt)
        #expect(left.x == far.x - BandMenu.besideGap - size.width)
        // Just above PTT it moves right of the button.
        let low = BandMenu.origin(finger: CGPoint(x: ptt.minX, y: ptt.minY), size: size, bandSize: band,
                                  sideways: true, avoid: ptt)
        #expect(!CGRect(origin: low, size: size).intersects(ptt))
        #expect(low.x == ptt.maxX + BandMenu.edgeInset)
    }

    // MARK: Display Range

    @Test("Ref Level is dB Max and keeps the span; Dyn Range is the span and keeps the top")
    func rangeArithmetic() {
        let values = BandMenu.values(maxDbm: -40, minDbm: -140)
        #expect(values.refLevel == -40)
        #expect(values.dynRange == 100)
        let ref = BandMenu.range(refLevel: -30, from: (-40, -140))
        #expect(ref.maxDbm == -30 && ref.minDbm == -130)
        let dyn = BandMenu.range(dynRange: 80, from: (-40, -140))
        #expect(dyn.maxDbm == -40 && dyn.minDbm == -120)
        // The desktop's slider ranges hold each end.
        #expect(BandMenu.range(refLevel: 50, from: (-40, -140)).maxDbm == 20)
        #expect(BandMenu.range(refLevel: -200, from: (-40, -140)).maxDbm == -160)
        #expect(BandMenu.range(dynRange: 10, from: (-40, -140)).minDbm == -60)
        #expect(BandMenu.range(dynRange: 300, from: (-40, -140)).minDbm == -200)
        #expect(BandMenu.refLevelRange.min == -160 && BandMenu.refLevelRange.max == 20)
        #expect(BandMenu.dynRangeRange.min == 20 && BandMenu.dynRangeRange.max == 160)
    }

    @Test("the range rows give transmit, an older Core, no link and no band as their reasons, in that order")
    func rangeReasons() {
        #expect(BandMenu.rangeReason(transmitting: false, shared: true, connected: true, bandKnown: true) == nil)
        #expect(BandMenu.rangeReason(transmitting: true, shared: false, connected: false, bandKnown: false)
                == DisplayPillPhoneKeys.transmitLevelsReason)
        #expect(BandMenu.rangeReason(transmitting: false, shared: false, connected: false, bandKnown: false)
                == CoreBandScale.olderCoreText)
        #expect(BandMenu.rangeReason(transmitting: false, shared: true, connected: false, bandKnown: false)
                == CoreBandScale.notConnectedText)
        #expect(BandMenu.rangeReason(transmitting: false, shared: true, connected: true, bandKnown: false)
                == PanSheetModel.noSliceReason)
    }

    @Test("the values and the frequency read as the desktop prints them")
    func texts() {
        #expect(BandMenu.refLevelText(-40) == "-40 dBm")
        #expect(BandMenu.dynRangeText(100) == "100 dB")
        #expect(BandMenu.frequencyText(7_240_125) == "7.240125 MHz")
    }

    // MARK: A notch at the finger

    /// A model connected to a fake Core; `.all` sends `notchControlVersion` 2, so a Core without it adds nothing.
    private func connected(_ additions: FakeStation.Additions = .all,
                           withoutCapabilities: Set<String> = []) async throws -> (AppModel, FakeStation) {
        let station = try FakeStation(additions: additions, withoutCapabilities: withoutCapabilities)
        let defaults = try #require(UserDefaults(suiteName: "BandMenuTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle { model.main.catalogFeed.catalog != nil && model.main.pan.sliceLetter == "A" })
        await quiet()
        return (model, station)
    }

    private func invokes(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap {
            if case .commandInvoke(let invoke) = $0, invoke.verb == verb {
                return invoke
            }
            return nil
        }
    }

    @Test("Add notch here sends notch.add for the active slice at the finger's frequency, 200 Hz wide")
    func addNotchAtFrequency() async throws {
        let (model, station) = try await connected()
        let pan = model.main.pan
        #expect(pan.canAddNotchAtFrequency)
        pan.addNotch(atHz: 14_205_250)
        #expect(await settle { !self.invokes(station, PanSheetModel.addNotchAtVerb).isEmpty })
        let sent = try #require(invokes(station, PanSheetModel.addNotchAtVerb).first)
        #expect(sent.args == [
            .init(name: "sliceId", value: .i64(0)),
            .init(name: "centreHz", value: .f64(14_205_250)),
            .init(name: "widthHz", value: .f64(200)),
        ])
        // A frequency that is not one is never sent.
        pan.addNotch(atHz: .nan)
        pan.addNotch(atHz: 0)
        await quiet()
        #expect(invokes(station, PanSheetModel.addNotchAtVerb).count == 1)
        await model.disconnect()
    }

    @Test("a Core without notch control greys Add notch here with the Pan sheet's reason and is sent nothing")
    func noNotchControl() async throws {
        let (model, station) = try await connected([], withoutCapabilities: [PanSheetModel.notchControlCapability])
        let pan = model.main.pan
        #expect(!pan.canAddNotchAtFrequency)
        #expect(!pan.notchUnavailableReason.isEmpty)
        pan.addNotch(atHz: 14_205_250)
        await quiet()
        #expect(invokes(station, PanSheetModel.addNotchAtVerb).isEmpty)
        await model.disconnect()
    }
}
