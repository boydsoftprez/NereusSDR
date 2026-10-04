// NereusSDR for iOS: one-use secret for a verified control path move
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The accepted `session.pathTicket` result. Construct this only from the
/// Core's ordinal-zero utf8 ticket and ordinal-one i64 expiry. The caller
/// owns command matching; the session owns the one-use join.
public struct PathTicket: Sendable, CustomStringConvertible, CustomDebugStringConvertible, CustomReflectable {
    let secret: String
    public let expiresInMs: Int64

    public init?(secret: String, expiresInMs: Int64) {
        guard secret.utf8.count == 43, let decoded = Base64URL.decode(secret),
              decoded.count == 32, Base64URL.encode(decoded) == secret,
              expiresInMs > 0 else {
            return nil
        }
        self.secret = secret
        self.expiresInMs = expiresInMs
    }

    public var description: String { "PathTicket(secret: <redacted>, expiresInMs: \(expiresInMs))" }
    public var debugDescription: String { description }
    public var customMirror: Mirror {
        Mirror(self, children: ["secret": "<redacted>", "expiresInMs": String(expiresInMs)])
    }
}
