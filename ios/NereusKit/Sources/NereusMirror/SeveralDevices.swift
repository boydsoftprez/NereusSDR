// NereusSDR for iOS: what the Core says about the other devices on it: who is there, their slices, its questions and its notices
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// Several devices on one Core (R-IOS-17, R-IOS-30; the several-devices
/// design, section 10; the link document, sections 7.1 and 7.5), read from
/// the Core's own words:
///
/// - ``ConnectedDevice``: an entry of `connectedDevices.listJson`, who is
///   on the Core now;
/// - ``SliceMarker``: a `marker:<id>` object, another device's slice;
/// - ``Question``: a `confirm.request`, asked before a change that reaches
///   another device and before a take;
/// - ``Notice``: a `notice`, told afterwards;
/// - ``Readback``: what a `confirm.proceed` answer says the change settled at.
///
/// Every name here is the Core's (numbered by it where two collide), and
/// every duration is one the Core measured when it sent it; the app turns
/// a `secondsAgo` into a time of day by its own clock.
///
/// The fifth device's question (`session.held`, `session.takeover`) is not
/// here: the link does not carry it yet (the link document, section 12.4,
/// "a later version's").
public enum SeveralDevices {
    /// The capability that says the Core shares itself with this device.
    public static let capability = "sessionHolderVersion"
    /// The agreed minor its objects, messages and verbs need.
    public static let minimumMinor: UInt16 = 11
    /// The mirrored class of another device's slice, and its key's prefix.
    public static let markerClass = "SliceMarker"
    public static let markerPrefix = "marker:"
    /// The object that lists who is on the Core.
    public static let connectedDevicesKey = "connectedDevices"
    /// The verbs that answer a question and take a slice back.
    public static let proceedVerb = "confirm.proceed"
    public static let cancelVerb = "confirm.cancel"
    public static let takeBackVerb = "notice.takeBack"
    /// The `choice` a question without choices is answered with.
    public static let noChoice: Int64 = -1
    /// The Core's answer to a change it holds for a question (section 7.3).
    public static let waitingReason = "Waiting for you to confirm."
    /// The `phase` that answer carries (sections 7.5 and 18.9).
    public static let needsConfirmationPhase = "needsConfirmation"

    /// The Core holds the change for a question: `phase`
    /// `needsConfirmation` where it sends one, else its words.
    public static func waitsForConfirmation(_ result: CommandResult) -> Bool {
        result.phase == needsConfirmationPhase || result.reason == waitingReason
    }

    /// Whether the Core shares itself with this device: it declared
    /// `sessionHolderVersion` 1 at agreed minor 11 or later.
    @MainActor
    public static func available(in store: MirrorStore) -> Bool {
        (store.agreedMinor ?? 0) >= minimumMinor && store.capabilityVersion(capability) >= 1
    }

    // MARK: Devices

    /// What a device is doing, as the Core says it.
    public enum DeviceState: Equatable, Sendable {
        case listening
        case transmitting
        case away
        /// A state this app does not know yet.
        case other(String)

        public init(wireName: String) {
            switch wireName {
            case "listening": self = .listening
            case "transmitting": self = .transmitting
            case "away": self = .away
            default: self = .other(wireName)
            }
        }
    }

    /// One slice a device listens on, as `listeningOn` names it.
    public struct ListeningSlice: Equatable, Sendable {
        public var sliceId: Int
        public var letter: String
        /// The `Band` value, as a slice's `band` carries it.
        public var band: Int
        /// The `dspMode` value.
        public var mode: Int
        /// In hertz, where the Core sends it (`session.held`).
        public var frequencyHz: Double?
    }

