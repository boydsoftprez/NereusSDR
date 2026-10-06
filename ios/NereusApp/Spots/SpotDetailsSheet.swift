// NereusSDR for iOS: one spot's details over the band: mode, source, spotter, comment and time, with Tune
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import SwiftUI

/// A spot's details (spec section 5.1 item 10, picture 04's third phone):
/// what the desktop shows when the pointer rests on a spot. The callsign in
/// its colour and the frequency, then its mode, source, spotter, comment,
/// when it was spotted and what your log says of it, with Tune and Close.
/// The Spot List shows the same details for every spot (D78).
struct SpotDetailsSheet: View {
    @ObservedObject var spots: SpotsModel
    let spot: SpotsModel.Spot

    /// "7.2330 MHz".
    static func megahertz(_ hz: Double) -> String {
        String(format: "%.4f MHz", hz / 1_000_000)
    }

    /// The rows, in order: each name and its value; a row with nothing to say is left out.
    static func rows(_ spot: SpotsModel.Spot) -> [(name: String, value: String)] {
        var rows: [(String, String)] = [("Mode", spot.mode.isEmpty ? "-" : spot.mode), ("Source", spot.source),
                                        ("Spotter", spot.spotter.isEmpty ? "-" : spot.spotter)]
        if !spot.comment.isEmpty {
            rows.append(("Comment", spot.comment))
        }
        if let time = spot.time {
            rows.append(("Spotted", "\(SpotsModel.longTime(time)) UTC"))
        }
        if let log = SpotsModel.logWords(spot.dxccPriority) {
            rows.append(("Log", log))
        }
        return rows
    }

    var body: some View {
        let colour = SpotColours.hex(spots.colour(spot))
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .firstTextBaseline, spacing: 10) {
                Text(spot.call)
                    .font(.system(size: 20, weight: .bold))
                    .foregroundStyle(colour)
                    .lineLimit(1)
                Spacer(minLength: 8)
                Text(Self.megahertz(spot.frequencyHz))
                    .font(.system(size: 15, weight: .bold, design: .monospaced))
                    .foregroundStyle(SpotColours.frequency)
            }
            Grid(alignment: .leadingFirstTextBaseline, horizontalSpacing: 10, verticalSpacing: 5) {
                ForEach(Self.rows(spot), id: \.name) { row in
                    GridRow {
                        Text(row.name)
                            .foregroundStyle(ChromeColours.textDim)
                            .frame(width: 74, alignment: .leading)
                        Text(row.value)
                            .fontWeight(.semibold)
                            .foregroundStyle(row.name == "Log" ? colour : ChromeColours.text)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
            }
            .font(.system(size: 13))
            HStack(spacing: 8) {
                let reason = spots.tuneReason
                Button {
                    spots.tune(spot, onBand: true)
                } label: {
                    Text("Tune")
                        .font(.system(size: 13, weight: .bold))
                        .foregroundStyle(reason == nil ? Color.white : ChromeColours.buttonOffText)
                        .frame(maxWidth: .infinity, minHeight: 40)
                        .background(reason == nil ? ChromeColours.buttonOnBlue : ChromeColours.buttonOff,
                                    in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4)
                            .strokeBorder(reason == nil ? ChromeColours.buttonOnBlueBorder : ChromeColours.buttonOffBorder,
                                          lineWidth: 1))
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(reason != nil)
                .accessibilityHint(reason ?? "")
                .accessibilityIdentifier("spotDetailsTune")
                Button {
                    spots.closeBandPopups()
                } label: {
                    Text("Close")
                        .font(.system(size: 13, weight: .bold))
                        .foregroundStyle(ChromeColours.text)
                        .frame(maxWidth: .infinity, minHeight: 40)
                        .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityIdentifier("spotDetailsClose")
            }
            if let reason = spots.tuneReason {
                AccessoryChrome.Note(text: reason)
            }
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 12)
        .background(SpotColours.sheet.opacity(0.97), in: RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(SpotColours.sheetBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.6), radius: 15, y: 12)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("spotDetails")
    }
}
