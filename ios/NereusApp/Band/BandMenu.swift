// NereusSDR for iOS: the band menu a half-second hold on empty band opens: the desktop's Display Range, Tuning, Notch and Display settings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import NereusModels
import SwiftUI
import UIKit

/// Where a hold on the band opens the band menu (R-IOS-57, DC6, spec
/// section 6.1): a press of ``holdSeconds`` that moves less than the drag
/// threshold, on band clear of the flags (the FreeDV row is the flag's),
/// the spots, other devices' slice labels and the dBm scale. A movement
/// past the threshold first is a drag, a lift first is a tap, and the
/// flags, spots and labels keep their own touches.
struct BandMenuHit {
    /// How long a press is held before the band menu opens.
    static let holdSeconds: Double = 0.5
    /// A press that moves this far is a drag, not a hold.
    static let moveLimitPoints: CGFloat = TuneGestures.dragThresholdPoints

    /// One press on the band while it may still become a hold.
    struct Press: Equatable {
        /// Where the finger came down.
        let start: CGPoint
        /// Which press this is, so a timer started for an earlier one does nothing.
        let generation: Int
        /// False once the finger has moved too far or a pinch has begun.
        var live = true
    }

    /// The finger at `point` is still holding where it came down at `start`.
    static func isHold(from start: CGPoint, to point: CGPoint) -> Bool {
        hypot(point.x - start.x, point.y - start.y) < moveLimitPoints
    }

    /// `point` is empty band: inside the band, off the dBm scale, and on
    /// none of the `flags` (full flags with their buttons, and folded
    /// tags), `spots` (labels and badges) or `stations` (other devices'
    /// slice labels).
    static func isEmptyBand(at point: CGPoint, flags: [CGRect], spots: [CGRect], stations: [CGRect],
                            layout: BandLayout) -> Bool {
        guard CGRect(origin: .zero, size: layout.size).contains(point),
              !layout.dbmScale.contains(point), !layout.dbmArrows.contains(point) else {
            return false
        }
        return !(flags + spots + stations).contains { $0.contains(point) }
    }

    /// The spots' labels and +N badges where ``SpotLayer`` draws them.
    static func spotRects(_ result: SpotLayout.Result) -> [CGRect] {
        result.labels.map(\.rect) + result.badges.map(\.rect)
    }

    /// Where another device's slice label sits (``ForeignSliceLabel``): its
    /// letter chip, name, "away" and TX badge, centred on the slice's line
    /// and kept inside the band; nil while its line is off the band.
    @MainActor
    static func stationRect(_ slice: ForeignSliceMarkers.Slice, geometry: BandGeometry,
                            layout: BandLayout) -> CGRect? {
        let lineX = geometry.x(forHz: slice.frequencyHz)
        guard lineX >= 0, lineX <= Double(geometry.size.width) else {
            return nil
        }
        func textWidth(_ text: String) -> CGFloat {
            (text as NSString).size(withAttributes: [.font: UIFont.systemFont(ofSize: 12, weight: .semibold)]).width
        }
        // ForeignSliceLabel's pieces: 3 lead, the 16-point letter, 6 apart, the name, 7 trail.
        var width = 3 + 16 + 6 + textWidth(slice.labelName) + 7
        if slice.away {
            width += 6 + textWidth("away")
        }
        if slice.txSlice {
            width += 6 + 24
        }
        let x = ForeignSliceMarkers.labelX(centreX: CGFloat(lineX), labelWidth: width,
                                          bandWidth: geometry.size.width)
        return CGRect(x: x, y: layout.strip.minY - ForeignSliceLabel.aboveFoot, width: width,
                      height: ForeignSliceLabel.height)
    }
}

/// Where the band menu was opened: the finger's point on the band and the
/// frequency under it, taken when the hold fired, as the desktop takes it
/// at the right-click (SpectrumWidget, setNotchAddFrequency).
struct BandMenuAnchor: Equatable {
    let point: CGPoint
    let frequencyHz: Double
}

