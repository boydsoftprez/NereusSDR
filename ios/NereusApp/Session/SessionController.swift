// NereusSDR for iOS: a long session: what the band asks for, the screen, the sleep timer and the data counters
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMedia
import UIKit

/// Runs a long session on this phone (R-IOS-22, R-IOS-23, R-IOS-10's phone
/// half, D27, D28; spec sections 4.7, 5.4 items 11 to 13 and 5.5 items 9 to
/// 12). From the phone's settings, its network, whether the app is in
/// front, Low Power Mode and the heat, it settles ``SessionPolicy``'s
/// request and hands it to the band's subscriber. It counts the data used,
/// notes the first time on cellular and the month's 5 GB on the band, marks
/// on the waterfall a stretch the band was not sent, runs the sleep timer,
/// keeps the measurement log (plan Task 68) and, once ``start()`` is
/// called, keeps the screen on as chosen.
@MainActor
final class SessionController: ObservableObject {
    /// Where the controller reads the phone.
    struct Sources {
        var power: ThermalAndPowerWatcher
        /// The phone's network; nil takes it as Wi-Fi throughout.
        var network: (any NetworkWatch)?
        var traffic: () -> TrafficCounter.Totals
        var now: () -> Date
        /// How often the counters are read and the sleep timer checked.
        var tick: Duration
        /// The traffic by kind, for the measurement log.
        var kinds: () -> TrafficCounter.ByKind = { TrafficCounter.shared.readingByKind }
        /// The battery's level in percent, for the measurement log.
        var battery: @MainActor () -> Int? = { nil }
        /// Where the measurement log is kept; nil keeps it in memory only.
        var measurementFile: URL?

        /// A cool phone on Wi-Fi, for the screens' own tests.
        @MainActor static func steady() -> Sources {
            Sources(power: ThermalAndPowerWatcher(read: { .steady }), network: nil,
                    traffic: { TrafficCounter.shared.reading }, now: Date.init, tick: .seconds(5))
        }

        /// The phone itself.
        @MainActor static func system() -> Sources {
            Sources(power: .system(), network: SystemNetworkWatch(), traffic: { TrafficCounter.shared.reading },
                    now: Date.init, tick: .seconds(5), kinds: { TrafficCounter.shared.readingByKind },
                    battery: { MeasurementLog.systemBatteryPercent() }, measurementFile: MeasurementLog.defaultFile)
        }
    }

    /// A note on the band: its words and how it is marked.
    struct Note: Equatable {
        enum Kind: Equatable {
            case cellular
            case monthWarning
        }

        let kind: Kind
        let text: String
        let presentationID: UInt64
    }

    struct SleepExpiry {
        let listeningOwner: UInt64
        let timer: SleepTimer.Expiry
    }

    /// How long a note stays on the band.
    static let noteShown: Duration = .seconds(10)
    /// The shortest stretch away the waterfall marks.
    static let shortestMarkedStretch: TimeInterval = 60

    /// The phone is on cellular now.
    @Published private(set) var cellular = false
    /// The phone's network type, only as far as its current path proves it.
    @Published private(set) var networkLabel = "Network unavailable"
    /// What the band asks for now.
    @Published private(set) var request = SessionPolicy.Mode.full.request
    /// The mode the band is at now, after Low Power Mode and the heat.
    @Published private(set) var mode = SessionPolicy.Mode.full
    /// The note on the band, if any.
    @Published private(set) var note: Note?

    let meter: DataUseMeter
    let sleepTimer: SleepTimer
    let power: ThermalAndPowerWatcher
    /// The measurement log: a row a minute while a session runs.
    let measurements: MeasurementLog

    private let settings: PhoneSettings
    private let subscriber: BandSubscriber
    private let band: BandModel
    private let transmit: TransmitModel
    private let network: (any NetworkWatch)?
    private let now: () -> Date
    private let battery: @MainActor () -> Int?
    private let tickInterval: Duration
    private var watches: Set<AnyCancellable> = []
    private var connected = false
    private var linkUp = false
    private var hasConnectedWithinOwner = false
    private var inForeground = true
    private var bandShowing = false
    private var appliesScreen = false
    private var ticker: Task<Void, Never>?
    /// The operator's current listening intent. A transport or media owner
    /// can change without ending this session.
    private var listeningOwner: UInt64?
    private var noteTimer: Task<Void, Never>?
    private var nextNotePresentationID: UInt64 = 0
    private var noteTimerRevision: UInt64 = 0
    #if DEBUG
    /// Holds one note timer at its wake boundary for owner and cancellation tests.
    var noteTimerWaitForTesting: (@MainActor () async -> Void)?
    #endif
    private var noteAcknowledged = false
    private var cellularNotePending = false
    /// Ends the session through the connecting flow; set by the app.
    private var disconnect: @MainActor (SleepExpiry) async -> Bool = { _ in false }
    /// When the band last stopped for the phone being away, and whether the
    /// phone was locked meanwhile.
    private var awaySince: Date?
    private var lockedWhileAway = false
    /// The sleep timer's choice as last seen, so a new choice restarts it.
    private var sleepChoice = SleepTimer.Choice.off

