// NereusSDR for iOS: reads and writes the link's JSON messages, by the link document's rules
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The link's message codec (link document sections 4, 11 and 12.3). A
/// message is refused when a required key is missing or holds the wrong
/// JSON type; no value is coerced from another type. Keys the codec does
/// not know are ignored. The identity, device and pairing strings are
/// checked for shape only, as the station's decoder checks them
/// (`SessionMessages.cpp`): what they hold is the sign-in's and the
/// pairing's to judge.
public enum LinkCodec {
    /// A whole `media.control` message is capped at 128 KiB.
    public static let mediaControlCapBytes = 131_072
    /// A whole `station.metrics.v1` message is capped at 16 KiB.
    public static let telemetryCapBytes = 16_384

    /// The three strings a non-finite `f64` travels as (section 4.2).
    private static let nanText = "nan"
    private static let infinityText = "inf"
    private static let negativeInfinityText = "-inf"

    // MARK: Decoding

    /// Reads one message from its text.
    public static func decode(_ text: String) throws -> LinkMessage {
        guard case .object(let object)? = try? LinkJSON.parse(text) else {
            throw LinkCodecError.notAnObject
        }
        guard case .string(let typeName)? = object["type"] else {
            throw LinkCodecError.unknownKind("")
        }
        guard let kind = LinkMessage.Kind(rawValue: typeName) else {
            throw LinkCodecError.unknownKind(typeName)
        }
        let bytes = text.utf8.count
        switch kind {
        case .mediaControl where bytes > mediaControlCapBytes:
            throw LinkCodecError.overCap(kind: typeName, bytes: bytes, cap: mediaControlCapBytes)
        case .stationMetrics where bytes > telemetryCapBytes:
            throw LinkCodecError.overCap(kind: typeName, bytes: bytes, cap: telemetryCapBytes)
        default:
            break
        }
        return try decode(kind: kind, Reader(kind: typeName, object: object))
    }

