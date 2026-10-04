// NereusSDR for iOS: the receive level calibration on Setup > Hardware > Calibration: the run, its questions and its answers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import os

/// Level Cal (Level Cal 2, JJ's board 2026-09-30): the desktop's Setup >
/// Hardware Config > Calibration group (CalibrationTab.cpp) on the phone.
/// The frequency and level are the phone's to set, in memory, with the
/// desktop's defaults; Start asks the desktop's question and then asks the
/// Core to calibrate the phone's own active slice; Reset asks first too;
/// Cancel stops a run wherever it was started. The run's state, percent
/// and message are the Core's (`radio`'s `levelCal*`, `levelCalibration`
/// 1). Every refusal shows the Core's words as sent.
@MainActor
final class LevelCalModel: ObservableObject {
    // The Core's verbs and properties (link document: `radioHardwareVersion`
    // 12 at the agreed minor 11; `radio` ordinals 31 to 34).
    static let startVerb = "startLevelCalibration"
    static let cancelVerb = "cancelLevelCalibration"
    static let resetVerb = "resetLevelCalibration"
    static let radioKey = "radio"
    static let runningProperty = "levelCalRunning"
    static let percentProperty = "levelCalPercent"
    static let messageProperty = "levelCalMessage"
    static let succeededProperty = "levelCalSucceeded"
    static let hardwareCapability = "radioHardwareVersion"
    static let hardwareVersion: Int64 = 12
    static let minor: UInt16 = 11
    static let commandTimeout: Duration = .seconds(5)

    // The desktop's two spin boxes (CalibrationTab.cpp's Level Cal group).
    static let frequencyRange: ClosedRange<Double> = 0...30_000_000
    static let frequencyDefault = 14_100_000.0
    static let frequencyStep = 1000.0
    static let levelRange: ClosedRange<Double> = -200...0
    static let levelDefault = -73.0
    static let levelStep = 1.0
    /// The desktop's disabled Rx2 6m LNA, shown at the Rx1 default.
    static let rx2LnaShown = 13.0

    // The desktop's words (CalibrationTab.cpp).
    static let frequencyLabel = "Frequency:"
    static let levelLabel = "Level (dBm):"
    static let rx2LnaLabel = "Rx2 6m LNA:"
    static let rx2LnaNote = "Not used: every receiver shares the Rx1 calibration."
    static let startLabel = "Start"
    static let resetLabel = "Reset"
    static let cancelLabel = "Cancel"
    static let runningText = "A level calibration is running."
    static let startHint = "Measure a signal of the level and frequency above and set the receive level calibration from it."
    static let resetHint = "Put the receive level calibration back to this radio's defaults."
    static let cancelHint = "Stop the level calibration and put the receiver back."
    static let nothingToStopText = "Nothing to stop: no level calibration is running."
    static let runOlderCoreText =
        "This Core cannot run the level calibration for this app. Updating the Core may help."
    static let resetOlderCoreText =
        "This Core cannot reset the level calibration for this app. Updating the Core may help."
    static let startQuestionTitle = "Level Calibration Check"
    static let startQuestion = "Is the calibrated signal present at the correct frequency?"
    static let resetQuestionTitle = "Level Defaults"
    static let resetQuestion = "Do you want to reset Level Calibration back to defaults?"
    static let refusalTitle = "Level Calibration"
    static let doneTitle = "Calibration"
    static let doneText = "Level Calibration complete."

    /// The phone's own words (JJ's rulings, 2026-09-30): which slice a run
    /// calibrates, and the band's line while one runs.
    static func calibratesText(_ letter: String) -> String { "Calibrates slice \(letter)." }
    static func bandLineText(_ letter: String?) -> String {
        letter.map { "Level calibration is running on slice \($0)." } ?? "Level calibration is running."
    }

    /// A question or an answer shown over the page.
    struct Alert: Identifiable, Equatable {
        enum Kind: Equatable {
            case startQuestion
            case resetQuestion
            case message
        }

        let kind: Kind
        let title: String
        let text: String
        var id: String { title + "\n" + text }
    }

    @Published var frequencyHz = LevelCalModel.frequencyDefault
    @Published var levelDbm = LevelCalModel.levelDefault
    @Published private(set) var running = false
    @Published private(set) var percent: Int64 = 0
    /// The Core's status line: empty while a run goes.
    @Published private(set) var message = ""
    /// The Core runs, cancels and resets it for this app.
    @Published private(set) var offered = false
    /// "Calibrates slice A.": the phone's active slice, nil without one.
    @Published private(set) var sliceLine: String?
    @Published private(set) var startEnabled = false
    @Published private(set) var resetEnabled = false
    @Published private(set) var cancelEnabled = false
    /// The greyed buttons' reasons, each once, in button order.
    @Published private(set) var reasons: [String] = []
    /// The band's line while a run goes; nil otherwise.
    @Published private(set) var bandLine: String?
    @Published var alert: Alert?

    private let store: MirrorStore
    private let commands: CommandClient?
    private let slices: BandSlicesModel
    private var watches: Set<AnyCancellable> = []
    private var radioWatch: AnyCancellable?
    private var watchedRadio: ObjectIdentifier?
    private var rebuildQueued = false
    /// The slice a run this phone started calibrates, until the run ends.
    private var startedSliceId: Int?
    private var startedHere = false
    private static let logger = Logger(subsystem: "NereusSDR", category: "setup.levelcal")

