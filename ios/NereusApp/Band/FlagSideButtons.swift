// NereusSDR for iOS: the round buttons beside a full flag: close the slice, lock its frequency, and more
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The column of round buttons beside a full flag, on its line's side
/// (``FlagLayout/sideColumnRect(flag:lineX:bandWidth:column:)``), as the
/// board's redrawn flag draws them: dark discs 44 points across at every
/// text size, 2 points apart. Close the slice (greyed on slice A, which always
/// stays, and a tap says so), lock its frequency (lit while locked), and
/// more, which opens the desktop flag's right-click menu (lit while open).
/// Close and lock ask the Core, which decides.
struct FlagSideButtons: View {
    let letter: String
    let colour: Color
    let locked: Bool
    /// On a slice this phone only listens to, the Core's line for who
    /// controls it: the buttons are dimmed and a tap on one gives these
    /// words instead (R-IOS-42, never hidden).
    var dimmedWords: String? = nil
    var showReason: (String) -> Void = { _ in }
    var moreOpen = false
    let close: () -> Void
    let toggleLock: () -> Void
    var more: () -> Void = {}

    @Environment(\.dynamicTypeSize) private var typeSize

    static let gap = FlagLayout.sideGap

    /// The buttons' size: 44 points at every text size.
    static func diameter(large: Bool) -> CGFloat {
        large ? FlagLayout.largeSideButton : FlagLayout.sideButton
    }

    /// Slice A always stays: its close is greyed.
    private var closable: Bool { letter != "A" }

    var body: some View {
        let large = typeSize.isAccessibilitySize
        VStack(spacing: Self.gap) {
            round(systemImage: "xmark", label: "Close slice \(letter)",
                  hint: dimmedWords ?? (closable ? FlagControls.DesktopTip.close : FlagControls.sliceAStaysOpenText),
                  lit: false, large: large, action: pressed(close))
                .opacity(closable ? 1 : VfoFlagView.dimmed)
                .accessibilityIdentifier("flagClose\(letter)")
            round(systemImage: locked ? "lock.fill" : "lock.open", label: locked ? "Unlock slice \(letter)" : "Lock slice \(letter)",
                  hint: dimmedWords ?? FlagControls.DesktopTip.lock, lit: locked, large: large,
                  action: pressed(toggleLock))
                .accessibilityIdentifier("flagLock\(letter)")
            round(systemImage: "ellipsis", label: "More for slice \(letter)", hint: dimmedWords ?? "", lit: moreOpen,
                  large: large, action: pressed(more))
                .accessibilityIdentifier("flagMore\(letter)")
        }
        .opacity(dimmedWords == nil ? 1 : VfoFlagView.listened)
    }

    /// A button's press: the Core's words on a listened slice, else its action.
    private func pressed(_ action: @escaping () -> Void) -> () -> Void {
        guard let dimmedWords else {
            return action
        }
        return { showReason(dimmedWords) }
    }

    private func round(systemImage: String, label: String, hint: String, lit: Bool, large: Bool,
                       action: @escaping () -> Void) -> some View {
        let size = Self.diameter(large: large)
        return Button(action: action) {
            Image(systemName: systemImage)
                .font(.system(size: large ? 19 : 15, weight: .semibold))
                .foregroundStyle(lit ? colour : BandColours.text)
                .frame(width: size, height: size)
                .background(BandColours.flagBackground, in: Circle())
                .overlay {
                    if lit {
                        Circle().strokeBorder(colour, lineWidth: 2)
                    } else {
                        // The board's disc edge, as the flag's own border.
                        Circle().strokeBorder(BandColours.flagBorder, lineWidth: 1)
                    }
                }
                .contentShape(Circle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(label)
        .accessibilityHint(hint)
        .accessibilityAddTraits(lit ? .isSelected : [])
    }
}
