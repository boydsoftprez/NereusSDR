// NereusSDR for iOS: the model a test builds keeps what it saves to itself, not in the host's standard defaults
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMedia
@testable import NereusSDR

/// A store for one model, gone with it.
private final class ScratchDefaults: UserDefaults, @unchecked Sendable {
    private let name: String

    init?(named name: String) {
        self.name = name
        super.init(suiteName: name)
    }

    deinit {
        removePersistentDomain(forName: name)
    }

    static func make() -> UserDefaults {
        guard let scratch = ScratchDefaults(named: "AppModelScratch-\(UUID().uuidString)") else {
            fatalError("a unique suite name was refused")
        }
        return scratch
    }
}

extension AppModel {
    /// Every test that builds `AppModel` without naming a settings store comes
    /// here rather than to the app's initializer: Swift prefers the overload
    /// with fewer defaulted parameters, and this one has no `phoneSettings`.
    /// The parameter types match the app's exactly, because an optional
    /// conversion here would make the app's initializer the better match.
    /// The app's defaults are the host's standard defaults, which every test
    /// shares and which outlive the run, so a mute or route saved by one test
    /// would be restored by the next. `PhoneDefaultsIsolationTests` fails if
    /// this overload stops being chosen. Its parameters follow the app's
    /// initializer; a test using a parameter missing here reaches the app's.
    convenience init(mediaPeerFactory: MediaControlClient.PeerFactory? = nil,
                     remoteMediaPeerFactory: RemotePeerFactory? = nil,
                     playback: AudioPlaybackCore? = nil,
                     audio: AudioSessionController? = nil,
                     microphone: (@Sendable (MediaUplink) -> any MicrophoneSource)? = nil,
                     displaySettings: BandDisplaySettingsStore = BandDisplaySettingsStore(defaults: ScratchDefaults.make()),
                     meterSettings: SMeterSettingsStore = SMeterSettingsStore(defaults: ScratchDefaults.make()),
                     platform: TransmitModel.Platform = TransmitModel.Platform(),
                     sessionSources: SessionController.Sources = .steady(),
                     mirrorClock: any LinkClock = SystemLinkClock()) {
        self.init(phoneSettings: PhoneSettings(defaults: ScratchDefaults.make()),
                  mediaPeerFactory: mediaPeerFactory,
                  remoteMediaPeerFactory: remoteMediaPeerFactory,
                  playback: playback,
                  audio: audio,
                  microphone: microphone,
                  displaySettings: displaySettings,
                  meterSettings: meterSettings,
                  platform: platform,
                  sessionSources: sessionSources,
                  mirrorClock: mirrorClock)
    }
}
