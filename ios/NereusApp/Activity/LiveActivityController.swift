// NereusSDR for iOS: the Live Activity's life: when it starts, what it shows, how often it changes, and its last message
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import os

/// The lock-screen card and the Dynamic Island (R-IOS-14, R-IOS-21, D24,
/// D25; spec sections 4.7 and 5.5 items 1 to 5 and 13).
///
/// - It starts the activity when the band first shows with the app in
///   front (iOS starts one only then), and ends it at once when the band
///   goes (Back to Cores, Disconnect).
/// - It shows the Core, the link, the active slice with its signal, mute,
///   who else is on the air while the radio transmits for them, and while
///   this phone is keyed the clock, forward power, SWR and the time left
///   before the Core's time-out. It changes the activity at most every 5
///   seconds, every second while keyed; a change the operator must see at
///   once (keyed or not, the link lost or back, mute, a message) goes at
///   once. The clocks count on in iOS between changes.
/// - Every change carries a stale date, worked out as it goes to iOS: 3
///   seconds on while keyed, with the card sent again each second changed
///   or not; 60 seconds on otherwise, sent again every 30. A card whose
///   app has died goes stale and says there's no news; a keyed one keeps
///   UNKEY and says it may still be on the air.
/// - The link lost with the app not in front lights the screen and buzzes
///   (an alert: iOS opens the island, or shows a banner on an iPhone
///   without one); the card keeps showing the tries until Cancel.
/// - UNKEY on the card or the opened island, and locking the phone (D24),
///   both run ``TransmitModel/stopEverythingHere(_:)``: the key through the
///   PTT's one ordered queue, VOX disarmed and the microphone stopped. Only
///   once that stop has been sent, and the Core no longer has this phone on
///   the air, does the card say "Unkeyed when you locked the phone. TX ran
///   0:42." or "Unkeyed. TX ran 0:47." Switching apps keeps transmitting
///   (D25): nothing here unkeys by itself.
/// - Just before iOS ends the activity at eight hours it leaves a last
///   message and ends it, but never while this phone is keyed: then the
///   last message waits for the unkey. Opening NereusSDR after that starts
///   a fresh one.
@MainActor
final class LiveActivityController: ObservableObject {
    /// What the activity is made from, read from the app's models.
    struct Inputs: Equatable {
        /// The band is the screen (a Core is chosen and connected or reconnecting).
        var bandShowing = false
        var stationName = ""
        var link: StationActivityAttributes.Link = .connecting
        var roundTripMs: Int?
        var retry: StationActivityAttributes.Retry?
        var slice: StationActivityAttributes.Slice?
        var meter: StationActivityAttributes.Meter?
        var muted = false
        /// This phone is on the air (its key, or its own VOX key).
        var transmitting = false
        /// The radio is on the air for someone else: who, as the PTT names
        /// them, empty when the Core names nobody; nil otherwise.
        var onAirElsewhere: String?
        /// The Core's count for the key now on.
        var keyedForSeconds: Int64 = 0
        var forwardWatts = 0.0
        var swr = 1.0
        /// -1 when no time-out applies.
        var timeOutRemainingSeconds: Int64 = -1
        /// The link went while this phone was keyed.
        var lostWhileKeyed = false
        /// The Core's newest stop of this phone's key.
        var stop: TransmitStopNotice?
        /// "Back on the air." is showing on the band.
        var backOnAir = false
        /// The newest stop of everything here, once it has been sent.
        var localStop: TransmitModel.LocalStop?
    }

    /// What the activity's buttons do in the app.
    struct Actions {
        /// Stops everything here, returning once the stop has been sent.
        var unkey: @MainActor () async -> Void = {}
        var setMuted: @MainActor (Bool) -> Void = { _ in }
        var cancelReconnecting: @MainActor () async -> Void = {}
        var reconnect: @MainActor () async -> Void = {}
    }

    /// The longest gap between changes while listening, and while keyed.
    static let listeningInterval: TimeInterval = 5
    static let keyedInterval: TimeInterval = 1
    /// How long a card that isn't keyed stays good, and how often it is
    /// sent again to keep it so.
    static let listeningFreshFor: TimeInterval = 60
    static let listeningHeartbeat: TimeInterval = 30
    /// iOS ends a Live Activity after eight hours.
    static let lifetime: TimeInterval = 8 * 3_600
    /// The last message goes this long before that.
    static let lastMessageLead: TimeInterval = 5 * 60
    /// A clock the Core moves by less than this stays where it is, so the
    /// activity isn't changed for a clock iOS already counts.
    static let clockSlack: TimeInterval = 1.5

