// NereusSDR for iOS: the Core's questions and notices about other devices, and this phone's answers to them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import os

/// This phone's side of sharing a Core (R-IOS-17, R-IOS-30; the
/// several-devices design, sections 7.3 and 7.4): the question the Core
/// holds open for it (`confirm.request`), the notices it has been sent
/// (`notice`), and the answers: `confirm.proceed`, `confirm.cancel` and
/// `notice.takeBack`.
///
/// Nothing changes on the phone until the Core answers. An accepted
/// proceed carries the readback of the change it applied (ruling 7.4a),
/// which goes into the mirror or the settings cache as the Core's own
/// word, since the Core withholds the change's echo from the device that
/// asked. A refused proceed leaves the question up with the Core's reason
/// until it is closed; a new question from the Core replaces the open one.
/// The Core drops open questions when the session ends, so a lost link
/// drops them here too, and the notices with them.
@MainActor
public final class SeveralDevicesClient: ObservableObject {
    /// Where an answer to the open question has got to.
    public enum Answering: Equatable, Sendable {
        case idle
        /// Confirm was sent; the Core has not answered yet.
        case proceeding
        /// The Core refused the answer, in its words (or the phone's when
        /// the Core never answered).
        case refused(String)
    }

    /// A notice, and when it reached this phone.
    public struct ReceivedNotice: Equatable, Sendable, Identifiable {
        public var notice: SeveralDevices.Notice
        public var received: Date
        /// Take it back was sent and its question is awaited.
        public var takingBack = false
        /// Take it back was refused, in the Core's words.
        public var takeBackRefusal: String?

        public var id: Int64 { notice.id }

        /// When it happened, by this phone's clock.
        public var happened: Date { notice.happened(received: received) }
    }

    /// What the phone says when the Core never answered an answer.
    public static let noAnswerText = "The Core didn't answer. Try again."
    /// How long an answer waits for the Core.
    public static let answerTimeout: Duration = .seconds(10)

    /// The Core's open question for this phone, if any.
    @Published public private(set) var question: SeveralDevices.Question?
    /// When the open question arrived.
    @Published public private(set) var questionReceived: Date?
    @Published public private(set) var answering: Answering = .idle
    /// The notices the Core has sent in this session, oldest first, until dismissed.
    @Published public private(set) var notices: [ReceivedNotice] = []

    /// The Core's words for a Take it back of control that can never work
    /// now (the slice closed, or its control moved on), whose notice is
    /// taken down: shown over the band as any refusal is, until closed.
    public struct EndedTakeBack: Equatable, Sendable, Identifiable {
        public let id: Int
        public let text: String
    }

    @Published public private(set) var endedTakeBack: EndedTakeBack?
    private var endedTakeBacks = 0
    /// Told the slice's id when Take it back of control worked: this phone
    /// controls it again, and, as after any take, its first key on it waits
    /// for the slice's TX button. With the slice id and the control
    /// revision the Core answered with, when it sent one.
    public var tookControlBack: ((Int, Int64?) -> Void)?

    private let store: MirrorStore
    private let settings: SettingsProxyClient
    private let commands: CommandClient?
    private let now: () -> Date
    private var linkUp = false
    /// The open question whose confirm the Core never answered: the one
    /// question closing cancels, since the Core may still hold it.
    private var unansweredQuestion: Int64?
    /// The notice a question answers Take it back for, by question id: an
    /// accepted proceed of that question closes the notice.
    private var takingBackFor: [Int64: Int64] = [:]
    private static let logger = Logger(subsystem: "NereusSDR", category: "mirror.devices")

    public init(store: MirrorStore, settings: SettingsProxyClient, commands: CommandClient?,
                now: @escaping () -> Date = Date.init) {
        self.store = store
        self.settings = settings
        self.commands = commands
        self.now = now
    }

    // MARK: Feeding

    /// One session event: a question, a notice, or a change of state.
    public func handle(_ event: StationSession.Event) {
        switch event {
        case .message(let message):
            receive(message)
        case .stateChanged(let state):
            let up = state == .receivingSnapshot || state == .ready
            if linkUp && !up {
                question = nil
                questionReceived = nil
                answering = .idle
                unansweredQuestion = nil
                notices = []
                takingBackFor = [:]
                endedTakeBack = nil
            }
            linkUp = up
        case .refused:
            break
        }
    }

