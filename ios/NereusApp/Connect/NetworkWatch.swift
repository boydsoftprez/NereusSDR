// NereusSDR for iOS: whether this phone has a network at all, Wi-Fi or cellular
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Darwin
import Network
import os

/// Stable, private topology facts observed at one path callback. Missing
/// addresses mean enumeration failed; they are not an empty address list.
struct NetworkSignature: Equatable, Sendable {
    struct Address: Hashable, Sendable {
        var interface: String
        var bytes: [UInt8]
        var prefixLength: Int
        var scopeID: UInt32
    }

    var addresses: Set<Address>?
    var gateways: Set<String>
    var supportsIPv4: Bool
    var supportsIPv6: Bool

    init(addresses: some Sequence<Address>?, gateways: some Sequence<String>,
         supportsIPv4: Bool, supportsIPv6: Bool) {
        self.addresses = addresses.map { Set($0) }
        self.gateways = Set(gateways)
        self.supportsIPv4 = supportsIPv4
        self.supportsIPv6 = supportsIPv6
    }
}

/// The phone's network as the connecting flow sees it: online or not, and
/// which interfaces carry it, so a new one (Wi-Fi to cellular) is a change
/// even when the phone was online all along.
struct NetworkPath: Equatable, Sendable {
    /// Wi-Fi, cellular or a wired network is up.
    var online: Bool
    /// The names of the interfaces that carry it, as the system lists them;
    /// empty while offline.
    var interfaces: [String]
    /// The phone reaches the network over cellular, not Wi-Fi or a wired
    /// network: the band's data mode is cellular's (D28).
    var cellular = false
    /// The active path uses Wi-Fi. False can also mean wired or unknown.
    var wifi = false
    var signature: NetworkSignature?

    init(online: Bool, interfaces: [String], cellular: Bool = false, wifi: Bool = false,
         signature: NetworkSignature? = nil) {
        self.online = online
        self.interfaces = interfaces
        self.cellular = cellular
        self.wifi = wifi
        self.signature = signature
    }

    static let offline = NetworkPath(online: false, interfaces: [])
}

/// Whether the phone is online, for the offline trouble screen and the
/// NO NETWORK cover: with no network the phone waits for one, and dials
/// again as soon as one is back or another interface comes up.
@MainActor
protocol NetworkWatch: AnyObject {
    /// Starts watching; `changed` runs on the main actor with each change,
    /// and once at the start.
    func start(_ changed: @escaping @MainActor (_ path: NetworkPath) -> Void)
}

/// The phone's own network path.
@MainActor
final class SystemNetworkWatch: NetworkWatch {
    private let monitor = NWPathMonitor()
    private nonisolated static let logger = Logger(subsystem: "NereusSDR", category: "connect.network")

    /// The interfaces that can reach a Core. A path the system calls
    /// satisfied over only a tunnel or loopback (a VPN left up in Airplane
    /// Mode) has none of these under it, so the phone is offline.
    nonisolated static let carrierTypes: [NWInterface.InterfaceType] = [.wifi, .cellular, .wiredEthernet]

    func start(_ changed: @escaping @MainActor (_ path: NetworkPath) -> Void) {
        monitor.pathUpdateHandler = { path in
            let seen = Self.networkPath(path)
            let kinds = path.availableInterfaces.map { "\($0.type)" }.joined(separator: ",")
            Self.logger.info("Network path \(String(describing: path.status), privacy: .public) over \(kinds, privacy: .public): online \(seen.online, privacy: .public)")
            Task { @MainActor in changed(seen) }
        }
        monitor.start(queue: DispatchQueue(label: "NereusSDR.network-watch"))
    }

