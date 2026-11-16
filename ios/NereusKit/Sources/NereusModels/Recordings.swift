// NereusSDR for iOS: the Core's recordings document, its list's entries and this device's own playback, as the Core sends them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 15, the phone's copy of the recordings document.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Recording plan Task 28, the document's `dsr` part (version 4)
//   and the recorder's three settings. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation

/// The Core's recordings, as its `recordings` object sends them (the `json`
/// and `revision` properties, `recordingsVersion` 1 to 4; link document,
/// the notes on the `recordings` key). The phone only reads it: the
/// `recordings.*` and `voiceKeyer.*` verbs change it. Keys the phone does
/// not know are ignored; a document whose known keys have the wrong shape
/// decodes to nil. The recordings themselves are not in it: a device asks
/// for them with `recordings.list`.
public struct RecordingsDocument: Equatable, Sendable {
    /// What a slice's Record is doing.
    public enum SliceState: String, Equatable, Sendable, Decodable {
        case idle, armed, recording
    }

    /// One open slice's Record, as every device sees it.
    public struct Slice: Equatable, Sendable, Decodable {
        /// The slice's number: 0 is Slice A.
        public let slice: Int
        public let state: SliceState
        /// ISO 8601 UTC, "" while idle.
        public let startUtc: String
        /// Who started it: "" for the Core's own window.
        public let startedBy: String
        /// The running time when the document was built.
        public let elapsedMs: Int64
        /// The recording keeps this station's own transmitted voice.
        public let myVoice: Bool

        public init(slice: Int, state: SliceState, startUtc: String, startedBy: String, elapsedMs: Int64,
                    myVoice: Bool) {
            self.slice = slice
            self.state = state
            self.startUtc = startUtc
            self.startedBy = startedBy
            self.elapsedMs = elapsedMs
            self.myVoice = myVoice
        }
    }

    /// Why the last recording was kept.
    public enum KeptReason: String, Equatable, Sendable {
        /// Stopped on the Core's own window.
        case here
        /// Stopped from a device, named in ``Kept/device``.
        case device
        /// Stopped after the quiet time.
        case quiet
        /// The disk filled.
        case diskFull
        /// Kept as WAV because MP3 could not be made.
        case wav
    }

    /// The last recording kept.
    public struct Kept: Equatable, Sendable {
        /// Moves by one for each recording kept; 0 means none yet.
        public let seq: Int64
        public let slice: Int
        public let id: String
        public let name: String
        public let lengthMs: Int64
        /// Nil before the first recording, or for a reason this app does not know.
        public let reason: KeptReason?
        /// Who stopped it, in the Core's words.
        public let device: String

        public init(seq: Int64, slice: Int, id: String, name: String, lengthMs: Int64, reason: KeptReason?,
                    device: String) {
            self.seq = seq
            self.slice = slice
            self.id = id
            self.name = name
            self.lengthMs = lengthMs
            self.reason = reason
            self.device = device
        }
    }

    /// What the Core's recordings folder holds.
    public struct Index: Equatable, Sendable, Decodable {
        /// Moves by one each time the list of recordings changes.
        public let revision: Int64
        public let count: Int
        public let usedBytes: Int64
        public let freeBytes: Int64
        /// Where the Core keeps them, in the Core's own spelling.
        public let folder: String

        public init(revision: Int64, count: Int, usedBytes: Int64, freeBytes: Int64, folder: String) {
            self.revision = revision
            self.count = count
            self.usedBytes = usedBytes
            self.freeBytes = freeBytes
            self.folder = folder
        }
    }

    /// One disk the Core can keep recordings on.
    public struct Disk: Equatable, Sendable, Decodable {
        public let root: String
        public let name: String
        public let free: Int64
        public let total: Int64
        /// An SD card.
        public let sd: Bool
        /// The Core's own disk, which `settings.disk` names as "".
        public let own: Bool

