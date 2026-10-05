// NereusSDR for iOS: the Pan 1 sheet: the band grid, a slice or a notch added here, and the extended view
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// The sheet the toolbar's Pan 1 drops over the band, as picture 25 draws
/// it (spec section 5.1 items 14 and 15, D73). Everything it lists is the
/// Core's (``PanSheetModel``).
struct PanSheet: View {
    @ObservedObject var model: PanSheetModel
    /// The active slice's colour, for the title's letter.
    let sliceColour: String?

    var body: some View {
        DropSheet(title: "Pan \(MainScreenModel.panNumber)", accessibilityIdentifier: "panSheet") {
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
            bandGrid
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
            if let note = model.note {
                SheetNote(note)
                    .accessibilityIdentifier("panSheetNote")
            }
        }
    }

    private var letter: String {
        model.sliceLetter ?? "A"
    }

    @ViewBuilder
    private var bandGrid: some View {
        switch model.bandGrid {
        case .needsNewerCore:
            PanelButton(label: CatalogFeed.needsNewerCoreText, lit: false, style: .blue, disabled: true) {}
                .accessibilityIdentifier("bandGridNeedsNewerCore")
            if model.sliceLetter == nil {
                SheetNote(model.bandUnavailableReason)
            }
        case .buttons(let buttons, let enabled):
            SheetGrid(columns: 4) {
                ForEach(buttons) { button in
                    PanelButton(label: button.label, lit: button.lit, style: .blue, disabled: !enabled) {
                        model.selectBand(button.id)
                    }
                    .accessibilityHint(enabled ? "" : model.bandUnavailableReason)
                }
            }
            if !enabled {
                SheetNote(model.bandUnavailableReason)
            }
        }
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
