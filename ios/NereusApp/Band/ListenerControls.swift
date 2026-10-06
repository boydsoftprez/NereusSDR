// NereusSDR for iOS: a listened slice's controls: dimmed ones that answer with the Core's words, and this phone's own volume
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusModels
import SwiftUI

/// A control that would change a slice this phone only listens to
/// (R-IOS-42, JJ's rulings of 2026-09-30): dimmed to the board's 0.4,
/// never hidden, and a tap anywhere on it gives the Core's line for who
/// controls the slice instead of changing it. Nil words leave it as it is.
struct ListenerDim: ViewModifier {
    /// The Core's line for who controls the slice; nil for a slice this phone controls.
    let words: String?
    let showReason: (String) -> Void

    func body(content: Content) -> some View {
        if let words {
            content
                .allowsHitTesting(false)
                .opacity(VfoFlagView.dimmed)
                .overlay {
                    Color.black.opacity(0.001)
                        .contentShape(Rectangle())
                        .onTapGesture { showReason(words) }
                        .accessibilityHidden(true)
                }
                .accessibilityHint(words)
        } else {
            content
        }
    }
}

extension View {
    /// Dims a control on a listened slice; a tap gives the Core's words.
    func listenerDim(_ words: String?, showReason: @escaping (String) -> Void) -> some View {
        modifier(ListenerDim(words: words, showReason: showReason))
    }
}

/// This phone's own volume and mute for a slice it listens to
/// (`slice.setListenLevel`), which change only what this phone hears, and
/// Stop listening: what the sound panel keeps live on a listened slice.
@MainActor
struct ListenerAudio {
    let slices: BandSlicesModel
    let sliceId: Int
    /// The Core's line for who controls the slice, for the dimmed controls.
    let words: String
    /// Stop listening's press.
    let stopListening: () -> Void

    /// The note under AF Gain and MUTE, the board's.
    static let noteText = "AF Gain and MUTE: only what this phone hears."
    /// AF Gain's range on this phone: the level from 0 to 100.
    static let range = StationCatalog.Range(min: 0, max: 100, step: 1)

    var level: BandSlicesModel.ListenLevel { slices.listenLevel(sliceId) }

    func setGain(_ gain: Double) {
        slices.setListenLevel(sliceId, level: gain / 100, muted: level.muted)
    }

    func toggleMute() {
        slices.setListenLevel(sliceId, level: level.level, muted: !level.muted)
    }

    func showReason(_ text: String) {
        slices.showReason(text)
    }
}

/// The sound panel's top on a listened slice: AF Gain and MUTE on this
/// phone only, the note that says so and Stop listening; Pan, BIN, SQL,
/// Squelch and the Core's speakers and headphones would change the slice,
/// so they are dimmed and a tap gives the Core's words.
struct ListenerAudioSection: View {
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var model: ModesTabModel
    @ObservedObject var rx: RxPanelModel
    let listener: ListenerAudio

    var body: some View {
        let level = listener.level
        let dim = listener.words
        ModesChrome.section("Audio") {
            PanelSliderRow(label: "AF Gain", value: (level.level * 100).rounded(), range: ListenerAudio.range,
                           accessibility: "Volume for slice \(letter), on this phone only") {
                listener.setGain($0)
            }
            .accessibilityIdentifier("listenerGain\(letter)")
            PanelSliderRow(label: "Pan", value: model.pan, range: ModesTabModel.panRange,
                           accessibility: "Pan, left to right", format: AudioSection.panText, notConfirmed: model.isUnconfirmed("audioPan")) { _ in }
                .listenerDim(dim, showReason: listener.showReason)
            ModesChrome.grid(columns: 3) {
                PanelButton(label: "MUTE", lit: level.muted, style: .red) {
                    listener.toggleMute()
                }
                .accessibilityLabel("Mute slice \(letter) on this phone only")
                .accessibilityIdentifier("listenerMute\(letter)")
                PanelButton(label: "BIN", lit: model.binaural == true, style: .dsp) {}
                    .accessibilityLabel("Binaural")
                    .listenerDim(dim, showReason: listener.showReason)
                PanelButton(label: "SQL", lit: rx.squelchOn, style: .dsp) {}
                    .accessibilityLabel("Squelch")
                    .listenerDim(dim, showReason: listener.showReason)
            }
            ModesChrome.note(ListenerAudio.noteText)
                .accessibilityIdentifier("listenerNote\(letter)")
            PanelButton(label: SliceListModel.stopListeningTitle, lit: false, style: .blue) {
                listener.stopListening()
            }
            .accessibilityIdentifier("listenerStop\(letter)")
            VStack(alignment: .leading, spacing: 9) {
                PanelSliderRow(label: "Squelch", value: rx.squelch, range: rx.squelchRange,
                               accessibility: "Squelch level", notConfirmed: rx.isUnconfirmed("ssqlThresh")) { _ in }
                ModesChrome.caption("At the Core:")
                ModesChrome.grid(columns: 2) {
                    PanelButton(label: "SPEAKERS", lit: model.outputRoute == ModesTabModel.speakersRoute,
                                style: .blue) {}
                    PanelButton(label: "PHONES", lit: model.outputRoute == ModesTabModel.phonesRoute,
                                style: .blue) {}
                }
            }
            .listenerDim(dim, showReason: listener.showReason)
        }
    }

    private var letter: String { BandSlice.letter(forIndex: listener.sliceId) }
}
