// NereusSDR for iOS: where the band plays: the speaker, the earpiece or something connected
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One place the band can play (spec section 5.4 item 6).
enum AudioRoute: Hashable, Sendable, Identifiable {
    /// The phone's own loudspeaker, the default.
    case speaker
    /// The iPhone's earpiece, held to the ear like a call. iPads have none.
    case earpiece
    /// AirPods, other Bluetooth headphones or wired headphones, by the name
    /// the phone gives them.
    case external(name: String)

    var id: Self {
        self
    }

    /// How the choice is kept on this phone.
    var storedForm: String {
        switch self {
        case .speaker: return "speaker"
        case .earpiece: return "earpiece"
        case .external(let name): return Self.externalPrefix + name
        }
    }

    /// The choice from its kept form; nil for one this build does not know.
    init?(storedForm: String) {
        switch storedForm {
        case "speaker": self = .speaker
        case "earpiece": self = .earpiece
        default:
            guard storedForm.hasPrefix(Self.externalPrefix) else { return nil }
            self = .external(name: String(storedForm.dropFirst(Self.externalPrefix.count)))
        }
    }

    private static let externalPrefix = "external:"
}
