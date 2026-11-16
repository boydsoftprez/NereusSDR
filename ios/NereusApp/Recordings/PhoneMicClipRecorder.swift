// NereusSDR for iOS: a voice keyer message recorded from this phone's own microphone, 48 kHz mono 16-bit WAV, up to two minutes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 18, the phone's voice keyer clips.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import AVFoundation
import Foundation
import Observation
import os

/// What writes the clip: this phone's microphone into a WAV file. The
/// app's is ``SystemMicClip``; the tests use a stand-in that records no sound.
@MainActor
protocol MicClipCapture: AnyObject {
    /// Starts writing the microphone to `url` as 48 kHz mono 16-bit PCM
    /// WAV, stopping by itself after `limit` seconds; false when it could
    /// not start.
    func begin(url: URL, limit: TimeInterval) -> Bool
    /// Stops writing; the file at the URL is whole.
    func end()
    /// Still writing: false once it has stopped, by ``end()`` or at the limit.
    var isRecording: Bool { get }
    /// The seconds written so far, kept after it stops.
    var seconds: TimeInterval { get }
    /// The microphone's latest peak, 0 for silence to 1 for full scale.
    func peak() -> Float
}

/// The phone's microphone through `AVAudioRecorder`, on the audio session
/// the band already plays on. iOS voice processing stays off, so the clip
/// is the voice as it comes from the microphone, before any processing;
/// the Core's transmit processing runs on each send (design section 3).
@MainActor
final class SystemMicClip: MicClipCapture {
    private var recorder: AVAudioRecorder?
    private var last: TimeInterval = 0

    func begin(url: URL, limit: TimeInterval) -> Bool {
        let settings: [String: Any] = [
            AVFormatIDKey: Int(kAudioFormatLinearPCM),
            AVSampleRateKey: PhoneMicClipRecorder.sampleRate,
            AVNumberOfChannelsKey: PhoneMicClipRecorder.channels,
            AVLinearPCMBitDepthKey: PhoneMicClipRecorder.bitDepth,
            AVLinearPCMIsFloatKey: false,
            AVLinearPCMIsBigEndianKey: false,
            AVLinearPCMIsNonInterleaved: false,
        ]
        guard let made = try? AVAudioRecorder(url: url, settings: settings) else {
            return false
        }
        made.isMeteringEnabled = true
        guard made.record(forDuration: limit) else {
            return false
        }
        recorder = made
        last = 0
        return true
    }

    func end() {
        if let recorder, recorder.isRecording {
            last = recorder.currentTime
        }
        recorder?.stop()
        recorder = nil
    }

    var isRecording: Bool { recorder?.isRecording ?? false }

    var seconds: TimeInterval {
        if let recorder, recorder.isRecording {
            last = recorder.currentTime
        }
        return last
    }

    func peak() -> Float {
        guard let recorder, recorder.isRecording else {
            return 0
        }
        recorder.updateMeters()
        let db = recorder.peakPower(forChannel: 0)
        return db <= -160 ? 0 : min(1, powf(10, db / 20))
    }
}

/// A voice keyer message from this phone's own microphone (design
/// 2026-11-15-recording-voice-keyer-dsr section 6.6; the plan's correction
/// table): the clip is recorded here, from the microphone before any
/// processing, as 48 kHz mono 16-bit WAV of at most two minutes, then
/// loaded into the Core's message with `voiceKeyer` import
/// (``RecordingsClient/importVoice(_:url:name:)``), as the desktop's
/// voice keyer panel records from the computer's microphone.
///
/// iOS asks for the microphone only here, the first time Record is
/// tapped, never before: opening the page asks nothing. Without the
/// permission a start says where to allow it.
@MainActor
@Observable
final class PhoneMicClipRecorder {
    nonisolated static let sampleRate = 48_000.0
    nonisolated static let channels = 1
    nonisolated static let bitDepth = 16
    /// The longest message the Core keeps ("Voice messages can be up to 2 minutes long.").
    nonisolated static let limitSeconds: TimeInterval = 120
    /// Words when the microphone is not allowed, as the transmit microphone says them.
    nonisolated static let notAllowedText = MicCapture.notAllowedText
    nonisolated static let couldNotStartText = "This phone's microphone could not start recording. Try again."

