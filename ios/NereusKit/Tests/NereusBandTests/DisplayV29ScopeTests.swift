// NereusSDR for iOS: master, sparse own and shared Phone picture scope
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-12: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import Foundation
import Testing
@testable import NereusBand

@MainActor
@Suite struct DisplayV29ScopeTests {
    private func isolated(_ run: (UserDefaults, BandDisplaySettingsStore) throws -> Void) throws {
        let name = "phone-display-scope-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        try run(defaults, BandDisplaySettingsStore(defaults: defaults))
    }
    private func legacy(_ settings: BandDisplaySettings, id: String, defaults: UserDefaults) throws {
        defaults.set(try JSONEncoder().encode(settings), forKey: BandDisplaySettingsStore.keyPrefix + id)
    }
    private let rx = [PaletteStop(at: 0, colour: "#010203"), PaletteStop(at: 1, colour: "#040506")]
    private let tx = [PaletteStop(at: 0, colour: "#102030"), PaletteStop(at: 1, colour: "#405060")]

    @Test func legacyPrimaryBecomesMasterAndSharedAuthorityWhileClosedPicturesAndBandsSurvive() throws {
        try isolated { defaults, store in
            var primary = BandDisplaySettings.desktopDefaults
            primary.traceWidthPoints = 0.6
            primary.gridColour = "#10203040"
            primary.customPalette = rx
            primary.txCustomPalette = tx
            primary.txGridTopDbm = 10
            primary.txGridRangeDb = 80
            primary.txWaterfallPaletteId = 2
            var closed = primary
            closed.traceWidthPoints = 1.2
            closed.traceFill = false
            closed.gridColour = "#AABBCCDD"
            closed.customPalette = tx
            closed.txCustomPalette = rx
            closed.txGridTopDbm = 80
            closed.txGridRangeDb = 200
            closed.txWaterfallPaletteId = 3
            closed.txWaterfallLowDbm = -130
            closed.enterBand("17")
            closed.scaleTopDbm = -35
            closed.scaleBottomDbm = -105
            closed.threeDFloorDb = 14
            try legacy(primary, id: "1", defaults: defaults)
            try legacy(closed, id: "7", defaults: defaults)
            let oldRaw = try #require(defaults.data(forKey: BandDisplaySettingsStore.keyPrefix + "7"))
            defaults.set(oldRaw, forKey: "phone.display.scope.unrelated-backup")
            let own = store.settings(forPan: "7")
            #expect(own.traceWidthPoints == 1.2 && !own.traceFill)
            #expect(own.gridColour == primary.gridColour)
            #expect(own.customPalette == rx && own.txCustomPalette == tx)
            #expect(own.txGridTopDbm == 10 && own.txGridRangeDb == 80)
            #expect(own.txWaterfallPaletteId == 2 && own.txWaterfallLowDbm == -70)
            #expect(own.scaleTopDbm == -35 && own.scaleBottomDbm == -105 && own.threeDFloorDb == 14)
            let inherited = store.settings(forPan: "9")
            #expect(inherited.traceWidthPoints == 0.6)
            #expect(inherited.gridColour == primary.gridColour && inherited.customPalette == rx)
            #expect(inherited.txGridTopDbm == 10 && inherited.txGridRangeDb == 80)
            let restarted = BandDisplaySettingsStore(defaults: defaults)
            #expect(restarted.settings(forPan: "7") == own)
            #expect(defaults.data(forKey: "phone.display.scope.unrelated-backup") == oldRaw)
        }
    }

    @Test func invalidPrimaryTransmitPairUsesShipValuesRatherThanAClosedPansPair() throws {
        try isolated { defaults, _ in
            for (ref, range, wantedRef, wantedRange) in [(250.0, 8.0, 20.0, 100.0),
                                                        (15.0, 8.0, 15.0, 100.0),
                                                        (250.0, 80.0, 20.0, 80.0)] {
                // Native loadTransmitDisplayScale validates each component independently.
                var primary = BandDisplaySettings.desktopDefaults
                primary.txGridTopDbm = ref
                primary.txGridRangeDb = range
                var closed = BandDisplaySettings.desktopDefaults
                closed.txGridTopDbm = 55
                closed.txGridRangeDb = 70
                // Isolate each migration by clearing only these test-owned keys.
                for key in defaults.dictionaryRepresentation().keys where key.hasPrefix("phone.display.") || key.hasPrefix(BandDisplaySettingsStore.keyPrefix) {
                    defaults.removeObject(forKey: key)
                }
                try legacy(primary, id: "1", defaults: defaults)
                try legacy(closed, id: "8", defaults: defaults)
                let reopened = BandDisplaySettingsStore(defaults: defaults)
                let effective = reopened.settings(forPan: "8")
                #expect(effective.txGridTopDbm == wantedRef && effective.txGridRangeDb == wantedRange)
            }
        }
    }

