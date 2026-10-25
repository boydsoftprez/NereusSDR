// NereusSDR for iOS: the Display pill in the toolbar's sheet: five tabs of this pan's display, drawn from the Core's description and live on the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusBand
import NereusMirror
import NereusModels
import SwiftUI
import UIKit

/// The pill's phone keys: Setup's own (``PhoneSetupKeys``), acting on the
/// pill's pan, with the pill's words for Cal Offset and for the Levels
/// rows while that pan transmits (spec DC5 and section 9).
@MainActor
final class DisplayPillPhoneKeys: SetupPhoneKeys {
    static let calibrationReason = "The Core calibrates for its radio."
    static let transmitLevelsReason = "Transmit has its own levels, on Transmit Display."
    static let calOffsetKey = "DisplayCalOffset"

    private let inner: PhoneSetupKeys
    private let main: MainScreenModel
    /// The pan the pill is open on.
    var pan: PanIdentity = .first

    init(inner: PhoneSetupKeys, main: MainScreenModel) {
        self.inner = inner
        self.main = main
    }

    /// Edits act on the pill's pan, never on All.
    func focus() {
        if main.displayEditScope != .pan(pan) {
            main.displayEditScope = .pan(pan)
        }
    }

    /// The display settings the pill's pan draws with now.
    var settings: BandDisplaySettings { main.displayEditingSettings }

    /// The pill's pan is drawn on the transmit scale now.
    var transmitting: Bool {
        pan == .first && main.band.transmitOverlay
    }

    private func heldForTransmit(_ key: String) -> Bool {
        transmitting && DisplayPillRows.levelsDescriptionKeys.contains(key)
    }

    func value(forPhoneKey key: String) -> SetupValue? {
        inner.value(forPhoneKey: key)
    }

    /// Why the pill holds a row the description leaves free: the 3D rows
    /// in the 2D view and Peak Hold's time while Peak Hold is off, as the
    /// desktop pill greys them.
    func pillHeldReason(_ key: String) -> String? {
        let settings = settings
        if DisplayPillReasons.threeDKeys.contains(key), settings.spectrumView == .flat {
            return DisplayPillReasons.only3D
        }
        if key == Self.peakHoldTimeKey, !settings.peakHold {
            return DisplayPillReasons.peakHoldOff
        }
        return nil
    }

    static let peakHoldTimeKey = "DisplayPeakHoldResetMs"
    static let retuneKey = "ClarityRetune"

