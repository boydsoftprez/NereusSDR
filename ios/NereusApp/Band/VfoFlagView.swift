// NereusSDR for iOS: a slice's full VFO flag on the band, 200 points wide, every control a finger-sized button
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import NereusModels
import SwiftUI

/// Where each of a full flag's buttons sits, in the flag's own points
/// (the board's redrawn flag, `#flagbtn-review`, approved 2026-09-30):
/// the desktop flag's rows at one geometry for every text size. Top to
/// bottom: the 44-point header (antennas, the passband's width, TX and the
/// slice's letter), the 44-point owner row on a slice this phone only
/// listens to, the 40-point frequency across the flag, a 4-point gap, the
/// 24-point level row, the RADE row in RADE (16 points and a 2-point gap)
/// and the four 44-point tabs across the flag's whole width. Only the
/// words grow in large type, by ``scale``, to the board's caps. The flag
/// draws from these, and the tests check its targets from them.
struct FlagMetrics: Equatable {
    /// Large type (an accessibility text size).
    let large: Bool

    /// The flag's border, above the header and below the tabs.
    static let top: CGFloat = 1
    static let bottom: CGFloat = 1
    static let inset: CGFloat = 5
    static let gap: CGFloat = 2
    static let header: CGFloat = 44
    static let ownerRow: CGFloat = FlagLayout.ownerRowHeight
    static let frequencyHeight: CGFloat = 40
    static let levelTop: CGFloat = 4
    static let levelHeight: CGFloat = 24
    static let radeTop: CGFloat = 2
    static let radeHeight: CGFloat = 16
    static let tabHeight: CGFloat = 44
    static let antennaWidth: CGFloat = 80
    static let bandwidthWidth: CGFloat = 36
    static let txWidth: CGFloat = 44
    static let letterSize: CGFloat = 20
    /// The four tabs' widths, left to right: sound, DSP, mode, X/RIT (the board's fbt-r4).
    static let soundTabWidth: CGFloat = 40
    static let dspTabWidth: CGFloat = 41.5
    static let modeTabWidth: CGFloat = 67.5
    static let xritTabWidth: CGFloat = 51
    static let tabWidths: [CGFloat] = [soundTabWidth, dspTabWidth, modeTabWidth, xritTabWidth]

    var width: CGFloat { FlagLayout.flagSize.width }
    /// How much the words grow: 1, and the board's cap of 1.25 in large type.
    var scale: CGFloat { large ? 1.25 : 1 }
    /// The tab words and Take control, which stop at 14 points.
    var tabPoints: CGFloat { min(12 * scale, 14) }
    /// The owner row's words, which stop at 12.5 points on 16-point lines.
    var ownerPoints: CGFloat { min(11 * scale, 12.5) }
    var ownerLine: CGFloat { min(14 * scale, 16) }
    var frequencyPoints: CGFloat { 24 * scale }
    /// The compact Take control's width on the owner row.
    var takeWidth: CGFloat { large ? 100 : 90 }

    /// The plain flag: 158 points at every text size.
    var plainHeight: CGFloat { height(rade: false, listening: false) }

    /// The flag's height: 158, 176 with the RADE row, and 44 more with the owner row.
    func height(rade: Bool, listening: Bool) -> CGFloat {
        Self.top + Self.header + (listening ? Self.ownerRow : 0) + Self.frequencyHeight + Self.levelTop
            + Self.levelHeight + (rade ? Self.radeTop + Self.radeHeight : 0) + Self.tabHeight + Self.bottom
    }

    var headerY: CGFloat { Self.top }
    func ownerY() -> CGFloat { Self.top + Self.header }
    func frequencyY(listening: Bool = false) -> CGFloat {
        Self.top + Self.header + (listening ? Self.ownerRow : 0)
    }
    func tabsY(rade: Bool = false, listening: Bool = false) -> CGFloat {
        height(rade: rade, listening: listening) - Self.bottom - Self.tabHeight
    }

