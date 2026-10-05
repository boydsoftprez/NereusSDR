// NereusSDR for iOS: resolves a Core's Bonjour service to every address its host answers with
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import dnssd
import Foundation

/// One address Bonjour answered for a Core's host: its bytes (4 for IPv4,
/// 16 for IPv6) and the interface it was heard on, which a link-local
/// address needs to be dialled.
public struct BonjourAddress: Hashable, Sendable {
    public var bytes: [UInt8]
    public var interfaceName: String?

    public init(bytes: [UInt8], interfaceName: String?) {
        self.bytes = bytes
        self.interfaceName = interfaceName
    }

    /// The address in a socket address as DNS-SD reports it, on the
    /// interface `interfaceIndex` (0 when any); for IPv6 with no index, the
    /// address's own scope. Nil for a family other than IPv4 or IPv6.
    public init?(socketAddress: UnsafePointer<sockaddr>, interfaceIndex: UInt32) {
        var index = interfaceIndex
        switch Int32(socketAddress.pointee.sa_family) {
        case AF_INET:
            bytes = socketAddress.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { four in
                withUnsafeBytes(of: four.pointee.sin_addr) { Array($0) }
            }
        case AF_INET6:
            let (address, scope) = socketAddress.withMemoryRebound(to: sockaddr_in6.self, capacity: 1) { six in
                (withUnsafeBytes(of: six.pointee.sin6_addr) { Array($0) }, six.pointee.sin6_scope_id)
            }
            bytes = address
            if index == 0 {
                index = scope
            }
        default:
            return nil
        }
        interfaceName = Self.name(ofInterface: index)
    }

    private static func name(ofInterface index: UInt32) -> String? {
        guard index != 0 else {
            return nil
        }
        var buffer = [CChar](repeating: 0, count: Int(IF_NAMESIZE) + 1)
        guard if_indextoname(index, &buffer) != nil else {
            return nil
        }
        let name = String(decoding: buffer.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
        return name.isEmpty ? nil : name
    }
}

/// Resolves one Bonjour service (link document section 14.2) to its host
/// and port, then asks for every A and AAAA record of that host and keeps
/// the set current while it runs: `report` gets the whole set and the port
/// each time it settles. What the NWConnection path to the service picks is
/// one of these; the rest, a Core's global IPv6 addresses among them, are
/// what the phone proves and keeps to dial from anywhere (R-IOS-16).
///
/// Everything runs on `queue`; `cancel` stops it, and no report follows.
final class BonjourHostResolver: @unchecked Sendable {
    typealias Report = @Sendable (_ addresses: [BonjourAddress], _ port: UInt16) -> Void

    private let queue: DispatchQueue
    private let report: Report
    // Touched on `queue` only.
    private var resolveRef: DNSServiceRef?
    private var addressRef: DNSServiceRef?
    private var port: UInt16 = 0
    private var addresses: [BonjourAddress] = []
    private var cancelled = false

    init(queue: DispatchQueue, report: @escaping Report) {
        self.queue = queue
        self.report = report
    }

    /// Resolves the instance `name` of `type` in `domain`, heard on the
    /// interface `interfaceIndex` (0 for any).
    func start(name: String, type: String, domain: String, interfaceIndex: UInt32) {
        queue.async { [self] in
            guard !cancelled else {
                return
            }
            var ref: DNSServiceRef?
            let context = Unmanaged.passUnretained(self).toOpaque()
            let error = DNSServiceResolve(&ref, 0, interfaceIndex, name, type, domain, Self.resolved, context)
            guard error == DNSServiceErrorType(kDNSServiceErr_NoError), let ref else {
                return
            }
            DNSServiceSetDispatchQueue(ref, queue)
            resolveRef = ref
        }
    }

    /// Stops resolving. The resolver lives until its references are freed on its queue.
    func cancel() {
        queue.async { [self] in
            cancelled = true
            free()
        }
    }

    private func free() {
        if let ref = resolveRef {
            DNSServiceRefDeallocate(ref)
            resolveRef = nil
        }
        if let ref = addressRef {
            DNSServiceRefDeallocate(ref)
            addressRef = nil
        }
    }

    private func hostResolved(_ host: String, port networkOrder: UInt16, interfaceIndex: UInt32) {
        guard !cancelled, addressRef == nil else {
            return
        }
        port = UInt16(bigEndian: networkOrder)
        // The service is resolved once; its host's records are followed.
        queue.async { [self] in
            if let ref = resolveRef {
                DNSServiceRefDeallocate(ref)
                resolveRef = nil
            }
        }
        var ref: DNSServiceRef?
        let context = Unmanaged.passUnretained(self).toOpaque()
        let protocols = DNSServiceProtocol(kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6)
        let error = DNSServiceGetAddrInfo(&ref, 0, interfaceIndex, protocols, host, Self.answered, context)
        guard error == DNSServiceErrorType(kDNSServiceErr_NoError), let ref else {
            return
        }
        DNSServiceSetDispatchQueue(ref, queue)
        addressRef = ref
    }

    private func addressAnswered(_ address: BonjourAddress?, flags: DNSServiceFlags) {
        guard !cancelled else {
            return
        }
        if let address {
            if flags & DNSServiceFlags(kDNSServiceFlagsAdd) != 0 {
                if !addresses.contains(address) {
                    addresses.append(address)
                }
            } else {
                addresses.removeAll { $0 == address }
            }
        }
        if flags & DNSServiceFlags(kDNSServiceFlagsMoreComing) == 0 {
            report(addresses, port)
        }
    }

    private static let resolved: DNSServiceResolveReply = { _, _, interfaceIndex, error, _, host, port, _, _, context in
        guard error == DNSServiceErrorType(kDNSServiceErr_NoError), let host, let context else {
            return
        }
        let resolver = Unmanaged<BonjourHostResolver>.fromOpaque(context).takeUnretainedValue()
        resolver.hostResolved(String(cString: host), port: port, interfaceIndex: interfaceIndex)
    }

    private static let answered: DNSServiceGetAddrInfoReply = { _, flags, interfaceIndex, error, _, address, _, context in
        guard let context else {
            return
        }
        let resolver = Unmanaged<BonjourHostResolver>.fromOpaque(context).takeUnretainedValue()
        guard error == DNSServiceErrorType(kDNSServiceErr_NoError), let address else {
            resolver.addressAnswered(nil, flags: flags)
            return
        }
        resolver.addressAnswered(BonjourAddress(socketAddress: address, interfaceIndex: interfaceIndex), flags: flags)
    }
}
