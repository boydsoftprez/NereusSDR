// NereusSDR for iOS: the RADE row on a slice's flag: its words, where they come from, and the flag it grows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-11, D9, spec section 5.1 item 4: the RADE row reads as the
/// board draws it and as the desktop composes it. The readings here are
/// synthetic; the lock state and offset are built in the test because the
/// Core does not send them.
@Suite("RADE row words")
struct RadeRowWordsTests {
    static func reception(_ call: String, snr: Double?, synced: Bool?, offset: Double?) -> RadeReception {
        RadeReception(callsign: call, snrDb: snr, synced: synced, offsetHz: offset)
    }

    @Test("locked on with good copy: callsign, green dot, whole dB and the offset")
    func receiving() {
        let row = Self.reception("K1ABC", snr: 12, synced: true, offset: 38)
        #expect(row.row == .reading(prefix: "K1ABC", dot: .good, value: "12dB", offset: "+38Hz"))
        #expect(row.text == "K1ABC ● 12dB +38Hz")
        #expect(row.spokenText == "RADE reception: K1ABC, locked on, signal to noise 12 dB, 38 hertz high")
    }

    @Test("no callsign decoded: the row says RADE, and a low offset reads low")
    func noCallsign() {
        let row = Self.reception("", snr: 11, synced: true, offset: -21)
        #expect(row.text == "RADE ● 11dB -21Hz")
        #expect(row.spokenText
            == "RADE reception: no callsign decoded, locked on, signal to noise 11 dB, 21 hertz low")
    }

    @Test("below 5 dB the dot is yellow and the words say marginal")
    func weak() {
        let row = Self.reception("K1ABC", snr: 3, synced: true, offset: 52)
        #expect(row.row == .reading(prefix: "K1ABC", dot: .marginal, value: "3dB", offset: "+52Hz"))
        #expect(row.spokenText
            == "RADE reception: K1ABC, locked on, signal to noise 3 dB, marginal, 52 hertz high")
        // 4.9 dB cuts to 4 and is marginal; exactly 5 is good.
        #expect(Self.reception("K1ABC", snr: 4.9, synced: true, offset: 0).row
            == .reading(prefix: "K1ABC", dot: .marginal, value: "4dB", offset: "+0Hz"))
        #expect(Self.reception("K1ABC", snr: 5, synced: true, offset: 0).row
            == .reading(prefix: "K1ABC", dot: .good, value: "5dB", offset: "+0Hz"))
    }

    @Test("not locked on: a hollow dot and no reading, with or without a callsign")
    func notLocked() {
        #expect(Self.reception("", snr: nil, synced: false, offset: 0).text == "RADE ○ ---")
        let after = Self.reception("K1ABC", snr: 12, synced: false, offset: 38)
        #expect(after.text == "K1ABC ○ ---")
        #expect(after.row == .reading(prefix: "K1ABC", dot: .hollow, value: "---", offset: nil))
        #expect(after.spokenText == "RADE reception: K1ABC, not locked on, no signal to noise reading")
    }

    // The Core's note: a filled dot only while synced with a number;
    // otherwise the hollow dot and three hyphens.
    @Test("synced with no reading: a hollow dot and no offset, as the desktop draws it")
    func syncedWithoutReading() {
        let row = Self.reception("K1ABC", snr: nil, synced: true, offset: 38)
        #expect(row.row == .reading(prefix: "K1ABC", dot: .hollow, value: "---", offset: nil))
        #expect(row.text == "K1ABC ○ ---")
        #expect(row.spokenText == "RADE reception: K1ABC, locked on, no signal to noise reading")
        #expect(Self.reception("K1ABC", snr: .nan, synced: true, offset: 38).text == "K1ABC ○ ---")
    }

    @Test("whole numbers are cut toward zero, as the desktop prints them")
    func truncation() {
        #expect(Self.reception("K1ABC", snr: 12.9, synced: true, offset: -38.7).text == "K1ABC ● 12dB -38Hz")
        #expect(Self.reception("K1ABC", snr: -3.7, synced: true, offset: 0).text == "K1ABC ● -3dB +0Hz")
        // With sync sent but no offset, the row leaves the offset off.
        #expect(Self.reception("K1ABC", snr: 12, synced: true, offset: nil).text == "K1ABC ● 12dB")
    }

    // The desktop takes the sign from the value before it is cut: "+" when
    // it is zero or above, nothing otherwise, then the cut whole hertz.
    @Test("the offset's sign comes from the value before it is cut", arguments: [
        (-0.4, "0Hz", "0 hertz low"),
        (-38.7, "-38Hz", "38 hertz low"),
        (0.4, "+0Hz", "0 hertz high"),
        (38.7, "+38Hz", "38 hertz high"),
    ])
    func offsetSign(hz: Double, shown: String, spoken: String) {
        let row = Self.reception("K1ABC", snr: 12, synced: true, offset: hz)
        #expect(row.text == "K1ABC ● 12dB " + shown)
        #expect(row.spokenText.hasSuffix(", " + spoken))
    }

    // The Core's note: an older Core still shows the callsign; the SNR part
    // is disabled with the reason.
    @Test("a Core that does not send sync greys the row: the callsign, then the reason")
    func olderCore() {
        #expect(RadeReception.olderCoreReason == "This Core does not send RADE sync. Updating the Core may help.")
        let row = Self.reception("K1ABC", snr: 12, synced: nil, offset: nil)
        #expect(row.row == .olderCore)
        #expect(row.text == "K1ABC This Core does not send RADE sync. Updating the Core may help.")
        #expect(row.spokenText
            == "RADE reception: K1ABC. This Core does not send RADE sync. Updating the Core may help.")
        let none = Self.reception("", snr: nil, synced: nil, offset: nil)
        #expect(none.text == "RADE This Core does not send RADE sync. Updating the Core may help.")
        #expect(none.spokenText
            == "RADE reception: no callsign decoded. This Core does not send RADE sync. Updating the Core may help.")
    }

    // The desktop's flag (VfoWidget.cpp setRadeReason, setRadeSynced): while
    // the Core's reason is set the row reads "<prefix> ○ off", whatever the
    // sync and reading, and the reason is the row's tooltip; the phone puts
    // it under the row.
    @Test("the Core's reason: the prefix, a hollow dot and off, the reason under them")
    func reason() {
        let words = "RADE could not start on slice A: the RADE model file was not found."
        let row = RadeReception(callsign: "", snrDb: 12, synced: true, offsetHz: 38, reason: words)
        #expect(row.row == .off(prefix: "RADE", reason: words))
        #expect(row.wraps)
        #expect(row.text == "RADE ○ off " + words)
        #expect(row.spokenText == "RADE reception: no callsign decoded, off. " + words)
        let called = RadeReception(callsign: "K1ABC", snrDb: nil, synced: false, offsetHz: nil, reason: words)
        #expect(called.row == .off(prefix: "K1ABC", reason: words))
        #expect(called.spokenText == "RADE reception: K1ABC, off. " + words)
        // An empty reason is none: the row reads as before.
        let decoding = RadeReception(callsign: "K1ABC", snrDb: 12, synced: true, offsetHz: 38, reason: "")
        #expect(decoding.text == "K1ABC ● 12dB +38Hz")
        #expect(!decoding.wraps)
        #expect(Self.reception("K1ABC", snr: 12, synced: nil, offset: nil).wraps)
    }

    @Test("the row's words are plain")
    func plainWords() {
        let all = [RadeReception.olderCoreReason,
                   Self.reception("", snr: nil, synced: false, offset: nil).spokenText,
                   Self.reception("K1ABC", snr: 3, synced: true, offset: -5).spokenText]
        for words in all {
            #expect(!words.contains("\u{2014}"))
            #expect(!words.lowercased().split(separator: " ").contains("yet"))
        }
    }
}

