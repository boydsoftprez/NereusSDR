// NereusSDR for iOS: Setup's page container and its workspace panels: the ground, each section a panel, and the row pieces they share
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// One Setup page (W6, spec section 7.1): a scroll view on the workspace
/// ground, each section a ``SetupPanelSection``, the page's own navigation
/// title and mark kept where the page sets them. Every Setup page draws in
/// one, so the iPad's split can show any page in its right-hand side as it is.
///
/// The container is a scroll view of panels, not a restyled `List` (plan
/// call 3, the container trial): only a scroll view draws each section as one
/// panel with the desktop's 10 point corner and its `kBorder` edge. VoiceOver
/// reads the rows in page order because they are laid out in that order,
/// Dynamic Type grows the rows without clipping, and a list's swipe to delete
/// is kept where a page has one by ``SetupSwipeToDelete``.
struct SetupPageContainer<Content: View>: View {
    private let content: Content
    /// The iPad's split holds a page's content to a readable width; nil fills the width.
    @Environment(\.setupReadableWidth) private var readableWidth

    /// The space between two sections, as the board draws it.
    static var sectionSpacing: CGFloat { 20 }

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Self.sectionSpacing) {
                // In the iPad's split, the page's title heads its own column.
                SetupInlineTitle()
                content
                    // A page pushed from this one draws its own title.
                    .environment(\.setupPageTitle, nil)
            }
            .padding(.horizontal, 16)
            .padding(.top, 8)
            .padding(.bottom, 24)
            .frame(maxWidth: readableWidth ?? .infinity, alignment: .leading)
            .frame(maxWidth: .infinity)
        }
        .scrollContentBackground(.hidden)
        .background(WorkspacePalette.appBg.ignoresSafeArea())
        .foregroundStyle(WorkspacePalette.textPrimary)
        .labeledContentStyle(SetupLabeledContentStyle())
    }
}

/// One section of a Setup page: its title in `kTitleText` with its mark,
/// its rows in one workspace panel with a line between them, and its note
/// under the panel (spec section 7.1). Shaped like `Section`, so a page's
/// sections read as they did in the list.
struct SetupPanelSection<Rows: View, Header: View, Footer: View>: View {
    private let rows: Rows
    private let header: Header
    private let footer: Footer

    init(@ViewBuilder content: () -> Rows, @ViewBuilder header: () -> Header,
         @ViewBuilder footer: () -> Footer) {
        rows = content()
        self.header = header()
        self.footer = footer()
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            header
                .font(.footnote.weight(.semibold))
                .foregroundStyle(WorkspacePalette.titleText)
                .padding(.horizontal, 4)
                .accessibilityAddTraits(.isHeader)
            VStack(alignment: .leading, spacing: 0) {
                Group {
                    rows
                }
                .modifier(SetupPanelRow())
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .workspacePanel()
            // The edge drawn over the rows, so the first row's line lies under it.
            .overlay {
                RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius, style: .continuous)
                    .strokeBorder(WorkspacePalette.border, lineWidth: 1)
            }
            footer
                .font(.footnote)
                .foregroundStyle(WorkspacePalette.textTertiary)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 6)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .contain)
    }
}

extension SetupPanelSection where Header == EmptyView, Footer == EmptyView {
    init(@ViewBuilder content: () -> Rows) {
        self.init(content: content, header: { EmptyView() }, footer: { EmptyView() })
    }
}

extension SetupPanelSection where Footer == EmptyView {
    init(@ViewBuilder content: () -> Rows, @ViewBuilder header: () -> Header) {
        self.init(content: content, header: header, footer: { EmptyView() })
    }
}

extension SetupPanelSection where Header == EmptyView {
    init(@ViewBuilder content: () -> Rows, @ViewBuilder footer: () -> Footer) {
        self.init(content: content, header: { EmptyView() }, footer: footer)
    }
}

