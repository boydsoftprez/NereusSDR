// NereusSDR for iOS: the Digital Spectrum Recorder's time strip, LIVE, SAVE, the clock and the past picture on the pan
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 28, the phone's and iPad's Digital Spectrum
//   Recorder on the pan. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Recording plan fix round: SAVE's handles move by a step for
//   VoiceOver, the hours are asked as the recorder changes, the play line
//   shows only for this device's own rewind. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: The note shows "Nothing was kept at this frequency then." while
//   the slice plays the past outside what was kept (DsrPanModel.keptNote).
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: A drag in from the right edge over the strip is the band's
//   edge swipe (spec 11.3), not time (DsrStripEdge). J.J. Boyd (KG4VCF),
//   AI-assisted via Claude Code.
// 2026-11-17: Fixes after JJ's phone run of PR 797: the picture and the past
//   spectrum follow the sound every display frame (DsrFrameLink), the strip's
//   LIVE is red while the band's own waterfall looks back (the band's own
//   LIVE no longer draws over it), and the past spectrum replaces the live
//   one instead of veiling it; the time strip's ticks move every display
//   frame, live and in the past, not on the 250 ms tick.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: The time strip redraws every display frame only in the past or
//   while a rewind plays; live it is back to its old pace (the phone's heat).
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: Screen walk for PR 808, the strip's times are written below
//   SAVE, never over it. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: PR 808 design calls, the PAST tag moves clear of a flag and
//   its round buttons. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: PR 808 sheet overlap: SAVE opens its bracket inside the
//   strip and its handles stay on it; the past spectrum is drawn by the
//   band's renderer, not here.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: PR 808 part 2, PAST and its time go on every flag's top line
//   and folded tag on the pan (DsrFlagPastChip); the PAST tag on the
//   spectrum shows only where no flag does. J.J. Boyd (KG4VCF), AI-assisted
//   via Claude Code.
// 2026-11-17: PR 808 final overlaps: the play label's room
//   (playLabelRoom), which the folded tags keep off; a full flag over it
//   moves the label below the flag. J.J. Boyd (KG4VCF), AI-assisted via
//   Claude Code.

import CoreGraphics
import NereusBand
import NereusModels
import QuartzCore
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

/// The band's edge zone under the strip (spec 11.3): a drag that starts
/// there and heads in is handed to the band, which opens what the edge is
/// set to, or moves the band when the edge is Off. Points are the band's.
struct DsrStripEdge {
    /// The point is in an edge zone.
    let inZone: (CGPoint) -> Bool
    /// A drag from `start`, moved this far, heads in from its edge.
    let claims: (_ start: CGPoint, _ translation: CGSize) -> Bool
    let changed: (_ start: CGPoint, _ translation: CGSize, _ location: CGPoint) -> Void
    let ended: (_ translation: CGSize, _ predicted: CGSize) -> Void
    let cancelled: () -> Void
}

/// The strip, the map, LIVE, SAVE, the clock and SAVE's handles, over the
/// band's drag: a drag on the strip moves the picture, never the band,
/// but for one in from the edge, which is the edge's swipe (`edge`).
struct DsrStripLayer: View {
    let model: DsrPanModel
    let place: DsrStripPlace
    /// Seconds a waterfall point stands for: the pan's Update period.
    var secondsPerPoint: Double = 0.1
    /// The band's edge zone; nil leaves every drag on the strip to time.
    var edge: DsrStripEdge?
    /// The band's own waterfall looks back (the Display sheet's Look back):
    /// LIVE is red, as the band's own LIVE was, and brings it back.
    var bandLooksBack = false
    /// Steps the picture with the sound every display frame while in the past.
    @State private var frameLink = DsrFrameLink()
    @State private var dragStart: Double?
    @State private var mapFinger: CGFloat?
    /// What the drag on the strip is: not yet known, time, or the edge's.
    @State private var stripDrag = StripDrag.undecided
    /// A drag on the strip is under way; a cancelled one ends here without its onEnded.
    @GestureState private var stripDragging = false

