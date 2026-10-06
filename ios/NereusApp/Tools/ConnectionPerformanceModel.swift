// NereusSDR for iOS: one bounded live collector for connection diagnostics
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMedia
import NereusMirror

@MainActor
final class ConnectionPerformanceModel: ObservableObject {
    private weak var app: AppModel?
    private let playback: AudioPlaybackCore?
    private let nowMilliseconds: @Sendable () -> Int64
    private let nowNanoseconds: @Sendable () -> Int64
    @Published private(set) var input: ConnectionPerformanceInput

    private var history = DiagnosticsHistory()
    private var range: DiagnosticsRange = .oneMinute
    private var current: [DiagnosticsMetric: DiagnosticsReading<Double>] = [:]
    private var details: [PerformanceDetailID: DiagnosticsReading<String>] = [:]
    private var controlRoute = ConnectionPerformanceModel.absentRoute("No control connection is open")
    private var mediaRoute = ConnectionPerformanceModel.absentRoute("No sound or display connection is open")
    private var status: DiagnosticsReading<String> = .disconnected(reason: "No Core selected")
    private var attempt: ConnectionAttempt?
    private var reset: DiagnosticsActionAvailability = .disabled(reason: "No active diagnostics session")
    private var owner: UInt64?
    private var mediaOwner: UInt64?
    private var station: StationSession?
    private var sessionStartedAt: Int64?
    private var arm: UInt64 = 0
    private var inFlightArm: UInt64?
    private var timer: Task<Void, Never>?
    private var pendingRead: Task<Void, Never>?
    private var deferredPublication: Task<Void, Never>?
    private var lastPublishedAt: Int64?
    private var visible = false
    private var segment: UInt64 = 0
    private let diagnostics: LinkDiagnostics
    private struct DiagnosticRouteIdentity: Equatable {
        let owner: UInt64?
        let generation: Int?
        let routeID: Int?
        let mediaID: String?
        let observation: SelectedRouteObservation
        let carrierGeneration: Int?
        let carrierRouteID: Int?
        let carrierRank: Int?
        let carrierObservation: SelectedRouteObservation?
    }
    private var controlDiagnosticRoutes = ObservedRouteDiagnosticLedger<DiagnosticRouteIdentity>()
    private var mediaDiagnosticRoutes = ObservedRouteDiagnosticLedger<DiagnosticRouteIdentity>()
    private var mirrorIdentity: UInt64?
    private var lastCoreReceipt: (identity: UInt64, sequence: Int64)?
    private var lastCoreObservedAt: Int64?
    private var lastRadioRttAge: (receivedAt: Int64, reportedMs: Double)?
    private var lastControlRttObservedAt: Int64?
    private var lastLocalObservedAt: Int64?
    private var rates = DiagnosticsRateLedger()
    private var maxFreshRadioRTT: Double?
    private var underrunBaseline: UInt64?
    private var latestLifetimeUnderruns: UInt64?
    private var activityRevision: UInt64 = 0
    private var lastActivity: MediaClockPlaybackActivity?
    #if DEBUG
    var beforePublicationForTesting: (@MainActor () async -> Void)?
    var finalMediaReadForTesting: (@MainActor () async -> SelectedMediaDiagnosticsSnapshot)?
    func stopAutomaticTicksForTesting() { timer?.cancel(); timer = nil }
    func restartObservationWindowForTesting() {
        guard let station, let owner, let mediaOwner else { return }
        attach(session: station, owner: owner, mediaOwner: mediaOwner)
        stopAutomaticTicksForTesting()
    }
    private(set) var publicationCountForTesting = 0
    var inFlightArmForTesting: UInt64? { inFlightArm }
    func tickForTesting() { tick(arm: arm) }
    func historyCountForTesting(_ metric: DiagnosticsMetric) -> Int { history.rawObservationCount(metric) }
    #endif

    init(app: AppModel, playback: AudioPlaybackCore?,
         nowMilliseconds: @escaping @Sendable () -> Int64 = { SystemLinkClock().nowMilliseconds },
         nowNanoseconds: @escaping @Sendable () -> Int64 = {
             Int64(clamping: DispatchTime.now().uptimeNanoseconds)
         }, diagnostics: LinkDiagnostics = .shared) {
        self.app = app
        self.diagnostics = diagnostics
        self.playback = playback
        self.nowMilliseconds = nowMilliseconds
        self.nowNanoseconds = nowNanoseconds
        input = Self.blank(at: nowMilliseconds())
    }

    /// A new logical owner synchronously revokes every old publication and
    /// in-flight read. A held OLD completion cannot clear NEW's marker.
    func attach(session: StationSession, owner: UInt64, mediaOwner: UInt64) {
        retire()
        self.station = session
        sessionStartedAt = nowMilliseconds()
        self.owner = owner
        self.mediaOwner = mediaOwner
        segment &+= 1
        rates.clear()
        maxFreshRadioRTT = nil
        underrunBaseline = nil
        latestLifetimeUnderruns = nil
        reset = .disabled(reason: "Waiting for active playback counters")
        let capturedArm = arm
        timer = Task { [weak self] in
            while !Task.isCancelled {
                guard let self, self.arm == capturedArm else { return }
                self.tick(arm: capturedArm)
                try? await Task.sleep(for: .seconds(1))
            }
        }
    }

    func retire() {
        arm &+= 1
        activityRevision &+= 1
        if let activity = lastActivity, let mediaOwner, let app {
            let off = MediaClockPlaybackActivity(mediaID: activity.mediaID,
                playbackLifetime: activity.playbackLifetime, outputEpoch: activity.outputEpoch,
                revision: activityRevision, generation: activity.generation,
                observedNs: nowNanoseconds(), isPlaying: false)
            Task { _ = await app.media.setClockPlaybackActivity(off, owner: mediaOwner) }
        }
        lastActivity = nil
        timer?.cancel()
        timer = nil
        pendingRead?.cancel()
        pendingRead = nil
        inFlightArm = nil
        station = nil
        controlDiagnosticRoutes.reset()
        mediaDiagnosticRoutes.reset()
        sessionStartedAt = nil
        owner = nil
        mediaOwner = nil
        mirrorIdentity = nil
        lastCoreReceipt = nil
        lastCoreObservedAt = nil
        lastRadioRttAge = nil
        lastControlRttObservedAt = nil
        lastLocalObservedAt = nil
        rates.clear()
        current.removeAll()
        details.removeAll()
        controlRoute = Self.absentRoute("The control connection has ended")
        mediaRoute = Self.absentRoute("The sound and display connection has ended")
        status = .disconnected(reason: "No active Core session")
        reset = .disabled(reason: "No active diagnostics session")
        // Owner retirement is a safety boundary. Clear the visible current
        // Core immediately; only ordinary sampling is coalesced to 1 Hz.
        deferredPublication?.cancel()
        deferredPublication = nil
        lastPublishedAt = nil
        if visible { publish(force: true) }
    }

    /// Combine emits the incoming state before AudioSessionController mutates
    /// its property. Advance the revision before scheduling any actor work.
    func playbackStateWillChange(_ state: AudioSessionController.PlaybackState) {
        guard state != .playing else { return }
        activityRevision &+= 1
        guard let activity = lastActivity, let mediaOwner, let app else { return }
        lastActivity = nil
        let off = MediaClockPlaybackActivity(mediaID: activity.mediaID,
            playbackLifetime: activity.playbackLifetime, outputEpoch: activity.outputEpoch,
            revision: activityRevision, generation: activity.generation,
            observedNs: nowNanoseconds(), isPlaying: false)
        Task { _ = await app.media.setClockPlaybackActivity(off, owner: mediaOwner) }
    }

