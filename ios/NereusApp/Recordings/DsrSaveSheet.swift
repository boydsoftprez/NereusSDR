// NereusSDR for iOS: the Digital Spectrum Recorder's SAVE panel and its "Go to a time" sheet
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 28, the phone's and iPad's Digital Spectrum
//   Recorder. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import NereusModels
import SwiftUI

/// SAVE's panel at the foot of the band (the phone mockup): the bracket's
/// times and length, then Whole pan or Sound only with their sizes. It
/// stays open while the handles move; Cancel takes the bracket away.
struct DsrSavePanel: View {
    let model: DsrPanModel
    /// Slice A's frequency, for the sound's line.
    var sliceFrequencyHz: Double?

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(alignment: .firstTextBaseline) {
                Text(model.saveTitle)
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(WorkspacePalette.textBright)
                    .accessibilityIdentifier("dsr.saveTitle")
                Spacer()
                Button("Cancel") {
                    model.cancelSave()
                }
                .font(.system(size: 16))
                .foregroundStyle(WorkspacePalette.accentText)
                .accessibilityIdentifier("dsr.saveCancel")
            }
            Text(model.saveDetail)
                .font(.system(size: 13))
                .foregroundStyle(WorkspacePalette.textSecondary)
                .fixedSize(horizontal: false, vertical: true)
            HStack(spacing: 8) {
                choice(title: "Whole pan", detail: model.wholePanSize, look: .glass, id: "dsr.saveWholePan") {
                    Task { await model.save(.wholePan) }
                }
                choice(title: model.soundTitle, detail: model.soundSize(sliceFrequencyHz: sliceFrequencyHz),
                       look: .raised, id: "dsr.saveSound") {
                    Task { await model.save(.sound) }
                }
            }
            .padding(.top, 4)
        }
        .padding(.horizontal, 12)
        .padding(.top, 12)
        .padding(.bottom, 16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .workspacePanel()
        .environment(\.colorScheme, .dark)
    }

    private func choice(title: String, detail: String, look: WorkspaceLook, id: String,
                        action: @escaping () -> Void) -> some View {
        Button(action: action) {
            VStack(spacing: 2) {
                Text(title)
                    .font(.system(size: 15, weight: .semibold))
                Text(detail)
                    .font(.system(size: 12))
                    .opacity(0.8)
                    .multilineTextAlignment(.center)
            }
            .padding(.vertical, 10)
            .padding(.horizontal, 6)
            .frame(maxWidth: .infinity, minHeight: 56)
        }
        .buttonStyle(WorkspaceButtonStyle(look))
        .accessibilityIdentifier(id)
    }
}

/// The clock's sheet: type a UTC time or take a chip, then Go. The words
/// under the field say why a time cannot be shown.
struct DsrTimeSheet: View {
    /// The sheet's height, enough for the field, the chips and both buttons.
    static let height: CGFloat = 400

    let model: DsrPanModel
    @State private var typed = ""
    @State private var error = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Go to a time (UTC)")
                .font(.system(size: 16, weight: .semibold))
                .foregroundStyle(WorkspacePalette.textBright)
            Text(model.keptText)
                .font(.system(size: 13))
                .foregroundStyle(WorkspacePalette.textSecondary)
                .fixedSize(horizontal: false, vertical: true)
            TextField("13:05:00", text: $typed)
                .font(.system(size: 28, weight: .semibold, design: .monospaced))
                .multilineTextAlignment(.center)
                .keyboardType(.numbersAndPunctuation)
                .autocorrectionDisabled()
                .padding(.vertical, 8)
                .workspaceInset()
                .accessibilityIdentifier("dsr.timeField")
                .accessibilityLabel("Time, UTC")
                .onChange(of: typed) { error = "" }
            HStack(spacing: 6) {
                ForEach(Self.chips, id: \.title) { chip in
                    Button {
                        typed = DsrPanModel.hms(moment(chip))
                        error = ""
                    } label: {
                        Text(chip.title)
                            .font(.system(size: 13))
                            .lineLimit(1)
                            .padding(.horizontal, 8)
                            .padding(.vertical, 6)
                    }
                    .buttonStyle(WorkspaceButtonStyle(.raised))
                }
            }
            Text(error)
                .font(.system(size: 12.5))
                .foregroundStyle(WorkspacePalette.txRed)
                .frame(minHeight: 16, alignment: .topLeading)
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("dsr.timeError")
            Button {
                error = model.go(toTyped: typed) ?? ""
            } label: {
                Text("Go").font(.system(size: 16, weight: .semibold)).frame(maxWidth: .infinity, minHeight: 44)
            }
            .buttonStyle(WorkspaceButtonStyle(.glass))
            .accessibilityIdentifier("dsr.timeGo")
            Button {
                model.timeSheetOpen = false
            } label: {
                Text("Cancel").font(.system(size: 16, weight: .semibold)).frame(maxWidth: .infinity, minHeight: 44)
            }
            .buttonStyle(WorkspaceButtonStyle(.raised))
        }
        .padding(16)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .background(WorkspacePalette.panelBg)
        .environment(\.colorScheme, .dark)
        .onAppear {
            typed = model.typedStart
            error = ""
        }
    }

    struct Chip {
        let title: String
        /// Seconds before now; nil for the oldest kept.
        let ago: Double?
    }

    static let chips = [Chip(title: "5 min ago", ago: 300), Chip(title: "15 min ago", ago: 900),
                        Chip(title: "1 hour ago", ago: 3600), Chip(title: "Oldest kept", ago: nil)]

    private func moment(_ chip: Chip) -> Double {
        if let ago = chip.ago {
            return model.clock() - ago
        }
        // The oldest kept, a whole picture in so the picture fills.
        return (model.oldestKept ?? model.clock()) + model.pictureSeconds
    }
}
