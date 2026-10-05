// NereusSDR for iOS: what the band asks the Core for, by data mode, network, lock, battery and heat
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The display the phone asks for over a long session (R-IOS-22, R-IOS-23,
/// D27, D28; spec sections 4.7, 5.4 items 11 to 13 and 5.5 items 9 to 12).
///
/// Each data mode sets a frame rate and a detail: Full (30 frames a second,
/// full detail), Balanced (15), Saver (5, half the detail) and Audio only
/// (no display endpoints at all, only the sound). Wi-Fi starts at Full and
/// cellular at Balanced. Then the circumstances bring it down, never up:
///
/// * locked or in another app with Sound only on, no display endpoints (the
///   R3 endpoint controls, R-R3-08); the band subscribes again on return,
///   and a new endpoint's first context brings a keyframe;
/// * Low Power Mode, with its switch on, no more than Saver;
/// * the phone's heat, with its switch on: serious halves the frame rate,
///   critical is no more than Saver, until it cools.
///
/// The request never asks for more than the Core's own display settings do;
/// ``DisplaySubscription``'s frame rate and width are capped by it.
public enum SessionPolicy {
    /// A data mode, in order from the most data to the least.
    public enum Mode: String, CaseIterable, Sendable, Comparable {
        case full
        case balanced
        case saver
        case audioOnly

        /// What the mode asks for on its own.
        public var request: Request {
            switch self {
            case .full:
                return Request(fps: 30, detail: .full)
            case .balanced:
                return Request(fps: 15, detail: .full)
            case .saver:
                return Request(fps: 5, detail: .half)
            case .audioOnly:
                return .none
            }
        }

        /// The modes Wi-Fi offers.
        public static let wifiModes: [Mode] = [.full, .balanced]
        /// The modes cellular offers.
        public static let cellularModes: [Mode] = [.full, .balanced, .saver, .audioOnly]
        /// Wi-Fi starts at Full, cellular at Balanced (D28).
        public static let wifiDefault = Mode.full
        public static let cellularDefault = Mode.balanced

        private var order: Int {
            switch self {
            case .full:
                return 0
            case .balanced:
                return 1
            case .saver:
                return 2
            case .audioOnly:
                return 3
            }
        }

        /// A mode is "less" when it asks for more data.
        public static func < (lhs: Mode, rhs: Mode) -> Bool {
            lhs.order < rhs.order
        }
    }

    /// The network the phone is on.
    public enum Network: Sendable, Equatable {
        /// Wi-Fi or a wired network.
        case wifi
        case cellular
    }

    /// The phone's heat, as iOS reports it.
    public enum Heat: Sendable, Equatable {
        case nominal
        case fair
        case serious
        case critical
    }

    /// How much of the band's detail a request asks for.
    public enum Detail: Sendable, Equatable {
        case full
        /// Half the band's width in samples.
        case half
    }

    /// What the band asks for: a frame rate and a detail, or no display
    /// endpoints at all (``none``).
    public struct Request: Sendable, Equatable {
        /// The most frames a second asked for; nil asks for no endpoints.
        public var fps: Int?
        public var detail: Detail

        public init(fps: Int?, detail: Detail) {
            self.fps = fps
            self.detail = detail
        }

        /// No display endpoints: only the sound.
        public static let none = Request(fps: nil, detail: .full)

        /// Whether the band subscribes at all.
        public var subscribes: Bool { fps != nil }

        /// The band's width in samples for `pixels` wanted.
        public func pixels(_ pixels: Int) -> Int {
            detail == .half ? max(1, pixels / 2) : pixels
        }

        /// The frame rate for `fps` wanted: never more than this request's.
        public func fps(_ wanted: Int) -> Int {
            guard let fps else {
                return wanted
            }
            return min(wanted, fps)
        }
    }

    /// Everything the request depends on.
    public struct Circumstances: Sendable, Equatable {
        public var network: Network
        /// The Wi-Fi choice: Full or Balanced.
        public var wifiMode: Mode
        /// The cellular choice: any of the four.
        public var cellularMode: Mode
        /// The app is in front, not locked and not behind another app.
        public var inForeground: Bool
        /// Setup's Sound only while locked or in another app.
        public var soundOnlyWhenAway: Bool
        public var lowPowerMode: Bool
        /// Setup's In Low Power Mode, drop to Saver.
        public var lowPowerDropsToSaver: Bool
        public var heat: Heat
        /// Setup's Slow the band when the phone is hot.
        public var hotPhoneSlows: Bool

        public init(network: Network = .wifi, wifiMode: Mode = Mode.wifiDefault,
                    cellularMode: Mode = Mode.cellularDefault, inForeground: Bool = true,
                    soundOnlyWhenAway: Bool = true, lowPowerMode: Bool = false, lowPowerDropsToSaver: Bool = true,
                    heat: Heat = .nominal, hotPhoneSlows: Bool = true) {
            self.network = network
            self.wifiMode = wifiMode
            self.cellularMode = cellularMode
            self.inForeground = inForeground
            self.soundOnlyWhenAway = soundOnlyWhenAway
            self.lowPowerMode = lowPowerMode
            self.lowPowerDropsToSaver = lowPowerDropsToSaver
            self.heat = heat
            self.hotPhoneSlows = hotPhoneSlows
        }
    }

    /// The mode the network's choice picks, brought down by Low Power Mode
    /// and a critical heat. Wi-Fi offers only Full and Balanced: anything
    /// else read for it is taken as Full.
    public static func mode(for circumstances: Circumstances) -> Mode {
        var mode: Mode
        switch circumstances.network {
        case .wifi:
            mode = Mode.wifiModes.contains(circumstances.wifiMode) ? circumstances.wifiMode : Mode.wifiDefault
        case .cellular:
            mode = circumstances.cellularMode
        }
        if circumstances.lowPowerMode && circumstances.lowPowerDropsToSaver {
            mode = max(mode, .saver)
        }
        if circumstances.hotPhoneSlows && circumstances.heat == .critical {
            mode = max(mode, .saver)
        }
        return mode
    }

    /// What the band asks the Core for in `circumstances`.
    public static func request(for circumstances: Circumstances) -> Request {
        if !circumstances.inForeground && circumstances.soundOnlyWhenAway {
            return .none
        }
        var request = mode(for: circumstances).request
        if circumstances.hotPhoneSlows, circumstances.heat == .serious, let fps = request.fps {
            request.fps = max(1, fps / 2)
        }
        return request
    }
}
