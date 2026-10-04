// NereusSDR for iOS: the Radio tab's readings and More list, following the Core while the tab is open
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusLink
import NereusMirror
import NereusModels

/// Keeps the Radio tab current (spec section 5.2 item 5): it reads the
/// Core's objects, capabilities, catalogue, telemetry and Setup pages again
/// whenever one changes, and publishes the More list when it changed. The
/// radio at a glance and Protocol Info are read on demand with the phone's
/// clock, so a reading goes unavailable once it is three seconds old. While
/// the readings are on screen (``show()``) it moves ``revision`` once a
/// second so they age; hidden (``hide()``) nothing periodic runs.
@MainActor
final class RadioTabModel: ObservableObject {
    /// The More list.
    @Published private(set) var menu: [RadioMenu.Entry] = []
    /// Moves on every change the readings follow, and each second while shown.
    @Published private(set) var revision: UInt64 = 0
    /// The readings are on screen and age once a second.
    private(set) var isShown = false

    /// How often shown readings are read again.
    static let tickInterval: Duration = .seconds(1)

    /// The Core's Setup category and page Antenna Setup opens.
    static let antennaCategory = "hardware"
    static let antennaPage = "hardware.antennaAlex"
    /// Fewer antenna ports than this is a radio without antenna control.
    static let antennaPortsWithControl = 3

    private let mirror: MirrorStore
    private let catalogFeed: CatalogFeed
    private let pages: SetupDescribedPages
    private let clock: any LinkClock
    private var watch: ToolMirrorWatch?
    private var tickTimer: (any LinkTimer)?
    /// Moves on each show and hide, so a tick from an earlier showing does nothing.
    private var showing: UInt64 = 0

    init(mirror: MirrorStore, catalogFeed: CatalogFeed, pages: SetupDescribedPages, clock: any LinkClock) {
        self.mirror = mirror
        self.catalogFeed = catalogFeed
        self.pages = pages
        self.clock = clock
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        watch.watch(catalogFeed.$catalog)
        watch.watch(catalogFeed.$needsNewerCore)
        watch.watch(mirror.$latestTelemetryReceipt)
        watch.watch(pages.$categories)
        watch.watch(pages.$isCurrent)
        self.watch = watch
        refresh()
    }

    convenience init(app: AppModel) {
        self.init(mirror: app.mirror, catalogFeed: app.main.catalogFeed, pages: app.setupPages,
                  clock: app.mirrorClock)
    }

    /// The readings came on screen: read them now, then once a second.
    func show() {
        guard !isShown else { return }
        isShown = true
        showing &+= 1
        revision &+= 1
        scheduleTick()
    }

    /// The readings left the screen: stop reading them on a timer.
    func hide() {
        guard isShown else { return }
        isShown = false
        showing &+= 1
        tickTimer?.cancel()
        tickTimer = nil
    }

    /// A tick is waiting on the clock.
    var isTicking: Bool { tickTimer != nil }

    /// The radio at a glance, as of `nowMilliseconds` on the mirror's clock.
    func glance(nowMilliseconds: Int64) -> [RadioAtAGlance.Row] {
        RadioAtAGlance.rows(inputs(nowMilliseconds: nowMilliseconds))
    }

    /// Protocol Info's lines.
    func protocolInfo(nowMilliseconds: Int64) -> [RadioAtAGlance.Row] {
        RadioAtAGlance.protocolInfo(inputs(nowMilliseconds: nowMilliseconds))
    }

    /// Why the Core has no radio, while it has none.
    func noRadio() -> String? {
        let inputs = inputs(nowMilliseconds: 0)
        return inputs.connected ? RadioAtAGlance.noRadio(inputs) : nil
    }

    func refresh() {
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        let next = RadioMenu.entries(RadioMenu.Inputs(
            items: catalogFeed.catalog?.radioItems, connected: connected, olderCore: catalogFeed.needsNewerCore,
            choosesRadio: mirror.capabilityVersion(StationRadio.capabilityName) >= 1
                && mirror.capabilityVersion(RecordStreamClient.capabilityName) >= 1,
            antennas: antennas()))
        if next != menu {
            menu = next
        }
        revision &+= 1
    }

    // MARK: Inside

    private func scheduleTick() {
        let expected = showing
        tickTimer = clock.schedule(after: Self.tickInterval) { [weak self] in
            await self?.tick(expected)
        }
    }

    private func tick(_ expected: UInt64) {
        guard isShown, showing == expected else { return }
        revision &+= 1
        scheduleTick()
    }

    private func inputs(nowMilliseconds: Int64) -> RadioAtAGlance.Inputs {
        let radio = watch?.object("radio")
        return RadioAtAGlance.Inputs(
            connected: mirror.isSnapshotComplete && !mirror.isStale, radio: radio?.values ?? [:],
            capabilities: mirror.capabilities, agreedMinor: mirror.agreedMinor ?? 0,
            board: catalogFeed.catalog?.board, ownSlices: mirror.objects(ofClass: "SliceModel").count,
            otherSlices: mirror.objects(ofClass: "SliceMarker").count, receipt: mirror.currentTelemetryReceipt,
            nowMilliseconds: nowMilliseconds)
    }

    /// Whether Antenna Setup has a page: the Core describes it; the radio
    /// has no antenna control; or not known now, with why.
    private func antennas() -> RadioMenu.Antennas {
        if let ports = catalogFeed.catalog?.board.rxAntennas.count, ports < Self.antennaPortsWithControl {
            return .absent
        }
        if pages.page(Self.antennaPage, in: Self.antennaCategory) != nil {
            return .described
        }
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        if connected, mirror.capabilityVersion("setupDescriptionVersion") < 1 {
            return .notDescribed(RadioMenu.olderCoreAntennaReason)
        }
        if pages.isCurrent {
            // The Core describes its Setup and leaves the antenna page out: no Alex board.
            return .absent
        }
        return .notDescribed(RadioMenu.antennaWaitingReason)
    }
}
