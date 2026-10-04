// NereusSDR for iOS: getting connected: welcome, an address, a code, your Cores, the band, and what goes wrong
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMedia
import NereusMirror
import os

/// The state machine behind the connecting screens (R-IOS-16, R-IOS-08;
/// spec section 5.3 items 1, 4, 6 to 9 and 14 for a direct connection;
/// D19, D23, D65, D69, D70, D72). It shows the welcome, Your Cores (the
/// paired ones), an address typed with its port apart, the code screen,
/// the microphone question after the first pairing, the connection and
/// the band, the link lost while listening and back on the air, and the
/// five trouble screens.
///
/// It drives one ``AppModel`` session. A pairing connection closes before
/// sign-in; initial sign-in may inspect two direct sockets and one service
/// introduction concurrently, without authenticating any until one verified
/// transport is selected. A later session retry retires its prior transport
/// first. Every reason the Core gives is shown as sent; the app's own words
/// are the board's.
///
/// It finds Cores on this Wi-Fi over Bonjour (D36, D71, spec section 5.3
/// items 2, 3 and 6): starting the browser asks iOS's Local Network
/// question once, Find my Core shows what is found (Found it), and Your
/// Cores lists, after the paired Cores and under On this network, each
/// found Core that isn't paired with this phone and takes a new device. An
/// unclaimed Core that allows it is claimed with one tap; the rest pair
/// with the code, the found address filled in. When the operator declines,
/// a typed address and a code still work.
///
/// A Core is renamed from Your Cores (D76): press and hold its row, Rename,
/// and the name goes to the Core (`station.rename`), so every device shows
/// it. The connected Core is renamed over its session; any other through a
/// sign-in made only for the rename (``RenameSignIn``), which closes after
/// it, so the phone is left as it was.
@MainActor
final class ConnectionFlow: ObservableObject {
    // MARK: What the screens show

    /// The screen the operator is on.
    enum Screen: Equatable {
        /// Nothing paired yet: one picture and two ways on.
        case welcome
        /// Find my Core: looking on this Wi-Fi, then Found it.
        case findCore
        /// How to run a Core, on a computer or a small box.
        case setUpCore
        /// Your Cores: the paired ones, and the ways to add one.
        case cores
        /// Enter an address: the address, and its port below it (D69).
        case typeAddress
        /// A paired Core's addresses, from its row's "⋯" (``addressesFor``).
        case addresses
        /// Pair with a code (picture 24).
        case pairByCode
        /// Paired: the microphone question, asked once (item 4).
        case microphone
        /// Connected: the band and the tabs.
        case band
    }

    /// One Core in Your Cores.
    struct CoreRow: Identifiable, Equatable {
        let station: PairedStation
        /// This phone must pair with it again before it can sign in (D70,
        /// D72), or the Core, found on this network, says it is unclaimed
        /// and so has forgotten this phone.
        let needsPairing: Bool
        /// The Core as found on this network, when its record names it.
        var found: FoundStation?

        var id: Data { station.identityKey }
        var label: String { station.label.isEmpty ? address : station.label }
        /// Where it is reached: the host, and the port when it isn't the
        /// standard one; for a Core this phone keeps no address for (paired
        /// through the remote access service), from anywhere.
        var address: String {
            guard let endpoint = station.dialOrder.first else {
                return ConnectionFlow.fromAnywhereText
            }
            return ConnectionFlow.addressText(endpoint)
        }
    }

    /// A Core found on this network that takes a new device (D71), listed
    /// under On this network.
    struct NearbyRow: Identifiable, Equatable {
        /// What its button does.
        enum Offer: Equatable {
            /// Unclaimed and one tap allowed: Pair claims it in one tap.
            case oneTap
            /// Use code: the code screen, the found address filled in.
            case code
            /// Unclaimed with pairing closed: listed greyed, saying so.
            case closed
        }

        let station: FoundStation
        let endpoint: StationEndpoint
        let offer: Offer

        var id: String { station.identityPrefix }
        var label: String { station.displayName }
        var address: String { ConnectionFlow.addressText(endpoint) }
    }

    /// A one-tap pairing the Core refused, or that failed, shown by its row
    /// with the Core's words as sent.
    struct NearbyProblem: Equatable {
        /// The row's identity prefix.
        let id: String
        let words: String
        /// The code is offered in its place.
        let offersCode: Bool
    }

    /// The notice over Your Cores.
    enum CoresNotice: Equatable {
        case placeTaken(byName: String, byId: String, happened: Date, reason: String)
        /// A Core removed or forgot this phone (D70).
        case removed(core: String)
        /// The Core's own words, or the app's, for an end it won't retry.
        case words(String)
        /// This phone can't read its key any more (D72), with Make a new key.
        case keyUnreadable
        /// The key couldn't be reached for now (the phone not yet unlocked,
        /// or the Keychain refused), with Try again.
        case keyUnavailable
    }

    /// One way the phone tried, as the Core-not-answering sheet lists it:
    /// this Wi-Fi, direct over the internet, or by relay, and how it went.
    struct TriedRow: Equatable {
        let place: String
        let result: String
    }

    /// A trouble sheet over Your Cores (item 14).
    enum Trouble: Equatable {
        /// The Core isn't answering: what was tried (this Wi-Fi, direct,
        /// relay), and what to check, with a `note` of its own when the
        /// remote access service could not be reached or the Core is too old
        /// to be reached through it. When iOS refused the local network, it
        /// says so and where to allow it.
        case notAnswering(core: String, tried: [TriedRow], localNetworkDenied: Bool, note: String?)
        /// The two ends share no link version: which one needs updating,
        /// both versions, and the Core's reason as it sent it.
        case needsUpdating(core: String, coreSpeaks: String, appSpeaks: String, reason: String?,
                           coreIsOlder: Bool, gap: Int?)
    }

    /// Which Core the code screen pairs with.
    struct PairTarget: Equatable {
        /// Where the pairing goes straight to; nil pairs through the remote
        /// access service's mailbox on the code's number, from anywhere.
        let endpoint: StationEndpoint?
        /// The Core's label, when it is one already listed.
        let label: String?
        /// The listed Core this pairing replaces (D70): what the phone kept
        /// for it is replaced by what this pairing gives.
        let replacing: Data?
        /// Where Back goes.
        let back: Screen
    }

    /// What the code screen says went wrong.
    enum PairingProblem: Equatable {
        /// Not a code; `word` is a word not in the Core's list, with the
        /// list's words that start like it.
        case notACode(word: String?, suggestions: [String])
        /// The address or port can't be read; nothing was sent.
        case address(String)
        /// The name can't be sent; nothing was sent.
        case name(String)
        /// The Core's wrong-code words, and its wait before the new code.
        case wrongCode(reason: String, waitSeconds: Int)
        /// The fifth wrong code in a row closed pairing at the Core.
        case pairingClosed
        /// The Core refused, with its words and its wait.
        case refused(reason: String, waitSeconds: Int)
        /// Anything else, in words.
        case failed(String)
    }

    /// The link lost while listening, on the band.
    struct LinkLost: Equatable {
        /// Which try this is since the link went.
        let attempt: Int
        /// When the next try goes; nil while one is under way.
        let retryAt: Date?
        /// Cancel stopped the retries.
        let stopped: Bool
        /// The Core's own words when it ended the session with a code and
        /// asked the phone to come back (its radio changing, link 12.4),
        /// kept until the next session is up; nil for a link that dropped.
        var words: String? = nil
        /// This phone was transmitting when the link went: the Core stops
        /// transmitting on its own (spec section 5.3 item 8).
        var keyed = false
    }

    /// The Core answers but can't hear the radio.
    struct RadioOff: Equatable {
        /// The radio, as the Core names it.
        let radio: String?
        /// Its address at the Core.
        let address: String?
        /// When this phone saw the radio go, if it saw it; nil when it was
        /// already gone at sign-in.
        let since: Date?
        /// When the screen started counting the Core's five-second retry.
        let shownAt: Date
        /// Why the Core has no radio, in its own words (`stationRadioWaiting`),
        /// when it says; nil otherwise.
        var waiting: String? = nil
    }

    @Published private(set) var screen: Screen {
        didSet {
            updateBrowsing()
        }
    }
    @Published private(set) var cores: [CoreRow] = []
    /// On this network: the found Cores that take a new device (D71).
    @Published private(set) var nearby: [NearbyRow] = []
    /// iOS doesn't allow this app to look on the local network.
    @Published private(set) var lookingDenied = false
    /// The found Core a one tap is claiming now, by its identity prefix.
    @Published private(set) var claiming: String?
    @Published private(set) var nearbyProblem: NearbyProblem?
    @Published private(set) var notice: CoresNotice?
    @Published private(set) var heldQuestion: LinkMessage.SessionHeld?
    @Published private(set) var heldChoiceID: String?
    @Published private(set) var heldBusy = false
    @Published private(set) var heldProblem: String?
    var heldCoreName: String { current.flatMap { $0.label.isEmpty ? nil : $0.label } ?? "this Core" }
    private var heldSession: StationSession?
    private var preferredTakeoverID: String?
    private var lastSessionEnd: LinkMessage.SessionEnd?
    /// Identity of a Core-removed notice, so forgetting another Core cannot
    /// erase that notice just because both rows have the same label.
    private var removedNoticeCoreID: Data?
    /// This phone can't read its key: the rows can't be used until a new one is made.
    @Published private(set) var keyUnreadable = false
    /// This phone made a new key: the Cores still list its old one (D72).
    @Published private(set) var madeNewKey = false
    @Published var trouble: Trouble?

    /// The Core being connected to, and what it said while it waits.
    @Published private(set) var connectingTo: Data?
    @Published private(set) var connectingNote: String?

    /// The paired Core the Addresses page is for, by its identity key.
    @Published private(set) var addressesFor: Data?
    /// Connect on Enter an address is reading which Core answers there.
    @Published private(set) var identifying = false
    /// The last connection attempt, address by address, in the order tried.
    @Published private(set) var attempt = ConnectionAttempt()

    // The address screen and the code screen.
    @Published var addressText = ""
    @Published var portText = ConnectionFlow.standardPortText
    @Published private(set) var addressProblem: String?
    @Published private(set) var pairTarget: PairTarget?
    @Published var codeText = ""
    @Published var nameText: String
    @Published private(set) var pairingProblem: PairingProblem?
    @Published private(set) var pairing = false
    /// Pair waits until the Core's new code appears.
    @Published private(set) var pairHeld = false
    /// The Core this phone just paired with, for the microphone question.
    @Published private(set) var pairedWith: PairedStation?

    // The band.
    @Published private(set) var linkLost: LinkLost?
    @Published private(set) var backOnAir = false
    @Published private(set) var radioOff: RadioOff?
    @Published private(set) var offline = false
    /// One of the band's covers is up (NO NETWORK, LINK LOST or
    /// DISCONNECTED): the band fades almost away under it.
    var coversBand: Bool {
        offline || linkLost != nil || radioOff != nil
    }
    /// The connected Core's label when it is one major behind this app.
    @Published private(set) var olderCore: String?

    // Renaming a Core (D76).
    /// The Core Rename's sheet is up for, by its identity key.
    @Published private(set) var renaming: Data?
    /// The name in the sheet's field.
    @Published var renameText = ""
    /// The Core's words (or the app's) under the field, when the rename didn't go.
    @Published private(set) var renameProblem: String?
    /// Save is signing in or waiting for the Core's answer.
    @Published private(set) var renameBusy = false
    /// The Cores this phone last found too old to rename (D23).
    @Published private(set) var coresWithoutRename: Set<Data> = []

    /// The Core named in the local-removal confirmation, and a failed save
    /// that can be retried from Your Cores.
    @Published private(set) var removeCandidate: CoreRow?
    struct RemoveProblem: Equatable {
        let id: Data
        let core: String
    }
    @Published private(set) var removeProblem: RemoveProblem?
    @Published private(set) var removingCore: Data?

    /// Whether a row's Rename can be used now, and if not, why.
    enum RenameAvailability: Equatable {
        case available
        /// The Core has no `station.rename` (D23).
        case needsNewerCore
        /// This phone must pair with the Core again before it can sign in.
        case needsPairing
        /// A connection or a pairing is under way.
        case busy
    }

    // MARK: Words

    /// A debug build's launch argument that opens on the band with no Core,
    /// for the UI tests of the band and its tabs.
    static let showBandArgument = "-NereusShowBand"
    /// A debug build's launch argument that opens on Your Cores with three
    /// made-up Cores kept only in memory, for the UI test of a row's
    /// press-and-hold menu.
    static let shotCoresArgument = "-NereusShotCores"
    /// A debug build's isolated empty station store for the Welcome UI test.
    /// It leaves the simulator's actual paired Cores and device key alone.
    static let uiTestWelcomeArgument = "-NereusUITestWelcome"

    /// The Core's standard port, as the port field starts.
    static let standardPortText = String(StationEndpoint.defaultPort)
    static let portProblemText = "The port is a number from 1 to 65535."
    static let addressProblemText =
        "That isn't an address this phone can read. Type the Core's name, or its IPv4 or IPv6 address."
    static let blankNameText = "Type the name the Core will list this phone under."
    static let unusableNameText =
        "The Core can't keep this name. Use a shorter name without line breaks or hidden characters."
    static let codeShapeText = "A code is a number and two words, like 7-anvil-harbor."
    static let endedText = "The connection to the Core ended before pairing finished. Try again."
    static let timedOutText = "The Core didn't answer in time, so pairing stopped. Try again."
    static let cannotPairText = "This Core can't pair devices with this app. Update NereusSDR on the Core."
    static let weakHashText =
        "The Core asked for a weaker pairing check than this app allows, so nothing was sent. Update NereusSDR on the Core."
    static let identityMismatchText =
        "The Core's answer didn't match the Core this phone reached, so pairing stopped. Try again."
    static let alreadyPairingText = "This phone is already pairing with that Core. Try again in a moment."
    // A Core's addresses.
    static let nothingAnsweredText =
        "Nothing answered at that address. Check the address and port, and that the Core is running."
    static let couldNotKeepAddressText = "This phone couldn't keep that address. Try again."
    static let keepOneAddressText = "A Core keeps at least one address."
    static let lastWorkedText = "Worked last"
    /// How long Enter an address, from Your Cores, waits to learn which Core
    /// answers before it goes on to the code: NereusSDR's own bound, a few
    /// seconds, well above a TLS opening on a working path.
    static let identifyDeadline: Duration = .seconds(5)
    static let fourAddressesText = "A Core keeps up to four addresses. Remove one to add another."
    static let typedAddressNote =
        "If this phone is paired with the Core at that address, it adds the address and connects. If not, the next step asks for the pairing code the Core shows."
    /// Enter an address, opened from a Core's Addresses page.
    static func addingNote(_ core: String) -> String {
        "This phone checks that \(core) answers at that address before it keeps it."
    }
    /// An address that reaches another Core than the one it was added for.
    static func otherCoreText(_ core: String) -> String {
        core.isEmpty ? "That address reaches another Core." : "That address reaches another Core, not \(core)."
    }
    static let couldNotKeepText = "This phone couldn't keep the pairing. Try again."
    static let offlineWaitText = "This phone is offline. It connects as soon as Wi-Fi or cellular is back."
    // Renaming a Core (D76).
    static let renameTitle = "Rename this Core"
    static let renameRule = "Your callsign, then / and a name, like KG4VCF/shack."
    nonisolated static let needsNewerCoreText = "Needs a newer Core"
    static let renameNoAnswerText = "The Core didn't answer. Try again."
    static let renameNeedsPairingText = "Pair with this Core again first."
    static let renameBusyText = "Wait until this connection is made."
    static let removeConfirmationText =
        "This removes the saved Core and its addresses from this phone. It does not remove this phone from the Core's Devices list."
    static let removeSaveFailedText = "This phone couldn't remove the saved Core. It is still listed. Try again."
    static let removeTransmitText = "Stop transmitting before removing this Core."
    static let removeBusyText = "Wait for the current Core action to finish."
    // Finding a Core (picture 06, Looking on this Wi-Fi and Found it; picture 24).
    static let lookingText = "Looking on this Wi-Fi\u{2026}"
    static let lookingDeniedText =
        "NereusSDR isn't allowed to look on this Wi-Fi. Turn on Local Network for NereusSDR in Settings, Privacy & Security."
    static let newCoreNote =
        "A new Core waits for its first device. Pairing claims it, and after that only a paired device can add another."
    /// "The station's Wi-Fi" is the ham station's network (D43).
    static let elsewhereNote =
        "Not on the station's Wi-Fi? The Core shows a code until it's claimed. Type it here from anywhere."
    static let closedNote =
        "Pairing closed on a Core that has no devices: it opens again only on that Core's own computer."

    // Reaching a Core from anywhere (R-IOS-16, pictures 07 and 10).
    /// A Core this phone keeps no address for, reached through the remote access service.
    nonisolated static let fromAnywhereText = "From anywhere"
    /// The Core-not-answering sheet's places, and how each went.
    nonisolated static let thisWiFiText = "This Wi-Fi"
    nonisolated static let directPlaceText = "Direct, over the internet"
    nonisolated static let relayPlaceText = "By relay"
    nonisolated static let notHereText = "Not here"
    nonisolated static let notAllowedText = "Not allowed to look"
    nonisolated static let notCheckedInText = "Core not checked in"
    nonisolated static let serviceNotReachedText = "Service not reached"
    /// The Addresses page of a Core this phone keeps no address for.
    nonisolated static let noAddressesText = "None kept. This phone reaches this Core from anywhere."
    /// Addresses, with no typed address but some learned from the Core.
    nonisolated static let noneAddedText = "None added on this phone."
    /// Addresses' second list: where the Core says it can be reached, and
    /// the addresses this phone proved on the Core's network. The phone
    /// keeps them up to date itself, so they have no Remove.
    nonisolated static let learnedHeadingText = "Learned from the Core"
    nonisolated static let learnedNoteText =
        "The Core tells this phone where it can be reached, and this phone keeps these up to date. It tries them too, so it can reach the Core straight when the internet service is down."

