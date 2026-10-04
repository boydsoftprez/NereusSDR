// NereusSDR for iOS: the pieces the accessory pages share: cards, the OPERATE button, rows and gauges
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The accessory pages' parts, in the board's picture 11 style: dark cards
/// with a thin border, a header card with the device's name, where it is
/// and its OPERATE button, value rows, rows that open a page, and the
/// Core's words under a control.
enum AccessoryChrome {
    /// A card.
    struct Card<Content: View>: View {
        @ViewBuilder var content: () -> Content

        var body: some View {
            VStack(alignment: .leading, spacing: 8) {
                content()
            }
            .padding(10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
        }
    }

    /// A card of rows that open pages, each row edge to edge, with a line between.
    struct Rows<Content: View>: View {
        @ViewBuilder var content: () -> Content

        var body: some View {
            VStack(spacing: 0) {
                content()
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
        }
    }

    /// The line between two rows.
    struct RowDivider: View {
        var body: some View {
            Rectangle().fill(ChromeColours.insetBorder).frame(height: 1)
        }
    }

    /// The page's first card: the device's short name, its status line and
    /// its OPERATE or STANDBY, which shows the device's report.
    struct Header: View {
        let name: String
        let status: String
        var dot: Color?
        let button: String
        let lit: Bool
        let reason: String?
        let action: () -> Void

        var body: some View {
            Card {
                HStack(spacing: 10) {
                    if let dot {
                        Circle().fill(dot).frame(width: 9, height: 9)
                    }
                    VStack(alignment: .leading, spacing: 3) {
                        Text(name)
                            .font(.system(size: 16, weight: .bold, design: .monospaced))
                            .foregroundStyle(ChromeColours.textBright)
                            .lineLimit(1)
                        Text(status)
                            .font(.system(size: 11))
                            .foregroundStyle(ChromeColours.textDim)
                            .fixedSize(horizontal: false, vertical: true)
                            .accessibilityIdentifier("accessoryStatus")
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    OperateButton(label: button, lit: lit, enabled: reason == nil, reason: reason, action: action)
                        .accessibilityIdentifier("accessoryOperate")
                }
                if let reason {
                    Note(text: reason)
                        .accessibilityIdentifier("accessoryOperateReason")
                }
            }
        }
    }

    /// OPERATE (green) or STANDBY (blue), as the device reports it.
    struct OperateButton: View {
        let label: String
        let lit: Bool
        let enabled: Bool
        var reason: String?
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                Text(label)
                    .font(.system(size: 12, weight: .bold))
                    .foregroundStyle(lit ? .white : ChromeColours.text)
                    .frame(minWidth: 96, minHeight: 40)
                    .background(lit ? ChromeColours.operateOn : ChromeColours.operateOff,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(lit ? ChromeColours.operateOnBorder : ChromeColours.operateOffBorder,
                                      lineWidth: 1))
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .opacity(enabled ? 1 : 0.5)
            .accessibilityValue(lit ? "On" : "Off")
            .accessibilityHint(reason ?? "")
        }
    }

    /// A choice among several (an antenna): lit in blue when chosen.
    struct ChoiceButton: View {
        let label: String
        var detail: String?
        let lit: Bool
        let enabled: Bool
        let action: () -> Void

        /// How it is drawn: the Tools pages' choice look, so a greyed one
        /// keeps its chosen choice in grey, never in the blue of one that
        /// can be pressed.
        static func look(lit: Bool, enabled: Bool) -> ToolPageParts.Choices.Look {
            ToolPageParts.Choices.look(chosen: lit, enabled: enabled)
        }

        var body: some View {
            let look = Self.look(lit: lit, enabled: enabled)
            Button(action: action) {
                VStack(spacing: 2) {
                    Text(label)
                        .font(.system(size: 12, weight: .bold))
                    if let detail, !detail.isEmpty {
                        Text(detail)
                            .font(.system(size: 9, weight: .semibold))
                            .lineLimit(1)
                    }
                }
                .foregroundStyle(look.text)
                .frame(maxWidth: .infinity, minHeight: 40)
                .background(look.fill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(look.border, lineWidth: 1))
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityAddTraits(lit ? .isSelected : [])
        }
    }

    /// A button that is greyed while it cannot run, with its reason under it.
    struct GreyedButton: View {
        let label: String

        var body: some View {
            Text(label)
                .font(.system(size: 12, weight: .bold))
                .foregroundStyle(ChromeColours.buttonOffText)
                .frame(maxWidth: .infinity, minHeight: 40)
                .background(ChromeColours.buttonOff, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonOffBorder, lineWidth: 1))
                .accessibilityElement()
                .accessibilityLabel(label)
                .accessibilityValue("Not available")
                .accessibilityAddTraits(.isButton)
        }
    }

    /// A reading: its name at the left, its value at the right.
    struct ValueRow: View {
        let label: String
        let value: String

