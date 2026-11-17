// NereusSDR for iOS: under each full flag, the playing bar and the Kept notice, and what keeps them current
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 16, the phone's playing bar and Kept notice on the band.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// What hangs under each full flag for its slice's recordings: the playing
/// bar 4 points under the flag while this phone plays, and the Kept notice
/// (or a greyed control's reason) across the band under that, for six
/// seconds. It asks for each flag's last eight when the Core's list
/// changes, and tells the model when the Core keeps a recording.
struct RecordingFlagLayer: View {
    /// One full flag.
    struct Item: Identifiable, Equatable {
        let slice: Int
        let letter: String
        /// The flag, in the band's points.
        let flag: CGRect

        var id: Int { slice }
    }

    let model: RecordingFlagModel
    let flags: [Item]
    let bandWidth: CGFloat
    var openAll: (() -> Void)?
    var openSettings: (() -> Void)?

    var body: some View {
        ZStack(alignment: .topLeading) {
            ForEach(flags) { item in
                if let bar = model.bar(slice: item.slice) {
                    PlayingBar(bar: bar, letter: item.letter, others: model.others(slice: item.slice),
                               pick: { entry in Task { await model.play(entry, slice: item.slice) } },
                               stop: { Task { await model.stop() } }, openAll: openAll, openSettings: openSettings)
                        .offset(x: item.flag.minX, y: item.flag.maxY + PlayingBar.gap)
                }
                if let notice = model.notice(slice: item.slice) {
                    KeptNotice(notice: notice, letter: item.letter,
                               play: { Task { await model.playKept(slice: item.slice) } })
                        .frame(width: max(0, bandWidth - 2 * KeptNotice.inset))
                        .offset(x: KeptNotice.inset, y: item.flag.maxY + KeptNotice.below)
                }
            }
        }
        .task(id: listKey) {
            for item in flags {
                await model.refreshList(slice: item.slice)
            }
        }
        .onChange(of: model.client.kept?.seq, initial: true) {
            model.keptChanged()
        }
        .task(id: model.noticeSerial) {
            try? await Task.sleep(for: .seconds(RecordingFlagModel.noticeSeconds))
            guard !Task.isCancelled else {
                return
            }
            model.expireNotices()
        }
    }

    /// Moves on when the Core's list, the link or the flags change.
    private var listKey: String {
        "\(model.client.state)-\(model.client.index?.revision ?? -1)-" + flags.map { String($0.slice) }.joined(separator: ",")
    }
}
