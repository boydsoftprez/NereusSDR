// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording service.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the Core's recording service: one
//             contact recorder per slice on a worker thread, start and stop
//             from any device, start by itself and stop by itself (spec 6.1,
//             6.2, V1, V4, section 7). AI tooling: Anthropic Claude Code.
// 2026-11-15  J.J. Boyd / KG4VCF; the player: one playback per listener,
//             decoded on the worker, pulled by the audio thread (spec 6.1
//             Player, 6.3, V6). AI tooling: Anthropic Claude Code.
// 2026-11-15  J.J. Boyd / KG4VCF; on the air: keying from a listener that
//             plays sends its playback in place of its microphone, through
//             the on-air source (spec 3, 6.1, 6.3, V5, V6, A6). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the voice keyer (spec 6.6), and a file
//             played by its path (the voice keyer's Listen). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; runOnWorker and waitForWorker (the voice
//             keyer's imports convert on the worker), and the voice keyer
//             told when the radio's connection drops. AI tooling: Anthropic
//             Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the Digital Spectrum Recorder: its tap,
//             its writer on the worker, and its disk rules (spec 3, 6.7,
//             section 7, A10, A14, A17). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; rewind sound: a slice plays the past from
//             the recorder, LIVE, seek, and PTT while rewound (spec 3, 6.7,
//             recording plan Task 22). AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/audio/PlaybackFeed.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/OnAirSource.h"
#include "core/recordings/QuietRule.h"
#include "core/recordings/RecordingDisks.h"
#include "core/recordings/RecordingPlayer.h"
#include "core/recordings/RecordingSettings.h"
#include "core/recordings/VoiceKeyer.h"
#include "core/recordings/dsr/DsrRecorder.h"
#include "core/recordings/dsr/DsrReplay.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QString>

#include <array>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

class QThread;
class QTimer;

namespace NereusSDR {

class AudioEngine;
class ContactRecorder;
class MicProfileManager;
class RadioModel;
class RecordingStore;
struct KeyerIdentity;

enum class RecordState { Idle, Armed, Recording };
enum class KeptReason { StoppedHere, StoppedOnDevice, StoppedQuiet, DiskFull, KeptAsWav };

struct RecordingSliceState {
    int sliceId;
    RecordState state;
    QDateTime startUtc;
    QString startedBy;
    qint64 elapsedMs;
    bool hearingMyVoice;
};

struct KeptEvent {
    int sliceId;
    QString id;
    QString name;
    qint64 lengthMs;
    KeptReason reason;
    QString deviceName;
};

// The Core's recordings: built by a RadioModel that runs its own radio
// (Role::Local) and owned by it. Lives on the main (model) thread; owns the
// recordings worker thread, where the files are written. The audio taps
// copy into each recorder's rings and return (ContactRecorder).
//
// A device name of "" is the window or Core that runs the radio itself:
// stopping from there is StoppedHere; any other name is StoppedOnDevice.
class NEREUS_CORE_EXPORT RecordingService : public QObject, public PlaybackFeed {
    Q_OBJECT

public:
    // Slices with an id from 0 up to this can record.
    static constexpr int kMaxRecorders = 8;
    // The worker writes what the taps hold this often.
    static constexpr int kDrainIntervalMs = 20;
    // The quiet rule is checked this often while anything records.
    static constexpr int kQuietCheckIntervalMs = 1000;
    // NoiseFloorTracker::agcNoiseFloor() is the noise in one bin of a
    // 4096-point FFT of the receiver's stream (NoiseFloorTracker.h,
    // kAgcReferenceFftSize); the quiet rule compares the S-meter, which
    // reads the whole passband, so the floor is raised to the passband.
    static constexpr int kFloorReferenceBins = 4096;

    explicit RecordingService(RadioModel* radio, QObject* parent = nullptr);
    ~RecordingService() override;   // stops every recording, keeps them, joins the worker

    RecordingStore* store() const;
    RecordingSettings settings() const;
    void setSettings(const RecordingSettings& s);

    void startRecording(int sliceId, const QString& byDeviceName);
    void stopRecording(int sliceId, const QString& byDeviceName);
    QList<RecordingSliceState> sliceStates() const;

    // Stops every recording and keeps it, waiting for the files. DaemonApp
    // calls it before the radio goes; the destructor calls it too.
    void stopAll();

    // The transmit monitor (TxChannel::sip1OutputReady, on the transmit
    // thread): your voice for every recording. Copies and returns.
    void consumeMonitor(const float* mono, int frames, int sampleRateHz) noexcept;

