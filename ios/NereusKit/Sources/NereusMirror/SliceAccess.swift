// NereusSDR for iOS: who controls each slice, as the Core says, and the words for a slice another device controls
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// Slice access (R-IOS-42; D109, D110, D115; the slice access contract
/// note, docs/architecture/2026-09-28-slice-access-phone-contract.md; the
/// link document, sections 6.2, 7.1 and 9.1): which device controls each
/// slice this phone has joined, read from the Core's `access:<id>`
/// objects, and the Core's own words for a slice another device controls.
///
/// The gate is two keys, both required: agreed minor 11 or later and
/// `sliceAccessVersion` 1 or more. A Core without it sends no `access:<id>`
/// and every `slice:<id>` it sends is this phone's own, as before. At 2
/// the `controlTaken` notice offers Take it back. At 3 (core-slice
/// take-over, JJ 2026-09-30) every slice this phone does not control can
/// be taken, the Core's own included, and the one refusal is while the
/// slice transmits; below 3 the Core's own slice on a Core no desktop
/// hosts cannot be taken (the contract note, "Taking the Core's own
/// slice").
public enum SliceAccess {
    /// The capability the Core sends a phone that declared `sliceAccess`.
    public static let capability = "sliceAccessVersion"
    /// The agreed minor its objects and verbs need.
    public static let minimumMinor: UInt16 = 11
    /// The version at which `controlTaken` offers Take it back.
    public static let takeBackVersion: Int64 = 2
    /// The version at which every slice can be taken, the Core's own
    /// included, and only a slice on the air is refused.
    public static let everySliceVersion: Int64 = 3
    /// The mirrored class of a slice's access, and its key's prefix.
    public static let accessClass = "SliceAccess"
    public static let accessPrefix = "access:"
    /// The verb that moves control of an existing slice to this phone.
    public static let takeControlVerb = "slice.takeControl"
    /// The controller id the Core gives its own operating position.
    public static let stationDeviceId = "station"

    /// The Core's words for Take it back that cannot work (the contract
    /// note, "The controlTaken notice"): a Core at `sliceAccessVersion` 1,
    /// and a notice the Core sent with `takeBack` false.
    public static let coreCannotGiveBackText = "This Core cannot give control back from here. Updating the Core may help."
    public static let noLongerTakenBackText = "That can no longer be taken back."

    /// The version the Core offers this phone; 0 below minor 11 or when it
    /// sends none.
    @MainActor
    public static func version(in store: MirrorStore) -> Int64 {
        guard (store.agreedMinor ?? 0) >= minimumMinor else {
            return 0
        }
        return max(0, store.capabilityVersion(capability))
    }

    /// Whether the Core shares its slices with this phone.
    @MainActor
    public static func available(in store: MirrorStore) -> Bool {
        version(in: store) >= 1
    }

    /// One slice's `access:<id>`.
    public struct State: Equatable, Sendable {
        public var sliceId: Int
        /// Never 0 and never reused while the Core runs.
        public var incarnation: Int64
        /// The controller's device id: empty for nobody, `station` for the Core's own position.
        public var controllerDeviceId: String
        /// One more on each change of controller.
        public var controlRevision: Int64
        /// Every joined device, controller first.
        public var listenerDeviceIds: [String]
        public var txSelected: Bool
        public var onAir: Bool

        public init(sliceId: Int, incarnation: Int64, controllerDeviceId: String, controlRevision: Int64,
                    listenerDeviceIds: [String] = [], txSelected: Bool = false, onAir: Bool = false) {
            self.sliceId = sliceId
            self.incarnation = incarnation
            self.controllerDeviceId = controllerDeviceId
            self.controlRevision = controlRevision
            self.listenerDeviceIds = listenerDeviceIds
            self.txSelected = txSelected
            self.onAir = onAir
        }

        /// Nobody controls the slice.
        public var nobodyControls: Bool { controllerDeviceId.isEmpty }
    }

    /// The access a mirrored object holds, or nil when it is not one.
    @MainActor
    public static func state(_ object: MirrorObject) -> State? {
        guard object.className == accessClass else {
            return nil
        }
        var id = object["sliceId"]?.whole.map(Int.init)
        if id == nil, object.key.hasPrefix(accessPrefix) {
            id = Int(object.key.dropFirst(accessPrefix.count))
        }
        guard let sliceId = id, let incarnation = object["incarnation"]?.whole,
              let revision = object["controlRevision"]?.whole else {
            return nil
        }
        return State(sliceId: sliceId, incarnation: incarnation,
                     controllerDeviceId: object["controllerDeviceId"]?.text ?? "", controlRevision: revision,
                     listenerDeviceIds: deviceIds(object["listenerDeviceIds"]?.text ?? ""),
                     txSelected: object["txSelected"]?.flag ?? false, onAir: object["onAir"]?.flag ?? false)
    }

