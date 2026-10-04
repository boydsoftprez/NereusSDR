// NereusSDR for iOS: the TX panel's transmit stage meters: the Core's seven stage readings as one strip under Mic level
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// One of the seven transmit stage readings the Core sends on `txState`
/// at `txReadingsVersion` 3 (JJ, 2026-09-28): its name, its property, and
/// its bar's scale as the desktop's bar draws it. The bar is linear from
/// `low` to `mid` over the first `midAt` of its length, then from `mid` to
/// `high` up to `highAt`; above `mid` it is red.
struct TxStage: Equatable, Identifiable {
    let name: String
    let property: String
    let low: Double
    let mid: Double
    let high: Double
    /// Where `mid` and `high` sit along the bar, 0 to 1.
    let midAt: Double
    let highAt: Double

    var id: String { property }

    /// The seven, in the order the sound passes through the transmitter.
    static let all: [TxStage] = [
        TxStage(name: "EQ", property: "eqDb", low: -30, mid: 0, high: 12, midAt: 0.665, highAt: 0.99),
        TxStage(name: "Leveler", property: "levelerDb", low: -30, mid: 0, high: 12, midAt: 0.665, highAt: 0.99),
        TxStage(name: "Leveler Gain", property: "levelerGainDb", low: 0, mid: 10, high: 30, midAt: 0.333,
                highAt: 0.99),
        TxStage(name: "CFC", property: "cfcDb", low: -30, mid: 0, high: 12, midAt: 0.665, highAt: 0.99),
        TxStage(name: "CFC Gain", property: "cfcGainDb", low: 0, mid: 10, high: 30, midAt: 0.333, highAt: 0.99),
        TxStage(name: "ALC Gain", property: "alcGainDb", low: 0, mid: 10, high: 30, midAt: 0.333, highAt: 0.99),
        TxStage(name: "ALC Group", property: "alcGroupDb", low: -30, mid: 0, high: 25, midAt: 0.545, highAt: 0.99),
    ]

    /// No reading for any stage.
    static let empty: [Double?] = Array(repeating: nil, count: all.count)
    /// The Core's "no reading" (it has no transmit channel): shown as "--".
    static let noReading = -400.0
    /// The capability version that carries the seven.
    static let readingsVersion: Int64 = 3
    /// How far a recent peak falls towards the reading with each new
    /// reading below it: a tenth of the way, so at ten readings a second a
    /// peak halves its lead in about two thirds of a second.
    static let peakFall = 0.1

    /// Where `value` sits along the bar, 0 to 1.
    func position(_ value: Double) -> Double {
        let clamped = min(max(value, low), high)
        let along = clamped <= mid
            ? (clamped - low) / (mid - low) * midAt
            : midAt + (clamped - mid) / (high - mid) * (highAt - midAt)
        return min(max(along, 0), 1)
    }

    /// A reading from the Core: nil for its "no reading".
    static func reading(_ value: Double?) -> Double? {
        guard let value, value.isFinite, value > noReading else {
            return nil
        }
        return value
    }

    /// The recent peaks after a new set of readings: a reading at or above
    /// its peak lifts the peak to it; below, the peak falls towards it
    /// (``peakFall``); a stage with no reading has no peak.
    static func peaks(_ previous: [Double?], _ readings: [Double?]) -> [Double?] {
        readings.enumerated().map { index, reading in
            guard let reading else {
                return nil
            }
            guard index < previous.count, let peak = previous[index], peak > reading else {
                return reading
            }
            return peak + (reading - peak) * peakFall
        }
    }

    /// The stage at `index` in a list of readings or peaks, nil past its end.
    static func at(_ list: [Double?], _ index: Int) -> Double? {
        index < list.count ? list[index] : nil
    }

    /// "-4.2 dB", or "--" with no reading.
    static func readout(_ value: Double?) -> String {
        guard let value else {
            return "--"
        }
        let tenths = (value * 10).rounded() / 10
        return String(format: "%.1f dB", tenths == 0 ? 0 : tenths)
    }

    /// A scale mark: "+12", "0", "-30".
    static func mark(_ value: Double) -> String {
        let whole = Int(value.rounded())
        return whole > 0 ? "+\(whole)" : "\(whole)"
    }
}

