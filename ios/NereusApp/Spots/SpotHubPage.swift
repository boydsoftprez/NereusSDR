// NereusSDR for iOS: Spot Hub, one Tools page: the Spot List and Display, each source with its state, your identity
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Spot Hub on the phone (R-IOS-25, D32, spec section 5.7 item 1, picture
/// 20's first phone): one page in place of the desktop's tabs. The Spot
/// List and Display first, then each spot source with a dot for its state,
/// then your identity. Each part is marked with where it lives: the list,
/// the sources and your identity at the Core, the look on the band on this
/// phone. A source the Core does not run from this app is listed, greyed,
/// with its reason.
struct SpotHubPage: View {
    /// A page under Spot Hub.
    enum Route: Hashable {
        case list
        case display
        case source(SpotsModel.Source)
        case identity

        var title: String {
            switch self {
            case .list: return "Spot List"
            case .display: return "Display"
            case .source(let source): return source.title
            case .identity: return "Settings"
            }
        }
    }

    @ObservedObject var spots: SpotsModel
    let open: (Route) -> Void

    static let note =
        "The Core keeps the source connections, so spots keep arriving while the phone is away. How they look on the band is set on each phone."

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Heading(text: "Spots")
            Card {
                Row(title: "Spot List", detail: spots.listSummary, tag: .core, identifier: "spotHub.list") {
                    open(.list)
                }
                Line()
                Row(title: "Display", detail: spots.displaySummary, tag: .thisPhone, identifier: "spotHub.display") {
                    open(.display)
                }
            }
            Heading(text: "Sources, at the Core").padding(.top, 8)
            Card {
                ForEach(Array(SpotsModel.Source.allCases.enumerated()), id: \.element) { index, source in
                    if index > 0 {
                        Line()
                    }
                    // FreeDV Reporter's page opens once the Core sends its state.
                    let enabled = source == .freeDv ? spots.freeDvStatus != nil : source.runsAtCore && spots.available
                    Row(title: source.title, detail: spots.sourceLine(source), dot: Self.dot(spots, source),
                        enabled: enabled, identifier: "spotHub.\(source.rawValue)") {
                        open(.source(source))
                    }
                }
            }
            Heading(text: "Identity").padding(.top, 8)
            Card {
                Row(title: "Settings", detail: spots.identitySummary, tag: .core, identifier: "spotHub.identity") {
                    open(.identity)
                }
            }
            ConnectChrome.Note(text: Self.note).padding(.top, 4)
        }
    }

    /// A source's dot: green connected, amber connecting (or FreeDV
    /// Reporter off with the Core's reason), red in error, grey off or not
    /// run by the Core.
    static func dot(_ spots: SpotsModel, _ source: SpotsModel.Source) -> Color {
        if source == .freeDv, let status = spots.freeDvStatus {
            if status.state == .off {
                return status.text.isEmpty ? ConnectChrome.pillUnknown : ConnectChrome.pillStale
            }
            return stateDot(status.state)
        }
        guard source.runsAtCore, spots.available else {
            return ConnectChrome.pillUnknown
        }
        return stateDot(spots.status(source).state)
    }

    private static func stateDot(_ state: SpotsModel.SourceState) -> Color {
        switch state {
        case .connected: return ConnectChrome.pillOn
        case .connecting: return ConnectChrome.pillStale
        case .error: return ConnectChrome.pillOff
        case .off: return ConnectChrome.pillUnknown
        }
    }

    // MARK: The parts every Spot Hub page shares

    /// A section's heading, with where its settings live.
    struct Heading: View {
        let text: String
        var tag: SetupTag?

        var body: some View {
            HStack(spacing: 6) {
                Text(text.uppercased())
                    .font(.system(size: 11, weight: .bold))
                    .kerning(1.1)
                    .foregroundStyle(ConnectChrome.heading)
                    .accessibilityAddTraits(.isHeader)
                if let tag {
                    SetupTagBadge(tag: tag)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, 2)
        }
    }

    /// A card of rows.
    struct Card<Content: View>: View {
        @ViewBuilder var content: () -> Content

        var body: some View {
            VStack(spacing: 0) {
                content()
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
        }
    }

    /// The line between two rows.
    struct Line: View {
        var body: some View {
            Rectangle().fill(ConnectChrome.rowBorder).frame(height: 1)
        }
    }

    /// A row that opens a page: an optional state dot, its title over its
    /// line, where it lives, and a chevron. Greyed, it opens nothing and
    /// its line says why.
    struct Row: View {
        let title: String
        let detail: String
        var dot: Color?
        var tag: SetupTag?
        var enabled = true
        let identifier: String
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                HStack(spacing: 10) {
                    if let dot {
                        Circle().fill(dot).frame(width: 10, height: 10)
                    }
                    VStack(alignment: .leading, spacing: 2) {
                        Text(title)
                            .font(.system(size: 14, weight: .bold))
                            .foregroundStyle(enabled ? ChromeColours.textBright : ChromeColours.textDim)
                        Text(detail)
                            .font(.system(size: 11))
                            .foregroundStyle(ChromeColours.textDim)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    if let tag {
                        SetupTagBadge(tag: tag)
                    }
                    Image(systemName: "chevron.right")
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundStyle(enabled ? ChromeColours.textFaint : ChromeColours.buttonOffText)
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 9)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityIdentifier(identifier)
        }
    }

    /// A row with a setting at its right: its title, the line under it, and the control.
    struct SettingRow<Control: View>: View {
        let title: String
        var detail: String?
        @ViewBuilder var control: () -> Control

        var body: some View {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(title)
                        .font(.system(size: 14))
                        .foregroundStyle(ChromeColours.text)
                    if let detail {
                        Text(detail)
                            .font(.system(size: 11))
                            .foregroundStyle(ChromeColours.textDim)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                control()
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 9)
            .frame(minHeight: 48)
        }
    }

    /// An On or Off button, green when on (the board's `.stog`).
    struct OnOff: View {
        let isOn: Bool
        var enabled = true
        let identifier: String
        let toggle: () -> Void

        var body: some View {
            Button(action: toggle) {
                Text(isOn ? "On" : "Off")
                    .font(.system(size: 12, weight: .bold))
                    .foregroundStyle(isOn ? ChromeColours.buttonOnGreenText : ChromeColours.text)
                    .frame(width: 44, height: 30)
                    .background(isOn ? ChromeColours.buttonOnGreen : ChromeColours.button,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(isOn ? ChromeColours.buttonOnGreenBorder : ChromeColours.buttonBorder, lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .opacity(enabled ? 1 : 0.5)
            .accessibilityValue(isOn ? "On" : "Off")
            .accessibilityIdentifier(identifier)
        }
    }

    /// The seven source pills (the desktop's), blue while on. A pill that
    /// can't filter anything on this phone is greyed.
    struct Pills: View {
        let isOn: (SpotsModel.Source) -> Bool
        let enabled: (SpotsModel.Source) -> Bool
        let prefix: String
        let toggle: (SpotsModel.Source) -> Void

        var body: some View {
            HStack(spacing: 5) {
                ForEach(SpotsModel.Source.allCases) { source in
                    let on = isOn(source) && enabled(source)
                    Button {
                        toggle(source)
                    } label: {
                        Text(source.pill)
                            .font(.system(size: 11, weight: .bold))
                            .foregroundStyle(on ? Color.white : (enabled(source) ? ChromeColours.text
                                                                                   : ChromeColours.buttonOffText))
                            .frame(maxWidth: .infinity, minHeight: 32)
                            .background(on ? ChromeColours.buttonOnBlue
                                           : (enabled(source) ? ChromeColours.button : ChromeColours.buttonOff),
                                        in: RoundedRectangle(cornerRadius: 4))
                            .overlay(RoundedRectangle(cornerRadius: 4)
                                .strokeBorder(on ? ChromeColours.buttonOnBlueBorder
                                                 : (enabled(source) ? ChromeColours.buttonBorder
                                                                    : ChromeColours.buttonOffBorder), lineWidth: 1))
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .disabled(!enabled(source))
                    .accessibilityLabel(source.title)
                    .accessibilityValue(on ? "Shown" : "Hidden")
                    .accessibilityIdentifier("\(prefix).\(source.rawValue)")
                }
            }
        }
    }
}
