// NereusSDR for iOS: native connection and performance charts and details
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI
import UIKit

/// The owner publishes a coherent input while this page is visible. This view owns no sampler.
struct ConnectionPerformanceView: View {
    let input: ConnectionPerformanceInput
    let onSelectRange: (DiagnosticsRange) -> Void
    let onResetSessionStats: () -> Void

    @Environment(\.dynamicTypeSize) private var textSize
    @State private var section: PerformanceSection = .connection
    @State private var chosen: [String: DiagnosticsMetric] = [:]
    @State private var selections: [String: Set<DiagnosticsMetric>] = [:]
    @State private var showingHelp: Set<String> = []
    @State private var copied = false

    init(input: ConnectionPerformanceInput, initialSection: PerformanceSection = .connection,
         onSelectRange: @escaping (DiagnosticsRange) -> Void,
         onResetSessionStats: @escaping () -> Void) {
        self.input = input
        self.onSelectRange = onSelectRange
        self.onResetSessionStats = onResetSessionStats
        _section = State(initialValue: initialSection)
    }

    var body: some View {
        GeometryReader { geometry in
            ScrollView {
                VStack(alignment: .leading, spacing: 12) {
                    selectors
                    statusCard
                    if section == .connection { routes }
                    if section == .details {
                        details
                    } else {
                        chartGrid(width: geometry.size.width)
                        Text("Gaps mark unavailable readings, stale samples and connection changes. Earlier sessions are historical.")
                            .font(.footnote).foregroundStyle(ChromeColours.textDim)
                    }
                }
                .padding(12)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
        .background(ChromeColours.page.ignoresSafeArea())
        .foregroundStyle(ChromeColours.text)
        .accessibilityIdentifier("connectionPerformance.page")
    }

    private var selectors: some View {
        Group {
            if textSize.isAccessibilitySize {
                VStack(alignment: .leading, spacing: 8) {
                    sectionPicker
                    rangePicker
                }
            } else {
                HStack(alignment: .top, spacing: 12) {
                    sectionPicker
                    rangePicker
                    Spacer(minLength: 0)
                }
            }
        }
        .frame(minHeight: 54)
    }

    private var sectionPicker: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("Section").font(.caption).foregroundStyle(ChromeColours.textDim)
            Picker("Section", selection: $section) {
                ForEach(PerformanceSection.allCases) { part in Text(part.rawValue).tag(part) }
            }
            .pickerStyle(.menu)
            .accessibilityIdentifier("connectionPerformance.section")
        }
    }

