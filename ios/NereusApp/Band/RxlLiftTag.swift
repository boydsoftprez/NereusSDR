// NereusSDR for iOS: the RXL tag on the flag: how far RXL turns the station up or down
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-15: New, the phone's copy of the desktop flag's RXL tag. J.J. Boyd
// (KG4VCF), AI-assisted via Claude Code.
// 2026-11-15: the readings live apart from the flag's entry, whole dB at
// most four times a second, and only the tag watches them (JJ: mind the
// phone's CPU). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Combine
import Foundation
import SwiftUI

/// The live RXL readings of the slices whose flag shows the RXL line, in
/// whole dB, by slice. Kept apart from ``BandSlicesModel/Entry`` so a new
/// reading redraws only the tag that watches it, never the flag, the band
/// or the other flags. It publishes only when a whole-dB value changes,
/// and at most every ``interval`` seconds: a change inside the interval
/// waits for its end, and only the latest is shown.
@MainActor
final class RxlLiftReadings: ObservableObject {
    /// Four readings a second at most.
    static let interval: TimeInterval = 0.25

    @Published private(set) var lifts: [Int: Int] = [:]

    private let now: () -> TimeInterval
    private var lastPublish: TimeInterval = -.infinity
    private var pending: [Int: Int]?
    private var trailing: Task<Void, Never>?

    init(now: @escaping () -> TimeInterval = { ProcessInfo.processInfo.systemUptime }) {
        self.now = now
    }

    /// The readings as the mirror has them now, in dB; whole dB is what counts.
    func offer(_ readings: [Int: Double]) {
        let next = readings.mapValues(RxlLiftTag.whole)
        // A slice whose line went away (RXL off, a mode it leaves alone)
        // leaves at once: nothing watches it any more.
        let gone = Set(lifts.keys).subtracting(next.keys)
        if next == (pending ?? lifts) {
            return
        }
        let at = now()
        if !gone.isEmpty || at - lastPublish >= Self.interval {
            publish(next, at: at)
            return
        }
        pending = next
        guard trailing == nil else {
            return
        }
        let wait = Self.interval - (at - lastPublish)
        trailing = Task { [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(max(wait, 0) * 1_000_000_000))
            guard let self, !Task.isCancelled else {
                return
            }
            self.trailing = nil
            if let pending = self.pending {
                self.publish(pending, at: self.now())
            }
        }
    }

    private func publish(_ next: [Int: Int], at: TimeInterval) {
        pending = nil
        trailing?.cancel()
        trailing = nil
        lastPublish = at
        if next != lifts {
            lifts = next
        }
    }
}

/// The RXL line's tag for one slice, watching ``RxlLiftReadings``: the only
/// view a new reading redraws.
struct RxlLiftLine: View {
    let sliceId: Int
    @ObservedObject var readings: RxlLiftReadings
    var points: CGFloat = 10

    var body: some View {
        RxlLiftTag(liftDb: Double(readings.lifts[sliceId] ?? 0), points: points)
    }
}

/// "RXL +12 dB" in a thin box, as the desktop flag shows it at the end of
/// its "You control" line (JJ, 2026-11-15): whole dB with a sign, a real
/// minus sign for a cut; green while RXL turns the station up, amber while
/// it turns it down, dim at 0. Shown while RXL is on in a mode it levels
/// (``BandSlicesModel/Entry/rxlOn``), reading ``RxlLiftReadings``.
struct RxlLiftTag: View {
    let liftDb: Double
    var points: CGFloat = 10

    static let lift = WorkspacePalette.liveGreen
    static let cut = WorkspacePalette.amberText
    static let level = ChromeColours.textDim

    /// The whole dB the tag shows.
    static func whole(_ dB: Double) -> Int {
        dB.isFinite ? Int(dB.rounded()) : 0
    }

    /// "RXL +12 dB", "RXL −5 dB" (U+2212), "RXL 0 dB".
    static func text(_ dB: Double) -> String {
        let value = whole(dB)
        let sign = value > 0 ? "+" : value < 0 ? "\u{2212}" : ""
        return "RXL \(sign)\(abs(value)) dB"
    }

    /// What VoiceOver says.
    static func spoken(_ dB: Double) -> String {
        let value = whole(dB)
        return value > 0 ? "RXL turning this station up \(value) dB"
            : value < 0 ? "RXL turning this station down \(-value) dB"
            : "RXL leaving this station's level as it is"
    }

    static func ink(_ dB: Double) -> Color {
        let value = whole(dB)
        return value > 0 ? lift : value < 0 ? cut : level
    }

    var body: some View {
        let ink = Self.ink(liftDb)
        Text(Self.text(liftDb))
            .font(.system(size: points, weight: .bold).monospacedDigit())
            .foregroundStyle(ink)
            .lineLimit(1)
            .fixedSize()
            .padding(.horizontal, 4)
            .padding(.vertical, 1)
            .overlay(RoundedRectangle(cornerRadius: 3) // kept-radius: flag
                .strokeBorder(ink.opacity(Self.whole(liftDb) == 0 ? 0.4 : 0.6), lineWidth: 1))
            .accessibilityElement()
            .accessibilityLabel(Self.spoken(liftDb))
            .accessibilityIdentifier("flagRxl")
    }
}
