// NereusSDR for iOS: a Live Activity host for the tests, which records what the controller asks of it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusSDR

/// Stands in for iOS's ActivityKit: every start, change and end is kept.
@MainActor
final class FakeLiveActivityHost: LiveActivityHost {
    final class Handle: LiveActivityHandle {
        let startedAt: Date
        let first: StationActivityAttributes.ContentState
        let firstStaleDate: Date?
        private(set) var updates: [(state: StationActivityAttributes.ContentState, staleDate: Date?,
                                    alert: LiveActivityAlert?)] = []
        private(set) var ends: [(state: StationActivityAttributes.ContentState?, staleDate: Date?,
                                 immediately: Bool)] = []
        /// iOS ended it (eight hours, or the operator's swipe).
        var endedBySystem = false

        init(startedAt: Date, first: StationActivityAttributes.ContentState, staleDate: Date?) {
            self.startedAt = startedAt
            self.first = first
            firstStaleDate = staleDate
        }

        var isActive: Bool { ends.isEmpty && !endedBySystem }

        /// The state it shows now.
        var shown: StationActivityAttributes.ContentState {
            ends.last?.state ?? updates.last?.state ?? first
        }

        func update(_ state: StationActivityAttributes.ContentState, staleDate: Date?,
                    alert: LiveActivityAlert?) async {
            updates.append((state, staleDate, alert))
        }

        func end(_ state: StationActivityAttributes.ContentState?, staleDate: Date?, immediately: Bool) async {
            ends.append((state, staleDate, immediately))
        }
    }

    var activitiesEnabled = true
    private(set) var handles: [Handle] = []

    var current: Handle? { handles.last }

    func start(startedAt: Date, state: StationActivityAttributes.ContentState,
               staleDate: Date?) -> (any LiveActivityHandle)? {
        let handle = Handle(startedAt: startedAt, first: state, staleDate: staleDate)
        handles.append(handle)
        return handle
    }
}