    private var rangePicker: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("History").font(.caption).foregroundStyle(ChromeColours.textDim)
            Picker("History range", selection: Binding(get: { input.selectedRange }, set: onSelectRange)) {
                ForEach(DiagnosticsRange.allCases, id: \.self) { range in
                    Text(range.label).tag(range)
                }
            }
            .pickerStyle(.menu)
            .accessibilityIdentifier("connectionPerformance.range")
        }
    }

    private var statusCard: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(input.status.text { $0 }).font(.headline).fixedSize(horizontal: false, vertical: true)
            if let wall = input.displayWallTime {
                Text("Displayed at \(wall)").font(.footnote).foregroundStyle(ChromeColours.textDim)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .cardStyle()
        .accessibilityElement(children: .combine)
    }

    private var routes: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Selected paths").font(.headline)
            routeCard("Control", input.controlRoute)
            routeCard("Media", input.mediaRoute)
            Text("Latest attempt: \(input.latestAttempt.text { $0 })")
            Text("Latest failure: \(input.latestFailure.text { $0 })")
        }
        .font(.subheadline)
    }

    private func routeCard(_ title: String, _ route: DiagnosticsSelectedRoute) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title).font(.headline)
            if let headline = route.headline {
                Text(headline).fontWeight(.semibold).fixedSize(horizontal: false, vertical: true)
            }
            ForEach(route.lines, id: \.self) { line in
                Text(line).textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .cardStyle()
    }

    private func chartGrid(width: CGFloat) -> some View {
        let twoColumns = width >= 680 && !textSize.isAccessibilitySize
        return LazyVGrid(columns: Array(repeating: GridItem(.flexible(), alignment: .top), count: twoColumns ? 2 : 1), spacing: 12) {
            ForEach(ConnectionPerformanceCatalog.charts.filter { $0.section == section }) { chart in
                chartCard(chart)
            }
        }
    }

    private func chartCard(_ chart: PerformanceChart) -> some View {
        let unit = chart.displayUnit(input.series)
        let selected = selections[chart.id] ?? []
        return VStack(alignment: .leading, spacing: 9) {
            HStack(alignment: .top) {
                Text(chart.title).font(.headline).fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 4)
                Button(showingHelp.contains(chart.id) ? "Hide help" : "Help") {
                    if !showingHelp.insert(chart.id).inserted { showingHelp.remove(chart.id) }
                }
                .buttonStyle(PerformanceButtonStyle())
                .accessibilityIdentifier("connectionPerformance.\(chart.id).help")
            }
            if showingHelp.contains(chart.id) {
                Text(chart.explanation).font(.footnote).foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            PerformancePlot(chart: chart, series: input.series, range: input.selectedRange,
                            visibleMetrics: selected, unit: unit)
                .frame(height: 150)
                .accessibilityLabel("\(chart.title), \(input.selectedRange.label) history")
                .accessibilityValue("\(chart.lines.count) series; gaps mark unavailable periods and connection changes")
            ForEach(chart.lines) { line in
                legendRow(line, unit: unit)
            }
            Picker("Choose series for \(chart.title)", selection: Binding(
                get: { chosen[chart.id] ?? chart.lines[0].metric },
                set: { chosen[chart.id] = $0 })) {
                    ForEach(chart.lines) { line in Text(line.title).tag(line.metric) }
                }
                .pickerStyle(.menu)
                .accessibilityIdentifier("connectionPerformance.\(chart.id).series")
            Group {
                if textSize.isAccessibilitySize {
                    VStack(alignment: .leading, spacing: 8) {
                        seriesButton("Focus", .focus, chart: chart, selected: selected)
                        seriesButton("Compare", .compare, chart: chart, selected: selected)
                        seriesButton("Show all", .showAll, chart: chart, selected: selected)
                    }
                } else {
                    HStack(spacing: 8) {
                        seriesButton("Focus", .focus, chart: chart, selected: selected)
                        seriesButton("Compare", .compare, chart: chart, selected: selected)
                        seriesButton("Show all", .showAll, chart: chart, selected: selected)
                    }
                }
            }
            .buttonStyle(PerformanceButtonStyle())
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .cardStyle()
        .accessibilityIdentifier("connectionPerformance.chart.\(chart.id)")
    }

    private func legendRow(_ line: PerformanceLine, unit: ChartDisplayUnit) -> some View {
        let value = input.reading(for: line.metric).text { unit.format($0) }
        return Group {
            if textSize.isAccessibilitySize {
                VStack(alignment: .leading, spacing: 2) {
                    Label(line.title, systemImage: "circle.fill")
                        .foregroundStyle(Color(hex: line.color))
                    Text(value).font(.subheadline.monospacedDigit())
                        .fixedSize(horizontal: false, vertical: true)
                }
            } else {
                HStack(alignment: .firstTextBaseline, spacing: 7) {
                    Circle().fill(Color(hex: line.color)).frame(width: 9, height: 9)
                    Text(line.title).font(.subheadline).fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 4)
                    Text(value).font(.subheadline.monospacedDigit())
                        .multilineTextAlignment(.trailing).fixedSize(horizontal: false, vertical: true)
                }
            }
        }
        .accessibilityElement(children: .combine)
    }

    private func seriesButton(_ title: String, _ action: PerformanceSeriesSelection.Action,
                              chart: PerformanceChart, selected: Set<DiagnosticsMetric>) -> some View {
        Button(title) {
            selections[chart.id] = PerformanceSeriesSelection.apply(action, to: selected,
                metric: chosen[chart.id] ?? chart.lines[0].metric)
        }
    }

    private var details: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 8) {
                Button(copied ? "Copied" : "Copy details") {
                    UIPasteboard.general.string = input.copyText
                    copied = true
                }
                Button("Reset session stats", action: onResetSessionStats)
                    .disabled(input.reset.reason != nil)
                    .opacity(input.reset.reason == nil ? 1 : 0.5)
            }
            .buttonStyle(PerformanceButtonStyle())
            if let reason = input.reset.reason {
                Text("Reset unavailable: \(reason)").font(.footnote).foregroundStyle(ChromeColours.textDim)
            } else {
                Text("Resets this session's underruns and the highest radio round trip only.")
                    .font(.footnote).foregroundStyle(ChromeColours.textDim)
            }
            ForEach(ConnectionPerformanceCatalog.details) { group in
                VStack(alignment: .leading, spacing: 9) {
                    Text(group.title).font(.headline)
                    ForEach(group.rows) { row in
                        VStack(alignment: .leading, spacing: 2) {
                            Text(row.title).font(.subheadline.weight(.semibold))
                            Text(input.detail(for: row.id).text { $0 })
                                .font(.subheadline).fixedSize(horizontal: false, vertical: true)
                                .textSelection(.enabled)
                            Text(row.owner).font(.caption).foregroundStyle(ChromeColours.textDim)
                        }
                        .accessibilityElement(children: .combine)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .cardStyle()
            }
        }
    }
}

