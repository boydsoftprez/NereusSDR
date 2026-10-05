// NereusSDR for iOS: the settings this phone keeps for itself, in UserDefaults
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMedia

/// The settings Setup marks This phone: kept on the phone in `UserDefaults`,
/// never sent to the Core. Each screen task adds its own settings here as
/// named properties over these accessors. Keys are stored under
/// ``keyPrefix`` so they never meet a system or library key.
@MainActor
final class PhoneSettings: ObservableObject {
    static let keyPrefix = "phone."

    private let defaults: UserDefaults

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
    }

    func bool(_ key: String, default fallback: Bool) -> Bool {
        defaults.object(forKey: Self.keyPrefix + key) as? Bool ?? fallback
    }

    func setBool(_ value: Bool, for key: String) {
        store(value, for: key)
    }

    func integer(_ key: String, default fallback: Int) -> Int {
        defaults.object(forKey: Self.keyPrefix + key) as? Int ?? fallback
    }

    func setInteger(_ value: Int, for key: String) {
        store(value, for: key)
    }

    func double(_ key: String, default fallback: Double) -> Double {
        defaults.object(forKey: Self.keyPrefix + key) as? Double ?? fallback
    }

    func setDouble(_ value: Double, for key: String) {
        store(value, for: key)
    }

    func string(_ key: String, default fallback: String) -> String {
        defaults.string(forKey: Self.keyPrefix + key) ?? fallback
    }

    func setString(_ value: String, for key: String) {
        store(value, for: key)
    }

    // MARK: Touch (Setup, General, Navigation; spec section 5.1 item 9)

    /// Drag the band to tune the active slice.
    var dragToTune: Bool {
        get { bool(Self.dragToTuneKey, default: true) }
        set { setBool(newValue, for: Self.dragToTuneKey) }
    }

    /// Tap the band to tune the active slice.
    var tapToTune: Bool {
        get { bool(Self.tapToTuneKey, default: true) }
        set { setBool(newValue, for: Self.tapToTuneKey) }
    }

    /// A tap lands on the nearest step rather than the frequency under the finger.
    var snapTapToStep: Bool {
        get { bool(Self.snapTapToStepKey, default: false) }
        set { setBool(newValue, for: Self.snapTapToStepKey) }
    }

    /// Pinch the band to zoom.
    var pinchToZoom: Bool {
        get { bool(Self.pinchToZoomKey, default: true) }
        set { setBool(newValue, for: Self.pinchToZoomKey) }
    }

    /// What a double tap on the band does: Tune (the default), Center or
    /// None. A value this build does not know reads as the default.
    var doubleTapAction: TuneGestures.DoubleTapAction {
        get {
            TuneGestures.DoubleTapAction(rawValue: string(Self.doubleTapActionKey, default: ""))
                ?? TuneGestures.DoubleTapAction.standard
        }
        set { setString(newValue.rawValue, for: Self.doubleTapActionKey) }
    }

    /// How the band handles taps: at once unless a double tap would do
    /// something else.
    var tapHandling: TuneGestures.TapHandling {
        TuneGestures.tapHandling(tapToTune: tapToTune, doubleTap: doubleTapAction)
    }

    static let dragToTuneKey = "touch.dragToTune"
    static let tapToTuneKey = "touch.tapToTune"
    static let snapTapToStepKey = "touch.snapTapToStep"
    static let pinchToZoomKey = "touch.pinchToZoom"
    static let doubleTapActionKey = "touch.doubleTapAction"

    // MARK: Tuning dial (Setup, General, Navigation; D12, spec section 5.1 item 8)

    /// The tuning dial this phone shows: Off until someone picks one. A
    /// value this build does not know reads as Off.
    var dialKind: DialKind {
        get { DialKind(rawValue: string(Self.dialKindKey, default: "")) ?? DialKind.standard }
        set { setString(newValue.rawValue, for: Self.dialKindKey) }
    }

    /// The dial turns the other way: clockwise tunes down.
    var dialReversed: Bool {
        get { bool(Self.dialReversedKey, default: false) }
        set { setBool(newValue, for: Self.dialReversedKey) }
    }

    /// A light haptic tick on each of the dial's detents.
    var dialDetentTicks: Bool {
        get { bool(Self.dialDetentTicksKey, default: true) }
        set { setBool(newValue, for: Self.dialDetentTicksKey) }
    }

    /// A firmer haptic bump on each whole kilohertz the dial passes.
    var dialKilohertzBumps: Bool {
        get { bool(Self.dialKilohertzBumpsKey, default: true) }
        set { setBool(newValue, for: Self.dialKilohertzBumpsKey) }
    }

    static let dialKindKey = "dial.kind"
    static let dialReversedKey = "dial.reversed"
    static let dialDetentTicksKey = "dial.detentTicks"
    static let dialKilohertzBumpsKey = "dial.kilohertzBumps"

    // MARK: Audio on this phone (spec section 5.4 items 6 to 9; MON plays in headphones only, D80)

    /// The microphone the operator talks into: the iPhone's by default. A
    /// value this build does not know reads as the default.
    var microphone: MicrophoneChoice {
        get { MicrophoneChoice(rawValue: string(Self.microphoneKey, default: "")) ?? MicrophoneChoice.standard }
        set { setString(newValue.rawValue, for: Self.microphoneKey) }
    }

    /// The band is silenced while this phone transmits, so the speaker
    /// can't feed back into the microphone.
    var muteBandWhileTalking: Bool {
        get { bool(Self.muteBandWhileTalkingKey, default: true) }
        set { setBool(newValue, for: Self.muteBandWhileTalkingKey) }
    }

    /// The audio quality this phone asks for: High unless chosen. A value
    /// this build does not know reads as High.
    var audioQuality: AudioQualityChoice {
        get { AudioQualityChoice(rawValue: string(Self.audioQualityKey, default: "")) ?? AudioQualityChoice.standard }
        set { setString(newValue.rawValue, for: Self.audioQualityKey) }
    }

    static let microphoneKey = "audio.microphone"
    static let audioQualityKey = "audio.quality"
    static let muteBandWhileTalkingKey = "audio.muteBandWhileTalking"

    // MARK: Connecting (spec section 5.3)

    /// This phone's name on the Cores it pairs with (D65), as the operator
    /// last confirmed it; nil until the first pairing.
    var deviceName: String? {
        get { defaults.string(forKey: Self.keyPrefix + Self.deviceNameKey) }
        set {
            if let newValue {
                setString(newValue, for: Self.deviceNameKey)
            } else {
                reset(Self.deviceNameKey)
            }
        }
    }

    /// The Cores this phone must pair with again before it can sign in,
    /// by their identity keys in base64url (D70, D72). Public keys only.
    var coresNeedingPairing: Set<String> {
        get { Set(defaults.stringArray(forKey: Self.keyPrefix + Self.coresNeedingPairingKey) ?? []) }
        set { store(newValue.sorted(), for: Self.coresNeedingPairingKey) }
    }

    /// The Cores this phone last found too old to be renamed (without
    /// `deviceAdminVersion` 1, D76, D23), by their identity keys in
    /// base64url: Rename is greyed on their rows until one says otherwise.
    var coresWithoutRename: Set<String> {
        get { Set(defaults.stringArray(forKey: Self.keyPrefix + Self.coresWithoutRenameKey) ?? []) }
        set { store(newValue.sorted(), for: Self.coresWithoutRenameKey) }
    }

    /// This phone made a new key (D72), and some Core still lists the old
    /// one: the note under Your Cores says so until every Core is paired again.
    var madeNewKey: Bool {
        get { bool(Self.madeNewKeyKey, default: false) }
        set { setBool(newValue, for: Self.madeNewKeyKey) }
    }

    static let deviceNameKey = "connect.deviceName"
    static let coresNeedingPairingKey = "connect.coresNeedingPairing"
    static let madeNewKeyKey = "connect.madeNewKey"
    static let coresWithoutRenameKey = "connect.coresWithoutRename"

    // MARK: Battery and sessions (Setup, General; spec section 5.5 items 9 to 11, D27)

    /// Locked or in another app, the Core stops sending the band; the
    /// sound carries on.
    var soundOnlyWhenAway: Bool {
        get { bool(Self.soundOnlyWhenAwayKey, default: true) }
        set { setBool(newValue, for: Self.soundOnlyWhenAwayKey) }
    }

    /// When the screen stays on: Always by default (D27). A value this
    /// build does not know reads as the default.
    var keepScreenOn: KeepScreenOn {
        get { KeepScreenOn(rawValue: string(Self.keepScreenOnKey, default: "")) ?? KeepScreenOn.standard }
        set { setString(newValue.rawValue, for: Self.keepScreenOnKey) }
    }

    /// The sleep timer: off, or disconnect after 30, 60 or 120 minutes. A
    /// value this build does not offer reads as off.
    var sleepTimer: SleepTimer.Choice {
        get { SleepTimer.Choice(rawValue: integer(Self.sleepTimerKey, default: 0)) ?? .off }
        set {
            if newValue != sleepTimer { sleepTimerWillChange?(newValue) }
            setInteger(newValue.rawValue, for: Self.sleepTimerKey)
        }
    }

    /// The session retires an in-flight expiry before this phone stores a new choice.
    var sleepTimerWillChange: (@MainActor (SleepTimer.Choice) -> Void)?

    /// In Low Power Mode the band drops to Saver.
    var lowPowerDropsToSaver: Bool {
        get { bool(Self.lowPowerDropsToSaverKey, default: true) }
        set { setBool(newValue, for: Self.lowPowerDropsToSaverKey) }
    }

    /// A hot phone slows the band until it cools.
    var hotPhoneSlows: Bool {
        get { bool(Self.hotPhoneSlowsKey, default: true) }
        set { setBool(newValue, for: Self.hotPhoneSlowsKey) }
    }

    static let soundOnlyWhenAwayKey = "session.soundOnlyWhenAway"
    static let keepScreenOnKey = "session.keepScreenOn"
    static let sleepTimerKey = "session.sleepTimer"
    static let lowPowerDropsToSaverKey = "session.lowPowerDropsToSaver"
    static let hotPhoneSlowsKey = "session.hotPhoneSlows"

    // MARK: Data use (Setup, CAT & Network; spec section 5.4 items 11 to 13, D28)

    /// The data mode on Wi-Fi: Full or Balanced, Full by default. Anything
    /// else reads as the default.
    var wifiMode: SessionPolicy.Mode {
        get {
            let mode = SessionPolicy.Mode(rawValue: string(Self.wifiModeKey, default: ""))
            return mode.flatMap { SessionPolicy.Mode.wifiModes.contains($0) ? $0 : nil } ?? SessionPolicy.Mode.wifiDefault
        }
        set { setString(newValue.rawValue, for: Self.wifiModeKey) }
    }

    /// The data mode on cellular: Balanced by default.
    var cellularMode: SessionPolicy.Mode {
        get { SessionPolicy.Mode(rawValue: string(Self.cellularModeKey, default: "")) ?? SessionPolicy.Mode.cellularDefault }
        set { setString(newValue.rawValue, for: Self.cellularModeKey) }
    }

    /// Warn once past 5 GB a month on cellular.
    var cellularWarning: Bool {
        get { bool(Self.cellularWarningKey, default: true) }
        set { setBool(newValue, for: Self.cellularWarningKey) }
    }

    /// The first time on cellular has been noted on the band.
    var cellularNoteShown: Bool {
        get { bool(Self.cellularNoteShownKey, default: false) }
        set { setBool(newValue, for: Self.cellularNoteShownKey) }
    }

    static let wifiModeKey = "data.wifiMode"
    static let cellularModeKey = "data.cellularMode"
    static let cellularWarningKey = "data.cellularWarning"
    static let cellularNoteShownKey = "data.cellularNoteShown"

    /// Keeps a value without telling the screens: for counters that change
    /// every few seconds and that only their owner reads.
    func storeQuietly(_ value: Any, for key: String) {
        defaults.set(value, forKey: Self.keyPrefix + key)
    }

    /// Forgets a setting, so its default applies again.
    func reset(_ key: String) {
        objectWillChange.send()
        defaults.removeObject(forKey: Self.keyPrefix + key)
    }

    private func store(_ value: Any, for key: String) {
        objectWillChange.send()
        defaults.set(value, forKey: Self.keyPrefix + key)
    }
}
