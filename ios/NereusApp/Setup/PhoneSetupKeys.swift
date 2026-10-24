// NereusSDR for iOS: the phone's own settings behind the Core-described Setup controls, by their phone keys
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusBand
import NereusMirror
import UIKit

/// Maps each `binding.phone` key the Core describes onto the phone's
/// existing typed stores: this pan's ``BandDisplaySettings`` (the Display
/// rows and the Colors & Theme swatches) and the S-meter's settings (its
/// menu, and the Multimeter page's units and decimal point, kept once for
/// the phone as the desktop keeps them). A change
/// goes through the same paths the native pages use, so every choice is
/// kept as they keep it. A key the phone does not keep reads nil. The
/// phone shows one pan, so the keys the desktop stores once for every pan
/// (the Spectrum Peaks keys, V12's overlays, normalize, the peak value,
/// the grid's noise-floor tracking and dB step) are kept once here too
/// and reach the band the phone shows; the per-band ones (dB Max and dB
/// Min) are kept for the band the pan is on, as the band's own scale.
/// V12's buttons run here as the desktop's do (``perform(_:)``).
@MainActor
final class PhoneSetupKeys: SetupPhoneKeys {
    private let main: MainScreenModel
    private let defaults: UserDefaults
    private let paValues: PaValuesModel?
    /// Where the buttons that open another page lead.
    var navigation: PhoneNavigation?
    /// The Filter Presets page's mode, which the phone keeps (V15).
    @Published private var filterPresetsMode: Int64
    /// The PA temperature's unit on this phone, "C" or "F".
    @Published private var paTempUnit: String
    /// What a copy button puts on the clipboard; the system one unless a
    /// test asks otherwise.
    var copyToClipboard: (String) -> Void = { UIPasteboard.general.string = $0 }

    init(main: MainScreenModel, defaults: UserDefaults = .standard, paValues: PaValuesModel? = nil) {
        self.main = main
        self.defaults = defaults
        self.paValues = paValues
        let kept = defaults.object(forKey: Self.filterPresetsModeDefault) as? Int
        filterPresetsMode = kept.map(Int64.init) ?? Self.filterPresetsModeStart
        let unit = defaults.string(forKey: Self.paTempUnitDefault)
        paTempUnit = unit == Self.fahrenheitUnit ? Self.fahrenheitUnit : Self.celsiusUnit
    }

    /// The PA temperature unit kept on this phone, Celsius until chosen,
    /// as the desktop's PaTempUnit defaults (src/core/PaTempUnit.h).
    static let paTempUnitDefault = "SetupPaTempUnit"
    static let celsiusUnit = "C"
    static let fahrenheitUnit = "F"

    /// The Filter Presets mode kept on this phone, and USB (1) before one is
    /// chosen, as the Core describes it.
    static let filterPresetsModeDefault = "SetupFilterPresetsMode"
    static let filterPresetsModeStart: Int64 = 1

    /// The app-owned Display grid choices, in hertz. These describe the
    /// phone's drawing and are independent of the connected radio.
    static let frequencyGridSpacingChoices = [0, 1000, 2000, 5000, 10000, 25000, 50000, 100000]

    /// Colours kept here without their own alpha: the trace's fill and its
    /// line, drawn opaque.
    static let opaqueColourKeys: Set<String> = ["DisplayFillColor", "DisplayTraceColor"]

    var changes: AnyPublisher<Void, Never> {
        let existing = Publishers.Merge4(main.band.$settings.map { _ in () }, main.sMeter.$state.map { _ in () },
                                          $filterPresetsMode.map { _ in () }, $paTempUnit.map { _ in () })
            .merge(with: main.$displayEditScope.map { _ in () })
            .merge(with: main.$displayBandChoice.map { _ in () })
            .merge(with: main.band.$transmit.map { _ in () })
            .merge(with: main.band.$showsTransmit.map { _ in () })
            .eraseToAnyPublisher()
        guard let paValues else { return existing }
        return existing.merge(with: paValues.objectWillChange.map { _ in () }).eraseToAnyPublisher()
    }

