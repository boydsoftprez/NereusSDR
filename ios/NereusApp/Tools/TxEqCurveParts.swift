// NereusSDR for iOS: the TX Equalizer page's curve: its drawing with numbered marks, the point editor and the curve's values
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The parts of the TX Equalizer page that show and change the Core's TX
/// EQ curve (plan Task 59a, R-IOS-18, the board's #eqcurve-review): the
/// curve card (which EQ is on the air, the drawing, its state), the Point
/// card (Previous and Next, and − and + with a field for frequency, gain
/// and Q) and the Curve values card (Shape, Bands, the curve preamp, Range,
/// Reset and the list of points). Every change is a labelled control; the
/// drawing is always the curve the Core sent back.
enum TxEqCurveParts {
    // The board's colours for the curve (part-style-eqcurve.css).
    static let plotGround = rgb(0x0A, 0x12, 0x1B)
    static let grid = rgb(0x1D, 0x2A, 0x38)
    static let zero = rgb(0x7F, 0x93, 0xA8)
    static let axisText = rgb(0x83, 0x94, 0xA5)
    static let axisName = rgb(0x5F, 0x6F, 0x80)
    static let preampLine = rgb(0xE9, 0xEE, 0xF4).opacity(0.7)
    static let preampText = rgb(0xDD, 0xE6, 0xEF)
    static let line = rgb(0x00, 0xB4, 0xD8)
    static let markFill = rgb(0x0C, 0x18, 0x24)
    static let markEdge = rgb(0x7F, 0xDC, 0xEF)
    static let markText = rgb(0xD9, 0xF5, 0xFB)
    static let chosenEdge = rgb(0xE8, 0xFB, 0xFF)
    static let chosenText = rgb(0x04, 0x14, 0x1C)
    static let bigText = rgb(0xC9, 0xD3, 0xDC)
    static let onAirCurve = rgb(0x6E, 0xE0, 0x7A)
    static let onAirTen = rgb(0xF0, 0xB4, 0x4C)
    static let onAirOff = rgb(0x6A, 0x76, 0x84)
    static let transmittingGround = rgb(0x2A, 0x12, 0x14)
    static let transmittingDot = rgb(0xFF, 0x4D, 0x4D)
    static let transmittingHead = rgb(0xFF, 0xC7, 0xC2)
    static let lineHead = rgb(0xE8, 0xF3, 0xEA)
    static let lineBody = rgb(0x8E, 0x9D, 0xAB)
    static let noticeGround = rgb(0x14, 0x30, 0x4A)
    static let noticeText = rgb(0xCF, 0xE8, 0xFF)
    static let refusalGround = rgb(0x3A, 0x1C, 0x18)
    static let refusalText = rgb(0xFF, 0xB3, 0xA1)
    static let softReason = rgb(0xC8, 0xA6, 0x4A)
    static let hint = rgb(0x7F, 0x8E, 0x9D)
    static let caption = rgb(0x9F, 0xB0, 0xC0)
    static let rowChosen = rgb(0x11, 0x34, 0x4A)
    static let rowText = rgb(0xD7, 0xE2, 0xEC)
    static let rowHead = rgb(0x7D, 0x8B, 0x99)
    static let rowLine = rgb(0x17, 0x22, 0x30)

