// NereusSDR for iOS: Setup's Display page: the Core's band plan, and every display setting this phone keeps for the pan
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import NereusModels
import SwiftUI
import UIKit

/// Setup, Display, On this phone (R-IOS-11, D73, D79; spec section 5.1
/// items 15 and 19): what the Display sheet's "More display options in
/// Setup" opens. First the Band plan group (D79): the picker changes the
/// Core's plan, so the desktop and every device follow, and its size is
/// kept on this phone. Then the Core's display settings as its catalogue
/// describes them (link 7.4, `display`; ``CoreDisplayModel``). Then every
/// display setting this phone keeps for the
/// pan, grouped as the desktop's Display pages group them (Waterfall
/// Defaults, Spectrum Defaults, Grid & Scales, Spectrum Peaks), with the
/// manual waterfall levels and the noise-floor level mode the sheet leaves
/// out. Ranges are the desktop's. What the Core computes (every level
/// mode but Manual, averaging, normalise, the noise-floor line and the
/// peaks) stays in place, greyed, with "Needs a newer Core" on a Core that
/// does not compute display extras (D23). Calibration offset is greyed on
/// every Core, with the desktop remote window's reason: the Core already
/// calibrates the display for its radio, so it is never applied twice.
struct DisplayOnThisPhonePage: View {
    @ObservedObject var main: MainScreenModel
    @ObservedObject var band: BandModel
    @ObservedObject var display: DisplaySheetModel
    @ObservedObject var pan: PanSheetModel
    /// The Core's transmit display settings (Task 54f).
    @ObservedObject var coreTx: CoreTxDisplaySettings
    @ObservedObject var core: CoreDisplayModel
    @Environment(\.displayScale) private var displayScale
    /// Reset to defaults asks first, as the desktop's does.
    @State var confirmingReset = false

    init(main: MainScreenModel) {
        self.main = main
        band = main.band
        display = main.display
        pan = main.pan
        coreTx = main.coreTxDisplay
        core = main.coreDisplay
    }

    /// The level modes in the order the page lists them, with their names.
    static let levelModes: [(mode: BandDisplaySettings.WaterfallLevelMode, title: String)] = [
        (.clarity, "Clarity"), (.agc, "Auto"), (.noiseFloorAgc, "Noise floor"), (.manual, "Manual"),
    ]

    // The desktop's ranges (src/gui/setup/DisplaySetupPages.cpp and
    // SpectrumPeaksPage.cpp): waterfall levels -200 to 0 dBm, NF-AGC offset
    // -60 to 60 dB, averaging 10 to 9999 ms, calibration -30 to 30 dBm,
    // noise-floor shift -12 to 12 dB, grid step 1 to 40 dB, scale -200 to 0
    // dBm, peak hold 100 to 60000 ms, drop and fall 1 to 60 dB/s, blobs 1 to 20.
    static let levelRange: ClosedRange<Double> = -200...0
    static let offsetRange: ClosedRange<Double> = -60...60
    static let averageRange: ClosedRange<Double> = 10...9999
    static let calibrationRange: ClosedRange<Double> = -30...30
    static let noiseShiftRange: ClosedRange<Double> = -12...12
    static let gridStepRange: ClosedRange<Double> = 1...40
    static let scaleRange: ClosedRange<Double> = -200...0
    static let holdRange: ClosedRange<Double> = 100...60000
    static let fallRange: ClosedRange<Double> = 1...60
    static let blobCountRange: ClosedRange<Double> = 1...20
    static let percentRange: ClosedRange<Double> = 0...100
    static let colorGainRange: ClosedRange<Double> = 0...100
    static let blackLevelRange: ClosedRange<Double> = 0...125
    static let spectrumHeightRange: ClosedRange<Double> = BandDisplaySettings.spectrumShareRange

