// NereusSDR for iOS: one Setup page the Core describes, drawn from its description with each control live
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI
import UniformTypeIdentifiers
import ImageIO
import UIKit

/// A page the Core describes (R-IOS-18): its sections and controls in the
/// desktop's order, with the Core's labels, ranges, units and help. Each
/// control writes to its owner through ``SetupControlDispatcher``; one
/// that cannot change now stays visible, greyed, with its reason.
struct DescribedPage: View {
    @ObservedObject var pages: SetupDescribedPages
    @ObservedObject var dispatcher: SetupControlDispatcher
    let category: String
    let pageId: String
    var specialized: SetupSpecializedPanels = .none
    /// Calibration's Level Cal group, on that page only (nil elsewhere).
    var levelCal: LevelCalModel?
    var main: MainScreenModel? = nil
    /// Display's version 29 pages: the pans the strip edits (``SetupRouter/displayScope``)
    /// and the radio's pan limit; nil elsewhere.
    var displayStrip: DisplayStripContext? = nil
    @State var profileOwner = UUID()
    @State private var paPresentation: UUID?

    /// The words for a page the Core no longer describes.
    static let goneText = "This page is no longer on the Core."

    private func openProfiles(_ page: SetupDescription.Page) {
        if let control = page.sections.flatMap(\.controls).first(where: { $0.modern?.profileUnsavedChanges != nil }) {
            dispatcher.txProfiles.open(control, in: category)
        }
    }

    /// Display at version 29: the desktop's eight pages with this phone's rows on them.
    private var displayV29: Bool {
        category == "display" && (pages.categories[category]?.version ?? 0) >= SetupTree.displayPagesVersion
    }

    /// The page's sections as drawn: on Display at version 29 without the
    /// desktop's thread priority and monitor rate (spec section 7.2).
    private func sections(of page: SetupDescription.Page)
        -> [(section: SetupDescription.Section, controls: [SetupDescription.Control])] {
        guard displayV29 else { return page.sections.map { ($0, $0.controls) } }
        return page.sections.compactMap { section in
            let kept = section.controls.filter { control in
                guard case .phone(let key)? = control.binding else { return true }
                return !DisplayPhoneGroups.leftOff.contains(key)
            }
            // A group left with none of its rows is left out with them.
            return kept.isEmpty && !section.controls.isEmpty ? nil : (section, kept)
        }
    }

    /// This phone's groups on this page, placed `where`.
    private func phoneGroups(_ placement: DisplayPhoneGroups.Placement) -> [DisplayPhoneGroups.Group] {
        // Appearance holds this phone's strongest-signal color beside the
        // spectrum's, once the Core's Display pages are version 29.
        let appearanceV29 = category == "appearance"
            && (pages.categories["display"]?.version ?? 0) >= SetupTree.displayPagesVersion
        guard displayV29 || appearanceV29 else { return [] }
        return DisplayPhoneGroups.groups(on: pageId).filter { $0.placement == placement }
    }

