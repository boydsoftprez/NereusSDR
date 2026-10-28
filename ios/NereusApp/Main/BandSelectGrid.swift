// NereusSDR for iOS: the Core's band grid, four across, as the Pan sheet and the band column's BAND menu draw it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The band select grid (R-IOS-27, D73): the Core's catalogue bands in its
/// order, four across, the active slice's band lit, a tap opening that band
/// through the Core (``PanSheetModel/selectBand(_:)``). A Core without bands
/// shows one greyed row; one that cannot open a band for the phone greys
/// the buttons, each with the reason under the grid. The Pan sheet draws
/// it, and so does the band column's BAND menu (R-IOS-58).
struct BandSelectGrid: View {
    @ObservedObject var model: PanSheetModel
    /// After a band button is tapped: the BAND menu closes.
    var selected: () -> Void = {}

    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
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
                            selected()
                        }
                        .accessibilityHint(enabled ? "" : model.bandUnavailableReason)
                    }
                }
                if !enabled {
                    SheetNote(model.bandUnavailableReason)
                }
            }
        }
    }
}
