// NereusSDR for iOS: Calibration's Level Cal rows: the slice, frequency and level, the run's buttons, bar and status
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Level Cal group on Setup > Hardware > Calibration, in the desktop's
/// order (CalibrationTab.cpp): which slice it calibrates (the phone's
/// words), Frequency and Level, then the Core's own rows (Rx1 6m LNA, from
/// Setup description 23), then the desktop's disabled Rx2 6m LNA, Start,
/// Reset and Cancel with the greyed ones' reasons, the bar and the Core's
/// status line.
enum LevelCalSection {
    /// The desktop group's title, which the Core's page uses from version 23.
    static let title = "Level Cal"
    /// The Core's page the group sits on.
    static let pageId = "hardware.calibration"

    /// The rows above the Core's own.
    struct Top: View {
        @ObservedObject var model: LevelCalModel

        var body: some View {
            if let line = model.sliceLine {
                Text(line)
                    .accessibilityIdentifier("levelCalSlice")
            }
            SetupNumberRow(title: LevelCalModel.frequencyLabel, value: model.frequencyHz,
                           range: LevelCalModel.frequencyRange, step: LevelCalModel.frequencyStep, unit: "Hz",
                           id: "levelCalFrequency") { model.frequencyHz = $0 }
            SetupNumberRow(title: LevelCalModel.levelLabel, value: model.levelDbm, range: LevelCalModel.levelRange,
                           step: LevelCalModel.levelStep, unit: "dBm", decimals: 1,
                           id: "levelCalLevel") { model.levelDbm = $0 }
        }
    }

    /// The rows below the Core's own.
    struct Run: View {
        @ObservedObject var model: LevelCalModel

        var body: some View {
            SetupNumberRow(title: LevelCalModel.rx2LnaLabel, value: LevelCalModel.rx2LnaShown, range: 0...25, step: 1,
                           unit: "dB", decimals: 1, enabled: false, id: "levelCalRx2Lna") { _ in }
            Text(LevelCalModel.rx2LnaNote)
                .font(.footnote)
                .foregroundStyle(.secondary)
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 8) {
                    button(LevelCalModel.startLabel, enabled: model.startEnabled, prominent: true,
                           hint: LevelCalModel.startHint, id: "levelCalStart") { model.start() }
                    button(LevelCalModel.resetLabel, enabled: model.resetEnabled, prominent: false,
                           hint: LevelCalModel.resetHint, id: "levelCalReset") { model.reset() }
                    button(LevelCalModel.cancelLabel, enabled: model.cancelEnabled, prominent: false,
                           hint: LevelCalModel.cancelHint, id: "levelCalCancel") { model.cancel() }
                }
                ForEach(model.reasons, id: \.self) { reason in
                    Text(reason)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                HStack(spacing: 10) {
                    ProgressView(value: Double(model.percent), total: 100)
                        .accessibilityLabel("Level calibration progress")
                    Text("\(model.percent)%")
                        .font(.footnote.monospacedDigit().bold())
                        .accessibilityHidden(true)
                }
                .accessibilityIdentifier("levelCalProgress")
                if !model.message.isEmpty {
                    Text(model.message)
                        .font(.footnote)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("levelCalStatus")
                }
            }
            .padding(.vertical, 4)
        }

        @ViewBuilder
        private func button(_ label: String, enabled: Bool, prominent: Bool, hint: String, id: String,
                            action: @escaping () -> Void) -> some View {
            let base = Button(action: action) {
                Text(label)
                    .frame(maxWidth: .infinity)
            }
            .disabled(!enabled)
            .accessibilityHint(enabled ? hint : "")
            .accessibilityIdentifier(id)
            if prominent {
                base.buttonStyle(.borderedProminent)
            } else {
                base.buttonStyle(.bordered)
            }
        }
    }

    /// The questions and answers, as the desktop asks and answers them.
    struct Alerts: ViewModifier {
        @ObservedObject var model: LevelCalModel

        func body(content: Content) -> some View {
            content.alert(model.alert?.title ?? "", isPresented: Binding(
                get: { model.alert != nil },
                set: { if !$0 { model.dismissAlert() } }
            ), presenting: model.alert) { alert in
                switch alert.kind {
                case .startQuestion:
                    Button("Yes") { model.confirmStart() }
                    Button("No", role: .cancel) { model.dismissAlert() }
                case .resetQuestion:
                    Button("Yes") { model.confirmReset() }
                    Button("No", role: .cancel) { model.dismissAlert() }
                case .message:
                    Button("OK", role: .cancel) { model.dismissAlert() }
                }
            } message: { alert in
                Text(alert.text)
            }
        }
    }
}

extension View {
    /// The Level Cal questions and answers, when the page has the group.
    @ViewBuilder
    func levelCalAlerts(_ model: LevelCalModel?) -> some View {
        if let model {
            modifier(LevelCalSection.Alerts(model: model))
        } else {
            self
        }
    }
}
