// NereusSDR for iOS: Setup's Band plan row and its picker: the Core's plans, a tick on the one it shows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import NereusModels
import SwiftUI

/// The Band plan row (D79, spec section 5.1 item 19): the name of the plan
/// the band shows, on a button that opens the picker under it. The picker
/// lists the Core's plans in its catalogue's order with a tick on the one
/// showing; picking one changes the Core's plan, so the desktop and every
/// device follow. The tick moves when the Core says so, and a refusal
/// shows the Core's words here. Without a catalogue the row is greyed
/// with "Needs a newer Core".
struct BandPlanPicker: View {
    @ObservedObject var model: DisplaySheetModel

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            row
            if model.planPickerOpen && !model.plans.isEmpty {
                list
            }
            if let note = model.planNote {
                SheetNote(note)
                    .accessibilityIdentifier("bandPlanNote")
            }
        }
    }

    private var unavailable: Bool { model.plans.isEmpty }

    private var shownName: String {
        model.shownPlan?.name ?? CatalogFeed.needsNewerCoreText
    }

    private var row: some View {
        HStack(spacing: 10) {
            Text("Band plan")
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(ChromeColours.text)
            Spacer(minLength: 0)
            Button {
                model.planPickerOpen.toggle()
            } label: {
                Text("\(shownName) \(model.planPickerOpen ? "\u{25B4}" : "\u{25BE}")")
                    .font(.system(size: 12, weight: .bold))
                    .lineLimit(1)
                    .foregroundStyle(unavailable ? ChromeColours.buttonOffText : ChromeColours.text)
                    .padding(.horizontal, 12)
                    .frame(minWidth: 88, minHeight: 36)
                    .background(unavailable ? ChromeColours.buttonOff : ChromeColours.button,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(unavailable ? ChromeColours.buttonOffBorder : ChromeColours.buttonBorder,
                                      lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(unavailable)
            .accessibilityLabel("Band plan")
            .accessibilityValue(shownName)
            .accessibilityHint(unavailable ? CatalogFeed.needsNewerCoreText : "")
            .accessibilityIdentifier("bandPlanButton")
        }
        .padding(.vertical, 2)
    }

    private var list: some View {
        VStack(spacing: 2) {
            ForEach(model.plans, id: \.id) { plan in
                let shown = plan.id == model.shownPlan?.id
                Button {
                    model.pickPlan(plan)
                } label: {
                    HStack {
                        Text(plan.name)
                            .font(.system(size: 13, weight: shown ? .bold : .regular))
                            .lineLimit(1)
                        Spacer(minLength: 8)
                        Image(systemName: "checkmark")
                            .font(.system(size: 12, weight: .bold))
                            .opacity(shown ? 1 : 0)
                            .accessibilityHidden(true)
                    }
                    .foregroundStyle(ChromeColours.text)
                    .padding(.horizontal, 12)
                    .frame(height: 36)
                    .background(shown ? ChromeColours.button : .clear, in: RoundedRectangle(cornerRadius: 4))
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel(plan.name)
                .accessibilityAddTraits(shown ? .isSelected : [])
                .accessibilityIdentifier("bandPlan.\(plan.id)")
            }
        }
        .padding(4)
        .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("bandPlanList")
    }
}
