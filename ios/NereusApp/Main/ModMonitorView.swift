// NereusSDR for iOS: the AM Mod Monitor's face on the TX panel: peaks and holds, readouts, flashers, the carrier lamp and the envelope
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The AM Mod Monitor as the board draws it (D102): a title bar with the
/// transmitting slice's mode and letter, the source, the peaks as bars (or
/// the lit analog pair), the readouts, the flashers, the carrier lamp, the
/// envelope scope, and RESET and Settings. Settings opens the monitor's
/// sheet, the only way to its settings. Every control is a labelled
/// button; greyed controls say why above them.
struct ModMonitorView: View {
    @ObservedObject var model: ModMonitorModel
    @State private var settingsOpen = false
    @Environment(\.dynamicTypeSize) private var typeSize

    @ScaledMetric(relativeTo: .caption2) private var small: CGFloat = 11
    @ScaledMetric(relativeTo: .caption) private var text: CGFloat = 12
    @ScaledMetric(relativeTo: .body) private var value: CGFloat = 13
    @ScaledMetric(relativeTo: .caption) private var buttonHeight: CGFloat = 36
    @ScaledMetric(relativeTo: .caption) private var trackHeight: CGFloat = 14
    @ScaledMetric(relativeTo: .caption) private var scopeHeight: CGFloat = 112

    /// Large type stacks the readings one per row.
    private var large: Bool {
        typeSize >= .accessibility1
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            title
            VStack(alignment: .leading, spacing: 9) {
                if let reason = model.reason {
                    Text(reason)
                        .font(.system(size: text))
                        .foregroundStyle(ChromeColours.text)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(.horizontal, 9)
                        .padding(.vertical, 8)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(Self.reasonGround, in: RoundedRectangle(cornerRadius: 6))
                        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                        .accessibilityIdentifier("modMonitorReason")
                }
                controls
                    .opacity(model.enabled ? 1 : 0.45)
            }
            .padding(.horizontal, 10)
            .padding(.top, 10)
            .padding(.bottom, 14)
        }
        .overlay(alignment: .top) {
            Rectangle().fill(ChromeColours.panelEdge).frame(height: 1)
        }
        .onAppear { model.appear() }
        .onDisappear { model.disappear() }
        .sheet(isPresented: $settingsOpen) {
            ModMonitorSettingsSheet(model: model)
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("AM Mod Monitor")
        .accessibilityIdentifier("modMonitor")
    }

