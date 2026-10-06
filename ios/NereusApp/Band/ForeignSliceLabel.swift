// NereusSDR for iOS: another device's slice's label at the foot of the spectrum, and the note it opens
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// Another device's slice's label (D46, spec section 5.8 item 1, picture
/// 22; section 5.9 item 1, picture 23): at the foot of the spectrum, away
/// from this phone's flags, its letter on its colour and its device's short
/// name, TX while that device transmits on it, and greyed with "away" while
/// the device is away. A tap opens a note: whose slice it is, where, and
/// the Core's own words for a change to it ("That slice belongs to MacBook
/// Pro. It can be changed only there."). Nothing here tunes it.
struct ForeignSliceLabel: View {
    let entry: ForeignSlicesModel.Entry
    let open: Bool
    let toggle: () -> Void

    /// The label's height, and how far above the foot of the spectrum it sits (the board's `.pan__fx-tag`).
    static let height: CGFloat = 24
    static let aboveFoot: CGFloat = 30
    /// The note's width (the board's `.pan__fx-pop`).
    static let noteWidth: CGFloat = 210
    static let away = Color(red: 0xDD / 255, green: 0xBB / 255, blue: 0)

    var body: some View {
        Button(action: toggle) {
            HStack(spacing: 6) {
                Text(entry.slice.letter)
                    .font(.system(size: 10, weight: .bold))
                    .foregroundStyle(.white)
                    .frame(width: 16, height: 16)
                    .background(colour, in: RoundedRectangle(cornerRadius: 3))
                    .opacity(entry.slice.away ? 0.55 : 1)
                Text(entry.slice.labelName)
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(entry.slice.away ? ChromeColours.textDim : ChromeColours.text)
                    .lineLimit(1)
                if entry.slice.away {
                    Text("away")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(Self.away)
                }
                if entry.slice.txSlice {
                    TxBadge(lit: true)
                        .scaleEffect(0.85)
                        .frame(width: 24, height: 16)
                }
            }
            .padding(.leading, 3)
            .padding(.trailing, 7)
            .frame(height: Self.height)
            .background(Color(red: 15 / 255, green: 15 / 255, blue: 26 / 255).opacity(0.9),
                        in: RoundedRectangle(cornerRadius: 3))
            .overlay(RoundedRectangle(cornerRadius: 3)
                .strokeBorder(entry.slice.away ? Color(red: 0x60 / 255, green: 0x70 / 255, blue: 0x80 / 255) : colour,
                              style: StrokeStyle(lineWidth: 1, dash: [3, 2])))
            .fixedSize()
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Slice \(entry.slice.letter), \(Self.whose(entry.slice)). Tap for details.")
        .accessibilityAddTraits(open ? .isSelected : [])
        .accessibilityIdentifier("foreignSlice-\(entry.slice.letter)")
    }

    private var colour: Color { BandColours.slice(entry.slice.colour) }

    /// "MacBook Pro\u{2019}s".
    static func possessive(_ name: String) -> String {
        "\(name)\u{2019}s"
    }

    /// Whose the slice is, for the label's spoken name: "MacBook\u{2019}s",
    /// as the label shows it, or, for an owner the Core sent no name for,
    /// "the Core\u{2019}s", "a phone\u{2019}s", "another device\u{2019}s".
    static func whose(_ slice: ForeignSliceMarkers.Slice) -> String {
        possessive(slice.labelName)
    }

    /// The note the label opens.
    struct Note: View {
        let entry: ForeignSlicesModel.Entry

        var body: some View {
            VStack(alignment: .leading, spacing: 3) {
                Text(Self.title(entry))
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                (Text(BandSlice.frequencyText(hz: entry.slice.frequencyHz))
                    .font(.system(size: 12, weight: .bold, design: .monospaced))
                    .foregroundStyle(ConfirmationSheetChrome.frequency)
                    + Text(Self.where_(entry)).font(.system(size: 12)).foregroundStyle(ChromeColours.text))
                Text(Self.fine(entry))
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .padding(.vertical, 9)
            .padding(.horizontal, 11)
            .frame(width: ForeignSliceLabel.noteWidth, alignment: .leading)
            .background(Color(red: 0x0D / 255, green: 0x1B / 255, blue: 0x28 / 255), in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
            .shadow(color: .black.opacity(0.5), radius: 8, y: 6)
            .accessibilityElement(children: .combine)
            .accessibilityIdentifier("foreignSliceNote")
        }

        /// "MacBook Pro\u{2019}s slice B"; for an owner the Core sent no name
        /// for, "The Core\u{2019}s slice A", "A phone\u{2019}s slice B",
        /// "Another device\u{2019}s slice B".
        static func title(_ entry: ForeignSlicesModel.Entry) -> String {
            title(entry.slice)
        }

        static func title(_ slice: ForeignSliceMarkers.Slice) -> String {
            "\(ForeignSliceLabel.possessive(SeveralDevicesWords.sliceOwner(slice, capitalised: true))) slice \(slice.letter)"
        }

        /// " LSB \u{00B7} has transmit": the mode, then what the device is doing on it.
        static func where_(_ entry: ForeignSlicesModel.Entry) -> String {
            let mode = entry.modeLabel.isEmpty ? "" : " " + entry.modeLabel
            return mode + " \u{00B7} " + doing(entry)
        }

        static func doing(_ entry: ForeignSlicesModel.Entry) -> String {
            if entry.slice.away {
                var away = "away"
                if let seconds = entry.awayForSeconds, seconds > 0 {
                    away += " for " + SeveralDevicesWords.duration(seconds: seconds)
                }
                return entry.slice.txSlice ? away + ", holding transmit" : away
            }
            return entry.slice.txSlice ? "has transmit" : "listening"
        }

        /// The Core's own words for a change to another device's slice, as
        /// it refuses one from this app: "That slice belongs to MacBook Pro.
        /// It can be changed only there." (NereusSDR
        /// src/core/session/StationServer.cpp:9085-9093
        /// `ownedElsewhereReason`, at c13abe564), its holder in the Core's
        /// words.
        static func fine(_ entry: ForeignSlicesModel.Entry) -> String {
            fine(entry.slice)
        }

        static func fine(_ slice: ForeignSliceMarkers.Slice) -> String {
            "That slice belongs to \(slice.holderWords). It can be changed only there."
        }
    }
}
