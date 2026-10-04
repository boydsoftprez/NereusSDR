// NereusSDR for iOS: the every-slice list's rows: letter order, where each slice is, who holds it, what may be done
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-11, R-IOS-42 (JJ's rulings of 2026-09-30, the board's
/// `#slicelist-review`): the rows behind the Slice button, in the board's
/// Rock cast: A at 7.177.000 RADE-L on another pan held by the Core, B and
/// C this phone's, and D on 20 m held by Shack desktop with JJ's iPad
/// listening.
@Suite struct SliceRosterTests {
    static let me = "phone"
    static let hostingId = "c3RhdGlvbg"

    static func device(_ id: String, _ name: String, kind: String = "computer",
                       hosts: Bool = false) -> SeveralDevices.ConnectedDevice {
        SeveralDevices.ConnectedDevice(deviceId: id, name: name, shortName: "", kind: kind, paired: true,
                                       hostsCore: hosts, revocable: true, state: .listening, holdsTransmit: false,
                                       lastActivitySeconds: 0, connectedForSeconds: 0, awayForSeconds: 0,
                                       transmittingForSeconds: 0, listeningOn: [], transmittingOn: nil)
    }

    static func marker(_ id: Int, hz: Double, owner: String, name: String = "", kind: String = "computer",
                       band: Int = 3, stream: Int = 1) -> SeveralDevices.SliceMarker {
        SeveralDevices.SliceMarker(sliceId: id, ownerDeviceId: owner, ownerName: name, ownerShortName: "",
                                   ownerKind: kind, ownerAway: false, frequencyHz: hz, mode: 0, filterLowHz: -3000,
                                   filterHighHz: -100, txSlice: false, band: band, streamIndex: stream, psPaused: false)
    }

    static func access(_ id: Int, _ controller: String, listeners: [String], onAir: Bool = false) -> SliceAccess.State {
        SliceAccess.State(sliceId: id, incarnation: Int64(10 + id), controllerDeviceId: controller, controlRevision: 1,
                          listenerDeviceIds: listeners, onAir: onAir)
    }

    static let devices = [device("phone", "JJ's iPhone", kind: "phone"), device("desk", "Shack desktop"),
                          device("pad", "JJ's iPad", kind: "tablet")]

    static let own = [
        SliceRoster.Slice(sliceId: 2, frequencyHz: 7_249_000, mode: 0, band: 3, streamIndex: 0, panKey: "pan-7"),
        SliceRoster.Slice(sliceId: 1, frequencyHz: 7_236_400, mode: 0, band: 3, streamIndex: 0, panKey: "pan-7"),
    ]

    func rock(version: Int64 = 3, aJoined: Bool = false, aOnAir: Bool = false) -> [SliceRoster.Row] {
        var joined = Self.own
        var access: [Int: SliceAccess.State] = [
            1: Self.access(1, Self.me, listeners: [Self.me, "desk"]),
            2: Self.access(2, Self.me, listeners: [Self.me]),
            0: Self.access(0, "station", listeners: aJoined ? ["station", Self.me] : ["station"], onAir: aOnAir),
            3: Self.access(3, "desk", listeners: ["desk", "pad"]),
        ]
        if aJoined {
            joined.append(SliceRoster.Slice(sliceId: 0, frequencyHz: 7_177_000, mode: 11, band: 3, streamIndex: 1,
                                            panKey: "pan-0"))
        }
        if version == 0 {
            access = [:]
        }
        return SliceRoster.rows(joined: joined,
                                markers: [Self.marker(0, hz: 7_177_000, owner: "", kind: "station"),
                                          Self.marker(3, hz: 14_074_000, owner: "desk", name: "Shack desktop",
                                                      band: 5, stream: 2)],
                                access: access, devices: Self.devices, me: Self.me, version: version,
                                homePanKey: "pan-7", homeStreamIndex: 0)
    }

    @Test func rowsAreInLetterOrderAndNeverRegrouped() {
        #expect(rock().map(\.letter) == ["A", "B", "C", "D"])
        // Joining A changes its row and not its place.
        #expect(rock(aJoined: true).map(\.letter) == ["A", "B", "C", "D"])
    }

    @Test func eachRowSaysWhereItIsAndWhoHoldsIt() {
        let rows = rock()
        #expect(rows.map(\.here) == [false, true, true, false])
        #expect(rows.map(\.relation) == [.none, .control, .control, .none])
        #expect(rows.map(\.holderLine) == ["Controlled by the Core", "You control", "You control",
                                           "Controlled by Shack desktop"])
        #expect(rows[1].alsoListening == ["Shack desktop"])
        #expect(rows[3].alsoListening == ["JJ's iPad"])
        #expect(rows.allSatisfy { $0.takeRefusal == nil })
        let listening = rock(aJoined: true)
        #expect(listening[0].relation == .listening)
        #expect(listening[0].holderLine == "Listening \u{00B7} controlled by the Core")
        #expect(!listening[0].here)
    }

    @Test func theHostingDesktopIsNamedByItsName() {
        var devices = Self.devices
        devices.append(Self.device(Self.hostingId, "Shack PC", hosts: true))
        let rows = SliceRoster.rows(joined: [], markers: [Self.marker(0, hz: 7_177_000, owner: "", kind: "station")],
                                    access: [0: Self.access(0, "station", listeners: ["station"])],
                                    devices: devices, me: Self.me, version: 3, homePanKey: nil, homeStreamIndex: nil)
        #expect(rows.first?.holderLine == "Controlled by Shack PC")
        // With no access object, the marker's own position is named the same way.
        let bare = SliceRoster.rows(joined: [], markers: [Self.marker(0, hz: 7_177_000, owner: "", kind: "station")],
                                    access: [:], devices: devices, me: Self.me, version: 3, homePanKey: nil,
                                    homeStreamIndex: nil)
        #expect(bare.first?.holder == "Shack PC")
    }

    @Test(arguments: [Int64(1), 2, 3])
    func takeControlIsGreyedOnlyOnTheAir(_ version: Int64) {
        let rows = rock(version: version, aOnAir: true)
        #expect(rows[0].takeRefusal == "Slice A is transmitting. Take control once it stops.")
        #expect(rows[3].takeRefusal == nil)
        // This phone's own slices offer no take.
        #expect(rows[1].takeRefusal == nil && rows[2].takeRefusal == nil)
    }

    @Test func anOlderCoreListsOnlyThisPhonesSlices() {
        let rows = rock(version: 0)
        #expect(rows.map(\.letter) == ["B", "C"])
        #expect(rows.allSatisfy { $0.relation == .control && $0.here })
    }

    @Test func aRefusedNewSliceOffersTheCoresUsableSlices() {
        let text = #"[{"sliceId":0,"incarnation":11,"letter":"A","controllerDeviceId":""},"#
            + #"{"sliceId":3,"incarnation":14,"letter":"D","controllerDeviceId":"desk"},{"bad":1}]"#
        #expect(SliceRoster.usableSlices(text) == [
            SliceRoster.UsableSlice(sliceId: 0, incarnation: 11, letter: "A", controllerDeviceId: ""),
            SliceRoster.UsableSlice(sliceId: 3, incarnation: 14, letter: "D", controllerDeviceId: "desk"),
        ])
        #expect(SliceRoster.usableSlices("nonsense").isEmpty)
    }

    /// R4: an id or incarnation that is not a whole number in range skips
    /// its entry; it never traps the app.
    @Test func usableSlicesSkipsNumbersThatAreNotWholeOrInRange() {
        let text = #"[{"sliceId":1e300,"incarnation":11},{"sliceId":2,"incarnation":-1e300},"#
            + #"{"sliceId":1.5,"incarnation":3},{"sliceId":4,"incarnation":2.25},"#
            + #"{"sliceId":5,"incarnation":20,"letter":"F","controllerDeviceId":"desk"}]"#
        #expect(SliceRoster.usableSlices(text) == [
            SliceRoster.UsableSlice(sliceId: 5, incarnation: 20, letter: "F", controllerDeviceId: "desk"),
        ])
    }
}
