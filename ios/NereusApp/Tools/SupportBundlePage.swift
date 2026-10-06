// NereusSDR for iOS: Support Bundle, one Tools page: collect and share the logs, the Core's recent log and its logging
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import UIKit

/// Support Bundle on the phone (spec section 5.2 item 4, R-IOS-18): what
/// goes in, a Collect button, then the files with a Share button that opens
/// the share sheet. The app sends nothing anywhere itself. Below it, as the
/// desktop's Support dialog shows them for a Core: the Core's logging
/// categories, and the Core's recent log with Copy and Reload.
struct SupportBundlePage: View {
    @ObservedObject var model: SupportBundleModel
    /// The app's one model of the Core's log, shared with Setup's Logs page.
    @ObservedObject var log: CoreLogModel
    @State private var copied = false

    init(model: SupportBundleModel, log: CoreLogModel) {
        self.model = model
        self.log = log
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "What goes in", tag: .both)
            SpotHubPage.Card {
                ToolPageParts.Reading(title: "This phone's log", value: "Included", identifier: "support.phone")
                SpotHubPage.Line()
                ToolPageParts.Reading(title: "The Core's bundle", value: model.coreReason == nil ? "Included" : "Not included",
                                      identifier: "support.core")
                if let reason = model.coreReason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "support.coreReason")
                }
            }
            Button {
                Task { await model.collect() }
            } label: {
                HStack(spacing: 8) {
                    if model.state == .collecting {
                        ProgressView().tint(Color.white)
                    }
                    Text(model.state == .collecting ? "Collecting" : model.state == .ready ? "Collect again" : "Collect")
                        .font(.system(size: 14, weight: .bold))
                }
                .foregroundStyle(Color.white)
                .frame(maxWidth: .infinity, minHeight: 40)
                .background(ChromeColours.buttonOnBlue, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonOnBlueBorder, lineWidth: 1))
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(model.state == .collecting)
            .accessibilityIdentifier("support.collect")
            .padding(.top, 4)
            if model.state == .ready {
                SpotHubPage.Heading(text: "Ready to share", tag: .thisPhone).padding(.top, 8)
                SpotHubPage.Card {
                    ForEach(Array(model.files.enumerated()), id: \.element) { index, file in
                        if index > 0 {
                            SpotHubPage.Line()
                        }
                        ToolPageParts.Reading(title: file.lastPathComponent, value: Self.size(file),
                                              identifier: "support.file\(index)")
                    }
                    if let note = model.coreNote {
                        SpotHubPage.Line()
                        ToolPageParts.Reason(text: note, identifier: "support.coreNote")
                    }
                }
                ShareLink(items: model.files) {
                    Label("Share", systemImage: "square.and.arrow.up")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(ChromeColours.text)
                        .frame(maxWidth: .infinity, minHeight: 40)
                        .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonBorder,
                                                                                lineWidth: 1))
                }
                .disabled(model.files.isEmpty)
                .accessibilityIdentifier("support.share")
            }
            ConnectChrome.Note(text: SupportBundleModel.nothingSentNote).padding(.top, 4)
            categoriesCard
            coreLogCard
            if let note = log.note {
                ToolPageParts.Refusal(text: note, identifier: "support.logNote").padding(.top, 4)
            }
        }
        .onAppear { log.setOpen(true) }
        .onDisappear { log.setOpen(false) }
    }

    // MARK: The Core's logging

    private var categoriesCard: some View {
        let enabled = log.reason == nil
        return VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "The Core's logging", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 5), count: 3), spacing: 5) {
                    ForEach(log.categories) { category in
                        categoryButton(category, enabled: enabled)
                    }
                }
                .padding(10)
                SpotHubPage.Line()
                HStack(spacing: 5) {
                    smallButton("Turn all on", enabled: enabled, identifier: "support.categoriesAllOn") {
                        log.setAll(true)
                    }
                    smallButton("Turn all off", enabled: enabled, identifier: "support.categoriesAllOff") {
                        log.setAll(false)
                    }
                }
                .padding(10)
                if let reason = log.reason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "support.categoriesReason")
                }
            }
        }
    }

    private func categoryButton(_ category: CoreLogModel.Category, enabled: Bool) -> some View {
        CategoryButton(title: category.title, isOn: log.on.contains(category.id), enabled: enabled,
                       identifier: "support.category.\(category.id)") {
            log.setCategory(category.id, !log.on.contains(category.id))
        }
    }

    /// One of the Core's logging categories, blue while it is on. Greyed,
    /// one that is on keeps a lighter grey outline and text, as the Tools
    /// pages' greyed choices do.
    struct CategoryButton: View {
        let title: String
        let isOn: Bool
        let enabled: Bool
        let identifier: String
        let action: () -> Void

        static func look(isOn: Bool, enabled: Bool) -> ToolPageParts.Choices.Look {
            ToolPageParts.Choices.look(chosen: isOn, enabled: enabled)
        }

        var body: some View {
            let look = Self.look(isOn: isOn, enabled: enabled)
            Button(action: action) {
                Text(title)
                    .font(.system(size: 12, weight: .bold))
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
                    .foregroundStyle(look.text)
                    .frame(maxWidth: .infinity, minHeight: 34)
                    .background(look.fill, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(look.border, lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .opacity(look.opacity)
            .accessibilityLabel(title)
            .accessibilityValue(isOn ? "On" : "Off")
            .accessibilityAddTraits(isOn ? .isSelected : [])
            .accessibilityIdentifier(identifier)
        }
    }

    // MARK: The Core's recent log

    private var coreLogCard: some View {
        let enabled = log.reason == nil
        return VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "The Core's recent log", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                ScrollViewReader { reader in
                    ScrollView {
                        VStack(alignment: .leading, spacing: 0) {
                            Text(log.text)
                                .font(.system(size: 11).monospaced())
                                .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.textDim)
                                .textSelection(.enabled)
                                .fixedSize(horizontal: false, vertical: true)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(8)
                            Color.clear.frame(height: 1).id(Self.logEnd)
                        }
                    }
                    .frame(height: 240)
                    .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
                    .accessibilityIdentifier("support.coreLog")
                    .onChange(of: log.lines.last?.id, initial: true) { _, _ in
                        reader.scrollTo(Self.logEnd, anchor: .bottomLeading)
                    }
                }
                .padding(10)
                SpotHubPage.Line()
                HStack(spacing: 5) {
                    smallButton(copied ? CoreLogModel.copiedText : "Copy",
                                enabled: enabled && !log.lines.isEmpty, identifier: "support.coreLogCopy") {
                        UIPasteboard.general.string = log.text
                        copied = true
                    }
                    smallButton("Reload", enabled: enabled, identifier: "support.coreLogReload") {
                        copied = false
                        log.reload()
                    }
                }
                .padding(10)
            }
        }
        .onChange(of: log.lines.last?.id) { _, _ in
            copied = false
        }
    }

    private static let logEnd = "support.coreLog.end"

    private func smallButton(_ title: String, enabled: Bool, identifier: String,
                             action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                .frame(maxWidth: .infinity, minHeight: 34)
                .background(enabled ? ChromeColours.button : ChromeColours.buttonOff,
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityIdentifier(identifier)
    }

    /// A file's size in words: "84 KB".
    static func size(_ file: URL) -> String {
        let bytes = (try? file.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0
        return ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)
    }
}
