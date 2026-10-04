// NereusSDR for iOS: what sits over the band: the flags and tags, the touches that pan, tune and zoom, the zoom buttons
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusBand
import NereusLink
import SwiftUI

/// Everything over one band's Metal view (R-IOS-11, R-IOS-12, D9, D10,
/// D74, spec section 5.1 items 3, 4, 9 and 17), sized to the band:
///
/// - each slice's flag, or its one-line tag where the full flag would land
///   on another (``FlagLayout``); a tap on another slice's flag or tag
///   makes it active; the flag's step opens the step menu
///   (``TuneStepMenu``) and its frequency the number pad (``FrequencyPad``);
/// - the slices' markers, handed to the band's renderer with each triangle
///   hanging from its flag (``SliceMarkers``);
/// - the touches, as on the desktop: a drag that starts on empty band moves
///   the band (the slices keep their frequencies, and their flags move with
///   the band), within the span limits, and past the receiver's window asks
///   the Core to move the window, as the desktop's remote window does; a
///   drag that starts on a flag or its passband tunes that slice in its
///   step (with drag to tune on); a tap tunes the active slice, pinch
///   zooms, and the double tap does Tune, Center or None;
/// - zoom minus and plus at the bottom right of the waterfall. The bottom
///   left stays clear for PTT;
/// - the tuning dial Setup picked for this phone, if any (``DialLayer``):
///   the knob on the waterfall (zoom moves up above it), the thumbwheel,
///   or the knob in a sheet, which a tap on a flag's frequency then raises
///   in place of the number pad. The dial sits outside the band's drag, so
///   turning it never moves the band. So do the flags' open menus and
///   panels and the step menu: a drag on a slider or a scroll in them
///   never moves the band.
///
/// Zooming and panning ask for a new view (``BandModel/requestView(_:)``);
/// the band shows it when the Core's context does. A pan's view stays where
/// it was dropped.
struct BandGestureLayer: View {
    @ObservedObject var band: BandModel
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var settings: PhoneSettings
    /// The flag's step menu and number pad; nil leaves the step and the
    /// frequency without their taps (the flag pictures).
    var tuning: BandTuningModel?
    /// While keyed the passband shading gives way to the TX filter.
    var keyed = false
    /// The orange TX filter while keyed, in hertz on the band.
    var txFilterHz: ClosedRange<Double>?
    /// The slice the radio's own PTT is on the air on, if one of this band's.
    var onAirSliceId: Int?
    /// The slice the TX zero line marks while the radio is on the air,
    /// whatever keyed it: the transmit slice, if one of this band's
    /// (``TransmitModel/txZeroLineSliceId``).
    var txZeroLineSliceId: Int?
    /// Other devices' slices, drawn read-only with a label at the foot of
    /// the spectrum.
    @ObservedObject var foreign: ForeignSlicesModel
    /// The band is turned sideways: the knob is smaller, with zoom to its left.
    var sideways = false
    /// Sideways, the phone's rounded edge on the right, which the knob keeps inside.
    var trailingInset: CGFloat = 0
    /// Sideways, the phone's other rounded edge on the left, which PTT keeps inside.
    var leadingInset: CGFloat = 0
    /// The pan's display, which a drag up or down on the frequency-scale
    /// row (the split) or on the dBm scale moves; nil leaves those places to
    /// pan the band, as everywhere else.
    var display: DisplaySheetModel?
    /// The Core's spots, drawn under the flags (``SpotLayer``); nil draws none.
    var spots: SpotsModel?
    /// How the flags print the signal level (the Multimeter page's units
    /// and decimal point).
    var readout: SMeterReadout = .desktopDefaults
    /// The Core refused this phone's key until a taken slice's TX button
    /// is pressed: the button on each slice it took is ringed (R-IOS-42).
    var firstKeyRefused = false
    /// What the flags' buttons open and run; nil leaves the antenna
    /// button, the tabs and more without menus (the flag pictures).
    var flagControls: FlagControls?
    /// Why this phone may not transmit (a listen-only session): each
    /// flag's TX is greyed as PTT is, and says why when tapped.
    var transmitReason: String?
    /// The TX badges' take (JJ's ruling of 2026-09-30); nil draws each TX
    /// button as the transmit choice alone (the flag pictures).
    var take: TransmitTakeModel?
    @Environment(\.uprightBandWidth) private var uprightWidth
    @Environment(\.dynamicTypeSize) private var typeSize