    private var controls: some View {
        VStack(alignment: .leading, spacing: 9) {
            sourceRow
            if model.enabled, !model.pureSignalRunning {
                why(ModMonitorModel.pureSignalOffText)
                    .accessibilityIdentifier("modMonitorPureSignalReason")
            }
            switch model.meterStyle {
            case .bars:
                ModMonitorPeakBar(label: "Positive peaks", value: model.posBar, hold: model.posHold,
                        scale: ModMonitorModel.posScale, height: trackHeight, font: small)
                    .accessibilityIdentifier("modMonitorPositiveBar")
                ModMonitorPeakBar(label: "Negative peaks", value: model.negBar, hold: model.negHold,
                        scale: ModMonitorModel.negScale, height: trackHeight, font: small)
                    .accessibilityIdentifier("modMonitorNegativeBar")
            case .meters:
                HStack(spacing: 4) {
                    ModMeterDial(caption: "NEGATIVE PEAKS", value: model.negBar, hold: model.negHold, lit: model.live)
                        .accessibilityLabel("Negative peaks")
                        .accessibilityValue(model.live ? ModMonitorModel.percentText(model.negBar) : "No reading")
                    ModMonitorAsymmetryBar(value: model.asymmetry, lit: model.live)
                        .frame(width: 24)
                        .accessibilityLabel("Asymmetry")
                        .accessibilityValue(model.asymmetryText)
                    ModMeterDial(caption: "POSITIVE PEAKS", value: model.posBar, hold: model.posHold, lit: model.live)
                        .accessibilityLabel("Positive peaks")
                        .accessibilityValue(model.live ? ModMonitorModel.percentText(model.posBar) : "No reading")
                }
                .accessibilityElement(children: .contain)
                .accessibilityIdentifier("modMonitorMeters")
            }
            readouts
            flashers
            HStack(spacing: 8) {
                caption("Carrier")
                ModMonitorLamp(text: model.lamp.text, colours: Self.lampColours(model.lamp), wide: true, font: small,
                     height: buttonHeight * 0.78)
                    .accessibilityLabel("Carrier lamp")
                    .accessibilityValue(model.lamp.text)
                    .accessibilityIdentifier("modMonitorLamp")
            }
            VStack(alignment: .leading, spacing: 3) {
                Text("Envelope, percent modulation")
                    .font(.system(size: small))
                    .foregroundStyle(ChromeColours.textDim)
                ModMonitorScope(points: model.scope, posFlash: Double(model.posFlashPct),
                              negFlash: Double(model.negFlashPct), vintage: model.meterStyle == .meters)
                    .frame(height: scopeHeight)
                    .accessibilityElement()
                    .accessibilityLabel("Envelope scope")
                    .accessibilityValue(model.scope.isEmpty ? "Empty" : "\(model.scope.count) points")
                    .accessibilityIdentifier("modMonitorScope")
            }
            if model.enabled, !model.live {
                why(ModMonitorModel.waitingText)
                    .accessibilityIdentifier("modMonitorWaiting")
            }
            HStack(spacing: 6) {
                ModMonitorButton(label: "RESET", height: buttonHeight + 4, font: text, disabled: !model.enabled) {
                    model.reset()
                }
                .accessibilityHint("Clears the peaks and flashers for every device watching")
                .accessibilityIdentifier("modMonitorReset")
                ModMonitorButton(label: "Settings", height: buttonHeight + 4, font: text, disabled: !model.enabled) {
                    settingsOpen = true
                }
                .accessibilityLabel("AM Mod Monitor settings")
                .accessibilityIdentifier("modMonitorSettings")
            }
            if let note = model.note {
                Text(note)
                    .font(.system(size: small))
                    .foregroundStyle(note == ModMonitorModel.clearedText ? Self.doneText : ChromeColours.refusalText)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("modMonitorNote")
            }
            Text(ModMonitorModel.resetFootText)
                .font(.system(size: small * 0.95))
                .foregroundStyle(ChromeColours.caption)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var title: some View {
        // At a large text size the chip goes under the name, so neither is cut.
        let layout = large ? AnyLayout(VStackLayout(alignment: .leading, spacing: 2))
            : AnyLayout(HStackLayout(spacing: 6))
        return layout {
            Text("AM Mod Monitor")
                .font(.system(size: small, weight: .bold))
                .foregroundStyle(ChromeColours.icon)
                .lineLimit(2)
                .fixedSize(horizontal: false, vertical: true)
            if !large {
                Spacer(minLength: 4)
            }
            if let chip = model.chipText {
                Text(chip)
                    .font(.system(size: small * 0.9, weight: .bold))
                    .foregroundStyle(Self.chipText)
                    .lineLimit(1)
                    .padding(.horizontal, 6)
                    .background(Self.chipGround, in: RoundedRectangle(cornerRadius: 3))
                    .accessibilityIdentifier("modMonitorChip")
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, large ? 4 : 0)
        .frame(maxWidth: .infinity, minHeight: 22, alignment: .leading)
        .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                           .init(color: ChromeColours.titleMiddle, location: 0.5),
                                           .init(color: ChromeColours.titleBottom, location: 1)],
                                   startPoint: .top, endPoint: .bottom))
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
        }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isHeader)
    }

    private var sourceRow: some View {
        let layout = large ? AnyLayout(VStackLayout(alignment: .leading, spacing: 6))
            : AnyLayout(HStackLayout(spacing: 8))
        return layout {
            caption("Source")
            ModMonitorSourcePicker(model: model, height: buttonHeight, font: text)
        }
    }

    private var readouts: some View {
        let columns = Array(repeating: GridItem(.flexible(minimum: 0), spacing: 8), count: large ? 1 : 2)
        return LazyVGrid(columns: columns, spacing: 4) {
            readout("Positive peak", model.posText, identifier: "modMonitorPositive")
            readout("Negative peak", model.negText, identifier: "modMonitorNegative")
            readout("Asymmetry", model.asymmetryText, identifier: "modMonitorAsymmetry")
            readout("Carrier", model.carrierText, identifier: "modMonitorCarrier")
        }
    }

    private var flashers: some View {
        let columns = Array(repeating: GridItem(.flexible(minimum: 0), spacing: 8), count: large ? 1 : 2)
        return LazyVGrid(columns: columns, alignment: .leading, spacing: 6) {
            flasher("+PEAK", lit: model.posLit, threshold: model.posFlashPct, identifier: "modMonitorPositiveFlasher",
                    name: "Positive peak flasher")
            flasher("-PEAK", lit: model.negLit, threshold: model.negFlashPct, identifier: "modMonitorNegativeFlasher",
                    name: "Negative peak flasher")
        }
    }

    private func flasher(_ label: String, lit: Bool, threshold: Int, identifier: String, name: String) -> some View {
        HStack(spacing: 6) {
            ModMonitorLamp(text: label, colours: lit ? Self.redLamp : Self.offLamp, wide: false, font: small,
                 height: buttonHeight * 0.66)
            Text("at \(threshold)%")
                .font(.system(size: small))
                .foregroundStyle(ChromeColours.textDim)
                .lineLimit(1)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(name)
        .accessibilityValue((lit ? "Lit" : "Off") + ", at \(threshold) percent")
        .accessibilityIdentifier(identifier)
    }

    private func readout(_ label: String, _ reading: String, identifier: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 4) {
            Text(label)
                .font(.system(size: small * 0.95))
                .foregroundStyle(ChromeColours.textDim)
                .lineLimit(1)
                .minimumScaleFactor(0.7)
            Spacer(minLength: 2)
            Text(reading)
                .font(.system(size: value, weight: .semibold).monospacedDigit())
                .foregroundStyle(ChromeColours.textBright)
                .lineLimit(1)
                .fixedSize()
        }
        .padding(.horizontal, 7)
        .padding(.vertical, 5)
        .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
        .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(label)
        .accessibilityValue(reading)
        .accessibilityIdentifier(identifier)
    }

    private func caption(_ words: String) -> some View {
        Text(words)
            .font(.system(size: small))
            .foregroundStyle(ChromeColours.textDim)
            .frame(minWidth: large ? nil : 50, alignment: .leading)
    }

    private func why(_ words: String) -> some View {
        Text(words)
            .font(.system(size: small))
            .foregroundStyle(Self.whyText)
            .fixedSize(horizontal: false, vertical: true)
    }

    // MARK: Colours (the desktop's palette)

    static let reasonGround = Color(red: 0x14 / 255, green: 0x1C / 255, blue: 0x28 / 255)
    static let whyText = Color(red: 0x90 / 255, green: 0xA4 / 255, blue: 0xB6 / 255)
    static let chipGround = Color(red: 0x0F / 255, green: 0x24 / 255, blue: 0x36 / 255)
    static let chipText = Color(red: 0x9A / 255, green: 0xD0 / 255, blue: 0xEC / 255)
    static let doneText = Color(red: 0x80 / 255, green: 0xD0 / 255, blue: 0xA0 / 255)
    static let offLamp = ModMonitorLamp.Colours(ground: ChromeColours.inset, border: ChromeColours.insetBorder,
                                      text: Color(red: 0x40 / 255, green: 0x50 / 255, blue: 0x60 / 255))
    static let redLamp = ModMonitorLamp.Colours(ground: ChromeColours.txRed, border: ChromeColours.gaugeRed, text: .white)
    static let amberLamp = ModMonitorLamp.Colours(ground: ChromeColours.buttonOnAmber, border: ChromeColours.buttonOnAmberBorder,
                                        text: ChromeColours.buttonOnAmberText)
    static let greenLamp = ModMonitorLamp.Colours(ground: Color(red: 0, green: 0x60 / 255, blue: 0x40 / 255),
                                        border: Color(red: 0, green: 0xA0 / 255, blue: 0x60 / 255),
                                        text: Color(red: 0, green: 1, blue: 0x88 / 255))

    static func lampColours(_ lamp: ModMonitorModel.Lamp) -> ModMonitorLamp.Colours {
        switch lamp {
        case .blank, .noCarrier:
            return offLamp
        case .high:
            return redLamp
        case .low:
            return amberLamp
        case .ok:
            return greenLamp
        }
    }
}

