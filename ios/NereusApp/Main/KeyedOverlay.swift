// NereusSDR for iOS: what transmit puts over the waterfall: the keyed gauges, and the Core's refusals and stops
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusModels
import SwiftUI

/// The keyed view's gauges on the waterfall (spec section 5.1 item 7, D14):
/// Radio/Amp/Tuner/Custom tabs and three individually replaceable gauges
/// (JJ, 2026-10-03), defaulting to RF power, SWR and mic level on Radio.
/// Choices stay on this phone; the overlay is visible only for its own key.
/// Below them, or alone
/// while unkeyed, the band says what the Core said of this phone's
/// transmit: a refusal in red with the fix it offers, a stop in amber, the
/// Core's reason for a tap while another device holds transmit, and after
/// a lost link that the Core stops on its own.
struct KeyedOverlay: View {
    @ObservedObject var transmit: TransmitModel
    @ObservedObject var accessories: AccessoriesModel
    @ObservedObject var keyedMeters: KeyedMetersModel
    /// Take transmit, for a refusal that offers it.
    var take: TransmitTakeModel? = nil
    let meters: StationCatalog.Meters?
    let showsStrip: Bool
    /// The spectrum's share of the band, the pan's split.
    var spectrumShare: CGFloat = BandLayout.spectrumShare
    let sideways: Bool

    static let gaugesWidth: CGFloat = 330

    @State private var editingSlot: Int?
    @State private var panelHeight: CGFloat = 0
    @State private var suppressTapUntil: TimeInterval = 0
    @Environment(\.dynamicTypeSize) private var textSize

    private var availability: KeyedMeterAvailability {
        KeyedMeterReadings.availability(transmit: transmit, accessories: accessories)
    }
    private var page: KeyedMeterPage { keyedMeters.settings.displayPage(availability: availability) }

    var body: some View {
        GeometryReader { proxy in
            let layout = BandLayout(size: proxy.size, scale: 1, showsStrip: showsStrip, spectrumShare: spectrumShare)
            let usualTop = layout.waterfall.minY + (sideways ? 14 : 8)
            let height = panelHeight > 0 ? panelHeight : sideways ? 104 : 154
            let bottom = sideways ? proxy.size.height : PttButton.frame(bandSize: proxy.size, leadingInset: 0).minY - 8
            let top = transmit.ptt.transmitting ? min(usualTop, max(0, bottom - height)) : usualTop
            VStack(spacing: 8) {
                if transmit.ptt.transmitting {
                    gauges
                        .frame(maxWidth: Self.gaugesWidth)
                        .overlay(alignment: .top) {
                            if let slot = editingSlot {
                                chooser(slot: slot, maximumHeight: max(44, min(320, bottom - top)))
                            }
                        }
                        .transition(.opacity)
                }
                TxNoticeCard(transmit: transmit, take: take)
                    .frame(maxWidth: sideways ? 546 : .infinity)
            }
            .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { panelHeight = $0 }
            .padding(.horizontal, 10)
            .frame(maxWidth: .infinity)
            .offset(y: top)
        }
        .onChange(of: transmit.ptt.transmitting) { _, keyed in if !keyed { editingSlot = nil } }
        .onChange(of: keyedMeters.settings.page) { _, _ in editingSlot = nil }
        .onChange(of: availability) { _, _ in editingSlot = nil }
    }

