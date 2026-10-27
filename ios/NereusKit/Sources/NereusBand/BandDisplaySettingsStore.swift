// NereusSDR for iOS: keeps each pan's display settings on this phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// Each pan's ``BandDisplaySettings``, kept on this phone in `UserDefaults`
/// and never sent to the Core (display settings are each device's own).
/// Stored as JSON under the app's `phone.` prefix, one key per pan. A
/// setting a stored value lacks (one added by a later version) takes the
/// desktop's default.
@MainActor
public final class BandDisplaySettingsStore {
    /// The key prefix, under the app's own `phone.` prefix.
    public static let keyPrefix = "phone.band.display."

    private let defaults: UserDefaults
    private static let logger = Logger(subsystem: "NereusSDR", category: "band")

    public init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
    }

    private static let masterKey = "phone.display.master.v29"
    private static let sharedKey = "phone.display.shared.v29"
    private static let revisionKey = "phone.display.revision.v29"
    private static let migrationKey = "phone.display.scopeVersion"
    /// Set once this device's own rows (``deviceRowFields``) are in the shared entry.
    private static let deviceRowsKey = "phone.display.deviceRowsShared"
    private var initialized = false
    private var revision: UInt64 = 1
    private var nextSnapshotToken: UInt64 = 0
    private var fieldRevisions: [String: UInt64] = [:]
    private var snapshotWriteRevisions: [String: UInt64] = [:]
    private var snapshots: [String: [String: Any]] = [:]
    private var snapshotOrder: [String] = []

    /// Device-shared exceptions: exactly nine Appearance colors (RX color
    /// and alpha are separate existing typed fields), two Custom definitions,
    /// and all existing TX scale/waterfall rendering settings, and this
    /// device's own rows (``deviceRowFields``).
    public static let sharedFields: Set<String> = Set([
        "gridColour", "gridFineColour", "hGridColour", "gridTextColour", "bandEdgeColour",
        "rxZeroLineColour", "txZeroLineColour", "passbandColour", "passbandOpacity", "txPassbandColour",
        "customPalette", "txCustomPalette", "txGridTopDbm", "txGridRangeDb",
        "txWaterfallLowDbm", "txWaterfallHighDbm", "txWaterfallPaletteId", "txWaterfallLowColour",
    ]).union(deviceRowFields)
    /// This device's own rows (spec DC2, DC9): Spectrum Height, Look back,
    /// CTUN, DUP and decimation. They are the device's, never a pan's, so
    /// every pan the iPad shows draws the one value, and no pan holds its own.
    public static let deviceRowFields: Set<String> = [
        "spectrumSharePercent", "spectrumSharePercentSideways", "rewindSeconds", "ctun", "displayDuplex",
        "decimation",
    ]
    public static let waterfallModeFields: Set<String> = [
        "clarityEnabled", "waterfallAgc", "waterfallNfAgc", "useSpectrumMinMax",
    ]
    /// Closed picture inventory. Band ranges/floors, device layout and Core
    /// settings never acquire picture overrides or participate in page copy.
    public static let pictureFields: Set<String> = [
        "clarityEnabled", "waterfallAgc", "waterfallNfAgc", "useSpectrumMinMax",
        "waterfallLowDbm", "waterfallHighDbm", "waterfallOffsetDb", "waterfallColorGain", "waterfallBlackLevel",
        "noiseFloorLine", "noiseFloorShiftDb", "noiseFloorLineWidth", "calibrationOffsetDb",
        "peakBlobs", "peakBlobCount", "peakBlobHold", "peakBlobHoldMs", "peakBlobFall", "peakBlobFallDbPerSec",
        "peakBlobsInsideFilterOnly", "activePeakHold", "activePeakHoldMs", "activePeakHoldFallDbPerSec",
        "activePeakHoldFill", "activePeakHoldOnTx", "grid", "showDbmScale", "showZeroLine", "gridStepDb",
        "frequencyGridSpacingHz", "frequencyScaleFontPoints", "noiseFloorPosition", "waterfallBlanking",
        "waterfallBlankingThresholdDb", "threeDSpeed", "backgroundEnabled", "backgroundColour",
        "backgroundImagePath", "backgroundOpacityPercent", "traceColour", "traceLineColour", "traceWidthPoints",
        "traceFill", "traceFillOpacity", "traceGradient", "frequencyLabelAlignment", "waterfallPaletteId",
        "spectrumAverageTimeMs", "waterfallAverageTimeMs", "spectrumDetector", "spectrumAveraging",
        "waterfallDetector", "waterfallAveraging", "normalize", "waterfallPeriodMs",
        "waterfallOpacityPercent", "waterfallStopOnTx", "showRxFilterOnWaterfall",
        "showTxFilterOnWaterfall", "showRxZeroLineOnWaterfall", "showTxZeroLineOnWaterfall", "timestampPosition",
        "timestampUtc", "waterfallLowColour", "gridFollowsNoiseFloor", "gridNoiseFloorOffsetDb", "gridKeepsRange",
        "showPeakValue", "peakValueCorner", "peakValueDelayMs", "peakValueColour", "showBinWidth", "showFps",
        "showCursorFrequency", "peakHold", "peakHoldDelayMs", "noiseFloorColour", "noiseFloorTextColour",
        "noiseFloorFastColour", "peakHoldColour", "peakBlobColour", "peakBlobTextColour", "spectrumView", "threeDGain",
        "threeDSpan", "threeDAngle", "threeDSliceShadow",
    ]

    /// Fields an earlier build kept that this one no longer reads: each
    /// band's own Top and Bottom (`bandScales`), now the Core's per-band
    /// dB Max and dB Min. Dropped from a kept pan as it loads, leaving its
    /// other fields as they were.
    public static let droppedFields: Set<String> = ["bandScales"]

    private static func dictionary(_ settings: BandDisplaySettings) -> [String: Any] {
        guard let data = try? JSONEncoder().encode(settings),
              var object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return [:] }
        object.removeValue(forKey: "storeSnapshotVersion")
        return object
    }
    private func read(_ key: String) -> [String: Any] {
        guard let data = defaults.data(forKey: key),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return [:] }
        return object
    }
    private func write(_ object: [String: Any], key: String) {
        do { defaults.set(try JSONSerialization.data(withJSONObject: object), forKey: key) }
        catch { Self.logger.warning("Display settings could not be kept") }
    }
    private static func equal(_ lhs: Any?, _ rhs: Any?) -> Bool {
        switch (lhs, rhs) {
        case (nil, nil): return true
        case let (a as String, b as String):
            return a.hasPrefix("#") && b.hasPrefix("#") ? a.caseInsensitiveCompare(b) == .orderedSame : a == b
        case let (a as NSNumber, b as NSNumber):
            return abs(a.doubleValue - b.doubleValue) <= max(1, max(abs(a.doubleValue), abs(b.doubleValue))) * 1e-6
        case let (a?, b?): return NSDictionary(dictionary: ["value": a]).isEqual(to: ["value": b])
        default: return false
        }
    }
    private var panKeys: [String] {
        defaults.dictionaryRepresentation().keys.filter { $0.hasPrefix(Self.keyPrefix) }.sorted()
    }
    private func normalizedLegacy(_ key: String) -> [String: Any] {
        var stored = read(key)
        stored.removeValue(forKey: "sideways")
        stored.removeValue(forKey: "storeSnapshotVersion")
        for field in Self.droppedFields { stored.removeValue(forKey: field) }
        Self.readEarlierStrip(&stored)
        Self.readEarlierDefaults(&stored)
        Self.readEarlierLevelMode(&stored)
        Self.readEarlierWaterfallLevels(&stored)
        if stored["txCustomPalette"] == nil, let receive = stored["customPalette"] { stored["txCustomPalette"] = receive }
        Self.readRewindAsOffered(&stored)
        return stored
    }
    private func initializeScope() {
        guard !initialized else { return }
        initialized = true
        if defaults.integer(forKey: Self.migrationKey) >= 29 {
            revision = max(1, UInt64(max(0, defaults.integer(forKey: Self.revisionKey))))
            for field in Self.pictureFields.union(Self.sharedFields) { fieldRevisions[field] = revision }
            shareDeviceRows()
            return
        }
        let seed = Self.dictionary(.desktopDefaults)
        let legacyPrimary = normalizedLegacy(Self.keyPrefix + "1")
        var primary = seed.merging(legacyPrimary) { _, new in new }
        let ref = (primary["txGridTopDbm"] as? NSNumber)?.doubleValue ?? 20
        let range = (primary["txGridRangeDb"] as? NSNumber)?.doubleValue ?? 100
        // Same independent component fallback as SpectrumWidget.cpp:3099–3104.
        if !ref.isFinite || !(-200...200).contains(ref) { primary["txGridTopDbm"] = seed["txGridTopDbm"] }
        if !range.isFinite || !(10...200).contains(range) { primary["txGridRangeDb"] = seed["txGridRangeDb"] }
        let master = primary.filter { Self.pictureFields.contains($0.key) }
        let shared = primary.filter { Self.sharedFields.contains($0.key) }
        write(master, key: Self.masterKey)
        write(shared, key: Self.sharedKey)
        for key in panKeys {
            var own = normalizedLegacy(key)
            for field in Self.sharedFields { own.removeValue(forKey: field) }
            for field in Self.pictureFields where Self.equal(own[field], master[field] ?? seed[field]) {
                own.removeValue(forKey: field)
            }
            own["scopeStorageVersion"] = 29
            write(own, key: key)
        }
        revision = 1
        defaults.set(1, forKey: Self.revisionKey)
        defaults.set(29, forKey: Self.migrationKey)
        defaults.set(true, forKey: Self.deviceRowsKey)
    }
    /// A store kept before this device's own rows were shared: each takes
    /// Pan 1's value as it drew it, so nothing on Pan 1 moves, and every
    /// other pan follows it from now on.
    private func shareDeviceRows() {
        guard !defaults.bool(forKey: Self.deviceRowsKey) else { return }
        defaults.set(true, forKey: Self.deviceRowsKey)
        var shared = read(Self.sharedKey)
        let missing = Self.deviceRowFields.filter { shared[$0] == nil }
        guard !missing.isEmpty else { return }
        var first = Self.dictionary(.desktopDefaults)
        first.merge(read(Self.masterKey)) { _, new in new }
        first.merge(read(Self.keyPrefix + "1")) { _, new in new }
        Self.readRewindAsOffered(&first)
        for field in missing {
            shared[field] = first[field]
        }
        write(shared, key: Self.sharedKey)
    }
    private func changed() {
        revision &+= 1
        defaults.set(Int(clamping: revision), forKey: Self.revisionKey)
    }
    private func normalizedOwn(_ panId: String, inherited: [String: Any]) -> [String: Any] {
        let panKey = Self.keyPrefix + panId
        var own = read(panKey)
        if !own.isEmpty && (own["scopeStorageVersion"] as? Int) != 29 {
            // A legacy blob restored after the singleton migration still
            // receives all existing per-blob conversions exactly once.
            own = normalizedLegacy(panKey)
            for field in Self.sharedFields { own.removeValue(forKey: field) }
            for field in Self.pictureFields where Self.equal(own[field], inherited[field]) { own.removeValue(forKey: field) }
            own["scopeStorageVersion"] = 29
            write(own, key: panKey)
        } else if !Self.droppedFields.isDisjoint(with: own.keys) {
            for field in Self.droppedFields { own.removeValue(forKey: field) }
            write(own, key: panKey)
        }
        return own
    }
    private func effective(_ panId: String, masterOnly: Bool) -> BandDisplaySettings {
        initializeScope()
        var object = Self.dictionary(.desktopDefaults)
        object.merge(read(Self.masterKey)) { _, new in new }
        let own = normalizedOwn(panId, inherited: object)
        object.merge(own.filter { !masterOnly || !Self.pictureFields.contains($0.key) }) { _, new in new }
        object.merge(read(Self.sharedKey)) { _, new in new }
        nextSnapshotToken &+= 1
        object["storeSnapshotVersion"] = ["revision": revision, "token": nextSnapshotToken]
        guard let data = try? JSONSerialization.data(withJSONObject: object),
              var settings = try? JSONDecoder().decode(BandDisplaySettings.self, from: data) else { return .desktopDefaults }
        settings.storeSnapshotVersion = .init(revision: revision, token: nextSnapshotToken)
        let id = "\(panId)|\(nextSnapshotToken)|\(masterOnly)"
        if snapshots[id] == nil { snapshotOrder.append(id) }
        snapshots[id] = Self.dictionary(settings)
        while snapshotOrder.count > 256 {
            let old = snapshotOrder.removeFirst()
            snapshots.removeValue(forKey: old)
            snapshotWriteRevisions.removeValue(forKey: old)
        }
        return settings
    }

    /// Effective picture over master plus per-band/device and shared exceptions.
    public func settings(forPan panId: String) -> BandDisplaySettings { effective(panId, masterOnly: false) }

    /// Settings kept before the strip's size (D79) had the strip on or
    /// off: on reads as Small, off as Off. The plan they chose on this
    /// phone is dropped, since the plan is now the Core's.
    private static func readEarlierStrip(_ stored: inout [String: Any]) {
        if let on = stored.removeValue(forKey: "bandPlanStrip") as? Bool, stored["bandPlanSize"] == nil {
            stored["bandPlanSize"] = (on ? BandPlanSize.small : BandPlanSize.off).rawValue
        }
        stored.removeValue(forKey: "bandPlanId")
    }

    /// A look-back depth kept before the phone offered only the Core's
    /// depths reads as the nearest offered one.
    private static func readRewindAsOffered(_ stored: inout [String: Any]) {
        if let seconds = stored["rewindSeconds"] as? Int, !BandDisplaySettings.rewindChoices.contains(seconds) {
            stored["rewindSeconds"] = BandDisplaySettings.nearestRewind(seconds)
        }
    }

    /// Settings kept before the three level switches held one mode: it
    /// reads as the switches that choose it, the others at their defaults.
    private static func readEarlierLevelMode(_ stored: inout [String: Any]) {
        guard let raw = stored.removeValue(forKey: "waterfallLevelMode") as? String,
              stored["clarityEnabled"] == nil, stored["waterfallNfAgc"] == nil, stored["waterfallAgc"] == nil,
              let mode = BandDisplaySettings.WaterfallLevelMode(rawValue: raw) else {
            return
        }
        var settings = BandDisplaySettings.desktopDefaults
        settings.waterfallLevelMode = mode
        stored["clarityEnabled"] = settings.clarityEnabled
        stored["waterfallNfAgc"] = settings.waterfallNfAgc
        stored["waterfallAgc"] = settings.waterfallAgc
        if stored["useSpectrumMinMax"] == nil { stored["useSpectrumMinMax"] = settings.useSpectrumMinMax }
    }

    /// Translate only unmarked legacy positions, before overlaying current
    /// defaults. Both conversions use the original Black, never its new value.
    private static func readEarlierWaterfallLevels(_ stored: inout [String: Any]) {
        guard stored["waterfallLevelsVersion"] == nil else { return }
        let oldBlack = stored["waterfallBlackLevel"] as? Int
        let oldGain = stored["waterfallColorGain"] as? Int
        if oldBlack != nil || oldGain != nil {
            let black = oldBlack ?? ClarityLevels.kLegacyBlackLevelDefault
            let gain = oldGain ?? ClarityLevels.kLegacyColorGainDefault
            stored["waterfallBlackLevel"] = ClarityLevels.convertLegacyBlackLevel(black)
            stored["waterfallColorGain"] = ClarityLevels.convertLegacyColorGain(gain, black)
        }
        stored["waterfallLevelsVersion"] = 1
    }

    /// The phone's own defaults before the band took the desktop's colours
    /// and sizes (D83), each with the desktop's value that replaces it.
    static let earlierDefaults: [(key: String, earlier: String, desktop: String)] = [
        ("traceColour", "#22D3EE", "#00E5FF"),
        ("gridTextColour", "#F5E663FF", "#FFFF00FF"),
        ("rxZeroLineColour", "#FF6666FF", "#FF0000FF"),
        ("peakValueColour", "#7FB2FFFF", "#1E90FFFF"),
        ("noiseFloorTextColour", "#F5E663FF", "#FFFF00FF"),
        ("peakBlobColour", "#FF6B3DFF", "#FF4500FF"),
        ("peakBlobTextColour", "#FFFFFFFF", "#7FFF00FF"),
    ]
    static let earlierFillOpacity = 0.35
    static let earlierPeakValueDelayMs = 400

    /// Settings kept before the desktop's colours and sizes (D83): a value
    /// still at the phone's earlier default reads as the desktop's; one the
    /// operator changed is kept.
    private static func readEarlierDefaults(_ stored: inout [String: Any]) {
        guard stored["desktopValuesVersion"] == nil else {
            return
        }
        let desktop = BandDisplaySettings.desktopDefaults
        for entry in earlierDefaults {
            if let text = stored[entry.key] as? String, text.uppercased() == entry.earlier {
                stored[entry.key] = entry.desktop
            }
        }
        if let fill = stored["traceFillOpacity"] as? Double, abs(fill - earlierFillOpacity) < 1e-9 {
            stored["traceFillOpacity"] = desktop.traceFillOpacity
        }
        if let delay = stored["peakValueDelayMs"] as? Int, delay == earlierPeakValueDelayMs {
            stored["peakValueDelayMs"] = desktop.peakValueDelayMs
        }
    }

    /// Persist only the caller's actual edits since its read snapshot. A stale
    /// full-settings save cannot restore shared fields or own values erased by
    /// an intervening master write. Snapshots are local and never wire fields.
    public func setSettings(_ settings: BandDisplaySettings, forPan panId: String, includingPicture: Bool = true) {
        initializeScope()
        let incoming = Self.dictionary(settings)
        let id = "\(panId)|\(settings.storeSnapshotVersion.token)|false"
        let masterId = "\(panId)|\(settings.storeSnapshotVersion.token)|true"
        let baseline = (includingPicture ? snapshots[id] : snapshots[masterId]) ?? snapshots[id] ?? (settings.storeSnapshotVersion.revision == 0
            ? Self.dictionary(effective(panId, masterOnly: false)) : incoming)
        let edited = Set(incoming.keys).union(baseline.keys).filter { !Self.equal(incoming[$0], baseline[$0]) }
        guard !edited.isEmpty else { return }
        let inherited = Self.dictionary(effective(panId, masterOnly: true))
        var own = read(Self.keyPrefix + panId)
        var shared = read(Self.sharedKey)
        var applied = false
        let observedRevision = snapshotWriteRevisions[id] ?? snapshotWriteRevisions[masterId] ?? settings.storeSnapshotVersion.revision
        for field in edited {
            if Self.sharedFields.contains(field) {
                guard observedRevision == 0 || observedRevision >= (fieldRevisions[field] ?? 1) else { continue }
                shared[field] = incoming[field]
            } else if Self.pictureFields.contains(field) {
                guard includingPicture, observedRevision == 0 || observedRevision >= (fieldRevisions[field] ?? 1) else { continue }
                if Self.equal(incoming[field], inherited[field]) { own.removeValue(forKey: field) }
                else { own[field] = incoming[field] }
            } else if field != "sideways" { own[field] = incoming[field] }
            applied = true
        }
        guard applied else { return }
        own["scopeStorageVersion"] = 29
        let sharedChanges = edited.filter { Self.sharedFields.contains($0) && !Self.equal(shared[$0], read(Self.sharedKey)[$0]) }
        write(shared, key: Self.sharedKey)
        write(own, key: Self.keyPrefix + panId)
        changed()
        for field in sharedChanges { fieldRevisions[field] = revision }
        // Reusing the same editable value can revert a prior edit; separately
        // captured reads retain their independent before-values and fences.
        if snapshots[id] != nil { snapshots[id] = incoming; snapshotWriteRevisions[id] = revision }
        if snapshots[masterId] != nil { snapshots[masterId] = incoming; snapshotWriteRevisions[masterId] = revision }
    }

    public func masterSettings(forPan panId: String) -> BandDisplaySettings { effective(panId, masterOnly: true) }
    private static func pictureSelection(_ fields: Set<String>) -> Set<String> {
        var selected = fields.intersection(pictureFields)
        if !selected.isDisjoint(with: waterfallModeFields) { selected.formUnion(waterfallModeFields) }
        return selected
    }
    public func setMasterSettings(_ settings: BandDisplaySettings, fields: Set<String>) {
        initializeScope()
        let selected = Self.pictureSelection(fields)
        guard !selected.isEmpty else { return }
        let incoming = Self.dictionary(settings)
        var master = read(Self.masterKey)
        for field in selected { master[field] = incoming[field] }
        write(master, key: Self.masterKey)
        for key in panKeys {
            var own = read(key)
            for field in selected { own.removeValue(forKey: field) }
            write(own, key: key)
        }
        changed()
        for field in selected { fieldRevisions[field] = revision }
    }
    /// Accepted reference display-pill-live.html:2375–2382: Smooth reaches
    /// every kept pan under All, but only Manual becomes Auto. Preserve each
    /// nonmanual tuple, including inactive flags, across the master write.
    public func convertManualWaterfallModesForSmooth(forPan panId: String) {
        initializeScope()
        let captured = panKeys.map { key in
            let id = String(key.dropFirst(Self.keyPrefix.count))
            return (id, settings(forPan: id))
        }
        var master = masterSettings(forPan: panId)
        guard master.waterfallLevelMode == .manual || captured.contains(where: { $0.1.waterfallLevelMode == .manual }) else { return }
        if master.waterfallLevelMode == .manual { master.waterfallLevelMode = .agc }
        setMasterSettings(master, fields: Self.waterfallModeFields)
        for (id, before) in captured {
            var desired = before
            if desired.waterfallLevelMode == .manual { desired.waterfallLevelMode = .agc }
            // Fresh snapshots retain the master write's revision fences. The
            // ordinary setter removes any tuple equal to its new inheritance.
            var restored = settings(forPan: id)
            restored.clarityEnabled = desired.clarityEnabled
            restored.waterfallAgc = desired.waterfallAgc
            restored.waterfallNfAgc = desired.waterfallNfAgc
            restored.useSpectrumMinMax = desired.useSpectrumMinMax
            setSettings(restored, forPan: id)
        }
    }

    public func ownFields(forPan panId: String, among fields: Set<String>) -> Set<String> {
        initializeScope()
        let inherited = Self.dictionary(.desktopDefaults).merging(read(Self.masterKey)) { _, new in new }
        let own = normalizedOwn(panId, inherited: inherited)
        return Set(fields.intersection(Self.pictureFields).filter { own[$0] != nil })
    }
    public func resetPicture(forPan panId: String, fields: Set<String>) {
        initializeScope()
        let selected = Self.pictureSelection(fields)
        var own = read(Self.keyPrefix + panId)
        guard selected.contains(where: { own[$0] != nil }) else { return }
        for field in selected { own.removeValue(forKey: field) }
        write(own, key: Self.keyPrefix + panId)
        changed()
    }
    public func copyOwnPictureToMaster(forPan panId: String, fields: Set<String>) {
        let owns = ownFields(forPan: panId, among: Self.pictureSelection(fields))
        guard !owns.isEmpty else { return }
        let selected = Self.pictureSelection(owns)
        let effective = settings(forPan: panId)
        var master = masterSettings(forPan: panId)
        var object = Self.dictionary(master)
        let source = Self.dictionary(effective)
        for field in selected { object[field] = source[field] }
        // An own endpoint stops at the untouched master's other endpoint.
        if selected.contains("waterfallLowDbm") || selected.contains("waterfallHighDbm") {
            var low = (object["waterfallLowDbm"] as? NSNumber)?.doubleValue ?? master.waterfallLowDbm
            var high = (object["waterfallHighDbm"] as? NSNumber)?.doubleValue ?? master.waterfallHighDbm
            if selected.contains("waterfallLowDbm") { low = min(low, high - 5) }
            if selected.contains("waterfallHighDbm") { high = max(high, low + 5) }
            object["waterfallLowDbm"] = low
            object["waterfallHighDbm"] = high
        }
        object["storeSnapshotVersion"] = ["revision": revision, "token": 0]
        guard let data = try? JSONSerialization.data(withJSONObject: object),
              let decoded = try? JSONDecoder().decode(BandDisplaySettings.self, from: data) else { return }
        master = decoded
        setMasterSettings(master, fields: selected)
    }

    /// Forget only this pan's picture. Band, device and shared values survive.
    public func reset(pan panId: String) { resetPicture(forPan: panId, fields: Self.pictureFields) }
}
