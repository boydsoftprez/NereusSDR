// NereusSDR for iOS: the tuning dial on the band: off until chosen, its detents, haptics and step menu
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// D12, R-IOS-12, spec section 5.1 item 8: the dial is off until Setup
/// names one; each detent moves the active slice one of its steps, written
/// to the Core as a drag's are, with a light impact per detent and a rigid
/// one per whole kilohertz; the dial's middle opens the flag's step menu;
/// a slice this phone may not change is not tuned.
@Suite("The tuning dial", .serialized)
@MainActor
struct DialTests {
    typealias Rig = BandTuningTests.Rig

    /// Records the impacts the dial plays.
    final class Recorder: DialImpactGenerator {
        var impacts: [DialImpact] = []
        var prepared = 0

        func prepare() {
            prepared += 1
        }

        func impact(_ impact: DialImpact) {
            impacts.append(impact)
        }
    }

    /// A clock the test moves by hand.
    final class TestClock {
        var now: TimeInterval = 1_000
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

    private func freshDefaults() throws -> UserDefaults {
        let name = "DialTests." + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defaults.removePersistentDomain(forName: name)
        return defaults
    }

    @Test("a new install shows no dial; the setting keeps the kind and the direction")
    func offUntilChosen() throws {
        let defaults = try freshDefaults()
        let settings = PhoneSettings(defaults: defaults)
        #expect(settings.dialKind == .off)
        #expect(!settings.dialReversed)
        settings.dialKind = .thumbwheel
        settings.dialReversed = true
        let again = PhoneSettings(defaults: defaults)
        #expect(again.dialKind == .thumbwheel)
        #expect(again.dialReversed)
        defaults.set("dial of the future", forKey: "phone." + PhoneSettings.dialKindKey)
        #expect(again.dialKind == .off)
        #expect(NavigationPage.title(.off) == "Off")
        #expect(NavigationPage.title(.waterfallKnob) == "Knob on the waterfall")
        #expect(NavigationPage.title(.sheetKnob) == "Pop-up knob")
        #expect(NavigationPage.title(.thumbwheel) == "Thumbwheel")
    }

