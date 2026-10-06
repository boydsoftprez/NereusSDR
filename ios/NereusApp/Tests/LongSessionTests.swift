// NereusSDR for iOS: long sessions: the data counters, the sleep timer, sound only and the notes on the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

/// R-IOS-22, R-IOS-23, R-IOS-10 (the phone's half), D27, D28: the counters
/// count a session and the month, the warning comes once past 5 GB, the
/// sleep timer disconnects, the band stops while away with Sound only and
/// comes back marked, and the notes and chip say what cellular costs.
@Suite("Long sessions", .serialized)
@MainActor
struct LongSessionTests {
    /// A clock the test moves.
    final class Clock {
        var now: Date

        init(_ now: Date) {
            self.now = now
        }
    }

    /// The connecting flow as the sleep timer sees it.
    @MainActor
    final class SleepFlow {
        var keyed = true
        var disconnects = 0
    }

    @MainActor
    final class NoteWaitGate {
        private var waiting: [CheckedContinuation<Void, Never>] = []
        var count: Int { waiting.count }

        func pause() async {
            await withCheckedContinuation { waiting.append($0) }
        }

        func release(_ index: Int) {
            waiting[index].resume()
        }
    }

    /// Traffic the test adds.
    final class Traffic {
        var totals = TrafficCounter.Totals()

        func add(in bytesIn: UInt64, out bytesOut: UInt64) {
            totals.bytesIn += bytesIn
            totals.bytesOut += bytesOut
        }
    }

    private static func settings() throws -> (PhoneSettings, String) {
        let suite = "LongSessionTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        return (PhoneSettings(defaults: defaults), suite)
    }

    /// 2026-09-27 19:42 UTC.
    static let start = Date(timeIntervalSince1970: 1_790_538_120)
    static let utc: Calendar = {
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = TimeZone(identifier: "UTC") ?? .current
        return calendar
    }()

    // MARK: The counters

    @Test("the session counts bytes in and out on any network, the month on cellular and on Wi-Fi apart")
    func counters() throws {
        let (settings, suite) = try Self.settings()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let clock = Clock(Self.start)
        let traffic = Traffic()
        traffic.add(in: 999, out: 999)
        let meter = DataUseMeter(settings: settings, reading: { traffic.totals }, now: { clock.now },
                                 calendar: Self.utc)
        meter.sessionStarted()
        traffic.add(in: 10_000_000, out: 1_000_000)
        meter.sample(cellular: false)
        traffic.add(in: 5_000_000, out: 500_000)
        meter.sample(cellular: true)
        #expect(meter.session == DataUseMeter.Count(bytesIn: 15_000_000, bytesOut: 1_500_000))
        #expect(meter.monthWifi == DataUseMeter.Count(bytesIn: 10_000_000, bytesOut: 1_000_000))
        #expect(meter.monthCellular == DataUseMeter.Count(bytesIn: 5_000_000, bytesOut: 500_000))
        #expect(meter.sessionOnCellular)
        #expect(DataUsePage.sessionText(meter) == "16.5 MB total")

        // The month is kept on this phone; a new session starts at nothing.
        let again = DataUseMeter(settings: settings, reading: { traffic.totals }, now: { clock.now },
                                 calendar: Self.utc)
        #expect(again.monthCellular.total == 5_500_000)
        again.sessionStarted()
        #expect(again.session.total == 0)

        // After the session, nothing more counts.
        again.sessionEnded(cellular: true)
        traffic.add(in: 7_000_000, out: 0)
        again.sample(cellular: true)
        #expect(again.monthCellular.total == 5_500_000)

        // The first of the next month starts the month again.
        clock.now = Self.start.addingTimeInterval(5 * 86_400)
        again.sessionStarted()
        traffic.add(in: 1_000_000, out: 0)
        again.sample(cellular: true)
        #expect(again.monthCellular.total == 1_000_000)
        #expect(again.monthWifi.total == 0)
    }

