// NereusSDR for iOS: the number pad a tap on a flag's frequency opens: MHz or kHz, digits, Tune to
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The number pad, as picture 26 draws it (D74, spec section 5.1 item 17):
/// "Frequency for slice A" with Cancel, the entry, MHz and kHz, the digits
/// with a point and delete, and "Tune to" the frequency as the flag writes
/// it. The Core's words for a frequency it refuses show under the entry,
/// and the pad stays open; a kept frequency closes it.
struct FrequencyPad: View {
    @ObservedObject var model: FrequencyPadModel

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            head
            entry
            if let refusal = model.refusal {
                Text(refusal)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.noticeWarn)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("frequencyPadRefusal")
            }
            HStack(spacing: 6) {
                ForEach(FrequencyPadModel.Unit.allCases, id: \.label) { unit in
                    PanelButton(label: unit.label, lit: model.unit == unit, style: .blue) {
                        model.select(unit)
                    }
                }
            }
            keys
            Button {
                Task { await model.enter() }
            } label: {
                Text(model.enterLabel)
                    .font(.system(size: 14, weight: .bold))
                    .foregroundStyle(model.canEnter ? Color.white : ChromeColours.buttonOffText)
                    .frame(maxWidth: .infinity, minHeight: 40)
                    .background(model.canEnter ? Self.go : ChromeColours.buttonOff, in: RoundedRectangle(cornerRadius: 6))
                    .overlay(RoundedRectangle(cornerRadius: 6)
                        .strokeBorder(model.canEnter ? Self.goBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!model.canEnter)
            .accessibilityIdentifier("frequencyPadEnter")
        }
        .padding(10)
        .background(ChromeColours.sheet, in: RoundedRectangle(cornerRadius: 8))
        .overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(ChromeColours.sheetBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.55), radius: 14, y: -8)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("frequencyPad")
    }

    private var head: some View {
        HStack(spacing: 4) {
            Text("Frequency for slice")
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.text.opacity(0.8))
            SliceLetterBadge(letter: model.letter, colour: BandColours.slice(model.colour))
            Spacer(minLength: 0)
            Button("Cancel") {
                model.cancel()
            }
            .font(.system(size: 13, weight: .semibold))
            .foregroundStyle(ChromeColours.accent)
            .buttonStyle(.plain)
        }
    }

    private var entry: some View {
        HStack(spacing: 2) {
            Spacer(minLength: 0)
            Text(model.entry)
                .font(.system(size: 28, weight: .bold, design: .monospaced))
                .foregroundStyle(ChromeColours.textBright)
                .lineLimit(1)
                .minimumScaleFactor(0.6)
            Rectangle().fill(ChromeColours.accent).frame(width: 2, height: 26)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .frame(maxWidth: .infinity)
        .background(Self.entryBackground, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(Self.entryBorder, lineWidth: 1))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Frequency")
        .accessibilityValue("\(model.entry) \(model.unit.label)")
    }

    private var keys: some View {
        let rows: [[FrequencyPadModel.Key]] = [
            [.digit(1), .digit(2), .digit(3)],
            [.digit(4), .digit(5), .digit(6)],
            [.digit(7), .digit(8), .digit(9)],
            [.point, .digit(0), .delete],
        ]
        return VStack(spacing: 6) {
            ForEach(rows.indices, id: \.self) { row in
                HStack(spacing: 6) {
                    ForEach(rows[row].indices, id: \.self) { column in
                        key(rows[row][column])
                    }
                }
            }
        }
    }

    private func key(_ key: FrequencyPadModel.Key) -> some View {
        Button {
            model.press(key)
        } label: {
            Group {
                switch key {
                case .digit(let digit):
                    Text(String(digit))
                case .point:
                    Text(".")
                case .delete:
                    Image(systemName: "delete.left")
                }
            }
            .font(.system(size: 18, weight: .semibold))
            .foregroundStyle(ChromeColours.textBright)
            .frame(maxWidth: .infinity, minHeight: 40)
            .background(Self.key, in: RoundedRectangle(cornerRadius: 6))
            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(Self.keyBorder, lineWidth: 1))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(Self.accessibilityLabel(key))
    }

    private static func accessibilityLabel(_ key: FrequencyPadModel.Key) -> String {
        switch key {
        case .digit(let digit):
            return String(digit)
        case .point:
            return "Point"
        case .delete:
            return "Delete"
        }
    }

    // The board's `.tupad` colours.
    private static let entryBackground = rgb(0x07, 0x0C, 0x14)
    private static let entryBorder = rgb(0x2A, 0x4A, 0x60)
    private static let key = rgb(0x1A, 0x26, 0x36)
    private static let keyBorder = rgb(0x2A, 0x3A, 0x4C)
    private static let go = rgb(0x1A, 0x5C, 0xA8)
    private static let goBorder = rgb(0x3A, 0x7C, 0xC8)

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}
