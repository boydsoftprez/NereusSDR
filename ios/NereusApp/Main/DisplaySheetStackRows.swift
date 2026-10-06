// NereusSDR for iOS: the Display sheet's View row and, in 3D, the five 3D settings and Reset 3D to defaults
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The top of the Display sheet (JJ's 3D View board, 2026-09-29): the View
/// row, 2D Waterfall or 3D Stacked Trace, always there; with 3D chosen, the
/// five 3D settings and Reset 3D to defaults (recommendation 1). On a Core
/// without the 3D view the View row is greyed with the reason
/// (recommendation 7). 3D Floor names the band it is kept for
/// (recommendation 6); Span says when the band saves data (recommendation 5).
struct DisplaySheetStackRows: View {
    @ObservedObject var model: DisplaySheetModel
    @State private var askingReset = false

    /// The board's colours: a greyed row's reason in amber, Reset's words in
    /// the accent blue.
    static let reasonColour = Color(red: 0xD8 / 255.0, green: 0xA8 / 255.0, blue: 0x60 / 255.0)

    var body: some View {
        SheetCaption("View:")
        SheetGrid(columns: 2) {
            PanelButton(label: DisplaySheetModel.flatViewLabel, lit: !model.stackChosen || !model.stackOffered,
                        style: .blue, disabled: !model.stackOffered) {
                model.selectView(.flat)
            }
            .accessibilityHint(model.viewNote ?? "")
            .accessibilityIdentifier("view2D")
            PanelButton(label: DisplaySheetModel.stackViewLabel, lit: model.stackChosen && model.stackOffered,
                        style: .blue, disabled: !model.stackOffered) {
                model.selectView(.stacked)
            }
            .accessibilityHint(model.viewNote ?? "")
            .accessibilityIdentifier("view3D")
        }
        if let note = model.viewNote {
            Self.reason(note)
                .accessibilityIdentifier("viewNote")
        }
        if model.stackChosen && model.stackOffered {
            Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1).padding(.vertical, 2)
            SheetCaption("3D:")
            PanelSliderRow(label: model.floorBand.map { "Floor\n\($0)" } ?? "Floor", value: model.stackFloor,
                           range: DisplaySheetModel.floorRange,
                           accessibility: model.floorBand.map { "3D Floor, \($0)" } ?? "3D Floor",
                           format: { "\(Int($0.rounded())) dB" }) { model.setStackFloor($0) }
                .accessibilityIdentifier("stackFloor")
            PanelSliderRow(label: "Gain", value: model.stackGain, range: DisplaySheetModel.percentRange,
                           accessibility: "3D Gain", format: DisplaySheetModel.percentText) { model.setStackGain($0) }
                .accessibilityIdentifier("stackGain")
            PanelSliderRow(label: "Span", value: model.stackSpan, range: DisplaySheetModel.percentRange,
                           accessibility: "3D Span", format: DisplaySheetModel.percentText) { model.setStackSpan($0) }
                .accessibilityIdentifier("stackSpan")
            if let note = model.spanNote {
                SheetNote(note)
                    .accessibilityIdentifier("stackSpanNote")
            }
            PanelSliderRow(label: "Angle", value: model.stackAngle, range: DisplaySheetModel.percentRange,
                           accessibility: "3D Angle", format: DisplaySheetModel.percentText) { model.setStackAngle($0) }
                .accessibilityIdentifier("stackAngle")
            sliceShadow
            reset
        }
    }

    private var sliceShadow: some View {
        HStack(spacing: 10) {
            VStack(alignment: .leading, spacing: 2) {
                Text("Slice shadow")
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(ChromeColours.text)
                Text(DisplaySheetModel.sliceShadowNote)
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 8)
            PanelButton(label: model.sliceShadow ? "On" : "Off", lit: model.sliceShadow, style: .blue) {
                model.setSliceShadow(!model.sliceShadow)
            }
            .frame(width: 72)
            .accessibilityLabel("3D Slice shadow")
            .accessibilityValue(model.sliceShadow ? "On" : "Off")
            .accessibilityIdentifier("stackSliceShadow")
        }
        .padding(.vertical, 4)
    }

    private var reset: some View {
        Button {
            askingReset = true
        } label: {
            Text(DisplaySheetModel.resetStackTitle)
                .font(.system(size: 12, weight: .bold))
                .foregroundStyle(ChromeColours.accent)
                .frame(maxWidth: .infinity, minHeight: 36)
                .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .padding(.top, 2)
        .accessibilityIdentifier("stackReset")
        .alert(DisplaySheetModel.resetStackTitle, isPresented: $askingReset) {
            // No is the default: nothing changes.
            Button("No", role: .cancel) {}
            Button("Yes") { model.resetStack() }
        } message: {
            Text(DisplaySheetModel.resetStackQuestion)
        }
    }

    static func reason(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(reasonColour)
            .lineSpacing(2)
            .fixedSize(horizontal: false, vertical: true)
            .padding(.horizontal, 2)
    }
}