/// TX I/Q and PA feedback: the monitor's source, this phone's own. PA
/// feedback is greyed while PureSignal is off on the Core's radio.
struct ModMonitorSourcePicker: View {
    @ObservedObject var model: ModMonitorModel
    var height: CGFloat = 36
    var font: CGFloat = 12

    var body: some View {
        HStack(spacing: 4) {
            ModMonitorSegment(label: "TX I/Q", chosen: model.source == .txIq, height: height, font: font,
                          disabled: !model.enabled) {
                model.choose(.txIq)
            }
            .accessibilityIdentifier("modMonitorSourceTx")
            ModMonitorSegment(label: "PA feedback", chosen: model.source == .paFeedback, height: height, font: font,
                          disabled: !model.enabled || !model.pureSignalRunning) {
                model.choose(.paFeedback)
            }
            .accessibilityHint(model.pureSignalRunning ? "" : ModMonitorModel.pureSignalOffText)
            .accessibilityIdentifier("modMonitorSourceFeedback")
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Source")
    }
}

/// One button of a set where one is chosen (the board's segments).
struct ModMonitorSegment: View {
    let label: String
    let chosen: Bool
    var height: CGFloat = 36
    var font: CGFloat = 12
    var disabled = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(label)
                .font(.system(size: font, weight: .bold))
                .lineLimit(1)
                .minimumScaleFactor(0.6)
                .foregroundStyle(foreground)
                .padding(.horizontal, 4)
                .frame(maxWidth: .infinity, minHeight: height)
                .background(background, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(border, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(disabled)
        .accessibilityLabel(label)
        .accessibilityAddTraits(chosen ? .isSelected : [])
    }

    private var foreground: Color {
        if disabled {
            return chosen ? Color(red: 0x78 / 255, green: 0x90 / 255, blue: 0xA8 / 255)
                : Color(red: 0x50 / 255, green: 0x60 / 255, blue: 0x70 / 255)
        }
        return chosen ? .white : ChromeColours.text
    }

    private var background: Color {
        if disabled {
            return chosen ? Color(red: 0x17 / 255, green: 0x30 / 255, blue: 0x48 / 255)
                : Color(red: 0x12 / 255, green: 0x1A / 255, blue: 0x24 / 255)
        }
        return chosen ? ChromeColours.buttonOnBlue : ChromeColours.button
    }

    private var border: Color {
        if disabled {
            return ChromeColours.insetBorder
        }
        return chosen ? ChromeColours.buttonOnBlueBorder : ChromeColours.buttonBorder
    }
}

/// RESET and Settings.
struct ModMonitorButton: View {
    let label: String
    var height: CGFloat = 40
    var font: CGFloat = 12
    var disabled = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(label)
                .font(.system(size: font + 1))
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .foregroundStyle(disabled ? Color(red: 0x50 / 255, green: 0x60 / 255, blue: 0x70 / 255)
                                 : ChromeColours.text)
                .frame(maxWidth: .infinity, minHeight: height)
                .background(disabled ? Color(red: 0x12 / 255, green: 0x1A / 255, blue: 0x24 / 255) : ChromeColours.button,
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(disabled ? ChromeColours.insetBorder : ChromeColours.buttonBorder, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(disabled)
        .accessibilityLabel(label)
    }
}

/// A status lamp: status only, never a control.
struct ModMonitorLamp: View {
    struct Colours: Equatable {
        var ground: Color
        var border: Color
        var text: Color
    }

