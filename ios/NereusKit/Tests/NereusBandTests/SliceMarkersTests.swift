// NereusSDR for iOS: the slices' markers follow the desktop: own colour when selected, darker and grey otherwise
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Testing
@testable import NereusBand

/// D10, R-IOS-11: the selected slice's centre line, triangle and passband
/// edges in its own colour; the others with a darker line and triangle and
/// grey edges; the selected marker on top; the shaded passband in the
/// operator's colour. The colours are synthetic stand-ins for the
/// catalogue's (D4).
@MainActor
@Suite(.serialized) struct SliceMarkersTests {
    static let cyan = "#00D4FF"
    static let magenta = "#FF40FF"

    static func slice(_ id: Int, _ hz: Double, colour: String) -> BandSlice {
        // The passband ends a kilohertz below the line, clear of it.
        BandSlice(id: id, frequencyHz: hz, filterLowHz: -3000, filterHighHz: -1000, colour: colour, lowerSideband: true)
    }

    // MARK: The styles

    @Test func theSelectedSliceIsDrawnInItsOwnColour() {
        let style = SliceMarkers.style(for: Self.cyan, selected: true)
        #expect(style.selected)
        #expect(style.triangle == SIMD4(0, Float(0xD4) / 255, 1, 1))
        #expect(style.line == SIMD4(0, Float(0xD4) / 255, 1, SliceMarkers.lineAlpha))
        #expect(style.edge == SIMD4(0, Float(0xD4) / 255, 1, SliceMarkers.edgeAlpha))
    }

    @Test func anotherSliceIsDarkerWithGreyEdges() {
        let style = SliceMarkers.style(for: Self.magenta, selected: false)
        #expect(!style.selected)
        #expect(style.triangle == SIMD4(0.5, Float(0x40) / 255 * 0.5, 0.5, 1))
        #expect(style.line.w == SliceMarkers.lineAlpha)
        #expect(style.line.x == style.triangle.x)
        #expect(style.edge == SIMD4(Float(0x80) / 255, Float(0x90) / 255, Float(0xA0) / 255, SliceMarkers.edgeAlpha))
    }

    @Test func theSelectedMarkerComesLastAndItsTriangleHangsFromItsFlag() {
        let slices = [Self.slice(0, 7_200_000, colour: Self.cyan), Self.slice(1, 7_210_000, colour: Self.magenta),
                      Self.slice(2, 7_190_000, colour: "#40FF40")]
        let placements: [FlagPlacement] = [.folded(CGRect(x: 0, y: 117, width: 151, height: 28)),
                                           .full(CGRect(x: 10, y: 0, width: 200, height: 111)),
                                           .folded(CGRect(x: 0, y: 290, width: 151, height: 28))]
        let markers = SliceMarkers.markers(slices: slices, activeSliceId: 1, placements: placements,
                                           spectrumHeightPoints: 300)
        #expect(markers.map(\.sliceId) == [0, 2, 1])
        #expect(markers.last?.style.selected == true)
        #expect(markers[0].triangleTopPoints == 145)
        #expect(markers[2].triangleTopPoints == 111)
        // Kept inside the spectrum.
        #expect(markers[1].triangleTopPoints == 290)
        #expect(markers.allSatisfy { $0.showsPassband })
        let keyed = SliceMarkers.markers(slices: slices, activeSliceId: 1, placements: placements,
                                         spectrumHeightPoints: 300, keyed: true)
        #expect(keyed.allSatisfy { !$0.showsPassband })
    }

    // MARK: Drawn

    /// Two slices on a plain band, A selected: every colour is sampled at
    /// its line.
    @Test func theRenderFollowsD10AtTheLines() throws {
        let settings = BandRendererTests.plain()
        let layout = BandRendererTests.layout(settings)
        let geometry = BandRendererTests.geometry(settings)
        let a = Self.slice(0, 7_195_000, colour: Self.cyan)
        let b = Self.slice(1, 7_210_000, colour: Self.magenta)
        let placements: [FlagPlacement] = [.full(CGRect(x: 0, y: 0, width: 200, height: 20)),
                                           .folded(CGRect(x: 0, y: 40, width: 151, height: 28))]
        let markers = SliceMarkers.markers(slices: [a, b], activeSliceId: 0, placements: placements,
                                           spectrumHeightPoints: CGFloat(geometry.size.height))
        var overlays = BandRendererTests.overlays(settings)
        overlays.markers = markers
        let target = try Offscreen(width: BandRendererTests.width, height: BandRendererTests.height)
        let renderer = try BandRenderer(device: target.device)
        let image = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: overlays)
        let row = Int(geometry.size.height) - 30
        let waterfallRow = Int(layout.waterfall.midY)

