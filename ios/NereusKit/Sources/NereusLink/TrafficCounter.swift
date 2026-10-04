// NereusSDR for iOS: every byte the app sends to or takes from a Core, counted for the data use page
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import os

/// The app's traffic with its Cores (R-IOS-23): the control connection's
/// messages and the media connection's display messages and audio packets,
/// in and out, as the app hands them to the network or takes them from it.
/// The totals only grow; the data use meter reads them now and then and
/// counts the difference. The network's own headers and encryption are not
/// counted, so the totals run a little under what the carrier counts.
///
/// Each count is kept by ``Kind`` as well, for the measurement log (plan
/// Task 68); ``reading`` is the sum over every kind.
public final class TrafficCounter: Sendable {
    /// One reading: the bytes taken in and sent out since the app started.
    public struct Totals: Sendable, Equatable {
        public var bytesIn: UInt64
        public var bytesOut: UInt64

        public init(bytesIn: UInt64 = 0, bytesOut: UInt64 = 0) {
            self.bytesIn = bytesIn
            self.bytesOut = bytesOut
        }

        /// What came since `earlier`, nothing where a count went back.
        public func since(_ earlier: Totals) -> Totals {
            Totals(bytesIn: bytesIn >= earlier.bytesIn ? bytesIn - earlier.bytesIn : 0,
                   bytesOut: bytesOut >= earlier.bytesOut ? bytesOut - earlier.bytesOut : 0)
        }
    }

    /// What the bytes carried.
    public enum Kind: String, CaseIterable, Sendable {
        /// The control connection: commands, the Core's state and its
        /// readings, over the Core's socket or a control data channel.
        case control
        /// The band's display messages.
        case display
        /// Sound: the band's audio packets in, the microphone's out.
        case audio
        /// The transmit channel beside the microphone.
        case transmit
        /// Media bytes on a channel or track the app has not taken.
        case other
    }

    /// One reading by kind.
    public struct ByKind: Sendable, Equatable {
        private var counts: [Totals]

        public init() {
            counts = Array(repeating: Totals(), count: Kind.allCases.count)
        }

        public subscript(kind: Kind) -> Totals {
            get { counts[Self.index(kind)] }
            set { counts[Self.index(kind)] = newValue }
        }

        /// Every kind together.
        public var sum: Totals {
            counts.reduce(into: Totals()) { total, count in
                total.bytesIn &+= count.bytesIn
                total.bytesOut &+= count.bytesOut
            }
        }

        /// What came since `earlier`, kind by kind.
        public func since(_ earlier: ByKind) -> ByKind {
            var result = ByKind()
            for kind in Kind.allCases {
                result[kind] = self[kind].since(earlier[kind])
            }
            return result
        }

        private static func index(_ kind: Kind) -> Int {
            switch kind {
            case .control:
                return 0
            case .display:
                return 1
            case .audio:
                return 2
            case .transmit:
                return 3
            case .other:
                return 4
            }
        }
    }

    /// The app's one counter.
    public static let shared = TrafficCounter()

    private let totals = OSAllocatedUnfairLock(initialState: ByKind())

    public init() {}

    /// Counts `bytes` taken in, carrying `kind`.
    public func received(_ bytes: Int, as kind: Kind = .other) {
        guard bytes > 0 else {
            return
        }
        totals.withLock { $0[kind].bytesIn &+= UInt64(bytes) }
    }

    /// Counts `bytes` sent out, carrying `kind`.
    public func sent(_ bytes: Int, as kind: Kind = .other) {
        guard bytes > 0 else {
            return
        }
        totals.withLock { $0[kind].bytesOut &+= UInt64(bytes) }
    }

    /// The totals now, every kind together.
    public var reading: Totals {
        totals.withLock { $0.sum }
    }

    /// The totals now, by kind.
    public var readingByKind: ByKind {
        totals.withLock { $0 }
    }
}