    static let planNote = "The plan is the Core's: the desktop and every device follow it. Its size is kept on this phone."
    static let extrasNote = "Every choice but Manual, averaging, normalize, the noise-floor line and the peaks: \(CatalogFeed.needsNewerCoreText)"
    /// Why Calibration offset is greyed: the desktop remote window's words.
    static let calibrationNote = "Calibration offset: \(BandDisplaySettings.calibrationReason)"

    var settings: BandDisplaySettings { band.settings }
    var extras: Bool { display.extrasAvailable }

    // MARK: Changes

    /// Changes the pan's settings on this phone: kept, and the band redraws.
    func change(_ edit: (inout BandDisplaySettings) -> Void) {
        main.changeDisplay(edit)
    }

    /// The level mode: any but Manual only while the Core computes the extras.
    func setLevelMode(_ mode: BandDisplaySettings.WaterfallLevelMode) {
        guard mode == .manual || extras else {
            return
        }
        change { $0.waterfallLevelMode = mode }
    }

    /// The manual high level, kept above the low one.
    func setHighLevel(_ value: Double) {
        let low = settings.waterfallLowDbm
        change { $0.waterfallHighDbm = min(max(value.rounded(), low + 1), Self.levelRange.upperBound) }
    }

    /// The manual low level, kept below the high one.
    func setLowLevel(_ value: Double) {
        let high = settings.waterfallHighDbm
        change { $0.waterfallLowDbm = max(min(value.rounded(), high - 1), Self.levelRange.lowerBound) }
    }

    /// The receive scale's top, its range kept as far as -200 dBm allows.
    /// Setup always moves the receive scale; the band's own scale moves the
    /// transmit grid while keyed.
    func setTop(_ value: Double) {
        let top = min(max(value.rounded(), Self.scaleRange.lowerBound), Self.scaleRange.upperBound)
        let depth = max(settings.scaleTopDbm - settings.scaleBottomDbm, 1)
        change {
            $0.scaleTopDbm = top
            $0.scaleBottomDbm = max(top - depth, min(Self.scaleRange.lowerBound, top - 1))
        }
    }

    /// The scale's bottom, kept below its top.
    func setBottom(_ value: Double) {
        let top = settings.scaleTopDbm
        change { $0.scaleBottomDbm = max(min(value.rounded(), top - 1), Self.scaleRange.lowerBound) }
    }

    /// A colour as `#RRGGBB`.
    static func hex(_ colour: Color) -> String {
        var red: CGFloat = 0
        var green: CGFloat = 0
        var blue: CGFloat = 0
        var alpha: CGFloat = 0
        UIColor(colour).getRed(&red, green: &green, blue: &blue, alpha: &alpha)
        func byte(_ part: CGFloat) -> Int { Int((min(max(part, 0), 1) * 255).rounded()) }
        return String(format: "#%02X%02X%02X", byte(red), byte(green), byte(blue))
    }

    // MARK: The page

