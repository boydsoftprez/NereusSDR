// NereusSDR for iOS: the Core's catalogue, the lists and ranges the app's controls draw from
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// The Core's catalogue (link document section 7.4, R-IOS-06, R-IOS-27):
/// the values the Core owns and the app shows, so each radio's controls come
/// from its own Core and the app carries no radio table of its own. Parsed
/// from the `catalog` object's `json`; nothing in it is per device.
///
/// A key or member the app does not know is ignored. Every colour is
/// upper-case `#RRGGBB`. Labels are the desktop's own words, shown as sent.
public struct StationCatalog: Equatable, Sendable {
    /// A setting's range: `{min, max, step}`.
    public struct Range: Equatable, Sendable, Decodable {
        public let min: Double
        public let max: Double
        public let step: Double

        public init(min: Double, max: Double, step: Double) {
            self.min = min
            self.max = max
            self.step = step
        }
    }

    /// One mode: `id` is the slice's `dspMode`.
    public struct Mode: Equatable, Sendable, Decodable {
        public enum Sideband: String, Equatable, Sendable, Decodable {
            case lower
            case upper
            case both
        }

        public let id: Int
        public let label: String
        public let sideband: Sideband
    }

    /// One of a mode's filter presets: slot 0 is `F1`; the edges are signed
    /// as a slice's `filterLow` and `filterHigh`.
    public struct FilterPreset: Equatable, Sendable, Decodable {
        public let slot: Int
        public let label: String
        public let lowHz: Double
        public let highHz: Double
    }

    /// One tune step, as the slice's `stepHz` takes it.
    public struct TuneStep: Equatable, Sendable, Decodable {
        public let hz: Double
        public let label: String
    }

    /// A choice with a whole-number id and a label (an AGC mode, a preamp item).
    public struct Choice: Equatable, Sendable, Decodable {
        public let id: Int
        public let label: String
    }

    /// The AGC: the modes an operator picks (`id` the slice's `agcMode`)
    /// and every range the Core gives, by the name of the setting it bounds
    /// (`thresholdDb` today).
    public struct Agc: Equatable, Sendable, Decodable {
        public let modes: [Choice]
        public let ranges: [String: Range]

        /// AGC-T's range, for the slice's `agcThreshold`.
        public var thresholdDb: Range? { ranges["thresholdDb"] }

        public init(from decoder: any Decoder) throws {
            let container = try decoder.container(keyedBy: AnyKey.self)
            modes = try container.decode([Choice].self, forKey: AnyKey("modes"))
            ranges = RangeSet.ranges(in: container, skipping: ["modes"])
        }
    }

    /// The receive settings' ranges, each named after the slice setting it bounds.
    public struct Receive: Equatable, Sendable, Decodable {
        public let ranges: [String: Range]

        /// The AF slider, in its own units.
        public var afGain: Range? { ranges["afGain"] }
        /// The SQL slider, in its own units.
        public var ssqlThresh: Range? { ranges["ssqlThresh"] }
        /// The AM squelch threshold, in dB.
        public var amsqThresh: Range? { ranges["amsqThresh"] }
        /// The FM squelch threshold, in dB.
        public var fmsqThresh: Range? { ranges["fmsqThresh"] }

        public init(from decoder: any Decoder) throws {
            ranges = RangeSet.ranges(in: try decoder.container(keyedBy: AnyKey.self), skipping: [])
        }
    }

    /// One labelled level on a gauge.
    public struct Mark: Equatable, Sendable, Decodable {
        public let label: String
        public let dbm: Double
    }

    /// The gauges the app draws.
    public struct Meters: Equatable, Sendable, Decodable {
        public struct SMeter: Equatable, Sendable, Decodable {
            public let minDbm: Double
            public let s9Dbm: Double
            public let maxDbm: Double
            public let dbPerSUnit: Double
            public let redFromDbm: Double
            public let sUnits: [Mark]
            public let overS9: [Mark]
        }

        public struct MicLevel: Equatable, Sendable, Decodable {
            public let minDb: Double
            public let maxDb: Double
            public let yellowFromDb: Double
            public let redFromDb: Double
        }

        public struct RfPower: Equatable, Sendable, Decodable {
            public let minW: Double
            public let maxW: Double
            public let ratedW: Double
            public let redFromW: Double
        }