extension SetupPanelSection where Header == SetupPanelTitle, Footer == SetupPanelNote {
    /// A section by its title, its optional mark and its optional note.
    init(_ title: String, tag: SetupTag? = nil, note: String? = nil, @ViewBuilder content: () -> Rows) {
        self.init(content: content, header: { SetupPanelTitle(title: title, tag: tag) },
                  footer: { SetupPanelNote(text: note) })
    }
}

/// A section's title, with its mark when it has one.
struct SetupPanelTitle: View {
    let title: String
    var tag: SetupTag?

    var body: some View {
        if !title.isEmpty || tag != nil {
            HStack(spacing: 6) {
                if !title.isEmpty {
                    Text(title)
                }
                if let tag {
                    SetupTagBadge(tag: tag)
                }
            }
        }
    }
}

/// A section's note under its panel.
struct SetupPanelNote: View {
    let text: String?

    var body: some View {
        if let text {
            Text(text)
        }
    }
}

// MARK: The rows

/// Each row of a panel: its padding, at least a list row's height, and the
/// `kBorderSubtle` line above it; a picked choice's row is drawn in the glass.
private struct SetupPanelRow: ViewModifier {
    func body(content: Content) -> some View {
        content
            .frame(maxWidth: .infinity, minHeight: SetupRowMetrics.minHeight - 2 * SetupRowMetrics.vertical,
                   alignment: .leading)
            .padding(.horizontal, SetupRowMetrics.horizontal)
            .padding(.vertical, SetupRowMetrics.vertical)
            .backgroundPreferenceValue(SetupRowPicked.self) { picked in
                if picked {
                    WorkspaceFace(.glass, radius: SetupRowMetrics.pickedRadius)
                        .padding(1)
                }
            }
            .overlayPreferenceValue(SetupRowPicked.self, alignment: .top) { picked in
                Rectangle()
                    .fill(WorkspacePalette.borderSubtle)
                    .frame(height: 1)
                    .padding(.horizontal, SetupRowMetrics.lineInset)
                    .opacity(picked ? 0 : 1)
                    .accessibilityHidden(true)
            }
    }
}

/// The board's row measures.
enum SetupRowMetrics {
    static let horizontal: CGFloat = 14
    static let vertical: CGFloat = 10
    /// A list row's height, so a short row is still a finger's height.
    static let minHeight: CGFloat = 44
    static let lineInset: CGFloat = 12
    /// The picked row's glass, one point inside the panel's 10 point corner.
    static let pickedRadius: CGFloat = 9
}

/// Set by a choice row that is picked: its panel row draws the glass.
struct SetupRowPicked: PreferenceKey {
    static let defaultValue = false
    static func reduce(value: inout Bool, nextValue: () -> Bool) {
        value = value || nextValue()
    }
}

extension View {
    /// Marks a choice row picked: its row is the glass and its words are
    /// `kGlassSelText`; the check mark stays, so the choice reads without color.
    func setupPicked(_ picked: Bool) -> some View {
        foregroundStyle(picked ? WorkspacePalette.glassSelText : WorkspacePalette.textPrimary)
            .preference(key: SetupRowPicked.self, value: picked)
    }

    /// A control that cannot act now: greyed in place at 0.4, its reason under it.
    func setupGreyed(_ greyed: Bool) -> some View {
        opacity(greyed ? WorkspacePalette.disabledOpacity : 1)
    }
}

/// A choice's check mark, shown on the picked row only.
struct SetupCheckMark: View {
    let chosen: Bool

    var body: some View {
        Image(systemName: "checkmark")
            .foregroundStyle(.primary)
            .opacity(chosen ? 1 : 0)
    }
}

/// `LabeledContent` in a panel row: the label, then its value at the trailing edge.
struct SetupLabeledContentStyle: LabeledContentStyle {
    func makeBody(configuration: Configuration) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            configuration.label
            Spacer(minLength: 8)
            configuration.content
        }
    }
}

// MARK: Value rows and large type