    /// The addresses learned from the Core (``PairedStation/directAddresses``)
    /// that the operator has not also typed, in the Core's order.
    nonisolated static func learnedAddresses(of station: PairedStation) -> [StationEndpoint] {
        let typed = Set(station.endpoints.map(\.canonical))
        return station.directAddresses.filter { !typed.contains($0.canonical) }
    }

    /// How long "Back on the air." stays on the band.
    static let backOnAirShown: Duration = .seconds(4)
    /// After the Core limits sign-ins from this phone (10 failures in 60 s
    /// lock the address out for 60 s), the flow waits this long and tries once more.
    static let signInLimitWait: Duration = .seconds(60)

    // MARK: What it works with

    /// What the flow needs from outside, injectable for tests.
    struct Dependencies {
        var keyStore: any DeviceKeyStore
        var stations: PairedStationStore
        var kind: DeviceKeyAuthenticator.Kind
        var transportFactory: LinkTransportFactory
        /// Resolves saved hostnames before the bounded direct dial scheduler.
        /// Tests inject their fake network's names; production resolves IPs.
        var directResolver: @Sendable ([StationEndpoint]) -> [StationEndpoint] = DirectRouteResolver.resolve
        /// This phone's networks now, which decide which of a Core's
        /// addresses a race may dial and how each ranks. Tests inject theirs.
        var networks: @Sendable () -> LocalNetworks = LocalNetworks.current
        var clock: any LinkClock
        var microphone: any MicrophoneAccess
        var network: (any NetworkWatch)?
        /// The link majors this app speaks.
        var appMajors: [UInt16]
        var now: () -> Date
        /// Finds Cores on this network; none in tests that don't look.
        var browser: (any StationBrowsing)?
        /// The remote access services, in the order to try them, for
        /// pairing by code from anywhere.
        var rendezvousServers: [RendezvousServer] = RendezvousServer.defaults
        /// The connections to a service that a pairing through its mailbox makes.
        var rendezvousTransportFactory: RendezvousTransportFactory = RendezvousWebSocket.factory
        /// Makes the way to a paired Core through the remote access service
        /// (``CoreServiceRoute``), tried after its addresses and this
        /// network; nil where there is none, so no connect goes that way.
        var serviceRoute: (@Sendable (PairedStation, DeviceIdentity) -> any CoreServiceRoute)?
        /// Tests can hold the post-identity sign-in boundary to exercise
        /// cancellation while the selected lease is being adopted.
        var sessionAuthenticator: (any StationAuthenticator)? = nil

        /// The phone's own: its key store and Keychain, the real connection,
        /// its microphone and network, and its kind from the idiom.
        static func live(kind: DeviceKeyAuthenticator.Kind) -> Dependencies {
            Dependencies(keyStore: DeviceIdentity.standardStore(), stations: PairedStationStore(), kind: kind,
                         transportFactory: WebSocketLinkTransport.factory, clock: SystemLinkClock(),
                         microphone: SystemMicrophoneAccess(), network: SystemNetworkWatch(),
                         appMajors: LinkVersionPolicy.supportedMajors, now: Date.init, browser: StationBrowser(),
                         serviceRoute: { station, device in
                             RendezvousServiceRoute(station: station, device: device,
                                                    servers: RendezvousServer.defaults)
                         })
        }
    }

    let app: AppModel
    private let dependencies: Dependencies
    private let phoneSettings: PhoneSettings
    private var identity: DeviceIdentity? {
        didSet {
            // The main screen tells this phone's hold of transmit, and which
            // slices it controls, by its id.
            app.main.transmit.thisDeviceId = identity?.id
            app.main.slices.thisDeviceId = identity?.id
        }
    }
    private static let logger = Logger(subsystem: "NereusSDR", category: "connect")

    // Where the session has got to, as the flow sees it.
    private enum Phase: Equatable {
        case idle
        case connecting
        case connected
        case lost
        case stopped
    }

    private var phase = Phase.idle
    /// The Core the session is for.
    private var current: PairedStation?
    /// The selected Core while a fresh dial is still retiring its old session.
    private var dialTarget: Data?
    /// One way a connect tries to reach a Core.
    enum Route: Equatable {
        /// One of its addresses, or where it was found on this network.
        case address(StationEndpoint)
        /// Through the remote access service (``CoreServiceRoute``).
        case throughService
    }

    /// The ways this connection tries, in order, and the one it is on.
    private var dialPlan: [Route] = []
    private var dialIndex = 0
    private var racer: PathRacer?
    private var lookRacer: PathRacer?
    private var lookTimer: (any LinkTimer)?
    private var lookTimerSerial: UInt64 = 0
    private var lookTask: Task<Void, Never>?
    private var lookReservation: UInt64?
    private var lookTaskSerial: UInt64?
    private var nextLookSerial: UInt64 = 0
    private var resumeHeldQueuedIntent: Int?
    private var lookQueued = false
    #if DEBUG
    private(set) var betterLookReservationsForTesting = 0
    func triggerBetterLookForTesting() {
        guard let session = app.session else { return }
        let intent = dialIntentGeneration
        Task { await betterLookDue(on: session, intent: intent) }
    }
    var queuedResumeIntentForTesting: Int? { resumeHeldQueuedIntent }
    func queueResumeForTesting() { queueResumeHeldLook() }
    func advanceResumeIntentForTesting() { advanceDialIntent() }
    #endif
    private var pendingStandby: PathRacer.Winner?
    private var heldLook = false
    private var lookStep = 0
    private enum BetterLookResult { case candidate(PathRacer.Winner), deferred, exhausted }
    private static let lookDelays: [Duration] = [.seconds(5), .seconds(30), .seconds(120), .seconds(300)]
    private var raceGeneration = 0
    private var dialIntentGeneration = 0
    private var winnerPath: ConnectionAttempt.Path?
    private var restartOnRetire: (station: PairedStation, intent: Int)?
    private var serviceDiscoveryUnknown: Set<Data> = []
    /// The way through the remote access service this connection made,
    /// when it got that far.
    private var service: (any CoreServiceRoute)?
    /// iOS refused the local network on one of this connection's tries.
    private var attemptDenied = false
    /// Enter an address's identity check under way, and its generation:
    /// Back, or leaving the screen, moves the generation, so a late answer
    /// is dropped.
    private var identifyTask: Task<CoreIdentityProbe.Outcome, Never>?
    /// The found addresses this connect has asked which Core answers at.
    private var proved: Set<StationEndpoint> = []
    /// Where this connect's Core was found on this network when it dialled:
    /// looking stops once the band shows, and these are still to be proved.
    private var dialFound: [StationEndpoint] = []
    /// The global addresses an introduction through the remote access
    /// service offered for each Core (``keepIntroduced(_:for:)``) that it
    /// has not proved its identity at: the service's word, not the Core's,
    /// so never in the Keychain. Held in memory until the app quits, and
    /// raced as found addresses are, so a connect with the service out of
    /// reach can still try them; one where the Core then proves itself is
    /// kept (link document section 7.1, R-IOS-16). Never logged.
    private var introduced: [Data: [StationEndpoint]] = [:]
    private var identifyGeneration = 0
    /// An address failed while its dial had not returned: the dial goes on
    /// to the next one when it does.
    private var advancePending = false
    /// The session reached the band since the operator chose this Core.
    private var reachedReady = false
    private var lostAttempts = 0
    private var signInLimitRetried = false
    private var dialling = false
    private var radioSeenConnected = false
    private var online = true
    /// Updated by the watch callback before its queued event is handled, so
    /// a tap immediately after path loss cannot launch a doomed first dial.
    private var latestNetworkPath: NetworkPath?
    private var networkObserved = false
    /// The interfaces the phone's network last came over.
    private var interfaces: [String] = []
    /// Last observed topology; retain known addresses across a failed read.
    private var networkSignature: NetworkSignature?
    private var offlineShown = false
    private var pairingTask: Task<PairedStation, Error>?
    /// Where Back goes from the address screen and a code screen opened
    /// without a Core: Find my Core or Your Cores.
    private var addFrom = Screen.cores
    /// What the browser last found, every Core it could read.
    private var found: [FoundStation] = []
    private var browsing = false
    /// Starts and stops the browser in the order the screens asked.
    private var browserTask: Task<Void, Never>?
    /// The one tap to try again from the Core-not-answering sheet.
    private var retryOneTap: OneTapTarget?
    /// The rename to try again from the Core-not-answering sheet: the Core and the name.
    private var retryRename: (id: Data, name: String)?
    /// The sign-in a rename of a Core that isn't connected is making.
    private var renameSignIn: RenameSignIn?

    /// A Core a one tap claims.
    private struct OneTapTarget {
        let endpoint: StationEndpoint
        let label: String
        /// The paired Core that forgot this phone, when it is one.
        let replacing: Data?
        /// The found Core's identity prefix.
        let id: String
    }

    // What the model publishes, handled one at a time in order.
    private enum Input: Sendable {
        /// The connection's state, and whether this phone was transmitting
        /// as it changed (read before the band's transmit hears of it).
        case connection(ConnectionState, keyed: Bool)
        /// A cancelled sleep leave replays the retained owner's link news.
        case retainedSleepConnection(ConnectionState, keyed: Bool, intent: Int, session: StationSession)
        case capabilities
        case network(NetworkPath)
        case found(StationBrowser.Update)
        /// The connected Core's name, as the band shows it, changed.
        case coreName
        /// Where the connected Core says it can be dialled changed.
        case coreAddresses
        /// This phone's key ended with the link: it was transmitting.
        case keyedLinkLost
    }

    /// The screens that look for Cores on this network.
    private static let browsingScreens: Set<Screen> = [.findCore, .cores, .typeAddress, .pairByCode]

    private let inputs: AsyncStream<Input>.Continuation
    private var inputPump: Task<Void, Never>?
    private var watches: Set<AnyCancellable> = []
    /// Only teardown of the current intent suppresses its connection events.
    /// An obsolete cleanup can finish after a new connection is already ready.
    /// Counts retain nested scopes, including Disconnect's session.leave interval.
    private var endingSessionCounts: [Int: Int] = [:]
    private var endingSession: Bool { endingSessionCounts[dialIntentGeneration, default: 0] > 0 }
    private struct SuppressedSleepConnection {
        let intent: Int
        let session: StationSession
        var loss: (state: ConnectionState, keyed: Bool)?
        var latest: (state: ConnectionState, keyed: Bool)?
    }
    private var suppressedSleepConnection: SuppressedSleepConnection?

    private func recordSuppressedSleepConnection(_ state: ConnectionState, keyed: Bool) {
        guard var held = suppressedSleepConnection, held.intent == dialIntentGeneration,
              app.session === held.session else { return }
        if case .waitingToRetry = state {
            held.loss = (state, keyed)
            held.latest = nil
        } else {
            held.latest = (state, keyed)
        }
        suppressedSleepConnection = held
    }

    private func finishSuppressedSleepConnection(intent: Int, boundLeave: BoundLeave?) {
        guard let held = suppressedSleepConnection, let boundLeave,
              held.intent == intent, held.session === boundLeave.session else { return }
        suppressedSleepConnection = nil
        guard intent == dialIntentGeneration, app.session === held.session,
              !boundLeave.handoff.wasSent else { return }
        if let loss = held.loss {
            inputs.yield(.retainedSleepConnection(loss.state, keyed: loss.keyed,
                                                  intent: intent, session: held.session))
        }
        if let latest = held.latest {
            inputs.yield(.retainedSleepConnection(latest.state, keyed: latest.keyed,
                                                  intent: intent, session: held.session))
        }
    }

    private func beginEndingSession() -> Int {
        let intent = dialIntentGeneration
        endingSessionCounts[intent, default: 0] += 1
        return intent
    }

    private func finishEndingSession(_ intent: Int) {
        let remaining = endingSessionCounts[intent, default: 0] - 1
        endingSessionCounts[intent] = remaining > 0 ? remaining : nil
    }

    private func advanceDialIntent() {
        dialIntentGeneration += 1
        // A pending Disconnect belongs to the intent it ended, not its replacement.
        disconnecting = false
    }

    private func retireDialIntent() {
        advanceDialIntent()
        restartOnRetire = nil
        dialling = false
        dialTarget = nil
    }

    #if DEBUG
    var diallingForTesting: Bool { dialling }
    var endingSessionForTesting: Bool { endingSession }
    #endif
    private var pairHoldTimer: (any LinkTimer)?
    private var backOnAirTimer: (any LinkTimer)?
    private var signInLimitTimer: (any LinkTimer)?
    private var redialTask: Task<Void, Never>?
    #if DEBUG
    /// How many times the flow has redialled at once, and that redial.
    private(set) var redialsForTesting = 0
    var redialTaskForTesting: Task<Void, Never>? { redialTask }
    #endif
    /// A first connection dialled again because the network came back: the
    /// failure of the try made offline may still be on its way, so the next
    /// one does not yet mean the Core isn't answering.
    private var redialledFirstConnection = false
    private let failures = OpenFailures()

    init(app: AppModel, dependencies: Dependencies) {
        self.app = app
        self.dependencies = dependencies
        phoneSettings = app.phoneSettings
        nameText = app.phoneSettings.deviceName ?? dependencies.kind.shortName
        screen = .welcome
        let (stream, continuation) = AsyncStream.makeStream(of: Input.self)
        inputs = continuation
        madeNewKey = phoneSettings.madeNewKey
        coresWithoutRename = Set(phoneSettings.coresWithoutRename.compactMap(Base64URL.decode))
        loadIdentity()
        app.main.transmit.thisDeviceId = identity?.id
        app.main.slices.thisDeviceId = identity?.id
        reloadCores()
        screen = cores.isEmpty && notice == nil ? .welcome : .cores

        inputPump = Task { [weak self] in
            for await input in stream {
                await self?.handle(input)
            }
        }
        app.$connection.dropFirst().sink { [weak self] state in
            guard let self else { return }
            let keyed = self.app.main.transmit.ptt.transmitting
            if self.endingSession {
                self.recordSuppressedSleepConnection(state, keyed: keyed)
                return
            }
            self.inputs.yield(.connection(state, keyed: keyed))
        }.store(in: &watches)
        app.mirror.$capabilities.dropFirst().sink { [weak self] _ in
            self?.inputs.yield(.capabilities)
        }.store(in: &watches)
        app.main.$coreName.dropFirst().sink { [weak self] _ in
            self?.inputs.yield(.coreName)
        }.store(in: &watches)
        app.main.$coreAddresses.dropFirst().sink { [weak self] _ in
            self?.inputs.yield(.coreAddresses)
        }.store(in: &watches)
        app.main.transmit.$ptt.dropFirst().sink { [weak self] snapshot in
            if snapshot.state == .linkLost {
                self?.inputs.yield(.keyedLinkLost)
            }
            self?.queueResumeHeldLook()
        }.store(in: &watches)
        app.main.transmit.$report.dropFirst().sink { [weak self] _ in
            self?.queueResumeHeldLook()
        }.store(in: &watches)
        app.main.transmit.$vox.dropFirst().sink { [weak self] _ in
            self?.queueResumeHeldLook()
        }.store(in: &watches)
        app.main.transmit.$coreOnAir.dropFirst().sink { [weak self] _ in
            self?.queueResumeHeldLook()
        }.store(in: &watches)
        app.snapshotCompleted = { [weak self] session in
            self?.initialSnapshotCompleted(on: session)
        }
        app.heldQuestionReceived = { [weak self] session, held in
            self?.presentHeld(held, from: session)
        }
        app.heldQuestionEnded = { [weak self] session in self?.clearHeld(from: session) }
        app.sessionEnded = { [weak self] session, end in
            guard self?.app.session === session else { return }
            self?.lastSessionEnd = end
            self?.clearHeld(from: session)
        }
        app.safetyMirrorChanged = { [weak self] session in
            guard let self, self.app.session === session else { return }
            self.queueResumeHeldLook()
        }
        app.main.$radioWaiting.dropFirst().sink { [weak self] _ in
            self?.inputs.yield(.capabilities)
        }.store(in: &watches)
        dependencies.network?.start { [weak self] path in
            self?.latestNetworkPath = path
            self?.inputs.yield(.network(path))
        }
        updateBrowsing()
    }

    deinit {
        inputs.finish()
    }

    /// The app's flow on this phone.
    static func live(app: AppModel, kind: DeviceKeyAuthenticator.Kind) -> ConnectionFlow {
        ConnectionFlow(app: app, dependencies: .live(kind: kind))
    }

    // MARK: Moving between screens

    func show(_ next: Screen) {
        addressProblem = nil
        if next != .typeAddress {
            cancelIdentifying()
        }
        screen = next
    }

    /// Stops Enter an address's identity check, if one runs; its answer is dropped.
    private func cancelIdentifying() {
        identifyGeneration += 1
        identifyTask?.cancel()
        identifyTask = nil
        identifying = false
    }

    /// Welcome's Find my Core: looking on this Wi-Fi, which asks iOS's Local
    /// Network question the first time, then Found it; a code and an
    /// address are offered there too.
    func findMyCore() {
        show(.findCore)
    }

    /// Back from the screen shown.
    func back() {
        switch screen {
        case .setUpCore, .findCore:
            show(.welcome)
        case .cores:
            if cores.isEmpty && notice == nil {
                show(.welcome)
            }
        case .typeAddress:
            show(addFrom)
        case .addresses:
            addressesFor = nil
            show(.cores)
        case .pairByCode:
            pairingTask?.cancel()
            show(pairTarget?.back ?? addFrom)
        case .welcome, .microphone, .band:
            break
        }
    }

    // MARK: Your Cores

