// NereusSDR for iOS: the number pad a typed setting opens: whole or decimal numbers within the control's range, and the Core's answer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror

/// What the number pad a typed setting opens holds (the flag's frequency
/// pad's twin, D74): the digits typed so far, a whole number or, on a pad
/// made with decimal places, a number with a decimal point (the TX EQ
/// curve's fields first, then every Setup number row, JJ 2026-09-30), its
/// sign where the control's range goes below zero, and the Core's words
/// when it refuses the value. Enter sends the value; the pad closes when
/// the Core keeps it and stays open, showing the Core's words, when it
/// does not. A value outside the control's range, or one the control's
/// own rule turns down (``check``), sends nothing and says why.
@MainActor
final class ValuePadModel: ObservableObject, Identifiable {
    /// One of the pad's keys.
    enum Key: Equatable {
        case digit(Int)
        /// Changes the sign; greyed where the range stays at or above zero.
        case minus
        /// The decimal point; a pad of whole numbers has none.
        case decimal
        case delete
    }

    /// The longest entry a pad takes, in digits, unless its range needs more.
    nonisolated static let longestEntry = 7

    /// The longest entry this pad takes, in digits: ``longestEntry``, or
    /// enough for the range's widest end with all its decimal places (an
    /// Alex filter edge of 200 MHz to 6 places, a notch at 61440000 Hz).
    let longest: Int

    let id = UUID()
    /// What is being set, as the pad's heading says it: "RIT offset".
    let title: String
    /// The value's unit, "Hz" or "dB".
    let unit: String
    /// The range as whole numbers (a decimal pad's range rounded outwards).
    let range: ClosedRange<Int64>
    /// The range as the control gives it, decimals included.
    let numberRange: ClosedRange<Double>
    /// How many digits the entry takes after the decimal point: 0 for a
    /// pad of whole numbers, which has no decimal point.
    let decimals: Int
    /// The step the control's buttons take, as the pad states it ("0.1");
    /// nil on a pad of whole numbers.
    let stepText: String?
    /// The value the control holds now, as a decimal pad states it.
    let nowText: String?
    /// The control's own rule beyond its range: why a value is turned
    /// down, or nil when it is fine.
    let check: ((Int64) -> String?)?
    private let numberCheck: ((Double) -> String?)?

    @Published private(set) var entry = ""
    @Published private(set) var negative = false
    /// The Core's words for the value it last refused.
    @Published private(set) var refusal: String?
    /// A value is on its way to the Core.
    @Published private(set) var sending = false

    private let send: (Double) async -> PropertyWriteOutcome?
    private let close: () -> Void
    private let sendWithLate: ((Double, @escaping @MainActor (PropertyWriteOutcome) -> Void) async -> PropertyWriteOutcome?)?
    private let onOutcome: ((PropertyWriteOutcome) -> Void)?
    private let readCurrent: (() -> Double?)?
    private var entryRevision: UInt64 = 0
    private var operation: UInt64 = 0
    private var retired = false

    /// A pad of whole numbers. `current` starts the entry; `send` returns
    /// the Core's answer, or nil when nothing could be sent.
    init(title: String, unit: String, range: ClosedRange<Int64>, current: Int64?,
         check: ((Int64) -> String?)? = nil,
         send: @escaping (Int64) async -> PropertyWriteOutcome? = { _ in nil },
         sendWithLate: ((Int64, @escaping @MainActor (PropertyWriteOutcome) -> Void) async -> PropertyWriteOutcome?)? = nil,
         onOutcome: ((PropertyWriteOutcome) -> Void)? = nil,
         readCurrent: (() -> Double?)? = nil, close: @escaping () -> Void) {
        self.title = title
        self.unit = unit
        self.range = range
        numberRange = Double(range.lowerBound)...Double(range.upperBound)
        decimals = 0
        stepText = nil
        nowText = nil
        self.check = check
        numberCheck = nil
        longest = Self.longest(Double(range.lowerBound)...Double(range.upperBound), decimals: 0)
        self.send = { await send(Int64($0.rounded())) }
        self.sendWithLate = sendWithLate.map { send in { value, late in await send(Int64(value.rounded()), late) } }
        self.onOutcome = onOutcome
        self.readCurrent = readCurrent
        self.close = close
        if let current {
            entry = String(current.magnitude)
            negative = current < 0 && range.lowerBound < 0
        }
    }

