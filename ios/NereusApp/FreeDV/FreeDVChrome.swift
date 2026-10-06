// NereusSDR for iOS: the parts FreeDV Reporter's sheets share: the sheet over the list and its wide buttons
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The parts the FreeDV Reporter page's sheets share (the board's
/// `.dsheet`, `.wide` and `.wide--go`).
enum FreeDVChrome {
    /// A sheet up from the bottom of the page, over a dimmed list.
    struct Sheet<Content: View>: View {
        let identifier: String
        @ViewBuilder var content: () -> Content

        var body: some View {
            VStack(spacing: 0) {
                Spacer(minLength: 0)
                VStack(alignment: .leading, spacing: 10) {
                    Capsule().fill(ChromeColours.textFaint).frame(width: 36, height: 4)
                        .frame(maxWidth: .infinity)
                    content()
                }
                .padding(.horizontal, 14)
                .padding(.top, 8)
                .padding(.bottom, 14)
                .background(ChromeColours.sheet, in: UnevenRoundedRectangle(topLeadingRadius: 12,
                                                                            topTrailingRadius: 12))
                .overlay(UnevenRoundedRectangle(topLeadingRadius: 12, topTrailingRadius: 12)
                    .strokeBorder(ChromeColours.sheetBorder, lineWidth: 1))
                .accessibilityElement(children: .contain)
                .accessibilityIdentifier(identifier)
            }
            .background(Color.black.opacity(0.45))
        }
    }

    /// A full-width button: blue for the main action, grey for the rest,
    /// greyed while it cannot act.
    struct Wide: View {
        let title: String
        var primary = false
        var enabled = true
        let identifier: String
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                Text(title)
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(!enabled ? ChromeColours.buttonOffText : (primary ? Color.white : ChromeColours.text))
                    .multilineTextAlignment(.center)
                    .frame(maxWidth: .infinity, minHeight: 42)
                    .background(!enabled ? ChromeColours.buttonOff
                                : (primary ? ChromeColours.buttonOnBlue : ChromeColours.button),
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(!enabled ? ChromeColours.buttonOffBorder
                                      : (primary ? ChromeColours.buttonOnBlueBorder : ChromeColours.buttonBorder),
                                      lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityIdentifier(identifier)
        }
    }

    /// A choice among a few, blue while chosen (the board's chips and `.fdseg`).
    struct Choice: View {
        let title: String
        let chosen: Bool
        var enabled = true
        let identifier: String
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                Text(title)
                    .font(.system(size: 11, weight: .bold))
                    .foregroundStyle(!enabled ? ChromeColours.buttonOffText : (chosen ? Color.white : ChromeColours.text))
                    .lineLimit(1)
                    .padding(.horizontal, 10)
                    .frame(minHeight: 30)
                    .frame(maxWidth: .infinity)
                    .background(!enabled ? ChromeColours.buttonOff
                                : (chosen ? ChromeColours.buttonOnBlue : ChromeColours.button),
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(!enabled ? ChromeColours.buttonOffBorder
                                      : (chosen ? ChromeColours.buttonOnBlueBorder : ChromeColours.buttonBorder),
                                      lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityAddTraits(chosen ? .isSelected : [])
            .accessibilityIdentifier(identifier)
        }
    }
}
