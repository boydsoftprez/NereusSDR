// NereusSDR for iOS: the "Settings for" strip on Setup's Display pages: All pans or one pan, its note, and its two actions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The strip's words, as functions of the pan's state, so the tests read
/// the same text the strip draws. Each is the desktop's
/// (`src/gui/SetupPage.cpp:376-466`), with the phone's two additions for
/// what the desktop hides or does not have (spec section 7.3, section 12
/// item 4).
enum DisplayStripText {
    // From src/gui/SetupPage.cpp:376 ("Settings for:")
    static let title = "Settings for"
    // From src/gui/SetupPage.cpp:437 (the All pans chip)
    static let allPans = "All pans"
    // From src/gui/SetupPage.cpp:451
    static let allPansNote = "A change here goes to every pan."
    // From src/gui/SetupPage.cpp:384
    static let reset = "Reset this pan"
    /// Under the two actions on All pans, where the desktop hides them.
    static let pickAPan = "Pick a pan to reset it."
    /// Beside a pan an iPhone cannot show.
    static let iPhoneOnePan = "An iPhone shows one pan."

    /// A pan's name on its button, with " •" when it has its own values on this page.
    // From src/gui/SetupPage.cpp:437-438 ("Pan %1%2")
    static func panTitle(_ pan: PanIdentity, ownCount: Int) -> String {
        pan.title + (ownCount > 0 ? " \u{2022}" : "")
    }

    /// A pan button's second line: its slices, "open" with none, or "not open".
    // From src/gui/SetupPage.cpp:435-436
    static func subtitle(open: Bool, letters: [String]) -> String {
        guard open else { return "not open" }
        return letters.isEmpty ? "open" : "Slice " + letters.joined(separator: ", ")
    }

    // From src/gui/SetupPage.cpp:447
    static func followsNote(_ pan: PanIdentity) -> String {
        "\(pan.title) follows All pans in every row on this page."
    }

    // From src/gui/SetupPage.cpp:454-455
    static func notOpenNote(_ pan: PanIdentity) -> String {
        "\(pan.title) is not open now. What you set here is kept and used when you open it."
    }

    // From src/gui/SetupPage.cpp:457-458
    static func ownNote(_ pan: PanIdentity, ownCount: Int) -> String {
        "\(pan.title) has its own value in \(ownCount) row\(ownCount == 1 ? "" : "s") on this page, marked with a dot."
    }

    /// The note under the strip, put together as the desktop's is: on a
    /// pan not open, that first, then its own values or that it follows.
    // From src/gui/SetupPage.cpp:449-460
    static func note(scope: MainScreenModel.DisplayEditScope, open: Bool, ownCount: Int) -> String {
        guard case .pan(let pan) = scope else { return allPansNote }
        let values = ownCount > 0 ? ownNote(pan, ownCount: ownCount) : followsNote(pan)
        return open ? values : notOpenNote(pan) + " " + values
    }

    /// The copy action's words for the pan chosen; on All pans, Pan 1's.
    // From src/gui/SetupPage.cpp:466
    static func copy(_ pan: PanIdentity) -> String {
        "Use \(pan.title)'s on all pans"
    }
}

/// The "Settings for" strip (DC12; spec section 7.3) at the top of
/// Spectrum, Levels, Waterfall, View and Peaks & Markers: All pans, then
/// each pan up to the radio's limit with its slices or "not open" and a dot
/// when it keeps its own values on this page; the note the desktop shows;
/// and, for a pan, Reset this pan and Use Pan N's on all pans, acting on
/// this page's rows only. On All pans the two actions stay in place, greyed
/// with their reason, where the desktop hides them; on an iPhone the pans
/// after Pan 1 are greyed, since an iPhone shows one pan.
struct DisplaySettingsStrip: View {
    let pageId: String
    @Binding var scope: MainScreenModel.DisplayEditScope
    let panLimit: Int
    let isPhone: Bool
    /// The pans open on this device, each with its slices' letters.
    let slicesByPan: [PanIdentity: [String]]
    let ownFieldCount: (PanIdentity) -> Int
    let reset: (PanIdentity) -> Void
    let useOnAll: (PanIdentity) -> Void
    @Environment(\.dynamicTypeSize) private var typeSize

    init(pageId: String, scope: Binding<MainScreenModel.DisplayEditScope>, panLimit: Int, isPhone: Bool,
         slicesByPan: [PanIdentity: [String]], ownFieldCount: @escaping (PanIdentity) -> Int,
         reset: @escaping (PanIdentity) -> Void, useOnAll: @escaping (PanIdentity) -> Void) {
        self.pageId = pageId
        _scope = scope
        self.panLimit = panLimit
        self.isPhone = isPhone
        self.slicesByPan = slicesByPan
        self.ownFieldCount = ownFieldCount
        self.reset = reset
        self.useOnAll = useOnAll
    }

