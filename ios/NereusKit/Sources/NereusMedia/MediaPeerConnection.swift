// NereusSDR for iOS: what the media control client needs of a media peer, so its tests can stand one in
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The parts of ``MediaPeer`` that ``MediaControlClient`` drives: the Core's
/// offer and candidates in, the answer and local candidates out, the
/// received media, the connection's state, the expected audio SSRC, and the
/// microphone line with the "tx" data channel.
/// ``MediaPeer`` is the one real conformer; the client's tests use a
/// scripted one.
public protocol MediaPeerConnection: AnyObject, Sendable {
    var localDescription: AsyncStream<String> { get }
    var localCandidates: AsyncStream<(candidate: String, mid: String)> { get }
    var audioPackets: AsyncStream<RtpPacket> { get }
    var displayDatagrams: AsyncStream<Data> { get }
    var state: AsyncStream<MediaPeer.State> { get }

    /// Reserve the connection's optional loopback carrier before signalling.
    func prepare(connectionId: UUID, gates: MediaFeatureGates) async throws
    var selectedTunnel: Bool { get }
    var selectedRelayOrTunnel: Bool { get }
    var selectedRouteObservation: SelectedRouteObservation { get }
    var trafficObservation: MediaTrafficObservation? { get }
    var currentAudioAdmissionObservation: MediaPeer.AudioAdmissionObservation? { get }

    func setRemoteDescription(_ sdp: String) throws
    func addRemoteCandidate(_ candidate: String, mid: String) throws
    func setExpectedAudioSsrc(_ ssrc: UInt32?)
    func enableMicrophoneLine(ssrc: UInt32)
    var microphoneLineReady: Bool { get }
    /// The microphone line was answered with the Core's L16 beside Opus, so
    /// the Core takes lossless packets on it too (R-IOS-09).
    var microphoneLosslessNegotiated: Bool { get }
    /// One element each time the Core closes the microphone line's track
    /// while the connection stays up.
    var microphoneLineClosed: AsyncStream<Void> { get }
    var txChannelReady: Bool { get }
    func sendMicrophone(_ packet: RtpPacket) throws
    func sendTx(_ message: Data) throws
    func close()
}

extension MediaPeer: MediaPeerConnection {
    public var currentAudioAdmissionObservation: AudioAdmissionObservation? { audioAdmissionObservation }
}

/// A peer that never carries the microphone line, as a stand-in that does
/// not script it: asking for the line changes nothing, and nothing goes out.
public extension MediaPeerConnection {
    func prepare(connectionId: UUID, gates: MediaFeatureGates) async throws {}
    var selectedTunnel: Bool { false }
    var selectedRelayOrTunnel: Bool { false }
    var selectedRouteObservation: SelectedRouteObservation { .unavailable(.unsupported) }
    var trafficObservation: MediaTrafficObservation? { nil }
    var currentAudioAdmissionObservation: MediaPeer.AudioAdmissionObservation? { nil }
    func enableMicrophoneLine(ssrc: UInt32) {}

    var microphoneLineReady: Bool { false }

    var microphoneLosslessNegotiated: Bool { false }

    var microphoneLineClosed: AsyncStream<Void> { AsyncStream { $0.finish() } }

    var txChannelReady: Bool { false }

    func sendMicrophone(_ packet: RtpPacket) throws {
        throw MediaPeerError.microphoneNotReady
    }

    func sendTx(_ message: Data) throws {
        throw MediaPeerError.txChannelNotReady
    }
}
