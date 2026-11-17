// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording service.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the Core's recording service (spec 6.1,
//             6.2, V1, V4, section 7). AI tooling: Anthropic Claude Code.
// 2026-11-15  J.J. Boyd / KG4VCF; the player (spec 6.1 Player, 6.3, V6),
//             and its feed to the listener's own speakers. AI tooling:
//             Anthropic Claude Code.
// 2026-11-15  J.J. Boyd / KG4VCF; on the air (spec 3, 6.1, 6.3, V5, V6,
//             A6): the on-air source follows MOX, and a listener's playback
//             has one reader, its speakers or the transmitter. AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the voice keyer, and playFile for its
//             Listen (spec 6.6). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; runOnWorker, waitForWorker, and the voice
//             keyer told when the radio's connection drops. AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the Digital Spectrum Recorder: started
//             while the radio streams, written on the worker, kept off an SD
//             card and stopped when its disk goes (spec 3, 6.7, section 7,
//             A10, A14, A17). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; rewind sound: a slice plays the past from
//             the recorder (read ahead on the worker, switched in on the DSP
//             worker), LIVE, seek, and PTT while rewound (spec 3, 6.7,
//             recording plan Task 22). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; setHost: the Digital Spectrum Recorder is
//             off by default in a desktop window running its own radio and
//             on in a Core (recording plan Task 10b). AI tooling: Anthropic
//             Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the past picture: past rows, a past
//             spectrum frame and the hours map, checked here and drawn on
//             the worker (spec 3, 6.7, A12, recording plan Task 23). AI
//             tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the pan's width in pixels is checked with
//             its other settings (recording plan Task 24). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; eraseDsr, and the receivers the recorder
//             keeps (and their bytes an hour) in its status (spec 3, 6.7,
//             recording plan Task 26). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; final review fixes: a closed slice and a
//             lost radio end their rewinds; an erase fails a save running
//             from the recorder. AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; renameRecording and removeRecordings stop
//             the recording's playback and close an opened stretch before
//             the file is renamed or deleted (Windows will not rename or
//             remove an open file). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the past picture tops up the rewound
//             sound between transforms (workerPastRenderer), so rewound
//             playback no longer falls behind while rows are drawn. AI
//             tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; a past spectrum asked while rows are
//             drawn is drawn and answered between their transforms, the
//             newest per slice and view; the recorder drains between
//             them too. AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; a rewind to a moment kept at other
//             frequencies says so (kRewindNoFrequencyReason). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; rewound() carries what was kept at the
//             moment of the rewind or last seek (keptCentreHz, keptSpanHz),
//             so a tune outside it can say why there is no sound. AI
//             tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; Roundtable pauses for a slice playing
//             the past: rewoundLiveFrequencyHz holds the live frequency
//             from the rewind's own retune on, rewoundSlicesChanged says
//             when it changes. AI tooling: Anthropic Claude Code.

#include "RecordingService.h"

#include "core/AudioEngine.h"
#include "core/FFTEngine.h"
#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/audio/MasterMixer.h"
#include "core/NoiseFloorTracker.h"
#include "core/Resampler.h"
#include "core/SpotSourceHost.h"
#include "core/meters/SliceMeterPump.h"
#include "core/recordings/ContactRecorder.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingStore.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QThread>
#include <QTimeZone>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <thread>

namespace NereusSDR {

namespace {

// The squelch that runs for the slice's mode: FM squelch in FM, AM squelch
// in the AM family, the voice (syllabic) squelch otherwise.
bool squelchOnFor(const SliceModel& slice)
{
    switch (slice.dspMode()) {
    case DSPMode::FM:
        return slice.fmsqEnabled();
    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
        return slice.amsqEnabled();
    default:
        return slice.ssqlEnabled();
    }
}

constexpr qint64 kNsPerMs = 1000000;
constexpr qint64 kSecondsPerHour = 3600;

QString suffixFor(AudioFileFormat format)
{
    return format == AudioFileFormat::Mp3 ? QStringLiteral("mp3") : QStringLiteral("wav");
}

// Rewind sound's view of the radio (recording plan Task 22).
class RadioRewindHost final : public DsrRewindHost {
public:
    explicit RadioRewindHost(RadioModel* radio) : m_radio(radio) {}

    bool sliceOpen(int slice) const override { return m_radio->sliceById(slice) != nullptr; }
    qint64 sliceFrequencyHz(int slice) const override
    {
        const SliceModel* s = m_radio->sliceById(slice);
        return s != nullptr ? std::llround(s->frequency()) : 0;
    }
    void setSliceFrequencyHz(int slice, qint64 hz) override
    {
        if (SliceModel* s = m_radio->sliceById(slice)) {
            s->setFrequency(double(hz));
        }
    }
    int sliceReceiver(int slice) const override
    {
        const SliceModel* s = m_radio->sliceById(slice);
        return s != nullptr ? s->streamIndex() : -1;
    }
    qint64 receiverCentreHz(int receiver) const override
    {
        return receiver >= 0 ? std::llround(m_radio->streamCentreHz(receiver)) : 0;
    }
    int receiverRateHz(int receiver) const override
    {
        return receiver >= 0 ? m_radio->streamSampleRateHz(receiver) : 0;
    }
    // A slice on the diversity mix is fed past the switch
    // (RxDspWorker::feedExternalDiversityTarget), so while diversity runs no
    // slice is rewound.
    bool sliceOnDiversity(int /*slice*/) const override { return m_radio->diversityActive(); }
    bool feedSlice(int slice, std::shared_ptr<DsrReplay> replay) override
    {
        return m_radio->setSliceReplay(slice, std::move(replay));
    }
    // Task 25: how the slice is set, for its sound out of a stretch.
    bool sliceSound(int slice, DsrSoundSettings* out) const override
    {
        const SliceModel* s = m_radio->sliceById(slice);
        if (s == nullptr || out == nullptr) {
            return false;
        }
        out->frequencyHz = std::llround(s->frequency());
        out->dspMode = static_cast<int>(s->dspMode());
        out->filterLowHz = double(s->filterLow());
        out->filterHighHz = double(s->filterHigh());
        out->agcMode = static_cast<int>(s->agcMode());
        return true;
    }

private:
    RadioModel* m_radio;
};

}  // namespace

RecordingService::RecordingService(RadioModel* radio, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_store(std::make_unique<RecordingStore>())
    , m_settings(RecordingSettings::load(m_host))
    , m_clock([] { return QDateTime::currentMSecsSinceEpoch(); })
    , m_onAir(std::make_unique<OnAirSource>(*this))
{
    m_dsr = std::make_unique<DsrRecorder>();
    m_dsrVolumes = [] { return RecordingDisks::volumes(); };
    m_voiceKeyer = std::make_unique<VoiceKeyer>(*this, radio != nullptr ? radio->moxController() : nullptr,
                                                m_onAir.get());
    if (radio != nullptr) {
        // A dropped radio ends a message and a wait between repeats; a
        // reconnect keys nothing.
        VoiceKeyer* const voice = m_voiceKeyer.get();
        connect(radio, &RadioModel::connectionStateChanged, voice, [voice](ConnectionState state) {
            if (state != ConnectionState::Connected) {
                voice->connectionLost();
            }
        });
    }
    qRegisterMetaType<ListenerId>("NereusSDR::ListenerId");
    m_listenerSlots.insert(QString::fromLatin1(kLocalListener), AudioEngine::kLocalPlaybackSlotId);
    m_quietTimer = new QTimer(this);
    m_quietTimer->setInterval(kQuietCheckIntervalMs);
    connect(m_quietTimer, &QTimer::timeout, this, &RecordingService::checkQuiet);
    // Task 25: an opened stretch deleted from any device closes.
    connect(m_store.get(), &RecordingStore::changed, this, [this] {
        if (!m_stretch.id.isEmpty() && !m_store->find(m_stretch.id).has_value()) {
            closeStretch();
        }
    });

    if (m_radio) {
        RadioModel* radio = m_radio;
        m_rewindHost = std::make_unique<RadioRewindHost>(radio);
        m_onAir->setTransmitChain([radio] { return radio->txChannel(); }, &radio->transmitModel());
        if (MoxController* mox = m_radio->moxController()) {
            // PTT while rewound (spec 3): the rewound slices go live on the
            // frequency they play before the key goes on as usual.
            connect(mox, &MoxController::txAboutToBegin, this, [this] { rewoundSlicesLiveForTransmit(); });
            connect(mox, &MoxController::moxChanged, this, [this](int, bool, bool on) {
                setTransmitting(on);
                if (!on) {
                    keyChanged(false, std::nullopt);
                }
            });
            // Off the speakers from the start of the key, and off the air at
            // the start of the unkey. The transmitter takes its first block
            // of the recording once its channel runs (after the rf delay and
            // any amplifier's grant; TxWorkerThread), so the air starts
            // where the speakers stopped (V5). A key in CW walks without the
            // TX channel: the playback stays on its speakers and no switch
            // is touched.
            connect(mox, &MoxController::txAboutToBegin, this, [this, mox] {
                if (mox->isCwKeying()) {
                    return;
                }
                keyChanged(true, listenerForKey(mox->currentKeyer(), mox->isManualMox()));
            });
            connect(mox, &MoxController::txAboutToEnd, this, [this] { keyChanged(false, std::nullopt); });
        }
        // The Digital Spectrum Recorder runs while the radio streams, and
        // follows each receiver's rate and centre.
        connect(m_radio, &RadioModel::connectionStateChanged, this, [this](ConnectionState state) {
            setRadioStreaming(state == ConnectionState::Connected);
        });
        connect(m_radio, &RadioModel::streamCentreChanged, this, [this](int stream, double centreHz, int rateHz) {
            if (m_dsrRunning) {
                m_dsr->setStream(stream, std::llround(centreHz), rateHz);
            }
            // A rewound slice's past moves with its receiver's centre.
            followLiveCentres();
            // Task 26: Setup's hours follow the receivers and rates.
            noteDsrReceivers();
        });
        connect(m_radio, &RadioModel::sliceAdded, this, &RecordingService::onSliceAdded);
        connect(m_radio, &RadioModel::sliceRemoved, this, &RecordingService::onSliceRemoved);
        // The player's speakers: every drain pulls each playback slot here.
        if (AudioEngine* engine = m_radio->audioEngine()) {
            m_feedEngine = engine;
            engine->setPlaybackFeed(this);
        }
    }
    applyArming();
}

RecordingService::~RecordingService()
{
    // Rewind: the radio is going too, so its slices are left as they are.
    // A replay still switched in stays alive with the radio
    // (RadioModel::setSliceReplay) until its DSP worker is gone.
    m_rewound.clear();
    m_liveAfterKey.clear();
    m_rewindHost.reset();
    // A message going out stops and the radio unkeys first.
    m_voiceKeyer.reset();
    m_onAir->end();
    stopAll();

    // No drain may still pull from here, and nothing on the audio thread
    // may still be inside a player.
    if (m_feedEngine) {
        m_feedEngine->setPlaybackFeed(nullptr);
    }
    m_playClosed.store(true, std::memory_order_seq_cst);
    waitForPulls();
    for (PlaybackChannel& ch : m_playChannels) {
        ch.player.store(nullptr, std::memory_order_seq_cst);
        ch.bound.store(false, std::memory_order_seq_cst);
    }

    // Nothing on the transmit thread may still be inside a recorder.
    m_monitorClosed.store(true, std::memory_order_seq_cst);
    while (m_monitorCallsInFlight.load(std::memory_order_seq_cst) != 0) {
        std::this_thread::yield();
    }

    if (m_thread) {
        QMetaObject::invokeMethod(m_workerContext, [this] {
            delete m_workerTimer;
            m_workerTimer = nullptr;
            // Its FFT engine was made on the worker; it goes there too.
            m_pastRenderer.reset();
            m_spectrumRenderer.reset();
            m_workerSpectra.clear();
        }, Qt::BlockingQueuedConnection);
        connect(m_thread.get(), &QThread::finished, m_workerContext, &QObject::deleteLater);
        m_thread->quit();
        m_thread->wait();
        m_workerContext = nullptr;
        m_spectrumContext = nullptr;   // its child, deleted with it
    }
}

RecordingStore* RecordingService::store() const
{
    ensureStoreOpen();
    return m_store.get();
}

RecordingSettings RecordingService::settings() const
{
    return m_settings;
}

void RecordingService::setSettings(const RecordingSettings& s)
{
    const bool folderChanged = s.recordingsFolder() != m_settings.recordingsFolder();
    m_settings = s;
    m_settings.save();
    if (folderChanged) {
        m_storeFolder.clear();
    }
    for (const std::unique_ptr<ContactRecorder>& recorder : m_recorders) {
        if (recorder) {
            recorder->setKeepMyVoice(m_settings.keepMyVoice);
        }
    }
    for (SliceRecord& rec : m_records) {
        if (rec.state == RecordState::Recording) {
            rec.quiet.setMinutes(m_settings.quietMinutes);
        }
    }
    applyArming();
    applyDsr();
}

void RecordingService::setHost(RecordingSettings::Host host)
{
    if (host == m_host) {
        return;
    }
    m_host = host;
    // A saved value is kept; only a missing key takes the new default.
    m_settings.dsrEnabled = RecordingSettings::load(m_host).dsrEnabled;
    applyDsr();
}

void RecordingService::ensureStoreOpen() const
{
    const QString folder = m_settings.recordingsFolder();
    if (folder == m_storeFolder) {
        return;
    }
    QString error;
    if (m_store->openFolder(folder, &error)) {
        m_storeFolder = folder;
    } else {
        qCWarning(lcRecordings) << "the recordings folder cannot be used:" << error;
    }
}

void RecordingService::ensureWorker()
{
    if (m_thread) {
        return;
    }
    m_thread = std::make_unique<QThread>();
    m_thread->setObjectName(QStringLiteral("NereusRecordings"));
    m_workerContext = new QObject;
    m_spectrumContext = new QObject(m_workerContext);
    m_workerContext->moveToThread(m_thread.get());
    m_thread->start();
    QMetaObject::invokeMethod(m_workerContext, [this] {
        m_workerTimer = new QTimer(m_workerContext);
        m_workerTimer->setInterval(kDrainIntervalMs);
        connect(m_workerTimer, &QTimer::timeout, m_workerContext, [this] {
            if (m_workerRoundsHeldForTest.load()) {
                return;
            }
            workerDrainAll();
            workerFillPlayers();
            workerDrainDsr();
            workerFillReplays();
        });
    }, Qt::QueuedConnection);
}

void RecordingService::runOnWorker(std::function<void()> job)
{
    if (QThread::currentThread() != m_thread.get()) {
        ensureWorker();
    }
    QMetaObject::invokeMethod(m_workerContext, std::move(job), Qt::QueuedConnection);
}

void RecordingService::waitForWorker()
{
    if (m_thread && m_workerContext != nullptr) {
        QMetaObject::invokeMethod(m_workerContext, [] {}, Qt::BlockingQueuedConnection);
    }
}

qint64 RecordingService::now() const
{
    return m_clock();
}

bool RecordingService::mp3Available() const
{
    return m_mp3Override.has_value() ? *m_mp3Override : AudioFileWriter::mp3Available();
}

ContactRecorder* RecordingService::recorderFor(int sliceId)
{
    std::unique_ptr<ContactRecorder>& slot = m_recorders[static_cast<size_t>(sliceId)];
    if (!slot) {
        slot = std::make_unique<ContactRecorder>(sliceId);
        m_monitorTargets[static_cast<size_t>(sliceId)].store(slot.get(), std::memory_order_release);
    }
    return slot.get();
}

void RecordingService::startRecording(int sliceId, const QString& byDeviceName)
{
    if (!m_radio) {
        return;
    }
    SliceModel* slice = m_radio->sliceById(sliceId);
    if (!slice) {
        qCWarning(lcRecordings) << "no slice" << sliceId << "to record";
        return;
    }
    if (sliceId < 0 || sliceId >= kMaxRecorders) {
        qCWarning(lcRecordings) << "slice" << sliceId << "cannot record: only ids below" << kMaxRecorders;
        return;
    }
    SliceRecord& rec = m_records[sliceId];
    // V1: one recording at a time per slice.
    if (rec.state == RecordState::Recording) {
        return;
    }
    ensureStoreOpen();
    if (m_storeFolder.isEmpty()) {
        qCWarning(lcRecordings) << "slice" << sliceId << "cannot record: no recordings folder";
        return;
    }
    ensureWorker();

    // Section 7: no MP3 encoder means WAV, and the Kept notice says so.
    AudioFileFormat format = m_settings.format;
    rec.asWav = false;
    if (format == AudioFileFormat::Mp3 && !mp3Available()) {
        format = AudioFileFormat::Wav;
        rec.asWav = true;
    }
    const bool apart = m_settings.sidesApart;
    const AudioFileSpec spec{format, ContactRecorder::kFileRateHz, apart ? 2 : 1,
                             apart ? ContactRecorder::kStereoKbitPerSec
                                   : ContactRecorder::kMonoKbitPerSec};
    // The working name; the file takes its real name when it stops.
    const QString path = QDir(m_storeFolder).filePath(
        QStringLiteral(".recording-slice%1-%2.%3")
            .arg(sliceId)
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces), suffixFor(format)));

