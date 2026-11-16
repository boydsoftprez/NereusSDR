// NereusSDR for iOS: the Digital Spectrum Recorder's time strip, LIVE, SAVE, the clock and the past picture on the pan
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 28, the phone's and iPad's Digital Spectrum
//   Recorder on the pan. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Recording plan fix round: SAVE's handles move by a step for
//   VoiceOver, the hours are asked as the recorder changes, the play line
//   shows only for this device's own rewind. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import CoreGraphics
import NereusBand
import NereusModels
import SwiftUI

/// Where the recorder's controls sit on the waterfall (the phone mockup):
/// the 44-point time strip at the right edge, the 14-point hours map just
/// left of it, LIVE and SAVE at the strip's top, the clock at its foot.
struct DsrStripPlace: Equatable {
    static let stripWidth: CGFloat = 44
    static let mapWidth: CGFloat = 14
    static let buttonSize = CGSize(width: 38, height: 22)
    static let clockHeight: CGFloat = 26
    static let handleSize: CGFloat = 30

    let waterfall: CGRect
    let strip: CGRect
    let map: CGRect

    /// The gap kept above the band's own controls at the waterfall's foot.
    static let controlsGap: CGFloat = 6
    /// The strip never gets shorter than this to keep clear of them.
    static let shortest: CGFloat = 120

    /// `avoid`: the band's own controls (zoom, the dial, PTT); the strip
    /// and the map end above any that sit under them.
    init(waterfall: CGRect, trailingInset: CGFloat, avoid: [CGRect] = []) {
        self.waterfall = waterfall
        let right = waterfall.maxX - trailingInset
        let left = right - Self.stripWidth - Self.mapWidth
        let under = avoid.filter { frame in
            frame.maxX > left && frame.minX < right && frame.minY < waterfall.maxY
                && frame.minY > waterfall.minY + Self.shortest
        }
        let foot = under.map { $0.minY - Self.controlsGap }.min() ?? waterfall.maxY
        let height = max(0, foot - waterfall.minY)
        strip = CGRect(x: right - Self.stripWidth, y: waterfall.minY, width: Self.stripWidth, height: height)
        map = CGRect(x: strip.minX - Self.mapWidth, y: waterfall.minY, width: Self.mapWidth, height: height)
    }

    var live: CGRect {
        CGRect(origin: CGPoint(x: strip.minX + 3, y: strip.minY + 5), size: Self.buttonSize)
    }

    var save: CGRect {
        CGRect(origin: CGPoint(x: strip.minX + 3, y: strip.minY + 31), size: Self.buttonSize)
    }

    var clock: CGRect {
        CGRect(x: strip.minX + 3, y: strip.maxY - Self.clockHeight - 4, width: Self.buttonSize.width,
               height: Self.clockHeight)
    }

    /// The picture left of the map, where "Drawing" dims.
    var picture: CGRect {
        CGRect(x: waterfall.minX, y: waterfall.minY, width: max(0, map.minX - waterfall.minX), height: waterfall.height)
    }
}

/// The strip, the map, LIVE, SAVE, the clock and SAVE's handles, over the
/// band's drag: a drag on the strip moves the picture, never the band.
struct DsrStripLayer: View {
    let model: DsrPanModel
    let place: DsrStripPlace
    /// Seconds a waterfall point stands for: the pan's Update period.
    var secondsPerPoint: Double = 0.1
    @State private var dragStart: Double?
    @State private var mapFinger: CGFloat?

