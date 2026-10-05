// NereusSDR for iOS: the radio at a glance and Protocol Info, each value as the Core sends it or unavailable with its reason
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMirror
import NereusModels

/// What the Radio tab says about the Core's radio (spec section 5.2 item 5,
/// the board's `tpl-radio` "Radio" block): model, firmware, protocol,
/// sample rate, slices in use, PA volts, ADC overload and the Core's CPU;
/// and Protocol Info's lines (the desktop's Radio > Protocol Info: radio,
/// protocol, firmware, MAC and address). Each value comes from what the
/// Core sends: the `radio` object, the capabilities, the catalogue and the
/// telemetry (link document sections 6.4, 7.1, 7.4 and 10). A value the
/// Core does not send, an older Core does not know, or a reading older than
/// three seconds shows as unavailable with its reason, never as a zero.
enum RadioAtAGlance {
    /// One value, or why there is none.
    enum Reading: Equatable, Sendable {
        case value(String, Tone = .plain)
        case unavailable(String)

        var text: String {
            switch self {
            case .value(let text, _): return text
            case .unavailable: return unavailableText
            }
        }

        var reason: String? {
            if case .unavailable(let reason) = self {
                return reason
            }
            return nil
        }
    }

    /// How a value is drawn: plain, good (no overload) or a warning.
    enum Tone: Equatable, Sendable {
        case plain
        case good
        case warning
    }

    /// One line: its name and its value.
    struct Row: Equatable, Identifiable, Sendable {
        let id: String
        let label: String
        let reading: Reading
    }

    /// Everything the rows are read from.
    struct Inputs {
        /// The Core's snapshot is complete and its link is up.
        var connected: Bool
        /// The `radio` object's values; empty when the Core sent none.
        var radio: [String: MirrorValue]
        var capabilities: [String: MirrorValue]
        var agreedMinor: UInt16
        var board: StationCatalog.Board?
        /// This device's slices (`slice:` objects) and the other devices' (`marker:` objects).
        var ownSlices: Int
        var otherSlices: Int
        /// The latest telemetry of this snapshot, and the phone's clock now.
        var receipt: StationTelemetryReceipt?
        var nowMilliseconds: Int64

        init(connected: Bool, radio: [String: MirrorValue] = [:], capabilities: [String: MirrorValue] = [:],
             agreedMinor: UInt16 = 0, board: StationCatalog.Board? = nil, ownSlices: Int = 0,
             otherSlices: Int = 0, receipt: StationTelemetryReceipt? = nil, nowMilliseconds: Int64 = 0) {
            self.connected = connected
            self.radio = radio
            self.capabilities = capabilities
            self.agreedMinor = agreedMinor
            self.board = board
            self.ownSlices = ownSlices
            self.otherSlices = otherSlices
            self.receipt = receipt
            self.nowMilliseconds = nowMilliseconds
        }
    }

    // MARK: Words

    static let unavailableText = "Unavailable"
    static let notConnectedReason = "The Core is not connected."
    static let noRadioReason = "The Core is not connected to its radio."
    static let notReportedReason = "Not reported by this Core."
    static let waitingReason = "Waiting for the Core's first reading."
    static let staleReason = "No reading from the Core in the last 3 seconds."
    static let olderSampleRateReason = "This Core does not report the radio's sample rate. Updating the Core may help."
    static let olderPaReason = "This Core does not report the radio's PA voltage. Updating the Core may help."
    static let olderAdcReason = "This Core does not report ADC overload. Updating the Core may help."
    static let olderCpuReason = "This Core does not report its computer's load. Updating the Core may help."
    static let absentSampleRateReason = "The Core has not reported the radio's sample rate."
    static let absentPaReason = "The radio has not reported its PA voltage."
    static let absentAdcReason = "The radio has not reported its ADC status."
    static let unknownAdcReason = "The radio's ADC status is out of date."
    static let absentCpuReason = "The Core does not measure its computer's load on this system."
    static let noOverloadText = "No overload"

    /// A reading older than this is not shown as current (link section 10).
    static let freshMilliseconds: Int64 = 3_000

    // MARK: The radio at a glance

