// NereusSDR for iOS: the media peer's signalling limits, RTP packets and receive bounds, without a Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMedia

/// The media control document's limits, checked before anything reaches
/// libdatachannel: each refusal is the peer's own error, never the library's.
@Suite struct MediaPeerSignallingTests {
    private static let hostCandidate = "candidate:1 1 UDP 2114977535 192.0.2.10 50000 typ host"

    /// Fix wave (fix-tx2 item 5): a microphone track that closes while
    /// the connection is up, with its display channel and audio line open
    /// and no close of the phone's own, is the Core closing the line. One
    /// that closes in any other state closes with the connection:
    /// libdatachannel leaves the connected state before it closes the
    /// tracks of a connection going down.
    @Test func theMicrophoneTrackClosesWhileUpOnlyOnAConnectionThatStays() {
        #expect(MediaPeer.microphoneClosedWhileUp(transportConnected: true, displayOpen: true, audioOpen: true,
                                                  closeStarted: false, state: .connected))
        #expect(!MediaPeer.microphoneClosedWhileUp(transportConnected: false, displayOpen: true, audioOpen: true,
                                                   closeStarted: false, state: .connected))
        #expect(!MediaPeer.microphoneClosedWhileUp(transportConnected: true, displayOpen: false, audioOpen: true,
                                                   closeStarted: false, state: .connected))
        #expect(!MediaPeer.microphoneClosedWhileUp(transportConnected: true, displayOpen: true, audioOpen: false,
                                                   closeStarted: false, state: .connected))
        #expect(!MediaPeer.microphoneClosedWhileUp(transportConnected: true, displayOpen: true, audioOpen: true,
                                                   closeStarted: true, state: .connected))
        for state in [MediaPeer.State.new, .connecting, .disconnected, .failed, .closed] {
            #expect(!MediaPeer.microphoneClosedWhileUp(transportConnected: true, displayOpen: true, audioOpen: true,
                                                       closeStarted: false, state: state), "\(state)")
        }
    }

    @Test func observationNeedsConnectedMediaPeer() {
        let peer = MediaPeer()
        #expect(peer.selectedRouteObservation == .unavailable(.notReady))
        peer.close()
        #expect(peer.selectedRouteObservation == .unavailable(.retired))
    }

    @Test func realPeerTrafficIsVisibleThroughConnectionProtocol() throws {
        let peer = MediaPeer()
        let connection: any MediaPeerConnection = peer
        let first = try #require(connection.trafficObservation)
        #expect(first.active)
        #expect(first.receivedDisplayPayloadBytes == 0 && first.receivedRtpBytes == 0)
        peer.receiveDisplay(Data(repeating: 3, count: 21))
        peer.receiveRtp(Data(repeating: 0, count: MediaPeer.minRtpBytes))
        let live = try #require(connection.trafficObservation)
        #expect(live.lifetime == first.lifetime)
        #expect(live.receivedDisplayPayloadBytes == 21)
        #expect(live.receivedRtpBytes == UInt64(MediaPeer.minRtpBytes))
        connection.close()
        let retired = try #require(connection.trafficObservation)
        #expect(retired.lifetime == first.lifetime)
        #expect(!retired.active)
        #expect(retired.receivedDisplayPayloadBytes == 21)
    }

    @Test func trafficCountsCallbackBytesBeforeRtpParseAndQueueDrop() throws {
        let peer = MediaPeer()
        defer { peer.close() }
        let first = try #require(peer.trafficObservation)
        #expect(first.receivedDisplayPayloadBytes == 0 && first.receivedRtpBytes == 0)
        peer.receiveDisplay(Data(repeating: 1, count: 32))
        for _ in 0...MediaPeer.displayQueueMessages {
            peer.receiveDisplay(Data(repeating: 2, count: 32))
        }
        #expect(peer.trafficObservation?.receivedDisplayPayloadBytes
                == UInt64(32 * (MediaPeer.displayQueueMessages + 2)))
        #expect(peer.displayQueue.counts.dropped > 0)
        peer.receiveRtp(Data(repeating: 0, count: MediaPeer.minRtpBytes))
        #expect(peer.trafficObservation?.receivedRtpBytes == UInt64(MediaPeer.minRtpBytes))
        peer.receiveRtp(Data(repeating: 0, count: MediaPeer.minRtpBytes - 1))
        #expect(peer.trafficObservation?.receivedRtpBytes == UInt64(MediaPeer.minRtpBytes))
        let valid = RtpPacket(payloadType: 111, sequence: 2, timestamp: 3,
                              ssrc: 7, payload: Data([1, 2, 3])).bytes
        for _ in 0...MediaPeer.opusAudioQueuePackets { peer.receiveRtp(valid) }
        #expect(peer.audioQueue.counts.dropped > 0)
        #expect(peer.trafficObservation?.receivedRtpBytes
                == UInt64(MediaPeer.minRtpBytes + valid.count * (MediaPeer.opusAudioQueuePackets + 1)))
        peer.close()
        #expect(peer.trafficObservation?.lifetime == first.lifetime)
        #expect(peer.trafficObservation?.active == false)
    }

    @Test func audioAdmissionCountsHeaderSsrcAndQueueLossAtTheirOwnBoundaries() {
        let peer = MediaPeer()
        defer { peer.close() }
        let initial = peer.audioAdmissionObservation
        peer.receiveRtp(Data(repeating: 0, count: MediaPeer.minRtpBytes - 1))
        peer.receiveRtp(Data(repeating: 0, count: MediaPeer.minRtpBytes))
        #expect(peer.audioAdmissionObservation.rejectedHeaders == initial.rejectedHeaders + 2)
        peer.setExpectedAudioSsrc(7)
        let foreign = RtpPacket(payloadType: 111, sequence: 1, timestamp: 1,
                                ssrc: 8, payload: Data([1])).bytes
        peer.receiveRtp(foreign)
        #expect(peer.audioAdmissionObservation.rejectedSsrc == initial.rejectedSsrc + 1)
        let valid = RtpPacket(payloadType: 111, sequence: 2, timestamp: 1,
                              ssrc: 7, payload: Data([1])).bytes
        for _ in 0...MediaPeer.opusAudioQueuePackets { peer.receiveRtp(valid) }
        let observed = peer.audioAdmissionObservation
        #expect(observed.acceptedIntoReceiveQueue == UInt64(MediaPeer.opusAudioQueuePackets + 1))
        #expect(observed.droppedByReceiveQueue == 1)
        #expect(observed.heldByReceiveQueue == MediaPeer.opusAudioQueuePackets)
    }

    /// R-IOS-09: the receive queue holds the same 2.56 s of audio whatever
    /// the format: 64 of Opus's 40 ms packets, as the media control
    /// document gives, and 640 of the lossless profile's 4 ms L16 ones.
    @Test(arguments: [(UInt8(111), 64), (L16Audio.payloadType, 640)])
    func theAudioQueueIsBoundedByFormat(payloadType: UInt8, bound: Int) {
        #expect(MediaPeer.audioQueuePackets(for: payloadType == 111 ? .opus : .l16) == bound)
        let peer = MediaPeer()
        defer { peer.close() }
        for sequence in 0...bound {
            peer.receiveRtp(RtpPacket(payloadType: payloadType, sequence: UInt16(sequence), timestamp: 1,
                                      ssrc: 7, payload: Data([1])).bytes)
        }
        let observed = peer.audioAdmissionObservation
        #expect(observed.acceptedIntoReceiveQueue == UInt64(bound + 1))
        #expect(observed.droppedByReceiveQueue == 1)
        #expect(observed.heldByReceiveQueue == bound)
    }

    @Test func mediaSubmissionGuardsDoNotCountRefusedPayload() throws {
        let peer = MediaPeer()
        defer { peer.close() }
        let packet = RtpPacket(payloadType: 111, sequence: 1, timestamp: 1,
                               ssrc: 7, payload: Data([1, 2, 3]))
        #expect(throws: MediaPeerError.audioNotReady) { try peer.sendAudio(packet) }
        #expect(throws: MediaPeerError.microphoneNotReady) { try peer.sendMicrophone(packet) }
        #expect(throws: MediaPeerError.txChannelNotReady) { try peer.sendTx(Data([1, 2])) }
        #expect(peer.trafficObservation?.submittedRtpBytes == 0)
        #expect(peer.trafficObservation?.submittedTxChannelBytes == 0)
    }

    @Test func aDescriptionOverItsLimitIsRefused() {
        let peer = MediaPeer()
        defer { peer.close() }
        let oversized = "v=0\r\n" + String(repeating: "a", count: MediaPeer.maxDescriptionBytes)
        #expect(throws: MediaPeerError.descriptionTooLarge(bytes: oversized.utf8.count)) {
            try peer.setRemoteDescription(oversized)
        }
        #expect(throws: MediaPeerError.emptyDescription) {
            try peer.setRemoteDescription("")
        }
        #expect(throws: MediaPeerError.containsNul) {
            try peer.setRemoteDescription("v=0\r\n\u{0}\r\n")
        }
        #expect(throws: MediaPeerError.candidatesInDescription) {
            try peer.setRemoteDescription("v=0\r\na=\(Self.hostCandidate)\r\n")
        }
        #expect(throws: MediaPeerError.candidatesInDescription) {
            try peer.setRemoteDescription("v=0\r\na=end-of-candidates\r\n")
        }
    }

    @Test func aCandidateOrMidOverItsLimitIsRefused() {
        let peer = MediaPeer()
        defer { peer.close() }
        let long = Self.hostCandidate + String(repeating: " x", count: MediaPeer.maxCandidateBytes)
        #expect(throws: MediaPeerError.invalidCandidateLength(bytes: long.utf8.count)) {
            try peer.addRemoteCandidate(long, mid: "audio")
        }
        #expect(throws: MediaPeerError.invalidCandidateLength(bytes: 0)) {
            try peer.addRemoteCandidate("", mid: "audio")
        }
        let longMid = String(repeating: "m", count: MediaPeer.maxMidBytes + 1)
        #expect(throws: MediaPeerError.invalidMidLength(bytes: longMid.utf8.count)) {
            try peer.addRemoteCandidate(Self.hostCandidate, mid: longMid)
        }
        #expect(throws: MediaPeerError.containsNul) {
            try peer.addRemoteCandidate(Self.hostCandidate + "\u{0}", mid: "audio")
        }
        #expect(throws: MediaPeerError.relayCandidate) {
            try peer.addRemoteCandidate(
                "candidate:3 1 UDP 16777215 203.0.113.9 50002 typ relay raddr 0.0.0.0 rport 0",
                mid: "audio")
        }
    }

    /// A direct-address peer (R-IOS-16 media ladder): IPv6 direct, then an
    /// IPv4 hole punch through STUN, then the Core's tunnel. The Core's
    /// server reflexive candidate is kept; a relay candidate is refused as
    /// before, and no relay server is ever passed.
    @Test func aDirectAddressPeerKeepsServerReflexiveCandidatesAndTakesNoRelay() throws {
        let plain = MediaPeer.Configuration.directAddress()
        #expect(!plain.hostCandidatesOnly)
        #expect(plain.refusesRelayCandidates)
        #expect(plain.iceServers.isEmpty)
        #expect(plain == MediaPeer.Configuration())
        let withStun = MediaPeer.Configuration.directAddress(stunServers: [
            "turn:user:secret@turn.invalid:3478?transport=udp", "stun:stun.invalid:3478",
        ])
        #expect(withStun.iceServers == ["stun:stun.invalid:3478"])
        #expect(!withStun.hostCandidatesOnly)
        #expect(withStun.refusesRelayCandidates)
        #expect(withStun.mtu == IceSettings.mtu)
        for configuration in [plain, withStun] {
            let peer = MediaPeer(configuration: configuration)
            defer { peer.close() }
            try peer.addRemoteCandidate(Self.hostCandidate, mid: "audio")
            try peer.addRemoteCandidate(
                "candidate:2 1 UDP 1677729535 198.51.100.7 50001 typ srflx raddr 0.0.0.0 rport 0",
                mid: "audio")
            try peer.addRemoteCandidate(
                "candidate:4 1 UDP 1677729279 2001:db8::7 50003 typ srflx raddr :: rport 0", mid: "audio")
            #expect(throws: MediaPeerError.relayCandidate) {
                try peer.addRemoteCandidate(
                    "candidate:3 1 UDP 16777215 203.0.113.9 50002 typ relay raddr 0.0.0.0 rport 0",
                    mid: "audio")
            }
        }
    }

    @Test func atMost64RemoteCandidatesAreAdmittedIncludingThoseBeforeTheOffer() throws {
        let peer = MediaPeer()
        defer { peer.close() }
        for port in 0..<MediaPeer.maxRemoteCandidates {
            try peer.addRemoteCandidate(
                "candidate:\(port) 1 UDP 2114977535 192.0.2.10 \(50000 + port) typ host", mid: "audio")
        }
        #expect(throws: MediaPeerError.tooManyCandidates) {
            try peer.addRemoteCandidate(Self.hostCandidate, mid: "audio")
        }
    }

    @Test func aClosedPeerRefusesSignallingAndEndsItsStreams() async {
        let peer = MediaPeer()
        peer.close()
        #expect(throws: MediaPeerError.closed) {
            try peer.addRemoteCandidate(Self.hostCandidate, mid: "audio")
        }
        var states: [MediaPeer.State] = []
        for await state in peer.state {
            states.append(state)
        }
        #expect(states == [.closed])
        for await _ in peer.audioPackets {
            Issue.record("a closed peer delivered audio")
        }
    }

    @Test func candidateLinesAreTakenOutOfAnAnswer() {
        let sdp = "v=0\r\nm=audio 9 UDP/TLS/RTP/SAVPF 111\r\na=\(Self.hostCandidate)\r\n"
            + "a=end-of-candidates\r\na=mid:audio\r\n"
        let stripped = MediaPeer.withoutCandidates(sdp)
        #expect(stripped == "v=0\r\nm=audio 9 UDP/TLS/RTP/SAVPF 111\r\na=mid:audio\r\n")
        #expect(!MediaPeer.embedsCandidates(stripped))
        #expect(MediaPeer.candidateType(Self.hostCandidate) == "host")
    }

    @Test func selectedTurnCandidateCountsAsRelayWithoutLoopbackClaim() {
        let host = "candidate:1 1 UDP 100 192.0.2.1 4000 typ host"
        let relay = "candidate:2 1 UDP 10 198.51.100.5 6000 typ relay raddr 192.0.2.1 rport 4000"
        #expect(MediaPeer.selectedPairUsesRelayOrTunnel((local: relay, remote: host), loopbackPort: nil))
        #expect(MediaPeer.selectedPairUsesRelayOrTunnel((local: host, remote: relay), loopbackPort: nil))
        #expect(!MediaPeer.selectedPairUsesRelayOrTunnel((local: host, remote: host), loopbackPort: nil))
        let loopback = "candidate:wsrelay2 1 UDP 1 127.0.0.1 41000 typ host"
        #expect(MediaPeer.selectedPairUsesRelayOrTunnel((local: host, remote: loopback), loopbackPort: 41000))
        #expect(!MediaPeer.selectedPairUsesRelayOrTunnel((local: host, remote: loopback), loopbackPort: 41001))
    }

    @Test func rtpPacketsRoundTripAndRtcpIsNotAudio() throws {
        let packet = RtpPacket(payloadType: 111, sequence: 0xbeef, timestamp: 0x0102_0304,
                               ssrc: 0xdead_beef, payload: Data([1, 2, 3]))
        let bytes = packet.bytes
        #expect(bytes.count == RtpPacket.headerBytes + 3)
        #expect(RtpPacket(parsing: bytes) == packet)
        // A receiver report: version 2, packet type 201 (payload type 73 with the marker bit).
        #expect(RtpPacket(parsing: Data([0x80, 0xc9, 0x00, 0x01, 0, 0, 0, 1, 0, 0, 0, 0])) == nil)
        #expect(RtpPacket(parsing: Data(bytes.prefix(11))) == nil)
        var withPadding = bytes
        withPadding[0] |= 0x20
        withPadding.append(contentsOf: [0, 2])
        #expect(RtpPacket(parsing: withPadding)?.payload == Data([1, 2, 3]))
    }

    @Test func theReceiveQueueDropsTheOldestByCountAndByBytes() async {
        let byCount = MediaReceiveQueue<Int>(maxCount: 3)
        for value in 0..<10 {
            byCount.push(value)
        }
        #expect(byCount.counts.accepted == 10)
        #expect(byCount.counts.dropped == 7)
        byCount.finish()
        var kept: [Int] = []
        for await value in byCount.stream {
            kept.append(value)
        }
        #expect(kept == [7, 8, 9])

        let byBytes = MediaReceiveQueue<Data>(maxCount: 8, maxBytes: 250, size: { $0.count })
        for value in 0..<5 {
            byBytes.push(Data(repeating: UInt8(value), count: 100))
        }
        #expect(byBytes.counts.held == 2)
        byBytes.finish()
        var sizes: [UInt8] = []
        for await data in byBytes.stream {
            sizes.append(data[0])
        }
        #expect(sizes == [3, 4])
    }
}
