// NereusSDR for iOS: tests for the desktop's band names by the Core's band number
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusMirror

@Suite struct SetupBandNameTests {
    @Test("bands 0 to 27 are named as the desktop names them, and no other")
    func names() {
        #expect(SetupDescription.bandName(0) == "160m")
        #expect(SetupDescription.bandName(13) == "XVTR")
        // src/models/Band.cpp:195-207: the shortwave broadcast bands, 14 to 26.
        let swl: [String?] = ["120m", "90m", "61m", "49m", "41m", "31m", "25m", "22m", "19m", "16m", "14m", "13m", "11m"]
        #expect((14...26).map(SetupDescription.bandName) == swl)
        #expect(SetupDescription.bandName(14) == "120m")
        #expect(SetupDescription.bandName(26) == "11m")
        #expect(SetupDescription.bandName(27) == "2m")
        #expect(SetupDescription.bandName(28) == nil)
        #expect(SetupDescription.bandName(-1) == nil)
        // The closed per-band list keeps its fourteen.
        #expect(SetupDescription.bandNames.count == 14)
    }
}