private struct PerformancePlot: View {
    let chart: PerformanceChart
    let series: [DiagnosticsMetric: DiagnosticsSeries]
    let range: DiagnosticsRange
    let visibleMetrics: Set<DiagnosticsMetric>
    let unit: ChartDisplayUnit

    var body: some View {
        let maximum = chart.axisMaximum(series)
        VStack(alignment: .leading, spacing: 2) {
            Text(unit.format(maximum)).font(.caption2.monospacedDigit())
            Canvas { context, size in
                let top = max(maximum, 1)
                var grid = Path()
                for fraction in [0.0, 0.5, 1.0] {
                    let y = size.height * (1 - fraction)
                    grid.move(to: CGPoint(x: 0, y: y))
                    grid.addLine(to: CGPoint(x: size.width, y: y))
                }
                context.stroke(grid, with: .color(ChromeColours.panelEdge), lineWidth: 1)
                if let reference = chart.reference {
                    let y = size.height * (1 - min(reference / top, 1))
                    var marker = Path()
                    marker.move(to: CGPoint(x: 0, y: y))
                    marker.addLine(to: CGPoint(x: size.width, y: y))
                    context.stroke(marker, with: .color(.orange), style: StrokeStyle(lineWidth: 1, dash: [4, 3]))
                }
                for line in chart.lines where visibleMetrics.isEmpty || visibleMetrics.contains(line.metric) {
                    guard let data = series[line.metric] else { continue }
                    var path = Path()
                    for segment in PerformanceChart.pathSegments(data) {
                        for (index, point) in segment.enumerated() {
                            let x = size.width * min(max(point.seconds / Double(range.seconds), 0), 1)
                            let y = size.height * (1 - min(max(point.value / top, 0), 1))
                            let location = CGPoint(x: x, y: y)
                            if index == 0 {
                                path.move(to: location)
                                // An isolated point remains visible.
                                path.addEllipse(in: CGRect(x: x - 1.5, y: y - 1.5, width: 3, height: 3))
                                path.move(to: location)
                            } else {
                                path.addLine(to: location)
                            }
                        }
                    }
                    context.stroke(path, with: .color(Color(hex: line.color)), lineWidth: 2)
                }
            }
            HStack {
                Text("0 \(unit.label)")
                Spacer()
                if chart.reference != nil { Text("100% Cannot keep up") }
            }
            .font(.caption2).foregroundStyle(ChromeColours.textDim)
            HStack {
                Text("\(range.label) ago")
                Spacer()
                Text("Now")
            }
            .font(.caption2).foregroundStyle(ChromeColours.textDim)
        }
    }
}

