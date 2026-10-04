// NereusSDR for iOS: the band's WIDE chip: the desktop pan's WIDE mark while the first receiver input is not Filtered
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// The band's WIDE chip (R-IOS-18, R-IOS-11, JJ's board `#widechip-review`,
/// approved 2026-09-30): the desktop marks a pan with an amber WIDE pill
/// while its receiver input's filters are not Filtered (its pan status
/// strip). The phone draws the same word in the same colours as a small
/// chip at the band's top right, left of the dBm scale, only while the
/// Core reports the first receiver input as not Filtered. When a flag, its
/// round buttons, a folded tag, the Back to your band bar, a marker at
/// the band's edge or the frames-a-second readout (while the pan shows it)
/// is there, the chip moves down the right edge below it,
/// and to the left when the band-plan strip and frequency scale are in the
/// way; it never covers any of them. A tap opens the active slice's More
/// menu under the chip, where the Filter policy lines already say why
/// (``FlagControls/filterStateLines(_:)``); a second tap, or one anywhere
/// else, closes it. The chip's target is at least 44 points each way.
enum WideChip {
    /// The desktop pill's word.
    static let word = "WIDE"
    /// What VoiceOver says: the board's words for the chip.
    static let accessibilityLabel = "WIDE: the first receiver input is not Filtered"
    static let accessibilityHint = "Opens the active slice's More menu, which says why"
    static let identifier = "bandWideChip"

    /// The desktop pill's colours (SpectrumStatusOverlay's WIDE): its
    /// ground, its word and its edge.
    static let ground = Color(red: 0x60 / 255, green: 0x40 / 255, blue: 0x00 / 255)
    static let text = Color(red: 0xFF / 255, green: 0xB8 / 255, blue: 0x00 / 255)
    static let edge = Color(red: 0x90 / 255, green: 0x60 / 255, blue: 0x00 / 255)

    /// The pill that shows: 46 by 20 points, 56 by 24 in large type (the board's).
    static func pillSize(large: Bool) -> CGSize {
        large ? CGSize(width: 56, height: 24) : CGSize(width: 46, height: 20)
    }

    /// The pill's word size, in points.
    static func wordPoints(large: Bool) -> CGFloat {
        large ? 13.75 : 11
    }

    /// The button around the pill, the tap target: 6 points either side
    /// of the pill and 44 points high.
    static func targetSize(large: Bool) -> CGSize {
        let pill = pillSize(large: large)
        return CGSize(width: max(44, pill.width + 12), height: 44)
    }

    /// The board's gap between the chip's target and anything it keeps
    /// clear of, and its steps when it looks for room.
    static let gap: CGFloat = 2
    static let stepAcross: CGFloat = 4
    static let stepDown: CGFloat = 2

    /// The chip shows while the Core reports the first input as anything
    /// but Filtered, as the desktop lights its pill; with no state from
    /// the Core, as on the desktop, it shows nothing.
    static func shows(_ state: ReceiveFilterState?) -> Bool {
        guard let state else {
            return false
        }
        return state.effective != .filtered
    }

    /// Where the chip's target sits on a band laid out as `layout`, clear
    /// of everything in `avoid` (the flags with their round buttons, folded
    /// tags, the Back to your band bar, the edge markers and the frames a
    /// second): from the top
    /// right, left of the dBm scale, down the right edge below whatever is
    /// there and, when the band-plan strip stops it, a step to the left
    /// and down again. Sideways it keeps inside the band's safe edges.
    @MainActor
    static func rect(layout: BandLayout, avoid: [CGRect], sideways: Bool, trailingInset: CGFloat,
                     large: Bool) -> CGRect {
        let floor = layout.strip.minY
        // The dBm scale and its arrows, moved in from the phone's rounded edge sideways.
        let dbmLeft = layout.dbmScale.minX - trailingInset
        let dbm = CGRect(x: dbmLeft, y: 0, width: layout.size.width - dbmLeft, height: floor)
        let leftLimit = sideways ? EdgeMarkers.sidewaysInset : 0
        return place(target: targetSize(large: large), floor: floor, leftLimit: leftLimit, rightLimit: dbmLeft,
                     avoid: avoid + [dbm])
    }