    var body: some View {
        let usable = model.usable
        ZStack(alignment: .topLeading) {
            DsrHoursMap(model: model, size: place.map.size)
                .frame(width: place.map.width, height: place.map.height)
                .contentShape(Rectangle())
                .gesture(DragGesture(minimumDistance: 0)
                    .onChanged { value in
                        mapFinger = value.location.y
                        model.jump(toMapFraction: value.location.y / max(1, place.map.height), commit: false)
                    }
                    .onEnded { value in
                        mapFinger = nil
                        model.jump(toMapFraction: value.location.y / max(1, place.map.height), commit: true)
                    })
                .accessibilityElement()
                .accessibilityIdentifier("dsr.map")
                .accessibilityLabel("Hours kept")
                .accessibilityHint(model.reason ?? "Jumps the picture to that hour")
                .offset(x: place.map.minX, y: place.map.minY)
            strip
                .offset(x: place.strip.minX, y: place.strip.minY)
            if let mapFinger, usable, let label = model.mapLabel(fraction: mapFinger / max(1, place.map.height)) {
                Text(label)
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundStyle(.white)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 3)
                    .background(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).fill(Color(white: 0.09, opacity: 0.95)))
                    .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).stroke(Color(white: 0.33)))
                    .fixedSize()
                    .frame(width: 220, alignment: .trailing)
                    .offset(x: place.map.minX - 6 - 220, y: place.map.minY + mapFinger - 11)
                    .allowsHitTesting(false)
            }
            buttons(usable: usable)
            if model.saving {
                handles
            }
        }
        .coordinateSpace(.named(Self.space))
        .onChange(of: model.client.dsr?.rewound) {
            model.documentChanged()
        }
        .onChange(of: place.waterfall.height, initial: true) { _, height in
            model.waterfallPoints = Double(height)
        }
        .onChange(of: secondsPerPoint, initial: true) { _, seconds in
            model.secondsPerPoint = max(seconds, 0.001)
        }
        .task {
            while !Task.isCancelled {
                model.tick()
                try? await Task.sleep(for: .milliseconds(250))
            }
        }
    }

    // MARK: The strip

    private var strip: some View {
        Canvas { context, size in
            Self.drawStrip(context, size: size, model: model)
        }
        .frame(width: place.strip.width, height: place.strip.height)
        .contentShape(Rectangle())
        .gesture(DragGesture(minimumDistance: 2)
            .onChanged { value in
                let start = dragStart ?? model.top
                dragStart = start
                model.drag(from: start, by: value.translation.height)
            }
            .onEnded { _ in
                dragStart = nil
                model.dragEnded()
            })
        .accessibilityElement()
        .accessibilityIdentifier("dsr.strip")
        .accessibilityLabel("Time strip")
        .accessibilityValue(model.viewTop.map { DsrPanModel.hms($0) + " UTC" } ?? "Live")
        .accessibilityHint("Drag up to go back, down to come forward")
        .accessibilityAdjustableAction { direction in
            let step = model.pictureSeconds / 2 / max(model.secondsPerPoint, 0.001)
            model.drag(from: model.top, by: direction == .increment ? step : -step)
            model.dragEnded()
        }
    }

    /// The strip's ticks and times: a tick each step, the time on every
    /// minute (or every other tick when the picture is short), the moments
    /// you transmitted as a red edge and SAVE's bracket in yellow.
    static func drawStrip(_ context: GraphicsContext, size: CGSize, model: DsrPanModel) {
        let past = model.inPast
        context.fill(Path(CGRect(origin: .zero, size: size)), with: .color(Color(red: 12 / 255, green: 13 / 255,
                                                                                 blue: 20 / 255, opacity: 0.86)))
        context.fill(Path(CGRect(x: 0, y: 0, width: 1, height: size.height)), with: .color(Color(white: 0.17)))
        let spp = max(model.secondsPerPoint, 0.001)
        let top = model.top
        let bottom = top - Double(size.height) * spp
        let steps: [Double] = [1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600]
        let step = steps.first { $0 / spp >= 30 } ?? 3600
        let labelEvery = step / spp >= 60 ? step : step * 2
        var moment = (top / step).rounded(.down) * step
        while moment > bottom {
            let y = (top - moment) / spp
            let full = (moment / labelEvery).rounded() * labelEvery == moment
            context.fill(Path(CGRect(x: 1, y: y, width: full ? 7 : 4, height: 1)),
                         with: .color(full ? Color(white: 0.6) : Color(white: 0.33)))
            if full, y > 16, y < Double(size.height) - 34 {
                let text = labelEvery >= 60 ? DsrPanModel.hm(moment) : ":" + String(DsrPanModel.hms(moment).suffix(2))
                context.draw(Text(text)
                    .font(.system(size: 10, design: .monospaced))
                    .foregroundStyle(past ? Color(red: 1, green: 0.82, blue: 0.63) : Color(white: 0.78)),
                             at: CGPoint(x: size.width - 3, y: y), anchor: .trailing)
            }
            moment -= step
        }
        // Your transmit, as the Core marks its rows.
        if model.coreDraws {
            for y in stride(from: 0, to: Int(size.height), by: 1) {
                if model.row(at: top - Double(y) * spp)?.transmit == true {
                    context.fill(Path(CGRect(x: 0, y: Double(y), width: 3, height: 1)), with: .color(.red))
                }
            }
        }
        if let stretch = model.stretch {
            let low = max(0, (top - stretch.to) / spp)
            let high = min(Double(size.height), (top - stretch.from) / spp)
            if high > low {
                let rect = CGRect(x: 0, y: low, width: size.width, height: high - low)
                context.fill(Path(rect), with: .color(DsrColours.yellow.opacity(0.18)))
                context.fill(Path(CGRect(x: 0, y: low, width: 3, height: high - low)), with: .color(DsrColours.yellow))
            }
        }
    }

    // MARK: LIVE, SAVE and the clock

    private func buttons(usable: Bool) -> some View {
        ZStack(alignment: .topLeading) {
            Button {
                Task { await model.goLive() }
            } label: {
                DsrButtonLabel(text: "LIVE", look: model.liveLit ? .red : .plain, size: DsrStripPlace.buttonSize)
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier("dsr.live")
            .accessibilityLabel("Live")
            .accessibilityHint(model.liveLit ? "Back to live" : "Already live")
            .offset(x: place.live.minX, y: place.live.minY)
            Button {
                model.toggleSave()
            } label: {
                DsrButtonLabel(text: "SAVE", look: model.saving ? .yellowOn : .yellow, size: DsrStripPlace.buttonSize)
                    .opacity(usable ? 1 : 0.42)
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier("dsr.save")
            .accessibilityLabel("Save a stretch")
            .accessibilityHint(model.reason ?? "Puts a bracket on the time strip")
            .offset(x: place.save.minX, y: place.save.minY)
            Button {
                model.openTimeSheet()
            } label: {
                DsrClockGlyph()
                    .frame(width: DsrStripPlace.buttonSize.width, height: DsrStripPlace.clockHeight)
                    .background(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).fill(Color(white: 0.165)))
                    .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).stroke(Color(white: 0.33)))
                    .opacity(usable ? 1 : 0.42)
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier("dsr.clock")
            .accessibilityLabel("Go to a time")
            .accessibilityHint(model.reason ?? "Type a time to go to")
            .offset(x: place.clock.minX, y: place.clock.minY)
        }
    }

    // MARK: SAVE's handles

    private var handles: some View {
        ZStack(alignment: .topLeading) {
            ForEach(0..<2, id: \.self) { which in
                if let y = handleY(which) {
                    Circle()
                        .fill(DsrColours.yellow)
                        .overlay(Circle().stroke(Color(white: 0.07), lineWidth: 2))
                        .overlay(VStack(spacing: 2) {
                            Rectangle().frame(width: 14, height: 2)
                            Rectangle().frame(width: 14, height: 2)
                        }
                        .foregroundStyle(Color(white: 0.07)))
                        .shadow(color: .black.opacity(0.6), radius: 2, y: 1)
                        .frame(width: DsrStripPlace.handleSize, height: DsrStripPlace.handleSize)
                        .contentShape(Circle())
                        .gesture(DragGesture(minimumDistance: 0, coordinateSpace: .named(DsrStripLayer.space))
                            .onChanged { value in
                                model.moveHandle(which, toPointsDown: value.location.y - place.waterfall.minY)
                            })
                        .accessibilityElement()
                        .accessibilityIdentifier("dsr.handle\(which)")
                        .accessibilityLabel(which == 0 ? "Bracket end" : "Bracket start")
                        .accessibilityValue(handleMoment(which).map { DsrPanModel.hms($0) + " UTC" } ?? "")
                        .accessibilityHint("Adjust to move it later or earlier")
                        .accessibilityAdjustableAction { direction in
                            model.stepHandle(which, later: direction == .increment)
                        }
                        .offset(x: place.strip.minX + 8, y: y - DsrStripPlace.handleSize / 2)
                }
            }
        }
    }

    static let space = "dsrStrip"

    private func handleMoment(_ which: Int) -> Double? {
        model.bracket.map { which == 0 ? $0.0 : $0.1 }
    }

    private func handleY(_ which: Int) -> CGFloat? {
        guard let bracket = model.bracket else {
            return nil
        }
        let moment = which == 0 ? bracket.0 : bracket.1
        let y = place.waterfall.minY + (model.top - moment) / max(model.secondsPerPoint, 0.001)
        guard y >= place.waterfall.minY - 2, y <= place.waterfall.maxY + 2 else {
            return nil
        }
        return y
    }
}

