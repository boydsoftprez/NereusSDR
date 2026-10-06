// NereusSDR for iOS: reads the Core's telemetry, in order and only as far as the link allows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// Reads `station.metrics.v1` (link document section 10). A message is
/// read only when the agreed minor is 3 or more and the Core's
/// `stationTelemetryVersion` is 1 or more; host fields need minor 10 and
/// version 2, receivers minor 11 and version 3, the radio's PA readings and
/// link quality minor 11 and version 4, its Hermes Lite 2 link minor 11
/// and version 5. Radio diagnostics need minor 11 and version 6.
/// A reading out of its range is read
/// as unavailable, never as 0. A message whose `sequence`
/// is not higher than the last, or whose `sampledElapsedMs` went back, is
/// dropped, and one over 16 KiB is refused.
public struct StationTelemetryDecoder: Sendable {
    /// The whole message's cap.
    public static let capBytes = LinkCodec.telemetryCapBytes

    private var lastSequence: Int64?
    private var lastSampledElapsedMs: Int64?

    public init() {}

    /// Forgets the last sample; a new session counts from its own start.
    public mutating func reset() {
        lastSequence = nil
        lastSampledElapsedMs = nil
    }

    /// The sample `message` carries, or nil when it is not read or dropped.
    public mutating func decode(_ message: LinkMessage.StationMetrics, agreedMinor: UInt16,
                                telemetryVersion: Int64) -> StationMetrics? {
        guard agreedMinor >= 3, telemetryVersion >= 1 else {
            return nil
        }
        guard LinkCodec.encode(.stationMetrics(message)).utf8.count <= Self.capBytes else {
            return nil
        }
        let payload = message.payload
        guard let sequence = Self.whole(payload["sequence"]),
              let elapsed = Self.whole(payload["sampledElapsedMs"]) else {
            return nil
        }
        if let lastSequence, sequence <= lastSequence {
            return nil
        }
        if let lastSampledElapsedMs, elapsed < lastSampledElapsedMs {
            return nil
        }
        // Validate every present negotiated V6 field before recording order.
        // One malformed diagnostic rejects the entire sample.
        var diagnostics: (connectionAgeMs: Int64?, radioUdpBasePort: Int64?,
                          adcOverloads: [StationMetrics.ADCOverload]?)?
        if agreedMinor >= 11, telemetryVersion >= 6, case .object(let radio)? = payload["radio"] {
            guard let valid = Self.radioDiagnostics(radio) else { return nil }
            diagnostics = valid
        }
        lastSequence = sequence
        lastSampledElapsedMs = elapsed

        var metrics = StationMetrics(sequence: sequence, sampledElapsedMs: elapsed)
        if case .object(let radio)? = payload["radio"] {
            var read = StationMetrics.Radio(
                connected: Self.bool(radio["connected"]), rxMbps: Self.number(radio["rxMbps"]),
                txMbps: Self.number(radio["txMbps"]), rttMs: Self.number(radio["rttMs"]),
                rttAgeMs: Self.number(radio["rttAgeMs"]))
            if agreedMinor >= 11, telemetryVersion >= 4 {
                read.paVolts = Self.notNegative(radio["paVolts"])
                read.supplyVolts = Self.notNegative(radio["supplyVolts"])
                read.paCurrentAmps = Self.notNegative(radio["paCurrentAmps"])
                read.paTemperatureCelsius = Self.number(radio["paTemperatureCelsius"]).flatMap {
                    $0 >= Self.absoluteZeroCelsius ? $0 : nil
                }
                read.packetLossPercent = Self.number(radio["packetLossPercent"]).flatMap {
                    (0...100).contains($0) ? $0 : nil
                }
                read.jitterMs = Self.notNegative(radio["jitterMs"])
                read.packetGapMs = Self.notNegative(radio["packetGapMs"])
                read.sampleRateHz = Self.whole(radio["sampleRateHz"]).flatMap { $0 >= 0 ? $0 : nil }
                read.udpPacketsSeen = Self.whole(radio["udpPacketsSeen"]).flatMap { $0 >= 0 ? $0 : nil }
            }
            if agreedMinor >= 11, telemetryVersion >= 5 {
                read.hl2RxBytesPerSecond = Self.notNegative(radio["hl2RxBytesPerSecond"])
                read.hl2TxBytesPerSecond = Self.notNegative(radio["hl2TxBytesPerSecond"])
                read.hl2Throttled = Self.bool(radio["hl2Throttled"])
                read.hl2SequenceGaps = Self.whole(radio["hl2SequenceGaps"]).flatMap { $0 >= 0 ? $0 : nil }
            }
            if let diagnostics {
                read.connectionAgeMs = diagnostics.connectionAgeMs
                read.radioUdpBasePort = diagnostics.radioUdpBasePort
                read.adcOverloads = diagnostics.adcOverloads
            }
            metrics.radio = read
        }
        if case .object(let audio)? = payload["audio"] {
            metrics.audio = StationMetrics.Audio(
                active: Self.bool(audio["active"]), contextGeneration: Self.whole(audio["contextGeneration"]),
                sourceFramesPerSecond: Self.number(audio["sourceFramesPerSecond"]),
                sourceDropsPerSecond: Self.number(audio["sourceDropsPerSecond"]),
                encodedPacketsPerSecond: Self.number(audio["encodedPacketsPerSecond"]),
                encodeFailuresPerSecond: Self.number(audio["encodeFailuresPerSecond"]),
                sendAcceptedPerSecond: Self.number(audio["sendAcceptedPerSecond"]),
                sendRejectedPerSecond: Self.number(audio["sendRejectedPerSecond"]))
        }
        if agreedMinor >= 10, telemetryVersion >= 2, case .object(let host)? = payload["host"] {
            metrics.host = StationMetrics.Host(
                systemCpuPercent: Self.number(host["systemCpuPercent"]),
                processCpuPercent: Self.number(host["processCpuPercent"]),
                memoryTotalKiB: Self.whole(host["memoryTotalKiB"]),
                memoryAvailableKiB: Self.whole(host["memoryAvailableKiB"]),
                processResidentKiB: Self.whole(host["processResidentKiB"]),
                hottestZoneCelsius: Self.number(host["hottestZoneCelsius"]),
                hottestZoneName: Self.text(host["hottestZoneName"]))
        }
        if agreedMinor >= 11, telemetryVersion >= 3, case .array(let receivers)? = payload["receivers"] {
            metrics.receivers = receivers.compactMap { element in
                guard case .object(let receiver) = element else {
                    return nil
                }
                return StationMetrics.Receiver(
                    sliceId: Self.whole(receiver["sliceId"]), inputDelayMs: Self.number(receiver["inputDelayMs"]),
                    skippedInputMs: Self.number(receiver["skippedInputMs"]),
                    loadPercent: Self.number(receiver["loadPercent"]))
            }
        }
        return metrics
    }

