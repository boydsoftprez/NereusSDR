// NereusSDR for iOS: a keying verb that has gone to the Core and waits for its answer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// A keying verb whose copies have gone to the Core, in order after every
/// one sent before it; ``answer()`` waits for the Core's answer.
public struct TransmitPending: Sendable {
    private let wait: @Sendable () async -> TransmitAnswer

    public init(_ wait: @escaping @Sendable () async -> TransmitAnswer) {
        self.wait = wait
    }

    /// The Core's answer, or `.noAnswer` when none came.
    public func answer() async -> TransmitAnswer {
        await wait()
    }
}
