// NereusSDR for iOS: what a described Setup control shows now: its value, and whether and why it can change
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One described control as the page draws it. `value` is the owner's
/// value, or the last one this phone saw when the Core is not current;
/// `reason` says in plain words why the control cannot change now. A
/// readout is never editable and needs no reason.
public struct SetupControlState: Equatable, Sendable {
    public let value: SetupValue?
    public let editable: Bool
    public let reason: String?

    public init(value: SetupValue?, editable: Bool, reason: String?) {
        self.value = value
        self.editable = editable
        self.reason = reason
    }
}
