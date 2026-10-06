// NereusSDR for iOS: Setup, Hardware, Antenna: the Core's per-band TX or RX antenna table, one tap per band and port
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// One of the Core's two antenna tables: a row for each of its bands (15 with 2 m),
/// a column for each antenna it describes (for receive, the RX1 group and
/// the receive-only group, named for this radio), the chosen antenna
/// filled. A tap on another port chooses it for that band on the
/// connected radio. A port blocked for transmit is greyed; the transmit
/// table locks while the radio is on the air; the receive table does not.
struct AntennaRowsPanel: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher

    @State private var problem: String?
    /// The cell waiting for the Core.
    @State private var pending: String?

    static let cellWidth: CGFloat = 40

    var body: some View {
        let table = dispatcher.antennaTable(control, in: category)
        VStack(alignment: .leading, spacing: 6) {
            Text(control.label)
                .font(.body.weight(.semibold))
            if case .antennaRows(let rows)? = control.binding, let grid = table.grid {
                header(rows)
                ForEach(Array(rows.rows.enumerated()), id: \.offset) { index, row in
                    HStack(spacing: 0) {
                        Text(row.label)
                            .font(.subheadline.monospacedDigit())
                            .frame(maxWidth: .infinity, alignment: .leading)
                        ForEach(Array(rows.columns.enumerated()), id: \.offset) { column, described in
                            cell(row: row, column: described, index: column,
                                 selected: grid.selected[index][column], blocked: grid.blocked[column],
                                 enabled: table.reason == nil)
                        }
                    }
                }
            } else if let gridReason = table.gridReason {
                Text(gridReason)
                    .font(.footnote)
                    .foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier("\(control.id).gridReason")
            }
            if let reason = table.reason, reason != table.gridReason {
                Text(reason)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .accessibilityIdentifier("\(control.id).reason")
            }
            if let problem {
                Text(problem)
                    .font(.footnote)
                    .foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier("\(control.id).problem")
            }
        }
        .accessibilityIdentifier(control.id)
    }

    @ViewBuilder
    private func header(_ rows: SetupDescription.AntennaRows) -> some View {
        if !rows.columnGroups.isEmpty {
            HStack(spacing: 0) {
                Spacer(minLength: 0)
                ForEach(Array(rows.columnGroups.enumerated()), id: \.offset) { _, group in
                    Text(group.label)
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                        .frame(width: Self.cellWidth * CGFloat(group.columns.count))
                }
            }
        }
        HStack(spacing: 0) {
            Spacer(minLength: 0)
            ForEach(Array(rows.columns.enumerated()), id: \.offset) { _, column in
                Text(column.label)
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
                    .frame(width: Self.cellWidth)
            }
        }
    }

    private func cell(row: SetupDescription.AntennaRow, column: SetupDescription.AntennaColumn, index: Int,
                      selected: Bool, blocked: Bool, enabled: Bool) -> some View {
        let tooltip = row.cells.indices.contains(index) ? row.cells[index].tooltip : ""
        let key = "\(row.band).\(column.id)"
        return Button {
            choose(band: row.band, column: column.id, key: key)
        } label: {
            Image(systemName: selected ? "largecircle.fill.circle" : "circle")
                .font(.title3)
                .foregroundStyle(blocked ? Color.secondary.opacity(0.4) : selected ? Color.accentColor : Color.secondary)
                .frame(width: Self.cellWidth, height: 36)
        }
        .buttonStyle(.plain)
        .disabled(!enabled || blocked || pending != nil)
        .accessibilityLabel(tooltip)
        .accessibilityValue(selected ? "Chosen" : blocked ? SetupControlDispatcher.antennaBlockedReason : "")
        .accessibilityIdentifier("antenna.\(control.id).\(key)")
    }

    private func choose(band: Int, column: String, key: String) {
        problem = nil
        switch dispatcher.admitAntenna(control, in: category) {
        case .failure(let refusal):
            problem = refusal.reason
        case .success(let admission):
            pending = key
            let dispatcher = dispatcher
            Task { @MainActor in
                let outcome = await dispatcher.performAntenna(admission, band: band, column: column)
                pending = nil
                switch outcome {
                case .applied, .awaitingConfirmation:
                    problem = nil
                case .refused(let reason), .notSent(let reason):
                    problem = reason
                }
            }
        }
    }
}
