// NereusSDR for iOS: the slice list behind the toolbar's Slice button: every live slice on the Core, and what the phone may do with each
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMirror

/// The list the toolbar's Slice button drops (R-IOS-42, R-IOS-11; JJ's
/// rulings of 2026-09-30, the board's `#slicelist-review`): every live
/// slice on the Core in letter order, never regrouped
/// (``SliceRoster``). This phone's own slices on its band are one tap
/// each (make active); a slice on another pan it is in has Show its band;
/// every slice it is not in has Listen and Take control; a slice it
/// listens to has Stop listening, Take control and its own volume and
/// mute; a slice it controls elsewhere has Release. Take control is
/// greyed only while the slice is on the air, with the Core's words.
/// New slice at the foot asks the Core for one on this pan; refused for
/// room, the Core's words show with the slices it offers to listen in to.
/// A Core that does not share slices lists only this phone's own, with
/// its one line.
@MainActor
final class SliceListModel: ObservableObject {
    @Published private(set) var rows: [SliceRoster.Row] = []
    /// The Core does not share slices: only this phone's own are listed.
    @Published private(set) var older = false
    /// The Core's words for the last thing it refused from the list, as sent.
    @Published private(set) var note: String?
    /// New slice was refused: the Core's words, and the slices it offers to listen in to.
    @Published private(set) var newSliceRefusal: NewSliceRefusal?
    /// A listen, stop, release or new slice sent and not answered.
    @Published private(set) var working: Set<Int> = []

    struct NewSliceRefusal: Equatable {
        let text: String
        let usable: [SliceRoster.UsableSlice]
    }

    /// The pan New slice adds to (``PanSheetModel/addSliceVerb``, `panId`).
    weak var pan: PanSheetModel?

    /// The list's title and the phone's words on it (the board's).
    static let title = "Slices on the Core"
    static let newSliceTitle = "New slice"
    static let listenTitle = "Listen"
    static let stopListeningTitle = "Stop listening"
    static let releaseTitle = "Release"
    static let makeActiveTitle = "Make active"
    static let activeTitle = "Active"
    static let showBandTitle = "Show its band"
    static let volumeTitle = "Your volume"
    static let onlyThisPhoneText = "Only what this phone hears"
    static let listenInsteadText = "Listen in instead"
    /// The Core's words to a phone for every listen verb on a Core that does
    /// not share slices (SessionCommandDispatcher), shown as the older
    /// Core's one line.
    static let olderCoreText = "This Core cannot share slices between devices."
    /// The id New slice's answer is kept under in `working`.
    static let newSliceWork = -1

    private let store: MirrorStore
    private let slices: BandSlicesModel
    private let commands: CommandClient?
    private var watches: Set<AnyCancellable> = []
    private var objectWatches: [String: AnyCancellable] = [:]
    private var queued = false

    init(store: MirrorStore, slices: BandSlicesModel, commands: CommandClient?, pan: PanSheetModel? = nil) {
        self.store = store
        self.slices = slices
        self.commands = commands
        self.pan = pan
        store.$objectKeys.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$capabilities.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        slices.$shownSliceId.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        slices.$taking.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        rebuild()
    }

    /// How many slices are live on the Core, as the list's title counts them.
    var liveText: String {
        older ? SliceRoster.thisPanText : "\(rows.count) live"
    }

    /// The row's slice is this phone's own on its own band: one tap makes it active.
    func isOwnHere(_ row: SliceRoster.Row) -> Bool {
        row.relation == .control && row.here
    }

    /// The row's slice is the active one (for an own row) or the one shown (another pan's).
    func isActive(_ row: SliceRoster.Row) -> Bool {
        row.id == slices.activeSliceId
    }

    /// A slice on another pan this phone is in: its row's top shows its band.
    func showsBand(_ row: SliceRoster.Row) -> Bool {
        row.relation != .none && !row.here
    }

    /// Take control was sent and the Core has not answered.
    func isTaking(_ row: SliceRoster.Row) -> Bool {
        slices.taking.contains(row.id)
    }

    /// Listen or Stop listening, Release or New slice was sent and not answered.
    func isWorking(_ row: SliceRoster.Row) -> Bool {
        working.contains(row.id)
    }

    // MARK: How a row reads

    /// The slice's colour from the catalogue, `#RRGGBB`.
    func colour(_ sliceId: Int) -> String {
        BandSlice.colour(forIndex: sliceId, in: slices.catalog?.sliceColours ?? [])
    }

    /// The catalogue's label for the row's mode, `LSB`; empty for none.
    func modeText(_ row: SliceRoster.Row) -> String {
        slices.catalog?.modes.first { $0.id == row.slice.mode }?.label ?? ""
    }

    /// The band grid's label for the row's band, `40m`; empty for none.
    func bandText(_ row: SliceRoster.Row) -> String {
        slices.catalog?.bands?.first { $0.id == row.slice.band }?.label ?? ""
    }

    /// The row's frequency as the flag reads it, `7.177.000`.
    func frequencyText(_ row: SliceRoster.Row) -> String {
        BandSlice.frequencyText(hz: row.slice.frequencyHz)
    }

    /// Who controls a slice a refused New slice offers, in the Core's ladder.
    func holderText(_ usable: SliceRoster.UsableSlice) -> String {
        SliceAccess.deviceWords(usable.controllerDeviceId, devices: SeveralDevices.connectedDevices(in: store))
    }

    /// The row a refused New slice offers, when the list has it.
    func row(_ sliceId: Int) -> SliceRoster.Row? {
        rows.first { $0.id == sliceId }
    }