    private static func decode(kind: LinkMessage.Kind, _ r: Reader) throws -> LinkMessage {
        switch kind {
        case .hello:
            return .hello(try hello(r))
        case .authRequest:
            return .authRequest(LinkMessage.AuthRequest(token: try r.string("token"), device: try deviceBlock(r)))
        case .authResult:
            return .authResult(LinkMessage.AuthResult(accepted: try r.bool("accepted"),
                                                      reason: try r.string("reason"),
                                                      retryable: try r.optionalBool("retryable"),
                                                      code: try endCode(r)))
        case .capabilities:
            return .capabilities(LinkMessage.Capabilities(properties: try r.entries("properties")))
        case .commandInvoke:
            let verb = try r.string("verb")
            return .commandInvoke(LinkMessage.CommandInvoke(verb: verb, id: try commandId(r, verb: verb),
                                                            args: try r.entries("args")))
        case .commandResult:
            return .commandResult(try commandResult(r))
        case .confirmRequest:
            return .confirmRequest(try confirmRequest(r))
        case .notice:
            return .notice(try notice(r))
        case .delta:
            return .delta(LinkMessage.Delta(key: try r.string("key"), properties: try r.entries("properties")))
        case .mediaControl:
            return .mediaControl(LinkMessage.MediaControl(payload: try r.object("payload")))
        case .objectCreate:
            return .objectCreate(LinkMessage.ObjectCreate(key: try r.string("key"),
                                                          className: try r.string("class"),
                                                          properties: try r.entries("properties")))
        case .objectDestroy:
            return .objectDestroy(LinkMessage.ObjectDestroy(key: try r.string("key"),
                                                            className: try r.string("class")))
        case .pathJoin:
            let ticket = try r.nonEmptyString("ticket")
            guard ticket.utf16.count <= 128 else {
                throw r.invalid("ticket")
            }
            return .pathJoin(LinkMessage.PathJoin(ticket: ticket))
        case .pathSwitch:
            return .pathSwitch
        case .propertyResult:
            return .propertyResult(try propertyResult(r))
        case .propertyWrite:
            var writeId: UInt32?
            if r.has("writeId") {
                writeId = UInt32(try r.whole("writeId", from: 1, to: 4_294_967_295))
            }
            return .propertyWrite(LinkMessage.PropertyWrite(key: try r.string("key"), writeId: writeId,
                                                            properties: try r.entries("properties")))
        case .recordBatch:
            return .recordBatch(try recordBatch(r))
        case .schema:
            return .schema(LinkMessage.Schema(className: try r.string("class"), fields: try r.fields("fields")))
        case .sessionEnd:
            return .sessionEnd(LinkMessage.SessionEnd(reason: try r.string("reason"),
                                                      retryable: try r.optionalBool("retryable"),
                                                      code: try endCode(r),
                                                      takenOverBy: try r.optionalString("takenOverBy"),
                                                      takenOverById: try r.optionalString("takenOverById"),
                                                      secondsAgo: r.has("secondsAgo")
                                                          ? try r.whole("secondsAgo", from: 0, to: 9_007_199_254_740_991)
                                                          : nil))
        case .sessionHeld:
            return .sessionHeld(try sessionHeld(r))
        case .sessionTakeover:
            return .sessionTakeover(LinkMessage.SessionTakeover(
                deviceId: try r.string("deviceId"),
                revision: UInt32(try r.whole("revision", from: 0, to: 4_294_967_295))))
        case .pairStart:
            guard let mode = LinkMessage.PairStart.Mode(rawValue: try r.string("mode")) else {
                throw r.invalid("mode")
            }
            let device = try r.object("device")
            guard case .string(let publicKey)? = device["publicKey"], case .string(let name)? = device["name"],
                  case .string(let deviceKind)? = device["kind"] else {
                throw r.invalid("device")
            }
            return .pairStart(LinkMessage.PairStart(mode: mode, device: LinkMessage.PairDevice(
                publicKey: publicKey, name: name, kind: deviceKind)))
        case .pairAccept:
            guard let identity = identityClaim(try r.value("identity")) else {
                throw r.invalid("identity")
            }
            return .pairAccept(LinkMessage.PairAccept(identity: identity, label: try r.string("label")))
        case .pairSpake:
            return .pairSpake(LinkMessage.PairSpake(step: Int(try r.whole("step", from: 0, to: 3)),
                                                    data: try r.nonEmptyString("data")))
        case .pairConfirm:
            return .pairConfirm(LinkMessage.PairConfirm(box: try r.nonEmptyString("box")))
        case .pairFail:
            return .pairFail(LinkMessage.PairFail(reason: try r.string("reason"),
                                                  retryAfterMs: Int(try r.whole("retryAfterMs", from: 0,
                                                                                to: 2_147_483_647))))
        case .settingsReject:
            return .settingsReject(LinkMessage.SettingsReject(key: try r.string("key"),
                                                              properties: try r.entries("properties"),
                                                              reason: try r.optionalString("reason")))
        case .settingsRemove:
            return .settingsRemove(LinkMessage.SettingsRemove(key: try r.string("key"),
                                                              properties: try r.entries("properties")))
        case .settingsSnapshot:
            return .settingsSnapshot(LinkMessage.SettingsSnapshot(properties: try r.entries("properties")))
        case .settingsValue:
            return .settingsValue(LinkMessage.SettingsValue(key: try r.string("key"), origin: try r.string("origin"),
                                                            properties: try r.entries("properties")))
        case .settingsWrite:
            return .settingsWrite(LinkMessage.SettingsWrite(key: try r.string("key"), origin: try r.string("origin"),
                                                            properties: try r.entries("properties")))
        case .snapshotComplete:
            return .snapshotComplete
        case .stationMetrics:
            return .stationMetrics(LinkMessage.StationMetrics(payload: try r.object("payload")))
        }
    }

    private static func hello(_ r: Reader) throws -> LinkMessage.Hello {
        var hello = LinkMessage.Hello(major: UInt16(try r.whole("major", from: 0, to: 65_535)),
                                      minor: UInt16(try r.whole("minor", from: 0, to: 65_535)),
                                      settingsSchema: Int32(try r.whole("settingsSchema",
                                                                        from: -2_147_483_648,
                                                                        to: 2_147_483_647)),
                                      peer: try r.string("peer"))
        if r.has("majors") {
            guard case .array(let elements)? = r.object["majors"], !elements.isEmpty else {
                throw r.invalid("majors")
            }
            hello.majors = try elements.map { element in
                guard let value = wholeNumber(element, from: 0, to: 65_535) else {
                    throw r.invalid("majors")
                }
                return UInt16(value)
            }
        }
        if r.has("features") {
            guard case .object(let declared)? = r.object["features"] else {
                throw r.invalid("features")
            }
            var features: [String: Int] = [:]
            for (name, version) in declared {
                guard !name.isEmpty, let value = wholeNumber(version, from: 0, to: 2_147_483_647) else {
                    throw r.invalid("features")
                }
                features[name] = Int(value)
            }
            hello.features = features
        }
        if r.has("identity") {
            guard let identity = identityClaim(try r.value("identity")) else {
                throw r.invalid("identity")
            }
            hello.identity = identity
        }
        if r.has("challenge") {
            hello.challenge = try r.nonEmptyString("challenge")
        }
        return hello
    }

