// NereusSDR for iOS: coordinated Diversity consumers, touch admission and listening badge regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusKitTesting
import NereusMirror
@testable import NereusSDR
import Testing

/// Added after the existing-model causal RED was observed.
/// Real models read exact complete synthetic radio summaries, not fabricated badge entries.
@Suite("Approved Diversity consumers", .serialized)
@MainActor
struct DiversityApprovedTests {
    @Test("a listening flag carries DIV only for the exact live slice, including a resource pause")
    func listeningBadgeAndPausedOwner() throws {
        let store = Self.store(sourceOwner: "desktop")
        let slices = BandSlicesModel(store: store, commands: nil)
        slices.thisDeviceId = "phone"
        #expect(slices.entries.first { $0.id == 1 }?.listening == true)
        #expect(slices.entries.filter(\.diversityOn).map(\.id) == [1])
        Self.publish(store, Self.summary(owner: "desktop", paused: true))
        // Radio summary is authoritative while per-slice enable values are unchanged.
        let paused = BandSlicesModel(store: store, commands: nil)
        paused.thisDeviceId = "phone"
        #expect(paused.entries.filter(\.diversityOn).map(\.id) == [1])
    }

    @Test("changing source control before queued touch handoff cannot send a Diversity command")
    func staleSourceTouch() async throws {
        try await refusedTouch { store in
            Self.access(store, id: 1, owner: "desktop", revision: 5)
        }
    }

    @Test("changing target control before queued touch handoff cannot send a Diversity command")
    func staleTargetTouch() async throws {
        try await refusedTouch { store in
            Self.access(store, id: 0, owner: "desktop", revision: 5)
        }
    }

    @Test("a newer complete owner revision invalidates an already captured move")
    func staleOwnerTouch() async throws {
        try await refusedTouch { store in
            Self.publish(store, Self.summary(live: 0, revision: 13))
        }
    }

    @Test("a new snapshot invalidates a captured move even with identical restored property values")
    func replacementSnapshotTouch() async throws {
        try await refusedTouch { store in
            store.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
            Self.populate(store)
            store.apply(.snapshotComplete)
        }
    }

