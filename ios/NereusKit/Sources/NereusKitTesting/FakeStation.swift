// NereusSDR for iOS: a fake Core in the same process, played from the link's conformance suite
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import NereusMedia

/// A Core for the app's tests, in the same process. Each connection a
/// session dials plays one session fixture's station side from the link's
/// conformance suite (by default `session-connect-connectable`: a signed-in
/// session with a connected radio, one slice and a complete snapshot). At
/// each of the fixture's client steps it waits for the app to send that
/// kind of message; an `auth.request` must carry ``token``, or a device
/// block that proves itself over the fake's challenge (the fake takes any
/// device key as paired), or the fake refuses the sign-in and closes. Its
/// `hello` carries a test identity made at run time, so a session can
/// trust the fake by ``certificateSHA256`` (``trust``) or by its identity
/// key (``identityTrust``). The fixture is filled once, so every
/// connection to one fake sees the same challenge. After the fixture's last step the
/// connection stays up: pings are answered, what the app sends is
/// recorded, and a test can ``deliver(_:)`` further messages or
/// ``dropLink()``.
///
/// It pairs as a Core does (link document section 3.6): a connection
/// dialled under the pairing trust plays the Core's side of one tap or of
/// the code, which the fake makes at run time (``pairingCode``). Like the
/// Core, it takes the code at step 1 for one exchange at a time, burns it on
/// a wrong code or when that exchange's connection ends early, and shows the
/// next one only after a wait of 5 s, doubling after each burn in a row,
/// the fifth closing pairing (``endPairingWait()`` ends a wait at once).
/// Pairing is open until the first device pairs; ``openPairing()`` opens it
/// for one more. A fake made with `requiresPairing` admits a device sign-in only
/// from a key paired with it, as a Core does.
///
/// Made with a `stationLabel`, it keeps a name as a Core does (link
/// document sections 8.2 and 9.1): its snapshot carries the `devices`
/// object with that name (`""` for a Core with none), and it takes
/// `station.rename` while it advertises `deviceAdminVersion` 1, refusing a
/// name outside the Core's rule in the Core's words, then sending the new
/// name in a `delta`; the next connection's snapshot carries it too. Made
/// `withoutCapabilities`, it plays a Core that sends neither those
/// capabilities nor what they gate.
///
/// It can also play a newer Core than its fixture (``Additions``): the
/// catalogue's `bands`, the verbs `slice.selectBand` and `notch.addAtSlice`,
/// the media control operation `clarity-retune`, media with the extended
/// view, and `session.leave` (after which it closes the connection), each served only when the fake is made with it, and
/// advertised by the capability the Core gates it with. With media, a
/// session's media client makes its peer with ``mediaPeerFactory``; the fake
/// answers each `subscribe` with its `context` (with `wideband` when the
/// subscription asks for the extended view) and sends display rows across
/// the accepted span. Without an addition the fake answers its verb as an older
/// Core answers any verb it does not know.
///
/// The fixtures are read from the checkout, never bundled; this library is
/// linked by the app's tests only.
public final class FakeStation: @unchecked Sendable {
    /// What a newer Core adds, beyond the fixture's Core.
    public struct Additions: OptionSet, Sendable {
        public let rawValue: Int

        public init(rawValue: Int) {
            self.rawValue = rawValue
        }

        /// The catalogue's `bands` (``catalogue(_:)`` adds them).
        public static let bands = Additions(rawValue: 1 << 0)
        /// `slice.selectBand`, with `bandSelectVersion` 1.
        public static let bandSelect = Additions(rawValue: 1 << 1)
        /// `notch.addAtSlice`, with `notchControlVersion` 2.
        public static let notchAtSlice = Additions(rawValue: 1 << 2)
        /// The media control operation `clarity-retune`, with `displayExtrasVersion` 2.
        public static let clarityRetune = Additions(rawValue: 1 << 3)
        /// Media with the extended view: `remoteMediaVersion` 1 and
        /// `remoteWidebandDisplayVersion` 1.
        public static let wideband = Additions(rawValue: 1 << 4)
        public static let all: Additions = [.bands, .bandSelect, .notchAtSlice, .clarityRetune, .wideband]
        /// `session.leave`, with `sessionHolderVersion` 1: the accepted
        /// result, then the Core closes the connection. Not in ``all``: the
        /// rest of what `sessionHolderVersion` gates is not played.
        public static let sessionLeave = Additions(rawValue: 1 << 5)
        /// Remote transmit, as a Core that lets this device transmit:
        /// `remoteTxVersion` 1 with `txPermitted` true and no refusal, and
        /// `txStateVersion` 2; the keying verbs played as the Core's
        /// RemoteKeying plays them for one device (``keyed``), and
        /// `tx.keepalive` accepted. Nothing reaches a radio. Not in ``all``.
        public static let remoteTx = Additions(rawValue: 1 << 6)
        /// With ``remoteTx``: `remoteTxVersion` 2 and `tx.tunerTune`, the
        /// Tuner Genius's autotune, played as one more key of the device's
        /// (refused on the air, as RemoteKeying::tunerTune refuses it). Not
        /// in ``all``.
        public static let tunerTune = Additions(rawValue: 1 << 60)
        /// The amplifier's and tuner's OPERATE, `setPgxlOperate` at
        /// `remotePgxlControlVersion` 4 and `setTgxlOperate` at
        /// `remoteTgxlControlVersion` 2, each accepted and changing
        /// nothing. Not in ``all``.
        public static let accessoryOperate = Additions(rawValue: 1 << 7)
        /// Setup descriptions: `setupDescriptionVersion` 11; the `setup`
        /// object follows ``deliverSetup(_:)``. Not in ``all``.
        public static let setupDescription = Additions(rawValue: 1 << 12)
        /// The transmit display with display duplex: `txDisplayVersion` 3.
        /// A media start that declares it gets `transmit` (false: the fake
        /// has no transmitter) in every context it sends. Not in ``all``.
        public static let txDisplay = Additions(rawValue: 1 << 8)
        /// The seven transmit stage readings on `txState` (`eqDb` to
        /// `alcGroupDb`): `txReadingsVersion` 3, sent right after
        /// `txStateVersion` and only with ``remoteTx``. The test delivers the
        /// readings themselves. Not in ``all``.
        public static let txStageReadings = Additions(rawValue: 1 << 26)
    }

    /// The band grid the fake's catalogue carries with ``Additions/bands``:
    /// the grid the suite's catalogues carry, in the shape the Core sends,
    /// `id` the slice's `band` value (a test holds the two equal).
    public static let bands: [(id: Int, label: String)] = [
        (0, "160"), (1, "80"), (2, "60"), (3, "40"), (4, "30"), (5, "20"),
        (6, "17"), (7, "15"), (8, "12"), (9, "10"), (10, "6"), (12, "WWV"),
    ]
    /// 2 m's button, after 6 m, in the grid of a Core that sends
    /// `band2mVersion` 1 (``Additions/band2m``; link document section 7.4).
    public static let band2mButton = (id: 27, label: "2")

    /// The grid this fake's catalogue carries: ``bands``, with 2 m after
    /// 6 m when made with ``Additions/band2m``.
    public static func bandGrid(band2m: Bool) -> [(id: Int, label: String)] {
        guard band2m, let sixMetres = bands.firstIndex(where: { $0.id == 10 }) else {
            return bands
        }
        var grid = bands
        grid.insert(band2mButton, at: sixMetres + 1)
        return grid
    }
    /// The verbs and operation the additions serve.
    public static let selectBandVerb = "slice.selectBand"
    public static let addNotchAtSliceVerb = "notch.addAtSlice"
    public static let clarityRetuneOp = "clarity-retune"
    public static let sessionLeaveVerb = "session.leave"
    /// The Core's refusal of `session.leave` with arguments (the suite's `verbs-session-leave`).
    public static let sessionLeaveNotUnderstoodReason = "The request to leave the Core was not understood."
    /// What an older Core answers to a verb it does not know (the suite's `unknown-verb`).
    public static let unknownVerbReason = "The Core does not know this request. Updating the Core may help."
    /// The fake's own refusals of the added verbs.
    public static let unknownSliceReason = "The fake Core has no such slice."
    public static let unknownBandReason = "The fake Core's radio cannot use this band."
    /// The `SliceModel` schema's ordinal of `band`.
    static let bandOrdinal: UInt16 = 14

    /// The fixture a fake plays unless told otherwise.
    public static let defaultFixture = "session-connect-connectable"

