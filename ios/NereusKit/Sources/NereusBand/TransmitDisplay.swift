// NereusSDR for iOS: the band while the Core's radio is keyed on it: the carrier, the keyed view and the high-SWR state
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// What the band needs to know about the Core's transmitter (R-IOS-11,
/// R-IOS-13; the media control document, "Transmit display"): whether the
/// Core's radio is keyed on a slice this band shows, where its carrier is,
/// and the high-SWR state that borders the transmitting band. With it the
/// band keeps the keyed view: centred on the carrier (which follows XIT),
/// at this phone's own span, held within the transmit display's reach
/// either side of the carrier. The values are the desktop remote window's
/// (`src/gui/MoxDisplayController.cpp`, D83); the phone's code is its own.
public struct TransmitDisplay: Equatable, Sendable {
    /// The Core's radio is keyed (MOX, TUNE or two-tone) on a slice this band shows.
    public var keyedHere: Bool
    /// The transmitting slice's carrier in hertz: its frequency, plus its
    /// XIT offset while XIT is on. 0 while not keyed here.
    public var carrierHz: Double
    /// The Core's `txState.highSwr`: the SWR is over its protection limit.
    public var highSwr: Bool
    /// `txState.swrWindBackLatched`: the Core has turned the power down for it.
    public var windBackLatched: Bool

    public init(keyedHere: Bool = false, carrierHz: Double = 0, highSwr: Bool = false, windBackLatched: Bool = false) {
        self.keyedHere = keyedHere
        self.carrierHz = carrierHz
        self.highSwr = highSwr
        self.windBackLatched = windBackLatched
    }

    /// How far either side of the carrier the transmit display reaches: half
    /// the transmitter's 96 kHz rate, as the desktop and the Core hold it.
    public static let halfBasebandHz = 48_000.0
    /// The keyed view's span the first time the phone keys: 4 kHz either
    /// side of the carrier, the desktop's first-key span. A zoom made while
    /// keyed is kept for the next key (``BandDisplaySettings/txViewSpanHz``).
    public static let firstSpanHz = 8_000.0
    /// The narrowest keyed view: the Core keeps its last good view below it.
    public static let smallestSpanHz = 1_000.0
    /// The keyed view's spans.
    public static let spanRange: ClosedRange<Double> = smallestSpanHz...(2 * halfBasebandHz)

    /// The keyed view at `carrierHz` with span `spanHz`, the carrier in the middle.
    public static func firstView(carrierHz: Double, spanHz: Double) -> TuneGestures.View {
        clamped(TuneGestures.View(centerHz: carrierHz, spanHz: spanHz), carrierHz: carrierHz)
    }

    /// `view` held to what the transmit display can fill: no wider than its
    /// reach, no narrower than ``smallestSpanHz``, and moved back inside
    /// the reach either side of the carrier when a pan takes it past.
    public static func clamped(_ view: TuneGestures.View, carrierHz: Double) -> TuneGestures.View {
        let finite = view.spanHz.isFinite && view.spanHz > 0 ? view.spanHz : firstSpanHz
        let span = min(max(finite, spanRange.lowerBound), spanRange.upperBound)
        let centre = view.centerHz.isFinite ? view.centerHz : carrierHz
        var low = centre - span / 2 - carrierHz
        var high = low + span
        if low < -halfBasebandHz {
            low = -halfBasebandHz
            high = low + span
        }
        if high > halfBasebandHz {
            high = halfBasebandHz
            low = high - span
        }
        return TuneGestures.View(centerHz: carrierHz + (low + high) / 2, spanHz: span)
    }

    /// `view` moved by the carrier's move from `oldCarrierHz` to
    /// `newCarrierHz`: the view keeps its place relative to the carrier, as
    /// the desktop's does when XIT or the frequency changes while keyed.
    public static func followed(_ view: TuneGestures.View, from oldCarrierHz: Double,
                                to newCarrierHz: Double) -> TuneGestures.View {
        clamped(TuneGestures.View(centerHz: view.centerHz + (newCarrierHz - oldCarrierHz), spanHz: view.spanHz),
                carrierHz: newCarrierHz)
    }

    /// The window the Core quantises the transmit display to, from this
    /// phone's transmit grid and transmit waterfall levels: the grid (its
    /// top down by its range) widened to hold the levels, each edge rounded
    /// outward to a tenth of a dB and held within -400 to 100 dBm, as the
    /// desktop's remote window sends it.
    public static func dbmWindow(gridTopDbm: Double, gridRangeDb: Double, waterfallLowDbm: Double,
                                 waterfallHighDbm: Double) -> ClosedRange<Double> {
        let limits = -400.0...100.0
        let gridBottom = gridTopDbm - gridRangeDb
        var low = min(gridBottom, waterfallLowDbm)
        var high = max(gridTopDbm, waterfallHighDbm)
        if !low.isFinite {
            low = limits.lowerBound
        }
        if !high.isFinite {
            high = limits.upperBound
        }
        low = min(max((low * 10).rounded(.down) / 10, limits.lowerBound), limits.upperBound)
        high = min(max((high * 10).rounded(.up) / 10, limits.lowerBound), limits.upperBound)
        if high <= low {
            if low > limits.lowerBound {
                low = high - 0.1
            } else {
                high = low + 0.1
            }
        }
        return low...high
    }
}
