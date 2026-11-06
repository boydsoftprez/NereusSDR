// NereusSDR for iOS: a pan's band stack under the band grid: a band's list, its arrows, its menus, add and reorder
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// The band stack block (docs/architecture/2026-11-05-band-stack-design.md
/// section 8), drawn in the Pan sheet and the column's BAND menu. Everything
/// in it is the Core's: the block shows ``BandStackModel``'s document and
/// sends its verbs. "40m" picks which band's list is read, and while that is
/// not the slice's band the arrows and add wait, with the reason under them.
/// A row's tap goes there. A Core without band stacks leaves one greyed row.
struct BandStackBlock: View {
    @ObservedObject var model: BandStackModel
    /// The block's width, which the BAND menu's narrower rows follow.
    let width: CGFloat

    /// The row whose grip is under a finger.
    @State private var dragged: String?
    /// The rows' places, for a drag to read.
    @State private var rowFrames: [String: CGRect] = [:]
    /// The row a grip's tap asks to move, without a drag.
    @State private var moving: BandStack.Entry?
    /// The row name field has the keyboard.
    @FocusState private var nameFieldFocused: Bool
    /// The band Clear unlocked asks about.
    @State private var clearing: String?

    private static let rowsSpace = "bandStackRows"
    /// A grip's touch that moves less than this is a tap.
    private static let gripTapLimit: CGFloat = 6
    /// A row's band plan colour at its left, fading out, as the mockup's `30` alpha.
    private static let tintOpacity = 0.19
    private static let narrowWidth: CGFloat = 300

    private static let down = "\u{25BE}"
    private static let previous = "\u{2039}"
    private static let next = "\u{203A}"
    private static let more = "\u{22EF}"
    private static let dot = " \u{00B7} "

    init(model: BandStackModel, width: CGFloat) {
        self.model = model
        self.width = width
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1).padding(.bottom, 8)
            if let reason = model.unavailableReason {
                unavailable(reason)
            } else {
                header
                rows
                footer
                notes
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("bandStackBlock")
        .confirmationDialog(clearTitle, isPresented: present($clearing), titleVisibility: .visible) {
            Button("Clear", role: .destructive) {
                model.clear(bandKey: clearing)
            }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Locked entries stay.")
        }
        .confirmationDialog("Move this entry", isPresented: present($moving), titleVisibility: .visible) {
            let index = moving.flatMap { entry in model.reorder?.ids.firstIndex(of: entry.id) } ?? 0
            let count = model.reorder?.ids.count ?? 0
            Button("Move up") { moveAsked(by: -1) }
                .disabled(index == 0)
            Button("Move down") { moveAsked(by: 1) }
                .disabled(index >= count - 1)
            Button("Cancel", role: .cancel) {}
        }
    }

    private func present<Value>(_ value: Binding<Value?>) -> Binding<Bool> {
        Binding(get: { value.wrappedValue != nil }, set: { shown in
            if !shown {
                value.wrappedValue = nil
            }
        })
    }

    private var clearTitle: String {
        "Clear the unlocked entries on \(clearing ?? "")?"
    }

    // MARK: A Core or a moment with no stack

