// NereusSDR for iOS: every message the link carries, one case per kind
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One message on the control connection, a JSON object whose `type` names
/// its kind (link document section 4). Every kind in the link's surface has
/// a case; `LinkCodec` reads and writes them.
public enum LinkMessage: Sendable, Equatable {
    case hello(Hello)
    case authRequest(AuthRequest)
    case authResult(AuthResult)
    case capabilities(Capabilities)
    case commandInvoke(CommandInvoke)
    case commandResult(CommandResult)
    case confirmRequest(ConfirmRequest)
    case delta(Delta)
    case mediaControl(MediaControl)
    case notice(Notice)
    case objectCreate(ObjectCreate)
    case objectDestroy(ObjectDestroy)
    case pairAccept(PairAccept)
    case pairConfirm(PairConfirm)
    case pairFail(PairFail)
    case pairSpake(PairSpake)
    case pairStart(PairStart)
    case pathJoin(PathJoin)
    case pathSwitch
    case propertyResult(PropertyResult)
    case propertyWrite(PropertyWrite)
    case recordBatch(RecordBatch)
    case schema(Schema)
    case sessionEnd(SessionEnd)
    case sessionHeld(SessionHeld)
    case sessionTakeover(SessionTakeover)
    case settingsReject(SettingsReject)
    case settingsRemove(SettingsRemove)
    case settingsSnapshot(SettingsSnapshot)
    case settingsValue(SettingsValue)
    case settingsWrite(SettingsWrite)
    case snapshotComplete
    case stationMetrics(StationMetrics)

    /// A message kind, by its wire name.
    public enum Kind: String, Sendable, CaseIterable {
        case hello = "hello"
        case authRequest = "auth.request"
        case authResult = "auth.result"
        case capabilities = "capabilities"
        case commandInvoke = "command.invoke"
        case commandResult = "command.result"
        case confirmRequest = "confirm.request"
        case delta = "delta"
        case mediaControl = "media.control"
        case notice = "notice"
        case objectCreate = "object.create"
        case objectDestroy = "object.destroy"
        case pairAccept = "pair.accept"
        case pairConfirm = "pair.confirm"
        case pairFail = "pair.fail"
        case pairSpake = "pair.spake"
        case pairStart = "pair.start"
        case pathJoin = "path.join"
        case pathSwitch = "path.switch"
        case propertyResult = "property.result"
        case propertyWrite = "property.write"
        case recordBatch = "record.batch"
        case schema = "schema"
        case sessionEnd = "session.end"
        case sessionHeld = "session.held"
        case sessionTakeover = "session.takeover"
        case settingsReject = "settings.reject"
        case settingsRemove = "settings.remove"
        case settingsSnapshot = "settings.snapshot"
        case settingsValue = "settings.value"
        case settingsWrite = "settings.write"
        case snapshotComplete = "snapshot.complete"
        case stationMetrics = "station.metrics.v1"

        /// A kind a client sends (link document section 16.2).
        public var sentByClient: Bool {
            switch self {
            case .hello, .authRequest, .commandInvoke, .mediaControl, .propertyWrite,
                 .settingsWrite, .settingsRemove, .pairStart, .pairSpake, .pairConfirm, .pairFail,
                 .pathJoin, .pathSwitch, .sessionTakeover:
                return true
            default:
                return false
            }
        }

        /// A kind the station sends (link document section 16.2).
        public var sentByStation: Bool {
            switch self {
            case .authRequest, .commandInvoke, .propertyWrite, .settingsWrite, .settingsRemove, .pairStart,
                 .pathJoin, .sessionTakeover:
                return false
            default:
                return true
            }
        }
    }