/// The desktop's band menu (`src/gui/SpectrumOverlayMenu.cpp:79-232`), in
/// its order and with its words: Display Range (Ref Level and Dyn Range,
/// which move the band's dB Max and dB Min on the Core through
/// ``CoreBandScale``: Ref Level is dB Max and keeps the span, Dyn Range is
/// dB Max less dB Min and keeps the top), Tuning (CTUN, this phone's),
/// Notch ("Add notch here" at the frequency under the finger) and
/// "Display settings...", which opens the pan's sheet. An item that cannot
/// act stays in place, greyed, with its reason under it.
struct BandMenu: View {
    let frequencyHz: Double
    /// The Core's number for the band the pan is on; one it does not number (-1) greys the range.
    let band: Int
    @ObservedObject var scale: CoreBandScale
    @ObservedObject var pan: PanSheetModel
    @Binding var ctun: Bool
    let openDisplay: () -> Void
    /// The pan shows the transmit scale: the range rows are greyed, as the pill's Levels rows are.
    var transmitting = false

    init(frequencyHz: Double, band: Int, scale: CoreBandScale, pan: PanSheetModel, ctun: Binding<Bool>,
         openDisplay: @escaping () -> Void, transmitting: Bool = false) {
        self.frequencyHz = frequencyHz
        self.band = band
        self.scale = scale
        self.pan = pan
        _ctun = ctun
        self.openDisplay = openDisplay
        self.transmitting = transmitting
    }

    /// The menu's width.
    // From src/gui/SpectrumOverlayMenu.cpp:231 (setFixedWidth(280))
    static let width: CGFloat = 280
    /// Ref Level's range, in dBm.
    // From src/gui/SpectrumOverlayMenu.cpp:140 (m_refLevelSlider->setRange(-160, 20))
    static let refLevelRange = StationCatalog.Range(min: -160, max: 20, step: 1)
    /// Dyn Range's range, in dB.
    // From src/gui/SpectrumOverlayMenu.cpp:157 (m_dynRangeSlider->setRange(20, 160))
    static let dynRangeRange = StationCatalog.Range(min: 20, max: 160, step: 1)
    /// How far the menu keeps from the band's edges, and from the finger when beside it.
    static let edgeInset: CGFloat = 8
    static let besideGap: CGFloat = 16

    // MARK: The range

    /// Ref Level and Dyn Range for the band's dB Max and dB Min.
    static func values(maxDbm: Double, minDbm: Double) -> (refLevel: Double, dynRange: Double) {
        (maxDbm, maxDbm - minDbm)
    }

    /// The band's dB Max and dB Min after Ref Level moves to `refLevel`:
    /// the top moves and the span is kept.
    // From src/gui/SpectrumWidget.cpp:13359-13367 (refLevelChanged: bandRangeRequested(dBm - m_dynamicRange, dBm))
    static func range(refLevel: Double, from current: (maxDbm: Double, minDbm: Double))
        -> (maxDbm: Double, minDbm: Double) {
        let ref = min(max(refLevel.rounded(), refLevelRange.min), refLevelRange.max)
        return (ref, ref - (current.maxDbm - current.minDbm))
    }

    /// The band's dB Max and dB Min after Dyn Range moves to `dynRange`:
    /// the top is kept and the bottom moves.
    // From src/gui/SpectrumWidget.cpp:13368-13376 (dynRangeChanged: bandRangeRequested(m_refLevel - dB, m_refLevel))
    static func range(dynRange: Double, from current: (maxDbm: Double, minDbm: Double))
        -> (maxDbm: Double, minDbm: Double) {
        let depth = min(max(dynRange.rounded(), dynRangeRange.min), dynRangeRange.max)
        return (current.maxDbm, current.maxDbm - depth)
    }

    /// Why Ref Level and Dyn Range cannot act now; nil while they can.
    static func rangeReason(transmitting: Bool, shared: Bool, connected: Bool, bandKnown: Bool) -> String? {
        if transmitting {
            return DisplayPillPhoneKeys.transmitLevelsReason
        }
        if !shared {
            return CoreBandScale.olderCoreText
        }
        if !connected {
            return CoreBandScale.notConnectedText
        }
        if !bandKnown {
            return PanSheetModel.noSliceReason
        }
        return nil
    }

    /// Ref Level as the desktop prints it.
    // From src/gui/SpectrumOverlayMenu.cpp:148 ("%1 dBm")
    static func refLevelText(_ value: Double) -> String { "\(Int(value.rounded())) dBm" }

    /// Dyn Range as the desktop prints it.
    // From src/gui/SpectrumOverlayMenu.cpp:165 ("%1 dB")
    static func dynRangeText(_ value: Double) -> String { "\(Int(value.rounded())) dB" }

