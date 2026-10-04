// NereusSDR for iOS: a machined tuning knob: the ring of detent marks, the index, the turning grip and its step
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// A main-tuning knob, as the board draws it (`.knob`): a ring of 36
/// detent marks, the index mark at the top, a knurled grip with a finger
/// dimple that turns under the finger, and the step in the middle, whose
/// tap opens the step menu. Turning it anywhere outside the middle moves
/// the active slice one step per detent (``BandTuningModel/turn(byRadians:)``);
/// the index flashes on each detent where the phone ticks. Greyed while
/// the active slice cannot be tuned from here.
struct Knob: View {
    /// The knob's sizes: the waterfall's knob and the sheet's big one.
    struct Metrics: Equatable {
        var diameter: CGFloat
        var gripInset: CGFloat
        var dimpleTop: CGFloat
        var dimpleDiameter: CGFloat
        var stepDiameter: CGFloat
        var stepPoints: CGFloat

        /// The board's `.knob`.
        static let waterfall = Metrics(diameter: 136, gripInset: 12, dimpleTop: 12, dimpleDiameter: 20,
                                       stepDiameter: 62, stepPoints: 12)
        /// The board's `.knob--big`.
        static let sheet = Metrics(diameter: 236, gripInset: 16, dimpleTop: 18, dimpleDiameter: 30,
                                   stepDiameter: 86, stepPoints: 15)

        /// Every size times `factor`.
        func scaled(_ factor: CGFloat) -> Metrics {
            Metrics(diameter: diameter * factor, gripInset: gripInset * factor, dimpleTop: dimpleTop * factor,
                    dimpleDiameter: dimpleDiameter * factor, stepDiameter: stepDiameter * factor,
                    stepPoints: stepPoints * factor)
        }
    }

    @ObservedObject var tuning: BandTuningModel
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var settings: PhoneSettings
    let metrics: Metrics

    /// The grip's motion: following the finger, then coasting.
    @StateObject private var spinner: DialSpinner
    /// The finger's last angle about the centre, while it turns the knob.
    @State private var lastAngle: Double?
    /// The finger came down in the middle: this touch belongs to the step.
    @State private var ignoring = false

    /// `spinner` is the knob's own unless a picture drives one by hand.
    init(tuning: BandTuningModel, slices: BandSlicesModel, settings: PhoneSettings, metrics: Metrics,
         spinner: DialSpinner? = nil) {
        self.tuning = tuning
        self.slices = slices
        self.settings = settings
        self.metrics = metrics
        _spinner = StateObject(wrappedValue: spinner ?? DialSpinner())
    }

    var body: some View {
        let tunes = tuning.dialTunes
        ZStack {
            Circle()
                .fill(DialColours.knob)
                .shadow(color: .black.opacity(0.55), radius: 11, y: 8)
            KnobRing()
                .equatable()
            KnobGrip(metrics: metrics)
                .equatable()
                .rotationEffect(.radians(spinner.angle))
            index
            DialStepButton(label: tuning.dialStepLabel, outline: Circle(),
                           size: CGSize(width: metrics.stepDiameter, height: metrics.stepDiameter),
                           labelPoints: metrics.stepPoints) {
                tuning.toggleDialStepMenu()
            }
        }
        .frame(width: metrics.diameter, height: metrics.diameter)
        .contentShape(Circle())
        .gesture(turning)
        .onDisappear { spinner.halt() }
        .opacity(tunes ? 1 : 0.45)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Tuning dial")
        .accessibilityHint("Turn to move the active slice one step per detent")
        .accessibilityValue(slices.active.map { BandSlice.frequencyText(hz: $0.slice.frequencyHz) } ?? "")
        .accessibilityAdjustableAction { direction in
            let detent = DialModel.radiansPerDetent
            tuning.beginTurn(reversed: false)
            switch direction {
            case .increment:
                tuning.turn(byRadians: detent)
            case .decrement:
                tuning.turn(byRadians: -detent)
            @unknown default:
                break
            }
            tuning.endTurn()
        }
        .accessibilityIdentifier("tuningKnob")
    }

    // MARK: Drawing

    private var index: some View {
        RoundedRectangle(cornerRadius: 2)
            .fill(spinner.flash ? DialColours.indexTick : DialColours.index)
            .shadow(color: spinner.flash ? DialColours.indexGlow : .clear, radius: 4.5)
            .frame(width: 3, height: 9)
            .padding(.top, 1)
            .frame(maxHeight: .infinity, alignment: .top)
            .allowsHitTesting(false)
    }

    // MARK: Turning

