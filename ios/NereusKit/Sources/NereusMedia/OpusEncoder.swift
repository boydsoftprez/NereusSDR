// NereusSDR for iOS: encodes the microphone to Opus packets for the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
internal import COpus
internal import COpusShim

/// One Opus encoder set up from a ``Profile``. It takes one frame of
/// interleaved float samples at a time and returns one packet for each.
public final class OpusEncoder {
    /// How the encoder is set up.
    public struct Profile: Equatable, Sendable {
        /// What the encoder is tuned for.
        public enum Application: Equatable, Sendable {
            case voip
            case audio
            case restrictedLowDelay
        }

        /// The kind of sound the encoder is told to expect.
        public enum Signal: Equatable, Sendable {
            case auto
            case voice
            case music
        }

        public var channels: Int
        public var sampleRate: Int
        /// Frames per packet, per channel.
        public var frameSamples: Int
        public var application: Application
        public var bitrate: Int
        public var variableBitrate: Bool
        public var constrainedVariableBitrate: Bool
        public var inbandFEC: Bool
        public var expectedLossPercent: Int
        public var discontinuousTransmission: Bool
        public var complexity: Int
        public var signal: Signal

        public init(channels: Int, sampleRate: Int, frameSamples: Int, application: Application,
                    bitrate: Int, variableBitrate: Bool, constrainedVariableBitrate: Bool,
                    inbandFEC: Bool, expectedLossPercent: Int, discontinuousTransmission: Bool,
                    complexity: Int, signal: Signal) {
            self.channels = channels
            self.sampleRate = sampleRate
            self.frameSamples = frameSamples
            self.application = application
            self.bitrate = bitrate
            self.variableBitrate = variableBitrate
            self.constrainedVariableBitrate = constrainedVariableBitrate
            self.inbandFEC = inbandFEC
            self.expectedLossPercent = expectedLossPercent
            self.discontinuousTransmission = discontinuousTransmission
            self.complexity = complexity
            self.signal = signal
        }

        /// The microphone's uplink (R-IOS-09): mono, 48000 Hz, 20 ms
        /// frames, voice tuning, 48000 bit/s constrained VBR, which libopus
        /// codes full band, in-band FEC for 10 % loss, no DTX, complexity
        /// 9. The application and the bandwidth (none set) are the desktop's
        /// remote microphone's.
        public static let microphone = microphone(bitrate: 48000)
        /// The same at 24000 bit/s, the phone's Save data choice.
        public static let microphoneSaveData = microphone(bitrate: 24000)

        private static func microphone(bitrate: Int) -> Profile {
            Profile(channels: 1, sampleRate: 48000, frameSamples: 960, application: .voip,
                    bitrate: bitrate, variableBitrate: true, constrainedVariableBitrate: true,
                    inbandFEC: true, expectedLossPercent: 10, discontinuousTransmission: false,
                    complexity: 9, signal: .voice)
        }
    }

    /// Room for the largest packet libopus writes.
    private static let maximumPacketBytes = 4000

    public private(set) var profile: Profile
    private let state: OpaquePointer

    public init(profile: Profile) throws {
        guard profile.channels == 1 || profile.channels == 2 else {
            throw OpusCodecError.unsupportedChannelCount(profile.channels)
        }
        var error: Int32 = OPUS_OK
        let created = opus_encoder_create(Int32(profile.sampleRate), Int32(profile.channels),
                                          Self.applicationCode(profile.application), &error)
        guard let created, error == OPUS_OK else {
            if let created {
                opus_encoder_destroy(created)
            }
            throw OpusCodecError.library(code: error)
        }
        let settings: [Int32] = [
            nereus_opus_set_signal(created, Self.signalCode(profile.signal)),
            nereus_opus_set_bitrate(created, Int32(clamping: profile.bitrate)),
            nereus_opus_set_vbr(created, profile.variableBitrate ? 1 : 0),
            nereus_opus_set_vbr_constraint(created, profile.constrainedVariableBitrate ? 1 : 0),
            nereus_opus_set_inband_fec(created, profile.inbandFEC ? 1 : 0),
            nereus_opus_set_packet_loss_perc(created, Int32(clamping: profile.expectedLossPercent)),
            nereus_opus_set_dtx(created, profile.discontinuousTransmission ? 1 : 0),
            nereus_opus_set_complexity(created, Int32(clamping: profile.complexity)),
        ]
        if let failed = settings.first(where: { $0 != OPUS_OK }) {
            opus_encoder_destroy(created)
            throw OpusCodecError.library(code: failed)
        }
        self.profile = profile
        self.state = created
    }

    deinit {
        opus_encoder_destroy(state)
    }

    /// The bit rate the encoder reports it is running at.
    public var bitrate: Int {
        var value: Int32 = 0
        guard nereus_opus_get_bitrate(state, &value) == OPUS_OK else {
            return 0
        }
        return Int(value)
    }

    /// Changes the rate on this encoder without replacing its state. Like
    /// encoding, configuration is serialised by the caller. The microphone
    /// owner changes only this setting between its paced 20 ms frames.
    public func setBitrate(_ bitrate: Int) throws {
        guard bitrate != profile.bitrate else { return }
        guard let setting = Int32(exactly: bitrate) else {
            throw OpusCodecError.library(code: OPUS_BAD_ARG)
        }
        let result = nereus_opus_set_bitrate(state, setting)
        guard result == OPUS_OK else { throw OpusCodecError.library(code: result) }
        profile.bitrate = bitrate
    }

    /// Encodes exactly one frame: `frameSamples * channels` interleaved samples.
    public func encode(_ pcm: [Float]) throws -> Data {
        let expected = profile.frameSamples * profile.channels
        guard pcm.count == expected else {
            throw OpusCodecError.wrongSampleCount(expected: expected, got: pcm.count)
        }
        var packet = [UInt8](repeating: 0, count: Self.maximumPacketBytes)
        let written: Int32 = pcm.withUnsafeBufferPointer { input in
            packet.withUnsafeMutableBufferPointer { output in
                opus_encode_float(state, input.baseAddress!, Int32(profile.frameSamples),
                                  output.baseAddress!, Int32(output.count))
            }
        }
        guard written >= 0 else {
            throw OpusCodecError.library(code: written)
        }
        return Data(packet.prefix(Int(written)))
    }

    private static func applicationCode(_ application: Profile.Application) -> Int32 {
        switch application {
        case .voip:
            return OPUS_APPLICATION_VOIP
        case .audio:
            return OPUS_APPLICATION_AUDIO
        case .restrictedLowDelay:
            return OPUS_APPLICATION_RESTRICTED_LOWDELAY
        }
    }

    private static func signalCode(_ signal: Profile.Signal) -> Int32 {
        switch signal {
        case .auto:
            return OPUS_AUTO
        case .voice:
            return OPUS_SIGNAL_VOICE
        case .music:
            return OPUS_SIGNAL_MUSIC
        }
    }
}