    var antenna: CGRect { CGRect(x: Self.inset, y: headerY, width: Self.antennaWidth, height: Self.header) }
    var diversity: CGRect { CGRect(x: antenna.maxX, y: headerY, width: 44, height: Self.header) }
    var transmit: CGRect {
        CGRect(x: width - Self.inset - Self.letterSize - Self.gap - Self.txWidth, y: headerY, width: Self.txWidth,
               height: Self.header)
    }
    func frequency(listening: Bool = false) -> CGRect {
        CGRect(x: Self.inset, y: frequencyY(listening: listening), width: width - 2 * Self.inset,
               height: Self.frequencyHeight)
    }
    /// The owner row's Take control, at the row's right.
    var take: CGRect {
        CGRect(x: width - Self.inset - takeWidth, y: ownerY(), width: takeWidth, height: Self.ownerRow)
    }

    /// The four tabs, left to right, across the flag's whole width.
    func tabs(rade: Bool = false, listening: Bool = false) -> [CGRect] {
        let y = tabsY(rade: rade, listening: listening)
        var x: CGFloat = 0
        return Self.tabWidths.map { tabWidth in
            defer { x += tabWidth }
            return CGRect(x: x, y: y, width: tabWidth, height: Self.tabHeight)
        }
    }

    /// Every button on the flag, named, in the flag's points.
    func targets(rade: Bool = false, listening: Bool = false) -> [(name: String, rect: CGRect)] {
        [("Antennas", antenna), ("TX", transmit), ("Frequency", frequency(listening: listening))]
            + (listening ? [("Take control", take)] : [])
            + zip(FlagTab.allCases, tabs(rade: rade, listening: listening)).map { ("Tab \($0.0.title)", $0.1) }
    }
}

/// A slice's full flag (D7, D9, spec section 5.1 item 4, R-IOS-11), as
/// JJ's board draws the redrawn flag (`#flagbtn-review`, approved
/// 2026-09-30): the desktop flag's rows, every control a finger-sized
/// button, with plain words and thin dividers (``FlagMetrics``). Top to
/// bottom: the antenna button (the receive antenna over the transmit one,
/// BYPS while it is on), the passband's width, TX and the slice's letter;
/// the owner row on a slice this phone only listens to; the frequency in
/// its thin frame, whose tap opens the number pad (D74); the signal level;
/// the RADE row in RADE; and the four tabs, sound, DSP, mode and X/RIT,
/// each dropping its panel from the flag. The step is the X/RIT panel's
/// step cycle, as on the desktop. The open tab and the open antenna menu
/// are underlined in the slice's colour; TX on is underlined red. A tap
/// elsewhere on another slice's flag makes that slice active. The round
/// buttons beside the flag are ``FlagSideButtons``.
///
/// A slice this phone listens to and does not control (R-IOS-42, JJ's
/// rulings of 2026-09-30) has a dashed edge in its colour and, under the
/// header, one 44-point owner row: the short owner words ("Shack desktop
/// controls A") at the left and a compact outlined Take control at the
/// right, which says "Taking control..." until the Core answers. While the
/// slice is on the air Take control is greyed, never hidden, and a tap on
/// it gives the Core's words. The controls that would change the slice
/// (antennas, TX, the frequency, DSP, mode and X/RIT) are dimmed and a tap
/// on one gives the Core's line for who controls it; the sound tab stays
/// live, for this phone's own volume. While this phone may not transmit,
/// TX is greyed as PTT is, and a tap on it says why.
struct VfoFlagView: View {
    let entry: BandSlicesModel.Entry
    let meter: StationCatalog.Meters.SMeter?
    /// How the level beside the bar is printed.
    var readout: SMeterReadout = .desktopDefaults
    var openPad: (() -> Void)?
    /// The radio's own PTT is on the air on this slice (spec section 5.9
    /// item 3): the flag says ON AIR in its TX button and its frequency
    /// greys; the Core refuses tuning it until the press ends.
    var onAir = false
    /// Told the flag's height whenever it changes: the RADE row and the
    /// owner row make the flag taller (``FlagLayout``'s `flagHeight`).
    var heightChanged: ((CGFloat) -> Void)?
    /// Take control was sent for this slice and the Core has not answered.
    var taking = false
    /// Take control on a slice this phone listens to; nil leaves the button greyed.
    var takeControl: (() -> Void)?
    /// Why Take control is greyed, in the Core's words: the slice is on
    /// the air. A tap on the greyed button shows them through `showReason`.
    var takeRefusal: String?
    /// TX's press on one of this phone's slices; nil leaves it doing nothing.
    var selectForTransmit: (() -> Void)?
    /// What TX offers (JJ's ruling of 2026-09-30): the transmit choice, a
    /// take of what the slice needs (`takeBadge`), or greyed with a
    /// reason. Nil draws TX as the transmit choice alone.
    var txBadge: TransmitTakeModel.BadgeOffer?
    var takeBadge: (() -> Void)?
    /// The Core refused a key until this slice's TX button is pressed: the button is ringed.
    var txHint = false
    /// Why this phone may not transmit (a listen-only session): TX is
    /// greyed, and its tap shows these words through `showReason`.
    var transmitReason: String?
    var showReason: ((String) -> Void)?
    /// The antenna button and the tabs; nil leaves them without a tap (the flag pictures).
    var openAntennas: (() -> Void)?
    var toggleTab: ((FlagTab) -> Void)?
    /// What the flag has open: lit and underlined in the slice's colour.
    var antennasOpen = false
    var openTab: FlagTab?
    /// RX bypass on transmit is on: the antenna button says BYPS.
    var bypassOn = false
    var openDiversity: (() -> Void)?

