// NereusSDR for iOS: the spots on the band: callsigns to tap, +N badges, and what they open
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The Core's spots on the band (R-IOS-25, D13, spec section 5.1 item 10,
/// picture 04), placed by ``SpotLayout`` under the flags. Each callsign
/// is drawn in its colour (the Display page's spot colour while colours
/// are overridden, else the Core's DXCC colour, else its source's) on the
/// Display page's background (black at 48 percent unless changed; none
/// while it is off), with a dotted tick down to the foot of the spectrum. A tap
/// tunes the active slice to it; press and hold shows its details, which
/// the Spot List also shows (D78). A +N badge's tap lists the spots it
/// hides. The flags draw over the spots.
struct SpotLayer: View {
    @ObservedObject var spots: SpotsModel
    let geometry: BandGeometry
    let placements: [FlagPlacement]

    /// How long a press is held before it shows the spot's details.
    static let holdSeconds = 0.45

    var body: some View {
        let result = SpotLayout.layout(spots: spots.bandSpots, flags: placements, geometry: geometry,
                                       settings: spots.display)
        let settings = spots.display
        ZStack(alignment: .topLeading) {
            ForEach(result.labels, id: \.spotId) { label in
                if let spot = spots.spot(id: label.spotId) {
                    let colour = SpotColours.hex(spots.colour(spot))
                    tick(x: label.lineX, from: label.rect.maxY, colour: colour)
                    SpotLabel(spot: spot, colour: colour, settings: settings, size: label.rect.size,
                              tune: { spots.tune(spot, onBand: true) }, details: { spots.showDetails(spot) })
                        .offset(x: label.rect.minX, y: label.rect.minY)
                }
            }
            ForEach(result.badges, id: \.spotIds) { badge in
                Button {
                    spots.showHidden(badge.spotIds)
                } label: {
                    Text(badge.text)
                        .font(.system(size: CGFloat(settings.fontSize) - SpotLayout.badgeFontReduction, weight: .bold))
                        .foregroundStyle(SpotColours.badgeText)
                        .frame(width: badge.rect.width, height: badge.rect.height)
                        .background(SpotColours.badge, in: RoundedRectangle(cornerRadius: 3))
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .offset(x: badge.rect.minX, y: badge.rect.minY)
                .accessibilityLabel("\(badge.spotIds.count) more spots here")
                .accessibilityHint("Lists them")
                .accessibilityIdentifier("spotBadge")
            }
        }
        .frame(width: geometry.size.width, height: geometry.size.height, alignment: .topLeading)
    }

    /// The dotted line from under the label to the foot of the spectrum.
    private func tick(x: CGFloat, from top: CGFloat, colour: Color) -> some View {
        let length = max(0, geometry.size.height - top)
        return Path { path in
            path.move(to: CGPoint(x: 0.5, y: 0))
            path.addLine(to: CGPoint(x: 0.5, y: length))
        }
        .stroke(colour.opacity(SpotColours.tickOpacity), style: StrokeStyle(lineWidth: 1, dash: [1, 2]))
        .frame(width: 1, height: length)
        .offset(x: x, y: top)
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }
}

/// One callsign on the band: a tap tunes, press and hold shows its details.
private struct SpotLabel: View {
    let spot: SpotsModel.Spot
    let colour: Color
    let settings: SpotDisplaySettings
    let size: CGSize
    let tune: () -> Void
    let details: () -> Void

    var body: some View {
        Text(spot.call)
            .font(.system(size: CGFloat(settings.fontSize), weight: .bold))
            .lineLimit(1)
            .fixedSize()
            .foregroundStyle(colour)
            .frame(width: size.width, height: size.height)
            .background(SpotColours.background(settings), in: RoundedRectangle(cornerRadius: 3))
            .contentShape(Rectangle())
            .gesture(LongPressGesture(minimumDuration: SpotLayer.holdSeconds)
                .exclusively(before: TapGesture())
                .onEnded { value in
                    switch value {
                    case .first:
                        details()
                    case .second:
                        tune()
                    }
                })
            .accessibilityElement()
            .accessibilityLabel(spot.call)
            .accessibilityValue(SpotDetailsSheet.megahertz(spot.frequencyHz))
            .accessibilityAddTraits(.isButton)
            .accessibilityHint("Tunes to this spot")
            .accessibilityAction(.default, tune)
            .accessibilityAction(named: "Details", details)
            .accessibilityIdentifier("spot.\(spot.call)")
    }
}

extension SpotLayer {
    /// What a spot or a badge opens over the band: the spot's details, or
    /// the hidden spots' list over a dimmed band whose tap closes it.
    struct Popups: View {
        @ObservedObject var spots: SpotsModel
        let sideways: Bool

        var body: some View {
            if let hidden = spots.openHidden {
                ZStack(alignment: .bottom) {
                    SpotColours.dim
                        .contentShape(Rectangle())
                        .onTapGesture { spots.closeBandPopups() }
                        .accessibilityLabel("Close the list")
                        .accessibilityAddTraits(.isButton)
                    HiddenSpotsList(spots: spots, hidden: hidden)
                }
            } else if let spot = spots.openDetails {
                ZStack(alignment: sideways ? .topTrailing : .bottom) {
                    Color.black.opacity(0.001)
                        .contentShape(Rectangle())
                        .onTapGesture { spots.closeBandPopups() }
                        .accessibilityHidden(true)
                    SpotDetailsSheet(spots: spots, spot: spot)
                        .frame(maxWidth: sideways ? 330 : .infinity)
                        .padding(.horizontal, sideways ? 0 : 14)
                        .padding(.trailing, sideways ? 72 : 0)
                        .padding(.top, sideways ? 12 : 0)
                        .padding(.bottom, sideways ? 0 : 86)
                }
            }
        }
    }
}
