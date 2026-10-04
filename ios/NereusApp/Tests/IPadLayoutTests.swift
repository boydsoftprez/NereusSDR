// NereusSDR for iOS: the iPad's layouts: the applet column on its side, the front panel upright, and the S-meter's scale
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing

/// R-IOS-24, D30, D31, spec section 5.6: which layout each screen takes,
/// the band's width beside the column and above the front panel, the flags
/// at those widths, and the analog S-meter's scale and faces from the Core's
/// meter ranges. The meter ranges here are synthetic (D4).
@Suite @MainActor struct IPadLayoutTests {
    /// The 11-inch iPad's screen under the status bar, over the tab bar
    /// (834 by 1210 points, less 24 and 70), either way up.
    static let upright = CGSize(width: 834, height: 1116)
    static let onItsSide = CGSize(width: 1210, height: 740)
    static let phoneUpright = CGSize(width: 402, height: 780)
    static let phoneSideways = CGSize(width: 874, height: 360)

    // MARK: Which layout

    @Test("an iPad on its side takes the applet column, upright the front panel")
    func iPadArrangements() {
        #expect(IPadLayout.arrangement(horizontal: .regular, size: Self.onItsSide) == .column)
        #expect(IPadLayout.arrangement(horizontal: .regular, size: Self.upright) == .frontPanel)
    }

    @Test("a phone either way up, and an iPad shared narrowly, take the phone's layout")
    func phoneArrangements() {
        #expect(IPadLayout.arrangement(horizontal: .compact, size: Self.phoneUpright) == .phone)
        // A large iPhone on its side is regular width, but not an iPad's room.
        #expect(IPadLayout.arrangement(horizontal: .regular, size: Self.phoneSideways) == .phone)
        // An iPad window beside another app is compact width.
        #expect(IPadLayout.arrangement(horizontal: .compact, size: CGSize(width: 500, height: 740)) == .phone)
        #expect(IPadLayout.arrangement(horizontal: nil, size: Self.onItsSide) == .phone)
    }

    @Test("the column's corner button shows only on its side; the Core's name shows either way up")
    func toolbar() {
        #expect(IPadLayout.column.hasColumnButton)
        #expect(!IPadLayout.frontPanel.hasColumnButton)
        #expect(!IPadLayout.phone.hasColumnButton)
        #expect(IPadLayout.column.isIPad && IPadLayout.frontPanel.isIPad && !IPadLayout.phone.isIPad)
    }

    // MARK: The band's room

    @Test("on its side the band gives the column 320 points, and takes them back when it hides")
    func bandBesideTheColumn() {
        #expect(IPadLayout.columnWidth == 320)
        #expect(IPadLayout.column.bandWidth(screenWidth: 1210, columnShown: true) == 890)
        #expect(IPadLayout.column.bandWidth(screenWidth: 1210, columnShown: false) == 1210)
    }

    @Test("upright the band keeps the whole width, and the three columns share it below the band")
    func frontPanel() {
        #expect(IPadLayout.frontPanel.bandWidth(screenWidth: 834, columnShown: true) == 834)
        #expect(IPadLayout.frontPanel.bandWidth(screenWidth: 834, columnShown: false) == 834)
        let height = IPadLayout.frontPanelHeight(underToolbar: Self.upright.height - Toolbar.height)
        #expect(height == 482)
        // The band keeps more than half the room under the toolbar.
        #expect(Self.upright.height - Toolbar.height - height > height)
        #expect(FrontPanelColumns.Column.allCases == [.sMeter, .rx, .tx])
        #expect(FrontPanelColumns.columnWidth(panelWidth: 834) == 278)
    }

    // MARK: The flags

    static func slice(_ id: Int, _ hz: Double) -> BandSlice {
        BandSlice(id: id, frequencyHz: hz, filterLowHz: -3000, filterHighHz: -100, colour: "#102030",
                  lowerSideband: true, txSlice: id == 0)
    }

    static func placements(width: CGFloat, spanHz: Double) -> [FlagPlacement] {
        let slices = [Self.slice(0, 7_236_000), Self.slice(1, 7_238_000)]
        let geometry = BandGeometry(centerHz: 7_237_000, spanHz: spanHz, size: CGSize(width: width, height: 300),
                                    dbmRange: -140 ... -40)
        return FlagLayout.layout(slices: slices, activeSliceId: 0, geometry: geometry)
    }

    @Test("two slices 2 kHz apart keep both flags full on the iPad either way up, where the phone folds one")
    func flagsStayFull() {
        // A 6 kHz span: 2 kHz is 296 points beside the column, 278 upright,
        // past a flag and its round buttons (242), and 134 on a phone.
        let span = 6_000.0
        let beside = Self.placements(width: IPadLayout.column.bandWidth(screenWidth: 1210, columnShown: true),
                                     spanHz: span)
        #expect(beside.allSatisfy { !$0.isFolded })
        #expect(!beside[0].rect.intersects(beside[1].rect))
        let upright = Self.placements(width: IPadLayout.frontPanel.bandWidth(screenWidth: 834, columnShown: true),
                                      spanHz: span)
        #expect(upright.allSatisfy { !$0.isFolded })
        #expect(!upright[0].rect.intersects(upright[1].rect))
        let phone = Self.placements(width: Self.phoneUpright.width, spanHz: span)
        #expect(phone[1].isFolded)
    }

