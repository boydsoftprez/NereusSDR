// NereusSDR for iOS: the number pad for a typed setting: digits, sign, range, and the Core's answer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
@testable import NereusSDR
import Testing

/// The typed-value pad (C1, C2, I5, I7, I9): the entry starts at the
/// setting's value, the sign key works only where the range goes below
/// zero, a value outside the range or the setting's own rule is turned
/// down before anything is sent, a kept value closes the pad and a
/// refused one keeps it open with the Core's words.
@Suite("Number pad for a typed setting")
@MainActor
struct ValuePadTests {
    final class Sent {
        var values: [Int64] = []
        var closed = 0
    }

    private func pad(range: ClosedRange<Int64>, current: Int64?, answer: PropertyWriteOutcome?,
                     check: ((Int64) -> String?)? = nil, sent: Sent) -> ValuePadModel {
        ValuePadModel(title: "RIT offset", unit: "Hz", range: range, current: current, check: check,
                      send: { value in
                          sent.values.append(value)
                          return answer
                      }, close: { sent.closed += 1 })
    }

    @Test("the entry starts at the value, takes digits and a sign, and deletes")
    func typing() {
        let sent = Sent()
        let model = pad(range: -10_000...10_000, current: -120, answer: nil, sent: sent)
        #expect(model.entry == "120" && model.negative && model.value == -120)
        #expect(model.shown == "\u{2212}120")
        model.press(.delete)
        model.press(.digit(5))
        #expect(model.value == -125)
        model.press(.minus)
        #expect(model.value == 125 && model.enterLabel == "Set to 125 Hz")
        #expect(model.rangeText == "From \u{2212}10000 to 10000 Hz")
    }

    @Test("a range at or above zero keeps the sign key off")
    func unsigned() {
        let model = pad(range: 0...5000, current: 100, answer: nil, sent: Sent())
        #expect(!model.signed)
        model.press(.minus)
        #expect(!model.negative && model.value == 100)
    }

    @Test("outside the range or the setting's rule, nothing is sent")
    func turnedDown() async {
        let sent = Sent()
        let model = pad(range: 200...10_000, current: 2900, answer: PropertyWriteOutcome(accepted: true, reason: "", value: nil),
                        check: { $0 == 4000 ? "Not that one." : nil }, sent: sent)
        model.press(.digit(0))
        #expect(model.value == 29000 && model.problem == "Choose a value from 200 to 10000 Hz." && !model.canEnter)
        #expect(await model.enter() == false)
        for _ in 0..<5 {
            model.press(.delete)
        }
        for digit in [4, 0, 0, 0] {
            model.press(.digit(digit))
        }
        #expect(model.problem == "Not that one.")
        #expect(await model.enter() == false)
        #expect(sent.values.isEmpty && sent.closed == 0)
    }

    @Test("a kept value closes the pad; a refused one keeps it open with the Core's words")
    func answers() async {
        let kept = Sent()
        let keeping = pad(range: 0...100, current: 10, answer: PropertyWriteOutcome(accepted: true, reason: "", value: nil),
                          sent: kept)
        #expect(await keeping.enter())
        #expect(kept.values == [10] && kept.closed == 1)

        let refused = Sent()
        let refusing = pad(range: 0...100, current: 10,
                           answer: PropertyWriteOutcome(accepted: false, reason: "The radio is on the air. Try again when it stops.",
                                                        value: nil),
                           sent: refused)
        #expect(await refusing.enter() == false)
        #expect(refused.closed == 0 && refusing.refusal == "The radio is on the air. Try again when it stops.")
        refusing.press(.digit(1))
        #expect(refusing.refusal == nil)
        refusing.cancel()
        #expect(refused.closed == 1)
    }

    // MARK: A pad with decimal places (the TX EQ curve's fields, then every Setup number row)

    final class SentNumbers {
        var values: [Double] = []
        var closed = 0
    }

    private func decimalPad(range: ClosedRange<Double>, decimals: Int, step: String?, current: Double?,
                            unit: String = "dB", answer: PropertyWriteOutcome?,
                            check: ((Double) -> String?)? = nil, sent: SentNumbers) -> ValuePadModel {
        ValuePadModel(title: "Point 3 gain", unit: unit, range: range, decimals: decimals, step: step, current: current,
                      check: check, send: { value in
                          sent.values.append(value)
                          return answer
                      }, close: { sent.closed += 1 })
    }

    @Test("a whole-number pad has no decimal point, and its key does nothing")
    func wholeHasNoPoint() {
        let model = pad(range: 0...5000, current: 100, answer: nil, sent: Sent())
        #expect(!model.takesDecimals)
        model.press(.decimal)
        #expect(model.entry == "100" && model.value == 100)
    }