    private var gauges: some View {
        VStack(spacing: 4) {
            HStack(spacing: 4) {
                ForEach(availability.visiblePages) { target in
                    Button {
                        guard ProcessInfo.processInfo.systemUptime >= suppressTapUntil else { return }
                        editingSlot = nil
                        keyedMeters.choose(target, availability: availability)
                    } label: {
                        Text(target.rawValue)
                            .font(.system(size: textSize.isAccessibilitySize ? 15 : 12, weight: .semibold))
                            .frame(maxWidth: .infinity, minHeight: 44)
                            .background(target == page ? ChromeColours.gaugeNormal.opacity(0.24) : ChromeColours.bar,
                                        in: RoundedRectangle(cornerRadius: 4))
                            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(
                                target == page ? ChromeColours.gaugeNormal : ChromeColours.panelEdge, lineWidth: 1))
                    }
                    .buttonStyle(.plain)
                    .foregroundStyle(ChromeColours.text)
                    .disabled(!availability.status(for: target).isAvailable)
                    .accessibilityAddTraits(target == page ? [.isSelected] : [])
                    .accessibilityHint(availability.status(for: target).reason ?? "Show \(target.rawValue) meters")
                    .accessibilityIdentifier("keyedTab-\(target.rawValue)")
                }
            }
            if sideways {
                HStack(spacing: 8) { ForEach(0..<3) { slot in gauge(slot) } }
            } else {
                HStack(spacing: 10) { gauge(0); gauge(1) }
                gauge(2)
            }
            if !reasons.isEmpty {
                ScrollView {
                    VStack(alignment: .leading, spacing: 3) {
                        ForEach(reasons, id: \.self) { reason in
                            Text(reason)
                                .font(.system(size: textSize.isAccessibilitySize ? 14 : 11))
                                .foregroundStyle(ChromeColours.caption)
                                .fixedSize(horizontal: false, vertical: true)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                    }
                }
                .frame(height: sideways ? 40 : 64)
                .accessibilityIdentifier("keyedMeterReasons")
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 6)
        .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
        .simultaneousGesture(DragGesture(minimumDistance: 30).onChanged { drag in
            if abs(drag.translation.width) >= 30, abs(drag.translation.width) > abs(drag.translation.height) {
                suppressTapUntil = ProcessInfo.processInfo.systemUptime + 0.3
            }
        }.onEnded { drag in
            guard abs(drag.translation.width) >= 40,
                  abs(drag.translation.width) > abs(drag.translation.height) else { return }
            suppressTapUntil = ProcessInfo.processInfo.systemUptime + 0.3
            editingSlot = nil
            keyedMeters.swipe(direction: drag.translation.width < 0 ? 1 : -1, availability: availability)
        }, including: editingSlot == nil ? .all : .none)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("keyedGauges")
    }

    private func gauge(_ slot: Int) -> some View {
        let meter = keyedMeters.settings.meters(on: page)[slot]
        return KeyedMeterGauge(sample: sample(meter), slot: slot) {
            guard ProcessInfo.processInfo.systemUptime >= suppressTapUntil else { return }
            editingSlot = slot
        }
    }
    private func sample(_ meter: KeyedMeter) -> KeyedMeterSample {
        KeyedMeterReadings.sample(meter, transmit: transmit, accessories: accessories, meters: meters)
    }
    private var reasons: [String] {
        var notes = availability.visiblePages.compactMap { availability.status(for: $0).reason }
        for meter in keyedMeters.settings.meters(on: page) {
            if let reason = sample(meter).reason, !notes.contains(reason) { notes.append(reason) }
        }
        return notes
    }

    private func chooser(slot: Int, maximumHeight: CGFloat) -> some View {
        VStack(spacing: 6) {
            HStack(spacing: 6) {
                Text("Replace \(sample(keyedMeters.settings.meters(on: page)[slot]).name)")
                    .font(.system(size: textSize.isAccessibilitySize ? 15 : 12, weight: .bold))
                    .lineLimit(2)
                Spacer(minLength: 0)
                Button { editingSlot = nil } label: {
                    Text("Done")
                        .font(.system(size: 14, weight: .semibold))
                        .frame(minWidth: 44, minHeight: 44)
                        .contentShape(Rectangle())
                }
                    .buttonStyle(.plain)
                    .accessibilityIdentifier("keyedMeterDone")
            }
            ScrollView {
                VStack(alignment: .leading, spacing: 6) {
                    if let reason = availability.stagesReason {
                        Text(reason)
                            .font(.system(size: textSize.isAccessibilitySize ? 14 : 11))
                            .foregroundStyle(ChromeColours.caption)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 4) {
                        ForEach(availability.pickerMeters) { meter in
                            let selected = keyedMeters.settings.meters(on: page)[slot] == meter
                            Button {
                                keyedMeters.replace(slot: slot, with: meter, on: page, availability: availability)
                                editingSlot = nil
                            } label: {
                                Text(sample(meter).name)
                                    .font(.system(size: textSize.isAccessibilitySize ? 14 : 11, weight: .semibold))
                                    .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
                                    .padding(.horizontal, 6)
                                    .background(selected ? ChromeColours.gaugeNormal.opacity(0.24) : ChromeColours.bar,
                                                in: RoundedRectangle(cornerRadius: 3))
                            }
                            .buttonStyle(.plain)
                            .disabled(!availability.status(for: meter).isAvailable)
                            .accessibilityAddTraits(selected ? [.isSelected] : [])
                            .accessibilityHint(availability.status(for: meter).reason ?? "Replace only this slot")
                            .accessibilityIdentifier("keyedMeterChoice-\(meter.id)")
                        }
                    }
                }
            }
            .frame(maxHeight: .infinity)
        }
        .padding(10)
        .frame(height: maximumHeight)
        .foregroundStyle(ChromeColours.text)
        .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Choose overlay meter")
        .accessibilityIdentifier("keyedMeterChooser")
    }

}

/// One card on the band for the Core's word on this phone's transmit, the
/// newest that applies: a refusal, the reason a tap sent nothing, a stop,
/// or the lost link. A tap on it clears a refusal or a stop. A refusal
/// or reason that offers taking transmit (`takeTransmit`) carries Take
/// transmit, as one that offers the amplifier's operate carries Operate amp.
struct TxNoticeCard: View {
    @ObservedObject var transmit: TransmitModel
    /// Take transmit, for a refusal that offers it.
    var take: TransmitTakeModel? = nil

