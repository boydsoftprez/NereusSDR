// NereusSDR for iOS: tests for the catalogue's transmit ranges, presence flags and noise-reduction controls
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Testing
@testable import NereusModels

/// Link document section 7.4 (`board.transmit`, `board.rx1Preamp`,
/// `board.relays`, `noiseReduction`; R-IOS-06, R-IOS-27). The ranges are
/// read from the Core's own fixtures at run time (D4); the readouts checked
/// are the link document's own worked examples.
@Suite struct StationCatalogRangesTests {
    static let anan = "session-catalog-anan-g2"
    static let hermesLite = "session-catalog-hermes-lite-2"

    // MARK: Present, per radio

    @Test func aHundredWattBoardShowsItsPowerAsTheNumberAndItsFixedTuneInWatts() throws {
        let catalog = try Self.catalog(Self.anan)
        let transmit = try #require(catalog.board.transmit)
        let wire = try #require(try Self.board(Self.anan)["transmit"] as? [String: Any])
        let power = try #require(transmit.power)
        #expect(power.range == Self.range(wire["power"]))
        #expect(power.text(0) == "0" && power.text(50) == "50" && power.text(100) == "100")
        let tune = try #require(transmit.tunePowerForTxBand)
        #expect(tune.range == Self.range(wire["tunePowerForTxBand"]))
        #expect(tune.text(37) == "37")
        let fixed = try #require(transmit.tunePower)
        #expect(fixed.text(50) == "50 W")
        #expect(transmit.micGainDb == Self.range(wire["micGainDb"]))
        #expect(catalog.board.rx1Preamp == false)
        let relays = try #require(catalog.board.relays)
        #expect(relays.rxOutOnTx == false && relays.rxOutOverride == false)
        #expect(relays.ext1OutOnTx != nil && relays.ext2OutOnTx != nil)
    }

