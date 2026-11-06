// NereusSDR for iOS: the iPad's Band Stack applet: a title bar with this pan's slices over the stack block
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Band Stack applet (docs/architecture/2026-11-05-band-stack-design.md
/// section 8): the block for the slice of the pan last touched, under a title
/// bar like the S-Meter's whose letters choose the slice. Upright it sits under
/// the S-meter and scrolls on its own; in the column it lies in the column's
/// one scroll. A Core without band stacks leaves the block's greyed row, so
/// the applet stays where it is.
struct BandStackApplet: View {
    @ObservedObject var model: BandStackModel
    /// The applet's width, which the block's narrower rows follow.
    let width: CGFloat
    var scrolls = true
    /// The title's menu button: Setup's Band Stack page.
    var openSetup: () -> Void = {}

    static let id = "bandStackApplet"
    private static let sidePadding: CGFloat = 5
    private static let endPadding: CGFloat = 6
    /// A slice letter's dimmed look while another slice is active (the mockup's).
    private static let otherSliceOpacity = 0.45

    var body: some View {
        VStack(spacing: 0) {
            title
            if scrolls {
                ScrollView {
                    content
                }
                .scrollIndicators(.hidden)
            } else {
                content
            }
        }
        .background(ChromeColours.panel)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier(Self.id)
    }

    private var content: some View {
        BandStackBlock(model: model, width: width - 2 * Self.sidePadding, surface: .applet)
            .padding(.horizontal, Self.sidePadding)
            .padding(.vertical, Self.endPadding)
    }

    private var title: some View {
        HStack(spacing: 0) {
            Text("Band Stack")
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(ChromeColours.icon)
                .accessibilityAddTraits(.isHeader)
            Spacer(minLength: 0)
            ForEach(model.slicesOnBands, id: \.id) { slice in
                sliceLetter(slice)
            }
            menuButton
        }
        .padding(.leading, 8)
        .padding(.trailing, 4)
        .frame(maxWidth: .infinity, minHeight: AnalogSMeter.titleHeight, maxHeight: AnalogSMeter.titleHeight)
        .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                           .init(color: ChromeColours.titleMiddle, location: 0.5),
                                           .init(color: ChromeColours.titleBottom, location: 1)],
                                   startPoint: .top, endPoint: .bottom))
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
        }
    }

    /// The mockup's title bar menu: it leads to the stack's settings.
    private var menuButton: some View {
        Button(action: openSetup) {
            Text("\u{2261}")
                .font(.system(size: 15))
                .foregroundStyle(ChromeColours.icon)
                .frame(width: 30, height: AnalogSMeter.titleHeight)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Band stack settings in Setup")
        .accessibilityIdentifier("bandStackAppletMenu")
    }

    private func sliceLetter(_ slice: BandStackModel.SliceOnBand) -> some View {
        let active = slice.id == model.activeSliceId
        return Button {
            model.activate(sliceId: slice.id)
        } label: {
            SliceLetterBadge(letter: slice.letter, colour: BandColours.slice(slice.colour), size: 15, points: 9)
                .opacity(active ? 1 : Self.otherSliceOpacity)
                .frame(width: 30, height: AnalogSMeter.titleHeight)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Slice \(slice.letter)")
        .accessibilityAddTraits(active ? .isSelected : [])
        .accessibilityIdentifier("bandStackAppletSlice.\(slice.letter)")
    }
}