    func open(attempt: ConnectionAttempt? = nil) {
        self.attempt = attempt
        visible = true
        if let app, let station, app.session === station { ageCachedReadings(at: nowMilliseconds(), app: app) }
        publish(force: true)
    }

    func close() { visible = false }

    func setAttempt(_ attempt: ConnectionAttempt) {
        self.attempt = attempt
    }

    func selectRange(_ range: DiagnosticsRange) {
        self.range = range
        // The next one-second publication presents the new bounded range.
    }

    func resetSessionStats() {
        guard owner != nil, let latestLifetimeUnderruns else { return }
        underrunBaseline = latestLifetimeUnderruns
        maxFreshRadioRTT = nil
        details[.sessionUnderruns] = .current("0")
        details[.maximumRadioRtt] = .missing(reason: "Waiting for a new radio round trip after the reset")
        if visible { publish(force: true) }
    }

    private func tick(arm capturedArm: UInt64) {
        guard arm == capturedArm, let station,
              let owner, let mediaOwner, let app, app.session === station else { return }
        ageCachedReadings(at: nowMilliseconds(), app: app)
        guard inFlightArm == nil else { return }
        inFlightArm = capturedArm
        let mirrorID = app.mirror.snapshotIdentity
        let state = app.audio?.state
        let playbackRevision = activityRevision
        pendingRead = Task { [weak self, weak app] in
            guard let self, let app else { return }
            let control = await station.diagnosticsSnapshot()
            guard self.isCurrent(capturedArm, owner, mediaOwner, station, mirrorID, app),
                  self.activityRevision == playbackRevision, app.audio?.state == state else {
                self.finish(capturedArm); return
            }
            let media = await app.media.selectedMediaDiagnostics(owner: mediaOwner)
            guard self.isCurrent(capturedArm, owner, mediaOwner, station, mirrorID, app),
                  self.activityRevision == playbackRevision, app.audio?.state == state else {
                self.finish(capturedArm); return
            }
            let playback = await self.playback?.observation()
            guard self.isCurrent(capturedArm, owner, mediaOwner, station, mirrorID, app),
                  self.activityRevision == playbackRevision, app.audio?.state == state else {
                self.finish(capturedArm); return
            }
            let timing = app.audio?.diagnosticsOutputTiming()
            let outputEpoch = self.playback?.currentOutputEpoch
            let sameOutput = timing?.outputEpoch == outputEpoch && outputEpoch == playback?.outputEpoch
            if state == .playing, let playback, let stream = playback.stream,
               media.acceptedAudioGeneration == stream.generation,
               let outputEpoch, sameOutput, let mediaID = media.mediaID {
                self.activityRevision &+= 1
                let activity = MediaClockPlaybackActivity(mediaID: mediaID,
                    playbackLifetime: playback.lifetimeID, outputEpoch: outputEpoch,
                    revision: self.activityRevision, generation: stream.generation,
                    observedNs: self.nowNanoseconds(), isPlaying: true)
                self.lastActivity = activity
                _ = await app.media.setClockPlaybackActivity(activity, owner: mediaOwner)
                guard self.isCurrent(capturedArm, owner, mediaOwner, station, mirrorID, app),
                      self.activityRevision == activity.revision,
                      app.audio?.state == .playing,
                      self.playback?.currentOutputEpoch == outputEpoch else {
                    self.finish(capturedArm); return
                }
            } else if self.lastActivity != nil {
                self.playbackStateWillChange(.stopped)
            }
            #if DEBUG
            await self.beforePublicationForTesting?()
            guard self.isCurrent(capturedArm, owner, mediaOwner, station, mirrorID, app) else {
                self.finish(capturedArm); return
            }
            #endif
            let finalControl = await station.diagnosticsSnapshot()
            guard self.isCurrent(capturedArm, owner, mediaOwner, station, mirrorID, app),
                  Self.sameControlSource(control, finalControl) else {
                self.finish(capturedArm); return
            }
            let finalMedia: SelectedMediaDiagnosticsSnapshot
            #if DEBUG
            if let read = self.finalMediaReadForTesting { finalMedia = await read() }
            else { finalMedia = await app.media.selectedMediaDiagnostics(owner: mediaOwner) }
            #else
            finalMedia = await app.media.selectedMediaDiagnostics(owner: mediaOwner)
            #endif
            guard self.isCurrent(capturedArm, owner, mediaOwner, station, mirrorID, app),
                  Self.sameMediaSource(media, finalMedia),
                  self.playback?.currentOutputEpoch == outputEpoch,
                  self.playback?.currentStreamEpoch == playback?.streamEpoch,
                  self.activityRevision == playbackRevision ||
                    (self.lastActivity?.revision == self.activityRevision && app.audio?.state == .playing) else {
                self.finish(capturedArm); return
            }
            // A same-session control move can occur during the final media await.
            // Recheck this bounded observation before emitting either route event.
            let diagnosticControl = await station.diagnosticsSnapshot()
            guard self.isCurrent(capturedArm, owner, mediaOwner, station, mirrorID, app),
                  self.playback?.currentOutputEpoch == outputEpoch,
                  self.playback?.currentStreamEpoch == playback?.streamEpoch,
                  self.activityRevision == playbackRevision ||
                    (self.lastActivity?.revision == self.activityRevision && app.audio?.state == .playing) else {
                self.finish(capturedArm); return
            }
            if Self.sameDiagnosticCarrier(finalControl, diagnosticControl) {
                self.recordObservedRoutes(control: diagnosticControl, media: finalMedia)
            }
            self.consume(control: control, media: finalMedia, playback: playback,
                         timing: timing, app: app, now: self.nowMilliseconds())
            self.finish(capturedArm)
        }
    }

    private func isCurrent(_ capturedArm: UInt64, _ expectedOwner: UInt64,
                           _ expectedMediaOwner: UInt64, _ expectedStation: StationSession,
                           _ expectedMirror: UInt64, _ app: AppModel) -> Bool {
        arm == capturedArm && owner == expectedOwner && mediaOwner == expectedMediaOwner &&
            station === expectedStation && app.session === expectedStation &&
            app.mirror.snapshotIdentity == expectedMirror
    }

    /// Called only after the collector's final current-owner and source checks.
    /// Full identities remain here; formatted events contain only fixed fields.
    private func recordObservedRoutes(control: StationDiagnosticsSnapshot,
                                      media: SelectedMediaDiagnosticsSnapshot) {
        if let summary = ObservedRouteDiagnosticSummary(control.selectedRoute) {
            let identity = DiagnosticRouteIdentity(owner: owner, generation: control.attemptGeneration,
                routeID: control.routeID, mediaID: nil, observation: control.selectedRoute,
                carrierGeneration: nil, carrierRouteID: nil, carrierRank: nil, carrierObservation: nil)
            if controlDiagnosticRoutes.changed(identity) {
                diagnostics.observedRoute(connection: .control, kind: summary.kind, rank: summary.rank)
            }
        }
        let tunneled: Bool
        if case .available(let route) = media.route { tunneled = route.kind == .wssTunnel }
        else { tunneled = false }
        // Unknown carrier rank returns nil before deduplication. A later
        // coherent stamp can therefore emit the still-unchanged media route.
        let coherent = control.state == .ready && control.attemptGeneration != nil && control.routeID != nil
        let carrierRank = coherent ? control.serviceRank : nil
        if let summary = ObservedRouteDiagnosticSummary(media.route, carrierRank: carrierRank) {
            let identity = DiagnosticRouteIdentity(owner: media.owner, generation: media.routeGeneration,
                routeID: nil, mediaID: media.mediaID, observation: media.route,
                carrierGeneration: tunneled ? control.attemptGeneration : nil,
                carrierRouteID: tunneled ? control.routeID : nil,
                carrierRank: tunneled ? carrierRank : nil,
                carrierObservation: tunneled ? control.selectedRoute : nil)
            if mediaDiagnosticRoutes.changed(identity) {
                diagnostics.observedRoute(connection: .media, kind: summary.kind, rank: summary.rank)
            }
        }
    }

