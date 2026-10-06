// NereusSDR for iOS: the flag's signal level: an S-meter bar on the Core's scale, with the level printed beside it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusModels
import SwiftUI

/// The flag's signal level, as the board draws it: the S-unit marks above
/// a bar, the bar filled to the slice's signal (blue up to S9, green past
/// it) and the level beside it, printed in the Multimeter page's units
/// with or without the decimal point (``SMeterReadout``). The scale and its
/// marks are the Core's (the catalogue's S-meter); the app writes none of
/// its own, and they do not change with the units.
struct FlagLevelBar: View {
    let dbm: Double?
    let meter: StationCatalog.Meters.SMeter?
    var readout: SMeterReadout = .desktopDefaults

    /// The level's text width beside the bar.
    static let dbmWidth: CGFloat = 52
    static let tickHeight: CGFloat = 8
    static let top: CGFloat = 4

    var body: some View {
        Canvas { context, size in
            draw(in: &context, size: size)
        }
        .accessibilityElement()
        .accessibilityLabel("Signal")
        .accessibilityValue(dbm.flatMap { readout.spoken(dbm: $0, meter: meter) } ?? "No signal reading")
    }

    /// The marks the flag labels: every other S-unit from S1 (the first
    /// with its S), then every 20 dB over S9.
    static func marks(_ meter: StationCatalog.Meters.SMeter) -> [(label: String, dbm: Double)] {
        var marks: [(String, Double)] = []
        for mark in meter.sUnits {
            guard let unit = Int(mark.label.dropFirst()), unit % 2 == 1 else {
                continue
            }
            marks.append((marks.isEmpty ? mark.label : String(mark.label.dropFirst()), mark.dbm))
        }
        for mark in meter.overS9 {
            let over = mark.dbm - meter.s9Dbm
            if over > 0, Int(over.rounded()) % 20 == 0, mark.dbm < meter.maxDbm {
                marks.append((mark.label, mark.dbm))
            }
        }
        return marks
    }

    private func draw(in context: inout GraphicsContext, size: CGSize) {
        let barWidth = size.width - Self.dbmWidth - 2
        let barTop = Self.top + Self.tickHeight
        let barHeight = size.height - barTop
        guard barWidth > 4, barHeight > 2 else {
            return
        }
        let floor = meter?.minDbm ?? 0
        let ceiling = meter?.maxDbm ?? 1
        func fraction(_ value: Double) -> Double {
            ceiling > floor ? min(max((value - floor) / (ceiling - floor), 0), 1) : 0
        }
        if let meter {
            let marks = Self.marks(meter)
            for (index, mark) in marks.enumerated() {
                let x = (fraction(mark.dbm) * Double(barWidth - 1)).rounded(.down)
                context.fill(Path(CGRect(x: x, y: Self.top + Self.tickHeight - 2, width: 1, height: 3)),
                             with: .color(BandColours.levelTicks))
                let anchor: UnitPoint = index == 0 ? .leading : (mark.dbm > meter.s9Dbm ? .trailing : .center)
                context.draw(Text(mark.label).font(.system(size: 8)).foregroundStyle(BandColours.levelTicks),
                             at: CGPoint(x: x, y: Self.top + (Self.tickHeight - 2) / 2), anchor: anchor)
            }
        }
        let bar = CGRect(x: 0, y: barTop, width: barWidth, height: barHeight)
        context.fill(Path(bar), with: .color(BandColours.levelBackground))
        context.stroke(Path(bar.insetBy(dx: 0.5, dy: 0.5)), with: .color(BandColours.levelBorder), lineWidth: 1)
        guard let dbm, let meter else {
            return
        }
        let inner = barWidth - 2
        let filled = (fraction(dbm) * Double(inner)).rounded(.down)
        if filled > 0 {
            let s9 = fraction(meter.s9Dbm)
            let gradient = Gradient(stops: [.init(color: BandColours.levelLow, location: 0),
                                            .init(color: BandColours.levelLow, location: s9),
                                            .init(color: BandColours.levelHigh, location: min(s9 + 0.15, 1)),
                                            .init(color: BandColours.levelHigh, location: 1)])
            context.fill(Path(CGRect(x: 1, y: barTop + 1, width: filled, height: barHeight - 2)),
                         with: .linearGradient(gradient, startPoint: CGPoint(x: 1, y: 0),
                                               endPoint: CGPoint(x: 1 + inner, y: 0)))
        }
        let colour = dbm >= meter.s9Dbm ? BandColours.levelHigh : BandColours.levelLow
        guard let printed = readout.text(dbm: dbm, meter: meter) else {
            return
        }
        // A long reading (a strong signal in microvolts) shrinks to fit
        // beside the bar rather than run off the flag.
        var level = context.resolve(Self.levelText(printed, size: Self.levelFontSize).foregroundStyle(colour))
        let width = level.measure(in: CGSize(width: CGFloat.greatestFiniteMagnitude, height: size.height)).width
        if width > Self.dbmWidth {
            let fitted = max(Self.levelFontSize * Self.dbmWidth / width, Self.smallestLevelFontSize)
            level = context.resolve(Self.levelText(printed, size: fitted).foregroundStyle(colour))
        }
        context.draw(level, at: CGPoint(x: barWidth + 3, y: barTop + barHeight / 2), anchor: .leading)
    }

    static let levelFontSize: CGFloat = 10
    static let smallestLevelFontSize: CGFloat = 7

    private static func levelText(_ text: String, size: CGFloat) -> Text {
        Text(text).font(.system(size: size, weight: .bold))
    }
}