    /// The reason the fake gives when the app signs in with another token.
    public static let wrongTokenReason = "The Core did not accept this sign-in."
    /// The reason the fake gives when a device block does not prove itself.
    public static let deviceProofFailedReason = "This device could not prove it is paired with this Core."
    /// The reason the fake gives when a device it has not paired signs in.
    public static let deviceNotPairedReason = "This device is not paired with this Core. Pair it first."

    // The Core's pairing refusals (link document section 3.6).
    public static let cannotPairReason = "This Core cannot pair new devices."
    public static let unreadableDeviceReason = "The Core could not read this device's details. Update this app."
    public static let windowClosedReason =
        "This Core is not taking new devices. Open pairing on the Core or on a paired device first."
    public static let alreadyPairedReason = "This device is already paired with this Core. Connect to it instead."
    public static let oneTapClaimedReason =
        "One tap pairs only a Core with no paired devices. Use the pairing code the Core shows."
    public static let oneTapDeniedReason = "This Core pairs only with its code. Use the pairing code the Core shows."
    public static let oneTapOffNetworkReason =
        "One tap works only on the Core's own network. Use the pairing code the Core shows."
    public static let malformedStep1Reason =
        "The pairing code was not accepted. A new code will appear on the Core."
    public static let wrongCodeReason = "The pairing code was not right. A new code will appear on the Core."
    public static let anotherDeviceReason = "Another device is pairing with this Core right now. Try again shortly."
    public static let waitingReason =
        "The Core is waiting before it shows a new pairing code. Try again when the new code appears."
    public static let codeChangedReason = "The pairing code changed. Enter the code the Core shows now."
    /// How long "Another device is pairing" asks the app to wait.
    public static let anotherDeviceRetryMs = 5000
    public static let cannotSaveReason = "The Core could not save this device. Try again."
    /// The wait after the first burned code; it doubles after each more.
    public static let firstRetryMs = 5000
    /// Burned codes in a row that close pairing.
    public static let maxConsecutiveFailures = 5

    /// One step of the fixture, as the fake plays it.
    private enum Step {
        /// A station message, filled and encoded.
        case station(String)
        /// The app must send a message of this kind.
        case client(LinkMessage.Kind)
        /// The `devices` object, made as it is played, with the Core's name then.
        case devices
    }

    /// Where the fake answers; each fake has its own, so pairings with
    /// different fakes never wait on each other.
    public let endpoint = StationEndpoint(host: "fake-core-\(UUID().uuidString.lowercased()).invalid")
    /// The certificate digest the fake presents; ``trust`` pins it.
    public let certificateSHA256: Data
    /// The token the fake accepts, chosen at run time.
    public let token: String
    /// The Core identity in the fake's `hello`, made at run time.
    public let identity: TestStationIdentity
    /// The challenge in the fake's `hello`, if the fixture's has one.
    public let challenge: Data?
    /// The fixture this fake plays.
    public let fixtureId: String
    /// The Core's label, in its pairing answers.
    public let label = "Fake Core"
    /// Whether a device sign-in must come from a key paired with the fake.
    public let requiresPairing: Bool
    /// What this fake adds beyond its fixture's Core.
    public let additions: Additions

    private let steps: [Step]
    let lock = NSLock()
    private var connections: [FakeStationConnection] = []
    private var received: [LinkMessage] = []
    private var unreadable: [String] = []
    private let waiters = ConditionWaiters()
    private var pairingConnections: [FakePairingConnection] = []
    private var code = FakeStation.newCode()
    private var codesOnMailboxNumbers = false
    private var pairingOpen = true
    private var oneTap = true
    private var onCoresNetwork = true
    private var failures = 0
    private var paired: [Data] = []
    /// The pairing connection whose exchange holds the code, from its step 1.
    private var codeHolder: ObjectIdentifier?
    /// Until when no code is shown, after a burned one.
    private var waitUntil: ContinuousClock.Instant?
    /// The slice indexes the fixture's snapshot creates.
    private let sliceIds: Set<Int64>
    /// Refusals to give the next time each verb or operation comes.
    private var refusals: [String: String] = [:]
    /// The link document's refusal code (section 18.3) each queued keying
    /// refusal carries, where the test gave one.
    private var refusalCodes: [String: String] = [:]
    /// The Core's `BandPlanName`; see `FakeStation+BandPlan.swift`.
    let bandPlanSetting = BandPlanSetting()
    /// The notches `notch.addAtSlice` added, and the notch list's revision.
    private var notchCount: Int64 = 0
    /// The Setup panels' notches, antennas and settings check (``Additions/setupPanels``).
    private var setupPanelState = SetupPanelState()
    /// The last keying epoch the fake handed out, this device's key while
    /// it is on (``Additions/remoteTx``), and each keying command's answer,
    /// by id, for its copies.
    private var txEpoch: Int64 = 0
    private var liveKey: (epoch: Int64, kind: String)?
    /// Another device that holds transmit at the fake Core, by name
    /// (``otherDeviceTakes(_:)``), or nil.
    private var otherHolder: String?
    private var keyingAnswers: [UInt32: KeyingOutcome] = [:]
    /// The Core's name, while the fake keeps one (``stationLabel``).
    private var coreLabel: String?
    /// The `devices` object's revision, moved by each rename.
    private var devicesRevision: Int64 = 1
    /// `devices`' `coreAddresses`, sent with ``Additions/coreAddresses``.
    var coreAddressesJson = FakeStation.noCoreAddresses
    /// The fake takes the Core's device commands: it advertises `deviceAdminVersion` 1.
    private let takesDeviceCommands: Bool
    /// The last revision the app subscribed each display endpoint at.
    private var endpointRevisions: [LinkJSON: LinkJSON] = [:]
    /// The newest media start declared `txDisplayVersion`: its contexts carry `transmit`.
    private var txDisplayDeclared = false
    /// The media peers sessions made, newest last.
    private var peers: [FakeMediaPeer] = []
    /// The display endpoint the fake last accepted, and its rows' sequence.
    private var display: AcceptedDisplay?
    private var extendedViewAvailable = true
    /// The accessories' state (``Additions/accessories``, FakeStation+Accessories.swift).
    let accessoryState = AccessoryState()
    /// The spots and spot sources (``Additions/spots``, FakeStation+Spots.swift).
    let spotState = SpotState()
    /// The AM Mod Monitor's streams (``Additions/modMonitor``, FakeStation+ModMonitor.swift).
    let modMonitorState = ModMonitorState()
    /// The transmit monitor's state (``Additions/monitorAudio``).
    let monitorAudioState = MonitorAudioState()
    /// The per-device audio quality's state (``Additions/audioQuality``).
    let audioQualityState = AudioQualityState()
    /// FreeDV Reporter's list, console and state (FakeStation+FreeDV.swift).
    let freedvState = FreeDVState()
    /// The TCI server and its apps (``Additions/stationTci``, FakeStation+StationTools.swift).
    let stationToolsState = StationToolsState()
    /// The VAX channels' scene and whether a device follows their meters.
    let vaxState = VaxState()
    /// The radios the Core can see (``Additions/stationRadios``, FakeStation+StationRadios.swift).
    let stationRadiosState = StationRadiosState()

    /// A display endpoint as the fake accepted it.
    private struct AcceptedDisplay {
        let endpointId: UInt32
        let generation: UInt32
        let centreHz: Double
        let spanHz: Double
        let pixels: Int
        let minDbm: Double
        let maxDbm: Double
        var sequence: UInt32
    }

    /// The receiver's sample rate the fake's contexts report (the fixture's slice).
    public static let ddcRateHz = 192_000.0
    /// The ADC rate the fake's wideband object reports; its half is the
    /// extended view's widest span.
    public static let adcRateHz = 122_880_000.0

