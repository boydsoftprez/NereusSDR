// NereusSDR for iOS: Setup > Hardware > Calibration's Level Cal against a Core: the questions, the slice sent, the run and the band's line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// R-IOS-18 (Level Cal 2, JJ's board 2026-09-30): Start asks the desktop's
/// question and then asks the Core to calibrate the phone's own active
/// slice with the level and frequency on the page; No sends nothing; the
/// Core's refusal shows as sent; while the Core reports a run the page
/// greys Start and Reset with the desktop's reason and the band says which
/// slice is being calibrated; a run this phone started ends with the
/// desktop's alert, one started elsewhere with the status line only. A
/// Core without the verbs greys all three with the desktop's two reasons.
@Suite("Level Cal", .serialized)
@MainActor
struct LevelCalTests {
    @Test("Start asks first, then sends the phone's active slice, level and frequency; the run shows on the page and the band")
    func startSendsThePhonesSlice() async throws {
        let (model, station) = try await connected()
        await station.deliver(BandFlagShotTests.slice(1, active: true))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 11, name: "active", value: .bool(false)),
        ])))
        let levelCal = model.levelCal
        #expect(await settle { model.main.slices.activeSliceId == 1 && levelCal.sliceLine == "Calibrates slice B." })
        #expect(levelCal.startEnabled && levelCal.resetEnabled && !levelCal.cancelEnabled)
        #expect(levelCal.reasons == [LevelCalModel.nothingToStopText])
        #expect(levelCal.frequencyHz == 14_100_000 && levelCal.levelDbm == -73)
        levelCal.levelDbm = -60
        levelCal.frequencyHz = 7_100_000

        // No sends nothing.
        levelCal.start()
        #expect(levelCal.alert?.kind == .startQuestion)
        #expect(levelCal.alert?.title == "Level Calibration Check")
        #expect(levelCal.alert?.text == "Is the calibrated signal present at the correct frequency?")
        levelCal.dismissAlert()
        try? await Task.sleep(for: .milliseconds(200))
        #expect(invokes(station, LevelCalModel.startVerb).isEmpty)

        // Yes sends the phone's own active slice.
        levelCal.start()
        levelCal.confirmStart()
        #expect(await settle { invokes(station, LevelCalModel.startVerb).count == 1 })
        #expect(invokes(station, LevelCalModel.startVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "levelDbm", value: .f64(-60)),
            LinkMessage.PropertyEntry(name: "frequencyHz", value: .f64(7_100_000)),
            LinkMessage.PropertyEntry(name: "sliceId", value: .i64(1)),
        ])
        await runs(station, true, percent: 42)
        await answer(station, LevelCalModel.startVerb)
        #expect(await settle { levelCal.running && levelCal.percent == 42 })
        #expect(!levelCal.startEnabled && !levelCal.resetEnabled && levelCal.cancelEnabled)
        #expect(levelCal.reasons == [LevelCalModel.runningText])
        #expect(levelCal.message.isEmpty)
        #expect(levelCal.bandLine == "Level calibration is running on slice B.")

        // It ends: the desktop's alert on this phone, the Core's status line, no band line.
        await runs(station, false, percent: 100, message: "Level calibration finished.", succeeded: true)
        #expect(await settle { !levelCal.running && levelCal.bandLine == nil })
        #expect(levelCal.alert == LevelCalModel.Alert(kind: .message, title: "Calibration",
                                                      text: "Level Calibration complete."))
        #expect(levelCal.message == "Level calibration finished." && levelCal.percent == 100)
        levelCal.dismissAlert()
        #expect(levelCal.startEnabled)
        await model.disconnect()
    }

    @Test("a refused Start shows the Core's words as sent; Reset asks first; Cancel stops a run started elsewhere, with no alert")
    func refusalsResetAndARunFromElsewhere() async throws {
        let (model, station) = try await connected()
        let levelCal = model.levelCal
        #expect(await settle { levelCal.startEnabled && levelCal.sliceLine == "Calibrates slice A." })
        levelCal.start()
        levelCal.confirmStart()
        #expect(await settle { invokes(station, LevelCalModel.startVerb).count == 1 })
        #expect(invokes(station, LevelCalModel.startVerb).first?.args.last
            == LinkMessage.PropertyEntry(name: "sliceId", value: .i64(0)))
        await answer(station, LevelCalModel.startVerb, refuse: "The radio is on the air. Try again when it stops.")
        #expect(await settle { levelCal.alert?.text == "The radio is on the air. Try again when it stops." })
        #expect(levelCal.alert?.title == "Level Calibration" && levelCal.alert?.kind == .message)
        levelCal.dismissAlert()

        // Reset: the desktop's question, then the verb.
        levelCal.reset()
        #expect(levelCal.alert == LevelCalModel.Alert(kind: .resetQuestion, title: "Level Defaults",
                                                      text: "Do you want to reset Level Calibration back to defaults?"))
        levelCal.confirmReset()
        #expect(await settle { invokes(station, LevelCalModel.resetVerb).count == 1 })
        #expect(invokes(station, LevelCalModel.resetVerb).first?.args.isEmpty == true)
        await answer(station, LevelCalModel.resetVerb)

        // Another device's run: the band says a run goes, without a slice;
        // Cancel stops it; its end brings no alert here.
        await runs(station, true, percent: 64)
        #expect(await settle { levelCal.running && levelCal.bandLine == "Level calibration is running." })
        levelCal.cancel()
        #expect(await settle { invokes(station, LevelCalModel.cancelVerb).count == 1 })
        await answer(station, LevelCalModel.cancelVerb)
        await runs(station, false, percent: 64, message: "Level calibration was canceled.", succeeded: false)
        #expect(await settle { !levelCal.running && levelCal.message == "Level calibration was canceled." })
        #expect(levelCal.alert == nil && levelCal.bandLine == nil)
        await model.disconnect()
    }

    @Test("a Core without the level calibration greys Start, Reset and Cancel with the desktop's two reasons")
    func olderCoreGreysTheButtons() async throws {
        let (model, station) = try await connected(hardwareVersion: 11)
        let levelCal = model.levelCal
        #expect(await settle { levelCal.reasons == [LevelCalModel.runOlderCoreText, LevelCalModel.resetOlderCoreText] })
        #expect(!levelCal.startEnabled && !levelCal.resetEnabled && !levelCal.cancelEnabled && !levelCal.offered)
        levelCal.start()
        levelCal.confirmStart()
        levelCal.reset()
        levelCal.confirmReset()
        levelCal.cancel()
        try? await Task.sleep(for: .milliseconds(200))
        #expect(levelCal.alert == nil)
        #expect(invokes(station, LevelCalModel.startVerb).isEmpty && invokes(station, LevelCalModel.resetVerb).isEmpty
            && invokes(station, LevelCalModel.cancelVerb).isEmpty)
        #expect(levelCal.bandLine == nil)
        await model.disconnect()
    }

    // MARK: Inside

    /// A model connected to a fake Core at `radioHardwareVersion`
    /// `hardwareVersion` (12 runs the level calibration).
    private func connected(hardwareVersion: Int64 = 12) async throws -> (AppModel, FakeStation) {
        let station = try FakeStation()
        let defaults = try #require(UserDefaults(suiteName: "LevelCalTests"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected && model.mirror.isSnapshotComplete })
        var capabilities = model.mirror.capabilities
        capabilities["radioHardwareVersion"] = .int(hardwareVersion)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
        #expect(await settle { model.mirror.capabilityVersion("radioHardwareVersion") == hardwareVersion })
        return (model, station)
    }

    /// The Core's run state on `radio`, as its delta.
    private func runs(_ station: FakeStation, _ running: Bool, percent: Int64, message: String = "",
                      succeeded: Bool = false) async {
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 31, name: "levelCalRunning", value: .bool(running)),
            .init(ordinal: 32, name: "levelCalPercent", value: .i64(percent)),
            .init(ordinal: 33, name: "levelCalMessage", value: .utf8(message)),
            .init(ordinal: 34, name: "levelCalSucceeded", value: .bool(succeeded)),
        ])))
    }

    /// What the app sent of `verb`, in order.
    private func invokes(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == verb {
                return invoke
            }
            return nil
        }
    }

    /// Answers the app's latest `verb`, accepted or refused with the Core's words.
    private func answer(_ station: FakeStation, _ verb: String, refuse reason: String? = nil) async {
        guard let invoke = invokes(station, verb).last else {
            Issue.record("no \(verb) reached the Core")
            return
        }
        await station.deliver(.commandResult(LinkMessage.CommandResult(verb: verb, id: invoke.id,
                                                                       accepted: reason == nil,
                                                                       reason: reason ?? "", affected: [],
                                                                       values: nil)))
    }

    /// Waits, in real time, for `condition`; the fake Core answers on its own tasks.
    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(5))
        }
        return condition()
    }
}