    var body: some View {
        if let page = pages.page(pageId, in: category) {
            SetupPageContainer {
                if let main, !PhoneSetupKeys.pictureFields(on: page).isEmpty {
                    DisplayV29Strip(main: main, slices: main.slices, page: page,
                                    context: displayStrip ?? .following(main))
                }
                if let main, page.sections.flatMap(\.controls).contains(where: { $0.perBand != nil }) {
                    DisplayV29BandHeader(main: main)
                }
                // A Core below Setup description 23 has no Level Cal
                // section: the group comes first, as on the desktop.
                if let levelCal, !page.sections.contains(where: { $0.title == LevelCalSection.title }) {
                    SetupPanelSection {
                        LevelCalSection.Top(model: levelCal)
                        LevelCalSection.Run(model: levelCal)
                    } header: {
                        Text(LevelCalSection.title)
                    }
                }
                ForEach(Array(sections(of: page).enumerated()), id: \.offset) { _, entry in
                    let section = entry.section
                    let group = levelCal.flatMap { section.title == LevelCalSection.title ? $0 : nil }
                    if let main {
                        ForEach(phoneGroups(.before(section.title)), id: \.title) { phone in
                            DisplayPhoneGroupSection(group: phone, main: main)
                        }
                    }
                    SetupPanelSection {
                        if let group {
                            LevelCalSection.Top(model: group)
                        }
                        ForEach(entry.controls, id: \.id) { control in
                            if let main, control.owner != nil {
                                DisplayV29Control(control: control, category: category, dispatcher: dispatcher,
                                    main: main, band: main.band, specialized: specialized, profileOwner: profileOwner)
                            } else {
                                DescribedControl(control: control, category: category, dispatcher: dispatcher,
                                                 specialized: specialized, profileOwner: profileOwner)
                            }
                        }
                        if let group {
                            LevelCalSection.Run(model: group)
                        }
                        if let main {
                            ForEach(phoneGroups(.into(section.title)), id: \.title) { phone in
                                DisplayPhoneGroupRows(group: phone, main: main)
                            }
                        }
                        if let note = section.note {
                            Text(note).font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
                        }
                    } header: {
                        if !section.title.isEmpty {
                            SetupPanelTitle(title: section.title,
                                            tag: section.owner.map { $0 == .station ? .core : .thisPhone })
                        }
                    }
                    if let main {
                        ForEach(phoneGroups(.after(section.title)), id: \.title) { phone in
                            DisplayPhoneGroupSection(group: phone, main: main)
                        }
                    }
                }
                if let main {
                    ForEach(phoneGroups(.end), id: \.title) { phone in
                        DisplayPhoneGroupSection(group: phone, main: main)
                    }
                }
            }
            .onAppear { openProfiles(page) }
            .onChange(of: page) { _, updated in openProfiles(updated) }
            .onDisappear {
                if page.sections.flatMap(\.controls).contains(where: { $0.modern?.profileUnsavedChanges != nil }) {
                    dispatcher.txProfiles.retire(owner: profileOwner)
                }
            }
            .levelCalAlerts(levelCal)
            .modifier(DisplayV29OptionalPresentation(main: main))
            .setupPageTitle(page.title)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    SetupTagBadge(tag: SetupTag(page.whereOwned))
                }
            }
            .accessibilityIdentifier(page.id)
            .onAppear {
                if category == "pa", pageId == "pa.values", let model = specialized.paValues {
                    paPresentation = model.beginPresentation()
                }
            }
            .onDisappear {
                if let token = paPresentation { specialized.paValues?.endPresentation(token) }
                paPresentation = nil
            }
        } else {
            SetupPageContainer {
                Text(Self.goneText)
                    .foregroundStyle(WorkspacePalette.textSecondary)
            }
        }
    }
}


/// Where Display's strip reads and keeps its choice: the Setup router's
/// scope (``SetupRouter/displayScope``), the radio's pan limit, and
/// whether this device is an iPhone.
struct DisplayStripContext {
    let scope: Binding<MainScreenModel.DisplayEditScope>
    let panLimit: Int
    let isPhone: Bool

    /// A strip that keeps its choice on the model itself, for a page shown
    /// outside the Setup tab.
    @MainActor
    static func following(_ main: MainScreenModel) -> DisplayStripContext {
        DisplayStripContext(scope: Binding(get: { main.displayEditScope }, set: { main.displayEditScope = $0 }),
                            panLimit: 1, isPhone: UIDevice.current.userInterfaceIdiom == .phone)
    }
}

/// How many rows of `page` keep their own value for a pan, given the
/// fields it keeps (`own`): each pan-owned row of this phone's, and the
/// background picture.
@MainActor
enum DisplayPageOwnRows {
    static func count(on page: SetupDescription.Page, own: Set<String>) -> Int {
        var count = 0
        for control in page.sections.flatMap(\.controls) {
            guard control.owner == .pan, control.perBand == nil, control.kind != .button,
                  case .phone(let key)? = control.binding else { continue }
            if !own.isDisjoint(with: PhoneSetupKeys.displayFields(forPhoneKey: key)) { count += 1 }
        }
        if own.contains("backgroundImagePath") { count += 1 }
        return count
    }
}

/// The strip on a Display page, live on the model: the pans' own rows on
/// this page, Pan 1's slices, and the choice carried to the model so every
/// row edits the pans chosen.
private struct DisplayV29Strip: View {
    @ObservedObject var main: MainScreenModel
    @ObservedObject var slices: BandSlicesModel
    let page: SetupDescription.Page
    let context: DisplayStripContext
    private var fields: Set<String> { PhoneSetupKeys.pictureFields(on: page) }

