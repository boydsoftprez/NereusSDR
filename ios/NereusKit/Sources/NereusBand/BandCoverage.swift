// NereusSDR for iOS: the frequencies one of the Core's frames covers, so it can be drawn into a view that has moved
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The frequencies a frame or a waterfall line covers: its context's
/// centre and span (D74). While the band's view moves ahead of the Core (a
/// pan or a zoom the Core has not answered yet), each frame and line is
/// drawn where its own frequencies fall in the view, as the desktop draws
/// its bins into a moved window; the view's edges it does not reach stay
/// empty until the Core's frames for them arrive.
public struct BandCoverage: Equatable, Sendable {
    public var centerHz: Double
    public var spanHz: Double

    public init(centerHz: Double, spanHz: Double) {
        self.centerHz = centerHz
        self.spanHz = spanHz
    }

    public var lowHz: Double { centerHz - spanHz / 2 }
    public var highHz: Double { centerHz + spanHz / 2 }

    /// The frequency at the centre of sample `index` of `samples`.
    public func hz(forSample index: Int, samples: Int) -> Double {
        guard samples > 0 else {
            return centerHz
        }
        return lowHz + (Double(index) + 0.5) / Double(samples) * spanHz
    }
}
