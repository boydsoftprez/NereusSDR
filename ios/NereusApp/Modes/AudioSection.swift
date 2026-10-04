// NereusSDR for iOS: the Modes tab's audio: AF gain, pan, MUTE, BIN, SQL and the squelch level
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The Audio section: the slice's AF gain and squelch (the RX panel's),
/// its pan from left to right, MUTE, BIN (binaural) and SQL.
struct AudioSection: View {
    @ObservedObject var model: ModesTabModel
    @ObservedObject var rx: RxPanelModel

    var body: some View {
        ModesChrome.section("Audio") {
            PanelSliderRow(label: "AF Gain", value: rx.afGain, range: rx.afRange, accessibility: "AF gain", notConfirmed: rx.isUnconfirmed("afGain")) {
                rx.setAfGain($0)
            }
            PanelSliderRow(label: "Pan", value: model.pan, range: ModesTabModel.panRange,
                           accessibility: "Pan, left to right", format: Self.panText, notConfirmed: model.isUnconfirmed("audioPan")) {
                model.setPan($0)
            }
            ModesChrome.grid(columns: 3) {
                PanelButton(label: "MUTE", lit: model.muted == true, style: .red, disabled: model.muted == nil) {
                    model.toggleMute()
                }
                PanelButton(label: "BIN", lit: model.binaural == true, style: .dsp, disabled: model.binaural == nil) {
                    model.toggleBinaural()
                }
                .accessibilityLabel("Binaural")
                PanelButton(label: "SQL", lit: rx.squelchOn, style: .dsp, disabled: rx.squelch == nil) {
                    rx.toggleSquelch()
                }
                .accessibilityLabel("Squelch")
            }
            PanelSliderRow(label: "Squelch", value: rx.squelch, range: rx.squelchRange, accessibility: "Squelch level", notConfirmed: rx.isUnconfirmed("ssqlThresh")) {
                rx.setSquelch($0)
            }
            // Where the Core plays this slice: its speakers or its
            // headphones, the desktop flag's SPEAKERS and PHONES (M5).
            ModesChrome.caption("At the Core:")
            ModesChrome.grid(columns: 2) {
                PanelButton(label: "SPEAKERS", lit: model.outputRoute == ModesTabModel.speakersRoute, style: .blue,
                            disabled: model.outputRoute == nil) {
                    model.selectOutputRoute(ModesTabModel.speakersRoute)
                }
                .accessibilityHint("Plays this slice on the Core's speakers")
                .accessibilityIdentifier("modesSpeakers")
                PanelButton(label: "PHONES", lit: model.outputRoute == ModesTabModel.phonesRoute, style: .blue,
                            disabled: model.outputRoute == nil) {
                    model.selectOutputRoute(ModesTabModel.phonesRoute)
                }
                .accessibilityHint("Plays this slice on the Core's headphones")
                .accessibilityIdentifier("modesPhones")
            }
            if !model.olderCore.isDisjoint(with: [ModesTabModel.Property.audioPan, ModesTabModel.Property.muted,
                                                  ModesTabModel.Property.binauralEnabled,
                                                  ModesTabModel.Property.outputRoute]) {
                ModesChrome.note(CatalogFeed.needsNewerCoreText)
            }
        }
    }

    /// The pan as the board shows it: C at the centre, else the side and how far.
    static func panText(_ pan: Double) -> String {
        let percent = Int((abs(pan) * 100).rounded())
        if percent == 0 {
            return "C"
        }
        return (pan < 0 ? "L" : "R") + "\(percent)"
    }
}