    /// The Hermes Lite 2's readouts, as the link document's examples give them.
    @Test func aHermesLite2ShowsItsPowerAndTuneInDecibels() throws {
        let catalog = try Self.catalog(Self.hermesLite)
        let transmit = try #require(catalog.board.transmit)
        let wire = try #require(try Self.board(Self.hermesLite)["transmit"] as? [String: Any])
        let power = try #require(transmit.power)
        #expect(power.range == Self.range(wire["power"]))
        #expect(power.range.max < 100 && power.range.step > 1)
        #expect(power.shown?.unit == "dB" && power.shown?.rounding == .halfEven && power.shown?.endSnap != nil)
        // A drive of 3 shows -7.5 dB, 87 shows -0.5 dB; the top shows 0.0.
        #expect(power.text(3) == "-7.5 dB")
        #expect(power.text(87) == "-0.5 dB")
        #expect(power.text(power.range.max) == "0.0 dB")
        #expect(power.text(88) == "0.0 dB")
        // Tune: 2 shows -16.5 dB, 97 shows 0.0.
        let tune = try #require(transmit.tunePowerForTxBand)
        #expect(tune.text(2) == "-16.5 dB")
        #expect(tune.text(97) == "0.0 dB")
        // The fixed tune spinbox takes the step below: 5 shows -16.0 dB.
        let fixed = try #require(transmit.tunePower)
        #expect(fixed.shown?.rounding == .down)
        #expect(fixed.text(5) == "-16.0 dB")
        #expect(catalog.board.rx1Preamp == false)
        #expect(catalog.board.relays == StationCatalog.Relays(rxOutOnTx: false, ext1OutOnTx: nil, ext2OutOnTx: nil,
                                                               rxOutOverride: false))
    }

    @Test func halfEvenTakesAHalfToTheEvenStep() {
        let range = StationCatalog.TransmitRange(
            min: 0, max: 90, step: 6,
            shown: StationCatalog.Shown(min: -7.5, max: 0, decimals: 1, unit: "dB", rounding: .halfEven))
        // 9 is half way between 6 (step 1) and 12 (step 2): the even step, 12.
        #expect(range.shownValue(9) == range.shownValue(12))
        // 15 is half way between 12 (step 2) and 18 (step 3): 12.
        #expect(range.shownValue(15) == range.shownValue(12))
        #expect(range.text(15) == "-6.5 dB")
    }

    // MARK: Absent and malformed

    @Test func anOlderCoresBoardHasNoRangesAndSaysNothingOfItsRelays() throws {
        var raw = try Self.object(Self.anan)
        var board = try #require(raw["board"] as? [String: Any])
        board.removeValue(forKey: "transmit")
        board.removeValue(forKey: "rx1Preamp")
        board.removeValue(forKey: "relays")
        raw["board"] = board
        raw.removeValue(forKey: "noiseReduction")
        let catalog = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(catalog.board.transmit == nil && catalog.board.rx1Preamp == nil && catalog.board.relays == nil)
        #expect(catalog.noiseReduction == nil)
    }

    @Test func anUnreadableLaterMemberCostsOnlyItself() throws {
        var raw = try Self.object(Self.hermesLite)
        let plain = try #require(StationCatalog.parse(json: try Self.text(raw)))
        var board = try #require(raw["board"] as? [String: Any])
        var transmit = try #require(board["transmit"] as? [String: Any])
        transmit["power"] = ["min": "none"]
        board["transmit"] = transmit
        board["rx1Preamp"] = "yes"
        board["relays"] = ["rxOutOnTx": 1]
        raw["board"] = board
        var nr = try #require(raw["noiseReduction"] as? [String: Any])
        nr["nr2"] = [["property": "nr2Post2Factor", "label": "Factor", "kind": "slider"]]
        var nr1 = try #require(nr["nr1"] as? [[String: Any]])
        nr1.append(["property": "aLaterControl", "label": "Later", "kind": "dial"])
        nr["nr1"] = nr1
        raw["noiseReduction"] = nr
        let read = try #require(StationCatalog.parse(json: try Self.text(raw)))
        #expect(read.board.transmit?.power == nil)
        #expect(read.board.transmit?.tunePowerForTxBand == plain.board.transmit?.tunePowerForTxBand)
        #expect(read.board.transmit?.micGainDb == plain.board.transmit?.micGainDb)
        #expect(read.board.rx1Preamp == nil && read.board.relays == nil)
        #expect(read.board.rxAntennas == plain.board.rxAntennas)
        // The unreadable slot is left out; a control of an unknown kind is ignored.
        #expect(read.noiseReduction?["nr2"] == nil)
        #expect(read.noiseReduction?["nr1"] == plain.noiseReduction?["nr1"])
        #expect(read.noiseReduction?["mnr"] == plain.noiseReduction?["mnr"])
        #expect(read.modes == plain.modes)
    }

    // MARK: Noise reduction

    @Test func everySlotIsDescribedWithWhatTheControlsNeed() throws {
        let catalog = try Self.catalog(Self.anan)
        let nr = try #require(catalog.noiseReduction)
        let raw = try #require(try Self.object(Self.anan)["noiseReduction"] as? [String: [[String: Any]]])
        #expect(Set(nr.slots.keys) == Set(StationCatalog.NoiseReduction.slotKeys))
        for key in StationCatalog.NoiseReduction.slotKeys {
            let controls = try #require(nr[key])
            let sent = try #require(raw[key])
            #expect(controls.map(\.property) == sent.map { $0["property"] as? String }, "\(key)")
            #expect(controls.map(\.label) == sent.map { $0["label"] as? String }, "\(key)")
        }
        // NR1's gain: the slider's position times its scale is the property,
        // and its readout is the position; a slice's default sits on the slider.
        let gain = try #require(nr["nr1"]?.first { $0.property == "nr1Gain" })
        guard case .slider(let slider) = gain.kind else {
            Issue.record("NR1 gain is not a slider")
            return
        }
        let position = slider.controlValue(slider.defaultValue)
        #expect(slider.range.min <= position && position <= slider.range.max)
        #expect(abs(slider.propertyValue(position) - slider.defaultValue) < 1e-12)
        // MNR's Floor reads in its own suffix; its Reset restores a new slice's value.
        let floor = try #require(nr["mnr"]?.first { $0.property == "mnrFloor" })
        guard case .slider(let floorSlider) = floor.kind, let reset = floorSlider.reset else {
            Issue.record("MNR floor is not a slider with a Reset")
            return
        }
        #expect(abs(floorSlider.propertyValue(reset) - floorSlider.defaultValue) < 1e-9)
        #expect(floorSlider.text(reset).hasSuffix(floorSlider.suffix))
        // NNR's model is a choice Reset keeps.
        let model = try #require(nr["nnr"]?.first { $0.property == "nnrModelSlot" })
        guard case .choice(let choices) = model.kind else {
            Issue.record("NNR's model is not a choice")
            return
        }
        #expect(choices.options.count == 2 && choices.reset == nil)
    }

    // MARK: Helpers

    static func catalog(_ fixture: String) throws -> StationCatalog {
        try #require(StationCatalog.parse(json: try StationCatalogTests.catalogueJson(fixture)))
    }

    static func object(_ fixture: String) throws -> [String: Any] {
        try StationCatalogTests.object(try StationCatalogTests.catalogueJson(fixture))
    }

    static func board(_ fixture: String) throws -> [String: Any] {
        try #require(try object(fixture)["board"] as? [String: Any])
    }

    static func text(_ object: [String: Any]) throws -> String {
        try StationCatalogTests.text(object)
    }

    static func range(_ value: Any?) -> StationCatalog.Range? {
        guard let object = value as? [String: Any], let min = (object["min"] as? NSNumber)?.doubleValue,
              let max = (object["max"] as? NSNumber)?.doubleValue,
              let step = (object["step"] as? NSNumber)?.doubleValue else {
            return nil
        }
        return StationCatalog.Range(min: min, max: max, step: step)
    }
}
