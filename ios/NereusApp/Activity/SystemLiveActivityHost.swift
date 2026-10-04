// NereusSDR for iOS: the Live Activity through iOS's ActivityKit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import ActivityKit
import Foundation
import os

/// iOS's Live Activities: one `Activity<StationActivityAttributes>`, local
/// updates only (no push). An `Activity` can't cross from the main actor,
/// so each change finds it again by its id where it runs.
@MainActor
final class SystemLiveActivityHost: LiveActivityHost {
    private static let logger = Logger(subsystem: "NereusSDR", category: "activity")

    var activitiesEnabled: Bool {
        ActivityAuthorizationInfo().areActivitiesEnabled
    }

    func start(startedAt: Date, state: StationActivityAttributes.ContentState,
               staleDate: Date?) -> (any LiveActivityHandle)? {
        // One card at a time: anything from an earlier run goes.
        let leftovers = Activity<StationActivityAttributes>.activities.map(\.id)
        Task {
            for id in leftovers {
                await Self.end(id: id, state: nil, staleDate: nil, immediately: true)
            }
        }
        do {
            let activity = try Activity.request(attributes: StationActivityAttributes(startedAt: startedAt),
                                                content: ActivityContent(state: state, staleDate: staleDate),
                                                pushType: nil)
            return Handle(activity: activity)
        } catch {
            Self.logger.info("iOS did not start the Live Activity: \(String(describing: error), privacy: .public)")
            return nil
        }
    }

    nonisolated private static func find(_ id: String) -> Activity<StationActivityAttributes>? {
        Activity<StationActivityAttributes>.activities.first { $0.id == id }
    }

    nonisolated private static func update(id: String, state: StationActivityAttributes.ContentState,
                                           staleDate: Date?, alert: LiveActivityAlert?) async {
        guard let activity = find(id) else {
            return
        }
        let content = ActivityContent(state: state, staleDate: staleDate)
        if let alert {
            await activity.update(content, alertConfiguration: AlertConfiguration(
                title: LocalizedStringResource(stringLiteral: alert.title),
                body: LocalizedStringResource(stringLiteral: alert.body), sound: .default))
        } else {
            await activity.update(content)
        }
    }

    nonisolated private static func end(id: String, state: StationActivityAttributes.ContentState?,
                                        staleDate: Date?, immediately: Bool) async {
        guard let activity = find(id) else {
            return
        }
        await activity.end(state.map { ActivityContent(state: $0, staleDate: staleDate) },
                           dismissalPolicy: immediately ? .immediate : .default)
    }

    private final class Handle: LiveActivityHandle {
        private let activity: Activity<StationActivityAttributes>
        private let id: String
        private var ended = false

        init(activity: Activity<StationActivityAttributes>) {
            self.activity = activity
            id = activity.id
        }

        var isActive: Bool {
            !ended && activity.activityState == .active
        }

        func update(_ state: StationActivityAttributes.ContentState, staleDate: Date?,
                    alert: LiveActivityAlert?) async {
            await SystemLiveActivityHost.update(id: id, state: state, staleDate: staleDate, alert: alert)
        }

        func end(_ state: StationActivityAttributes.ContentState?, staleDate: Date?, immediately: Bool) async {
            ended = true
            await SystemLiveActivityHost.end(id: id, state: state, staleDate: staleDate, immediately: immediately)
        }
    }
}
