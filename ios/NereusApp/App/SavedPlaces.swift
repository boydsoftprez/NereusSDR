// NereusSDR for iOS: where the operator was in Setup, Tools and Radio, in a stable form kept on this phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-05: Connecting returns the operator to their place. J.J. Boyd
// (KG4VCF), AI-assisted via Claude Code.

/// The Setup, Tools and Radio pages as ``PhoneSettings`` keeps them: one
/// plain string for each page, the same on every build. Reading stops at the
/// first string this build does not know, so a page gone from the app leaves
/// the operator on the page above it. A Roundtable room is kept as
/// Roundtable's list: its title and room are live.
enum SavedPlaces {
    private static let separator: Character = "|"

    private static func join(_ parts: String...) -> String {
        parts.joined(separator: String(separator))
    }

    private static func parts(_ stored: String) -> [String] {
        stored.split(separator: separator, maxSplits: 2, omittingEmptySubsequences: false).map(String.init)
    }

    /// Everything up to the first string `read` does not know.
    private static func prefix<Route>(_ stored: [String], read: (String) -> Route?) -> [Route] {
        var routes: [Route] = []
        for item in stored {
            guard let route = read(item) else { break }
            routes.append(route)
        }
        return routes
    }

    // MARK: Setup

    static func stored(_ route: SetupTree.Route) -> String {
        switch route {
        case .category(let id): return join("category", id)
        case .page(let page): return join("page", page.rawValue)
        case .described(let category, let page): return join("described", category, page)
        case .cfcBands(let category, let control): return join("cfcBands", category, control)
        }
    }

    static func setupRoute(_ stored: String) -> SetupTree.Route? {
        let parts = parts(stored)
        switch (parts.first, parts.count) {
        case ("category", 2): return .category(parts[1])
        case ("page", 2): return SetupTree.Page(rawValue: parts[1]).map(SetupTree.Route.page)
        case ("described", 3): return .described(category: parts[1], page: parts[2])
        case ("cfcBands", 3): return .cfcBands(category: parts[1], control: parts[2])
        default: return nil
        }
    }

    static func setupPath(_ stored: [String]) -> [SetupTree.Route] {
        prefix(stored, read: setupRoute)
    }

    // MARK: Tools

    /// What is kept of the pages open on the Tools tab.
    static func stored(_ route: [ToolsTab.Page]) -> [String] {
        var kept: [String] = []
        for page in route {
            let item = stored(page)
            if kept.last != item {
                kept.append(item)
            }
        }
        return kept
    }

    private static func stored(_ page: ToolsTab.Page) -> String {
        switch page {
        case .spotHub: return "spotHub"
        case .spotHubPage(.list): return join("spotHubPage", "list")
        case .spotHubPage(.display): return join("spotHubPage", "display")
        case .spotHubPage(.source(let source)): return join("spotHubPage", "source", source.rawValue)
        case .spotHubPage(.identity): return join("spotHubPage", "identity")
        case .performance: return "performance"
        case .freedvReporter: return "freedvReporter"
        case .txEqualizer: return "txEqualizer"
        case .pureSignal: return "pureSignal"
        case .diversity: return "diversity"
        case .cwx: return "cwx"
        case .catControl: return "catControl"
        case .catControlPage(.channel(let number)): return join("catControlPage", "channel", String(number))
        case .catControlPage(.options): return join("catControlPage", "options")
        case .catControlPage(.ptt): return join("catControlPage", "ptt")
        case .catControlPage(.test): return join("catControlPage", "test")
        case .tciServer: return "tciServer"
        case .vaxAudio: return "vaxAudio"
        case .supportBundle: return "supportBundle"
        case .rotor: return "rotor"
        case .roundtable, .roundtableRoom: return "roundtable"
        }
    }

    private static func toolsPage(_ stored: String) -> ToolsTab.Page? {
        let parts = parts(stored)
        switch (parts.first, parts.count) {
        case ("spotHub", 1): return .spotHub
        case ("spotHubPage", 2):
            switch parts[1] {
            case "list": return .spotHubPage(.list)
            case "display": return .spotHubPage(.display)
            case "identity": return .spotHubPage(.identity)
            default: return nil
            }
        case ("spotHubPage", 3):
            guard parts[1] == "source", let source = SpotsModel.Source(rawValue: parts[2]) else { return nil }
            return .spotHubPage(.source(source))
        case ("performance", 1): return .performance
        case ("freedvReporter", 1): return .freedvReporter
        case ("txEqualizer", 1): return .txEqualizer
        case ("pureSignal", 1): return .pureSignal
        case ("diversity", 1): return .diversity
        case ("cwx", 1): return .cwx
        case ("catControl", 1): return .catControl
        case ("catControlPage", 2):
            switch parts[1] {
            case "options": return .catControlPage(.options)
            case "ptt": return .catControlPage(.ptt)
            case "test": return .catControlPage(.test)
            default: return nil
            }
        case ("catControlPage", 3):
            guard parts[1] == "channel", let number = Int(parts[2]) else { return nil }
            return .catControlPage(.channel(number))
        case ("tciServer", 1): return .tciServer
        case ("vaxAudio", 1): return .vaxAudio
        case ("supportBundle", 1): return .supportBundle
        case ("rotor", 1): return .rotor
        case ("roundtable", 1): return .roundtable
        default: return nil
        }
    }

    static func toolsRoute(_ stored: [String]) -> [ToolsTab.Page] {
        prefix(stored, read: toolsPage)
    }

    // MARK: Radio

    static func stored(_ route: RadioView.Route) -> String {
        switch route {
        case .accessory(.page(let device)): return join("accessory", "page", device.rawValue)
        case .accessory(.faults(let device)): return join("accessory", "faults", device.rawValue)
        case .accessory(.advanced(let device)): return join("accessory", "advanced", device.rawValue)
        case .accessory(.tuneMemory): return join("accessory", "tuneMemory")
        case .accessory(.rotor): return join("accessory", "rotor")
        case .antennaSetup: return "antennaSetup"
        case .manageRadios: return "manageRadios"
        case .protocolInfo: return "protocolInfo"
        }
    }

    private static func radioPage(_ stored: String) -> RadioView.Route? {
        let parts = parts(stored)
        switch (parts.first, parts.count) {
        case ("accessory", 2):
            switch parts[1] {
            case "tuneMemory": return .accessory(.tuneMemory)
            case "rotor": return .accessory(.rotor)
            default: return nil
            }
        case ("accessory", 3):
            guard let device = AccessoriesModel.Device(rawValue: parts[2]) else { return nil }
            switch parts[1] {
            case "page": return .accessory(.page(device))
            case "faults": return .accessory(.faults(device))
            case "advanced": return .accessory(.advanced(device))
            default: return nil
            }
        case ("antennaSetup", 1): return .antennaSetup
        case ("manageRadios", 1): return .manageRadios
        case ("protocolInfo", 1): return .protocolInfo
        default: return nil
        }
    }

    static func radioRoute(_ stored: [String]) -> [RadioView.Route] {
        prefix(stored, read: radioPage)
    }
}
