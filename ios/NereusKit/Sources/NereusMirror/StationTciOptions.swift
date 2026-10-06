// NereusSDR for iOS: the four options of the Core's TCI server, read from its stationTci object and sent back whole
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The options the Core keeps for its station TCI server at
/// `stationTciVersion` 2 (the accessory control document, "The `stationTci`
/// object and the station TCI server"): what a new TCI app is told about the
/// protocol and the device, whether CWL and CWU reach it as CW, and whether
/// it is sent the radio's state when it connects. They arrive as four
/// read-only properties of the `stationTci` object and change only through
/// `setStationTciOptions`, which always carries all four.
public struct StationTciOptions: Sendable, Equatable {
    public var emulateExpertSdr3: Bool
    public var emulateSunSdr2Pro: Bool
    public var cwluBecomesCw: Bool
    public var sendInitialState: Bool

    /// Saves the four options on the Core.
    public static let verb = "setStationTciOptions"
    /// The first version with the options.
    public static let version: Int64 = 2
    /// The properties and the verb's arguments, in the verb's order.
    public static let names = ["emulateExpertSdr3", "emulateSunSdr2Pro", "cwluBecomesCw", "sendInitialState"]

    public init(emulateExpertSdr3: Bool, emulateSunSdr2Pro: Bool, cwluBecomesCw: Bool, sendInitialState: Bool) {
        self.emulateExpertSdr3 = emulateExpertSdr3
        self.emulateSunSdr2Pro = emulateSunSdr2Pro
        self.cwluBecomesCw = cwluBecomesCw
        self.sendInitialState = sendInitialState
    }

    /// Reads the options from the `stationTci` object's values; nil unless
    /// all four arrived as flags, as from a Core before version 2.
    public init?(values: [String: MirrorValue]) {
        var flags: [Bool] = []
        for name in Self.names {
            guard case .bool(let flag)? = values[name] else {
                return nil
            }
            flags.append(flag)
        }
        self.init(emulateExpertSdr3: flags[0], emulateSunSdr2Pro: flags[1], cwluBecomesCw: flags[2],
                  sendInitialState: flags[3])
    }

    /// The verb's four arguments, in its order.
    public var arguments: [CommandArgument] {
        [
            CommandArgument(name: Self.names[0], value: .bool(emulateExpertSdr3)),
            CommandArgument(name: Self.names[1], value: .bool(emulateSunSdr2Pro)),
            CommandArgument(name: Self.names[2], value: .bool(cwluBecomesCw)),
            CommandArgument(name: Self.names[3], value: .bool(sendInitialState)),
        ]
    }
}
