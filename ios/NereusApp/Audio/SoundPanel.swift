// NereusSDR for iOS: the Sound panel the band's speaker button opens: Mute, then where the band plays
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Everything about the band's sound in one small panel, opened by one tap
/// on the speaker button without leaving the band (D77, spec section 5.4
/// item 10): a Mute switch, then where the band plays: the speaker, the
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

    /// Mutes or unmutes the band, as the switch does.
    func setMuted(_ muted: Bool) {
        app.setAudioMuted(muted)
    }

    /// Moves the band to `route`, as Setup's Play the band through does.
    func choose(_ route: AudioRoute) {
        app.audio?.select(route)
    }

    // The colours are the board's route menu (spec board, "From the band").
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Toggle(isOn: Binding(get: { app.audioMuted }, set: { setMuted($0) })) {
                Text("Mute")
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
