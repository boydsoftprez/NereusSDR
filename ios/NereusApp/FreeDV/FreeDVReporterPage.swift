// NereusSDR for iOS: FreeDV Reporter, one Tools page: the stations the Core hears, their filters, and your status
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// FreeDV Reporter on the phone (R-IOS-26, D33, D78, spec section 5.7
/// items 5 to 10, picture 21's first phone): the band buttons (All first)
/// and "follow the radio" by band or frequency; the stations, newest news
/// first, each tinted while it transmits, hears someone or changes its
/// message; then your status message and Send. A tap on a station tunes
/// there; its (i) button opens its details and actions, and Ask to QSY and
/// the status message editor open over the list.
struct FreeDVReporterPage: View {
    @ObservedObject var freedv: FreeDVReporterModel

    var body: some View {
        ZStack {
            VStack(spacing: 0) {
                filters
                stateBanner
                if let note = freedv.note, freedv.openQsy == nil, !freedv.editingMessage {
                    AccessoryChrome.Refusal(text: note) { freedv.note = nil }
                        .padding(.horizontal, 12)
                        .padding(.vertical, 6)
                }
                list
                statusBar
            }
            if let station = freedv.openDetails {
                FreeDVStationDetails(freedv: freedv, station: station)
            } else if let station = freedv.openQsy {
                AskToQsySheet(freedv: freedv, station: station)
            } else if freedv.editingMessage {
                StatusMessageEditor(freedv: freedv)
            }
        }
    }

    // MARK: The filters