    // MOX (MoxController::moxChanged).
    void setTransmitting(bool on);

    // ---- The player (spec 6.1, 6.3): one playback per listener, heard only
    // by that listener. The file is decoded on the worker into a ring at
    // 48 kHz; the audio side only reads the ring (pullPlayback). Playing
    // never keys the radio.
    static constexpr int kMaxPlaybackListeners = 8;
    static constexpr const char* kPlayWholePanReason = "Open it on the waterfall.";
    static constexpr const char* kPlayGoneReason = "That recording is no longer kept.";
    static constexpr const char* kPlayFailedReason = "That recording cannot be played.";
    static constexpr const char* kPlayBusyReason = "Too many devices are playing recordings.";

    // From the start, or from where a seek left a stopped playback of the
    // same recording. A whole-pan recording, a recording no longer kept or
    // one that cannot be opened is refused (playbackRefused).
    void play(const ListenerId& who, const QString& recordingId);
    // A file that is not in the store (a voice keyer message, Listen), the
    // same way: `playbackId` is what playback(who) names it.
    void playFile(const ListenerId& who, const QString& playbackId, const QString& path, qint64 lengthMs);
    // Ends the listener's playback; playback(who) is empty after.
    void stopPlayback(const ListenerId& who);
    // A playing listener goes on from there; a stopped one starts there.
    void seek(const ListenerId& who, qint64 positionMs);
    // The position is what the audio side has taken, not a wall clock.
    std::optional<PlaybackState> playback(const ListenerId& who) const;
    // Audio thread: up to `frames` mono 48 kHz samples of who's playback,
    // the rest silence; returns the frames that were the recording. No lock,
    // no wait, no allocation.
    int pullPlayback(const ListenerId& who, float* mono48k, int frames) noexcept;

    // The speakers that hear a listener's playback, as AudioEngine's
    // playback slot: the radio's AudioEngine pulls each slot it drains from
    // here (PlaybackFeed). "local" has AudioEngine::kLocalPlaybackSlotId from
    // the start; a session's device gets its owner mix's slot from
    // DaemonMediaController, and MasterMixer::kNoPlaybackSlot when it goes.
    static constexpr const char* kLocalListener = "local";
    void setListenerSpeakerSlot(const ListenerId& who, int playbackSlotId);
    // PlaybackFeed, the DSP thread: the playback of whoever is bound to the
    // slot, while it is routed to the speakers.
    int pullSpeaker(int playbackSlotId, float* mono48k, int frames) noexcept override;

    // ---- On the air (spec 3, 6.1, 6.3, V5, V6): keying from a listener
    // while it plays sends its playback in place of its microphone, from
    // where it is, until the key ends or the recording does. Playing alone
    // never keys.
    OnAirSource* onAirSource() const { return m_onAir.get(); }
    // Whose playback a key may send: the station's own key is the window
    // that runs the radio ("local"), a remote device's key is its device id.
    // A program's key (TCI, CAT), TUNE, VOX and CW send none.
    static std::optional<ListenerId> listenerForKey(const KeyerIdentity& keyer, bool tune);
    // The radio keyed for `keyer` (on), or unkeyed. Followed from
    // MoxController (txAboutToBegin, txAboutToEnd, moxChanged).
    void keyChanged(bool on, const std::optional<ListenerId>& keyer);
    // OnAirSource's. beginOnAir takes who's playback off its speakers and
    // returns its channel, or -1 when who plays nothing; endOnAir gives it
    // back to them. pullOnAir (audio thread): as pullPlayback for that
    // channel, or -1 once there is no recording left to send.
    int beginOnAir(const ListenerId& who);
    void endOnAir(int channel);
    int pullOnAir(int channel, float* mono48k, int frames) noexcept;
    // The microphone profiles whose switch re-sends the transmit
    // processing (null without a radio).
    MicProfileManager* micProfiles() const;

    // ---- The voice keyer (spec 6.6): M1 to M8, sent through the on-air
    // source. The only part of recordings that keys the radio.
    VoiceKeyer* voiceKeyer() const { return m_voiceKeyer.get(); }

