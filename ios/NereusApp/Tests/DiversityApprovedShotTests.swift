// NereusSDR for iOS: approved native Diversity geometry using the production drawing sizes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing
import UIKit

@Suite("Approved Diversity native geometry", .serialized)
@MainActor
struct DiversityApprovedShotTests {
    @Test("full and folded live flags fit the native phone and iPad band viewports")
    func fullFoldedViewportAndUnchangedTxGeometry() throws {
        var records: [[String: Any]] = []
        for size in [CGSize(width: 402, height: 640), CGSize(width: 874, height: 300),
                     CGSize(width: 1024, height: 1100), CGSize(width: 1366, height: 850)] {
            for large in [false, true] {
                for active in [0, 1] {
                    let store = DiversityApprovedTests.store()
                    // Put all three joined slices close together to exercise the real fold rule.
                    for id in 0...2 {
                        store.apply(.delta(.init(key: "slice:\(id)", properties: [
                            .init(ordinal: 1, name: "frequency", value: .f64(7_236_400 + Double(id) * 100)),
                            .init(ordinal: 11, name: "active", value: .bool(id == active)),
                        ])))
                    }
                    let model = BandSlicesModel(store: store, commands: nil)
                    model.thisDeviceId = "phone"
                    let entries = model.entries
                    let metrics = FlagMetrics(large: large)
                    let geometry = BandGeometry(centerHz: 7_244_500, spanHz: 48_000, size: size, dbmRange: -140 ... -40)
                    // Production BandGestureLayer must use this same helper for its foldedSize closure.
                    let placements = FlagLayout.layout(slices: entries.map(\.slice), activeSliceId: active,
                        geometry: geometry, foldedSize: { slice in
                            FoldedTagView.size(for: entries.first { $0.id == slice.id }!)
                        }, flagHeight: { slice in
                            let entry = entries.first { $0.id == slice.id }!
                            return metrics.height(rade: entry.rade != nil, listening: entry.listening)
                        })
                    let viewport = CGRect(origin: .zero, size: size)
                    #expect(entries.filter(\.diversityOn).map(\.id) == [1])
                    #expect(metrics.width == 200)
                    #expect(metrics.transmit == CGRect(x: 129, y: metrics.headerY, width: 44, height: 44))
                    #expect(metrics.antenna == CGRect(x: 5, y: metrics.headerY, width: 80, height: 44))
                    #expect(metrics.diversity.width == 44 && metrics.diversity.height == 44)
                    #expect(!metrics.diversity.intersects(metrics.transmit))
                    #expect(!metrics.diversity.intersects(metrics.antenna))
                    for (entry, placement) in zip(entries, placements) {
                        #expect(viewport.contains(placement.rect), "\(size) active\(active) slice\(entry.id)")
                        if placement.isFolded {
                            let drawn = FoldedTagView.size(for: entry)
                            #expect(placement.rect.size == drawn)
                            #expect(drawn.height == (entry.diversityOn ? 44 : 28))
                            if entry.diversityOn {
                                #expect(drawn.width >= FlagLayout.foldedSize(frequencyText: entry.slice.frequencyText).width + 44)
                            }
                        }
                        records.append(["width": size.width, "height": size.height, "large": large,
                                        "active": active, "slice": entry.id, "diversity": entry.diversityOn,
                                        "folded": placement.isFolded, "rect": [placement.rect.minX, placement.rect.minY,
                                        placement.rect.width, placement.rect.height]])
                    }
                }
            }
        }
        if let root = ProcessInfo.processInfo.environment["NEREUS_DIVERSITY_SHOTS"] {
            let data = try JSONSerialization.data(withJSONObject: records, options: [.prettyPrinted, .sortedKeys])
            try data.write(to: URL(fileURLWithPath: root).appendingPathComponent("diversity-native-band-geometry.json"))
        }
    }

    @Test("Diversity light validation requires its dedicated fixture and its own explicit argument")
    func dedicatedLightDoubleGuard() {
        #expect(UITestDiversityFixture.scheme([]) == .dark)
        #expect(UITestDiversityFixture.scheme([UITestDiversityFixture.lightArgument]) == .dark)
        #expect(UITestDiversityFixture.scheme([UITestDiversityFixture.argument]) == .dark)
        #expect(UITestDiversityFixture.scheme([UITestDiversityFixture.argument, "-NereusShowBand", "-NereusTxPanelLightValidation"]) == .dark)
        #expect(UITestDiversityFixture.scheme([UITestDiversityFixture.argument, UITestDiversityFixture.lightArgument]) == .light)
    }
}
