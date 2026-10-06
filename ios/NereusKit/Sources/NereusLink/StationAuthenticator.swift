// NereusSDR for iOS: what the app signs in to a Core with
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Builds the app's `auth.request` once the Core's `hello` has arrived and
/// its certificate, or for a paired Core its identity, has been checked.
/// `DeviceKeyAuthenticator` signs in a paired device by its key;
/// `TokenAuthenticator` sends a Core's older token. A thrown
/// `StationAuthenticationError` carries plain words for the operator; any
/// other error is shown as a failed sign-in.
public protocol StationAuthenticator: Sendable {
    func authRequest(stationHello: LinkMessage.Hello, certificateSHA256: Data) async throws
        -> LinkMessage.AuthRequest
    /// True when this signs with the device's key, which a session uses
    /// only under an identity trust, after checking the Core's key and its
    /// certificate binding.
    var signsWithDeviceKey: Bool { get }
}

extension StationAuthenticator {
    public var signsWithDeviceKey: Bool { false }
}

/// An authenticator could not sign in, in words for the operator.
public struct StationAuthenticationError: Error, Equatable {
    public let reason: String

    public init(reason: String) {
        self.reason = reason
    }
}