        public init(root: String, name: String, free: Int64, total: Int64, sd: Bool, own: Bool) {
            self.root = root
            self.name = name
            self.free = free
            self.total = total
            self.sd = sd
            self.own = own
        }
    }

    /// The file a recording is kept as.
    public enum Format: String, Equatable, Sendable, Decodable {
        case mp3, wav
    }

    /// The recording settings, which live on the Core.
    public struct Settings: Equatable, Sendable {
        public let keepMyVoice: Bool
        public let sidesApart: Bool
        public let autoStart: Bool
        public let autoStop: Bool
        public let quietMinutes: Int
        public let format: Format
        /// "" for the Core's own disk.
        public let disk: String
        public let disks: [Disk]
        /// The on-air switches (version 2), false on an older Core.
        public let onAirUseTxEq: Bool
        public let onAirUseCompressor: Bool
        public let onAirUseLeveler: Bool
        public let onAirUseCfc: Bool
        public let onAirUsePhaseRotator: Bool

        public init(keepMyVoice: Bool, sidesApart: Bool, autoStart: Bool, autoStop: Bool, quietMinutes: Int,
                    format: Format, disk: String, disks: [Disk], onAirUseTxEq: Bool = false,
                    onAirUseCompressor: Bool = false, onAirUseLeveler: Bool = false, onAirUseCfc: Bool = false,
                    onAirUsePhaseRotator: Bool = false) {
            self.keepMyVoice = keepMyVoice
            self.sidesApart = sidesApart
            self.autoStart = autoStart
            self.autoStop = autoStop
            self.quietMinutes = quietMinutes
            self.format = format
            self.disk = disk
            self.disks = disks
            self.onAirUseTxEq = onAirUseTxEq
            self.onAirUseCompressor = onAirUseCompressor
            self.onAirUseLeveler = onAirUseLeveler
            self.onAirUseCfc = onAirUseCfc
            self.onAirUsePhaseRotator = onAirUsePhaseRotator
        }
    }

    /// A slice playing the past (`dsr.rewound`).
    public struct Rewound: Equatable, Sendable, Decodable {
        public let slice: Int
        /// The moment played when the document was built, ns since 1970 UTC.
        public let utcNs: Int64
        /// The slice's frequency now.
        public let frequencyHz: Int64
        /// The frequency LIVE returns it to.
        public let liveFrequencyHz: Int64

        public init(slice: Int, utcNs: Int64, frequencyHz: Int64, liveFrequencyHz: Int64) {
            self.slice = slice
            self.utcNs = utcNs
            self.frequencyHz = frequencyHz
            self.liveFrequencyHz = liveFrequencyHz
        }
    }

    /// The saved whole-pan stretch Slice A plays (`dsr.stretch`).
    public struct Stretch: Equatable, Sendable, Decodable {
        public let id: String
        public let fromUtcNs: Int64
        public let toUtcNs: Int64

        public init(id: String, fromUtcNs: Int64, toUtcNs: Int64) {
            self.id = id
            self.fromUtcNs = fromUtcNs
            self.toUtcNs = toUtcNs
        }
    }

    /// A receiver the recorder keeps now (`dsr.receivers`).
    public struct Receiver: Equatable, Sendable, Decodable {
        public let receiver: Int
        public let rateHz: Int64

        public init(receiver: Int, rateHz: Int64) {
            self.receiver = receiver
            self.rateHz = rateHz
        }
    }