    rec.state = RecordState::Recording;
    rec.startMs = now();
    rec.startUtc = QDateTime::fromMSecsSinceEpoch(rec.startMs, QTimeZone::utc());
    rec.startedBy = byDeviceName;
    rec.frequencyHz = std::llround(slice->frequency());
    rec.mode = SliceModel::modeName(slice->dspMode());
    rec.token = m_nextToken++;
    rec.quiet.setMinutes(m_settings.quietMinutes);
    rec.quiet.start(rec.startMs);

    ContactRecorder* recorder = recorderFor(sliceId);
    recorder->setKeepMyVoice(m_settings.keepMyVoice);
    recorder->setTransmitting(m_transmitting);
    recorder->takeHeardSound();

    const quint64 token = rec.token;
    QMetaObject::invokeMethod(m_workerContext, [this, sliceId, token, path, spec] {
        workerBegin(sliceId, token, path, spec);
    }, Qt::QueuedConnection);

    recorder->setCapturing(true);
    if (AudioEngine* engine = m_radio->audioEngine()) {
        if (!engine->setSliceAudioTap(sliceId, recorder)) {
            qCWarning(lcRecordings) << "slice" << sliceId
                                    << "records without its receive audio: every audio tap is in use";
        }
    }
    emit sliceStateChanged(sliceId);
    updateQuietTimer();
}

void RecordingService::stopRecording(int sliceId, const QString& byDeviceName)
{
    stopSlice(sliceId, byDeviceName.isEmpty() ? KeptReason::StoppedHere : KeptReason::StoppedOnDevice,
              byDeviceName);
}

void RecordingService::stopSlice(int sliceId, KeptReason reason, const QString& deviceName)
{
    auto it = m_records.find(sliceId);
    if (it == m_records.end() || it->state != RecordState::Recording) {
        return;
    }
    SliceRecord& rec = it.value();
    ContactRecorder* recorder = m_recorders[static_cast<size_t>(sliceId)].get();
    if (recorder) {
        if (m_radio) {
            if (AudioEngine* engine = m_radio->audioEngine()) {
                engine->clearSliceAudioTap(recorder);
            }
        }
        recorder->setCapturing(false);
    }

    Finishing f;
    f.sliceId = sliceId;
    f.startUtc = rec.startUtc;
    f.startMs = rec.startMs;
    f.endMs = now();
    f.frequencyHz = rec.frequencyHz;
    f.mode = rec.mode;
    f.reason = reason;
    f.deviceName = deviceName;
    f.asWav = rec.asWav;
    const quint64 token = rec.token;
    m_finishing.insert(token, f);

    rec.state = m_settings.autoStartOnTransmit ? RecordState::Armed : RecordState::Idle;
    rec.startedBy.clear();
    rec.startUtc = QDateTime();

    QMetaObject::invokeMethod(m_workerContext, [this, sliceId, token] {
        workerEnd(sliceId, token);
    }, Qt::QueuedConnection);
    emit sliceStateChanged(sliceId);
    updateQuietTimer();
}

void RecordingService::finishKept(quint64 token, const WorkerResult& result)
{
    const auto it = m_finishing.find(token);
    if (it == m_finishing.end()) {
        return;
    }
    const Finishing f = it.value();
    m_finishing.erase(it);
    if (!result.opened) {
        return;
    }
    ensureStoreOpen();

    // The call: the station heard longest in the slice's Roundtable room.
    QString call;
    if (m_radio) {
        RoundtableModel* roundtable = m_radio->roundtableModel();
        if (roundtable && roundtable->linkState() != RoundtableModel::LinkState::Off) {
            const QString roomId = roundtable->sliceRoomId(f.sliceId);
            if (!roomId.isEmpty()) {
                call = callForRecording(roundtable->room(roomId), f.startMs, f.endMs,
                                        SpotSourceHost::roundtableCallsign());
            }
        }
    }

    QString finalPath = result.path;
    if (!m_storeFolder.isEmpty()) {
        const QString base = recordingBaseName(f.startUtc, f.frequencyHz, f.mode, call);
        const QString wanted = m_store->pathForNew(base, QFileInfo(result.path).suffix());
        if (QFile::rename(result.path, wanted)) {
            finalPath = wanted;
        } else if (QFile::copy(result.path, wanted)) {
            QFile::remove(result.path);
            finalPath = wanted;
        } else {
            qCWarning(lcRecordings) << "recording" << result.path << "kept under its working name:"
                                    << "it cannot be renamed to" << wanted;
        }
    }

    RecordingEntry entry;
    entry.kind = RecordingKind::Contact;
    entry.sliceId = f.sliceId;
    entry.startUtc = f.startUtc;
    entry.lengthMs = result.lengthMs;
    entry.frequencyHz = f.frequencyHz;
    entry.mode = f.mode;
    entry.call = call;
    entry.name = QFileInfo(finalPath).completeBaseName();
    entry.fileName = QFileInfo(finalPath).fileName();
    const QString id = m_store->add(entry);

    KeptReason reason = f.reason;
    if (result.failed) {
        reason = KeptReason::DiskFull;
    } else if (f.asWav || result.asWav) {
        reason = KeptReason::KeptAsWav;
    }
    emit kept(KeptEvent{f.sliceId, id, entry.name, result.lengthMs, reason, f.deviceName});
}

void RecordingService::onWriteFailed(int sliceId, quint64 token)
{
    const auto it = m_records.constFind(sliceId);
    if (it != m_records.cend() && it->state == RecordState::Recording && it->token == token) {
        stopSlice(sliceId, KeptReason::DiskFull, QString());
    }
}

void RecordingService::onBeginFailed(int sliceId, quint64 token)
{
    const auto it = m_records.constFind(sliceId);
    if (it != m_records.cend() && it->state == RecordState::Recording && it->token == token) {
        // Nothing was opened, so nothing is kept.
        stopSlice(sliceId, KeptReason::StoppedHere, QString());
    }
}

QList<RecordingSliceState> RecordingService::sliceStates() const
{
    QList<RecordingSliceState> states;
    if (!m_radio) {
        return states;
    }
    const qint64 t = now();
    for (const SliceModel* slice : m_radio->slices()) {
        if (!slice) {
            continue;
        }
        const int id = slice->sliceIndex();
        const SliceRecord rec = m_records.value(id);
        const bool recording = rec.state == RecordState::Recording;
        const ContactRecorder* recorder =
            id >= 0 && id < kMaxRecorders ? m_recorders[static_cast<size_t>(id)].get() : nullptr;
        states.append(RecordingSliceState{
            id, rec.state, recording ? rec.startUtc : QDateTime(),
            recording ? rec.startedBy : QString(), recording ? std::max<qint64>(0, t - rec.startMs) : 0,
            recording && recorder && recorder->hearingMyVoice()});
    }
    return states;
}

void RecordingService::stopAll()
{
    QList<int> recording;
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        if (it->state == RecordState::Recording) {
            recording.append(it.key());
        }
    }
    for (const int id : std::as_const(recording)) {
        stopSlice(id, KeptReason::StoppedHere, QString());
    }
    // The spectrum recorder writes what it holds and closes its file.
    const bool dsrWasRunning = m_dsrRunning;
    stopDsr();
    if (m_thread && (!m_finishing.isEmpty() || dsrWasRunning)) {
        // The files are finished on the worker; wait for them and keep them.
        QMetaObject::invokeMethod(m_workerContext, [] {}, Qt::BlockingQueuedConnection);
        QCoreApplication::sendPostedEvents(this);
    }
}

void RecordingService::consumeMonitor(const float* mono, int frames, int sampleRateHz) noexcept
{
    if (m_monitorClosed.load(std::memory_order_seq_cst)) {
        return;
    }
    m_monitorCallsInFlight.fetch_add(1, std::memory_order_seq_cst);
    if (!m_monitorClosed.load(std::memory_order_seq_cst)) {
        for (std::atomic<ContactRecorder*>& target : m_monitorTargets) {
            if (ContactRecorder* recorder = target.load(std::memory_order_acquire)) {
                recorder->consumeMonitor(mono, frames, sampleRateHz);
            }
        }
        // Your transmit beside the spectrum recorder's I/Q.
        m_dsr->consumeTransmitAudio(mono, frames, sampleRateHz);
    }
    m_monitorCallsInFlight.fetch_sub(1, std::memory_order_seq_cst);
}

void RecordingService::setTransmitting(bool on)
{
    if (on == m_transmitting) {
        return;
    }
    m_transmitting = on;
    m_dsr->setTransmitting(on);
    for (const std::unique_ptr<ContactRecorder>& recorder : m_recorders) {
        if (recorder) {
            recorder->setTransmitting(on);
        }
    }
    QList<int> changed;
    if (on) {
        const qint64 t = now();
        for (auto it = m_records.begin(); it != m_records.end(); ++it) {
            if (it->state == RecordState::Recording) {
                it->quiet.onTransmit(t);
            }
        }
        // Spec 6.2 step 4: keying the transmit slice starts its recording.
        if (m_settings.autoStartOnTransmit && m_radio) {
            for (const SliceModel* slice : m_radio->slices()) {
                if (slice && slice->isTxSlice()
                    && m_records.value(slice->sliceIndex()).state == RecordState::Armed) {
                    startRecording(slice->sliceIndex(), QString());
                }
            }
        }
    }
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        if (it->state == RecordState::Recording) {
            changed.append(it.key());
        }
    }
    for (const int id : std::as_const(changed)) {
        emit sliceStateChanged(id);
    }
}

void RecordingService::applyArming()
{
    if (!m_radio) {
        return;
    }
    for (const SliceModel* slice : m_radio->slices()) {
        if (!slice) {
            continue;
        }
        const int id = slice->sliceIndex();
        SliceRecord& rec = m_records[id];
        if (rec.state == RecordState::Idle && m_settings.autoStartOnTransmit) {
            rec.state = RecordState::Armed;
            emit sliceStateChanged(id);
        } else if (rec.state == RecordState::Armed && !m_settings.autoStartOnTransmit) {
            rec.state = RecordState::Idle;
            emit sliceStateChanged(id);
        }
    }
}

