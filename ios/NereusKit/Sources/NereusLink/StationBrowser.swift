// NereusSDR for iOS: finds Cores on this Wi-Fi over Bonjour and resolves where each listens
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import dnssd
import Foundation
import Network
import os

/// Finds Cores on this network (D36, R-IOS-16, link document section 14.2):
/// it browses `_nereus-station._tcp` in `local.` with each service's TXT
/// record, reads each record into a ``FoundStation`` and resolves the
/// service for its host and port, which is the listener's own port and not
/// always 47910. Only resolved Cores are reported. Starting it is what makes
/// iOS ask the Local Network question, once; when the answer is no, the
/// update says so and a typed address and a code still work.
///
/// Resolving goes through a UDP path to the service, which sends nothing to
/// the Core: the path's remote endpoint is the address the connection then
/// dials, through the same WebSocket transport as a typed address. A
/// link-local IPv6 address keeps its zone. Beside it, the service's host is
/// resolved to every address it answers with (``BonjourHostResolver``), so
/// the race dials them all on this network and the phone can keep the
/// Core's global IPv6 addresses to dial from anywhere (R-IOS-16).
public actor StationBrowser: StationBrowsing {
    /// What is found now.
    public struct Update: Equatable, Sendable {
        /// The Cores found and resolved, by the name each row shows.
        public var stations: [FoundStation]
        /// iOS does not allow this app to look on the local network.
        public var localNetworkDenied: Bool

        public init(stations: [FoundStation] = [], localNetworkDenied: Bool = false) {
            self.stations = stations
            self.localNetworkDenied = localNetworkDenied
        }
    }

    /// The service a Core registers.
    public static let serviceType = "_nereus-station._tcp"

    private static let logger = Logger(subsystem: "NereusSDR", category: "link.browse")

    private let queue = DispatchQueue(label: "NereusSDR.station-browser")
    private var browser: NWBrowser?
    private var generation = 0
    private var onUpdate: (@Sendable (Update) -> Void)?
    /// The readable records by instance name.
    private var records: [String: FoundStation] = [:]
    /// The Bonjour endpoint each record came with, to resolve it.
    private var services: [String: NWEndpoint] = [:]
    private var resolved: [String: StationEndpoint] = [:]
    private var resolvers: [String: NWConnection] = [:]
    /// Every address of each service's host, and what follows it.
    private var hostAddresses: [String: [StationEndpoint]] = [:]
    private var hostResolvers: [String: BonjourHostResolver] = [:]
    private var denied = false
    private var lastReported: Update?

    public init() {}

    public func start(_ onUpdate: @escaping @Sendable (Update) -> Void) {
        stopBrowsing()
        generation += 1
        let current = generation
        self.onUpdate = onUpdate
        let browser = NWBrowser(for: .bonjourWithTXTRecord(type: Self.serviceType, domain: nil), using: .tcp)
        // Each handler runs on the serial queue; the order counter keeps a
        // late hop onto the actor from undoing a newer result set.
        let order = Order()
        browser.stateUpdateHandler = { [weak self] state in
            Task { await self?.browserState(state, generation: current) }
        }
        browser.browseResultsChangedHandler = { [weak self] results, _ in
            let sequence = order.next()
            Task { await self?.results(results, sequence: sequence, generation: current) }
        }
        self.browser = browser
        browser.start(queue: queue)
    }

    public func stop() {
        stopBrowsing()
        onUpdate = nil
        lastReported = nil
    }

    // MARK: What Bonjour reports

    private var lastSequence = 0

    private func browserState(_ state: NWBrowser.State, generation current: Int) {
        guard current == generation else {
            return
        }
        switch state {
        case .ready:
            denied = false
        case .waiting(let error), .failed(let error):
            if case .dns(let code) = error, code == DNSServiceErrorType(kDNSServiceErr_PolicyDenied) {
                denied = true
            } else {
                Self.logger.info("Looking for Cores paused: \(String(describing: error), privacy: .public)")
            }
        default:
            return
        }
        report()
    }

    private func results(_ results: Set<NWBrowser.Result>, sequence: Int, generation current: Int) {
        guard current == generation, sequence > lastSequence else {
            return
        }
        lastSequence = sequence
        var found: [String: (FoundStation, NWEndpoint)] = [:]
        for result in results {
            guard case .service(let name, _, _, _) = result.endpoint,
                  case .bonjour(let record) = result.metadata,
                  let station = FoundStation.parse(txtRecord: record.data, instanceName: name),
                  found[name] == nil else {
                continue
            }
            found[name] = (station, result.endpoint)
        }
        for name in records.keys where found[name] == nil {
            forget(name)
        }
        for (name, entry) in found {
            records[name] = entry.0
            if services[name] != entry.1 {
                services[name] = entry.1
                resolved[name] = nil
                hostAddresses[name] = nil
                resolve(name, service: entry.1, generation: current)
                resolveHost(name, service: entry.1, generation: current)
            } else if resolved[name] == nil, resolvers[name] == nil {
                resolve(name, service: entry.1, generation: current)
            }
        }
        report()
    }

    private func forget(_ name: String) {
        records[name] = nil
        services[name] = nil
        resolved[name] = nil
        resolvers.removeValue(forKey: name)?.cancel()
        hostAddresses[name] = nil
        hostResolvers.removeValue(forKey: name)?.cancel()
        hostSequences[name] = nil
    }

    // MARK: Resolving

    private func resolve(_ name: String, service: NWEndpoint, generation current: Int) {
        resolvers.removeValue(forKey: name)?.cancel()
        let connection = NWConnection(to: service, using: .udp)
        connection.stateUpdateHandler = { [weak self, weak connection] state in
            switch state {
            case .ready:
                let remote = connection?.currentPath?.remoteEndpoint
                Task { await self?.resolvedService(name, remote: remote, service: service, generation: current) }
            case .failed(let error):
                Self.logger.info("A Core found on this network could not be resolved: \(String(describing: error), privacy: .public)")
                Task { await self?.resolveFailed(name, service: service, generation: current) }
            default:
                break
            }
        }
        resolvers[name] = connection
        connection.start(queue: queue)
    }

    private func resolvedService(_ name: String, remote: NWEndpoint?, service: NWEndpoint, generation current: Int) {
        guard current == generation, services[name] == service else {
            return
        }
        resolvers.removeValue(forKey: name)?.cancel()
        guard let remote, let endpoint = Self.stationEndpoint(remote) else {
            return
        }
        resolved[name] = endpoint
        report()
    }

    private func resolveFailed(_ name: String, service: NWEndpoint, generation current: Int) {
        guard current == generation, services[name] == service else {
            return
        }
        resolvers.removeValue(forKey: name)?.cancel()
    }

    private func resolveHost(_ name: String, service: NWEndpoint, generation current: Int) {
        hostResolvers.removeValue(forKey: name)?.cancel()
        // Each resolver numbers its own reports from 1.
        hostSequences[name] = nil
        guard case .service(let instance, let type, let domain, let interface) = service else {
            return
        }
        let order = Order()
        let resolver = BonjourHostResolver(queue: queue) { [weak self] addresses, port in
            let sequence = order.next()
            Task { await self?.hostResolved(name, addresses: addresses, port: port, service: service,
                                            sequence: sequence, generation: current) }
        }
        hostResolvers[name] = resolver
        resolver.start(name: instance, type: type, domain: domain.isEmpty ? "local." : domain,
                       interfaceIndex: interface.map { UInt32(truncatingIfNeeded: $0.index) } ?? 0)
    }

    private var hostSequences: [String: Int] = [:]

    private func hostResolved(_ name: String, addresses: [BonjourAddress], port: UInt16, service: NWEndpoint,
                              sequence: Int, generation current: Int) {
        guard current == generation, services[name] == service, sequence > hostSequences[name, default: 0] else {
            return
        }
        hostSequences[name] = sequence
        hostAddresses[name] = Self.endpoints(addresses, port: port)
        report()
    }

    /// The addresses Bonjour answered for a Core's host, with the service's
    /// `port`, kept as a typed address is: each once, a link-local IPv6
    /// address with its interface as its zone (left out when none is
    /// known), in the order a direct dial prefers them
    /// (``LocalNetworks/directPreference(_:)``). None for port 0.
    public static func endpoints(_ addresses: [BonjourAddress], port: UInt16) -> [StationEndpoint] {
        guard port != 0 else {
            return []
        }
        var seen = Set<StationEndpoint>()
        var endpoints: [StationEndpoint] = []
        for address in addresses {
            let family = address.bytes.count == 4 ? AF_INET : AF_INET6
            guard address.bytes.count == 4 || address.bytes.count == 16,
                  var text = literal(Data(address.bytes), family: family) else {
                continue
            }
            if address.bytes.count == 16, address.bytes[0] == 0xFE, address.bytes[1] & 0xC0 == 0x80 {
                guard let zone = address.interfaceName, !zone.isEmpty else {
                    continue
                }
                text += "%\(zone)"
            }
            let endpoint = StationEndpoint(host: text.lowercased(), port: port)
            if seen.insert(endpoint.canonical).inserted {
                endpoints.append(endpoint)
            }
        }
        return LocalNetworks.directPreference(endpoints)
    }

    /// The host and port a resolved endpoint names, the way a typed address
    /// is kept: an IPv4 literal, an IPv6 literal without brackets (with its
    /// zone when it is link-local), or a name without its trailing dot.
    public static func stationEndpoint(_ endpoint: NWEndpoint) -> StationEndpoint? {
        guard case .hostPort(let host, let port) = endpoint, port.rawValue != 0 else {
            return nil
        }
        let text: String
        switch host {
        case .name(let name, _):
            text = name.hasSuffix(".") ? String(name.dropLast()) : name
        case .ipv4(let address):
            guard let written = literal(address.rawValue, family: AF_INET) else {
                return nil
            }
            text = written
        case .ipv6(let address):
            guard let written = literal(address.rawValue, family: AF_INET6) else {
                return nil
            }
            if address.isLinkLocal, let zone = address.interface?.name, !zone.isEmpty {
                text = "\(written)%\(zone)"
            } else {
                text = written
            }
        @unknown default:
            return nil
        }
        guard !text.isEmpty else {
            return nil
        }
        return StationEndpoint(host: text.lowercased(), port: port.rawValue)
    }

    private static func literal(_ raw: Data, family: Int32) -> String? {
        var buffer = [CChar](repeating: 0, count: Int(INET6_ADDRSTRLEN))
        let written = raw.withUnsafeBytes { bytes -> Bool in
            guard let base = bytes.baseAddress else {
                return false
            }
            return inet_ntop(family, base, &buffer, socklen_t(buffer.count)) != nil
        }
        guard written else {
            return nil
        }
        return String(decoding: buffer.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
    }

    // MARK: Reporting

    private func report() {
        let stations = records.compactMap { name, station -> FoundStation? in
            let addresses = hostAddresses[name] ?? []
            guard let endpoint = resolved[name] ?? addresses.first else {
                return nil
            }
            var found = station
            found.endpoint = endpoint
            found.endpoints = addresses
            return found
        }.sorted { ($0.displayName, $0.instanceName) < ($1.displayName, $1.instanceName) }
        let update = Update(stations: stations, localNetworkDenied: denied)
        guard update != lastReported else {
            return
        }
        lastReported = update
        onUpdate?(update)
    }

    private func stopBrowsing() {
        generation += 1
        browser?.cancel()
        browser = nil
        for connection in resolvers.values {
            connection.cancel()
        }
        resolvers = [:]
        for resolver in hostResolvers.values {
            resolver.cancel()
        }
        hostResolvers = [:]
        hostAddresses = [:]
        hostSequences = [:]
        records = [:]
        services = [:]
        resolved = [:]
        denied = false
        lastSequence = 0
    }

    /// Numbers the result sets in the order the queue delivers them.
    private final class Order: @unchecked Sendable {
        private let lock = NSLock()
        private var count = 0

        func next() -> Int {
            lock.withLock {
                count += 1
                return count
            }
        }
    }
}
