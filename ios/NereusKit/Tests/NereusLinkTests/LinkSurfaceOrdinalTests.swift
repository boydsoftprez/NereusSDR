// NereusSDR for iOS: the positions of the Core's newest capabilities and radio properties in the link surface
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: Roundtable capability and mirrored properties pinned. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: merge of main: stationRoundtableVersion at 116, after displayCodecVersion; coreBuildInfo 117.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-26: CW transmit, remoteCwVersion at 75 moves every later entry
//   one place. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-27: CW transmit final fix wave F2: cwQskUnavailableReason pinned
//   after the CW settings. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

// 2026-10-31: Pin the additive NNR warning capability and read-only slice property.
//   J.J. Boyd (KG4VCF), AI-assisted via Codex.

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
        #expect(capabilities.count == 120)
        #expect(capabilities[69]["value"] == nil, "The loopback capture cannot advertise a live TX watch value")
        // The merged b44d638 surface inserts diversityControlVersion after
        // diversityPatternVersion. Capture b20ad1186 also includes the existing
        // optional txWatchPathVersion after remoteTxVersion, shifting later indices.
        // Radio Mic 2 changes its value, without moving any other entry.
        // Station CAT setup inserts stationCatVersion and the radio speaker
        // inserts radioSpeakerVersion before coreBuildInfo, and the Core
        // speaker then coreSpeakerVersion, with coreBuildInfo staying last.
        // The Core's rotor inserts remoteRotorControlVersion after
        // accessoryTxVersion (index 84), moving every later entry one
        // place.
        let expected: [(index: Int, name: String, kind: String)] = [
            (69, "txWatchPathVersion", "i64"),
            // Remote CW (feature `remoteCw` 1) inserts remoteCwVersion after
            // txReadingsVersion (index 75), moving every later entry one
            // place; coreBuildInfo stays last, at 117.
            (75, "remoteCwVersion", "i64"),
            (85, "remoteRotorControlVersion", "i64"),
            (91, "diversityControlVersion", "i64"),
            (98, "paProfileVersion", "i64"),
            (100, "txInhibitReasonVersion", "i64"),
            (101, "paTransmitBandVersion", "i64"),
            (102, "sliceAccessVersion", "i64"),
            (103, "mediaDirectVersion", "i64"),
            (104, "mediaStunUrls", "utf8"),
            (105, "rx2AttenuatorVersion", "i64"),
            (106, "radioMicVersion", "i64"),
            (107, "rxFilterLowPassVersion", "i64"),
            (108, "radeReasonVersion", "i64"),
            (109, "stationCatVersion", "i64"),
            // The radio speaker (feature `radioSpeaker` 1) goes before the
            // build info, which is read by name.
            (110, "radioSpeakerVersion", "i64"),
            // The Core speaker (feature `coreSpeaker` 1) goes before the
            // build info too, which moves to 111.
            (111, "coreSpeakerVersion", "i64"),
            // The picture a spectrum context continues (feature
            // `spectrumPicture` 1, R-DISP-RETUNE) goes before the build
            // info as well, which moves to 112.
            (112, "spectrumPictureVersion", "i64"),
            // The radio's second firmware number (feature `firmwareBeta`
            // 1) goes before the build info too, which moves to 113.
            (113, "firmwareBeta", "i64"),
            // The waveform feed (feature `waveform` 1, which the phone
            // does not declare) goes before the build info as well, which
            // moves to 114.
            (114, "waveformVersion", "i64"),
            // The RX meter stamps (feature `rxMeterStamps` 1, which the
            // phone does not declare) go before the build info as well,
            // which moves to 115.
            (115, "rxMeterStampsVersion", "i64"),
            // The display codec version (feature `displayCodec` 2, NSDC
            // v2) goes before the build info as well, which moves to 116.
            (116, "displayCodecVersion", "i64"),
            // Roundtable (feature `roundtable` 1, which the phone
            // declares) goes after it, before the build info, which moves
            // to 117.
            (117, "stationRoundtableVersion", "i64"),
            // The NNR warning capability precedes the final build info.
            (118, "nnrLoadWarningVersion", "i64"),
            (119, "coreBuildInfo", "utf8"),
        ]
        // Core trunk b26112687: sliceAccessVersion reads 3 (the Core's own
        // slice can be taken); nothing moved.
        #expect((capabilities[102]["value"] as? NSNumber)?.intValue == 3)
        #expect((capabilities[91]["value"] as? NSNumber)?.intValue == 1)
        #expect((capabilities[106]["value"] as? NSNumber)?.intValue == 2)
        #expect((capabilities[85]["value"] as? NSNumber)?.intValue == 1)
        #expect((capabilities[109]["value"] as? NSNumber)?.intValue == 1)
        #expect((capabilities[116]["value"] as? NSNumber)?.intValue == 2)
        #expect((capabilities[117]["value"] as? NSNumber)?.intValue == 2)
        #expect((capabilities[118]["value"] as? NSNumber)?.intValue == 1)
        #expect(Set(capabilities.compactMap { $0["name"] as? String }).count == 120)
        for entry in expected {
            try #require(capabilities.count > entry.index)
            #expect(capabilities[entry.index]["name"] as? String == entry.name, "\(entry.name)")
            #expect(capabilities[entry.index]["kind"] as? String == entry.kind, "\(entry.name)")
        }
    }

    /// Roundtable's mirrored object (feature `roundtable` 1, which the
    /// phone declares): six read-only properties, and the two spot source
    /// properties appended after FreeDV Reporter's, so no property the
    /// phone reads moved.
    @Test func roundtablePropertiesSitWhereTheSurfacePinsThem() throws {
        let surface = try Self.surface()
        let classes = try #require(surface["mirrorClasses"] as? [String: Any])
        let state = try #require(classes["RoundtableState"] as? [String: Any])
        let properties = try #require(state["properties"] as? [[String: Any]])
        let expected: [(ordinal: Int, name: String, kind: String)] = [
            (0, "linkState", "enum"),
            (1, "linkSince", "i64"),
            (2, "firstRunAnswered", "bool"),
            (3, "hidden", "bool"),
            (4, "mentionCounts", "utf8"),
            (5, "sliceRooms", "utf8"),
        ]
        #expect(properties.count == expected.count)
        for entry in expected {
            let property = try #require(properties.first { $0["name"] as? String == entry.name })
            #expect((property["ordinal"] as? NSNumber)?.intValue == entry.ordinal, "\(entry.name)")
            #expect(property["kind"] as? String == entry.kind, "\(entry.name)")
            #expect(property["direction"] as? String == "outbound", "\(entry.name)")
        }
        let linkState = try #require(properties.first { $0["name"] as? String == "linkState" })
        #expect((linkState["enumValues"] as? [NSNumber])?.map(\.intValue) == [0, 1, 2, 3, 4, 5])
        let host = try #require(classes["SpotSourceHost"] as? [String: Any])
        let hostProperties = try #require(host["properties"] as? [[String: Any]])
        #expect(hostProperties.count == 13)
        for (ordinal, name) in [(10, "freedvReporterHidden"), (11, "roundtableState"), (12, "roundtableText")] {
            let property = try #require(hostProperties.first { $0["name"] as? String == name })
            #expect((property["ordinal"] as? NSNumber)?.intValue == ordinal, "\(name)")
            #expect(property["direction"] as? String == "outbound", "\(name)")
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
        #expect(properties.count == 49)
        #expect(properties.compactMap { ($0["ordinal"] as? NSNumber)?.intValue }.sorted() == Array(0..<49))
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

    /// The radio speaker (feature `radioSpeaker` 1, which the phone
    /// declares): its level, mute and amplifier choice, written by the
    /// phone, then the two reports, read only, appended after the
    /// Diversity state, so no property the phone reads moved.
    @Test func radioSpeakerPropertiesFollowTheDiversityState() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let radio = try #require(classes["RadioModel"] as? [String: Any])
        let properties = try #require(radio["properties"] as? [[String: Any]])
        let expected: [(ordinal: Int, name: String, kind: String, direction: String)] = [
            (38, "radioSpeakerVolume", "i64", "bidirectional"),
            (39, "radioSpeakerMuted", "bool", "bidirectional"),
            (40, "speakerAmplifierMode", "i64", "bidirectional"),
            (41, "radioSpeakerAvailability", "i64", "outbound"),
            (42, "speakerAmplifierAvailable", "bool", "outbound"),
        ]
        for entry in expected {
            let property = try #require(properties.first { $0["name"] as? String == entry.name })
            #expect((property["ordinal"] as? NSNumber)?.intValue == entry.ordinal, "\(entry.name)")
            #expect(property["kind"] as? String == entry.kind, "\(entry.name)")
            #expect(property["direction"] as? String == entry.direction, "\(entry.name)")
        }
        let capabilities = try #require(try Self.surface()["capabilities"] as? [[String: Any]])
        let version = try #require(capabilities.first { $0["name"] as? String == "radioSpeakerVersion" })
        #expect((version["value"] as? NSNumber)?.intValue == 1)
    }

    /// The Core speaker (feature `coreSpeaker` 1, which the phone declares):
    /// its level, mute and chosen card, which a peer may set, the card list
    /// and the state, read only, then the details, appended after the
    /// radio speaker's five, so no property the phone reads moved. The
    /// phone reads the level, the mute and the state.
    @Test func coreSpeakerPropertiesFollowTheRadioSpeaker() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let radio = try #require(classes["RadioModel"] as? [String: Any])
        let properties = try #require(radio["properties"] as? [[String: Any]])
        let expected: [(ordinal: Int, name: String, kind: String, direction: String)] = [
            (43, "coreSpeakerVolume", "i64", "bidirectional"),
            (44, "coreSpeakerMuted", "bool", "bidirectional"),
            (45, "coreSpeakerDevice", "utf8", "bidirectional"),
            (46, "coreSpeakerDevices", "utf8", "outbound"),
            (47, "coreSpeakerState", "utf8", "outbound"),
            (48, "coreSpeakerDetails", "utf8", "bidirectional"),
        ]
        for entry in expected {
            let property = try #require(properties.first { $0["name"] as? String == entry.name })
            #expect((property["ordinal"] as? NSNumber)?.intValue == entry.ordinal, "\(entry.name)")
            #expect(property["kind"] as? String == entry.kind, "\(entry.name)")
            #expect(property["direction"] as? String == entry.direction, "\(entry.name)")
        }
        let capabilities = try #require(try Self.surface()["capabilities"] as? [[String: Any]])
        let version = try #require(capabilities.first { $0["name"] as? String == "coreSpeakerVersion" })
        #expect((version["value"] as? NSNumber)?.intValue == 1)
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
    /// channels, 14 with the Anvelina's "Gnd BPF2 on Tx" and DX Outputs.
    @Test func radioHardwareVersionIsFourteenInTheSurface() throws {
        let capabilities = try #require(try Self.surface()["capabilities"] as? [[String: Any]])
        let hardware = try #require(capabilities.first { $0["name"] as? String == "radioHardwareVersion" })
        #expect((hardware["value"] as? NSNumber)?.intValue == 14)
    }

    /// The RADE reason (Core trunk 4582efc90, hello `radeReason` 1, which
    /// the phone declares) is appended after the RADE status on
    /// `SliceModel`, so the RADE frequency offset the phone reads keeps its
    /// ordinal. The RX meter stamp (hello `rxMeterStamps` 1, which the
    /// phone does not declare) is appended after it and moves neither.
    /// The NNR warning follows at 155 and also leaves the prior ordinals unchanged.
    @Test func sliceRadeReasonAndNnrWarningKeepTheirAppendedOrdinals() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let slice = try #require(classes["SliceModel"] as? [String: Any])
        let properties = try #require(slice["properties"] as? [[String: Any]])
        #expect(properties.count == 156)
        let warning = try #require(properties.first { $0["name"] as? String == "nnrLoadWarning" })
        #expect((warning["ordinal"] as? NSNumber)?.intValue == 155)
        #expect(warning["kind"] as? String == "utf8")
        #expect(warning["direction"] as? String == "outbound")
        let stamp = try #require(properties.first { $0["name"] as? String == "meterReadingNs" })
        #expect((stamp["ordinal"] as? NSNumber)?.intValue == 154)
        #expect(stamp["kind"] as? String == "i64")
        #expect(stamp["direction"] as? String == "outbound")
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

    /// Why the Core does not offer QSK (`cwSettings` 1, CW transmit final
    /// fix wave F2): the Core's alone, after the 30 CW settings, so no CW
    /// setting the phone reads moved.
    @Test func theQskReasonFollowsTheCwSettings() throws {
        let classes = try #require(try Self.surface()["mirrorClasses"] as? [String: Any])
        let transmit = try #require(classes["TransmitModel"] as? [String: Any])
        let properties = try #require(transmit["properties"] as? [[String: Any]])
        let speed = try #require(properties.first { $0["name"] as? String == "cwKeyerSpeed" })
        #expect((speed["ordinal"] as? NSNumber)?.intValue == 89)
        let last = try #require(properties.first { $0["name"] as? String == "cwxMemory8" })
        #expect((last["ordinal"] as? NSNumber)?.intValue == 118)
        let reason = try #require(properties.first { $0["name"] as? String == "cwQskUnavailableReason" })
        #expect((reason["ordinal"] as? NSNumber)?.intValue == 119)
        #expect(reason["kind"] as? String == "utf8")
        #expect(reason["direction"] as? String == "outbound")
    }
}
