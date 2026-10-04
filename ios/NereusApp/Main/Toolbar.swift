// NereusSDR for iOS: the main screen's toolbar: RX panel, speaker, slice, pan, display and the link
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The toolbar over the band (spec section 5.1 item 1, D7, D11), left to
/// right: the RX panel's button, the speaker, the active slice, Pan 1,
/// Display, and the link's dot with its round-trip time. Sideways the same
/// buttons sit on either side of the Core's name. The TX panel's button
/// sits at the right end, after the link.
///
/// One tap on the speaker opens the Sound panel (D77) in a popover from the
/// button, without leaving the band; the speaker's colour still shows when
/// the band is muted. The slice button makes the next slice active. Pan 1
/// and Display each drop their sheet over the band, one sheet at a time; a
/// second tap closes it, and the open one's button shows open. A tap on the
/// link's dot opens Connection and performance under Tools.
///
/// On an iPad (spec section 5.6) the toolbar is the sideways one either way
/// up, without the panel buttons: the RX and TX applets have their own
/// place. On its side the right corner carries the applet column's button.
struct Toolbar: View {
    /// The toolbar's items, in their order.
    enum Item: String, CaseIterable {
        case rxPanel
        case speaker
        case slice
        case pan
        case display
        case link
        case txPanel
    }

    @ObservedObject var app: AppModel
    @ObservedObject var main: MainScreenModel
    let sideways: Bool
    @Binding var rxPanelOpen: Bool
    @Binding var txPanelOpen: Bool
    @Binding var soundPanelOpen: Bool
    @Binding var sheet: OpenSheet?
    /// The phone has no network at all.
    var offline = false
    /// Switches to the Radio tab from other main-screen controls.
    var openRadio: () -> Void = {}
    /// Opens the connection diagnostics destination from the link dot.
    var openPerformance: () -> Void = {}
    /// The iPad's layout, whose toolbar carries no panel buttons (spec
    /// section 5.6): the RX and TX applets sit beside or below the band.
    var layout: IPadLayout = .phone
    /// On its side, the iPad's applet column shows: the corner button's state.
    var columnShown: Binding<Bool>?

    static let height: CGFloat = 44
    /// The applet column's corner button (the iPad on its side).
    static let columnButtonIdentifier = "appletColumnButton"

