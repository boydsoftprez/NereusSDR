// NereusSDR for iOS: the blue playing bar under the flag while this phone plays a recording, red while it goes on the air
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 16, the phone's playing bar.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import NereusBand
import NereusModels
import SwiftUI

/// The playing bar under a flag (design section 3, Phone flag look; 6.3),
/// shown only while this phone plays a recording: what plays and where it
/// is ("0:12 / 3:25 · speaker"), Others (the slice's last eight, All
/// recordings…, Recording settings…) and Stop, with a thin line for how
/// far through. While this phone's key sends it on the air the bar turns
/// red and reads "ON AIR · 0:12 / 3:25".
struct PlayingBar: View {
    let bar: RecordingFlagModel.Bar
    let letter: String
    /// Others' recordings, newest first.
    let others: [RecordingEntry]
    let pick: (RecordingEntry) -> Void
    let stop: () -> Void
    /// All recordings… and Recording settings…; nil leaves the item greyed.
    var openAll: (() -> Void)?
    var openSettings: (() -> Void)?

    /// The flag's width, 38 points tall, 4 points under the flag.
    static let size = CGSize(width: FlagLayout.flagSize.width, height: 38)
    static let gap: CGFloat = 4

    private static let ground = WorkspacePalette.rgb(0x12243C).opacity(0.96)
    private static let edge = WorkspacePalette.rgb(0x3A7BD5)
    private static let detailInk = WorkspacePalette.rgb(0xCFE2FF)
    private static let othersGround = WorkspacePalette.rgb(0x24466F)
    private static let stopInk = WorkspacePalette.rgb(0x1D5FB0)
    private static let line = WorkspacePalette.rgb(0x3A9CFF)
    private static let airGround = WorkspacePalette.rgb(0x46100D).opacity(0.97)
    private static let airEdge = WorkspacePalette.rgb(0xFF453A)
    private static let airDetailInk = WorkspacePalette.rgb(0xFFB3AD)
    private static let airStopInk = WorkspacePalette.rgb(0xC0392B)

    var body: some View {
        let onAir = bar.onAir
        HStack(spacing: 6) {
            VStack(alignment: .leading, spacing: 1) {
                Text(bar.title)
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(Color.white)
                    .lineLimit(1)
                    .truncationMode(.tail)
                // Proportional with even digits, so "ON AIR · 0:12 / 3:25" fits beside Others and Stop.
                Text(bar.detail)
                    .font(.system(size: 10).monospacedDigit())
                    .foregroundStyle(onAir ? Self.airDetailInk : Self.detailInk)
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .accessibilityElement(children: .combine)
            .accessibilityIdentifier("playingBarText\(letter)")
            Menu {
                if others.isEmpty {
                    Button("Nothing recorded on \(RecordingFlagModel.sliceName(sliceIndex)) yet") {}
                        .disabled(true)
                }
                ForEach(others) { entry in
                    Button {
                        pick(entry)
                    } label: {
                        Text(RecordingFlagModel.title(of: entry))
                        Text(RecordingFlagModel.lengthText(entry.lengthMs))
                    }
                }
                Divider()
                Button("All recordings…") { openAll?() }
                    .disabled(openAll == nil)
                Button("Recording settings…") { openSettings?() }
                    .disabled(openSettings == nil)
            } label: {
                chip("Others", ink: Self.detailInk, ground: Self.othersGround)
            }
            .accessibilityIdentifier("playingBarOthers\(letter)")
            Button(action: stop) {
                chip("Stop", ink: onAir ? Self.airStopInk : Self.stopInk, ground: .white)
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Stop playing")
            .accessibilityIdentifier("playingBarStop\(letter)")
        }
        .padding(.leading, 9)
        .padding(.trailing, 4)
        .frame(width: Self.size.width, height: Self.size.height)
        .background(onAir ? Self.airGround : Self.ground)
        .overlay(alignment: .bottomLeading) {
            Rectangle()
                .fill(onAir ? Self.airEdge : Self.line)
                .frame(width: Self.size.width * bar.fraction, height: 2)
        }
        .clipShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay {
            RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius)
                .strokeBorder(onAir ? Self.airEdge : Self.edge, lineWidth: 1)
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("playingBar\(letter)")
    }

    /// The slice's number from its letter, for the empty list's words.
    private var sliceIndex: Int {
        Int((letter.unicodeScalars.first?.value ?? 65) - 65)
    }

    private func chip(_ title: String, ink: Color, ground: Color) -> some View {
        Text(title)
            .font(.system(size: 11.5, weight: .semibold))
            .foregroundStyle(ink)
            .padding(.horizontal, 6)
            .frame(height: 28)
            .background(ground, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
            .contentShape(Rectangle())
    }
}
