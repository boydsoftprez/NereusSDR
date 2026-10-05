// NereusSDR for iOS: what a flag's buttons open and run: its antenna and more menus, its tab panels, each on its own slice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror

/// One of the four tabs along a flag's foot, as the redrawn flag has
/// them (the board's `#flagbtn-review`): VAX sits at the foot of the
/// sound panel, and the step at the foot of the X/RIT panel.
enum FlagTab: String, CaseIterable, Identifiable {
    case audio
    case dsp
    case mode
    case xrit

    var id: String { rawValue }

    /// The panel's title, the board's.
    var title: String {
        switch self {
        case .audio:
            return "Audio"
        case .dsp:
            return "DSP"
        case .mode:
            return "Mode"
        case .xrit:
            return "X/RIT"
        }
    }

    /// The desktop flag's tooltip for the tab (VfoWidget.cpp kTabTooltips).
    var desktopTip: String {
        switch self {
        case .audio:
            return "Show/hide audio controls (AF gain, AGC, pan, mute, squelch)"
        case .dsp:
            return "Show/hide DSP controls (NB, NR, ANF, SNB, APF)"
        case .mode:
            return "Show/hide mode and filter controls"
        case .xrit:
            return "Show/hide RIT/XIT and frequency-lock controls"
        }
    }
}

/// What a flag has open over the band: at most one menu or panel at a time.
enum FlagPopover: Equatable {
    /// The antenna button's menu: RX antenna, TX antenna and BYPS.
    case antennas(Int)
    /// The more button's menu: the desktop flag's right-click menu.
    case more(Int)
    /// A tab's panel, dropping from the flag across the band.
    case panel(Int, FlagTab)

    var sliceId: Int {
        switch self {
        case .antennas(let id), .more(let id), .panel(let id, _):
            return id
        }
    }
}

/// The flag's buttons (R-IOS-11, the board's "Flag controls at finger
/// size", look 2): each acts on its own slice. The antenna menu and the
/// tab panels show the Modes tab's controls, which follow the active
/// slice, so opening one on another slice's flag makes that slice active
/// first, as the board does, and its controls show once the Core says so.
/// TX, the step, the frequency, lock, close and the sample rate act on
/// their slice by its id. A tap on the open tab, or anywhere else on the
/// band, closes what is open.
@MainActor
final class FlagControls: ObservableObject {
    /// The open menu or panel, if any.
    @Published private(set) var open: FlagPopover?
    /// The more menu's Sample rate list is out.
    @Published var rateListOpen = false
    /// Diversity, from the more menu, is up.
    @Published var diversityOpen = false
    @Published var diversity: DiversityModel?
    @Published private(set) var diversitySliceId: Int?
    /// The slice whose step menu is open under its flag's step (not the dial's).
    @Published private(set) var stepOpenSliceId: Int?
    /// RX bypass on transmit is on: the antenna button says BYPS.
    @Published private(set) var bypassOn = false
    /// The open more menu hangs under the band's WIDE chip, which opened
    /// it, rather than under its flag's more button (``WideChip``).
    @Published private(set) var moreFromWideChip = false
    /// The WIDE chip's target on the band while it shows; nil while it is hidden.
    @Published var wideChipRect: CGRect?

    let slices: BandSlicesModel
    /// The Modes tab's model, whose controls the antenna menu and the
    /// panels show; nil in the flag pictures, where nothing opens.
    let modes: ModesTabModel?
    let tuning: BandTuningModel?
    /// The mirror, for the VAX panel's channels.
    let store: MirrorStore?
    private var watches: Set<AnyCancellable> = []

    /// The phone's words for greyed flag buttons (the board's).
    static let sliceAStaysOpenText = "Slice A always stays open."
    static let noBypassText = "This radio has no BYPS switch."
    /// The desktop flag's own name for its bypass switch (VfoWidget.cpp),
    /// the same on every radio: not a radio's input label.
    static let bypassTitle = "BYPS"
    /// The desktop flag's words for BYPS (VfoWidget.cpp rxBypassToolTip).
    static let bypassTip = "RX Bypass on TX: routes the receive path through the bypass relay while transmitting."
    /// Why the VAX panel's channels are greyed: the Core lists each
    /// channel's slices but takes no choice of channel from a phone.
    static let vaxChoiceText = "A slice's VAX channel is chosen on the desktop."
    /// Why the more menu's Filter policy is greyed: the phone has no page for it.
    static let filterPolicyText = "Filter policy is set on the desktop."