    /// An `identity` object: two strings, `publicKey` and `certBinding`.
    private static func identityClaim(_ json: LinkJSON) -> LinkMessage.StationIdentityClaim? {
        guard case .object(let fields) = json, case .string(let publicKey)? = fields["publicKey"],
              case .string(let certBinding)? = fields["certBinding"] else {
            return nil
        }
        return LinkMessage.StationIdentityClaim(publicKey: publicKey, certBinding: certBinding)
    }

    /// `auth.request`'s optional `device`: five strings, and `shortName`
    /// a string when present.
    private static func deviceBlock(_ r: Reader) throws -> LinkMessage.DeviceBlock? {
        guard r.has("device") else {
            return nil
        }
        guard case .object(let fields) = try r.value("device"),
              case .string(let id)? = fields["id"], case .string(let publicKey)? = fields["publicKey"],
              case .string(let name)? = fields["name"], case .string(let kind)? = fields["kind"],
              case .string(let signature)? = fields["signature"] else {
            throw r.invalid("device")
        }
        var shortName: String?
        if let value = fields["shortName"] {
            guard case .string(let text) = value else {
                throw r.invalid("device")
            }
            shortName = text
        }
        return LinkMessage.DeviceBlock(id: id, publicKey: publicKey, name: name, kind: kind, signature: signature,
                                       shortName: shortName)
    }

    /// `code`, when present, is a non-empty string (section 12.4).
    private static func endCode(_ r: Reader) throws -> String? {
        r.has("code") ? try r.nonEmptyString("code") : nil
    }

    private static func sessionHeld(_ r: Reader) throws -> LinkMessage.SessionHeld {
        let elements = try r.array("devices")
        guard elements.count <= 4 else { throw r.invalid("devices") }
        let revision = UInt32(try r.whole("revision", from: 0, to: 4_294_967_295))
        let devices = try elements.map { element -> LinkMessage.SessionHeld.Device in
            guard case .object(let fields) = element,
                  case .string(let id)? = fields["deviceId"], !id.isEmpty,
                  case .string(let name)? = fields["name"],
                  case .string(let shortName)? = fields["shortName"],
                  case .string(let kind)? = fields["kind"],
                  case .string(let stateName)? = fields["state"],
                  let state = LinkMessage.SessionHeld.Device.State(rawValue: stateName),
                  case .string(let from)? = fields["from"],
                  case .bool(let replaceable)? = fields["replaceable"],
                  case .bool(let holdsTransmit)? = fields["holdsTransmit"],
                  let lastActivity = fields["lastActivitySeconds"].flatMap({ wholeNumber($0, from: 0, to: 9_007_199_254_740_991) }),
                  let connectedFor = fields["connectedForSeconds"].flatMap({ wholeNumber($0, from: 0, to: 9_007_199_254_740_991) }),
                  let awayFor = fields["awayForSeconds"].flatMap({ wholeNumber($0, from: 0, to: 9_007_199_254_740_991) }),
                  let transmittingFor = fields["transmittingForSeconds"].flatMap({ wholeNumber($0, from: 0, to: 9_007_199_254_740_991) }),
                  case .array(let slices)? = fields["listeningOn"] else {
                throw r.invalid("devices")
            }
            let listening = try slices.map { value -> LinkMessage.SessionHeld.Slice in
                guard let slice = heldSlice(value) else { throw r.invalid("devices") }
                return slice
            }
            let transmitting: LinkMessage.SessionHeld.Slice?
            if let value = fields["transmittingOn"] {
                guard let slice = heldSlice(value) else { throw r.invalid("devices") }
                transmitting = slice
            } else {
                transmitting = nil
            }
            return .init(deviceId: id, name: name, shortName: shortName, kind: kind, state: state,
                         from: from, replaceable: replaceable, holdsTransmit: holdsTransmit,
                         lastActivitySeconds: lastActivity, connectedForSeconds: connectedFor,
                         awayForSeconds: awayFor, transmittingForSeconds: transmittingFor,
                         listeningOn: listening, transmittingOn: transmitting)
        }
        var placeTaken: LinkMessage.SessionHeld.PlaceTaken?
        if r.has("placeTaken") {
            let fields = try r.object("placeTaken")
            guard case .string(let byName)? = fields["byName"],
                  case .string(let byId)? = fields["byId"],
                  let seconds = fields["secondsAgo"].flatMap({ wholeNumber($0, from: 0, to: 9_007_199_254_740_991) }) else {
                throw r.invalid("placeTaken")
            }
            placeTaken = .init(byName: byName, byId: byId, secondsAgo: seconds)
        }
        var placeFreed: LinkMessage.SessionHeld.PlaceFreed?
        if r.has("placeFreed") {
            let fields = try r.object("placeFreed")
            guard let seconds = fields["secondsAgo"].flatMap({ wholeNumber($0, from: 0, to: 9_007_199_254_740_991) }) else {
                throw r.invalid("placeFreed")
            }
            placeFreed = .init(secondsAgo: seconds)
        }
        return .init(devices: devices, revision: revision, placeTaken: placeTaken, placeFreed: placeFreed)
    }

