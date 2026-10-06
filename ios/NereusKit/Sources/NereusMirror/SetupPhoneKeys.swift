// NereusSDR for iOS: the phone's own settings a Core-described Setup control names by its phone key
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine

/// The phone's typed local stores behind `binding.phone` (the Display pan
/// settings, the colours, the S-meter's menu). A key the phone does not
/// keep reads nil and cannot be set, so its control stays visible and
/// disabled. Nothing here is sent to the Core.
@MainActor
public protocol SetupPhoneKeys: AnyObject {
    /// The kept value for `key`, or nil when this phone has no such setting.
    func value(forPhoneKey key: String) -> SetupValue?
    /// Keeps `value` for `key`; false when the phone has no such setting or
    /// the value does not fit it. A refused value changes nothing.
    func set(_ value: SetupValue, forPhoneKey key: String) -> Bool
    /// Fires whenever any kept value may have changed.
    var changes: AnyPublisher<Void, Never> { get }
    /// Whether the phone can run the V12 button `action` now (its
    /// `binding.phone`, for example `smoothDefaults`).
    func canPerform(_ action: String) -> Bool
    /// Runs the V12 button `action`; false when the phone cannot.
    func perform(_ action: String) -> Bool
    /// Runs a V15 button `action` with what its row names: the Setup page
    /// id of `openSetupPage`, or the text of a copy button. False when the
    /// phone cannot.
    func perform(_ action: String, argument: String?) -> Bool
    /// The desktop's name of the band a per-band row edits now, or nil
    /// before the Core names one.
    var perBandName: String? { get }
    /// This screen's refresh rate in frames a second, for Get Monitor Hz;
    /// nil when it is not known.
    var screenRefreshRate: Int? { get }
    /// The options of a choice kept under `key` that this phone cannot
    /// show, each value with why, in plain words. Shown disabled, never chosen.
    func unavailableOptions(forPhoneKey key: String) -> [Int64: String]
    /// Why this phone cannot keep `key` at all, in plain words, for a row
    /// the phone draws greyed with its own reason; nil for the general one.
    func unavailableReason(forPhoneKey key: String) -> String?
}

public extension SetupPhoneKeys {
    func canPerform(_ action: String) -> Bool { false }
    func perform(_ action: String) -> Bool { false }
    func perform(_ action: String, argument: String?) -> Bool { perform(action) }
    var perBandName: String? { nil }
    var screenRefreshRate: Int? { nil }
    func unavailableOptions(forPhoneKey key: String) -> [Int64: String] { [:] }
    func unavailableReason(forPhoneKey key: String) -> String? { nil }
}