    /// The alert when the link goes with the app not in front.
    static let lostAlertTitle = "Link lost"
    static let lostAlertBody = "The phone keeps trying to reconnect."
    static let lostWhileKeyedAlertBody = "The Core unkeyed itself. The phone keeps trying to reconnect."

    private let host: any LiveActivityHost
    private let clock: any LinkClock
    private let now: @Sendable () -> Date
    private static let logger = Logger(subsystem: "NereusSDR", category: "activity")

    private(set) var inputs = Inputs()
    private(set) var actions = Actions()
    private var sceneActive = false
    private var handle: (any LiveActivityHandle)?
    /// The last state sent, and when.
    private(set) var lastSent: StationActivityAttributes.ContentState?
    private var lastSentAt: Date?
    private var pending: (any LinkTimer)?
    /// The next send that moves the stale date on.
    private var heartbeat: (any LinkTimer)?
    private var lifetimeTimer: (any LinkTimer)?
    /// The activity ended at eight hours; the next one waits for the app to open.
    private var expired = false
    /// Eight hours came while keyed: the last message waits for the unkey.
    private var lastMessageDue = false
    /// The line shown in place of the signal.
    private var note: (text: String, good: Bool)?
    private var keyedSince: Date?
    private var timeOutAt: Date?
    /// How long the last key ran, for the note that follows it.
    private var lastRanSeconds = 0
    private var lastStopSerial: Int64?
    /// The Core's stop, seen while the PTT still read keyed.
    private var stopText: String?
    /// A stop of everything here, sent while the Core still had this phone
    /// on the air: its note waits for the unkey.
    private var heldLocalStop: TransmitModel.LocalStop?
    private var lastLocalStopSerial: Int?
    /// This phone was keyed when the link went: the Core has already unkeyed.
    private var keyedAtLoss = false
    /// Each change to the activity goes after the one before.
    private var sendTail: Task<Void, Never>?
    private var watches: Set<AnyCancellable> = []
    private var refreshQueued = false
    private var readInputs: (@MainActor () -> Inputs)?

    init(host: any LiveActivityHost, clock: any LinkClock = SystemLinkClock(),
         now: @escaping @Sendable () -> Date = Date.init) {
        self.host = host
        self.clock = clock
        self.now = now
    }

    deinit {
        pending?.cancel()
        heartbeat?.cancel()
        lifetimeTimer?.cancel()
    }

    // MARK: Wiring

    /// Follows the app's models, and takes the activity's buttons: UNKEY to
    /// the stop of everything here, the speaker to the band's sound, Cancel
    /// and Reconnect to the link-lost flow. The lock's stop is the transmit
    /// model's own; its result reaches here through ``Inputs/localStop``.
    func observe(app: AppModel, flow: ConnectionFlow?) {
        let main = app.main
        let transmit = main.transmit
        actions = Actions(unkey: { [weak transmit] in await transmit?.stopEverythingHere(.unkeyButton) },
                          setMuted: { [weak app] muted in app?.setAudioMuted(muted) },
                          cancelReconnecting: { [weak flow] in await flow?.cancelReconnecting() },
                          reconnect: { [weak flow] in await flow?.reconnect() })
        readInputs = { [weak app, weak flow] in
            guard let app else {
                return Inputs()
            }
            return Self.inputs(app: app, flow: flow)
        }
        var changes: [AnyPublisher<Void, Never>] = [
            app.objectWillChange.map { _ in () }.eraseToAnyPublisher(),
            main.objectWillChange.map { _ in () }.eraseToAnyPublisher(),
            transmit.objectWillChange.map { _ in () }.eraseToAnyPublisher(),
            main.slices.objectWillChange.map { _ in () }.eraseToAnyPublisher(),
            main.catalogFeed.objectWillChange.map { _ in () }.eraseToAnyPublisher(),
        ]
        if let flow {
            changes.append(flow.objectWillChange.map { _ in () }.eraseToAnyPublisher())
        }
        Publishers.MergeMany(changes)
            .sink { [weak self] _ in self?.queueRefresh() }
            .store(in: &watches)
        queueRefresh()
    }

    /// Takes the activity's buttons with these actions (the tests).
    func setActions(_ actions: Actions) {
        self.actions = actions
    }

    private func queueRefresh() {
        guard !refreshQueued else {
            return
        }
        refreshQueued = true
        Task { @MainActor [weak self] in
            guard let self else {
                return
            }
            self.refreshQueued = false
            if let read = self.readInputs {
                self.update(read())
            }
        }
    }