    /// What the card says.
    enum Content: Equatable {
        case refusal(TxRefusalInfo)
        case held(String)
        case stop(TransmitStopNotice)
        case linkLost
    }

    static func content(ptt: PttController.Snapshot, heldNote: String?) -> Content? {
        if case .refused(let refusal) = ptt.state {
            return .refusal(refusal)
        }
        if let heldNote, !heldNote.isEmpty {
            return .held(heldNote)
        }
        if let stop = ptt.stop {
            return .stop(stop)
        }
        if ptt.state == .linkLost {
            return .linkLost
        }
        return nil
    }

    var body: some View {
        if let content = Self.content(ptt: transmit.ptt, heldNote: transmit.heldNote) {
            card(content)
                .onTapGesture {
                    transmit.dismissNotice()
                }
                .accessibilityIdentifier("txNotice")
        }
    }

    @ViewBuilder
    private func card(_ content: Content) -> some View {
        HStack(alignment: .top, spacing: 10) {
            Circle()
                .fill(dot(content))
                .frame(width: 10, height: 10)
                .shadow(color: Self.takeOver(content) ? ChromeColours.noticeWarn.opacity(0.6) : .clear, radius: 3)
                .padding(.top, 4)
            VStack(alignment: .leading, spacing: 8) {
                text(content)
                    .font(.system(size: 13))
                    .lineSpacing(2)
                    .fixedSize(horizontal: false, vertical: true)
                if case .refusal(let refusal) = content, refusal.fix == TxRefusalInfo.operateAmp, transmit.amp != nil {
                    Button {
                        transmit.applyFix(refusal.fix)
                    } label: {
                        Text("Operate amp")
                            .font(.system(size: 13, weight: .bold))
                            .foregroundStyle(.white)
                            .padding(.horizontal, 12)
                            .frame(height: 32)
                            .background(ChromeColours.operateOn, in: RoundedRectangle(cornerRadius: 4))
                            .overlay(RoundedRectangle(cornerRadius: 4)
                                .strokeBorder(ChromeColours.operateOnBorder, lineWidth: 1))
                    }
                    .buttonStyle(.plain)
                    .accessibilityIdentifier("txFix")
                }
                if let take, Self.offersTake(content, permission: transmit.permission) {
                    TakeTransmitButton(take: take, identifier: "txTakeTransmitFix") {
                        transmit.dismissNotice()
                    }
                }
            }
            Spacer(minLength: 0)
        }
        .padding(.vertical, 10)
        .padding(.horizontal, 12)
        .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
        .contentShape(Rectangle())
    }

    /// Whether the card offers Take transmit: a refusal whose fix is
    /// `takeTransmit`, or the reason a tap sent nothing while the Core's
    /// reason this phone may not transmit offers it.
    static func offersTake(_ content: Content, permission: TxRefusalInfo?) -> Bool {
        switch content {
        case .refusal(let refusal):
            return refusal.fix == TxRefusalInfo.takeTransmit
        case .held:
            return permission?.fix == TxRefusalInfo.takeTransmit
        case .stop, .linkLost:
            return false
        }
    }

    /// The Core's first-key refusal after a take (`chooseTransmitSlice`):
    /// the board's take-over notice, amber dot and plain words (R-IOS-42).
    static func takeOver(_ content: Content) -> Bool {
        if case .refusal(let refusal) = content {
            return refusal.code == BandSlicesModel.chooseTransmitSliceCode
        }
        return false
    }

    private func dot(_ content: Content) -> Color {
        if Self.takeOver(content) {
            return ChromeColours.noticeWarn
        }
        switch content {
        case .refusal, .held:
            return ChromeColours.gaugeRed
        case .stop:
            return ChromeColours.noticeWarn
        case .linkLost:
            return ChromeColours.noticeInfo
        }
    }

    private func text(_ content: Content) -> Text {
        switch content {
        case .refusal(let refusal) where Self.takeOver(content):
            return Text(refusal.reason).foregroundColor(ChromeColours.text)
        case .refusal(let refusal):
            return Text(refusal.reason).foregroundColor(ChromeColours.refusalText)
        case .held(let words):
            return Text(words).foregroundColor(ChromeColours.refusalText)
        case .stop(let stop):
            return Self.firstSentenceBold(stop.text)
        case .linkLost:
            return Text(PttController.linkLostText).foregroundColor(ChromeColours.text)
        }
    }

    /// The Core's sentence as sent, its first sentence in bold, as the
    /// board sets the band's notices.
    static func firstSentenceBold(_ text: String) -> Text {
        var styled = AttributedString(text)
        styled.foregroundColor = ChromeColours.text
        let first = text.range(of: ". ").map { String(text[..<$0.lowerBound]) + "." } ?? text
        if let range = styled.range(of: first) {
            styled[range].font = .system(size: 13, weight: .bold)
            styled[range].foregroundColor = ChromeColours.textBright
        }
        return Text(styled)
    }
}