    /// The pans open on this device: Pan 1, the band, with its slices.
    private var slicesByPan: [PanIdentity: [String]] {
        let letters = slices.entries
            .filter { entry in entry.panKey.flatMap(PanIdentity.init(corePanKey:)).map { $0 == .first } ?? true }
            .map(\.slice.letter)
        return [.first: letters]
    }

    var body: some View {
        DisplaySettingsStrip(
            pageId: page.id, scope: context.scope, panLimit: context.panLimit, isPhone: context.isPhone,
            slicesByPan: slicesByPan,
            ownFieldCount: { pan in
                DisplayPageOwnRows.count(on: page, own: main.ownDisplayFields(among: fields, pan: pan))
            },
            reset: { pan in main.resetDisplayPicture(fields: fields, pan: pan) },
            useOnAll: { pan in main.copyOwnDisplayPictureToMaster(fields: fields, pan: pan) })
        .onAppear { follow(context.scope.wrappedValue) }
        .onChange(of: context.scope.wrappedValue) { _, scope in follow(scope) }
    }

    /// Every row on the page edits the pans the strip has chosen.
    private func follow(_ scope: MainScreenModel.DisplayEditScope) {
        if main.displayEditScope != scope {
            main.displayEditScope = scope
        }
    }
}

/// One of this phone's groups on a Display page (``DisplayPhoneGroups``).
private struct DisplayPhoneGroupSection: View {
    let group: DisplayPhoneGroups.Group
    let main: MainScreenModel

    var body: some View {
        SetupPanelSection {
            DisplayPhoneGroupRows(group: group, main: main)
        } header: {
            SetupPanelTitle(title: group.title, tag: group.tag)
        }
        .accessibilityIdentifier("phoneGroup." + group.title)
    }
}

/// The rows of one of this phone's groups, as the older "On this phone"
/// page drew them, each with its own words and reasons.
private struct DisplayPhoneGroupRows: View {
    let group: DisplayPhoneGroups.Group
    @ObservedObject var main: MainScreenModel
    @ObservedObject var display: DisplaySheetModel
    @ObservedObject var pan: PanSheetModel
    @ObservedObject var band: BandModel

    init(group: DisplayPhoneGroups.Group, main: MainScreenModel) {
        self.group = group
        self.main = main
        display = main.display
        pan = main.pan
        band = main.band
    }

    var body: some View {
        ForEach(group.rows, id: \.self) { row in
            self.row(row)
        }
    }

    @ViewBuilder private func row(_ id: String) -> some View {
        switch id {
        case "setupSpectrumHeight":
            SetupNumberRow(title: "Spectrum height", value: display.spectrumHeight,
                           range: BandDisplaySettings.spectrumShareRange, step: 5, unit: "%",
                           id: "setupSpectrumHeight") { display.setSpectrumHeight($0) }
        case "extendedView":
            Toggle("Extended view", isOn: Binding(get: { pan.extendedViewOn }, set: { _ in pan.toggleExtendedView() }))
                .disabled(!pan.extendedViewAvailable)
                .accessibilityIdentifier("extendedView")
            Text(pan.extendedViewAvailable ? "The radio's full width either side of the band."
                 : "Extended view: \(CatalogFeed.needsNewerCoreText)")
                .font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
        case "setupCtun":
            SetupSwitchRow(title: "CTUN", detail: display.ctun ? DisplaySheetModel.ctunOnNote : DisplaySheetModel.ctunOffNote,
                           isOn: Binding(get: { display.ctun }, set: { display.setCtun($0) }), id: "setupCtun")
        case "bandPlanPicker":
            BandPlanPicker(model: display)
        case "bandPlanSize":
            Picker("Size", selection: Binding(get: { band.settings.bandPlanSize },
                                              set: { display.setBandPlanSize($0) })) {
                ForEach(BandPlanSize.allCases, id: \.self) { size in
                    Text(size.label).tag(size)
                }
            }
            .pickerStyle(.segmented)
            .accessibilityLabel("Band plan size")
            .accessibilityIdentifier("bandPlanSize")
            Text(DisplayOnThisPhonePage.planNote)
                .font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
        case "setupDuplex":
            SetupSwitchRow(title: "DUP", detail: display.duplexNote,
                           isOn: Binding(get: { display.duplex && display.duplexAvailable },
                                         set: { display.setDuplex($0) }),
                           enabled: display.duplexAvailable, id: "setupDuplex")
        case "peakValueColour":
            ColorPicker("Strongest signal",
                        selection: Binding(get: { BandColours.withAlpha(main.displayEditingSettings.peakValueColour) },
                                           set: { colour in
                main.changeDisplay { $0.peakValueColour = DisplayOnThisPhonePage.hexWithAlpha(colour) }
            }), supportsOpacity: true)
            .accessibilityIdentifier("peakValueColour")
        default:
            EmptyView()
        }
    }
}