    /// Connects to a listed Core, or opens its code screen when it needs
    /// pairing again. Its addresses are tried in order, `first` (when
    /// given) before the rest.
    func connect(to row: CoreRow, first: StationEndpoint? = nil) async {
        guard !keyUnreadable, connectingTo == nil, !renameBusy, removingCore == nil else {
            return
        }
        if row.needsPairing {
            await pairAgain(row)
            return
        }
        await dial(row.station, fresh: true, first: first)
    }

    /// Stops a connection still being made.
    func cancelConnecting() async {
        advancePending = false
        restartOnRetire = nil
        racer?.cancel()
        signInLimitTimer?.cancel()
        signInLimitTimer = nil
        guard await endSession() else { return }
        phase = .idle
        connectingTo = nil
        connectingNote = nil
    }

    /// The Core already ordered this roster by eligibility and inactivity.
    /// A refreshed revision keeps the operator's choice only while it is
    /// still listed and replaceable; no choice is sent by this callback.
    func presentHeld(_ held: LinkMessage.SessionHeld, from session: StationSession) {
        guard app.session === session else { return }
        let preferred = preferredTakeoverID ?? held.placeTaken?.byId ?? heldChoiceID
        heldChoiceID = held.devices.first(where: { $0.deviceId == preferred && $0.replaceable })?.deviceId
            ?? held.devices.first(where: \.replaceable)?.deviceId
        heldQuestion = held
        heldSession = session
        heldBusy = false
        heldProblem = nil
    }

    func clearHeld(from session: StationSession) {
        guard heldSession === session else { return }
        heldQuestion = nil
        heldChoiceID = nil
        heldSession = nil
        heldBusy = false
        heldProblem = nil
    }

    func selectHeldDevice(_ id: String) {
        guard heldQuestion?.devices.contains(where: { $0.deviceId == id && $0.replaceable }) == true,
              !heldBusy else { return }
        heldChoiceID = id
    }

    func answerHeld(cancel: Bool = false) async {
        guard let heldQuestion, let session = heldSession, app.session === session, !heldBusy else { return }
        let id = cancel ? "" : heldChoiceID ?? ""
        guard cancel || heldQuestion.devices.contains(where: { $0.deviceId == id && $0.replaceable }) else { return }
        heldBusy = true
        do {
            try await session.send(.sessionTakeover(.init(deviceId: id, revision: heldQuestion.revision)))
        } catch {
            heldBusy = false
            heldProblem = "The Core could not read that choice. Wait for its updated list or try again."
        }
    }

    /// The replaced device dials again by the operator's hand. If the Core
    /// is still full, its next roster starts on the device that took its
    /// place, provided that device is still there and replaceable.
    func takePlaceBack() async {
        guard case .placeTaken(_, let byId, _, _) = notice,
              let current, let row = cores.first(where: { $0.id == current.identityKey }) else { return }
        preferredTakeoverID = byId
        notice = nil
        await connect(to: row)
    }

    func dismissPlaceTaken() {
        if case .placeTaken = notice { notice = nil }
    }

    /// Pair opens the code screen for a listed Core, its address filled in
    /// (D70). A paired Core found on this network unclaimed, that allows one
    /// tap, is claimed again with one tap instead; one found taking the code
    /// fills in the address it was found at.
    func pairAgain(_ row: CoreRow) async {
        guard removingCore == nil else { return }
        if let found = row.found, let endpoint = found.endpoint, !found.claimed {
            if found.pairing == .click {
                await oneTap(OneTapTarget(endpoint: endpoint, label: row.label, replacing: row.id,
                                          id: found.identityPrefix))
                return
            }
            openCodeScreen(PairTarget(endpoint: endpoint, label: row.label, replacing: row.id, back: screen))
            return
        }
        // A Core this phone keeps no address for pairs again through the
        // remote access service, from anywhere.
        openCodeScreen(PairTarget(endpoint: row.station.endpoints.first, label: row.label, replacing: row.id,
                                  back: .cores))
    }

    // MARK: On this network (D71)

    /// The button of a Core found on this network: one tap, or the code
    /// screen with the found address filled in.
    func pairNearby(_ row: NearbyRow) async {
        guard removingCore == nil else { return }
        switch row.offer {
        case .oneTap:
            await oneTap(OneTapTarget(endpoint: row.endpoint, label: row.label, replacing: nil, id: row.id))
        case .code:
            useCode(row)
        case .closed:
            break
        }
    }

    /// Use code: the code screen for a found Core, its address filled in.
    func useCode(_ row: NearbyRow) {
        openCodeScreen(PairTarget(endpoint: row.endpoint, label: row.label, replacing: nil, back: screen))
    }

    /// Use code, after a one tap was refused.
    func useCodeAfterRefusal() {
        guard let problem = nearbyProblem else {
            return
        }
        nearbyProblem = nil
        if let row = nearby.first(where: { $0.id == problem.id }) {
            useCode(row)
        } else if let row = cores.first(where: { $0.found?.identityPrefix == problem.id }),
                  let endpoint = row.found?.endpoint ?? row.station.endpoints.first {
            openCodeScreen(PairTarget(endpoint: endpoint, label: row.label, replacing: row.id, back: screen))
        }
    }

    /// Claims a found Core with one tap, then goes straight on as a code
    /// pairing does: the microphone question after the first pairing, then
    /// the band. A refusal shows the Core's words as sent and offers the code.
    private func oneTap(_ target: OneTapTarget) async {
        guard !keyUnreadable, claiming == nil, !pairing, connectingTo == nil, removingCore == nil else {
            return
        }
        guard let identity else {
            notice = keyUnreadable ? .keyUnreadable : .keyUnavailable
            return
        }
        let name = phoneSettings.deviceName ?? dependencies.kind.shortName
        let client: PairingClient
        do {
            client = try PairingClient(identity: identity, name: name, kind: dependencies.kind,
                                       clock: dependencies.clock, transportFactory: observedFactory)
        } catch {
            nearbyProblem = NearbyProblem(id: target.id, words: Self.unusableNameText, offersCode: false)
            return
        }
        claiming = target.id
        nearbyProblem = nil
        retryOneTap = nil
        failures.reset()
        do {
            let paired = try await client.pairOnThisNetwork(endpoint: target.endpoint)
            claiming = nil
            let pairTarget = PairTarget(endpoint: target.endpoint, label: target.label, replacing: target.replacing,
                                        back: screen)
            if !(await finishPairing(paired, target: pairTarget, name: name)) {
                nearbyProblem = NearbyProblem(id: target.id, words: Self.couldNotKeepText, offersCode: false)
            }
        } catch {
            claiming = nil
            oneTapFailed(error, target: target)
        }
    }

    private func oneTapFailed(_ error: Error, target: OneTapTarget) {
        if error is CancellationError {
            return
        }
        let words: String
        switch error as? PairingError {
        case .refused(let reason, _)?:
            // The Core's own words, as sent (link document section 3.6).
            nearbyProblem = NearbyProblem(id: target.id, words: reason, offersCode: true)
            return
        case .didNotOpen(let denied)?:
            retryOneTap = target
            trouble = notAnswering(core: target.label, at: target.endpoint, localNetworkDenied: denied)
            return
        case .timedOut?:
            words = Self.timedOutText
        case .ended(let reason)?:
            // A Core that sent its reason has answered, even if another
            // observed route failed to open during this attempt.
            if reason == nil, failures.openFailed {
                retryOneTap = target
                trouble = notAnswering(core: target.label, at: target.endpoint)
                return
            }
            words = reason ?? Self.endedText
        case .cannotPair?:
            words = Self.cannotPairText
        case .identityMismatch?:
            words = Self.identityMismatchText
        case .alreadyPairing?:
            words = Self.alreadyPairingText
        default:
            words = Self.endedText
        }
        nearbyProblem = NearbyProblem(id: target.id, words: words, offersCode: true)
    }

    /// What the browser found: the paired Cores it names, and the others
    /// that take a new device.
    private func foundChanged(_ update: StationBrowser.Update) {
        found = update.stations
        lookingDenied = update.localNetworkDenied
        relist()
        proveFoundAddresses()
    }

    /// Lists the found Cores: a paired one by its key's prefix (which names
    /// it and proves nothing), the rest under On this network by D71.
    private func relist() {
        let listing = Self.listing(found, paired: cores.map(\.station))
        cores = cores.map { row in
            var next = row
            next.found = listing.matched[row.station.identityKey]
            return CoreRow(station: row.station, needsPairing: baseNeedsPairing(row.station)
                               || (next.found.map { !$0.claimed } ?? false), found: next.found)
        }
        nearby = listing.nearby
        if let problem = nearbyProblem, !nearby.contains(where: { $0.id == problem.id }),
           !cores.contains(where: { $0.found?.identityPrefix == problem.id }) {
            nearbyProblem = nil
        }
    }

    /// Splits what was found (D71): each found Core a paired one matches,
    /// and, in the order found, each other Core that takes a new device. An
    /// unclaimed Core allowing one tap offers it; one taking the code
    /// (claimed or not) offers the code; an unclaimed one with pairing
    /// closed is listed greyed. A claimed Core with pairing closed is not
    /// listed, nor one that isn't resolved yet.
    nonisolated static func listing(_ found: [FoundStation], paired: [PairedStation])
        -> (nearby: [NearbyRow], matched: [Data: FoundStation]) {
        var matched: [Data: FoundStation] = [:]
        var nearby: [NearbyRow] = []
        for station in found {
            if let known = paired.first(where: { station.matches(identityKey: $0.identityKey) }) {
                if matched[known.identityKey] == nil {
                    matched[known.identityKey] = station
                }
                continue
            }
            guard let endpoint = station.endpoint else {
                continue
            }
            let offer: NearbyRow.Offer
            switch (station.claimed, station.pairing) {
            case (false, .click):
                offer = .oneTap
            case (_, .code):
                offer = .code
            case (false, .closed):
                offer = .closed
            case (true, .click), (true, .closed):
                continue
            }
            nearby.append(NearbyRow(station: station, endpoint: endpoint, offer: offer))
        }
        return (nearby, matched)
    }

    private func updateBrowsing() {
        guard let browser = dependencies.browser else {
            return
        }
        let wanted = Self.browsingScreens.contains(screen)
        guard wanted != browsing else {
            return
        }
        browsing = wanted
        let previous = browserTask
        let inputs = inputs
        if wanted {
            browserTask = Task {
                await previous?.value
                await browser.start { update in
                    inputs.yield(.found(update))
                }
            }
        } else {
            found = []
            lookingDenied = false
            relist()
            browserTask = Task {
                await previous?.value
                await browser.stop()
            }
        }
    }

    /// Pair with a code, from Your Cores or Find my Core: the code alone,
    /// which pairs through the remote access service's mailbox on the
    /// code's number, from anywhere (R-IOS-08, picture 07). A Core's
    /// address is given with Enter an address, which goes on to the code.
    func pairWithCode() {
        guard removingCore == nil else { return }
        addFrom = screen == .findCore ? .findCore : .cores
        pairTarget = nil
        addressText = ""
        portText = Self.standardPortText
        resetCodeScreen()
        show(.pairByCode)
    }

    /// Enter an address, from Your Cores or Find my Core.
    func enterAddress() {
        guard removingCore == nil else { return }
        addFrom = screen == .findCore ? .findCore : .cores
        addressText = ""
        portText = Self.standardPortText
        show(.typeAddress)
    }

    /// Makes a new key, only on the operator's tap (D72): every Core then
    /// needs pairing again, and still lists the old key until it is removed there.
    func makeNewKey() {
        do {
            identity = try DeviceIdentity.makeNewKey(store: dependencies.keyStore)
        } catch {
            Self.logger.warning("A new device key could not be made")
            notice = .keyUnavailable
            return
        }
        keyUnreadable = false
        notice = nil
        let all = (try? dependencies.stations.all()) ?? []
        phoneSettings.coresNeedingPairing = Set(all.map { Base64URL.encode($0.identityKey) })
        phoneSettings.madeNewKey = !all.isEmpty
        madeNewKey = !all.isEmpty
        reloadCores()
    }

    /// Try again, after the key couldn't be reached.
    func retryKey() {
        notice = nil
        loadIdentity()
        reloadCores()
    }

    // MARK: Enter an address (D69)

    /// The address field changed from `old` to `new`: a pasted address with
    /// its port moves the port into the port field.
    func addressChanged(from old: String, to new: String) {
        addressProblem = nil
        // A paste inserts several characters at once; typing inserts one.
        guard new.count > old.count + 1 else {
            return
        }
        let parts = ManualAddress.split(new)
        guard let port = parts.port, ManualAddress.parsePort(port) != nil else {
            return
        }
        addressText = parts.host
        portText = port
    }

    /// Connect, on the address screen. An address a paired Core already has
    /// signs in to that Core, trying it first. Any other is asked which Core
    /// answers there, by its identity key, before anything is sent: a Core
    /// this phone has paired gets the address added to its row and signs in
    /// with the device key, with no code; an unknown Core goes on to the
    /// code, as before. Opened from a Core's Addresses page, only that Core
    /// is accepted: an address that reaches another Core, or none this
    /// phone can identify, is refused in plain words and nothing is kept.
    func connectToTypedAddress() async {
        guard removingCore == nil else { return }
        guard !identifying, let endpoint = readAddress() else {
            return
        }
        let key = endpoint.canonical
        let addingTo = addFrom == .addresses ? addressesFor : nil
        let listed = cores.first { row in row.station.endpoints.contains { $0.canonical == key } }
        if let listed, addingTo == nil || addingTo == listed.id {
            if addingTo != nil {
                // Already one of this Core's addresses.
                show(.addresses)
                return
            }
            await connect(to: listed, first: endpoint)
            return
        }
        identifying = true
        failures.reset()
        identifyGeneration += 1
        let generation = identifyGeneration
        let from = addFrom
        let factory = observedFactory
        let clock = dependencies.clock
        // From Your Cores a short check, so an address where nothing answers
        // reaches the code screen in seconds; from a Core's Addresses page,
        // where nothing else can follow, the link's whole connect time.
        let deadline = addingTo == nil ? Self.identifyDeadline : StationSession.connectDeadline
        let check = Task {
            await CoreIdentityProbe.identify(endpoint, transportFactory: factory, clock: clock, deadline: deadline)
        }
        identifyTask = check
        let outcome = await check.value
        // Back, or anything else the operator did meanwhile, left this
        // check behind: its answer is dropped.
        guard generation == identifyGeneration, screen == .typeAddress, addFrom == from,
              (addFrom == .addresses ? addressesFor : nil) == addingTo else {
            if generation == identifyGeneration {
                identifying = false
                identifyTask = nil
            }
            return
        }
        identifying = false
        identifyTask = nil
        let addingLabel = addingTo.flatMap { id in cores.first { $0.id == id }?.label }
        switch outcome {
        case .notReached(let denied):
            if denied {
                addressProblem = Self.lookingDeniedText
            } else if addingTo != nil {
                addressProblem = Self.nothingAnsweredText
            } else {
                // Not answered within the short check: on to the code, as
                // before; the pairing reports its own trouble if the Core
                // stays silent.
                openCodeScreen(PairTarget(endpoint: endpoint, label: nil, replacing: nil, back: .typeAddress))
            }
        case .unidentified:
            if let addingLabel {
                addressProblem = Self.otherCoreText(addingLabel)
                return
            }
            openCodeScreen(PairTarget(endpoint: endpoint, label: nil, replacing: nil, back: .typeAddress))
        case .identified(let identityKey):
            if let addingTo, addingTo != identityKey {
                // Never re-pointed: this address belongs to another Core.
                addressProblem = Self.otherCoreText(addingLabel ?? "")
                return
            }
            guard let row = cores.first(where: { $0.id == identityKey }) else {
                openCodeScreen(PairTarget(endpoint: endpoint, label: nil, replacing: nil, back: .typeAddress))
                return
            }
            if row.needsPairing {
                openCodeScreen(PairTarget(endpoint: endpoint, label: row.label, replacing: row.id, back: .typeAddress))
                return
            }
            let added: PairedStation?
            do {
                added = try dependencies.stations.addAddress(identityKey: identityKey, endpoint)
            } catch {
                Self.logger.warning("A Core's new address could not be kept in the Keychain")
                added = nil
            }
            guard let added else {
                addressProblem = Self.couldNotKeepAddressText
                return
            }
            reloadCores()
            if addingTo != nil {
                show(.addresses)
                return
            }
            show(.cores)
            await dial(added, fresh: true)
        }
    }

    // MARK: A Core's addresses

    /// Addresses, from a Core row's "⋯": its addresses, the one the last
    /// session reached it at marked, each with Remove, and Add an address.
    func showAddresses(_ row: CoreRow) {
        addressesFor = row.id
        show(.addresses)
    }

    /// The Core's name while Enter an address adds an address for it, from
    /// its Addresses page; nil otherwise.
    var addingAddressFor: String? {
        guard screen == .typeAddress, addFrom == .addresses, let id = addressesFor else {
            return nil
        }
        return cores.first { $0.id == id }?.label
    }

    /// The Core the Addresses page is for.
    var addressesStation: PairedStation? {
        addressesFor.flatMap { id in cores.first { $0.id == id }?.station }
    }

    /// Whether Remove can be used on the Addresses page: a Core keeps at
    /// least one address, so the last one's Remove is greyed with the reason.
    var canRemoveAddress: Bool {
        (addressesStation?.endpoints.count ?? 0) > 1
    }

    /// Remove, on the Addresses page. Never the Core's last address.
    func removeAddress(_ endpoint: StationEndpoint) {
        guard let id = addressesFor, canRemoveAddress else {
            return
        }
        do {
            _ = try dependencies.stations.removeAddress(identityKey: id, endpoint)
        } catch {
            Self.logger.warning("A Core's address could not be removed from the Keychain")
        }
        reloadCores()
    }

    /// Add an address, on the Addresses page: the same address entry, for
    /// this Core only.
    func addAddress() {
        guard addressesFor != nil else {
            return
        }
        addFrom = .addresses
        addressText = ""
        portText = Self.standardPortText
        show(.typeAddress)
    }

