// NereusSDR for iOS: Setup's slider, a UISlider in the workspace look: the track in the inset, the filled part in the glass rim blue, a light raised thumb
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import UIKit

/// The UISlider under ``SetupSlider``. Its touch handling is UISlider's own:
/// the value moves only while the thumb is dragged, a tap on the track
/// changes nothing, a drag that starts off the thumb belongs to the page's
/// scroll, and VoiceOver adjusts it. Only the images are the workspace's
/// (spec section 7.1, "Sliders"), and VoiceOver's adjust moves one step.
final class SetupUISlider: UISlider {
    /// The values the slider covers.
    var range: ClosedRange<Double> = 0...1 {
        didSet {
            minimumValue = Float(range.lowerBound)
            maximumValue = Float(range.upperBound)
        }
    }
    /// The step the value snaps to; 0 is a slider without steps.
    var step: Double = 0
    /// Called with each new value while the slider is edited.
    var onMove: (Double) -> Void = { _ in }
    /// Called with true when an edit starts and false when it ends, as `Slider` does.
    var onEditingChanged: (Bool) -> Void = { _ in }
    /// What VoiceOver reads as the value in place of UISlider's percentage;
    /// nil keeps the percentage.
    var spokenValue: String?
    /// True from an edit's start to its end.
    private(set) var isEditing = false
    /// The value last handed to `onMove`, so a move that snaps to the same
    /// step hands nothing.
    private var lastMoved: Double?

    override init(frame: CGRect) {
        super.init(frame: frame)
        setMinimumTrackImage(SetupSliderImages.filledTrack, for: .normal)
        setMaximumTrackImage(SetupSliderImages.insetTrack, for: .normal)
        setThumbImage(SetupSliderImages.thumb, for: .normal)
        setThumbImage(SetupSliderImages.thumb, for: .highlighted)
        addTarget(self, action: #selector(valueMoved), for: .valueChanged)
    }

    required init?(coder: NSCoder) {
        nil
    }

    /// Shows `number` while no edit is under way; an edit's own value is
    /// never pulled back to the model's.
    func show(_ number: Double) {
        guard !isEditing, !isTracking else {
            return
        }
        lastMoved = nil
        setValue(Float(number), animated: false)
    }

    override func beginTracking(_ touch: UITouch, with event: UIEvent?) -> Bool {
        let began = super.beginTracking(touch, with: event)
        if began {
            editingBegan()
        }
        return began
    }

    override func endTracking(_ touch: UITouch?, with event: UIEvent?) {
        super.endTracking(touch, with: event)
        editingEnded()
    }

    override func cancelTracking(with event: UIEvent?) {
        super.cancelTracking(with: event)
        editingEnded()
    }

    override var accessibilityValue: String? {
        get { spokenValue ?? super.accessibilityValue }
        set { super.accessibilityValue = newValue }
    }

    /// VoiceOver's swipe up: one step, sent as one edit.
    override func accessibilityIncrement() {
        adjust(by: 1)
    }

    /// VoiceOver's swipe down: one step, sent as one edit.
    override func accessibilityDecrement() {
        adjust(by: -1)
    }

    /// An edit starts: the finger is on the thumb, or VoiceOver adjusts.
    func editingBegan() {
        guard !isEditing else {
            return
        }
        isEditing = true
        onEditingChanged(true)
    }

    /// An edit ends: the finger lifts or leaves, or VoiceOver's adjust is done.
    func editingEnded() {
        guard isEditing else {
            return
        }
        isEditing = false
        onEditingChanged(false)
    }

    /// The thumb moved: snap to the step and hand the value on.
    @objc func valueMoved() {
        let snapped = Self.snapped(Double(value), range: range, step: step)
        if Float(snapped) != value {
            value = Float(snapped)
        }
        guard snapped != lastMoved else {
            return
        }
        lastMoved = snapped
        onMove(snapped)
    }

    /// `number` on the nearest step of `range`, inside it.
    static func snapped(_ number: Double, range: ClosedRange<Double>, step: Double) -> Double {
        let clamped = min(max(number, range.lowerBound), range.upperBound)
        guard step > 0 else {
            return clamped
        }
        let steps = ((clamped - range.lowerBound) / step).rounded()
        return min(max(range.lowerBound + steps * step, range.lowerBound), range.upperBound)
    }

    /// One VoiceOver adjust: a step, or a tenth of the range without steps,
    /// as UISlider moves it.
    private func adjust(by direction: Double) {
        guard isEnabled else {
            return
        }
        let amount = step > 0 ? step : (range.upperBound - range.lowerBound) / 10
        let next = Self.snapped(Double(value) + direction * amount, range: range, step: step)
        guard Float(next) != value else {
            return
        }
        editingBegan()
        value = Float(next)
        valueMoved()
        editingEnded()
    }
}

/// The slider's images, drawn once.
enum SetupSliderImages {
    /// The track's height and the thumb's diameter. The board draws a 4
    /// point track (setup-board.html:48); the inset's rim needs 6 to show.
    static let trackHeight: CGFloat = 6
    static let thumbDiameter: CGFloat = 22 // setup-board.html:50

    /// The filled part: kGlassSelBorder (setup-board.html:174).
    static let filledTrack = track(fill: UIColor(WorkspacePalette.glassSelBorder), rim: nil)

    /// The rest of the track: the inset, kInsetBg with its kInsetBorder rim
    /// (spec section 7.1, "Sliders").
    static let insetTrack = track(fill: UIColor(WorkspacePalette.insetBg), rim: UIColor(WorkspacePalette.insetBorder))

    /// The light raised thumb, as the board draws it (setup-board.html:175):
    /// kTitleText to kTextTertiary from top to bottom, a kTextInactive rim.
    static let thumb: UIImage = {
        let size = CGSize(width: thumbDiameter, height: thumbDiameter)
        return UIGraphicsImageRenderer(size: size).image { context in
            let cg = context.cgContext
            let circle = CGRect(origin: .zero, size: size)
            cg.saveGState()
            cg.addEllipse(in: circle)
            cg.clip()
            let colors = [UIColor(WorkspacePalette.titleText).cgColor, UIColor(WorkspacePalette.textTertiary).cgColor]
            if let gradient = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: colors as CFArray,
                                         locations: [0, 1]) {
                cg.drawLinearGradient(gradient, start: CGPoint(x: 0, y: circle.minY), end: CGPoint(x: 0, y: circle.maxY),
                                      options: [])
            }
            cg.restoreGState()
            cg.setStrokeColor(UIColor(WorkspacePalette.textInactive).cgColor)
            cg.setLineWidth(1)
            cg.strokeEllipse(in: circle.insetBy(dx: 0.5, dy: 0.5))
        }
    }()

    /// A capsule track piece, stretched between its round ends.
    private static func track(fill: UIColor, rim: UIColor?) -> UIImage {
        let radius = trackHeight / 2
        let size = CGSize(width: 2 * radius + 1, height: trackHeight)
        let image = UIGraphicsImageRenderer(size: size).image { _ in
            let shape = UIBezierPath(roundedRect: CGRect(origin: .zero, size: size), cornerRadius: radius)
            fill.setFill()
            shape.fill()
            if let rim {
                let edge = UIBezierPath(roundedRect: CGRect(origin: .zero, size: size).insetBy(dx: 0.5, dy: 0.5),
                                        cornerRadius: radius - 0.5)
                edge.lineWidth = 1
                rim.setStroke()
                edge.stroke()
            }
        }
        return image.resizableImage(withCapInsets: UIEdgeInsets(top: 0, left: radius, bottom: 0, right: radius),
                                    resizingMode: .stretch)
    }
}