    init(store: MirrorStore, commands: CommandClient?, slices: BandSlicesModel) {
        self.store = store
        self.commands = commands
        self.slices = slices
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$objectKeys.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$capabilities.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$isSnapshotComplete.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        rebuild()
    }

    // MARK: Acting

    /// Start: the desktop's question first; Yes sends, No sends nothing.
    func start() {
        guard startEnabled else {
            return
        }
        alert = Alert(kind: .startQuestion, title: Self.startQuestionTitle, text: Self.startQuestion)
    }

    /// Yes to the Start question: the phone's own active slice, or -1 (the
    /// Core's active slice) when the phone has none.
    func confirmStart() {
        alert = nil
        guard startEnabled else {
            return
        }
        let sliceId = slices.activeSliceId
        let arguments = [CommandArgument(name: "levelDbm", value: .double(levelDbm)),
                         CommandArgument(name: "frequencyHz", value: .double(frequencyHz)),
                         CommandArgument(name: "sliceId", value: .int(Int64(sliceId ?? -1)))]
        // Held from the send, so a run the Core starts before it answers is
        // this phone's; a refusal or no answer lets it go.
        startedHere = true
        startedSliceId = sliceId
        send(Self.startVerb, arguments, refusalTitle: Self.refusalTitle) { [weak self] accepted in
            guard let self, !accepted, !self.running else {
                return
            }
            self.startedHere = false
            self.startedSliceId = nil
        }
    }

    func reset() {
        guard resetEnabled else {
            return
        }
        alert = Alert(kind: .resetQuestion, title: Self.resetQuestionTitle, text: Self.resetQuestion)
    }

    func confirmReset() {
        alert = nil
        guard resetEnabled else {
            return
        }
        send(Self.resetVerb, [], refusalTitle: Self.resetQuestionTitle)
    }

    func cancel() {
        guard cancelEnabled else {
            return
        }
        send(Self.cancelVerb, [], refusalTitle: Self.refusalTitle)
    }

    func dismissAlert() {
        alert = nil
    }

    private func send(_ verb: String, _ arguments: [CommandArgument], refusalTitle: String,
                      accepted: ((Bool) -> Void)? = nil) {
        guard let commands else {
            return
        }
        Task { [weak self] in
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: Self.commandTimeout)
                if !result.accepted {
                    // The Core's words, as sent.
                    self?.alert = Alert(kind: .message, title: refusalTitle,
                                        text: result.reason.isEmpty ? BandSlicesModel.refusedText : result.reason)
                }
                accepted?(result.accepted)
            } catch {
                Self.logger.info("A \(verb, privacy: .public) request had no answer from the Core")
                accepted?(false)
            }
        }
    }

    // MARK: Reading

    private func queueRebuild() {
        guard !rebuildQueued else {
            return
        }
        rebuildQueued = true
        Task { @MainActor [weak self] in
            self?.rebuildQueued = false
            self?.rebuild()
        }
    }

    private func rebuild() {
        let radio = store.object(Self.radioKey)
        let id = radio.map(ObjectIdentifier.init)
        if id != watchedRadio {
            watchedRadio = id
            radioWatch = radio?.$values.dropFirst().sink { [weak self] _ in self?.queueRebuild() }
        }
        let offered = (store.agreedMinor ?? 0) >= Self.minor
            && store.capabilityVersion(Self.hardwareCapability) >= Self.hardwareVersion
        let wasRunning = running
        let running = offered && ToolValue.flag(radio?[Self.runningProperty]) == true
        set(\.offered, offered)
        set(\.running, running)
        set(\.percent, min(max(ToolValue.whole(radio?[Self.percentProperty]) ?? 0, 0), 100))
        set(\.message, running ? "" : ToolValue.text(radio?[Self.messageProperty]) ?? "")

        let active = slices.entries.first { $0.id == slices.activeSliceId }
        set(\.sliceLine, active.map { Self.calibratesText(BandSlice.letter(forIndex: $0.id)) })
        // Never a slice another device controls: its owner line says why.
        var ownerLine: String?
        if case .listening(let line)? = active?.control {
            ownerLine = line
        }
        let ready = store.isSnapshotComplete
        let startEnabled = ready && offered && !running && ownerLine == nil
        let resetEnabled = ready && offered && !running
        let cancelEnabled = ready && offered && running
        set(\.startEnabled, startEnabled)
        set(\.resetEnabled, resetEnabled)
        set(\.cancelEnabled, cancelEnabled)
        var reasons: [String] = []
        func add(_ reason: String?) {
            if let reason, !reasons.contains(reason) {
                reasons.append(reason)
            }
        }
        if ready {
            if !offered {
                add(Self.runOlderCoreText)
                add(Self.resetOlderCoreText)
            } else if running {
                add(Self.runningText)
            } else {
                add(ownerLine)
                add(Self.nothingToStopText)
            }
        }
        set(\.reasons, reasons)

        if wasRunning, !running, startedHere {
            // The desktop's alert, on the device that started the run only.
            if ToolValue.flag(radio?[Self.succeededProperty]) == true {
                alert = Alert(kind: .message, title: Self.doneTitle, text: Self.doneText)
            }
            startedHere = false
            startedSliceId = nil
        }
        set(\.bandLine, running ? Self.bandLineText(startedHere ? startedSliceId.map { BandSlice.letter(forIndex: $0) }
                                                                : nil) : nil)
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<LevelCalModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
}
