// NereusSDR for iOS: the PTT button test's event log (debug copies only, never keys the radio)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if PTT_PROBE
import Combine
import Foundation

/// Where a probe event came from (R-IOS-15, plan Task 65 step 1). The
/// probe records presses and the system's Push to Talk begin and end; it
/// never keys the radio.
enum PttProbeSource: String, CaseIterable, Sendable {
    /// Apple's own Talk button in the system's Push to Talk interface.
    case systemTalkButton
    /// A hands-free button, such as a wired headset's, as Push to Talk reports it.
    case headset
    /// A Bluetooth button's characteristic change, through CoreBluetooth.
    case bluetooth
    /// The Action button, through the probe's Push to Talk intent.
    case actionButton
    /// A media remote command (play, pause, toggle), which a headset can also send.
    case mediaButton
    /// The probe's own begin or end button on its page.
    case probeScreen
    /// The channel itself: joining, leaving, the microphone's session.
    case channel
    /// The phone: locking, unlocking, going to the background, audio interruptions.
    case phone
    /// Push to Talk named no source it documents.
    case unknown

    var label: String {
        switch self {
        case .systemTalkButton: "System Talk button"
        case .headset: "Headset button"
        case .bluetooth: "Bluetooth button"
        case .actionButton: "Action button"
        case .mediaButton: "Media button"
        case .probeScreen: "Test page button"
        case .channel: "Channel"
        case .phone: "Phone"
        case .unknown: "Unknown"
        }
    }
}

/// What happened.
enum PttProbeKind: String, Sendable {
    /// A button was pressed (before any begin or end it asks for).
    case press
    /// Push to Talk began a transmission.
    case begin
    /// Push to Talk ended a transmission.
    case end
    /// Anything else worth a line: joins, leaves, failures, the microphone, the lock.
    case note

    var label: String {
        switch self {
        case .press: "press"
        case .begin: "BEGIN"
        case .end: "END"
        case .note: "note"
        }
    }
}

/// Push to Talk's own names for who asked to begin or end, mirrored so the
/// probe's model builds and tests on the simulator, where the framework is
/// missing.
enum PttProbeRequestSource: Sendable, Equatable {
    case unknown
    case userRequest
    case developerRequest
    case handsfreeButton
}

/// Whether the app is in front, and whether the phone is locked, when an
/// event is recorded.
struct PttProbeMoment: Sendable, Equatable {
    enum AppState: String, Sendable {
        case active, inactive, background
    }

    var locked: Bool
    var appState: AppState
}

/// One line of the log.
struct PttProbeEvent: Sendable, Equatable, Identifiable {
    let id: Int
    let at: Date
    /// Seconds since the log started (or was cleared).
    let sinceStart: TimeInterval
    let source: PttProbeSource
    let kind: PttProbeKind
    let moment: PttProbeMoment
    let detail: String

    /// The event as one line of the shared text. The wall clock is UTC so
    /// lines from different days and zones sort the same way.
    var line: String {
        let since = String(format: "+%.3f s", sinceStart)
        let lock = moment.locked ? "locked" : "unlocked"
        var parts = [Self.clock(at), since, source.label, kind.label, lock, moment.appState.rawValue]
        if !detail.isEmpty {
            parts.append(detail)
        }
        return parts.joined(separator: " | ")
    }

    static func clock(_ date: Date) -> String {
        date.formatted(Date.ISO8601FormatStyle(includingFractionalSeconds: true, timeZone: .gmt))
    }
}

/// What a toggling press (the Action button, a Bluetooth button, the test
/// page's button) asks Push to Talk for.
enum PttProbeToggle: Equatable, Sendable {
    case begin
    case end
    /// Not joined to a channel: there is nothing to begin.
    case nothing

    static func next(joined: Bool, talking: Bool) -> PttProbeToggle {
        guard joined else { return .nothing }
        return talking ? .end : .begin
    }
}

/// Which Bluetooth characteristic changes count as a press.
enum PttProbeBluetoothRule: String, CaseIterable, Sendable, Identifiable {
    /// Every change toggles (a button that reports only presses).
    case everyChange
    /// Only a value with a non-zero byte toggles (a button that reports
    /// press as non-zero and release as zero).
    case nonZeroOnly

    var id: String { rawValue }

    var label: String {
        switch self {
        case .everyChange: "Every change"
        case .nonZeroOnly: "Non-zero only"
        }
    }

    func isPress(_ value: Data) -> Bool {
        switch self {
        case .everyChange: true
        case .nonZeroOnly: value.contains { $0 != 0 }
        }
    }
}