    /// One device with a session on the Core, live or away.
    public struct ConnectedDevice: Equatable, Sendable, Identifiable {
        public var deviceId: String
        public var name: String
        public var shortName: String
        /// `phone`, `tablet`, `computer` or `station`.
        public var kind: String
        public var paired: Bool
        public var hostsCore: Bool
        public var revocable: Bool
        public var state: DeviceState
        public var holdsTransmit: Bool
        public var lastActivitySeconds: Int64
        public var connectedForSeconds: Int64
        public var awayForSeconds: Int64
        public var transmittingForSeconds: Int64
        public var listeningOn: [ListeningSlice]
        public var transmittingOn: ListeningSlice?

        public var id: String { deviceId }
    }

    /// Who is on the Core now, in the order they were let in, from
    /// `connectedDevices.listJson`; empty when the text is not the list.
    public static func connectedDevices(fromListJson text: String) -> [ConnectedDevice] {
        guard case .array(let entries)? = try? LinkJSON.parse(text) else {
            return []
        }
        return entries.compactMap { entry in
            guard case .object(let o) = entry, let id = o.string("deviceId") else {
                return nil
            }
            return ConnectedDevice(deviceId: id, name: o.string("name") ?? "", shortName: o.string("shortName") ?? "",
                                   kind: o.string("kind") ?? "", paired: o.bool("paired") ?? false,
                                   hostsCore: o.bool("hostsCore") ?? false, revocable: o.bool("revocable") ?? false,
                                   state: DeviceState(wireName: o.string("state") ?? ""),
                                   holdsTransmit: o.bool("holdsTransmit") ?? false,
                                   lastActivitySeconds: o.whole("lastActivitySeconds") ?? 0,
                                   connectedForSeconds: o.whole("connectedForSeconds") ?? 0,
                                   awayForSeconds: o.whole("awayForSeconds") ?? 0,
                                   transmittingForSeconds: o.whole("transmittingForSeconds") ?? 0,
                                   listeningOn: o.array("listeningOn").compactMap(listeningSlice),
                                   transmittingOn: o["transmittingOn"].flatMap(listeningSlice))
        }
    }

    /// Who is on the Core now, from the mirror's `connectedDevices`.
    @MainActor
    public static func connectedDevices(in store: MirrorStore) -> [ConnectedDevice] {
        guard case .text(let text)? = store.object(connectedDevicesKey)?["listJson"] else {
            return []
        }
        return connectedDevices(fromListJson: text)
    }

    private static func listeningSlice(_ json: LinkJSON) -> ListeningSlice? {
        guard case .object(let o) = json, let id = o.whole("sliceId") else {
            return nil
        }
        return ListeningSlice(sliceId: Int(id), letter: o.string("letter") ?? "", band: Int(o.whole("band") ?? -1),
                              mode: Int(o.whole("mode") ?? -1), frequencyHz: o.number("frequencyHz"))
    }

    // MARK: Markers

    /// Another device's slice, from its `marker:<id>` (ruling 5.4).
    public struct SliceMarker: Equatable, Sendable, Identifiable {
        public var sliceId: Int
        public var ownerDeviceId: String
        public var ownerName: String
        public var ownerShortName: String
        public var ownerKind: String
        /// The owner is away, or the slice is held for it.
        public var ownerAway: Bool
        public var frequencyHz: Double
        /// The `dspMode` value.
        public var mode: Int
        public var filterLowHz: Double
        public var filterHighHz: Double
        /// Its owner holds transmit and transmits on it (ruling 5.4a).
        public var txSlice: Bool
        public var band: Int
        /// The receiver it sits on, from 0; -1 on none. Shown plus one.
        public var streamIndex: Int
        public var psPaused: Bool

        public var id: Int { sliceId }

        /// 'A' + its id, on every device.
        public var letter: String {
            guard sliceId >= 0, sliceId < 26, let scalar = Unicode.Scalar(UInt32(65 + sliceId)) else {
                return "?"
            }
            return String(Character(scalar))
        }
    }

