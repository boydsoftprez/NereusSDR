// NereusSDR for iOS: PureSignal's status as the Core sends it in pureSignal's statusJson
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The part of PureSignal's status a phone shows, read from the
/// `pureSignal` object's `statusJson` (link document section 7.1,
/// `PureSignalSessionFacade`): a compact JSON object whose `schema` is 1.
/// Calibration itself stays at the Core; the phone shows whether it is
/// calibrating, correcting and hearing its feedback.
public struct PureSignalStatus: Sendable, Equatable {
    /// PureSignal runs on the Core's radio.
    public var enabled: Bool
    /// The radio is on the air.
    public var mox: Bool
    /// The Core's calibration step, as a number.
    public var engineState: Int
    /// The feedback level the Core measures, 0 to 255.
    public var feedbackLevel: Int
    /// Calibrations that found a solution, and every one tried.
    public var successfulCalibrations: Int
    public var attemptedCalibrations: Int
    /// A correction is being applied to the transmitted signal.
    public var correctionsApplied: Bool

    /// The only `schema` this reads.
    public static let schema = 1
    /// The most text the Core sends.
    public static let longestJson = 8192
    /// The calibration steps during which the Core is calibrating.
    public static let calibratingStates: Set<Int> = [3, 4, 6]
    /// The top of the feedback level.
    public static let feedbackTop = 255

    public init(enabled: Bool = false, mox: Bool = false, engineState: Int = 0, feedbackLevel: Int = 0,
                successfulCalibrations: Int = 0, attemptedCalibrations: Int = 0, correctionsApplied: Bool = false) {
        self.enabled = enabled
        self.mox = mox
        self.engineState = engineState
        self.feedbackLevel = feedbackLevel
        self.successfulCalibrations = successfulCalibrations
        self.attemptedCalibrations = attemptedCalibrations
        self.correctionsApplied = correctionsApplied
    }

    /// The status in `json`, or nil when it is empty, too long, of another
    /// schema, or missing any field this reads.
    public static func parse(_ json: String) -> PureSignalStatus? {
        guard !json.isEmpty, json.utf8.count <= longestJson,
              let object = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [String: Any],
              wholeNumber(object["schema"]) == schema else {
            return nil
        }
        guard let enabled = flag(object["psEnabled"]), let mox = flag(object["mox"]),
              let engineState = wholeNumber(object["engineState"]),
              let feedbackLevel = wholeNumber(object["feedbackLevel"]),
              let successful = wholeNumber(object["successfulCalibrations"]),
              let attempted = wholeNumber(object["attemptedCalibrations"]),
              let correcting = flag(object["correctionsApplied"]) else {
            return nil
        }
        return PureSignalStatus(enabled: enabled, mox: mox, engineState: engineState, feedbackLevel: feedbackLevel,
                                successfulCalibrations: successful, attemptedCalibrations: attempted,
                                correctionsApplied: correcting)
    }

    /// The Core is working out a correction now.
    public var calibrating: Bool {
        Self.calibratingStates.contains(engineState)
    }

    /// The Core hears the radio's output: on the air with some feedback.
    public var hearingFeedback: Bool {
        mox && feedbackLevel > 0
    }

    /// The feedback level as a share of its top, 0 to 1.
    public var feedbackShare: Double {
        min(max(Double(feedbackLevel) / Double(Self.feedbackTop), 0), 1)
    }

    private static func flag(_ value: Any?) -> Bool? {
        // A JSON true or false, not a number standing in for one.
        guard let number = value as? NSNumber, CFGetTypeID(number) == CFBooleanGetTypeID() else {
            return nil
        }
        return number.boolValue
    }

    private static func wholeNumber(_ value: Any?) -> Int? {
        guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID() else {
            return nil
        }
        let double = number.doubleValue
        guard double.isFinite, double == double.rounded(), abs(double) <= Double(Int32.max) else {
            return nil
        }
        return Int(double)
    }
}
