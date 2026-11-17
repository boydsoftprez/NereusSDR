// NereusSDR for iOS: the band's words and numbers, drawn with Core Text into a layer the renderer shows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-29: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//   request the existing scale face by its canonical PostScript name.
// 2026-11-15: J.J. Boyd (KG4VCF), AI-assisted via Claude Code:
//   draw one label alone into its own picture, to move as a layer's would.

import CoreGraphics
import CoreText
import Foundation

/// Draws the band's labels (the dBm scale, the frequency scale, the strip's
/// segment names) into a premultiplied BGRA layer, top-left origin, in
/// pixels, or each label alone into its own picture (``glyph``), which the
/// renderer makes once and moves with the view.
enum BandLabels {
    enum Alignment: Sendable {
        case leading
        case centre
        case trailing
    }

    struct Item: Equatable, Sendable {
        var text: String
        /// Where the text is anchored across, in the layer's pixels.
        var x: CGFloat
        /// The text's vertical centre, in the layer's pixels.
        var y: CGFloat
        var alignment: Alignment
        var colour: SIMD4<Float>
        var points: CGFloat
        /// In the system's bold face (the strip's names, bold as the
        /// desktop's are) rather than the scales' ``fontName``.
        var bold = false
    }

    static let fontName = "Menlo-Regular"

    /// The width `text` takes, in pixels.
    static func width(of text: String, points: CGFloat, scale: CGFloat, bold: Bool = false) -> CGFloat {
        CGFloat(CTLineGetTypographicBounds(line(text, points: points, scale: scale, colour: SIMD4(1, 1, 1, 1),
                                                bold: bold),
                                           nil, nil, nil))
    }

    /// The ascent and descent of `text`, in pixels.
    static func verticalMetrics(of text: String, points: CGFloat, scale: CGFloat,
                                bold: Bool = false) -> (ascent: CGFloat, descent: CGFloat) {
        var ascent: CGFloat = 0
        var descent: CGFloat = 0
        _ = CTLineGetTypographicBounds(line(text, points: points, scale: scale, colour: SIMD4(1, 1, 1, 1), bold: bold),
                                       &ascent, &descent, nil)
        return (ascent, descent)
    }

    /// The layer's bytes, `width` × `height` pixels, BGRA premultiplied.
    static func render(width: Int, height: Int, scale: CGFloat, items: [Item]) -> [UInt8] {
        let bytesPerRow = width * 4
        var bytes = [UInt8](repeating: 0, count: max(0, bytesPerRow * height))
        guard width > 0, height > 0, !items.isEmpty else {
            return bytes
        }
        let space = CGColorSpace(name: CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB()
        let info = CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue
        bytes.withUnsafeMutableBytes { raw in
            guard let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8,
                                          bytesPerRow: bytesPerRow, space: space, bitmapInfo: info) else {
                return
            }
            // Top-left origin, with text the right way up.
            context.translateBy(x: 0, y: CGFloat(height))
            context.scaleBy(x: 1, y: -1)
            context.textMatrix = CGAffineTransform(scaleX: 1, y: -1)
            context.setShouldAntialias(true)
            for item in items {
                let line = line(item.text, points: item.points, scale: scale, colour: item.colour, bold: item.bold)
                var ascent: CGFloat = 0
                var descent: CGFloat = 0
                let textWidth = CGFloat(CTLineGetTypographicBounds(line, &ascent, &descent, nil))
                let x: CGFloat
                switch item.alignment {
                case .leading:
                    x = item.x
                case .centre:
                    x = item.x - textWidth / 2
                case .trailing:
                    x = item.x - textWidth
                }
                // The baseline that centres the glyphs' height on y.
                context.textPosition = CGPoint(x: x.rounded(), y: (item.y + (ascent - descent) / 2).rounded())
                CTLineDraw(line, context)
            }
        }
        return bytes
    }

    /// One label drawn alone: its pixels, BGRA premultiplied, and where its
    /// line starts in them. Put with ``origin`` at a whole pixel, its pixels
    /// are the ones ``render(width:height:scale:items:)`` draws for it in a
    /// layer, since that starts each line at a whole pixel too.
    struct Glyph {
        var width: Int
        var height: Int
        var bytes: [UInt8]
        /// Where the line starts across, and its baseline, in its pixels.
        var lineX: Int
        var baseline: Int
        /// The line's width, ascent and descent, as ``render`` measures them.
        var textWidth: CGFloat
        var ascent: CGFloat
        var descent: CGFloat
        /// Every pixel the line can touch, its ink and the edges' padding,
        /// in its pixels; null for a line with no ink.
        var ink: CGRect

