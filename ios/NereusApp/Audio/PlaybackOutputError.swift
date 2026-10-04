// NereusSDR for iOS: why the band's audio output could not start
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why a ``PlaybackOutput`` could not start. For the log, never for the operator.
enum PlaybackOutputError: Error, Equatable, Sendable {
    /// The playback core could not make its source node.
    case noSourceNode
}
