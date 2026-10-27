// NereusSDR for iOS: the Display pill's five tabs and each tab's rows, in the desktop pill's order
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror

/// The Display pill's tabs, as the desktop's pill names them (spec DC1).
enum DisplayPillTab: String, CaseIterable {
    case spectrum, levels, waterfall, view, fft

    var title: String {
        switch self {
        case .spectrum: "Spectrum"
        case .levels: "Levels"
        case .waterfall: "Waterfall"
        case .view: "View"
        case .fft: "FFT"
        }
    }

    /// The version 29 Display page that holds this tab's defaults.
    var setupPageId: String {
        switch self {
        case .spectrum: "display.spectrumDefaults"
        case .levels: "display.levels"
        case .waterfall: "display.waterfallDefaults"
        case .view: "display.threeD"
        case .fft: "display.fftEngine"
        }
    }
}

/// A row the phone draws itself: kept on this phone, or a Core value the
/// description does not carry as a row (spec DC2, DC5).
enum PhoneRow {
    case spectrumHeight, lookBack, ctun, dup, decimation
}

/// One line of a tab: a group's caption, a row the Core describes (by the
/// desktop's setting key), a row the phone draws, a link to a Setup page,
/// or the FFT tab's readout of what the Core runs.
enum DisplayPillRow: Equatable {
    case group(String)
    case setting(key: String)
    case phone(PhoneRow)
    case link(title: String, pageId: String)
    /// The bin width the Core runs now (the desktop's `pillFftReadout`).
    case fftReadout
}

/// Each tab's rows: the desktop pill's keys in the order its page function
/// builds them, with the phone's rows where spec DC2 puts them.
enum DisplayPillRows {
    static func rows(_ tab: DisplayPillTab) -> [DisplayPillRow] {
        switch tab {
        case .spectrum: spectrum
        case .levels: levels
        case .waterfall: waterfall
        case .view: view
        case .fft: fft
        }
    }

    /// The desktop's setting keys of `tab`, in order.
    static func settingKeys(_ tab: DisplayPillTab) -> [String] {
        rows(tab).compactMap { row in
            if case .setting(let key) = row { return key }
            return nil
        }
    }

    /// The key a version 29 description binds for a desktop pill key. Most
    /// are the same; these five are named differently in Setup.
    static let descriptionKeys: [String: String] = [
        // From src/gui/SpectrumOverlayPanel.cpp:3494 (SpectrumScaleMode); Setup's Levels page row.
        "SpectrumScaleMode": "DisplaySpectrumScaleMode",
        // From src/gui/SpectrumOverlayPanel.cpp:3556 (WaterfallLevelsMode); Setup's Levels page row.
        "WaterfallLevelsMode": "DisplayWaterfallLevelsMode",
        // From src/gui/SpectrumOverlayPanel.cpp:4253 (Display3DFloor); Setup's View page row.
        "Display3DFloor": "Display3DFloorDepth",
        // From src/gui/SpectrumOverlayPanel.cpp:4368 (the Choose button's row); Setup's View page button.
        "DisplayBackgroundImage": "chooseBackgroundImage",
        // From src/gui/SpectrumOverlayPanel.cpp:4369 (the Clear button's row); Setup's View page button.
        "DisplayBackgroundImageClear": "clearBackgroundImage",
    ]

    /// The desktop pill's Clear beside Choose, the description's "Clear picture" button.
    // From src/gui/SpectrumOverlayPanel.cpp:4369 (DisplayBackgroundImageClear)
    static let clearPictureKey = "DisplayBackgroundImageClear"

    static func descriptionKey(for key: String) -> String {
        descriptionKeys[key] ?? key
    }

    /// The control `description` binds to the desktop key `key`, on any of
    /// its pages (the Spectrum tab's markers live on Peaks & Markers).
    static func control(for key: String, in description: SetupDescription) -> SetupDescription.Control? {
        let bound = descriptionKey(for: key)
        for page in description.pages {
            for section in page.sections {
                for control in section.controls {
                    switch control.binding {
                    case .phone(let name)? where name == bound: return control
                    case .setting(let name)? where name == bound: return control
                    default: continue
                    }
                }
            }
        }
        return nil
    }

    /// The description keys of the Levels tab: every one is greyed while
    /// the pan transmits (spec section 9).
    static var levelsDescriptionKeys: Set<String> {
        Set(settingKeys(.levels).map(descriptionKey(for:)))
    }

    /// The link under the Spectrum tab's markers.
    // From src/gui/SpectrumOverlayPanel.cpp:3333-3341 (pillSpectrumMarkersNote opens Spectrum Peaks)
    static let peaksLinkTitle = "Peaks & Markers in Setup \u{203A}"
    static let peaksPageId = "display.spectrumPeaks"

