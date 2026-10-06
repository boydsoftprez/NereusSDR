// NereusSDR for iOS: a tuner tune refusal remains visible on the page where the tune was requested
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@Suite("TGXL tune feedback", .serialized)
@MainActor
struct TgxlTuneFeedbackTests {
    private func turns(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() { return true }
            await Task.yield()
        }
        return condition()
    }

    @Test("the real tuner tune refusal is visible on its page and dismissal sends no command",
          arguments: [false, true])
    func refusedTunePage(dark: Bool) async throws {
        let rig = try await TgxlCommandFeedbackTests.Rig()
        let platform = TestPlatform()
        rig.base.mirror.apply(.capabilities(.init(properties: [
            .init(name: "remoteTxVersion", value: .i64(2)),
            .init(name: "txPermitted", value: .bool(true)),
        ])))
        let controller = PttController(commands: TransmitCommandClient(commands: rig.commands), clock: rig.clock)
        let transmit = TransmitModel(mirror: rig.base.mirror, commands: rig.commands,
                                     slices: rig.base.main.slices, subscriber: nil,
                                     controller: controller, platform: platform.platform)
        transmit.refresh()
        await transmit.sessionChanged(.ready, owner: 1)
        #expect(transmit.tunerTuneReason == nil)
        let window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 874))
        window.overrideUserInterfaceStyle = dark ? .dark : .light
        let host = UIHostingController(rootView: ScrollView {
            TunerGeniusPage(model: rig.accessories, transmit: transmit, open: { _ in })
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
        transmit.toggleTunerTune()
        #expect(await turns { rig.recorded.commands.count == 3 })
        let command = try #require(rig.recorded.commands.first)
        #expect(rig.recorded.commands.allSatisfy { $0.verb == "tx.tunerTune" && $0.id == command.id
            && $0.args == [.init(name: "on", value: .bool(true))] })
        let reason = AccessoriesModel.onAirReason
        await rig.answer(command, accepted: false, reason: reason,
                         values: [.init(name: "refusalCode", value: .utf8("holderOnAir")),
                                  .init(name: "refusalFix", value: .utf8(""))])
        #expect(await turns { if case .refused = transmit.ptt.state { return true }; return false })
        #expect(!transmit.ptt.transmitting && !transmit.ptt.keepaliveRunning)
        #expect(transmit.tunerTuneReason == nil, "permission does not describe the asynchronous refusal")
        await ShotWait.laidOut(window)
        try TgxlCommandFeedbackTests.saveFeedbackShot("tgxl-tune-refusal-\(dark ? "dark" : "light")", window: window)
        #expect(!AccessoryPagesTests.elements(labelled: reason, in: window).isEmpty,
                "the page that sent the tune must show the Core's refusal")
        let before = rig.recorded.commands
        transmit.dismissNotice()
        #expect(await turns { transmit.ptt.state == .idle })
        await ShotWait.laidOut(window)
        #expect(AccessoryPagesTests.elements(labelled: reason, in: window).isEmpty)
        #expect(rig.recorded.commands == before)
        await transmit.sessionChanged(.idle, owner: 1)
    }
}
