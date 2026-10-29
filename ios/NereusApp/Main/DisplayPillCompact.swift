// NereusSDR for iOS: the Display pill's compact rows as the approved board draws them, and the desktop pill's words for a greyed row
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import NereusModels
import SwiftUI

/// One toggle in a line of toggles: a row the Core describes (by the
/// desktop's key) or one of this phone's.
enum DisplayPillChip: Equatable {
    case setting(String)
    case phone(PhoneRow)
}

/// One line of a pill tab as the board draws it (docs/architecture/
/// 2026-09-23-iphone-app-design/board.html, "Display, as on the desktop:
/// the sheet"): label and value on one line, the control under it. Keys
/// are the desktop pill's (``DisplayPillRows``).
enum DisplayPillLine: Equatable {
    /// A group's caption.
    case caption(String)
    /// The FFT tab's head: the Core tag and that one value serves every device.
    case coreHead
    /// A pick-one choice as a row of buttons; `showsLabel` false where the
    /// group's caption above already names it.
    case segment(key: String, showsLabel: Bool)
    /// A menu of choices, with an On/Off toggle beside it when `toggle` is set.
    case menu(key: String, toggle: String? = nil, label: String? = nil)
    /// A slider with its value box, an On/Off toggle on the label's line
    /// and a color swatch after the value box when set.
    case slider(key: String, toggle: String? = nil, swatch: String? = nil, label: String? = nil)
    /// A label with an On/Off toggle.
    case toggle(key: String)
    /// A line of toggles.
    case chips([DisplayPillChip], columns: Int)
    /// The background picture's Choose button.
    case picture(key: String)
    /// Re-tune, with the waterfall's note on its line.
    case retune(key: String)
    case phone(PhoneRow)
    case link(title: String, pageId: String)
    case fftReadout
}

/// Each tab's lines: every row of ``DisplayPillRows`` drawn once, merged
/// and grouped as the board draws the tab.
enum DisplayPillLayout {
    static func lines(_ tab: DisplayPillTab) -> [DisplayPillLine] {
        switch tab {
        case .spectrum: spectrum
        case .levels: levels
        case .waterfall: waterfall
        case .view: view
        case .fft: fft
        }
    }

    /// The rows the lines of `tab` draw, in the order drawn: each of
    /// ``DisplayPillRows/rows(_:)`` once.
    static func rowsDrawn(_ tab: DisplayPillTab) -> [DisplayPillRow] {
        lines(tab).flatMap { line -> [DisplayPillRow] in
            switch line {
            case .caption(let title): return [.group(title)]
            case .coreHead: return [.group(fftGroup)]
            case .segment(let key, _), .toggle(let key), .picture(let key), .retune(let key):
                return [.setting(key: key)]
            case .menu(let key, let toggle, _):
                return (toggle.map { [DisplayPillRow.setting(key: $0)] } ?? []) + [.setting(key: key)]
            case .slider(let key, let toggle, let swatch, _):
                return (toggle.map { [DisplayPillRow.setting(key: $0)] } ?? [])
                    + (swatch.map { [DisplayPillRow.setting(key: $0)] } ?? []) + [.setting(key: key)]
            case .chips(let chips, _):
                return chips.map { chip in
                    switch chip {
                    case .setting(let key): .setting(key: key)
                    case .phone(let row): .phone(row)
                    }
                }
            case .phone(let row): return [.phone(row)]
            case .link(let title, let pageId): return [.link(title: title, pageId: pageId)]
            case .fftReadout: return [.fftReadout]
            }
        }
    }

    /// The FFT tab's group, drawn as the Core head.
    // From src/gui/SpectrumOverlayPanel.cpp:3661 (the FFT page's header, with the Core's scope tag)
    static let fftGroup = "FFT"
    static let coreHeadText = "One value for every device on this Core."

    private static let spectrum: [DisplayPillLine] = [
        .caption("Trace"),
        .menu(key: "DisplaySpectrumDetector"),
        .chips([.setting("DisplayDispNormalize"), .setting("DisplayPanFill"), .setting("DisplayGradientEnabled")],
               columns: 3),
        .slider(key: "DisplayFftFillAlpha", swatch: "DisplayFillColor"),
        .menu(key: "DisplaySpectrumAveraging"),
        .slider(key: "DisplaySpectrumAverageTimeMs"),
        .slider(key: "DisplayLineWidth", swatch: "DisplayTraceColor"),
        // From src/gui/SpectrumOverlayPanel.cpp:3255 ("Peak Hold:", its On switch beside it)
        .slider(key: "DisplayPeakHoldResetMs", toggle: "DisplayPeakHoldEnabled", label: "Peak Hold"),
        .phone(.spectrumHeight),
        .slider(key: "DisplayCalOffset"),
        .caption("Grid"),
        // From src/gui/SpectrumOverlayPanel.cpp:3290 ("Grid:", its On switch beside it)
        .menu(key: "DisplayFreqGridSpacing", toggle: "DisplayGridEnabled", label: "Grid"),
        .menu(key: "DisplayFreqScaleFontPx"),
        .caption("Markers"),
        .chips([.setting("DisplayShowNoiseFloor"), .setting("DisplayShowBinWidth"),
                .setting("DisplayShowPeakValueOverlay"), .setting("DisplayShowCursorFreq")], columns: 4),
        .link(title: DisplayPillRows.peaksLinkTitle, pageId: DisplayPillRows.peaksPageId),
    ]