    // This bounds the final-media await window; it is not an atomic cross-actor
    // current-at-publication guarantee. Private observations never reach Event.line.
    private static func sameDiagnosticCarrier(_ a: StationDiagnosticsSnapshot,
                                               _ b: StationDiagnosticsSnapshot) -> Bool {
        sameControlSource(a, b) && a.serviceRank == b.serviceRank && a.selectedRoute == b.selectedRoute
    }

    private static func sameControlSource(_ a: StationDiagnosticsSnapshot,
                                          _ b: StationDiagnosticsSnapshot) -> Bool {
        a.state == b.state && a.attemptGeneration == b.attemptGeneration && a.routeID == b.routeID &&
            a.traffic?.lifetime == b.traffic?.lifetime
    }

    private static func sameMediaSource(_ a: SelectedMediaDiagnosticsSnapshot,
                                        _ b: SelectedMediaDiagnosticsSnapshot) -> Bool {
        a.owner == b.owner && a.routeGeneration == b.routeGeneration && a.mediaID == b.mediaID &&
            a.traffic?.lifetime == b.traffic?.lifetime &&
            a.audioAdmission?.peerLifetime == b.audioAdmission?.peerLifetime &&
            a.acceptedAudioGeneration == b.acceptedAudioGeneration
    }

    private func finish(_ capturedArm: UInt64) {
        guard inFlightArm == capturedArm else { return }
        inFlightArm = nil
        pendingRead = nil
    }

    private static func elapsed(_ since: Int64, at now: Int64) -> Int64? {
        let (age, overflow) = now.subtractingReportingOverflow(since)
        return overflow || age < 0 ? nil : age
    }

    private static func staleRoute(_ route: DiagnosticsSelectedRoute, reason: String) -> DiagnosticsSelectedRoute {
        func aged(_ reading: DiagnosticsReading<String>) -> DiagnosticsReading<String> {
            if case .current(let value) = reading { return .stale(reason: reason, historical: value) }
            return reading
        }
        return DiagnosticsSelectedRoute(numericPeer: aged(route.numericPeer),
            addressFamily: aged(route.addressFamily), transport: aged(route.transport),
            path: aged(route.path), generation: aged(route.generation),
            rendezvousRole: route.rendezvousRole, peerLabel: route.peerLabel,
            relayService: nil, showsAddress: route.showsAddress)
    }

    /// This runs on the existing timer even when an asynchronous source read
    /// is held. It never starts a second read or attributes a new receipt.
    private func ageCachedReadings(at now: Int64, app: AppModel) {
        var changed = false
        if let lastCoreObservedAt,
           lastCoreReceipt?.identity != app.mirror.snapshotIdentity ||
            (Self.elapsed(lastCoreObservedAt, at: now).map { $0 > 3_000 } ?? true) {
            let reason = "The Core's last reading is older than 3 s or from before this connection"
            for metric in Self.coreMetrics {
                if case .current = current[metric] {
                    current[metric] = .stale(reason: reason)
                    history.breakMetric(metric)
                    changed = true
                }
            }
            for id in Self.coreDetails {
                if case .current = details[id] {
                    details[id] = .stale(reason: reason)
                    changed = true
                }
            }
        }
        if let lastRadioRttAge,
           (Self.elapsed(lastRadioRttAge.receivedAt, at: now).map {
               Double($0) + lastRadioRttAge.reportedMs > 60_000
           } ?? true) {
            if case .current = current[.radioRttMs] {
                current[.radioRttMs] = .stale(reason: "The radio round trip is older than 60 s")
                history.breakMetric(.radioRttMs)
                changed = true
            }
            if case .current = details[.radioRtt] {
                details[.radioRtt] = .stale(reason: "The radio round trip is older than 60 s")
                changed = true
            }
        }
        if let lastLocalObservedAt,
           Self.elapsed(lastLocalObservedAt, at: now).map({ $0 > 3_000 }) ?? true {
            let reason = "No completed current-source collection within 3 s"
            for metric in Self.localMetrics where metric != .sessionRttMs {
                if case .current = current[metric] {
                    current[metric] = .stale(reason: reason)
                    history.breakMetric(metric)
                    changed = true
                }
            }
            for id in PerformanceDetailID.allCases
                where !Self.coreDetails.contains(id) && !Self.persistentDetails.contains(id) {
                if case .current = details[id] {
                    details[id] = .stale(reason: reason)
                    changed = true
                }
            }
            if case .current = status { status = .stale(reason: reason); changed = true }
            controlRoute = Self.staleRoute(controlRoute, reason: reason)
            mediaRoute = Self.staleRoute(mediaRoute, reason: reason)
            reset = .disabled(reason: reason)
        }
        if let lastControlRttObservedAt,
           Self.elapsed(lastControlRttObservedAt, at: now).map({ $0 > 60_000 }) ?? true {
            if case .current = current[.sessionRttMs] {
                current[.sessionRttMs] = .stale(reason: "The Core round trip on this connection is older than 60 s")
                history.breakMetric(.sessionRttMs)
                changed = true
            }
            if case .current = details[.coreRtt] {
                details[.coreRtt] = .stale(reason: "The Core round trip on this connection is older than 60 s")
                changed = true
            }
        }
        if changed && visible { publish(force: false) }
    }

    private func consume(control: StationDiagnosticsSnapshot,
                         media: SelectedMediaDiagnosticsSnapshot,
                         playback: AudioPlaybackObservation?,
                         timing: PlaybackOutputTimingObservation?,
                         app: AppModel, now: Int64) {
        if mirrorIdentity != app.mirror.snapshotIdentity {
            mirrorIdentity = app.mirror.snapshotIdentity
            segment &+= 1
            rates.clear()
            lastCoreReceipt = nil
            lastCoreObservedAt = nil
            lastRadioRttAge = nil
            lastControlRttObservedAt = nil
            lastLocalObservedAt = nil
            maxFreshRadioRTT = nil
        }
        status = control.state == .ready ? .current("Connected") : .missing(reason: "Control route is not ready")
        let introduction = app.diagnosticsRendezvousRole
        let relayService = attempt?.tries.first { $0.path == .relay && $0.outcome == .connected }?.address
        controlRoute = Self.route(control.selectedRoute, generation: control.routeID.map(String.init),
                                  rendezvousRole: introduction, relayService: relayService)
        // Media tunnelled through the Core's secure WebSocket travels on
        // the control connection, so it takes that connection's family.
        let controlFamily: NumericRouteEndpoint.Family?
        if case .available(let controlPath) = control.selectedRoute {
            controlFamily = controlPath.selectedEndpoint.family
        } else {
            controlFamily = nil
        }
        mediaRoute = Self.route(media.route, generation: media.mediaID.map { "\(media.routeGeneration) · \($0)" },
                                rendezvousRole: introduction, relayService: relayService,
                                tunnelCarrierFamily: controlFamily)
        let core = app.mirror.currentTelemetryReceipt
        let fresh = core.flatMap { receipt -> StationTelemetryReceipt? in
            let age = now - receipt.observedAtMilliseconds
            return age >= 0 && age <= 3_000 ? receipt : nil
        }
        applyCore(fresh, latest: core, now: now, app: app)
        applyLocal(control: control, media: media, playback: playback,
                   timing: timing, now: now, app: app)
        lastLocalObservedAt = now
        if visible { publish(force: false) }
    }

