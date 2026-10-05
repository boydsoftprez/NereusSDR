// NereusSDR for iOS: showing another slice's band: the Back to your band bar, and the markers at the band's edge
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// The bar at the band's top while it shows another pan's slice (R-IOS-42,
/// JJ's rulings of 2026-09-30, the board's `.sll-jumpbar`): one plain
/// "Back to your band" button, never a gesture alone, and whose band it
/// is. Back makes the slice the phone left active again and shows its band.
struct JumpBar: View {
    let entry: BandSlicesModel.Entry
    let sideways: Bool
    let back: () -> Void

    @Environment(\.dynamicTypeSize) private var typeSize

    static let backTitle = "Back to your band"
    /// How far the flags start below the band's top while the bar is up:
    /// its 4-point inset, its 44 points and a 4-point gap.
    static let clearance: CGFloat = 52
    static let inset: CGFloat = 4
    static let sidewaysInset: CGFloat = 66
    static let height: CGFloat = 44
    static let ground = Color(red: 10 / 255, green: 10 / 255, blue: 20 / 255).opacity(0.92)
    static let edge = Color(red: 0x30 / 255, green: 0x40 / 255, blue: 0x50 / 255)

    /// The bar's width before it is drawn, for keeping the flags clear:
    /// "Back to your band" and the slice's words at their size.
    static let estimatedWidth: CGFloat = 300
    static let largeEstimatedWidth: CGFloat = 420

    /// Where the bar sits on the band, before it is drawn.
    static func rect(sideways: Bool, large: Bool) -> CGRect {
        CGRect(x: sideways ? sidewaysInset : inset, y: inset, width: large ? largeEstimatedWidth : estimatedWidth,
               height: height)
    }

    /// The spectrum's least share of a band `bandHeight` points high while
    /// the bar is up upright, so the lowest flag under it, ending at
    /// `flagFoot`, stays above the frequency scale (JJ's choice of
    /// 2026-09-30 on the iPhone 17 and 17 Pro bands). The scale starts
    /// flush with the flag's foot: the board names no gap there. Nil when
    /// the pan's own split, `savedShare`, already clears it, as on the Pro
    /// Max bands, so nothing moves there. At most the split's upper limit.
    static func spectrumFloor(flagFoot: CGFloat, bandHeight: CGFloat, savedShare: CGFloat) -> CGFloat? {
        let shared = bandHeight - BandLayout.scaleHeightPoints
        guard flagFoot.isFinite, flagFoot > 0, shared > 0 else {
            return nil
        }
        let saved = BandLayout(size: CGSize(width: 1, height: bandHeight), scale: 1, stripPoints: 0,
                               spectrumShare: savedShare)
        let foot = flagFoot.rounded(.up)
        guard saved.frequencyScale.minY < foot else {
            return nil
        }
        return min(foot / shared, CGFloat(BandDisplaySettings.spectrumShareRange.upperBound / 100))
    }

    /// "Slice A’s band".
    static func bandText(_ letter: String) -> String {
        "Slice \(letter)\u{2019}s band"
    }

    var body: some View {
        let large = typeSize.isAccessibilitySize
        let colour = BandColours.slice(entry.slice.colour)
        HStack(spacing: 8) {
            Button(action: back) {
                HStack(spacing: 6) {
                    Text("\u{25C0}")
                        .font(.system(size: large ? 15 : 11))
                        .foregroundStyle(ChromeColours.accent)
                    Text(Self.backTitle)
                        .font(.system(size: large ? 19 : 13, weight: .bold))
                        .foregroundStyle(ChromeColours.textBright)
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)
                }
                .padding(.horizontal, 10)
                .frame(minWidth: 40, minHeight: Self.height)
                .background(ChromeColours.button)
                .overlay(alignment: .trailing) {
                    Rectangle().fill(Self.edge).frame(width: 1)
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Self.backTitle)
            .accessibilityIdentifier("backToYourBand")
            HStack(spacing: 6) {
                SliceLetterBadge(letter: entry.slice.letter, colour: colour, size: 20, points: 12)
                Text(Self.bandText(entry.slice.letter))
                    .font(.system(size: large ? 19 : 12.5, weight: .semibold))
                    .foregroundStyle(ChromeColours.text)
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
            }
            .padding(.trailing, 10)
            .accessibilityElement(children: .combine)
            .accessibilityIdentifier("jumpBarLabel")
        }
        .frame(height: Self.height)
        .background(Self.ground)
        .overlay(alignment: .leading) {
            Rectangle().fill(colour).frame(width: 3)
        }
        .clipShape(RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(Self.edge, lineWidth: 1))
        .fixedSize(horizontal: true, vertical: false)
        .offset(x: sideways ? Self.sidewaysInset : Self.inset, y: Self.inset)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("jumpBar")
    }
}

