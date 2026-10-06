// NereusSDR for iOS: which band plan the strip shows: the Core's active plan, then its setting, then the default
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels
import Testing
@testable import NereusBand

/// D79, R-IOS-27: the band plan is the Core's. The strip shows the plan the
/// Core marks `active`; from a Core that marks none, the plan its
/// `BandPlanName` setting names; otherwise the catalogue's default. Every
/// plan here is synthetic (D4).
@Suite struct BandPlanChoiceTests {
    static func plans(active: String?) throws -> [StationCatalog.BandPlan] {
        try [("usual", "Usual (US)", true), ("east", "East", false), ("west", "West", false)].map { id, name, isDefault in
            try BandFixtures.plan(id: id, name: name, isDefault: isDefault,
                                  active: active.map { $0 == id }, licensed: [])
        }
    }

    @Test func aCoreThatMarksAPlanActiveIsFollowedWhateverItsSettingSays() throws {
        let plans = try Self.plans(active: "west")
        #expect(BandPlanStrip.plan(in: plans, stationPlanName: nil)?.id == "west")
        #expect(BandPlanStrip.plan(in: plans, stationPlanName: "East")?.id == "west")
        #expect(BandPlanStrip.plan(in: plans, stationPlanName: "Gone")?.id == "west")
    }

    @Test func aCoreThatMarksNoneIsFollowedByItsSetting() throws {
        let plans = try Self.plans(active: nil)
        #expect(plans.allSatisfy { !$0.isActive })
        #expect(BandPlanStrip.plan(in: plans, stationPlanName: "East")?.id == "east")
        #expect(BandPlanStrip.plan(in: plans, stationPlanName: "West")?.id == "west")
        // The setting names a plan by its name, not its id.
        #expect(BandPlanStrip.plan(in: plans, stationPlanName: "east")?.id == "usual")
    }

    @Test func withoutActiveOrAKnownSettingTheDefaultShows() throws {
        let marksNone = try Self.plans(active: "nowhere")
        #expect(marksNone.allSatisfy { !$0.isActive })
        #expect(BandPlanStrip.plan(in: marksNone, stationPlanName: nil)?.id == "usual")
        #expect(BandPlanStrip.plan(in: marksNone, stationPlanName: "Gone")?.id == "usual")
        #expect(BandPlanStrip.plan(in: try Self.plans(active: nil), stationPlanName: "")?.id == "usual")
        #expect(BandPlanStrip.plan(in: [], stationPlanName: "East") == nil)
    }

    @Test func thePlansActiveAndSpotsReadAndAnOlderCoresPlanStillReads() throws {
        let plan = try BandFixtures.plan(id: "p", isDefault: false, active: true, licensed: [],
                                         spots: [(7_074_000, "FT8"), (7_040_000, "Beacon")])
        #expect(plan.isActive)
        #expect(plan.spots == [.init(hz: 7_074_000, label: "FT8"), .init(hz: 7_040_000, label: "Beacon")])
        let older = try BandFixtures.plan(id: "o", isDefault: true, licensed: [])
        #expect(!older.isActive)
        #expect(older.spots.isEmpty)
        // Spots that do not read cost only the dots.
        let object: [String: Any] = ["id": "u", "name": "U", "default": false, "segments": [], "spots": "none"]
        let unreadable = try JSONDecoder().decode(StationCatalog.BandPlan.self,
                                                  from: JSONSerialization.data(withJSONObject: object))
        #expect(unreadable.spots.isEmpty)
    }
}
