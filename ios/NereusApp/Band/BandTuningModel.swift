// NereusSDR for iOS: what a flag's step button and frequency open: the step menu and the number pad
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import NereusModels

/// The flag's two tuning controls (D74, spec section 5.1 item 17, picture
/// 26): the step button, which lists the radio's steps (the Core's
/// catalogue `tuneSteps`, R-IOS-27; none are the app's own) and writes the
/// slice's step, and the frequency, whose tap opens the number pad. At most
/// one of the two is open. Without the catalogue's steps the step button is
/// greyed and its menu says "Needs a newer Core".
///
/// The tuning dial (D12, R-IOS-12, spec section 5.1 item 8) turns here
/// too: each detent moves the active slice one of its steps, written as a
/// drag's are, with a haptic tick, and a firmer bump on each whole
/// kilohertz. The dial's middle opens the same step menu, beside the dial
/// rather than under the flag, and the knob in a sheet opens and closes here.
@MainActor
final class BandTuningModel: ObservableObject {
    /// The slice whose step menu is open.
    @Published private(set) var stepMenuSliceId: Int?
    /// The open step menu came from the dial's middle, not the flag.
    @Published private(set) var stepMenuFromDial = false
    /// The number pad, while it is open.
    @Published private(set) var pad: FrequencyPadModel?
    /// The knob in a sheet is up.
    @Published private(set) var dialSheetOpen = false

    private let slices: BandSlicesModel
    private let haptics: DialHaptics
    /// The dial while a finger turns it, on the slice it began on.
    private var turning: (sliceId: Int, dial: DialModel)?

    init(slices: BandSlicesModel, haptics: DialHaptics = DialHaptics()) {
        self.slices = slices
        self.haptics = haptics
    }

    /// Which of the dial's haptics play, as Setup, Navigation sets them:
    /// the tick on each detent and the bump on each whole kilohertz.
    func setDialHaptics(ticksOnDetents: Bool, bumpsOnKilohertz: Bool) {
        haptics.ticksOnDetents = ticksOnDetents
        haptics.bumpsOnKilohertz = bumpsOnKilohertz
    }

    /// Which of the dial's haptics play now.
    var dialHaptics: (ticksOnDetents: Bool, bumpsOnKilohertz: Bool) {
        (haptics.ticksOnDetents, haptics.bumpsOnKilohertz)
    }

    // MARK: The step

    /// The radio's steps, smallest first, from the Core's catalogue.
    var steps: [StationCatalog.TuneStep] { slices.catalog?.tuneSteps ?? [] }

    /// Whether the Core lists its steps.
    var stepsAvailable: Bool { !steps.isEmpty }

    /// The step the flag shows for `entry`: the catalogue's label for it,
    /// or nil when the slice has none.
    func stepLabel(for entry: BandSlicesModel.Entry) -> String? {
        Self.stepLabel(stepHz: entry.stepHz, steps: steps)
    }

    /// The catalogue's label for `stepHz`; a step the catalogue does not
    /// list shows as its hertz.
    static func stepLabel(stepHz: Double?, steps: [StationCatalog.TuneStep]) -> String? {
        guard let stepHz, stepHz > 0 else {
            return nil
        }
        if let step = steps.first(where: { $0.hz == stepHz }) {
            return step.label
        }
        return "\(Int(stepHz.rounded())) Hz"
    }

    /// The menu's title, `Step for slice A`.
    func stepMenuTitle(sliceId: Int) -> String {
        "Step for slice \(BandSlice.letter(forIndex: sliceId))"
    }

    func openStepMenu(sliceId: Int) {
        pad?.retire()
        pad = nil
        stepMenuFromDial = false
        stepMenuSliceId = sliceId
    }

    /// The dial's middle: the step menu for the active slice, beside the
    /// dial; a second tap closes it.
    func toggleDialStepMenu() {
        guard !(stepMenuFromDial && stepMenuSliceId != nil), let id = slices.activeSliceId else {
            closeStepMenu()
            return
        }
        pad?.retire()
        pad = nil
        stepMenuFromDial = true
        stepMenuSliceId = id
    }

    func closeStepMenu() {
        stepMenuSliceId = nil
        stepMenuFromDial = false
    }

    /// Whether `step` is the open menu's slice's step.
    func isCurrent(_ step: StationCatalog.TuneStep) -> Bool {
        guard let id = stepMenuSliceId else {
            return false
        }
        return slices.entries.first { $0.id == id }?.stepHz == step.hz
    }

