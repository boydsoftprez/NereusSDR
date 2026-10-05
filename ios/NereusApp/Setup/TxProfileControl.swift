// NereusSDR for iOS: native TX Profile naming and save/discard/cancel questions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI
import UIKit

/// The existing described command rows with the Core's prompt metadata.
/// Each alert is attached to the row whose gesture opened it.
struct TxProfileControl: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher
    @ObservedObject var flow: SetupTxProfileFlow
    let owner: UUID

    private var isChoice: Bool { control.modern?.profileUnsavedChanges != nil }
    private var question: SetupTxProfileFlow.Question? {
        guard flow.presentationOwner == owner, let question = flow.question else { return nil }
        switch question {
        case .unsaved: return isChoice ? question : nil
        case .name, .overwrite: return isChoice ? nil : question
        }
    }
    var body: some View {
        let state = dispatcher.state(of: control, in: category)
        VStack(alignment: .leading, spacing: 4) {
            if isChoice {
                Picker(control.label, selection: Binding(get: { flow.currentName }, set: { flow.choose($0, owner: owner) })) {
                    ForEach(dispatcher.resolved(control).options ?? [], id: \.value) { option in
                        Text(option.label).tag(option.label)
                    }
                }
                .pickerStyle(.menu)
                .disabled(!state.editable || flow.busy)
                .accessibilityIdentifier(control.id)
            } else {
                Button(control.label) { flow.beginSave(control, in: category, owner: owner) }
                    .buttonStyle(.bordered)
                    .disabled(!state.editable || flow.busy)
                    .accessibilityIdentifier(control.id)
            }
            if !control.tooltip.isEmpty {
                Text(control.tooltip).font(.footnote).foregroundStyle(.secondary).lineLimit(3)
            }
            if let reason = state.reason {
                Text(reason).font(.footnote).foregroundStyle(.secondary)
                    .accessibilityIdentifier(control.id + ".reason")
            }
            if let problem = flow.problem(for: owner), flow.problemControlId == control.id {
                Text(problem).font(.footnote).foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier(control.id + ".problem")
            }
        }
        .background(TxProfileQuestionPresenter(flow: flow, question: question, controlId: control.id)
            .frame(width: 0, height: 0))
    }
}

/// UIKit's native alerts serialize dismissal before presenting the next question.
/// That completion is needed for the name -> overwrite sequence on both devices.
struct TxProfileQuestionPresenter: UIViewControllerRepresentable {
    let flow: SetupTxProfileFlow
    let question: SetupTxProfileFlow.Question?
    let controlId: String

    func makeUIViewController(context: Context) -> Presenter { Presenter() }
    func updateUIViewController(_ presenter: Presenter, context: Context) {
        presenter.configure(flow: flow, question: question, controlId: controlId)
    }
    static func dismantleUIViewController(_ presenter: Presenter, coordinator: ()) {
        presenter.retire()
    }

    final class Presenter: UIViewController {
        private weak var flow: SetupTxProfileFlow?
        private weak var ownedAlert: UIAlertController?
        private var desired: SetupTxProfileFlow.Question?
        private var identity: UUID?
        private var shown: SetupTxProfileFlow.Question?
        private var shownIdentity: UUID?
        private var controlId = ""
        private var changing = false
        override func viewDidAppear(_ animated: Bool) { super.viewDidAppear(animated); reconcile() }
        func configure(flow: SetupTxProfileFlow, question: SetupTxProfileFlow.Question?, controlId: String) {
            self.flow = flow; desired = question; identity = flow.questionIdentity; self.controlId = controlId
            reconcile()
        }
        func retire() { desired = nil; identity = nil; reconcile() }
        private func reconcile() {
            guard !changing, viewIfLoaded?.window != nil else { return }
            if let presentedViewController {
                // A sibling row can see the same ancestor's alert; it does not own it.
                guard presentedViewController === ownedAlert else { return }
                guard shown != desired || shownIdentity != identity else { return }
                changing = true
                presentedViewController.dismiss(animated: true) { [weak self] in
                    guard let self else { return }
                    self.ownedAlert = nil
                    self.shown = nil; self.shownIdentity = nil; self.changing = false; self.reconcile()
                }
                return
            }
            guard let desired, let identity, let flow else { return }
            let alert = UIAlertController(title: desired.title, message: desired.text, preferredStyle: .alert)
            let expected = identity
            func action(_ title: String, id: String, cancel: Bool = false,
                        _ body: @escaping @MainActor () -> Void) -> UIAlertAction {
                let action = UIAlertAction(title: title, style: cancel ? .cancel : .default) { [weak flow] _ in
                    guard let flow, flow.questionIdentity == expected, flow.question == desired else { return }
                    body()
                }
                action.accessibilityIdentifier = controlId + "." + id
                alert.addAction(action)
                return action
            }
            switch desired {
            case .name(let prompt):
                alert.addTextField { field in
                    field.placeholder = prompt.label; field.text = flow.name
                    field.accessibilityLabel = prompt.label
                    field.accessibilityIdentifier = self.controlId + ".name"
                }
                _ = action("Save", id: "saveName") { [weak alert] in
                    flow.name = alert?.textFields?.first?.text ?? ""
                    Task { @MainActor in
                        guard flow.questionIdentity == expected, flow.question == desired else { return }
                        await flow.acceptName()
                    }
                }
                _ = action("Cancel", id: "cancel", cancel: true) { flow.cancel() }
            case .overwrite:
                _ = action(DescribedControl.yesText, id: "yes") { Task { @MainActor in
                        guard flow.questionIdentity == expected, flow.question == desired else { return }
                        await flow.confirmOverwrite()
                    } }
                alert.preferredAction = action(DescribedControl.noText, id: "no", cancel: true) { flow.cancel() }
            case .unsaved:
                _ = action("Save", id: "saveChanges") { Task { @MainActor in
                        guard flow.questionIdentity == expected, flow.question == desired else { return }
                        await flow.saveAndSwitch()
                    } }
                _ = action("Discard", id: "discard") { Task { @MainActor in
                        guard flow.questionIdentity == expected, flow.question == desired else { return }
                        await flow.discardAndSwitch()
                    } }
                _ = action("Cancel", id: "cancel", cancel: true) { flow.cancel() }
            }
            shown = desired; shownIdentity = identity
            ownedAlert = alert
            present(alert, animated: true)
        }
    }
}

/// The initiating existing screen owns its question; disappearing old
/// screens cannot retire another screen's pending decision or result note.
struct TxProfileSurfaceQuestion: View {
    @ObservedObject var flow: SetupTxProfileFlow
    let owner: UUID
    let identifier: String
    var body: some View {
        TxProfileQuestionPresenter(flow: flow,
            question: flow.presentationOwner == owner ? flow.question : nil, controlId: identifier)
            .frame(width: 0, height: 0)
            .onDisappear { flow.retire(owner: owner) }
    }
}