    // MARK: What the rows do

    /// One of this phone's own slices: it becomes active (and the phone
    /// comes back to its own band).
    func makeActive(_ row: SliceRoster.Row) {
        slices.activate(row.id)
    }

    /// Show its band: the slice becomes active on this phone and the band jumps to it.
    func showBand(_ row: SliceRoster.Row) {
        slices.show(row.id)
    }

    /// Listen: the Core joins this phone to the slice, which then shows
    /// (on another pan the band jumps to it).
    func listen(_ row: SliceRoster.Row) {
        run(row.id) { slices in await slices.listen(row.id, incarnation: row.access?.incarnation) }
    }

    /// Listen on one of the slices a refused New slice offered.
    func listen(_ usable: SliceRoster.UsableSlice) {
        newSliceRefusal = nil
        run(usable.sliceId) { slices in await slices.listen(usable.sliceId, incarnation: usable.incarnation) }
    }

    func stopListening(_ row: SliceRoster.Row) {
        run(row.id) { slices in await slices.stopListening(row.id) }
    }

    func release(_ row: SliceRoster.Row) {
        run(row.id) { slices in await slices.releaseSlice(row.id) }
    }

    func takeControl(_ row: SliceRoster.Row) {
        note = nil
        slices.takeControl(row.id)
    }

    /// The row's volume and mute on this phone only.
    func level(_ row: SliceRoster.Row) -> BandSlicesModel.ListenLevel {
        slices.listenLevel(row.id)
    }

    func setLevel(_ row: SliceRoster.Row, level: Double, muted: Bool) {
        slices.setListenLevel(row.id, level: level, muted: muted)
        objectWillChange.send()
    }

    /// New slice: the Core adds one on this pan. Refused for room, its
    /// words show with the slices it offers to listen in to.
    func newSlice() {
        guard let commands, let panKey = pan?.panKey, !working.contains(Self.newSliceWork) else {
            return
        }
        note = nil
        newSliceRefusal = nil
        working.insert(Self.newSliceWork)
        Task { @MainActor in
            defer { self.working.remove(Self.newSliceWork) }
            let result: CommandResult
            do {
                result = try await commands.invoke(PanSheetModel.addSliceVerb, arguments: [
                    CommandArgument(name: "panId", value: .text(panKey)),
                ], timeout: BandSlicesModel.commandTimeout)
            } catch {
                self.note = SeveralDevicesClient.noAnswerText
                return
            }
            guard !result.accepted else {
                return
            }
            var usable: [SliceRoster.UsableSlice] = []
            if case .text(let text)? = result.values["usableSlices"] {
                usable = SliceRoster.usableSlices(text)
            }
            self.newSliceRefusal = NewSliceRefusal(text: result.reason.isEmpty ? BandSlicesModel.refusedText
                                                                                 : result.reason,
                                                   usable: usable)
        }
    }

    /// New slice can be pressed: the Core is there and this pan is known.
    var canAddSlice: Bool {
        commands != nil && pan?.panKey != nil && newSliceRefusal == nil
    }

    /// Closes the Core's words under the list.
    func dismissNote() {
        note = nil
    }

    private func run(_ id: Int, _ verb: @escaping (BandSlicesModel) async -> String?) {
        guard !working.contains(id) else {
            return
        }
        note = nil
        working.insert(id)
        Task { @MainActor in
            let words = await verb(self.slices)
            self.working.remove(id)
            if let words {
                self.note = words
            }
        }
    }

    // MARK: Inside

    private func queueRebuild() {
        guard !queued else {
            return
        }
        queued = true
        Task { @MainActor [weak self] in
            self?.queued = false
            self?.rebuild()
        }
    }

    private func rebuild() {
        watchObjects()
        let version = SliceAccess.version(in: store)
        let joined = slices.entries.map { entry in
            SliceRoster.Slice(sliceId: entry.id, frequencyHz: entry.slice.frequencyHz, mode: entry.mode ?? -1,
                              band: entry.band ?? -1, streamIndex: entry.streamIndex, panKey: entry.panKey)
        }
        let home = slices.homeEntry
        let next = SliceRoster.rows(joined: joined, markers: SeveralDevices.markers(in: store),
                                    access: SliceAccess.states(in: store),
                                    devices: SeveralDevices.connectedDevices(in: store), me: slices.thisDeviceId,
                                    version: version,
                                    homePanKey: home?.panKey.flatMap { $0.isEmpty ? nil : $0 },
                                    homeStreamIndex: home.flatMap { $0.streamIndex >= 0 ? $0.streamIndex : nil })
            .map { row in
                // A slice with no pan or receiver the phone can tell apart is on its own band.
                var row = row
                if home == nil || (row.slice.panKey == nil && row.slice.streamIndex < 0) {
                    row.here = true
                }
                return row
            }
        if rows != next {
            rows = next
        }
        let isOlder = version < 1
        if older != isOlder {
            older = isOlder
        }
    }

    /// The list follows every marker, access object and the devices list.
    private func watchObjects() {
        var objects = store.objects(ofClass: SeveralDevices.markerClass)
            + store.objects(ofClass: SliceAccess.accessClass)
        if let devices = store.object(SeveralDevices.connectedDevicesKey) {
            objects.append(devices)
        }
        let keys = Set(objects.map(\.key))
        for key in objectWatches.keys where !keys.contains(key) {
            objectWatches[key] = nil
        }
        for object in objects where objectWatches[object.key] == nil {
            objectWatches[object.key] = object.$values.dropFirst().sink { [weak self] _ in self?.queueRebuild() }
        }
    }
}