    /// The pans offered: Pan 1 to the radio's limit, and a pan chosen beyond it.
    private var pans: [PanIdentity] {
        var pans = (1...max(1, panLimit)).map(PanIdentity.init(number:))
        if case .pan(let chosen) = scope, !pans.contains(chosen) {
            pans.append(chosen)
        }
        return pans
    }

    /// A pan this device cannot show: on an iPhone, every pan after Pan 1.
    static func greyed(_ pan: PanIdentity, isPhone: Bool) -> Bool {
        isPhone && pan != .first
    }

    private var chosenPan: PanIdentity? {
        if case .pan(let pan) = scope { return pan }
        return nil
    }

    var body: some View {
        SetupPanelSection {
            LazyVGrid(columns: [GridItem(.adaptive(minimum: typeSize.isAccessibilitySize ? 220 : 96), spacing: 6)],
                      alignment: .leading, spacing: 6) {
                chip(title: DisplayStripText.allPans, subtitle: nil, chosen: scope == .all, greyed: false,
                     id: "displayStrip.all") { scope = .all }
                ForEach(pans, id: \.self) { pan in
                    let own = ownFieldCount(pan)
                    chip(title: DisplayStripText.panTitle(pan, ownCount: own),
                         subtitle: DisplayStripText.subtitle(open: slicesByPan[pan] != nil, letters: slicesByPan[pan] ?? []),
                         chosen: scope == .pan(pan), greyed: Self.greyed(pan, isPhone: isPhone),
                         id: "displayStrip.pan\(pan.number)") { scope = .pan(pan) }
                }
            }
            .accessibilityElement(children: .contain)
            .accessibilityLabel(DisplayStripText.title)
            .accessibilityIdentifier("displayScope")
            if isPhone, panLimit > 1 {
                Text(DisplayStripText.iPhoneOnePan)
                    .font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
                    .accessibilityIdentifier("displayStripOnePan")
            }
            Text(note)
                .font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("displayStripNote")
            actions
        } header: {
            SetupPanelTitle(title: DisplayStripText.title, tag: .thisPhone)
        }
        .accessibilityIdentifier("displayStrip")
    }

    private var note: String {
        let open = chosenPan.map { slicesByPan[$0] != nil } ?? true
        return DisplayStripText.note(scope: scope, open: open, ownCount: chosenPan.map(ownFieldCount) ?? 0)
    }

    @ViewBuilder private var actions: some View {
        let pan = chosenPan
        let acts = pan.map { ownFieldCount($0) > 0 } ?? false
        let wrap = typeSize.isAccessibilitySize
        let layout = wrap ? AnyLayout(VStackLayout(alignment: .leading, spacing: 6))
            : AnyLayout(HStackLayout(spacing: 6))
        layout {
            action(DisplayStripText.reset, enabled: acts, id: "displayStripReset") { if let pan { reset(pan) } }
            action(DisplayStripText.copy(pan ?? .first), enabled: acts, id: "displayStripCopy") {
                if let pan { useOnAll(pan) }
            }
        }
        if pan == nil {
            Text(DisplayStripText.pickAPan)
                .font(.footnote).foregroundStyle(WorkspacePalette.textSecondary)
                .accessibilityIdentifier("displayStripPickAPan")
        }
    }

    /// Reset this pan and Use Pan N's on all pans: raised, at the chip radius.
    private func action(_ title: String, enabled: Bool, id: String, act: @escaping () -> Void) -> some View {
        Button(action: act) {
            Text(title).font(.subheadline)
                .padding(.horizontal, 12)
                .frame(minHeight: SetupRowMetrics.minHeight)
        }
        .buttonStyle(WorkspaceButtonStyle(.raised))
        .disabled(!enabled)
        .accessibilityIdentifier(id)
    }

    private func chip(title: String, subtitle: String?, chosen: Bool, greyed: Bool, id: String,
                      choose: @escaping () -> Void) -> some View {
        Button(action: choose) {
            VStack(spacing: 1) {
                Text(title).font(.subheadline.weight(.semibold))
                if let subtitle {
                    Text(subtitle).font(.caption2)
                }
            }
            .multilineTextAlignment(.center)
            .frame(maxWidth: .infinity, minHeight: SetupRowMetrics.minHeight)
            .padding(.horizontal, 6)
        }
        .buttonStyle(WorkspaceButtonStyle(chosen ? .glass : .raised))
        .disabled(greyed)
        .accessibilityAddTraits(chosen ? .isSelected : [])
        .accessibilityIdentifier(id)
    }
}
