// NereusSDR for iOS: Setup's Display page, the transmit display: this phone's grid, levels, palette and DUP, and the Core's own
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// The transmit display on Setup's Display page (Task 54f, desktop PR
/// #317): first what this phone keeps for the pan (the transmit grid, the
/// transmit waterfall's levels and palette, and display duplex), then the
/// Core's own analyzer settings, which every device watching it shares.
/// Ranges are the desktop's. What the Core cannot take from this app stays
/// in place, greyed, with its reason under it.
extension DisplayOnThisPhonePage {
    static let transmitFooter = "While the Core's radio is keyed on this band it shows the Core's transmit display "
        + "with this scale and these levels. The scale's arrows and a drag on it move this scale while keyed."
    static let coreTransmitFooter = "The Core's own settings for its transmit display: every device watching it "
        + "sees the same."

    var transmitDisplay: some View {
        Section {
            SetupNumberRow(title: "Top", value: settings.txGridTopDbm, range: BandDisplaySettings.txGridTopRange,
                           step: 1, unit: "dBm", id: "txGridTop") { value in change { $0.txGridTopDbm = value.rounded() } }
            SetupNumberRow(title: "Range", value: settings.txGridRangeDb,
                           range: BandDisplaySettings.txGridRangeRange, step: 1, unit: "dB",
                           id: "txGridRange") { value in change { $0.txGridRangeDb = value.rounded() } }
            SetupNumberRow(title: "Waterfall high level", value: settings.txWaterfallHighDbm,
                           range: BandDisplaySettings.txWaterfallLevelRange, step: 5, unit: "dBm",
                           id: "txWaterfallHigh") { setTxHighLevel($0) }
            SetupNumberRow(title: "Waterfall low level", value: settings.txWaterfallLowDbm,
                           range: BandDisplaySettings.txWaterfallLevelRange, step: 5, unit: "dBm",
                           id: "txWaterfallLow") { setTxLowLevel($0) }
            Picker("Waterfall palette", selection: Binding(get: { settings.txWaterfallPaletteId },
                                                           set: { id in change { $0.txWaterfallPaletteId = id } })) {
                if display.palettes.isEmpty && settings.txWaterfallPaletteId != BandPalette.customPaletteId {
                    Text(CatalogFeed.needsNewerCoreText).tag(settings.txWaterfallPaletteId)
                }
                ForEach(display.palettes, id: \.id) { palette in
                    Text(palette.name).tag(palette.id)
                }
                Text(BandPalette.customPaletteName).tag(BandPalette.customPaletteId)
            }
            .accessibilityIdentifier("txWaterfallPalette")
            SetupSwitchRow(title: "DUP", detail: display.duplexNote,
                           isOn: Binding(get: { display.duplex && display.duplexAvailable },
                                         set: { display.setDuplex($0) }),
                           enabled: display.duplexAvailable, id: "setupDuplex")
        } header: {
            HStack(spacing: 6) {
                Text("Transmit display")
                SetupTagBadge(tag: .thisPhone)
            }
        } footer: {
            Text(Self.transmitFooter)
        }
        .accessibilityIdentifier("transmitDisplayGroup")
    }

