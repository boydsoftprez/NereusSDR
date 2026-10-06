// NereusSDR for iOS: the app's model connects to a Core and feeds the mirror, settings and connection state
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Combine
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

@Suite("AppModel")
@MainActor
struct AppModelTests {
    /// The media ladder on a Core dialled by address (R-IOS-16): IPv6
    /// direct, an IPv4 hole punch through STUN, then the Core's tunnel.
    /// With no service session first the peer still takes every candidate
    /// type but relay, and the STUN a caller hands in; after one it takes
    /// that session's STUN server. Never a relay server, always the tunnel.
    @Test func aDirectAddressMediaPeerTakesStunAndEveryCandidateButRelayAndKeepsTheTunnel() throws {
        let session = StationSession(trust: .identity(publicKey: Data(repeating: 1, count: 91)),
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     asyncTransport: { throw CancellationError() })
        let tunnel = MediaTunnelContext(session: session)
        let made = MadeDirectPeers()
        let maker: AppModel.DirectAddressPeerMaker = { configuration, carrier in
            made.add(configuration, tunnelled: carrier === tunnel)
            return FakeMediaPeer()
        }

        let plain = AppModel.directAddressMedia(serviceIce: nil, stunServers: [], tunnel: tunnel, makePeer: maker)
        _ = plain.factory()
        #expect(plain.deadline == MediaControlClient.connectDeadline)
        let handedIn = AppModel.directAddressMedia(serviceIce: nil,
                                                   stunServers: ["turn:user:secret@turn.invalid:3478",
                                                                 "stun:stun.invalid:3478"],
                                                   tunnel: tunnel, makePeer: maker)
        _ = handedIn.factory()
        #expect(handedIn.deadline == IceSettings.connectDeadline)
        var ice = IceSettings(stunUrls: ["stun:stun.invalid:3478"], local: .ipv4Only)
        ice.setRelay(RendezvousTurn(username: "1800086400:station", password: "cGFzcw==", expires: 1_800_086_400,
                                    urls: ["turn:turn.invalid:3478?transport=udp"]), families: 1)
        let afterService = AppModel.directAddressMedia(serviceIce: ice, stunServers: [], tunnel: tunnel,
                                                       makePeer: maker)
        _ = afterService.factory()
        #expect(afterService.deadline == IceSettings.connectDeadline)

        let peers = made.all
        try #require(peers.count == 3)
        #expect(peers.allSatisfy { $0.tunnelled })
        #expect(peers.allSatisfy { !$0.configuration.hostCandidatesOnly })
        #expect(peers.allSatisfy { !$0.configuration.iceServers.contains { $0.hasPrefix("turn") } })
        #expect(peers[0].configuration.iceServers.isEmpty)
        #expect(peers[0].configuration.refusesRelayCandidates)
        #expect(peers[1].configuration.iceServers == ["stun:stun.invalid:3478"])
        #expect(peers[1].configuration.refusesRelayCandidates)
        #expect(peers[2].configuration.iceServers == ["stun:stun.invalid:3478"])
    }

