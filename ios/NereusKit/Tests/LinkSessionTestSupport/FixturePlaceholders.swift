// NereusSDR for iOS: the session fixtures' placeholders, filled and matched as an app's runner does
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink

/// Link document section 16.1's placeholders, from an app's side: an app's
/// runner fills the station messages it gives its client, and matches the
/// messages its client sends, recording what the client chose.
public struct FixturePlaceholders {
    /// Values recorded under a name, for `"$ref:<name>"`.
    public var record: [String: LinkJSON] = [:]

    /// The class properties `surface.json`'s `mirrorClasses` lists, for the
    /// summarised snapshot stand-ins.
    public let mirrorClasses: [String: [[String: Any]]]
    /// The station identity every station `hello` carries.
    public let stationIdentity: TestStationIdentity
    /// The SHA-256 of the certificate the runner's transport reports, which
    /// the identity binds.
    public let certificateSHA256: Data
    /// The challenge of the latest station `hello`, as bytes.
    public private(set) var challenge: Data?

    typealias Malformed = LinkFixtureLoader.Malformed

    public init(mirrorClasses: [String: [[String: Any]]], stationIdentity: TestStationIdentity = TestStationIdentity(),
                certificateSHA256: Data) {
        self.mirrorClasses = mirrorClasses
        self.stationIdentity = stationIdentity
        self.certificateSHA256 = certificateSHA256
    }

    // MARK: Station messages

    /// A station message as an app's runner sends it: `"$string"` as `""`,
    /// `"$int"` as `0`, `"$ref:<name>"` as the recorded value,
    /// `"$within:<t>:<v>"` as `<v>`, and a summarised snapshot's `"$any"`
    /// as the class's stand-ins. In a `hello`, `identity` is the test
    /// station identity binding ``certificateSHA256``, and `challenge` 32
    /// fresh random bytes, recorded as `challenge` when the fixture writes
    /// `"$capture:challenge"`.
    public mutating func fillStation(_ message: LinkJSON, at label: String) throws -> LinkJSON {
        guard case .object(var object) = message, case .string(let type)? = object["type"] else {
            throw Malformed(description: "\(label): a station message must be an object with a type")
        }
        if type == "hello" {
            if object["identity"] != nil {
                let claim = try stationIdentity.claim(certificateSHA256: certificateSHA256)
                object["identity"] = .object(["publicKey": .string(claim.publicKey),
                                              "certBinding": .string(claim.certBinding)])
            }
            if let written = object["challenge"] {
                let fresh = TestStationIdentity.newChallenge()
                challenge = Base64URL.decode(fresh)
                if written == .string("$capture:challenge") {
                    record["challenge"] = .string(fresh)
                }
                object["challenge"] = .string(fresh)
            }
        }
        if type == "schema", object["fields"] == .string("$any") {
            object["fields"] = .array(try properties(of: object["class"], at: label).map { property in
                .object(["ordinal": property.ordinal, "name": property.name, "kind": property.kind])
            })
        }
        if type == "object.create", object["properties"] == .string("$any") {
            object["properties"] = .array(try properties(of: object["class"], at: label).map { property in
                .object(["ordinal": property.ordinal, "name": property.name, "kind": property.kind,
                         "value": property.standIn])
            })
        }
        return try fillStationValue(.object(object), at: label)
    }

