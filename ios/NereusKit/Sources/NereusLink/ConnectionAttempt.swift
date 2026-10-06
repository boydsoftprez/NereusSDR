// NereusSDR for iOS: one attempt to reach a Core, path by path, for the trouble screens
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One connection attempt, path by path, in the order tried (R-IOS-16; spec
/// section 5.3 item 14, "what the phone tried"). The desktop's twin is its
/// attempt record, `StationConnectionAttempt` (iPhone app plan Task 27),
/// with the same fields and the same words; the trouble screens (Task 56)
/// show it.
public struct ConnectionAttempt: Sendable, Equatable {
    /// This network: an address on one of this phone's own networks (or
    /// this phone). Direct: any other address. Relay: through the remote
    /// access service's relay.
    public enum Path: String, Sendable, Equatable, Codable {
        case thisNetwork
        case direct
        case relay
    }

    public enum Outcome: Sendable, Equatable {
        case trying
        case connected
        /// Nothing answered at that address.
        case noAnswer
        /// It did not answer in time.
        case timedOut
        /// Another computer answered there.
        case notThisCore
        /// It answered, and the attempt ended there.
        case failed
        /// A different verified path won, or the operator cancelled.
        case cancelled
        /// The computer at that address turned the connection away: nothing
        /// listens on that port there, or a firewall there says no.
        case refused
        /// This phone has no route to that address from the network it is on.
        case unreachable
    }

    /// One path tried.
    public struct Try: Sendable, Equatable {
        public var path: Path
        /// host:port as the operator would type it.
        public var address: String
        public var outcome: Outcome
        /// Through the remote access service: `address` is the service's
        /// host, which introduced the two ends; the connection itself went
        /// by `path`, straight between them or through the relay.
        public var throughService: Bool
        /// Through the service: what each end offered and how each pair
        /// went, once the dial ended; nil otherwise.
        public var ice: IceCandidateEvidence?

        public init(path: Path, address: String, outcome: Outcome = .trying, throughService: Bool = false,
                    ice: IceCandidateEvidence? = nil) {
            self.path = path
            self.address = address
            self.outcome = outcome
            self.throughService = throughService
            self.ice = ice
        }
    }

    public var started: Date
    public var tries: [Try]

    public init(started: Date = Date(), tries: [Try] = []) {
        self.started = started
        self.tries = tries
    }

    /// True when a path connected.
    public var connected: Bool {
        tries.contains { $0.outcome == .connected }
    }

    /// Starts a path.
    public mutating func begin(_ path: Path, address: String) {
        tries.append(Try(path: path, address: address))
    }

    /// Ends the path still being tried, if any.
    public mutating func end(_ outcome: Outcome) {
        guard let last = tries.indices.last, tries[last].outcome == .trying else {
            return
        }
        tries[last].outcome = outcome
    }

    /// Completes a particular concurrent rung without changing another.
    public mutating func end(_ index: Int, as outcome: Outcome) {
        guard tries.indices.contains(index), tries[index].outcome == .trying else { return }
        tries[index].outcome = outcome
    }

    /// Catches up with how concurrent paths ended after this record was
    /// copied: each path here still `.trying`, other than `kept` (the one
    /// the session owns), takes its outcome from `settled`, the same rows
    /// in the same order. A row `settled` also reports as still trying, or
    /// one it does not have, is left as it is.
    public mutating func settle(from settled: [Try], keeping kept: Int) {
        for index in tries.indices where index != kept && index < settled.count {
            guard tries[index].outcome == .trying, settled[index].outcome != .trying else { continue }
            tries[index].outcome = settled[index].outcome
        }
    }

    /// Plain words for the trouble screens, for example "Tried this network
    /// (192.168.1.20:47910): no answer; direct (shack.example.net:47910):
    /// connected." Empty before any try.
    public var summary: String {
        guard !tries.isEmpty else {
            return ""
        }
        let parts = tries.map { "\(Self.pathText($0)) (\($0.address)): \(Self.outcomeText($0.outcome))" }
        return "Tried " + parts.joined(separator: "; ") + "."
    }

    /// The words for how one try went: a path through the service names
    /// the service, so its host never reads as an address dialled directly
    /// (the link document's "through the internet service", section 21.1).
    public static func pathText(_ tried: Try) -> String {
        guard tried.throughService else {
            return pathText(tried.path)
        }
        return tried.path == .relay ? "relay through the internet service" : "through the internet service"
    }

    public static func pathText(_ path: Path) -> String {
        switch path {
        case .thisNetwork:
            return "this network"
        case .direct:
            return "direct"
        case .relay:
            return "relay"
        }
    }

    public static func outcomeText(_ outcome: Outcome) -> String {
        switch outcome {
        case .trying:
            return "still trying"
        case .connected:
            return "connected"
        case .noAnswer:
            return "no answer"
        case .timedOut:
            return "no answer in time"
        case .notThisCore:
            return "another computer answered"
        case .failed:
            return "did not connect"
        case .cancelled:
            return "stopped after another path connected"
        case .refused:
            return "the connection was refused"
        case .unreachable:
            return "no route to it from this network"
        }
    }

    /// This network for a name ending ".local", `localhost`, a loopback
    /// address or one inside the subnet of one of this phone's own
    /// addresses; direct otherwise.
    public static func path(for endpoint: StationEndpoint, networks: LocalNetworks) -> Path {
        let host = endpoint.canonical.host
        if host.hasSuffix(".local") || host == "localhost" {
            return .thisNetwork
        }
        return networks.contains(host) ? .thisNetwork : .direct
    }

    /// `host:port` as the operator would type it, an IPv6 literal in brackets.
    public static func addressText(_ endpoint: StationEndpoint) -> String {
        let host = endpoint.canonical.host
        return host.contains(":") ? "[\(host)]:\(endpoint.port)" : "\(host):\(endpoint.port)"
    }

    /// Reaches a Core: its last good addresses first, in order, each dialled
    /// by `dial`, then `throughService` when none connected, if one is
    /// given (the path through the remote access service: an introduction,
    /// then the connection it sets up). The service is not asked while an
    /// address the phone already has answers, so a phone with a cached
    /// address connects with the service unreachable (the pairing design,
    /// section 5.3). Returns the record of what was tried.
    public static func reach(
        endpoints: [StationEndpoint], networks: LocalNetworks = .current(), started: Date = Date(),
        dial: (StationEndpoint) async -> Outcome,
        throughService: (() async -> (path: Path, address: String, outcome: Outcome))? = nil
    ) async -> ConnectionAttempt {
        var attempt = ConnectionAttempt(started: started)
        for endpoint in endpoints {
            attempt.begin(path(for: endpoint, networks: networks), address: addressText(endpoint))
            let outcome = await dial(endpoint)
            attempt.end(outcome)
            if outcome == .connected {
                return attempt
            }
        }
        if let throughService {
            let result = await throughService()
            attempt.tries.append(Try(path: result.path, address: result.address, outcome: result.outcome))
        }
        return attempt
    }
}
