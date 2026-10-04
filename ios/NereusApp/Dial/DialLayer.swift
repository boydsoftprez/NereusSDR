// NereusSDR for iOS: where the chosen tuning dial sits on the band, and its step menu
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The tuning dial Setup, General, Navigation picked for this phone (D12),
/// over the band, sized to it: nothing while the setting is Off (a new
/// install's); the knob at the bottom right of the waterfall; the
/// thumbwheel along its bottom, clear of PTT at the left (sideways at the
/// right, as wide as it is upright); or, with the knob
/// in a sheet, the sheet while it is up. Zoom moves up above the knob or
/// the wheel (``zoomOrigin(kind:waterfall:sideways:)``). The step menu the
/// dial's middle opens sits just above the knob or the wheel, over a clear
/// layer whose tap closes it.
struct DialLayer: View {
    @ObservedObject var tuning: BandTuningModel
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var settings: PhoneSettings
    let layout: BandLayout
    let sideways: Bool
    /// Sideways, the phone's rounded edge on the right, which the knob and
    /// the wheel keep inside.
    var trailingInset: CGFloat = 0
    @Environment(\.uprightBandWidth) private var uprightWidth

    // The board's places (`.knob--wf`, `.twheel`, `.has-knob .zoom`,
    // `.has-twheel .zoom`, `.scr--land`), from the waterfall's edges.
    static let rightInset: CGFloat = 12
    static let knobBottomInset: CGFloat = 9
    static let knobZoomGap: CGFloat = 10
    static let sidewaysZoomGap: CGFloat = 15
    static let wheelLeftInset: CGFloat = 100
    static let wheelBottomInset: CGFloat = 8
    static let wheelZoomGap: CGFloat = 9
    /// The step menu's gap above the knob or the wheel.
    static let menuGap: CGFloat = 8

    var body: some View {
        switch settings.dialKind {
        case .off:
            EmptyView()
        case .waterfallKnob:
            let frame = Self.knobFrame(waterfall: layout.waterfall, sideways: sideways, trailingInset: trailingInset)
            ZStack(alignment: .topLeading) {
                WaterfallKnob(tuning: tuning, slices: slices, settings: settings,
                              metrics: WaterfallKnob.metrics(sideways: sideways,
                                                             waterfallHeight: layout.waterfall.height))
                    .offset(x: frame.minX, y: frame.minY)
                menu(above: frame)
            }
            .frame(width: layout.size.width, height: layout.size.height, alignment: .topLeading)
        case .thumbwheel:
            let frame = Self.wheelFrame(waterfall: layout.waterfall, sideways: sideways, uprightWidth: uprightWidth,
                                        trailingInset: trailingInset)
            ZStack(alignment: .topLeading) {
                Thumbwheel(tuning: tuning, slices: slices, settings: settings)
                    .frame(width: frame.width, height: frame.height)
                    .offset(x: frame.minX, y: frame.minY)
                menu(above: frame)
            }
            .frame(width: layout.size.width, height: layout.size.height, alignment: .topLeading)
        case .sheetKnob:
            if tuning.dialSheetOpen {
                ZStack(alignment: .bottom) {
                    Color.black.opacity(0.001)
                        .contentShape(Rectangle())
                        .onTapGesture { tuning.closeDialSheet() }
                        .accessibilityLabel("Close the tuning dial")
                        .accessibilityAddTraits(.isButton)
                    SheetKnob(tuning: tuning, slices: slices, settings: settings,
                              metrics: Self.sheetMetrics(bandHeight: layout.size.height))
                        .frame(maxWidth: sideways ? 402 : .infinity)
                        .frame(maxWidth: .infinity, alignment: .trailing)
                }
                .frame(width: layout.size.width, height: layout.size.height)
            }
        }
    }