/// The transmit stage meters (JJ, 2026-09-28): the Core's seven stage
/// readings as one strip in the TX panel, directly under Mic level. Each
/// row has the stage's name, a bar on its own scale with a thin white mark
/// at the recent peak (kept by the phone from the readings while the radio
/// is on the air), and the reading. Off the air the last readings stay,
/// dimmed, with a line saying so; a Core that sends none shows the reason
/// once above seven greyed bars. Nothing here can be tapped.
struct TxStageMeters: View {
    @ObservedObject var transmit: TransmitModel
    @Environment(\.dynamicTypeSize) private var textSize

    /// What the strip shows.
    enum Look: Equatable {
        /// The Core does not send the readings (`txReadingsVersion` below 3).
        case olderCore
        /// The Core sends "no reading" for all seven.
        case noReadings
        /// The radio is on the air: the readings move, with their peaks.
        case onAir
        /// Off the air: the last readings the Core sent, dimmed.
        case offAir
    }

    static let title = "Transmit stages"
    static let olderCoreText = "This Core does not send these readings. Updating the Core may help."
    static let noReadingsText = "The Core has no transmit readings right now."
    static let offAirText =
        "Not transmitting. These are the last readings the Core sent; they move again when the radio transmits."

    static func look(_ transmit: TransmitModel) -> Look {
        if transmit.txReadingsVersion < TxStage.readingsVersion {
            return .olderCore
        }
        if transmit.stageReadings.allSatisfy({ $0 == nil }) {
            return .noReadings
        }
        return transmit.stagesOnAir ? .onAir : .offAir
    }

    /// The line over the bars, if any.
    static func line(_ look: Look) -> String? {
        switch look {
        case .olderCore: olderCoreText
        case .noReadings: noReadingsText
        case .offAir: offAirText
        case .onAir: nil
        }
    }

    var body: some View {
        let look = Self.look(transmit)
        let large = textSize.isAccessibilitySize
        VStack(alignment: .leading, spacing: large ? 8 : 5) {
            HStack(alignment: .firstTextBaseline) {
                Text(Self.title)
                    .font(.system(size: large ? 16 : 11))
                    .foregroundStyle(ChromeColours.caption)
                Spacer(minLength: 0)
                Text("dB")
                    .font(.system(size: large ? 15 : 10))
                    .foregroundStyle(ChromeColours.caption)
                    .accessibilityHidden(true)
            }
            if look == .olderCore {
                Text(Self.olderCoreText)
                    .font(.system(size: large ? 17 : 11.5))
                    .foregroundStyle(Self.reasonText)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 7)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(Self.reasonFill, in: RoundedRectangle(cornerRadius: 6))
                    .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(Self.reasonBorder, lineWidth: 1))
                    .accessibilityIdentifier("txStagesReason")
            } else if let line = Self.line(look) {
                Text(line)
                    .font(.system(size: large ? 15 : 11))
                    .foregroundStyle(Self.noteText)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("txStagesNote")
            }
            VStack(alignment: .leading, spacing: large ? 8 : 3) {
                ForEach(Array(TxStage.all.enumerated()), id: \.element.id) { index, stage in
                    let reading = look == .olderCore ? nil : TxStage.at(transmit.stageReadings, index)
                    let peak = look == .onAir ? TxStage.at(transmit.stagePeaks, index) : nil
                    TxStageRow(stage: stage, reading: reading, peak: peak, dimmed: look == .offAir, large: large)
                }
            }
            .opacity(look == .olderCore ? 0.45 : 1)
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Transmit stage meters")
        .accessibilityIdentifier("txStageMeters")
    }

    static let reasonFill = rgb(0x14, 0x1C, 0x28)
    static let reasonBorder = rgb(0x30, 0x40, 0x50)
    static let reasonText = rgb(0xD4, 0xE2, 0xEE)
    static let noteText = rgb(0x90, 0xA4, 0xB6)

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}

/// One stage: its name, its bar with the scale's marks under it, and its reading.
private struct TxStageRow: View {
    let stage: TxStage
    let reading: Double?
    let peak: Double?
    let dimmed: Bool
    let large: Bool