    public var kind: Kind {
        switch self {
        case .hello: return .hello
        case .authRequest: return .authRequest
        case .authResult: return .authResult
        case .capabilities: return .capabilities
        case .commandInvoke: return .commandInvoke
        case .commandResult: return .commandResult
        case .confirmRequest: return .confirmRequest
        case .delta: return .delta
        case .mediaControl: return .mediaControl
        case .notice: return .notice
        case .objectCreate: return .objectCreate
        case .objectDestroy: return .objectDestroy
        case .pairAccept: return .pairAccept
        case .pairConfirm: return .pairConfirm
        case .pairFail: return .pairFail
        case .pairSpake: return .pairSpake
        case .pairStart: return .pairStart
        case .pathJoin: return .pathJoin
        case .pathSwitch: return .pathSwitch
        case .propertyResult: return .propertyResult
        case .propertyWrite: return .propertyWrite
        case .recordBatch: return .recordBatch
        case .schema: return .schema
        case .sessionEnd: return .sessionEnd
        case .sessionHeld: return .sessionHeld
        case .sessionTakeover: return .sessionTakeover
        case .settingsReject: return .settingsReject
        case .settingsRemove: return .settingsRemove
        case .settingsSnapshot: return .settingsSnapshot
        case .settingsValue: return .settingsValue
        case .settingsWrite: return .settingsWrite
        case .snapshotComplete: return .snapshotComplete
        case .stationMetrics: return .stationMetrics
        }
    }

    // MARK: Property entries (link document section 4.1)

    /// The wire kind of a property.
    public enum WireKind: String, Sendable, CaseIterable {
        case bool
        case i64
        case f64
        case utf8
        case enumeration = "enum"
    }

    /// A property value in its wire kind. An `f64` may be NaN or infinite;
    /// it travels as `"nan"`, `"inf"` or `"-inf"` (section 4.2).
    public enum PropertyValue: Sendable, Equatable {
        case bool(Bool)
        case i64(Int64)
        case f64(Double)
        case utf8(String)
        case enumeration(Int64)

        public var kind: WireKind {
            switch self {
            case .bool: return .bool
            case .i64: return .i64
            case .f64: return .f64
            case .utf8: return .utf8
            case .enumeration: return .enumeration
            }
        }
    }

    /// The entry capabilities, settings, object properties, deltas, command
    /// arguments and command result values all carry.
    public struct PropertyEntry: Sendable, Equatable {
        public var ordinal: UInt16
        public var name: String
        public var value: PropertyValue

        public init(ordinal: UInt16 = 0, name: String, value: PropertyValue) {
            self.ordinal = ordinal
            self.name = name
            self.value = value
        }

        public var kind: WireKind { value.kind }
    }

    /// One field of a class's schema.
    public struct SchemaField: Sendable, Equatable {
        public var ordinal: UInt16
        public var name: String
        public var kind: WireKind

        public init(ordinal: UInt16, name: String, kind: WireKind) {
            self.ordinal = ordinal
            self.name = name
            self.kind = kind
        }
    }

    // MARK: Connecting (sections 5 and 6)

    public struct Hello: Sendable, Equatable {
        public var major: UInt16
        public var minor: UInt16
        public var settingsSchema: Int32
        public var peer: String
        /// The sender's supported majors, oldest first; absent from a peer
        /// built before the key existed.
        public var majors: [UInt16]?
        /// Declared features, name to version; absent means none.
        public var features: [String: Int]?
        /// The Core's identity key and its binding of this connection's
        /// certificate (section 3.4); absent from a Core without a usable
        /// identity key, and from a client.
        public var identity: StationIdentityClaim?
        /// This connection's sign-in challenge, base64url (section 3.4).
        public var challenge: String?

        public init(major: UInt16, minor: UInt16, settingsSchema: Int32, peer: String,
                    majors: [UInt16]? = nil, features: [String: Int]? = nil,
                    identity: StationIdentityClaim? = nil, challenge: String? = nil) {
            self.major = major
            self.minor = minor
            self.settingsSchema = settingsSchema
            self.peer = peer
            self.majors = majors
            self.features = features
            self.identity = identity
            self.challenge = challenge
        }

        /// The majors the sender supports: `majors`, or `[major]` without it.
        public var supportedMajors: [UInt16] { majors ?? [major] }
    }

    /// The Core's identity as a `hello` or `pair.accept` carries it: its
    /// public key and its certificate binding, each base64url.
    public struct StationIdentityClaim: Sendable, Equatable {
        public var publicKey: String
        public var certBinding: String

