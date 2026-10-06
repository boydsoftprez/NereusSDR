// NereusSDR for iOS: one radio the Core can see, read from the Core's stationRadios record
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// One radio the Core can see (link document section 7.7, stream
/// `stationRadios`, at `stationRadiosVersion` 1): the Core's radio first,
/// each keyed by its MAC in upper case. The Radio tab's Manage Radios lists
/// them and asks the Core to run one of them (`station.selectRadio`), to
/// look again (`station.rescanRadios`) or to forget one
/// (`station.forgetRadio`). With `radioModelsVersion` 1 each also names
/// its model (`modelLabel`) and the models its board can run as (`models`),
/// which `station.setRadioModel` takes.
///
/// Reading is tolerant: a text that is missing reads as empty, and a
/// number that is missing or not a whole number reads as nil, never as a
/// made-up value, so a record from a newer or older Core still lists.
public struct StationRadio: Sendable, Equatable, Identifiable {
    /// The radio's MAC in upper case (the record's id).
    public var id: String
    /// The MAC as the Core sends it; the same as `id`.
    public var mac: String
    /// The name the radio reports for itself.
    public var name: String
    /// The model number the Core runs it as (its `hpsdrModel`); nil when not sent.
    public var model: Int64?
    /// Its IP address; empty when the Core does not know it.
    public var address: String
    /// 1 or 2; nil when not sent.
    public var protocolVersion: Int64?
    /// The Core runs this radio now.
    public var inUse: Bool
    /// The name the desktop's Setup shows for `model`; empty when the Core
    /// does not send it (before `radioModelsVersion` 1).
    public var modelLabel: String
    /// Every model this radio's board can run as, in the desktop model
    /// choice's order: exactly what `station.setRadioModel` accepts for it.
    /// Nil when the Core does not send the list (before `radioModelsVersion` 1).
    public var models: [ModelChoice]?

    /// One model a radio can run as.
    public struct ModelChoice: Sendable, Equatable, Hashable {
        public var model: Int64
        public var label: String

        public init(model: Int64, label: String) {
            self.model = model
            self.label = label
        }
    }

    /// The stream's name, and the most records it holds.
    public static let streamName = "stationRadios"
    public static let capacity = 64
    /// The capability under which the Core lets an app choose its radio.
    public static let capabilityName = "stationRadiosVersion"
    /// Makes a radio the Core's.
    public static let selectVerb = "station.selectRadio"
    /// Looks for radios again.
    public static let rescanVerb = "station.rescanRadios"
    /// Removes a radio's saved entry.
    public static let forgetVerb = "station.forgetRadio"
    /// Sets the model the Core runs a radio as.
    public static let setModelVerb = "station.setRadioModel"
    /// `radio`'s property: why the Core has no radio, in its words.
    public static let waitingProperty = "stationRadioWaiting"
    /// The feature a device declares in its hello for `modelLabel` and
    /// `models`, and the capability the Core answers with (1 on a Core that
    /// keeps the radio list, 0 otherwise).
    public static let modelsFeatureName = "radioModels"
    public static let modelsCapabilityName = "radioModelsVersion"

    public init(id: String, mac: String? = nil, name: String = "", model: Int64? = nil, address: String = "",
                protocolVersion: Int64? = nil, inUse: Bool = false, modelLabel: String = "",
                models: [ModelChoice]? = nil) {
        self.id = id
        self.mac = mac ?? id
        self.name = name
        self.model = model
        self.address = address
        self.protocolVersion = protocolVersion
        self.inUse = inUse
        self.modelLabel = modelLabel
        self.models = models
    }

    /// Reads one `stationRadios` record.
    public init(record: LinkMessage.RecordBatch.Record) {
        let fields = record.fields
        func text(_ name: String) -> String {
            if case .string(let value)? = fields[name] {
                return value
            }
            return ""
        }
        func whole(_ value: LinkJSON?) -> Int64? {
            guard case .number(let value)? = value, value.isFinite, value.rounded() == value,
                  abs(value) < 9e15 else {
                return nil
            }
            return Int64(value)
        }
        func whole(_ name: String) -> Int64? {
            whole(fields[name])
        }
        // Only a whole model number with its label is a choice the phone can offer.
        var models: [ModelChoice]?
        if case .array(let items)? = fields["models"] {
            models = items.compactMap { item in
                guard case .object(let entry) = item, let model = whole(entry["model"]),
                      case .string(let label)? = entry["label"], !label.isEmpty else {
                    return nil
                }
                return ModelChoice(model: model, label: label)
            }
        }
        var inUse = false
        if case .bool(let value)? = fields["inUse"] {
            inUse = value
        }
        let mac = text("mac")
        self.init(id: record.id, mac: mac.isEmpty ? record.id : mac, name: text("name"), model: whole("model"),
                  address: text("address"), protocolVersion: whole("protocol"), inUse: inUse,
                  modelLabel: text("modelLabel"), models: models)
    }
}