    func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
        guard !heldForTransmit(key), pillHeldReason(key) == nil else { return false }
        focus()
        return inner.set(value, forPhoneKey: key)
    }

    var changes: AnyPublisher<Void, Never> { inner.changes }

    func canPerform(_ action: String) -> Bool {
        !heldForTransmit(action) && inner.canPerform(action)
    }

    func perform(_ action: String) -> Bool {
        guard canPerform(action) else { return false }
        focus()
        return inner.perform(action)
    }

    func perform(_ action: String, argument: String?) -> Bool {
        guard canPerform(action) else { return false }
        focus()
        return inner.perform(action, argument: argument)
    }

    func perform(_ action: String, argument: String?, descriptionVersion: Int) -> Bool {
        guard canPerform(action) else { return false }
        focus()
        return inner.perform(action, argument: argument, descriptionVersion: descriptionVersion)
    }

    // MARK: The pan's own values (spec section 5.3, DC4)

    /// A row that can hold a pan's own value: the pan's own picture row
    /// on this phone. The Core's rows, each band's rows (dB Max, dB Min
    /// and 3D Floor) and the buttons are shared and never show the dot,
    /// as Setup's Display pages mark them (DescribedPage's own-value dot).
    static func holdsOwnValue(_ control: SetupDescription.Control) -> Bool {
        guard control.owner == .pan, control.perBand == nil, control.kind != .button,
              case .phone? = control.binding else { return false }
        return true
    }

    /// The kept fields the pill's pan holds its own value for in `control`;
    /// empty for a shared row or one that follows Setup.
    func ownFields(of control: SetupDescription.Control) -> Set<String> {
        guard Self.holdsOwnValue(control), case .phone(let key)? = control.binding else { return [] }
        return main.ownDisplayFields(among: PhoneSetupKeys.displayFields(forPhoneKey: key), pan: pan)
    }

    /// The pill's pan follows Setup again in `fields` (the dot's tap).
    func followSetup(_ fields: Set<String>) {
        for field in fields.sorted() {
            main.clearOwnDisplayField(field, pan: pan)
        }
    }

    /// How many rows the pill's pan keeps its own value in, on any page:
    /// the four waterfall level mode fields are one choice.
    var ownRowCount: Int {
        let own = main.ownDisplayFields(for: pan)
        let modes = BandDisplaySettingsStore.waterfallModeFields
        return own.subtracting(modes).count + (own.isDisjoint(with: modes) ? 0 : 1)
    }

    /// Reset this pan.
    func resetPan() {
        main.resetDisplay(pan: pan)
    }

    var perBandName: String? { inner.perBandName }
    var screenRefreshRate: Int? { inner.screenRefreshRate }

    func unavailableOptions(forPhoneKey key: String) -> [Int64: String] {
        inner.unavailableOptions(forPhoneKey: key)
    }

    func unavailableReason(forPhoneKey key: String) -> String? {
        if key == Self.calOffsetKey {
            return Self.calibrationReason
        }
        if heldForTransmit(key) {
            return Self.transmitLevelsReason
        }
        if key == Self.retuneKey, settings.waterfallLevelMode != .clarity {
            // The desktop pill's words for Re-tune outside Clarity.
            return DisplayPillReasons.retune
        }
        if let reason = inner.unavailableReason(forPhoneKey: key) {
            // The desktop pill's words for the rows Track floor sets.
            if key == "DisplayGridMin", settings.gridFollowsNoiseFloor {
                return DisplayPillReasons.dbMinTracked
            }
            if key == "DisplayGridMax", settings.gridFollowsNoiseFloor, settings.gridKeepsRange {
                return DisplayPillReasons.dbMaxKept
            }
            return reason
        }
        return pillHeldReason(key)
    }
}

/// What the pill draws from: the Core's Display description (current, or
/// the last one seen while the Core is away) and the pill's own dispatcher,
/// whose phone rows keep working while the link is lost and whose Core rows
/// read "Not connected".
@MainActor
final class DisplayPillSource {
    /// The description version whose Display pages carry the pill's rows.
    static let descriptionVersion = 29
    static let category = "display"

    let pages: SetupDescribedPages
    let dispatcher: SetupControlDispatcher
    /// Held here: the dispatcher keeps its phone keys weakly.
    let keys: DisplayPillPhoneKeys