    /// The drag under way, if any (what it moves is settled where it began).
    @State private var dragger = BandDrag()
    @State private var pinching = false
    /// The band when the pinch began.
    @State private var pinchStart: BandGeometry?
    /// Each full flag's measured height, by slice: the RADE row makes a flag taller.
    @State private var flagHeights: [Int: CGFloat] = [:]
    /// Moves on whenever the take changes, so each badge redraws.
    @State private var badgeTick = 0

    var body: some View {
        GeometryReader { proxy in
            if let axes = band.pointGeometry(size: proxy.size) {
                observingControls {
                    content(layout: axes.layout, geometry: axes.geometry)
                }
            } else {
                Color.clear
                    .task { band.markers = [] }
            }
        }
        .overlay {
            if let tuning, settings.dialKind != .off {
                GeometryReader { proxy in
                    if let axes = band.pointGeometry(size: proxy.size) {
                        DialLayer(tuning: tuning, slices: slices, settings: settings, layout: axes.layout,
                                  sideways: sideways, trailingInset: trailingInset)
                    }
                }
            }
        }
        .overlay(alignment: .bottom) {
            if let tuning {
                PadLayer(tuning: tuning)
            }
        }
    }

    // MARK: Layout

    /// Large type: the flags' taller rows and larger round buttons.
    private var large: Bool { typeSize.isAccessibilitySize }

    /// The layer redraws when what the flags have open changes.
    @ViewBuilder
    private func observingControls<Content: View>(@ViewBuilder _ content: @escaping () -> Content) -> some View {
        if let flagControls {
            ObservingControls(controls: flagControls, content: content)
        } else {
            content()
        }
    }