    /// The typed address, or nil with the reason shown.
    private func readAddress() -> StationEndpoint? {
        guard ManualAddress.parsePort(portText) != nil else {
            addressProblem = Self.portProblemText
            return nil
        }
        guard let endpoint = ManualAddress.parse(host: addressText, port: portText) else {
            addressProblem = Self.addressProblemText
            return nil
        }
        addressProblem = nil
        return endpoint
    }

    // MARK: Pair with a code

    private func openCodeScreen(_ target: PairTarget) {
        pairTarget = target
        resetCodeScreen()
        show(.pairByCode)
    }

    private func resetCodeScreen() {
        codeText = ""
        nameText = phoneSettings.deviceName ?? dependencies.kind.shortName
        pairingProblem = nil
        pairHoldTimer?.cancel()
        pairHeld = false
    }

    /// Clears the closed-pairing notice so Pair can be tried again.
    func clearPairingProblem() {
        pairingProblem = nil
    }

    /// Replaces a word of the typed code with one of the list's.
    func useSuggestion(_ suggestion: String, for word: String) {
        var parts = Self.codeParts(codeText)
        guard let index = parts.lastIndex(of: word) else {
            return
        }
        parts[index] = suggestion
        codeText = parts.joined(separator: "-")
        pairingProblem = nil
    }

    /// Pairs by the typed code with the Core, straight to its address or,
    /// with none, through the remote access service's mailbox on the code's
    /// number; then connects straight away and signs in with the device key
    /// on a new connection, by the ways that reach it from anywhere.
    func pair() async {
        guard removingCore == nil else { return }
        guard !pairing, !pairHeld else {
            return
        }
        pairingProblem = nil
        let target = pairTarget ?? PairTarget(endpoint: nil, label: nil, replacing: nil, back: addFrom)
        let name = nameText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard DeviceName.isUsable(name) else {
            pairingProblem = .name(name.isEmpty ? Self.blankNameText : Self.unusableNameText)
            return
        }
        guard let code = PairingCodeText.normalise(codeText) else {
            pairingProblem = Self.codeProblem(codeText)
            return
        }
        guard let identity else {
            notice = keyUnreadable ? .keyUnreadable : .keyUnavailable
            return
        }
        let carrier: PairingCarrier
        if let endpoint = target.endpoint {
            carrier = .direct(endpoint)
        } else if let nameplate = PairingClient.nameplate(ofNormalised: code) {
            carrier = .rendezvous(server: dependencies.rendezvousServers, nameplate: nameplate)
        } else {
            pairingProblem = Self.codeProblem(codeText)
            return
        }
        let client: PairingClient
        do {
            client = try PairingClient(identity: identity, name: name, kind: dependencies.kind,
                                       clock: dependencies.clock, transportFactory: observedFactory,
                                       rendezvousTransportFactory: dependencies.rendezvousTransportFactory)
        } catch {
            pairingProblem = .name(Self.unusableNameText)
            return
        }
        pairing = true
        failures.reset()
        let task = Task { try await client.pair(code: code, via: carrier) }
        pairingTask = task
        do {
            let paired = try await task.value
            pairing = false
            pairingTask = nil
            await finishPairing(paired, target: target, name: name)
        } catch {
            pairing = false
            pairingTask = nil
            pairingFailed(error, target: target)
        }
    }

    /// Keeps the pairing and connects; false when the phone couldn't keep it.
    @discardableResult
    private func finishPairing(_ paired: PairedStation, target: PairTarget, name: String) async -> Bool {
        let before = (try? dependencies.stations.all()) ?? []
        let first = before.isEmpty
        phoneSettings.deviceName = name
        var needing = phoneSettings.coresNeedingPairing
        if let replaced = target.replacing, replaced != paired.identityKey {
            try? dependencies.stations.remove(identityKey: replaced)
            needing.remove(Base64URL.encode(replaced))
        }
        needing.remove(Base64URL.encode(paired.identityKey))
        phoneSettings.coresNeedingPairing = needing
        if needing.isEmpty {
            phoneSettings.madeNewKey = false
            madeNewKey = false
        }
        // Pairing again with a Core this phone already keeps adds the
        // address it paired at, first, to the ones it had.
        var kept = paired
        if let earlier = before.first(where: { $0.identityKey == paired.identityKey }) {
            kept = earlier
            kept.label = paired.label
            if target.endpoint == nil {
                // A successful authenticated mailbox exchange proves this
                // Core can answer via the service now. Recheck its current
                // capability after sign-in instead of retaining an old 0.
                kept.controlChannelVersion = nil
                kept.controlChannelObservedAtUnixMs = nil
            }
            for endpoint in paired.endpoints.reversed() {
                kept.add(endpoint)
            }
        }
        do {
            try dependencies.stations.save(kept)
        } catch {
            Self.logger.warning("The paired Core could not be kept in the Keychain")
            pairingProblem = .failed(Self.couldNotKeepText)
            return false
        }
        notice = nil
        codeText = ""
        pairedWith = kept
        reloadCores()
        // The microphone is asked for right after the first pairing, so iOS
        // never interrupts the first transmission (item 4).
        screen = first && dependencies.microphone.needsAsking ? .microphone : .cores
        await dial(kept, fresh: true)
        return true
    }

    private func pairingFailed(_ error: Error, target: PairTarget) {
        if error is CancellationError {
            return
        }
        guard let failure = error as? PairingError else {
            pairingProblem = error is DeviceKeyAuthenticator.UnusableName ? .name(Self.unusableNameText)
                : .failed(Self.endedText)
            return
        }
        switch failure {
        case .notACode:
            pairingProblem = Self.codeProblem(codeText)
        case .wrongCode(let wait, let reason):
            if wait == .zero {
                // The fifth wrong code in a row closed pairing at the Core.
                pairingProblem = .pairingClosed
            } else {
                pairingProblem = .wrongCode(reason: reason, waitSeconds: Self.seconds(wait))
                hold(for: wait)
            }
        case .refused(let reason, let wait):
            pairingProblem = .refused(reason: reason, waitSeconds: Self.seconds(wait))
            if wait > .zero {
                hold(for: wait)
            }
        case .didNotOpen(let denied):
            if let endpoint = target.endpoint {
                trouble = notAnswering(core: target.label ?? Self.addressText(endpoint), at: endpoint,
                                       localNetworkDenied: denied)
            } else {
                pairingProblem = .failed(Self.endedText)
            }
        case .timedOut:
            pairingProblem = .failed(Self.timedOutText)
        case .ended(let reason):
            if reason == nil, failures.openFailed, let endpoint = target.endpoint {
                trouble = notAnswering(core: target.label ?? Self.addressText(endpoint), at: endpoint)
            } else {
                pairingProblem = .failed(reason ?? Self.endedText)
            }
        case .cannotPair:
            pairingProblem = .failed(Self.cannotPairText)
        case .weakHashSettings:
            pairingProblem = .failed(Self.weakHashText)
        case .identityMismatch:
            pairingProblem = .failed(Self.identityMismatchText)
        case .alreadyPairing:
            pairingProblem = .failed(Self.alreadyPairingText)
        }
    }

    /// Pair waits until the Core's new code appears.
    private func hold(for wait: Duration) {
        pairHeld = true
        pairHoldTimer?.cancel()
        pairHoldTimer = dependencies.clock.schedule(after: wait) { [weak self] in
            await self?.releaseHold()
        }
    }

    private func releaseHold() {
        pairHeld = false
        pairHoldTimer = nil
    }

    // MARK: The microphone question

    /// Allow the microphone or Not now; then the band, or Your Cores while
    /// the connection is still being made.
    func answerMicrophone(allow: Bool) async {
        if allow {
            _ = await dependencies.microphone.ask()
        }
        goToBand()
    }

    /// Go to the band, from the paired screen.
    func goToBand() {
        guard screen == .microphone else {
            return
        }
        screen = phase == .connected ? .band : .cores
    }

    // MARK: The band

    /// Cancel, while the link is lost: no more tries.
    func cancelReconnecting() async {
        guard phase == .lost || (phase == .connecting && reachedReady) else {
            return
        }
        guard await endSession() else { return }
        phase = .stopped
        connectingTo = nil
        linkLost = LinkLost(attempt: linkLost?.attempt ?? lostAttempts, retryAt: nil, stopped: true,
                            words: linkLost?.words, keyed: linkLost?.keyed ?? false)
        // Stopped is LINK LOST with Reconnect, offline or not: nothing
        // reconnects by itself after Cancel.
        refreshOffline()
    }

    /// Reconnect, after Cancel.
    func reconnect() async {
        guard phase == .stopped, let station = current, removingCore == nil else {
            return
        }
        lostAttempts = 1
        linkLost = LinkLost(attempt: 1, retryAt: nil, stopped: false, keyed: linkLost?.keyed ?? false)
        await dial(station, fresh: false)
    }

    /// Leaves the band for Your Cores, ending the session.
    func leaveBand() async {
        guard await endSession() else { return }
        showCoresAfterLeaving()
    }

    // MARK: Disconnect, on the Radio tab

    /// The verb that ends this device's session with no away time (link
    /// section 9.1, `sessionHolderVersion` 1 at minor 11).
    static let leaveVerb = "session.leave"
    static let leaveCapability = "sessionHolderVersion"
    static let leaveMinimumMinor: UInt16 = 11
    /// How long the Core has to answer `session.leave` before the phone closes anyway.
    static let leaveTimeout: Duration = .seconds(2)

    /// Whether a Core with these capabilities, at this agreed minor, takes `session.leave`.
    static func leaves(_ mirror: MirrorStore) -> Bool {
        (mirror.agreedMinor ?? 0) >= leaveMinimumMinor && mirror.capabilityVersion(leaveCapability) >= 1
    }

    /// This phone is transmitting (a key of its own is on or on its way):
    /// Disconnect is not offered.
    var transmitting: Bool { app.main.transmit.ptt.transmitting }

    /// Disconnect is on its way: the button waits.
    @Published private(set) var disconnecting = false
    #if DEBUG
    var sleepLeaveResultForTesting: (@MainActor () async -> Void)?
    var retainedSleepInputForTesting: (@MainActor () async -> Void)?
    var retainedSleepDecisionForTesting: (@MainActor (Bool) -> Void)?
    #endif

    /// Disconnect, on the Radio tab (spec section 5.2 item 5): the session
    /// ends cleanly (`session.leave` when the Core takes it, so its place is
    /// free at once; otherwise a close), the sound and the band stop, and
    /// Your Cores shows. A connection still being made, or a link being
    /// retried, is cancelled. Nothing reconnects by itself afterwards; the
    /// operator connects again from Your Cores.
    func disconnect() async {
        _ = await disconnectCurrent()
    }

    /// Sleep may leave only the exact session whose idle expiry was admitted.
    func disconnect(ifCurrent expected: StationSession,
                    authority: CommandSendPermit,
                    stillAllowed: @escaping @MainActor () -> Bool) async -> Bool {
        guard let captured = app.captureSleepLeaveSender(ifCurrent: expected) else { return false }
        let handoff = CommandHandoffReceipt()
        return await disconnectCurrent(stillAllowed: { [weak self] in
            guard let self else { return false }
            return self.app.session === expected && stillAllowed()
        }, boundLeave: BoundLeave(session: expected, sender: captured, authority: authority, handoff: handoff))
    }

    private struct BoundLeave: Sendable {
        let session: StationSession
        let sender: CommandClient.CommandSender
        let authority: CommandSendPermit
        let handoff: CommandHandoffReceipt
    }

    /// True only when this disconnect still owns the session after every
    /// suspension. Remove Core requires that proof before touching storage.
    private func disconnectCurrent(stillAllowed: @MainActor () -> Bool = { true },
                                   boundLeave: BoundLeave? = nil) async -> Bool {
        guard stillAllowed(), !transmitting, !disconnecting else {
            return false
        }
        // A guarded expiry can still be cancelled while its leave waits in
        // the command queue. Keep that session's retry and better-look intent
        // until the expiry is committed; generation cannot be rolled back.
        if boundLeave == nil {
            signInLimitTimer?.cancel()
            signInLimitTimer = nil
            redialTask?.cancel()
            retireDialIntent()
        }
        // From here what the session reports is the flow's own doing: the
        // Core closing after an admitted leave is not a lost link.
        if let boundLeave {
            suppressedSleepConnection = SuppressedSleepConnection(intent: dialIntentGeneration,
                                                                   session: boundLeave.session)
        }
        let intent = beginEndingSession()
        var disconnectIntent = intent
        disconnecting = true
        defer {
            finishEndingSession(intent)
            finishSuppressedSleepConnection(intent: intent, boundLeave: boundLeave)
            if disconnectIntent == dialIntentGeneration { disconnecting = false }
        }
        if phase == .connected, Self.leaves(app.mirror) {
            do {
                let result: CommandResult
                if let boundLeave {
                    result = try await app.commands.invokeBound(Self.leaveVerb, arguments: [],
                        timeout: Self.leaveTimeout, sender: boundLeave.sender,
                        stillAllowed: { !boundLeave.authority.isRevoked }, authority: boundLeave.authority,
                        handoffReceipt: boundLeave.handoff)
                } else {
                    result = try await app.commands.invoke(Self.leaveVerb, arguments: [], timeout: Self.leaveTimeout)
                }
                if !result.accepted {
                    Self.logger.info("The Core did not take session.leave: \(result.reason, privacy: .private)")
                }
            } catch {
                Self.logger.info("session.leave had no answer: \(String(describing: error), privacy: .public)")
            }
        }
        #if DEBUG
        if boundLeave != nil { await sleepLeaveResultForTesting?() }
        #endif
        guard intent == dialIntentGeneration else { return false }
        if let boundLeave {
            // Once Core has the leave, a later timer change cannot undo it.
            // Finish only the captured old session, never a replacement.
            let current = app.session
            guard stillAllowed() || (boundLeave.handoff.wasSent
                && (current == nil || current === boundLeave.session)) else { return false }
        } else {
            guard stillAllowed() else { return false }
        }
        if boundLeave != nil {
            signInLimitTimer?.cancel()
            signInLimitTimer = nil
            redialTask?.cancel()
            retireDialIntent()
            disconnectIntent = dialIntentGeneration
            disconnecting = true
        }
        guard await endSession(invalidateDialIntent: false) else { return false }
        showCoresAfterLeaving()
        return true
    }

    /// Your Cores, after the band was left on purpose: nothing of the
    /// session is kept showing, and nothing dials again.
    private func showCoresAfterLeaving() {
        phase = .idle
        linkLost = nil
        radioOff = nil
        offline = false
        offlineShown = false
        olderCore = nil
        connectingTo = nil
        connectingNote = nil
        dialTarget = nil
        show(.cores)
    }

    // MARK: Removing a saved Core (D91)

    /// A reason displayed with a disabled Remove action. An unrelated saved
    /// Core can be removed without interrupting the one on the air.
    func removeDisabledReason(identityKey id: Data) -> String? {
        guard cores.contains(where: { $0.id == id }) else { return Self.removeBusyText }
        // The previous Core is still being retired by a new dial. Its
        // teardown must finish before that old saved entry can be removed.
        if let dialTarget, dialTarget != id, current?.identityKey == id {
            return Self.removeBusyText
        }
        if removingCore != nil || disconnecting || pairing || claiming != nil || renameBusy || renaming != nil {
            return Self.removeBusyText
        }
        if transmitting, current?.identityKey == id || connectingTo == id || dialTarget == id {
            return Self.removeTransmitText
        }
        return nil
    }

    func requestRemove(identityKey id: Data) {
        guard removeDisabledReason(identityKey: id) == nil,
              let row = cores.first(where: { $0.id == id }) else { return }
        removeCandidate = row
    }

    func cancelRemove() {
        guard removeCandidate != nil else { return }
        removeCandidate = nil
    }

    func confirmRemove() {
        guard let candidate = removeCandidate else { return }
        removeCandidate = nil
        Task { await removeCore(identityKey: candidate.id) }
    }

    /// Disconnects the selected Core first and then removes its local saved
    /// record. A failed write leaves the row and its local flags intact.
    @discardableResult
    func removeCore(identityKey id: Data) async -> Bool {
        guard let row = cores.first(where: { $0.id == id }),
              removeDisabledReason(identityKey: id) == nil else { return false }
        removingCore = id
        defer { removingCore = nil }
        // A fresh dial can be retiring OLD while NEW has already become
        // the intent owner. In that interval OLD is only historical state.
        let selected = dialTarget.map { $0 == id }
            ?? ((phase != .idle && current?.identityKey == id) || connectingTo == id)
        if selected {
            guard await disconnectCurrent() else { return false }
        }
        // The disconnect may have yielded while a newer intent took over.
        // Never forget an identity that still belongs to an active session.
        guard (phase == .idle || current?.identityKey != id || (dialTarget != nil && dialTarget != id)),
              connectingTo != id, dialTarget != id else { return false }
        do {
            try dependencies.stations.remove(identityKey: id)
        } catch {
            Self.logger.warning("The saved Core could not be removed from the Keychain")
            removeProblem = RemoveProblem(id: id, core: row.label)
            return false
        }
        removeProblem = nil
        introduced[id] = nil
        let encoded = Base64URL.encode(id)
        var needing = phoneSettings.coresNeedingPairing
        needing.remove(encoded)
        phoneSettings.coresNeedingPairing = needing
        if needing.isEmpty {
            phoneSettings.madeNewKey = false
            madeNewKey = false
        }
        coresWithoutRename.remove(id)
        phoneSettings.coresWithoutRename = Set(coresWithoutRename.map(Base64URL.encode))
        serviceDiscoveryUnknown.remove(id)
        if current?.identityKey == id { current = nil }
        if addressesFor == id {
            addressesFor = nil
            show(.cores)
        }
        if renaming == id { closeRename() }
        if retryRename?.id == id { retryRename = nil }
        if retryOneTap?.replacing == id { retryOneTap = nil }
        if pairTarget?.replacing == id { pairTarget = nil }
        if pairedWith?.identityKey == id { pairedWith = nil }
        if removedNoticeCoreID == id {
            if case .removed = notice { notice = nil }
            removedNoticeCoreID = nil
        }
        reloadCores()
        return true
    }

