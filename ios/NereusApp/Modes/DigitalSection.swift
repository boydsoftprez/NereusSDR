// NereusSDR for iOS: the Modes tab's digital settings: the DIG offset (DIGL, DIGU) and RTTY mark and shift (DIGL)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The DIG / RTTY section (I9): the slice's DIG offset for DIGL or DIGU,
/// 10 Hz a step, and RTTY's mark (25 Hz a step) and shift (5 Hz a step)
/// for DIGL, each moved by its arrows or typed in, as the desktop flag's
/// DIG and RTTY containers move them. In other modes they stay here,
/// greyed with the reason. The flag's FM container is not built on the
/// desktop, so the phone has none.
struct DigitalSection: View {
    @ObservedObject var model: ModesTabModel

    var body: some View {
        ModesChrome.section("DIG / RTTY") {
            row("DIG offset", value: model.digOffsetHz, signed: true, live: model.digOffsetReason == nil,
                identifier: "modesDigOffset", open: model.openDigOffsetPad) { model.stepDigOffset(up: $0) }
            if let reason = model.digOffsetReason {
                ModesChrome.note(reason)
            }
            row("Mark", value: model.rttyMarkHz, signed: false, live: model.rttyReason == nil,
                identifier: "modesRttyMark", open: { model.openRttyPad(mark: true) }) {
                model.stepRtty(mark: true, up: $0)
            }
            row("Shift", value: model.rttyShiftHz, signed: false, live: model.rttyReason == nil,
                identifier: "modesRttyShift", open: { model.openRttyPad(mark: false) }) {
                model.stepRtty(mark: false, up: $0)
            }
            if let reason = model.rttyReason {
                ModesChrome.note(reason)
            }
        }
    }

    private func row(_ name: String, value: Int64?, signed: Bool, live: Bool, identifier: String,
                     open: @escaping () -> Void, step: @escaping (Bool) -> Void) -> some View {
        let on = live && value != nil
        return HStack(spacing: 6) {
            ModesChrome.label(name)
            ModesChrome.arrow(left: true, accessibility: "\(name) down", disabled: !on) { step(false) }
            ValueField(text: ModesChrome.hertz(value, signed: signed), accessibility: name, disabled: !on, grow: true,
                       open: open)
                .accessibilityIdentifier(identifier)
            ModesChrome.arrow(left: false, accessibility: "\(name) up", disabled: !on) { step(true) }
        }
    }
}