    /// The Digital Spectrum Recorder (version 4): its settings, why it does
    /// not record although on, the loop's figures, the slices playing the
    /// past, the stretch Slice A plays and the saves under way.
    public struct Dsr: Equatable, Sendable, Decodable {
        public let enabled: Bool
        /// "" for the Core's own disk.
        public let disk: String
        public let sizeLimitGb: Int
        /// It writes now, which it does only while the radio streams.
        public let recording: Bool
        /// Why it does not record although on, in the Core's words; "" otherwise.
        public let reason: String
        public let sizeLimitBytes: Int64
        public let usedBytes: Int64
        /// Where the next second goes, in bytes from the loop's start.
        public let writePosition: Int64
        /// ms since 1970 UTC, 0 when nothing is kept.
        public let oldestUtcMs: Int64
        public let newestUtcMs: Int64
        public let hoursHeld: Double
        public let overflows: Int64
        public let rewound: [Rewound]
        public let stretch: Stretch?
        /// Stretches being saved now.
        public let saving: Int
        /// The receivers streaming now and their rates; empty from a Core that does not send them.
        public let receivers: [Receiver]
        /// The disk bytes those receivers take an hour; 0 when not known.
        public let bytesPerHour: Int64

        public init(enabled: Bool, disk: String = "", sizeLimitGb: Int = 10, recording: Bool, reason: String = "",
                    sizeLimitBytes: Int64 = 0, usedBytes: Int64 = 0, writePosition: Int64 = 0, oldestUtcMs: Int64 = 0,
                    newestUtcMs: Int64 = 0, hoursHeld: Double = 0, overflows: Int64 = 0, rewound: [Rewound] = [],
                    stretch: Stretch? = nil, saving: Int = 0, receivers: [Receiver] = [], bytesPerHour: Int64 = 0) {
            self.enabled = enabled
            self.disk = disk
            self.sizeLimitGb = sizeLimitGb
            self.recording = recording
            self.reason = reason
            self.sizeLimitBytes = sizeLimitBytes
            self.usedBytes = usedBytes
            self.writePosition = writePosition
            self.oldestUtcMs = oldestUtcMs
            self.newestUtcMs = newestUtcMs
            self.hoursHeld = hoursHeld
            self.overflows = overflows
            self.rewound = rewound
            self.stretch = stretch
            self.saving = saving
            self.receivers = receivers
            self.bytesPerHour = bytesPerHour
        }

        private enum CodingKeys: String, CodingKey {
            case enabled, disk, sizeLimitGb, recording, reason, sizeLimitBytes, usedBytes, writePosition
            case oldestUtcMs, newestUtcMs, hoursHeld, overflows, rewound, stretch, saving, receivers, bytesPerHour
        }

        /// The settings and the loop's figures are required; what later
        /// steps added (rewound, the stretch, the saves, the receivers) may
        /// be missing and reads as none.
        public init(from decoder: Decoder) throws {
            let keys = try decoder.container(keyedBy: CodingKeys.self)
            enabled = try keys.decode(Bool.self, forKey: .enabled)
            disk = try keys.decode(String.self, forKey: .disk)
            sizeLimitGb = try keys.decode(Int.self, forKey: .sizeLimitGb)
            recording = try keys.decode(Bool.self, forKey: .recording)
            reason = try keys.decode(String.self, forKey: .reason)
            sizeLimitBytes = try keys.decode(Int64.self, forKey: .sizeLimitBytes)
            usedBytes = try keys.decode(Int64.self, forKey: .usedBytes)
            writePosition = try keys.decode(Int64.self, forKey: .writePosition)
            oldestUtcMs = try keys.decode(Int64.self, forKey: .oldestUtcMs)
            newestUtcMs = try keys.decode(Int64.self, forKey: .newestUtcMs)
            hoursHeld = try keys.decode(Double.self, forKey: .hoursHeld)
            overflows = try keys.decodeIfPresent(Int64.self, forKey: .overflows) ?? 0
            rewound = try keys.decodeIfPresent([Rewound].self, forKey: .rewound) ?? []
            stretch = try keys.decodeIfPresent(Stretch.self, forKey: .stretch)
            saving = try keys.decodeIfPresent(Int.self, forKey: .saving) ?? 0
            receivers = try keys.decodeIfPresent([Receiver].self, forKey: .receivers) ?? []
            bytesPerHour = try keys.decodeIfPresent(Int64.self, forKey: .bytesPerHour) ?? 0
        }