/// The catalogue supplies band identities; this editing choice is independent
/// of live tuning. A focused pan's band stays authoritative.
private struct DisplayV29BandHeader: View {
    @ObservedObject var main: MainScreenModel
    var body: some View {
        SetupPanelSection {
            if main.displayEditScope == .all {
                Picker("Band", selection: Binding(
                    get: { main.displayEditingBand ?? "" },
                    set: { main.displayBandChoice = $0 })) {
                    ForEach(main.catalogFeed.catalog?.bands ?? [], id: \.id) { band in
                        Text(band.label).tag(String(band.id))
                    }
                }
                .accessibilityIdentifier("displayEditingBand")
                .setupMenuRow("Band")
                Text("Edits the kept band values without tuning the radio.")
                    .font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
            } else {
                LabeledContent("Band", value: main.displayEditingBand.flatMap(Int.init)
                    .flatMap(SetupDescription.bandName) ?? "—")
            }
        }
    }
}

/// Effective numbers observe runtime Band state, independently of kept values.
private struct DisplayV29Control: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher
    @ObservedObject var main: MainScreenModel
    @ObservedObject var band: BandModel
    var specialized: SetupSpecializedPanels
    let profileOwner: UUID?
    /// The live numbers move with the Core's frames, published apart from the band.
    @ObservedObject private var readouts: BandModel.EffectiveReadouts

    init(control: SetupDescription.Control, category: String, dispatcher: SetupControlDispatcher,
         main: MainScreenModel, band: BandModel, specialized: SetupSpecializedPanels, profileOwner: UUID?) {
        self.control = control
        self.category = category
        self.dispatcher = dispatcher
        self.main = main
        self.band = band
        self.specialized = specialized
        self.profileOwner = profileOwner
        readouts = band.effectiveReadouts
    }
    /// The pan whose own value this row holds, marked with the amber dot.
    private var ownPan: PanIdentity? {
        guard case .pan(let pan) = main.displayEditScope, control.owner == .pan, control.perBand == nil,
              control.kind != .button, case .phone(let key)? = control.binding else { return nil }
        return main.ownDisplayFields(among: PhoneSetupKeys.displayFields(forPhoneKey: key), pan: pan).isEmpty
            ? nil : pan
    }
    var body: some View {
        HStack(spacing: 8) {
            if let pan = ownPan {
                Circle().fill(WorkspacePalette.amberText).frame(width: 5, height: 5)
                    .accessibilityLabel("\(pan.title)'s own value")
                    .accessibilityIdentifier(control.id + ".own")
            }
            DescribedControl(control: control, category: category, dispatcher: dispatcher,
                             specialized: specialized, profileOwner: profileOwner)
                .effectiveDisplayText(readout)
        }
    }
    private var readout: String? {
        guard let readout = control.readout else { return nil }
        guard let value = main.displayEffectiveValue(readout) else { return "—" }
        return String(format: "%.1f dBm", value)
    }
}

extension DescribedControl {
    func effectiveDisplayText(_ text: String?) -> DescribedControl {
        var copy = self
        copy.effectiveText = text
        return copy
    }
}

private struct DisplayV29OptionalPresentation: ViewModifier {
    let main: MainScreenModel?
    @ViewBuilder func body(content: Content) -> some View {
        if let main { content.modifier(DisplayV29Presentation(main: main, model: main.display)) }
        else { content }
    }
}

/// The actions stay local to this device; neither Custom definition nor a
/// private picture URL enters a Core command or display subscription.
struct DisplayV29Presentation: ViewModifier {
    let main: MainScreenModel
    @ObservedObject var model: DisplaySheetModel
    @State private var pictureProblem: String?

