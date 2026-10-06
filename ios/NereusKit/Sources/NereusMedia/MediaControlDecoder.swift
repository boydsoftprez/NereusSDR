// NereusSDR for iOS: reads the Core's media control operations in exactly the shapes the negotiated capabilities give
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// Decodes the Core's media control operations (the media control document
/// and the link document's section 11). Each decoder accepts exactly one
/// key set, every key present with a value in range, and gives nil for
/// anything else; which key set is chosen by the caller from the
/// negotiated gates. Session identity (the connection ID, revisions,
/// generations) is the caller's to check.
public enum MediaControlDecoder {
    static let maxUInt32 = Double(UInt32.max)
    /// The largest whole number JSON carries exactly (2^53 - 1).
    static let maxJsonWhole = 9_007_199_254_740_991.0
    /// `kDisplaySenderMessagesPerSecond`.
    static let maxMessagesPerSecond = 200.0
    /// The Core's reasons are read up to this many characters.
    static let maxReasonCharacters = 512
    /// `FFTEngine::maximumFftSize()`.
    static let maxGrantedFftSize = 262_144.0
    static let maxPixels = 4096.0
    /// `SpectrumEndpoint::kMaxWideSamples`.
    static let maxWideSamples = 768.0
    static let maxFramesPerLine = 10_000.0
    static let dbmRange = -400.0...100.0
    /// The Opus bandwidths the Core's encoder can report, in Hz.
    static let opusBandwidths: Set<Int> = [4000, 6000, 8000, 12000, 20000]

    // MARK: Signalling

    public static func description(_ payload: [String: LinkJSON]) -> MediaControlEvent.Description? {
        guard payload.count == 4, isOp(payload, "description"), string(payload["connectionId"]) != nil,
              let sdp = string(payload["sdp"]), let type = string(payload["type"]), type == "offer" else {
            return nil
        }
        return MediaControlEvent.Description(sdp: sdp, type: type)
    }

    public static func candidate(_ payload: [String: LinkJSON]) -> MediaControlEvent.Candidate? {
        guard payload.count == 4, isOp(payload, "candidate"), string(payload["connectionId"]) != nil,
              let candidate = string(payload["candidate"]), let mid = string(payload["mid"]) else {
            return nil
        }
        return MediaControlEvent.Candidate(candidate: candidate, mid: mid)
    }

    // MARK: Display

