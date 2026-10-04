// NereusSDR for iOS: the settings proxy's cache, write-through, echoes, rejects and scope
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
@Suite struct SettingsProxyClientTests {
    private let sent = SentMessages()
    private static let origin = "phone-under-test"

    private func connected(_ snapshot: [String: String] = [:]) -> SettingsProxyClient {
        let proxy = SettingsProxyClient(origin: Self.origin, send: sent.sender)
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(Self.snapshot(snapshot))
        proxy.handle(.stateChanged(.ready))
        return proxy
    }

    private static func snapshot(_ values: [String: String]) -> LinkMessage {
        .settingsSnapshot(LinkMessage.SettingsSnapshot(properties: values.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: .utf8(values[$0]!))
        }))
    }

    private static func value(_ key: String, _ value: String?, origin: String) -> LinkMessage {
        .settingsValue(LinkMessage.SettingsValue(key: key, origin: origin, properties: value.map {
            [LinkMessage.PropertyEntry(name: key, value: .utf8($0))]
        } ?? []))
    }

    private func startWrite(_ proxy: SettingsProxyClient, _ key: String, _ value: String)
        async -> Task<SettingsWriteOutcome, Never> {
        let before = sent.count
        let task = Task { await proxy.write(key, value) }
        #expect(await sent.settle(untilCount: before + 1))
        return task
    }

    @Test("an expired setting restores the latest Core value and then follows Core changes", arguments: [false, true])
    func timeoutRestoresLatestCore(missing: Bool) async {
        let clock = ManualLinkClock()
        let proxy = connected(["CWPitch": "600"], clock: clock)
        var late: [SettingsWriteOutcome] = []
        let task = Task { await proxy.write("CWPitch", "650", onLateOutcome: { late.append($0) }) }
        #expect(await sent.settle(untilCount: 1))
        let latest: String? = missing ? nil : "720"
        proxy.apply(Self.value("CWPitch", latest, origin: "another-phone"))
        await clock.advance(by: 4_999)
        #expect(proxy.value("CWPitch") == "650" && !proxy.isUnconfirmed("CWPitch"))
        await clock.advance(by: 1)
        #expect(await task.value == .notConfirmed)
        #expect(proxy.value("CWPitch") == latest && proxy.isUnconfirmed("CWPitch"))
        proxy.apply(Self.value("CWPitch", "730", origin: "another-phone"))
        #expect(proxy.value("CWPitch") == "730" && proxy.isUnconfirmed("CWPitch"))
        proxy.apply(Self.snapshot(["CWPitch": "740"]))
        #expect(proxy.value("CWPitch") == "740" && proxy.isUnconfirmed("CWPitch"))
        proxy.apply(.settingsReject(.init(key: "CWPitch", properties: [
            .init(name: "CWPitch", value: .utf8("740")),
        ], reason: "Current pitch refusal.")))
        #expect(late == [.rejected(reason: "Current pitch refusal.")])
        #expect(proxy.value("CWPitch") == "740" && !proxy.isUnconfirmed("CWPitch"))
        #expect(sent.count == 1)
    }

    @Test("old setting deadlines and replies cannot restore over a newer adjustment")
    func timeoutCannotRestoreOverNewerTouch() async {
        let clock = ManualLinkClock()
        let proxy = connected(["CWPitch": "600"], clock: clock)
        var oldLate: [SettingsWriteOutcome] = []
        var currentLate: [SettingsWriteOutcome] = []
        let first = Task { await proxy.write("CWPitch", "650", onLateOutcome: { oldLate.append($0) }) }
        #expect(await sent.settle(untilCount: 1))
        await clock.advance(by: 4_000)
        let second = Task { await proxy.write("CWPitch", "700", onLateOutcome: { currentLate.append($0) }) }
        #expect(await sent.settle(untilCount: 2))
        proxy.apply(Self.value("CWPitch", "720", origin: "another-phone"))
        await clock.advance(by: 1_000)
        #expect(await first.value == .notConfirmed)
        #expect(proxy.value("CWPitch") == "700" && !proxy.isUnconfirmed("CWPitch"))
        proxy.apply(.settingsReject(.init(key: "CWPitch", properties: [
            .init(name: "CWPitch", value: .utf8("600")),
        ], reason: "Obsolete pitch refusal.")))
        #expect(proxy.value("CWPitch") == "700" && oldLate.isEmpty && currentLate.isEmpty)
        proxy.apply(Self.value("CWPitch", "730", origin: "another-phone"))
        await clock.advance(by: 4_000)
        #expect(await second.value == .notConfirmed)
        #expect(proxy.value("CWPitch") == "730" && proxy.isUnconfirmed("CWPitch"))
        proxy.apply(Self.value("CWPitch", "700", origin: Self.origin))
        #expect(currentLate == [.accepted] && oldLate.isEmpty)
        #expect(proxy.value("CWPitch") == "700" && !proxy.isUnconfirmed("CWPitch"))
        #expect(sent.count == 2)
    }

    @Test("late older setting outcomes after both deadlines cannot replace the newest Core display", arguments: [false, true])
    func lateOlderOutcomeCannotRestoreExpiredNewerTouch(accepted: Bool) async {
        let clock = ManualLinkClock()
        let proxy = connected(["CWPitch": "600"], clock: clock)
        var oldLate: [SettingsWriteOutcome] = []
        var currentLate: [SettingsWriteOutcome] = []
        let first = Task { await proxy.write("CWPitch", "650", onLateOutcome: { oldLate.append($0) }) }
        #expect(await sent.settle(untilCount: 1))
        await clock.advance(by: 4_000)
        let second = Task { await proxy.write("CWPitch", "700", onLateOutcome: { currentLate.append($0) }) }
        #expect(await sent.settle(untilCount: 2))
        proxy.apply(Self.value("CWPitch", "730", origin: "another-phone"))
        await clock.advance(by: 5_000)
        let firstOutcome = await first.value
        let secondOutcome = await second.value
        #expect(firstOutcome == .notConfirmed && secondOutcome == .notConfirmed)
        #expect(proxy.value("CWPitch") == "730" && proxy.isUnconfirmed("CWPitch"))
        if accepted {
            proxy.apply(Self.value("CWPitch", "650", origin: Self.origin))
        } else {
            proxy.apply(.settingsReject(.init(key: "CWPitch", properties: [
                .init(name: "CWPitch", value: .utf8("600")),
            ], reason: "Obsolete pitch refusal.")))
        }
        #expect(proxy.value("CWPitch") == "730" && proxy.isUnconfirmed("CWPitch"))
        #expect(oldLate.isEmpty && currentLate.isEmpty)
        proxy.apply(Self.value("CWPitch", "740", origin: "another-phone"))
        #expect(proxy.value("CWPitch") == "740")
        proxy.apply(.settingsReject(.init(key: "CWPitch", properties: [
            .init(name: "CWPitch", value: .utf8("740")),
        ], reason: "Current pitch refusal.")))
        #expect(currentLate == [.rejected(reason: "Current pitch refusal.")] && oldLate.isEmpty)
        #expect(proxy.value("CWPitch") == "740" && !proxy.isUnconfirmed("CWPitch"))
        #expect(sent.count == 2)
    }

    @Test("late setting callbacks belong only to the latest write, including retirement",
          arguments: [false, true], [false, true])
    func lateCallbackOwnsNewestWrite(newer: Bool, loseLink: Bool) async {
        let clock = ManualLinkClock()
        let proxy = SettingsProxyClient(origin: Self.origin, send: sent.sender, clock: clock)
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(Self.snapshot(["CWPitch": "600"]))
        proxy.handle(.stateChanged(.ready))
        var old: [SettingsWriteOutcome] = []
        var current: [SettingsWriteOutcome] = []
        let first = Task { await proxy.write("CWPitch", "650", onLateOutcome: { old.append($0) }) }
        #expect(await sent.settle(untilCount: 1))
        await clock.advance(by: 5_000)
        #expect(await first.value == .notConfirmed)
        if newer {
            let second = Task { await proxy.write("CWPitch", "700", onLateOutcome: { current.append($0) }) }
            #expect(await sent.settle(untilCount: 2))
            await clock.advance(by: 5_000)
            #expect(await second.value == .notConfirmed)
        }
        if loseLink {
            proxy.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            #expect(old == (newer ? [] : [.linkLost]))
            #expect(current == (newer ? [.linkLost] : []))
        } else {
            proxy.apply(.settingsReject(LinkMessage.SettingsReject(key: "CWPitch", properties: [],
                                                                   reason: "Old pitch refusal.")))
            #expect(old == (newer ? [] : [.rejected(reason: "Old pitch refusal.")]))
            if newer {
                #expect(proxy.value("CWPitch") == "600")
                proxy.apply(.settingsReject(LinkMessage.SettingsReject(key: "CWPitch", properties: [],
                                                                       reason: "Current pitch refusal.")))
                #expect(current == [.rejected(reason: "Current pitch refusal.")])
            }
        }
        #expect(sent.count == (newer ? 2 : 1), "late outcomes never replay a setting")
    }

    @Test func aWriteUpdatesTheCacheAtOnceAndSettlesOnItsEcho() async {
        let clock = ManualLinkClock()
        let proxy = connected(["DisplaySpectrumFps": "20"], clock: clock)
        #expect(proxy.value("DisplaySpectrumFps") == "20")
        let task = await startWrite(proxy, "DisplaySpectrumFps", "30")
        #expect(proxy.value("DisplaySpectrumFps") == "30")
        #expect(sent.messages.last == .settingsWrite(LinkMessage.SettingsWrite(
            key: "DisplaySpectrumFps", origin: Self.origin,
            properties: [LinkMessage.PropertyEntry(name: "DisplaySpectrumFps", value: .utf8("30"))])))
        proxy.apply(Self.value("DisplaySpectrumFps", "30", origin: Self.origin))
        #expect(await task.value == .accepted)
        #expect(proxy.value("DisplaySpectrumFps") == "30")
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func anEarlierEchoDoesNotUndoALaterWrite() async {
        let clock = ManualLinkClock()
        let (handoffs, handedOff) = AsyncStream<LinkMessage>.makeStream(bufferingPolicy: .unbounded)
        let proxy = SettingsProxyClient(origin: Self.origin, send: { [sent] message in
            try await sent.sender(message)
            handedOff.yield(message)
        }, clock: clock)
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(Self.snapshot(["CWPitch": "600"]))
        proxy.handle(.stateChanged(.ready))
        var writes = handoffs.makeAsyncIterator()
        let first = Task { await proxy.write("CWPitch", "650") }
        let firstSent = await writes.next()
        #expect(firstSent == .settingsWrite(.init(key: "CWPitch", origin: Self.origin,
                                                properties: [.init(name: "CWPitch", value: .utf8("650"))])))
        #expect(clock.pendingDueTimes == [5_000])
        let second = Task { await proxy.write("CWPitch", "700") }
        let secondSent = await writes.next()
        #expect(secondSent == .settingsWrite(.init(key: "CWPitch", origin: Self.origin,
                                                 properties: [.init(name: "CWPitch", value: .utf8("700"))])))
        proxy.apply(Self.value("CWPitch", "650", origin: Self.origin))
        let firstOutcome = await first.value
        #expect(firstOutcome == .accepted)
        #expect(proxy.value("CWPitch") == "700")
        proxy.apply(Self.value("CWPitch", "700", origin: Self.origin))
        #expect(await second.value == .accepted)
        #expect(proxy.value("CWPitch") == "700")
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func aRejectPutsBackTheValueItCarries() async {
        let clock = ManualLinkClock()
        let proxy = connected(["SwrProtectionLimit": "2.0"], clock: clock)
        let task = await startWrite(proxy, "SwrProtectionLimit", "9.0")
        #expect(proxy.value("SwrProtectionLimit") == "9.0")
        let reason = "Choose an SWR protection limit from 1.0 to 5.0."
        proxy.apply(.settingsReject(LinkMessage.SettingsReject(
            key: "SwrProtectionLimit",
            properties: [LinkMessage.PropertyEntry(name: "SwrProtectionLimit", value: .utf8("2.0"))],
            reason: reason)))
        #expect(await task.value == .rejected(reason: reason))
        #expect(proxy.value("SwrProtectionLimit") == "2.0")
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func aWriteHeldForThisPhonesQuestionKeepsTheTouchedValue() async {
        // Held for a question is not a refusal (StationClient.cpp:7308-7317).
        let clock = ManualLinkClock()
        let proxy = connected(["SwrProtectionLimit": "2.0"], clock: clock)
        let task = await startWrite(proxy, "SwrProtectionLimit", "3.0")
        proxy.apply(.settingsReject(LinkMessage.SettingsReject(
            key: "SwrProtectionLimit",
            properties: [LinkMessage.PropertyEntry(name: "SwrProtectionLimit", value: .utf8("2.0"))],
            reason: SeveralDevices.waitingReason)))
        #expect(await task.value == .rejected(reason: SeveralDevices.waitingReason))
        #expect(proxy.value("SwrProtectionLimit") == "3.0")
        // The Core's next value of the key settles it.
        proxy.apply(Self.value("SwrProtectionLimit", "2.0", origin: "another-device"))
        #expect(proxy.value("SwrProtectionLimit") == "2.0")
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func aRejectWithNoEntryLeavesTheKeyWithNoValue() async {
        // A key the Core owns by code is station-scoped, so it is sent.
        let clock = ManualLinkClock()
        let proxy = connected([:], clock: clock)
        let task = await startWrite(proxy, "NotchCount", "3")
        proxy.apply(.settingsReject(LinkMessage.SettingsReject(key: "NotchCount", properties: [],
                                                               reason: "This Core keeps its own notch list.")))
        #expect(await task.value == .rejected(reason: "This Core keeps its own notch list."))
        #expect(proxy.value("NotchCount") == nil)

        // An operator-local key comes back with no entry.
        proxy.apply(.settingsReject(LinkMessage.SettingsReject(
            key: "ExtendedTxAllowed", properties: [], reason: "Each app keeps this setting itself; the Core does not store it.")))
        #expect(proxy.value("ExtendedTxAllowed") == nil)
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func aKeyEachAppKeepsItselfIsNeverSent() async {
        let proxy = connected()
        for key in ["ExtendedTxAllowed", "TciLogWindowGeometry", "radios/abc/name", "SomethingNew"] {
            #expect(await proxy.write(key, "True") == .keptOnThisDevice, "\(key)")
            await proxy.remove(key)
            #expect(proxy.value(key) == nil)
        }
        #expect(sent.count == 0)
    }

    @Test func disableHfPaAndThePerBandGridRangeAreTheCores() {
        // Disable HF PA (transmitSettingsVersion 11) and Grid & Scales' per-band dB range (parity ruling C12).
        for key in ["DisableHfPa", "DisplayGridMax_20m", "DisplayGridMin_GEN", "DisplayGridMax_2m"] {
            #expect(SettingsScope.of(key) == .station, "\(key)")
        }
        // The phone's own grid keys without a band stay its own.
        #expect(SettingsScope.of("DisplayGridMax") != .station)
    }

    @Test func newlyStationOwnedSettingsAreProxiedWhileReporterDisplayChoicesStayLocal() async {
        let clock = ManualLinkClock()
        let proxy = connected([:], clock: clock)
        for key in ["FreeDvReporter/Hidden", "ModMon/FbStream"] {
            let write = await startWrite(proxy, key, "True")
            #expect(sent.messages.last == .settingsWrite(LinkMessage.SettingsWrite(
                key: key, origin: Self.origin,
                properties: [LinkMessage.PropertyEntry(name: key, value: .utf8("True"))])))
            proxy.apply(Self.value(key, "True", origin: Self.origin))
            #expect(await write.value == .accepted)
        }
        let sentBeforeLocal = sent.count
        #expect(await proxy.write("FreeDvReporter/ColumnWidths", "100,120") == .keptOnThisDevice)
        #expect(sent.count == sentBeforeLocal)
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func anotherDevicesChangeUpdatesTheCache() {
        let proxy = connected(["StationCallsign": "KG4VCF"])
        proxy.apply(Self.value("StationCallsign", "W1AW", origin: "another-phone"))
        #expect(proxy.value("StationCallsign") == "W1AW")
        // The Core's own change carries an empty origin.
        proxy.apply(Self.value("StationCallsign", "KG4VCF", origin: ""))
        #expect(proxy.value("StationCallsign") == "KG4VCF")
        proxy.apply(Self.value("StationCallsign", nil, origin: ""))
        #expect(proxy.value("StationCallsign") == nil)
    }

    @Test func theAppsOwnRemovalComesBackWithAnEmptyOrigin() async {
        let clock = ManualLinkClock()
        let proxy = connected(["DisplaySpectrumFps": "30", "CWPitch": "600"], clock: clock)
        await proxy.remove("DisplaySpectrumFps")
        #expect(proxy.value("DisplaySpectrumFps") == nil)
        #expect(sent.messages.last == .settingsRemove(LinkMessage.SettingsRemove(key: "DisplaySpectrumFps")))
        // Removal echoes carry origin "", the app's own included.
        proxy.apply(Self.value("DisplaySpectrumFps", nil, origin: ""))
        #expect(proxy.value("DisplaySpectrumFps") == nil)

        // A removal, then a write before the removal's echo: the write stays.
        await proxy.remove("CWPitch")
        let write = await startWrite(proxy, "CWPitch", "700")
        proxy.apply(Self.value("CWPitch", nil, origin: ""))
        #expect(proxy.value("CWPitch") == "700")
        proxy.apply(Self.value("CWPitch", "700", origin: Self.origin))
        #expect(await write.value == .accepted)
    }

    @Test func aLateRadiosSnapshotMergesAndANewSessionsReplaces() {
        let proxy = connected(["CWPitch": "600", "DisplaySpectrumFps": "30"])
        // Capabilities, then settings: a radio that arrived late.
        proxy.apply(FixtureReplay.capabilities(["radioConnected": .bool(true)]))
        proxy.apply(Self.snapshot(["hardware/AA:BB:CC:DD:EE:01/radioInfo/sampleRate": "192000", "CWPitch": "650"]))
        #expect(proxy.values == ["CWPitch": "650", "DisplaySpectrumFps": "30",
                                 "hardware/AA:BB:CC:DD:EE:01/radioInfo/sampleRate": "192000"])

        // A lost link keeps the cache readable; the next session's snapshot replaces it.
        proxy.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(proxy.value("CWPitch") == "650")
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(Self.snapshot(["CWPitch": "700"]))
        #expect(proxy.values == ["CWPitch": "700"])
    }

    @Test func aWriteThatCannotBeSentOrLosesItsLinkIsReported() async {
        let proxy = connected(["CWPitch": "600"], clock: ManualLinkClock())
        sent.refuseAll()
        #expect(await proxy.write("CWPitch", "650") == .notSent)
        #expect(proxy.value("CWPitch") == "600")
        sent.refuseAll(false)

        let task = await startWrite(proxy, "CWPitch", "650")
        proxy.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await task.value == .linkLost)
    }

    // MARK: Held until the Core answers (JJ, 2026-10-01; StationClient.cpp:1040-1068)

    private func connected(_ snapshot: [String: String], clock: ManualLinkClock) -> SettingsProxyClient {
        let proxy = SettingsProxyClient(origin: Self.origin, send: sent.sender, clock: clock)
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(Self.snapshot(snapshot))
        proxy.handle(.stateChanged(.ready))
        return proxy
    }

    @Test func anotherDevicesChangeWhileAWriteWaitsDoesNotUndoIt() async {
        let clock = ManualLinkClock()
        let proxy = connected(["CWPitch": "600", "DisplaySpectrumFps": "20"], clock: clock)
        let task = await startWrite(proxy, "CWPitch", "650")
        proxy.apply(Self.value("CWPitch", "700", origin: "another-phone"))
        #expect(proxy.value("CWPitch") == "650")
        // Another key's change shows as it comes.
        proxy.apply(Self.value("DisplaySpectrumFps", "30", origin: "another-phone"))
        #expect(proxy.value("DisplaySpectrumFps") == "30")
        // A late radio's snapshot does not undo it either.
        proxy.apply(Self.snapshot(["CWPitch": "710"]))
        #expect(proxy.value("CWPitch") == "650")
        proxy.apply(Self.value("CWPitch", "650", origin: Self.origin))
        #expect(await task.value == .accepted)
        #expect(proxy.value("CWPitch") == "650")
        // After the answer, another device's change shows as it comes.
        proxy.apply(Self.value("CWPitch", "720", origin: "another-phone"))
        #expect(proxy.value("CWPitch") == "720")
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func aLostLinkShowsTheCoresValueAndNothingIsSentAgain() async {
        let clock = ManualLinkClock()
        let proxy = connected(["CWPitch": "600"], clock: clock)
        let task = await startWrite(proxy, "CWPitch", "650")
        proxy.apply(Self.value("CWPitch", "700", origin: "another-phone"))
        let before = sent.count
        proxy.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await task.value == .linkLost)
        #expect(proxy.value("CWPitch") == "700")
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(Self.snapshot(["CWPitch": "700"]))
        #expect(sent.count == before)
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func noAnswerInFiveSecondsRestoresCoreMarkedNotConfirmed() async {
        let clock = ManualLinkClock()
        let proxy = connected(["CWPitch": "600"], clock: clock)
        let task = await startWrite(proxy, "CWPitch", "650")
        await clock.advance(by: 4_999)
        #expect(!proxy.isUnconfirmed("CWPitch"))
        await clock.advance(by: 1)
        #expect(await task.value == .notConfirmed)
        #expect(proxy.value("CWPitch") == "600")
        #expect(proxy.isUnconfirmed("CWPitch"))
        await clock.advance(by: 60_000)
        #expect(proxy.value("CWPitch") == "600")
        // The echo comes late: the mark goes.
        proxy.apply(Self.value("CWPitch", "650", origin: Self.origin))
        #expect(!proxy.isUnconfirmed("CWPitch"))
        #expect(proxy.value("CWPitch") == "650")
    }

    // MARK: Scope

    @Test func theScopeFollowsTheLinkDocumentsRules() {
        #expect(SettingsScope.of("DisplaySpectrumFps") == .station)
        #expect(SettingsScope.of("DisplaySpectrumFps_2") == .station)
        #expect(SettingsScope.of("RxOnly") == .station)
        #expect(SettingsScope.of("TciServerPort") == .operatorLocal)
        #expect(SettingsScope.of("TciLogWindowGeometry") == .operatorLocal)
        #expect(SettingsScope.of("StationTci_Port") == .station)
        #expect(SettingsScope.of("FreeDvReporter/Hidden") == .station)
        #expect(SettingsScope.of("ModMon/FbStream") == .station)
        #expect(SettingsScope.of("FreeDvReporter/Callsign") == .station)
        #expect(SettingsScope.of("hardware/AA:BB/radioInfo/sampleRate") == .station)
        #expect(SettingsScope.of("Unlisted") == .operatorLocal)
        // Keys the Core owns by code, matched without regard to case.
        #expect(SettingsScope.of("dspAssets/nnr/0") == .station)
        #expect(SettingsScope.of("DSPASSETS/x") == .station)
        #expect(SettingsScope.of("nr3modelpath") == .station)
        #expect(SettingsScope.of("notch12center") == .station)
        #expect(SettingsScope.of("notchcount") == .station)
        #expect(SettingsScope.of("Notch012Center") == .station)  // Still the Notch prefix.
        #expect(SettingsScope.ownedByCoreCode("Notch012Center") == false)
        #expect(SettingsScope.ownedByCoreCode("HARDWARE/aa/options/stepAtt/value") == true)
        #expect(SettingsScope.ownedByCoreCode("hardware/aa/alex/antenna/tx/1") == true)
        #expect(SettingsScope.ownedByCoreCode("hardware/aa/puresignal/x") == true)
        #expect(SettingsScope.ownedByCoreCode("hardware/aa/slices/0/nnr/x") == true)
        #expect(SettingsScope.ownedByCoreCode("hardware/aa/slices/0/agc") == false)
    }

    /// The app's tables are the link's: every rule of `surface.json`'s
    /// `settingsScope`, in its tier, with its scope.
    @Test func theScopeTablesMatchTheLinksSurface() throws {
        let surface = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        let section = try #require(surface["settingsScope"] as? [String: Any])
        let exceptions = try #require(section["exceptions"] as? [[String: String]])
        var surfaceExceptions: [String: SettingsScope] = [:]
        for rule in exceptions {
            surfaceExceptions[rule["key"] ?? ""] = rule["scope"] == "station" ? .station : .operatorLocal
        }
        func list(_ name: String, _ scope: SettingsScope) throws -> [String: SettingsScope] {
            let texts = try #require(section[name] as? [String])
            return Dictionary(uniqueKeysWithValues: texts.map { ($0, scope) })
        }
        let surfacePrefixes = try list("stationPrefixes", .station)
            .merging(try list("operatorLocalPrefixes", .operatorLocal)) { first, _ in first }
        let surfaceKeys = try list("stationKeys", .station)
            .merging(try list("operatorLocalKeys", .operatorLocal)) { first, _ in first }
        func mine(_ rules: [SettingsScope.Rule]) -> [String: SettingsScope] {
            Dictionary(uniqueKeysWithValues: rules.map { ($0.text, $0.scope) })
        }
        #expect(!surfaceExceptions.isEmpty)
        #expect(mine(SettingsScope.exceptions) == surfaceExceptions)
        #expect(mine(SettingsScope.prefixes) == surfacePrefixes)
        #expect(mine(SettingsScope.wholeKeys) == surfaceKeys)
    }
}