    /// The marker a mirrored object holds, or nil when it is not a marker.
    @MainActor
    public static func marker(_ object: MirrorObject) -> SliceMarker? {
        guard object.className == markerClass else {
            return nil
        }
        var id = object["sliceId"]?.wholeNumber.map(Int.init)
        if id == nil, object.key.hasPrefix(markerPrefix) {
            id = Int(object.key.dropFirst(markerPrefix.count))
        }
        guard let sliceId = id, let frequency = object["frequency"]?.decimal else {
            return nil
        }
        return SliceMarker(sliceId: sliceId, ownerDeviceId: object["ownerDeviceId"]?.words ?? "",
                           ownerName: object["ownerName"]?.words ?? "",
                           ownerShortName: object["ownerShortName"]?.words ?? "",
                           ownerKind: object["ownerKind"]?.words ?? "", ownerAway: object["ownerAway"]?.flag ?? false,
                           frequencyHz: frequency, mode: Int(object["dspMode"]?.wholeNumber ?? -1),
                           filterLowHz: object["filterLow"]?.decimal ?? 0,
                           filterHighHz: object["filterHigh"]?.decimal ?? 0,
                           txSlice: object["txSlice"]?.flag ?? false, band: Int(object["band"]?.wholeNumber ?? -1),
                           streamIndex: Int(object["streamIndex"]?.wholeNumber ?? -1),
                           psPaused: object["psPaused"]?.flag ?? false)
    }

    /// Every other device's slice the mirror holds, by slice id.
    @MainActor
    public static func markers(in store: MirrorStore) -> [SliceMarker] {
        store.objects(ofClass: markerClass).compactMap(marker).sorted { $0.sliceId < $1.sliceId }
    }

    // MARK: Questions

    /// What a question asks about.
    public enum QuestionKind: Equatable, Sendable {
        /// A change every device on an ADC, a receiver or the radio hears (D53).
        case sharedSetting
        /// Moving a receiver another device shares (D50).
        case panMove
        /// Taking a receiver, or one slice, from another device (D49).
        case takeReceiver
        case takeSlice
        /// Taking transmit (D51), after this device's `tx.take`.
        case takeTransmit
        /// A kind this app does not know yet.
        case other(String)

        public init(wireName: String) {
            switch wireName {
            case "sharedSetting": self = .sharedSetting
            case "panMove": self = .panMove
            case "takeReceiver": self = .takeReceiver
            case "takeSlice": self = .takeSlice
            case "takeTransmit": self = .takeTransmit
            default: self = .other(wireName)
            }
        }
    }

    /// A change as the Core words it: `label`, `from` and `to`.
    public struct Change: Equatable, Sendable {
        public var label: String
        public var from: String
        public var to: String
    }

    /// What happens to one of another device's slices.
    public enum Effect: Equatable, Sendable {
        /// It keeps receiving, differently.
        case changes
        /// It moves to a free receiver.
        case moves
        case closes
        case pausesWhileTransmitting
        case other(String)

        public init(wireName: String) {
            switch wireName {
            case "changes": self = .changes
            case "moves": self = .moves
            case "closes": self = .closes
            case "pausesWhileTransmitting": self = .pausesWhileTransmitting
            default: self = .other(wireName)
            }
        }
    }

    /// One of another device's slices a change reaches.
    public struct AffectedSlice: Equatable, Sendable {
        public var sliceId: Int
        public var letter: String
        public var frequencyHz: Double
        public var band: Int
        public var mode: Int
        /// The ADC its receiver is fed from, from 0; shown plus one.
        public var adc: Int
        /// Its receiver, from 0; shown plus one.
        public var streamIndex: Int
        public var effect: Effect
    }

    /// One device a change reaches.
    public struct AffectedDevice: Equatable, Sendable {
        public var deviceId: String
        public var deviceName: String
        public var deviceShortName: String
        public var state: DeviceState
        public var holdsTransmit: Bool
        public var slices: [AffectedSlice]
    }

