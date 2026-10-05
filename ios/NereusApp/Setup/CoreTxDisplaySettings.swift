// NereusSDR for iOS: the Core's transmit display settings, its nine DisplayTx keys, read and written through the settings proxy
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMedia
import NereusMirror

/// The Core's own transmit display settings (Task 54f; the link document
/// section 8.1, `txDisplayVersion` 2): the analyzer the Core runs while
/// keyed, which every device watching it shares. They are the desktop's
/// Setup > Display > TX Display analyzer controls and its keys; the Core
/// applies a write at once, on or off the air, and writes back what its
/// analyzer takes. Below version 2 the Core does not apply them from this
/// app, so the controls are greyed with that reason under them, as the
/// desktop's remote window greys them.
@MainActor
final class CoreTxDisplaySettings: ObservableObject {
    // The Core's keys.
    static let fftSizeKey = "DisplayTxFftSize"
    static let windowKey = "DisplayTxWindowType"
    static let traceDetectorKey = "DisplayTxPanDetector"
    static let traceAveragingKey = "DisplayTxPanAveraging"
    static let traceAverageTimeKey = "DisplayTxPanAvTimeMs"
    static let normalizeKey = "DisplayTxPanNormalize"
    static let waterfallDetectorKey = "DisplayTxWfDetector"
    static let waterfallAveragingKey = "DisplayTxWfAveraging"
    static let waterfallAverageTimeKey = "DisplayTxWfAvTimeMs"

    /// The Core's defaults when a key is absent (the link document: FFT
    /// size 32768, window 4, detectors and averaging 0, 30 ms and 120 ms,
    /// normalize off).
    static let defaultFftSize = 32_768
    static let defaultWindow = 4
    static let defaultTraceAverageTimeMs = 30
    static let defaultWaterfallAverageTimeMs = 120

    /// The FFT sizes the Core's analyzer takes: 4096 doubled up to six times.
    static let fftSizes: [Int] = (0...6).map { 4096 << $0 }
    /// The transmit display's rate, for the bin width: 96 kHz.
    static let sampleRateHz = 96_000.0
    /// The averaging times' range, in milliseconds.
    static let averageTimeRange: ClosedRange<Double> = 1...9999
    /// The analyzer's windows, by the Core's number.
    static let windows: [(id: Int, title: String)] = [
        (0, "Rectangular"), (1, "Blackman-Harris 4-term"), (2, "Hann"), (3, "Flat top"), (4, "Hamming"),
        (5, "Kaiser"), (6, "Blackman-Harris 7-term"),
    ]

    /// Why the controls are greyed on an older Core: the desktop remote window's words.
    static let notAppliedText = "This Core does not apply transmit display settings from this app. "
        + "Updating the Core may help."
    /// Why Normalize is greyed: it works only with the Average, Sample and RMS detectors.
    static let normalizeReason = "Normalize works with the Average, Sample and RMS detectors."

    /// The Core's words for the last change it refused.
    @Published private(set) var note: String?
    /// Set whenever the Core's values or its offer change.
    @Published private(set) var revision = 0

    private let settings: SettingsProxyClient
    private let mirror: MirrorStore
    private var watches: Set<AnyCancellable> = []

    init(settings: SettingsProxyClient, mirror: MirrorStore) {
        self.settings = settings
        self.mirror = mirror
        settings.$values.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.changed() }.store(in: &watches)
    }

    private func changed() {
        Task { @MainActor [weak self] in
            self?.revision &+= 1
        }
    }

    /// The Core applies this app's writes: `txDisplayVersion` 2 or more.
    var available: Bool {
        MediaFeatureGates(agreedMinor: mirror.agreedMinor ?? 0) { [mirror] in mirror.capabilityVersion($0) }
            .txDisplaySettings
    }

    /// The Core's value of `key` as a whole number, `fallback` when absent or not a number.
    func whole(_ key: String, _ fallback: Int) -> Int {
        settings.value(key).flatMap { Int($0.trimmingCharacters(in: .whitespaces)) } ?? fallback
    }

    var fftSize: Int { whole(Self.fftSizeKey, Self.defaultFftSize) }
    var window: Int { min(max(whole(Self.windowKey, Self.defaultWindow), 0), 6) }
    var traceDetector: Int { min(max(whole(Self.traceDetectorKey, 0), 0), 4) }
    var traceAveraging: Int { min(max(whole(Self.traceAveragingKey, 0), 0), 3) }
    var traceAverageTimeMs: Int { min(max(whole(Self.traceAverageTimeKey, Self.defaultTraceAverageTimeMs), 1), 9999) }
    var normalize: Bool { settings.value(Self.normalizeKey)?.caseInsensitiveCompare("True") == .orderedSame }
    /// Normalize can be set: the trace detector is Average, Sample or RMS.
    var normalizeAvailable: Bool { traceDetector >= 2 }
    var waterfallDetector: Int { min(max(whole(Self.waterfallDetectorKey, 0), 0), 3) }
    var waterfallAveraging: Int { min(max(whole(Self.waterfallAveragingKey, 0), 0), 3) }
    var waterfallAverageTimeMs: Int {
        min(max(whole(Self.waterfallAverageTimeKey, Self.defaultWaterfallAverageTimeMs), 1), 9999)
    }

    /// The FFT size shown: the Core's, or the nearest size it takes above it.
    var shownFftSize: Int { Self.fftSizes.first { $0 >= fftSize } ?? Self.fftSizes[Self.fftSizes.count - 1] }

    /// The bin width at `size`, in hertz, to three places.
    static func binWidthText(_ size: Int) -> String {
        String(format: "%.3f Hz", sampleRateHz / Double(max(size, 1)))
    }

    /// Writes one of the Core's keys; the Core's refusal is shown as it said it.
    func write(_ key: String, _ value: String) {
        guard available else {
            return
        }
        Task { @MainActor in
            switch await settings.write(key, value) {
            case .rejected(let reason):
                note = reason.isEmpty ? "The Core did not change this setting." : reason
            case .accepted:
                note = nil
            default:
                break
            }
        }
    }
}