/// The recorder's colours, from the mockups.
enum DsrColours {
    static let yellow = Color(red: 232 / 255, green: 192 / 255, blue: 42 / 255)
    static let red = Color(red: 192 / 255, green: 32 / 255, blue: 32 / 255)
    static let redEdge = Color(red: 224 / 255, green: 64 / 255, blue: 64 / 255)
}

/// LIVE's and SAVE's 38 by 22 face.
struct DsrButtonLabel: View {
    enum Look {
        case plain
        case red
        case yellow
        case yellowOn
    }

    let text: String
    let look: Look
    let size: CGSize

    var body: some View {
        let (fill, edge, ink): (Color, Color, Color) = switch look {
        case .plain: (Color(white: 0.165), Color(white: 0.33), Color(white: 0.72))
        case .red: (DsrColours.red, DsrColours.redEdge, .white)
        case .yellow: (Color(red: 34 / 255, green: 29 / 255, blue: 8 / 255), Color(red: 122 / 255, green: 100 / 255,
                                                                                  blue: 24 / 255), DsrColours.yellow)
        case .yellowOn: (DsrColours.yellow, DsrColours.yellow, Color(white: 0.07))
        }
        Text(text)
            .font(.system(size: 10.5, weight: .bold))
            .tracking(0.5)
            .foregroundStyle(ink)
            .frame(width: size.width, height: size.height)
            .background(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).fill(fill))
            .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).stroke(edge))
    }
}