    /// Online only over Wi-Fi, cellular or a wired network, with the names
    /// of those interfaces.
    nonisolated static func networkPath(_ path: NWPath) -> NetworkPath {
        let carriers = path.availableInterfaces.filter { carrierTypes.contains($0.type) }
        guard path.status == .satisfied, !carriers.isEmpty else {
            return .offline
        }
        let cellular = path.usesInterfaceType(.cellular) && !path.usesInterfaceType(.wifi)
            && !path.usesInterfaceType(.wiredEthernet)
        return NetworkPath(online: true, interfaces: carriers.map(\.name),
                           cellular: cellular, wifi: path.usesInterfaceType(.wifi),
                           signature: NetworkSignature(
                            addresses: interfaceAddresses(names: Set(carriers.map(\.name))),
                            gateways: path.gateways.compactMap(gatewayKey),
                            supportsIPv4: path.supportsIPv4,
                            supportsIPv6: path.supportsIPv6))
    }

    private nonisolated static func gatewayKey(_ endpoint: NWEndpoint) -> String? {
        guard case let .hostPort(host, _) = endpoint else { return nil }
        switch host {
        case let .ipv4(address):
            return "4:\(address.rawValue.map { String(format: "%02x", $0) }.joined())"
        case let .ipv6(address):
            let bytes = address.rawValue.map { String(format: "%02x", $0) }.joined()
            return "6:\(bytes)%\(address.interface?.name ?? "")"
        case .name:
            return nil
        @unknown default:
            return nil
        }
    }

    /// Read only addresses on the path's carrier interfaces. Available
    /// interfaces are used consistently with the existing online decision;
    /// a separate path-selected-interface API is not exposed by NWPath.
    private nonisolated static func interfaceAddresses(names: Set<String>) -> [NetworkSignature.Address]? {
        var list: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&list) == 0, let first = list else { return nil }
        defer { freeifaddrs(list) }
        var result: [NetworkSignature.Address] = []
        var cursor: UnsafeMutablePointer<ifaddrs>? = first
        while let item = cursor {
            defer { cursor = item.pointee.ifa_next }
            let name = String(cString: item.pointee.ifa_name)
            let flags = Int32(item.pointee.ifa_flags)
            guard names.contains(name), flags & IFF_UP != 0, flags & IFF_RUNNING != 0,
                  flags & IFF_LOOPBACK == 0,
                  let address = item.pointee.ifa_addr, let mask = item.pointee.ifa_netmask else { continue }
            let family = Int32(address.pointee.sa_family)
            let offset: Int
            let count: Int
            let scopeID: UInt32
            switch family {
            case AF_INET:
                offset = MemoryLayout<sockaddr_in>.offset(of: \.sin_addr)!
                count = 4
                scopeID = 0
            case AF_INET6:
                offset = MemoryLayout<sockaddr_in6>.offset(of: \.sin6_addr)!
                count = 16
                guard Int(address.pointee.sa_len) >= MemoryLayout<sockaddr_in6>.size else { continue }
                scopeID = UnsafeRawPointer(address).load(fromByteOffset:
                    MemoryLayout<sockaddr_in6>.offset(of: \.sin6_scope_id)!, as: UInt32.self)
            default:
                continue
            }
            guard Int(address.pointee.sa_len) >= offset + count else { continue }
            let bytes = (0..<count).map { UnsafeRawPointer(address).load(fromByteOffset: offset + $0, as: UInt8.self) }
            guard !bytes.allSatisfy({ $0 == 0 }),
                  !(count == 4 && (bytes[0] == 127 || bytes[0] >= 224)),
                  !(count == 16 && (bytes[0] == 0xff || bytes[0..<15].allSatisfy({ $0 == 0 }) && bytes[15] == 1))
            else { continue }
            let maskLength = Int(mask.pointee.sa_len)
            let prefix = (0..<count).reduce(0) { value, index in
                value + (offset + index < maskLength
                    ? UnsafeRawPointer(mask).load(fromByteOffset: offset + index, as: UInt8.self).nonzeroBitCount : 0)
            }
            result.append(.init(interface: name, bytes: bytes, prefixLength: prefix, scopeID: scopeID))
        }
        return result
    }

    deinit {
        monitor.cancel()
    }
}