    static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }

    // MARK: Words

    /// A point's frequency as the page shows it, in whole hertz: "1200 Hz".
    static func hz(_ value: Double) -> String {
        "\(Int64(value.rounded())) Hz"
    }

    /// A curve gain to 0.1 dB, signed: "+3.0 dB", "−6.0 dB", "0.0 dB".
    static func db(_ value: Double) -> String {
        ValuePadModel.text(value, decimals: 1, plus: true) + " dB"
    }

    /// A Q to two places: "1.50".
    static func q(_ value: Double) -> String {
        String(format: "%.2f", value)
    }

    /// The caption under the drawing, for a curve that draws.
    static func caption(_ curve: TxEqCurve) -> String {
        if curve.state == .default {
            return "Default curve: flat, as every factory TX profile starts."
        }
        return "Saved curve \u{00B7} \(curve.points.count) points \u{00B7} "
            + (curve.parametric ? "bells" : "straight lines")
    }

    /// A mark's name for VoiceOver: "Point 3, 1200 Hz, −1.5 dB".
    static func markLabel(_ index: Int, _ point: TxEqCurve.Point) -> String {
        "Point \(index + 1), \(hz(point.frequencyHz)), \(db(point.gainDb))"
    }

    /// The even Hz scale's step: at most six steps across the range.
    static func tickStep(_ span: Double) -> Double {
        for step in [50.0, 100, 200, 250, 500, 1000, 2000, 2500, 5000] where span / step <= 6 {
            return step
        }
        return 10000
    }

    /// Text sizes grow with large type, as the rest of this page's fixed sizes do.
    static func size(_ base: CGFloat, large: Bool) -> CGFloat {
        large ? (base * 1.3).rounded() : base
    }

    // MARK: The curve card

    /// Which EQ is on the air, whether the radio is transmitting, the
    /// drawing and the curve's state.
    struct CurveCard: View {
        @ObservedObject var model: TxEqualizerModel
        var sideways = false
        @Environment(\.dynamicTypeSize) private var typeSize

        var body: some View {
            let large = typeSize.isAccessibilitySize
            let height: CGFloat = large ? (sideways ? 200 : 214) : (sideways ? 168 : 190)
            let connected = model.curveReason != TxEqualizerModel.notConnectedReason
            VStack(alignment: .leading, spacing: 8) {
                SpotHubPage.Heading(text: "Parametric curve", tag: .core)
                SpotHubPage.Card {
                    if model.curve != nil, let onAir = model.onAir {
                        onAirLine(onAir, large: large)
                    }
                    if model.transmitting, model.curve != nil, connected {
                        transmittingLine(large: large)
                    }
                    Drawing(curve: model.curve, selected: model.selected, large: large, dim: !connected,
                            height: height) { model.choose($0) }
                    stateLine(large: large, connected: connected)
                }
            }
        }

        private func onAirLine(_ onAir: TxEqualizerModel.OnAir, large: Bool) -> some View {
            let words: (dot: Color, head: String, body: String) = switch onAir {
            case .curve:
                (TxEqCurveParts.onAirCurve, "On the air: this curve", "TX EQ is on and Legacy EQ is off.")
            case .tenBands:
                (TxEqCurveParts.onAirTen, "On the air: the ten-band EQ",
                 "Legacy EQ is on, so this curve is kept but not in use.")
            case .off:
                (TxEqCurveParts.onAirOff, "TX EQ is off", "Neither this curve nor the ten bands shape the audio.")
            }
            return StateLine(dot: words.dot, head: words.head, text: words.body, headColour: TxEqCurveParts.lineHead,
                             large: large)
                .accessibilityIdentifier("txEq.onAir")
        }

        private func transmittingLine(large: Bool) -> some View {
            let text = model.takesOnAir
                ? "Every TX EQ control stays live, as on the desktop. A change reaches the air as soon as the Core "
                    + "takes it. Nothing on this page keys the radio."
                : "This Core takes no TX EQ change while the radio is on the air, so every TX EQ control waits for "
                    + "it to stop."
            return StateLine(dot: TxEqCurveParts.transmittingDot, head: "Transmitting now", text: text,
                             headColour: TxEqCurveParts.transmittingHead, large: large)
                .background(TxEqCurveParts.transmittingGround)
                .accessibilityIdentifier("txEq.transmitting")
        }

        @ViewBuilder
        private func stateLine(large: Bool, connected: Bool) -> some View {
            if !connected {
                ToolPageParts.Reason(text: TxEqualizerModel.notConnectedReason, identifier: "txEq.curveState")
            } else if let curve = model.curve {
                if curve.state == .unavailable {
                    ToolPageParts.Reason(text: TxEqualizerModel.unavailableText, identifier: "txEq.curveState")
                } else {
                    Text(TxEqCurveParts.caption(curve))
                        .font(.system(size: TxEqCurveParts.size(12, large: large)))
                        .foregroundStyle(TxEqCurveParts.caption)
                        .fixedSize(horizontal: false, vertical: true)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 6)
                        .accessibilityIdentifier("txEq.curveState")
                }
            } else {
                ToolPageParts.Reason(text: TxEqualizerModel.olderCoreReason, identifier: "txEq.curveState")
            }
        }
    }

    /// A line with a dot: which EQ is on the air, or that the radio is transmitting.
    struct StateLine: View {
        let dot: Color
        let head: String
        let text: String
        let headColour: Color
        let large: Bool

        var body: some View {
            HStack(alignment: .top, spacing: 9) {
                Circle().fill(dot).frame(width: 10, height: 10).padding(.top, 4)
                VStack(alignment: .leading, spacing: 1) {
                    Text(head)
                        .font(.system(size: TxEqCurveParts.size(14, large: large), weight: .semibold))
                        .foregroundStyle(headColour)
                    Text(text)
                        .font(.system(size: TxEqCurveParts.size(11.5, large: large)))
                        .foregroundStyle(TxEqCurveParts.lineBody)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 7)
            .accessibilityElement(children: .combine)
        }
    }

    // MARK: The drawing

    /// The curve on an even Hz scale from its low end to its high end and
    /// on −24 to +24 dB, gridlines at ±12 and a brighter 0 dB line; the
    /// curve's preamp dashed and labelled; each point a numbered mark at
    /// its own gain, which chooses it; the chosen point's guide line.
    struct Drawing: View {
        let curve: TxEqCurve?
        let selected: Int
        let large: Bool
        let dim: Bool
        let height: CGFloat
        let choose: (Int) -> Void

        var body: some View {
            GeometryReader { geometry in
                let frame = Frame(size: geometry.size, large: large, curve: curve)
                ZStack(alignment: .topLeading) {
                    Canvas { context, _ in
                        draw(frame, in: &context)
                    }
                    .accessibilityElement()
                    .accessibilityLabel(summary)
                    .accessibilityIdentifier("txEq.curveDrawing")
                    if let curve, curve.drawable {
                        marks(curve, frame)
                    } else {
                        Text(curve?.state == .unavailable ? "Unavailable" : "--")
                            .font(.system(size: frame.font * 2, weight: .bold))
                            .foregroundStyle(TxEqCurveParts.bigText)
                            .padding(.horizontal, 10)
                            .padding(.vertical, 4)
                            .background(TxEqCurveParts.plotGround.opacity(0.9), in: RoundedRectangle(cornerRadius: 4))
                            .position(x: (frame.left + frame.right) / 2, y: (frame.top + frame.bottom) / 2)
                            .accessibilityIdentifier("txEq.curveMissing")
                    }
                }
            }
            .frame(height: height)
            .padding(.horizontal, 6)
            .padding(.top, 6)
            .padding(.bottom, 2)
            .background(TxEqCurveParts.plotGround)
            .opacity(dim ? 0.45 : 1)
            .saturation(dim ? 0 : 1)
        }

        private var summary: String {
            guard let curve, curve.drawable else {
                return "TX EQ curve not shown"
            }
            return "TX EQ curve from \(Int64(curve.minHz.rounded())) to \(Int64(curve.maxHz.rounded())) hertz, "
                + "\(curve.points.count) points"
        }

        /// Where the plot sits in the drawing and how its scales map.
        struct Frame {
            let left: CGFloat
            let right: CGFloat
            let top: CGFloat
            let bottom: CGFloat
            let height: CGFloat
            let font: CGFloat
            let minHz: Double
            let maxHz: Double

            init(size: CGSize, large: Bool, curve: TxEqCurve?) {
                font = large ? 12 : 9.5
                left = large ? 40 : 32
                right = size.width - 8
                top = 8
                bottom = size.height - (large ? 22 : 18)
                height = size.height
                minHz = curve?.minHz ?? 0
                maxHz = curve?.maxHz ?? 1
            }

            func x(_ hz: Double) -> CGFloat {
                let span = maxHz - minHz
                guard span > 0 else {
                    return left
                }
                return left + CGFloat((hz - minHz) / span) * (right - left)
            }

            func y(_ db: Double) -> CGFloat {
                top + CGFloat((24 - TxEqCurve.drawnDb(db)) / 48) * (bottom - top)
            }
        }

        private func label(_ text: String, size: CGFloat, colour: Color = TxEqCurveParts.axisText) -> Text {
            Text(text).font(.system(size: size)).foregroundColor(colour)
        }

        private func draw(_ frame: Frame, in context: inout GraphicsContext) {
            // The drawing's scale, a line each 12 dB from +24 to −24.
            for level in stride(from: TxEqCurve.scaleDb.upperBound, through: TxEqCurve.scaleDb.lowerBound, by: -12) {
                var path = Path()
                path.move(to: CGPoint(x: frame.left, y: frame.y(level)))
                path.addLine(to: CGPoint(x: frame.right, y: frame.y(level)))
                context.stroke(path, with: .color(level == 0 ? TxEqCurveParts.zero : TxEqCurveParts.grid),
                               lineWidth: level == 0 ? 1.2 : 1)
                let text = level > 0 ? "+\(Int(level))" : level < 0 ? "\u{2212}\(Int(-level))" : "0"
                context.draw(label(text, size: frame.font), at: CGPoint(x: frame.left - 4, y: frame.y(level)),
                             anchor: .trailing)
            }
            context.draw(label("dB", size: frame.font - 1, colour: TxEqCurveParts.axisName),
                         at: CGPoint(x: frame.left + 2, y: frame.top + 2), anchor: .topLeading)
            let baseline = frame.height - 4
            guard let curve, curve.drawable else {
                context.draw(label("--", size: frame.font), at: CGPoint(x: frame.left, y: baseline),
                             anchor: .bottomLeading)
                context.draw(label("-- Hz", size: frame.font), at: CGPoint(x: frame.right, y: baseline),
                             anchor: .bottomTrailing)
                return
            }
            let span = curve.maxHz - curve.minHz
            context.draw(label("\(Int64(curve.minHz.rounded()))", size: frame.font),
                         at: CGPoint(x: frame.left, y: baseline), anchor: .bottomLeading)
            context.draw(label("\(Int64(curve.maxHz.rounded())) Hz", size: frame.font),
                         at: CGPoint(x: frame.right, y: baseline), anchor: .bottomTrailing)
            let step = TxEqCurveParts.tickStep(span)
            var tick = (curve.minHz / step).rounded(.up) * step
            while tick < curve.maxHz {
                if tick - curve.minHz >= span * 0.1, curve.maxHz - tick >= span * 0.22 {
                    var path = Path()
                    path.move(to: CGPoint(x: frame.x(tick), y: frame.top))
                    path.addLine(to: CGPoint(x: frame.x(tick), y: frame.bottom))
                    context.stroke(path, with: .color(TxEqCurveParts.grid), lineWidth: 1)
                    context.draw(label("\(Int64(tick))", size: frame.font), at: CGPoint(x: frame.x(tick), y: baseline),
                                 anchor: .bottom)
                }
                tick += step
            }
            if curve.preampDb != 0 {
                var path = Path()
                path.move(to: CGPoint(x: frame.left, y: frame.y(curve.preampDb)))
                path.addLine(to: CGPoint(x: frame.right, y: frame.y(curve.preampDb)))
                context.stroke(path, with: .color(TxEqCurveParts.preampLine),
                               style: StrokeStyle(lineWidth: 1.2, dash: [5, 4]))
                context.draw(label("Preamp \(TxEqCurveParts.db(curve.preampDb))", size: frame.font - 0.5,
                                   colour: TxEqCurveParts.preampText),
                             at: CGPoint(x: frame.right - 2, y: frame.y(curve.preampDb) + 2), anchor: .topTrailing)
            }
            var line = Path()
            for (index, sample) in curve.line(samples: 241).enumerated() {
                let point = CGPoint(x: frame.x(sample.hz), y: frame.y(sample.db))
                if index == 0 {
                    line.move(to: point)
                } else {
                    line.addLine(to: point)
                }
            }
            context.stroke(line, with: .color(TxEqCurveParts.line), style: StrokeStyle(lineWidth: 2.2, lineJoin: .round))
            if curve.points.indices.contains(selected) {
                var guide = Path()
                let x = frame.x(curve.points[selected].frequencyHz)
                guide.move(to: CGPoint(x: x, y: frame.top))
                guide.addLine(to: CGPoint(x: x, y: frame.bottom))
                context.stroke(guide, with: .color(TxEqCurveParts.line.opacity(0.45)),
                               style: StrokeStyle(lineWidth: 1, dash: [2, 3]))
            }
        }

        private func marks(_ curve: TxEqCurve, _ frame: Frame) -> some View {
            let radius: CGFloat = large ? 9 : 7.5
            let crowded = (frame.right - frame.left) / CGFloat(curve.points.count) < radius * 2.4
            return ForEach(Array(curve.points.enumerated()), id: \.offset) { index, point in
                let chosen = index == selected
                let size = crowded && !chosen ? radius * 0.72 : radius
                Button {
                    choose(index)
                } label: {
                    ZStack {
                        Circle()
                            .fill(chosen ? TxEqCurveParts.line : TxEqCurveParts.markFill)
                        Circle()
                            .strokeBorder(chosen ? TxEqCurveParts.chosenEdge : TxEqCurveParts.markEdge, lineWidth: 1.6)
                        if !crowded || chosen {
                            Text("\(index + 1)")
                                .font(.system(size: size * 1.25, weight: .bold))
                                .foregroundStyle(chosen ? TxEqCurveParts.chosenText : TxEqCurveParts.markText)
                                .minimumScaleFactor(0.5)
                        }
                    }
                    .frame(width: size * 2, height: size * 2)
                    .frame(width: max(size * 2, 22), height: max(size * 2, 28))
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .position(x: frame.x(point.frequencyHz), y: frame.y(point.gainDb))
                .zIndex(chosen ? 1 : 0)
                .accessibilityLabel(TxEqCurveParts.markLabel(index, point))
                .accessibilityAddTraits(chosen ? .isSelected : [])
                .accessibilityIdentifier("txEq.mark\(index)")
            }
        }
    }

    // MARK: The point editor

    /// Previous, "Point N of M" and Next, then frequency, gain and Q, each
    /// with −, a field for the number pad and +, the step written under its name.
    struct PointCard: View {
        @ObservedObject var model: TxEqualizerModel
        @Environment(\.dynamicTypeSize) private var typeSize

        var body: some View {
            let large = typeSize.isAccessibilitySize
            let point = model.chosenPoint
            let count = model.curve?.drawable == true ? model.curve?.points.count ?? 0 : 0
            VStack(alignment: .leading, spacing: 8) {
                SpotHubPage.Heading(text: "Point", tag: .core)
                SpotHubPage.Card {
                    if model.changedElsewhere {
                        Banner(text: TxEqualizerModel.changedElsewhereText, ground: TxEqCurveParts.noticeGround,
                               colour: TxEqCurveParts.noticeText, large: large, identifier: "txEq.changedElsewhere")
                    }
                    picker(point: point, count: count, large: large)
                    StepRow(name: "Frequency", step: "10 Hz a step",
                            value: point.map { TxEqCurveParts.hz($0.frequencyHz) } ?? "--",
                            enabled: model.frequencyReason == nil && point != nil, large: large,
                            identifier: "txEq.pointFrequency",
                            down: { model.stepFrequency(up: false) }, up: { model.stepFrequency(up: true) },
                            open: { model.openPointFrequencyPad() })
                    if model.curveReason == nil, model.chosenIsEnd, let reason = model.frequencyReason {
                        SoftReason(text: reason, large: large, identifier: "txEq.endReason")
                    }
                    StepRow(name: "Gain", step: "0.5 dB a step",
                            value: point.map { TxEqCurveParts.db($0.gainDb) } ?? "--",
                            enabled: model.curveReason == nil && point != nil, large: large,
                            identifier: "txEq.pointGain",
                            down: { model.stepGain(up: false) }, up: { model.stepGain(up: true) },
                            open: { model.openGainPad() })
                    StepRow(name: "Q", step: "0.1 a step; higher is narrower",
                            value: point.flatMap { model.curve?.parametric == true ? TxEqCurveParts.q($0.q) : nil }
                                ?? "--",
                            enabled: model.qReason == nil && point != nil, large: large, identifier: "txEq.pointQ",
                            down: { model.stepQ(up: false) }, up: { model.stepQ(up: true) },
                            open: { model.openQPad() })
                    if model.curveReason == nil, model.curve?.parametric == false {
                        SoftReason(text: TxEqualizerModel.straightLinesQReason, large: large,
                                   identifier: "txEq.qReason")
                    }
                    if let refusal = model.curveNote {
                        Banner(text: refusal, ground: TxEqCurveParts.refusalGround, colour: TxEqCurveParts.refusalText,
                               large: large, identifier: "txEq.curveRefusal")
                        if model.chosenCount == nil {
                            Text(TxEqualizerModel.bandsWayText)
                                .font(.system(size: TxEqCurveParts.size(11.5, large: large)))
                                .foregroundStyle(TxEqCurveParts.hint)
                                .fixedSize(horizontal: false, vertical: true)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(.horizontal, 10)
                                .padding(.top, 5)
                                .padding(.bottom, 8)
                                .accessibilityIdentifier("txEq.bandsWay")
                        }
                    }
                    if let reason = model.curveReason {
                        ToolPageParts.Reason(text: reason, identifier: "txEq.pointReason")
                    }
                }
            }
        }

        @ViewBuilder
        private func picker(point: TxEqCurve.Point?, count: Int, large: Bool) -> some View {
            let title = Text(point == nil ? "No point" : "Point \(model.selected + 1) of \(count)")
                .font(.system(size: TxEqCurveParts.size(15, large: large), weight: .bold))
                .foregroundStyle(ChromeColours.textBright)
                .accessibilityIdentifier("txEq.pointTitle")
            let previous = PlainButton(title: "\u{2039} Previous", enabled: point != nil && model.selected > 0,
                                       large: large, identifier: "txEq.previous") { model.choosePrevious() }
            let next = PlainButton(title: "Next \u{203A}", enabled: point != nil && model.selected < count - 1,
                                   large: large, identifier: "txEq.next") { model.chooseNext() }
            Group {
                if large {
                    VStack(spacing: 8) {
                        title.frame(maxWidth: .infinity)
                        HStack(spacing: 8) {
                            previous.frame(maxWidth: .infinity)
                            next.frame(maxWidth: .infinity)
                        }
                    }
                } else {
                    HStack(spacing: 8) {
                        previous
                        title.frame(maxWidth: .infinity)
                        next
                    }
                }
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
        }
    }

    // MARK: The curve's values

    /// Shape, Bands, the curve preamp, Range, Reset and the list of points,
    /// each row of which chooses its point.
    struct ValuesCard: View {
        @ObservedObject var model: TxEqualizerModel
        @Environment(\.dynamicTypeSize) private var typeSize

        var body: some View {
            let large = typeSize.isAccessibilitySize
            let curve = model.curve?.drawable == true ? model.curve : nil
            let editable = model.curveReason == nil && curve != nil
            VStack(alignment: .leading, spacing: 8) {
                SpotHubPage.Heading(text: "Curve values", tag: .core)
                SpotHubPage.Card {
                    header("Shape", "Bells: each point is a bell as wide as its Q. Straight lines: lines join the points.",
                           large: large)
                    ToolPageParts.Choices(options: [(1, "Bells"), (0, "Straight lines")],
                                          selected: curve.map { $0.parametric ? 1 : 0 }, enabled: editable,
                                          identifier: "txEq.shape") { model.setShape(bells: $0 == 1) }
                        .padding(.horizontal, 10)
                        .padding(.bottom, 9)
                    SpotHubPage.Line()
                    header("Bands", "Resets per-band points: each goes to 0 dB and Q 4, spread evenly from Low to High.",
                           large: large)
                    ToolPageParts.Choices(options: TxEqualizerModel.curveCounts.map { (Int64($0), "\($0)-band") },
                                          selected: model.chosenCount.map(Int64.init), enabled: editable,
                                          identifier: "txEq.bands") { model.setBands(Int($0)) }
                        .padding(.horizontal, 10)
                        .padding(.bottom, 9)
                    if let curve, model.chosenCount == nil {
                        SoftReason(text: "This curve has \(curve.points.count) points, so none of the three is chosen.",
                                   large: large, identifier: "txEq.otherCount")
                    }
                    SpotHubPage.Line()
                    StepRow(name: "Curve preamp", step: "0.5 dB a step; not the ten-band Preamp",
                            value: curve.map { TxEqCurveParts.db($0.preampDb) } ?? "--", enabled: editable,
                            large: large, identifier: "txEq.curvePreamp",
                            down: { model.stepCurvePreamp(up: false) }, up: { model.stepCurvePreamp(up: true) },
                            open: { model.openCurvePreampPad() })
                    SpotHubPage.Line()
                    range(curve, editable: editable, large: large)
                    SpotHubPage.Line()
                    reset(large: large)
                    SpotHubPage.Line()
                    list(curve, large: large)
                    if let reason = model.curveReason {
                        ToolPageParts.Reason(text: reason, identifier: "txEq.valuesReason")
                    }
                }
            }
        }

        private func header(_ title: String, _ detail: String, large: Bool) -> some View {
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.system(size: TxEqCurveParts.size(14, large: large)))
                    .foregroundStyle(ChromeColours.text)
                Text(detail)
                    .font(.system(size: TxEqCurveParts.size(11, large: large)))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, 10)
            .padding(.top, 9)
            .padding(.bottom, 6)
        }

        private func range(_ curve: TxEqCurve?, editable: Bool, large: Bool) -> some View {
            let low = ValueField(text: curve.map { TxEqCurveParts.hz($0.minHz) } ?? "--", accessibility: "Low",
                                 disabled: !editable, minWidth: 76, grow: large) { model.openRangePad(low: true) }
                .accessibilityIdentifier("txEq.low")
            let high = ValueField(text: curve.map { TxEqCurveParts.hz($0.maxHz) } ?? "--", accessibility: "High",
                                  disabled: !editable, minWidth: 76, grow: large) { model.openRangePad(low: false) }
                .accessibilityIdentifier("txEq.high")
            let fields = HStack(spacing: 6) {
                low
                Text("to")
                    .font(.system(size: TxEqCurveParts.size(12, large: large)))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize()
                high
            }
            return StepLayout(large: large) {
                StepName(name: "Range", step: "Low and High, the curve\u{2019}s ends", large: large)
            } controls: {
                fields
            }
        }

        private func reset(large: Bool) -> some View {
            let detail = "Reset all parametric bands to a flat curve: every point to 0 dB and Q 4, spread evenly from "
                + "Low to High, and the curve preamp to 0 dB. Bands and Shape stay."
            return StepLayout(large: large) {
                StepName(name: "Reset", step: detail, large: large)
            } controls: {
                PlainButton(title: "Reset", enabled: model.resetReason == nil, large: large,
                            identifier: "txEq.reset") { model.reset() }
            }
        }

        private func list(_ curve: TxEqCurve?, large: Bool) -> some View {
            VStack(spacing: 0) {
                if !large {
                    row(["Point", "Frequency", "Gain", "Q"], head: true, large: large)
                }
                if let curve {
                    ForEach(Array(curve.points.enumerated()), id: \.offset) { index, point in
                        let chosen = index == model.selected
                        let q = curve.parametric ? TxEqCurveParts.q(point.q) : "--"
                        Button {
                            model.choose(index)
                        } label: {
                            Group {
                                if large {
                                    VStack(alignment: .leading, spacing: 2) {
                                        Text("Point \(index + 1)")
                                            .font(.system(size: TxEqCurveParts.size(13, large: large), weight: .bold))
                                        Text("\(TxEqCurveParts.hz(point.frequencyHz)) \u{00B7} "
                                             + "\(TxEqCurveParts.db(point.gainDb)) \u{00B7} Q \(q)")
                                            .font(.system(size: TxEqCurveParts.size(13, large: large)).monospacedDigit())
                                            .fixedSize(horizontal: false, vertical: true)
                                    }
                                    .frame(maxWidth: .infinity, alignment: .leading)
                                    .padding(.vertical, 6)
                                } else {
                                    row(["\(index + 1)", TxEqCurveParts.hz(point.frequencyHz),
                                         TxEqCurveParts.db(point.gainDb), q], head: false, large: large)
                                }
                            }
                            .foregroundStyle(chosen ? Color.white : TxEqCurveParts.rowText)
                            .padding(.horizontal, 6)
                            .frame(minHeight: 38)
                            .background(chosen ? TxEqCurveParts.rowChosen : Color.clear)
                            .overlay(alignment: .leading) {
                                Rectangle().fill(chosen ? TxEqCurveParts.line : Color.clear).frame(width: 3)
                            }
                            .overlay(alignment: .top) {
                                Rectangle().fill(index == 0 ? Color.clear : TxEqCurveParts.rowLine).frame(height: 1)
                            }
                            .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain)
                        .accessibilityElement(children: .ignore)
                        .accessibilityLabel(TxEqCurveParts.markLabel(index, point) + (curve.parametric ? ", Q \(q)" : ""))
                        .accessibilityAddTraits(chosen ? .isSelected : [])
                        .accessibilityIdentifier("txEq.row\(index)")
                    }
                } else {
                    row(["--", "--", "--", "--"], head: false, large: large)
                        .foregroundStyle(ChromeColours.buttonOffText)
                        .padding(.horizontal, 6)
                }
            }
            .padding(.horizontal, 6)
            .padding(.top, 4)
            .padding(.bottom, 6)
        }

        private func row(_ cells: [String], head: Bool, large: Bool) -> some View {
            HStack(spacing: 4) {
                Text(cells[0]).frame(width: 44, alignment: .leading)
                Text(cells[1]).frame(maxWidth: .infinity, alignment: .trailing)
                Text(cells[2]).frame(maxWidth: .infinity, alignment: .trailing)
                Text(cells[3]).frame(width: 48, alignment: .trailing)
            }
            .font(head ? .system(size: 11, weight: .bold) : .system(size: 13, weight: .medium).monospacedDigit())
            .foregroundStyle(head ? TxEqCurveParts.rowHead : TxEqCurveParts.rowText)
            .lineLimit(1)
            .minimumScaleFactor(0.8)
            .frame(minHeight: head ? 26 : 38)
            .padding(.horizontal, head ? 6 : 0)
        }
    }

    // MARK: Small parts

    /// A value's name with its step under it.
    struct StepName: View {
        let name: String
        let step: String
        let large: Bool

        var body: some View {
            VStack(alignment: .leading, spacing: 1) {
                Text(name)
                    .font(.system(size: TxEqCurveParts.size(14.5, large: large), weight: .medium))
                    .foregroundStyle(ChromeColours.text)
                Text(step)
                    .font(.system(size: TxEqCurveParts.size(11, large: large)))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    /// A name beside its controls, or, at a large text size, above them at full width.
    struct StepLayout<Name: View, Controls: View>: View {
        let large: Bool
        @ViewBuilder var name: () -> Name
        @ViewBuilder var controls: () -> Controls

        var body: some View {
            Group {
                if large {
                    VStack(alignment: .leading, spacing: 6) {
                        name()
                        controls().frame(maxWidth: .infinity, alignment: .leading)
                    }
                } else {
                    HStack(spacing: 8) {
                        name()
                        controls()
                    }
                }
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 7)
        }
    }

    /// A value with − and + and a field for the number pad between them.
    struct StepRow: View {
        let name: String
        let step: String
        let value: String
        let enabled: Bool
        let large: Bool
        let identifier: String
        let down: () -> Void
        let up: () -> Void
        let open: () -> Void

        var body: some View {
            StepLayout(large: large) {
                StepName(name: name, step: step, large: large)
            } controls: {
                HStack(spacing: 6) {
                    StepButton(symbol: "\u{2212}", label: "\(name) down", enabled: enabled, large: large,
                               identifier: "\(identifier).down", action: down)
                    ValueField(text: value, accessibility: name, disabled: !enabled, minWidth: 84, grow: large,
                               open: open)
                        .frame(minHeight: large ? 44 : 34)
                        .accessibilityIdentifier(identifier)
                    StepButton(symbol: "+", label: "\(name) up", enabled: enabled, large: large,
                               identifier: "\(identifier).up", action: up)
                }
            }
        }
    }

    /// A − or + button.
    struct StepButton: View {
        let symbol: String
        let label: String
        let enabled: Bool
        let large: Bool
        let identifier: String
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                Text(symbol)
                    .font(.system(size: large ? 22 : 18, weight: .semibold))
                    .foregroundStyle(enabled ? ChromeColours.textBright : ChromeColours.buttonOffText)
                    .frame(width: large ? 48 : 38, height: large ? 44 : 34)
                    .background(enabled ? ChromeColours.button : ChromeColours.buttonOff,
                                in: RoundedRectangle(cornerRadius: 5))
                    .overlay(RoundedRectangle(cornerRadius: 5)
                        .strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder,
                                      lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityLabel(label)
            .accessibilityIdentifier(identifier)
        }
    }

    /// A labelled button in the page's button style.
    struct PlainButton: View {
        let title: String
        let enabled: Bool
        let large: Bool
        let identifier: String
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                Text(title)
                    .font(.system(size: large ? 16 : 13, weight: .semibold))
                    .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                    .lineLimit(1)
                    .padding(.horizontal, 12)
                    .frame(maxWidth: large ? .infinity : nil, minHeight: large ? 44 : 34)
                    .background(enabled ? ChromeColours.button : ChromeColours.buttonOff,
                                in: RoundedRectangle(cornerRadius: 5))
                    .overlay(RoundedRectangle(cornerRadius: 5)
                        .strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder,
                                      lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityIdentifier(identifier)
        }
    }

    /// A reason in the soft yellow of a control's own rule.
    struct SoftReason: View {
        let text: String
        let large: Bool
        let identifier: String

        var body: some View {
            Text(text)
                .font(.system(size: TxEqCurveParts.size(11.5, large: large)))
                .foregroundStyle(TxEqCurveParts.softReason)
                .fixedSize(horizontal: false, vertical: true)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 10)
                .padding(.top, 2)
                .padding(.bottom, 6)
                .accessibilityIdentifier(identifier)
        }
    }

    /// A notice or the Core's refusal across the card.
    struct Banner: View {
        let text: String
        let ground: Color
        let colour: Color
        let large: Bool
        let identifier: String

        var body: some View {
            Text(text)
                .font(.system(size: TxEqCurveParts.size(12.5, large: large)))
                .foregroundStyle(colour)
                .fixedSize(horizontal: false, vertical: true)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 10)
                .padding(.vertical, 8)
                .background(ground)
                .accessibilityIdentifier(identifier)
        }
    }
}
