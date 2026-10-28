// NereusSDR for iOS: the Core's RF hold in the transmit state and when the phone shows "Held"
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-27: Remote transmit hold then cut: the hold's defaults and the
//   "Held" display rule. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import Testing
@testable import NereusLink

/// Spec section 5.3, decision 7: the phone's TX pill reads "Held" while the
/// Core holds RF off for its key, and for a hold it never saw held, for
/// that hold's length (at most 3000 ms) once the report that ends it lands.
@Suite("TransmitStateReport hold")
struct TransmitStateReportTests {
    @Test func cwTupleRequiresAllFieldsAndAValidNonzeroEpoch() {
        typealias Activity = TransmitStateReport.RemoteCwActivity
        #expect(Activity(deviceId: nil, epoch: 1, active: true) == nil)
        #expect(Activity(deviceId: "aa", epoch: nil, active: true) == nil)
        #expect(Activity(deviceId: "aa", epoch: 1, active: nil) == nil)
        #expect(Activity(deviceId: "", epoch: 1, active: true) == nil)
        #expect(Activity(deviceId: "aa", epoch: 0, active: true) == nil)
        #expect(Activity(deviceId: "aa", epoch: Int64(UInt32.max) + 1, active: true) == nil)
        let ended = Activity(deviceId: "aa", epoch: Int64(UInt32.max), active: false)
        #expect(ended?.active == false && ended?.epoch == Int64(UInt32.max))
        #expect(TransmitStateReport().remoteCw == nil)
    }

    private let start = ContinuousClock.now

    private func at(_ ms: Int) -> ContinuousClock.Instant {
        start.advanced(by: .milliseconds(ms))
    }

    private func report(held: Bool = false, reason: String = "", serial: UInt64 = 0,
                        lastMs: Int64 = 0) -> TransmitStateReport {
        var report = TransmitStateReport()
        report.keyed = true
        report.rfHeld = held
        report.rfHeldReason = reason
        report.rfHoldSerial = serial
        report.rfLastHoldMs = lastMs
        return report
    }

    @Test("a report with nothing of the hold in it is unheld, as from an older Core")
    func defaultsAreUnheld() {
        let report = TransmitStateReport()
        #expect(!report.rfHeld)
        #expect(report.rfHeldReason.isEmpty)
        #expect(report.rfHoldSerial == 0)
        #expect(report.rfLastHoldMs == 0)
        var display = TransmitStateReport.HoldDisplay()
        display.observe(report, keyed: true, now: at(0))
        #expect(!display.showsHeld(report, keyed: true, now: at(0)))
        #expect(TransmitStateReport.linkReason == "link")
        #expect(TransmitStateReport.micReason == "mic")
    }

    @Test("keyed and held reads Held for either reason; unkeyed never does")
    func liveHold() {
        var display = TransmitStateReport.HoldDisplay()
        display.observe(report(), keyed: true, now: at(0))
        for reason in [TransmitStateReport.linkReason, TransmitStateReport.micReason] {
            let held = report(held: true, reason: reason)
            display.observe(held, keyed: true, now: at(10))
            #expect(display.showsHeld(held, keyed: true, now: at(10)))
            #expect(!display.showsHeld(held, keyed: false, now: at(10)))
        }
        // A hold seen live shows nothing once it ends.
        let ended = report(serial: 1, lastMs: 900)
        display.observe(ended, keyed: true, now: at(20))
        #expect(!display.showsHeld(ended, keyed: true, now: at(21)))
        #expect(display.afterHoldUntil == nil)
    }

    @Test("a hold never seen held shows for its length, then TX")
    func unseenHoldShowsForItsLength() {
        var display = TransmitStateReport.HoldDisplay()
        display.observe(report(), keyed: true, now: at(0))
        let ended = report(serial: 1, lastMs: 400)
        display.observe(ended, keyed: true, now: at(100))
        #expect(display.showsHeld(ended, keyed: true, now: at(100)))
        #expect(display.showsHeld(ended, keyed: true, now: at(499)))
        #expect(!display.showsHeld(ended, keyed: true, now: at(500)))
    }

