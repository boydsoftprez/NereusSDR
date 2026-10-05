// NereusSDR for iOS: what a flag's buttons open: the antenna menu, the more menu and the tab panels
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// Behind the flags while a flag has a menu or panel open: a tap anywhere
/// on the band outside the flags closes it. The flags sit over it, so a
/// tap on the open tab closes it and a tap on another tab switches.
struct FlagPopoverCatcher: View {
    @ObservedObject var controls: FlagControls

    var body: some View {
        if controls.open != nil {
            Color.black.opacity(0.001)
                .contentShape(Rectangle())
                .onTapGesture { controls.close() }
                .accessibilityLabel("Close the menu")
                .accessibilityAddTraits(.isButton)
        }
    }
}

/// The open menu or panel over the band (the board's "Flag controls at
/// finger size"): a menu under the button that opened it, kept on the
/// band, or a tab's panel dropping from the flag across the band's whole
/// width, with the number pad its controls open.
struct FlagPopoverLayer: View {
    @ObservedObject var controls: FlagControls
    @ObservedObject var slices: BandSlicesModel
    /// Each full flag's rectangle and its round buttons' column, by slice.
    let flags: [Int: CGRect]
    let columns: [Int: CGRect]
    let bandSize: CGSize
    let sideways: Bool
    let large: Bool
    /// Why this phone may not transmit, if so.
    let transmitReason: String?
    /// The WIDE chip's target, under which the more menu hangs when the chip opened it.
    var wideChip: CGRect?

    /// The menus' widths, at most (the board's).
    static let antennaMenuWidth: CGFloat = 320
    static let moreMenuWidth: CGFloat = 250
    /// A menu's gap below its button, and its margin from the band's edges.
    static let belowButton: CGFloat = 4
    static let margin: CGFloat = 4

    var body: some View {
        ZStack(alignment: .topLeading) {
            if let open = controls.open, let modes = controls.modes,
               let entry = slices.entries.first(where: { $0.id == open.sliceId }),
               flags[open.sliceId] != nil || chipAnchor != nil {
                let flag = flags[open.sliceId] ?? .zero
                switch open {
                case .antennas:
                    let button = FlagMetrics(large: large).antenna.offsetBy(dx: flag.minX, dy: flag.minY)
                    menu(under: button, width: Self.antennaMenuWidth, alignRight: false) {
                        FlagAntennaMenu(controls: controls, modes: modes, entry: entry, large: large)
                    }
                case .more:
                    // Under the WIDE chip when it opened the menu, as the board has it.
                    if let chip = chipAnchor {
                        menu(under: chip, width: Self.moreMenuWidth, alignRight: true) {
                            FlagMoreMenu(controls: controls, entry: entry, rates: slices.catalog?.board.sampleRates ?? [],
                                         transmitReason: transmitReason, large: large)
                        }
                    } else if let column = columns[open.sliceId] {
                        let button = Self.moreButton(column: column, large: large)
                        menu(under: button, width: Self.moreMenuWidth, alignRight: true) {
                            FlagMoreMenu(controls: controls, entry: entry, rates: slices.catalog?.board.sampleRates ?? [],
                                         transmitReason: transmitReason, large: large)
                        }
                    }
                case .panel(_, let tab):
                    let top = Self.panelTop(flag: flag, column: columns[open.sliceId])
                    FlagPanel(controls: controls, modes: modes, rx: modes.rx, entry: entry, tab: tab,
                              sideways: sideways, large: large)
                        .frame(width: bandSize.width)
                        .frame(maxHeight: max(80, bandSize.height - top), alignment: .top)
                        .offset(y: top)
                }
                ModesPadLayer(modes: modes)
            }
        }
        .frame(width: bandSize.width, height: bandSize.height, alignment: .topLeading)
    }

    /// The WIDE chip, while the open more menu is the chip's.
    private var chipAnchor: CGRect? {
        guard case .more? = controls.open, controls.moreFromWideChip else {
            return nil
        }
        return wideChip
    }

    /// The more button: the third of the column's round buttons.
    static func moreButton(column: CGRect, large: Bool) -> CGRect {
        let size = FlagSideButtons.diameter(large: large)
        return CGRect(x: column.minX, y: column.minY + 2 * (size + FlagLayout.sideGap), width: size, height: size)
    }

    /// A panel's top: 2 points below the flag and its column.
    static func panelTop(flag: CGRect, column: CGRect?) -> CGFloat {
        max(flag.maxY, column?.maxY ?? 0) + 2
    }

