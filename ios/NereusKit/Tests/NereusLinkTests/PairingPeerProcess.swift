// NereusSDR for iOS: runs the station's pairing peer and carries its lines as a pairing connection
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process, which runs the station's helper, exists only on
// macOS; on the iOS simulator the interop tests are not built.
#if os(macOS)

import Foundation
import LinkSessionTestSupport
import NereusLink

/// The station-side helper `nereus_pairing_peer` (tests/tools), the Core's
/// side of one pairing over standard input and output, one message per line
/// (link document section 3.6). It is found through `NEREUS_PAIRING_PEER`,
/// which `ios/scripts/interop-test.sh` sets after building it.
///
/// As a `LinkTransport` it is the pairing connection: its first line,
/// `peer.ready`, gives the scratch Core's code and its certificate's
/// SHA-256, which the connection reports as the certificate it presented;
/// every Core message after it is a text frame, and `peer.done` is the
/// close. Closing the connection ends standard input, which ends the Core's
/// connection. The code is kept for the test to use and never printed.
final class PairingPeerProcess: LinkTransport, @unchecked Sendable {
    struct Ready: Sendable {
        let code: String
        let certificateSHA256: Data
    }

    struct Done: Sendable, Equatable {
        let paired: Bool
        let devices: Int
    }

    struct Failure: Error, CustomStringConvertible {
        let description: String
    }

    /// The helper's path, or nil when the interop tests are not asked for.
    static var path: String? {
        ProcessInfo.processInfo.environment["NEREUS_PAIRING_PEER"].flatMap { $0.isEmpty ? nil : $0 }
    }

    private let process = Process()
    private let input = Pipe()
    private let output = Pipe()
    private let events: AsyncStream<LinkTransportEvent>
    private let sink: AsyncStream<LinkTransportEvent>.Continuation
    private let waiters = ConditionWaiters()

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var buffer = Data()
    private var readyLine: Ready?
    private var doneLine: Done?
    private var station: [LinkMessage] = []
    private var inputClosed = false

    /// Starts the helper with `arguments` (`--lan-deny`, `--address <ip>`,
    /// `--claimed`).
    init(arguments: [String] = []) throws {
        guard let path = Self.path else {
            throw Failure(description: "NEREUS_PAIRING_PEER is not set")
        }
        (events, sink) = AsyncStream.makeStream(of: LinkTransportEvent.self)
        process.executableURL = URL(fileURLWithPath: path)
        process.arguments = arguments
        process.standardInput = input
        process.standardOutput = output
        process.standardError = FileHandle.nullDevice
        output.fileHandleForReading.readabilityHandler = { [weak self] handle in
            self?.take(handle.availableData)
        }
        try process.run()
    }

    deinit {
        stop()
    }

    /// The `peer.ready` line.
    func ready(within timeout: Duration = .seconds(30)) async throws -> Ready {
        _ = await waiters.wait(within: timeout) { [self] in lock.withLock { readyLine != nil } }
        guard let ready = lock.withLock({ readyLine }) else {
            throw Failure(description: "no peer.ready line within \(timeout)")
        }
        return ready
    }

    /// The `peer.done` line.
    func done(within timeout: Duration = .seconds(30)) async throws -> Done {
        _ = await waiters.wait(within: timeout) { [self] in lock.withLock { doneLine != nil } }
        guard let done = lock.withLock({ doneLine }) else {
            throw Failure(description: "no peer.done line within \(timeout)")
        }
        return done
    }

    /// Every message the Core sent, in order.
    var stationMessages: [LinkMessage] { lock.withLock { station } }

    // MARK: LinkTransport

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let ready = try await ready()
        let stream = events
        Task {
            for await event in stream {
                await onEvent(event)
            }
        }
        return ready.certificateSHA256
    }

    @discardableResult func send(_ text: String) -> Bool {
        lock.withLock {
            guard !inputClosed, process.isRunning else {
                return false
            }
            do {
                try input.fileHandleForWriting.write(contentsOf: Data((text + "\n").utf8))
                return true
            } catch {
                return false
            }
        }
    }

    func ping() {}

    func close() {
        lock.withLock {
            guard !inputClosed else {
                return
            }
            inputClosed = true
            try? input.fileHandleForWriting.close()
        }
    }

    /// Ends the helper and waits for it, for at most five seconds before
    /// terminating it. The wait polls rather than using waitUntilExit(),
    /// which needs a run loop the test's threads do not have.
    func stop() {
        close()
        let deadline = Date().addingTimeInterval(5)
        while process.isRunning && Date() < deadline {
            usleep(10_000)
        }
        if process.isRunning {
            process.terminate()
        }
        output.fileHandleForReading.readabilityHandler = nil
    }

    // MARK: Lines

    private func take(_ data: Data) {
        guard !data.isEmpty else {
            return
        }
        var lines: [Data] = []
        lock.withLock {
            buffer.append(data)
            while let newline = buffer.firstIndex(of: 0x0a) {
                lines.append(Data(buffer[buffer.startIndex..<newline]))
                buffer.removeSubrange(buffer.startIndex...newline)
            }
        }
        for line in lines {
            handle(line)
        }
        waiters.release()
    }

    private func handle(_ line: Data) {
        guard let text = String(data: line, encoding: .utf8),
              case .object(let fields)? = try? LinkJSON.parse(text),
              case .string(let type)? = fields["type"] else {
            return
        }
        switch type {
        case "peer.ready":
            guard case .string(let code)? = fields["code"], case .string(let digest)? = fields["certSha256"],
                  let sha256 = Base64URL.decode(digest) else {
                return
            }
            lock.withLock { readyLine = Ready(code: code, certificateSHA256: sha256) }
        case "peer.done":
            guard case .bool(let paired)? = fields["paired"], case .number(let devices)? = fields["devices"] else {
                return
            }
            lock.withLock { doneLine = Done(paired: paired, devices: Int(devices)) }
            sink.yield(.closed)
        default:
            if let message = try? LinkCodec.decode(text) {
                lock.withLock { station.append(message) }
            }
            sink.yield(.text(text))
        }
    }
}

#endif
