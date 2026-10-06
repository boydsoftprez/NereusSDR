// NereusSDR for iOS: a sign-in made only to rename a Core that isn't connected, then closed
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import NereusMirror

/// One sign-in to a paired Core that isn't connected, made only to send
/// `station.rename` (D76, link document section 9.1) and closed after it,
/// so the phone is left as it was: no band, no sound, no media. It signs in
/// as any connection does (the same device key, trust and 30 s bound from
/// the dial), but never retries: a Core that doesn't answer, or asks the
/// phone to wait, is reported at once.
@MainActor
final class RenameSignIn {
    /// How the rename went.
    enum Outcome: Equatable {
        /// The Core answered the rename: accepted, or refused with its words.
        /// `name` is the Core's name as its devices object then reported it.
        case answered(CommandResult, name: String?)
        /// The Core signs in but has no `station.rename` (D23).
        case needsNewerCore
        /// The connection never opened, or the sign-in ran out of time.
        case notAnswering
        /// The Core refused the sign-in and ended it.
        case refused(Refusal)
        /// The Core asked the phone to wait, in its words.
        case waitAndRetry(String)
        /// The Core took the rename and gave no answer in time, or the link went.
        case noAnswer
        /// Cancel, before the Core answered.
        case cancelled
    }

    /// The verb, and the link minor and capability that carry it.
    static let verb = "station.rename"
    static let capability = "deviceAdminVersion"
    static let minimumMinor: UInt16 = 11
    /// How long the Core has to answer the rename.
    static let commandTimeout: Duration = .seconds(10)
    /// How long the Core has, after accepting, to send the name in its delta.
    static let nameWait: Duration = .seconds(2)

    /// Whether a Core with these capabilities, at this agreed minor, takes a rename.
    static func renames(_ mirror: MirrorStore) -> Bool {
        (mirror.agreedMinor ?? 0) >= minimumMinor && mirror.capabilityVersion(capability) >= 1
    }

    private let session: StationSession
    private let mirror: MirrorStore
    private let commands: CommandClient
    private let clock: any LinkClock
    private var cancelled = false
    /// Whether the Core, once signed in, took renames; nil until it said.
    private(set) var coreRenames: Bool?

    convenience init(station: PairedStation, endpoint: StationEndpoint, authenticator: any StationAuthenticator,
                     clock: any LinkClock, transportFactory: @escaping LinkTransportFactory) {
        self.init(station: station, authenticator: authenticator, clock: clock,
                  transport: { transportFactory(endpoint, station.trust) })
    }

    /// A sign-in over transports `transport` makes: through the remote
    /// access service (``CoreServiceRoute/makeTransport()``) after direct
    /// addresses do not answer, or when this phone keeps no address.
    init(station: PairedStation, authenticator: any StationAuthenticator, clock: any LinkClock,
         transport: @escaping @Sendable () -> any SessionTransport) {
        let session = StationSession(trust: station.trust, authenticator: authenticator, clock: clock,
                                     transport: transport)
        self.session = session
        self.clock = clock
        mirror = MirrorStore(session: session)
        commands = CommandClient(session: session, clock: clock)
    }

    /// Signs in, asks the Core to take `label` as its name, and closes.
    func run(label: String) async -> Outcome {
        let (events, sink) = AsyncStream.makeStream(of: StationSession.Event.self)
        let source = session.events
        let mirror = mirror
        let commands = commands
        let pump = Task { @MainActor in
            for await event in source {
                mirror.handle(event)
                await commands.handle(event)
                sink.yield(event)
            }
            sink.finish()
        }
        defer { pump.cancel() }
        await session.connect()

        var iterator = events.makeAsyncIterator()
        var refusal: Refusal?
        var ready = false
        waiting: while let event = await iterator.next() {
            switch event {
            case .refused(let reason):
                refusal = reason
            case .stateChanged(.ready):
                ready = true
                break waiting
            case .stateChanged(.waitingToRetry):
                // No retries: this sign-in is for one rename.
                await session.disconnect()
                if cancelled {
                    return .cancelled
                }
                if let refusal {
                    switch refusal.reason {
                    case .authentication(let words), .ended(let words, true):
                        return .waitAndRetry(words)
                    default:
                        return .refused(refusal)
                    }
                }
                return .notAnswering
            case .stateChanged(.stopped):
                if cancelled {
                    return .cancelled
                }
                return refusal.map(Outcome.refused) ?? .notAnswering
            default:
                continue
            }
        }
        guard ready, !cancelled else {
            await session.disconnect()
            return .cancelled
        }
        coreRenames = Self.renames(mirror)
        guard coreRenames == true else {
            await session.disconnect()
            return .needsNewerCore
        }
        let result: CommandResult
        do {
            result = try await commands.invoke(Self.verb, arguments: [CommandArgument(name: "label", value: .text(label))],
                                               timeout: Self.commandTimeout)
        } catch {
            await session.disconnect()
            return cancelled ? .cancelled : .noAnswer
        }
        var name: String?
        if result.accepted {
            // The Core sends its new name in the devices object's next delta;
            // wait a moment for it, then close whether or not it came.
            let deadline = clock.schedule(after: Self.nameWait) { [weak self] in
                await self?.session.disconnect()
            }
            if mirror.object("devices") != nil {
                while let event = await iterator.next() {
                    if case .message(.delta(let delta)) = event, delta.key == "devices" {
                        break
                    }
                    if case .stateChanged(.stopped) = event {
                        break
                    }
                }
            }
            deadline.cancel()
            name = currentName
        }
        await session.disconnect()
        return .answered(result, name: name)
    }

    /// Ends the sign-in; `run` returns ``Outcome/cancelled``.
    func cancel() async {
        cancelled = true
        await session.disconnect()
    }

    private var currentName: String? {
        if case .text(let name)? = mirror.object("devices")?["stationLabel"] {
            return name
        }
        return nil
    }
}
