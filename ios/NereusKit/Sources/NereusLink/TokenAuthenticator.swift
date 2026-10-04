// NereusSDR for iOS: signs in with the Core's pairing token
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Signs in with the Core's token (link document section 3.3), as an
/// older Core expects and the tests use.
public struct TokenAuthenticator: StationAuthenticator {
    private let token: String

    public init(token: String) {
        self.token = token
    }

    public func authRequest(stationHello: LinkMessage.Hello, certificateSHA256: Data) async throws
        -> LinkMessage.AuthRequest {
        LinkMessage.AuthRequest(token: token)
    }
}
