// NereusSDR for iOS: why the playback path could not be set up
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why an ``AudioPlaybackCore`` could not be made. For the log, never for
/// the operator.
public enum AudioPlaybackError: Error, Equatable, Sendable {
    /// The ring or its shared numbers could not be allocated.
    case outOfMemory
}