    private static let levels: [DisplayPillLine] = [
        .caption("Spectrum scale"),
        .segment(key: "SpectrumScaleMode", showsLabel: false),
        .slider(key: "DisplayGridMax"),
        .slider(key: "DisplayGridMin"),
        .slider(key: "DisplayNFOffsetGridFollow"),
        .toggle(key: "DisplayMaintainNFAdjustDelta"),
        .caption("Waterfall levels"),
        .segment(key: "WaterfallLevelsMode", showsLabel: false),
        .retune(key: "ClarityRetune"),
        .slider(key: "DisplayWfBlackLevel"),
        .slider(key: "DisplayWfColorGain"),
        .slider(key: "WaterfallAGCOffsetDb"),
        .slider(key: "DisplayWfLowLevel"),
        .slider(key: "DisplayWfHighLevel"),
    ]

    private static let waterfall: [DisplayPillLine] = [
        .caption("Drawing"),
        .menu(key: "DisplayWfColorScheme"),
        .slider(key: "DisplayWfUpdatePeriodMs"),
        .slider(key: "DisplayWfOpacity"),
        // From src/gui/SpectrumOverlayPanel.cpp:4072 (the threshold with NB Blank's switch beside it)
        .slider(key: "WaterfallBlankingThresholdDb", toggle: "WaterfallBlankingEnabled", label: "NB Blank"),
        .menu(key: "DisplayWaterfallAveraging"),
        .slider(key: "DisplayWaterfallAverageTimeMs"),
        .caption("Overlays"),
        .chips([.setting("WaterfallStopOnTx"), .setting("DisplayShowRxFilterOnWaterfall"),
                .setting("DisplayShowTxFilterOnRxWaterfall"), .setting("DisplayShowRxZeroLine"),
                .setting("DisplayShowTxZeroLine")], columns: 3),
        .caption("Time"),
        .segment(key: "DisplayWfTimestampPos", showsLabel: true),
        .menu(key: "DisplayWaterfallHistoryMs"),
        .phone(.lookBack),
    ]

    private static let view: [DisplayPillLine] = [
        .caption("View"),
        .segment(key: "DisplaySpectrumRenderMode", showsLabel: false),
        .caption("3D"),
        .slider(key: "Display3DFloor"),
        .slider(key: "Display3DGain"),
        .slider(key: "Display3DSpan"),
        .slider(key: "Display3DAngle"),
        .slider(key: "Display3DSpeed"),
        .toggle(key: "Display3DSliceShadow"),
        .caption("Background"),
        .chips([.setting("DisplayBackgroundEnabled")], columns: 1),
        .picture(key: "DisplayBackgroundImage"),
        .slider(key: "DisplayBackgroundOpacity", swatch: "DisplayBackgroundColor"),
        .caption("On this phone"),
        .chips([.phone(.ctun), .phone(.dup)], columns: 2),
    ]

    private static let fft: [DisplayPillLine] = [
        .coreHead,
        .segment(key: "DisplayFftSizingMode", showsLabel: true),
        .slider(key: "DisplayFftSize"),
        .slider(key: "DisplayHzPerBinTarget"),
        .fftReadout,
        .slider(key: "DisplaySpectrumFps"),
        .menu(key: "DisplayFftWindow"),
        .caption("On this phone"),
        .phone(.decimation),
    ]
}

extension EnvironmentValues {
    /// The key of the pill row a finger holds on its slider (the peek), or
    /// nil: the pill sets it, and the sheet or column it sits in draws only
    /// that row while it is set (spec section 5.3).
    @Entry var displayPeek: Binding<String?> = .constant(nil)
    /// The drop sheet draws clear: no title, face, rim or shadow, only what
    /// its content draws (the pill's peek).
    @Entry var dropSheetClear: Bool = false
}

/// The peek (spec section 5.3, the board's `.dp-sheet.is-peek`): while a
/// finger holds a slider the pill clears away but for that row, until the
/// finger lifts.
enum DisplayPillPeek {
    /// The key a line's slider peeks with; nil for a line with no slider.
    static func key(of line: DisplayPillLine) -> String? {
        switch line {
        case .slider(let key, _, _, _): key
        case .phone(.spectrumHeight): "phone.spectrumHeight"
        case .phone(.lookBack): "phone.lookBack"
        case .phone(.decimation): "phone.decimation"
        default: nil
        }
    }

    /// Whether `line` draws while the row `held` is held: every line while
    /// none is, only the held row while one is.
    static func draws(_ line: DisplayPillLine, held: String?) -> Bool {
        guard let held else { return true }
        return key(of: line) == held
    }

    /// The pill's title, tabs and footer draw only while no row is held.
    static func drawsChrome(held: String?) -> Bool {
        held == nil
    }

    /// The new held key when the slider of `key` is held or lifted.
    static func held(_ key: String, holding: Bool, current: String?) -> String? {
        if holding {
            return key
        }
        return current == key ? nil : current
    }
}

/// A line kept in its place while another row is held, drawn clear; the
/// held row on a panel of its own, so it reads over the band.
struct DisplayPillPeekRow: ViewModifier {
    let draws: Bool
    let held: Bool

    // One branch for both states: the held slider keeps its identity, so
    // the finger's drag carries on, and its line does not move. The panel
    // reaches out past the line rather than padding it.
    func body(content: Content) -> some View {
        content
            .background {
                Color.clear
                    .workspacePanel()
                    .padding(.horizontal, -8)
                    .padding(.vertical, -2)
                    .opacity(held ? 1 : 0)
            }
            .opacity(draws || held ? 1 : 0)
            .allowsHitTesting(draws || held)
            .accessibilityHidden(!(draws || held))
    }
}