        /// The slice's past, while it plays one.
        public func rewound(_ slice: Int) -> Rewound? {
            rewound.first { $0.slice == slice }
        }
    }

    /// The mirrored object's key.
    public static let objectKey = "recordings"

    /// The document's own `version`, which follows the Core's `recordingsVersion`.
    public let version: Int
    /// The `revision` property beside the document.
    public let revision: Int64
    public let slices: [Slice]
    public let kept: Kept
    public let index: Index
    public let settings: Settings
    /// The voice keyer (version 3); nil on an older Core.
    public let voiceKeyer: VoiceKeyerState?
    /// The Digital Spectrum Recorder (version 4); nil on an older Core.
    public let dsr: Dsr?

    /// One slice's Record; nil for a slice that is not open.
    public func slice(_ slice: Int) -> Slice? {
        slices.first { $0.slice == slice }
    }

    /// Nil for a revision outside the link's range (an unsigned 32 bit
    /// count), or a document that is empty, as a Core sends before it
    /// publishes, or malformed.
    public init?(json: String, revision: Int64) {
        guard (0...Int64(UInt32.max)).contains(revision),
              let document = try? JSONDecoder().decode(Wire.self, from: Data(json.utf8)),
              document.version >= 1 else {
            return nil
        }
        self.revision = revision
        version = document.version
        slices = document.slices
        kept = Kept(seq: document.kept.seq, slice: document.kept.slice, id: document.kept.id,
                    name: document.kept.name, lengthMs: document.kept.lengthMs,
                    reason: KeptReason(rawValue: document.kept.reason), device: document.kept.device)
        index = document.index
        let wire = document.settings
        settings = Settings(keepMyVoice: wire.keepMyVoice, sidesApart: wire.sidesApart, autoStart: wire.autoStart,
                            autoStop: wire.autoStop, quietMinutes: wire.quietMinutes, format: wire.format,
                            disk: wire.disk, disks: wire.disks, onAirUseTxEq: wire.onAirUseTxEq ?? false,
                            onAirUseCompressor: wire.onAirUseCompressor ?? false,
                            onAirUseLeveler: wire.onAirUseLeveler ?? false, onAirUseCfc: wire.onAirUseCfc ?? false,
                            onAirUsePhaseRotator: wire.onAirUsePhaseRotator ?? false)
        voiceKeyer = document.voiceKeyer?.state
        dsr = document.dsr
    }

    // MARK: The wire

    private struct WireKept: Decodable {
        let seq: Int64
        let slice: Int
        let id: String
        let name: String
        let lengthMs: Int64
        let reason: String
        let device: String
    }

    private struct WireSettings: Decodable {
        let keepMyVoice: Bool
        let sidesApart: Bool
        let autoStart: Bool
        let autoStop: Bool
        let quietMinutes: Int
        let format: Format
        let disk: String
        let disks: [Disk]
        let onAirUseTxEq: Bool?
        let onAirUseCompressor: Bool?
        let onAirUseLeveler: Bool?
        let onAirUseCfc: Bool?
        let onAirUsePhaseRotator: Bool?
    }

    /// A Core without a voice keyer sends an empty `voiceKeyer` object.
    private struct WireVoiceKeyer: Decodable {
        let state: VoiceKeyerState?

        init(from decoder: Decoder) throws {
            let keys = try decoder.container(keyedBy: AnyKey.self)
            state = keys.allKeys.isEmpty ? nil : try VoiceKeyerState(from: decoder)
        }
    }

    private struct AnyKey: CodingKey {
        let stringValue: String
        let intValue: Int? = nil

        init?(stringValue: String) {
            self.stringValue = stringValue
        }

        init?(intValue: Int) {
            return nil
        }
    }

    private struct Wire: Decodable {
        let version: Int
        let slices: [Slice]
        let kept: WireKept
        let index: Index
        let settings: WireSettings
        let voiceKeyer: WireVoiceKeyer?
        let dsr: Dsr?
    }
}

