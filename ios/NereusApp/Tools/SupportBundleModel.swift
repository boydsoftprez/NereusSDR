// NereusSDR for iOS: the support bundle: this phone's log and the Core's bundle, written as files for the share sheet
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror
import os
import UIKit

/// The Support Bundle page's model (spec section 5.2 item 4, R-IOS-18):
/// this phone's own log and the Core's support bundle (`support.collect`,
/// link document section 9.1, at `supportBundleVersion` 1: a ZIP the Core
/// writes without keys, tokens or codes), written as two files and handed to
/// the share sheet. The app sends them nowhere itself; the operator picks
/// where they go. Without the Core, or from one that makes no bundle, the
/// phone's log goes alone and the page says why.
///
/// It lives on ``AppModel``: the Tools tab is rebuilt on every tab switch,
/// and the Core may take a few seconds to make its bundle.
@MainActor
final class SupportBundleModel: ObservableObject {
    enum State: Equatable {
        case idle
        case collecting
        case ready
    }

    static let capability = "supportBundleVersion"
    static let minor: UInt16 = 11
    static let verb = "support.collect"
    /// The Core writes its bundle on a worker thread and answers later.
    static let timeout: Duration = .seconds(120)

    // MARK: Words

    static let notConnectedReason = "The Core is not connected, so only this phone's log is included."
    static let olderCoreReason = "This Core does not make a support bundle. Updating the Core may help."
    static let noAnswerReason = "The Core did not send its bundle. Try again in a moment."
    static let unreadableReason = "The Core's bundle could not be read. Try again in a moment."
    static let phoneLogUnreadable = "This phone's log could not be read; the file says so."
    static let nothingSentNote =
        "Nothing is sent by this app. Collect the files, then choose where they go in the share sheet."

    // MARK: State

    @Published private(set) var state = State.idle
    /// The files to share, this phone's log first.
    @Published private(set) var files: [URL] = []
    /// Why the Core's bundle will not be included, or nil when it will.
    @Published private(set) var coreReason: String?
    /// Why the last collection has no Core bundle, in the Core's words when it gave them.
    @Published private(set) var coreNote: String?
    /// The phone's log could not be read on the last collection.
    @Published private(set) var phoneLogFailed = false

    private static let logger = Logger(subsystem: "NereusSDR", category: "tools.support")

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let phoneLog: PhoneLog
    private let measurements: MeasurementLog?
    private let directory: URL
    private let now: () -> Date
    private var watch: ToolMirrorWatch?

    init(mirror: MirrorStore, commands: CommandClient?, phoneLog: PhoneLog = .system,
         measurements: MeasurementLog? = nil,
         directory: URL = FileManager.default.temporaryDirectory.appendingPathComponent("SupportBundle",
                                                                                          isDirectory: true),
         now: @escaping () -> Date = Date.init) {
        self.mirror = mirror
        self.commands = commands
        self.phoneLog = phoneLog
        self.measurements = measurements
        self.directory = directory
        self.now = now
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        self.watch = watch
        refresh()
    }

    func refresh() {
        let next: String?
        if !mirror.isSnapshotComplete || mirror.isStale {
            next = Self.notConnectedReason
        } else if (mirror.agreedMinor ?? 0) < Self.minor || mirror.capabilityVersion(Self.capability) < 1 {
            next = Self.olderCoreReason
        } else {
            next = nil
        }
        if coreReason != next {
            coreReason = next
        }
    }

    /// Collects this phone's log and, when the Core makes one, its bundle,
    /// and writes both as files. Nothing leaves the phone.
    func collect() async {
        guard state != .collecting else {
            return
        }
        state = .collecting
        coreNote = nil
        let stamp = Self.stamp(now())
        try? FileManager.default.removeItem(at: directory)
        do {
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        } catch {
            Self.logger.warning("The support bundle's folder could not be made")
        }
        var written: [URL] = []
        let phone = directory.appendingPathComponent("NereusSDR-phone-log-\(stamp).txt")
        if (try? Data(phoneLogText().utf8).write(to: phone, options: .atomic)) != nil {
            written.append(phone)
        }
        if coreReason == nil, let commands {
            do {
                let result = try await commands.invoke(Self.verb, arguments: [], timeout: Self.timeout)
                if result.accepted {
                    if case .text(let text)? = result.values["bundle"],
                       let data = Data(base64Encoded: text, options: .ignoreUnknownCharacters), !data.isEmpty {
                        let core = directory.appendingPathComponent("NereusSDR-Core-support-\(stamp).zip")
                        try data.write(to: core, options: .atomic)
                        written.append(core)
                    } else {
                        coreNote = Self.unreadableReason
                    }
                } else {
                    coreNote = result.reason.isEmpty ? Self.noAnswerReason : result.reason
                }
            } catch {
                Self.logger.info("The Core's support bundle did not arrive")
                coreNote = Self.noAnswerReason
            }
        } else {
            coreNote = coreReason
        }
        files = written
        state = .ready
    }

    /// The phone's log file: what this app and this phone are, the
    /// measurement log (plan Task 68) when there is one, then the lines.
    private func phoneLogText() -> String {
        let info = Bundle.main.infoDictionary
        let version = info?["CFBundleShortVersionString"] as? String ?? "--"
        let build = info?["CFBundleVersion"] as? String ?? "--"
        var header = [
            "NereusSDR for iPhone and iPad",
            "Version \(version) (\(build))",
        ]
        if let tag = BuildTag.current {
            header.append(BuildTag.label(for: tag))
        }
        header.append("\(UIDevice.current.systemName) \(UIDevice.current.systemVersion), \(UIDevice.current.model)")
        header.append("Collected \(now().formatted(Date.ISO8601FormatStyle()))")
        header.append("")
        if let measurements {
            header.append(contentsOf: measurements.bundleLines())
            header.append("")
        }
        var body: [String]
        do {
            body = try phoneLog.read()
            phoneLogFailed = false
        } catch {
            body = [Self.phoneLogUnreadable]
            phoneLogFailed = true
        }
        return (header + body).joined(separator: "\n") + "\n"
    }

    /// A file name's time: `20260928-183400`, in UTC.
    static func stamp(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(identifier: "UTC")
        formatter.dateFormat = "yyyyMMdd-HHmmss"
        return formatter.string(from: date)
    }
}
