// NereusSDR for iOS: the mic level meter: live from this phone's microphone before transmitting, the Core's reading while keyed
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import UIKit

/// The mic level meter, in the TX panel and in the Modes tab's Transmit
/// section. While keyed it shows the Core's reading, as it always has.
/// Otherwise, while it is on screen and this phone may transmit, it shows
/// this phone's microphone with the Mic Gain added (``LiveMicLevel``), with
/// a line under it saying so; where the phone may not use the microphone,
/// the line says why and Open Settings goes to the app's settings. While
/// the Core's mic is muted it is greyed with that reason.
struct MicLevelGauge: View {
    @ObservedObject var transmit: TransmitModel
    @ObservedObject var level: LiveMicLevel
    /// The line under the meter while it is live.
    let liveText: String

    @State private var viewer = UUID()
    @State private var appeared = false
    /// Inside a scrolling page, whether the meter is scrolled into view.
    @State private var inView = true

    var body: some View {
        let coreReading = Self.showsCoreReading(transmit)
        let muted = Self.mutedReason(transmit) != nil
        trackingScroll(VStack(alignment: .leading, spacing: 4) {
            LinearGauge(scale: .micLevel,
                        value: muted ? LinearGauge.Scale.micLevel.min
                            : Self.value(coreReading: coreReading, coreDb: transmit.micLevelDb, live: level))
                .opacity(muted ? 0.45 : 1)
                .accessibilityHint(Self.mutedReason(transmit) ?? "")
                .accessibilityIdentifier("micLevelGauge")
            if muted {
                // The Core's mic is muted (another device, or the Core's own
                // window): greyed with the reason, as the desktop greys its
                // mic slider; this phone's microphone is not opened for it.
                note(TransmitModel.micMutedText)
                    .accessibilityIdentifier("micLevelMuted")
            } else if !coreReading {
                if let reason = level.reason {
                    note(reason)
                        .accessibilityIdentifier("micLevelReason")
                    if reason == MicCapture.notAllowedText {
                        PanelButton(label: "Open Settings", lit: false, style: .blue) {
                            if let settings = URL(string: UIApplication.openSettingsURLString) {
                                UIApplication.shared.open(settings)
                            }
                        }
                        .accessibilityHint("Opens NereusSDR's settings, where the microphone is allowed")
                        .accessibilityIdentifier("micLevelOpenSettings")
                    }
                } else if level.listening {
                    note(liveText)
                        .accessibilityIdentifier("micLevelLive")
                }
            }
        })
        .onAppear {
            appeared = true
            sync()
        }
        .onDisappear {
            appeared = false
            sync()
        }
        .onChange(of: transmit.micMuted) {
            sync()
        }
    }

    /// Why the meter is greyed: the Core's mic is muted; nil otherwise.
    static func mutedReason(_ transmit: TransmitModel) -> String? {
        transmit.micMuted ? TransmitModel.micMutedText : nil
    }

    /// While keyed, by this phone or at the Core, the meter shows the
    /// Core's reading of the microphone that is on the air.
    static func showsCoreReading(_ transmit: TransmitModel) -> Bool {
        transmit.transmittingHere || transmit.coreOnAir
    }

    /// What the meter shows: the Core's reading while keyed or while this
    /// phone is not listening, otherwise the live level.
    static func value(coreReading: Bool, coreDb: Double, live: LiveMicLevel) -> Double {
        coreReading || !live.listening ? coreDb : live.levelDb
    }

    private func sync() {
        if appeared && inView && !transmit.micMuted {
            level.show(viewer)
        } else {
            level.hide(viewer)
        }
    }

    /// Inside a scrolling page (the Modes tab), the meter counts as on
    /// screen only while it is scrolled into view.
    @ViewBuilder
    private func trackingScroll(_ content: some View) -> some View {
        if #available(iOS 18.0, *) {
            content.onScrollVisibilityChange(threshold: 0.05) { visible in
                inView = visible
                sync()
            }
        } else {
            content
        }
    }

    private func note(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.textFaint)
            .fixedSize(horizontal: false, vertical: true)
    }
}
