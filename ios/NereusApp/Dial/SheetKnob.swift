// NereusSDR for iOS: the knob in a sheet, raised by tapping the frequency
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import NereusModels
import SwiftUI

/// The knob in a sheet (D12, spec section 5.1 item 8, picture 3): a tap on
/// a flag's frequency raises it from the foot of the band, the active
/// slice's letter and frequency across its top with Done, the radio's
/// steps in a row (the active slice's lit), and a big knob. More room to
/// turn, but it covers the waterfall while it is up. A tap on the knob's
/// middle opens the step menu over it; a tap on the sheet's frequency
/// opens the number pad in its place.
struct SheetKnob: View {
    @ObservedObject var tuning: BandTuningModel
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var settings: PhoneSettings
    /// The knob's size, the board's big knob unless the band is too short for it.
    var metrics = Knob.Metrics.sheet
    /// The knob's motion, when a picture drives it by hand.
    var spinner: DialSpinner?

    /// Everything in the sheet but the knob: its padding, the grab handle,
    /// the head and the steps' row, with the gaps between.
    static let chromeHeight: CGFloat = 8 + 5 + 10 + 34 + 10 + 30 + 10 + 14

    var body: some View {
        VStack(spacing: 10) {
            Capsule()
                .fill(DialColours.grab)
                .frame(width: 40, height: 5)
            head
            steps
            Knob(tuning: tuning, slices: slices, settings: settings, metrics: metrics, spinner: spinner)
                .overlay {
                    if tuning.stepMenuFromDial, let id = tuning.stepMenuSliceId {
                        TuneStepMenu(tuning: tuning, sliceId: id)
                            .fixedSize()
                    }
                }
        }
        .padding(.top, 8)
        .padding(.horizontal, 14)
        .padding(.bottom, 14)
        .frame(maxWidth: .infinity)
        .background(DialColours.sheet, in: UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14))
        .overlay(alignment: .top) {
            UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14)
                .strokeBorder(DialColours.sheetBorder, lineWidth: 1)
                .mask(Rectangle().frame(height: 14).frame(maxHeight: .infinity, alignment: .top))
        }
        .shadow(color: .black.opacity(0.5), radius: 15, y: -10)
        .contentShape(Rectangle())
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Tuning dial")
        .accessibilityIdentifier("dialSheet")
    }

    private var head: some View {
        let active = slices.active
        return HStack(spacing: 8) {
            if let active {
                SliceLetterBadge(letter: active.slice.letter, colour: BandColours.slice(active.slice.colour))
                Button {
                    tuning.openPad(sliceId: active.id)
                } label: {
                    Text(BandSlice.frequencyText(hz: active.slice.frequencyHz))
                        .font(.system(size: 30, weight: .bold, design: .monospaced))
                        .foregroundStyle(DialColours.frequency)
                        .lineLimit(1)
                        .minimumScaleFactor(0.6)
                }
                .buttonStyle(.plain)
                .accessibilityHint("Tap to type a frequency")
            }
            Spacer(minLength: 0)
            Button {
                tuning.closeDialSheet()
            } label: {
                Text("Done")
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(DialColours.stepText)
                    .padding(.horizontal, 14)
                    .frame(height: 34)
                    .background(DialColours.button, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(DialColours.buttonBorder, lineWidth: 1))
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier("dialSheetDone")
        }
        .frame(height: 34)
    }

    @ViewBuilder
    private var steps: some View {
        if tuning.stepsAvailable {
            ScrollViewReader { reader in
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 6) {
                        ForEach(tuning.steps, id: \.hz) { step in
                            stepButton(step)
                                .id(step.hz)
                        }
                    }
                }
                .onAppear { scroll(reader) }
                .onChange(of: slices.active?.stepHz) { _, _ in scroll(reader) }
            }
            .frame(height: 30)
        } else {
            Text(CatalogFeed.needsNewerCoreText)
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.buttonOffText)
                .frame(maxWidth: .infinity, minHeight: 30, alignment: .leading)
        }
    }

    private func stepButton(_ step: StationCatalog.TuneStep) -> some View {
        let lit = tuning.isActiveStep(step)
        return Button {
            tuning.pickForActive(step)
        } label: {
            Text(step.label)
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(lit ? Color.white : DialColours.stepText)
                .lineLimit(1)
                .padding(.horizontal, 10)
                .frame(height: 30)
                .background(lit ? DialColours.buttonOn : DialColours.button, in: RoundedRectangle(cornerRadius: 3))
                .overlay(RoundedRectangle(cornerRadius: 3)
                    .strokeBorder(lit ? DialColours.buttonOnBorder : DialColours.buttonBorder, lineWidth: 1))
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(lit ? .isSelected : [])
    }

    /// The row starts with the lit step in view, a little in from the left, as the board scrolls it.
    private func scroll(_ reader: ScrollViewProxy) {
        guard let hz = slices.active?.stepHz else {
            return
        }
        reader.scrollTo(hz, anchor: UnitPoint(x: 0.2, y: 0.5))
    }
}
