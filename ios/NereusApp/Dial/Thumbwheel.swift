// NereusSDR for iOS: the flat thumbwheel along the bottom of the waterfall
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The thumbwheel (D12, spec section 5.1 item 8, picture 3): a flat wheel
/// along the bottom of the waterfall, rolled with one thumb like the
/// camera's zoom, one notch every 12 points. Rolling left brings higher
/// frequencies to the index, like a ruler. The step sits at its left end;
/// a tap there opens the step menu. Greyed while the active slice cannot
/// be tuned from here.
struct Thumbwheel: View {
    @ObservedObject var tuning: BandTuningModel
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var settings: PhoneSettings

    /// The board's `.twheel`: 52 points high, the step 62 wide, 6 between.
    static let height: CGFloat = 52
    static let stepWidth: CGFloat = 62
    static let gap: CGFloat = 6

    /// The wheel's motion, in dial radians: following the thumb, then coasting.
    @StateObject private var spinner = DialSpinner()
    /// The finger's last position along the wheel, while it rolls it.
    @State private var lastX: CGFloat?

    /// Where the ticks have rolled to, in points, rightward positive: the
    /// dial's turn at one detent every 12 points, rolling left tuning up.
    private var offset: CGFloat {
        CGFloat(-spinner.angle / DialModel.radiansPerDetent * DialModel.pointsPerNotch)
    }

    var body: some View {
        HStack(spacing: Self.gap) {
            DialStepButton(label: tuning.dialStepLabel, outline: RoundedRectangle(cornerRadius: 6),
                           size: CGSize(width: Self.stepWidth, height: Self.height)) {
                tuning.toggleDialStepMenu()
            }
            track
        }
        .frame(height: Self.height)
    }

    private var track: some View {
        ZStack {
            LinearGradient(stops: [
                .init(color: DialColours.trackEdge, location: 0),
                .init(color: DialColours.trackMiddle, location: 0.5),
                .init(color: DialColours.trackEdge, location: 1),
            ], startPoint: .top, endPoint: .bottom)
            Canvas { context, size in
                let notch = CGFloat(DialModel.pointsPerNotch)
                let phase = offset.truncatingRemainder(dividingBy: notch)
                var x = (phase < 0 ? phase + notch : phase) - notch
                while x < size.width {
                    context.fill(Path(CGRect(x: x, y: 7, width: 2, height: size.height - 14)),
                                 with: .color(DialColours.wheelTick))
                    x += notch
                }
            }
            LinearGradient(stops: [
                .init(color: DialColours.wheelFade, location: 0),
                .init(color: .clear, location: 0.24),
                .init(color: .clear, location: 0.76),
                .init(color: DialColours.wheelFade, location: 1),
            ], startPoint: .leading, endPoint: .trailing)
            Rectangle()
                .fill(spinner.flash ? DialColours.indexTick : DialColours.index)
                .shadow(color: spinner.flash ? DialColours.indexGlow : .clear, radius: 4.5)
                .frame(width: 2)
                .padding(.vertical, 4)
        }
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(DialColours.stepBorder, lineWidth: 1))
        .contentShape(RoundedRectangle(cornerRadius: 6))
        .gesture(rolling)
        .onDisappear { spinner.halt() }
        .opacity(tuning.dialTunes ? 1 : 0.45)
        .accessibilityElement()
        .accessibilityLabel("Thumbwheel")
        .accessibilityHint("Roll to move the active slice one step per notch")
        .accessibilityValue(slices.active.map { BandSlice.frequencyText(hz: $0.slice.frequencyHz) } ?? "")
        .accessibilityAdjustableAction { direction in
            let notch = DialModel.pointsPerNotch
            tuning.beginTurn(reversed: false)
            switch direction {
            case .increment:
                tuning.roll(byPoints: -notch)
            case .decrement:
                tuning.roll(byPoints: notch)
            @unknown default:
                break
            }
            tuning.endTurn()
        }
        .accessibilityIdentifier("thumbwheel")
    }

    /// The thumb rolls the wheel with it, every touch; the wheel coasts on
    /// when the thumb lifts while moving. A touch stops a coast.
    private var rolling: some Gesture {
        DragGesture(minimumDistance: 0, coordinateSpace: .local)
            .onChanged { value in
                guard let last = lastX else {
                    spinner.touchDown(tuning: tuning, reversed: settings.dialReversed)
                    lastX = value.location.x
                    return
                }
                let delta = Double(value.location.x - last)
                lastX = value.location.x
                spinner.follow(byRadians: -delta / DialModel.pointsPerNotch * DialModel.radiansPerDetent)
            }
            .onEnded { _ in
                lastX = nil
                spinner.release()
            }
    }
}