    private static func heldSlice(_ value: LinkJSON) -> LinkMessage.SessionHeld.Slice? {
        guard case .object(let fields) = value,
              case .string(let letter)? = fields["letter"],
              case .number(let frequency)? = fields["frequencyHz"], frequency.isFinite, frequency >= 0,
              let mode = fields["mode"].flatMap({ wholeNumber($0, from: 0, to: 65_535) }),
              let band = fields["band"].flatMap({ wholeNumber($0, from: 0, to: 65_535) }) else {
            return nil
        }
        let sliceId = fields["sliceId"].flatMap({ wholeNumber($0, from: 0, to: 9_007_199_254_740_991) })
        return .init(sliceId: sliceId, letter: letter, frequencyHz: frequency, mode: UInt16(mode), band: UInt16(band))
    }

    private static func heldSliceJSON(_ slice: LinkMessage.SessionHeld.Slice) -> LinkJSON {
        var fields: [String: LinkJSON] = [
            "letter": .string(slice.letter), "frequencyHz": .number(slice.frequencyHz),
            "mode": .number(Double(slice.mode)), "band": .number(Double(slice.band)),
        ]
        if let id = slice.sliceId { fields["sliceId"] = .number(Double(id)) }
        return .object(fields)
    }

    /// The `nnr.*`, `ps3.*` and `dspAssets.*` families need an id from 1
    /// (section 9.1); every other verb's id may be 0.
    private static func commandId(_ r: Reader, verb: String) throws -> UInt32 {
        let lowest: Int64 = isNumberedFamily(verb) ? 1 : 0
        return UInt32(try r.whole("id", from: lowest, to: 4_294_967_295))
    }

    static func isNumberedFamily(_ verb: String) -> Bool {
        verb.hasPrefix("nnr.") || verb.hasPrefix("ps3.") || verb.hasPrefix("dspAssets.")
    }

    private static func commandResult(_ r: Reader) throws -> LinkMessage.CommandResult {
        let verb = try r.string("verb")
        let id = try commandId(r, verb: verb)
        let accepted = try r.bool("accepted")
        let reason = try r.string("reason")
        guard case .array(let affectedValues)? = r.object["affected"] else {
            throw r.has("affected") ? r.invalid("affected") : r.missing("affected")
        }
        let affected = try affectedValues.map { value in
            guard case .string(let key) = value else {
                throw r.invalid("affected")
            }
            return key
        }
        var values: [LinkMessage.PropertyEntry]?
        if r.has("values") {
            let entries = try r.entries("values")
            let names = Set(entries.map(\.name))
            guard entries.count <= 128, names.count == entries.count, !names.contains("") else {
                throw r.invalid("values")
            }
            values = entries
        }
        return LinkMessage.CommandResult(verb: verb, id: id, accepted: accepted, reason: reason,
                                         affected: affected, values: values)
    }

    /// The largest whole number JSON carries exactly, the bound of a
    /// question's or a notice's id and of `secondsAgo`.
    static let largestExactWhole: Int64 = 9_007_199_254_740_991

    /// `confirm.request` (section 7.5): a whole-number id, a kind, a reason,
    /// `affected` and `expiresInMs`; the nested values are kept as sent.
    private static func confirmRequest(_ r: Reader) throws -> LinkMessage.ConfirmRequest {
        LinkMessage.ConfirmRequest(
            id: try r.whole("id", from: 0, to: largestExactWhole),
            kind: try r.string("kind"),
            reason: try r.string("reason"),
            affected: try r.array("affected"),
            expiresInMs: try r.whole("expiresInMs", from: 0, to: 2_147_483_647),
            change: r.has("change") ? try r.object("change") : nil,
            choices: r.has("choices") ? try r.array("choices") : nil,
            forCommandId: r.has("forCommandId") ? try r.whole("forCommandId", from: 0, to: 4_294_967_295) : nil,
            forWriteId: r.has("forWriteId") ? try r.whole("forWriteId", from: 0, to: 4_294_967_295) : nil,
            forSettingsKey: try r.optionalString("forSettingsKey"),
            holder: r.has("holder") ? try r.object("holder") : nil)
    }

