// NereusSDR for iOS: the step in the middle of a knob or at the thumbwheel's end
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The active slice's step, over the small word STEP, in a round button in
/// the knob's middle or a rounded one at the thumbwheel's left end, as the
/// board draws them (`.knob__step`, `.twheel__step`). A tap opens the step
/// menu. Without a step it reads Step, greyed, as the flag's does.
struct DialStepButton<Outline: InsettableShape>: View {
    let label: String?
    let outline: Outline
    let size: CGSize
    var labelPoints: CGFloat = 12
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(spacing: 1) {
                Text(label ?? "Step")
                    .font(.system(size: labelPoints, weight: .bold))
                    .foregroundStyle(label == nil ? DialColours.stepCaption : DialColours.stepText)
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
                Text("STEP")
                    .font(.system(size: 8, weight: .bold))
                    .tracking(0.96)
                    .foregroundStyle(DialColours.stepCaption)
            }
            .frame(width: size.width, height: size.height)
            .background(DialColours.stepBackground, in: outline)
            .overlay(outline.strokeBorder(DialColours.stepBorder, lineWidth: 1))
            .contentShape(outline)
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Tuning step")
        .accessibilityValue(label ?? "")
        .accessibilityHint("Tap to change")
        .accessibilityIdentifier("dialStep")
    }
}
