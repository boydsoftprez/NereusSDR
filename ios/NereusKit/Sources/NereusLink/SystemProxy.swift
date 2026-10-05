// NereusSDR for iOS: ordered system proxy choices for WebSocket dials.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// NereusSDR-original. The localhost bypass follows Core SystemProxy.cpp at 0b41e581.

import CFNetwork
import Darwin
import Foundation
import Network

public struct SystemProxyEndpoint: Sendable, Equatable, CustomStringConvertible, CustomDebugStringConvertible {
    public let host: String
    public let port: UInt16
    public let username: String?
    public let password: String?

    public init(host: String, port: UInt16, username: String? = nil, password: String? = nil) {
        self.host = host
        self.port = port
        self.username = username
        self.password = password
    }

    /// Credentials are deliberately absent from ordinary descriptions.
    public var description: String { "\(host):\(port)" }
    public var debugDescription: String { description }
}

public enum SystemProxyRoute: Sendable, Equatable, CustomStringConvertible, CustomDebugStringConvertible {
    case direct
    case httpCONNECT(SystemProxyEndpoint)
    case socks5(SystemProxyEndpoint)

    public var description: String {
        switch self {
        case .direct: "direct"
        case .httpCONNECT(let endpoint): "HTTP CONNECT \(endpoint)"
        case .socks5(let endpoint): "SOCKS5 \(endpoint)"
        }
    }
    public var debugDescription: String { description }
}

public enum SystemProxyDiagnostic: Sendable, Equatable {
    case invalidEntry(index: Int)
    case unsupportedEntry(index: Int)
    case pacFailure(index: Int)
}

public struct SystemProxyPlan: Sendable, Equatable {
    /// Retry these in order. An empty list is a failure, never implicit direct access.
    public let routes: [SystemProxyRoute]
    public let diagnostics: [SystemProxyDiagnostic]

    public init(routes: [SystemProxyRoute], diagnostics: [SystemProxyDiagnostic] = []) {
        self.routes = routes
        self.diagnostics = diagnostics
    }
}

public enum SystemProxyError: Error, Sendable, Equatable {
    case invalidTarget
    case noUsableRoute
    case pacFailed
    case timedOut
    case cancelled
}

enum SystemProxyPACSource: Sendable {
    case script(String)
    case url(URL)
}

protocol SystemProxyPACEvaluating: Sendable {
    func evaluate(_ source: SystemProxyPACSource, for target: URL, timeout: Duration) async throws -> SystemProxyPACEntries
}

struct SystemProxyPACEntries: @unchecked Sendable {
    let entries: [NSDictionary]
}

public struct SystemProxyResolver: Sendable {
    private let settingsProvider: @Sendable () -> NSDictionary?
    private let pacEvaluator: any SystemProxyPACEvaluating

    public init() {
        settingsProvider = { CFNetworkCopySystemProxySettings()?.takeRetainedValue() as NSDictionary? }
        pacEvaluator = CFNetworkPACEvaluator()
    }

    init(settingsProvider: @escaping @Sendable () -> NSDictionary?, pacEvaluator: any SystemProxyPACEvaluating) {
        self.settingsProvider = settingsProvider
        self.pacEvaluator = pacEvaluator
    }

    /// Reads current settings on every call. CFNetwork selects by HTTP(S) URL,
    /// because its proxy lookup does not recognize WebSocket schemes.
    public func resolve(for target: URL, timeout: Duration = .seconds(5)) async throws -> SystemProxyPlan {
        guard let host = target.host, !host.isEmpty, let scheme = target.scheme?.lowercased(),
              scheme == "ws" || scheme == "wss" || scheme == "http" || scheme == "https" else {
            throw SystemProxyError.invalidTarget
        }
        if Self.isLocalhostOrLiteralLoopback(host) {
            return SystemProxyPlan(routes: [.direct])
        }
        var lookup = URLComponents(url: target, resolvingAgainstBaseURL: false)
        lookup?.scheme = (scheme == "wss") ? "https" : (scheme == "ws" ? "http" : scheme)
        guard let lookupURL = lookup?.url else { throw SystemProxyError.invalidTarget }
        guard let settings = settingsProvider() else { return SystemProxyPlan(routes: [.direct]) }
        let selected = CFNetworkCopyProxiesForURL(lookupURL as CFURL, settings as CFDictionary).takeRetainedValue() as NSArray
        return try await resolveEntries(selected.compactMap { $0 as? NSDictionary }, target: lookupURL, timeout: timeout)
    }