    @Test("incoming and outgoing app payload rates scale independently and expire without fresh samples")
    func trafficRates() throws {
        let (settings, suite) = try Self.settings()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let clock = Clock(Self.start)
        let traffic = Traffic()
        let meter = DataUseMeter(settings: settings, reading: { traffic.totals }, now: { clock.now })
        meter.sessionStarted()
        #expect(meter.currentRate == nil)
        traffic.add(in: 750_000, out: 15_000)
        clock.now.addTimeInterval(5)
        meter.sample(cellular: true)
        let rates = try #require(meter.currentRate)
        #expect(rates.incoming == 1_200_000)
        #expect(rates.outgoing == 24_000)
        #expect(DataUseMeter.Rate.text(rates.incoming) == "1.2 Mbps")
        #expect(DataUseMeter.Rate.text(rates.outgoing) == "24 kbps")
        #expect(DataUseMeter.Rate.text(125) == "125 bps")
        #expect(DataUseMeter.Rate.text(.nan) == "Unavailable")
        clock.now.addTimeInterval(16)
        #expect(meter.currentRate == nil)
        meter.invalidateRate()
        #expect(meter.currentRate == nil)
        clock.now.addTimeInterval(5)
        meter.sample(cellular: false)
        #expect(meter.currentRate == DataUseMeter.Rate(incoming: 0, outgoing: 0))
        meter.sessionEnded(cellular: false)
        #expect(meter.currentRate == nil)
    }

    @Test("past 5 GB a month on cellular the warning is due once, and never with its switch off")
    func warning() throws {
        let (settings, suite) = try Self.settings()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let clock = Clock(Self.start)
        let traffic = Traffic()
        let meter = DataUseMeter(settings: settings, reading: { traffic.totals }, now: { clock.now },
                                 calendar: Self.utc)
        meter.sessionStarted()
        #expect(settings.cellularWarning)
        traffic.add(in: 4_900_000_000, out: 0)
        #expect(!meter.sample(cellular: true))
        // Wi-Fi never counts toward it.
        traffic.add(in: 900_000_000, out: 0)
        #expect(!meter.sample(cellular: false))
        traffic.add(in: 100_000_000, out: 1)
        #expect(meter.sample(cellular: true))
        #expect(settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
        traffic.add(in: 1_000_000, out: 0)
        #expect(meter.sample(cellular: true))
        meter.markWarningShown()
        #expect(!settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
        traffic.add(in: 1_000_000, out: 0)
        #expect(!meter.sample(cellular: true))

        let (quiet, quietSuite) = try Self.settings()
        defer { UserDefaults.standard.removePersistentDomain(forName: quietSuite) }
        quiet.cellularWarning = false
        let off = DataUseMeter(settings: quiet, reading: { traffic.totals }, now: { clock.now }, calendar: Self.utc)
        off.sessionStarted()
        traffic.add(in: 6_000_000_000, out: 0)
        #expect(!off.sample(cellular: true))
        #expect(DataUseMeter.text(off.monthCellular.total) == "6.00 GB")
    }

    // MARK: The sleep timer

