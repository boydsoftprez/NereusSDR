// NereusSDR for iOS: individually replaceable keyed gauges using native radio, accessory and stage scales
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

struct KeyedMeterGauge: View {
    let sample: KeyedMeterSample
    let slot: Int
    let choose: () -> Void
    @Environment(\.dynamicTypeSize) private var textSize

    var body: some View {
        Button(action: choose) {
            VStack(spacing: 2) {
                HStack(spacing: 3) {
                    Text(sample.name)
                        .lineLimit(1)
                        .minimumScaleFactor(0.75)
                    Spacer(minLength: 0)
                    Image(systemName: "chevron.down")
                        .foregroundStyle(ChromeColours.gaugeNormal)
                }
                .font(.system(size: textSize.isAccessibilitySize ? 14 : 11, weight: .semibold))
                .foregroundStyle(ChromeColours.text)
                Canvas { context, size in draw(in: &context, size: size) }
                    .frame(height: LinearGauge.height)
            }
            .frame(maxWidth: .infinity, minHeight: 44)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Change \(sample.name)")
        .accessibilityValue(sample.readout)
        .accessibilityHint(sample.reason ?? "Choose a meter for this slot")
        .accessibilityIdentifier("keyedMeterSlot-\(slot)")
    }

    private func draw(in context: inout GraphicsContext, size: CGSize) {
        let bar = CGRect(x: 1, y: 1, width: max(0, size.width - 2), height: 14)
        context.fill(Path(roundedRect: bar, cornerRadius: 2), with: .color(ChromeColours.gaugeGround))
        context.stroke(Path(roundedRect: bar.insetBy(dx: 0.5, dy: 0.5), cornerRadius: 2),
                       with: .color(ChromeColours.gaugeBorder), lineWidth: 1)
        let inner = bar.insetBy(dx: 1, dy: 1)
        let yellow = sample.stage?.midAt ?? sample.linearPosition(sample.scale.yellowFrom)
        let red = sample.stage?.midAt ?? sample.linearPosition(sample.scale.redFrom)
        if let stage = sample.stage {
            context.fill(Path(CGRect(x: inner.minX + inner.width * stage.midAt, y: inner.minY,
                                     width: inner.width * (1 - stage.midAt), height: inner.height)),
                         with: .color(Color(red: 0.12, green: 0.055, blue: 0.07)))
        }
        if let fill = sample.position {
            zone(&context, inner, from: 0, to: min(fill, yellow), colour: ChromeColours.gaugeNormal)
            zone(&context, inner, from: yellow, to: min(fill, red), colour: ChromeColours.gaugeWarn)
            zone(&context, inner, from: red, to: fill, colour: ChromeColours.gaugeRed)
        }
        if let peak = sample.peakPosition, sample.value != nil {
            let x = inner.minX + inner.width * peak
            context.fill(Path(CGRect(x: x, y: inner.minY, width: 1, height: inner.height)), with: .color(.white))
        }
        var shadowed = context
        shadowed.addFilter(.shadow(color: .black, radius: 1))
        shadowed.draw(Text(sample.readout).font(.system(size: textSize.isAccessibilitySize ? 12 : 10, weight: .bold))
            .foregroundStyle(ChromeColours.textBright), at: CGPoint(x: bar.midX, y: bar.midY))
        let ticks: [(Double, String)] = sample.stage.map {
            [($0.low, TxStage.mark($0.low)), ($0.mid, TxStage.mark($0.mid)), ($0.high, TxStage.mark($0.high))]
        } ?? sample.scale.ticks.map { ($0.value, $0.label) }
        for (value, label) in ticks {
            let position = sample.stage?.position(value) ?? sample.linearPosition(value)
            let anchor: UnitPoint = position <= 0 ? .topLeading : position >= 0.98 ? .topTrailing : .top
            context.draw(Text(label).font(.system(size: 7, weight: .semibold)).foregroundStyle(ChromeColours.caption),
                         at: CGPoint(x: inner.minX + inner.width * position, y: bar.maxY + 1), anchor: anchor)
        }
    }

    private func zone(_ context: inout GraphicsContext, _ rect: CGRect, from: Double, to: Double, colour: Color) {
        guard to > from else { return }
        context.fill(Path(CGRect(x: rect.minX + rect.width * from, y: rect.minY,
                                 width: rect.width * (to - from), height: rect.height)), with: .color(colour))
    }
}
