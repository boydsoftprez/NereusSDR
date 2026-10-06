// NereusSDR for iOS: local arrival and session identity for a decoded Core sample
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// A successfully decoded Core sample and when this phone received it.
/// `observedAtMilliseconds` uses the phone's monotonic link clock; the Core's
/// `sampledElapsedMs` has a different origin and must not be subtracted from it.
public struct StationTelemetryReceipt: Equatable, Sendable {
    public let metrics: StationMetrics
    public let observedAtMilliseconds: Int64
    public let snapshotIdentity: UInt64

    public init(metrics: StationMetrics, observedAtMilliseconds: Int64, snapshotIdentity: UInt64) {
        self.metrics = metrics
        self.observedAtMilliseconds = observedAtMilliseconds
        self.snapshotIdentity = snapshotIdentity
    }
}
