// NereusSDR for iOS: the flag's finger-sized buttons: their sizes, their places, and what each does to its own slice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing

/// R-IOS-11, the board's redrawn flag (`#flagbtn-review`, approved
/// 2026-09-30): every control on a full flag and beside it is a button at
/// least 40 by 44 points (the frequency's thin frame 40 tall, as drawn),
/// no two touch areas overlap, the words stop at the board's caps, and
/// each button acts on its own slice, the one that is not active included.
@Suite("Flag buttons")
@MainActor
struct FlagButtonsTests {
    // MARK: Sizes and places

    static func geometry(width: CGFloat) -> BandGeometry {
        BandGeometry(centerHz: 7_245_000, spanHz: 48_000, size: CGSize(width: width, height: 300), dbmRange: -140 ... -40)
    }

    static func slice(_ id: Int, _ hz: Double) -> BandSlice {
        BandSlice(id: id, frequencyHz: hz, filterLowHz: -3000, filterHighHz: -100, colour: "#00D0FF",
                  lowerSideband: true, txSlice: id == 0)
    }

    /// Every touch area on the band's full flags, named, in the band's points.
    static func targets(slices: [BandSlice], width: CGFloat, large: Bool,
                        listening: Bool = false) -> [(name: String, rect: CGRect)] {
        let metrics = FlagMetrics(large: large)
        let geometry = geometry(width: width)
        let button = FlagSideButtons.diameter(large: large)
        let column = FlagLayout.sideColumnSize(button: button)
        let placements = FlagLayout.layout(slices: slices, activeSliceId: 0, geometry: geometry,
                                           flagHeight: { _ in metrics.height(rade: false, listening: listening) },
                                           sideColumn: column)
        var targets: [(String, CGRect)] = []
        for (slice, placement) in zip(slices, placements) where !placement.isFolded {
            let flag = placement.rect
            for target in metrics.targets(listening: listening) {
                targets.append(("\(slice.letter) \(target.name)", target.rect.offsetBy(dx: flag.minX, dy: flag.minY)))
            }
            let side = FlagLayout.sideColumnRect(flag: flag, lineX: geometry.x(forHz: slice.frequencyHz),
                                                 bandWidth: Double(width), column: column)
            for (index, name) in ["Close", "Lock", "More"].enumerated() {
                targets.append(("\(slice.letter) \(name)",
                                CGRect(x: side.minX, y: side.minY + CGFloat(index) * (button + FlagLayout.sideGap),
                                       width: button, height: button)))
            }
        }
        return targets
    }

    @Test("the full flag is 200 by 158 points at every text size, its four tabs span its whole width, the words stop at the caps")
    func theFlagsSize() {
        for large in [false, true] {
            let metrics = FlagMetrics(large: large)
            #expect(metrics.plainHeight == 158)
            #expect(metrics.height(rade: true, listening: false) == 176)
            #expect(metrics.height(rade: false, listening: true) == 202)
            #expect(metrics.height(rade: true, listening: true) == 220)
            let tabs = metrics.tabs()
            #expect(tabs.count == 4)
            #expect(tabs.map(\.width) == [40, 41.5, 67.5, 51])
            #expect(tabs.map(\.minX).min() == 0)
            #expect(tabs.map(\.maxX).max() == 200)
            #expect(tabs.last?.maxY == metrics.plainHeight - FlagMetrics.bottom)
            #expect(tabs.allSatisfy { $0.height == 44 })
        }
        #expect(FlagLayout.flagSize == CGSize(width: 200, height: 158))
        #expect(FlagLayout.largeFlagHeight == FlagMetrics(large: true).plainHeight)
        #expect(FlagLayout.radeFlagHeight == FlagMetrics(large: false).height(rade: true, listening: false))
        // The words grow by 1.25 in large type and stop at the board's caps.
        let regular = FlagMetrics(large: false)
        let large = FlagMetrics(large: true)
        #expect(regular.scale == 1 && large.scale == 1.25)
        #expect(regular.tabPoints == 12 && large.tabPoints == 14)
        #expect(regular.ownerPoints == 11 && large.ownerPoints == 12.5)
        #expect(regular.ownerLine == 14 && large.ownerLine == 16)
        #expect(regular.frequencyPoints == 24 && large.frequencyPoints == 30)
        #expect(FlagSideButtons.diameter(large: false) == 44 && FlagSideButtons.diameter(large: true) == 44)
    }