    // MARK: Trouble

    /// Try again, on the Core-not-answering sheet.
    func tryAgain() async {
        guard case .notAnswering = trouble else {
            return
        }
        trouble = nil
        if let rename = retryRename {
            retryRename = nil
            renaming = rename.id
            renameText = rename.name
            renameProblem = nil
            await saveRename()
        } else if let target = retryOneTap {
            retryOneTap = nil
            await oneTap(target)
        } else if screen == .pairByCode {
            await pair()
        } else if let station = current {
            await dial(station, fresh: true)
        }
    }

    func dismissTrouble() {
        trouble = nil
        retryOneTap = nil
        retryRename = nil
    }

    // MARK: Connecting

    /// Opens eligible direct and service paths together, checking the saved
    /// Core identity on every connection before one session signs in.
    private func dial(_ station: PairedStation, fresh: Bool, first: StationEndpoint? = nil) async {
        guard !dialling, removingCore == nil else {
            return
        }
        guard let authenticator = makeAuthenticator(), let identity else {
            return
        }
        dialling = true
        dialTarget = station.identityKey
        advanceDialIntent()
        let intent = dialIntentGeneration
        guard await endSession(invalidateDialIntent: false) else { return }
        guard intent == dialIntentGeneration else { return }
        lastSessionEnd = nil
        raceGeneration += 1
        let generation = raceGeneration
        let pathOnline = latestNetworkPath?.online ?? online
        let found = (cores.first { $0.id == station.identityKey }?.found?.dialable ?? [])
            + (introduced[station.identityKey] ?? [])
        proved = []
        dialFound = found
        dialPlan = Self.reachPlan(station, first: first, found: found, throughService: servesFromAnywhere(station))
        dialIndex = 0
        advancePending = false
        service = nil
        winnerPath = nil
        attemptDenied = false
        attempt = ConnectionAttempt(started: dependencies.now())
        current = station
        connectingTo = station.identityKey
        connectingNote = pathOnline ? nil : Self.offlineWaitText
        trouble = nil
        if fresh {
            notice = nil
            reachedReady = false
            lostAttempts = 0
            signInLimitRetried = false
            linkLost = nil
            radioOff = nil
            olderCore = nil
            offlineShown = false
        }
        radioSeenConnected = false
        failures.reset()
        redialledFirstConnection = false
        phase = .connecting
        refreshOffline()
        guard !dialPlan.isEmpty else {
            dialling = false
            await showNotAnswering(localNetworkDenied: lookingDenied)
            return
        }
        if !pathOnline {
            // A first connect or explicit Reconnect has no session retry
            // scheduler yet. Keep its intent until an actual online path.
            dialling = false
            return
        }
        if let make = serviceRouteMaker, servesFromAnywhere(station) {
            let route = ObservedServiceRoute(inner: make(station, identity), failures: failures)
            service = route
        }
        let racer = PathRacer()
        self.racer = racer
        let direct = dialPlan.compactMap { route -> StationEndpoint? in
            if case .address(let endpoint) = route { return endpoint }
            return nil
        }
        let result = await racer.run(direct: direct, service: service, trust: station.trust,
                                     transportFactory: observedFactory, clock: dependencies.clock,
                                     serviceAddress: dependencies.rendezvousServers.first?.host ?? "",
                                     preferredFirst: first != nil, resolveNames: dependencies.directResolver,
                                     retainBetterStandby: true, networks: dependencies.networks)
        guard generation == raceGeneration, self.racer === racer else {
            racer.cancel()
            await restartRetiredRaceIfNeeded(intent: intent)
            return
        }
        attempt.tries = result.tries
        dialPlan = result.routes.map { route in
            switch route {
            case .address(let endpoint): .address(endpoint)
            case .service: .throughService
            }
        }
        attemptDenied = result.localNetworkDenied
        if let hello = result.incompatible {
            dialling = false
            await ended(Refusal(LinkVersionPolicy.refusal(station: hello.supportedMajors,
                                                         app: LinkVersionPolicy.supportedMajors)))
            return
        }
        if latestNetworkPath?.online == false {
            // The monitor callback can arrive while the race is finishing,
            // before its queued network input retires this generation. A
            // route failure on the lost path is a wait, not terminal trouble;
            // an already verified winner must close before authentication.
            racer.cancel()
            self.racer = nil
            dialling = false
            connectingNote = Self.offlineWaitText
            return
        }
        guard let winner = result.winner else {
            dialling = false
            if !result.tries.isEmpty && result.tries.allSatisfy({ $0.outcome == .notThisCore }) {
                await ended(Refusal(.authentication(
                    "This Core is not the one this app paired with. Pair with it again."),
                    code: .identityChanged))
                return
            }
            await showNotAnswering(localNetworkDenied: result.localNetworkDenied)
            return
        }
        dialIndex = winner.row
        winnerPath = winner.path
        // Each later same-service session attempt gets its own dialer.
        // A retired ControlDial can finish late; its mutable last-pair
        // getters must never be shared with the currently opened retry.
        let retryServiceRoute: (@Sendable () -> any CoreServiceRoute)?
        if let make = serviceRouteMaker {
            let retryStation = station
            let retryIdentity = identity
            let retryFailures = failures
            retryServiceRoute = {
                let route = make(retryStation, retryIdentity)
                return ObservedServiceRoute(inner: route, failures: retryFailures)
            }
        } else {
            retryServiceRoute = nil
        }
        let adopted = await app.connect(winner: winner, name: station.label.isEmpty ? nil : station.label,
                                        trust: station.trust, authenticator: authenticator,
                                        clock: dependencies.clock, transportFactory: observedFactory,
                                        serviceRetryRoute: retryServiceRoute,
                                        raceAgain: { [weak self] in
                                            guard let self else { throw LinkTransportError.failed("automatic route ended") }
                                            return try await self.selectRetryRoute(station, intent: intent)
                                        })
        guard generation == raceGeneration, self.racer === racer else {
            racer.cancel()
            await restartRetiredRaceIfNeeded(intent: intent)
            return
        }
        if !adopted {
            if latestNetworkPath?.online == false {
                racer.cancel()
                self.racer = nil
                dialling = false
                connectingNote = Self.offlineWaitText
                return
            }
            attempt.end(dialIndex, as: .noAnswer)
            dialling = false
            await showNotAnswering(localNetworkDenied: attemptDenied)
            return
        }
        guard racer.transferInitialWinner() else {
            dialling = false
            await showNotAnswering(localNetworkDenied: attemptDenied)
            return
        }
        if !online { await app.holdRetries() }
        dialling = false
    }

    /// StationSession calls this from its own selection task each time its
    /// existing retry policy fires. The selected transport has already
    /// passed the saved-identity check and is never opened a second time.
    private func selectRetryRoute(_ station: PairedStation, intent: Int) async throws -> PathRacer.Winner {
        guard intent == dialIntentGeneration, phase != .stopped, online,
              let identity else { throw LinkTransportError.failed("automatic route retired") }
        let found = cores.first { $0.id == station.identityKey }?.found?.dialable ?? []
        let plan = Self.reachPlan(station, first: nil, found: found,
                                  throughService: servesFromAnywhere(station))
        let direct = plan.compactMap { route -> StationEndpoint? in
            if case .address(let endpoint) = route { return endpoint }
            return nil
        }
        let through = servesFromAnywhere(station) ? serviceRouteMaker.map {
            ObservedServiceRoute(inner: $0(station, identity), failures: failures)
        } : nil
        let selector = PathRacer()
        racer?.cancel()
        racer = selector
        raceGeneration += 1
        let generation = raceGeneration
        let result = await withTaskCancellationHandler {
            await selector.run(direct: direct, service: through, trust: station.trust,
                               transportFactory: observedFactory, clock: dependencies.clock,
                               serviceAddress: dependencies.rendezvousServers.first?.host ?? "",
                               resolveNames: dependencies.directResolver, networks: dependencies.networks)
        } onCancel: {
            selector.cancel()
        }
        guard !Task.isCancelled, intent == dialIntentGeneration,
              generation == raceGeneration, racer === selector, online,
              let winner = result.winner else {
            selector.cancel()
            throw LinkTransportError.failed("no verified automatic route")
        }
        attempt = ConnectionAttempt(started: dependencies.now(), tries: result.tries)
        dialPlan = result.routes.map { route in
            switch route {
            case .address(let endpoint): .address(endpoint)
            case .service: .throughService
            }
        }
        dialIndex = winner.row
        winnerPath = winner.path
        if case .service(let route) = winner.route { service = route }
        guard selector.transferInitialWinner() else {
            selector.cancel()
            throw LinkTransportError.failed("retired verified route")
        }
        racer = nil
        return winner
    }

    private func restartRetiredRaceIfNeeded(intent: Int) async {
        guard intent == dialIntentGeneration else { return }
        let pending = restartOnRetire
        restartOnRetire = nil
        dialling = false
        if let pending, pending.intent == intent, phase == .connecting,
           latestNetworkPath?.online != false {
            await dial(pending.station, fresh: false)
        }
    }

    /// The ways a connect tries, in order: the Core's addresses as kept
    /// (the one that worked last first, `first` before all when given),
    /// then where the Core says it can be dialled and the global addresses
    /// this phone proved (``PairedStation/directAddresses``), then `found`,
    /// every address it was found at on this network (global IPv6 first,
    /// ``FoundStation/dialable``), each only once, then through the remote
    /// access service when `throughService`. The race ranks each by the
    /// network it is on (link document section 21.1); this order only
    /// decides which of equal rank starts first.
    nonisolated static func reachPlan(_ station: PairedStation, first: StationEndpoint?, found: [StationEndpoint],
                                      throughService: Bool) -> [Route] {
        var addresses = station.dialOrder
        if let first {
            addresses = [first] + addresses.filter { $0.canonical != first.canonical }
        }
        for endpoint in station.directAddresses + found
            where !addresses.contains(where: { $0.canonical == endpoint.canonical }) {
            addresses.append(endpoint)
        }
        var plan = addresses.map(Route.address)
        if throughService {
            plan.append(.throughService)
        }
        return plan
    }

    /// ``reachPlan(_:first:found:throughService:)`` with one found address, or none.
    nonisolated static func reachPlan(_ station: PairedStation, first: StationEndpoint?, found: StationEndpoint?,
                                      throughService: Bool) -> [Route] {
        reachPlan(station, first: first, found: found.map { [$0] } ?? [], throughService: throughService)
    }

    /// Whether a connect to `station` may go through the remote access
    /// service: there is one, and the Core declared the control connection
    /// at its last sign-in, or has had none (``PairedStation/reachableFromAnywhere``).
    private func servesFromAnywhere(_ station: PairedStation) -> Bool {
        dependencies.serviceRoute != nil &&
            (serviceDiscoveryUnknown.contains(station.identityKey) ||
             station.reachableFromAnywhere(now: dependencies.now()))
    }

    /// The authenticator a sign-in uses, or nil with the reason shown.
    private func makeAuthenticator() -> (any StationAuthenticator)? {
        if let injected = dependencies.sessionAuthenticator { return injected }
        guard let identity else {
            notice = keyUnreadable ? .keyUnreadable : .keyUnavailable
            return nil
        }
        let name = phoneSettings.deviceName ?? dependencies.kind.shortName
        guard let authenticator = try? DeviceKeyAuthenticator(identity: identity, name: name, kind: dependencies.kind)
        else {
            notice = .words(Self.unusableNameText)
            return nil
        }
        return authenticator
    }

    /// Dials the plan's current way.
    private func dialRoute(_ station: PairedStation, fresh: Bool) async {
        guard !dialling, dialPlan.indices.contains(dialIndex), let authenticator = makeAuthenticator(),
              let identity else {
            return
        }
        let route = dialPlan[dialIndex]
        var through: (any CoreServiceRoute)?
        switch route {
        case .address(let endpoint):
            attempt.begin(ConnectionAttempt.path(for: endpoint, networks: dependencies.networks()),
                          address: Self.addressText(endpoint))
        case .throughService:
            guard let make = serviceRouteMaker else {
                return
            }
            let made = ObservedServiceRoute(inner: make(station, identity), failures: failures)
            through = made
            service = made
            attempt.tries.append(ConnectionAttempt.Try(path: .direct,
                                                       address: dependencies.rendezvousServers.first?.host ?? "",
                                                       throughService: true))
        }
        dialling = true
        // The session before this one has ended before this one dials.
        guard await endSession(invalidateDialIntent: false) else { return }
        current = station
        connectingTo = station.identityKey
        connectingNote = online ? nil : Self.offlineWaitText
        trouble = nil
        if fresh {
            notice = nil
            reachedReady = false
            lostAttempts = 0
            signInLimitRetried = false
            linkLost = nil
            radioOff = nil
            olderCore = nil
            offlineShown = false
        }
        radioSeenConnected = false
        failures.reset()
        redialledFirstConnection = false
        phase = .connecting
        refreshOffline()
        switch route {
        case .address(let endpoint):
            await app.connect(to: endpoint, trust: station.trust, authenticator: authenticator,
                              transportFactory: observedFactory, clock: dependencies.clock)
        case .throughService:
            if let through {
                await app.connect(through: through, name: station.label.isEmpty ? nil : station.label,
                                  trust: station.trust, authenticator: authenticator, clock: dependencies.clock)
            }
        }
        if !online {
            // This one try was the operator's; offline, the next waits for a network.
            await app.holdRetries()
        }
        dialling = false
        // This way failed while it was still being dialled: the next one
        // goes now.
        if advancePending {
            advancePending = false
            await dialRoute(station, fresh: false)
        }
    }

    /// How the way being tried went, when it failed before signing in: the
    /// service's own reading of a dial through it, otherwise no answer when
    /// the connection never opened and no answer in time when it opened and
    /// went quiet.
    private func failedOutcome() -> ConnectionAttempt.Outcome {
        if dialPlan.indices.contains(dialIndex), dialPlan[dialIndex] == .throughService,
           let tried = service?.lastTry, tried.outcome != .trying, tried.outcome != .connected {
            return tried.outcome
        }
        return failures.openFailed ? .noAnswer : .timedOut
    }

    /// Ends the session the flow started; what it reports while ending is
    /// the flow's own doing, not the Core's.
    private func endSession(invalidateDialIntent: Bool = true) async -> Bool {
        if invalidateDialIntent {
            retireDialIntent()
        }
        racer?.cancel()
        racer = nil
        lookRacer?.cancel()
        lookRacer = nil
        lookTimer?.cancel()
        lookTimer = nil
        lookTimerSerial &+= 1
        lookTask?.cancel()
        lookTask = nil
        lookReservation = nil
        lookTaskSerial = nil
        nextLookSerial &+= 1
        resumeHeldQueuedIntent = nil
        lookQueued = false
        if let standby = pendingStandby { attempt.end(standby.row, as: .cancelled) }
        pendingStandby?.transport.close()
        pendingStandby = nil
        heldLook = false
        lookStep = 0
        raceGeneration += 1
        let intent = beginEndingSession()
        defer { finishEndingSession(intent) }
        await app.disconnect()
        // A later end or connect now owns the screens, retries and busy state.
        return intent == dialIntentGeneration
    }

    private func handle(_ input: Input) async {
        switch input {
        case .connection(let state, let keyed):
            await connectionChanged(state, keyed: keyed)
        case .retainedSleepConnection(let state, let keyed, let intent, let session):
            #if DEBUG
            await retainedSleepInputForTesting?()
            #endif
            let current = intent == dialIntentGeneration && app.session === session
            #if DEBUG
            retainedSleepDecisionForTesting?(current)
            #endif
            guard current else { return }
            await connectionChanged(state, keyed: keyed)
        case .capabilities:
            refreshRadio()
            refreshRenameSupport()
        case .network(let path):
            await networkChanged(path)
        case .found(let update):
            foundChanged(update)
        case .coreName:
            keepConnectedCoresName()
        case .coreAddresses:
            keepCoreAddresses()
        case .keyedLinkLost:
            if var lost = linkLost, !lost.keyed, !lost.stopped {
                lost.keyed = true
                linkLost = lost
            }
        }
    }

    private func connectionChanged(_ state: ConnectionState, keyed: Bool) async {
        guard phase != .idle, phase != .stopped else {
            return
        }
        switch state {
        case .notConnected:
            break
        case .connecting, .signingIn, .loading:
            if let lost = linkLost, !lost.stopped {
                linkLost = LinkLost(attempt: lost.attempt, retryAt: nil, stopped: false, words: lost.words,
                                    keyed: lost.keyed)
            }
        case .connected:
            connected()
        case .waitingToRetry(let seconds, let reason):
            await waitingToRetry(seconds: seconds, reason: reason, keyed: keyed)
        case .refused(let refusal):
            await ended(refusal)
        }
    }

    private func connected() {
        preferredTakeoverID = nil
        lastSessionEnd = nil
        let wasLost = linkLost != nil || offlineShown
        phase = .connected
        reachedReady = true
        redialledFirstConnection = false
        connectingTo = nil
        connectingNote = nil
        signInLimitRetried = false
        lostAttempts = 0
        linkLost = nil
        offline = false
        offlineShown = false
        if wasLost {
            showBackOnAir()
        }
        if screen != .microphone {
            screen = .band
        }
        attempt.end(dialIndex, as: .connected)
        rememberReached()
        rememberControlChannel()
        rememberProvenWinner()
        keepCoreAddresses()
        proveFoundAddresses()
        refreshOlderCore()
        refreshRadio()
        refreshRenameSupport()
        keepConnectedCoresName()
        if let session = app.session {
            resumeHeldLook()
            scheduleBetterLook(on: session)
        }
    }

