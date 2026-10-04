// NereusSDR for iOS: Setup's Display page, the rest of the desktop's display: overlays, detectors, rewind, colours and reset
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI
import UIKit

/// The rest of the desktop's Display pages on this phone (Task 54e part 2,
/// the parity audit's rows 13, 14, 16, 19, 21, 22, 24 to 28, 30 to 34),
/// grouped as the desktop groups them. Ranges are the desktop's where it
/// has one. Colours and sizes are the desktop's (D83).
extension DisplayOnThisPhonePage {
    // The words.
    static let resetTitle = "Reset to defaults"
    static let resetQuestion = "Reset the spectrum and waterfall to their defaults?"
    static let resetMessage = "Each band's scale, the band plan's size and the extended view are kept."
    static let decimationNote = "Decimation: \(CatalogFeed.needsNewerCoreText)"
    static let fastAttackReason = "The Core does not say when the noise floor is catching up, so the line keeps its own color."
    static let customPaletteFooter = "Your own gradient, kept on this phone. Choose it as the palette to use it."

    // MARK: Waterfall

    var waterfallMore: some View {
        Section {
            if !core.available {
                // With the Core's description these are its WF Detector and
                // WF Averaging, in the Core's own groups above.
                Picker("Detector", selection: binding(\.waterfallDetector)) {
                    ForEach(SpectrumDetector.waterfallCases, id: \.self) { Text($0.title).tag($0) }
                }
                .accessibilityIdentifier("waterfallDetector")
                Picker("Averaging", selection: binding(\.waterfallAveraging)) {
                    ForEach(SpectrumAveraging.allCases, id: \.self) { Text($0.title).tag($0) }
                }
                .disabled(!extras)
                .accessibilityIdentifier("waterfallAveraging")
            }
            SetupNumberRow(title: "Update period", value: Double(settings.waterfallPeriodMs),
                           range: Self.range(BandDisplaySettings.waterfallPeriodRange), step: 10, unit: "ms",
                           id: "waterfallPeriod") { value in change { $0.waterfallPeriodMs = Int(value) } }
            SetupNumberRow(title: "Opacity", value: Double(settings.waterfallOpacityPercent),
                           range: Self.range(BandDisplaySettings.waterfallOpacityRange), step: 5, unit: "%",
                           id: "waterfallOpacity") { value in change { $0.waterfallOpacityPercent = Int(value) } }
            Toggle("Stop while transmitting", isOn: binding(\.waterfallStopOnTx))
                .accessibilityIdentifier("waterfallStopOnTx")
            Toggle("Use the scale's top and bottom", isOn: binding(\.useSpectrumMinMax))
                .accessibilityIdentifier("useSpectrumMinMax")
            Button("Copy the scale to High and Low") { copyScaleToLevels() }
                .accessibilityIdentifier("copyScaleToLevels")
            Button("Copy High and Low to the scale") { copyLevelsToScale() }
                .accessibilityIdentifier("copyLevelsToScale")
            Picker("Look back up to", selection: binding(\.rewindSeconds)) {
                ForEach(BandDisplaySettings.rewindChoices, id: \.self) { Text(Self.minutesText($0)).tag($0) }
            }
            .accessibilityIdentifier("rewindDepth")
            Picker("Time", selection: binding(\.timestampPosition)) {
                ForEach(TimestampPosition.allCases, id: \.self) { Text($0.title).tag($0) }
            }
            .accessibilityIdentifier("timestampPosition")
            Picker("Time zone", selection: binding(\.timestampUtc)) {
                Text("UTC").tag(true)
                Text("This phone's").tag(false)
            }
            .disabled(settings.timestampPosition == .none)
            .accessibilityIdentifier("timestampZone")
            Toggle("Receive filter", isOn: binding(\.showRxFilterOnWaterfall))
                .accessibilityIdentifier("showRxFilterOnWaterfall")
            Toggle("Transmit filter while transmitting", isOn: binding(\.showTxFilterOnWaterfall))
                .accessibilityIdentifier("showTxFilterOnWaterfall")
            Toggle("Receive zero line", isOn: binding(\.showRxZeroLineOnWaterfall))
                .accessibilityIdentifier("showRxZeroLineOnWaterfall")
            Toggle("Transmit zero line while transmitting", isOn: binding(\.showTxZeroLineOnWaterfall))
                .accessibilityIdentifier("showTxZeroLineOnWaterfall")
            customPalette
        } header: {
            Text("Waterfall: more")
        } footer: {
            Text("Looking back keeps up to \(Self.minutesText(Int(Double(settings.rewindLines) * Double(settings.waterfallPeriodMs) / 1000))) at this update period. "
                 + Self.customPaletteFooter)
        }
    }

