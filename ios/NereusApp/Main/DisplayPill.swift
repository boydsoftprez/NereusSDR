// NereusSDR for iOS: the Display pill in the toolbar's sheet: five tabs of this pan's display, drawn from the Core's description and live on the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusBand
import NereusMirror
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

    /// The pill's pan is drawn on the transmit scale now.
    private var transmitting: Bool {
        pan == .first && main.band.transmitOverlay
    }

    private func heldForTransmit(_ key: String) -> Bool {
        transmitting && DisplayPillRows.levelsDescriptionKeys.contains(key)
    }

    func value(forPhoneKey key: String) -> SetupValue? {
        inner.value(forPhoneKey: key)
    }

    func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
        guard !heldForTransmit(key) else { return false }
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
        return inner.unavailableReason(forPhoneKey: key)
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

    init(pan: PanIdentity, device: String, tab: Binding<DisplayPillTab>, main: MainScreenModel,
         core: CoreDisplayModel, onSetupDefaults: @escaping (DisplayPillTab, PanIdentity) -> Void,
         onOpenPage: @escaping (String) -> Void = { _ in }) {
        self.pan = pan
        self.device = device
        _tab = tab
        self.main = main
        self.core = core
        self.onSetupDefaults = onSetupDefaults
        self.onOpenPage = onOpenPage
    }

    var body: some View {
        DropSheet(title: "Display", accessibilityIdentifier: "displayPill") {
            Text("\(pan.title) \u{00B7} \(device)")
                .font(.system(size: 11, weight: .semibold))
                .tracking(0.44)
                .foregroundStyle(ChromeColours.textDim)
                .accessibilityIdentifier("displayPillPan")
        } content: {
            tabs
            DisplayPillRowsView(rows: DisplayPillRows.rows(tab), source: main.displayPillSource,
                                display: main.display, core: core, main: main, onOpenPage: onOpenPage)
        }
        .onAppear {
            main.displayPillSource?.keys.pan = pan
            main.displayPillSource?.keys.focus()
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

/// One tab's rows.
private struct DisplayPillRowsView: View {
    let rows: [DisplayPillRow]
    let source: DisplayPillSource?
    @ObservedObject var display: DisplaySheetModel
    @ObservedObject var core: CoreDisplayModel
    let main: MainScreenModel
    let onOpenPage: (String) -> Void

    var body: some View {
        ForEach(Array(rows.enumerated()), id: \.offset) { _, row in
            switch row {
            case .group(let title):
                SheetCaption(title)
            case .setting(let key):
                setting(key)
            case .phone(let phoneRow):
                phone(phoneRow)
            case .link(let title, let pageId):
                Button(title) { onOpenPage(pageId) }
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(ChromeColours.accentText)
                    .buttonStyle(.plain)
                    .accessibilityIdentifier("displayPillLink.\(pageId)")
            case .fftReadout:
                SheetNote("\(core.binWidthLabel): \(core.binWidthText ?? "")")
                    .accessibilityIdentifier("displayPillFftReadout")
            }
        }
    }

    @ViewBuilder
    private func setting(_ key: String) -> some View {
        if let source {
            DisplayPillDescribedRow(key: key, source: source, pages: source.pages)
        } else {
            Self.missingRow(key)
        }
    }

    /// A row the Core does not describe. Disabled, never hidden: the row
    /// stays in its place with its reason.
    static func missingRow(_ key: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(Self.fallbackLabel(key))
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(ChromeColours.text)
            Text(SetupControlDispatcher.needsNewerCoreReason)
                .font(.footnote)
                .foregroundStyle(WorkspacePalette.textSecondary)
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

    @ViewBuilder
    private func phone(_ row: PhoneRow) -> some View {
        switch row {
        case .spectrumHeight:
            SheetCaption("Spectrum Height (this phone)")
            PanelSliderRow(label: nil, value: display.spectrumHeight,
                           range: DisplaySheetModel.spectrumHeightRange, accessibility: "Spectrum height",
                           format: DisplaySheetModel.percentText) { display.setSpectrumHeight($0) }
                .accessibilityIdentifier("displayPillSpectrumHeight")
        case .lookBack:
            SheetCaption("Look back (this phone)")
            HStack(spacing: 8) {
                PanelSliderRow(label: nil, value: display.lookBackSeconds,
                               range: display.lookBackRange.max > 0 ? display.lookBackRange : nil,
                               accessibility: "Look back", format: DisplaySheetModel.lookBackText) {
                    display.setLookBack(seconds: $0)
                }
                .accessibilityIdentifier("displayPillLookBack")
                PanelButton(label: "LIVE", lit: display.lookBackSeconds == 0, style: .blue,
                            disabled: display.lookBackSeconds == 0) { display.goLive() }
                    .frame(width: 64)
                    .accessibilityIdentifier("displayPillLookBackLive")
            }
        case .ctun:
            HStack(spacing: 8) {
                PanelButton(label: "CTUN", lit: display.ctun, style: .dsp) { display.setCtun(!display.ctun) }
                    .frame(width: 84)
                    .accessibilityIdentifier("displayPillCtun")
                SheetNote(display.ctun ? DisplaySheetModel.ctunOnNote : DisplaySheetModel.ctunOffNote)
            }
        case .dup:
            HStack(spacing: 8) {
                // DUP, the desktop's display duplex: greyed with its reason
                // on a Core that cannot keep the receiver on the band.
                PanelButton(label: "DUP", lit: display.duplex && display.duplexAvailable, style: .dsp,
                            disabled: !display.duplexAvailable) { display.setDuplex(!display.duplex) }
                    .frame(width: 84)
                    .accessibilityLabel("Display duplex")
                    .accessibilityHint(display.duplexAvailable ? "" : DisplaySheetModel.duplexUnavailableText)
                    .accessibilityIdentifier("displayPillDup")
                SheetNote(display.duplexNote)
            }
        case .decimation:
            let range = BandDisplaySettings.decimationRange
            SetupNumberRow(title: "Decimation", value: Double(main.band.settings.decimation),
                           range: Double(range.lowerBound)...Double(range.upperBound), step: 1, unit: "",
                           enabled: display.gates.decimation, id: "displayPillDecimation") { value in
                main.displayPillSource?.keys.focus()
                main.changeDisplay { $0.decimation = Int(value) }
            }
            if !display.gates.decimation {
                SheetNote(CatalogFeed.needsNewerCoreText)
            }
        }
    }
}

/// A row the Core describes, drawn as Setup draws it; redrawn when the
/// Core's description changes.
private struct DisplayPillDescribedRow: View {
    let key: String
    let source: DisplayPillSource
    @ObservedObject var pages: SetupDescribedPages

    var body: some View {
        if let control = source.control(for: key) {
            DescribedControl(control: control, category: DisplayPillSource.category, dispatcher: source.dispatcher)
                .id(control.id)
        } else {
            DisplayPillRowsView.missingRow(key)
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

    var body: some View {
        if source.description != nil {
            DisplayPill(pan: .first, device: device, tab: $tab, main: main, core: main.coreDisplay,
                        onSetupDefaults: onSetupDefaults, onOpenPage: onOpenPage)
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
