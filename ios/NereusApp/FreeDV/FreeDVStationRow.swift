// NereusSDR for iOS: one station on FreeDV Reporter, the desktop's columns folded into two lines, with its details button
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// One station (spec section 5.7 item 5, picture 21's first phone): the
/// callsign, locator and how long since its news, with its frequency, over
/// what it is doing (transmitting, whom it hears and how well) and its
/// mode, then its message. The row takes its tint's colour. A tap tunes
/// there (D33); the (i) button opens every column and the actions (D78),
/// and press and hold opens them too, as a shortcut.
struct FreeDVStationRow: View {
    @ObservedObject var freedv: FreeDVReporterModel
    let station: FreeDVStation
    let tint: FreeDVReporterModel.Tint?

    var body: some View {
        HStack(spacing: 0) {
            lines
                .padding(.leading, 12)
                .padding(.vertical, 9)
                .contentShape(Rectangle())
                .onTapGesture { freedv.tune(station) }
                .onLongPressGesture { freedv.showDetails(station) }
                .accessibilityElement(children: .combine)
                .accessibilityAddTraits(.isButton)
                .accessibilityHint(freedv.tuneReason(station) ?? "Tunes there")
                .accessibilityAction(named: "Details") { freedv.showDetails(station) }
                .accessibilityIdentifier("freedv.row.\(station.callsign)")
            Button {
                freedv.showDetails(station)
            } label: {
                Image(systemName: "info.circle")
                    .font(.system(size: 18))
                    .foregroundStyle(ChromeColours.accent)
                    .frame(width: 44, height: 44)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Details for \(station.callsign)")
            .accessibilityIdentifier("freedv.details.\(station.callsign)")
        }
        .background(FreeDVColours.colour(tint))
        .animation(.easeOut(duration: 0.6), value: tint)
        .overlay(alignment: .bottom) {
            Rectangle().fill(FreeDVColours.rowDivider).frame(height: 1)
        }
    }

    private var lines: some View {
        let state = FreeDVReporterModel.stateLine(station)
        return VStack(alignment: .leading, spacing: 3) {
            HStack(alignment: .firstTextBaseline, spacing: 7) {
                Text(station.callsign.isEmpty ? "-" : station.callsign)
                    .font(.system(size: 15, weight: .bold))
                    .foregroundStyle(FreeDVColours.callsign)
                    .lineLimit(1)
                Text(details)
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundStyle(ChromeColours.textDim)
                    .lineLimit(1)
                Spacer(minLength: 8)
                Text(freedv.frequencyText(station.frequencyHz))
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(FreeDVColours.frequency)
            }
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                stateText(state)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .lineLimit(1)
                    .truncationMode(.tail)
                Spacer(minLength: 8)
                if !station.txMode.isEmpty {
                    Text(station.txMode)
                        .font(.system(size: 10, design: .monospaced))
                        .foregroundStyle(FreeDVColours.modeText)
                        .padding(.horizontal, 6)
                        .padding(.vertical, 1)
                        .background(FreeDVColours.modeChip, in: RoundedRectangle(cornerRadius: 3))
                }
            }
            if !station.userMessage.isEmpty {
                Text("\u{201C}\(station.userMessage)\u{201D}")
                    .font(.system(size: 12).italic())
                    .foregroundStyle(ChromeColours.text)
                    .lineLimit(1)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    /// "FN42 · now", with distance and heading once both grid squares are known.
    private var details: String {
        var parts: [String] = []
        if !station.gridSquare.isEmpty {
            parts.append(station.gridSquare)
        }
        if let distance = freedv.distanceText(station) {
            parts.append(distance)
        }
        if let heading = FreeDVReporterModel.headingText(station) {
            parts.append(heading)
        }
        parts.append(freedv.age(station.lastUpdate))
        return parts.joined(separator: " \u{00B7} ")
    }

    private func stateText(_ state: (lead: String, callsign: String?, rest: String)) -> Text {
        if state.lead == "Transmitting" {
            return Text(state.lead).fontWeight(.bold).foregroundStyle(ChromeColours.text)
        }
        guard let callsign = state.callsign else {
            return Text(state.lead)
        }
        let name = Text(callsign).fontWeight(.bold).foregroundStyle(ChromeColours.text)
        let rest = state.rest.isEmpty ? "" : " " + state.rest
        return Text("\(state.lead) \(name)\(rest)")
    }
}
