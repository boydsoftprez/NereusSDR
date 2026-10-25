// NereusSDR for iOS: one open pan's controls on the iPad: its flags' menus, its Pan sheet, its Display pill and its scale
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand

/// What one open pan's band, column and pill act through (R-IOS-59, DC9,
/// spec section 6.3): its flags' step menu and number pad, the flags'
/// menus on its own slices, its Pan sheet (bands, +RX, notches), its
/// Display pill's model and its band's dB Max and dB Min. Pan 1's are the
/// main screen's own; another pan's are made when it opens and dropped
/// when it closes (``MainScreenModel/controls(for:)``).
@MainActor
final class PanControls {
    let band: PanBand
    let tuning: BandTuningModel
    let flagControls: FlagControls
    let sheet: PanSheetModel
    let display: DisplaySheetModel
    let bandScale: CoreBandScale

    var pan: PanIdentity { band.pan }

    init(band: PanBand, tuning: BandTuningModel, flagControls: FlagControls, sheet: PanSheetModel,
         display: DisplaySheetModel, bandScale: CoreBandScale) {
        self.band = band
        self.tuning = tuning
        self.flagControls = flagControls
        self.sheet = sheet
        self.display = display
        self.bandScale = bandScale
    }
}
