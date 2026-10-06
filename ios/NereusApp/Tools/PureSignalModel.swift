// NereusSDR for iOS: PureSignal on, off and its status, as the Core runs it; calibration stays at the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror

/// The PureSignal page's model (spec section 5.2 item 4, R-IOS-18):
/// PureSignal's automatic calibration on and off, through its owner, the
/// transmit controls (``TransmitModel/togglePsa()``: `ps3.automatic` and
/// `ps3.off`, link document section 9.1), and its status from the Core's
/// `pureSignal` object (`available`, `canActuate`, `statusJson`,
/// `lastActionError`). Single calibrations, saving and restoring
/// corrections and the two-tone test stay at the Core.
@MainActor
final class PureSignalModel: ObservableObject {
    // MARK: Words

    static let notConnectedReason = SpotsModel.notConnectedReason
    static let noStatusText = "The Core has not sent PureSignal's status."
    static let atCoreNote =
        "Calibration runs at the Core: single calibrations, saved corrections and the two-tone test are there."

    // MARK: State

    /// Automatic calibration is on at the Core (`pureSignalSettings.autoCalEnabled`).
    @Published private(set) var on = false
    /// The radio has PureSignal; false hides the controls (the radio has none).
    @Published private(set) var radioHasIt = true
    /// PureSignal is ready on the Core's radio (`available`).
    @Published private(set) var available = false
    /// The Core's status, when it sent one this page reads.
    @Published private(set) var status: PureSignalStatus?
    /// Why it cannot be turned on, or off, now; nil when it can.
    @Published private(set) var onReason: String?
    @Published private(set) var offReason: String?
    /// The Core's words for its last PureSignal action that failed, and the
    /// transmit controls' note for a request it refused.
    @Published private(set) var actionError: String?
    @Published private(set) var note: String?

    private let mirror: MirrorStore
    private let transmit: TransmitModel
    private let catalogFeed: CatalogFeed
    private var watch: ToolMirrorWatch?

    init(mirror: MirrorStore, transmit: TransmitModel, catalogFeed: CatalogFeed) {
        self.mirror = mirror
        self.transmit = transmit
        self.catalogFeed = catalogFeed
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        watch.watch(transmit.$psa)
        watch.watch(transmit.$psaReason)
        watch.watch(transmit.$note)
        watch.watch(catalogFeed.$catalog)
        self.watch = watch
        refresh()
    }

    func refresh() {
        let pureSignal = watch?.object(TransmitModel.pureSignalKey)
        _ = watch?.object(TransmitModel.pureSignalSettingsKey)
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<PureSignalModel, Value>, _ value: Value) {
            if self[keyPath: path] != value {
                self[keyPath: path] = value
            }
        }
        set(\.on, connected && transmit.psa)
        set(\.radioHasIt, catalogFeed.catalog?.board.pureSignal != false)
        set(\.available, connected && ToolValue.flag(pureSignal?["available"]) == true)
        set(\.status, connected ? ToolValue.text(pureSignal?["statusJson"]).flatMap(PureSignalStatus.parse) : nil)
        let error = ToolValue.text(pureSignal?["lastActionError"]) ?? ""
        set(\.actionError, connected && !error.isEmpty ? error : nil)
        set(\.note, transmit.note)
        if !connected {
            set(\.onReason, Self.notConnectedReason)
            set(\.offReason, Self.notConnectedReason)
        } else {
            set(\.onReason, transmit.psaReason)
            set(\.offReason, transmit.psaVerbsOffered ? nil : TransmitModel.psaNotOfferedText)
        }
    }

    /// Turns automatic calibration on, or off, at the Core.
    func set(on next: Bool) {
        guard next != on else {
            return
        }
        if next {
            guard onReason == nil else {
                return
            }
        } else {
            guard offReason == nil else {
                return
            }
        }
        transmit.togglePsa()
    }

    /// The state in a few words: what the Core is doing with PureSignal.
    var stateText: String {
        guard let status else {
            return on ? "On" : "Off"
        }
        if status.calibrating {
            return "Calibrating"
        }
        if status.correctionsApplied {
            return "Correcting"
        }
        return on ? "On, waiting to calibrate" : "Off"
    }
}
