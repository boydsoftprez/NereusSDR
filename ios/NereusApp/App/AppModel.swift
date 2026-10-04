// NereusSDR for iOS: the app's model of its Core: the session and everything fed from it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
import NereusModels

/// Everything the screens read about the connected Core, and the way to
/// connect to one. It owns the session and feeds each of its events, in
/// order, to the mirror, the settings proxy, the command client and media
/// control, then to ``connection``. The clients live as long as the app:
/// they send through whichever session is current, so a screen holds them
/// across reconnects and a change of Core.
///
/// Finding Cores on the network, pairing and remote access arrive in their
/// own tasks; each adds its own member here and a way into ``connect(to:trust:authenticator:transportFactory:clock:)``.
@MainActor
final class AppModel: ObservableObject {
    /// The session to the current Core, or nil when none is chosen.
    @Published private(set) var session: StationSession?
    /// Where the connection has got to.
    @Published private(set) var connection: ConnectionState = .notConnected
    /// The link's latest round-trip time in milliseconds, once a ping has
    /// been answered in this session.
    @Published private(set) var roundTripMs: Int?
    /// The address of the Core this session reaches.
    @Published private(set) var coreHost: String?
    /// The band's sound is muted on this phone: the Core is asked for none.
    @Published private(set) var audioMuted = false
    /// The Core's `hello` in the current session, once it has sent one:
    /// the link versions it speaks.
    @Published private(set) var stationHello: LinkMessage.Hello?
    /// The session is up through the remote access service's relay: the
    /// link chip says Relay, and back on the air says so.
    @Published private(set) var linkRelayed = false

    /// The Core's objects, capabilities and telemetry.
    /// Setup, Audio, On this phone, Audio quality (R-IOS-09).
    let audioQuality: AudioQualityModel
    let mirror: MirrorStore
    /// The settings the Core keeps.
    let settings: SettingsProxyClient
    /// Commands to the Core.
    let commands: CommandClient
    /// The media connection: audio and the band's display.
    let media: MediaControlClient
    /// The settings this phone keeps for itself.
    let phoneSettings: PhoneSettings
    /// The band's sound on this phone: playing while connected, and where it
    /// plays. Nil where there is no sound (the tests).
    let audio: AudioSessionController?
    /// The same feed object MediaControlClient plays from, observed asynchronously.
    private let diagnosticsPlayback: AudioPlaybackCore?
    lazy var connectionPerformance = ConnectionPerformanceModel(app: self, playback: diagnosticsPlayback)
    /// The main screen: the band, its display subscription and the RX panel.
    let main: MainScreenModel
    /// The Core's questions and notices about the other devices on it.
    let devices: SeveralDevicesClient
    /// The long session: what the band asks for, the data counters, the
    /// sleep timer and the screen.
    let longSession: SessionController
    /// The Core's record streams: its spots and its spot sources' consoles.
    let records: RecordStreamClient
    /// The Core's spots on the band and Spot Hub.
    let spots: SpotsModel
    /// FreeDV Reporter as the Core runs it; kept here, since the Tools tab is rebuilt on every tab switch.
    let freedv: FreeDVReporterModel
    /// The support bundle this phone collects; kept here, since the Tools tab is rebuilt on every tab switch
    /// and the Core takes a moment to make its bundle.
    lazy var supportBundle = SupportBundleModel(mirror: mirror, commands: commands,
                                                measurements: longSession.measurements)
    /// The Core's log and its logging categories: one for the Support Bundle
    /// page and Setup's Logs page, so the log is asked for while either shows
    /// and one page leaving never stops the other's (D95).
    /// One authoritative Diversity context shared by Tools and the band’s DIV route.
    lazy var diversity = DiversityModel(mirror: mirror, phone: phoneSettings, commands: commands,
        slices: main.slices, captureSender: { [route] in route.captureCommandSender() })
    lazy var coreLog = CoreLogModel(mirror: mirror, commands: commands, records: records)
    /// The Setup pages the Core describes (R-IOS-18), for as long as it
    /// sends them in the current session.
    let setupFeed: SetupDescriptionFeed
    /// Those pages as the Setup tree shows them, kept while the Core is away.
    lazy var setupPages = SetupDescribedPages(feed: setupFeed, store: mirror,
                                              hello: $stationHello.eraseToAnyPublisher())
    /// Each described control's owner: the settings proxy, the mirror, the
    /// command client or this phone's own stores.
    /// Setup > Hardware > Calibration's Level Cal run and the band's line
    /// while one goes (Level Cal 2).
    lazy var levelCal = LevelCalModel(store: mirror, commands: commands, slices: main.slices)
    lazy var setupControls: SetupControlDispatcher = {
        let dispatcher = SetupControlDispatcher(
            store: mirror, feed: setupFeed, settings: settings, commands: commands,
            captureSender: { [route] in route.captureCommandSender() },
            selectedSlice: { [weak slices = main.slices] in slices?.activeSliceId },
            selectionChanges: main.slices.$activeSliceId.eraseToAnyPublisher(), phone: phoneSetupKeys,
            signedInWithDeviceKey: { [weak self] in self?.session?.signsWithDeviceKey ?? false })
        // Enable VOX waits for this phone's microphone line (V15's `gate.micLine`).
        dispatcher.microphoneLineOpen = { [weak transmit = main.transmit] in transmit?.microphoneLine ?? false }
        // Limits a row takes from the radio's transmit ranges (V15's `rangeFrom`).
        dispatcher.catalogueTransmitRange = { [weak catalogFeed = main.catalogFeed] name in
            guard let transmit = catalogFeed?.catalog?.board.transmit else { return nil }
            let range: StationCatalog.Range?
            switch name {
            case "power": range = transmit.power?.range
            case "tunePowerForTxBand": range = transmit.tunePowerForTxBand?.range
            case "tunePower": range = transmit.tunePower?.range
            case "micGainDb": range = transmit.micGainDb
            default: range = nil
            }
            return range.map { SetupDescription.Range(minimum: $0.min, maximum: $0.max, step: $0.step) }
        }
        return dispatcher
    }()
    /// The clock the mirror stamps the Core's telemetry with, for its age.
    let mirrorClock: any LinkClock
    /// This phone's settings behind the described controls' phone keys.
    private lazy var phoneSetupKeys = PhoneSetupKeys(main: main)
    /// Where Setup's buttons that open another page lead; set by the root
    /// view, which owns the tabs.
    var phoneNavigation: PhoneNavigation? {
        get { phoneSetupKeys.navigation }
        set { phoneSetupKeys.navigation = newValue }
    }

