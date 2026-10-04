// NereusSDR for iOS: tests for the Core's catalogue, against the link's catalogue fixtures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Testing
@testable import NereusModels

/// Link document section 7.4 and R-IOS-06, R-IOS-27. Every catalogue value
/// here is read from the Core's own fixtures at run time (D4); none is
/// written into the tests or the app.
@Suite struct StationCatalogTests {
    static let fixtures = ["session-catalog-anan-g2", "session-catalog-hermes-lite-2"]

    @Test(arguments: fixtures)
    func eachCatalogueFixtureParsesToExactlyWhatItHolds(_ fixture: String) throws {
        let json = try Self.catalogueJson(fixture)
        let catalog = try #require(StationCatalog.parse(json: json))
        var raw = try Self.object(json)
        // The thirteen sections every catalogue carries, the band grid a
        // Core with bandSelectVersion 1 adds, and the Setup > Display and
        // noise-reduction descriptions a newer Core adds (link 7.4).
        #expect(Set(raw.keys).count == 16)
        #expect(raw["bands"] != nil)
        #expect(raw["noiseReduction"] != nil)
        let board = try #require(raw["board"] as? [String: Any])
        for key in Self.laterBoardMembers {
            #expect(board[key] != nil, "\(fixture): board.\(key)")
        }
        raw = Self.upperColours(raw) as? [String: Any] ?? raw
        let rebuilt = Self.foundation(catalog)
        #expect(Set(rebuilt.keys) == Set(raw.keys))
        for key in raw.keys.sorted() {
            #expect(NSDictionary(dictionary: [key: rebuilt[key] as Any])
                .isEqual(to: [key: raw[key] as Any]), "\(fixture): \(key)")
        }
    }

    @Test(arguments: fixtures)
    func whatTheAppShowsFollowsTheCore(_ fixture: String) throws {
        let json = try Self.catalogueJson(fixture)
        let catalog = try #require(StationCatalog.parse(json: json))
        let raw = try Self.object(json)
        // Only offered items, in their place (D41).
        let tools = try #require(raw["tools"] as? [[String: Any]])
        #expect(catalog.offeredTools.map(\.id) == tools.filter { $0["offered"] as? Bool == true }.map { $0["id"] as? String })
        let items = try #require(raw["radioItems"] as? [[String: Any]])
        #expect(catalog.offeredRadioItems.map(\.id)
                == items.filter { $0["offered"] as? Bool == true }.map { $0["id"] as? String })
        // The default band plan is the one marked default, and there is one.
        let plans = try #require(raw["bandPlans"] as? [[String: Any]])
        let marked = plans.filter { $0["default"] as? Bool == true }
        #expect(marked.count == 1)
        #expect(catalog.defaultBandPlan?.id == marked.first?["id"] as? String)
        // The tune steps come smallest first, as sent.
        #expect(catalog.tuneSteps.map(\.hz) == catalog.tuneSteps.map(\.hz).sorted())
        // Presets are keyed by each mode's label, one to ten a mode, F1 in slot 0.
        #expect(Set(catalog.filterPresets.keys) == Set(catalog.modes.map(\.label)))
        for (label, presets) in catalog.filterPresets {
            #expect((1...10).contains(presets.count), "\(label)")
            #expect(presets.map(\.slot) == Array(0..<presets.count), "\(label)")
        }
        // One slice colour for each slice the radio allows.
        #expect(catalog.sliceColours.count == catalog.board.maxSlices)
        // The receive ranges, each named after its setting.
        let receive = try #require(raw["receive"] as? [String: Any])
        #expect(Set(catalog.receive.ranges.keys) == Set(receive.keys))
        for range in [catalog.receive.afGain, catalog.receive.ssqlThresh, catalog.receive.amsqThresh,
                      catalog.receive.fmsqThresh] {
            #expect(range != nil)
        }
        #expect(catalog.agc.thresholdDb != nil)
    }

    /// The Core that follows its band plan (link 7.4) marks exactly one plan
    /// `active`, here its default, and gives every plan its spots; the app
    /// reads both as sent.
    @Test(arguments: fixtures)
    func theCoresPlanIsMarkedActiveAndEveryPlanCarriesItsSpots(_ fixture: String) throws {
        let json = try Self.catalogueJson(fixture)
        let catalog = try #require(StationCatalog.parse(json: json))
        let raw = try #require(try Self.object(json)["bandPlans"] as? [[String: Any]])
        #expect(raw.allSatisfy { $0["active"] is Bool && $0["spots"] is [[String: Any]] })
        #expect(catalog.bandPlans.filter(\.isActive).map(\.id)
                == raw.filter { $0["active"] as? Bool == true }.map { $0["id"] as? String })
        #expect(catalog.bandPlans.filter(\.isActive).count == 1)
        for (plan, sent) in zip(catalog.bandPlans, raw) {
            let spots = try #require(sent["spots"] as? [[String: Any]])
            #expect(!plan.spots.isEmpty, "\(plan.name)")
            #expect(plan.spots.map(\.hz) == spots.map { ($0["hz"] as? NSNumber)?.doubleValue }, "\(plan.name)")
            #expect(plan.spots.map(\.label) == spots.map { $0["label"] as? String }, "\(plan.name)")
        }
    }

    @Test func theTwoRadiosDifferOnlyInTheBoardThePowerGaugeAndWhatTheyOffer() throws {
        let g2 = try #require(StationCatalog.parse(json: try Self.catalogueJson(Self.fixtures[0])))
        let hl2 = try #require(StationCatalog.parse(json: try Self.catalogueJson(Self.fixtures[1])))
        #expect(g2.board != hl2.board)
        #expect(g2.meters.rfPower != hl2.meters.rfPower)
        #expect(g2.meters.sMeter == hl2.meters.sMeter)
        #expect(g2.meters.micLevel == hl2.meters.micLevel)
        #expect(g2.meters.swr == hl2.meters.swr)
        #expect(g2.modes == hl2.modes)
        #expect(g2.filterPresets == hl2.filterPresets)
        #expect(g2.tuneSteps == hl2.tuneSteps)
        #expect(g2.agc == hl2.agc)
        #expect(g2.receive == hl2.receive)
        #expect(g2.bandPlans == hl2.bandPlans)
        #expect(g2.palettes == hl2.palettes)
        #expect(g2.sliceColours == hl2.sliceColours)
        // The same tools and Radio items in the same order; the Hermes Lite 2
        // offers no Diversity (no diversity receiver) and no Antenna Setup
        // (no Alex antenna control), as the Core's catalogue says.
        #expect(g2.tools.map(\.id) == hl2.tools.map(\.id))
        #expect(g2.radioItems.map(\.id) == hl2.radioItems.map(\.id))
        let toolDifferences = zip(g2.tools, hl2.tools).filter { $0 != $1 }.map(\.0.id)
        #expect(toolDifferences == ["diversity"])
        #expect(g2.tools.first { $0.id == "diversity" }?.offered == true)
        #expect(hl2.tools.first { $0.id == "diversity" }?.offered == false)
        let itemDifferences = zip(g2.radioItems, hl2.radioItems).filter { $0 != $1 }.map(\.0.id)
        #expect(itemDifferences == ["antennaSetup"])
        #expect(g2.radioItems.first { $0.id == "antennaSetup" }?.offered == true)
        #expect(hl2.radioItems.first { $0.id == "antennaSetup" }?.offered == false)
        #expect(g2.audio == hl2.audio)
        #expect(g2.noiseReduction == hl2.noiseReduction)
    }

    @Test func unknownKeysAndMembersAreIgnored() throws {
        let json = try Self.catalogueJson(Self.fixtures[0])
        let plain = try #require(StationCatalog.parse(json: json))
        var raw = try Self.object(json)
        raw["aKeyFromALaterCore"] = ["anything": [1, 2, 3]]
        var modes = try #require(raw["modes"] as? [[String: Any]])
        modes[0]["aMemberFromALaterCore"] = "text"
        raw["modes"] = modes
        var board = try #require(raw["board"] as? [String: Any])
        board["aMemberFromALaterCore"] = true
        raw["board"] = board
        var agc = try #require(raw["agc"] as? [String: Any])
        agc["notARange"] = "text"
        raw["agc"] = agc
        let extended = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(extended == plain)

        // A later range in the AGC section is kept by the name of its setting.
        agc["aLaterRange"] = ["min": 0, "max": 10, "step": 1]
        raw["agc"] = agc
        let withRange = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(withRange.agc.ranges["aLaterRange"] == StationCatalog.Range(min: 0, max: 10, step: 1))
        #expect(withRange.agc.thresholdDb == plain.agc.thresholdDb)
    }

    @Test func theBandGridIsTheCoresInItsOrderAndAbsentFromAnOlderCore() throws {
        let json = try Self.catalogueJson(Self.fixtures[0])
        // The Core's own grid, read in the order it sends it.
        let current = try #require(StationCatalog.parse(json: json))
        var raw = try Self.object(json)
        let fixtureBands = try #require(raw["bands"] as? [[String: Any]])
        #expect(current.bands == fixtureBands.map { .init(id: $0["id"] as? Int ?? -1, label: $0["label"] as? String ?? "") })
        #expect(current.bands?.isEmpty == false)
        // A Core from before band select sends no grid.
        raw.removeValue(forKey: "bands")
        let older = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(older.bands == nil)
        #expect(older.modes == current.modes)
        // Synthetic bands in the shape the Core sends; none come from a radio table.
        let sent: [[String: Any]] = [["id": 3, "label": "40"], ["id": 0, "label": "160"], ["id": 12, "label": "WWV"]]
        raw["bands"] = sent
        let newer = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(newer.bands == [.init(id: 3, label: "40"), .init(id: 0, label: "160"), .init(id: 12, label: "WWV")])
        #expect(NSDictionary(dictionary: ["bands": Self.foundation(newer)["bands"] as Any]).isEqual(to: ["bands": sent]))
        // An unreadable grid costs only the grid.
        raw["bands"] = [["id": "forty", "label": 40]]
        let unreadable = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(unreadable.bands == nil)
        #expect(unreadable.modes == older.modes)
    }

    @Test func anEmptyJsonIsNoCatalogueYet() {
        #expect(StationCatalog.parse(json: "") == nil)
    }

    @Test func anUnreadableCatalogueIsNil() throws {
        let json = try Self.catalogueJson(Self.fixtures[0])
        #expect(StationCatalog.parse(json: "{") == nil)
        #expect(StationCatalog.parse(json: "[]") == nil)
        #expect(StationCatalog.parse(json: String(json.dropLast())) == nil)
        let raw = try Self.object(json)
        // A key of the thirteen missing, or of the wrong shape. The band grid
        // is not one of them (a Core before band select sends none), nor is
        // what a newer Core adds.
        for key in raw.keys.sorted() where key != "bands" && !Self.laterSections.contains(key) {
            var missing = raw
            missing.removeValue(forKey: key)
            #expect(StationCatalog.parse(json: try Self.text(missing)) == nil, "without \(key)")
        }
        var wrong = raw
        wrong["modes"] = "LSB"
        #expect(StationCatalog.parse(json: try Self.text(wrong)) == nil)
        // A colour that is not #RRGGBB.
        var colours = raw
        colours["sliceColours"] = ["#12345"]
        #expect(StationCatalog.parse(json: try Self.text(colours)) == nil)
        colours["sliceColours"] = ["123456"]
        #expect(StationCatalog.parse(json: try Self.text(colours)) == nil)
        colours["sliceColours"] = ["#GG0000"]
        #expect(StationCatalog.parse(json: try Self.text(colours)) == nil)
    }

    /// Each segment names its lowest licence class as the Core sends it,
    /// and a catalogue from a Core that sends none still reads (link 7.4).
    @Test func segmentsCarryTheirLowestClassAndAnOlderCoreStillReads() throws {
        let raw = try Self.object(try Self.catalogueJson(Self.fixtures[0]))
        let catalog = try #require(StationCatalog.parse(json: try Self.text(raw)))
        let classes = Set(catalog.bandPlans.flatMap { $0.segments.map(\.lowestClass) })
        #expect(classes.isSubset(of: ["Tech", "General", "Extra", ""]))
        #expect(classes.contains("Extra") && classes.contains("General"))

        var older = raw
        let plans = try #require(raw["bandPlans"] as? [[String: Any]])
        older["bandPlans"] = plans.map { plan -> [String: Any] in
            var plan = plan
            let segments = plan["segments"] as? [[String: Any]] ?? []
            plan["segments"] = segments.map { segment -> [String: Any] in
                var segment = segment
                segment.removeValue(forKey: "lowestClass")
                return segment
            }
            return plan
        }
        let read = try #require(StationCatalog.parse(json: try Self.text(older)))
        let olderClasses = read.bandPlans.flatMap { $0.segments.map(\.lowestClass) }
        #expect(Set(olderClasses) == [""])
        #expect(read.bandPlans.map(\.segments.count) == catalog.bandPlans.map(\.segments.count))
    }

    @Test func coloursAreUpperCase() throws {
        var raw = try Self.object(try Self.catalogueJson(Self.fixtures[0]))
        let colours = try #require(raw["sliceColours"] as? [String])
        raw["sliceColours"] = colours.map { $0.lowercased() }
        let catalog = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(catalog.sliceColours == colours.map { $0.uppercased() })
        let every = catalog.sliceColours + catalog.palettes.flatMap { $0.stops.map(\.colour) }
            + catalog.bandPlans.flatMap { $0.segments.map(\.colour) }
        for colour in every {
            #expect(colour == colour.uppercased() && colour.hasPrefix("#") && colour.count == 7)
        }
    }

    /// Every section a newer Core adds: none is required.
    static let laterSections = ["display", "noiseReduction"]
    /// The same for the board: the transmit ranges, the RX1 preamp, the
    /// relays, RX2's own input control (Level Cal 2) and the radio's mic
    /// input (the radio codec lane).
    static let laterBoardMembers = ["transmit", "rx1Preamp", "relays", "rx2Attenuator", "rx2PreampItems",
                                    "rx2AttenuatorReason", "radioMic", "radioMicNote"]

    /// A catalogue from a Core before the Display description, the
    /// noise-reduction controls and the board's transmit ranges and relays
    /// still reads: the sections not kept change nothing, and without
    /// `display` the catalogue has none.
    @Test func aCatalogueWithOrWithoutTheNewerSectionsReads() throws {
        for fixture in Self.fixtures {
            let raw = try Self.object(try Self.catalogueJson(fixture))
            let current = try #require(StationCatalog.parse(json: try Self.text(raw)), "\(fixture)")
            #expect(current.display != nil, "\(fixture)")
            var withoutDisplay = raw
            withoutDisplay.removeValue(forKey: "display")
            let noDisplay = try #require(StationCatalog.parse(json: try Self.text(withoutDisplay)), "\(fixture)")
            #expect(noDisplay.display == nil, "\(fixture)")
            #expect(noDisplay.modes == current.modes && noDisplay.board == current.board, "\(fixture)")
            #expect(current.audio.opusProfiles?.isEmpty == false, "\(fixture)")
            var older = withoutDisplay
            older.removeValue(forKey: "noiseReduction")
            // An older Core's catalogue carries an empty `audio`.
            older["audio"] = [String: Any]()
            var board = try #require(older["board"] as? [String: Any])
            for key in Self.laterBoardMembers {
                board.removeValue(forKey: key)
            }
            older["board"] = board
            let read = try #require(StationCatalog.parse(json: try Self.text(older)), "\(fixture)")
            #expect(read.noiseReduction == nil && read.board.transmit == nil, "\(fixture)")
            #expect(read.audio.opusProfiles == nil, "\(fixture)")
            #expect(read.board.rx1Preamp == nil && read.board.relays == nil, "\(fixture)")
            #expect(read.board.rx2Attenuator == nil && read.board.rx2PreampItems == nil
                && read.board.rx2AttenuatorReason == nil, "\(fixture)")
            #expect(read.board.radioMic == nil && read.board.radioMicNote == nil, "\(fixture)")
            #expect(read.modes == noDisplay.modes && read.board.rxAntennas == noDisplay.board.rxAntennas, "\(fixture)")
        }
    }

    /// RX2's own input control (Level Cal 2), as each radio's board sends
    /// it: the ANAN-G2's second ADC has a 0 to 31 dB attenuator and no
    /// preamp list; the Hermes Lite 2 shares RX1's input and says so; an
    /// HPSDR's second Mercury has two preamp states. Read as sent.
    @Test func rx2InputControlIsReadAsTheBoardSendsIt() throws {
        let g2 = try #require(StationCatalog.parse(json: try Self.catalogueJson(Self.fixtures[0])))
        #expect(g2.board.rx2Attenuator == StationCatalog.Range(min: 0, max: 31, step: 1))
        #expect(g2.board.rx2PreampItems == [])
        #expect(g2.board.rx2AttenuatorReason == nil)
        let hl2 = try #require(StationCatalog.parse(json: try Self.catalogueJson(Self.fixtures[1])))
        #expect(hl2.board.rx2Attenuator == nil)
        #expect(hl2.board.rx2PreampItems == [])
        #expect(hl2.board.rx2AttenuatorReason == "RX2 uses RX1's input on this radio. Set it with RX1's attenuator.")

        var raw = try Self.object(try Self.catalogueJson(Self.fixtures[0]))
        var board = try #require(raw["board"] as? [String: Any])
        board["rx2Attenuator"] = NSNull()
        board["rx2PreampItems"] = [["id": 1, "label": "0dB"], ["id": 0, "label": "-20dB"]]
        raw["board"] = board
        let hpsdr = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(hpsdr.board.rx2Attenuator == nil)
        #expect(hpsdr.board.rx2PreampItems?.map(\.id) == [1, 0])
        #expect(hpsdr.board.rx2PreampItems?.map(\.label) == ["0dB", "-20dB"])
        #expect(hpsdr.board.rx2AttenuatorReason == nil)

        // An unreadable RX2 list costs only itself.
        board["rx2PreampItems"] = "0dB"
        raw["board"] = board
        let odd = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(odd.board.rx2PreampItems == nil && odd.board.preampItems == g2.board.preampItems)
    }

    /// The radio's own mic input (the radio codec lane, `radioMicVersion`
    /// 1), as each radio's board sends it: the ANAN-G2's mic jack with no
    /// note; the Hermes Lite 2 through its audio add-on board, with the
    /// Core's note. An unreadable member costs only itself.
    @Test func theRadioMicIsReadAsTheBoardSendsIt() throws {
        let g2 = try #require(StationCatalog.parse(json: try Self.catalogueJson(Self.fixtures[0])))
        #expect(g2.board.radioMic == true)
        #expect(g2.board.radioMicNote == nil)
        let hl2 = try #require(StationCatalog.parse(json: try Self.catalogueJson(Self.fixtures[1])))
        #expect(hl2.board.radioMic == true)
        #expect(hl2.board.radioMicNote
                == "Needs the Hermes Lite 2 audio add-on board. A stock Hermes Lite 2 sends no mic audio.")

        var raw = try Self.object(try Self.catalogueJson(Self.fixtures[1]))
        var board = try #require(raw["board"] as? [String: Any])
        board["radioMic"] = "yes"
        raw["board"] = board
        let odd = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(odd.board.radioMic == nil && odd.board.radioMicNote == hl2.board.radioMicNote)
    }

    // MARK: Helpers

    /// The `json` property of the fixture's `catalog` object.
    static func catalogueJson(_ fixtureId: String) throws -> String {
        guard let entry = try LinkFixtureLoader.manifest().first(where: { $0.id == fixtureId }) else {
            throw LinkFixtureLoader.Malformed(description: "no fixture \(fixtureId)")
        }
        let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(entry.file))
        let steps = object["steps"] as? [[String: Any]] ?? []
        for step in steps {
            guard let message = step["message"] as? [String: Any], message["type"] as? String == "object.create",
                  message["key"] as? String == "catalog",
                  let properties = message["properties"] as? [[String: Any]],
                  let json = properties.first(where: { $0["name"] as? String == "json" })?["value"] as? String else {
                continue
            }
            return json
        }
        throw LinkFixtureLoader.Malformed(description: "\(fixtureId) creates no catalog object")
    }

    static func object(_ json: String) throws -> [String: Any] {
        guard let object = try JSONSerialization.jsonObject(with: Data(json.utf8)) as? [String: Any] else {
            throw LinkFixtureLoader.Malformed(description: "the catalogue is not an object")
        }
        return object
    }

    static func text(_ object: [String: Any]) throws -> String {
        String(decoding: try JSONSerialization.data(withJSONObject: object), as: UTF8.self)
    }

    /// `value` with every `#rrggbb` string in upper case, as the parser holds them.
    static func upperColours(_ value: Any) -> Any {
        switch value {
        case let text as String where text.hasPrefix("#"):
            return text.uppercased()
        case let array as [Any]:
            return array.map(upperColours)
        case let object as [String: Any]:
            return object.mapValues(upperColours)
        default:
            return value
        }
    }

    /// The parsed catalogue written back in the wire's shape, to compare
    /// with the fixture member by member.
    static func foundation(_ c: StationCatalog) -> [String: Any] {
        func range(_ r: StationCatalog.Range) -> [String: Any] {
            ["min": r.min, "max": r.max, "step": r.step]
        }
        func choice(_ item: StationCatalog.Choice) -> [String: Any] {
            ["id": item.id, "label": item.label]
        }
        func mark(_ m: StationCatalog.Mark) -> [String: Any] {
            ["label": m.label, "dbm": m.dbm]
        }
        let s = c.meters.sMeter
        var agc: [String: Any] = ["modes": c.agc.modes.map(choice)]
        for (name, r) in c.agc.ranges {
            agc[name] = range(r)
        }
        let board = c.board
        return [
            "modes": c.modes.map { ["id": $0.id, "label": $0.label, "sideband": $0.sideband.rawValue] },
            "filterPresets": c.filterPresets.mapValues { presets in
                presets.map { ["slot": $0.slot, "label": $0.label, "lowHz": $0.lowHz, "highHz": $0.highHz] }
            },
            "tuneSteps": c.tuneSteps.map { ["hz": $0.hz, "label": $0.label] },
            "agc": agc,
            "receive": c.receive.ranges.mapValues(range),
            "meters": [
                "sMeter": ["minDbm": s.minDbm, "s9Dbm": s.s9Dbm, "maxDbm": s.maxDbm, "dbPerSUnit": s.dbPerSUnit,
                           "redFromDbm": s.redFromDbm, "sUnits": s.sUnits.map(mark), "overS9": s.overS9.map(mark)],
                "micLevel": ["minDb": c.meters.micLevel.minDb, "maxDb": c.meters.micLevel.maxDb,
                             "yellowFromDb": c.meters.micLevel.yellowFromDb,
                             "redFromDb": c.meters.micLevel.redFromDb],
                "rfPower": ["minW": c.meters.rfPower.minW, "maxW": c.meters.rfPower.maxW,
                            "ratedW": c.meters.rfPower.ratedW, "redFromW": c.meters.rfPower.redFromW],
                "swr": ["min": c.meters.swr.min, "max": c.meters.swr.max, "redFrom": c.meters.swr.redFrom],
            ],
            "board": [
                "model": board.model, "productLabel": board.productLabel, "maxSlices": board.maxSlices,
                "attenuator": board.attenuator.map(range) ?? NSNull(),
                "preampItems": board.preampItems.map(choice), "rxAntennas": board.rxAntennas,
                "txAntennas": board.txAntennas, "rxOnlyInputs": board.rxOnlyInputs,
                "sampleRates": board.sampleRates, "pureSignal": board.pureSignal, "paRatingW": board.paRatingW,
                "micJack": board.micJack,
            ].merging(boardLaterFoundation(board)) { $1 },
            "bandPlans": c.bandPlans.map { plan in
                ["id": plan.id, "name": plan.name, "default": plan.isDefault, "active": plan.isActive,
                 "segments": plan.segments.map {
                     ["lowHz": $0.lowHz, "highHz": $0.highHz, "label": $0.label, "licence": $0.licence,
                      "lowestClass": $0.lowestClass, "colour": $0.colour]
                 },
                 "spots": plan.spots.map { ["hz": $0.hz, "label": $0.label] as [String: Any] }] as [String: Any]
            },
            "palettes": c.palettes.map { palette in
                ["id": palette.id, "name": palette.name,
                 "stops": palette.stops.map { ["at": $0.at, "colour": $0.colour] }] as [String: Any]
            },
            "sliceColours": c.sliceColours,
            "tools": c.tools.map { ["id": $0.id, "label": $0.label, "where": $0.where, "offered": $0.offered] },
            "radioItems": c.radioItems.map { ["id": $0.id, "label": $0.label, "offered": $0.offered] },
            "audio": c.audio.opusProfiles.map { profiles in
                ["opusProfiles": profiles.map { ["bitrate": $0.bitrate, "bandwidthHz": $0.bandwidthHz] }] as [String: Any]
            } ?? [String: Any](),
        ].merging(c.bands.map { ["bands": $0.map { ["id": $0.id, "label": $0.label] as [String: Any] }] } ?? [:]) { $1 }
            .merging(c.display.map { ["display": displayFoundation($0)] } ?? [:]) { $1 }
            .merging(c.noiseReduction.map { ["noiseReduction": noiseReductionFoundation($0)] } ?? [:]) { $1 }
    }

    /// The board's later members written back in the wire's shape.
    static func boardLaterFoundation(_ board: StationCatalog.Board) -> [String: Any] {
        func range(_ r: StationCatalog.Range) -> [String: Any] {
            ["min": r.min, "max": r.max, "step": r.step]
        }
        func transmitRange(_ r: StationCatalog.TransmitRange) -> [String: Any] {
            var out: [String: Any] = ["min": r.min, "max": r.max, "step": r.step]
            if let shown = r.shown {
                out["shown"] = ["min": shown.min, "max": shown.max, "decimals": shown.decimals, "unit": shown.unit,
                                "rounding": shown.rounding.rawValue,
                                "endSnap": shown.endSnap.map { ["below": $0.below, "above": $0.above] as Any }
                                    ?? NSNull()] as [String: Any]
            }
            return out
        }
        var out: [String: Any] = [:]
        if let transmit = board.transmit {
            var t: [String: Any] = [:]
            if let value = transmit.power { t["power"] = transmitRange(value) }
            if let value = transmit.tunePowerForTxBand { t["tunePowerForTxBand"] = transmitRange(value) }
            if let value = transmit.tunePower { t["tunePower"] = transmitRange(value) }
            if let value = transmit.micGainDb { t["micGainDb"] = range(value) }
            out["transmit"] = t
        }
        if let value = board.rx1Preamp {
            out["rx1Preamp"] = value
        }
        if let items = board.rx2PreampItems {
            // Level Cal 2: the three come together; null where the radio has none.
            out["rx2Attenuator"] = board.rx2Attenuator.map(range) ?? NSNull()
            out["rx2PreampItems"] = items.map { ["id": $0.id, "label": $0.label] as [String: Any] }
            out["rx2AttenuatorReason"] = board.rx2AttenuatorReason as Any? ?? NSNull()
        }
        if let radioMic = board.radioMic {
            // The radio codec lane: the two come together; the note null where there is none.
            out["radioMic"] = radioMic
            out["radioMicNote"] = board.radioMicNote as Any? ?? NSNull()
        }
        if let relays = board.relays {
            out["relays"] = ["rxOutOnTx": relays.rxOutOnTx, "ext1OutOnTx": relays.ext1OutOnTx as Any? ?? NSNull(),
                             "ext2OutOnTx": relays.ext2OutOnTx as Any? ?? NSNull(),
                             "rxOutOverride": relays.rxOutOverride] as [String: Any]
        }
        return out
    }

    /// The noise-reduction controls written back in the wire's shape.
    static func noiseReductionFoundation(_ nr: StationCatalog.NoiseReduction) -> [String: Any] {
        nr.slots.mapValues { controls in
            controls.map { control -> [String: Any] in
                var out: [String: Any] = ["property": control.property, "label": control.label]
                switch control.kind {
                case .slider(let s):
                    out.merge(["kind": "slider", "min": s.min, "max": s.max, "step": s.step, "scale": s.scale,
                               "divide": s.divide, "decimals": s.decimals, "suffix": s.suffix,
                               "default": s.defaultValue, "reset": s.reset as Any? ?? NSNull()]) { $1 }
                case .toggle(let on):
                    out.merge(["kind": "switch", "default": on]) { $1 }
                case .choice(let c):
                    out.merge(["kind": "choice", "options": c.options.map { ["id": $0.id, "label": $0.label] },
                               "default": c.defaultId, "reset": c.reset as Any? ?? NSNull()]) { $1 }
                }
                return out
            }
        }
    }

    /// The Display description written back in the wire's shape.
    static func displayFoundation(_ d: StationCatalog.Display) -> [String: Any] {
        [
            "controls": d.controls.map { control -> [String: Any] in
                var out: [String: Any] = [
                    "settingsKey": control.settingsKey.map { $0 as Any } ?? NSNull(), "scope": control.scope,
                    "subscribe": control.subscribe, "page": control.page, "group": control.group,
                    "label": control.label, "kind": control.kind.name,
                ]
                if let value = control.defaultValue { out["default"] = value }
                if !control.options.isEmpty {
                    out["options"] = control.options.map { ["value": $0.value, "label": $0.label] as [String: Any] }
                }
                if let value = control.min { out["min"] = value }
                if let value = control.max { out["max"] = value }
                if let value = control.step { out["step"] = value }
                if let value = control.unit { out["unit"] = value }
                if let value = control.decimals { out["decimals"] = value }
                if let value = control.offValue { out["offValue"] = value }
                if let value = control.offLabel { out["offLabel"] = value }
                return out
            },
            "binWidth": ["label": d.binWidth.label, "decimals": d.binWidth.decimals],
            "fftPlan": ["minFftSize": d.fftPlan.minFftSize, "maxFftSize": d.fftPlan.maxFftSize],
        ]
    }
}