    let text: String
    let colours: Colours
    var wide = false
    var font: CGFloat = 11
    var height: CGFloat = 22

    var body: some View {
        Text(text)
            .font(.system(size: font, weight: .heavy))
            .tracking(0.3)
            .lineLimit(1)
            .minimumScaleFactor(0.7)
            .foregroundStyle(colours.text)
            .padding(.horizontal, 6)
            .frame(minWidth: 60, maxWidth: wide ? .infinity : nil, minHeight: height)
            .background(colours.ground, in: RoundedRectangle(cornerRadius: 3))
            .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(colours.border, lineWidth: 1))
    }
}

/// A peak bar: the window's peak filled over its green, yellow and red
/// zones, the Core's hold as a white marker, ticks below.
struct ModMonitorPeakBar: View {
    let label: String
    let value: Double
    let hold: Double?
    let scale: (max: Double, yellow: Double, red: Double, ticks: [Int])
    var height: CGFloat = 14
    var font: CGFloat = 11

    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            HStack {
                Text(label)
                    .font(.system(size: font, weight: .semibold))
                    .foregroundStyle(Color(red: 0xB0 / 255, green: 0xC0 / 255, blue: 0xD0 / 255))
                Spacer(minLength: 4)
                Text("percent")
                    .font(.system(size: font))
                    .foregroundStyle(ChromeColours.caption)
            }
            Canvas { context, size in
                let width = size.width
                func x(_ percent: Double) -> CGFloat {
                    CGFloat(min(max(percent, 0), scale.max) / scale.max) * width
                }
                let yellow = x(scale.yellow)
                let red = x(scale.red)
                context.fill(Path(CGRect(x: 0, y: 0, width: yellow, height: size.height)),
                             with: .color(Color(red: 0x0E / 255, green: 0x1A / 255, blue: 0x26 / 255)))
                context.fill(Path(CGRect(x: yellow, y: 0, width: red - yellow, height: size.height)),
                             with: .color(Color(red: 0x2A / 255, green: 0x26 / 255, blue: 0x10 / 255)))
                context.fill(Path(CGRect(x: red, y: 0, width: width - red, height: size.height)),
                             with: .color(Color(red: 0x2E / 255, green: 0x14 / 255, blue: 0x14 / 255)))
                let fill = x(value)
                let inner = CGRect(x: 1, y: 1, width: max(0, fill - 1), height: size.height - 2)
                context.fill(Path(inner.intersection(CGRect(x: 0, y: 0, width: yellow, height: size.height))),
                             with: .color(ChromeColours.gaugeNormal))
                if fill > yellow {
                    context.fill(Path(CGRect(x: yellow, y: 1, width: min(fill, red) - yellow, height: size.height - 2)),
                                 with: .color(ChromeColours.gaugeWarn))
                }
                if fill > red {
                    context.fill(Path(CGRect(x: red, y: 1, width: fill - red, height: size.height - 2)),
                                 with: .color(ChromeColours.gaugeRed))
                }
                if let hold {
                    let at = x(hold)
                    context.fill(Path(CGRect(x: at - 1, y: 0, width: 2, height: size.height)), with: .color(.white))
                }
                context.stroke(Path(roundedRect: CGRect(origin: .zero, size: size).insetBy(dx: 0.5, dy: 0.5),
                                    cornerRadius: 2), with: .color(ChromeColours.insetBorder), lineWidth: 1)
            }
            .frame(height: height)
            GeometryReader { proxy in
                let width = proxy.size.width
                let maximum = scale.max
                let last = scale.ticks.count - 1
                ForEach(Array(scale.ticks.enumerated()), id: \.offset) { index, tick in
                    Text("\(tick)")
                        .font(.system(size: font * 0.9).monospacedDigit())
                        .foregroundStyle(ChromeColours.caption)
                        .fixedSize()
                        .alignmentGuide(.leading) { dimension in
                            let at = CGFloat(Double(tick) / maximum) * width
                            if index == 0 {
                                return -at
                            }
                            if index == last {
                                return dimension.width - at
                            }
                            return dimension.width / 2 - at
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
            }
            .frame(height: font * 1.2)
            .accessibilityHidden(true)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(label)
        .accessibilityValue(hold.map { "\(ModMonitorModel.percentText(value)), held \(ModMonitorModel.percentText($0))" }
                            ?? "No reading")
    }
}

/// The envelope, oldest point first, on a -100 to +160 percent scale, with
/// the gridlines at 0 and plus and minus 100 and this phone's flasher lines.
/// The Meters style draws it as the desktop's green scope with a mirrored fill.
struct ModMonitorScope: View {
    let points: [Double]
    let posFlash: Double
    let negFlash: Double
    let vintage: Bool