        public struct Swr: Equatable, Sendable, Decodable {
            public let min: Double
            public let max: Double
            public let redFrom: Double
        }

        public let sMeter: SMeter
        public let micLevel: MicLevel
        public let rfPower: RfPower
        public let swr: Swr
    }

    /// The radio.
    public struct Board: Equatable, Sendable, Decodable {
        /// As `hpsdrModel` in capabilities.
        public let model: Int
        public let productLabel: String
        public let maxSlices: Int
        /// The step attenuator's range in dB; nil without one.
        public let attenuator: Range?
        /// `id` is the `stepAtt` object's `preampMode`.
        public let preampItems: [Choice]
        public let rxAntennas: [String]
        public let txAntennas: [String]
        /// The receive-only inputs by the product's own labels.
        public let rxOnlyInputs: [String]
        public let sampleRates: [Int]
        public let pureSignal: Bool
        public let paRatingW: Double
        public let micJack: Bool
        /// The transmit controls' ranges on this radio; nil from a Core that
        /// sends none (an older one), or sends them unreadably.
        public let transmit: Transmit?
        /// Whether the radio has the second receiver input's preamp; nil
        /// from a Core that does not say.
        public let rx1Preamp: Bool?
        /// The antenna relays the radio has; nil from a Core that does not say.
        public let relays: Relays?
        /// RX2's own input attenuator in dB (link 7.4, `rx2AttenuatorVersion`
        /// 1): the second ADC's step attenuator, written as `stepAtt`'s
        /// `rx2AttenuationDb` with `rx2StepAttEnabled` true; nil on a radio
        /// without one, and from a Core that does not send it.
        public let rx2Attenuator: Range?
        /// RX2's own preamp choices where RX2's input has two states instead
        /// of an attenuator; `id` is `stepAtt`'s `rx2PreampMode`. Empty where
        /// the radio has none; nil from a Core that does not send the list.
        public let rx2PreampItems: [Choice]?
        /// Why RX2 has no input control of its own, in the Core's words, when
        /// the radio has neither; nil otherwise.
        public let rx2AttenuatorReason: String?
        /// Whether the operator can choose the radio's own mic input on this
        /// radio (link 7.4, `radioMicVersion` 1); nil from a Core that does
        /// not say.
        public let radioMic: Bool?
        /// What the radio's mic input needs, in the Core's words, when it
        /// depends on hardware the radio cannot report (the Hermes Lite 2's
        /// audio add-on board); nil otherwise.
        public let radioMicNote: String?

        enum CodingKeys: String, CodingKey {
            case model, productLabel, maxSlices, attenuator, preampItems, rxAntennas, txAntennas, rxOnlyInputs
            case sampleRates, pureSignal, paRatingW, micJack, transmit, rx1Preamp, relays
            case rx2Attenuator, rx2PreampItems, rx2AttenuatorReason
            case radioMic, radioMicNote
        }

        public init(from decoder: any Decoder) throws {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            model = try container.decode(Int.self, forKey: .model)
            productLabel = try container.decode(String.self, forKey: .productLabel)
            maxSlices = try container.decode(Int.self, forKey: .maxSlices)
            attenuator = try container.decode(Range?.self, forKey: .attenuator)
            preampItems = try container.decode([Choice].self, forKey: .preampItems)
            rxAntennas = try container.decode([String].self, forKey: .rxAntennas)
            txAntennas = try container.decode([String].self, forKey: .txAntennas)
            rxOnlyInputs = try container.decode([String].self, forKey: .rxOnlyInputs)
            sampleRates = try container.decode([Int].self, forKey: .sampleRates)
            pureSignal = try container.decode(Bool.self, forKey: .pureSignal)
            paRatingW = try container.decode(Double.self, forKey: .paRatingW)
            micJack = try container.decode(Bool.self, forKey: .micJack)
            // Later members a Core may add (link 7.4): each read on its own,
            // so an older Core's board still reads and an unreadable one
            // costs only itself.
            transmit = StationCatalog.later(Transmit.self, in: container, forKey: .transmit)
            rx1Preamp = StationCatalog.later(Bool.self, in: container, forKey: .rx1Preamp)
            relays = StationCatalog.later(Relays.self, in: container, forKey: .relays)
            rx2Attenuator = StationCatalog.later(Range.self, in: container, forKey: .rx2Attenuator)
            rx2PreampItems = StationCatalog.later([Choice].self, in: container, forKey: .rx2PreampItems)
            rx2AttenuatorReason = StationCatalog.later(String.self, in: container, forKey: .rx2AttenuatorReason)
            radioMic = StationCatalog.later(Bool.self, in: container, forKey: .radioMic)
            radioMicNote = StationCatalog.later(String.self, in: container, forKey: .radioMicNote)
        }
    }

