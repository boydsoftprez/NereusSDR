// NereusSDR for iOS: a control's property writes, one at a time per property, the newest value waiting its turn
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusLink

/// The writes a screen's controls make to the Core's properties. Writes to
/// one property go one at a time, each after the Core has answered the one
/// before; a value that arrives meanwhile replaces any other still waiting,
/// so a slider's last position is the one the Core keeps. Every value shows
/// at the touch (``MirrorStore/hold(_:property:value:)``) and stays until
/// the Core answers, as the desktop's remote window keeps the operator's
/// value (`StationClient.cpp:1040-1068`). Each outcome goes to the owner,
/// a late one too (``MirrorStore/lateAnswers``). After a lost link a value
/// still waiting is dropped, never sent again by itself.
@MainActor
public final class PropertyWriteQueue {
    public typealias OnOutcome = @MainActor (_ property: String, _ outcome: PropertyWriteOutcome) -> Void

    private let store: MirrorStore
    private let onOutcome: OnOutcome
    private var writers: [String: Task<Void, Never>] = [:]
    private struct Binding {
        let sender: MirrorStore.BoundSender
        let authority: CommandSendPermit
        let stillAllowed: @MainActor () -> Bool
    }
    private typealias Request = (value: MirrorValue, edit: UInt64?, touch: UInt64, binding: Binding?)
    private var queued: [String: Request] = [:]
    private var nextTouch: UInt64 = 0
    private var currentTouches: [String: UInt64] = [:]
    public init(store: MirrorStore, onOutcome: @escaping OnOutcome) {
        self.store = store
        self.onOutcome = onOutcome
    }

    /// True while a write of `property` to `key` waits for its answer.
    public func isWriting(_ key: String, _ property: String) -> Bool {
        writers[Self.slot(key, property)] != nil
    }

    /// Writes `value` to one property of one object, shown at once.
    public func write(_ key: String, _ property: String, _ value: MirrorValue) {
        enqueue(key, property, value, binding: nil)
    }

    /// Captures session and participant authority at the touch, including a value waiting behind another write.
    @discardableResult
    public func writeBound(_ key: String, _ property: String, _ value: MirrorValue,
                           sender: @escaping MirrorStore.BoundSender, authority: CommandSendPermit,
                           stillAllowed: @escaping @MainActor () -> Bool) -> UInt64? {
        enqueue(key, property, value, binding: Binding(sender: sender, authority: authority, stillAllowed: stillAllowed))
    }

    @discardableResult
    private func enqueue(_ key: String, _ property: String, _ value: MirrorValue, binding: Binding?) -> UInt64? {
        let slot = Self.slot(key, property)
        let edit = store.hold(key, property: property, value: value)
        nextTouch &+= 1
        let touch = nextTouch
        currentTouches[slot] = touch
        guard writers[slot] == nil else {
            queued[slot] = (value, edit, touch, binding)
            return edit
        }
        let store = store
        writers[slot] = Task { [weak self] in
            var next: Request? = (value, edit, touch, binding)
            while let current = next {
                let late: @MainActor (PropertyWriteOutcome) -> Void = { [weak self] outcome in
                    guard let self, self.currentTouches[slot] == current.touch,
                          outcome.isCurrent, !outcome.heldForQuestion else { return }
                    self.onOutcome(property, outcome)
                }
                let outcome: PropertyWriteOutcome
                if let binding = current.binding {
                    if binding.authority.isRevoked || !binding.stillAllowed() {
                        if let edit = current.edit { store.retireBoundEdit(key, property: property, edit: edit) }
                        outcome = .notSent
                    } else {
                        outcome = await store.writeBound(key, property: property, value: current.value,
                            sender: binding.sender, authority: binding.authority, edit: current.edit,
                            expiresAuthorityAtDeadline: true, onLateOutcome: late)
                    }
                } else {
                    outcome = await store.write(key, property: property, value: current.value,
                                                edit: current.edit, onLateOutcome: late)
                }
                guard let self else {
                    return
                }
                if !outcome.answeredByCore, outcome.reason == PropertyWriteOutcome.linkLost.reason,
                   let waiting = self.queued[slot],
                   !(waiting.edit.map { store.isCurrent(key, property: property, edit: $0) } ?? false) {
                    // A touch retired with the link is never replayed. A new
                    // deliberate touch after reconnect has its own live edit.
                    self.queued[slot] = nil
                    if self.currentTouches[slot] == waiting.touch {
                        self.onOutcome(property, .linkLost)
                    }
                }
                if outcome.isCurrent && self.currentTouches[slot] == current.touch && !outcome.heldForQuestion {
                    self.onOutcome(property, outcome)
                }
                next = self.queued.removeValue(forKey: slot)
            }
            self?.writers[slot] = nil
        }
        return edit
    }

    private static func slot(_ key: String, _ property: String) -> String {
        key + "." + property
    }
}