void RecordingService::checkQuiet()
{
    if (!m_radio) {
        return;
    }
    const qint64 t = now();
    QList<int> quiet;
    for (auto it = m_records.begin(); it != m_records.end(); ++it) {
        if (it->state != RecordState::Recording) {
            continue;
        }
        const int id = it.key();
        SliceRecord& rec = it.value();
        if (m_transmitting) {
            rec.quiet.onTransmit(t);
        }
        ContactRecorder* recorder = m_recorders[static_cast<size_t>(id)].get();
        const bool heard = recorder && recorder->takeHeardSound();
        const SliceModel* slice = m_radio->sliceById(id);
        if (slice && squelchOnFor(*slice)) {
            rec.quiet.onLevel(t, true, heard, 0.0, 0.0);
        } else if (slice) {
            // Squelch off: the S-meter against the noise floor, raised from
            // one bin to the slice's passband.
            const double level = slice->signalStrengthDbm();
            const NoiseFloorTracker* tracker = m_radio->noiseFloorTrackerForSlice(slice);
            const int stream = slice->streamIndex();
            const int rateHz = stream >= 0 ? m_radio->streamSampleRateHz(stream) : 0;
            const int widthHz = std::abs(slice->filterHigh() - slice->filterLow());
            if (level > SliceMeterPump::kNoReadingDbm && tracker && tracker->isGood() && rateHz > 0
                && widthHz > 0) {
                const double binHz = static_cast<double>(rateHz) / kFloorReferenceBins;
                const double floorDb = static_cast<double>(tracker->agcNoiseFloor())
                    + 10.0 * std::log10(static_cast<double>(widthHz) / binHz);
                rec.quiet.onLevel(t, false, false, level, floorDb);
            }
        }
        if (m_settings.autoStopWhenQuiet && rec.quiet.quiet(t)) {
            quiet.append(id);
        }
    }
    for (const int id : std::as_const(quiet)) {
        stopSlice(id, KeptReason::StoppedQuiet, QString());
    }
}

void RecordingService::updateQuietTimer()
{
    const bool any = std::any_of(m_records.cbegin(), m_records.cend(),
                                 [](const SliceRecord& r) { return r.state == RecordState::Recording; });
    if (any && !m_quietTimer->isActive()) {
        m_quietTimer->start();
    } else if (!any && m_quietTimer->isActive()) {
        m_quietTimer->stop();
    }
}

void RecordingService::onSliceAdded(int sliceId)
{
    Q_UNUSED(sliceId);
    applyArming();
}

void RecordingService::onSliceRemoved(int sliceId)
{
    stopSlice(sliceId, KeptReason::StoppedHere, QString());
    m_records.remove(sliceId);
    // A closed slice's replay is switched out: the next slice on that id
    // hears the radio. Slice A closing closes the stretch it plays.
    endRewind(sliceId, false);
    if (sliceId == kStretchSlice) {
        closeStretch();
    }
}

// ---- Worker thread.

void RecordingService::workerBegin(int sliceId, quint64 token, const QString& path, const AudioFileSpec& spec)
{
    ContactRecorder* recorder = m_recorders[static_cast<size_t>(sliceId)].get();
    const auto open = [this](const QString& p, const AudioFileSpec& s, QString* error) {
        return m_writerFactory ? m_writerFactory(p, s, error) : AudioFileWriter::open(p, s, error);
    };
    WorkerResult file;
    file.token = token;
    file.path = path;
    QString error;
    std::unique_ptr<AudioFileWriter> writer = open(path, spec, &error);
    if (!writer && spec.format == AudioFileFormat::Mp3) {
        // Section 7: the MP3 encoder failed, so the recording is WAV.
        qCWarning(lcRecordings) << "slice" << sliceId << "records as WAV: MP3 cannot start:" << error;
        AudioFileSpec wav = spec;
        wav.format = AudioFileFormat::Wav;
        const QFileInfo info(path);
        file.path = info.dir().filePath(info.completeBaseName() + QStringLiteral(".wav"));
        file.asWav = true;
        writer = open(file.path, wav, &error);
    }
    if (!writer || !recorder) {
        qCWarning(lcRecordings) << "slice" << sliceId << "cannot record:" << error;
        QMetaObject::invokeMethod(this, [this, sliceId, token] { onBeginFailed(sliceId, token); },
                                  Qt::QueuedConnection);
        return;
    }
    recorder->begin(std::move(writer), spec.channels == 2);
    file.opened = true;
    m_workerFiles.insert(sliceId, file);
    if (m_workerTimer && !m_workerTimer->isActive()) {
        m_workerTimer->start();
    }
}

void RecordingService::workerDrainAll()
{
    for (auto it = m_workerFiles.cbegin(); it != m_workerFiles.cend(); ++it) {
        ContactRecorder* recorder = m_recorders[static_cast<size_t>(it.key())].get();
        if (recorder && recorder->isOpen() && !recorder->drain()) {
            const int sliceId = it.key();
            const quint64 token = it->token;
            QMetaObject::invokeMethod(this, [this, sliceId, token] { onWriteFailed(sliceId, token); },
                                      Qt::QueuedConnection);
        }
    }
}

void RecordingService::workerEnd(int sliceId, quint64 token)
{
    WorkerResult result;
    const auto it = m_workerFiles.find(sliceId);
    if (it != m_workerFiles.end() && it->token == token) {
        result = it.value();
        m_workerFiles.erase(it);
        ContactRecorder* recorder = m_recorders[static_cast<size_t>(sliceId)].get();
        if (recorder) {
            recorder->finish();
            result.failed = recorder->failed();
            result.lengthMs = recorder->lengthMs();
        }
    }
    workerStopTimerIfIdle();
    QMetaObject::invokeMethod(this, [this, token, result] { finishKept(token, result); },
                              Qt::QueuedConnection);
}

// ---- The player.

void RecordingService::play(const ListenerId& who, const QString& recordingId)
{
    ensureStoreOpen();
    const std::optional<RecordingEntry> entry = m_store->find(recordingId);
    if (!entry) {
        emit playbackRefused(who, QString::fromLatin1(kPlayGoneReason));
        return;
    }
    const QString path = QDir(m_store->folder()).filePath(entry->fileName);
    // Spec 6.3 (Task 25): a whole-pan recording opens on the waterfall
    // (openStretch); played here it is its sound only ("Play its sound
    // only"), slice A's as it was set when the stretch was saved.
    if (entry->kind == RecordingKind::WholePan) {
        DsrSoundSettings sound;
        sound.frequencyHz = entry->frequencyHz;
        sound.dspMode = static_cast<int>(SliceModel::modeFromName(entry->mode));
        sound.filterLowHz = entry->filterLowHz;
        sound.filterHighHz = entry->filterHighHz;
        if (!DsrSoundRender::available()) {
            emit playbackRefused(who, QString::fromLatin1(kPlayWholePanReason));
            return;
        }
        playPath(who, recordingId, path, entry->lengthMs, sound);
        return;
    }
    playPath(who, recordingId, path, entry->lengthMs, std::nullopt);
}

void RecordingService::playFile(const ListenerId& who, const QString& recordingId, const QString& path,
                                qint64 lengthMs)
{
    playPath(who, recordingId, path, lengthMs, std::nullopt);
}

void RecordingService::playPath(const ListenerId& who, const QString& recordingId, const QString& path,
                                qint64 lengthMs, const std::optional<DsrSoundSettings>& stretchSound)
{
    auto it = m_listeners.find(who);
    if (it != m_listeners.end() && it->recordingId == recordingId && it->playing) {
        return;
    }
    if (it == m_listeners.end()) {
        const int channel = bindChannel(who);
        if (channel < 0) {
            qCWarning(lcRecordings) << "no playback channel free for" << who;
            emit playbackRefused(who, QString::fromLatin1(kPlayBusyReason));
            return;
        }
        it = m_listeners.insert(who, Listener{});
        it->channel = channel;
    }
    Listener& l = it.value();
    qint64 from = 0;
    if (l.recordingId == recordingId && !l.playing && l.positionMs < l.lengthMs) {
        from = l.positionMs;
    }
    l.recordingId = recordingId;
    l.path = path;
    l.lengthMs = lengthMs;
    l.stretchSound = stretchSound;
    startPlayer(who, l, from);
    emit playbackChanged(who);
}

bool RecordingService::renameRecording(const QString& id, const QString& newName, QString* error)
{
    ensureStoreOpen();
    // Only a rename that can go ahead stops anything: a refused name, an
    // unknown recording or the same name leave the playback alone.
    const std::optional<RecordingEntry> entry = m_store->find(id);
    if (entry.has_value() && entry->name != newName && isValidRecordingName(newName, nullptr)) {
        releaseRecordingFiles({id});
    }
    return m_store->rename(id, newName, error);
}

bool RecordingService::removeRecordings(const QStringList& ids, QString* error)
{
    ensureStoreOpen();
    releaseRecordingFiles(ids);
    return m_store->remove(ids, error);
}

void RecordingService::releaseRecordingFiles(const QStringList& ids)
{
    bool released = false;
    const QList<ListenerId> listeners = m_listeners.keys();
    for (const ListenerId& who : listeners) {
        const auto it = m_listeners.constFind(who);
        if (it != m_listeners.cend() && ids.contains(it->recordingId)) {
            endListener(who);
            emit playbackChanged(who);
            released = true;
        }
    }
    if (!m_stretch.id.isEmpty() && ids.contains(m_stretch.id)) {
        closeStretch();
        released = true;
    }
    // The worker holds its own share of a player or a stretch until it
    // reads the drops queued above; on Windows the file stays open until
    // then.
    if (released) {
        waitForWorker();
    }
}

void RecordingService::stopPlayback(const ListenerId& who)
{
    if (!m_listeners.contains(who)) {
        return;
    }
    endListener(who);
    emit playbackChanged(who);
}

void RecordingService::seek(const ListenerId& who, qint64 positionMs)
{
    const auto it = m_listeners.find(who);
    if (it == m_listeners.end()) {
        return;
    }
    Listener& l = it.value();
    const qint64 length = l.player ? l.player->lengthMs() : l.lengthMs;
    const qint64 to = std::clamp<qint64>(positionMs, 0, std::max<qint64>(0, length));
    if (l.playing) {
        startPlayer(who, l, to);
    } else {
        l.positionMs = to;
    }
    emit playbackChanged(who);
}

std::optional<PlaybackState> RecordingService::playback(const ListenerId& who) const
{
    const auto it = m_listeners.constFind(who);
    if (it == m_listeners.cend()) {
        return std::nullopt;
    }
    const Listener& l = it.value();
    const bool onAir = l.channel >= 0
        && m_playChannels[static_cast<size_t>(l.channel)].route.load(std::memory_order_seq_cst) == kRouteOnAir;
    return PlaybackState{who,
                         l.recordingId,
                         l.player ? l.player->positionMs() : l.positionMs,
                         l.player ? l.player->lengthMs() : l.lengthMs,
                         l.playing,
                         onAir};
}

int RecordingService::pullPlayback(const ListenerId& who, float* mono48k, int frames) noexcept
{
    int n = 0;
    bool pulled = false;
    if (!m_playClosed.load(std::memory_order_seq_cst)) {
        m_pullsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (!m_playClosed.load(std::memory_order_seq_cst)) {
            for (PlaybackChannel& ch : m_playChannels) {
                if (ch.bound.load(std::memory_order_seq_cst) && ch.listener == who) {
                    if (RecordingPlayer* player = ch.player.load(std::memory_order_seq_cst)) {
                        n = player->pull(mono48k, frames);
                        pulled = true;
                    }
                    break;
                }
            }
        }
        m_pullsInFlight.fetch_sub(1, std::memory_order_seq_cst);
    }
    if (!pulled && mono48k != nullptr && frames > 0) {
        std::fill(mono48k, mono48k + frames, 0.0f);
    }
    return n;
}

int RecordingService::pullSpeaker(int playbackSlotId, float* mono48k, int frames) noexcept
{
    int n = 0;
    bool pulled = false;
    if (!m_playClosed.load(std::memory_order_seq_cst)) {
        m_pullsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (!m_playClosed.load(std::memory_order_seq_cst)) {
            for (PlaybackChannel& ch : m_playChannels) {
                if (ch.bound.load(std::memory_order_seq_cst)
                    && ch.speakerSlot.load(std::memory_order_seq_cst) == playbackSlotId) {
                    RecordingPlayer* player = ch.player.load(std::memory_order_seq_cst);
                    if (player && ch.route.load(std::memory_order_seq_cst) == kRouteSpeaker) {
                        n = player->pull(mono48k, frames);
                        pulled = true;
                    }
                    break;
                }
            }
        }
        m_pullsInFlight.fetch_sub(1, std::memory_order_seq_cst);
    }
    if (!pulled && mono48k != nullptr && frames > 0) {
        std::fill(mono48k, mono48k + frames, 0.0f);
    }
    return n;
}

void RecordingService::setListenerSpeakerSlot(const ListenerId& who, int playbackSlotId)
{
    if (playbackSlotId == MasterMixer::kNoPlaybackSlot) {
        m_listenerSlots.remove(who);
    } else {
        // One listener per slot: whoever had it before no longer does.
        for (auto it = m_listenerSlots.begin(); it != m_listenerSlots.end();) {
            if (it.value() == playbackSlotId && it.key() != who) {
                const auto l = m_listeners.constFind(it.key());
                if (l != m_listeners.cend()) {
                    m_playChannels[static_cast<size_t>(l->channel)].speakerSlot.store(
                        MasterMixer::kNoPlaybackSlot, std::memory_order_seq_cst);
                }
                it = m_listenerSlots.erase(it);
            } else {
                ++it;
            }
        }
        m_listenerSlots.insert(who, playbackSlotId);
    }
    const auto l = m_listeners.constFind(who);
    if (l != m_listeners.cend()) {
        m_playChannels[static_cast<size_t>(l->channel)].speakerSlot.store(
            m_listenerSlots.value(who, MasterMixer::kNoPlaybackSlot), std::memory_order_seq_cst);
    }
}

