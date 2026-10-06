// NereusSDR for iOS: what came of one setting written to the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The answer to `SettingsProxyClient.write` (link document section 8.1).
public enum SettingsWriteOutcome: Equatable, Sendable {
    /// The Core stored the value and echoed it back.
    case accepted
    /// The Core refused it, with its own reason (empty when it gave none);
    /// the cache holds the Core's value again.
    case rejected(reason: String)
    /// Not a setting the Core keeps: each app keeps it itself, so nothing
    /// was sent and the cache is unchanged.
    case keptOnThisDevice
    /// Not sent: the snapshot or authority was stale, no captured route was
    /// available, or the transport refused handoff. Optimism is rolled back
    /// unless a newer operation on the same key is still pending.
    case notSent
    /// The session was lost before the Core answered, including sends still
    /// queued at loss. The cache remains readable until the next snapshot.
    case linkLost
    /// No answer came within ``MirrorStore/answerDeadline``: the cache keeps
    /// latest Core value, marked not confirmed while the pending answer
    /// remains observable.
    case notConfirmed
}


extension SettingsWriteOutcome {
    /// Existing property-note wording shared by ordinary settings owners.
    public var propertyOutcome: PropertyWriteOutcome {
        switch self {
        case .accepted, .keptOnThisDevice: return .init(accepted: true, reason: "", value: nil)
        case .rejected(let reason): return .init(accepted: false, reason: reason, value: nil)
        case .notSent: return .notSent
        case .linkLost: return .linkLost
        case .notConfirmed: return .notConfirmed
        }
    }
}