        public init(publicKey: String, certBinding: String) {
            self.publicKey = publicKey
            self.certBinding = certBinding
        }
    }

    /// A paired device's sign-in block (section 3.5). Its strings are the
    /// sign-in's to judge, not the codec's.
    public struct DeviceBlock: Sendable, Equatable {
        public var id: String
        public var publicKey: String
        public var name: String
        public var kind: String
        public var signature: String
        /// The device's short name, outside the signed transcript; written
        /// only when it is not empty.
        public var shortName: String?

        public init(id: String, publicKey: String, name: String, kind: String, signature: String,
                    shortName: String? = nil) {
            self.id = id
            self.publicKey = publicKey
            self.name = name
            self.kind = kind
            self.signature = signature
            self.shortName = shortName
        }
    }

    public struct AuthRequest: Sendable, Equatable {
        public var token: String
        /// A paired device's sign-in, sent with `token` `""`.
        public var device: DeviceBlock?

        public init(token: String, device: DeviceBlock? = nil) {
            self.token = token
            self.device = device
        }
    }

    /// A one-use secret for joining the verified new control connection.
    /// Keep it out of diagnostic descriptions as with an auth token.
    public struct PathJoin: Sendable, Equatable, CustomStringConvertible, CustomDebugStringConvertible {
        public var ticket: String

        public init(ticket: String) {
            self.ticket = ticket
        }

        public var description: String { "PathJoin(ticket: <redacted>)" }
        public var debugDescription: String { description }
    }

    public struct AuthResult: Sendable, Equatable {
        public var accepted: Bool
        public var reason: String
        public var retryable: Bool?
        /// The end's stable code (section 12.4), when the Core sends one.
        public var code: String?

        public init(accepted: Bool, reason: String, retryable: Bool? = nil, code: String? = nil) {
            self.accepted = accepted
            self.reason = reason
            self.retryable = retryable
            self.code = code
        }
    }

    public struct Capabilities: Sendable, Equatable {
        public var properties: [PropertyEntry]

        public init(properties: [PropertyEntry]) {
            self.properties = properties
        }
    }

    public struct SessionEnd: Sendable, Equatable {
        public var reason: String
        public var retryable: Bool?
        /// The end's stable code (section 12.4), when the Core sends one.
        public var code: String?
        public var takenOverBy: String?
        public var takenOverById: String?
        public var secondsAgo: Int64?

        public init(reason: String, retryable: Bool? = nil, code: String? = nil,
                    takenOverBy: String? = nil, takenOverById: String? = nil, secondsAgo: Int64? = nil) {
            self.reason = reason
            self.retryable = retryable
            self.code = code
            self.takenOverBy = takenOverBy
            self.takenOverById = takenOverById
            self.secondsAgo = secondsAgo
        }
    }

    public struct SessionHeld: Sendable, Equatable {
        public struct Slice: Sendable, Equatable {
            public var sliceId: Int64?
            public var letter: String
            public var frequencyHz: Double
            public var mode: UInt16
            public var band: UInt16

            public init(sliceId: Int64? = nil, letter: String, frequencyHz: Double, mode: UInt16, band: UInt16) {
                self.sliceId = sliceId
                self.letter = letter
                self.frequencyHz = frequencyHz
                self.mode = mode
                self.band = band
            }
        }

        public struct Device: Sendable, Equatable {
            public enum State: String, Sendable { case away, listening, transmitting }
            public var deviceId: String
            public var name: String
            public var shortName: String
            public var kind: String
            public var state: State
            public var from: String
            public var replaceable: Bool
            public var holdsTransmit: Bool
            public var lastActivitySeconds: Int64
            public var connectedForSeconds: Int64
            public var awayForSeconds: Int64
            public var transmittingForSeconds: Int64
            public var listeningOn: [Slice]
            public var transmittingOn: Slice?

