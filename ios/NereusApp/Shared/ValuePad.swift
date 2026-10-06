// NereusSDR for iOS: the number pad for a typed setting, in the flag's number pad's style, and the box that opens it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The number pad a typed setting opens (the flag's number pad, D74, for a
/// whole number or, on a pad with decimal places, a decimal one): the
/// setting's name with Cancel, the entry, the range, the digits with a
/// sign key, the decimal point where the pad takes one, and delete, and "Set to" the value. The
/// Core's words for a value it refuses show under the entry, and the pad
/// stays open; a kept value closes it.
struct ValuePad: View {
    @ObservedObject var model: ValuePadModel

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 4) {
                Text(model.title)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.text.opacity(0.8))
                Spacer(minLength: 0)
                Button("Cancel") {
                    model.cancel()
                }
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(ChromeColours.accent)
                .buttonStyle(.plain)
                .accessibilityIdentifier("valuePadCancel")
            }
            entry
            Text(model.problem ?? model.refusal ?? model.rangeText)
                .font(.system(size: 12))
                .foregroundStyle(model.problem != nil || model.refusal != nil ? ChromeColours.noticeWarn
                                                                               : ChromeColours.textDim)
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("valuePadNote")
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
            .accessibilityIdentifier("valuePadEnter")
        }
        .padding(10)
        .frame(maxWidth: 380)
        .background(ChromeColours.sheet, in: RoundedRectangle(cornerRadius: 8))
        .overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(ChromeColours.sheetBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.55), radius: 14, y: -8)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("valuePad")
    }

    private var entry: some View {
        HStack(spacing: 6) {
            Spacer(minLength: 0)
            Text(model.shown)
                .font(.system(size: 28, weight: .bold, design: .monospaced))
                .foregroundStyle(ChromeColours.textBright)
                .lineLimit(1)
                .minimumScaleFactor(0.6)
            Rectangle().fill(ChromeColours.accent).frame(width: 2, height: 26)
            Text(model.unit)
                .font(.system(size: 14, weight: .semibold))
                .foregroundStyle(ChromeColours.textDim)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .frame(maxWidth: .infinity)
        .background(Self.entryBackground, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(Self.entryBorder, lineWidth: 1))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(model.title)
        .accessibilityValue("\(model.shown) \(model.unit)")
        .accessibilityIdentifier("valuePadEntry")
    }

    private var keys: some View {
        let rows: [[ValuePadModel.Key]] = [
            [.digit(1), .digit(2), .digit(3)],
            [.digit(4), .digit(5), .digit(6)],
            [.digit(7), .digit(8), .digit(9)],
            // A pad with decimal places gains the decimal point key.
            model.takesDecimals ? [.minus, .digit(0), .decimal, .delete] : [.minus, .digit(0), .delete],
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

    private func key(_ key: ValuePadModel.Key) -> some View {
        // The sign key is greyed where the range stays at or above zero.
        let off = key == .minus && !model.signed
        return Button {
            model.press(key)
        } label: {
            Group {
                switch key {
                case .digit(let digit):
                    Text(String(digit))
                case .minus:
                    Text("+/\u{2212}")
                case .decimal:
                    Text(".")
                case .delete:
                    Image(systemName: "delete.left")
                }
            }
            .font(.system(size: 18, weight: .semibold))
            .foregroundStyle(off ? ChromeColours.buttonOffText : ChromeColours.textBright)
            .frame(maxWidth: .infinity, minHeight: 40)
            .background(off ? ChromeColours.buttonOff : Self.key, in: RoundedRectangle(cornerRadius: 6))
            .overlay(RoundedRectangle(cornerRadius: 6)
                .strokeBorder(off ? ChromeColours.buttonOffBorder : Self.keyBorder, lineWidth: 1))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(off)
        .accessibilityLabel(Self.accessibilityLabel(key))
    }

    private static func accessibilityLabel(_ key: ValuePadModel.Key) -> String {
        switch key {
        case .digit(let digit):
            return String(digit)
        case .minus:
            return "Change sign"
        case .decimal:
            return "Decimal point"
        case .delete:
            return "Delete"
        }
    }

    // The flag's number pad's colours (the board's `.tupad`).
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

/// An open pad at the foot of a page, over a clear layer whose tap closes it.
struct ValuePadLayer: View {
    let pad: ValuePadModel?

    var body: some View {
        if let pad {
            ZStack(alignment: .bottom) {
                Color.black.opacity(0.35)
                    .contentShape(Rectangle())
                    .onTapGesture { pad.cancel() }
                    .accessibilityLabel("Close the number pad")
                    .accessibilityAddTraits(.isButton)
                ValuePad(model: pad)
                    .padding(8)
            }
        }
    }
}

/// A typed setting's value in its box, looking like a field: it opens the
/// number pad. Greyed, and taking no touch, while the setting cannot change.
struct ValueField: View {
    let text: String
    let accessibility: String
    var disabled = false
    var minWidth: CGFloat = 34
    var minHeight: CGFloat = 28
    var grow = false
    let open: () -> Void

    var body: some View {
        Button(action: open) {
            HStack(spacing: 4) {
                Text(text)
                    .font(.system(size: 12).monospacedDigit())
                    .foregroundStyle(disabled ? ChromeColours.buttonOffText : ChromeColours.text)
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
                if grow {
                    Spacer(minLength: 0)
                }
                Image(systemName: "number")
                    .font(.system(size: 9, weight: .semibold))
                    .foregroundStyle(disabled ? ChromeColours.buttonOffText : ChromeColours.accent)
            }
            .padding(.horizontal, 5)
            .frame(minWidth: minWidth, maxWidth: grow ? .infinity : nil, minHeight: minHeight)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
            .overlay(RoundedRectangle(cornerRadius: 3)
                .strokeBorder(disabled ? ChromeColours.insetBorder : ChromeColours.accent.opacity(0.7), lineWidth: 1))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(disabled)
        .accessibilityLabel(accessibility)
        .accessibilityValue(text)
        .accessibilityHint(disabled ? "" : "Type a value")
    }
}