    @Test("the length shown is capped at 3000 ms")
    func capped() {
        var display = TransmitStateReport.HoldDisplay()
        display.observe(report(), keyed: true, now: at(0))
        let ended = report(serial: 1, lastMs: 5200)
        display.observe(ended, keyed: true, now: at(0))
        #expect(TransmitStateReport.HoldDisplay.maxAfterHoldMs == 3000)
        #expect(display.showsHeld(ended, keyed: true, now: at(2999)))
        #expect(!display.showsHeld(ended, keyed: true, now: at(3000)))
    }

    @Test("two holds ended at once: the last was never seen, so it shows")
    func twoHoldsAtOnce() {
        var display = TransmitStateReport.HoldDisplay()
        display.observe(report(), keyed: true, now: at(0))
        display.observe(report(held: true, reason: "link"), keyed: true, now: at(10))
        let ended = report(serial: 2, lastMs: 300)
        display.observe(ended, keyed: true, now: at(20))
        #expect(display.showsHeld(ended, keyed: true, now: at(310)))
        #expect(!display.showsHeld(ended, keyed: true, now: at(320)))
    }

    @Test("a hold that ended while unkeyed is not shown, and unkeying ends an after-hold display")
    func unkeyed() {
        var display = TransmitStateReport.HoldDisplay()
        display.observe(report(), keyed: false, now: at(0))
        let ended = report(serial: 1, lastMs: 800)
        display.observe(ended, keyed: false, now: at(10))
        #expect(!display.showsHeld(ended, keyed: true, now: at(10)))
        display.observe(ended, keyed: true, now: at(20))
        #expect(!display.showsHeld(ended, keyed: true, now: at(20)))

        let next = report(serial: 2, lastMs: 800)
        display.observe(next, keyed: true, now: at(30))
        #expect(display.showsHeld(next, keyed: true, now: at(40)))
        display.observe(next, keyed: false, now: at(50))
        display.observe(next, keyed: true, now: at(60))
        #expect(!display.showsHeld(next, keyed: true, now: at(60)))
    }

    @Test("a serial that goes down is no hold ended, and counting starts again from it")
    func serialGoesDown() {
        var display = TransmitStateReport.HoldDisplay()
        display.observe(report(serial: 5, lastMs: 2500), keyed: true, now: at(0))
        let other = report(serial: 2, lastMs: 900)
        display.observe(other, keyed: true, now: at(10))
        #expect(!display.showsHeld(other, keyed: true, now: at(10)))
        #expect(display.afterHoldUntil == nil)
        // The new Core's next hold shows for its own length.
        let next = report(serial: 3, lastMs: 300)
        display.observe(next, keyed: true, now: at(20))
        #expect(display.showsHeld(next, keyed: true, now: at(319)))
        #expect(!display.showsHeld(next, keyed: true, now: at(320)))
    }

    @Test("a serial that goes down ends an after-hold display already running")
    func serialGoesDownEndsAfterHold() {
        var display = TransmitStateReport.HoldDisplay()
        display.observe(report(serial: 4), keyed: true, now: at(0))
        let ended = report(serial: 5, lastMs: 2000)
        display.observe(ended, keyed: true, now: at(10))
        #expect(display.showsHeld(ended, keyed: true, now: at(20)))
        let other = report(serial: 1, lastMs: 2000)
        display.observe(other, keyed: true, now: at(30))
        #expect(!display.showsHeld(other, keyed: true, now: at(30)))
    }

    @Test("the first report is already seen, whatever it reads")
    func firstReportIsSeen() {
        var display = TransmitStateReport.HoldDisplay()
        let first = report(serial: 7, lastMs: 1500)
        display.observe(first, keyed: true, now: at(0))
        #expect(!display.showsHeld(first, keyed: true, now: at(0)))
        display.reset()
        display.observe(report(serial: 9, lastMs: 1500), keyed: true, now: at(10))
        #expect(display.afterHoldUntil == nil)
    }
}
