// NereusSDR for iOS: CWX on Tools: text and the Core's eight memories sent as CW by the Core's sender, and Stop
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-26: CW transmit, the CWX page's model: cw.send, cw.memory and
//   cw.stop at remoteCwVersion 1 (R-CW-50). J.J. Boyd (KG4VCF), AI-assisted
//   via Claude Code.

import Foundation
import NereusLink
import NereusMirror
import os

/// The CWX page's model (R-CW-50, C13): the phone hands the Core text or a
/// memory number and the Core's CWX sender keys the radio on its own timer
/// (link document section 18.10). The phone never keys: Send queues
/// `cw.send {text}`, a memory `cw.memory {number}`, and Stop `cw.stop {}`.
/// Each request goes three times under one id, as the desktop's remote
/// window sends it, and the Core acts once.
///
/// A Core without `remoteCwVersion` 1 (at the agreed minor 11) leaves every
/// control shown and greyed with the reason. The Core refuses a send while
/// another device holds transmit, or while nobody does, in its own words;
/// those show on the page, with Take transmit when the Core offers it.
@MainActor
final class CwxModel: ObservableObject {
    // MARK: The link

    static let capability = "remoteCwVersion"
    static let version: Int64 = 1
    static let minor: UInt16 = 11
    static let sendVerb = "cw.send"
    static let memoryVerb = "cw.memory"
    static let stopVerb = "cw.stop"
    /// Each request's copies under one id (link document section 18.6).
    static let copies = 3
    static let timeout: Duration = .seconds(5)
    /// The Core's memories, `cwxMemory1` to `cwxMemory8` on `transmit`.
    static let memoryNumbers = Array(1...8)
    static func memoryProperty(_ number: Int) -> String { "cwxMemory\(number)" }

    // MARK: Words

    static let notConnectedReason = SpotsModel.notConnectedReason
    static let olderCoreReason = "This Core can't send CW from a phone. Updating the Core may help."
    static let noTextReason = "Type the text to send first."
    static let noAnswerText = "The Core did not answer. Try again."
    static let atCoreNote =
        "The Core keys the radio and sends the CW at its own CWX speed. Stop ends it and clears what is left."
    static func emptyMemoryReason(_ number: Int) -> String { "Memory \(number) is empty." }

    // MARK: State

    /// The text to send, as typed.
    @Published var text = ""
    /// Why Send, the memories and Stop cannot be used now; nil when they can.
    @Published private(set) var reason: String?
    /// Each memory's text, as the Core sends it; nil where it sends none.
    @Published private(set) var memories: [String?] = Array(repeating: nil, count: 8)
    /// The Core's words for the last request it refused, with its code and fix.
    @Published private(set) var refusal: TxRefusalInfo?

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private var watch: ToolMirrorWatch?
    private static let logger = Logger(subsystem: "NereusSDR", category: "tools.cwx")

    init(mirror: MirrorStore, commands: CommandClient?) {
        self.mirror = mirror
        self.commands = commands
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        self.watch = watch
        refresh()
    }

    /// The Core takes the CW verbs from this phone.
    var offered: Bool {
        (mirror.agreedMinor ?? 0) >= Self.minor && mirror.capabilityVersion(Self.capability) >= Self.version
    }

    func refresh() {
        let transmit = watch?.object(TransmitModel.transmitKey)
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        let nextReason: String? = !connected ? Self.notConnectedReason : !offered ? Self.olderCoreReason : nil
        if reason != nextReason {
            reason = nextReason
        }
        let nextMemories = Self.memoryNumbers.map { ToolValue.text(transmit?[Self.memoryProperty($0)]) }
        if memories != nextMemories {
            memories = nextMemories
        }
    }

    /// Why Send cannot be pressed now, or nil when it can.
    var sendReason: String? {
        reason ?? (text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? Self.noTextReason : nil)
    }

    /// Why memory `number` cannot be sent now, or nil when it can.
    func memoryReason(_ number: Int) -> String? {
        if let reason {
            return reason
        }
        guard Self.memoryNumbers.contains(number) else {
            return nil
        }
        return memories[number - 1] == "" ? Self.emptyMemoryReason(number) : nil
    }

    /// Queues the typed text at the Core's sender.
    func send() {
        guard sendReason == nil else {
            return
        }
        invoke(Self.sendVerb, [CommandArgument(name: "text", value: .text(text))])
    }

    /// Queues memory `number` (1 to 8) at the Core's sender.
    func sendMemory(_ number: Int) {
        guard Self.memoryNumbers.contains(number), memoryReason(number) == nil else {
            return
        }
        invoke(Self.memoryVerb, [CommandArgument(name: "number", value: .int(Int64(number)))])
    }

    /// Ends what the Core's sender runs for this phone and clears it.
    func stop() {
        guard reason == nil else {
            return
        }
        invoke(Self.stopVerb, [])
    }

    /// A take of transmit started from the refusal: its words close.
    func takeStarted() {
        refusal = nil
    }

    private func invoke(_ verb: String, _ arguments: [CommandArgument]) {
        guard let commands else {
            refusal = TxRefusalInfo(reason: Self.noAnswerText)
            return
        }
        Task { [weak self] in
            do {
                let result = try await commands.invoke(verb, arguments: arguments, copies: Self.copies,
                                                       timeout: Self.timeout)
                self?.note(result)
            } catch {
                Self.logger.info("A CWX request had no answer from the Core")
                self?.refusal = TxRefusalInfo(reason: Self.noAnswerText)
            }
        }
    }

    private func note(_ result: CommandResult) {
        if result.accepted {
            refusal = nil
        } else {
            refusal = TxRefusalInfo(reason: result.reason, code: ToolValue.text(result.values["refusalCode"]) ?? "",
                                    fix: ToolValue.text(result.values["refusalFix"]) ?? "")
        }
    }
}
