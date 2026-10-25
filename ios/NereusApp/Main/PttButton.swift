// NereusSDR for iOS: the PTT on the waterfall: a toggle, its clock while keyed, and who holds transmit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, press and hold with the dashed ring
// and "Hold" while the Roundtable sheet is open. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 18, a touch on the tap PTT that moves
// past tapSlop is a swipe and keys nothing. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave: the tap PTT reads its finger on a UIKit
// surface (PttTap), so a cancelled touch never leaves a swipe behind to
// swallow the next tap; a quick tap on a keyed hold PTT unkeys (.unkey);
// VoiceOver keys and unkeys the hold PTT with its action. J.J. Boyd
// (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave re-review: a quick tap on the hold PTT
// unkeys whenever the sheet shows it keyed (holdKeyed, keying included),
// and both PTTs take a touch only inside their circle (inFace). J.J. Boyd
// (KG4VCF), AI-assisted via Claude Code.

import NereusLink
import SwiftUI
import UIKit

/// The PTT (spec section 5.1 items 12 and 13, D8, D51): a round toggle at
/// the bottom left of the waterfall, in the MOX button's red. One tap keys
/// and a second unkeys. Keyed it turns full red with the transmission's
/// clock under TX; while another device holds transmit it names that
/// device ("Radio" after the radio's own PTT) in muted red, full red with a
/// ring while that device is on the air; while transmit changes hands it
/// waits with a turning ring. After a lost link it reads Tap again: nothing
/// keys by itself.
///
/// While the Roundtable sheet is open (spec 11.3, U2) it is press and
/// hold (``Mode/hold(diameter:)``): a dashed ring and "Hold", keyed only
/// while a finger rests on it (``PttHold``). A swipe, a scroll or a drag
/// never keys; letting go, a cancelled touch and the button leaving the
/// screen unkey. A quick tap while this device's key is on unkeys it, so
/// a key made with a tap or TUNE never needs a press to come off.
struct PttButton: View {
    @ObservedObject var transmit: TransmitModel
    /// The session is up: PTT can be used.
    let linkUp: Bool
    /// Tap to toggle (the band's), or press and hold at `diameter` (the Roundtable sheet's).
    var mode: Mode = .tap
    /// Told each press and release the hold makes (the UI tests' record).
    var holdObserver: ((PttHold.Action) -> Void)?

    enum Mode: Equatable {
        case tap
        case hold(diameter: CGFloat)
    }

    /// The hold PTT beside the Roundtable sheet's message box.
    static let holdDiameter: CGFloat = 58
    /// Its clear space to the message box.
    static let holdClearance: CGFloat = 24

    static let diameter: CGFloat = 76
    /// How far a touch on the tap PTT may move and still be a tap. Past
    /// it the touch is a swipe or a drag, and lifting keys nothing (spec
    /// 11.3: a swipe never keys), even when it lifts on the button
    /// (``PttTap``).
    static let tapSlop: CGFloat = 10
    /// Its inset from the waterfall's bottom left corner.
    static let inset: CGFloat = 14
    /// Its inset from the band's foot.
    static let bottomInset: CGFloat = 12