    /// The Core's own STUN list (`mediaStunUrls`) comes first for a Core
    /// dialled by address; without a usable `stun:` server in it, the last
    /// service session's STUN server; with neither, none.
    @Test func theCoresStunListComesBeforeTheServiceSessionsStun() throws {
        let session = StationSession(trust: .identity(publicKey: Data(repeating: 1, count: 91)),
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     asyncTransport: { throw CancellationError() })
        let tunnel = MediaTunnelContext(session: session)
        let made = MadeDirectPeers()
        let maker: AppModel.DirectAddressPeerMaker = { configuration, carrier in
            made.add(configuration, tunnelled: carrier === tunnel)
            return FakeMediaPeer()
        }
        let ice = IceSettings(stunUrls: ["stun:service.invalid:3478"], local: .ipv4Only)
        _ = AppModel.directAddressMedia(serviceIce: ice, stunServers: ["stun:core.invalid:3478"],
                                        tunnel: tunnel, makePeer: maker).factory()
        _ = AppModel.directAddressMedia(serviceIce: ice, stunServers: ["turn:core.invalid:3478"],
                                        tunnel: tunnel, makePeer: maker).factory()
        let peers = made.all
        try #require(peers.count == 2)
        #expect(peers[0].configuration.iceServers == ["stun:core.invalid:3478"])
        #expect(peers[1].configuration.iceServers == ["stun:service.invalid:3478"])
        #expect(peers.allSatisfy { $0.tunnelled })

        #expect(AppModel.directStunServers(core: ["stun:core.invalid:3478"], serviceIce: ice)
                == ["stun:core.invalid:3478"])
        #expect(AppModel.directStunServers(core: [], serviceIce: ice) == ["stun:service.invalid:3478"])
        #expect(AppModel.directStunServers(core: nil, serviceIce: ice) == ["stun:service.invalid:3478"])
        #expect(AppModel.directStunServers(core: nil, serviceIce: nil).isEmpty)
        #expect(AppModel.coreStunUrls(in: ["mediaStunUrls": .text(#"["stun:core.invalid:3478"]"#)])
                == ["stun:core.invalid:3478"])
        #expect(AppModel.coreStunUrls(in: [:]) == nil)
        #expect(AppModel.coreStunUrls(in: ["mediaStunUrls": .int(1)]) == nil)
    }

    /// The Core's STUN list lives for its session alone: read from its
    /// capabilities, gone at the disconnect, and never written to settings.
    @Test func theCoresStunListIsHeldForTheSessionAndNeverSaved() async throws {
        let station = try FakeStation()
        let suite = "AppModelCoreStun-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults))
        await connect(model, to: station)
        #expect(await settle { model.connection == .connected })
        #expect(model.coreStunUrls == nil)
        let list = #"["stun:core.invalid:3478","turn:relay.invalid:3478"]"#
        let capabilities = model.mirror.capabilities.map {
            LinkMessage.PropertyEntry(name: $0.key, value: $0.value.wireValue)
        } + [.init(name: "mediaStunUrls", value: .utf8(list))]
        await station.deliver(.capabilities(.init(properties: capabilities)))
        #expect(await settle { model.coreStunUrls == ["stun:core.invalid:3478"] })
        let saved = defaults.persistentDomain(forName: suite) ?? [:]
        #expect(!saved.values.contains { "\($0)".contains("core.invalid") })
        await model.disconnect()
        #expect(model.coreStunUrls == nil)
        let afterwards = defaults.persistentDomain(forName: suite) ?? [:]
        #expect(!afterwards.values.contains { "\($0)".contains("core.invalid") })
    }

    /// The ladder reads the same transmit state a control move does, and
    /// nothing known reads as keyed.
    @Test func withNoSessionTheLadderReadsKeyed() {
        #expect(AppModel().mediaLadderState() == .unknown)
        #expect(!MediaLadderState.unknown.quiet)
    }

    private final class SessionClock {
        var now: Date
        init(_ now: Date) { self.now = now }
    }

    private func longSessionSources(clock: SessionClock, traffic: TrafficCounter) -> SessionController.Sources {
        SessionController.Sources(power: ThermalAndPowerWatcher(read: { .steady }, center: NotificationCenter()),
                                  network: nil, traffic: { traffic.reading }, now: { clock.now },
                                  tick: .seconds(3600))
    }
    private actor StartGate {
        private var waiting: CheckedContinuation<Void, Never>?
        private(set) var entered = false

        func hold() async {
            entered = true
            await withCheckedContinuation { waiting = $0 }
        }

        func release() {
            waiting?.resume()
            waiting = nil
        }
    }

    /// Waits, without sleeping, until `condition` holds. Each turn lets the
    /// main actor run what is queued on it, where the model is fed.
    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func connect(_ model: AppModel, to station: FakeStation) async {
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
    }

    @Test("starts with no Core")
    func startsNotConnected() {
        let model = AppModel()
        #expect(model.session == nil)
        #expect(model.connection == .notConnected)
        #expect(!model.mirror.isSnapshotComplete)
    }

    @Test("queued OLD transmit copies never reach AppModel's NEW session")
    func queuedTransmitStaysWithAdmittingSession() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let newer = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        await connect(model, to: old)
        #expect(await settle { model.connection == .connected })

        let gate = StartGate()
        model.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == "held.command" {
                await gate.hold()
            }
        }
        let earlier = await model.commands.start("held.command", arguments: [], copies: 1,
                                                 timeout: .seconds(10))
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        let oldOff = await model.commands.start("tx.unkey", arguments: [
            CommandArgument(name: "epoch", value: .int(7)),
        ], copies: 3, timeout: .seconds(10))
        await model.disconnect()
        await connect(model, to: newer)
        #expect(await settle { model.connection == .connected })
        await gate.release()
        await earlier.sent()
        await oldOff.sent()
        #expect(newer.messages.compactMap { message -> String? in
            if case .commandInvoke(let invoke) = message { return invoke.verb }
            return nil
        }.contains("tx.unkey") == false)

        let current = await model.commands.start("new.command", arguments: [], copies: 1,
                                                 timeout: .seconds(10))
        await current.sent()
        #expect(newer.messages.contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "new.command" }
            return false
        })
        model.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await model.disconnect()
    }

    @Test("OLD transmit paused inside SessionRoute cannot send on NEW")
    func suspendedTransmitHandoffStaysWithOldSession() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let newer = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        await connect(model, to: old)
        #expect(await settle { model.connection == .connected })
        let gate = StartGate()
        model.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == "tx.key" {
                await gate.hold()
            }
        }
        let oldKey = await model.commands.start("tx.key", arguments: [
            CommandArgument(name: "trigger", value: .text("screen")),
        ], copies: 3, timeout: .seconds(10))
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        await model.disconnect()
        await connect(model, to: newer)
        #expect(await settle { model.connection == .connected })
        await gate.release()
        await oldKey.sent()
        #expect(!newer.messages.contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "tx.key" }
            return false
        })
        let current = await model.commands.start("new.command", arguments: [], copies: 1,
                                                 timeout: .seconds(10))
        await current.sent()
        #expect(newer.messages.contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "new.command" }
            return false
        })
        model.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await model.disconnect()
    }

    @Test("cancellation at the actual command handoff stops every key copy", arguments: [1, 2, 3])
    func cancellationAtTransportHandoff(copy: Int) async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        await connect(model, to: station)
        #expect(await settle { model.connection == .connected })
        let gate = StartGate()
        let counter = HandoffCounter()
        model.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == "tx.key", counter.next() == copy {
                await gate.hold()
            }
        }
        let key = Task {
            try await model.commands.invoke("tx.key", arguments: [], copies: 3, timeout: .seconds(10))
        }
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        key.cancel()
        for _ in 0..<50_000 {
            if await model.commands.waitingCount == 0 { break }
            await Task.yield()
        }
        #expect(await model.commands.waitingCount == 0)
        model.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await gate.release()
        _ = try? await key.value
        let fresh = await model.commands.start("new.command", arguments: [], copies: 3, timeout: .seconds(10))
        await fresh.sent()
        #expect(station.messages.filter { TransmitScreenTests.invoke($0)?.verb == "tx.key" }.count == copy - 1)
        #expect(station.messages.filter { TransmitScreenTests.invoke($0)?.verb == "new.command" }.count == 3)
        await model.disconnect()
    }

    @Test("timeout authority reaches the final StationSession handoff")
    func timeoutAtTransportHandoff() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        await connect(model, to: station)
        #expect(await settle { model.connection == .connected })
        let clock = TestLinkClock()
        let route = model.commandRouteForTesting
        let client = CommandClient(clock: clock, send: { try await route.send($0) },
                                   captureSender: { route.captureCommandSender() })
        await client.handle(.stateChanged(.ready))
        let gate = StartGate()
        route.holdCommandHandoffForTesting { message in
            if TransmitScreenTests.invoke(message)?.verb == "tx.key" { await gate.hold() }
        }
        let key = await client.start("tx.key", arguments: [], copies: 3, timeout: .seconds(1))
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        await clock.advance(by: 1_000)
        await #expect(throws: CommandError.timedOut) { try await key.result() }
        route.holdCommandHandoffForTesting(nil)
        await gate.release()
        await key.sent()
        #expect(!station.messages.contains { TransmitScreenTests.invoke($0)?.verb == "tx.key" })
        let fresh = await client.start("new.command", arguments: [], copies: 1, timeout: .seconds(1))
        await fresh.sent()
        #expect(station.messages.contains { TransmitScreenTests.invoke($0)?.verb == "new.command" })
        await model.disconnect()
    }

    @Test("a queued old VOX property cannot reach a replacement session after permit revocation")
    func oldVoxPropertyAtTransportHandoff() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let newer = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        await connect(model, to: old)
        #expect(await settle { model.connection == .connected })
        let route = model.commandRouteForTesting
        let sender = route.captureVoxSender()
        let permit = CommandSendPermit()
        let gate = StartGate()
        route.holdCommandHandoffForTesting { message in
            if case .propertyWrite(let write) = message,
               write.key == "transmit", write.properties.first?.name == "voxEnabled" {
                await gate.hold()
            }
        }
        let pending = Task {
            await model.mirror.writeBound("transmit", property: "voxEnabled", value: .bool(true),
                                          sender: sender, authority: permit)
        }
        for _ in 0..<50_000 { if await gate.entered { break }; await Task.yield() }
        #expect(await gate.entered)
        permit.revoke()
        route.holdCommandHandoffForTesting(nil)
        await model.disconnect()
        await connect(model, to: newer)
        #expect(await settle { model.connection == .connected })
        await gate.release()
        let outcome = await pending.value
        #expect(!outcome.accepted)
        #expect(!old.messages.contains { message in
            if case .propertyWrite(let write) = message { return write.key == "transmit" }
            return false
        })
        #expect(!newer.messages.contains { message in
            if case .propertyWrite(let write) = message { return write.key == "transmit" }
            return false
        })
        await model.disconnect()
    }

    @Test("a retired heartbeat cannot hand off after this session rearms VOX")
    func retiredHeartbeatAtStationHandoff() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        await connect(model, to: station)
        #expect(await settle { model.connection == .connected })
        let clock = TestLinkClock()
        let ptt = PttController(commands: TransmitCommandClient(commands: model.commands), clock: clock)
        await ptt.linkChanged(up: true)
        await ptt.setVoxArmed(true)
        let gate = StartGate()
        model.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if TransmitScreenTests.invoke(message)?.verb == "tx.keepalive" { await gate.hold() }
        }
        let tick = Task { await clock.advance(by: 100) }
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        await ptt.setVoxArmed(false)
        await ptt.setVoxArmed(true)
        model.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await gate.release()
        await tick.value
        #expect(!station.messages.contains { TransmitScreenTests.invoke($0)?.verb == "tx.keepalive" })
        await clock.advance(by: 100)
        #expect(station.messages.filter { TransmitScreenTests.invoke($0)?.verb == "tx.keepalive" }.count == 1)
        await ptt.linkChanged(up: false)
        await model.disconnect()
    }

    @Test("OLD PTT authority survives suspension before CommandClient admission")
    func pttBeforeCommandAdmissionCannotAcquireNewSession() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let newer = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        await connect(model, to: old)
        #expect(await settle { model.connection == .connected })
        let gate = StartGate()
        let adapter = AdmissionHeldTransmit(base: TransmitCommandClient(commands: model.commands),
                                             holdVerb: { _ in await gate.hold() })
        let ptt = PttController(commands: adapter)
        await ptt.logicalSessionChanged(1)
        await ptt.linkChanged(up: true)
        await ptt.tap()
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        await ptt.logicalSessionChanged(2)
        await model.disconnect()
        await connect(model, to: newer)
        #expect(await settle { model.connection == .connected })
        await ptt.linkChanged(up: true)
        await gate.release()
        // A fresh queue item establishes that the released OLD step completed.
        await ptt.settle()
        #expect(!newer.messages.contains { TransmitScreenTests.invoke($0)?.verb == "tx.key" })
        let fresh = await model.commands.start("new.command", arguments: [], copies: 1, timeout: .seconds(10))
        await fresh.sent()
        #expect(newer.messages.contains { TransmitScreenTests.invoke($0)?.verb == "new.command" })
        await model.disconnect()
    }

    @Test("OLD off and compensation retain authority before downstream admission", arguments: [false, true])
    func oldOffBeforeAdmissionCannotReachNew(compensation: Bool) async throws {
        let old = try FakeStation(additions: [.remoteTx])
        let newer = try FakeStation(additions: [.remoteTx])
        let replies = HeldCommandReplies()
        if compensation { replies.hold("tx.key") }
        let model = AppModel()
        await model.connect(to: old.endpoint, trust: old.trust, authenticator: old.authenticator,
                            transportFactory: { endpoint, trust in
            ReplyHoldingTransport(inner: old.transportFactory(endpoint, trust), replies: replies)
        })
        #expect(await settle { model.connection == .connected })
        let gate = StartGate()
        let offCount = HandoffCounter()
        let adapter = AdmissionHeldTransmit(base: TransmitCommandClient(commands: model.commands), holdVerb: { verb in
            if case .unkey = verb, offCount.next() == (compensation ? 2 : 1) { await gate.hold() }
        })
        let ptt = PttController(commands: adapter)
        await ptt.logicalSessionChanged(1)
        await ptt.linkChanged(up: true)
        await ptt.tap()
        if compensation {
            #expect(await settle { replies.count("tx.key") == 3 })
        } else {
            for _ in 0..<50_000 { if await ptt.state.isKeyed { break }; await Task.yield() }
            #expect(await ptt.state.isKeyed)
        }
        await ptt.tap()
        if compensation {
            for _ in 0..<50_000 { if await ptt.state == .idle { break }; await Task.yield() }
            #expect(await ptt.state == .idle)
            await replies.release("tx.key")
        }
        for _ in 0..<50_000 { if await gate.entered { break }; await Task.yield() }
        #expect(await gate.entered)
        await ptt.logicalSessionChanged(2)
        await model.disconnect()
        await connect(model, to: newer)
        #expect(await settle { model.connection == .connected })
        await ptt.linkChanged(up: true)
        await gate.release()
        await ptt.settle()
        #expect(!newer.messages.contains { TransmitScreenTests.invoke($0)?.verb == "tx.unkey" })
        let fresh = await model.commands.start("new.command", arguments: [], copies: 1, timeout: .seconds(1))
        await fresh.sent()
        #expect(newer.messages.contains { TransmitScreenTests.invoke($0)?.verb == "new.command" })
        await model.disconnect()
    }

    @Test("buffered OLD key answers released after NEW is keyed cannot control NEW")
    func bufferedOldKeyAnswersAfterNewReadyCannotControlNew() async throws {
        let old = try FakeStation(additions: [.remoteTx])
        let newer = try FakeStation(additions: [.remoteTx])
        let replies = HeldCommandReplies()
        replies.hold("tx.key")
        let model = AppModel()
        await model.connect(to: old.endpoint, trust: old.trust, authenticator: old.authenticator,
                            transportFactory: { endpoint, trust in
            ReplyHoldingTransport(inner: old.transportFactory(endpoint, trust), replies: replies)
        })
        try #require(await settle { model.connection == .connected })
        // Keep OLD alive so release exercises its retired transport callback.
        let oldSession = try #require(model.session)
        #expect(model.commandRouteForTesting.session === oldSession)
        let clock = TestLinkClock()
        let ptt = PttController(commands: TransmitCommandClient(commands: model.commands), clock: clock)
        await ptt.logicalSessionChanged(1)
        await ptt.linkChanged(up: true)
        await ptt.tap()
        try #require(await settle { replies.count("tx.key") == 3 })
        let oldKeys = old.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }
        #expect(oldKeys.count == 3)
        let oldKeyId = try #require(oldKeys.first?.id)
        #expect(oldKeys.allSatisfy { $0.id == oldKeyId })
        #expect(old.keyed)
        #expect(await ptt.state == .keying)

        await ptt.linkChanged(up: false)
        await model.disconnect()
        #expect(await oldSession.state == .stopped)
        await ptt.logicalSessionChanged(2)
        await connect(model, to: newer)
        try #require(await settle { model.connection == .connected })
        let newSession = try #require(model.session)
        #expect(newSession !== oldSession)
        #expect(model.commandRouteForTesting.session === newSession)
        await ptt.linkChanged(up: true)
        await ptt.tap()
        await ptt.settle()
        #expect(await ptt.state.isKeyed)
        #expect(newer.keyed)
        let newKeys = newer.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }
        #expect(newKeys.count == 3)
        let newKeyId = try #require(newKeys.first?.id)
        #expect(newKeyId != oldKeyId)
        #expect(newKeys.allSatisfy { $0.id == newKeyId })
        let newPtt = await ptt.snapshot
        #expect(newPtt.logicalSessionOwner == 2)
        #expect(newPtt.keyKind == .ptt)
        // OLD's three actual accepted replies remain buffered through NEW readiness and keying.
        #expect(replies.count("tx.key") == 3)
        #expect(!newer.messages.contains { TransmitScreenTests.invoke($0)?.verb == "tx.unkey" })

        await replies.release("tx.key")
        #expect(replies.count("tx.key") == 0)
        #expect(await oldSession.state == .stopped)
        await ptt.settle()
        let fresh = await model.commands.start("new.command", arguments: [], copies: 3, timeout: .seconds(1))
        await fresh.sent()
        let afterReply = newer.messages.compactMap(TransmitScreenTests.invoke)
        #expect(afterReply.filter { $0.verb == "tx.key" }.count == 3)
        #expect(afterReply.filter { $0.verb == "tx.key" }.allSatisfy { $0.id == newKeyId })
        #expect(afterReply.filter { $0.verb == "tx.unkey" }.isEmpty)
        #expect(afterReply.filter { $0.verb == "new.command" }.count == 3)
        #expect(await ptt.snapshot == newPtt)
        #expect(await ptt.state.isKeyed)
        #expect(newer.keyed)

        // A fresh NEW off still traverses the same app command route.
        await ptt.tap()
        await ptt.settle()
        #expect(await ptt.state == .idle)
        #expect(!newer.keyed)
        #expect(newer.messages.filter { TransmitScreenTests.invoke($0)?.verb == "tx.unkey" }.count == 3)
        await ptt.linkChanged(up: false)
        #expect(clock.pendingDueTimes.isEmpty)
        await model.disconnect()
    }

    @Test("disconnect cancels a start suspended before media activation")
    func disconnectCancelsPendingMediaStart() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        let gate = StartGate()
        model.startBeforeMediaActivationForTesting = { await gate.hold() }
        let connecting = Task { await connect(model, to: station) }
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        await model.disconnect()
        await gate.release()
        await connecting.value
        #expect(model.session == nil)
        #expect(model.connection == .notConnected)
        #expect(station.connectionCount == 0)
        model.startBeforeMediaActivationForTesting = nil
    }

    @Test("disconnect retires a media owner activated before session publication")
    func disconnectRetiresActivatedPendingStart() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let model = AppModel()
        let gate = StartGate()
        model.startAfterMediaActivationForTesting = { await gate.hold() }
        let connecting = Task { await connect(model, to: station) }
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        let oldOwner = try #require(model.mediaOwnerForTesting)
        await model.disconnect()
        await gate.release()
        await connecting.value
        #expect(model.session == nil)
        #expect(station.connectionCount == 0)
        #expect(!(await model.media.usePeers({ MediaPeer() },
                                             connectDeadline: MediaControlClient.connectDeadline,
                                             owner: oldOwner)))
        model.startAfterMediaActivationForTesting = nil
    }

    @Test("a canceled pre-start winner cannot publish over a newer Core")
    func canceledWinnerBeforeStartCannotReplaceNewSession() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let newer = try FakeStation(fixture: "session-device-sign-in")
        let endpoint = StationEndpoint(host: "198.51.100.8")
        let lease = PreauthenticatedTransport(old.transportFactory(endpoint, old.identityTrust))
        _ = try await lease.inspect(clock: SystemLinkClock(), deadline: .seconds(5))
        let service = RankedServiceRoute(station: old, rank: 3)
        let winner = PathRacer.Winner(transport: lease, route: .service(service), path: .relay,
                                      mediaIce: nil, row: 0, rank: .turn)
        let model = AppModel()
        let gate = StartGate()
        model.winnerAvailabilityCheckedForTesting = { await gate.hold() }
        let oldConnect = Task {
            await model.connect(winner: winner, name: old.label, trust: old.identityTrust,
                                authenticator: old.authenticator, clock: SystemLinkClock(),
                                transportFactory: old.transportFactory)
        }
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        await model.disconnect()
        #expect(model.currentPathRank == nil)
        await connect(model, to: newer)
        #expect(await settle { model.connection == .connected })
        let newSession = try #require(model.session)
        await gate.release()
        #expect(!(await oldConnect.value))
        #expect(model.session === newSession)
        #expect(model.coreHost == newer.endpoint.host)
        #expect(old.messages.filter { $0.kind == .authRequest }.isEmpty)
        model.winnerAvailabilityCheckedForTesting = nil
        await model.disconnect()
    }

    @Test("a verified service lease lost before start releases its pending route")
    func unavailableWinnerBeforeStartClearsPendingRoute() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let endpoint = StationEndpoint(host: "198.51.100.8")
        let lease = PreauthenticatedTransport(station.transportFactory(endpoint, station.identityTrust))
        _ = try await lease.inspect(clock: SystemLinkClock(), deadline: .seconds(5))
        let service = RankedServiceRoute(station: station, rank: 3)
        let winner = PathRacer.Winner(transport: lease, route: .service(service), path: .relay,
                                      mediaIce: nil, row: 0, rank: .turn)
        let model = AppModel()
        model.winnerAvailabilityCheckedForTesting = { lease.close() }
        let adopted = await model.connect(winner: winner, name: station.label, trust: station.identityTrust,
                                          authenticator: station.authenticator, clock: SystemLinkClock(),
                                          transportFactory: station.transportFactory)
        #expect(!adopted)
        #expect(model.session == nil)
        #expect(model.currentPathRank == nil)
        #expect(model.mediaOwnerForTesting == nil)
        #expect(station.messages.filter { $0.kind == .authRequest }.isEmpty)
        model.winnerAvailabilityCheckedForTesting = nil
    }

    @Test("expected transmit silence follows only fresh authenticated transmit state")
    func expectedTransmitSilenceUsesFreshMirror() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.remoteTx])
        let model = AppModel()
        await connect(model, to: station)
        #expect(await settle { model.connection == .connected })
        #expect(!model.expectedTransmitSilence())
        let capabilities = model.mirror.capabilities.map {
            LinkMessage.PropertyEntry(name: $0.key, value: $0.value.wireValue)
        } + [.init(name: "txStateVersion", value: .i64(1))]
        await station.deliver(.capabilities(.init(properties: capabilities)))
        await station.deliver(.objectCreate(.init(key: "txState", className: "TxState", properties: [
            .init(name: "keyed", value: .bool(true)),
        ])))
        #expect(await settle { model.expectedTransmitSilence() })
        await station.deliver(.delta(.init(key: "txState", properties: [
            .init(name: "keyed", value: .bool(false)),
            .init(name: "tuning", value: .bool(true)),
        ])))
        #expect(await settle { model.expectedTransmitSilence() })
        await station.deliver(.delta(.init(key: "txState", properties: [
            .init(name: "tuning", value: .bool(false)),
            .init(name: "txEnding", value: .bool(true)),
        ])))
        #expect(await settle { model.expectedTransmitSilence() })
        await station.deliver(.delta(.init(key: "txState", properties: [
            .init(name: "txEnding", value: .bool(false)),
        ])))
        #expect(await settle { !model.expectedTransmitSilence() })
        await model.disconnect()
        #expect(!model.expectedTransmitSilence())
    }

    @Test("path ticket accepts only the Core's ordinal-zero UTF-8 secret and ordinal-one i64 expiry")
    func typedPathTicket() throws {
        let secret = Base64URL.encode(Data(repeating: 0x5a, count: 32))
        func answer(_ entries: [LinkMessage.PropertyEntry], accepted: Bool = true) -> CommandResult {
            CommandResult(.init(verb: "session.pathTicket", id: 1, accepted: accepted,
                                reason: "", affected: [], values: entries))
        }
        let correct: [LinkMessage.PropertyEntry] = [
            .init(ordinal: 0, name: "ticket", value: .utf8(secret)),
            .init(ordinal: 1, name: "expiresInMs", value: .i64(10_000)),
        ]
        #expect(try AppModel.pathTicket(from: answer(correct)).expiresInMs == 10_000)
        #expect(throws: AppModel.RouteMoveError.self) {
            try AppModel.pathTicket(from: answer(correct, accepted: false))
        }
        #expect(throws: AppModel.RouteMoveError.self) {
            try AppModel.pathTicket(from: answer([
                .init(ordinal: 1, name: "ticket", value: .utf8(secret)), correct[1],
            ]))
        }
        #expect(throws: AppModel.RouteMoveError.self) {
            try AppModel.pathTicket(from: answer([
                correct[0], .init(ordinal: 1, name: "expiresInMs", value: .utf8("10000")),
            ]))
        }
    }

    @Test("move safety reads fresh keyed, tuning, and VOX mirror values before the UI refresh")
    func moveSafetyUsesCurrentMirror() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.remoteTx])
        let model = AppModel()
        await connect(model, to: station)
        #expect(await settle { model.connection == .connected })
        let session = try #require(model.session)
        let capabilities = model.mirror.capabilities.map {
            LinkMessage.PropertyEntry(name: $0.key, value: $0.value.wireValue)
        } + [.init(name: "controlSwitchVersion", value: .i64(1))]
        await station.deliver(.capabilities(.init(properties: capabilities)))
        #expect(await settle { model.supportsControlMove(on: session) })
        model.mirror.apply(.objectCreate(.init(key: "txState", className: "TxState",
                                                properties: [.init(name: "keyed", value: .bool(false))])))
        #expect(await settle { model.safeToMoveControl(session) })

        model.mirror.apply(.objectCreate(.init(key: "txState", className: "TxState",
                                                properties: [.init(name: "keyed", value: .bool(true))])))
        #expect(!model.main.transmit.report.keyed)
        #expect(!model.safeToMoveControl(session))

        model.mirror.apply(.delta(.init(key: "txState", properties: [
            .init(name: "keyed", value: .bool(false)),
            .init(name: "tuning", value: .bool(true)),
        ])))
        #expect(!model.safeToMoveControl(session))
        model.mirror.apply(.delta(.init(key: "txState", properties: [
            .init(name: "tuning", value: .bool(false)),
        ])))
        model.mirror.apply(.delta(.init(key: "transmit", properties: [
            .init(name: "voxEnabled", value: .bool(true)),
        ])))
        #expect(!model.safeToMoveControl(session))
        model.mirror.apply(.delta(.init(key: "transmit", properties: [
            .init(name: "voxEnabled", value: .bool(false)),
        ])))
        model.mirror.apply(.delta(.init(key: "radio", properties: [
            .init(name: "transmitting", value: .bool(true)),
        ])))
        #expect(!model.safeToMoveControl(session))
        await model.disconnect()
    }

    @Test("an app-owned route move uses one ticket and keeps one authenticated snapshot")
    func appMoveKeepsOneAuthenticationAndSnapshot() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let oldEndpoint = StationEndpoint(host: "198.51.100.8")
        let newEndpoint = StationEndpoint(host: "127.0.0.1")
        let old = TicketAnsweringOldTransport(inner: station.transportFactory(oldEndpoint, station.identityTrust))
        let oldLease = PreauthenticatedTransport(old)
        _ = try await oldLease.inspect(clock: SystemLinkClock(), deadline: .seconds(5))
        let suite = "AppModelMoveLongSession-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let phone = PhoneSettings(defaults: defaults)
        phone.sleepTimer = .thirtyMinutes
        let sessionClock = SessionClock(Date(timeIntervalSince1970: 1_790_538_120))
        let traffic = TrafficCounter()
        let model = AppModel(phoneSettings: phone,
                             sessionSources: longSessionSources(clock: sessionClock, traffic: traffic))
        let first = PathRacer.Winner(transport: oldLease, route: .address(oldEndpoint), path: .direct,
                                     mediaIce: nil, row: 0, rank: .otherWebSocket)
        #expect(await model.connect(winner: first, name: station.label, trust: station.identityTrust,
                                    authenticator: station.authenticator, clock: SystemLinkClock(),
                                    transportFactory: station.transportFactory))
        #expect(await settle { model.connection == .connected })
        let originalDeadline = model.longSession.sleepTimer.endsAt
        traffic.sent(25)
        await model.longSession.tick()
        #expect(model.longSession.meter.session.bytesOut == 25)
        let session = try #require(model.session)
        let capabilities = model.mirror.capabilities.map {
            LinkMessage.PropertyEntry(name: $0.key, value: $0.value.wireValue)
        } + [.init(name: "controlSwitchVersion", value: .i64(1))]
        await station.deliver(.capabilities(.init(properties: capabilities)))
        #expect(await settle { model.safeToMoveControl(session) })

        var observedHostOnNewMessage: String?
        var observedRankOnNewMessage: PathRacer.Rank?
        model.safetyMirrorChanged = { _ in
            guard model.mirror.object("txState") != nil else { return }
            observedHostOnNewMessage = model.coreHost
            observedRankOnNewMessage = model.currentPathRank
        }

        let digest = Data(repeating: 0x93, count: 32)
        let hello = LinkMessage.Hello(major: 1, minor: model.mirror.agreedMinor ?? 11, settingsSchema: 0,
                                      peer: "nereusd", majors: [1],
                                      identity: try station.identity.claim(certificateSHA256: digest))
        let new = JoinedCandidateTransport(hello: hello, digest: digest, old: old,
                                           afterJoin: .objectCreate(.init(key: "txState", className: "TxState",
                                                                          properties: [])))
        let newLease = PreauthenticatedTransport(new)
        _ = try await newLease.inspect(clock: SystemLinkClock(), deadline: .seconds(5))
        let better = PathRacer.Winner(transport: newLease, route: .address(newEndpoint), path: .thisNetwork,
                                      mediaIce: nil, row: 1, rank: .localWebSocket)
        let outcome = await model.moveControl(to: better, on: session)
        guard case .moved = outcome else {
            Issue.record("the app did not commit the inspected route")
            await model.disconnect()
            return
        }
        #expect(old.ticketRequests == 1)
        #expect(station.messages.filter { $0.kind == .authRequest }.count == 1)
        #expect(new.messages.contains { if case .pathJoin = $0 { return true }; return false })
        #expect(!new.messages.contains { if case .authRequest = $0 { return true }; return false })
        #expect(model.mirror.isSnapshotComplete)
        #expect(model.coreHost == newEndpoint.host)
        #expect(model.currentPathRank == .localWebSocket)
        // The session hands NEW's held messages to its events only after the
        // route commit, and it resumes `moveControl` first: the app's event
        // pump handles the held txState on a later MainActor turn, so wait
        // for it rather than assume it ran before this line.
        #expect(await settle { observedHostOnNewMessage != nil })
        #expect(observedHostOnNewMessage == newEndpoint.host)
        #expect(observedRankOnNewMessage == .localWebSocket)
        let probe = await model.commands.start("move.probe", arguments: [], copies: 1,
                                               timeout: .seconds(10))
        await probe.sent()
        #expect(new.messages.contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "move.probe" }
            return false
        })
        traffic.received(40)
        await model.longSession.tick()
        #expect(model.longSession.sleepTimer.endsAt == originalDeadline)
        #expect(model.longSession.meter.session == DataUseMeter.Count(bytesIn: 40, bytesOut: 25))
        await model.disconnect()
    }

    @Test("old disconnect completion cannot end a newer listening intent")
    func oldDisconnectCannotEndNewLongSession() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let newer = try FakeStation(fixture: "session-device-sign-in")
        let suite = "AppModelOldLongSession-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let phone = PhoneSettings(defaults: defaults)
        phone.sleepTimer = .thirtyMinutes
        let clock = SessionClock(Date(timeIntervalSince1970: 1_790_538_120))
        let traffic = TrafficCounter()
        let model = AppModel(phoneSettings: phone,
                             sessionSources: longSessionSources(clock: clock, traffic: traffic))
        await connect(model, to: old)
        #expect(await settle { model.connection == .connected })
        let oldDeadline = try #require(model.longSession.sleepTimer.endsAt)
        let gate = StartGate()
        model.disconnectAfterSessionStopForTesting = { await gate.hold() }
        let oldDisconnect = Task { await model.disconnect() }
        for _ in 0..<50_000 {
            if await gate.entered { break }
            await Task.yield()
        }
        #expect(await gate.entered)
        model.disconnectAfterSessionStopForTesting = nil
        clock.now = clock.now.addingTimeInterval(60)
        await connect(model, to: newer)
        #expect(await settle { model.connection == .connected })
        let newSession = try #require(model.session)
        let newDeadline = try #require(model.longSession.sleepTimer.endsAt)
        #expect(newDeadline > oldDeadline)
        traffic.sent(13)
        await model.longSession.tick()
        await gate.release()
        await oldDisconnect.value
        #expect(model.session === newSession)
        #expect(model.longSession.sleepTimer.endsAt == newDeadline)
        #expect(model.longSession.meter.session.bytesOut == 13)
        await model.disconnect()
    }

    @Test("connecting to a Core fills the mirror and settings and ends connected")
    func connectsToFakeCore() async throws {
        let station = try FakeStation()
        let model = AppModel()
        await connect(model, to: station)
        #expect(model.session != nil)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        #expect(model.mirror.isSnapshotComplete)
        #expect(model.mirror.object("radio")?.className == "RadioModel")
        #expect(model.mirror.objects(ofClass: "SliceModel").map(\.key) == ["slice:0"])
        #expect(model.settings.values.isEmpty)
        await model.disconnect()
    }

    @Test("a write from a screen reaches the Core through the model's session")
    func writesReachTheCore() async throws {
        let station = try FakeStation()
        let model = AppModel()
        await connect(model, to: station)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })

        let write = Task { await model.mirror.write("slice:0", property: "frequency", value: .double(7_074_000)) }
        let sent = await station.waitForMessage { message in
            if case .propertyWrite(let write) = message {
                return write.key == "slice:0" && write.properties.first?.name == "frequency"
            }
            return false
        }
        // This Core answers writes with property.result, by the write's id.
        guard case .propertyWrite(let request)? = sent, let writeId = request.writeId else {
            Issue.record("the write did not reach the Core with a write id: \(String(describing: sent))")
            await model.disconnect()
            return
        }
        let kept = LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(7_074_000))
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: writeId, results: [
            LinkMessage.PropertyResult.Result(property: "frequency", accepted: true, reason: "", value: kept),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [kept])))
        let outcome = await write.value
        #expect(outcome.accepted)
        #expect(outcome.answeredByCore)
        await model.disconnect()
    }

    @Test("disconnecting leaves no session and the mirror stale")
    func disconnects() async throws {
        let station = try FakeStation()
        let model = AppModel()
        await connect(model, to: station)
        #expect(await settle { model.connection == .connected })
        await model.disconnect()
        #expect(model.session == nil)
        #expect(model.connection == .notConnected)
        #expect(model.mirror.isStale)
    }

    @Test("a lost link waits to retry")
    func lostLinkRetries() async throws {
        let station = try FakeStation()
        let model = AppModel()
        await connect(model, to: station)
        #expect(await settle { model.connection == .connected })
        await station.dropLink()
        let retrying = await settle {
            if case .waitingToRetry = model.connection {
                return true
            }
            return false
        }
        #expect(retrying)
        await model.disconnect()
    }

    @Test("a refused sign-in shows the Core's reason")
    func refusedSignIn() async throws {
        let station = try FakeStation()
        let suite = "AppModelRefusedLongSession-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let phone = PhoneSettings(defaults: defaults)
        phone.sleepTimer = .thirtyMinutes
        let clock = SessionClock(Date(timeIntervalSince1970: 1_790_538_120))
        let traffic = TrafficCounter()
        let model = AppModel(phoneSettings: phone,
                             sessionSources: longSessionSources(clock: clock, traffic: traffic))
        await model.connect(to: station.endpoint, trust: station.trust,
                            authenticator: TokenAuthenticator(token: "not-the-token"),
                            transportFactory: station.transportFactory)
        let refused = await settle {
            model.connection == .refused(Refusal(.authentication(FakeStation.wrongTokenReason)))
        }
        #expect(refused)
        #expect(model.longSession.sleepTimer.endsAt == nil)
        let endedCount = model.longSession.meter.session
        traffic.sent(99)
        await model.longSession.tick()
        #expect(model.longSession.meter.session == endedCount)
        await model.disconnect()
        #expect(model.connection == .notConnected)
    }

    @Test("the band plays while connected and stops when the session ends")
    func soundFollowsTheSession() async throws {
        let station = try FakeStation()
        let output = FakePlaybackOutput()
        let audio = AudioSessionController(session: FakeAudioSession(), output: output,
                                           notificationCenter: NotificationCenter())
        let model = AppModel(audio: audio)
        #expect(audio.state == .stopped)
        await connect(model, to: station)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        #expect(await settle { audio.state == .playing })
        await audio.settle()
        #expect(output.isRunning)
        await model.disconnect()
        #expect(audio.state == .stopped)
        #expect(output.stops == 1)
    }

    /// Wait for the actual published predicate, retaining a match that arrives
    /// before iteration starts. The bound ends a missing event, not readiness.
    private func heartbeatEvent(_ predicate: AnyPublisher<Bool, Never>,
                                within limit: Duration = .seconds(30)) async -> Bool {
        guard !Task.isCancelled else { return false }
        let (events, sink) = AsyncStream.makeStream(of: Bool.self, bufferingPolicy: .bufferingNewest(1))
        let watch = predicate.filter { @Sendable value in value }.sink { @Sendable _ in
            sink.yield(true)
            sink.finish()
        }
        let timeout = Task {
            do {
                try await Task.sleep(for: limit)
            } catch {
                return
            }
            sink.finish()
        }
        defer {
            timeout.cancel()
            watch.cancel()
            sink.finish()
        }
        var iterator = events.makeAsyncIterator()
        let observed = await iterator.next()
        return !Task.isCancelled && observed == true
    }

    private func relayedHeartbeatReady(_ model: AppModel) async -> Bool {
        await heartbeatEvent(model.$connection.combineLatest(model.$linkRelayed)
            .map { @Sendable pair in pair.0 == .connected && pair.1 }.eraseToAnyPublisher())
    }

    private func heartbeatMeasured(_ model: AppModel) async -> Bool {
        await heartbeatEvent(model.$roundTripMs.map { @Sendable value in value != nil }.eraseToAnyPublisher())
    }

    @Test("the link chip shows no number until the first heartbeat is answered")
    func noRoundTripUntilMeasured() async throws {
        let station = try FakeStation()
        let clock = TestLinkClock()
        let model = AppModel()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: clock)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        // Connected, and no ping has gone yet: the chip shows the dot alone.
        #expect(model.roundTripMs == nil)
        #expect(LinkState(connection: model.connection, roundTripMs: model.roundTripMs).text == nil)

        // The first heartbeat, 20 s in, is answered by the fake at once.
        await clock.advance(by: 20_000)
        #expect(await settle { model.roundTripMs != nil })
        let measured = try #require(model.roundTripMs)
        #expect(measured >= 1)
        #expect(LinkState(connection: model.connection, roundTripMs: measured).text == "\(measured) ms")
        await model.disconnect()
    }

    @Test("selected TURN and web relay control use the two-second heartbeat", arguments: [3, 4])
    func relayedControlAcceleratesHeartbeat(rank: Int) async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let route = RankedServiceRoute(station: station, rank: rank)
        let clock = TestLinkClock()
        let model = AppModel()
        await model.connect(through: route, name: station.label, trust: station.identityTrust,
                            authenticator: station.authenticator, clock: clock)
        #expect(await relayedHeartbeatReady(model))
        #expect(model.roundTripMs == nil)
        await clock.advance(by: 1_999)
        #expect(model.roundTripMs == nil)
        await clock.advance(by: 1)
        #expect(await heartbeatMeasured(model))
        await model.disconnect()
    }

    @Test("relayed heartbeat waits for held snapshot completion before advancing its clock", arguments: [3, 4])
    func snapshotCompletionWaitsBeforeRelayedHeartbeat(rank: Int) async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let snapshot = HeartbeatEventGate()
        let route = RankedServiceRoute(station: station, rank: rank, snapshot: snapshot)
        let clock = TestLinkClock()
        let model = AppModel()
        do {
            await model.connect(through: route, name: station.label, trust: station.identityTrust,
                                authenticator: station.authenticator, clock: clock)
            try #require(await heartbeatEvent(snapshot.entered))
            let readiness = Task { await relayedHeartbeatReady(model) }
            // A control probe proves the old finite budget is insufficient
            // while the actual snapshot completion remains held.
            #expect(!(await settle { model.connection == .connected && model.linkRelayed }))
            #expect(clock.now == 0)
            #expect(clock.pendingDueTimes.contains(20_000))
            #expect(model.roundTripMs == nil)
            snapshot.release()
            #expect(await readiness.value)
            #expect(model.connection == .connected && model.linkRelayed)
            #expect(clock.now == 0)
            #expect(clock.pendingDueTimes.contains(2_000))
            #expect(model.roundTripMs == nil)
            await clock.advance(by: 1_999)
            #expect(model.roundTripMs == nil)
            await clock.advance(by: 1)
            #expect(await heartbeatMeasured(model))
            #expect(model.roundTripMs != nil)
        } catch {
            snapshot.cancel()
            await model.disconnect()
            throw error
        }
        snapshot.cancel()
        await model.disconnect()
    }

    @Test("relayed heartbeat measurement waits for the actual held pong completion", arguments: [3, 4])
    func pongCompletionWaitsForRoundTrip(rank: Int) async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let pong = HeartbeatEventGate()
        let route = RankedServiceRoute(station: station, rank: rank, pong: pong)
        let clock = TestLinkClock()
        let model = AppModel()
        do {
            await model.connect(through: route, name: station.label, trust: station.identityTrust,
                                authenticator: station.authenticator, clock: clock)
            try #require(await relayedHeartbeatReady(model))
            #expect(model.connection == .connected && model.linkRelayed)
            #expect(model.roundTripMs == nil)
            await clock.advance(by: 1_999)
            #expect(model.roundTripMs == nil)
            await clock.advance(by: 1)
            try #require(await heartbeatEvent(pong.entered))
            #expect(clock.now == 2_000)
            // The heartbeat was submitted, but the session has not yet
            // received its answer. Yield counts cannot complete that event.
            #expect(!(await settle { model.roundTripMs != nil }))
            #expect(model.roundTripMs == nil)
            let measured = Task { await heartbeatMeasured(model) }
            pong.release()
            #expect(await measured.value)
            #expect(model.roundTripMs != nil)
            #expect(clock.now == 2_000)
        } catch {
            pong.cancel()
            await model.disconnect()
            throw error
        }
        pong.cancel()
        await model.disconnect()
    }

    @Test("a selected media tunnel accelerates direct control's heartbeat")
    func selectedTunnelAcceleratesHeartbeat() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.wideband])
        let clock = TestLinkClock()
        let model = AppModel(mediaPeerFactory: { TunnelReportingPeer(inner: station.mediaPeerFactory()) })
        await model.connect(to: station.endpoint, trust: station.identityTrust,
                            authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: clock)
        #expect(await settle { model.connection == .connected })
        #expect(await model.media.selectedTunnel)
        #expect(model.roundTripMs == nil)
        await clock.advance(by: 1_999)
        #expect(model.roundTripMs == nil)
        await clock.advance(by: 1)
        #expect(await settle { model.roundTripMs != nil })
        await model.disconnect()
    }

    /// The media connections the fake Core was asked to start, in order.
    private func mediaStarts(_ station: FakeStation) -> [String] {
        station.messages.compactMap { message in
            guard case .mediaControl(let control) = message, control.payload["op"] == .string("start"),
                  case .string(let id)? = control.payload["connectionId"] else {
                return nil
            }
            return id
        }
    }

    @Test("a network change checks ready control without restarting healthy media")
    func networkChangeWhileConnected() async throws {
        let station = try FakeStation(additions: .wideband)
        let clock = TestLinkClock()
        let model = AppModel(mediaPeerFactory: station.mediaPeerFactory)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: clock)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        #expect(await settle { mediaStarts(station).count == 1 })
        #expect(model.roundTripMs == nil)

        await model.retryNow()
        // A ping went out at once, not at the heartbeat's first tick, and
        // the fake answered it.
        #expect(await settle { model.roundTripMs != nil })
        #expect(mediaStarts(station).count == 1)
        #expect(await model.media.connectionId == mediaStarts(station).first)
        #expect(model.connection == .connected)
        #expect(station.connectionCount == 1)
        let owner = try #require(model.mediaOwnerForTesting)
        await model.disconnect()
        #expect(!(await model.media.usePeers({ MediaPeer() },
                                             connectDeadline: MediaControlClient.connectDeadline,
                                             owner: owner)))
    }

    @Test("a round trip under a millisecond reads 1 ms, never 0 ms")
    func subMillisecondRoundTrip() {
        #expect(AppModel.shownMilliseconds(.zero) == 1)
        #expect(AppModel.shownMilliseconds(.microseconds(300)) == 1)
        #expect(AppModel.shownMilliseconds(.microseconds(38_200)) == 39)
        #expect(AppModel.shownMilliseconds(.milliseconds(38)) == 38)
    }

    @Test("with no Core, a write is not sent")
    func writeWithoutCore() async {
        let model = AppModel()
        let outcome = await model.mirror.write("slice:0", property: "frequency", value: .double(1))
        #expect(!outcome.accepted)
        #expect(!outcome.answeredByCore)
    }
}

