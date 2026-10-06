// NereusSDR for iOS: one libdatachannel peer connection over its C API
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
internal import CDataChannel
import NereusLink
import os

/// What libdatachannel reports for a peer connection and its channels. The
/// callbacks run on the library's own threads.
enum RtcEvent: Sendable {
    case localDescription(sdp: String, type: String)
    case localCandidate(candidate: String, mid: String)
    case peerState(RtcPeerState)
    /// The remote peer opened a data channel.
    case dataChannel(id: Int32)
    /// The remote description brought a media line.
    case track(id: Int32)
    case open(id: Int32)
    case closed(id: Int32)
    case error(id: Int32, message: String)
    /// A binary message on a data channel, or a packet on a track.
    case message(id: Int32, data: Data)
    /// A text message, which no media line or channel carries.
    case textMessage(id: Int32)
    /// Every local candidate has been reported.
    case gatheringComplete
}

/// libdatachannel's peer connection states (`rtcState`).
enum RtcPeerState: Int32, Sendable {
    case new = 0, connecting, connected, disconnected, failed, closed
}

/// The peer connection settings the bridge passes to libdatachannel.
struct RtcPeerSettings: Sendable {
    var iceServers: [String]
    var mtu: Int
    var maxMessageSize: Int
    var enableIceTcp: Bool
    var forceMediaTransport: Bool
    var disableAutoNegotiation: Bool
    /// Gathering waits for ``RtcBridge/gatherLocalCandidates(relayServers:)``
    /// (ios/patches/libdatachannel/0002).
    var disableAutoGathering = false
}

/// One libdatachannel peer connection, driven through the C API
/// (`rtc/rtc.h`). The bridge owns the connection and every channel and
/// track the library hands it, and deletes them all on ``close()``. The
/// library's callbacks find the bridge through a token held in a registry,
/// never through a pointer to the bridge, so a callback that runs while or
/// after the bridge closes finds nothing and does nothing.
final class RtcBridge: @unchecked Sendable {
    /// The id of the peer connection in libdatachannel.
    let peer: Int32

    private let token: Int
    private let onEvent: @Sendable (RtcEvent) -> Void

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var children: [Int32] = []
    private var isClosed = false
    /// What each channel and track carries, for the traffic counter's
    /// kinds; one not named here counts as ``TrafficCounter/Kind/other``.
    /// Its own lock, never held across a library call, since the library's
    /// threads read it on every message.
    private let trafficKinds = OSAllocatedUnfairLock(initialState: [Int32: TrafficCounter.Kind]())

    /// Applies the SCTP buffer sizes to libdatachannel's process-wide
    /// settings, once per process and before the first peer connection:
    /// the library applies them to connections created afterwards.
    static func applySctpSettingsOnce(sendBufferBytes: Int, receiveBufferBytes: Int) {
        SctpSettingsOnce.shared.apply(send: sendBufferBytes, receive: receiveBufferBytes)
    }

