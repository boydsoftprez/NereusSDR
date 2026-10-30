// NereusSDR for iOS: exact-frequency presence remains independent of loaded RF reports.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-30: Bounded presence-stack acceptance correction.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import Testing
import UIKit
@testable import NereusSDR

@Suite("Roundtable presence stack")
struct RoundtablePresenceStackTests {
    private var tag: RoundtableTag {
        RoundtableTag(roomId: "frequency:lsb-7236400", hz: 7_236_400,
            label: "W1AW\nK4ABC (2)\nN3XYZ\nW4KZO\nKG4VCF", reportLines: [
                "Last talker: W4KZO", "Heard by dBm Age", "N4XYZ  -43  4 s",
                "K9XYZ  -61  2 s", "+2 · details"], members: 5)
    }

    @Test("loaded and unloaded reports preserve the same capped exact-frequency people")
    func loadedReportsKeepPresence() {
        let loaded = RoundtableTagLayout.contentLines(tag, width: 80, maxRows: 18)
        var unloaded = tag
        unloaded.reportLines = ["Read reports · details"]
        let waiting = RoundtableTagLayout.contentLines(unloaded, width: 80, maxRows: 18)
        for rows in [loaded, waiting] {
            #expect(rows.contains("W1AW") && rows.contains("K4ABC (2)"))
            #expect(rows.contains("+3 people"))
            #expect(!rows.contains("N3XYZ"))
        }
        #expect(loaded.contains("W4KZO") && loaded.contains("N4XYZ"))
        #expect(loaded.contains(where: { $0.hasPrefix("-43dBm ") }))
        #expect(loaded.contains("Heard by"))
        let font = UIFont.monospacedSystemFont(ofSize: 10, weight: .regular)
        #expect(loaded.allSatisfy { ($0 as NSString).size(withAttributes: [.font: font]).width <= 67 })
    }

    @Test("short legal height keeps people and an explicit reports affordance")
    func shortHeightUsesHonestDetails() {
        for limit in 0...18 {
            let rows = RoundtableTagLayout.contentLines(tag, width: 176, maxRows: limit)
            #expect(rows.count <= limit)
            #expect(RoundtableTagLayout.tagHeight(tag, width: 176, maxRows: limit)
                    == CGFloat(rows.count * 13 + 8))
            if limit >= 5 {
                #expect(rows.contains("W1AW") && rows.contains("K4ABC (2)"))
                #expect(rows.contains(where: { $0.contains("people") }))
                #expect(rows.last == "Reports ›")
            }
        }
        let compact = RoundtableTagLayout.contentLines(tag, width: 80, maxRows: 11)
        #expect(compact.contains("W1AW") && compact.contains("K4ABC (2)"))
        #expect(compact.contains("N4XYZ"))
        #expect(!compact.contains("K9XYZ"))
    }

    @Test("unknown and doubled signal descriptions do not replace presence")
    func unattributedSignalsKeepPresence() {
        for reports in [["Unknown signal", "Heard by dBm Age", "N4XYZ  -43  4 s", "Open details"],
                        ["Two stations keyed", "Combined RF · unattributed", "Open details"]] {
            var value = tag
            value.reportLines = reports
            let rows = RoundtableTagLayout.contentLines(value, width: 176, maxRows: 18)
            #expect(rows.contains("W1AW") && rows.contains("K4ABC (2)"))
            #expect(!rows.contains(where: { $0.hasPrefix("Last") }))
        }
    }
}