    @Test("the sleep timer disconnects at its time, waits while keyed, and a dropped link keeps its time")
    func sleepTimer() async {
        let clock = Clock(Self.start)
        let timer = SleepTimer(now: { clock.now })
        let flow = SleepFlow()
        timer.onTime = {
            guard !flow.keyed else {
                return false
            }
            flow.disconnects += 1
            return true
        }
        timer.connected(.thirtyMinutes)
        #expect(timer.endsAt == Self.start.addingTimeInterval(1800))
        clock.now = Self.start.addingTimeInterval(600)
        // A link that comes back keeps the time it had.
        timer.connected(.thirtyMinutes)
        #expect(timer.endsAt == Self.start.addingTimeInterval(1800))
        await timer.tick()
        #expect(flow.disconnects == 0)
        clock.now = Self.start.addingTimeInterval(1800)
        await timer.tick()
        #expect(flow.disconnects == 0)
        #expect(timer.endsAt != nil)
        flow.keyed = false
        await timer.tick()
        #expect(flow.disconnects == 1)
        #expect(timer.endsAt == nil)

        // A new choice while connected starts again from now; Off stops it.
        timer.chosen(.oneHour)
        #expect(timer.endsAt == clock.now.addingTimeInterval(3600))
        timer.chosen(.off)
        #expect(timer.endsAt == nil)
        timer.chosen(.twoHours)
        timer.disconnected()
        #expect(timer.endsAt == nil)
        #expect(BatteryAndSessionsPage.sleepFooter(Self.start, timeZone: TimeZone(identifier: "UTC") ?? .current)
            == "Disconnects at 19:42.")
    }

    @Test("an old sleep completion cannot clear a newer choice or session")
    func oldSleepCompletion() async {
        let clock = Clock(Self.start)
        let timer = SleepTimer(now: { clock.now })
        let entered = AsyncStream.makeStream(of: Void.self)
        let release = AsyncStream.makeStream(of: Void.self)
        timer.onTime = {
            entered.continuation.yield(())
            for await _ in release.stream { break }
            return true
        }
        timer.connected(.thirtyMinutes)
        clock.now = Self.start.addingTimeInterval(1800)
        let oldTick = Task { await timer.tick() }
        for await _ in entered.stream { break }
        entered.continuation.finish()
        timer.disconnected()
        timer.connected(.oneHour)
        let newDeadline = timer.endsAt
        release.continuation.yield(())
        release.continuation.finish()
        await oldTick.value
        #expect(timer.endsAt == newDeadline)
    }

    @Test("changing the sleep choice revokes a suspended expiry before its callback resumes")
    func changingSleepChoiceRevokesExpiry() async {
        let clock = Clock(Self.start)
        let timer = SleepTimer(now: { clock.now })
        let entered = AsyncStream.makeStream(of: SleepTimer.Expiry.self)
        let release = AsyncStream.makeStream(of: Void.self)
        timer.onExpiry = { expiry in
            entered.continuation.yield(expiry)
            for await _ in release.stream { break }
            return true
        }
        timer.connected(.thirtyMinutes)
        clock.now.addTimeInterval(1800)
        let tick = Task { await timer.tick() }
        var old: SleepTimer.Expiry?
        for await expiry in entered.stream { old = expiry; break }
        let captured = old
        #expect(captured != nil)
        timer.chosen(.oneHour)
        let freshDeadline = timer.endsAt
        #expect(captured?.permit.isRevoked == true)
        if let captured { #expect(!timer.isCurrent(captured)) }
        release.continuation.yield(())
        release.continuation.finish()
        entered.continuation.finish()
        await tick.value
        #expect(timer.endsAt == freshDeadline)
    }

    @Test("the screen stays on as chosen while the band shows, and always while keyed (D27)")
    func screen() {
        #expect(KeepScreenOn.standard == .always)
        #expect(KeepScreenOn.always.screenStaysOn(bandShowing: true, charging: false, keyed: false))
        #expect(!KeepScreenOn.always.screenStaysOn(bandShowing: false, charging: true, keyed: false))
        #expect(KeepScreenOn.whileCharging.screenStaysOn(bandShowing: true, charging: true, keyed: false))
        #expect(!KeepScreenOn.whileCharging.screenStaysOn(bandShowing: true, charging: false, keyed: false))
        #expect(!KeepScreenOn.never.screenStaysOn(bandShowing: true, charging: true, keyed: false))
        #expect(KeepScreenOn.never.screenStaysOn(bandShowing: false, charging: false, keyed: true))
    }

    // MARK: The session

    struct Rig {
        let app: AppModel
        let settings: PhoneSettings
        let network: ConnectionFlowTests.FakeNetwork
        let power: ThermalAndPowerWatcher
        let powerCenter: NotificationCenter
        let traffic: Traffic
        let clock: Clock
        let suite: String
        @MainActor var session: SessionController { app.longSession }
    }

    final class PowerReading {
        var reading = ThermalAndPowerWatcher.Reading.steady
    }

    private static func rig(reading: PowerReading = PowerReading(),
                            mediaPeerFactory: MediaControlClient.PeerFactory? = nil) throws -> Rig {
        let (settings, suite) = try Self.settings()
        let defaults = try #require(UserDefaults(suiteName: suite))
        let network = ConnectionFlowTests.FakeNetwork()
        let center = NotificationCenter()
        let power = ThermalAndPowerWatcher(read: { reading.reading }, center: center)
        let clock = Clock(Self.start)
        let traffic = Traffic()
        let app = AppModel(phoneSettings: settings, mediaPeerFactory: mediaPeerFactory,
                           displaySettings: BandDisplaySettingsStore(defaults: defaults),
                           sessionSources: SessionController.Sources(
                               power: power, network: network, traffic: { traffic.totals },
                               now: { clock.now }, tick: .seconds(3600)))
        return Rig(app: app, settings: settings, network: network, power: power, powerCenter: center,
                   traffic: traffic, clock: clock, suite: suite)
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    @Test("the band asks for the network's mode, Saver in Low Power Mode, and half the frames when hot")
    func requests() async throws {
        let reading = PowerReading()
        let rig = try Self.rig(reading: reading)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let subscriber = rig.app.main.subscriber
        #expect(subscriber.session == SessionPolicy.Mode.full.request)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        #expect(subscriber.session == SessionPolicy.Mode.balanced.request)
        rig.settings.cellularMode = .full
        #expect(await settle { subscriber.session == SessionPolicy.Mode.full.request })
        #expect(rig.settings.cellularMode == .full)
        rig.network.set(NetworkPath(online: true, interfaces: ["en0"], wifi: true))
        #expect(await settle { subscriber.session == SessionPolicy.Mode.full.request })
        rig.settings.wifiMode = .balanced
        #expect(await settle { subscriber.session == SessionPolicy.Mode.balanced.request })
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        #expect(await settle { subscriber.session == SessionPolicy.Mode.full.request })
        #expect(rig.settings.cellularMode == .full && rig.settings.wifiMode == .balanced)
        rig.settings.cellularMode = .balanced
        #expect(await settle { subscriber.session == SessionPolicy.Mode.balanced.request })
        #expect(DataModeText.chip(rig.session.mode, fps: rig.session.request.fps) == "Balanced \u{00B7} 15 fps")
        rig.settings.cellularMode = .audioOnly
        #expect(await settle { !subscriber.session.subscribes })
        #expect(DataModeText.chip(rig.session.mode, fps: rig.session.request.fps) == "Audio only")
        rig.settings.cellularMode = .balanced
        rig.settings.wifiMode = .full
        rig.network.set(NetworkPath(online: true, interfaces: ["en0"], wifi: true))
        #expect(await settle { subscriber.session == SessionPolicy.Mode.full.request })

        reading.reading.lowPowerMode = true
        rig.powerCenter.post(name: .NSProcessInfoPowerStateDidChange, object: nil)
        #expect(await settle { subscriber.session == SessionPolicy.Mode.saver.request })
        reading.reading.lowPowerMode = false
        reading.reading.heat = .serious
        rig.powerCenter.post(name: ProcessInfo.thermalStateDidChangeNotification, object: nil)
        #expect(await settle { subscriber.session == SessionPolicy.Request(fps: 15, detail: .full) })
        reading.reading.heat = .nominal
        rig.powerCenter.post(name: ProcessInfo.thermalStateDidChangeNotification, object: nil)
        #expect(await settle { subscriber.session == SessionPolicy.Mode.full.request })
    }

    @Test("changing the cellular display request keeps the connected receive media open")
    func liveCellularModeChangesKeepReceiveMedia() async throws {
        let station = try FakeStation(additions: .wideband)
        let rig = try Self.rig(mediaPeerFactory: station.mediaPeerFactory)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let model = rig.app
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        var mediaId = await model.media.connectionId
        for _ in 0..<50_000 where mediaId == nil {
            await Task.yield()
            mediaId = await model.media.connectionId
        }
        let originalId = try #require(mediaId)
        let originalSession = try #require(model.session)
        let originalPeer = try #require(station.mediaPeers.first)
        let subscriber = model.main.subscriber
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        #expect(await settle { subscriber.session == SessionPolicy.Mode.balanced.request })
        rig.settings.cellularMode = .full
        #expect(await settle { subscriber.session == SessionPolicy.Mode.full.request })
        rig.settings.cellularMode = .audioOnly
        #expect(await settle { !subscriber.session.subscribes })
        rig.settings.cellularMode = .balanced
        #expect(await settle { subscriber.session == SessionPolicy.Mode.balanced.request })
        #expect(model.session === originalSession)
        #expect(model.connection == .connected)
        #expect(await model.media.connectionId == originalId)
        #expect(station.connectionCount == 1)
        #expect(station.mediaPeers.count == 1)
        #expect(!originalPeer.isClosed)
        await model.disconnect()
    }

    @Test("away with Sound only the band stops, and a stretch of a minute or more is marked on return")
    func soundOnly() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let subscriber = rig.app.main.subscriber
        let band = rig.app.main.band
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.sceneChanged(inForeground: false)
        #expect(!subscriber.session.subscribes)
        rig.session.phoneLocked()
        rig.clock.now = Self.start.addingTimeInterval(33 * 60)
        rig.session.sceneChanged(inForeground: true)
        #expect(subscriber.session == SessionPolicy.Mode.full.request)
        #expect(band.awayMarks.count == 1)
        #expect(band.awayMarks.first?.text.hasPrefix("Locked ") == true)
        #expect(band.awayMarks.first?.text.hasSuffix(" \u{00B7} sound only") == true)

        // A short look at another app is not marked.
        rig.session.sceneChanged(inForeground: false)
        rig.clock.now = rig.clock.now.addingTimeInterval(20)
        rig.session.sceneChanged(inForeground: true)
        #expect(band.awayMarks.count == 1)

        // With Sound only off the band keeps coming, and nothing is marked.
        rig.settings.soundOnlyWhenAway = false
        #expect(await settle { true })
        rig.session.sceneChanged(inForeground: false)
        #expect(subscriber.session.subscribes)
        rig.clock.now = rig.clock.now.addingTimeInterval(3600)
        rig.session.sceneChanged(inForeground: true)
        #expect(band.awayMarks.count == 1)

        let utc = TimeZone(identifier: "UTC") ?? .current
        #expect(SessionController.awayText(from: Self.start, to: Self.start.addingTimeInterval(33 * 60), locked: true,
                                           timeZone: utc) == "Locked 19:42 to 20:15 \u{00B7} sound only")
        #expect(SessionController.awayText(from: Self.start, to: Self.start.addingTimeInterval(33 * 60), locked: false,
                                           timeZone: utc) == "In another app 19:42 to 20:15 \u{00B7} sound only")
    }

    @Test("sound-only marks belong to the connected listening intent, never no session or an older owner")
    func soundOnlyOwner() throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let band = rig.app.main.band

        rig.session.sceneChanged(inForeground: false)
        rig.clock.now = Self.start.addingTimeInterval(120)
        rig.session.sceneChanged(inForeground: true)
        #expect(band.awayMarks.isEmpty)

        rig.session.sessionStarted(owner: 1)
        rig.session.sceneChanged(inForeground: false)
        rig.clock.now = rig.clock.now.addingTimeInterval(120)
        rig.session.sceneChanged(inForeground: true)
        #expect(band.awayMarks.isEmpty)

        rig.session.connectionChanged(.connected)
        rig.session.sceneChanged(inForeground: false)
        rig.clock.now = rig.clock.now.addingTimeInterval(120)
        rig.session.sessionEnded(owner: 1)
        rig.session.sessionStarted(owner: 2)
        rig.session.connectionChanged(.connected)
        rig.clock.now = rig.clock.now.addingTimeInterval(20)
        rig.session.sceneChanged(inForeground: true)
        #expect(band.awayMarks.isEmpty)
    }

    @Test("the waterfall's mark moves down with each new line and goes once past the oldest")
    func markMoves() {
        let band = BandModel()
        band.endpointId = 1
        _ = band.prepareToDraw(size: CGSize(width: 300, height: 600), scale: 1)
        band.markAway("Locked 19:42 to 20:15 \u{00B7} sound only")
        #expect(band.awayMarks.first?.linesSince == 0)
        BandFlagShotTests.feed(band, width: 64, lines: 5)
        _ = band.prepareToDraw(size: CGSize(width: 300, height: 600), scale: 1)
        #expect(band.awayMarks.first?.linesSince == 5)
        BandFlagShotTests.feed(band, width: 64, lines: band.state.history.capacity + 1)
        _ = band.prepareToDraw(size: CGSize(width: 300, height: 600), scale: 1)
        #expect(band.awayMarks.isEmpty)
    }

    @Test("the first time on cellular the band says it is at Balanced and what that costs, once")
    func cellularNote() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        #expect(rig.session.note == nil)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        #expect(rig.session.note == nil)
        #expect(!rig.settings.cellularNoteShown)
        rig.session.bandShown(true)
        #expect(rig.session.note?.kind == .cellular)
        #expect(rig.session.note?.text
            == "On cellular: Balanced. 15 frames a second, about 45 MB an hour. Change it in Setup, Data use.")
        #expect(!rig.settings.cellularNoteShown)
        if let note = rig.session.note { rig.session.noteAppeared(note) }
        #expect(rig.settings.cellularNoteShown)
        rig.network.set(NetworkPath(online: true, interfaces: ["en0"]))
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        #expect(rig.settings.cellularNoteShown)
        rig.session.sessionEnded(owner: 1)
    }

    @Test("an old equal-text cellular appearance cannot consume a replacement owner's card")
    func staleCellularAppearanceCannotConsumeReplacement() throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        let oldCard = try #require(rig.session.note)
        rig.session.sessionStarted(owner: 2)
        rig.session.connectionChanged(.connected)
        let newCard = try #require(rig.session.note)
        #expect(oldCard.text == newCard.text)
        #expect(oldCard != newCard)
        rig.session.noteAppeared(oldCard)
        #expect(!rig.settings.cellularNoteShown)
        #expect(rig.session.note == newCard)
        rig.session.noteAppeared(newCard)
        #expect(rig.settings.cellularNoteShown)
    }

    @Test("an acknowledged cellular card cannot return with the old mode after Setup changes it")
    func cellularModeChangeRetiresStaleCard() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        let balanced = try #require(rig.session.note)
        rig.session.noteAppeared(balanced)
        rig.session.bandShown(false)
        rig.settings.cellularMode = .full
        #expect(await settle { rig.session.request == SessionPolicy.Mode.full.request })
        rig.session.bandShown(true)
        #expect(rig.session.note == nil)
        #expect(rig.settings.cellularNoteShown)
    }

    @Test("a cancelled note timer cannot dismiss the same card after the band returns")
    func cancelledNoteTimerCannotDismissResumedCard() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let gate = NoteWaitGate()
        let resumed = AsyncStream.makeStream(of: Void.self)
        rig.session.noteTimerWaitForTesting = {
            await gate.pause()
            resumed.continuation.yield(())
        }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        let card = try #require(rig.session.note)
        rig.session.noteAppeared(card)
        #expect(await settle { gate.count == 1 })
        rig.session.bandShown(false)
        rig.session.bandShown(true)
        #expect(await settle { gate.count == 2 })
        gate.release(0)
        for await _ in resumed.stream { break }
        #expect(rig.session.note == card)
        rig.session.sessionEnded(owner: 1)
        gate.release(1)
        resumed.continuation.finish()
    }

    @Test("an acknowledged first-cellular card ends with its listening owner")
    func acknowledgedCellularNoteEndsWithOwner() throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        let firstCard = try #require(rig.session.note)
        #expect(firstCard.kind == .cellular)
        rig.session.noteAppeared(firstCard)
        #expect(rig.settings.cellularNoteShown)

        rig.session.sessionEnded(owner: 1)
        #expect(rig.session.note == nil)
        rig.session.sessionStarted(owner: 2)
        rig.session.connectionChanged(.connected)
        #expect(rig.session.note == nil)
        #expect(rig.settings.cellularNoteShown)
    }

    @Test("an acknowledged monthly card ends with its listening owner")
    func acknowledgedMonthWarningEndsWithOwner() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.settings.cellularNoteShown = true
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        rig.traffic.add(in: DataUseMeter.warningBytes + 1, out: 0)
        await rig.session.tick()
        let firstCard = try #require(rig.session.note)
        #expect(firstCard.kind == .monthWarning)
        rig.session.noteAppeared(firstCard)
        let warnedMonth = rig.settings.string(DataUseMeter.warnedMonthKey, default: "")
        #expect(!warnedMonth.isEmpty)

        rig.session.sessionEnded(owner: 1)
        #expect(rig.session.note == nil)
        rig.session.sessionStarted(owner: 2)
        rig.session.connectionChanged(.connected)
        #expect(rig.session.note == nil)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "") == warnedMonth)
    }

    @Test("a replacement retires the old card and stale teardown leaves the new warning alone")
    func replacementRetiresAcknowledgedCard() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        let firstCard = try #require(rig.session.note)
        #expect(firstCard.kind == .cellular)
        rig.session.noteAppeared(firstCard)
        rig.traffic.add(in: DataUseMeter.warningBytes + 1, out: 0)
        await rig.session.tick()
        #expect(rig.session.note == firstCard)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)

        rig.session.sessionStarted(owner: 2)
        #expect(rig.session.note == nil)
        rig.session.connectionChanged(.connected)
        let newCard = try #require(rig.session.note)
        #expect(newCard.kind == .monthWarning)
        rig.session.sessionEnded(owner: 1)
        #expect(rig.session.note == newCard)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
        rig.session.noteAppeared(newCard)
        #expect(!rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
    }

    @Test("a new month shows its own warning after the prior month's card was acknowledged")
    func newMonthWarningAfterOldCard() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.settings.cellularNoteShown = true
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        rig.traffic.add(in: 6_000_000_000, out: 0)
        await rig.session.tick()
        let septemberCard = try #require(rig.session.note)
        #expect(septemberCard.kind == .monthWarning)
        #expect(septemberCard.text.contains("6.00 GB"))
        rig.session.noteAppeared(septemberCard)
        let septemberKey = rig.settings.string(DataUseMeter.warnedMonthKey, default: "")

        rig.session.sessionEnded(owner: 1)
        rig.clock.now = Self.start.addingTimeInterval(5 * 86_400)
        rig.session.sessionStarted(owner: 2)
        rig.session.connectionChanged(.connected)
        #expect(rig.session.note == nil)
        rig.traffic.add(in: 5_100_000_000, out: 0)
        await rig.session.tick()
        let octoberCard = try #require(rig.session.note)
        #expect(octoberCard.kind == .monthWarning)
        #expect(octoberCard.text.contains("5.10 GB"))
        #expect(octoberCard != septemberCard)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "") == septemberKey)
        rig.session.noteAppeared(octoberCard)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "") != septemberKey)
    }

    @Test("a still-hidden cellular notice remains eligible through same-owner link recovery")
    func pendingCellularNoteSurvivesRecovery() throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        #expect(rig.session.note == nil)
        rig.session.connectionChanged(.waitingToRetry(seconds: 1, reason: nil))
        rig.session.bandShown(true)
        #expect(rig.session.note == nil)
        #expect(!rig.settings.cellularNoteShown)
        rig.session.connectionChanged(.connected)
        let recoveredCard = try #require(rig.session.note)
        #expect(recoveredCard.kind == .cellular)
        rig.session.noteAppeared(recoveredCard)
        #expect(rig.settings.cellularNoteShown)
    }

    @Test("a hidden cellular note waits for the band and survives a network change")
    func hiddenCellularNote() throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.sceneChanged(inForeground: false)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        rig.network.set(NetworkPath(online: true, interfaces: ["en0"]))
        #expect(rig.session.note == nil)
        #expect(!rig.settings.cellularNoteShown)
        rig.session.sceneChanged(inForeground: true)
        rig.session.bandShown(true)
        #expect(rig.session.note == nil)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        #expect(rig.session.note?.kind == .cellular)
        #expect(!rig.settings.cellularNoteShown)
        if let note = rig.session.note { rig.session.noteAppeared(note) }
        #expect(rig.settings.cellularNoteShown)
    }

    @Test("the month warning survives a hidden cellular-to-Wi-Fi final sample")
    func warningOnNetworkChange() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.settings.cellularNoteShown = true
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        rig.traffic.add(in: DataUseMeter.warningBytes - 1, out: 0)
        await rig.session.tick()
        rig.session.sceneChanged(inForeground: false)
        rig.traffic.add(in: 2, out: 0)
        rig.network.set(NetworkPath(online: true, interfaces: ["en0"]))
        #expect(rig.session.note == nil)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
        rig.session.sceneChanged(inForeground: true)
        rig.session.bandShown(true)
        #expect(rig.session.note?.kind == .monthWarning)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
        if let note = rig.session.note { rig.session.noteAppeared(note) }
        #expect(!rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
    }

    @Test("a final sample past 5 GB waits until the next session shows the band")
    func warningOnSessionEnd() throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.settings.cellularNoteShown = true
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        rig.traffic.add(in: DataUseMeter.warningBytes + 1, out: 0)
        rig.session.sessionEnded(owner: 1)
        #expect(rig.session.note == nil)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
        rig.session.sessionStarted(owner: 2)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        #expect(rig.session.note?.kind == .monthWarning)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
        if let note = rig.session.note { rig.session.noteAppeared(note) }
        #expect(!rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
    }

    @Test("a second due note waits for the displayed note instead of replacing it")
    func notesDoNotReplaceEachOther() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.session.sessionStarted(owner: 1)
        rig.session.connectionChanged(.connected)
        rig.session.bandShown(true)
        rig.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        #expect(rig.session.note?.kind == .cellular)
        if let note = rig.session.note { rig.session.noteAppeared(note) }
        rig.traffic.add(in: DataUseMeter.warningBytes + 1, out: 0)
        await rig.session.tick()
        #expect(rig.session.note?.kind == .cellular)
        #expect(rig.settings.string(DataUseMeter.warnedMonthKey, default: "").isEmpty)
    }

    @Test("control moves and media replacement keep the listening deadline and counts; old teardown is ignored")
    func listeningOwner() async throws {
        let rig = try Self.rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        rig.settings.sleepTimer = .thirtyMinutes
        rig.session.sessionStarted(owner: 10)
        rig.session.connectionChanged(.connected)
        let firstDeadline = rig.session.sleepTimer.endsAt
        rig.traffic.add(in: 100, out: 20)
        await rig.session.tick()
        #expect(rig.session.meter.session.total == 120)
        // A route move and media replacement publish no new listening intent.
        rig.session.connectionChanged(.waitingToRetry(seconds: 1, reason: nil))
        rig.clock.now = Self.start.addingTimeInterval(60)
        rig.session.connectionChanged(.connected)
        rig.session.sessionStarted(owner: 10)
        rig.traffic.add(in: 30, out: 10)
        await rig.session.tick()
        #expect(rig.session.sleepTimer.endsAt == firstDeadline)
        #expect(rig.session.meter.session.total == 160)
        rig.session.sessionStarted(owner: 11)
        rig.session.sessionEnded(owner: 10)
        #expect(rig.session.meter.session.total == 0)
        #expect(rig.session.sleepTimer.endsAt == nil)
        rig.session.connectionChanged(.connected)
        #expect(rig.session.sleepTimer.endsAt == rig.clock.now.addingTimeInterval(1800))
        rig.session.sessionEnded(owner: 11)
    }

    @Test("the Data use page's choices are the spec's, with its estimates, and D28's defaults")
    func dataPage() throws {
        let (settings, suite) = try Self.settings()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        #expect(settings.wifiMode == .full)
        #expect(settings.cellularMode == .balanced)
        settings.cellularMode = .full
        #expect(PhoneSettings(defaults: try #require(UserDefaults(suiteName: suite))).cellularMode == .full)
        settings.cellularMode = .balanced
        #expect(settings.soundOnlyWhenAway && settings.lowPowerDropsToSaver && settings.hotPhoneSlows)
        #expect(settings.keepScreenOn == .always)
        #expect(settings.sleepTimer == .off)
        // The spec's figures with the sound at High's 48 kbit/s (R-IOS-09).
        #expect(SessionPolicy.Mode.wifiModes.map(DataUsePage.wifiDetail) == [
            "30 frames a second, full detail \u{00B7} about 70 MB an hour",
            "15 frames a second \u{00B7} about 45 MB an hour",
        ])
        #expect(SessionPolicy.Mode.cellularModes.map(DataUsePage.cellularDetail) == [
            "The same as Wi-Fi \u{00B7} about 70 MB an hour",
            "15 frames a second \u{00B7} about 45 MB an hour",
            "5 frames a second, half the detail \u{00B7} about 30 MB an hour",
            "No band, just the sound \u{00B7} about 24 MB an hour",
        ])
        #expect(DataUsePage.footnote.contains("estimates"))
        #expect(DataUsePage.footnote
            == "The figures above include High audio. Save data audio uses about 13 MB an hour; "
            + "Lossless audio uses about 720 MB an hour while the connection carries it. "
            + "Transmitting adds about 24 MB for each hour of talking at High, 13 MB at Save data, "
            + "or 720 MB while the microphone connection carries Lossless. If it cannot, the microphone uses High. "
            + "These are estimates; actual data use varies.")
        #expect(BatteryAndSessionsPage.soundOnlyDetail
            == "The Core stops sending the band \u{00B7} about 24 MB an hour")
        // Lossless chosen: the cellular choices say its cost plainly.
        #expect(DataUsePage.cellularFooter(.lossless)
            == "Lossless audio is chosen in Setup, Audio. On cellular it uses about 720 MB an hour.")
        #expect(DataUsePage.cellularFooter(.high) == nil)
        #expect(DataUsePage.cellularFooter(.saveData) == nil)
        // A Wi-Fi mode that Wi-Fi does not offer reads as Full.
        settings.setString("saver", for: PhoneSettings.wifiModeKey)
        #expect(settings.wifiMode == .full)
    }
}
