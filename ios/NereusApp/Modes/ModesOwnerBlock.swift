// NereusSDR for iOS: the Modes tab's owner block on a slice this phone only listens to
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// Under the Modes tab's slice switch while the active slice is one this
/// phone only listens to (R-IOS-42, the board's `.sll-mblock`): in a box
/// edged dashed in the slice's colour, the Core's full line for who
/// controls it, Stop listening and Take control (greyed only while the
/// slice is on the air, with the Core's words), and this phone's own
/// volume and mute. The sections below are greyed; a tap on one rings
/// this block.
struct ModesOwnerBlock: View {
    @ObservedObject var slices: BandSlicesModel
    let entry: BandSlicesModel.Entry
    /// A greyed control was tapped: the block is ringed for a moment.
    let nudged: Bool

    @Environment(\.dynamicTypeSize) private var typeSize
    @State private var stopping = false

    var body: some View {
        let large = typeSize.isAccessibilitySize
        let colour = BandColours.slice(entry.slice.colour)
        let letter = entry.slice.letter
        let taking = slices.taking.contains(entry.id)
        VStack(alignment: .leading, spacing: 0) {
            Text(entry.ownerLine ?? "")
                .font(.system(size: large ? 19 : 13))
                .foregroundStyle(ChromeColours.textBright)
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("modesOwnerLine")
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(minimum: 0), spacing: 8), count: large ? 1 : 2),
                      spacing: 8) {
                SliceListButton(title: SliceListModel.stopListeningTitle, kind: stopping ? .greyed : .plain,
                                large: large) {
                    stop()
                }
                .accessibilityIdentifier("modesStopListening")
                SliceListButton(title: taking ? VfoFlagView.takingControlTitle : VfoFlagView.takeControlTitle,
                                kind: taking || entry.takeRefusal != nil ? .greyed : .go, large: large) {
                    if let why = entry.takeRefusal {
                        slices.showReason(why)
                    } else if !taking {
                        slices.takeControl(entry.id)
                    }
                }
                .accessibilityLabel(taking ? VfoFlagView.takingControlTitle : "Take control of slice \(letter)")
                .accessibilityHint(entry.takeRefusal ?? "")
                .accessibilityIdentifier("modesTakeControl")
            }
            .padding(.top, 8)
            if let why = entry.takeRefusal, !taking {
                Text(why)
                    .font(.system(size: large ? 19 : 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.top, 6)
                    .accessibilityIdentifier("modesTakeRefusal")
            }
            YourVolumeRow(letter: letter, colour: colour, level: slices.listenLevel(entry.id), large: large,
                          setLevel: { slices.setListenLevel(entry.id, level: $0, muted: $1) }, place: "modes")
        }
        .padding(.horizontal, 10)
        .padding(.top, 8)
        .padding(.bottom, 10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(SliceListStyle.modesBlock, in: RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(colour, style: StrokeStyle(lineWidth: 1, dash: [4, 3])))
        .overlay {
            if nudged {
                RoundedRectangle(cornerRadius: 6).strokeBorder(SliceListStyle.nudge, lineWidth: 3).padding(-3)
            }
        }
        .animation(.easeOut(duration: 0.2), value: nudged)
        .padding(.horizontal, 12)
        .padding(.bottom, 8)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Who controls this slice")
        .accessibilityIdentifier("modesOwnerBlock")
    }

    private func stop() {
        guard !stopping else {
            return
        }
        stopping = true
        let slices = slices
        let id = entry.id
        Task { @MainActor in
            if let words = await slices.stopListening(id) {
                slices.showReason(words)
            }
            stopping = false
        }
    }
}