/// The probe's log: every event with its time, source, begin or end, and
/// whether the phone was locked. It turns Push to Talk's begin and end
/// into the button that asked for them, and times each transmission.
@MainActor
final class PttProbeLog: ObservableObject {
    @Published private(set) var events: [PttProbeEvent] = []
    /// When the transmission Push to Talk has begun and not ended began.
    @Published private(set) var talkingSince: Date?
    @Published private(set) var talkingSource: PttProbeSource?
    /// A begin the probe asked for that Push to Talk has not answered.
    @Published private(set) var beginPending = false

    private var startedAt: Date
    private var nextId = 0
    /// The button that asked the probe to begin or end, until Push to Talk
    /// answers with a developer request.
    private var pendingRequester: PttProbeSource?

    init(now: Date = Date()) {
        startedAt = now
    }

    var talking: Bool { talkingSince != nil }

    @discardableResult
    func record(_ source: PttProbeSource, _ kind: PttProbeKind, moment: PttProbeMoment,
                detail: String = "", at now: Date = Date()) -> PttProbeEvent {
        let event = PttProbeEvent(id: nextId, at: now, sinceStart: now.timeIntervalSince(startedAt),
                                  source: source, kind: kind, moment: moment, detail: detail)
        nextId += 1
        events.append(event)
        return event
    }

    /// A toggling button pressed: records the press and says what to ask
    /// Push to Talk for. A begin still unanswered counts as talking, so a
    /// quick second press ends it rather than asking twice.
    func toggle(by source: PttProbeSource, joined: Bool, moment: PttProbeMoment,
                detail: String = "", at now: Date = Date()) -> PttProbeToggle {
        let next = PttProbeToggle.next(joined: joined, talking: talking || beginPending)
        let asks = switch next {
        case .begin: "asks to begin"
        case .end: "asks to end"
        case .nothing: "not joined to a channel, nothing to begin"
        }
        record(source, .press, moment: moment, detail: detail.isEmpty ? asks : "\(detail), \(asks)", at: now)
        if next != .nothing {
            pendingRequester = source
        }
        beginPending = next == .begin
        return next
    }

    /// The source Push to Talk named, as the button that pressed it.
    func source(for request: PttProbeRequestSource) -> PttProbeSource {
        switch request {
        case .userRequest: .systemTalkButton
        case .handsfreeButton: .headset
        case .developerRequest: pendingRequester ?? .unknown
        case .unknown: .unknown
        }
    }

    /// Push to Talk began a transmission.
    @discardableResult
    func began(from request: PttProbeRequestSource, moment: PttProbeMoment, at now: Date = Date()) -> PttProbeEvent {
        let source = source(for: request)
        if request == .developerRequest {
            pendingRequester = nil
        }
        beginPending = false
        talkingSince = now
        talkingSource = source
        return record(source, .begin, moment: moment, at: now)
    }

    /// Push to Talk ended a transmission. The line says how long it lasted
    /// and how many microphone buffers arrived while it did.
    @discardableResult
    func ended(from request: PttProbeRequestSource, moment: PttProbeMoment, microphoneBuffers: Int,
               at now: Date = Date()) -> PttProbeEvent {
        let source = source(for: request)
        if request == .developerRequest {
            pendingRequester = nil
        }
        beginPending = false
        var detail = "microphone buffers \(microphoneBuffers)"
        if let since = talkingSince {
            detail = String(format: "lasted %.3f s, ", now.timeIntervalSince(since)) + detail
            if let began = talkingSource, began != source {
                detail += ", begun by \(began.label)"
            }
        }
        talkingSince = nil
        talkingSource = nil
        return record(source, .end, moment: moment, detail: detail, at: now)
    }

    /// Push to Talk refused a begin or end the probe asked for.
    func requestFailed(moment: PttProbeMoment, detail: String, at now: Date = Date()) {
        let source = pendingRequester ?? .unknown
        pendingRequester = nil
        beginPending = false
        record(source, .note, moment: moment, detail: detail, at: now)
    }

    /// The channel is gone: no transmission can still be going.
    func channelLeft(moment: PttProbeMoment, detail: String, at now: Date = Date()) {
        talkingSince = nil
        talkingSource = nil
        pendingRequester = nil
        beginPending = false
        record(.channel, .note, moment: moment, detail: detail, at: now)
    }

    func clear(now: Date = Date()) {
        events = []
        startedAt = now
    }

    /// The whole log as text, oldest first, to share.
    func text(title: String = "NereusSDR PTT button test") -> String {
        ([title, "time (UTC) | since start | source | event | lock | app | detail"] + events.map(\.line))
            .joined(separator: "\n") + "\n"
    }
}
#endif
