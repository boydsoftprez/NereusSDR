// NereusSDR for iOS: the Roundtable flag line, sheet, hold PTT and first-run question against a fake Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, the phone's Roundtable sheet.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Spec 11.2 and 11.3: the flag's Roundtable line, the sheet's places on
/// the band. With `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the sheet's states are written there
/// for comparing with the board's phone pictures.
@Suite("Roundtable sheet", .serialized)
@MainActor
struct RoundtableSheetTests {
    private let platform = TestPlatform()
    private static let room = "lsb-7236400"

    // MARK: The flag line

    private static func summary(members: Int64, state: String = "talking", talkers: [String] = ["K4ABC"])
        -> RoundtableModel.RoomSummary {
        RoundtableModel.RoomSummary(id: room, band: "40m", family: "lsb", hz: 7_236_400, state: state,
                                    talkers: talkers, members: members, messages: 0, last: 0, folded: false,
                                    start: nil, heard: 0, readings: [], lastMessage: nil, ended: nil)
    }

    private static func member(_ ref: String, _ callsign: String, km: Int64? = nil, you: Bool = false,
                               keyed: Bool = false, dbm: Double? = nil) -> RoundtableModel.Member {
        RoundtableModel.Member(ref: ref, callsign: callsign, nth: 1, grid: "EM73", km: km, offsetHz: 0,
                               joining: false, away: false, keyed: keyed, software: "NereusSDR",
                               softwareVersion: "26.10.3", you: you,
                               reading: dbm.map { RoundtableModel.Reading(dbm: $0, t: 1_000, present: true, turn: nil) })
    }

    private static func roomWith(_ members: [RoundtableModel.Member], talker: String?) -> RoundtableModel.Room {
        let now = talker.map { ref in
            [RoundtableModel.Turn(id: 1, kind: "talking", talkers: [.init(ref: ref, callsign: "X", nth: 1)],
                                  start: 0, end: nil, ending: false, names: [], heard: 0, mine: nil)]
        } ?? []
        return RoundtableModel.Room(id: room, band: "40m", family: "lsb", hz: 7_236_400, members: members, now: now,
                                    earlier: [], messages: [])
    }