    /// The step menu from the dial's middle, its right edge on the dial's
    /// and its foot just above it.
    @ViewBuilder
    private func menu(above dial: CGRect) -> some View {
        if tuning.stepMenuFromDial, let id = tuning.stepMenuSliceId {
            Color.black.opacity(0.001)
                .contentShape(Rectangle())
                .frame(width: layout.size.width, height: layout.size.height)
                .onTapGesture { tuning.closeStepMenu() }
                .accessibilityLabel("Close the step menu")
                .accessibilityAddTraits(.isButton)
            TuneStepMenu(tuning: tuning, sliceId: id)
                .frame(width: dial.maxX, height: max(0, dial.minY - Self.menuGap), alignment: .bottomTrailing)
        }
    }

    // MARK: Places

    /// The knob at the bottom right of the waterfall.
    static func knobFrame(waterfall: CGRect, sideways: Bool, trailingInset: CGFloat = 0) -> CGRect {
        let diameter = WaterfallKnob.metrics(sideways: sideways, waterfallHeight: waterfall.height).diameter
        return CGRect(x: waterfall.maxX - trailingInset - rightInset - diameter, y: waterfall.maxY - knobBottomInset - diameter,
                      width: diameter, height: diameter)
    }

    /// The thumbwheel along the bottom of the waterfall, clear of PTT, at
    /// its right. Sideways it keeps the width it has upright (from a band
    /// `uprightWidth` wide; nil takes the waterfall's width) and sits at the
    /// right, under the right thumb inside the phone's rounded edge, as the
    /// knob does; never wider than the waterfall leaves room for.
    static func wheelFrame(waterfall: CGRect, sideways: Bool = false, uprightWidth: CGFloat? = nil,
                           trailingInset: CGFloat = 0) -> CGRect {
        let inset = sideways ? trailingInset : 0
        let room = waterfall.width - inset - wheelLeftInset - rightInset
        let upright = (uprightWidth ?? waterfall.width) - wheelLeftInset - rightInset
        let width = max(Thumbwheel.stepWidth * 2, sideways ? min(room, upright) : room)
        return CGRect(x: waterfall.maxX - inset - rightInset - width,
                      y: waterfall.maxY - wheelBottomInset - Thumbwheel.height,
                      width: width, height: Thumbwheel.height)
    }

    /// Where zoom minus and plus sit: at the waterfall's bottom right with no
    /// dial there; above the knob or the wheel upright, and to their left
    /// sideways, where the waterfall is short.
    static func zoomOrigin(kind: DialKind, waterfall: CGRect, sideways: Bool, trailingInset: CGFloat = 0,
                           uprightWidth: CGFloat? = nil) -> CGPoint {
        let zoom = ZoomButtons.size
        let standard = CGPoint(x: waterfall.maxX - ZoomButtons.inset - zoom.width,
                               y: waterfall.maxY - ZoomButtons.inset - zoom.height)
        switch kind {
        case .off, .sheetKnob:
            return standard
        case .waterfallKnob:
            let knob = knobFrame(waterfall: waterfall, sideways: sideways, trailingInset: trailingInset)
            if sideways {
                return CGPoint(x: knob.minX - sidewaysZoomGap - zoom.width, y: knob.midY - zoom.height / 2)
            }
            return CGPoint(x: standard.x, y: knob.minY - knobZoomGap - zoom.height)
        case .thumbwheel:
            let wheel = wheelFrame(waterfall: waterfall, sideways: sideways, uprightWidth: uprightWidth,
                                   trailingInset: trailingInset)
            if sideways {
                return CGPoint(x: wheel.minX - sidewaysZoomGap - zoom.width, y: wheel.midY - zoom.height / 2)
            }
            return CGPoint(x: standard.x, y: wheel.minY - wheelZoomGap - zoom.height)
        }
    }

    /// The sheet's knob: the board's big one, smaller when the band is too
    /// short for it (sideways).
    static func sheetMetrics(bandHeight: CGFloat) -> Knob.Metrics {
        let big = Knob.Metrics.sheet
        let room = bandHeight - SheetKnob.chromeHeight
        guard room < big.diameter else {
            return big
        }
        return big.scaled(max(0.4, room / big.diameter))
    }
}
