// NereusSDR for iOS: how the phone's microphone start ended, for the PTT's key
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// What ``PttController``'s microphone start came to. A voice key goes to
/// the Core only after `started`; on `failed` nothing is sent, and PTT
/// shows `reason`, the operator's words for why.
public enum MicrophoneStart: Equatable, Sendable {
    case started
    case failed(reason: String)
}