/// Holds an inbound event without changing it. Entry is a retained current
/// value; release before hold is valid. Close/cancellation resumes every hold.
private final class HeartbeatEventGate: @unchecked Sendable {
    private enum State { case held, released, cancelled }
    private let lock = NSLock()
    private var state: State = .held
    private var waiting: [UUID: CheckedContinuation<Bool, Never>] = [:]
    private let entry = CurrentValueSubject<Bool, Never>(false)

    var entered: AnyPublisher<Bool, Never> { entry.eraseToAnyPublisher() }

    func hold() async -> Bool {
        let id = UUID()
        return await withTaskCancellationHandler {
            await withCheckedContinuation { continuation in
                let immediate: Bool? = lock.withLock {
                    if Task.isCancelled { return false }
                    switch state {
                    case .released:
                        return true
                    case .cancelled:
                        return false
                    case .held:
                        waiting[id] = continuation
                        return nil
                    }
                }
                entry.send(true)
                if let immediate { continuation.resume(returning: immediate) }
            }
        } onCancel: {
            self.cancel()
        }
    }

    func release() {
        let continuations: [CheckedContinuation<Bool, Never>] = lock.withLock {
            guard case .held = state else { return [] }
            state = .released
            let continuations = Array(waiting.values)
            waiting.removeAll()
            return continuations
        }
        for continuation in continuations { continuation.resume(returning: true) }
    }

