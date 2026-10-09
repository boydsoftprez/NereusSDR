// NereusSDR for iOS: the Sound panel the band's speaker button opens: Mute this phone, the radio speaker, then where the band plays
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Everything about the band's sound in one small panel, opened by one tap
/// on the speaker button without leaving the band (D77, spec section 5.4
/// item 10): a Mute this phone switch, then the speaker at the radio
/// (R-SPK-20, D12: its level and Mute radio speaker, the Core's, in the
/// desktop's RADIO amber), then the speaker at the Core (R-AUD-29: its
/// level and Mute Core speaker in the desktop card's cyan, left out while
/// the Core plays on no card), then where the band plays: the speaker, the
/// earpiece on an iPhone, and AirPods or headphones only while they are
/// connected, by their own name, with a tick on the current one.
///
/// Where the band plays is the one choice Setup, Audio, On this phone also
/// makes: both go through ``AudioSessionController/select(_:)`` and both
/// tick ``AudioSessionController/markedRoute``, so each shows the other's
/// choice.
struct SoundPanel: View {
    @ObservedObject var app: AppModel

    /// One place the band can play.
    struct Row: Identifiable, Equatable {
        let route: AudioRoute
        let title: String
        let isCurrent: Bool

        var id: AudioRoute {
            route
        }

        /// The row's accessibility identifier.
        var identifier: String {
            switch route {
            case .speaker:
                return "soundRoute.speaker"
            case .earpiece:
                return "soundRoute.earpiece"
            case .external:
                return "soundRoute.external"
            }
        }
    }

    /// The rows, in order, with a tick on `current`.
    static func rows(routes: [AudioRoute], current: AudioRoute?) -> [Row] {
        routes.map { route in
            Row(route: route, title: title(for: route), isCurrent: route == current)
        }
    }

    static func title(for route: AudioRoute) -> String {
        switch route {
        case .speaker:
            return "Speaker"
        case .earpiece:
            return "Earpiece"
        case .external(let name):
            return name
        }
    }

    /// The panel's rows now; none where the phone has no sound.
    var rows: [Row] {
        guard let audio = app.audio else {
            return []
        }
        return Self.rows(routes: audio.routes, current: audio.markedRoute)
    }

    /// Mutes or unmutes the band on this phone, as the switch does.
    func setMuted(_ muted: Bool) {
        app.setAudioMuted(muted)
    }

    /// The speaker at the radio, the section's model.
    var radioSpeaker: RadioSpeakerModel {
        app.radioSpeaker
    }

    /// The speaker at the Core, the section's model.
    var coreSpeaker: CoreSpeakerModel {
        app.coreSpeaker
    }

    /// Moves the band to `route`, as Setup's Play the band through does.
    func choose(_ route: AudioRoute) {
        app.audio?.select(route)
    }

    // The colours are the board's route menu (spec board, "From the band").
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Toggle(isOn: Binding(get: { app.audioMuted }, set: { setMuted($0) })) {
                Text("Mute this phone")
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(Self.rowText)
            }
            .tint(ChromeColours.accent)
            .padding(.horizontal, 12)
            .frame(minHeight: 44)
            .accessibilityIdentifier("soundMute")
            Rectangle()
                .fill(Self.border)
                .frame(height: 1)
                .padding(.vertical, 6)
            SoundPanelRadioSpeaker(speaker: app.radioSpeaker)
            SoundPanelCoreSpeaker(speaker: app.coreSpeaker)
            Rectangle()
                .fill(Self.border)
                .frame(height: 1)
                .padding(.vertical, 6)
            Text("Play the band through")
                .textCase(.uppercase)
                .font(.system(size: 11, weight: .bold))
                .tracking(0.9)
                .foregroundStyle(Self.headText)
                .padding(.horizontal, 6)
                .padding(.bottom, 6)
                .accessibilityAddTraits(.isHeader)
            if let audio = app.audio {
                SoundPanelRoutes(panel: self, audio: audio)
            } else {
                Text("This phone can't play the band's sound right now.")
                    .font(.system(size: 12))
                    .foregroundStyle(Self.noteText)
                    .padding(.horizontal, 6)
                    .padding(.bottom, 2)
            }
        }
        .padding(8)
        .frame(width: 250)
        .background(Self.background)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("soundPanel")
    }

    static let background = Color(red: 15 / 255, green: 15 / 255, blue: 26 / 255)
    static let border = Color(red: 0x30 / 255, green: 0x40 / 255, blue: 0x50 / 255)
    static let rowText = Color(red: 0xc8 / 255, green: 0xd8 / 255, blue: 0xe8 / 255)
    static let headText = Color(red: 0x70 / 255, green: 0x80 / 255, blue: 0x90 / 255)
    static let noteText = Color(red: 0x60 / 255, green: 0x70 / 255, blue: 0x80 / 255)
    static let currentFill = Color(red: 0x1a / 255, green: 0x2a / 255, blue: 0x3a / 255)
    /// The desktop's RADIO slider amber (phone-sound-panel.html).
    static let radioAmber = Color(red: 0xe0 / 255, green: 0xa0 / 255, blue: 0x30 / 255)
    /// How a greyed control shows (phone-sound-panel.html's disabled rows).
    static let greyed = 0.4
}