struct ChartDisplayUnit {
    let label: String
    let divisor: Double

    func format(_ value: Double) -> String {
        guard value.isFinite else { return "Unavailable: nonfinite reading" }
        return "\((value / divisor).formatted(.number.precision(.fractionLength(0...2)))) \(label)"
    }
}

enum PerformanceSeriesSelection {
    enum Action { case focus, compare, showAll }

    /// Empty selection means all lines are visible.
    static func apply(_ action: Action, to current: Set<DiagnosticsMetric>,
                      metric: DiagnosticsMetric) -> Set<DiagnosticsMetric> {
        switch action {
        case .focus: return [metric]
        case .showAll: return []
        case .compare:
            if current.isEmpty { return [metric] }
            var next = current
            if !next.insert(metric).inserted { next.remove(metric) }
            return next
        }
    }
}

extension PerformanceChart {
    static func pathSegments(_ data: DiagnosticsSeries) -> [[DiagnosticsPoint]] {
        var segments: [[DiagnosticsPoint]] = []
        var current: [DiagnosticsPoint] = []
        for point in data.points {
            guard point.seconds.isFinite, point.value.isFinite else {
                if !current.isEmpty { segments.append(current) }
                current = []
                continue
            }
            if let previous = current.last,
               point.breakBefore || previous.sessionSegment != point.sessionSegment ||
               point.seconds <= previous.seconds ||
               point.seconds - previous.seconds > data.maximumConnectGapSeconds {
                segments.append(current)
                current = []
            }
            current.append(point)
        }
        if !current.isEmpty { segments.append(current) }
        return segments
    }

    func displayUnit(_ series: [DiagnosticsMetric: DiagnosticsSeries]) -> ChartDisplayUnit {
        let maximum = axisMaximum(series)
        switch unit {
        case .kbps where maximum >= 1_000: return ChartDisplayUnit(label: "Mbps", divisor: 1_000)
        case .kbit where maximum >= 1_000: return ChartDisplayUnit(label: "Mbps", divisor: 1_000)
        case .memory where maximum >= 1_024: return ChartDisplayUnit(label: "GiB", divisor: 1_024)
        default: return ChartDisplayUnit(label: unit.label, divisor: 1)
        }
    }

    func axisMaximum(_ series: [DiagnosticsMetric: DiagnosticsSeries]) -> Double {
        max(1, reference ?? 0, lines.flatMap { line in
            series[line.metric]?.points.compactMap { $0.value.isFinite ? max(0, $0.value) : nil } ?? []
        }.max() ?? 0)
    }
}

extension DiagnosticsRange {
    var label: String {
        switch self {
        case .oneMinute: "1 min"
        case .fiveMinutes: "5 min"
        case .fifteenMinutes: "15 min"
        case .oneHour: "1 h"
        case .oneDay: "24 h"
        case .sevenDays: "7 d"
        }
    }
}

private struct PerformanceButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.subheadline.weight(.semibold))
            .foregroundStyle(ChromeColours.textBright)
            .frame(minHeight: 44)
            .padding(.horizontal, 10)
            .background(ChromeColours.button.opacity(configuration.isPressed ? 0.7 : 1))
            .clipShape(RoundedRectangle(cornerRadius: 7))
            .overlay(RoundedRectangle(cornerRadius: 7).stroke(ChromeColours.buttonBorder))
    }
}

private struct PerformanceCard: ViewModifier {
    func body(content: Content) -> some View {
        content.padding(12)
            .background(ChromeColours.panel)
            .clipShape(RoundedRectangle(cornerRadius: 9))
            .overlay(RoundedRectangle(cornerRadius: 9).stroke(ChromeColours.panelEdge))
    }
}

private extension View {
    func cardStyle() -> some View { modifier(PerformanceCard()) }
}

private extension Color {
    init(hex: UInt32) {
        self.init(.sRGB, red: Double((hex >> 16) & 0xFF) / 255,
                  green: Double((hex >> 8) & 0xFF) / 255,
                  blue: Double(hex & 0xFF) / 255, opacity: 1)
    }
}