// ---- On the air.

std::optional<ListenerId> RecordingService::listenerForKey(const KeyerIdentity& keyer, bool tune)
{
    // VOX is left out too: the playback on the speakers could trip it and
    // then hold the key for the whole recording (playback alone never keys).
    if (tune || keyer.program || keyer.tunerPress || keyer.source == PttMode::Manual
        || keyer.source == PttMode::Cw || keyer.source == PttMode::Vox) {
        return std::nullopt;
    }
    if (keyer.isStation()) {
        return QString::fromLatin1(kLocalListener);
    }
    if (keyer.deviceId.isEmpty()) {
        return std::nullopt;
    }
    return QString::fromLatin1(keyer.deviceId);
}

void RecordingService::keyChanged(bool on, const std::optional<ListenerId>& keyer)
{
    if (!on) {
        m_onAir->end();
        return;
    }
    // V6: only the keying device's own playback goes out.
    if (keyer.has_value() && m_onAir->active() == OnAirSource::Kind::None) {
        m_onAir->beginPlayback(*keyer);
    }
}

int RecordingService::beginOnAir(const ListenerId& who)
{
    const auto it = m_listeners.constFind(who);
    if (it == m_listeners.cend() || !it->playing || !it->player || it->channel < 0) {
        return -1;
    }
    PlaybackChannel& ch = m_playChannels[static_cast<size_t>(it->channel)];
    ch.route.store(kRouteOnAir, std::memory_order_seq_cst);
    // A speaker pull that started before the flag may still be reading:
    // once it is out, the transmitter is the only reader.
    waitForPulls();
    emit playbackChanged(who);
    return it->channel;
}

void RecordingService::endOnAir(int channel)
{
    if (channel < 0 || channel >= kMaxPlaybackListeners) {
        return;
    }
    PlaybackChannel& ch = m_playChannels[static_cast<size_t>(channel)];
    ch.route.store(kRouteSpeaker, std::memory_order_seq_cst);
    // The callers that change the playback as well (it ended, or it was
    // stopped) tell it once, after.
    if (ch.bound.load(std::memory_order_seq_cst) && !m_quietOnAirEnd) {
        emit playbackChanged(ch.listener);
    }
}

int RecordingService::pullOnAir(int channel, float* mono48k, int frames) noexcept
{
    int n = -1;
    if (channel >= 0 && channel < kMaxPlaybackListeners && !m_playClosed.load(std::memory_order_seq_cst)) {
        m_pullsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (!m_playClosed.load(std::memory_order_seq_cst)) {
            PlaybackChannel& ch = m_playChannels[static_cast<size_t>(channel)];
            if (ch.bound.load(std::memory_order_seq_cst)
                && ch.route.load(std::memory_order_seq_cst) == kRouteOnAir) {
                RecordingPlayer* player = ch.player.load(std::memory_order_seq_cst);
                // A recording taken to its end has nothing left to send: the
                // microphone takes over from this block (V5).
                if (player == nullptr) {
                    // Between two players (a seek while on the air): silence,
                    // never a block of microphone.
                    if (mono48k != nullptr && frames > 0) {
                        std::fill(mono48k, mono48k + frames, 0.0f);
                    }
                    n = 0;
                } else if (!player->finished()) {
                    n = player->pull(mono48k, frames);
                }
            }
        }
        m_pullsInFlight.fetch_sub(1, std::memory_order_seq_cst);
    }
    return n;
}

MicProfileManager* RecordingService::micProfiles() const
{
    return m_radio ? m_radio->micProfileManager() : nullptr;
}

void RecordingService::startPlayer(const ListenerId& who, Listener& l, qint64 fromMs)
{
    dropPlayer(l);
    auto player = std::make_shared<RecordingPlayer>(fromMs, l.lengthMs);
    if (l.stretchSound.has_value()) {
        player->setStretchSound(*l.stretchSound);
    }
    l.player = player;
    l.playing = true;
    l.positionMs = fromMs;
    l.token = m_nextPlayToken++;
    m_playChannels[static_cast<size_t>(l.channel)].player.store(player.get(), std::memory_order_seq_cst);
    ensureWorker();
    const quint64 token = l.token;
    const QString path = l.path;
    QMetaObject::invokeMethod(m_workerContext, [this, who, token, player, path] {
        workerPlayBegin(who, token, player, path);
    }, Qt::QueuedConnection);
}

void RecordingService::dropPlayer(Listener& l)
{
    if (!l.player) {
        return;
    }
    l.positionMs = l.player->positionMs();
    l.lengthMs = l.player->lengthMs();
    m_playChannels[static_cast<size_t>(l.channel)].player.store(nullptr, std::memory_order_seq_cst);
    // No audio thread may still be inside it; the worker lets go of its own
    // share when it reads the end below.
    waitForPulls();
    const quint64 token = l.token;
    if (m_workerContext) {
        QMetaObject::invokeMethod(m_workerContext, [this, token] { workerPlayEnd(token); },
                                  Qt::QueuedConnection);
    }
    l.player.reset();
}

void RecordingService::endListener(const ListenerId& who)
{
    const auto it = m_listeners.find(who);
    if (it == m_listeners.end()) {
        return;
    }
    // Its recording stops going out first; the radio stays keyed. The
    // caller tells the playback's change once.
    if (m_onAir->playbackListener() == who) {
        m_quietOnAirEnd = true;
        m_onAir->end();
        m_quietOnAirEnd = false;
    }
    dropPlayer(it.value());
    PlaybackChannel& ch = m_playChannels[static_cast<size_t>(it->channel)];
    ch.player.store(nullptr, std::memory_order_seq_cst);
    ch.bound.store(false, std::memory_order_seq_cst);
    waitForPulls();
    m_listeners.erase(it);
}

int RecordingService::bindChannel(const ListenerId& who)
{
    for (int k = 0; k < kMaxPlaybackListeners; ++k) {
        PlaybackChannel& ch = m_playChannels[static_cast<size_t>(k)];
        if (!ch.bound.load(std::memory_order_seq_cst)) {
            // Unbound, and endListener waited out every pull that saw it
            // bound, so no audio thread reads the name now.
            ch.listener = who;
            ch.player.store(nullptr, std::memory_order_seq_cst);
            ch.speakerSlot.store(m_listenerSlots.value(who, MasterMixer::kNoPlaybackSlot),
                                 std::memory_order_seq_cst);
            ch.route.store(kRouteSpeaker, std::memory_order_seq_cst);
            ch.bound.store(true, std::memory_order_seq_cst);
            return k;
        }
    }
    return -1;
}

void RecordingService::waitForPulls() const
{
    while (m_pullsInFlight.load(std::memory_order_seq_cst) != 0) {
        std::this_thread::yield();
    }
}

void RecordingService::onPlaybackEnded(const ListenerId& who, quint64 token)
{
    const auto it = m_listeners.find(who);
    if (it == m_listeners.end() || it->token != token || !it->player) {
        return;
    }
    Listener& l = it.value();
    // V5: it ended on the air; the microphone already has the key. One
    // change is told, below: not playing and not on the air.
    if (m_onAir->playbackListener() == who) {
        m_quietOnAirEnd = true;
        m_onAir->end();
        m_quietOnAirEnd = false;
    }
    dropPlayer(l);
    l.positionMs = l.lengthMs;
    l.playing = false;
    emit playbackChanged(who);
}

void RecordingService::onPlaybackFailed(const ListenerId& who, quint64 token)
{
    const auto it = m_listeners.find(who);
    if (it == m_listeners.end() || it->token != token || !it->player) {
        return;
    }
    endListener(who);
    emit playbackRefused(who, QString::fromLatin1(kPlayFailedReason));
    emit playbackChanged(who);
}

// ---- Worker thread: the playbacks.

void RecordingService::workerPlayBegin(const ListenerId& who, quint64 token,
                                       std::shared_ptr<RecordingPlayer> player, const QString& path)
{
    QString error;
    if (!player->open(path, &error)) {
        qCWarning(lcRecordings) << "recording" << path << "cannot be played:" << error;
        QMetaObject::invokeMethod(this, [this, who, token] { onPlaybackFailed(who, token); },
                                  Qt::QueuedConnection);
        return;
    }
    player->fill();
    m_workerPlayers.push_back(WorkerPlayer{who, token, std::move(player)});
    if (m_workerTimer && !m_workerTimer->isActive()) {
        m_workerTimer->start();
    }
}

void RecordingService::workerPlayEnd(quint64 token)
{
    m_workerPlayers.erase(std::remove_if(m_workerPlayers.begin(), m_workerPlayers.end(),
                                         [token](const WorkerPlayer& p) { return p.token == token; }),
                          m_workerPlayers.end());
    workerStopTimerIfIdle();
}

void RecordingService::workerFillPlayers()
{
    for (auto it = m_workerPlayers.begin(); it != m_workerPlayers.end();) {
        it->player->fill();
        if (it->player->finished()) {
            // Reported once: the player leaves the worker here.
            const ListenerId who = it->who;
            const quint64 token = it->token;
            QMetaObject::invokeMethod(this, [this, who, token] { onPlaybackEnded(who, token); },
                                      Qt::QueuedConnection);
            it = m_workerPlayers.erase(it);
        } else {
            ++it;
        }
    }
    workerStopTimerIfIdle();
}

void RecordingService::workerStopTimerIfIdle()
{
    if (m_workerFiles.isEmpty() && m_workerPlayers.empty() && !m_dsrWorkerOpen && m_workerReplays.empty()
        && m_workerTimer) {
        m_workerTimer->stop();
    }
}

// ---- Test seams.

void RecordingService::setClockForTest(Clock clock)
{
    if (clock) {
        m_clock = std::move(clock);
    }
}

void RecordingService::setWriterFactoryForTest(WriterFactory factory)
{
    m_writerFactory = std::move(factory);
}

void RecordingService::setDsrVolumesForTest(VolumesProvider provider)
{
    if (provider) {
        m_dsrVolumes = std::move(provider);
    }
    m_dsrReasonKnown = false;
}

void RecordingService::setMp3AvailableForTest(std::optional<bool> available)
{
    m_mp3Override = available;
}

ContactRecorder* RecordingService::recorderForTest(int sliceId) const
{
    return sliceId >= 0 && sliceId < kMaxRecorders ? m_recorders[static_cast<size_t>(sliceId)].get()
                                                   : nullptr;
}

void RecordingService::pumpForTest()
{
    if (!m_thread) {
        return;
    }
    // More than once: a drain that hits a full disk asks the main thread to
    // stop, and that stop's finish runs on the worker again.
    for (int round = 0; round < 3; ++round) {
        QMetaObject::invokeMethod(m_workerContext, [this] {
            workerDrainAll();
            workerFillPlayers();
            workerDrainDsr();
            workerFillReplays();
        }, Qt::BlockingQueuedConnection);
        QCoreApplication::sendPostedEvents(this);
    }
}

// ---- The Digital Spectrum Recorder.

QList<RecordingVolume> RecordingService::dsrVolumes() const
{
    return m_dsrVolumes ? m_dsrVolumes() : RecordingDisks::volumes();
}

QString RecordingService::dsrReasonWords(DsrOff why)
{
    switch (why) {
    case DsrOff::None:
        return QString();
    case DsrOff::SdOnly:
        return QStringLiteral("The only disk is an SD card.");
    case DsrOff::SdChosen:
        return QStringLiteral("The recorder's disk is an SD card. Choose a USB SSD or NVMe drive.");
    case DsrOff::DiskMissing:
        return QStringLiteral("The recorder's disk is missing.");
    case DsrOff::CannotWrite:
        return QStringLiteral("The recorder cannot write to its disk.");
    }
    return QString();
}

RecordingService::DsrOff RecordingService::dsrDiskProblem(const QList<RecordingVolume>& volumes) const
{
    const QString disk = QDir::fromNativeSeparators(m_settings.dsrDisk);
    const auto chosen = std::find_if(volumes.cbegin(), volumes.cend(), [&disk](const RecordingVolume& v) {
        return disk.isEmpty() ? v.isCoreOwnDisk : QDir::fromNativeSeparators(v.rootPath) == disk;
    });
    if (chosen == volumes.cend()) {
        // The Core's own disk is always there.
        return disk.isEmpty() ? DsrOff::None : DsrOff::DiskMissing;
    }
    if (!disk.isEmpty() && !QDir(chosen->rootPath).exists()) {
        return DsrOff::DiskMissing;
    }
    if (chosen->isSdCard) {
        // A14: the loop never wears an SD card.
        const bool onlySd = std::all_of(volumes.cbegin(), volumes.cend(),
                                        [](const RecordingVolume& v) { return v.isSdCard; });
        return onlySd ? DsrOff::SdOnly : DsrOff::SdChosen;
    }
    return DsrOff::None;
}

