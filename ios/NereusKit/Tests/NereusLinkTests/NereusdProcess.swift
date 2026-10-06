// NereusSDR for iOS: runs a scratch nereusd, kept off the network, for the pairing and sign-in test
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process exists only on macOS; on the iOS simulator this
// test is not built.
#if os(macOS)

import Darwin
import Foundation
import NereusLink

/// A real Core, `nereusd`, found through `NEREUS_NEREUSD`, which
/// `ios/scripts/interop-test.sh` sets after building it. It runs with a
/// scratch home, profile and config under /tmp, made for this run and
/// removed after it: no token and no paired device, so its pairing window
/// is open until the first device pairs. The config sets `remote_port` and
/// `remote_bind` together (a free port on the loopback address asked for,
/// 127.0.0.1 or ::1), turns the status page
/// off and keeps the console socket in a short `state_directory`; its
/// `radio_mac` names no radio. The Core and its console run under
/// `sandbox-exec` with a profile that refuses every outbound connection
/// but loopback and local sockets, so its radio discovery never reaches a
/// radio on this computer's networks.
///
/// Every console command takes the same `--config` and `--profile` as the
/// running Core; one exits 1 when no Core answers and 2 for a config file it
/// cannot read or validate. The pairing code is read from the console's
/// standard output and never printed. The Core's own standard output and
/// error go to `nereusd.log` in the scratch directory (``log``), for a
/// failing test to print; the Core never logs the code. Its port is picked
/// below the ephemeral range, and ``waitUntilListening(within:)`` proves the
/// Core listens on it before the app dials.
final class NereusdProcess: @unchecked Sendable {
    struct Failure: Error, CustomStringConvertible {
        let description: String
    }

    /// The Core's path, or nil when the test is not asked for.
    static var path: String? {
        ProcessInfo.processInfo.environment["NEREUS_NEREUSD"].flatMap { $0.isEmpty ? nil : $0 }
    }

    static let sandboxExec = "/usr/bin/sandbox-exec"

    /// Outbound traffic to loopback and local sockets only.
    static let sandboxProfile = """
        (version 1)
        (allow default)
        (deny network-outbound)
        (allow network-outbound (remote ip "localhost:*"))
        (allow network-outbound (remote unix-socket))
        """

    let endpoint: StationEndpoint
    private let binary: String
    private let scratch: URL
    private let config: URL
    private let sandbox: URL
    private let profile: String
    private let environment: [String: String]
    private let logFile: URL
    private let process = Process()

    /// Writes the scratch files and starts the Core, listening on `host`
    /// ("127.0.0.1" or "::1") only.
    init(host: String = "127.0.0.1") throws {
        guard let binary = Self.path else {
            throw Failure(description: "NEREUS_NEREUSD is not set")
        }
        guard FileManager.default.isExecutableFile(atPath: Self.sandboxExec) else {
            throw Failure(description: "\(Self.sandboxExec) is missing; the Core is never run unsandboxed")
        }
        self.binary = binary
        // A short path: the console socket's path is limited to about 104 bytes.
        var template = Array("/tmp/nps.XXXXXX".utf8CString)
        guard let made = mkdtemp(&template) else {
            throw Failure(description: "no scratch directory under /tmp")
        }
        scratch = URL(fileURLWithPath: String(cString: made))
        let home = scratch.appendingPathComponent("home")
        let state = scratch.appendingPathComponent("s")
        try FileManager.default.createDirectory(at: home, withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: state, withIntermediateDirectories: true)

        let port = try Self.freeLoopbackPort(ipv6: host.contains(":"))
        endpoint = StationEndpoint(host: host, port: port)
        profile = "iospair" + String(UInt32.random(in: 0...UInt32.max), radix: 16)
        config = scratch.appendingPathComponent("nereusd.conf")
        try """
            radio_mac = 02:00:00:00:00:01
            remote_port = \(port)
            remote_bind = \(host)
            status_page = off
            pairing_lan_click = deny
            state_directory = \(state.path)

            """.write(to: config, atomically: true, encoding: .utf8)
        sandbox = scratch.appendingPathComponent("offline.sb")
        try Self.sandboxProfile.write(to: sandbox, atomically: true, encoding: .utf8)

        var environment = ProcessInfo.processInfo.environment
        environment["HOME"] = home.path
        environment["CFFIXED_USER_HOME"] = home.path
        self.environment = environment

        process.executableURL = URL(fileURLWithPath: Self.sandboxExec)
        process.arguments = ["-f", sandbox.path, binary, "--config", config.path, "--profile", profile]
        process.environment = environment
        // Its output, kept for a failing test to print (never the code:
        // the Core does not log it).
        logFile = scratch.appendingPathComponent("nereusd.log")
        guard FileManager.default.createFile(atPath: logFile.path, contents: nil),
              let log = FileHandle(forWritingAtPath: logFile.path) else {
            throw Failure(description: "no log file in the scratch directory")
        }
        process.standardInput = FileHandle.nullDevice
        process.standardOutput = log
        process.standardError = log
        try process.run()
        try log.close()
    }

    /// What the Core has written to its standard output and error so far.
    var log: String {
        (try? String(contentsOf: logFile, encoding: .utf8)) ?? "(no log)"
    }

    deinit {
        stop()
    }

