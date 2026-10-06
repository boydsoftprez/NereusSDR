// NereusSDR for iOS: the audio quality on this phone: High, Save data or Lossless, what it asks of the Core and how the Core answers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMedia
import NereusMirror
import NereusModels

/// The audio quality this phone asks for (R-IOS-09), kept on this phone.
enum AudioQualityChoice: String, CaseIterable, Sendable {
    /// Opus at 48 kbit/s, audio up to 20 kHz: the default.
    case high
    /// Opus at 24 kbit/s, audio up to 8 kHz.
    case saveData
    /// The Core's audio unchanged (L16), about 1.6 Mbit/s, for digital modes.
    case lossless

    static let standard = AudioQualityChoice.high

    /// The Opus bitrate the choice asks for: Lossless asks for High's, the
    /// Opus the Core runs when lossless cannot be carried.
    var opusBitrate: Int {
        switch self {
        case .high, .lossless:
            return 48000
        case .saveData:
            return 24000
        }
    }
}

/// Setup, Audio, On this phone, Audio quality (R-IOS-09, R-IOS-20; the
/// media control document, "Per-device audio quality"; the desktop's
/// audio quality, R-R3-23).
///
/// The choice goes to the media client as an ``MediaControlClient/AudioRequest``:
/// `opusBitrate` only from the catalogue's `audio.opusProfiles` (the client
/// sends it only to a Core that told this phone `audioQualityVersion` 1),
/// and lossless asked for as the desktop asks (`profile` lossless). The
/// microphone follows: 48 kbit/s, 24 under Save data, lossless where the
/// line carries it.
///
/// Each row is shown, greyed with its reason when the Core does not offer
/// it. High can always be chosen (sent with no bitrate to a Core without
/// the choice or without 48000 in its table), so Lossless is never the
/// only row left. Save data waits for the table, greyed
/// as being checked, not as not offered. The tick is on what the Core runs
/// once it says (so a refusal puts it back), else on the choice; a bitrate
/// the rows do not list ticks nothing. The notes are the Core's own words
/// for a refused bitrate, then, while Lossless is chosen, why Opus plays,
/// in the desktop's words (`RemoteAudioStatus.cpp`,
/// `remoteAudioQualityReasonText`), unless the Lossless row already says
/// it. On cellular with Lossless chosen, its row says its cost plainly.
@MainActor
final class AudioQualityModel: ObservableObject {
    /// What the Core last said it runs for this phone's main stream.
    struct Heard: Equatable, Sendable {
        var lossless: Bool
        /// The Opus bitrate it runs; nil while lossless or without detail.
        var opusBitrate: Int?
        var profileRefusal: MediaControlEvent.AudioContext.ProfileRefusal?
        /// The Core's words for a bitrate it did not take.
        var bitrateRefusal: String?

        init(lossless: Bool, opusBitrate: Int?, profileRefusal: MediaControlEvent.AudioContext.ProfileRefusal?,
             bitrateRefusal: String?) {
            self.lossless = lossless
            self.opusBitrate = opusBitrate
            self.profileRefusal = profileRefusal
            self.bitrateRefusal = bitrateRefusal
        }

        /// An enabled context's answer; nil for a disabled one.
        init?(_ context: MediaControlEvent.AudioContext) {
            guard context.enabled else {
                return nil
            }
            self.init(lossless: context.lossless, opusBitrate: context.encoder?.targetBitrate,
                      profileRefusal: context.profileRefusal, bitrateRefusal: context.opusBitrateRefusal)
        }
    }

    /// Everything the rows, the tick and the notes are read from.
    struct Inputs: Equatable {
        var chosen: AudioQualityChoice
        /// A session with a Core is up (its snapshot complete).
        var connected: Bool
        /// The Core takes `opusBitrate` (``MediaFeatureGates/audioQuality``).
        var qualityOffered: Bool
        /// The Core has the lossless profile (``MediaFeatureGates/audioProfile``).
        var profileOffered: Bool
        /// The catalogue's measured bitrates; nil from a Core that sends none.
        var opusBitrates: [Int]?
        var heard: Heard?
        /// The phone's link trial moved lossless back to Opus.
        var fallback: Bool
        /// The Core's catalogue has come (with or without a table).
        var catalogueLoaded = true
        /// The phone is on cellular now.
        var cellular = false
    }

    /// One row of Audio quality.
    struct Row: Identifiable, Equatable {
        let choice: AudioQualityChoice
        let title: String
        let detail: String
        let cost: String
        let enabled: Bool
        /// Why it is greyed; nil when it can be chosen.
        let reason: String?
        /// Said plainly on the row: Lossless's cost while chosen on cellular.
        let warning: String?

