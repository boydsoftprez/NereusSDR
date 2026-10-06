// NereusSDR for iOS: a slice's flag folded to one line: its letter, its frequency and its TX badge
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// A folded flag (D9): the flag's letter, frequency and TX badge on one
/// line, used when the full flag would land on another flag. One tap makes
/// the slice active, and its flag and the active one's swap.
struct FoldedTagView: View {
    let entry: BandSlicesModel.Entry
    var activate: (() -> Void)?
    var openDiversity: (() -> Void)?

    static func size(for entry: BandSlicesModel.Entry) -> CGSize {
        let normal = FlagLayout.foldedSize(frequencyText: entry.slice.frequencyText)
        return entry.diversityOn ? CGSize(width: normal.width + 44, height: 44) : normal
    }

    var body: some View {
        let colour = BandColours.slice(entry.slice.colour)
        let normal = FlagLayout.foldedSize(frequencyText: entry.slice.frequencyText)
        let size = Self.size(for: entry)
        HStack(spacing: 0) {
            HStack(spacing: 6) {
                SliceLetterBadge(letter: entry.slice.letter, colour: colour)
                Text(entry.slice.frequencyText)
                    .font(.system(size: FlagLayout.foldedFrequencyPoints, weight: .bold, design: .monospaced))
                    .foregroundStyle(BandColours.frequency).lineLimit(1).fixedSize()
                TxBadge(lit: entry.slice.txSlice)
            }
            .frame(width: normal.width, height: size.height)
            .contentShape(Rectangle())
            .onTapGesture { activate?() }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("Slice \(entry.slice.letter), \(entry.slice.frequencyText)")
            .accessibilityHint("Makes this slice active")
            .accessibilityAddTraits(.isButton)
            if entry.diversityOn {
                Button { openDiversity?() } label: {
                    Text("DIV").font(.system(size: 10, weight: .bold)).foregroundStyle(ChromeColours.accent)
                        .frame(width: 44, height: 44).contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Diversity, slice \(entry.slice.letter)")
                .accessibilityIdentifier("flagDiv\(entry.slice.letter)")
            }
        }
        .padding(.top, 1)
        .frame(width: size.width, height: size.height)
        .background(BandColours.flagBackground, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(BandColours.flagBorder, lineWidth: 1))
        .overlay(alignment: .top) { Rectangle().fill(colour).frame(height: 2).padding(.horizontal, 1) }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("foldedFlag\(entry.slice.letter)")
    }
}
