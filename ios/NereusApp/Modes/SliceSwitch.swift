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

    private func border(_ choice: ModesTabModel.SliceChoice) -> Color {
        if choice.listening {
            return BandColours.slice(choice.colour)
        }
        return choice.active ? ChromeColours.accent : ChromeColours.buttonBorder
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
                    .foregroundStyle(choice.active ? ChromeColours.toolbarOpenText : ChromeColours.text)
                    .frame(maxWidth: .infinity, minHeight: 40)
                    .background(choice.active ? ChromeColours.sliceChosen : ChromeColours.button,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(border(choice),
                                      style: StrokeStyle(lineWidth: 1, dash: choice.listening ? [4, 3] : [])))
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