    /// A fake that plays the session fixture `fixtureId`. Throws when the
    /// suite has no such fixture or it does not read as the link document
    /// describes. With `requiresPairing`, only a device key paired with the
    /// fake signs in.
    public init(fixture fixtureId: String = FakeStation.defaultFixture, requiresPairing: Bool = false,
                additions: Additions = [], stationLabel: String? = nil,
                withoutCapabilities: Set<String> = []) throws {
        guard let fixture = try SessionFixtures.all().first(where: { $0.id == fixtureId }) else {
            throw LinkFixtureLoader.Malformed(description: "the suite has no session fixture \(fixtureId)")
        }
        let token = SessionFixturePlayer.randomToken()
        let certificateSHA256 = Data((0..<32).map { _ in UInt8.random(in: 0...255) })
        let identity = TestStationIdentity()
        var placeholders = FixturePlaceholders(mirrorClasses: try SessionFixtures.mirrorClasses(),
                                               stationIdentity: identity, certificateSHA256: certificateSHA256)
        placeholders.record["token"] = .string(token)

        var steps: [Step] = []
        let snapshotAdditions = try Self.wireBatchSnapshotAdditions(additions)
        var sliceIds: Set<Int64> = []
        var takesDeviceCommands = false
        for (index, step) in fixture.steps.enumerated() {
            let label = "\(fixtureId) step \(index)"
            // The fake plays the Core to the app alone: another client's
            // steps and the Core's messages to it are not the app's.
            if SessionFixturePlayer.isAnotherClients(step) {
                continue
            }
            guard let from = step["from"] as? String, var message = step["message"] else {
                // Time passing and expected closes belong to the conformance runner.
                continue
            }
            if from == "station", var object = message as? [String: Any] {
                if object["type"] as? String == "capabilities", let properties = object["properties"] as? [[String: Any]] {
                    let sent = Self.capabilities(properties, adding: additions, removing: withoutCapabilities)
                    object["properties"] = sent
                    message = object
                    takesDeviceCommands = sent.contains {
                        $0["name"] as? String == "deviceAdminVersion" && ($0["value"] as? Int ?? 0) >= 1
                    }
                }
                if object["type"] as? String == "snapshot.complete",
                   stationLabel != nil || additions.contains(.coreAddresses) {
                    // The devices object goes to an app that declares deviceAuth, as every app does.
                    let schema = try LinkJSON(foundation: ["type": "schema", "class": Self.devicesClass,
                                                           "fields": "$any"] as [String: Any])
                    let filled = try placeholders.fillStation(schema, at: "\(label) devices schema").compactText
                    steps.append(.station(try Self.devicesSchema(filled, additions: additions)))
                    steps.append(.devices)
                }
                object = try Self.withWireBatchProperties(object, added: snapshotAdditions)
                message = object
                if withoutCapabilities.contains(Self.freedvCapability) {
                    // A Core before FreeDV Reporter sends none of its spot source properties.
                    object = Self.withoutFreedvSourceProperties(object)
                    message = object
                }
                if object["type"] as? String == "object.create", object["class"] as? String == "SliceModel" {
                    let properties = object["properties"] as? [[String: Any]] ?? []
                    if let index = properties.first(where: { $0["name"] as? String == "sliceIndex" })?["value"] as? Int {
                        sliceIds.insert(Int64(index))
                    }
                }
            }
            let json = try LinkJSON(foundation: message)
            if from == "station" {
                let filled = try placeholders.fillStation(json, at: label)
                steps.append(.station(filled.compactText))
            } else {
                guard case .object(let object) = json, case .string(let type)? = object["type"],
                      let kind = LinkMessage.Kind(rawValue: type) else {
                    throw LinkFixtureLoader.Malformed(description: "\(label): a client message without a known type")
                }
                steps.append(.client(kind))
            }
        }
        self.fixtureId = fixtureId
        self.requiresPairing = requiresPairing
        self.additions = additions
        if additions.contains(.band2m) {
            setupPanelState.withBand2m()
        }
        self.sliceIds = sliceIds
        self.takesDeviceCommands = takesDeviceCommands
        coreLabel = stationLabel
        self.token = token
        self.steps = steps
        self.identity = identity
        self.certificateSHA256 = certificateSHA256
        challenge = placeholders.challenge
        freedvState.runs = !withoutCapabilities.contains(Self.freedvCapability)
    }

    // MARK: What the app connects with

    /// The trust that pins the fake's certificate.
    public var trust: StationTrust { .certificate(pinSHA256: certificateSHA256) }

    /// The trust that knows the fake by its identity key, as a paired Core.
    public var identityTrust: StationTrust { identity.trust }

    /// Signs in with the token the fake accepts.
    public var authenticator: TokenAuthenticator { TokenAuthenticator(token: token) }

    /// Whether the fake admits this sign-in: the right token, or a device
    /// block that proves itself; the refusal to send otherwise.
    func refusal(for request: LinkMessage.AuthRequest) -> LinkMessage.AuthResult? {
        guard let device = request.device else {
            return request.token == token ? nil
                : LinkMessage.AuthResult(accepted: false, reason: Self.wrongTokenReason, retryable: false)
        }
        let json = LinkCodec.json(.authRequest(LinkMessage.AuthRequest(token: "", device: device)))
        guard case .object(let fields) = json, let block = fields["device"], let challenge,
              DeviceBlockCheck.failure(block, challenge: challenge, certificateSHA256: certificateSHA256,
                                       stationKey: identity.publicKey) == nil else {
            return LinkMessage.AuthResult(accepted: false, reason: Self.deviceProofFailedReason, retryable: false,
                                          code: "deviceProofFailed")
        }
        if requiresPairing {
            let key = Base64URL.decode(device.publicKey) ?? Data()
            guard lock.withLock({ paired.contains(key) }) else {
                return LinkMessage.AuthResult(accepted: false, reason: Self.deviceNotPairedReason, retryable: false,
                                              code: "deviceNotPaired")
            }
        }
        return nil
    }

    /// Makes a connection to the fake for each attempt a session dials,
    /// and a pairing connection for each one dialled under the pairing trust.
    public var transportFactory: LinkTransportFactory {
        { [self] _, trust in
            if trust == .pairing {
                let connection = FakePairingConnection(station: self)
                lock.withLock { pairingConnections.append(connection) }
                return connection
            }
            let connection = FakeStationConnection(station: self)
            lock.withLock {
                connections.append(connection)
                // A new session: the Core unkeyed the last one's key when its
                // link went, and forgot its commands.
                liveKey = nil
                keyingAnswers = [:]
            }
            return connection
        }
    }

    /// Makes a connection to a stand-in for the remote access service for
    /// each one a pairing through the service opens: its mailbox reaches
    /// this fake's Core side of pairing (``FakeRendezvousMailbox``).
    public var rendezvousTransportFactory: RendezvousTransportFactory {
        { [self] _ in
            FakeRendezvousMailbox(station: self)
        }
    }

    /// The Core's side of one pairing through a mailbox: no `hello` either
    /// way. Counted with the pairing connections.
    func mailboxPairingConnection() -> FakePairingConnection {
        let connection = FakePairingConnection(station: self, mailbox: true)
        lock.withLock { pairingConnections.append(connection) }
        return connection
    }

    /// A session to the fake.
    public func makeSession(clock: any LinkClock = SystemLinkClock()) -> StationSession {
        StationSession(endpoint: endpoint, trust: trust, authenticator: authenticator, clock: clock,
                       transportFactory: transportFactory)
    }

    // MARK: Pairing

    /// The code a device pairs with now, normalised; made at run time, and a
    /// new one after each burned or used code. Empty while pairing is closed,
    /// while the Core waits after a burned code, and while an exchange holds
    /// the code, as the Core shows none then. Never print it.
    public var pairingCode: String {
        lock.withLock { pairingOpen && !isWaiting && codeHolder == nil ? code : "" }
    }

    /// Whether one tap may pair while no device is paired (the Core's
    /// `pairing_lan_click`); true unless set otherwise.
    public var allowsOneTap: Bool {
        get { lock.withLock { oneTap } }
        set { lock.withLock { oneTap = newValue } }
    }

    /// Whether the app reaches the fake from one of the Core's own networks,
    /// where one tap may pair; true unless set otherwise.
    public var reachedOnItsOwnNetwork: Bool {
        get { lock.withLock { onCoresNetwork } }
        set { lock.withLock { onCoresNetwork = newValue } }
    }

    /// The Bonjour TXT record the fake's Core registers now (link document
    /// section 14.2): `v`, `id`, `claimed`, `pair` and `name`, each after a
    /// one-byte length. `name` is the fake's label.
    public var bonjourRecord: Data {
        let (claimed, pair): (Bool, String) = lock.withLock {
            let claimed = !paired.isEmpty
            let pair = !pairingOpen ? "closed" : (!claimed && oneTap ? "click" : "code")
            return (claimed, pair)
        }
        var record = Data()
        for entry in ["v=1", "id=\(FoundStation.identityPrefix(of: identity.publicKey))",
                      "claimed=\(claimed ? 1 : 0)", "pair=\(pair)", "name=\(label)"] {
            let bytes = Data(entry.utf8)
            record.append(UInt8(bytes.count))
            record.append(bytes)
        }
        return record
    }