    /// The eight lines, in the board's order.
    static func rows(_ inputs: Inputs) -> [Row] {
        [
            Row(id: "model", label: "Model", reading: model(inputs)),
            Row(id: "firmware", label: "Firmware", reading: firmware(inputs)),
            Row(id: "protocol", label: "Protocol", reading: protocolNumber(inputs)),
            Row(id: "sampleRate", label: "Sample rate", reading: sampleRate(inputs)),
            Row(id: "slices", label: "Slices", reading: slices(inputs)),
            Row(id: "pa", label: "PA", reading: paVolts(inputs)),
            Row(id: "adc", label: "ADC", reading: adc(inputs)),
            Row(id: "coreCpu", label: "Core CPU", reading: coreCpu(inputs)),
        ]
    }

    /// Why the Core has no radio, in its own words when it sent them; nil while it has one.
    static func noRadio(_ inputs: Inputs) -> String? {
        let connected: Bool? = bool(inputs.radio["connected"]) ?? bool(inputs.capabilities["radioConnected"])
        guard connected == false else {
            return nil
        }
        if let words = text(inputs.radio[StationRadio.waitingProperty]), !words.isEmpty {
            return words
        }
        return noRadioReason
    }

    static func model(_ inputs: Inputs) -> Reading {
        guard inputs.connected else { return .unavailable(notConnectedReason) }
        if let name = [text(inputs.radio["model"]), text(inputs.capabilities["radioModel"]),
                       inputs.board?.productLabel].compactMap({ $0 }).first(where: { !$0.isEmpty }) {
            return .value(name)
        }
        return .unavailable(noRadio(inputs) ?? notReportedReason)
    }

    static func firmware(_ inputs: Inputs) -> Reading {
        guard inputs.connected else { return .unavailable(notConnectedReason) }
        if let reason = noRadio(inputs) { return .unavailable(reason) }
        if let version = [text(inputs.radio["version"]), text(inputs.capabilities["firmwareVersion"])]
            .compactMap({ $0 }).first(where: { !$0.isEmpty }) {
            return .value(version)
        }
        return .unavailable(notReportedReason)
    }

    /// "1" or "2", as the board shows it.
    static func protocolNumber(_ inputs: Inputs) -> Reading {
        guard inputs.connected else { return .unavailable(notConnectedReason) }
        if let reason = noRadio(inputs) { return .unavailable(reason) }
        guard let number = radioProtocol(inputs) else { return .unavailable(notReportedReason) }
        return .value(String(number))
    }

    static func sampleRate(_ inputs: Inputs) -> Reading {
        telemetry(inputs, version: 4, minor: 11, older: olderSampleRateReason, radioReading: true) { metrics in
            guard let hertz = metrics.radio?.sampleRateHz, hertz > 0 else {
                return .unavailable(absentSampleRateReason)
            }
            return .value(kilohertz(hertz))
        }
    }

    /// "2 of 5": every slice on the Core, of the most it runs.
    static func slices(_ inputs: Inputs) -> Reading {
        guard inputs.connected else { return .unavailable(notConnectedReason) }
        let inUse = inputs.ownSlices + inputs.otherSlices
        let most = int(inputs.capabilities["effectiveMaxSlices"]).flatMap { $0 > 0 ? $0 : nil }
            ?? inputs.board.flatMap { $0.maxSlices > 0 ? Int64($0.maxSlices) : nil }
        guard let most else {
            return .value("\(inUse) in use")
        }
        return .value("\(inUse) of \(most)")
    }

    static func paVolts(_ inputs: Inputs) -> Reading {
        telemetry(inputs, version: 4, minor: 11, older: olderPaReason, radioReading: true) { metrics in
            guard let volts = metrics.radio?.paVolts, volts.isFinite else {
                return .unavailable(absentPaReason)
            }
            return .value(String(format: "%.1f V", volts))
        }
    }

