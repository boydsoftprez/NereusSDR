// NereusSDR for iOS: Setup's Band Stack page: its place in the tree, its choices, and the question after a region edit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// The page's choices are the values the Core takes, and the question after
/// a region edit follows spec V1: this device's own edit, within the window,
/// on a Core with band stacks.
@Suite("Band stack setup", .serialized, .timeLimit(.minutes(3)))
@MainActor
struct BandStackSetupTests {
    /// A clock the test moves.
    @MainActor
    private final class TestClock {
        var now = ContinuousClock.now
    }

    @MainActor
    private struct Rig {
        let app: AppModel
        let station: FakeStation
        let suite: String
        let clock: TestClock
        let prompt: BandStackRegionPrompt
        var stack: BandStackModel { app.main.controls(for: .first).bandStack }

        /// The Core's document moves to `region`, however it came to.
        func moveRegion(to region: String) async {
            await station.deliverBandStack(BandStackSetupTests.document(region: region))
            #expect(await Eventually.holds { stack.stack?.region == region })
        }

        /// What was sent, by verb.
        var verbs: [String] { station.bandStackCommands.map(\.verb) }

        func close() async {
            await app.disconnect()
            UserDefaults.standard.removePersistentDomain(forName: suite)
        }
    }

    private func connected(additions: FakeStation.Additions = .bandStack) async throws -> Rig {
        let suite = "BandStackSetupTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let app = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: additions)
        let clock = TestClock()
        let stack = app.main.controls(for: .first).bandStack
        let prompt = BandStackRegionPrompt(model: stack, now: { clock.now })
        await app.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                          transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        return Rig(app: app, station: station, suite: suite, clock: clock, prompt: prompt)
    }

    private func ready() async throws -> Rig {
        let rig = try await connected()
        await rig.moveRegion(to: "United States")
        #expect(await Eventually.holds { rig.stack.availability == .ready })
        return rig
    }

    fileprivate static func document(region: String) -> String {
        """
        {"schema":1,"region":"\(region)","options":{"dupeProtection":true,"expiryMinutes":15,"dwellSeconds":30},\
        "bands":{},"entries":[],"places":{},"lastExpiry":{"batchId":"","count":0},\
        "lastRecall":{"seq":0,"sliceId":-1,"applyView":false,"centerHz":0,"spanHz":0}}
        """
    }

    /// Long enough for a question, had one been raised, to have shown.
    private func settled() async {
        for _ in 0..<20 {
            await Task.yield()
        }
    }

    // MARK: The page

    @Test("General lists Band Stack after Options, marked Core, and it opens through General")
    func treePlace() throws {
        let alone = try #require(SetupTree.categories.first { $0.title == "General" })
        #expect(alone.pages.map(\.title) == ["Navigation", "Battery and sessions", "Band Stack"])
        let page = try #require(alone.pages.last)
        #expect(page.destination == .native(.bandStack))
        #expect(page.tag == .core)
        #expect(page.unavailableReason == nil)
        #expect(SetupTree.route(to: .bandStack) == [.category("General"), .page(.bandStack)])

        let router = SetupRouter()
        router.open(.bandStack)
        #expect(router.path == [.category("General"), .page(.bandStack)])
    }

    @Test("A link elsewhere opens the page on the Setup tab")
    func navigation() {
        let router = SetupRouter()
        var selected = 0
        let navigation = RootView.navigation(setupRouter: router, toolsRouter: ToolsRouter(),
                                             selectSetup: { selected += 1 }, selectTools: {})
        navigation.openPage(.bandStack)
        #expect(router.path == SetupTree.route(to: .bandStack))
        #expect(selected == 1)
    }

    @Test("The choices are the values the Core takes (design 6.2)")
    func choices() {
        #expect(BandStackSetupPage.autoSave.map(\.value) == [0, 10, 30, 60])
        #expect(BandStackSetupPage.autoExpiry.map(\.value) == [0, 5, 15, 30, 60])
        #expect(BandStackSetupPage.resetDetail(region: "Japan") == "Japan. Locked entries you made stay.")
        #expect(BandStackSetupPage.resetDetail(region: nil) == "Locked entries you made stay.")
    }

    // MARK: The region question

    @Test("A change with no edit here is another device's, and asks nothing")
    func anotherDeviceAsksNothing() async throws {
        let rig = try await ready()
        await rig.moveRegion(to: "Europe")
        await settled()
        #expect(rig.prompt.asked == nil)
        await rig.close()
    }