    /// `context`: 19 keys, plus `wideband` exactly when `wideband` is true,
    /// plus the five grant keys exactly when `grant` is true.
    public static func context(_ payload: [String: LinkJSON], wideband expectsWideband: Bool,
                               grant expectsGrant: Bool,
                               transmit expectsTransmit: Bool = false) -> MediaControlEvent.DisplayContext? {
        let expected = 19 + (expectsWideband ? 1 : 0) + (expectsGrant ? 5 : 0) + (expectsTransmit ? 1 : 0)
        guard payload.count == expected, isOp(payload, "context"), string(payload["connectionId"]) != nil,
              let endpointId = whole(payload["endpointId"], 1...maxUInt32),
              let revision = whole(payload["revision"], 1...maxUInt32),
              let generation = whole(payload["contextGeneration"], 1...maxUInt32),
              let stream = whole(payload["sourceStream"], 0...255),
              let sourceCentre = number(payload["sourceCentreHz"], 0...1.0e12),
              let rate = number(payload["sampleRateHz"], 1...1.0e8) else {
            return nil
        }
        var wideband: MediaControlEvent.Wideband?
        if expectsWideband {
            guard case .object(let object)? = payload["wideband"], let decoded = Self.wideband(object) else {
                return nil
            }
            wideband = decoded
        }
        let maxSpan = MediaControlEvent.DisplayContext.spanCeilingHz(sampleRateHz: rate, wideband: wideband)
        guard let centre = number(payload["centreHz"], 0...1.0e12),
              let span = number(payload["spanHz"], 0.000001...maxSpan),
              let wideCentre = number(payload["wideCentreHz"], 0...1.0e12),
              let wideSpan = number(payload["wideSpanHz"], 0...rate),
              let trace = whole(payload["traceSamples"], 1...maxPixels),
              let waterfall = whole(payload["waterfallSamples"], 1...maxPixels),
              let wide = whole(payload["wideSamples"], 0...maxWideSamples),
              let minDbm = number(payload["minDbm"], dbmRange),
              let maxDbm = number(payload["maxDbm"], dbmRange), minDbm < maxDbm,
              let fps = whole(payload["fps"], 1...60),
              let lines = whole(payload["framesPerLine"], 1...maxFramesPerLine),
              // A wide row exists exactly when it has coverage.
              (wide == 0) == (wideSpan == 0) else {
            return nil
        }
        var grant: MediaControlEvent.Grant?
        if expectsGrant {
            guard let tierName = string(payload["grantedTier"]),
                  let tier = DisplaySubscription.Tier(rawValue: tierName),
                  let limitName = string(payload["limit"]),
                  let limit = MediaControlEvent.Grant.Limit(rawValue: limitName),
                  let fftSize = whole(payload["grantedFftSize"], 1...maxGrantedFftSize),
                  let requested = whole(payload["requestedPixels"], 1...maxPixels),
                  let granted = whole(payload["grantedPixels"], 1...maxPixels), granted <= requested else {
                return nil
            }
            grant = MediaControlEvent.Grant(grantedFftSize: Int(fftSize), grantedTier: tier,
                                            requestedPixels: Int(requested), grantedPixels: Int(granted),
                                            limit: limit)
        }
        // A connection that declared the transmit display gets `transmit` in
        // every context (the media control document, "Context").
        var transmit: Bool?
        if expectsTransmit {
            guard case .bool(let value)? = payload["transmit"] else {
                return nil
            }
            transmit = value
        }
        return MediaControlEvent.DisplayContext(
            endpointId: UInt32(endpointId), revision: UInt32(revision), contextGeneration: UInt32(generation),
            sourceStream: Int(stream), sourceCentreHz: sourceCentre, sampleRateHz: rate, centreHz: centre,
            spanHz: span, wideCentreHz: wideCentre, wideSpanHz: wideSpan, traceSamples: Int(trace),
            waterfallSamples: Int(waterfall), wideSamples: Int(wide), minDbm: minDbm, maxDbm: maxDbm,
            fps: Int(fps), framesPerLine: Int(lines), wideband: wideband, grant: grant, transmit: transmit)
    }

    /// A context's `wideband`: `{version, available, active}` while not
    /// available, or those with the eight fields describing the ADC.
    static func wideband(_ object: [String: LinkJSON]) -> MediaControlEvent.Wideband? {
        guard whole(object["version"], 1...1) != nil,
              case .bool(let available)? = object["available"],
              case .bool(let active)? = object["active"] else {
            return nil
        }
        guard available else {
            guard object.count == 3, !active else {
                return nil
            }
            return MediaControlEvent.Wideband(available: false, active: false)
        }
        guard object.count == 11,
              let adc = whole(object["physicalAdcIndex"], 0...1),
              let chain = whole(object["filterChainIndex"], 0...1),
              let generation = whole(object["sourceGeneration"], 0...maxUInt32),
              let adcRate = number(object["adcRateHz"], 0.000001...Double.greatestFiniteMagnitude),
              let low = number(object["lowHz"], 0...0), low == 0,
              let high = number(object["highHz"], 0...Double.greatestFiniteMagnitude), high == adcRate / 2,
              string(object["geometryRateBasis"]) == "thetisLocalReference",
              string(object["levelReference"]) == "localWingRelativeWithStationRxOffset",
              active == (generation != 0) else {
            return nil
        }
        return MediaControlEvent.Wideband(available: true, active: active, physicalAdcIndex: Int(adc),
                                          filterChainIndex: Int(chain), sourceGeneration: UInt32(generation),
                                          adcRateHz: adcRate)
    }

    /// `rejected`: endpoint and revision 0 for the whole peer.
    public static func rejected(_ payload: [String: LinkJSON]) -> MediaControlEvent.Rejection? {
        guard payload.count == 5, isOp(payload, "rejected"), string(payload["connectionId"]) != nil,
              let endpointId = whole(payload["endpointId"], 0...maxUInt32),
              let revision = whole(payload["revision"], 0...maxUInt32),
              let reason = string(payload["reason"]) else {
            return nil
        }
        return MediaControlEvent.Rejection(endpointId: UInt32(endpointId), revision: UInt32(revision),
                                           reason: String(reason.prefix(maxReasonCharacters)))
    }