/// One recording as `recordings.list` sends it.
public struct RecordingEntry: Equatable, Sendable, Identifiable {
    /// What a recording is.
    public enum Kind: String, Equatable, Sendable, Decodable {
        /// One slice's contact.
        case contact = "Contact"
        /// A whole panadapter (the Digital Spectrum Recorder).
        case wholePan = "WholePan"
        /// A sound file.
        case sound = "Sound"
    }

    public let id: String
    public let kind: Kind
    /// The slice's number, 0 for Slice A, or -1 for none.
    public let slice: Int
    /// ISO 8601 UTC.
    public let startUtc: String
    public let lengthMs: Int64
    public let frequencyHz: Int64
    public let mode: String
    public let call: String
    public let name: String
    public let fileName: String
    public let sizeBytes: Int64

    public init(id: String, kind: Kind, slice: Int, startUtc: String, lengthMs: Int64, frequencyHz: Int64,
                mode: String, call: String, name: String, fileName: String, sizeBytes: Int64) {
        self.id = id
        self.kind = kind
        self.slice = slice
        self.startUtc = startUtc
        self.lengthMs = lengthMs
        self.frequencyHz = frequencyHz
        self.mode = mode
        self.call = call
        self.name = name
        self.fileName = fileName
        self.sizeBytes = sizeBytes
    }

    /// The `entries` text of a `recordings.list` answer, newest first. Nil
    /// when it is not a JSON array; an entry without an id or of a kind this
    /// app does not know is left out, as the Core's own reader leaves it.
    public static func list(json: String) -> [RecordingEntry]? {
        guard let raw = try? JSONSerialization.jsonObject(with: Data(json.utf8)) as? [Any] else {
            return nil
        }
        return raw.compactMap { element in
            guard let object = element as? [String: Any],
                  let data = try? JSONSerialization.data(withJSONObject: object),
                  let entry = try? JSONDecoder().decode(RecordingEntry.self, from: data),
                  !entry.id.isEmpty else {
                return nil
            }
            return entry
        }
    }
}

extension RecordingEntry: Decodable {}

/// One page of the Core's recordings.
public struct RecordingsPage: Equatable, Sendable {
    public let entries: [RecordingEntry]
    /// Where the page starts in the whole list.
    public let offset: Int
    /// How many recordings match, on every page.
    public let total: Int
    /// The asking list's own number, as the Core gave it back; nil when none was sent.
    public let reader: Int64?

    public init(entries: [RecordingEntry], offset: Int, total: Int, reader: Int64?) {
        self.entries = entries
        self.offset = offset
        self.total = total
        self.reader = reader
    }
}

/// This device's own playback, which the Core sends to this device's
/// sessions and no other's (`recordings.playback`).
public struct RecordingsPlayback: Equatable, Sendable {
    /// The recording playing or paused; "" once playback has stopped.
    public let id: String
    public let positionMs: Int64
    public let lengthMs: Int64
    public let playing: Bool
    /// This device's key sends the recording on the air (version 2).
    public let onAir: Bool

    public init(id: String, positionMs: Int64, lengthMs: Int64, playing: Bool, onAir: Bool) {
        self.id = id
        self.positionMs = positionMs
        self.lengthMs = lengthMs
        self.playing = playing
        self.onAir = onAir
    }

    /// Nothing plays.
    public static let stopped = RecordingsPlayback(id: "", positionMs: 0, lengthMs: 0, playing: false, onAir: false)
}