    private func publish(force: Bool) {
        let now = nowMilliseconds()
        if let lastPublishedAt, now >= lastPublishedAt, now - lastPublishedAt < 1_000 {
            if force && visible && deferredPublication == nil {
                let wait = 1_000 - (now - lastPublishedAt)
                deferredPublication = Task { [weak self] in
                    do { try await Task.sleep(for: .milliseconds(wait)) }
                    catch { return }
                    guard let self else { return }
                    self.deferredPublication = nil
                    if self.visible { self.publish(force: true) }
                }
            }
            return
        }
        deferredPublication?.cancel()
        deferredPublication = nil
        lastPublishedAt = now
        var series: [DiagnosticsMetric: DiagnosticsSeries] = [:]
        for metric in DiagnosticsMetric.allCases { series[metric] = history.series(metric, at: now, range: range) }
        var allDetails = Dictionary(uniqueKeysWithValues: PerformanceDetailID.allCases.map {
            ($0, DiagnosticsReading<String>.missing(reason: "This source has not supplied a reading"))
        })
        allDetails.merge(details) { _, new in new }
        let age = attempt.map { max(0, Int(Date().timeIntervalSince($0.started))) }
        let attempted: DiagnosticsReading<String> = attempt.flatMap { record in
            record.tries.isEmpty ? nil : .current("\(record.summary) \(age ?? 0) s ago")
        } ?? .missing(reason: "No recorded connection attempt")
        let failure = Self.latestFailure(in: attempt, attemptAgeSeconds: age)
        input = ConnectionPerformanceInput(sampledAtMonotonicMilliseconds: now,
            displayWallTime: Date().formatted(date: .omitted, time: .standard), status: status,
            selectedRange: range, series: series, current: current, details: allDetails,
            controlRoute: controlRoute, mediaRoute: mediaRoute,
            latestAttempt: attempted, latestFailure: failure, reset: reset)
        #if DEBUG
        publicationCountForTesting += 1
        #endif
    }

    private static func absentRoute(_ reason: String) -> DiagnosticsSelectedRoute {
        DiagnosticsSelectedRoute(numericPeer: .missing(reason: reason), addressFamily: .missing(reason: reason),
            transport: .missing(reason: reason), path: .missing(reason: reason),
            generation: .missing(reason: reason), rendezvousRole: .missing(reason: reason))
    }

    static func latestFailure(in attempt: ConnectionAttempt?,
                              attemptAgeSeconds: Int?) -> DiagnosticsReading<String> {
        guard let failed = attempt?.tries.last(where: {
            switch $0.outcome {
            case .noAnswer, .timedOut, .notThisCore, .failed, .refused, .unreachable: true
            case .trying, .connected, .cancelled: false
            }
        }) else { return .current("None") }
        let age = attemptAgeSeconds.map { "; attempt started \($0) s ago" } ?? ""
        return .current("\(ConnectionAttempt.outcomeText(failed.outcome))\(age)")
    }

    /// What each end offered on the latest attempt's path through the
    /// internet service, and how each pair of addresses went.
    static func serviceOffers(in attempt: ConnectionAttempt?) -> DiagnosticsReading<String> {
        guard let evidence = attempt?.tries.last(where: { $0.throughService && $0.ice != nil })?.ice else {
            return .missing(reason: "The latest attempt did not finish a connection through the internet service")
        }
        return .current(evidence.summary)
    }

    private static func blank(at now: Int64) -> ConnectionPerformanceInput {
        ConnectionPerformanceInput(sampledAtMonotonicMilliseconds: now, displayWallTime: nil,
            status: .disconnected(reason: "No Core selected"), selectedRange: .oneMinute,
            series: [:], current: [:], details: [:],
            controlRoute: absentRoute("No control connection is open"),
            mediaRoute: absentRoute("No sound or display connection is open"),
            latestAttempt: .missing(reason: "No recorded attempt"),
            latestFailure: .current("None"),
            reset: .disabled(reason: "No active diagnostics session"))
    }

    static func route(_ observation: SelectedRouteObservation, generation: String?,
                      rendezvousRole: String, relayService: String? = nil,
                      tunnelCarrierFamily: NumericRouteEndpoint.Family? = nil) -> DiagnosticsSelectedRoute {
        guard case .available(let selected) = observation else {
            guard case .unavailable(let unavailable, let kind, let carrier) = observation else {
                return absentRoute("The path is not available")
            }
            let reason: String
            switch unavailable {
            case .notReady: reason = "The connection is still opening"
            case .noSelectedPath: reason = "The phone has not settled on a network path"
            case .nonNumericPathEndpoint: reason = "The path is known only by name, so there is no address to show"
            case .noSelectedPair: reason = "The connection has not settled on a path"
            case .malformedSelectedPair: reason = "The phone could not read the path this connection chose"
            case .retired: reason = "This connection has closed"
            case .noCurrentMedia: reason = "No sound or display connection is open"
            case .unsupported: reason = "This connection does not report its path"
            }
            let absent: DiagnosticsReading<String> = unavailable == .unsupported
                ? .unsupported(reason: reason) : .missing(reason: reason)
            let unknown = DiagnosticsReading<String>.missing(reason: reason)
            return DiagnosticsSelectedRoute(numericPeer: absent, addressFamily: unknown,
                transport: carrier.map { .current(transportText($0)) } ?? unknown,
                path: kind == .unknown ? unknown : .current(pathText(kind)),
                generation: generation.map(DiagnosticsReading<String>.current) ?? unknown,
                rendezvousRole: .current(rendezvousRole),
                relayService: isRelayed(kind) ? relayService : nil)
        }
        let endpoint = selected.selectedEndpoint
        let literal = endpoint.scope.map { "\(endpoint.address)%\($0)" } ?? endpoint.address
        let peer = endpoint.family == .ipv6 ? "[\(literal)]:\(endpoint.port)" : "\(literal):\(endpoint.port)"
        let family: DiagnosticsReading<String>
        let transport: String
        if selected.kind == .wssTunnel {
            // The selected pair is only the hand-off on this phone; the media
            // rides the control connection's secure WebSocket.
            family = tunnelCarrierFamily.map { .current(familyText($0)) }
                ?? .missing(reason: "The control connection the sound and display run over is not open")
            transport = transportText(.tlsWebSocket)
        } else {
            family = .current(familyText(endpoint.family))
            transport = transportText(selected.transport)
        }
        let label: String
        switch selected.endpointRole {
        case .readyPathRemote, .nominatedICEPeer:
            // The far end's candidate is a relay's only when it is that
            // relay's allocation; relayed at this end alone, it is the Core's.
            label = selected.selectedEndpointIsRelay ? "Relay address" : "Core address"
        case .frameworkReportedProxiedPath: label = "Proxy address"
        case .claimedLocalShim: label = "Hand-off on this phone"
        }
        return DiagnosticsSelectedRoute(numericPeer: .current(peer), addressFamily: family,
            transport: .current(transport), path: .current(pathText(selected.kind)),
            generation: generation.map(DiagnosticsReading<String>.current)
                ?? .missing(reason: "No current connection number"),
            rendezvousRole: .current(rendezvousRole), peerLabel: label,
            relayService: isRelayed(selected.kind) ? relayService : nil,
            showsAddress: selected.endpointRole != .claimedLocalShim)
    }