    static let low = -100.0
    static let high = 160.0

    var body: some View {
        Canvas { context, size in
            let rect = CGRect(origin: .zero, size: size)
            if vintage {
                drawVintage(context, rect)
            } else {
                drawTrace(context, rect)
            }
            context.stroke(Path(roundedRect: rect.insetBy(dx: 0.5, dy: 0.5), cornerRadius: 2),
                           with: .color(vintage ? Color(red: 0x6A / 255, green: 0x6A / 255, blue: 0x78 / 255)
                                        : ChromeColours.insetBorder), lineWidth: 1)
        }
    }

    private func y(_ percent: Double, _ height: CGFloat) -> CGFloat {
        let clamped = min(max(percent, Self.low), Self.high)
        return height - CGFloat((clamped - Self.low) / (Self.high - Self.low)) * height
    }

    private func drawTrace(_ context: GraphicsContext, _ rect: CGRect) {
        let height = rect.height
        let width = rect.width
        context.fill(Path(rect), with: .color(ChromeColours.inset))
        // The scope's reference lines: no modulation, and 100% either way.
        for line in stride(from: -100.0, through: 100, by: 100) {
            var path = Path()
            path.move(to: CGPoint(x: 0, y: y(line, height)))
            path.addLine(to: CGPoint(x: width, y: y(line, height)))
            context.stroke(path, with: .color(Color(red: 0x20 / 255, green: 0x30 / 255, blue: 0x40 / 255)), lineWidth: 1)
        }
        for line in [posFlash, -negFlash] {
            var path = Path()
            path.move(to: CGPoint(x: 0, y: y(line, height)))
            path.addLine(to: CGPoint(x: width, y: y(line, height)))
            context.stroke(path, with: .color(ChromeColours.gaugeRed.opacity(0.8)),
                           style: StrokeStyle(lineWidth: 1, dash: [4, 3]))
        }
        let label = Color(red: 0x70 / 255, green: 0x80 / 255, blue: 0x90 / 255)
        for (line, words) in [(100.0, "+100"), (0, "0"), (-100, "-100")] {
            context.draw(Text(words).font(.system(size: 9)).foregroundStyle(label),
                         at: CGPoint(x: 4, y: y(line, height) - 3), anchor: .bottomLeading)
        }
        context.draw(Text("+\(Int(posFlash))").font(.system(size: 9)).foregroundStyle(ChromeColours.refusalText),
                     at: CGPoint(x: width - 4, y: y(posFlash, height) - 2), anchor: .bottomTrailing)
        context.draw(Text("-\(Int(negFlash))").font(.system(size: 9)).foregroundStyle(ChromeColours.refusalText),
                     at: CGPoint(x: width - 4, y: y(-negFlash, height) - 2), anchor: .bottomTrailing)
        guard points.count > 1 else {
            return
        }
        let step = width / CGFloat(points.count - 1)
        var line = Path()
        for (index, point) in points.enumerated() {
            let at = CGPoint(x: CGFloat(index) * step, y: y(point, height))
            if index == 0 {
                line.move(to: at)
            } else {
                line.addLine(to: at)
            }
        }
        var fill = line
        fill.addLine(to: CGPoint(x: width, y: y(0, height)))
        fill.addLine(to: CGPoint(x: 0, y: y(0, height)))
        fill.closeSubpath()
        let green = Color(red: 0, green: 1, blue: 0x88 / 255)
        context.fill(fill, with: .color(green.opacity(0.16)))
        context.stroke(line, with: .color(green), lineWidth: 1.2)
    }