    private func content(layout: BandLayout, geometry: BandGeometry) -> some View {
        let shown = slices.bandEntries(viewHz: geometry.lowHz...max(geometry.lowHz, geometry.highHz))
        let entries = shown.flags
        let large = large
        let jumped = slices.jumped
        let expected = Dictionary(entries.map { ($0.id, VfoFlagView.expectedHeight(for: $0, large: large)) },
                                  uniquingKeysWith: { first, _ in first })
        let measured = flagHeights
        let column = FlagLayout.sideColumnSize(button: FlagSideButtons.diameter(large: large))
        let olderRade = Set(entries.filter { $0.rade?.wraps == true }.map(\.id))
        func place(top: CGFloat) -> [FlagPlacement] {
            FlagLayout.layout(slices: entries.map(\.slice), activeSliceId: slices.activeSliceId,
                              geometry: geometry,
                              foldedSize: { slice in
                                  entries.first(where: { $0.id == slice.id }).map(FoldedTagView.size(for:))
                                    ?? FlagLayout.foldedSize(frequencyText: slice.frequencyText)
                              },
                              flagHeight: { slice in
                                  let fixed = expected[slice.id] ?? FlagLayout.flagSize.height
                                  // Every flag is its own fixed size but one with an
                                  // older Core's RADE reason or the Core's reason for
                                  // no RADE decoder, which is as measured.
                                  guard olderRade.contains(slice.id) else {
                                      return fixed
                                  }
                                  return measured[slice.id] ?? fixed
                              },
                              sideColumn: column, top: top)
        }
        // While another slice's band shows, the flags start under Back to
        // your band; sideways, where the band is short, they stay at the top
        // when they hang beside it (the board's sideways jump).
        var placements = place(top: 0)
        var underBar = false
        if jumped {
            let bar = JumpBar.rect(sideways: sideways, large: large)
            let reach = column.width + FlagLayout.sideGap
            let clear = sideways && placements.allSatisfy { placement in
                !placement.rect.insetBy(dx: -reach, dy: 0).intersects(bar)
            }
            if !clear {
                placements = place(top: JumpBar.clearance)
                underBar = true
            }
        }
        let others = foreign.entries
        // Other devices' markers first, so this phone's own draw over them.
        let markers = ForeignSliceMarkers.markers(others.map(\.slice))
            + SliceMarkers.markers(slices: entries.map(\.slice), activeSliceId: slices.activeSliceId,
                                   placements: placements, spectrumHeightPoints: geometry.size.height,
                                   keyed: keyed)
        let flags = Array(zip(entries, placements))
        let columns = Self.sideColumns(flags, geometry: geometry, column: column)
        let rects = Self.flagRects(flags, columns: columns)
        let fullFlags = Dictionary(flags.compactMap { $0.1.isFolded ? nil : ($0.0.id, $0.1.rect) },
                                   uniquingKeysWith: { first, _ in first })
        // Upright under Back to your band, the split moves down just enough
        // for the flags to end above the frequency scale; never kept, and
        // cleared on Back. Sideways stays as the board has it.
        let splitFloor = underBar && !sideways
            ? rects.map(\.maxY).max().flatMap { foot in
                JumpBar.spectrumFloor(flagFoot: foot, bandHeight: layout.size.height,
                                      savedShare: band.settings.spectrumShare)
            }
            : nil
        // The WIDE chip keeps clear of every flag and folded tag, the Back to
        // your band bar, the markers at the band's edge and the frames a second.
        // The dBm scale's arrows where DbmScaleArrows draws them: sideways, in from the rounded edge.
        let arrows = layout.dbmArrows.offsetBy(dx: -trailingInset, dy: 0)
        let dialKind = tuning == nil ? DialKind.off : settings.dialKind
        let zoomAt = DialLayer.zoomOrigin(kind: dialKind, waterfall: layout.waterfall, sideways: sideways,
                                          trailingInset: trailingInset, uprightWidth: uprightWidth)
        // The band's own controls, which the edge markers keep clear of.
        var controls = [CGRect(origin: zoomAt, size: ZoomButtons.size),
                        PttButton.frame(bandSize: layout.size, leadingInset: leadingInset)]
        switch dialKind {
        case .waterfallKnob:
            controls.append(DialLayer.knobFrame(waterfall: layout.waterfall, sideways: sideways,
                                                trailingInset: trailingInset))
        case .thumbwheel:
            controls.append(DialLayer.wheelFrame(waterfall: layout.waterfall, sideways: sideways,
                                                 uprightWidth: uprightWidth, trailingInset: trailingInset))
        case .off, .sheetKnob:
            break
        }
        let edgeRects = EdgeMarkers.rects(shown.edges, spectrumFoot: layout.spectrum.maxY,
                                          bandWidth: geometry.size.width, avoid: rects, sideways: sideways,
                                          large: large, dbmArrows: arrows, controls: controls)
        let chipAvoid = rects + placements.filter(\.isFolded).map(\.rect) + edgeRects
            + (jumped && slices.shownEntry != nil ? [JumpBar.rect(sideways: sideways, large: large)] : [])
            + (band.settings.showFps && band.framesPerSecond != nil ? [BandReadouts.fpsRect(layout: layout)] : [])
        let chipRect = WideChip.rect(layout: layout, avoid: chipAvoid, sideways: sideways,
                                     trailingInset: trailingInset, large: large)
        // The active slice's flag on top of the others.
        let ordered = flags.sorted { ($0.0.id == slices.activeSliceId ? 1 : 0) < ($1.0.id == slices.activeSliceId ? 1 : 0) }
        return ZStack(alignment: .topLeading) {
            touchSurface(geometry: geometry)
            if let txFilterHz {
                txFilter(txFilterHz, layout: layout, geometry: geometry)
            }
            // At the transmit slice's dial frequency, as the desktop draws it.
            if let transmitting = entries.first(where: { $0.id == txZeroLineSliceId }) {
                txZeroLine(transmitting.slice.frequencyHz, layout: layout, geometry: geometry)
            }
            ForeignLabels(foreign: foreign, layout: layout, geometry: geometry)
            if let spots {
                // Spots start under the flags, which draw over them (D13).
                SpotLayer(spots: spots, geometry: geometry, placements: placements)
            }
            if let flagControls {
                // Behind the flags: a tap outside them closes an open menu or panel.
                FlagPopoverCatcher(controls: flagControls)
            }
            ForEach(ordered, id: \.0.id) { pair in
                flag(pair.0, pair.1, column: columns[pair.0.id])
            }
            EdgeMarkers(slices: slices, edges: shown.edges, spectrumFoot: layout.spectrum.maxY,
                        bandWidth: geometry.size.width, avoid: rects, sideways: sideways, large: large,
                        dbmArrows: arrows, controls: controls)
            if jumped, let entry = slices.shownEntry {
                JumpBar(entry: entry, sideways: sideways) { slices.back() }
            }
            if let flagControls, let store = flagControls.store {
                WideChipLayer(store: store, controls: flagControls, rect: chipRect, large: large)
            }
            ZoomButtons(zoomOut: { zoom(by: 1 / TuneGestures.buttonZoomFactor, geometry: geometry) },
                        zoomIn: { zoom(by: TuneGestures.buttonZoomFactor, geometry: geometry) })
                .offset(x: zoomAt.x, y: zoomAt.y)
            if let tuning {
                // Over the flags: a tap outside the step menu closes it, and a drag there moves the band.
                StepMenuLayer(tuning: tuning, flags: flags, bandWidth: geometry.size.width, large: large,
                              part: .catcher)
            }
        }
        .simultaneousGesture(drag(layout: layout, geometry: geometry, entries: entries, placements: placements))
        .overlay(alignment: .topLeading) {
            // Over the band's drag, not inside it: a drag that starts in an
            // open menu or panel (a slider, a scroll) is that control's
            // alone and never moves, tunes or zooms the band behind it.
            ZStack(alignment: .topLeading) {
                if let flagControls {
                    FlagPopoverLayer(controls: flagControls, slices: slices, flags: fullFlags, columns: columns,
                                     // The whole band, waterfall included: menus and panels hang over it.
                                     bandSize: layout.size, sideways: sideways, large: large,
                                     transmitReason: transmitReason, wideChip: chipRect)
                }
                if let tuning {
                    StepMenuLayer(tuning: tuning, flags: flags, bandWidth: geometry.size.width, large: large,
                                  part: .menu)
                }
            }
        }
        .task(id: markers) {
            band.markers = markers
        }
        .task(id: rects) {
            slices.flagRects = rects
        }
        .onReceive(take?.objectWillChange.eraseToAnyPublisher() ?? Empty().eraseToAnyPublisher()) { _ in
            badgeTick &+= 1
        }
        // In the same update as the jump, not on a later turn of the main
        // queue: the first frame drawn after the jump already holds the
        // split at the floor.
        .onChange(of: splitFloor, initial: true) { _, floor in
            band.setJumpedShareFloor(floor)
        }
    }