    /// The frequency under the finger, as the desktop's notch row prints it.
    // From src/gui/SpectrumOverlayMenu.cpp:245-246 ("%1 MHz", 6 decimals)
    static func frequencyText(_ hz: Double) -> String { String(format: "%.6f MHz", hz / 1.0e6) }

    // MARK: Placing it

    /// Where the menu's top-left corner goes for a finger at `finger`:
    /// upright at the finger, as the desktop's opens at the cursor; sideways
    /// beside it, on the side away from the PTT (the band's bottom left), so
    /// on its right while it fits there. Always inside the band, clear of
    /// its rounded edges, and moved right of `avoid` (the PTT button) when
    /// it would cover it.
    static func origin(finger: CGPoint, size: CGSize, bandSize: CGSize, sideways: Bool,
                       leadingInset: CGFloat = 0, trailingInset: CGFloat = 0, avoid: CGRect? = nil) -> CGPoint {
        let lowX = edgeInset + leadingInset
        let highX = max(lowX, bandSize.width - trailingInset - edgeInset - size.width)
        let lowY = edgeInset
        let highY = max(lowY, bandSize.height - edgeInset - size.height)
        func inside(_ x: CGFloat, _ y: CGFloat) -> CGPoint {
            let placed = CGPoint(x: min(max(x, lowX), highX), y: min(max(y, lowY), highY))
            guard let avoid, CGRect(origin: placed, size: size).intersects(avoid) else {
                return placed
            }
            return CGPoint(x: min(max(avoid.maxX + edgeInset, lowX), highX), y: placed.y)
        }
        guard sideways else {
            return inside(finger.x, finger.y)
        }
        let right = finger.x + besideGap
        let x = right <= highX ? right : finger.x - besideGap - size.width
        return inside(x, finger.y - size.height / 2)
    }

    // MARK: Drawing

    var body: some View {
        let range = scale.range(band: band)
        let reason = Self.rangeReason(transmitting: transmitting, shared: scale.isShared, connected: scale.isConnected,
                                      bandKnown: range != nil)
        let values = range.map { Self.values(maxDbm: $0.maxDbm, minDbm: $0.minDbm) }
        let unconfirmed = scale.isUnconfirmed(band: band)
        VStack(alignment: .leading, spacing: 6) {
            // From src/gui/SpectrumOverlayMenu.cpp:131 ("Display Range")
            DisplayPillCaption("Display Range")
                .accessibilityIdentifier("bandMenuDisplayRange")
            // From src/gui/SpectrumOverlayMenu.cpp:137 ("Ref Level")
            PanelSliderRow(label: "Ref Level", value: values?.refLevel, range: Self.refLevelRange,
                           accessibility: "Ref Level", format: Self.refLevelText, greyed: reason != nil,
                           notConfirmed: unconfirmed) { value in
                guard let current = scale.range(band: band) else { return }
                let next = Self.range(refLevel: value, from: current)
                scale.set(maxDbm: next.maxDbm, minDbm: next.minDbm, band: band)
            }
            .accessibilityIdentifier("bandMenuRefLevel")
            // From src/gui/SpectrumOverlayMenu.cpp:154 ("Dyn Range")
            PanelSliderRow(label: "Dyn Range", value: values?.dynRange, range: Self.dynRangeRange,
                           accessibility: "Dyn Range", format: Self.dynRangeText, greyed: reason != nil,
                           notConfirmed: unconfirmed) { value in
                guard let current = scale.range(band: band) else { return }
                let next = Self.range(dynRange: value, from: current)
                scale.set(maxDbm: next.maxDbm, minDbm: next.minDbm, band: band)
            }
            .accessibilityIdentifier("bandMenuDynRange")
            if let reason {
                DisplayPillReason(text: reason)
                    .accessibilityIdentifier("bandMenuRangeReason")
            } else if let refusal = scale.refusal(band: band) {
                DisplayPillReason(text: refusal, refusal: true)
                    .accessibilityIdentifier("bandMenuRangeReason")
            }

            // From src/gui/SpectrumOverlayMenu.cpp:170 ("Tuning")
            DisplayPillCaption("Tuning")
                .accessibilityIdentifier("bandMenuTuning")
            Button {
                ctun.toggle()
            } label: {
                // From src/gui/SpectrumOverlayMenu.cpp:174 ("CTUN (independent pan)")
                Text("CTUN (independent pan)")
                    .font(.system(size: 12, weight: .semibold))
                    .padding(.horizontal, 10)
                    .frame(maxWidth: .infinity, minHeight: 30, alignment: .leading)
            }
            .buttonStyle(WorkspaceButtonStyle(ctun ? .litGreen : .raised))
            .accessibilityValue(ctun ? "On" : "Off")
            .accessibilityIdentifier("bandMenuCtun")

            // From src/gui/SpectrumOverlayMenu.cpp:189 ("Notch")
            DisplayPillCaption("Notch")
                .accessibilityIdentifier("bandMenuNotch")
            HStack(spacing: 8) {
                Button {
                    pan.addNotch(atHz: frequencyHz)
                } label: {
                    // From src/gui/SpectrumOverlayMenu.cpp:194 ("Add notch here")
                    Text("Add notch here")
                        .font(.system(size: 12, weight: .semibold))
                        .padding(.horizontal, 10)
                        .frame(minHeight: 30)
                }
                .buttonStyle(WorkspaceButtonStyle(.raised))
                .disabled(!pan.canAddNotchAtFrequency)
                .accessibilityIdentifier("bandMenuAddNotch")
                Text(Self.frequencyText(frequencyHz))
                    .font(.system(size: 12).monospacedDigit())
                    .foregroundStyle(ChromeColours.text)
                    .opacity(pan.canAddNotchAtFrequency ? 1 : WorkspacePalette.disabledOpacity)
                    .accessibilityIdentifier("bandMenuNotchHz")
                Spacer(minLength: 0)
            }
            if !pan.canAddNotchAtFrequency {
                DisplayPillReason(text: pan.notchUnavailableReason)
                    .accessibilityIdentifier("bandMenuNotchReason")
            } else if let note = pan.note {
                DisplayPillReason(text: note, refusal: true)
                    .accessibilityIdentifier("bandMenuNotchReason")
            }

            Button(action: openDisplay) {
                // From src/gui/SpectrumOverlayMenu.cpp:217 ("Display settings...")
                Text("Display settings...")
                    .font(.system(size: 12, weight: .semibold))
                    .padding(.horizontal, 10)
                    .frame(minHeight: 30)
            }
            .buttonStyle(WorkspaceButtonStyle(.raised))
            .padding(.top, 6)
            .accessibilityIdentifier("bandMenuDisplaySettings")
        }
        .padding(10)
        .frame(width: Self.width, alignment: .leading)
        .workspacePanel()
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("bandMenu")
    }
}