/// The clock: a ring with two hands.
struct DsrClockGlyph: View {
    var body: some View {
        Canvas { context, size in
            let centre = CGPoint(x: size.width / 2, y: size.height / 2)
            let ring = Path(ellipseIn: CGRect(x: centre.x - 6.5, y: centre.y - 6.5, width: 13, height: 13))
            context.stroke(ring, with: .color(Color(white: 0.78)), lineWidth: 1.5)
            var hands = Path()
            hands.move(to: CGPoint(x: centre.x, y: centre.y - 4))
            hands.addLine(to: CGPoint(x: centre.x, y: centre.y + 0.3))
            hands.addLine(to: CGPoint(x: centre.x + 2.8, y: centre.y + 2))
            context.stroke(hands, with: .color(Color(white: 0.78)), style: StrokeStyle(lineWidth: 1.5, lineCap: .round))
        }
    }
}

// MARK: The past picture

/// What the past puts over the band, under the flags and touching nothing:
/// the Core's rows over the waterfall, SAVE's yellow band, "Drawing", the
/// PAST tag and the past spectrum, the line of the moment played, and the
/// recorder's notes.
struct DsrPictureLayer: View {
    let model: DsrPanModel
    @ObservedObject var band: BandModel
    let layout: BandLayout
    let geometry: BandGeometry
    let place: DsrStripPlace
    /// The letter of the slice that plays.
    var sliceLetter = "A"