    private static func isRelayed(_ kind: SelectedRouteKind) -> Bool {
        kind == .turnRelay || kind == .webRelay
    }

    private static func familyText(_ family: NumericRouteEndpoint.Family) -> String {
        family == .ipv6 ? "IPv6" : "IPv4"
    }

    private static func transportText(_ transport: SelectedRouteTransport) -> String {
        switch transport {
        case .tlsWebSocket: "secure WebSocket"
        case .iceUDP: "peer-to-peer link"
        case .iceTCP: "peer-to-peer link, ordered stream"
        }
    }

    private static func pathText(_ kind: SelectedRouteKind) -> String {
        switch kind {
        case .direct: "Direct"
        case .turnRelay: "Relayed"
        case .webRelay: "Relayed over the web"
        case .wssTunnel: "Tunnelled through the control connection"
        case .systemProxy: "Through this phone's proxy"
        case .unknown: "Path not known"
        }
    }
}

private extension ConnectionPerformanceModel {
    static let coreMetrics: Set<DiagnosticsMetric> = [
        .radioRxMbps, .radioTxMbps, .radioRttMs,
        .audioSourceFramesPerSecond, .audioEncodedPacketsPerSecond,
        .audioSendAcceptedPerSecond, .audioSendRejectedPerSecond, .audioSourceDropsPerSecond,
        .coreSystemCpuPercent, .coreProcessCpuPercent, .coreMemoryAvailableMiB,
        .coreProcessResidentMiB, .coreHottestZoneCelsius,
        .coreReceiverLoadPercentSlot0, .coreReceiverLoadPercentSlot1,
        .coreReceiverLoadPercentSlot2, .coreReceiverLoadPercentSlot3, .coreReceiverLoadPercentSlot4
    ]

    static let localMetrics = Set(DiagnosticsMetric.allCases).subtracting(coreMetrics)
    static let coreDetails: Set<PerformanceDetailID> = [
        .radioState, .coreAge, .radioUptime, .radioRtt,
        .radioThroughput, .radioSampleRate, .radioUdpPackets, .radioLoss,
        .radioJitter, .radioGap, .paVoltage, .dcVoltage, .adcOverload,
        .coreSource, .coreEncoded, .coreAccepted, .coreRefused, .coreDrops,
        .cpu, .memory, .sensor, .receivers,
        .radioIdentity, .radioAddress, .radioMac
    ]
    static let persistentDetails: Set<PerformanceDetailID> = [
        .coreRtt, .maximumRadioRtt, .latestAttempt, .latestFailure, .serviceOffers,
        .rendezvous, .resetScope, .delayExplanation
    ]

    func setCore(_ metric: DiagnosticsMetric, _ value: Double?, reason: String,
                 values: inout [DiagnosticsMetric: Double]) {
        if let value, value.isFinite {
            current[metric] = .current(value)
            values[metric] = value
        } else {
            current[metric] = .missing(reason: reason)
        }
    }

