// NereusSDR for iOS: why a display datagram was not accepted
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why a display datagram was not accepted (`none` when it was). The names
/// match the station's decoder and the link's conformance vectors.
public enum DisplayDecodeReason: String, Equatable, Sendable {
    case none
    case invalidInput
    case noHistory
    case sequenceGap
    case staleSequence
    case oldContext
    case contextMismatch
    case badMagic
    case unsupportedVersion
    case unknownFlags
    case truncated
    case oversized
    case malformed
}