    /// Where a menu sits: under its button, its left edge on the button's
    /// (its right edge on the more button's), kept on the band.
    static func menuOrigin(under button: CGRect, width: CGFloat, alignRight: Bool, bandWidth: CGFloat) -> CGPoint {
        let left = alignRight ? button.maxX - width : button.minX
        return CGPoint(x: min(max(left, margin), max(margin, bandWidth - width - margin)),
                       y: button.maxY + belowButton)
    }

    private func menu(under button: CGRect, width: CGFloat, alignRight: Bool,
                      @ViewBuilder content: @escaping () -> some View) -> some View {
        let width = min(bandSize.width - 2 * Self.margin, width)
        let origin = Self.menuOrigin(under: button, width: width, alignRight: alignRight, bandWidth: bandSize.width)
        return FlagMenuShell(content: content)
            .frame(width: width)
            .frame(maxHeight: max(80, bandSize.height - origin.y - Self.margin), alignment: .top)
            .offset(x: origin.x, y: origin.y)
    }
}

/// A menu's frame, the board's `.fbt-menu`: scrolling only when the band is too short for it.
private struct FlagMenuShell<Content: View>: View {
    @ViewBuilder let content: () -> Content

    static var background: Color { Color(red: 0x12 / 255, green: 0x1A / 255, blue: 0x28 / 255) }

    var body: some View {
        ViewThatFits(in: .vertical) {
            inner
            ScrollView { inner }
        }
        .background(Self.background, in: RoundedRectangle(cornerRadius: 8))
        .overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
        .shadow(color: .black.opacity(0.6), radius: 14, y: 10)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("flagMenu")
    }

    private var inner: some View {
        VStack(alignment: .leading, spacing: 0) {
            content()
        }
        .padding(6)
    }
}

/// The menus' shared pieces, the board's `.fbt-menu__*` and `.fbt-mitem`.
@MainActor
private enum FlagMenuChrome {
    static let itemText = ChromeColours.textBright
    static let why = ChromeColours.textDim
    static let danger = Color(red: 0xFF / 255, green: 0x80 / 255, blue: 0x80 / 255)
    static let separator = Color(red: 0x24 / 255, green: 0x34 / 255, blue: 0x46 / 255)
    static let tick = ChromeColours.accent

    static func title(_ entry: BandSlicesModel.Entry, _ text: String, large: Bool) -> some View {
        HStack(spacing: 8) {
            SliceLetterBadge(letter: entry.slice.letter, colour: BandColours.slice(entry.slice.colour))
            Text(text)
                .font(.system(size: large ? 19 : 13, weight: .bold))
                .foregroundStyle(itemText)
        }
        .padding(.horizontal, 4)
        .padding(.top, 2)
        .padding(.bottom, 6)
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isHeader)
    }

    static func separatorLine() -> some View {
        Rectangle().fill(separator).frame(height: 1).padding(.vertical, 4).padding(.horizontal, 6)
            .accessibilityHidden(true)
    }

    /// Why an item or switch is greyed, under it.
    static func reason(_ text: String, large: Bool) -> some View {
        Text(text)
            .font(.system(size: large ? 19 : 12))
            .foregroundStyle(why)
            .fixedSize(horizontal: false, vertical: true)
            .padding(.horizontal, 10)
            .padding(.bottom, 6)
    }

    /// One of a menu's rows, 44 points tall.
    static func item(_ label: String, large: Bool, danger: Bool = false, checked: Bool = false,
                     disabled: Bool = false, identifier: String, hint: String = "",
                     action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 10) {
                Text(label)
                    .font(.system(size: large ? 19 : 14))
                    .foregroundStyle(danger ? Self.danger : itemText)
                Spacer(minLength: 0)
                if checked {
                    Text("\u{2713}").font(.system(size: large ? 19 : 14, weight: .bold)).foregroundStyle(tick)
                }
            }
            .padding(.horizontal, 10)
            .frame(maxWidth: .infinity, minHeight: 44)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(disabled)
        .opacity(disabled ? VfoFlagView.dimmed : 1)
        .accessibilityHint(hint)
        .accessibilityAddTraits(checked ? .isSelected : [])
        .accessibilityIdentifier(identifier)
    }
}