DsrStatus RecordingService::dsrStatus() const
{
    if (!m_dsrReasonKnown) {
        m_dsrOff = m_settings.dsrEnabled ? dsrDiskProblem(dsrVolumes()) : DsrOff::None;
        m_dsrReasonKnown = true;
    }
    DsrStatus st = m_dsrFigures;
    st.enabled = m_settings.dsrEnabled;
    st.recording = m_dsrRunning;
    st.disk = m_settings.dsrDisk;
    st.sizeLimitGb = m_settings.dsrSizeLimitGb;
    st.reason = m_settings.dsrEnabled ? dsrReasonWords(m_dsrOff) : QString();
    st.overflows = m_dsr->overflows();
    st.rewound = rewound();
    st.stretchId = m_stretch.id;
    st.stretchFromUtcNs = m_stretch.fromUtcNs;
    st.stretchToUtcNs = m_stretch.toUtcNs;
    st.saving = int(m_saves.size());
    // Task 26: one second of each receiver is one block on disk.
    st.receivers = dsrReceivers();
    for (const DsrReceiverRate& r : std::as_const(st.receivers)) {
        st.bytesPerHour += DsrRingFile::blockBytes(r.rateHz) * kSecondsPerHour;
    }
    if (st.sizeLimitBytes == 0) {
        st.sizeLimitBytes = qint64(std::max(m_settings.dsrSizeLimitGb, RecordingSettings::kDsrSizeLimitGbMin))
                            * kBytesPerGb;
    }
    return st;
}

QList<DsrReceiverRate> RecordingService::dsrReceivers() const
{
    QList<DsrReceiverRate> out;
    if (m_radio == nullptr) {
        return out;
    }
    for (const SliceModel* slice : m_radio->slices()) {
        const int stream = slice != nullptr ? slice->streamIndex() : -1;
        if (stream < 0 || stream >= DsrRecorder::kMaxReceivers) {
            continue;
        }
        const int rateHz = m_radio->streamSampleRateHz(stream);
        const bool known = std::any_of(out.cbegin(), out.cend(),
                                       [stream](const DsrReceiverRate& r) { return r.receiver == stream; });
        if (rateHz > 0 && !known) {
            out.append(DsrReceiverRate{stream, rateHz});
        }
    }
    std::sort(out.begin(), out.end(),
              [](const DsrReceiverRate& a, const DsrReceiverRate& b) { return a.receiver < b.receiver; });
    return out;
}

void RecordingService::noteDsrReceivers()
{
    QList<int> now;
    for (const DsrReceiverRate& r : dsrReceivers()) {
        now << r.receiver << r.rateHz;
    }
    if (now == m_dsrReceiversShown) {
        return;
    }
    m_dsrReceiversShown = now;
    emit dsrChanged();
}

void RecordingService::eraseDsr()
{
    // What is erased cannot be played: slices rewound into the recorder go
    // back to the radio. One playing a saved stretch reads its own file.
    const QList<int> rewoundSlices = m_rewound.keys();
    for (const int slice : rewoundSlices) {
        if (!m_rewound.value(slice).source) {
            endRewind(slice, true);
        }
    }
    // A save running from the recorder cannot finish whole: it fails when
    // it ends. One from a saved stretch reads its own file.
    for (PendingSave& save : m_saves) {
        if (save.fromRecorder) {
            save.erased = true;
        }
    }
    const QString path = QDir(m_settings.dsrFolder()).filePath(QString::fromLatin1(kDsrFileName));
    const QString runningPath = m_dsrPath;
    const qint64 limit = m_dsrLimit;
    const bool wasRunning = m_dsrRunning;
    if (wasRunning) {
        m_dsrRunning = false;
        m_dsr->setAccepting(false);
    }
    // Figures from before the erase are dropped when they arrive.
    ++m_dsrToken;
    ensureWorker();
    QMetaObject::invokeMethod(m_workerContext, [this, path, runningPath] {
        if (m_dsrWorkerOpen) {
            // What the rings still hold goes with the rest.
            m_dsrWorkerOpen = false;
            m_dsr->abandon();
        }
        QStringList paths{path};
        if (!runningPath.isEmpty() && runningPath != path) {
            paths.append(runningPath);
        }
        for (const QString& p : std::as_const(paths)) {
            if (QFile::exists(p) && !QFile::remove(p) && !QFile::resize(p, 0)) {
                qCWarning(lcRecordings) << "cannot erase the spectrum recorder's file" << p;
            }
        }
        workerStopTimerIfIdle();
    }, Qt::QueuedConnection);
    m_dsrFigures = DsrStatus{};
    m_dsrFiguresShownMs = -1;
    if (wasRunning) {
        // Queued after the erase on the worker: a new, empty ring file.
        startDsr(runningPath, limit);
    }
    emit dsrChanged();
}

void RecordingService::setRadioStreaming(bool on)
{
    if (on == m_radioStreaming) {
        return;
    }
    m_radioStreaming = on;
    if (!on) {
        m_liveAfterKey.clear();
        // The radio is gone: no slice stays switched to the past into a
        // reconnect (the teardown drops the worker's replays), a stretch
        // slice A plays included. They go back to their frequency before,
        // as stopDsr does.
        endAllRewinds(true);
    }
    applyDsr();
}

void RecordingService::applyDsr()
{
    DsrOff problem = DsrOff::None;
    QString path;
    bool want = false;
    const qint64 limit =
        qint64(std::max(m_settings.dsrSizeLimitGb, RecordingSettings::kDsrSizeLimitGbMin)) * kBytesPerGb;
    if (m_settings.dsrEnabled) {
        problem = dsrDiskProblem(dsrVolumes());
        want = problem == DsrOff::None && m_radioStreaming;
        path = QDir(m_settings.dsrFolder()).filePath(QString::fromLatin1(kDsrFileName));
    }
    if (m_dsrRunning && (!want || path != m_dsrPath || limit != m_dsrLimit)) {
        stopDsr();
    }
    if (want && !m_dsrRunning) {
        startDsr(path, limit);
    }
    m_dsrOff = problem;
    m_dsrReasonKnown = true;
    updateDsrDiskTimer();
    emit dsrChanged();
}

void RecordingService::startDsr(const QString& path, qint64 limitBytes)
{
    ensureWorker();
    m_dsr->setAccepting(false);
    // What each receiver streams now; streamCentreChanged follows it.
    if (m_radio) {
        for (const SliceModel* slice : m_radio->slices()) {
            const int stream = slice != nullptr ? slice->streamIndex() : -1;
            if (stream >= 0) {
                m_dsr->setStream(stream, std::llround(m_radio->streamCentreHz(stream)),
                                 m_radio->streamSampleRateHz(stream));
            }
        }
    }
    const quint64 token = ++m_dsrToken;
    m_dsrRunning = true;
    m_dsrPath = path;
    m_dsrLimit = limitBytes;
    m_dsrFiguresShownMs = -1;
    const Clock clock = m_clock;
    QMetaObject::invokeMethod(m_workerContext, [this, token, path, limitBytes, clock] {
        m_dsrWorkerClock = clock;
        m_dsrWorkerToken = token;
        m_dsrWorkerBlocks = 0;
        QString error;
        if (!m_dsr->begin(path, limitBytes, &error)) {
            qCWarning(lcRecordings) << "the spectrum recorder cannot start:" << error;
            QMetaObject::invokeMethod(this, [this, token] { onDsrFailed(token); }, Qt::QueuedConnection);
            return;
        }
        m_dsrWorkerOpen = true;
        const DsrStatus figures = m_dsr->figures();
        QMetaObject::invokeMethod(this, [this, token, figures] { onDsrFigures(token, figures); },
                                  Qt::QueuedConnection);
        if (m_workerTimer && !m_workerTimer->isActive()) {
            m_workerTimer->start();
        }
    }, Qt::QueuedConnection);
    m_dsr->setAccepting(true);
}

void RecordingService::stopDsr()
{
    if (!m_dsrRunning) {
        return;
    }
    // Nothing is kept to play: rewound slices go back to the radio.
    endAllRewinds(true);
    m_dsrRunning = false;
    m_dsr->setAccepting(false);
    const quint64 token = m_dsrToken;
    QMetaObject::invokeMethod(m_workerContext, [this, token] {
        if (m_dsrWorkerOpen) {
            m_dsrWorkerOpen = false;
            m_dsr->finish(m_dsrWorkerClock() * kNsPerMs);
            const DsrStatus figures = m_dsr->figures();
            QMetaObject::invokeMethod(this, [this, token, figures] { onDsrFigures(token, figures); },
                                      Qt::QueuedConnection);
        }
        workerStopTimerIfIdle();
    }, Qt::QueuedConnection);
}

void RecordingService::checkDsrDisk()
{
    if (!m_settings.dsrEnabled || !m_radioStreaming) {
        return;
    }
    const DsrOff problem = dsrDiskProblem(dsrVolumes());
    if (m_dsrRunning && problem != DsrOff::None) {
        // Section 7: the recorder stops and says its disk is missing;
        // recordings go on.
        qCWarning(lcRecordings) << "the spectrum recorder stops:" << dsrReasonWords(problem);
        stopDsr();
        m_dsrOff = problem;
        emit dsrChanged();
    } else if (!m_dsrRunning && problem == DsrOff::None && m_dsrOff == DsrOff::DiskMissing) {
        // The disk is back.
        applyDsr();
    }
}

void RecordingService::updateDsrDiskTimer()
{
    // Only a chosen disk can go away; the Core's own cannot.
    const bool watch = m_settings.dsrEnabled && m_radioStreaming && !m_settings.dsrDisk.isEmpty();
    if (watch && !m_dsrDiskTimer) {
        m_dsrDiskTimer = new QTimer(this);
        m_dsrDiskTimer->setInterval(kDsrDiskCheckMs);
        connect(m_dsrDiskTimer, &QTimer::timeout, this, &RecordingService::checkDsrDisk);
    }
    if (m_dsrDiskTimer) {
        if (watch && !m_dsrDiskTimer->isActive()) {
            m_dsrDiskTimer->start();
        } else if (!watch && m_dsrDiskTimer->isActive()) {
            m_dsrDiskTimer->stop();
        }
    }
}

void RecordingService::onDsrFigures(quint64 token, const DsrStatus& figures)
{
    if (token != m_dsrToken) {
        return;
    }
    m_dsrFigures = figures;
    const qint64 t = now();
    if (!m_dsrRunning || m_dsrFiguresShownMs < 0 || t - m_dsrFiguresShownMs >= kDsrFiguresRefreshMs) {
        m_dsrFiguresShownMs = t;
        emit dsrChanged();
    }
}