        var id: AudioQualityChoice {
            choice
        }
    }

    // MARK: The words

    static let olderCoreReason = "This Core does not offer a choice of audio quality. Updating the Core may help."
    static let notOfferedReason = "This Core does not offer this audio quality."
    static let checkingReason = "Checking what this Core offers."
    static let cellularLosslessWarning = "On cellular, Lossless uses about 720 MB an hour."
    // From the desktop's remoteAudioQualityReasonText (src/gui/RemoteAudioStatus.cpp).
    static let coreCannotSendText = "This Core cannot send lossless audio."
    static let coreNotAllowedText = "This Core does not allow lossless audio."
    static let connectionUnavailableText = "This connection could not set up lossless audio; staying on Opus."
    static let networkTooSlowText = "The network could not carry lossless audio; staying on Opus."
    /// Under the rows, after the desktop's tooltip.
    static let footer = "High and Save data are compressed. Lossless needs about 1.6 Mbit/s; if the "
        + "network cannot carry it, audio stays on Opus. Saved on this phone."

    static func title(_ choice: AudioQualityChoice) -> String {
        switch choice {
        case .high:
            return "High"
        case .saveData:
            return "Save data"
        case .lossless:
            return "Lossless"
        }
    }

    static func detail(_ choice: AudioQualityChoice) -> String {
        switch choice {
        case .high:
            return "Opus at 48 kbit/s, audio up to 20 kHz"
        case .saveData:
            return "Opus at 24 kbit/s, audio up to 8 kHz"
        case .lossless:
            return "The Core's audio unchanged, for digital modes"
        }
    }

    /// The estimated cost of an hour's sound, on the scale of Data use's
    /// Audio only: about 13 MB an hour at 24 kbit/s, the spec's figure;
    /// 48 kbit/s adds 24000 x 3600 / 8 = 10.8 MB; L16 at 1.6 Mbit/s is
    /// 1.6e6 x 3600 / 8 = 720 MB.
    static func cost(_ choice: AudioQualityChoice) -> String {
        switch choice {
        case .high:
            return "About 24 MB an hour"
        case .saveData:
            return "About 13 MB an hour"
        case .lossless:
            return "About 720 MB an hour"
        }
    }

    // MARK: Reading the inputs

    static func rows(_ inputs: Inputs) -> [Row] {
        AudioQualityChoice.allCases.map { choice in
            let reason = inputs.connected ? greyedReason(choice, inputs) : nil
            let warning = choice == .lossless && inputs.chosen == .lossless && inputs.cellular
                ? cellularLosslessWarning : nil
            return Row(choice: choice, title: title(choice), detail: detail(choice), cost: cost(choice),
                       enabled: reason == nil, reason: reason, warning: warning)
        }
    }

    private static func greyedReason(_ choice: AudioQualityChoice, _ inputs: Inputs) -> String? {
        switch choice {
        case .high:
            // Asked for with no bitrate where the Core has no choice or its
            // table lacks 48000, so it can always be chosen: Lossless is
            // never the only row left.
            return nil
        case .saveData:
            guard inputs.qualityOffered else {
                return olderCoreReason
            }
            guard inputs.catalogueLoaded else {
                return checkingReason
            }
            guard inputs.opusBitrates?.contains(choice.opusBitrate) == true else {
                return notOfferedReason
            }
            return nil
        case .lossless:
            guard inputs.profileOffered else {
                return coreCannotSendText
            }
            if inputs.heard?.profileRefusal == .notAllowed {
                return coreNotAllowedText
            }
            return nil
        }
    }

    /// The ticked row: what the Core runs once it says, else the choice.
    static func shown(_ inputs: Inputs) -> AudioQualityChoice? {
        if inputs.connected, let heard = inputs.heard {
            if heard.lossless {
                return .lossless
            }
            if let bitrate = heard.opusBitrate {
                return [AudioQualityChoice.high, .saveData].first { $0.opusBitrate == bitrate }
            }
        }
        return inputs.chosen
    }

    /// What the page says under the rows about the Core's answer.
    static func notes(_ inputs: Inputs) -> [String] {
        guard inputs.connected else {
            return []
        }
        var notes: [String] = []
        if let words = inputs.heard?.bitrateRefusal {
            notes.append(words)
        }
        // The desktop's order (RemoteMediaController.cpp, R-R3-23).
        var reason: String?
        if inputs.chosen == .lossless {
            if inputs.fallback {
                reason = networkTooSlowText
            } else if !inputs.profileOffered {
                reason = coreCannotSendText
            } else if let refusal = inputs.heard?.profileRefusal {
                reason = refusal == .notAllowed ? coreNotAllowedText : connectionUnavailableText
            }
        }
        // Said once: not again here when the Lossless row already says it.
        if let reason, reason != greyedReason(.lossless, inputs) {
            notes.append(reason)
        }
        return notes
    }

