// NereusSDR for iOS: the CFC band editor's reads and sends through cfc.setProfile
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// What the CFC editor draws now: the Core's description of it, the
/// profile (the phone's held edit, or the Core's), why it cannot change,
/// and the Core's last refusal in its own words.
public struct SetupCfcPanel: Equatable, Sendable {
    public let editor: SetupDescription.CfcEditor?
    public let profile: CfcProfile?
    /// Nil while the editor can change the profile.
    public let reason: String?
    /// The Core's refusal of the last send, or why its answer did not come.
    public let problem: String?
    /// A send is waiting for the Core's answer.
    public let sending: Bool

    public var editable: Bool { reason == nil && profile != nil }
}

/// The CFC editor's held edit and its send (desktop TxCfcDialog): one send
/// at a time, the newest edit held while one is out, sent after a pause
/// in editing rather than on every step.
struct CfcEditorState {
    /// Bumped when the session ends: a late answer from before is ignored.
    var epoch: UInt64 = 0
    /// Each input edit owns its completion independently of the page note.
    var edit: UInt64 = 0
    /// The phone's edit, shown until the Core has taken or refused it.
    var draft: CfcProfile?
    var sending = false
    /// Changed again while a send was out: sent once it is answered.
    var editedSinceSend = false
    var problem: String?
    var pause: Task<Void, Never>?
}

public extension SetupControlDispatcher {
    /// The Core's refusal when it cannot take cfc.setProfile
    /// (StationServer.cpp, the transmit-settings version check).
    nonisolated static let cfcNoTransmitSettingsReason = "This Core cannot change its transmit settings."
    nonisolated static let cfcProfileMissingReason = "The Core has not sent the CFC settings."

    /// The CFC editor as it stands now.
    func cfcPanel(_ control: SetupDescription.Control, in category: String) -> SetupCfcPanel {
        guard case .cfcProfile(let editor)? = control.binding else {
            return SetupCfcPanel(editor: nil, profile: nil, reason: Self.unreadableReason, problem: nil, sending: false)
        }
        let current = coreCfcProfile(editor)
        var reason = specializedReason(control, in: category)
        if reason == Self.needsNewerCoreReason {
            reason = cfcGateReason(control)
        }
        if reason == nil, current == nil {
            reason = Self.cfcProfileMissingReason
        }
        let shown = reason == nil ? (cfc.draft ?? current) : current
        return SetupCfcPanel(editor: editor, profile: shown, reason: reason, problem: cfc.problem,
                             sending: cfc.sending)
    }

    /// Changes the editor's profile on the phone, and sends the whole of it
    /// to the Core after a pause in editing.
    func editCfc(_ control: SetupDescription.Control, in category: String,
                 _ change: (inout CfcProfile) -> Void) {
        let panel = cfcPanel(control, in: category)
        guard panel.reason == nil, var profile = panel.profile else {
            return
        }
        change(&profile)
        cfc.edit &+= 1
        cfc.draft = profile
        cfc.problem = nil
        if cfc.sending {
            cfc.editedSinceSend = true
        }
        changed()
        cfc.pause?.cancel()
        let epoch = cfc.epoch
        let pause = cfcSendPause
        let clock = cfcClock
        cfc.pause = Task { @MainActor [weak self] in
            try? await clock.sleep(for: pause)
            guard !Task.isCancelled, let self, self.cfc.epoch == epoch, !self.cfc.sending else {
                return
            }
            // The pause is over: a later edit no longer cancels this task,
            // which now carries the send and waits for its answer.
            self.cfc.pause = nil
            await self.sendCfc(control, in: category)
        }
    }

    /// Why the editor's gate is closed, in the Core's words; nil when open.
    internal func cfcGateReason(_ control: SetupDescription.Control) -> String? {
        guard let reason = gateReason(control.gate) else {
            return nil
        }
        return reason == Self.needsNewerCoreReason ? Self.cfcNoTransmitSettingsReason : reason
    }
}