    /// This phone's own gradient: each stop's place and colour, a stop added
    /// or the last middle one removed.
    @ViewBuilder
    private var customPalette: some View {
        ForEach(settings.customPalette.indices, id: \.self) { index in
            let stop = settings.customPalette[index]
            HStack {
                ColorPicker("Stop \(index + 1)", selection: Binding(get: { BandColours.slice(stop.colour) },
                                                                  set: { colour in
                    change { $0.customPalette[index].colour = Self.hex(colour) }
                }), supportsOpacity: false)
                Stepper("\(Int((stop.at * 100).rounded()))%", value: Binding(get: { stop.at * 100 }, set: { value in
                    change { $0.customPalette[index].at = min(max(value, 0), 100) / 100 }
                }), in: 0...100, step: 5)
                .fixedSize()
                .disabled(index == 0 || index == settings.customPalette.count - 1)
            }
            .accessibilityIdentifier("customStop\(index)")
        }
        HStack {
            Button("Add a stop") { addCustomStop() }
                .disabled(settings.customPalette.count >= 8)
            Spacer()
            Button("Remove a stop") { change { $0.customPalette.remove(at: $0.customPalette.count - 2) } }
                .disabled(settings.customPalette.count <= 2)
        }
        .buttonStyle(.borderless)
        .accessibilityIdentifier("customStops")
    }

    func addCustomStop() {
        change { settings in
            let stops = settings.customPalette
            guard stops.count >= 2, stops.count < 8 else {
                return
            }
            // Halfway between the last two stops, in the colour of the lower.
            let low = stops[stops.count - 2]
            let high = stops[stops.count - 1]
            settings.customPalette.insert(PaletteStop(at: (low.at + high.at) / 2, colour: low.colour),
                                          at: stops.count - 1)
        }
    }

    func copyScaleToLevels() {
        let top = settings.scaleTopDbm
        let bottom = settings.scaleBottomDbm
        change {
            $0.waterfallHighDbm = top
            $0.waterfallLowDbm = bottom
        }
    }

    func copyLevelsToScale() {
        let high = settings.waterfallHighDbm
        let low = min(settings.waterfallLowDbm, high - 1)
        change {
            $0.scaleTopDbm = high
            $0.scaleBottomDbm = low
        }
    }

    // MARK: Spectrum

    var spectrumMore: some View {
        Section {
            if !core.available {
                // With the Core's description these are its Spectrum
                // Detector, Spectrum Averaging and Decimation, above.
                Picker("Detector", selection: binding(\.spectrumDetector)) {
                    ForEach(SpectrumDetector.allCases, id: \.self) { Text($0.title).tag($0) }
                }
                .accessibilityIdentifier("spectrumDetector")
                Picker("Averaging", selection: binding(\.spectrumAveraging)) {
                    ForEach(SpectrumAveraging.allCases, id: \.self) { Text($0.title).tag($0) }
                }
                .disabled(!extras)
                .accessibilityIdentifier("spectrumAveraging")
                SetupNumberRow(title: "Decimation", value: Double(settings.decimation),
                               range: Self.range(BandDisplaySettings.decimationRange), step: 1, unit: "",
                               enabled: display.gates.decimation,
                               id: "decimation") { value in change { $0.decimation = Int(value) } }
            }
            Toggle("Peak hold", isOn: binding(\.peakHold))
                .accessibilityIdentifier("classicPeakHoldSetup")
            SetupNumberRow(title: "Peak hold starts over every", value: Double(settings.peakHoldDelayMs),
                           range: Self.range(BandDisplaySettings.peakHoldDelayRange), step: 100, unit: "ms",
                           enabled: settings.peakHold,
                           id: "peakHoldDelay") { value in change { $0.peakHoldDelayMs = Int(value) } }
            Toggle("Zero line", isOn: binding(\.showZeroLine))
                .accessibilityIdentifier("showZeroLine")
        } header: {
            Text("Spectrum: more")
        } footer: {
            if !display.gates.decimation && !core.available {
                Text(Self.decimationNote)
            }
        }
    }

    // MARK: Grid

    var gridMore: some View {
        Section {
            Toggle("dBm scale", isOn: binding(\.showDbmScale))
                .accessibilityIdentifier("showDbmScale")
            Picker("Frequency labels", selection: binding(\.frequencyLabelAlignment)) {
                ForEach(FrequencyLabelAlignment.allCases, id: \.self) { Text($0.title).tag($0) }
            }
            .accessibilityIdentifier("frequencyLabelAlignment")
            Toggle("Bottom follows the noise floor", isOn: binding(\.gridFollowsNoiseFloor))
                .disabled(!extras)
                .accessibilityIdentifier("gridFollowsNoiseFloor")
            SetupNumberRow(title: "Above the noise floor by", value: Double(settings.gridNoiseFloorOffsetDb),
                           range: Self.range(BandDisplaySettings.gridNoiseFloorOffsetRange), step: 1, unit: "dB",
                           enabled: extras && settings.gridFollowsNoiseFloor,
                           id: "gridNoiseFloorOffset") { value in change { $0.gridNoiseFloorOffsetDb = Int(value) } }
            Toggle("Keep the range, moving the top too", isOn: binding(\.gridKeepsRange))
                .disabled(!(extras && settings.gridFollowsNoiseFloor))
                .accessibilityIdentifier("gridKeepsRange")
        } header: {
            Text("Grid & Scales: more")
        } footer: {
            if !extras {
                Text("Bottom follows the noise floor: \(CatalogFeed.needsNewerCoreText)")
            }
        }
    }