    public static func noiseFloor(_ payload: [String: LinkJSON]) -> MediaControlEvent.NoiseFloor? {
        guard payload.count == 6, isOp(payload, "noise-floor"), string(payload["connectionId"]) != nil,
              let endpointId = whole(payload["endpointId"], 1...maxUInt32),
              let revision = whole(payload["revision"], 1...maxUInt32),
              let generation = whole(payload["contextGeneration"], 1...maxUInt32),
              let floor = number(payload["floorDbm"], dbmRange) else {
            return nil
        }
        return MediaControlEvent.NoiseFloor(endpointId: UInt32(endpointId), revision: UInt32(revision),
                                            contextGeneration: UInt32(generation), floorDbm: floor)
    }

    /// `allocation-result`: exactly 11 keys, and a retained charge that is
    /// all zero exactly when the Core holds no revision.
    public static func allocationResult(_ payload: [String: LinkJSON]) -> MediaControlEvent.AllocationResult? {
        guard payload.count == 11, isOp(payload, "allocation-result"), string(payload["connectionId"]) != nil,
              case .bool(let accepted)? = payload["accepted"],
              let reason = string(payload["reason"]),
              let endpointId = whole(payload["endpointId"], 1...maxUInt32),
              let revision = whole(payload["revision"], 1...maxUInt32),
              let budgetGeneration = whole(payload["budgetGeneration"], 1...maxUInt32),
              let acceptedRevision = whole(payload["acceptedRevision"], 0...maxUInt32),
              let bytes = whole(payload["applicationBytesPerSecond"], 0...maxJsonWhole),
              let units = whole(payload["spectrumSampleUnitsPerSecond"], 0...maxJsonWhole),
              let messages = whole(payload["messagesPerSecond"], 0...maxMessagesPerSecond) else {
            return nil
        }
        let held = acceptedRevision != 0
        guard held ? (bytes > 0 && units > 0 && messages > 0) : (bytes == 0 && units == 0 && messages == 0) else {
            return nil
        }
        return MediaControlEvent.AllocationResult(
            endpointId: UInt32(endpointId), revision: UInt32(revision), accepted: accepted,
            reason: String(reason.prefix(maxReasonCharacters)), budgetGeneration: UInt32(budgetGeneration),
            acceptedRevision: UInt32(acceptedRevision), applicationBytesPerSecond: UInt64(bytes),
            spectrumSampleUnitsPerSecond: UInt64(units), messagesPerSecond: UInt32(messages))
    }

    // MARK: Audio