        var body: some View {
            HStack(alignment: .firstTextBaseline, spacing: 10) {
                Text(label)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                Spacer(minLength: 8)
                Text(value)
                    .font(.system(size: 12, weight: .semibold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .multilineTextAlignment(.trailing)
            }
            .accessibilityElement(children: .combine)
        }
    }

    /// A row that opens a page: its title over what it holds, and a chevron.
    struct PageRow: View {
        let title: String
        let detail: String
        let identifier: String
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                HStack(spacing: 10) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text(title)
                            .font(.system(size: 14, weight: .bold))
                            .foregroundStyle(ChromeColours.textBright)
                        Text(detail)
                            .font(.system(size: 11))
                            .foregroundStyle(ChromeColours.textDim)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    Image(systemName: "chevron.right")
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundStyle(ChromeColours.textFaint)
                }
                .padding(10)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier(identifier)
        }
    }

    /// Small words under a control: why it is greyed, or what it does.
    struct Note: View {
        let text: String

        var body: some View {
            Text(text)
                .font(.system(size: 11))
                .foregroundStyle(ChromeColours.textFaint)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    /// The Core's refusal on a page, in red; a tap clears it.
    struct Refusal: View {
        let text: String
        let dismiss: () -> Void

        var body: some View {
            Button(action: dismiss) {
                HStack(alignment: .top, spacing: 8) {
                    Circle().fill(ChromeColours.gaugeRed).frame(width: 8, height: 8).padding(.top, 4)
                    Text(text)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.refusalText)
                        .fixedSize(horizontal: false, vertical: true)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    Image(systemName: "xmark")
                        .font(.system(size: 11, weight: .bold))
                        .foregroundStyle(ChromeColours.textFaint)
                }
                .padding(10)
                .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.gaugeRed.opacity(0.5),
                                                                        lineWidth: 1))
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel(text)
            .accessibilityHint("Clears this message")
            .accessibilityIdentifier("accessoryRefusal")
        }
    }

    /// The Core's warning while it lasts, in red: the Power Genius over its
    /// output limit. It goes when the Core says it is over.
    struct Alert: View {
        let text: String

        var body: some View {
            HStack(alignment: .top, spacing: 8) {
                Circle().fill(ChromeColours.gaugeRed).frame(width: 8, height: 8).padding(.top, 4)
                Text(text)
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(ChromeColours.refusalText)
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(10)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.gaugeRed.opacity(0.5), lineWidth: 1))
            .accessibilityElement(children: .combine)
        }
    }

    /// A small caption over a group, as the board's "Antenna:".
    struct Caption: View {
        let text: String

        var body: some View {
            Text(text)
                .font(.system(size: 11))
                .foregroundStyle(ChromeColours.caption)
        }
    }

    /// One of the Tuner Genius's matching relays, 0 to 255, as a bar with its value.
    struct RelayBar: View {
        let label: String
        let value: Int64?

        var body: some View {
            HStack(spacing: 8) {
                Text(label)
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .frame(width: 22, alignment: .leading)
                GeometryReader { proxy in
                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 2).fill(ChromeColours.gaugeGround)
                        if let value {
                            RoundedRectangle(cornerRadius: 2)
                                .fill(ChromeColours.gaugeNormal)
                                .frame(width: proxy.size.width * CGFloat(min(max(value, 0), 255)) / 255)
                        }
                        RoundedRectangle(cornerRadius: 2).strokeBorder(ChromeColours.gaugeBorder, lineWidth: 1)
                    }
                }
                .frame(height: 13)
                Text(value.map(String.init) ?? "--")
                    .font(.system(size: 11, weight: .semibold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .frame(width: 30, alignment: .trailing)
            }
            .accessibilityElement()
            .accessibilityLabel("Relay \(label)")
            .accessibilityValue(value.map(String.init) ?? "--")
        }
    }
}

extension LinearGauge.Scale {
    // The amplifiers' gauges, on the board's picture 11 scales.

    /// Forward power, 0 to 2 kW, yellow from 1 kW and red from 1.5 kW.
    static let ampPower = LinearGauge.Scale(title: "Fwd Pwr", min: 0, max: 2000, yellowFrom: 1000, redFrom: 1500,
                                            ticks: [(0, "0"), (500, "500"), (1000, "1000"), (1500, "1.5k"),
                                                    (2000, "2k")])
    /// SWR, 1 to 3, yellow from 2 and red from 2.5.
    static let ampSwr = LinearGauge.Scale(title: "SWR", min: 1, max: 3, yellowFrom: 2, redFrom: 2.5,
                                          ticks: [(1, "1"), (1.5, "1.5"), (2, "2"), (2.5, "2.5"), (3, "3")])
    /// Temperature, 0 to 100 C, yellow from 55 and red from 80.
    static let ampTemperature = LinearGauge.Scale(title: "Temp", min: 0, max: 100, yellowFrom: 55, redFrom: 80,
                                                  ticks: [(0, "0"), (30, "30"), (55, "55"), (80, "80"),
                                                          (100, "100")])
}
