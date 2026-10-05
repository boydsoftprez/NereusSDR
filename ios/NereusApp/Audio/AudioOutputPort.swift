// NereusSDR for iOS: one output the phone is playing through now, as the audio session reports it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// An output in the audio session's current route.
struct AudioOutputPort: Equatable, Sendable {
    enum Kind: Equatable, Sendable {
        case speaker
        case earpiece
        /// Anything outside the phone: Bluetooth, wired headphones, a car.
        case external
    }

    let kind: Kind
    let name: String
    /// True for headphones, AirPods and any other Bluetooth or wired output:
    /// when one goes away the band pauses instead of moving to the speaker.
    let pausesOnRemoval: Bool
    /// True for headphones and AirPods: worn on the ears, so the band and
    /// MON heard through them cannot feed back into the microphone.
    let isHeadphones: Bool

    init(kind: Kind, name: String, pausesOnRemoval: Bool = false, isHeadphones: Bool = false) {
        self.kind = kind
        self.name = name
        self.pausesOnRemoval = pausesOnRemoval
        self.isHeadphones = isHeadphones
    }

    /// The route this output plays.
    var route: AudioRoute {
        switch kind {
        case .speaker:
            return .speaker
        case .earpiece:
            return .earpiece
        case .external:
            return .external(name: name)
        }
    }
}
