// NereusSDR for iOS: the SSB Mod Monitor's face on the TX panel: peak and average, readouts, OVER, the signal lamp and the envelope
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The SSB Mod Monitor, drawn as the AM monitor is: a title bar with the
/// transmitting slice's mode and letter, the source, peak and average as the
/// lit analog pair (or bars), the readouts, the OVER lamp, the signal lamp,
/// the envelope scope, and RESET and Settings. Every control is a labelled
/// button; greyed controls say why above them.
struct SsbModMonitorView: View {
    @ObservedObject var model: SsbModMonitorModel
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
                        .background(ModMonitorView.reasonGround, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                        .accessibilityIdentifier("ssbModMonitorReason")
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
            SsbModMonitorSettingsSheet(model: model)
                .workspacePresentationRoot()
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("SSB Mod Monitor")
        .accessibilityIdentifier("ssbModMonitor")
    }

    private var controls: some View {
        VStack(alignment: .leading, spacing: 9) {
            sourceRow
            if model.enabled, let reason = model.paFeedbackReason {
                why(reason)
                    .accessibilityIdentifier("ssbModMonitorPureSignalReason")
            }
            switch model.meterStyle {
            case .meters:
                HStack(spacing: 4) {
                    ModMeterDial(caption: "PEAK", value: model.peakBar, hold: model.peakHold, lit: model.live)
                        .accessibilityLabel("Peak")
                        .accessibilityValue(model.live ? ModMonitorModel.percentText(model.peakBar) : "No reading")
                    ModMeterDial(caption: "AVERAGE", value: model.avgBar, hold: nil, lit: model.live)
                        .accessibilityLabel("Average")
                        .accessibilityValue(model.live ? ModMonitorModel.percentText(model.avgBar) : "No reading")
                }
                .accessibilityElement(children: .contain)
                .accessibilityIdentifier("ssbModMonitorMeters")
            case .bars:
                ModMonitorPeakBar(label: "Peak", value: model.peakBar, hold: model.peakHold,
                                  scale: SsbModMonitorModel.scale, height: trackHeight, font: small)
                    .accessibilityValue(model.live ? ModMonitorModel.percentText(model.peakBar) : "No reading")
                    .accessibilityIdentifier("ssbModMonitorPeakBar")
                ModMonitorPeakBar(label: "Average", value: model.avgBar, hold: nil,
                                  scale: SsbModMonitorModel.scale, height: trackHeight, font: small)
                    .accessibilityValue(model.live ? ModMonitorModel.percentText(model.avgBar) : "No reading")
                    .accessibilityIdentifier("ssbModMonitorAverageBar")
            }
            readouts
            HStack(spacing: 6) {
                ModMonitorLamp(text: "OVER", colours: model.overLit ? ModMonitorView.redLamp : ModMonitorView.offLamp,
                               wide: false, font: small, height: buttonHeight * 0.66)
                Text("at \(model.overDbfs) dBFS")
                    .font(.system(size: small))
                    .foregroundStyle(ChromeColours.textDim)
                    .lineLimit(1)
            }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("Over lamp")
            .accessibilityValue((model.overLit ? "Lit" : "Off") + ", at \(model.overDbfs) dB full scale")
            .accessibilityIdentifier("ssbModMonitorOver")
            HStack(spacing: 8) {
                caption("Signal")
                ModMonitorLamp(text: model.lamp.text, colours: Self.lampColours(model.lamp), wide: true, font: small,
                               height: buttonHeight * 0.78)
                    .accessibilityLabel("Signal lamp")
                    .accessibilityValue(model.lamp.text)
                    .accessibilityIdentifier("ssbModMonitorLamp")
            }
            VStack(alignment: .leading, spacing: 3) {
                Text("Envelope, percent of full scale")
                    .font(.system(size: small))
                    .foregroundStyle(ChromeColours.textDim)
                SsbModMonitorScope(points: model.scope, overPct: SsbModMonitorModel.percent(dbfs: model.overDbfs),
                                   vintage: model.meterStyle == .meters)
                    .frame(height: scopeHeight)
                    .accessibilityElement()
                    .accessibilityLabel("Envelope scope")
                    .accessibilityValue(model.scope.isEmpty ? "Empty" : "\(model.scope.count) points")
                    .accessibilityIdentifier("ssbModMonitorScope")
            }
            if model.enabled, !model.live {
                why(SsbModMonitorModel.waitingText)
                    .accessibilityIdentifier("ssbModMonitorWaiting")
            }
            HStack(spacing: 6) {
                ModMonitorButton(label: "RESET", height: buttonHeight + 4, font: text, disabled: !model.enabled) {
                    model.reset()
                }
                .accessibilityHint("Clears the peak hold and the over lamp for every device watching")
                .accessibilityIdentifier("ssbModMonitorReset")
                ModMonitorButton(label: "Settings", height: buttonHeight + 4, font: text, disabled: !model.enabled) {
                    settingsOpen = true
                }
                .accessibilityLabel("SSB Mod Monitor settings")
                .accessibilityIdentifier("ssbModMonitorSettings")
            }
            if let note = model.note {
                Text(note)
                    .font(.system(size: small))
                    .foregroundStyle(note == SsbModMonitorModel.clearedText ? ModMonitorView.doneText : ChromeColours.refusalText)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("ssbModMonitorNote")
            }
            foot(SsbModMonitorModel.resetFootText)
            foot(SsbModMonitorModel.crestFootText)
        }
    }