void RecordingService::onDsrFailed(quint64 token)
{
    if (token != m_dsrToken || !m_dsrRunning) {
        return;
    }
    endAllRewinds(true);
    m_dsrRunning = false;
    m_dsr->setAccepting(false);
    const DsrOff problem = dsrDiskProblem(dsrVolumes());
    m_dsrOff = problem == DsrOff::None ? DsrOff::CannotWrite : problem;
    m_dsrReasonKnown = true;
    qCWarning(lcRecordings) << "the spectrum recorder stops:" << dsrReasonWords(m_dsrOff);
    // The worker has closed the file; this sends what it still holds.
    QMetaObject::invokeMethod(m_workerContext, [this, token] {
        const DsrStatus figures = m_dsr->figures();
        workerStopTimerIfIdle();
        QMetaObject::invokeMethod(this, [this, token, figures] { onDsrFigures(token, figures); },
                                  Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    emit dsrChanged();
}

void RecordingService::workerDrainDsr()
{
    if (!m_dsrWorkerOpen) {
        return;
    }
    const quint64 token = m_dsrWorkerToken;
    if (!m_dsr->drain(m_dsrWorkerClock() * kNsPerMs)) {
        m_dsrWorkerOpen = false;
        workerStopTimerIfIdle();
        QMetaObject::invokeMethod(this, [this, token] { onDsrFailed(token); }, Qt::QueuedConnection);
        return;
    }
    if (m_dsr->blocksWritten() != m_dsrWorkerBlocks) {
        m_dsrWorkerBlocks = m_dsr->blocksWritten();
        const DsrStatus figures = m_dsr->figures();
        QMetaObject::invokeMethod(this, [this, token, figures] { onDsrFigures(token, figures); },
                                  Qt::QueuedConnection);
    }
}

// ---- Rewind sound (spec 3, 6.7, recording plan Task 22).
//
// The slice's frequency moves on the main thread; the past is read ahead on
// the recordings worker (DsrReplay::fill) into a lock-free ring; once the
// first read is in, the DSP worker's switch takes the slice's demodulator
// off the radio's I/Q and onto that ring (DsrRewindHost::feedSlice). The
// recorder's own tap sees only the radio's I/Q, before the switch, so it
// keeps the live signal while a slice plays the past.

namespace {
constexpr int kMaxRewoundSlices = 32;
}  // namespace

bool RecordingService::rewind(int slice, qint64 utcNs, qint64 frequencyHz, QString* refusal)
{
    const auto refuse = [refusal](const char* why) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(why);
        }
        return false;
    };
    if (!m_rewindHost || slice < 0 || slice >= kMaxRewoundSlices || !m_rewindHost->sliceOpen(slice)) {
        return refuse(kRewindNoSliceReason);
    }
    if (m_rewindHost->sliceOnDiversity(slice)) {
        return refuse(kRewindDiversityReason);
    }
    // Task 25: an opened stretch plays on slice A wherever it keeps the
    // moment; the recorder's ring otherwise.
    const std::shared_ptr<DsrRingFile> stretch = stretchFor(slice, utcNs);
    if ((!stretch && !m_dsrRunning) || frequencyHz <= 0) {
        return refuse(kRewindGoneReason);
    }
    // The receiver that kept that moment with that frequency in its span,
    // the slice's own receiver first.
    const DsrRingFile& ring = stretch ? *stretch : m_dsr->ring();
    QList<int> receivers = ring.receivers();
    const int own = m_rewindHost->sliceReceiver(slice);
    if (receivers.removeOne(own)) {
        receivers.prepend(own);
    }
    int receiver = -1;
    bool momentKept = false;
    DsrBlockHeader kept;
    for (const int r : std::as_const(receivers)) {
        DsrBlockHeader h;
        if (!ring.blockAt(r, utcNs, &h) || h.sampleRateHz <= 0) {
            continue;
        }
        momentKept = true;
        if (std::llabs(frequencyHz - h.centreHz) * 2 > qint64(h.sampleRateHz)) {
            continue;
        }
        receiver = r;
        kept = h;
        break;
    }
    if (receiver < 0) {
        // Kept then, but not at this frequency: a different reason, so the
        // flag can be moved into what was kept.
        return refuse(momentKept ? kRewindNoFrequencyReason : kRewindGoneReason);
    }
    // The slice moves to the moment's frequency. Its demodulator runs at its
    // receiver's rate, which must be the rate that moment was kept at.
    const qint64 before = m_rewindHost->sliceFrequencyHz(slice);
    const auto existing = m_rewound.constFind(slice);
    const qint64 liveHz = existing != m_rewound.cend() ? existing->liveFrequencyHz : before;
    // Roundtable keeps the slice where it was live through this retune too
    // (rewoundLiveFrequencyHz), before the slice is listed as rewound.
    m_rewindRetuneSlice = slice;
    m_rewindRetuneLiveHz = liveHz;
    if (before != frequencyHz) {
        m_rewindHost->setSliceFrequencyHz(slice, frequencyHz);
    }
    const int nowReceiver = m_rewindHost->sliceReceiver(slice);
    if (nowReceiver < 0 || m_rewindHost->receiverRateHz(nowReceiver) != kept.sampleRateHz) {
        if (before != frequencyHz) {
            m_rewindHost->setSliceFrequencyHz(slice, before);
        }
        m_rewindRetuneSlice = -1;
        emit rewoundSlicesChanged();
        return refuse(kRewindRateReason);
    }
    m_rewindRetuneSlice = -1;
    m_liveAfterKey.remove(slice);
    Rewound& entry = m_rewound[slice];
    entry.liveFrequencyHz = liveHz;
    if (entry.pending) {
        dropReplayOnWorker(entry.pending);
    }
    entry.pendingToken = m_nextRewindToken++;
    entry.source = stretch;
    entry.keptCentreHz = kept.centreHz;
    entry.keptSpanHz = kept.sampleRateHz;
    entry.pending = startReplay(slice, receiver, kept.sampleRateHz, utcNs, entry.pendingToken, stretch);
    emit dsrChanged();
    emit rewoundSlicesChanged();
    return true;
}

bool RecordingService::goLive(int slice, QString* refusal)
{
    Q_UNUSED(refusal);
    if (m_rewound.contains(slice)) {
        endRewind(slice, true);
        return true;
    }
    // After PTT took it live: LIVE still returns to the frequency before
    // rewinding (spec 3).
    const auto after = m_liveAfterKey.constFind(slice);
    if (after != m_liveAfterKey.cend()) {
        const qint64 hz = *after;
        m_liveAfterKey.erase(after);
        if (m_rewindHost && m_rewindHost->sliceOpen(slice)) {
            m_rewindHost->setSliceFrequencyHz(slice, hz);
        }
    }
    return true;
}

bool RecordingService::seekRewind(int slice, qint64 utcNs, QString* refusal)
{
    const auto refuse = [refusal](const char* why) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(why);
        }
        return false;
    };
    const auto it = m_rewound.find(slice);
    if (it == m_rewound.end()) {
        return refuse(kRewindNotRewoundReason);
    }
    const std::shared_ptr<DsrReplay>& current = it->pending ? it->pending : it->replay;
    if (!current || (!it->source && !m_dsrRunning)) {
        return refuse(kRewindGoneReason);
    }
    const DsrRingFile& ring = it->source ? *it->source : m_dsr->ring();
    DsrBlockHeader h;
    if (!ring.blockAt(current->receiver(), utcNs, &h)) {
        return refuse(kRewindGoneReason);
    }
    if (h.sampleRateHz != current->sampleRateHz()) {
        return refuse(kRewindRateReason);
    }
    const int receiver = current->receiver();
    const int rate = current->sampleRateHz();
    if (it->pending) {
        dropReplayOnWorker(it->pending);
    }
    it->pendingToken = m_nextRewindToken++;
    it->keptCentreHz = h.centreHz;
    it->keptSpanHz = h.sampleRateHz;
    it->pending = startReplay(slice, receiver, rate, utcNs, it->pendingToken, it->source);
    emit dsrChanged();
    return true;
}

QList<DsrRewoundSlice> RecordingService::rewound() const
{
    QList<int> slices = m_rewound.keys();
    std::sort(slices.begin(), slices.end());
    QList<DsrRewoundSlice> out;
    for (const int slice : std::as_const(slices)) {
        const Rewound& entry = m_rewound[slice];
        const std::shared_ptr<DsrReplay>& playing = entry.pending ? entry.pending : entry.replay;
        DsrRewoundSlice r;
        r.slice = slice;
        r.utcNs = playing ? (entry.pending ? playing->fromUtcNs() : playing->positionUtcNs()) : 0;
        r.frequencyHz = m_rewindHost ? m_rewindHost->sliceFrequencyHz(slice) : 0;
        r.liveFrequencyHz = entry.liveFrequencyHz;
        r.keptCentreHz = entry.keptCentreHz;
        r.keptSpanHz = entry.keptSpanHz;
        out.append(r);
    }
    return out;
}

std::optional<qint64> RecordingService::rewoundLiveFrequencyHz(int slice) const
{
    if (slice >= 0 && slice == m_rewindRetuneSlice) {
        return m_rewindRetuneLiveHz;
    }
    const auto it = m_rewound.constFind(slice);
    if (it == m_rewound.cend()) {
        return std::nullopt;
    }
    return it->liveFrequencyHz;
}

void RecordingService::rewoundSlicesLiveForTransmit()
{
    const QList<int> slices = m_rewound.keys();
    for (const int slice : slices) {
        m_liveAfterKey.insert(slice, m_rewound[slice].liveFrequencyHz);
        endRewind(slice, false);
    }
}

void RecordingService::setDsrRewindHostForTest(std::unique_ptr<DsrRewindHost> host)
{
    m_rewindHost = std::move(host);
}

std::shared_ptr<DsrReplay> RecordingService::replayForTest(int slice) const
{
    const auto it = m_rewound.constFind(slice);
    return it != m_rewound.cend() ? it->replay : nullptr;
}

std::shared_ptr<DsrReplay> RecordingService::startReplay(int slice, int receiver, int rateHz, qint64 utcNs,
                                                         quint64 token, std::shared_ptr<DsrRingFile> source)
{
    const qint64 liveCentre =
        m_rewindHost ? m_rewindHost->receiverCentreHz(m_rewindHost->sliceReceiver(slice)) : 0;
    auto replay = std::make_shared<DsrReplay>(slice, receiver, rateHz, utcNs, liveCentre);
    ensureWorker();
    QMetaObject::invokeMethod(m_workerContext, [this, slice, token, replay, source] {
        WorkerReplay w;
        w.slice = slice;
        w.token = token;
        w.replay = replay;
        w.source = source;
        m_workerReplays.push_back(std::move(w));
        workerFillReplays();
        if (m_workerTimer && !m_workerTimer->isActive()) {
            m_workerTimer->start();
        }
    }, Qt::QueuedConnection);
    return replay;
}

void RecordingService::dropReplayOnWorker(const std::shared_ptr<DsrReplay>& replay)
{
    if (!replay || m_workerContext == nullptr) {
        return;
    }
    DsrReplay* const gone = replay.get();
    QMetaObject::invokeMethod(m_workerContext, [this, gone] {
        m_workerReplays.erase(std::remove_if(m_workerReplays.begin(), m_workerReplays.end(),
                                             [gone](const WorkerReplay& w) { return w.replay.get() == gone; }),
                              m_workerReplays.end());
        workerStopTimerIfIdle();
    }, Qt::QueuedConnection);
}

void RecordingService::onReplayReady(int slice, quint64 token)
{
    const auto it = m_rewound.find(slice);
    if (it == m_rewound.end() || it->pendingToken != token || !it->pending) {
        return;
    }
    // From the next block the slice's demodulator plays the past.
    if (!m_rewindHost || !m_rewindHost->feedSlice(slice, it->pending)) {
        // No DSP worker to switch: the slice stays on the radio.
        endRewind(slice, false);
        return;
    }
    if (it->replay) {
        dropReplayOnWorker(it->replay);
    }
    it->replay = std::move(it->pending);
    it->replayToken = it->pendingToken;
    it->pending.reset();
    emit dsrChanged();
}

void RecordingService::onReplayFinished(int slice, quint64 token)
{
    const auto it = m_rewound.constFind(slice);
    if (it == m_rewound.cend() || it->replayToken != token || it->pending) {
        return;
    }
    // Played up to the newest second kept: the DSP worker already feeds the
    // radio's I/Q, on the frequency being played.
    endRewind(slice, false);
}

void RecordingService::endRewind(int slice, bool restoreFrequency)
{
    const auto it = m_rewound.find(slice);
    if (it == m_rewound.end()) {
        return;
    }
    const Rewound entry = *it;
    m_rewound.erase(it);
    // Task 25: slice A going live closes the stretch it played.
    if (entry.source && entry.source == m_stretch.ring) {
        m_stretch = OpenStretch{};
    }
    if (m_rewindHost) {
        m_rewindHost->feedSlice(slice, nullptr);
    }
    dropReplayOnWorker(entry.replay);
    dropReplayOnWorker(entry.pending);
    if (restoreFrequency && m_rewindHost && m_rewindHost->sliceOpen(slice)
        && m_rewindHost->sliceFrequencyHz(slice) != entry.liveFrequencyHz) {
        m_rewindHost->setSliceFrequencyHz(slice, entry.liveFrequencyHz);
    }
    emit dsrChanged();
    emit rewoundSlicesChanged();
}

void RecordingService::endAllRewinds(bool restoreFrequency)
{
    const QList<int> slices = m_rewound.keys();
    for (const int slice : slices) {
        endRewind(slice, restoreFrequency);
    }
}

void RecordingService::followLiveCentres()
{
    if (!m_rewindHost) {
        return;
    }
    QList<int> rateChanged;
    for (auto it = m_rewound.cbegin(); it != m_rewound.cend(); ++it) {
        const int receiver = m_rewindHost->sliceReceiver(it.key());
        const qint64 centre = m_rewindHost->receiverCentreHz(receiver);
        for (const std::shared_ptr<DsrReplay>& replay : {it->replay, it->pending}) {
            if (!replay) {
                continue;
            }
            replay->setLiveCentreHz(centre);
            if (m_rewindHost->receiverRateHz(receiver) != replay->sampleRateHz()) {
                rateChanged.append(it.key());
            }
        }
    }
    // The receiver's rate changed: its demodulator can no longer take the
    // past, so the slice plays the radio on the frequency it is on.
    for (const int slice : std::as_const(rateChanged)) {
        endRewind(slice, false);
    }
}

void RecordingService::workerFillReplays()
{
    for (WorkerReplay& w : m_workerReplays) {
        w.replay->fill(w.source ? *w.source : m_dsr->ring());
        const int slice = w.slice;
        const quint64 token = w.token;
        if (!w.readyTold && w.replay->primed()) {
            w.readyTold = true;
            QMetaObject::invokeMethod(this, [this, slice, token] { onReplayReady(slice, token); },
                                      Qt::QueuedConnection);
        }
        if (!w.finishedTold && w.replay->finished()) {
            w.finishedTold = true;
            QMetaObject::invokeMethod(this, [this, slice, token] { onReplayFinished(slice, token); },
                                      Qt::QueuedConnection);
        }
    }
}

// ---- The past picture (spec 3, 6.7, A12, recording plan Task 23).
//
// The request is checked here, on the main thread, against the slice's
// receiver; the drawing (disk reads and one FFT per row) runs on the
// recordings worker, and the answer comes back here.

bool RecordingService::checkPastRequest(int slice, const DsrPanView& view, int* receiver, int* liveRateHz,
                                        qint64* liveCentreHz, QString* refusal) const
{
    const auto refuse = [refusal](const char* why) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(why);
        }
        return false;
    };
    // The pan's own settings first, then the slice and its receiver.
    if (view.fftSize < DsrPastRenderer::kMinFftSize || view.fftSize > DsrPastRenderer::kMaxFftSize
        || (view.fftSize & (view.fftSize - 1)) != 0) {
        return refuse(kPastSizeReason);
    }
    if (view.windowType < static_cast<int>(WindowFunction::Rectangular)
        || view.windowType >= static_cast<int>(WindowFunction::Count)) {
        return refuse(kPastWindowReason);
    }
    // 0 keeps the bins unreduced (a caller on the Core); the wire asks for
    // 1 or more (SessionCommandDispatcher sends 0 here as -1).
    if (view.pixels < 0 || view.pixels > DsrPastRenderer::kMaxPixels) {
        return refuse(kPastPixelsReason);
    }
    if (view.spanHz <= 0) {
        return refuse(kPastSpanReason);
    }
    if (!m_rewindHost || slice < 0 || !m_rewindHost->sliceOpen(slice)) {
        return refuse(kRewindNoSliceReason);
    }
    *receiver = m_rewindHost->sliceReceiver(slice);
    if (*receiver < 0) {
        return refuse(kRewindNoSliceReason);
    }
    *liveRateHz = m_rewindHost->receiverRateHz(*receiver);
    *liveCentreHz = m_rewindHost->receiverCentreHz(*receiver);
    // The rate the receiver keeps: its live rate, or the one it was last
    // kept at.
    int keptRate = *liveRateHz;
    if (keptRate <= 0 && m_dsrRunning) {
        const DsrRingFile& ring = m_dsr->ring();
        DsrBlockHeader h;
        if (ring.blockAt(*receiver, ring.newestUtcNs(*receiver) - 1, &h)) {
            keptRate = h.sampleRateHz;
        }
    }
    if (keptRate > 0 && view.spanHz > keptRate) {
        return refuse(kPastSpanReason);
    }
    // Task 25: slice A's pan reads an opened stretch with the recorder off.
    if (!m_dsrRunning && !(slice == kStretchSlice && m_stretch.ring)) {
        return refuse(kRewindGoneReason);
    }
    return true;
}

