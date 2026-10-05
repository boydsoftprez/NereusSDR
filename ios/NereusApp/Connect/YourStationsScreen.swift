// NereusSDR for iOS: Your Cores: the paired Cores, each with Connect or Pair, and the ways to add one
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import SwiftUI

/// Your Cores (spec section 5.3 item 6, pictures 07 and 24): the paired
/// Cores, each with Connect, or with Pair when it removed or forgot this
/// phone (D70), when this phone made a new key (D72), or when it is found
/// on this network unclaimed; then, under On this network, the found Cores
/// that take a new device (D71); then Pair with a code and Enter an address. The notice over the list says why a Core
/// ended a session or why the phone can't sign in. The Core-not-answering
/// and needs-updating sheets rise over it. Each Core's row has a "⋯"
/// button for its actions, Rename (D76, D78), whose sheet rises over it
/// too; pressing and holding the row opens the same menu, as a shortcut.
struct YourStationsScreen: View {
    @ObservedObject var flow: ConnectionFlow
    @State private var showingAbout = false

    /// The footnote under the list.
    static let footnote = "This phone connects to a NereusSDR Core, not straight to a radio."
    /// The footnote after a new key (D72).
    static let oldEntriesNote =
        "Each Core still lists this phone under its old key. Remove that entry from Devices on another paired device, or on the Core."
    static let noneListed = "No Cores. Pairing adds one here, and it stays reachable from anywhere."

