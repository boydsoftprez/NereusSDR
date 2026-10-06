// NereusSDR for iOS: one number in a Setup list, stepped within its range or typed on the number pad, with its unit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// A number set with minus and plus (the desktop's spin boxes), its value
/// and unit beside its title, kept within its range. Tapping the value
/// opens the number pad for a typed value (JJ 2026-09-30), with the row's
/// unit, range and decimal places; minus and plus stay for small steps. A
/// number that can't be changed here is dimmed as a whole, like a greyed
/// switch, and opens no pad; its section says why.
struct SetupNumberRow: View {
    let title: String
    let value: Double
    let range: ClosedRange<Double>
    let step: Double
    let unit: String
    var decimals = 0
    var enabled = true
    let id: String
    /// The pad's title where the row's own title needs its section to be
    /// understood; nil uses the row's title.
    var padTitle: String? = nil
    /// Sends a typed value and returns the answer the pad shows; nil sends
    /// it through `set`, which keeps it at once.
    var enter: ((Double) async -> PropertyWriteOutcome?)?
    var enterWithLate: ((Double, @escaping @MainActor (PropertyWriteOutcome) -> Void) async -> PropertyWriteOutcome?)? = nil
    var onOutcome: ((PropertyWriteOutcome) -> Void)? = nil
    var readCurrent: (() -> Double?)? = nil
    /// Closed editors capture their gesture before opening the shared pad.
    var openValuePad: (() -> Void)? = nil
    let set: (Double) -> Void

    @Environment(\.valuePadHost) private var pads

    /// VoiceOver's action that opens the number pad.
    static let enterValueAction = "Enter value"
    /// How much of a greyed row shows: as dim as a greyed switch.
    static let greyedOpacity = 0.4

    /// The value as the row shows it: its decimals, then its unit.
    static func text(_ value: Double, decimals: Int, unit: String) -> String {
        let number = String(format: "%.\(max(0, decimals))f", value)
        return unit.isEmpty ? number : "\(number) \(unit)"
    }

    /// The pad a row opens: its title, unit and range, the value it holds
    /// now, and a decimal point only where the row has decimal places. A
    /// value outside the range is turned down on the pad; any other value
    /// goes to `send` as typed.
    static func pad(title: String, value: Double, range: ClosedRange<Double>, unit: String, decimals: Int,
                    send: @escaping (Double) async -> PropertyWriteOutcome?,
                    sendWithLate: ((Double, @escaping @MainActor (PropertyWriteOutcome) -> Void) async -> PropertyWriteOutcome?)? = nil,
                    onOutcome: ((PropertyWriteOutcome) -> Void)? = nil,
                    readCurrent: (() -> Double?)? = nil, close: @escaping () -> Void) -> ValuePadModel {
        if decimals > 0 {
            return ValuePadModel(title: title, unit: unit, range: range, decimals: decimals, current: value,
                                 send: send, sendWithLate: sendWithLate, onOutcome: onOutcome, readCurrent: readCurrent, close: close)
        }
        let low = Int64(range.lowerBound.rounded(.up))
        let high = max(low, Int64(range.upperBound.rounded(.down)))
        return ValuePadModel(title: title, unit: unit, range: low...high, current: Int64(value.rounded()),
                             send: { await send(Double($0)) },
                             sendWithLate: sendWithLate.map { send in { value, late in await send(Double(value), late) } },
                             onOutcome: onOutcome, readCurrent: readCurrent, close: close)
    }

    /// The answer for a value the phone keeps at once.
    static let kept = PropertyWriteOutcome(accepted: true, reason: "", value: nil)

    /// The row opens a pad: it can change and the screen draws one.
    private var typable: Bool { enabled && pads != nil }

    var body: some View {
        let shown = Self.text(value, decimals: decimals, unit: unit)
        Stepper {
            HStack {
                Text(title)
                Spacer(minLength: 8)
                Button {
                    openPad()
                } label: {
                    Text(shown)
                        .monospacedDigit()
                        .foregroundStyle(typable ? AnyShapeStyle(.tint) : AnyShapeStyle(.secondary))
                }
                .buttonStyle(.borderless)
                .disabled(!typable)
                .accessibilityIdentifier("\(id).value")
            }
        } onIncrement: {
            increment()
        } onDecrement: {
            decrement()
        }
        .disabled(!enabled)
        .opacity(enabled ? 1 : Self.greyedOpacity)
        // One element for VoiceOver: the title, the value, swipe up or
        // down to step it, and Enter value for the pad.
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(title)
        .accessibilityValue(shown)
        .accessibilityAdjustableAction { direction in
            guard enabled else {
                return
            }
            switch direction {
            case .increment:
                increment()
            case .decrement:
                decrement()
            @unknown default:
                break
            }
        }
        .accessibilityActions {
            if typable {
                Button(Self.enterValueAction) {
                    openPad()
                }
            }
        }
        .accessibilityIdentifier(id)
    }

    private func increment() {
        set(min(value + step, range.upperBound))
    }

    private func decrement() {
        set(max(value - step, range.lowerBound))
    }

    private func openPad() {
        guard typable, let pads else {
            return
        }
        if let openValuePad { openValuePad(); return }
        let set = set
        let send = enter ?? { typed in
            set(typed)
            return Self.kept
        }
        pads.open { close in
            Self.pad(title: padTitle ?? title, value: value, range: range, unit: unit, decimals: decimals, send: send,
                     sendWithLate: enterWithLate, onOutcome: onOutcome, readCurrent: readCurrent, close: close)
        }
    }
}
