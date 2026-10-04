// NereusSDR for iOS: the Display sheet: this pan's palette and levels, fill and scale, and the Core's extras on the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// The sheet the toolbar's Display drops over the band, as picture 25
/// draws it (spec section 5.1 items 15 and 19, D73): this pan's waterfall
/// and spectrum on this phone. The band plan's picker and size live in
/// Setup's Display page (D79), which More display options in Setup opens.
/// Beside Line and Spectrum height sit the Core's FFT size and Hz/bin
/// target with the live bin width (JJ asked for the bin control close to
/// hand); they change the Core's own settings, which every device follows.
struct DisplaySheet: View {
    @ObservedObject var model: DisplaySheetModel
    @ObservedObject var core: CoreDisplayModel
    /// Switches to the Setup tab.
    let openSetup: () -> Void
    @Environment(\.displayScale) private var displayScale

    var body: some View {
        DropSheet(title: "Display", accessibilityIdentifier: "displaySheet") {
            Text("Pan \(MainScreenModel.panNumber) \u{00B7} this phone")
                .font(.system(size: 11, weight: .semibold))
                .tracking(0.44)
                .foregroundStyle(ChromeColours.textDim)
        } content: {
            // The View row, and in 3D its five settings and Reset (the 3D View board).
            DisplaySheetStackRows(model: model)
            Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1).padding(.vertical, 2)
            SheetCaption("Waterfall:")
            palette
            SheetGrid(columns: 3) {
                ForEach(DisplaySheetModel.Level.allCases, id: \.label) { level in
                    PanelButton(label: level.label, lit: model.level == level, style: .blue,
                                disabled: !model.isAvailable(level)) {
                        model.select(level)
                    }
                    .accessibilityHint(model.isAvailable(level) ? "" : CatalogFeed.needsNewerCoreText)
                }
            }
            levelNote
            if !model.extrasAvailable {
                SheetNote("Clarity and Auto: \(CatalogFeed.needsNewerCoreText)")
            }
            PanelSliderRow(label: "Color gain", value: model.colorGain, range: DisplaySheetModel.colorGainRange,
                           accessibility: "Waterfall color gain") { model.setColorGain($0) }
                .accessibilityIdentifier("waterfallColorGain")
            PanelSliderRow(label: "Black level", value: model.blackLevel, range: DisplaySheetModel.blackLevelRange,
                           accessibility: "Waterfall black level") { model.setBlackLevel($0) }
                .accessibilityIdentifier("waterfallBlackLevel")
            SheetCaption("Spectrum:")
            if !model.flatControlsAvailable {
                DisplaySheetStackRows.reason(DisplaySheetModel.flatOnlyText)
                    .accessibilityIdentifier("flatOnlyNote")
            }
            HStack(spacing: 8) {
                PanelButton(label: "Fill", lit: model.fillOn, style: .dsp, disabled: !model.flatControlsAvailable) {
                    model.toggleFill()
                }
                    .frame(width: 62)
                    .accessibilityHint(model.flatControlsAvailable ? "" : DisplaySheetModel.flatOnlyText)
                    .accessibilityIdentifier("fillSwitch")
                PanelSliderRow(label: nil, value: model.fill,
                               range: DisplaySheetModel.fillRange, accessibility: "Fill strength",
                               greyed: !(model.fillOn && model.flatControlsAvailable)) { model.setFill($0) }
            }
            PanelSliderRow(label: "Line", value: model.line, range: DisplaySheetModel.lineRange(scale: displayScale),
                           accessibility: "Trace line width", format: DisplaySheetModel.lineText) {
                model.setLine($0, scale: displayScale)
            }
            PanelSliderRow(label: "Spectrum height", value: model.spectrumHeight,
                           range: DisplaySheetModel.spectrumHeightRange, accessibility: "Spectrum height",
                           format: DisplaySheetModel.percentText) { model.setSpectrumHeight($0) }
                .accessibilityIdentifier("spectrumHeight")
            fft
            PanelSliderRow(label: "Top", value: model.top,
                           range: model.topLimits, accessibility: "Reference level",
                           greyed: !model.flatControlsAvailable) { model.setTop($0) }
            PanelSliderRow(label: "Range", value: model.range, range: model.rangeRange,
                           accessibility: "Dynamic range") { model.setRange($0) }
            SheetCaption("While transmitting:")
            HStack(spacing: 8) {
                // DUP, the desktop's display duplex: greyed with its reason
                // on a Core that cannot keep the receiver on the band.
                PanelButton(label: "DUP", lit: model.duplex && model.duplexAvailable, style: .blue,
                            disabled: !model.duplexAvailable) { model.setDuplex(!model.duplex) }
                    .frame(width: 84)
                    .accessibilityLabel("Display duplex")
                    .accessibilityHint(model.duplexAvailable ? "" : DisplaySheetModel.duplexUnavailableText)
                    .accessibilityIdentifier("duplexSwitch")
                SheetNote(model.duplexNote)
                    .accessibilityIdentifier("duplexNote")
            }
            SheetCaption("On the band:")
            SheetGrid(columns: 2) {
                ForEach(DisplaySheetModel.Feature.allCases, id: \.label) { feature in
                    PanelButton(label: feature.label, lit: model.isOn(feature), style: .dsp,
                                disabled: !model.extrasAvailable) {
                        model.toggle(feature)
                    }
                    .accessibilityHint(model.extrasAvailable ? "" : CatalogFeed.needsNewerCoreText)
                }
                PanelButton(label: "Peak hold", lit: model.classicPeakHold, style: .dsp) {
                    model.toggleClassicPeakHold()
                }
                .accessibilityIdentifier("classicPeakHold")
            }
            if !model.extrasAvailable {
                SheetNote("Active peak hold, Peaks and Noise floor: \(CatalogFeed.needsNewerCoreText)")
            }
            SheetCaption("Tuning:")
            HStack(spacing: 8) {
                PanelButton(label: "CTUN", lit: model.ctun, style: .blue) { model.setCtun(!model.ctun) }
                    .frame(width: 84)
                    .accessibilityIdentifier("ctunSwitch")
                SheetNote(model.ctun ? DisplaySheetModel.ctunOnNote : DisplaySheetModel.ctunOffNote)
            }
            SheetCaption("Look back:")
            HStack(spacing: 8) {
                PanelSliderRow(label: nil, value: model.lookBackSeconds,
                               range: model.lookBackRange.max > 0 ? model.lookBackRange : nil,
                               accessibility: "Look back", format: DisplaySheetModel.lookBackText) {
                    model.setLookBack(seconds: $0)
                }
                .accessibilityIdentifier("lookBack")
                PanelButton(label: "LIVE", lit: model.lookBackSeconds == 0, style: .red,
                            disabled: model.lookBackSeconds == 0) { model.goLive() }
                    .frame(width: 64)
                    .accessibilityIdentifier("lookBackLive")
            }
            if let note = model.note {
                SheetNote(note)
                    .accessibilityIdentifier("displaySheetNote")
            }
            more
        }
    }

    /// The Core's FFT size and Hz/bin target, and the bin width now.
    @ViewBuilder
    private var fft: some View {
        let size = core.sizeControl
        PanelSliderRow(label: "FFT size", value: core.sizeIndex, range: core.sizeIndexRange,
                       accessibility: "FFT size", format: { core.sizeOption(at: $0)?.label ?? "" }, notConfirmed: core.isUnconfirmed(CoreDisplayModel.fftSizeKey)) {
            core.setSizeIndex($0)
        }
        .accessibilityIdentifier("sheetFftSize")
        let target = core.hzPerBinControl
        PanelSliderRow(label: "Hz/bin", value: target.flatMap { core.value($0) }, range: target?.range,
                       accessibility: target?.label ?? "Hz per bin target",
                       format: { value in target?.text(value) ?? "" }, notConfirmed: core.isUnconfirmed(CoreDisplayModel.hzPerBinKey)) { value in
            if let target {
                core.set(target, to: value)
            }
        }
        .accessibilityIdentifier("sheetHzPerBin")
        SheetNote("\(core.binWidthLabel): \(core.binWidthText ?? "")")
            .accessibilityIdentifier("sheetBinWidth")
        if size == nil || target == nil {
            SheetNote("FFT size and Hz/bin: \(CoreDisplayModel.olderCoreText)")
                .accessibilityIdentifier("sheetFftOlder")
        } else if let note = core.note {
            SheetNote(note)
        }
    }

    private var palette: some View {
        HStack(spacing: 10) {
            Text("Palette")
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(ChromeColours.text)
            Spacer(minLength: 0)
            Menu {
                ForEach(model.palettes, id: \.id) { palette in
                    Button(palette.name) {
                        model.selectPalette(palette.id)
                    }
                }
                Button(BandPalette.customPaletteName) {
                    model.selectPalette(BandPalette.customPaletteId)
                }
            } label: {
                Text("\(model.paletteName ?? CatalogFeed.needsNewerCoreText) \u{25BE}")
                    .font(.system(size: 12, weight: .bold))
                    .lineLimit(1)
                    .foregroundStyle(ChromeColours.text)
                    .padding(.horizontal, 12)
                    .frame(minWidth: 88, minHeight: 36)
                    .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
            }
            .accessibilityLabel("Palette")
            .accessibilityValue(model.paletteName ?? CatalogFeed.needsNewerCoreText)
            .accessibilityIdentifier("paletteMenu")
        }
        .padding(.vertical, 6)
    }

    @ViewBuilder
    private var levelNote: some View {
        if let note = model.levelNote {
            if model.level == .clarity {
                HStack(alignment: .firstTextBaseline, spacing: 4) {
                    SheetNote(note)
                    Button {
                        model.retune()
                    } label: {
                        Text("Re-tune")
                            .font(.system(size: 11, weight: .bold))
                            .foregroundStyle(model.retuneAvailable ? ChromeColours.accent : ChromeColours.buttonOffText)
                    }
                    .buttonStyle(.plain)
                    .disabled(!model.retuneAvailable)
                    .accessibilityHint(model.retuneAvailable ? "" : CatalogFeed.needsNewerCoreText)
                    .accessibilityIdentifier("retune")
                }
                if !model.retuneAvailable {
                    SheetNote("Re-tune: \(CatalogFeed.needsNewerCoreText)")
                }
            } else {
                SheetNote(note)
            }
        }
    }

    private var more: some View {
        VStack(spacing: 0) {
            Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1)
            Button(action: openSetup) {
                Text("More display options in Setup \u{203A}")
                    .font(.system(size: 12, weight: .bold))
                    .foregroundStyle(ChromeColours.accent)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.top, 8)
                    .padding(.bottom, 2)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier("moreDisplayOptions")
        }
        .padding(.top, 8)
    }
}
