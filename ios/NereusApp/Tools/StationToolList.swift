// NereusSDR for iOS: the Tools tab's list, in the desktop's order from the Core's catalogue, each marked where it runs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusModels

/// The tools the Tools tab lists (spec section 5.2 item 3, R-IOS-18, D41,
/// D42): the Core's own list from its catalogue (`tools`, link document
/// section 7.4), in the desktop's order, each marked Core or Both as the
/// catalogue says where it works. The catalogue changes while the tab is
/// open, and the list follows.
///
/// A tool the Core offers opens. One it does not offer is left out when
/// the desktop has not built it (D41) or the radio lacks its hardware
/// (PureSignal, Diversity: JJ's hardware-presence exception); a tool this
/// app has a page for that the Core does not offer for another reason (a
/// Core without its own TCI server, one whose computer has no VAX audio)
/// stays listed and greyed with that reason (Task 59).
///
/// Before a Core has listed its tools (no Core yet, or one too old to send a
/// catalogue) the list is the tools this app has pages for, in the same
/// order; those that work at the Core stay listed and greyed, with the
/// reason (no Core, a Core too old to list them, or one that has not
/// listed them). Connection and performance is this phone's own measurement, so it
/// is always listed and always opens.
enum StationToolList {
    /// The page a row opens.
    enum Page: Equatable, Sendable {
        case spotHub
        case freedvReporter
        case txEqualizer
        case pureSignal
        case diversity
        case tciServer
        case vaxAudio
        case performance
        case supportBundle
    }

    /// One row of the list.
    struct Entry: Equatable, Identifiable, Sendable {
        /// The catalogue's id for the tool (`txEqualizer`).
        let id: String
        let title: String
        /// The line under the title: what the tool is, or why it cannot open.
        let detail: String
        let tag: SetupTag
        /// The page it opens; nil for a tool this app has no page for.
        let page: Page?
        /// Why it cannot open now, or nil when it opens.
        let reason: String?

        var enabled: Bool { reason == nil }
    }

    // MARK: Words

    /// As ``SpotsModel/notConnectedReason`` says it.
    static let notConnectedReason = "The Core is not connected."
    static let olderCoreReason = "This Core does not list its tools. Updating the Core may help."
    static let notListedReason = "The Core has not listed its tools."
    static let unknownToolReason = "This app does not have this tool. Updating this app may help."
    static let noTciServerReason = "This Core does not run its own TCI server. Updating the Core may help."
    static let noVaxReason = "This Core's computer has no VAX audio devices."
    static let notOfferedReason = "This Core does not offer this tool."

    /// The tools a radio without their hardware leaves out when the Core
    /// does not offer them (the hardware-presence exception).
    static let hardwareTools: Set<String> = ["pureSignal", "diversity"]

    /// Why a tool this app has a page for cannot open when the Core does
    /// not offer it, or nil when it is left out instead.
    static func reasonWhenNotOffered(_ id: String) -> String? {
        if hardwareTools.contains(id) {
            return nil
        }
        switch id {
        case "tciServer": return noTciServerReason
        case "vaxAudio": return noVaxReason
        default: return notOfferedReason
        }
    }

    /// The catalogue's ids for the tools this app has pages for.
    static let performanceId = "networkDiagnostics"

    /// One tool this app knows: its page, its title and its line.
    private struct Known {
        let id: String
        let title: String
        let detail: String
        let tag: SetupTag
        let page: Page
    }

    /// The tools this app has pages for, in the desktop's order, as a Core
    /// lists them (`where` station for Core, both for Both).
    private static let known: [Known] = [
        Known(id: "spotHub", title: "Spot Hub", detail: "DX cluster, RBN, WSJT-X, POTA and PSK Reporter spots",
              tag: .both, page: .spotHub),
        Known(id: "freedvReporter", title: "FreeDV Reporter", detail: "Stations on the air, as the Core hears them",
              tag: .core, page: .freedvReporter),
        Known(id: "txEqualizer", title: "TX Equalizer", detail: "Ten-band transmit equalizer and profiles",
              tag: .core, page: .txEqualizer),
        Known(id: "pureSignal", title: "PureSignal", detail: "On, off and status; calibration stays at the Core",
              tag: .core, page: .pureSignal),
        Known(id: "diversity", title: "Diversity", detail: "Two-receiver diversity and phasing",
              tag: .core, page: .diversity),
        Known(id: "tciServer", title: "TCI Server", detail: "TCI apps connected to the Core",
              tag: .core, page: .tciServer),
        Known(id: "vaxAudio", title: "VAX Audio", detail: "Audio for digital-mode apps on the Core's computer",
              tag: .core, page: .vaxAudio),
        Known(id: performanceId, title: "Connection and performance",
              detail: "Live routes, traffic, audio and Core history", tag: .both, page: .performance),
        Known(id: "supportBundle", title: "Support Bundle", detail: "This phone's log and the Core's bundle, to share",
              tag: .both, page: .supportBundle),
    ]

    /// The list, from the Core's `tools` when it has sent them (nil when
    /// it has not), with `connected` saying the Core is connected now and
    /// `olderCore` that the connected Core sends no catalogue at all.
    static func entries(tools: [StationCatalog.Tool]?, connected: Bool, olderCore: Bool) -> [Entry] {
        guard let tools else {
            let reason = olderCore ? olderCoreReason : connected ? notListedReason : notConnectedReason
            return known.map { tool in
                entry(tool, tag: tool.tag, reason: tool.tag == .core ? reason : nil)
            }
        }
        var entries: [Entry] = []
        for tool in tools {
            let tag: SetupTag = tool.where == "both" ? .both : .core
            guard let page = known.first(where: { $0.id == tool.id }) else {
                // Not built on the desktop, or not by this app: listed only once the Core offers it.
                if tool.offered {
                    entries.append(Entry(id: tool.id, title: tool.label, detail: unknownToolReason, tag: tag,
                                         page: nil, reason: unknownToolReason))
                }
                continue
            }
            // This phone measures its connection itself, so that page opens whatever the Core says.
            if tool.offered || tool.id == performanceId {
                let reason = tag == .core && !connected ? notConnectedReason : nil
                entries.append(entry(page, tag: tag, reason: reason))
            } else if let reason = reasonWhenNotOffered(tool.id) {
                entries.append(entry(page, tag: tag, reason: !connected ? notConnectedReason : reason))
            }
        }
        // This phone measures its connection itself, so its page stays
        // listed even from a Core that leaves it out.
        if !entries.contains(where: { $0.id == performanceId }), let performance = known.first(where: {
            $0.id == performanceId
        }) {
            entries.append(entry(performance, tag: performance.tag, reason: nil))
        }
        return entries
    }

    private static func entry(_ tool: Known, tag: SetupTag, reason: String?) -> Entry {
        // A Core tool shows its reason on its line; the rest keep their line.
        Entry(id: tool.id, title: tool.title, detail: reason ?? tool.detail, tag: tag, page: tool.page,
              reason: reason)
    }
}