/// The open band menu over the band: a clear layer whose tap closes it,
/// and the menu placed for the finger once its size is known. CTUN is the
/// pan's display's, read again as it changes.
struct BandMenuLayer: View {
    let anchor: BandMenuAnchor
    @ObservedObject var display: DisplaySheetModel
    @ObservedObject var scale: CoreBandScale
    let pan: PanSheetModel
    let sideways: Bool
    var leadingInset: CGFloat = 0
    var trailingInset: CGFloat = 0
    /// Where the PTT is when the band column moved it; nil at its home.
    var ptt: CGRect? = nil
    let close: () -> Void
    let openDisplay: () -> Void

    @State private var size: CGSize = .zero

    var body: some View {
        GeometryReader { proxy in
            let origin = BandMenu.origin(finger: anchor.point, size: size, bandSize: proxy.size, sideways: sideways,
                                         leadingInset: leadingInset, trailingInset: trailingInset,
                                         avoid: ptt ?? PttButton.frame(bandSize: proxy.size, leadingInset: leadingInset))
            ZStack(alignment: .topLeading) {
                Color.black.opacity(0.001)
                    .contentShape(Rectangle())
                    .onTapGesture(perform: close)
                    .accessibilityLabel("Close the band menu")
                    .accessibilityAddTraits(.isButton)
                    .accessibilityIdentifier("bandMenuClose")
                BandMenu(frequencyHz: anchor.frequencyHz, band: scale.shownBand ?? -1, scale: scale, pan: pan,
                         ctun: Binding(get: { display.ctun }, set: { display.setCtun($0) }),
                         openDisplay: openDisplay, transmitting: display.keyedScale)
                    .fixedSize()
                    .background(GeometryReader { measured in
                        Color.clear
                            .onAppear { size = measured.size }
                            .onChange(of: measured.size) { _, next in size = next }
                    })
                    .offset(x: origin.x, y: origin.y)
                    .opacity(size == .zero ? 0 : 1)
            }
        }
    }
}