    private func drawVintage(_ context: GraphicsContext, _ rect: CGRect) {
        let height = rect.height
        let width = rect.width
        context.fill(Path(rect), with: .color(Color(red: 0x10 / 255, green: 0x30 / 255, blue: 0x18 / 255)))
        let graticule = Color(red: 192 / 255, green: 112 / 255, blue: 48 / 255).opacity(0.66)
        for column in 1..<10 {
            var path = Path()
            path.move(to: CGPoint(x: width * CGFloat(column) / 10, y: 0))
            path.addLine(to: CGPoint(x: width * CGFloat(column) / 10, y: height))
            context.stroke(path, with: .color(graticule), lineWidth: 1)
        }
        for row in 1..<6 {
            var path = Path()
            path.move(to: CGPoint(x: 0, y: height * CGFloat(row) / 6))
            path.addLine(to: CGPoint(x: width, y: height * CGFloat(row) / 6))
            context.stroke(path, with: .color(graticule), lineWidth: 1)
        }
        guard points.count > 1 else {
            return
        }
        let middle = height / 2
        let half = middle * 0.92
        let step = width / CGFloat(points.count - 1)
        var shape = Path()
        var bottom: [CGPoint] = []
        for (index, point) in points.enumerated() {
            let amplitude = CGFloat(min(max((100 + point) / 260, 0), 1))
            let x = CGFloat(index) * step
            let top = CGPoint(x: x, y: middle - amplitude * half)
            if index == 0 {
                shape.move(to: top)
            } else {
                shape.addLine(to: top)
            }
            bottom.append(CGPoint(x: x, y: middle + amplitude * half))
        }
        for point in bottom.reversed() {
            shape.addLine(to: point)
        }
        shape.closeSubpath()
        context.fill(shape, with: .color(Color(red: 0x8C / 255, green: 0xE0 / 255, blue: 0x50 / 255)))
        context.stroke(shape, with: .color(Color(red: 0x50 / 255, green: 0x90 / 255, blue: 0x30 / 255)), lineWidth: 1)
    }
}

/// One of the Meters style's lit analog faces: 0 to 140 percent, red
/// above 100, a dB row, the needle at the window's peak and a blue dot at
/// the Core's hold. Unlit (lamp off) without a carrier.
struct ModMeterDial: View {
    let caption: String
    let value: Double
    let hold: Double?
    let lit: Bool

