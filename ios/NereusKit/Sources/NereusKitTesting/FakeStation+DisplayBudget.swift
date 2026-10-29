// NereusSDR for iOS: the fake Core's display budget wire: allocation results and the Core's endpoint-identity rules
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

extension FakeStation.Additions {
    /// The display budget wire, beside ``wideband``'s media:
    /// `remoteDisplayBudgetVersion` 1 and a budget (``FakeStation/budgetBytesPerSecond``,
    /// ``FakeStation/budgetSampleUnitsPerSecond``, generation
    /// ``FakeStation/budgetGeneration``). Each `subscribe` and `unsubscribe`
    /// is answered with an `allocation-result` in place of `rejected`, under
    /// the Core's endpoint-identity rules
    /// (src/core/session/media/DaemonMediaController.cpp:3766-3802 and
    /// :4248-4291): a new endpoint ID at or below the highest the connection
    /// has used is refused, "This display had already been closed.", as is a
    /// closed one asked for again. Not in ``all``.
    public static let displayBudget = FakeStation.Additions(rawValue: 1 << 61)
}

extension FakeStation {
    /// The budget the fake advertises with ``Additions/displayBudget``.
    public static let budgetBytesPerSecond: Int64 = 50_000_000
    public static let budgetSampleUnitsPerSecond: Int64 = 50_000_000
    public static let budgetGeneration: Int64 = 1
    /// What the fake's `allocation-result` says an accepted display holds.
    static let heldBytesPerSecond = 1000
    static let heldSampleUnitsPerSecond = 1000
    static let heldMessagesPerSecond = 30

    /// The Core's words for a display ID it has retired or passed
    /// (src/core/session/media/DaemonMediaController.cpp:3793, :3799).
    public static let displayClosedReason = "This display had already been closed."
    /// The Core's words for a revision no newer than one it has
    /// (src/core/session/media/DaemonMediaController.cpp:3778, :3794).
    public static let newerDisplayRequestReason = "A newer request for this display had already arrived."

    /// One media connection's display identities, as the Core keeps them
    /// (`m_endpoints`, `m_nonliveOperations`, `m_endpointHighWater`), and
    /// forgets them with its peer (`clearAllocationIdentity`, :1876-1881).
    struct BudgetWireState {
        /// Each endpoint the Core holds, with the revision it accepted.
        var live: [UInt32: Record] = [:]
        /// Each endpoint the Core refused or let go, with its last revision.
        var nonlive: [UInt32: Record] = [:]
        var highWater: UInt32 = 0

        struct Record {
            let revision: UInt32
            let request: [String: LinkJSON]
            let accepted: Bool
            let reason: String
            /// Closed by an `unsubscribe`: never asked for again.
            var retired = false
        }
    }

    /// The fixture's capabilities with the budget wire's.
    static func budgetCapabilities(set: (String, Int) -> Void) {
        set("remoteDisplayBudgetVersion", 1)
        set("displayApplicationBytesPerSecond", Int(budgetBytesPerSecond))
        set("spectrumSampleUnitsPerSecond", Int(budgetSampleUnitsPerSecond))
        set("displayBudgetGeneration", Int(budgetGeneration))
    }

    /// The `wrapping` comparison the Core uses
    /// (src/core/session/media/DaemonMediaController.cpp:479-483).
    static func staleOrEqualRevision(_ candidate: UInt32, _ accepted: UInt32) -> Bool {
        let difference = candidate &- accepted
        return difference == 0 || difference >= 0x8000_0000
    }