    var body: some View {
        List {
            bandPlan
            coreDisplay
            waterfall
            waterfallMore
            spectrum
            spectrumMore
            grid
            gridMore
            noiseFloor
            peaks
            readouts
            colours
            view
            transmitDisplay
            coreTransmitDisplay
            reset
        }
        .confirmationDialog(Self.resetQuestion, isPresented: $confirmingReset, titleVisibility: .visible) {
            Button(Self.resetTitle, role: .destructive) { resetDisplay() }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text(Self.resetMessage)
        }
        .navigationTitle("Display")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .both)
            }
        }
    }

    private var bandPlan: some View {
        Section {
            BandPlanPicker(model: display)
            Picker("Size", selection: Binding(get: { settings.bandPlanSize },
                                              set: { display.setBandPlanSize($0) })) {
                ForEach(BandPlanSize.allCases, id: \.self) { size in
                    Text(size.label).tag(size)
                }
            }
            .pickerStyle(.segmented)
            .accessibilityLabel("Band plan size")
            .accessibilityIdentifier("bandPlanSize")
        } header: {
            HStack(spacing: 6) {
                Text("Band plan")
                SetupTagBadge(tag: .both)
            }
        } footer: {
            Text(Self.planNote)
        }
        .accessibilityIdentifier("bandPlanGroup")
    }

    private var waterfall: some View {
        Section {
            Picker("Palette", selection: Binding(get: { settings.waterfallPaletteId },
                                                 set: { display.selectPalette($0) })) {
                if display.palettes.isEmpty && settings.waterfallPaletteId != BandPalette.customPaletteId {
                    Text(CatalogFeed.needsNewerCoreText).tag(settings.waterfallPaletteId)
                }
                ForEach(display.palettes, id: \.id) { palette in
                    Text(palette.name).tag(palette.id)
                }
                Text(BandPalette.customPaletteName).tag(BandPalette.customPaletteId)
            }
            .accessibilityIdentifier("displayPalette")
            Picker("Levels", selection: Binding(get: { extras ? settings.waterfallLevelMode : .manual },
                                                set: { setLevelMode($0) })) {
                ForEach(Self.levelModes, id: \.mode) { entry in
                    Text(entry.title).tag(entry.mode)
                }
            }
            .disabled(!extras)
            .accessibilityIdentifier("waterfallLevels")
            SetupNumberRow(title: "High level", value: settings.waterfallHighDbm, range: Self.levelRange, step: 1,
                           unit: "dBm", id: "waterfallHigh") { setHighLevel($0) }
            SetupNumberRow(title: "Low level", value: settings.waterfallLowDbm, range: Self.levelRange, step: 1,
                           unit: "dBm", id: "waterfallLow") { setLowLevel($0) }
            SetupNumberRow(title: "Noise-floor offset", value: Double(settings.waterfallOffsetDb),
                           range: Self.offsetRange, step: 1, unit: "dB",
                           enabled: extras && settings.waterfallLevelMode == .noiseFloorAgc,
                           id: "waterfallOffset") { value in change { $0.waterfallOffsetDb = Int(value) } }
            if !core.available {
                // With the Core's description this is its WF Avg Time, above.
                SetupNumberRow(title: "Averaging", value: Double(settings.waterfallAverageTimeMs),
                               range: Self.averageRange, step: 10, unit: "ms", enabled: extras,
                               id: "waterfallAverage") { value in change { $0.waterfallAverageTimeMs = Int(value) } }
            }
            SetupNumberRow(title: "Color gain", value: display.colorGain, range: Self.colorGainRange, step: 1,
                           unit: "", id: "setupColorGain") { display.setColorGain($0) }
            SetupNumberRow(title: "Black level", value: display.blackLevel, range: Self.blackLevelRange, step: 1,
                           unit: "", id: "setupBlackLevel") { display.setBlackLevel($0) }
        } header: {
            Text("Waterfall")
        } footer: {
            Text("High and Low are the Manual levels, used too until the Core first sends its own. "
                 + "Color gain and Black level narrow whichever levels color the waterfall.")
        }
    }

    private var spectrum: some View {
        Section {
            Toggle("Fill", isOn: Binding(get: { settings.traceFill }, set: { on in change { $0.traceFill = on } }))
                .accessibilityIdentifier("traceFill")
            SetupNumberRow(title: "Fill strength", value: display.fill, range: Self.percentRange, step: 5,
                           unit: "%", enabled: settings.traceFill, id: "fillStrength") { display.setFill($0) }
            Toggle("Gradient fill", isOn: Binding(get: { settings.traceGradient },
                                                  set: { on in change { $0.traceGradient = on } }))
                .disabled(!settings.traceFill)
                .accessibilityIdentifier("traceGradient")
            let line = DisplaySheetModel.lineRange(scale: displayScale)
            Stepper {
                HStack {
                    Text("Line")
                    Spacer(minLength: 8)
                    Text(DisplaySheetModel.lineText(display.line))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
            } onIncrement: {
                display.setLine(display.line + line.step, scale: displayScale)
            } onDecrement: {
                display.setLine(display.line - line.step, scale: displayScale)
            }
            .accessibilityValue(DisplaySheetModel.lineText(display.line))
            .accessibilityIdentifier("traceLine")
            SetupNumberRow(title: "Spectrum height", value: display.spectrumHeight, range: Self.spectrumHeightRange,
                           step: 5, unit: "%", id: "setupSpectrumHeight") { display.setSpectrumHeight($0) }
            ColorPicker("Trace color", selection: Binding(get: { BandColours.slice(settings.traceColour) },
                                                           set: { colour in
                                                               change { $0.traceColour = Self.hex(colour) }
                                                           }), supportsOpacity: false)
                .accessibilityIdentifier("traceColour")
            ColorPicker("Passband color", selection: Binding(get: { BandColours.slice(settings.passbandColour) },
                                                              set: { colour in
                                                                  change { $0.passbandColour = Self.hex(colour) }
                                                              }), supportsOpacity: false)
                .accessibilityIdentifier("passbandColour")
            SetupNumberRow(title: "Passband strength", value: (settings.passbandOpacity * 100).rounded(),
                           range: Self.percentRange, step: 5, unit: "%",
                           id: "passbandStrength") { value in change { $0.passbandOpacity = value / 100 } }
            if !core.available {
                // With the Core's description this is its Spectrum Avg Time, above.
                SetupNumberRow(title: "Averaging", value: Double(settings.spectrumAverageTimeMs),
                               range: Self.averageRange, step: 10, unit: "ms", enabled: extras,
                               id: "spectrumAverage") { value in change { $0.spectrumAverageTimeMs = Int(value) } }
            }
            // Greyed always, as the desktop's remote window greys Cal Offset:
            // the Core's levels already carry its calibration.
            SetupNumberRow(title: "Calibration offset", value: settings.calibrationOffsetDb,
                           range: Self.calibrationRange, step: 0.5, unit: "dBm", decimals: 1, enabled: false,
                           id: "calibrationOffset") { _ in }
                .accessibilityHint(BandDisplaySettings.calibrationReason)
            Toggle("Normalize", isOn: Binding(get: { settings.normalize },
                                              set: { on in change { $0.normalize = on } }))
                .disabled(!extras)
                .accessibilityIdentifier("normalize")
        } header: {
            Text("Spectrum")
        } footer: {
            VStack(alignment: .leading, spacing: 4) {
                Text(Self.calibrationNote)
                    .accessibilityIdentifier("calibrationNote")
                if !extras {
                    Text(Self.extrasNote)
                        .accessibilityIdentifier("displayExtrasNote")
                }
            }
        }
    }

    private var grid: some View {
        Section {
            Toggle("Grid", isOn: Binding(get: { settings.grid }, set: { on in change { $0.grid = on } }))
                .accessibilityIdentifier("grid")
            SetupNumberRow(title: "Grid step", value: settings.gridStepDb, range: Self.gridStepRange, step: 1,
                           unit: "dB", enabled: settings.grid,
                           id: "gridStep") { value in change { $0.gridStepDb = value } }
            SetupNumberRow(title: "Top", value: settings.scaleTopDbm, range: Self.scaleRange, step: 1, unit: "dBm",
                           id: "scaleTop") { setTop($0) }
            SetupNumberRow(title: "Bottom", value: settings.scaleBottomDbm, range: Self.scaleRange, step: 1,
                           unit: "dBm", id: "scaleBottom") { setBottom($0) }
        } header: {
            Text("Grid & Scales")
        }
    }

    private var noiseFloor: some View {
        Section {
            Toggle("Noise-floor line", isOn: Binding(get: { settings.noiseFloorLine },
                                                     set: { on in change { $0.noiseFloorLine = on } }))
                .disabled(!extras)
                .accessibilityIdentifier("noiseFloorLine")
            SetupNumberRow(title: "Line shift", value: settings.noiseFloorShiftDb, range: Self.noiseShiftRange,
                           step: 0.5, unit: "dB", decimals: 1, enabled: extras && settings.noiseFloorLine,
                           id: "noiseFloorShift") { value in change { $0.noiseFloorShiftDb = value } }
        } header: {
            Text("Noise floor")
        }
    }

    private var peaks: some View {
        Section {
            Toggle("Peak hold", isOn: Binding(get: { settings.activePeakHold },
                                              set: { on in change { $0.activePeakHold = on } }))
                .disabled(!extras)
                .accessibilityIdentifier("activePeakHold")
            SetupNumberRow(title: "Hold for", value: Double(settings.activePeakHoldMs), range: Self.holdRange,
                           step: 100, unit: "ms", enabled: extras && settings.activePeakHold,
                           id: "activePeakHoldMs") { value in change { $0.activePeakHoldMs = Int(value) } }
            SetupNumberRow(title: "Then drop", value: settings.activePeakHoldFallDbPerSec, range: Self.fallRange,
                           step: 1, unit: "dB/s", enabled: extras && settings.activePeakHold,
                           id: "activePeakHoldFall") { value in change { $0.activePeakHoldFallDbPerSec = value } }
            Toggle("Shade down to the trace", isOn: Binding(get: { settings.activePeakHoldFill },
                                                            set: { on in change { $0.activePeakHoldFill = on } }))
                .disabled(!(extras && settings.activePeakHold))
                .accessibilityIdentifier("activePeakHoldFill")
            Toggle("Peaks", isOn: Binding(get: { settings.peakBlobs },
                                          set: { on in change { $0.peakBlobs = on } }))
                .disabled(!extras)
                .accessibilityIdentifier("peakBlobs")
            let blobs = extras && settings.peakBlobs
            SetupNumberRow(title: "How many", value: Double(settings.peakBlobCount), range: Self.blobCountRange,
                           step: 1, unit: "", enabled: blobs,
                           id: "peakBlobCount") { value in change { $0.peakBlobCount = Int(value) } }
            Toggle("Hold them", isOn: Binding(get: { settings.peakBlobHold },
                                              set: { on in change { $0.peakBlobHold = on } }))
                .disabled(!blobs)
                .accessibilityIdentifier("peakBlobHold")
            SetupNumberRow(title: "Hold for", value: Double(settings.peakBlobHoldMs), range: Self.holdRange,
                           step: 100, unit: "ms", enabled: blobs && settings.peakBlobHold,
                           id: "peakBlobHoldMs") { value in change { $0.peakBlobHoldMs = Int(value) } }
            Toggle("Let them fall", isOn: Binding(get: { settings.peakBlobFall },
                                                  set: { on in change { $0.peakBlobFall = on } }))
                .disabled(!blobs)
                .accessibilityIdentifier("peakBlobFall")
            SetupNumberRow(title: "Fall", value: settings.peakBlobFallDbPerSec, range: Self.fallRange, step: 1,
                           unit: "dB/s", enabled: blobs && settings.peakBlobFall,
                           id: "peakBlobFallRate") { value in change { $0.peakBlobFallDbPerSec = value } }
            Toggle("Only inside the filter", isOn: Binding(get: { settings.peakBlobsInsideFilterOnly },
                                                           set: { on in change { $0.peakBlobsInsideFilterOnly = on } }))
                .disabled(!blobs)
                .accessibilityIdentifier("peakBlobsInsideOnly")
        } header: {
            Text("Peaks")
        }
    }

    private var view: some View {
        Section {
            Toggle("Extended view", isOn: Binding(get: { pan.extendedViewOn },
                                                  set: { _ in pan.toggleExtendedView() }))
                .disabled(!pan.extendedViewAvailable)
                .accessibilityIdentifier("extendedView")
            SetupSwitchRow(title: "CTUN", detail: display.ctun ? DisplaySheetModel.ctunOnNote : DisplaySheetModel.ctunOffNote,
                           isOn: Binding(get: { display.ctun }, set: { display.setCtun($0) }), id: "setupCtun")
        } header: {
            Text("View")
        } footer: {
            Text(pan.extendedViewAvailable ? "The radio's full width either side of the band."
                : "Extended view: \(CatalogFeed.needsNewerCoreText)")
        }
    }
}
