// NereusSDR for iOS: Setup, PA Values: the radio's PA current or DC voltage as the Core last measured it, and how long ago
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// One of the two PA readings the Core describes: the radio's PA current
/// or its DC voltage from the Core's telemetry, to the described places
/// with its unit, and when it was measured. A reading older than three
/// seconds, one the radio has not reported, or one from an earlier
/// connection shows as unavailable with its reason, never as zero; a
/// measured zero shows as zero. It only reads.
struct PaTelemetryPanel: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher
    /// The clock the Core's samples were stamped with on arrival.
    let now: () -> Int64

    var body: some View {
        TimelineView(.periodic(from: .now, by: 1)) { _ in
            let reading = dispatcher.telemetryReading(control, in: category, nowMilliseconds: now())
            VStack(alignment: .leading, spacing: 2) {
                LabeledContent(control.label) {
                    Text(Self.text(reading, control: control))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
                Text(reading.reason ?? Self.age(reading.ageMilliseconds ?? 0))
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .accessibilityIdentifier("\(control.id).age")
            }
            .accessibilityElement(children: .combine)
            .accessibilityIdentifier(control.id)
        }
    }

    /// "1.25 A", or Unavailable.
    static func text(_ reading: SetupTelemetryReading, control: SetupDescription.Control) -> String {
        guard let value = reading.value else {
            return DescribedControl.unavailableText
        }
        return DescribedControl.reading(.decimal(value), control: control)
    }

    /// "Measured just now", "Measured 2 s ago".
    static func age(_ milliseconds: Int64) -> String {
        let seconds = milliseconds / 1_000
        return seconds < 1 ? "Measured just now" : "Measured \(seconds) s ago"
    }
}