    /// The fake as a browser finds it now: its record read by the app's own
    /// reader, resolved to ``endpoint``.
    public var foundStation: FoundStation {
        var found = FoundStation.parse(txtRecord: bonjourRecord, instanceName: label)
            ?? FoundStation(instanceName: label, label: label, identityPrefix: "", claimed: true, pairing: .closed)
        found.endpoint = endpoint
        return found
    }

    /// The device keys paired with the fake, oldest first.
    public var pairedDeviceKeys: [Data] { lock.withLock { paired } }

    /// How many pairing connections have been dialled.
    public var pairingConnectionCount: Int { lock.withLock { pairingConnections.count } }

    /// Opens pairing for one more device, as the Core's console does:
    /// failures and the wait forgotten, a new code at once.
    public func openPairing() {
        lock.withLock {
            pairingOpen = true
            failures = 0
            waitUntil = nil
            codeHolder = nil
            code = Self.newCode(onMailboxNumber: codesOnMailboxNumbers)
        }
    }

    /// The wait after a burned code is over, as if its time had passed.
    public func endPairingWait() {
        lock.withLock { waitUntil = nil }
    }

    private var isWaiting: Bool {
        guard let waitUntil else {
            return false
        }
        return ContinuousClock.now < waitUntil
    }

    private var remainingWaitMs: Int {
        guard let waitUntil else {
            return 0
        }
        let left = (waitUntil - ContinuousClock.now).components
        return max(1, Int(left.seconds * 1000 + left.attoseconds / 1_000_000_000_000_000))
    }

    /// The refusal of a one-tap pairing from the device with `key`, or nil
    /// when one tap pairs. In the Core's order
    /// (src/core/session/StationServer.cpp:6829-6870): a device it already
    /// has, then a closed window, then an unclaimed Core only, one tap
    /// allowed, and the Core's own network.
    func oneTapRefusal(key: Data) -> LinkMessage.PairFail? {
        lock.withLock {
            if paired.contains(key) {
                return LinkMessage.PairFail(reason: Self.alreadyPairedReason, retryAfterMs: 0)
            }
            if !pairingOpen {
                return LinkMessage.PairFail(reason: Self.windowClosedReason, retryAfterMs: 0)
            }
            if !paired.isEmpty {
                return LinkMessage.PairFail(reason: Self.oneTapClaimedReason, retryAfterMs: 0)
            }
            if !oneTap {
                return LinkMessage.PairFail(reason: Self.oneTapDeniedReason, retryAfterMs: 0)
            }
            if !onCoresNetwork {
                return LinkMessage.PairFail(reason: Self.oneTapOffNetworkReason, retryAfterMs: 0)
            }
            return nil
        }
    }

    /// The code a code pairing's `pair.start` hashes, or the refusal: pairing
    /// closed, another exchange holding the code, or no code shown during
    /// the wait after a burned one.
    func codeForStart() -> Result<String, PairFailure> {
        lock.withLock {
            guard pairingOpen else {
                return .failure(PairFailure(LinkMessage.PairFail(reason: Self.windowClosedReason, retryAfterMs: 0)))
            }
            guard codeHolder == nil else {
                return .failure(PairFailure(LinkMessage.PairFail(reason: Self.anotherDeviceReason,
                                                                  retryAfterMs: Self.anotherDeviceRetryMs)))
            }
            guard !isWaiting else {
                return .failure(PairFailure(LinkMessage.PairFail(reason: Self.waitingReason,
                                                                  retryAfterMs: remainingWaitMs)))
            }
            return .success(code)
        }
    }

    /// Step 1 takes the code for `holder`'s exchange: nil when it now holds
    /// it, or the refusal. Another exchange holding it is refused without a
    /// burn; so is a code that changed since step 0.
    func takeCode(_ hashed: String, holder: ObjectIdentifier) -> LinkMessage.PairFail? {
        lock.withLock {
            if let codeHolder, codeHolder != holder {
                return LinkMessage.PairFail(reason: Self.anotherDeviceReason, retryAfterMs: Self.anotherDeviceRetryMs)
            }
            guard pairingOpen else {
                return LinkMessage.PairFail(reason: Self.windowClosedReason, retryAfterMs: 0)
            }
            guard !isWaiting, hashed == code else {
                return LinkMessage.PairFail(reason: Self.codeChangedReason, retryAfterMs: remainingWaitMs)
            }
            codeHolder = holder
            return nil
        }
    }

    /// The code is burned: a new one after the wait, and the Core's refusal
    /// with that wait; the fifth burn in a row closes pairing instead.
    func burn(reason: String) -> LinkMessage.PairFail {
        lock.withLock {
            codeHolder = nil
            failures += 1
            code = Self.newCode(onMailboxNumber: codesOnMailboxNumbers)
            if failures >= Self.maxConsecutiveFailures {
                pairingOpen = false
                waitUntil = nil
                return LinkMessage.PairFail(reason: reason, retryAfterMs: 0)
            }
            let waitMs = Self.firstRetryMs << (failures - 1)
            waitUntil = ContinuousClock.now + .milliseconds(waitMs)
            return LinkMessage.PairFail(reason: reason, retryAfterMs: waitMs)
        }
    }

    /// A code paired a device: the wait is reset and pairing closes.
    func codePaired() {
        lock.withLock {
            codeHolder = nil
            failures = 0
            waitUntil = nil
            pairingOpen = false
            code = Self.newCode(onMailboxNumber: codesOnMailboxNumbers)
        }
    }

    func addPairedDevice(_ key: Data) {
        lock.withLock {
            if !paired.contains(key) {
                paired.append(key)
            }
            pairingOpen = false
        }
        waiters.release()
    }

    /// A `pair.fail` as an error, for a `Result`.
    struct PairFailure: Error {
        let fail: LinkMessage.PairFail

        init(_ fail: LinkMessage.PairFail) {
            self.fail = fail
        }
    }

    /// A code as a Core makes one: a number from 1 to 99 and two words.
    /// The number range is ``MailboxNameplates/coreRange``, which the tests'
    /// other mailbox numbers stay above. With `onMailboxNumber`, the number
    /// is instead one no other pairing in the process can hold.
    private static func newCode(onMailboxNumber: Bool = false) -> String {
        let words = PairingCodeText.words
        guard let first = words.randomElement(), let second = words.randomElement() else {
            return ""
        }
        let number = onMailboxNumber ? MailboxNameplates.unshown() : Int.random(in: MailboxNameplates.coreRange)
        return "\(number)-\(first)-\(second)"
    }

    /// Shows this and every later code on a mailbox number no other pairing
    /// in the process can hold, in place of a Core's 1 to 99.
    ///
    /// A mailbox pairing claims `PairingClient`'s process-wide gate on its
    /// code's number for the whole pairing. Tests run in parallel in one
    /// process, so two fakes that show the same number from 1 to 99 would
    /// refuse each other's pairings as already pairing. A test that pairs
    /// through ``rendezvousTransportFactory`` calls this before it reads
    /// ``pairingCode``.
    public func showCodesOnMailboxNumbers() {
        lock.withLock {
            codesOnMailboxNumbers = true
            code = Self.newCode(onMailboxNumber: true)
        }
    }

    /// A mailbox number no fake Core shows and no other pairing in the
    /// process holds, for a code that must find no Core on the mailbox.
    /// The app's tests reach ``MailboxNameplates`` through this.
    public static func unshownNameplate() -> Int {
        MailboxNameplates.unshown()
    }

    // MARK: A newer Core