            public init(deviceId: String, name: String, shortName: String, kind: String, state: State,
                        from: String, replaceable: Bool, holdsTransmit: Bool, lastActivitySeconds: Int64,
                        connectedForSeconds: Int64, awayForSeconds: Int64, transmittingForSeconds: Int64,
                        listeningOn: [Slice], transmittingOn: Slice? = nil) {
                self.deviceId = deviceId
                self.name = name
                self.shortName = shortName
                self.kind = kind
                self.state = state
                self.from = from
                self.replaceable = replaceable
                self.holdsTransmit = holdsTransmit
                self.lastActivitySeconds = lastActivitySeconds
                self.connectedForSeconds = connectedForSeconds
                self.awayForSeconds = awayForSeconds
                self.transmittingForSeconds = transmittingForSeconds
                self.listeningOn = listeningOn
                self.transmittingOn = transmittingOn
            }
        }

        public struct PlaceTaken: Sendable, Equatable {
            public var byName: String
            public var byId: String
            public var secondsAgo: Int64

            public init(byName: String, byId: String, secondsAgo: Int64) {
                self.byName = byName
                self.byId = byId
                self.secondsAgo = secondsAgo
            }
        }

        public struct PlaceFreed: Sendable, Equatable {
            public var secondsAgo: Int64

            public init(secondsAgo: Int64) { self.secondsAgo = secondsAgo }
        }

        public var devices: [Device]
        public var revision: UInt32
        public var placeTaken: PlaceTaken?
        public var placeFreed: PlaceFreed?

        public init(devices: [Device], revision: UInt32, placeTaken: PlaceTaken? = nil,
                    placeFreed: PlaceFreed? = nil) {
            self.devices = devices
            self.revision = revision
            self.placeTaken = placeTaken
            self.placeFreed = placeFreed
        }
    }

    public struct SessionTakeover: Sendable, Equatable {
        public var deviceId: String
        public var revision: UInt32

        public init(deviceId: String, revision: UInt32) {
            self.deviceId = deviceId
            self.revision = revision
        }
    }

    // MARK: Pairing (section 3.6)

    /// The device's details as `pair.start` carries them.
    public struct PairDevice: Sendable, Equatable {
        public var publicKey: String
        public var name: String
        public var kind: String

        public init(publicKey: String, name: String, kind: String) {
            self.publicKey = publicKey
            self.name = name
            self.kind = kind
        }
    }

    public struct PairStart: Sendable, Equatable {
        /// How the device pairs.
        public enum Mode: String, Sendable, CaseIterable {
            case lan
            case code
        }

        public var mode: Mode
        public var device: PairDevice

        public init(mode: Mode, device: PairDevice) {
            self.mode = mode
            self.device = device
        }
    }

    public struct PairAccept: Sendable, Equatable {
        public var identity: StationIdentityClaim
        public var label: String

        public init(identity: StationIdentityClaim, label: String) {
            self.identity = identity
            self.label = label
        }
    }

    public struct PairSpake: Sendable, Equatable {
        /// 0 to 3.
        public var step: Int
        public var data: String

        public init(step: Int, data: String) {
            self.step = step
            self.data = data
        }
    }

    public struct PairConfirm: Sendable, Equatable {
        public var box: String

        public init(box: String) {
            self.box = box
        }
    }

    public struct PairFail: Sendable, Equatable {
        public var reason: String
        /// 0 to 2147483647.
        public var retryAfterMs: Int

        public init(reason: String, retryAfterMs: Int) {
            self.reason = reason
            self.retryAfterMs = retryAfterMs
        }
    }

    // MARK: Objects and properties (section 7)

    public struct Schema: Sendable, Equatable {
        public var className: String
        public var fields: [SchemaField]

        public init(className: String, fields: [SchemaField]) {
            self.className = className
            self.fields = fields
        }
    }

    public struct ObjectCreate: Sendable, Equatable {
        public var key: String
        public var className: String
        public var properties: [PropertyEntry]

        public init(key: String, className: String, properties: [PropertyEntry]) {
            self.key = key
            self.className = className
            self.properties = properties
        }
    }

    public struct ObjectDestroy: Sendable, Equatable {
        public var key: String
        public var className: String

        public init(key: String, className: String) {
            self.key = key
            self.className = className
        }
    }

    public struct Delta: Sendable, Equatable {
        public var key: String
        public var properties: [PropertyEntry]