extension SetupControlDispatcher {
    /// The profile the Core holds now; nil when it has sent none that reads.
    func coreCfcProfile(_ editor: SetupDescription.CfcEditor) -> CfcProfile? {
        watch(editor.property.object)
        guard case .text(let json)? = store.object(editor.property.object)?[editor.property.name] else {
            return nil
        }
        return CfcProfile(json: json)
    }

    /// Sends the held edit, with the revision of the profile it was made
    /// from (setup description 19: "that value's `revision` as
    /// `expectedRevision`"), never the Core's newer one, so the Core refuses
    /// it rather than overwrite a change the phone has not shown. Taken: the
    /// editor follows the Core's profile again, or sends the edit made
    /// meanwhile, which now follows on from the profile this send left on
    /// the Core. Refused: the Core's words, and the Core's profile back. No
    /// answer: the Core may or may not have it, so the editor follows the
    /// Core's next profile.
    func sendCfc(_ control: SetupDescription.Control, in category: String) async {
        guard !cfc.sending, let draft = cfc.draft, case .cfcProfile(let editor)? = control.binding else {
            return
        }
        let panel = cfcPanel(control, in: category)
        if let reason = panel.reason {
            cfc.problem = reason
            cfc.draft = nil
            changed()
            return
        }
        guard let current = coreCfcProfile(editor) else {
            return
        }
        if draft.sameValues(as: current) {
            cfc.draft = nil
            changed()
            return
        }
        guard let sender = captureSender() else {
            cfc.problem = Self.notConnectedReason
            changed()
            return
        }
        let admission = admitSpecial(control, in: category, sender: sender, special: SetupAdmission.Special())
        let epoch = cfc.epoch
        let edit = cfc.edit
        cfc.sending = true
        cfc.editedSinceSend = false
        changed()
        let arguments = [CommandArgument(name: CfcProfile.profileArgumentName, value: .text(draft.profileJson)),
                         CommandArgument(name: CfcProfile.revisionArgumentName, value: .text(draft.revision))]
        let sent = await send(admission, verb: editor.verb, arguments: arguments, onLateOutcome: { [weak self] outcome in
            guard let self, self.cfc.epoch == epoch, self.cfc.edit == edit else { return }
            switch outcome {
            case .applied: self.cfc.problem = nil
            case .awaitingConfirmation: return
            case .refused(let reason), .notSent(let reason): self.cfc.problem = reason
            }
            self.changed()
        })
        admissions.removeAll { $0 === admission }
        guard cfc.epoch == epoch else {
            return
        }
        cfc.sending = false
        if cfc.edit != edit, sent.outcome != .applied {
            // A later unsubmitted edit is neither replayed nor retired by
            // this older answer/deadline. Its own pause/send remains the owner.
            changed()
            return
        }
        switch sent.outcome {
        case .applied:
            cfc.problem = nil
            if cfc.editedSinceSend {
                cfc.editedSinceSend = false
                // The accepted answer carries the profile the Core now holds,
                // which may come before its transmit delta.
                if case .text(let json)? = sent.result?.values[CfcProfile.returnedProfileName],
                   let returned = CfcProfile(json: json) {
                    cfc.draft?.revision = returned.revision
                } else if let latest = coreCfcProfile(editor) {
                    cfc.draft?.revision = latest.revision
                }
                changed()
                await sendCfc(control, in: category)
                return
            }
            cfc.draft = nil
        case .awaitingConfirmation:
            // Held for the Core's question, which carries it on: nothing
            // to show as a refusal, and the editor follows the Core's
            // profile, which the answer to the question settles.
            cfc.problem = nil
            cfc.draft = nil
            cfc.editedSinceSend = false
        case .refused(let reason):
            cfc.problem = reason
            cfc.draft = nil
            cfc.editedSinceSend = false
        case .notSent(let reason):
            cfc.problem = reason
            if reason == Self.linkLostReason || reason == Self.noAnswerReason {
                cfc.draft = nil
                cfc.editedSinceSend = false
            }
        }
        changed()
    }

    /// The session ended: nothing held, nothing waiting.
    func resetCfc() {
        cfc.pause?.cancel()
        cfc = CfcEditorState(epoch: cfc.epoch &+ 1)
        changed()
    }
}
