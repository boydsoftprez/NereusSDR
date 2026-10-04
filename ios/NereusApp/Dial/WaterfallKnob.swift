// NereusSDR for iOS: the knob on the waterfall, opposite PTT
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The knob on the waterfall (D12, spec section 5.1 item 8, picture 3):
/// always on screen at the bottom right of the waterfall, opposite PTT, so
/// one thumb keys and the other tunes; zoom moves up above it. Sideways it
/// keeps its upright size (smaller if the waterfall is too short for it),
/// under the right thumb inside the phone's rounded edge, with zoom to its
/// left.
/// A tap on its middle opens the step menu.
struct WaterfallKnob: View {
    @ObservedObject var tuning: BandTuningModel
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var settings: PhoneSettings
    let metrics: Knob.Metrics

    /// The board's knob on the waterfall, the same size sideways, no taller
    /// than the waterfall leaves room for (half the size at the least).
    static func metrics(sideways: Bool, waterfallHeight: CGFloat) -> Knob.Metrics {
        guard sideways else {
            return Knob.Metrics.waterfall
        }
        let full = Knob.Metrics.waterfall.diameter
        let fits = (waterfallHeight - DialLayer.knobBottomInset * 2) / full
        return Knob.Metrics.waterfall.scaled(max(0.5, min(1, fits)))
    }

    var body: some View {
        Knob(tuning: tuning, slices: slices, settings: settings, metrics: metrics)
    }
}