    /// Where it sits on a band of `bandSize`, `leadingInset` in from the phone's rounded edge.
    static func frame(bandSize: CGSize, leadingInset: CGFloat) -> CGRect {
        CGRect(x: inset + leadingInset, y: bandSize.height - bottomInset - diameter, width: diameter, height: diameter)
    }

    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        switch mode {
        case .tap:
            // A finger lands on the surface over the face, which tells a tap
            // from a swipe. VoiceOver and the UI tests see a plain button
            // instead (it draws nothing and takes no touch), so a PTT with no
            // link reads as not enabled.
            face
                .overlay {
                    PttTapSurface(enabled: linkUp) { transmit.tapPtt() }
                        .clipShape(Circle())
                }
                // Never hidden: with no link it cannot be used; on a Core without
                // remote transmit a tap says why.
                .disabled(!linkUp)
                .opacity(linkUp && transmit.offered ? 1 : 0.4)
                .accessibilityRepresentation {
                    Button("Push to talk, tap to key and tap again to unkey") {
                        transmit.tapPtt()
                    }
                    .disabled(!linkUp)
                }
                .accessibilityValue(spoken)
                .accessibilityAddTraits(look == .keyed ? .isSelected : [])
                .accessibilityIdentifier("ptt")
        case .hold:
            face
                .overlay {
                    PttHoldSurface(enabled: linkUp, keyed: Self.holdKeyed(transmit.ptt)) { action in
                        Self.perform(action, on: transmit)
                        holdObserver?(action)
                    }
                    .clipShape(Circle())
                }
                .opacity(linkUp && transmit.offered ? 1 : 0.4)
                // Leaving the screen ends the press.
                .onDisappear { transmit.releaseHeldPtt() }
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("Push to talk, press and hold to talk")
                .accessibilityValue(spoken)
                .accessibilityHint(linkUp ? Self.holdHint : "")
                .accessibilityAddTraits(look == .keyed ? [.isButton, .isSelected] : .isButton)
                .accessibilityRespondsToUserInteraction(linkUp)
                // VoiceOver cannot rest a finger: its activate keys, and the next unkeys.
                .accessibilityAction {
                    guard linkUp else {
                        return
                    }
                    Self.accessibilityActivate(transmit, keyed: Self.holdKeyed(transmit.ptt))
                }
                .accessibilityIdentifier("ptt")
        }
    }

    /// What a hold's action does to transmit.
    static func perform(_ action: PttHold.Action, on transmit: TransmitModel) {
        switch action {
        case .press:
            transmit.holdPtt()
        case .release:
            transmit.releaseHeldPtt()
        case .unkey:
            transmit.unkeyFromHold()
        }
    }

    /// This device's key is on, on its way or ending: the Roundtable sheet
    /// is keyed (its keyed foot and layout), and a quick tap on the hold PTT
    /// unkeys. One state for both, so the PTT the sheet shows to unkey is
    /// never inert while the key is still coming up.
    static func holdKeyed(_ ptt: PttController.Snapshot) -> Bool {
        ptt.transmitting
    }

    /// A point inside the round face of a PTT drawn in `bounds`: the
    /// square's corners, over the waterfall, take no touch.
    static func inFace(_ point: CGPoint, of bounds: CGRect) -> Bool {
        let radius = min(bounds.width, bounds.height) / 2
        return hypot(point.x - bounds.midX, point.y - bounds.midY) <= radius
    }

    /// What VoiceOver's activate does on the hold PTT.
    static let holdHint = "Double-tap to talk, and double-tap again to stop."

    /// VoiceOver's activate on the hold PTT: unkeys this device's key when
    /// one is on or the hold owns one on its way, and otherwise keys and
    /// holds it until the next activate, the sheet closing, the scene or
    /// the link.
    static func accessibilityActivate(_ transmit: TransmitModel, keyed: Bool) {
        if keyed || transmit.holding {
            transmit.unkeyFromHold()
        } else {
            transmit.holdPtt()
        }
    }

    private var diameter: CGFloat {
        switch mode {
        case .tap:
            return Self.diameter
        case .hold(let diameter):
            return diameter
        }
    }

    private var holds: Bool { mode != .tap }

    /// How the button looks.
    enum Look: Equatable {
        case ready
        case keyed
        case elsewhere(onAir: Bool)
        case waiting
    }

    /// The words on the button: the big one and the small one under it.
    struct Words: Equatable {
        let label: String
        let sub: String
    }

    var look: Look {
        Self.look(transmit.ptt)
    }

    static func look(_ ptt: PttController.Snapshot) -> Look {
        switch ptt.state {
        case .keyed, .ending:
            return .keyed
        case .keying, .unkeying, .waiting:
            return .waiting
        case .heldElsewhere(_, let onAir):
            return .elsewhere(onAir: onAir)
        case .idle, .refused, .linkLost:
            return .ready
        }
    }

    /// The hold PTT's words: as the tap PTT's, but "Hold" where that says "Tap".
    static func holdWords(_ ptt: PttController.Snapshot, clock: String) -> Words {
        let words = words(ptt, clock: clock)
        return words.sub == "Tap" ? Words(label: words.label, sub: "Hold") : words
    }

    /// The words for a snapshot, with the clock's text for a keyed one.
    static func words(_ ptt: PttController.Snapshot, clock: String) -> Words {
        switch ptt.state {
        case .keyed:
            return Words(label: ptt.keyKind == .tune || ptt.keyKind == .tunerTune ? "TUNE" : "TX", sub: clock)
        case .ending:
            return Words(label: "TX", sub: "ending")
        case .keying, .unkeying, .waiting:
            return Words(label: "PTT", sub: "Wait")
        case .heldElsewhere(let device, let onAir):
            return Words(label: onAir ? "TX" : "PTT", sub: device)
        case .idle, .refused, .linkLost:
            return Words(label: "PTT", sub: "Tap")
        }
    }

    /// A transmission's clock: minutes and seconds.
    static func clock(seconds: Int) -> String {
        let whole = max(0, seconds)
        return "\(whole / 60):" + String(format: "%02d", whole % 60)
    }

    private func words(clock: String) -> Words {
        holds ? Self.holdWords(transmit.ptt, clock: clock) : Self.words(transmit.ptt, clock: clock)
    }

    private var spoken: String {
        let words = words(clock: Self.clock(seconds: Int(transmit.keyedForSeconds)))
        return words.label + ", " + words.sub
    }

    @ViewBuilder
    private var face: some View {
        TimelineView(.periodic(from: .now, by: 1)) { _ in
            let words = words(clock: Self.clock(seconds: elapsedSeconds))
            let scale = diameter / Self.diameter
            ZStack {
                if look == .keyed {
                    KeyedFace()
                } else {
                    Circle().fill(ground)
                    if holds {
                        // Press and hold: the dashed ring (U2).
                        Circle().strokeBorder(edge, style: StrokeStyle(lineWidth: 1.5, dash: Self.holdDash))
                    } else {
                        Circle().strokeBorder(edge, lineWidth: 1)
                    }
                }
                VStack(spacing: 2) {
                    Text(words.label)
                        .font(.system(size: max(14, (look == .ready || look == .keyed ? 18 : 16) * scale),
                                      weight: .heavy))
                        .tracking(1)
                        .foregroundStyle(labelColour)
                    Text(words.sub)
                        .font(.system(size: 11, weight: .semibold).monospacedDigit())
                        .foregroundStyle(subColour)
                        .lineLimit(1)
                        .minimumScaleFactor(0.6)
                        .padding(.horizontal, 8)
                }
            }
            .frame(width: diameter, height: diameter)
            .overlay {
                if look == .keyed || look == .elsewhere(onAir: true) {
                    Circle().stroke(ChromeColours.pttEdge.opacity(0.25), lineWidth: 3).padding(-3)
                }
                if look == .waiting {
                    TurningRing(reduceMotion: reduceMotion)
                }
            }
            .shadow(color: .black.opacity(0.5), radius: 8, y: 6)
            .contentShape(Circle())
        }
    }

    /// The hold PTT's dashed ring.
    static let holdDash: [CGFloat] = [5, 4]

    /// Seconds on the air: from the PTT's own key, else the Core's count.
    private var elapsedSeconds: Int {
        Self.keyedSeconds(since: Self.keyedSince(transmit.ptt), fallback: Int(transmit.keyedForSeconds))
    }

    /// When this device's key went on, while it is on.
    static func keyedSince(_ ptt: PttController.Snapshot) -> ContinuousClock.Instant? {
        if case .keyed(let since) = ptt.state {
            return since
        }
        return nil
    }

    /// Seconds on the air at `now`: from the key's start, else `fallback`
    /// (the Core's count). The PTT and the Roundtable TX line both read it,
    /// each in its own timeline, so neither waits for a parent to redraw.
    static func keyedSeconds(since: ContinuousClock.Instant?, fallback: Int,
                             now: ContinuousClock.Instant = .now) -> Int {
        guard let since else {
            return fallback
        }
        return Int((now - since).components.seconds)
    }

    /// At rest, the danger set's ground with the transmit red edge; keyed,
    /// the keyed treatment (``KeyedFace``); elsewhere, the muted red-grey
    /// (docs/architecture/2026-10-20-phone-workspace-look-design.md, section 9).
    private var ground: Color {
        switch look {
        case .ready:
            return ChromeColours.pttGround
        case .keyed:
            return ChromeColours.txRed
        case .elsewhere, .waiting:
            return ChromeColours.pttElsewhereGround
        }
    }

    private var edge: Color {
        switch look {
        case .ready, .keyed, .elsewhere(onAir: true):
            return ChromeColours.pttEdge
        case .elsewhere, .waiting:
            return ChromeColours.pttElsewhereEdge
        }
    }

    private var labelColour: Color {
        switch look {
        case .ready, .elsewhere(onAir: true):
            return ChromeColours.pttText
        case .keyed:
            return workspaceForeground(.keyed)
        case .elsewhere, .waiting:
            return ChromeColours.pttElsewhereText
        }
    }

    private var subColour: Color {
        switch look {
        case .ready:
            return ChromeColours.pttSub
        case .keyed:
            return workspaceForeground(.keyed)
        case .elsewhere(onAir: true):
            return ChromeColours.pttElsewhereAirSub
        case .elsewhere:
            // A holder that's away is greyed.
            return transmit.report.holderAway ? ChromeColours.textFaint : ChromeColours.pttElsewhereSub
        case .waiting:
            return ChromeColours.text
        }
    }
}