    init(settings: RtcPeerSettings, lifetime: RtcPeerLifetime? = nil, onEvent: @escaping @Sendable (RtcEvent) -> Void) throws {
        self.onEvent = onEvent
        self.token = RtcRegistry.shared.reserve()

        var config = rtcConfiguration()
        config.certificateType = RTC_CERTIFICATE_DEFAULT
        config.iceTransportPolicy = RTC_TRANSPORT_POLICY_ALL
        config.enableIceTcp = settings.enableIceTcp
        config.enableIceUdpMux = false
        config.disableAutoNegotiation = settings.disableAutoNegotiation
        config.forceMediaTransport = settings.forceMediaTransport
        config.portRangeBegin = 0
        config.portRangeEnd = 0
        config.mtu = Int32(settings.mtu)
        config.maxMessageSize = Int32(settings.maxMessageSize)
        config.disableAutoGathering = settings.disableAutoGathering

        let servers: [UnsafeMutablePointer<CChar>?] = Self.iceServers(settings.iceServers).map { strdup($0) }
        defer {
            servers.forEach { free($0) }
        }
        var serverPointers: [UnsafePointer<CChar>?] = servers.map { $0.map { UnsafePointer($0) } }
        let created: Int32 = serverPointers.withUnsafeMutableBufferPointer { buffer in
            config.iceServers = buffer.isEmpty ? nil : buffer.baseAddress
            config.iceServersCount = Int32(buffer.count)
            guard let lifetime else { return rtcCreatePeerConnection(&config) }
            // The native call consumes this retain even when creation fails.
            // Its callback owns only completion state, never this bridge.
            let pointer = Unmanaged.passRetained(lifetime).toOpaque()
            return rtcCreatePeerConnectionWithLifetime(&config, { pointer in
                guard let pointer else { return }
                Unmanaged<RtcPeerLifetime>.fromOpaque(pointer).takeRetainedValue().finish()
            }, pointer)
        }
        guard created >= 0 else {
            RtcRegistry.shared.release(token)
            throw MediaPeerError.library(call: "rtcCreatePeerConnection", code: created)
        }
        self.peer = created
        RtcRegistry.shared.register(self, token: token)

        rtcSetUserPointer(peer, Self.pointer(for: token))
        rtcSetLocalDescriptionCallback(peer) { _, sdp, type, pointer in
            RtcBridge.deliver(pointer) {
                .localDescription(sdp: RtcBridge.string(sdp), type: RtcBridge.string(type))
            }
        }
        rtcSetLocalCandidateCallback(peer) { _, candidate, mid, pointer in
            RtcBridge.deliver(pointer) {
                .localCandidate(candidate: RtcBridge.string(candidate), mid: RtcBridge.string(mid))
            }
        }
        rtcSetStateChangeCallback(peer) { _, state, pointer in
            RtcBridge.deliver(pointer) {
                .peerState(RtcPeerState(rawValue: Int32(state.rawValue)) ?? .failed)
            }
        }
        rtcSetGatheringStateChangeCallback(peer) { _, state, pointer in
            guard state == RTC_GATHERING_COMPLETE else {
                return
            }
            RtcBridge.deliver(pointer) { .gatheringComplete }
        }
        rtcSetDataChannelCallback(peer) { _, channel, pointer in
            RtcBridge.adopt(channel, pointer: pointer)
            RtcBridge.deliver(pointer) { .dataChannel(id: channel) }
        }
        rtcSetTrackCallback(peer) { _, track, pointer in
            RtcBridge.adopt(track, pointer: pointer)
            RtcBridge.deliver(pointer) { .track(id: track) }
        }
    }

    deinit {
        close()
    }

    /// The ICE servers the peer connection is made with, within the pinned
    /// library's limits (``IceSettings``): the first STUN server only, since
    /// libdatachannel would pick among several at random, and the first two
    /// relay servers, all libjuice holds. Anything else is left out.
    static func iceServers(_ urls: [String]) -> [String] {
        var stun: String?
        var relays: [String] = []
        for url in urls {
            if url.hasPrefix("stun:") {
                stun = stun ?? url
            } else if url.hasPrefix("turn:"), relays.count < IceSettings.maxRelayServers {
                relays.append(url)
            }
        }
        return (stun.map { [$0] } ?? []) + relays
    }

