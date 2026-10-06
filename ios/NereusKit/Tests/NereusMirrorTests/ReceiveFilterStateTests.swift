// NereusSDR for iOS: the first receiver input's filter state and its low-pass reason, read from the radio object
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-18, R-IOS-16: the phone reads the first input's filter state as
/// the desktop's remote window does (all four chain properties, in range),
/// and the shared-input low-pass reason and slice when the Core sends them
/// (`rxFilterLowPass` 1), the Core's words as sent.
@Suite struct ReceiveFilterStateTests {
    static let lowPassWords = "The receive low-pass filter is set for slice B on 20m, the highest band on this "
        + "receiver input. Slice A on 80m shares the input, so it has less protection from strong signals on "
        + "higher bands."
    static let highPassWords = "The broadcast-band high-pass filter is off because slice A on 160m needs it off."

    static func radio(effective: Int64 = 1, reason: String = "BYPASS (multi-band: 80m + 20m)",
                      lowPass: String? = nil, slice: Int64? = nil) -> [String: MirrorValue] {
        var values: [String: MirrorValue] = [
            "rxFilter0Mode": MirrorValue(.i64(0)),
            "rxFilter0Effective": MirrorValue(.i64(effective)),
            "rxFilter0Band": MirrorValue(.i64(5)),
            "rxFilter0Reason": MirrorValue(.utf8(reason)),
            "txInhibitReason": MirrorValue(.utf8("")),
        ]
        if let lowPass {
            values["rxFilter0LowPassReason"] = MirrorValue(.utf8(lowPass))
        }
        if let slice {
            values["rxFilter0LowPassSlice"] = MirrorValue(.i64(slice))
        }
        return values
    }

    @Test func theWideReasonAsSent() throws {
        let state = try #require(ReceiveFilterState(radio: Self.radio()))
        #expect(state == ReceiveFilterState(effective: .bypass, reason: "BYPASS (multi-band: 80m + 20m)"))
        #expect(ReceiveFilterState(radio: Self.radio(effective: 0, reason: "20m"))?.effective == .filtered)
        #expect(ReceiveFilterState(radio: Self.radio(effective: 2))?.effective == .widebandLocked)
    }

    @Test func aLowPassReasonNamingASlice() throws {
        let state = try #require(ReceiveFilterState(radio: Self.radio(lowPass: Self.lowPassWords, slice: 1)))
        #expect(state.lowPassReason == Self.lowPassWords)
        #expect(state.lowPassSliceId == 1)
    }

    @Test func theHighPassSentenceAloneNamesNoSlice() throws {
        let state = try #require(ReceiveFilterState(radio: Self.radio(effective: 0, reason: "160m",
                                                                      lowPass: Self.highPassWords, slice: -1)))
        #expect(state.lowPassReason == Self.highPassWords)
        #expect(state.lowPassSliceId == nil)
        // The HL2 board off because the top slice's band has no pins: the WIDE reason alone.
        let noPins = try #require(ReceiveFilterState(radio: Self.radio(
            reason: "BYPASS (slice B on WWV has no filter pins)", lowPass: "", slice: -1)))
        #expect(noPins.reason == "BYPASS (slice B on WWV has no filter pins)")
        #expect(noPins.lowPassReason == nil && noPins.lowPassSliceId == nil)
    }

    @Test func aCoreThatSendsNoLowPassStillShowsItsChain() throws {
        let state = try #require(ReceiveFilterState(radio: Self.radio()))
        #expect(state.lowPassReason == nil && state.lowPassSliceId == nil)
    }

    @Test func theChainWaitsForItsFourPropertiesInRange() {
        for name in ["rxFilter0Mode", "rxFilter0Effective", "rxFilter0Band", "rxFilter0Reason"] {
            var values = Self.radio()
            values[name] = nil
            #expect(ReceiveFilterState(radio: values) == nil, "\(name)")
        }
        var values = Self.radio()
        values["rxFilter0Mode"] = MirrorValue(.i64(3))
        #expect(ReceiveFilterState(radio: values) == nil)
        #expect(ReceiveFilterState(radio: Self.radio(effective: 3)) == nil)
        #expect(ReceiveFilterState(radio: Self.radio(reason: String(repeating: "x", count: 513))) == nil)
        #expect(ReceiveFilterState(radio: Self.radio(reason: String(repeating: "x", count: 512))) != nil)
    }

    @Test func aLowPassOutOfRangeIsNotTaken() throws {
        let long = try #require(ReceiveFilterState(radio: Self.radio(lowPass: String(repeating: "x", count: 513),
                                                                     slice: 64)))
        #expect(long.lowPassReason == nil && long.lowPassSliceId == nil)
    }
}
