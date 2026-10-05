// NereusSDR for iOS: one Setup page the Core describes, drawn from its description with each control live
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// A page the Core describes (R-IOS-18): its sections and controls in the
/// desktop's order, with the Core's labels, ranges, units and help. Each
/// control writes to its owner through ``SetupControlDispatcher``; one
/// that cannot change now stays visible, greyed, with its reason.
struct DescribedPage: View {
    @ObservedObject var pages: SetupDescribedPages
    @ObservedObject var dispatcher: SetupControlDispatcher
    let category: String
    let pageId: String
    var specialized: SetupSpecializedPanels = .none
    /// Calibration's Level Cal group, on that page only (nil elsewhere).
    var levelCal: LevelCalModel?
    @State var profileOwner = UUID()
    @State private var paPresentation: UUID?

    /// The words for a page the Core no longer describes.
    static let goneText = "This page is no longer on the Core."

    private func openProfiles(_ page: SetupDescription.Page) {
        if let control = page.sections.flatMap(\.controls).first(where: { $0.modern?.profileUnsavedChanges != nil }) {
            dispatcher.txProfiles.open(control, in: category)
        }
    }

    var body: some View {
        if let page = pages.page(pageId, in: category) {
            List {
                // A Core below Setup description 23 has no Level Cal
                // section: the group comes first, as on the desktop.
                if let levelCal, !page.sections.contains(where: { $0.title == LevelCalSection.title }) {
                    Section {
                        LevelCalSection.Top(model: levelCal)
                        LevelCalSection.Run(model: levelCal)
                    } header: {
                        Text(LevelCalSection.title)
                    }
                }
                ForEach(Array(page.sections.enumerated()), id: \.offset) { _, section in
                    let group = levelCal.flatMap { section.title == LevelCalSection.title ? $0 : nil }
                    Section {
                        if let group {
                            LevelCalSection.Top(model: group)
                        }
                        ForEach(section.controls, id: \.id) { control in
                            DescribedControl(control: control, category: category, dispatcher: dispatcher,
                                             specialized: specialized, profileOwner: profileOwner)
                        }
                        if let group {
                            LevelCalSection.Run(model: group)
                        }
                    } header: {
                        Text(section.title)
                    }
                }
            }
            .onAppear { openProfiles(page) }
            .onChange(of: page) { _, updated in openProfiles(updated) }
            .onDisappear {
                if page.sections.flatMap(\.controls).contains(where: { $0.modern?.profileUnsavedChanges != nil }) {
                    dispatcher.txProfiles.retire(owner: profileOwner)
                }
            }
            .levelCalAlerts(levelCal)
            .navigationTitle(page.title)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    SetupTagBadge(tag: SetupTag(page.whereOwned))
                }
            }
            .accessibilityIdentifier(page.id)
            .onAppear {
                if category == "pa", pageId == "pa.values", let model = specialized.paValues {
                    paPresentation = model.beginPresentation()
                }
            }
            .onDisappear {
                if let token = paPresentation { specialized.paValues?.endPresentation(token) }
                paPresentation = nil
            }
        } else {
            List {
                Text(Self.goneText)
                    .foregroundStyle(.secondary)
            }
        }
    }
}
