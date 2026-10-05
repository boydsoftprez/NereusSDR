// NereusSDR for iOS: the remote access service, a STUN and TURN fake and a Core, all on this computer, for the interop tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process exists only on macOS; on the iOS simulator the
// interop tests are not built.
#if os(macOS)

import CryptoKit
import Darwin
import Foundation
import LinkTestSupport
import NereusLink

/// What the control-channel interop tests run, every piece on 127.0.0.1 and
/// nothing leaving this computer, as the Core's own tst_rendezvous_client
/// does:
///
/// - ``TurnFake``: `tests/tools/fake_turn_server.py`, a STUN and TURN (UDP)
///   server for tests, which prints `ALLOCATED n` and `RELEASED n`;
/// - ``Service``: the rendezvous service itself (`rendezvous/server`, `python3
///   -m nereus_rendezvous`), listening on a loopback port without TLS, its
///   STUN and TURN URLs the fake's, its relay secret made for the run;
/// - ``Core``: `nereus_rendezvous_peer core` (tests/tools), a Core as
///   nereusd runs it (a StationServer with no radio and StationRendezvous),
///   registered with the service and with this phone's key paired.
///
/// `ios/scripts/interop-test.sh` builds the peer and sets
/// `NEREUS_RENDEZVOUS_PEER`; without it these tests are skipped. Keys and the
/// relay secret are made at run time in a scratch directory and never
/// printed.
enum LocalRendezvous {
    struct Failure: Error, CustomStringConvertible {
        let description: String
    }

    /// The repository's root, from the fixtures' place in it.
    static var repository: URL {
        LinkFixtureLoader.root.deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent()
    }

    /// `nereus_rendezvous_peer`, or nil when the interop tests are not asked for.
    static var peerPath: String? {
        ProcessInfo.processInfo.environment["NEREUS_RENDEZVOUS_PEER"].flatMap { $0.isEmpty ? nil : $0 }
    }

    /// Immutable Core-service source supplied explicitly for the web-relay
    /// interop run. Ordinary interop continues to use this checkout.
    static var frozenSource: URL? {
        ProcessInfo.processInfo.environment["NEREUS_FROZEN_RENDEZVOUS_SOURCE"]
            .flatMap { $0.isEmpty ? nil : URL(fileURLWithPath: $0) }
    }

    /// Whether python3 can run the service (it needs websockets and cryptography).
    static var pythonReady: Bool {
        let probe = Process()
        probe.executableURL = URL(fileURLWithPath: "/usr/bin/env")
        probe.arguments = ["python3", "-c", "import websockets, cryptography"]
        probe.standardOutput = FileHandle.nullDevice
        probe.standardError = FileHandle.nullDevice
        guard (try? probe.run()) != nil else {
            return false
        }
        probe.waitUntilExit()
        return probe.terminationStatus == 0
    }

    /// A scratch directory, removed by ``remove(_:)``.
    static func scratch() throws -> URL {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("nereus-rv-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        try FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: url.path)
        return url
    }

    static func remove(_ url: URL) {
        try? FileManager.default.removeItem(at: url)
    }

    /// A TCP port free on 127.0.0.1 now.
    static func freeTcpPort() throws -> UInt16 {
        let socket = Darwin.socket(AF_INET, SOCK_STREAM, 0)
        guard socket >= 0 else {
            throw Failure(description: "no socket")
        }
        defer { Darwin.close(socket) }
        var address = sockaddr_in()
        address.sin_family = sa_family_t(AF_INET)
        address.sin_addr.s_addr = inet_addr("127.0.0.1")
        address.sin_port = 0
        var length = socklen_t(MemoryLayout<sockaddr_in>.size)
        let bound = withUnsafeMutablePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { raw in
                Darwin.bind(socket, raw, length) == 0 && getsockname(socket, raw, &length) == 0
            }
        }
        guard bound else {
            throw Failure(description: "no free port")
        }
        return UInt16(bigEndian: address.sin_port)
    }

    /// Waits until `condition` holds, polling, or throws naming `what`.
    static func waitUntil(_ what: String, within timeout: Duration = .seconds(30),
                          whileAlive: (() -> String?)? = nil,
                          _ condition: () -> Bool) async throws {
        let deadline = ContinuousClock.now + timeout
        while true {
            if let failure = whileAlive?() {
                throw Failure(description: "\(what): \(failure)")
            }
            if condition() {
                return
            }
            guard ContinuousClock.now < deadline else {
                throw Failure(description: "timed out waiting for \(what)")
            }
            try await Task.sleep(for: .milliseconds(50))
        }
    }