    /// The finger turns the grip by its angle about the centre, every
    /// touch; the grip coasts on when the finger lifts while moving. A
    /// touch stops a coast; one in the middle belongs to the step.
    private var turning: some Gesture {
        DragGesture(minimumDistance: 0, coordinateSpace: .local)
            .onChanged { value in
                let centre = CGPoint(x: metrics.diameter / 2, y: metrics.diameter / 2)
                let dx = Double(value.location.x - centre.x)
                let dy = Double(value.location.y - centre.y)
                let now = atan2(dy, dx)
                guard let last = lastAngle else {
                    let start = value.startLocation
                    let fromCentre = hypot(Double(start.x - centre.x), Double(start.y - centre.y))
                    ignoring = fromCentre < Double(metrics.stepDiameter / 2)
                    if ignoring {
                        spinner.halt()
                    } else {
                        spinner.touchDown(tuning: tuning, reversed: settings.dialReversed)
                    }
                    lastAngle = now
                    return
                }
                guard !ignoring else {
                    return
                }
                var delta = now - last
                if delta > Double.pi {
                    delta -= 2 * Double.pi
                } else if delta < -Double.pi {
                    delta += 2 * Double.pi
                }
                lastAngle = now
                spinner.follow(byRadians: delta)
            }
            .onEnded { _ in
                lastAngle = nil
                if ignoring {
                    ignoring = false
                } else {
                    spinner.release()
                }
            }
    }
}

// MARK: Drawing

/// The knob's ring: 36 marks, one per detent, 1.6 degrees wide, in the
/// knob's outer 8 points. Drawn once; it never changes.
private struct KnobRing: View, Equatable {
    var body: some View {
        Canvas { context, size in
            let centre = CGPoint(x: size.width / 2, y: size.height / 2)
            let outer = size.width / 2
            let inner = outer - 8.5
            let half = 0.8 * Double.pi / 180
            for mark in 0..<DialModel.detentsPerTurn {
                let middle = Double(mark) * DialModel.radiansPerDetent - Double.pi / 2
                var path = Path()
                path.addArc(center: centre, radius: outer, startAngle: .radians(middle - half),
                            endAngle: .radians(middle + half), clockwise: false)
                path.addArc(center: centre, radius: inner, startAngle: .radians(middle + half),
                            endAngle: .radians(middle - half), clockwise: true)
                path.closeSubpath()
                context.fill(path, with: .color(DialColours.ringTick))
            }
        }
        .allowsHitTesting(false)
    }
}

/// The grip: lit from the upper left, knurled round its edge, a dimple
/// near its top. Drawn once for its size; turning it only rotates it.
private struct KnobGrip: View, Equatable {
    let metrics: Knob.Metrics

    var body: some View {
        let diameter = metrics.diameter - metrics.gripInset * 2
        return ZStack(alignment: .top) {
            Circle()
                .fill(RadialGradient(stops: [
                    .init(color: DialColours.gripLight, location: 0),
                    .init(color: DialColours.gripMiddle, location: 0.55),
                    .init(color: DialColours.gripDark, location: 1),
                ], center: UnitPoint(x: 0.38, y: 0.32), startRadius: 0, endRadius: diameter * 0.75))
                .overlay(Circle().strokeBorder(Color.white.opacity(0.08), lineWidth: 1).offset(y: 1).mask(Circle()))
                .shadow(color: .black.opacity(0.6), radius: 3, y: 2)
            Canvas { context, size in
                let centre = CGPoint(x: size.width / 2, y: size.height / 2)
                let outer = size.width / 2
                let inner = outer - 6.5
                let tooth = 2.5 * Double.pi / 180
                for index in 0..<72 {
                    let start = Double(index) * tooth * 2 - Double.pi / 2
                    for (offset, colour) in [(0.0, DialColours.knurlDark), (tooth, DialColours.knurlLight)] {
                        var path = Path()
                        path.addArc(center: centre, radius: outer, startAngle: .radians(start + offset),
                                    endAngle: .radians(start + offset + tooth), clockwise: false)
                        path.addArc(center: centre, radius: inner, startAngle: .radians(start + offset + tooth),
                                    endAngle: .radians(start + offset), clockwise: true)
                        path.closeSubpath()
                        context.fill(path, with: .color(colour))
                    }
                }
            }
            Circle()
                .fill(RadialGradient(colors: [DialColours.dimpleDark, DialColours.dimpleLight],
                                     center: UnitPoint(x: 0.45, y: 0.4), startRadius: 0,
                                     endRadius: metrics.dimpleDiameter / 2))
                .overlay(Circle().strokeBorder(Color.black.opacity(0.5), lineWidth: 1.5).blur(radius: 1).mask(Circle()))
                .frame(width: metrics.dimpleDiameter, height: metrics.dimpleDiameter)
                .padding(.top, metrics.dimpleTop)
        }
        .frame(width: diameter, height: diameter)
        .allowsHitTesting(false)
    }
}