    /// The object whose `rxFilter0*` properties the Filter policy lines read.
    static let radioKey = "radio"
    /// The desktop's Filter Policy dialog's words for the input's state
    /// (FilterPolicyDialog.cpp, its Current state group, as a window of a
    /// Core shows it).
    enum FilterState {
        static let unavailable = "Core filter state is not available."
        static let filtered = "Filtered"
        static let wideband = "BYPASS (wideband)"
        static let bypass = "BYPASS"
        static func effective(_ text: String) -> String { "Effective: \(text)" }
        static func reason(_ text: String) -> String { "Reason: \(text)" }
        static func sharedInput(_ text: String) -> String { "Shared input: \(text)" }
    }

    /// The Filter policy lines: the effective state, the Core's reason and,
    /// when the Core sends one, its words for the shared input's low-pass.
    static func filterStateLines(_ state: ReceiveFilterState?) -> [String] {
        guard let state else {
            return [FilterState.unavailable]
        }
        let effective: String
        switch state.effective {
        case .filtered:
            effective = FilterState.filtered
        case .widebandLocked:
            effective = FilterState.wideband
        case .bypass:
            effective = FilterState.bypass
        }
        var lines = [FilterState.effective(effective), FilterState.reason(state.reason)]
        if let lowPass = state.lowPassReason {
            lines.append(FilterState.sharedInput(lowPass))
        }
        return lines
    }

    /// The desktop flag's right-click menu, in its order and words
    /// (VfoWidget.cpp, the flag's context menu).
    enum MoreItem {
        static let makeTransmit = "Make this the TX slice"
        static let antenna = "Antenna >"
        static let sampleRate = "Sample rate >"
        static let diversity = "Diversity..."
        static let filterPolicy = "Filter policy..."
        static let remove = "Remove slice"
    }

    /// The desktop flag's tooltips for its buttons (VfoWidget.cpp).
    enum DesktopTip {
        static let close = "Close slice"
        static let lock = "Keeps the VFO from changing while in the middle of a QSO."
        static let rxAntenna = "Toggles receive antenna between RX and TX antennas for RX1"
        static let txAntenna = "Select TX antenna"
        static let transmit = "Indicates this slice is the TX slice"
    }

    init(slices: BandSlicesModel, modes: ModesTabModel?, tuning: BandTuningModel?, store: MirrorStore?) {
        self.slices = slices
        self.modes = modes
        self.tuning = tuning
        self.store = store
        if let tuning {
            tuning.$stepMenuSliceId.combineLatest(tuning.$stepMenuFromDial)
                .map { id, fromDial in fromDial ? nil : id }
                .removeDuplicates()
                .sink { [weak self] in self?.stepOpenSliceId = $0 }
                .store(in: &watches)
        }
        if let modes {
            modes.$bypass.map { $0 == true }.removeDuplicates()
                .sink { [weak self] in self?.bypassOn = $0 }
                .store(in: &watches)
        }
    }

    /// Nothing opens: the flag pictures, which draw the buttons alone.
    var inert: Bool { modes == nil }

    /// Whether `popover` is the one open.
    func isOpen(_ popover: FlagPopover) -> Bool {
        open == popover
    }

    /// The tab lit on `sliceId`'s flag: the one whose panel is open.
    func openTab(_ sliceId: Int) -> FlagTab? {
        if case .panel(sliceId, let tab)? = open {
            return tab
        }
        return nil
    }

    /// A menu button or a tab: opens it, making its slice active first;
    /// pressed again, it closes. Whatever else was open closes.
    func toggle(_ popover: FlagPopover) {
        guard !inert else {
            return
        }
        if open == popover, !moreFromWideChip {
            close()
            return
        }
        tuning?.closeStepMenu()
        tuning?.closePad()
        modes?.closePad()
        rateListOpen = false
        moreFromWideChip = false
        slices.activate(popover.sliceId)
        open = popover
    }

    /// The band's WIDE chip: the active slice's more menu, under the chip
    /// (the board's `#widechip-review`); pressed again, it closes. With
    /// that menu open under the flag's more button, it moves under the chip.
    func toggleFromWideChip() {
        guard !inert, let id = slices.activeSliceId else {
            return
        }
        let popover = FlagPopover.more(id)
        if open == popover {
            if moreFromWideChip {
                close()
            } else {
                rateListOpen = false
                moreFromWideChip = true
            }
            return
        }
        toggle(popover)
        moreFromWideChip = open == popover
    }

