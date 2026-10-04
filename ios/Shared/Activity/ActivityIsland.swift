// NereusSDR for iOS: the Dynamic Island's pieces: beside the camera, the smallest, and opened
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// NereusSDR in the Dynamic Island while another app is in front (spec
/// section 5.5 items 3 to 5; picture 14). Beside the camera: the active slice's
/// letter and its frequency, or, keyed, TX and its clock in red. Opened
/// (a press and hold, iOS's own gesture, or by itself when the link is
/// lost): the card without its head, with UNKEY working in one tap.
struct ActivityIsland: View {
    enum Part: Equatable {
        case compactLeading
        case compactTrailing
        case minimal
        /// The opened island's top row, left of the camera.
        case expandedLeading
        /// The opened island's top row, right of the camera.
        case expandedTrailing
        /// The opened island under the camera.
        case expandedBottom
    }

    let part: Part
    let state: StationActivityAttributes.ContentState

    var body: some View {
        switch part {
        case .compactLeading:
            compactLeading
        case .compactTrailing:
            compactTrailing
        case .minimal:
            minimal
        case .expandedLeading:
            expandedLeading
        case .expandedTrailing:
            expandedTrailing
        case .expandedBottom:
            expandedBottom
        }
    }

    private var lost: Bool { state.link == .lost }

    @ViewBuilder
    private var compactLeading: some View {
        if state.keyed {
            ActivityBadge(kind: .tx)
        } else if let slice = state.slice {
            ActivityBadge(kind: .slice(letter: slice.letter, colour: slice.colour))
        }
    }

    @ViewBuilder
    private var compactTrailing: some View {
        if state.keyed, let since = state.keyedSince {
            Text(since, style: .timer)
                .font(.system(size: 13, weight: .bold, design: .monospaced))
                .monospacedDigit()
                .foregroundStyle(ActivityColours.txText)
                .frame(maxWidth: 56, alignment: .trailing)
        } else if state.staleKeyed {
            Text(ActivityWords.noNewsShort)
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(ActivityColours.timeOut)
        } else if lost {
            Text(ActivityWords.lost)
                .font(.system(size: 13, weight: .bold, design: .monospaced))
                .foregroundStyle(ActivityColours.linkLost)
        } else if let slice = state.slice {
            Text(ActivityWords.compactFrequency(slice.frequencyHz))
                .font(.system(size: 13, weight: .bold, design: .monospaced))
                .foregroundStyle(ActivityColours.frequency)
        }
    }

    @ViewBuilder
    private var minimal: some View {
        if state.keyed {
            ActivityBadge(kind: .tx)
        } else if lost {
            Circle().fill(ActivityColours.linkLost).frame(width: 10, height: 10)
        } else if let slice = state.slice {
            ActivityBadge(kind: .slice(letter: slice.letter, colour: slice.colour))
        }
    }

    @ViewBuilder
    private var expandedLeading: some View {
        if lost {
            ActivityAppIcon()
        } else {
            HStack(spacing: 6) {
                if let slice = state.slice {
                    ActivityBadge(kind: .slice(letter: slice.letter, colour: slice.colour))
                }
                if state.keyed {
                    ActivityBadge(kind: .tx)
                }
            }
        }
    }

    @ViewBuilder
    private var expandedTrailing: some View {
        if state.keyed && !lost {
            ActivityTxClock(since: state.keyedSince)
        } else {
            ActivityLinkChip(link: state.link, roundTripMs: state.roundTripMs)
        }
    }

    @ViewBuilder
    private var expandedBottom: some View {
        if lost {
            ActivityLostBlock(state: state)
        } else {
            VStack(alignment: .leading, spacing: 7) {
                ActivityFrequencyRow(slice: state.slice, keyed: state.keyed, withBadges: false)
                ActivityFoot(state: state)
            }
        }
    }
}