    // ---- The Digital Spectrum Recorder (spec 3, 6.7, section 7,
    // recordingsVersion 4): every receiver's I/Q in one ring file on the
    // chosen disk, while the radio streams. On by default on the Core's own
    // disk at 10 GB; never on an SD card.
    // Why the recorder is off although it is on (spec 6.7, section 7, A14).
    enum class DsrOff { None, SdOnly, SdChosen, DiskMissing, CannotWrite };
    // The words the document carries for it; "" for None.
    static QString dsrReasonWords(DsrOff why);
    // The ring file in RecordingSettings::dsrFolder().
    static constexpr const char* kDsrFileName = "Spectrum Recorder.ndsr";
    // DsrSizeLimitGb counts disk makers' gigabytes.
    static constexpr qint64 kBytesPerGb = 1000000000;
    // A chosen disk that goes away is noticed this often.
    static constexpr int kDsrDiskCheckMs = 5000;
    // The ring's figures (oldest, newest, write position) reach the
    // document at most this often; on, off and why change it at once.
    static constexpr int kDsrFiguresRefreshMs = 5000;
    // The tap: RadioModel::forkIqToTaps copies into it and returns.
    DsrRecorder* dsrRecorder() const { return m_dsr.get(); }
    DsrStatus dsrStatus() const;
    // The radio streams I/Q (connected), or not; the recorder runs only
    // while it does. Followed from RadioModel::connectionStateChanged.
    void setRadioStreaming(bool on);

    // ---- Rewind sound (spec 3, 6.7, recording plan Task 22): a slice
    // plays the past. Its frequency moves to the moment's frequency and its
    // demodulator is fed from the ring file (the one switch on the receive
    // path, RxDspWorker), in real time, from the next block. Other slices
    // and receivers keep playing the radio. The recorder goes on keeping
    // the live I/Q. Main thread.
    static constexpr const char* kRewindGoneReason = "That moment is no longer kept.";
    static constexpr const char* kRewindNoSliceReason = "That slice is not open.";
    static constexpr const char* kRewindNotRewoundReason = "That slice is not playing the past.";
    static constexpr const char* kRewindRateReason = "That moment was kept at another sample rate.";
    static constexpr const char* kRewindDiversityReason = "Turn diversity off to rewind that slice.";
    // Rewinds `slice` to `utcNs` (ns since 1970 UTC) at `frequencyHz`: the
    // receiver that kept that moment and frequency plays from there. A
    // slice already rewound keeps the frequency LIVE returns to.
    bool rewind(int slice, qint64 utcNs, qint64 frequencyHz, QString* refusal);
    // Back to the radio's I/Q, at the frequency the slice had before it was
    // rewound. A live slice: nothing to do (true).
    bool goLive(int slice, QString* refusal);
    // A rewound slice plays on from `utcNs`, at the same frequency.
    bool seekRewind(int slice, qint64 utcNs, QString* refusal);
    // The slices playing the past, by slice.
    QList<DsrRewoundSlice> rewound() const;
    // PTT while rewound (spec 3): every rewound slice goes live on the
    // frequency it plays, then the key goes on as usual; LIVE afterwards
    // still returns to the frequency before rewinding. Followed from
    // MoxController::txAboutToBegin; no prompt, no warning.
    void rewoundSlicesLiveForTransmit();
    // What rewind talks to; the radio's (RadioModel) unless a test sets one.
    void setDsrRewindHostForTest(std::unique_ptr<DsrRewindHost> host);
    std::shared_ptr<DsrReplay> replayForTest(int slice) const;

    // ---- The recordings worker thread, for work that must not run on the
    // main thread (the voice keyer's conversions).
    // Runs `job` on the worker after what is queued there already. From
    // the main thread or the worker.
    void runOnWorker(std::function<void()> job);
    // Returns once the worker has run what was queued before this call.
    // Main thread only; returns at once with no worker.
    void waitForWorker();

    // ---- Test seams.
    using Clock = std::function<qint64()>;
    using WriterFactory = std::function<std::unique_ptr<AudioFileWriter>(
        const QString& path, const AudioFileSpec& spec, QString* error)>;
    void setClockForTest(Clock clock);
    // Called on the worker thread; set before the first recording.
    void setWriterFactoryForTest(WriterFactory factory);
    void setMp3AvailableForTest(std::optional<bool> available);
    ContactRecorder* recorderForTest(int sliceId) const;
    // Runs the worker's drain and everything it hands back, now.
    void pumpForTest();
    // The quiet check, now.
    void checkQuietForTest() { checkQuiet(); }
    // The disks the recorder may use (RecordingDisks::volumes otherwise).
    using VolumesProvider = std::function<QList<RecordingVolume>()>;
    void setDsrVolumesForTest(VolumesProvider provider);
    // The recorder's disk check, now.
    void checkDsrDiskForTest() { checkDsrDisk(); }

signals:
    void sliceStateChanged(int sliceId);
    void kept(const NereusSDR::KeptEvent& e);
    // Started, stopped, sought, ended (once) or failed.
    void playbackChanged(const NereusSDR::ListenerId& who);
    void playbackRefused(const NereusSDR::ListenerId& who, const QString& reason);
    // The recorder went on or off, its reason changed, or its figures moved.
    void dsrChanged();

private:
    struct SliceRecord {
        RecordState state = RecordState::Idle;
        QDateTime startUtc;
        qint64 startMs = 0;
        QString startedBy;
        qint64 frequencyHz = 0;
        QString mode;
        quint64 token = 0;
        bool asWav = false;     // MP3 chosen, no encoder
        QuietRule quiet;
    };
    struct Finishing {
        int sliceId = -1;
        QDateTime startUtc;
        qint64 startMs = 0;
        qint64 endMs = 0;
        qint64 frequencyHz = 0;
        QString mode;
        KeptReason reason = KeptReason::StoppedHere;
        QString deviceName;
        bool asWav = false;
    };
    struct WorkerResult {
        quint64 token = 0;
        bool opened = false;
        bool failed = false;
        bool asWav = false;
        QString path;
        qint64 lengthMs = 0;
    };