    /// What a flag's TX button and its VoiceOver action run: the band's
    /// guarded transmit choice, which sends nothing for a slice another
    /// device controls; nil when the Core takes no choice from this phone.
    static func selectForTransmit(_ slices: BandSlicesModel, sliceId: Int) -> (() -> Void)? {
        guard slices.canSelectTransmitSlice else {
            return nil
        }
        return { [weak slices] in slices?.selectForTransmit(sliceId) }
    }

    /// Why this phone may not transmit, as PTT and the TX panel say it: the
    /// Core's reason, or that it takes no transmit from a phone; nil while
    /// it lets this phone transmit, and while only the holder's refusal
    /// stands in the way and the Core takes transmit from here (`take`,
    /// ``TransmitTakeModel/takesTransmit``), when the TX button takes
    /// transmit instead.
    static func transmitReason(_ transmit: TransmitModel, take: TransmitTakeModel? = nil) -> String? {
        guard !transmit.permitted else {
            return nil
        }
        if let take, take.takesTransmit {
            return nil
        }
        guard transmit.offered else {
            return TransmitModel.noRemoteTransmitText
        }
        return transmit.permission?.reason ?? TransmitModel.noRemoteTransmitText
    }

    /// Each full flag's column of round buttons, by slice, on its line's
    /// side (``FlagLayout/sideColumnRect(flag:lineX:bandWidth:column:)``).
    static func sideColumns(_ flags: [(BandSlicesModel.Entry, FlagPlacement)], geometry: BandGeometry,
                            column: CGSize) -> [Int: CGRect] {
        var columns: [Int: CGRect] = [:]
        for (entry, placement) in flags where !placement.isFolded {
            columns[entry.id] = FlagLayout.sideColumnRect(flag: placement.rect,
                                                          lineX: geometry.x(forHz: entry.slice.frequencyHz),
                                                          bandWidth: Double(geometry.size.width), column: column)
        }
        return columns
    }