/// The slices this phone is in on another pan, outside the band's view,
/// each a marker at the band's edge on the side its frequency lies (the
/// board's `.sll-tag`): the arrow, the letter, the frequency and "MODE ·
/// Listening" (dashed in the slice's colour) or "MODE · You control"
/// (solid). They stack upward from the spectrum's foot and drop below any
/// flag they would cover. A tap shows the slice's band.
///
/// A stack too tall for the spectrum follows the flags' fold rule (D9):
/// the markers farthest from the foot fold to one line (the arrow, the
/// letter and the frequency), as few as it takes, the one nearest the foot
/// folding last. Markers that do not fit even folded go on down past the
/// spectrum's foot, so none leaves the band's top or lands on the dBm
/// scale's arrows, and every listened slice stays in sight (D120). A marker
/// that would land on one of the band's own controls (zoom, the dial, PTT)
/// moves in from the edge, clear of it.
struct EdgeMarkers: View {
    @ObservedObject var slices: BandSlicesModel
    let edges: [BandSlicesModel.EdgeMark]
    let spectrumFoot: CGFloat
    let bandWidth: CGFloat
    /// The flags and their round buttons, which the markers keep clear of.
    let avoid: [CGRect]
    let sideways: Bool
    let large: Bool
    /// The dBm scale's arrow buttons, which a stack on their side stays under.
    var dbmArrows: CGRect = .zero
    /// The band's own controls (zoom, the dial, PTT), which a marker moves in from the edge to clear.
    var controls: [CGRect] = []

    static let listeningText = "Listening"
    static let inset: CGFloat = 4
    static let sidewaysInset: CGFloat = 66
    static let height: CGFloat = 44
    static let largeHeight: CGFloat = 48
    /// The stack's pitch: 50 points, 56 in large type.
    static let pitch: CGFloat = 50
    static let largePitch: CGFloat = 56
    /// A marker's width before it is drawn, for keeping clear of the flags.
    static let width: CGFloat = 180
    static let largeWidth: CGFloat = 230
    /// A folded marker, as a folded flag (``FlagLayout/foldedHeight``) at
    /// every text size: its padding and border (18), the arrow and its gap
    /// (16), the letter badge and its gap (24), and the frequency.
    static let foldedFixedWidth: CGFloat = 58
    /// The most times a marker is moved clear of what it lands on.
    static let placementPasses = 32

    /// One marker's place: its rectangle on the band, and whether it is folded.
    struct Placement: Equatable {
        var rect: CGRect
        var folded: Bool
    }

    /// Each marker's rectangle on the band, in `edges`' order.
    static func rects(_ edges: [BandSlicesModel.EdgeMark], spectrumFoot: CGFloat, bandWidth: CGFloat,
                      avoid: [CGRect], sideways: Bool, large: Bool, dbmArrows: CGRect = .zero,
                      controls: [CGRect] = []) -> [CGRect] {
        placements(edges, spectrumFoot: spectrumFoot, bandWidth: bandWidth, avoid: avoid, sideways: sideways,
                   large: large, dbmArrows: dbmArrows, controls: controls).map(\.rect)
    }

    /// The width of a folded marker for `frequencyText`.
    static func foldedWidth(frequencyText: String) -> CGFloat {
        foldedFixedWidth + CGFloat(frequencyText.count) * FlagLayout.foldedCharacterWidth
    }