    /// Snapshot completion is the racer's final ordered marker. Its standby
    /// stays unauthenticated until the session is fully ready for a move.
    private func initialSnapshotCompleted(on session: StationSession) {
        guard let racer, app.session === session else { return }
        let generation = raceGeneration
        let intent = dialIntentGeneration
        let rank = app.currentPathRank
        Task { [weak self] in
            let standby = await racer.finishInitialRace(currentRank: rank)
            guard let self, self.raceGeneration == generation,
                  self.dialIntentGeneration == intent,
                  self.app.session === session else {
                standby?.transport.close()
                return
            }
            if self.racer === racer { self.racer = nil }
            // The record was copied when the first path won. The paths
            // still opening then have ended by now; say how.
            if let settled = racer.settledTries {
                self.attempt.settle(from: settled, keeping: self.dialIndex)
            }
            if self.app.supportsControlMove(on: session) {
                self.pendingStandby = standby
            } else {
                standby?.transport.close()
                if let standby { self.attempt.end(standby.row, as: .cancelled) }
            }
            self.resumeHeldLook()
        }
    }

    private func scheduleBetterLook(on session: StationSession) {
        guard phase == .connected, app.session === session,
              app.supportsControlMove(on: session),
              app.currentPathRank != .localWebSocket,
              lookTimer == nil, lookTask == nil, lookReservation == nil,
              !lookQueued, !heldLook else { return }
        let delay = Self.lookDelays[min(lookStep, Self.lookDelays.count - 1)]
        let intent = dialIntentGeneration
        lookTimerSerial &+= 1
        let timerSerial = lookTimerSerial
        lookTimer = dependencies.clock.schedule(after: delay) { [weak self] in
            await self?.betterLookDue(on: session, intent: intent, timerSerial: timerSerial)
        }
    }

    private func queueResumeHeldLook() {
        let intent = dialIntentGeneration
        guard resumeHeldQueuedIntent != intent else { return }
        resumeHeldQueuedIntent = intent
        Task { [weak self] in
            guard let self else { return }
            guard self.resumeHeldQueuedIntent == intent else { return }
            self.resumeHeldQueuedIntent = nil
            guard intent == self.dialIntentGeneration else { return }
            self.resumeHeldLook()
        }
    }

    private func resumeHeldLook() {
        guard phase == .connected, let session = app.session,
              app.safeToMoveControl(session), lookTask == nil, lookReservation == nil else { return }
        if let standby = pendingStandby {
            pendingStandby = nil
            startUpgrade(standby, on: session, intent: dialIntentGeneration, scheduled: false,
                         standbyRow: standby.row)
        } else if heldLook, !lookQueued {
            lookQueued = true
            let intent = dialIntentGeneration
            Task { [weak self] in
                guard let self else { return }
                guard self.dialIntentGeneration == intent, self.app.session === session else { return }
                self.lookQueued = false
                await self.betterLookDue(on: session, intent: intent)
            }
        }
    }

    private func betterLookDue(on session: StationSession, intent: Int,
                               timerSerial: UInt64? = nil) async {
        if let timerSerial {
            guard timerSerial == lookTimerSerial else { return }
            lookTimer = nil
        }
        guard intent == dialIntentGeneration, app.session === session,
              phase == .connected else { return }
        guard lookTask == nil, lookReservation == nil else {
            heldLook = true
            return
        }
        nextLookSerial &+= 1
        let lookSerial = nextLookSerial
        lookReservation = lookSerial
        #if DEBUG
        betterLookReservationsForTesting += 1
        #endif
        heldLook = false
        await app.refreshCurrentPath(on: session)
        guard lookReservation == lookSerial, intent == dialIntentGeneration,
              app.session === session, phase == .connected else {
            if lookReservation == lookSerial { lookReservation = nil }
            return
        }
        guard app.safeToMoveControl(session) else {
            lookReservation = nil
            heldLook = true
            return
        }
        let generation = raceGeneration
        lookReservation = nil
        lookTaskSerial = lookSerial
        lookTask = Task { [weak self] in
            guard let self else { return }
            let result = await self.findBetterRoute(on: session, intent: intent, generation: generation)
            guard !Task.isCancelled, self.dialIntentGeneration == intent,
                  self.raceGeneration == generation, self.app.session === session,
                  self.lookTaskSerial == lookSerial else {
                if case .candidate(let candidate) = result { candidate.transport.close() }
                return
            }
            self.lookTask = nil
            self.lookTaskSerial = nil
            switch result {
            case .candidate(let candidate):
                self.startUpgrade(candidate, on: session, intent: intent, scheduled: true)
            case .deferred:
                self.heldLook = true
            case .exhausted:
                self.lookStep += 1
                if self.heldLook { self.resumeHeldLook() }
                else { self.scheduleBetterLook(on: session) }
            }
        }
    }

    /// Every look creates new route objects. A service look is offered only
    /// by a route that proves TURN and the web relay are disabled.
    private func findBetterRoute(on session: StationSession, intent: Int,
                                 generation: Int) async -> BetterLookResult {
        guard let station = current, let identity,
              let rank = app.currentPathRank, rank > .localWebSocket else { return .exhausted }
        guard app.safeToMoveControl(session) else { return .deferred }
        let found = cores.first { $0.id == station.identityKey }?.found?.dialable ?? []
        let endpoints = Self.reachPlan(station, first: nil, found: found, throughService: false)
            .compactMap { route -> StationEndpoint? in
                guard case .address(let endpoint) = route else { return nil }
                let local = ConnectionAttempt.path(for: endpoint, networks: dependencies.networks()) == .thisNetwork
                return (local ? PathRacer.Rank.localWebSocket : .otherWebSocket) < rank ? endpoint : nil
            }
        let service: (any CoreServiceRoute)?
        if rank > .directIce, servesFromAnywhere(station), let make = serviceRouteMaker {
            service = make(station, identity).directOnlyRoute()
        } else {
            service = nil
        }
        guard !endpoints.isEmpty || service != nil else { return .exhausted }
        let selector = PathRacer()
        lookRacer = selector
        let result = await withTaskCancellationHandler {
            await selector.run(direct: endpoints, service: service, trust: station.trust,
                               transportFactory: observedFactory, clock: dependencies.clock,
                               serviceAddress: dependencies.rendezvousServers.first?.host ?? "",
                               resolveNames: dependencies.directResolver, betterThan: rank,
                               networks: dependencies.networks)
        } onCancel: {
            selector.cancel()
        }
        guard !Task.isCancelled, intent == dialIntentGeneration,
              generation == raceGeneration, app.session === session,
              lookRacer === selector, let winner = result.winner else {
            selector.cancel()
            if lookRacer === selector { lookRacer = nil }
            return app.safeToMoveControl(session) ? .exhausted : .deferred
        }
        guard selector.transferInitialWinner() else {
            selector.cancel()
            lookRacer = nil
            return app.safeToMoveControl(session) ? .exhausted : .deferred
        }
        lookRacer = nil
        return .candidate(winner)
    }

    /// `standbyRow` is the attempt record's row for the initial race's held
    /// standby; that row, not a new one, then says how the move ended.
    private func startUpgrade(_ winner: PathRacer.Winner, on session: StationSession,
                              intent: Int, scheduled: Bool, standbyRow: Int? = nil) {
        guard lookTask == nil, lookReservation == nil else {
            winner.transport.close()
            if let standbyRow { attempt.end(standbyRow, as: .cancelled) }
            return
        }
        nextLookSerial &+= 1
        let lookSerial = nextLookSerial
        lookTaskSerial = lookSerial
        lookTask = Task { [weak self] in
            guard let self else { winner.transport.close(); return }
            let outcome = await self.app.moveControl(to: winner, on: session)
            guard self.dialIntentGeneration == intent, self.app.session === session,
                  self.lookTaskSerial == lookSerial, !Task.isCancelled else { return }
            self.lookTask = nil
            self.lookTaskSerial = nil
            if let standbyRow, self.attempt.tries.indices.contains(standbyRow) {
                switch outcome {
                case .moved:
                    self.attempt.end(standbyRow, as: .connected)
                    if case .service(let through) = winner.route { self.service = through }
                    self.dialIndex = standbyRow
                    self.winnerPath = winner.path
                    self.rememberReached()
                case .failed:
                    self.attempt.end(standbyRow, as: .failed)
                case .deferredSafety, .deferredTicket:
                    // The held connection was closed; a later look races again.
                    self.attempt.end(standbyRow, as: .cancelled)
                }
            } else if case .moved = outcome {
                let route: Route
                let address: String
                switch winner.route {
                case .address(let endpoint):
                    route = .address(endpoint)
                    address = Self.addressText(endpoint)
                case .service(let through):
                    route = .throughService
                    address = self.dependencies.rendezvousServers.first?.host ?? ""
                    self.service = through
                }
                self.dialPlan.append(route)
                self.attempt.tries.append(.init(path: winner.path, address: address, outcome: .connected))
                assert(self.dialPlan.count == self.attempt.tries.count)
                self.dialIndex = self.dialPlan.count - 1
                self.winnerPath = winner.path
                self.rememberReached()
            }
            switch outcome {
            case .deferredSafety:
                self.heldLook = true
            case .deferredTicket:
                if self.heldLook { self.resumeHeldLook() }
                else { self.scheduleBetterLook(on: session) }
            case .moved, .failed:
                if scheduled { self.lookStep += 1 }
                if self.heldLook { self.resumeHeldLook() }
                else { self.scheduleBetterLook(on: session) }
            }
        }
    }

    private func waitingToRetry(seconds: Int, reason: Refusal?, keyed: Bool) async {
        if case .authentication(let words)? = reason?.reason {
            await signInLimited(words)
            return
        }
        guard reachedReady else {
            // The first connection to this Core: the phone says what it
            // tried rather than retrying out of sight.
            if !online {
                connectingNote = Self.offlineWaitText
                return
            }
            if redialledFirstConnection {
                // Perhaps the offline try's failure, read after the network
                // came back; the redial's own outcome follows.
                redialledFirstConnection = false
                return
            }
            if case .ended(let words, true)? = reason?.reason {
                // The Core answered and asked the phone to try again shortly.
                connectingNote = words
                return
            }
            let denied = failures.localNetworkDenied
            if racer == nil, await tryNextAddress(after: failedOutcome()) {
                return
            }
            if racer != nil { attempt.end(dialIndex, as: failedOutcome()) }
            await showNotAnswering(localNetworkDenied: denied)
            return
        }
        // Transmitting as the link went from up: the Core stops on its own.
        lookTimer?.cancel()
        lookTimer = nil
        lookTimerSerial &+= 1
        lookTask?.cancel()
        lookTask = nil
        lookReservation = nil
        lookTaskSerial = nil
        nextLookSerial &+= 1
        resumeHeldQueuedIntent = nil
        lookQueued = false
        lookRacer?.cancel()
        lookRacer = nil
        if let standby = pendingStandby { attempt.end(standby.row, as: .cancelled) }
        pendingStandby?.transport.close()
        pendingStandby = nil
        heldLook = false
        lookStep = 0
        let wasKeyed = phase == .connected ? keyed || app.main.transmit.ptt.state == .linkLost
            : (linkLost?.keyed ?? false)
        phase = .lost
        lostAttempts += 1
        // An end the Core gave a code and asked the phone to come back from
        // (its radio changing) is shown in its words until the phone is back.
        let words = reason.flatMap { $0.code == nil ? nil : Self.words(of: $0) } ?? linkLost?.words
        linkLost = LinkLost(attempt: lostAttempts, retryAt: dependencies.now().addingTimeInterval(TimeInterval(seconds)),
                            stopped: false, words: words, keyed: wasKeyed)
        backOnAir = false
        radioOff = nil
        refreshOffline()
    }

    /// The Core limits sign-ins from this phone for a while: wait, then try
    /// once more; a second refusal stops with the Core's words.
    private func signInLimited(_ words: String) async {
        let station = current
        guard await endSession() else { return }
        guard !signInLimitRetried, let station else {
            phase = .idle
            connectingTo = nil
            connectingNote = nil
            linkLost = nil
            notice = .words(words)
            show(.cores)
            return
        }
        signInLimitRetried = true
        phase = .connecting
        connectingTo = station.identityKey
        connectingNote = words
        signInLimitTimer?.cancel()
        signInLimitTimer = dependencies.clock.schedule(after: Self.signInLimitWait) { [weak self] in
            await self?.retryAfterSignInLimit(station)
        }
    }

    private func retryAfterSignInLimit(_ station: PairedStation) async {
        signInLimitTimer = nil
        guard phase == .connecting, connectingTo == station.identityKey else {
            return
        }
        await dial(station, fresh: false)
    }

    /// Ends the way being tried with `outcome` and, on a first connection
    /// with another way left, dials that one. False when there is none, or
    /// the Core was already reached this time.
    private func tryNextAddress(after outcome: ConnectionAttempt.Outcome) async -> Bool {
        attempt.end(outcome)
        attemptDenied = attemptDenied || failures.localNetworkDenied
        guard !reachedReady, dialIndex + 1 < dialPlan.count, let station = current else {
            return false
        }
        dialIndex += 1
        if dialling {
            // The dial of this address has not returned yet; it dials the
            // next one when it does.
            advancePending = true
            return true
        }
        // A newer intent also consumes this failure; it must not become
        // a no-answer sheet for the connection that replaced this one.
        guard await endSession() else { return true }
        await dialRoute(station, fresh: false)
        return true
    }

    /// None of the ways reached the Core on a first connection: the sheet
    /// lists what was tried (this Wi-Fi, direct, relay) and how each went.
    private func showNotAnswering(localNetworkDenied: Bool) async {
        let station = current
        guard await endSession() else { return }
        phase = .idle
        connectingTo = nil
        connectingNote = nil
        guard let station else {
            return
        }
        let denied = localNetworkDenied || attemptDenied || failures.localNetworkDenied || lookingDenied
        let tries = zip(dialPlan, attempt.tries).map { (route: $0, tried: $1) }
        let olderCore = dependencies.serviceRoute != nil && !servesFromAnywhere(station)
        let rows = Self.triedRows(tries, serviceError: service?.lastError, olderCore: olderCore,
                                  localNetworkDenied: denied)
        trouble = .notAnswering(core: station.label.isEmpty ? Self.addressText(station.endpoints.first) : station.label,
                                tried: rows.rows, localNetworkDenied: denied, note: rows.note)
    }

    /// The Core-not-answering sheet's rows for what a connect tried, each
    /// try with the way it took, in the board's order: this Wi-Fi (not
    /// here when no address on it was tried, not allowed to look when iOS
    /// refused the local network), direct over the internet when an address
    /// elsewhere was tried, and by relay when the remote access service was
    /// asked, or when the Core is too old to be reached through it. The
    /// note is the service unreachable, or the Core needing an update.
    nonisolated static func triedRows(_ tries: [(route: Route, tried: ConnectionAttempt.Try)],
                                      serviceError: RendezvousDialError?, olderCore: Bool,
                                      localNetworkDenied: Bool) -> (rows: [TriedRow], note: String?) {
        var rows: [TriedRow] = []
        let local = tries.last { $0.route != .throughService && $0.tried.path == .thisNetwork }
        let wifi = localNetworkDenied ? notAllowedText : local.map { resultText($0.tried.outcome) } ?? notHereText
        rows.append(TriedRow(place: thisWiFiText, result: wifi))
        if let direct = tries.last(where: { $0.route != .throughService && $0.tried.path != .thisNetwork }) {
            rows.append(TriedRow(place: directPlaceText, result: resultText(direct.tried.outcome)))
        }
        var note: String?
        if let service = tries.last(where: { $0.route == .throughService }) {
            let result: String
            switch serviceError {
            case .service(.offline)?:
                result = notCheckedInText
            case .service(.unreachable)?:
                result = serviceNotReachedText
                note = RendezvousDialError.unreachableServiceText
            default:
                result = resultText(service.tried.outcome)
            }
            rows.append(TriedRow(place: relayPlaceText, result: result))
        } else if olderCore {
            rows.append(TriedRow(place: relayPlaceText, result: needsNewerCoreText))
            note = PairedStation.updateToReachFromAnywhereText
        }
        return (rows, note)
    }

    /// How one way went, in the sheet's short words.
    nonisolated static func resultText(_ outcome: ConnectionAttempt.Outcome) -> String {
        switch outcome {
        case .trying:
            return "Still trying"
        case .connected:
            return "Connected"
        case .noAnswer:
            return "No reply"
        case .timedOut:
            return "No reply in time"
        case .notThisCore:
            return "Another Core answered"
        case .failed:
            return "Didn\u{2019}t connect"
        case .cancelled:
            return "Stopped after another path connected"
        case .refused:
            return "Refused"
        case .unreachable:
            return "No route from this network"
        }
    }

    /// The sheet for a Core that didn't answer at the one address tried
    /// (a one tap, a pairing, a rename's sign-in).
    private func notAnswering(core: String, at endpoint: StationEndpoint,
                              localNetworkDenied: Bool? = nil) -> Trouble {
        let denied = localNetworkDenied ?? failures.localNetworkDenied
        let tried = ConnectionAttempt.Try(path: ConnectionAttempt.path(for: endpoint, networks: dependencies.networks()),
                                          address: Self.addressText(endpoint), outcome: .noAnswer)
        let rows = Self.triedRows([(route: .address(endpoint), tried: tried)], serviceError: nil, olderCore: false,
                                  localNetworkDenied: denied)
        return .notAnswering(core: core, tried: rows.rows, localNetworkDenied: denied, note: rows.note)
    }