    @Test("zoom stays at the waterfall's corner with no dial and moves up above the knob or the wheel")
    func zoomMakesRoom() {
        let waterfall = CGRect(x: 0, y: 430, width: 402, height: 365)
        let standard = DialLayer.zoomOrigin(kind: .off, waterfall: waterfall, sideways: false)
        #expect(standard == CGPoint(x: 402 - 12 - 101, y: 795 - 12 - 42))
        #expect(DialLayer.zoomOrigin(kind: .sheetKnob, waterfall: waterfall, sideways: false) == standard)
        // The board: the knob at 650 to 786, zoom's foot 10 above it.
        let knob = DialLayer.knobFrame(waterfall: waterfall, sideways: false)
        #expect(knob == CGRect(x: 402 - 12 - 136, y: 795 - 9 - 136, width: 136, height: 136))
        #expect(DialLayer.zoomOrigin(kind: .waterfallKnob, waterfall: waterfall, sideways: false).y
            == knob.minY - 10 - 42)
        let wheel = DialLayer.wheelFrame(waterfall: waterfall)
        #expect(wheel == CGRect(x: 100, y: 795 - 8 - 52, width: 402 - 112, height: 52))
        #expect(DialLayer.zoomOrigin(kind: .thumbwheel, waterfall: waterfall, sideways: false).y
            == wheel.minY - 9 - 42)
    }

    @Test("sideways on an iPhone 17 Pro Max the thumbwheel keeps its upright width, at the right")
    func wheelSidewaysKeepsItsUprightSize() {
        // Upright the band is the window's width, 440; sideways it is 956
        // across with the rounded edge's 62 on the right.
        let upright = DialLayer.wheelFrame(waterfall: CGRect(x: 0, y: 420, width: 440, height: 400))
        let waterfall = CGRect(x: 0, y: 190, width: 956, height: 120)
        let sideways = DialLayer.wheelFrame(waterfall: waterfall, sideways: true, uprightWidth: 440,
                                            trailingInset: 62)
        #expect(abs(sideways.width - upright.width) < 1)
        #expect(sideways.height == upright.height)
        #expect(sideways.maxX == 956 - 62 - DialLayer.rightInset)
        #expect(sideways.maxY == waterfall.maxY - DialLayer.wheelBottomInset)
        // Zoom sits to its left, level with it.
        let zoom = DialLayer.zoomOrigin(kind: .thumbwheel, waterfall: waterfall, sideways: true, trailingInset: 62,
                                        uprightWidth: 440)
        #expect(zoom.x == sideways.minX - DialLayer.sidewaysZoomGap - ZoomButtons.size.width)
        #expect(abs((zoom.y + ZoomButtons.size.height / 2) - sideways.midY) < 0.5)
        // Sideways it is never wider than the waterfall leaves room for.
        let narrow = CGRect(x: 0, y: 0, width: 300, height: 120)
        let room: CGFloat = 300 - DialLayer.wheelLeftInset - DialLayer.rightInset
        #expect(DialLayer.wheelFrame(waterfall: narrow, sideways: true, uprightWidth: 440).width == room)
    }

    @Test("sideways the knob keeps its upright size where it fits, and shrinks where the waterfall is short")
    func knobSidewaysKeepsItsUprightSize() {
        let upright = WaterfallKnob.metrics(sideways: false, waterfallHeight: 400).diameter
        #expect(WaterfallKnob.metrics(sideways: true, waterfallHeight: 200).diameter == upright)
        #expect(DialLayer.knobFrame(waterfall: CGRect(x: 0, y: 0, width: 956, height: 200), sideways: true,
                                    trailingInset: 62).width == upright)
        // Task 61's short waterfall: the knob shrinks to fit it.
        let short = WaterfallKnob.metrics(sideways: true, waterfallHeight: 94).diameter
        #expect(short < upright)
        #expect(abs(short - max(upright * 0.5, 94 - 2 * DialLayer.knobBottomInset)) < 0.001)
    }

    @Test("a full turn moves the active slice 3600 Hz at its 100 Hz step: 36 light impacts and 4 rigid ones")
    func fullTurnTunesAndTicks() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let recorder = Recorder()
        // A slow turn: each move a sixtieth of a second after the last.
        let clock = TestClock()
        let tuning = BandTuningModel(slices: rig.slices,
                                     haptics: DialHaptics(generator: recorder, clock: { clock.now }))
        #expect(tuning.dialTunes)
        #expect(tuning.dialStepLabel == "100 Hz")
        tuning.beginTurn(reversed: false)
        #expect(recorder.prepared == 1)
        for _ in 0..<72 {
            clock.now += 1.0 / 60
            tuning.turn(byRadians: 2 * .pi / 72)
        }
        // From 7.236400 to 7.240000: whole kilohertz at .237, .238, .239 and .240.
        #expect(recorder.impacts.filter { $0 == .light }.count == 36)
        #expect(recorder.impacts.filter { $0 == .rigid }.count == 4)
        #expect(recorder.impacts.suffix(2) == [.light, .rigid])
        #expect(rig.slices.entries.first?.slice.frequencyHz == 7_240_000)
        tuning.endTurn()
        // One write in flight at a time; the final value goes last.
        #expect(await settle { !rig.outbox.frequencies.isEmpty })
        rig.keepLastWrite()
        #expect(await settle { rig.outbox.frequencies.last == 7_240_000 })
        #expect(rig.outbox.messages.allSatisfy { message in
            if case .propertyWrite(let write) = message { return write.key == "slice:0" }
            return true
        })
    }

    @Test("reversed, clockwise tunes down; a new step applies from the next detent")
    func reversedAndStepChange() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let recorder = Recorder()
        let tuning = BandTuningModel(slices: rig.slices, haptics: DialHaptics(generator: recorder))
        tuning.beginTurn(reversed: true)
        tuning.turn(byRadians: DialModel.radiansPerDetent * 4)
        #expect(rig.slices.entries.first?.slice.frequencyHz == 7_236_000)
        // Four detents in one move are one tick, and the kilohertz's bump.
        #expect(recorder.impacts == [.light, .rigid])
        // The Core takes slice A's step to 1 kHz mid-turn.
        rig.store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 6, name: "stepHz", value: .i64(1_000)),
        ])))
        #expect(await settle { rig.slices.entries.first?.stepHz == 1_000 })
        tuning.turn(byRadians: DialModel.radiansPerDetent)
        #expect(rig.slices.entries.first?.slice.frequencyHz == 7_235_000)
        tuning.endTurn()
    }

    @Test("the thumbwheel rolled left tunes up, one detent every 12 points")
    func thumbwheelRolls() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let recorder = Recorder()
        let tuning = BandTuningModel(slices: rig.slices, haptics: DialHaptics(generator: recorder))
        tuning.beginTurn(reversed: false)
        tuning.roll(byPoints: -30)
        #expect(rig.slices.entries.first?.slice.frequencyHz == 7_236_600)
        #expect(recorder.impacts == [.light])
        tuning.endTurn()
    }

    @Test("a slice this phone may not change is not tuned, and nothing ticks")
    func lockedIsNotTuned() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        rig.store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 35, name: "locked", value: .bool(true)),
        ])))
        #expect(await settle { rig.slices.entries.first?.locked == true })
        let recorder = Recorder()
        let tuning = BandTuningModel(slices: rig.slices, haptics: DialHaptics(generator: recorder))
        #expect(!tuning.dialTunes)
        tuning.beginTurn(reversed: false)
        #expect(tuning.turn(byRadians: 2 * .pi).isEmpty)
        tuning.endTurn()
        #expect(recorder.impacts.isEmpty)
        for _ in 0..<200 {
            await Task.yield()
        }
        #expect(rig.outbox.frequencies.isEmpty)
    }

    @Test("the dial's middle opens the flag's step menu for the active slice, beside the dial; a pick writes the step")
    func middleOpensTheStepMenu() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let tuning = BandTuningModel(slices: rig.slices, haptics: DialHaptics(generator: Recorder()))
        tuning.toggleDialStepMenu()
        #expect(tuning.stepMenuSliceId == 0)
        #expect(tuning.stepMenuFromDial)
        let step = try #require(tuning.steps.first { $0.hz == 1_000 })
        tuning.pick(step)
        #expect(tuning.stepMenuSliceId == nil)
        #expect(!tuning.stepMenuFromDial)
        #expect(await settle {
            rig.outbox.messages.contains { message in
                if case .propertyWrite(let write) = message {
                    return write.key == "slice:0" && write.properties.first?.name == "stepHz"
                        && write.properties.first?.value == .i64(1_000)
                }
                return false
            }
        })
        // A second tap closes it; the flag's step still opens under the flag.
        tuning.toggleDialStepMenu()
        tuning.toggleDialStepMenu()
        #expect(tuning.stepMenuSliceId == nil)
        tuning.openStepMenu(sliceId: 1)
        #expect(!tuning.stepMenuFromDial)
    }

    @Test("the knob in a sheet comes up in place of the pad, and its frequency opens the pad")
    func sheetAndPad() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let tuning = BandTuningModel(slices: rig.slices, haptics: DialHaptics(generator: Recorder()))
        tuning.openDialSheet(sliceId: 0)
        #expect(tuning.dialSheetOpen)
        #expect(tuning.pad == nil)
        tuning.openPad(sliceId: 0)
        #expect(!tuning.dialSheetOpen)
        #expect(tuning.pad != nil)
        tuning.closePad()
        tuning.openDialSheet(sliceId: 0)
        tuning.closeDialSheet()
        #expect(!tuning.dialSheetOpen)
    }

    // MARK: The feel (Task 61, fix round 1)

    /// The rig, a recorder and a spinner on one test clock, the spinner
    /// driven by hand rather than by the display.
    private func spinnerRig() async throws -> (Rig, Recorder, TestClock, BandTuningModel, DialSpinner) {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let recorder = Recorder()
        let clock = TestClock()
        let tuning = BandTuningModel(slices: rig.slices,
                                     haptics: DialHaptics(generator: recorder, clock: { clock.now }))
        let spinner = DialSpinner(clock: { clock.now }, drivesItself: false)
        return (rig, recorder, clock, tuning, spinner)
    }

    static let frame = 1.0 / 120
    static let detent = DialModel.radiansPerDetent

    @Test("a slow drag turns the knob with the finger every frame and tunes at each detent's angle")
    func slowDragFollows() async throws {
        let (rig, recorder, clock, tuning, spinner) = try await spinnerRig()
        spinner.touchDown(tuning: tuning, reversed: false)
        var angle = 0.0
        for _ in 0..<95 {
            clock.now += Self.frame
            spinner.follow(byRadians: Self.detent / 10)
            angle += Self.detent / 10
            // The drawing is where the finger is, not at a detent.
            #expect(abs(spinner.angle - angle) < 1e-9)
            let detents = Int((angle / Self.detent + 1e-9).rounded(.down))
            #expect(rig.slices.entries.first?.slice.frequencyHz == 7_236_400 + Double(detents) * 100)
        }
        #expect(recorder.impacts.filter { $0 == .light }.count == 9)
        // Held still, then lifted: no coast, and the turn ends.
        clock.now += 0.1
        spinner.release()
        #expect(!spinner.isCoasting)
        #expect(await settle { !rig.outbox.frequencies.isEmpty })
    }

    @Test("a flick at 20 rad/s coasts 1.48 s under the friction and stops, tuning the detents it predicts")
    func flickCoasts() async throws {
        let (rig, _, clock, tuning, spinner) = try await spinnerRig()
        let feel = DialFeel.standard
        spinner.touchDown(tuning: tuning, reversed: false)
        for _ in 0..<8 {
            clock.now += Self.frame
            spinner.follow(byRadians: 20 * Self.frame)
        }
        // 1.33 rad under the finger: 7 detents.
        #expect(rig.slices.entries.first?.slice.frequencyHz == 7_237_100)
        spinner.release()
        #expect(spinner.isCoasting)
        let start = clock.now
        var frames = 0
        var previous = spinner.angle
        while spinner.isCoasting, frames < 1_000 {
            clock.now += Self.frame
            frames += 1
            spinner.frame(at: clock.now)
            // It slows: each frame turns no further than the one before.
            #expect(spinner.angle >= previous)
            previous = spinner.angle
        }
        let lasted = clock.now - start
        #expect(lasted >= feel.coastDuration(from: 20))
        #expect(lasted < feel.coastDuration(from: 20) + Self.frame)
        #expect(lasted > 1 && lasted < 2)
        // 1.33 + 7.8 rad is 52 detents: 45 more while coasting.
        #expect(abs(spinner.angle - (8 * 20 * Self.frame + feel.coastDistance(from: 20))) < 1e-9)
        #expect(rig.slices.entries.first?.slice.frequencyHz == 7_241_600.0)
    }

    @Test("a touch during the coast stops the knob where it is")
    func touchStopsTheCoast() async throws {
        let (rig, _, clock, tuning, spinner) = try await spinnerRig()
        spinner.touchDown(tuning: tuning, reversed: false)
        for _ in 0..<6 {
            clock.now += Self.frame
            spinner.follow(byRadians: -30 * Self.frame)
        }
        spinner.release()
        #expect(spinner.isCoasting)
        for _ in 0..<12 {
            clock.now += Self.frame
            spinner.frame(at: clock.now)
        }
        clock.now += 0.002
        spinner.touchDown(tuning: tuning, reversed: false)
        #expect(!spinner.isCoasting)
        let angle = spinner.angle
        let hz = rig.slices.entries.first?.slice.frequencyHz
        for _ in 0..<60 {
            clock.now += Self.frame
            spinner.frame(at: clock.now)
        }
        #expect(spinner.angle == angle)
        #expect(rig.slices.entries.first?.slice.frequencyHz == hz)
        #expect((hz ?? 0) < 7_236_400)
        spinner.release()
    }

    @Test("a fast spin sends the newest frequency in order, never a backlog, and the band shows it at once")
    func fastSpinWritesCoalesce() async throws {
        let (rig, _, clock, tuning, spinner) = try await spinnerRig()
        spinner.touchDown(tuning: tuning, reversed: false)
        for _ in 0..<8 {
            clock.now += Self.frame
            spinner.follow(byRadians: 40 * Self.frame)
        }
        spinner.release()
        var frames = 0
        while spinner.isCoasting, frames < 1_000 {
            clock.now += Self.frame
            frames += 1
            spinner.frame(at: clock.now)
            // The shown frequency is the knob's, frame by frame.
            let turned = spinner.angle / Self.detent
            #expect(rig.slices.entries.first?.slice.frequencyHz
                == 7_236_400 + (turned + 1e-9).rounded(.down) * 100)
        }
        // 2.67 + 15.8 rad: 105 detents in all.
        let final = 7_236_400.0 + 105 * 100
        #expect(rig.slices.entries.first?.slice.frequencyHz == final)
        // The Core answers each write as it comes; the phone sends only
        // the newest value waiting, in order.
        for _ in 0..<5 {
            #expect(await settle { !rig.outbox.frequencies.isEmpty })
            rig.keepLastWrite()
            for _ in 0..<500 {
                await Task.yield()
            }
        }
        let sent = rig.outbox.frequencies
        #expect(sent.last == final)
        #expect(sent == sent.sorted())
        #expect(Set(sent).count == sent.count)
        #expect(sent.count <= 3)
    }

    @Test("ticks are held to one a sixtieth of a second, the kilohertz bump still lands")
    func ticksAreRateLimited() {
        let recorder = Recorder()
        let clock = TestClock()
        let haptics = DialHaptics(generator: recorder, clock: { clock.now })
        // One frame of a fast spin passes twelve detents and a kilohertz.
        let frame = (1...12).map { DialEvent.detent(7_236_400 + Double($0) * 100) } + [.wholeKilohertz(7_237_000)]
        haptics.play(frame)
        #expect(recorder.impacts == [.light, .rigid])
        // The next ProMotion frame is too soon: nothing plays.
        clock.now += 1.0 / 120
        haptics.play(frame)
        #expect(recorder.impacts == [.light, .rigid])
        // One more frame: a sixtieth of a second since the last, it plays.
        clock.now += 1.0 / 120
        haptics.play([.detent(7_237_100)])
        #expect(recorder.impacts == [.light, .rigid, .light])
        clock.now += 1.0 / 60
        haptics.play([.detent(7_238_000), .wholeKilohertz(7_238_000)])
        #expect(recorder.impacts == [.light, .rigid, .light, .light, .rigid])
        // A second of a fast spin at 120 frames: at most 60 ticks, not 120.
        recorder.impacts.removeAll()
        for index in 0..<120 {
            clock.now += 1.0 / 120
            haptics.play([.detent(Double(index)), .detent(Double(index) + 0.5)])
        }
        #expect(recorder.impacts.count == 60)
    }
}