/// The keyed PTT's face: the keyed treatment drawn on the round button,
/// kTxRed with the lighter top and its rim
/// (docs/architecture/2026-10-20-phone-workspace-look-design.md, section 5).
/// ``WorkspaceFace`` is a rounded rectangle; this is the same fill on a circle.
private struct KeyedFace: View {
    var body: some View {
        Circle().fill(WorkspacePalette.txRed)
            .overlay {
                Circle().fill(LinearGradient(colors: [WorkspacePalette.glassLight, .clear],
                                             startPoint: .top, endPoint: .bottom))
            }
            .overlay { Circle().strokeBorder(WorkspacePalette.keyedRim, lineWidth: 1) }
    }
}

/// The turning ring while PTT waits (the board's `.ptt.is-moving`).
private struct TurningRing: View {
    let reduceMotion: Bool
    @State private var turned = false

    var body: some View {
        Circle()
            .trim(from: 0, to: 0.25)
            .stroke(ChromeColours.pttText, style: StrokeStyle(lineWidth: 3, lineCap: .round))
            .padding(-5)
            .rotationEffect(.degrees(turned ? 360 : 0))
            .animation(reduceMotion ? nil : .linear(duration: 0.9).repeatForever(autoreverses: false), value: turned)
            .onAppear { turned = true }
    }
}