    init(pages: SetupDescribedPages, store: MirrorStore, feed: SetupDescriptionFeed, settings: SettingsProxyClient,
         commands: CommandClient, captureSender: @escaping SetupControlDispatcher.CaptureSender,
         selectedSlice: @escaping () -> Int?, selectionChanges: AnyPublisher<Int?, Never>,
         main: MainScreenModel, inner: PhoneSetupKeys) {
        self.pages = pages
        keys = DisplayPillPhoneKeys(inner: inner, main: main)
        dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                            captureSender: captureSender, selectedSlice: selectedSlice,
                                            selectionChanges: selectionChanges, phone: keys)
        dispatcher.phoneRowsWorkOffline = true
        dispatcher.offlineReason = CoreBandScale.notConnectedText
        // What the FFT tab's Size and Hz/bin show while Sizing has another
        // control in charge (description 28), as Setup's dispatcher does.
        dispatcher.runningFftSize = { [weak core = main.coreDisplay] in core?.runningFftSize }
        dispatcher.runningBinWidthHz = { [weak core = main.coreDisplay] in core?.runningBinWidthHz }
    }

    /// The Core's Display description at version 29 or later; nil from an
    /// older Core, which keeps today's sheet.
    var description: SetupDescription? {
        guard let description = pages.categories[Self.category],
              description.version >= Self.descriptionVersion else {
            return nil
        }
        return description
    }

    /// The control bound to the desktop key `key`; nil where the Core does
    /// not describe it.
    func control(for key: String) -> SetupDescription.Control? {
        description.flatMap { DisplayPillRows.control(for: key, in: $0) }
    }

    /// The fields the pill's pan keeps its own value for in the rows bound
    /// to the desktop keys `keys` (one line's slider, its switch and its
    /// swatch); empty where they all follow Setup or are shared.
    func ownFields(ofKeys keys: [String]) -> Set<String> {
        keys.reduce(into: Set<String>()) { fields, key in
            if let control = control(for: key) {
                fields.formUnion(self.keys.ownFields(of: control))
            }
        }
    }

    /// The control as the pill draws it: the dispatcher's state, with the
    /// desktop pill's words in place of Setup's "Needs a different choice
    /// above." for the row bound to the desktop key `key`.
    func state(of control: SetupDescription.Control, key: String) -> SetupControlState {
        let state = dispatcher.state(of: control, in: Self.category)
        guard state.reason == SetupControlDispatcher.dependsReason,
              let words = DisplayPillReasons.depends(DisplayPillRows.descriptionKey(for: key), settings: keys.settings,
                                                     sizing: dispatcher.sizingMode) else {
            return state
        }
        return SetupControlState(value: state.value, editable: state.editable, reason: words)
    }

    /// The label on a row's line: the description's without its colon; a
    /// per-band row names its band ("dB Max \u{00B7} 40 m"), and dB Max and
    /// dB Min add that the Core keeps them (spec DC3).
    func label(of control: SetupDescription.Control, key: String) -> String {
        Self.label(control.label, perBand: control.perBand != nil ? keys.perBandName : nil,
                   onCore: Self.coreBandKeys.contains(key))
    }

    /// dB Max and dB Min: each band's, kept on the Core.
    static let coreBandKeys: Set<String> = ["DisplayGridMax", "DisplayGridMin"]

    static func label(_ described: String, perBand band: String?, onCore: Bool) -> String {
        var label = described.trimmingCharacters(in: .whitespaces)
        if label.hasSuffix(":") {
            label.removeLast()
        }
        guard let band else { return label }
        return "\(label) \u{00B7} \(spacedBand(band))\(onCore ? ", on the Core" : "")"
    }

    /// "40m" reads "40 m", as the board writes it.
    static func spacedBand(_ band: String) -> String {
        guard let last = band.last, last == "m", band.dropLast().allSatisfy(\.isNumber), band.count > 1 else {
            return band
        }
        return "\(band.dropLast()) m"
    }
}

/// The Display pill as the toolbar's sheet (R-IOS-55, spec 5.1 and 5.2):
/// the desktop pill's five tabs, each tab's rows in the desktop's order,
/// every change live on this pan's band.
struct DisplayPill: View {
    let pan: PanIdentity
    /// "this phone" or "this iPad", for the title.
    let device: String
    @Binding var tab: DisplayPillTab
    let main: MainScreenModel
    @ObservedObject var core: CoreDisplayModel
    /// Opens the tab's Setup page for this pan.
    let onSetupDefaults: (DisplayPillTab, PanIdentity) -> Void
    /// Opens a described Setup page (the Spectrum tab's Peaks & Markers link).
    let onOpenPage: (String) -> Void
    /// The pill opened on its pan: an open Setup Display page edits that pan too.
    let onOpen: (PanIdentity) -> Void
    /// The row a finger holds, from the sheet or column the pill sits in.
    @Environment(\.displayPeek) private var peek