    /// A pad for a number with up to `decimals` digits after its decimal
    /// point. `step` is the step the control's buttons take ("0.1"), which
    /// the pad states with the range and the value held now. `current`
    /// starts the entry, without trailing zeros; `send` gets the value
    /// typed and returns the Core's answer, or nil when nothing could be sent.
    init(title: String, unit: String, range: ClosedRange<Double>, decimals: Int, step: String? = nil,
         current: Double?, check: ((Double) -> String?)? = nil,
         send: @escaping (Double) async -> PropertyWriteOutcome? = { _ in nil },
         sendWithLate: ((Double, @escaping @MainActor (PropertyWriteOutcome) -> Void) async -> PropertyWriteOutcome?)? = nil,
         onOutcome: ((PropertyWriteOutcome) -> Void)? = nil,
         readCurrent: (() -> Double?)? = nil, close: @escaping () -> Void) {
        let places = max(0, decimals)
        self.title = title
        self.unit = unit
        self.range = Int64(range.lowerBound.rounded(.down))...Int64(range.upperBound.rounded(.up))
        numberRange = range
        self.decimals = places
        stepText = step
        nowText = current.map { Self.text($0, decimals: places, plus: range.lowerBound < 0) }
        self.check = nil
        numberCheck = check
        longest = Self.longest(range, decimals: places)
        self.send = send
        self.sendWithLate = sendWithLate
        self.onOutcome = onOutcome
        self.readCurrent = readCurrent
        self.close = close
        if let current {
            entry = Self.entryText(current.magnitude, decimals: places)
            negative = current < 0 && range.lowerBound < 0 && entry != "0"
        }
    }

    /// The range goes below zero, so the sign key works.
    var signed: Bool { numberRange.lowerBound < 0 }

    /// The pad takes a decimal point.
    var takesDecimals: Bool { decimals > 0 }

    /// A key pressed: a digit adds to the entry, minus changes the sign,
    /// the decimal point starts the decimals, delete takes the last
    /// character away.
    func press(_ key: Key) {
        let before = (entry, negative)
        defer { if before.0 != entry || before.1 != negative { entryRevision &+= 1 } }
        refusal = nil
        switch key {
        case .digit(let digit):
            guard (0...9).contains(digit), entry.filter(\.isNumber).count < longest else {
                return
            }
            if let point = entry.firstIndex(of: ".") {
                guard entry.distance(from: point, to: entry.endIndex) - 1 < decimals else {
                    return
                }
            } else if entry == "0" {
                entry = ""
            }
            entry.append(String(digit))
        case .minus:
            guard signed else {
                return
            }
            negative.toggle()
        case .decimal:
            guard takesDecimals, !entry.contains(".") else {
                return
            }
            entry.append(entry.isEmpty ? "0." : ".")
        case .delete:
            if !entry.isEmpty {
                entry.removeLast()
            }
        }
    }

    /// The typed value, decimals included, or nil while nothing is typed.
    var number: Double? {
        guard !entry.isEmpty, let magnitude = Double(entry) else {
            return nil
        }
        return negative ? -magnitude : magnitude
    }

    /// The typed value as a whole number, or nil while nothing is typed.
    var value: Int64? {
        number.map { Int64($0.rounded()) }
    }

    /// The entry as it reads on the pad, with a true minus sign, and on a
    /// signed pad of decimals a plus sign above zero.
    var shown: String {
        let text = entry.isEmpty ? "0" : entry
        if negative {
            return "\u{2212}" + text
        }
        if takesDecimals, signed, let number, number > 0 {
            return "+" + text
        }
        return text
    }

    /// The range as the pad states it: "From −10000 to 10000 Hz". A
    /// decimal pad adds its step and the value held now: "From −24 to 24
    /// dB, in steps of 0.1 dB. Now −1.5 dB."
    var rangeText: String {
        let range = "From \(Self.text(numberRange.lowerBound)) to \(Self.text(numberRange.upperBound)) \(unit)"
        guard takesDecimals else {
            return range
        }
        let space = unit.isEmpty ? "" : " \(unit)"
        var text = range.trimmingCharacters(in: .whitespaces)
        if let stepText {
            text += ", in steps of \(stepText)\(space)"
        }
        text += "."
        if let nowText {
            text += " Now \(nowText)\(space)."
        }
        return text
    }

    /// Why the typed value cannot be sent, or nil when it can.
    var problem: String? {
        guard let number else {
            return nil
        }
        if !numberRange.contains(number) {
            let range = "\(Self.text(numberRange.lowerBound)) to \(Self.text(numberRange.upperBound))"
            return takesDecimals && unit.isEmpty ? "Choose a value from \(range)." : "Choose a value from \(range) \(unit)."
        }
        if takesDecimals {
            return numberCheck?(number)
        }
        return check?(Int64(number.rounded()))
    }

    var canEnter: Bool { !retired && number != nil && problem == nil && !sending }

    /// A specialized sender uses the same typed-entry owner as this pad.
    var outcomeIdentity: UInt64? { retired ? nil : entryRevision }