/// Press and hold, as the Roundtable sheet's PTT reads a finger (spec
/// 11.3, U2): the key goes on only once a finger has rested on the button
/// for ``delay`` without moving past ``slop``, and comes off when it lifts,
/// when its touch is cancelled (a scroll or the system taking it), or when
/// it slides off the button. A touch that moves first is spent: it never
/// keys, even if it comes to rest later. A drag that starts elsewhere
/// never reaches the button's touches at all. While this device's key is
/// on (a tap's, TUNE's), a finger that lifts before the delay without
/// moving unkeys it (``Action/unkey``).
struct PttHold: Equatable {
    enum Action: Equatable {
        case press
        case release
        /// A quick tap while the key was on: unkey it.
        case unkey
    }

    enum Phase: Equatable {
        case idle
        /// A finger is down at `start` since `since`, not yet held long
        /// enough; `keyed` says this device's key was on as it landed.
        case pressing(start: CGPoint, since: TimeInterval, keyed: Bool)
        /// The key is on for this press.
        case held
        /// This touch moved first, or slid off: nothing more until it lifts.
        case spent
    }

    /// How long a finger rests before the key goes on.
    static let delay: TimeInterval = 0.15
    /// How far a finger may move before the press is a swipe.
    static let slop: CGFloat = 10

    private(set) var phase: Phase = .idle

