// NereusSDR for iOS: the addresses a Core says it can be dialled at, from its devices object
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The Core's own dialable addresses (link document sections 6.1, 6.3, 7.1
/// and 21.1, R-IOS-16): a phone that declares `coreAddresses` 1 and signs in
/// with its own key at agreed minor 11 is sent `coreAddressesVersion` and
/// the `devices` object's `coreAddresses`, compact JSON such as
/// `{"addresses":["[2001:db8:1:0:211:22ff:fe33:4455]:47910","203.0.113.7:47910"]}`:
/// the Core's stable global IPv6 addresses, then any public IPv4 address on
/// its own interfaces, each with the port its listener holds, at most 8.
/// It comes at sign-in and again, as a `delta` of its own, when the Core's
/// interfaces renumber. The phone keeps the list with the paired Core
/// (``PairedStation/keepCoreAddresses(_:)``), never in a log, and races each
/// address as a direct rung.
public enum CoreAddressList {
    /// The hello feature the app declares, the capability that gates the
    /// property, the property itself, and the agreed minor and version it
    /// needs. The one place these are named.
    public static let featureName = "coreAddresses"
    public static let capabilityName = "coreAddressesVersion"
    public static let propertyName = "coreAddresses"
    public static let minimumMinor: UInt16 = 11
    public static let minimumVersion: Int64 = 1
    /// The most addresses a Core lists (`CoreAddresses::kMaxAddresses`).
    public static let maximumCount = 8

    /// Whether the Core sends its addresses to this app: agreed minor 11 and
    /// `coreAddressesVersion` 1 or later, as every per-feature capability
    /// is read.
    public static func isOffered(agreedMinor: UInt16, capabilityVersion: Int64) -> Bool {
        agreedMinor >= minimumMinor && capabilityVersion >= minimumVersion
    }

    /// The addresses in the Core's `coreAddresses` value, in its order, at
    /// most ``maximumCount``; nil when the value is not the Core's object.
    /// An entry is `[<IPv6>]:<port>` or `<IPv4>:<port>` with a port from 1 to
    /// 65535; one that is not, or names an address no network but the
    /// Core's own could reach (``LocalNetworks/isGlobal(_:)``), is left out.
    /// An empty list means the Core has none to name now, not that the ones
    /// kept are wrong.
    public static func parse(_ text: String) -> [StationEndpoint]? {
        guard case .object(let object)? = try? LinkJSON.parse(text), case .array(let entries)? = object["addresses"] else {
            return nil
        }
        var seen = Set<StationEndpoint>()
        var addresses: [StationEndpoint] = []
        for entry in entries {
            guard case .string(let written) = entry, let endpoint = endpoint(written),
                  seen.insert(endpoint.canonical).inserted else {
                continue
            }
            addresses.append(endpoint)
            if addresses.count == maximumCount {
                break
            }
        }
        return addresses
    }

    /// The Core's global addresses seen in an introduction through the
    /// remote access service (R-IOS-16): its answer's SDP and any candidate
    /// it trickled, each entry a candidate line or a whole SDP. Only a host
    /// candidate, the Core's own interface address, on a global address
    /// (``LocalNetworks/isGlobal(_:)``) is kept: never a reflexive or relay
    /// candidate, a unique local, link-local or private address, or an mDNS
    /// name. A candidate's port is an ICE UDP port, so each address is paired
    /// with `port`, the one the phone dials the Core's control channel on
    /// (``PairedStation/directDialPort``). In the order seen, each once. The
    /// addresses say where the operator is: never logged.
    public static func introducedAddresses(_ candidates: [String], port: UInt16) -> [StationEndpoint] {
        var seen = Set<StationEndpoint>()
        var addresses: [StationEndpoint] = []
        for entry in candidates {
            for line in entry.split(whereSeparator: \.isNewline) {
                guard let candidate = IceCandidateEvidence.Candidate(line: String(line)), candidate.kind == .host,
                      LocalNetworks.isGlobal(candidate.address) else {
                    continue
                }
                let endpoint = StationEndpoint(host: candidate.address.lowercased(), port: port)
                if seen.insert(endpoint.canonical).inserted {
                    addresses.append(endpoint)
                }
            }
        }
        return addresses
    }

    /// One entry as a direct address, or nil.
    static func endpoint(_ written: String) -> StationEndpoint? {
        let host: Substring
        let portText: Substring
        if written.hasPrefix("[") {
            guard let close = written.firstIndex(of: "]"),
                  written.index(after: close) < written.endIndex, written[written.index(after: close)] == ":" else {
                return nil
            }
            host = written[written.index(after: written.startIndex)..<close]
            portText = written[written.index(close, offsetBy: 2)...]
            guard host.contains(":") else {
                return nil
            }
        } else {
            let parts = written.split(separator: ":", omittingEmptySubsequences: false)
            guard parts.count == 2 else {
                return nil
            }
            host = parts[0]
            portText = parts[1]
        }
        guard !portText.isEmpty, portText.allSatisfy(\.isASCII), portText.allSatisfy(\.isNumber),
              portText.count <= 5, let port = UInt16(portText), port != 0,
              LocalNetworks.isGlobal(String(host)) else {
            return nil
        }
        return StationEndpoint(host: host.lowercased(), port: port)
    }
}
