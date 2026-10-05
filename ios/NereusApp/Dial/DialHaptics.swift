// NereusSDR for iOS: the tuning haptics: a light tick per detent, a firmer bump per whole kilohertz, a soft tick per drag step
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import UIKit

/// The impacts tuning plays.
enum DialImpact: Equatable, Sendable {
    /// Each detent.
    case light
    /// Each whole kilohertz.
    case rigid
    /// Each step a flag or passband drag moves its slice: softer than a detent.
    case soft
}

/// What plays an impact: the phone's Taptic Engine, or a recorder in tests.
@MainActor
protocol DialImpactGenerator: AnyObject {
    /// Wakes the engine, so the first tick is not late.
    func prepare()
    func impact(_ impact: DialImpact)
}

/// The dial's haptics (D12, R-IOS-12, spec section 5.1 item 8): a light
/// impact on each detent and a rigid one on each whole kilohertz, as the
/// dial's events come. A fast spin passes many detents a frame, so the
/// ticks are held to one every ``DialFeel/minimumTickInterval`` (and the
/// bumps likewise, on their own), keeping a spin a train of ticks rather
/// than a buzz; a whole kilohertz passed in a frame still bumps.
@MainActor
final class DialHaptics {
    private let generator: DialImpactGenerator
    private let clock: () -> TimeInterval
    private let interval: TimeInterval
    private var lastLight: TimeInterval?
    private var lastRigid: TimeInterval?
    private var lastSoft: TimeInterval?
    /// The light tick on each detent plays (Setup, Navigation; on by default).
    var ticksOnDetents = true
    /// The firmer bump on each whole kilohertz plays (Setup, Navigation; on by default).
    var bumpsOnKilohertz = true

    /// How hard a drag step's soft tick plays, of the generator's full
    /// intensity: lighter than the dial's detent tick.
    static let dragStepIntensity: CGFloat = 0.5

    init(generator: DialImpactGenerator? = nil, feel: DialFeel = .standard,
         clock: @escaping () -> TimeInterval = { ProcessInfo.processInfo.systemUptime }) {
        self.generator = generator ?? SystemDialImpactGenerator()
        self.clock = clock
        // A millisecond's grace, so frame timing's jitter does not drop a
        // tick that is due.
        interval = feel.minimumTickInterval - 0.001
    }

    func prepare() {
        generator.prepare()
    }

    /// Plays one frame's events: at most one light tick, then at most one
    /// bump, each only if it is switched on and its last one is at least
    /// the interval ago.
    func play(_ events: [DialEvent]) {
        let detent = events.contains { if case .detent = $0 { true } else { false } }
        let kilohertz = events.contains { if case .wholeKilohertz = $0 { true } else { false } }
        let now = clock()
        if detent, ticksOnDetents, lastLight.map({ now - $0 >= interval }) ?? true {
            lastLight = now
            generator.impact(.light)
        }
        if kilohertz, bumpsOnKilohertz, lastRigid.map({ now - $0 >= interval }) ?? true {
            lastRigid = now
            generator.impact(.rigid)
        }
    }

    /// A flag or passband drag moved its slice one or more steps this
    /// frame: one soft tick, only if the last is at least the interval ago,
    /// so a fast drag stays a train of ticks rather than a buzz.
    func dragStep() {
        let now = clock()
        guard lastSoft.map({ now - $0 >= interval }) ?? true else {
            return
        }
        lastSoft = now
        generator.impact(.soft)
    }
}

/// The phone's Taptic Engine: UIKit's light and rigid impacts, and its
/// soft one at half intensity for a drag's steps.
@MainActor
private final class SystemDialImpactGenerator: DialImpactGenerator {
    private let light = UIImpactFeedbackGenerator(style: .light)
    private let rigid = UIImpactFeedbackGenerator(style: .rigid)
    private let soft = UIImpactFeedbackGenerator(style: .soft)

    func prepare() {
        light.prepare()
        rigid.prepare()
        soft.prepare()
    }

    func impact(_ impact: DialImpact) {
        switch impact {
        case .light:
            light.impactOccurred()
        case .rigid:
            rigid.impactOccurred()
        case .soft:
            soft.impactOccurred(intensity: DialHaptics.dragStepIntensity)
        }
    }
}
