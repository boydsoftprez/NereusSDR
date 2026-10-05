// NereusSDR for iOS: one line of the Core's own log, read from the Core's coreLog record stream
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// One line of the Core's log (link document section 7.7, stream
/// `coreLog`, at `supportBundleVersion` 1): the line as the Core's log file
/// has it (`[HH:mm:ss.zzz] INF: text`), with addresses shortened and keys,
/// tokens and pairing codes removed by the Core before it is sent. The
/// record's id is the line's number in the Core's log, rising.
///
/// Reading is tolerant: a missing or other-kind `line` reads as empty.
public struct CoreLogLine: Sendable, Equatable, Identifiable {
    public var id: String
    public var line: String

    /// The stream's name, and the most records it holds (link 7.7).
    public static let streamName = "coreLog"
    public static let capacity = 200
    /// The capability under which the Core shares its log and its logging categories.
    public static let capabilityName = "supportBundleVersion"
    /// Turns on exactly the named logging categories at the Core (link 9.1).
    public static let setCategoriesVerb = "support.setLogCategories"
    /// The `radio` property naming the categories that are on, joined by commas.
    public static let categoriesProperty = "logCategories"

    public init(id: String, line: String) {
        self.id = id
        self.line = line
    }

    /// Reads one `coreLog` record.
    public init(record: LinkMessage.RecordBatch.Record) {
        var line = ""
        if case .string(let value)? = record.fields["line"] {
            line = value
        }
        self.init(id: record.id, line: line)
    }

    /// The ids in a `logCategories` value: comma-separated, blanks dropped.
    public static func categories(_ text: String) -> [String] {
        text.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }.filter { !$0.isEmpty }
    }
}