    /// A finger down at `point`, `keyed` while this device's key is on; a
    /// second finger is ignored.
    mutating func down(at point: CGPoint, time: TimeInterval, keyed: Bool = false) -> Action? {
        guard phase == .idle else {
            return nil
        }
        phase = .pressing(start: point, since: time, keyed: keyed)
        return nil
    }

    /// The finger moved to `point`; `inside` says whether it is still over
    /// the button (its bounds grown by ``slop``).
    mutating func move(to point: CGPoint, inside: Bool) -> Action? {
        switch phase {
        case .pressing(let start, _, _):
            if hypot(point.x - start.x, point.y - start.y) > Self.slop || !inside {
                phase = .spent
            }
            return nil
        case .held:
            guard inside else {
                phase = .spent
                return .release
            }
            return nil
        case .idle, .spent:
            return nil
        }
    }

    /// Time passed: a finger at rest long enough keys.
    mutating func tick(time: TimeInterval) -> Action? {
        guard case .pressing(_, let since, _) = phase, time - since >= Self.delay else {
            return nil
        }
        phase = .held
        return .press
    }

    /// The finger lifted: a held press lets go, and a quick tap on a key
    /// that was on unkeys it.
    mutating func up() -> Action? {
        let was = phase
        phase = .idle
        switch was {
        case .held:
            return .release
        case .pressing(_, _, let keyed):
            return keyed ? .unkey : nil
        case .idle, .spent:
            return nil
        }
    }

    /// The touch was cancelled, or the button left the screen: a held press
    /// lets go; anything else is forgotten.
    mutating func cancel() -> Action? {
        let held = phase == .held
        phase = .idle
        return held ? .release : nil
    }
}

/// The finger's surface for ``PttHold``: a UIKit view, so a cancelled
/// touch (a scroll, a sheet's drag, a system gesture, the app leaving the
/// screen) arrives as its own event.
struct PttHoldSurface: UIViewRepresentable {
    let enabled: Bool
    /// This device's key is on: a quick tap unkeys it.
    var keyed = false
    let act: (PttHold.Action) -> Void

    func makeUIView(context: Context) -> PttHoldView {
        let view = PttHoldView()
        view.act = act
        view.enabled = enabled
        view.keyed = keyed
        return view
    }

    func updateUIView(_ view: PttHoldView, context: Context) {
        view.act = act
        view.enabled = enabled
        view.keyed = keyed
    }

    static func dismantleUIView(_ view: PttHoldView, coordinator: ()) {
        view.end()
    }
}

/// See ``PttHoldSurface``.
final class PttHoldView: UIView {
    var act: (PttHold.Action) -> Void = { _ in }
    var enabled = true {
        didSet {
            if !enabled {
                end()
            }
        }
    }
    /// This device's key is on: a quick tap unkeys it.
    var keyed = false
    private var hold = PttHold()
    private var timer: Timer?

    /// Where the press stands (the tests).
    var phase: PttHold.Phase { hold.phase }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = false
        backgroundColor = .clear
    }

    /// Only the round face takes a touch, not the square's corners.
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        PttButton.inFace(point, of: bounds)
    }

    required init?(coder: NSCoder) {
        nil
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard enabled, let touch = touches.first else {
            return
        }
        begin(at: touch.location(in: self), time: touch.timestamp)
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: PttHold.delay, repeats: false) { [weak self] _ in
            MainActor.assumeIsolated {
                self?.tick(time: ProcessInfo.processInfo.systemUptime)
            }
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = touches.first else {
            return
        }
        move(to: touch.location(in: self))
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        lift()
    }

    /// A finger landed at `point` (touchesBegan, and the tests).
    func begin(at point: CGPoint, time: TimeInterval) {
        guard enabled else {
            return
        }
        perform(hold.down(at: point, time: time, keyed: keyed))
    }

    /// Time passed with the finger down (the timer, and the tests).
    func tick(time: TimeInterval) {
        perform(hold.tick(time: time))
    }

    /// The finger moved to `point` (touchesMoved, and the tests).
    func move(to point: CGPoint) {
        let inside = bounds.insetBy(dx: -PttHold.slop, dy: -PttHold.slop).contains(point)
        perform(hold.move(to: point, inside: inside))
    }

    /// The finger lifted (touchesEnded, and the tests).
    func lift() {
        timer?.invalidate()
        timer = nil
        perform(hold.up())
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        end()
    }

    override func willMove(toWindow newWindow: UIWindow?) {
        super.willMove(toWindow: newWindow)
        if newWindow == nil {
            end()
        }
    }

    /// Ends a press, whatever ended it.
    func end() {
        timer?.invalidate()
        timer = nil
        perform(hold.cancel())
    }

    private func perform(_ action: PttHold.Action?) {
        if let action {
            act(action)
        }
    }
}