    /// Every full flag with its round buttons: what the band's notices keep clear of.
    static func flagRects(_ flags: [(BandSlicesModel.Entry, FlagPlacement)], columns: [Int: CGRect]) -> [CGRect] {
        flags.compactMap { entry, placement in
            guard !placement.isFolded else {
                return nil
            }
            return columns[entry.id].map { placement.rect.union($0) } ?? placement.rect
        }
    }

    /// What the flag's TX badge offers, read again at every redraw.
    private func badgeOffer(_ entry: BandSlicesModel.Entry) -> TransmitTakeModel.BadgeOffer? {
        _ = badgeTick
        return take?.badgeOffer(entry)
    }

    @ViewBuilder
    private func flag(_ entry: BandSlicesModel.Entry, _ placement: FlagPlacement, column: CGRect?) -> some View {
        let rect = placement.rect
        let controls = flagControls
        let id = entry.id
        switch placement {
        case .full:
            VfoFlagView(entry: entry, meter: band.catalog?.meters.sMeter, readout: readout,
                        openPad: tuning.map { tuning in {
                            controls?.close()
                            openFrequency(id, tuning: tuning)
                        } },
                        onAir: id == onAirSliceId,
                        heightChanged: { height in
                            if flagHeights[id] != height {
                                flagHeights[id] = height
                            }
                        },
                        taking: slices.taking.contains(id),
                        takeControl: entry.access == nil ? nil : { slices.takeControl(id) },
                        takeRefusal: entry.takeRefusal,
                        selectForTransmit: Self.selectForTransmit(slices, sliceId: id),
                        txBadge: badgeOffer(entry),
                        takeBadge: take.map { take in { take.badgeTapped(id) } },
                        txHint: firstKeyRefused && slices.takenHere.contains(id),
                        transmitReason: transmitReason,
                        showReason: { [weak slices] in slices?.showReason($0) },
                        openAntennas: controls.map { controls in { controls.toggle(.antennas(id)) } },
                        toggleTab: controls.map { controls in { controls.toggle(.panel(id, $0)) } },
                        antennasOpen: controls?.isOpen(.antennas(id)) ?? false,
                        openTab: controls?.openTab(id),
                        bypassOn: controls?.bypassOn ?? false,
                        openDiversity: controls.map { controls in { controls.openDiversity(entry) } })
                .onTapGesture(coordinateSpace: .local) { point in
                    // Even if an ancestor recognises the tap, the DIV target never selects its band slice.
                    guard !entry.diversityOn || !FlagMetrics(large: large).diversity.contains(point) else { return }
                    controls?.close()
                    slices.activate(id)
                }
                .offset(x: rect.minX, y: rect.minY)
            if let column {
                FlagSideButtons(letter: entry.slice.letter, colour: BandColours.slice(entry.slice.colour),
                                locked: entry.locked, dimmedWords: entry.ownerLine,
                                showReason: { [weak slices] in slices?.showReason($0) },
                                moreOpen: (controls?.isOpen(.more(id)) ?? false) && controls?.moreFromWideChip != true,
                                close: {
                                    if let controls {
                                        controls.closeSlice(id)
                                    } else if id != 0 {
                                        slices.close(id)
                                    }
                                },
                                toggleLock: {
                                    if let controls {
                                        controls.toggleLock(entry)
                                    } else {
                                        slices.setLocked(!entry.locked, sliceId: id)
                                    }
                                },
                                more: { controls?.toggle(.more(id)) })
                    .offset(x: column.minX, y: column.minY)
            }
        case .folded:
            FoldedTagView(entry: entry, activate: {
                controls?.close()
                slices.activate(id)
            }, openDiversity: { controls?.openDiversity(entry) })
                .offset(x: rect.minX, y: rect.minY)
        }
    }