    private let route: SessionRoute
    private var pump: Task<Void, Never>?
    private var roundTripPump: Task<Void, Never>?
    private var mediaPump: Task<Void, Never>?
    private var watches: Set<AnyCancellable> = []
    /// The last request for the band's sound, so each goes in order.
    private var audioRequest: Task<Void, Never>?
    /// The last refusal in the current session, shown with the state that follows it.
    private var refusal: Refusal?
    /// The media peers of a session reached directly: host candidates only.
    private let directPeerFactory: MediaControlClient.PeerFactory
    private let liveDirectPeers: Bool
    /// The media peers of a session that came through the remote access
    /// service, from that connection's ICE settings.
    private let remotePeerFactory: RemotePeerFactory
    private let liveRemotePeers: Bool
    /// The current session's way through the remote access service, when
    /// it came that way.
    private var serviceRoute: (any CoreServiceRoute)?
    var diagnosticsRendezvousRole: String {
        serviceRoute == nil ? "Dialled by address"
            : "Found through the remote access service"
    }
    private var serviceContext: ServiceRouteContext?
    /// The selected route belongs to this logical session, including after
    /// its transport is replaced by an authenticated control move.
    private var selectedRank: PathRacer.Rank?
    private var lastServiceIce: IceSettings?
    /// The Core's own STUN servers (`mediaStunUrls`) for this session, the
    /// first choice for a session dialled by address. Held for the session
    /// only, never written to settings.
    private(set) var coreStunUrls: [String]?
    /// The current session's media route, so a later STUN list reaches it.
    private var currentMediaRoute: MediaRoute?
    private var mediaTunnel: MediaTunnelContext?
    private var nextMediaOwner: UInt64 = 0
    private var mediaOwner: UInt64?
    private var connectIntent: UInt64 = 0
    private var listeningIntent: UInt64?
    var snapshotCompleted: ((StationSession) -> Void)?
    var heldQuestionReceived: ((StationSession, LinkMessage.SessionHeld) -> Void)?
    var heldQuestionEnded: ((StationSession) -> Void)?
    var sessionEnded: ((StationSession, LinkMessage.SessionEnd) -> Void)?
    var safetyMirrorChanged: ((StationSession) -> Void)?

    #if DEBUG
    /// Holds teardown after stopping its session to test overlapping owners.
    var disconnectAfterSessionStopForTesting: (@MainActor () async -> Void)?
    /// Runs as teardown starts, before the session it ends is stopped.
    var disconnectBeforeSessionStopForTesting: (@MainActor () -> Void)?
    var ownsPublishedSessionForTesting: Bool {
        guard let session else { return false }
        return pump != nil && route.session === session
    }
    var retryAvailabilityCheckedForTesting: (@MainActor () async -> Void)?
    var pathRefreshAfterHeartbeatForTesting: (@MainActor () async -> Void)?
    var startBeforeMediaActivationForTesting: (@MainActor () async -> Void)?
    var startAfterMediaActivationForTesting: (@MainActor () async -> Void)?
    var mediaOwnerForTesting: UInt64? { mediaOwner }
    var commandRouteForTesting: SessionRoute { route }
    var winnerAvailabilityCheckedForTesting: (@MainActor () async -> Void)?
    var beforeMediaEventForTesting: (@MainActor (MediaControlEvent) async -> Void)?
    var afterMediaEventForTesting: (@MainActor (MediaControlEvent) async -> Void)?
    #endif

    /// Makes a media peer for a session through the remote access service,
    /// from its control connection's ICE settings.
    typealias RemotePeerFactory = @Sendable (IceSettings?) -> any MediaPeerConnection

    /// `platform` is what transmit reaches in iOS itself (the lock, background
    /// time, the idle timer); the app uses iOS's own, and each test its own.
    #if DEBUG
    /// Only the command-line band fixture supplies this stand-in; a real
    /// session and all command/ordered keying senders keep their own route.
    func useBandFixturePropertySender(_ sender: @escaping @Sendable (LinkMessage.PropertyWrite) async throws -> Void) {
        route.useBandFixturePropertySender(sender)
    }
    #endif

    init(phoneSettings: PhoneSettings = PhoneSettings(),
         mediaPeerFactory: MediaControlClient.PeerFactory? = nil,
         remoteMediaPeerFactory: RemotePeerFactory? = nil,
         playback: AudioPlaybackCore? = nil,
         audio: AudioSessionController? = nil,
         microphone: (@Sendable (MediaUplink) -> any MicrophoneSource)? = nil,
         displaySettings: BandDisplaySettingsStore = BandDisplaySettingsStore(),
         meterSettings: SMeterSettingsStore = SMeterSettingsStore(),
         platform: TransmitModel.Platform = TransmitModel.Platform(),
         sessionSources: SessionController.Sources = .steady(),
         mirrorClock: any LinkClock = SystemLinkClock()) {
        let route = SessionRoute()
        self.route = route
        self.phoneSettings = phoneSettings
        self.audio = audio
        diagnosticsPlayback = playback
        let direct = mediaPeerFactory ?? { MediaPeer() }
        directPeerFactory = direct
        liveDirectPeers = mediaPeerFactory == nil
        remotePeerFactory = remoteMediaPeerFactory ?? AppModel.remoteMediaPeer
        liveRemotePeers = remoteMediaPeerFactory == nil
        self.mirrorClock = mirrorClock
        mirror = MirrorStore(send: { message in try await route.send(message) }, clock: mirrorClock)
        settings = SettingsProxyClient(send: { message in try await route.send(message) },
                                       captureSender: { route.captureCommandSender() }, clock: mirrorClock)
        commands = CommandClient(send: { message in try await route.send(message) },
                                 captureSender: { route.captureCommandSender() },
                                 captureGuardedSender: { route.captureHeartbeatSender() })
        media = MediaControlClient(send: { message in try await route.send(message) },
                                   peerFactory: direct, playback: playback,
                                   onMediaConnected: { await route.mediaConnectionUp() })
        let media = media
        devices = SeveralDevicesClient(store: mirror, settings: settings, commands: commands)
        main = MainScreenModel(mirror: mirror, settings: settings, commands: commands,
                               operations: BandSubscriber.Operations(
                                   subscribe: { subscription in try await media.subscribe(subscription) },
                                   unsubscribe: { endpointId in try await media.unsubscribe(endpointId: endpointId) },
                                   retuneClarity: { endpointId in try await media.retuneClarity(endpointId: endpointId) }),
                               displaySettings: displaySettings, meterSettings: meterSettings, devices: devices,
                               microphone: microphone?(media.uplink), uplink: media.uplink,
                               captureVoxSender: { route.captureVoxSender() },
                               captureTakeSender: { route.captureCommandSender() }, platform: platform)
        // MON's route on the media connection (D80, `monitor-audio`).
        main.transmit.monitorRouteSender = { route in await media.setMonitorRoute(route) }
        longSession = SessionController(settings: phoneSettings, subscriber: main.subscriber, band: main.band,
                                        transmit: main.transmit, sources: sessionSources)
        records = RecordStreamClient(mirror: mirror, commands: commands)
        // The TX panel's AM Mod Monitor (D102), on the Core's record streams.
        main.modMonitor = ModMonitorModel(mirror: mirror, commands: commands, records: records, settings: settings,
                                          slices: main.slices, phone: phoneSettings)
        spots = SpotsModel(records: records, mirror: mirror, commands: commands, settings: settings,
                           slices: main.slices, catalogFeed: main.catalogFeed, phone: phoneSettings)
        freedv = FreeDVReporterModel(records: records, mirror: mirror, commands: commands, settings: settings,
                                     spots: spots, slices: main.slices, catalogFeed: main.catalogFeed,
                                     phone: phoneSettings)
        setupFeed = SetupDescriptionFeed(store: mirror)
        // The audio quality this phone asks for, and the microphone's (R-IOS-09).
        audioQuality = AudioQualityModel(settings: phoneSettings, mirror: mirror, catalogFeed: main.catalogFeed,
                                         media: media, cellular: longSession.$cellular.eraseToAnyPublisher())
        // The band's display events, in order, for as long as the client lives.
        let events = media.events
        mediaPump = Task { [weak self] in
            for await event in events {
                guard let self else { return }
                let microphone: MediaControlEvent.MicrophoneLifecycle?
                if case .microphoneLifecycle(let lifecycle) = event {
                    microphone = lifecycle
                } else {
                    microphone = nil
                }
                let delivered = microphone?.event ?? event
                #if DEBUG
                await self.beforeMediaEventForTesting?(delivered)
                #endif
                if let microphone {
                    if self.mediaOwner == microphone.owner, !microphone.authority.isRevoked {
                        switch microphone.change {
                        case .line:
                            self.main.receive(delivered)
                        case .trackClosed:
                            self.main.transmit.microphoneTrackClosed(owner: microphone.owner,
                                                                     sourceAuthority: microphone.authority)
                        }
                    }
                } else {
                    self.main.receive(delivered)
                }
                self.audioQuality.receive(delivered)
                #if DEBUG
                await self.afterMediaEventForTesting?(delivered)
                #endif
                if case .mediaState = delivered, let session = self.session {
                    await self.refreshCurrentPath(on: session)
                }
            }
        }
        // The band plays unless muted; the wish carries across connections.
        requestAudio(true)
        $connection
            .sink { [weak self] state in
                self?.longSession.connectionChanged(state)
            }
            .store(in: &watches)
        audio?.$state
            .sink { [weak self] newState in
                self?.connectionPerformance.playbackStateWillChange(newState)
            }
            .store(in: &watches)
        wireTransmitAudio()
        wireDialHaptics()
    }

