// NereusSDR for iOS: Setup's Logs page's own view of the Core's log: what it shows, and Clear on this phone only
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror

/// The Logs page's view of the Core's log (D95, R-IOS-36). The log itself,
/// its asking and its categories are the one ``CoreLogModel`` the app keeps;
/// this holds only what the page adds: Clear, which empties this page's view
/// on this phone and sends nothing to the Core. Lines the Core sends after
/// a Clear show as they arrive. Reload reads the Core's newest lines again,
/// as the desktop's Refresh does after its Clear, and the view starts over
/// when the log is lost with the Core.
@MainActor
final class CoreLogsModel: ObservableObject {
    static let clearedText = "New lines from the Core show here."

    let log: CoreLogModel
    /// The newest line when Clear was pressed; the view shows the lines after it.
    @Published private(set) var clearedThrough: String?
    private var watches: Set<AnyCancellable> = []

    init(log: CoreLogModel) {
        self.log = log
        log.objectWillChange.sink { [weak self] _ in self?.objectWillChange.send() }.store(in: &watches)
        log.$lines.sink { [weak self] lines in
            // The log went with the Core, or the page closed: nothing is hidden when it comes back.
            if lines.isEmpty, self?.clearedThrough != nil {
                self?.clearedThrough = nil
            }
        }.store(in: &watches)
    }

    /// The page opened or closed: the Core's log is followed only while it shows.
    func setOpen(_ shown: Bool) {
        log.setOpen(shown)
    }

    /// The lines the view shows, oldest first.
    var lines: [CoreLogLine] {
        Self.shown(log.lines, after: clearedThrough)
    }

    /// The view's text: the reason the log cannot be read, the lines, or
    /// what an empty view is waiting for.
    var text: String {
        if let reason = log.reason {
            return reason
        }
        if clearedThrough != nil {
            let shown = lines
            return shown.isEmpty ? Self.clearedText : shown.map(\.line).joined(separator: "\n")
        }
        return log.text
    }

    /// Whether Clear and Reload can be pressed.
    var canClear: Bool { log.reason == nil && !lines.isEmpty }
    var canReload: Bool { log.reason == nil }

    /// Empties this page's view on this phone. Nothing goes to the Core.
    func clear() {
        guard let newest = log.lines.last?.id else {
            return
        }
        clearedThrough = newest
    }

    /// Reads the Core's log again from its newest lines, showing them all.
    func reload() {
        clearedThrough = nil
        log.reload()
    }

    /// The lines after the one with id `cleared`. When that line is no
    /// longer held (it rolled off the stream's bound, or the Core started
    /// its log again), every line held is newer, so all show.
    static func shown(_ lines: [CoreLogLine], after cleared: String?) -> [CoreLogLine] {
        guard let cleared, let index = lines.lastIndex(where: { $0.id == cleared }) else {
            return lines
        }
        return Array(lines[lines.index(after: index)...])
    }
}