    @Test("This device's edit asks with the desktop's words, once")
    func ownEditAsks() async throws {
        let rig = try await ready()
        #expect(rig.prompt.noteEdit() != nil)
        await rig.moveRegion(to: "Japan")
        #expect(await Eventually.holds { rig.prompt.asked?.region == "Japan" })
        #expect(BandStackRegionPrompt.text(region: "Japan")
            == "You changed the region to Japan. Replace the unlocked entries with the defaults for Japan? Your locked entries stay.")
        rig.prompt.keep()
        #expect(rig.prompt.asked == nil)
        // The mark went with that change.
        await rig.moveRegion(to: "Europe")
        await settled()
        #expect(rig.prompt.asked == nil)
        await rig.close()
    }

    @Test("A document that keeps its region leaves the edit waiting")
    func sameRegionWaits() async throws {
        let rig = try await ready()
        #expect(rig.prompt.noteEdit() != nil)
        await rig.station.deliverBandStack(Self.document(region: "United States") + " ")
        await settled()
        #expect(rig.prompt.asked == nil)
        await rig.moveRegion(to: "Europe")
        #expect(await Eventually.holds { rig.prompt.asked?.region == "Europe" })
        await rig.close()
    }

    /// V1 as written: a device knows only that it marked an edit, so another
    /// device's change landing in the window asks here too.
    @Test("Inside 65 seconds it asks, and past them it does not")
    func window() async throws {
        let rig = try await ready()
        #expect(BandStackRegionPrompt.editWindow == .seconds(65))
        #expect(rig.prompt.noteEdit() != nil)
        rig.clock.now = rig.clock.now.advanced(by: .seconds(64))
        await rig.moveRegion(to: "Europe")
        #expect(await Eventually.holds { rig.prompt.asked?.region == "Europe" })
        rig.prompt.keep()

        #expect(rig.prompt.noteEdit() != nil)
        rig.clock.now = rig.clock.now.advanced(by: .seconds(65))
        await rig.moveRegion(to: "Japan")
        await settled()
        #expect(rig.prompt.asked == nil)
        await rig.close()
    }

    @Test("A refused edit, or one never sent, asks nothing; a held one still does")
    func refusedEdit() async throws {
        let rig = try await ready()
        var mark = try #require(rig.prompt.noteEdit())
        rig.prompt.editEnded(mark, outcome: .refused("No"))
        await rig.moveRegion(to: "Europe")
        await settled()
        #expect(rig.prompt.asked == nil)

        mark = try #require(rig.prompt.noteEdit())
        rig.prompt.editEnded(mark, outcome: .notSent("Not connected"))
        await rig.moveRegion(to: "Japan")
        await settled()
        #expect(rig.prompt.asked == nil)

        // The Core holds it for its question; the region comes when that is answered.
        mark = try #require(rig.prompt.noteEdit())
        rig.prompt.editEnded(mark, outcome: .awaitingConfirmation)
        await rig.moveRegion(to: "Italy")
        #expect(await Eventually.holds { rig.prompt.asked?.region == "Italy" })
        rig.prompt.keep()

        // An older edit's refusal does not take the newer edit's mark.
        let older = try #require(rig.prompt.noteEdit())
        _ = try #require(rig.prompt.noteEdit())
        rig.prompt.editEnded(older, outcome: .refused("No"))
        await rig.moveRegion(to: "Spain")
        #expect(await Eventually.holds { rig.prompt.asked?.region == "Spain" })
        await rig.close()
    }

    @Test("Keep sends nothing, and Replace sends reseed and closes")
    func answers() async throws {
        let rig = try await ready()
        #expect(rig.prompt.noteEdit() != nil)
        await rig.moveRegion(to: "Europe")
        #expect(await Eventually.holds { rig.prompt.asked != nil })
        rig.prompt.keep()
        await settled()
        #expect(rig.verbs.isEmpty)

        #expect(rig.prompt.noteEdit() != nil)
        await rig.moveRegion(to: "Japan")
        #expect(await Eventually.holds { rig.prompt.asked != nil })
        rig.prompt.replace()
        #expect(rig.prompt.replacing)
        #expect(await Eventually.holds { rig.prompt.asked == nil })
        #expect(rig.verbs == ["bandStack.reseed"])
        #expect(rig.station.bandStackCommands.first?.args.isEmpty == true)
        await rig.close()
    }

    @Test("A refused Replace keeps the question, with the Core's words")
    func refusedReplace() async throws {
        let rig = try await ready()
        #expect(rig.prompt.noteEdit() != nil)
        await rig.moveRegion(to: "Europe")
        #expect(await Eventually.holds { rig.prompt.asked != nil })
        rig.station.refuseNext("bandStack.reseed", reason: "Not now")
        rig.prompt.replace()
        #expect(await Eventually.holds { rig.prompt.refusal == "Not now" })
        #expect(rig.prompt.asked?.region == "Europe")
        #expect(!rig.prompt.replacing)
        rig.prompt.keep()
        #expect(rig.prompt.asked == nil)
        #expect(rig.prompt.refusal == nil)
        await rig.close()
    }

    @Test("Another region arriving closes a question that names the old one")
    func staleQuestionCloses() async throws {
        let rig = try await ready()
        #expect(rig.prompt.noteEdit() != nil)
        await rig.moveRegion(to: "Europe")
        #expect(await Eventually.holds { rig.prompt.asked != nil })
        await rig.moveRegion(to: "Japan")
        #expect(await Eventually.holds { rig.prompt.asked == nil })
        await rig.close()
    }

    @Test("A Core without band stacks takes no mark")
    func olderCore() async throws {
        let rig = try await connected(additions: [])
        #expect(await Eventually.holds { rig.stack.availability == .needsNewerCore })
        #expect(rig.prompt.noteEdit() == nil)
        await rig.close()
    }

    @Test("The link dropping ends the question and the edit, and nothing is sent")
    func linkDrop() async throws {
        let rig = try await ready()
        #expect(rig.prompt.noteEdit() != nil)
        await rig.moveRegion(to: "Europe")
        #expect(await Eventually.holds { rig.prompt.asked != nil })
        let station = rig.station
        await rig.close()
        #expect(await Eventually.holds { rig.prompt.asked == nil })
        #expect(station.bandStackCommands.isEmpty)
    }
}
