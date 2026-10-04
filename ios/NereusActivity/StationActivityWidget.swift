// NereusSDR for iOS: the Live Activity's lock-screen card and Dynamic Island
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import ActivityKit
import SwiftUI
import WidgetKit

/// The Live Activity's four presentations (R-IOS-14, R-IOS-21; spec
/// section 5.5 items 1 to 5; pictures 13 and 14): the lock-screen card
/// (which StandBy shows at twice the size), and the Dynamic Island beside
/// the camera, at its smallest, and opened. The views are
/// ``ActivityCard`` and ``ActivityIsland``, which the app builds too so
/// its tests can draw them. A keyed card iOS marks stale (the app didn't
/// refresh it in time) draws ``StationActivityAttributes/ContentState/shown(stale:)``.
struct StationActivityWidget: Widget {
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: StationActivityAttributes.self) { context in
            let state = context.state.shown(stale: context.isStale)
            ActivityCard(state: state)
                .activityBackgroundTint(ActivityCard.background(state))
                .activitySystemActionForegroundColor(ActivityColours.text)
        } dynamicIsland: { context in
            let state = context.state.shown(stale: context.isStale)
            return DynamicIsland {
                DynamicIslandExpandedRegion(.leading) {
                    ActivityIsland(part: .expandedLeading, state: state)
                        .padding(.leading, 6)
                }
                DynamicIslandExpandedRegion(.trailing) {
                    ActivityIsland(part: .expandedTrailing, state: state)
                        .padding(.trailing, 6)
                }
                DynamicIslandExpandedRegion(.bottom) {
                    ActivityIsland(part: .expandedBottom, state: state)
                        .padding(.horizontal, 6)
                }
            } compactLeading: {
                ActivityIsland(part: .compactLeading, state: state)
            } compactTrailing: {
                ActivityIsland(part: .compactTrailing, state: state)
            } minimal: {
                ActivityIsland(part: .minimal, state: state)
            }
            .keylineTint(state.keyed ? ActivityColours.txEdge : ActivityColours.frequency)
        }
    }
}
