// NereusSDR for iOS: this phone's own log lines, read from the system log for the support bundle
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import OSLog

/// This phone's own log for the support bundle: the app's lines in the
/// system log since it started, oldest first, each with its time and
/// category. The app writes its log with private values hidden, so no
/// address, key or code comes out here that the log did not already show.
struct PhoneLog: Sendable {
    /// The lines, or an error when the system log could not be read.
    let read: @Sendable () throws -> [String]

    /// The subsystem the app logs under.
    static let subsystem = "NereusSDR"
    /// The most lines a bundle carries: the newest are kept.
    static let mostLines = 20_000

    /// The app's own lines from the system log.
    static let system = PhoneLog {
        let store = try OSLogStore(scope: .currentProcessIdentifier)
        let entries = try store.getEntries(matching: NSPredicate(format: "subsystem == %@", subsystem))
        let format = Date.ISO8601FormatStyle(includingFractionalSeconds: true)
        var lines: [String] = []
        for case let entry as OSLogEntryLog in entries {
            lines.append("[\(entry.date.formatted(format))] \(entry.category): \(entry.composedMessage)")
        }
        return Array(lines.suffix(mostLines))
    }
}
