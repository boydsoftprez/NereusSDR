// NereusSDR for iOS: the Radio tab's More list, in the desktop's Radio menu order from the Core's catalogue
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusModels

/// The rest of the desktop's Radio menu on the Radio tab (spec section 5.2
/// item 5, D41): Antenna Setup, Transverters, Manage Radios and Protocol
/// Info, from the Core's own list (the catalogue's `radioItems`, link
/// document section 7.4) in its order, each shown once the Core offers it.
/// Antenna Setup is left out on a radio without antenna control (the
/// hardware-presence exception); every other item that cannot open now
/// stays listed and greyed with its reason.
///
/// Before a Core lists them (no Core, or one too old to send a
/// catalogue) the list is the items this app has pages for, in the same
/// order, each greyed with its reason when it cannot open.
enum RadioMenu {
    /// The page a row opens.
    enum Page: Equatable, Sendable {
        case antennaSetup
        case manageRadios
        case protocolInfo
    }

    /// One row.
    struct Entry: Equatable, Identifiable, Sendable {
        /// The catalogue's id (`manageRadios`).
        let id: String
        let title: String
        /// The line under the title: what it holds, or why it cannot open.
        let detail: String
        /// The page it opens; nil for an item this app has no page for.
        let page: Page?
        /// Why it cannot open now, or nil when it opens.
        let reason: String?

        var enabled: Bool { reason == nil && page != nil }
    }

    /// Whether the Core's radio has antenna control, as far as the phone can tell.
    enum Antennas: Equatable, Sendable {
        /// The Core describes its antenna page.
        case described
        /// The radio has no antenna control: the row is left out.
        case absent
        /// Not known now, with why.
        case notDescribed(String)
    }

    /// What the rows are read from.
    struct Inputs: Sendable {
        /// The Core's list; nil when it has not sent one.
        var items: [StationCatalog.RadioItem]?
        var connected: Bool
        /// The connected Core sends no catalogue at all.
        var olderCore: Bool
        /// The Core lets an app choose its radio (`stationRadiosVersion` 1 with record streams).
        var choosesRadio: Bool
        var antennas: Antennas
    }

    // MARK: Words

    static let notConnectedReason = "The Core is not connected."
    static let olderCoreRadioReason = "This Core does not let this app change its radio. Updating the Core may help."
    static let olderCoreAntennaReason =
        "This Core does not send its antenna settings to this app. Updating the Core may help."
    static let antennaWaitingReason = "The Core has not described its antenna settings."
    static let unknownItemReason = "This app does not have this page. Updating this app may help."

    static let antennaSetupId = "antennaSetup"
    static let manageRadiosId = "manageRadios"
    static let protocolInfoId = "protocolInfo"

    private struct Known {
        let id: String
        let title: String
        let detail: String
        let page: Page
    }

    /// The items this app has pages for, in the desktop's order, with the board's lines.
    private static let known: [Known] = [
        Known(id: antennaSetupId, title: "Antenna Setup", detail: "Per-band RX and TX antennas", page: .antennaSetup),
        Known(id: manageRadiosId, title: "Manage Radios", detail: "Which radio the Core connects to",
              page: .manageRadios),
        Known(id: protocolInfoId, title: "Protocol Info", detail: "Board, protocol and link details",
              page: .protocolInfo),
    ]

    static func entries(_ inputs: Inputs) -> [Entry] {
        guard let items = inputs.items else {
            return known.compactMap { entry($0, inputs) }
        }
        var entries: [Entry] = []
        for item in items where item.offered {
            guard let page = known.first(where: { $0.id == item.id }) else {
                entries.append(Entry(id: item.id, title: item.label, detail: unknownItemReason, page: nil,
                                     reason: unknownItemReason))
                continue
            }
            if let entry = entry(page, inputs) {
                entries.append(entry)
            }
        }
        return entries
    }

    private static func entry(_ item: Known, _ inputs: Inputs) -> Entry? {
        var reason: String?
        if !inputs.connected {
            reason = notConnectedReason
        }
        switch item.page {
        case .antennaSetup:
            switch inputs.antennas {
            case .absent:
                return nil
            case .described:
                break
            case .notDescribed(let why):
                reason = reason ?? why
            }
        case .manageRadios:
            if reason == nil, !inputs.choosesRadio {
                reason = olderCoreRadioReason
            }
        case .protocolInfo:
            break
        }
        return Entry(id: item.id, title: item.title, detail: reason ?? item.detail, page: item.page, reason: reason)
    }
}