    @Test("every button on the flags and beside them is at least 40 by 44 points, and no two touch areas overlap",
          arguments: [false, true])
    func everyTargetIsFingerSized(_ large: Bool) {
        // Upright, one slice; sideways, the board's pair, both full; and a listened flag with its owner row.
        let cases: [([BandSlice], CGFloat, Bool)] = [
            ([Self.slice(0, 7_236_400)], 402, false),
            ([Self.slice(0, 7_236_400), Self.slice(1, 7_256_000)], 874, false),
            ([Self.slice(0, 7_236_400)], 402, true),
            ([Self.slice(0, 7_236_400), Self.slice(1, 7_256_000)], 874, true),
        ]
        for (slices, width, listening) in cases {
            let targets = Self.targets(slices: slices, width: width, large: large, listening: listening)
            #expect(targets.count == slices.count * (listening ? 11 : 10))
            for target in targets {
                // The frequency's thin frame is the board's 40 points tall; every other target 44.
                let tall: CGFloat = target.name.hasSuffix("Frequency") ? 40 : 44
                #expect(target.rect.width >= 40 && target.rect.height >= tall, "\(target.name) is \(target.rect.size)")
                #expect(target.rect.minX >= 0 && target.rect.maxX <= width, "\(target.name) leaves the band")
            }
            for (index, one) in targets.enumerated() {
                for other in targets[(index + 1)...] {
                    // Tabs that meet share an edge; a millionth of a point of rounding is not an overlap.
                    #expect(!FlagLayout.overlaps(one.rect.insetBy(dx: 0.001, dy: 0.001), other.rect),
                            "\(one.name) overlaps \(other.name)")
                }
            }
        }
    }

    @Test("sideways, the second flag folds when the lines are closer than a flag and its buttons: 246 points at every size")
    func theFoldThreshold() {
        let geometry = Self.geometry(width: 874)
        let perPoint = geometry.spanHz / 874
        let aHz = geometry.lowHz + 100 * perPoint
        for large in [false, true] {
            let threshold = 246.0
            let metrics = FlagMetrics(large: large)
            let column = FlagLayout.sideColumnSize(button: FlagSideButtons.diameter(large: large))
            #expect(Double(metrics.width + FlagLayout.sideGap + column.width) == threshold)
            func second(_ points: Double) -> FlagPlacement {
                FlagLayout.layout(slices: [Self.slice(0, aHz), Self.slice(1, aHz + points * perPoint)], activeSliceId: 0,
                                  geometry: geometry, flagHeight: { _ in metrics.plainHeight }, sideColumn: column)[1]
            }
            #expect(second(threshold - 0.5).isFolded)
            #expect(!second(threshold + 0.5).isFolded)
        }
    }

    @Test("the dial's step menu hangs under the frequency, and the menus stay on the band")
    func menusSitUnderTheirButtons() {
        let flag = CGRect(x: 150, y: 0, width: 200, height: 158)
        for large in [false, true] {
            let origin = StepMenuLayer.origin(flag: flag, bandWidth: 402, large: large)
            #expect(origin.y == 1 + 44 + 40 + FlagPopoverLayer.belowButton)
            #expect(origin.x + TuneStepMenu.width == flag.maxX - FlagMetrics.inset)
        }
        // The more menu's right edge on the more button's, kept on the band.
        let column = CGRect(x: 8, y: 0, width: 44, height: 136)
        let more = FlagPopoverLayer.moreButton(column: column, large: false)
        #expect(more == CGRect(x: 8, y: 92, width: 44, height: 44))
        let placed = FlagPopoverLayer.menuOrigin(under: more, width: 250, alignRight: true, bandWidth: 402)
        #expect(placed == CGPoint(x: FlagPopoverLayer.margin, y: 140))
        // A panel drops from under the flag and its buttons.
        #expect(FlagPopoverLayer.panelTop(flag: flag, column: column) == 160)
    }

    @Test("the X/RIT panel's step cycle climbs the desktop's ladder one rung a tap, wrapping, and reads \"%1 Hz\"")
    func theStepCycle() {
        #expect(FlagControls.stepLadder == [1, 10, 100, 500, 1000, 10000])
        #expect(FlagControls.nextStep(after: 100) == 500)
        #expect(FlagControls.nextStep(after: 500) == 1000)
        #expect(FlagControls.nextStep(after: 10000) == 1)
        #expect(FlagControls.nextStep(after: 1) == 10)
        // A step off the ladder goes to its first rung, as the desktop does.
        #expect(FlagControls.nextStep(after: 250) == 1)
        #expect(FlagControls.nextStep(after: nil) == 1)
        #expect(FlagControls.stepCycleText(1000) == "1000 Hz")
        #expect(FlagControls.stepCycleText(100) == "100 Hz")
        #expect(FlagTab.allCases.map(\.title) == ["Audio", "DSP", "Mode", "X/RIT"])
    }

    @Test("sideways the notice keeps clear of every flag's buttons")
    func noticeKeepsClearOfEveryFlag() {
        let a = CGRect(x: 80, y: 0, width: 242, height: 151)
        let b = CGRect(x: 330, y: 0, width: 242, height: 151)
        let rects = [a, b]
        let place = SeveralDevicesLayers.noticePlace(bandWidth: 874, waterfallTop: 120, sideways: true, leadingInset: 62,
                                                     avoid: rects)
        let notice = CGRect(x: place.x, y: place.y, width: place.width, height: 60)
        #expect(place.width >= SeveralDevicesLayers.noticeMinWidthBeside)
        for rect in rects {
            #expect(!FlagLayout.overlaps(rect, notice))
        }
        // Beside B, the widest room: against B's right side.
        #expect(place.x == b.maxX + 10)
        // Upright, below the lowest flag it would cover.
        let upright = SeveralDevicesLayers.noticePlace(bandWidth: 402, waterfallTop: 120, sideways: false, leadingInset: 0,
                                                       avoid: [CGRect(x: 0, y: 0, width: 200, height: 151),
                                                               CGRect(x: 202, y: 0, width: 200, height: 245)])
        #expect(upright.y == 253)
    }

    @Test("the VAX panel lights the channel whose slices list the flag's letter, Off for none")
    func vaxChannelFromTheCore() {
        let values: [String: MirrorValue] = [StationVax.slicesProperty(1): .text("A"),
                                             StationVax.slicesProperty(3): .text("BC")]
        #expect(FlagControls.vaxChannel(values: values, letter: "A") == 1)
        #expect(FlagControls.vaxChannel(values: values, letter: "C") == 3)
        #expect(FlagControls.vaxChannel(values: values, letter: "D") == 0)
    }

    // MARK: What each button does, on its own slice

    final class Sent: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [LinkMessage] = []

        var messages: [LinkMessage] {
            lock.lock()
            defer { lock.unlock() }
            return stored
        }

        func record(_ message: LinkMessage) {
            lock.lock()
            stored.append(message)
            lock.unlock()
        }

        func invokes(_ verb: String) -> [LinkMessage.CommandInvoke] {
            messages.compactMap { message in
                guard case .commandInvoke(let invoke) = message, invoke.verb == verb else {
                    return nil
                }
                return invoke
            }
        }

        func writes(_ key: String, _ property: String) -> [LinkMessage.PropertyEntry] {
            messages.compactMap { message in
                guard case .propertyWrite(let write) = message, write.key == key else {
                    return nil
                }
                return write.properties.first { $0.name == property }
            }
        }
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

    private func sliceObject(_ index: Int64, hz: Double, active: Bool) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(hz)),
            .init(ordinal: 2, name: "dspMode", value: .enumeration(0)),
            .init(ordinal: 3, name: "filterLow", value: .i64(-3000)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
            .init(ordinal: 11, name: "active", value: .bool(active)),
            .init(ordinal: 12, name: "txSlice", value: .bool(index == 0)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(index)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            .init(ordinal: 35, name: "locked", value: .bool(false)),
        ]))
    }

    /// The main screen's models on a Core that takes a transmit slice from
    /// this phone, with slice A active and slice B beside it: the board's
    /// sideways pair.
    private func rig() async -> (MainScreenModel, MirrorStore, Sent) {
        let sent = Sent()
        let mirror = MirrorStore(send: { sent.record($0) })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(ordinal: 0, name: BandSlicesModel.remoteTxCapability, value: .i64(1)),
        ])))
        mirror.apply(sliceObject(0, hz: 7_236_400, active: true))
        mirror.apply(sliceObject(1, hz: 7_256_000, active: false))
        let commands = CommandClient(send: { sent.record($0) })
        await commands.handle(.stateChanged(.ready))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: commands,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        return (main, mirror, sent)
    }

    @Test("sideways, B's full flag is not the active one, and each of its buttons acts on slice B")
    func buttonsActOnTheirOwnSlice() async throws {
        let (main, _, sent) = await rig()
        let slices = main.slices
        let controls = main.flagControls
        #expect(await settle { slices.entries.count == 2 && slices.activeSliceId == 0 })
        #expect(slices.canSelectTransmitSlice)
        // The board's pair sideways: both flags full, B not active.
        let placements = FlagLayout.layout(slices: slices.entries.map(\.slice), activeSliceId: slices.activeSliceId,
                                           geometry: Self.geometry(width: 874))
        #expect(placements.allSatisfy { !$0.isFolded })
        let b = try #require(slices.entries.first { $0.id == 1 })

        // TX: the guarded transmit choice for B.
        controls.makeTransmit(1, reason: nil)
        #expect(await settle { !sent.invokes(BandSlicesModel.setTxSliceVerb).isEmpty })
        #expect(sent.invokes(BandSlicesModel.setTxSliceVerb).first?.args == [.init(name: "sliceId", value: .i64(1))])
        // Lock: B's own lock.
        controls.toggleLock(b)
        #expect(await settle { !sent.writes("slice:1", "locked").isEmpty })
        #expect(sent.writes("slice:1", "locked").first?.value == .bool(true))
        #expect(sent.writes("slice:0", "locked").isEmpty)
        // Close: B closes.
        controls.closeSlice(1)
        #expect(await settle { !sent.invokes(BandSlicesModel.removeVerb).isEmpty })
        #expect(sent.invokes(BandSlicesModel.removeVerb).first?.args == [.init(name: "sliceId", value: .i64(1))])
        // Sample rate: the receiver that hears B.
        controls.pickSampleRate(96_000, sliceId: 1)
        #expect(await settle { !sent.invokes(BandSlicesModel.sampleRateVerb).isEmpty })
        #expect(sent.invokes(BandSlicesModel.sampleRateVerb).first?.args == [
            .init(name: "sliceId", value: .i64(1)), .init(name: "rateHz", value: .i64(96_000)),
        ])
        // Step: B's step cycle, one rung up from its step.
        controls.cycleStep(b)
        #expect(await settle { !sent.writes("slice:1", "stepHz").isEmpty })
        #expect(sent.writes("slice:1", "stepHz").first?.value == .i64(Int64(FlagControls.nextStep(after: b.stepHz))))
        #expect(sent.writes("slice:0", "stepHz").isEmpty)
        // Nothing was sent for A.
        #expect(sent.invokes(BandSlicesModel.activateVerb).isEmpty)
    }

    @Test("slice A's close and a listen-only TX say why, and send nothing")
    func greyedButtonsSayWhy() async {
        let (main, _, sent) = await rig()
        let slices = main.slices
        let controls = main.flagControls
        #expect(await settle { slices.entries.count == 2 })
        controls.closeSlice(0)
        #expect(slices.refusal?.text == FlagControls.sliceAStaysOpenText)
        #expect(slices.refusal?.takeOver == true)
        // While this phone may not transmit, TX is greyed with PTT's words.
        let reason = TransmitModel.noRemoteTransmitText
        controls.makeTransmit(1, reason: reason)
        #expect(slices.refusal?.text == reason)
        for _ in 0..<200 {
            await Task.yield()
        }
        #expect(sent.invokes(BandSlicesModel.removeVerb).isEmpty)
        #expect(sent.invokes(BandSlicesModel.setTxSliceVerb).isEmpty)
    }

    @Test("a tab or menu on B's flag makes B active first; the open tab closes it, another tab switches, outside closes")
    func menusAndPanelsOpenAndClose() async throws {
        let (main, mirror, sent) = await rig()
        let slices = main.slices
        let controls = main.flagControls
        #expect(await settle { slices.entries.count == 2 })
        controls.toggle(.panel(1, .mode))
        #expect(controls.open == .panel(1, .mode))
        #expect(controls.openTab(1) == .mode)
        #expect(controls.openTab(0) == nil)
        #expect(await settle { !sent.invokes(BandSlicesModel.activateVerb).isEmpty })
        #expect(sent.invokes(BandSlicesModel.activateVerb).first?.args == [.init(name: "sliceId", value: .i64(1))])
        // The Modes controls wait until the Core makes B active.
        #expect(!controls.showsControls(for: 1))
        mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [.init(ordinal: 11, name: "active", value: .bool(false))])))
        mirror.apply(.delta(LinkMessage.Delta(key: "slice:1", properties: [.init(ordinal: 11, name: "active", value: .bool(true))])))
        #expect(await settle { controls.showsControls(for: 1) })
        // Another tab switches; the open one closes.
        controls.toggle(.panel(1, .xrit))
        #expect(controls.open == .panel(1, .xrit))
        controls.toggle(.panel(1, .xrit))
        #expect(controls.open == nil)
        // The antenna menu, then the more menu: one at a time.
        controls.toggle(.antennas(1))
        #expect(controls.isOpen(.antennas(1)))
        controls.toggle(.more(1))
        #expect(controls.open == .more(1))
        controls.rateListOpen = true
        // A tap outside closes it, and its rates.
        controls.close()
        #expect(controls.open == nil && !controls.rateListOpen)
        // A menu closes the step menu; Diversity closes the menu and opens its sheet.
        controls.openStep(1)
        controls.toggle(.more(1))
        #expect(main.tuning.stepMenuSliceId == nil)
        controls.openDiversity()
        #expect(controls.open == nil && controls.diversityOpen)
    }
}