DsrPastRenderer& RecordingService::workerPastRenderer()
{
    if (!m_pastRenderer) {
        m_pastRenderer = std::make_unique<DsrPastRenderer>();
        // A drawing can run longer than the rewound sound's read-ahead
        // (DsrReplay::kReadAheadMs), and the worker's rounds wait behind
        // it: top the sound up before each transform. fill returns at once
        // when the read-ahead is full. Safe here: a drawing job holds no
        // iterator into m_workerReplays, and the ready and finished notices
        // are queued to the main thread as from a round.
        m_pastRenderer->setBetweenWork([this] { workerBetweenPastWork(true); });
    }
    m_pastRenderThread.store(QThread::currentThread());
    return *m_pastRenderer;
}

DsrPastRenderer& RecordingService::workerSpectrumRenderer()
{
    if (!m_spectrumRenderer) {
        m_spectrumRenderer = std::make_unique<DsrPastRenderer>();
        // Never takes spectrum asks itself: it may be drawing one between
        // two transforms of a rows drawing already.
        m_spectrumRenderer->setBetweenWork([this] { workerBetweenPastWork(false); });
    }
    m_pastRenderThread.store(QThread::currentThread());
    return *m_spectrumRenderer;
}

void RecordingService::workerBetweenPastWork(bool serveSpectra)
{
    workerFillReplays();
    // The recorder's I/Q ring holds well under a second at the widest
    // rates; a long drawing must not leave it to overflow.
    workerDrainDsr();
    if (!serveSpectra || m_workerTakingSpectra || m_spectrumContext == nullptr) {
        return;
    }
    // Only the spectrum asks queued meanwhile run here (each one only
    // records itself), then the newest of them are drawn and answered.
    m_workerTakingSpectra = true;
    QCoreApplication::sendPostedEvents(m_spectrumContext, QEvent::MetaCall);
    m_workerTakingSpectra = false;
    workerServeSpectra();
}

void RecordingService::workerServeSpectra()
{
    while (!m_workerSpectra.empty()) {
        PendingSpectrum p = std::move(m_workerSpectra.front());
        m_workerSpectra.erase(m_workerSpectra.begin());
        DsrPastFrames frame;
        const bool kept = workerSpectrumRenderer().spectrum(p.source ? *p.source : m_dsr->ring(), p.readReceiver,
                                                            p.liveRateHz, p.liveCentreHz, p.view, p.utcNs, &frame);
        frame.slice = p.slice;
        for (PastFramesDone& done : p.dones) {
            QMetaObject::invokeMethod(this, [done = std::move(done), kept, frame] {
                done(kept, kept ? QString() : QString::fromLatin1(kRewindGoneReason), frame);
            }, Qt::QueuedConnection);
        }
    }
}

void RecordingService::pastRows(int slice, const DsrPanView& view, qint64 fromUtcNs, qint64 toUtcNs,
                                int rowCount, PastFramesDone done)
{
    int receiver = -1;
    int liveRate = 0;
    qint64 liveCentre = 0;
    QString refusal;
    if (rowCount < 1 || rowCount > DsrPastRenderer::kMaxRows) {
        refusal = QString::fromLatin1(kPastRowsReason);
    }
    if (!refusal.isEmpty() || !checkPastRequest(slice, view, &receiver, &liveRate, &liveCentre, &refusal)) {
        done(false, refusal, DsrPastFrames{});
        return;
    }
    if (toUtcNs < fromUtcNs) {
        std::swap(fromUtcNs, toUtcNs);
    }
    int readReceiver = receiver;
    const std::shared_ptr<DsrRingFile> source = pastSource(slice, receiver, fromUtcNs, toUtcNs, &readReceiver);
    runOnWorker([this, slice, view, fromUtcNs, toUtcNs, rowCount, readReceiver, liveRate, liveCentre, source,
                 done = std::move(done)]() mutable {
        DsrPastFrames frames = workerPastRenderer().rows(source ? *source : m_dsr->ring(), readReceiver, liveRate,
                                                         liveCentre, view, fromUtcNs, toUtcNs, rowCount);
        frames.slice = slice;
        QMetaObject::invokeMethod(this, [done = std::move(done), frames] { done(true, QString(), frames); },
                                  Qt::QueuedConnection);
    });
}

void RecordingService::pastSpectrum(int slice, const DsrPanView& view, qint64 utcNs, PastFramesDone done)
{
    int receiver = -1;
    int liveRate = 0;
    qint64 liveCentre = 0;
    QString refusal;
    if (!checkPastRequest(slice, view, &receiver, &liveRate, &liveCentre, &refusal)) {
        done(false, refusal, DsrPastFrames{});
        return;
    }
    int readReceiver = receiver;
    const std::shared_ptr<DsrRingFile> source = pastSource(slice, receiver, utcNs, utcNs, &readReceiver);
    PendingSpectrum ask;
    ask.slice = slice;
    ask.view = view;
    ask.utcNs = utcNs;
    ask.readReceiver = readReceiver;
    ask.liveRateHz = liveRate;
    ask.liveCentreHz = liveCentre;
    ask.source = source;
    ask.dones.push_back(std::move(done));
    ensureWorker();
    // Queued on its own context: a rows drawing takes it between two
    // transforms (workerBetweenPastWork) instead of after the whole batch.
    QMetaObject::invokeMethod(m_spectrumContext, [this, ask = std::move(ask)]() mutable {
        const auto same = std::find_if(m_workerSpectra.begin(), m_workerSpectra.end(),
                                       [&ask](const PendingSpectrum& p) {
                                           return p.slice == ask.slice && p.view.fftSize == ask.view.fftSize
                                               && p.view.windowType == ask.view.windowType
                                               && p.view.centreHz == ask.view.centreHz
                                               && p.view.spanHz == ask.view.spanHz
                                               && p.view.pixels == ask.view.pixels;
                                       });
        if (same != m_workerSpectra.end()) {
            // The newer moment for every caller still waiting.
            std::vector<PastFramesDone> dones = std::move(same->dones);
            for (PastFramesDone& d : ask.dones) {
                dones.push_back(std::move(d));
            }
            ask.dones = std::move(dones);
            *same = std::move(ask);
        } else {
            m_workerSpectra.push_back(std::move(ask));
        }
        if (!m_workerTakingSpectra) {
            workerServeSpectra();
        }
    }, Qt::QueuedConnection);
}

void RecordingService::pastHours(HoursDone done)
{
    const std::shared_ptr<DsrRingFile> stretch = m_stretch.ring;
    if (!m_dsrRunning && !stretch) {
        // Nothing is kept.
        done({});
        return;
    }
    const bool running = m_dsrRunning;
    runOnWorker([this, running, stretch, done = std::move(done)]() mutable {
        QList<DsrHour> hours = running ? workerPastRenderer().hours(m_dsr->ring()) : QList<DsrHour>{};
        // Task 25: an opened stretch's hours too, newest first.
        if (stretch) {
            for (const DsrHour& h : workerPastRenderer().hours(*stretch)) {
                const auto same = std::find_if(hours.begin(), hours.end(), [&h](const DsrHour& o) {
                    return o.startUtcNs == h.startUtcNs;
                });
                if (same == hours.end()) {
                    hours.append(h);
                } else {
                    same->activity = std::max(same->activity, h.activity);
                }
            }
            std::sort(hours.begin(), hours.end(),
                      [](const DsrHour& a, const DsrHour& b) { return a.startUtcNs > b.startUtcNs; });
        }
        QMetaObject::invokeMethod(this, [done = std::move(done), hours] { done(hours); }, Qt::QueuedConnection);
    });
}

// ---- Saving a stretch (spec 3 "Saving a stretch", 6.3, 6.4, 6.7 SAVE, A13;
// recording plan Task 25).
//
// Checked here on the main thread from the ring's index (no disk read); the
// copy or the sound runs on the recordings worker in steps, each one
// re-queued behind the worker's own rounds, and the recording goes into
// the store here when it is done.

namespace {

QString utcText(qint64 utcNs)
{
    return QDateTime::fromMSecsSinceEpoch(utcNs / kNsPerMs, QTimeZone::utc()).toString(QStringLiteral("HH:mm"));
}

}  // namespace

namespace {

// A saved sound's format, as a note names it.
QString savedFormatWord(bool asWav)
{
    return asWav ? QStringLiteral("WAV") : QStringLiteral("MP3");
}

}  // namespace

// About two seconds of 48 kHz sound per step of a sound-only save, and room
// for the resampler's few frames of carry.
constexpr int kSaveStepFrames = DsrSoundRender::kDspRateHz * 2;
constexpr int kSaveResampleSlack = 256;

struct RecordingService::SaveJob {
    quint64 token = 0;
    bool wholePan = true;
    std::shared_ptr<DsrRingFile> source;   // null: the recorder's ring
    DsrStretchExtent extent;
    QString path;
    std::unique_ptr<DsrStretchCopy> copy;
    std::unique_ptr<DsrSoundRender> render;
    std::unique_ptr<AudioFileWriter> writer;
    // The sound is demodulated at 48 kHz, as the live slices are, and
    // brought to the file's rate as a Contact recording's is.
    std::unique_ptr<Resampler> resampler;
    std::vector<float> sound;
    std::vector<float> resampled;
};

std::shared_ptr<DsrRingFile> RecordingService::stretchFor(int slice, qint64 utcNs) const
{
    if (slice != kStretchSlice || !m_stretch.ring || utcNs < m_stretch.fromUtcNs || utcNs >= m_stretch.toUtcNs) {
        return nullptr;
    }
    return m_stretch.ring;
}

std::shared_ptr<DsrRingFile> RecordingService::pastSource(int slice, int liveReceiver, qint64 fromUtcNs,
                                                          qint64 toUtcNs, int* receiver) const
{
    if (slice != kStretchSlice || !m_stretch.ring || toUtcNs < m_stretch.fromUtcNs
        || fromUtcNs >= m_stretch.toUtcNs) {
        return nullptr;
    }
    // The receiver slice A plays the stretch from, else the slice's own,
    // else the stretch's first.
    const QList<int> kept = m_stretch.ring->receivers();
    int chosen = kept.isEmpty() ? liveReceiver : kept.first();
    const auto it = m_rewound.constFind(slice);
    if (it != m_rewound.cend() && it->source == m_stretch.ring) {
        const std::shared_ptr<DsrReplay>& playing = it->pending ? it->pending : it->replay;
        if (playing) {
            chosen = playing->receiver();
        }
    } else if (kept.contains(liveReceiver)) {
        chosen = liveReceiver;
    }
    *receiver = chosen;
    return m_stretch.ring;
}

void RecordingService::closeStretch()
{
    if (m_stretch.id.isEmpty()) {
        return;
    }
    const auto it = m_rewound.constFind(kStretchSlice);
    if (it != m_rewound.cend() && it->source == m_stretch.ring) {
        endRewind(kStretchSlice, true);   // closes it too
    }
    m_stretch = OpenStretch{};
    emit dsrChanged();
}