    /// Closes the open menu or panel, and any number pad it opened.
    func close() {
        guard open != nil || rateListOpen else {
            return
        }
        open = nil
        rateListOpen = false
        moreFromWideChip = false
        modes?.closePad()
    }

    /// Whether the open popover's controls are its slice's: the Modes
    /// tab's controls follow the active slice, so they show once the Core
    /// has made the popover's slice active.
    func showsControls(for sliceId: Int) -> Bool {
        slices.activeSliceId == sliceId
    }

    // MARK: The buttons' actions, each on its own slice

    /// The desktop's step ladder, in hertz, which its flag's step cycle
    /// button climbs one rung a tap and wraps (NereusSDR desktop,
    /// src/models/SliceModel.h `kStageOneStepLadder`, and MainWindow.cpp's
    /// `stepCycleRequested` handler).
    static let stepLadder: [Double] = [1, 10, 100, 500, 1000, 10000]

    /// The step after `current` on the ladder, wrapping; the ladder's
    /// first rung for a step that is not on it, as the desktop does.
    static func nextStep(after current: Double?) -> Double {
        guard let current, let index = stepLadder.firstIndex(of: current.rounded()) else {
            return stepLadder[0]
        }
        return stepLadder[(index + 1) % stepLadder.count]
    }

    /// The step cycle button's words, the desktop's "%1 Hz".
    static func stepCycleText(_ hz: Double?) -> String {
        guard let hz else {
            return "Step"
        }
        return "\(Int(hz.rounded())) Hz"
    }

    /// The listened sound panel's Stop listening: the phone leaves the
    /// slice (and its band, if shown), and the panel closes.
    func stopListening(_ sliceId: Int) {
        close()
        let slices = slices
        Task { @MainActor in
            if let words = await slices.stopListening(sliceId) {
                slices.showReason(words)
            }
        }
    }

    /// The X/RIT panel's step cycle: the slice's step one rung up the ladder.
    func cycleStep(_ entry: BandSlicesModel.Entry) {
        slices.setStep(Self.nextStep(after: entry.stepHz), sliceId: entry.id)
    }

    /// The step button: the step menu under it.
    func openStep(_ sliceId: Int) {
        close()
        guard let tuning else {
            return
        }
        if tuning.stepMenuSliceId == sliceId, !tuning.stepMenuFromDial {
            tuning.closeStepMenu()
        } else {
            tuning.openStepMenu(sliceId: sliceId)
        }
    }

    /// The lock: the slice's lock, which the Core decides.
    func toggleLock(_ entry: BandSlicesModel.Entry) {
        close()
        slices.setLocked(!entry.locked, sliceId: entry.id)
    }

    /// The close button and Remove slice: the Core closes the slice; slice
    /// A always stays, and says so.
    func closeSlice(_ sliceId: Int) {
        close()
        guard sliceId != 0 else {
            slices.showReason(Self.sliceAStaysOpenText)
            return
        }
        slices.close(sliceId)
    }

    /// TX and Make this the TX slice: the guarded transmit choice, or,
    /// while this phone may not transmit, the reason.
    func makeTransmit(_ sliceId: Int, reason: String?) {
        close()
        if let reason {
            slices.showReason(reason)
            return
        }
        slices.selectForTransmit(sliceId)
    }

    /// One of the Sample rate list's rates, for the receiver that hears the slice.
    func pickSampleRate(_ rateHz: Int, sliceId: Int) {
        rateListOpen = false
        slices.requestSampleRate(rateHz, sliceId: sliceId)
    }

    /// Diversity..., from the more menu.
    func openDiversity() {
        close()
        diversitySliceId = diversity?.state?.live?.identity.sliceId
        diversityOpen = true
    }

    /// A DIV badge opens the exact authoritative live context without selecting or tuning its slice.
    func openDiversity(_ entry: BandSlicesModel.Entry) {
        guard entry.diversityOn, diversity?.badgeMatches(entry.id, incarnation: entry.access?.incarnation) == true else { return }
        close()
        diversitySliceId = entry.id
        diversityOpen = true
    }

    func setDiversity(_ entry: BandSlicesModel.Entry) {
        guard let diversity else { return }
        diversity.setTarget(entry.diversityOn ? nil : entry.id, enabled: !entry.diversityOn)
    }


    /// The VAX channel the Core feeds the slice lettered `letter` to: the
    /// one whose slices list it, or 0 for Off, from the `vax` object's values.
    static func vaxChannel(values: [String: MirrorValue], letter: String) -> Int {
        StationVax(values: values).channels.first { $0.slices.contains(letter) }?.number ?? 0
    }
}