    /// How a transmit control takes a value between its steps before it
    /// shows it (link 7.4, `board.transmit.*.shown.rounding`).
    public enum Rounding: String, Equatable, Sendable {
        /// The nearest step, a half to the even step.
        case halfEven
        /// The step at or below.
        case down
        /// No step: the value as it is.
        case none
    }

    /// What a transmit control shows at its two ends, linear in between.
    public struct Shown: Equatable, Sendable, Decodable {
        /// A value below `below` shows as the control's `min`, one above
        /// `above` as its `max`.
        public struct EndSnap: Equatable, Sendable, Decodable {
            public let below: Double
            public let above: Double

            public init(below: Double, above: Double) {
                self.below = below
                self.above = above
            }
        }

        public let min: Double
        public let max: Double
        public let decimals: Int
        /// `""`, `W` or `dB`.
        public let unit: String
        public let rounding: Rounding
        public let endSnap: EndSnap?

        enum CodingKeys: String, CodingKey {
            case min, max, decimals, unit, rounding, endSnap
        }

        public init(min: Double, max: Double, decimals: Int, unit: String, rounding: Rounding = .none,
                    endSnap: EndSnap? = nil) {
            self.min = min
            self.max = max
            self.decimals = decimals
            self.unit = unit
            self.rounding = rounding
            self.endSnap = endSnap
        }

        public init(from decoder: any Decoder) throws {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            min = try container.decode(Double.self, forKey: .min)
            max = try container.decode(Double.self, forKey: .max)
            decimals = try container.decode(Int.self, forKey: .decimals)
            unit = try container.decode(String.self, forKey: .unit)
            // A Core from before `rounding` and `endSnap` sends neither; a
            // rounding this app does not know takes the value as it is.
            let rule = try container.decodeIfPresent(String.self, forKey: .rounding)
            rounding = rule.flatMap(Rounding.init(rawValue:)) ?? .none
            endSnap = try container.decodeIfPresent(EndSnap.self, forKey: .endSnap)
        }
    }

    /// One transmit control's range, in its property's own units, and what
    /// it shows (the power controls).
    public struct TransmitRange: Equatable, Sendable, Decodable {
        public let min: Double
        public let max: Double
        public let step: Double
        /// Nil where the control shows its value as it is.
        public let shown: Shown?

        public init(min: Double, max: Double, step: Double, shown: Shown? = nil) {
            self.min = min
            self.max = max
            self.step = step
            self.shown = shown
        }

        /// The range the control moves over.
        public var range: Range {
            Range(min: min, max: max, step: step)
        }

        /// What the control shows for `value`, in `shown`'s units: first
        /// `endSnap`, then `rounding` to a step, then linear between the
        /// ends. Nil without `shown`.
        public func shownValue(_ value: Double) -> Double? {
            guard let shown else {
                return nil
            }
            var v = value
            if let snap = shown.endSnap {
                if v < snap.below {
                    v = min
                } else if v > snap.above {
                    v = max
                }
            }
            if step > 0 {
                switch shown.rounding {
                case .halfEven:
                    v = min + step * ((v - min) / step).rounded(.toNearestOrEven)
                case .down:
                    v = min + step * ((v - min) / step).rounded(.down)
                case .none:
                    break
                }
            }
            guard max > min else {
                return shown.min
            }
            return shown.min + (v - min) * (shown.max - shown.min) / (max - min)
        }

        /// The control's readout for `value`: `shown`'s number to its
        /// places and its unit ("-7.5 dB", "50 W", "50"); the whole value
        /// without `shown`.
        public func text(_ value: Double) -> String {
            guard let shown, let number = shownValue(value) else {
                return String(Int(value.rounded()))
            }
            var digits = String(format: "%.\(Swift.max(shown.decimals, 0))f", number)
            // No "-0.0".
            if digits.hasPrefix("-"), Double(digits) == 0 {
                digits.removeFirst()
            }
            return shown.unit.isEmpty ? digits : digits + " " + shown.unit
        }
    }