    func cancel() {
        let continuations: [CheckedContinuation<Bool, Never>] = lock.withLock {
            state = .cancelled
            let continuations = Array(waiting.values)
            waiting.removeAll()
            return continuations
        }
        for continuation in continuations { continuation.resume(returning: false) }
    }
}

/// Forwards the same fake-station protocol/authentication/metadata. Only the
/// selected inbound completion is held; native media configuration is intact.
private final class HeartbeatGatedTransport: LinkTransport, @unchecked Sendable {
    private let inner: any LinkTransport
    private let snapshot: HeartbeatEventGate?
    private let pong: HeartbeatEventGate?
    private let lock = NSLock()
    private var closed = false

    init(inner: any LinkTransport, snapshot: HeartbeatEventGate?, pong: HeartbeatEventGate?) {
        self.inner = inner
        self.snapshot = snapshot
        self.pong = pong
    }

    private var isClosed: Bool { lock.withLock { closed } }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        do {
            return try await inner.open { [self] event in
                guard !isClosed, !Task.isCancelled else { return }
                if case .text(let text) = event,
                   let message = try? LinkCodec.decode(text), case .snapshotComplete = message,
                   let snapshot, !(await snapshot.hold()) {
                    return
                }
                if case .pong = event, let pong, !(await pong.hold()) {
                    return
                }
                guard !isClosed, !Task.isCancelled else { return }
                await onEvent(event)
            }
        } catch {
            snapshot?.cancel()
            pong?.cancel()
            throw error
        }
    }

    @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
    func ping() { inner.ping() }
    func close() {
        lock.withLock { closed = true }
        snapshot?.cancel()
        pong?.cancel()
        inner.close()
    }
    func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) { inner.setBinaryReceiver(receiver) }
    @discardableResult func sendBinary(_ frame: Data) -> Bool { inner.sendBinary(frame) }
    @discardableResult func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool {
        inner.sendBinary(frame, ownership: ownership)
    }
    func discardBinary(ownership: BinaryMediaOwnership) { inner.discardBinary(ownership: ownership) }
    var boundsItsOwnOpening: Bool { inner.boundsItsOwnOpening }
    var selectedRouteObservation: SelectedRouteObservation { inner.selectedRouteObservation }
    var diagnosticServiceRank: Int? { inner.diagnosticServiceRank }
    var trafficObservation: LinkTrafficObservation? { inner.trafficObservation }
}

