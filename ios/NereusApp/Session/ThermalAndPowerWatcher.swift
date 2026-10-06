// NereusSDR for iOS: watches Low Power Mode, the phone's heat and whether it is charging
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMedia
import UIKit

/// What a long session reads of the phone's power (R-IOS-22, spec section
/// 5.5 items 10 and 11): Low Power Mode and the heat, which slow the band,
/// and whether the phone is charging, for Keep the screen on's While
/// charging. Each changes as iOS says so.
@MainActor
final class ThermalAndPowerWatcher: ObservableObject {
    /// One reading of the phone's power.
    struct Reading: Equatable, Sendable {
        var lowPowerMode: Bool
        var heat: SessionPolicy.Heat
        var charging: Bool

        /// A cool phone on battery, not in Low Power Mode.
        static let steady = Reading(lowPowerMode: false, heat: .nominal, charging: false)
    }

    @Published private(set) var reading: Reading

    private let read: @MainActor () -> Reading
    private let center: NotificationCenter
    private var watches: Set<AnyCancellable> = []

    /// Watches the phone itself.
    static func system() -> ThermalAndPowerWatcher {
        ThermalAndPowerWatcher(read: { Self.systemReading() }, center: .default)
    }

    /// Reads with `read` at the start and whenever `center` says the power
    /// state, the heat or the battery changed.
    init(read: @escaping @MainActor () -> Reading, center: NotificationCenter = NotificationCenter()) {
        self.read = read
        self.center = center
        reading = read()
        let names: [Notification.Name] = [.NSProcessInfoPowerStateDidChange, ProcessInfo.thermalStateDidChangeNotification,
                                          UIDevice.batteryStateDidChangeNotification]
        for name in names {
            center.publisher(for: name)
                .receive(on: DispatchQueue.main)
                .sink { [weak self] _ in self?.refresh() }
                .store(in: &watches)
        }
    }

    /// Reads the phone again.
    func refresh() {
        let next = read()
        if next != reading {
            reading = next
        }
    }

    /// The phone's power now. Battery monitoring is switched on here, since
    /// the battery's state reads unknown without it.
    static func systemReading() -> Reading {
        let device = UIDevice.current
        if !device.isBatteryMonitoringEnabled {
            device.isBatteryMonitoringEnabled = true
        }
        let process = ProcessInfo.processInfo
        return Reading(lowPowerMode: process.isLowPowerModeEnabled, heat: heat(process.thermalState),
                       charging: device.batteryState == .charging || device.batteryState == .full)
    }

    static func heat(_ state: ProcessInfo.ThermalState) -> SessionPolicy.Heat {
        switch state {
        case .nominal:
            return .nominal
        case .fair:
            return .fair
        case .serious:
            return .serious
        case .critical:
            return .critical
        @unknown default:
            return .critical
        }
    }
}