    /// No overload, the ADCs in overload now, or why the status is not known.
    static func adc(_ inputs: Inputs) -> Reading {
        telemetry(inputs, version: 6, minor: 11, older: olderAdcReason, radioReading: true) { metrics in
            guard let overloads = metrics.radio?.adcOverloads, !overloads.isEmpty else {
                return .unavailable(absentAdcReason)
            }
            let over = overloads.filter { $0.overloaded == true }.map(\.adc).sorted()
            if !over.isEmpty {
                let names = over.map { "ADC \($0)" }
                return .value("Overload on " + names.joined(separator: " and "), .warning)
            }
            guard overloads.allSatisfy({ $0.overloaded == false }) else {
                return .unavailable(unknownAdcReason)
            }
            return .value(noOverloadText, .good)
        }
    }

    /// The Core computer's whole CPU load, as the desktop's diagnostics show it.
    static func coreCpu(_ inputs: Inputs) -> Reading {
        telemetry(inputs, version: 2, minor: 10, older: olderCpuReason, radioReading: false) { metrics in
            guard let percent = metrics.host?.systemCpuPercent, percent.isFinite else {
                return .unavailable(absentCpuReason)
            }
            return .value(String(format: "%.0f%%", percent))
        }
    }

    // MARK: Protocol Info

    /// The desktop's Protocol Info lines, with the model.
    static func protocolInfo(_ inputs: Inputs) -> [Row] {
        func identity(_ values: [String?]) -> Reading {
            guard inputs.connected else { return .unavailable(notConnectedReason) }
            if let value = values.compactMap({ $0 }).first(where: { !$0.isEmpty }) {
                return .value(value)
            }
            return .unavailable(noRadio(inputs) ?? notReportedReason)
        }
        var protocolReading = protocolNumber(inputs)
        if case .value(let number, _) = protocolReading {
            protocolReading = .value("Protocol \(number)")
        }
        return [
            Row(id: "radio", label: "Radio",
                reading: identity([text(inputs.radio["name"]), text(inputs.capabilities["stationName"])])),
            Row(id: "model", label: "Model", reading: model(inputs)),
            Row(id: "protocol", label: "Protocol", reading: protocolReading),
            Row(id: "firmware", label: "Firmware", reading: firmware(inputs)),
            Row(id: "mac", label: "MAC", reading: identity([text(inputs.capabilities["macAddress"])])),
            Row(id: "address", label: "IP address",
                reading: identity([inputs.agreedMinor >= 11 ? text(inputs.capabilities["radioAddress"]) : nil])),
        ]
    }

    // MARK: Inside

    private static func radioProtocol(_ inputs: Inputs) -> Int64? {
        // The radio identity entries arrive at agreed minor 11 (link 6.4).
        guard inputs.agreedMinor >= 11, let number = int(inputs.capabilities["radioProtocol"]),
              number == 1 || number == 2 else {
            return nil
        }
        return number
    }

    /// A telemetry reading: the Core's version and the link's minor allow
    /// it, the sample is this snapshot's and no older than three seconds.
    private static func telemetry(_ inputs: Inputs, version: Int64, minor: UInt16, older: String, radioReading: Bool,
                                  _ read: (StationMetrics) -> Reading) -> Reading {
        guard inputs.connected else { return .unavailable(notConnectedReason) }
        guard inputs.agreedMinor >= minor, (int(inputs.capabilities["stationTelemetryVersion"]) ?? 0) >= version else {
            return .unavailable(older)
        }
        if radioReading, let reason = noRadio(inputs) { return .unavailable(reason) }
        guard let receipt = inputs.receipt else { return .unavailable(waitingReason) }
        let age = inputs.nowMilliseconds - receipt.observedAtMilliseconds
        guard age >= 0, age <= freshMilliseconds else { return .unavailable(staleReason) }
        return read(receipt.metrics)
    }

    /// "192 kHz", "44.1 kHz".
    static func kilohertz(_ hertz: Int64) -> String {
        if hertz % 1_000 == 0 {
            return "\(hertz / 1_000) kHz"
        }
        return String(format: "%.1f kHz", Double(hertz) / 1_000)
    }

    private static func text(_ value: MirrorValue?) -> String? {
        if case .text(let text)? = value { return text }
        return nil
    }

    private static func bool(_ value: MirrorValue?) -> Bool? {
        if case .bool(let flag)? = value { return flag }
        return nil
    }

    private static func int(_ value: MirrorValue?) -> Int64? {
        switch value {
        case .int(let number)?, .enumeration(let number)?: return number
        default: return nil
        }
    }
}