/// The Sound panel's Radio speaker section, watching the Core's level and
/// mute so the slider follows a change made anywhere. Greyed with its
/// note, never hidden, while it cannot be used.
private struct SoundPanelRadioSpeaker: View {
    @ObservedObject var speaker: RadioSpeakerModel

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Radio speaker")
                .textCase(.uppercase)
                .font(.system(size: 11, weight: .bold))
                .tracking(0.9)
                .foregroundStyle(SoundPanel.headText)
                .padding(.horizontal, 6)
                .padding(.bottom, 6)
                .accessibilityAddTraits(.isHeader)
            HStack(spacing: 10) {
                Image(systemName: "radio")
                    .font(.system(size: 17))
                    .foregroundStyle(SoundPanel.rowText)
                    .accessibilityHidden(true)
                Slider(value: Binding(get: { Double(speaker.volume ?? 0) }, set: { speaker.setVolume($0) }),
                       in: RadioSpeakerModel.volumeRange, step: 1)
                    .tint(SoundPanel.radioAmber)
                    .accessibilityLabel("Radio speaker volume")
                    .accessibilityValue(speaker.volume.map { "\($0)" } ?? "None")
                    .accessibilityIdentifier("radioSpeakerSlider")
                Text(speaker.isEnabled ? speaker.volume.map { "\($0)" } ?? "--" : "--")
                    .font(.system(size: 13).monospacedDigit())
                    .foregroundStyle(SoundPanel.headText)
                    .frame(width: 28, alignment: .trailing)
                    .accessibilityHidden(true)
            }
            .padding(.horizontal, 12)
            .frame(minHeight: 44)
            .opacity(speaker.isEnabled ? 1 : SoundPanel.greyed)
            Toggle(isOn: Binding(get: { speaker.muted }, set: { speaker.setMuted($0) })) {
                Text("Mute radio speaker")
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(SoundPanel.rowText)
            }
            .tint(SoundPanel.radioAmber)
            .padding(.horizontal, 12)
            .frame(minHeight: 44)
            .opacity(speaker.isEnabled ? 1 : SoundPanel.greyed)
            .accessibilityIdentifier("radioSpeakerMute")
            if let words = speaker.reason ?? speaker.note {
                Text(words)
                    .font(.system(size: 12))
                    .foregroundStyle(SoundPanel.noteText)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 12)
                    .padding(.bottom, 4)
                    .accessibilityIdentifier("radioSpeakerNote")
            }
        }
        .disabled(!speaker.isEnabled)
    }
}

/// The Sound panel's Core speaker section, watching the Core's level and
/// mute so the slider follows a change made anywhere, with its own divider
/// above it. Left out while the Core plays on no card (D26) and back by
/// itself when it plays on one; otherwise greyed with its reason, never
/// hidden, while it cannot be used.
private struct SoundPanelCoreSpeaker: View {
    @ObservedObject var speaker: CoreSpeakerModel

