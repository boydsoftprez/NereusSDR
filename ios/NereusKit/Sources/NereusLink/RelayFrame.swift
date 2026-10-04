// NereusSDR for iOS: the web relay's binary datagram frames
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// A service-issued grant. Its token is opaque and belongs only in a JOIN
/// binary message to this URL. The value's description deliberately hides it.
public struct RelayGrant: Sendable, Equatable, CustomStringConvertible, CustomDebugStringConvertible, CustomReflectable {
    public let url: URL
    fileprivate let token: String
    public let expires: UInt32

    /// Use this overload when decoding the service's raw relayUrl so its
    /// 512-byte printable-ASCII bound is checked before URL normalization.
    public init(urlString: String, token: String, expires: UInt32) throws {
        let bytes = urlString.utf8
        guard bytes.count >= 1, bytes.count <= 512,
              bytes.allSatisfy({ (0x21...0x7e).contains($0) }),
              urlString.hasPrefix("wss://"), let url = URL(string: urlString) else {
            throw RelayFrameError.invalidGrant
        }
        try self.init(url: url, token: token, expires: expires)
    }

    public init(url: URL, token: String, expires: UInt32) throws {
        let address = url.absoluteString.utf8
        guard address.count >= 1, address.count <= 512,
              address.allSatisfy({ (0x21...0x7e).contains($0) }),
              url.scheme == "wss", url.host != nil, url.user == nil, url.password == nil,
              url.fragment == nil, token.utf8.count >= 1, token.utf8.count <= 512,
              token.utf8.allSatisfy({ byte in
                  (65...90).contains(byte) || (97...122).contains(byte) ||
                  (48...57).contains(byte) || byte == 45 || byte == 95
              }) else {
            throw RelayFrameError.invalidGrant
        }
        self.url = url
        self.token = token
        self.expires = expires
    }

    public var description: String { "RelayGrant(url: <redacted>, token: <redacted>)" }
    public var debugDescription: String { description }
    public var customMirror: Mirror { Mirror(self, children: ["url": "<redacted>", "token": "<redacted>"]) }

    public static func == (lhs: RelayGrant, rhs: RelayGrant) -> Bool {
        lhs.url == rhs.url && lhs.token == rhs.token && lhs.expires == rhs.expires
    }

    // Used only by the service-message codec; the token stays off URLs and logs.
    var wireToken: String { token }
}

public enum RelayLane: UInt8, Sendable, Equatable {
    case control = 1
    case media = 2
}

public enum RelayEndCode: String, Sendable, Equatable {
    case protocolError, timeout, badToken, expired, ended, full
    case tooManyConnections, tooManySessions, replaced, peerGone, idle, shuttingDown
    case unknown

    /// The same grant can be used again after a transient refusal or reset.
    public var permitsRejoin: Bool {
        switch self {
        case .timeout, .full, .tooManyConnections, .tooManySessions, .shuttingDown: true
        default: false
        }
    }

    public var retryDelay: Duration {
        switch self {
        case .full, .tooManyConnections, .tooManySessions: .seconds(1)
        default: .zero
        }
    }
}

public enum RelayFrameError: Error, Sendable, Equatable {
    case invalidGrant, invalidDatagram, malformedFrame, oversizeFrame
}

public enum RelayReceivedFrame: Sendable, Equatable {
    case ready(peerPresent: Bool)
    case peer(present: Bool)
    case datagram(RelayLane, Data)
    case end(RelayEndCode)
    case ignored
}

/// Parses only the relay header. Datagram bytes remain opaque encrypted
/// ICE/DTLS/SRTP data and never enter a text codec or a log.
public struct RelayRoutedMedia: Sendable, Equatable {
    public let connectionId: UUID
    public let datagram: Data
}

public enum RelayFrame {
    public static let maximumMessageBytes = 1501
    public static let maximumDatagramBytes = 1500

    public static func join(_ grant: RelayGrant) throws -> Data {
        var frame = Data([0x80])
        frame.append(contentsOf: grant.token.utf8)
        return frame
    }

    public static func datagram(_ lane: RelayLane, _ bytes: Data) throws -> Data {
        guard !bytes.isEmpty, bytes.count <= maximumDatagramBytes else {
            throw RelayFrameError.invalidDatagram
        }
        var frame = Data([lane.rawValue])
        frame.append(bytes)
        return frame
    }

    /// With negotiated mediaRelayRoutingVersion 1, the UUID precedes the
    /// unchanged encrypted agent datagram inside tag 2. The caller decides
    /// whether routing was negotiated; raw media uses `datagram` instead.
    public static func routedMedia(_ connectionId: UUID, _ datagram: Data) throws -> Data {
        guard !datagram.isEmpty, datagram.count <= 1484 else { throw RelayFrameError.invalidDatagram }
        var uuid = connectionId.uuid
        var payload = withUnsafeBytes(of: &uuid) { Data($0) }
        payload.append(datagram)
        return try self.datagram(.media, payload)
    }

    public static func unrouteMedia(_ payload: Data) throws -> RelayRoutedMedia {
        guard (17...1500).contains(payload.count) else { throw RelayFrameError.malformedFrame }
        let bytes = Array(payload.prefix(16))
        let uuid = uuid_t(bytes[0], bytes[1], bytes[2], bytes[3],
                          bytes[4], bytes[5], bytes[6], bytes[7],
                          bytes[8], bytes[9], bytes[10], bytes[11],
                          bytes[12], bytes[13], bytes[14], bytes[15])
        return RelayRoutedMedia(connectionId: UUID(uuid: uuid), datagram: Data(payload.dropFirst(16)))
    }

    public static func read(_ frame: Data) throws -> RelayReceivedFrame {
        guard !frame.isEmpty else { throw RelayFrameError.malformedFrame }
        guard frame.count <= maximumMessageBytes else { throw RelayFrameError.oversizeFrame }
        let tag = frame[frame.startIndex]
        let payload = frame.dropFirst()
        switch tag {
        case 0, 0x80:
            throw RelayFrameError.malformedFrame
        case 1, 2:
            guard !payload.isEmpty else { throw RelayFrameError.malformedFrame }
            return .datagram(RelayLane(rawValue: tag)!, Data(payload))
        case 0x81:
            guard payload.count >= 2, payload[payload.startIndex] >= 1,
                  payload[payload.startIndex + 1] <= 1 else { throw RelayFrameError.malformedFrame }
            return .ready(peerPresent: payload[payload.startIndex + 1] == 1)
        case 0x82:
            guard let first = payload.first, first <= 1 else { throw RelayFrameError.malformedFrame }
            return .peer(present: first == 1)
        case 0x83:
            guard (1...64).contains(payload.count),
                  payload.allSatisfy({ (65...90).contains($0) || (97...122).contains($0) }),
                  let code = String(data: payload, encoding: .ascii) else {
                throw RelayFrameError.malformedFrame
            }
            return .end(RelayEndCode(rawValue: code) ?? .unknown)
        default:
            // Unknown data and future relay control tags are versioned out.
            return .ignored
        }
    }
}
