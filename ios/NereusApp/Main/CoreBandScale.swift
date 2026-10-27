// NereusSDR for iOS: each band's dB Max and dB Min, kept by the Core for every device and drawn on the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror

/// The Core's per-band dB Max and dB Min (DC3, R-IOS-56): the scale the
/// desktop keeps for each band in `DisplayGridMax_<band>` and
/// `DisplayGridMin_<band>`, station settings shared by every device. On a
/// Core whose Setup description reaches version 29 the band draws the
/// Core's scale for the band it is on (``BandModel/sharedScale``), and
/// Top, Range, the dBm arrows, a drag on the scale and Setup's dB Max and
/// dB Min write it through the settings proxy, one end at a time as the
/// desktop writes it. A written value shows at once, marked not confirmed
/// until the Core echoes it; a refusal puts the Core's value back and shows
/// the Core's words (``refusal(band:)``).
///
/// An older Core keeps the phone's own Top and Bottom, with
/// ``olderCoreText`` under them, and is sent nothing. While the link is
/// lost the band keeps the Core's last scale and the rows are greyed with
/// ``notConnectedText``.
@MainActor
final class CoreBandScale: ObservableObject {
    /// Why Top and Range stay on this phone on an older Core.
    static let olderCoreText = "Needs a newer Core to share the scale"
    /// Why the scale's rows are greyed while the link is lost.
    static let notConnectedText = "Not connected"
    /// The Setup description version whose Core shares each band's scale.
    static let sharedVersion: Int64 = 29
    static let versionCapability = "setupDescriptionVersion"

    /// The desktop's dB Max and dB Min for a band with nothing stored.
    // From src/models/PanadapterModel.cpp:101-102 (the per-band dB Max and dB Min defaults)
    static let defaultMaxDbm = -40.0
    static let defaultMinDbm = -140.0

    /// The Core's band number for XVTR, whose grid 2 m shares.
    private static let xvtrBand = 13

    /// Set whenever what the scale shows may have changed.
    @Published private(set) var revision = 0

    private let band: BandModel
    private let mirror: MirrorStore
    private let settingsProxy: SettingsProxyClient
    private let coreDisplay: CoreDisplayModel
    /// What this phone wrote to each key, shown until the Core answers it:
    /// the settings proxy takes a written value only once its write starts.
    private var pending: [String: String] = [:]
    private var watches: Set<AnyCancellable> = []

    init(band: BandModel, mirror: MirrorStore, settingsProxy: SettingsProxyClient, coreDisplay: CoreDisplayModel) {
        self.band = band
        self.mirror = mirror
        self.settingsProxy = settingsProxy
        self.coreDisplay = coreDisplay
        // A published value reaches its sinks before it is stored: each
        // looks again once it has been.
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        settingsProxy.$values.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        settingsProxy.$unconfirmedKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        settingsProxy.$currentSnapshotIdentity.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        coreDisplay.$revision.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        coreDisplay.$refusals.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        band.$settings.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
    }

    // MARK: The keys

    /// The band whose grid `band` reads and writes: its own, or for 2 m
    /// XVTR's, as the desktop keeps no grid of its own for 2 m.
    // From src/models/PanadapterModel.cpp:114 (gridSlot)
    private static func gridSlot(_ band: Int) -> Int {
        band == SetupDescription.twoMetreBand ? xvtrBand : band
    }

    /// The Core's dB Max for the Core's band `band`; nil for a band it does not number.
    // From src/models/PanadapterModel.cpp:116 (gridMaxKey)
    static func maxKey(band: Int) -> String? {
        SetupDescription.bandName(gridSlot(band)).map { "DisplayGridMax_" + $0 }
    }

    /// The Core's dB Min for the Core's band `band`; nil for a band it does not number.
    // From src/models/PanadapterModel.cpp:117 (gridMinKey)
    static func minKey(band: Int) -> String? {
        SetupDescription.bandName(gridSlot(band)).map { "DisplayGridMin_" + $0 }
    }

    // MARK: What the Core shares

    /// The Core keeps each band's scale for every device: its Setup
    /// description reaches version 29.
    var isShared: Bool { mirror.capabilityVersion(Self.versionCapability) >= Self.sharedVersion }

    /// The link is up, so a write can reach the Core.
    var isConnected: Bool { settingsProxy.currentSnapshotIdentity != nil }

    /// The Core's band number for the band shown on the pan; nil before the Core names one.
    var shownBand: Int? { band.settings.scaleBand.flatMap { Int($0) } }

    /// The band's dB Max and dB Min: what this phone wrote and the Core has
    /// not answered, else the Core's, else the desktop's default; nil for a
    /// band the Core does not number.
    func range(band: Int) -> (maxDbm: Double, minDbm: Double)? {
        guard let maxKey = Self.maxKey(band: band), let minKey = Self.minKey(band: band) else {
            return nil
        }
        return (stored(maxKey) ?? Self.defaultMaxDbm, stored(minKey) ?? Self.defaultMinDbm)
    }

