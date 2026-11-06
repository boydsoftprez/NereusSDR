// NereusSDR for iOS: where the band stack's chips sit on the spectrum, when they show, and the phone's switch for them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusMirror
@testable import NereusSDR
import Testing

@Suite("Band stack markers")
@MainActor
struct BandStackMarkerTests {
    private static let layout = BandLayout(size: CGSize(width: 400, height: 600), scale: 1, stripPoints: 10)
    private static let geometry = layout.spectrumGeometry(centerHz: 7_200_000, spanHz: 96_000, dbmRange: -140 ... -40)

    private func document(_ entries: [(id: String, hz: Int64, locked: Bool)]) throws -> [BandStack.Entry] {
        let rows = entries.map { entry in
            """
            {"id":"\(entry.id)","band":"40m","frequencyHz":\(entry.hz),"mode":"LSB","filterLowHz":-2800,"filterHighHz":-100,\
            "centerHz":\(entry.hz),"spanHz":96000,"locked":\(entry.locked),"seeded":false,"name":"","lastUsedMs":0}
            """
        }.joined(separator: ",")
        let order = entries.map { "\"\($0.id)\"" }.joined(separator: ",")
        let json = """
        {"schema":1,"region":"United States","options":{"dupeProtection":true,"expiryMinutes":15,"dwellSeconds":30},\
        "bands":{"40m":{"order":[\(order)],"returnMode":"current","specificId":""}},\
        "entries":[\(rows)],"places":{},"lastExpiry":{"batchId":"","count":0},\
        "lastRecall":{"seq":0,"sliceId":0,"applyView":false,"centerHz":0,"spanHz":0}}
        """
        return try #require(BandStack(json: json, revision: 1)).entries
    }

    private func chips(_ entries: [BandStack.Entry], active: String? = nil) -> [BandStackMarkerLayout.Chip] {
        BandStackMarkerLayout.chips(entries: entries, isActive: { $0.id == active },
                                    geometry: Self.geometry, layout: Self.layout)
    }

    @Test("only entries in view get a chip, low to high, at the spectrum's foot above the band plan")
    func inViewAtTheFoot() throws {
        let entries = try document([(id: "high", hz: 7_230_000, locked: false), (id: "low", hz: 7_170_000, locked: false),
                                    (id: "away", hz: 14_074_000, locked: false), (id: "under", hz: 7_100_000, locked: false)])
        let drawn = chips(entries)
        #expect(drawn.map(\.id) == ["low", "high"])
        for chip in drawn {
            #expect(chip.rect.height == BandStackMarkerLayout.height)
            #expect(chip.rect.maxY == Self.layout.strip.minY - BandStackMarkerLayout.footGap)
        }
        let x = CGFloat(Self.geometry.x(forHz: 7_170_000))
        #expect(drawn[0].rect.minX == x - BandStackMarkerLayout.leadIn)
    }

    @Test("close entries take the second row, and a third goes back to the first once it has room")
    func twoRows() throws {
        let entries = try document([(id: "a", hz: 7_170_000, locked: false), (id: "b", hz: 7_171_000, locked: false),
                                    (id: "c", hz: 7_200_000, locked: false)])
        let drawn = chips(entries)
        #expect(!drawn[0].rect.intersects(drawn[1].rect))
        #expect(drawn[1].rect.maxY == drawn[0].rect.minY - (BandStackMarkerLayout.rowStep - BandStackMarkerLayout.height))
        #expect(drawn[2].rect.minY == drawn[0].rect.minY)
    }

    @Test("a chip stays between the band's left edge and the dBm scale")
    func keptOnTheBand() throws {
        let entries = try document([(id: "left", hz: 7_152_000, locked: true), (id: "right", hz: 7_248_000, locked: true)])
        let drawn = chips(entries)
        #expect(drawn[0].rect.minX == BandStackMarkerLayout.edgeGap)
        #expect(drawn[1].rect.maxX == Self.layout.strip.maxX - BandStackMarkerLayout.edgeGap)
        #expect(drawn[1].rect.maxX <= Self.layout.dbmScale.minX)
    }

    @Test("a short spectrum keeps the second row on the band")
    func shortSpectrum() throws {
        let layout = BandLayout(size: CGSize(width: 400, height: 90), scale: 1, stripPoints: 10, spectrumShare: 0.5)
        let geometry = layout.spectrumGeometry(centerHz: 7_200_000, spanHz: 96_000, dbmRange: -140 ... -40)
        let entries = try document([(id: "a", hz: 7_170_000, locked: false), (id: "b", hz: 7_171_000, locked: false)])
        let drawn = BandStackMarkerLayout.chips(entries: entries, isActive: { _ in false }, geometry: geometry, layout: layout)
        #expect(drawn.allSatisfy { $0.rect.minY >= 0 })
    }

    @Test("active and locked are in the chip and in what VoiceOver reads")
    func activeAndLocked() throws {
        let drawn = chips(try document([(id: "net", hz: 7_185_000, locked: true), (id: "ssb", hz: 7_230_250, locked: false)]),
                          active: "net")
        #expect(drawn[0].active && !drawn[1].active)
        #expect(drawn[0].text == "7.185")
        #expect(drawn[1].text == "7.230.250")
        #expect(drawn[0].rect.width == BandStackMarkerLayout.width(text: "7.185", locked: true))
        #expect(drawn[0].rect.width > BandStackMarkerLayout.width(text: "7.185", locked: false))
        #expect(BandStackMarkerLayer.words(drawn[0]) == "Saved spot, 7.185, LSB, locked, active")
        #expect(BandStackMarkerLayer.words(drawn[1]) == "Saved spot, 7.230.250, LSB")
    }

    @Test("they show with the switch on, a Core with a stack, and the radio off the air")
    func showing() {
        typealias Layout = BandStackMarkerLayout
        #expect(Layout.shows(switchOn: true, availability: .ready, onAir: false))
        #expect(!Layout.shows(switchOn: false, availability: .ready, onAir: false))
        #expect(!Layout.shows(switchOn: true, availability: .ready, onAir: true))
        #expect(!Layout.shows(switchOn: true, availability: .needsNewerCore, onAir: false))
        #expect(!Layout.shows(switchOn: true, availability: .notConnected, onAir: false))
    }

    @Test("the switch is off until turned on, stays on this device's store, and is in the pill's On this phone")
    func theSwitch() throws {
        let suite = "BandStackMarkerTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        #expect(!settings.showBandStackMarkers)
        settings.showBandStackMarkers = true
        #expect(PhoneSettings(defaults: defaults).showBandStackMarkers)
        #expect(DisplayPillRows.rows(.view).suffix(4) == [.group("On this phone"), .phone(.ctun), .phone(.dup), .phone(.bandStack)])
        #expect(DisplayPillLayout.rowsDrawn(.view).suffix(1) == [.phone(.bandStack)])
    }
}