    /// The dial's haptics follow Setup, Navigation's two switches, now and
    /// whenever this phone's settings change.
    private func wireDialHaptics() {
        applyDialHaptics()
        phoneSettings.objectWillChange
            .sink { [weak self] _ in
                // Sent before the change: apply once it has landed.
                Task { @MainActor [weak self] in
                    self?.applyDialHaptics()
                }
            }
            .store(in: &watches)
    }

    private func applyDialHaptics() {
        main.tuning.setDialHaptics(ticksOnDetents: phoneSettings.dialDetentTicks,
                                   bumpsOnKilohertz: phoneSettings.dialKilohertzBumps)
    }

    /// Transmit and the phone's sound (Task 55): a call or Siri unkeys and
    /// stops the microphone; while this phone transmits the band follows
    /// the While you transmit settings, which, with the microphone choice,
    /// come from this phone's settings.
    private func wireTransmitAudio() {
        guard let audio else {
            return
        }
        let transmit = main.transmit
        let micLevel = main.micLevel
        audio.onInterruptionBegan = { [weak transmit, weak micLevel] in
            transmit?.interrupted()
            micLevel?.interrupted()
        }
        audio.onMediaServicesReset = { [weak transmit, weak micLevel] in
            transmit?.mediaServicesReset()
            micLevel?.interrupted()
        }
        audio.setMicrophone(phoneSettings.microphone)
        // MON plays in headphones only: the rule, not a choice (D80).
        audio.setWhileTransmitting(muteBand: phoneSettings.muteBandWhileTalking, monInHeadphonesOnly: true)
        transmit.$transmittingHere.combineLatest(transmit.$mon)
            .sink { [weak audio] transmitting, mon in
                audio?.setTransmitting(transmitting, monitorOn: mon)
            }
            .store(in: &watches)
        // MON needs headphones (D80): the route follows the phone's output.
        audio.$onHeadphones
            .sink { [weak transmit] headphones in
                transmit?.headphonesChanged(headphones)
            }
            .store(in: &watches)
    }

    // MARK: Sound

    /// Mutes or unmutes the band on this phone. Muted, the Core sends no
    /// sound at all; the band keeps showing.
    func setAudioMuted(_ muted: Bool) {
        guard muted != audioMuted else {
            return
        }
        audioMuted = muted
        requestAudio(!muted)
    }

    private func requestAudio(_ enabled: Bool) {
        let previous = audioRequest
        let media = media
        audioRequest = Task {
            await previous?.value
            await media.setAudioEnabled(enabled)
        }
    }

    /// The app's model: the Core's audio plays through the phone's audio
    /// session. Without memory for the playback path there is no sound, and
    /// everything else still works.
    static func live() -> AppModel {
        guard let playback = try? AudioPlaybackCore() else {
            return AppModel(sessionSources: .system())
        }
        let audio = AudioSessionController(session: SystemAudioSession(),
                                           output: EnginePlaybackOutput(core: playback))
        return AppModel(playback: playback, audio: audio, microphone: { uplink in MicCapture(uplink: uplink) },
                        sessionSources: .system())
    }

    // MARK: Connecting

    /// Connects to a Core, ending any session to another first. Returns once
    /// the connection has opened or failed to; ``connection`` follows it
    /// from there.
    func connect(to endpoint: StationEndpoint, trust: StationTrust, authenticator: any StationAuthenticator,
                 transportFactory: @escaping LinkTransportFactory = WebSocketLinkTransport.factory,
                 clock: any LinkClock = SystemLinkClock()) async {
        longSession.cancelSleepExpiry()
        connectIntent &+= 1
        let intent = connectIntent
        await disconnectCurrent(ifIntent: intent)
        guard connectIntent == intent else { return }
        let next = StationSession(endpoint: endpoint, trust: trust, authenticator: authenticator, clock: clock,
                                  transportFactory: transportFactory,
                                  diagnosticEndpointRank: PathRacer.Rank.otherWebSocket.rawValue)
        serviceRoute = nil
        serviceContext = nil
        selectedRank = .otherWebSocket
        await start(next, host: endpoint.host, mediaRoute: .address, connectIntent: intent)
    }