    var body: some View {
        Canvas { context, size in
            // The face's own units: 128 by 96, scaled to fit.
            let scale = min(size.width / 128, size.height / 96)
            context.translateBy(x: (size.width - 128 * scale) / 2, y: (size.height - 96 * scale) / 2)
            context.scaleBy(x: scale, y: scale)
            let centre = CGPoint(x: 64, y: 84)
            let radius: CGFloat = 62
            func point(_ percent: Double, _ distance: CGFloat) -> CGPoint {
                let angle = (-52 + min(max(percent, 0), 140) / 140 * 104) * .pi / 180
                return CGPoint(x: centre.x + distance * CGFloat(sin(angle)), y: centre.y - distance * CGFloat(cos(angle)))
            }
            func arc(_ from: Double, _ to: Double) -> Path {
                var path = Path()
                let steps = 24
                for step in 0...steps {
                    let at = point(from + (to - from) * Double(step) / Double(steps), radius)
                    if step == 0 {
                        path.move(to: at)
                    } else {
                        path.addLine(to: at)
                    }
                }
                return path
            }
            let ink = Color(red: 0x1A / 255, green: 0x1A / 255, blue: 0x1A / 255)
            let red = Color(red: 0xC0 / 255, green: 0x20 / 255, blue: 0x20 / 255)
            let face = Path(roundedRect: CGRect(x: 1, y: 1, width: 126, height: 94), cornerRadius: 6)
            context.fill(face, with: .color(lit ? Color(red: 0xF3 / 255, green: 0xE2 / 255, blue: 0xB0 / 255)
                                            : Color(red: 0x3A / 255, green: 0x34 / 255, blue: 0x26 / 255)))
            context.stroke(face, with: .color(lit ? Color(red: 0xB0 / 255, green: 0x8A / 255, blue: 0x40 / 255)
                                              : Color(red: 0x6A / 255, green: 0x5A / 255, blue: 0x3A / 255)),
                           lineWidth: 1.5)
            context.stroke(arc(0, 100), with: .color(ink), lineWidth: 1.4)
            context.stroke(arc(100, 140), with: .color(red), lineWidth: 2.4)
            for percent in stride(from: 0.0, through: 140, by: 20) {
                var tick = Path()
                let long = percent.truncatingRemainder(dividingBy: 50) == 0 || percent == 140
                tick.move(to: point(percent, radius))
                tick.addLine(to: point(percent, radius - (long ? 8 : 5)))
                context.stroke(tick, with: .color(percent > 100 ? red : ink), lineWidth: 1.2)
            }
            let number = lit ? ink : Color(red: 0x6A / 255, green: 0x5E / 255, blue: 0x48 / 255)
            for (percent, words) in [(0.0, "0"), (50, "50"), (100, "100"), (140, "140")] {
                context.draw(Text(words).font(.system(size: 8, weight: .bold)).foregroundStyle(number),
                             at: point(percent, radius - 17))
            }
            for (percent, words) in [(50.0, "-6"), (100, "0"), (140, "+3")] {
                context.draw(Text(words).font(.system(size: 7, weight: .bold)).foregroundStyle(red),
                             at: point(percent, radius + 7))
            }
            let captionInk = lit ? Color(red: 0x3A / 255, green: 0x2A / 255, blue: 0x10 / 255) : number
            context.draw(Text(caption).font(.system(size: 7, weight: .heavy)).foregroundStyle(captionInk),
                         at: CGPoint(x: 64, y: 66))
            context.draw(Text("%  \u{00B7}  dB").font(.system(size: 7, weight: .semibold))
                .foregroundStyle(Color(red: 0x7A / 255, green: 0x5A / 255, blue: 0x2A / 255)),
                         at: CGPoint(x: 64, y: 75))
            var needle = Path()
            needle.move(to: centre)
            needle.addLine(to: point(lit ? value : 0, radius - 4))
            context.stroke(needle, with: .color(lit ? Color(red: 0x11 / 255, green: 0x11 / 255, blue: 0x11 / 255)
                                                : Color(red: 0x1E / 255, green: 0x1A / 255, blue: 0x12 / 255)),
                           style: StrokeStyle(lineWidth: 1.6, lineCap: .round))
            if lit, let hold {
                let at = point(hold, radius)
                let dot = Path(ellipseIn: CGRect(x: at.x - 2.8, y: at.y - 2.8, width: 5.6, height: 5.6))
                context.fill(dot, with: .color(Color(red: 0x1E / 255, green: 0x6C / 255, blue: 1)))
                context.stroke(dot, with: .color(.white), lineWidth: 0.8)
            }
            context.fill(Path(ellipseIn: CGRect(x: centre.x - 4, y: centre.y - 4, width: 8, height: 8)),
                         with: .color(Color(red: 0x22 / 255, green: 0x22 / 255, blue: 0x22 / 255)))
        }
        .aspectRatio(128 / 96, contentMode: .fit)
        .accessibilityElement()
    }
}

/// The Meters style's asymmetry bar between the faces: up for a positive
/// lean, down for a negative one, to 40 percent.
struct ModMonitorAsymmetryBar: View {
    let value: Double
    let lit: Bool