    /// One slice on a receiver a take could take.
    public struct ChoiceSlice: Equatable, Sendable {
        public var sliceId: Int
        public var letter: String
        public var deviceId: String
        public var deviceName: String
        public var frequencyHz: Double
        public var mode: Int
        public var band: Int
        public var txSlice: Bool
    }

    /// One device on a receiver a take could take.
    public struct ChoiceDevice: Equatable, Sendable {
        public var deviceId: String
        public var name: String
        public var shortName: String
        public var state: DeviceState
        public var lastActivitySeconds: Int64
    }

    /// One receiver (or, for `takeSlice`, one slice) a take could take.
    public struct Choice: Equatable, Sendable, Identifiable {
        /// What `confirm.proceed` sends to pick it.
        public var choice: Int64
        public var streamIndex: Int
        public var adc: Int
        public var anchorName: String
        public var slices: [ChoiceSlice]
        public var devices: [ChoiceDevice]
        public var takeable: Bool
        /// Why it can't be taken, in the Core's words; empty when it can.
        public var why: String

        public var id: Int64 { choice }
    }

    /// Who has transmit, as a `takeTransmit` question names it (the entry
    /// of `connectedDevices`, with how it took transmit and whether it is
    /// keyed now).
    public struct Holder: Equatable, Sendable {
        public var deviceId: String
        public var name: String
        public var shortName: String
        /// `phone`, `tablet`, `computer` or `station`.
        public var kind: String
        /// It took transmit with the radio's own PTT.
        public var radioPtt: Bool
        public var state: DeviceState
        /// It is on the air now.
        public var keyed: Bool
        public var connectedForSeconds: Int64
        public var lastActivitySeconds: Int64
        public var awayForSeconds: Int64
        public var transmittingForSeconds: Int64

        public init(deviceId: String = "", name: String = "", shortName: String = "", kind: String = "",
                    radioPtt: Bool = false, state: DeviceState = .listening, keyed: Bool = false,
                    connectedForSeconds: Int64 = 0, lastActivitySeconds: Int64 = 0, awayForSeconds: Int64 = 0,
                    transmittingForSeconds: Int64 = 0) {
            self.deviceId = deviceId
            self.name = name
            self.shortName = shortName
            self.kind = kind
            self.radioPtt = radioPtt
            self.state = state
            self.keyed = keyed
            self.connectedForSeconds = connectedForSeconds
            self.lastActivitySeconds = lastActivitySeconds
            self.awayForSeconds = awayForSeconds
            self.transmittingForSeconds = transmittingForSeconds
        }

        /// On the air: keyed, or `transmitting`.
        public var onAir: Bool {
            keyed || state == .transmitting
        }
    }

    static func holder(_ o: [String: LinkJSON]) -> Holder {
        Holder(deviceId: o.string("deviceId") ?? "", name: o.string("name") ?? "",
               shortName: o.string("shortName") ?? "", kind: o.string("kind") ?? "",
               radioPtt: o.string("source") == "radioPtt", state: DeviceState(wireName: o.string("state") ?? ""),
               keyed: o.bool("keyed") ?? false, connectedForSeconds: o.whole("connectedForSeconds") ?? 0,
               lastActivitySeconds: o.whole("lastActivitySeconds") ?? 0,
               awayForSeconds: o.whole("awayForSeconds") ?? 0,
               transmittingForSeconds: o.whole("transmittingForSeconds") ?? 0)
    }

    /// A `confirm.request`, read.
    public struct Question: Equatable, Sendable, Identifiable {
        public var id: Int64
        public var kind: QuestionKind
        /// The Core's words, sent as they are.
        public var reason: String
        public var change: Change?
        public var affected: [AffectedDevice]
        public var choices: [Choice]
        public var expiresInMs: Int64
        public var forCommandId: Int64?
        public var forWriteId: Int64?
        public var forSettingsKey: String?
        /// Who has transmit, on a `takeTransmit` question.
        public var holder: Holder?