    /// Connects to a paired Core through the remote access service
    /// (R-IOS-16, link document section 20), ending any session to another
    /// first: each attempt dials through `service` and runs the session over
    /// the control connection it opens, and the media connection uses that
    /// connection's ICE settings, with the longer deadline its gathering
    /// needs. `name` stands in for the address the band shows until the
    /// Core names itself. Returns once the connection has opened or failed
    /// to.
    func connect(through service: any CoreServiceRoute, name: String?, trust: StationTrust,
                 authenticator: any StationAuthenticator, clock: any LinkClock = SystemLinkClock()) async {
        longSession.cancelSleepExpiry()
        connectIntent &+= 1
        let intent = connectIntent
        await disconnectCurrent(ifIntent: intent)
        guard connectIntent == intent else { return }
        let next = StationSession(trust: trust, authenticator: authenticator, clock: clock,
                                  transport: { service.makeTransport() })
        serviceRoute = service
        serviceContext = nil
        let rank = service.selectedPathRank.flatMap(PathRacer.Rank.init(rawValue:)) ?? .directIce
        selectedRank = rank
        await start(next, host: name, mediaRoute: .service(service, ice: nil, rank: rank),
                    connectIntent: intent)
    }

    /// Starts one session on the race's already open, identity-verified
    /// transport. The lease can be retired while any setup await is in
    /// progress; every suspension is followed by a liveness check.
    func connect(winner: PathRacer.Winner, name: String?, trust: StationTrust,
                 authenticator: any StationAuthenticator, clock: any LinkClock,
                 transportFactory: @escaping LinkTransportFactory,
                 serviceRetryRoute: (@Sendable () -> any CoreServiceRoute)? = nil,
                 raceAgain: (@Sendable () async throws -> PathRacer.Winner)? = nil) async -> Bool {
        longSession.cancelSleepExpiry()
        connectIntent &+= 1
        let intent = connectIntent
        await disconnectCurrent(ifIntent: intent)
        guard connectIntent == intent, await winner.transport.isAvailable(),
              connectIntent == intent else { return false }
        let context = ServiceRouteContext(ice: winner.mediaIce, path: winner.path)
        let fallback: @Sendable () -> any SessionTransport = {
            switch winner.route {
            case .address(let endpoint): return transportFactory(endpoint, trust)
            case .service:
                guard let route = serviceRetryRoute?() else { return UnavailableServiceRetryTransport() }
                return ContextualServiceTransport(inner: route.makeTransport(), route: route, context: context,
                                                  generation: context.beginRetry())
            }
        }
        let first = InitialSessionTransport(lease: winner.transport, fallback: fallback)
        let next: StationSession
        if let raceAgain {
            let reference = AutomaticSessionReference()
            // Selection runs in StationSession's owned task. The session's
            // normal retry timer and online hold still decide when it runs.
            next = StationSession(trust: trust, authenticator: authenticator, clock: clock,
                                  asyncTransport: { [weak self] in
                if let lease = first.takeFirst() { return lease }
                let chosen = try await raceAgain()
                guard let expected = reference.session, !Task.isCancelled,
                      await self?.adoptRetry(chosen, on: expected) == true else {
                    chosen.transport.close()
                    throw LinkTransportError.failed("retired automatic route")
                }
                return chosen.transport
            })
            reference.session = next
        } else {
            next = StationSession(trust: trust, authenticator: authenticator, clock: clock,
                                  transport: { first.make() })
        }
        selectedRank = winner.rank
        let mediaRoute: MediaRoute
        let host: String?
        switch winner.route {
        case .address(let endpoint):
            serviceRoute = nil
            serviceContext = nil
            mediaRoute = .address
            host = endpoint.host
        case .service(let service):
            serviceRoute = service
            serviceContext = context
            lastServiceIce = winner.mediaIce
            mediaRoute = .service(service, ice: context.ice, rank: winner.rank)
            host = name
        }
        #if DEBUG
        await winnerAvailabilityCheckedForTesting?()
        #endif
        let stillAvailable = await winner.transport.isAvailable()
        guard connectIntent == intent, stillAvailable else {
            if connectIntent == intent { await disconnectCurrent(ifIntent: intent) }
            return false
        }
        let adopted = await start(next, host: host, lease: winner.transport, mediaRoute: mediaRoute,
                                  connectIntent: intent)
        if !adopted, connectIntent == intent { await disconnectCurrent(ifIntent: intent) }
        return adopted
    }

    /// A retry's winner is its own inspected lease. Capture that exact
    /// service route before the session can release its buffered hello.
    private func adoptRetry(_ winner: PathRacer.Winner, on expected: StationSession) async -> Bool {
        guard session === expected, let owner = mediaOwner, let tunnel = mediaTunnel else { return false }
        guard await winner.transport.isAvailable() else { return false }
        #if DEBUG
        await retryAvailabilityCheckedForTesting?()
        #endif
        guard session === expected, mediaOwner == owner, !Task.isCancelled else { return false }
        selectedRank = winner.rank
        let mediaRoute: MediaRoute
        switch winner.route {
        case .address(let endpoint):
            serviceRoute = nil
            serviceContext = nil
            coreHost = endpoint.host
            mediaRoute = .address
        case .service(let route):
            serviceRoute = route
            serviceContext = ServiceRouteContext(ice: winner.mediaIce, path: winner.path)
            lastServiceIce = winner.mediaIce
            mediaRoute = .service(route, ice: winner.mediaIce, rank: winner.rank)
        }
        currentMediaRoute = mediaRoute
        let choice = mediaChoice(for: mediaRoute, tunnel: tunnel)
        guard await media.usePeers(choice.factory, connectDeadline: choice.deadline, owner: owner),
              session === expected, mediaOwner == owner, !Task.isCancelled else { return false }
        guard await media.useDirectLadder(directLadder(for: mediaRoute, tunnel: tunnel), owner: owner),
              session === expected, mediaOwner == owner, !Task.isCancelled else { return false }
        let available = await winner.transport.isAvailable()
        return available && session === expected && mediaOwner == owner && !Task.isCancelled
    }

    var currentPathRank: PathRacer.Rank? {
        if let live = serviceRoute?.selectedPathRank, let rank = PathRacer.Rank(rawValue: live) {
            return rank
        }
        return selectedRank
    }

    func refreshCurrentPath(on expected: StationSession) async {
        guard session === expected, let owner = mediaOwner else { return }
        selectedRank = currentPathRank
        linkRelayed = (selectedRank ?? .otherWebSocket) >= .turn
        let selectedTunnel = await media.selectedTunnel(owner: owner)
        guard session === expected, mediaOwner == owner else { return }
        await expected.setAcceleratedHeartbeat(linkRelayed || selectedTunnel)
        #if DEBUG
        await pathRefreshAfterHeartbeatForTesting?()
        #endif
    }

    func supportsControlMove(on expected: StationSession) -> Bool {
        session === expected && mirror.isSnapshotComplete &&
            (mirror.agreedMinor ?? 0) >= 11 &&
            mirror.capabilityVersion("controlSwitchVersion") >= 1
    }