    /// The transmit controls' ranges on this radio (link 7.4,
    /// `board.transmit`), each nil where the Core sends none or sends it
    /// unreadably.
    public struct Transmit: Equatable, Sendable, Decodable {
        /// The TX panel's RF Power (`power`).
        public let power: TransmitRange?
        /// The TX panel's Tune Pwr (`tunePowerForTxBand`).
        public let tunePowerForTxBand: TransmitRange?
        /// Setup's fixed tune power (`tunePower`).
        public let tunePower: TransmitRange?
        /// The mic level (`micGainDb`), in dB. The Core takes a wider range.
        public let micGainDb: Range?

        enum CodingKeys: String, CodingKey {
            case power, tunePowerForTxBand, tunePower, micGainDb
        }

        public init(power: TransmitRange?, tunePowerForTxBand: TransmitRange?, tunePower: TransmitRange?,
                    micGainDb: Range?) {
            self.power = power
            self.tunePowerForTxBand = tunePowerForTxBand
            self.tunePower = tunePower
            self.micGainDb = micGainDb
        }

        public init(from decoder: any Decoder) throws {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            power = StationCatalog.later(TransmitRange.self, in: container, forKey: .power)
            tunePowerForTxBand = StationCatalog.later(TransmitRange.self, in: container, forKey: .tunePowerForTxBand)
            tunePower = StationCatalog.later(TransmitRange.self, in: container, forKey: .tunePower)
            micGainDb = StationCatalog.later(Range.self, in: container, forKey: .micGainDb)
        }
    }

    /// The antenna relays the radio has (link 7.4, `board.relays`). The
    /// desktop hides a control for a relay the radio lacks, and so does the
    /// app.
    public struct Relays: Equatable, Sendable, Decodable {
        /// RX out on TX: the RX bypass relay (the flag's BYPS).
        public let rxOutOnTx: Bool
        /// The Ext-on-TX switches' labels; nil where the radio has none.
        public let ext1OutOnTx: String?
        public let ext2OutOnTx: String?
        /// The RX out override.
        public let rxOutOverride: Bool

        public init(rxOutOnTx: Bool, ext1OutOnTx: String?, ext2OutOnTx: String?, rxOutOverride: Bool) {
            self.rxOutOnTx = rxOutOnTx
            self.ext1OutOnTx = ext1OutOnTx
            self.ext2OutOnTx = ext2OutOnTx
            self.rxOutOverride = rxOutOverride
        }
    }

    /// One noise-reduction quick control (link 7.4, `noiseReduction`): the
    /// slice property it writes, its label, and how it maps to the property.
    public struct NrControl: Equatable, Sendable, Identifiable {
        /// A slider: `min`, `max`, `step` in the control's own units; the
        /// property is the control value times `scale`; the readout is the
        /// control value over `divide` to `decimals` places, then `suffix`.
        public struct Slider: Equatable, Sendable {
            public let min: Double
            public let max: Double
            public let step: Double
            public let scale: Double
            public let divide: Double
            public let decimals: Int
            public let suffix: String
            /// A new slice's value, in the property's own units.
            public let defaultValue: Double
            /// The control value Reset restores; nil where it has none.
            public let reset: Double?

            public init(min: Double, max: Double, step: Double, scale: Double, divide: Double, decimals: Int,
                        suffix: String, defaultValue: Double, reset: Double?) {
                self.min = min
                self.max = max
                self.step = step
                self.scale = scale
                self.divide = divide
                self.decimals = decimals
                self.suffix = suffix
                self.defaultValue = defaultValue
                self.reset = reset
            }

            /// The range the control moves over, in its own units.
            public var range: Range {
                Range(min: min, max: max, step: step)
            }

            /// The control value that shows `property` (the property's units).
            public func controlValue(_ property: Double) -> Double {
                scale != 0 ? property / scale : property
            }

            /// The property value the control value writes.
            public func propertyValue(_ control: Double) -> Double {
                control * scale
            }

