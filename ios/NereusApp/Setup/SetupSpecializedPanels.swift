// NereusSDR for iOS: the one hook where a panel for a closed Setup control (notches, settings check, antennas, PA readings, CFC bands) plugs in
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Draws the closed controls a generic row cannot carry. `make`
/// returns the panel for one control, or nil, and then the page shows the
/// control visibly disabled with ``unavailableReason``. The app uses
/// ``live(_:)``; ``none`` leaves every closed control greyed.
struct SetupSpecializedPanels {
    typealias Make = @MainActor (_ kind: SetupSpecialized, _ control: SetupDescription.Control,
                                 _ category: String) -> AnyView?

    typealias MakePa = @MainActor (_ control: SetupDescription.Control, _ category: String) -> AnyView?
    let make: Make
    var makePa: MakePa?

    init(_ make: @escaping Make, makePa: MakePa? = nil) {
        self.make = make
        self.makePa = makePa
    }

    /// Includes malformed PA controls so the actual metadata refusal stays visible.
    static func isPaProfile(_ control: SetupDescription.Control) -> Bool {
        ["pa.gain.profile", "pa.gain.new", "pa.gain.copy", "pa.gain.delete", "pa.gain.reset", "pa.gain.table"].contains(control.id)
    }

    /// No panels: every closed control shows disabled, with its reason.
    static let none = SetupSpecializedPanels { _, _, _ in nil }

    /// What a closed control without its panel says.
    static let unavailableReason = SetupControlDispatcher.notOnThisPhoneReason

    /// The app's panels: the notch table, the settings check, the antenna
    /// tables, the PA readings and the CFC band editor, each reading from and sending to the
    /// Core through `app`'s dispatcher.
    @MainActor
    static func live(_ app: AppModel) -> SetupSpecializedPanels {
        let dispatcher = app.setupControls
        let clock = app.mirrorClock
        var panels = SetupSpecializedPanels { kind, control, category in
            switch kind {
            case .notchTable:
                return AnyView(NotchTablePanel(control: control, category: category, dispatcher: dispatcher))
            case .settingsHygiene:
                return AnyView(SettingsHygienePanel(control: control, category: category, dispatcher: dispatcher))
            case .antennaRows:
                return AnyView(AntennaRowsPanel(control: control, category: category, dispatcher: dispatcher))
            case .paTelemetry:
                return AnyView(PaTelemetryPanel(control: control, category: category, dispatcher: dispatcher,
                                                now: { clock.nowMilliseconds }))
            case .cfcBands:
                return AnyView(CfcBandsPanel(control: control, category: category, dispatcher: dispatcher))
            }
        }
        panels.makePa = { control, category in
            guard isPaProfile(control) else { return nil }
            return AnyView(PaProfilesPanel(control: control, category: category,
                                           pages: app.setupPages, dispatcher: dispatcher))
        }
        return panels
    }
}
