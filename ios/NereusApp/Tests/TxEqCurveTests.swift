// NereusSDR for iOS: the TX Equalizer page's curve against a fake Core: what each change sends, refusals, other devices, gates
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// Plan Task 59a (R-IOS-18): the TX EQ curve on the TX Equalizer page.
/// Every change sends the whole curve the Core last sent with only the
/// changed value different, and the page then shows the curve the Core
/// answers with; a refusal shows the Core's words and changes nothing; a
/// change from another device redraws the page and closes a curve number
/// pad without sending it; Bands resets flat as the desktop does; Reset is
/// the Core's `txEq.resetCurve`; the gates follow `txEqCurveVersion`,
/// `transmitSettingsVersion` 13 on the air and the holder of transmit. The
/// curves and the Core's words are the conformance suite's own
/// (sessions/tx-eq-set-curve.json).
@Suite("TX EQ curve", .serialized)
@MainActor
struct TxEqCurveTests {
    private let platform = TestPlatform()

    @Test("a submitted curve pad restores its Core field at timeout; newer typing and replacement pads remain drafts", arguments: [0, 1, 2])
    func timeoutRestoresOnlySubmittedCurveEntry(owner: Int) async throws {
        let clock = TestLinkClock()
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: clock)
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        eq.openGainPad()
        let submitted = try #require(eq.pad)
        let before = shown.points[eq.selected].gainDb
        TxEqCurveHarness.type(submitted, "2.5")
        let entered = Task { await submitted.enter() }
        let sent = try #require(await harness.nextRecordedCurveInvoke())
        if owner == 1 {
            TxEqCurveHarness.type(submitted, "3.5")
            if submitted.negative { submitted.press(.minus) }
        }
        if owner == 2 {
            eq.openCurvePreampPad()
            let replacement = try #require(eq.pad)
            TxEqCurveHarness.type(replacement, "3.5")
            if replacement.negative { replacement.press(.minus) }
        }
        let current = try #require(eq.pad)
        let entry = current.entry
        await clock.advance(by: 5_000)
        #expect(await entered.value == false)
        #expect(eq.curve == shown && eq.pad === current)
        #expect(current.number == (owner == 0 ? before : 3.5))
        #expect(current.refusal == (owner == 0 ? PropertyWriteOutcome.notConfirmed.reason : nil))
        #expect(eq.curveNote == (owner == 0 ? PropertyWriteOutcome.notConfirmed.reason : nil))
        if owner != 0 { #expect(current.entry == entry) }
        await harness.replyRecorded(sent, accepted: false, reason: "Current curve refusal.")
        await drain()
        #expect(current.number == (owner == 0 ? before : 3.5))
        #expect(current.refusal == (owner == 0 ? "Current curve refusal." : nil))
        #expect(eq.curveNote == (owner == 0 ? "Current curve refusal." : nil))
        #expect(eq.pad === current && eq.curve == shown && harness.recordedCurveInvokes.count == 1)
        await harness.app.disconnect()
    }