        // A's line: its own cyan, in the spectrum and the waterfall.
        for y in [row, waterfallRow] {
            let line = image.pixel(Int(geometry.x(forHz: a.frequencyHz)), y)
            #expect(line.r < 20 && line.g > 170 && line.b > 210, "A's line \(line) at row \(y)")
        }
        // B's line: magenta at half its brightness.
        for y in [row, waterfallRow] {
            let line = image.pixel(Int(geometry.x(forHz: b.frequencyHz)), y)
            #expect((95...135).contains(line.r) && line.g < 50 && (95...140).contains(line.b), "B's line \(line)")
        }
        // A's edges in cyan, B's in grey, over the shaded passband.
        let aEdge = image.pixel(Int(geometry.x(forHz: a.passbandHz.lowerBound).rounded(.down)), row)
        #expect(aEdge.r < 25 && aEdge.b > 150, "A's edge \(aEdge)")
        let bEdge = image.pixel(Int(geometry.x(forHz: b.passbandHz.lowerBound).rounded(.down)), row)
        #expect(bEdge.r > 50 && bEdge.b > bEdge.r && abs(bEdge.g - bEdge.r) < 60, "B's edge \(bEdge)")
        // The shading is the operator's colour for both slices.
        let expected = Self.over(settings.passbandColour, alpha: settings.passbandOpacity)
        for slice in [a, b] {
            let middle = (slice.passbandHz.lowerBound + slice.passbandHz.upperBound) / 2
            let shade = image.pixel(Int(geometry.x(forHz: middle)), row)
            #expect(abs(shade.r - expected.r) <= 8 && abs(shade.g - expected.g) <= 8 && abs(shade.b - expected.b) <= 8,
                    "shade \(shade) against \(expected)")
        }
        // The triangles hang from their flags, opaque: A's cyan, B's darker magenta.
        let aTriangle = image.pixel(Int(geometry.x(forHz: a.frequencyHz)), 22)
        #expect(aTriangle.r < 10 && aTriangle.g > 200 && aTriangle.b > 245, "A's triangle \(aTriangle)")
        let bTriangle = image.pixel(Int(geometry.x(forHz: b.frequencyHz)), 70)
        #expect((120...135).contains(bTriangle.r) && bTriangle.g < 40 && (120...135).contains(bTriangle.b),
                "B's triangle \(bTriangle)")
    }

    @Test func theSelectedMarkerIsDrawnOnTop() throws {
        let settings = BandRendererTests.plain()
        let geometry = BandRendererTests.geometry(settings)
        // Two slices on one frequency: whichever is selected shows.
        let a = Self.slice(0, 7_200_000, colour: Self.cyan)
        let b = Self.slice(1, 7_200_000, colour: Self.magenta)
        let target = try Offscreen(width: BandRendererTests.width, height: BandRendererTests.height)
        let renderer = try BandRenderer(device: target.device)
        let x = Int(geometry.x(forHz: 7_200_000))
        for (active, cyanOnTop) in [(0, true), (1, false)] {
            var overlays = BandRendererTests.overlays(settings)
            overlays.markers = SliceMarkers.markers(slices: [a, b], activeSliceId: active, placements: [],
                                                    spectrumHeightPoints: CGFloat(geometry.size.height))
            let image = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                          overlays: overlays)
            let line = image.pixel(x, 200)
            if cyanOnTop {
                #expect(line.r < 40 && line.g > 150, "cyan on top \(line)")
            } else {
                #expect(line.r > 200 && line.g < 110, "magenta on top \(line)")
            }
        }
    }

    /// `colour` at `alpha` over the band's background, as bytes.
    static func over(_ colour: String, alpha: Double) -> (r: Int, g: Int, b: Int) {
        let c = BandPalette.rgba(colour) ?? SIMD4(0, 0, 0, 1)
        let bg = BandRenderer.background
        func mix(_ fg: Float, _ back: Float) -> Int {
            Int(((fg * Float(alpha) + back * (1 - Float(alpha))) * 255).rounded())
        }
        return (mix(c.x, bg.x), mix(c.y, bg.y), mix(c.z, bg.z))
    }
}