    func body(content: Content) -> some View {
        content
            .sheet(item: $model.gradientEditor) { editor in
                DisplayGradientEditor(main: main, band: main.band, editor: editor)
                    .workspacePresentationRoot()
            }
            .fileImporter(isPresented: $model.choosingBackgroundPicture,
                          allowedContentTypes: [.png, .jpeg, .bmp]) { result in
                switch result {
                case .success(let url): importPicture(url)
                case .failure(let error): pictureProblem = error.localizedDescription
                }
            }
            .alert("Background image", isPresented: Binding(
                get: { pictureProblem != nil }, set: { if !$0 { pictureProblem = nil } })) {
                Button("OK") { pictureProblem = nil }
            } message: { Text(pictureProblem ?? "") }
    }

    private func importPicture(_ url: URL) {
        let accessed = url.startAccessingSecurityScopedResource()
        defer { if accessed { url.stopAccessingSecurityScopedResource() } }
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
              CGImageSourceCreateImageAtIndex(source, 0, nil) != nil else {
            pictureProblem = "This file could not be read as an image."
            return
        }
        do {
            let files = FileManager.default
            let support = try files.url(for: .applicationSupportDirectory, in: .userDomainMask,
                                        appropriateFor: nil, create: true)
            let directory = support.appendingPathComponent("DisplayPictures", isDirectory: true)
            try files.createDirectory(at: directory, withIntermediateDirectories: true)
            let destination = directory.appendingPathComponent(UUID().uuidString)
                .appendingPathExtension(url.pathExtension)
            try files.copyItem(at: url, to: destination)
            main.changeDisplay { $0.backgroundImagePath = destination.path; $0.backgroundEnabled = true }
        } catch { pictureProblem = error.localizedDescription }
    }
}

/// Receive and transmit retain independent stop arrays. The editor uses the
/// existing native stop controls, including locked end positions and 2–8 stops.
private struct DisplayGradientEditor: View {
    let main: MainScreenModel
    @ObservedObject var band: BandModel
    let editor: DisplaySheetModel.GradientEditor
    @Environment(\.dismiss) private var dismiss
    private var stops: [PaletteStop] {
        editor == .receive ? band.settings.customPalette : band.settings.txCustomPalette
    }
    private func change(_ edit: (inout [PaletteStop]) -> Void) {
        main.changeDisplay { settings in
            if editor == .receive { edit(&settings.customPalette) }
            else { edit(&settings.txCustomPalette) }
        }
    }
    var body: some View {
        NavigationStack {
            Form {
                Section {
                    ForEach(stops.indices, id: \.self) { index in
                        HStack {
                            ColorPicker("Stop \(index + 1)", selection: Binding(
                                get: { BandColours.slice(stops[index].colour) },
                                set: { colour in change { $0[index].colour = DisplayOnThisPhonePage.hex(colour) } }),
                                supportsOpacity: false)
                            Stepper("\(Int((stops[index].at * 100).rounded()))%", value: Binding(
                                get: { stops[index].at * 100 },
                                set: { value in change { $0[index].at = min(max(value, 0), 100) / 100 } }),
                                in: 0...100, step: 5)
                                .fixedSize()
                                .disabled(index == 0 || index == stops.count - 1)
                        }
                        .accessibilityIdentifier("\(editor.rawValue)Gradient.stop\(index)")
                    }
                    HStack {
                        Button("Add a stop") {
                            change { stops in
                                guard stops.count >= 2, stops.count < 8 else { return }
                                // Halfway between the last two stops, in the colour of the lower.
                                let low = stops[stops.count - 2]
                                let high = stops[stops.count - 1]
                                stops.insert(PaletteStop(at: (low.at + high.at) / 2, colour: low.colour),
                                             at: stops.count - 1)
                            }
                        }.disabled(stops.count >= 8)
                        Spacer()
                        Button("Remove a stop") {
                            change { stops in
                                guard stops.count > 2 else { return }
                                stops.remove(at: stops.count - 2)
                            }
                        }.disabled(stops.count <= 2)
                    }.buttonStyle(.borderless).tint(WorkspacePalette.accentText)
                } footer: { Text("Custom gradient, kept on this device.") }
            }
            .navigationTitle(editor == .receive ? "Receive gradient" : "Transmit gradient")
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
        }
    }
}