    var body: some View {
        ZStack(alignment: .bottom) {
            VStack(spacing: 0) {
                ConnectChrome.NavBar(title: "Cores", back: showsBack ? "Back" : nil, onBack: { flow.back() }) {
                    if !flow.keyUnreadable {
                        Menu {
                            Button("Pair with a code") { flow.pairWithCode() }
                            Button("Enter an address") { flow.enterAddress() }
                        } label: {
                            Text("+")
                                .font(.system(size: 26))
                                .foregroundStyle(ChromeColours.accent)
                                .frame(width: 40, height: 40)
                        }
                        .accessibilityLabel("Add a Core")
                        .accessibilityIdentifier("addCore")
                    }
                }
                ScrollView {
                    VStack(spacing: 8) {
                        if let notice = flow.notice, !Self.isPlaceTaken(notice) {
                            noticeBox(notice)
                                .padding(.top, 8)
                        }
                        if let problem = flow.removeProblem {
                            ConnectChrome.NoticeBox(title: "\(problem.core) is still saved.",
                                                    text: ConnectionFlow.removeSaveFailedText,
                                                    button: "Try again") {
                                flow.requestRemove(identityKey: problem.id)
                            }
                            .accessibilityIdentifier("removeCoreProblem")
                            .padding(.top, 8)
                        }
                        ConnectChrome.Heading(text: "Your Cores")
                            .padding(.top, 10)
                        if flow.cores.isEmpty {
                            Text(Self.noneListed)
                                .font(.system(size: 12))
                                .foregroundStyle(ChromeColours.textDim)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(12)
                                .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
                                .overlay(RoundedRectangle(cornerRadius: 4)
                                    .strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
                        }
                        ForEach(Array(flow.cores.enumerated()), id: \.element.id) { index, row in
                            CoreRowView(flow: flow, row: row, first: index == 0)
                        }
                        if let problem = flow.nearbyProblem,
                           flow.cores.contains(where: { $0.found?.identityPrefix == problem.id }) {
                            NearbyProblemBox(flow: flow, problem: problem)
                        }
                        if !flow.nearby.isEmpty {
                            ConnectChrome.Heading(text: "On this network")
                                .padding(.top, 10)
                            NearbyList(flow: flow)
                        }
                        if !flow.keyUnreadable {
                            VStack(spacing: 8) {
                                ConnectChrome.WideButton(title: "Pair with a code") { flow.pairWithCode() }
                                    .accessibilityIdentifier("pairWithCode")
                                ConnectChrome.WideButton(title: "Enter an address") { flow.enterAddress() }
                                    .accessibilityIdentifier("enterAddress")
                            }
                            .padding(.top, 16)
                        }
                        if flow.nearby.contains(where: { $0.offer == .closed }) {
                            ConnectChrome.Note(text: ConnectionFlow.closedNote)
                                .padding(.top, 12)
                        }
                        ConnectChrome.Note(text: flow.madeNewKey || (flow.keyUnreadable && !flow.cores.isEmpty)
                                           ? Self.oldEntriesNote : Self.footnote)
                            .padding(.top, 12)
                        Button("About this app") { showingAbout = true }
                            .font(.footnote)
                            .foregroundStyle(ChromeColours.accent)
                            .frame(minHeight: 44)
                            .accessibilityIdentifier("coresAbout")
                    }
                    .padding(.horizontal, 14)
                    .padding(.bottom, 20)
                }
            }
            if let notice = flow.notice, Self.isPlaceTaken(notice) {
                Color.black.opacity(0.65).ignoresSafeArea().accessibilityHidden(true)
                PlaceTakenScreen(flow: flow, notice: notice)
                    .transition(.opacity)
            } else if let trouble = flow.trouble {
                TroubleSheet(flow: flow, trouble: trouble)
                    .transition(.move(edge: .bottom))
            } else if let id = flow.renaming, let row = flow.cores.first(where: { $0.id == id }) {
                // The list stays in sight, dimmed and out of reach, while the sheet is up.
                Color.black.opacity(0.45)
                    .ignoresSafeArea()
                    .contentShape(Rectangle())
                    .onTapGesture {}
                    .accessibilityHidden(true)
                RenameCoreSheet(flow: flow, row: row)
                    .transition(.move(edge: .bottom))
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .animation(.easeOut(duration: 0.2), value: flow.trouble)
        .animation(.easeOut(duration: 0.2), value: flow.renaming)
        .alert("Remove Core?", isPresented: Binding(
            get: { flow.removeCandidate != nil },
            set: { if !$0 { flow.cancelRemove() } }
        )) {
            Button("Remove Core", role: .destructive) { flow.confirmRemove() }
            Button("Cancel", role: .cancel) { flow.cancelRemove() }
        } message: {
            Text(ConnectionFlow.removeConfirmationText)
        }
        .sheet(isPresented: $showingAbout) {
            NavigationStack { AboutAppPage(showsDone: true) }
        }
    }

    private var showsBack: Bool {
        flow.cores.isEmpty && flow.notice == nil
    }

    private static func isPlaceTaken(_ notice: ConnectionFlow.CoresNotice) -> Bool {
        if case .placeTaken = notice { return true }
        return false
    }

    @ViewBuilder
    private func noticeBox(_ notice: ConnectionFlow.CoresNotice) -> some View {
        switch notice {
        case .placeTaken:
            EmptyView()
        case .removed(let core):
            ConnectChrome.NoticeBox(title: "\(core) removed this phone.", text: "Pair with it again to use it.")
                .accessibilityIdentifier("coresNotice")
        case .words(let words):
            let parts = Self.firstSentence(words)
            ConnectChrome.NoticeBox(title: parts.first, text: parts.rest)
                .accessibilityIdentifier("coresNotice")
        case .keyUnreadable:
            ConnectChrome.NoticeBox(
                title: "This phone can't read its key any more.",
                text: "It happens after a phone is erased and restored. Without its key the phone can't sign in to your Cores. Make a new key, then pair with each Core again.",
                button: "Make a new key") { flow.makeNewKey() }
                .accessibilityIdentifier("coresNotice")
        case .keyUnavailable:
            ConnectChrome.NoticeBox(title: "This phone couldn't reach its key.",
                                    text: "Unlock the phone, then try again.", button: "Try again") { flow.retryKey() }
                .accessibilityIdentifier("coresNotice")
        }
    }

    /// "4 devices on it", from the count a Core's Bonjour record carries
    /// (ruling 10.4); nothing without a count, or with nobody on it.
    static func devicesOn(_ count: Int?) -> String? {
        guard let count, count > 0 else {
            return nil
        }
        return count == 1 ? "1 device on it" : "\(count) devices on it"
    }

    /// A Core row's line under its name: the address (and "pair again"),
    /// then how many devices are on it, then "Waiting for a radio" while
    /// its Bonjour record says it waits for one to be chosen (JJ,
    /// 2026-09-28). A radio connected or offline adds nothing: the record
    /// names no radio.
    static func detail(_ base: String, found: FoundStation?) -> String {
        let parts = [base, devicesOn(found?.devices), radioNote(found?.radio)].compactMap { $0 }
        return parts.joined(separator: " \u{00B7} ")
    }

    /// "Waiting for a radio" for a Core waiting for one to be chosen;
    /// nothing otherwise, or without the record's `radio`.
    static func radioNote(_ radio: FoundStation.RadioState?) -> String? {
        radio == .waiting ? waitingForRadioText : nil
    }

    static let waitingForRadioText = "Waiting for a radio"

    /// The first sentence, set bold as the board sets a notice, and the rest.
    static func firstSentence(_ words: String) -> (first: String, rest: String?) {
        guard let range = words.range(of: ". ") else {
            return (words, nil)
        }
        let rest = String(words[range.upperBound...]).trimmingCharacters(in: .whitespaces)
        return (String(words[..<range.lowerBound]) + ".", rest.isEmpty ? nil : rest)
    }
}

/// One Core in the list: its state, its name and address, and its button.
private struct CoreRowView: View {
    @ObservedObject var flow: ConnectionFlow
    let row: ConnectionFlow.CoreRow
    let first: Bool

    var body: some View {
        HStack(spacing: 12) {
            ConnectChrome.Pill(colour: pill)
            VStack(alignment: .leading, spacing: 3) {
                Text(row.label)
                    .font(.system(size: 15, weight: .bold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .lineLimit(1)
                    .truncationMode(.tail)
                Text(subtitle)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    // "Waiting for a radio" comes last; a long address
                    // must not push it out of sight, so the line may wrap.
                    .lineLimit(connecting || waiting ? 3 : 1)
                    .truncationMode(.tail)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 0)
            button
            actions
        }
        .padding(.vertical, 10)
        .padding(.leading, 12)
        .padding(.trailing, 4)
        .frame(minHeight: 66)
        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
        .opacity(flow.keyUnreadable ? 0.75 : 1)
        .contentShape(.contextMenuPreview, RoundedRectangle(cornerRadius: 4))
        .contextMenu { menu }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("core-\(row.label)")
    }

    /// The row's "⋯" button at its trailing edge (D78): the row's actions,
    /// the same menu as pressing and holding the row.
    private var actions: some View {
        Menu {
            menu
        } label: {
            Image(systemName: "ellipsis.circle")
                .font(.system(size: 20))
                .foregroundStyle(ChromeColours.accent)
                .frame(width: 44, height: 44)
                .contentShape(Rectangle())
        }
        .accessibilityLabel("Actions for \(row.label)")
        .accessibilityIdentifier("coreActions-\(row.label)")
    }

    /// The row's actions, from its "⋯" button or, as a shortcut, pressing
    /// and holding it: Rename, Addresses and local Remove Core.
    @ViewBuilder
    private var menu: some View {
        let availability = flow.renameAvailability(row)
        Button {
            flow.startRename(row)
        } label: {
            // A second text is the menu item's subtitle.
            Text("Rename")
            if let reason = ConnectionFlow.renameReason(availability) {
                Text(reason)
            }
        }
        .disabled(availability != .available)
        .accessibilityIdentifier("rename-\(row.label)")
        Button {
            flow.showAddresses(row)
        } label: {
            Text("Addresses")
        }
        .accessibilityIdentifier("addresses-\(row.label)")
        let removeReason = flow.removeDisabledReason(identityKey: row.id)
        Button(role: .destructive) {
            flow.requestRemove(identityKey: row.id)
        } label: {
            Text("Remove Core")
            if let removeReason { Text(removeReason) }
        }
        .disabled(removeReason != nil)
        .accessibilityIdentifier("removeCore-\(row.label)")
    }

    private var connecting: Bool { flow.connectingTo == row.id }

    /// The Core says it is waiting for a radio to be chosen.
    private var waiting: Bool { YourStationsScreen.radioNote(row.found?.radio) != nil }

    private var pill: Color {
        if connecting {
            return ConnectChrome.pillStale
        }
        return row.needsPairing ? ConnectChrome.pillStale : ConnectChrome.pillUnknown
    }

    private var subtitle: String {
        if connecting {
            return flow.connectingNote ?? "Connecting\u{2026}"
        }
        if flow.keyUnreadable {
            return "\(row.address) \u{00B7} needs pairing again"
        }
        let base = row.needsPairing ? "\(row.address) \u{00B7} pair again" : row.address
        return YourStationsScreen.detail(base, found: row.found)
    }


    @ViewBuilder
    private var button: some View {
        if connecting {
            ConnectChrome.RowButton(title: "Cancel") {
                Task { await flow.cancelConnecting() }
            }
            .accessibilityIdentifier("cancelConnecting")
        } else if flow.keyUnreadable {
            EmptyView()
        } else if row.needsPairing {
            if flow.claiming != nil && flow.claiming == row.found?.identityPrefix {
                ProgressView().tint(ChromeColours.text).frame(minWidth: 88, minHeight: 40)
            } else {
                ConnectChrome.RowButton(title: "Pair") { Task { await flow.pairAgain(row) } }
                    .disabled(flow.claiming != nil)
                    .accessibilityIdentifier("pair-\(row.label)")
            }
        } else {
            ConnectChrome.RowButton(title: "Connect", go: first) {
                Task { await flow.connect(to: row) }
            }
            .disabled(flow.connectingTo != nil)
            .accessibilityIdentifier("connect-\(row.label)")
        }
    }
}