    var body: some View {
        if speaker.isShown {
            VStack(alignment: .leading, spacing: 0) {
                Rectangle()
                    .fill(SoundPanel.border)
                    .frame(height: 1)
                    .padding(.vertical, 6)
                Text("Core speaker")
                    .textCase(.uppercase)
                    .font(.system(size: 11, weight: .bold))
                    .tracking(0.9)
                    .foregroundStyle(SoundPanel.headText)
                    .padding(.horizontal, 6)
                    .padding(.bottom, 6)
                    .accessibilityAddTraits(.isHeader)
                VStack(alignment: .leading, spacing: 0) {
                    HStack(spacing: 10) {
                        Image(systemName: "hifispeaker")
                            .font(.system(size: 17))
                            .foregroundStyle(SoundPanel.rowText)
                            .accessibilityHidden(true)
                        Slider(value: Binding(get: { Double(speaker.volume ?? 0) }, set: { speaker.setVolume($0) }),
                               in: CoreSpeakerModel.volumeRange, step: 1)
                            .tint(ChromeColours.accent)
                            .accessibilityLabel("Core speaker volume")
                            .accessibilityValue(speaker.volume.map { "\($0)" } ?? "None")
                            .accessibilityIdentifier("coreSpeakerSlider")
                        Text(speaker.isEnabled ? speaker.volume.map { "\($0)" } ?? "--" : "--")
                            .font(.system(size: 13).monospacedDigit())
                            .foregroundStyle(SoundPanel.headText)
                            .frame(width: 28, alignment: .trailing)
                            .accessibilityHidden(true)
                    }
                    .padding(.horizontal, 12)
                    .frame(minHeight: 44)
                    .opacity(speaker.isEnabled ? 1 : SoundPanel.greyed)
                    Toggle(isOn: Binding(get: { speaker.muted }, set: { speaker.setMuted($0) })) {
                        Text("Mute Core speaker")
                            .font(.system(size: 15, weight: .semibold))
                            .foregroundStyle(SoundPanel.rowText)
                    }
                    .tint(ChromeColours.accent)
                    .padding(.horizontal, 12)
                    .frame(minHeight: 44)
                    .opacity(speaker.isEnabled ? 1 : SoundPanel.greyed)
                    .accessibilityIdentifier("coreSpeakerMute")
                }
                .disabled(!speaker.isEnabled)
                if let missing = speaker.missingNote {
                    note(missing, colour: SoundPanel.radioAmber)
                } else if let words = speaker.reason ?? speaker.refusal {
                    note(words, colour: SoundPanel.noteText)
                }
            }
            .accessibilityElement(children: .contain)
            .accessibilityIdentifier("coreSpeakerSection")
        }
    }

    private func note(_ words: String, colour: Color) -> some View {
        Text(words)
            .font(.system(size: 12))
            .foregroundStyle(colour)
            .fixedSize(horizontal: false, vertical: true)
            .padding(.horizontal, 12)
            .padding(.bottom, 4)
            .accessibilityIdentifier("coreSpeakerNote")
    }
}

/// The Sound panel's routes, watching the audio session so the tick moves
/// as the sound does.
private struct SoundPanelRoutes: View {
    let panel: SoundPanel
    @ObservedObject var audio: AudioSessionController

    var body: some View {
        ForEach(SoundPanel.rows(routes: audio.routes, current: audio.markedRoute)) { row in
            Button {
                panel.choose(row.route)
            } label: {
                HStack {
                    Text(row.title)
                        .font(.system(size: 14))
                        .lineLimit(1)
                    Spacer()
                    Image(systemName: "checkmark")
                        .opacity(row.isCurrent ? 1 : 0)
                        .accessibilityHidden(true)
                }
                .foregroundStyle(SoundPanel.rowText)
                .padding(.horizontal, 12)
                .frame(height: 44)
                .background(row.isCurrent ? SoundPanel.currentFill : .clear, in: RoundedRectangle(cornerRadius: 4))
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityAddTraits(row.isCurrent ? .isSelected : [])
            .accessibilityIdentifier(row.identifier)
        }
        if audio.microphone == .iPhone {
            Text("The microphone stays on the \(audio.isPhone ? "iPhone" : "iPad"). Change it in Setup.")
                .font(.system(size: 11))
                .foregroundStyle(SoundPanel.noteText)
                .padding(.horizontal, 6)
                .padding(.top, 6)
                .padding(.bottom, 2)
        }
    }
}