    func value(forPhoneKey key: String) -> SetupValue? {
        let settings = main.displayEditingSettings
        if let path = Self.switches[key] {
            return .bool(settings[keyPath: path])
        }
        if let path = Self.wholes[key] {
            return .integer(Int64(settings[keyPath: path]))
        }
        if let path = Self.colours[key] {
            return Self.rgba(settings[keyPath: path])
        }
        if let path = Self.wholeRates[key] {
            return .integer(Int64(settings[keyPath: path].rounded()))
        }
        if let path = Self.fractions[key] {
            return .decimal(settings[keyPath: path])
        }
        let meter = main.sMeter.state.settings
        switch key {
        case "DisplayFreqGridSpacing": return .integer(Int64(settings.frequencyGridSpacingHz))
        case "DisplayFreqScaleFontPx": return .integer(Int64(settings.frequencyScaleFontPoints))
        case "Display3DSpeed": return .integer(Int64(settings.threeDSpeed))
        case "DisplayShowNoiseFloorPosition":
            return OverlayCorner.allCases.firstIndex(of: settings.noiseFloorPosition).map { .integer(Int64($0)) }
        case "WaterfallBlankingEnabled": return .bool(settings.waterfallBlanking)
        case "WaterfallBlankingThresholdDb": return .integer(Int64(settings.waterfallBlankingThresholdDb))
        case "DisplayBackgroundEnabled": return .bool(settings.backgroundEnabled)
        case "DisplayBackgroundColor": return Self.rgba(settings.backgroundColour)
        case "DisplayBackgroundOpacity": return .integer(Int64(settings.backgroundOpacityPercent))
        case "DisplayCalOffset": return .decimal(settings.calibrationOffsetDb)
        case "DisplayWaterfallLevelsMode":
            return .text(Self.levelToken(settings.waterfallLevelMode))
        case "DisplaySpectrumScaleMode":
            return .text(settings.gridFollowsNoiseFloor ? "TrackFloor" : "Manual")
        case "DisplayLineWidth":
            return .decimal(Double(settings.traceWidthPixels(scale: UIScreen.main.scale)))
        case "DisplayWfBlackLevel": return .integer(Int64(settings.waterfallBlackLevel))
        case "DisplayWfColorGain": return .integer(Int64(settings.waterfallColorGain))
        case "DisplayTxGridRefLevel": return .integer(Int64(settings.txGridTopDbm.rounded()))
        case "DisplayTxGridMin": return .integer(Int64((settings.txGridTopDbm - settings.txGridRangeDb).rounded()))
        case "DisplayPeakValuePosition":
            return OverlayCorner.allCases.firstIndex(of: settings.peakValueCorner).map { .integer(Int64($0)) }
        case "DisplayWfColorScheme":
            // This phone's own gradient is none of the Core's options: no option shows chosen.
            return .integer(Int64(settings.waterfallPaletteId == BandPalette.customPaletteId ? Self.desktopCustomScheme : settings.waterfallPaletteId))
        case "DisplayTxWfPalette":
            return .integer(Int64(settings.txWaterfallPaletteId == BandPalette.customPaletteId ? Self.desktopCustomScheme : settings.txWaterfallPaletteId))
        case "DisplaySpectrumRenderMode":
            return .integer(Int64(settings.spectrumView.rawValue))
        case "DisplayWaterfallHistoryMs":
            return .integer(Int64(settings.rewindSeconds) * 1000)
        case "DisplayWfTimestampPos":
            return TimestampPosition.allCases.firstIndex(of: settings.timestampPosition).map { .integer(Int64($0)) }
        case "DisplayWfTimestampMode":
            return .integer(settings.timestampUtc ? 0 : 1)
        case "DisplayFreqLabelAlign":
            return FrequencyLabelAlignment.allCases.firstIndex(of: settings.frequencyLabelAlignment)
                .map { .integer(Int64($0)) }
        case "DisplayGridMax":
            return .integer(Int64(settings.scaleTopDbm.rounded()))
        case "DisplayGridMin":
            return .integer(Int64(settings.scaleBottomDbm.rounded()))
        case "DisplaySpectrumDetector":
            return .integer(Int64(settings.spectrumDetector.rawValue))
        case "DisplaySpectrumAveraging":
            return .integer(Int64(settings.spectrumAveraging.rawValue))
        case "DisplayWaterfallDetector":
            return .integer(Int64(settings.waterfallDetector.rawValue))
        case "DisplayWaterfallAveraging":
            return .integer(Int64(settings.waterfallAveraging.rawValue))
        case "DisplayFftFillAlpha":
            return .integer(Int64((settings.traceFillOpacity * 100).rounded()))
        case "DisplayFillColor":
            return Self.rgba(settings.traceColour)
        case "DisplayTraceColor":
            // The line's own colour, and the fill's until one is chosen, as
            // the desktop's Trace Color button shows it.
            return Self.rgba(settings.traceLineColourDrawn)
        case "DisplayRxFilterColor":
            guard case .text(let rgb)? = Self.rgba(settings.passbandColour) else { return nil }
            let alpha = Int((min(max(settings.passbandOpacity, 0), 1) * 255).rounded())
            return .text(String(rgb.prefix(7)) + String(format: "%02X", alpha))
        case "SMeter_FaceStyle":
            return SMeterFace.allCases.firstIndex(of: meter.face).map { .integer(Int64($0)) }
        case "PeakHoldEnabled":
            return .bool(meter.peakHold)
        case "PeakDecayRate":
            return SMeterPeakDecay.allCases.firstIndex(of: meter.peakDecay).map { .integer(Int64($0)) }
        case "MultimeterShowDecimal":
            return .bool(meter.showDecimal)
        case "MultimeterUnitMode":
            return SMeterUnit.allCases.firstIndex(of: meter.unit).map { .integer(Int64($0)) }
        case "filterPresetsMode":
            return .integer(filterPresetsMode)
        case PaValuesModel.showPageKey:
            return paValues.map { .bool($0.showPage) }
        case SetupControlDispatcher.paTempUnitKey:
            return .text(paTempUnit)
        default:
            return nil
        }
    }