    var coreTransmitDisplay: some View {
        Section {
            Picker("FFT size", selection: Binding(get: { coreTx.shownFftSize },
                                                  set: { coreTx.write(CoreTxDisplaySettings.fftSizeKey, String($0)) })) {
                ForEach(CoreTxDisplaySettings.fftSizes, id: \.self) { size in
                    Text("\(size)").tag(size)
                }
            }
            .accessibilityIdentifier("coreTxFftSize")
            HStack {
                Text("Bin width")
                Spacer(minLength: 8)
                Text(CoreTxDisplaySettings.binWidthText(coreTx.shownFftSize))
                    .monospacedDigit()
                    .foregroundStyle(.secondary)
            }
            .accessibilityElement(children: .combine)
            .accessibilityIdentifier("coreTxBinWidth")
            Picker("Window", selection: Binding(get: { coreTx.window },
                                                set: { coreTx.write(CoreTxDisplaySettings.windowKey, String($0)) })) {
                ForEach(CoreTxDisplaySettings.windows, id: \.id) { window in
                    Text(window.title).tag(window.id)
                }
            }
            .accessibilityIdentifier("coreTxWindow")
            Picker("Spectrum detector", selection: Binding(get: { coreTx.traceDetector },
                                                           set: { coreTx.write(CoreTxDisplaySettings.traceDetectorKey,
                                                                               String($0)) })) {
                ForEach(SpectrumDetector.allCases, id: \.self) { Text($0.title).tag($0.rawValue) }
            }
            .accessibilityIdentifier("coreTxTraceDetector")
            Picker("Spectrum averaging", selection: Binding(get: { coreTx.traceAveraging },
                                                            set: { coreTx.write(CoreTxDisplaySettings.traceAveragingKey,
                                                                                String($0)) })) {
                ForEach(SpectrumAveraging.allCases, id: \.self) { Text($0.title).tag($0.rawValue) }
            }
            .accessibilityIdentifier("coreTxTraceAveraging")
            SetupNumberRow(title: "Spectrum averaging time", value: Double(coreTx.traceAverageTimeMs),
                           range: CoreTxDisplaySettings.averageTimeRange, step: 10, unit: "ms",
                           enabled: coreTx.available, id: "coreTxTraceTime") { value in
                coreTx.write(CoreTxDisplaySettings.traceAverageTimeKey, String(Int(value)))
            }
            Toggle("Normalize", isOn: Binding(get: { coreTx.normalize },
                                              set: { coreTx.write(CoreTxDisplaySettings.normalizeKey,
                                                                  $0 ? "True" : "False") }))
                .disabled(!coreTx.available || !coreTx.normalizeAvailable)
                .accessibilityHint(coreTx.normalizeAvailable ? "" : CoreTxDisplaySettings.normalizeReason)
                .accessibilityIdentifier("coreTxNormalize")
            Picker("Waterfall detector", selection: Binding(get: { coreTx.waterfallDetector },
                                                            set: { coreTx.write(CoreTxDisplaySettings.waterfallDetectorKey,
                                                                                String($0)) })) {
                ForEach(SpectrumDetector.waterfallCases, id: \.self) { Text($0.title).tag($0.rawValue) }
            }
            .accessibilityIdentifier("coreTxWaterfallDetector")
            Picker("Waterfall averaging", selection: Binding(get: { coreTx.waterfallAveraging },
                                                             set: { coreTx.write(
                                                                 CoreTxDisplaySettings.waterfallAveragingKey,
                                                                 String($0)) })) {
                ForEach(SpectrumAveraging.allCases, id: \.self) { Text($0.title).tag($0.rawValue) }
            }
            .accessibilityIdentifier("coreTxWaterfallAveraging")
            SetupNumberRow(title: "Waterfall averaging time", value: Double(coreTx.waterfallAverageTimeMs),
                           range: CoreTxDisplaySettings.averageTimeRange, step: 10, unit: "ms",
                           enabled: coreTx.available, id: "coreTxWaterfallTime") { value in
                coreTx.write(CoreTxDisplaySettings.waterfallAverageTimeKey, String(Int(value)))
            }
        } header: {
            HStack(spacing: 6) {
                Text("The Core's transmit display")
                SetupTagBadge(tag: .core)
            }
        } footer: {
            VStack(alignment: .leading, spacing: 4) {
                if !coreTx.available {
                    Text(CoreTxDisplaySettings.notAppliedText)
                        .accessibilityIdentifier("coreTxNotApplied")
                } else if !coreTx.normalizeAvailable {
                    Text(CoreTxDisplaySettings.normalizeReason)
                }
                if let note = coreTx.note {
                    Text(note)
                        .accessibilityIdentifier("coreTxNote")
                }
                Text(Self.coreTransmitFooter)
            }
        }
        .disabled(!coreTx.available)
        .accessibilityIdentifier("coreTransmitDisplayGroup")
    }

    /// The transmit waterfall's high level, kept above the low one.
    func setTxHighLevel(_ value: Double) {
        let low = settings.txWaterfallLowDbm
        change {
            $0.txWaterfallHighDbm = min(max(value.rounded(), low + 1), BandDisplaySettings.txWaterfallLevelRange.upperBound)
        }
    }

    /// The transmit waterfall's low level, kept below the high one.
    func setTxLowLevel(_ value: Double) {
        let high = settings.txWaterfallHighDbm
        change {
            $0.txWaterfallLowDbm = max(min(value.rounded(), high - 1), BandDisplaySettings.txWaterfallLevelRange.lowerBound)
        }
    }
}
