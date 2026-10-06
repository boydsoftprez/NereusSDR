// NereusSDR for iOS: the parts every several-devices sheet is built from, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The pieces of the several-devices sheets (pictures 22 and 23; the
/// board's `.tsheet`, `.tsheet__who`, `.tsheet__what`, `.mcpick` and
/// `.tsheet__note`): a sheet rising over the band, a device's row, a
/// change's row, a row to pick, and the note under them.
enum ConfirmationSheetChrome {
    static let sheetFill = rgb(0x0A, 0x0A, 0x18)
    static let sheetEdge = rgb(0x30, 0x40, 0x50)
    static let rowFill = rgb(0x0D, 0x1B, 0x28)
    static let rowEdge = rgb(0x20, 0x30, 0x40)
    static let pickedFill = rgb(0x0D, 0x22, 0x36)
    static let picked = rgb(0x00, 0x90, 0xE0)
    static let dotOff = rgb(0x40, 0x50, 0x60)
    static let frequency = rgb(0x00, 0xE5, 0xFF)
    static let changedTo = rgb(0xFF, 0xD7, 0x00)
    static let changedFrom = rgb(0x60, 0x70, 0x80)
    static let refused = rgb(0xFF, 0x80, 0x80)

    /// The sheet: its grab, its heading and its content, rising from the
    /// foot of the band.
    struct Sheet<Content: View>: View {
        let kicker: String
        @ViewBuilder let content: () -> Content

        var body: some View {
            VStack(alignment: .leading, spacing: 12) {
                Capsule()
                    .fill(ChromeColours.panelEdge)
                    .frame(width: 36, height: 4)
                    .frame(maxWidth: .infinity)
                Text(kicker)
                    .font(.system(size: 19, weight: .heavy))
                    .foregroundStyle(ChromeColours.textBright)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityAddTraits(.isHeader)
                content()
            }
            .padding(.top, 8)
            .padding(.horizontal, 18)
            .padding(.bottom, 20)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(sheetFill, in: UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14))
            .overlay(alignment: .top) {
                Rectangle().fill(sheetEdge).frame(height: 1)
            }
            .shadow(color: .black.opacity(0.5), radius: 15, y: -10)
        }
    }

    /// A device, with what it is doing under its name.
    struct Who: View {
        let kind: String
        let name: String
        let detail: String

        var body: some View {
            HStack(spacing: 12) {
                Image(systemName: Self.glyph(kind))
                    .font(.system(size: 24, weight: .light))
                    .foregroundStyle(ChromeColours.icon)
                    .frame(width: 34, height: 34)
                VStack(alignment: .leading, spacing: 2) {
                    Text(name)
                        .font(.system(size: 16, weight: .bold))
                        .foregroundStyle(ChromeColours.text)
                    Text(detail)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Spacer(minLength: 0)
            }
            .padding(12)
            .background(rowFill, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(rowEdge, lineWidth: 1))
            .accessibilityElement(children: .combine)
        }

        /// The device's picture from its kind.
        static func glyph(_ kind: String) -> String {
            switch kind {
            case "phone":
                return "iphone"
            case "tablet":
                return "ipad"
            case "station":
                return "antenna.radiowaves.left.and.right"
            default:
                return "laptopcomputer"
            }
        }
    }

    /// The change itself: its label, and from and to.
    struct What: View {
        let label: String
        let from: String
        let to: String

        var body: some View {
            HStack(spacing: 8) {
                Text(label)
                    .font(.system(size: 14))
                    .foregroundStyle(ChromeColours.text)
                Spacer(minLength: 8)
                HStack(spacing: 4) {
                    Text(from).strikethrough().foregroundStyle(changedFrom)
                    Text("\u{2192}").foregroundStyle(ChromeColours.textDim)
                    Text(to).bold().foregroundStyle(changedTo)
                }
                .font(.system(size: 14, design: .monospaced))
                .lineLimit(1)
            }
            .padding(.vertical, 10)
            .padding(.horizontal, 12)
            .background(rowFill, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(rowEdge, lineWidth: 1))
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("\(label), from \(from) to \(to)")
        }
    }

    /// One row to pick from: a radio dot, a heading and what it holds.
    struct Pick: View {
        let title: String
        let detail: String
        let picked: Bool
        let enabled: Bool
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                HStack(spacing: 12) {
                    Circle()
                        .strokeBorder(picked ? ConfirmationSheetChrome.picked : dotOff, lineWidth: 2)
                        .background(Circle().fill(picked ? ConfirmationSheetChrome.picked : .clear).padding(5))
                        .frame(width: 16, height: 16)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(title)
                            .font(.system(size: 15, weight: .bold))
                            .foregroundStyle(ChromeColours.textBright)
                        Text(detail)
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.textDim)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    Spacer(minLength: 0)
                }
                .padding(.vertical, 10)
                .padding(.horizontal, 12)
                .background(picked ? pickedFill : rowFill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(picked ? ConfirmationSheetChrome.picked : rowEdge, lineWidth: 1))
                .opacity(enabled ? 1 : 0.55)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityAddTraits(picked ? .isSelected : [])
        }
    }

    /// The note under the rows; `lead` sets it brighter, as the board's lead note.
    struct Note: View {
        let text: String
        var lead = false

        var body: some View {
            Text(text)
                .font(.system(size: lead ? 14 : 13))
                .foregroundStyle(lead ? ChromeColours.text : ChromeColours.textDim)
                .lineSpacing(3)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    /// The Core's refusal of an answer, as it sent it.
    struct Refusal: View {
        let text: String

        var body: some View {
            Text(text)
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(refused)
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("confirmationRefused")
        }
    }

    /// The sheet's two buttons: Confirm, which reads `busy` while the Core
    /// answers, and Cancel. After a refusal the Core's words show above one
    /// Close.
    struct Answers: View {
        @ObservedObject var devices: SeveralDevicesClient
        let go: String
        let busy: String
        var cancel = "Cancel"
        var enabled = true
        var choice: Int64 = SeveralDevices.noChoice

        var body: some View {
            VStack(spacing: 8) {
                if case .refused(let reason) = devices.answering {
                    Refusal(text: reason)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    ConnectChrome.WideButton(title: "Close") { devices.closeQuestion() }
                        .accessibilityIdentifier("confirmationClose")
                } else {
                    let proceeding = devices.answering == .proceeding
                    ConnectChrome.WideButton(title: proceeding ? busy : go, look: .go, enabled: enabled,
                                             busy: proceeding) {
                        Task { await devices.proceed(choice: choice) }
                    }
                    .accessibilityIdentifier("confirmationGo")
                    ConnectChrome.WideButton(title: cancel) { devices.cancel() }
                        .disabled(proceeding)
                        .accessibilityIdentifier("confirmationCancel")
                }
            }
        }
    }

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}
