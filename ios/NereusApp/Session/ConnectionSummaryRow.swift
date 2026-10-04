// NereusSDR for iOS: one passive connection and app traffic line below the band's toolbar
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// A compact status line. The control route comes from the selected route;
/// the media path can differ, so this line makes no media-path claim.
struct ConnectionSummaryRow: View {
    @ObservedObject var app: AppModel
    @ObservedObject var session: SessionController
    @ObservedObject var meter: DataUseMeter

    init(app: AppModel) {
        self.app = app
        session = app.longSession
        meter = app.longSession.meter
    }

    static func controlText(_ rank: PathRacer.Rank?) -> String {
        guard let rank else { return "Control unavailable" }
        return rank >= .turn ? "Control relay" : "Control direct"
    }

    private var rateText: (incoming: String, outgoing: String) {
        guard let rate = meter.currentRate else { return ("n/a", "n/a") }
        return (DataUseMeter.Rate.text(rate.incoming), DataUseMeter.Rate.text(rate.outgoing))
    }

    var body: some View {
        if app.connection == .connected {
            HStack(spacing: 6) {
                Text("\(session.networkLabel) \u{00B7} \(Self.controlText(app.currentPathRank))")
                    .foregroundStyle(ChromeColours.text)
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
                Spacer(minLength: 4)
                HStack(spacing: 5) {
                    Text("↓\(rateText.incoming)")
                        .accessibilityLabel("Incoming app payload \(rateText.incoming)")
                    Text("↑\(rateText.outgoing)")
                        .accessibilityLabel("Outgoing app payload \(rateText.outgoing)")
                }
                .foregroundStyle(ChromeColours.textDim)
                .lineLimit(1)
                .minimumScaleFactor(0.8)
            }
            .font(.system(size: 11, weight: .medium, design: .monospaced))
            .padding(.horizontal, 9)
            .frame(height: 24)
            .frame(maxWidth: .infinity)
            .background(ChromeColours.bar)
            .overlay(alignment: .bottom) { ChromeColours.panelEdge.frame(height: 1) }
                .allowsHitTesting(false)
                .accessibilityElement(children: .combine)
                .accessibilityIdentifier("connectionSummary")
        }
    }
}