    /// A step picked: the slice's step is written, and the menu closes.
    func pick(_ step: StationCatalog.TuneStep) {
        guard let id = stepMenuSliceId else {
            return
        }
        slices.setStep(step.hz, sliceId: id)
        closeStepMenu()
    }

    /// A step picked in the sheet's row: the active slice's step; the row stays.
    func pickForActive(_ step: StationCatalog.TuneStep) {
        guard let id = slices.activeSliceId else {
            return
        }
        slices.setStep(step.hz, sliceId: id)
    }

    /// Whether `step` is the active slice's step.
    func isActiveStep(_ step: StationCatalog.TuneStep) -> Bool {
        slices.active?.stepHz == step.hz
    }

    // MARK: The dial

    /// The active slice's step as the dial's middle shows it, or nil when
    /// it has none.
    var dialStepLabel: String? {
        slices.active.flatMap(stepLabel(for:))
    }

    /// Whether the dial can tune: the active slice is one this phone may
    /// change and has a step.
    var dialTunes: Bool {
        guard let active = slices.active else {
            return false
        }
        return slices.isTunable(active) && (active.stepHz ?? 0) > 0
    }

    /// A finger comes down on the dial: it starts from the active slice's
    /// frequency. A slice this phone may not change is not tuned.
    func beginTurn(reversed: Bool) {
        guard turning == nil else {
            turning?.dial.reversed = reversed
            return
        }
        guard dialTunes, let active = slices.active else {
            return
        }
        turning = (active.id, DialModel(hz: active.slice.frequencyHz, stepHz: active.stepHz, reversed: reversed))
        haptics.prepare()
    }

    /// The knob turned by `radians`, clockwise positive: the detents it made.
    @discardableResult
    func turn(byRadians radians: Double) -> [DialEvent] {
        advance { $0.rotate(byRadians: radians) }
    }

    /// The thumbwheel rolled by `points`, rightward positive: rolling left
    /// tunes up, one detent every 12 points.
    @discardableResult
    func roll(byPoints points: Double) -> [DialEvent] {
        advance { $0.roll(byPoints: points) }
    }

    /// The finger lifts: the last frequency goes, if it has not already.
    func endTurn() {
        guard turning != nil else {
            return
        }
        turning = nil
        slices.finishDrag()
    }

    /// Moves the dial with the slice's step as it is now, so a new step
    /// applies from the next detent. A slice locked or gone meanwhile is
    /// not tuned.
    private func advance(_ move: (inout DialModel) -> [DialEvent]) -> [DialEvent] {
        guard var current = turning, let entry = slices.entries.first(where: { $0.id == current.sliceId }),
              slices.isTunable(entry) else {
            return []
        }
        current.dial.stepHz = entry.stepHz
        let events = move(&current.dial)
        turning = current
        tune(events, sliceId: current.sliceId)
        return events
    }

    /// Detents become the slice's frequency, written as a drag's are (at
    /// most every 50 ms, then the final value), and the haptics.
    private func tune(_ events: [DialEvent], sliceId: Int) {
        guard !events.isEmpty else {
            return
        }
        haptics.play(events)
        var last: Double?
        for case .detent(let hz) in events {
            last = hz
        }
        if let last {
            slices.drag(sliceId: sliceId, to: last)
        }
    }

    // MARK: The knob in a sheet

    /// A tap on a flag's frequency with the knob in a sheet chosen: that
    /// slice becomes active and the sheet comes up.
    func openDialSheet(sliceId: Int) {
        slices.activate(sliceId)
        pad?.retire()
        pad = nil
        closeStepMenu()
        dialSheetOpen = true
    }

    func closeDialSheet() {
        endTurn()
        closeStepMenu()
        dialSheetOpen = false
    }

    // MARK: The number pad

    /// Opens the pad for `sliceId`, when this phone may change that slice.
    func openPad(sliceId: Int) {
        guard let entry = slices.entries.first(where: { $0.id == sliceId }), slices.isTunable(entry) else {
            return
        }
        closeStepMenu()
        dialSheetOpen = false
        let slices = slices
        pad?.retire()
        pad = FrequencyPadModel(sliceId: sliceId, letter: entry.slice.letter, colour: entry.slice.colour,
                                tune: { hz in await slices.enter(hz, sliceId: sliceId) },
                                tuneWithLate: { hz, late in await slices.enter(hz, sliceId: sliceId, onLateOutcome: late) },
                                readCurrent: { [weak slices] in
                                    slices?.entries.first { $0.id == sliceId }?.slice.frequencyHz
                                }, close: { [weak self] in self?.closePad() })
    }

    func closePad() {
        pad?.retire()
        pad = nil
    }
}
