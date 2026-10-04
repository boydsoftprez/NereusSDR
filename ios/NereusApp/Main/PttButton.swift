// NereusSDR for iOS: the PTT on the waterfall: a toggle, its clock while keyed, and who holds transmit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import SwiftUI

/// The PTT (spec section 5.1 items 12 and 13, D8, D51): a round toggle at
/// the bottom left of the waterfall, in the MOX button's red. One tap keys
/// and a second unkeys. Keyed it turns full red with the transmission's
/// clock under TX; while another device holds transmit it names that
/// device ("Radio" after the radio's own PTT) in muted red, full red with a
/// ring while that device is on the air; while transmit changes hands it
/// waits with a turning ring. After a lost link it reads Tap again: nothing
/// keys by itself.
struct PttButton: View {
    @ObservedObject var transmit: TransmitModel
    /// The session is up: PTT can be used.
    let linkUp: Bool

    static let diameter: CGFloat = 76
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
        Button {
            transmit.tapPtt()
        } label: {
            face
        }
        .buttonStyle(.plain)
        // Never hidden: with no link it cannot be used; on a Core without
        // remote transmit a tap says why.
        .disabled(!linkUp)
        .opacity(linkUp && transmit.offered ? 1 : 0.4)
        .accessibilityLabel("Push to talk, tap to key and tap again to unkey")
        .accessibilityValue(spoken)
        .accessibilityAddTraits(look == .keyed ? .isSelected : [])
        .accessibilityIdentifier("ptt")
    }

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

    private var spoken: String {
        let words = Self.words(transmit.ptt, clock: Self.clock(seconds: Int(transmit.keyedForSeconds)))
        return words.label + ", " + words.sub
    }

    @ViewBuilder
    private var face: some View {
        TimelineView(.periodic(from: .now, by: 1)) { _ in
            let words = Self.words(transmit.ptt, clock: Self.clock(seconds: elapsedSeconds))
            ZStack {
                Circle().fill(ground)
                Circle().strokeBorder(edge, lineWidth: 1)
                VStack(spacing: 2) {
                    Text(words.label)
                        .font(.system(size: look == .ready || look == .keyed ? 18 : 16, weight: .heavy))
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
            .frame(width: Self.diameter, height: Self.diameter)
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

    /// Seconds on the air: from the PTT's own key, else the Core's count.
    private var elapsedSeconds: Int {
        if case .keyed(let since) = transmit.ptt.state {
            let parts = (ContinuousClock.now - since).components
            return Int(parts.seconds)
        }
        return Int(transmit.keyedForSeconds)
    }

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
            return .white
        case .elsewhere, .waiting:
            return ChromeColours.pttElsewhereText
        }
    }

    private var subColour: Color {
        switch look {
        case .ready:
            return ChromeColours.pttSub
        case .keyed:
            return .white
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