    var body: some View {
        Group {
            if layout.isIPad {
                // The iPad's toolbar either way up: the phone's buttons, the
                // Core in the middle, the link, and on its side the column's button.
                HStack(spacing: 8) {
                    HStack(spacing: 10) {
                        speakerButton
                        sliceButton
                        panButton
                        displayButton
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    coreName
                    HStack(spacing: 8) {
                        linkChip(showsPath: true)
                        if layout.hasColumnButton, let columnShown {
                            columnButton(columnShown)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .trailing)
                }
                .padding(.horizontal, 8)
                .accessibilityElement(children: .contain)
                .accessibilityIdentifier("toolbarIPad")
            } else if sideways {
                // The board's sideways toolbar: the buttons, the Core in the middle, the link.
                HStack(spacing: 8) {
                    HStack(spacing: 0) {
                        rxPanelButton
                        speakerButton
                        sliceButton
                        panButton
                        displayButton
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    coreName
                    HStack(spacing: 4) {
                        linkChip(showsPath: true)
                        txPanelButton
                    }
                    .frame(maxWidth: .infinity, alignment: .trailing)
                }
                .accessibilityElement(children: .contain)
                .accessibilityIdentifier("toolbarSideways")
            } else {
                HStack(spacing: 0) {
                    rxPanelButton
                    Spacer(minLength: 0)
                    speakerButton
                    Spacer(minLength: 0)
                    sliceButton
                    Spacer(minLength: 0)
                    panButton
                    Spacer(minLength: 0)
                    displayButton
                    Spacer(minLength: 0)
                    // Upright the chip names the path only when it is the
                    // relay, as picture 07 draws "Relay 71 ms".
                    linkChip(showsPath: app.linkRelayed)
                    Spacer(minLength: 0)
                    txPanelButton
                }
                .padding(.horizontal, 4)
                .accessibilityElement(children: .contain)
                .accessibilityIdentifier("toolbar")
            }
        }
        .frame(height: Self.height)
        .background(ChromeColours.bar.ignoresSafeArea(edges: [.top, .horizontal]))
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.barBorder).frame(height: 1).ignoresSafeArea(edges: .horizontal)
        }
    }

    // MARK: The buttons

    private var rxPanelButton: some View {
        Button {
            rxPanelOpen.toggle()
            if rxPanelOpen {
                sheet = nil
                txPanelOpen = false
            }
        } label: {
            PanelGlyph(side: .leading)
                .stroke(rxPanelOpen ? ChromeColours.accent : ChromeColours.icon,
                        style: StrokeStyle(lineWidth: 1.7, lineCap: .round, lineJoin: .round))
                .frame(width: 22, height: 22)
                .frame(width: 40, height: 40)
                .background(rxPanelOpen ? ChromeColours.iconOpenBackground : .clear, in: RoundedRectangle(cornerRadius: 8))
                .overlay {
                    if rxPanelOpen {
                        RoundedRectangle(cornerRadius: 8).strokeBorder(ChromeColours.iconOpenBorder, lineWidth: 1)
                    }
                }
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("RX panel")
        .accessibilityValue(rxPanelOpen ? "Open" : "Closed")
        .accessibilityIdentifier(Item.rxPanel.rawValue)
    }

    /// The TX panel's button, the RX panel's mirror at the right end.
    private var txPanelButton: some View {
        Button {
            txPanelOpen.toggle()
            if txPanelOpen {
                sheet = nil
                rxPanelOpen = false
                soundPanelOpen = false
            }
        } label: {
            PanelGlyph(side: .trailing)
                .stroke(txPanelOpen ? ChromeColours.accent : ChromeColours.icon,
                        style: StrokeStyle(lineWidth: 1.7, lineCap: .round, lineJoin: .round))
                .frame(width: 22, height: 22)
                .frame(width: 40, height: 40)
                .background(txPanelOpen ? ChromeColours.iconOpenBackground : .clear,
                            in: RoundedRectangle(cornerRadius: 8))
                .overlay {
                    if txPanelOpen {
                        RoundedRectangle(cornerRadius: 8).strokeBorder(ChromeColours.iconOpenBorder, lineWidth: 1)
                    }
                }
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("TX panel")
        .accessibilityValue(txPanelOpen ? "Open" : "Closed")
        .accessibilityIdentifier(Item.txPanel.rawValue)
    }

    /// The applet column's button at the toolbar's right corner: hides the
    /// column so the band takes the whole width, and brings it back.
    private func columnButton(_ shown: Binding<Bool>) -> some View {
        Button {
            shown.wrappedValue.toggle()
        } label: {
            PanelGlyph(side: .trailing)
                .stroke(shown.wrappedValue ? ChromeColours.accent : ChromeColours.icon,
                        style: StrokeStyle(lineWidth: 1.7, lineCap: .round, lineJoin: .round))
                .frame(width: 22, height: 22)
                .frame(width: 40, height: 40)
                .background(shown.wrappedValue ? ChromeColours.iconOpenBackground : .clear,
                            in: RoundedRectangle(cornerRadius: 8))
                .overlay {
                    if shown.wrappedValue {
                        RoundedRectangle(cornerRadius: 8).strokeBorder(ChromeColours.iconOpenBorder, lineWidth: 1)
                    }
                }
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Applet column")
        .accessibilityValue(shown.wrappedValue ? "Shown" : "Hidden")
        .accessibilityIdentifier(Self.columnButtonIdentifier)
    }

    private var speakerButton: some View {
        Button {
            soundPanelOpen = true
        } label: {
            SpeakerGlyph()
                .stroke(app.audioMuted ? ChromeColours.iconMuted : ChromeColours.icon,
                        style: StrokeStyle(lineWidth: 1.7, lineCap: .round, lineJoin: .round))
                .frame(width: 22, height: 22)
                .frame(width: 40, height: 40)
                .background(soundPanelOpen ? ChromeColours.iconOpenBackground : .clear,
                            in: RoundedRectangle(cornerRadius: 8))
                .overlay {
                    if soundPanelOpen {
                        RoundedRectangle(cornerRadius: 8).strokeBorder(ChromeColours.iconOpenBorder, lineWidth: 1)
                    }
                }
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .popover(isPresented: $soundPanelOpen, arrowEdge: .top) {
            SoundPanel(app: app)
                .presentationCompactAdaptation(.popover)
                .presentationBackground(SoundPanel.background)
                .preferredColorScheme(.dark)
        }
        .accessibilityLabel("Sound options")
        .accessibilityValue(app.audioMuted ? "Muted" : "On")
        .accessibilityIdentifier(Item.speaker.rawValue)
    }

    /// The Slice button: the list of every slice on the Core drops under
    /// the toolbar (R-IOS-42), or closes; its letter is the active slice's.
    private var sliceButton: some View {
        let open = sheet == .slices
        return Button {
            if open {
                sheet = nil
            } else {
                sheet = .slices
                rxPanelOpen = false
                txPanelOpen = false
                soundPanelOpen = false
            }
        } label: {
            HStack(spacing: 5) {
                Text("Slice")
                SliceLetterBadge(letter: main.sliceLetter ?? "A", colour: BandColours.slice(main.sliceColour ?? BandSlice.colourUnknown))
                    .opacity(main.sliceLetter == nil ? 0.4 : 1)
            }
            .toolbarText(open: open)
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Slices on the Core, active slice")
        .accessibilityValue(main.sliceLetter.map { "Slice \($0)" } ?? "None")
        .accessibilityIdentifier(Item.slice.rawValue)
    }

    private var panButton: some View {
        sheetButton(.pan, title: "Pan \(MainScreenModel.panNumber)", item: .pan)
    }

    private var displayButton: some View {
        sheetButton(.display, title: "Display", item: .display)
    }

    /// Pan 1 or Display: opens its sheet, closing the other and the RX
    /// panel, or closes its own.
    private func sheetButton(_ kind: OpenSheet, title: String, item: Item) -> some View {
        let open = sheet == kind
        return Button {
            if open {
                sheet = nil
            } else {
                sheet = kind
                rxPanelOpen = false
                txPanelOpen = false
                soundPanelOpen = false
            }
        } label: {
            Text(title)
                .toolbarText(open: open)
        }
        .buttonStyle(.plain)
        .accessibilityValue(open ? "Open" : "Closed")
        .accessibilityIdentifier(item.rawValue)
    }

    // MARK: The Core and the link

    private var coreName: some View {
        VStack(spacing: 0) {
            Text(main.coreName ?? app.coreHost ?? "")
                .font(.system(size: 12, weight: .bold, design: .monospaced))
                .foregroundStyle(ChromeColours.text)
            if let radio = main.radioName {
                Text(radio)
                    .font(.system(size: 10, weight: .bold, design: .monospaced))
                    .foregroundStyle(ChromeColours.textFaint)
            }
        }
        .lineLimit(1)
        .padding(.horizontal, 10)
        .padding(.vertical, 1)
        .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.stationBorder, lineWidth: 1))
        .opacity(main.coreName == nil && app.coreHost == nil ? 0 : 1)
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("coreName")
    }

    /// The link's dot and round-trip time open Connection and performance.
    /// Its touch area reaches 44 points each way around the chip
    /// without taking any more of the toolbar's room.
    private func linkChip(showsPath: Bool) -> some View {
        let link = LinkState(connection: app.connection, roundTripMs: app.roundTripMs, offline: offline,
                             relayed: app.linkRelayed)
        let core = main.coreName ?? app.coreHost
        return Button {
            openPerformance()
        } label: {
            LinkChip(link: link, showsPath: showsPath, core: core)
                .padding(.horizontal, Self.linkTouchOutset)
                .padding(.vertical, (Self.height - LinkChip.height) / 2)
                .contentShape(Rectangle())
                .padding(.horizontal, -Self.linkTouchOutset)
        }
        .buttonStyle(.plain)
        .accessibilityElement(children: .ignore)
        .accessibilityAddTraits(.isButton)
        .accessibilityLabel("\(link.spoken(core: core)), opens Connection and performance")
        .accessibilityIdentifier(Item.link.rawValue)
    }

    /// How far the link button's touch area reaches past each side of the
    /// chip, so even the bare dot has 44 points to hit.
    static let linkTouchOutset: CGFloat = 10
}

private extension View {
    /// A toolbar word; `open` while its sheet is out (the board's `.tbtn.is-open`).
    func toolbarText(open: Bool = false) -> some View {
        font(.system(size: 14, weight: .semibold))
            .foregroundStyle(open ? ChromeColours.toolbarOpenText : ChromeColours.text)
            .lineLimit(1)
            .fixedSize()
            .padding(.horizontal, 5)
            .frame(height: 34)
            .background(open ? ChromeColours.toolbarOpenBackground : .clear, in: RoundedRectangle(cornerRadius: 4))
            .frame(height: 40)
            .contentShape(Rectangle())
    }
}