    var body: some View {
        ZStack(alignment: .topLeading) {
            if model.coreDraws, let image = pastImage() {
                image.picture
                    .resizable()
                    .interpolation(.none)
                    .frame(width: image.rect.width, height: image.rect.height)
                    .offset(x: image.rect.minX, y: image.rect.minY)
                    .background(Color.black.frame(width: layout.waterfall.width, height: layout.waterfall.height)
                        .offset(x: layout.waterfall.minX, y: layout.waterfall.minY), alignment: .topLeading)
            }
            if model.inPast, let spectrum = model.pastSpectrum {
                pastSpectrum(spectrum)
            }
            if model.inPast {
                pastTag
            }
            if let stretch = model.stretch {
                bracketBand(stretch)
            }
            if model.followsSound, model.inPast {
                playLine
            }
            if let text = model.drawingText {
                Color.black.opacity(0.55)
                    .frame(width: place.picture.width, height: place.picture.height)
                    .overlay(DsrNote(text: text))
                    .offset(x: place.picture.minX, y: place.picture.minY)
                    .accessibilityElement(children: .combine)
                    .accessibilityIdentifier("dsr.drawing")
            }
            if let note = model.note {
                DsrNote(text: note)
                    .frame(width: place.picture.width)
                    .offset(x: place.picture.minX, y: place.picture.minY + 40)
                    .accessibilityElement(children: .combine)
                    .accessibilityIdentifier("dsr.note")
            }
        }
        .allowsHitTesting(false)
    }

    private struct PastImage {
        let picture: Image
        let rect: CGRect
    }

    /// The rows the Core drew, coloured with the waterfall's palette and
    /// levels, placed by their frequencies.
    private func pastImage() -> PastImage? {
        guard let rowsLayout = model.rowsLayout, rowsLayout.count > 0, geometry.spanHz > 0 else {
            return nil
        }
        let settings = band.drawnSettings
        let table = DsrPastPicture.table(settings: settings, catalog: band.catalog)
        let levels = band.effectiveWaterfallLevels ?? settings.manualLevels
        guard let image = DsrPastPicture.image(model: model, width: rowsLayout.count,
                                               height: Int(layout.waterfall.height), table: table, levels: levels) else {
            return nil
        }
        let left = geometry.x(forHz: rowsLayout.firstHz - rowsLayout.hzPerBin / 2)
        let right = geometry.x(forHz: rowsLayout.firstHz + (Double(rowsLayout.count) - 0.5) * rowsLayout.hzPerBin)
        return PastImage(picture: Image(decorative: image, scale: 1),
                         rect: CGRect(x: layout.waterfall.minX + left, y: layout.waterfall.minY,
                                      width: max(1, right - left), height: layout.waterfall.height))
    }

    private var pastTag: some View {
        let moment = model.playedMoment ?? model.top
        return HStack(spacing: 5) {
            Text(model.client.dsr?.stretch != nil ? "SAVED" : "PAST")
                .font(.system(size: 10.5, weight: .bold))
                .tracking(0.6)
                .foregroundStyle(WorkspacePalette.keyedRim)
            Text(DsrPanModel.hms(moment))
                .font(.system(size: 10, design: .monospaced))
                .foregroundStyle(Color(red: 1, green: 0.82, blue: 0.82))
        }
        .padding(.horizontal, 6)
        .frame(height: 20)
        .background(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).fill(Color(red: 58 / 255, green: 13 / 255, blue: 13 / 255)))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).stroke(DsrColours.red))
        .offset(x: layout.spectrum.minX + 5, y: layout.spectrum.minY + 4)
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("dsr.pastTag")
    }

    /// The past spectrum over the live one, which dims under it.
    private func pastSpectrum(_ frame: DsrPastFrames) -> some View {
        Canvas { context, size in
            context.fill(Path(CGRect(origin: .zero, size: size)), with: .color(.black.opacity(0.6)))
            guard frame.rowCount > 0, frame.binCount > 1 else {
                return
            }
            var trace = Path()
            for bin in 0..<frame.binCount {
                let hz = frame.firstBinHz + Double(bin) * frame.hzPerBin
                let point = CGPoint(x: geometry.x(forHz: hz), y: geometry.y(forDbm: Double(frame.dbm(row: 0, bin: bin))))
                if bin == 0 {
                    trace.move(to: point)
                } else {
                    trace.addLine(to: point)
                }
            }
            context.stroke(trace, with: .color(Color(white: 0.92)), lineWidth: 1.2)
        }
        .frame(width: layout.spectrum.width, height: layout.spectrum.height)
        .offset(x: layout.spectrum.minX, y: layout.spectrum.minY)
    }

    private func bracketBand(_ stretch: (from: Double, to: Double)) -> some View {
        let spp = max(model.secondsPerPoint, 0.001)
        let low = max(0, (model.top - stretch.to) / spp)
        let high = min(Double(layout.waterfall.height), (model.top - stretch.from) / spp)
        let width = max(0, place.strip.minX - layout.waterfall.minX)
        return Rectangle()
            .fill(DsrColours.yellow.opacity(0.13))
            .overlay(alignment: .top) { Rectangle().fill(DsrColours.yellow).frame(height: 2) }
            .overlay(alignment: .bottom) { Rectangle().fill(DsrColours.yellow).frame(height: 2) }
            .frame(width: width, height: max(0, high - low))
            .offset(x: layout.waterfall.minX, y: layout.waterfall.minY + low)
            .opacity(high > low ? 1 : 0)
    }

    private var playLine: some View {
        let moment = model.playedMoment ?? model.top
        return ZStack(alignment: .topLeading) {
            Rectangle()
                .fill(Color.white.opacity(0.85))
                .frame(width: place.picture.width, height: 1.5)
            Text("\u{25B6} " + DsrPanModel.hms(moment) + "  Slice " + sliceLetter)
                .font(.system(size: 10.5, design: .monospaced))
                .foregroundStyle(.white)
                .padding(.horizontal, 4)
                .padding(.vertical, 1)
                .background(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).fill(Color.black.opacity(0.6)))
                .offset(x: 4, y: 3)
        }
        .offset(x: place.picture.minX, y: layout.waterfall.minY + 1)
    }
}