    init(settings: PhoneSettings, subscriber: BandSubscriber, band: BandModel, transmit: TransmitModel,
         sources: Sources) {
        self.settings = settings
        self.subscriber = subscriber
        self.band = band
        self.transmit = transmit
        power = sources.power
        network = sources.network
        now = sources.now
        battery = sources.battery
        tickInterval = sources.tick
        meter = DataUseMeter(settings: settings, reading: sources.traffic, now: sources.now)
        measurements = MeasurementLog(file: sources.measurementFile, now: sources.now, traffic: sources.kinds)
        sleepTimer = SleepTimer(now: sources.now)
        sleepTimer.onExpiry = { [weak self] expiry in
            guard let self, let owner = self.listeningOwner, self.sleepTimer.isCurrent(expiry),
                  self.settings.sleepTimer == expiry.choice else { return false }
            return await self.disconnect(SleepExpiry(listeningOwner: owner, timer: expiry))
        }
        settings.sleepTimerWillChange = { [weak self] choice in
            guard let self else { return }
            self.sleepChoice = choice
            if self.connected { self.sleepTimer.chosen(choice) }
            else { self.sleepTimer.invalidatePendingExpiry() }
        }
        sleepChoice = settings.sleepTimer
        wire()
        settle()
    }

    /// How the sleep timer ends the session: returns false while it can't
    /// (this phone is keyed).
    func setDisconnect(_ action: @escaping @MainActor (SleepExpiry) async -> Bool) {
        disconnect = action
    }

    func isCurrent(_ expiry: SleepExpiry) -> Bool {
        listeningOwner == expiry.listeningOwner && settings.sleepTimer == expiry.timer.choice
            && sleepTimer.isCurrent(expiry.timer)
    }

    func cancelSleepExpiry() { sleepTimer.invalidatePendingExpiry() }

    /// From now on the screen stays on as Setup chooses. Only the app does
    /// this, so the other screens' tests keep the system's idle timer.
    func start() {
        appliesScreen = true
        applyScreen()
    }

    // MARK: What the app tells it

    /// The app came to the front (true) or went to the background: locked or
    /// in another app.
    func sceneChanged(inForeground front: Bool) {
        guard front != inForeground else {
            return
        }
        inForeground = front
        if !front {
            lockedWhileAway = !UIApplication.shared.isProtectedDataAvailable
        }
        settle()
        applyScreen()
        refreshNote()
    }

    /// The phone locked while away: the stretch is marked Locked.
    func phoneLocked() {
        if !inForeground {
            lockedWhileAway = true
        }
    }

    /// The band's tab is on screen with the band connected, or not.
    func bandShown(_ shown: Bool) {
        guard shown != bandShowing else {
            return
        }
        bandShowing = shown
        applyScreen()
        refreshNote()
    }

    /// A new operator listening intent starts its own counter and deadline.
    /// Recovery within that intent leaves both running.
    func sessionStarted(owner: UInt64) {
        guard listeningOwner != owner else { return }
        if listeningOwner != nil { meter.sessionEnded(cellular: cellular) }
        retireNote()
        clearAway()
        listeningOwner = owner
        linkUp = false
        hasConnectedWithinOwner = false
        meter.sessionStarted()
        sleepTimer.disconnected()
        startTicking()
        settle()
        measurements.begin(measurementConditions, power: measurementPower)
        refreshNote()
    }

    /// Only the current intent can end the visible long session. An old
    /// teardown completing after a new start has no effect.
    func sessionEnded(owner: UInt64) {
        guard listeningOwner == owner else { return }
        meter.sessionEnded(cellular: cellular)
        measurements.end(power: measurementPower)
        retireNote()
        sleepTimer.disconnected()
        ticker?.cancel()
        ticker = nil
        listeningOwner = nil
        connected = false
        linkUp = false
        hasConnectedWithinOwner = false
        clearAway()
        applyScreen()
        refreshNote()
    }

    /// The connection's state changed within the listening intent.
    func connectionChanged(_ state: ConnectionState) {
        let up = state == .connected
        connected = listeningOwner != nil && state != .notConnected
        linkUp = listeningOwner != nil && up
        if up && listeningOwner != nil {
            hasConnectedWithinOwner = true
            // The sleep timer runs from the first time the Core is up; a
            // link that comes back keeps the time it had.
            sleepTimer.connected(settings.sleepTimer)
            noteCellularFirstTime()
        }
        settle()
        applyScreen()
        refreshNote()
    }

    // MARK: Inside

