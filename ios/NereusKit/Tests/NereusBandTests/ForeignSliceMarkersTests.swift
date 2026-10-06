// NereusSDR for iOS: another device's slice on the band: a dashed line, a hollow triangle, dashed grey edges, no shading
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Testing
@testable import NereusBand

/// D46, R-IOS-17, spec section 5.8 item 1: another device's slice drawn
/// read-only. The colours are synthetic stand-ins for the catalogue's (D4).
@MainActor
@Suite(.serialized) struct ForeignSliceMarkersTests {
    static let magenta = "#FF40FF"

    static func slice(_ id: Int = 1, _ hz: Double = 7_210_000) -> ForeignSliceMarkers.Slice {
        // The passband ends a kilohertz below the line, clear of it.
        ForeignSliceMarkers.Slice(id: id, frequencyHz: hz, filterLowHz: -3000, filterHighHz: -1000, colour: magenta,
                                  ownerName: "MacBook Pro", ownerShortName: "MacBook")
    }

    @Test func itsStyleIsItsLettersColourWithGreyEdges() {
        let style = ForeignSliceMarkers.style(for: Self.magenta)
        #expect(style.line == SIMD4(1, Float(0x40) / 255, 1, ForeignSliceMarkers.lineAlpha))
        #expect(style.triangle == SIMD4(1, Float(0x40) / 255, 1, 1))
        #expect(style.edge == SIMD4(Float(0x80) / 255, Float(0x90) / 255, Float(0xA0) / 255,
                                    ForeignSliceMarkers.edgeAlpha))
        let marker = ForeignSliceMarkers.markers([Self.slice()])[0]
        #expect(marker.foreign)
        #expect(!marker.showsPassband)
        #expect(marker.triangleTopPoints == 0)
        #expect(Self.slice().letter == "B")
    }

    @Test func theLabelIsCentredOnItsLineAndKeptInsideTheBand() {
        #expect(ForeignSliceMarkers.labelX(centreX: 200, labelWidth: 80, bandWidth: 400) == 160)
        #expect(ForeignSliceMarkers.labelX(centreX: 10, labelWidth: 80, bandWidth: 400) == 4)
        #expect(ForeignSliceMarkers.labelX(centreX: 395, labelWidth: 80, bandWidth: 400) == 316)
        // A label wider than the band starts at the inset.
        #expect(ForeignSliceMarkers.labelX(centreX: 50, labelWidth: 500, bandWidth: 400) == 4)
        #expect(Self.slice().labelName == "MacBook")
        var nameless = Self.slice()
        nameless.ownerShortName = ""
        #expect(nameless.labelName == "MacBook Pro")
    }

    /// Who holds a slice, in the Core's words for a holder
    /// (StationServer::sliceHolderWords, StationServer.cpp:9059-9083 at
    /// c13abe564): the device's own name as sent; "the Core" for kind
    /// `station`, which the Core sends with no name for a slice it holds
    /// itself or no device holds (StationServer.cpp:3295-3297); "a" and the
    /// Core's lowercase kind word (DeviceSessionRegistry::kindWord, any
    /// kind but phone and tablet reads as a computer); "another device"
    /// when neither is known. No label is ever blank.
    @Test(arguments: [
        // kind, name, short name, holder words, label
        ("station", "", "", "the Core", "the Core"),
        // A station owner is the Core's even if a name came with it.
        ("station", "Shack", "", "the Core", "the Core"),
        ("phone", "", "", "a phone", "a phone"),
        ("tablet", "", "", "a tablet", "a tablet"),
        ("computer", "", "", "a computer", "a computer"),
        ("watch", "", "", "a computer", "a computer"),
        ("", "", "", "another device", "another device"),
        ("computer", "MacBook Pro", "MacBook", "MacBook Pro", "MacBook"),
        ("computer", "MacBook Pro", "", "MacBook Pro", "MacBook Pro"),
        ("phone", "Grant's iPhone", "", "Grant's iPhone", "Grant's iPhone"),
        ("", "", "Tablet A", "Tablet A", "Tablet A"),
    ])
    func theHolderInTheCoresWords(_ words: (String, String, String, String, String)) {
        let (kind, name, shortName, holder, label) = words
        var slice = Self.slice()
        slice.ownerName = name
        slice.ownerShortName = shortName
        slice.ownerKind = kind
        #expect(slice.holderWords == holder)
        #expect(slice.labelName == label)
        #expect(slice.ownerIsCore == (kind == "station"))
        #expect(slice.ownerNamed == (kind != "station" && !(name.isEmpty && shortName.isEmpty)))
        #expect(!slice.labelName.isEmpty)
    }

    /// The Core's kind words, as DeviceSessionRegistry::kindWord gives them.
    @Test func theCoresKindWords() {
        #expect(ForeignSliceMarkers.Slice.kindWord("phone") == "Phone")
        #expect(ForeignSliceMarkers.Slice.kindWord("tablet") == "Tablet")
        #expect(ForeignSliceMarkers.Slice.kindWord("computer") == "Computer")
        #expect(ForeignSliceMarkers.Slice.kindWord("") == "Computer")
    }

    /// Drawn on a plain band: the line is dashed in its colour, the triangle
    /// hollow, the edges dashed grey, the passband unshaded.
    @Test func theRenderIsDashedHollowAndUnfilled() throws {
        let settings = BandRendererTests.plain()
        let geometry = BandRendererTests.geometry(settings)
        let slice = Self.slice()
        var overlays = BandRendererTests.overlays(settings)
        overlays.markers = ForeignSliceMarkers.markers([slice])
        let target = try Offscreen(width: BandRendererTests.width, height: BandRendererTests.height)
        let renderer = try BandRenderer(device: target.device)
        let image = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: overlays)
        let background = image.pixel(20, 150)
        func isMagenta(_ p: Image.Pixel) -> Bool { p.r > 180 && p.g < 90 && p.b > 180 }
        func isBackground(_ p: Image.Pixel) -> Bool {
            abs(p.r - background.r) <= 3 && abs(p.g - background.g) <= 3 && abs(p.b - background.b) <= 3
        }

        // The line: dashes of magenta with the band between them.
        let lineX = Int(geometry.x(forHz: slice.frequencyHz))
        let lineRows = (20..<220).map { image.pixel(lineX, $0) }
        #expect(lineRows.filter(isMagenta).count > 60, "the line's dashes")
        #expect(lineRows.filter(isBackground).count > 40, "the gaps between them")

        // The triangle: a magenta ring at its top edge, the band's dark inside it.
        let ring = image.pixel(lineX - 4, 0)
        #expect(isMagenta(ring), "the ring \(ring)")
        let inside = image.pixel(lineX + 1, 3)
        #expect(inside.r < 40 && inside.g < 40 && inside.b < 50, "inside the triangle \(inside)")

        // The edges: grey dashes; between them, the band.
        let edgeX = Int(geometry.x(forHz: slice.passbandHz.lowerBound).rounded(.down))
        let edgeRows = (20..<220).map { image.pixel(edgeX, $0) }
        #expect(edgeRows.filter { $0.r > background.r + 20 && $0.b > $0.r }.count > 40, "the edge's dashes")
        #expect(edgeRows.filter(isBackground).count > 40, "the gaps between them")

        // The passband is not shaded: its middle is the band itself.
        let middle = Int(geometry.x(forHz: (slice.passbandHz.lowerBound + slice.passbandHz.upperBound) / 2))
        for y in [100, 150, 200] {
            #expect(isBackground(image.pixel(middle, y)), "the passband at row \(y)")
        }
    }

    /// Another device's marker never covers this device's own: the own
    /// markers come after it and draw on top.
    @Test func theOwnSlicesMarkerDrawsOverIt() throws {
        let settings = BandRendererTests.plain()
        let geometry = BandRendererTests.geometry(settings)
        let own = BandSlice(id: 0, frequencyHz: 7_210_000, filterLowHz: -3000, filterHighHz: -1000, colour: "#00D4FF",
                            lowerSideband: true)
        var overlays = BandRendererTests.overlays(settings)
        overlays.markers = ForeignSliceMarkers.markers([Self.slice()])
            + SliceMarkers.markers(slices: [own], activeSliceId: 0, placements: [],
                                   spectrumHeightPoints: CGFloat(geometry.size.height))
        let target = try Offscreen(width: BandRendererTests.width, height: BandRendererTests.height)
        let renderer = try BandRenderer(device: target.device)
        let image = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: overlays)
        let x = Int(geometry.x(forHz: 7_210_000))
        for y in stride(from: 30, to: 200, by: 7) {
            let line = image.pixel(x, y)
            #expect(line.r < 40 && line.g > 150, "own line on top at row \(y): \(line)")
        }
    }
}
