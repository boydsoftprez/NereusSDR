// NereusSDR for iOS: tests for the phone's half of the display budget
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusMedia

/// R-IOS-11 and the budget design's agreed quality policy.
@Suite struct DisplayQualityAllocatorTests {
    typealias Allocator = DisplayQualityAllocator

    static let plenty: UInt64 = 1_000_000_000

    static func pan(_ id: String, active: Bool = false, pixels: Int = 1000, fps: Int = 30,
                    extras: DisplayExtrasRequest? = nil) -> Allocator.Intent {
        Allocator.Intent(panId: id, pixels: pixels, fps: fps, waterfallPeriodMs: 120, active: active, extras: extras)
    }

    /// A budget limited by spectrum samples a second.
    static func samples(_ units: UInt64, generation: UInt32 = 1) -> Allocator.Budget {
        Allocator.Budget(applicationBytesPerSecond: plenty, spectrumSampleUnitsPerSecond: units, generation: generation)
    }

    static func allocate(_ budget: Allocator.Budget?, _ intents: [Allocator.Intent]) throws -> Allocator.Allocation {
        try Allocator.allocate(budget: budget, intents: intents).get()
    }

    static func quality(_ allocation: Allocator.Allocation, _ pan: String) throws -> Allocator.Quality {
        try #require(allocation.quality(forPan: pan))
    }