/// The row's readings come from the slice's mirror, live, and only in RADE.
@Suite("RADE row from the mirror")
@MainActor
struct RadeRowMirrorTests {
    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    static func mode(_ label: String, in catalog: StationCatalog) -> Int64? {
        catalog.modes.first { $0.label == label }.map { Int64($0.id) }
    }

    static func slice(_ index: Int, mode: Int64, snr: Double?, callsign: String?,
                      synced: Bool? = nil, offsetHz: Double? = nil) -> LinkMessage {
        var properties = [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(7_236_400)),
            LinkMessage.PropertyEntry(ordinal: 2, name: "dspMode", value: .enumeration(mode)),
            LinkMessage.PropertyEntry(ordinal: 11, name: "active", value: .bool(index == 0)),
            LinkMessage.PropertyEntry(ordinal: 13, name: "sliceIndex", value: .i64(Int64(index))),
        ]
        if let snr {
            properties.append(LinkMessage.PropertyEntry(ordinal: 142, name: "snrDb", value: .f64(snr)))
        }
        if let callsign {
            properties.append(LinkMessage.PropertyEntry(ordinal: 143, name: "lastRadeRxCallsign",
                                                        value: .utf8(callsign)))
        }
        // The Core's rade-flag note: radeSynced (bool, 151) and
        // radeFreqOffsetHz (f64, 152), read by name.
        if let synced {
            properties.append(LinkMessage.PropertyEntry(ordinal: 151, name: "radeSynced", value: .bool(synced)))
        }
        if let offsetHz {
            properties.append(LinkMessage.PropertyEntry(ordinal: 152, name: "radeFreqOffsetHz", value: .f64(offsetHz)))
        }
        return .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel",
                                                      properties: properties))
    }

    static func delta(_ index: Int, _ entries: [LinkMessage.PropertyEntry]) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "slice:\(index)", properties: entries))
    }

    /// A Core at `minor` that sends `radeStatusVersion` at `version`, as its hello and capabilities.
    static func core(_ store: MirrorStore, minor: UInt16 = 11, radeStatus version: Int64?,
                     radeReason reasonVersion: Int64? = nil) {
        store.apply(.hello(LinkMessage.Hello(major: 1, minor: minor, settingsSchema: 0, peer: "nereusd")))
        var entries: [LinkMessage.PropertyEntry] = [.init(ordinal: 0, name: "remoteMediaVersion", value: .i64(1))]
        if let version {
            entries.append(.init(ordinal: 0, name: "radeStatusVersion", value: .i64(version)))
        }
        if let reasonVersion {
            entries.append(.init(ordinal: 0, name: "radeReasonVersion", value: .i64(reasonVersion)))
        }
        store.apply(.capabilities(LinkMessage.Capabilities(properties: entries)))
    }

    @Test("a Core that sends radeStatusVersion 1 at minor 11 lights the dot, SNR and offset")
    func gatedCore() async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeU = try #require(Self.mode("RADE-U", in: catalog))
        let store = MirrorStore(send: { _ in })
        Self.core(store, radeStatus: 1)
        store.apply(Self.slice(0, mode: radeU, snr: 12.9, callsign: "K1ABC", synced: true, offsetHz: -12.4))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade != nil })
        #expect(model.entries.first?.rade == RadeReception(callsign: "K1ABC", snrDb: 12.9, synced: true, offsetHz: -12.4))
        #expect(model.entries.first?.rade?.text == "K1ABC ● 12dB -12Hz")

        // Loss of sync: the Core keeps snrDb and the offset; the row stops showing them.
        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 151, name: "radeSynced", value: .bool(false))]))
        #expect(await settle { model.entries.first?.rade?.text == "K1ABC ○ ---" })
        #expect(model.entries.first?.rade?.snrDb == 12.9)
        #expect(model.entries.first?.rade?.offsetHz == -12.4)

        // Sync and the offset in one delta, as the Core sends them together.
        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 151, name: "radeSynced", value: .bool(true)),
                                   LinkMessage.PropertyEntry(ordinal: 152, name: "radeFreqOffsetHz", value: .f64(38.7))]))
        #expect(await settle { model.entries.first?.rade?.text == "K1ABC ● 12dB +38Hz" })
    }

    @Test("a gated Core's first snapshot: not synced, offset 0, reads the hollow dot")
    func gatedCoreSnapshot() async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeL = try #require(Self.mode("RADE-L", in: catalog))
        let store = MirrorStore(send: { _ in })
        Self.core(store, radeStatus: 1)
        store.apply(Self.slice(0, mode: radeL, snr: nil, callsign: "", synced: false, offsetHz: 0))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade != nil })
        #expect(model.entries.first?.rade == RadeReception(callsign: "", snrDb: nil, synced: false, offsetHz: 0))
        #expect(model.entries.first?.rade?.text == "RADE ○ ---")
    }

    @Test("the gate arriving after the slice redraws the row")
    func gateArrivesLater() async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeU = try #require(Self.mode("RADE-U", in: catalog))
        let store = MirrorStore(send: { _ in })
        Self.core(store, radeStatus: nil)
        store.apply(Self.slice(0, mode: radeU, snr: 9, callsign: "W1XYZ", synced: true, offsetHz: 5))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade?.row == .olderCore })
        Self.core(store, radeStatus: 1)
        #expect(await settle { model.entries.first?.rade?.text == "W1XYZ ● 9dB +5Hz" })
    }

    @Test("without the gate the row is the older-Core row, even if sync were sent", arguments: [
        (UInt16(11), Int64?.none), (UInt16(10), Int64?.some(1)), (UInt16(11), Int64?.some(0)),
    ])
    func olderCoreGate(minor: UInt16, version: Int64?) async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeU = try #require(Self.mode("RADE-U", in: catalog))
        let store = MirrorStore(send: { _ in })
        Self.core(store, minor: minor, radeStatus: version)
        store.apply(Self.slice(0, mode: radeU, snr: 12, callsign: "K1ABC", synced: true, offsetHz: 38))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade != nil })
        #expect(model.entries.first?.rade == RadeReception(callsign: "K1ABC", snrDb: 12, synced: nil, offsetHz: nil))
        #expect(model.entries.first?.rade?.row == .olderCore)
    }

    @Test("an older Core's RADE slice carries its SNR and callsign, greyed with the reason")
    func readsTheRow() async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeL = try #require(Self.mode("RADE-L", in: catalog))
        let store = MirrorStore(send: { _ in })
        store.apply(Self.slice(0, mode: radeL, snr: 12.4, callsign: "K1ABC"))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade != nil })
        let rade = try #require(model.entries.first?.rade)
        #expect(rade == RadeReception(callsign: "K1ABC", snrDb: 12.4, synced: nil, offsetHz: nil))
        // With no lock state from the Core, the row is the older-Core row.
        #expect(rade.row == .olderCore)
    }

    @Test("the row follows every change the Core sends; a NaN is no reading")
    func followsTheMirror() async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeU = try #require(Self.mode("RADE-U", in: catalog))
        let store = MirrorStore(send: { _ in })
        store.apply(Self.slice(0, mode: radeU, snr: nil, callsign: nil))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade?.callsign == "" })
        #expect(model.entries.first?.rade?.snrDb == nil)

        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 142, name: "snrDb", value: .f64(7.5))]))
        #expect(await settle { model.entries.first?.rade?.snrDb == 7.5 })
        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 143, name: "lastRadeRxCallsign",
                                                             value: .utf8("W1XYZ"))]))
        #expect(await settle { model.entries.first?.rade?.callsign == "W1XYZ" })
        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 142, name: "snrDb", value: .f64(.nan))]))
        #expect(await settle { model.entries.first?.rade?.snrDb == nil })
        // The Core clears the callsign; the phone shows what it sends.
        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 143, name: "lastRadeRxCallsign",
                                                             value: .utf8(""))]))
        #expect(await settle { model.entries.first?.rade?.callsign == "" })
    }

    /// The Core's `radeReason` (utf8, ordinal 153), read by name.
    static func reason(_ words: String) -> LinkMessage.PropertyEntry {
        LinkMessage.PropertyEntry(ordinal: 153, name: "radeReason", value: .utf8(words))
    }

    nonisolated static let modelFileWords = "RADE could not start on slice A: the RADE model file was not found."

    @Test("a Core that sends radeReasonVersion 1: the reason turns the row off, and clearing it brings the reading back")
    func reasonFromTheCore() async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeU = try #require(Self.mode("RADE-U", in: catalog))
        let store = MirrorStore(send: { _ in })
        Self.core(store, radeStatus: 1, radeReason: 1)
        store.apply(Self.slice(0, mode: radeU, snr: 12, callsign: "K1ABC", synced: true, offsetHz: 38))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade?.text == "K1ABC ● 12dB +38Hz" })

        store.apply(Self.delta(0, [Self.reason(Self.modelFileWords)]))
        #expect(await settle { model.entries.first?.rade?.row == .off(prefix: "K1ABC", reason: Self.modelFileWords) })
        #expect(model.entries.first?.rade?.text == "K1ABC ○ off " + Self.modelFileWords)

        // The Core clears the reason when the decoder starts.
        store.apply(Self.delta(0, [Self.reason("")]))
        #expect(await settle { model.entries.first?.rade?.text == "K1ABC ● 12dB +38Hz" })
        #expect(model.entries.first?.rade?.reason == nil)
    }

    @Test("a Core without radeReasonVersion: the row reads as it did, whatever the slice carries", arguments: [
        Int64?.none, Int64?.some(0),
    ])
    func reasonWithoutTheCapability(version: Int64?) async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeU = try #require(Self.mode("RADE-U", in: catalog))
        let store = MirrorStore(send: { _ in })
        Self.core(store, radeStatus: 1, radeReason: version)
        store.apply(Self.slice(0, mode: radeU, snr: 12, callsign: "K1ABC", synced: true, offsetHz: 38))
        store.apply(Self.delta(0, [Self.reason(Self.modelFileWords)]))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade != nil })
        #expect(model.entries.first?.rade == RadeReception(callsign: "K1ABC", snrDb: 12, synced: true, offsetHz: 38))
        #expect(model.entries.first?.rade?.text == "K1ABC ● 12dB +38Hz")
    }

    @Test("the reason's capability arriving after the slice redraws the row; leaving RADE drops it")
    func reasonCapabilityArrivesLater() async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeL = try #require(Self.mode("RADE-L", in: catalog))
        let lsb = try #require(Self.mode("LSB", in: catalog))
        let store = MirrorStore(send: { _ in })
        Self.core(store, radeStatus: 1)
        store.apply(Self.slice(0, mode: radeL, snr: nil, callsign: "", synced: false, offsetHz: 0))
        store.apply(Self.delta(0, [Self.reason(Self.modelFileWords)]))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.rade?.text == "RADE ○ ---" })
        Self.core(store, radeStatus: 1, radeReason: 1)
        #expect(await settle { model.entries.first?.rade?.text == "RADE ○ off " + Self.modelFileWords })
        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 2, name: "dspMode", value: .enumeration(lsb))]))
        #expect(await settle { model.entries.first?.rade == nil })
    }

    @Test("outside RADE the flag carries no row")
    func onlyInRade() async throws {
        let catalog = try #require(BandFlagShotTests.catalogue())
        let radeL = try #require(Self.mode("RADE-L", in: catalog))
        let lsb = try #require(Self.mode("LSB", in: catalog))
        let store = MirrorStore(send: { _ in })
        store.apply(Self.slice(0, mode: lsb, snr: 12, callsign: "K1ABC"))
        let model = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        #expect(await settle { model.entries.first?.modeLabel == "LSB" })
        #expect(model.entries.first?.rade == nil)
        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 2, name: "dspMode", value: .enumeration(radeL))]))
        #expect(await settle { model.entries.first?.rade != nil })
        #expect(model.entries.first?.modeLabel == "RADE-L")
        store.apply(Self.delta(0, [LinkMessage.PropertyEntry(ordinal: 2, name: "dspMode", value: .enumeration(lsb))]))
        #expect(await settle { model.entries.first?.rade == nil })
    }
}