    /// Read after every suspension in the upgrade path. Local state keeps
    /// the phone from asking for a move during TX; the Core has final say on
    /// MOX delay timers not represented in the mirror.
    func safeToMoveControl(_ expected: StationSession) -> Bool {
        guard supportsControlMove(on: expected), connection == .connected else { return false }
        return mediaLadderState().quiet
    }

    /// This phone's transmit, VOX and the Core's air as the direct media
    /// ladder reads them before each step, fallback and recovery; the same
    /// reading a control move makes. Nothing known reads as keyed.
    func mediaLadderState() -> MediaLadderState {
        guard session != nil, mirror.isSnapshotComplete else { return .unknown }
        // MirrorStore is updated synchronously by the event pump. The band's
        // TransmitModel refreshes on a later MainActor turn, so it cannot be
        // the only source for a move preflight immediately after a TX delta.
        guard !mirror.isStale else { return .unknown }
        var coreOnAir = false
        if mirror.capabilityVersion("txStateVersion") >= 1 {
            guard let state = mirror.object("txState") else { return .unknown }
            for name in ["keyed", "tuning", "twoTone", "txEnding", "holderTransferring"] {
                if state[name] == .bool(true) { coreOnAir = true }
            }
        }
        var voxArmed = mirror.object("transmit")?["voxEnabled"] == .bool(true)
        if mirror.object("transmit")?["tune"] == .bool(true) ||
            mirror.object("radio")?["transmitting"] == .bool(true) ||
            mirror.object("pureSignal")?["twoToneOn"] == .bool(true) { coreOnAir = true }
        let tx = main.transmit
        var keyed = tx.ptt.transmitting
        if tx.ptt.voxArmed || tx.vox { voxArmed = true }
        if tx.coreOnAir || tx.report.keyed || tx.report.txEnding || tx.report.holderTransferring {
            coreOnAir = true
        }
        switch tx.ptt.state {
        case .keying, .keyed, .ending, .unkeying, .waiting, .linkLost:
            keyed = true
        case .idle, .refused, .heldElsewhere:
            break
        }
        return MediaLadderState(keyed: keyed, voxArmed: voxArmed, coreOnAir: coreOnAir,
                                controlRelayed: linkRelayed)
    }

    enum RouteMoveError: Error { case ticketRefused, invalidTicket }
    enum RouteMoveOutcome { case moved, deferredSafety, deferredTicket, failed }

    private final class MoveDeferral: @unchecked Sendable {
        private let lock = NSLock()
        private var reason: RouteMoveOutcome?
        func mark(_ value: RouteMoveOutcome) { lock.withLock { reason = value } }
        var value: RouteMoveOutcome? { lock.withLock { reason } }
    }

    nonisolated static func pathTicket(from answer: CommandResult) throws -> PathTicket {
        guard answer.accepted else { throw RouteMoveError.ticketRefused }
        guard answer.valueEntries.count == 2,
              answer.valueEntries[0].ordinal == 0,
              answer.valueEntries[0].name == "ticket",
              case .utf8(let secret) = answer.valueEntries[0].value,
              answer.valueEntries[1].ordinal == 1,
              answer.valueEntries[1].name == "expiresInMs",
              case .i64(let expiry) = answer.valueEntries[1].value,
              let ticket = PathTicket(secret: secret, expiresInMs: expiry) else {
            throw RouteMoveError.invalidTicket
        }
        return ticket
    }

    /// Called from a separate owned task, never from the serial session
    /// event pump that feeds CommandClient its ticket answer.
    func moveControl(to winner: PathRacer.Winner, on expected: StationSession) async -> RouteMoveOutcome {
        guard safeToMoveControl(expected) else {
            winner.transport.close()
            return .deferredSafety
        }
        guard let current = currentPathRank, winner.rank < current else {
            winner.transport.close()
            return .failed
        }
        let commands = commands
        let deferral = MoveDeferral()
        let moved = await expected.moveControl(to: winner.transport,
            safetyCheck: { [weak self, weak expected] in
                guard let self, let expected else { return false }
                let safe = await self.safeToMoveControl(expected)
                if !safe { deferral.mark(.deferredSafety) }
                return safe
            },
            requestTicket: {
                do {
                    let answer = try await commands.invokeBound("session.pathTicket", arguments: [],
                        timeout: .seconds(10),
                        sender: { [weak expected] message, permit in
                            guard let expected else { throw LinkSendError.notConnected }
                            try await expected.send(message, permit: permit)
                        },
                        stillAllowed: { [weak self, weak expected] in
                            guard let self, let expected else { return false }
                            let safe = await self.safeToMoveControl(expected)
                            if !safe { deferral.mark(.deferredSafety) }
                            return safe
                        })
                    return try Self.pathTicket(from: answer)
                } catch RouteMoveError.ticketRefused {
                    deferral.mark(.deferredTicket)
                    throw RouteMoveError.ticketRefused
                }
            },
            onRouteCommit: { [weak self, weak expected] in
                guard let self, let expected else { return }
                await self.routeDidMove(to: winner, on: expected)
            })
        return moved ? .moved : (deferral.value ?? .failed)
    }

    private func routeDidMove(to winner: PathRacer.Winner, on expected: StationSession) async {
        guard session === expected, let owner = mediaOwner, let tunnel = mediaTunnel else { return }
        selectedRank = winner.rank
        linkRelayed = winner.rank >= .turn
        let mediaRoute: MediaRoute
        switch winner.route {
        case .address(let endpoint):
            serviceRoute = nil
            serviceContext = nil
            coreHost = endpoint.host
            mediaRoute = .address
        case .service(let service):
            serviceRoute = service
            serviceContext = ServiceRouteContext(ice: winner.mediaIce, path: winner.path)
            lastServiceIce = winner.mediaIce
            mediaRoute = .service(service, ice: winner.mediaIce, rank: winner.rank)
        }
        currentMediaRoute = mediaRoute
        let choice = mediaChoice(for: mediaRoute, tunnel: tunnel)
        await expected.setAcceleratedHeartbeat(linkRelayed)
        guard session === expected, mediaOwner == owner else { return }
        _ = await media.useDirectLadder(directLadder(for: mediaRoute, tunnel: tunnel), owner: owner)
        guard session === expected, mediaOwner == owner else { return }
        _ = await media.controlRouteDidMove(peerFactory: choice.factory, connectDeadline: choice.deadline,
            safeToReplace: { [weak self, weak expected] in
                guard let self, let expected else { return false }
                return await self.safeToMoveControl(expected)
            }, owner: owner)
        guard session === expected, mediaOwner == owner else { return }
        await refreshCurrentPath(on: expected)
    }

    private enum MediaRoute {
        case address
        case service(any CoreServiceRoute, ice: IceSettings?, rank: PathRacer.Rank)
    }

    struct MediaChoice {
        let factory: MediaControlClient.PeerFactory
        let deadline: Duration
    }

    /// Makes a direct-address media peer from its configuration, carried
    /// by the Core's tunnel as the ladder's last rung.
    typealias DirectAddressPeerMaker = @Sendable (MediaPeer.Configuration, MediaTunnelContext)
        -> any MediaPeerConnection