    @Test func theChargeIsTheCoresFormula() {
        let plain = Allocator.charge(pixels: 1179, fps: 30, includesWidePlane: false, extras: nil)
        #expect(plain == .init(applicationBytesPerSecond: 75_180, spectrumSampleUnitsPerSecond: 70_740,
                               messagesPerSecond: 30))
        let wide = Allocator.charge(pixels: 1179, fps: 30, includesWidePlane: true, extras: nil)
        #expect(wide == .init(applicationBytesPerSecond: 99_210, spectrumSampleUnitsPerSecond: 93_780,
                              messagesPerSecond: 30))
        let hold = DisplayExtrasRequest(activePeakHold: .init(enabled: true, holdMs: 2000, fallDbPerSec: 6))
        let extras = Allocator.charge(pixels: 1179, fps: 30, includesWidePlane: false, extras: hold)
        let bytes: UInt64 = 75_180 + 1_252 * 30
        let units: UInt64 = 70_740 + 1_179 * 30
        let expected = Allocator.Charge(applicationBytesPerSecond: bytes, spectrumSampleUnitsPerSecond: units,
                                        messagesPerSecond: 60)
        #expect(extras == expected)
        // Asking for nothing costs nothing more.
        #expect(Allocator.charge(pixels: 1179, fps: 30, includesWidePlane: false, extras: DisplayExtrasRequest())
                == plain)
        #expect(Allocator.framesPerLine(periodMs: 120, fps: 30) == 4)
        #expect(Allocator.framesPerLine(periodMs: 120, fps: 10) == 2)
    }

    @Test func aBudgetThatFitsChangesNothing() throws {
        let intents = [Self.pan("a", active: true), Self.pan("b")]
        for budget in [nil, Self.samples(Self.plenty)] {
            let allocation = try Self.allocate(budget, intents)
            for pan in ["a", "b"] {
                let quality = try Self.quality(allocation, pan)
                #expect(quality.pixels == 1000 && quality.fps == 30 && !quality.reduced && !quality.suspended)
                #expect(quality.framesPerLine == 4)
            }
        }
    }

    @Test func theBackgroundFrameRateGoesFirstOneUnitPerPaneInPanOrder() throws {
        let intents = [Self.pan("c"), Self.pan("a", active: true), Self.pan("b")]
        // Everything asks 180,000 samples a second; each frame a second off a pane saves 2,000.
        var allocation = try Self.allocate(Self.samples(178_000), intents)
        #expect(try Self.quality(allocation, "b").fps == 29)
        #expect(try Self.quality(allocation, "c").fps == 30)
        #expect(try Self.quality(allocation, "a").fps == 30)
        allocation = try Self.allocate(Self.samples(176_000), intents)
        #expect(try Self.quality(allocation, "b").fps == 29)
        #expect(try Self.quality(allocation, "c").fps == 29)
        #expect(allocation.pans.map(\.pixels) == [1000, 1000, 1000])
        #expect(allocation.pans.map(\.panId) == ["a", "b", "c"])
    }

    @Test func backgroundsStopAtTheFloorBeforeTheActiveBandIsLowered() throws {
        let intents = [Self.pan("a", active: true), Self.pan("b")]
        // The active band whole and the background at its floor, 256 by 10.
        var allocation = try Self.allocate(Self.samples(60_000 + 5_120), intents)
        var background = try Self.quality(allocation, "b")
        #expect(background.pixels == 256 && background.fps == 10 && background.reduced)
        #expect(background.framesPerLine == 2)
        var active = try Self.quality(allocation, "a")
        #expect(active.pixels == 1000 && active.fps == 30 && !active.reduced)
        // One sample less and the active band gives up a frame, not a pixel.
        allocation = try Self.allocate(Self.samples(60_000 + 5_119), intents)
        background = try Self.quality(allocation, "b")
        active = try Self.quality(allocation, "a")
        #expect(background.pixels == 256 && background.fps == 10)
        #expect(active.fps == 29 && active.pixels == 1000)
        // A pan that asks for less than the floor keeps what it asked for.
        let small = try Self.allocate(Self.samples(1), [Self.pan("a", active: true, pixels: 100, fps: 5)])
        #expect(try Self.quality(small, "a").suspended)
        let fits = try Self.quality(Self.allocate(Self.samples(1_000), [Self.pan("a", active: true, pixels: 100, fps: 5)]),
                                    "a")
        #expect(fits.pixels == 100 && fits.fps == 5)
    }

    @Test func whenTheFloorsDoNotFitBackgroundsAreSuspendedLastFirstThenTheActiveBand() throws {
        let intents = [Self.pan("a", active: true), Self.pan("b"), Self.pan("c")]
        var allocation = try Self.allocate(Self.samples(10_240), intents)
        var a = try Self.quality(allocation, "a")
        var b = try Self.quality(allocation, "b")
        var c = try Self.quality(allocation, "c")
        #expect(c.suspended)
        #expect(!b.suspended && b.pixels == 256)
        #expect(a.fps == 10 && a.pixels == 256)
        allocation = try Self.allocate(Self.samples(5_120), intents)
        a = try Self.quality(allocation, "a")
        b = try Self.quality(allocation, "b")
        c = try Self.quality(allocation, "c")
        #expect(c.suspended && b.suspended)
        #expect(!a.suspended)
        // The active band is paused only when its own floor does not fit.
        allocation = try Self.allocate(Self.samples(5_119), intents)
        let everyPaused = allocation.pans.allSatisfy { $0.suspended }
        #expect(everyPaused)
        #expect(allocation.total == .init())
    }

    @Test func askingForExtrasMakesTheSameBudgetFitFewerFrames() throws {
        let levels = DisplayExtrasRequest(waterfallLevels: .init(mode: .clarity, lowDbm: -122, highDbm: -62,
                                                                 offsetDb: 0))
        let budget = Allocator.Budget(applicationBytesPerSecond: 30 * 2_128, spectrumSampleUnitsPerSecond: Self.plenty,
                                      generation: 1)
        let without = try Self.allocate(budget, [Self.pan("a", active: true)])
        #expect(try Self.quality(without, "a").fps == 30)
        let with = try Self.allocate(budget, [Self.pan("a", active: true, extras: levels)])
        #expect(try Self.quality(with, "a").fps == 29)
        #expect(try Self.quality(with, "a").pixels == 1000)
    }

    @Test func aLargerBudgetLaterRestoresTheRequestedQuality() {
        var allocator = Allocator(budget: Self.samples(65_120))
        allocator.update(intents: [Self.pan("a", active: true), Self.pan("b")])
        #expect(allocator.allocation.quality(forPan: "b")?.fps == 10)
        allocator.update(budget: Self.samples(Self.plenty, generation: 2))
        #expect(allocator.allocation.quality(forPan: "b")?.fps == 30)
        #expect(allocator.allocation.quality(forPan: "b")?.pixels == 1000)
        #expect(allocator.allocation.budgetGeneration == 2)
        // Focus moves: the other pan is now the one kept whole.
        allocator.update(budget: Self.samples(65_120, generation: 3))
        allocator.update(intents: [Self.pan("a"), Self.pan("b", active: true)])
        #expect(allocator.allocation.quality(forPan: "a")?.fps == 10)
        #expect(allocator.allocation.quality(forPan: "b")?.fps == 30)
    }

    @Test func malformedWishesKeepTheLastAllocation() {
        var allocator = Allocator()
        allocator.update(intents: [Self.pan("a", active: true)])
        let good = allocator.allocation
        allocator.update(intents: [Self.pan("a", active: true), Self.pan("b", active: true)])
        #expect(allocator.lastProblem == .moreThanOneActive)
        #expect(allocator.allocation == good)
        allocator.update(intents: [Self.pan("a"), Self.pan("a")])
        #expect(allocator.lastProblem == .duplicatePan)
        allocator.update(intents: (0..<9).map { Self.pan("p\($0)") })
        #expect(allocator.lastProblem == .tooManyPans)
        allocator.update(intents: [Self.pan("a", pixels: 5000)])
        #expect(allocator.lastProblem == .outOfRange)
    }

    @Test func theBudgetIsReadFromTheCoresCapabilities() {
        let open = MediaFeatureGates(agreedMinor: 11) { _ in 1 }
        var values: [String: Int64] = ["displayApplicationBytesPerSecond": 500_000,
                                       "spectrumSampleUnitsPerSecond": 200_000, "displayBudgetGeneration": 4]
        var reason: String? = "coreBusy"
        func budget(_ gates: MediaFeatureGates) -> Allocator.Budget? {
            Allocator.Budget(gates: gates, integer: { values[$0] }, text: { $0 == "displayBudgetReason" ? reason : nil })
        }
        #expect(budget(open) == .init(applicationBytesPerSecond: 500_000, spectrumSampleUnitsPerSecond: 200_000,
                                      generation: 4, reason: .coreBusy))
        reason = "sharedConnection"
        #expect(budget(open)?.reason == .sharedConnection)
        reason = "sharedProcessing"
        #expect(budget(open)?.reason == .sharedProcessing)
        reason = "somethingLater"
        #expect(budget(open)?.reason == .unknown("somethingLater"))
        reason = nil
        #expect(budget(open)?.reason == Allocator.Budget.Reason.none)
        // No budget wire, a partial descriptor or a zero: no budget, never a zero one.
        #expect(budget(MediaFeatureGates(agreedMinor: 6) { _ in 1 }) == nil)
        values["spectrumSampleUnitsPerSecond"] = 0
        #expect(budget(open) == nil)
        values["spectrumSampleUnitsPerSecond"] = nil
        #expect(budget(open) == nil)
    }

    @Test func anAllocationResultIsNotedAndSendsNothing() {
        var allocator = Allocator(budget: Self.samples(Self.plenty))
        let refused = MediaControlEvent.AllocationResult(endpointId: 1, revision: 2, accepted: false,
                                                         reason: "The Core's display limit has no room left.",
                                                         budgetGeneration: 2, acceptedRevision: 1,
                                                         applicationBytesPerSecond: 0,
                                                         spectrumSampleUnitsPerSecond: 0, messagesPerSecond: 0)
        let wasRefusal = allocator.note(refused)
        #expect(wasRefusal)
        #expect(allocator.lastRefusal == refused)
        #expect(allocator.awaitingBudget)
        allocator.update(budget: Self.samples(Self.plenty, generation: 2))
        #expect(!allocator.awaitingBudget)
        #expect(allocator.lastRefusal == nil)
    }

    // MARK: Every pan of one device on one budget (R-IOS-59, spec 6.3)

    @Test @MainActor func twoPansShareOneBudgetBothFramedNeitherPastItsShare() throws {
        let budget: UInt64 = 100_000
        let shared = SharedDisplayAllocator(budget: Self.samples(budget))
        var told = 0
        shared.observe(pan: "1") { told += 1 }
        shared.observe(pan: "2") { told += 1 }
        // Each subscriber offers its own wish; each asks for 60,000 samples a second.
        shared.update(intent: Self.pan("1", active: true), forPan: "1")
        shared.update(intent: Self.pan("2", active: true), forPan: "2")
        #expect(shared.lastProblem == nil)
        #expect(shared.intents.map(\.panId) == ["1", "2"])
        // With none chosen, the first pan is the active one, and only it.
        #expect(shared.intents.map(\.active) == [true, false])
        let one = try #require(shared.quality(forPan: "1"))
        let two = try #require(shared.quality(forPan: "2"))
        for quality in [one, two] {
            #expect(!quality.suspended && quality.fps > 0 && quality.pixels > 0)
        }
        #expect(one.pixels == 1000 && one.fps == 30 && !one.reduced)
        #expect(two.reduced)
        let total = one.charge.spectrumSampleUnitsPerSecond + two.charge.spectrumSampleUnitsPerSecond
        #expect(total == shared.allocation.total.spectrumSampleUnitsPerSecond)
        #expect(total <= budget)
        #expect(told > 0)

        // The operator touches Pan 2: it is kept whole, Pan 1 gives way.
        told = 0
        shared.activePanId = "2"
        #expect(told == 2)
        #expect(shared.intents.map(\.active) == [false, true])
        #expect(shared.quality(forPan: "2")?.reduced == false)
        #expect(shared.quality(forPan: "1")?.reduced == true)
        #expect(shared.allocation.total.spectrumSampleUnitsPerSecond <= budget)

        // The same wish again moves nothing and tells nobody.
        told = 0
        shared.update(intent: Self.pan("1"), forPan: "1")
        #expect(told == 0)
    }

    /// A pan's subscriber, as the shared allocator keys its claim.
    private final class Claimant {}

    @Test @MainActor func aPansIncreaseGoesOnlyIntoTheHeadroomTheOthersLeave() throws {
        let shared = SharedDisplayAllocator(budget: Self.samples(100_000))
        let first = Claimant(), second = Claimant(), third = Claimant()
        #expect(shared.headroom(for: ObjectIdentifier(second))?.spectrumSampleUnitsPerSecond == 100_000)
        shared.claim(.init(applicationBytesPerSecond: 70_000, spectrumSampleUnitsPerSecond: 60_000,
                           messagesPerSecond: 30), by: ObjectIdentifier(first))
        let left = try #require(shared.headroom(for: ObjectIdentifier(second)))
        #expect(left.spectrumSampleUnitsPerSecond == 40_000)
        #expect(left.applicationBytesPerSecond == Self.plenty - 70_000)
        // A subscriber's own claim is its own headroom, not the others'.
        #expect(shared.headroom(for: ObjectIdentifier(first))?.spectrumSampleUnitsPerSecond == 100_000)
        // A claim past the budget leaves nothing, never a wrapped number.
        shared.claim(.init(spectrumSampleUnitsPerSecond: 200_000), by: ObjectIdentifier(third))
        #expect(shared.headroom(for: ObjectIdentifier(second))?.spectrumSampleUnitsPerSecond == 0)
        // Without a budget there is no headroom to keep to.
        #expect(SharedDisplayAllocator().headroom(for: ObjectIdentifier(first)) == nil)
    }

    @Test @MainActor func aClosedPansWishGoesAndItsClaimStandsUntilItsSubscriberHoldsNothing() {
        let shared = SharedDisplayAllocator(budget: Self.samples(65_120))
        let first = Claimant(), second = Claimant()
        shared.update(intents: [Self.pan("2"), Self.pan("1")])
        #expect(shared.allocation.pans.map(\.panId) == ["1", "2"])
        #expect(shared.quality(forPan: "2")?.fps == 10)
        shared.claim(.init(spectrumSampleUnitsPerSecond: 5_120), by: ObjectIdentifier(second))
        shared.remove(pan: "2")
        #expect(shared.allocation.pans.map(\.panId) == ["1"])
        #expect(shared.quality(forPan: "1")?.reduced == false)
        // The closed pan's endpoint is still held until the Core lets it go.
        #expect(shared.headroom(for: ObjectIdentifier(first))?.spectrumSampleUnitsPerSecond == 60_000)
        shared.claim(.init(), by: ObjectIdentifier(second))
        #expect(shared.headroom(for: ObjectIdentifier(first))?.spectrumSampleUnitsPerSecond == 65_120)
        // An active pan with no wish leaves the first pan active.
        shared.activePanId = "2"
        #expect(shared.intents.map(\.active) == [true])
    }

    @Test @MainActor func aRetiringPansEmptyClaimLeavesTheReopenedPansClaimInPlace() {
        let shared = SharedDisplayAllocator(budget: Self.samples(100_000))
        let first = Claimant(), retiring = Claimant(), reopened = Claimant()
        // Pan 2 closed, its subscriber still letting its endpoint go, and Pan 2 opened again.
        shared.claim(.init(spectrumSampleUnitsPerSecond: 30_000), by: ObjectIdentifier(retiring))
        shared.remove(pan: "2")
        shared.update(intent: Self.pan("2"), forPan: "2")
        shared.claim(.init(spectrumSampleUnitsPerSecond: 50_000), by: ObjectIdentifier(reopened))
        #expect(shared.headroom(for: ObjectIdentifier(first))?.spectrumSampleUnitsPerSecond == 20_000)
        // The Core let the old endpoint go: only the retiring subscriber's claim goes.
        shared.claim(.init(), by: ObjectIdentifier(retiring))
        #expect(shared.headroom(for: ObjectIdentifier(first))?.spectrumSampleUnitsPerSecond == 50_000)
        #expect(shared.quality(forPan: "2") != nil)
    }

    @Test @MainActor func everyPansEndpointIdsComeFromOneRisingCounterUntilTheConnectionEnds() {
        let shared = SharedDisplayAllocator()
        // Pan 1, then Pan 2, then Pan 1 again after a release: always higher.
        #expect(shared.takeEndpointId() == 1)
        #expect(shared.takeEndpointId() == 2)
        #expect(shared.takeEndpointId() == 3)
        // A new media connection starts again from 1.
        shared.restartEndpointIds()
        #expect(shared.takeEndpointId() == 1)
    }
}