    @Test("the board's two slices, 12.6 kHz apart, fold B as the phone sideways does: closer than a flag and its buttons")
    func boardSlicesStayFull() {
        let slices = [BandSlice(id: 0, frequencyHz: 7_236_400, filterLowHz: -3000, filterHighHz: -100,
                                colour: "#102030", lowerSideband: true, txSlice: true),
                      BandSlice(id: 1, frequencyHz: 7_249_000, filterLowHz: -3000, filterHighHz: -100,
                                colour: "#203040", lowerSideband: true, txSlice: false)]
        for width in [IPadLayout.column.bandWidth(screenWidth: 1210, columnShown: true), 834] {
            let geometry = BandGeometry(centerHz: 7_244_000, spanHz: 48_000, size: CGSize(width: width, height: 300),
                                        dbmRange: -140 ... -40)
            let placements = FlagLayout.layout(slices: slices, activeSliceId: 0, geometry: geometry)
            // 233 points beside the column and 219 upright: under the 242 a
            // full flag and its round buttons need (R-IOS-11), so B folds.
            let apart = geometry.x(forHz: slices[1].frequencyHz) - geometry.x(forHz: slices[0].frequencyHz)
            #expect(apart < 242)
            #expect(!placements[0].isFolded && placements[1].isFolded)
            #expect(placements[0].rect.minY == 0)
        }
    }

    // MARK: The S-meter

    /// A synthetic meter: S0 at -130, 5 dB an S-unit to S9 at -85, marks
    /// every 10 dB over S9 to -25, red from S9; and a 0 to 200 W power scale,
    /// red from 150.
    static func meters() throws -> StationCatalog.Meters {
        var sUnits: [[String: Any]] = []
        for unit in 0 ... 9 {
            sUnits.append(["label": "S\(unit)", "dbm": -130 + unit * 5])
        }
        var over: [[String: Any]] = []
        for step in 1 ... 6 {
            over.append(["label": "+\(step * 10)", "dbm": -85 + step * 10])
        }
        let object: [String: Any] = [
            "sMeter": ["minDbm": -130, "s9Dbm": -85, "maxDbm": -25, "dbPerSUnit": 5, "redFromDbm": -85,
                       "sUnits": sUnits, "overS9": over],
            "micLevel": ["minDb": -30, "maxDb": 5, "yellowFromDb": -5, "redFromDb": 0],
            "rfPower": ["minW": 0, "maxW": 200, "ratedW": 150, "redFromW": 150],
            "swr": ["min": 1, "max": 4, "redFrom": 3],
        ]
        let data = try JSONSerialization.data(withJSONObject: object)
        return try JSONDecoder().decode(StationCatalog.Meters.self, from: data)
    }

    @Test("the needle's place follows the Core's range, S9 at 60% of the arc, held at its ends")
    func needle() throws {
        let meter = try Self.meters().sMeter
        #expect(SMeterScale.receiveFraction(dbm: -200, meter: meter) == 0)
        #expect(abs(SMeterScale.receiveFraction(dbm: -85, meter: meter) - 0.6) < 1e-9)
        #expect(SMeterScale.receiveFraction(dbm: 10, meter: meter) == 1)
        let power = try Self.meters().rfPower
        #expect(SMeterScale.transmitFraction(50, mode: .power, power: power) == 0.25)
        #expect(SMeterScale.transmitFraction(500, mode: .power, power: power) == 1)
        #expect(SMeterScale.transmitFraction(50, mode: .power, power: nil) == 0)
    }

    @Test("the Core's no-reading value reads as none")
    func noReading() {
        #expect(SMeterReadings.reading(-400) == nil)
        #expect(SMeterReadings.reading(nil) == nil)
        #expect(SMeterReadings.reading(-399.02) == -399.02)
    }

    @Test("the classic arc sweeps evenly either side of straight up, and the needle turns under the meter")
    func classicArc() {
        let arc = ClassicMeterFace.Geometry(size: CGSize(width: 320, height: 160))
        let left = arc.point(0, radius: arc.radius)
        let middle = arc.point(0.5, radius: arc.radius)
        let right = arc.point(1, radius: arc.radius)
        #expect(abs(middle.x - 160) < 1e-6)
        #expect(abs((160 - left.x) - (right.x - 160)) < 1e-6)
        #expect(abs(left.y - right.y) < 1e-6)
        #expect(middle.y < left.y)
        // The scale stays inside the meter, clear of the readouts along its top.
        #expect(left.x > 0 && right.x < 320)
        #expect(middle.y > 40)
        #expect(arc.pivot.y > 160)
    }

    @Test("a vintage card's scale and its mirror line stay on the card at the column's width and the front panel's")
    func vintageArc() {
        for size in [CGSize(width: 320, height: 160), CGSize(width: 278, height: 139)] {
            let g = VintageMeterFace.Geometry(size: size)
            let outer = g.radius + VintageMeterFace.mirrorLineUnits * g.unit
            let left = g.point(degrees: -VintageMeterFace.sweepDegrees / 2 - 2, radius: outer)
            let right = g.point(degrees: VintageMeterFace.sweepDegrees / 2 + 2, radius: outer)
            #expect(left.x > g.face.minX && right.x < g.face.maxX)
            #expect(g.pivot.y - outer > g.face.minY)
            #expect(g.pivot.y < g.face.maxY)
        }
    }
}