    /// The media peer for a session to a Core dialled by address, the
    /// media ladder (R-IOS-16): IPv6 direct, then an IPv4 hole punch
    /// through STUN, then the Core's tunnel, which every peer carries.
    /// The Core's own STUN server (`stunServers`, from `mediaStunUrls`) when
    /// it sent a usable one (``MediaPeer/Configuration/directAddress(stunServers:)``);
    /// else, after a service session in this connection (`serviceIce`), its
    /// STUN server and no relay of this end's own (``IceSettings/forMedia(controlPathRelayed:)``);
    /// and with neither, host candidates and the tunnel. Either way every
    /// candidate type the Core sends but relay is kept, and no relay server
    /// is passed.
    nonisolated static func directAddressMedia(
        serviceIce: IceSettings?, stunServers: [String], tunnel: MediaTunnelContext,
        makePeer: @escaping DirectAddressPeerMaker = { MediaPeer(configuration: $0, route: .tunnel($1)) }
    ) -> MediaChoice {
        let coresOwn = MediaPeer.Configuration.directAddress(stunServers: stunServers)
        let throughService = coresOwn.iceServers.isEmpty ? serviceIce.flatMap {
            try? MediaPeer.Configuration.throughRendezvous($0.forMedia(controlPathRelayed: false))
        } : nil
        let configuration = throughService ?? coresOwn
        // ICE's own deadline whenever a STUN server may be gathered from.
        let deadline = throughService != nil || !configuration.iceServers.isEmpty
            ? IceSettings.connectDeadline : MediaControlClient.connectDeadline
        return MediaChoice(factory: { makePeer(configuration, tunnel) }, deadline: deadline)
    }

    private func mediaChoice(for route: MediaRoute, tunnel: MediaTunnelContext) -> MediaChoice {
        switch route {
        case .address:
            guard liveDirectPeers else {
                return MediaChoice(factory: directPeerFactory, deadline: MediaControlClient.connectDeadline)
            }
            return Self.directAddressMedia(serviceIce: lastServiceIce, stunServers: coreStunUrls ?? [],
                                           tunnel: tunnel)
        case .service(let service, let ice, let rank):
            guard liveRemotePeers else {
                let remote = remotePeerFactory
                return MediaChoice(factory: { remote(ice ?? service.mediaIceSettings()) },
                                   deadline: IceSettings.connectDeadline)
            }
            return MediaChoice(factory: {
                let selectedRank = service.selectedPathRank.flatMap(PathRacer.Rank.init(rawValue:)) ?? rank
                let relay = selectedRank == .webRelay ? service.mediaRelayContext() : nil
                let mediaIce = (ice ?? service.mediaIceSettings())?.forMedia(
                    controlPathRelayed: selectedRank >= .turn)
                let configuration = mediaIce.flatMap { try? MediaPeer.Configuration.throughRendezvous($0) }
                    ?? MediaPeer.Configuration(hostCandidatesOnly: false, refusesRelayCandidates: false)
                return MediaPeer(configuration: configuration,
                                 route: relay.map(MediaPeer.Route.relay) ?? .direct)
            }, deadline: IceSettings.connectDeadline)
        }
    }

    /// The direct media ladder's peers (``MediaDirectLadder``) for a session
    /// dialled by address, whose media can ride the Core's tunnel: a
    /// direct-only peer with the Core's STUN server, else the last service
    /// session's, and the fallback's peer on the tunnel alone. None through
    /// the remote access service, where media has no tunnel.
    private func directLadder(for route: MediaRoute, tunnel: MediaTunnelContext) -> DirectLadderPeers? {
        guard case .address = route else { return nil }
        let state: @Sendable () async -> MediaLadderState = { [weak self] in
            await self?.mediaLadderState() ?? .unknown
        }
        guard liveDirectPeers else {
            return DirectLadderPeers(direct: directPeerFactory, tunnelAlone: nil, state: state)
        }
        let stun = Self.directStunServers(core: coreStunUrls, serviceIce: lastServiceIce)
        return DirectLadderPeers(
            direct: { MediaPeer(configuration: .directAddress(stunServers: stun), route: .direct) },
            tunnelAlone: { MediaPeer(configuration: .tunnelAlone, route: .tunnel(tunnel)) },
            state: state)
    }

    /// The STUN servers a direct-only peer gathers with: the Core's own list
    /// when it holds a usable `stun:` server, else the last service
    /// session's STUN server, else none (host candidates alone).
    nonisolated static func directStunServers(core: [String]?, serviceIce: IceSettings?) -> [String] {
        if let core, !MediaPeer.Configuration.directAddress(stunServers: core).iceServers.isEmpty {
            return core
        }
        guard let server = serviceIce?.stun else { return [] }
        let host = server.host.contains(":") ? "[\(server.host)]" : server.host
        return ["stun:\(host):\(server.port)"]
    }

    /// The Core's `mediaStunUrls` from its capabilities: nil when absent or
    /// not a JSON array of strings.
    nonisolated static func coreStunUrls(in capabilities: [String: MirrorValue]) -> [String]? {
        guard case .text(let text)? = capabilities[MediaDirectLadder.stunUrlsCapability] else { return nil }
        return MediaDirectLadder.stunUrls(fromCapability: text)
    }

    /// A new capabilities message may carry a new STUN list: the next media
    /// peers use it.
    private func coreCapabilitiesChanged(on expected: StationSession, owner: UInt64) async {
        let list = Self.coreStunUrls(in: mirror.capabilities)
        guard list != coreStunUrls else { return }
        coreStunUrls = list
        guard let route = currentMediaRoute, let tunnel = mediaTunnel else { return }
        let choice = mediaChoice(for: route, tunnel: tunnel)
        guard await media.usePeers(choice.factory, connectDeadline: choice.deadline, owner: owner),
              session === expected, mediaOwner == owner else { return }
        _ = await media.useDirectLadder(directLadder(for: route, tunnel: tunnel), owner: owner)
    }

    /// The media peer for a session through the remote access service:
    /// its control connection's ICE settings (``MediaPeer/Configuration/throughRendezvous(_:)``).
    /// Settings that are not complete, which a connection that opened
    /// always has, leave a peer of every candidate type with no servers.
    nonisolated static func remoteMediaPeer(_ ice: IceSettings?) -> any MediaPeerConnection {
        if let ice, let configuration = try? MediaPeer.Configuration.throughRendezvous(ice) {
            return MediaPeer(configuration: configuration)
        }
        return MediaPeer(configuration: MediaPeer.Configuration(hostCandidatesOnly: false,
                                                                refusesRelayCandidates: false))
    }

