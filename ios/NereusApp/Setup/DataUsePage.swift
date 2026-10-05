// NereusSDR for iOS: Setup's Data use page: what the band asks for on Wi-Fi and on cellular, the counters and the warning
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMedia
import SwiftUI

/// Setup, CAT & Network, Data use (R-IOS-23, spec section 5.4 items 11 to
/// 13, D28, picture 12), in the board's words, kept on this phone: this
/// session's data and the month's on cellular; one choice for Wi-Fi (Full
/// by default) and one for cellular (Balanced by default), each with its
/// estimated cost; and the warning past 5 GB a month on cellular, on by
/// default. The costs are the spec's estimates, and the footnote says so.
struct DataUsePage: View {
    @ObservedObject var settings: PhoneSettings
    @ObservedObject var meter: DataUseMeter

    /// The mode totals include High audio. Lossless's 48 kHz, two-channel
    /// 16-bit packets (the microphone repeats mono into both channels) are
    /// about 1.6 Mbit/s including transport overhead, or 720 MB an hour.
    /// Opus uses the existing 24/13 MB estimates for 48/24 kbit/s; the mic
    /// uses High after a failed Lossless negotiation or fallback.
    static let footnote = "The figures above include High audio. Save data audio uses about 13 MB an hour; "
        + "Lossless audio uses about 720 MB an hour while the connection carries it. "
        + "Transmitting adds about 24 MB for each hour of talking at High, 13 MB at Save data, "
        + "or 720 MB while the microphone connection carries Lossless. If it cannot, the microphone uses High. "
        + "These are estimates; actual data use varies."

    /// Said plainly under the cellular choices while Lossless is chosen
    /// (R-IOS-09): the band's figures assume High.
    static let losslessOnCellular = "Lossless audio is chosen in Setup, Audio. On cellular it uses about 720 MB an hour."

    /// The cellular choices' footer: the Lossless cost while it is chosen.
    static func cellularFooter(_ quality: AudioQualityChoice) -> String? {
        quality == .lossless ? losslessOnCellular : nil
    }

    /// A Wi-Fi row's line: what the mode asks for and its cost.
    static func wifiDetail(_ mode: SessionPolicy.Mode) -> String {
        "\(DataModeText.asks(mode)) \u{00B7} \(DataModeText.estimate(mode))"
    }

    /// A cellular row's line; Full is "The same as Wi-Fi".
    static func cellularDetail(_ mode: SessionPolicy.Mode) -> String {
        let asks = mode == .full ? "The same as Wi-Fi" : DataModeText.asks(mode)
        return "\(asks) \u{00B7} \(DataModeText.estimate(mode))"
    }

    var body: some View {
        List {
            Section {
                counter("This session", Self.sessionText(meter), id: "dataThisSession")
                counter("This month on cellular", DataUseMeter.text(meter.monthCellular.total), id: "dataThisMonth")
            }
            Section {
                ForEach(SessionPolicy.Mode.wifiModes, id: \.self) { mode in
                    SetupChoiceRow(title: DataModeText.title(mode), detail: Self.wifiDetail(mode),
                                   chosen: settings.wifiMode == mode, id: "wifi.\(DataModeText.title(mode))") {
                        settings.wifiMode = mode
                    }
                }
            } header: {
                Text("On Wi-Fi")
            }
            Section {
                ForEach(SessionPolicy.Mode.cellularModes, id: \.self) { mode in
                    SetupChoiceRow(title: DataModeText.title(mode), detail: Self.cellularDetail(mode),
                                   chosen: settings.cellularMode == mode, id: "cellular.\(DataModeText.title(mode))") {
                        settings.cellularMode = mode
                    }
                }
            } header: {
                Text("On cellular")
            } footer: {
                if let footer = Self.cellularFooter(settings.audioQuality) {
                    Text(footer)
                        .foregroundStyle(.primary)
                        .accessibilityIdentifier("cellularLossless")
                }
            }
            Section {
                SetupSwitchRow(title: "Warn me past 5 GB a month on cellular", detail: "A notice on the band",
                               isOn: Binding(get: { settings.cellularWarning }, set: { settings.cellularWarning = $0 }),
                               id: "cellularWarning")
            } footer: {
                Text(Self.footnote)
            }
        }
        .navigationTitle("Data use")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .thisPhone)
            }
        }
    }

    private func counter(_ title: String, _ value: String, id: String) -> some View {
        HStack {
            Text(title)
                .foregroundStyle(.secondary)
            Spacer(minLength: 8)
            Text(value)
                .font(.body.weight(.semibold).monospacedDigit())
                .accessibilityIdentifier(id)
        }
        .accessibilityElement(children: .combine)
    }

    /// The whole session, which can span Wi-Fi and cellular.
    static func sessionText(_ meter: DataUseMeter) -> String {
        "\(DataUseMeter.text(meter.session.total)) total"
    }
}