private final class RankedServiceRoute: CoreServiceRoute, @unchecked Sendable {
    let station: FakeStation
    let rank: Int
    private let snapshot: HeartbeatEventGate?
    private let pong: HeartbeatEventGate?

    init(station: FakeStation, rank: Int, snapshot: HeartbeatEventGate? = nil,
         pong: HeartbeatEventGate? = nil) {
        self.station = station
        self.rank = rank
        self.snapshot = snapshot
        self.pong = pong
    }

    func makeTransport() -> any SessionTransport {
        let inner = station.transportFactory(station.endpoint, station.identityTrust)
        guard snapshot != nil || pong != nil else { return inner }
        return HeartbeatGatedTransport(inner: inner, snapshot: snapshot, pong: pong)
    }
    func mediaIceSettings() -> IceSettings? { nil }
    var selectedPathRank: Int? { rank }
    var lastTry: ConnectionAttempt.Try? { nil }
    var lastError: RendezvousDialError? { nil }
}

private final class TunnelReportingPeer: MediaPeerConnection, @unchecked Sendable {
    private let inner: any MediaPeerConnection
    init(inner: any MediaPeerConnection) { self.inner = inner }
    var localDescription: AsyncStream<String> { inner.localDescription }
    var localCandidates: AsyncStream<(candidate: String, mid: String)> { inner.localCandidates }
    var audioPackets: AsyncStream<RtpPacket> { inner.audioPackets }
    var displayDatagrams: AsyncStream<Data> { inner.displayDatagrams }
    var state: AsyncStream<MediaPeer.State> { inner.state }
    var selectedTunnel: Bool { true }
    var selectedRelayOrTunnel: Bool { true }
    func prepare(connectionId: UUID, gates: MediaFeatureGates) async throws {
        try await inner.prepare(connectionId: connectionId, gates: gates)
    }
    func setRemoteDescription(_ sdp: String) throws { try inner.setRemoteDescription(sdp) }
    func addRemoteCandidate(_ candidate: String, mid: String) throws { try inner.addRemoteCandidate(candidate, mid: mid) }
    func setExpectedAudioSsrc(_ ssrc: UInt32?) { inner.setExpectedAudioSsrc(ssrc) }
    func enableMicrophoneLine(ssrc: UInt32) { inner.enableMicrophoneLine(ssrc: ssrc) }
    var microphoneLineReady: Bool { inner.microphoneLineReady }
    var txChannelReady: Bool { inner.txChannelReady }
    func sendMicrophone(_ packet: RtpPacket) throws { try inner.sendMicrophone(packet) }
    func sendTx(_ message: Data) throws { try inner.sendTx(message) }
    func close() { inner.close() }
}

