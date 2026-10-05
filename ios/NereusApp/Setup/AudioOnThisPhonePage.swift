// NereusSDR for iOS: Setup's Audio page on this phone: where the band plays, the microphone, while you transmit, your voice and the quality
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Setup, Audio, On this phone (R-IOS-20; spec section 5.4 items 6 to 9;
/// picture 12, "Audio on this phone"), kept on this phone, in the board's
/// words. Where the band plays (the same choice as the band's Sound panel),
/// which microphone you talk into (the iPhone's by default), muting the
/// band while you talk (on by default) with the rule that MON plays in
/// headphones only (D80: a rule, not a choice, so the speaker never feeds
/// the microphone), iPhone voice processing (always off, so the Core's
/// processing shapes the voice) and the audio quality (R-IOS-09: High,
/// Save data or Lossless, each with its cost an hour; a row the Core does
/// not offer is greyed with its reason, never hidden; see
/// ``AudioQualityModel``).
struct AudioOnThisPhonePage: View {
    @ObservedObject var settings: PhoneSettings
    /// The band's sound on this phone; nil where there is none (tests).
    var audio: AudioSessionController?
    /// The audio quality; nil where there is none (tests), when the rows
    /// show the choice kept on this phone.
    var quality: AudioQualityModel?

    /// One row of Play the band through.
    struct RouteRow: Identifiable, Equatable {
        let route: AudioRoute
        let title: String
        let detail: String
        let isCurrent: Bool

        var id: AudioRoute {
            route
        }
    }

    /// The places the band can play, with a tick on the current one.
    static func routeRows(routes: [AudioRoute], current: AudioRoute?, isPhone: Bool) -> [RouteRow] {
        routes.map { route in
            switch route {
            case .speaker:
                return RouteRow(route: route, title: isPhone ? "iPhone speaker" : "iPad speaker",
                                detail: "The default", isCurrent: current == route)
            case .earpiece:
                return RouteRow(route: route, title: "Earpiece", detail: "Hold it to your ear, like a call",
                                isCurrent: current == route)
            case .external(let name):
                return RouteRow(route: route, title: name, detail: "Connected \u{00B7} full quality",
                                isCurrent: current == route)
            }
        }
    }

    /// The board's words for each microphone.
    static func title(_ choice: MicrophoneChoice) -> String {
        switch choice {
        case .iPhone:
            return "iPhone microphone"
        case .airPods:
            return "AirPods microphone"
        }
    }

    static func detail(_ choice: MicrophoneChoice) -> String {
        switch choice {
        case .iPhone:
            return "Keeps AirPods at full quality for the band"
        case .airPods:
            return "iOS then runs them at phone-call quality, both ways"
        }
    }

    // MARK: Actions

    /// Moves the band, as the Sound panel does.
    func choose(_ route: AudioRoute) {
        audio?.select(route)
    }

    /// The microphone you talk into, kept on this phone and applied at once.
    func choose(_ microphone: MicrophoneChoice) {
        settings.microphone = microphone
        audio?.setMicrophone(microphone)
    }

    func setMuteBandWhileTalking(_ on: Bool) {
        settings.muteBandWhileTalking = on
        applyWhileTransmitting()
    }

    private func applyWhileTransmitting() {
        audio?.setWhileTransmitting(muteBand: settings.muteBandWhileTalking, monInHeadphonesOnly: true)
    }

    /// The rule under the While you transmit settings (D80).
    static let monRule = "Monitor (MON) plays in headphones only, so the speaker can't feed back into the microphone."

    // MARK: The page

    var body: some View {
        List {
            Section {
                if let audio {
                    // Watches the sound, so a choice here or in the band's
                    // Sound panel moves the tick at once.
                    AudioRouteRows(page: self, audio: audio)
                } else {
                    ForEach(rows) { row in
                        routeChoice(row)
                    }
                }
            } header: {
                Text("Play the band through")
            }
            Section {
                ForEach(MicrophoneChoice.allCases, id: \.self) { microphone in
                    choice(Self.title(microphone), Self.detail(microphone), chosen: settings.microphone == microphone,
                           id: "microphone.\(microphone.rawValue)") {
                        choose(microphone)
                    }
                }
            } header: {
                Text("Microphone")
            }
            Section {
                toggle("Mute the band while you talk", "So the speaker can't feed back into the microphone",
                       isOn: Binding(get: { settings.muteBandWhileTalking }, set: { setMuteBandWhileTalking($0) }),
                       id: "muteBandWhileTalking")
            } header: {
                Text("While you transmit")
            } footer: {
                Text(Self.monRule)
                    .accessibilityIdentifier("monRule")
            }
            Section {
                HStack(spacing: 12) {
                    labels("iPhone voice processing", "Off: the Core's PROC, EQ and leveler shape your audio")
                    Spacer(minLength: 0)
                    Text("Off")
                        .font(.footnote.weight(.semibold))
                        .foregroundStyle(.secondary)
                }
                .accessibilityElement(children: .combine)
                .accessibilityIdentifier("voiceProcessing")
            } header: {
                Text("Your voice")
            }
            if let quality {
                AudioQualitySection(model: quality)
            } else {
                AudioQualitySection.section(
                    inputs: AudioQualityModel.Inputs(chosen: settings.audioQuality, connected: false,
                                                     qualityOffered: false, profileOffered: false,
                                                     opusBitrates: nil, heard: nil, fallback: false)) { choice in
                    settings.audioQuality = choice
                }
            }
        }
        .navigationTitle("On this phone")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .thisPhone)
            }
        }
    }

    private var rows: [RouteRow] {
        guard let audio else {
            return Self.routeRows(routes: [.speaker, .earpiece], current: nil, isPhone: true)
        }
        return Self.routeRows(routes: audio.routes, current: audio.markedRoute, isPhone: audio.isPhone)
    }

    /// One row of Play the band through.
    fileprivate func routeChoice(_ row: RouteRow) -> some View {
        choice(row.title, row.detail, chosen: row.isCurrent, id: "audioRoute.\(row.title)") {
            choose(row.route)
        }
    }

    private func labels(_ title: String, _ detail: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title)
                .font(.body.weight(.semibold))
                .foregroundStyle(.primary)
            Text(detail)
                .font(.footnote)
                .foregroundStyle(.secondary)
        }
    }

    private func choice(_ title: String, _ detail: String, chosen: Bool, id: String,
                        action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 12) {
                labels(title, detail)
                Spacer(minLength: 0)
                Image(systemName: "checkmark")
                    .foregroundStyle(ChromeColours.accent)
                    .opacity(chosen ? 1 : 0)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(chosen ? .isSelected : [])
        .accessibilityIdentifier(id)
    }

    private func toggle(_ title: String, _ detail: String, isOn: Binding<Bool>, id: String) -> some View {
        Toggle(isOn: isOn) {
            labels(title, detail)
        }
        .accessibilityIdentifier(id)
    }
}

