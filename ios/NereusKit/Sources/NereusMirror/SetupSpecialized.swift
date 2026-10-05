// NereusSDR for iOS: the Setup controls that need their own panel rather than the generic rows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The closed kinds a generic row cannot carry: the notch list, the
/// settings check, the antenna grids, the PA telemetry readings and the
/// CFC band editor (V19). Each
/// is drawn by its own panel; until one is provided, the page shows the
/// control visibly disabled with a plain reason.
public enum SetupSpecialized: String, Equatable, Sendable, CaseIterable {
    case notchTable
    case settingsHygiene
    case antennaRows
    case paTelemetry
    case cfcBands
}

public extension SetupDescription.Control {
    /// The panel this control needs, or nil for a generic row.
    var specialized: SetupSpecialized? {
        switch binding {
        case .table?:
            return .notchTable
        case .settingsHygiene?:
            return .settingsHygiene
        case .antennaRows?:
            return .antennaRows
        case .cfcProfile?:
            return .cfcBands
        case .telemetry? where modern == nil:
            // A V13 to V16 telemetry row is a generic reading.
            return .paTelemetry
        default:
            if kind == .settingsHygiene { return .settingsHygiene }
            return nil
        }
    }
}
