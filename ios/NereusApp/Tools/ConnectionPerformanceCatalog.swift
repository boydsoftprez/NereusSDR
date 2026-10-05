// NereusSDR for iOS: complete chart and detail inventory for connection diagnostics
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMirror

enum PerformanceSection: String, CaseIterable, Identifiable {
    case connection = "Connection"
    case roundTrip = "Round trip"
    case audio = "Audio"
    case core = "Core"
    case details = "Details"

    var id: String { rawValue }
}

struct PerformanceLine: Identifiable {
    let metric: DiagnosticsMetric
    let title: String
    let color: UInt32
    var id: DiagnosticsMetric { metric }
}

enum PerformanceUnit {
    case kbps, kbit, mbps, milliseconds, packets, frames, events, percent, memory, celsius, receiver

    var label: String {
        switch self {
        case .kbps: "kbps"
        case .kbit: "kbit/s"
        case .mbps: "Mbps"
        case .milliseconds: "ms"
        case .packets: "packets/s"
        case .frames: "frames/s"
        case .events: "events/s"
        case .percent: "%"
        case .memory: "MiB"
        case .celsius: "°C"
        case .receiver: "% real time"
        }
    }
}

struct PerformanceChart: Identifiable {
    let id: String
    let section: PerformanceSection
    let title: String
    let explanation: String
    let unit: PerformanceUnit
    let lines: [PerformanceLine]
    let reference: Double?

    init(_ id: String, _ section: PerformanceSection, _ title: String, _ explanation: String,
         _ unit: PerformanceUnit, _ lines: [PerformanceLine], reference: Double? = nil) {
        self.id = id
        self.section = section
        self.title = title
        self.explanation = explanation
        self.unit = unit
        self.lines = lines
        self.reference = reference
    }
}

enum PerformanceDetailID: String, CaseIterable, Hashable {
    case controlState
    case radioState
    case playbackState
    case coreAge
    case phoneUptime
    case radioUptime
    case controlPeer
    case mediaPeer
    case rendezvous
    case latestAttempt
    case latestFailure
    case serviceOffers
    case routeGeneration
    case controlTraffic
    case totalTraffic
    case audioTraffic
    case coreRtt
    case radioRtt
    case maximumRadioRtt
    case radioIdentity
    case radioAddress
    case radioMac
    case radioThroughput
    case radioSampleRate
    case radioUdpPackets
    case radioLoss
    case radioJitter
    case radioGap
    case paVoltage
    case dcVoltage
    case adcOverload
    case coreSource
    case coreEncoded
    case coreAccepted
    case coreRefused
    case coreDrops
    case cpu
    case memory
    case sensor
    case receivers
    case audioBackend
    case speakerQueue
    case reorder
    case adaptiveHold
    case audioDelay
    case delivery
    case accuracy
    case delayExplanation
    case streamGeneration
    case acceptedPackets
    case startupDiscarded
    case decoded
    case concealed
    case late
    case invalid
    case duplicate
    case headerRejected
    case underflows
    case overflows
    case consumedFrames
    case lastPacketAge
    case lifetimeInterruptions
    case arrivalJitter
    case missingPackets
    case expectedPackets
    case concealedIntervals
    case linkInterruptions
    case burstDropped
    case skipped
    case clockDrift
    case audioBuffer
    case sessionUnderruns
    case resetScope
}

enum ConnectionPerformanceCatalog {
    private static func l(_ metric: DiagnosticsMetric, _ title: String, _ color: UInt32) -> PerformanceLine {
        PerformanceLine(metric: metric, title: title, color: color)
    }

