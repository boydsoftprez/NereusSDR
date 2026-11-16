// NereusSDR for iOS: the buttons beside a full flag: close the slice, lock its frequency, Record, Play, and more
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 16: Record and Play join the column, now
//   gray icons with no box inside the flag's height. J.J. Boyd (KG4VCF),
//   AI-assisted via Claude Code.
// 2026-11-16: The armed Record ring drops its AUTO caption. J.J. Boyd
//   (KG4VCF), AI-assisted via Claude Code.

import NereusBand
import SwiftUI

/// The column of buttons beside a full flag, on its line's side
/// (``FlagLayout/sideColumnRect(flag:lineX:bandWidth:column:)``), as the
/// recording design of 2026-11-15 draws it for the phone: close, lock,
/// Record, Play and more, gray icons with no box, all inside the flag's
/// height (five rows 30 points tall, 44 across, 2 apart). Close the slice
/// (greyed on slice A, which always stays, and a tap says so), lock its
/// frequency (lit while locked), Record (blinks red with the time under
/// it while the Core records the slice; a hollow red ring while it
/// waits for a transmit), Play (a blue stop square with the position
/// under it while this phone plays, red while that goes on the air), and
/// more, which opens the desktop flag's right-click menu (lit while open).
/// Close, lock and Record ask the Core, which decides. A Record or Play
/// that cannot act is dimmed, never hidden, and its tap says why.
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
    /// Record and Play as the Core's recordings have them.
    var recording: RecordingFlagModel.Look = .idle
    /// Record blinks while it records; still for pictures.
    var blinks = true
    let close: () -> Void
    let toggleLock: () -> Void
    var record: () -> Void = {}
    var play: () -> Void = {}
    var more: () -> Void = {}

    @Environment(\.dynamicTypeSize) private var typeSize
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @State private var blinkLow = false

    static let gap = FlagLayout.sideGap

    /// The column's width: 44 points at every text size.
    static func diameter(large: Bool) -> CGFloat {
        large ? FlagLayout.largeSideButton : FlagLayout.sideButton
    }

    /// The icons' gray, as the phone mockup draws them.
    static let iconGray = WorkspacePalette.rgb(0x5C5C60)
    static let recordRed = WorkspacePalette.rgb(0xFF453A)
    static let recordTime = WorkspacePalette.rgb(0xFF8A84)
    static let armedRed = WorkspacePalette.rgb(0xFF6B62)
    static let playBlue = WorkspacePalette.rgb(0x3A9CFF)
    static let playTime = WorkspacePalette.rgb(0x8CC4FF)
    /// A Record or Play that cannot act.
    static let unavailable = 0.35

    /// Slice A always stays: its close is greyed.
    private var closable: Bool { letter != "A" }

    var body: some View {
        let large = typeSize.isAccessibilitySize
        VStack(spacing: Self.gap) {
            row(label: "Close slice \(letter)",
                hint: dimmedWords ?? (closable ? FlagControls.DesktopTip.close : FlagControls.sliceAStaysOpenText),
                lit: false, large: large, action: pressed(close)) {
                icon("xmark", lit: false)
            }
            .opacity(closable ? 1 : VfoFlagView.dimmed)
            .accessibilityIdentifier("flagClose\(letter)")
            row(label: locked ? "Unlock slice \(letter)" : "Lock slice \(letter)",
                hint: dimmedWords ?? FlagControls.DesktopTip.lock, lit: locked, large: large,
                action: pressed(toggleLock)) {
                icon(locked ? "lock.fill" : "lock.open", lit: locked)
            }
            .accessibilityIdentifier("flagLock\(letter)")
            row(label: recordLabel, hint: dimmedWords ?? recording.recordReason ?? recordHint,
                lit: recordLit, large: large, action: pressed(record)) {
                recordFace
            }
            .opacity(recording.recordReason == nil ? 1 : Self.unavailable)
            .accessibilityIdentifier("flagRecord\(letter)")
            row(label: playLabel, hint: dimmedWords ?? recording.playReason ?? playHint,
                lit: recording.play != .idle, large: large, action: pressed(play)) {
                playFace
            }
            .opacity(recording.playReason == nil ? 1 : Self.unavailable)
            .accessibilityIdentifier("flagPlay\(letter)")
            row(label: "More for slice \(letter)", hint: dimmedWords ?? "", lit: moreOpen, large: large,
                action: pressed(more)) {
                icon("ellipsis", lit: moreOpen)
            }
            .accessibilityIdentifier("flagMore\(letter)")
        }
        .opacity(dimmedWords == nil ? 1 : VfoFlagView.listened)
    }

    // MARK: Record and Play

    private var recordLit: Bool {
        if case .idle = recording.record {
            return false
        }
        return true
    }

    private var recordLabel: String {
        if case .recording = recording.record {
            return "Stop recording slice \(letter)"
        }
        return "Record slice \(letter)"
    }

    private var recordHint: String {
        switch recording.record {
        case .idle:
            return "Records what you hear, and your own voice when you transmit, on the Core."
        case .armed:
            return "Waits for you to transmit. Tap to record now."
        case .recording(let elapsed):
            return "Recording, " + RecordingFlagModel.lengthText(elapsed) + "."
        }
    }

    private var playLabel: String {
        recording.play == .idle ? "Play slice \(letter)'s latest recording" : "Stop playing"
    }

    private var playHint: String {
        switch recording.play {
        case .idle:
            return "Plays on this phone's speaker. Press PTT while it plays to send it on the air."
        case .playing:
            return "Playing on this phone's speaker."
        case .onAir:
            return "On the air."
        }
    }

    @ViewBuilder
    private var recordFace: some View {
        switch recording.record {
        case .idle:
            glyph(Circle().fill(Self.iconGray), size: 12, caption: nil)
        case .armed:
            glyph(Circle().strokeBorder(Self.armedRed, lineWidth: 2.2).padding(0.6), size: 11.6, caption: nil)
        case .recording(let elapsed):
            glyph(Circle().fill(Self.recordRed).opacity(blinkLow ? 0.35 : 1), size: 9.5,
                  caption: Text(RecordingFlagModel.lengthText(elapsed))
                    .font(.system(size: 9, weight: .semibold).monospacedDigit())
                    .foregroundStyle(Self.recordTime))
                .onAppear(perform: startBlink)
                .onDisappear { blinkLow = false }
        }
    }

    @ViewBuilder
    private var playFace: some View {
        switch recording.play {
        case .idle:
            glyph(Triangle().fill(Self.iconGray), size: 12, caption: nil)
        case .playing(let position):
            glyph(RoundedRectangle(cornerRadius: 1.2).fill(Self.playBlue), size: 8.5, // kept-radius: glyph
                  caption: Text(RecordingFlagModel.lengthText(position))
                    .font(.system(size: 9, weight: .semibold).monospacedDigit())
                    .foregroundStyle(Self.playTime))
        case .onAir(let position):
            glyph(RoundedRectangle(cornerRadius: 1.2).fill(Self.recordRed), size: 8.5, // kept-radius: glyph
                  caption: Text(RecordingFlagModel.lengthText(position))
                    .font(.system(size: 9, weight: .semibold).monospacedDigit())
                    .foregroundStyle(Self.recordTime))
        }
    }

    private func startBlink() {
        guard blinks, !reduceMotion else {
            return
        }
        withAnimation(.easeInOut(duration: 0.55).repeatForever(autoreverses: true)) {
            blinkLow = true
        }
    }

    // MARK: Parts

    /// A button's press: the Core's words on a listened slice, else its action.
    private func pressed(_ action: @escaping () -> Void) -> () -> Void {
        guard let dimmedWords else {
            return action
        }
        return { showReason(dimmedWords) }
    }

    /// A gray icon, in the slice's colour while lit.
    private func icon(_ systemImage: String, lit: Bool) -> some View {
        Image(systemName: systemImage)
            .font(.system(size: 15, weight: .semibold))
            .foregroundStyle(lit ? colour : Self.iconGray)
    }

    /// A drawn glyph `size` points across with its words under it.
    private func glyph<S: View>(_ shape: S, size: CGFloat, caption: Text?) -> some View {
        VStack(spacing: 1) {
            shape.frame(width: size, height: size)
            if let caption {
                caption.lineLimit(1).fixedSize()
            }
        }
    }

    private func row<Face: View>(label: String, hint: String, lit: Bool, large: Bool,
                                 action: @escaping () -> Void, @ViewBuilder face: () -> Face) -> some View {
        Button(action: action) {
            face()
                // Dark edges keep the gray readable over the spectrum, as the mockup's drop shadow.
                .shadow(color: .black, radius: 1.5)
                .shadow(color: .black, radius: 1.5)
                .frame(width: Self.diameter(large: large), height: FlagLayout.sideRowHeight)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(label)
        .accessibilityHint(hint)
        .accessibilityAddTraits(lit ? .isSelected : [])
    }
}

/// Play's triangle, pointing right.
private struct Triangle: Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        path.move(to: CGPoint(x: rect.minX + rect.width * 0.1, y: rect.minY))
        path.addLine(to: CGPoint(x: rect.maxX, y: rect.midY))
        path.addLine(to: CGPoint(x: rect.minX + rect.width * 0.1, y: rect.maxY))
        path.closeSubpath()
        return path
    }
}