    /// A relay URL's `user:password@` in `text`, wherever it appears.
    private static let credentialPattern = try? NSRegularExpression(pattern: #"(?i)(turns?:)[^@\s"']*@"#)

    /// `text` with every relay URL's credentials taken out: what may be
    /// shown or logged of an ICE server, or of a library message that
    /// quotes one (an unreadable URL is quoted whole when it is refused).
    static func withoutCredentials(_ text: String) -> String {
        guard let pattern = credentialPattern else {
            return text.contains("@") ? "<removed>" : text
        }
        let range = NSRange(text.startIndex..., in: text)
        return pattern.stringByReplacingMatches(in: text, range: range, withTemplate: "$1<removed>@")
    }

    // MARK: Signalling

    func setRemoteDescription(_ sdp: String, type: String) throws {
        try check("rtcSetRemoteDescription", rtcSetRemoteDescription(peer, sdp, type))
    }

    func setLocalDescription(type: String) throws {
        try check("rtcSetLocalDescription", rtcSetLocalDescription(peer, type))
    }

    func addRemoteCandidate(_ candidate: String, mid: String) throws {
        try check("rtcAddRemoteCandidate", rtcAddRemoteCandidate(peer, candidate, mid))
    }

    /// Starts gathering on a peer made with `disableAutoGathering`, once its
    /// local description is set, with `relayServers` (`turn:` URLs) added.
    func gatherLocalCandidates(relayServers: [String]) throws {
        let servers: [UnsafeMutablePointer<CChar>?] = relayServers.map { strdup($0) }
        defer {
            servers.forEach { free($0) }
        }
        var pointers: [UnsafePointer<CChar>?] = servers.map { $0.map { UnsafePointer($0) } }
        let result: Int32 = pointers.withUnsafeMutableBufferPointer { buffer in
            rtcGatherLocalCandidates(peer, buffer.isEmpty ? nil : buffer.baseAddress, Int32(buffer.count))
        }
        try check("rtcGatherLocalCandidates", result)
    }

    /// The fingerprint of the certificate the remote peer presented in the
    /// DTLS handshake, as the DTLS verifier recorded it from that
    /// certificate: `"<algorithm> <value>"`, nil before the handshake.
    func remoteFingerprint() -> String? {
        Self.readString { buffer, size in rtcGetRemoteFingerprint(peer, buffer, size) }
    }

    /// The candidate pair ICE selected, as candidate lines, once it has.
    func selectedCandidatePair() -> (local: String, remote: String)? {
        lock.withLock {
            guard !isClosed else { return nil }
            var local = [CChar](repeating: 0, count: 512)
            var remote = [CChar](repeating: 0, count: 512)
            let result = local.withUnsafeMutableBufferPointer { localBuffer in
                remote.withUnsafeMutableBufferPointer { remoteBuffer in
                    rtcGetSelectedCandidatePair(peer, localBuffer.baseAddress, Int32(localBuffer.count),
                                                remoteBuffer.baseAddress, Int32(remoteBuffer.count))
                }
            }
            guard result > 0, result <= local.count, result <= remote.count else { return nil }
            let text = { (buffer: [CChar]) in buffer.withUnsafeBufferPointer { String(cString: $0.baseAddress!) } }
            return (text(local), text(remote))
        }
    }

    // MARK: Channels and tracks

    /// A data channel's label, or nil when the library has no such channel.
    func label(ofChannel channel: Int32) -> String? {
        Self.readString { buffer, size in rtcGetDataChannelLabel(channel, buffer, size) }
    }

    /// Whether a data channel is unordered with no retransmissions.
    func isUnorderedWithoutRetransmissions(channel: Int32) -> Bool {
        var reliability = rtcReliability()
        guard rtcGetDataChannelReliability(channel, &reliability) == RTC_ERR_SUCCESS else {
            return false
        }
        return reliability.unordered && reliability.unreliable
            && reliability.maxPacketLifeTime == 0 && reliability.maxRetransmits == 0
    }

    /// A track's mid, or nil when the library has no such track.
    func mid(ofTrack track: Int32) -> String? {
        Self.readString { buffer, size in rtcGetTrackMid(track, buffer, size) }
    }

    /// A track's media description (its m-line and attributes).
    func description(ofTrack track: Int32) -> String? {
        Self.readString { buffer, size in rtcGetTrackDescription(track, buffer, size) }
    }

    func isOpen(_ id: Int32) -> Bool {
        rtcIsOpen(id)
    }

    /// Sends one binary message on a data channel, or one packet on a track.
    func send(_ data: Data, on id: Int32) throws {
        let result: Int32 = data.withUnsafeBytes { raw in
            rtcSendMessage(id, raw.bindMemory(to: CChar.self).baseAddress, Int32(raw.count))
        }
        try check("rtcSendMessage", result)
        TrafficCounter.shared.sent(data.count, as: trafficKind(of: id))
    }

    /// Counts what a channel or track carries as `kind` from now on.
    func countTraffic(on id: Int32, as kind: TrafficCounter.Kind) {
        trafficKinds.withLock { $0[id] = kind }
    }

    /// What a channel or track carries, as the traffic counter counts it.
    func trafficKind(of id: Int32) -> TrafficCounter.Kind {
        trafficKinds.withLock { $0[id] } ?? .other
    }

    /// Opens a data channel from this side (the offerer's role), listened to
    /// like the ones the remote peer opens. A nil `maxRetransmits` is a
    /// reliable channel.
    func createDataChannel(label: String, unordered: Bool, maxRetransmits: Int?) throws -> Int32 {
        var initial = rtcDataChannelInit()
        initial.reliability.unordered = unordered
        if let maxRetransmits {
            initial.reliability.unreliable = true
            initial.reliability.maxRetransmits = UInt32(maxRetransmits)
        }
        let channel = rtcCreateDataChannelEx(peer, label, &initial)
        try check("rtcCreateDataChannelEx", channel)
        Self.adopt(channel, bridge: self)
        return channel
    }

    /// Sets this side's description of a media line the remote description
    /// brought (libdatachannel's `addTrack` with that line's mid replaces
    /// the track's description rather than adding a line), before the
    /// answer is written. Returns the id the library gives this handle on
    /// the track, listened to like the others. Never call it from inside a
    /// library callback: the library holds its track table's lock there.
    func describeTrack(_ mediaDescription: String) throws -> Int32 {
        let track = rtcAddTrack(peer, mediaDescription)
        try check("rtcAddTrack", track)
        Self.adopt(track, bridge: self)
        return track
    }

    /// Stops listening to a channel or track the peer does not use and
    /// deletes it.
    func reject(_ id: Int32) {
        lock.withLock {
            children.removeAll { $0 == id }
        }
        trafficKinds.withLock { _ = $0.removeValue(forKey: id) }
        Self.silence(id)
        rtcDelete(id)
    }

    /// Unregisters the bridge, so no further event reaches it, then deletes
    /// every channel, track and the peer connection. Safe to call twice.
    func close() {
        let owned: [Int32]? = lock.withLock {
            guard !isClosed else {
                return nil
            }
            isClosed = true
            let owned = children
            children.removeAll()
            return owned
        }
        guard let owned else {
            return
        }
        RtcRegistry.shared.release(token)
        for id in owned {
            Self.silence(id)
            rtcDelete(id)
        }
        rtcDeletePeerConnection(peer)
    }

    // MARK: Callback plumbing

    private static func pointer(for token: Int) -> UnsafeMutableRawPointer? {
        UnsafeMutableRawPointer(bitPattern: token)
    }

    private static func bridge(for pointer: UnsafeMutableRawPointer?) -> RtcBridge? {
        guard let pointer else {
            return nil
        }
        return RtcRegistry.shared.bridge(for: Int(bitPattern: pointer))
    }

    private static func deliver(_ pointer: UnsafeMutableRawPointer?, _ make: () -> RtcEvent) {
        guard let bridge = bridge(for: pointer) else {
            return
        }
        bridge.onEvent(make())
    }

    private static func string(_ text: UnsafePointer<CChar>?) -> String {
        text.map { String(cString: $0) } ?? ""
    }

    /// Takes ownership of a channel or track the library created and listens
    /// to it. It inherits the peer connection's token as its user pointer.
    private static func adopt(_ id: Int32, pointer: UnsafeMutableRawPointer?) {
        guard let bridge = bridge(for: pointer) else {
            rtcDelete(id)
            return
        }
        adopt(id, bridge: bridge)
    }

    private static func adopt(_ id: Int32, bridge: RtcBridge) {
        let accepted: Bool = bridge.lock.withLock {
            guard !bridge.isClosed else {
                return false
            }
            bridge.children.append(id)
            return true
        }
        guard accepted else {
            rtcDelete(id)
            return
        }
        rtcSetOpenCallback(id) { id, pointer in
            RtcBridge.deliver(pointer) { .open(id: id) }
        }
        rtcSetClosedCallback(id) { id, pointer in
            RtcBridge.deliver(pointer) { .closed(id: id) }
        }
        rtcSetErrorCallback(id) { id, message, pointer in
            RtcBridge.deliver(pointer) { .error(id: id, message: RtcBridge.string(message)) }
        }
        rtcSetMessageCallback(id) { id, message, size, pointer in
            guard let bridge = RtcBridge.bridge(for: pointer) else {
                return
            }
            // A negative size marks a text message (libdatachannel's C API).
            guard size >= 0 else {
                bridge.onEvent(.textMessage(id: id))
                return
            }
            guard let message, size > 0 else {
                bridge.onEvent(.message(id: id, data: Data()))
                return
            }
            TrafficCounter.shared.received(Int(size), as: bridge.trafficKind(of: id))
            bridge.onEvent(.message(id: id, data: Data(bytes: message, count: Int(size))))
        }
        if rtcIsOpen(id) {
            bridge.onEvent(.open(id: id))
        }
    }

    private static func silence(_ id: Int32) {
        rtcSetOpenCallback(id, nil)
        rtcSetClosedCallback(id, nil)
        rtcSetErrorCallback(id, nil)
        rtcSetMessageCallback(id, nil)
    }

    /// Reads a string through a libdatachannel getter that copies into a
    /// buffer and returns the size it needs, NUL included.
    private static func readString(_ get: (UnsafeMutablePointer<CChar>?, Int32) -> Int32) -> String? {
        let needed = get(nil, 0)
        guard needed > 0 else {
            return nil
        }
        var buffer = [CChar](repeating: 0, count: Int(needed))
        let written = buffer.withUnsafeMutableBufferPointer { get($0.baseAddress, needed) }
        guard written > 0 else {
            return nil
        }
        return buffer.withUnsafeBufferPointer { String(cString: $0.baseAddress!) }
    }

    private func check(_ call: String, _ result: Int32) throws {
        guard result >= 0 else {
            throw MediaPeerError.library(call: call, code: result)
        }
    }
}

extension RtcBridge {
    /// Routes libdatachannel's log lines, informational and above, to
    /// `sink`; nil turns that route off again. The library logs nothing
    /// unless this or ``setConsoleLog(_:)`` is set. Process-wide.
    static func setLogSink(_ sink: (@Sendable (String) -> Void)?) {
        RtcLogSink.shared.set(sink)
    }