/// The antenna button's menu (the board's `antennaMenu`): the RX antenna,
/// from the main TX/RX antennas and the receive-only inputs, the TX
/// antenna, BYPS, and Done. The lists and the reasons are the Modes tab's
/// (``ModesTabModel``), which follow the active slice: they show once the
/// Core has made this flag's slice active.
struct FlagAntennaMenu: View {
    @ObservedObject var controls: FlagControls
    @ObservedObject var modes: ModesTabModel
    let entry: BandSlicesModel.Entry
    let large: Bool

    var body: some View {
        FlagMenuChrome.title(entry, "Antennas", large: large)
        if controls.showsControls(for: entry.id) {
            section("RX antenna", colour: BandColours.rxAntenna)
            subsection("Main TX/RX")
            grid(modes.rxAntennas, style: .blue, reason: modes.rxAntennaReason, prefix: "flagRx") {
                modes.selectRxAntenna($0)
            }
            if !modes.rxOnlyInputs.isEmpty {
                subsection("RX only")
                grid(modes.rxOnlyInputs, style: .blue, reason: modes.rxAntennaReason, prefix: "flagRx") {
                    modes.selectRxAntenna($0)
                }
            }
            if let reason = modes.rxAntennaReason {
                FlagMenuChrome.reason(reason, large: large)
            }
            section("TX antenna", colour: BandColours.txAntenna)
            subsection("Main TX/RX")
            grid(modes.txAntennas, style: .red, reason: modes.txAntennaReason, prefix: "flagTx") {
                modes.selectTxAntenna($0)
            }
            if let reason = modes.txAntennaReason {
                FlagMenuChrome.reason(reason, large: large)
            }
            FlagMenuChrome.separatorLine()
            bypass
        } else {
            ProgressView().frame(maxWidth: .infinity, minHeight: 44)
        }
        FlagMenuChrome.item("Done", large: large, identifier: "flagAntennasDone") { controls.close() }
    }

    /// Why BYPS cannot change: a radio without the relay, or the Core's reason.
    private var bypassReason: String? {
        modes.bypassPresent ? modes.bypassReason : FlagControls.noBypassText
    }

    private var bypass: some View {
        let reason = bypassReason
        let on = modes.bypass == true && modes.bypassPresent
        let disabled = reason != nil || modes.bypass == nil
        return VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(FlagControls.bypassTitle)
                        .font(.system(size: large ? 19 : 13, weight: .bold))
                        .foregroundStyle(VfoFlagView.bypassText)
                    Text(FlagControls.bypassTip)
                        .font(.system(size: large ? 19 : 11.5))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .accessibilityHidden(true)
                Button {
                    modes.toggleBypass()
                } label: {
                    Capsule()
                        .fill(on ? Self.switchOnGround : ChromeColours.sliderTrack)
                        .overlay(Capsule().strokeBorder(on ? VfoFlagView.bypassText : ChromeColours.panelEdge, lineWidth: 1))
                        .overlay(alignment: on ? .trailing : .leading) {
                            Circle().fill(on ? VfoFlagView.bypassText : ChromeColours.textDim)
                                .frame(width: 18, height: 18).padding(3)
                        }
                        .frame(width: 52, height: 24)
                        .frame(width: 52, height: 44)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(disabled)
                .opacity(disabled ? VfoFlagView.dimmed : 1)
                .accessibilityLabel("BYPS, slice \(entry.slice.letter)")
                .accessibilityValue(on ? "On" : "Off")
                .accessibilityHint(reason ?? FlagControls.bypassTip)
                .accessibilityAddTraits(.isToggle)
                .accessibilityIdentifier("flagBypass")
            }
            .padding(4)
            .frame(minHeight: 44)
            if let reason {
                FlagMenuChrome.reason(reason, large: large)
            }
        }
    }

    static let switchOnGround = Color(red: 0x2A / 255, green: 0x2A / 255, blue: 0x1A / 255)

    private func section(_ text: String, colour: Color) -> some View {
        Text(text)
            .font(.system(size: large ? 19 : 11, weight: .bold))
            .foregroundStyle(colour)
            .padding(.horizontal, 4)
            .padding(.top, 8)
            .padding(.bottom, 4)
            .accessibilityAddTraits(.isHeader)
    }

    private func subsection(_ text: String) -> some View {
        Text(text)
            .font(.system(size: large ? 19 : 11))
            .foregroundStyle(ChromeColours.caption)
            .padding(.horizontal, 4)
            .padding(.top, 6)
            .padding(.bottom, 3)
    }

    private func grid(_ antennas: [ModesTabModel.Antenna], style: PanelButton.Style, reason: String?, prefix: String,
                      select: @escaping (ModesTabModel.Antenna) -> Void) -> some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 56), spacing: 4)], spacing: 4) {
            ForEach(antennas) { antenna in
                PanelButton(label: antenna.label, lit: antenna.lit, style: style, disabled: reason != nil) {
                    select(antenna)
                }
                .accessibilityIdentifier("\(prefix)\(antenna.label)")
            }
        }
        .environment(\.panelButtonHeight, 44)
    }
}

