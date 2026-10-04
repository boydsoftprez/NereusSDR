// NereusSDR for iOS: Setup's Diagnostics, Logs: the Core's log as it happens, Reload and Clear, and its log categories
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Setup, Diagnostics, Logs (D95, R-IOS-36, spec section 5.2 item 11): the
/// Core's log as it happens, newest at the foot, asked for only while this
/// page is open; Reload reads it again and Clear empties this phone's view,
/// leaving the Core's log as it is. Below, the Core's log categories as
/// switches, with Turn all on and Turn all off. This phone's own log is not
/// here; it goes in the Support Bundle. A Core that does not share its log
/// leaves every control shown, greyed, with the reason.
struct CoreLogsPage: View {
    @StateObject private var model: CoreLogsModel

    static let title = "Logs"
    static let logHeading = "The Core's log"
    static let categoriesHeading = "The Core's log categories"
    static let reloadText = "Reload"
    static let clearText = "Clear"
    static let allOnText = "Turn all on"
    static let allOffText = "Turn all off"
    static let clearNote = "Clear empties this view on this phone. The Core keeps its log."
    static let categoriesNote = "Turning a category on adds its lines to the Core's log for every device."
    static let phoneLogNote = "This phone's own log goes in the Support Bundle, in Tools."

    init(log: CoreLogModel) {
        _model = StateObject(wrappedValue: CoreLogsModel(log: log))
    }

    var body: some View {
        let log = model.log
        let enabled = log.reason == nil
        List {
            Section {
                logView(enabled: enabled)
                HStack(spacing: 12) {
                    button(Self.reloadText, enabled: model.canReload, id: "coreLogs.reload") { model.reload() }
                    button(Self.clearText, enabled: model.canClear, id: "coreLogs.clear") { model.clear() }
                }
            } header: {
                Text(Self.logHeading)
            } footer: {
                Text(Self.clearNote)
            }
            Section {
                ForEach(log.categories) { category in
                    Toggle(category.title, isOn: Binding(get: { log.on.contains(category.id) },
                                                         set: { log.setCategory(category.id, $0) }))
                        .disabled(!enabled)
                        .accessibilityIdentifier("coreLogs.category.\(category.id)")
                }
                HStack(spacing: 12) {
                    button(Self.allOnText, enabled: enabled, id: "coreLogs.allOn") { log.setAll(true) }
                    button(Self.allOffText, enabled: enabled, id: "coreLogs.allOff") { log.setAll(false) }
                }
            } header: {
                Text(Self.categoriesHeading)
            } footer: {
                VStack(alignment: .leading, spacing: 6) {
                    if let reason = log.reason {
                        Text(reason)
                            .accessibilityIdentifier("coreLogs.reason")
                    } else if let note = log.note {
                        Text(note)
                            .foregroundStyle(ConnectChrome.badText)
                            .accessibilityIdentifier("coreLogs.note")
                    }
                    Text(Self.categoriesNote)
                    Text(Self.phoneLogNote)
                }
            }
        }
        .navigationTitle(Self.title)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .core)
            }
        }
        .onAppear { model.setOpen(true) }
        .onDisappear { model.setOpen(false) }
    }

    /// The log, newest line at the foot, following new lines as they come.
    private func logView(enabled: Bool) -> some View {
        ScrollViewReader { reader in
            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    Text(model.text)
                        .font(.system(size: 11).monospaced())
                        .foregroundStyle(enabled && !model.lines.isEmpty ? ChromeColours.text : ChromeColours.textDim)
                        .textSelection(.enabled)
                        .fixedSize(horizontal: false, vertical: true)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(8)
                    Color.clear.frame(height: 1).id(Self.logEnd)
                }
            }
            .frame(height: 320)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
            .accessibilityIdentifier("coreLogs.log")
            .onChange(of: model.lines.last?.id, initial: true) { _, _ in
                reader.scrollTo(Self.logEnd, anchor: .bottomLeading)
            }
        }
        .listRowInsets(EdgeInsets(top: 8, leading: 8, bottom: 8, trailing: 8))
    }

    private static let logEnd = "coreLogs.log.end"

    /// A button in a list row; each button in the row is pressed on its own.
    private func button(_ title: String, enabled: Bool, id: String,
                        action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.body.weight(.semibold))
                .frame(maxWidth: .infinity, minHeight: 34)
        }
        .buttonStyle(.bordered)
        .disabled(!enabled)
        .accessibilityIdentifier(id)
    }
}