    /// A second, lasting route for the same lines: the device console
    /// (``MediaTransportLog``). Unlike ``setLogSink(_:)`` it stays on when
    /// that sink is turned off. Process-wide; the first call wins.
    static func setConsoleLog(_ console: @escaping @Sendable (String) -> Void) {
        RtcLogSink.shared.setConsole(console)
    }
}

/// Where libdatachannel's log goes, when anywhere.
private final class RtcLogSink: @unchecked Sendable {
    static let shared = RtcLogSink()

    private let lock = NSLock()
    private var sink: (@Sendable (String) -> Void)?
    private var console: (@Sendable (String) -> Void)?

    func set(_ sink: (@Sendable (String) -> Void)?) {
        lock.withLock {
            self.sink = sink
        }
        apply()
    }

    func setConsole(_ console: @escaping @Sendable (String) -> Void) {
        let first: Bool = lock.withLock {
            guard self.console == nil else {
                return false
            }
            self.console = console
            return true
        }
        if first {
            apply()
        }
    }

    private func apply() {
        let on = lock.withLock { sink != nil || console != nil }
        rtcInitLogger(on ? RTC_LOG_INFO : RTC_LOG_NONE) { _, message in
            RtcLogSink.shared.write(message.map { String(cString: $0) } ?? "")
        }
    }

