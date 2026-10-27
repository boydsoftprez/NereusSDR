// NereusSDR for iOS: one Setup page the Core describes, drawn from its description with each control live
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI
import UniformTypeIdentifiers
import ImageIO

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
    @State var profileOwner = UUID()
    @State private var paPresentation: UUID?

    /// The words for a page the Core no longer describes.
    static let goneText = "This page is no longer on the Core."

    private func openProfiles(_ page: SetupDescription.Page) {
        if let control = page.sections.flatMap(\.controls).first(where: { $0.modern?.profileUnsavedChanges != nil }) {
            dispatcher.txProfiles.open(control, in: category)
        }
    }

    var body: some View {
        if let page = pages.page(pageId, in: category) {
            SetupPageContainer {
                if let main, !PhoneSetupKeys.pictureFields(on: page).isEmpty {
                    DisplayV29ScopeHeader(main: main, page: page)
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
                ForEach(Array(page.sections.enumerated()), id: \.offset) { _, section in
                    let group = levelCal.flatMap { section.title == LevelCalSection.title ? $0 : nil }
                    SetupPanelSection {
                        if let group {
                            LevelCalSection.Top(model: group)
                        }
                        ForEach(section.controls, id: \.id) { control in
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
                        if let note = section.note {
                            Text(note).font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
                        }
                    } header: {
                        if !section.title.isEmpty {
                            SetupPanelTitle(title: section.title,
                                            tag: section.owner.map { $0 == .station ? .core : .thisPhone })
                        }
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


/// The accepted scope context and row own indicators use the actual
/// singlePan typed store. Disabled stored preferences retain their own dot.
private struct DisplayV29ScopeHeader: View {
    @ObservedObject var main: MainScreenModel
    let page: SetupDescription.Page
    private var fields: Set<String> { PhoneSetupKeys.pictureFields(on: page) }
    private var own: Set<String> { main.ownDisplayFields(among: fields) }
    private var ownCount: Int {
        var count = 0
        for control in page.sections.flatMap(\.controls) {
            guard control.owner == .pan, control.perBand == nil, control.kind != .button,
                  case .phone(let key)? = control.binding else { continue }
            if !own.isDisjoint(with: PhoneSetupKeys.displayFields(forPhoneKey: key)) { count += 1 }
        }
        if own.contains("backgroundImagePath") { count += 1 }
        return count
    }
    var body: some View {
        SetupPanelSection {
            SheetGrid(columns: 2) {
                PanelButton(label: "All pans", lit: main.displayEditScope == .all, style: .blue) {
                    main.displayEditScope = .all
                }
                PanelButton(label: "Pan 1", lit: main.displayEditScope == .pan, style: .blue) {
                    main.displayEditScope = .pan
                }
            }
            .accessibilityLabel("Display settings for")
            .accessibilityIdentifier("displayScope")
            if main.displayEditScope == .pan {
                Text("\(ownCount) own value\(ownCount == 1 ? "" : "s") on this page")
                    .font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
                    .accessibilityIdentifier("displayPageOwnCount")
                Button("Follow All pans on this page") { main.resetDisplayPicture(fields: fields) }
                    .buttonStyle(SetupButtonStyle())
                    .disabled(own.isEmpty).accessibilityIdentifier("displayPageFollow")
                Button("Copy own values to All pans") { _ = main.copyOwnDisplayPictureToMaster(fields: fields) }
                    .buttonStyle(SetupButtonStyle())
                    .disabled(own.isEmpty).accessibilityIdentifier("displayPageCopy")
            } else {
                Text("Changes here go to every pan. A pan's own value for the changed row is cleared.")
                    .font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
            }
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
                    .flatMap(SetupDescription.bandName) ?? "\u{2013}")
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
    private var own: Bool {
        guard main.displayEditScope == .pan, control.owner == .pan, control.perBand == nil,
              control.kind != .button, case .phone(let key)? = control.binding else { return false }
        return !main.ownDisplayFields(among: PhoneSetupKeys.displayFields(forPhoneKey: key)).isEmpty
    }
    var body: some View {
        HStack(spacing: 8) {
            if own {
                Circle().fill(WorkspacePalette.accentFill).frame(width: 5, height: 5)
                    .accessibilityLabel("Pan 1's own value")
                    .accessibilityIdentifier(control.id + ".own")
            }
            DescribedControl(control: control, category: category, dispatcher: dispatcher,
                             specialized: specialized, profileOwner: profileOwner)
                .effectiveDisplayText(readout)
        }
    }
    private var readout: String? {
        guard let readout = control.readout else { return nil }
        guard let value = main.displayEffectiveValue(readout) else { return "\u{2013}" }
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
