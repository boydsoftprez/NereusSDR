// NereusSDR for iOS: the iPad's pans: how many the radio allows, opening and closing one, the slices that move, the status line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMedia
import NereusMirror
import NereusModels
import os

/// One open pan's band: its own ``BandModel`` (from its own entry in the
/// display store), the slices on its pan and its own display subscription.
struct PanBand {
    let pan: PanIdentity
    let band: BandModel
    let slices: BandSlicesModel
    let subscriber: BandSubscriber
}

/// The pans the iPad shows, stacked Pan 1 on top (R-IOS-59, DC9, DC10; spec
/// section 6.3).
///
/// - **The limit** is the radio's (``PanLimits/panCount(capabilities:)``):
///   "Add a pan" is grayed there with "The <radio> can show <n> pans.", the
///   number a word as the board writes it (``RadioWords``).
/// - **Add a pan** opens the next pan below the last. It takes a spare
///   slice from a pan holding more than one, as the desktop's layout
///   spreads them (`RadioModel::spreadSlicesOntoEmptyPans`,
///   src/models/RadioModel.cpp:35966; a pan with a single slice is never
///   raided), by writing that slice's `panKey`; with none spare it asks
///   for a slice on the new pan (`addSliceOnPan` with its Core key), as the
///   desktop's `applyPanLayout` does. A refusal closes the new pan again and
///   shows the Core's words.
/// - **Close this pan** is grayed on Pan 1 with "Pan 1 stays open."; on
///   another pan it moves the pan's slices to Pan 1, as the desktop's
///   layout rehomes them (`RadioModel::rehomeSlicesToPans`,
///   src/models/RadioModel.cpp:35902, the survivor being the first live
///   pan), and keeps the pan's own values in the store for next time.
/// - **A smaller radio**: when the Core reports a lower limit than the pans
///   open, the highest close with a notice naming the limit, and nothing is
///   written to their slices (they are left to the Core's own rules).
/// - **An iPhone** shows one pan (spec section 6.4) and never opens another.
///
/// Each action says what it did in ``status``, the column's status line,
/// naming the slices that moved. Only slices this device controls move: a
/// slice it only listens to stays where its controller put it (the
/// desktop's scope, `controlled` only).
///
/// While more than one pan is open each pan's band draws its own pan's
/// slices (``BandSlicesModel/panKey``), so each subscription is on a slice
/// of its own pan; with Pan 1 alone it draws every slice, as before. The
/// display budget is shared through one ``SharedDisplayAllocator``, which
/// keeps the pan last touched (``lastTouched``) whole longest.
@MainActor
final class PanStackModel: ObservableObject {
    /// An iPhone's reason for no second pan (spec section 6.4).
    static let iPhoneReason = "An iPhone shows one pan."
    /// "Close this pan" on Pan 1 (spec section 6.3).
    static let firstStaysOpenReason = "Pan 1 stays open."
    /// "Add a pan" before the Core's snapshot is whole, as the pan sheet's add says.
    static let notConnectedReason = "Wait for the Core to connect."
    static let commandTimeout: Duration = .seconds(5)

    /// The open pans, Pan 1 first.
    @Published private(set) var open: [PanIdentity] = [.first]
    /// The pan the operator last touched: the toolbar follows it, and the
    /// shared budget keeps it whole longest. Only an open pan can be set.
    @Published var lastTouched: PanIdentity = .first {
        didSet {
            if !open.contains(lastTouched) {
                lastTouched = open.contains(oldValue) ? oldValue : .first
            }
            shared.activePanId = lastTouched.storeKey
        }
    }
    /// What the last action did, in the column's status line; nil before any.
    @Published private(set) var status: String?
    /// Each open pan whose display the Core refused, with its words
    /// (``BandSubscriber/displayRefusal``): a second pan's subscribe can be
    /// refused, and the pan says so rather than staying blank.
    @Published private(set) var displayRefusals: [PanIdentity: String] = [:]
    /// The device can show more than one pan: the iPad. Set by the main
    /// screen from the device; a test may set it.
    var allowsMorePans: Bool

    /// The pans the radio allows.
    var limit: Int { PanLimits.panCount(capabilities: mirror.capabilities) }

    private let first: PanBand
    private var bands: [PanIdentity: PanBand] = [:]
    /// Closed pans' subscribers still letting their endpoints go.
    private var retiring: [BandSubscriber] = []
    private let mirror: MirrorStore
    private let settings: SettingsProxyClient
    private let commands: CommandClient?
    private let operations: BandSubscriber.Operations
    private let displaySettings: BandDisplaySettingsStore
    private let shared: SharedDisplayAllocator
    private let deadline: Duration
    private let catalogFeed: CatalogFeed
    /// Every slice on the Core, whatever pan it is on, for the spare-slice rule.
    private let everySlice: BandSlicesModel
    /// A pan whose opening waits for the Core's answer.
    private var adding: PanIdentity?
    private var mediaConnected = false
    private var watches: Set<AnyCancellable> = []
    private var refusalWatches: [PanIdentity: AnyCancellable] = [:]
    private var limitQueued = false
    private static let logger = Logger(subsystem: "NereusSDR", category: "pan.stack")

