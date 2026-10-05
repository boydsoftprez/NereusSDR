// NereusSDR for iOS: the band's words and numbers, drawn with Core Text into a layer the renderer shows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import CoreText
import Foundation

/// Draws the band's labels (the dBm scale, the frequency scale, the strip's
/// segment names) into a premultiplied BGRA layer, top-left origin, in
/// pixels. The renderer draws a layer again only when its labels change.
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

    static let fontName = "Menlo"

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
