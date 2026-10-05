// NereusSDR for iOS: system proxy and PAC selection tests.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CFNetwork
import Foundation
import Network
import Testing
@testable import NereusLink

private struct ProxySettings: @unchecked Sendable {
    let value: NSDictionary
}

private final class MutableProxySettings: @unchecked Sendable {
    private let lock = NSLock()
    private var stored: NSDictionary
    init(_ value: NSDictionary) { stored = value }
    var value: NSDictionary { lock.withLock { stored } }
    func replace(_ value: NSDictionary) { lock.withLock { stored = value } }
}

private func proxyEntry(_ type: CFString, host: String? = nil, port: Int? = nil) -> NSDictionary {
    let entry = NSMutableDictionary()
    entry[kCFProxyTypeKey as NSString] = type as NSString
    if let host { entry[kCFProxyHostNameKey as NSString] = host }
    if let port { entry[kCFProxyPortNumberKey as NSString] = port }
    return entry
}

extension PACEvaluatingSuites {
    @Suite struct SystemProxyTests {
        @Test func staticRoutesKeepProxyAndDirectOrder() async throws {
            let resolver = SystemProxyResolver()
            let entries = [
                proxyEntry(kCFProxyTypeHTTP, host: "first.example", port: 8080),
                proxyEntry(kCFProxyTypeSOCKS, host: "second.example", port: 1080),
                proxyEntry(kCFProxyTypeNone),
            ]
            let plan = try await resolver.resolveEntries(entries, target: URL(string: "https://core.example/")!, timeout: .seconds(1))
            #expect(plan.routes == [
                .httpCONNECT(.init(host: "first.example", port: 8080)),
                .socks5(.init(host: "second.example", port: 1080)),
                .direct,
            ])
        }

        @Test func proxyOnlyAndInvalidEntriesDoNotCreateDirectFallback() async throws {
            let resolver = SystemProxyResolver()
            let plan = try await resolver.resolveEntries([
                proxyEntry(kCFProxyTypeFTP, host: "ftp.example", port: 21),
                proxyEntry(kCFProxyTypeHTTPS, host: "secure.example", port: 3128),
                proxyEntry(kCFProxyTypeSOCKS, host: "missing-port.example"),
            ], target: URL(string: "https://core.example/")!, timeout: .seconds(1))
            #expect(plan.routes == [.httpCONNECT(.init(host: "secure.example", port: 3128))])
            #expect(plan.diagnostics == [.unsupportedEntry(index: 0), .invalidEntry(index: 2)])
            let parameters = NWParameters.tcp
            let context = SystemProxyNetworkAdapter.apply(plan.routes[0], to: parameters)
            #expect(context.proxyConfigurations.count == 1)
            #expect(context.proxyConfigurations[0].allowFailover == false)
        }

        @Test func localhostAndLiteralLoopbackBypassSettings() async throws {
            let settings = ProxySettings(value: ["HTTPEnable": 1, "HTTPProxy": "proxy.example", "HTTPPort": 8080])
            let resolver = SystemProxyResolver(settingsProvider: { settings.value }, pacEvaluator: CFNetworkPACEvaluator())
            for host in ["localhost", "LOCALHOST", "127.2.3.4", "[::1]"] {
                let plan = try await resolver.resolve(for: URL(string: "wss://\(host):47910/")!)
                #expect(plan.routes == [.direct])
            }
        }

        @Test func webSocketSchemeUsesHTTPProxySettings() async throws {
            let settings = ProxySettings(value: ["HTTPEnable": 1, "HTTPProxy": "proxy.example", "HTTPPort": 8080])
            let resolver = SystemProxyResolver(settingsProvider: { settings.value }, pacEvaluator: CFNetworkPACEvaluator())
            let plan = try await resolver.resolve(for: URL(string: "ws://core.example:47910/")!)
            #expect(plan.routes == [.httpCONNECT(.init(host: "proxy.example", port: 8080))])
        }

        @Test func eachResolutionReadsCurrentSettings() async throws {
            let settings = MutableProxySettings(["HTTPEnable": 1, "HTTPProxy": "first.example", "HTTPPort": 8080])
            let resolver = SystemProxyResolver(settingsProvider: { settings.value }, pacEvaluator: CFNetworkPACEvaluator())
            let target = URL(string: "ws://core.example:47910/")!
            #expect(try await resolver.resolve(for: target).routes == [.httpCONNECT(.init(host: "first.example", port: 8080))])
            settings.replace(["HTTPEnable": 1, "HTTPProxy": "second.example", "HTTPPort": 8081])
            #expect(try await resolver.resolve(for: target).routes == [.httpCONNECT(.init(host: "second.example", port: 8081))])
        }

        @Test func proxyDescriptionsNeverContainProvidedCredentials() async throws {
            let entry = NSMutableDictionary(dictionary: proxyEntry(kCFProxyTypeHTTP, host: "proxy.example", port: 8080))
            entry[kCFProxyUsernameKey as NSString] = "private-user"
            entry[kCFProxyPasswordKey as NSString] = "private-password"
            let plan = try await SystemProxyResolver().resolveEntries([entry], target: URL(string: "https://core.example/")!, timeout: .seconds(1))
            #expect(plan.routes == [.httpCONNECT(.init(host: "proxy.example", port: 8080, username: "private-user", password: "private-password"))])
            #expect(!String(describing: plan.routes[0]).contains("private-user"))
            #expect(!String(reflecting: plan.routes[0]).contains("private-password"))
        }

        /// A real PAC evaluation's deadline where the test is not about the
        /// deadline: generous, so a busy machine cannot turn a PAC result into
        /// a timeout. A passing evaluation returns long before it.
        static let pacTimeout: Duration = .seconds(30)

        @Test func nativePACSelectsByTargetAndPreservesItsDirectChoice() async throws {
            let entry = NSMutableDictionary()
            entry[kCFProxyTypeKey as NSString] = kCFProxyTypeAutoConfigurationJavaScript as NSString
            entry[kCFProxyAutoConfigurationJavaScriptKey as NSString] = """
                function FindProxyForURL(url, host) {
                    if (host == 'direct.example') return 'DIRECT';
                    return 'PROXY gateway.example:3128; SOCKS socks.example:1080; DIRECT';
                }
                """
            let resolver = SystemProxyResolver()
            let direct = try await resolver.resolveEntries([entry], target: URL(string: "https://direct.example/")!, timeout: Self.pacTimeout)
            #expect(direct.routes == [.direct])
            let other = try await resolver.resolveEntries([entry], target: URL(string: "https://other.example/")!, timeout: Self.pacTimeout)
            #expect(other.routes == [
                .httpCONNECT(.init(host: "gateway.example", port: 3128)),
                .socks5(.init(host: "socks.example", port: 1080)),
                .direct,
            ])
        }

        @Test func failedPACKeepsConfiguredHTTPSFallback() async throws {
            let settings = ProxySettings(value: [
                "ProxyAutoConfigEnable": 1,
                "ProxyAutoConfigJavaScript": "function FindProxyForURL(url, host) {",
                "HTTPSEnable": 1,
                "HTTPSProxy": "static.example",
                "HTTPSPort": 3128,
            ])
            let resolver = SystemProxyResolver(settingsProvider: { settings.value }, pacEvaluator: CFNetworkPACEvaluator())
            let plan = try await resolver.resolve(for: URL(string: "wss://core.example:47910/")!, timeout: Self.pacTimeout)
            #expect(plan.routes == [.httpCONNECT(.init(host: "static.example", port: 3128))])
            #expect(plan.diagnostics == [.pacFailure(index: 0)])
        }

        @Test func failedPACWithoutConfiguredFallbackNeverBecomesDirect() async throws {
            // CFNetwork may itself append a DIRECT entry to system settings.
            // Supply only PAC here to prove this resolver adds no such route.
            let pac = NSMutableDictionary()
            pac[kCFProxyTypeKey as NSString] = kCFProxyTypeAutoConfigurationJavaScript as NSString
            pac[kCFProxyAutoConfigurationJavaScriptKey as NSString] = "function FindProxyForURL(url, host) {"
            await #expect(throws: SystemProxyError.pacFailed) {
                _ = try await SystemProxyResolver().resolveEntries([pac], target: URL(string: "https://core.example/")!, timeout: Self.pacTimeout)
            }
        }

        @Test func failedPACEarlierChoicesAndExplicitDirectRemainOrdered() async throws {
            let pac = NSMutableDictionary()
            pac[kCFProxyTypeKey as NSString] = kCFProxyTypeAutoConfigurationJavaScript as NSString
            pac[kCFProxyAutoConfigurationJavaScriptKey as NSString] = "function FindProxyForURL(url, host) {"
            let plan = try await SystemProxyResolver().resolveEntries([
                proxyEntry(kCFProxyTypeSOCKS, host: "first.example", port: 1080),
                pac,
                proxyEntry(kCFProxyTypeNone),
            ], target: URL(string: "https://core.example/")!, timeout: Self.pacTimeout)
            #expect(plan.routes == [.socks5(.init(host: "first.example", port: 1080)), .direct])
            #expect(plan.diagnostics == [.pacFailure(index: 1)])
        }

        @Test func exhaustedDeadlineDoesNotTryConfiguredFallback() async throws {
            let pac = NSMutableDictionary()
            pac[kCFProxyTypeKey as NSString] = kCFProxyTypeAutoConfigurationJavaScript as NSString
            pac[kCFProxyAutoConfigurationJavaScriptKey as NSString] = "function FindProxyForURL(url, host) { return 'DIRECT'; }"
            await #expect(throws: SystemProxyError.timedOut) {
                _ = try await SystemProxyResolver().resolveEntries(
                    [pac, proxyEntry(kCFProxyTypeHTTP, host: "static.example", port: 3128)],
                    target: URL(string: "https://core.example/")!, timeout: .zero)
            }
        }

        @Test func pacFailureAndDeadlineDoNotBecomeDirect() async throws {
            let evaluator = CFNetworkPACEvaluator()
            let target = URL(string: "https://core.example/")!
            await #expect(throws: SystemProxyError.timedOut) {
                _ = try await evaluator.evaluate(.script("function FindProxyForURL(url, host) { return 'DIRECT'; }"), for: target, timeout: .zero)
            }
            await #expect(throws: SystemProxyError.pacFailed) {
                _ = try await evaluator.evaluate(.script("function FindProxyForURL(url, host) {"), for: target, timeout: Self.pacTimeout)
            }
        }

        @Test func cancelledPACDoesNotPublishLateResult() async throws {
            let evaluator = CFNetworkPACEvaluator()
            let target = URL(string: "https://core.example/")!
            let task = Task {
                try await evaluator.evaluate(.script("function FindProxyForURL(url, host) { return 'DIRECT'; }"), for: target, timeout: Self.pacTimeout)
            }
            task.cancel()
            await #expect(throws: SystemProxyError.cancelled) { _ = try await task.value }
        }

        @Test func latePACCallbackCannotReplaceDeadline() async throws {
            let target = URL(string: "https://core.example/")!
            let operation = PACOperation(source: .script(""), target: target)
            operation.finish(.failure(.timedOut))
            operation.finish(.success(SystemProxyPACEntries(entries: [proxyEntry(kCFProxyTypeNone)])))
            await #expect(throws: SystemProxyError.timedOut) {
                _ = try await withCheckedThrowingContinuation { continuation in
                    operation.start(continuation: continuation, timeout: .seconds(1))
                }
            }
        }
    }
}
