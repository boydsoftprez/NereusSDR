// NereusSDR for iOS: the positions of the Core's newest capabilities and radio properties in the link surface
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Testing
@testable import NereusLink

/// R-IOS-16: the phone reads the transmit inhibit reason, the PA profile
/// version and the Alex-1 low-pass in use by the positions the Core's
/// surface pins. A Core that moves them
/// breaks this test before it breaks a radio.
@Suite struct LinkSurfaceOrdinalTests {
    private static func surface() throws -> [String: Any] {
        try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
    }

    @Test func newestCapabilitiesSitWhereTheSurfacePinsThem() throws {
        let capabilities = try #require(try Self.surface()["capabilities"] as? [[String: Any]])
        #expect(capabilities.count == 107)
        // The merged b44d638 surface inserts diversityControlVersion after
        // diversityPatternVersion, shifting every later capability by one.
        // Radio Mic 2 changes its value, without moving any other entry.
        let expected: [(index: Int, name: String, kind: String)] = [
            (88, "diversityControlVersion", "i64"),
            (95, "paProfileVersion", "i64"),
            (97, "txInhibitReasonVersion", "i64"),
            (98, "paTransmitBandVersion", "i64"),
            (99, "sliceAccessVersion", "i64"),
            (100, "mediaDirectVersion", "i64"),
            (101, "mediaStunUrls", "utf8"),
            (102, "rx2AttenuatorVersion", "i64"),
            (103, "radioMicVersion", "i64"),
            (104, "rxFilterLowPassVersion", "i64"),
            (105, "radeReasonVersion", "i64"),
            (106, "coreBuildInfo", "utf8"),
        ]
        // Core trunk b26112687: sliceAccessVersion reads 3 (the Core's own
        // slice can be taken); nothing moved.
        #expect((capabilities[99]["value"] as? NSNumber)?.intValue == 3)
        #expect((capabilities[88]["value"] as? NSNumber)?.intValue == 1)
        #expect((capabilities[103]["value"] as? NSNumber)?.intValue == 2)
        #expect(Set(capabilities.compactMap { $0["name"] as? String }).count == 107)
        for entry in expected {
            try #require(capabilities.count > entry.index)
            #expect(capabilities[entry.index]["name"] as? String == entry.name, "\(entry.name)")
            #expect(capabilities[entry.index]["kind"] as? String == entry.kind, "\(entry.name)")
        }
    }