    /// The Enter button's words: "Set to −250 Hz", "Set to +2.0 dB".
    var enterLabel: String {
        guard let number else {
            return "Set"
        }
        guard takesDecimals else {
            return "Set to \(Self.text(Int64(number.rounded()))) \(unit)"
        }
        let text = Self.text(number, decimals: decimals, plus: signed)
        return unit.isEmpty ? "Set to \(text)" : "Set to \(text) \(unit)"
    }

    /// Enter: sends the value. A kept answer returns true; only its current pad owner closes.
    @discardableResult
    func enter() async -> Bool {
        guard canEnter, let number else {
            return false
        }
        sending = true
        operation &+= 1
        let attempt = operation
        let revision = entryRevision
        var finished = false
        var buffered: PropertyWriteOutcome?
        let late: @MainActor (PropertyWriteOutcome) -> Void = { [weak self] outcome in
            guard let self, self.current(attempt, revision) else { return }
            if finished { _ = self.receive(outcome) } else { buffered = outcome }
        }
        let outcome = if let sendWithLate { await sendWithLate(number, late) } else { await send(number) }
        if operation == attempt { sending = false }
        finished = true
        guard let outcome = buffered ?? outcome else { return false }
        if retired {
            // A replaced pad has already closed. Its admitted write can still
            // report acceptance without touching the new pad or its notes.
            return entryRevision == revision && outcome.isCurrent && outcome.accepted
        }
        guard current(attempt, revision) else { return false }
        return receive(outcome)
    }

    private func current(_ attempt: UInt64, _ revision: UInt64) -> Bool {
        !retired && operation == attempt && entryRevision == revision
    }

    private func receive(_ outcome: PropertyWriteOutcome) -> Bool {
        guard outcome.isCurrent, !outcome.heldForQuestion else { return false }
        onOutcome?(outcome)
        if outcome.accepted {
            retire()
            close()
            return true
        }
        if !outcome.answeredByCore, let readCurrent {
            // Restoration is presentation for this submitted revision. It
            // does not masquerade as a new edit or retire its late answer.
            if let value = readCurrent(), value.isFinite {
                entry = Self.entryText(value.magnitude, decimals: decimals)
                negative = value < 0 && signed && entry != "0"
            } else {
                entry = ""
                negative = false
            }
        }
        refusal = outcome.reason.isEmpty ? nil : outcome.reason
        return false
    }

    func retire() { retired = true; operation &+= 1; sending = false }

    /// Cancel: the pad closes and nothing is sent.
    func cancel() {
        retire()
        close()
    }

    /// The digits the widest end of `range` needs with `decimals` places,
    /// and never fewer than ``longestEntry``.
    nonisolated static func longest(_ range: ClosedRange<Double>, decimals: Int) -> Int {
        let widest = max(range.lowerBound.magnitude, range.upperBound.magnitude)
        guard widest.isFinite else {
            return longestEntry
        }
        let whole = String(format: "%.0f", widest.rounded(.down)).count
        return max(longestEntry, whole + max(0, decimals))
    }

    /// A whole number with a true minus sign.
    static func text(_ value: Int64) -> String {
        value < 0 ? "\u{2212}\(value.magnitude)" : "\(value)"
    }

    /// A number with a true minus sign and no trailing zeros: "−24", "0.2".
    nonisolated static func text(_ value: Double) -> String {
        let magnitude = entryText(value.magnitude, decimals: 6)
        return value < 0 && magnitude != "0" ? "\u{2212}" + magnitude : magnitude
    }

    /// A number to `decimals` places with a true minus sign, and a plus
    /// sign above zero when `plus`: "+2.0", "−1.5", "0.0".
    nonisolated static func text(_ value: Double, decimals: Int, plus: Bool) -> String {
        let magnitude = String(format: "%.\(max(0, decimals))f", value.magnitude)
        if Double(magnitude) == 0 {
            return magnitude
        }
        return value < 0 ? "\u{2212}" + magnitude : (plus ? "+" + magnitude : magnitude)
    }

    /// A magnitude as a decimal pad's entry starts: to `decimals` places,
    /// trailing zeros and a bare point taken off ("4", "1.5", "0").
    nonisolated static func entryText(_ magnitude: Double, decimals: Int) -> String {
        var text = String(format: "%.\(max(0, decimals))f", magnitude)
        if text.contains(".") {
            while text.hasSuffix("0") {
                text.removeLast()
            }
            if text.hasSuffix(".") {
                text.removeLast()
            }
        }
        return text
    }
}


/// One note area's current operator action, retained after its answer.
@MainActor
final class ControlOutcomeOwner {
    private var next: UInt64 = 0
    private var current: [String: UInt64] = [:]
    func begin(_ slot: String = "note") -> UInt64 {
        next &+= 1
        current[slot] = next
        return next
    }
    func isCurrent(_ edit: UInt64, _ slot: String = "note") -> Bool { current[slot] == edit }
}
