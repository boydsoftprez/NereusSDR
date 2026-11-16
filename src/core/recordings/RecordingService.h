// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording service.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the Core's recording service: one
//             contact recorder per slice on a worker thread, start and stop
//             from any device, start by itself and stop by itself (spec 6.1,
//             6.2, V1, V4, section 7). AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/QuietRule.h"
#include "core/recordings/RecordingSettings.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>

class QThread;
class QTimer;

namespace NereusSDR {

class ContactRecorder;
class RadioModel;
class RecordingStore;

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
class NEREUS_CORE_EXPORT RecordingService : public QObject {
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

signals:
    void sliceStateChanged(int sliceId);
    void kept(const NereusSDR::KeptEvent& e);

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

    // Worker thread.
    void workerBegin(int sliceId, quint64 token, const QString& path, const AudioFileSpec& spec);
    void workerDrainAll();
    void workerEnd(int sliceId, quint64 token);

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
    QTimer* m_quietTimer = nullptr;
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::KeptEvent)
