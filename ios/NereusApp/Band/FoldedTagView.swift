// NereusSDR for iOS: a slice's flag folded to one line: its letter, its frequency and its TX badge
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-17: PR 808 part 2, PAST and its time at the tag's end while the
//   pan shows the past (DsrFlagPastChip). J.J. Boyd (KG4VCF), AI-assisted
//   via Claude Code.

import NereusBand
import SwiftUI

/// A folded flag (D9): the flag's letter, frequency and TX badge on one
/// line, used when the full flag would land on another flag. One tap makes
/// the slice active, and its flag and the active one's swap.
struct FoldedTagView: View {
    let entry: BandSlicesModel.Entry
    var activate: (() -> Void)?
    var openDiversity: (() -> Void)?
    /// The pan's recorder: while it shows the past, PAST and its time end the tag (PR 808).
    var past: DsrPanModel?

    /// The tag's size; `past` adds the PAST chip at its end.
    static func size(for entry: BandSlicesModel.Entry, past: Bool = false) -> CGSize {
        var normal = FlagLayout.foldedSize(frequencyText: entry.slice.frequencyText)
        if past {
            normal.width += pastWidth
        }
        return entry.diversityOn ? CGSize(width: normal.width + 44, height: 44) : normal
    }

    /// What PAST adds to the tag: the chip, its gap before and after.
    static let pastWidth: CGFloat = DsrFlagPastChip.size.width + DsrFlagPastChip.foldedGap

    var body: some View {
        let colour = BandColours.slice(entry.slice.colour)
        let normal = FlagLayout.foldedSize(frequencyText: entry.slice.frequencyText)
        let pastShows = past?.inPast == true
        let size = Self.size(for: entry, past: pastShows)
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
            if pastShows, let past {
                DsrFlagPastChip(model: past)
                    .padding(.trailing, DsrFlagPastChip.foldedGap)
            }
            if entry.diversityOn {
                Button { openDiversity?() } label: {
                    Text("DIV").font(.system(size: 10, weight: .bold)).foregroundStyle(ChromeColours.accentText)
                        .frame(width: 44, height: 44).contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Diversity, slice \(entry.slice.letter)")
                .accessibilityIdentifier("flagDiv\(entry.slice.letter)")
            }
        }
        .padding(.top, 1)
        .frame(width: size.width, height: size.height)
        .background(BandColours.flagBackground, in: RoundedRectangle(cornerRadius: 4)) // kept-radius: flag
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(BandColours.flagBorder, lineWidth: 1)) // kept-radius: flag
        .overlay(alignment: .top) { Rectangle().fill(colour).frame(height: 2).padding(.horizontal, 1) }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("foldedFlag\(entry.slice.letter)")
    }
}
