// NereusSDR for iOS: the spots a +N badge hides, as a list whose tap tunes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The spots behind a +N badge (spec section 5.1 item 10, picture 04's
/// second phone), as a sheet from the foot of the band: each callsign in
/// its colour, its frequency in kilohertz and its mode. A tap tunes to it.
struct HiddenSpotsList: View {
    @ObservedObject var spots: SpotsModel
    let hidden: [SpotsModel.Spot]

    /// "3 spots at this frequency".
    static func heading(_ count: Int) -> String {
        count == 1 ? "1 spot at this frequency" : "\(count) spots at this frequency"
    }

    /// "7246.2 kHz".
    static func kilohertz(_ hz: Double) -> String {
        String(format: "%.1f kHz", hz / 1000)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Capsule()
                .fill(ConnectChrome.grab)
                .frame(width: 36, height: 4)
                .frame(maxWidth: .infinity)
                .padding(.bottom, 6)
                .accessibilityHidden(true)
            Text(Self.heading(hidden.count))
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(SpotColours.sheetHead)
                .padding(.horizontal, 2)
                .padding(.top, 4)
                .padding(.bottom, 8)
                .accessibilityAddTraits(.isHeader)
            ScrollView {
                VStack(spacing: 0) {
                    ForEach(hidden) { spot in
                        Button {
                            spots.tune(spot, onBand: true)
                        } label: {
                            HStack(spacing: 12) {
                                Text(spot.call)
                                    .font(.system(size: 16, weight: .bold))
                                    .foregroundStyle(SpotColours.hex(spots.colour(spot)))
                                    .frame(maxWidth: .infinity, alignment: .leading)
                                Text(Self.kilohertz(spot.frequencyHz))
                                    .monospacedDigit()
                                    .foregroundStyle(ChromeColours.text)
                                Text(spot.mode)
                                    .foregroundStyle(ChromeColours.textDim)
                                    .frame(width: 48, alignment: .leading)
                            }
                            .font(.system(size: 14))
                            .padding(.horizontal, 12)
                            .frame(minHeight: 48)
                            .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain)
                        .disabled(spots.tuneReason != nil)
                        .overlay(alignment: .bottom) {
                            Rectangle().fill(SpotColours.rowDivider).frame(height: 1)
                        }
                        .accessibilityLabel("\(spot.call), \(Self.kilohertz(spot.frequencyHz)), \(spot.mode)")
                        .accessibilityHint("Tunes to this spot")
                        .accessibilityIdentifier("hiddenSpot.\(spot.call)")
                    }
                }
            }
            .frame(maxHeight: 48 * 5)
            .fixedSize(horizontal: false, vertical: true)
            if let reason = spots.tuneReason {
                AccessoryChrome.Note(text: reason)
                    .padding(.top, 8)
            }
        }
        .padding(.horizontal, 14)
        .padding(.top, 8)
        .padding(.bottom, 14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(SpotColours.sheet, in: UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14))
        .overlay(alignment: .top) {
            Rectangle().fill(SpotColours.sheetBorder).frame(height: 1).padding(.horizontal, 10)
        }
        .shadow(color: .black.opacity(0.5), radius: 15, y: -10)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("hiddenSpots")
    }
}
