// NereusSDR for iOS: the active slice's signal on the card, on the Core's S-meter scale, as the flag draws it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The flag's signal bar at card size: the Core's S-unit marks above a
/// bar filled to the signal (blue up to S9, green past it), and the level
/// beside it as the flag prints it (the app's text, in the Multimeter
/// page's units). iOS limits how often the card changes, so the reading
/// steps every few seconds.
struct ActivityLevelBar: View {
    let dbm: Double?
    /// The level as the app printed it; nil prints nothing.
    var text: String?
    /// What VoiceOver says for it.
    var spoken: String?
    let meter: StationActivityAttributes.Meter?

    static let dbmWidth: CGFloat = 58
    static let tickHeight: CGFloat = 9

    var body: some View {
        HStack(spacing: 3) {
            GeometryReader { proxy in
                bar(width: proxy.size.width, height: proxy.size.height)
            }
            Text(dbm == nil ? "" : text ?? "")
                .font(.system(size: 10, weight: .bold))
                .foregroundStyle(high ? ActivityColours.levelHigh : ActivityColours.levelLow)
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .frame(width: Self.dbmWidth, alignment: .leading)
                .padding(.top, Self.tickHeight)
        }
        .frame(height: 26)
        .accessibilityElement()
        .accessibilityLabel("Signal")
        .accessibilityValue(dbm == nil ? "No signal reading" : spoken ?? text ?? "No signal reading")
    }

    private var high: Bool {
        guard let dbm, let meter else {
            return false
        }
        return dbm >= meter.s9Dbm
    }

    private func fraction(_ value: Double) -> Double {
        guard let meter, meter.maxDbm > meter.minDbm else {
            return 0
        }
        return min(max((value - meter.minDbm) / (meter.maxDbm - meter.minDbm), 0), 1)
    }

    private func bar(width: CGFloat, height: CGFloat) -> some View {
        let barHeight = max(2, height - Self.tickHeight)
        let inner = max(0, width - 2)
        return ZStack(alignment: .topLeading) {
            if let meter {
                ForEach(Array(meter.marks.enumerated()), id: \.offset) { index, mark in
                    let x = CGFloat(fraction(mark.dbm)) * (width - 1)
                    Rectangle()
                        .fill(ActivityColours.levelTicks)
                        .frame(width: 1, height: 3)
                        .offset(x: x, y: Self.tickHeight - 3)
                    Text(mark.label)
                        .font(.system(size: 7))
                        .foregroundStyle(ActivityColours.levelTicks)
                        .fixedSize()
                        .position(x: labelX(x, index: index, mark: mark, meter: meter), y: 3)
                }
            }
            Rectangle()
                .fill(ActivityColours.levelBackground)
                .overlay(Rectangle().strokeBorder(ActivityColours.levelEdge, lineWidth: 1))
                .frame(width: width, height: barHeight)
                .offset(y: Self.tickHeight)
            if let dbm, let meter {
                let s9 = fraction(meter.s9Dbm)
                Rectangle()
                    .fill(LinearGradient(stops: [
                        .init(color: ActivityColours.levelLow, location: 0),
                        .init(color: ActivityColours.levelLow, location: s9),
                        .init(color: ActivityColours.levelHigh, location: min(s9 + 0.15, 1)),
                        .init(color: ActivityColours.levelHigh, location: 1),
                    ], startPoint: .leading, endPoint: .trailing))
                    .frame(width: inner, height: max(0, barHeight - 2))
                    .mask(alignment: .leading) {
                        Rectangle().frame(width: CGFloat(fraction(dbm)) * inner)
                    }
                    .offset(x: 1, y: Self.tickHeight + 1)
            }
        }
        .frame(width: width, height: height, alignment: .topLeading)
    }

    /// The first mark hangs right of its tick, marks over S9 left of it, the rest centre on it.
    private func labelX(_ x: CGFloat, index: Int, mark: StationActivityAttributes.Meter.Mark,
                        meter: StationActivityAttributes.Meter) -> CGFloat {
        let half = CGFloat(mark.label.count) * 2
        if index == 0 {
            return x + half
        }
        return mark.dbm > meter.s9Dbm ? x - half : x
    }
}