    void ensureStoreOpen() const;
    void ensureWorker();
    qint64 now() const;
    bool mp3Available() const;
    ContactRecorder* recorderFor(int sliceId);
    void stopSlice(int sliceId, KeptReason reason, const QString& deviceName);
    void finishKept(quint64 token, const WorkerResult& result);
    void onWriteFailed(int sliceId, quint64 token);
    void onBeginFailed(int sliceId, quint64 token);
    void applyArming();
    void checkQuiet();
    void updateQuietTimer();
    void onSliceAdded(int sliceId);
    void onSliceRemoved(int sliceId);

    // The player.
    struct Listener {
        QString recordingId;
        QString path;
        std::shared_ptr<RecordingPlayer> player;   // null when stopped
        qint64 positionMs = 0;                     // when stopped
        qint64 lengthMs = 0;
        bool playing = false;
        quint64 token = 0;
        int channel = -1;
    };
    // What the audio side reads. `listener` is written on the main thread
    // only while unbound and no pull runs (waitForPulls).
    enum PlaybackRoute : int { kRouteSpeaker = 0, kRouteOnAir = 1 };
    struct PlaybackChannel {
        ListenerId listener;
        std::atomic<bool> bound{false};
        std::atomic<RecordingPlayer*> player{nullptr};
        // The mixer playback slot of the listener's speakers.
        std::atomic<int> speakerSlot{std::numeric_limits<int>::min()};
        // One reader per listener: the speakers (pullSpeaker) unless this
        // says on the air. Speaker when bound; Task 10's on-air source
        // sets kRouteOnAir while the listener transmits its playback and
        // reads pullPlayback itself, so the speakers stop pulling.
        std::atomic<int> route{kRouteSpeaker};
    };
    struct WorkerPlayer {
        ListenerId who;
        quint64 token = 0;
        std::shared_ptr<RecordingPlayer> player;
    };
    void startPlayer(const ListenerId& who, Listener& l, qint64 fromMs);
    void dropPlayer(Listener& l);
    void endListener(const ListenerId& who);
    int bindChannel(const ListenerId& who);
    void waitForPulls() const;
    void onPlaybackEnded(const ListenerId& who, quint64 token);
    void onPlaybackFailed(const ListenerId& who, quint64 token);

    // Worker thread.
    void workerBegin(int sliceId, quint64 token, const QString& path, const AudioFileSpec& spec);
    void workerDrainAll();
    void workerEnd(int sliceId, quint64 token);
    void workerPlayBegin(const ListenerId& who, quint64 token, std::shared_ptr<RecordingPlayer> player,
                         const QString& path);
    void workerPlayEnd(quint64 token);
    void workerFillPlayers();
    void workerStopTimerIfIdle();

    // The Digital Spectrum Recorder.
    DsrOff dsrDiskProblem(const QList<RecordingVolume>& volumes) const;
    QList<RecordingVolume> dsrVolumes() const;
    void applyDsr();
    void startDsr(const QString& path, qint64 limitBytes);
    void stopDsr();
    void checkDsrDisk();
    void updateDsrDiskTimer();
    void onDsrFigures(quint64 token, const DsrStatus& figures);
    void onDsrFailed(quint64 token);
    void workerDrainDsr();