    @Test("the flag line is hidden without Roundtable or with it off, and says the link's state otherwise")
    func flagLineStates() {
        let summary = Self.summary(members: 9)
        #expect(RoundtableFlagLine.line(available: false, link: .live, summary: summary, room: nil, mentions: 0) == nil)
        #expect(RoundtableFlagLine.line(available: true, link: .off, summary: summary, room: nil, mentions: 0) == nil)
        #expect(RoundtableFlagLine.line(available: true, link: .connecting, summary: summary, room: nil,
                                        mentions: 0) == RoundtableFlagLine(glyph: .waiting,
                                                                           text: "Roundtable \u{00B7} Connecting"))
        #expect(RoundtableFlagLine.line(available: true, link: .reconnecting, summary: summary, room: nil,
                                        mentions: 0)?.text == "Roundtable \u{00B7} Reconnecting")
        #expect(RoundtableFlagLine.line(available: true, link: .offline, summary: summary, room: nil,
                                        mentions: 0) == RoundtableFlagLine(glyph: .offline,
                                                                           text: "Roundtable \u{00B7} Offline"))
        #expect(RoundtableFlagLine.line(available: true, link: .live, summary: nil, room: nil,
                                        mentions: 0)?.text == "Roundtable \u{00B7} Nobody here")
    }

    @Test("the flag line reads members and talker, Doubled, Quiet, @N, and N hear you while you talk")
    func flagLineWords() {
        let talking = RoundtableFlagLine.line(available: true, link: .live, summary: Self.summary(members: 9),
                                              room: nil, mentions: 2)
        #expect(talking == RoundtableFlagLine(glyph: .talking, text: "Roundtable \u{00B7} 9 \u{00B7} K4ABC", mentions: 2))
        #expect(RoundtableFlagLine.line(available: true, link: .hidden, summary: Self.summary(members: 6, state: "doubled"),
                                        room: nil, mentions: 0)?.text == "Roundtable \u{00B7} 6 \u{00B7} Doubled")
        #expect(RoundtableFlagLine.line(available: true, link: .live,
                                        summary: Self.summary(members: 3, state: "quiet", talkers: []),
                                        room: nil, mentions: 0)?.glyph == .quiet)
        // You on the air: the others whose reading of you is present.
        let room = Self.roomWith([Self.member("y", "KG4VCF", you: true, keyed: true),
                                  Self.member("a", "N4XYZ", dbm: -50), Self.member("b", "K4QRS", dbm: -60),
                                  Self.member("c", "W9ABC")], talker: "y")
        #expect(RoundtableFlagLine.line(available: true, link: .live, summary: Self.summary(members: 4), room: room,
                                        mentions: 0) == RoundtableFlagLine(glyph: .you,
                                                                           text: "Roundtable \u{00B7} 2 hear you"))
    }

    @Test("readings drop the talker, and you while keyed, nearest first then by callsign")
    func readingOrder() {
        let room = Self.roomWith([Self.member("t", "K4ABC", km: 10, dbm: -40),
                                  Self.member("q", "K4QRS", km: 593, dbm: -55),
                                  Self.member("n", "N4XYZ", km: 215, dbm: -43),
                                  Self.member("w", "W9ABC", dbm: -60), Self.member("a", "AA5ABC", dbm: -62),
                                  Self.member("y", "KG4VCF", km: 300, you: true, dbm: -61)], talker: "t")
        #expect(RoundtableReadings.rows(room: room, keyed: false).map(\.member.callsign)
                == ["N4XYZ", "KG4VCF", "K4QRS", "AA5ABC", "W9ABC"])
        #expect(RoundtableReadings.rows(room: room, keyed: true).map(\.member.callsign)
                == ["N4XYZ", "K4QRS", "AA5ABC", "W9ABC"])
        #expect(RoundtableReadings.title(room: room, keyed: true) == "READINGS OF YOU")
    }

    @Test("a mention is a shown message, not yours, naming your base callsign as a word")
    func mentions() {
        func message(_ id: Int64, _ from: String, _ text: String, state: String = "shown") -> RoundtableModel.Message {
            RoundtableModel.Message(id: id, ref: nil, callsign: from, nth: 1, t: id, state: state, text: text, dbm: nil)
        }
        #expect(RoundtableText.names("kg4vcf you are S9", callsign: "KG4VCF/P"))
        #expect(!RoundtableText.names("KG4VCFX is not you", callsign: "KG4VCF"))
        let messages = [message(1, "N4XYZ", "hi KG4VCF"), message(2, "KG4VCF", "KG4VCF here"),
                        message(3, "K4QRS", "KG4VCF again"), message(4, "W9ABC", "KG4VCF gone", state: "deleted")]
        #expect(RoundtableText.unreadMentionIds(messages, count: 2, myRef: nil, callsign: "KG4VCF") == [1, 3])
        #expect(RoundtableText.unreadMentionIds(messages, count: 1, myRef: nil, callsign: "KG4VCF") == [3])
    }

    @Test("Send is on only tuned here, live, not away, and within 500 characters and 10 lines")
    func sendRules() {
        #expect(RoundtableSheet.canSend(text: "hi", tunedHere: true, live: true, away: false, sending: false))
        #expect(!RoundtableSheet.canSend(text: "  ", tunedHere: true, live: true, away: false, sending: false))
        #expect(!RoundtableSheet.canSend(text: "hi", tunedHere: false, live: true, away: false, sending: false))
        #expect(!RoundtableSheet.canSend(text: "hi", tunedHere: true, live: false, away: false, sending: false))
        #expect(!RoundtableSheet.canSend(text: "hi", tunedHere: true, live: true, away: true, sending: false))
        #expect(!RoundtableSheet.canSend(text: String(repeating: "a", count: 501), tunedHere: true, live: true,
                                         away: false, sending: false))
        #expect(RoundtableSheet.counter(String(repeating: "a", count: 399)) == nil)
        #expect(RoundtableSheet.counter(String(repeating: "a", count: 400)) == "400 / 500")
        #expect(RoundtableSheet.counter(Array(repeating: "a", count: 11).joined(separator: "\n"))
                == "11 lines, up to 10")
    }

    // MARK: The sheet's places

    @Test("half height starts 48% down the band, full at its top, keyed below the gauges, above the dial strip")
    func sheetPlaces() {
        let half = MainScreen.roundtablePlace(bandHeight: 600, detent: .half, gaugesFoot: nil, dialStrip: false)
        #expect(half.top == 288 && half.height == 312)
        let full = MainScreen.roundtablePlace(bandHeight: 600, detent: .full, gaugesFoot: nil, dialStrip: false)
        #expect(full.top == 0 && full.height == 600)
        let keyed = MainScreen.roundtablePlace(bandHeight: 600, detent: .full, gaugesFoot: 330, dialStrip: false)
        #expect(keyed.top == 338 && keyed.height == 262)
        let strip = MainScreen.roundtablePlace(bandHeight: 600, detent: .half, gaugesFoot: nil, dialStrip: true)
        #expect(strip.top == 288 && strip.height == 600 - 92 - 288)
        #expect(DialLayer.stripHeight == 92 && DialLayer.stripKnobDiameter == 76)
        #expect(MainScreen.keepsStrip(.waterfallKnob) && MainScreen.keepsStrip(.thumbwheel))
        #expect(!MainScreen.keepsStrip(.off) && !MainScreen.keepsStrip(.sheetKnob))
    }

    // MARK: The app that owns the model

    @Test("Roundtable reads as not connected once the app that owns it is freed")
    func notConnectedAfterTheAppIsFreed() async throws {
        let station = try FakeStation(additions: [.roundtable])
        let defaults = try #require(UserDefaults(suiteName: "RoundtableSheetTests-\(UUID().uuidString)"))
        var model: AppModel? = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                                        displaySettings: BandDisplaySettingsStore(defaults: defaults),
                                        platform: platform.platform)
        await model?.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                             transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let rt = try #require(model?.roundtable)
        #expect(await settle { rt.connected && rt.available })
        await model?.disconnect()
        weak var freed = model
        model = nil
        #expect(await settle { freed == nil })
        #expect(await settle { !rt.connected && !rt.available })
    }

    // MARK: Shots

    @Test("shots: the flag line, half and full, keyed, and the dial strip")
    func shots() async throws {
        do {
            let (model, station) = try await shotStation()
            try await station.deliverRoundtable(Self.boardScene())
            #expect(await settle { model.roundtable.room != nil })
            try await shoot("01-band-flag-line", model: model, detent: nil)
            try await shoot("02-sheet-half", model: model, detent: .half)
            try await shoot("03-sheet-full", model: model, detent: .full)
            model.main.transmit.tapPtt()
            #expect(await settle { model.main.transmit.ptt.transmitting })
            try await shoot("04-keyed-sheet-open", model: model, detent: .half)
            model.main.transmit.tapPtt()
            #expect(await settle { model.main.transmit.ptt.state == .idle })
            model.phoneSettings.dialKind = .waterfallKnob
            try await shoot("05-knob-sheet-open", model: model, detent: .half)
            model.phoneSettings.dialKind = .thumbwheel
            model.roundtable.setDraft("copy you 59", for: Self.room)
            model.main.transmit.tapPtt()
            #expect(await settle { model.main.transmit.ptt.transmitting })
            try await shoot("06-thumbwheel-keyed", model: model, detent: .half)
            model.main.transmit.tapPtt()
            #expect(await settle { model.main.transmit.ptt.state == .idle })
            model.phoneSettings.dialKind = .off
            await model.disconnect()
        }
    }

    // MARK: Helpers

    /// Slice A in a 40 m room of five: K4ABC on the air, readings of it
    /// near and far, you, an earlier turn and messages, the newest naming you.
    static func boardScene() -> FakeStation.RoundtableScene {
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        func member(_ ref: String, _ callsign: String, km: Double?, offset: Double, seq: Int64,
                    you: Bool = false) -> LinkMessage.RecordBatch.Record {
            .init(id: "member:\(ref)", fields: [
                "record": .string("member"), "ref": .string(ref), "callsign": .string(callsign), "nth": .number(1),
                "grid": .string("EM73"), "km": km.map(LinkJSON.number) ?? .null, "offsetHz": .number(offset),
                "joining": .bool(false), "away": .bool(false), "keyed": .bool(false),
                "software": .string("NereusSDR"), "softwareVersion": .string("26.10.3"), "you": .bool(you),
                "seq": .number(Double(seq)),
            ])
        }
        func reading(_ ref: String, _ dbm: Double, ageMs: Int64) -> LinkMessage.RecordBatch.Record {
            FakeStation.roundtableReading(ref: ref, dbm: dbm, t: now - ageMs, turn: 12)
        }
        let records: [LinkMessage.RecordBatch.Record] = [
            member("k/1", "K4ABC", km: nil, offset: 0, seq: 0),
            member("n/2", "N4XYZ", km: 215, offset: 0, seq: 1), reading("n/2", -43, ageMs: 3_000),
            member("q/3", "K4QRS", km: 593, offset: -20, seq: 2), reading("q/3", -55, ageMs: 1_000),
            member("w/5", "W9ABC", km: 1_210, offset: 40, seq: 3), reading("w/5", -64, ageMs: 25_000),
            member("y/4", "KG4VCF", km: 340, offset: 0, seq: 4, you: true), reading("y/4", -61, ageMs: 1_000),
            .init(id: "turn:12", fields: [
                "record": .string("turn"), "id": .number(12), "kind": .string("talking"),
                "talkers": .array([.object(["ref": .string("k/1"), "callsign": .string("K4ABC"), "nth": .number(1)])]),
                "start": .number(Double(now - 78_000)), "end": .null, "ending": .bool(false), "heard": .number(3),
                "mine": .null,
            ]),
            FakeStation.roundtableTurn(id: 11, start: now - 200_000, end: now - 140_000),
            FakeStation.roundtableName(turn: 11, name: "N0CALL", count: 2),
            FakeStation.roundtableMessage(id: 21, t: now - 90_000, callsign: "N4XYZ", text: "good copy all"),
            FakeStation.roundtableMessage(id: 22, t: now - 30_000, callsign: "K4QRS", text: "KG4VCF you are S9 here"),
        ]
        return FakeStation.RoundtableScene(
            rooms: [FakeStation.roundtableSummary(id: room, members: 5, talkers: ["K4ABC"])],
            roomRecords: [room: records], linkSince: now - 600_000, mentionCounts: [room: 1], sliceRooms: [0: room])
    }

    private func connected(additions: FakeStation.Additions = [.remoteTx]) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "RoundtableSheetTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await settle { model.main.transmit.permitted && model.connection == .connected })
        return (model, station)
    }

    /// A fake Core with Roundtable, the main screen's two slices and a band to draw them on.
    private func shotStation() async throws -> (AppModel, FakeStation) {
        let (model, station) = try await connected(additions: [.remoteTx, .roundtable])
        try await MainScreenShotTests.fill(station)
        #expect(await settle { model.main.slices.entries.count == 2 })
        model.main.band.endpointId = 1
        model.main.band.receive(.context(try #require(BandFlagShotTests.context())))
        return (model, station)
    }

    private static func verbs(_ station: FakeStation, _ verb: String) -> Int {
        station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == verb }.count
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    private func host<Content: View>(_ content: Content,
                                     size: CGSize = CGSize(width: 402, height: 874)) throws -> (UIWindow, UIView) {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let host = ShotHosting.host(content.preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        return (window, host.view)
    }

    private func shoot(_ name: String, model: AppModel, detent: RoundtableSheet.Detent?) async throws {
        let size = CGSize(width: 402, height: 874)
        let (window, _) = try host(RoundtableShotRoot(model: model, detent: detent), size: size)
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        try await Task.sleep(for: .milliseconds(300))
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("roundtable-\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The main screen with the Roundtable sheet open.
private struct SheetHost: View {
    @ObservedObject var model: AppModel
    let detent: RoundtableSheet.Detent

    var body: some View {
        MainScreen(app: model, main: model.main, roundtableSheet: detent)
    }
}

/// The app's root as `RootView` lays it out, with the Roundtable sheet open or not.
private struct RoundtableShotRoot: View {
    @ObservedObject var model: AppModel
    let detent: RoundtableSheet.Detent?

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, roundtableSheet: detent)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
