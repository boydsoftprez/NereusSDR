// NereusSDR for iOS: the Core's spots, newest first, with the source pills and a band filter; a tap tunes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Spot List (spec section 5.7 item 2, picture 20's second phone): the
/// Core's spots, newest first, with the desktop's columns folded into two
/// lines per spot: the callsign in its colour, the frequency in kilohertz,
/// the mode and the source, then the time, the spotter and the comment.
/// The source pills and the band filter (the active slice's band, or all
/// bands) choose which show; a tap tunes the active slice to the spot. It
/// is the same list the desktop shows, and it carries every detail a
/// spot's press and hold shows on the band (D78).
struct SpotListPage: View {
    @ObservedObject var spots: SpotsModel

    static let note = "Tap a spot to tune to it. The list is the Core's, the same one the desktop shows."

    /// "7258.5".
    static func kilohertz(_ hz: Double) -> String {
        String(format: "%.1f", hz / 1000)
    }

    /// The second line: "19:43 UTC · K6ABC · grey line".
    static func detailLine(_ spot: SpotsModel.Spot) -> String {
        var parts: [String] = []
        if let time = spot.time {
            parts.append("\(SpotsModel.shortTime(time)) UTC")
        }
        if !spot.spotter.isEmpty {
            parts.append(spot.spotter)
        }
        if !spot.comment.isEmpty {
            parts.append(spot.comment)
        }
        return parts.joined(separator: " \u{00B7} ")
    }

    var body: some View {
        let band = spots.activeBand
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Pills(isOn: { spots.listSources.contains($0) },
                              enabled: { SpotsModel.pillReason($0) == nil },
                              prefix: "spotList.pill", toggle: spots.toggleListSource)
            HStack(spacing: 5) {
                bandButton(band?.label ?? "This band", lit: spots.listThisBandOnly && band != nil,
                           enabled: band != nil, identifier: "spotList.thisBand") {
                    spots.listThisBandOnly = true
                }
                bandButton("All bands", lit: !spots.listThisBandOnly || band == nil, enabled: true,
                           identifier: "spotList.allBands") {
                    spots.listThisBandOnly = false
                }
            }
            if !spots.available {
                AccessoryChrome.Note(text: spots.connected ? SpotsModel.olderCoreReason : SpotsModel.notConnectedReason)
            }
            let listed = spots.listed
            if listed.isEmpty {
                SpotHubPage.Card {
                    Text(spots.spots.isEmpty ? "No spots" : "No spots match these pills and this band")
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.textDim)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(12)
                }
            } else {
                SpotHubPage.Card {
                    LazyVStack(spacing: 0) {
                        ForEach(Array(listed.enumerated()), id: \.element.id) { index, spot in
                            if index > 0 {
                                SpotHubPage.Line()
                            }
                            row(spot)
                        }
                    }
                }
            }
            if let reason = spots.tuneReason, spots.available {
                AccessoryChrome.Note(text: reason)
            }
            ConnectChrome.Note(text: Self.note).padding(.top, 2)
            ConnectChrome.Note(text: SpotsModel.listensOnEachComputerReason)
        }
    }

    private func row(_ spot: SpotsModel.Spot) -> some View {
        Button {
            spots.tune(spot, onBand: false)
        } label: {
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 10) {
                    Text(spot.call)
                        .font(.system(size: 15, weight: .bold))
                        .foregroundStyle(SpotColours.hex(spots.colour(spot)))
                        .lineLimit(1)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    Text(Self.kilohertz(spot.frequencyHz))
                        .font(.system(size: 13, design: .monospaced))
                        .foregroundStyle(SpotColours.frequency)
                    Text(spot.mode)
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(1)
                        .frame(width: 40, alignment: .leading)
                    Text(SpotsModel.Source.labelled(spot.source)?.pill ?? spot.source)
                        .font(.system(size: 10, design: .monospaced))
                        .foregroundStyle(SpotColours.sourceTag)
                        .padding(.horizontal, 5)
                        .padding(.vertical, 1)
                        .background(SpotColours.sourceTag.opacity(0.1), in: RoundedRectangle(cornerRadius: 3))
                        .frame(width: 44, alignment: .trailing)
                }
                Text(Self.detailLine(spot))
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textFaint)
                    .lineLimit(1)
                    .truncationMode(.tail)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(spots.tuneReason != nil)
        .accessibilityElement(children: .combine)
        .accessibilityHint("Tunes to this spot")
        .accessibilityIdentifier("spotList.\(spot.call)")
    }

    private func bandButton(_ title: String, lit: Bool, enabled: Bool, identifier: String,
                            action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(lit ? Color.white : (enabled ? ChromeColours.text : ChromeColours.buttonOffText))
                .frame(maxWidth: .infinity, minHeight: 32)
                .background(lit ? ChromeColours.buttonOnBlue : (enabled ? ChromeColours.button : ChromeColours.buttonOff),
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(lit ? ChromeColours.buttonOnBlueBorder
                                      : (enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder),
                                  lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityAddTraits(lit ? .isSelected : [])
        .accessibilityIdentifier(identifier)
    }
}