    /// Link document section 10: a temperature is not below absolute zero.
    private static let absoluteZeroCelsius = -273.15
    private static let maxExactJSONInteger = 9_007_199_254_740_991.0

    private static func exactNonnegativeInteger(_ value: LinkJSON?) -> Int64? {
        guard let number = number(value), number.isFinite, number >= 0,
              number <= maxExactJSONInteger, number.rounded(.towardZero) == number else {
            return nil
        }
        return Int64(number)
    }

    private static func radioDiagnostics(_ radio: [String: LinkJSON])
        -> (connectionAgeMs: Int64?, radioUdpBasePort: Int64?, adcOverloads: [StationMetrics.ADCOverload]?)? {
        let ageKey = "connectionAgeMs", portKey = "radioUdpBasePort", adcKey = "adcOverloads"
        let hasV6 = radio[ageKey] != nil || radio[portKey] != nil || radio[adcKey] != nil
        if hasV6 && bool(radio["connected"]) != true { return nil }
        let age = radio[ageKey].flatMap(exactNonnegativeInteger)
        if radio[ageKey] != nil && age == nil { return nil }
        let port = radio[portKey].flatMap(exactNonnegativeInteger)
        if radio[portKey] != nil && !(port.map { (1...65_535).contains($0) } ?? false) { return nil }
        guard let rawEntries = radio[adcKey] else { return (age, port, nil) }
        guard case .array(let entries) = rawEntries, entries.count <= 3 else { return nil }
        var decoded: [StationMetrics.ADCOverload] = []
        var seen: Set<Int64> = []
        for value in entries {
            guard case .object(let entry) = value,
                  let adc = exactNonnegativeInteger(entry["adc"]), adc <= 2,
                  let count = exactNonnegativeInteger(entry["eventsSinceConnection"]),
                  seen.insert(adc).inserted else { return nil }
            let statusAge = entry["statusAgeMs"].flatMap(exactNonnegativeInteger)
            if entry["statusAgeMs"] != nil && statusAge == nil { return nil }
            let lastAge = entry["lastOverloadAgeMs"].flatMap(exactNonnegativeInteger)
            if entry["lastOverloadAgeMs"] != nil && lastAge == nil { return nil }
            let overloaded = entry["overloaded"].flatMap(bool)
            if entry["overloaded"] != nil && (overloaded == nil || !(statusAge.map { $0 <= 3_000 } ?? false)
                                              || (overloaded == true && count == 0)) { return nil }
            if let lastAge, count == 0 || (statusAge.map { lastAge < $0 } ?? false) { return nil }
            decoded.append(StationMetrics.ADCOverload(adc: Int(adc), eventsSinceConnection: count,
                                                      statusAgeMs: statusAge, overloaded: overloaded,
                                                      lastOverloadAgeMs: lastAge))
        }
        return (age, port, decoded)
    }

    /// A finite reading that is not negative.
    private static func notNegative(_ value: LinkJSON?) -> Double? {
        number(value).flatMap { $0.isFinite && $0 >= 0 ? $0 : nil }
    }

    private static func number(_ value: LinkJSON?) -> Double? {
        if case .number(let number)? = value {
            return number
        }
        return nil
    }

    /// A whole number inside the 64-bit range.
    private static func whole(_ value: LinkJSON?) -> Int64? {
        guard let number = number(value), number == number.rounded(.towardZero),
              number >= -9.223_372_036_854_775_808e18, number < 9.223_372_036_854_775_808e18 else {
            return nil
        }
        return Int64(number)
    }

    private static func bool(_ value: LinkJSON?) -> Bool? {
        if case .bool(let flag)? = value {
            return flag
        }
        return nil
    }

    private static func text(_ value: LinkJSON?) -> String? {
        if case .string(let text)? = value {
            return text
        }
        return nil
    }
}
