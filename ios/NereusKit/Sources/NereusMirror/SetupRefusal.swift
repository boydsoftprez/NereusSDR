// NereusSDR for iOS: why a described Setup control cannot be edited now
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why a described control cannot be edited now, in plain words.
public struct SetupRefusal: Error, Equatable, Sendable {
    public let reason: String

    public init(reason: String) {
        self.reason = reason
    }
}

/// A row's question as it was asked (V12's `confirm`), with what its `%1`
/// named then: Yes sends only while the row would still send that.
public struct SetupQuestion: Equatable, Sendable {
    public let text: String
    let named: String?
}