    /// Starts `next`, the session to the chosen Core, and feeds its events.
    @discardableResult
    private func start(_ next: StationSession, host: String?, lease: PreauthenticatedTransport? = nil,
                       mediaRoute: MediaRoute, connectIntent intent: UInt64) async -> Bool {
        guard connectIntent == intent else { return false }
        nextMediaOwner &+= 1
        let owner = nextMediaOwner
        mediaOwner = owner
        // A session that carries media counts as up only once media is up.
        await next.setWaitsForMedia(true)
        #if DEBUG
        await startBeforeMediaActivationForTesting?()
        #endif
        guard mediaOwner == owner, connectIntent == intent else { return false }
        if let lease, !(await lease.isAvailable()) { return false }
        guard mediaOwner == owner, connectIntent == intent else { return false }
        let tunnel = MediaTunnelContext(session: next)
        let activated = await media.activateLogicalSession(next, owner: owner)
        #if DEBUG
        await startAfterMediaActivationForTesting?()
        #endif
        guard activated, mediaOwner == owner, connectIntent == intent else {
            _ = await media.retireLogicalSession(owner: owner)
            await tunnel.close()
            return false
        }
        // A new session's STUN list comes with its own capabilities.
        coreStunUrls = nil
        let choice = mediaChoice(for: mediaRoute, tunnel: tunnel)
        guard await media.usePeers(choice.factory, connectDeadline: choice.deadline, owner: owner),
              await media.useDirectLadder(directLadder(for: mediaRoute, tunnel: tunnel), owner: owner),
              mediaOwner == owner, connectIntent == intent else {
            _ = await media.retireLogicalSession(owner: owner)
            await tunnel.close()
            return false
        }
        currentMediaRoute = mediaRoute
        mediaTunnel = tunnel
        route.use(next)
        session = next
        listeningIntent = intent
        connectionPerformance.attach(session: next, owner: intent, mediaOwner: owner)
        longSession.sessionStarted(owner: intent)
        refusal = nil
        roundTripMs = nil
        stationHello = nil
        linkRelayed = false
        coreHost = host
        let times = next.roundTrips
        roundTripPump = Task { [weak self] in
            for await time in times {
                guard let self, self.session === next, self.mediaOwner == owner,
                      self.listeningIntent == intent else { return }
                self.roundTripMs = Self.shownMilliseconds(time)
            }
        }
        let events = next.events
        pump = Task { [weak self] in
            for await event in events {
                guard let self else {
                    return
                }
                await self.handle(event, from: next, owner: owner)
                if case .stateChanged(.stopped) = event {
                    return
                }
            }
        }
        await next.connect()
        return session === next && mediaOwner == owner && connectIntent == intent
    }

    /// The phone's network came back or changed. Waiting sessions redial
    /// immediately; a ready session checks its current control path. A
    /// route commit or a media failure decides whether media must move.
    func retryNow() async {
        guard let session else {
            return
        }
        await session.redialNow()
    }

    /// The phone has no network: the session spends no tries until
    /// ``retryNow()``, since none could succeed.
    func holdRetries() async {
        await session?.holdRetries()
    }

    /// A measured round trip in whole milliseconds, rounded up, and never
    /// less than 1: the link chip shows a number only once one is measured,
    /// so a round trip under a millisecond reads "1 ms", never "0 ms".
    nonisolated static func shownMilliseconds(_ time: Duration) -> Int {
        let parts = time.components
        let ms = Double(parts.seconds) * 1000 + Double(parts.attoseconds) / 1e15
        return max(1, Int(ms.rounded(.up)))
    }

    /// Ends the session to the current Core, if any. The mirror keeps the
    /// Core's last values, marked stale.
    func disconnect() async {
        longSession.cancelSleepExpiry()
        connectIntent &+= 1
        await disconnectCurrent(ifIntent: connectIntent)
    }

    /// Captured by an expiring timer before any transmit or leave await.
    var sleepSessionOwner: (listening: UInt64, media: UInt64, session: StationSession)? {
        guard let listeningIntent, let mediaOwner, let session else { return nil }
        return (listeningIntent, mediaOwner, session)
    }

    /// Captures the old control route before sleep crosses command admission.
    /// The route and the caller's permit both remain checked at handoff.
    func captureSleepLeaveSender(ifCurrent expected: StationSession) -> CommandClient.CommandSender? {
        guard session === expected else { return nil }
        return route.captureCommandSender()
    }

    private func disconnectCurrent(ifIntent intent: UInt64) async {
        guard connectIntent == intent else { return }
        // A take belongs to the session ending: nothing it asked or sent
        // carries to the next Core.
        main.take.sessionChanged(.stopped)
        guard let current = session else {
            let pendingOwner = mediaOwner
            let pendingTunnel = mediaTunnel
            nextMediaOwner &+= 1
            mediaOwner = nil
            serviceRoute = nil
            serviceContext = nil
            selectedRank = nil
            lastServiceIce = nil
            coreStunUrls = nil
            currentMediaRoute = nil
            mediaTunnel = nil
            coreHost = nil
            linkRelayed = false
            if let pendingOwner { _ = await media.retireLogicalSession(owner: pendingOwner) }
            await pendingTunnel?.close()
            return
        }
        let endingPump = pump
        let endingTunnel = mediaTunnel
        let endingOwner = mediaOwner
        let endingListeningIntent = listeningIntent
        connectionPerformance.retire()
        #if DEBUG
        disconnectBeforeSessionStopForTesting?()
        #endif
        await current.disconnect()
        #if DEBUG
        await disconnectAfterSessionStopForTesting?()
        #endif
        // The pump ends after handling the session's stop.
        await endingPump?.value
        if let endingOwner { _ = await media.retireLogicalSession(owner: endingOwner) }
        await endingTunnel?.close()
        // A newer teardown may have finished and published another session
        // while this one was suspended. Only its owner clears model state.
        guard session === current else { return }
        if let endingListeningIntent { longSession.sessionEnded(owner: endingListeningIntent) }
        listeningIntent = nil
        pump = nil
        roundTripPump?.cancel()
        roundTripPump = nil
        roundTripMs = nil
        coreHost = nil
        route.use(nil)
        session = nil
        serviceRoute = nil
        serviceContext = nil
        selectedRank = nil
        lastServiceIce = nil
        coreStunUrls = nil
        currentMediaRoute = nil
        mediaTunnel = nil
        mediaOwner = nil
        linkRelayed = false
        refusal = nil
        connection = .notConnected
    }

    // MARK: Events