            /// The readout for a control value.
            public func text(_ control: Double) -> String {
                let number = divide != 0 ? control / divide : control
                var digits = String(format: "%.\(Swift.max(decimals, 0))f", number)
                if digits.hasPrefix("-"), Double(digits) == 0 {
                    digits.removeFirst()
                }
                return digits + suffix
            }
        }

        /// A choice: `id` is the property's value.
        public struct Choices: Equatable, Sendable {
            public let options: [Choice]
            public let defaultId: Int
            /// The id Reset restores; nil where Reset keeps it.
            public let reset: Int?

            public init(options: [Choice], defaultId: Int, reset: Int?) {
                self.options = options
                self.defaultId = defaultId
                self.reset = reset
            }
        }

        public enum Kind: Equatable, Sendable {
            case slider(Slider)
            /// On or off; its default.
            case toggle(Bool)
            case choice(Choices)
        }

        public let property: String
        public let label: String
        public let kind: Kind

        public var id: String { property }

        public init(property: String, label: String, kind: Kind) {
            self.property = property
            self.label = label
            self.kind = kind
        }
    }

    /// The noise-reduction quick controls, slot by slot (link 7.4). The
    /// same on every radio.
    public struct NoiseReduction: Equatable, Sendable {
        /// The slots in the VFO flag's order, as the catalogue keys them.
        public static let slotKeys = ["nr1", "nr2", "nr3", "nr4", "dfnr", "mnr", "nnr"]

        /// Each slot's controls in the flag's order, by its key; a slot the
        /// Core does not describe, or describes unreadably, is absent.
        public let slots: [String: [NrControl]]

        public init(slots: [String: [NrControl]]) {
            self.slots = slots
        }

        /// A slot's controls; nil where the Core does not describe it.
        public subscript(slot: String) -> [NrControl]? {
            slots[slot]
        }
    }

    /// One segment of a band plan.
    public struct Segment: Equatable, Sendable, Decodable {
        public let lowHz: Double
        public let highHz: Double
        public let label: String
        /// The licence classes (`E,G`); empty for a beacon or no transmit.
        public let licence: String
        /// The lowest class the segment allows (`Tech`, `General`, `Extra`),
        /// which the strip names after the label; empty when none applies
        /// or when an older Core does not send it.
        public let lowestClass: String
        public let colour: String

        enum CodingKeys: String, CodingKey {
            case lowHz, highHz, label, licence, lowestClass, colour
        }

        init(lowHz: Double, highHz: Double, label: String, licence: String, lowestClass: String, colour: String) {
            self.lowHz = lowHz
            self.highHz = highHz
            self.label = label
            self.licence = licence
            self.lowestClass = lowestClass
            self.colour = colour
        }

        public init(from decoder: any Decoder) throws {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            lowHz = try container.decode(Double.self, forKey: .lowHz)
            highHz = try container.decode(Double.self, forKey: .highHz)
            label = try container.decode(String.self, forKey: .label)
            licence = try container.decode(String.self, forKey: .licence)
            // Link document section 7.4: a Core from before lowestClass sends
            // none, and its catalogue still reads.
            lowestClass = try container.decodeIfPresent(String.self, forKey: .lowestClass) ?? ""
            colour = try container.decode(String.self, forKey: .colour)
        }
    }

    /// One bundled band plan.
    public struct BandPlan: Equatable, Sendable, Decodable {
        /// One of the plan's spots (a calling frequency, a beacon): drawn as
        /// a dot on the strip, without its label, as the desktop does.
        public struct Spot: Equatable, Sendable, Decodable {
            public let hz: Double
            public let label: String

            public init(hz: Double, label: String) {
                self.hz = hz
                self.label = label
            }
        }

        public let id: String
        public let name: String
        public let isDefault: Bool
        /// The plan the Core shows now. A Core from before `active` sends
        /// none, and no plan is active: the phone then goes by the Core's
        /// `BandPlanName` setting.
        public let isActive: Bool
        public let segments: [Segment]
        /// Empty from a Core that sends none, or sends them unreadably.
        public let spots: [Spot]

        enum CodingKeys: String, CodingKey {
            case id, name, segments, spots
            case isDefault = "default"
            case isActive = "active"
        }

