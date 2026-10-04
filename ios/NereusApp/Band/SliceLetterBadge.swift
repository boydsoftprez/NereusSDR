// NereusSDR for iOS: a slice's letter on its own colour, on the flag and the folded tag
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The slice's letter in white on the slice's colour, 18 points square
/// (24 on a flag in large type, the letter at 19 points).
struct SliceLetterBadge: View {
    let letter: String
    let colour: Color
    var size: CGFloat = 18
    var points: CGFloat = 11

    var body: some View {
        Text(letter)
            .font(.system(size: points, weight: .bold))
            .foregroundStyle(.white)
            .frame(width: size, height: size)
            .background(colour, in: RoundedRectangle(cornerRadius: 3))
    }
}