    /// One child process whose standard output is kept.
    final class Child: @unchecked Sendable {
        private let process = Process()
        private let output = Pipe()
        private let errorOutput = Pipe()
        private let lock = NSLock()
        private var text = ""
        private var errorText = ""

        init(executable: String, arguments: [String], environment: [String: String] = [:],
             directory: URL? = nil) throws {
            process.executableURL = URL(fileURLWithPath: executable)
            process.arguments = arguments
            var env = ProcessInfo.processInfo.environment
            for (key, value) in environment {
                env[key] = value
            }
            process.environment = env
            if let directory {
                process.currentDirectoryURL = directory
            }
            process.standardOutput = output
            process.standardError = errorOutput
            output.fileHandleForReading.readabilityHandler = { [weak self] handle in
                let data = handle.availableData
                guard !data.isEmpty else {
                    return
                }
                guard let self else {
                    return
                }
                let chunk = String(decoding: data, as: UTF8.self)
                self.lock.withLock { self.text += chunk }
            }
            errorOutput.fileHandleForReading.readabilityHandler = { [weak self] handle in
                let data = handle.availableData
                guard !data.isEmpty, let self else {
                    return
                }
                let chunk = String(decoding: data, as: UTF8.self)
                self.lock.withLock { self.errorText += chunk }
            }
            try process.run()
        }

        deinit {
            stop()
        }

        var standardOutput: String { lock.withLock { text } }
        var standardError: String { lock.withLock { errorText } }
        var isRunning: Bool { process.isRunning }

        /// Nil while alive; on exit, a bounded diagnostic with the actual
        /// status and stderr. The TURN fake never writes credentials there.
        var exitDiagnostic: String? {
            guard !process.isRunning else {
                return nil
            }
            let errors = String(standardError.suffix(4_000)).trimmingCharacters(in: .whitespacesAndNewlines)
            return "child exited (status \(process.terminationStatus), reason \(process.terminationReason)); stderr: \(errors.isEmpty ? "(empty)" : errors)"
        }

        /// Ends the child: SIGTERM, then SIGKILL if it has not gone within
        /// five seconds. Polled rather than `waitUntilExit()`, which on a
        /// loaded machine was seen to wait on its run loop for good after
        /// the child had gone (Task 56).
        func stop() {
            output.fileHandleForReading.readabilityHandler = nil
            errorOutput.fileHandleForReading.readabilityHandler = nil
            guard process.isRunning else {
                return
            }
            process.terminate()
            let deadline = Date().addingTimeInterval(5)
            while process.isRunning && Date() < deadline {
                usleep(20_000)
            }
            if process.isRunning {
                kill(process.processIdentifier, SIGKILL)
            }
        }
    }

    /// The STUN and TURN fake, with a relay secret made for the run.
    final class TurnFake: @unchecked Sendable {
        let port: UInt16
        let secret: String
        let secretFile: URL
        private let child: Child

        var exitDiagnostic: String? { child.exitDiagnostic }

        init(in directory: URL, options: [String] = []) async throws {
            secret = Data((0..<24).map { _ in UInt8.random(in: 0...255) }).map { String(format: "%02x", $0) }.joined()
            secretFile = directory.appendingPathComponent("turn-secret")
            try Data(secret.utf8).write(to: secretFile)
            let portFile = directory.appendingPathComponent("turn-port")
            child = try Child(executable: "/usr/bin/env",
                              arguments: ["python3",
                                          LocalRendezvous.repository.appendingPathComponent("tests/tools/fake_turn_server.py").path,
                                          "--secret-file", secretFile.path, "--port-file", portFile.path] + options)
            let startedChild = child
            try await LocalRendezvous.waitUntil("the TURN fake's port", whileAlive: { startedChild.exitDiagnostic }) {
                (try? String(contentsOf: portFile, encoding: .utf8)).flatMap { UInt16($0.trimmingCharacters(in: .whitespacesAndNewlines)) } != nil
            }
            port = UInt16((try String(contentsOf: portFile, encoding: .utf8)).trimmingCharacters(in: .whitespacesAndNewlines)) ?? 0
        }