/// Play the band through's rows, watching the band's sound.
private struct AudioRouteRows: View {
    let page: AudioOnThisPhonePage
    @ObservedObject var audio: AudioSessionController

    var body: some View {
        ForEach(AudioOnThisPhonePage.routeRows(routes: audio.routes, current: audio.markedRoute,
                                               isPhone: audio.isPhone)) { row in
            page.routeChoice(row)
        }
    }
}

/// Audio quality's rows, watching the model: each with its cost an hour
/// and, greyed, its reason; the Core's answers and the desktop's tooltip
/// words under them.
struct AudioQualitySection: View {
    @ObservedObject var model: AudioQualityModel

    var body: some View {
        Self.section(inputs: model.inputs) { choice in
            model.choose(choice)
        }
    }

    @MainActor
    static func section(inputs: AudioQualityModel.Inputs,
                        choose: @escaping (AudioQualityChoice) -> Void) -> some View {
        let shown = AudioQualityModel.shown(inputs)
        return Section {
            ForEach(AudioQualityModel.rows(inputs)) { row in
                Button {
                    choose(row.choice)
                } label: {
                    HStack(spacing: 12) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(row.title)
                                .font(.body.weight(.semibold))
                                .foregroundStyle(row.enabled ? Color.primary : Color.secondary)
                            Text(row.detail)
                                .font(.footnote)
                                .foregroundStyle(.secondary)
                            Text(row.cost)
                                .font(.footnote)
                                .foregroundStyle(.secondary)
                            if let warning = row.warning {
                                Text(warning)
                                    .font(.footnote.weight(.semibold))
                                    .foregroundStyle(.primary)
                                    .accessibilityIdentifier("quality.\(row.choice.rawValue).warning")
                            }
                            if let reason = row.reason {
                                Text(reason)
                                    .font(.footnote)
                                    .foregroundStyle(.secondary)
                                    .accessibilityIdentifier("quality.\(row.choice.rawValue).reason")
                            }
                        }
                        Spacer(minLength: 0)
                        Image(systemName: "checkmark")
                            .foregroundStyle(row.enabled ? ChromeColours.accent : Color.secondary)
                            .opacity(shown == row.choice ? 1 : 0)
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(!row.enabled)
                .accessibilityAddTraits(shown == row.choice ? .isSelected : [])
                .accessibilityIdentifier("quality.\(row.choice.rawValue)")
            }
        } header: {
            Text("Audio quality")
        } footer: {
            VStack(alignment: .leading, spacing: 6) {
                ForEach(AudioQualityModel.notes(inputs), id: \.self) { note in
                    Text(note)
                        .foregroundStyle(.primary)
                        .accessibilityIdentifier("qualityNote")
                }
                Text(AudioQualityModel.footer)
                    .accessibilityIdentifier("qualityFooter")
            }
        }
    }
}