    @Environment(\.dynamicTypeSize) private var typeSize

    /// The words on the owner row's button.
    static let takeControlTitle = "Take control"
    static let takingControlTitle = "Taking control\u{2026}"
    /// How dim a control this phone may not use is, the board's 0.4.
    static let dimmed: Double = 0.4
    /// How dim a listened slice's controls are, the board's 0.35.
    static let listened: Double = 0.35
    /// The thin dividers of look 2: beside the antennas and TX, and between the tabs.
    static let dividerColour = BandColours.tabDivider
    static let hairline = Color.white.opacity(0.12)
    /// The owner row's hairlines, the board's.
    static let ownerHairline = Color.white.opacity(0.14)
    /// Take control's outline and words on the owner row, the board's.
    static let takeEdge = Color(red: 160 / 255, green: 190 / 255, blue: 220 / 255).opacity(0.55)
    static let takeText = ChromeColours.textBright
    static let offText = BandColours.txBadgeOffText
    static let transmitOnText = BandColours.txBadgeOnText
    static let transmitOnLine = BandColours.txBadgeOnBorder
    static let bypassText = Color(red: 0xFF / 255, green: 0xCC / 255, blue: 0x44 / 255)
    /// A listened flag's dashed edge.
    static let listenedDash: [CGFloat] = [4, 3]

    /// The flag's VoiceOver action for its TX button: its name and what it
    /// runs (the band's guarded transmit choice), or nil where the flag
    /// offers none: no choice from this Core, or already the transmit slice.
    static func transmitAction(_ entry: BandSlicesModel.Entry,
                               selectForTransmit: (() -> Void)?) -> (name: String, run: () -> Void)? {
        guard let selectForTransmit, !entry.slice.txSlice else {
            return nil
        }
        return ("Make slice \(entry.slice.letter) the transmit slice", selectForTransmit)
    }

    /// A flag's height: the plain flag, with the RADE row in RADE and the
    /// owner row on a slice this phone only listens to. One geometry at
    /// every text size, so large type does not change it.
    static func expectedHeight(for entry: BandSlicesModel.Entry, large: Bool = false) -> CGFloat {
        let height = FlagMetrics(large: large).height(rade: entry.rade != nil, listening: entry.listening)
        // An older Core's RADE reason, or the Core's reason for no RADE
        // decoder, takes a second line, measured once drawn.
        return entry.rade?.wraps == true ? height + olderRadeExtra : height
    }

    /// The older Core's RADE reason's second line, before it is measured.
    static let olderRadeExtra: CGFloat = 14

    private var metrics: FlagMetrics { FlagMetrics(large: typeSize.isAccessibilitySize) }
    private var colour: Color { BandColours.slice(entry.slice.colour) }
    private var letter: String { entry.slice.letter }
    /// The Core's line for who controls a listened slice: a dimmed control's tap.
    private var ownerLine: String? { entry.ownerLine }