    func resolveEntries(_ entries: [NSDictionary], target: URL, timeout: Duration) async throws -> SystemProxyPlan {
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        var routes: [SystemProxyRoute] = []
        var diagnostics: [SystemProxyDiagnostic] = []
        var hadPACFailure = false
        for (index, entry) in entries.enumerated() {
            if Task.isCancelled { throw SystemProxyError.cancelled }
            guard let type = entry[kCFProxyTypeKey as NSString] as? String else {
                diagnostics.append(.invalidEntry(index: index))
                continue
            }
            if type == (kCFProxyTypeAutoConfigurationURL as NSString as String)
                || type == (kCFProxyTypeAutoConfigurationJavaScript as NSString as String) {
                let source: SystemProxyPACSource
                if type == (kCFProxyTypeAutoConfigurationURL as NSString as String),
                   let url = entry[kCFProxyAutoConfigurationURLKey as NSString] as? URL {
                    source = .url(url)
                } else if type == (kCFProxyTypeAutoConfigurationJavaScript as NSString as String),
                          let script = entry[kCFProxyAutoConfigurationJavaScriptKey as NSString] as? String {
                    source = .script(script)
                } else {
                    diagnostics.append(.invalidEntry(index: index))
                    continue
                }
                let remaining = deadline - clock.now
                guard remaining > .zero else { throw SystemProxyError.timedOut }
                do {
                    let pacEntries = try await pacEvaluator.evaluate(source, for: target, timeout: remaining)
                    // PAC results may not contain another PAC indirection. Treat one as invalid.
                    let pac = Self.parse(pacEntries.entries, offset: index)
                    routes.append(contentsOf: pac.routes)
                    diagnostics.append(contentsOf: pac.diagnostics)
                } catch {
                    if Task.isCancelled { throw SystemProxyError.cancelled }
                    if let proxyError = error as? SystemProxyError {
                        switch proxyError {
                        case .cancelled, .timedOut: throw proxyError
                        default: break
                        }
                    }
                    if clock.now >= deadline { throw SystemProxyError.timedOut }
                    // CFNetwork's ordered list can include a static choice
                    // after PAC. Keep it available when this PAC source fails.
                    hadPACFailure = true
                    diagnostics.append(.pacFailure(index: index))
                }
            } else {
                let parsed = Self.parse([entry], offset: index)
                routes.append(contentsOf: parsed.routes)
                diagnostics.append(contentsOf: parsed.diagnostics)
            }
        }
        guard !routes.isEmpty else {
            throw hadPACFailure ? SystemProxyError.pacFailed : SystemProxyError.noUsableRoute
        }
        return SystemProxyPlan(routes: routes, diagnostics: diagnostics)
    }

    private static func parse(_ entries: [NSDictionary], offset: Int) -> SystemProxyPlan {
        var routes: [SystemProxyRoute] = []
        var diagnostics: [SystemProxyDiagnostic] = []
        for (position, entry) in entries.enumerated() {
            let index = offset + position
            guard let type = entry[kCFProxyTypeKey as NSString] as? String else {
                diagnostics.append(.invalidEntry(index: index))
                continue
            }
            if type == (kCFProxyTypeNone as NSString as String) {
                routes.append(.direct)
                continue
            }
            let isHTTP = type == (kCFProxyTypeHTTP as NSString as String)
                || type == (kCFProxyTypeHTTPS as NSString as String)
            let isSOCKS = type == (kCFProxyTypeSOCKS as NSString as String)
            guard isHTTP || isSOCKS else {
                diagnostics.append(.unsupportedEntry(index: index))
                continue
            }
            guard let host = entry[kCFProxyHostNameKey as NSString] as? String, !host.isEmpty,
                  let number = entry[kCFProxyPortNumberKey as NSString] as? NSNumber,
                  number.intValue > 0, number.intValue <= 65535 else {
                diagnostics.append(.invalidEntry(index: index))
                continue
            }
            let endpoint = SystemProxyEndpoint(
                host: host, port: UInt16(number.intValue),
                username: entry[kCFProxyUsernameKey as NSString] as? String,
                password: entry[kCFProxyPasswordKey as NSString] as? String)
            routes.append(isHTTP ? .httpCONNECT(endpoint) : .socks5(endpoint))
        }
        return SystemProxyPlan(routes: routes, diagnostics: diagnostics)
    }

    private static func isLocalhostOrLiteralLoopback(_ host: String) -> Bool {
        if host.lowercased() == "localhost" { return true }
        let bare = String(host.split(separator: "%", maxSplits: 1, omittingEmptySubsequences: false)[0])
        var ipv4 = in_addr()
        if inet_pton(AF_INET, bare, &ipv4) == 1 {
            return withUnsafeBytes(of: ipv4) { $0[0] == 127 }
        }
        var ipv6 = in6_addr()
        if inet_pton(AF_INET6, bare, &ipv6) == 1 {
            return withUnsafeBytes(of: ipv6) { bytes in
                bytes.prefix(15).allSatisfy { $0 == 0 } && bytes[15] == 1
            }
        }
        return false
    }
}

/// Applies one selected route to one connection's parameters. The caller owns
/// retries through SystemProxyPlan.routes, so Network must not invent direct
/// failover for a proxy-only policy.
public enum SystemProxyNetworkAdapter {
    @discardableResult
    public static func apply(_ route: SystemProxyRoute, to parameters: NWParameters) -> NWParameters.PrivacyContext {
        let context = NWParameters.PrivacyContext(description: "NereusSDR WebSocket")
        switch route {
        case .direct:
            context.proxyConfigurations = []
        case .httpCONNECT(let endpoint), .socks5(let endpoint):
            let proxyEndpoint = NWEndpoint.hostPort(
                host: NWEndpoint.Host(endpoint.host),
                port: NWEndpoint.Port(rawValue: endpoint.port)!)
            var configuration: ProxyConfiguration
            switch route {
            case .httpCONNECT: configuration = ProxyConfiguration(httpCONNECTProxy: proxyEndpoint)
            case .socks5: configuration = ProxyConfiguration(socksv5Proxy: proxyEndpoint)
            case .direct: preconditionFailure("unreachable")
            }
            configuration.allowFailover = false
            if let username = endpoint.username, let password = endpoint.password {
                configuration.applyCredential(username: username, password: password)
            }
            context.proxyConfigurations = [configuration]
        }
        parameters.setPrivacyContext(context)
        return context
    }
}