final class TicketAnsweringOldTransport: LinkTransport, @unchecked Sendable {
    private let inner: any LinkTransport
    private let lock = NSLock()
    private var recipient: (@Sendable (LinkTransportEvent) async -> Void)?
    private var requests = 0
    private var refused = false
    private let offersSwitch: Bool
    private let ticket = Base64URL.encode(Data(repeating: 0x5a, count: 32))

    init(inner: any LinkTransport, offersSwitch: Bool = false) {
        self.inner = inner
        self.offersSwitch = offersSwitch
    }
    var ticketRequests: Int { lock.withLock { requests } }
    func refuseTickets(_ refused: Bool) { lock.withLock { self.refused = refused } }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        lock.withLock { recipient = onEvent }
        return try await inner.open { event in
            if self.offersSwitch, case .text(let text) = event,
               let message = try? LinkCodec.decode(text), case .capabilities(let capabilities) = message {
                let existing = capabilities.properties.filter { $0.name != "controlSwitchVersion" }
                let offered = LinkMessage.capabilities(.init(properties: existing + [
                    .init(name: "controlSwitchVersion", value: .i64(1)),
                ]))
                await onEvent(.text(LinkCodec.encode(offered)))
            } else {
                await onEvent(event)
            }
        }
    }

    @discardableResult func send(_ text: String) -> Bool {
        if let message = try? LinkCodec.decode(text), case .commandInvoke(let invoke) = message,
           invoke.verb == "session.pathTicket" {
            let refused = lock.withLock { () -> Bool in requests += 1; return self.refused }
            let result = LinkMessage.commandResult(.init(verb: invoke.verb, id: invoke.id,
                accepted: !refused, reason: refused ? "wait for transmit to settle" : "", affected: [], values: refused ? nil : [
                    .init(ordinal: 0, name: "ticket", value: .utf8(ticket)),
                    .init(ordinal: 1, name: "expiresInMs", value: .i64(10_000)),
                ]))
            Task { await deliver(result) }
            return true
        }
        return inner.send(text)
    }

    func deliver(_ message: LinkMessage) async {
        await lock.withLock { recipient }?(.text(LinkCodec.encode(message)))
    }

    func ping() { inner.ping() }
    func close() { inner.close() }
}