    /// The activity's inputs from the app's models now.
    static func inputs(app: AppModel, flow: ConnectionFlow?) -> Inputs {
        let main = app.main
        let transmit = main.transmit
        var inputs = Inputs()
        if let flow {
            inputs.bandShowing = flow.screen == .band
        } else {
            inputs.bandShowing = app.session != nil
        }
        inputs.stationName = main.coreName ?? app.coreHost ?? ""
        if flow?.offline == true {
            inputs.link = .offline
        } else if let lost = flow?.linkLost {
            inputs.link = .lost
            inputs.retry = StationActivityAttributes.Retry(attempt: lost.attempt, at: lost.retryAt,
                                                           stopped: lost.stopped)
        } else if app.connection == .connected {
            inputs.link = .up
        } else if case .waitingToRetry = app.connection {
            inputs.link = .lost
        } else {
            inputs.link = .connecting
        }
        inputs.roundTripMs = app.roundTripMs
        // The slice the operator works: the active one, as the desktop's RX
        // dashboard follows it (parity row M14); slice A only when none is.
        let entries = main.slices.entries
        if let entry = main.slices.active ?? entries.first(where: { $0.id == 0 }) ?? entries.first {
            // The level printed as the flag prints it (the Multimeter page's units).
            let meter = main.catalogFeed.catalog?.meters.sMeter
            let readout = main.meterReadout
            inputs.slice = StationActivityAttributes.Slice(letter: entry.slice.letter, colour: entry.slice.colour,
                                                           frequencyHz: entry.slice.frequencyHz,
                                                           mode: entry.modeLabel,
                                                           bandwidth: entry.slice.bandwidthText,
                                                           signalDbm: entry.signalDbm,
                                                           signalText: entry.signalDbm.flatMap {
                                                               readout.text(dbm: $0, meter: meter)
                                                           },
                                                           signalSpoken: entry.signalDbm.flatMap {
                                                               readout.spoken(dbm: $0, meter: meter)
                                                           })
        }
        if let meter = main.catalogFeed.catalog?.meters.sMeter {
            inputs.meter = StationActivityAttributes.Meter(
                minDbm: meter.minDbm, s9Dbm: meter.s9Dbm, maxDbm: meter.maxDbm,
                marks: FlagLevelBar.marks(meter).map { StationActivityAttributes.Meter.Mark(label: $0.label,
                                                                                            dbm: $0.dbm) })
        }
        inputs.muted = app.audioMuted
        inputs.transmitting = transmit.transmittingHere
        inputs.onAirElsewhere = transmit.onAirElsewhere
        inputs.keyedForSeconds = transmit.keyedForSeconds
        inputs.forwardWatts = transmit.forwardWatts
        inputs.swr = transmit.swr
        inputs.timeOutRemainingSeconds = transmit.timeOutRemainingSeconds
        inputs.lostWhileKeyed = transmit.ptt.state == .linkLost
        inputs.stop = transmit.ptt.stop
        inputs.backOnAir = flow?.backOnAir ?? false
        inputs.localStop = transmit.localStop
        return inputs
    }

    // MARK: What happens

    /// The app came to the front, or left it. Opening the app starts a
    /// fresh activity after one that ended, and clears a message the
    /// operator has now seen in the app.
    func sceneChanged(active: Bool) {
        let opened = active && !sceneActive
        sceneActive = active
        if opened {
            expired = false
            note = nil
            refresh(urgent: true)
        }
    }

    /// One of the activity's buttons, from its intent.
    func perform(_ action: ActivityAction) async {
        switch action {
        case .unkey:
            await actions.unkey()
        case .setMuted(let muted):
            actions.setMuted(muted)
        case .cancelReconnecting:
            await actions.cancelReconnecting()
        case .reconnect:
            await actions.reconnect()
        }
    }