    @Test func sharedDefinitionsColoursAndTransmitScaleCannotBeOverwrittenByAStalePanSave() throws {
        try isolated { _, store in
            var primary = store.settings(forPan: "1")
            primary.customPalette = rx
            primary.txCustomPalette = tx
            store.setSettings(primary, forPan: "1")
            let stale = store.settings(forPan: "1")
            var second = store.settings(forPan: "2")
            second.traceFill = false
            second.customPalette = tx
            second.gridColour = "#10203040"
            second.txGridTopDbm = 0
            second.txGridRangeDb = 90
            second.txWaterfallPaletteId = 3
            second.txWaterfallLowDbm = -80
            second.txWaterfallHighDbm = 40
            second.txWaterfallLowColour = "#112233FF"
            store.setSettings(second, forPan: "2")
            var inactive = stale
            inactive.traceWidthPoints = 1.4
            store.setSettings(inactive, forPan: "1")
            let one = store.settings(forPan: "1")
            let two = store.settings(forPan: "2")
            #expect(one.customPalette == tx && two.customPalette == tx)
            #expect(one.txCustomPalette == tx && two.txCustomPalette == tx)
            #expect(one.gridColour == "#10203040" && two.gridColour == "#10203040")
            #expect(one.txGridTopDbm == 0 && two.txGridRangeDb == 90)
            #expect(one.txWaterfallPaletteId == 3 && two.txWaterfallPaletteId == 3)
            #expect(one.txWaterfallLowDbm == -80 && two.txWaterfallHighDbm == 40)
            #expect(one.txWaterfallLowColour == "#112233FF" && two.txWaterfallLowColour == "#112233FF")
            #expect(one.traceWidthPoints == 1.4 && !two.traceFill)
            store.resetPicture(forPan: "1", fields: ["traceWidthPoints", "customPalette", "txGridTopDbm"])
            let reset = store.settings(forPan: "1")
            #expect(reset.traceWidthPoints == 0.5)
            #expect(reset.customPalette == tx && reset.txGridTopDbm == 0)
        }
    }

    @Test func sparseEqualityAndExplicitMasterWritesClearClosedOwnsOnlyForSelectedFields() throws {
        try isolated { _, store in
            var one = store.settings(forPan: "1")
            one.traceWidthPoints = 1.2
            one.traceFill = false
            store.setSettings(one, forPan: "1")
            var closed = store.settings(forPan: "12")
            closed.traceWidthPoints = 1.4
            closed.waterfallOpacityPercent = 40
            store.setSettings(closed, forPan: "12")
            #expect(store.ownFields(forPan: "1", among: ["traceWidthPoints", "traceFill"]) == ["traceWidthPoints", "traceFill"])
            one.traceWidthPoints = 0.5
            store.setSettings(one, forPan: "1")
            #expect(store.ownFields(forPan: "1", among: ["traceWidthPoints", "traceFill"]) == ["traceFill"])
            var master = store.masterSettings(forPan: "1")
            master.traceFill = false
            store.setMasterSettings(master, fields: ["traceFill"])
            #expect(store.settings(forPan: "12").traceFill == false)
            #expect(store.ownFields(forPan: "1", among: ["traceFill"]).isEmpty)
            // Same master value is an explicit write and still removes this closed own.
            master.traceWidthPoints = 0.5
            store.setMasterSettings(master, fields: ["traceWidthPoints"])
            let followed = store.settings(forPan: "12")
            #expect(followed.traceWidthPoints == 0.5 && followed.waterfallOpacityPercent == 40)
            #expect(store.ownFields(forPan: "12", among: ["traceWidthPoints"]).isEmpty)
        }
    }

