// NereusSDR for iOS: runs the station's media offerer helper for the interop tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process, which runs the station's helper, exists only on
// macOS; on the iOS simulator the interop tests are not built.
#if os(macOS)

import Foundation
import LinkTestSupport
import NereusLink

/// The station-side helper `nereus_media_offerer` (tests/tools), which runs
/// the Core's own media transport as the offering peer. It is found through
/// `NEREUS_MEDIA_OFFERER`, which `ios/scripts/interop-test.sh` sets after
/// building it. Signalling and commands travel as JSON lines on its stdin
/// and stdout.
final class OffererProcess: @unchecked Sendable {
    /// One line the helper printed.
    struct Line: Sendable {
        let type: String
        let strings: [String: String]
        let numbers: [String: Int]
        /// The `payload` object of a media-control line.
        let payload: [String: LinkJSON]?
    }

    struct Failure: Error, CustomStringConvertible {
        let description: String
    }

    /// The helper's path, or nil when the interop tests are not asked for.
    static var path: String? {
        ProcessInfo.processInfo.environment["NEREUS_MEDIA_OFFERER"].flatMap { $0.isEmpty ? nil : $0 }
    }

    private let process = Process()
    private let input = Pipe()
    private let output = Pipe()

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var buffer = Data()
    private var lines: [Line] = []

    /// With `mediaControl`, the helper plays the Core's side of media
    /// control (`--media-control`) instead of starting at once.
    init(wrongFingerprint: Bool = false, mediaControl: Bool = false) throws {
        guard let path = Self.path else {
            throw Failure(description: "NEREUS_MEDIA_OFFERER is not set")
        }
        process.executableURL = URL(fileURLWithPath: path)
        let vectors = LinkFixtureLoader.root.appendingPathComponent("media").path
        process.arguments = ["--vectors", vectors] + (wrongFingerprint ? ["--wrong-fingerprint"] : [])
            + (mediaControl ? ["--media-control"] : [])
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

    /// Sends one command line.
    func send(_ object: [String: Any]) {
        guard process.isRunning,
              var line = try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]) else {
            return
        }
        line.append(0x0a)
        try? input.fileHandleForWriting.write(contentsOf: line)
    }

    /// Sends one media control payload, as the session would carry it.
    func sendControl(_ payload: [String: LinkJSON]) {
        guard process.isRunning else {
            return
        }
        let line = LinkJSON.object(["type": .string("media-control"), "payload": .object(payload)]).compactText + "\n"
        try? input.fileHandleForWriting.write(contentsOf: Data(line.utf8))
    }

    /// Every line printed so far, in order.
    var received: [Line] {
        lock.withLock { lines }
    }

    /// Waits until a line of `type` has arrived (the `occurrence`-th, from 1)
    /// and returns it.
    /// With `what`, only lines whose `what` is that, and with `count`,
    /// only those whose `count` has reached it.
    func waitFor(_ type: String, occurrence: Int = 1, timeout: Duration = .seconds(10),
                 what: String? = nil, count: Int? = nil) async throws -> Line {
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        while true {
            let matching = received.filter { line in
                line.type == type && (what == nil || line.strings["what"] == what)
                    && (count.map { (line.numbers["count"] ?? 0) >= $0 } ?? true)
            }
            if matching.count >= occurrence {
                return matching[occurrence - 1]
            }
            guard clock.now < deadline else {
                throw Failure(description: "no \"\(type)\" line from the offerer within \(timeout)")
            }
            try await Task.sleep(for: .milliseconds(10))
        }
    }

    /// Ends the helper and waits for it, for at most five seconds before
    /// terminating it. The wait polls rather than using waitUntilExit(),
    /// which needs a run loop the test's threads do not have.
    func stop() {
        output.fileHandleForReading.readabilityHandler = nil
        guard process.isRunning else {
            return
        }
        send(["type": "stop"])
        try? input.fileHandleForWriting.close()
        let deadline = Date().addingTimeInterval(5)
        while process.isRunning && Date() < deadline {
            usleep(10_000)
        }
        if process.isRunning {
            process.terminate()
        }
    }

    private func take(_ data: Data) {
        guard !data.isEmpty else {
            return
        }
        lock.withLock {
            buffer.append(data)
            while let newline = buffer.firstIndex(of: 0x0a) {
                let line = buffer[buffer.startIndex..<newline]
                buffer.removeSubrange(buffer.startIndex...newline)
                if let parsed = Self.parse(Data(line)) {
                    lines.append(parsed)
                }
            }
        }
    }

    private static func parse(_ line: Data) -> Line? {
        guard let object = (try? JSONSerialization.jsonObject(with: line)) as? [String: Any],
              let type = object["type"] as? String else {
            return nil
        }
        var strings: [String: String] = [:]
        var numbers: [String: Int] = [:]
        for (key, value) in object {
            if let text = value as? String {
                strings[key] = text
            } else if let number = value as? NSNumber {
                numbers[key] = number.intValue
            }
        }
        var payload: [String: LinkJSON]?
        if let raw = object["payload"], case .object(let decoded)? = try? LinkJSON(foundation: raw) {
            payload = decoded
        }
        return Line(type: type, strings: strings, numbers: numbers, payload: payload)
    }
}

#endif
