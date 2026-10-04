// NereusSDR for iOS: a long session's note on the band: the first time on cellular, or the month's 5 GB
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The long session's note at the waterfall's top, as picture 12 draws the
/// first time on cellular: an amber dot and the note, its first sentence
/// in bold. It stays a few seconds and goes by itself; it takes no touch.
struct SessionNoteCard: View {
    @ObservedObject var session: SessionController
    let sideways: Bool
    let bandSize: CGSize
    let showsStrip: Bool
    let spectrumShare: CGFloat

    var body: some View {
        let layout = BandLayout(size: bandSize, scale: 1, showsStrip: showsStrip, spectrumShare: spectrumShare)
        Group {
            if let note = session.note {
                HStack(alignment: .top, spacing: 10) {
                    Circle()
                        .fill(ChromeColours.noticeWarn)
                        .frame(width: 10, height: 10)
                        .shadow(color: ChromeColours.noticeWarn.opacity(0.6), radius: 3)
                        .padding(.top, 4)
                    Self.styled(note.text)
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.text)
                        .lineSpacing(2)
                        .fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 0)
                }
                .padding(.vertical, 10)
                .padding(.horizontal, 12)
                .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                .accessibilityElement(children: .combine)
                .accessibilityIdentifier("sessionNote")
                .transition(.opacity)
                .id(note.presentationID)
                .onAppear { session.noteAppeared(note) }
            }
        }
        .frame(maxWidth: sideways ? 546 : .infinity)
        .padding(.horizontal, 10)
        .frame(maxWidth: .infinity)
        .offset(y: layout.waterfall.minY + (sideways ? 14 : 8))
        .allowsHitTesting(false)
        .animation(.easeOut(duration: 0.2), value: session.note)
    }

    /// The note's first sentence in bold, as the board sets it.
    static func styled(_ text: String) -> Text {
        var styled = AttributedString(text)
        let first = text.range(of: ". ").map { String(text[..<$0.lowerBound]) + "." } ?? text
        if let range = styled.range(of: first) {
            styled[range].font = .system(size: 13, weight: .bold)
            styled[range].foregroundColor = ChromeColours.textBright
        }
        return Text(styled)
    }
}