/// The amber dot: the pan has its own value in this row; a tap puts the
/// row back to Setup's value (spec section 5.3, DC4).
struct DisplayPillOwnDot: View {
    let pan: PanIdentity
    /// The row's name, for VoiceOver.
    let name: String
    let id: String
    var diameter: CGFloat = 8
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Circle()
                .fill(WorkspacePalette.amberText)
                .frame(width: diameter, height: diameter)
                .frame(width: diameter + 14, height: diameter + 14)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(Self.label(pan, name: name))
        .accessibilityIdentifier(id)
    }

    /// What the dot says: the desktop's tooltip, with a tap for its click.
    // From src/gui/SpectrumOverlayPanel.cpp:2455-2456 ("Pan %1's own %2. Click the dot to follow the Setup default again.")
    static func label(_ pan: PanIdentity, name: String) -> String {
        "\(pan.title)'s own \(name). Tap the dot to follow the Setup default again."
    }
}

/// The desktop pill's words for a greyed row (src/gui/SpectrumOverlayPanel.cpp
/// `updatePillEnables`), in place of Setup's "Needs a different choice
/// above." Keys are the description's.
enum DisplayPillReasons {
    // From src/gui/SpectrumOverlayPanel.cpp:2659
    static let normalize = "Normalize to 1 Hz needs the Average, Sample or RMS detector."
    // From src/gui/SpectrumOverlayPanel.cpp:2661
    static let peakHoldOff = "Turn Peak Hold on to set how long a peak is held."
    // From src/gui/SpectrumOverlayPanel.cpp:2672
    static let gridOff = "Grid is off, so no grid lines are drawn."
    // From src/gui/SpectrumOverlayPanel.cpp:2675
    static let averagingNone = "Averaging is None, so there is no time to set."
    // From src/gui/SpectrumOverlayPanel.cpp:2678
    static let waterfallAveragingNone = "WF Avg is None, so there is no time to set."
    // From src/gui/SpectrumOverlayPanel.cpp:2680
    static let blankingOff = "NB Blank is off, so there is no threshold to set."
    // From src/gui/SpectrumOverlayPanel.cpp:2710 (Clear greyed with no picture chosen)
    static let noPicture = "No picture is chosen, so there is nothing to clear."
    /// Why Clear is greyed while the Picture row itself is not: the
    /// desktop's words with no picture chosen; else, from a Core that does
    /// not describe Clear, that it needs a newer one, as a row the Core
    /// does not describe says (``DisplayPillMissingRow``). Nil while Clear
    /// can act, or while the whole row is greyed with its own reason.
    static func clear(editable: Bool, described: Bool, picture: Bool) -> String? {
        guard editable else {
            return nil
        }
        if !picture {
            return noPicture
        }
        return described ? nil : SetupControlDispatcher.needsNewerCoreReason
    }
    // From src/gui/SpectrumOverlayPanel.cpp:2683
    static let fillOff = "Fill is off."
    // From src/gui/SpectrumOverlayPanel.cpp:2713 and :2717
    static let backgroundOff = "Background is off."
    // From src/gui/SpectrumOverlayPanel.cpp:2723
    static let only3D = "The 2D view has no 3D surface. Pick 3D Stacked Trace to use this row."
    // From src/gui/DisplayFftReadout.h:93-101 with src/gui/SpectrumOverlayPanel.cpp:2742-2745's words
    static let fftSizeAuto = "Auto is chosen, so the zoom sets the FFT size. Choose Fixed to set it yourself."
    static let fftSizeHzPerBin = "Hz/bin is chosen, so the bin width sets the FFT size. Choose Fixed to set it yourself."
    // From src/gui/DisplayFftReadout.h:118-121 with src/gui/SpectrumOverlayPanel.cpp:2749's words
    static let hzPerBin = "Choose Hz/bin above to set the bin width yourself."
    // From src/gui/SpectrumOverlayPanel.cpp:2797
    static let floorOffset = "Floor Offset is used by Track floor. Pick Track floor to use it."
    // From src/gui/SpectrumOverlayPanel.cpp:2799
    static let keepRange = "Keep range is used by Track floor. Pick Track floor to use it."
    // From src/gui/SpectrumOverlayPanel.cpp:2803
    static let dbMinTracked = "Track floor sets dB Min: the noise floor plus Floor Offset. Pick Manual to set it yourself."
    // From src/gui/SpectrumOverlayPanel.cpp:2806
    static let dbMaxKept = "Keep range is on, so dB Max moves with dB Min. Turn Keep range off to set it yourself."
    // From src/gui/SpectrumOverlayPanel.cpp:2816
    static let nfOffset = "NF Offset is used by Floor. Pick Floor to use it."
    // From src/gui/SpectrumOverlayPanel.cpp:2818
    static let lowIsScale = "Low is the spectrum's dB Min. Pick Manual to set Low here."
    // From src/gui/SpectrumOverlayPanel.cpp:2821
    static let highIsScale = "High is the spectrum's dB Max. Pick Manual to set High here."
    // From src/gui/SpectrumOverlayPanel.cpp:2822
    static let highIsClarity = "Clarity is setting High. Pick Auto, Floor or Manual to set it yourself."
    // From src/gui/SpectrumOverlayPanel.cpp:2824
    static let retune = "Re-tune makes Clarity measure the noise floor again. Pick Clarity to use it."