    /// The app's models changed.
    func update(_ next: Inputs) {
        let previous = inputs
        inputs = next
        let date = now()
        var urgent = false
        var alert: LiveActivityAlert?

        // The Core's own stop of this phone's key: its words, as sent. It
        // may come a moment before the PTT goes idle.
        var freshStop: TransmitStopNotice?
        if let stop = next.stop, stop.serial != lastStopSerial {
            lastStopSerial = stop.serial
            freshStop = stop
        }
        // A stop of everything here that has been sent.
        var freshLocal: TransmitModel.LocalStop?
        if let local = next.localStop, local.serial != lastLocalStopSerial {
            lastLocalStopSerial = local.serial
            freshLocal = local
        }
        if next.transmitting {
            if !previous.transmitting {
                note = nil
                stopText = nil
                heldLocalStop = nil
                keyedSince = nil
                urgent = true
            }
            if let freshStop {
                stopText = freshStop.text
            }
            if let freshLocal, freshLocal.wasTransmitting {
                // Sent, but the Core still has this phone on the air: the note waits.
                heldLocalStop = freshLocal
            }
            let since = date.addingTimeInterval(-TimeInterval(next.keyedForSeconds))
            if keyedSince == nil || abs(since.timeIntervalSince(keyedSince ?? since)) > Self.clockSlack {
                keyedSince = since
            }
            if next.timeOutRemainingSeconds >= 0 {
                let end = date.addingTimeInterval(TimeInterval(next.timeOutRemainingSeconds))
                if timeOutAt == nil || abs(end.timeIntervalSince(timeOutAt ?? end)) > Self.clockSlack {
                    timeOutAt = end
                }
            } else {
                timeOutAt = nil
            }
        } else if previous.transmitting {
            lastRanSeconds = Int(date.timeIntervalSince(keyedSince ?? date).rounded())
            if let text = freshStop?.text ?? stopText {
                note = (text, false)
            } else if let local = freshLocal ?? heldLocalStop, local.wasTransmitting {
                noteLocalStop(local)
            }
            stopText = nil
            heldLocalStop = nil
            keyedSince = nil
            timeOutAt = nil
            urgent = true
        } else if let freshStop {
            note = (freshStop.text, false)
            urgent = true
        } else if let freshLocal, freshLocal.wasTransmitting {
            // The Core had already let go when the stop's answers came in.
            noteLocalStop(freshLocal)
            urgent = true
        }
        if next.backOnAir && !previous.backOnAir {
            note = (ActivityWords.backOnAir, true)
            urgent = true
        }
        if next.link == .lost && previous.link != .lost {
            // The PTT may learn of the loss a moment after the link does.
            keyedAtLoss = next.lostWhileKeyed || next.transmitting || previous.transmitting
            urgent = true
            if !sceneActive {
                alert = LiveActivityAlert(title: Self.lostAlertTitle,
                                          body: keyedAtLoss ? Self.lostWhileKeyedAlertBody : Self.lostAlertBody)
            }
        } else if next.link != .lost {
            keyedAtLoss = false
        }
        if next.link != previous.link || next.muted != previous.muted || next.retry != previous.retry
            || next.stationName != previous.stationName || next.lostWhileKeyed != previous.lostWhileKeyed
            || next.bandShowing != previous.bandShowing || next.onAirElsewhere != previous.onAirElsewhere {
            urgent = true
        }
        if lastMessageDue && !next.transmitting {
            leaveLastMessage()
            return
        }
        refresh(urgent: urgent, alert: alert)
    }

    /// The words for a stop of everything here that ended a transmission.
    private func noteLocalStop(_ stop: TransmitModel.LocalStop) {
        switch stop.cause {
        case .lock:
            note = (ActivityWords.unkeyedByLock(ran: lastRanSeconds), false)
        case .unkeyButton:
            note = (ActivityWords.unkeyedHere(ran: lastRanSeconds), false)
        case .interruption, .microphoneLost:
            break
        }
    }

    /// Waits until every change sent so far has reached iOS (the tests).
    func settle() async {
        await sendTail?.value
    }

    // MARK: Sending

    /// The state the activity shows now.
    func content() -> StationActivityAttributes.ContentState {
        StationActivityAttributes.ContentState(
            stationName: inputs.stationName, link: inputs.link,
            roundTripMs: inputs.link == .up ? inputs.roundTripMs : nil,
            retry: inputs.link == .lost ? inputs.retry : nil,
            lostWhileKeyed: inputs.link == .lost && (keyedAtLoss || inputs.lostWhileKeyed),
            slice: inputs.slice, meter: inputs.meter, muted: inputs.muted,
            keyed: inputs.transmitting, keyedSince: inputs.transmitting ? keyedSince : nil,
            forwardWatts: inputs.transmitting ? inputs.forwardWatts : 0,
            swr: inputs.transmitting ? inputs.swr : 1,
            timeOutAt: inputs.transmitting ? timeOutAt : nil,
            onAirFrom: inputs.transmitting ? nil : inputs.onAirElsewhere,
            // A lost card shows the tries, never a note left from before;
            // the note comes back with the link.
            message: inputs.link == .lost ? "" : note?.text ?? "",
            messageGood: inputs.link == .lost ? false : note?.good ?? false)
    }

    /// The stale date a change carries when it goes to iOS at `date`: 3
    /// seconds on while keyed, a minute otherwise.
    static func staleDate(for state: StationActivityAttributes.ContentState, at date: Date) -> Date {
        date.addingTimeInterval(state.keyed ? StationActivityAttributes.keyedFreshFor : listeningFreshFor)
    }

