// NereusSDR for iOS: the PTT button test's microphone meter (debug copies only, sends nothing anywhere)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if PTT_PROBE
import AVFoundation
import Combine
import Foundation

/// Shows that the microphone is capturing while Push to Talk has its
/// audio on (plan Task 65, question 3). It counts buffers and shows their
/// peak; the samples go nowhere, not to a file and not off the phone.
@MainActor
final class PttProbeMicrophone: ObservableObject {
    @Published private(set) var running = false
    /// Buffers since the last start.
    @Published private(set) var buffers = 0
    /// The last buffer's peak, 0 to 1.
    @Published private(set) var peak: Float = 0

    private var engine: AVAudioEngine?

    /// Starts counting. Returns the line for the log.
    func start() -> String {
        stop()
        buffers = 0
        peak = 0
        let engine = AVAudioEngine()
        let input = engine.inputNode
        let format = input.outputFormat(forBus: 0)
        guard format.sampleRate > 0, format.channelCount > 0 else {
            return "microphone gave no input format"
        }
        input.installTap(onBus: 0, bufferSize: AVAudioFrameCount(format.sampleRate / 10), format: format,
                         block: Self.tap { [weak self] peak in
                             self?.received(peak: peak)
                         })
        do {
            try engine.start()
        } catch {
            input.removeTap(onBus: 0)
            return "microphone would not start: \(error.localizedDescription)"
        }
        self.engine = engine
        running = true
        return String(format: "microphone started, %.0f Hz, %u channel(s)", format.sampleRate, format.channelCount)
    }

    func stop() {
        guard let engine else { return }
        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
        self.engine = nil
        running = false
    }

    private func received(peak: Float) {
        guard running else { return }
        buffers += 1
        self.peak = peak
    }

    /// The tap runs on the engine's own thread, so it is built outside the
    /// main actor and hands each peak to it.
    nonisolated private static func tap(_ report: @escaping @MainActor @Sendable (Float) -> Void)
        -> AVAudioNodeTapBlock {
        { buffer, _ in
            let value = peakOf(buffer)
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    report(value)
                }
            }
        }
    }

    nonisolated static func peakOf(_ buffer: AVAudioPCMBuffer) -> Float {
        guard let channels = buffer.floatChannelData, buffer.frameLength > 0 else { return 0 }
        var peak: Float = 0
        for channel in 0..<Int(buffer.format.channelCount) {
            let samples = channels[channel]
            for frame in 0..<Int(buffer.frameLength) {
                peak = max(peak, abs(samples[frame]))
            }
        }
        return min(peak, 1)
    }
}
#endif
