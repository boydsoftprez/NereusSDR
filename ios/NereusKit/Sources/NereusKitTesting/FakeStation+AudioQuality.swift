// NereusSDR for iOS: the fake Core's per-device audio quality: audioQualityVersion, audio and its audio-context, lossless
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMedia

extension FakeStation.Additions {
    /// The per-device audio quality (the media control document,
    /// "Per-device audio quality"): `audioQualityVersion` 1 beside the
    /// audio profile (`remoteMediaVersion`, `remoteAudioStatusVersion`,
    /// `audioProfileVersion` 1). Each `audio` is answered with one
    /// `audio-context` as the Core answers it. Not in ``all``.
    public static let audioQuality = FakeStation.Additions(rawValue: 1 << 46)
}

extension FakeStation {
    /// What the fake Core does with a request for lossless: gives it (its
    /// `audio_lossless` allows it and the connection carries it), refuses it
    /// as not allowed, or refuses it as unavailable on this connection.
    public enum LosslessAnswer: Sendable, Equatable {
        case given
        case notAllowed
        case unavailable
    }

    /// The Core's words for a bitrate outside its measured table.
    public static let opusBitrateRefusal = "This Core does not offer that audio quality. The audio stays as it was."

    /// The Core's measured table: each bitrate a device may ask for, with
    /// the audio bandwidth it carries (the catalogue's `audio.opusProfiles`).
    public static let measuredOpusProfiles: [(bitrate: Int, bandwidthHz: Int)] = [(24000, 8000), (48000, 20000)]
    /// The Core's `audio_bitrate`, where a new media peer starts.
    public static let defaultOpusBitrate = 48000

    /// One `audio` the fake took.
    public struct AudioRequest: Sendable, Equatable {
        public var connectionId: String
        public var revision: UInt32
        public var enabled: Bool
        public var profile: String
        public var opusBitrate: Int?
    }

    /// The audio the app asked for and what the fake runs, behind its own lock.
    final class AudioQualityState: @unchecked Sendable {
        struct Values {
            var answer = LosslessAnswer.given
            var bitrate = FakeStation.defaultOpusBitrate
            var generation: UInt32 = 0
            var requests: [AudioRequest] = []
        }

        private let lock = NSLock()
        private var values = Values()

        func read<Result>(_ body: (inout Values) -> Result) -> Result {
            lock.withLock { body(&values) }
        }
    }

    /// What the fake does with a request for lossless (``LosslessAnswer/given``
    /// unless set). Set it before the app connects: the microphone line of a
    /// media peer made after carries lossless only when given.
    public var losslessAnswer: LosslessAnswer {
        get { audioQualityState.read { $0.answer } }
        set { audioQualityState.read { $0.answer = newValue } }
    }

    /// Every `audio` the fake took, in order.
    public var audioRequests: [AudioRequest] {
        audioQualityState.read { $0.requests }
    }

    static func audioQualityCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        guard additions.contains(.audioQuality) else {
            return []
        }
        return [("remoteMediaVersion", 1), ("remoteAudioStatusVersion", 1), ("audioProfileVersion", 1),
                ("audioQualityVersion", 1)]
    }

    /// Whether a media peer made now carries lossless on its microphone line.
    var microphoneCarriesLossless: Bool {
        additions.contains(.audioQuality) && losslessAnswer == .given
    }

    /// The fake's answer to an audio operation; nil leaves the operation to
    /// the rest of the fake. A `start` begins a new media peer at the Core's
    /// `audio_bitrate` and is left to the rest.
    func audioQualityReplies(_ payload: [String: LinkJSON]) -> [LinkMessage]? {
        guard additions.contains(.audioQuality) else {
            return nil
        }
        if payload["op"] == .string("start") {
            audioQualityState.read { $0.bitrate = Self.defaultOpusBitrate }
            return nil
        }
        guard payload["op"] == .string("audio") else {
            return nil
        }
        // The profile shape, perhaps with a whole opusBitrate; anything
        // else is ignored, as the Core ignores it.
        var shape = payload
        let bitrateValue = shape.removeValue(forKey: "opusBitrate")
        guard Set(shape.keys) == ["op", "connectionId", "revision", "enabled", "profile"],
              case .string(let id)? = payload["connectionId"],
              case .number(let number)? = payload["revision"], number >= 1, number <= Double(UInt32.max),
              number.rounded(.towardZero) == number,
              case .bool(let enabled)? = payload["enabled"],
              case .string(let profile)? = payload["profile"], ["opus", "lossless"].contains(profile) else {
            return []
        }
        var bitrate: Int?
        if let bitrateValue {
            guard case .number(let value) = bitrateValue, value >= 0, value <= Double(Int32.max),
                  value.rounded(.towardZero) == value else {
                return []
            }
            bitrate = Int(value)
        }
        let revision = UInt32(number)
        let (running, refused, answer, generation) = audioQualityState.read { values in
            values.requests.append(AudioRequest(connectionId: id, revision: revision, enabled: enabled,
                                                profile: profile, opusBitrate: bitrate))
            // A measured bitrate becomes this device's; any other is
            // refused and the running one stays.
            var refused = false
            if let bitrate {
                if Self.measuredOpusProfiles.contains(where: { $0.bitrate == bitrate }) {
                    values.bitrate = bitrate
                } else {
                    refused = true
                }
            }
            values.generation &+= 1
            return (values.bitrate, refused, values.answer, values.generation)
        }
        var context: [String: LinkJSON] = [
            "op": .string("audio-context"), "connectionId": .string(id), "revision": .number(Double(revision)),
            "generation": .number(Double(generation)), "enabled": .bool(enabled),
            "ssrc": .number(Double(MediaControlClient.audioSsrc(forConnection: id))),
            "firstSequence": .number(0), "firstTimestamp": .number(0),
        ]
        let lossless = profile == "lossless" && answer == .given
        context["profile"] = .string(lossless ? "lossless" : "opus")
        if enabled {
            if lossless {
                context["encoder"] = .object(["codec": .string("l16"), "sampleRate": .number(48000),
                                              "channels": .number(2), "frameSamples": .number(192),
                                              "bitsPerSample": .number(16), "payloadType": .number(96)])
            } else {
                let bandwidth = Self.measuredOpusProfiles.first { $0.bitrate == running }?.bandwidthHz ?? 20000
                context["encoder"] = .object(["codec": .string("opus"), "sampleRate": .number(48000),
                                              "channels": .number(2), "frameSamples": .number(1920),
                                              "targetBitrate": .number(Double(running)),
                                              "audioBandwidthHz": .number(Double(bandwidth))])
            }
        } else {
            context["reason"] = .string("client-disabled")
        }
        if profile == "lossless", !lossless {
            context["profileRefusal"] = .string(answer == .notAllowed ? "lossless-not-allowed" : "lossless-unavailable")
        }
        if refused {
            context["opusBitrateRefusal"] = .string(Self.opusBitrateRefusal)
        }
        return [.mediaControl(LinkMessage.MediaControl(payload: context))]
    }
}
