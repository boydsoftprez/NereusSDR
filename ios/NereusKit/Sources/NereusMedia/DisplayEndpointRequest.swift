// NereusSDR for iOS: checks a display endpoint request against the media control document and writes its operation
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// The display endpoint operations the app sends (`subscribe`,
/// `unsubscribe`, `keyframe`), written in exactly the media control
/// document's shapes, and the checks a ``DisplaySubscription`` passes
/// before it is sent. A request the Core would refuse never leaves.
public enum DisplayEndpointRequest {
    /// Up to eight endpoints are admitted.
    public static let maximumEndpoints = 8
    public static let pixelRange = 1...4096
    public static let fpsRange = 1...60
    /// `kMaxFramesPerLine`.
    public static let framesPerLineRange = 1...10_000
    public static let minimumFftSize = 1024
    /// The quantisation window's limits (`kMinDbmLimit`, `kMaxDbmLimit`).
    public static let dbmRange = -400.0...100.0
    /// SpectrumDetectorMode's codes.
    public static let detectorRange = 0...4
    /// SpectrumAvenger's codes.
    public static let averageModeRange = -1...3
    /// `decimation`'s range.
    public static let decimationRange = 1...32

    /// Why a request was not sent. For the log and for the app's own code,
    /// never for the operator.
    public enum Invalid: Error, Equatable, Sendable {
        case endpointIdZero
        case revisionZero
        /// A revision at or before the endpoint's last one.
        case staleRevision
        case sliceId
        case fftSize
        case windowType
        /// The centre or span is not finite, or the span is not positive.
        case frequency
        case pixels
        case fps
        case framesPerLine
        case plane
        /// The dBm window is not finite, not within -400 to 100, or its
        /// minimum is not below its maximum.
        case dbmWindow
        case wideSpanFactor
        /// A ninth endpoint.
        case tooManyEndpoints
        /// The extended view asked of a Core that does not offer it.
        case extendedViewUnavailable
        /// Decimation asked of a Core that does not take it, or outside 1 to 32.
        case decimation
        /// Display extras asked of a Core that does not offer them.
        case displayExtrasUnavailable
        /// A display extras field outside its range.
        case displayExtras(DisplayExtrasRequest.Invalid)
        /// A transmit window asked on a connection that did not declare the
        /// transmit display, or display duplex on one that did not declare 3.
        case txDisplayUnavailable
        /// The transmit window is not finite, not within -400 to 100, or
        /// its minimum is not below its maximum.
        case txWindow
    }

    /// Checks `subscription` on its own (not the endpoint count or its
    /// revision order, which need the client's state).
    public static func validate(_ subscription: DisplaySubscription, gates: MediaFeatureGates) throws {
        let s = subscription
        guard s.endpointId != 0 else {
            throw Invalid.endpointIdZero
        }
        guard s.revision != 0 else {
            throw Invalid.revisionZero
        }
        guard s.sliceId >= 0, s.sliceId <= Int(Int32.max) else {
            throw Invalid.sliceId
        }
        guard s.fftSize >= minimumFftSize, s.fftSize <= Int(Int32.max), s.fftSize & (s.fftSize - 1) == 0 else {
            throw Invalid.fftSize
        }
        guard s.windowType >= 0, s.windowType <= Int(Int32.max) else {
            throw Invalid.windowType
        }
        let halfSpan = s.spanHz / 2
        guard s.centreHz.isFinite, s.spanHz.isFinite, s.spanHz > 0,
              (s.centreHz - halfSpan).isFinite, (s.centreHz + halfSpan).isFinite else {
            throw Invalid.frequency
        }
        guard pixelRange.contains(s.pixels) else {
            throw Invalid.pixels
        }
        guard fpsRange.contains(s.fps) else {
            throw Invalid.fps
        }
        guard framesPerLineRange.contains(s.framesPerLine) else {
            throw Invalid.framesPerLine
        }
        for plane in [s.trace, s.waterfall] {
            guard detectorRange.contains(plane.detector), averageModeRange.contains(plane.averageMode),
                  plane.averageAlpha.isFinite, (0.0...1.0).contains(plane.averageAlpha) else {
                throw Invalid.plane
            }
        }
        guard s.minDbm.isFinite, s.maxDbm.isFinite, dbmRange.contains(s.minDbm), dbmRange.contains(s.maxDbm),
              s.minDbm < s.maxDbm else {
            throw Invalid.dbmWindow
        }
        guard s.wideSpanFactor.isFinite, s.wideSpanFactor == 0 || s.wideSpanFactor > 1,
              (s.wideSpanFactor * s.spanHz).isFinite else {
            throw Invalid.wideSpanFactor
        }
        if s.extendedView == true && !gates.wideband {
            throw Invalid.extendedViewUnavailable
        }
        if let decimation = s.decimation, decimation != 1 {
            guard gates.decimation, decimationRange.contains(decimation) else {
                throw Invalid.decimation
            }
        }
        if let window = s.txWindow {
            guard gates.txDisplay else {
                throw Invalid.txDisplayUnavailable
            }
            guard window.lowerBound.isFinite, window.upperBound.isFinite, dbmRange.contains(window.lowerBound),
                  dbmRange.contains(window.upperBound), window.lowerBound < window.upperBound else {
                throw Invalid.txWindow
            }
        }
        if s.duplex == true && !gates.displayDuplex {
            throw Invalid.txDisplayUnavailable
        }
        if let extras = s.extras, !extras.isEmpty {
            // An older Core refuses the whole subscribe as one it cannot read.
            guard gates.displayExtras else {
                throw Invalid.displayExtrasUnavailable
            }
            do {
                try extras.validate()
            } catch let invalid as DisplayExtrasRequest.Invalid {
                throw Invalid.displayExtras(invalid)
            }
        }
    }