final class JoinedCandidateTransport: LinkTransport, @unchecked Sendable {
    private let hello: LinkMessage.Hello
    private let digest: Data
    private let old: TicketAnsweringOldTransport
    private let beforeHello: (@Sendable () async -> Void)?
    private let afterJoin: LinkMessage?
    private let lock = NSLock()
    private var sent: [LinkMessage] = []
    private var closed = false
    private var recipient: (@Sendable (LinkTransportEvent) async -> Void)?

    init(hello: LinkMessage.Hello, digest: Data, old: TicketAnsweringOldTransport,
         beforeHello: (@Sendable () async -> Void)? = nil, afterJoin: LinkMessage? = nil) {
        self.hello = hello
        self.digest = digest
        self.old = old
        self.beforeHello = beforeHello
        self.afterJoin = afterJoin
    }

    var messages: [LinkMessage] { lock.withLock { sent } }
    var isClosed: Bool { lock.withLock { closed } }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        lock.withLock { recipient = onEvent }
        await beforeHello?()
        await onEvent(.text(LinkCodec.encode(.hello(hello))))
        return digest
    }

    @discardableResult func send(_ text: String) -> Bool {
        guard let message = try? LinkCodec.decode(text) else { return false }
        let admitted = lock.withLock { () -> Bool in
            guard !closed else { return false }
            sent.append(message)
            return true
        }
        guard admitted else { return false }
        if case .pathJoin = message {
            Task {
                if let afterJoin, let recipient = lock.withLock({ recipient }) {
                    await recipient(.text(LinkCodec.encode(afterJoin)))
                }
                await old.deliver(.pathSwitch)
            }
        }
        return true
    }

    func ping() {}
    func close() { lock.withLock { closed = true } }
}

private struct AdmissionHeldTransmit: TransmitCommandSending {
    let base: TransmitCommandClient
    let holdVerb: @Sendable (TransmitVerb) async -> Void
    func send(_ verb: TransmitVerb, copies: Int) async -> TransmitPending {
        await holdVerb(verb)
        return await base.send(verb, copies: copies)
    }
    func send(_ verb: TransmitVerb, copies: Int, authority: CommandSendPermit) async -> TransmitPending {
        await holdVerb(verb)
        return await base.send(verb, copies: copies, authority: authority)
    }
    func sendKeepalive(sequence: Int64, epoch: Int64) async {}
}

private final class HandoffCounter: @unchecked Sendable {
    private let lock = NSLock()
    private var value = 0
    func next() -> Int { lock.withLock { value += 1; return value } }
}

private final class MadeDirectPeers: @unchecked Sendable {
    struct Made {
        let configuration: MediaPeer.Configuration
        let tunnelled: Bool
    }
    private let lock = NSLock()
    private var made: [Made] = []
    func add(_ configuration: MediaPeer.Configuration, tunnelled: Bool) {
        lock.withLock { made.append(Made(configuration: configuration, tunnelled: tunnelled)) }
    }
    var all: [Made] { lock.withLock { made } }
}