    /// The answer to a `subscribe` or `unsubscribe` on the budget wire, or
    /// nil for any other operation or one the fake cannot read. A
    /// `subscribe` the fake takes gets its `context` after the result, as
    /// ``Additions/wideband`` sends it.
    func budgetReplies(_ payload: [String: LinkJSON], refusal: (UInt32) -> String?,
                       context: () -> LinkMessage?) -> [LinkMessage]? {
        let op = payload["op"]
        guard op == .string("subscribe") || op == .string("unsubscribe"),
              case .number(let idValue)? = payload["endpointId"], case .number(let revisionValue)? = payload["revision"],
              idValue >= 1, idValue <= Double(UInt32.max), revisionValue >= 1, revisionValue <= Double(UInt32.max) else {
            return nil
        }
        let id = UInt32(idValue)
        let revision = UInt32(revisionValue)
        let connection = payload["connectionId"] ?? .string("")
        let subscribing = op == .string("subscribe")
        // Taken before the fake's lock, which the refusal's own lock shares.
        let refused = subscribing ? refusal(id) : nil
        enum Outcome {
            case answer(accepted: Bool, reason: String)
            case proceed
        }
        let outcome: Outcome = lock.withLock {
            var state = budgetWire
            defer { budgetWire = state }
            // From src/core/session/media/DaemonMediaController.cpp:3766-3802 (subscribe)
            // and :4255-4287 (unsubscribe): the same identity checks.
            if let prior = state.live[id] {
                if revision == prior.revision && payload == prior.request {
                    return .answer(accepted: prior.accepted, reason: prior.reason)
                }
                if Self.staleOrEqualRevision(revision, prior.revision) {
                    return .answer(accepted: false, reason: Self.newerDisplayRequestReason)
                }
            } else if let prior = state.nonlive[id] {
                if revision == prior.revision && payload == prior.request {
                    return .answer(accepted: prior.accepted, reason: prior.reason)
                }
                if prior.retired || Self.staleOrEqualRevision(revision, prior.revision) {
                    return .answer(accepted: false,
                                   reason: prior.retired ? Self.displayClosedReason : Self.newerDisplayRequestReason)
                }
            } else if id <= state.highWater {
                return .answer(accepted: false, reason: Self.displayClosedReason)
            }
            state.highWater = max(state.highWater, id)
            if !subscribing {
                // From :4288-4291: the endpoint goes, remembered as closed.
                state.live[id] = nil
                state.nonlive[id] = .init(revision: revision, request: payload, accepted: true, reason: "", retired: true)
                return .answer(accepted: true, reason: "")
            }
            if let reason = refused {
                // Refused as the Core refuses a display it cannot place, and
                // remembered (rejectAllocation, :1918-1945).
                let record = BudgetWireState.Record(revision: revision, request: payload, accepted: false,
                                                    reason: reason)
                if state.live[id] == nil {
                    state.nonlive[id] = record
                }
                return .answer(accepted: false, reason: reason)
            }
            state.nonlive[id] = nil
            state.live[id] = .init(revision: revision, request: payload, accepted: true, reason: "")
            return .proceed
        }
        switch outcome {
        case .answer(let accepted, let reason):
            return [allocationResult(connection: connection, id: id, revision: revision,
                                     accepted: accepted, reason: reason)]
        case .proceed:
            return [allocationResult(connection: connection, id: id, revision: revision, accepted: true, reason: "")]
                + (context().map { [$0] } ?? [])
        }
    }

    /// A new media connection: the Core forgets the last one's identities.
    func startBudgetWire() {
        lock.withLock { budgetWire = BudgetWireState() }
    }

    /// The Core's `allocation-result` (src/core/session/media/DaemonMediaController.cpp:1883-1916):
    /// what it holds for the endpoint now, after the operation.
    private func allocationResult(connection: LinkJSON, id: UInt32, revision: UInt32, accepted: Bool,
                                  reason: String) -> LinkMessage {
        let held = lock.withLock { budgetWire.live[id]?.revision } ?? 0
        let holds = held != 0
        return .mediaControl(LinkMessage.MediaControl(payload: [
            "op": .string("allocation-result"),
            "connectionId": connection,
            "endpointId": .number(Double(id)),
            "revision": .number(Double(revision)),
            "accepted": .bool(accepted),
            "reason": .string(reason),
            "budgetGeneration": .number(Double(Self.budgetGeneration)),
            "acceptedRevision": .number(Double(held)),
            "applicationBytesPerSecond": .number(Double(holds ? Self.heldBytesPerSecond : 0)),
            "spectrumSampleUnitsPerSecond": .number(Double(holds ? Self.heldSampleUnitsPerSecond : 0)),
            "messagesPerSecond": .number(Double(holds ? Self.heldMessagesPerSecond : 0)),
        ]))
    }
}
