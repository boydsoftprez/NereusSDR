// NereusSDR for iOS: why the media control client did not send an operation
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why ``MediaControlClient`` sent nothing. For the log and the app's own
/// code, never for the operator.
public enum MediaControlError: Error, Equatable, Sendable {
    /// No media connection has been started with the Core in this session:
    /// the Core does not offer media, the snapshot is not complete, or the
    /// connection was retired.
    case noMediaConnection
    /// No endpoint with this ID is held.
    case unknownEndpoint
    /// The Core does not offer this operation, or the endpoint has no
    /// accepted context for it yet.
    case notOffered
}
