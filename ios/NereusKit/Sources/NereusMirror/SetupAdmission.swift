// NereusSDR for iOS: one admitted edit of a described Setup control, with what it was admitted against
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// An edit the dispatcher admitted when its gesture began: the session's
/// sender, the completed snapshot, the description generation, the
/// settings identity and the selected slice it was admitted against. Its
/// permit is revoked as soon as any of those moves on, and it travels to
/// the final transport handoff, so a stale edit sends nothing.
@MainActor
public final class SetupAdmission {
    public let control: SetupDescription.Control
    public let category: String
    let generation: UInt64
    let mirrorIdentity: UInt64
    let settingsIdentity: UInt64?
    let sender: MirrorStore.BoundSender?
    /// The mirrored object a property binding writes (`slice:active` resolved).
    let objectKey: String?
    /// The selected slice the edit was admitted with, when it needs one.
    let sliceId: Int?
    /// A closed panel's edit: what else it was admitted against.
    let special: Special?
    let permit = CommandSendPermit()

    /// What a closed panel's edit captured beyond a generic control's: the
    /// notch list's revision and row with the selection at the time, the
    /// radio's address, and whether it may be sent only off the air.
    struct Special {
        var notchRevision: Int64?
        var notchRow: Int64?
        /// The selected slice when the edit began, nil when none was.
        var selection: Int?
        var tracksSelection = false
        var radioMac: String?
        var offAir = false
        var hygiene: SetupHygieneAction?
        var pa: PaContext?
    }

    /// PA's revision is a local admission guard, not a wire CAS argument.
    struct PaContext {
        let owner: UUID
        let profiles: PaProfiles
        let capabilities: [String: MirrorValue]
        let transmitState: [String: MirrorValue]?
        let objectIdentity: ObjectIdentifier
        let band: Int?
        let column: String?
    }

    init(control: SetupDescription.Control, category: String, generation: UInt64, mirrorIdentity: UInt64,
         settingsIdentity: UInt64?, sender: MirrorStore.BoundSender?, objectKey: String?, sliceId: Int?,
         special: Special? = nil) {
        self.control = control
        self.category = category
        self.generation = generation
        self.mirrorIdentity = mirrorIdentity
        self.settingsIdentity = settingsIdentity
        self.sender = sender
        self.objectKey = objectKey
        self.sliceId = sliceId
        self.special = special
    }

    /// True once the edit can no longer be sent.
    public var isRevoked: Bool { permit.isRevoked }

    /// Ends the edit before it is sent.
    public func revoke() { permit.revoke() }
}
