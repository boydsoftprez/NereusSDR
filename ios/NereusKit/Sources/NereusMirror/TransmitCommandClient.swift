// NereusSDR for iOS: the PTT's commands, sent through the command client
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-28: CW v2 lease, heartbeat and retirement integration.
//   J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import NereusLink

/// ``PttController``'s way to the Core: each keying verb three times under
/// one id through ``CommandClient``, and the keepalive sent once without
/// waiting (link document sections 18.6 and 18.7): on the media
/// connection's "tx" data channel while it is open, otherwise posted on
/// the session, never both.
public struct TransmitCommandClient: TransmitCommandSending {
    /// How long a keying verb waits for the Core's answer. A voice key may
    /// wait 250 ms for the microphone's buffer (section 18.6), and the
    /// unkey-confirmed gate up to 2 s more.
    public static let answerTimeout: Duration = .seconds(5)

    private let commands: CommandClient
    private let diagnostics: LinkDiagnostics
    // Legacy ungated calls have no PTT activity boundary. Their diagnostic
    // activity is this adapter's lifetime; struct copies share the identity.
    // This gate is only an identity and never controls an ungated send.
    private let ungatedDiagnosticActivity = TransmitHeartbeatGate()
    private let keepaliveOnMedia: (@Sendable (Int64, Int64) -> Bool)?
    private let keepaliveOnMediaV2: (@Sendable (Int64, Int64, Int64) -> Bool)?
    private let remoteCwV2: @Sendable () async -> Bool

    /// `keepaliveOnMedia` sends one keepalive on the "tx" data channel and
    /// says whether it went; nil sends every keepalive on the session.
    public init(commands: CommandClient, keepaliveOnMedia: (@Sendable (Int64, Int64) -> Bool)? = nil,
                diagnostics: LinkDiagnostics = .shared,
                keepaliveOnMediaV2: (@Sendable (Int64, Int64, Int64) -> Bool)? = nil,
                remoteCwV2: @escaping @Sendable () async -> Bool = { false }) {
        self.commands = commands
        self.keepaliveOnMedia = keepaliveOnMedia
        self.diagnostics = diagnostics
        self.keepaliveOnMediaV2 = keepaliveOnMediaV2
        self.remoteCwV2 = remoteCwV2
    }

    public func send(_ verb: TransmitVerb, copies: Int) async -> TransmitPending {
        await send(verb, copies: copies, authority: CommandSendPermit())
    }

    public func send(_ verb: TransmitVerb, copies: Int, authority: CommandSendPermit) async -> TransmitPending {
        let arguments: [CommandArgument]
        switch verb {
        case .key(let trigger):
            arguments = [CommandArgument(name: "trigger", value: .text(trigger))]
        case .unkey(let epoch):
            arguments = [CommandArgument(name: "epoch", value: .int(epoch))]
        case .tune(let on), .twoTone(let on), .tunerTune(let on):
            arguments = [CommandArgument(name: "on", value: .bool(on))]
        }
        let started = await commands.start(verb.name, arguments: arguments, copies: copies,
                                           timeout: Self.answerTimeout, authority: authority)
        await started.sent()
        return TransmitPending {
            do {
                return Self.answer(try await started.result())
            } catch {
                return .noAnswer
            }
        }
    }

    public func sendKeepalive(sequence: Int64, epoch: Int64) async {
        if keepaliveOnMedia?(sequence, epoch) == true {
            diagnostics.successfulKeepaliveHandoff(activity: ungatedDiagnosticActivity)
            return
        }
        try? await commands.post("tx.keepalive", arguments: [CommandArgument(name: "sequence", value: .int(sequence)),
                                                            CommandArgument(name: "epoch", value: .int(epoch))],
            onSuccessfulHandoff: { diagnostics.successfulKeepaliveHandoff(activity: ungatedDiagnosticActivity) })
    }

    public func sendKeepalive(sequence: Int64, epoch: Int64,
                              gate: TransmitHeartbeatGate,
                              stillAllowed: @escaping @Sendable () async -> Bool) async {
        guard await stillAllowed() else { return }
        if gate.handoff({ keepaliveOnMedia?(sequence, epoch) == true }) {
            diagnostics.successfulKeepaliveHandoff(activity: gate)
            return
        }
        guard await stillAllowed() else { return }
        try? await commands.post("tx.keepalive", arguments: [
            CommandArgument(name: "sequence", value: .int(sequence)),
            CommandArgument(name: "epoch", value: .int(epoch)),
        ], gate: gate, stillAllowed: stillAllowed,
           onSuccessfulHandoff: { diagnostics.successfulKeepaliveHandoff(activity: gate) })
    }

    /// v2 fans one record out over every existing open path. A successful
    /// media handoff does not suppress the primary session's keepalive.
    public func sendKeepalive(sequence: Int64, epoch: Int64, cwEpoch: Int64,
                              gate: TransmitHeartbeatGate,
                              stillAllowed: @escaping @Sendable () async -> Bool) async {
        guard await remoteCwV2() else {
            await sendKeepalive(sequence: sequence, epoch: epoch, gate: gate, stillAllowed: stillAllowed)
            return
        }
        guard await stillAllowed() else { return }
        if gate.handoff({ keepaliveOnMediaV2?(sequence, epoch, cwEpoch) == true }) {
            diagnostics.successfulKeepaliveHandoff(activity: gate)
        }
        guard await stillAllowed() else { return }
        try? await commands.post("tx.keepalive", arguments: [
            CommandArgument(name: "sequence", value: .int(sequence)),
            CommandArgument(name: "epoch", value: .int(epoch)),
            CommandArgument(name: "cwEpoch", value: .int(cwEpoch)),
        ], gate: gate, stillAllowed: stillAllowed,
           onSuccessfulHandoff: { diagnostics.successfulKeepaliveHandoff(activity: gate) })
    }

    /// A keying verb's `command.result` as the PTT reads it.
    public static func answer(_ result: CommandResult) -> TransmitAnswer {
        if result.accepted {
            if case .int(let epoch)? = result.values["epoch"] {
                return .accepted(epoch: epoch)
            }
            return .accepted(epoch: nil)
        }
        var code = ""
        var fix = ""
        if case .text(let text)? = result.values["refusalCode"] {
            code = text
        }
        if case .text(let text)? = result.values["refusalFix"] {
            fix = text
        }
        return .refused(TxRefusalInfo(reason: result.reason, code: code, fix: fix))
    }
}