    /// The 3D rows, greyed in the 2D view (src/gui/SpectrumOverlayPanel.cpp:2719-2725).
    static let threeDKeys: Set<String> = ["Display3DFloorDepth", "Display3DGain", "Display3DSpan", "Display3DAngle",
                                          "Display3DSpeed", "Display3DSliceShadow"]

    /// The levels mode's name as the desktop's `kNames` says it.
    // From src/gui/SpectrumOverlayPanel.cpp:2812 (kNames: Clarity, Auto, Floor, Manual, Scale)
    static func modeName(_ mode: BandDisplaySettings.WaterfallLevelMode) -> String {
        switch mode {
        case .clarity: "Clarity"
        case .agc: "Auto"
        case .noiseFloorAgc: "Floor"
        case .manual: "Manual"
        case .scale: "Scale"
        }
    }

    /// Why the row bound to `key` cannot change while the choice it follows
    /// is another; nil for a key the pill has no words for.
    static func depends(_ key: String, settings: BandDisplaySettings,
                        sizing: StationCatalog.Display.FftSizing) -> String? {
        switch key {
        case "DisplayDispNormalize": normalize
        case "DisplayFftFillAlpha": fillOff
        case "DisplaySpectrumAverageTimeMs": averagingNone
        case "DisplayFreqGridSpacing": gridOff
        case "DisplayNFOffsetGridFollow": floorOffset
        case "DisplayMaintainNFAdjustDelta": keepRange
        case "WaterfallAGCOffsetDb": nfOffset
        case "DisplayWfLowLevel":
            settings.waterfallLevelMode == .scale ? lowIsScale
                // From src/gui/SpectrumOverlayPanel.cpp:2819
                : "\(modeName(settings.waterfallLevelMode)) is setting Low. Pick Manual to set it yourself."
        case "DisplayWfHighLevel": settings.waterfallLevelMode == .scale ? highIsScale : highIsClarity
        case "ClarityRetune": retune
        case "WaterfallBlankingThresholdDb": blankingOff
        case "DisplayWaterfallAverageTimeMs": waterfallAveragingNone
        case "DisplayBackgroundOpacity", "DisplayBackgroundColor": backgroundOff
        case "DisplayFftSize": sizing == .hzPerBin ? fftSizeHzPerBin : fftSizeAuto
        case "DisplayHzPerBinTarget": hzPerBin
        default: nil
        }
    }

    /// The waterfall's note on the Re-tune line, by levels mode.
    // From src/gui/SpectrumOverlayPanel.cpp:2840-2857 (updateWaterfallNote; kNotes at :2843-2847)
    static func waterfallNote(_ settings: BandDisplaySettings, transmitting: Bool) -> String {
        if transmitting {
            // From src/gui/SpectrumOverlayPanel.cpp:2854
            return "Transmitting: the waterfall uses the transmit levels. Transmit Display \u{2192}"
        }
        switch settings.waterfallLevelMode {
        case .clarity: return "Clarity sets Low and High from the noise floor."
        case .agc: return "Auto sets Low from the weakest signals. You set High."
        case .noiseFloorAgc: return "Low is the noise floor plus NF Offset. You set High."
        case .manual: return "You set Low and High."
        case .scale:
            // From src/gui/SpectrumOverlayPanel.cpp:2851
            return settings.gridFollowsNoiseFloor ? "Low and High are dB Min and dB Max. Track floor moves dB Min."
                : "Low and High are dB Min and dB Max, above."
        }
    }
}

/// A group's caption in the pill, bold as the board's `.dp-cap`.
struct DisplayPillCaption: View {
    let text: String

    init(_ text: String) {
        self.text = text
    }

    var body: some View {
        Text(text)
            .font(.system(size: 11, weight: .bold))
            .tracking(0.44)
            .foregroundStyle(ChromeColours.caption)
            .padding(.top, 4)
    }
}

/// A greyed row's reason, or a refusal, under its control.
struct DisplayPillReason: View {
    let text: String
    var refusal = false

    var body: some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(refusal ? ConnectChrome.badText : WorkspacePalette.textSecondary)
            .fixedSize(horizontal: false, vertical: true)
    }
}

/// A label on a row's first line.
struct DisplayPillLabel: View {
    let text: String

    var body: some View {
        Text(text)
            .font(.system(size: 12))
            .foregroundStyle(ChromeColours.text)
            .lineLimit(1)
            .minimumScaleFactor(0.8)
    }
}

/// The On/Off toggle on a row's line (the desktop's `makePillToggle`).
struct DisplayPillSwitch: View {
    let on: Bool
    let enabled: Bool
    let accessibility: String
    let action: () -> Void

    var body: some View {
        PanelButton(label: on ? DescribedControl.onText : DescribedControl.offText, lit: on, style: .dsp,
                    disabled: !enabled, action: action)
            .frame(width: 52)
            .accessibilityLabel(accessibility)
            .accessibilityValue(on ? DescribedControl.onText : DescribedControl.offText)
    }
}

/// One line of a tab that the Core describes: bound, enabled and greyed by
/// the version 29 description through the pill's dispatcher, drawn compact.
struct DisplayPillSettingLine: View {
    let line: DisplayPillLine
    let source: DisplayPillSource
    @ObservedObject var pages: SetupDescribedPages
    @ObservedObject var dispatcher: SetupControlDispatcher
    @ObservedObject var band: BandModel
    @State private var problem: String?
    @Environment(\.displayPeek) private var peek