    init(pan: PanIdentity, device: String, tab: Binding<DisplayPillTab>, main: MainScreenModel,
         core: CoreDisplayModel, onSetupDefaults: @escaping (DisplayPillTab, PanIdentity) -> Void,
         onOpenPage: @escaping (String) -> Void = { _ in }, onOpen: @escaping (PanIdentity) -> Void = { _ in }) {
        self.pan = pan
        self.device = device
        _tab = tab
        self.main = main
        self.core = core
        self.onSetupDefaults = onSetupDefaults
        self.onOpenPage = onOpenPage
        self.onOpen = onOpen
    }

    var body: some View {
        let chrome = DisplayPillPeek.drawsChrome(held: peek.wrappedValue)
        DropSheet(title: "Display", accessibilityIdentifier: "displayPill") {
            Text("\(pan.title) \u{00B7} \(device)")
                .font(.system(size: 11, weight: .semibold))
                .tracking(0.44)
                .foregroundStyle(ChromeColours.textDim)
                .accessibilityIdentifier("displayPillPan")
        } content: {
            tabs
                .modifier(DisplayPillPeekRow(draws: chrome, held: false))
            DisplayPillRowsView(tab: tab, source: main.displayPillSource, display: main.display, core: core,
                                main: main, onOpenPage: onOpenPage)
                .environment(\.panelButtonHeight, 30)
            if let source = main.displayPillSource {
                DisplayPillFooter(tab: tab, keys: source.keys, band: main.band, onSetupDefaults: onSetupDefaults)
                    .modifier(DisplayPillPeekRow(draws: chrome, held: false))
            }
        }
        // The peek: the sheet's own face clears away with its chrome.
        .environment(\.dropSheetClear, !chrome)
        .onAppear {
            main.displayPillSource?.keys.pan = pan
            main.displayPillSource?.keys.focus()
            onOpen(pan)
        }
        .onDisappear {
            peek.wrappedValue = nil
        }
    }

    /// The pick-one tabs, the open one in the glass.
    private var tabs: some View {
        SheetGrid(columns: DisplayPillTab.allCases.count) {
            ForEach(DisplayPillTab.allCases, id: \.self) { each in
                PanelButton(label: each.title, lit: each == tab, style: .blue) {
                    tab = each
                }
                .accessibilityAddTraits(each == tab ? .isSelected : [])
                .accessibilityIdentifier("displayPillTab.\(each.rawValue)")
            }
        }
    }
}

/// One tab's lines, compact as the board draws them.
private struct DisplayPillRowsView: View {
    let tab: DisplayPillTab
    let source: DisplayPillSource?
    @ObservedObject var display: DisplaySheetModel
    @ObservedObject var core: CoreDisplayModel
    let main: MainScreenModel
    let onOpenPage: (String) -> Void

    /// The row a finger holds; while one is, only it draws.
    @Environment(\.displayPeek) private var peek

    var body: some View {
        let held = peek.wrappedValue
        ForEach(Array(DisplayPillLayout.lines(tab).enumerated()), id: \.offset) { _, line in
            self.line(line)
                .modifier(DisplayPillPeekRow(draws: DisplayPillPeek.draws(line, held: held),
                                             held: held != nil && DisplayPillPeek.key(of: line) == held))
        }
    }

    @ViewBuilder
    private func line(_ line: DisplayPillLine) -> some View {
            switch line {
            case .caption(let title):
                DisplayPillCaption(title)
            case .coreHead:
                coreHead
            case .chips(let chips, let columns):
                if let source {
                    DisplayPillChipsLine(chips: chips, columns: columns, source: source, display: display,
                                         band: main.band)
                }
            case .phone(let row):
                phone(row)
            case .link(let title, let pageId):
                Button(title) { onOpenPage(pageId) }
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(ChromeColours.accentText)
                    .buttonStyle(.plain)
                    .accessibilityIdentifier("displayPillLink.\(pageId)")
            case .fftReadout:
                // From src/gui/DisplayFftReadout.h:57-68 (the line under FFT Size)
                Text(core.fftReadoutText ?? "")
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .accessibilityIdentifier("displayPillFftReadout")
            case .segment(let key, _), .menu(let key, _, _), .slider(let key, _, _, _), .toggle(let key),
                 .picture(let key), .retune(let key):
                if let source {
                    DisplayPillSettingLine(line: line, source: source, band: main.band)
                } else {
                    DisplayPillMissingRow(key: key)
                }
            }
    }