    /// What the media client asks the Core for.
    static func request(_ inputs: Inputs) -> MediaControlClient.AudioRequest {
        let bitrate = inputs.chosen.opusBitrate
        return MediaControlClient.AudioRequest(lossless: inputs.chosen == .lossless,
                                               opusBitrate: inputs.opusBitrates?.contains(bitrate) == true
                                                   ? bitrate : nil)
    }

    static func microphone(_ choice: AudioQualityChoice) -> MediaUplink.MicrophoneQuality {
        switch choice {
        case .high:
            return .high
        case .saveData:
            return .saveData
        case .lossless:
            return .lossless
        }
    }

    // MARK: The model

    @Published private(set) var inputs: Inputs

    var rows: [Row] {
        Self.rows(inputs)
    }

    var shown: AudioQualityChoice? {
        Self.shown(inputs)
    }

    var notes: [String] {
        Self.notes(inputs)
    }

    private let settings: PhoneSettings
    private let mirror: MirrorStore
    private let catalogFeed: CatalogFeed
    private let media: MediaControlClient
    private var watches: Set<AnyCancellable> = []
    private var refreshQueued = false
    /// The requests to the media client, in order.
    private var sending: Task<Void, Never>?

    /// `cellular` is the phone's network, on cellular or not, as the long
    /// session tracks it.
    init(settings: PhoneSettings, mirror: MirrorStore, catalogFeed: CatalogFeed, media: MediaControlClient,
         cellular: AnyPublisher<Bool, Never> = Just(false).eraseToAnyPublisher()) {
        self.settings = settings
        self.mirror = mirror
        self.catalogFeed = catalogFeed
        self.media = media
        inputs = Inputs(chosen: settings.audioQuality, connected: false, qualityOffered: false,
                        profileOffered: false, opusBitrates: nil, heard: nil, fallback: false,
                        catalogueLoaded: false)
        mirror.$isSnapshotComplete.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        cellular.removeDuplicates().sink { [weak self] onCellular in
            self?.inputs.cellular = onCellular
        }.store(in: &watches)
        apply(choosing: false)
    }

    /// The operator picks a row: kept on this phone and asked for at once.
    /// Picking Lossless again after a fall back gives it a fresh chance.
    func choose(_ choice: AudioQualityChoice) {
        settings.audioQuality = choice
        var next = inputs
        next.chosen = choice
        next.fallback = false
        // The tick follows the choice until the Core answers it.
        next.heard = nil
        inputs = next
        apply(choosing: true)
    }

    /// One of the media client's events.
    func receive(_ event: MediaControlEvent) {
        switch event {
        case .audioContext(let context):
            inputs.heard = Heard(context)
        case .losslessFallback:
            inputs.fallback = true
        default:
            break
        }
    }

    /// Waits for the requests to the media client so far (tests).
    func settle() async {
        await sending?.value
    }

    // MARK: Inside

    /// The store publishes before it changes: read it on the next turn.
    private func queueRefresh() {
        guard !refreshQueued else {
            return
        }
        refreshQueued = true
        Task { @MainActor [weak self] in
            self?.refresh()
        }
    }

    private func refresh() {
        refreshQueued = false
        let gates = MediaFeatureGates(agreedMinor: mirror.agreedMinor ?? 0) { [mirror] in
            mirror.capabilityVersion($0)
        }
        var next = inputs
        next.connected = mirror.isSnapshotComplete
        next.qualityOffered = gates.audioQuality
        next.profileOffered = gates.audioProfile
        next.opusBitrates = catalogFeed.catalog?.audio.opusProfiles?.map(\.bitrate)
        next.catalogueLoaded = catalogFeed.catalog != nil
        if !next.connected {
            // A new connection starts with nothing heard and no fall back.
            next.heard = nil
            next.fallback = false
        }
        guard next != inputs else {
            return
        }
        let bitratesChanged = next.opusBitrates != inputs.opusBitrates
        inputs = next
        if bitratesChanged {
            apply(choosing: false)
        }
    }

    private func apply(choosing: Bool) {
        let request = Self.request(inputs)
        media.uplink.microphoneQuality = Self.microphone(inputs.chosen)
        let previous = sending
        let media = media
        sending = Task {
            await previous?.value
            await media.setAudioRequest(request, choosing: choosing)
        }
    }
}