    var body: some View {
        let metrics = metrics
        let height = Self.expectedHeight(for: entry, large: metrics.large)
        VStack(alignment: .leading, spacing: 0) {
            header(metrics)
            if entry.listening {
                ownerRow(metrics)
            }
            frequencyRow(metrics)
            FlagLevelBar(dbm: entry.signalDbm, meter: meter, readout: readout)
                .frame(height: FlagMetrics.levelHeight)
                .padding(.top, FlagMetrics.levelTop)
            if let rade = entry.rade {
                if rade.wraps {
                    // The older Core's reason, or the Core's reason for no
                    // decoder, wraps: the flag grows for it.
                    RadeRowView(reception: rade, fixedPoints: 10 * metrics.scale)
                        .padding(.top, FlagMetrics.radeTop)
                } else {
                    RadeRowView(reception: rade, fixedPoints: 10 * metrics.scale)
                        .frame(height: FlagMetrics.radeHeight)
                        .clipped()
                        .padding(.top, FlagMetrics.radeTop)
                }
            }
            tabs(metrics)
        }
        .padding(.top, FlagMetrics.top)
        .padding(.bottom, FlagMetrics.bottom)
        .padding(.horizontal, FlagMetrics.inset)
        .modifier(FlagFrame(grows: entry.rade?.wraps == true, height: height))
        .onGeometryChange(for: CGFloat.self, of: { $0.size.height }, action: { heightChanged?($0) })
        .background(BandColours.flagBackground, in: RoundedRectangle(cornerRadius: 4))
        .overlay {
            if entry.listening {
                RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(colour, style: StrokeStyle(lineWidth: 1, dash: Self.listenedDash))
                    .accessibilityHidden(true)
            } else {
                RoundedRectangle(cornerRadius: 4).strokeBorder(BandColours.flagBorder, lineWidth: 1)
            }
        }
        .overlay(alignment: .top) {
            Rectangle().fill(colour).frame(height: 2).padding(.horizontal, 1)
        }
        .contentShape(Rectangle())
        // Each button its own element, named with its slice.
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Slice \(letter), \(entry.slice.frequencyText), \(entry.modeLabel)")
        .accessibilityIdentifier("flag\(letter)")
    }

    /// A control's press: on a listened slice the Core's line for who
    /// controls it, else the control's own action.
    private func press(_ action: () -> Void) {
        if let ownerLine {
            showReason?(ownerLine)
        } else {
            action()
        }
    }

    // MARK: The header