    /// The fixture's capabilities with what the additions advertise, less those removed.
    /// The suite's Core already advertises band select and the notch at a
    /// slice; a fake made without them lowers those capabilities to a Core
    /// before them, so it never advertises a verb it answers as unknown.
    private static func capabilities(_ properties: [[String: Any]], adding additions: Additions,
                                     removing removed: Set<String>) -> [[String: Any]] {
        var properties = properties.filter { !removed.contains($0["name"] as? String ?? "") }
        func set(_ name: String, _ value: Int) {
            let entry: [String: Any] = ["kind": "i64", "name": name, "ordinal": 0, "value": value]
            if let index = properties.firstIndex(where: { $0["name"] as? String == name }) {
                properties[index] = entry
            } else {
                properties.append(entry)
            }
        }
        func lower(_ name: String, to highest: Int) {
            if let index = properties.firstIndex(where: { $0["name"] as? String == name }),
               let value = properties[index]["value"] as? Int, value > highest {
                set(name, highest)
            }
        }
        if additions.contains(.notchAtSlice) {
            set("notchControlVersion", 2)
        } else {
            lower("notchControlVersion", to: 1)
        }
        if additions.contains(.clarityRetune) {
            set("displayExtrasVersion", 2)
        } else {
            lower("displayExtrasVersion", to: 1)
        }
        if additions.contains(.bandSelect) {
            // Appended after the last minor-11 capability, as the Core adds it.
            set("bandSelectVersion", 1)
        } else {
            properties.removeAll { $0["name"] as? String == "bandSelectVersion" }
        }
        if additions.contains(.wideband) {
            set("remoteMediaVersion", 1)
            set("remoteWidebandDisplayVersion", 1)
        }
        if additions.contains(.sessionLeave) {
            set("sessionHolderVersion", 1)
        }
        if additions.contains(.txDisplay) {
            set("txDisplayVersion", 3)
        }
        if additions.contains(.band2m) {
            set("band2mVersion", 1)
        }
        if additions.contains(.setupDescription) {
            set("setupDescriptionVersion", setupDescriptionVersion)
        }
        if additions.contains(.setupPanels) {
            set("settingsHygieneVersion", 2)
            set("radioAntennaRowsVersion", 1)
            set("stationTelemetryVersion", 4)
        }
        if additions.contains(.accessoryOperate) {
            set("remotePgxlControlVersion", 4)
            set("remoteTgxlControlVersion", 2)
        }
        for (name, value) in accessoryCapabilityVersions(additions) {
            set(name, value)
        }
        for (name, value) in stationToolCapabilityVersions(additions) {
            set(name, value)
        }
        for (name, value) in Self.monitorAudioCapabilityVersions(additions) where !removed.contains(name) {
            set(name, value)
        }
        for (name, value) in Self.audioQualityCapabilityVersions(additions) where !removed.contains(name) {
            set(name, value)
        }
        if additions.contains(.spots) {
            set("recordStreamVersion", additions.contains(.spotModes) ? 2 : 1)
        } else {
            // A fake without its spots plays a Core before record streams.
            lower("recordStreamVersion", to: 0)
        }
        if additions.contains(.modMonitor) {
            // The monitor rides the record streams (at least version 1).
            let streams = properties.first { $0["name"] as? String == "recordStreamVersion" }?["value"] as? Int
            set("recordStreamVersion", max(1, streams ?? 0))
            set("txModMonitorVersion", 1)
        } else if !additions.contains(.spots) {
            // A Core before record streams has no monitor either.
            lower("txModMonitorVersion", to: 0)
        }
        for (name, value) in stationRadioCapabilityVersions(additions) where !removed.contains(name) {
            set(name, value)
        }
        for (name, value) in wireBatchCapabilityVersions(additions) where !removed.contains(name) {
            set(name, value)
        }
        for (name, value) in coreAddressesCapabilityVersions(additions) where !removed.contains(name) {
            set(name, value)
        }
        for (name, value) in vaxCapabilityVersions(additions) where !removed.contains(name) {
            if value >= 1 {
                // The meters ride the record streams (at least version 1).
                let streams = properties.first { $0["name"] as? String == "recordStreamVersion" }?["value"] as? Int
                set("recordStreamVersion", max(1, streams ?? 0))
            }
            set(name, value)
        }
        // The suite's Core now advertises FreeDV Reporter at 2 (its stations'
        // bands); without ``Additions/freedvBand`` the fake plays one at 1.
        if additions.contains(.freedvBand), !removed.contains(freedvCapability) {
            set(freedvCapability, 2)
        } else {
            lower(freedvCapability, to: 1)
        }
        if additions.contains(.remoteTx) {
            properties.removeAll { ["txPermitted", "txRefusalCode", "txRefusalReason", "txRefusalFix"]
                .contains($0["name"] as? String ?? "") }
            properties.append(["kind": "bool", "name": "txPermitted", "ordinal": 0, "value": true])
            set("remoteTxVersion", additions.contains(.tunerTune) ? 2 : 1)
            for name in ["txRefusalCode", "txRefusalReason", "txRefusalFix"] {
                properties.append(["kind": "utf8", "name": name, "ordinal": 0, "value": ""])
            }
            set("txStateVersion", 2)
            if additions.contains(.txStageReadings) {
                set("txReadingsVersion", 3)
            }
        }
        return properties
    }

    /// A catalogue `json` as this fake's Core sends it: with ``bands`` when
    /// made with ``Additions/bands``, else without any (a Core before band
    /// select; the suite's catalogues carry the grid).
    public func catalogue(_ json: String) -> String {
        Self.catalogue(json, bands: additions.contains(.bands), band2m: additions.contains(.band2m))
    }