    private func foot(_ words: String) -> some View {
        Text(words)
            .font(.system(size: small * 0.95))
            .foregroundStyle(ChromeColours.caption)
            .fixedSize(horizontal: false, vertical: true)
    }

    private var title: some View {
        // At a large text size the chip goes under the name, so neither is cut.
        let layout = large ? AnyLayout(VStackLayout(alignment: .leading, spacing: 2))
            : AnyLayout(HStackLayout(spacing: 6))
        return layout {
            Text("SSB Mod Monitor")
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
                    .foregroundStyle(ModMonitorView.chipText)
                    .lineLimit(1)
                    .padding(.horizontal, 6)
                    .background(ModMonitorView.chipGround, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                    .accessibilityIdentifier("ssbModMonitorChip")
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
            SsbModMonitorSourcePicker(model: model, height: buttonHeight, font: text)
        }
    }

    private var readouts: some View {
        let columns = Array(repeating: GridItem(.flexible(minimum: 0), spacing: 8), count: large ? 1 : 2)
        return LazyVGrid(columns: columns, spacing: 4) {
            readout("PEAK", model.peakText, name: "Peak", identifier: "ssbModMonitorPeak")
            readout("AVG", model.avgText, name: "Average", identifier: "ssbModMonitorAverage")
            readout("CREST", model.crestText, name: "Crest", identifier: "ssbModMonitorCrest")
            readout("%FS", model.percentText, name: "Percent of full scale", identifier: "ssbModMonitorPercent")
        }
    }

    private func readout(_ label: String, _ reading: String, name: String, identifier: String) -> some View {
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
        .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(name)
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
            .foregroundStyle(ModMonitorView.whyText)
            .fixedSize(horizontal: false, vertical: true)
    }

    static func lampColours(_ lamp: SsbModMonitorModel.Lamp) -> ModMonitorLamp.Colours {
        switch lamp {
        case .blank, .noSignal:
            return ModMonitorView.offLamp
        case .over:
            return ModMonitorView.redLamp
        case .on:
            return ModMonitorView.greenLamp
        }
    }
}

/// TX I/Q and PA FB: the monitor's source, this phone's own. PA FB is greyed
/// while PureSignal is off on the Core's radio.
struct SsbModMonitorSourcePicker: View {
    @ObservedObject var model: SsbModMonitorModel
    var height: CGFloat = 36
    var font: CGFloat = 12