    static let charts: [PerformanceChart] = [
        .init("total", .connection, "Total traffic · Core ↔ app",
              "What the app and Core exchange: control, display, sound and transmit keep-alive messages. Incoming counts what arrived, not what was played. Outgoing counts what this phone handed over, not what arrived. If any part is not known, Total is unavailable.", .kbps,
              [l(.coreGuiRxKbps, "Core → app received", 0x00B4D8), l(.coreGuiTxKbps, "App → Core outgoing", 0x5FFF8A), l(.coreGuiTotalKbps, "Total", 0xFFD700)]),
        .init("radioLink", .connection, "Radio link throughput", "Core ↔ radio rates, separate from Core ↔ phone traffic.", .mbps,
              [l(.radioRxMbps, "Radio RX", 0x00B4D8), l(.radioTxMbps, "Radio TX", 0x5FFF8A)]),
        .init("control", .connection, "Control traffic", "Control messages only. Sent means this phone handed them over, not that they arrived.", .kbit,
              [l(.sessionPayloadRxKbps, "Control received", 0x5FA8FF), l(.sessionPayloadTxKbps, "Control sent", 0xFFD700)]),
        .init("rtt", .roundTrip, "Round-trip time", "The radio round trip is between the Core and the radio; the Core round trip is between this phone and the Core. Neither is the sound's one-way delay. The radio round trip needs a recent Core reading, and both expire after 60 seconds.", .milliseconds,
              [l(.radioRttMs, "Last radio round trip", 0x00B4D8), l(.sessionRttMs, "Last Core round trip", 0xFFB86C)]),
        .init("speaker", .roundTrip, "Speaker buffering", "Measured frames queued for the phone speaker, excluding network, encoder, arrival smoothing and device delay.", .milliseconds,
              [l(.speakerBufferMs, "Speaker buffer", 0x5FFF8A)]),
        .init("packetAge", .roundTrip, "Time since last audio packet", "Age since the last accepted audio packet. It advances between arrivals and is unavailable when playback stops.", .milliseconds,
              [l(.playbackPacketAgeMs, "Packet age", 0xC792EA)]),
        .init("delay", .roundTrip, "Audio delay", "Measured Core capture to phone playback. Delivery excludes speaker queue. Accuracy is an uncertainty bound, never a delay. Half the control round trip is not used.", .milliseconds,
              [l(.audioDelayMs, "Audio delay", 0x5FFF8A), l(.audioDeliveryDelayMs, "Delivery", 0x00B4D8), l(.audioDelayAccuracyMs, "Accuracy (±)", 0xFFD700)]),
        .init("audioTraffic", .audio, "Audio traffic received", "Audio packets counts each packet whole, its header too, with packets later dropped. Audio content counts only the sound kept, duplicates too. Neither counts network overhead.", .kbps,
              [l(.audioRtpRxKbps, "Audio packets", 0x00B4D8), l(.audioPayloadRxKbps, "Audio content", 0x5FA8FF)]),
        .init("packetActivity", .audio, "Audio packet activity", "Core and phone counts belong to different ends of the stream. Concealment is not radio loss.", .packets,
              [l(.audioEncodedPacketsPerSecond, "Core encoded", 0x00B4D8), l(.audioSendAcceptedPerSecond, "Core accepted", 0x5FFF8A), l(.audioSendRejectedPerSecond, "Core refused", 0xFF6060), l(.playbackDecodedPacketsPerSecond, "Decoded here", 0x5FA8FF), l(.playbackConcealedPacketsPerSecond, "Filled in here", 0xFFD700), l(.playbackLatePacketsPerSecond, "Late here", 0xFF8C00)]),
        .init("sourceFrames", .audio, "Core source frames", "Source frames produced at the Core, separate from accepted audio packets.", .frames,
              [l(.audioSourceFramesPerSecond, "Core source", 0x00B4D8)]),
        .init("interruptions", .audio, "Audio interruption events", "Core source drops and phone playback underflow and overflow events per second. Stream and lifetime boundaries remain separate.", .events,
              [l(.audioSourceDropsPerSecond, "Core source drops", 0xFF6060), l(.playbackUnderflowsPerSecond, "Underflows here", 0xFFD700), l(.playbackOverflowsPerSecond, "Overflows here", 0xFF8C00)]),
        .init("cpu", .core, "Core computer CPU", "System and NereusSDR Core process use percentage of all processors together. A Core without host readings shows unavailable values.", .percent,
              [l(.coreSystemCpuPercent, "System", 0x00B4D8), l(.coreProcessCpuPercent, "NereusSDR Core", 0x5FFF8A)]),
        .init("memory", .core, "Core computer memory", "Available and resident Core process memory share one MiB or GiB unit for this range.", .memory,
              [l(.coreMemoryAvailableMiB, "Available", 0x00B4D8), l(.coreProcessResidentMiB, "Used by NereusSDR Core", 0xFFD700)]),
        .init("temperature", .core, "Core computer temperature", "Hottest sensor on the current Core computer. Its name comes only from the current sample.", .celsius,
              [l(.coreHottestZoneCelsius, "Hottest sensor", 0xFF8C00)]),
        .init("receiver", .core, "Receiver processing", "One series per slice A–E. Idle without a processing measurement is unavailable. At 100% a receiver cannot keep up.", .receiver,
              [l(.coreReceiverLoadPercentSlot0, "Slice A", 0x00B4D8), l(.coreReceiverLoadPercentSlot1, "Slice B", 0x5FFF8A), l(.coreReceiverLoadPercentSlot2, "Slice C", 0xFFD700), l(.coreReceiverLoadPercentSlot3, "Slice D", 0xC792EA), l(.coreReceiverLoadPercentSlot4, "Slice E", 0xFF8C00)], reference: 100)
    ]

