// NereusSDR for iOS: the link chip and what it shows: the dot, the round-trip time or the lost link
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// What the link chip shows for a connection state.
struct LinkState: Equatable {
    let connection: ConnectionState
    let roundTripMs: Int?
    /// The phone has no network at all: the chip says Offline, in grey.
    var offline = false
    /// The session came through the remote access service's relay.
    var relayed = false

    /// How the link reaches the Core, in the chip's word: Direct, or Relay
    /// through the remote access service (picture 07).
    var pathWord: String { relayed ? "Relay" : "Direct" }

    var isUp: Bool { connection == .connected && !offline }

    var isLost: Bool {
        if offline {
            return false
        }
        if case .waitingToRetry = connection {
            return true
        }
        return false
    }

    var dot: Color {
        isUp ? ChromeColours.linkUp : isLost ? ChromeColours.linkLost : ChromeColours.linkOffDot
    }

    var pulses: Bool { isUp }

    /// The chip's words: the round-trip time while up, "Lost" while it
    /// retries, "Offline" with no network, nothing otherwise.
    var text: String? {
        if offline {
            return "Offline"
        }
        if isUp {
            return roundTripMs.map { "\($0) ms" }
        }
        return isLost ? "Lost" : nil
    }

    var textColour: Color {
        offline ? ChromeColours.textDim : isUp ? ChromeColours.linkUp : ChromeColours.linkLost
    }

    func spoken(core: String?) -> String {
        let name = core.map { "Core \($0)" } ?? "Core"
        if isUp {
            let path = relayed ? "relay link" : "direct link"
            return roundTripMs.map { "\(name), \(path), \($0) milliseconds" } ?? "\(name), \(path)"
        }
        if offline {
            return "\(name), this phone is offline"
        }
        return isLost ? "\(name), link lost" : "\(name), not connected"
    }
}

/// The link chip (the board's `.conn`): the dot, "Direct" beside it when
/// asked for and the link is up, then the round-trip time, Lost or
/// Offline. The toolbar and the Radio tab's bar both show it.
struct LinkChip: View {
    let link: LinkState
    var showsPath = false
    /// The Core's name or address, for VoiceOver.
    let core: String?

    static let height: CGFloat = 26

    var body: some View {
        HStack(spacing: 6) {
            LinkDot(colour: link.dot, pulses: link.pulses)
            if showsPath && link.isUp {
                Text(link.pathWord).foregroundStyle(ChromeColours.linkUpText)
            }
            if let text = link.text {
                Text(text).foregroundStyle(link.textColour)
            }
        }
        .font(.system(size: 10, weight: .semibold, design: .monospaced))
        .lineLimit(1)
        .padding(.leading, 7)
        .padding(.trailing, 8)
        .frame(height: Self.height)
        .workspaceInset()
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(link.spoken(core: core))
    }
}

/// The link's dot, gently pulsing while the link is up (the board's
/// `conn-pulse`), still when motion is reduced.
///
/// The pulse is worked out from the time, 30 times a second. A SwiftUI
/// animation repeating forever would have the whole app drawn at the
/// screen's full rate, up to 120 times a second, for as long as the link
/// is up, which costs the battery far more than the dot is worth.
private struct LinkDot: View {
    let colour: Color
    let pulses: Bool
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    /// One way of the pulse, in seconds; it then comes back.
    static let pulseSeconds = 1.2
    /// The pulse's frames a second.
    static let pulseFps = 30.0

    var body: some View {
        Group {
            if pulses && !reduceMotion {
                TimelineView(.periodic(from: .now, by: 1 / Self.pulseFps)) { context in
                    dot(dim: Self.dim(at: context.date.timeIntervalSinceReferenceDate))
                }
            } else {
                dot(dim: 0)
            }
        }
        .frame(width: 10, height: 10)
    }

    private func dot(dim: Double) -> some View {
        Circle()
            .fill(colour)
            .overlay {
                Circle().fill(ChromeColours.linkUpPulse).opacity(dim)
            }
    }

    /// How far toward the pulse's colour the dot is at `seconds`: eased in
    /// and out over each way of the pulse.
    static func dim(at seconds: Double) -> Double {
        let phase = seconds.truncatingRemainder(dividingBy: 2 * pulseSeconds) / pulseSeconds
        let way = phase <= 1 ? phase : 2 - phase
        return way * way * (3 - 2 * way)
    }
}