    private func refresh(urgent: Bool, alert: LiveActivityAlert? = nil) {
        guard inputs.bandShowing else {
            endNow()
            return
        }
        if handle?.isActive != true {
            // Not running: start one if iOS allows (it carries the state now).
            handle = nil
            _ = startIfAllowed()
            return
        }
        let state = content()
        guard state != lastSent || alert != nil else {
            return
        }
        let interval = inputs.transmitting ? Self.keyedInterval : Self.listeningInterval
        let elapsed = lastSentAt.map { now().timeIntervalSince($0) } ?? .infinity
        if urgent || alert != nil || elapsed >= interval {
            send(state, alert: alert)
        } else if pending == nil {
            let wait = max(0, interval - elapsed)
            pending = clock.schedule(after: .milliseconds(Int64((wait * 1000).rounded(.up)))) { [weak self] in
                await self?.flush()
            }
        }
    }

    private func flush() {
        pending = nil
        guard handle?.isActive == true else {
            return
        }
        let state = content()
        if state != lastSent {
            send(state, alert: nil)
        }
    }

    /// The card is sent again, changed or not, so its stale date keeps
    /// moving while the app lives: each second while keyed, else every 30.
    private func armHeartbeat(_ state: StationActivityAttributes.ContentState) {
        heartbeat?.cancel()
        let every = state.keyed ? Self.keyedInterval : Self.listeningHeartbeat
        heartbeat = clock.schedule(after: .milliseconds(Int64(every * 1000))) { [weak self] in
            await self?.beat()
        }
    }

    private func beat() {
        heartbeat = nil
        guard handle?.isActive == true, inputs.bandShowing else {
            return
        }
        send(content(), alert: nil)
    }

    private func send(_ state: StationActivityAttributes.ContentState, alert: LiveActivityAlert?) {
        pending?.cancel()
        pending = nil
        lastSent = state
        lastSentAt = now()
        guard let handle else {
            return
        }
        armHeartbeat(state)
        let previous = sendTail
        let now = now
        sendTail = Task { @MainActor in
            await previous?.value
            // Worked out as it goes, not as it was queued.
            await handle.update(state, staleDate: Self.staleDate(for: state, at: now()), alert: alert)
        }
    }

    /// Starts the activity when iOS allows: the app in front, Live
    /// Activities on, and not waiting for the app to open after eight hours.
    private func startIfAllowed() -> Bool {
        guard sceneActive, !expired, host.activitiesEnabled else {
            return false
        }
        let state = content()
        let started = now()
        guard let next = host.start(startedAt: started, state: state,
                                    staleDate: Self.staleDate(for: state, at: started)) else {
            return false
        }
        handle = next
        lastSent = state
        lastSentAt = started
        lastMessageDue = false
        armHeartbeat(state)
        lifetimeTimer?.cancel()
        let delay = Self.lifetime - Self.lastMessageLead
        lifetimeTimer = clock.schedule(after: .seconds(Int64(delay))) { [weak self] in
            await self?.lastMessageTime()
        }
        return true
    }

    /// Five minutes before iOS's eight hours. While keyed the island stays
    /// (D25) and the last message waits for the unkey.
    private func lastMessageTime() {
        lifetimeTimer = nil
        if inputs.transmitting {
            lastMessageDue = true
            return
        }
        leaveLastMessage()
    }

    /// The last message, and the end, with a stale date like every change.
    /// The sound carries on; opening NereusSDR starts a fresh activity.
    private func leaveLastMessage() {
        lastMessageDue = false
        guard let current = handle, current.isActive else {
            return
        }
        note = (ActivityWords.eightHours, false)
        let state = content()
        lastSent = state
        lastSentAt = now()
        handle = nil
        expired = true
        pending?.cancel()
        pending = nil
        heartbeat?.cancel()
        heartbeat = nil
        let previous = sendTail
        let now = now
        sendTail = Task { @MainActor in
            await previous?.value
            await current.end(state, staleDate: Self.staleDate(for: state, at: now()), immediately: false)
        }
    }

    /// The band went: the activity goes at once.
    private func endNow() {
        pending?.cancel()
        pending = nil
        heartbeat?.cancel()
        heartbeat = nil
        lifetimeTimer?.cancel()
        lifetimeTimer = nil
        lastMessageDue = false
        note = nil
        keyedSince = nil
        timeOutAt = nil
        lastSent = nil
        lastSentAt = nil
        guard let current = handle else {
            return
        }
        handle = nil
        let previous = sendTail
        sendTail = Task { @MainActor in
            await previous?.value
            await current.end(nil, staleDate: nil, immediately: true)
        }
    }
}