    /// `audio-context`: the eight keys; with `detail`, one more (`encoder`
    /// when enabled, `reason` when not); with `profile` (only beside
    /// `detail`), `profile` and, beside profile opus, perhaps `profileRefusal`.
    public static func audioContext(_ payload: [String: LinkJSON], detail: Bool,
                                    profile expectsProfile: Bool,
                                    bitrate expectsBitrate: Bool = false) -> MediaControlEvent.AudioContext? {
        let profiled = expectsProfile && detail
        // `opusBitrateRefusal` only answers a connection that sent
        // `opusBitrate`, which goes only beside `profile`.
        let bitrateRefused = profiled && expectsBitrate && payload["opusBitrateRefusal"] != nil
        let profileKeys = profiled
            ? 1 + (payload["profileRefusal"] != nil ? 1 : 0) + (bitrateRefused ? 1 : 0) : 0
        guard payload.count == (detail ? 9 : 8) + profileKeys, isOp(payload, "audio-context"),
              string(payload["connectionId"]) != nil,
              case .bool(let enabled)? = payload["enabled"],
              let revision = whole(payload["revision"], 1...maxUInt32),
              let generation = whole(payload["generation"], 1...maxUInt32),
              let ssrc = whole(payload["ssrc"], 1...maxUInt32),
              let firstSequence = whole(payload["firstSequence"], 0...Double(UInt16.max)),
              let firstTimestamp = whole(payload["firstTimestamp"], 0...maxUInt32) else {
            return nil
        }
        var context = MediaControlEvent.AudioContext(
            revision: UInt32(revision), enabled: enabled,
            anchor: AudioStreamAnchor(generation: UInt32(generation), ssrc: UInt32(ssrc),
                                      firstSequence: UInt16(firstSequence), firstTimestamp: UInt32(firstTimestamp)),
            encoder: nil, lossless: false, offReason: nil, profile: nil, profileRefusal: nil)
        guard detail else {
            return context
        }
        if profiled {
            guard let name = string(payload["profile"]),
                  let profile = MediaControlEvent.AudioContext.Profile(rawValue: name) else {
                return nil
            }
            context.profile = profile
            if payload["profileRefusal"] != nil {
                guard profile == .opus, let refusalName = string(payload["profileRefusal"]),
                      let refusal = MediaControlEvent.AudioContext.ProfileRefusal(rawValue: refusalName) else {
                    return nil
                }
                context.profileRefusal = refusal
            }
            if bitrateRefused {
                // Plain words, never empty, at most 512 characters as
                // the Core counts them.
                guard let words = string(payload["opusBitrateRefusal"]), !words.isEmpty,
                      words.utf16.count <= maxReasonCharacters else {
                    return nil
                }
                context.opusBitrateRefusal = words
            }
        }
        if enabled {
            guard payload["reason"] == nil, case .object(let encoder)? = payload["encoder"] else {
                return nil
            }
            if context.profile == .lossless {
                guard losslessEncoder(encoder) else {
                    return nil
                }
                context.lossless = true
                context.anchor.format = .l16
            } else {
                guard let opus = opusEncoder(encoder) else {
                    return nil
                }
                context.encoder = opus
            }
        } else {
            guard payload["encoder"] == nil, let name = string(payload["reason"]),
                  let reason = MediaControlEvent.AudioContext.OffReason(rawValue: name) else {
                return nil
            }
            context.offReason = reason
        }
        return context
    }

    /// The Opus encoder the app plays: exactly six keys, 48000 Hz, two
    /// channels, 1920-sample frames, 6000 to 510000 bit/s and one of the
    /// five Opus bandwidths.
    static func opusEncoder(_ object: [String: LinkJSON]) -> MediaControlEvent.AudioContext.OpusEncoder? {
        guard object.count == 6, string(object["codec"]) == "opus",
              let rate = whole(object["sampleRate"], 48000...48000),
              let channels = whole(object["channels"], 2...2),
              let frames = whole(object["frameSamples"], 1920...1920),
              let bitrate = whole(object["targetBitrate"], 6000...510_000),
              let bandwidth = whole(object["audioBandwidthHz"], 4000...20000),
              opusBandwidths.contains(Int(bandwidth)) else {
            return nil
        }
        return MediaControlEvent.AudioContext.OpusEncoder(sampleRate: Int(rate), channels: Int(channels),
                                                          frameSamples: Int(frames), targetBitrate: Int(bitrate),
                                                          audioBandwidthHz: Int(bandwidth))
    }

    /// The one lossless shape the Core sends: L16, 48000 Hz, two channels,
    /// 192-sample frames, 16 bits, payload type 96.
    static func losslessEncoder(_ object: [String: LinkJSON]) -> Bool {
        object.count == 6 && string(object["codec"]) == "l16"
            && whole(object["sampleRate"], 48000...48000) != nil
            && whole(object["channels"], 2...2) != nil
            && whole(object["frameSamples"], 192...192) != nil
            && whole(object["bitsPerSample"], 16...16) != nil
            && whole(object["payloadType"], 96...96) != nil
    }

    // MARK: Values

    static func isOp(_ payload: [String: LinkJSON], _ op: String) -> Bool {
        string(payload["op"]) == op
    }

    static func string(_ value: LinkJSON?) -> String? {
        if case .string(let text)? = value {
            return text
        }
        return nil
    }

    /// A finite JSON number within `range`.
    static func number(_ value: LinkJSON?, _ range: ClosedRange<Double>) -> Double? {
        guard case .number(let number)? = value, number.isFinite, range.contains(number) else {
            return nil
        }
        return number
    }

    /// A whole JSON number within `range`.
    static func whole(_ value: LinkJSON?, _ range: ClosedRange<Double>) -> Double? {
        guard let number = number(value, range), number.rounded(.towardZero) == number else {
            return nil
        }
        return number
    }
}
