// NereusSDR for iOS: the Pan 1 sheet: the band grid, a slice or a notch added here, and the extended view
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// The sheet the toolbar's Pan 1 drops over the band, as picture 25 draws
/// it (spec section 5.1 items 14 and 15, D73). Everything it lists is the
/// Core's (``PanSheetModel``). On the iPad it is the sheet of the pan last
/// touched, with Add a pan and Close this pan under Pans (R-IOS-59, DC9,
/// spec section 6.3); on a phone those two are greyed with their reason.
struct PanSheet: View {
    @ObservedObject var model: PanSheetModel
    /// The active slice's colour, for the title's letter.
    let sliceColour: String?
    /// The pan this sheet is for.
    var pan: PanIdentity = .first
    /// The pans, for Add a pan and Close this pan; nil leaves them out (tests of the sheet alone).
    var stack: PanStackModel?
    /// This pan's slices, for Close this pan's note.
    var slices: BandSlicesModel?

    var body: some View {
        DropSheet(title: pan.title, accessibilityIdentifier: "panSheet") {
            HStack(spacing: 5) {
                Text("Slice")
                    .font(.system(size: 11, weight: .semibold))
                    .tracking(0.44)
                    .foregroundStyle(ChromeColours.textDim)
                if let letter = model.sliceLetter {
                    SliceLetterBadge(letter: letter, colour: BandColours.slice(sliceColour ?? BandSlice.colourUnknown))
                        .scaleEffect(15.0 / 18.0)
                        .frame(width: 15, height: 15)
                }
            }
        } content: {
            SheetCaption("Band:")
            BandSelectGrid(model: model)
            if model.sliceLetter != nil {
                SheetNote("A band opens where slice \(letter) last was on it: its frequency, mode and filter.")
            }
            SheetGrid(columns: 2) {
                PanelButton(label: "Add a slice here", lit: false, style: .blue, disabled: !model.canAddSlice) {
                    model.addSlice()
                }
                .accessibilityHint(model.canAddSlice ? "" : model.addSliceUnavailableReason)
                PanelButton(label: model.sliceLetter == nil ? "Add a notch" : "Add a notch at \(letter)",
                            lit: false, style: .blue, disabled: !model.canAddNotch) {
                    model.addNotch()
                }
                .accessibilityHint(model.canAddNotch ? "" : model.notchUnavailableReason)
            }
            if !model.canAddSlice {
                SheetNote("Add a slice here: \(model.addSliceUnavailableReason)")
            }
            if !model.canAddNotch {
                SheetNote("Add a notch: \(model.notchUnavailableReason)")
            }
            extendedView
            if let stack, let slices {
                PanSheetPans(stack: stack, slices: slices, pan: pan)
            }
            if let note = model.note {
                SheetNote(note)
                    .accessibilityIdentifier("panSheetNote")
            }
        }
    }

    private var letter: String {
        model.sliceLetter ?? "A"
    }

    private var extendedView: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Extended view")
                        .font(.system(size: 13, weight: .bold))
                        .foregroundStyle(ChromeColours.text)
                    Text("Show the radio's full width either side of the band.")
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Spacer(minLength: 0)
                PanelButton(label: model.extendedViewOn ? "On" : "Off", lit: model.extendedViewOn, style: .blue,
                            disabled: !model.extendedViewAvailable) {
                    model.toggleExtendedView()
                }
                .frame(width: 88)
                .accessibilityLabel("Extended view")
                .accessibilityValue(model.extendedViewOn ? "On" : "Off")
                .accessibilityHint(model.extendedViewAvailable ? "" : CatalogFeed.needsNewerCoreText)
            }
            if !model.extendedViewAvailable {
                SheetNote("Extended view: \(CatalogFeed.needsNewerCoreText)")
            }
        }
        .padding(.vertical, 6)
    }
}

/// The Pan sheet's Pans row (spec section 6.3, the board's "Two pans, two
/// pills"): Add a pan and Close this pan, each greyed with its reason, the
/// board's notes under them, and what the last of them did.
private struct PanSheetPans: View {
    @ObservedObject var stack: PanStackModel
    /// The slices on this pan, for Close this pan's note.
    @ObservedObject var slices: BandSlicesModel
    let pan: PanIdentity

    var body: some View {
        let add = stack.addPanState()
        let close = stack.closePanState(pan)
        SheetCaption("Pans:")
        SheetGrid(columns: 2) {
            PanelButton(label: "Add a pan", lit: false, style: .blue, disabled: !add.enabled) {
                stack.addPan()
            }
            .accessibilityHint(add.reason ?? "")
            .accessibilityIdentifier("panSheet.addPan")
            PanelButton(label: "Close this pan", lit: false, style: .blue, disabled: !close.enabled) {
                stack.closePan(pan)
            }
            .accessibilityHint(close.reason ?? "")
            .accessibilityIdentifier("panSheet.closePan")
        }
        SheetNote(notes(add: add, close: close))
            .accessibilityIdentifier("panSheetPansNote")
        if let status = stack.status {
            SheetNote(status)
                .accessibilityIdentifier("panSheetPanStatus")
        }
    }

    /// The board's words: why Add a pan is greyed or what it does, then
    /// why Close this pan is greyed or what it does.
    private func notes(add: (enabled: Bool, reason: String?), close: (enabled: Bool, reason: String?)) -> String {
        var words: [String] = []
        if let reason = add.reason {
            words.append(reason)
        } else if add.enabled {
            let last = stack.open.max() ?? .first
            words.append(stack.open.count > 1
                ? "Add a pan opens Pan \(last.number + 1) below \(last.title), with a spare slice moved onto it."
                : "Add a pan opens Pan 2 below this one, with a spare slice moved onto it.")
        }
        if let reason = close.reason {
            words.append(reason)
        } else if close.enabled {
            let letters = slices.entries.filter { $0.panKey == pan.corePanKey && !$0.listening }
                .map { BandSlice.letter(forIndex: $0.id) }
            let what = letters.count == 1 ? "slice \(letters[0])" : "its slices"
            words.append("Close this pan moves \(what) to Pan 1 and keeps this pan's own values for next time.")
        }
        return words.joined(separator: " ")
    }
}