    @Test func radioTransmitInhibitPropertiesKeepTheirOrdinals() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let radio = try #require(classes["RadioModel"] as? [String: Any])
        let properties = try #require(radio["properties"] as? [[String: Any]])
        func property(_ name: String) -> [String: Any]? { properties.first { $0["name"] as? String == name } }
        let inhibited = try #require(property("txInhibited"))
        #expect(inhibited["kind"] as? String == "bool")
        #expect((inhibited["ordinal"] as? NSNumber)?.intValue == 20)
        let reason = try #require(property("txInhibitReason"))
        #expect(reason["kind"] as? String == "utf8")
        #expect((reason["ordinal"] as? NSNumber)?.intValue == 28)
    }

    /// The Alex-1 low-pass in use, read only. The PA on-the-air band
    /// (ordinal 30, from c13abe564) now follows it; the phone does not read it.
    /// The level calibration's four (31 to 34, from 5cdb9a1ab) follow that.
    @Test func radioLowPassBitsKeepTheirOrdinal() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let radio = try #require(classes["RadioModel"] as? [String: Any])
        let properties = try #require(radio["properties"] as? [[String: Any]])
        #expect(properties.count == 38)
        #expect(properties.compactMap { ($0["ordinal"] as? NSNumber)?.intValue }.sorted() == Array(0..<38))
        let diversity = try #require(properties.first { $0["name"] as? String == "diversityState" })
        #expect(diversity["kind"] as? String == "utf8")
        #expect(diversity["direction"] as? String == "outbound")
        #expect((diversity["ordinal"] as? NSNumber)?.intValue == 37)
        let band = try #require(properties.first { $0["name"] as? String == "paTransmitBand" })
        #expect((band["ordinal"] as? NSNumber)?.intValue == 30)
        let bits = try #require(properties.first { $0["name"] as? String == "alexLpfBits" })
        #expect(bits["kind"] as? String == "i64")
        #expect(bits["direction"] as? String == "outbound")
        #expect((bits["ordinal"] as? NSNumber)?.intValue == 29)
    }

    /// Why the receive low-pass on the first input is set for another slice
    /// (Core trunk 75cef2f4b), sent only to a peer that declares
    /// `rxFilterLowPass`, which the phone does: appended after the level
    /// calibration's four, read only; the chain's own four keep theirs.
    @Test func lowPassReasonFollowsTheLevelCalibration() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let radio = try #require(classes["RadioModel"] as? [String: Any])
        let properties = try #require(radio["properties"] as? [[String: Any]])
        let expected: [(ordinal: Int, name: String, kind: String)] = [
            (11, "rxFilter0Mode", "i64"),
            (12, "rxFilter0Effective", "i64"),
            (13, "rxFilter0Band", "i64"),
            (14, "rxFilter0Reason", "utf8"),
            (35, "rxFilter0LowPassReason", "utf8"),
            (36, "rxFilter0LowPassSlice", "i64"),
        ]
        for entry in expected {
            let property = try #require(properties.first { $0["name"] as? String == entry.name })
            #expect((property["ordinal"] as? NSNumber)?.intValue == entry.ordinal, "\(entry.name)")
            #expect(property["kind"] as? String == entry.kind, "\(entry.name)")
            #expect(property["direction"] as? String == "outbound", "\(entry.name)")
        }
        let capabilities = try #require(try Self.surface()["capabilities"] as? [[String: Any]])
        let version = try #require(capabilities.first { $0["name"] as? String == "rxFilterLowPassVersion" })
        #expect((version["value"] as? NSNumber)?.intValue == 1)
    }

    /// The per-band power tables are the Core's alone (outbound from
    /// c13abe564), as the phone has always treated them: it never writes them.
    @Test func perBandPowerTablesAreTheCoresAlone() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let transmit = try #require(classes["TransmitModel"] as? [String: Any])
        let properties = try #require(transmit["properties"] as? [[String: Any]])
        for name in ["powerByBandJson", "tunePowerByBandJson"] {
            let property = try #require(properties.first { $0["name"] as? String == name })
            #expect(property["direction"] as? String == "outbound", "\(name)")
        }
    }

    /// The level calibration's run state (5cdb9a1ab), sent only to a peer
    /// that declares `levelCalibration`, which the phone does: appended
    /// after the PA on-the-air band, so no property the phone reads moved.
    @Test func levelCalibrationPropertiesFollowThePaTransmitBand() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let radio = try #require(classes["RadioModel"] as? [String: Any])
        let properties = try #require(radio["properties"] as? [[String: Any]])
        let expected: [(ordinal: Int, name: String, kind: String)] = [
            (31, "levelCalRunning", "bool"),
            (32, "levelCalPercent", "i64"),
            (33, "levelCalMessage", "utf8"),
            (34, "levelCalSucceeded", "bool"),
        ]
        for entry in expected {
            let property = try #require(properties.first { $0["name"] as? String == entry.name })
            #expect((property["ordinal"] as? NSNumber)?.intValue == entry.ordinal, "\(entry.name)")
            #expect(property["kind"] as? String == entry.kind, "\(entry.name)")
            #expect(property["direction"] as? String == "outbound", "\(entry.name)")
        }
    }

    /// RX2's own preamp mode (5cdb9a1ab) is appended last on `stepAtt`, sent
    /// only to a peer that declares `adcAttenuators`, which the phone does;
    /// the preamp mode the phone writes keeps its ordinal.
    @Test func stepAttRx2PreampModeIsAppendedLast() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let stepAtt = try #require(classes["StepAttenuatorFacade"] as? [String: Any])
        let properties = try #require(stepAtt["properties"] as? [[String: Any]])
        let ordinals = properties.compactMap { ($0["ordinal"] as? NSNumber)?.intValue }
        let rx2 = try #require(properties.first { $0["name"] as? String == "rx2PreampMode" })
        #expect((rx2["ordinal"] as? NSNumber)?.intValue == 24)
        #expect(ordinals.max() == 24)
        #expect(rx2["kind"] as? String == "i64")
        #expect(rx2["direction"] as? String == "bidirectional")
        let preamp = try #require(properties.first { $0["name"] as? String == "preampMode" })
        #expect((preamp["ordinal"] as? NSNumber)?.intValue == 2)
        #expect(preamp["kind"] as? String == "i64")
        #expect(preamp["direction"] as? String == "bidirectional")
    }

    /// The Core's hardware version: 12 with the level calibration (5cdb9a1ab),
    /// 13 with the radio codec lane (dc238c2d5), which opened HL2 Swap audio
    /// channels.
    @Test func radioHardwareVersionIsThirteenInTheSurface() throws {
        let capabilities = try #require(try Self.surface()["capabilities"] as? [[String: Any]])
        let hardware = try #require(capabilities.first { $0["name"] as? String == "radioHardwareVersion" })
        #expect((hardware["value"] as? NSNumber)?.intValue == 13)
    }

    /// The RADE reason (Core trunk 4582efc90, hello `radeReason` 1, which
    /// the phone declares) is appended last on `SliceModel`, so the
    /// RADE frequency offset the phone reads keeps its ordinal.
    @Test func sliceRadeReasonIsAppendedLast() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let slice = try #require(classes["SliceModel"] as? [String: Any])
        let properties = try #require(slice["properties"] as? [[String: Any]])
        #expect(properties.count == 154)
        let offset = try #require(properties.first { $0["name"] as? String == "radeFreqOffsetHz" })
        #expect((offset["ordinal"] as? NSNumber)?.intValue == 152)
        #expect(offset["kind"] as? String == "f64")
        let reason = try #require(properties.first { $0["name"] as? String == "radeReason" })
        #expect((reason["ordinal"] as? NSNumber)?.intValue == 153)
        #expect(reason["kind"] as? String == "utf8")
        #expect(reason["direction"] as? String == "outbound")
    }

    /// The CFC profile the band editor reads (`cfcProfile` 1): the Core's
    /// alone, written only through cfc.setProfile, after the TX EQ curve.
    @Test func theCfcProfileSitsAfterTheTxEqCurve() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let transmit = try #require(classes["TransmitModel"] as? [String: Any])
        let properties = try #require(transmit["properties"] as? [[String: Any]])
        let curve = try #require(properties.first { $0["name"] as? String == "txEqCurve" })
        #expect((curve["ordinal"] as? NSNumber)?.intValue == 87)
        let profile = try #require(properties.first { $0["name"] as? String == "cfcProfile" })
        #expect((profile["ordinal"] as? NSNumber)?.intValue == 88)
        #expect(profile["kind"] as? String == "utf8")
        #expect(profile["direction"] as? String == "outbound")
    }
}
