// NereusSDR for iOS: a control's commands, one at a time per control, each value shown at the touch
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The commands a screen's controls send to change a value the Core
/// mirrors, as a property write would (TX antenna, a TX profile, PS-A, an
/// amplifier's or tuner's operate, a receiver's window, its sample rate,
/// the transmit slice). The value shows at the touch
/// (``MirrorStore/hold(_:property:value:)``) and stays until the Core
/// answers, as the desktop's remote window keeps the operator's value
/// (`StationClient.cpp:1040-1068`): refused, the Core's value shows again
/// with its words; taken, it stays until the Core's next value of it; with
/// no answer in time, the latest Core value shows, marked not confirmed. Commands for one
/// control go one at a time; a touch that comes meanwhile replaces any
/// other still waiting and goes after the answer, so the newest touch wins.
/// After a lost link the Core's values show and nothing is sent again.
/// Keying (PTT, TUNE, MOX, 2-Tone, VOX arm, take) never comes here.
@MainActor
public final class CommandHoldQueue {
    /// One mirrored value a command changes.
    public struct Shown: Equatable, Sendable {
        public let key: String
        public let property: String
        public let value: MirrorValue

        public init(_ key: String, _ property: String, _ value: MirrorValue) {
            self.key = key
            self.property = property
            self.value = value
        }

        fileprivate var slot: String {
            key + "." + property
        }
    }

    /// How a command ended: the Core's answer, or why there was none.
    public typealias Outcome = Result<CommandResult, CommandError>
    public typealias Invoke = @MainActor () async throws -> CommandResult
    public typealias OnOutcome = @MainActor (Outcome) -> Void

    public typealias InvokeWithLate = @MainActor (@escaping OnOutcome) async throws -> CommandResult

    private final class Request {
        let id: UInt64
        let shows: [Shown]
        let snapshot: UInt64
        var edits: [String: UInt64] = [:]
        var invoke: InvokeWithLate?
        let onOutcome: OnOutcome
        var initialFinished = false
        var late: Outcome?

        init(id: UInt64, shows: [Shown], snapshot: UInt64, invoke: @escaping InvokeWithLate, onOutcome: @escaping OnOutcome) {
            self.id = id
            self.shows = shows
            self.snapshot = snapshot
            self.invoke = invoke
            self.onOutcome = onOutcome
        }
    }

    private let store: MirrorStore
    private var sending: [String: Request] = [:]
    private var queued: [String: Request] = [:]
    private var nextId: UInt64 = 0
    private var current: [String: UInt64] = [:]

    public init(store: MirrorStore) { self.store = store }

    public func isSending(_ control: String) -> Bool { sending[control] != nil }

    public func send(_ control: String, shows: [Shown], invoke: @escaping Invoke,
                     onOutcome: @escaping OnOutcome) {
        send(control, shows: shows, invokeWithLate: { _ in try await invoke() }, onOutcome: onOutcome)
    }

    /// The answer callback belongs to this touch, including after timeout.
    public func send(_ control: String, shows: [Shown], invokeWithLate: @escaping InvokeWithLate,
                     onOutcome: @escaping OnOutcome) {
        nextId &+= 1
        current[control] = nextId
        let request = Request(id: nextId, shows: shows, snapshot: store.snapshotIdentity, invoke: invokeWithLate, onOutcome: onOutcome)
        if let replaced = queued[control] {
            let covered = Set(shows.map(\.slot))
            for shown in replaced.shows where !covered.contains(shown.slot) {
                if let inFlight = sending[control],
                   let original = inFlight.shows.first(where: { $0.slot == shown.slot }) {
                    inFlight.edits[shown.slot] = store.hold(original.key, property: original.property, value: original.value)
                } else {
                    store.releaseUnsent(shown.key, property: shown.property)
                }
            }
        }
        for shown in shows {
            request.edits[shown.slot] = store.hold(shown.key, property: shown.property, value: shown.value)
        }
        guard sending[control] == nil else {
            queued[control] = request
            return
        }
        start(control, request)
    }

    private func start(_ control: String, _ request: Request) {
        guard let invoke = request.invoke else { return }
        sending[control] = request
        Task { [weak self] in
            let outcome: Outcome
            do {
                outcome = .success(try await invoke { [weak self] late in
                    guard let self else { return }
                    if request.initialFinished { self.settle(control, request, late) }
                    else { request.late = late }
                })
            } catch let error as CommandError { outcome = .failure(error) }
            catch { outcome = .failure(.timedOut) }
            self?.finish(control, request, outcome)
        }
    }

    private func finish(_ control: String, _ request: Request, _ outcome: Outcome) {
        sending[control] = nil
        var next = queued.removeValue(forKey: control)
        if case .failure(.linkLost) = outcome, let dropped = next {
            // Retired touches never replay. A deliberate touch in the
            // replacement snapshot belongs to its own live edit, even if
            // this older loss continuation has not resumed until now.
            let fresh = dropped.snapshot != request.snapshot
                && dropped.snapshot == store.snapshotIdentity
                && store.isSnapshotComplete && !store.isStale
                && dropped.shows.allSatisfy { shown in
                    dropped.edits[shown.slot].map {
                        store.isCurrent(shown.key, property: shown.property, edit: $0)
                    } ?? false
                }
            if !fresh {
                for shown in dropped.shows {
                    if let edit = dropped.edits[shown.slot],
                       store.isCurrent(shown.key, property: shown.property, edit: edit) {
                        store.releaseUnsent(shown.key, property: shown.property)
                    }
                }
                if current[control] == dropped.id { dropped.onOutcome(.failure(.linkLost)) }
                next = nil
            }
        }
        request.invoke = nil
        request.initialFinished = true
        settle(control, request, outcome)
        if let late = request.late {
            request.late = nil
            settle(control, request, late)
        }
        if let next { start(control, next) }
    }

    private func settle(_ control: String, _ request: Request, _ outcome: Outcome) {
        for shown in request.shows {
            guard let edit = request.edits[shown.slot],
                  store.isCurrent(shown.key, property: shown.property, edit: edit) else { continue }
            switch outcome {
            case .success(let result):
                store.commandAnswered(shown.key, property: shown.property,
                                      accepted: result.accepted || SeveralDevices.waitsForConfirmation(result))
            case .failure(.timedOut):
                store.commandNotConfirmed(shown.key, property: shown.property)
            case .failure(.notSent), .failure(.linkLost):
                store.releaseUnsent(shown.key, property: shown.property)
            }
        }
        guard current[control] == request.id else { return }
        if case .success(let result) = outcome, SeveralDevices.waitsForConfirmation(result) { return }
        request.onOutcome(outcome)
    }

}

public extension PropertyWriteOutcome {
    /// A command's ending in a property write's terms, so a control says
    /// the same of either: the Core's answer and words, or the phone's
    /// words for no answer.
    init(_ outcome: CommandHoldQueue.Outcome) {
        switch outcome {
        case .success(let result) where !result.accepted && SeveralDevices.waitsForConfirmation(result):
            self.init(accepted: false, reason: SeveralDevices.waitingReason, value: nil)
        case .success(let result):
            self.init(accepted: result.accepted, reason: result.reason, value: nil)
        case .failure(.notSent):
            self = .notSent
        case .failure(.linkLost):
            self = .linkLost
        case .failure(.timedOut):
            self = .notConfirmed
        }
    }
}
