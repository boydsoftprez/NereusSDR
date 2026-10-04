// NereusSDR for iOS: VAX Audio, one Tools page: the VAX channels of the Core's computer, their levels, mutes and meters
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// VAX Audio on the phone (spec section 5.2 item 4, R-IOS-18): the Core's
/// VAX channels as its computer's VAX applet shows them. Each channel shows
/// its device and the slices feeding it, its level, its meter and its mute;
/// then the transmit slice with the level and meter of VAX used as the
/// microphone. Every control stays visible; one that cannot change now is
/// greyed with the reason written under it.
struct VaxAudioPage: View {
    @ObservedObject var model: VaxAudioModel

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "Channels", tag: .core)
            if let reason = model.reason {
                SpotHubPage.Card {
                    ToolPageParts.Reason(text: reason, identifier: "vax.reason")
                }
            } else if let reason = model.levelsReason {
                SpotHubPage.Card {
                    ToolPageParts.Reason(text: reason, identifier: "vax.levelsReason")
                }
            }
            ForEach(1...StationVax.channelCount, id: \.self) { number in
                channelCard(number)
            }
            SpotHubPage.Heading(text: "VAX microphone", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                ToolPageParts.Reading(title: "Transmit slice",
                                      value: model.vax.map { VaxAudioModel.slicesText($0.txSlice) } ?? "--",
                                      identifier: "vax.txSlice")
                SpotHubPage.Line()
                ToolPageParts.SliderRow(title: "Level", value: model.vax?.txGain, range: StationVax.gainRange,
                                        step: 0.01, shown: VaxAudioModel.percent, enabled: model.txReason == nil,
                                        identifier: "vax.txGain", notConfirmed: model.isUnconfirmed(StationVax.txGainProperty)) { model.setTxGain($0) }
                SpotHubPage.Line()
                Meter(title: "Meter", level: model.levels?.tx, identifier: "vax.txMeter")
                if model.reason == nil, let reason = model.txReason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "vax.txReason")
                }
            }
            if let note = model.note {
                ToolPageParts.Refusal(text: note, identifier: "vax.note").padding(.top, 4)
            }
            ConnectChrome.Note(text: VaxAudioModel.note).padding(.top, 4)
        }
        .onAppear { model.setOpen(true) }
        .onDisappear { model.setOpen(false) }
    }

    private func channelCard(_ number: Int) -> some View {
        let channel = model.vax?.channels.first { $0.number == number }
        let enabled = model.reason == nil
        return SpotHubPage.Card {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("VAX \(number)")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(ChromeColours.textBright)
                    Text(channel.map { $0.device.isEmpty ? "No device" : $0.device } ?? "--")
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("vax.ch\(number).device")
                }
                Spacer(minLength: 8)
                Text(channel.map { VaxAudioModel.slicesText($0.slices) } ?? "--")
                    .font(.system(size: 13).monospacedDigit())
                    .foregroundStyle(ChromeColours.textBright)
                    .accessibilityIdentifier("vax.ch\(number).slices")
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .accessibilityElement(children: .combine)
            SpotHubPage.Line()
            ToolPageParts.SliderRow(title: "Level", value: channel?.rxGain, range: StationVax.gainRange, step: 0.01,
                                    shown: VaxAudioModel.percent, enabled: enabled,
                                    identifier: "vax.ch\(number).gain", notConfirmed: model.isUnconfirmed(StationVax.rxGainProperty(number))) { model.setRxGain(number, $0) }
            SpotHubPage.Line()
            Meter(title: "Meter", level: model.levels?.channels[number - 1], identifier: "vax.ch\(number).meter")
            SpotHubPage.Line()
            SpotHubPage.SettingRow(title: "Mute", detail: "Silences this channel for the apps, not the speaker") {
                SpotHubPage.OnOff(isOn: channel?.muted ?? false, enabled: enabled && channel?.muted != nil,
                                  identifier: "vax.ch\(number).mute") {
                    model.setMuted(number, !(channel?.muted ?? false))
                }
                .accessibilityLabel("Mute VAX \(number)")
            }
        }
    }

    /// A meter as the Core reads it, 0 to 1, with its value; "--" when none is here.
    struct Meter: View {
        let title: String
        let level: Double?
        let identifier: String

        var body: some View {
            HStack(spacing: 10) {
                Text(title)
                    .font(.system(size: 13))
                    .foregroundStyle(level == nil ? ChromeColours.textDim : ChromeColours.text)
                    .frame(width: 72, alignment: .leading)
                GeometryReader { geometry in
                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 3).fill(ChromeColours.inset)
                        RoundedRectangle(cornerRadius: 3)
                            .fill(ChromeColours.accent)
                            .frame(width: geometry.size.width * CGFloat(min(max(level ?? 0, 0), 1)))
                    }
                    .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
                }
                .frame(height: 10)
                ToolPageParts.ValueBox(text: level.map(VaxAudioModel.percent) ?? "--")
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .frame(minHeight: 44)
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(title)
            .accessibilityValue(level.map(VaxAudioModel.percent) ?? "Not sent")
            .accessibilityIdentifier(identifier)
        }
    }
}
