// NereusSDR for iOS: the Modes tab's RIT and XIT: each switch, its offset by the slice's step or typed, and its 0
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The RIT / XIT section: each switch, lit amber while on, and its offset,
/// moved by the slice's tuning step with the arrows or typed in, within
/// -10000 to 10000 Hz, and a "0" that clears it, as the desktop flag's.
struct RitXitSection: View {
    @ObservedObject var model: ModesTabModel

    var body: some View {
        ModesChrome.section("RIT / XIT") {
            row("RIT", on: model.ritOn, hz: model.ritHz, toggle: model.toggleRit, open: model.openRitPad,
                clear: model.clearRit) { model.stepRit(up: $0) }
            row("XIT", on: model.xitOn, hz: model.xitHz, toggle: model.toggleXit, open: model.openXitPad,
                clear: model.clearXit) { model.stepXit(up: $0) }
            if model.olderCore.contains(ModesTabModel.Property.ritHz)
                || model.olderCore.contains(ModesTabModel.Property.xitHz) {
                ModesChrome.note(CatalogFeed.needsNewerCoreText)
            }
        }
    }

    private func row(_ name: String, on: Bool?, hz: Int64?, toggle: @escaping () -> Void, open: @escaping () -> Void,
                     clear: @escaping () -> Void, step: @escaping (Bool) -> Void) -> some View {
        let live = hz != nil && model.stepHz != nil
        return HStack(spacing: 6) {
            PanelButton(label: name, lit: on == true, style: .amber, disabled: on == nil, action: toggle)
                .frame(width: 56)
            ModesChrome.arrow(left: true, accessibility: "\(name) down", disabled: !live) { step(false) }
            ValueField(text: ModesChrome.hertz(hz, signed: true), accessibility: "\(name) offset", disabled: hz == nil,
                       grow: true, open: open)
                .accessibilityIdentifier("modes\(name)Offset")
            ModesChrome.arrow(left: false, accessibility: "\(name) up", disabled: !live) { step(true) }
            PanelButton(label: "0", lit: false, style: .blue, disabled: hz == nil, action: clear)
                .frame(width: 36)
                .accessibilityLabel("Clear \(name)")
                .accessibilityIdentifier("modes\(name)Clear")
        }
    }
}
