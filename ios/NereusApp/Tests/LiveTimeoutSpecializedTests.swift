// NereusSDR for iOS: ordinary specialized controls restore only the responsible submitted entry at its confirmation deadline
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

@Suite("Specialized live timeout controls", .serialized)
@MainActor
struct LiveTimeoutSpecializedTests {
    private func turns(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() { return true }
            await Task.yield()
        }
        return condition()
    }

    @Test("a current TCI port entry restores at five seconds; newer drafts and replacement owners survive", arguments: [0, 1, 2], [false, true])
    func tciSubmittedEntryTimeout(owner: Int, loseLink: Bool) async throws {
        let clock = TestLinkClock()
        let outbox = SliceListTests.Outbox()
        let mirror = MirrorStore(send: { outbox.record($0) }, clock: clock)
        mirror.apply(.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(.init(properties: [
            .init(name: StationTciClient.capabilityName, value: .i64(StationTciOptions.version)),
        ])))
        mirror.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
        mirror.apply(.objectCreate(.init(key: StationTciClient.objectKey, className: FakeStation.tciClass, properties: [
            .init(ordinal: 0, name: "enabled", value: .bool(true)),
            .init(ordinal: 1, name: "port", value: .i64(40001)),
            .init(ordinal: 2, name: "listening", value: .bool(true)),
            .init(ordinal: 3, name: "stationAddress", value: .utf8("")),
            .init(ordinal: 4, name: "error", value: .utf8("")),
        ])))
        mirror.apply(.snapshotComplete)
        let commands = CommandClient(clock: clock, send: { outbox.record($0) })
        await commands.handle(.stateChanged(.ready))
        let model = TciServerModel(mirror: mirror, commands: commands, records: nil)
        #expect(await turns { model.reason == nil && model.port == 40001 })
        model.openPortPad()
        let submitted = try #require(model.pad)
        func type(_ pad: ValuePadModel, _ value: String) {
            while !pad.entry.isEmpty { pad.press(.delete) }
            for digit in value { pad.press(.digit(digit.wholeNumberValue!)) }
        }
        type(submitted, "50001")
        let entered = Task { await submitted.enter() }
        #expect(await turns { outbox.invokes(StationTciClient.setVerb).count == 1 })
        let invoke = try #require(outbox.invokes(StationTciClient.setVerb).first)
        #expect(invoke.args.first { $0.name == "port" }?.value == .i64(50001))
        mirror.apply(.delta(.init(key: StationTciClient.objectKey, properties: [.init(ordinal: 1, name: "port", value: .i64(40002))])))
        #expect(await turns { model.port == 40002 })
        if owner == 1 { type(submitted, "50002") }
        if owner == 2 { model.openPortPad(); type(try #require(model.pad), "50002") }
        let current = try #require(model.pad)
        let entry = current.entry
        if loseLink {
            await commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        } else {
            await clock.advance(by: 4_999)
            #expect(await commands.waitingCount == 1)
            #expect(submitted.sending == (owner != 2) && current.refusal == nil)
            await clock.advance(by: 1)
        }
        #expect(await turns { !submitted.sending })
        let hint = loseLink ? PropertyWriteOutcome.linkLost.reason : PropertyWriteOutcome.notConfirmed.reason
        #expect(model.pad === current && model.port == 40002)
        #expect(current.value == (owner == 0 ? 40002 : 50002))
        #expect(current.refusal == (owner == 0 ? hint : nil))
        #expect(model.note == (owner == 0 ? hint : nil))
        if owner != 0 { #expect(current.entry == entry) }
        // If a regression restores the prior ten-second behavior, retire
        // that waiter only after recording the exact five-second failures.
        if submitted.sending { await clock.advance(by: 5_000) }
        #expect(await entered.value == false)
        let refusal = "This TCI port is already in use."
        await commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: false,
                                                     reason: refusal, affected: [], values: nil)))
        #expect(await turns { owner != 0 || loseLink || current.refusal == refusal })
        #expect(current.value == (owner == 0 ? 40002 : 50002))
        #expect(current.refusal == (owner == 0 ? (loseLink ? hint : refusal) : nil))
        #expect(model.note == (owner == 0 ? (loseLink ? hint : refusal) : nil))
        #expect(model.pad === current && outbox.invokes(StationTciClient.setVerb).count == 1)
        await commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
    }
}