/// A short note in the recorder's grey box.
struct DsrNote: View {
    let text: String

    var body: some View {
        Text(text)
            .font(.system(size: 12.5))
            .foregroundStyle(Color(white: 0.87))
            .multilineTextAlignment(.center)
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
            .background(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).fill(Color(red: 30 / 255, green: 30 / 255, blue: 34 / 255,
                                                                       opacity: 0.95)))
            .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).stroke(Color(white: 0.27)))
            .padding(.horizontal, 12)
    }
}

/// The Core's rows as a picture the size of the waterfall: one line a
/// point, from the picture's top down, each the row kept nearest that
/// moment; a moment with no row is black.
enum DsrPastPicture {
    /// The waterfall's 256 colours, as the band's renderer picks them.
    static func table(settings: BandDisplaySettings, catalog: StationCatalog?) -> [UInt8] {
        if settings.waterfallPaletteId == BandPalette.customPaletteId {
            return BandPalette.table(BandPalette.table(custom: settings.customPalette), lowColour: settings.waterfallLowColour)
        }
        let palette = catalog.flatMap { BandPalette.palette(id: settings.waterfallPaletteId, in: $0.palettes) }
        return BandPalette.table(BandPalette.table(for: palette), lowColour: settings.waterfallLowColour)
    }

    /// Palette position 0 at the low level, 255 at the high one.
    static func index(dbm: Float, levels: WaterfallHistory.Levels) -> Int {
        let span = max(levels.highDbm - levels.lowDbm, 1)
        return Int(min(255, max(0, ((dbm - levels.lowDbm) / span * 255).rounded())))
    }

    @MainActor
    static func image(model: DsrPanModel, width: Int, height: Int, table: [UInt8],
                      levels: WaterfallHistory.Levels) -> CGImage? {
        guard width > 0, height > 0, table.count >= BandPalette.entries * 4 else {
            return nil
        }
        var pixels = [UInt8](repeating: 0, count: width * height * 4)
        let spp = max(model.secondsPerPoint, 0.001)
        let top = model.top
        for y in 0..<height {
            let line = y * width * 4
            guard let row = model.row(at: top - Double(y) * spp), row.tenthsDb.count == width else {
                for x in 0..<width {
                    pixels[line + x * 4 + 3] = 255
                }
                continue
            }
            for x in 0..<width {
                let at = index(dbm: Float(row.tenthsDb[x]) / 10, levels: levels) * 4
                let out = line + x * 4
                pixels[out] = table[at]
                pixels[out + 1] = table[at + 1]
                pixels[out + 2] = table[at + 2]
                pixels[out + 3] = 255
            }
        }
        guard let provider = CGDataProvider(data: Data(pixels) as CFData) else {
            return nil
        }
        return CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: width * 4,
                       space: CGColorSpaceCreateDeviceRGB(),
                       bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue),
                       provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent)
    }
}
