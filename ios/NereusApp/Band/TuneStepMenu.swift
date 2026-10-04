// NereusSDR for iOS: the flag's step menu: the radio's tuning steps, the slice's own lit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import NereusModels
import SwiftUI

/// The list a tap on a flag's step opens, as picture 26 draws it (D74,
/// spec section 5.1 item 17): "Step for slice A", then the radio's steps
/// from the Core's catalogue, the slice's own lit. Picking one writes the
/// slice's step, which drags, snapped taps and the dial then use. A Core
/// that lists no steps leaves the menu with "Needs a newer Core".
struct TuneStepMenu: View {
    @ObservedObject var tuning: BandTuningModel
    let sliceId: Int

    static let width: CGFloat = 132

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text(tuning.stepMenuTitle(sliceId: sliceId))
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(ChromeColours.textDim)
                .padding(.horizontal, 6)
                .padding(.top, 2)
                .padding(.bottom, 6)
            if tuning.stepsAvailable {
                ForEach(tuning.steps, id: \.hz) { step in
                    item(step)
                }
            } else {
                Text(CatalogFeed.needsNewerCoreText)
                    .font(.system(size: 13))
                    .foregroundStyle(ChromeColours.buttonOffText)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 7)
            }
        }
        .padding(6)
        .frame(minWidth: Self.width, alignment: .leading)
        .fixedSize()
        .background(ChromeColours.sheet, in: RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(ChromeColours.sheetBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.55), radius: 14, y: 10)
        .accessibilityElement(children: .contain)
        .accessibilityLabel(tuning.stepMenuTitle(sliceId: sliceId))
        .accessibilityIdentifier("tuneStepMenu")
    }

    private func item(_ step: StationCatalog.TuneStep) -> some View {
        let lit = tuning.isCurrent(step)
        return Button {
            tuning.pick(step)
        } label: {
            Text(step.label)
                .font(.system(size: 13))
                .foregroundStyle(lit ? Color.white : ChromeColours.text)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 8)
                .padding(.vertical, 7)
                .background(lit ? Self.lit : Color.clear, in: RoundedRectangle(cornerRadius: 4))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(lit ? .isSelected : [])
    }

    /// The lit step, the board's `.tustep__item.is-on`.
    private static let lit = Color(red: 0x1A / 255.0, green: 0x5C / 255.0, blue: 0xA8 / 255.0)
}
