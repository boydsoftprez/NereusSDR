// NereusSDR for iOS: the Digital Spectrum Recorder's hours map beside the time strip
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 28, the phone's and iPad's Digital Spectrum
//   Recorder. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// Every kept hour in one 14-point column (the phone mockup): now at the
/// top, the oldest kept at the foot, brighter where the band was busier,
/// the stretch erased next as a red edge at the foot and a white frame
/// where the picture is. A touch jumps the picture there (DsrStripLayer).
struct DsrHoursMap: View {
    /// The edge at the foot: what the recorder erases next.
    static let erasedNext = Color(red: 165 / 255, green: 48 / 255, blue: 44 / 255)
    static let erasedNextPoints: CGFloat = 3

    let model: DsrPanModel
    let size: CGSize

    var body: some View {
        Canvas { context, size in
            Self.draw(context, size: size, model: model)
        }
        .opacity(model.usable ? 1 : 0.42)
    }

    /// One point's colour for a level of activity, 0 quiet to 1 busy.
    static func colour(activity: Double) -> Color {
        let v = min(1, max(0, activity))
        return Color(red: (22 + v * 120) / 255, green: (24 + v * 140) / 255, blue: (34 + v * 170) / 255)
    }

    static func draw(_ context: GraphicsContext, size: CGSize, model: DsrPanModel) {
        let height = Int(size.height.rounded(.up))
        for y in 0..<max(0, height) {
            let fraction = Double(y) / max(1, Double(size.height))
            context.fill(Path(CGRect(x: 0, y: Double(y), width: size.width, height: 1)),
                         with: .color(colour(activity: model.activity(atMapFraction: fraction))))
        }
        guard model.usable else {
            return
        }
        context.fill(Path(CGRect(x: 0, y: size.height - erasedNextPoints, width: size.width, height: erasedNextPoints)),
                     with: .color(erasedNext))
        if let mark = model.mapMark {
            let top = mark.lowerBound * size.height
            let tall = max(4, (mark.upperBound - mark.lowerBound) * size.height)
            let y = min(size.height - 5 - tall, top)
            let frame = CGRect(x: 1.5, y: max(0, y) + 0.75, width: size.width - 3, height: tall)
            context.stroke(Path(frame), with: .color(.white), lineWidth: 1.5)
        }
    }
}
