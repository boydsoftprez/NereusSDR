// NereusSDR for iOS: which setting keys the Core keeps and which each app keeps itself
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Whether a setting key is kept by the Core (station-scoped, proxied to
/// the app) or by each app itself (operator-local, never sent), by the
/// rules of link document section 8: keys the Core owns by code first, then
/// with a trailing `_<digits>` ignored, exact exception keys, prefixes and
/// whole keys, the first match winning. A key that matches nothing is kept
/// by each app. The conformance tests hold these tables to `surface.json`.
public enum SettingsScope: Sendable, Equatable {
    case station
    case operatorLocal

    /// The scope of `key`.
    public static func of(_ key: String) -> SettingsScope {
        if ownedByCoreCode(key) {
            return .station
        }
        let stripped = strippingPanSuffix(key)
        if let rule = exceptions.first(where: { $0.text == stripped }) {
            return rule.scope
        }
        if let rule = prefixes.first(where: { stripped.hasPrefix($0.text) }) {
            return rule.scope
        }
        if let rule = wholeKeys.first(where: { $0.text == stripped }) {
            return rule.scope
        }
        return .operatorLocal
    }

    struct Rule: Sendable {
        let text: String
        let scope: SettingsScope
    }

    // Link document section 8, the settings scope table: tier 1.
    static let exceptions: [Rule] = [
        Rule(text: "TciLogWindowGeometry", scope: .operatorLocal),
        Rule(text: "TciLogWindowAutoScroll", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/ColumnWidths", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/SortColumn", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/SortAscending", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/VisibleColumns", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/ColumnFilters", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/BandFilter", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/DistanceMiles", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/DirectionAsCardinal", scope: .operatorLocal),
        Rule(text: "FreeDvReporter/FrequencyAsKhz", scope: .operatorLocal),
    ]

    // Tier 2, in the order the Core checks them.
    static let prefixes: [Rule] = [
        Rule(text: "hardware/", scope: .station),
        Rule(text: "Tci", scope: .operatorLocal),
        Rule(text: "Slice", scope: .station),
        Rule(text: "Vfo", scope: .station),
        Rule(text: "radios/", scope: .operatorLocal),
        Rule(text: "ConnectionTargets/", scope: .operatorLocal),
        Rule(text: "RemoteVax/", scope: .operatorLocal),
        Rule(text: "PGXL_", scope: .station),
        Rule(text: "TGXL_", scope: .station),
        Rule(text: "RfKit_", scope: .station),
        Rule(text: "StationTci_", scope: .station),
        Rule(text: "DxCluster", scope: .station),
        Rule(text: "Rbn", scope: .station),
        Rule(text: "Pota", scope: .station),
        Rule(text: "PskReporter", scope: .station),
        Rule(text: "FreeDv", scope: .station),
        // WSJT-X and SpotCollector listen on the computer they run on, so
        // each device keeps its own (link 8; parity Task 19).
        Rule(text: "SpotCollector", scope: .operatorLocal),
        Rule(text: "Wsjtx", scope: .operatorLocal),
        Rule(text: "User/", scope: .station),
        Rule(text: "Notch", scope: .station),
        Rule(text: "DspOptions", scope: .station),
        Rule(text: "Nb", scope: .station),
        Rule(text: "Snb", scope: .station),
        Rule(text: "Rade", scope: .station),
        // The Core's filter presets (`filters/<mode>/<slot>/...`), which its
        // catalogue carries.
        Rule(text: "filters/", scope: .station),
        // Setup > Display > Grid & Scales' dB Max and dB Min per band: the
        // Core's per-band values win (link 8.2, parity ruling C12).
        Rule(text: "DisplayGridMax_", scope: .station),
        Rule(text: "DisplayGridMin_", scope: .station),
    ]

    // Tier 3.
    static let wholeKeys: [Rule] = [
        Rule(text: "DisplayFftSize", scope: .station),
        Rule(text: "DisplayFftWindow", scope: .station),
        Rule(text: "DisplayHzPerBinTarget", scope: .station),
        Rule(text: "DisplaySpectrumFps", scope: .station),
        Rule(text: "DisplayTxFftSize", scope: .station),
        Rule(text: "DisplayTxWindowType", scope: .station),
        Rule(text: "DisplayTxPanDetector", scope: .station),
        Rule(text: "DisplayTxPanAveraging", scope: .station),
        Rule(text: "DisplayTxPanAvTimeMs", scope: .station),
        Rule(text: "DisplayTxPanNormalize", scope: .station),
        Rule(text: "DisplayTxWfDetector", scope: .station),
        Rule(text: "DisplayTxWfAveraging", scope: .station),
        Rule(text: "DisplayTxWfAvTimeMs", scope: .station),
        Rule(text: "audio/DspRate", scope: .station),
        Rule(text: "audio/DspBlockSize", scope: .station),
        Rule(text: "BandPlanName", scope: .station),
        Rule(text: "BandPlanRegion", scope: .station),
        Rule(text: "ExtendedTransmit", scope: .station),
        Rule(text: "Region", scope: .station),
        Rule(text: "CWPitch", scope: .station),
        Rule(text: "Nr3ModelPath", scope: .station),
        Rule(text: "StationCallsign", scope: .station),
        Rule(text: "RX1_MeterCalOffsetDb", scope: .station),
        Rule(text: "RX1_DisplayCalOffsetDb", scope: .station),
        Rule(text: "RxMeterCalOffsetDbByRadio", scope: .station),
        Rule(text: "RxDisplayCalOffsetDbByRadio", scope: .station),
        Rule(text: "RX1_PreampOffsetsDb", scope: .station),
        Rule(text: "PeripheralsMigrationDone", scope: .station),
        Rule(text: "SwrProtectionEnabled", scope: .station),
        Rule(text: "SwrProtectionLimit", scope: .station),
        Rule(text: "SwrTuneProtectionEnabled", scope: .station),
        Rule(text: "TunePowerSwrIgnore", scope: .station),
        Rule(text: "TxInhibitMonitorEnabled", scope: .station),
        Rule(text: "TxInhibitMonitorReversed", scope: .station),
        Rule(text: "WindBackPowerSwr", scope: .station),
        Rule(text: "MultimeterDelayMs", scope: .station),
        Rule(text: "NetworkWatchdogEnabled", scope: .station),
        // Remote transmit's time-outs are the Core's: they end any device's
        // key, and the station's own.
        Rule(text: "MoxTimeOutEnabled", scope: .station),
        Rule(text: "MoxTimeOutSeconds", scope: .station),
        Rule(text: "PingTimeOutEnabled", scope: .station),
        Rule(text: "PingTimeOutSeconds", scope: .station),
        Rule(text: "PingTimeOutHost", scope: .station),
        Rule(text: "RemoteMoxTimeOutEnabled", scope: .station),
        Rule(text: "RemoteMoxTimeOutSeconds", scope: .station),
        // Receive only is the Core's setting: it stops every key on the
        // station, whichever device asks.
        Rule(text: "RxOnly", scope: .station),
        Rule(text: "ModMon/FbStream", scope: .station),
        // Disable HF PA switches the Core's radio's PA off (link 6.3,
        // transmitSettingsVersion 11): the Core's setting.
        Rule(text: "DisableHfPa", scope: .station),
        Rule(text: "ExtendedTxAllowed", scope: .operatorLocal),
        // Prevent transmitting on a different band is the Core's General
        // row (transmitSettingsVersion 14): the Core refuses the key, so it
        // is the station's setting.
        Rule(text: "PreventTxOnDifferentBandToRx", scope: .station),
    ]

    /// Link document section 8.2: families the Core changes only through
    /// their objects and commands. They are station-scoped (a write of one
    /// reaches the Core, which refuses it with its own value). Matched
    /// without regard to case.
    static func ownedByCoreCode(_ key: String) -> Bool {
        let lower = key.lowercased()
        if lower.hasPrefix("dspassets/") || lower == "nr3modelpath" {
            return true
        }
        if isNotchListKey(lower) {
            return true
        }
        guard lower.hasPrefix("hardware/") else {
            return false
        }
        let parts = lower.split(separator: "/", omittingEmptySubsequences: false)
        if parts.count >= 5, parts[2] == "options", ["stepatt", "autoatt", "preamp"].contains(parts[3]) {
            return true
        }
        if parts.count >= 5, parts[2] == "alex", parts[3] == "antenna" {
            return true
        }
        if parts.count >= 4, parts[2] == "puresignal" {
            return true
        }
        return parts.count >= 6 && parts[2] == "slices" && parts[4] == "nnr"
    }

    /// `NotchCount`, `NotchGlobalEnabled`, `NotchAutoIncrease` and
    /// `Notch<N>Center|Width|Active`, N without leading zeros.
    private static func isNotchListKey(_ lower: String) -> Bool {
        guard lower.hasPrefix("notch") else {
            return false
        }
        let rest = lower.dropFirst("notch".count)
        if rest == "count" || rest == "globalenabled" || rest == "autoincrease" {
            return true
        }
        for suffix in ["center", "width", "active"] where rest.hasSuffix(suffix) {
            let number = rest.dropLast(suffix.count)
            guard !number.isEmpty, number.allSatisfy({ $0.isASCII && $0.isNumber }) else {
                return false
            }
            return number == "0" || number.first != "0"
        }
        return false
    }

    /// The key without a trailing `_<digits>` (a panadapter index).
    static func strippingPanSuffix(_ key: String) -> String {
        guard let underscore = key.lastIndex(of: "_") else {
            return key
        }
        let suffix = key[key.index(after: underscore)...]
        guard !suffix.isEmpty, suffix.allSatisfy({ $0.isWholeNumber }) else {
            return key
        }
        return String(key[..<underscore])
    }
}
