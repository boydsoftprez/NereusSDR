// NereusSDR for iOS: one second of the Core's own health readings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One `station.metrics.v1` sample (link document section 10), read by
/// `StationTelemetryDecoder`. A field the Core did not send is nil; the
/// host readings, the receivers and the radio's version 4 through 6 readings
/// are nil unless the agreed link allows them.
public struct StationMetrics: Equatable, Sendable {
    public struct Radio: Equatable, Sendable {
        public var connected: Bool?
        public var rxMbps: Double?
        public var txMbps: Double?
        public var rttMs: Double?
        public var rttAgeMs: Double?
        // Telemetry version 4 at agreed minor 11: the PA readings and the
        // radio link's quality, each nil when absent or out of range.
        public var paVolts: Double?
        public var supplyVolts: Double?
        public var paCurrentAmps: Double?
        public var paTemperatureCelsius: Double?
        /// 0 to 100.
        public var packetLossPercent: Double?
        public var jitterMs: Double?
        public var packetGapMs: Double?
        public var sampleRateHz: Int64?
        public var udpPacketsSeen: Int64?
        // Telemetry version 5 at agreed minor 11: the Hermes Lite 2 link,
        // sent only for a connected HL2, each nil when absent or out of range.
        public var hl2RxBytesPerSecond: Double?
        public var hl2TxBytesPerSecond: Double?
        public var hl2Throttled: Bool?
        /// EP6 sequence gaps since the radio connected.
        public var hl2SequenceGaps: Int64?
        /// Telemetry version 6, minor 11. Age of the Core's current radio connection.
        public var connectionAgeMs: Int64?
        /// Observed radio outbound UDP base or control destination port.
        public var radioUdpBasePort: Int64?
        /// Nil means no ADC status array was sent; empty means an observed empty array.
        public var adcOverloads: [ADCOverload]?
    }

    public struct ADCOverload: Equatable, Sendable {
        public var adc: Int
        /// Observed transitions into overload on this radio connection.
        public var eventsSinceConnection: Int64
        public var statusAgeMs: Int64?
        /// Nil is unknown, including stale status; false is an observed clear.
        public var overloaded: Bool?
        public var lastOverloadAgeMs: Int64?
    }

    public struct Audio: Equatable, Sendable {
        public var active: Bool?
        public var contextGeneration: Int64?
        public var sourceFramesPerSecond: Double?
        public var sourceDropsPerSecond: Double?
        public var encodedPacketsPerSecond: Double?
        public var encodeFailuresPerSecond: Double?
        public var sendAcceptedPerSecond: Double?
        public var sendRejectedPerSecond: Double?
    }

    public struct Host: Equatable, Sendable {
        public var systemCpuPercent: Double?
        public var processCpuPercent: Double?
        public var memoryTotalKiB: Int64?
        public var memoryAvailableKiB: Int64?
        public var processResidentKiB: Int64?
        public var hottestZoneCelsius: Double?
        public var hottestZoneName: String?
    }

    public struct Receiver: Equatable, Sendable {
        public var sliceId: Int64?
        public var inputDelayMs: Double?
        public var skippedInputMs: Double?
        public var loadPercent: Double?
    }

    public var sequence: Int64
    public var sampledElapsedMs: Int64
    public var radio: Radio?
    public var audio: Audio?
    /// Needs agreed minor 10 and telemetry version 2.
    public var host: Host?
    /// Needs agreed minor 11 and telemetry version 3.
    public var receivers: [Receiver]?
}