    func applyCore(_ receipt: StationTelemetryReceipt?, latest: StationTelemetryReceipt?,
                   now: Int64, app: AppModel) {
        guard let receipt else {
            let reason = latest == nil ? "Waiting for the Core's first reading" : "The Core's last reading is older than 3 s"
            for metric in Self.coreMetrics { current[metric] = .stale(reason: reason) }
            history.append(DiagnosticsSample(monotonicMilliseconds: now, sessionSegment: segment,
                                             updated: Self.coreMetrics, values: [:], stale: true))
            for id in Self.coreDetails {
                details[id] = .stale(reason: reason)
            }
            if app.mirror.agreedMinor.map({ $0 < 10 }) == true {
                let unsupported = "This Core does not report its computer's processor, memory or temperature"
                for metric in [DiagnosticsMetric.coreSystemCpuPercent, .coreProcessCpuPercent,
                               .coreMemoryAvailableMiB, .coreProcessResidentMiB,
                               .coreHottestZoneCelsius] {
                    current[metric] = .unsupported(reason: unsupported)
                }
                for id in [PerformanceDetailID.cpu, .memory, .sensor] {
                    details[id] = .unsupported(reason: unsupported)
                }
            }
            return
        }
        let age = now - receipt.observedAtMilliseconds
        lastCoreObservedAt = receipt.observedAtMilliseconds
        let metrics = receipt.metrics
        var values: [DiagnosticsMetric: Double] = [:]
        let missing = "The current Core did not report this field"
        setCore(.radioRxMbps, metrics.radio?.rxMbps, reason: missing, values: &values)
        setCore(.radioTxMbps, metrics.radio?.txMbps, reason: missing, values: &values)
        let ageOfRadioRtt = metrics.radio?.rttAgeMs.flatMap { reported -> Double? in
            guard reported.isFinite, reported >= 0, reported + Double(age) <= 60_000 else { return nil }
            return reported + Double(age)
        }
        lastRadioRttAge = ageOfRadioRtt == nil ? nil : metrics.radio?.rttAgeMs.map {
            (receipt.observedAtMilliseconds, $0)
        }
        setCore(.radioRttMs, ageOfRadioRtt == nil ? nil : metrics.radio?.rttMs,
                reason: "No radio round trip in a recent Core reading within 60 s", values: &values)
        setCore(.audioSourceFramesPerSecond, metrics.audio?.sourceFramesPerSecond,
                reason: missing, values: &values)
        setCore(.audioEncodedPacketsPerSecond, metrics.audio?.encodedPacketsPerSecond,
                reason: missing, values: &values)
        setCore(.audioSendAcceptedPerSecond, metrics.audio?.sendAcceptedPerSecond,
                reason: missing, values: &values)
        setCore(.audioSendRejectedPerSecond, metrics.audio?.sendRejectedPerSecond,
                reason: missing, values: &values)
        setCore(.audioSourceDropsPerSecond, metrics.audio?.sourceDropsPerSecond,
                reason: missing, values: &values)
        let hostReason = app.mirror.agreedMinor.map { $0 < 10 } == true
            ? "This Core does not report its computer's processor, memory or temperature" : "The Core did not send this reading"
        setCore(.coreSystemCpuPercent, metrics.host?.systemCpuPercent,
                reason: hostReason, values: &values)
        setCore(.coreProcessCpuPercent, metrics.host?.processCpuPercent,
                reason: hostReason, values: &values)
        setCore(.coreMemoryAvailableMiB, metrics.host?.memoryAvailableKiB.map { Double($0) / 1024 },
                reason: hostReason, values: &values)
        setCore(.coreProcessResidentMiB, metrics.host?.processResidentKiB.map { Double($0) / 1024 },
                reason: hostReason, values: &values)
        setCore(.coreHottestZoneCelsius, metrics.host?.hottestZoneCelsius,
                reason: hostReason, values: &values)
        for slot in 0..<Self.receiverSlots(app.mirror.capabilities) {
            guard let metric = DiagnosticsMetric.receiverLoad(slot: slot) else { continue }
            let load = metrics.receivers?.first(where: { $0.sliceId == Int64(slot) })?.loadPercent
            setCore(metric, load, reason: "No receiver processing measurement for this slice", values: &values)
        }
        if app.mirror.agreedMinor.map({ $0 < 10 }) == true {
            for metric in [DiagnosticsMetric.coreSystemCpuPercent, .coreProcessCpuPercent,
                           .coreMemoryAvailableMiB, .coreProcessResidentMiB,
                           .coreHottestZoneCelsius] where values[metric] == nil {
                current[metric] = .unsupported(reason: hostReason)
            }
        }
        if lastCoreReceipt?.identity != receipt.snapshotIdentity || lastCoreReceipt?.sequence != metrics.sequence {
            lastCoreReceipt = (receipt.snapshotIdentity, metrics.sequence)
            history.append(DiagnosticsSample(monotonicMilliseconds: now, sessionSegment: segment,
                                             updated: Self.coreMetrics, values: values))
            if let radioRtt = values[.radioRttMs] {
                maxFreshRadioRTT = max(maxFreshRadioRTT ?? radioRtt, radioRtt)
            }
        }
        details[.coreAge] = .current("\(age) ms")
        details[.radioState] = metrics.radio?.connected.map { .current($0 ? "Connected" : "Disconnected") }
            ?? .missing(reason: missing)
        details[.radioUptime] = metrics.radio?.connectionAgeMs.map { .current("\($0) ms") }
            ?? .missing(reason: "This Core does not report how long the radio has been connected")
        details[.radioRtt] = ageOfRadioRtt.flatMap { elapsed in
            metrics.radio?.rttMs.map { .current(String(format: "%.1f ms, age %.0f ms", $0, elapsed)) }
        } ?? .missing(reason: "No radio round trip within 60 s")
        details[.maximumRadioRtt] = maxFreshRadioRTT.map { .current(String(format: "%.1f ms", $0)) }
            ?? .missing(reason: "Waiting for a radio round trip")
        details[.radioThroughput] = pair(metrics.radio?.rxMbps, metrics.radio?.txMbps, unit: "Mbps")
        details[.radioSampleRate] = metrics.radio?.sampleRateHz.map {
            .current(String(format: "%.1f kHz", Double($0) / 1_000))
        } ?? .missing(reason: missing)
        details[.radioUdpPackets] = metrics.radio?.udpPacketsSeen.map { .current("\($0) since radio connect") }
            ?? .missing(reason: missing)
        details[.radioLoss] = number(metrics.radio?.packetLossPercent, "%, last 5 s")
        details[.radioJitter] = number(metrics.radio?.jitterMs, " ms")
        details[.radioGap] = number(metrics.radio?.packetGapMs, " ms, last 1 s")
        details[.paVoltage] = number(metrics.radio?.paVolts, " V")
        details[.dcVoltage] = number(metrics.radio?.supplyVolts, " V")
        details[.adcOverload] = adc(metrics.radio?.adcOverloads)
        details[.coreSource] = number(metrics.audio?.sourceFramesPerSecond, " frames/s")
        details[.coreEncoded] = number(metrics.audio?.encodedPacketsPerSecond, " packets/s")
        details[.coreAccepted] = number(metrics.audio?.sendAcceptedPerSecond, " packets/s, local Core acceptance")
        details[.coreRefused] = number(metrics.audio?.sendRejectedPerSecond, " packets/s")
        details[.coreDrops] = number(metrics.audio?.sourceDropsPerSecond, " events/s")
        details[.cpu] = pair(metrics.host?.systemCpuPercent, metrics.host?.processCpuPercent, unit: "%")
        details[.memory] = pair(metrics.host?.memoryAvailableKiB.map { Double($0) / 1024 },
                                metrics.host?.processResidentKiB.map { Double($0) / 1024 }, unit: "MiB")
        details[.sensor] = metrics.host?.hottestZoneCelsius.flatMap { value in
            metrics.host?.hottestZoneName.map { .current(String(format: "%.1f °C · %@", value, $0)) }
        } ?? .missing(reason: "No current Core host sensor")
        if app.mirror.agreedMinor.map({ $0 < 10 }) == true {
            for id in [PerformanceDetailID.cpu, .memory, .sensor] {
                details[id] = .unsupported(reason: hostReason)
            }
        }
        details[.receivers] = .current((0..<Self.receiverSlots(app.mirror.capabilities)).map { slot in
            let name = String(UnicodeScalar(65 + slot)!)
            let value = metrics.receivers?.first(where: { $0.sliceId == Int64(slot) })?.loadPercent
            return "\(name): \(value.map { String(format: "%.1f%%", $0) } ?? "unavailable")"
        }.joined(separator: ", "))
        let caps = app.mirror.capabilities
        func text(_ key: String) -> String? {
            if case .text(let value)? = caps[key], !value.isEmpty { return value }
            return nil
        }
        let model = text("radioModel") ?? "model unavailable"
        let firmware = text("firmwareVersion") ?? "firmware unavailable"
        let protocolVersion: String
        if case .int(let number)? = caps["radioProtocol"], number == 1 || number == 2 {
            protocolVersion = "Protocol \(number)"
        } else { protocolVersion = "protocol unavailable" }
        details[.radioIdentity] = .current("\(text("stationName") ?? "name unavailable") · \(model) · \(protocolVersion) · \(firmware)")
        details[.radioMac] = text("macAddress").map(DiagnosticsReading<String>.current)
            ?? .missing(reason: "Core did not report a radio MAC")
        if let address = text("radioAddress"), let port = metrics.radio?.radioUdpBasePort {
            let literal = address.contains(":") ? "[\(address)]" : address
            details[.radioAddress] = .current("\(literal):\(port) (the radio's address on its network, as the Core sees it)")
        } else {
            details[.radioAddress] = .missing(reason: "The Core has not sent the radio's network address")
        }
    }

    func number(_ value: Double?, _ suffix: String) -> DiagnosticsReading<String> {
        guard let value, value.isFinite else { return .missing(reason: "Current source did not report this reading") }
        return .current(String(format: "%.1f%@", value, suffix))
    }

    func pair(_ first: Double?, _ second: Double?, unit: String) -> DiagnosticsReading<String> {
        guard let first, let second, first.isFinite, second.isFinite else {
            return .missing(reason: "One or both current components are unavailable")
        }
        return .current(String(format: "%.1f / %.1f %@", first, second, unit))
    }

    func adc(_ overloads: [StationMetrics.ADCOverload]?) -> DiagnosticsReading<String> {
        guard let overloads else { return .missing(reason: "This Core does not report the radio's receiver overloads") }
        if overloads.isEmpty { return .current("Observed empty ADC list") }
        return .current(overloads.map { item in
            let state = item.overloaded.map { $0 ? "overloaded" : "clear" }
                ?? "status unavailable (not reported or expired)"
            let statusAge = item.statusAgeMs.map { "\($0) ms at Core sample" }
                ?? "unavailable (status age not reported)"
            let lastAge = item.lastOverloadAgeMs.map { "\($0) ms at Core sample" }
                ?? "unavailable (last overload age not reported)"
            return "ADC \(item.adc): \(state), \(item.eventsSinceConnection) events, " +
                "status age \(statusAge), last overload age \(lastAge)"
        }.joined(separator: "; "))
    }
}

private extension ConnectionPerformanceModel {
    func setLocal(_ metric: DiagnosticsMetric, _ value: Double?, reason: String,
                  values: inout [DiagnosticsMetric: Double]) {
        if let value, value.isFinite {
            current[metric] = .current(value)
            values[metric] = value
        } else {
            current[metric] = .missing(reason: reason)
        }
    }

