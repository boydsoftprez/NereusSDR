// NereusSDR for iOS: the Modes tab's slice switch: this phone's slices, the active one lit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The top of the Modes tab (the board's `.mhead` and `.sliceseg`): each of
/// this phone's slices on the band with its letter and frequency; the
/// active one is lit, and a tap on another makes it active, as a tap on its
/// flag does. Everything below it is the active slice's. A slice this
/// phone only listens to is edged dashed in its colour (R-IOS-42).
struct SliceSwitch: View {
    @ObservedObject var model: ModesTabModel

    /// The active slice in the glass, the others raised (W3: a pick-one choice).
    private func look(_ choice: ModesTabModel.SliceChoice) -> WorkspaceLook {
        choice.active ? .glass : .raised
    }

    var body: some View {
        LazyVGrid(columns: Array(repeating: GridItem(.flexible(minimum: 0), spacing: 6), count: 2), spacing: 6) {
            ForEach(model.sliceChoices) { choice in
                Button {
                    model.selectSlice(choice.id)
                } label: {
                    HStack(spacing: 8) {
                        SliceLetterBadge(letter: choice.letter, colour: BandColours.slice(choice.colour))
                        Text(choice.frequencyText)
                            .font(.system(size: 15, design: .monospaced))
                            .lineLimit(1)
                            .minimumScaleFactor(0.7)
                    }
                    .foregroundStyle(workspaceForeground(look(choice)))
                    .frame(maxWidth: .infinity, minHeight: 40)
                    .background { WorkspaceFace(look(choice)) }
                    .overlay {
                        // A slice this phone only listens to: edged dashed in its colour.
                        if choice.listening {
                            RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius, style: .continuous)
                                .strokeBorder(BandColours.slice(choice.colour),
                                              style: StrokeStyle(lineWidth: 1, dash: [4, 3]))
                        }
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Slice \(choice.letter), \(choice.frequencyText)"
                                    + (choice.listening ? ", listening" : ""))
                .accessibilityAddTraits(choice.active ? .isSelected : [])
                .accessibilityIdentifier("modesSlice\(choice.letter)")
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(ChromeColours.modesHead)
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.barBorder).frame(height: 1)
        }
    }
}