        public init(_ wire: LinkMessage.ConfirmRequest) {
            id = wire.id
            kind = QuestionKind(wireName: wire.kind)
            reason = wire.reason
            change = wire.change.flatMap(SeveralDevices.change)
            affected = wire.affected.compactMap(SeveralDevices.affectedDevice)
            choices = (wire.choices ?? []).compactMap(SeveralDevices.choice)
            expiresInMs = wire.expiresInMs
            forCommandId = wire.forCommandId
            forWriteId = wire.forWriteId
            forSettingsKey = wire.forSettingsKey
            holder = wire.holder.map(SeveralDevices.holder)
        }

        /// The first choice that can be taken, where the choice starts.
        public var firstTakeable: Choice? {
            choices.first(where: \.takeable)
        }
    }

    static func change(_ o: [String: LinkJSON]) -> Change? {
        guard let label = o.string("label"), let from = o.string("from"), let to = o.string("to") else {
            return nil
        }
        return Change(label: label, from: from, to: to)
    }

    static func affectedDevice(_ json: LinkJSON) -> AffectedDevice? {
        guard case .object(let o) = json else {
            return nil
        }
        let slices: [AffectedSlice] = o.array("slices").compactMap { slice in
            guard case .object(let s) = slice, let id = s.whole("sliceId") else {
                return nil
            }
            return AffectedSlice(sliceId: Int(id), letter: s.string("letter") ?? "",
                                 frequencyHz: s.number("frequencyHz") ?? 0, band: Int(s.whole("band") ?? -1),
                                 mode: Int(s.whole("mode") ?? -1), adc: Int(s.whole("adc") ?? 0),
                                 streamIndex: Int(s.whole("streamIndex") ?? -1),
                                 effect: Effect(wireName: s.string("effect") ?? ""))
        }
        return AffectedDevice(deviceId: o.string("deviceId") ?? "", deviceName: o.string("deviceName") ?? "",
                              deviceShortName: o.string("deviceShortName") ?? "",
                              state: DeviceState(wireName: o.string("state") ?? ""),
                              holdsTransmit: o.bool("holdsTransmit") ?? false, slices: slices)
    }

    /// A take's choice: a receiver with its slices and devices
    /// (`takeReceiver`), or one slice with its device (`takeSlice`), read
    /// as one shape.
    static func choice(_ json: LinkJSON) -> Choice? {
        guard case .object(let o) = json, let number = o.whole("choice") else {
            return nil
        }
        var slices: [ChoiceSlice] = o.array("slices").compactMap { entry in
            guard case .object(let s) = entry, let id = s.whole("sliceId") else {
                return nil
            }
            return ChoiceSlice(sliceId: Int(id), letter: s.string("letter") ?? "", deviceId: s.string("deviceId") ?? "",
                               deviceName: s.string("deviceName") ?? "", frequencyHz: s.number("frequencyHz") ?? 0,
                               mode: Int(s.whole("mode") ?? -1), band: Int(s.whole("band") ?? -1),
                               txSlice: s.bool("txSlice") ?? false)
        }
        var devices: [ChoiceDevice] = o.array("devices").compactMap { entry in
            guard case .object(let d) = entry else {
                return nil
            }
            return ChoiceDevice(deviceId: d.string("deviceId") ?? "", name: d.string("name") ?? "",
                                shortName: d.string("shortName") ?? "",
                                state: DeviceState(wireName: d.string("state") ?? ""),
                                lastActivitySeconds: d.whole("lastActivitySeconds") ?? 0)
        }
        if o["slices"] == nil, let id = o.whole("sliceId") {
            // A takeSlice choice names its one slice and device flat.
            slices = [ChoiceSlice(sliceId: Int(id), letter: o.string("letter") ?? "",
                                  deviceId: o.string("deviceId") ?? "", deviceName: o.string("deviceName") ?? "",
                                  frequencyHz: o.number("frequencyHz") ?? 0, mode: Int(o.whole("mode") ?? -1),
                                  band: Int(o.whole("band") ?? -1), txSlice: o.bool("txSlice") ?? false)]
            devices = [ChoiceDevice(deviceId: o.string("deviceId") ?? "", name: o.string("deviceName") ?? "",
                                    shortName: o.string("deviceShortName") ?? "",
                                    state: DeviceState(wireName: o.string("state") ?? ""),
                                    lastActivitySeconds: o.whole("lastActivitySeconds") ?? 0)]
        }
        return Choice(choice: number, streamIndex: Int(o.whole("streamIndex") ?? -1), adc: Int(o.whole("adc") ?? 0),
                      anchorName: o.string("anchorName") ?? "", slices: slices, devices: devices,
                      takeable: o.bool("takeable") ?? false, why: o.string("why") ?? "")
    }