    /// One message from the Core; only `confirm.request` and `notice` matter here.
    public func receive(_ message: LinkMessage) {
        switch message {
        case .confirmRequest(let wire):
            // A new question replaces the open one (ruling 7.5).
            question = SeveralDevices.Question(wire)
            questionReceived = now()
            answering = .idle
            unansweredQuestion = nil
            for index in notices.indices where notices[index].takingBack {
                notices[index].takingBack = false
                takingBackFor[wire.id] = notices[index].id
            }
        case .notice(let wire):
            let notice = SeveralDevices.Notice(wire)
            notices.removeAll { $0.id == notice.id }
            notices.append(ReceivedNotice(notice: notice, received: now()))
        default:
            break
        }
    }

    // MARK: Answering

    /// Confirm: `confirm.proceed` with `choice` (-1 when the question has
    /// none). Returns once the Core has answered, true when it accepted
    /// the answer. `timeout` is how long the answer may take; a take of
    /// transmit also waits out the Core's unkey handover.
    @discardableResult
    public func proceed(choice: Int64 = SeveralDevices.noChoice,
                        timeout: Duration = SeveralDevicesClient.answerTimeout) async -> Bool {
        guard let asked = question, answering != .proceeding, let commands else {
            return false
        }
        answering = .proceeding
        unansweredQuestion = nil
        let result: CommandResult
        do {
            result = try await commands.invoke(SeveralDevices.proceedVerb, arguments: [
                CommandArgument(name: "id", value: .int(asked.id)),
                CommandArgument(name: "choice", value: .int(choice)),
            ], timeout: timeout)
        } catch {
            Self.logger.info("confirm.proceed did not reach the Core: \(String(describing: error), privacy: .public)")
            if question?.id == asked.id {
                answering = .refused(Self.noAnswerText)
                unansweredQuestion = asked.id
            }
            return false
        }
        if result.accepted {
            if let readback = SeveralDevices.Readback(result) {
                apply(readback)
            }
            if question?.id == asked.id {
                question = nil
                questionReceived = nil
            }
            answering = .idle
            if let noticeId = takingBackFor.removeValue(forKey: asked.id) {
                // Taken back: what the notice told is undone.
                dismiss(noticeId)
            }
            return true
        }
        guard question?.id == asked.id, !SeveralDevices.waitsForConfirmation(result) else {
            // The Core asks again (what the change reaches grew): its new
            // question stands, or is on its way.
            if answering == .proceeding {
                answering = .idle
            }
            return false
        }
        answering = .refused(result.reason)
        return false
    }

    /// Cancel: `confirm.cancel`. The question closes at once, since
    /// cancelling changes nothing.
    public func cancel() {
        guard let asked = question else {
            return
        }
        question = nil
        questionReceived = nil
        answering = .idle
        unansweredQuestion = nil
        sendCancel(asked.id)
    }

    /// Closes a question whose answer the Core refused, or never answered.
    /// A refused confirm has already closed the question at the Core (it
    /// takes the question as it reads the answer, ConfirmStep::answer), so
    /// nothing is sent. A confirm that had no answer may not have reached
    /// it, so the question may still be open there: it is cancelled.
    public func closeQuestion() {
        let unanswered = unansweredQuestion.flatMap { $0 == question?.id ? $0 : nil }
        question = nil
        questionReceived = nil
        answering = .idle
        unansweredQuestion = nil
        if let unanswered {
            sendCancel(unanswered)
        }
    }

    private func sendCancel(_ id: Int64) {
        guard let commands else {
            return
        }
        Task {
            do {
                let result = try await commands.invoke(SeveralDevices.cancelVerb,
                                                       arguments: [CommandArgument(name: "id", value: .int(id))],
                                                       timeout: Self.answerTimeout)
                if !result.accepted {
                    Self.logger.info("The Core refused confirm.cancel: \(result.reason, privacy: .private)")
                }
            } catch {
                Self.logger.info("confirm.cancel did not reach the Core: \(String(describing: error), privacy: .public)")
            }
        }
    }

    /// Take it back: `notice.takeBack`, which the Core answers with a new
    /// question the other way; for control of a slice (`controlTaken`) it
    /// runs the take at once and answers with its result.
    public func takeBack(_ noticeId: Int64) async {
        guard let index = notices.firstIndex(where: { $0.id == noticeId }), notices[index].notice.takeBack,
              !notices[index].takingBack, let commands else {
            return
        }
        if notices[index].notice.kind == .controlTaken {
            await takeControlBack(noticeId, commands: commands)
            return
        }
        notices[index].takingBack = true
        notices[index].takeBackRefusal = nil
        var refusal: String?
        do {
            let result = try await commands.invoke(SeveralDevices.takeBackVerb,
                                                   arguments: [CommandArgument(name: "id", value: .int(noticeId))],
                                                   timeout: Self.answerTimeout)
            if !result.accepted && !SeveralDevices.waitsForConfirmation(result) {
                refusal = result.reason
            }
        } catch {
            Self.logger.info("notice.takeBack did not reach the Core: \(String(describing: error), privacy: .public)")
            refusal = Self.noAnswerText
        }
        guard let refusal, let now = notices.firstIndex(where: { $0.id == noticeId }) else {
            return
        }
        notices[now].takingBack = false
        notices[now].takeBackRefusal = refusal
    }