    /// Every slice's access the mirror holds, by slice id; empty below the gate.
    @MainActor
    public static func states(in store: MirrorStore) -> [Int: State] {
        guard available(in: store) else {
            return [:]
        }
        var states: [Int: State] = [:]
        for object in store.objects(ofClass: accessClass) {
            if let state = state(object) {
                states[state.sliceId] = state
            }
        }
        return states
    }

    /// A JSON array of device ids, as `listenerDeviceIds` carries it.
    static func deviceIds(_ text: String) -> [String] {
        guard case .array(let entries)? = try? LinkJSON.parse(text) else {
            return []
        }
        return entries.compactMap { entry in
            if case .string(let id) = entry {
                return id
            }
            return nil
        }
    }

    // MARK: The Core's words

    /// 'A' + the slice id, as every device letters it.
    public static func letter(_ sliceId: Int) -> String {
        guard sliceId >= 0, sliceId < 26, let scalar = Unicode.Scalar(UInt32(65 + sliceId)) else {
            return "?"
        }
        return String(Character(scalar))
    }

    /// Who holds a slice, as the Core names a holder in its refusals
    /// (src/core/session/StationServer.cpp `sliceHolderWords`, at trunk
    /// 5cdb9a1ab): "the Core" for nobody; the device's own name as sent;
    /// for the Core's own position, the name of the desktop hosting the
    /// Core, or "the Core" when no desktop hosts it; "a phone", "a tablet"
    /// or "a computer" for a device with no name; "another device" when
    /// the phone knows neither.
    public static func holderWords(_ deviceId: String, devices: [SeveralDevices.ConnectedDevice]) -> String {
        deviceWords(deviceId, devices: devices, short: false)
    }

    /// A device named by its id in `access:<id>` (`controllerDeviceId` or
    /// an entry of `listenerDeviceIds`), in the Core's ladder
    /// (``holderWords``). `short` names it by its short name where the
    /// phone uses short names, and by its name when it has none.
    ///
    /// The Core's own position is `station` in `access:<id>`, but its
    /// entry in `connectedDevices` carries another id (the hosting
    /// desktop's device id, not `station`), so it is never matched by id:
    /// the entry with `hostsCore` true is the desktop hosting the Core,
    /// and names it. With no such entry the Core runs headless, and it is
    /// "the Core".
    public static func deviceWords(_ deviceId: String, devices: [SeveralDevices.ConnectedDevice],
                                   short: Bool = false) -> String {
        if deviceId.isEmpty {
            return coreWords
        }
        if deviceId == stationDeviceId {
            guard let desktop = hostingDesktop(devices) else {
                return coreWords
            }
            let first = short ? desktop.shortName : desktop.name
            let second = short ? desktop.name : desktop.shortName
            if !first.isEmpty {
                return first
            }
            return second.isEmpty ? coreWords : second
        }
        let device = devices.first { $0.deviceId == deviceId }
        if let device {
            let name = short && !device.shortName.isEmpty ? device.shortName : device.name
            if !name.isEmpty {
                return name
            }
        }
        if let kind = device?.kind, !kind.isEmpty {
            return "a " + kindWord(kind).lowercased()
        }
        return "another device"
    }

    /// The desktop hosting the Core (`hostsCore` true in
    /// `connectedDevices`), or nil for a headless Core.
    public static func hostingDesktop(_ devices: [SeveralDevices.ConnectedDevice]) -> SeveralDevices.ConnectedDevice? {
        devices.first { $0.hostsCore }
    }

    /// Everyone who listens to the slice but the controller, in the
    /// Core's order, each named as ``deviceWords`` names it; `excluding`
    /// leaves out a device (this phone).
    public static func listenerWords(_ state: State, devices: [SeveralDevices.ConnectedDevice],
                                     short: Bool = false, excluding: String? = nil) -> [String] {
        state.listenerDeviceIds.filter { $0 != state.controllerDeviceId && $0 != excluding }.map {
            deviceWords($0, devices: devices, short: short)
        }
    }

    /// The Core's words for itself as a slice's holder.
    public static let coreWords = "the Core"

    /// A device's kind as the Core words it (DeviceSessionRegistry
    /// `kindWord`): "Phone", "Tablet", and "Computer" for any other kind.
    public static func kindWord(_ kind: String) -> String {
        switch kind {
        case "phone":
            return "Phone"
        case "tablet":
            return "Tablet"
        default:
            return "Computer"
        }
    }