    var body: some View {
        HStack(spacing: 4) {
            ModMonitorSegment(label: "TX I/Q", chosen: model.source == .txIq, height: height, font: font,
                              disabled: !model.enabled) {
                model.choose(.txIq)
            }
            .accessibilityIdentifier("ssbModMonitorSourceTx")
            ModMonitorSegment(label: "PA FB", chosen: model.source == .paFeedback, height: height, font: font,
                              disabled: !model.enabled || !model.pureSignalRunning) {
                model.choose(.paFeedback)
            }
            .accessibilityHint(model.paFeedbackReason ?? "")
            .accessibilityIdentifier("ssbModMonitorSourceFeedback")
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Source")
    }
}

/// The envelope, oldest point first, on a 0 to 120 percent of full scale
/// scale, with gridlines at 0, 50 and 100 and this phone's OVER line. The
/// Meters style draws it as the AM monitor's green scope, mirrored, where
/// 100 percent fills the screen.
struct SsbModMonitorScope: View {
    let points: [Double]
    let overPct: Double
    let vintage: Bool

    static let high = 120.0

    var body: some View {
        Canvas { context, size in
            let rect = CGRect(origin: .zero, size: size)
            if vintage {
                drawVintage(context, rect)
            } else {
                drawTrace(context, rect)
            }
            context.stroke(Path(roundedRect: rect.insetBy(dx: 0.5, dy: 0.5), cornerRadius: 2), // kept-radius: canvas
                           with: .color(vintage ? ChromeColours.iconMuted : ChromeColours.insetBorder), lineWidth: 1)
        }
    }

    private func y(_ percent: Double, _ height: CGFloat) -> CGFloat {
        height - CGFloat(min(max(percent, 0), Self.high) / Self.high) * height
    }

    private func drawTrace(_ context: GraphicsContext, _ rect: CGRect) {
        let height = rect.height
        let width = rect.width
        context.fill(Path(rect), with: .color(ChromeColours.inset))
        for (line, words) in [(100.0, "100"), (50, "50"), (0, "0")] {
            var path = Path()
            path.move(to: CGPoint(x: 0, y: y(line, height)))
            path.addLine(to: CGPoint(x: width, y: y(line, height)))
            context.stroke(path, with: .color(ChromeColours.sliderTrack), lineWidth: 1)
            context.draw(Text(words).font(.system(size: 9)).foregroundStyle(ChromeColours.caption),
                         at: CGPoint(x: 4, y: y(line, height) - 3), anchor: .bottomLeading)
        }
        var over = Path()
        over.move(to: CGPoint(x: 0, y: y(overPct, height)))
        over.addLine(to: CGPoint(x: width, y: y(overPct, height)))
        context.stroke(over, with: .color(ChromeColours.gaugeRed.opacity(0.8)),
                       style: StrokeStyle(lineWidth: 1, dash: [4, 3]))
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
        context.fill(fill, with: .color(ModMonitorScope.traceGreen.opacity(0.16)))
        context.stroke(line, with: .color(ModMonitorScope.traceGreen), lineWidth: 1.2)
    }

    private func drawVintage(_ context: GraphicsContext, _ rect: CGRect) {
        let height = rect.height
        let width = rect.width
        context.fill(Path(rect), with: .color(ModMonitorScope.vintageGround))
        for column in 1..<10 {
            var path = Path()
            path.move(to: CGPoint(x: width * CGFloat(column) / 10, y: 0))
            path.addLine(to: CGPoint(x: width * CGFloat(column) / 10, y: height))
            context.stroke(path, with: .color(ModMonitorScope.vintageGraticule), lineWidth: 1)
        }
        for row in 1..<6 {
            var path = Path()
            path.move(to: CGPoint(x: 0, y: height * CGFloat(row) / 6))
            path.addLine(to: CGPoint(x: width, y: height * CGFloat(row) / 6))
            context.stroke(path, with: .color(ModMonitorScope.vintageGraticule), lineWidth: 1)
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
            let amplitude = CGFloat(min(max(point / 100, 0), 1))
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
        context.fill(shape, with: .color(ModMonitorScope.vintageFill))
        context.stroke(shape, with: .color(ModMonitorScope.vintageEdge), lineWidth: 1)
    }
}

/// Where the TX panel keeps the monitor: shown only while the transmitting
/// slice is in USB or LSB, so the monitor leaves the screen (and stops the
/// Core's readings) when the mode leaves them.
struct SsbModMonitorSlot: View {
    @ObservedObject var model: SsbModMonitorModel

    var body: some View {
        if model.shown {
            SsbModMonitorView(model: model)
        }
    }
}