    func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
        if displayUnavailableReason(forPhoneKey: key) != nil { return false }
        if let path = Self.switches[key] {
            guard let on = value.flag else { return false }
            changeDisplay(forPhoneKey: key) { $0[keyPath: path] = on }
            return true
        }
        if let path = Self.wholes[key] {
            guard let whole = value.whole, let number = Int(exactly: whole) else { return false }
            changeDisplay(forPhoneKey: key) { $0[keyPath: path] = number }
            return true
        }
        if let path = Self.colours[key] {
            guard let text = Self.normalised(value.text) else { return false }
            changeDisplay(forPhoneKey: key) { $0[keyPath: path] = text }
            return true
        }
        if let path = Self.wholeRates[key] {
            guard let whole = value.whole, let number = Int(exactly: whole) else { return false }
            changeDisplay(forPhoneKey: key) { $0[keyPath: path] = Double(number) }
            return true
        }
        if let path = Self.fractions[key] {
            guard let number = value.number else { return false }
            changeDisplay(forPhoneKey: key) { $0[keyPath: path] = number }
            return true
        }
        let whole = value.whole.flatMap { Int(exactly: $0) }
        switch key {
        case "DisplayFreqGridSpacing":
            guard let spacing = whole, Self.frequencyGridSpacingChoices.contains(spacing) else { return false }
            changeDisplay(forPhoneKey: key) { $0.frequencyGridSpacingHz = spacing }
        case "DisplayFreqScaleFontPx":
            guard let size = whole, (8...14).contains(size) else { return false }
            changeDisplay(forPhoneKey: key) { $0.frequencyScaleFontPoints = size }
        case "Display3DSpeed":
            guard let speed = whole, (0...10).contains(speed) else { return false }
            changeDisplay(forPhoneKey: key) { $0.threeDSpeed = speed }
        case "DisplayShowNoiseFloorPosition": return false // Kept but not used, as the accepted live page.
        case "WaterfallBlankingEnabled", "DisplayBackgroundEnabled":
            guard let enabled = value.flag else { return false }
            changeDisplay(forPhoneKey: key) {
                if key == "WaterfallBlankingEnabled" { $0.waterfallBlanking = enabled }
                else { $0.backgroundEnabled = enabled }
            }
        case "WaterfallBlankingThresholdDb":
            guard let threshold = whole, (1...20).contains(threshold) else { return false }
            changeDisplay(forPhoneKey: key) { $0.waterfallBlankingThresholdDb = threshold }
        case "DisplayBackgroundOpacity":
            guard let opacity = whole, (0...100).contains(opacity) else { return false }
            changeDisplay(forPhoneKey: key) { $0.backgroundOpacityPercent = opacity }
        case "DisplayBackgroundColor":
            guard let colour = Self.normalised(value.text) else { return false }
            changeDisplay(forPhoneKey: key) { $0.backgroundColour = colour }
        case "DisplayCalOffset": return false // Core calibration is already in the frame.
        case "DisplayWaterfallLevelsMode":
            guard let token = value.text, let mode = Self.levelModes[token] else { return false }
            changeDisplay(forPhoneKey: key) { $0.waterfallLevelMode = mode }
        case "DisplaySpectrumScaleMode":
            guard let token = value.text, token == "Manual" || token == "TrackFloor" else { return false }
            changeDisplay(forPhoneKey: key) { $0.gridFollowsNoiseFloor = token == "TrackFloor" }
        case "DisplayLineWidth":
            guard let pixels = value.number, pixels.isFinite, (0.1...5).contains(pixels) else { return false }
            changeDisplay(forPhoneKey: key) { $0.traceWidthPoints = pixels / Double(UIScreen.main.scale) }
        case "DisplayWfBlackLevel", "DisplayWfColorGain":
            guard let position = whole, (0...100).contains(position) else { return false }
            changeDisplay(forPhoneKey: key) {
                if key == "DisplayWfBlackLevel" { $0.waterfallBlackLevel = position }
                else { $0.waterfallColorGain = position }
            }
        case "DisplayTxGridRefLevel":
            guard let top = value.number, top.isFinite, (-200...200).contains(top) else { return false }
            changeDisplay(forPhoneKey: key) { $0.txGridTopDbm = top }
        case "DisplayTxGridMin":
            guard let bottom = value.number, bottom.isFinite, (-200...200).contains(bottom),
                  BandDisplaySettings.txGridRangeRange.contains(main.displayEditingSettings.txGridTopDbm - bottom) else { return false }
            changeDisplay(forPhoneKey: key) { $0.txGridRangeDb = $0.txGridTopDbm - bottom }
        case "DisplayPeakValuePosition":
            guard let index = whole, OverlayCorner.allCases.indices.contains(index) else { return false }
            changeDisplay(forPhoneKey: key) { $0.peakValueCorner = OverlayCorner.allCases[index] }
        case "DisplayWfColorScheme":
            guard let scheme = whole, (0...8).contains(scheme) else { return false }
            changeDisplay(forPhoneKey: key) { $0.waterfallPaletteId = scheme == Self.desktopCustomScheme ? BandPalette.customPaletteId : scheme }
        case "DisplayTxWfPalette":
            guard let scheme = whole, (0...8).contains(scheme) else { return false }
            changeDisplay(forPhoneKey: key) { $0.txWaterfallPaletteId = scheme == Self.desktopCustomScheme ? BandPalette.customPaletteId : scheme }
        case "DisplaySpectrumRenderMode":
            // The 3D choice, on a Core that offers the 3D view.
            guard let view = whole.flatMap(SpectrumView.init(rawValue:)) else { return false }
            changeDisplay(forPhoneKey: key) { $0.spectrumView = view }
        case "DisplayWaterfallHistoryMs":
            guard let ms = whole, ms > 0 else { return false }
            changeDisplay(forPhoneKey: key) { $0.rewindSeconds = ms / 1000 }
        case "DisplayWfTimestampPos":
            guard let index = whole, TimestampPosition.allCases.indices.contains(index) else { return false }
            changeDisplay(forPhoneKey: key) { $0.timestampPosition = TimestampPosition.allCases[index] }
        case "DisplayWfTimestampMode":
            guard let mode = whole, mode == 0 || mode == 1 else { return false }
            changeDisplay(forPhoneKey: key) { $0.timestampUtc = mode == 0 }
        case "DisplayFreqLabelAlign":
            guard let index = whole, FrequencyLabelAlignment.allCases.indices.contains(index) else { return false }
            changeDisplay(forPhoneKey: key) { $0.frequencyLabelAlignment = FrequencyLabelAlignment.allCases[index] }
        case "DisplayGridMax":
            // The band the pan is on keeps it, as the desktop's per-band dB Max.
            guard let top = whole else { return false }
            changeDisplay(forPhoneKey: key) { $0.scaleTopDbm = Double(top) }
        case "DisplayGridMin":
            guard let bottom = whole else { return false }
            changeDisplay(forPhoneKey: key) { $0.scaleBottomDbm = Double(bottom) }
        case "DisplaySpectrumDetector":
            guard let detector = whole.flatMap(SpectrumDetector.init(rawValue:)) else { return false }
            changeDisplay(forPhoneKey: key) { $0.spectrumDetector = detector }
        case "DisplayWaterfallDetector":
            guard let detector = whole.flatMap(SpectrumDetector.init(rawValue:)), detector != .rms else { return false }
            changeDisplay(forPhoneKey: key) { $0.waterfallDetector = detector }
        case "DisplaySpectrumAveraging":
            guard let averaging = whole.flatMap(SpectrumAveraging.init(rawValue:)) else { return false }
            changeDisplay(forPhoneKey: key) { $0.spectrumAveraging = averaging }
        case "DisplayWaterfallAveraging":
            guard let averaging = whole.flatMap(SpectrumAveraging.init(rawValue:)) else { return false }
            changeDisplay(forPhoneKey: key) { $0.waterfallAveraging = averaging }
        case "DisplayFftFillAlpha":
            // The percent the Core describes, kept as the typed fraction.
            guard let percent = whole, (0...100).contains(percent) else { return false }
            changeDisplay(forPhoneKey: key) { $0.traceFillOpacity = Double(percent) / 100 }
        case "DisplayFillColor":
            guard let text = Self.normalised(value.text) else { return false }
            changeDisplay(forPhoneKey: key) { $0.traceColour = String(text.prefix(7)) }
        case "DisplayTraceColor":
            guard let text = Self.normalised(value.text) else { return false }
            changeDisplay(forPhoneKey: key) { $0.traceLineColour = String(text.prefix(7)) }
        case "DisplayRxFilterColor":
            guard let text = Self.normalised(value.text), let alpha = Int(text.suffix(2), radix: 16) else {
                return false
            }
            changeDisplay(forPhoneKey: key) {
                $0.passbandColour = String(text.prefix(7))
                $0.passbandOpacity = Double(alpha) / 255
            }
        case "SMeter_FaceStyle":
            guard let index = whole, SMeterFace.allCases.indices.contains(index) else { return false }
            main.sMeter.chooseFace(SMeterFace.allCases[index])
        case "PeakHoldEnabled":
            guard let on = value.flag else { return false }
            main.sMeter.setPeakHold(on)
        case "PeakDecayRate":
            guard let index = whole, SMeterPeakDecay.allCases.indices.contains(index) else { return false }
            main.sMeter.choosePeakDecay(SMeterPeakDecay.allCases[index])
        case "MultimeterShowDecimal":
            guard let on = value.flag else { return false }
            main.sMeter.setShowDecimal(on)
        case "MultimeterUnitMode":
            // The Core's options in order: S, dBm, uV.
            guard let index = whole, SMeterUnit.allCases.indices.contains(index) else { return false }
            main.sMeter.chooseUnit(SMeterUnit.allCases[index])
        case "filterPresetsMode":
            // Any of the Core's modes; the row's own options bound it.
            guard let mode = value.whole, mode >= 0, let number = Int(exactly: mode) else { return false }
            filterPresetsMode = mode
            defaults.set(number, forKey: Self.filterPresetsModeDefault)
        case PaValuesModel.showPageKey:
            guard let on = value.flag else { return false }
            return paValues?.setShowPage(on) ?? false
        case SetupControlDispatcher.paTempUnitKey:
            guard let unit = value.text, unit == Self.celsiusUnit || unit == Self.fahrenheitUnit else { return false }
            paTempUnit = unit
            defaults.set(unit, forKey: Self.paTempUnitDefault)
        default:
            return false
        }
        return true
    }

    // MARK: V12's buttons

    /// The buttons this phone runs, by their action.
    static let actions: Set<String> = ["smoothDefaults", "copySpectrumMinMax", "copyWaterfallThresholds",
                                       "resetColors", "reset3d", "copySupportInfo"]
    /// V15's buttons that open another page, run once the root view says
    /// where they lead.
    static let navigationActions: Set<String> = ["openTxEq", "openSetupPage"]

    func canPerform(_ action: String) -> Bool {
        if displayUnavailableReason(forPhoneKey: action) != nil { return false }
        switch action {
        case "editReceiveGradient": return main.displayEditingSettings.waterfallPaletteId == BandPalette.customPaletteId
        case "editTransmitGradient": return main.displayEditingSettings.txWaterfallPaletteId == BandPalette.customPaletteId
        case "ClarityRetune": return main.displayEditingSettings.waterfallLevelMode == .clarity && main.display.retuneAvailable
        case "chooseBackgroundImage": return true
        case "clearBackgroundImage": return !main.displayEditingSettings.backgroundImagePath.isEmpty
        case "openDisplaySpectrum", "openDisplayPeaks", "openDisplayMultimeter": return navigation != nil
        default: break
        }
        return (action == PaValuesModel.resetAction && paValues?.canReset == true) || Self.actions.contains(action) || (Self.navigationActions.contains(action) && navigation != nil)
    }

    func perform(_ action: String, argument: String?) -> Bool {
        switch action {
        case "openTxEq":
            guard let navigation else { return false }
            navigation.openTxEqualizer()
            return true
        case "openSetupPage":
            guard let navigation, let page = argument else { return false }
            return navigation.openSetupPage(page)
        case "copySupportInfo":
            guard let text = argument else { return false }
            copyToClipboard(text)
            return true
        default:
            return perform(action)
        }
    }

    /// The frozen row keeps its original action semantics; native/current
    /// Appearance resets only the nine colors still on that page.
    func perform(_ action: String, descriptionVersion: Int) -> Bool {
        if action == "resetColors", descriptionVersion <= 28 {
            main.changeDisplay(Self.resetLegacyColours)
            return true
        }
        if action == "smoothDefaults", descriptionVersion <= 28 {
            // Exact ef74a7707:346–355: the old action enabled AGC even
            // beneath Clarity/Scale, without changing their other flags.
            main.changeDisplay { settings in
                let clarity = settings.clarityEnabled, floor = settings.waterfallNfAgc
                let scale = settings.useSpectrumMinMax
                Self.smoothDefaults(&settings)
                settings.clarityEnabled = clarity; settings.waterfallNfAgc = floor
                settings.useSpectrumMinMax = scale; settings.waterfallAgc = true
            }
            return true
        }
        return perform(action)
    }

    func perform(_ action: String, argument: String?, descriptionVersion: Int) -> Bool {
        if argument == nil && (action == "resetColors" || action == "smoothDefaults") {
            return perform(action, descriptionVersion: descriptionVersion)
        }
        return perform(action, argument: argument)
    }

    func perform(_ action: String) -> Bool {
        if displayUnavailableReason(forPhoneKey: action) != nil { return false }
        switch action {
        case "editReceiveGradient", "editTransmitGradient":
            guard canPerform(action) else { return false }
            main.display.gradientEditor = action == "editReceiveGradient" ? .receive : .transmit
        case "ClarityRetune":
            guard canPerform(action) else { return false }
            main.display.retune()
        case "chooseBackgroundImage": main.display.choosingBackgroundPicture = true
        case "clearBackgroundImage":
            guard canPerform(action) else { return false }
            main.changeDisplay { $0.backgroundImagePath = "" }
        case "openDisplaySpectrum", "openDisplayPeaks", "openDisplayMultimeter":
            guard let navigation else { return false }
            let page = action == "openDisplaySpectrum" ? "display.spectrumDefaults"
                : action == "openDisplayPeaks" ? "display.spectrumPeaks" : "display.multimeter"
            return navigation.openSetupPage(page)
        case PaValuesModel.resetAction:
            return paValues?.reset() ?? false
        case "smoothDefaults":
            main.applySmoothDisplayDefaults()
        case "copySpectrumMinMax":
            // The pan's scale as it is drawn now becomes the thresholds.
            main.changeDisplay { settings in
                let scale = settings.scaleRange
                settings.waterfallHighDbm = scale.upperBound
                settings.waterfallLowDbm = scale.lowerBound
            }
        case "copyWaterfallThresholds":
            // The band's dB Max and dB Min become the thresholds, rounded.
            main.changeDisplay { settings in
                settings.scaleTopDbm = settings.waterfallHighDbm.rounded()
                settings.scaleBottomDbm = settings.waterfallLowDbm.rounded()
            }
        case "resetColors":
            main.changeDisplay(Self.resetColours)
        case "reset3d":
            // The six 3D rows back, 3D Floor for the pan's band only.
            main.changeDisplay { $0.resetThreeD() }
        default:
            return false
        }
        return true
    }

    /// The desktop's Reset to Smooth Defaults, as the Core's V12 note lists
    /// it: Clarity Blue, Log Recursive averaging, the white trace, no fill
    /// under it, the waterfall AGC on and a 30 ms update period; with D97's
    /// 650 ms spectrum average time. Nothing else moves. The white goes to
    /// the fill (`traceColour`, the desktop's `DisplayFillColor`) and to the
    /// trace line (`traceLineColour`, the desktop's `DisplayTraceColor`), as
    /// the desktop's button sets both: a trace color chosen earlier does
    /// not outlive the reset.
    static func smoothDefaults(_ settings: inout BandDisplaySettings) {
        settings.waterfallPaletteId = clarityBlueScheme
        settings.spectrumAveraging = .logRecursive
        settings.spectrumAverageTimeMs = smoothAverageTimeMs
        settings.traceColour = smoothTraceColour
        settings.traceLineColour = smoothTraceColour
        settings.traceFill = false
        if settings.waterfallLevelMode == .manual { settings.waterfallLevelMode = .agc }
        settings.waterfallPeriodMs = smoothUpdatePeriodMs
    }

    static let clarityBlueScheme = 7
    static let smoothAverageTimeMs = 650
    static let smoothTraceColour = "#FFFFFF"
    static let smoothUpdatePeriodMs = 30

    /// Reset the nine device-shared Appearance colors. Trace, fill and
    /// background remain owned by their Display pages.
    static func resetColours(_ settings: inout BandDisplaySettings) {
        let defaults = BandDisplaySettings.desktopDefaults
        // Appearance's reset keeps trace/fill/background in their new owner pages.
        settings.gridColour = defaults.gridColour
        settings.gridFineColour = defaults.gridFineColour
        settings.hGridColour = defaults.hGridColour
        settings.gridTextColour = defaults.gridTextColour
        settings.bandEdgeColour = defaults.bandEdgeColour
        settings.rxZeroLineColour = defaults.rxZeroLineColour
        settings.txZeroLineColour = defaults.txZeroLineColour
        settings.passbandColour = defaults.passbandColour
        settings.passbandOpacity = defaults.passbandOpacity
        settings.txPassbandColour = defaults.txPassbandColour
    }

    /// Exact original ef74a7707 PhoneSetupKeys.swift:365–379. The old
    /// Appearance page included fill and did not include band-edge color.
    static func resetLegacyColours(_ settings: inout BandDisplaySettings) {
        let defaults = BandDisplaySettings.desktopDefaults
        settings.traceColour = defaults.traceColour
        settings.gridColour = defaults.gridColour
        settings.gridFineColour = defaults.gridFineColour
        settings.hGridColour = defaults.hGridColour
        settings.gridTextColour = defaults.gridTextColour
        settings.rxZeroLineColour = defaults.rxZeroLineColour
        settings.txZeroLineColour = defaults.txZeroLineColour
        settings.passbandColour = defaults.passbandColour
        settings.passbandOpacity = defaults.passbandOpacity
        settings.txPassbandColour = defaults.txPassbandColour
    }

    private func changeDisplay(forPhoneKey key: String, _ change: (inout BandDisplaySettings) -> Void) {
        main.changeDisplay(fields: Self.displayFields(forPhoneKey: key), change)
    }

    /// Exact established typed carriers; virtual mode is one coherent stored
    /// tuple and RX filter color carries its existing color/alpha pair.
    static func displayFields(forPhoneKey key: String) -> Set<String> {
        if key == "DisplayWaterfallLevelsMode" { return BandDisplaySettingsStore.waterfallModeFields }
        if key == "DisplayRxFilterColor" { return ["passbandColour", "passbandOpacity"] }
        return displayFieldNames[key].map { [$0] } ?? []
    }
    static func pictureFields(on page: SetupDescription.Page) -> Set<String> {
        let controls = page.sections.flatMap(\.controls)
        var fields = controls.reduce(into: Set<String>()) { result, control in
            guard control.owner == .pan, control.kind != .button, control.perBand == nil,
                  case .phone(let key)? = control.binding else { return }
            result.formUnion(displayFields(forPhoneKey: key).intersection(BandDisplaySettingsStore.pictureFields))
        }
        // The local picker action carries a stored picture path; the action
        // itself contributes no own marker, but page reset/copy includes its value.
        if controls.contains(where: { $0.owner == .pan && $0.binding == .phone("chooseBackgroundImage") }) {
            fields.insert("backgroundImagePath")
        }
        return fields
    }
    private static let displayFieldNames: [String: String] = [
        "ClarityEnabled": "clarityEnabled",
        "Display3DAngle": "threeDAngle",
        "Display3DFloorDepth": "threeDFloors",
        "Display3DGain": "threeDGain",
        "Display3DSliceShadow": "threeDSliceShadow",
        "Display3DSpan": "threeDSpan",
        "Display3DSpeed": "threeDSpeed",
        "DisplayActivePeakHoldColor": "peakHoldColour",
        "DisplayActivePeakHoldDropDbPerSec": "activePeakHoldFallDbPerSec",
        "DisplayActivePeakHoldDurationMs": "activePeakHoldMs",
        "DisplayActivePeakHoldEnabled": "activePeakHold",
        "DisplayActivePeakHoldFill": "activePeakHoldFill",
        "DisplayActivePeakHoldOnTx": "activePeakHoldOnTx",
        "DisplayAdjustGridMinToNoiseFloor": "gridFollowsNoiseFloor",
        "DisplayBackgroundColor": "backgroundColour",
        "DisplayBackgroundEnabled": "backgroundEnabled",
        "DisplayBackgroundOpacity": "backgroundOpacityPercent",
        "DisplayBandEdgeColor": "bandEdgeColour",
        "DisplayDbmScaleVisible": "showDbmScale",
        "DisplayDispNormalize": "normalize",
        "DisplayFftFillAlpha": "traceFillOpacity",
        "DisplayFillColor": "traceColour",
        "DisplayFreqGridSpacing": "frequencyGridSpacingHz",
        "DisplayFreqLabelAlign": "frequencyLabelAlignment",
        "DisplayFreqScaleFontPx": "frequencyScaleFontPoints",
        "DisplayGradientEnabled": "traceGradient",
        "DisplayGridColor": "gridColour",
        "DisplayGridEnabled": "grid",
        "DisplayGridFineColor": "gridFineColour",
        "DisplayGridMax": "scaleTopDbm",
        "DisplayGridMin": "scaleBottomDbm",
        "DisplayGridStep": "gridStepDb",
        "DisplayGridTextColor": "gridTextColour",
        "DisplayHGridColor": "hGridColour",
        "DisplayLineWidth": "traceWidthPoints",
        "DisplayMaintainNFAdjustDelta": "gridKeepsRange",
        "DisplayNFOffsetGridFollow": "gridNoiseFloorOffsetDb",
        "DisplayNoiseFloorColor": "noiseFloorColour",
        "DisplayNoiseFloorFastColor": "noiseFloorFastColour",
        "DisplayNoiseFloorLineWidth": "noiseFloorLineWidth",
        "DisplayNoiseFloorShiftDb": "noiseFloorShiftDb",
        "DisplayNoiseFloorTextColor": "noiseFloorTextColour",
        "DisplayPanFill": "traceFill",
        "DisplayPeakBlobColor": "peakBlobColour",
        "DisplayPeakBlobTextColor": "peakBlobTextColour",
        "DisplayPeakBlobsCount": "peakBlobCount",
        "DisplayPeakBlobsEnabled": "peakBlobs",
        "DisplayPeakBlobsFallDbPerSec": "peakBlobFallDbPerSec",
        "DisplayPeakBlobsHoldDrop": "peakBlobFall",
        "DisplayPeakBlobsHoldEnabled": "peakBlobHold",
        "DisplayPeakBlobsHoldMs": "peakBlobHoldMs",
        "DisplayPeakBlobsInsideFilterOnly": "peakBlobsInsideFilterOnly",
        "DisplayPeakHoldEnabled": "peakHold",
        "DisplayPeakHoldResetMs": "peakHoldDelayMs",
        "DisplayPeakTextDelayMs": "peakValueDelayMs",
        "DisplayPeakValuePosition": "peakValueCorner",
        "DisplayRxZeroLineColor": "rxZeroLineColour",
        "DisplayShowBinWidth": "showBinWidth",
        "DisplayShowCursorFreq": "showCursorFrequency",
        "DisplayShowFps": "showFps",
        "DisplayShowNoiseFloor": "noiseFloorLine",
        "DisplayShowNoiseFloorPosition": "noiseFloorPosition",
        "DisplayShowPeakValueOverlay": "showPeakValue",
        "DisplayShowRxFilterOnWaterfall": "showRxFilterOnWaterfall",
        "DisplayShowRxZeroLine": "showRxZeroLineOnWaterfall",
        "DisplayShowTxFilterOnRxWaterfall": "showTxFilterOnWaterfall",
        "DisplayShowTxZeroLine": "showTxZeroLineOnWaterfall",
        "DisplayShowZeroLine": "showZeroLine",
        "DisplaySpectrumAverageTimeMs": "spectrumAverageTimeMs",
        "DisplaySpectrumAveraging": "spectrumAveraging",
        "DisplaySpectrumDetector": "spectrumDetector",
        "DisplaySpectrumRenderMode": "spectrumView",
        "DisplaySpectrumScaleMode": "gridFollowsNoiseFloor",
        "DisplayTraceColor": "traceLineColour",
        "DisplayTxFilterColor": "txPassbandColour",
        "DisplayTxGridMin": "txGridRangeDb",
        "DisplayTxGridRefLevel": "txGridTopDbm",
        "DisplayTxWfHighLevel": "txWaterfallHighDbm",
        "DisplayTxWfLowColor": "txWaterfallLowColour",
        "DisplayTxWfLowLevel": "txWaterfallLowDbm",
        "DisplayTxWfPalette": "txWaterfallPaletteId",
        "DisplayTxZeroLineColor": "txZeroLineColour",
        "DisplayWaterfallAverageTimeMs": "waterfallAverageTimeMs",
        "DisplayWaterfallAveraging": "waterfallAveraging",
        "DisplayWaterfallDetector": "waterfallDetector",
        "DisplayWaterfallHistoryMs": "rewindSeconds",
        "DisplayWfAgc": "waterfallAgc",
        "DisplayWfBlackLevel": "waterfallBlackLevel",
        "DisplayWfColorGain": "waterfallColorGain",
        "DisplayWfColorScheme": "waterfallPaletteId",
        "DisplayWfHighLevel": "waterfallHighDbm",
        "DisplayWfLowLevel": "waterfallLowDbm",
        "DisplayWfOpacity": "waterfallOpacityPercent",
        "DisplayWfTimestampMode": "timestampUtc",
        "DisplayWfTimestampPos": "timestampPosition",
        "DisplayWfUpdatePeriodMs": "waterfallPeriodMs",
        "DisplayWfUseSpectrumMinMax": "useSpectrumMinMax",
        "WaterfallAGCOffsetDb": "waterfallOffsetDb",
        "WaterfallBlankingEnabled": "waterfallBlanking",
        "WaterfallBlankingThresholdDb": "waterfallBlankingThresholdDb",
        "WaterfallNFAGCEnabled": "waterfallNfAgc",
        "WaterfallStopOnTx": "waterfallStopOnTx",
        "decimation": "decimation",
    ]

    var perBandName: String? {
        // 160m to XVTR, and 2 m as the Core names it (band 27).
        main.displayEditingBand.flatMap(Int.init).flatMap(SetupDescription.bandName)
    }

    var screenRefreshRate: Int? {
        UIScreen.main.maximumFramesPerSecond
    }

    /// The desktop's Custom colour scheme (`DisplayWfColorScheme` and
    /// `DisplayTxWfPalette` 6): the desktop's own colours, which the Core's
    /// catalogue leaves out (no stops it can send), so this phone cannot
    /// draw it and shows it disabled (`display-v12-for-phone.md`, "Not
    /// described"). It is not this phone's own gradient.
    static let desktopCustomScheme = 6

    /// Why the desktop's Custom scheme cannot be chosen here.
    static let customSchemeReason = "The Core does not send the desktop's custom colors."

    /// Why History duration is greyed: the desktop's setting spans its
    /// signal history graph, which this phone does not draw.
    static let noHistoryGraphReason = "This phone has no signal history graph."

    /// Why the PA Values page's switch and its peak and minimum reset are
    /// greyed: this phone does not track the running peak and minimum.
    static let paValuesPeakReason = "Peak and minimum PA readings are kept on the desktop."

    /// Accepted HTML581/700–711: every receive Levels row of a keyed
    /// selected pan is unavailable; All keeps editing the remembered master.
    /// Copy actions inherit the gate of the receive rows they would change.
    private static let receiveLevelsKeys: Set<String> = [
        "DisplaySpectrumScaleMode", "DisplayGridMax", "DisplayGridMin",
        "DisplayNFOffsetGridFollow", "DisplayMaintainNFAdjustDelta", "copyWaterfallThresholds",
        "DisplayWaterfallLevelsMode", "DisplayWfBlackLevel", "DisplayWfColorGain",
        "WaterfallAGCOffsetDb", "DisplayWfLowLevel", "DisplayWfHighLevel",
        "ClarityRetune", "copySpectrumMinMax",
    ]
    private func displayUnavailableReason(forPhoneKey key: String) -> String? {
        if main.displayEditScope == .pan(.first), main.band.transmitOverlay, Self.receiveLevelsKeys.contains(key) {
            return "Pan 1 is transmitting. Receive scale and level settings are kept until it stops. Change transmit settings on Transmit Display."
        }
        let display = main.displayEditingSettings
        if display.gridFollowsNoiseFloor {
            if key == "DisplayGridMin" { return "Track floor sets dB Min." }
            if key == "DisplayGridMax" && display.gridKeepsRange { return "Track floor with Keep Range sets dB Max." }
        }
        return nil
    }

    func unavailableReason(forPhoneKey key: String) -> String? {
        if let reason = displayUnavailableReason(forPhoneKey: key) { return reason }
        switch key {
        case "DisplayShowNoiseFloorPosition":
            return "Not used: the noise floor text sits beside its box. The setting is kept but has no visual effect."
        case "DisplayThreadPriority":
            return "Thread Priority is controlled by the Core computer. This phone does not run the Core FFT thread."
        case "DisplayCalOffset":
            return BandDisplaySettings.calibrationReason
        case "MultimeterSignalHistoryDurationMs":
            return Self.noHistoryGraphReason
        case "display/showPaValuesPage", "resetPaValues":
            return paValues == nil ? Self.paValuesPeakReason : nil
        default:
            return nil
        }
    }

    func unavailableOptions(forPhoneKey key: String) -> [Int64: String] {
        switch key {
        case "DisplayWfColorScheme", "DisplayTxWfPalette":
            return [:]
        default:
            return [:]
        }
    }

    static let levelModes: [String: BandDisplaySettings.WaterfallLevelMode] = [
        "Clarity": .clarity, "Auto": .agc, "Floor": .noiseFloorAgc, "Manual": .manual, "Scale": .scale,
    ]
    static func levelToken(_ mode: BandDisplaySettings.WaterfallLevelMode) -> String {
        switch mode {
        case .clarity: return "Clarity"
        case .agc: return "Auto"
        case .noiseFloorAgc: return "Floor"
        case .manual: return "Manual"
        case .scale: return "Scale"
        }
    }

    // MARK: The keys

    /// On/off keys: V9's rendering switches, V10's overlays and V11's
    /// Spectrum Peaks switches.
    static let switches: [String: WritableKeyPath<BandDisplaySettings, Bool>] = [
        "DisplayPanFill": \.traceFill,
        "DisplayGradientEnabled": \.traceGradient,
        "DisplayPeakHoldEnabled": \.peakHold,
        "WaterfallStopOnTx": \.waterfallStopOnTx,
        "DisplayShowRxFilterOnWaterfall": \.showRxFilterOnWaterfall,
        "DisplayShowTxFilterOnRxWaterfall": \.showTxFilterOnWaterfall,
        "DisplayShowRxZeroLine": \.showRxZeroLineOnWaterfall,
        "DisplayShowTxZeroLine": \.showTxZeroLineOnWaterfall,
        "DisplayActivePeakHoldEnabled": \.activePeakHold,
        "DisplayActivePeakHoldFill": \.activePeakHoldFill,
        "DisplayActivePeakHoldOnTx": \.activePeakHoldOnTx,
        "DisplayPeakBlobsEnabled": \.peakBlobs,
        "DisplayPeakBlobsInsideFilterOnly": \.peakBlobsInsideFilterOnly,
        "DisplayPeakBlobsHoldEnabled": \.peakBlobHold,
        "DisplayPeakBlobsHoldDrop": \.peakBlobFall,
        // V12
        "ClarityEnabled": \.clarityEnabled,
        "DisplayWfAgc": \.waterfallAgc,
        "WaterfallNFAGCEnabled": \.waterfallNfAgc,
        "DisplayWfUseSpectrumMinMax": \.useSpectrumMinMax,
        "DisplayShowCursorFreq": \.showCursorFrequency,
        "DisplayShowBinWidth": \.showBinWidth,
        "DisplayShowNoiseFloor": \.noiseFloorLine,
        "DisplayDispNormalize": \.normalize,
        "DisplayShowPeakValueOverlay": \.showPeakValue,
        "DisplayGridEnabled": \.grid,
        "DisplayDbmScaleVisible": \.showDbmScale,
        "DisplayShowZeroLine": \.showZeroLine,
        "DisplayShowFps": \.showFps,
        "DisplayAdjustGridMinToNoiseFloor": \.gridFollowsNoiseFloor,
        "DisplayMaintainNFAdjustDelta": \.gridKeepsRange,
        // V12's 3D View page
        "Display3DSliceShadow": \.threeDSliceShadow,
    ]

    /// Whole-number keys kept as the same number.
    static let wholes: [String: WritableKeyPath<BandDisplaySettings, Int>] = [
        "DisplaySpectrumAverageTimeMs": \.spectrumAverageTimeMs,
        "DisplayWaterfallAverageTimeMs": \.waterfallAverageTimeMs,
        "decimation": \.decimation,
        "DisplayPeakHoldResetMs": \.peakHoldDelayMs,
        "DisplayWfUpdatePeriodMs": \.waterfallPeriodMs,
        "DisplayWfOpacity": \.waterfallOpacityPercent,
        "DisplayActivePeakHoldDurationMs": \.activePeakHoldMs,
        "DisplayPeakBlobsCount": \.peakBlobCount,
        "DisplayPeakBlobsHoldMs": \.peakBlobHoldMs,
        // V12
        "DisplayPeakTextDelayMs": \.peakValueDelayMs,
        "WaterfallAGCOffsetDb": \.waterfallOffsetDb,
        "DisplayNFOffsetGridFollow": \.gridNoiseFloorOffsetDb,
        // V12's 3D View page: 3D Floor is kept for the pan's band.
        "Display3DFloorDepth": \.threeDFloorDb,
        "Display3DGain": \.threeDGain,
        "Display3DSpan": \.threeDSpan,
        "Display3DAngle": \.threeDAngle,
    ]

    /// Whole numbers the Core describes (rates in dB/s, levels in dBm, the
    /// grid's dB step), kept as the typed Double.
    static let wholeRates: [String: WritableKeyPath<BandDisplaySettings, Double>] = [
        "DisplayActivePeakHoldDropDbPerSec": \.activePeakHoldFallDbPerSec,
        "DisplayPeakBlobsFallDbPerSec": \.peakBlobFallDbPerSec,
        // V12
        "DisplayWfHighLevel": \.waterfallHighDbm,
        "DisplayWfLowLevel": \.waterfallLowDbm,
        "DisplayGridStep": \.gridStepDb,
        "DisplayTxWfLowLevel": \.txWaterfallLowDbm,
        "DisplayTxWfHighLevel": \.txWaterfallHighDbm,
    ]

    /// Decimals the Core describes, kept as the same number (V12).
    static let fractions: [String: WritableKeyPath<BandDisplaySettings, Double>] = [
        "DisplayNoiseFloorShiftDb": \.noiseFloorShiftDb,
        "DisplayNoiseFloorLineWidth": \.noiseFloorLineWidth,
    ]

    /// Colours kept as `#RRGGBBAA`, the same as the description's.
    static let colours: [String: WritableKeyPath<BandDisplaySettings, String>] = [
        "DisplayGridColor": \.gridColour,
        "DisplayGridFineColor": \.gridFineColour,
        "DisplayHGridColor": \.hGridColour,
        "DisplayGridTextColor": \.gridTextColour,
        "DisplayBandEdgeColor": \.bandEdgeColour,
        "DisplayRxZeroLineColor": \.rxZeroLineColour,
        "DisplayTxZeroLineColor": \.txZeroLineColour,
        "DisplayTxFilterColor": \.txPassbandColour,
        "DisplayActivePeakHoldColor": \.peakHoldColour,
        "DisplayPeakBlobColor": \.peakBlobColour,
        "DisplayPeakBlobTextColor": \.peakBlobTextColour,
        // V12
        "DisplayNoiseFloorColor": \.noiseFloorColour,
        "DisplayNoiseFloorTextColor": \.noiseFloorTextColour,
        "DisplayNoiseFloorFastColor": \.noiseFloorFastColour,
        "DisplayTxWfLowColor": \.txWaterfallLowColour,
    ]

    /// A kept colour as `#RRGGBBAA`, capitals; `#RRGGBB` reads as opaque.
    static func rgba(_ text: String) -> SetupValue? {
        let upper = text.uppercased()
        guard upper.first == "#", upper.dropFirst().allSatisfy(\.isHexDigit) else { return nil }
        switch upper.count {
        case 7:
            return .text(upper + "FF")
        case 9:
            return .text(upper)
        default:
            return nil
        }
    }

    private static func normalised(_ text: String?) -> String? {
        guard let text, text.count == 9, case .text(let upper)? = rgba(text) else { return nil }
        return upper
    }
}

/// Where Setup's buttons that open another page lead: a Setup page by the
/// Core's id, and the TX Equalizer in Tools.
@MainActor
struct PhoneNavigation {
    /// Opens the Core-described page with this id; false when there is none.
    let openSetupPage: (String) -> Bool
    let openTxEqualizer: () -> Void
}