    init(line: DisplayPillLine, source: DisplayPillSource, band: BandModel) {
        self.line = line
        self.source = source
        pages = source.pages
        dispatcher = source.dispatcher
        self.band = band
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            content
            if let problem {
                DisplayPillReason(text: problem, refusal: true)
            }
        }
    }

    @ViewBuilder
    private var content: some View {
        switch line {
        case .segment(let key, let showsLabel):
            if let control = source.control(for: key) {
                segment(key, control, showsLabel: showsLabel)
            } else {
                DisplayPillMissingRow(key: key)
            }
        case .menu(let key, let toggle, let label):
            if let control = source.control(for: key) {
                menu(key, control, toggle: toggle, label: label)
            } else {
                DisplayPillMissingRow(key: key)
            }
        case .slider(let key, let toggle, let swatch, let label):
            if let control = source.control(for: key) {
                slider(key, control, toggle: toggle, swatch: swatch, label: label)
            } else {
                DisplayPillMissingRow(key: key)
            }
        case .toggle(let key):
            if let control = source.control(for: key) {
                toggleLine(key, control)
            } else {
                DisplayPillMissingRow(key: key)
            }
        case .picture(let key):
            if let control = source.control(for: key) {
                picture(key, control)
            } else {
                DisplayPillMissingRow(key: key)
            }
        case .retune(let key):
            if let control = source.control(for: key) {
                retune(key, control)
            } else {
                DisplayPillMissingRow(key: key)
            }
        default:
            EmptyView()
        }
    }

    // MARK: Lines

    private func segment(_ key: String, _ control: SetupDescription.Control, showsLabel: Bool) -> some View {
        let state = source.state(of: control, key: key)
        let items = Self.items(dispatcher.resolved(control))
        let unavailable = dispatcher.unavailableOptions(of: control)
        let label = source.label(of: control, key: key)
        return VStack(alignment: .leading, spacing: 4) {
            if showsLabel {
                HStack(spacing: 2) {
                    ownDot([key], name: label, key: key)
                    DisplayPillCaption(label)
                }
            }
            HStack(spacing: 4) {
                if !showsLabel {
                    ownDot([key], name: label, key: key)
                }
                SheetGrid(columns: max(items.count, 1)) {
                    ForEach(items, id: \.value) { item in
                        PanelButton(label: item.label, lit: state.value?.whole == item.value, style: .blue,
                                    disabled: !state.editable || unavailable[item.value] != nil) {
                            send(control, .integer(item.value))
                        }
                        .accessibilityAddTraits(state.value?.whole == item.value ? .isSelected : [])
                    }
                }
                .accessibilityElement(children: .contain)
                .accessibilityLabel(label)
            }
            reason(state)
        }
        .accessibilityIdentifier("displayPill.\(key)")
    }

    private func menu(_ key: String, _ control: SetupDescription.Control, toggle: String?, label: String?) -> some View {
        let state = source.state(of: control, key: key)
        let items = Self.items(dispatcher.resolved(control))
        let unavailable = dispatcher.unavailableOptions(of: control)
        let title = label ?? source.label(of: control, key: key)
        let chosen = items.first { $0.value == state.value?.whole }?.label ?? DescribedControl.unavailableText
        return VStack(alignment: .leading, spacing: 2) {
            HStack(spacing: 6) {
                ownDot([toggle, key].compactMap { $0 }, name: title, key: key)
                DisplayPillLabel(text: title)
                    .setupGreyed(!state.editable)
                Spacer(minLength: 4)
                if let toggle {
                    inlineSwitch(toggle)
                }
                Menu {
                    ForEach(items, id: \.value) { item in
                        Button {
                            send(control, .integer(item.value))
                        } label: {
                            if item.value == state.value?.whole {
                                Label(item.label, systemImage: "checkmark")
                            } else {
                                Text(item.label)
                            }
                        }
                        .disabled(unavailable[item.value] != nil)
                    }
                } label: {
                    HStack(spacing: 4) {
                        Text(chosen)
                            .font(.system(size: 12, weight: .bold))
                            .lineLimit(1)
                        Image(systemName: "chevron.down")
                            .font(.system(size: 9, weight: .bold))
                    }
                    .foregroundStyle(ChromeColours.text)
                    .padding(.horizontal, 8)
                    .frame(minHeight: 30)
                    .workspaceInset()
                }
                .disabled(!state.editable || state.value == nil)
                .setupGreyed(!state.editable || state.value == nil)
                .accessibilityLabel(title)
                .accessibilityValue(chosen)
            }
            .frame(minHeight: 30)
            reason(state)
        }
        .accessibilityIdentifier("displayPill.\(key)")
    }

    private func slider(_ key: String, _ control: SetupDescription.Control, toggle: String?, swatch: String?,
                        label: String?) -> some View {
        let state = source.state(of: control, key: key)
        let drawn = dispatcher.resolved(control)
        let title = label ?? source.label(of: control, key: key)
        return VStack(alignment: .leading, spacing: 2) {
            HStack(spacing: 6) {
                ownDot([toggle, swatch, key].compactMap { $0 }, name: title, key: key)
                DisplayPillLabel(text: title)
                    .setupGreyed(!state.editable)
                Spacer(minLength: 4)
                if let toggle {
                    inlineSwitch(toggle)
                }
            }
            .frame(minHeight: 26)
            HStack(spacing: 8) {
                sliderControl(key, control, drawn: drawn, state: state, title: title)
                if let swatch {
                    swatchControl(swatch)
                }
            }
            reason(state)
        }
        .accessibilityIdentifier("displayPill.\(key)")
    }

    @ViewBuilder
    private func sliderControl(_ key: String, _ control: SetupDescription.Control, drawn: SetupDescription.Control,
                               state: SetupControlState, title: String) -> some View {
        let peek = peek
        if let options = drawn.options, !options.isEmpty {
            // The options by index, their values written (the FFT sizes).
            let index = options.firstIndex { $0.value == state.value?.whole }
            PanelSliderRow(label: nil, value: index.map(Double.init),
                           range: StationCatalog.Range(min: 0, max: Double(max(options.count - 1, 1)), step: 1),
                           accessibility: title,
                           format: { options[min(max(Int($0.rounded()), 0), options.count - 1)].label },
                           greyed: !state.editable || options.count < 2) { position in
                let chosen = options[min(max(Int(position.rounded()), 0), options.count - 1)]
                send(control, .integer(chosen.value))
            }
            .onEditing { holding in
                peek.wrappedValue = DisplayPillPeek.held(key, holding: holding, current: peek.wrappedValue)
            }
        } else {
            let range = drawn.range.map { StationCatalog.Range(min: $0.minimum, max: $0.maximum, step: $0.step) }
            let decimals = Self.decimals(control)
            PanelSliderRow(label: nil, value: state.value?.number, range: range, accessibility: title,
                           format: { DisplayPillValueText.text(key: key, number: $0, decimals: decimals,
                                                               unit: control.unit) },
                           greyed: !state.editable) { number in
                send(control, Self.value(number, control: control, decimals: decimals))
            }
            .onEditing { holding in
                peek.wrappedValue = DisplayPillPeek.held(key, holding: holding, current: peek.wrappedValue)
            }
        }
    }

    private func toggleLine(_ key: String, _ control: SetupDescription.Control) -> some View {
        let state = source.state(of: control, key: key)
        let title = source.label(of: control, key: key)
        return VStack(alignment: .leading, spacing: 2) {
            HStack(spacing: 6) {
                ownDot([key], name: title, key: key)
                DisplayPillLabel(text: title)
                    .setupGreyed(!state.editable)
                Spacer(minLength: 4)
                DisplayPillSwitch(on: state.value?.flag ?? false, enabled: state.editable && state.value != nil,
                                  accessibility: title) {
                    send(control, .bool(!(state.value?.flag ?? false)))
                }
            }
            .frame(minHeight: 30)
            reason(state)
        }
        .accessibilityIdentifier("displayPill.\(key)")
    }

    private func picture(_ key: String, _ control: SetupDescription.Control) -> some View {
        let state = source.state(of: control, key: key)
        let clear = source.control(for: DisplayPillRows.clearPictureKey)
        let picture = !source.keys.settings.backgroundImagePath.isEmpty
        return VStack(alignment: .leading, spacing: 2) {
            HStack(spacing: 6) {
                DisplayPillLabel(text: source.label(of: control, key: key))
                    .setupGreyed(!state.editable)
                Spacer(minLength: 4)
                // From src/gui/SpectrumOverlayPanel.cpp:4329 (the Choose button's words)
                PanelButton(label: "Choose...", lit: false, style: .blue, disabled: !state.editable) {
                    send(control, nil)
                }
                .frame(width: 96)
                // From src/gui/SpectrumOverlayPanel.cpp:4333-4335, 4362-4366 (Clear beside Choose: it removes the picture)
                PanelButton(label: "Clear", lit: false, style: .blue,
                            disabled: !state.editable || clear == nil || !picture) {
                    if let clear {
                        send(clear, nil)
                    }
                }
                .frame(width: 72)
                .accessibilityIdentifier("displayPill.\(DisplayPillRows.clearPictureKey)")
            }
            .frame(minHeight: 30)
            reason(state)
            if let why = DisplayPillReasons.clear(editable: state.editable, described: clear != nil,
                                                  picture: picture) {
                DisplayPillReason(text: why)
                    .accessibilityIdentifier("displayPill.\(DisplayPillRows.clearPictureKey).reason")
            }
        }
        .accessibilityIdentifier("displayPill.\(key)")
    }

    private func retune(_ key: String, _ control: SetupDescription.Control) -> some View {
        let state = source.state(of: control, key: key)
        let settings = source.keys.settings
        let clarity = settings.waterfallLevelMode == .clarity
        return VStack(alignment: .leading, spacing: 2) {
            HStack(spacing: 6) {
                // The desktop's C badge (src/gui/SpectrumOverlayPanel.cpp:3584), lit while Clarity sets the levels.
                Text("C")
                    .font(.system(size: 10, weight: .heavy))
                    .foregroundStyle(ChromeColours.panel)
                    .frame(width: 16, height: 16)
                    .background(ChromeColours.accent, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                    .opacity(clarity ? 1 : WorkspacePalette.disabledOpacity)
                    .accessibilityLabel("Clarity")
                Text(DisplayPillReasons.waterfallNote(settings, transmitting: source.keys.transmitting))
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("displayPillWaterfallNote")
                Spacer(minLength: 4)
                PanelButton(label: source.label(of: control, key: key), lit: false, style: .blue,
                            disabled: !state.editable) {
                    send(control, nil)
                }
                .frame(width: 76)
            }
            .frame(minHeight: 30)
            reason(state)
        }
        .accessibilityIdentifier("displayPill.\(key)")
    }

    // MARK: Parts

    /// The amber dot when the pill's pan keeps its own value in the rows
    /// bound to `keys` (the line's row, its switch and its swatch); a tap
    /// clears those fields, and the line shows Setup's value again.
    @ViewBuilder
    private func ownDot(_ keys: [String], name: String, key: String) -> some View {
        let fields = source.ownFields(ofKeys: keys)
        if !fields.isEmpty {
            let pill = source.keys
            DisplayPillOwnDot(pan: pill.pan, name: name, id: "displayPillOwn.\(key)") {
                pill.followSetup(fields)
            }
        }
    }

    @ViewBuilder
    private func inlineSwitch(_ key: String) -> some View {
        if let control = source.control(for: key) {
            let state = source.state(of: control, key: key)
            DisplayPillSwitch(on: state.value?.flag ?? false, enabled: state.editable && state.value != nil,
                              accessibility: source.label(of: control, key: key)) {
                send(control, .bool(!(state.value?.flag ?? false)))
            }
            .accessibilityIdentifier("displayPill.\(key)")
        }
    }

    @ViewBuilder
    private func swatchControl(_ key: String) -> some View {
        if let control = source.control(for: key) {
            let state = source.state(of: control, key: key)
            let opaque: Bool = {
                if case .phone(let name)? = control.binding { return PhoneSetupKeys.opaqueColourKeys.contains(name) }
                return false
            }()
            ColorPicker(source.label(of: control, key: key),
                        selection: Binding(get: { BandColours.withAlpha(state.value?.text ?? "#000000FF") },
                                           set: { send(control, .text(DisplayOnThisPhonePage.hexWithAlpha($0))) }),
                        supportsOpacity: !opaque)
                .labelsHidden()
                .frame(width: 30, height: 26)
                .disabled(!state.editable || state.value == nil)
                .setupGreyed(!state.editable || state.value == nil)
                .accessibilityIdentifier("displayPill.\(key)")
        }
    }

    @ViewBuilder
    private func reason(_ state: SetupControlState) -> some View {
        if let reason = state.reason {
            DisplayPillReason(text: reason)
        }
    }

    private func send(_ control: SetupDescription.Control, _ value: SetupValue?) {
        let dispatcher = dispatcher
        Task { @MainActor in
            problem = DescribedControl.problem(await dispatcher.edit(control, in: DisplayPillSource.category, to: value))
        }
    }

    // MARK: Values

    /// One choice of a menu or a row of buttons.
    struct Item: Hashable {
        let value: Int64
        let label: String
    }

    static func items(_ control: SetupDescription.Control) -> [Item] {
        control.options.map { $0.map { Item(value: $0.value, label: $0.label) } }
            ?? (control.choices ?? []).enumerated().map { Item(value: Int64($0.offset), label: $0.element) }
    }

    /// The places a number shows: the Core's, else its step's.
    static func decimals(_ control: SetupDescription.Control) -> Int {
        if let decimals = control.decimals {
            return decimals
        }
        guard let step = control.range?.step, step > 0 else {
            return 0
        }
        var places = 0
        var scaled = step
        while places < 6, scaled.rounded() != scaled {
            scaled *= 10
            places += 1
        }
        return places
    }

    /// The value box's words: the number with its unit, a percent sign
    /// against the number.
    static func valueText(_ number: Double, decimals: Int, unit: String?) -> String {
        let text = String(format: "%.\(max(0, decimals))f", number)
        guard let unit, !unit.isEmpty else { return text }
        return unit == "%" ? "\(text)%" : "\(text) \(unit)"
    }

    /// The value a slider sends: whole, or held to its places.
    static func value(_ number: Double, control: SetupDescription.Control, decimals: Int) -> SetupValue {
        if control.kind == .decimal || decimals > 0 {
            return .decimal(DescribedControl.rounded(number, decimals: decimals))
        }
        return .integer(Int64(number.rounded()))
    }
}