    /// The search itself, the board's: each column from the right, top
    /// down, jumping below each thing in the way; the top right if the
    /// band has no room at all.
    static func place(target: CGSize, floor: CGFloat, leftLimit: CGFloat, rightLimit: CGFloat,
                      avoid: [CGRect]) -> CGRect {
        let xStart = rightLimit - gap - target.width
        let yMax = floor - gap - target.height
        let kept = avoid.filter { $0.width > 0 && $0.height > 0 }.map { $0.insetBy(dx: -gap, dy: -gap) }
        var x = xStart
        while x >= leftLimit + gap {
            var y = gap
            while y <= yMax {
                let box = CGRect(origin: CGPoint(x: x, y: y), size: target)
                guard let clash = kept.first(where: { overlaps(box, $0) }) else {
                    return box
                }
                y = max(y + stepDown, clash.maxY)
            }
            x -= stepAcross
        }
        return CGRect(origin: CGPoint(x: xStart, y: gap), size: target)
    }

    /// Whether two rectangles share any area (touching edges do not).
    static func overlaps(_ a: CGRect, _ b: CGRect) -> Bool {
        a.minX < b.maxX && a.maxX > b.minX && a.minY < b.maxY && a.maxY > b.minY
    }
}

/// The chip on the band, while the Core reports the first receiver input
/// as not Filtered. It reads the `radio` object the Filter policy lines read.
struct WideChipLayer: View {
    @ObservedObject var store: MirrorStore
    @ObservedObject var controls: FlagControls
    /// The chip's target on the band (``WideChip/rect(layout:avoid:sideways:trailingInset:large:)``).
    let rect: CGRect
    let large: Bool

    var body: some View {
        if let radio = store.object(FlagControls.radioKey) {
            Reading(radio: radio, controls: controls, rect: rect, large: large)
        } else {
            Hidden(controls: controls)
        }
    }

    private struct Reading: View {
        @ObservedObject var radio: MirrorObject
        @ObservedObject var controls: FlagControls
        let rect: CGRect
        let large: Bool

        var body: some View {
            if WideChip.shows(ReceiveFilterState(radio: radio.values)) {
                WideChipButton(expanded: controls.moreFromWideChip && controls.open != nil, large: large,
                               size: rect.size) {
                    controls.toggleFromWideChip()
                }
                .offset(x: rect.minX, y: rect.minY)
                .task(id: rect) {
                    controls.wideChipRect = rect
                }
            } else {
                Hidden(controls: controls)
            }
        }
    }

    /// No chip: the band as it is while the input is Filtered.
    private struct Hidden: View {
        let controls: FlagControls

        var body: some View {
            Color.clear
                .frame(width: 0, height: 0)
                .allowsHitTesting(false)
                .accessibilityHidden(true)
                .task {
                    controls.wideChipRect = nil
                }
        }
    }
}

/// The chip itself: the desktop pill's word and colours in the middle of
/// its 44-point target, ringed in the word's amber while its menu is open.
struct WideChipButton: View {
    let expanded: Bool
    let large: Bool
    let size: CGSize
    let action: () -> Void

    var body: some View {
        let pill = WideChip.pillSize(large: large)
        Button(action: action) {
            Text(WideChip.word)
                .font(.system(size: WideChip.wordPoints(large: large), weight: .bold, design: .monospaced))
                .tracking(0.5)
                .foregroundStyle(WideChip.text)
                .lineLimit(1)
                .fixedSize()
                .padding(.horizontal, 7)
                .frame(minWidth: pill.width, minHeight: pill.height, maxHeight: pill.height)
                .background(WideChip.ground, in: RoundedRectangle(cornerRadius: 3))
                .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(WideChip.edge, lineWidth: 1))
                .overlay {
                    if expanded {
                        RoundedRectangle(cornerRadius: 5)
                            .strokeBorder(WideChip.text, lineWidth: 2)
                            .padding(-2)
                    }
                }
                .frame(width: size.width, height: size.height)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(WideChip.accessibilityLabel)
        .accessibilityHint(WideChip.accessibilityHint)
        .accessibilityValue(expanded ? "Open" : "")
        .accessibilityIdentifier(WideChip.identifier)
    }
}