    private mutating func fillStationValue(_ value: LinkJSON, at label: String) throws -> LinkJSON {
        switch value {
        case .string(let text) where text.hasPrefix("$"):
            if text == "$string" {
                return .string("")
            }
            if text == "$int" {
                return .number(0)
            }
            if text == "$capture:diversityIncarnation0" || text == "$capture:diversityIncarnation1" {
                // Test-only identities for the isolated producer corpus. The
                // Core runner captures its random boot nonce; the app stand-in
                // keeps one positive identity throughout the transcript.
                let name = String(text.dropFirst("$capture:".count))
                let identity = LinkJSON.number(text.hasSuffix("0") ? 1 : 2)
                record[name] = identity
                return identity
            }
            if text.hasPrefix("$ref:") {
                return try recorded(String(text.dropFirst("$ref:".count)), at: label)
            }
            if text.hasPrefix("$within:") {
                let parts = text.split(separator: ":", omittingEmptySubsequences: false)
                guard parts.count == 3, Self.isJSONNumber(parts[1]), Self.isJSONNumber(parts[2]),
                      let tolerance = Double(parts[1]), tolerance >= 0, let centre = Double(parts[2]) else {
                    throw Malformed(description: "\(label): malformed \(text)")
                }
                return .number(centre)
            }
            throw Malformed(description: "\(label): \(text) may not appear in a station message for the app")
        case .array(let elements):
            return .array(try elements.map { try fillStationValue($0, at: label) })
        case .object(let members):
            if let expectation = members[Self.jsonKey] {
                // JSON inside a string (section 16.1): the expectation,
                // filled by these same rules, sent as its compact text.
                guard members.count == 1 else {
                    throw Malformed(description: "\(label): \"\(Self.jsonKey)\" beside another key")
                }
                return .string(try fillStationValue(expectation, at: label).compactText)
            }
            return .object(try members.mapValues { try fillStationValue($0, at: label) })
        default:
            return value
        }
    }

    /// The one key of a `{"$json": <expectation>}` (section 16.1).
    public static let jsonKey = "$json"

    private struct StandIn {
        let ordinal: LinkJSON
        let name: LinkJSON
        let kind: LinkJSON
        let standIn: LinkJSON
    }

    /// A class's properties with the stand-in value of their kind: `bool`
    /// false, `i64` and `f64` 0, `utf8` "", `enum` the first of its values.
    private func properties(of className: LinkJSON?, at label: String) throws -> [StandIn] {
        guard case .string(let name)? = className, let properties = mirrorClasses[name] else {
            throw Malformed(description: "\(label): no class \(String(describing: className)) in mirrorClasses")
        }
        return try properties.map { property in
            guard let ordinal = property["ordinal"] as? Int, let propertyName = property["name"] as? String,
                  let kind = property["kind"] as? String else {
                throw Malformed(description: "\(label): mirrorClasses \(name) has a malformed property")
            }
            let standIn: LinkJSON
            switch kind {
            case "bool":
                standIn = .bool(false)
            case "i64", "f64":
                standIn = .number(0)
            case "utf8":
                standIn = .string("")
            case "enum":
                guard let first = (property["enumValues"] as? [Int])?.first else {
                    throw Malformed(description: "\(label): enum \(name).\(propertyName) has no values")
                }
                standIn = .number(Double(first))
            default:
                throw Malformed(description: "\(label): \(name).\(propertyName) has unknown kind \(kind)")
            }
            return StandIn(ordinal: .number(Double(ordinal)), name: .string(propertyName), kind: .string(kind),
                           standIn: standIn)
        }
    }

    // MARK: Client messages