/// The more button's menu: the desktop flag's right-click menu, in its
/// order and words (VfoWidget.cpp): Make this the TX slice; Antenna >;
/// Sample rate >, whose rates open in place, the receiver's own ticked;
/// Diversity...; Filter policy...; Remove slice.
struct FlagMoreMenu: View {
    @ObservedObject var controls: FlagControls
    let entry: BandSlicesModel.Entry
    /// The radio's sample rates, the catalogue's.
    let rates: [Int]
    let transmitReason: String?
    let large: Bool

    var body: some View {
        let id = entry.id
        FlagMenuChrome.title(entry, "More", large: true)
        FlagMenuChrome.item(FlagControls.MoreItem.makeTransmit, large: large, disabled: transmitReason != nil,
                            identifier: "flagMoreTransmit", hint: transmitReason ?? "") {
            controls.makeTransmit(id, reason: nil)
        }
        if let transmitReason {
            FlagMenuChrome.reason(transmitReason, large: large)
        }
        FlagMenuChrome.separatorLine()
        FlagMenuChrome.item(FlagControls.MoreItem.antenna, large: large, identifier: "flagMoreAntenna") {
            controls.toggle(.antennas(id))
        }
        FlagMenuChrome.item(FlagControls.MoreItem.sampleRate, large: large, disabled: rates.isEmpty,
                            identifier: "flagMoreSampleRate") {
            controls.rateListOpen.toggle()
        }
        .accessibilityValue(controls.rateListOpen ? "Open" : "")
        if controls.rateListOpen {
            VStack(spacing: 0) {
                ForEach(rates, id: \.self) { hz in
                    FlagMenuChrome.item("\(hz / 1000) kHz", large: large,
                                        checked: entry.sampleRateHz.map { Int($0.rounded()) } == hz,
                                        identifier: "flagRate\(hz / 1000)") {
                        controls.pickSampleRate(hz, sliceId: id)
                    }
                }
            }
            .padding(.leading, 12)
        }
        FlagMenuChrome.separatorLine()
        if let model = controls.diversity {
            FlagDiversityMenuRow(model: model, controls: controls, entry: entry, large: large)
        }
        FlagMenuChrome.item(FlagControls.MoreItem.diversity, large: large, identifier: "flagMoreDiversity") {
            controls.openDiversity()
        }
        FlagMenuChrome.item(FlagControls.MoreItem.filterPolicy, large: large, disabled: true,
                            identifier: "flagMoreFilterPolicy", hint: FlagControls.filterPolicyText) {}
        FlagMenuChrome.reason(FlagControls.filterPolicyText, large: large)
        FlagFilterState(controls: controls, large: large)
        FlagMenuChrome.separatorLine()
        FlagMenuChrome.item(FlagControls.MoreItem.remove, large: large, danger: true, disabled: id == 0,
                            identifier: "flagMoreRemove", hint: id == 0 ? FlagControls.sliceAStaysOpenText : "") {
            controls.closeSlice(id)
        }
        if id == 0 {
            FlagMenuChrome.reason(FlagControls.sliceAStaysOpenText, large: large)
        }
    }
}

private struct FlagDiversityMenuRow: View {
    @ObservedObject var model: DiversityModel
    let controls: FlagControls
    let entry: BandSlicesModel.Entry
    let large: Bool
    var body: some View {
        let reason = model.actionReason(entry.diversityOn ? nil : entry.id, enabled: !entry.diversityOn)
        HStack(spacing: 10) {
            Text("Diversity").font(.system(size: 19, weight: .semibold))
                .foregroundStyle(ChromeColours.text).frame(maxWidth: .infinity, alignment: .leading)
            DiversityOnOff(isOn: entry.diversityOn, enabled: reason == nil && !model.changing,
                           identifier: "flagMoreDiversityEnabled") { controls.setDiversity(entry) }
        }
        .padding(.horizontal, 12).frame(minHeight: 44)
        if let reason { FlagMenuChrome.reason(reason, large: large) }
    }
}