    /// The Core's line for a slice this phone listens to and does not
    /// control (StationServer.cpp `listenerChangeReason`): "Nobody controls
    /// slice B. Take control to change it." or "Slice B is controlled by
    /// Shack desktop. Take control to change it."
    public static func ownerLine(_ state: State, devices: [SeveralDevices.ConnectedDevice]) -> String {
        let letter = letter(state.sliceId)
        if state.nobodyControls {
            return "Nobody controls slice \(letter). Take control to change it."
        }
        return "Slice \(letter) is controlled by \(holderWords(state.controllerDeviceId, devices: devices)). "
            + "Take control to change it."
    }

    // MARK: Taking control

    /// The Core's refusal of a take while the slice transmits
    /// (src/core/session/SliceAccessController.cpp:72 at trunk b26112687).
    public static func transmittingText(_ sliceId: Int) -> String {
        "Slice \(letter(sliceId)) is transmitting. Take control once it stops."
    }

    /// The Core's refusal, below `sliceAccessVersion` 3, of a take of its
    /// own slice with nobody at its desktop (StationServer.cpp
    /// `handOffRefusal` and StationClient.cpp
    /// `coreSliceTakeUnavailableReason`, at trunk b26112687).
    public static func coreItselfText(_ sliceId: Int) -> String {
        "Slice \(letter(sliceId)) is run by the Core itself, so control of it cannot pass to this device."
    }

    /// Why Take control on a slice this phone listens to is greyed, in
    /// the Core's words, or nil when it can be pressed (the contract note,
    /// "Taking the Core's own slice"). While the slice is on the air
    /// (`onAir`) it is greyed at every version the Core sends `onAir`, as
    /// the desktop greys it (JJ, 2026-09-30). At `sliceAccessVersion` 3 or
    /// more every other slice can be taken. Below 3 the Core's own slice
    /// (`station`) on a Core no desktop hosts cannot be; every other take
    /// is the Core's to answer, as before.
    public static func takeRefusal(_ state: State, version: Int64,
                                   devices: [SeveralDevices.ConnectedDevice]) -> String? {
        if state.onAir {
            return transmittingText(state.sliceId)
        }
        if version >= everySliceVersion {
            return nil
        }
        if state.controllerDeviceId == stationDeviceId, hostingDesktop(devices) == nil {
            return coreItselfText(state.sliceId)
        }
        return nil
    }

    /// What `slice.takeControl` came to.
    public enum TakeOutcome: Equatable, Sendable {
        /// This phone controls the slice now; the Core's new revision.
        case accepted(controlRevision: Int64?)
        /// The Core refused, in its words as sent.
        case refused(String)
        /// Nothing reached the Core, or it never answered.
        case notAnswered
        /// The phone knows no access for the slice, so sent nothing.
        case notSent
    }

    /// How long a take waits for the Core.
    public static let takeTimeout: Duration = .seconds(10)

    /// `slice.takeControl {sliceId, incarnation, controlRevision}`, each an
    /// `i64` from the slice's `access:<id>` as the phone last saw it (link
    /// section 9.1). Only the controller changes; every setting stays.
    @MainActor
    public static func takeControl(_ state: State, commands: CommandClient) async -> TakeOutcome {
        let result: CommandResult
        do {
            result = try await commands.invoke(takeControlVerb, arguments: [
                CommandArgument(name: "sliceId", value: .int(Int64(state.sliceId))),
                CommandArgument(name: "incarnation", value: .int(state.incarnation)),
                CommandArgument(name: "controlRevision", value: .int(state.controlRevision)),
            ], timeout: takeTimeout)
        } catch {
            return .notAnswered
        }
        return outcome(result)
    }

    // MARK: Listening, leaving and the phone's own level

    /// The verbs that join a slice, leave it, give up control of it and set
    /// this phone's own level for it (link section 9.1; the contract note,
    /// "Verbs").
    public static let listenVerb = "slice.listen"
    public static let stopListeningVerb = "slice.stopListening"
    public static let releaseVerb = "slice.release"
    public static let setListenLevelVerb = "slice.setListenLevel"

    /// `slice.listen {sliceId, incarnation}`: joins the slice, which then
    /// arrives as a `slice:<id>`; accepted with the slice's control revision.
    @MainActor
    public static func listen(_ state: State, commands: CommandClient) async -> TakeOutcome {
        await run(listenVerb, state, [], commands: commands)
    }