    private enum StripDrag {
        case undecided
        case time
        case edge
    }

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
        .onChange(of: model.inPast, initial: true) { _, past in
            if past {
                // Stopped when the past ends or the strip goes away.
                frameLink.start { [model] period in
                    model.frame(periodSeconds: period)
                }
            } else {
                frameLink.stop()
            }
        }
        .onDisappear {
            frameLink.stop()
        }
    }

    // MARK: The strip

    private var strip: some View {
        // In the past, or while a rewind plays, the strip redraws every display
        // frame and moves as the picture's top does, which the display link
        // steps every frame while the sound plays and the finger moves while it
        // drags. Live it redraws only as its values change, as before, to keep
        // the phone cool (stripRunsEveryFrame).
        TimelineView(.animation(minimumInterval: DsrPanModel.defaultFrameSeconds,
                                paused: !model.stripRunsEveryFrame)) { _ in
            let top = model.top
            Canvas { context, size in
                Self.drawStrip(context, size: size, model: model, top: top)
            }
        }
        .frame(width: place.strip.width, height: place.strip.height)
        .contentShape(Rectangle())
        .gesture(DragGesture(minimumDistance: 2, coordinateSpace: .named(Self.space))
            .updating($stripDragging) { _, state, _ in
                state = true
            }
            .onChanged { value in
                if stripDrag == .undecided {
                    // In the edge zone the drag's way is known once it has
                    // gone as far as the band's own drag needs.
                    if let edge, edge.inZone(value.startLocation) {
                        let moved = hypot(value.translation.width, value.translation.height)
                        guard moved >= TuneGestures.dragThresholdPoints else {
                            return
                        }
                        stripDrag = edge.claims(value.startLocation, value.translation) ? .edge : .time
                    } else {
                        stripDrag = .time
                    }
                }
                if stripDrag == .edge {
                    edge?.changed(value.startLocation, value.translation, value.location)
                    return
                }
                let start = dragStart ?? model.top
                dragStart = start
                model.drag(from: start, by: value.translation.height)
            }
            .onEnded { value in
                let kind = stripDrag
                stripDrag = .undecided
                switch kind {
                case .edge:
                    edge?.ended(value.translation, value.predictedEndTranslation)
                case .time:
                    dragStart = nil
                    model.dragEnded()
                case .undecided:
                    break
                }
            })
        .onChange(of: stripDragging) { _, now in
            // A drag cancelled before it ended: an edge swipe springs back.
            if !now, stripDrag != .undecided {
                if stripDrag == .edge {
                    edge?.cancelled()
                }
                stripDrag = .undecided
            }
        }
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

    /// The strip's tick spacings in seconds, shortest first: the first one
    /// that leaves 30 points between ticks is drawn. Drawing values, the
    /// same on every radio.
    static let tickSteps: [Double] = [1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600]

    /// The highest a time is written on the strip, in points from its top:
    /// under SAVE (31 down, 22 tall) with half a label's height to spare.
    /// It was 16, under LIVE only, so a time could be written over SAVE.
    static let labelTop: CGFloat = 31 + DsrStripPlace.buttonSize.height + 6

    /// The strip's ticks and times: a tick each step, the time on every
    /// minute (or every other tick when the picture is short), the moments
    /// you transmitted as a red edge and SAVE's bracket in yellow.
    static func drawStrip(_ context: GraphicsContext, size: CGSize, model: DsrPanModel, top: Double) {
        let past = model.inPast
        context.fill(Path(CGRect(origin: .zero, size: size)), with: .color(Color(red: 12 / 255, green: 13 / 255,
                                                                                 blue: 20 / 255, opacity: 0.86)))
        context.fill(Path(CGRect(x: 0, y: 0, width: 1, height: size.height)), with: .color(Color(white: 0.17)))
        let spp = max(model.secondsPerPoint, 0.001)
        let bottom = top - Double(size.height) * spp
        let step = tickSteps.first { $0 / spp >= 30 } ?? 3600
        let labelEvery = step / spp >= 60 ? step : step * 2
        var moment = (top / step).rounded(.down) * step
        while moment > bottom {
            let y = (top - moment) / spp
            let full = (moment / labelEvery).rounded() * labelEvery == moment
            context.fill(Path(CGRect(x: 1, y: y, width: full ? 7 : 4, height: 1)),
                         with: .color(full ? Color(white: 0.6) : Color(white: 0.33)))
            // Clear of LIVE and SAVE at the strip's top and the clock at its foot.
            if full, y > Double(labelTop), y < Double(size.height) - 34 {
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
                DsrButtonLabel(text: "LIVE", look: model.liveLit || bandLooksBack ? .red : .plain,
                               size: DsrStripPlace.buttonSize)
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier("dsr.live")
            .accessibilityLabel("Live")
            .accessibilityHint(model.liveLit || bandLooksBack ? "Back to live" : "Already live")
            .offset(x: place.live.minX, y: place.live.minY)
            Button {
                model.toggleSave(stripPoints: Double(place.strip.height))
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
        guard y >= place.waterfall.minY - 2 else {
            return nil
        }
        // A handle never leaves the strip: a bracket that runs on below it (the
        // shortest stretch at a fast waterfall) keeps its handle above the clock (PR 808).
        return min(y, place.strip.maxY - CGFloat(DsrPanModel.bracketClockRoomPoints))
    }
}

/// A display link for the recorder's picture: while the past shows it calls
/// back once a display frame, at the band's own pace (30 to 60 a second),
/// with the frame's length in seconds.
@MainActor
final class DsrFrameLink {
    private var link: CADisplayLink?

    func start(_ step: @escaping @MainActor (Double) -> Void) {
        stop()
        let link = CADisplayLink(target: Target(step), selector: #selector(Target.step(_:)))
        // The band's own display link's range (BandView): never faster than 60 a second.
        link.preferredFrameRateRange = CAFrameRateRange(minimum: 30, maximum: 60, preferred: 60)
        link.add(to: .main, forMode: .common)
        self.link = link
    }

    func stop() {
        link?.invalidate()
        link = nil
    }

    /// The display link holds its target strongly; the target holds only the step.
    /// On the main actor, as the band's own link target is (BandView's LinkTarget): the link runs on the main run loop.
    @MainActor
    private final class Target: NSObject {
        let call: @MainActor (Double) -> Void

        init(_ call: @escaping @MainActor (Double) -> Void) {
            self.call = call
        }

        @objc func step(_ link: CADisplayLink) {
            call(link.targetTimestamp - link.timestamp)
        }
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
/// PAST tag, the line of the moment played, and the recorder's notes. The
/// past spectrum is the band's own trace, drawn by its renderer
/// (``DsrPanModel/pastTrace``, PR 808).
struct DsrPictureLayer: View {
    /// The play label's offset inside the play line's frame.
    static let playLabelInset = CGSize(width: 4, height: 3)
    /// The room the play label ("▶ hh:mm:ss  Slice X", 10.5-point
    /// monospaced at a fixed size, about 128 points) takes, with a little
    /// over: what the folded tags keep off (PR 808 final overlaps).
    static let playLabelSize = CGSize(width: 140, height: 17)

    /// The gap kept between the play label and a flag it drops below.
    static let playLabelGap: CGFloat = 3

    /// Where the play label sits on a waterfall at `waterfall`, as
    /// ``playLine`` draws it: at the waterfall's top, or below any flag or
    /// folded tag in `below` that would cover it (a full flag taller than
    /// a sideways spectrum hangs over the waterfall's top).
    static func playLabelRoom(waterfall: CGRect, below: [CGRect] = []) -> CGRect {
        var room = CGRect(x: waterfall.minX + playLabelInset.width, y: waterfall.minY + 1 + playLabelInset.height,
                          width: playLabelSize.width, height: playLabelSize.height)
        var moved = true
        while moved {
            moved = false
            for rect in below where rect.minX < room.maxX && room.minX < rect.maxX
                && rect.minY < room.maxY && room.minY < rect.maxY {
                room.origin.y = rect.maxY + playLabelGap
                moved = true
            }
        }
        return room
    }

    let model: DsrPanModel
    @ObservedObject var band: BandModel
    let layout: BandLayout
    let geometry: BandGeometry
    let place: DsrStripPlace
    /// The letter of the slice that plays.
    var sliceLetter = "A"
    /// A flag or folded tag on this pan shows PAST (``DsrFlagPastChip``),
    /// so the PAST tag on the spectrum stays away (PR 808).
    var pastOnFlag = false
    /// The flags and folded tags, which the play label drops below (PR 808 final overlaps).
    var flags: [CGRect] = []
    /// What the PAST tag keeps clear of: the flags with their round
    /// buttons, folded tags, the button column and
    /// the edge markers (PR 808 screen walk). With no flag on the pan it
    /// stays at the spectrum's top left, moved clear of those.
    var avoid: [CGRect] = []

    /// The PAST tag's measured size.
    @State private var tagSize = CGSize(width: 104, height: 20)

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
            if model.inPast, !pastOnFlag {
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
            if let note = model.note ?? model.keptNote {
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
        guard let image = DsrPastPicture.cachedImage(model: model, width: rowsLayout.count,
                                                     height: Int(layout.waterfall.height), table: table,
                                                     levels: levels) else {
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
        let place = Self.pastTagPlace(spectrum: layout.spectrum, size: tagSize, avoid: avoid)
        return HStack(spacing: 5) {
            Text(model.client.dsr?.stretch != nil ? "SAVED" : "PAST")
                .font(.system(size: 10.5, weight: .bold))
                .tracking(0.6)
                .foregroundStyle(WorkspacePalette.keyedRim)
            if !place.compact {
                Text(DsrPanModel.hms(moment))
                    .font(.system(size: 10, design: .monospaced))
                    .foregroundStyle(Color(red: 1, green: 0.82, blue: 0.82))
            }
        }
        .padding(.horizontal, 6)
        .frame(height: 20)
        .background(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).fill(Color(red: 58 / 255, green: 13 / 255, blue: 13 / 255)))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).stroke(DsrColours.red))
        .fixedSize()
        .onGeometryChange(for: CGSize.self) { $0.size } action: { size in
            // The full tag's size; the compact one is never measured into it.
            if !place.compact {
                tagSize = size
            }
        }
        .offset(place.origin)
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("dsr.pastTag")
    }

    /// The PAST tag without its time: "PAST" alone, where the whole tag has no room.
    static let compactTagSize = CGSize(width: 44, height: 20)

    /// Where the PAST tag sits (PR 808 screen walk): the spectrum's top
    /// left, else beside or under whatever covers that (a flag and its round
    /// buttons); with no room for the whole tag, "PAST" alone in the first
    /// clear place; the top left again when nothing is clear.
    static func pastTagPlace(spectrum: CGRect, size: CGSize, avoid: [CGRect]) -> (origin: CGSize, compact: Bool) {
        let gap: CGFloat = 5
        let top = spectrum.minY + 4
        let blocks = avoid.filter { $0.intersects(spectrum) }
        for (tagSize, compact) in [(size, false), (compactTagSize, true)] {
            func clear(_ x: CGFloat, _ y: CGFloat) -> Bool {
                let tag = CGRect(x: x, y: y, width: tagSize.width, height: tagSize.height)
                return tag.minX >= spectrum.minX && tag.maxX <= spectrum.maxX - gap && tag.maxY <= spectrum.maxY
                    && !blocks.contains { $0.insetBy(dx: -2, dy: -2).intersects(tag) }
            }
            let xs = [spectrum.minX + gap] + blocks.map { $0.minX - gap - tagSize.width }.sorted()
                + blocks.map { $0.maxX + gap }.sorted()
            for y in [top] + blocks.map({ $0.maxY + gap }).sorted() {
                if let x = xs.first(where: { clear($0, y) }) {
                    return (CGSize(width: x, height: y), compact)
                }
            }
        }
        return (CGSize(width: spectrum.minX + gap, height: top), false)
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
        let lineTop = layout.waterfall.minY + 1
        let room = Self.playLabelRoom(waterfall: CGRect(x: place.picture.minX, y: layout.waterfall.minY,
                                                        width: place.picture.width, height: layout.waterfall.height),
                                      below: flags)
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
                .offset(x: Self.playLabelInset.width, y: room.minY - lineTop)
        }
        .offset(x: place.picture.minX, y: lineTop)
    }
}

/// PAST (SAVED for an opened stretch) and the moment played, small, in a
/// flag's top line over its filter width and TX, and at the end of a
/// folded tag (PR 808, the manager's call: the past is the whole pan's
/// state, so every flag on the pan shows it). Draws nothing live; takes no
/// touch, so the flag's buttons under it keep theirs.
struct DsrFlagPastChip: View {
    let model: DsrPanModel

    /// The chip's size, fixed so the folded tag's width is known before it draws.
    static let size = CGSize(width: 84, height: 12)
    /// The gap before it at the end of a folded tag.
    static let foldedGap: CGFloat = 6

    var body: some View {
        if model.inPast {
            TimelineView(.periodic(from: .now, by: 0.5)) { _ in
                chip
            }
            .allowsHitTesting(false)
        }
    }

    private var chip: some View {
        HStack(spacing: 4) {
            Text(model.client.dsr?.stretch != nil ? "SAVED" : "PAST")
                .font(.system(size: 8.5, weight: .bold))
                .tracking(0.4)
                .foregroundStyle(WorkspacePalette.keyedRim)
            Text(DsrPanModel.hms(model.playedMoment ?? model.top))
                .font(.system(size: 8.5, design: .monospaced))
                .foregroundStyle(Color(red: 1, green: 0.82, blue: 0.82))
        }
        .lineLimit(1)
        .minimumScaleFactor(0.8)
        .frame(width: Self.size.width, height: Self.size.height)
        .background(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).fill(Color(red: 58 / 255, green: 13 / 255, blue: 13 / 255)))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).stroke(DsrColours.red, lineWidth: 0.75))
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("dsr.flagPast")
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

    /// ``image(model:width:height:table:levels:)``, drawn again only when the
    /// rows, the top's line, the size, the colours or the levels change: the
    /// picture's top moves every display frame while the sound plays, but
    /// its lines change only as the top crosses into the next one.
    @MainActor
    static func cachedImage(model: DsrPanModel, width: Int, height: Int, table: [UInt8],
                            levels: WaterfallHistory.Levels) -> CGImage? {
        let spp = max(model.secondsPerPoint, 0.001)
        let top = model.top
        // The rows at the picture's top and foot stand for the lines between.
        let ends = [top, top - Double(max(0, height - 1)) * spp].map { model.row(at: $0)?.utcNs ?? 0 }
        let line = Int64((top / spp).rounded())
        let key = "\(model.rowsRevision)|\(line)|\(ends[0])|\(ends[1])|\(spp)|\(width)|\(height)|\(table.hashValue)|"
            + "\(levels.lowDbm)|\(levels.highDbm)"
        if let cache = model.pictureCache, cache.key == key {
            return cache.image
        }
        guard let image = image(model: model, width: width, height: height, table: table, levels: levels) else {
            return nil
        }
        model.pictureCache = (key, image)
        return image
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
