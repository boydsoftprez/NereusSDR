// NereusSDR for iOS: the mic level meter live from this phone's microphone before transmitting, so the operator can set Mic Gain
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import UIKit

/// The mic level meter's own level while this phone is not keyed (JJ,
/// 2026-09-26: set Mic Gain for a good SSB drive before transmitting).
///
/// While a mic level meter is on screen (the TX panel's, or the Modes
/// tab's Transmit section's) and this phone may transmit, it listens to
/// the phone's microphone (``MicrophoneSource/startLevel(_:lost:)``) and shows
/// each piece of sound's peak in dB with the Core's Mic Gain added, so a
/// change of Mic Gain moves it by as many dB. The level is worked out here:
/// nothing is sent to the Core and nothing keys. The microphone closes
/// when the last meter leaves the screen, the app goes to the background,
/// the phone locks, or a call or Siri interrupts, so iOS's microphone
/// indicator shows only while a meter is up. Where the phone may not use
/// the microphone, ``reason`` says why in ``MicCapture``'s words.
///
/// The desktop's mic level gauge shows its own microphone's peak with the
/// Mic Gain applied too; this is the phone's own version of that idea, not
/// a copy of its code. While keyed the meter shows the Core's reading
/// instead (``MicLevelGauge``).
@MainActor
final class LiveMicLevel: ObservableObject {
    /// Under the meter while it is live, in the TX panel, where Mic Gain
    /// sits on the row right below it (as on the desktop's Phone/CW
    /// applet): the desktop's words for the zone to aim for are "yellow zone".
    static let liveInTxPanelText =
        "Live from this phone's microphone. Speak normally and set Mic Gain below so the peaks reach the yellow."
    /// Under the meter while it is live, beside Mic Gain.
    static let liveText =
        "Live from this phone's microphone. Speak normally and set Mic Gain so the peaks reach the yellow."

    /// The level to show, in dB on the mic level scale, while listening;
    /// the scale's floor until the first sound.
    @Published private(set) var levelDb: Double = LinearGauge.Scale.micLevel.min
    /// The microphone is open for the meter.
    @Published private(set) var listening = false
    /// Why the microphone could not open, in the operator's words.
    @Published private(set) var reason: String?

    private let microphone: (any MicrophoneSource)?
    private var watches: Set<AnyCancellable> = []
    /// The meters on screen now.
    private var viewers: Set<UUID> = []
    private var allowed = false
    private var foreground = true
    private var locked = false
    private var interruptedNow = false
    private var gainDb = 0.0
    /// The newest peak heard, in dB full scale, while listening.
    private var peakDb: Double?
    /// The microphone is wanted for the meter.
    private var wanted = false
    /// Moves with each change of ``wanted``, so a late answer or peak from
    /// an earlier start is dropped.
    private var generation = 0
    /// The last start or stop, so each goes in order.
    private var request: Task<Void, Never>?

    /// The quietest peak worked out; silence reads as this.
    static let silenceDb = -120.0

    /// `gain` is the Core's Mic Gain in dB (nil while it has not sent one,
    /// read as 0); `allowed` is whether this phone may transmit.
    init(microphone: (any MicrophoneSource)?, gain: AnyPublisher<Double?, Never>,
         allowed: AnyPublisher<Bool, Never>, notificationCenter: NotificationCenter = .default) {
        self.microphone = microphone
        gain.sink { [weak self] value in
            self?.gainChanged(value ?? 0)
        }.store(in: &watches)
        allowed.removeDuplicates().sink { [weak self] value in
            self?.allowed = value
            self?.update()
        }.store(in: &watches)
        observe(UIApplication.didEnterBackgroundNotification, on: notificationCenter) { $0.foreground = false }
        observe(UIApplication.willEnterForegroundNotification, on: notificationCenter) { level in
            level.foreground = true
            level.interruptedNow = false
        }
        // Locking the phone closes the microphone at once (D24), before the
        // app reaches the background.
        observe(UIApplication.protectedDataWillBecomeUnavailableNotification, on: notificationCenter) {
            $0.locked = true
        }
        observe(UIApplication.protectedDataDidBecomeAvailableNotification, on: notificationCenter) {
            $0.locked = false
        }
    }

