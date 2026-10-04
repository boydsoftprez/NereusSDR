// NereusSDR for iOS: changing the TX EQ curve through txEq.setCurve and txEq.resetCurve, protocol layer only
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-34, D92 (link document section 7.1, "Changing the curve";
/// `txEqCurveVersion` 2): the phone sends the whole curve as `curveJson`,
/// resets with no arguments, and draws the `curve` the Core returns, never
/// the one it sent. A refusal is shown in the Core's own words. A Core at
/// version 1 keeps the curve read-only.
@Suite struct TxEqCurveEditTests {
    private let sent = SentMessages()
    private let clock = ManualLinkClock()

    /// The `curve` the Core returned for the conformance session's taken curve.
    static let taken = """
    {"maxHz":3000,"minHz":50,"parametric":true,"points":[{"frequencyHz":50,"gainDb":-6,"q":1.5},\
    {"frequencyHz":300,"gainDb":3,"q":2},{"frequencyHz":1200,"gainDb":-1.5,"q":4},\
    {"frequencyHz":2400,"gainDb":4,"q":3},{"frequencyHz":3000,"gainDb":0,"q":1}],"preampDb":-2.5,"state":"saved"}
    """

    @Test("the verbs, argument and result names are the link document's")
    func names() {
        #expect(TxEqCurve.setCurveVerb == "txEq.setCurve")
        #expect(TxEqCurve.resetCurveVerb == "txEq.resetCurve")
        #expect(TxEqCurve.curveArgumentName == "curveJson")
        #expect(TxEqCurve.returnedCurveName == "curve")
        #expect(TxEqCurve.editableVersion == 2)
    }

    @Test("only a Core at txEqCurveVersion 2 or later takes a change; an older one says why in the phone's words")
    func versionGate() {
        #expect(!TxEqCurve.canChange(capabilityVersion: nil))
        #expect(!TxEqCurve.canChange(capabilityVersion: 0))
        #expect(!TxEqCurve.canChange(capabilityVersion: 1))
        #expect(TxEqCurve.canChange(capabilityVersion: 2))
        #expect(TxEqCurve.canChange(capabilityVersion: 3))
        #expect(TxEqCurve.readOnlyReason
                == "This Core cannot change the TX EQ curve from here. Updating the Core may help.")
    }

    @Test("curveJson carries the curve's shape, points in the order held, and no state")
    func curveJson() throws {
        let curve = try #require(TxEqCurve(json: Self.taken))
        #expect(curve.curveJson == """
        {"maxHz":3000,"minHz":50,"parametric":true,"points":[{"frequencyHz":50,"gainDb":-6,"q":1.5},\
        {"frequencyHz":300,"gainDb":3,"q":2},{"frequencyHz":1200,"gainDb":-1.5,"q":4},\
        {"frequencyHz":2400,"gainDb":4,"q":3},{"frequencyHz":3000,"gainDb":0,"q":1}],"preampDb":-2.5}
        """)
        var unsorted = curve
        unsorted.points.swapAt(0, 3)
        unsorted.parametric = false
        let reread = try #require(TxEqCurve(json: unsorted.curveJson.replacingOccurrences(
            of: "\"preampDb\":-2.5}", with: "\"preampDb\":-2.5,\"state\":\"saved\"}")))
        #expect(reread.points == unsorted.points)
        #expect(!reread.parametric)
    }

    @Test("an accepted answer gives the Core's curve; a refusal gives its words")
    func outcomes() {
        let accepted = CommandResult(accepted: true, reason: "", affectedKeys: [],
                                     values: ["curve": .text(Self.taken)], phase: nil)
        #expect(TxEqCurve.outcome(accepted) == .taken(TxEqCurve(json: Self.taken)))
        let unreadable = CommandResult(accepted: true, reason: "", affectedKeys: [],
                                       values: ["curve": .text("not json")], phase: nil)
        #expect(TxEqCurve.outcome(unreadable) == .taken(.unavailable))
        let bare = CommandResult(accepted: true, reason: "", affectedKeys: [], values: [:], phase: nil)
        #expect(TxEqCurve.outcome(bare) == .taken(nil))
        let refused = CommandResult(accepted: false, reason: "Choose a curve of 5, 10 or 18 points.",
                                    affectedKeys: [], values: [:], phase: nil)
        #expect(TxEqCurve.outcome(refused) == .refused("Choose a curve of 5, 10 or 18 points."))
    }

    private func client() async -> CommandClient {
        let client = CommandClient(clock: clock, send: sent.sender)
        await client.handle(.stateChanged(.ready))
        return client
    }

    private func lastInvoke(after count: Int) async throws -> LinkMessage.CommandInvoke {
        #expect(await sent.settle(untilCount: count + 1))
        guard case .commandInvoke(let invoke)? = sent.messages.last else {
            Issue.record("no command.invoke was sent")
            throw CancellationError()
        }
        return invoke
    }

    @Test("a change sends the whole curve once and reads back the curve the Core holds")
    func setThroughTheCommandClient() async throws {
        let commands = await client()
        let curve = try #require(TxEqCurve(json: Self.taken))
        let before = sent.count
        let task = Task { try await TxEqCurve.set(curve, through: commands) }
        let invoke = try await lastInvoke(after: before)
        #expect(invoke.verb == "txEq.setCurve")
        #expect(invoke.args == [LinkMessage.PropertyEntry(name: "curveJson", value: .utf8(curve.curveJson))])
        await commands.handle(.message(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: true, reason: "", affected: [],
            values: [LinkMessage.PropertyEntry(name: "curve", value: .utf8(Self.taken))]))))
        #expect(try await task.value == .taken(curve))
    }

    @Test("a reset sends no arguments and a refusal comes back in the Core's words")
    func resetThroughTheCommandClient() async throws {
        let commands = await client()
        let before = sent.count
        let task = Task { try await TxEqCurve.reset(through: commands) }
        let invoke = try await lastInvoke(after: before)
        #expect(invoke.verb == "txEq.resetCurve")
        #expect(invoke.args.isEmpty)
        await commands.handle(.message(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: false,
            reason: "Update this app to change the TX EQ curve on this Core.", affected: []))))
        #expect(try await task.value == .refused("Update this app to change the TX EQ curve on this Core."))
    }
}
