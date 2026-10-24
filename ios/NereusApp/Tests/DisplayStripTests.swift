// NereusSDR for iOS: Setup > Display's "Settings for" strip, its words, the router's pan, each page's reset and copy, and where the older "On this phone" rows went
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-60, DC11, DC12 (spec sections 7.1 to 7.4): on a Core whose Display
/// description reaches version 29 Setup > Display is the desktop's eight
/// pages, each of the five picture pages topped by the "Settings for"
/// strip; every row of the older "On this phone" page has a place on them;
/// the strip's words are the desktop's; the router keeps the pan chosen
/// across Display's pages; Reset this pan and Use Pan N's on all pans act
/// on one page's rows for one pan; a link from the band opens the page
/// cold, with its pan chosen.
@Suite("Display's settings strip", .serialized)
@MainActor
struct DisplayStripTests {
    typealias Described = SetupDescribedPagesTests

    static let pan2 = PanIdentity(number: 2)

    // MARK: The strip's words

    @Test("the strip's words for All pans, a pan following, a pan with its own values and a pan not open")
    func words() {
        #expect(DisplayStripText.title == "Settings for")
        #expect(DisplayStripText.allPans == "All pans")
        #expect(DisplayStripText.reset == "Reset this pan")
        #expect(DisplayStripText.pickAPan == "Pick a pan to reset it.")
        #expect(DisplayStripText.iPhoneOnePan == "An iPhone shows one pan.")
        // The pan buttons: the name, a dot with own values, and the second line.
        #expect(DisplayStripText.panTitle(.first, ownCount: 0) == "Pan 1")
        #expect(DisplayStripText.panTitle(Self.pan2, ownCount: 3) == "Pan 2 \u{2022}")
        #expect(DisplayStripText.subtitle(open: true, letters: ["A", "B"]) == "Slice A, B")
        #expect(DisplayStripText.subtitle(open: true, letters: ["A"]) == "Slice A")
        #expect(DisplayStripText.subtitle(open: true, letters: []) == "open")
        #expect(DisplayStripText.subtitle(open: false, letters: []) == "not open")
        // The notes, the desktop's words.
        #expect(DisplayStripText.note(scope: .all, open: true, ownCount: 0) == "A change here goes to every pan.")
        #expect(DisplayStripText.note(scope: .pan(.first), open: true, ownCount: 0)
                == "Pan 1 follows All pans in every row on this page.")
        #expect(DisplayStripText.note(scope: .pan(.first), open: true, ownCount: 1)
                == "Pan 1 has its own value in 1 row on this page, marked with a dot.")
        #expect(DisplayStripText.note(scope: .pan(.first), open: true, ownCount: 4)
                == "Pan 1 has its own value in 4 rows on this page, marked with a dot.")
        #expect(DisplayStripText.note(scope: .pan(Self.pan2), open: false, ownCount: 0)
                == "Pan 2 is not open now. What you set here is kept and used when you open it. "
                + "Pan 2 follows All pans in every row on this page.")
        #expect(DisplayStripText.note(scope: .pan(Self.pan2), open: false, ownCount: 2)
                == "Pan 2 is not open now. What you set here is kept and used when you open it. "
                + "Pan 2 has its own value in 2 rows on this page, marked with a dot.")
        #expect(DisplayStripText.copy(.first) == "Use Pan 1's on all pans")
        #expect(DisplayStripText.copy(Self.pan2) == "Use Pan 2's on all pans")
        // On an iPhone every pan after Pan 1 is greyed; on an iPad none is.
        #expect(!DisplaySettingsStrip.greyed(.first, isPhone: true))
        #expect(DisplaySettingsStrip.greyed(Self.pan2, isPhone: true))
        #expect(!DisplaySettingsStrip.greyed(Self.pan2, isPhone: false))
    }

    // MARK: The router's pan

    @Test("the router opens Display's pages on All pans, keeps a pan across them, and drops it on leaving Display")
    func routerScope() {
        let router = SetupRouter()
        #expect(router.displayScope == .all)
        // From the Setup list: All pans.
        #expect(router.open(describedPage: "display.levels"))
        #expect(router.displayScope == .all)
        // A link with a pan opens its page with the pan chosen.
        #expect(router.open(describedPage: "display.spectrumDefaults", scope: .pan(Self.pan2)))
        #expect(router.path == [.category("Display"), .described(category: "display", page: "display.spectrumDefaults")])
        #expect(router.displayScope == .pan(Self.pan2))
        // Moving between Display's pages keeps it.
        router.path = [.category("Display")]
        #expect(router.displayScope == .pan(Self.pan2))
        #expect(router.open(describedPage: "display.threeD"))
        #expect(router.displayScope == .pan(Self.pan2))
        router.open(.displayOnThisPhone)
        #expect(router.displayScope == .pan(Self.pan2))
        // A page no category holds opens nothing and leaves the pan.
        #expect(!router.open(describedPage: "nowhere", scope: .all))
        #expect(router.displayScope == .pan(Self.pan2))
        // Back to the list, or into another category: All pans next time.
        router.path = []
        #expect(router.displayScope == .all)
        #expect(router.open(describedPage: "display.levels", scope: .pan(.first)))
        #expect(router.open(describedPage: "dsp.agcAlc"))
        #expect(router.displayScope == .all)
        #expect(SetupRouter.inDisplay([.category("Display"), .page(.displayOnThisPhone)]))
        #expect(!SetupRouter.inDisplay([.category("DSP"), .described(category: "dsp", page: "dsp.agcAlc")]))
    }

    // MARK: Where the older page's rows are

    static func resource(_ name: String) throws -> SetupDescription {
        let folder = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("resources/setup")
        return try SetupDescription.parse(json: String(contentsOf: folder.appendingPathComponent(name), encoding: .utf8))
    }

    /// Every identifier the older "On this phone" page draws, read from its source.
    static func onThisPhoneIds() throws -> Set<String> {
        let folder = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .appendingPathComponent("Setup")
        let files = try FileManager.default.contentsOfDirectory(atPath: folder.path)
            .filter { $0.hasPrefix("DisplayOnThisPhonePage") && $0.hasSuffix(".swift") }
        #expect(files.count == 4)
        let pattern = try NSRegularExpression(pattern: #"(?:\bid: |accessibilityIdentifier\()"([A-Za-z0-9_.]+)""#)
        var ids = Set<String>()
        for file in files {
            let text = try String(contentsOf: folder.appendingPathComponent(file), encoding: .utf8)
            for match in pattern.matches(in: text, range: NSRange(text.startIndex..., in: text)) {
                if let range = Range(match.range(at: 1), in: text) {
                    ids.insert(String(text[range]))
                }
            }
        }
        return ids
    }

    /// The key a described control is bound to, a setting's or this phone's.
    static func key(_ control: SetupDescription.Control) -> String? {
        switch control.binding {
        case .setting(let key)?, .phone(let key)?: key
        default: nil
        }
    }

    @Test("every row of the older On this phone page is on its version 29 page and group, or in Appearance")
    func everyOldRowIsReachable() throws {
        let display = try Self.resource("display.json")
        let appearance = try Self.resource("appearance.json")
        #expect(display.version >= SetupTree.displayPagesVersion)
        let ids = try Self.onThisPhoneIds()
        #expect(ids.count > 100)
        #expect(ids == Set(DisplayPhoneGroups.onThisPhonePlaces.keys),
                "drawn but not placed: \(ids.subtracting(DisplayPhoneGroups.onThisPhonePlaces.keys).sorted()); placed but not drawn: \(Set(DisplayPhoneGroups.onThisPhonePlaces.keys).subtracting(ids).sorted())")
        func section(_ page: String, _ title: String) -> SetupDescription.Section? {
            display.pages.first { $0.id == page }?.sections.first { $0.title == title }
        }
        for (id, place) in DisplayPhoneGroups.onThisPhonePlaces.sorted(by: { $0.key < $1.key }) {
            switch place {
            case .described(let page, let group, let key):
                let row = section(page, group)?.controls.first { Self.key($0) == key }
                #expect(row != nil, "\(id): no \(key) in \(page) \(group)")
                #expect(!DisplayPhoneGroups.leftOff.contains(key), "\(id): \(key) is left off")
            case .phone(let page, let group, let row):
                let found = DisplayPhoneGroups.groups(on: page).first { $0.title == group }
                #expect(found?.rows.contains(row) == true, "\(id): no \(row) in this phone's \(group) on \(page)")
                switch found?.placement {
                case .before(let anchor)?, .after(let anchor)?, .into(let anchor)?:
                    let anchored = section(page, anchor)
                        ?? appearance.pages.first { $0.id == page }?.sections.first { $0.title == anchor }
                    #expect(anchored != nil, "\(id): \(page) has no \(anchor) group")
                case .end?, nil:
                    break
                }
            case .appearance(let group, let key):
                let row = appearance.pages.flatMap(\.sections).filter { $0.title == group }.flatMap(\.controls)
                    .first { Self.key($0) == key }
                #expect(row != nil, "\(id): no \(key) in Appearance's \(group)")
            case .notARow:
                break
            case .noDesktopRow:
                Issue.record("\(id) has no place")
            }
        }
        // Spec 7.2's named places.
        let places = DisplayPhoneGroups.onThisPhonePlaces
        #expect(places["setupSpectrumHeight"] == .phone(page: "display.spectrumDefaults", group: "Layout",
                                                        row: "setupSpectrumHeight"))
        #expect(DisplayPhoneGroups.groups(on: "display.spectrumDefaults").first?.tag == .thisPhone)
        #expect(places["scaleTop"] == .described(page: "display.levels", group: "Spectrum scale", key: "DisplayGridMax"))
        #expect(places["scaleBottom"] == .described(page: "display.levels", group: "Spectrum scale",
                                                    key: "DisplayGridMin"))
        #expect(places["setupCtun"] == .phone(page: "display.threeD", group: "On this phone", row: "setupCtun"))
        #expect(places["extendedView"] == .phone(page: "display.threeD", group: "On this phone", row: "extendedView"))
        #expect(places["setupDuplex"] == .phone(page: "display.txDisplay", group: "Transmit scale", row: "setupDuplex"))
        #expect(places["peakValueColour"] == .phone(page: "appearance.colorsTheme", group: "Spectrum",
                                                    row: "peakValueColour"))
        #expect(places["bandPlanSize"] == .phone(page: "display.threeD", group: "Band plan", row: "bandPlanSize"))
        if case .appearance = places["gridColour"] {} else { Issue.record("grid colour is not in Appearance") }

        // Transmit Display's scale says dB Max and dB Min.
        let txScale = try #require(section("display.txDisplay", "Transmit scale"))
        #expect(txScale.controls.map(\.label) == ["dB Max:", "dB Min:"])
        // FFT & Engine holds the thread priority and the monitor's rate, left off on this phone.
        let fft = try #require(display.pages.first { $0.id == "display.fftEngine" })
        let fftKeys = Set(fft.sections.flatMap(\.controls).compactMap(Self.key))
        #expect(DisplayPhoneGroups.leftOff.isSubset(of: fftKeys))
        // Multimeter: the Core's polling delay and this phone's decimal point, units and history.
        let meter = try #require(display.pages.first { $0.id == "display.multimeter" })
        let meterKeys = meter.sections.flatMap(\.controls).compactMap(Self.key)
        for key in ["MultimeterDelayMs", "MultimeterShowDecimal", "MultimeterUnitMode",
                    "MultimeterSignalHistoryDurationMs"] {
            #expect(meterKeys.contains(key), "Multimeter has no \(key)")
        }
    }

    // MARK: Reset and copy, a page and a pan at a time

    @Test("Reset this pan and Use Pan N's on all pans act on one page's own rows for the pan chosen")
    func resetAndCopyPerPage() throws {
        let rig = try DisplaySheetTests.Rig()
        defer { UserDefaults(suiteName: rig.suite)?.removePersistentDomain(forName: rig.suite) }
        let display = try Self.resource("display.json")
        let spectrum = try #require(display.pages.first { $0.id == "display.spectrumDefaults" })
        let peaks = try #require(display.pages.first { $0.id == "display.spectrumPeaks" })
        let spectrumFields = PhoneSetupKeys.pictureFields(on: spectrum)
        let peaksFields = PhoneSetupKeys.pictureFields(on: peaks)
        #expect(spectrumFields.contains("traceFill"))
        #expect(peaksFields.contains("activePeakHold"))
        #expect(spectrumFields.isDisjoint(with: peaksFields))
        let main = rig.main
        let fill = main.band.settings.traceFill
        let hold = main.band.settings.activePeakHold

        // Pan 1 keeps its own value on two pages.
        main.displayEditScope = .pan(.first)
        main.changeDisplay(fields: ["traceFill"]) { $0.traceFill = !fill }
        main.changeDisplay(fields: ["activePeakHold"]) { $0.activePeakHold = !hold }
        #expect(main.ownDisplayFields(among: spectrumFields, pan: .first) == ["traceFill"])
        #expect(main.ownDisplayFields(among: peaksFields, pan: .first) == ["activePeakHold"])
        #expect(DisplayPageOwnRows.count(on: spectrum, own: ["traceFill"]) == 1)
        #expect(DisplayPageOwnRows.count(on: spectrum, own: []) == 0)

        // Reset on Spectrum: Spectrum's row follows All pans again; Peaks keeps its own.
        main.resetDisplayPicture(fields: spectrumFields, pan: .first)
        #expect(main.ownDisplayFields(among: spectrumFields, pan: .first).isEmpty)
        #expect(main.band.settings.traceFill == fill)
        #expect(main.ownDisplayFields(among: peaksFields, pan: .first) == ["activePeakHold"])
        #expect(main.band.settings.activePeakHold == !hold)

        // Use Pan 1's on all pans on Peaks: All pans takes it; nothing left to copy.
        #expect(main.copyOwnDisplayPictureToMaster(fields: peaksFields, pan: .first))
        #expect(rig.store.masterSettings(forPan: BandSubscriber.panId).activePeakHold == !hold)
        #expect(main.ownDisplayFields(among: peaksFields, pan: .first).isEmpty)
        #expect(!main.copyOwnDisplayPictureToMaster(fields: peaksFields, pan: .first))
        #expect(rig.store.masterSettings(forPan: BandSubscriber.panId).traceFill == fill)

        // Pan 2, not open: its own value is kept for it and Pan 1 is left alone.
        main.displayEditScope = .pan(Self.pan2)
        main.changeDisplay(fields: ["traceFill"]) { $0.traceFill = !fill }
        #expect(rig.store.settings(forPan: Self.pan2.storeKey).traceFill == !fill)
        #expect(main.displayEditingSettings.traceFill == !fill)
        #expect(main.band.settings.traceFill == fill)
        #expect(main.ownDisplayFields(among: spectrumFields, pan: Self.pan2) == ["traceFill"])
        #expect(main.ownDisplayFields(among: spectrumFields, pan: .first).isEmpty)
        main.resetDisplayPicture(fields: spectrumFields, pan: Self.pan2)
        #expect(main.ownDisplayFields(among: spectrumFields, pan: Self.pan2).isEmpty)
        #expect(rig.store.settings(forPan: Self.pan2.storeKey).traceFill == fill)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    // MARK: Levels' band picker on a Core that keeps each band's scale

    @Test("at version 29 the Levels band picker edits the chosen band's dB Max on the Core without tuning")
    func levelsPickerWritesTheCoresBand() async throws {
        let rig = try await Described.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Described.asV12Core(app, setupDescription: 29)
        await rig.station.deliverSetup(try Described.coreCategories(peer: 29))
        #expect(await Described.settle {
            app.setupPages.isCurrent && app.setupPages.categories["display"]?.version == 29
                && app.main.bandScale.isShared && app.main.bandScale.isConnected
        })
        #expect(app.setupPages.categories["display"]?.version == 29,
                "pages \(String(describing: app.setupPages.categories["display"]?.version)) feed \(String(describing: app.setupFeed.description(for: "display")?.version)) current \(app.setupPages.isCurrent) unreadable \(app.setupPages.unreadable) shared \(app.main.bandScale.isShared) connected \(app.main.bandScale.isConnected)")
        let main = app.main
        let shown = main.band.settings.scaleBand
        let choice = shown == "5" ? "7" : "5"
        let key = try #require(CoreBandScale.maxKey(band: Int(choice) ?? 0))
        let vfo = app.mirror.object("slice:0")?["frequency"]
        let suite = "DisplayStripTests-keys-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let keys = PhoneSetupKeys(main: main, defaults: defaults)
        main.displayEditScope = .all
        main.displayBandChoice = choice
        #expect(main.displayEditingBand == choice)
        #expect(keys.set(.integer(-44), forPhoneKey: "DisplayGridMax"))
        let write = try await Described.settingsWrite(key, rig.station)
        #expect(write.properties.first?.value == .utf8("-44"))
        #expect(keys.value(forPhoneKey: "DisplayGridMax") == .integer(-44))
        // No tune: the pan stays on its band, the slice on its frequency.
        #expect(main.band.settings.scaleBand == shown)
        #expect(app.mirror.object("slice:0")?["frequency"] == vfo)
        await app.disconnect()
    }

    // MARK: Opening a Display page from the band, cold

    @Test("a pan's Setup link opens its version 29 page with the pan chosen, before Setup has ever been shown")
    func coldOpenReachesTheVersion29Page() async throws {
        let rig = try await Described.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        // An older Core first: More display options opens On this phone.
        await rig.station.deliverSetup(try Described.coreCategories())
        #expect(await Described.settle { app.setupPages.isCurrent && app.setupPages.categories["display"] != nil })
        let older = SetupRouter()
        RootView.openDisplaySetup(older, app: app)
        #expect(older.path == [.category("Display"), .page(.displayOnThisPhone)])

        Described.asV12Core(app, setupDescription: 29)
        await rig.station.deliverSetup(try Described.coreCategories(peer: 29))
        #expect(await Described.settle {
            app.setupPages.isCurrent && app.setupPages.categories["display"]?.version == 29
        })
        #expect(app.setupPages.categories["display"]?.version == 29,
                "pages \(String(describing: app.setupPages.categories["display"]?.version)) feed \(String(describing: app.setupFeed.description(for: "display")?.version)) current \(app.setupPages.isCurrent) unreadable \(app.setupPages.unreadable) shared \(app.main.bandScale.isShared) connected \(app.main.bandScale.isConnected)")
        let newer = SetupRouter()
        RootView.openDisplaySetup(newer, app: app)
        #expect(newer.path == [.category("Display"),
                               .described(category: "display", page: "display.spectrumDefaults")])
        #expect(newer.displayScope == .all)

        // The app's root on the band, the Setup tab never shown.
        #expect(app.phoneNavigation == nil)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 874)
        let root = RootView(selection: .panadapter, setupRouter: rig.router)
            .environmentObject(app)
            .environmentObject(rig.flow)
        let host = ShotHosting.host(root)
        host.view.frame = window.bounds
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let navigation = try #require(app.phoneNavigation)
        #expect(rig.router.path.isEmpty)
        #expect(navigation.openDisplayPage("display.spectrumPeaks", .pan(.first)))
        #expect(rig.router.path == [.category("Display"),
                                    .described(category: "display", page: "display.spectrumPeaks")])
        #expect(rig.router.displayScope == .pan(.first))
        // The Setup tab came forward and its rows edit Pan 1.
        await ShotWait.laidOut(window)
        #expect(await Described.settle { app.main.displayEditScope == .pan(.first) })
        // A page no category holds opens nothing.
        #expect(!navigation.openDisplayPage("nowhere", .all))
        await app.disconnect()
    }
}
