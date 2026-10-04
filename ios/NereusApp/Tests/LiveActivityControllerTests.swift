// NereusSDR for iOS: the Live Activity's life: its start, its pace, the lock and UNKEY, the lost link and eight hours
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing
import UIKit

/// R-IOS-14, R-IOS-21, D24, D25: the controller behind the lock-screen card
/// and the Dynamic Island, over a stand-in for ActivityKit and a clock the
/// tests move, then the real app keying a fake Core.
@Suite("The Live Activity", .serialized)
@MainActor
struct LiveActivityControllerTests {
    /// This test's own lock, background time and idle timer.
    private let platform = TestPlatform()
    nonisolated static let base = Date(timeIntervalSinceReferenceDate: 800_000_000)

    @MainActor
    struct Harness {
        let host = FakeLiveActivityHost()
        let clock = TestLinkClock()
        let controller: LiveActivityController

        init() {
            let clock = clock
            controller = LiveActivityController(host: host, clock: clock, now: {
                LiveActivityControllerTests.base.addingTimeInterval(TimeInterval(clock.now) / 1000)
            })
        }

        var now: Date { LiveActivityControllerTests.base.addingTimeInterval(TimeInterval(clock.now) / 1000) }

        func advance(seconds: Double) async {
            await clock.advance(by: Int64(seconds * 1000))
            await controller.settle()
        }
    }

    /// Listening on 40 m, as the board draws it.
    static func listening(dbm: Double = -81) -> LiveActivityController.Inputs {
        var inputs = LiveActivityController.Inputs()
        inputs.bandShowing = true
        inputs.stationName = "KG4VCF/shack"
        inputs.link = .up
        inputs.roundTripMs = 38
        inputs.slice = StationActivityAttributes.Slice(letter: "A", colour: "#00D4FF", frequencyHz: 7_236_400,
                                                       mode: "LSB", bandwidth: "2.9K", signalDbm: dbm)
        return inputs
    }

    /// The card the controller would send for these inputs.
    static func card(_ inputs: LiveActivityController.Inputs) -> StationActivityAttributes.ContentState {
        let h = Harness()
        h.controller.update(inputs)
        return h.controller.content()
    }

    /// A time the test moves by hand.
    final class DateBox: @unchecked Sendable {
        private let lock = NSLock()
        private var date: Date

        init(_ date: Date) {
            self.date = date
        }

        var value: Date {
            get { lock.withLock { date } }
            set { lock.withLock { date = newValue } }
        }
    }

    static func keyed(for seconds: Int64, timeOutLeft: Int64 = 138) -> LiveActivityController.Inputs {
        var inputs = listening()
        inputs.transmitting = true
        inputs.keyedForSeconds = seconds
        inputs.forwardWatts = 100
        inputs.swr = 1.15
        inputs.timeOutRemainingSeconds = timeOutLeft
        return inputs
    }

    // MARK: Starting and ending

    @Test("it starts when the band first shows with the app in front, never before or from the background")
    func startsWithTheBand() async {
        let h = Harness()
        h.controller.sceneChanged(active: false)
        h.controller.update(Self.listening())
        #expect(h.host.handles.isEmpty)
        h.controller.sceneChanged(active: true)
        #expect(h.host.handles.count == 1)
        #expect(h.host.current?.first.stationName == "KG4VCF/shack")
        #expect(h.host.current?.first.slice?.frequencyHz == 7_236_400)
        #expect(h.host.current?.first.link == .up)
        #expect(h.host.current?.first.roundTripMs == 38)
        // Not while the connecting screens show.
        let other = Harness()
        other.controller.sceneChanged(active: true)
        var connecting = Self.listening()
        connecting.bandShowing = false
        other.controller.update(connecting)
        #expect(other.host.handles.isEmpty)
    }

    @Test("with Live Activities off nothing starts")
    func activitiesOff() {
        let h = Harness()
        h.host.activitiesEnabled = false
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        #expect(h.host.handles.isEmpty)
    }

