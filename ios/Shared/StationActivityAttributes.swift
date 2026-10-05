// NereusSDR for iOS: what the app's Live Activity shows, shared by the app and its widget
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import ActivityKit
import Foundation

/// The Live Activity for the connected Core (R-IOS-14, R-IOS-21; spec
/// section 5.5 items 1 to 5 and 13): the lock-screen card and the Dynamic
/// Island. The app fills ``ContentState`` from the Core's state and the
/// widget draws it; the widget computes nothing of its own. Everything
/// here is data the Core sent (names, the S-meter's scale, the
/// frequency, the Core's words) or the app's own words.
struct StationActivityAttributes: ActivityAttributes {
    /// The link, as the card's chip shows it.
    enum Link: String, Codable, Hashable, Sendable {
        /// Up: the chip shows the round-trip time.
        case up
        /// Signing in or loading: the chip shows its dot only.
        case connecting
        /// Lost, and the phone keeps retrying (or the operator stopped it).
        case lost
        /// The phone has no network.
        case offline
    }

    /// The active slice as its flag shows it.
    struct Slice: Codable, Hashable, Sendable {
        /// The slice's letter, `A`.
        var letter: String
        /// `#RRGGBB`, the Core's colour for the slice.
        var colour: String
        var frequencyHz: Double
        /// The Core's label for the mode, `LSB`.
        var mode: String
        /// The passband's width as the flag shows it, `2.9K`.
        var bandwidth: String
        /// The signal in dBm, nil with no reading.
        var signalDbm: Double?
        /// The signal as the flag prints it, in the Multimeter page's units
        /// (`-85.6 dBm`, `S5.3`, `12.3 uV`); nil with no reading. The app
        /// prints it; the widget shows it as sent.
        var signalText: String?
        /// What VoiceOver says for it; nil with no reading.
        var signalSpoken: String?
    }

    /// The Core's S-meter scale (its catalogue), so the card's bar reads as
    /// the flag's does.
    struct Meter: Codable, Hashable, Sendable {
        struct Mark: Codable, Hashable, Sendable {
            var label: String
            var dbm: Double
        }

        var minDbm: Double
        var s9Dbm: Double
        var maxDbm: Double
        /// The marks the flag labels: every other S-unit, then every 20 dB over S9.
        var marks: [Mark]
    }

    /// The link lost: which try, when the next goes, and whether the
    /// operator stopped the tries.
    struct Retry: Codable, Hashable, Sendable {
        var attempt: Int
        /// When the next try goes; nil while one is under way.
        var at: Date?
        var stopped: Bool
    }

    /// What changes while the activity runs.
    struct ContentState: Codable, Hashable, Sendable {
        /// The Core's name, as the Core gives it (or its address).
        var stationName: String
        var link: Link
        /// The link's round trip while up, in whole milliseconds.
        var roundTripMs: Int?
        /// Set while the link is lost.
        var retry: Retry?
        /// The link went while this phone was keyed: the Core has already unkeyed.
        var lostWhileKeyed: Bool
        /// The active slice, nil before the Core has sent it.
        var slice: Slice?
        var meter: Meter?
        /// The band's sound is muted on this phone.
        var muted: Bool
        /// This phone is on the air.
        var keyed: Bool
        /// When the key began, for the clock that counts up.
        var keyedSince: Date?
        /// The radio's forward power in watts and SWR while keyed.
        var forwardWatts: Double
        var swr: Double
        /// When the Core's transmit time-out stops this key; nil when none applies.
        var timeOutAt: Date?
        /// The radio is on the air for someone else (another device, the
        /// radio's own PTT): who, empty when the Core names nobody; nil
        /// otherwise. The foot says "On the air from Radio".
        var onAirFrom: String?
        /// A line for the operator in place of the signal, empty for none.
        var message: String
        /// The message is good news (a green dot beside it).
        var messageGood: Bool
        /// Drawn only: a keyed card gone stale, which may still be on the
        /// air. It keeps UNKEY and shows no clock or readings.
        var staleKeyed = false

        init(stationName: String = "", link: Link = .connecting, roundTripMs: Int? = nil, retry: Retry? = nil,
             lostWhileKeyed: Bool = false, slice: Slice? = nil, meter: Meter? = nil, muted: Bool = false,
             keyed: Bool = false, keyedSince: Date? = nil, forwardWatts: Double = 0, swr: Double = 1,
             timeOutAt: Date? = nil, onAirFrom: String? = nil, message: String = "", messageGood: Bool = false) {
            self.stationName = stationName
            self.link = link
            self.roundTripMs = roundTripMs
            self.retry = retry
            self.lostWhileKeyed = lostWhileKeyed
            self.slice = slice
            self.meter = meter
            self.muted = muted
            self.keyed = keyed
            self.keyedSince = keyedSince
            self.forwardWatts = forwardWatts
            self.swr = swr
            self.timeOutAt = timeOutAt
            self.onAirFrom = onAirFrom
            self.message = message
            self.messageGood = messageGood
        }

        /// What the card and the island draw. A card gone stale (iOS passed
        /// the stale date the app gave it) no longer knows what the app
        /// knows: the app may have stopped. Keyed, it can't know whether
        /// this phone still transmits (the Core stops a key whose app went
        /// quiet, but the card doesn't know that either), so it shows no
        /// clock, readings or time-out, says it may still be on the air, and
        /// keeps UNKEY, which is harmless if the key has already ended.
        /// Listening, the link and signal go and the words say there's no
        /// news, unless a last message is up, which stays. Lost, the
        /// countdown gives way to the same words.
        func shown(stale: Bool) -> ContentState {
            guard stale else {
                return self
            }
            var shown = self
            shown.roundTripMs = nil
            if link != .lost {
                shown.link = .connecting
            }
            if keyed {
                shown.keyed = false
                shown.staleKeyed = true
                shown.keyedSince = nil
                shown.forwardWatts = 0
                shown.swr = 1
                shown.timeOutAt = nil
                shown.message = ActivityWords.noNewsKeyed
                shown.messageGood = false
            } else if link == .lost || message.isEmpty {
                shown.message = ActivityWords.noNews
                shown.messageGood = false
            }
            // A stale card can't know who is on the air now.
            shown.onAirFrom = nil
            if var slice = shown.slice {
                slice.signalDbm = nil
                slice.signalText = nil
                slice.signalSpoken = nil
                shown.slice = slice
            }
            return shown
        }
    }

    /// How long a keyed card stays good without a change from the app. The
    /// app refreshes a keyed card every second, so three seconds allows two
    /// late refreshes before it goes stale. (A card that isn't keyed stays
    /// good for a minute; the app sets both.)
    static let keyedFreshFor: TimeInterval = 3

    /// When the activity began: iOS ends it after eight hours.
    var startedAt: Date
}