/// A tab's panel, dropping from the flag across the band (the board's
/// `.fbt-panel`): a bar with the slice's letter, the tab's name and a
/// close button, over the Modes tab's sections for that tab at finger
/// size: Audio, AGC and VAX (which channel the Core feeds the slice to,
/// Off or 1 to 4); Noise; Mode and Filter; RIT and XIT with the step
/// cycle. On a slice this phone only listens to, the sound panel keeps
/// AF Gain and MUTE for this phone alone with Stop listening, and dims
/// the rest (``ListenerAudioSection``).
struct FlagPanel: View {
    @ObservedObject var controls: FlagControls
    @ObservedObject var modes: ModesTabModel
    @ObservedObject var rx: RxPanelModel
    let entry: BandSlicesModel.Entry
    let tab: FlagTab
    let sideways: Bool
    let large: Bool

    static let ground = Color(red: 12 / 255, green: 18 / 255, blue: 30 / 255).opacity(0.97)

    var body: some View {
        VStack(spacing: 0) {
            bar
            ViewThatFits(in: .vertical) {
                sections
                ScrollView { sections }
            }
            if let note = modes.note ?? rx.note {
                Text(note)
                    .font(.system(size: large ? 19 : 12))
                    .foregroundStyle(ChromeColours.refusalText)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, sideways ? 62 : 12)
                    .padding(.vertical, 8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityIdentifier("flagPanelNote")
            }
        }
        .background(Self.ground)
        .overlay(alignment: .top) {
            Rectangle().fill(BandColours.slice(entry.slice.colour)).frame(height: 2)
        }
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.panelEdge).frame(height: 1)
        }
        .shadow(color: .black.opacity(0.55), radius: 14, y: 12)
        .environment(\.panelButtonHeight, 44)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("\(tab.title), slice \(entry.slice.letter)")
        .accessibilityIdentifier("flagPanel\(tab.title)")
    }

    private var bar: some View {
        HStack(spacing: 8) {
            SliceLetterBadge(letter: entry.slice.letter, colour: BandColours.slice(entry.slice.colour))
            Text(tab.title)
                .font(.system(size: large ? 19 : 13, weight: .bold))
                .foregroundStyle(ChromeColours.text)
                .accessibilityAddTraits(.isHeader)
            Spacer(minLength: 0)
            Button {
                controls.close()
            } label: {
                Text("\u{2715}")
                    .font(.system(size: 16))
                    .foregroundStyle(ChromeColours.text)
                    .frame(width: 44, height: 44)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Close \(tab.title)")
            .accessibilityIdentifier("flagPanelClose")
        }
        .padding(.leading, sideways ? 62 : 10)
        .padding(.trailing, sideways ? 50 : 0)
        .frame(height: 44)
        .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                           .init(color: ChromeColours.titleMiddle, location: 0.5),
                                           .init(color: ChromeColours.titleBottom, location: 1)],
                                   startPoint: .top, endPoint: .bottom))
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
        }
    }

    @ViewBuilder
    private var sections: some View {
        VStack(alignment: .leading, spacing: 0) {
            if tab == .audio, let words = entry.ownerLine {
                // A listened slice: this phone's own volume and mute, and the rest dimmed.
                if controls.showsControls(for: entry.id) {
                    ListenerAudioSection(slices: controls.slices, model: modes, rx: rx,
                                         listener: ListenerAudio(slices: controls.slices, sliceId: entry.id,
                                                                 words: words) {
                                             controls.stopListening(entry.id)
                                         })
                    AgcSection(model: modes, rx: rx)
                        .listenerDim(words) { controls.slices.showReason($0) }
                }
                FlagVaxSection(controls: controls, entry: entry)
                    .listenerDim(words) { controls.slices.showReason($0) }
            } else if controls.showsControls(for: entry.id) {
                switch tab {
                case .audio:
                    AudioSection(model: modes, rx: rx)
                    AgcSection(model: modes, rx: rx)
                    FlagVaxSection(controls: controls, entry: entry)
                case .dsp:
                    NoiseSection(model: modes, rx: rx)
                case .mode:
                    ModePicker(model: modes)
                    FilterSection(model: modes, rx: rx)
                case .xrit:
                    RitXitSection(model: modes)
                    stepCycle
                }
            } else {
                ProgressView().frame(maxWidth: .infinity, minHeight: 88)
            }
        }
        .padding(.horizontal, sideways ? 52 : 0)
        .padding(.bottom, 4)
    }

    /// The X/RIT panel's foot: the desktop flag's step cycle button,
    /// "%1 Hz", one rung up the ladder a tap (``FlagControls/nextStep(after:)``).
    private var stepCycle: some View {
        let text = FlagControls.stepCycleText(entry.stepHz)
        return ModesChrome.section("Step") {
            PanelButton(label: text, lit: false, style: .blue) {
                controls.cycleStep(entry)
            }
            .accessibilityLabel("Step, slice \(entry.slice.letter)")
            .accessibilityValue(text)
            .accessibilityHint("Moves to the next step")
            .accessibilityIdentifier("flagStepCycle\(entry.slice.letter)")
        }
    }
}