    /// `notice` (section 7.5): a whole-number id, a kind, a reason,
    /// `secondsAgo` and `takeBack`; who did it and what it touched when sent.
    private static func notice(_ r: Reader) throws -> LinkMessage.Notice {
        LinkMessage.Notice(
            id: try r.whole("id", from: 0, to: largestExactWhole),
            kind: try r.string("kind"),
            reason: try r.string("reason"),
            secondsAgo: try r.whole("secondsAgo", from: 0, to: largestExactWhole),
            takeBack: try r.bool("takeBack"),
            byDeviceId: try r.optionalString("byDeviceId"),
            byName: try r.optionalString("byName"),
            byShortName: try r.optionalString("byShortName"),
            byKind: try r.optionalString("byKind"),
            bySource: try r.optionalString("bySource"),
            slices: r.has("slices") ? try r.array("slices") : nil,
            change: r.has("change") ? try r.object("change") : nil)
    }

    /// A record batch names its stream and a generation from 1; every upsert
    /// carries a non-empty `id` and its `fields` object, and every remove is
    /// a non-empty `id`, as the station's decoder checks them.
    private static func recordBatch(_ r: Reader) throws -> LinkMessage.RecordBatch {
        let upserts = try r.array("upserts").map { element -> LinkMessage.RecordBatch.Record in
            guard case .object(let upsert) = element, case .string(let id)? = upsert["id"], !id.isEmpty,
                  case .object(let fields)? = upsert["fields"] else {
                throw r.invalid("upserts")
            }
            return LinkMessage.RecordBatch.Record(id: id, fields: fields)
        }
        let removes = try r.array("removes").map { element -> String in
            guard case .string(let id) = element, !id.isEmpty else {
                throw r.invalid("removes")
            }
            return id
        }
        return LinkMessage.RecordBatch(stream: try r.nonEmptyString("stream"),
                                       generation: try r.whole("generation", from: 1, to: 9_007_199_254_740_991),
                                       reset: try r.bool("reset"), upserts: upserts, removes: removes)
    }

    private static func propertyResult(_ r: Reader) throws -> LinkMessage.PropertyResult {
        let key = try r.string("key")
        let writeId = UInt32(try r.whole("writeId", from: 1, to: 4_294_967_295))
        guard case .array(let entries)? = r.object["results"] else {
            throw r.has("results") ? r.invalid("results") : r.missing("results")
        }
        guard entries.count <= 512 else {
            throw r.invalid("results")
        }
        var seen: Set<String> = []
        let results = try entries.map { entry -> LinkMessage.PropertyResult.Result in
            guard case .object(let fields) = entry,
                  case .string(let property)? = fields["property"],
                  case .bool(let accepted)? = fields["accepted"],
                  case .string(let reason)? = fields["reason"],
                  case .bool(let hasValue)? = fields["hasValue"],
                  !property.isEmpty, seen.insert(property).inserted else {
                throw r.invalid("results")
            }
            var value: LinkMessage.PropertyEntry?
            if hasValue {
                guard let kept = fields["value"].flatMap(entry(from:)), kept.name == property else {
                    throw r.invalid("results")
                }
                value = kept
            }
            if accepted && (!hasValue || !reason.isEmpty) {
                throw r.invalid("results")
            }
            return LinkMessage.PropertyResult.Result(property: property, accepted: accepted,
                                                     reason: reason, value: value)
        }
        return LinkMessage.PropertyResult(key: key, writeId: writeId, results: results)
    }

    /// A property entry (section 4.1), or nil when it is malformed.
    static func entry(from json: LinkJSON) -> LinkMessage.PropertyEntry? {
        guard case .object(let fields) = json,
              let ordinal = fields["ordinal"].flatMap({ wholeNumber($0, from: 0, to: 65_535) }),
              case .string(let name)? = fields["name"],
              case .string(let kindName)? = fields["kind"],
              let kind = LinkMessage.WireKind(rawValue: kindName),
              let raw = fields["value"] else {
            return nil
        }
        let value: LinkMessage.PropertyValue
        switch (kind, raw) {
        case (.bool, .bool(let flag)):
            value = .bool(flag)
        case (.i64, _):
            guard let whole = int64(raw) else {
                return nil
            }
            value = .i64(whole)
        case (.enumeration, _):
            guard let whole = int64(raw) else {
                return nil
            }
            value = .enumeration(whole)
        case (.f64, .number(let number)):
            value = .f64(number)
        case (.f64, .string(let token)):
            switch token {
            case nanText: value = .f64(.nan)
            case infinityText: value = .f64(.infinity)
            case negativeInfinityText: value = .f64(-.infinity)
            default: return nil
            }
        case (.utf8, .string(let text)):
            value = .utf8(text)
        default:
            return nil
        }
        return LinkMessage.PropertyEntry(ordinal: UInt16(ordinal), name: name, value: value)
    }