    // MARK: Readouts

    var readouts: some View {
        Section {
            Toggle("Frequency under the finger", isOn: binding(\.showCursorFrequency))
                .accessibilityIdentifier("showCursorFrequency")
            Toggle("Bin width", isOn: binding(\.showBinWidth))
                .accessibilityIdentifier("showBinWidth")
            Toggle("Frames a second", isOn: binding(\.showFps))
                .accessibilityIdentifier("showFps")
            Toggle("Strongest signal", isOn: binding(\.showPeakValue))
                .accessibilityIdentifier("showPeakValue")
            Picker("Corner", selection: binding(\.peakValueCorner)) {
                ForEach(OverlayCorner.allCases, id: \.self) { Text($0.title).tag($0) }
            }
            .disabled(!settings.showPeakValue)
            .accessibilityIdentifier("peakValueCorner")
            SetupNumberRow(title: "Every", value: Double(settings.peakValueDelayMs),
                           range: Self.range(BandDisplaySettings.peakValueDelayRange), step: 100, unit: "ms",
                           enabled: settings.showPeakValue,
                           id: "peakValueDelay") { value in change { $0.peakValueDelayMs = Int(value) } }
        } header: {
            Text("On the band")
        } footer: {
            Text("The frequency under the finger shows while you drag the band.")
        }
    }

    // MARK: Colours

    var colours: some View {
        Section {
            colourRow("Grid", \.gridColour, id: "gridColour")
            colourRow("Fine grid", \.gridFineColour, id: "gridFineColour")
            colourRow("Level grid", \.hGridColour, id: "hGridColour")
            colourRow("Frequency labels", \.gridTextColour, id: "gridTextColour")
            colourRow("Receive zero line", \.rxZeroLineColour, id: "rxZeroLineColour")
            colourRow("Transmit zero line", \.txZeroLineColour, id: "txZeroLineColour")
            colourRow("Transmit passband", \.txPassbandColour, id: "txPassbandColour")
            colourRow("Active peak hold", \.peakHoldColour, id: "peakHoldColour")
            colourRow("Peaks", \.peakBlobColour, id: "peakBlobColour")
            colourRow("Peaks' levels", \.peakBlobTextColour, id: "peakBlobTextColour")
            colourRow("Noise floor", \.noiseFloorColour, id: "noiseFloorColour")
            colourRow("Noise floor's level", \.noiseFloorTextColour, id: "noiseFloorTextColour")
            ColorPicker("Noise floor catching up", selection: .constant(Self.fastAttackColour), supportsOpacity: false)
                .disabled(true)
                .accessibilityHint(Self.fastAttackReason)
                .accessibilityIdentifier("noiseFloorFastColour")
            colourRow("Strongest signal", \.peakValueColour, id: "peakValueColour")
        } header: {
            Text("Colors")
        } footer: {
            Text(Self.fastAttackReason)
        }
    }

    /// The desktop's own grey for a floor catching up, shown greyed.
    static let fastAttackColour = Color(red: 200.0 / 255, green: 200.0 / 255, blue: 200.0 / 255)

    private func colourRow(_ title: String, _ key: WritableKeyPath<BandDisplaySettings, String>,
                           id: String) -> some View {
        ColorPicker(title, selection: Binding(get: { BandColours.withAlpha(settings[keyPath: key]) },
                                              set: { colour in
            change { $0[keyPath: key] = Self.hexWithAlpha(colour) }
        }), supportsOpacity: true)
        .accessibilityIdentifier(id)
    }

    // MARK: Reset

    var reset: some View {
        Section {
            Button(Self.resetTitle, role: .destructive) { confirmingReset = true }
                .accessibilityIdentifier("resetDisplay")
        } footer: {
            Text(Self.resetMessage)
        }
    }

    /// The spectrum and waterfall back at their defaults (after the question).
    func resetDisplay() {
        change { $0 = $0.resetKeepingPlace() }
    }

    // MARK: Inside

    private func binding<Value>(_ key: WritableKeyPath<BandDisplaySettings, Value>) -> Binding<Value> {
        Binding(get: { settings[keyPath: key] }, set: { value in change { $0[keyPath: key] = value } })
    }

    static func range(_ range: ClosedRange<Int>) -> ClosedRange<Double> {
        Double(range.lowerBound)...Double(range.upperBound)
    }

    static func minutesText(_ seconds: Int) -> String {
        let minutes = max(0, seconds) / 60
        if minutes == 0 {
            return "\(max(0, seconds)) seconds"
        }
        return minutes == 1 ? "1 minute" : "\(minutes) minutes"
    }

    /// A colour as `#RRGGBBAA`.
    static func hexWithAlpha(_ colour: Color) -> String {
        var red: CGFloat = 0
        var green: CGFloat = 0
        var blue: CGFloat = 0
        var alpha: CGFloat = 0
        UIColor(colour).getRed(&red, green: &green, blue: &blue, alpha: &alpha)
        return BandPalette.hex(SIMD4(Float(red), Float(green), Float(blue), Float(alpha)))
    }
}