        init(id: String, name: String, isDefault: Bool, isActive: Bool, segments: [Segment], spots: [Spot]) {
            self.id = id
            self.name = name
            self.isDefault = isDefault
            self.isActive = isActive
            self.segments = segments
            self.spots = spots
        }

        public init(from decoder: any Decoder) throws {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            id = try container.decode(String.self, forKey: .id)
            name = try container.decode(String.self, forKey: .name)
            isDefault = try container.decode(Bool.self, forKey: .isDefault)
            segments = try container.decode([Segment].self, forKey: .segments)
            // Later members a Core may add: an older Core's plan still reads,
            // and unreadable spots cost only the dots.
            isActive = (try? container.decodeIfPresent(Bool.self, forKey: .isActive)) ?? false
            do {
                spots = try container.decodeIfPresent([Spot].self, forKey: .spots) ?? []
            } catch {
                StationCatalog.logger.warning("A band plan's spots could not be read; they are left out")
                spots = []
            }
        }
    }

    /// One waterfall palette: `id` is the desktop's palette number.
    public struct Palette: Equatable, Sendable, Decodable {
        public struct Stop: Equatable, Sendable, Decodable {
            /// From 0 to 1.
            public let at: Double
            public let colour: String
        }

        public let id: Int
        public let name: String
        public let stops: [Stop]
    }

    /// One of the desktop's Tools menu items; `where` is `station` (works at
    /// the Core) or `both`.
    public struct Tool: Equatable, Sendable, Decodable {
        public let id: String
        public let label: String
        public let `where`: String
        public let offered: Bool
    }

    /// One of the desktop's Radio menu items.
    public struct RadioItem: Equatable, Sendable, Decodable {
        public let id: String
        public let label: String
        public let offered: Bool
    }

    /// `audio` (link 7.4): the Opus profiles the Core has measured, in its
    /// order. An older Core sends an empty `audio`, read as nil profiles.
    public struct Audio: Equatable, Sendable, Decodable {
        /// One measured Opus profile: the bitrate a device may ask for as
        /// its `opusBitrate`, and the audio bandwidth it carries.
        public struct OpusProfile: Equatable, Sendable, Decodable {
            public let bitrate: Int
            public let bandwidthHz: Int

            public init(bitrate: Int, bandwidthHz: Int) {
                self.bitrate = bitrate
                self.bandwidthHz = bandwidthHz
            }
        }

        /// nil from a Core that sends no `opusProfiles`.
        public let opusProfiles: [OpusProfile]?

        public init(opusProfiles: [OpusProfile]? = nil) {
            self.opusProfiles = opusProfiles
        }
    }

    /// One button of the band grid: `id` is the Core's band number, the
    /// value a slice's `band` carries; `label` is the desktop grid's text.
    public struct Band: Equatable, Sendable, Decodable {
        public let id: Int
        public let label: String

        public init(id: Int, label: String) {
            self.id = id
            self.label = label
        }
    }

    public let modes: [Mode]
    /// The Core's Setup > Display description (link 7.4, `display`); nil
    /// from a Core that sends none, or sends it unreadably. See ``Display``.
    public let display: Display?
    /// Each mode's presets, keyed by the mode's label (`USB`), not its id.
    public let filterPresets: [String: [FilterPreset]]
    /// Smallest first, as the Core sends them.
    public let tuneSteps: [TuneStep]
    public let agc: Agc
    public let receive: Receive
    public let meters: Meters
    public let board: Board
    public let bandPlans: [BandPlan]
    public let palettes: [Palette]
    /// One colour for each slice the radio allows, slice A's first.
    public let sliceColours: [String]
    /// Every tool in the desktop's order, offered or not.
    public let tools: [Tool]
    /// Every Radio menu item in the desktop's order, offered or not.
    public let radioItems: [RadioItem]
    public let audio: Audio
    /// The band grid's buttons in the desktop grid's order; nil from a Core
    /// that sends no `bands` (an older one), or sends them unreadably.
    public let bands: [Band]?
    /// The noise-reduction quick controls; nil from a Core that sends none
    /// (an older one), or sends them unreadably.
    public let noiseReduction: NoiseReduction?