    /// A whole number inside the 64-bit signed range; JSON numbers are
    /// doubles, so the upper bound is the exclusive 2^63.
    private static func int64(_ json: LinkJSON) -> Int64? {
        guard case .number(let number) = json, number.isFinite,
              number >= -9_223_372_036_854_775_808.0, number < 9_223_372_036_854_775_808.0,
              number.rounded(.towardZero) == number else {
            return nil
        }
        return Int64(number)
    }

    private static func wholeNumber(_ json: LinkJSON, from lowest: Int64, to highest: Int64) -> Int64? {
        guard let value = int64(json), value >= lowest, value <= highest else {
            return nil
        }
        return value
    }

    /// Reads the keys of one message, naming the kind in every refusal.
    private struct Reader {
        let kind: String
        let object: [String: LinkJSON]

        func has(_ key: String) -> Bool {
            object[key] != nil
        }

        func missing(_ key: String) -> LinkCodecError {
            .missingKey(kind: kind, key: key)
        }

        func invalid(_ key: String) -> LinkCodecError {
            .invalidValue(kind: kind, key: key)
        }

        func value(_ key: String) throws -> LinkJSON {
            guard let value = object[key] else {
                throw missing(key)
            }
            return value
        }

        func string(_ key: String) throws -> String {
            guard case .string(let text) = try value(key) else {
                throw invalid(key)
            }
            return text
        }

        func nonEmptyString(_ key: String) throws -> String {
            let text = try string(key)
            guard !text.isEmpty else {
                throw invalid(key)
            }
            return text
        }

        func optionalString(_ key: String) throws -> String? {
            has(key) ? try string(key) : nil
        }

        func bool(_ key: String) throws -> Bool {
            guard case .bool(let flag) = try value(key) else {
                throw invalid(key)
            }
            return flag
        }

        func optionalBool(_ key: String) throws -> Bool? {
            has(key) ? try bool(key) : nil
        }

        func whole(_ key: String, from lowest: Int64, to highest: Int64) throws -> Int64 {
            guard let number = LinkCodec.wholeNumber(try value(key), from: lowest, to: highest) else {
                throw invalid(key)
            }
            return number
        }

        func object(_ key: String) throws -> [String: LinkJSON] {
            guard case .object(let members) = try value(key) else {
                throw invalid(key)
            }
            return members
        }

        func array(_ key: String) throws -> [LinkJSON] {
            guard case .array(let elements) = try value(key) else {
                throw invalid(key)
            }
            return elements
        }

        func entries(_ key: String) throws -> [LinkMessage.PropertyEntry] {
            guard case .array(let elements) = try value(key) else {
                throw invalid(key)
            }
            return try elements.map { element in
                guard let entry = LinkCodec.entry(from: element) else {
                    throw invalid(key)
                }
                return entry
            }
        }

        func fields(_ key: String) throws -> [LinkMessage.SchemaField] {
            guard case .array(let elements) = try value(key) else {
                throw invalid(key)
            }
            return try elements.map { element in
                guard case .object(let field) = element,
                      let ordinal = field["ordinal"].flatMap({ LinkCodec.wholeNumber($0, from: 0, to: 65_535) }),
                      case .string(let name)? = field["name"],
                      case .string(let kindName)? = field["kind"],
                      let kind = LinkMessage.WireKind(rawValue: kindName) else {
                    throw invalid(key)
                }
                return LinkMessage.SchemaField(ordinal: UInt16(ordinal), name: name, kind: kind)
            }
        }
    }

    // MARK: Encoding

    /// Writes one message as compact JSON.
    public static func encode(_ message: LinkMessage) -> String {
        json(message).compactText
    }