/// A row whose value sits beside its label, or under it at the accessibility
/// text sizes (W9, spec section 7.2): a number row and a menu row.
struct SetupValueRowLayout<Label: View, Value: View>: View {
    private let probe: String?
    private let label: Label
    private let value: Value
    @Environment(\.dynamicTypeSize) private var dynamicTypeSize
    @Environment(\.setupLayoutProbe) private var layoutProbe

    init(probe: String? = nil, @ViewBuilder label: () -> Label, @ViewBuilder value: () -> Value) {
        self.probe = probe
        self.label = label()
        self.value = value()
    }

    /// The value goes under the label at this text size.
    static func stacks(_ size: DynamicTypeSize) -> Bool {
        size.isAccessibilitySize
    }

    var body: some View {
        if Self.stacks(dynamicTypeSize) {
            VStack(alignment: .leading, spacing: 8) {
                probed(label, part: .label)
                    .frame(maxWidth: .infinity, alignment: .leading)
                probed(value, part: .value)
            }
        } else {
            HStack(spacing: 10) {
                probed(label, part: .label)
                Spacer(minLength: 8)
                probed(value, part: .value)
            }
        }
    }

    @ViewBuilder
    private func probed(_ view: some View, part: SetupLayoutProbe.Part) -> some View {
        if let layoutProbe, let probe {
            view.onGeometryChange(for: CGRect.self) { $0.frame(in: .global) } action: { frame in
                layoutProbe.frames[SetupLayoutProbe.Key(id: probe, part: part)] = frame
            }
        } else {
            view
        }
    }
}

/// Where a value row's label and value were laid out, for the tests'
/// large-type checks; nil (the default) records nothing.
@MainActor
final class SetupLayoutProbe {
    enum Part: Hashable {
        case label, value
    }

    struct Key: Hashable {
        let id: String
        let part: Part
    }

    var frames: [Key: CGRect] = [:]

    func frame(_ id: String, _ part: Part) -> CGRect? {
        frames[Key(id: id, part: part)]
    }
}

private struct SetupLayoutProbeKey: EnvironmentKey {
    static let defaultValue: SetupLayoutProbe? = nil
}

/// A Setup page's title. On an iPhone and a compact iPad it is the page's
/// large navigation title, as before. At the root of the iPad split's
/// right-hand side (``SetupSplitView``) the navigation bar shows no title
/// and the page draws it inline (``SetupInlineTitle``), in line with its
/// panels inside the readable width, as setup-board.html's chosen iPad
/// frame draws it. The bar keeps the title for the system's own use.
struct SetupPageTitle: ViewModifier {
    let title: String
    @Environment(\.setupInlineTitles) private var inline

    func body(content: Content) -> some View {
        if inline {
            content
                .environment(\.setupPageTitle, title)
                .navigationTitle(title)
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    // Stands in the bar's middle so the bar draws no title.
                    ToolbarItem(placement: .principal) {
                        Color.clear
                            .frame(width: 1, height: 1)
                            .accessibilityHidden(true)
                    }
                }
        } else {
            content
                .navigationTitle(title)
        }
    }
}

extension View {
    /// The page's title: the navigation title, or inline at the root of the iPad's split.
    func setupPageTitle(_ title: String) -> some View {
        modifier(SetupPageTitle(title: title))
    }
}

/// A page's title drawn inline at the top of its content, in the large
/// title's font, read by VoiceOver as a header; nothing outside the split.
struct SetupInlineTitle: View {
    @Environment(\.setupPageTitle) private var title

    var body: some View {
        if let title {
            Text(title)
                .font(.largeTitle.bold())
                .foregroundStyle(WorkspacePalette.textPrimary)
                .fixedSize(horizontal: false, vertical: true)
                // The section titles' inset, so the title starts where they do.
                .padding(.horizontal, 4)
                .accessibilityAddTraits(.isHeader)
                .accessibilityIdentifier("setupPageTitle")
        }
    }
}