    /// Closes a notice on this phone.
    public func dismiss(_ noticeId: Int64) {
        notices.removeAll { $0.id == noticeId }
    }

    /// Closes the words of a Take it back that ended.
    public func dismissEndedTakeBack() {
        endedTakeBack = nil
    }

    /// Why a `controlTaken` notice's Take it back is shown greyed, in the
    /// Core's words (the slice access contract note): a Core below
    /// `sliceAccessVersion` 2 cannot give control back from here; a notice
    /// a Core at 2 sent with `takeBack` false can no longer be taken back.
    /// Nil while Take it back is offered, and for every other kind.
    public func takeBackUnavailableReason(_ received: ReceivedNotice) -> String? {
        guard received.notice.kind == .controlTaken, !received.notice.takeBack else {
            return nil
        }
        return SliceAccess.version(in: store) >= SliceAccess.takeBackVersion
            ? SliceAccess.noLongerTakenBackText : SliceAccess.coreCannotGiveBackText
    }

    /// Take it back of control (`sliceAccessVersion` 2): the Core runs
    /// `slice.takeControl` for this phone with the notice's slice entry.
    /// Accepted, the card goes. Refused while the Core still holds the
    /// take-back (the slice is the entry's, at the entry's revision, as the
    /// mirror's `access:<id>` shows it: the slice was transmitting), the
    /// card stays with the Core's words and can be tried again. Any other
    /// refusal (the slice closed, its control moved on, or it was already
    /// taken back) takes the card down and shows the Core's words over the
    /// band.
    private func takeControlBack(_ noticeId: Int64, commands: CommandClient) async {
        guard let index = notices.firstIndex(where: { $0.id == noticeId }) else {
            return
        }
        notices[index].takingBack = true
        notices[index].takeBackRefusal = nil
        let outcome: SliceAccess.TakeOutcome
        do {
            let result = try await commands.invoke(SeveralDevices.takeBackVerb,
                                                   arguments: [CommandArgument(name: "id", value: .int(noticeId))],
                                                   timeout: Self.answerTimeout)
            outcome = SliceAccess.outcome(result)
        } catch {
            Self.logger.info("notice.takeBack did not reach the Core: \(String(describing: error), privacy: .public)")
            outcome = .notAnswered
        }
        guard let now = notices.firstIndex(where: { $0.id == noticeId }) else {
            return
        }
        let received = notices[now]
        switch outcome {
        case .accepted(let revision):
            dismiss(noticeId)
            if let slice = received.notice.slices.first {
                tookControlBack?(slice.sliceId, revision)
            }
        case .refused(let reason) where stillTakesBack(received.notice):
            notices[now].takingBack = false
            notices[now].takeBackRefusal = reason
        case .refused(let reason):
            dismiss(noticeId)
            endedTakeBacks += 1
            endedTakeBack = EndedTakeBack(id: endedTakeBacks, text: reason)
        case .notAnswered, .notSent:
            notices[now].takingBack = false
            notices[now].takeBackRefusal = Self.noAnswerText
        }
    }

    /// Whether the Core still holds a `controlTaken` notice's take-back:
    /// the slice it names is live with the entry's incarnation and still at
    /// the entry's control revision.
    private func stillTakesBack(_ notice: SeveralDevices.Notice) -> Bool {
        guard let entry = notice.slices.first, let incarnation = entry.incarnation,
              let revision = entry.controlRevision,
              let access = SliceAccess.states(in: store)[entry.sliceId] else {
            return false
        }
        return access.incarnation == incarnation && access.controlRevision == revision
    }

    // MARK: Inside

    /// The readback goes in as the Core's own word: a property write's
    /// values into its object, a setting's value or absence into the cache.
    private func apply(_ readback: SeveralDevices.Readback) {
        switch readback {
        case .properties(let key, let values):
            let entries = values.sorted { $0.key < $1.key }.map { name, value in
                LinkMessage.PropertyEntry(name: name, value: value.wireValue)
            }
            store.apply(.delta(LinkMessage.Delta(key: key, properties: entries)))
        case .setting(let key, let value):
            settings.apply(.settingsValue(LinkMessage.SettingsValue(
                key: key, origin: "", properties: value.map {
                    [LinkMessage.PropertyEntry(name: key, value: .utf8($0))]
                } ?? [])))
        }
    }
}