    private func write(_ raw: String) {
        // No relay credentials leave here, whichever route the line takes.
        let line = RtcBridge.withoutCredentials(raw)
        let (sink, console) = lock.withLock { (self.sink, self.console) }
        sink?(line)
        console?(line)
    }
}

/// The live bridges by token, for the library's callbacks.
private final class RtcRegistry: @unchecked Sendable {
    static let shared = RtcRegistry()

    private let lock = NSLock()
    private var next = 1
    private var bridges: [Int: RtcBridge] = [:]

    func reserve() -> Int {
        lock.withLock {
            defer { next += 1 }
            return next
        }
    }

    func register(_ bridge: RtcBridge, token: Int) {
        lock.withLock {
            bridges[token] = bridge
        }
    }

    func release(_ token: Int) {
        let removed: RtcBridge? = lock.withLock {
            bridges.removeValue(forKey: token)
        }
        // Released outside the lock: dropping the last reference runs the
        // bridge's deinit.
        _ = removed
    }

    func bridge(for token: Int) -> RtcBridge? {
        lock.withLock { bridges[token] }
    }
}

/// libdatachannel's process-wide SCTP settings, applied once.
private final class SctpSettingsOnce: @unchecked Sendable {
    static let shared = SctpSettingsOnce()

    private let lock = NSLock()
    private var applied = false

    func apply(send: Int, receive: Int) {
        lock.withLock {
            guard !applied else {
                return
            }
            applied = true
            var settings = rtcSctpSettings()
            // Zero leaves every other field at libdatachannel's default.
            settings.sendBufferSize = Int32(send)
            settings.recvBufferSize = Int32(receive)
            rtcSetSctpSettings(&settings)
        }
    }
}

/// Optional native ownership barrier for a peer that uses external candidate
/// resources. Configuration and each agent retain the callback through deferred
/// teardown and TURN retirement. Handle deletion alone is not this barrier.
/// Safe to await after close, repeatedly, and from a cancelled Swift task.
final class RtcPeerLifetime: @unchecked Sendable {
    private let lock = NSLock()
    private var finished = false
    private var waiters: [CheckedContinuation<Void, Never>] = []

    func wait() async {
        await withCheckedContinuation { continuation in
            let ready = lock.withLock { () -> Bool in
                if finished { return true }
                waiters.append(continuation)
                return false
            }
            if ready { continuation.resume() }
        }
    }

    fileprivate func finish() {
        let pending = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
            guard !finished else { return [] }
            finished = true
            defer { waiters.removeAll() }
            return waiters
        }
        // Resume outside the lock. Waiters run on their Swift executor;
        // this callback never blocks or reenters the RTC API.
        for waiter in pending { waiter.resume() }
    }
}