    /// The `subscribe` operation: the document's 18 keys, plus
    /// `extendedView` when the Core offers it and the subscription asks, the
    /// display extras fields asked for while the Core offers them, and the
    /// transmit window and `duplex` where the connection declared them.
    public static func subscribe(_ s: DisplaySubscription, connectionId: String,
                                 gates: MediaFeatureGates) -> [String: LinkJSON] {
        var payload: [String: LinkJSON] = [
            "op": .string("subscribe"),
            "connectionId": .string(connectionId),
            "endpointId": .number(Double(s.endpointId)),
            "revision": .number(Double(s.revision)),
            "sliceId": .number(Double(s.sliceId)),
            "tier": .string(s.tier.rawValue),
            "fftSize": .number(Double(s.fftSize)),
            "windowType": .number(Double(s.windowType)),
            "centreHz": .number(s.centreHz),
            "spanHz": .number(s.spanHz),
            "pixels": .number(Double(s.pixels)),
            "fps": .number(Double(s.fps)),
            "framesPerLine": .number(Double(s.framesPerLine)),
            "trace": plane(s.trace),
            "waterfall": plane(s.waterfall),
            "minDbm": .number(s.minDbm),
            "maxDbm": .number(s.maxDbm),
            "wideSpanFactor": .number(s.wideSpanFactor),
        ]
        if gates.wideband, let extendedView = s.extendedView {
            payload["extendedView"] = .bool(extendedView)
        }
        if gates.decimation, let decimation = s.decimation {
            payload["decimation"] = .number(Double(decimation))
        }
        if gates.displayExtras, var extras = s.extras {
            if !gates.peakHoldOnTx {
                // An older Core refuses the extra member.
                extras.activePeakHold?.onTx = nil
            }
            if !gates.noiseFloorFastAttack {
                // Only a Core that sent displayExtrasVersion 4 takes it.
                extras.noiseFloor?.fastAttack = nil
            }
            payload.merge(extras.subscribeFields) { current, _ in current }
        }
        // The transmit window, both edges, only where the start declared
        // the transmit display; `duplex` only as true, only at 3.
        if gates.txDisplay, let window = s.txWindow {
            payload["txMinDbm"] = .number(window.lowerBound)
            payload["txMaxDbm"] = .number(window.upperBound)
        }
        if gates.displayDuplex, s.duplex == true {
            payload["duplex"] = .bool(true)
        }
        return payload
    }

    /// The `unsubscribe` operation: `revision` exactly on the display budget wire.
    public static func unsubscribe(endpointId: UInt32, revision: UInt32, connectionId: String,
                                   gates: MediaFeatureGates) -> [String: LinkJSON] {
        var payload: [String: LinkJSON] = [
            "op": .string("unsubscribe"),
            "connectionId": .string(connectionId),
            "endpointId": .number(Double(endpointId)),
        ]
        if gates.displayBudget {
            payload["revision"] = .number(Double(revision))
        }
        return payload
    }

    /// The `keyframe` operation for the endpoint's current context.
    public static func keyframe(endpointId: UInt32, contextGeneration: UInt32,
                                connectionId: String) -> [String: LinkJSON] {
        [
            "op": .string("keyframe"),
            "connectionId": .string(connectionId),
            "endpointId": .number(Double(endpointId)),
            "contextGeneration": .number(Double(contextGeneration)),
        ]
    }

    /// The `clarity-retune` operation: the Core re-estimates the endpoint's
    /// noise floor for its Clarity now.
    public static func clarityRetune(endpointId: UInt32, connectionId: String) -> [String: LinkJSON] {
        [
            "op": .string("clarity-retune"),
            "connectionId": .string(connectionId),
            "endpointId": .number(Double(endpointId)),
        ]
    }

    private static func plane(_ plane: DisplaySubscription.Plane) -> LinkJSON {
        .object([
            "detector": .number(Double(plane.detector)),
            "averageMode": .number(Double(plane.averageMode)),
            "averageAlpha": .number(plane.averageAlpha),
        ])
    }
}