    struct Detail: Identifiable {
        let id: PerformanceDetailID
        let title: String
        let owner: String
    }
    struct DetailGroup: Identifiable {
        let id: String
        let title: String
        let rows: [Detail]
    }
    private static func d(_ id: PerformanceDetailID, _ title: String, _ owner: String) -> Detail {
        Detail(id: id, title: title, owner: owner)
    }
    static let details: [DetailGroup] = [
        .init(id: "state", title: "Connection and age", rows: [
            d(.controlState, "Phone → Core control", "Phone"), d(.radioState, "Core → radio", "Core"), d(.playbackState, "Phone playback", "Phone"), d(.coreAge, "Core sample age", "Phone observation"), d(.phoneUptime, "Phone → Core session uptime", "Phone"), d(.radioUptime, "Core → radio uptime", "Core")]),
        .init(id: "routes", title: "Selected routes and attempts", rows: [
            d(.controlPeer, "Control path address", "Phone transport"), d(.mediaPeer, "Media path address", "Phone media"), d(.rendezvous, "How the path was found", "Phone"), d(.latestAttempt, "Latest attempt and age", "Phone"), d(.latestFailure, "Latest failure and age", "Phone"), d(.serviceOffers, "Addresses each end offered", "Phone"), d(.routeGeneration, "Connection number", "Phone")]),
        .init(id: "traffic", title: "Traffic and round trips", rows: [
            d(.controlTraffic, "Control received / sent", "Phone, content only"), d(.totalTraffic, "Core → app / app → Core / combined", "Phone, content only"), d(.audioTraffic, "Audio packets / content", "Phone media"), d(.coreRtt, "Core round trip and age", "Phone control"), d(.radioRtt, "Radio round trip and age", "Core"), d(.maximumRadioRtt, "Highest radio round trip", "Phone diagnostics session")]),
        .init(id: "radio", title: "Radio and local diagnostics", rows: [
            d(.radioIdentity, "Radio name / model / protocol / firmware", "Core capability"), d(.radioAddress, "Radio address on its network, as the Core sees it", "Core"), d(.radioMac, "Radio MAC", "Core capability"), d(.radioThroughput, "Radio RX / TX", "Core"), d(.radioSampleRate, "Radio sample rate", "Core"), d(.radioUdpPackets, "Radio packets seen", "Core since radio connect"), d(.radioLoss, "Radio loss · last 5 s", "Core"), d(.radioJitter, "Radio jitter", "Core"), d(.radioGap, "Longest radio packet gap · last 1 s", "Core"), d(.paVoltage, "PA voltage", "Core radio"), d(.dcVoltage, "DC voltage", "Core radio"), d(.adcOverload, "ADC overload by ADC", "Core")]),
        .init(id: "core", title: "Core audio and host", rows: [
            d(.coreSource, "Core source frames", "Core"), d(.coreEncoded, "Core encoded packets", "Core"), d(.coreAccepted, "Core accepted for sending", "Core; delivery unconfirmed"), d(.coreRefused, "Core refused packets", "Core"), d(.coreDrops, "Core source drops", "Core"), d(.cpu, "System / Core CPU", "Core computer"), d(.memory, "Available / Core resident memory", "Core computer"), d(.sensor, "Hottest sensor and name", "Core computer"), d(.receivers, "Receiver processing by slice", "Core")]),
        .init(id: "playback", title: "Phone audio and playback", rows: [
            d(.audioBackend, "Audio backend and route", "Phone"), d(.speakerQueue, "Speaker queued", "Phone"), d(.reorder, "Reorder buffer", "Phone"), d(.adaptiveHold, "Adaptive network hold", "Phone"), d(.audioDelay, "Measured audio delay", "Phone and Core clock"), d(.delivery, "Delivery delay", "Phone and Core clock"), d(.accuracy, "Audio delay accuracy bound (±)", "Phone and Core clock"), d(.delayExplanation, "Audio delay explanation", "Phone"), d(.streamGeneration, "Audio stream generation", "Phone"), d(.acceptedPackets, "Accepted audio packets", "Phone active stream"), d(.startupDiscarded, "Startup-discarded packets", "Phone active stream"), d(.decoded, "Decoded packets", "Phone active stream"), d(.concealed, "Concealed packets", "Phone active stream"), d(.late, "Late packets", "Phone active stream"), d(.invalid, "Invalid packets", "Phone active stream"), d(.duplicate, "Duplicate packets", "Phone active stream"), d(.headerRejected, "Header-rejected packets", "Phone active stream"), d(.underflows, "Playback underflows", "Phone active stream"), d(.overflows, "Playback overflows", "Phone active stream"), d(.consumedFrames, "Consumed device frames", "Phone active stream"), d(.lastPacketAge, "Last accepted packet age", "Phone active stream"), d(.lifetimeInterruptions, "Lifetime interruptions", "Phone app lifetime"), d(.arrivalJitter, "Arrival jitter", "Phone active stream"), d(.missingPackets, "Missing packets", "Phone active stream"), d(.expectedPackets, "Expected packets", "Phone active stream"), d(.concealedIntervals, "Concealed 40 ms intervals", "Phone active stream"), d(.linkInterruptions, "Link interruptions", "Phone active stream"), d(.burstDropped, "Burst-dropped audio", "Phone active stream"), d(.skipped, "Skipped audio", "Phone active stream"), d(.clockDrift, "Clock drift", "Phone active stream")]),
        .init(id: "session", title: "Session statistics", rows: [
            d(.audioBuffer, "Audio buffer", "Phone speaker queue"), d(.sessionUnderruns, "Session underruns", "Phone diagnostics session"), d(.resetScope, "Reset session stats", "Phone diagnostics session")])
    ]
}