    /// Each marker's place on the band, in `edges`' order.
    static func placements(_ edges: [BandSlicesModel.EdgeMark], spectrumFoot: CGFloat, bandWidth: CGFloat,
                           avoid: [CGRect], sideways: Bool, large: Bool,
                           dbmArrows: CGRect = .zero, controls: [CGRect] = []) -> [Placement] {
        let height = large ? largeHeight : Self.height
        let gap = (large ? largePitch : pitch) - height
        let margin = sideways ? sidewaysInset : inset
        // Never below nothing: a band narrower than the margins (an iPad
        // column, a window being resized) draws the markers at no width.
        let fullWidth = max(0, min(large ? largeWidth : width, bandWidth - 2 * margin))
        // The room above the foot, under the dBm scale's arrows on their
        // side, and on each side how many of the stack fold (the fewest that
        // let it fit, from its top down) and how many stay above the foot.
        var folds: [Bool: Int] = [:]
        var above: [Bool: Int] = [:]
        for below in [false, true] {
            let reach = below ? margin...(margin + fullWidth) : (bandWidth - margin - fullWidth)...(bandWidth - margin)
            let underArrows = dbmArrows.height > 0 && reach.overlaps(dbmArrows.minX...dbmArrows.maxX)
            let room = spectrumFoot - inset - (underArrows ? dbmArrows.maxY + inset : inset)
            let count = edges.filter { $0.below == below }.count
            let stack = { (folded: Int) -> CGFloat in
                CGFloat(count - folded) * (height + gap) + CGFloat(folded) * (FlagLayout.foldedHeight + gap) - gap
            }
            if let folded = (0...count).first(where: { stack($0) <= room }) {
                folds[below] = folded
                above[below] = count
            } else {
                // Not even folded: as many as fit above, the rest below.
                folds[below] = count
                above[below] = max(0, Int(((room + gap) / (FlagLayout.foldedHeight + gap)).rounded(.down)))
            }
        }
        var placed: [Placement] = []
        var rank: [Bool: Int] = [:]
        var stackTop: [Bool: CGFloat] = [:]
        var stackFoot: [Bool: CGFloat] = [:]
        for edge in edges {
            let below = edge.below
            let count = edges.filter { $0.below == below }.count
            let index = rank[below, default: 0]
            rank[below] = index + 1
            let stays = index < above[below, default: count]
            let folded = !stays || index >= count - folds[below, default: 0]
            let markerWidth = folded
                ? max(0, min(foldedWidth(frequencyText: edge.entry.slice.frequencyText), bandWidth - 2 * margin))
                : fullWidth
            let markerHeight = folded ? FlagLayout.foldedHeight : height
            let x = below ? margin : bandWidth - margin - markerWidth
            let y: CGFloat
            if stays {
                // Upward from the spectrum's foot.
                y = stackTop[below, default: spectrumFoot - inset] - markerHeight
                stackTop[below] = y - gap
            } else {
                // On down past the foot.
                y = stackFoot[below, default: spectrumFoot + inset]
                stackFoot[below] = y + markerHeight + gap
            }
            var rect = CGRect(x: x, y: y, width: markerWidth, height: markerHeight)
            // Clear of every flag and marker (below the lowest it would
            // cover) and of the band's own controls (in from the edge, past
            // them; below one too wide to pass beside inside the band), all
            // checked again after each move until nothing moves.
            let others = avoid + placed.map(\.rect)
            for _ in 0..<Self.placementPasses {
                var moved = false
                for other in others where rect.intersects(other) {
                    rect.origin.y = other.maxY + inset
                    moved = true
                }
                for control in controls where rect.intersects(control) {
                    let past = below ? control.maxX + inset : control.minX - inset - rect.width
                    if past >= 0, past + rect.width <= bandWidth {
                        rect.origin.x = past
                    } else {
                        rect.origin = CGPoint(x: x, y: control.maxY + inset)
                    }
                    moved = true
                }
                if !moved {
                    break
                }
            }
            rect.origin.x = min(max(rect.minX, 0), max(0, bandWidth - rect.width))
            placed.append(Placement(rect: rect, folded: folded))
        }
        return placed
    }

