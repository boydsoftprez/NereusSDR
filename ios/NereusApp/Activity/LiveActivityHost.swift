// NereusSDR for iOS: where the app starts its Live Activity: iOS's ActivityKit, or a stand-in in the tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Starts the Live Activity. ``SystemLiveActivityHost`` is iOS's; the
/// tests use a stand-in that records what it was asked.
@MainActor
protocol LiveActivityHost: AnyObject {
    /// The operator allows Live Activities for NereusSDR, and the device has them.
    var activitiesEnabled: Bool { get }

    /// Ends any activity left from an earlier run, then starts one; nil
    /// when iOS refuses (for one, when the app is not in front).
    func start(startedAt: Date, state: StationActivityAttributes.ContentState,
               staleDate: Date?) -> (any LiveActivityHandle)?
}

/// One running Live Activity.
@MainActor
protocol LiveActivityHandle: AnyObject {
    /// Still running: not ended by the app, by iOS after eight hours, or by
    /// the operator's swipe.
    var isActive: Bool { get }

    /// Shows `state`; with `alert`, iOS lights the screen and buzzes (and
    /// opens the island, or shows a banner on an iPhone without one). Past
    /// `staleDate` with no newer change, iOS marks the activity stale.
    func update(_ state: StationActivityAttributes.ContentState, staleDate: Date?, alert: LiveActivityAlert?) async

    /// Ends the activity. With a final state it stays on the lock screen
    /// as long as iOS keeps it (up to four hours), stale after `staleDate`;
    /// `immediately` takes it away.
    func end(_ state: StationActivityAttributes.ContentState?, staleDate: Date?, immediately: Bool) async
}

/// What iOS says as it lights the screen for an update.
struct LiveActivityAlert: Equatable, Sendable {
    let title: String
    let body: String
}