    /// Holding or lifting the phone's slider `key`.
    private func holding(_ key: String) -> (Bool) -> Void {
        let peek = peek
        return { holding in
            peek.wrappedValue = DisplayPillPeek.held(key, holding: holding, current: peek.wrappedValue)
        }
    }

    /// The FFT tab's head: these values are the Core's, one for every device.
    private var coreHead: some View {
        // The board's head: the Core tag and its line, in place of the FFT caption.
        HStack(spacing: 6) {
            SetupTagBadge(tag: .core)
            Text(DisplayPillLayout.coreHeadText)
                .font(.system(size: 11))
                .foregroundStyle(ChromeColours.textDim)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("displayPillCoreHead")
    }

    @ViewBuilder
    private func phone(_ row: PhoneRow) -> some View {
        switch row {
        case .spectrumHeight:
            VStack(alignment: .leading, spacing: 2) {
                DisplayPillLabel(text: "Spectrum Height")
                    .frame(minHeight: 26)
                PanelSliderRow(label: nil, value: display.spectrumHeight,
                               range: DisplaySheetModel.spectrumHeightRange, accessibility: "Spectrum height",
                               format: DisplaySheetModel.percentText) { display.setSpectrumHeight($0) }
                    .onEditing(holding("phone.spectrumHeight"))
                    .accessibilityIdentifier("displayPillSpectrumHeight")
            }
        case .lookBack:
            VStack(alignment: .leading, spacing: 2) {
                DisplayPillLabel(text: "Look back")
                    .frame(minHeight: 26)
                HStack(spacing: 8) {
                    PanelSliderRow(label: nil, value: display.lookBackSeconds,
                                   range: display.lookBackRange.max > 0 ? display.lookBackRange : nil,
                                   accessibility: "Look back", format: DisplaySheetModel.lookBackText) {
                        display.setLookBack(seconds: $0)
                    }
                    .onEditing(holding("phone.lookBack"))
                    .accessibilityIdentifier("displayPillLookBack")
                    PanelButton(label: "LIVE", lit: display.lookBackSeconds == 0, style: .blue,
                                disabled: display.lookBackSeconds == 0) { display.goLive() }
                        .frame(width: 64)
                        .accessibilityIdentifier("displayPillLookBackLive")
                }
            }
        case .ctun, .dup:
            if let source {
                DisplayPillChipsLine(chips: [.phone(row)], columns: 1, source: source, display: display,
                                     band: main.band)
            }
        case .decimation:
            let range = BandDisplaySettings.decimationRange
            VStack(alignment: .leading, spacing: 2) {
                DisplayPillLabel(text: "Decimation")
                    .frame(minHeight: 26)
                    .setupGreyed(!display.gates.decimation)
                PanelSliderRow(label: nil, value: Double(main.band.settings.decimation),
                               range: StationCatalog.Range(min: Double(range.lowerBound),
                                                           max: Double(range.upperBound), step: 1),
                               accessibility: "Decimation", format: { String(Int($0.rounded())) },
                               greyed: !display.gates.decimation) { value in
                    main.displayPillSource?.keys.focus()
                    main.changeDisplay { $0.decimation = Int(value.rounded()) }
                }
                .onEditing(holding("phone.decimation"))
                .accessibilityIdentifier("displayPillDecimation")
                if !display.gates.decimation {
                    DisplayPillReason(text: CatalogFeed.needsNewerCoreText)
                }
            }
        }
    }
}

/// The footer's words (src/gui/SpectrumOverlayPanel.cpp:1738-1770, :2464)
/// and the phone's for a pan with no own values (spec section 5.3).
enum DisplayPillFooterText {
    /// Reset this pan, with how many rows the pan keeps its own value in.
    // From src/gui/SpectrumOverlayPanel.cpp:2464 ("Reset this pan (%1)")
    static func reset(ownCount: Int) -> String {
        ownCount > 0 ? "Reset this pan (\(ownCount))" : "Reset this pan"
    }