    private func observe(_ name: Notification.Name, on center: NotificationCenter,
                         _ change: @escaping @MainActor (LiveMicLevel) -> Void) {
        center.publisher(for: name)
            .sink { [weak self] _ in
                MainActor.assumeIsolated {
                    guard let self else {
                        return
                    }
                    change(self)
                    self.update()
                }
            }
            .store(in: &watches)
    }

    /// A mic level meter came on screen.
    func show(_ viewer: UUID) {
        if viewers.isEmpty {
            // Opening a meter again tries again after an interruption.
            interruptedNow = false
        }
        viewers.insert(viewer)
        update()
    }

    /// A mic level meter left the screen.
    func hide(_ viewer: UUID) {
        viewers.remove(viewer)
        update()
    }

    /// A call or Siri began: the microphone closes until the app comes
    /// back to the foreground or a meter opens again.
    func interrupted() {
        interruptedNow = true
        update()
    }

    /// Returns once every start and stop asked for so far has run.
    func settle() async {
        await request?.value
    }

    /// A peak (0 to 1 of full scale) in dB full scale; silence, and
    /// anything that is not a number, reads as ``silenceDb``.
    static func decibels(peak: Float) -> Double {
        let value = Double(peak)
        guard value.isFinite, value > 0 else {
            return silenceDb
        }
        return max(20 * log10(value), silenceDb)
    }

    /// The level to show for a peak in dB full scale with the Mic Gain
    /// added, held to the mic level scale.
    static func shown(peakDb: Double, gainDb: Double) -> Double {
        let scale = LinearGauge.Scale.micLevel
        return min(max(peakDb + gainDb, scale.min), scale.max)
    }

    // MARK: Inside

    private func gainChanged(_ value: Double) {
        gainDb = value.isFinite ? value : 0
        refreshLevel()
    }

    private func refreshLevel() {
        let next = peakDb.map { Self.shown(peakDb: $0, gainDb: gainDb) } ?? LinearGauge.Scale.micLevel.min
        if levelDb != next {
            levelDb = next
        }
    }

    private func update() {
        let want = microphone != nil && !viewers.isEmpty && allowed && foreground && !locked && !interruptedNow
        guard want != wanted, let microphone else {
            return
        }
        wanted = want
        generation += 1
        let current = generation
        let previous = request
        if want {
            let heard: @Sendable (Float) -> Void = { [weak self] peak in
                DispatchQueue.main.async {
                    MainActor.assumeIsolated {
                        self?.heard(peak, generation: current)
                    }
                }
            }
            let lost: @Sendable () -> Void = { [weak self] in
                DispatchQueue.main.async {
                    MainActor.assumeIsolated {
                        self?.levelLost(generation: current)
                    }
                }
            }
            request = Task { [weak self] in
                await previous?.value
                let result = await microphone.startLevel(heard, lost: lost)
                self?.started(result, generation: current)
            }
        } else {
            peakDb = nil
            refreshLevel()
            if listening {
                listening = false
            }
            if reason != nil {
                reason = nil
            }
            request = Task {
                await previous?.value
                microphone.stopLevel()
            }
        }
    }

    private func started(_ result: MicrophoneStart, generation started: Int) {
        guard started == generation else {
            return
        }
        switch result {
        case .started:
            listening = true
            if reason != nil {
                reason = nil
            }
        case .failed(let why):
            listening = false
            reason = why
        }
    }

    /// The level stopped by itself (the input could not be built again):
    /// the meter shows no level, not the last one, and is no longer
    /// listening. Opening a meter again, or any other change that wants
    /// the microphone, starts it again.
    private func levelLost(generation lostIn: Int) {
        guard lostIn == generation, wanted else {
            return
        }
        wanted = false
        generation += 1
        peakDb = nil
        refreshLevel()
        if listening {
            listening = false
        }
    }

    private func heard(_ peak: Float, generation heardIn: Int) {
        guard heardIn == generation, wanted else {
            return
        }
        peakDb = Self.decibels(peak: peak)
        refreshLevel()
    }
}
