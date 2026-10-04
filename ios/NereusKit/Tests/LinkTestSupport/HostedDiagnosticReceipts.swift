// NereusSDR: passive, bounded receipts for the isolated hosted Kit diagnosis
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
#if canImport(Darwin)
import Darwin
#endif

/// Capture has no buffer lock or callback. Append may follow a phase unlock;
/// occurrence and append time/order remain distinct. Export follows assertions.
public final class HostedDiagnosticReceipts: @unchecked Sendable {
    public struct Captured: Sendable {
        fileprivate let uptime: Double
        fileprivate let thread: UInt64
        fileprivate let main: Bool
        fileprivate let phase: String?
    }
    private struct Entry {
        let captured: Captured
        let appended: Double
    }
    public let name: String
    private let lock = NSLock()
    private var entries: [Entry] = []
    private var dropped = 0
    private let capacity = 512
    private static let nameBytes = 128
    private static let phaseBytes = 256

    public init(_ name: String) {
        if name.utf8.count <= Self.nameBytes { self.name = name }
        else { self.name = "invalid oversized name"; dropped = 1 }
    }

    public static func capture(_ phase: String) -> Captured {
        let uptime = ProcessInfo.processInfo.systemUptime
        #if canImport(Darwin)
        let thread = UInt64(pthread_mach_thread_np(pthread_self()))
        #else
        let thread: UInt64 = 0
        #endif
        return Captured(uptime: uptime, thread: thread, main: Thread.isMainThread,
                        phase: phase.utf8.count <= phaseBytes ? phase : nil)
    }

    public func append(_ captured: Captured) {
        lock.withLock {
            if entries.count < capacity && captured.phase != nil {
                entries.append(Entry(captured: captured, appended: ProcessInfo.processInfo.systemUptime))
            } else { dropped += 1 }
        }
    }

    public func mark(_ phase: String) { append(Self.capture(phase)) }

    /// Oversized names/labels and record overflow count as explicit diagnostic
    /// failures, evaluated by the harness; they never alter a phase disposition.
    public func export() {
        guard let directory = ProcessInfo.processInfo.environment["KIT_HOSTED_RECEIPTS_DIR"] else { return }
        let wallBefore = Date().timeIntervalSince1970
        let exported = ProcessInfo.processInfo.systemUptime
        let wallAfter = Date().timeIntervalSince1970
        let snapshot = lock.withLock { (entries, dropped) }
        let records: [[String: Any]] = snapshot.0.enumerated().map { index, entry in
            ["sequence": index, "uptime": entry.captured.uptime, "append_uptime": entry.appended,
             "thread": entry.captured.thread, "main_thread": entry.captured.main,
             "phase": entry.captured.phase!]
        }
        do {
            let data = try JSONSerialization.data(withJSONObject: ["name": name, "export_uptime": exported,
                 "export_wall_unix": (wallBefore + wallAfter) / 2,
                 "export_wall_before_unix": wallBefore, "export_wall_after_unix": wallAfter,
                 "record_capacity": capacity, "name_utf8_capacity": Self.nameBytes,
                 "phase_utf8_capacity": Self.phaseBytes,
                 "dropped": snapshot.1, "entries": records], options: [.sortedKeys])
            let file = URL(fileURLWithPath: directory).appendingPathComponent(UUID().uuidString + ".json")
            try data.write(to: file, options: .atomic)
        } catch {
            print("KIT HOSTED DIAGNOSTIC EXPORT FAILED")
        }
    }
}