    private func wire() {
        network?.start { [weak self] path in
            self?.networkChanged(path)
        }
        power.$reading
            .dropFirst()
            .sink { [weak self] _ in
                Task { @MainActor [weak self] in
                    self?.settle()
                    self?.applyScreen()
                }
            }
            .store(in: &watches)
        settings.objectWillChange
            .sink { [weak self] _ in
                // Sent before the change: settle once it has landed.
                Task { @MainActor [weak self] in
                    guard let self else {
                        return
                    }
                    let choice = self.settings.sleepTimer
                    if choice != self.sleepChoice {
                        self.sleepChoice = choice
                        if self.connected {
                            self.sleepTimer.chosen(choice)
                        }
                    }
                    self.settle()
                    self.applyScreen()
                    self.refreshNote()
                }
            }
            .store(in: &watches)
        transmit.$transmittingHere
            .sink { [weak self] _ in
                // After the transmit model's own keep-awake, on the next turn.
                Task { @MainActor [weak self] in self?.applyScreen() }
            }
            .store(in: &watches)
        NotificationCenter.default.publisher(for: UIApplication.protectedDataWillBecomeUnavailableNotification)
            .sink { [weak self] _ in
                Task { @MainActor [weak self] in self?.phoneLocked() }
            }
            .store(in: &watches)
    }

    private func networkChanged(_ path: NetworkPath) {
        let next = path.online && path.cellular
        networkLabel = !path.online ? "Network unavailable" : next ? "Cellular" : path.wifi ? "Wi-Fi" : "Network available"
        if next != cellular {
            // What came before the change counts on the network it came over.
            meter.sample(cellular: cellular)
            meter.invalidateRate()
            cellular = next
        }
        settle()
        if linkUp {
            noteCellularFirstTime()
        }
        refreshNote()
    }

    /// The circumstances now.
    var circumstances: SessionPolicy.Circumstances {
        let reading = power.reading
        return SessionPolicy.Circumstances(
            network: cellular ? .cellular : .wifi, wifiMode: settings.wifiMode, cellularMode: settings.cellularMode,
            inForeground: inForeground, soundOnlyWhenAway: settings.soundOnlyWhenAway,
            lowPowerMode: reading.lowPowerMode, lowPowerDropsToSaver: settings.lowPowerDropsToSaver,
            heat: reading.heat, hotPhoneSlows: settings.hotPhoneSlows)
    }

    /// Settles the request and hands it to the band; marks a stretch away
    /// when the band comes back after one.
    private func settle() {
        let circumstances = circumstances
        let next = SessionPolicy.request(for: circumstances)
        let nextMode = SessionPolicy.mode(for: circumstances)
        let away = listeningOwner != nil && hasConnectedWithinOwner
            && !circumstances.inForeground && !next.subscribes
        if away, awaySince == nil {
            awaySince = now()
        } else if !away, let since = awaySince {
            let locked = lockedWhileAway
            clearAway()
            markStretch(from: since, to: now(), locked: locked)
        }
        if mode != nextMode {
            mode = nextMode
        }
        if request != next {
            request = next
        }
        subscriber.session = next
        let conditions = Self.stackConditions(circumstances, mode: nextMode)
        if band.stackConditions != conditions {
            band.stackConditions = conditions
        }
        measurements.sample(measurementConditions, power: measurementPower)
    }

    /// What the measurement log's row is measured under now.
    private var measurementConditions: MeasurementLog.Conditions {
        MeasurementLog.Conditions(network: networkLabel, inFront: inForeground, mode: mode, request: request)
    }

    /// The phone's power for the measurement log now.
    private var measurementPower: MeasurementLog.Power {
        let reading = power.reading
        return MeasurementLog.Power(batteryPercent: battery(), charging: reading.charging, heat: reading.heat)
    }

    /// What the 3D view follows (JJ's board, recommendations 4 and 5): Low
    /// Power Mode; a hot phone, by the heat rule the band's frame rate
    /// follows (serious or critical, with Slow the band when hot on); and a
    /// band saving data: Saver, or cellular with anything but Full chosen.
    static func stackConditions(_ circumstances: SessionPolicy.Circumstances,
                                mode: SessionPolicy.Mode) -> BandModel.StackConditions {
        let hot = circumstances.hotPhoneSlows && (circumstances.heat == .serious || circumstances.heat == .critical)
        let savesData = mode >= .saver || (circumstances.network == .cellular && circumstances.cellularMode != .full)
        return BandModel.StackConditions(lowPower: circumstances.lowPowerMode, hot: hot, savesData: savesData)
    }

    private func clearAway() {
        awaySince = nil
        lockedWhileAway = false
    }

    private func markStretch(from start: Date, to end: Date, locked: Bool) {
        guard end.timeIntervalSince(start) >= Self.shortestMarkedStretch else {
            return
        }
        band.markAway(Self.awayText(from: start, to: end, locked: locked))
    }