/// Holds a page that draws its own scroll view (Devices) to the split's
/// readable width, centered; with no readable width set it changes nothing.
struct SetupReadableWidth: ViewModifier {
    @Environment(\.setupReadableWidth) private var readableWidth

    func body(content: Content) -> some View {
        if let readableWidth {
            content
                .frame(maxWidth: readableWidth)
                .frame(maxWidth: .infinity)
        } else {
            content
        }
    }
}

private struct SetupReadableWidthKey: EnvironmentKey {
    static let defaultValue: CGFloat? = nil
}

private struct SetupInlineTitlesKey: EnvironmentKey {
    static let defaultValue = false
}

private struct SetupPageTitleKey: EnvironmentKey {
    static let defaultValue: String? = nil
}

extension EnvironmentValues {
    /// The widest a Setup page's content is drawn: the iPad's split sets it
    /// (``SetupSplit/readableWidth``); nil, the default, fills the width.
    var setupReadableWidth: CGFloat? {
        get { self[SetupReadableWidthKey.self] }
        set { self[SetupReadableWidthKey.self] = newValue }
    }

    /// True at the root of the iPad split's right-hand side, where a page
    /// draws its title inline (``SetupPageTitle``).
    var setupInlineTitles: Bool {
        get { self[SetupInlineTitlesKey.self] }
        set { self[SetupInlineTitlesKey.self] = newValue }
    }

    /// The title a page draws inline, set by ``SetupPageTitle`` in the split.
    var setupPageTitle: String? {
        get { self[SetupPageTitleKey.self] }
        set { self[SetupPageTitleKey.self] = newValue }
    }

    var setupLayoutProbe: SetupLayoutProbe? {
        get { self[SetupLayoutProbeKey.self] }
        set { self[SetupLayoutProbeKey.self] = newValue }
    }
}

/// A menu row (spec section 7.1): its label, and a raised button showing the
/// value with the up-down chevrons; the menu is the picker it wraps. Under
/// the label at the accessibility text sizes.
struct SetupMenuRow<Picker: View>: View {
    let title: String
    let picker: Picker
    @Environment(\.isEnabled) private var isEnabled

    var body: some View {
        SetupValueRowLayout(probe: title) {
            Text(title)
        } value: {
            picker
                .pickerStyle(.menu)
                .labelsHidden()
                .tint(WorkspacePalette.textPrimary)
                .fixedSize()
                .padding(.horizontal, 4)
                .frame(minHeight: SetupStepButton.height)
                .background { WorkspaceFace(.raised) }
        }
        .setupGreyed(!isEnabled)
    }
}

extension View {
    /// This menu picker as a Setup menu row, labelled `title`. A row that
    /// cannot change takes `.disabled` after this, so the whole row greys.
    func setupMenuRow(_ title: String) -> some View {
        SetupMenuRow(title: title, picker: self)
    }
}

// MARK: Buttons

/// A Setup button in one of the desktop's treatments, padded as the board's
/// buttons are: raised at rest, glass for a primary action, the muted danger
/// set for Revoke; 0.4 when it cannot act (``WorkspaceButtonStyle``'s faces).
struct SetupButtonStyle: ButtonStyle {
    private let look: WorkspaceLook
    private let fill: Bool
    @Environment(\.isEnabled) private var isEnabled

    init(_ look: WorkspaceLook = .raised, fill: Bool = false) {
        self.look = look
        self.fill = fill
    }

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .padding(.horizontal, 14)
            .frame(maxWidth: fill ? .infinity : nil, minHeight: SetupStepButton.height)
            .foregroundStyle(workspaceForeground(look))
            .background {
                WorkspaceFace(look, pressed: configuration.isPressed)
            }
            .contentShape(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius, style: .continuous))
            .opacity(isEnabled ? 1 : WorkspacePalette.disabledOpacity)
    }
}