    /// The TX filter while keyed (the board's `.pan__tx`): a wash in the
    /// pan's TX passband colour with 2-point edges over the spectrum, and
    /// over the waterfall too when the pan's settings ask (the desktop's
    /// Show TX filter on RX waterfall).
    private func txFilter(_ span: ClosedRange<Double>, layout: BandLayout, geometry: BandGeometry) -> some View {
        let lowX = CGFloat(geometry.x(forHz: span.lowerBound))
        let highX = CGFloat(geometry.x(forHz: span.upperBound))
        let width = max(highX - lowX, 2)
        let settings = band.settings
        return Rectangle()
            .fill(BandColours.withAlpha(settings.txPassbandColour))
            .overlay(alignment: .leading) { Rectangle().fill(ChromeColours.txFilterEdge).frame(width: 2) }
            .overlay(alignment: .trailing) { Rectangle().fill(ChromeColours.txFilterEdge).frame(width: 2) }
            .frame(width: width, height: settings.showTxFilterOnWaterfall ? layout.waterfall.maxY : layout.spectrum.maxY)
            .offset(x: lowX)
            .allowsHitTesting(false)
            .accessibilityHidden(true)
    }

    /// The TX zero line while the radio is on the air: down the spectrum at
    /// the transmit slice's dial frequency, and down the waterfall too when
    /// the pan's settings ask.
    private func txZeroLine(_ hz: Double, layout: BandLayout, geometry: BandGeometry) -> some View {
        let x = CGFloat(geometry.x(forHz: hz))
        let settings = band.settings
        let colour = BandColours.withAlpha(settings.txZeroLineColour)
        // Dashed down the spectrum, solid down the waterfall, as the desktop draws it.
        return ZStack(alignment: .topLeading) {
            Path { path in
                path.move(to: CGPoint(x: 0.5, y: 0))
                path.addLine(to: CGPoint(x: 0.5, y: layout.spectrum.maxY))
            }
            .stroke(colour, style: StrokeStyle(lineWidth: 1, dash: BandColours.txZeroLineDash))
            .frame(width: 1, height: layout.spectrum.maxY)
            if settings.showTxZeroLineOnWaterfall {
                Rectangle()
                    .fill(colour)
                    .frame(width: 1, height: max(0, layout.waterfall.maxY - layout.waterfall.minY))
                    .offset(y: layout.waterfall.minY)
            }
        }
        .offset(x: x)
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }

    // MARK: Touches

    private func touchSurface(geometry: BandGeometry) -> some View {
        Color.clear
            .contentShape(Rectangle())
            .gesture(taps(geometry: geometry))
            .simultaneousGesture(pinch(geometry: geometry), including: settings.pinchToZoom ? .all : .none)
            .accessibilityElement()
            .accessibilityLabel("Band")
            .accessibilityHint(settings.tapToTune ? "Tap to tune the active slice, drag to move the band"
                                                  : "Drag to move the band")
    }

    /// A tap on a flag's frequency: the number pad, or with the knob in a
    /// sheet chosen, the sheet for that slice.
    private func openFrequency(_ sliceId: Int, tuning: BandTuningModel) {
        if settings.dialKind == .sheetKnob {
            guard let entry = slices.entries.first(where: { $0.id == sliceId }), slices.isTunable(entry) else {
                return
            }
            tuning.openDialSheet(sliceId: sliceId)
        } else {
            tuning.openPad(sliceId: sliceId)
        }
    }

    /// One drag: what it moves is settled where the finger came down (a
    /// flag or passband of a slice this phone may change, with drag to
    /// tune on, else the band) and stays so until it lifts.
    private func drag(layout: BandLayout, geometry: BandGeometry, entries: [BandSlicesModel.Entry],
                      placements: [FlagPlacement]) -> some Gesture {
        DragGesture(minimumDistance: TuneGestures.dragThresholdPoints)
            .onChanged { value in
                guard !pinching else {
                    return
                }
                dragger.changed(from: value.startLocation, translation: value.translation.width,
                                rise: value.translation.height, geometry: geometry, entries: entries,
                                placements: placements, band: band, slices: slices, dragToTune: settings.dragToTune,
                                layout: layout, display: display)
                // The frequency under the finger, for the cursor readout.
                band.cursorHz = geometry.hz(forX: Double(value.location.x))
            }
            .onEnded { _ in
                dragger.ended(slices: slices)
                band.cursorHz = nil
            }
    }