/// The RADE reason end to end: the real app signs in to a fake Core,
/// declares `radeReason` 1 in its hello, and a Core that answers with
/// `radeReasonVersion` 1 turns a RADE slice's row off with its reason.
@Suite("RADE reason against a fake Core", .serialized)
@MainActor
struct RadeReasonFakeCoreTests {
    private let platform = TestPlatform()

    @Test("the hello declares radeReason 1, and the Core's reason reaches the flag's row and clears")
    func reasonFromAFakeCore() async throws {
        let defaults = try #require(UserDefaults(suiteName: "RadeReasonFakeCoreTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await ShotWait.until { model.connection == .connected })
        let hello = try #require(station.messages.first { if case .hello = $0 { return true } else { return false } })
        guard case .hello(let ours) = hello else {
            return
        }
        #expect(ours.features?["radeReason"] == 1)

        // The suite's catalogue (its RADE modes), and a RADE slice that
        // carries a reason, from a Core before the reason's capability: the
        // phone does not read it.
        let json = try #require(ModesTabBindingTests.catalogueJSON(ModesTabBindingTests.anan))
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await ShotWait.until { model.main.catalogFeed.catalog != nil })
        let catalog = try #require(model.main.catalogFeed.catalog)
        let radeU = try #require(RadeRowMirrorTests.mode("RADE-U", in: catalog))
        let slices = model.main.slices
        let id = 0
        await station.deliver(RadeRowMirrorTests.slice(id, mode: radeU, snr: nil, callsign: nil))
        await station.deliver(RadeRowMirrorTests.delta(id, [
            RadeRowMirrorTests.reason(RadeRowMirrorTests.modelFileWords),
        ]))
        #expect(await ShotWait.until { slices.entries.first { $0.id == id }?.rade != nil })
        #expect(slices.entries.first { $0.id == id }?.rade?.reason == nil)

