// NereusSDR for iOS: the Core's paired devices, read as its devices object writes them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMirror

/// R-IOS-08: `devices.listJson` (link document section 7.1) read in
/// pairing order, with its two times as dates and "" as never.
@Suite struct StationDevicesTests {
    @Test func theListReadsInPairingOrderWithItsTimes() throws {
        let text = """
        [{"id":"aaa","name":"JJ's iPhone","shortName":"iPhone","kind":"phone","pairedAt":"2026-08-02T14:00:00Z",\
        "lastSeen":"2026-09-26T17:00:00Z","connected":true},\
        {"id":"bbb","name":"Old ThinkPad","shortName":"Computer","kind":"computer","pairedAt":"2026-06-30T09:30:00.250Z",\
        "lastSeen":"","connected":false}]
        """
        let devices = StationDevices.pairedDevices(fromListJson: text)
        #expect(devices.map(\.id) == ["aaa", "bbb"])
        #expect(devices.map(\.name) == ["JJ's iPhone", "Old ThinkPad"])
        #expect(devices.map(\.kind) == ["phone", "computer"])
        #expect(devices.map(\.connected) == [true, false])
        let paired = try #require(devices[0].pairedAt)
        #expect(paired == Date(timeIntervalSince1970: 1_785_679_200))
        #expect(devices[1].pairedAt != nil)
        #expect(devices[1].lastSeen == nil)
    }

    @Test func anythingButTheListReadsAsNoDevices() {
        #expect(StationDevices.pairedDevices(fromListJson: "").isEmpty)
        #expect(StationDevices.pairedDevices(fromListJson: "{}").isEmpty)
        #expect(StationDevices.pairedDevices(fromListJson: "[{\"name\":\"no id\"}]").isEmpty)
    }
}
