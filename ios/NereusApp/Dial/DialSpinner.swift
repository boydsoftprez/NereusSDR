// NereusSDR for iOS: a dial's motion on screen: following the finger every frame, then coasting to a stop
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import QuartzCore

/// One dial's motion (R-IOS-12, Task 61 fix round 1): the knob or wheel
/// turns with the finger on every touch the screen reports (120 a second
/// on ProMotion), never snapping to detents; when the finger lifts while
/// moving, a display link keeps it turning at the finger's speed, slowing
/// under ``DialFeel``'s friction to a stop. Every turn, the finger's or the
/// coast's, goes to ``BandTuningModel/turn(byRadians:)`` at once, so the
/// detents (their writes and ticks) fire as the turn passes them and the
/// drawing never waits for them or for the Core. A touch during a coast
/// stops it where it is. The thumbwheel's roll is carried in the same dial
/// radians, one detent every 12 points.
@MainActor
final class DialSpinner: ObservableObject {
    /// The dial's turn on screen, in radians, clockwise positive.
    @Published private(set) var angle = 0.0
    /// The index is lit: a detent passed in the last 90 ms.
    @Published private(set) var flash = false

    private(set) var momentum: DialMomentum
    private let clock: () -> TimeInterval
    private let drivesItself: Bool
    private weak var tuning: BandTuningModel?
    private var link: CADisplayLink?
    private var flashUntil: TimeInterval = 0
    private var flashTask: Task<Void, Never>?

    /// `clock` is the display link's clock; tests give their own and call
    /// ``frame(at:)`` themselves (`drivesItself` false).
    init(feel: DialFeel = .standard, clock: @escaping () -> TimeInterval = { CACurrentMediaTime() },
         drivesItself: Bool = true) {
        momentum = DialMomentum(feel: feel)
        self.clock = clock
        self.drivesItself = drivesItself
    }

    /// The dial is coasting on its own.
    var isCoasting: Bool { momentum.isCoasting }

    /// A finger comes down on the dial: a coast stops where it is, and a
    /// turn begins on the active slice.
    func touchDown(tuning: BandTuningModel, reversed: Bool) {
        stopLink()
        momentum.touch(at: clock())
        self.tuning = tuning
        tuning.beginTurn(reversed: reversed)
    }

    /// The finger turned the dial by `radians`.
    func follow(byRadians radians: Double) {
        guard radians.isFinite, radians != 0 else {
            return
        }
        angle += radians
        momentum.follow(byRadians: radians, at: clock())
        turn(radians)
    }

    /// The finger lifts: the dial coasts on if it was moving, else the
    /// turn ends and its last frequency goes.
    func release() {
        let tunes = tuning?.dialTunes ?? false
        if tunes, momentum.release(at: clock()) {
            if drivesItself {
                startLink()
            }
        } else {
            momentum.stop()
            tuning?.endTurn()
        }
    }

    /// A frame drawn at `time` during a coast: the dial turns on, and the
    /// turn ends when the coast does.
    func frame(at time: TimeInterval) {
        guard momentum.isCoasting else {
            return
        }
        let radians = momentum.advance(to: time)
        if radians != 0 {
            angle += radians
            turn(radians)
        }
        if !momentum.isCoasting {
            stopLink()
            tuning?.endTurn()
        }
    }

    /// Stops the dial where it is and ends any turn: the dial left the screen.
    func halt() {
        momentum.stop()
        stopLink()
        tuning?.endTurn()
    }

    // MARK: Inside

    private func turn(_ radians: Double) {
        guard let tuning else {
            return
        }
        let events = tuning.turn(byRadians: radians)
        if events.contains(where: { if case .detent = $0 { true } else { false } }) {
            lightIndex()
        }
    }

    /// The index lights for 90 ms after the last detent, where the phone ticks.
    private func lightIndex() {
        flashUntil = clock() + 0.09
        flash = true
        guard flashTask == nil else {
            return
        }
        flashTask = Task { @MainActor [weak self] in
            while let self {
                let left = self.flashUntil - self.clock()
                if left <= 0 || Task.isCancelled {
                    break
                }
                try? await Task.sleep(for: .seconds(left))
            }
            self?.flash = false
            self?.flashTask = nil
        }
    }

    private func startLink() {
        guard link == nil else {
            return
        }
        let link = CADisplayLink(target: LinkTarget(self), selector: #selector(LinkTarget.step(_:)))
        link.preferredFrameRateRange = CAFrameRateRange(minimum: 60, maximum: 120, preferred: 120)
        link.add(to: .main, forMode: .common)
        self.link = link
    }

    private func stopLink() {
        link?.invalidate()
        link = nil
    }

    /// The display link holds its target strongly; this holds the dial weakly.
    private final class LinkTarget: NSObject {
        weak var spinner: DialSpinner?

        init(_ spinner: DialSpinner) {
            self.spinner = spinner
        }

        @objc func step(_ link: CADisplayLink) {
            guard let spinner else {
                // The dial is gone: the link stops too.
                link.invalidate()
                return
            }
            let time = link.targetTimestamp
            MainActor.assumeIsolated {
                spinner.frame(at: time)
            }
        }
    }
}