/// A tap on the tap PTT, as its surface reads a finger (spec 11.3: a swipe
/// never keys): a finger that lifts without having moved past
/// ``PttButton/tapSlop`` is a tap; one that moved is spent until it lifts.
/// A cancelled touch (a call, Control Center, a notification, the button
/// leaving the screen) is forgotten, and every new touch starts fresh, so
/// nothing a touch leaves behind can swallow the next tap, the one that
/// may be unkeying the radio.
struct PttTap: Equatable {
    enum Phase: Equatable {
        case idle
        /// A finger is down at `start` and has not moved past the slop.
        case pressing(start: CGPoint)
        /// This touch moved past the slop: its lift is not a tap.
        case spent
    }

    private(set) var phase: Phase = .idle

    /// A finger down at `point`: a new touch, whatever the last one left.
    mutating func down(at point: CGPoint) {
        phase = .pressing(start: point)
    }

    /// The finger moved to `point`.
    mutating func move(to point: CGPoint) {
        if case .pressing(let start) = phase,
           hypot(point.x - start.x, point.y - start.y) > PttButton.tapSlop {
            phase = .spent
        }
    }

    /// The finger lifted: true for a tap.
    mutating func up() -> Bool {
        let tapped: Bool
        if case .pressing = phase {
            tapped = true
        } else {
            tapped = false
        }
        phase = .idle
        return tapped
    }

    /// The touch was cancelled, or the button left the screen.
    mutating func cancel() {
        phase = .idle
    }
}

/// The finger's surface for ``PttTap``: a UIKit view, so a cancelled touch
/// arrives as its own event and resets the tap.
struct PttTapSurface: UIViewRepresentable {
    let enabled: Bool
    let tap: () -> Void

    func makeUIView(context: Context) -> PttTapView {
        let view = PttTapView()
        view.tap = tap
        view.enabled = enabled
        return view
    }

    func updateUIView(_ view: PttTapView, context: Context) {
        view.tap = tap
        view.enabled = enabled
    }

    static func dismantleUIView(_ view: PttTapView, coordinator: ()) {
        view.end()
    }
}

/// See ``PttTapSurface``.
final class PttTapView: UIView {
    var tap: () -> Void = {}
    var enabled = true {
        didSet {
            if !enabled {
                end()
            }
        }
    }
    private var reader = PttTap()

    /// Where the touch stands (the tests).
    var phase: PttTap.Phase { reader.phase }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = false
        backgroundColor = .clear
    }

    /// Only the round face takes a touch, not the square's corners.
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        PttButton.inFace(point, of: bounds)
    }

    required init?(coder: NSCoder) {
        nil
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = touches.first else {
            return
        }
        begin(at: touch.location(in: self))
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = touches.first else {
            return
        }
        move(to: touch.location(in: self))
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        lift()
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        end()
    }

    override func willMove(toWindow newWindow: UIWindow?) {
        super.willMove(toWindow: newWindow)
        if newWindow == nil {
            end()
        }
    }

    /// A finger landed at `point` (touchesBegan, and the tests).
    func begin(at point: CGPoint) {
        guard enabled else {
            return
        }
        reader.down(at: point)
    }

    /// The finger moved to `point` (touchesMoved, and the tests).
    func move(to point: CGPoint) {
        reader.move(to: point)
    }

    /// The finger lifted (touchesEnded, and the tests): a tap taps.
    func lift() {
        if reader.up() && enabled {
            tap()
        }
    }

    /// The touch ended without a tap, whatever ended it.
    func end() {
        reader.cancel()
    }
}