bool RecordingService::saveStretch(qint64 fromUtcNs, qint64 toUtcNs, bool wholePan, QString* note,
                                   QString* refusal)
{
    const auto refuse = [refusal](const char* why) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(why);
        }
        return false;
    };
    if (toUtcNs < fromUtcNs) {
        std::swap(fromUtcNs, toUtcNs);
    }
    // The recorder's ring, or an opened stretch that keeps more of it.
    std::shared_ptr<DsrRingFile> source;
    DsrStretchExtent extent;
    if (m_dsrRunning) {
        extent = DsrStretch::extent(m_dsr->ring(), fromUtcNs, toUtcNs);
    }
    if (m_stretch.ring) {
        const DsrStretchExtent fromStretch = DsrStretch::extent(*m_stretch.ring, fromUtcNs, toUtcNs);
        if (fromStretch.toUtcNs - fromStretch.fromUtcNs > extent.toUtcNs - extent.fromUtcNs) {
            extent = fromStretch;
            source = m_stretch.ring;
        }
    }
    if (!extent.kept() || extent.toUtcNs - extent.fromUtcNs < kMinStretchNs / 2) {
        return refuse(kSaveNothingKeptReason);
    }
    const DsrRingFile& ring = source ? *source : m_dsr->ring();

    // Slice A as it is now: its frequency names the recording, and its
    // sound is what sound only keeps.
    DsrSoundSettings sound;
    const bool sliceOpen = m_rewindHost && m_rewindHost->sliceSound(kStretchSlice, &sound);
    const int own = m_rewindHost ? m_rewindHost->sliceReceiver(kStretchSlice) : -1;
    DsrBlockHeader first;
    int soundReceiver = sliceOpen
        ? DsrStretch::receiverFor(ring, extent.fromUtcNs, extent.toUtcNs, sound.frequencyHz, own, &first)
        : -1;
    if (!wholePan && soundReceiver < 0) {
        return refuse(kSaveSliceReason);
    }
    if (soundReceiver < 0) {
        // The pan's centre names a whole-pan stretch slice A is not in.
        soundReceiver = extent.receivers.first();
        if (!ring.blockAt(soundReceiver, extent.fromUtcNs, &first)) {
            ring.nextBlock(soundReceiver, extent.fromUtcNs, &first);
        }
        if (!sliceOpen) {
            sound.frequencyHz = first.centreHz;
        }
    }

    RecordingEntry entry;
    entry.kind = wholePan ? RecordingKind::WholePan : RecordingKind::Sound;
    entry.sliceId = wholePan ? -1 : kStretchSlice;
    entry.startUtc = QDateTime::fromMSecsSinceEpoch(extent.fromUtcNs / kNsPerMs, QTimeZone::utc());
    entry.frequencyHz = sound.frequencyHz;
    entry.mode = sliceOpen ? SliceModel::modeName(static_cast<DSPMode>(sound.dspMode)) : QString();
    if (wholePan) {
        entry.centreHz = first.centreHz;
        entry.spanHz = first.sampleRateHz;
        entry.filterLowHz = int(std::lround(sound.filterLowHz));
        entry.filterHighHz = int(std::lround(sound.filterHighHz));
    }
    if (!startSave(source, extent, wholePan, sound, soundReceiver, first.sampleRateHz, entry, refusal)) {
        return false;
    }
    if (note != nullptr) {
        const bool whole = extent.fromUtcNs <= fromUtcNs + kStretchSlackNs && extent.toUtcNs + kStretchSlackNs >= toUtcNs;
        const QString format = savedFormatWord(m_saves.value(m_nextSaveToken - 1).asWav);
        const QString what = wholePan ? QStringLiteral("whole pan") : QStringLiteral("sound only as %1").arg(format);
        *note = whole ? QStringLiteral("Saving %1 to %2 UTC to Recordings, %3.")
                            .arg(utcText(extent.fromUtcNs), utcText(extent.toUtcNs), what)
                      : QStringLiteral("Only %1 to %2 UTC is still kept. Saving that part to Recordings, %3.")
                            .arg(utcText(extent.fromUtcNs), utcText(extent.toUtcNs), what);
    }
    return true;
}

bool RecordingService::saveSoundOnly(const QString& recordingId, QString* note, QString* refusal)
{
    ensureStoreOpen();
    const std::optional<RecordingEntry> entry = m_store->find(recordingId);
    if (!entry) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(kPlayGoneReason);
        }
        return false;
    }
    if (entry->kind != RecordingKind::WholePan) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(kSaveNotWholePanReason);
        }
        return false;
    }
    auto ring = std::make_shared<DsrRingFile>();
    QString error;
    if (!ring->open(QDir(m_store->folder()).filePath(entry->fileName), DsrStretchCopy::kNeverWrapBytes, &error)
        || ring->blockCount() == 0) {
        qCWarning(lcRecordings) << "saved stretch" << entry->fileName << "cannot be read:" << error;
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(kPlayFailedReason);
        }
        return false;
    }
    const DsrStretchExtent extent = DsrStretch::extent(*ring, ring->oldestUtcNs(), ring->newestUtcNs());
    // Slice A's sound as it was set when the stretch was saved.
    DsrSoundSettings sound;
    sound.frequencyHz = entry->frequencyHz;
    sound.dspMode = static_cast<int>(SliceModel::modeFromName(entry->mode));
    sound.filterLowHz = entry->filterLowHz;
    sound.filterHighHz = entry->filterHighHz;
    DsrBlockHeader first;
    const int receiver =
        DsrStretch::receiverFor(*ring, extent.fromUtcNs, extent.toUtcNs, sound.frequencyHz, -1, &first);
    if (!extent.kept() || receiver < 0) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(kSaveSliceReason);
        }
        return false;
    }
    RecordingEntry out;
    out.kind = RecordingKind::Sound;
    out.sliceId = kStretchSlice;
    out.startUtc = QDateTime::fromMSecsSinceEpoch(extent.fromUtcNs / kNsPerMs, QTimeZone::utc());
    out.frequencyHz = sound.frequencyHz;
    out.mode = entry->mode;
    out.call = entry->call;
    if (!startSave(ring, extent, false, sound, receiver, first.sampleRateHz, out, refusal)) {
        return false;
    }
    if (note != nullptr) {
        const QString format = savedFormatWord(m_saves.value(m_nextSaveToken - 1).asWav);
        *note = QStringLiteral("Saving the sound of %1 from that stretch to Recordings as %2.")
                    .arg(recordingFrequencyText(sound.frequencyHz), format);
    }
    return true;
}

bool RecordingService::startSave(std::shared_ptr<DsrRingFile> source, const DsrStretchExtent& extent,
                                 bool wholePan, const DsrSoundSettings& sound, int soundReceiver, int soundRateHz,
                                 RecordingEntry entry, QString* refusal)
{
    ensureStoreOpen();
    if (m_storeFolder.isEmpty()) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(kSaveNoFolderReason);
        }
        return false;
    }
    if (!wholePan && !DsrSoundRender::available()) {
        if (refusal != nullptr) {
            *refusal = QStringLiteral("This Core cannot pull the sound out of a stretch.");
        }
        return false;
    }
    ensureWorker();
    PendingSave pending;
    AudioFileFormat format = m_settings.format;
    if (!wholePan && format == AudioFileFormat::Mp3 && !mp3Available()) {
        // Section 7: no MP3 encoder means WAV.
        format = AudioFileFormat::Wav;
    }
    pending.asWav = format == AudioFileFormat::Wav;
    pending.suffix = wholePan ? QStringLiteral("ndsr") : suffixFor(format);
    pending.workPath = QDir(m_storeFolder).filePath(QStringLiteral(".stretch-%1.%2")
                                                        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                                             pending.suffix));
    pending.entry = std::move(entry);
    pending.lengthMs = (extent.toUtcNs - extent.fromUtcNs) / kNsPerMs;
    pending.fromRecorder = !source;
    const quint64 token = m_nextSaveToken++;
    m_saves.insert(token, pending);

    auto job = std::make_shared<SaveJob>();
    job->token = token;
    job->wholePan = wholePan;
    job->source = std::move(source);
    job->extent = extent;
    job->path = pending.workPath;
    const AudioFileSpec spec{format, ContactRecorder::kFileRateHz, 1, ContactRecorder::kMonoKbitPerSec};
    QMetaObject::invokeMethod(m_workerContext, [this, job, spec, sound, soundReceiver, soundRateHz] {
        QString error;
        bool ok = true;
        if (job->wholePan) {
            job->copy = std::make_unique<DsrStretchCopy>();
            ok = job->copy->begin(job->path, job->extent, &error);
        } else {
            job->render = std::make_unique<DsrSoundRender>();
            ok = job->render->begin(soundReceiver, soundRateHz, job->extent.fromUtcNs, job->extent.toUtcNs, sound,
                                    DsrSoundRender::kDspRateHz, &error);
            job->resampler = std::make_unique<Resampler>(double(DsrSoundRender::kDspRateHz),
                                                         double(spec.sampleRateHz), kSaveStepFrames);
            if (ok) {
                job->writer = m_writerFactory ? m_writerFactory(job->path, spec, &error)
                                              : AudioFileWriter::open(job->path, spec, &error);
                ok = job->writer != nullptr;
            }
        }
        if (!ok) {
            qCWarning(lcRecordings) << "a stretch cannot be saved:" << error;
            const quint64 token = job->token;
            QMetaObject::invokeMethod(this, [this, token, error] { onSaveDone(token, false, 0, error); },
                                      Qt::QueuedConnection);
            return;
        }
        workerSaveStep(job);
    }, Qt::QueuedConnection);
    emit dsrChanged();
    return true;
}

void RecordingService::workerSaveStep(const std::shared_ptr<SaveJob>& job)
{
    const DsrRingFile& ring = job->source ? *job->source : m_dsr->ring();
    bool done = false;
    bool ok = true;
    QString error;
    qint64 lengthMs = 0;
    if (job->copy) {
        if (job->copy->step(ring)) {
            done = true;
            ok = job->copy->finish(&error) && job->copy->bytesCopied() > 0;
            lengthMs = (job->copy->copiedToUtcNs() - job->copy->copiedFromUtcNs()) / kNsPerMs;
        }
    } else {
        job->sound.resize(size_t(kSaveStepFrames));
        job->resampled.resize(size_t(kSaveStepFrames) + kSaveResampleSlack);
        const int rendered = job->render->render(ring, job->sound.data(), kSaveStepFrames);
        const int n = rendered > 0 ? job->resampler->processInto(job->sound.data(), rendered, job->resampled.data(),
                                                                 int(job->resampled.size()))
                                   : 0;
        if (n > 0 && !job->writer->write(job->resampled.data(), n)) {
            done = true;
            ok = false;
            error = QStringLiteral("the disk refused the sound");
        } else if (job->render->done() || rendered <= 0) {
            done = true;
            ok = job->writer->finish();
            lengthMs = job->render->framesDone() * 1000 / DsrSoundRender::kDspRateHz;
        }
    }
    if (!done) {
        QMetaObject::invokeMethod(m_workerContext, [this, job] { workerSaveStep(job); }, Qt::QueuedConnection);
        return;
    }
    job->copy.reset();
    job->render.reset();
    job->writer.reset();
    const quint64 token = job->token;
    QMetaObject::invokeMethod(this, [this, token, ok, lengthMs, error] { onSaveDone(token, ok, lengthMs, error); },
                              Qt::QueuedConnection);
}

void RecordingService::onSaveDone(quint64 token, bool ok, qint64 lengthMs, const QString& error)
{
    const auto it = m_saves.find(token);
    if (it == m_saves.end()) {
        return;
    }
    PendingSave pending = it.value();
    m_saves.erase(it);
    if (pending.erased) {
        // What it read from was erased under it: never a whole stretch.
        QFile::remove(pending.workPath);
        emit stretchSaveFailed(QString::fromLatin1(kSaveErasedReason));
        emit dsrChanged();
        return;
    }
    if (!ok) {
        qCWarning(lcRecordings) << "a stretch was not saved:" << error;
        QFile::remove(pending.workPath);
        emit stretchSaveFailed(QString::fromLatin1(kSaveFailedReason));
        emit dsrChanged();
        return;
    }
    ensureStoreOpen();
    RecordingEntry entry = pending.entry;
    const QString base = recordingBaseName(entry.startUtc, entry.frequencyHz, entry.mode, entry.call);
    QString finalPath = pending.workPath;
    const QString wanted = m_store->pathForNew(base, pending.suffix);
    if (QFile::rename(pending.workPath, wanted)) {
        finalPath = wanted;
    } else {
        qCWarning(lcRecordings) << "saved stretch" << pending.workPath << "kept under its working name:"
                                << "it cannot be renamed to" << wanted;
    }
    entry.lengthMs = lengthMs > 0 ? lengthMs : pending.lengthMs;
    entry.name = QFileInfo(finalPath).completeBaseName();
    entry.fileName = QFileInfo(finalPath).fileName();
    const QString id = m_store->add(entry);
    emit stretchSaved(id);
    emit dsrChanged();
}

bool RecordingService::openStretch(const QString& recordingId, QString* refusal)
{
    const auto refuse = [refusal](const char* why) {
        if (refusal != nullptr) {
            *refusal = QString::fromLatin1(why);
        }
        return false;
    };
    ensureStoreOpen();
    const std::optional<RecordingEntry> entry = m_store->find(recordingId);
    if (!entry) {
        return refuse(kPlayGoneReason);
    }
    if (entry->kind != RecordingKind::WholePan) {
        return refuse(kSaveNotWholePanReason);
    }
    if (!m_rewindHost || !m_rewindHost->sliceOpen(kStretchSlice)) {
        return refuse(kRewindNoSliceReason);
    }
    auto ring = std::make_shared<DsrRingFile>();
    QString error;
    if (!ring->open(QDir(m_store->folder()).filePath(entry->fileName), DsrStretchCopy::kNeverWrapBytes, &error)
        || ring->blockCount() == 0) {
        qCWarning(lcRecordings) << "saved stretch" << entry->fileName << "cannot be opened:" << error;
        return refuse(kPlayFailedReason);
    }
    // Another stretch slice A plays goes first.
    if (!m_stretch.id.isEmpty() && m_stretch.id != recordingId) {
        closeStretch();
    }
    const OpenStretch before = m_stretch;
    m_stretch.id = recordingId;
    m_stretch.ring = ring;
    m_stretch.fromUtcNs = ring->oldestUtcNs();
    m_stretch.toUtcNs = ring->newestUtcNs();
    // From its start, at the frequency slice A was on when it was saved (or
    // the kept centre when that is not in it).
    qint64 hz = entry->frequencyHz;
    DsrBlockHeader first;
    if (DsrStretch::receiverFor(*ring, m_stretch.fromUtcNs, m_stretch.toUtcNs, hz, -1, &first) < 0) {
        if (!ring->nextBlock(ring->receivers().first(), m_stretch.fromUtcNs, &first)) {
            m_stretch = before;
            return refuse(kPlayFailedReason);
        }
        hz = first.centreHz;
    }
    QString why;
    if (!rewind(kStretchSlice, m_stretch.fromUtcNs, hz, &why)) {
        m_stretch = before;
        if (refusal != nullptr) {
            *refusal = why;
        }
        return false;
    }
    emit dsrChanged();
    return true;
}

}  // namespace NereusSDR
