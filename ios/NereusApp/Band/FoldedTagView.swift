// NereusSDR for iOS: a slice's flag folded to one line: its letter, its frequency and its TX badge
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-15: the tag of a flag folded by a tap on its letter opens the
// flag again (`opensFlag`), with a VoiceOver action to match. J.J. Boyd
// (KG4VCF), AI-assisted via Claude Code.

import NereusBand
import SwiftUI

/// A folded flag (D9): the flag's letter, frequency and TX badge on one
/// line, used when the full flag would land on another flag, or when a tap
/// on the flag's letter folded it. One tap makes the slice active, and its
/// flag and the active one's swap; on a flag folded by its letter, the tap
/// also opens the flag again.
struct FoldedTagView: View {
    let entry: BandSlicesModel.Entry
    var activate: (() -> Void)?
    var openDiversity: (() -> Void)?
    /// The flag was folded by a tap on its letter: a tap opens it again.
    var opensFlag = false

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
            .accessibilityHint(opensFlag ? "Opens this slice's flag" : "Makes this slice active")
            .accessibilityAddTraits(.isButton)
            .accessibilityActions {
                if opensFlag {
                    Button("Open slice \(entry.slice.letter)'s flag") { activate?() }
                }
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
