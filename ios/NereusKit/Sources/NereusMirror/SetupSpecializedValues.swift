// NereusSDR for iOS: what the closed Setup panels read: the notch list, the settings check's answer, the antenna rows, a PA reading
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreFoundation
import Foundation
import NereusLink

/// One notch of the Core's notch list.
public struct SetupNotch: Equatable, Sendable, Identifiable {
    public let id: Int64
    public let centreHz: Double
    public let widthHz: Double
    public let active: Bool
}

/// The whole notch list at one revision, read at once.
public struct SetupNotchList: Equatable, Sendable {
    public let revision: Int64
    public let rows: [SetupNotch]

    public func row(_ id: Int64) -> SetupNotch? {
        rows.first { $0.id == id }
    }
}

/// One change to one notch, as the notch table's row actions send it.
public enum SetupNotchEdit: Equatable, Sendable {
    case move(centreHz: Double, widthHz: Double)
    case setActive(Bool)
    case delete
}

/// The notch table as the panel draws it: the list, or why it cannot be
/// shown, and why nothing can be changed now (nil when it can).
public struct SetupNotchTable: Equatable, Sendable {
    public let list: SetupNotchList?
    public let listReason: String?
    public let reason: String?
}

/// The settings check's actions that send anything.
public enum SetupHygieneAction: String, Equatable, Sendable {
    case validate
    case repair
    case forget
}

/// One problem the Core found in the radio's settings.
public struct SetupHygieneIssue: Equatable, Sendable {
    public enum Severity: String, Equatable, Sendable {
        case info, warning, critical
    }

    public let severity: Severity
    public let key: String
    public let summary: String
    public let detail: String
    /// Diagnostic text only; never acted on.
    public let fixActionId: String
}

/// The Core's answer to a settings check, for the radio it names.
public struct SetupHygieneReport: Equatable, Sendable {
    public let mac: String
    public let issues: [SetupHygieneIssue]
}

/// The settings check as the panel draws it.
public struct SetupHygienePanel: Equatable, Sendable {
    /// The current answer for this radio in this session, if there is one.
    public let report: SetupHygieneReport?
    /// Why there is no answer to show.
    public let reportReason: String?
    /// A check, a repair or a forget is waiting for the Core.
    public let busy: Bool
    /// Why Re-validate cannot be used now; nil when it can.
    public let validateReason: String?
    /// Why Forget cannot be used now; nil when it can.
    public let forgetReason: String?
    /// Why Repair cannot be used now; nil when it can.
    public let repairReason: String?
    /// Reset (a version 1 Core's) is never sent; this is why, in the
    /// Core's words.
    public let resetReason: String
    /// The question Forget asks first.
    public let confirmation: SetupDescription.Confirmation?
    /// The question Repair asks first.
    public let repairConfirmation: SetupDescription.Confirmation?
    public let validateLabel: String
    /// Empty when the Core does not describe Repair.
    public let repairLabel: String
    /// Empty when the Core does not describe Reset.
    public let resetLabel: String
    public let forgetLabel: String
}

/// One antenna table's cells: which is chosen on each band, and which
/// columns are blocked for transmit.
public struct SetupAntennaGrid: Equatable, Sendable {
    /// By row (band), by described column.
    public let selected: [[Bool]]
    /// By described column.
    public let blocked: [Bool]
}

/// An antenna table as the panel draws it.
public struct SetupAntennaTable: Equatable, Sendable {
    public let grid: SetupAntennaGrid?
    public let gridReason: String?
    public let reason: String?
}

/// A PA reading: its value and how long ago it was measured, or why none.
public struct SetupTelemetryReading: Equatable, Sendable {
    public let value: Double?
    public let ageMilliseconds: Int64?
    public let reason: String?
}

// MARK: Reading the Core's values

public enum SetupSpecializedWire {
    /// `XX:XX:XX:XX:XX:XX`, upper-case hexadecimal: the only form the Core
    /// takes for a radio.
    public static func canonicalMac(_ text: String) -> Bool {
        let parts = text.split(separator: ":", omittingEmptySubsequences: false)
        return text.utf8.count == 17 && parts.count == 6 && parts.allSatisfy { part in
            part.count == 2 && part.allSatisfy { ("0"..."9").contains($0) || ("A"..."F").contains($0) }
        }
    }

    enum NotchFailure: Error, Equatable {
        case unreadable, tooLong, duplicate, outOfRange
    }