    private func header(_ metrics: FlagMetrics) -> some View {
        HStack(spacing: 0) {
            antennaButton(metrics)
            Group {
                if entry.diversityOn {
                    Button { openDiversity?() } label: {
                        VStack(spacing: 2) {
                            filterWidth(metrics)
                            Text("DIV").font(.system(size: 9 * metrics.scale, weight: .bold))
                                .foregroundStyle(ChromeColours.accent)
                        }
                        .frame(width: metrics.diversity.width, height: FlagMetrics.header)
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("Diversity, slice \(letter)")
                    .accessibilityIdentifier("flagDiv\(letter)")
                } else {
                    filterWidth(metrics).frame(width: metrics.diversity.width, height: FlagMetrics.header)
                }
            }
            transmitButton(metrics)
            Spacer().frame(width: FlagMetrics.gap)
            SliceLetterBadge(letter: letter, colour: colour, size: FlagMetrics.letterSize, points: 11 * metrics.scale)
                .accessibilityHidden(true)
        }
        .frame(height: FlagMetrics.header)
    }

    private func filterWidth(_ metrics: FlagMetrics) -> some View {
        Text(entry.slice.bandwidthText)
            .font(.system(size: 11 * metrics.scale, weight: .bold))
            .foregroundStyle(BandColours.bandwidth)
            .lineLimit(1).minimumScaleFactor(0.6)
            .accessibilityLabel("Filter width, slice \(letter)")
            .accessibilityValue(entry.slice.bandwidthText)
    }

    /// The one antenna button: RX over TX, BYPS while on, and the caret.
    private func antennaButton(_ metrics: FlagMetrics) -> some View {
        let dim = entry.listening
        return Button {
            press { openAntennas?() }
        } label: {
            HStack(spacing: 5) {
                VStack(alignment: .leading, spacing: 0) {
                    Text(entry.rxAntenna).foregroundStyle(BandColours.rxAntenna)
                        .frame(height: 17)
                    Text(entry.txAntenna).foregroundStyle(BandColours.txAntenna)
                        .frame(height: 17)
                }
                .font(.system(size: 11 * metrics.scale, weight: .bold))
                .lineLimit(1)
                .minimumScaleFactor(0.6)
                VStack(spacing: 2) {
                    if bypassOn {
                        Text(FlagControls.bypassTitle)
                            .font(.system(size: 9 * metrics.scale, weight: .bold))
                            .foregroundStyle(Self.bypassText)
                            .lineLimit(1)
                            .frame(height: 13)
                    }
                    Text("\u{25BE}")
                        .font(.system(size: 10 * metrics.scale))
                        .foregroundStyle(antennasOpen ? colour : Self.offText)
                        .frame(height: 12)
                }
            }
            .padding(.horizontal, 3)
            .frame(width: FlagMetrics.antennaWidth, height: FlagMetrics.header)
            .modifier(OpenLine(lit: antennasOpen, colour: colour))
            .contentShape(Rectangle())
            .opacity(dim ? Self.listened : 1)
        }
        .buttonStyle(.plain)
        .overlay(alignment: .trailing) { divider(24) }
        .accessibilityLabel("Antennas, slice \(letter)")
        .accessibilityValue(antennaWords)
        .accessibilityHint(dim ? ownerLine ?? "" : FlagControls.DesktopTip.rxAntenna)
        .accessibilityAddTraits(antennasOpen ? .isSelected : [])
        .accessibilityIdentifier("flagAntennas\(letter)")
    }

    private var antennaWords: String {
        let words = "RX \(entry.rxAntenna), TX \(entry.txAntenna)"
        return bypassOn ? words + ", BYPS on" : words
    }

    /// TX: moves transmit to this slice by the guarded path; greyed while
    /// this phone may not transmit, and dimmed on a listened slice. Where
    /// the badge offers a take it is live on every flag, a listened one
    /// too, and a press takes what the slice needs; greyed, a press says why.
    private func transmitButton(_ metrics: FlagMetrics) -> some View {
        let lit = entry.slice.txSlice
        return Button {
            switch txBadge {
            case .take?:
                takeBadge?()
            case .held(let reason)?:
                showReason?(reason)
            case .choose?, nil:
                press {
                    if let transmitReason {
                        showReason?(transmitReason)
                    } else {
                        selectForTransmit?()
                    }
                }
            }
        } label: {
            Group {
                if onAir {
                    Text("ON AIR")
                        .font(.system(size: 10 * metrics.scale, weight: .bold))
                        .foregroundStyle(.white)
                        .padding(.horizontal, 3)
                        .frame(height: 18)
                        .background(ChromeColours.txRed, in: RoundedRectangle(cornerRadius: 3))
                        .accessibilityIdentifier("flagOnAir\(letter)")
                } else {
                    Text("TX")
                        .font(.system(size: metrics.tabPoints, weight: .bold))
                        .foregroundStyle(lit ? Self.transmitOnText : Self.offText)
                }
            }
            .lineLimit(1)
            .minimumScaleFactor(0.7)
            .frame(width: FlagMetrics.txWidth, height: FlagMetrics.header)
            .modifier(OpenLine(lit: lit && !onAir, colour: Self.transmitOnLine))
            .overlay {
                if txHint {
                    RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(Self.hintRing, lineWidth: 2)
                        .padding(2)
                        .shadow(color: Self.hintRing.opacity(0.7), radius: 4)
                        .accessibilityIdentifier("txHint\(letter)")
                }
            }
            .contentShape(Rectangle())
            .opacity(transmitOpacity)
        }
        .buttonStyle(.plain)
        .overlay(alignment: .leading) { divider(24) }
        .accessibilityLabel("TX, slice \(letter)")
        .accessibilityValue(lit ? "Transmit slice" : txBadge.map(Self.badgeValue) ?? "Not the transmit slice")
        .accessibilityHint(transmitHint)
        .accessibilityIdentifier("flagTx\(letter)")
    }

    /// TX's dimming: live while its badge offers a take, the listened
    /// slice's dimming on a listened slice, dimmed while greyed.
    private var transmitOpacity: Double {
        switch txBadge {
        case .take?:
            return 1
        case .held?:
            return entry.listening ? Self.listened : Self.dimmed
        case .choose?, nil:
            return entry.listening ? Self.listened : transmitReason == nil ? 1 : Self.dimmed
        }
    }

    /// What VoiceOver says TX does: the badge's take or its reason, else
    /// the transmit choice as before.
    private var transmitHint: String {
        switch txBadge {
        case .take(let hint)?:
            return hint
        case .held(let reason)?:
            return reason
        case .choose?, nil:
            return ownerLine ?? transmitReason
                ?? Self.transmitAction(entry, selectForTransmit: selectForTransmit)?.name
                ?? FlagControls.DesktopTip.transmit
        }
    }

    /// TX's value while its badge is greyed: not available.
    static func badgeValue(_ offer: TransmitTakeModel.BadgeOffer) -> String {
        if case .held = offer {
            return "Not the transmit slice, not available"
        }
        return "Not the transmit slice"
    }

    /// The ring on the TX button the first-key refusal points at, the board's amber.
    static let hintRing = Color(red: 0xDD / 255, green: 0xBB / 255, blue: 0)

    // MARK: The owner row

    /// Who controls the slice in short words, and a compact outlined Take
    /// control: a plain button, with no press and hold, swipe or hidden tap.
    private func ownerRow(_ metrics: FlagMetrics) -> some View {
        let refused = takeRefusal != nil && !taking
        return HStack(spacing: 6) {
            Text(entry.ownerWords ?? entry.ownerLine ?? "")
                .font(.system(size: metrics.ownerPoints, weight: .semibold))
                .tracking(metrics.large ? -0.2 : 0)
                .lineSpacing(max(0, metrics.ownerLine - metrics.ownerPoints * 1.2))
                .foregroundStyle(Self.ownerText)
                .lineLimit(2)
                .minimumScaleFactor(0.8)
                .frame(maxWidth: .infinity, alignment: .leading)
                .accessibilityIdentifier("flagOwner\(letter)")
            Button {
                if let takeRefusal {
                    showReason?(takeRefusal)
                } else if !taking {
                    takeControl?()
                }
            } label: {
                Text(taking ? Self.takingControlTitle : Self.takeControlTitle)
                    .font(.system(size: metrics.tabPoints, weight: .semibold))
                    .tracking(metrics.large ? -0.25 : 0)
                    .foregroundStyle(Self.takeText)
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
                    .padding(.horizontal, metrics.large ? 4 : 7)
                    .frame(width: metrics.takeWidth, height: FlagMetrics.ownerRow)
                    .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(Self.takeEdge, lineWidth: 1))
                    .contentShape(Rectangle())
                    .opacity(refused || takeControl == nil ? Self.dimmed : 1)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(taking ? Self.takingControlTitle : "Take control of slice \(letter)")
            .accessibilityValue(refused ? "Not available" : "")
            .accessibilityHint(takeRefusal ?? "")
            .accessibilityIdentifier("takeControl\(letter)")
        }
        .padding(.leading, 6)
        .padding(.trailing, 5)
        .frame(height: FlagMetrics.ownerRow)
        .overlay(alignment: .top) { Rectangle().fill(Self.ownerHairline).frame(height: 1) }
        .overlay(alignment: .bottom) { Rectangle().fill(Self.ownerHairline).frame(height: 1) }
        .padding(.horizontal, -FlagMetrics.inset)
    }

    /// The owner row's words' colour, the board's.
    static let ownerText = Color(red: 0xC8 / 255, green: 0xD8 / 255, blue: 0xE8 / 255)

    // MARK: The frequency

    private func frequencyRow(_ metrics: FlagMetrics) -> some View {
        Button {
            press { openPad?() }
        } label: {
            Text(entry.slice.frequencyText)
                .font(.system(size: metrics.frequencyPoints, weight: .bold, design: .monospaced))
                .foregroundStyle(onAir ? BandColours.tab.opacity(0.6) : BandColours.frequency)
                .lineLimit(1)
                .minimumScaleFactor(0.6)
                .padding(.horizontal, 2)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .trailing)
                .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(BandColours.frequencyBorder, lineWidth: 1))
                .contentShape(Rectangle())
                .opacity(entry.listening ? Self.listened : 1)
        }
        .buttonStyle(.plain)
        .frame(height: FlagMetrics.frequencyHeight)
        .accessibilityLabel("Frequency, slice \(letter)")
        .accessibilityValue(entry.slice.frequencyText)
        .accessibilityHint(entry.listening ? ownerLine ?? "" : openPad == nil ? "" : "Type a frequency")
        .accessibilityIdentifier("flagFrequency\(letter)")
    }

    // MARK: The tabs

    private func tabs(_ metrics: FlagMetrics) -> some View {
        HStack(spacing: 0) {
            ForEach(Array(FlagTab.allCases.enumerated()), id: \.element) { index, kind in
                tab(kind, metrics, width: FlagMetrics.tabWidths[index], first: index == 0,
                    last: index == FlagTab.allCases.count - 1)
            }
        }
        .frame(width: metrics.width, height: FlagMetrics.tabHeight)
        .overlay(alignment: .top) {
            Rectangle().fill(Self.hairline).frame(height: 1)
        }
        .padding(.horizontal, -FlagMetrics.inset)
    }

    private func tab(_ kind: FlagTab, _ metrics: FlagMetrics, width: CGFloat, first: Bool, last: Bool) -> some View {
        let lit = openTab == kind
        // A listened slice keeps its sound; its other tabs change the slice.
        let usable = kind == .audio || !entry.listening
        return Button {
            if usable {
                toggleTab?(kind)
            } else {
                press {}
            }
        } label: {
            tabLabel(kind)
                .font(.system(size: metrics.tabPoints, weight: .bold))
                .tracking(metrics.large ? -0.25 : 0)
                .foregroundStyle(lit ? colour : kind == .mode && entry.rade != nil ? BandColours.radeMode : BandColours.tab)
                .lineLimit(1)
                .minimumScaleFactor(0.6)
                .padding(.leading, first ? 0 : 1)
                .padding(.trailing, last ? 1 : 0)
                .frame(width: width, height: FlagMetrics.tabHeight)
                .modifier(OpenLine(lit: lit, colour: colour))
                .contentShape(Rectangle())
                .opacity(usable ? 1 : Self.listened)
        }
        .buttonStyle(.plain)
        .overlay(alignment: .leading) {
            if !first {
                divider(16)
            }
        }
        .accessibilityLabel("\(kind.title), slice \(letter)")
        .accessibilityValue(kind == .mode ? entry.modeLabel : kind == .audio && entry.muted ? "Muted" : "")
        .accessibilityHint(usable ? kind.desktopTip : ownerLine ?? "")
        .accessibilityAddTraits(lit ? .isSelected : [])
        .accessibilityIdentifier("flagTab\(kind.title)\(letter)")
    }

    @ViewBuilder
    private func tabLabel(_ kind: FlagTab) -> some View {
        switch kind {
        case .audio:
            Image(systemName: entry.muted ? "speaker.slash.fill" : "speaker.wave.2.fill")
        case .dsp:
            Text("DSP")
        case .mode:
            // Purple while in RADE, as the desktop's mode tab is.
            Text(entry.modeLabel.isEmpty ? "Mode" : entry.modeLabel)
        case .xrit:
            Text("X/RIT")
        }
    }

    private func divider(_ height: CGFloat) -> some View {
        Rectangle().fill(Self.dividerColour).frame(width: 1, height: height).allowsHitTesting(false)
    }
}

/// Look 2's mark of what is open: a 2-point line along the button's foot.
private struct OpenLine: ViewModifier {
    let lit: Bool
    let colour: Color

    func body(content: Content) -> some View {
        content.overlay(alignment: .bottom) {
            if lit {
                Rectangle().fill(colour).frame(height: 2).allowsHitTesting(false)
            }
        }
    }
}

/// The flag's frame: 200 points wide and its fixed height, or, with an
/// older Core's RADE reason, as tall as its parts need.
private struct FlagFrame: ViewModifier {
    let grows: Bool
    let height: CGFloat

    func body(content: Content) -> some View {
        if grows {
            content
                .frame(width: FlagLayout.flagSize.width, alignment: .top)
                .frame(minHeight: height, alignment: .top)
                .fixedSize(horizontal: false, vertical: true)
        } else {
            content
                .frame(width: FlagLayout.flagSize.width, height: height, alignment: .top)
        }
    }
}
