// NereusSDR for iOS: the iPad's open pans stacked in the band's place, Pan 1 on top, each sharing the height equally
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import UIKit
import UIKit.UIGestureRecognizerSubclass

/// The iPad's pans in the band's place (R-IOS-59, DC9, spec section 6.3,
/// the board's "Two pans, two pills"): every open pan gets an equal share
/// of the height, Pan 1 on top, a hairline between them. Each pan draws
/// with its own band, column and pill (`content`); any touch on a pan makes
/// it the pan last touched, which the toolbar follows, as the finger comes
/// down (``PanTouchDown``; on iOS 17 by the first move or the lift).
struct PanStack<Content: View>: View {
    @ObservedObject var stack: PanStackModel
    @ViewBuilder let content: (PanIdentity) -> Content

    var body: some View {
        VStack(spacing: 0) {
            ForEach(Array(stack.open.enumerated()), id: \.element) { index, pan in
                if index > 0 {
                    Rectangle()
                        .fill(ChromeColours.barBorder)
                        .frame(height: 1)
                        .accessibilityHidden(true)
                }
                content(pan)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .clipped()
                    .contentShape(Rectangle())
                    .simultaneousGesture(TapGesture().onEnded { touch(pan) })
                    .simultaneousGesture(DragGesture(minimumDistance: 0).onChanged { _ in touch(pan) })
                    .modifier(PanTouchDownModifier {
                        touch(pan)
                        #if DEBUG
                        PanTouchDownProbe.shared.count(pan)
                        #endif
                    })
                    #if DEBUG
                    .overlay(alignment: .topLeading) {
                        if PanTouchDownProbe.isOn {
                            PanTouchDownProbe.Readout(pan: pan)
                        }
                    }
                    #endif
                    .accessibilityElement(children: .contain)
                    .accessibilityIdentifier("pan.\(pan.number)")
            }
        }
    }

    private func touch(_ pan: PanIdentity) {
        if stack.lastTouched != pan {
            stack.lastTouched = pan
        }
    }
}

/// Marks a pan last touched as a finger comes down anywhere on it, before
/// whatever on the pan takes the touch knows what it is (the PR 653
/// re-review d, point b and Minor C): a still finger, which SwiftUI's drag
/// reports nothing for, marks its pan at once, not at the lift.
private struct PanTouchDownModifier: ViewModifier {
    let touched: () -> Void

    func body(content: Content) -> some View {
        if #available(iOS 18.0, *) {
            content.gesture(PanTouchDown(touched: touched))
        } else {
            content
        }
    }
}

/// ``PanTouchDownRecognizer`` on a pan, through SwiftUI's own gesture system,
/// so it sees every touch on the pan's views, the band's UIKit surface included.
@available(iOS 18.0, *)
private struct PanTouchDown: UIGestureRecognizerRepresentable {
    let touched: () -> Void

    func makeUIGestureRecognizer(context: Context) -> PanTouchDownRecognizer {
        PanTouchDownRecognizer.make()
    }

    func updateUIGestureRecognizer(_ recognizer: PanTouchDownRecognizer, context: Context) {
        recognizer.touched = touched
    }
}

/// Says when a finger comes down and then fails at once, so it never
/// recognizes, never takes, delays or cancels a touch, and never stands in
/// the way of any other gesture: the band's taps, drags, pinch and hold,
/// the column's buttons and the menus keep every touch as before.
final class PanTouchDownRecognizer: UIGestureRecognizer {
    var touched: (() -> Void)?

    /// One that leaves every touch where it goes.
    static func make() -> PanTouchDownRecognizer {
        let recognizer = PanTouchDownRecognizer(target: nil, action: nil)
        recognizer.cancelsTouchesInView = false
        recognizer.delaysTouchesBegan = false
        recognizer.delaysTouchesEnded = false
        return recognizer
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) {
        // UIKit's own touch tracking first, as every recognizer subclass does
        // (the PR 653 re-review e, Minor N1).
        super.touchesBegan(touches, with: event)
        touched?()
        state = .failed
    }
}

#if DEBUG
/// With ``argument`` on a debug build's command line, each pan counts the
/// touches its ``PanTouchDownRecognizer`` was given, and shows the count to
/// the UI tests on an element of its own, ``identifier(_:)``, which draws
/// nothing and takes no touch. Only the recognizer counts, so a UI test can
/// tell that UIKit gave it a real finger on the pan's band, whatever the
/// band's tap and drag did with it (the PR 653 re-review e, Minor N1).
@MainActor
final class PanTouchDownProbe: ObservableObject {
    static let argument = "-NereusPanTouchProbe"
    static let shared = PanTouchDownProbe()
    static var isOn: Bool { ProcessInfo.processInfo.arguments.contains(argument) }
    static func identifier(_ pan: PanIdentity) -> String { "panTouchDowns.\(pan.number)" }

    @Published private(set) var downs: [Int: Int] = [:]

    func count(_ pan: PanIdentity) {
        guard Self.isOn else {
            return
        }
        downs[pan.number, default: 0] += 1
    }

    /// The count, as the label of a point that draws nothing.
    struct Readout: View {
        let pan: PanIdentity
        @ObservedObject var probe = PanTouchDownProbe.shared

        var body: some View {
            Color.clear
                .frame(width: 1, height: 1)
                .allowsHitTesting(false)
                .accessibilityElement(children: .ignore)
                .accessibilityIdentifier(PanTouchDownProbe.identifier(pan))
                .accessibilityLabel("\(probe.downs[pan.number] ?? 0)")
        }
    }
}
#endif
