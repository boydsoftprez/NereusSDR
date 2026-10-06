// NereusSDR for iOS: a stand-in for Bonjour in tests: the test announces the Cores the phone finds
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// Finds what the test announces, in place of ``StationBrowser``. It counts
/// starts and stops, and reports an announcement only while started, as the
/// real browser does.
public actor FakeStationBrowser: StationBrowsing {
    private var onUpdate: (@Sendable (StationBrowser.Update) -> Void)?
    private var current = StationBrowser.Update()
    public private(set) var starts = 0
    public private(set) var stops = 0

    public init() {}

    public var isBrowsing: Bool { onUpdate != nil }

    public func start(_ onUpdate: @escaping @Sendable (StationBrowser.Update) -> Void) {
        starts += 1
        self.onUpdate = onUpdate
        onUpdate(current)
    }

    public func stop() {
        stops += 1
        onUpdate = nil
    }

    /// What the phone finds from now on.
    public func announce(_ stations: [FoundStation], localNetworkDenied: Bool = false) {
        current = StationBrowser.Update(stations: stations, localNetworkDenied: localNetworkDenied)
        onUpdate?(current)
    }
}