    /// The code `nereusd pairing show` prints once the Core answers. Never print it.
    func pairingCode(within timeout: Duration = .seconds(60)) async throws -> String {
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        while true {
            guard process.isRunning else {
                throw Failure(description: "the Core stopped before it answered its console")
            }
            let (status, output) = try await console(["pairing", "show"])
            switch status {
            case 0:
                let prefix = "Pairing code: "
                if let line = output.split(separator: "\n").first(where: { $0.hasPrefix(prefix) }) {
                    return String(line.dropFirst(prefix.count))
                }
            case 1:
                break
            default:
                throw Failure(description: "the console refused the scratch config (exit \(status))")
            }
            guard clock.now < deadline else {
                throw Failure(description: "the Core gave no pairing code within \(timeout)")
            }
            try await Task.sleep(for: .milliseconds(100))
        }
    }

    /// Runs one console command against this Core; its exit status and
    /// standard output.
    func console(_ arguments: [String]) async throws -> (Int32, String) {
        let command = Process()
        let output = Pipe()
        command.executableURL = URL(fileURLWithPath: Self.sandboxExec)
        command.arguments = ["-f", sandbox.path, binary] + arguments
            + ["--config", config.path, "--profile", profile]
        command.environment = environment
        command.standardInput = FileHandle.nullDevice
        command.standardOutput = output
        command.standardError = FileHandle.nullDevice
        try command.run()
        let data = output.fileHandleForReading.readDataToEndOfFile()
        while command.isRunning {
            try await Task.sleep(for: .milliseconds(10))
        }
        return (command.terminationStatus, String(decoding: data, as: UTF8.self))
    }

    /// Stops the Core (SIGTERM, then SIGKILL after five seconds) and removes
    /// the scratch files. The wait polls rather than using waitUntilExit(),
    /// which needs a run loop the test's threads do not have.
    func stop() {
        if process.isRunning {
            process.terminate()
            let deadline = Date().addingTimeInterval(5)
            while process.isRunning && Date() < deadline {
                usleep(10_000)
            }
            if process.isRunning {
                kill(process.processIdentifier, SIGKILL)
            }
        }
        try? FileManager.default.removeItem(at: scratch)
    }

    /// Waits until the Core's own port takes a TCP connection: `pairing
    /// show` answers from the console socket, which comes up even while the
    /// listener is still retrying, so it is not proof the Core can be dialled.
    func waitUntilListening(within timeout: Duration = .seconds(60)) async throws {
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        let ipv6 = endpoint.host.contains(":")
        while !Self.accepts(ipv6: ipv6, port: endpoint.port) {
            guard process.isRunning else {
                throw Failure(description: "the Core stopped before it listened on its port")
            }
            guard clock.now < deadline else {
                throw Failure(description: "the Core did not listen on its port within \(timeout)")
            }
            try await Task.sleep(for: .milliseconds(50))
        }
    }

    /// The ports tried for the Core: below macOS's ephemeral range
    /// (49152-65535), so no outgoing connection on this computer, and no
    /// socket bound to port 0, is handed one between the check here and the
    /// Core's own listen.
    static let candidatePorts: Range<UInt16> = 20_000 ..< 40_000

    /// A port on 127.0.0.1, or on ::1, that nothing holds now, picked at
    /// random from ``candidatePorts``.
    private static func freeLoopbackPort(ipv6: Bool) throws -> UInt16 {
        for _ in 0 ..< 200 {
            let port = UInt16.random(in: candidatePorts)
            if bindable(ipv6: ipv6, port: port) {
                return port
            }
        }
        throw Failure(description: "could not find a free port")
    }

    /// Whether a socket can bind `port` on loopback now.
    private static func bindable(ipv6: Bool, port: UInt16) -> Bool {
        let socket = Darwin.socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)
        guard socket >= 0 else {
            return false
        }
        defer { Darwin.close(socket) }
        return withLoopback(ipv6: ipv6, port: port) { address, length in
            Darwin.bind(socket, address, length) == 0
        }
    }

    /// Whether something takes a TCP connection at `port` on loopback now.
    private static func accepts(ipv6: Bool, port: UInt16) -> Bool {
        let socket = Darwin.socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)
        guard socket >= 0 else {
            return false
        }
        defer { Darwin.close(socket) }
        return withLoopback(ipv6: ipv6, port: port) { address, length in
            Darwin.connect(socket, address, length) == 0
        }
    }

    /// Calls `body` with the loopback address at `port`.
    private static func withLoopback(ipv6: Bool, port: UInt16,
                                     _ body: (UnsafePointer<sockaddr>, socklen_t) -> Bool) -> Bool {
        if ipv6 {
            var address = sockaddr_in6()
            address.sin6_len = UInt8(MemoryLayout<sockaddr_in6>.size)
            address.sin6_family = sa_family_t(AF_INET6)
            address.sin6_addr = in6addr_loopback
            address.sin6_port = port.bigEndian
            return withUnsafePointer(to: &address) { pointer in
                pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { raw in
                    body(raw, socklen_t(MemoryLayout<sockaddr_in6>.size))
                }
            }
        }
        var address = sockaddr_in()
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        address.sin_family = sa_family_t(AF_INET)
        address.sin_addr.s_addr = inet_addr("127.0.0.1")
        address.sin_port = port.bigEndian
        return withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { raw in
                body(raw, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
    }
}

#endif