        public init(key: String, properties: [PropertyEntry]) {
            self.key = key
            self.properties = properties
        }
    }

    public struct PropertyWrite: Sendable, Equatable {
        public var key: String
        /// Nonzero when the client asks for a `property.result`.
        public var writeId: UInt32?
        public var properties: [PropertyEntry]

        public init(key: String, writeId: UInt32? = nil, properties: [PropertyEntry]) {
            self.key = key
            self.writeId = writeId
            self.properties = properties
        }
    }

    public struct PropertyResult: Sendable, Equatable {
        public struct Result: Sendable, Equatable {
            public var property: String
            public var accepted: Bool
            public var reason: String
            /// The value the station kept, when it has one.
            public var value: PropertyEntry?

            public init(property: String, accepted: Bool, reason: String, value: PropertyEntry?) {
                self.property = property
                self.accepted = accepted
                self.reason = reason
                self.value = value
            }
        }

        public var key: String
        public var writeId: UInt32
        public var results: [Result]

        public init(key: String, writeId: UInt32, results: [Result]) {
            self.key = key
            self.writeId = writeId
            self.results = results
        }
    }

    // MARK: The settings proxy (section 8)

    public struct SettingsSnapshot: Sendable, Equatable {
        public var properties: [PropertyEntry]

        public init(properties: [PropertyEntry]) {
            self.properties = properties
        }
    }

    public struct SettingsWrite: Sendable, Equatable {
        public var key: String
        public var origin: String
        public var properties: [PropertyEntry]

        public init(key: String, origin: String, properties: [PropertyEntry]) {
            self.key = key
            self.origin = origin
            self.properties = properties
        }
    }

    public struct SettingsRemove: Sendable, Equatable {
        public var key: String
        public var properties: [PropertyEntry]

        public init(key: String, properties: [PropertyEntry] = []) {
            self.key = key
            self.properties = properties
        }
    }

    public struct SettingsValue: Sendable, Equatable {
        public var key: String
        public var origin: String
        /// Empty when the key was removed.
        public var properties: [PropertyEntry]

        public init(key: String, origin: String, properties: [PropertyEntry]) {
            self.key = key
            self.origin = origin
            self.properties = properties
        }
    }

    public struct SettingsReject: Sendable, Equatable {
        public var key: String
        /// The station's own value, when it has one.
        public var properties: [PropertyEntry]
        public var reason: String?

        public init(key: String, properties: [PropertyEntry], reason: String? = nil) {
            self.key = key
            self.properties = properties
            self.reason = reason
        }
    }

    // MARK: Commands (section 9)

    public struct CommandInvoke: Sendable, Equatable {
        public var verb: String
        public var id: UInt32
        public var args: [PropertyEntry]

        public init(verb: String, id: UInt32, args: [PropertyEntry]) {
            self.verb = verb
            self.id = id
            self.args = args
        }
    }

    public struct CommandResult: Sendable, Equatable {
        public var verb: String
        public var id: UInt32
        public var accepted: Bool
        public var reason: String
        public var affected: [String]
        public var values: [PropertyEntry]?

        public init(verb: String, id: UInt32, accepted: Bool, reason: String, affected: [String],
                    values: [PropertyEntry]? = nil) {
            self.verb = verb
            self.id = id
            self.accepted = accepted
            self.reason = reason
            self.affected = affected
            self.values = values
        }
    }

    // MARK: Several devices on one Core (section 7.5)

    /// A question the Core asks before a change that reaches another
    /// device, or before a take (`sessionHolderVersion` 1). The nested
    /// values are kept as the Core sent them; `SeveralDevices` in the
    /// mirror reads them as the link document describes them.
    public struct ConfirmRequest: Sendable, Equatable {
        public var id: Int64
        /// `sharedSetting`, `panMove`, `takeReceiver`, `takeSlice` or
        /// `takeTransmit`; a kind this app does not know is kept as sent.
        public var kind: String
        public var reason: String
        /// One entry per device the change reaches.
        public var affected: [LinkJSON]
        public var expiresInMs: Int64
        /// `{label, from, to}`; absent for a take.
        public var change: [String: LinkJSON]?
        /// A take's list, one entry per receiver or slice.
        public var choices: [LinkJSON]?
        public var forCommandId: Int64?
        public var forWriteId: Int64?
        public var forSettingsKey: String?
        /// A `takeTransmit` question's holder, in the shape of a
        /// `connectedDevices` entry with its `source` and `keyed`.
        public var holder: [String: LinkJSON]?