    /// Under Reset this pan, greyed while the pan has no own values.
    static func follows(_ pan: PanIdentity) -> String {
        "\(pan.title) follows Setup"
    }

    /// The link to the Setup page for the open tab, as the pill's other link reads.
    // From src/gui/SpectrumOverlayPanel.cpp:1770 ("Setup defaults \u{2192}")
    static let setupDefaults = "Setup defaults \u{203A}"
}

/// The pill's footer (spec DC1, DC4): Reset this pan on the left, greyed
/// with its reason while the pan follows Setup in every row, and Setup
/// defaults on the right, opening the open tab's Setup page on this pan.
struct DisplayPillFooter: View {
    let tab: DisplayPillTab
    let keys: DisplayPillPhoneKeys
    /// The pan's kept values change the count.
    @ObservedObject var band: BandModel
    let onSetupDefaults: (DisplayPillTab, PanIdentity) -> Void

    var body: some View {
        let count = keys.ownRowCount
        HStack(alignment: .top, spacing: 8) {
            VStack(alignment: .leading, spacing: 3) {
                Button {
                    keys.resetPan()
                } label: {
                    Text(DisplayPillFooterText.reset(ownCount: count))
                        .font(.system(size: 12, weight: .semibold))
                        .padding(.horizontal, 12)
                        .frame(minHeight: 32)
                }
                .buttonStyle(WorkspaceButtonStyle(.raised))
                .disabled(count == 0)
                .accessibilityIdentifier("displayPillReset")
                if count == 0 {
                    DisplayPillReason(text: DisplayPillFooterText.follows(keys.pan))
                        .accessibilityIdentifier("displayPillFollows")
                }
            }
            Spacer(minLength: 8)
            Button(DisplayPillFooterText.setupDefaults) {
                onSetupDefaults(tab, keys.pan)
            }
            .font(.system(size: 12, weight: .semibold))
            .foregroundStyle(ChromeColours.accentText)
            .buttonStyle(.plain)
            .frame(minHeight: 32)
            .accessibilityIdentifier("displayPillSetupDefaults")
        }
        .padding(.top, 8)
        .overlay(alignment: .top) {
            Rectangle().fill(ChromeColours.panelEdge).frame(height: 1)
        }
    }
}

/// The toolbar's Display sheet: the pill from a Core that describes the
/// version 29 Display pages, today's sheet with its note from an older one
/// (spec section 9).
struct DisplayDropSheet: View {
    static let olderCoreNote = "Needs a newer Core for the desktop's Display tabs"

    @ObservedObject var pages: SetupDescribedPages
    let source: DisplayPillSource
    let main: MainScreenModel
    @Binding var tab: DisplayPillTab
    let device: String
    /// Switches to the Setup tab.
    let openSetup: () -> Void
    let onSetupDefaults: (DisplayPillTab, PanIdentity) -> Void
    let onOpenPage: (String) -> Void
    var onOpen: (PanIdentity) -> Void = { _ in }
    /// The pan the pill acts on: the toolbar's is Pan 1, a band column's its own pan.
    var pan: PanIdentity = .first

    var body: some View {
        if source.description != nil {
            DisplayPill(pan: pan, device: device, tab: $tab, main: main, core: main.coreDisplay,
                        onSetupDefaults: onSetupDefaults, onOpenPage: onOpenPage, onOpen: onOpen)
        } else {
            DisplaySheet(model: main.display, core: main.coreDisplay, topNote: Self.olderCoreNote,
                         openSetup: openSetup)
        }
    }

    /// The word for this device in the pill's title.
    static var deviceWord: String {
        UIDevice.current.userInterfaceIdiom == .pad ? "this iPad" : "this phone"
    }
}