    // MARK: Notices

    /// What a notice tells.
    public enum NoticeKind: Equatable, Sendable {
        /// Another device changed a shared setting (D53).
        case settingChanged
        /// Another device moved a shared receiver: this device's slice moved or closed (D50).
        case sliceMoved
        case sliceClosed
        /// Another device took a receiver or a slice; Take it back (D49).
        case receiverTaken
        case sliceTaken
        /// Transmit was taken; the transmit screens' own (D51, D52).
        case transmitTaken
        /// This device's place was taken while it was away (D55).
        case placeTaken
        /// The receive antenna stayed put while another device listens through it (D61).
        case antennaKept
        /// This device came back after its 3 minutes (D62).
        case graceEnded
        /// Saved slices that did not fit when this device came back.
        case slicesNotRestored
        /// Another device took control of a slice this device controlled;
        /// this device still listens to it (D109, `sliceAccessVersion`).
        case controlTaken
        case other(String)

        public init(wireName: String) {
            switch wireName {
            case "settingChanged": self = .settingChanged
            case "sliceMoved": self = .sliceMoved
            case "sliceClosed": self = .sliceClosed
            case "receiverTaken": self = .receiverTaken
            case "sliceTaken": self = .sliceTaken
            case "transmitTaken": self = .transmitTaken
            case "placeTaken": self = .placeTaken
            case "antennaKept": self = .antennaKept
            case "graceEnded": self = .graceEnded
            case "slicesNotRestored": self = .slicesNotRestored
            case "controlTaken": self = .controlTaken
            default: self = .other(wireName)
            }
        }
    }

    /// Who did what a notice tells.
    public struct NoticeBy: Equatable, Sendable {
        public var deviceId: String
        public var name: String
        public var shortName: String
        public var kind: String
        /// The radio's own PTT did it, not a device.
        public var radioPtt: Bool
    }

    /// One slice a notice touched, closed ones included.
    public struct NoticeSlice: Equatable, Sendable {
        public var sliceId: Int
        public var letter: String
        public var frequencyHz: Double
        public var mode: Int
        public var band: Int
        /// On a `controlTaken` sent at `sliceAccessVersion` 2: the slice's
        /// incarnation and the control revision after the take, which Take
        /// it back names; nil on any other notice.
        public var incarnation: Int64? = nil
        public var controlRevision: Int64? = nil
    }

    /// A `notice`, read.
    public struct Notice: Equatable, Sendable, Identifiable {
        public var id: Int64
        public var kind: NoticeKind
        /// The Core's words, sent as they are.
        public var reason: String
        /// How long ago it happened when the Core sent it.
        public var secondsAgo: Int64
        public var takeBack: Bool
        /// Who did it; nil for a notice about this device's own state.
        public var by: NoticeBy?
        public var slices: [NoticeSlice]
        public var change: Change?