    /// A behaviour step's message filled with what the runner tells its
    /// client to send: `"$int:<name>..."` takes the next number of the
    /// client's own counter, `"$string:<name>"` the client's origin, and
    /// `"$uuid:<name>"` (a media connection id the client makes) a new
    /// random UUID, canonical and lower case, at each fill. The name is
    /// recorded when the client's message is matched.
    public func fillClient(_ message: LinkJSON, counter: inout Int, origin: String, at label: String) throws -> LinkJSON {
        switch message {
        case .string(let text) where text.hasPrefix("$"):
            if text.hasPrefix("$int:") {
                let parts = text.split(separator: ":", omittingEmptySubsequences: false)
                let value = counter
                counter += 1
                if parts.count == 4 {
                    guard let lowest = Int(parts[2]), let highest = Int(parts[3]) else {
                        throw Malformed(description: "\(label): malformed \(text)")
                    }
                    guard value >= lowest && value <= highest else {
                        throw Malformed(description: "\(label): \(value) is outside \(text)")
                    }
                }
                return .number(Double(value))
            }
            if text.hasPrefix("$string:") {
                return .string(origin)
            }
            if text.hasPrefix("$uuid:") {
                guard text.split(separator: ":", omittingEmptySubsequences: false).count == 2 else {
                    throw Malformed(description: "\(label): malformed \(text)")
                }
                return .string(UUID().uuidString.lowercased())
            }
            if text.hasPrefix("$ref:") {
                return try recorded(String(text.dropFirst("$ref:".count)), at: label)
            }
            throw Malformed(description: "\(label): \(text) in a message the runner has its client send")
        case .array(let elements):
            return .array(try elements.map { try fillClient($0, counter: &counter, origin: origin, at: label) })
        case .object(let members):
            var filled: [String: LinkJSON] = [:]
            for key in members.keys.sorted() {
                filled[key] = try fillClient(members[key]!, counter: &counter, origin: origin, at: label)
            }
            return .object(filled)
        default:
            return message
        }
    }

    /// Matches what the client sent against a behaviour step, recording the
    /// names its placeholders give. Returns why they differ, or nil.
    public mutating func matchClient(_ template: LinkJSON, _ actual: LinkJSON, at label: String) throws -> String? {
        try match(template, actual, path: "", sibling: nil, at: label)
    }

    private mutating func match(_ template: LinkJSON, _ actual: LinkJSON, path: String,
                                sibling: [String: LinkJSON]?, at label: String) throws -> String? {
        if case .string(let text) = template, text.hasPrefix("$") {
            return try matchPlaceholder(text, actual, path: path, sibling: sibling, at: label)
        }
        switch (template, actual) {
        case (.object(let expected), .object(let got)):
            if Set(expected.keys) != Set(got.keys) {
                return "\(path.isEmpty ? "the message" : path): keys \(got.keys.sorted()), expected \(expected.keys.sorted())"
            }
            for key in expected.keys.sorted() {
                if let failure = try match(expected[key]!, got[key]!, path: path + "." + key, sibling: got, at: label) {
                    return failure
                }
            }
            return nil
        case (.array(let expected), .array(let got)):
            guard expected.count == got.count else {
                return "\(path): \(got.count) elements, expected \(expected.count)"
            }
            for index in expected.indices {
                if let failure = try match(expected[index], got[index], path: "\(path)[\(index)]", sibling: nil,
                                           at: label) {
                    return failure
                }
            }
            return nil
        default:
            return template == actual ? nil : "\(path): \(actual.compactText), expected \(template.compactText)"
        }
    }