    // From src/gui/SpectrumOverlayPanel.cpp:3152 (buildPillSpectrumPage)
    private static let spectrum: [DisplayPillRow] = [
        .group("Trace"), // :3154
        .setting(key: "DisplaySpectrumDetector"), // :3164
        .setting(key: "DisplayDispNormalize"), // :3174
        .setting(key: "DisplayPanFill"), // :3178
        .setting(key: "DisplayGradientEnabled"), // :3182
        .setting(key: "DisplayFillColor"), // :3190
        .setting(key: "DisplayFftFillAlpha"), // :3195
        .setting(key: "DisplaySpectrumAveraging"), // :3207
        .setting(key: "DisplaySpectrumAverageTimeMs"), // :3215
        .setting(key: "DisplayTraceColor"), // :3225
        .setting(key: "DisplayLineWidth"), // :3232
        .setting(key: "DisplayPeakHoldEnabled"), // :3248
        .setting(key: "DisplayPeakHoldResetMs"), // :3256
        .phone(.spectrumHeight),
        .setting(key: "DisplayCalOffset"), // :3265
        .group("Grid"), // :3272
        .setting(key: "DisplayGridEnabled"), // :3273
        .setting(key: "DisplayFreqGridSpacing"), // :3284
        .setting(key: "DisplayFreqScaleFontPx"), // :3300
        .group("Markers"), // :3306
        .setting(key: "DisplayShowNoiseFloor"), // :3309
        .setting(key: "DisplayShowBinWidth"), // :3314
        .setting(key: "DisplayShowPeakValueOverlay"), // :3320
        .setting(key: "DisplayShowCursorFreq"), // :3325
        .link(title: peaksLinkTitle, pageId: peaksPageId), // :3333
    ]

    // From src/gui/SpectrumOverlayPanel.cpp:3490 (buildPillLevelsPage)
    private static let levels: [DisplayPillRow] = [
        .group("Spectrum scale"), // :3492
        .setting(key: "SpectrumScaleMode"), // :3494
        .setting(key: "DisplayGridMax"), // :3506
        .setting(key: "DisplayGridMin"), // :3517
        .setting(key: "DisplayNFOffsetGridFollow"), // :3531
        .setting(key: "DisplayMaintainNFAdjustDelta"), // :3537
        .group("Waterfall levels"), // :3545
        .setting(key: "WaterfallLevelsMode"), // :3556
        .setting(key: "ClarityRetune"), // :3598 (Re-tune)
        .setting(key: "DisplayWfBlackLevel"), // :3601
        .setting(key: "DisplayWfColorGain"), // :3616
        .setting(key: "WaterfallAGCOffsetDb"), // :3629
        .setting(key: "DisplayWfLowLevel"), // :3638
        .setting(key: "DisplayWfHighLevel"), // :3644
    ]

    // From src/gui/SpectrumOverlayPanel.cpp:4018 (buildPillWaterfallPage)
    private static let waterfall: [DisplayPillRow] = [
        .group("Drawing"), // :4020
        .setting(key: "DisplayWfColorScheme"), // :4031
        .setting(key: "DisplayWfUpdatePeriodMs"), // :4043
        .setting(key: "DisplayWfOpacity"), // :4050
        .setting(key: "WaterfallBlankingEnabled"), // :4067
        .setting(key: "WaterfallBlankingThresholdDb"), // :4072
        .setting(key: "DisplayWaterfallAveraging"), // :4087
        .setting(key: "DisplayWaterfallAverageTimeMs"), // :4097
        .group("Overlays"), // :4102
        .setting(key: "WaterfallStopOnTx"), // :4105
        .setting(key: "DisplayShowRxFilterOnWaterfall"), // :4109
        .setting(key: "DisplayShowTxFilterOnRxWaterfall"), // :4114
        .setting(key: "DisplayShowRxZeroLine"), // :4122
        .setting(key: "DisplayShowTxZeroLine"), // :4126
        .group("Time"), // :4132
        .setting(key: "DisplayWfTimestampPos"), // :4133
        .setting(key: "DisplayWaterfallHistoryMs"), // :4150
        .phone(.lookBack),
    ]

    // From src/gui/SpectrumOverlayPanel.cpp:4233 (buildPillView3DPage)
    private static let view: [DisplayPillRow] = [
        .group("View"), // :4235
        .setting(key: "DisplaySpectrumRenderMode"), // :4240
        .group("3D"), // the board's 3D caption
        .setting(key: "Display3DFloor"), // :4253
        .setting(key: "Display3DGain"), // :4260
        .setting(key: "Display3DSpan"), // :4268
        .setting(key: "Display3DAngle"), // :4277
        .setting(key: "Display3DSpeed"), // :4286
        .setting(key: "Display3DSliceShadow"), // :4298
        .group("Background"), // :4310
        .setting(key: "DisplayBackgroundImage"), // :4368
        .setting(key: "DisplayBackgroundEnabled"), // :4371
        .setting(key: "DisplayBackgroundOpacity"), // :4378
        .setting(key: "DisplayBackgroundColor"), // :4388
        .group("On this phone"),
        .phone(.ctun),
        .phone(.dup),
    ]

    // From src/gui/SpectrumOverlayPanel.cpp:3659 (buildPillFftPage)
    private static let fft: [DisplayPillRow] = [
        .group("FFT"), // :3661
        .setting(key: "DisplayFftSizingMode"), // :3729
        .setting(key: "DisplayFftSize"), // :3745
        .setting(key: "DisplayHzPerBinTarget"), // :3823
        .fftReadout, // :3854 (pillFftReadout)
        .setting(key: "DisplaySpectrumFps"), // :3859
        .setting(key: "DisplayFftWindow"), // :3880
        .group("On this phone"),
        .phone(.decimation), // :3901 (DisplayDecimation)
    ]
}
