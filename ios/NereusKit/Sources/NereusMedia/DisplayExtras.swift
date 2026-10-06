// NereusSDR for iOS: one decoded NSDX datagram, the Core's display extras for one frame
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One accepted NSDX v1 datagram (display extras document, section 3): the
/// results of the display computations the Core ran for one endpoint's
/// frame, paired with that frame by its endpoint, context generation and
/// encoder sequence. Only the sections the datagram carried are present.
///
/// Calibration and normalise are already in every dBm the frame and the
/// datagram carry: the app adds nothing.
public struct DisplayExtras: Equatable, Sendable {
    /// One peak marker.
    public struct PeakBlob: Equatable, Sendable {
        /// The trace sample the peak is at, below the context's trace
        /// length; not a pixel of any view.
        public let traceSample: Int
        public let dbm: Float

        public init(traceSample: Int, dbm: Float) {
            self.traceSample = traceSample
            self.dbm = dbm
        }
    }

    /// The waterfall's levels in force for the latest waterfall line.
    public struct WaterfallLevels: Equatable, Sendable {
        public let lowDbm: Float
        public let highDbm: Float

        public init(lowDbm: Float, highDbm: Float) {
            self.lowDbm = lowDbm
            self.highDbm = highDbm
        }
    }

    public let endpointId: UInt32
    public let contextGeneration: UInt32
    /// The encoder sequence of the NSDC frame this goes beside.
    public let encoderSequence: UInt32
    /// Strongest first.
    public let peakBlobs: [PeakBlob]?
    /// The active peak hold, the trace's length; a sample the hold has not
    /// reached yet is the context's `minDbm`.
    public let peakHoldDbm: [Float]?
    /// Where the noise-floor line is drawn: the smoothed estimate plus
    /// `shiftDb`. Not the `noise-floor` operation's full-source floor,
    /// which feeds Clarity.
    public let noiseFloorDbm: Float?
    public let waterfallLevels: WaterfallLevels?
    /// Whether the noise floor is in fast attack (section 0x10, version 4);
    /// nil when the datagram did not say.
    public let noiseFloorFastAttack: Bool?

    public init(endpointId: UInt32, contextGeneration: UInt32, encoderSequence: UInt32,
                peakBlobs: [PeakBlob]? = nil, peakHoldDbm: [Float]? = nil, noiseFloorDbm: Float? = nil,
                waterfallLevels: WaterfallLevels? = nil, noiseFloorFastAttack: Bool? = nil) {
        self.noiseFloorFastAttack = noiseFloorFastAttack
        self.endpointId = endpointId
        self.contextGeneration = contextGeneration
        self.encoderSequence = encoderSequence
        self.peakBlobs = peakBlobs
        self.peakHoldDbm = peakHoldDbm
        self.noiseFloorDbm = noiseFloorDbm
        self.waterfallLevels = waterfallLevels
    }
}