    // Rewind sound.
    struct Rewound {
        qint64 liveFrequencyHz = 0;
        std::shared_ptr<DsrReplay> replay;   // switched in on the DSP worker
        quint64 replayToken = 0;
        std::shared_ptr<DsrReplay> pending;  // a rewind's or seek's, until it has read
        quint64 pendingToken = 0;
    };
    std::shared_ptr<DsrReplay> startReplay(int slice, int receiver, int rateHz, qint64 utcNs, quint64 token);
    void dropReplayOnWorker(const std::shared_ptr<DsrReplay>& replay);
    void onReplayReady(int slice, quint64 token);
    void onReplayFinished(int slice, quint64 token);
    void endRewind(int slice, bool restoreFrequency);
    void endAllRewinds(bool restoreFrequency);
    void followLiveCentres();
    void workerFillReplays();

    RadioModel* m_radio = nullptr;
    mutable std::unique_ptr<RecordingStore> m_store;
    mutable QString m_storeFolder;
    RecordingSettings m_settings;
    QHash<int, SliceRecord> m_records;
    QHash<quint64, Finishing> m_finishing;
    quint64 m_nextToken = 1;
    bool m_transmitting = false;
    Clock m_clock;
    WriterFactory m_writerFactory;
    std::optional<bool> m_mp3Override;

    std::array<std::unique_ptr<ContactRecorder>, kMaxRecorders> m_recorders;
    // What the transmit thread reads; set once per slot, cleared only when
    // the service goes.
    std::array<std::atomic<ContactRecorder*>, kMaxRecorders> m_monitorTargets{};
    std::atomic<bool> m_monitorClosed{false};
    std::atomic<int> m_monitorCallsInFlight{0};

    std::unique_ptr<QThread> m_thread;
    QObject* m_workerContext = nullptr;   // lives on m_thread; deleted there
    QTimer* m_workerTimer = nullptr;      // worker thread only
    QHash<int, WorkerResult> m_workerFiles;   // worker thread only: the files being written
    std::vector<WorkerPlayer> m_workerPlayers;   // worker thread only: the playbacks being decoded

    QHash<ListenerId, Listener> m_listeners;
    QHash<ListenerId, int> m_listenerSlots;     // who -> AudioEngine playback slot
    QPointer<AudioEngine> m_feedEngine;          // the engine that pulls from this
    std::array<PlaybackChannel, kMaxPlaybackListeners> m_playChannels;
    std::atomic<bool> m_playClosed{false};
    mutable std::atomic<int> m_pullsInFlight{0};
    quint64 m_nextPlayToken = 1;
    QTimer* m_quietTimer = nullptr;
    // Set while a caller that tells the playback's change itself ends what
    // is on the air, so endOnAir does not tell it first.
    bool m_quietOnAirEnd = false;
    // Last, so it goes first: its end() still reaches the channels above.
    std::unique_ptr<OnAirSource> m_onAir;
    // After the on-air source, so it goes before it: its end unkeys and
    // ends its message there.
    std::unique_ptr<VoiceKeyer> m_voiceKeyer;

    // The Digital Spectrum Recorder. The radio's tap gate (RadioModel)
    // closes before this goes, and the transmit monitor's above.
    std::unique_ptr<DsrRecorder> m_dsr;
    VolumesProvider m_dsrVolumes;
    bool m_radioStreaming = false;
    bool m_dsrRunning = false;
    QString m_dsrPath;
    qint64 m_dsrLimit = 0;
    quint64 m_dsrToken = 0;
    // Why it is off although on; worked out when first asked, then on each
    // change of settings, streaming or disk.
    mutable DsrOff m_dsrOff = DsrOff::None;
    mutable bool m_dsrReasonKnown = false;
    DsrStatus m_dsrFigures;
    qint64 m_dsrFiguresShownMs = -1;
    QTimer* m_dsrDiskTimer = nullptr;
    bool m_dsrWorkerOpen = false;   // worker thread only
    quint64 m_dsrWorkerToken = 0;   // worker thread only
    int m_dsrWorkerBlocks = 0;      // worker thread only
    Clock m_dsrWorkerClock;         // worker thread only

    // Rewind sound. Main thread, except the worker's list.
    std::unique_ptr<DsrRewindHost> m_rewindHost;
    QHash<int, Rewound> m_rewound;
    // After PTT took a rewound slice live: the frequency LIVE returns it to.
    QHash<int, qint64> m_liveAfterKey;
    quint64 m_nextRewindToken = 1;
    struct WorkerReplay {
        int slice = -1;
        quint64 token = 0;
        std::shared_ptr<DsrReplay> replay;
        bool readyTold = false;
        bool finishedTold = false;
    };
    std::vector<WorkerReplay> m_workerReplays;   // worker thread only
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::KeptEvent)