    /// The tools the app shows: only offered ones, in their place (D41).
    public var offeredTools: [Tool] { tools.filter(\.offered) }
    /// The Radio menu items the app shows: only offered ones, in their place.
    public var offeredRadioItems: [RadioItem] { radioItems.filter(\.offered) }
    /// The band plan marked `default`.
    public var defaultBandPlan: BandPlan? { bandPlans.first(where: \.isDefault) }

    private static let logger = Logger(subsystem: "NereusSDR", category: "catalog")

    /// The catalogue in `json`, or nil. An empty `json` is no catalogue yet
    /// and is not an error; anything unreadable is logged, without its
    /// content, and gives nil.
    public static func parse(json: String) -> StationCatalog? {
        guard !json.isEmpty else {
            return nil
        }
        do {
            let wire = try JSONDecoder().decode(Wire.self, from: Data(json.utf8))
            return try StationCatalog(wire)
        } catch let error as DecodingError {
            let path = Self.path(of: error)
            logger.warning("The Core's catalogue could not be read at \(path, privacy: .public)")
            return nil
        } catch {
            logger.warning("The Core's catalogue could not be read: a colour is not #RRGGBB")
            return nil
        }
    }

    // MARK: Inside

    /// The thirteen keys as they travel, and the later ones a Core may add.
    private struct Wire: Decodable {
        let modes: [Mode]
        let filterPresets: [String: [FilterPreset]]
        let tuneSteps: [TuneStep]
        let agc: Agc
        let receive: Receive
        let meters: Meters
        let board: Board
        let bandPlans: [BandPlan]
        let palettes: [Palette]
        let sliceColours: [String]
        let tools: [Tool]
        let radioItems: [RadioItem]
        let audio: Audio
        /// Read on its own, so an unreadable `bands` costs only the grid.
        let bands: Lenient<[Band]>?
        /// Read on its own, so an unreadable `display` costs only the
        /// Core's display settings.
        let display: Lenient<Display>?
        /// Read on its own, so an unreadable `noiseReduction` costs only
        /// the noise-reduction settings.
        let noiseReduction: Lenient<NoiseReductionWire>?
    }

    /// A value that is nil when it does not read, rather than an error.
    private struct Lenient<Value: Decodable>: Decodable {
        let value: Value?

        init(from decoder: any Decoder) throws {
            do {
                value = try Value(from: decoder)
            } catch {
                StationCatalog.logger.warning("A later part of the Core's catalogue could not be read; it is left out")
                value = nil
            }
        }
    }

    private struct BadColour: Error {}

    /// `noiseReduction` as it travels: each slot read on its own, so an
    /// unreadable slot costs only itself; a control of a kind this app does
    /// not know is left out.
    private struct NoiseReductionWire: Decodable {
        let value: NoiseReduction

        init(from decoder: any Decoder) throws {
            let container = try decoder.container(keyedBy: AnyKey.self)
            var slots: [String: [NrControl]] = [:]
            for key in container.allKeys {
                do {
                    let controls = try container.decode([NrControlWire].self, forKey: key)
                    slots[key.stringValue] = controls.compactMap(\.value)
                } catch {
                    StationCatalog.logger.warning(
                        "The catalogue's \(key.stringValue, privacy: .public) controls could not be read; they are left out")
                }
            }
            value = NoiseReduction(slots: slots)
        }
    }

    /// One noise-reduction control as it travels; nil for a kind this app
    /// does not know.
    private struct NrControlWire: Decodable {
        let value: NrControl?

        enum CodingKeys: String, CodingKey {
            case property, label, kind, min, max, step, scale, divide, decimals, suffix, options, reset
            case defaultValue = "default"
        }

