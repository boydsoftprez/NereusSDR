// NereusSDR for iOS: Setup's Devices page: rename the Core, back up its key, who is connected and paired, Revoke, Add a device
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// Setup, Devices (R-IOS-08, R-IOS-17, D57; spec section 5.2 item 8,
/// section 5.8 item 13, section 5.9 item 4; pictures 05 and 22), as the
/// board draws it: the Core with Rename; the reminder to back up its key;
/// Connected now, then Paired, each device with one-tap Revoke but this
/// phone; Add a device, which shows the Core's code, and Close pairing; and
/// the note on four devices and Revoke.
struct DevicesPage: View {
    @ObservedObject var model: DevicesModel
    @ObservedObject var main: MainScreenModel
    @ObservedObject var flow: ConnectionFlow
    @State private var showingBackup = false

    static let backupTitle = "Back up the Core's key."
    static let backupText = "The key is the Core's identity. If its card fails without a backup, every device has to pair again."
    static let note = "Up to four devices can be connected at once. TX marks the one with transmit. Revoking drops that device at once, even mid-session."
    static let codeLead = "On the new device, choose Pair with a code and type:"
    static let codeFoot = "It works once. The pairing window closes when the new device pairs."

    var body: some View {
        ZStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 10) {
                    coreCard
                    if model.keyBackupNeeded {
                        backupCard
                    }
                    if model.listsConnected {
                        heading("Connected now")
                        VStack(spacing: 0) {
                            ForEach(model.connected) { row in
                                connectedRow(row)
                                if row.id != model.connected.last?.id {
                                    Divider().overlay(ConnectChrome.rowBorder)
                                }
                            }
                        }
                        .modifier(Card())
                    }
                    if !model.paired.isEmpty {
                        heading(model.listsConnected ? "Paired" : "Paired devices")
                        VStack(spacing: 0) {
                            ForEach(model.paired) { row in
                                pairedRow(row)
                                if row.id != model.paired.last?.id {
                                    Divider().overlay(ConnectChrome.rowBorder)
                                }
                            }
                        }
                        .modifier(Card())
                    }
                    addDevice
                    Text(Self.note)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("devicesNote")
                }
                .padding(12)
            }
            if let id = flow.renaming, let row = flow.cores.first(where: { $0.id == id }) {
                Color.black.opacity(0.45)
                    .ignoresSafeArea()
                    .contentShape(Rectangle())
                    .onTapGesture {}
                    .accessibilityHidden(true)
                VStack {
                    Spacer()
                    RenameCoreSheet(flow: flow, row: row)
                }
                .transition(.move(edge: .bottom))
            }
        }
        .background(ChromeColours.page.ignoresSafeArea())
        .animation(.easeOut(duration: 0.2), value: flow.renaming)
        .navigationTitle("Devices")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .core)
            }
        }
        .sheet(isPresented: $showingBackup) {
            KeyBackupSheet(model: model) { showingBackup = false }
                .presentationDetents([.medium, .large])
                .preferredColorScheme(.dark)
        }
    }

    // MARK: The Core

    private var renameRow: ConnectionFlow.CoreRow? { flow.connectedCoreRow }

    private var renameReason: String? {
        guard let row = renameRow else {
            return nil
        }
        return ConnectionFlow.renameReason(flow.renameAvailability(row))
    }

    private var coreCard: some View {
        HStack(spacing: 10) {
            VStack(alignment: .leading, spacing: 2) {
                Text(main.coreName ?? model.coreName ?? "")
                    .font(.system(size: 17, weight: .bold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .accessibilityIdentifier("devicesCoreName")
                if let radio = main.radioName {
                    Text(radio)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(1)
                }
                if let reason = renameReason {
                    Text("Rename: \(reason)")
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            smallButton("Rename", enabled: renameRow != nil && renameReason == nil, style: .plain, id: "devicesRename") {
                if let row = renameRow {
                    flow.startRename(row)
                }
            }
        }
        .padding(12)
        .modifier(Card())
    }

    private var backupCard: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(Self.backupTitle)
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(ConnectChrome.warn)
            Text(Self.backupText)
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.text)
                .fixedSize(horizontal: false, vertical: true)
            smallButton("Show me how", enabled: true, style: .plain, id: "keyBackupShowMe") {
                showingBackup = true
            }
            .padding(.top, 2)
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(ConnectChrome.warn.opacity(0.08), in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.warn.opacity(0.5), lineWidth: 1))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("keyBackupReminder")
    }

    // MARK: Devices

    private func heading(_ text: String) -> some View {
        Text(text.uppercased())
            .font(.system(size: 11, weight: .bold))
            .tracking(0.8)
            .foregroundStyle(ConnectChrome.heading)
            .padding(.top, 6)
            .accessibilityAddTraits(.isHeader)
    }

    private func connectedRow(_ row: DevicesModel.ConnectedRow) -> some View {
        deviceRow(id: row.id, dot: row.away ? ConnectChrome.pillStale : ConnectChrome.pillOn, name: row.name,
                  tx: row.holdsTransmit, revoke: row.revoke) {
            HStack(spacing: 4) {
                ForEach(Array(row.slices.enumerated()), id: \.offset) { _, slice in
                    Text(slice.letter)
                        .font(.system(size: 10, weight: .heavy))
                        .foregroundStyle(Color.black)
                        .frame(width: 14, height: 14)
                        .background(BandColours.slice(slice.colour), in: RoundedRectangle(cornerRadius: 2))
                        .accessibilityLabel("Slice \(slice.letter)")
                    if !slice.band.isEmpty {
                        Text(slice.band)
                    }
                }
                if !row.slices.isEmpty {
                    Text("\u{00B7}")
                }
                Text(row.line)
                    .foregroundStyle(row.away ? ConnectChrome.pillStale : ChromeColours.textDim)
            }
        }
    }

    private func pairedRow(_ row: DevicesModel.PairedRow) -> some View {
        deviceRow(id: row.id, dot: row.isThisPhone ? ConnectChrome.pillOn : ConnectChrome.pillOff, name: row.name,
                  tx: false, revoke: row.revoke) {
            Text(row.line)
        }
    }

    private func deviceRow<Line: View>(id: String, dot: Color, name: String, tx: Bool, revoke: DevicesModel.Revoke,
                                       @ViewBuilder line: () -> Line) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 10) {
                Circle().fill(dot).frame(width: 9, height: 9)
                VStack(alignment: .leading, spacing: 3) {
                    HStack(spacing: 6) {
                        Text(name)
                            .font(.system(size: 14, weight: .bold))
                            .foregroundStyle(ChromeColours.text)
                            .lineLimit(1)
                        if tx {
                            Text("TX")
                                .font(.system(size: 9, weight: .heavy))
                                .foregroundStyle(BandColours.txBadgeOnText)
                                .padding(.horizontal, 4)
                                .padding(.vertical, 1)
                                .background(BandColours.txBadgeOn, in: RoundedRectangle(cornerRadius: 2))
                                .overlay(RoundedRectangle(cornerRadius: 2)
                                    .strokeBorder(BandColours.txBadgeOnBorder, lineWidth: 1))
                                .accessibilityLabel("has transmit")
                        }
                    }
                    line()
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(2)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                switch revoke {
                case .none:
                    EmptyView()
                case .available:
                    smallButton("Revoke", enabled: !model.revoking.contains(id), style: .revoke,
                                id: "revoke.\(id)") {
                        Task { await model.revoke(id) }
                    }
                case .unavailable:
                    smallButton("Revoke", enabled: false, style: .revoke, id: "revoke.\(id)") {}
                }
            }
            if case .unavailable(let reason) = revoke {
                Text(reason)
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.leading, 19)
                    .accessibilityIdentifier("revokeReason.\(id)")
            }
            if let problem = model.revokeProblems[id] {
                Text(problem)
                    .font(.system(size: 11))
                    .foregroundStyle(ConnectChrome.badText)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.leading, 19)
                    .accessibilityIdentifier("revokeProblem.\(id)")
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("device.\(id)")
    }

    // MARK: Add a device

    @ViewBuilder
    private var addDevice: some View {
        if model.pairingOpen && !model.pairingCode.isEmpty {
            VStack(alignment: .leading, spacing: 6) {
                Text(Self.codeLead)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.text)
                Text(model.pairingCode)
                    .font(.system(size: 22, weight: .bold, design: .monospaced))
                    .foregroundStyle(ConnectChrome.codeText)
                    .textSelection(.enabled)
                    .accessibilityIdentifier("pairingCode")
                Text(Self.codeFoot)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                ConnectChrome.WideButton(title: "Close pairing", enabled: model.pairs, busy: model.pairingBusy) {
                    Task { await model.closePairing() }
                }
                .padding(.top, 4)
                .accessibilityIdentifier("closePairing")
            }
            .padding(12)
            .modifier(Card())
        } else {
            ConnectChrome.WideButton(title: "Add a device", look: .go, enabled: model.pairs, busy: model.pairingBusy) {
                Task { await model.openPairing() }
            }
            .padding(.top, 4)
            .accessibilityIdentifier("addDevice")
            if !model.pairs {
                Text("Add a device: \(DevicesModel.needsNewerCoreText)")
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
            }
        }
        if let problem = model.pairingProblem {
            Text(problem)
                .font(.system(size: 12))
                .foregroundStyle(ConnectChrome.badText)
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("pairingProblem")
        }
    }

    // MARK: Pieces

    private enum ButtonStyleKind {
        case plain
        case revoke
    }

    private func smallButton(_ title: String, enabled: Bool, style: ButtonStyleKind, id: String,
                             action: @escaping () -> Void) -> some View {
        let revoke = style == .revoke
        return Button(action: action) {
            Text(title)
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(enabled ? (revoke ? ChromeColours.revokeText : ChromeColours.text)
                    : ChromeColours.buttonOffText)
                .padding(.horizontal, 12)
                .frame(minHeight: 36)
                .background(enabled ? (revoke ? ChromeColours.revoke : ChromeColours.button) : ChromeColours.buttonOff,
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(enabled ? (revoke ? ChromeColours.revokeBorder : ChromeColours.buttonBorder)
                        : ChromeColours.buttonOffBorder, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityIdentifier(id)
    }

    /// The board's card: a dark row fill with a thin border.
    private struct Card: ViewModifier {
        func body(content: Content) -> some View {
            content
                .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.stationBorder, lineWidth: 1))
        }
    }
}