    /// The session stopped for a reason it won't retry.
    private func ended(_ refusal: Refusal) async {
        // Another Core at one of this Core's addresses, on the way in: that
        // address is passed, never re-pointed, and the next is tried.
        if !reachedReady, racer == nil, Self.code(of: refusal) == .identityChanged {
            if await tryNextAddress(after: .notThisCore) {
                return
            }
            // Every address was tried. Only when each one reached another
            // Core does this Core's identity look changed; when some did not
            // answer, the Core is simply not reachable now, whatever the
            // order the addresses were tried in.
            if !attempt.tries.allSatisfy({ $0.outcome == .notThisCore }) {
                await showNotAnswering(localNetworkDenied: false)
                return
            }
        } else {
            if racer != nil { attempt.end(dialIndex, as: .failed) }
            else { attempt.end(.failed) }
        }
        let station = current
        phase = .idle
        connectingTo = nil
        connectingNote = nil
        linkLost = nil
        radioOff = nil
        offline = false
        offlineShown = false
        backOnAir = false
        olderCore = nil
        heldQuestion = nil
        heldChoiceID = nil
        heldSession = nil
        heldBusy = false
        showEnd(refusal, station: station)
        show(.cores)
    }

    /// Shows why the Core ended a session or refused a sign-in, over Your
    /// Cores: a Core that removed or forgot this phone is marked to pair
    /// again (D70), a version gap rises as a sheet, the rest are the
    /// notice's words.
    private func showEnd(_ refusal: Refusal, station: PairedStation?) {
        let words = Self.words(of: refusal)
        let label = station.map { $0.label.isEmpty ? Self.addressText($0.endpoints.first) : $0.label } ?? ""
        switch Self.code(of: refusal) {
        case .deviceRemoved?, .deviceNotPaired?, .pairingRequired?:
            // A Core that removed or forgot this phone stays listed with Pair (D70).
            markNeedsPairing(station)
            removedNoticeCoreID = station?.identityKey
            notice = .removed(core: label)
        case .identityChanged?:
            markNeedsPairing(station)
            notice = .words(words)
        case .linkVersion?:
            trouble = versionTrouble(refusal, core: label)
        case .takenOver?:
            // Never redialled: the operator takes the Core back by hand.
            if let end = lastSessionEnd, end.code == "takenOver", let byName = end.takenOverBy,
               let byId = end.takenOverById, let seconds = end.secondsAgo {
                notice = .placeTaken(byName: byName, byId: byId,
                                     happened: dependencies.now().addingTimeInterval(-Double(seconds)), reason: words)
            } else {
                notice = .words(words)
            }
        default:
            switch refusal.reason {
            case .stationTooOld, .appTooOld:
                trouble = versionTrouble(refusal, core: label)
            case .authentication, .ended:
                notice = .words(words)
            }
        }
    }

    /// The phone's network changed. Offline, the session spends no tries
    /// (none could succeed) and the band says NO NETWORK; back online, or
    /// over another interface (Wi-Fi to cellular), it dials at once with the
    /// schedule started over, since the new network may reach the Core; a
    /// connected session checks its link and starts its media again.
    private func networkChanged(_ path: NetworkPath) async {
        let wasOnline = online
        let otherInterfaces = Set(path.interfaces) != Set(interfaces)
        if !path.online || !wasOnline || otherInterfaces {
            networkSignature = nil
        }
        var topologyChanged = false
        if let incoming = path.signature {
            if let previous = networkSignature {
                topologyChanged = incoming.gateways != previous.gateways ||
                    incoming.supportsIPv4 != previous.supportsIPv4 ||
                    incoming.supportsIPv6 != previous.supportsIPv6 ||
                    (incoming.addresses != nil && previous.addresses != nil &&
                     incoming.addresses != previous.addresses)
            }
            var observed = incoming
            if observed.addresses == nil { observed.addresses = networkSignature?.addresses }
            networkSignature = observed
        }
        let actualChange = wasOnline != path.online || otherInterfaces || topologyChanged
        let hadObservation = networkObserved
        networkObserved = true
        online = path.online
        interfaces = Array(Set(path.interfaces)).sorted()
        if hadObservation && actualChange {
            for row in cores where row.station.controlChannelVersion == 0 {
                serviceDiscoveryUnknown.insert(row.id)
            }
        }
        let sessionKept = phase != .idle && phase != .stopped
        guard online else {
            if phase == .connecting, racer != nil {
                // A first race has no session retry scheduler yet. Retire
                // its pre-auth sockets and wait for the next real path.
                raceGeneration += 1
                racer?.cancel()
                restartOnRetire = nil
            }
            if sessionKept {
                await app.holdRetries()
            }
            if phase == .connecting, !reachedReady {
                connectingNote = Self.offlineWaitText
            }
            refreshOffline()
            return
        }
        if phase == .connecting, connectingNote == Self.offlineWaitText {
            connectingNote = nil
        }
        refreshOffline()
        guard sessionKept, actualChange else {
            return
        }
        guard !endingSession else { return }
        if phase == .connecting {
            if !reachedReady { redialledFirstConnection = true }
            if let current {
                raceGeneration += 1
                racer?.cancel()
                if dialling {
                    restartOnRetire = (current, dialIntentGeneration)
                    return
                }
                racer = nil
                advanceDialIntent()
                let intent = dialIntentGeneration
                Task { [weak self] in
                    guard let self, self.dialIntentGeneration == intent,
                          self.phase == .connecting, self.online else { return }
                    await self.dial(current, fresh: false)
                }
                return
            }
        }
        redialNow()
        if phase == .connected, let session = app.session {
            lookTimer?.cancel()
            lookTimer = nil
            lookTimerSerial &+= 1
            let intent = dialIntentGeneration
            Task { [weak self] in
                await self?.betterLookDue(on: session, intent: intent)
            }
        }
    }

    /// Dials at once and lifts a hold; a connected session checks its link
    /// at once and starts its media again, and one signing in is left as it
    /// is. It does not wait for the dial, so the next change to the network
    /// is read while a try is still opening.
    private func redialNow() {
        #if DEBUG
        redialsForTesting += 1
        #endif
        redialTask = Task { [weak self] in
            guard let self else {
                return
            }
            await self.app.retryNow()
            if !self.online {
                // Offline again while that try went out.
                await self.app.holdRetries()
            }
        }
    }

    /// NO NETWORK covers the band while the phone is offline and the band is
    /// being kept: listening, reconnecting, or a Reconnect still dialling,
    /// whichever came first, the network or the link. A link that drops
    /// while the phone is online stays LINK LOST.
    private func refreshOffline() {
        let keptBand = reachedReady && (phase == .connected || phase == .lost || phase == .connecting)
        offline = !online && keptBand
        if offline {
            offlineShown = true
        }
    }

    private func showBackOnAir() {
        backOnAir = true
        backOnAirTimer?.cancel()
        backOnAirTimer = dependencies.clock.schedule(after: Self.backOnAirShown) { [weak self] in
            await self?.hideBackOnAir()
        }
    }

    private func hideBackOnAir() {
        backOnAir = false
        backOnAirTimer = nil
    }

    // MARK: What the Core says

    private func refreshRadio() {
        guard phase == .connected else {
            if phase == .idle {
                radioOff = nil
            }
            return
        }
        let capabilities = app.mirror.capabilities
        guard case .bool(let connected)? = capabilities["radioConnected"] else {
            radioOff = nil
            return
        }
        if connected {
            radioSeenConnected = true
            radioOff = nil
            return
        }
        let waiting = app.main.radioWaiting
        if var shown = radioOff {
            if shown.waiting != waiting {
                shown.waiting = waiting
                radioOff = shown
            }
            return
        }
        var radio: String?
        if case .text(let model)? = capabilities["radioModel"], !model.isEmpty {
            radio = model
        } else {
            radio = app.main.radioName
        }
        var address: String?
        if case .text(let text)? = capabilities["radioAddress"], !text.isEmpty {
            address = text
        }
        let now = dependencies.now()
        radioOff = RadioOff(radio: radio, address: address, since: radioSeenConnected ? now : nil, shownAt: now,
                            waiting: waiting)
    }

    private func refreshOlderCore() {
        guard let hello = app.stationHello, let newest = dependencies.appMajors.max(),
              let agreed = LinkVersionPolicy.agree(ours: dependencies.appMajors, theirs: hello.supportedMajors),
              agreed < newest, let station = current else {
            olderCore = nil
            return
        }
        olderCore = station.label.isEmpty ? Self.addressText(station.endpoints.first) : station.label
    }

    private func versionTrouble(_ refusal: Refusal, core: String) -> Trouble {
        let appMajors = dependencies.appMajors
        var stationMajors = app.stationHello?.supportedMajors
        var coreIsOlder: Bool?
        var reason: String?
        switch refusal.reason {
        case .stationTooOld(let station, _):
            stationMajors = station
            coreIsOlder = true
        case .appTooOld(let station, _):
            stationMajors = station
            coreIsOlder = false
        case .ended(let words, _), .authentication(let words):
            reason = words
        }
        let stationNewest = stationMajors?.max()
        let appNewest = appMajors.max() ?? 1
        if coreIsOlder == nil {
            if let stationNewest, stationNewest != appNewest {
                coreIsOlder = stationNewest < appNewest
            } else {
                // The Core's own words say which side to update.
                coreIsOlder = !(reason?.hasSuffix("Update this app.") ?? false)
            }
        }
        var gap: Int?
        if let stationNewest, stationNewest != appNewest {
            gap = abs(Int(appNewest) - Int(stationNewest))
        }
        let minor = app.stationHello?.minor
        let coreSpeaks = stationNewest.map { newest in
            minor.map { "Remote link \(newest).\($0)" } ?? "Remote link \(newest).x"
        } ?? "An older remote link"
        return .needsUpdating(core: core, coreSpeaks: coreSpeaks, appSpeaks: Self.appSpeaks(appMajors),
                              reason: reason, coreIsOlder: coreIsOlder ?? true, gap: gap)
    }

    /// "Remote link 3.x, and 2.x for older Cores", from the majors this app speaks.
    nonisolated static func appSpeaks(_ majors: [UInt16]) -> String {
        let sorted = majors.sorted(by: >)
        guard let newest = sorted.first else {
            return ""
        }
        guard sorted.count > 1 else {
            return "Remote link \(newest).x"
        }
        let older = sorted.dropFirst().map { "\($0).x" }.joined(separator: " and ")
        return "Remote link \(newest).x, and \(older) for older Cores"
    }

    /// The words of an end, as the Core sent them or as the app words its own.
    nonisolated static func words(of refusal: Refusal) -> String {
        switch refusal.reason {
        case .authentication(let words), .ended(let words, _):
            return words
        case .stationTooOld:
            return "This Core runs an older remote link than this app can talk to. Update the Core."
        case .appTooOld:
            return "This Core runs a newer remote link than this app can talk to. Update this app."
        }
    }

    /// The end's code, or for an older Core that sends none, the code its words stand for.
    nonisolated static func code(of refusal: Refusal) -> Refusal.Code? {
        if let code = refusal.code {
            return code
        }
        let words = Self.words(of: refusal)
        switch words {
        case "This device was removed from the Core.":
            return .deviceRemoved
        case "This device is not paired with this Core. Pair it first.":
            return .deviceNotPaired
        case "This Core uses paired devices. Pair this device first.":
            return .pairingRequired
        default:
            break
        }
        if words.hasPrefix("Another app at "), words.contains("took over") {
            return .takenOver
        }
        if words.hasPrefix("This Core runs link version ") {
            return .linkVersion
        }
        return nil
    }

    // MARK: Renaming a Core (D76)

    /// Whether a row's Rename can be used now: not while this phone must
    /// pair with the Core again, nor while a connection or pairing is under
    /// way, nor on a Core this phone last found too old to take it.
    func renameAvailability(_ row: CoreRow) -> RenameAvailability {
        if row.needsPairing || keyUnreadable {
            return .needsPairing
        }
        if connectingTo != nil || claiming != nil || pairing || renameBusy {
            return .busy
        }
        if phase == .connected, current?.identityKey == row.id {
            return RenameSignIn.renames(app.mirror) ? .available : .needsNewerCore
        }
        return coresWithoutRename.contains(row.id) ? .needsNewerCore : .available
    }

    /// The connected Core's row in Your Cores, for Setup, Devices' Rename;
    /// nil while no Core is connected.
    var connectedCoreRow: CoreRow? {
        guard phase == .connected, let id = current?.identityKey else {
            return nil
        }
        return cores.first { $0.id == id }
    }

    /// The saved Core shown on Radio, including while its link is lost.
    var selectedCoreIdentity: Data? { current?.identityKey ?? dialTarget }

    var radioRemoveReason: String? {
        guard let id = selectedCoreIdentity else { return Self.removeBusyText }
        return removeDisabledReason(identityKey: id)
    }

    /// Why a row's Rename is greyed, shown under it in the menu.
    static func renameReason(_ availability: RenameAvailability) -> String? {
        switch availability {
        case .available:
            return nil
        case .needsNewerCore:
            return needsNewerCoreText
        case .needsPairing:
            return renameNeedsPairingText
        case .busy:
            return renameBusyText
        }
    }

    /// Rename, from a row's press-and-hold menu: the sheet, its field filled
    /// with the Core's name (empty when it has none).
    func startRename(_ row: CoreRow) {
        guard renameAvailability(row) == .available else {
            return
        }
        renaming = row.id
        renameText = row.station.label
        renameProblem = nil
    }

    /// Cancel, on the sheet: a sign-in still being made is ended, and
    /// nothing is renamed that the Core had not yet taken.
    func cancelRename() async {
        if let signIn = renameSignIn {
            await signIn.cancel()
        }
        closeRename()
    }

    /// Save, on the sheet: the name goes to the Core, over the connected
    /// Core's session or a sign-in made for it. Accepted, the sheet closes
    /// and the row (and the band's toolbar) show the new name; refused, the
    /// sheet stays with the Core's words under the field. A Core that isn't
    /// answering, or refuses the sign-in, shows what any connection shows.
    func saveRename() async {
        guard let id = renaming, !renameBusy, let row = cores.first(where: { $0.id == id }) else {
            return
        }
        let name = renameText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !name.isEmpty else {
            return
        }
        renameBusy = true
        renameProblem = nil
        if phase == .connected, current?.identityKey == id {
            await renameConnected(id, to: name)
        } else {
            await renameBySigningIn(row, to: name)
        }
        renameBusy = false
    }

    /// Renames the connected Core over its own session.
    private func renameConnected(_ id: Data, to name: String) async {
        guard RenameSignIn.renames(app.mirror) else {
            noteRenameSupport(id, renames: false)
            renameProblem = Self.needsNewerCoreText
            return
        }
        do {
            let result = try await app.commands.invoke(RenameSignIn.verb, arguments: [
                CommandArgument(name: "label", value: .text(name)),
            ], timeout: RenameSignIn.commandTimeout)
            guard result.accepted else {
                renameProblem = result.reason.isEmpty ? Self.renameNoAnswerText : result.reason
                return
            }
            app.main.coreRenamed(to: name)
            keepName(name, for: id)
            closeRename()
        } catch {
            renameProblem = Self.renameNoAnswerText
        }
    }

    /// Renames a Core that isn't connected: a sign-in for the rename alone,
    /// closed after it.
    private func renameBySigningIn(_ row: CoreRow, to name: String) async {
        let plan = Self.reachPlan(row.station, first: nil, found: row.found?.dialable ?? [],
                                  throughService: servesFromAnywhere(row.station))
        guard let identity else {
            closeRename()
            notice = keyUnreadable ? .keyUnreadable : .keyUnavailable
            return
        }
        let deviceName = phoneSettings.deviceName ?? dependencies.kind.shortName
        guard let authenticator = try? DeviceKeyAuthenticator(identity: identity, name: deviceName,
                                                               kind: dependencies.kind) else {
            closeRename()
            notice = .words(Self.unusableNameText)
            return
        }
        var tries: [(route: Route, tried: ConnectionAttempt.Try)] = []
        var serviceError: RendezvousDialError?
        var localNetworkDenied = false
        var identityRefusal: Refusal?
        for route in plan {
            // Cancel may close the sheet between one failed way and the next.
            guard renaming == row.id else { return }
            failures.reset()
            let signIn: RenameSignIn
            var through: ObservedServiceRoute?
            switch route {
            case .address(let endpoint):
                signIn = RenameSignIn(station: row.station, endpoint: endpoint, authenticator: authenticator,
                                      clock: dependencies.clock, transportFactory: observedFactory)
            case .throughService:
                guard let make = serviceRouteMaker else { continue }
                let service = ObservedServiceRoute(inner: make(row.station, identity), failures: failures)
                through = service
                signIn = RenameSignIn(station: row.station, authenticator: authenticator,
                                      clock: dependencies.clock, transport: { service.makeTransport() })
            }
            renameSignIn = signIn
            let outcome = await signIn.run(label: name)
            renameSignIn = nil
            if let renames = signIn.coreRenames {
                noteRenameSupport(row.id, renames: renames)
            }
            switch outcome {
            case .answered(let result, let reported):
                guard result.accepted else {
                    renameProblem = result.reason.isEmpty ? Self.renameNoAnswerText : result.reason
                    return
                }
                keepName(reported.flatMap { $0.isEmpty ? nil : $0 } ?? name, for: row.id)
                closeRename()
                return
            case .needsNewerCore:
                renameProblem = Self.needsNewerCoreText
                return
            case .notAnswering:
                localNetworkDenied = localNetworkDenied || failures.localNetworkDenied
                if case .address(let endpoint) = route {
                    let tried = ConnectionAttempt.Try(
                        path: ConnectionAttempt.path(for: endpoint, networks: dependencies.networks()),
                        address: Self.addressText(endpoint),
                        outcome: failures.openFailed ? .noAnswer : .timedOut)
                    tries.append((route: route, tried: tried))
                } else {
                    serviceError = through?.lastError
                    var tried = through?.lastTry ?? ConnectionAttempt.Try(path: .direct, address: "", outcome: .noAnswer,
                                                                          throughService: true)
                    if tried.outcome == .trying || tried.outcome == .connected {
                        tried.outcome = failures.openFailed ? .noAnswer : .timedOut
                    }
                    tries.append((route: route, tried: tried))
                }
            case .refused(let refusal):
                // A different Core at a saved address is another failed way,
                // as it is for Connect. Other refusals end this sign-in.
                if Self.code(of: refusal) == .identityChanged, case .address(let endpoint) = route {
                    let tried = ConnectionAttempt.Try(
                        path: ConnectionAttempt.path(for: endpoint, networks: dependencies.networks()),
                        address: Self.addressText(endpoint), outcome: .notThisCore)
                    tries.append((route: route, tried: tried))
                    identityRefusal = refusal
                    continue
                }
                closeRename()
                showEnd(refusal, station: row.station)
                return
            case .waitAndRetry(let words):
                renameProblem = words
                return
            case .noAnswer:
                renameProblem = Self.renameNoAnswerText
                return
            case .cancelled:
                return
            }
        }
        closeRename()
        if let identityRefusal, !tries.isEmpty, tries.allSatisfy({ $0.tried.outcome == .notThisCore }) {
            showEnd(identityRefusal, station: row.station)
            return
        }
        retryRename = (row.id, name)
        let olderCore = dependencies.serviceRoute != nil && !servesFromAnywhere(row.station)
        let rows = Self.triedRows(tries, serviceError: serviceError, olderCore: olderCore,
                                  localNetworkDenied: localNetworkDenied)
        trouble = .notAnswering(core: row.label, tried: rows.rows, localNetworkDenied: localNetworkDenied,
                                note: rows.note)
    }