        /// Where the glyph's top-left goes in a layer to draw `item` there,
        /// in the layer's pixels.
        func origin(for item: Item) -> CGPoint {
            let x: CGFloat
            switch item.alignment {
            case .leading:
                x = item.x
            case .centre:
                x = item.x - textWidth / 2
            case .trailing:
                x = item.x - textWidth
            }
            return CGPoint(x: x.rounded() - CGFloat(lineX),
                           y: (item.y + (ascent - descent) / 2).rounded() - CGFloat(baseline))
        }
    }

    /// Pixels kept clear round a glyph's ink, for its edges' antialiasing.
    static let glyphPadPixels: CGFloat = 2

    /// `text` drawn alone, in its look, with room for all of its ink.
    static func glyph(_ text: String, points: CGFloat, scale: CGFloat, colour: SIMD4<Float>, bold: Bool) -> Glyph {
        let line = line(text, points: points, scale: scale, colour: colour, bold: bold)
        var ascent: CGFloat = 0
        var descent: CGFloat = 0
        let textWidth = CGFloat(CTLineGetTypographicBounds(line, &ascent, &descent, nil))
        // The ink, from the line's start and baseline, y up.
        let ink = CTLineGetBoundsWithOptions(line, .useGlyphPathBounds)
        let left = Int((min(0, ink.isNull ? 0 : ink.minX) - glyphPadPixels).rounded(.down))
        let right = Int((max(textWidth, ink.isNull ? 0 : ink.maxX) + glyphPadPixels).rounded(.up))
        let above = Int((max(ascent, ink.isNull ? 0 : ink.maxY) + glyphPadPixels).rounded(.up))
        let below = Int((max(descent, ink.isNull ? 0 : -ink.minY) + glyphPadPixels).rounded(.up))
        let width = max(1, right - left)
        let height = max(1, above + below)
        let lineX = -left
        let bytesPerRow = width * 4
        var bytes = [UInt8](repeating: 0, count: bytesPerRow * height)
        let space = CGColorSpace(name: CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB()
        let info = CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue
        bytes.withUnsafeMutableBytes { raw in
            guard let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8,
                                          bytesPerRow: bytesPerRow, space: space, bitmapInfo: info) else {
                return
            }
            // As ``render`` sets its layer up.
            context.translateBy(x: 0, y: CGFloat(height))
            context.scaleBy(x: 1, y: -1)
            context.textMatrix = CGAffineTransform(scaleX: 1, y: -1)
            context.setShouldAntialias(true)
            context.textPosition = CGPoint(x: CGFloat(lineX), y: CGFloat(above))
            CTLineDraw(line, context)
        }
        let touched = ink.isNull ? CGRect.null
            : CGRect(x: CGFloat(lineX) + ink.minX - glyphPadPixels, y: CGFloat(above) - ink.maxY - glyphPadPixels,
                     width: ink.width + 2 * glyphPadPixels, height: ink.height + 2 * glyphPadPixels)
        return Glyph(width: width, height: height, bytes: bytes, lineX: lineX, baseline: above,
                     textWidth: textWidth, ascent: ascent, descent: descent, ink: touched)
    }

    private static func line(_ text: String, points: CGFloat, scale: CGFloat, colour: SIMD4<Float>,
                             bold: Bool) -> CTLine {
        let font = bold ? boldFont(size: points * scale) : CTFontCreateWithName(fontName as CFString, points * scale, nil)
        let cgColour = CGColor(srgbRed: CGFloat(colour.x), green: CGFloat(colour.y), blue: CGFloat(colour.z),
                               alpha: CGFloat(colour.w))
        let attributes: [NSAttributedString.Key: Any] = [
            NSAttributedString.Key(kCTFontAttributeName as String): font,
            NSAttributedString.Key(kCTForegroundColorAttributeName as String): cgColour,
        ]
        return CTLineCreateWithAttributedString(NSAttributedString(string: text, attributes: attributes))
    }

    /// The system's bold face at `size` pixels.
    private static func boldFont(size: CGFloat) -> CTFont {
        CTFontCreateUIFontForLanguage(.emphasizedSystem, size, nil)
            ?? CTFontCreateWithName("Helvetica-Bold" as CFString, size, nil)
    }
}
