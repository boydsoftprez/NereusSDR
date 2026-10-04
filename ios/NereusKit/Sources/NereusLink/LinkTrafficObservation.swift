// NereusSDR for iOS: application payload counts for one control transport lifetime
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// An exact cumulative reading from one physical control transport. A new
/// lifetime requires a new baseline; a retired reading is historical only.
/// Binary media, framing, heartbeats and network overhead are excluded.
public struct LinkTrafficObservation: Sendable, Equatable {
    public let lifetime: UUID
    public let active: Bool
    public let receivedPayloadBytes: UInt64
    public let acceptedPayloadBytes: UInt64

    public init(lifetime: UUID, active: Bool, receivedPayloadBytes: UInt64, acceptedPayloadBytes: UInt64) {
        self.lifetime = lifetime
        self.active = active
        self.receivedPayloadBytes = receivedPayloadBytes
        self.acceptedPayloadBytes = acceptedPayloadBytes
    }
}