    private var filters: some View {
        let bandReason = freedv.bandReason
        let follow = freedv.effectiveFollow
        return VStack(alignment: .leading, spacing: 8) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 5) {
                    bandButton("All", chosen: freedv.selectedBand == nil, enabled: true, identifier: "freedv.band.all") {
                        freedv.pickBand(nil)
                    }
                    ForEach(freedv.bandButtons, id: \.id) { band in
                        bandButton(band.label, chosen: freedv.selectedBand == band.id, enabled: bandReason == nil,
                                   identifier: "freedv.band.\(band.id)") {
                            freedv.pickBand(band.id)
                        }
                    }
                }
            }
            .modifier(NoTopScrollEdge())
            HStack {
                Text("Follow the radio")
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                Spacer()
                HStack(spacing: 0) {
                    segment("Band", chosen: follow == .band, enabled: bandReason == nil,
                            identifier: "freedv.follow.band") { freedv.toggleFollow(.band) }
                    Rectangle().fill(FreeDVColours.segmentBorder).frame(width: 1, height: 28)
                    segment("Frequency", chosen: follow == .frequency, enabled: freedv.coreReason == nil,
                            identifier: "freedv.follow.frequency") { freedv.toggleFollow(.frequency) }
                }
                .clipShape(RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(FreeDVColours.segmentBorder, lineWidth: 1))
            }
            if let bandReason, freedv.coreReason == nil {
                AccessoryChrome.Note(text: bandReason)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(FreeDVColours.top)
        .overlay(alignment: .bottom) {
            Rectangle().fill(FreeDVColours.topBorder).frame(height: 1)
        }
    }

    private func bandButton(_ title: String, chosen: Bool, enabled: Bool, identifier: String,
                            action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(!enabled ? ChromeColours.buttonOffText : (chosen ? Color.white : ChromeColours.text))
                .padding(.horizontal, 10)
                .frame(height: 30)
                .background(!enabled ? ChromeColours.buttonOff : (chosen ? ChromeColours.buttonOnBlue : ChromeColours.button),
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(!enabled ? ChromeColours.buttonOffBorder
                                  : (chosen ? ChromeColours.buttonOnBlueBorder : ChromeColours.buttonBorder), lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityAddTraits(chosen ? .isSelected : [])
        .accessibilityIdentifier(identifier)
    }

    private func segment(_ title: String, chosen: Bool, enabled: Bool, identifier: String,
                         action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(!enabled ? ChromeColours.buttonOffText : (chosen ? Color.white : ChromeColours.text))
                .padding(.horizontal, 12)
                .frame(height: 28)
                .background(!enabled ? ChromeColours.buttonOff : (chosen ? ChromeColours.buttonOnBlue : ChromeColours.button))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityAddTraits(chosen ? .isSelected : [])
        .accessibilityValue(chosen ? "On" : "Off")
        .accessibilityIdentifier(identifier)
    }

    // MARK: The reporter's state

    /// Why the list is empty or can't act: the Core's reason, or the reporter's state while it is not connected.
    @ViewBuilder
    private var stateBanner: some View {
        if let words = Self.bannerWords(freedv) {
            HStack(alignment: .top, spacing: 8) {
                Circle().fill(ConnectChrome.pillStale).frame(width: 8, height: 8).padding(.top, 4)
                Text(words)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.text)
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
            .background(ChromeColours.inset)
            .accessibilityIdentifier("freedv.state")
        }
    }

    /// The banner's words, or nil while FreeDV Reporter is connected at the Core.
    static func bannerWords(_ freedv: FreeDVReporterModel) -> String? {
        if let reason = freedv.coreReason {
            return reason
        }
        guard let status = freedv.status else {
            return nil
        }
        switch status.state {
        case .connected:
            return nil
        case .connecting:
            return status.text.isEmpty ? "FreeDV Reporter is connecting at the Core." : status.text
        case .error:
            return status.text.isEmpty ? "FreeDV Reporter stopped with an error." : "Error: \(status.text)"
        case .off:
            return status.text.isEmpty ? "FreeDV Reporter is stopped at the Core. Start it in Spot Hub, FreeDV."
                : status.text
        }
    }

    // MARK: The stations

    private var list: some View {
        TimelineView(.periodic(from: .now, by: 1)) { context in
            ScrollView {
                LazyVStack(spacing: 0) {
                    let rows = freedv.listed
                    if rows.isEmpty {
                        Text(emptyWords)
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.textFaint)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(12)
                    }
                    // Redrawn each second so a tint fades on the model's clock.
                    let _ = context.date
                    ForEach(rows) { station in
                        FreeDVStationRow(freedv: freedv, station: station, tint: freedv.tint(station))
                    }
                }
            }
        }
        .frame(maxHeight: .infinity)
        .accessibilityIdentifier("freedv.list")
    }

    private var emptyWords: String {
        if freedv.stations.isEmpty {
            return "No stations on the air."
        }
        return freedv.effectiveFollow == .frequency ? "No stations on your frequency." : "No stations on this band."
    }

    // MARK: Your status

    private var statusBar: some View {
        let reason = freedv.coreReason
        let text = freedv.statusText
        return HStack(spacing: 8) {
            Text("MY STATUS")
                .font(.system(size: 10, weight: .bold))
                .kerning(1)
                .foregroundStyle(ChromeColours.textDim)
            Button {
                freedv.editingMessage = true
            } label: {
                HStack(spacing: 4) {
                    Text(text.isEmpty ? "Status message" : text)
                        .foregroundStyle(text.isEmpty ? ChromeColours.textFaint : ChromeColours.textBright)
                        .lineLimit(1)
                    Text("\u{25BE}")
                        .foregroundStyle(ChromeColours.textDim)
                }
                .font(.system(size: 13))
                .padding(.horizontal, 8)
                .frame(maxWidth: .infinity, minHeight: 36, alignment: .leading)
                .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
                .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Status message")
            .accessibilityValue(text)
            .accessibilityHint("Type one or pick a saved one")
            .accessibilityIdentifier("freedv.status.open")
            FreeDVChrome.Choice(title: "Send", chosen: reason == nil, enabled: reason == nil,
                                identifier: "freedv.status.sendBar") {
                freedv.sendMessage(text)
            }
            .frame(width: 64)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .background(FreeDVColours.top)
        .overlay(alignment: .top) {
            Rectangle().fill(FreeDVColours.topBorder).frame(height: 1)
        }
    }
}

/// The band buttons sit right under the page's title bar. On a phone on its
/// side there is no status bar above them, and the system's soft scroll edge
/// would fade the top of the buttons; they scroll sideways only, so the edge
/// is not wanted.
private struct NoTopScrollEdge: ViewModifier {
    func body(content: Content) -> some View {
        if #available(iOS 26.0, *) {
            content.scrollEdgeEffectHidden(true, for: .top)
        } else {
            content
        }
    }
}