/// A line of toggles: the Core's rows and this phone's CTUN and DUP, each
/// lit green while on, the first greyed one's reason under the line.
struct DisplayPillChipsLine: View {
    let chips: [DisplayPillChip]
    let columns: Int
    let source: DisplayPillSource
    @ObservedObject var pages: SetupDescribedPages
    @ObservedObject var dispatcher: SetupControlDispatcher
    @ObservedObject var display: DisplaySheetModel
    /// The pan's kept values move the chips' dots.
    @ObservedObject var band: BandModel
    @State private var problem: String?

    init(chips: [DisplayPillChip], columns: Int, source: DisplayPillSource, display: DisplaySheetModel,
         band: BandModel) {
        self.chips = chips
        self.columns = columns
        self.source = source
        pages = source.pages
        dispatcher = source.dispatcher
        self.display = display
        self.band = band
    }

    private struct Chip {
        let id: String
        let label: String
        let on: Bool
        let enabled: Bool
        let reason: String?
        /// The fields the pan keeps its own value for in this chip's row.
        var own: Set<String> = []
        let action: () -> Void
    }

    var body: some View {
        let drawn = chips.map(chip)
        VStack(alignment: .leading, spacing: 3) {
            SheetGrid(columns: columns) {
                ForEach(drawn, id: \.id) { chip in
                    PanelButton(label: chip.label, lit: chip.on, style: .dsp, disabled: !chip.enabled,
                                action: chip.action)
                        .accessibilityValue(chip.on ? DescribedControl.onText : DescribedControl.offText)
                        .accessibilityHint(chip.reason ?? "")
                        .accessibilityIdentifier(chip.id)
                        .overlay(alignment: .topTrailing) {
                            // The board's chip dot (`.dp-cdot`), in the chip's corner.
                            if !chip.own.isEmpty {
                                let pill = source.keys
                                DisplayPillOwnDot(pan: pill.pan, name: chip.label,
                                                  id: chip.id.replacingOccurrences(of: "displayPill.",
                                                                                   with: "displayPillOwn."),
                                                  diameter: 7) {
                                    pill.followSetup(chip.own)
                                }
                            }
                        }
                }
            }
            if let reason = drawn.first(where: { $0.reason != nil })?.reason {
                DisplayPillReason(text: reason)
            }
            if let problem {
                DisplayPillReason(text: problem, refusal: true)
            }
        }
    }