    private func unavailable(_ reason: String) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            PanelButton(label: model.availability == .needsNewerCore ? CatalogFeed.needsNewerCoreText : "Band stack",
                        lit: false, style: .blue, disabled: true) {}
                .accessibilityHint(reason)
                .accessibilityIdentifier("bandStackUnavailable")
            SheetNote(reason)
                .accessibilityIdentifier("bandStackUnavailableReason")
        }
    }

    // MARK: The head

    private var header: some View {
        let band = model.shownBand
        let viewing = model.isViewingAnotherBand
        let stepReason = stepUnavailableReason
        let narrow = width < Self.narrowWidth
        return HStack(spacing: 5) {
            picker(band)
                .fixedSize()
            if let band {
                Text(model.countText(band: band))
                    .font(.system(size: 11.5, weight: .semibold, design: .monospaced))
                    .foregroundStyle(ChromeColours.textDim)
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
                    .accessibilityIdentifier("bandStackCount")
            }
            Spacer(minLength: 0)
            headButton(Self.previous, width: narrow ? 34 : 38, id: "bandStackPrevious", label: "Previous entry",
                       disabled: stepReason != nil, hint: stepReason) {
                model.step(.previous)
            }
            headButton(Self.next, width: narrow ? 34 : 38, id: "bandStackNext", label: "Next entry",
                       disabled: stepReason != nil, hint: stepReason) {
                model.step(.next)
            }
            if let band {
                bandMenu(band, width: narrow ? 30 : 34)
            }
        }
        .frame(minHeight: 34)
        .padding(.bottom, 5)
        .accessibilityValue(viewing ? "Showing another band" : "")
    }

    /// Why the arrows and add cannot act; nil when they can.
    private var stepUnavailableReason: String? {
        if !model.hasActiveSlice {
            return PanSheetModel.noSliceReason
        }
        if model.isViewingAnotherBand, let band = model.shownBand {
            return "Tune to \(band) to add a spot there. Tap a row to go."
        }
        return nil
    }

    private func headButton(_ text: String, width: CGFloat, id: String, label: String, disabled: Bool, hint: String?,
                            action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(text)
                .font(.system(size: 15, weight: .bold))
                .frame(width: width, height: 32)
        }
        .buttonStyle(WorkspaceButtonStyle(.raised))
        .disabled(disabled)
        .accessibilityLabel(label)
        .accessibilityHint(hint ?? "")
        .accessibilityIdentifier(id)
    }

    /// "40m ▾": the visible way to a band's list, as a hold on its button is the quick one.
    private func picker(_ band: String?) -> some View {
        Menu {
            Section("Show the stack for") {
                ForEach(model.pickerBands, id: \.self) { key in
                    Button {
                        model.showBand(key)
                    } label: {
                        let count = model.stack?.entries(band: key).count ?? 0
                        if key == band {
                            Label("\(key)  \(count)", systemImage: "checkmark")
                        } else {
                            Text("\(key)  \(count)")
                        }
                    }
                }
            }
        } label: {
            HStack(spacing: 5) {
                Text(band ?? "Band")
                    .font(.system(size: 12, weight: .bold))
                Text(Self.down)
                    .font(.system(size: 9))
                    .foregroundStyle(ChromeColours.textDim)
            }
            .foregroundStyle(ChromeColours.text)
            .padding(.horizontal, 9)
            .frame(height: 32)
            .background { WorkspaceFace(.raised) }
        }
        .menuOrder(.fixed)
        .accessibilityLabel("Band stack for \(band ?? "no band")")
        .accessibilityIdentifier("bandStackPicker")
    }

    /// The band's "...": where it returns a slice, and Clear unlocked.
    private func bandMenu(_ band: String, width: CGFloat) -> some View {
        let mode = model.stack?.returnMode(band: band)
        let entries = model.stack?.entries(band: band) ?? []
        return Menu {
            Section("When \(band) opens, return to") {
                returnChoice("Current entry", band: band, mode: .current, chosen: mode == .current)
                returnChoice("Last frequency used", band: band, mode: .lastUsed, chosen: mode == .lastUsed)
                if entries.isEmpty {
                    Button {} label: {
                        Text("Specific entry")
                        Text("This band has no entries.")
                    }
                    .disabled(true)
                } else {
                    returnChoice("Specific entry", band: band, mode: .specific, chosen: mode == .specific,
                                 entryId: model.activeSlicePlace(band: band) ?? entries.first?.id)
                }
            }
            Divider()
            Button("Clear unlocked \(band) entries", role: .destructive) {
                clearing = band
            }
        } label: {
            Text(Self.more)
                .font(.system(size: 15, weight: .bold))
                .foregroundStyle(ChromeColours.text)
                .frame(width: width, height: 32)
                .background { WorkspaceFace(.raised) }
        }
        .menuOrder(.fixed)
        .accessibilityLabel("\(band) options")
        .accessibilityIdentifier("bandStackBandMenu")
    }

    private func returnChoice(_ title: String, band: String, mode: BandStack.ReturnMode, chosen: Bool,
                              entryId: String? = nil) -> some View {
        Button {
            model.setReturn(bandKey: band, mode: mode, entryId: entryId)
        } label: {
            if chosen {
                Label(title, systemImage: "checkmark")
            } else {
                Text(title)
            }
        }
    }

    // MARK: The rows

    /// The list in the Core's order, or Reorder's working order.
    private var shownEntries: [BandStack.Entry] {
        guard let band = model.shownBand, let entries = model.stack?.entries(band: band) else {
            return []
        }
        guard let reorder = model.reorder, reorder.band == band else {
            return entries
        }
        let working = reorder.ids.compactMap { id in entries.first { $0.id == id } }
        return working + entries.filter { !reorder.ids.contains($0.id) }
    }

    private var rows: some View {
        VStack(spacing: 3) {
            ForEach(shownEntries) { entry in
                row(entry)
            }
        }
        .coordinateSpace(name: Self.rowsSpace)
        .onPreferenceChange(BandStackRowFrameKey.self) { rowFrames = $0 }
    }

    private func row(_ entry: BandStack.Entry) -> some View {
        let active = model.activeSlicePlace(band: entry.band) == entry.id
        let inReorder = model.reorder != nil
        return HStack(spacing: 0) {
            if model.renaming == entry.id {
                renameFace(entry, active: active)
            } else {
                Button {
                    if !inReorder {
                        model.goTo(entry.id)
                    }
                } label: {
                    rowFace(entry, active: active)
                }
                .buttonStyle(.plain)
                .disabled(!model.hasActiveSlice)
                .allowsHitTesting(!inReorder)
                .accessibilityLabel(Self.rowWords(entry, specific: isSpecific(entry)))
                .accessibilityAddTraits(active ? .isSelected : [])
                .accessibilityHint(model.hasActiveSlice ? "" : PanSheetModel.noSliceReason)
                .accessibilityIdentifier("bandStackRow.\(entry.id)")
            }
            if inReorder {
                grip(entry)
            } else {
                rowMenu(entry)
            }
        }
        .padding(.leading, 9)
        .frame(minHeight: 46)
        .background { rowGround(entry, active: active) }
        .offset(x: dragged == entry.id ? 5 : 0)
        .opacity(dragged == entry.id ? 0.75 : 1)
        .background {
            GeometryReader { proxy in
                Color.clear.preference(key: BandStackRowFrameKey.self,
                                       value: [entry.id: proxy.frame(in: .named(Self.rowsSpace))])
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityActions {
            if inReorder {
                Button("Move up") { model.moveInReorder(entry.id, by: -1) }
                Button("Move down") { model.moveInReorder(entry.id, by: 1) }
            }
        }
    }

    private func isSpecific(_ entry: BandStack.Entry) -> Bool {
        model.stack?.returnMode(band: entry.band) == .specific && model.stack?.specificId(band: entry.band) == entry.id
    }

    @ViewBuilder
    private func rowGround(_ entry: BandStack.Entry, active: Bool) -> some View {
        if active {
            WorkspaceFace(.glass)
        } else {
            let shape = RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius, style: .continuous)
            shape.fill(ChromeColours.button)
                .overlay {
                    if let tint = model.tintColour(hz: entry.frequencyHz) {
                        shape.fill(LinearGradient(stops: [
                            .init(color: BandColours.withAlpha(tint).opacity(Self.tintOpacity), location: 0),
                            .init(color: .clear, location: 0.72),
                        ], startPoint: .leading, endPoint: .trailing))
                    }
                }
                .overlay { shape.strokeBorder(ChromeColours.sheetBorder, lineWidth: 1) }
        }
    }

    private func rowFace(_ entry: BandStack.Entry, active: Bool) -> some View {
        let narrow = width < Self.narrowWidth
        return HStack(spacing: 4) {
            VStack(alignment: .leading, spacing: 0) {
                Text(Self.frequencyText(entry.frequencyHz))
                    .font(.system(size: narrow ? 14.5 : 15.5, weight: .bold, design: .monospaced))
                    .foregroundStyle(active ? ChromeColours.textBright : ChromeColours.text)
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
                HStack(spacing: 0) {
                    Text(entry.mode + Self.dot + BandSlice.widthText(hz: Double(entry.filterHighHz - entry.filterLowHz)))
                        .foregroundStyle(active ? ChromeColours.toolbarOpenText : ChromeColours.textDim)
                        .fixedSize()
                    if !entry.name.isEmpty {
                        Text(Self.dot)
                            .foregroundStyle(active ? ChromeColours.toolbarOpenText : ChromeColours.textDim)
                            .fixedSize()
                        Text(entry.name)
                            .italic()
                            .foregroundStyle(active ? ChromeColours.toolbarOpenText : ChromeColours.accentText)
                            .truncationMode(.tail)
                    }
                }
                .font(.system(size: 11))
                .lineLimit(1)
            }
            .padding(.vertical, 4)
            Spacer(minLength: 0)
            ForEach(model.otherSlices(on: entry), id: \.id) { slice in
                SliceLetterBadge(letter: slice.letter, colour: BandColours.slice(slice.colour), size: 15, points: 9)
            }
            if isSpecific(entry) {
                Image(systemName: "star.fill")
                    .font(.system(size: 10))
                    .foregroundStyle(ChromeColours.buttonOnAmberText)
                    .accessibilityIdentifier("bandStackStar")
            }
            if entry.locked {
                Image(systemName: "lock.fill")
                    .font(.system(size: 10))
                    .foregroundStyle(ChromeColours.buttonOnAmberText)
                    .accessibilityIdentifier("bandStackLock")
            }
        }
        .padding(.trailing, 3)
        .contentShape(Rectangle())
    }

    /// Rename: the row's second line is its name, typed in place, saved on
    /// Return or on leaving the field.
    private func renameFace(_ entry: BandStack.Entry, active: Bool) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(Self.frequencyText(entry.frequencyHz))
                .font(.system(size: width < Self.narrowWidth ? 14.5 : 15.5, weight: .bold, design: .monospaced))
                .foregroundStyle(active ? ChromeColours.textBright : ChromeColours.text)
                .lineLimit(1)
            TextField("Name this spot", text: $model.renameDraft)
                .font(.system(size: 11).italic())
                .foregroundStyle(ChromeColours.textBright)
                .textFieldStyle(.plain)
                .submitLabel(.done)
                .focused($nameFieldFocused)
                .padding(.horizontal, 3)
                .frame(height: 19)
                .background {
                    let shape = RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius, style: .continuous)
                    shape.fill(ChromeColours.inset)
                        .overlay { shape.strokeBorder(ChromeColours.buttonOnBlueBorder, lineWidth: 1) }
                }
                .onSubmit { model.finishRename() }
                .onAppear { nameFieldFocused = true }
                .onChange(of: nameFieldFocused) { _, focused in
                    if !focused {
                        model.finishRename()
                    }
                }
                .accessibilityLabel("Name of this spot")
                .accessibilityIdentifier("bandStackRenameField")
        }
        .padding(.vertical, 4)
        .padding(.trailing, 3)
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    /// A row's frequency: megahertz and kilohertz, and hertz only when there are any.
    static func frequencyText(_ hz: Int64) -> String {
        let text = BandSlice.frequencyText(hz: Double(hz))
        return text.hasSuffix(".000") ? String(text.dropLast(4)) : text
    }

    private static func rowWords(_ entry: BandStack.Entry, specific: Bool) -> String {
        var words = [frequencyText(entry.frequencyHz), entry.mode,
                     BandSlice.widthText(hz: Double(entry.filterHighHz - entry.filterLowHz))]
        if !entry.name.isEmpty {
            words.append(entry.name)
        }
        if entry.locked {
            words.append("Locked")
        }
        if specific {
            words.append("Always return here")
        }
        return words.joined(separator: ", ")
    }

    /// A row's "...": the five items of spec section 8, each greyed one with its reason.
    private func rowMenu(_ entry: BandStack.Entry) -> some View {
        let specific = isSpecific(entry)
        let saveReason = saveUnavailableReason(entry)
        return Menu {
            Button {
                model.saveHere(entry.id)
            } label: {
                Text("Save this spot here")
                if let saveReason {
                    Text(saveReason)
                }
            }
            .disabled(saveReason != nil)
            Button("Rename") {
                model.startRename(entry)
            }
            Button(entry.locked ? "Unlock" : "Lock") {
                model.edit(entry.id, name: nil, locked: !entry.locked)
            }
            Button {
                model.setReturn(bandKey: entry.band, mode: specific ? .current : .specific,
                                entryId: specific ? nil : entry.id)
            } label: {
                if specific {
                    Label("Always return here", systemImage: "checkmark")
                } else {
                    Text("Always return here")
                }
            }
            Divider()
            Button("Delete", role: .destructive) {
                model.delete(entry.id)
            }
        } label: {
            Text(Self.more)
                .font(.system(size: 16))
                .foregroundStyle(ChromeColours.textDim)
                .frame(width: 40, height: 44)
                .contentShape(Rectangle())
        }
        .menuOrder(.fixed)
        .accessibilityLabel("Entry options")
        .accessibilityIdentifier("bandStackRowMenu.\(entry.id)")
    }

    private func saveUnavailableReason(_ entry: BandStack.Entry) -> String? {
        if !model.hasActiveSlice {
            return PanSheetModel.noSliceReason
        }
        if entry.locked {
            return "This entry is locked."
        }
        if model.currentBand != entry.band {
            return "Tune to \(entry.band) to save here."
        }
        return nil
    }

    // MARK: Reorder

    /// A row's grip, in Reorder only: drag it, or tap it to move the row one place.
    private func grip(_ entry: BandStack.Entry) -> some View {
        VStack(spacing: 3.5) {
            ForEach(0..<3, id: \.self) { _ in
                Rectangle().fill(ChromeColours.textDim).frame(width: 16, height: 1.6)
            }
        }
        .frame(width: 40, height: 44)
        .contentShape(Rectangle())
        .gesture(DragGesture(minimumDistance: 0, coordinateSpace: .named(Self.rowsSpace))
            .onChanged { touch in
                if hypot(touch.translation.width, touch.translation.height) >= Self.gripTapLimit || dragged != nil {
                    dragged = entry.id
                    drag(entry.id, toY: touch.location.y)
                }
            }
            .onEnded { touch in
                if dragged == nil, hypot(touch.translation.width, touch.translation.height) < Self.gripTapLimit {
                    moving = entry
                }
                dragged = nil
            })
        .accessibilityElement()
        .accessibilityAddTraits(.isButton)
        .accessibilityLabel("Move this entry")
        .accessibilityIdentifier("bandStackGrip.\(entry.id)")
        .accessibilityAction { moving = entry }
    }

    /// The dragged row takes the place of the row under the finger.
    private func drag(_ id: String, toY y: CGFloat) {
        guard let ids = model.reorder?.ids, let from = ids.firstIndex(of: id),
              let under = rowFrames.first(where: { $0.key != id && $0.value.minY <= y && y <= $0.value.maxY })?.key,
              let to = ids.firstIndex(of: under) else {
            return
        }
        model.moveInReorder(id, by: to - from)
    }

    private func moveAsked(by places: Int) {
        if let moving {
            model.moveInReorder(moving.id, by: places)
        }
    }

    // MARK: The foot

    private var footer: some View {
        let addReason = stepUnavailableReason
        let empty = shownEntries.isEmpty
        return HStack(spacing: 6) {
            PanelButton(label: "+ Add this spot", lit: false, style: .blue, disabled: addReason != nil) {
                model.add()
            }
            .accessibilityHint(addReason ?? "")
            .accessibilityIdentifier("bandStackAdd")
            PanelButton(label: model.reorder == nil ? "Reorder" : "Done", lit: model.reorder != nil, style: .blue,
                        disabled: empty) {
                model.toggleReorder()
            }
            .frame(width: 92)
            .accessibilityHint(empty ? "This band has no entries." : "")
            .accessibilityIdentifier("bandStackReorder")
        }
        .padding(.top, 5)
    }

    @ViewBuilder
    private var notes: some View {
        if let reason = stepUnavailableReason {
            SheetNote(reason)
                .padding(.top, 6)
                .accessibilityIdentifier("bandStackReason")
        }
        if shownEntries.isEmpty, let band = model.shownBand {
            SheetNote("No saved entries on \(band).")
                .padding(.top, 6)
                .accessibilityIdentifier("bandStackEmpty")
        }
        if let note = model.note {
            HStack(alignment: .top, spacing: 6) {
                Text(note)
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.refusalText)
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityIdentifier("bandStackNote")
                Button {
                    model.note = nil
                } label: {
                    Image(systemName: "xmark")
                        .font(.system(size: 10, weight: .bold))
                        .foregroundStyle(ChromeColours.textDim)
                        .frame(width: 28, height: 28)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Close this note")
                .accessibilityIdentifier("bandStackNoteClose")
            }
            .padding(.top, 6)
        }
    }
}

/// The rows' frames in the block, by entry id, for a grip's drag.
private struct BandStackRowFrameKey: PreferenceKey {
    static let defaultValue: [String: CGRect] = [:]

    static func reduce(value: inout [String: CGRect], nextValue: () -> [String: CGRect]) {
        value.merge(nextValue()) { $1 }
    }
}
