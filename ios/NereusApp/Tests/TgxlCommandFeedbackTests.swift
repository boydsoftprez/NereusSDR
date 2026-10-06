// NereusSDR for iOS: tuner command feedback against the real command client and a clock moved by the test
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-19: command feedback belongs to the latest device action and
/// snapshot. Command acceptance never stands in for the tuner's report.
@Suite("TGXL command feedback", .serialized)
@MainActor
struct TgxlCommandFeedbackTests {
    @MainActor final class Recorded {
        var commands: [LinkMessage.CommandInvoke] = []
    }

    @MainActor struct Rig {
        let base: DisplaySheetTests.Rig
        let clock: TestLinkClock
        let recorded: Recorded
        let commands: CommandClient
        let accessories: AccessoriesModel

        init(failSend: Bool = false) async throws {
            let clock = TestLinkClock()
            let base = try DisplaySheetTests.Rig(clock: clock)
            let recorded = Recorded()
            let commands = CommandClient(clock: clock, send: { message in
                if failSend { throw LinkSendError.notConnected }
                if case .commandInvoke(let command) = message {
                    await MainActor.run { recorded.commands.append(command) }
                }
            })
            await commands.handle(.stateChanged(.ready))
            base.mirror.handle(.stateChanged(.ready))
            base.mirror.apply(.objectCreate(.init(key: "tuner", className: "TunerModel", properties: [
                .init(name: "connectionPhase", value: .enumeration(6)),
                .init(name: "isPresent", value: .bool(true)),
                .init(name: "isOperate", value: .bool(false)),
                .init(name: "isBypass", value: .bool(true)),
            ])))
            let accessories = AccessoriesModel(mirror: base.mirror, commands: commands,
                                               slices: base.main.slices, catalogFeed: base.main.catalogFeed,
                                               settings: base.settings)
            accessories.refresh()
            self.base = base
            self.clock = clock
            self.recorded = recorded
            self.commands = commands
            self.accessories = accessories
        }

        func answer(_ command: LinkMessage.CommandInvoke, accepted: Bool, reason: String = "",
                    values: [LinkMessage.PropertyEntry]? = nil) async {
            await commands.receive(.commandResult(.init(verb: command.verb, id: command.id,
                                                        accepted: accepted, reason: reason,
                                                        affected: [], values: values)))
        }
    }