        /// The fake's allocation count is currently live, release count cumulative.
        func count(_ word: String) -> Int {
            child.standardOutput.split(whereSeparator: \.isNewline).compactMap { line -> Int? in
                let parts = line.split(separator: " ")
                return parts.count == 2 && parts[0] == word ? Int(parts[1]) : nil
            }.max() ?? 0
        }

        /// The opaque transaction IDs of zero-lifetime Refresh requests,
        /// in arrival order. A retransmission must keep its ID.
        var releaseTransactions: [String] {
            child.standardOutput.split(whereSeparator: \.isNewline).compactMap { line in
                guard line.hasPrefix("RELEASE_REQUEST ") else { return nil }
                return line.split(separator: " ").first { $0.hasPrefix("tx=") }
                    .map { String($0.dropFirst(3)) }
            }
        }

        func saw(_ prefix: String) -> Bool {
            child.standardOutput.split(whereSeparator: \.isNewline).contains {
                $0.hasPrefix(prefix)
            }
        }

        func allocationCount(_ field: String) -> Int {
            guard let line = child.standardOutput.split(whereSeparator: \.isNewline)
                .last(where: { $0.hasPrefix("TURN ") }) else { return 0 }
            return line.split(separator: " ").first { $0.hasPrefix("\(field)=") }
                .flatMap { Int($0.dropFirst(field.count + 1)) } ?? 0
        }

        /// Current fake status and its last opaque allocation accounting line.
        var diagnostic: String {
            let last = child.standardOutput.split(whereSeparator: \.isNewline).last { $0.hasPrefix("TURN ") }
            return "\(last.map(String.init) ?? "no allocation line"); \(child.exitDiagnostic ?? "child running")"
        }

        func waitUntilReleased(_ count: Int, within timeout: Duration) async throws {
            do {
                try await LocalRendezvous.waitUntil("the allocations to be given back", within: timeout,
                                                    whileAlive: { self.child.exitDiagnostic }) {
                    self.count("RELEASED") >= count
                }
            } catch {
                throw Failure(description: "\(error); created \(allocationCount("created")), released \(allocationCount("released")), live \(allocationCount("live")); \(diagnostic)")
            }
        }

        /// Credentials as the service mints them (coturn's use-auth-secret):
        /// the password is base64 of HMAC-SHA1(secret, username).
        func credentials(user: String) -> (username: String, password: String) {
            let expires = Int(Date().timeIntervalSince1970) + 3_600
            let username = "\(expires):\(user)"
            let mac = HMAC<Insecure.SHA1>.authenticationCode(for: Data(username.utf8),
                                                             using: SymmetricKey(data: Data(secret.utf8)))
            return (username, Data(mac).base64EncodedString())
        }

        func stop() {
            child.stop()
        }
    }

    /// Frozen relay behind the frozen TLS front. Only the front listens on
    /// TCP, and only on loopback; the relay itself uses a private Unix socket.
    final class WebRelay: @unchecked Sendable {
        let frontPort: UInt16
        let authority: URL
        let secretFile: URL
        let reportFile: URL
        private let socketFile: URL
        private let certificate: URL
        private let source: URL
        private let relay: Child
        private var front: Child?