/// A change to the recording settings, `recordings.setSettings`: only the
/// fields set are sent, in the link's order.
public struct RecordingsSettingsChange: Equatable, Sendable {
    public var keepMyVoice: Bool?
    public var sidesApart: Bool?
    public var autoStart: Bool?
    public var autoStop: Bool?
    public var quietMinutes: Int?
    public var format: RecordingsDocument.Format?
    /// "" for the Core's own disk.
    public var disk: String?
    public var onAirUseTxEq: Bool?
    public var onAirUseCompressor: Bool?
    public var onAirUseLeveler: Bool?
    public var onAirUseCfc: Bool?
    public var onAirUsePhaseRotator: Bool?
    /// The Digital Spectrum Recorder's three (version 4).
    public var dsrEnabled: Bool?
    /// "" for the Core's own disk.
    public var dsrDisk: String?
    /// 1 to 100000 GB.
    public var dsrSizeLimitGb: Int?

    public init(keepMyVoice: Bool? = nil, sidesApart: Bool? = nil, autoStart: Bool? = nil, autoStop: Bool? = nil,
                quietMinutes: Int? = nil, format: RecordingsDocument.Format? = nil, disk: String? = nil,
                onAirUseTxEq: Bool? = nil, onAirUseCompressor: Bool? = nil, onAirUseLeveler: Bool? = nil,
                onAirUseCfc: Bool? = nil, onAirUsePhaseRotator: Bool? = nil, dsrEnabled: Bool? = nil,
                dsrDisk: String? = nil, dsrSizeLimitGb: Int? = nil) {
        self.keepMyVoice = keepMyVoice
        self.sidesApart = sidesApart
        self.autoStart = autoStart
        self.autoStop = autoStop
        self.quietMinutes = quietMinutes
        self.format = format
        self.disk = disk
        self.onAirUseTxEq = onAirUseTxEq
        self.onAirUseCompressor = onAirUseCompressor
        self.onAirUseLeveler = onAirUseLeveler
        self.onAirUseCfc = onAirUseCfc
        self.onAirUsePhaseRotator = onAirUsePhaseRotator
        self.dsrEnabled = dsrEnabled
        self.dsrDisk = dsrDisk
        self.dsrSizeLimitGb = dsrSizeLimitGb
    }

    /// One value of the change, as it goes on the wire.
    public enum Value: Equatable, Sendable {
        case flag(Bool)
        case whole(Int64)
        case text(String)
    }

    /// The fields set, by their `recordings.setSettings` names, in the link's order.
    public var fields: [(name: String, value: Value)] {
        var out: [(name: String, value: Value)] = []
        func flag(_ name: String, _ value: Bool?) {
            if let value { out.append((name, .flag(value))) }
        }
        flag("keepMyVoice", keepMyVoice)
        flag("sidesApart", sidesApart)
        flag("autoStart", autoStart)
        flag("autoStop", autoStop)
        if let quietMinutes { out.append(("quietMinutes", .whole(Int64(quietMinutes)))) }
        if let format { out.append(("format", .text(format.rawValue))) }
        if let disk { out.append(("disk", .text(disk))) }
        flag("onAirUseTxEq", onAirUseTxEq)
        flag("onAirUseCompressor", onAirUseCompressor)
        flag("onAirUseLeveler", onAirUseLeveler)
        flag("onAirUseCfc", onAirUseCfc)
        flag("onAirUsePhaseRotator", onAirUsePhaseRotator)
        flag("dsrEnabled", dsrEnabled)
        if let dsrDisk { out.append(("dsrDisk", .text(dsrDisk))) }
        if let dsrSizeLimitGb { out.append(("dsrSizeLimitGb", .whole(Int64(dsrSizeLimitGb)))) }
        return out
    }

    /// The change sets one of the on-air switches, which need `recordingsVersion` 2.
    public var setsOnAirSwitches: Bool {
        onAirUseTxEq != nil || onAirUseCompressor != nil || onAirUseLeveler != nil || onAirUseCfc != nil
            || onAirUsePhaseRotator != nil
    }

    /// The change sets one of the recorder's settings, which need `recordingsVersion` 4.
    public var setsDsr: Bool {
        dsrEnabled != nil || dsrDisk != nil || dsrSizeLimitGb != nil
    }

    /// Nothing is set.
    public var isEmpty: Bool { fields.isEmpty }
}