    /// Same native renderer/environment as the existing app picture tests; actual theme is recorded beside each PNG.
    static func saveFeedbackShot(_ name: String, window: UIWindow) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty else { return }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        let data = try #require(image.pngData())
        let base = URL(fileURLWithPath: directory)
        try data.write(to: base.appendingPathComponent(name + ".png"))
        let theme = window.traitCollection.userInterfaceStyle == .dark ? "dark" : "light"
        let metadata: [String: Any] = ["name": name, "actualTheme": theme,
                                      "width": window.bounds.width, "height": window.bounds.height]
        try JSONSerialization.data(withJSONObject: metadata, options: [.sortedKeys, .prettyPrinted])
            .write(to: base.appendingPathComponent(name + ".json"))
        print("Wrote TGXL feedback picture \(name), actual theme \(theme)")
    }

    private func turns(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() { return true }
            await Task.yield()
        }
        return condition()
    }

    private func start(_ rig: Rig, verb: String = "setTgxlBypass") async throws
        -> (Task<Bool, Never>, LinkMessage.CommandInvoke) {
        let before = rig.recorded.commands.count
        let task = Task { await rig.accessories.invoke(.tunerGenius, verb,
                                                       [.init(name: "on", value: .bool(false))]) }
        #expect(await turns { rig.recorded.commands.count == before + 1 })
        return (task, try #require(rig.recorded.commands.last))
    }

    @Test("timeout is visible at the unchanged five-second boundary and the report stays authoritative",
          arguments: ["setTgxlBypass", "setTgxlOperate"])
    func timeout(verb: String) async throws {
        let rig = try await Rig()
        let before = try #require(rig.accessories.tunerGenius)
        let (task, command) = try await start(rig, verb: verb)
        #expect(command.args == [.init(name: "on", value: .bool(false))])
        #expect(rig.clock.pendingDueTimes.contains(5_000))
        await rig.clock.advance(by: 4_999)
        #expect(rig.accessories.notes[.tunerGenius] == nil)
        await rig.clock.advance(by: 1)
        #expect(await task.value == false)
        #expect(rig.accessories.notes[.tunerGenius] == PropertyWriteOutcome.notConfirmed.reason)
        #expect(rig.accessories.tunerGenius == before)
        #expect(rig.recorded.commands.count == 1)
    }

    @Test("the public Bypass and Operate actions show the command timeout", arguments: [false, true])
    func publicActions(operate: Bool) async throws {
        let rig = try await Rig()
        rig.base.mirror.apply(.capabilities(.init(properties: [
            .init(name: "remoteTgxlControlVersion", value: .i64(3)),
        ])))
        rig.accessories.refresh()
        let before = rig.accessories.tunerGenius
        if operate { rig.accessories.setTunerOperate(false) }
        else { rig.accessories.setTunerBypass(false) }
        #expect(await turns { rig.recorded.commands.count == 1 })
        #expect(rig.recorded.commands.first?.verb == (operate ? "setTgxlOperate" : "setTgxlBypass"))
        await rig.clock.advance(by: 5_000)
        #expect(await turns { rig.accessories.notes[.tunerGenius] == PropertyWriteOutcome.notConfirmed.reason })
        #expect(rig.accessories.tunerGenius == before)
        #expect(rig.recorded.commands.count == 1)
    }

    @Test("link loss reports the established reason without changing the tuner")
    func linkLoss() async throws {
        let rig = try await Rig()
        let before = rig.accessories.tunerGenius
        let (task, _) = try await start(rig)
        rig.base.mirror.handle(.stateChanged(.idle))
        await rig.commands.handle(.stateChanged(.idle))
        #expect(await task.value == false)
        #expect(rig.accessories.notes[.tunerGenius] == PropertyWriteOutcome.linkLost.reason)
        #expect(rig.accessories.tunerGenius == before)
    }

    @Test("no session, missing command client, and failed send report not sent", arguments: [0, 1, 2])
    func notSent(mode: Int) async throws {
        let rig = try await Rig(failSend: mode == 2)
        if mode == 1 {
            #expect(await rig.base.main.accessories.invoke(.tunerGenius, "setTgxlBypass", []) == false)
            #expect(rig.base.main.accessories.notes[.tunerGenius] == PropertyWriteOutcome.notSent.reason)
        } else {
            if mode == 0 { await rig.commands.handle(.stateChanged(.idle)) }
            #expect(await rig.accessories.invoke(.tunerGenius, "setTgxlBypass", []) == false)
            #expect(rig.accessories.notes[.tunerGenius] == PropertyWriteOutcome.notSent.reason)
        }
        #expect(rig.recorded.commands.isEmpty)
    }

    @Test("Core refusal uses its words or the established fallback", arguments: [false, true])
    func refusal(empty: Bool) async throws {
        let rig = try await Rig()
        let before = rig.accessories.tunerGenius
        let (task, command) = try await start(rig)
        let words = empty ? "" : AccessoriesModel.onAirReason
        await rig.answer(command, accepted: false, reason: words)
        #expect(await task.value == false)
        #expect(rig.accessories.notes[.tunerGenius] == (empty ? BandSlicesModel.refusedText : words))
        #expect(rig.accessories.tunerGenius == before)
    }

    @Test("a held question preserves the visible note and does not claim acceptance")
    func heldQuestion() async throws {
        let rig = try await Rig()
        let words = PropertyWriteOutcome.notConfirmed.reason
        rig.accessories.notes[.tunerGenius] = words
        let (task, command) = try await start(rig)
        await rig.answer(command, accepted: false, reason: SeveralDevices.waitingReason)
        #expect(await task.value == false)
        #expect(rig.accessories.notes[.tunerGenius] == words)
    }

    @Test("an older success or failure cannot replace the newer failure", arguments: [false, true])
    func latestAttempt(olderAccepted: Bool) async throws {
        let rig = try await Rig()
        let (older, first) = try await start(rig)
        let (newer, second) = try await start(rig, verb: "setTgxlOperate")
        await rig.answer(second, accepted: false, reason: AccessoriesModel.tunerNotConnectedReason)
        #expect(await newer.value == false)
        await rig.answer(first, accepted: olderAccepted, reason: AccessoriesModel.onAirReason)
        _ = await older.value
        #expect(rig.accessories.notes[.tunerGenius] == AccessoriesModel.tunerNotConnectedReason)
    }

    @Test("an older timeout cannot replace a newer Core refusal")
    func latestTimeout() async throws {
        let rig = try await Rig()
        let (older, _) = try await start(rig)
        let (newer, second) = try await start(rig, verb: "setTgxlOperate")
        await rig.answer(second, accepted: false, reason: AccessoriesModel.tunerNotConnectedReason)
        #expect(await newer.value == false)
        await rig.clock.advance(by: 5_000)
        #expect(await older.value == false)
        #expect(rig.accessories.notes[.tunerGenius] == AccessoriesModel.tunerNotConnectedReason)
    }

    @Test("an old snapshot answer cannot replace the fresh snapshot note", arguments: [false, true])
    func oldSnapshot(olderAccepted: Bool) async throws {
        let rig = try await Rig()
        let (older, first) = try await start(rig)
        let previous = rig.base.mirror.snapshotIdentity
        rig.base.mirror.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
        rig.base.mirror.apply(.snapshotComplete)
        #expect(rig.base.mirror.snapshotIdentity != previous)
        // No new command owner: the snapshot fence itself must reject the old answer.
        rig.accessories.notes[.tunerGenius] = PropertyWriteOutcome.notSent.reason
        await rig.answer(first, accepted: olderAccepted, reason: AccessoriesModel.onAirReason)
        #expect(await older.value == false)
        #expect(rig.accessories.notes[.tunerGenius] == PropertyWriteOutcome.notSent.reason)
    }

    @Test("a Core answer cannot update a stale snapshot")
    func staleSnapshot() async throws {
        let rig = try await Rig()
        let (task, command) = try await start(rig)
        rig.base.mirror.handle(.stateChanged(.idle))
        await rig.answer(command, accepted: false, reason: AccessoriesModel.onAirReason)
        #expect(await task.value == false)
        #expect(rig.accessories.notes[.tunerGenius] == nil)
    }

    @Test("blocked actions and dismissal retire the pending note owner", arguments: [false, true])
    func localNoteOwner(dismiss: Bool) async throws {
        let rig = try await Rig()
        let (task, command) = try await start(rig)
        if dismiss { rig.accessories.dismissNote(.tunerGenius) }
        else { #expect(!rig.accessories.allowed(.tunerGenius, AccessoriesModel.onAirReason)) }
        await rig.answer(command, accepted: true)
        _ = await task.value
        #expect(rig.accessories.notes[.tunerGenius] == (dismiss ? nil : AccessoriesModel.onAirReason))
    }

    @Test("an accepted command clears its failure but follows only the reported tuner state")
    func authoritativeReport() async throws {
        let rig = try await Rig()
        let before = try #require(rig.accessories.tunerGenius)
        rig.accessories.notes[.tunerGenius] = PropertyWriteOutcome.notConfirmed.reason
        let (task, command) = try await start(rig, verb: "setTgxlOperate")
        await rig.answer(command, accepted: true)
        #expect(await task.value)
        #expect(rig.accessories.notes[.tunerGenius] == nil)
        #expect(rig.accessories.tunerGenius == before)
        #expect(TunerGeniusPage.buttonLabel(before) == "BYPASS")
        rig.base.mirror.apply(.delta(.init(key: "tuner", properties: [
            .init(name: "isOperate", value: .bool(true)), .init(name: "isBypass", value: .bool(false)),
        ])))
        rig.accessories.refresh()
        #expect(TunerGeniusPage.buttonLabel(try #require(rig.accessories.tunerGenius)) == "OPERATE")
    }

    @Test("timeout feedback is rendered on the TGXL page", arguments: [false, true])
    func pageFeedback(dark: Bool) async throws {
        let rig = try await Rig()
        let (task, _) = try await start(rig)
        await rig.clock.advance(by: 5_000)
        _ = await task.value
        let window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 874))
        window.overrideUserInterfaceStyle = dark ? .dark : .light
        let host = UIHostingController(rootView: ScrollView {
            TunerGeniusPage(model: rig.accessories, transmit: rig.base.main.transmit, open: { _ in })
        }.preferredColorScheme(dark ? .dark : .light))
        let wasOn = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        host.view.frame = window.bounds
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
            SetupTypedEntryTests.setApplicationAccessibility(wasOn)
        }
        await ShotWait.laidOut(window)
        try Self.saveFeedbackShot("tgxl-command-timeout-\(dark ? "dark" : "light")", window: window)
        #expect(!AccessoryPagesTests.elements(labelled: PropertyWriteOutcome.notConfirmed.reason, in: window).isEmpty)
    }
}