    var body: some View {
        Group {
            if large {
                VStack(alignment: .leading, spacing: 3) {
                    HStack(alignment: .firstTextBaseline) {
                        name
                        Spacer(minLength: 8)
                        value
                    }
                    bar
                }
            } else {
                HStack(alignment: .top, spacing: 6) {
                    name
                        .frame(width: 74, height: 12, alignment: .leading)
                    bar
                    value
                        .frame(width: 54, height: 12, alignment: .trailing)
                }
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(stage.name)
        .accessibilityValue(reading.map { _ in TxStage.readout(reading) } ?? "No reading")
        .accessibilityIdentifier("txStage-\(stage.property)")
    }

    private var name: some View {
        Text(stage.name)
            .font(.system(size: large ? 17 : 11, weight: .semibold))
            .foregroundStyle(TxStageRow.nameColour)
            .lineLimit(large ? nil : 1)
            .fixedSize(horizontal: !large, vertical: large)
    }

    private var value: some View {
        Text(TxStage.readout(reading))
            .font(.system(size: large ? 19 : 12, weight: .semibold).monospacedDigit())
            .foregroundStyle(reading == nil ? ChromeColours.textDim
                             : dimmed ? TxStageRow.dimmedValue : ChromeColours.textBright)
            .lineLimit(1)
            .fixedSize()
    }

    private var bar: some View {
        VStack(spacing: 0) {
            Canvas { context, size in
                drawTrack(in: &context, size: size)
            }
            .frame(height: large ? 18 : 12)
            Canvas { context, size in
                drawMarks(in: &context, size: size)
            }
            .frame(height: large ? 16 : 11)
        }
        .frame(maxWidth: .infinity)
    }

    private func drawTrack(in context: inout GraphicsContext, size: CGSize) {
        let track = CGRect(origin: .zero, size: size)
        let midX = track.width * stage.midAt
        let shape = Path(roundedRect: track, cornerRadius: 2)
        context.fill(shape, with: .color(ChromeColours.gaugeGround))
        var red = context
        red.clip(to: shape)
        red.fill(Path(CGRect(x: midX, y: 0, width: track.width - midX, height: track.height)),
                 with: .color(TxStageRow.redGround))
        context.stroke(Path(roundedRect: track.insetBy(dx: 0.5, dy: 0.5), cornerRadius: 2),
                       with: .color(ChromeColours.gaugeBorder), lineWidth: 1)
        let inner = track.insetBy(dx: 1, dy: 1)
        if let reading {
            let fill = inner.width * stage.position(reading)
            let innerMid = inner.width * stage.midAt
            var bars = context
            bars.opacity = dimmed ? 0.45 : 1
            let normal = min(fill, innerMid)
            if normal > 0 {
                bars.fill(Path(CGRect(x: inner.minX, y: inner.minY, width: normal, height: inner.height)),
                          with: .color(ChromeColours.gaugeNormal))
            }
            if fill > innerMid {
                bars.fill(Path(CGRect(x: inner.minX + innerMid, y: inner.minY, width: fill - innerMid,
                                      height: inner.height)),
                          with: .color(ChromeColours.gaugeRed))
            }
        }
        if let peak {
            let x = inner.minX + inner.width * stage.position(peak)
            context.fill(Path(CGRect(x: x - 1, y: inner.minY, width: 2, height: inner.height)), with: .color(.white))
        }
    }

    private func drawMarks(in context: inout GraphicsContext, size: CGSize) {
        let marks: [(Double, Double, UnitPoint)] = [(stage.low, 0, .topLeading), (stage.mid, stage.midAt, .top),
                                                     (stage.high, stage.highAt, .topTrailing)]
        for (value, along, anchor) in marks {
            let x = size.width * along
            context.draw(Text(TxStage.mark(value)).font(.system(size: large ? 13 : 9).monospacedDigit())
                            .foregroundStyle(ChromeColours.textFaint),
                         at: CGPoint(x: x, y: 0), anchor: anchor)
        }
    }

    static let nameColour = Color(red: 0xB0 / 255.0, green: 0xC0 / 255.0, blue: 0xD0 / 255.0)
    static let dimmedValue = Color(red: 0xA8 / 255.0, green: 0xB8 / 255.0, blue: 0xC8 / 255.0)
    static let redGround = Color(red: 0x1E / 255.0, green: 0x0E / 255.0, blue: 0x12 / 255.0)
}
