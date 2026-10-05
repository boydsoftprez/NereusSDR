// NereusSDR for iOS: Setup, Diagnostics, Settings Validation: the Core's check of this radio's settings, Re-validate, Repair and Forget
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The Core's settings check for the connected radio: the problems it
/// found, each with its severity, summary and detail as plain text; the
/// Core's three actions in its order. Re-validate asks the Core to check
/// again. Repair and Forget each ask the Core's question first, with
/// Cancel the default, and send only if the session, the radio, this
/// phone's pairing and the radio being off the air all still hold; Repair
/// is greyed with the Core's reason below the Core version it needs. An
/// older Core's Reset stays greyed with its reason and is never sent. No
/// answer, or one that cannot be read, is never shown as a healthy radio.
struct SettingsHygienePanel: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher

    /// The Repair or Forget admitted when it was tapped, while its
    /// question is open.
    @State private var confirming: SetupAdmission?
    @State private var confirmingAction: SetupHygieneAction = .forget
    @State private var asking = false
    @State private var problem: String?

    static let noIssuesText = "No problems found in this radio's settings."
    static let criticalText = "Critical"
    static let warningText = "Warning"
    static let infoText = "Info"
    static let cancelText = "Cancel"

    var body: some View {
        let panel = dispatcher.hygiene(control, in: category)
        VStack(alignment: .leading, spacing: 10) {
            Text(control.label)
                .font(.body.weight(.semibold))
            report(panel)
            action(panel.validateLabel, reason: panel.validateReason, id: "validate", role: nil) {
                validate()
            }
            if !panel.repairLabel.isEmpty {
                action(panel.repairLabel, reason: panel.repairReason, id: "repair", role: nil) {
                    ask(.repair)
                }
            } else if !panel.resetLabel.isEmpty {
                action(panel.resetLabel, reason: panel.resetReason, id: "reset", role: nil) {}
            }
            action(panel.forgetLabel, reason: panel.forgetReason, id: "forget", role: .destructive) {
                ask(.forget)
            }
            if let problem {
                Text(problem)
                    .font(.footnote)
                    .foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier("\(control.id).problem")
            }
        }
        .alert(question(panel)?.title ?? "", isPresented: $asking) {
            Button(Self.cancelText, role: .cancel) { cancelQuestion() }
            if confirmingAction == .repair {
                Button(panel.repairLabel) { confirmQuestion() }
            } else {
                Button(panel.forgetLabel, role: .destructive) { confirmQuestion() }
            }
        } message: {
            Text(question(panel)?.message ?? "")
        }
        .accessibilityIdentifier(control.id)
    }

    @ViewBuilder
    private func report(_ panel: SetupHygienePanel) -> some View {
        if let report = panel.report {
            if report.issues.isEmpty {
                Text(Self.noIssuesText)
                    .accessibilityIdentifier("\(control.id).healthy")
            }
            ForEach(Array(report.issues.enumerated()), id: \.offset) { index, issue in
                VStack(alignment: .leading, spacing: 2) {
                    HStack(spacing: 6) {
                        Text(Self.severity(issue.severity))
                            .font(.caption.weight(.bold))
                            .foregroundStyle(issue.severity == .info ? Color.secondary : ConnectChrome.badText)
                        Text(issue.summary)
                            .font(.subheadline.weight(.semibold))
                    }
                    Text(issue.detail)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .accessibilityElement(children: .combine)
                .accessibilityIdentifier("\(control.id).issue.\(index)")
            }
        } else if let reason = panel.reportReason {
            Text(reason)
                .font(.footnote)
                .foregroundStyle(.secondary)
                .accessibilityIdentifier("\(control.id).reportReason")
        }
    }

    static func severity(_ severity: SetupHygieneIssue.Severity) -> String {
        switch severity {
        case .critical: return criticalText
        case .warning: return warningText
        case .info: return infoText
        }
    }

    private func action(_ label: String, reason: String?, id: String, role: ButtonRole?,
                        perform: @escaping () -> Void) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Button(label, role: role, action: perform)
                .buttonStyle(.bordered)
                .disabled(reason != nil)
                .accessibilityIdentifier("\(control.id).\(id)")
            if let reason {
                Text(reason)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .accessibilityIdentifier("\(control.id).\(id).reason")
            }
        }
    }

    // MARK: Actions

    private func validate() {
        problem = nil
        switch dispatcher.admitHygiene(control, in: category, action: .validate) {
        case .failure(let refusal):
            problem = refusal.reason
        case .success(let admission):
            let dispatcher = dispatcher
            Task { @MainActor in
                if case .notSent(let reason) = await dispatcher.performHygiene(admission),
                   dispatcher.hygiene(control, in: category).reportReason != reason {
                    problem = reason
                }
            }
        }
    }

    /// The Core's question for the open Repair or Forget.
    private func question(_ panel: SetupHygienePanel) -> SetupDescription.Confirmation? {
        confirmingAction == .repair ? panel.repairConfirmation : panel.confirmation
    }

    private func ask(_ hygieneAction: SetupHygieneAction) {
        problem = nil
        switch dispatcher.admitHygiene(control, in: category, action: hygieneAction) {
        case .failure(let refusal):
            problem = refusal.reason
        case .success(let admission):
            confirming = admission
            confirmingAction = hygieneAction
            asking = true
        }
    }

    private func cancelQuestion() {
        confirming?.revoke()
        confirming = nil
    }

    private func confirmQuestion() {
        guard let admission = confirming else { return }
        confirming = nil
        let dispatcher = dispatcher
        Task { @MainActor in
            switch await dispatcher.performHygiene(admission) {
            case .applied, .awaitingConfirmation:
                problem = nil
            case .refused(let reason), .notSent(let reason):
                problem = reason
            }
        }
    }
}