    /// A catalogue `json` with ``bands`` (a Core with band select) or without
    /// any grid (a Core before it).
    public static func catalogue(_ json: String, bands: Bool, band2m: Bool = false) -> String {
        guard var object = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [String: Any] else {
            return json
        }
        if bands {
            object["bands"] = Self.bandGrid(band2m: band2m).map { ["id": $0.id, "label": $0.label] as [String: Any] }
        } else {
            guard object.removeValue(forKey: "bands") != nil else {
                return json
            }
        }
        guard let data = try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]) else {
            return json
        }
        return String(decoding: data, as: UTF8.self)
    }

    /// The next `verb` (a command's verb, or a media control operation's
    /// `op`) the fake serves is refused with `reason`, as the Core refuses.
    public func refuseNext(_ verb: String, reason: String) {
        lock.withLock {
            refusals[verb] = reason
            refusalCodes[verb] = nil
        }
    }

    /// The next keying `verb` (`tx.key`, `tx.tune`, `tx.twoTone`) is
    /// refused with `reason` and the refusal code `code`, sent as the
    /// Core sends them: `refusalCode` and an empty `refusalFix` in the
    /// result's values (link document section 18.3).
    public func refuseNext(_ verb: String, reason: String, code: String) {
        lock.withLock {
            refusals[verb] = reason
            refusalCodes[verb] = code
        }
    }

    /// Reads and changes the Setup panels' state under the fake's lock.
    func withSetupPanels<Value>(_ body: (inout SetupPanelState) -> Value) -> Value {
        lock.withLock { body(&setupPanelState) }
    }

    /// A command a test sends to learn that the fake has read everything
    /// the app sent before it: the fake accepts it at once, and its answer
    /// comes back over the same ordered link.
    public static let barrierVerb = "test.barrier"

    func takeRefusal(_ verb: String) -> String? {
        lock.withLock {
            refusalCodes[verb] = nil
            return refusals.removeValue(forKey: verb)
        }
    }

    /// What the fake answers, once its fixture is played, to a message the
    /// app sent: the added verbs and operation, and a refusal queued by
    /// ``refuseNext(_:reason:)``. Anything else is left for the test.
    func replies(to message: LinkMessage) -> [LinkMessage] {
        switch message {
        case .commandInvoke(let invoke):
            return commandReplies(invoke)
        case .mediaControl(let control):
            return mediaReplies(control.payload)
        case .settingsWrite(let write):
            return modMonitorSettingReplies(write) ?? accessorySettingReplies(write) ?? settingsReplies(write)
        case .propertyWrite(let write):
            return vaxWriteReplies(write) ?? []
        default:
            return []
        }
    }

    /// Whether the fake closes the connection after answering `message`:
    /// an accepted `session.leave`, as the Core does (link section 9.1).
    func closesAfterReplying(to message: LinkMessage) -> Bool {
        guard case .commandInvoke(let invoke) = message, invoke.verb == Self.sessionLeaveVerb else {
            return false
        }
        return additions.contains(.sessionLeave) && invoke.args.isEmpty
    }

    private func commandReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage] {
        func result(_ accepted: Bool, _ reason: String = "", affected: [String] = [],
                    values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: affected, values: values))
        }
        func argument(_ name: String) -> Int64? {
            switch invoke.args.first(where: { $0.name == name })?.value {
            case .i64(let value)?, .enumeration(let value)?:
                return value
            default:
                return nil
            }
        }
        if invoke.verb == Self.barrierVerb {
            // Answered after everything the app sent before it.
            return [result(true)]
        }
        if let replies = accessoryReplies(invoke) {
            return replies
        }
        if let replies = stationRadioReplies(invoke) {
            return replies
        }
        if let replies = radioModelReplies(invoke) {
            return replies
        }
        if let replies = stationToolReplies(invoke) {
            return replies
        }
        if let replies = modMonitorReplies(invoke) {
            return replies
        }
        if let replies = vaxReplies(invoke) {
            return replies
        }
        if let replies = freedvReplies(invoke) {
            return replies
        }
        if let replies = spotReplies(invoke) {
            return replies
        }
        if let replies = setupPanelReplies(invoke) {
            return replies
        }
        let served: Bool
        switch invoke.verb {
        case Self.selectBandVerb:
            served = additions.contains(.bandSelect)
        case Self.addNotchAtSliceVerb:
            served = additions.contains(.notchAtSlice)
        case Self.sessionLeaveVerb:
            guard additions.contains(.sessionLeave) else {
                return [result(false, Self.unknownVerbReason)]
            }
            guard invoke.args.isEmpty else {
                return [result(false, Self.sessionLeaveNotUnderstoodReason)]
            }
            return [result(true)]
        case "setPgxlOperate", "setTgxlOperate":
            guard additions.contains(.accessoryOperate) else {
                return [result(false, Self.unknownVerbReason)]
            }
            return [result(true)]
        case "tx.key", "tx.unkey", "tx.tune", "tx.twoTone", "tx.tunerTune", "tx.keepalive":
            guard additions.contains(.remoteTx),
                  invoke.verb != "tx.tunerTune" || additions.contains(.tunerTune) else {
                return [result(false, Self.unknownVerbReason)]
            }
            return lock.withLock { () -> [KeyingOutcome] in
                // A queued refusal answers this command and, as the Core
                // answers them, every copy of it (link section 18.6).
                if keyingAnswers[invoke.id] == nil, let reason = refusals.removeValue(forKey: invoke.verb) {
                    let refused = KeyingOutcome.refused(reason: reason,
                                                        code: refusalCodes.removeValue(forKey: invoke.verb))
                    if invoke.verb != "tx.keepalive" {
                        keyingAnswers[invoke.id] = refused
                    }
                    return [refused]
                }
                return keying(invoke)
            }.map { outcome in
                switch outcome {
                case .accepted(let epoch):
                    return result(true, values: epoch.map { [.init(name: "epoch", value: .i64($0))] })
                case .keyEnded:
                    return result(false, Self.keyEndedReason,
                                  values: [.init(name: "refusalCode", value: .utf8("keyEnded")),
                                           .init(name: "refusalFix", value: .utf8(""))])
                case .refused(let reason, let code?):
                    return result(false, reason,
                                  values: [.init(name: "refusalCode", value: .utf8(code)),
                                           .init(name: "refusalFix", value: .utf8(""))])
                case .refused(let reason, nil):
                    return result(false, reason)
                case .otherDeviceHolds(let holder):
                    // TxRefusals::otherDeviceHoldsStop (src/core/safety/TxRefusal.cpp:217-225).
                    return result(false, holder + " has the transmitter. Take it to stop the transmission.",
                                  values: [.init(name: "refusalCode", value: .utf8("otherDeviceHolds")),
                                           .init(name: "refusalFix", value: .utf8("takeTransmit"))])
                }
            }
        case Self.renameVerb:
            guard takesDeviceCommands else {
                return [result(false, Self.unknownVerbReason)]
            }
            if let reason = takeRefusal(invoke.verb) {
                return [result(false, reason)]
            }
            return renameReplies(invoke)
        default:
            // Another verb answers only with a refusal a test queued.
            return takeRefusal(invoke.verb).map { [result(false, $0)] } ?? []
        }
        guard served else {
            return [result(false, Self.unknownVerbReason)]
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        guard let sliceId = argument("sliceId"), sliceIds.contains(sliceId) else {
            return [result(false, Self.unknownSliceReason)]
        }
        if invoke.verb == Self.selectBandVerb {
            guard let band = argument("band"),
                  Self.bandGrid(band2m: additions.contains(.band2m)).contains(where: { Int64($0.id) == band }) else {
                return [result(false, Self.unknownBandReason)]
            }
            let key = "slice:\(sliceId)"
            return [result(true, affected: [key]),
                    .delta(LinkMessage.Delta(key: key, properties: [
                        .init(ordinal: Self.bandOrdinal, name: "band", value: .enumeration(band)),
                    ]))]
        }
        let count = lock.withLock { () -> Int64 in
            notchCount += 1
            return notchCount
        }
        return [result(true, affected: ["notches"], values: [.init(name: "revision", value: .i64(count)),
                                                             .init(name: "id", value: .i64(count))])]
    }

    // MARK: Keying

    /// The Core's refusal of a copy of a key it has already ended (link 18.3).
    public static let keyEndedReason = "The Core already stopped this transmission. Key again to transmit."

    /// TxRefusals::radioOnAir (src/core/safety/TxRefusal.cpp:204-209).
    public static let onAirReason = "The radio is on the air. Try again when it stops."

    /// The kind of this device's key while it is on at the fake Core: its
    /// verb (`tx.key`, `tx.tune`, `tx.twoTone` or `tx.tunerTune`).
    public var keyedVerb: String? { lock.withLock { liveKey?.kind } }

    /// This device's key is on at the fake Core (``Additions/remoteTx``).
    public var keyed: Bool { lock.withLock { liveKey != nil } }

    /// The fake Core stops this device's key on its own, as its watchdog or
    /// time-out would; returns the key's epoch, or 0 with nothing on.
    @discardableResult
    public func stopKeyOnItsOwn() -> Int64 {
        lock.withLock {
            let epoch = liveKey?.epoch ?? 0
            liveKey = nil
            return epoch
        }
    }

    /// Another device takes transmit at the fake Core: this device's key
    /// ends at once, and until ``otherDeviceReleases()`` a release from
    /// this device with nothing of its own on is refused `otherDeviceHolds`
    /// in the Core's words, as RemoteKeying::stopFrom refuses it
    /// (src/core/session/RemoteKeying.cpp:549-556, link 18.6). The test
    /// delivers the matching `txState` itself.
    public func otherDeviceTakes(_ name: String) {
        lock.withLock {
            otherHolder = name
            liveKey = nil
        }
    }

    /// The other device lets transmit go: releases are accepted again.
    public func otherDeviceReleases() {
        lock.withLock { otherHolder = nil }
    }

    enum KeyingOutcome {
        case accepted(Int64?)
        case keyEnded
        /// A refusal a test queued, with its code where it gave one.
        case refused(reason: String, code: String?)
        /// A release from this device refused while the named device holds transmit.
        case otherDeviceHolds(String)
    }

    /// One keying command as the Core's RemoteKeying acts on it, for this
    /// fake's one device: one key, which PTT, TUNE and two-tone share; a key
    /// while it is on answers its epoch; an unkey older than the live key
    /// is ignored and one with nothing on changes nothing, or is refused
    /// while another device holds transmit; TUNE and two-tone off end only
    /// their own; a copy of a key answers as the
    /// first did while that key is on, and `keyEnded` once it has ended.
    /// Called with the lock held.
    private func keying(_ invoke: LinkMessage.CommandInvoke) -> [KeyingOutcome] {
        func flag(_ name: String) -> Bool {
            if case .bool(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return false
        }
        let starts = invoke.verb == "tx.key"
            || ((invoke.verb == "tx.tune" || invoke.verb == "tx.twoTone" || invoke.verb == "tx.tunerTune") && flag("on"))
        if let earlier = keyingAnswers[invoke.id] {
            if starts, case .accepted(let epoch?) = earlier, liveKey?.epoch != epoch {
                return [.keyEnded]
            }
            return [earlier]
        }
        let outcome: KeyingOutcome
        switch invoke.verb {
        case "tx.tunerTune" where starts && liveKey != nil && liveKey?.kind != invoke.verb:
            // RemoteKeying::tunerTune: never started on the air, this
            // device's own key included (RemoteKeying.cpp:694-699).
            outcome = .refused(reason: Self.onAirReason, code: "holderOnAir")
        case _ where starts:
            if let live = liveKey {
                outcome = .accepted(live.epoch)
            } else {
                txEpoch += 1
                liveKey = (txEpoch, invoke.verb)
                outcome = .accepted(txEpoch)
            }
        case "tx.tune", "tx.twoTone", "tx.tunerTune":
            if liveKey?.kind == invoke.verb {
                liveKey = nil
                outcome = .accepted(nil)
            } else {
                outcome = nothingOn()
            }
        case "tx.unkey":
            var epoch: Int64 = 0
            if case .i64(let value)? = invoke.args.first(where: { $0.name == "epoch" })?.value {
                epoch = value
            }
            if let live = liveKey {
                if epoch >= live.epoch {
                    liveKey = nil
                }
                outcome = .accepted(nil)
            } else {
                outcome = nothingOn()
            }
        default:
            // tx.keepalive: accepted, whether or not it counted.
            outcome = .accepted(nil)
        }
        if invoke.verb != "tx.keepalive" {
            keyingAnswers[invoke.id] = outcome
        }
        return [outcome]
    }

    /// RemoteKeying::stopFrom with nothing of this device's on: refused
    /// while another device holds transmit, otherwise accepted and nothing
    /// changes. Called with the lock held.
    private func nothingOn() -> KeyingOutcome {
        if let otherHolder {
            return .otherDeviceHolds(otherHolder)
        }
        return .accepted(nil)
    }

    // MARK: The Core's name

    /// The Core's name now, while the fake keeps one: `""` for a Core with none.
    public var stationLabel: String? { lock.withLock { coreLabel } }

    /// The `devices` object's class.
    public static let devicesClass = "StationDevicesFacade"
    /// The verb that renames the Core.
    public static let renameVerb = "station.rename"
    /// The Core's refusals of a rename (the suite's `devices`).
    public static let renameNotUnderstoodReason = "The request to rename the Core was not understood."
    public static let renameRuleReason =
        "Name the Core with a callsign of letters, digits and /, then if you like a / and up to 32 letters, digits, dashes or underscores, for example KG4VCF/shack."

    /// `station.rename` as the Core answers it: the name stored and sent
    /// back as a setting, then the answer, then the `devices` object's delta.
    private func renameReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage] {
        func result(_ accepted: Bool, _ reason: String, _ affected: [String]) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: affected, values: nil))
        }
        guard invoke.args.count == 1, let argument = invoke.args.first, argument.name == "label",
              case .utf8(let text) = argument.value else {
            return [result(false, Self.renameNotUnderstoodReason, [])]
        }
        guard let name = Self.coreName(text) else {
            return [result(false, Self.renameRuleReason, [])]
        }
        let (sendsObject, revision) = lock.withLock { () -> (Bool, Int64) in
            let had = coreLabel != nil
            coreLabel = name
            devicesRevision += 1
            return (had, devicesRevision)
        }
        var replies: [LinkMessage] = [
            .settingsValue(LinkMessage.SettingsValue(key: "StationLabel", origin: "", properties: [
                .init(ordinal: 0, name: "StationLabel", value: .utf8(name)),
            ])),
            result(true, "", ["devices"]),
        ]
        if sendsObject {
            replies.append(.delta(LinkMessage.Delta(key: "devices", properties: [
                .init(ordinal: 1, name: "revision", value: .i64(revision)),
                .init(ordinal: 2, name: "stationLabel", value: .utf8(name)),
            ])))
        }
        return replies
    }

    /// A name in the Core's form, as the Core keeps it (trimmed, a callsign
    /// of letters, digits and `/`, then optionally `/` and up to 32 letters,
    /// digits, `-` or `_`), or nil for one the Core refuses.
    static func coreName(_ text: String) -> String? {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        let callsign: Substring
        let suffix: Substring
        if let slash = trimmed.lastIndex(of: "/") {
            callsign = trimmed[..<slash]
            suffix = trimmed[trimmed.index(after: slash)...]
        } else {
            callsign = Substring(trimmed)
            suffix = ""
        }
        func alnum(_ c: Character) -> Bool { c.isASCII && (c.isLetter || c.isNumber) }
        guard !callsign.isEmpty, callsign.count <= 32, callsign.first != "/", callsign.last != "/",
              callsign.allSatisfy({ alnum($0) || $0 == "/" }),
              suffix.count <= 32, suffix.allSatisfy({ alnum($0) || $0 == "_" || $0 == "-" }) else {
            return nil
        }
        return suffix.isEmpty ? String(callsign) : "\(callsign)/\(suffix)"
    }

    /// The `devices` object as the fake sends it now.
    private func devicesObject() -> String {
        let (name, revision, addresses) = lock.withLock { (coreLabel ?? "", devicesRevision, coreAddressesJson) }
        let sendsAddresses = additions.contains(.coreAddresses)
        return LinkCodec.encode(.objectCreate(LinkMessage.ObjectCreate(key: "devices", className: Self.devicesClass,
                                                                       properties: [
            .init(ordinal: 0, name: "listJson", value: .utf8("[]")),
            .init(ordinal: 1, name: "revision", value: .i64(revision)),
            .init(ordinal: 2, name: "stationLabel", value: .utf8(name)),
            .init(ordinal: 3, name: "claimed", value: .bool(true)),
            .init(ordinal: 4, name: "tokenActive", value: .bool(false)),
            .init(ordinal: 5, name: "keyBackupAcknowledged", value: .bool(false)),
            .init(ordinal: 6, name: "keyPath", value: .utf8("")),
            .init(ordinal: 7, name: "pairingWindowOpen", value: .bool(false)),
            .init(ordinal: 8, name: "pairingCode", value: .utf8("")),
        ] + (sendsAddresses ? [.init(ordinal: Self.coreAddressesOrdinal, name: CoreAddressList.propertyName,
                                     value: .utf8(addresses))] : []))))
    }

    private func mediaReplies(_ payload: [String: LinkJSON]) -> [LinkMessage] {
        if let replies = monitorAudioReplies(payload) {
            return replies
        }
        if let replies = audioQualityReplies(payload) {
            return replies
        }
        if payload["op"] == .string("start") {
            lock.withLock { txDisplayDeclared = payload["txDisplayVersion"] != nil }
        }
        if payload["op"] == .string("subscribe"), let endpoint = payload["endpointId"], let revision = payload["revision"] {
            lock.withLock { endpointRevisions[endpoint] = revision }
            if additions.contains(.wideband), let context = context(for: payload) {
                return [context]
            }
        }
        if payload["op"] == .string("keyframe"), additions.contains(.wideband) {
            sendDisplayRows(1)
        }
        let endpoint = payload["endpointId"] ?? .number(0)
        let revision = lock.withLock { endpointRevisions[endpoint] } ?? .number(0)
        guard payload["op"] == .string(Self.clarityRetuneOp), additions.contains(.clarityRetune),
              let reason = takeRefusal(Self.clarityRetuneOp) else {
            // An older Core, or a retune the fake takes: no answer.
            return []
        }
        // Refused in the shape of a refused display operation.
        return [.mediaControl(LinkMessage.MediaControl(payload: [
            "op": .string("rejected"),
            "connectionId": payload["connectionId"] ?? .string(""),
            "endpointId": endpoint,
            "revision": revision,
            "reason": .string(reason),
        ]))]
    }

    // MARK: Media with the extended view

    /// Makes the media peer a session's media client uses with this fake.
    public var mediaPeerFactory: MediaControlClient.PeerFactory {
        { [self] in
            let peer = FakeMediaPeer()
            peer.microphoneLosslessNegotiated = microphoneCarriesLossless
            lock.withLock { peers.append(peer) }
            return peer
        }
    }

    /// The media peers the session's media client made with this fake, oldest first.
    public var mediaPeers: [FakeMediaPeer] {
        lock.withLock { peers }
    }

    /// Whether the Core can give the extended view now (true unless set
    /// otherwise); when not, its `wideband` says unavailable.
    public var widebandAvailable: Bool {
        get { lock.withLock { extendedViewAvailable } }
        set { lock.withLock { extendedViewAvailable = newValue } }
    }

    /// The `context` answering a `subscribe`: the requested view, its span
    /// no wider than the DDC rate, or with the extended view available the
    /// ADC's half rate; `wideband` exactly when the subscription asked.
    private func context(for payload: [String: LinkJSON]) -> LinkMessage? {
        func number(_ key: String) -> Double? {
            if case .number(let value)? = payload[key] {
                return value
            }
            return nil
        }
        guard let connection = payload["connectionId"], let endpoint = number("endpointId"),
              let revision = number("revision"), let centre = number("centreHz"), let span = number("spanHz"),
              let pixels = number("pixels"), let minDbm = number("minDbm"), let maxDbm = number("maxDbm"),
              let fps = number("fps"), let lines = number("framesPerLine") else {
            return nil
        }
        var context: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": connection, "endpointId": .number(endpoint),
            "revision": .number(revision), "sourceStream": .number(0), "sourceCentreHz": .number(centre),
            "sampleRateHz": .number(Self.ddcRateHz), "centreHz": .number(centre), "wideCentreHz": .number(0),
            "wideSpanHz": .number(0), "traceSamples": .number(pixels), "waterfallSamples": .number(pixels),
            "wideSamples": .number(0), "minDbm": .number(minDbm), "maxDbm": .number(maxDbm), "fps": .number(fps),
            "framesPerLine": .number(lines),
        ]
        var ceiling = Self.ddcRateHz
        if case .bool(let asked)? = payload["extendedView"] {
            if asked && widebandAvailable {
                context["wideband"] = .object([
                    "version": .number(1), "available": .bool(true), "active": .bool(true),
                    "physicalAdcIndex": .number(0), "filterChainIndex": .number(0), "sourceGeneration": .number(1),
                    "adcRateHz": .number(Self.adcRateHz), "lowHz": .number(0), "highHz": .number(Self.adcRateHz / 2),
                    "geometryRateBasis": .string("thetisLocalReference"),
                    "levelReference": .string("localWingRelativeWithStationRxOffset"),
                ])
                ceiling = max(Self.ddcRateHz, Self.adcRateHz / 2)
            } else {
                context["wideband"] = .object(["version": .number(1), "available": .bool(false), "active": .bool(false)])
            }
        }
        let accepted = min(span, ceiling)
        context["spanHz"] = .number(accepted)
        if lock.withLock({ txDisplayDeclared }) {
            context["transmit"] = .bool(false)
        }
        let generation = lock.withLock { () -> UInt32 in
            let next = (display?.generation ?? 0) &+ 1
            display = AcceptedDisplay(endpointId: UInt32(endpoint), generation: next, centreHz: centre,
                                      spanHz: accepted, pixels: Int(pixels), minDbm: minDbm, maxDbm: maxDbm,
                                      sequence: 0)
            return next
        }
        context["contextGeneration"] = .number(Double(generation))
        return .mediaControl(LinkMessage.MediaControl(payload: context))
    }

    /// Sends `count` display rows (keyframes) of the accepted endpoint over
    /// the newest media peer: a noise floor, a little higher over the
    /// receiver's own band, with a carrier every 25 kHz nearby and every
    /// 2 MHz further out, across the whole accepted span.
    public func sendDisplayRows(_ count: Int) {
        for _ in 0..<count {
            guard let (peer, frame) = lock.withLock({ () -> (FakeMediaPeer, Data)? in
                guard let peer = peers.last, var accepted = display else {
                    return nil
                }
                accepted.sequence &+= 1
                display = accepted
                return (peer, Self.keyframe(accepted))
            }) else {
                return
            }
            peer.send(frame)
        }
    }

    /// One NSDC v1 keyframe of `display`'s trace and waterfall, from the
    /// display codec document's byte layout.
    private static func keyframe(_ display: AcceptedDisplay) -> Data {
        var generator = SystemRandomNumberGenerator()
        let low = display.centreHz - display.spanHz / 2
        let binHz = display.spanHz / Double(display.pixels)
        let row: [UInt8] = (0..<display.pixels).map { index in
            let hz = low + (Double(index) + 0.5) * binHz
            let offset = hz - display.centreHz
            var dbm = -121.0 + Double.random(in: 0...12, using: &generator)
            if abs(offset) <= ddcRateHz / 2 {
                dbm += 6
            }
            let spacing = abs(offset) <= ddcRateHz / 2 ? 25_000.0 : 2_000_000.0
            let nearest = (offset / spacing).rounded() * spacing
            if nearest != 0, abs(offset - nearest) <= max(binHz, 1_500) {
                dbm = -80 - Double.random(in: 0...8, using: &generator)
            }
            let q = (dbm - display.minDbm) / (display.maxDbm - display.minDbm) * 255
            return UInt8(min(max(q.rounded(), 0), 255))
        }
        var out = Data("NSDC".utf8)
        out.append(1)
        out.append(0x01 | 0x02)
        put(42, 2, into: &out)
        put(UInt64(display.endpointId), 4, into: &out)
        put(UInt64(display.generation), 4, into: &out)
        put(UInt64(display.sequence), 4, into: &out)
        put(UInt64(display.sequence) * 33_000_000, 8, into: &out)
        put(UInt64(Float(display.minDbm).bitPattern), 4, into: &out)
        put(UInt64(Float(display.maxDbm).bitPattern), 4, into: &out)
        put(UInt64(row.count), 2, into: &out)
        put(UInt64(row.count), 2, into: &out)
        put(0, 2, into: &out)
        for _ in 0..<2 {
            let size = 128
            let blocks = (row.count + size - 1) / size
            out.append(3)
            put(UInt64(blocks), 2, into: &out)
            for block in 0..<blocks {
                let slice = row[(block * size)..<min(row.count, (block + 1) * size)]
                out.append(contentsOf: [1, 8, UInt8(slice.count)])
                put(UInt64(slice.count), 2, into: &out)
                out.append(contentsOf: slice)
            }
        }
        return out
    }

    private static func put(_ value: UInt64, _ bytes: Int, into out: inout Data) {
        for index in (0..<bytes).reversed() {
            out.append(UInt8((value >> (8 * UInt64(index))) & 0xFF))
        }
    }

    // MARK: What the app sent

    /// Every message the app sent, over every connection, in order.
    public var messages: [LinkMessage] { lock.withLock { received } }

    /// Frames the app sent that the fake could not read.
    public var unreadableFrames: [String] { lock.withLock { unreadable } }

    /// How many connections sessions have dialled.
    public var connectionCount: Int { lock.withLock { connections.count } }

    /// True once the newest connection has played its whole fixture.
    public var isLive: Bool { lock.withLock { connections.last?.isLive ?? false } }

    /// Waits until the app has sent a message `matching`; returns it, or nil
    /// if none came by `timeout`.
    public func waitForMessage(within timeout: Duration = .seconds(30),
                               matching: @escaping @Sendable (LinkMessage) -> Bool) async -> LinkMessage? {
        let found = await waiters.wait(within: timeout) { [self] in
            messages.contains(where: matching)
        }
        return found ? messages.first(where: matching) : nil
    }

    /// Waits until `condition` holds, checked again at each message the
    /// fake receives (never by polling); false if `timeout` passes first.
    public func waitUntil(within timeout: Duration = .seconds(30),
                          _ condition: @escaping @Sendable () -> Bool) async -> Bool {
        await waiters.wait(within: timeout, until: condition)
    }

    /// Waits until the newest connection has played its whole fixture.
    @discardableResult
    public func waitUntilLive(within timeout: Duration = .seconds(30)) async -> Bool {
        await waiters.wait(within: timeout) { [self] in isLive }
    }

    // MARK: Acting as the Core

    /// Sends one message to the app on the newest connection; returns once
    /// the session has handled it.
    public func deliver(_ message: LinkMessage) async {
        noteCatalogue(message)
        await lock.withLock { connections.last }?.deliverNow(LinkCodec.encode(message))
    }

    /// The link to the newest connection drops.
    public func dropLink() async {
        // A holder whose link drops is unkeyed at once (link 18.2).
        lock.withLock { liveKey = nil }
        await lock.withLock { connections.last }?.dropNow()
    }

    // MARK: Driving a connection

    var stepCount: Int { steps.count }

    /// The station messages from `position` up to the next client step, and
    /// the position of that step (or the end).
    func stationRun(from position: Int) -> (texts: [String], next: Int) {
        var texts: [String] = []
        var index = position
        while index < steps.count {
            switch steps[index] {
            case .station(let text):
                texts.append(text)
            case .devices:
                texts.append(devicesObject())
            case .client:
                return (texts, index)
            }
            index += 1
        }
        return (texts, index)
    }

    func expectedKind(at position: Int) -> LinkMessage.Kind? {
        guard position < steps.count, case .client(let kind) = steps[position] else {
            return nil
        }
        return kind
    }

    func record(_ text: String) -> LinkMessage? {
        let message: LinkMessage?
        do {
            message = try LinkCodec.decode(text)
        } catch {
            message = nil
        }
        lock.withLock {
            if let message {
                received.append(message)
            } else {
                unreadable.append(text)
            }
        }
        waiters.release()
        return message
    }

    func connectionChanged() {
        waiters.release()
    }
}
