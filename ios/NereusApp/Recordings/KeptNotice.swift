// NereusSDR for iOS: the notice under a flag when a recording is kept, or why its Record or Play cannot act
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 16, the phone's Kept notice.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-19: belowWaterfallTop, its place at the waterfall's top (JJ's
//   phone run of PR 808). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// The notice under a flag for six seconds (design 6.2 item 3, and the
/// recording-together mockup's phone half): "Kept in Recordings: <name>,
/// <length>", why it stopped in the desktop flag's words ("Stopped on the
/// iPhone", "Stopped by itself after two quiet minutes", "Stopped: the disk
/// is full", "Kept as WAV: the MP3 encoder failed") and Play, which plays
/// it on this phone. The same place carries the reason a greyed Record or
/// Play gives when tapped, in the tool pages' reason style (V9).
struct KeptNotice: View {
    let notice: RecordingFlagModel.Notice
    let letter: String
    let play: () -> Void

    /// 12 points in from each side of the band, as the mockup places it.
    static let inset: CGFloat = 12
    /// Under the flag, past where the playing bar would be.
    static let below: CGFloat = PlayingBar.gap + PlayingBar.size.height + 7
    /// Below the waterfall's top, clear of the recorder's play label there
    /// (DsrPictureLayer.playLabelRoom), where it sits since PR 808 round B.
    static let belowWaterfallTop: CGFloat = 1 + DsrPictureLayer.playLabelInset.height
        + DsrPictureLayer.playLabelSize.height + 4

    private static let ground = WorkspacePalette.rgb(0x242428).opacity(0.97)
    private static let edge = WorkspacePalette.rgb(0x48484E)
    private static let text = WorkspacePalette.rgb(0xEEEEEE)
    private static let small = WorkspacePalette.rgb(0xA8A8AE)
    /// The desktop Kept notice's Play: a deep blue with the workspace accent edge.
    private static let playGround = WorkspacePalette.rgb(0x1E3D5F)

    var body: some View {
        Group {
            switch notice {
            case .kept(_, let title, let reason):
                HStack(spacing: 8) {
                    VStack(alignment: .leading, spacing: 1) {
                        Text(title)
                            .font(.system(size: 13))
                            .foregroundStyle(Self.text)
                            .fixedSize(horizontal: false, vertical: true)
                        Text(reason)
                            .font(.system(size: 11.5))
                            .foregroundStyle(Self.small)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityElement(children: .combine)
                    .accessibilityIdentifier("keptNoticeText\(letter)")
                    Button(action: play) {
                        Text("Play")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(Color.white)
                            .padding(.horizontal, 10)
                            .frame(minWidth: 44, minHeight: 32)
                            .background(Self.playGround, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                            .overlay {
                                RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius)
                                    .strokeBorder(WorkspacePalette.accentFill, lineWidth: 1)
                            }
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("Play the recording just kept")
                    .accessibilityIdentifier("keptNoticePlay\(letter)")
                }
                .padding(.leading, 12)
                .padding(.trailing, 8)
                .padding(.vertical, 8)
            case .reason(let text):
                ToolPageParts.Reason(text: text, identifier: "recordingReason\(letter)")
            }
        }
        .background(Self.ground, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay {
            RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius)
                .strokeBorder(Self.edge, lineWidth: 1)
        }
        .shadow(color: .black.opacity(0.6), radius: 15, y: 10)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("keptNotice\(letter)")
    }
}