    private func closeRename() {
        renaming = nil
        renameText = ""
        renameProblem = nil
    }

    /// Keeps a Core's name as the Core reports it, on its row and in the stored Core.
    private func keepName(_ name: String, for id: Data) {
        guard var station = try? dependencies.stations.station(identityKey: id), station.label != name else {
            return
        }
        station.label = name
        do {
            try dependencies.stations.save(station)
        } catch {
            Self.logger.warning("The Core's new name could not be kept in the Keychain")
        }
        if current?.identityKey == id {
            current = station
        }
        reloadCores()
    }

    /// The connected Core's name, as its devices object reports it, becomes
    /// the stored Core's, so its row shows it too, and keeps it after a
    /// rename from another device. A Core with no name (or none sent) leaves
    /// the stored name as it is.
    private func keepConnectedCoresName() {
        guard phase == .connected, app.mirror.isSnapshotComplete, !app.main.renamePending, let station = current,
              case .text(let name)? = app.mirror.object(MainScreenModel.devicesKey)?["stationLabel"],
              !name.isEmpty, name != station.label else {
            return
        }
        keepName(name, for: station.identityKey)
    }

    /// Records whether the connected Core takes a rename, for its row's menu.
    private func refreshRenameSupport() {
        guard phase == .connected, app.mirror.isSnapshotComplete, let station = current else {
            return
        }
        noteRenameSupport(station.identityKey, renames: RenameSignIn.renames(app.mirror))
    }

    private func noteRenameSupport(_ id: Data, renames: Bool) {
        guard renames == coresWithoutRename.contains(id) else {
            return
        }
        if renames {
            coresWithoutRename.remove(id)
        } else {
            coresWithoutRename.insert(id)
        }
        phoneSettings.coresWithoutRename = Set(coresWithoutRename.map(Base64URL.encode))
    }

    // MARK: Keeping what the phone knows

    private func loadIdentity() {
        do {
            identity = try DeviceIdentity.load(store: dependencies.keyStore)
            keyUnreadable = false
        } catch DeviceKeyError.unreadableKey {
            // Never replaced here: only Make a new key does that (D72).
            identity = nil
            keyUnreadable = true
            notice = .keyUnreadable
        } catch {
            identity = nil
            notice = .keyUnavailable
        }
    }

    private func reloadCores() {
        let stations: [PairedStation]
        do {
            stations = try dependencies.stations.all()
        } catch {
            Self.logger.warning("The paired Cores could not be read from the Keychain")
            stations = []
        }
        cores = stations.map { station in
            CoreRow(station: station, needsPairing: baseNeedsPairing(station))
        }
        relist()
    }

    /// This phone must pair again: its key can't be read, or the Core removed
    /// or forgot it, or a new key was made (D70, D72).
    private func baseNeedsPairing(_ station: PairedStation) -> Bool {
        keyUnreadable || phoneSettings.coresNeedingPairing.contains(Base64URL.encode(station.identityKey))
    }

    private func markNeedsPairing(_ station: PairedStation?) {
        guard let station else {
            return
        }
        var needing = phoneSettings.coresNeedingPairing
        needing.insert(Base64URL.encode(station.identityKey))
        phoneSettings.coresNeedingPairing = needing
        reloadCores()
    }

    /// The Core was reached by the plan's current way. At an address, that
    /// address goes first for next time, with the path it took
    /// (``PairedStation/reached(_:by:)``); through the remote access
    /// service, the path (direct or relay) is kept and no address moves
    /// (``PairedStation/reachedThroughService(by:)``).
    private func rememberReached() {
        guard let station = current, dialPlan.indices.contains(dialIndex) else {
            return
        }
        switch dialPlan[dialIndex] {
        case .address(let endpoint):
            let path = ConnectionAttempt.path(for: endpoint, networks: dependencies.networks())
            guard station.endpoints.first?.canonical != endpoint.canonical || station.lastPath != path.rawValue
                    || !station.isLastGood(endpoint) else {
                return
            }
            do {
                if let updated = try dependencies.stations.recordReached(identityKey: station.identityKey, at: endpoint,
                                                                         by: path) {
                    current = updated
                }
            } catch {
                Self.logger.warning("The Core's address that worked could not be kept in the Keychain")
            }
        case .throughService:
            let path = winnerPath ?? service?.lastTry?.path ?? .direct
            if attempt.tries.indices.contains(dialIndex) {
                attempt.tries[dialIndex].path = path
            }
            guard station.lastPath != path.rawValue || station.lastGood != nil else {
                return
            }
            do {
                if let updated = try dependencies.stations.recordReachedThroughService(identityKey: station.identityKey,
                                                                                        by: path) {
                    current = updated
                }
            } catch {
                Self.logger.warning("How the Core was reached could not be kept in the Keychain")
            }
        }
        reloadCores()
    }

    /// The Core's `controlChannelVersion` at this sign-in, kept with it: a
    /// Core that declared none (0) cannot be reached through the remote
    /// access service, and says so where that is offered
    /// (``PairedStation/reachableFromAnywhere``, link document section 6.3).
    private func rememberControlChannel() {
        guard let station = current else {
            return
        }
        let version = Int(clamping: app.mirror.capabilityVersion(Self.controlChannelCapability))
        let relay: Bool?
        if case .bool(let allowed)? = app.mirror.capabilities[Self.relayAllowedCapability] {
            relay = allowed
        } else {
            relay = nil
        }
        guard version == 0 || station.controlChannelVersion != version || station.relayAllowed != relay else {
            return
        }
        do {
            if let updated = try dependencies.stations.recordRouteCapabilities(identityKey: station.identityKey,
                                                                                controlChannelVersion: version,
                                                                                relayAllowed: relay,
                                                                                observedAt: dependencies.now()) {
                current = updated
                serviceDiscoveryUnknown.remove(station.identityKey)
            }
        } catch {
            Self.logger.warning("Whether the Core can be reached from anywhere could not be kept in the Keychain")
        }
        reloadCores()
    }

    /// The address this session reached the Core at, when it is a global
    /// one: its identity was checked there, so it is kept to dial from
    /// anywhere (``PairedStation/addProvenAddresses(_:)``, R-IOS-16).
    private func rememberProvenWinner() {
        guard let station = current, dialPlan.indices.contains(dialIndex),
              case .address(let endpoint) = dialPlan[dialIndex], LocalNetworks.isGlobal(endpoint.host),
              station.directAddresses.first?.canonical != endpoint.canonical else {
            return
        }
        keepProven([endpoint], for: station.identityKey)
    }

    /// The global addresses the connected Core was found at on this network
    /// that it has not answered at yet: each is asked which Core answers
    /// there, reading its hello and sending nothing (``CoreIdentityProbe``),
    /// and each where this Core proves its identity is kept to dial from
    /// anywhere, not only the one the race picked (R-IOS-16). Each address
    /// is asked once per connect.
    private func proveFoundAddresses() {
        guard phase == .connected, let station = current else {
            return
        }
        let found = dialFound + (cores.first(where: { $0.id == station.identityKey })?.found?.dialable ?? [])
        var winner: StationEndpoint?
        if dialPlan.indices.contains(dialIndex), case .address(let endpoint) = dialPlan[dialIndex] {
            winner = endpoint.canonical
        }
        prove(found.filter { $0.canonical != winner }, of: station)
    }

    /// Asks each global address in `candidates` that `station` does not
    /// keep, and this connect has not asked, which Core answers there
    /// (``CoreIdentityProbe``: its hello read, nothing sent), at most
    /// ``CoreAddressList/maximumCount``; each where this Core proves its
    /// identity is kept to dial from anywhere (R-IOS-16).
    private func prove(_ candidates: [StationEndpoint], of station: PairedStation) {
        let kept = Set(station.directAddresses.map(\.canonical))
        let asking = candidates.map(\.canonical).filter { endpoint in
            LocalNetworks.isGlobal(endpoint.host) && !kept.contains(endpoint) && !proved.contains(endpoint)
        }.reduce(into: [StationEndpoint]()) { unique, endpoint in
            if !unique.contains(endpoint) { unique.append(endpoint) }
        }.prefix(CoreAddressList.maximumCount)
        guard !asking.isEmpty else {
            return
        }
        proved.formUnion(asking)
        let key = station.identityKey
        let factory = dependencies.transportFactory
        let clock = dependencies.clock
        let order = Array(asking)
        Task { [weak self] in
            let answered = await withTaskGroup(of: StationEndpoint?.self) { group -> Set<StationEndpoint> in
                for endpoint in order {
                    group.addTask {
                        let outcome = await CoreIdentityProbe.identify(endpoint, transportFactory: factory,
                                                                       clock: clock, deadline: Self.identifyDeadline)
                        return outcome == .identified(publicKey: key) ? endpoint : nil
                    }
                }
                var answered = Set<StationEndpoint>()
                for await endpoint in group {
                    if let endpoint {
                        answered.insert(endpoint)
                    }
                }
                return answered
            }
            let proven = order.filter(answered.contains)
            guard let self, !proven.isEmpty else {
                return
            }
            self.keepProven(proven, for: key)
        }
    }

    /// Makes the way through the remote access service to a paired Core
    /// (``Dependencies/serviceRoute``), keeping the Core's global addresses
    /// each introduction offers (``keepIntroduced(_:for:)``); nil where
    /// there is none.
    private var serviceRouteMaker: (@Sendable (PairedStation, DeviceIdentity) -> any CoreServiceRoute)? {
        guard let make = dependencies.serviceRoute else {
            return nil
        }
        let keep: @Sendable (Data, [String]) -> Void = { [weak self] key, lines in
            Task { @MainActor in self?.keepIntroduced(lines, for: key) }
        }
        return { station, device in
            let route = make(station, device)
            let key = station.identityKey
            route.observeCoreCandidates { keep(key, $0) }
            return route
        }
    }

    /// The Core's global addresses an introduction through the remote
    /// access service offered (R-IOS-16): its host candidates on a global
    /// address, each with the port the phone dials the Core's control
    /// channel on (``PairedStation/directDialPort``), never the candidate's
    /// ICE UDP port. The service passed them on, so they are the service's
    /// word, not the Core's: each is held in memory (``introduced``) and
    /// asked at once which Core answers there, and only one where this
    /// Core proves its identity is kept, whether or not that connection
    /// then opens. One that never answers as this Core never reaches the
    /// Keychain nor pushes out the Core's own list. Never logged.
    private func keepIntroduced(_ lines: [String], for key: Data) {
        guard let station = try? dependencies.stations.station(identityKey: key) else {
            return
        }
        let kept = Set(station.directAddresses.map(\.canonical))
        let seen = CoreAddressList.introducedAddresses(lines, port: station.directDialPort)
            .filter { !kept.contains($0.canonical) }
        guard !seen.isEmpty else {
            return
        }
        var held = introduced[key] ?? []
        for endpoint in seen where !held.contains(endpoint.canonical) {
            held.append(endpoint.canonical)
        }
        introduced[key] = Array(held.suffix(CoreAddressList.maximumCount))
        prove(seen, of: station)
    }

    private func keepProven(_ addresses: [StationEndpoint], for key: Data) {
        do {
            if let updated = try dependencies.stations.recordProvenAddresses(identityKey: key, addresses),
               current?.identityKey == key {
                current = updated
            }
        } catch {
            Self.logger.warning("Where the Core can be dialled from anywhere could not be kept in the Keychain")
        }
        reloadCores()
    }

    /// Keeps where the connected Core says it can be dialled (`devices`'
    /// `coreAddresses`, ``CoreAddressList``), replacing the last list, when
    /// the Core offers it (agreed minor 11 and `coreAddressesVersion` 1). An
    /// empty list, and a Core that sends none, leave the kept ones as they are.
    private func keepCoreAddresses() {
        guard phase == .connected, let station = current,
              CoreAddressList.isOffered(agreedMinor: app.mirror.agreedMinor ?? 0,
                                        capabilityVersion: app.mirror.capabilityVersion(CoreAddressList.capabilityName)),
              let text = app.main.coreAddresses, let list = CoreAddressList.parse(text), !list.isEmpty,
              list != station.directAddresses else {
            return
        }
        do {
            if let updated = try dependencies.stations.recordCoreAddresses(identityKey: station.identityKey, list) {
                current = updated
            }
        } catch {
            Self.logger.warning("Where the Core can be dialled from anywhere could not be kept in the Keychain")
        }
        reloadCores()
    }

    /// The capability a Core declares its control data channel with.
    nonisolated static let controlChannelCapability = "controlChannelVersion"
    nonisolated static let relayAllowedCapability = "relayAllowed"

    // MARK: Small parts

    /// The host, and the port when it isn't the Core's standard one.
    nonisolated static func addressText(_ endpoint: StationEndpoint?) -> String {
        guard let endpoint else {
            return ""
        }
        return endpoint.port == StationEndpoint.defaultPort ? endpoint.host : "\(endpoint.host) port \(endpoint.port)"
    }

    nonisolated static func seconds(_ duration: Duration) -> Int {
        let parts = duration.components
        return Int((Double(parts.seconds) + Double(parts.attoseconds) / 1e18).rounded(.up))
    }

    /// The runs of letters and digits in a typed code, lowercased.
    nonisolated static func codeParts(_ text: String) -> [String] {
        text.lowercased().split { !($0.isASCII && ($0.isLetter || $0.isNumber)) }.map(String.init)
    }

    /// Why `text` is not a code: the first word not in the Core's list,
    /// with the list's words that start most like it.
    nonisolated static func codeProblem(_ text: String) -> PairingProblem {
        let words = Set(PairingCodeText.words)
        for part in codeParts(text).dropFirst() where !words.contains(part) && part.allSatisfy(\.isLetter) {
            var prefix = part
            var found: [String] = []
            while !prefix.isEmpty && found.isEmpty {
                found = PairingCodeText.suggestions(forPrefix: prefix)
                prefix.removeLast()
            }
            return .notACode(word: part, suggestions: Array(found.prefix(4)))
        }
        return .notACode(word: nil, suggestions: [])
    }

    /// Connections the flow makes: the real one, watched for a connection
    /// that could not open and for iOS refusing the local network.
    private var observedFactory: LinkTransportFactory {
        let inner = dependencies.transportFactory
        let failures = failures
        return { endpoint, trust in
            ObservedTransport(inner: inner(endpoint, trust), failures: failures)
        }
    }
}

/// What the flow learns from connections that did not open.
final class OpenFailures: @unchecked Sendable {
    private let lock = NSLock()
    private var failed = false
    private var denied = false

    var openFailed: Bool { lock.withLock { failed } }
    var localNetworkDenied: Bool { lock.withLock { denied } }

    func reset() {
        lock.withLock {
            failed = false
            denied = false
        }
    }

    func record(_ error: Error) {
        lock.withLock {
            failed = true
            if let failure = error as? LinkTransportError, failure == .localNetworkDenied {
                denied = true
            }
        }
    }
}

/// A connection passed through as it is, noting a failure to open. Every
/// other part of the connection is the inner one's: its selected route and
/// traffic for the connection page, and its binary frames.
struct ObservedTransport: LinkTransport {
    let inner: any LinkTransport
    let failures: OpenFailures

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
        do {
            return try await inner.open(onEvent: onEvent)
        } catch {
            failures.record(error)
            throw error
        }
    }

    @discardableResult func send(_ text: String) -> Bool {
        inner.send(text)
    }

    func ping() {
        inner.ping()
    }

    func close() {
        inner.close()
    }
}

/// A way through the remote access service passed through as it is, each
/// of its connections noting a failure to open.
private final class ObservedServiceRoute: CoreServiceRoute {
    let inner: any CoreServiceRoute
    let failures: OpenFailures

    init(inner: any CoreServiceRoute, failures: OpenFailures) {
        self.inner = inner
        self.failures = failures
    }

    func makeTransport() -> any SessionTransport {
        ObservedTransport(inner: inner.makeTransport(), failures: failures)
    }

    func mediaIceSettings() -> IceSettings? {
        inner.mediaIceSettings()
    }

    var selectedPathRank: Int? { inner.selectedPathRank }
    func directOnlyRoute() -> (any CoreServiceRoute)? {
        inner.directOnlyRoute().map { ObservedServiceRoute(inner: $0, failures: failures) }
    }
    func mediaRelayContext() -> RelayRouteContext? { inner.mediaRelayContext() }

    var lastTry: ConnectionAttempt.Try? { inner.lastTry }
    var lastError: RendezvousDialError? { inner.lastError }
    func observeCoreCandidates(_ handler: @escaping @Sendable ([String]) -> Void) {
        inner.observeCoreCandidates(handler)
    }
}