    func applyLocal(control: StationDiagnosticsSnapshot,
                    media: SelectedMediaDiagnosticsSnapshot,
                    playback: AudioPlaybackObservation?,
                    timing: PlaybackOutputTimingObservation?,
                    now: Int64, app: AppModel) {
        var values: [DiagnosticsMetric: Double] = [:]
        let controlLife = control.traffic?.lifetime.uuidString
        let mediaLife = media.traffic?.lifetime.uuidString
        let streamLife = playback.flatMap { observation -> String? in
            guard let stream = observation.stream, media.mediaID != nil,
                  media.acceptedAudioGeneration == stream.generation else { return nil }
            return "\(observation.lifetimeID):\(observation.streamEpoch):\(stream.generation)"
        }
        func controlRate(_ key: String, _ count: UInt64?) -> Double? {
            rates.rate(key, lifetime: controlLife, count: count, at: now, multiplier: 8)
        }
        func mediaRate(_ key: String, _ count: UInt64?) -> Double? {
            rates.rate(key, lifetime: mediaLife, count: count, at: now, multiplier: 8)
        }
        let controlRx = controlRate("control-rx", control.traffic?.receivedPayloadBytes)
        let controlTx = controlRate("control-tx", control.traffic?.acceptedPayloadBytes)
        let displayRx = mediaRate("display-rx", media.traffic?.receivedDisplayPayloadBytes)
        let rtpRx = mediaRate("rtp-rx", media.traffic?.receivedRtpBytes)
        let displayTx = mediaRate("display-tx", media.traffic?.submittedDisplayPayloadBytes)
        let rtpTx = mediaRate("rtp-tx", media.traffic?.submittedRtpBytes)
        let txChannel = mediaRate("tx-channel", media.traffic?.submittedTxChannelBytes)
        let content = rates.rate("audio-content", lifetime: streamLife,
                                 count: streamLife == nil ? nil : playback?.counters.receivedContentBytes,
                                 at: now, multiplier: 8)
        let missingTraffic = "Waiting for two readings from the same connection"
        setLocal(.sessionPayloadRxKbps, controlRx, reason: missingTraffic, values: &values)
        setLocal(.sessionPayloadTxKbps, controlTx, reason: missingTraffic, values: &values)
        setLocal(.audioRtpRxKbps, rtpRx, reason: missingTraffic, values: &values)
        setLocal(.audioPayloadRxKbps, content, reason: missingTraffic, values: &values)
        let totalRx = sumRequired(controlRx, displayRx, rtpRx)
        let totalTx = sumRequired(controlTx, displayTx, rtpTx, txChannel)
        setLocal(.coreGuiRxKbps, totalRx,
                 reason: "Part of the connection this needs (control, display or received sound) is not running",
                 values: &values)
        setLocal(.coreGuiTxKbps, totalTx,
                 reason: "Part of the connection this needs (control, display, microphone sound or transmit sound) is not running",
                 values: &values)
        setLocal(.coreGuiTotalKbps, sumRequired(totalRx, totalTx),
                 reason: "Receive or outgoing total is incomplete", values: &values)
        if let receipt = control.roundTrip {
            lastControlRttObservedAt = receipt.observedAtMilliseconds
            let age = now - receipt.observedAtMilliseconds
            let parts = receipt.duration.components
            let ms = Double(parts.seconds) * 1_000 + Double(parts.attoseconds) / 1e15
            let valid = age >= 0 && age <= 60_000 && ms.isFinite && ms >= 0
            setLocal(.sessionRttMs, valid ? ms : nil,
                     reason: "No Core round trip on this connection within 60 s", values: &values)
            details[.coreRtt] = valid ? .current(String(format: "%.1f ms, age %lld ms", ms, age))
                : .stale(reason: "The Core round trip on this connection is older than 60 s")
        } else {
            lastControlRttObservedAt = nil
            setLocal(.sessionRttMs, nil, reason: "Waiting for a current-route Core pong", values: &values)
            details[.coreRtt] = .missing(reason: "No current-route Core pong")
        }
        if let playback, let stream = playback.stream,
           media.mediaID != nil, media.acceptedAudioGeneration == stream.generation {
            let buffer = playback.counters
            func packetRate(_ key: String, _ count: UInt64?) -> Double? {
                rates.rate(key, lifetime: streamLife, count: count, at: now, multiplier: 1_000)
            }
            setLocal(.playbackDecodedPacketsPerSecond, packetRate("decoded", buffer.decodedPackets),
                     reason: "Waiting for two current-stream packet counts", values: &values)
            setLocal(.playbackConcealedPacketsPerSecond, packetRate("concealed", buffer.concealedIntervals),
                     reason: "Waiting for two current-stream concealment counts", values: &values)
            setLocal(.playbackLatePacketsPerSecond, packetRate("late", buffer.latePackets),
                     reason: "Waiting for two current-stream late counts", values: &values)
            setLocal(.playbackPacketAgeMs, playback.lastAdmittedPacketAgeMs,
                     reason: "No accepted packet in the current playing stream", values: &values)
            details[.streamGeneration] = .current("\(stream.generation), stream epoch \(playback.streamEpoch)")
            details[.acceptedPackets] = .current("\(buffer.acceptedPackets)")
            details[.startupDiscarded] = .current("\(buffer.startDiscardedPackets)")
            details[.decoded] = .current("\(buffer.decodedPackets)")
            details[.concealed] = .current("\(buffer.concealedIntervals) 40 ms intervals")
            details[.late] = .current("\(buffer.latePackets)")
            details[.invalid] = .current("\(buffer.invalidPackets)")
            details[.duplicate] = .current("\(buffer.duplicatePackets)")
            details[.headerRejected] = media.audioAdmission.map {
                .current("\($0.rejectedHeaders) at current peer")
            } ?? .missing(reason: "Current peer admission counters unavailable")
            details[.underflows] = .current("\(buffer.underflowEvents) buffer events, \(playback.streamRenderUnderruns.map(String.init) ?? "unknown") render events")
            details[.overflows] = .current("\(buffer.overflowEvents) buffer events")
            details[.lastPacketAge] = playback.lastAdmittedPacketAgeMs.map {
                .current(String(format: "%.1f ms", $0))
            } ?? .missing(reason: "No accepted current-stream packet")
            details[.reorder] = playback.reorderQueuedMs.map { .current("\($0) ms") }
                ?? .missing(reason: "Current reorder depth unavailable")
            details[.adaptiveHold] = playback.adaptiveTargetMs.map { .current("\($0) ms") }
                ?? .missing(reason: "Current adaptive hold unavailable")
            details[.arrivalJitter] = buffer.reception.arrivalJitterMs.map {
                .current(String(format: "%.1f ms", $0))
            } ?? .missing(reason: "Not enough current admitted packets for arrival jitter")
            details[.missingPackets] = .current("\(buffer.reception.missingPackets) sequence positions")
            details[.expectedPackets] = .current("\(buffer.reception.expectedPackets) sequence positions")
            details[.concealedIntervals] = .current("\(buffer.concealedIntervals) 40 ms intervals")
            details[.linkInterruptions] = .current("\(buffer.linkInterruptionEvents)")
            details[.burstDropped] = media.audioAdmission.map {
                .current("\($0.droppedByReceiveQueue * 40) ms at bounded receive queue")
            } ?? .missing(reason: "Current peer receive-queue drops unavailable")
            details[.skipped] = .current("\(buffer.skippedIntervals * 40) ms after playback began")
            details[.clockDrift] = playback.measuredDriftPpm.map {
                .current(String(format: "%.1f ppm", $0))
            } ?? .missing(reason: "No measured current-stream drift")
        } else {
            for metric in [DiagnosticsMetric.playbackDecodedPacketsPerSecond,
                           .playbackConcealedPacketsPerSecond, .playbackLatePacketsPerSecond,
                           .playbackPacketAgeMs] {
                setLocal(metric, nil, reason: "Playback stream is not active", values: &values)
            }
            for id in [PerformanceDetailID.streamGeneration, .acceptedPackets, .startupDiscarded,
                       .decoded, .concealed, .late, .invalid, .duplicate, .headerRejected,
                       .underflows, .overflows, .lastPacketAge, .reorder, .adaptiveHold,
                       .arrivalJitter, .missingPackets, .expectedPackets, .concealedIntervals,
                       .linkInterruptions, .burstDropped, .skipped, .clockDrift] {
                details[id] = .missing(reason: "Playback stream is not active")
            }
        }
        let playbackLife = playback.map { String($0.lifetimeID) }
        let underflowRate = rates.rate("lifetime-underflows", lifetime: playbackLife,
                                       count: playback?.lifetimeUnderflows, at: now, multiplier: 1_000)
        let overflowRate = rates.rate("lifetime-overflows", lifetime: playbackLife,
                                      count: playback?.lifetimeOverflows, at: now, multiplier: 1_000)
        setLocal(.playbackUnderflowsPerSecond, underflowRate,
                 reason: "Waiting for two app-lifetime underflow counts", values: &values)
        setLocal(.playbackOverflowsPerSecond, overflowRate,
                 reason: "Waiting for two app-lifetime overflow counts", values: &values)
        if let playback {
            latestLifetimeUnderruns = playback.lifetimeUnderflows
            if underrunBaseline == nil { underrunBaseline = playback.lifetimeUnderflows }
            if let baseline = underrunBaseline, playback.lifetimeUnderflows >= baseline {
                details[.sessionUnderruns] = .current("\(playback.lifetimeUnderflows - baseline)")
                reset = .enabled
            } else {
                details[.sessionUnderruns] = .missing(reason: "Lifetime counter reset")
                reset = .disabled(reason: "Waiting for a stable local interruption baseline")
            }
            details[.lifetimeInterruptions] = .current("\(playback.lifetimeUnderflows) underflow + \(playback.lifetimeOverflows) overflow stage events")
        } else {
            details[.lifetimeInterruptions] = .missing(reason: "No phone playback observation")
            details[.sessionUnderruns] = .missing(reason: "No phone playback observation")
            reset = .disabled(reason: "No phone playback observation")
        }
        details[.resetScope] = .current("Clears only this phone's count of sound interruptions and the highest radio round trip; the history and the Core's counts stay")
        let delayReason = "The sound waiting in the phone's speaker queue is not measured"
        setLocal(.speakerBufferMs, nil, reason: delayReason, values: &values)
        setLocal(.audioDelayMs, nil, reason: delayReason, values: &values)
        setLocal(.audioDelayAccuracyMs, nil, reason: delayReason, values: &values)
        details[.speakerQueue] = .missing(reason: delayReason)
        details[.audioBuffer] = .missing(reason: delayReason)
        details[.audioDelay] = .missing(reason: delayReason)
        details[.accuracy] = .missing(reason: delayReason)
        details[.delayExplanation] = .current("The delay from the Core's capture to the speaker needs the phone's speaker queue, which is not measured. The round trip to the Core is not the sound's one-way delay.")
        details[.consumedFrames] = .missing(reason: "The phone does not report how much sound its speaker has played")
        details[.playbackState] = .current(String(describing: app.audio?.state ?? .stopped))
        let outputRoute: String?
        switch app.audio?.route {
        case .speaker?: outputRoute = "Speaker"
        case .earpiece?: outputRoute = "Earpiece"
        case .external(let name)?: outputRoute = name
        case nil: outputRoute = nil
        }
        if let epoch = timing?.outputEpoch, let outputRoute {
            details[.audioBackend] = .current("Phone sound output to \(outputRoute) (run \(epoch))")
        } else {
            details[.audioBackend] = .missing(reason: timing?.outputEpoch == nil
                ? "The phone's sound output is not running" : "The phone has not said where its sound goes")
        }
        let clock = media.clock
        let currentClock = clock.active && clock.mediaID == media.mediaID &&
            clock.playbackLifetime == playback?.lifetimeID &&
            clock.outputEpoch == playback?.outputEpoch &&
            clock.playingGeneration == playback?.stream?.generation && clock.offset != nil
        let delivery = currentClock ? measureAudioDelivery(offset: clock.offset, capture: clock.capture,
            playingGeneration: playback?.stream?.generation ?? 0, release: playback?.release) : nil
        setLocal(.audioDeliveryDelayMs, delivery?.delayMs,
                 reason: "No matching reading of the Core's clock and this phone's playback times",
                 values: &values)
        details[.delivery] = delivery.map { .current(String(format: "%.1f ± %.1f ms", $0.delayMs, $0.boundMs)) }
            ?? .missing(reason: "No matching reading of the Core's clock and this phone's playback times")
        details[.controlState] = control.state == .ready ? .current("Ready")
            : .missing(reason: "The connection to the Core is not ready")
        details[.phoneUptime] = sessionStartedAt.map { .current("\(max(0, now - $0)) ms") }
            ?? .missing(reason: "Not connected to a Core")
        details[.controlPeer] = controlRoute.numericPeer
        details[.mediaPeer] = mediaRoute.numericPeer
        details[.routeGeneration] = .current("Control \(control.routeID.map(String.init) ?? "none"); media \(media.mediaID.map { _ in String(media.routeGeneration) } ?? "none")")
        details[.rendezvous] = .current(app.diagnosticsRendezvousRole)
        details[.controlTraffic] = pair(controlRx, controlTx, unit: "kbit/s")
        details[.totalTraffic] = totalRx.flatMap { rx in
            totalTx.flatMap { tx in sumRequired(rx, tx).map {
                .current(String(format: "%.1f / %.1f / %.1f kbps", rx, tx, $0))
            } }
        } ?? .missing(reason: "Part of what this phone exchanges with the Core is not known")
        details[.audioTraffic] = pair(rtpRx, content, unit: "kbps")
        let age = attempt.map { max(0, Int(Date().timeIntervalSince($0.started))) }
        details[.latestAttempt] = attempt.flatMap { $0.tries.isEmpty ? nil : .current("\($0.summary) \(age ?? 0) s ago") }
            ?? .missing(reason: "No recorded attempt")
        details[.latestFailure] = Self.latestFailure(in: attempt, attemptAgeSeconds: age)
        details[.serviceOffers] = Self.serviceOffers(in: attempt)
        history.append(DiagnosticsSample(monotonicMilliseconds: now, sessionSegment: segment,
                                         updated: Self.localMetrics, values: values,
                                         connected: control.state == .ready))
    }

    func sumRequired(_ values: Double?...) -> Double? {
        guard values.allSatisfy({ $0?.isFinite == true }) else { return nil }
        let result = values.reduce(0.0) { $0 + ($1 ?? 0) }
        return result.isFinite ? result : nil
    }
}

extension ConnectionPerformanceModel {
    /// The receivers the Core runs for this radio: its reported count
    /// (`userDdcCount`, which follows the radio's own Max RX), else the
    /// slices it offers, else five. Never more than the history keeps.
    static func receiverSlots(_ capabilities: [String: MirrorValue]) -> Int {
        func count(_ key: String) -> Int? {
            if case .int(let value)? = capabilities[key], value > 0 { return Int(value) }
            return nil
        }
        let most = (0..<64).first { DiagnosticsMetric.receiverLoad(slot: $0) == nil } ?? 5
        return min(count("userDdcCount") ?? count("effectiveMaxSlices") ?? most, most)
    }
}
