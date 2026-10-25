// NereusSDR for iOS: the PTT on the waterfall: a toggle, its clock while keyed, and who holds transmit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, press and hold with the dashed ring
// and "Hold" while the Roundtable sheet is open. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 18, a touch on the tap PTT that moves
// past tapSlop is a swipe and keys nothing. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.

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
/// screen unkey.
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
    /// 11.3: a swipe never keys), even when it lifts on the button.
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
    /// The touch on the tap PTT moved past ``tapSlop``: its lift keys nothing.
    @State private var swiped = false

    var body: some View {
        switch mode {
        case .tap:
            Button {
                // A touch that moved is a swipe, not a tap.
                guard !swiped else {
                    return
                }
                transmit.tapPtt()
            } label: {
                face
            }
            .buttonStyle(.plain)
            .simultaneousGesture(
                DragGesture(minimumDistance: Self.tapSlop)
                    .onChanged { _ in
                        swiped = true
                    }
                    .onEnded { _ in
                        // After the button has seen the lift.
                        DispatchQueue.main.async {
                            swiped = false
                        }
                    }
            )
            // Never hidden: with no link it cannot be used; on a Core without
            // remote transmit a tap says why.
            .disabled(!linkUp)
            .opacity(linkUp && transmit.offered ? 1 : 0.4)
            .accessibilityLabel("Push to talk, tap to key and tap again to unkey")
            .accessibilityValue(spoken)
            .accessibilityAddTraits(look == .keyed ? .isSelected : [])
            .accessibilityIdentifier("ptt")
        case .hold:
            face
                .overlay {
                    PttHoldSurface(enabled: linkUp) { action in
                        switch action {
                        case .press:
                            transmit.holdPtt()
                        case .release:
                            transmit.releaseHeldPtt()
                        }
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
                .accessibilityAddTraits(look == .keyed ? [.isButton, .isSelected] : .isButton)
                .accessibilityRespondsToUserInteraction(linkUp)
                .accessibilityIdentifier("ptt")
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
        if case .keyed(let since) = transmit.ptt.state {
            let parts = (ContinuousClock.now - since).components
            return Int(parts.seconds)
        }
        return Int(transmit.keyedForSeconds)
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
/// never reaches the button's touches at all.
struct PttHold: Equatable {
    enum Action: Equatable {
        case press
        case release
    }

    enum Phase: Equatable {
        case idle
        /// A finger is down at `start` since `since`, not yet held long enough.
        case pressing(start: CGPoint, since: TimeInterval)
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

    /// A finger down at `point`; a second finger is ignored.
    mutating func down(at point: CGPoint, time: TimeInterval) -> Action? {
        guard phase == .idle else {
            return nil
        }
        phase = .pressing(start: point, since: time)
        return nil
    }

    /// The finger moved to `point`; `inside` says whether it is still over
    /// the button (its bounds grown by ``slop``).
    mutating func move(to point: CGPoint, inside: Bool) -> Action? {
        switch phase {
        case .pressing(let start, _):
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
        guard case .pressing(_, let since) = phase, time - since >= Self.delay else {
            return nil
        }
        phase = .held
        return .press
    }

    /// The finger lifted.
    mutating func up() -> Action? {
        let held = phase == .held
        phase = .idle
        return held ? .release : nil
    }

    /// The touch was cancelled, or the button left the screen.
    mutating func cancel() -> Action? {
        up()
    }
}

/// The finger's surface for ``PttHold``: a UIKit view, so a cancelled
/// touch (a scroll, a sheet's drag, a system gesture, the app leaving the
/// screen) arrives as its own event.
struct PttHoldSurface: UIViewRepresentable {
    let enabled: Bool
    let act: (PttHold.Action) -> Void

    func makeUIView(context: Context) -> PttHoldView {
        let view = PttHoldView()
        view.act = act
        view.enabled = enabled
        return view
    }

    func updateUIView(_ view: PttHoldView, context: Context) {
        view.act = act
        view.enabled = enabled
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
    private var hold = PttHold()
    private var timer: Timer?

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = false
        backgroundColor = .clear
    }

    required init?(coder: NSCoder) {
        nil
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard enabled, let touch = touches.first else {
            return
        }
        perform(hold.down(at: touch.location(in: self), time: touch.timestamp))
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: PttHold.delay, repeats: false) { [weak self] _ in
            MainActor.assumeIsolated {
                guard let self else { return }
                self.perform(self.hold.tick(time: ProcessInfo.processInfo.systemUptime))
            }
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = touches.first else {
            return
        }
        let point = touch.location(in: self)
        let inside = bounds.insetBy(dx: -PttHold.slop, dy: -PttHold.slop).contains(point)
        perform(hold.move(to: point, inside: inside))
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        timer?.invalidate()
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