    /// One clip, whole, for the Core.
    struct Clip: Equatable {
        /// The message it goes into, 1 to 8.
        let slot: Int
        let url: URL
        let lengthMs: Int64
        /// It ran to the two minutes and stopped by itself.
        let reachedLimit: Bool
    }

    /// The message being recorded into; nil when not recording.
    private(set) var slot: Int?
    /// How long it has run.
    private(set) var elapsedMs: Int64 = 0
    /// The microphone's level, 0 to 1, while recording.
    private(set) var level: Float = 0
    var isRecording: Bool { slot != nil }

    @ObservationIgnored private let access: MicrophoneAccess
    @ObservationIgnored private let permitted: @MainActor () -> Bool
    @ObservationIgnored private let capture: MicClipCapture
    @ObservationIgnored private let folder: URL
    @ObservationIgnored private var url: URL?
    @ObservationIgnored private var starting = false
    private static let logger = Logger(subsystem: "NereusSDR", category: "recordings.clip")

    init(access: MicrophoneAccess,
         permitted: @escaping @MainActor () -> Bool = { AVAudioApplication.shared.recordPermission == .granted },
         capture: MicClipCapture? = nil,
         folder: URL = FileManager.default.temporaryDirectory.appendingPathComponent("VoiceKeyer", isDirectory: true)) {
        self.access = access
        self.permitted = permitted
        self.capture = capture ?? SystemMicClip()
        self.folder = folder
    }

    /// Starts recording into Mn. The first time, iOS asks for the
    /// microphone here. Returns the words for why it could not start, or
    /// nil once it records.
    func start(slot: Int) async -> String? {
        guard self.slot == nil, !starting else {
            return nil
        }
        starting = true
        defer { starting = false }
        if access.needsAsking {
            guard await access.ask() else {
                return Self.notAllowedText
            }
        } else if !permitted() {
            return Self.notAllowedText
        }
        let file = folder.appendingPathComponent("M\(slot)-\(UUID().uuidString).wav")
        do {
            try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        } catch {
            Self.logger.info("The clip folder could not be made: \(error.localizedDescription, privacy: .public)")
            return Self.couldNotStartText
        }
        guard capture.begin(url: file, limit: Self.limitSeconds) else {
            try? FileManager.default.removeItem(at: file)
            return Self.couldNotStartText
        }
        self.slot = slot
        url = file
        elapsedMs = 0
        level = 0
        return nil
    }

    /// Reads the time and the level. Returns the clip once the recording
    /// has stopped by itself at the two minutes; nil otherwise.
    func tick() -> Clip? {
        guard slot != nil else {
            return nil
        }
        elapsedMs = Int64((capture.seconds * 1000).rounded())
        level = capture.peak()
        if !capture.isRecording {
            return finish()
        }
        return nil
    }

    /// Stops recording and returns the clip; nil when nothing was recording.
    func finish() -> Clip? {
        guard let slot, let url else {
            return nil
        }
        let seconds = capture.isRecording ? capture.seconds : max(capture.seconds, Double(elapsedMs) / 1000)
        capture.end()
        let reached = seconds >= Self.limitSeconds - 0.05
        let lengthMs = reached ? Int64(Self.limitSeconds * 1000) : Int64((seconds * 1000).rounded())
        reset()
        return Clip(slot: slot, url: url, lengthMs: lengthMs, reachedLimit: reached)
    }

    /// Stops and throws the clip away.
    func cancel() {
        guard let url else {
            return
        }
        capture.end()
        try? FileManager.default.removeItem(at: url)
        reset()
    }

    private func reset() {
        slot = nil
        url = nil
        level = 0
    }
}