    /// The whole list and its revision at once; one bad row refuses all of it.
    static func notches(json: MirrorValue?, revision: MirrorValue?, table: SetupDescription.Table)
        -> Result<SetupNotchList, NotchFailure> {
        guard case .text(let text)? = json, case .int(let revisionNumber)? = revision,
              revisionNumber >= 0, revisionNumber <= Int64(UInt32.max),
              let data = text.data(using: .utf8),
              let array = try? JSONSerialization.jsonObject(with: data) as? [Any] else {
            return .failure(.unreadable)
        }
        guard array.count <= table.maxRows else {
            return .failure(.tooLong)
        }
        let centre = table.columns.first { $0.field == "centreHz" }?.range
        let width = table.columns.first { $0.field == "widthHz" }?.range
        guard let centre, let width else {
            return .failure(.unreadable)
        }
        var ids = Set<Int64>()
        var rows: [SetupNotch] = []
        var failure: NotchFailure?
        for item in array {
            guard let object = item as? [String: Any],
                  Set(object.keys) == ["id", "centreHz", "widthHz", "active"],
                  let id = integer(object["id"]), id >= 1, id <= 2_147_483_647,
                  let centreHz = number(object["centreHz"]), let widthHz = number(object["widthHz"]),
                  let active = boolean(object["active"]) else {
                return .failure(.unreadable)
            }
            if !fits(centreHz, centre) || !fits(widthHz, width) {
                failure = failure ?? .outOfRange
            }
            if !ids.insert(id).inserted {
                failure = failure ?? .duplicate
            }
            rows.append(SetupNotch(id: id, centreHz: centreHz, widthHz: widthHz, active: active))
        }
        if let failure {
            return .failure(failure)
        }
        return .success(SetupNotchList(revision: revisionNumber, rows: rows))
    }

    static func fits(_ value: Double, _ range: SetupDescription.Range) -> Bool {
        value.isFinite && value >= range.minimum && value <= range.maximum
    }

    /// A settings check's answer: exactly `mac` and `issuesJson`, both text,
    /// for the radio asked about, and a bounded list; anything else is no
    /// answer at all, never an empty list.
    static func hygieneReport(_ result: CommandResult, mac: String) -> SetupHygieneReport? {
        guard result.valueEntries.count == 2 else {
            return nil
        }
        var answeredMac: String?
        var json: String?
        for entry in result.valueEntries {
            guard entry.ordinal == 0, case .utf8(let text) = entry.value else {
                return nil
            }
            switch entry.name {
            case "mac" where answeredMac == nil:
                answeredMac = text
            case "issuesJson" where json == nil:
                json = text
            default:
                return nil
            }
        }
        guard let answeredMac, answeredMac == mac, canonicalMac(answeredMac), let json,
              json.utf8.count <= 64 * 1024, let data = json.data(using: .utf8),
              let array = try? JSONSerialization.jsonObject(with: data) as? [Any], array.count <= 32 else {
            return nil
        }
        var issues: [SetupHygieneIssue] = []
        for item in array {
            guard let object = item as? [String: Any],
                  Set(object.keys) == ["severity", "key", "summary", "detail", "fixActionId"],
                  let severity = (object["severity"] as? String).flatMap(SetupHygieneIssue.Severity.init(rawValue:)),
                  let key = object["key"] as? String, key.utf8.count <= 256,
                  let summary = object["summary"] as? String, summary.utf8.count <= 256,
                  let detail = object["detail"] as? String, detail.utf8.count <= 1024,
                  let fix = object["fixActionId"] as? String, fix.utf8.count <= 64 else {
                return nil
            }
            issues.append(SetupHygieneIssue(severity: severity, key: key, summary: summary, detail: detail,
                                            fixActionId: fix))
        }
        return SetupHygieneReport(mac: answeredMac, issues: issues)
    }

    /// One of the Core's per-band antenna lists (14 entries, 15 with 2 m):
    /// exactly `bands` whole numbers, comma-separated, each in `range`.
    static func antennaList(_ value: MirrorValue?, bands: Int, range: ClosedRange<Int>) -> [Int]? {
        guard case .text(let text)? = value else {
            return nil
        }
        let parts = text.split(separator: ",", omittingEmptySubsequences: false)
        guard parts.count == bands else {
            return nil
        }
        var values: [Int] = []
        for part in parts {
            guard !part.isEmpty, part.count <= 2, part.allSatisfy({ ("0"..."9").contains($0) }),
                  let number = Int(part), range.contains(number) else {
                return nil
            }
            values.append(number)
        }
        return values
    }

    private static func boolean(_ value: Any?) -> Bool? {
        guard let number = value as? NSNumber, CFGetTypeID(number) == CFBooleanGetTypeID() else { return nil }
        return number.boolValue
    }

    private static func integer(_ value: Any?) -> Int64? {
        guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
              number.doubleValue.isFinite, number.doubleValue.rounded() == number.doubleValue,
              abs(number.doubleValue) < 9.0e15 else { return nil }
        return number.int64Value
    }

    private static func number(_ value: Any?) -> Double? {
        guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID() else { return nil }
        return number.doubleValue
    }
}