/// A row that opens a page: the row itself, dimmed a little while pressed.
struct SetupLinkStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .contentShape(Rectangle())
            .opacity(configuration.isPressed ? 0.6 : 1)
    }
}

/// A row that opens a page, as a panel row draws it: its words, then its chevron.
struct SetupLinkLabel<Title: View>: View {
    private let title: Title

    init(@ViewBuilder title: () -> Title) {
        self.title = title()
    }

    var body: some View {
        HStack(spacing: 10) {
            title
                .frame(maxWidth: .infinity, alignment: .leading)
            SetupChevron()
        }
        .foregroundStyle(WorkspacePalette.textPrimary)
    }
}

extension SetupLinkLabel where Title == Text {
    init(_ title: String) {
        self.init { Text(title) }
    }
}

extension View {
    /// A plain page under Setup (a licence's text): on the workspace ground.
    func setupPlainPage() -> some View {
        background(WorkspacePalette.appBg.ignoresSafeArea())
            .foregroundStyle(WorkspacePalette.textPrimary)
    }
}

/// A row's chevron: it opens a page.
struct SetupChevron: View {
    var body: some View {
        Image(systemName: "chevron.right")
            .font(.footnote.weight(.semibold))
            .foregroundStyle(WorkspacePalette.textScale)
            .accessibilityHidden(true)
    }
}

/// A number row's raised minus or plus, the desktop's spin box buttons.
/// Held down it steps again and again, as the stock stepper does.
struct SetupStepButton: View {
    static let height: CGFloat = 30
    /// The hold before the first repeat, and the time between repeats.
    static let repeatDelay: Duration = .milliseconds(500)
    static let repeatInterval: Duration = .milliseconds(100)

    let systemImage: String
    let action: () -> Void
    @ScaledMetric(relativeTo: .body) private var width: CGFloat = 40
    @ScaledMetric(relativeTo: .body) private var height: CGFloat = SetupStepButton.height
    @State private var repeater: Task<Void, Never>?
    @State private var repeated = false

    var body: some View {
        Button {
            // A hold that already stepped does not step once more on release.
            if repeated {
                repeated = false
            } else {
                action()
            }
        } label: {
            Image(systemName: systemImage)
                .font(.body.weight(.medium))
                .frame(width: width, height: height)
        }
        .buttonStyle(Pressing(look: .raised) { pressed in
            repeater?.cancel()
            repeater = nil
            guard pressed else {
                return
            }
            repeated = false
            repeater = Task { @MainActor in
                try? await Task.sleep(for: Self.repeatDelay)
                while !Task.isCancelled {
                    repeated = true
                    action()
                    try? await Task.sleep(for: Self.repeatInterval)
                }
            }
        })
        .onDisappear {
            repeater?.cancel()
            repeater = nil
        }
    }

    /// The raised face, telling the button when a press starts and ends. Its
    /// number row greys as a whole, so the face itself does not dim again.
    private struct Pressing: ButtonStyle {
        let look: WorkspaceLook
        let changed: (Bool) -> Void

        func makeBody(configuration: Configuration) -> some View {
            configuration.label
                .foregroundStyle(workspaceForeground(look))
                .background { WorkspaceFace(look, pressed: configuration.isPressed) }
                .contentShape(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius, style: .continuous))
                .onChange(of: configuration.isPressed) { _, pressed in
                    changed(pressed)
                }
        }
    }
}

/// A value stepped with raised minus and plus that has no pad (a line's
/// width, a choice stepped along its list): its words in the inset field.
/// Drawn as ``SetupNumberRow`` draws, under its title at large type (W9);
/// VoiceOver steps it as the stock stepper it stands for. A row that
/// cannot change takes `.disabled`, and greys as a whole at 0.4.
struct SetupStepperRow<Label: View>: View {
    let valueText: String
    let id: String?
    let increment: () -> Void
    let decrement: () -> Void
    private let label: Label
    @Environment(\.isEnabled) private var isEnabled
    @Environment(\.dynamicTypeSize) private var dynamicTypeSize