    /// The message as a JSON value.
    public static func json(_ message: LinkMessage) -> LinkJSON {
        var o: [String: LinkJSON] = ["type": .string(message.kind.rawValue)]
        switch message {
        case .hello(let hello):
            o["major"] = .number(Double(hello.major))
            o["minor"] = .number(Double(hello.minor))
            o["settingsSchema"] = .number(Double(hello.settingsSchema))
            o["peer"] = .string(hello.peer)
            if let majors = hello.majors {
                o["majors"] = .array(majors.map { .number(Double($0)) })
            }
            if let features = hello.features {
                o["features"] = .object(features.mapValues { .number(Double($0)) })
            }
            if let identity = hello.identity {
                o["identity"] = json(identity)
            }
            if let challenge = hello.challenge {
                o["challenge"] = .string(challenge)
            }
        case .authRequest(let request):
            o["token"] = .string(request.token)
            if let device = request.device {
                var fields: [String: LinkJSON] = [
                    "id": .string(device.id),
                    "publicKey": .string(device.publicKey),
                    "name": .string(device.name),
                    "kind": .string(device.kind),
                    "signature": .string(device.signature),
                ]
                if let shortName = device.shortName, !shortName.isEmpty {
                    fields["shortName"] = .string(shortName)
                }
                o["device"] = .object(fields)
            }
        case .authResult(let result):
            o["accepted"] = .bool(result.accepted)
            o["reason"] = .string(result.reason)
            if let retryable = result.retryable {
                o["retryable"] = .bool(retryable)
            }
            if let code = result.code {
                o["code"] = .string(code)
            }
        case .capabilities(let capabilities):
            o["properties"] = entries(capabilities.properties)
        case .commandInvoke(let invoke):
            o["verb"] = .string(invoke.verb)
            o["id"] = .number(Double(invoke.id))
            o["args"] = entries(invoke.args)
        case .commandResult(let result):
            o["verb"] = .string(result.verb)
            o["id"] = .number(Double(result.id))
            o["accepted"] = .bool(result.accepted)
            o["reason"] = .string(result.reason)
            o["affected"] = .array(result.affected.map { .string($0) })
            if let values = result.values {
                o["values"] = entries(values)
            }
        case .confirmRequest(let request):
            o["id"] = .number(Double(request.id))
            o["kind"] = .string(request.kind)
            o["reason"] = .string(request.reason)
            o["affected"] = .array(request.affected)
            o["expiresInMs"] = .number(Double(request.expiresInMs))
            if let change = request.change {
                o["change"] = .object(change)
            }
            if let choices = request.choices {
                o["choices"] = .array(choices)
            }
            if let id = request.forCommandId {
                o["forCommandId"] = .number(Double(id))
            }
            if let id = request.forWriteId {
                o["forWriteId"] = .number(Double(id))
            }
            if let key = request.forSettingsKey {
                o["forSettingsKey"] = .string(key)
            }
            if let holder = request.holder {
                o["holder"] = .object(holder)
            }
        case .notice(let notice):
            o["id"] = .number(Double(notice.id))
            o["kind"] = .string(notice.kind)
            o["reason"] = .string(notice.reason)
            o["secondsAgo"] = .number(Double(notice.secondsAgo))
            o["takeBack"] = .bool(notice.takeBack)
            let names: [(String, String?)] = [("byDeviceId", notice.byDeviceId), ("byName", notice.byName),
                                              ("byShortName", notice.byShortName), ("byKind", notice.byKind),
                                              ("bySource", notice.bySource)]
            for (key, value) in names {
                if let value {
                    o[key] = .string(value)
                }
            }
            if let slices = notice.slices {
                o["slices"] = .array(slices)
            }
            if let change = notice.change {
                o["change"] = .object(change)
            }
        case .delta(let delta):
            o["key"] = .string(delta.key)
            o["properties"] = entries(delta.properties)
        case .mediaControl(let control):
            o["payload"] = .object(control.payload)
        case .objectCreate(let create):
            o["key"] = .string(create.key)
            o["class"] = .string(create.className)
            o["properties"] = entries(create.properties)
        case .objectDestroy(let destroy):
            o["key"] = .string(destroy.key)
            o["class"] = .string(destroy.className)
        case .pathJoin(let join):
            o["ticket"] = .string(join.ticket)
        case .pathSwitch:
            break
        case .propertyResult(let result):
            o["key"] = .string(result.key)
            o["writeId"] = .number(Double(result.writeId))
            o["results"] = .array(result.results.map { entry in
                var fields: [String: LinkJSON] = [
                    "property": .string(entry.property),
                    "accepted": .bool(entry.accepted),
                    "reason": .string(entry.reason),
                    "hasValue": .bool(entry.value != nil),
                ]
                if let value = entry.value {
                    fields["value"] = json(value)
                }
                return .object(fields)
            })
        case .propertyWrite(let write):
            o["key"] = .string(write.key)
            if let writeId = write.writeId {
                o["writeId"] = .number(Double(writeId))
            }
            o["properties"] = entries(write.properties)
        case .recordBatch(let batch):
            o["stream"] = .string(batch.stream)
            o["generation"] = .number(Double(batch.generation))
            o["reset"] = .bool(batch.reset)
            o["upserts"] = .array(batch.upserts.map { record in
                .object(["id": .string(record.id), "fields": .object(record.fields)])
            })
            o["removes"] = .array(batch.removes.map { .string($0) })
        case .schema(let schema):
            o["class"] = .string(schema.className)
            o["fields"] = .array(schema.fields.map { field in
                .object([
                    "ordinal": .number(Double(field.ordinal)),
                    "name": .string(field.name),
                    "kind": .string(field.kind.rawValue),
                ])
            })
        case .sessionEnd(let end):
            o["reason"] = .string(end.reason)
            if let retryable = end.retryable {
                o["retryable"] = .bool(retryable)
            }
            if let code = end.code {
                o["code"] = .string(code)
            }
            if let takenOverBy = end.takenOverBy { o["takenOverBy"] = .string(takenOverBy) }
            if let takenOverById = end.takenOverById { o["takenOverById"] = .string(takenOverById) }
            if let secondsAgo = end.secondsAgo { o["secondsAgo"] = .number(Double(secondsAgo)) }
        case .sessionHeld(let held):
            o["devices"] = .array(held.devices.map { device in
                var fields: [String: LinkJSON] = [
                    "deviceId": .string(device.deviceId), "name": .string(device.name),
                    "shortName": .string(device.shortName), "kind": .string(device.kind),
                    "state": .string(device.state.rawValue), "from": .string(device.from),
                    "replaceable": .bool(device.replaceable), "holdsTransmit": .bool(device.holdsTransmit),
                    "lastActivitySeconds": .number(Double(device.lastActivitySeconds)),
                    "connectedForSeconds": .number(Double(device.connectedForSeconds)),
                    "awayForSeconds": .number(Double(device.awayForSeconds)),
                    "transmittingForSeconds": .number(Double(device.transmittingForSeconds)),
                    "listeningOn": .array(device.listeningOn.map(heldSliceJSON)),
                ]
                if let slice = device.transmittingOn { fields["transmittingOn"] = heldSliceJSON(slice) }
                return .object(fields)
            })
            o["revision"] = .number(Double(held.revision))
            if let taken = held.placeTaken {
                o["placeTaken"] = .object(["byName": .string(taken.byName), "byId": .string(taken.byId),
                                            "secondsAgo": .number(Double(taken.secondsAgo))])
            }
            if let freed = held.placeFreed {
                o["placeFreed"] = .object(["secondsAgo": .number(Double(freed.secondsAgo))])
            }
        case .sessionTakeover(let answer):
            o["deviceId"] = .string(answer.deviceId)
            o["revision"] = .number(Double(answer.revision))
        case .pairStart(let start):
            o["mode"] = .string(start.mode.rawValue)
            o["device"] = .object([
                "publicKey": .string(start.device.publicKey),
                "name": .string(start.device.name),
                "kind": .string(start.device.kind),
            ])
        case .pairAccept(let accept):
            o["identity"] = json(accept.identity)
            o["label"] = .string(accept.label)
        case .pairSpake(let spake):
            o["step"] = .number(Double(spake.step))
            o["data"] = .string(spake.data)
        case .pairConfirm(let confirm):
            o["box"] = .string(confirm.box)
        case .pairFail(let fail):
            o["reason"] = .string(fail.reason)
            o["retryAfterMs"] = .number(Double(fail.retryAfterMs))
        case .settingsReject(let reject):
            o["key"] = .string(reject.key)
            o["properties"] = entries(reject.properties)
            if let reason = reject.reason {
                o["reason"] = .string(reason)
            }
        case .settingsRemove(let remove):
            o["key"] = .string(remove.key)
            o["properties"] = entries(remove.properties)
        case .settingsSnapshot(let snapshot):
            o["properties"] = entries(snapshot.properties)
        case .settingsValue(let value):
            o["key"] = .string(value.key)
            o["origin"] = .string(value.origin)
            o["properties"] = entries(value.properties)
        case .settingsWrite(let write):
            o["key"] = .string(write.key)
            o["origin"] = .string(write.origin)
            o["properties"] = entries(write.properties)
        case .snapshotComplete:
            break
        case .stationMetrics(let metrics):
            o["payload"] = .object(metrics.payload)
        }
        return .object(o)
    }

    private static func json(_ identity: LinkMessage.StationIdentityClaim) -> LinkJSON {
        .object(["publicKey": .string(identity.publicKey), "certBinding": .string(identity.certBinding)])
    }

    private static func entries(_ list: [LinkMessage.PropertyEntry]) -> LinkJSON {
        .array(list.map(json))
    }

    /// A property entry as JSON; a non-finite `f64` becomes its string.
    public static func json(_ entry: LinkMessage.PropertyEntry) -> LinkJSON {
        let value: LinkJSON
        switch entry.value {
        case .bool(let flag):
            value = .bool(flag)
        case .i64(let whole), .enumeration(let whole):
            value = .number(Double(whole))
        case .f64(let number):
            if number.isNaN {
                value = .string(nanText)
            } else if number.isInfinite {
                value = .string(number > 0 ? infinityText : negativeInfinityText)
            } else {
                value = .number(number)
            }
        case .utf8(let text):
            value = .string(text)
        }
        return .object([
            "ordinal": .number(Double(entry.ordinal)),
            "name": .string(entry.name),
            "kind": .string(entry.kind.rawValue),
            "value": value,
        ])
    }
}