    @Test("leaving the band ends the activity at once")
    func leavingTheBandEnds() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        var gone = Self.listening()
        gone.bandShowing = false
        h.controller.update(gone)
        await h.controller.settle()
        #expect(h.host.current?.ends.count == 1)
        #expect(h.host.current?.ends.first?.immediately == true)
    }

    // MARK: The pace

    @Test("listening, the reading changes at most every 5 seconds")
    func listeningPace() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        for step in 1...8 {
            await h.advance(seconds: 0.5)
            h.controller.update(Self.listening(dbm: -81 - Double(step)))
        }
        await h.controller.settle()
        #expect(h.host.current?.updates.isEmpty == true)
        await h.advance(seconds: 1)
        #expect(h.host.current?.updates.count == 1)
        #expect(h.host.current?.shown.slice?.signalDbm == -89)
        // Nothing new, nothing sent.
        await h.advance(seconds: 10)
        #expect(h.host.current?.updates.count == 1)
    }

    @Test("keyed, it changes every second, and the key itself goes at once")
    func keyedPace() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        await h.advance(seconds: 1)
        h.controller.update(Self.keyed(for: 0, timeOutLeft: 180))
        await h.controller.settle()
        #expect(h.host.current?.updates.count == 1)
        let shown = h.host.current?.shown
        #expect(shown?.keyed == true)
        #expect(shown?.keyedSince == h.now)
        #expect(shown?.timeOutAt == h.now.addingTimeInterval(180))
        #expect(shown?.forwardWatts == 100)
        #expect(shown?.swr == 1.15)
        var inputs = Self.keyed(for: 0, timeOutLeft: 180)
        for tenth in 1...20 {
            await h.advance(seconds: 0.1)
            inputs.forwardWatts = 100 - Double(tenth)
            h.controller.update(inputs)
        }
        await h.advance(seconds: 0.1)
        // Twenty changes over 2.1 s: the key at 1 s, then one each at 2 s
        // and 3 s (the heartbeat carries the newest reading), never more.
        #expect(h.host.current?.updates.count == 3)
        #expect(h.host.current?.shown.forwardWatts == 81)
    }

    @Test("the clocks stay put when the Core's count agrees with them, so iOS counts them on")
    func clocksStayPut() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 10, timeOutLeft: 170))
        let since = h.controller.content().keyedSince
        let end = h.controller.content().timeOutAt
        await h.advance(seconds: 5)
        h.controller.update(Self.keyed(for: 15, timeOutLeft: 165))
        #expect(h.controller.content().keyedSince == since)
        #expect(h.controller.content().timeOutAt == end)
        #expect(since == Self.base.addingTimeInterval(-10))
        #expect(end == Self.base.addingTimeInterval(170))
        // No time-out applies: none shows.
        h.controller.update(Self.keyed(for: 16, timeOutLeft: -1))
        #expect(h.controller.content().timeOutAt == nil)
    }

    // MARK: A card whose app has gone quiet

    @Test("every change carries a stale date: 3 s on while keyed, a minute otherwise, refreshed while the app lives")
    func staleDates() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        #expect(h.host.current?.firstStaleDate == h.now.addingTimeInterval(60))
        h.controller.update(Self.keyed(for: 0, timeOutLeft: 180))
        await h.controller.settle()
        let key = h.host.current?.updates.last
        #expect(key?.state.keyed == true)
        #expect(key?.staleDate == h.now.addingTimeInterval(3))
        // Nothing changes, yet the card is sent again each second with its stale date moved on.
        for second in 1...5 {
            await h.advance(seconds: 1)
            let last = h.host.current?.updates.last
            #expect(last?.staleDate == h.now.addingTimeInterval(3), "second \(second)")
        }
        #expect(h.host.current?.updates.count == 6)
        // Unkeyed: a minute on, and sent again every 30 seconds.
        h.controller.update(Self.listening())
        await h.controller.settle()
        #expect(h.host.current?.updates.last?.staleDate == h.now.addingTimeInterval(60))
        let count = h.host.current?.updates.count ?? 0
        await h.advance(seconds: 29)
        #expect(h.host.current?.updates.count == count)
        await h.advance(seconds: 1)
        #expect(h.host.current?.updates.count == count + 1)
        #expect(h.host.current?.updates.last?.staleDate == h.now.addingTimeInterval(60))
    }

    @Test("the stale date is worked out as the change goes to iOS, not as it was queued")
    func staleDateAtSend() async {
        let host = FakeLiveActivityHost()
        let clock = TestLinkClock()
        let box = DateBox(Self.base)
        let controller = LiveActivityController(host: host, clock: clock, now: { box.value })
        controller.sceneChanged(active: true)
        controller.update(Self.listening())
        // The queued change runs after time has moved on.
        controller.update(Self.keyed(for: 0))
        box.value = Self.base.addingTimeInterval(2)
        await controller.settle()
        #expect(host.current?.updates.last?.staleDate == Self.base.addingTimeInterval(5))
    }

    @Test("started while keyed, the first card carries the keyed stale date")
    func startedKeyed() {
        let h = Harness()
        h.controller.update(Self.keyed(for: 5))
        h.controller.sceneChanged(active: true)
        #expect(h.host.current?.firstStaleDate == h.now.addingTimeInterval(3))
    }

    @Test("a stale keyed card keeps UNKEY and says it may still be on the air; it never says it stopped")
    func staleKeyedCard() {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 40, timeOutLeft: 140))
        let fresh = h.controller.content()
        #expect(fresh.shown(stale: false) == fresh)
        let stale = fresh.shown(stale: true)
        #expect(stale.staleKeyed)
        #expect(!stale.keyed)
        #expect(stale.keyedSince == nil)
        #expect(stale.timeOutAt == nil)
        #expect(stale.forwardWatts == 0)
        #expect(stale.roundTripMs == nil)
        #expect(stale.link == .connecting)
        #expect(stale.message == "No news from the app. It may still be on the air.")
        #expect(stale.slice?.frequencyHz == 7_236_400)
        #expect(stale.stationName == "KG4VCF/shack")
    }

    @Test("a stale listening card says there's no news, but keeps a last message; a stale lost card too")
    func staleOtherCards() {
        let listening = Self.card(Self.listening())
        let stale = listening.shown(stale: true)
        #expect(!stale.staleKeyed)
        #expect(stale.message == "No news from the app. Open NereusSDR to check.")
        #expect(stale.slice?.signalDbm == nil)
        #expect(stale.roundTripMs == nil)
        #expect(stale.link == .connecting)
        var last = listening
        last.message = ActivityWords.eightHours
        #expect(last.shown(stale: true).message == ActivityWords.eightHours)
        var lost = listening
        lost.link = .lost
        lost.retry = StationActivityAttributes.Retry(attempt: 3, at: Self.base, stopped: false)
        let staleLost = lost.shown(stale: true)
        #expect(staleLost.link == .lost)
        #expect(staleLost.message == "No news from the app. Open NereusSDR to check.")
    }

    // MARK: Who may press what

    @Test("UNKEY, the speaker and Cancel run with one tap, locked or not; Reconnect waits for Face ID")
    func intentPolicies() {
        #expect(UnkeyIntent.authenticationPolicy == .alwaysAllowed)
        #expect(MuteIntent.authenticationPolicy == .alwaysAllowed)
        #expect(CancelReconnectingIntent.authenticationPolicy == .alwaysAllowed)
        #expect(ReconnectIntent.authenticationPolicy == .requiresAuthentication)
    }

    // MARK: Locking, UNKEY and the Core's stops

    @Test("locked while keyed: the card says the lock unkeyed it, only once the stop was sent and the Core let go")
    func lockedWhileKeyed() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 0))
        await h.advance(seconds: 42)
        h.controller.sceneChanged(active: false)
        // The stop has been sent, but the Core still has this phone on the air: no words yet.
        var sent = Self.keyed(for: 42)
        sent.localStop = TransmitModel.LocalStop(cause: .lock, serial: 1, wasTransmitting: true)
        h.controller.update(sent)
        await h.controller.settle()
        #expect(h.host.current?.shown.keyed == true)
        #expect(h.host.current?.shown.message == "")
        var off = Self.listening()
        off.localStop = sent.localStop
        h.controller.update(off)
        await h.controller.settle()
        let shown = h.host.current?.shown
        #expect(shown?.keyed == false)
        #expect(shown?.message == "Unkeyed when you locked the phone. TX ran 0:42.")
        #expect(shown?.keyedSince == nil)
        // Opening the app clears it.
        h.controller.sceneChanged(active: true)
        await h.controller.settle()
        #expect(h.host.current?.shown.message == "")
    }

    @Test("a key that ends with no stop of this phone's says nothing of a lock")
    func unkeyWithoutLocalStop() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 0))
        await h.advance(seconds: 10)
        h.controller.update(Self.listening())
        await h.controller.settle()
        #expect(h.host.current?.shown.message == "")
        // Locking while listening says nothing either.
        var locked = Self.listening()
        locked.localStop = TransmitModel.LocalStop(cause: .lock, serial: 1, wasTransmitting: false)
        h.controller.update(locked)
        await h.advance(seconds: 6)
        #expect(h.host.current?.shown.message == "")
    }

    @Test("UNKEY waits for the stop of everything here, then the card says how long TX ran")
    func unkeyFromTheIsland() async {
        let h = Harness()
        var unkeys = 0
        var mutes: [Bool] = []
        h.controller.setActions(LiveActivityController.Actions(unkey: { unkeys += 1 },
                                                               setMuted: { mutes.append($0) }))
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 0))
        await h.advance(seconds: 47)
        h.controller.sceneChanged(active: false)
        await h.controller.perform(.unkey)
        #expect(unkeys == 1)
        var off = Self.listening()
        off.localStop = TransmitModel.LocalStop(cause: .unkeyButton, serial: 1, wasTransmitting: true)
        h.controller.update(off)
        await h.controller.settle()
        #expect(h.host.current?.shown.message == "Unkeyed. TX ran 0:47.")
        await h.controller.perform(.setMuted(true))
        #expect(mutes == [true])
    }

    @Test("the Core's own stop shows in the Core's words, as sent")
    func coresStop() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 0))
        await h.advance(seconds: 180)
        var stopped = Self.listening()
        stopped.stop = TransmitStopNotice(reason: "timeOut", text: TransmitScreenTests.timeOutText, serial: 1)
        h.controller.update(stopped)
        await h.controller.settle()
        #expect(h.host.current?.shown.message == TransmitScreenTests.timeOutText)
    }

    @Test("the Core's stop that comes a moment before the PTT goes idle still shows, over the lock's words")
    func coresStopFirst() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 0))
        await h.advance(seconds: 180)
        var stopping = Self.keyed(for: 180)
        stopping.stop = TransmitStopNotice(reason: "timeOut", text: TransmitScreenTests.timeOutText, serial: 3)
        h.controller.update(stopping)
        var stopped = Self.listening()
        stopped.stop = stopping.stop
        stopped.localStop = TransmitModel.LocalStop(cause: .lock, serial: 1, wasTransmitting: true)
        h.controller.update(stopped)
        await h.controller.settle()
        #expect(h.host.current?.shown.message == TransmitScreenTests.timeOutText)
        // The same stop again changes nothing; the next key clears it.
        h.controller.update(stopped)
        h.controller.update(Self.keyed(for: 0))
        await h.controller.settle()
        #expect(h.host.current?.shown.message == "")
    }

    @Test("mute shows at once")
    func muteAtOnce() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        var muted = Self.listening()
        muted.muted = true
        h.controller.update(muted)
        await h.controller.settle()
        #expect(h.host.current?.updates.count == 1)
        #expect(h.host.current?.shown.muted == true)
    }

    // MARK: The link

    @Test("the link lost in another app while keyed: an alert, the Core has unkeyed, and the tries")
    func lostWhileKeyedAway() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 0))
        h.controller.sceneChanged(active: false)
        await h.advance(seconds: 20)
        var lost = Self.keyed(for: 20)
        lost.link = .lost
        lost.retry = StationActivityAttributes.Retry(attempt: 1, at: h.now.addingTimeInterval(1), stopped: false)
        h.controller.update(lost)
        await h.controller.settle()
        let last = h.host.current?.updates.last
        #expect(last?.alert == LiveActivityAlert(title: "Link lost",
                                                 body: "The Core unkeyed itself. The phone keeps trying to reconnect."))
        #expect(last?.state.link == .lost)
        #expect(last?.state.lostWhileKeyed == true)
        #expect(last?.state.retry?.attempt == 1)
        // The PTT catches up: still one alert.
        lost.transmitting = false
        lost.lostWhileKeyed = true
        h.controller.update(lost)
        // The next try is counted on the card.
        lost.retry = StationActivityAttributes.Retry(attempt: 2, at: h.now.addingTimeInterval(2), stopped: false)
        h.controller.update(lost)
        await h.controller.settle()
        #expect(h.host.current?.shown.retry?.attempt == 2)
        #expect(h.host.current?.updates.filter { $0.alert != nil }.count == 1)
        #expect(h.host.current?.shown.lostWhileKeyed == true)
        // Cancel stops the tries.
        lost.retry = StationActivityAttributes.Retry(attempt: 2, at: nil, stopped: true)
        h.controller.update(lost)
        await h.controller.settle()
        #expect(h.host.current?.shown.retry?.stopped == true)
    }

    @Test("a lost card carrying a note from before still shows its tries; the note comes back with the link")
    func lostCardKeepsItsTries() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.keyed(for: 0))
        await h.advance(seconds: 42)
        var off = Self.listening()
        off.localStop = TransmitModel.LocalStop(cause: .lock, serial: 1, wasTransmitting: true)
        h.controller.update(off)
        #expect(h.controller.content().message == "Unkeyed when you locked the phone. TX ran 0:42.")
        var lost = off
        lost.link = .lost
        lost.retry = StationActivityAttributes.Retry(attempt: 4, at: h.now.addingTimeInterval(8), stopped: false)
        h.controller.update(lost)
        await h.controller.settle()
        let shown = h.host.current?.shown
        #expect(shown?.message == "", "the retry line is shown, not the note")
        #expect(shown?.retry?.attempt == 4)
        lost.retry = StationActivityAttributes.Retry(attempt: 4, at: nil, stopped: true)
        h.controller.update(lost)
        #expect(h.controller.content().message == "")
        #expect(h.controller.content().retry?.stopped == true)
        // Only a stale lost card carries words there.
        #expect(h.controller.content().shown(stale: true).message == ActivityWords.noNews)
        h.controller.update(off)
        #expect(h.controller.content().message == "Unkeyed when you locked the phone. TX ran 0:42.")
    }

    @Test("the link lost with the app in front: no alert, the band says so")
    func lostInFront() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        var lost = Self.listening()
        lost.link = .lost
        lost.retry = StationActivityAttributes.Retry(attempt: 1, at: nil, stopped: false)
        h.controller.update(lost)
        await h.controller.settle()
        #expect(h.host.current?.updates.last?.alert == nil)
        #expect(h.host.current?.shown.link == .lost)
        #expect(h.host.current?.shown.lostWhileKeyed == false)
    }

    @Test("back on the air: the card says so, with transmit off")
    func backOnTheAir() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        var lost = Self.listening()
        lost.link = .lost
        h.controller.update(lost)
        h.controller.sceneChanged(active: false)
        var back = Self.listening()
        back.backOnAir = true
        h.controller.update(back)
        await h.controller.settle()
        let shown = h.host.current?.shown
        #expect(shown?.link == .up)
        #expect(shown?.keyed == false)
        #expect(shown?.message == "Back on the air.")
        #expect(shown?.messageGood == true)
    }

    // MARK: Eight hours

    @Test("just before eight hours the card gets its last message and ends; opening the app starts a fresh one")
    func eightHours() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        h.controller.sceneChanged(active: false)
        await h.advance(seconds: 8 * 3_600 - 5 * 60 - 1)
        #expect(h.host.current?.ends.isEmpty == true)
        await h.advance(seconds: 1)
        let first = h.host.handles.first
        #expect(first?.ends.count == 1)
        #expect(first?.ends.first?.immediately == false)
        #expect(first?.ends.first?.staleDate == h.now.addingTimeInterval(60))
        #expect(first?.ends.first?.state?.message
                == "Still listening. iOS ends this card after 8 hours; open NereusSDR to bring it back.")
        // The sound carries on; nothing starts in the background.
        h.controller.update(Self.listening(dbm: -60))
        await h.advance(seconds: 30)
        #expect(h.host.handles.count == 1)
        // Opening NereusSDR starts a fresh card.
        h.controller.sceneChanged(active: true)
        #expect(h.host.handles.count == 2)
        #expect(h.host.current?.first.message == "")
    }

    @Test("eight hours while keyed: the island stays until the unkey, then the last message, with a stale date")
    func eightHoursKeyed() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        h.controller.sceneChanged(active: false)
        await h.advance(seconds: 8 * 3_600 - 5 * 60 - 60)
        h.controller.update(Self.keyed(for: 0))
        await h.advance(seconds: 120)
        // Past the moment for the last message, still keyed: nothing ends (D25).
        #expect(h.host.current?.ends.isEmpty == true)
        #expect(h.host.current?.shown.keyed == true)
        h.controller.update(Self.listening())
        await h.controller.settle()
        let end = h.host.handles.first?.ends.first
        #expect(end?.state?.keyed == false)
        #expect(end?.state?.message == ActivityWords.eightHours)
        #expect(end?.staleDate == h.now.addingTimeInterval(60))
        #expect(end?.immediately == false)
    }

    @Test("a card iOS ended, or the operator swiped away, comes back when the app opens")
    func endedBySystem() async {
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        h.controller.sceneChanged(active: false)
        h.host.current?.endedBySystem = true
        h.controller.update(Self.listening(dbm: -60))
        await h.advance(seconds: 6)
        #expect(h.host.handles.count == 1)
        h.controller.sceneChanged(active: true)
        #expect(h.host.handles.count == 2)
    }

    @Test("while the radio is on the air for someone else, the card says who, in place of the signal")
    func onAirElsewhere() async {
        var inputs = Self.listening()
        inputs.onAirElsewhere = TransmitStateReport.radioLabel
        #expect(Self.card(inputs).onAirFrom == "Radio")
        #expect(ActivityWords.onAirFrom("Radio") == "On the air from Radio")
        #expect(ActivityWords.onAirFrom("MacBook") == "On the air from MacBook")
        #expect(ActivityWords.onAirFrom("") == "On the air")
        // It goes to iOS at once, not at the next five-second change.
        let h = Harness()
        h.controller.sceneChanged(active: true)
        h.controller.update(Self.listening())
        await h.controller.settle()
        #expect(h.host.current?.updates.isEmpty == true)
        h.controller.update(inputs)
        await h.controller.settle()
        #expect(h.host.current?.updates.count == 1)
        #expect(h.host.current?.shown.onAirFrom == "Radio")
        // This phone's own key is the keyed card, never "on the air from".
        var keyed = Self.keyed(for: 3)
        keyed.onAirElsewhere = "MacBook"
        #expect(Self.card(keyed).onAirFrom == nil)
        // A stale card can't know who is on the air now.
        #expect(Self.card(inputs).shown(stale: true).onAirFrom == nil)
    }

    // MARK: With the app and a fake Core

    @Test("switching apps while keyed keeps transmitting; the island's UNKEY sends tx.unkey through the PTT's queue")
    func switchAppsThenUnkey() async throws {
        let (model, station) = try await connected()
        let host = FakeLiveActivityHost()
        let controller = LiveActivityController(host: host)
        controller.observe(app: model, flow: nil)
        // The intents' actions reach this test's controller alone; the
        // handler the host app set is left as it is.
        let toController: @MainActor @Sendable (ActivityAction) async -> Void = { action in
            await controller.perform(action)
        }
        controller.sceneChanged(active: true)
        #expect(await settle(seconds: 30) { host.current != nil })

        let transmit = model.main.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        controller.sceneChanged(active: false)
        // D25: nothing unkeys when the app leaves the screen. Two more
        // keepalives after it prove the key ran on past it (an unkey stops
        // them), so the checks below come after a known point, not a guess.
        let keepalives = TransmitScreenTests.keepalives(station)
        #expect(await settle(seconds: 30) { TransmitScreenTests.keepalives(station) >= keepalives + 2 })
        #expect(station.keyed)
        #expect(!station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb == "tx.unkey" })
        #expect(await settle(seconds: 30) { host.current?.shown.keyed == true })

        // The intent's way in: iOS runs it in the app, which hands it here.
        try await ActivityAction.$taskHandler.withValue(toController) { try await UnkeyIntent().perform() }
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        let verbs = station.messages.compactMap(TransmitScreenTests.invoke).map(\.verb)
            .filter { $0 != "tx.keepalive" }
        #expect(verbs == ["tx.key", "tx.key", "tx.key", "tx.unkey", "tx.unkey", "tx.unkey"])
        #expect(!station.keyed)
        #expect(await settle(seconds: 30) { host.current?.shown.message.hasPrefix("Unkeyed. TX ran ") == true })
        // A second UNKEY, late, sends nothing and keys nothing. The intent
        // returns once its stop has had every answer, so whatever it sent
        // has reached the Core by then.
        try await ActivityAction.$taskHandler.withValue(toController) { try await UnkeyIntent().perform() }
        let after = station.messages.compactMap(TransmitScreenTests.invoke).map(\.verb)
        #expect(after.filter { $0 == "tx.key" }.count == 3)
        #expect(after.filter { $0 == "tx.unkey" }.count == 3)
        #expect(!station.keyed)
        await model.disconnect()
    }

    @Test("locking the phone while keyed sends tx.unkey under background time from iOS, and the card says the lock did it")
    func lockSendsUnkey() async throws {
        let (model, station) = try await connected()
        let host = FakeLiveActivityHost()
        let controller = LiveActivityController(host: host)
        controller.observe(app: model, flow: nil)
        controller.sceneChanged(active: true)
        let transmit = model.main.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        controller.sceneChanged(active: false)
        // Background time is asked for before anything goes, and given back
        // only once the three unkeys have gone to the Core. The recorder only
        // records; every check is made here, inside this test.
        var unkeysAtBegin: [Int] = []
        var unkeysAtEnd: [Int] = []
        func unkeys() -> Int {
            station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.unkey" }.count
        }
        platform.onBackgroundWorkBegin = { unkeysAtBegin.append(unkeys()) }
        platform.onBackgroundWorkEnd = { unkeysAtEnd.append(unkeys()) }
        // This test's lock reaches this test's app alone.
        platform.lock()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle && !unkeysAtEnd.isEmpty })
        #expect(platform.backgroundWorkBegun == ["Stop transmitting"])
        #expect(unkeysAtBegin == [0])
        #expect(unkeysAtEnd == [3])
        #expect(station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb == "tx.unkey" })
        #expect(!station.keyed)
        #expect(await settle(seconds: 30) {
            host.current?.shown.message.hasPrefix("Unkeyed when you locked the phone. TX ran ") == true
        })
        await model.disconnect()
    }

    @Test("the Core's time-out left shows on the card while keyed")
    func timeOutFromTheCore() async throws {
        let (model, station) = try await connected()
        let host = FakeLiveActivityHost()
        let controller = LiveActivityController(host: host)
        controller.observe(app: model, flow: nil)
        controller.sceneChanged(active: true)
        let transmit = model.main.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
            .init(ordinal: 8, name: "timeOutRemainingSeconds", value: .i64(133)),
            .init(ordinal: 9, name: "forwardPowerWatts", value: .f64(100)),
            .init(ordinal: 11, name: "swr", value: .f64(1.15)),
            .init(ordinal: 27, name: "keyedForSeconds", value: .i64(47)),
        ]))
        #expect(await settle(seconds: 30) { transmit.timeOutRemainingSeconds == 133 })
        #expect(await settle(seconds: 30) { host.current?.shown.timeOutAt != nil })
        let shown = try #require(host.current?.shown)
        let left = try #require(shown.timeOutAt).timeIntervalSinceNow
        #expect(left > 125 && left <= 134)
        #expect(shown.forwardWatts == 100)
        #expect(shown.swr == 1.15)
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        await model.disconnect()
    }

    @Test("another device's key and the radio's own PTT show on the card with the holder's name")
    func onAirFromTheCore() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        #expect(transmit.onAirElsewhere == nil)
        await station.deliver(TransmitScreenTests.txStateDelta(TransmitScreenTests.holder("mac-1", short: "MacBook",
                                                                                          keyed: true)))
        #expect(await settle(seconds: 30) { transmit.onAirElsewhere == "MacBook" })
        #expect(LiveActivityController.inputs(app: model, flow: nil).onAirElsewhere == "MacBook")
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 22, name: "holderSource", value: .utf8(TransmitStateReport.radioPttSource)),
        ]))
        #expect(await settle(seconds: 30) { transmit.onAirElsewhere == "Radio" })
        // Off the air: nothing to say.
        await station.deliver(TransmitScreenTests.txStateDelta(TransmitScreenTests.holder("", short: "", keyed: false)))
        #expect(await settle(seconds: 30) { transmit.onAirElsewhere == nil })
        await model.disconnect()
    }

    @Test("the card follows the active slice, as the desktop's RX dashboard does")
    func activeSlice() async throws {
        let (model, station) = try await connected()
        await station.deliver(BandFlagShotTests.slice(0, active: false))
        await station.deliver(BandFlagShotTests.slice(1, active: true))
        #expect(await settle(seconds: 30) { model.main.slices.active?.id == 1 })
        let slice = try #require(LiveActivityController.inputs(app: model, flow: nil).slice)
        #expect(slice.letter == "B")
        #expect(slice.frequencyHz == BandFlagShotTests.slices[1].hz)
        await model.disconnect()
    }

    // MARK: The fake Core

    private func connected() async throws -> (AppModel, FakeStation) {
        let suite = "LiveActivityControllerTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: [.remoteTx])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await settle(seconds: 30) { model.main.transmit.permitted && model.connection == .connected })
        return (model, station)
    }

    private func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }
}