        public init(id: Int64, kind: String, reason: String, affected: [LinkJSON], expiresInMs: Int64,
                    change: [String: LinkJSON]? = nil, choices: [LinkJSON]? = nil, forCommandId: Int64? = nil,
                    forWriteId: Int64? = nil, forSettingsKey: String? = nil, holder: [String: LinkJSON]? = nil) {
            self.id = id
            self.kind = kind
            self.reason = reason
            self.affected = affected
            self.expiresInMs = expiresInMs
            self.change = change
            self.choices = choices
            self.forCommandId = forCommandId
            self.forWriteId = forWriteId
            self.forSettingsKey = forSettingsKey
            self.holder = holder
        }
    }

    /// What another device did that reached this one, or what happened to
    /// this device's own place and slices (`sessionHolderVersion` 1).
    public struct Notice: Sendable, Equatable {
        public var id: Int64
        public var kind: String
        public var reason: String
        /// Whole seconds since it happened, measured when the Core sent it.
        public var secondsAgo: Int64
        public var takeBack: Bool
        public var byDeviceId: String?
        public var byName: String?
        public var byShortName: String?
        public var byKind: String?
        /// `device`, or `radioPtt` when the radio's own PTT did it.
        public var bySource: String?
        /// `[{sliceId, letter, frequencyHz, mode, band}]`, closed ones included.
        public var slices: [LinkJSON]?
        /// `{label, from, to}` of a `settingChanged`.
        public var change: [String: LinkJSON]?

        public init(id: Int64, kind: String, reason: String, secondsAgo: Int64, takeBack: Bool,
                    byDeviceId: String? = nil, byName: String? = nil, byShortName: String? = nil,
                    byKind: String? = nil, bySource: String? = nil, slices: [LinkJSON]? = nil,
                    change: [String: LinkJSON]? = nil) {
            self.id = id
            self.kind = kind
            self.reason = reason
            self.secondsAgo = secondsAgo
            self.takeBack = takeBack
            self.byDeviceId = byDeviceId
            self.byName = byName
            self.byShortName = byShortName
            self.byKind = byKind
            self.bySource = bySource
            self.slices = slices
            self.change = change
        }
    }

    // MARK: Record streams (section 7.7)

    /// One `record.batch`: a stream's upserts and removes, or with `reset`
    /// the newest records that replace the peer's copy.
    public struct RecordBatch: Sendable, Equatable {
        /// One record: its `id`, unique in its stream, and its fields as sent.
        public struct Record: Sendable, Equatable {
            public var id: String
            public var fields: [String: LinkJSON]

            public init(id: String, fields: [String: LinkJSON]) {
                self.id = id
                self.fields = fields
            }
        }

        public var stream: String
        public var generation: Int64
        public var reset: Bool
        public var upserts: [Record]
        public var removes: [String]

        public init(stream: String, generation: Int64, reset: Bool, upserts: [Record], removes: [String]) {
            self.stream = stream
            self.generation = generation
            self.reset = reset
            self.upserts = upserts
            self.removes = removes
        }
    }

    // MARK: Telemetry and media control (sections 10 and 11)

    /// One `station.metrics.v1` sample; its payload is kept as it arrived.
    public struct StationMetrics: Sendable, Equatable {
        public var payload: [String: LinkJSON]

        public init(payload: [String: LinkJSON]) {
            self.payload = payload
        }
    }

    /// One media control operation; its payload is kept as it arrived, for
    /// the media layer to read.
    public struct MediaControl: Sendable, Equatable {
        public var payload: [String: LinkJSON]

        public init(payload: [String: LinkJSON]) {
            self.payload = payload
        }
    }
}