    var body: some View {
        let placed = Self.placements(edges, spectrumFoot: spectrumFoot, bandWidth: bandWidth, avoid: avoid,
                                     sideways: sideways, large: large, dbmArrows: dbmArrows, controls: controls)
        ForEach(Array(zip(edges, placed)), id: \.0.id) { edge, place in
            EdgeMarker(edge: edge, large: large, folded: place.folded) { slices.show(edge.entry.id) }
                .frame(width: place.rect.width, alignment: edge.below ? .leading : .trailing)
                .offset(x: place.rect.minX, y: place.rect.minY)
        }
    }
}

/// One marker at the band's edge.
struct EdgeMarker: View {
    let edge: BandSlicesModel.EdgeMark
    let large: Bool
    /// Folded to one line: the arrow, the letter and the frequency (D9).
    var folded = false
    let show: () -> Void

    var body: some View {
        let entry = edge.entry
        let colour = BandColours.slice(entry.slice.colour)
        let words = (entry.modeLabel.isEmpty ? "" : entry.modeLabel + " \u{00B7} ")
            + (entry.listening ? EdgeMarkers.listeningText : SliceRoster.youControlText)
        Button(action: show) {
            if folded {
                foldedLine(entry, colour)
            } else {
                fullMarker(entry, colour, words)
            }
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Slice \(entry.slice.letter), \(entry.slice.frequencyText), \(words)")
        .accessibilityHint("Shows its band")
        .accessibilityIdentifier("edgeMarker\(entry.slice.letter)")
    }

    /// The folded marker: as a folded flag, fixed at every text size.
    private func foldedLine(_ entry: BandSlicesModel.Entry, _ colour: Color) -> some View {
        HStack(spacing: 6) {
            if edge.below {
                Text("\u{25C0}").font(.system(size: 11)).foregroundStyle(colour)
            }
            SliceLetterBadge(letter: entry.slice.letter, colour: colour)
            Text(entry.slice.frequencyText)
                .font(.system(size: FlagLayout.foldedFrequencyPoints, weight: .bold, design: .monospaced))
                .foregroundStyle(ChromeColours.textBright)
                .lineLimit(1)
                .fixedSize()
            if !edge.below {
                Text("\u{25B6}").font(.system(size: 11)).foregroundStyle(colour)
            }
        }
        .padding(.horizontal, 8)
        .frame(height: FlagLayout.foldedHeight)
        .background(JumpBar.ground, in: RoundedRectangle(cornerRadius: 4))
        .overlay {
            RoundedRectangle(cornerRadius: 4)
                .strokeBorder(colour, style: StrokeStyle(lineWidth: 1, dash: entry.listening ? [4, 3] : []))
        }
        .contentShape(Rectangle())
    }

    private func fullMarker(_ entry: BandSlicesModel.Entry, _ colour: Color, _ words: String) -> some View {
        HStack(spacing: 6) {
            if edge.below {
                arrow("\u{25C0}", colour)
            }
            SliceLetterBadge(letter: entry.slice.letter, colour: colour, size: 20, points: 12)
            VStack(alignment: .leading, spacing: 0) {
                Text(entry.slice.frequencyText)
                    .font(.system(size: large ? 19 : 14, weight: .bold, design: .monospaced))
                    .foregroundStyle(ChromeColours.textBright)
                Text(words)
                    .font(.system(size: large ? 15 : 11, weight: .semibold))
                    .foregroundStyle(colour)
            }
            .lineLimit(1)
            .minimumScaleFactor(0.7)
            if !edge.below {
                arrow("\u{25B6}", colour)
            }
        }
        .padding(.horizontal, 8)
        .frame(minHeight: large ? EdgeMarkers.largeHeight : EdgeMarkers.height)
        .background(JumpBar.ground, in: RoundedRectangle(cornerRadius: 4))
        .overlay {
            RoundedRectangle(cornerRadius: 4)
                .strokeBorder(colour, style: StrokeStyle(lineWidth: 1, dash: entry.listening ? [4, 3] : []))
        }
        .contentShape(Rectangle())
    }

    private func arrow(_ text: String, _ colour: Color) -> some View {
        Text(text)
            .font(.system(size: large ? 15 : 11))
            .foregroundStyle(colour)
    }
}
