// NereusSDR for iOS: the remote access service's messages, read and written by its document's tables
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One message between a client and the remote access service (the
/// rendezvous document, section 5): the five kinds a client sends and the
/// nine the service sends a client. Every key a kind lists must be present
/// with its field kind (section 5.2); a key a kind does not list is ignored
/// and never written; a kind of the other direction or one this app does
/// not know does not decode, and the client logs and ignores it (section
/// 5.3). Numbers keep their written form: `7.0`, `"7"` and `true` are not
/// whole numbers.
public enum RendezvousMessage: Sendable, Equatable {
    // From a client to the service.
    case introduce(Introduce)
    /// One ICE candidate (`candidate:...`), or the empty string that ends
    /// them. Travels both ways.
    case candidate(String)
    case mailboxOpen(nameplate: Int)
    /// One mailbox message, passed on untouched. Travels both ways.
    case mailbox(body: String)
    case mailboxClose

    // From the service to a client.
    case hello(Hello)
    /// The Core's answer, and the relay credentials when the Core allowed
    /// the relay and the service has one.
    case answer(sdp: String, turn: RendezvousTurn?)
    /// A service-issued binary relay grant for the current introduction.
    case relayGrant(RelayGrant)
    case introductionEnd(code: String)
    case mailboxOpened(nameplate: Int)
    case mailboxClosed(code: String)
    case error(ServiceError)

    /// Which way a message travels, which decides the kinds it may be.
    public enum Direction: Sendable {
        /// From a client to the service.
        case toService
        /// From the service to a client.
        case toClient
    }

    public struct Introduce: Sendable, Equatable {
        /// The Core's rendezvous id.
        public var id: String
        /// This device's id (link document section 3.5).
        public var device: String
        /// base64url of the device's signature over the introduction transcript.
        public var deviceSignature: String
        /// The offer, untouched.
        public var offer: String

        public init(id: String, device: String, deviceSignature: String, offer: String) {
            self.id = id
            self.device = device
            self.deviceSignature = deviceSignature
            self.offer = offer
        }
    }

    public struct Hello: Sendable, Equatable {
        public var version: Int
        /// base64url of 32 bytes, new for this connection.
        public var nonce: String
        /// The service's STUN servers, in its order.
        public var stun: [String]

        public init(version: Int, nonce: String, stun: [String]) {
            self.version = version
            self.nonce = nonce
            self.stun = stun
        }
    }

    public struct ServiceError: Sendable, Equatable {
        public var code: String
        /// Plain words the app may show as sent.
        public var reason: String
        /// Milliseconds to wait before trying again; 0 is no advice.
        public var retryAfterMs: Int64

        public init(code: String, reason: String, retryAfterMs: Int64) {
            self.code = code
            self.reason = reason
            self.retryAfterMs = retryAfterMs
        }
    }

    /// Why a text is not a message of the direction asked for.
    public enum DecodeError: Error, Equatable {
        case notAnObject
        /// A kind this direction does not carry, or no string `type`.
        case unknownKind(String)
        /// A listed key missing, or not of its field kind.
        case badField(kind: String, key: String)
    }

    // MARK: Limits (section 2 and 5.2)

    /// The most a message may take on the wire, encoded.
    public static let maxSentBytes = 131_072
    /// The most the app accepts from the service.
    public static let maxReceivedBytes = 262_144
    static let maxSdpBytes = 65_536
    static let maxBodyBytes = 65_536
    static let maxCandidateBytes = 4096
    static let maxReasonBytes = 1024
    static let maxUrls = 8
    static let maxUrlBytes = 512
    static let maxUsernameBytes = 512
    static let maxPasswordBytes = 128
    static let maxNameplate = 999_999
    static let maxVersion = 65_535
    static let maxRetryMs: Int64 = 2_147_483_647
    static let maxExpires: Int64 = 4_294_967_295

    /// The `type` this message is written with.
    public var kindName: String {
        switch self {
        case .introduce: return "introduce"
        case .candidate: return "candidate"
        case .mailboxOpen: return "mailbox.open"
        case .mailbox: return "mailbox"
        case .mailboxClose: return "mailbox.close"
        case .hello: return "hello"
        case .answer: return "answer"
        case .relayGrant: return "relay.grant"
        case .introductionEnd: return "introduction.end"
        case .mailboxOpened: return "mailbox.opened"
        case .mailboxClosed: return "mailbox.closed"
        case .error: return "error"
        }
    }

    // MARK: Decoding

    /// Reads one message travelling `direction`.
    public static func decode(_ text: String, direction: Direction) throws -> RendezvousMessage {
        guard let parsed = try? JSONSerialization.jsonObject(with: Data(text.utf8)),
              let object = parsed as? [String: Any] else {
            throw DecodeError.notAnObject
        }
        guard let type = object["type"] as? String else {
            throw DecodeError.unknownKind("")
        }
        let r = Reader(kind: type, object: object)
        switch (direction, type) {
        case (.toService, "introduce"):
            return .introduce(Introduce(id: try r.string("id", Field.rid),
                                        device: try r.string("device", Field.bytes(32)),
                                        deviceSignature: try r.string("deviceSignature", Field.bytes(64)),
                                        offer: try r.string("offer", Field.sdp)))
        case (_, "candidate"):
            return .candidate(try r.string("candidate", Field.candidate))
        case (.toService, "mailbox.open"):
            return .mailboxOpen(nameplate: Int(try r.whole("nameplate", 1...Int64(maxNameplate))))
        case (_, "mailbox"):
            return .mailbox(body: try r.string("body", Field.body))
        case (.toService, "mailbox.close"):
            return .mailboxClose
        case (.toClient, "hello"):
            return .hello(Hello(version: Int(try r.whole("version", 1...Int64(maxVersion))),
                                nonce: try r.string("nonce", Field.bytes(32)),
                                stun: try r.urls("stun")))
        case (.toClient, "answer"):
            return .answer(sdp: try r.string("answer", Field.sdp), turn: try r.turn("turn"))
        case (.toClient, "relay.grant"):
            let url = try r.string("url", Field.relayUrl)
            let token = try r.string("token", Field.relayToken)
            let expires = try r.whole("expires", 0...maxExpires)
            guard let grant = try? RelayGrant(urlString: url, token: token, expires: UInt32(expires)) else {
                throw DecodeError.badField(kind: type, key: "url")
            }
            return .relayGrant(grant)
        case (.toClient, "introduction.end"):
            return .introductionEnd(code: try r.string("code", Field.code))
        case (.toClient, "mailbox.opened"):
            return .mailboxOpened(nameplate: Int(try r.whole("nameplate", 1...Int64(maxNameplate))))
        case (.toClient, "mailbox.closed"):
            return .mailboxClosed(code: try r.string("code", Field.code))
        case (.toClient, "error"):
            return .error(ServiceError(code: try r.string("code", Field.code),
                                       reason: try r.string("reason", Field.reason),
                                       retryAfterMs: try r.whole("retryAfterMs", 0...maxRetryMs)))
        default:
            throw DecodeError.unknownKind(type)
        }
    }

    // MARK: Encoding

    /// The message as compact JSON, keys in the order section 5.3 lists them.
    public var encoded: String {
        var out = ""
        write(.object(members), into: &out)
        return out
    }

    /// The message as the app sends it, travelling `direction`: nil when a
    /// field is not of its kind or the whole is over 131072 bytes (the
    /// sender's rule, section 2), so it is never sent.
    public func encodedForSending(_ direction: Direction) -> String? {
        let text = encoded
        guard text.utf8.count <= Self.maxSentBytes,
              let decoded = try? Self.decode(text, direction: direction), decoded == self else {
            return nil
        }
        return text
    }

    private indirect enum Out {
        case string(String)
        case int(Int64)
        case null
        case array([Out])
        case object([(String, Out)])
    }

    private var members: [(String, Out)] {
        var members: [(String, Out)] = [("type", .string(kindName))]
        switch self {
        case .introduce(let introduce):
            members += [("id", .string(introduce.id)), ("device", .string(introduce.device)),
                        ("deviceSignature", .string(introduce.deviceSignature)),
                        ("offer", .string(introduce.offer))]
        case .candidate(let candidate):
            members.append(("candidate", .string(candidate)))
        case .mailboxOpen(let nameplate), .mailboxOpened(let nameplate):
            members.append(("nameplate", .int(Int64(nameplate))))
        case .mailbox(let body):
            members.append(("body", .string(body)))
        case .mailboxClose:
            break
        case .hello(let hello):
            members += [("version", .int(Int64(hello.version))), ("nonce", .string(hello.nonce)),
                        ("stun", .array(hello.stun.map { .string($0) }))]
        case .answer(let sdp, let turn):
            members.append(("answer", .string(sdp)))
            if let turn {
                members.append(("turn", .object([("username", .string(turn.username)),
                                                 ("password", .string(turn.password)),
                                                 ("expires", .int(turn.expires)),
                                                 ("urls", .array(turn.urls.map { .string($0) }))])))
            } else {
                members.append(("turn", .null))
            }
        case .relayGrant(let grant):
            members += [("url", .string(grant.url.absoluteString)), ("token", .string(grant.wireToken)),
                        ("expires", .int(Int64(grant.expires)))]
        case .introductionEnd(let code), .mailboxClosed(let code):
            members.append(("code", .string(code)))
        case .error(let error):
            members += [("code", .string(error.code)), ("reason", .string(error.reason)),
                        ("retryAfterMs", .int(error.retryAfterMs))]
        }
        return members
    }

    private func write(_ value: Out, into out: inout String) {
        switch value {
        case .string(let text):
            Self.writeString(text, into: &out)
        case .int(let number):
            out += String(number)
        case .null:
            out += "null"
        case .array(let elements):
            out += "["
            for (index, element) in elements.enumerated() {
                if index > 0 {
                    out += ","
                }
                write(element, into: &out)
            }
            out += "]"
        case .object(let pairs):
            out += "{"
            for (index, pair) in pairs.enumerated() {
                if index > 0 {
                    out += ","
                }
                Self.writeString(pair.0, into: &out)
                out += ":"
                write(pair.1, into: &out)
            }
            out += "}"
        }
    }

    /// A JSON string: quotes, backslashes and control characters escaped,
    /// everything else as UTF-8 (section 5.1).
    private static func writeString(_ value: String, into out: inout String) {
        out += "\""
        for scalar in value.unicodeScalars {
            switch scalar {
            case "\"":
                out += "\\\""
            case "\\":
                out += "\\\\"
            case "\n":
                out += "\\n"
            case "\r":
                out += "\\r"
            case "\t":
                out += "\\t"
            default:
                if scalar.value < 0x20 {
                    let hex = String(scalar.value, radix: 16)
                    out += "\\u" + String(repeating: "0", count: 4 - hex.count) + hex
                } else {
                    out.unicodeScalars.append(scalar)
                }
            }
        }
        out += "\""
    }

    // MARK: Field kinds (section 5.2)

    /// A check on a string field.
    private enum Field {
        case rid
        /// Strict base64url of exactly this many bytes.
        case bytes(Int)
        case sdp
        case candidate
        case body
        case code
        case reason
        case relayUrl
        case relayToken

        func accepts(_ text: String) -> Bool {
            let size = text.utf8.count
            switch self {
            case .rid:
                return RendezvousIdentity.isStationId(text)
            case .bytes(let count):
                return Base64URL.decode(text)?.count == count
            case .sdp:
                return (1...RendezvousMessage.maxSdpBytes).contains(size)
            case .candidate:
                return size <= RendezvousMessage.maxCandidateBytes && (text.isEmpty || text.hasPrefix("candidate:"))
            case .body:
                return (1...RendezvousMessage.maxBodyBytes).contains(size)
            case .code:
                return (1...64).contains(size) && text.utf8.allSatisfy { (0x41...0x5A).contains($0) || (0x61...0x7A).contains($0) }
            case .reason:
                return (1...RendezvousMessage.maxReasonBytes).contains(size)
            case .relayUrl:
                return (1...RendezvousMessage.maxUrlBytes).contains(size)
                    && text.utf8.allSatisfy { (0x21...0x7e).contains($0) } && text.hasPrefix("wss://")
            case .relayToken:
                return (1...512).contains(size) && text.utf8.allSatisfy {
                    (65...90).contains($0) || (97...122).contains($0) || (48...57).contains($0)
                        || $0 == 45 || $0 == 95
                }
            }
        }
    }

    /// Reads a kind's listed keys, refusing any that is missing or not of
    /// its field kind.
    private struct Reader {
        let kind: String
        let object: [String: Any]

        private func bad(_ key: String) -> DecodeError {
            .badField(kind: kind, key: key)
        }

        func string(_ key: String, _ field: Field) throws -> String {
            guard let text = Self.string(object[key], field) else {
                throw bad(key)
            }
            return text
        }

        func whole(_ key: String, _ range: ClosedRange<Int64>) throws -> Int64 {
            guard let number = Self.whole(object[key], range) else {
                throw bad(key)
            }
            return number
        }

        func urls(_ key: String) throws -> [String] {
            guard let urls = Self.urls(object[key]) else {
                throw bad(key)
            }
            return urls
        }

        /// `null`, or the four keys of a `turn` object (others ignored).
        func turn(_ key: String) throws -> RendezvousTurn? {
            guard let value = object[key] else {
                throw bad(key)
            }
            if value is NSNull {
                return nil
            }
            guard let turn = value as? [String: Any],
                  let username = Self.string(turn["username"], nil),
                  (1...RendezvousMessage.maxUsernameBytes).contains(username.utf8.count),
                  let password = Self.string(turn["password"], nil),
                  (1...RendezvousMessage.maxPasswordBytes).contains(password.utf8.count),
                  let expires = Self.whole(turn["expires"], 0...RendezvousMessage.maxExpires),
                  let urls = Self.urls(turn["urls"]) else {
                throw bad(key)
            }
            return RendezvousTurn(username: username, password: password, expires: expires, urls: urls)
        }

        private static func string(_ value: Any?, _ field: Field?) -> String? {
            guard let text = value as? String else {
                return nil
            }
            if let field, !field.accepts(text) {
                return nil
            }
            return text
        }

        /// A JSON number written without a fraction or an exponent, in
        /// `range`. Foundation reads `7.0` and `7e0` as doubles and `7` as
        /// an integer, and `true` as a boolean, which are told apart here.
        private static func whole(_ value: Any?, _ range: ClosedRange<Int64>) -> Int64? {
            guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID() else {
                return nil
            }
            let type = String(cString: number.objCType)
            guard type != "d", type != "f" else {
                return nil
            }
            let whole = number.int64Value
            return range.contains(whole) ? whole : nil
        }

        private static func urls(_ value: Any?) -> [String]? {
            guard let list = value as? [Any], list.count <= RendezvousMessage.maxUrls else {
                return nil
            }
            var urls: [String] = []
            for element in list {
                guard let url = element as? String, (1...RendezvousMessage.maxUrlBytes).contains(url.utf8.count) else {
                    return nil
                }
                urls.append(url)
            }
            return urls
        }
    }
}