    init(valueText: String, id: String? = nil, increment: @escaping () -> Void,
         decrement: @escaping () -> Void, @ViewBuilder label: () -> Label) {
        self.valueText = valueText
        self.id = id
        self.increment = increment
        self.decrement = decrement
        self.label = label()
    }

    var body: some View {
        let stacked = SetupValueRowLayout<EmptyView, EmptyView>.stacks(dynamicTypeSize)
        SetupValueRowLayout(probe: id) {
            label
        } value: {
            HStack(spacing: 6) {
                Text(valueText)
                    .monospacedDigit()
                    .foregroundStyle(WorkspacePalette.textSecondary)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 6)
                    .frame(minWidth: SetupNumberRow.fieldMinWidth, maxWidth: stacked ? .infinity : nil,
                           alignment: stacked ? .leading : .trailing)
                    .workspaceInset()
                SetupStepButton(systemImage: "minus", action: decrement)
                SetupStepButton(systemImage: "plus", action: increment)
            }
        }
        .setupGreyed(!isEnabled)
        .accessibilityRepresentation {
            Stepper(onIncrement: increment, onDecrement: decrement) {
                label
            }
            .accessibilityValue(valueText)
        }
    }
}

extension SetupStepperRow where Label == Text {
    init(_ title: String, valueText: String, id: String? = nil, increment: @escaping () -> Void,
         decrement: @escaping () -> Void) {
        self.init(valueText: valueText, id: id, increment: increment, decrement: decrement) { Text(title) }
    }
}

// MARK: Sliders

/// A Setup slider: a UISlider in the workspace look (spec section 7.1,
/// "Sliders": the track in the inset, the filled part in kGlassSelBorder, a
/// light raised thumb). It is UIKit's slider underneath so it behaves as the
/// stock one does, which a drawn slider could not promise: the value moves
/// only while the thumb is dragged, a tap on the track changes nothing and
/// sends nothing, a vertical drag scrolls the page, and VoiceOver adjusts it.
/// Its greying is the row's (`setupGreyed`), so it is never greyed twice.
struct SetupSlider: UIViewRepresentable {
    @Binding var value: Double
    let range: ClosedRange<Double>
    let step: Double
    /// What VoiceOver names the slider, set on the UISlider itself (an
    /// accessibility modifier on a representable may not reach it).
    var label: String?
    /// The slider's accessibility identifier, set on the UISlider itself.
    var identifier: String?
    /// What VoiceOver reads as the value; nil keeps UISlider's percentage.
    var valueText: String?
    /// Called with true when a drag starts and false when it ends, as `Slider` does.
    var onEditingChanged: (Bool) -> Void = { _ in }

    func makeUIView(context: Context) -> SetupUISlider {
        let slider = SetupUISlider(frame: .zero)
        slider.setContentHuggingPriority(.defaultLow, for: .horizontal)
        slider.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        return slider
    }

    func updateUIView(_ slider: SetupUISlider, context: Context) {
        let binding = $value
        slider.range = range
        slider.step = step
        slider.onMove = { binding.wrappedValue = $0 }
        slider.onEditingChanged = onEditingChanged
        slider.isEnabled = context.environment.isEnabled
        slider.accessibilityLabel = label
        slider.accessibilityIdentifier = identifier
        slider.spokenValue = valueText
        slider.show(value)
    }
}

// MARK: Swipe to delete

/// A list's swipe to delete, for a row in a panel: swipe the row left to
/// show Delete, tap Delete to remove it; swipe right or tap elsewhere on the
/// row to put it back. VoiceOver offers Delete as an action. It is an extra
/// way in: the page keeps its visible Remove control (D114). A drag that
/// starts in one of the row's fields (marked ``View/setupSwipeExcluded()``)
/// belongs to the field and never swipes the row.
struct SetupSwipeToDelete: ViewModifier {
    let enabled: Bool
    let id: String
    let delete: () -> Void
    @State private var offset: CGFloat = 0
    @State private var open = false
    /// The row's fields, in ``space``.
    @State private var fields: [CGRect] = []