        public init(_ wire: LinkMessage.Notice) {
            id = wire.id
            kind = NoticeKind(wireName: wire.kind)
            reason = wire.reason
            secondsAgo = wire.secondsAgo
            takeBack = wire.takeBack
            if wire.byName != nil || wire.byShortName != nil || wire.byDeviceId != nil || wire.bySource != nil {
                by = NoticeBy(deviceId: wire.byDeviceId ?? "", name: wire.byName ?? "",
                              shortName: wire.byShortName ?? "", kind: wire.byKind ?? "",
                              radioPtt: wire.bySource == "radioPtt")
            }
            slices = (wire.slices ?? []).compactMap { entry in
                guard case .object(let s) = entry, let id = s.whole("sliceId") else {
                    return nil
                }
                return NoticeSlice(sliceId: Int(id), letter: s.string("letter") ?? "",
                                   frequencyHz: s.number("frequencyHz") ?? 0, mode: Int(s.whole("mode") ?? -1),
                                   band: Int(s.whole("band") ?? -1), incarnation: s.whole("incarnation"),
                                   controlRevision: s.whole("controlRevision"))
            }
            change = wire.change.flatMap(SeveralDevices.change)
        }

        /// When it happened, by this phone's clock: `secondsAgo` before
        /// `received`, the moment the notice arrived.
        public func happened(received: Date) -> Date {
            received.addingTimeInterval(-TimeInterval(secondsAgo))
        }
    }

    // MARK: The answer to Confirm

    /// What a `confirm.proceed` answer says the change settled at (ruling
    /// 7.4a): a property write's object and values, or a setting's value
    /// (nil for a removal, link document section 8.1).
    public enum Readback: Equatable, Sendable {
        case properties(objectKey: String, values: [String: MirrorValue])
        case setting(key: String, value: String?)

        /// The readback an accepted proceed carries, or nil (a command's own
        /// result values, or none).
        public init?(_ result: CommandResult) {
            guard result.accepted else {
                return nil
            }
            if case .text(let key)? = result.values["objectKey"] {
                var values = result.values
                values["objectKey"] = nil
                values["phase"] = nil
                guard !values.isEmpty else {
                    return nil
                }
                self = .properties(objectKey: key, values: values)
                return
            }
            if case .text(let key)? = result.values["settingsKey"] {
                // An accepted removal carries settingsKey alone (link document section 8.1).
                switch result.values["value"] {
                case nil:
                    self = .setting(key: key, value: nil)
                case .text(let value)?:
                    self = .setting(key: key, value: value)
                default:
                    return nil
                }
                return
            }
            return nil
        }
    }
}

private extension Dictionary where Key == String, Value == LinkJSON {
    func string(_ key: String) -> String? {
        if case .string(let text)? = self[key] {
            return text
        }
        return nil
    }

    func bool(_ key: String) -> Bool? {
        if case .bool(let flag)? = self[key] {
            return flag
        }
        return nil
    }

    func number(_ key: String) -> Double? {
        if case .number(let value)? = self[key], value.isFinite {
            return value
        }
        return nil
    }

    func whole(_ key: String) -> Int64? {
        guard let value = number(key), value.rounded(.towardZero) == value,
              abs(value) < 9_007_199_254_740_992 else {
            return nil
        }
        return Int64(value)
    }

    func array(_ key: String) -> [LinkJSON] {
        if case .array(let elements)? = self[key] {
            return elements
        }
        return []
    }
}

private extension MirrorValue {
    var decimal: Double? {
        switch self {
        case .double(let value):
            return value
        case .int(let value), .enumeration(let value):
            return Double(value)
        case .bool, .text:
            return nil
        }
    }

    var wholeNumber: Int64? {
        switch self {
        case .int(let value), .enumeration(let value):
            return value
        case .double(let value) where value.isFinite && value.rounded(.towardZero) == value:
            return Int64(value)
        default:
            return nil
        }
    }

    var words: String? {
        if case .text(let value) = self {
            return value
        }
        return nil
    }

    var flag: Bool? {
        if case .bool(let value) = self {
            return value
        }
        return nil
    }
}
