// NereusSDR for iOS: why a Core would not have this app, or ended its session
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why a session stopped short or ended. The strings are the Core's own
/// words, shown as sent, or the app's plain words where the app decided.
/// `code` is the end's stable code (link document section 12.4), which the
/// app acts on; an older Core sends none, and the words are then all there is.
public struct Refusal: Sendable, Equatable {
    public enum Reason: Sendable, Equatable {
        /// No shared link version, and the Core is the older side.
        case stationTooOld(station: [UInt16], app: [UInt16])
        /// No shared link version, and this app is the older side.
        case appTooOld(station: [UInt16], app: [UInt16])
        /// Sign-in was refused, or the Core is not the one this app knows.
        case authentication(String)
        /// The Core ended the session; `retryable` is its own classification.
        case ended(String, retryable: Bool)
    }

    /// The stable code of an end (section 12.4).
    public enum Code: Sendable, Hashable {
        /// Another app signed in and took the Core.
        case takenOver
        /// No link version is shared.
        case linkVersion
        /// The Core signs in paired devices only; pair this device first.
        case pairingRequired
        /// The Core did not accept the token.
        case wrongToken
        /// The Core has not paired this device.
        case deviceNotPaired
        /// This device's sign-in did not prove itself.
        case deviceProofFailed
        /// This device was removed from the Core.
        case deviceRemoved
        /// The app's own end: the Core is not the one this app paired with.
        case identityChanged
        /// The Core could not read what this app sent, or it came out of turn.
        case protocolError
        /// A code this app does not know, kept as sent.
        case other(String)

        /// The code a wire name stands for.
        public init(wireName: String) {
            switch wireName {
            case "takenOver": self = .takenOver
            case "linkVersion": self = .linkVersion
            case "pairingRequired": self = .pairingRequired
            case "wrongToken": self = .wrongToken
            case "deviceNotPaired": self = .deviceNotPaired
            case "deviceProofFailed": self = .deviceProofFailed
            case "deviceRemoved": self = .deviceRemoved
            case "identityChanged": self = .identityChanged
            case "protocolError": self = .protocolError
            default: self = .other(wireName)
            }
        }

        /// The code of an end the station sent. `identityChanged` is the
        /// app's own end, which a station never sends (section 12.4), so
        /// from a station it is kept as an unknown code.
        public init(stationWireName: String) {
            self = stationWireName == "identityChanged" ? .other(stationWireName) : Code(wireName: stationWireName)
        }

        /// The code as the link writes it.
        public var wireName: String {
            switch self {
            case .takenOver: return "takenOver"
            case .linkVersion: return "linkVersion"
            case .pairingRequired: return "pairingRequired"
            case .wrongToken: return "wrongToken"
            case .deviceNotPaired: return "deviceNotPaired"
            case .deviceProofFailed: return "deviceProofFailed"
            case .deviceRemoved: return "deviceRemoved"
            case .identityChanged: return "identityChanged"
            case .protocolError: return "protocolError"
            case .other(let text): return text
            }
        }
    }

    public let reason: Reason
    public let code: Code?

    public init(_ reason: Reason, code: Code? = nil) {
        self.reason = reason
        self.code = code
    }
}