    /// "Locked 19:42 to 20:15 · sound only" (spec section 5.5 item 12), or
    /// "In another app ..." when the phone was not locked meanwhile.
    static func awayText(from start: Date, to end: Date, locked: Bool, timeZone: TimeZone = .current) -> String {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = timeZone
        let place = locked ? "Locked" : "In another app"
        return "\(place) \(formatter.string(from: start)) to \(formatter.string(from: end)) \u{00B7} sound only"
    }

    /// The screen stays on as chosen, or always while keyed.
    private func applyScreen() {
        guard appliesScreen else {
            return
        }
        let on = settings.keepScreenOn.screenStaysOn(bandShowing: bandShowing && connected && inForeground,
                                                     charging: power.reading.charging,
                                                     keyed: transmit.transmittingHere)
        if UIApplication.shared.isIdleTimerDisabled != on {
            UIApplication.shared.isIdleTimerDisabled = on
        }
    }

    // MARK: Notes

    private func noteCellularFirstTime() {
        guard cellular, linkUp, !settings.cellularNoteShown else {
            return
        }
        cellularNotePending = true
        refreshNote()
    }

    private var canShowNote: Bool { bandShowing && inForeground && linkUp }

    /// A displayed card belongs to one listening intent. A notice that was
    /// never shown remains due in the pending flag or the month's counter.
    private func retireNote() {
        cancelNoteTimer()
        note = nil
        noteAcknowledged = false
    }

    private func cancelNoteTimer() {
        noteTimerRevision &+= 1
        noteTimer?.cancel()
        noteTimer = nil
    }

    private func makeNote(kind: Note.Kind, text: String) -> Note {
        nextNotePresentationID &+= 1
        return Note(kind: kind, text: text, presentationID: nextNotePresentationID)
    }

    /// The card calls this when SwiftUI has put the note on the visible band.
    /// Merely sampling bytes or preparing a note never consumes a one-time notice.
    func noteAppeared(_ shown: Note) {
        guard canShowNote, note == shown, !noteAcknowledged else { return }
        noteAcknowledged = true
        switch shown.kind {
        case .cellular:
            settings.cellularNoteShown = true
            cellularNotePending = false
        case .monthWarning:
            meter.markWarningShown()
        }
        startNoteTimer(for: shown)
    }

    /// Hold due notices until the band can show them. A second due notice
    /// waits for the current card to finish instead of replacing it.
    private func refreshNote() {
        if let current = note, current.kind == .cellular,
           current.text != DataModeText.cellularNote(settings.cellularMode) {
            // A mode change must not restore an acknowledged card with the
            // old mode after the band was hidden. The once-only flag stays.
            retireNote()
        }
        guard canShowNote else {
            cancelNoteTimer()
            if !noteAcknowledged { note = nil }
            return
        }
        if let current = note {
            let staleCellular = current.kind == .cellular && !cellular && !noteAcknowledged
            let staleWarning = current.kind == .monthWarning && !meter.warningDue() && !noteAcknowledged
            if !staleCellular && !staleWarning {
                if noteAcknowledged && noteTimer == nil { startNoteTimer(for: current) }
                return
            }
            cancelNoteTimer()
            note = nil
        }
        noteAcknowledged = false
        if meter.warningDue() {
            note = makeNote(kind: .monthWarning,
                            text: DataModeText.monthWarning(DataUseMeter.text(meter.monthCellular.total)))
        } else if cellular && cellularNotePending && !settings.cellularNoteShown {
            note = makeNote(kind: .cellular, text: DataModeText.cellularNote(settings.cellularMode))
        }
    }

    private func startNoteTimer(for shown: Note) {
        cancelNoteTimer()
        let revision = noteTimerRevision
        noteTimer = Task { @MainActor [weak self] in
            #if DEBUG
            if let wait = self?.noteTimerWaitForTesting { await wait() }
            else { do { try await Task.sleep(for: Self.noteShown) } catch { return } }
            #else
            do { try await Task.sleep(for: Self.noteShown) } catch { return }
            #endif
            guard let self else { return }
            guard self.noteTimerRevision == revision, self.note == shown, self.canShowNote else { return }
            self.noteTimer = nil
            self.note = nil
            self.noteAcknowledged = false
            self.refreshNote()
        }
    }

    // MARK: Ticking

    private func startTicking() {
        ticker?.cancel()
        let interval = tickInterval
        ticker = Task { @MainActor [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: interval)
                guard !Task.isCancelled, let self else {
                    return
                }
                await self.tick()
            }
        }
    }

    /// Reads the counters and checks the sleep timer.
    func tick() async {
        meter.sample(cellular: cellular)
        measurements.sample(measurementConditions, power: measurementPower)
        refreshNote()
        await sleepTimer.tick()
    }
}