    /// The row's own coordinate space, which the fields report their frames in.
    static let space = "SetupSwipeToDelete"

    /// True when a drag from `start` may swipe the row: it starts outside
    /// every field.
    static func swipes(from start: CGPoint, fields: [CGRect]) -> Bool {
        !fields.contains { $0.contains(start) }
    }

    static let deleteText = "Delete"
    static let buttonWidth: CGFloat = 84
    /// How far a swipe goes before it opens Delete.
    static let openDistance: CGFloat = 50

    func body(content: Content) -> some View {
        ZStack(alignment: .trailing) {
            if open || offset < 0 {
                Button(role: .destructive) {
                    close()
                    delete()
                } label: {
                    Text(Self.deleteText)
                        .font(.body.weight(.semibold))
                        .frame(maxHeight: .infinity)
                }
                .buttonStyle(SetupButtonStyle(.danger))
                .frame(width: Self.buttonWidth)
                .accessibilityIdentifier("\(id).delete")
            }
            content
                // The row's own controls stay reachable; Delete is an action on them.
                .accessibilityElement(children: .contain)
                .accessibilityAction(named: Self.deleteText) {
                    if enabled {
                        delete()
                    }
                }
                .offset(x: offset)
                .contentShape(Rectangle())
                .allowsHitTesting(!open)
                .overlay {
                    if open {
                        // A tap on the shifted row puts it back.
                        Color.clear
                            .contentShape(Rectangle())
                            .padding(.trailing, Self.buttonWidth)
                            .onTapGesture { close() }
                            .accessibilityHidden(true)
                    }
                }
        }
        .onPreferenceChange(SetupSwipeExcludedKey.self) { fields = $0 }
        .simultaneousGesture(DragGesture(minimumDistance: 20, coordinateSpace: .named(Self.space)).onChanged { drag in
            guard enabled, Self.swipes(from: drag.startLocation, fields: fields),
                  abs(drag.translation.width) > abs(drag.translation.height) else {
                return
            }
            let base = open ? -Self.buttonWidth : 0
            offset = min(0, max(-Self.buttonWidth * 1.5, base + drag.translation.width))
        }.onEnded { drag in
            guard enabled, Self.swipes(from: drag.startLocation, fields: fields) else {
                return
            }
            let horizontal = abs(drag.translation.width) > abs(drag.translation.height)
            withAnimation(.snappy(duration: 0.2)) {
                if horizontal {
                    open = (open ? -Self.buttonWidth : 0) + drag.translation.width < -Self.openDistance
                }
                offset = open ? -Self.buttonWidth : 0
            }
        })
        // Outside the gesture, so the drag and the fields share the space.
        .coordinateSpace(name: Self.space)
        .onChange(of: enabled) { _, enabled in
            if !enabled {
                close()
            }
        }
    }

    private func close() {
        withAnimation(.snappy(duration: 0.2)) {
            open = false
            offset = 0
        }
    }
}

/// The frames of a swipe-to-delete row's fields, in the row's space.
struct SetupSwipeExcludedKey: PreferenceKey {
    static let defaultValue: [CGRect] = []

    static func reduce(value: inout [CGRect], nextValue: () -> [CGRect]) {
        value += nextValue()
    }
}

extension View {
    /// A field inside a ``SetupSwipeToDelete`` row: a drag that starts on it
    /// edits the field and never swipes the row.
    func setupSwipeExcluded() -> some View {
        background {
            GeometryReader { geometry in
                Color.clear.preference(key: SetupSwipeExcludedKey.self,
                                       value: [geometry.frame(in: .named(SetupSwipeToDelete.space))])
            }
        }
    }
}
