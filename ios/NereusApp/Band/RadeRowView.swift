// NereusSDR for iOS: the RADE row on a slice's flag, drawn as the board draws it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The flag's RADE row (spec section 5.1 item 4, D9, R-IOS-11), under the
/// level bar and above the tabs: one bold 10 point line, 16 points tall,
/// reading the callsign or "RADE", the dot, the signal to noise and the
/// offset, with no extra label. On a Core that does not send RADE
/// sync it greys, reading the callsign or "RADE" and the reason, wrapping.
/// While the Core says the slice has no working RADE decoder the row reads
/// the callsign or "RADE", the hollow dot and "off", and the Core's reason
/// wraps under it, greyed (the desktop's tooltip). With large type the
/// row grows and wraps, the callsign on its own line; the flag keeps its
/// width. It is not a touch surface: a tap on it is a tap on the flag.
struct RadeRowView: View {
    let reception: RadeReception
    /// The row's text size, the desktop's 10 points, grown with the phone's text size.
    @ScaledMetric(relativeTo: .caption2) private var scaledPoints: CGFloat = 10
    /// The redrawn flag's fixed size (10 points, to the flag's cap in
    /// large type): the row stays one 16-point line. Nil grows it with
    /// the phone's text size and lets it wrap.
    var fixedPoints: CGFloat? = nil

    private var points: CGFloat { fixedPoints ?? scaledPoints }

    /// The row's height at the phone's usual text size.
    static let height: CGFloat = 16

    /// Large type: the row wraps instead of staying one line.
    private var large: Bool { fixedPoints == nil && points > 12 }

    var body: some View {
        content
            .font(.system(size: points, weight: .bold))
            .frame(maxWidth: .infinity, alignment: .leading)
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(reception.spokenText)
            .accessibilityIdentifier("flagRadeRow")
    }

    @ViewBuilder
    private var content: some View {
        switch reception.row {
        case .olderCore:
            HStack(alignment: .top, spacing: large ? 6 : 4) {
                Text(reception.prefix)
                    .frame(minHeight: large ? nil : 14, alignment: .top)
                Text(RadeReception.olderCoreReason)
                    .font(.system(size: points, weight: .semibold))
                    .lineSpacing(large ? 2 : 1)
                    .padding(.top, 1)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .foregroundStyle(BandColours.radeOff)
            .padding(.bottom, 1)
        case .off(let prefix, let reason):
            VStack(alignment: .leading, spacing: large ? 2 : 1) {
                HStack(spacing: large ? 6 : 4) {
                    Text(prefix)
                    readings(dot: .hollow, value: RadeReception.offText)
                }
                .foregroundStyle(BandColours.radeRow)
                .lineLimit(1)
                .minimumScaleFactor(0.75)
                .frame(minHeight: large ? nil : 14, alignment: .leading)
                Text(reason)
                    .font(.system(size: points, weight: .semibold))
                    .foregroundStyle(BandColours.radeOff)
                    .lineSpacing(large ? 2 : 1)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .padding(.bottom, 1)
        case .reading(let prefix, let dot, let value, let offset):
            if large {
                VStack(alignment: .leading, spacing: 0) {
                    Text(prefix)
                        .fixedSize(horizontal: false, vertical: true)
                    ViewThatFits(in: .horizontal) {
                        HStack(spacing: 6) {
                            readings(dot: dot, value: value)
                            offsetText(offset)
                        }
                        VStack(alignment: .leading, spacing: 0) {
                            HStack(spacing: 6) {
                                readings(dot: dot, value: value)
                            }
                            offsetText(offset)
                        }
                    }
                }
                .foregroundStyle(BandColours.radeRow)
                .padding(.top, 2)
                .padding(.bottom, 3)
            } else {
                HStack(spacing: 4) {
                    Text(prefix)
                    readings(dot: dot, value: value)
                    offsetText(offset)
                }
                .foregroundStyle(BandColours.radeRow)
                .lineLimit(1)
                .minimumScaleFactor(0.75)
                .frame(height: Self.height)
            }
        }
    }

    @ViewBuilder
    private func readings(dot: RadeReception.Dot, value: String) -> some View {
        Text(dot.character)
            .font(.system(size: large ? points * 0.9 : points, weight: .bold))
            .foregroundStyle(colour(dot))
        Text(value)
    }

    @ViewBuilder
    private func offsetText(_ offset: String?) -> some View {
        if let offset {
            Text(offset)
        }
    }

    private func colour(_ dot: RadeReception.Dot) -> Color {
        switch dot {
        case .good:
            return BandColours.radeGood
        case .marginal:
            return BandColours.radeMarginal
        case .hollow:
            return BandColours.radeHollow
        }
    }
}