    /// A tap tunes at once whenever a double tap would do the same thing;
    /// it waits to rule out a double tap only when the two differ.
    private func taps(geometry: BandGeometry) -> AnyGesture<Void> {
        let action = settings.doubleTapAction
        let single = SpatialTapGesture(count: 1).onEnded { value in
            tap(atX: value.location.x, geometry: geometry)
        }
        let double = SpatialTapGesture(count: 2).onEnded { value in
            doubleTap(action, atX: value.location.x, geometry: geometry)
        }
        switch settings.tapHandling {
        case .none, .singleAtOnce:
            return AnyGesture(single.map { _ in () })
        case .doubleOnly:
            return AnyGesture(double.map { _ in () })
        case .doubleThenSingle:
            return AnyGesture(double.exclusively(before: single).map { _ in () })
        }
    }

    /// The band zooms with the fingers as they spread or close, at once,
    /// from the band as it was when the pinch began (D74).
    private func pinch(geometry: BandGeometry) -> some Gesture {
        MagnifyGesture()
            .onChanged { value in
                let start = pinchStart ?? geometry
                pinchStart = start
                pinching = true
                pinched(value, from: start)
            }
            .onEnded { value in
                pinching = false
                pinched(value, from: pinchStart ?? geometry)
                pinchStart = nil
            }
    }

    private func pinched(_ value: MagnifyGesture.Value, from start: BandGeometry) {
        guard value.magnification > 0 else {
            return
        }
        // Fingers spreading close in: the span shrinks by their spread.
        let about = start.hz(forX: Double(value.startLocation.x))
        band.requestView(TuneGestures.zoomed(start, by: 1 / Double(value.magnification), aboutHz: about,
                                             spanLimits: spanLimits))
    }

    private func tap(atX x: CGFloat, geometry: BandGeometry) {
        if foreign.openNote != nil {
            // A touch on the band closes an open note, and tunes nothing.
            foreign.openNote = nil
            return
        }
        guard settings.tapToTune, let active = slices.active else {
            return
        }
        slices.tap(to: TuneGestures.tapped(atX: x, geometry: geometry, snap: settings.snapTapToStep,
                                           stepHz: snapStep(active)))
    }

    /// Tune writes to the active slice, even with tap-to-tune off; Center
    /// asks for a new view, as zoom does, and retunes nothing.
    private func doubleTap(_ action: TuneGestures.DoubleTapAction, atX x: CGFloat, geometry: BandGeometry) {
        let step = slices.active.flatMap { snapStep($0) }
        switch TuneGestures.doubleTapped(action, atX: x, geometry: geometry, snap: settings.snapTapToStep,
                                         stepHz: step) {
        case .tune(let hz):
            slices.tap(to: hz)
        case .view(let view):
            band.requestView(view)
        case nil:
            break
        }
    }

    private func snapStep(_ active: BandSlicesModel.Entry) -> Double? {
        BandDrag.snapStep(active, catalog: band.catalog)
    }

    private func zoom(by factor: Double, geometry: BandGeometry) {
        let about = slices.active?.slice.frequencyHz ?? geometry.centerHz
        band.requestView(TuneGestures.zoomed(geometry, by: factor, aboutHz: about, spanLimits: spanLimits))
    }

    private var spanLimits: ClosedRange<Double> {
        band.spanLimits(sampleRateHz: slices.active?.sampleRateHz)
    }
}

/// Other devices' slices' labels at the foot of the spectrum, each centred
/// on its line and kept inside the band, with the open one's note above it.
private struct ForeignLabels: View {
    @ObservedObject var foreign: ForeignSlicesModel
    let layout: BandLayout
    let geometry: BandGeometry