    @Test("an expired curve set or reset follows the latest Core curve and rejects an old returned curve", arguments: [false, true])
    func timeoutPreservesLatestCoreCurve(reset: Bool) async throws {
        let clock = TestLinkClock()
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: clock)
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        if reset { eq.reset() } else { eq.stepGain(up: true) }
        let sent = try #require(await harness.nextRecordedCurveInvoke())
        var remote = shown
        remote.preampDb += 1
        try await harness.deliverCurve(remote.savedJson)
        #expect(await turns { eq.curve == remote })
        await clock.advance(by: 4_999)
        #expect(eq.curve == remote && eq.curveNote == nil)
        await clock.advance(by: 1)
        #expect(await turns { eq.curveNote == PropertyWriteOutcome.notConfirmed.reason })
        #expect(eq.curve == remote && harness.recordedCurveInvokes.count == 1)
        await harness.replyRecorded(sent, accepted: true, curve: shown)
        await drain()
        #expect(eq.curve == remote && harness.recordedCurveInvokes.count == 1)
        await harness.app.disconnect()
    }

    @Test("curve set and reset show unresolved or lost-link outcomes at their current control",
          arguments: [false, true], [false, true])
    func curveUnresolvedOwner(reset: Bool, loseLink: Bool) async throws {
        let clock = TestLinkClock()
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: clock)
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        if reset { eq.reset() } else { eq.stepGain(up: true) }
        let sent = try #require(await harness.nextRecordedCurveInvoke())
        let commands = try #require(harness.injectedCommands)
        #expect(sent.verb == (reset ? TxEqCurve.resetCurveVerb : TxEqCurve.setCurveVerb))
        #expect(await turns { clock.pendingDueTimes.contains(clock.now + 5_000) })
        if loseLink {
            await commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        } else {
            await clock.advance(by: 4_999)
            #expect(eq.curveNote == nil)
            await clock.advance(by: 1)
        }
        #expect(!clock.pendingDueTimes.contains(5_000))
        await drain()
        let expected = loseLink ? PropertyWriteOutcome.linkLost.reason : PropertyWriteOutcome.notConfirmed.reason
        #expect(eq.curveNote == expected)
        #expect(eq.curve == shown)
        #expect(harness.recordedCurveInvokes.count == 1)
        let refusal = "Choose a curve of 5, 10 or 18 points."
        await harness.replyRecorded(sent, accepted: false, reason: refusal)
        await drain()
        #expect(eq.curveNote == (loseLink ? expected : refusal))
        #expect(eq.curve == shown && harness.recordedCurveInvokes.count == 1)
        await harness.app.disconnect()
    }

    @Test("a late old curve refusal cannot replace a newer curve operation's note")
    func lateCurveResultOwnsNewestTouch() async throws {
        let clock = TestLinkClock()
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: clock)
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        eq.stepGain(up: true)
        let first = try #require(await harness.nextRecordedCurveInvoke())
        await clock.advance(by: 5_000)
        await drain()
        #expect(eq.curveNote == PropertyWriteOutcome.notConfirmed.reason)
        eq.stepGain(up: false)
        let second = try #require(await harness.nextRecordedCurveInvoke(after: first.id))
        await harness.replyRecorded(first, accepted: false, reason: "Old curve refusal.")
        await drain()
        #expect(eq.curveNote != "Old curve refusal.")
        await harness.replyRecorded(second, accepted: false, reason: "Current curve refusal.")
        #expect(await turns { eq.curveNote == "Current curve refusal." })
        #expect(eq.curve == shown && harness.recordedCurveInvokes.count == 2)
        await harness.app.disconnect()
    }

    @Test("a curve pad's retired or newly typed owner cannot receive its prior refusal", arguments: [false, true])
    func curvePadRetiresResult(replace: Bool) async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: TestLinkClock())
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        eq.openGainPad()
        let old = try #require(eq.pad)
        TxEqCurveHarness.type(old, "2.5")
        let entered = Task { await old.enter() }
        let sent = try #require(await harness.nextRecordedCurveInvoke())
        if replace { eq.openCurvePreampPad() } else { old.press(.delete) }
        let current = try #require(eq.pad)
        let entry = current.entry
        await harness.replyRecorded(sent, accepted: false, reason: "Retired curve refusal.")
        #expect(await entered.value == false)
        await drain()
        #expect(eq.pad === current && current.entry == entry && current.refusal == nil)
        #expect(eq.curveNote == nil)
        #expect(eq.curve == shown && harness.recordedCurveInvokes.count == 1)
        await harness.app.disconnect()
    }

    @Test("a returned old curve cannot overwrite a newer Core curve or a replacement pad")
    func returnedCurveRetiresWithCoreIdentity() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: TestLinkClock())
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        eq.stepGain(up: true)
        let sent = try #require(await harness.nextRecordedCurveInvoke())
        var remote = shown
        remote.preampDb += 1
        try await harness.deliverCurve(remote.savedJson)
        #expect(await turns { eq.curve == remote })
        eq.openCurvePreampPad()
        let current = try #require(eq.pad)
        await harness.replyRecorded(sent, accepted: true, curve: shown)
        await drain()
        #expect(eq.curve == remote && eq.pad === current && eq.curveNote == nil)
        #expect(harness.recordedCurveInvokes.count == 1)
        await harness.app.disconnect()
    }

    @Test("a current curve pad shows timeout or loss and an eventual exact refusal", arguments: [false, true])
    func curvePadOwnsUnresolvedAndLate(loseLink: Bool) async throws {
        let clock = TestLinkClock()
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: clock)
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        eq.openGainPad()
        let pad = try #require(eq.pad)
        TxEqCurveHarness.type(pad, "2.5")
        let entered = Task { await pad.enter() }
        let sent = try #require(await harness.nextRecordedCurveInvoke())
        if loseLink {
            await harness.injectedCommands?.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        } else { await clock.advance(by: 5_000) }
        #expect(await entered.value == false)
        let hint = loseLink ? PropertyWriteOutcome.linkLost.reason : PropertyWriteOutcome.notConfirmed.reason
        #expect(pad.refusal == hint && eq.curveNote == hint && eq.pad === pad)
        await harness.replyRecorded(sent, accepted: false, reason: "Current curve refusal.")
        await drain()
        let expected = loseLink ? hint : "Current curve refusal."
        #expect(pad.refusal == expected && eq.curveNote == expected && eq.pad === pad)
        #expect(eq.curve == shown && harness.recordedCurveInvokes.count == 1)
        await harness.app.disconnect()
    }

    @Test("a late current accepted curve keeps the Core's returned curve and retires its pad")
    func currentLateCurveKeepsCoreAnswer() async throws {
        let clock = TestLinkClock()
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: clock)
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        eq.openGainPad()
        let pad = try #require(eq.pad)
        TxEqCurveHarness.type(pad, "2.5")
        let entered = Task { await pad.enter() }
        let sent = try #require(await harness.nextRecordedCurveInvoke())
        await clock.advance(by: 5_000)
        #expect(await entered.value == false)
        var kept = shown
        kept.points[0].gainDb = 2.4
        await harness.replyRecorded(sent, accepted: true, curve: kept)
        #expect(await turns { eq.curve == kept && eq.pad == nil && eq.curveNote == nil })
        #expect(harness.recordedCurveInvokes.count == 1)
        await harness.app.disconnect()
    }

    @Test("a held curve reply preserves the established refusal; a retired session cannot publish its result")
    func curveHeldAndSessionOwnership() async throws {
        let clock = TestLinkClock()
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: clock)
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        eq.stepGain(up: true)
        let first = try #require(await harness.nextRecordedCurveInvoke())
        await harness.replyRecorded(first, accepted: false, reason: "Established curve refusal.")
        #expect(await turns { eq.curveNote == "Established curve refusal." })
        eq.stepGain(up: true)
        let held = try #require(await harness.nextRecordedCurveInvoke(after: first.id))
        await harness.replyRecorded(held, accepted: false, reason: SeveralDevices.waitingReason)
        await drain()
        #expect(eq.curveNote == "Established curve refusal." && eq.curve == shown)
        eq.stepGain(up: true)
        let retired = try #require(await harness.nextRecordedCurveInvoke(after: held.id))
        await clock.advance(by: 5_000)
        await drain()
        #expect(eq.curveNote == PropertyWriteOutcome.notConfirmed.reason)
        // Replace the app's snapshot before the retired command's callback is admitted.
        harness.app.mirror.handle(.stateChanged(.receivingSnapshot))
        harness.app.mirror.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
        await harness.injectedCommands?.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await harness.injectedCommands?.handle(.stateChanged(.ready))
        await harness.replyRecorded(retired, accepted: true, curve: shown)
        await drain()
        #expect(eq.curveNote == PropertyWriteOutcome.notConfirmed.reason && eq.curve == shown)
        #expect(harness.recordedCurveInvokes.count == 3)
        await harness.app.disconnect()
    }

    @Test("an accepted curve answer without a returned curve uses the Core delta and completes its pad")
    func deltaOnlyCurveAnswerCompletes() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform, commandClock: TestLinkClock())
        let eq = harness.eq
        let shown = try #require(TxEqCurve(json: TxEqCurveHarness.fixture().setCurveAnswer))
        try await harness.deliverCurve(shown.savedJson)
        #expect(await turns { eq.curve == shown && eq.curveReason == nil })
        eq.openGainPad()
        let pad = try #require(eq.pad)
        TxEqCurveHarness.type(pad, "2.5")
        let entered = Task { await pad.enter() }
        let sent = try #require(await harness.nextRecordedCurveInvoke())
        var kept = shown
        kept.points[0].gainDb = 2.4
        try await harness.deliverCurve(kept.savedJson)
        #expect(await turns { eq.curve == kept })
        await harness.replyRecorded(sent, accepted: true)
        #expect(await entered.value)
        #expect(eq.curve == kept && eq.pad == nil && eq.curveNote == nil)
        #expect(harness.recordedCurveInvokes.count == 1)
        await harness.app.disconnect()
    }

    private func turns(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() { return true }
            await Task.yield()
        }
        return condition()
    }

    private func drain() async {
        for _ in 0..<2_000 { await Task.yield() }
    }

    @Test("a change sends the Core's curve with only that value different, and the page draws the Core's answer")
    func changesSendTheCoreCurve() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform)
        let eq = harness.eq
        let worked = try TxEqCurveHarness.fixture().setCurveAnswer
        try await harness.deliverCurve(worked)
        let shown = try #require(TxEqCurve(json: worked))
        #expect(await harness.settle { eq.curve == shown && eq.curveReason == nil })
        #expect(eq.selected == 0 && eq.chosenCount == 5 && eq.onAir != nil)

        // Point 3's gain, 0.5 dB up: the whole curve, only that gain changed.
        eq.choose(2)
        eq.stepGain(up: true)
        var expected = shown
        expected.points[2].gainDb = -1
        let sent = try #require(await harness.nextSetCurve())
        #expect(sent.json == expected.curveJson)
        // The Core answers with its own rounding; the page shows that, not what was asked.
        var kept = expected
        kept.points[2].gainDb = -1.1
        await harness.answer(sent.id, curve: kept)
        // The delta comes before the answer; the answer clears the notices.
        #expect(await harness.settle {
            eq.curve?.points[2].gainDb == -1.1 && !eq.changedElsewhere && eq.curveNote == nil
        })
        #expect(!eq.changedElsewhere && eq.curveNote == nil)

        // The next change starts from the Core's answer: frequency 10 Hz up in whole hertz.
        eq.stepFrequency(up: true)
        var next = kept
        next.points[2].frequencyHz = 1210
        let second = try #require(await harness.nextSetCurve())
        #expect(second.json == next.curveJson)
        await harness.answer(second.id, curve: next)
        #expect(await harness.settle { eq.curve?.points[2].frequencyHz == 1210 })

        // Q, the curve preamp and Shape.
        eq.stepQ(up: false)
        next.points[2].q = 3.9
        let third = try #require(await harness.nextSetCurve())
        #expect(third.json == next.curveJson)
        await harness.answer(third.id, curve: next)
        #expect(await harness.settle { eq.curve?.points[2].q == 3.9 })
        eq.stepCurvePreamp(up: true)
        next.preampDb = -2
        let fourth = try #require(await harness.nextSetCurve())
        #expect(fourth.json == next.curveJson)
        await harness.answer(fourth.id, curve: next)
        #expect(await harness.settle { eq.curve?.preampDb == -2 })
        eq.setShape(bells: false)
        next.parametric = false
        let fifth = try #require(await harness.nextSetCurve())
        #expect(fifth.json == next.curveJson)
        await harness.answer(fifth.id, curve: next)
        #expect(await harness.settle { eq.curve?.parametric == false })
        #expect(eq.qReason == TxEqualizerModel.straightLinesQReason)

        // The ends follow Low and High: their frequency is not stepped.
        eq.choose(0)
        #expect(eq.frequencyReason == TxEqualizerModel.endReason(point: 1, low: true))
        eq.chooseNext()
        eq.chooseNext()
        eq.chooseNext()
        eq.chooseNext()
        eq.chooseNext()
        #expect(eq.selected == 4)
        #expect(eq.frequencyReason == "Point 5 sits at the high end of the range. Change High under Curve values to move it.")

        // The gain pad takes a decimal and sends it on the Core's curve.
        eq.choose(3)
        eq.openGainPad()
        let pad = try #require(eq.pad)
        #expect(pad.takesDecimals && pad.title == "Point 4 gain")
        #expect(pad.rangeText == "From \u{2212}24 to 24 dB, in steps of 0.1 dB. Now +4.0 dB.")
        TxEqCurveHarness.type(pad, "2.5")
        async let entered = pad.enter()
        next.points[3].gainDb = 2.5
        let typed = try #require(await harness.nextSetCurve())
        #expect(typed.json == next.curveJson)
        await harness.answer(typed.id, curve: next)
        #expect(await entered)
        #expect(eq.pad == nil)
        await harness.app.disconnect()
    }

    @Test("a refused change shows the Core's words and keeps its curve; Bands resets flat; Reset is the Core's")
    func refusalsBandsAndReset() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform)
        let eq = harness.eq
        let fixture = try TxEqCurveHarness.fixture()
        // An old profile's curve of 7 points: none of the three counts is chosen.
        try await harness.deliverCurve(TxEqCurveHarness.sevenPoints)
        let seven = try #require(TxEqCurve(json: TxEqCurveHarness.sevenPoints))
        #expect(await harness.settle { eq.curve == seven })
        #expect(eq.chosenCount == nil)
        eq.choose(3)
        eq.stepGain(up: true)
        let refused = try #require(await harness.nextSetCurve())
        await harness.refuse(refused.id, fixture.countRefusal)
        #expect(await harness.settle { eq.curveNote == fixture.countRefusal })
        #expect(fixture.countRefusal == "Choose a curve of 5, 10 or 18 points.")
        #expect(eq.curve == seven && eq.curveReason == nil)

        // Bands: five flat points from Low to High, the preamp and Shape kept.
        eq.setBands(5)
        var flat = seven
        flat.points = TxEqualizerModel.flatPoints(5, from: 100, to: 3100)
        #expect(flat.points.map(\.frequencyHz) == [100, 850, 1600, 2350, 3100])
        #expect(flat.points.allSatisfy { $0.gainDb == 0 && $0.q == 4 })
        let bands = try #require(await harness.nextSetCurve())
        #expect(bands.json == flat.curveJson)
        flat.state = .saved
        await harness.answer(bands.id, curve: flat)
        // The delta comes before the answer; the answer clears the refusal.
        #expect(await harness.settle { eq.curve?.points.count == 5 && eq.chosenCount == 5 && eq.curveNote == nil })
        #expect(eq.curveNote == nil)

        // Reset: txEq.resetCurve with no arguments; the page shows the Core's flat curve.
        eq.reset()
        let reset = try #require(await harness.nextInvoke(TxEqCurve.resetCurveVerb))
        #expect(reset.args.isEmpty)
        let resetCurve = try #require(TxEqCurve(json: fixture.resetAnswer))
        await harness.answer(reset.id, curve: resetCurve, verb: TxEqCurve.resetCurveVerb)
        #expect(await harness.settle { eq.curve == resetCurve && !eq.changedElsewhere })
        #expect(resetCurve.preampDb == 0 && resetCurve.points.count == 5)
        #expect(!eq.changedElsewhere)

        // Save: txProfile.save with the active profile's name.
        await harness.station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: try TxEqCurveHarness.ordinal("activeTxProfile"), name: "activeTxProfile",
                  value: .utf8("Default")),
        ])))
        #expect(await harness.settle { eq.activeProfile == "Default" && eq.saveReason == nil })
        eq.saveProfile()
        let save = try #require(await harness.nextInvoke(TxEqualizerModel.saveProfileVerb))
        #expect(save.args.first?.name == "name" && save.args.first?.value == .utf8("Default"))
        await harness.station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: TxEqualizerModel.saveProfileVerb, id: save.id, accepted: true, reason: "", affected: [])))
        // A profile command the Core refuses shows its words, and says it
        // was refused when the Core gave none.
        eq.saveProfile()
        let wordless = try #require(await harness.nextInvoke(TxEqualizerModel.saveProfileVerb))
        await harness.station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: TxEqualizerModel.saveProfileVerb, id: wordless.id, accepted: false, reason: "", affected: [])))
        #expect(await harness.settle { eq.note == BandSlicesModel.refusedText })
        eq.saveProfile()
        let worded = try #require(await harness.nextInvoke(TxEqualizerModel.saveProfileVerb))
        await harness.station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: TxEqualizerModel.saveProfileVerb, id: worded.id, accepted: false,
            reason: "The profile is in use elsewhere.", affected: [])))
        #expect(await harness.settle { eq.note == "The profile is in use elsewhere." })
        await harness.app.disconnect()
    }

    @Test("Low and High carry every point along, each keeping its place between the ends, then spaced in order")
    func rangeCarriesThePoints() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform)
        let eq = harness.eq
        try await harness.deliverCurve(TxEqCurveHarness.crowded)
        let crowded = try #require(TxEqCurve(json: TxEqCurveHarness.crowded))
        #expect(await harness.settle { eq.curve == crowded && eq.curveReason == nil })
        // Low from 100 to 1100 Hz: each point keeps its share of the span,
        // 100, 102, 104 and 106 of 100 to 3100 Hz become 1100, 1101.33,
        // 1102.67 and 1104 of 1100 to 3100 Hz, then are spaced 5 Hz apart.
        eq.openRangePad(low: true)
        let low = try #require(eq.pad)
        TxEqCurveHarness.type(low, "1100")
        async let lowEntered = low.enter()
        let sentLow = try #require(await harness.nextSetCurve())
        var lowCurve = crowded
        lowCurve.minHz = 1100
        for (index, hz) in [1100.0, 1105, 1110, 1115, 3100].enumerated() {
            lowCurve.points[index].frequencyHz = hz
        }
        #expect(sentLow.json == lowCurve.curveJson)
        await harness.answer(sentLow.id, curve: lowCurve)
        #expect(await lowEntered)
        // High from 3100 to 5100 Hz: the span doubles, so 1105, 1110 and
        // 1115 become 1110, 1120 and 1130.
        eq.openRangePad(low: false)
        let high = try #require(eq.pad)
        TxEqCurveHarness.type(high, "5100")
        async let highEntered = high.enter()
        let sentHigh = try #require(await harness.nextSetCurve())
        var highCurve = lowCurve
        highCurve.maxHz = 5100
        for (index, hz) in [1100.0, 1110, 1120, 1130, 5100].enumerated() {
            highCurve.points[index].frequencyHz = hz
        }
        #expect(sentHigh.json == highCurve.curveJson)
        await harness.answer(sentHigh.id, curve: highCurve)
        #expect(await highEntered)
        await harness.app.disconnect()
    }

    @Test("a pad open over this phone's own change still enters; a wordless refusal still shows")
    func padFollowsThisPhonesChange() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform)
        let eq = harness.eq
        let worked = try TxEqCurveHarness.fixture().setCurveAnswer
        try await harness.deliverCurve(worked)
        let shown = try #require(TxEqCurve(json: worked))
        #expect(await harness.settle { eq.curve == shown })
        eq.choose(2)
        eq.openGainPad()
        let pad = try #require(eq.pad)
        // A step from this phone lands while the pad is open.
        eq.stepGain(up: true)
        var stepped = shown
        stepped.points[2].gainDb = -1
        let step = try #require(await harness.nextSetCurve())
        #expect(step.json == stepped.curveJson)
        await harness.answer(step.id, curve: stepped)
        #expect(await harness.settle { eq.curve == stepped })
        #expect(eq.pad === pad && !eq.changedElsewhere)
        // Enter sends the typed gain on the curve as it now is. The pad
        // opened on a cut, so the digits keep its minus sign.
        #expect(pad.negative)
        TxEqCurveHarness.type(pad, "2.5")
        async let entered = pad.enter()
        var typed = stepped
        typed.points[2].gainDb = -2.5
        let sent = try #require(await harness.nextSetCurve())
        #expect(sent.json == typed.curveJson)
        await harness.answer(sent.id, curve: typed)
        #expect(await entered)
        // A refusal the Core gave no words for still says so.
        eq.stepGain(up: true)
        let refused = try #require(await harness.nextSetCurve())
        await harness.refuse(refused.id, "")
        #expect(await harness.settle { eq.curveNote == BandSlicesModel.refusedText })
        await harness.app.disconnect()
    }

    @Test("a change from another device redraws the page, says so, and closes the pad without sending it")
    func changedElsewhere() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform)
        let eq = harness.eq
        let worked = try TxEqCurveHarness.fixture().setCurveAnswer
        try await harness.deliverCurve(worked)
        let shown = try #require(TxEqCurve(json: worked))
        #expect(await harness.settle { eq.curve == shown })
        eq.choose(2)
        eq.openGainPad()
        let pad = try #require(eq.pad)
        TxEqCurveHarness.type(pad, "2")
        var other = shown
        other.points[3].gainDb = 6
        other.preampDb = -1
        try await harness.deliverCurve(other.savedJson)
        #expect(await harness.settle { eq.pad == nil })
        #expect(eq.changedElsewhere && eq.curve == other && eq.selected == 2)
        // The typed value is never sent, even if Enter comes late.
        #expect(await pad.enter() == false)
        try? await Task.sleep(for: .milliseconds(200))
        #expect(harness.setCurveCount == 0)

        // Another profile brings its own curve: no notice for that.
        var profile = other
        profile.points[1].gainDb = -3
        await harness.station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: try TxEqCurveHarness.ordinal("activeTxProfile"), name: "activeTxProfile",
                  value: .utf8("Contest")),
            .init(ordinal: try TxEqCurveHarness.ordinal("txEqCurve"), name: "txEqCurve", value: .utf8(profile.savedJson)),
        ])))
        #expect(await harness.settle { eq.curve == profile })
        #expect(!eq.changedElsewhere)
        await harness.app.disconnect()
    }

    @Test("the curve's gates: older Core, a Core that cannot take changes, an unreadable curve, on the air, no Core")
    func gates() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform)
        let eq = harness.eq
        let worked = try TxEqCurveHarness.fixture().setCurveAnswer
        try await harness.deliverCurve(worked)
        #expect(await harness.settle { eq.curve != nil && eq.curveReason == nil && eq.resetReason == nil })

        // A Core that answered txEqCurve 1: drawn, read only, Reset included.
        SetupDescribedPagesTests.withCapabilities(harness.app, ["txEqCurveVersion": .i64(1)])
        #expect(await harness.settle { eq.curveReason == TxEqCurve.readOnlyReason })
        #expect(eq.resetReason == TxEqCurve.readOnlyReason && eq.curve != nil)
        #expect(TxEqCurve.readOnlyReason == "This Core cannot change the TX EQ curve from here. Updating the Core may help.")
        eq.stepGain(up: true)
        eq.reset()

        // A Core that does not send the curve.
        SetupDescribedPagesTests.withCapabilities(harness.app, ["txEqCurveVersion": .i64(0)])
        #expect(await harness.settle { eq.curve == nil && eq.curveReason == TxEqualizerModel.olderCoreReason })

        // An unreadable curve: everything greyed but Reset.
        SetupDescribedPagesTests.withCapabilities(harness.app, ["txEqCurveVersion": .i64(2)])
        try await harness.deliverCurve(#"{"state":"unavailable"}"#)
        #expect(await harness.settle { eq.curve?.state == .unavailable })
        #expect(eq.curveReason == TxEqualizerModel.unavailableReason && eq.resetReason == nil)
        #expect(eq.chosenPoint == nil)

        // On the air: live at transmitSettingsVersion 13 and later, greyed below.
        try await harness.deliverCurve(worked)
        await harness.setTransmitting(true)
        #expect(await harness.settle { eq.transmitting })
        #expect(eq.curveReason == nil && eq.resetReason == nil && eq.takesOnAir)
        SetupDescribedPagesTests.withCapabilities(harness.app, ["transmitSettingsVersion": .i64(12)])
        #expect(await harness.settle { eq.curveReason == TransmitModel.onAirText })
        #expect(eq.resetReason == TransmitModel.onAirText && eq.profileReason == TransmitModel.onAirText)
        eq.choose(3)
        #expect(eq.selected == 3)
        eq.stepGain(up: true)
        await harness.setTransmitting(false)
        #expect(await harness.settle { eq.curveReason == nil })
        try? await Task.sleep(for: .milliseconds(200))
        #expect(harness.setCurveCount == 0 && harness.invokeCount(TxEqCurve.resetCurveVerb) == 0)

        // No Core: the last curve stays, greyed with the reason.
        await harness.app.disconnect()
        #expect(await harness.settle { eq.curveReason == TxEqualizerModel.notConnectedReason })
        #expect(eq.resetReason == TxEqualizerModel.notConnectedReason)
    }

    @Test("the marks name their point, frequency and gain; the drawing's words")
    func words() throws {
        let curve = try #require(TxEqCurve(json: try TxEqCurveHarness.fixture().setCurveAnswer))
        #expect(TxEqCurveParts.markLabel(2, curve.points[2]) == "Point 3, 1200 Hz, \u{2212}1.5 dB")
        #expect(TxEqCurveParts.markLabel(1, curve.points[1]) == "Point 2, 300 Hz, +3.0 dB")
        #expect(TxEqCurveParts.db(0) == "0.0 dB" && TxEqCurveParts.db(-0.04) == "0.0 dB")
        #expect(TxEqCurveParts.q(1.5) == "1.50")
        #expect(TxEqCurveParts.caption(curve) == "Saved curve \u{00B7} 5 points \u{00B7} bells")
        #expect(TxEqCurveParts.tickStep(2950) == 500 && TxEqCurveParts.tickStep(4000) == 1000)
    }
}