        init(in directory: URL, source: URL) async throws {
            self.source = source
            frontPort = try LocalRendezvous.freeTcpPort()
            authority = directory.appendingPathComponent("relay-ca.pem")
            let caKey = directory.appendingPathComponent("relay-ca-key.pem")
            certificate = directory.appendingPathComponent("relay-front.pem")
            let leafKey = directory.appendingPathComponent("relay-front-key.pem")
            let request = directory.appendingPathComponent("relay-front.csr")
            let caConfig = directory.appendingPathComponent("relay-ca.cnf")
            let leafConfig = directory.appendingPathComponent("relay-front.cnf")
            let leafExtensions = directory.appendingPathComponent("relay-front.ext")
            reportFile = directory.appendingPathComponent("relay-front-report.json")
            secretFile = directory.appendingPathComponent("relay-secret")
            socketFile = URL(fileURLWithPath: "/tmp/nereus-relay-\(UUID().uuidString.prefix(8)).sock")

            try Data("[req]\ndistinguished_name=dn\nx509_extensions=v3_ca\nprompt=no\n[dn]\nCN=Nereus relay test CA\n[v3_ca]\nbasicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n".utf8).write(to: caConfig)
            try Data("[req]\ndistinguished_name=dn\nprompt=no\n[dn]\nCN=127.0.0.1\n".utf8).write(to: leafConfig)
            try Data("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=IP:127.0.0.1\n".utf8).write(to: leafExtensions)
            try Self.openssl(["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                              "-config", caConfig.path, "-keyout", caKey.path, "-out", authority.path])
            try Self.openssl(["req", "-newkey", "rsa:2048", "-nodes", "-config", leafConfig.path,
                              "-keyout", leafKey.path, "-out", request.path])
            try Self.openssl(["x509", "-req", "-in", request.path, "-CA", authority.path,
                              "-CAkey", caKey.path, "-CAcreateserial", "-days", "1",
                              "-extfile", leafExtensions.path, "-out", certificate.path])
            // Python's TLS front expects the certificate and private key in
            // one PEM file. Neither is put in any child argument or log.
            var pem = try Data(contentsOf: certificate)
            pem.append(try Data(contentsOf: leafKey))
            try pem.write(to: certificate)
            try Data((0..<32).map { _ in UInt8.random(in: 0...255) }).write(to: secretFile)
            let configFile = directory.appendingPathComponent("relay.conf")
            try Data("[relay]\nsocket = \(socketFile.path)\nsocket_group =\nrelay_secret_file = \(secretFile.path)\nlog_level = info\n".utf8).write(to: configFile)
            relay = try Child(executable: "/usr/bin/env",
                              arguments: ["python3", "-m", "nereus_relay", "--config", configFile.path],
                              environment: ["PYTHONPATH": source.appendingPathComponent("rendezvous/server").path],
                              directory: directory)
            let relay = self.relay
            try await LocalRendezvous.waitUntil("the frozen relay Unix socket", whileAlive: { relay.exitDiagnostic }) {
                FileManager.default.fileExists(atPath: self.socketFile.path)
            }
        }

        private static func openssl(_ arguments: [String]) throws {
            let child = try Child(executable: "/usr/bin/openssl", arguments: arguments)
            let deadline = Date().addingTimeInterval(20)
            while child.isRunning && Date() < deadline { usleep(20_000) }
            guard !child.isRunning, child.exitDiagnostic?.contains("status 0") == true else {
                throw Failure(description: "the local TLS certificate could not be made")
            }
        }

        func startFront(service: Service, marker: Data) async throws {
            let script = source.appendingPathComponent("tests/tools/harness_front.py")
            front = try Child(executable: "/usr/bin/env",
                              arguments: ["python3", script.path, "--listen", "127.0.0.1",
                                          "--port", String(frontPort), "--cert", certificate.path,
                                          "--relay-socket", socketFile.path, "--service-port", String(service.port),
                                          "--marker", marker.map { String(format: "%02x", $0) }.joined(),
                                          "--report", reportFile.path], directory: serviceDirectory)
            let front = self.front
            try await LocalRendezvous.waitUntil("the loopback TLS front", whileAlive: { front?.exitDiagnostic }) {
                LocalRendezvous.accepts(port: self.frontPort)
            }
        }

        private var serviceDirectory: URL { certificate.deletingLastPathComponent() }

        func report() -> (controlDatagrams: Int, markerHits: Int, connections: Int) {
            guard let data = try? Data(contentsOf: reportFile),
                  let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
                return (0, 0, 0)
            }
            let datagrams = object["datagrams"] as? [String: Int] ?? [:]
            let hits = object["markerHits"] as? [String: Int] ?? [:]
            return (datagrams["1"] ?? 0, hits.values.reduce(0, +), object["relayConnections"] as? Int ?? 0)
        }

        func stop() {
            front?.stop()
            relay.stop()
            try? FileManager.default.removeItem(at: socketFile)
        }
    }

    /// The rendezvous service on 127.0.0.1, with the fake's STUN and TURN.
    final class Service: @unchecked Sendable {
        let port: UInt16
        private let child: Child