    private func handle(_ event: StationSession.Event, from expected: StationSession,
                        owner: UInt64) async {
        guard session === expected, mediaOwner == owner else { return }
        if case .stateChanged(let state) = event {
            // Before the command client fails what was waiting: a take
            // from the session that was answers nothing on this one.
            main.take.sessionChanged(state)
        }
        mirror.handle(event)
        if case .message(let message) = event, Self.changesMoveSafety(message) {
            safetyMirrorChanged?(expected)
        }
        settings.handle(event)
        await commands.handle(event)
        guard session === expected, mediaOwner == owner else { return }
        // After the mirror, which holds the capabilities the streams need.
        records.handle(event)
        devices.handle(event)
        _ = await media.setExpectedAudioSilenceDuringTransmit(expectedTransmitSilence(), owner: owner)
        if case .message(.capabilities) = event {
            await coreCapabilitiesChanged(on: expected, owner: owner)
            guard session === expected, mediaOwner == owner else { return }
        }
        await media.handle(event, owner: owner)
        guard session === expected, mediaOwner == owner else { return }
        switch event {
        case .refused(let reason):
            refusal = reason
        case .stateChanged(let state):
            if state == .ready {
                // How this connection went, read before the screens hear it is up.
                await refreshCurrentPath(on: expected)
                guard session === expected, mediaOwner == owner else { return }
            }
            connection = connectionState(for: state)
            if state == .stopped, let listeningIntent {
                longSession.sessionEnded(owner: listeningIntent)
                self.listeningIntent = nil
                connectionPerformance.retire()
            }
            await main.transmit.sessionChanged(state, owner: owner)
            if state != .ready {
                // A new connection times its own round trip.
                roundTripMs = nil
            }
            // The band plays from the moment the Core is up (media included)
            // until the session ends; through a wait to retry it keeps
            // playing silence, so the app stays alive in the background.
            switch state {
            case .ready:
                audio?.start()
            case .stopped:
                audio?.stop()
            case .idle, .connecting, .authenticating, .receivingSnapshot, .waitingToRetry:
                break
            }
        case .message(.hello(let hello)):
            stationHello = hello
        case .message(.sessionHeld(let held)):
            heldQuestionReceived?(expected, held)
        case .message(.capabilities):
            heldQuestionEnded?(expected)
        case .message(.sessionEnd(let end)):
            sessionEnded?(expected, end)
        case .message(.snapshotComplete):
            snapshotCompleted?(expected)
        case .message:
            break
        }
    }

    func expectedTransmitSilence() -> Bool {
        guard mirror.isSnapshotComplete, !mirror.isStale,
              mirror.capabilityVersion("txStateVersion") >= 1,
              let state = mirror.object("txState") else { return false }
        return ["keyed", "tuning", "txEnding"].contains { state[$0] == .bool(true) }
    }

    nonisolated private static func changesMoveSafety(_ message: LinkMessage) -> Bool {
        switch message {
        case .capabilities, .snapshotComplete:
            return true
        case .delta(let delta):
            return ["txState", "transmit", "radio", "pureSignal"].contains(delta.key)
        case .objectCreate(let created):
            return ["txState", "transmit", "radio", "pureSignal"].contains(created.key)
        case .objectDestroy(let destroyed):
            return ["txState", "transmit", "radio", "pureSignal"].contains(destroyed.key)
        default:
            return false
        }
    }

    private func connectionState(for state: StationSession.State) -> ConnectionState {
        switch state {
        case .idle:
            return .notConnected
        case .connecting:
            return .connecting
        case .authenticating:
            return .signingIn
        case .receivingSnapshot:
            return .loading
        case .ready:
            refusal = nil
            return .connected
        case .waitingToRetry(let seconds):
            return .waitingToRetry(seconds: seconds, reason: refusal)
        case .stopped:
            return refusal.map(ConnectionState.refused) ?? .notConnected
        }
    }
}

/// Consumes the verified lease once. A later session retry makes a fresh
/// transport for the same route and cannot replay the original hello.
private final class InitialSessionTransport: @unchecked Sendable {
    private let lock = NSLock()
    private var lease: PreauthenticatedTransport?
    private let fallback: @Sendable () -> any SessionTransport

    init(lease: PreauthenticatedTransport, fallback: @escaping @Sendable () -> any SessionTransport) {
        self.lease = lease
        self.fallback = fallback
    }

    func make() -> any SessionTransport {
        if let first = takeFirst() { return first }
        return fallback()
    }

    func takeFirst() -> PreauthenticatedTransport? {
        lock.withLock {
            let first = lease
            lease = nil
            return first
        }
    }
}

private final class AutomaticSessionReference: @unchecked Sendable {
    private let lock = NSLock()
    private weak var saved: StationSession?
    var session: StationSession? {
        get { lock.withLock { saved } }
        set { lock.withLock { saved = newValue } }
    }
}

/// One selected service dial's media route. The initial winner supplies the
/// first value; each later same-service retry replaces it before that retry
/// releases its hello to StationSession.
final class ServiceRouteContext: @unchecked Sendable {
    private let lock = NSLock()
    private var savedIce: IceSettings?
    private var savedPath: ConnectionAttempt.Path
    private var generation = 0
    private var retired = false

    init(ice: IceSettings?, path: ConnectionAttempt.Path) {
        savedIce = ice
        savedPath = path
    }

    var ice: IceSettings? { lock.withLock { savedIce } }
    var path: ConnectionAttempt.Path { lock.withLock { savedPath } }

    func beginRetry() -> Int {
        lock.withLock {
            generation += 1
            retired = false
            return generation
        }
    }

    func replace(ice: IceSettings?, path: ConnectionAttempt.Path, generation expected: Int) {
        lock.withLock {
            guard generation == expected, !retired else { return }
            savedIce = ice
            savedPath = path
        }
    }

    func retire(generation expected: Int) {
        lock.withLock {
            if generation == expected { retired = true }
        }
    }
}

/// Captures a service retry's selected pair before the session can process
/// its buffered hello and eventually start media on that connection.
struct ContextualServiceTransport: LinkTransport {
    let inner: any SessionTransport
    let route: any CoreServiceRoute
    let context: ServiceRouteContext
    let generation: Int

    var boundsItsOwnOpening: Bool { inner.boundsItsOwnOpening }
    var selectedRouteObservation: SelectedRouteObservation { inner.selectedRouteObservation }
    var trafficObservation: LinkTrafficObservation? { inner.trafficObservation }
    func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) { inner.setBinaryReceiver(receiver) }
    @discardableResult func sendBinary(_ frame: Data) -> Bool { inner.sendBinary(frame) }
    @discardableResult func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool {
        inner.sendBinary(frame, ownership: ownership)
    }
    func discardBinary(ownership: BinaryMediaOwnership) { inner.discardBinary(ownership: ownership) }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let digest = try await inner.open(onEvent: onEvent)
        context.replace(ice: route.mediaIceSettings(), path: route.lastTry?.path ?? .direct,
                        generation: generation)
        return digest
    }

    @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
    func ping() { inner.ping() }
    func close() {
        context.retire(generation: generation)
        inner.close()
    }
}

/// A service winner without a factory for later attempts is an integration
/// error. Fail that retry locally instead of reusing a prior dialer's mutable
/// metadata or authenticating on an unowned route.
private struct UnavailableServiceRetryTransport: LinkTransport {
    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        throw LinkTransportError.failed("no service route for retry")
    }
    @discardableResult func send(_ text: String) -> Bool { false }
    func ping() {}
    func close() {}
}
