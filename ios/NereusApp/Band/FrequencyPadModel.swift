// NereusSDR for iOS: the number pad's state: the typed frequency in MHz or kHz, and the Core's answer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror

/// What the number pad a tap on a flag's frequency opens holds (D74,
/// spec section 5.1 item 17, picture 26): the digits typed so far, in
/// megahertz or kilohertz, and the Core's words when it refuses the
/// frequency. Enter writes the slice's frequency; the pad closes when the
/// Core keeps it and stays open, showing the Core's words, when it does not.
/// An empty entry, or one that is not a number, sends nothing.
@MainActor
final class FrequencyPadModel: ObservableObject {
    /// The unit the entry is typed in.
    enum Unit: Equatable, CaseIterable {
        case megahertz
        case kilohertz

        var label: String {
            switch self {
            case .megahertz:
                return "MHz"
            case .kilohertz:
                return "kHz"
            }
        }

        var hertz: Double {
            switch self {
            case .megahertz:
                return 1_000_000
            case .kilohertz:
                return 1_000
            }
        }
    }

    /// One of the pad's keys.
    enum Key: Equatable {
        case digit(Int)
        case point
        case delete
    }

    /// The longest entry the pad takes, in characters.
    static let longestEntry = 12

    let sliceId: Int
    /// The slice's letter, `A`, and its colour from the catalogue.
    let letter: String
    let colour: String

    @Published private(set) var entry = ""
    @Published private(set) var unit: Unit = .megahertz
    /// The Core's words for the frequency it last refused.
    @Published private(set) var refusal: String?
    /// A frequency is on its way to the Core.
    @Published private(set) var sending = false

    private let tune: (Double) async -> PropertyWriteOutcome
    private let close: () -> Void
    private let readCurrent: (() -> Double?)?
    private let tuneWithLate: ((Double, @escaping @MainActor (PropertyWriteOutcome) -> Void) async -> PropertyWriteOutcome)?
    private var revision: UInt64 = 0
    private var attempt: UInt64 = 0
    private var retired = false


    init(sliceId: Int, letter: String, colour: String, tune: @escaping (Double) async -> PropertyWriteOutcome,
         tuneWithLate: ((Double, @escaping @MainActor (PropertyWriteOutcome) -> Void) async -> PropertyWriteOutcome)? = nil,
         readCurrent: (() -> Double?)? = nil, close: @escaping () -> Void) {
        self.sliceId = sliceId
        self.letter = letter
        self.colour = colour
        self.tune = tune
        self.tuneWithLate = tuneWithLate
        self.readCurrent = readCurrent
        self.close = close
    }

    /// A key pressed: a digit or the point adds to the entry (one point at
    /// most), delete takes the last character away.
    func press(_ key: Key) {
        let before = entry
        defer { if before != entry { revision &+= 1 } }
        refusal = nil
        switch key {
        case .digit(let digit):
            guard (0...9).contains(digit), entry.count < Self.longestEntry else {
                return
            }
            entry.append(String(digit))
        case .point:
            guard !entry.contains("."), entry.count < Self.longestEntry else {
                return
            }
            entry.append(".")
        case .delete:
            if !entry.isEmpty {
                entry.removeLast()
            }
        }
    }

    func select(_ unit: Unit) {
        refusal = nil
        if self.unit != unit { revision &+= 1 }
        self.unit = unit
    }

    /// The typed frequency in whole hertz, or nil when there is none.
    var hertz: Double? { Self.hertz(entry, unit: unit) }

    /// `text` in `unit` as whole hertz: nil for an empty entry, one that is
    /// not a number, or one that is not above zero.
    static func hertz(_ text: String, unit: Unit) -> Double? {
        let trimmed = text.trimmingCharacters(in: .whitespaces)
        guard !trimmed.isEmpty, trimmed.allSatisfy({ $0.isASCII && ($0.isNumber || $0 == ".") }),
              trimmed.filter({ $0 == "." }).count <= 1, let value = Double(trimmed), value.isFinite else {
            return nil
        }
        let hz = (value * unit.hertz).rounded()
        return hz > 0 ? hz : nil
    }

    /// The Enter button's words: where it tunes, as the flag writes it.
    var enterLabel: String {
        guard let hertz else {
            return "Tune to"
        }
        return "Tune to \(BandSlice.frequencyText(hz: hertz))"
    }

    var canEnter: Bool { hertz != nil && !sending }

    /// Enter: writes the frequency. Returns true when the Core kept it and
    /// the pad closed.
    @discardableResult
    func enter() async -> Bool {
        guard let hertz, !sending else {
            return false
        }
        sending = true
        attempt &+= 1
        let operation = attempt
        let edit = revision
        var finished = false
        var buffered: PropertyWriteOutcome?
        let late: @MainActor (PropertyWriteOutcome) -> Void = { [weak self] outcome in
            guard let self, self.current(operation, edit) else { return }
            if finished { _ = self.receive(outcome) } else { buffered = outcome }
        }
        let outcome = if let tuneWithLate { await tuneWithLate(hertz, late) } else { await tune(hertz) }
        if attempt == operation { sending = false }
        finished = true
        guard current(operation, edit) else { return false }
        return receive(buffered ?? outcome)
    }

    private func current(_ operation: UInt64, _ edit: UInt64) -> Bool {
        !retired && attempt == operation && revision == edit
    }

    private func receive(_ outcome: PropertyWriteOutcome) -> Bool {
        guard outcome.isCurrent, !outcome.heldForQuestion else { return false }
        if outcome.accepted { retire(); close(); return true }
        if !outcome.answeredByCore, let hertz = readCurrent?(), hertz.isFinite, hertz > 0 {
            entry = ValuePadModel.entryText(hertz / unit.hertz, decimals: unit == .megahertz ? 6 : 3)
        }
        refusal = outcome.reason.isEmpty ? nil : outcome.reason
        return false
    }

    func retire() { retired = true; attempt &+= 1; sending = false }

    /// Cancel: the pad closes and nothing is sent.
    func cancel() { retire(); close() }
}