    /// The value shown for `key` now; nil when the Core keeps none.
    private func stored(_ key: String) -> Double? {
        shownText(key)
            .flatMap { Double($0.trimmingCharacters(in: .whitespaces)) }
            .flatMap { $0.isFinite ? $0 : nil }
    }

    /// The text shown for `key`: what this phone wrote while the Core has
    /// not answered it, else the Core's. A write the Core has answered
    /// shows the Core's value at once, before ``refresh()`` forgets it.
    private func shownText(_ key: String) -> String? {
        let written = coreDisplay.isAwaitingAnswer(key) ? pending[key] : nil
        return written ?? coreDisplay.shownSetting(key)
    }

    /// Writes the band's dB Max and dB Min to the Core, as whole dB, as the
    /// desktop's storePerBandDb does: only the end that changed, unless
    /// the other is not stored yet, and nothing when neither changed or
    /// the top is not over the bottom.
    // From src/models/PanadapterModel.cpp:258-305 (setPerBandDbRange, storePerBandDb)
    func set(maxDbm: Double, minDbm: Double, band: Int) {
        guard isShared, isConnected, maxDbm.isFinite, minDbm.isFinite,
              let maxKey = Self.maxKey(band: band), let minKey = Self.minKey(band: band),
              let old = range(band: band) else {
            return
        }
        let newMax = Int(maxDbm.rounded())
        let newMin = Int(minDbm.rounded())
        let oldMax = Int(old.maxDbm.rounded())
        let oldMin = Int(old.minDbm.rounded())
        guard newMax > newMin, oldMax != newMax || oldMin != newMin else {
            return
        }
        writeEnd(maxKey, newMax, changed: oldMax != newMax)
        writeEnd(minKey, newMin, changed: oldMin != newMin)
        refresh()
    }

    private func writeEnd(_ key: String, _ value: Int, changed: Bool) {
        if let kept = shownText(key),
           !changed || Double(kept.trimmingCharacters(in: .whitespaces)) == Double(value) {
            return
        }
        let text = String(value)
        if coreDisplay.writeSetting(key, text) {
            pending[key] = text
        }
    }

    /// A write of the band's scale has not been answered by the Core yet.
    func isUnconfirmed(band: Int) -> Bool {
        [Self.maxKey(band: band), Self.minKey(band: band)].compactMap { $0 }.contains { key in
            coreDisplay.isAwaitingAnswer(key) || settingsProxy.isUnconfirmed(key)
        }
    }

    /// The Core's words for its refusal of the band's latest scale write;
    /// nil once it takes one.
    func refusal(band: Int) -> String? {
        [Self.maxKey(band: band), Self.minKey(band: band)].compactMap { $0 }
            .lazy.compactMap { self.coreDisplay.refusalReason($0) }.first
    }

    // MARK: The band shown on the pan

    /// The shown band's scale on a Core that shares it; nil otherwise.
    var shownRange: (maxDbm: Double, minDbm: Double)? {
        guard isShared, let shown = shownBand else {
            return nil
        }
        return range(band: shown)
    }

    /// Writes the shown band's scale on a Core that shares it, and says it
    /// took the change: while the link is lost nothing is written and the
    /// phone's own scale is left as it is. False on an older Core, or before
    /// the Core names the band, when the phone's own scale takes it.
    func setShown(maxDbm: Double, minDbm: Double) -> Bool {
        guard isShared, let shown = shownBand, Self.maxKey(band: shown) != nil else {
            return false
        }
        set(maxDbm: maxDbm, minDbm: minDbm, band: shown)
        return true
    }

    /// The scale's rows are greyed: the Core shares the scale and the link is lost.
    var greyed: Bool { isShared && !isConnected }

    /// The shown band's scale is waiting for the Core's answer.
    var shownUnconfirmed: Bool {
        guard isShared, let shown = shownBand else {
            return false
        }
        return isUnconfirmed(band: shown)
    }

    /// The line under the scale's rows: why they are greyed, the Core's
    /// refusal, or on an older Core that the scale is this phone's; nil
    /// when there is nothing to say.
    var shownNote: String? {
        guard isShared else {
            return isConnected ? Self.olderCoreText : nil
        }
        guard isConnected else {
            return Self.notConnectedText
        }
        return shownBand.flatMap { refusal(band: $0) }
    }

    // MARK: Keeping the band in step

    private func queueRefresh() {
        Task { @MainActor [weak self] in
            self?.refresh()
        }
    }

    /// Forgets what the Core has answered and hands the band the scale to draw.
    private func refresh() {
        for key in pending.keys where !coreDisplay.isAwaitingAnswer(key) {
            pending.removeValue(forKey: key)
        }
        let shared = shownRange.map { BandDisplaySettings.BandScale(topDbm: $0.maxDbm, bottomDbm: $0.minDbm) }
        if band.sharedScale != shared {
            band.sharedScale = shared
        }
        revision &+= 1
    }
}