        init(from decoder: any Decoder) throws {
            let c = try decoder.container(keyedBy: CodingKeys.self)
            let property = try c.decode(String.self, forKey: .property)
            let label = try c.decode(String.self, forKey: .label)
            let kind: NrControl.Kind
            switch try c.decode(String.self, forKey: .kind) {
            case "slider":
                kind = .slider(NrControl.Slider(
                    min: try c.decode(Double.self, forKey: .min), max: try c.decode(Double.self, forKey: .max),
                    step: try c.decode(Double.self, forKey: .step), scale: try c.decode(Double.self, forKey: .scale),
                    divide: try c.decode(Double.self, forKey: .divide),
                    decimals: try c.decode(Int.self, forKey: .decimals),
                    suffix: try c.decode(String.self, forKey: .suffix),
                    defaultValue: try c.decode(Double.self, forKey: .defaultValue),
                    reset: try c.decodeIfPresent(Double.self, forKey: .reset)))
            case "switch":
                kind = .toggle(try c.decode(Bool.self, forKey: .defaultValue))
            case "choice":
                kind = .choice(NrControl.Choices(
                    options: try c.decode([Choice].self, forKey: .options),
                    defaultId: try c.decode(Int.self, forKey: .defaultValue),
                    reset: try c.decodeIfPresent(Int.self, forKey: .reset)))
            default:
                value = nil
                return
            }
            value = NrControl(property: property, label: label, kind: kind)
        }
    }

    private init(_ wire: Wire) throws {
        modes = wire.modes
        filterPresets = wire.filterPresets
        tuneSteps = wire.tuneSteps
        agc = wire.agc
        receive = wire.receive
        meters = wire.meters
        board = wire.board
        bandPlans = try wire.bandPlans.map { plan in
            BandPlan(id: plan.id, name: plan.name, isDefault: plan.isDefault, isActive: plan.isActive,
                     segments: try plan.segments.map {
                         Segment(lowHz: $0.lowHz, highHz: $0.highHz, label: $0.label, licence: $0.licence,
                                 lowestClass: $0.lowestClass, colour: try Self.colour($0.colour))
                     }, spots: plan.spots)
        }
        palettes = try wire.palettes.map { palette in
            Palette(id: palette.id, name: palette.name,
                    stops: try palette.stops.map { Palette.Stop(at: $0.at, colour: try Self.colour($0.colour)) })
        }
        sliceColours = try wire.sliceColours.map(Self.colour)
        tools = wire.tools
        radioItems = wire.radioItems
        audio = wire.audio
        bands = wire.bands?.value
        display = wire.display?.value
        noiseReduction = wire.noiseReduction?.value?.value
    }

    /// A later member of an object, read on its own: nil when absent, null
    /// or unreadable (logged, without its content).
    fileprivate static func later<Value: Decodable, Key: CodingKey>(
        _ type: Value.Type, in container: KeyedDecodingContainer<Key>, forKey key: Key) -> Value? {
        do {
            return try container.decodeIfPresent(Value.self, forKey: key)
        } catch {
            logger.warning("The catalogue's \(key.stringValue, privacy: .public) could not be read; it is left out")
            return nil
        }
    }

    /// `#RRGGBB`, in upper case; anything else throws.
    private static func colour(_ text: String) throws -> String {
        let digits = text.dropFirst()
        guard text.hasPrefix("#"), digits.count == 6, digits.allSatisfy(\.isHexDigit) else {
            throw BadColour()
        }
        return text.uppercased()
    }

    /// Where in the JSON a decoding error happened, by key names only.
    private static func path(of error: DecodingError) -> String {
        let context: DecodingError.Context
        switch error {
        case .typeMismatch(_, let found), .valueNotFound(_, let found), .dataCorrupted(let found):
            context = found
        case .keyNotFound(let key, let found):
            return (found.codingPath + [key]).map(\.stringValue).joined(separator: ".")
        @unknown default:
            return "an unknown place"
        }
        let path = context.codingPath.map(\.stringValue).joined(separator: ".")
        return path.isEmpty ? "the top" : path
    }
}

/// Any JSON object key.
private struct AnyKey: CodingKey {
    let stringValue: String
    var intValue: Int? { nil }

    init(_ name: String) {
        stringValue = name
    }

    init?(stringValue: String) {
        self.stringValue = stringValue
    }

    init?(intValue: Int) {
        nil
    }
}

/// Reads every member of an object that is a `{min, max, step}`, by name.
private enum RangeSet {
    static func ranges(in container: KeyedDecodingContainer<AnyKey>,
                       skipping: Set<String>) -> [String: StationCatalog.Range] {
        var ranges: [String: StationCatalog.Range] = [:]
        for key in container.allKeys where !skipping.contains(key.stringValue) {
            // A member of another shape is one this app does not know: ignored.
            if let range = try? container.decode(StationCatalog.Range.self, forKey: key) {
                ranges[key.stringValue] = range
            }
        }
        return ranges
    }
}