        init(in directory: URL, turn: TurnFake, webRelay: WebRelay? = nil,
             source: URL? = nil, turnEnabled: Bool = true) async throws {
            port = try LocalRendezvous.freeTcpPort()
            let config = """
            [rendezvous]
            listen = 127.0.0.1:\(port)
            stun_urls = stun:127.0.0.1:\(turn.port)
            turn_urls = turn:127.0.0.1:\(turn.port)?transport=udp
            \(turnEnabled ? "turn_secret_file = \(turn.secretFile.path)" : "")
            \(webRelay.map { "relay_url = wss://127.0.0.1:\($0.frontPort)/v1/relay\nrelay_secret_file = \($0.secretFile.path)" } ?? "")
            log_level = info

            """
            let configFile = directory.appendingPathComponent("rendezvous.conf")
            try Data(config.utf8).write(to: configFile)
            child = try Child(executable: "/usr/bin/env",
                              arguments: ["python3", "-m", "nereus_rendezvous", "--config", configFile.path],
                              environment: ["PYTHONPATH": (source ?? LocalRendezvous.repository)
                                  .appendingPathComponent("rendezvous/server").path],
                              directory: directory)
            let port = self.port
            try await LocalRendezvous.waitUntil("the service to listen") {
                LocalRendezvous.accepts(port: port)
            }
        }

        var server: RendezvousServer { RendezvousServer(host: "127.0.0.1", port: port) }

        func stop() {
            child.stop()
        }
    }

    /// Whether something accepts TCP connections on 127.0.0.1:`port`.
    static func accepts(port: UInt16) -> Bool {
        let socket = Darwin.socket(AF_INET, SOCK_STREAM, 0)
        guard socket >= 0 else {
            return false
        }
        defer { Darwin.close(socket) }
        var address = sockaddr_in()
        address.sin_family = sa_family_t(AF_INET)
        address.sin_addr.s_addr = inet_addr("127.0.0.1")
        address.sin_port = port.bigEndian
        return withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { raw in
                Darwin.connect(socket, raw, socklen_t(MemoryLayout<sockaddr_in>.size)) == 0
            }
        }
    }

    /// A Core registered with the service, this phone paired with it.
    final class Core: @unchecked Sendable {
        /// The Core's identity key (SubjectPublicKeyInfo DER) and its id on
        /// the service.
        let identityKey: Data
        let stationId: String
        private let child: Child
        var exitDiagnostic: String? { child.exitDiagnostic }

        init(in directory: URL, service: Service, pairedDevice: Data, relay: Bool = true,
             authority: URL? = nil, listenPort: UInt16? = nil,
             listenLoopback: Bool = false, media: Bool = false) async throws {
            guard let path = LocalRendezvous.peerPath else {
                throw Failure(description: "NEREUS_RENDEZVOUS_PEER is not set")
            }
            guard (listenPort == nil && !listenLoopback)
                    || (listenPort != nil && listenLoopback) else {
                throw Failure(description: "Core test listening requires both a port and loopback binding")
            }
            let coreDirectory = directory.appendingPathComponent("core")
            try FileManager.default.createDirectory(at: coreDirectory, withIntermediateDirectories: true)
            // The Core's identity key, made in the directory it will load it from.
            let key = try Child(executable: path, arguments: ["key", "--dir",
                                                              coreDirectory.appendingPathComponent("security").path])
            try await LocalRendezvous.waitUntil("the Core's key") { !key.isRunning }
            guard let spki = Base64URL.decode(key.standardOutput.trimmingCharacters(in: .whitespacesAndNewlines)) else {
                throw Failure(description: "the Core's key could not be read")
            }
            identityKey = spki
            let idFile = directory.appendingPathComponent("core-id")
            var arguments = ["core", "--dir", coreDirectory.path,
                             "--server", "ws://127.0.0.1:\(service.port)/",
                             "--paired", Base64URL.encode(pairedDevice),
                             "--id-file", idFile.path]
            if !relay { arguments += ["--relay", "deny"] }
            if let authority { arguments += ["--ca", authority.path] }
            if let listenPort { arguments += ["--listen", String(listenPort)] }
            if listenLoopback { arguments.append("--listen-loopback") }
            if media { arguments.append("--media") }
            child = try Child(executable: path, arguments: arguments)
            try await LocalRendezvous.waitUntil("the Core to register") {
                FileManager.default.fileExists(atPath: idFile.path)
            }
            let written = try String(contentsOf: idFile, encoding: .utf8).split(separator: " ")
            stationId = written.first.map(String.init) ?? ""
            guard stationId == RendezvousIdentity.stationId(spki: spki) else {
                throw Failure(description: "the Core registered under another id than its key gives")
            }
        }

        /// Whether the Core has signed a device in (it prints each session).
        var sessions: Int {
            child.standardOutput.split(whereSeparator: \.isNewline).filter { $0.contains("\"event\":\"session\"") }.count
        }

        func stop() {
            child.stop()
        }
    }
}

#endif