    var body: some View {
        Canvas { context, size in
            let scale = min(size.width / 22, size.height / 96)
            context.translateBy(x: (size.width - 22 * scale) / 2, y: (size.height - 96 * scale) / 2)
            context.scaleBy(x: scale, y: scale)
            let label = Color(red: 0x80 / 255, green: 0x90 / 255, blue: 0xA0 / 255)
            context.draw(Text("Pos").font(.system(size: 7)).foregroundStyle(label), at: CGPoint(x: 11, y: 5))
            context.draw(Text("Neg").font(.system(size: 7)).foregroundStyle(label), at: CGPoint(x: 11, y: 91))
            let track = Path(roundedRect: CGRect(x: 6, y: 10, width: 10, height: 76), cornerRadius: 2)
            context.fill(track, with: .color(ChromeColours.inset))
            context.stroke(track, with: .color(ChromeColours.insetBorder), lineWidth: 1)
            var zero = Path()
            zero.move(to: CGPoint(x: 4, y: 48))
            zero.addLine(to: CGPoint(x: 18, y: 48))
            context.stroke(zero, with: .color(Color(red: 0x60 / 255, green: 0x70 / 255, blue: 0x80 / 255)), lineWidth: 1)
            let length = lit ? CGFloat(min(abs(value), 40) / 40 * 38) : 0
            if length > 0 {
                let top = value >= 0 ? 48 - length : 48
                context.fill(Path(CGRect(x: 7, y: top, width: 8, height: length)), with: .color(ChromeColours.buttonOnAmberText))
            }
        }
        .aspectRatio(22 / 96, contentMode: .fit)
        .accessibilityElement()
    }
}

/// Where the TX panel keeps the monitor: shown only while the transmitting
/// slice is in AM, SAM or DSB, so the monitor leaves the screen (and stops
/// the Core's readings) when the mode leaves them.
struct ModMonitorSlot: View {
    @ObservedObject var model: ModMonitorModel

    var body: some View {
        if model.shown {
            ModMonitorView(model: model)
        }
    }
}