    private func chip(_ chip: DisplayPillChip) -> Chip {
        switch chip {
        case .setting(let key):
            guard let control = source.control(for: key) else {
                return Chip(id: "displayPill.\(key)", label: DisplayPillMissingRow.fallbackLabel(key), on: false,
                            enabled: false, reason: SetupControlDispatcher.needsNewerCoreReason, action: {})
            }
            let state = source.state(of: control, key: key)
            let on = state.value?.flag ?? false
            return Chip(id: "displayPill.\(key)", label: source.label(of: control, key: key), on: on,
                        enabled: state.editable && state.value != nil, reason: state.reason,
                        own: source.keys.ownFields(of: control)) {
                let dispatcher = dispatcher
                Task { @MainActor in
                    problem = DescribedControl.problem(await dispatcher.edit(control, in: DisplayPillSource.category,
                                                                              to: .bool(!on)))
                }
            }
        case .phone(.ctun):
            return Chip(id: "displayPillCtun", label: "CTUN", on: display.ctun, enabled: true, reason: nil) {
                display.setCtun(!display.ctun)
            }
        case .phone(.dup):
            // DUP, the desktop's display duplex: greyed with its reason on a
            // Core that cannot keep the receiver on the band.
            return Chip(id: "displayPillDup", label: "DUP", on: display.duplex && display.duplexAvailable,
                        enabled: display.duplexAvailable,
                        reason: display.duplexAvailable ? nil : DisplaySheetModel.duplexUnavailableText) {
                display.setDuplex(!display.duplex)
            }
        case .phone:
            return Chip(id: "displayPillPhone", label: "", on: false, enabled: false, reason: nil, action: {})
        }
    }
}

