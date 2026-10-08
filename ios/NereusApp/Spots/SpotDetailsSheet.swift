// NereusSDR for iOS: one spot's details over the band: mode, source, bearing, spotter, comment and time, with Tune and Turn beam
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import SwiftUI

/// A spot's details (spec section 5.1 item 10, picture 04's third phone):
/// what the desktop shows when the pointer rests on a spot. The callsign in
/// its colour and the frequency, then its mode, source, spotter, comment,
/// when it was spotted and what your log says of it, with Tune and Close.
/// The Spot List shows the same details for every spot (D78). With the
/// bearing the Core works out (remote rotor control), the short and long
/// paths, and Turn beam between Tune and Close: one tap turns the rotor to
/// the short path. It stays shown, greyed with its reason, with no rotor or
/// no bearing.
struct SpotDetailsSheet: View {
    @ObservedObject var spots: SpotsModel
    let spot: SpotsModel.Spot
    /// Turn beam was tapped on this spot: the Core's refusal, if any, shows here.
    @State private var beamSent = false

    /// "7.2330 MHz".
    static func megahertz(_ hz: Double) -> String {
        String(format: "%.4f MHz", hz / 1_000_000)
    }

    /// "330°": a bearing as a whole compass degree.
    static func degrees(_ bearing: Double) -> String {
        "\(Int(RotorModel.compass(bearing.rounded())))\u{00B0}"
    }

    /// "330° short · 150° long", or nil when the bearing is not known.
    static func bearingWords(_ spot: SpotsModel.Spot) -> String? {
        guard let short = spot.shortPathDeg, let long = spot.longPathDeg else {
            return nil
        }
        return "\(degrees(short)) short \u{00B7} \(degrees(long)) long"
    }

    /// Turn beam's label: "Turn beam 330°", or "Turn beam" with no bearing.
    static func turnBeamTitle(_ spot: SpotsModel.Spot) -> String {
        spot.shortPathDeg.map { "Turn beam \(degrees($0))" } ?? "Turn beam"
    }

    /// The rows, in order: each name and its value; a row with nothing to say is left out.
    static func rows(_ spot: SpotsModel.Spot) -> [(name: String, value: String)] {
        var rows: [(String, String)] = [("Mode", spot.mode.isEmpty ? "-" : spot.mode), ("Source", spot.source)]
        if let bearing = bearingWords(spot) {
            rows.append(("Bearing", bearing))
        }
        rows.append(("Spotter", spot.spotter.isEmpty ? "-" : spot.spotter))
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
                            .foregroundStyle(row.name == "Log" ? colour
                                             : row.name == "Bearing" ? RotorColours.amber : ChromeColours.text)
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
                TurnBeamButton(rotor: spots.rotor, spot: spot) { beamSent = true }
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
            TurnBeamNote(rotor: spots.rotor, spot: spot, showsRefusal: beamSent)
        }
        .onChange(of: spot.id) { _, _ in beamSent = false }
        .padding(.horizontal, 14)
        .padding(.vertical, 12)
        .background(SpotColours.sheet.opacity(0.97), in: RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(SpotColours.sheetBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.6), radius: 15, y: 12)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("spotDetails")
    }
}

/// Turn beam on a spot's details: one tap turns the rotor to the spot's
/// short path. Greyed, with its reason under the buttons, when the rotor
/// cannot turn or the Core does not know the bearing.
private struct TurnBeamButton: View {
    @ObservedObject var rotor: RotorModel
    let spot: SpotsModel.Spot
    let sent: () -> Void

    var body: some View {
        let reason = rotor.beamReason(bearing: spot.shortPathDeg)
        Button {
            sent()
            rotor.turnBeam(toBearing: spot.shortPathDeg)
        } label: {
            Text(SpotDetailsSheet.turnBeamTitle(spot))
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(reason == nil ? RotorColours.amber : ChromeColours.buttonOffText)
                .lineLimit(1)
                .minimumScaleFactor(0.8)
                .frame(maxWidth: .infinity, minHeight: 40)
                .background(reason == nil ? RotorColours.beam : ChromeColours.buttonOff,
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(reason == nil ? RotorColours.beamBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(reason != nil)
        .accessibilityHint(reason ?? "")
        .accessibilityIdentifier("spotDetailsTurnBeam")
    }
}

/// Why Turn beam is greyed, or, once it was tapped here, the Core's words
/// when it refused the turn.
private struct TurnBeamNote: View {
    @ObservedObject var rotor: RotorModel
    let spot: SpotsModel.Spot
    let showsRefusal: Bool

    var body: some View {
        if let reason = rotor.beamReason(bearing: spot.shortPathDeg) ?? (showsRefusal ? rotor.note : nil) {
            AccessoryChrome.Note(text: reason)
                .accessibilityIdentifier("spotDetailsTurnBeamNote")
        }
    }
}