    /// `slice.listen` for a slice the phone knows only by id and
    /// incarnation, as a refused new slice's `usableSlices` names it.
    @MainActor
    public static func listen(sliceId: Int, incarnation: Int64, commands: CommandClient) async -> TakeOutcome {
        await listen(State(sliceId: sliceId, incarnation: incarnation, controllerDeviceId: "", controlRevision: 0),
                     commands: commands)
    }

    /// `slice.stopListening {sliceId, incarnation}`: leaves a slice this
    /// phone listens to. The Core refuses it from the controller.
    @MainActor
    public static func stopListening(_ state: State, commands: CommandClient) async -> TakeOutcome {
        await run(stopListeningVerb, state, [], commands: commands)
    }

    /// `slice.release {sliceId, incarnation, controlRevision}`: this phone
    /// gives up control and leaves; the slice stays for its other listeners.
    @MainActor
    public static func release(_ state: State, commands: CommandClient) async -> TakeOutcome {
        await run(releaseVerb, state, [CommandArgument(name: "controlRevision", value: .int(state.controlRevision))],
                  commands: commands)
    }

    /// `slice.setListenLevel {sliceId, incarnation, level, muted}`: what this
    /// phone hears of a slice it listens to, `level` from 0 to 1. It never
    /// changes the slice's own AF gain or mute, or anyone else's audio.
    @MainActor
    public static func setListenLevel(_ state: State, level: Double, muted: Bool,
                                      commands: CommandClient) async -> TakeOutcome {
        await run(setListenLevelVerb, state, [
            CommandArgument(name: "level", value: .double(min(max(level, 0), 1))),
            CommandArgument(name: "muted", value: .bool(muted)),
        ], commands: commands)
    }

    @MainActor
    private static func run(_ verb: String, _ state: State, _ extra: [CommandArgument],
                            commands: CommandClient) async -> TakeOutcome {
        let result: CommandResult
        do {
            result = try await commands.invoke(verb, arguments: [
                CommandArgument(name: "sliceId", value: .int(Int64(state.sliceId))),
                CommandArgument(name: "incarnation", value: .int(state.incarnation)),
            ] + extra, timeout: takeTimeout)
        } catch {
            return .notAnswered
        }
        return outcome(result)
    }

    // MARK: The phone's own words for a holder

    /// The listening flag's owner row (JJ, 2026-09-30, the board's slim
    /// owner row): "<holder> controls A", the holder named as
    /// ``deviceWords`` names it (the hosting desktop's name, "the Core"
    /// for a headless Core or nobody, "a phone", "another device"), its
    /// first letter raised where the words are the phone's own. A device's
    /// own name is shown as sent.
    public static func ownerWords(_ state: State, devices: [SeveralDevices.ConnectedDevice]) -> String {
        "\(sentenceStart(state.controllerDeviceId, devices: devices)) controls \(letter(state.sliceId))"
    }

    /// A holder at the start of a line: a device's name as sent, the
    /// phone's own words ("the Core", "a phone", "another device") with the
    /// first letter raised.
    public static func sentenceStart(_ deviceId: String, devices: [SeveralDevices.ConnectedDevice]) -> String {
        let words = deviceWords(deviceId, devices: devices)
        guard isOwnWords(deviceId, devices: devices), let first = words.first else {
            return words
        }
        return first.uppercased() + words.dropFirst()
    }

    /// Whether ``deviceWords`` names the device with the phone's own words
    /// rather than a name the Core sent.
    static func isOwnWords(_ deviceId: String, devices: [SeveralDevices.ConnectedDevice]) -> Bool {
        let words = deviceWords(deviceId, devices: devices)
        if devices.contains(where: { !$0.name.isEmpty && $0.name == words || !$0.shortName.isEmpty && $0.shortName == words }) {
            return false
        }
        return words == coreWords || words == "another device" || words.hasPrefix("a ")
    }

    /// A take's `command.result` (or Take it back's, which the Core runs as
    /// the same take): `controlRevision` in `values` when accepted.
    public static func outcome(_ result: CommandResult) -> TakeOutcome {
        guard result.accepted else {
            return .refused(result.reason)
        }
        if case .int(let revision)? = result.values["controlRevision"] {
            return .accepted(controlRevision: revision)
        }
        return .accepted(controlRevision: nil)
    }
}

private extension MirrorValue {
    var whole: Int64? {
        switch self {
        case .int(let value), .enumeration(let value):
            return value
        case .double(let value) where value.isFinite && value.rounded(.towardZero) == value
            && abs(value) < 9_007_199_254_740_992:
            return Int64(value)
        default:
            return nil
        }
    }

    var text: String? {
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