    @Test func pageCopyAndFollowPreserveBandAndSharedExceptionsAndKeepTheModeTupleCoherent() throws {
        try isolated { _, store in
            var selected = store.settings(forPan: "1")
            selected.waterfallLevelMode = .noiseFloorAgc
            selected.traceFill = false
            selected.waterfallOpacityPercent = 45
            selected.enterBand("3")
            selected.scaleTopDbm = -30
            selected.scaleBottomDbm = -100
            selected.threeDFloorDb = 16
            selected.txGridTopDbm = 15
            store.setSettings(selected, forPan: "1")
            let tuple: Set<String> = ["clarityEnabled", "waterfallAgc", "waterfallNfAgc", "useSpectrumMinMax"]
            store.copyOwnPictureToMaster(forPan: "1", fields: tuple.union(["traceFill", "scaleTopDbm", "threeDFloorDb", "txGridTopDbm"]))
            let other = store.settings(forPan: "10")
            #expect(other.waterfallLevelMode == .noiseFloorAgc && !other.traceFill)
            #expect(other.scaleTopDbm == -40 && other.threeDFloorDb == 6)
            #expect(other.txGridTopDbm == 15)
            #expect(store.ownFields(forPan: "1", among: tuple.union(["traceFill"])).isEmpty)
            #expect(store.ownFields(forPan: "1", among: ["waterfallOpacityPercent"]) == ["waterfallOpacityPercent"])
            store.resetPicture(forPan: "1", fields: ["waterfallOpacityPercent", "scaleTopDbm", "threeDFloorDb", "txGridTopDbm"])
            let followed = store.settings(forPan: "1")
            #expect(followed.waterfallOpacityPercent == 100)
            #expect(followed.scaleTopDbm == -30 && followed.threeDFloorDb == 16 && followed.txGridTopDbm == 15)
        }
    }

    /// Spec DC2, DC9: Spectrum Height, Look back, CTUN, DUP and decimation
    /// are this device's own rows, so a change on one pan shows on every
    /// pan and no pan keeps its own; a picture row stays the pan's.
    @Test func thisDevicesOwnRowsAreSharedByEveryPanAndAPictureRowIsNot() throws {
        try isolated { _, store in
            var second = store.settings(forPan: "2")
            second.ctun = false
            second.displayDuplex = true
            second.rewindSeconds = BandDisplaySettings.rewindChoices.last ?? second.rewindSeconds
            second.decimation = 4
            second.spectrumSharePercent = 60
            second.traceFill = !second.traceFill
            store.setSettings(second, forPan: "2")
            let first = store.settings(forPan: "1")
            #expect(!first.ctun && first.displayDuplex && first.decimation == 4 && first.spectrumSharePercent == 60)
            #expect(first.rewindSeconds == second.rewindSeconds)
            #expect(first.traceFill != second.traceFill)
            #expect(store.ownFields(forPan: "2", among: BandDisplaySettingsStore.deviceRowFields).isEmpty)
            #expect(store.ownFields(forPan: "2", among: ["traceFill"]) == ["traceFill"])
            #expect(BandDisplaySettingsStore.pictureFields.isDisjoint(with: BandDisplaySettingsStore.deviceRowFields))
        }
    }

    /// A store kept before the device rows were shared: each takes Pan 1's
    /// value as Pan 1 drew it, and Pan 2 follows it.
    @Test func aStoreKeptBeforeTheDeviceRowsWereSharedTakesPanOnesValues() throws {
        try isolated { defaults, store in
            var kept = store.settings(forPan: "1")
            kept.ctun = false
            kept.decimation = 2
            store.setSettings(kept, forPan: "1")
            // As an earlier build kept them: in Pan 1's own entry, none shared.
            defaults.removeObject(forKey: "phone.display.deviceRowsShared")
            defaults.set(try JSONSerialization.data(withJSONObject: [String: Any]()), forKey: "phone.display.shared.v29")
            let blob = try #require(defaults.data(forKey: BandDisplaySettingsStore.keyPrefix + "1"))
            var own = try #require(try JSONSerialization.jsonObject(with: blob) as? [String: Any])
            own["ctun"] = false
            own["decimation"] = 2
            defaults.set(try JSONSerialization.data(withJSONObject: own), forKey: BandDisplaySettingsStore.keyPrefix + "1")
            let reopened = BandDisplaySettingsStore(defaults: defaults)
            let first = reopened.settings(forPan: "1")
            #expect(!first.ctun && first.decimation == 2)
            let second = reopened.settings(forPan: "2")
            #expect(!second.ctun && second.decimation == 2)
        }
    }
}