/// The app connected to a fake Core that sends the TX EQ curve
/// (`txEqCurveVersion` 2, `transmitSettingsVersion` 15), with the TX
/// Equalizer page's model, and answers to its curve changes.
@MainActor
final class TxEqCurveHarness {
    let app: AppModel
    let station: FakeStation
    let eq: TxEqualizerModel
    private var answered: Set<UInt32> = []
    private let commandOutbox: BandSlicesModelTests.Outbox?
    let injectedCommands: CommandClient?

    var recordedCurveInvokes: [LinkMessage.CommandInvoke] {
        commandOutbox?.messages.compactMap { message in
            guard case .commandInvoke(let invoke) = message else { return nil }
            return invoke
        } ?? []
    }

    func nextRecordedCurveInvoke(after id: UInt32 = 0) async -> LinkMessage.CommandInvoke? {
        for _ in 0..<50_000 {
            if let next = recordedCurveInvokes.first(where: { $0.id > id }) { return next }
            await Task.yield()
        }
        Issue.record("No curve command reached the injected Core recorder.")
        return nil
    }

    func replyRecorded(_ invoke: LinkMessage.CommandInvoke, accepted: Bool, reason: String = "",
                       curve: TxEqCurve? = nil) async {
        await injectedCommands?.handle(.message(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: accepted, reason: reason, affected: [],
            values: curve.map { [.init(ordinal: 0, name: TxEqCurve.returnedCurveName, value: .utf8($0.savedJson))] }))))
    }

    private init(app: AppModel, station: FakeStation, eq: TxEqualizerModel,
                 commandOutbox: BandSlicesModelTests.Outbox?, commands: CommandClient?) {
        self.app = app
        self.station = station
        self.eq = eq
        self.commandOutbox = commandOutbox
        self.injectedCommands = commands
    }

    static func connected(platform: TestPlatform,
                          additions: FakeStation.Additions = [],
                          commandClock: TestLinkClock? = nil) async throws -> TxEqCurveHarness {
        let defaults = try #require(UserDefaults(suiteName: "TxEqCurveTests-\(UUID().uuidString)"))
        let app = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                           displaySettings: BandDisplaySettingsStore(defaults: defaults), platform: platform.platform)
        // The suite's default fake: the transmit surface, txEqCurve among it, with its profiles and bands.
        let station = try FakeStation(additions: additions)
        await app.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                          transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let outbox = commandClock.map { _ in BandSlicesModelTests.Outbox() }
        let commands = commandClock.flatMap { clock in
            outbox.map { box in CommandClient(clock: clock, send: { box.record($0) }) }
        }
        await commands?.handle(.stateChanged(.ready))
        let eq = TxEqualizerModel(mirror: app.mirror, transmit: app.main.transmit,
                                  commands: commands ?? app.commands)
        let harness = TxEqCurveHarness(app: app, station: station, eq: eq,
                                       commandOutbox: outbox, commands: commands)
        #expect(await harness.settle { app.connection == .connected && app.mirror.isSnapshotComplete })
        SetupDescribedPagesTests.withCapabilities(app, ["txEqCurveVersion": .i64(2),
                                                        "transmitSettingsVersion": .i64(15)])
        #expect(await harness.settle { app.main.transmit.settingsVersion == 15 && eq.editorReason == nil })
        return harness
    }

    // MARK: The conformance suite's curves and words

    struct Fixture {
        /// The worked example as the Core answered `txEq.setCurve` with it.
        let setCurveAnswer: String
        /// The refusal of a curve of four points.
        let countRefusal: String
        /// The curve the Core answered `txEq.resetCurve` with.
        let resetAnswer: String
    }

    static func fixture() throws -> Fixture {
        let file = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
            .appendingPathComponent("tests/data/link/v1/sessions/tx-eq-set-curve.json")
        let object = try #require(try JSONSerialization.jsonObject(with: Data(contentsOf: file)) as? [String: Any])
        let results = (object["steps"] as? [[String: Any]] ?? []).compactMap { step -> [String: Any]? in
            guard let message = step["message"] as? [String: Any], message["type"] as? String == "command.result" else {
                return nil
            }
            return message
        }
        func curve(_ id: Int) -> String? {
            let values = results.first { $0["id"] as? Int == id }?["values"] as? [[String: Any]]
            return values?.first { $0["name"] as? String == "curve" }?["value"] as? String
        }
        let refusal = results.first { $0["id"] as? Int == 1402 }?["reason"] as? String
        return Fixture(setCurveAnswer: try #require(curve(1401)), countRefusal: try #require(refusal),
                       resetAnswer: try #require(curve(1403)))
    }

    /// An old profile's curve of seven points (the board's example).
    static let sevenPoints = #"{"state":"saved","parametric":true,"preampDb":0,"minHz":100,"maxHz":3100,"points":["#
        + #"{"frequencyHz":100,"gainDb":-3,"q":2},{"frequencyHz":500,"gainDb":1.5,"q":3},"#
        + #"{"frequencyHz":1000,"gainDb":2,"q":4},{"frequencyHz":1600,"gainDb":-1,"q":4},"#
        + #"{"frequencyHz":2200,"gainDb":3,"q":3},{"frequencyHz":2700,"gainDb":1,"q":3},"#
        + #"{"frequencyHz":3100,"gainDb":-2,"q":2}]}"#

    /// Five points crowded at the low end, as another device might leave them.
    static let crowded = #"{"state":"saved","parametric":true,"preampDb":0,"minHz":100,"maxHz":3100,"points":["#
        + #"{"frequencyHz":100,"gainDb":-3,"q":2},{"frequencyHz":102,"gainDb":1.5,"q":3},"#
        + #"{"frequencyHz":104,"gainDb":2,"q":4},{"frequencyHz":106,"gainDb":-1,"q":4},"#
        + #"{"frequencyHz":3100,"gainDb":-2,"q":2}]}"#

    static func ordinal(_ property: String) throws -> UInt16 {
        try #require(try FakeStation.schema(ofClass: "TransmitModel").fields.first { $0.name == property }?.ordinal)
    }

    // MARK: The Core's side

    func deliverCurve(_ json: String) async throws {
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: try Self.ordinal(TxEqCurve.propertyName), name: TxEqCurve.propertyName, value: .utf8(json)),
        ])))
    }

    func setTransmitting(_ on: Bool) async {
        let schema = try? FakeStation.schema(ofClass: "RadioModel")
        let ordinal = schema?.fields.first { $0.name == "transmitting" }?.ordinal ?? 0
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: ordinal, name: "transmitting", value: .bool(on)),
        ])))
    }

    /// The app's next `verb` that has no answer so far.
    func nextInvoke(_ verb: String) async -> LinkMessage.CommandInvoke? {
        let done = answered
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .commandInvoke(let invoke) = message {
                return invoke.verb == verb && !done.contains(invoke.id)
            }
            return false
        }
        guard case .commandInvoke(let invoke)? = sent else {
            Issue.record("no \(verb) reached the Core")
            return nil
        }
        answered.insert(invoke.id)
        return invoke
    }

    /// The app's next `txEq.setCurve`, with its `curveJson`.
    func nextSetCurve() async -> (id: UInt32, json: String)? {
        guard let invoke = await nextInvoke(TxEqCurve.setCurveVerb),
              invoke.args.count == 1, invoke.args[0].name == TxEqCurve.curveArgumentName,
              case .utf8(let json) = invoke.args[0].value else {
            return nil
        }
        return (invoke.id, json)
    }

    /// Takes a change as the Core does: the transmit delta first, then the answer with the curve it keeps.
    func answer(_ id: UInt32, curve: TxEqCurve, verb: String = TxEqCurve.setCurveVerb) async {
        let json = curve.savedJson
        try? await deliverCurve(json)
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: verb, id: id, accepted: true, reason: "", affected: [],
            values: [.init(ordinal: 0, name: TxEqCurve.returnedCurveName, value: .utf8(json))])))
    }

    func refuse(_ id: UInt32, _ reason: String) async {
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: TxEqCurve.setCurveVerb, id: id, accepted: false, reason: reason, affected: [], values: nil)))
    }

    var setCurveCount: Int {
        invokeCount(TxEqCurve.setCurveVerb)
    }

    func invokeCount(_ verb: String) -> Int {
        station.messages.filter { message in
            if case .commandInvoke(let invoke) = message {
                return invoke.verb == verb
            }
            return false
        }.count
    }

    // MARK: Waiting and typing

    func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }

    /// Types `text` on a number pad from an empty entry.
    static func type(_ pad: ValuePadModel, _ text: String) {
        while !pad.entry.isEmpty {
            pad.press(.delete)
        }
        for character in text {
            if character == "." {
                pad.press(.decimal)
            } else if character == "-" {
                pad.press(.minus)
            } else {
                pad.press(.digit(character.wholeNumberValue ?? 0))
            }
        }
    }
}

extension TxEqCurve {
    /// The curve as the Core sends it, `state` saved: its `curveJson` with the state added.
    var savedJson: String {
        var text = curveJson
        text.removeFirst()
        return #"{"state":"\#(state.rawValue)","# + text
    }
}