    @Test("a listening source is read from the summary and cannot be switched off or moved by this phone")
    func sourceOwnershipRefusal() async throws {
        let outbox = SliceListTests.Outbox()
        let commands = CommandClient(send: { outbox.record($0) })
        await commands.handle(.stateChanged(.ready))
        let store = Self.store(sourceOwner: "desktop")
        let slices = BandSlicesModel(store: store, commands: commands)
        slices.thisDeviceId = "phone"
        let defaults = try #require(UserDefaults(suiteName: "diversity-source-ownership"))
        defer { defaults.removePersistentDomain(forName: "diversity-source-ownership") }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), commands: commands,
                                   slices: slices, captureSender: { { message, permit in
            guard !permit.isRevoked else { throw LinkSendError.notConnected }
            outbox.record(message)
        } })
        #expect(model.sliceKey == "slice:1" && model.phaseDeg == 123.4 && model.gainDb == -2)
        model.setTarget(0, enabled: true)
        model.setEnabled(false)
        #expect(!model.changing) // Synchronous refusal never enters the command pipeline.
        #expect(outbox.invokes("diversity.setTarget").isEmpty)
        #expect(outbox.messages.allSatisfy { if case .propertyWrite = $0 { return false }; return true })
        #expect(model.reason?.contains("controlled by") == true)
    }

    @Test("Off depends on the live source, even when the unrelated active slice has another controller")
    func offWithForeignActiveSlice() async throws {
        let outbox = SliceListTests.Outbox()
        let commands = CommandClient(send: { outbox.record($0) })
        await commands.handle(.stateChanged(.ready))
        let store = Self.store()
        Self.access(store, id: 0, owner: "desktop", revision: 5)
        let slices = BandSlicesModel(store: store, commands: commands)
        slices.thisDeviceId = "phone"
        let defaults = try #require(UserDefaults(suiteName: "diversity-off-source-only"))
        defer { defaults.removePersistentDomain(forName: "diversity-off-source-only") }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), commands: commands,
                                   slices: slices, captureSender: { { message, permit in
            guard !permit.isRevoked else { throw LinkSendError.notConnected }
            outbox.record(message)
        } })
        #expect(slices.activeSliceId == 0)
        model.setEnabled(false)
        #expect(await ShotWait.until { outbox.invokes("diversity.setTarget").count == 1 })
        let sent = try #require(outbox.invokes("diversity.setTarget").first)
        #expect(sent.args.map(\.value) == [.bool(false), .i64(12), .i64(1), .i64(101), .i64(4),
                                          .i64(-1), .i64(0), .i64(0)])
        #expect(outbox.messages.allSatisfy { if case .propertyWrite = $0 { return false }; return true })
        await commands.receive(.commandResult(.init(verb: sent.verb, id: sent.id, accepted: true,
            reason: "", affected: ["radio"], values: [.init(name: "diversityState", value: .utf8(Self.summary(live: nil, revision: 13)))])))
        #expect(await ShotWait.until { !model.hasPendingAction })
    }

    @Test("a prospective corrected LinkLost summary retains the owner and authoritative pause reason")
    func syntheticCorrectedOutageSummary() throws {
        // Prospective contract fixture only. The reviewed Core connection check is still external/unrun.
        let store = Self.store()
        let json = Self.summary(paused: true)
            .replacingOccurrences(of: "pureSignalResources", with: "radioDisconnected")
            .replacingOccurrences(of: "Diversity pauses while PureSignal transmits on this radio.",
                                  with: "Synthetic Core outage reason.")
        Self.publish(store, json)
        let defaults = try #require(UserDefaults(suiteName: "diversity-outage-summary"))
        defer { defaults.removePersistentDomain(forName: "diversity-outage-summary") }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults))
        #expect(model.sliceKey == "slice:1" && model.enabled == true && model.paused)
        #expect(model.phaseDeg == 123.4 && model.gainDb == -2)
        #expect(model.pauseReason == "Synthetic Core outage reason.")
    }

    @Test("one coordinated move carries exact participants and shows the Core success notice")
    func coordinatedMoveAndNotice() async throws {
        let outbox = SliceListTests.Outbox()
        let commands = CommandClient(send: { outbox.record($0) })
        await commands.handle(.stateChanged(.ready))
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: commands)
        slices.thisDeviceId = "phone"
        let defaults = try #require(UserDefaults(suiteName: "diversity-move-notice"))
        defer { defaults.removePersistentDomain(forName: "diversity-move-notice") }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), commands: commands,
                                   slices: slices, captureSender: { { message, permit in
            guard !permit.isRevoked else { throw LinkSendError.notConnected }
            outbox.record(message)
        } })
        model.setTarget(0, enabled: true)
        #expect(await ShotWait.until { outbox.invokes("diversity.setTarget").count == 1 })
        let sent = try #require(outbox.invokes("diversity.setTarget").first)
        #expect(sent.args.map(\.name) == ["enabled", "stateRevision", "sourceSliceId", "sourceIncarnation",
                                         "sourceControlRevision", "targetSliceId", "targetIncarnation", "targetControlRevision"])
        #expect(sent.args.map(\.value) == [.bool(true), .i64(12), .i64(1), .i64(101), .i64(4), .i64(0), .i64(100), .i64(4)])
        let notice = "Diversity moved from B to A. Both slices paused briefly."
        await commands.receive(.commandResult(.init(verb: sent.verb, id: sent.id, accepted: true,
            reason: notice, affected: ["radio", "slice:1", "slice:0"], values: [
                .init(name: "diversityState", value: .utf8(Self.summary(live: 0, revision: 13))),
                .init(name: "reasonCode", value: .utf8("")),
            ])))
        #expect(await ShotWait.until { !model.changing })
        #expect(model.note == notice)
        #expect(outbox.messages.allSatisfy { if case .propertyWrite = $0 { return false }; return true })
    }

    @Test("a held real SessionRoute handoff rechecks the synchronously revoked source permit")
    func revokedAtActualSessionHandoff() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let app = AppModel()
        await app.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                          transportFactory: station.transportFactory)
        #expect(await ShotWait.until { app.connection == .connected })
        Self.populate(app.mirror)
        app.main.slices.thisDeviceId = "phone"
        let route = app.commandRouteForTesting
        let commands = CommandClient(send: { try await route.send($0) }, captureSender: { route.captureCommandSender() })
        await commands.handle(.stateChanged(.ready))
        let model = DiversityModel(mirror: app.mirror, phone: app.phoneSettings, commands: commands,
            slices: app.main.slices, captureSender: { route.captureCommandSender() })
        let gate = HandoffGate()
        route.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let request) = message, request.verb == DiversityTargetAction.verb { await gate.hold() }
        }
        model.useActiveSlice()
        #expect(await Self.until { await gate.entered })
        Self.access(app.mirror, id: 1, owner: "desktop", revision: 5)
        route.holdCommandHandoffForTesting(nil)
        await gate.release()
        #expect(await Self.until { await commands.waitingCount == 0 })
        #expect(!station.messages.contains { if case .commandInvoke(let request) = $0 { return request.verb == DiversityTargetAction.verb }; return false })
        await app.disconnect()
    }

    @Test("the exact live badge opens through native flag controls with an admitted sender, without selecting, tuning, taking or joining")
    func admittedBadgeRouteHasNoTuneSideEffect() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let app = AppModel()
        await app.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                          transportFactory: station.transportFactory)
        #expect(await ShotWait.until { app.connection == .connected })
        Self.populate(app.mirror)
        app.main.slices.thisDeviceId = "phone"
        let controls = app.main.flagControls
        controls.diversity = app.diversity
        let route = app.commandRouteForTesting
        let probe = CommandClient(send: { try await route.send($0) }, captureSender: { route.captureCommandSender() })
        await probe.handle(.stateChanged(.ready))
        let before = await probe.start("diversity.route.before", arguments: [], copies: 1, timeout: .seconds(5))
        await before.sent()
        #expect(station.messages.contains { if case .commandInvoke(let request) = $0 { return request.verb == "diversity.route.before" }; return false })
        let offset = station.messages.count
        let entry = try #require(app.main.slices.entries.first { $0.id == 1 })
        let active = app.main.slices.activeSliceId
        let frequencies = app.main.slices.entries.map { $0.slice.frequencyHz }
        controls.openDiversity(entry)
        #expect(controls.diversityOpen && controls.diversitySliceId == 1)
        #expect(app.diversity.sliceKey == "slice:1" && app.diversity.phaseDeg == 123.4)
        #expect(app.main.slices.activeSliceId == active)
        #expect(app.main.slices.entries.map { $0.slice.frequencyHz } == frequencies)
        let after = await probe.start("diversity.route.after", arguments: [], copies: 1, timeout: .seconds(5))
        await after.sent()
        let sent = Array(station.messages.dropFirst(offset))
        #expect(sent.contains { if case .commandInvoke(let request) = $0 { return request.verb == "diversity.route.after" }; return false })
        #expect(!sent.contains { message in
            if case .propertyWrite(let write) = message { return write.key.hasPrefix("slice:") }
            if case .commandInvoke(let request) = message {
                return [DiversityTargetAction.verb, BandSlicesModel.activateVerb, SliceAccess.takeControlVerb,
                        "slice.join", BandSlicesModel.setTxSliceVerb].contains(request.verb)
            }
            return false
        })
        await probe.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await app.disconnect()
    }

    @Test("five seconds restores the latest complete Core blend and a newer touch owns its late result")
    func ordinaryActionTimeoutAndLateOwnership() async throws {
        let clock = TestLinkClock()
        let outbox = SliceListTests.Outbox()
        let commands = CommandClient(clock: clock, send: { outbox.record($0) })
        await commands.handle(.stateChanged(.ready))
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: commands)
        slices.thisDeviceId = "phone"
        let name = "diversity-timeout-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), commands: commands,
            slices: slices, captureSender: { { message, permit in
                guard !permit.isRevoked else { throw LinkSendError.notConnected }; outbox.record(message)
            } })
        model.setEnabled(false)
        #expect(await ShotWait.until { outbox.invokes(DiversityTargetAction.verb).count == 1 })
        let old = try #require(outbox.invokes(DiversityTargetAction.verb).first)
        #expect(model.enabled == false)
        await clock.advance(by: 4_999)
        #expect(model.changing && model.enabled == false)
        await clock.advance(by: 1)
        #expect(await ShotWait.until { !model.changing })
        #expect(model.enabled == true && model.phaseDeg == 123.4)
        #expect(model.actionNotConfirmed && model.note == PropertyWriteOutcome.notConfirmed.reason)
        model.setTarget(0, enabled: true)
        #expect(await ShotWait.until { outbox.invokes(DiversityTargetAction.verb).count == 2 })
        await commands.receive(.commandResult(.init(verb: old.verb, id: old.id, accepted: true,
            reason: "obsolete", affected: ["radio"], values: [.init(name: "diversityState", value: .utf8(Self.summary(live: nil, revision: 13)))])))
        #expect(model.changing && model.note != "obsolete" && model.state?.live?.identity.sliceId == 1)
        await commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await ShotWait.until { !model.hasPendingAction })
    }

    @Test("an old finger cannot send a blend to a replacement live context")
    func heldFingerContextIsImmutable() throws {
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: nil)
        slices.thisDeviceId = "phone"
        let outbox = SliceListTests.Outbox()
        let name = "diversity-drag-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), slices: slices,
            captureSender: { { message, _ in outbox.record(message) } })
        model.blendEditing("diversityPhaseDeg", true)
        Self.publish(store, Self.summary(live: 0, revision: 13))
        model.refresh()
        model.setPhase(200)
        #expect(store.object("slice:0")?["diversityPhaseDeg"] == .double(10))
        #expect(outbox.messages.isEmpty)
        model.blendEditing("diversityPhaseDeg", false)
    }


    @Test("recall supersedes an already queued AX offer before it can capture a new Touch")
    func queuedBlendOfferCannotSupersedeRecall() async throws {
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: nil); slices.thisDeviceId = "phone"
        let name = "diversity-recall-offer-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let phone = PhoneSettings(defaults: defaults)
        phone.setString("123.4,-2", for: DiversityModel.memoryKey(band: 5, 0))
        let model = DiversityModel(mirror: store, phone: phone, slices: slices,
            captureSender: { { _, _ in } })
        let clock = TestLinkClock()
        let pacer = SliderSendPacer(clock: clock)
        func offer(_ value: Double) {
            let send = model.capturePhaseCommit(value)
            pacer.send = { next in if next == value { send() } }
            pacer.move(to: value)
        }
        model.blendEditing("diversityPhaseDeg", true)
        offer(180); offer(200)
        #expect(model.phaseDeg == 180 && clock.pendingDueTimes == [50])
        let prior = model.blendGeneration
        model.tapMemory(0) // Explicit recall supersedes BEFORE its direct phase/gain writes.
        #expect(model.blendGeneration != prior && model.phaseDeg == 123.4 && model.gainDb == -2)
        await clock.advance(by: 50) // Row onChange has deliberately not run: model admission is atomic.
        pacer.release() // Nor may a late primary.false create a newer Touch from the old offer.
        #expect(model.phaseDeg == 123.4 && model.gainDb == -2)
        model.capturePhaseCommit(210)() // A fresh offer after recall is admitted normally.
        #expect(model.phaseDeg == 210)
    }

    @Test("queued offers cannot capture a Touch after context replacement, even before refresh", arguments: ["live", "snapshot", "object"])
    func queuedBlendOfferCannotCrossReplacementContext(replacement: String) async throws {
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: nil); slices.thisDeviceId = "phone"
        let name = "diversity-context-offer-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), slices: slices,
            captureSender: { { _, _ in } })
        let clock = TestLinkClock()
        var commits = 0
        let pacer = SliderSendPacer(clock: clock)
        pacer.move(to: 180) // Arm pacing; the next offered value remains queued.
        let queued = model.capturePhaseCommit(200)
        pacer.send = { _ in commits += 1; queued() }
        pacer.move(to: 200)
        if replacement == "live" { Self.publish(store, Self.summary(live: 0, revision: 13)) }
        else if replacement == "object" {
            store.apply(.objectDestroy(.init(key: "access:1", className: SliceAccess.accessClass)))
            Self.access(store, id: 1, owner: "phone", revision: 4) // Same values, different admitted object.
        } else {
            store.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
            Self.populate(store); store.apply(.snapshotComplete)
        }
        let id = replacement == "live" ? 0 : 1
        await clock.advance(by: 50)
        #expect(commits == 1)
        #expect(store.object("slice:\(id)")?["diversityPhaseDeg"] == .double(Double(id + 1) * 10))
        model.refresh()
        model.capturePhaseCommit(210)()
        #expect(model.phaseDeg == 210)
    }

    @Test("terminal revalue cannot publish a new generation or borrow a replaced context", arguments: ["live", "snapshot", "object"])
    func pureTerminalRevalueDoesNotPublishGeneration(replacement: String) throws {
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: nil); slices.thisDeviceId = "phone"
        let name = "diversity-terminal-pure-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), slices: slices,
                                   captureSender: { { _, _ in } })
        let seed = model.capturePhaseCommit(180)
        var generations: [UInt64] = []
        let observation = model.$blendGeneration.dropFirst().sink { generations.append($0) }
        defer { observation.cancel() }
        // No await/refresh: this is the window in which ordinary capture can synchronize and publish.
        if replacement == "live" { Self.publish(store, Self.summary(live: 0, revision: 13)) }
        else if replacement == "object" {
            store.apply(.objectDestroy(.init(key: "access:1", className: SliceAccess.accessClass)))
            Self.access(store, id: 1, owner: "phone", revision: 4)
        } else {
            store.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
            Self.populate(store); store.apply(.snapshotComplete)
        }
        let generation = model.blendGeneration, published = generations
        let phase = model.phaseDeg
        let final = try #require(seed.revalue?(273.8))
        #expect(final.value == 273.8 && final.generation == seed.generation)
        #expect(model.blendGeneration == generation && generations == published)
        final() // Same captured owner, object and snapshot fences as an ordinary queued offer.
        #expect(model.phaseDeg == phase)
        #expect(model.blendGeneration == generation && generations == published)
        let id = replacement == "live" ? 0 : 1
        #expect(store.object("slice:\(id)")?["diversityPhaseDeg"] == .double(Double(id + 1) * 10))
        model.refresh()
        model.capturePhaseCommit(210)()
        #expect(model.phaseDeg == 210) // A normal fresh capture remains admitted.
    }

    @Test("pure terminal revalue keeps the bound normal final send", arguments: ["phase", "gain"])
    func pureTerminalRevalueSendsValidFinal(property: String) throws {
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: nil); slices.thisDeviceId = "phone"
        let name = "diversity-terminal-final-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), slices: slices,
                                   captureSender: { { _, _ in } })
        let key = property == "phase" ? "diversityPhaseDeg" : "diversityGainDb"
        model.blendEditing(key, true)
        defer { model.blendEditing(key, false) }
        let seed = property == "phase" ? model.capturePhaseCommit(180) : model.captureGainCommit(-2)
        var generations: [UInt64] = []
        let observation = model.$blendGeneration.dropFirst().sink { generations.append($0) }
        defer { observation.cancel() }
        let expected = property == "phase" ? 273.8 : -11.0
        let final = try #require(seed.revalue?(expected))
        #expect(final.value == expected && final.generation == seed.generation)
        #expect(model.blendGeneration == seed.generation && generations.isEmpty)
        final()
        #expect((property == "phase" ? model.phaseDeg : model.gainDb) == expected)
        #expect(model.blendGeneration == seed.generation && generations.isEmpty)
    }

    @Test("Core echo alone keeps the newer queued offer's intent")
    func coreEchoDoesNotRetireQueuedBlendOffer() async throws {
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: nil); slices.thisDeviceId = "phone"
        let name = "diversity-echo-offer-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), slices: slices,
            captureSender: { { _, _ in } })
        let generation = model.blendGeneration
        let clock = TestLinkClock()
        let pacer = SliderSendPacer(clock: clock)
        pacer.move(to: 180)
        let queued = model.capturePhaseCommit(190)
        pacer.send = { _ in queued() }; pacer.move(to: 190)
        Self.publish(store, Self.summary(revision: 13)); model.refresh()
        #expect(model.blendGeneration == generation)
        await clock.advance(by: 50)
        #expect(model.phaseDeg == 190)
    }

    @Test("queued blend intent retires synchronously at relevant deletion before recreation or refresh",
          arguments: ["access:1", "slice:1", "radio"])
    func queuedBlendIntentRetiresAtObjectDeletion(key: String) async throws {
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: nil); slices.thisDeviceId = "phone"
        let name = "diversity-deletion-offer-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), slices: slices,
            captureSender: { { _, _ in } })
        let clock = TestLinkClock()
        let pacer = SliderSendPacer(clock: clock)
        pacer.move(to: 180)
        let queued = model.capturePhaseCommit(200)
        let generation = model.blendGeneration
        pacer.send = { _ in queued() }; pacer.move(to: 200)
        let className = key == "radio" ? "RadioModel" : key == "slice:1" ? "SliceModel" : SliceAccess.accessClass
        store.apply(.objectDestroy(.init(key: key, className: className)))
        // No await, replacement, refresh or retained old object: observe the deletion boundary itself.
        #expect(store.object(key) == nil)
        #expect(model.blendGeneration != generation)
        #expect(clock.pendingDueTimes == [50]) // Row retirement has deliberately not run.
        if key == "access:1" { Self.access(store, id: 1, owner: "phone", revision: 4) }
        else if key == "radio" {
            store.apply(.objectCreate(.init(key: "radio", className: "RadioModel", properties: [
                .init(ordinal: 37, name: "diversityState", value: .utf8(Self.summary())),
            ])))
        } else {
            store.apply(BandFlagShotTests.slice(1, active: false))
            store.apply(.delta(.init(key: "slice:1", properties: [
                .init(ordinal: 14, name: "band", value: .enumeration(5)),
                .init(ordinal: 29, name: "diversityEnabled", value: .bool(true)),
                .init(ordinal: 30, name: "diversityPhaseDeg", value: .f64(20)),
                .init(ordinal: 31, name: "diversityGainDb", value: .f64(1)),
            ])))
        }
        await clock.advance(by: 50)
        pacer.release()
        #expect(store.object("slice:1")?["diversityPhaseDeg"] == .double(20))
        model.refresh()
        model.capturePhaseCommit(210)()
        #expect(model.phaseDeg == 210)
    }

    @Test("unrelated object removal and recreation plus Core echo preserve the queued blend intent")
    func unrelatedObjectChangesKeepQueuedBlendOffer() async throws {
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: nil); slices.thisDeviceId = "phone"
        let name = "diversity-unrelated-offer-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), slices: slices,
            captureSender: { { _, _ in } })
        let clock = TestLinkClock()
        let pacer = SliderSendPacer(clock: clock)
        pacer.move(to: 180)
        let queued = model.capturePhaseCommit(200)
        let generation = model.blendGeneration
        pacer.send = { _ in queued() }; pacer.move(to: 200)
        store.apply(.objectDestroy(.init(key: "access:0", className: SliceAccess.accessClass)))
        #expect(model.blendGeneration == generation)
        Self.access(store, id: 0, owner: "phone", revision: 4)
        #expect(model.blendGeneration == generation)
        Self.publish(store, Self.summary(revision: 13)); model.refresh()
        #expect(model.blendGeneration == generation && clock.pendingDueTimes == [50])
        await clock.advance(by: 50)
        #expect(model.phaseDeg == 200 && store.object("slice:1")?["diversityPhaseDeg"] == .double(200))
    }

    private actor HandoffGate {
        private var waiting: CheckedContinuation<Void, Never>?
        private(set) var entered = false
        func hold() async { entered = true; await withCheckedContinuation { waiting = $0 } }
        func release() { waiting?.resume(); waiting = nil }
    }
    private static func until(_ condition: () async -> Bool) async -> Bool {
        for _ in 0..<50_000 { if await condition() { return true }; await Task.yield() }
        return await condition()
    }

    @Test("a negotiated complete summary opens the existing Tools route before a delayed catalogue, but explicit unsupported hardware stays omitted")
    func delayedCatalogueVersusExplicitUnsupported() async throws {
        let store = Self.store()
        let feed = CatalogFeed(store: store)
        let list = ToolListModel(mirror: store, catalogFeed: feed)
        #expect(list.entries.first { $0.id == "diversity" }?.enabled == true)
        var catalog = try #require(ModesTabBindingTests.catalogueObject("catalog-anan-g2"))
        var tools = try #require(catalog["tools"] as? [[String: Any]])
        let index = try #require(tools.firstIndex { $0["id"] as? String == "diversity" })
        tools[index]["offered"] = false
        catalog["tools"] = tools
        let json = String(decoding: try JSONSerialization.data(withJSONObject: catalog), as: UTF8.self)
        store.apply(.capabilities(.init(properties: [
            .init(name: DiversityState.capabilityName, value: .i64(1)),
            .init(name: "stationCatalogVersion", value: .i64(1)),
        ])))
        store.apply(.objectCreate(.init(key: "catalog", className: "StationCatalog", properties: [
            .init(name: "json", value: .utf8(json)), .init(name: "revision", value: .i64(1)),
        ])))
        #expect(await ShotWait.until { feed.catalog != nil && !list.entries.contains { $0.id == "diversity" } })
        let old = MirrorStore(send: { _ in })
        old.apply(.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        old.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
        old.apply(.snapshotComplete)
        let oldList = ToolListModel(mirror: old, catalogFeed: CatalogFeed(store: old))
        #expect(oldList.entries.first { $0.id == "diversity" }?.enabled == false)
    }

    private func refusedTouch(_ mutate: (MirrorStore) -> Void) async throws {
        let outbox = SliceListTests.Outbox()
        let commands = CommandClient(send: { outbox.record($0) })
        await commands.handle(.stateChanged(.ready))
        let store = Self.store()
        let slices = BandSlicesModel(store: store, commands: commands)
        slices.thisDeviceId = "phone"
        let name = "diversity-touch-" + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults), commands: commands,
                                   slices: slices, captureSender: { { message, permit in
            guard !permit.isRevoked else { throw LinkSendError.notConnected }
            outbox.record(message)
        } })
        model.setTarget(0, enabled: true)
        mutate(store) // Same MainActor turn: the touch was captured, no queued handoff has run.
        #expect(await ShotWait.until { !model.hasPendingAction })
        #expect(outbox.invokes("diversity.setTarget").isEmpty)
    }

    static func store(sourceOwner: String = "phone") -> MirrorStore {
        let store = MirrorStore(send: { _ in })
        store.apply(.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        store.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
        Self.populate(store, sourceOwner: sourceOwner)
        store.apply(.snapshotComplete)
        return store
    }

    static func populate(_ store: MirrorStore, sourceOwner: String = "phone") {
        store.apply(.capabilities(.init(properties: [
            .init(name: "diversityControlVersion", value: .i64(1)), .init(name: "sliceAccessVersion", value: .i64(3)),
        ])))
        store.apply(.schema(.init(className: "RadioModel", fields: [.init(ordinal: 37, name: "diversityState", kind: .utf8)])))
        store.apply(.schema(.init(className: "SliceModel", fields: [
            .init(ordinal: 29, name: "diversityEnabled", kind: .bool),
            .init(ordinal: 30, name: "diversityPhaseDeg", kind: .f64),
            .init(ordinal: 31, name: "diversityGainDb", kind: .f64),
        ])))
        store.apply(.objectCreate(.init(key: "radio", className: "RadioModel", properties: [
            .init(ordinal: 37, name: "diversityState", value: .utf8(Self.summary(owner: sourceOwner))),
        ])))
        for id in 0...2 {
            store.apply(BandFlagShotTests.slice(id, active: id == 0))
            store.apply(.delta(.init(key: "slice:\(id)", properties: [
                .init(ordinal: 14, name: "band", value: .enumeration(5)),
                .init(ordinal: 29, name: "diversityEnabled", value: .bool(id == 1)),
                .init(ordinal: 30, name: "diversityPhaseDeg", value: .f64(Double(id + 1) * 10)),
                .init(ordinal: 31, name: "diversityGainDb", value: .f64(Double(id))),
            ])))
            Self.access(store, id: id, owner: id == 1 ? sourceOwner : "phone", revision: 4)
        }
    }

    static func access(_ store: MirrorStore, id: Int, owner: String, revision: Int64) {
        store.apply(.objectCreate(.init(key: "access:\(id)", className: "SliceAccess", properties: [
            .init(ordinal: 0, name: "sliceId", value: .i64(Int64(id))),
            .init(ordinal: 1, name: "incarnation", value: .i64(Int64(100 + id))),
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(owner)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(revision)),
            .init(ordinal: 4, name: "listenerDeviceIds", value: .utf8(LinkJSON.array(owner == "phone" ? [.string("phone")] : [.string(owner), .string("phone")]).compactText)),
            .init(ordinal: 6, name: "txSelected", value: .bool(id == 0)),
            .init(ordinal: 7, name: "onAir", value: .bool(false)),
        ])))
    }

    static func summary(live: Int? = 1, revision: Int = 12, owner: String = "phone", paused: Bool = false) -> String {
        var object: [String: LinkJSON] = [
            "version": .number(1), "revision": .number(Double(revision)), "requested": .bool(live != nil),
            "running": .bool(live != nil && !paused), "paused": .bool(paused),
            "reasonCode": .string(paused ? "pureSignalResources" : ""),
            "reason": .string(paused ? "Diversity pauses while PureSignal transmits on this radio." : ""),
        ]
        object["live"] = live.map { id in .object([
            "sliceId": .number(Double(id)), "incarnation": .number(Double(100 + id)), "controlRevision": .number(4),
            "controllerDeviceId": .string(owner), "letter": .string(SliceAccess.letter(id)), "band": .number(5),
            "frequencyHz": .number(14200000), "phaseDeg": .number(123.4), "gainDb": .number(-2),
            "fineNullEnabled": .bool(false), "pattern": .null,
        ]) } ?? .null
        object["targets"] = .array((0...2).map { id in .object([
            "sliceId": .number(Double(id)), "incarnation": .number(Double(100 + id)), "controlRevision": .number(4),
            "controllerDeviceId": .string(id == live ? owner : "phone"), "eligible": .bool(true),
            "reasonCode": .string(""), "reason": .string(""),
        ]) })
        return LinkJSON.object(object).compactText
    }

    static func publish(_ store: MirrorStore, _ summary: String) {
        store.apply(.delta(.init(key: "radio", properties: [.init(ordinal: 37, name: "diversityState", value: .utf8(summary))])))
    }
}
