// NereusSDR for iOS: immutable input for the connection diagnostics presentation
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMirror

/// Historical text is explanatory only. A previous value never becomes a current value.
enum DiagnosticsReading<Value> {
    case current(Value)
    case missing(reason: String, historical: Value? = nil)
    case stale(reason: String, historical: Value? = nil)
    case unsupported(reason: String)
    case disconnected(reason: String, historical: Value? = nil)

    var current: Value? {
        if case .current(let value) = self { return value }
        return nil
    }

    func text(_ format: (Value) -> String) -> String {
        switch self {
        case .current(let value): return format(value)
        case .missing(let reason, let old): return Self.absent("Unavailable", reason, old, format)
        case .stale(let reason, let old): return Self.absent("Stale", reason, old, format)
        // The reason is already a plain sentence saying what is never reported here.
        case .unsupported(let reason): return reason
        case .disconnected(let reason, let old): return Self.absent("Disconnected", reason, old, format)
        }
    }

    private static func absent(_ state: String, _ reason: String, _ historical: Value?,
                               _ format: (Value) -> String) -> String {
        guard let historical else { return "\(state): \(reason)" }
        return "\(state): \(reason). Previous: \(format(historical)) (historical)"
    }
}

enum DiagnosticsActionAvailability {
    case enabled
    case disabled(reason: String)

    var reason: String? {
        if case .disabled(let reason) = self { return reason }
        return nil
    }
}

/// The selected socket or nominated media candidate pair, never a saved address.
struct DiagnosticsSelectedRoute {
    let numericPeer: DiagnosticsReading<String>
    let addressFamily: DiagnosticsReading<String>
    let transport: DiagnosticsReading<String>
    let path: DiagnosticsReading<String>
    let generation: DiagnosticsReading<String>
    let rendezvousRole: DiagnosticsReading<String>
    /// What the address is, in the operator's words: the Core's, a relay's.
    var peerLabel = "Address"
    /// The remote access service that set up a relayed path, when known.
    /// This is the service's host, not necessarily the relay server's.
    var relayService: String?
    /// False where the address is only this phone's hand-off to a relay,
    /// which says nothing about the path; it stays in the details.
    var showsAddress = true

    /// Path, address family and transport, as far as each is known.
    var headline: String? {
        let known = [path, addressFamily, transport].compactMap(\.current)
        return known.isEmpty ? nil : known.joined(separator: " · ")
    }

    /// The address, the relay service, and why anything is missing, each once.
    var lines: [String] {
        var rows: [String] = []
        if showsAddress, let peer = numericPeer.current { rows.append("\(peerLabel): \(peer)") }
        if let relayService { rows.append("Remote access service: \(relayService)") }
        if numericPeer.current == nil { rows.append(numericPeer.text { $0 }) }
        return rows
    }
}

/// One coherent publication. The owner gathers and ages readings and supplies bounded plots.
/// `sampledAtMonotonicMilliseconds` is not a wall clock and is never printed as one.
struct ConnectionPerformanceInput {
    let sampledAtMonotonicMilliseconds: Int64
    let displayWallTime: String?
    let status: DiagnosticsReading<String>
    let selectedRange: DiagnosticsRange
    let series: [DiagnosticsMetric: DiagnosticsSeries]
    let current: [DiagnosticsMetric: DiagnosticsReading<Double>]
    let details: [PerformanceDetailID: DiagnosticsReading<String>]
    let controlRoute: DiagnosticsSelectedRoute
    let mediaRoute: DiagnosticsSelectedRoute
    let latestAttempt: DiagnosticsReading<String>
    let latestFailure: DiagnosticsReading<String>
    let reset: DiagnosticsActionAvailability

    func reading(for metric: DiagnosticsMetric) -> DiagnosticsReading<Double> {
        current[metric] ?? .missing(reason: "This reading has not been supplied")
    }

    func detail(for id: PerformanceDetailID) -> DiagnosticsReading<String> {
        switch id {
        case .controlPeer: return controlRoute.numericPeer
        case .mediaPeer: return mediaRoute.numericPeer
        case .rendezvous:
            return .current("Control: \(controlRoute.rendezvousRole.text { $0 }); media: \(mediaRoute.rendezvousRole.text { $0 })")
        case .latestAttempt: return latestAttempt
        case .latestFailure: return latestFailure
        case .routeGeneration:
            return .current("Control: \(controlRoute.generation.text { $0 }); media: \(mediaRoute.generation.text { $0 })")
        default: break
        }
        return details[id] ?? .missing(reason: "This reading has not been supplied")
    }

    /// Matches every visible detail row, route and status from this single publication.
    var copyText: String {
        var rows = ["Connection and performance"]
        if let displayWallTime { rows.append("Displayed at: \(displayWallTime)") }
        rows.append("Status: \(status.text { $0 })")
        rows.append(contentsOf: routeText("Control", controlRoute))
        rows.append(contentsOf: routeText("Media", mediaRoute))
        rows.append("Latest attempt: \(latestAttempt.text { $0 })")
        rows.append("Latest failure: \(latestFailure.text { $0 })")
        rows.append("\nCurrent chart readings (\(selectedRange.label))")
        for chart in ConnectionPerformanceCatalog.charts {
            let unit = chart.displayUnit(series)
            for line in chart.lines {
                rows.append("\(chart.title), \(line.title): \(reading(for: line.metric).text { unit.format($0) })")
            }
        }
        for group in ConnectionPerformanceCatalog.details {
            rows.append("\n\(group.title)")
            for row in group.rows {
                rows.append("\(row.title) [\(row.owner)]: \(detail(for: row.id).text { $0 })")
            }
        }
        let resetText = reset.reason.map { "Unavailable: \($0)" } ?? "Available"
        rows.append("Reset session stats: \(resetText)")
        return rows.joined(separator: "\n")
    }

    private func routeText(_ title: String, _ route: DiagnosticsSelectedRoute) -> [String] {
        (route.headline.map { ["\(title): \($0)"] } ?? [])
            + route.lines.map { "\(title): \($0)" }
    }
}