    init(first: PanBand, mirror: MirrorStore, settings: SettingsProxyClient, commands: CommandClient?,
         operations: BandSubscriber.Operations, displaySettings: BandDisplaySettingsStore,
         shared: SharedDisplayAllocator, deadline: Duration = BandSubscriber.acknowledgementDeadline,
         catalogFeed: CatalogFeed, allowsMorePans: Bool) {
        self.first = first
        self.mirror = mirror
        self.settings = settings
        self.commands = commands
        self.operations = operations
        self.displaySettings = displaySettings
        self.shared = shared
        self.deadline = deadline
        self.catalogFeed = catalogFeed
        self.allowsMorePans = allowsMorePans
        everySlice = BandSlicesModel(store: mirror, commands: nil)
        bands[.first] = first
        shared.activePanId = PanIdentity.first.storeKey
        watchRefusal(first)
        // The other pans follow Pan 1's long-session policy and its asks.
        first.subscriber.sessionChanged = { [weak self] session in
            self?.others.forEach { $0.subscriber.session = session }
        }
        first.subscriber.askedAgain = { [weak self] in
            self?.others.forEach { $0.subscriber.transmitHolderChanged() }
        }
        catalogFeed.$catalog.sink { [weak self] catalog in
            self?.everySlice.catalog = catalog
            for band in self?.others ?? [] {
                band.band.catalog = catalog
                band.slices.catalog = catalog
            }
        }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queueLimitCheck() }.store(in: &watches)
        mirror.$isSnapshotComplete.sink { [weak self] _ in self?.queueLimitCheck() }.store(in: &watches)
    }

    /// Every open pan's band but Pan 1's.
    private var others: [PanBand] {
        open.filter { $0 != .first }.compactMap { bands[$0] }
    }

    /// `pan`'s band; Pan 1's for a pan not open.
    func band(for pan: PanIdentity) -> PanBand {
        bands[pan] ?? first
    }

    // MARK: Add a pan

    /// Whether "Add a pan" can act, and why not.
    func addPanState() -> (enabled: Bool, reason: String?) {
        guard allowsMorePans else {
            return (false, Self.iPhoneReason)
        }
        guard commands != nil, mirror.isSnapshotComplete else {
            return (false, Self.notConnectedReason)
        }
        let limit = self.limit
        guard open.count < limit else {
            return (false, RadioWords.panLimit(radio: radioName, limit: limit))
        }
        guard adding == nil else {
            // The pan before it is still waiting for the Core.
            return (false, nil)
        }
        return (true, nil)
    }

    /// "Add a pan": the next pan opens below the last, with a spare slice
    /// moved onto it or a new slice the Core adds there.
    func addPan() {
        guard addPanState().enabled else {
            return
        }
        everySlice.thisDeviceId = first.slices.thisDeviceId
        let pan = nextPan()
        let above = open.filter { $0 < pan }.max() ?? .first
        let controlled = everySlice.entries.filter { !$0.listening }
        let already = controlled.filter { $0.panKey == pan.corePanKey }
        let spare = Self.spareSlice(controlled)
        openBand(pan)
        lastTouched = pan
        let opened = "\(pan.title) opened below \(above.title)."
        if !already.isEmpty {
            // The pan is not empty: the desktop moves nothing onto it.
            status = opened + " " + Self.sliceWords(already) + (already.count > 1 ? " are" : " is") + " on it."
            return
        }
        adding = pan
        if let spare {
            let from = spare.panKey.flatMap(PanIdentity.init(corePanKey:)) ?? .first
            let letter = BandSlice.letter(forIndex: spare.id)
            Task { [weak self, mirror] in
                let outcome = await mirror.write("slice:\(spare.id)", property: Self.panKeyProperty,
                                                 value: .text(pan.corePanKey))
                guard let self else { return }
                self.finishAdding(pan, accepted: outcome.accepted, reason: outcome.reason,
                                  said: opened + " Slice \(letter) moved to it from \(from.title), which had a spare.")
            }
            return
        }
        guard let commands else {
            return
        }
        Task { [weak self] in
            let result: CommandResult
            do {
                result = try await commands.invoke(PanSheetModel.addSliceVerb,
                                                   arguments: [CommandArgument(name: "panId",
                                                                               value: .text(pan.corePanKey))],
                                                   timeout: Self.commandTimeout)
            } catch {
                Self.logger.info("addSliceOnPan did not reach the Core: \(String(describing: error), privacy: .public)")
                self?.finishAdding(pan, accepted: false, reason: Self.words(for: error), said: "")
                return
            }
            let added = result.affectedKeys.compactMap(Self.sliceIndex(ofKey:)).first
            let said = added.map { opened + " Slice \(BandSlice.letter(forIndex: $0)) added on \(pan.title)." } ?? opened
            self?.finishAdding(pan, accepted: result.accepted, reason: result.reason, said: said)
        }
    }

    private func finishAdding(_ pan: PanIdentity, accepted: Bool, reason: String, said: String) {
        if adding == pan {
            adding = nil
        }
        guard open.contains(pan) else {
            return
        }
        if accepted {
            status = said
        } else {
            // Nothing moved or added: the pan closes again, with the Core's words.
            retire(pan)
            status = reason.isEmpty ? BandSlicesModel.refusedText : reason
        }
    }

    /// The lowest pan number after 1 not open: the next pan below the last.
    private func nextPan() -> PanIdentity {
        var number = 2
        while open.contains(PanIdentity(number: number)) {
            number += 1
        }
        return PanIdentity(number: number)
    }

    /// The slice a new pan takes, or nil when no pan holds more than one.
    /// The donor pan is the desktop's: the first, in slice order, holding
    /// more than one of this device's slices
    /// (`RadioModel::spreadSlicesOntoEmptyPans`, src/models/RadioModel.cpp:35966-35997).
    /// The slice is the donor's newest, as the board moves slice C, so the
    /// pan's first slice stays where it is.
    static func spareSlice(_ controlled: [BandSlicesModel.Entry]) -> BandSlicesModel.Entry? {
        let sorted = controlled.sorted { $0.id < $1.id }
        var occupancy: [String: Int] = [:]
        for entry in sorted {
            occupancy[entry.panKey ?? "", default: 0] += 1
        }
        guard let donor = sorted.first(where: { (occupancy[$0.panKey ?? ""] ?? 0) > 1 }) else {
            return nil
        }
        return sorted.last { ($0.panKey ?? "") == (donor.panKey ?? "") }
    }

    // MARK: Close this pan

    /// Whether "Close this pan" can act on `pan`, and why not.
    func closePanState(_ pan: PanIdentity) -> (enabled: Bool, reason: String?) {
        guard pan != .first else {
            return (false, Self.firstStaysOpenReason)
        }
        guard open.contains(pan) else {
            return (false, nil)
        }
        return (true, nil)
    }

    /// "Close this pan": the pan closes, its slices move to Pan 1, and its
    /// own values stay in the store for next time.
    func closePan(_ pan: PanIdentity) {
        guard closePanState(pan).enabled, let band = bands[pan] else {
            return
        }
        let moving = band.slices.entries.filter { $0.panKey == pan.corePanKey && !$0.listening }
        let home = first.band.requestedView
        retire(pan)
        // From src/models/RadioModel.cpp:35902-35925 (rehomeSlicesToPans: each slice off the live pans to the first)
        for entry in moving {
            Task { [weak self, mirror] in
                let outcome = await mirror.write("slice:\(entry.id)", property: Self.panKeyProperty,
                                                 value: .text(PanIdentity.first.corePanKey))
                if !outcome.accepted, outcome.isCurrent, !outcome.reason.isEmpty {
                    self?.status = outcome.reason
                }
            }
        }
        var words = "\(pan.title) closed."
        if let lead = moving.first {
            let hz = lead.slice.frequencyHz
            var place = ", at \(BandSlice.frequencyText(hz: hz))"
            if let home {
                if hz < home.centerHz - home.spanHz / 2 {
                    place += ", below the band in view"
                } else if hz > home.centerHz + home.spanHz / 2 {
                    place += ", above the band in view"
                }
            }
            words += " " + Self.sliceWords(moving) + " moved to \(PanIdentity.first.title)" + place + "."
        }
        status = words + " \(pan.title)'s own values are kept for next time."
    }

    // MARK: The radio's limit

    private func queueLimitCheck() {
        guard !limitQueued else {
            return
        }
        limitQueued = true
        Task { @MainActor [weak self] in
            self?.limitQueued = false
            self?.checkLimit()
        }
    }

    /// A Core reporting fewer pans than are open (another radio after a
    /// reconnect): the highest close, with a notice naming the limit, and
    /// nothing is written to their slices. Read only once the Core's
    /// snapshot is whole, so a reconnect's empty capabilities close nothing.
    private func checkLimit() {
        guard mirror.isSnapshotComplete, !mirror.capabilities.isEmpty else {
            return
        }
        let limit = self.limit
        guard open.count > limit else {
            return
        }
        let closing = Array(open.sorted().suffix(open.count - max(limit, 1)))
        for pan in closing {
            retire(pan)
        }
        let names = closing.count == 1 ? closing[0].title
            : "Pans " + RadioWords.list(closing.map { String($0.number) })
        status = RadioWords.panLimit(radio: radioName, limit: limit) + " " + names + " closed."
    }

    // MARK: The Core's media events

    /// One of the media client's events, for every pan but Pan 1 (whose
    /// band and subscriber the main screen feeds) and for closed pans still
    /// letting their endpoints go.
    func receive(_ event: MediaControlEvent) {
        switch event {
        case .mediaState(.connected):
            mediaConnected = true
        case .mediaState(.closed), .mediaState(.failed):
            mediaConnected = false
        default:
            break
        }
        for band in others {
            band.band.receive(event)
            band.subscriber.receive(event)
        }
        for subscriber in retiring {
            subscriber.receive(event)
        }
        pruneRetiring()
    }

    // MARK: Inside

    private static let panKeyProperty = "panKey"

    /// The radio's name as the main screen shows it.
    private var radioName: String? {
        MainScreenModel.radioName(catalog: catalogFeed.catalog, radio: mirror.object("radio"))
    }

    private func openBand(_ pan: PanIdentity) {
        let key = pan.storeKey
        let band = BandModel(settings: displaySettings.settings(forPan: key))
        band.catalog = catalogFeed.catalog
        band.settings.sideways = first.band.settings.sideways
        band.keepSettings = { [weak band, displaySettings] kept in
            displaySettings.setSettings(kept, forPan: key)
            guard let band else { return }
            var saved = displaySettings.settings(forPan: key)
            saved.sideways = band.settings.sideways
            band.settings = saved
        }
        let slices = BandSlicesModel(store: mirror, commands: commands, catalog: catalogFeed.catalog)
        slices.thisDeviceId = first.slices.thisDeviceId
        let subscriber = BandSubscriber(band: band, slices: slices, mirror: mirror, settings: settings,
                                        operations: operations, deadline: deadline, pan: pan, shared: shared)
        subscriber.session = first.subscriber.session
        let opened = PanBand(pan: pan, band: band, slices: slices, subscriber: subscriber)
        bands[pan] = opened
        open = (open + [pan]).sorted()
        applyFilters()
        watchRefusal(opened)
        if mediaConnected {
            band.receive(.mediaState(.connected))
            subscriber.receive(.mediaState(.connected))
        }
    }

    /// The pan goes: its subscriber lets its endpoint go, its band is
    /// dropped, and its own values stay in the store.
    private func retire(_ pan: PanIdentity) {
        guard pan != .first, let band = bands.removeValue(forKey: pan) else {
            return
        }
        band.subscriber.close()
        retiring.append(band.subscriber)
        open.removeAll { $0 == pan }
        refusalWatches[pan] = nil
        displayRefusals[pan] = nil
        if adding == pan {
            adding = nil
        }
        if lastTouched == pan {
            lastTouched = .first
        }
        applyFilters()
        pruneRetiring()
    }

    private func pruneRetiring() {
        retiring.removeAll { $0.holdsNothing }
    }

    /// With more than one pan each band draws its own pan's slices; Pan 1
    /// alone draws every slice.
    private func applyFilters() {
        if open.count > 1 {
            for pan in open {
                bands[pan]?.slices.panKey = pan.corePanKey
            }
        } else {
            first.slices.panKey = nil
        }
    }

    private func watchRefusal(_ band: PanBand) {
        let pan = band.pan
        refusalWatches[pan] = band.subscriber.$displayRefusal.sink { [weak self] refusal in
            guard let self else { return }
            if self.displayRefusals[pan] != refusal {
                self.displayRefusals[pan] = refusal
            }
        }
    }

    /// "Slice C", "Slices B and C".
    private static func sliceWords(_ entries: [BandSlicesModel.Entry]) -> String {
        let letters = entries.sorted { $0.id < $1.id }.map { BandSlice.letter(forIndex: $0.id) }
        return (letters.count > 1 ? "Slices " : "Slice ") + RadioWords.list(letters)
    }

    private static func sliceIndex(ofKey key: String) -> Int? {
        let prefix = "slice:"
        guard key.hasPrefix(prefix) else {
            return nil
        }
        return Int(key.dropFirst(prefix.count))
    }

    private static func words(for error: Error) -> String {
        switch error as? CommandError {
        case .notSent: return "The request could not be sent to the Core."
        case .linkLost: return "The connection to the Core was lost."
        case .timedOut: return "The Core did not answer this request."
        case nil: return "The request failed: \(error.localizedDescription)"
        }
    }
}
