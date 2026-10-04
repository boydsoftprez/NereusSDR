// NereusSDR for iOS: one display endpoint the app asks the Core for, with exactly the subscribe operation's fields
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One display endpoint as the app sizes it (spec section 4.2, R3): the
/// fields of the media control document's `subscribe` and, where the Core
/// offers them, the display extras document's, and nothing else.
/// Endpoints are keyed by slice, since the Core has no pan objects.
/// ``DisplayEndpointRequest`` checks one before it is sent.
public struct DisplaySubscription: Sendable, Equatable {
    /// Which of the stream's two engines the endpoint reads.
    public enum Tier: String, Sendable, Equatable {
        case wide
        case fine
    }

    /// A plane's reduction: exactly `detector`, `averageMode` and
    /// `averageAlpha`. Detector codes: 0 peak, 1 Rosenfell, 2 average,
    /// 3 sample, 4 RMS. Averaging codes: -1 peak hold, 0 none, 1 recursive
    /// linear, 2 time window linear, 3 recursive logarithmic.
    public struct Plane: Sendable, Equatable {
        public var detector: Int
        public var averageMode: Int
        public var averageAlpha: Double

        public init(detector: Int, averageMode: Int, averageAlpha: Double) {
            self.detector = detector
            self.averageMode = averageMode
            self.averageAlpha = averageAlpha
        }
    }

    public var endpointId: UInt32
    public var revision: UInt32
    public var sliceId: Int
    public var tier: Tier
    public var fftSize: Int
    public var windowType: Int
    public var centreHz: Double
    public var spanHz: Double
    public var pixels: Int
    public var fps: Int
    public var framesPerLine: Int
    public var trace: Plane
    public var waterfall: Plane
    public var minDbm: Double
    public var maxDbm: Double
    /// 0 for no wide coverage; otherwise above 1.
    public var wideSpanFactor: Double
    /// Asks for the extended view; sent only when the Core offers it
    /// (`remoteWidebandDisplayVersion` 1 or more). Nil does not ask.
    public var extendedView: Bool?
    /// Asks the Core for display extras (display extras document, section
    /// 2); sent only when the Core offers them (`displayExtrasVersion` 1 or
    /// more at minor 11). Nil, or a request with no field, asks for none.
    public var extras: DisplayExtrasRequest?
    /// The engine's decimation, 1 to 32 (`spectrumGrantVersion` 2, the
    /// link document's `decimation`); sent only when the Core takes it. Nil
    /// asks for none, which the Core reads as 1.
    public var decimation: Int?
    /// The window the Core quantises this endpoint's transmit display to
    /// while keyed, its `txMinDbm` and `txMaxDbm` (both or neither); sent
    /// only on a connection whose `start` declared `txDisplayVersion`. Nil
    /// asks for none, and the transmit display then uses `minDbm`/`maxDbm`.
    public var txWindow: ClosedRange<Double>?
    /// Display duplex (`duplex`): true keeps the receiver on this endpoint
    /// while keyed. Sent only as true, and only on a connection whose
    /// `start` declared `txDisplayVersion` 3; nil or false sends nothing.
    public var duplex: Bool?

    public init(endpointId: UInt32, revision: UInt32, sliceId: Int, tier: Tier, fftSize: Int,
                windowType: Int, centreHz: Double, spanHz: Double, pixels: Int, fps: Int,
                framesPerLine: Int, trace: Plane, waterfall: Plane, minDbm: Double, maxDbm: Double,
                wideSpanFactor: Double, extendedView: Bool? = nil, extras: DisplayExtrasRequest? = nil) {
        self.endpointId = endpointId
        self.revision = revision
        self.sliceId = sliceId
        self.tier = tier
        self.fftSize = fftSize
        self.windowType = windowType
        self.centreHz = centreHz
        self.spanHz = spanHz
        self.pixels = pixels
        self.fps = fps
        self.framesPerLine = framesPerLine
        self.trace = trace
        self.waterfall = waterfall
        self.minDbm = minDbm
        self.maxDbm = maxDbm
        self.wideSpanFactor = wideSpanFactor
        self.extendedView = extendedView
        self.extras = extras
    }
}