        // The Core's capabilities again with the RADE status and reason versions.
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: Self.capabilities(
            model.mirror.capabilities, adding: ["radeStatusVersion": 1, "radeReasonVersion": 1]))))
        #expect(await ShotWait.until {
            slices.entries.first { $0.id == id }?.rade?.row
                == .off(prefix: "RADE", reason: RadeRowMirrorTests.modelFileWords)
        })

        // The decoder starts: the Core clears the reason and the row reads its sync again.
        await station.deliver(RadeRowMirrorTests.delta(id, [
            RadeRowMirrorTests.reason(""),
            LinkMessage.PropertyEntry(ordinal: 151, name: "radeSynced", value: .bool(false)),
        ]))
        #expect(await ShotWait.until { slices.entries.first { $0.id == id }?.rade?.text == "RADE ○ ---" })
        await model.disconnect()
    }

    /// The mirror's capabilities as a whole new set, with `adding` set.
    static func capabilities(_ held: [String: MirrorValue], adding: [String: Int64]) -> [LinkMessage.PropertyEntry] {
        var set = held
        for (name, version) in adding {
            set[name] = .int(version)
        }
        return set.keys.sorted().compactMap { name in
            let value: LinkMessage.PropertyValue
            switch set[name] {
            case .bool(let flag)?:
                value = .bool(flag)
            case .int(let whole)?:
                value = .i64(whole)
            case .double(let number)?:
                value = .f64(number)
            case .text(let words)?:
                value = .utf8(words)
            case .enumeration(let raw)?:
                value = .enumeration(raw)
            case nil:
                return nil
            }
            return LinkMessage.PropertyEntry(ordinal: 0, name: name, value: value)
        }
    }
}
