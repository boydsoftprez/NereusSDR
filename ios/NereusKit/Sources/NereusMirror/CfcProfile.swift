// NereusSDR for iOS: the CFC profile the Core sends on transmit.cfcProfile, read as sent and edited as the desktop edits it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The CFC profile as the Core sends it (link document, "The CFC band
/// editor", `cfcProfile` 1): compact JSON on `transmit`'s `cfcProfile`.
/// Key order does not matter and unknown keys are ignored; a state the
/// phone does not know reads as `legacy`. A value that does not read as
/// the profile's shape reads as nil, and the editor has nothing to show.
public struct CfcProfile: Equatable, Sendable {
    public enum State: String, Equatable, Sendable {
        /// A profile the operator saved.
        case saved
        /// Values the Core carried over from before profiles were saved whole.
        case legacy
    }

    /// One band: its compression and its post-EQ, each with its Q.
    public struct Band: Equatable, Sendable {
        public var frequencyHz: Double
        public var compressionDb: Double
        public var compressionQ: Double
        public var postEqGainDb: Double
        public var postEqQ: Double

        public init(frequencyHz: Double, compressionDb: Double, compressionQ: Double,
                    postEqGainDb: Double, postEqQ: Double) {
            self.frequencyHz = frequencyHz
            self.compressionDb = compressionDb
            self.compressionQ = compressionQ
            self.postEqGainDb = postEqGainDb
            self.postEqQ = postEqQ
        }
    }

    /// The feature a device declares in its hello.
    public static let featureName = "cfcProfile"
    /// The `transmit` property that carries it.
    public static let propertyName = "cfcProfile"
    /// The verb that changes it, and its arguments.
    public static let setProfileVerb = "cfc.setProfile"
    public static let profileArgumentName = "profileJson"
    public static let revisionArgumentName = "expectedRevision"
    /// The accepted answer's value: the profile the Core now holds.
    public static let returnedProfileName = "profile"
    /// The capability, and the version from which the Core takes the verb.
    public static let capabilityName = "transmitSettingsVersion"
    public static let editableVersion: Int64 = 15
    /// A new band's Q when the band count changes, as the desktop's
    /// editor spreads them (ParametricEqWidget::resetPointsDefault).
    public static let spreadQ = 4.0

    public var state: State
    /// The Core's revision, sent back as `expectedRevision`.
    public var revision: String
    /// "Use Q Factors".
    public var parametric: Bool
    public var minHz: Double
    public var maxHz: Double
    public var precompDb: Double
    public var postEqGainDb: Double
    /// Lowest first, as sent; the first sits at `minHz`, the last at `maxHz`.
    public var bands: [Band]

    public init(state: State = .saved, revision: String = "", parametric: Bool = false, minHz: Double = 0,
                maxHz: Double = 0, precompDb: Double = 0, postEqGainDb: Double = 0, bands: [Band] = []) {
        self.state = state
        self.revision = revision
        self.parametric = parametric
        self.minHz = minHz
        self.maxHz = maxHz
        self.precompDb = precompDb
        self.postEqGainDb = postEqGainDb
        self.bands = bands
    }

    /// Reads the Core's value; nil when it sent none or it does not read.
    public init?(json: String) {
        guard !json.isEmpty, case .object(let object)? = try? LinkJSON.parse(json),
              case .string(let revision)? = object["revision"],
              case .bool(let parametric)? = object["parametric"],
              case .number(let minHz)? = object["minHz"], case .number(let maxHz)? = object["maxHz"],
              case .number(let precompDb)? = object["precompDb"],
              case .number(let postEqGainDb)? = object["postEqGainDb"],
              case .array(let rawBands)? = object["bands"], !rawBands.isEmpty else {
            return nil
        }
        var bands: [Band] = []
        for rawBand in rawBands {
            guard case .object(let band) = rawBand,
                  case .number(let frequency)? = band["frequencyHz"],
                  case .number(let compression)? = band["compressionDb"],
                  case .number(let compressionQ)? = band["compressionQ"],
                  case .number(let gain)? = band["postEqGainDb"],
                  case .number(let postEqQ)? = band["postEqQ"] else {
                return nil
            }
            bands.append(Band(frequencyHz: frequency, compressionDb: compression, compressionQ: compressionQ,
                              postEqGainDb: gain, postEqQ: postEqQ))
        }
        var state = State.legacy
        if case .string(let text)? = object["state"], let known = State(rawValue: text) {
            state = known
        }
        self.init(state: state, revision: revision, parametric: parametric, minHz: minHz, maxHz: maxHz,
                  precompDb: precompDb, postEqGainDb: postEqGainDb, bands: bands)
    }

    /// The profile as `cfc.setProfile` takes it: the edited values, without
    /// `state` and `revision`, which are the Core's to say.
    public var profileJson: String {
        let items: [LinkJSON] = bands.map { band in
            .object(["frequencyHz": .number(band.frequencyHz), "compressionDb": .number(band.compressionDb),
                     "compressionQ": .number(band.compressionQ), "postEqGainDb": .number(band.postEqGainDb),
                     "postEqQ": .number(band.postEqQ)])
        }
        return LinkJSON.object(["parametric": .bool(parametric), "minHz": .number(minHz), "maxHz": .number(maxHz),
                                "precompDb": .number(precompDb), "postEqGainDb": .number(postEqGainDb),
                                "bands": .array(items)]).compactText
    }

    /// The same edit, the values compared without the Core's state and revision.
    public func sameValues(as other: CfcProfile) -> Bool {
        parametric == other.parametric && minHz == other.minHz && maxHz == other.maxHz
            && precompDb == other.precompDb && postEqGainDb == other.postEqGainDb && bands == other.bands
    }

    /// `count` bands spread evenly from Low to High, flat, each Q at 4, as
    /// the desktop's editor does when the band count changes.
    public func withBandCount(_ count: Int) -> CfcProfile {
        var result = self
        let total = max(count, 2)
        let span = maxHz - minHz > 0 ? maxHz - minHz : 1
        result.bands = (0..<total).map { index in
            let frequency = minHz + Double(index) / Double(total - 1) * span
            return Band(frequencyHz: frequency, compressionDb: 0, compressionQ: Self.spreadQ,
                        postEqGainDb: 0, postEqQ: Self.spreadQ)
        }
        result.pinEnds()
        return result
    }

    /// Low and High moved: every band keeps its place between them, and
    /// the end bands sit on the new ends, as the desktop's editor does.
    public func withRange(minHz newMin: Double, maxHz newMax: Double) -> CfcProfile {
        var result = self
        let oldSpan = maxHz - minHz > 0 ? maxHz - minHz : 1
        let newSpan = newMax - newMin > 0 ? newMax - newMin : 1
        result.minHz = newMin
        result.maxHz = newMax
        for index in result.bands.indices {
            let place = min(max((bands[index].frequencyHz - minHz) / oldSpan, 0), 1)
            result.bands[index].frequencyHz = newMin + place * newSpan
        }
        result.pinEnds()
        return result
    }

    private mutating func pinEnds() {
        if let first = bands.indices.first { bands[first].frequencyHz = minHz }
        if bands.count > 1, let last = bands.indices.last { bands[last].frequencyHz = maxHz }
    }
}