    private mutating func matchPlaceholder(_ text: String, _ actual: LinkJSON, path: String,
                                           sibling: [String: LinkJSON]?, at label: String) throws -> String? {
        let parts = text.split(separator: ":", omittingEmptySubsequences: false).map(String.init)
        switch parts[0] {
        case "$string":
            guard case .string = actual else {
                return "\(path): \(actual.compactText) is not a string"
            }
            if parts.count == 2 {
                record[parts[1]] = actual
            }
            return nil
        case "$int":
            guard case .number(let number) = actual, number.rounded(.towardZero) == number else {
                return "\(path): \(actual.compactText) is not a whole number"
            }
            if parts.count == 4 {
                guard let lowest = Double(parts[2]), let highest = Double(parts[3]) else {
                    throw Malformed(description: "\(label): malformed \(text)")
                }
                if number < lowest || number > highest {
                    return "\(path): \(actual.compactText) is outside \(parts[2]) to \(parts[3])"
                }
            }
            if parts.count >= 2 {
                record[parts[1]] = actual
            }
            return nil
        case "$object":
            guard case .object = actual else {
                return "\(path): \(actual.compactText) is not an object"
            }
            return nil
        case "$majors":
            guard case .array(let elements) = actual, !elements.isEmpty else {
                return "\(path): majors must be a non-empty array"
            }
            var previous = -1.0
            for element in elements {
                guard case .number(let major) = element, major.rounded(.towardZero) == major,
                      major >= 0, major <= 65_535, major > previous else {
                    return "\(path): majors must be whole numbers 0 to 65535, ascending, without repeats"
                }
                previous = major
            }
            guard let major = sibling?["major"], elements.contains(major) else {
                return "\(path): majors does not hold the hello's major"
            }
            return nil
        case "$uuid":
            // A media connection id the client chose: a canonical UUID,
            // lower case, without braces, recorded for the `"$ref:<name>"`
            // steps after it.
            guard parts.count == 2, !parts[1].isEmpty else {
                throw Malformed(description: "\(label): malformed \(text)")
            }
            guard case .string(let id) = actual, Self.isCanonicalUUID(id) else {
                return "\(path): \(actual.compactText) is not a lower-case canonical UUID"
            }
            record[parts[1]] = actual
            return nil
        case "$ref":
            let expected = try recorded(parts.dropFirst().joined(separator: ":"), at: label)
            return expected == actual ? nil : "\(path): \(actual.compactText), expected \(expected.compactText)"
        case "$device":
            guard parts == ["$device", "signed"], path == ".device" else {
                throw Malformed(description: "\(label): \(text) may appear only as auth.request's device, as $device:signed")
            }
            guard case .string(let written)? = record["challenge"], let challenge = Base64URL.decode(written) else {
                throw Malformed(description: "\(label): $device:signed with no challenge recorded")
            }
            return DeviceBlockCheck.failure(actual, challenge: challenge, certificateSHA256: certificateSHA256,
                                            stationKey: stationIdentity.publicKey).map { "\(path): \($0)" }
        default:
            throw Malformed(description: "\(label): \(text) may not appear in a client message")
        }
    }

    private func recorded(_ name: String, at label: String) throws -> LinkJSON {
        guard let value = record[name] else {
            throw Malformed(description: "\(label): nothing is recorded under \(name)")
        }
        return value
    }

    /// Exactly a canonical UUID in lower case without braces
    /// (8-4-4-4-12 hexadecimal digits), as `"$uuid:<name>"` matches.
    public static func isCanonicalUUID(_ text: String) -> Bool {
        text.range(of: #"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"#,
                   options: .regularExpression) != nil
    }

    /// Exactly a JSON number (RFC 8259 section 6).
    public static func isJSONNumber(_ text: Substring) -> Bool {
        text.range(of: #"^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$"#, options: .regularExpression) != nil
    }

    /// Only Diversity's two participant identity references may occur in a
    /// scripted message. All other existing placeholder guards still apply.
    public func fillDiversityScriptedIdentity(_ message: LinkJSON, at label: String) throws -> LinkJSON {
        guard case .object(var object) = message, object["type"] == .string("command.invoke"),
              object["verb"] == .string("diversity.setTarget"), case .array(var args)? = object["args"],
              args.count == 8 else { return message }
        for (index, name) in [(3, "sourceIncarnation"), (6, "targetIncarnation")] {
            guard case .object(var arg) = args[index], arg["name"] == .string(name),
                  case .string(let text)? = arg["value"],
                  text == "$ref:diversityIncarnation0" || text == "$ref:diversityIncarnation1" else { continue }
            arg["value"] = try recorded(String(text.dropFirst("$ref:".count)), at: label)
            args[index] = .object(arg)
        }
        object["args"] = .array(args)
        return .object(object)
    }

    /// True when a scripted message holds a placeholder other than `"$ref:token"`.
    public static func holdsPlaceholder(_ value: LinkJSON) -> Bool {
        switch value {
        case .string(let text):
            return text.hasPrefix("$") && text != "$ref:token"
        case .array(let elements):
            return elements.contains(where: holdsPlaceholder)
        case .object(let members):
            return members.values.contains(where: holdsPlaceholder)
        default:
            return false
        }
    }
}