    var body: some View {
        ForEach(foreign.entries) { other in
            let lineX = geometry.x(forHz: other.slice.frequencyHz)
            if lineX >= 0, lineX <= Double(geometry.size.width) {
                let top = layout.strip.minY - ForeignSliceLabel.aboveFoot
                ForeignLabelPlacement(centreX: CGFloat(lineX), bandWidth: geometry.size.width, y: top) {
                    ForeignSliceLabel(entry: other, open: foreign.openNote == other.id) {
                        foreign.openNote = foreign.openNote == other.id ? nil : other.id
                    }
                }
                if foreign.openNote == other.id {
                    let x = ForeignSliceMarkers.labelX(centreX: CGFloat(lineX), labelWidth: ForeignSliceLabel.noteWidth,
                                                       bandWidth: geometry.size.width)
                    // Hung above the label: a frame of no height, its
                    // content standing up from its foot.
                    ForeignSliceLabel.Note(entry: other)
                        .fixedSize()
                        .frame(width: ForeignSliceLabel.noteWidth, height: 0, alignment: .bottom)
                        .offset(x: x, y: top - 6)
                        .allowsHitTesting(false)
                }
            }
        }
    }
}

/// Places a label centred on `centreX` and kept inside the band, once its
/// own width is known (``ForeignSliceMarkers/labelX(centreX:labelWidth:bandWidth:)``).
private struct ForeignLabelPlacement<Label: View>: View {
    let centreX: CGFloat
    let bandWidth: CGFloat
    let y: CGFloat
    @ViewBuilder let label: () -> Label

    @State private var width: CGFloat = 0

    var body: some View {
        label()
            .background(GeometryReader { proxy in
                Color.clear
                    .onAppear { width = proxy.size.width }
                    .onChange(of: proxy.size.width) { _, next in width = next }
            })
            .offset(x: ForeignSliceMarkers.labelX(centreX: centreX, labelWidth: width, bandWidth: bandWidth), y: y)
            .opacity(width > 0 ? 1 : 0)
    }
}

/// Redraws its content whenever what the flags have open changes.
private struct ObservingControls<Content: View>: View {
    @ObservedObject var controls: FlagControls
    @ViewBuilder let content: () -> Content

    var body: some View {
        content()
    }
}

/// The open step menu, under its flag's step, over a clear layer whose tap
/// closes it.
struct StepMenuLayer: View {
    /// The layer's two parts, drawn apart: the clear layer under the
    /// band's drag, the menu over it, so a drag on the menu never moves
    /// the band.
    enum Part {
        case catcher
        case menu
    }

    @ObservedObject var tuning: BandTuningModel
    let flags: [(BandSlicesModel.Entry, FlagPlacement)]
    let bandWidth: CGFloat
    var large = false
    var part: Part

    /// Where the menu hangs from the flag: under its frequency, its right
    /// edge on the frequency's (the redrawn flag has its step in the X/RIT
    /// panel, so the menu opens from the dial alone).
    static func origin(flag rect: CGRect, bandWidth: CGFloat, large: Bool) -> CGPoint {
        let step = FlagMetrics(large: large).frequency().offsetBy(dx: rect.minX, dy: rect.minY)
        let x = step.maxX - TuneStepMenu.width
        return CGPoint(x: min(max(x, 4), max(4, bandWidth - TuneStepMenu.width - 4)),
                       y: step.maxY + FlagPopoverLayer.belowButton)
    }

    var body: some View {
        if !tuning.stepMenuFromDial, let id = tuning.stepMenuSliceId, let rect = flags.first(where: { $0.0.id == id })?.1.rect {
            switch part {
            case .catcher:
                Color.black.opacity(0.001)
                    .contentShape(Rectangle())
                    .onTapGesture { tuning.closeStepMenu() }
                    .accessibilityLabel("Close the step menu")
                    .accessibilityAddTraits(.isButton)
            case .menu:
                let origin = Self.origin(flag: rect, bandWidth: bandWidth, large: large)
                TuneStepMenu(tuning: tuning, sliceId: id)
                    .offset(x: origin.x, y: origin.y)
            }
        }
    }
}

/// The open number pad at the foot of the band, over a clear layer whose
/// tap closes it.
private struct PadLayer: View {
    @ObservedObject var tuning: BandTuningModel

    var body: some View {
        if let pad = tuning.pad {
            ZStack(alignment: .bottom) {
                Color.black.opacity(0.001)
                    .contentShape(Rectangle())
                    .onTapGesture { tuning.closePad() }
                    .accessibilityLabel("Close the number pad")
                    .accessibilityAddTraits(.isButton)
                FrequencyPad(model: pad)
                    .padding(8)
            }
        }
    }
}