    @Test("a decimal pad starts at the value, takes one point and its places, and states step and value")
    func decimalTyping() {
        let sent = SentNumbers()
        let model = decimalPad(range: -24...24, decimals: 1, step: "0.1", current: -1.5, answer: nil, sent: sent)
        #expect(model.takesDecimals && model.signed)
        #expect(model.entry == "1.5" && model.negative && model.number == -1.5)
        #expect(model.shown == "\u{2212}1.5")
        #expect(model.rangeText == "From \u{2212}24 to 24 dB, in steps of 0.1 dB. Now \u{2212}1.5 dB.")
        // Deleting every digit keeps the sign.
        while !model.entry.isEmpty {
            model.press(.delete)
        }
        model.press(.decimal)
        #expect(model.entry == "0." && model.shown == "\u{2212}0.")
        model.press(.minus)
        model.press(.delete)
        model.press(.delete)
        model.press(.digit(2))
        model.press(.decimal)
        model.press(.decimal)
        model.press(.digit(0))
        model.press(.digit(5))
        // One place only: the second digit after the point is not taken.
        #expect(model.entry == "2.0" && model.number == 2 && model.shown == "+2.0")
        #expect(model.enterLabel == "Set to +2.0 dB")
        // A whole value starts without trailing zeros.
        let whole = decimalPad(range: 0.2...20, decimals: 2, step: "0.01", current: 4, unit: "", answer: nil,
                               sent: SentNumbers())
        #expect(whole.entry == "4" && !whole.signed)
        #expect(whole.rangeText == "From 0.2 to 20, in steps of 0.01. Now 4.00.")
        whole.press(.decimal)
        whole.press(.digit(2))
        whole.press(.digit(5))
        whole.press(.digit(9))
        #expect(whole.number == 4.25 && whole.enterLabel == "Set to 4.25")
    }

    @Test("a decimal value outside the range or the rule sends nothing; a kept one is sent as typed")
    func decimalAnswers() async {
        let sent = SentNumbers()
        let model = decimalPad(range: 0.2...20, decimals: 2, step: "0.01", current: 0.2, unit: "",
                               answer: PropertyWriteOutcome(accepted: true, reason: "", value: nil),
                               check: { $0 == 7 ? "Not that one." : nil }, sent: sent)
        while !model.entry.isEmpty {
            model.press(.delete)
        }
        model.press(.digit(0))
        model.press(.decimal)
        model.press(.digit(1))
        #expect(model.problem == "Choose a value from 0.2 to 20." && !model.canEnter)
        #expect(await model.enter() == false)
        model.press(.delete)
        model.press(.delete)
        model.press(.delete)
        model.press(.digit(7))
        #expect(model.problem == "Not that one.")
        model.press(.delete)
        model.press(.digit(1))
        model.press(.digit(2))
        model.press(.decimal)
        model.press(.digit(7))
        model.press(.digit(5))
        #expect(await model.enter())
        #expect(sent.values == [12.75] && sent.closed == 1)
    }
    @Test("a held question creates no refusal on a typed-value pad")
    func heldQuestionIsNotAPadRefusal() async {
        let sent = Sent()
        let model = pad(range: 0...100, current: 10,
                        answer: .init(accepted: false, reason: SeveralDevices.waitingReason, value: nil), sent: sent)
        #expect(await model.enter() == false)
        #expect(model.refusal == nil)
        #expect(sent.closed == 0)
    }

    @Test("an earlier pad entry's answer cannot close or add a note after the entry changes", arguments: [true, false])
    func answerAfterNewEntryIsRetired(accepted: Bool) async {
        var continuation: CheckedContinuation<PropertyWriteOutcome?, Never>?
        var closed = 0
        let model = ValuePadModel(title: "Offset", unit: "Hz", range: Int64(0)...100, current: 10,
                                  send: { _ in await withCheckedContinuation { continuation = $0 } },
                                  close: { closed += 1 })
        let enter = Task { await model.enter() }
        for _ in 0..<2_000 {
            if continuation != nil { break }
            await Task.yield()
        }
        #expect(continuation != nil)
        model.press(.delete)
        model.press(.digit(2))
        continuation?.resume(returning: .init(accepted: accepted, reason: accepted ? "" : "Old entry refused.", value: nil))
        #expect(await enter.value == false)
        #expect(model.value == 12)
        #expect(model.refusal == nil)
        #expect(closed == 0)
    }

}