/// A row the Core does not describe. Disabled, never hidden: the row stays
/// in its place with its reason.
struct DisplayPillMissingRow: View {
    let key: String

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            DisplayPillLabel(text: Self.fallbackLabel(key))
            DisplayPillReason(text: SetupControlDispatcher.needsNewerCoreReason)
                .accessibilityIdentifier("displayPill.\(key).reason")
        }
        .opacity(WorkspacePalette.disabledOpacity)
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("displayPill.\(key)")
    }

    /// A label for a row the Core does not describe: the desktop key in
    /// words ("DisplayWfBlackLevel" reads "Wf Black Level").
    static func fallbackLabel(_ key: String) -> String {
        let name = key.hasPrefix("Display") ? String(key.dropFirst("Display".count)) : key
        var words = ""
        for character in name {
            if character.isUppercase, let last = words.last, !last.isUppercase, last != " " {
                words.append(" ")
            }
            words.append(character)
        }
        return words
    }
}

/// The pill's value words where the desktop pill writes its own
/// (`SpectrumOverlayPanel::buildPillLevelsPage` and `buildPillView3DPage`)
/// rather than the number with the description's unit.
@MainActor
enum DisplayPillValueText {
    static func text(key: String, number: Double, decimals: Int, unit: String?) -> String {
        let whole = Int(number.rounded())
        switch key {
        case "Display3DGain", "Display3DSpan", "Display3DAngle":
            // From src/gui/SpectrumOverlayPanel.cpp:4236, 4260-4284 (the plain number, no unit)
            return String(whole)
        case "Display3DSpeed":
            // From src/gui/SpectrumOverlayPanel.cpp:4286-4293 (0 reads "Match")
            return whole == 0 ? "Match" : String(whole)
        case "DisplayWfBlackLevel":
            // From src/gui/SpectrumOverlayPanel.cpp:3601-3613 (whole dB from the noise floor N, a half dB reads down)
            let offset = Int((Double(whole - blackLevelAtNoise) * blackLevelDbPerStep).rounded(.down))
            return "N\(offset < 0 ? "-" : "+")\(abs(offset)) dB"
        case "DisplayWfColorGain":
            // From src/gui/SpectrumOverlayPanel.cpp:3616-3621 (qRound(colorSpanDb(v)) dB)
            return "\(Int((colorSpanAtZeroGainDb - Double(whole) * colorSpanDbPerGainStep).rounded())) dB"
        default:
            return DisplayPillSettingLine.valueText(number, decimals: decimals, unit: unit)
        }
    }

    // From src/core/spectrum/ClarityLevels.h:62 (kBlackLevelAtNoise)
    static let blackLevelAtNoise = 50
    // From src/core/spectrum/ClarityLevels.h:68 (kBlackLevelDbPerStep 0.5f)
    static let blackLevelDbPerStep = 0.5
    // From src/core/spectrum/ClarityLevels.h:77 (kColorSpanAtZeroGainDb 80.0f)
    static let colorSpanAtZeroGainDb = 80.0
    // From src/core/spectrum/ClarityLevels.h:78 (kColorSpanDbPerGainStep 0.7f)
    static let colorSpanDbPerGainStep = 0.7
}