/// Under Filter policy: what the desktop's Filter Policy dialog shows as
/// its current state (FilterPolicyDialog.cpp), for the first receiver
/// input, as the desktop flag's Filter policy opens it: the band-pass's
/// effective state, the Core's reason and, when a slice holds the receive
/// low-pass on that shared input, the Core's words for it, each as sent.
private struct FlagFilterState: View {
    @ObservedObject var controls: FlagControls
    let large: Bool

    var body: some View {
        if let radio = controls.store?.object(FlagControls.radioKey) {
            Reading(radio: radio, large: large)
        } else {
            Lines(lines: FlagControls.filterStateLines(nil), large: large)
        }
    }

    private struct Reading: View {
        @ObservedObject var radio: MirrorObject
        let large: Bool

        var body: some View {
            Lines(lines: FlagControls.filterStateLines(ReceiveFilterState(radio: radio.values)), large: large)
        }
    }

    private struct Lines: View {
        let lines: [String]
        let large: Bool

        var body: some View {
            VStack(alignment: .leading, spacing: 0) {
                ForEach(Array(lines.enumerated()), id: \.offset) { _, line in
                    FlagMenuChrome.reason(line, large: large)
                }
            }
            .accessibilityElement(children: .combine)
            .accessibilityIdentifier("flagFilterState")
        }
    }
}

/// The VAX panel: Off and channels 1 to 4, the one the Core feeds this
/// slice to lit (its `vax` object's channel slices). The Core takes no
/// channel choice from a phone, so the buttons are greyed with the reason.
private struct FlagVaxSection: View {
    @ObservedObject var controls: FlagControls
    let entry: BandSlicesModel.Entry

    var body: some View {
        if let object = controls.store?.object(StationVax.objectKey) {
            VaxChannels(object: object, letter: entry.slice.letter)
        } else {
            VaxButtons(lit: nil)
        }
    }

    private struct VaxChannels: View {
        @ObservedObject var object: MirrorObject
        let letter: String

        var body: some View {
            VaxButtons(lit: FlagControls.vaxChannel(values: object.values, letter: letter))
        }
    }

    private struct VaxButtons: View {
        let lit: Int?

        var body: some View {
            ModesChrome.section("VAX") {
                ModesChrome.grid(columns: 5) {
                    ForEach(0...StationVax.channelCount, id: \.self) { channel in
                        PanelButton(label: channel == 0 ? "Off" : "\(channel)", lit: lit == channel, style: .blue,
                                    disabled: true) {}
                            .accessibilityHint(FlagControls.vaxChoiceText)
                            .accessibilityIdentifier("flagVax\(channel)")
                    }
                }
                ModesChrome.note(FlagControls.vaxChoiceText)
            }
        }
    }
}

/// The number pad a panel's control opens, over the band.
private struct ModesPadLayer: View {
    @ObservedObject var modes: ModesTabModel

    var body: some View {
        ValuePadLayer(pad: modes.pad)
    }
}

/// Diversity, from a flag's more menu: the Tools tab's Diversity page in a sheet.
struct FlagDiversitySheet: ViewModifier {
    @ObservedObject var controls: FlagControls
    let app: AppModel

    func body(content: Content) -> some View {
        content.onAppear { controls.diversity = app.diversity }
            .sheet(isPresented: $controls.diversityOpen) {
            NavigationStack {
                ToolScreen(model: app.diversity) {
                    DiversityPage(model: $0)
                }
                .background(ChromeColours.panel.ignoresSafeArea())
                .navigationTitle("Diversity")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Done") { controls.diversityOpen = false }
                    }
                }
            }
        }
    }
}
