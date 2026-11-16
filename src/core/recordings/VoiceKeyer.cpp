// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original voice keyer; no logic is ported.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; the voice keyer on the Core (spec 3, 6.1,
//             6.6, V3, section 7, A8, A9). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; review fixes: ruling 8.5 (only the
//             sender's controls stop its message), the wait ends when the
//             radio drops, imports convert on the recordings worker a few
//             chunks a turn, a link import's file is numbered and goes with
//             its session. AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; a message on the air is never cleared,
//             imported into or replaced; leftover import files go on load.
//             AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; fix round 2: a waiting repeat ends with
//             its sender's session and never keys for a sender with none;
//             Repeat off from another device is refused under a message;
//             the press that takes the air is told (handedToMicrophone); a
//             failed replace keeps the old message. AI tooling: Anthropic
//             Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; final review fix: a message is refused
//             while the transmit slice is in CW (a CW key never takes it),
//             and one going out ends and unkeys if CW keys under it. AI
//             tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; JJ's ruling on PR 797: the imported mark,
//             kept in voice-keyer.json beside the names, and an imported
//             message sent at the fixed level. AI tooling: Anthropic Claude
//             Code.

#include "VoiceKeyer.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/Resampler.h"
#include "core/recordings/AudioFileReader.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/OnAirSource.h"
#include "core/recordings/RecordingService.h"
#include "core/safety/TransmitHolder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QtEndian>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace NereusSDR {

namespace {

const QString kRepeatKey = QStringLiteral("VoiceKeyerRepeat");
const QString kRepeatDelayKey = QStringLiteral("VoiceKeyerRepeatDelaySec");
const QString kNamesFile = QStringLiteral("voice-keyer.json");
const QString kTrue = QStringLiteral("True");
const QString kFalse = QStringLiteral("False");

// This object's own requests, told apart from every other MOX request by
// their tag (MoxController::requestAccepted carries it back).
constexpr quint64 kRequestTagBase = 0x564B000000000000ULL;   // "VK"

// Frames read, folded and resampled at a time when a file is converted.
constexpr int kConvertChunkFrames = 4096;
// Chunks a conversion takes per turn of the recordings worker: about a third
// of a second of 48 kHz audio, so the worker's 20 ms drain of the recorders'
// rings (2.7 s deep, ContactRecorder.h) and the players still runs between
// turns.
constexpr int kConvertChunksPerTurn = 4;
// Room for r8brain's uneven output per call (as RecordingPlayer allows).
constexpr int kResamplerSlackFrames = 4096;

QString text(const char* words)
{
    return QString::fromLatin1(words);
}

void setRefusal(QString* refusal, const QString& words)
{
    if (refusal != nullptr) {
        *refusal = words;
    }
}

bool validSlot(int n)
{
    return n >= 1 && n <= VoiceKeyer::kVoiceKeyerSlots;
}

size_t indexOf(int n)
{
    return static_cast<size_t>(n - 1);
}

qint64 lengthMsOf(const QString& path)
{
    QString error;
    const std::unique_ptr<AudioFileReader> reader = AudioFileReader::open(path, &error);
    if (!reader || reader->sampleRateHz() <= 0) {
        return -1;
    }
    return reader->lengthFrames() * 1000 / reader->sampleRateHz();
}

// A RIFF WAVE file whose format is PCM, one channel, 48 kHz, 16-bit: what
// a message is kept as (V3), so it is kept as it came.
bool isPcm16Mono48k(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray head = f.read(12);
    if (head.size() < 12 || !head.startsWith("RIFF") || head.mid(8, 4) != "WAVE") {
        return false;
    }
    while (!f.atEnd()) {
        const QByteArray chunk = f.read(8);
        if (chunk.size() < 8) {
            return false;
        }
        const quint32 size = qFromLittleEndian<quint32>(chunk.constData() + 4);
        if (chunk.startsWith("fmt ")) {
            const QByteArray fmt = f.read(std::min<qint64>(size, 64));
            if (fmt.size() < 16) {
                return false;
            }
            const quint16 format = qFromLittleEndian<quint16>(fmt.constData());
            const quint16 channels = qFromLittleEndian<quint16>(fmt.constData() + 2);
            const quint32 rate = qFromLittleEndian<quint32>(fmt.constData() + 4);
            const quint16 bits = qFromLittleEndian<quint16>(fmt.constData() + 14);
            return format == 1 && channels == 1 && rate == VoiceKeyer::kSampleRateHz && bits == 16;
        }
        if (!f.seek(f.pos() + size + (size & 1u))) {
            return false;
        }
    }
    return false;
}

}  // namespace

// `from` (WAV or MP3, any rate, any channels) as Mn's file `to`: 48 kHz mono
// 16-bit WAV, at most two minutes. Worker thread only, but `cancelled`, set
// on the main thread; the results are read on the main thread after the
// end is posted there.
struct VoiceKeyer::Conversion {
    enum class Step { More, Done, Failed };

    quint64 id = 0;
    QString from;
    QString to;
    QString folder;
    std::atomic<bool> cancelled{false};

    // Results.
    qint64 lengthMs = 0;
    QString refusal;

    // Worker thread state.
    bool opened = false;
    std::unique_ptr<AudioFileReader> reader;
    std::unique_ptr<AudioFileWriter> writer;
    std::unique_ptr<Resampler> resampler;
    int rate = 0;
    int channels = 0;
    int outCapacity = kConvertChunkFrames;
    std::vector<float> read;
    std::vector<float> mono;
    std::vector<float> out;
    qint64 totalIn = 0;
    qint64 written = 0;
    // The resampled length the input makes; what the resampler still holds
    // at the end comes out behind zeros, up to it.
    qint64 expected = std::numeric_limits<qint64>::max();

    Step fail(const char* reason)
    {
        writer.reset();
        reader.reset();
        QFile::remove(to);
        refusal = text(reason);
        return Step::Failed;
    }

    // Cancelled: nothing is kept.
    void discard()
    {
        writer.reset();
        reader.reset();
        QFile::remove(to);
    }

    bool put(const float* samples, int frames)
    {
        const int n = static_cast<int>(std::min<qint64>(frames, std::max<qint64>(0, expected - written)));
        if (n <= 0) {
            return true;
        }
        written += n;
        return writer->write(samples, n);
    }

    Step open()
    {
        opened = true;
        QString error;
        reader = AudioFileReader::open(from, &error);
        if (!reader || reader->sampleRateHz() <= 0 || reader->channels() < 1) {
            qCWarning(lcRecordings) << "a voice message import cannot be read:" << from << error;
            return fail(VoiceKeyer::kUnreadableReason);
        }
        rate = reader->sampleRateHz();
        channels = reader->channels();
        const qint64 known = reader->lengthFrames();
        if (known > 0 && known * 1000 / rate > VoiceKeyer::kVoiceClipMaxMs) {
            return fail(VoiceKeyer::kTooLongReason);
        }
        QFile::remove(to);
        if (known > 0 && isPcm16Mono48k(from)) {
            reader.reset();
            if (!QFile::copy(from, to)) {
                qCWarning(lcRecordings) << "cannot keep a voice message at" << to;
                return fail(VoiceKeyer::kCannotKeepReason);
            }
            lengthMs = known * 1000 / VoiceKeyer::kSampleRateHz;
            return Step::Done;
        }
        writer = AudioFileWriter::open(
            to, AudioFileSpec{AudioFileFormat::Wav, VoiceKeyer::kSampleRateHz, 1, 0}, &error);
        if (!writer) {
            qCWarning(lcRecordings) << "cannot keep a voice message at" << to << error;
            return fail(VoiceKeyer::kCannotKeepReason);
        }
        if (rate != VoiceKeyer::kSampleRateHz) {
            resampler = std::make_unique<Resampler>(static_cast<double>(rate),
                                                    static_cast<double>(VoiceKeyer::kSampleRateHz),
                                                    kConvertChunkFrames);
            outCapacity = static_cast<int>(std::ceil(static_cast<double>(kConvertChunkFrames)
                                                     * VoiceKeyer::kSampleRateHz / rate))
                + kResamplerSlackFrames;
        }
        read.resize(static_cast<size_t>(kConvertChunkFrames) * static_cast<size_t>(channels));
        mono.resize(static_cast<size_t>(kConvertChunkFrames));
        out.resize(static_cast<size_t>(outCapacity));
        return Step::More;
    }

    // Up to kConvertChunksPerTurn chunks; the end once the input is used up.
    Step step()
    {
        for (int chunk = 0; chunk < kConvertChunksPerTurn; ++chunk) {
            const int n = reader->read(read.data(), kConvertChunkFrames);
            if (n <= 0) {
                return finish();
            }
            totalIn += n;
            if (totalIn * 1000 / rate > VoiceKeyer::kVoiceClipMaxMs) {
                return fail(VoiceKeyer::kTooLongReason);
            }
            if (channels == 1) {
                std::copy(read.begin(), read.begin() + n, mono.begin());
            } else {
                // The channels averaged, as the player folds a stereo recording.
                const float scale = 1.0f / static_cast<float>(channels);
                for (int i = 0; i < n; ++i) {
                    float sum = 0.0f;
                    for (int c = 0; c < channels; ++c) {
                        sum += read[static_cast<size_t>(i * channels + c)];
                    }
                    mono[static_cast<size_t>(i)] = sum * scale;
                }
            }
            bool ok = true;
            if (resampler) {
                const int made = resampler->processInto(mono.data(), n, out.data(), outCapacity);
                ok = put(out.data(), made);
            } else {
                ok = put(mono.data(), n);
            }
            if (!ok) {
                return fail(VoiceKeyer::kCannotKeepReason);
            }
        }
        return Step::More;
    }

    Step finish()
    {
        if (totalIn == 0) {
            return fail(VoiceKeyer::kUnreadableReason);
        }
        expected = totalIn * VoiceKeyer::kSampleRateHz / rate;
        if (resampler) {
            int flushLeft = resampler->latencyInputSamples() + kConvertChunkFrames;
            std::fill(mono.begin(), mono.end(), 0.0f);
            while (written < expected && flushLeft > 0) {
                const int k = std::min(kConvertChunkFrames, flushLeft);
                flushLeft -= k;
                const int made = resampler->processInto(mono.data(), k, out.data(), outCapacity);
                if (!put(out.data(), made)) {
                    return fail(VoiceKeyer::kCannotKeepReason);
                }
            }
        }
        if (!writer->finish()) {
            return fail(VoiceKeyer::kCannotKeepReason);
        }
        writer.reset();
        reader.reset();
        lengthMs = written * 1000 / VoiceKeyer::kSampleRateHz;
        return Step::Done;
    }
};

VoiceKeyer::VoiceKeyer(RecordingService& service, MoxController* mox, OnAirSource* onAir, QObject* parent)
    : QObject(parent)
    , m_service(service)
    , m_mox(mox)
    , m_onAir(onAir)
    , m_folder(defaultFolder())
    , m_clock([] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    })
{
    const AppSettings& s = AppSettings::instance();
    m_repeat = s.value(kRepeatKey, kFalse).toString() == kTrue;
    bool ok = false;
    const int delay = s.value(kRepeatDelayKey, kRepeatDelayDefaultSec).toInt(&ok);
    m_repeatDelaySec = ok ? std::clamp(delay, kRepeatDelayMinSec, kRepeatDelayMaxSec) : kRepeatDelayDefaultSec;

    m_timer = new QTimer(this);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &VoiceKeyer::tick);

    if (m_mox) {
        connect(m_mox, &MoxController::requestAccepted, this,
                [this](const KeyerIdentity& requester, quint64, bool requestedOn) {
                    onRequestAccepted(requester, requestedOn);
                });
        connect(m_mox, &MoxController::moxChanged, this, [this](int, bool, bool on) { onMoxChanged(on); });
    }
}

VoiceKeyer::~VoiceKeyer()
{
    stop();
    for (auto it = m_imports.begin(); it != m_imports.end(); ++it) {
        if (it->file) {
            it->file->remove();
        }
    }
    // A conversion under way stops at its next turn and keeps nothing. Once
    // the worker has run what was queued before this, no turn posts back
    // here.
    for (const std::shared_ptr<Conversion>& c : m_conversions) {
        c->cancelled.store(true, std::memory_order_release);
    }
    if (!m_conversions.empty()) {
        m_service.waitForWorker();
    }
}

QString VoiceKeyer::defaultFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("Voice keyer"));
}

void VoiceKeyer::setFolder(const QString& folder)
{
    if (folder == m_folder) {
        return;
    }
    stop();
    m_folder = folder;
    m_loaded = false;
    emit changed();
}

void VoiceKeyer::setTransmitHolder(TransmitHolder* holder)
{
    m_holder = holder;
}

void VoiceKeyer::setSenderAlive(SenderAlive alive)
{
    m_senderAlive = std::move(alive);
}

void VoiceKeyer::setClockForTest(Clock clock)
{
    if (clock) {
        m_clock = std::move(clock);
    }
}

qint64 VoiceKeyer::now() const
{
    return m_clock();
}

QString VoiceKeyer::pathFor(int n) const
{
    return QDir(m_folder).filePath(QStringLiteral("M%1.wav").arg(n));
}

QString VoiceKeyer::emptyReason(int n)
{
    return QStringLiteral("M%1 is empty.").arg(n);
}

QString VoiceKeyer::onAirReason(int n)
{
    return QStringLiteral("M%1 is on the air.").arg(n);
}

bool VoiceKeyer::isOnAir(int n) const
{
    return m_phase != Phase::Idle && m_n == n;
}

void VoiceKeyer::ensureLoaded() const
{
    if (m_loaded) {
        return;
    }
    m_loaded = true;
    m_names.fill(QString());
    m_imported.fill(false);
    QFile index(QDir(m_folder).filePath(kNamesFile));
    if (index.open(QIODevice::ReadOnly)) {
        const QJsonObject kept = QJsonDocument::fromJson(index.readAll()).object();
        const QJsonArray names = kept.value(QStringLiteral("names")).toArray();
        for (int i = 0; i < kVoiceKeyerSlots && i < names.size(); ++i) {
            m_names[static_cast<size_t>(i)] = names.at(i).toString().left(kNameMaxChars);
        }
        // Which messages came from an imported file (JJ, 2026-11-16); a
        // file kept before the mark has none, so all are mic messages.
        const QJsonArray imported = kept.value(QStringLiteral("imported")).toArray();
        for (int i = 0; i < kVoiceKeyerSlots && i < imported.size(); ++i) {
            m_imported[static_cast<size_t>(i)] = imported.at(i).toBool(false);
        }
    }
    // A Core that shut down in the middle of an import leaves its file
    // behind: nothing will finish it. One under way now is kept.
    QSet<QString> inUse;
    for (const Import& import : m_imports) {
        if (import.file) {
            inUse.insert(QFileInfo(import.file->fileName()).absoluteFilePath());
        }
    }
    for (const auto& c : m_conversions) {
        inUse.insert(QFileInfo(c->from).absoluteFilePath());
        inUse.insert(QFileInfo(c->to).absoluteFilePath());
    }
    const QDir dir(m_folder);
    const QStringList leftovers =
        dir.entryList({QStringLiteral("import-*.part"), QStringLiteral("M*-*.part")}, QDir::Files);
    for (const QString& name : leftovers) {
        const QString path = dir.absoluteFilePath(name);
        if (!inUse.contains(path) && !QFile::remove(path)) {
            qCWarning(lcRecordings) << "a leftover voice message import cannot be removed:" << path;
        }
    }
    for (int n = 1; n <= kVoiceKeyerSlots; ++n) {
        const QString path = pathFor(n);
        const qint64 length = QFileInfo::exists(path) ? lengthMsOf(path) : -1;
        m_filled[indexOf(n)] = length >= 0;
        m_lengths[indexOf(n)] = std::max<qint64>(0, length);
        m_imported[indexOf(n)] = m_imported[indexOf(n)] && m_filled[indexOf(n)];
    }
}

void VoiceKeyer::saveNames() const
{
    QDir().mkpath(m_folder);
    QJsonArray names;
    for (const QString& name : m_names) {
        names.append(name);
    }
    QJsonArray imported;
    for (const bool mark : m_imported) {
        imported.append(mark);
    }
    QSaveFile file(QDir(m_folder).filePath(kNamesFile));
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(QJsonObject{{QStringLiteral("names"), names},
                                                {QStringLiteral("imported"), imported}})
                          .toJson())
               < 0
        || !file.commit()) {
        qCWarning(lcRecordings) << "the voice message names cannot be kept in" << m_folder
                                << file.errorString();
    }
}

QList<VoiceKeyer::Message> VoiceKeyer::messages() const
{
    ensureLoaded();
    QList<Message> list;
    for (int n = 1; n <= kVoiceKeyerSlots; ++n) {
        list.append(Message{n, m_names[indexOf(n)], m_lengths[indexOf(n)], !m_filled[indexOf(n)],
                            m_imported[indexOf(n)]});
    }
    return list;
}

int VoiceKeyer::firstEmpty() const
{
    ensureLoaded();
    for (int n = 1; n <= kVoiceKeyerSlots; ++n) {
        if (!m_filled[indexOf(n)]) {
            return n;
        }
    }
    return 0;
}

QString VoiceKeyer::messagePath(int n) const
{
    ensureLoaded();
    return validSlot(n) && m_filled[indexOf(n)] ? pathFor(n) : QString();
}

bool VoiceKeyer::isImported(int n) const
{
    ensureLoaded();
    return validSlot(n) && m_filled[indexOf(n)] && m_imported[indexOf(n)];
}

std::optional<VoiceKeyer::Sending> VoiceKeyer::sending() const
{
    if (m_phase == Phase::Idle) {
        return std::nullopt;
    }
    Sending s;
    s.n = m_n;
    s.by = m_who.name;
    s.deviceId = m_who.wireId;
    s.lengthMs = m_samples ? static_cast<qint64>(m_samples->size()) * 1000 / kSampleRateHz : 0;
    if (m_phase == Phase::Sending && m_clip) {
        s.positionMs = std::min(s.lengthMs, m_clip->position.load(std::memory_order_acquire) * 1000
                                                / kSampleRateHz);
    } else {
        s.positionMs = s.lengthMs;
        s.waitingMs = std::max<qint64>(0, m_waitUntilMs - now());
    }
    return s;
}

QJsonObject VoiceKeyer::documentJson() const
{
    QJsonArray slotList;
    for (const Message& m : messages()) {
        slotList.append(QJsonObject{{QStringLiteral("n"), m.n},
                                    {QStringLiteral("name"), m.name},
                                    {QStringLiteral("lengthMs"), m.lengthMs},
                                    {QStringLiteral("empty"), m.empty}});
    }
    QJsonValue sendingValue = QJsonValue::Null;
    if (const std::optional<Sending> s = sending()) {
        sendingValue = QJsonObject{{QStringLiteral("n"), s->n},
                                   {QStringLiteral("positionMs"), s->positionMs},
                                   {QStringLiteral("lengthMs"), s->lengthMs},
                                   {QStringLiteral("waitingMs"), s->waitingMs},
                                   {QStringLiteral("by"), s->by},
                                   {QStringLiteral("deviceId"), s->deviceId}};
    }
    return QJsonObject{{QStringLiteral("slots"), slotList},
                       {QStringLiteral("repeat"), m_repeat},
                       {QStringLiteral("repeatDelaySec"), m_repeatDelaySec},
                       {QStringLiteral("sending"), sendingValue}};
}

// ---- Keying.

TxRefusal VoiceKeyer::holderRefusal(const KeyerIdentity& keyer) const
{
    // A question only: nothing changes (TransmitHolder::keyRefusalFor).
    return m_holder ? m_holder->keyRefusalFor(keyer.deviceId, false) : TxRefusal{};
}

std::shared_ptr<const std::vector<float>> VoiceKeyer::loadSamples(int n, QString* refusal) const
{
    QString error;
    const std::unique_ptr<AudioFileReader> reader = AudioFileReader::open(pathFor(n), &error);
    if (!reader || reader->sampleRateHz() != kSampleRateHz || reader->channels() != 1) {
        qCWarning(lcRecordings) << "voice message" << n << "cannot be read:" << error;
        setRefusal(refusal, QStringLiteral("M%1 cannot be read on the Core.").arg(n));
        return nullptr;
    }
    auto samples = std::make_shared<std::vector<float>>();
    samples->reserve(static_cast<size_t>(std::max<qint64>(0, reader->lengthFrames())));
    std::vector<float> chunk(static_cast<size_t>(kConvertChunkFrames));
    for (;;) {
        const int got = reader->read(chunk.data(), kConvertChunkFrames);
        if (got <= 0) {
            break;
        }
        samples->insert(samples->end(), chunk.begin(), chunk.begin() + got);
    }
    return samples;
}

bool VoiceKeyer::send(int n, const Requester& who, QString* refusal)
{
    if (!validSlot(n)) {
        setRefusal(refusal, text(kSlotRangeReason));
        return false;
    }
    ensureLoaded();
    if (!m_filled[indexOf(n)]) {
        setRefusal(refusal, emptyReason(n));
        return false;
    }
    if (!m_mox) {
        setRefusal(refusal, text(kNoRadioReason));
        return false;
    }
    // In CW a key walks without the TX channel, so the message would never
    // be taken and the radio would stay keyed: nothing keys.
    if (m_mox->isCwModeActive()) {
        setRefusal(refusal, text(kCwModeReason));
        return false;
    }
    // The same device's next message while one goes out: the air moves to
    // it, keyed as it is.
    if (m_phase == Phase::Sending && m_keyer.deviceId == who.keyer.deviceId
        && m_keyer.session == who.keyer.session && m_mox->isMox()
        && m_mox->currentKeyer().deviceId == m_keyer.deviceId) {
        std::shared_ptr<const std::vector<float>> samples = loadSamples(n, refusal);
        if (!samples) {
            return false;
        }
        cutClip();
        auto clip = std::make_shared<Clip>();
        clip->samples = samples;
        m_clip = clip;
        m_samples = samples;
        m_n = n;
        // No block between the two messages is left without one. An
        // imported message at the fixed level, a mic one at the mic gain.
        m_onAir->beginVoiceMessage(QStringLiteral("M%1").arg(n), pullFor(clip), m_imported[indexOf(n)]);
        emit changed();
        return true;
    }
    // Section 7: another station has transmit. Nothing keys, and what
    // goes out goes on.
    if (const TxRefusal held = holderRefusal(who.keyer); !held.isEmpty()) {
        setRefusal(refusal, held.text);
        return false;
    }
    // Another device's message on the air, or waiting to repeat, is left
    // alone (ruling 8.5).
    if (m_phase != Phase::Idle && m_keyer.deviceId != who.keyer.deviceId) {
        setRefusal(refusal, text(kTransmittingReason));
        return false;
    }
    if (m_phase != Phase::Idle) {
        stop();
    }
    if (m_mox->isMox()) {
        setRefusal(refusal, text(kTransmittingReason));
        return false;
    }
    std::shared_ptr<const std::vector<float>> samples = loadSamples(n, refusal);
    if (!samples) {
        return false;
    }
    return startSend(n, who, std::move(samples), refusal);
}

std::function<int(float*, int)> VoiceKeyer::pullFor(std::shared_ptr<Clip> clip)
{
    return [clip](float* out, int frames) {
        // The transmit thread: no lock, no wait, no allocation. Once the
        // message is over, or cut, silence until the radio unkeys: the air
        // never falls back to a microphone by itself.
        if (clip->stopped.load(std::memory_order_acquire)) {
            std::fill(out, out + frames, 0.0f);
            return frames;
        }
        const std::vector<float>& s = *clip->samples;
        const qint64 pos = clip->position.load(std::memory_order_relaxed);
        const qint64 left = static_cast<qint64>(s.size()) - pos;
        const int k = static_cast<int>(std::clamp<qint64>(left, 0, frames));
        std::copy(s.begin() + pos, s.begin() + pos + k, out);
        std::fill(out + k, out + frames, 0.0f);
        clip->position.store(pos + k, std::memory_order_release);
        if (k < frames) {
            clip->ended.store(true, std::memory_order_release);
        }
        return frames;
    };
}

bool VoiceKeyer::startSend(int n, const Requester& who, std::shared_ptr<const std::vector<float>> samples,
                           QString* refusal)
{
    auto clip = std::make_shared<Clip>();
    clip->samples = samples;
    // On the air before the key, so the first block the transmitter takes
    // is the message's and no playback is sent in its place. An imported
    // message goes out at the fixed recording level (JJ, 2026-11-16).
    m_onAir->beginVoiceMessage(QStringLiteral("M%1").arg(n), pullFor(clip), m_imported[indexOf(n)]);

    KeyerIdentity keyer = who.keyer;
    keyer.requestTag = kRequestTagBase + (++m_nextTag);
    QString refused;
    const QMetaObject::Connection capture =
        connect(m_mox, &MoxController::moxRefused, this, [&refused](const TxRefusal& r) {
            if (refused.isEmpty()) {
                refused = r.text;
            }
        });
    m_keying = true;
    m_mox->setMox(true, keyer);
    m_keying = false;
    disconnect(capture);
    if (!m_mox || !m_mox->isMox() || m_mox->currentKeyer().deviceId != keyer.deviceId) {
        clip->stopped.store(true, std::memory_order_release);
        if (m_onAir->active() == OnAirSource::Kind::VoiceMessage) {
            m_onAir->end();
        }
        if (refused.isEmpty() && m_mox) {
            refused = m_mox->lastRefusal().text;
        }
        setRefusal(refusal, refused.isEmpty() ? text(kNotKeyedReason) : refused);
        const bool wasActive = m_phase != Phase::Idle;
        m_phase = Phase::Idle;
        m_clip.reset();
        updateTimer();
        if (wasActive) {
            emit changed();
        }
        return false;
    }
    m_phase = Phase::Sending;
    m_n = n;
    m_who = who;
    m_keyer = keyer;
    m_clip = clip;
    m_samples = std::move(samples);
    updateTimer();
    qCInfo(lcRecordings) << "voice keyer: M" << n << "keyed for" << m_who.name;
    emit changed();
    return true;
}

void VoiceKeyer::cutClip()
{
    if (m_clip) {
        m_clip->stopped.store(true, std::memory_order_release);
    }
}

void VoiceKeyer::endOnAirIfOurs()
{
    if (m_onAir->active() == OnAirSource::Kind::VoiceMessage) {
        m_onAir->end();
    }
}

void VoiceKeyer::becomeIdle()
{
    m_phase = Phase::Idle;
    m_clip.reset();
    updateTimer();
    emit changed();
}

void VoiceKeyer::unkeyIfOurs(const KeyerIdentity& keyer)
{
    if (m_mox && m_mox->isMox() && m_mox->currentKeyer().deviceId == keyer.deviceId) {
        m_mox->setMox(false, keyer);
    }
}

void VoiceKeyer::stop()
{
    if (m_phase == Phase::Idle) {
        return;
    }
    const bool wasSending = m_phase == Phase::Sending;
    const KeyerIdentity keyer = m_keyer;
    // Silence from the next block, then the unkey.
    cutClip();
    m_phase = Phase::Idle;
    if (wasSending) {
        unkeyIfOurs(keyer);
        endOnAirIfOurs();
    }
    m_clip.reset();
    updateTimer();
    emit changed();
}

void VoiceKeyer::handOverToPress()
{
    // The press keeps the radio keyed: its microphone goes out from the
    // next block.
    const QByteArray deviceId = m_keyer.deviceId;
    cutClip();
    endOnAirIfOurs();
    becomeIdle();
    // The microphone line was not needed under the message; the station
    // server judges it again now that it is (a starved line is held).
    if (m_mox && m_mox->isMox() && m_mox->currentKeyer().deviceId == deviceId) {
        emit handedToMicrophone(deviceId);
    }
}

bool VoiceKeyer::mayEnd(const KeyerIdentity& who, TxRefusal* refusal) const
{
    // Ruling 8.5: the sender ends its own message; another device only
    // by taking transmit (which unkeys it, onMoxChanged), after which it
    // holds transmit and may end a wait too.
    const bool sender = who.deviceId == m_keyer.deviceId;
    const bool holder = m_holder && m_holder->isHeldBy(who.deviceId);
    if (sender || holder) {
        return true;
    }
    if (refusal != nullptr) {
        const std::optional<TransmitHolder::Holder> held = m_holder ? m_holder->holder() : std::nullopt;
        *refusal = TxRefusals::otherDeviceHoldsStop(held.has_value() ? held->name : m_who.name);
    }
    return false;
}

bool VoiceKeyer::stopFrom(const KeyerIdentity& who, TxRefusal* refusal)
{
    if (m_phase == Phase::Idle) {
        return true;
    }
    if (!mayEnd(who, refusal)) {
        return false;
    }
    stop();
    return true;
}

void VoiceKeyer::sessionEnded(const QString& session)
{
    // A wait between repeats has MOX off, so no unkey or watchdog reaches
    // it: it ends with its sender's session. A message going out is cut by
    // the link's own unkey.
    if (m_phase == Phase::Waiting && !session.isEmpty() && !m_keyer.isStation()
        && m_keyer.session == session) {
        becomeIdle();
    }
}

void VoiceKeyer::keyAdmittedFor(const QByteArray& deviceId, const QString& session)
{
    // The key was admitted with the radio already keyed for the sender (a
    // copy or a refusal never reaches here): its microphone takes the air.
    if (m_phase == Phase::Sending && deviceId == m_keyer.deviceId && session == m_keyer.session) {
        handOverToPress();
    }
}

void VoiceKeyer::connectionLost()
{
    // A wait between repeats has MOX off, so the teardown's unkey never
    // reaches onMoxChanged: it ends here, and nothing keys on a reconnect.
    stop();
}

void VoiceKeyer::onRequestAccepted(const KeyerIdentity& requester, bool requestedOn)
{
    if (m_phase == Phase::Idle || m_keying) {
        return;
    }
    if (requester.requestTag != 0 && requester.requestTag == m_keyer.requestTag) {
        return;   // this object's own key or unkey
    }
    // VOX is audio, not a person (ruling 8.4).
    if (requester.source == PttMode::Vox) {
        return;
    }
    if (m_phase == Phase::Waiting) {
        // Unkeyed between repeats: a key MOX took (the radio is free, or
        // the press is the holder's) ends the repeat.
        becomeIdle();
        return;
    }
    if (requestedOn) {
        // While keyed, MOX accepts a press only from the keyed device (the
        // sender): its key stays on (PTT keeps the mic on the air). Another
        // device's press is refused (ruling 8.5) and cuts nothing.
        handOverToPress();
    } else {
        // A release: the radio unkeys by it.
        cutClip();
        becomeIdle();
    }
}

void VoiceKeyer::onMoxChanged(bool on)
{
    // MOX went off under a message going out (Stop All TX, the link
    // watchdog, a change of holder): it is over, with no repeat.
    if (!on && m_phase == Phase::Sending) {
        cutClip();
        becomeIdle();
    }
}

void VoiceKeyer::tick()
{
    if (m_phase == Phase::Sending) {
        // The transmit slice moved to CW, or MOX runs as a CW key: the
        // message is never taken in CW, so it ends and unkeys, with no
        // repeat.
        if (m_mox && (m_mox->isCwModeActive() || m_mox->isCwKeying())) {
            stop();
        } else if (m_clip && m_clip->ended.load(std::memory_order_acquire)) {
            endOfMessage();
        }
    } else if (m_phase == Phase::Waiting) {
        if (now() >= m_waitUntilMs) {
            resend();
        }
    }
}

void VoiceKeyer::endOfMessage()
{
    const KeyerIdentity keyer = m_keyer;
    cutClip();
    if (m_repeat) {
        m_phase = Phase::Waiting;
        m_waitUntilMs = now() + static_cast<qint64>(m_repeatDelaySec) * 1000;
    } else {
        m_phase = Phase::Idle;
    }
    unkeyIfOurs(keyer);
    endOnAirIfOurs();
    if (m_phase == Phase::Idle) {
        m_clip.reset();
    }
    updateTimer();
    emit changed();
}

void VoiceKeyer::resend()
{
    const int n = m_n;
    const Requester who = m_who;
    std::shared_ptr<const std::vector<float>> samples = m_samples;
    m_phase = Phase::Idle;
    QString refusal;
    if (!who.keyer.isStation() && m_senderAlive && !m_senderAlive(who.keyer)) {
        // Its sender's session is gone (an away holder is still admitted
        // by the holder's rule): nothing keys as it.
        refusal = TxRefusals::notReady().text;
    } else if (const TxRefusal held = holderRefusal(who.keyer); !held.isEmpty()) {
        refusal = held.text;
    } else if (!m_mox) {
        refusal = text(kNoRadioReason);
    } else if (m_mox->isCwModeActive()) {
        refusal = text(kCwModeReason);
    } else if (m_mox->isMox()) {
        refusal = text(kTransmittingReason);
    } else if (samples && startSend(n, who, samples, &refusal)) {
        return;
    }
    if (refusal.isEmpty()) {
        refusal = text(kNotKeyedReason);
    }
    m_clip.reset();
    updateTimer();
    emit changed();
    emit sendRefused(who.keyer.deviceId, refusal);
}

void VoiceKeyer::updateTimer()
{
    if (m_phase == Phase::Idle) {
        m_timer->stop();
    } else if (!m_timer->isActive()) {
        m_timer->start();
    }
}

// ---- The messages.

bool VoiceKeyer::rename(int n, const QString& name, QString* refusal)
{
    if (!validSlot(n)) {
        setRefusal(refusal, text(kSlotRangeReason));
        return false;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.size() > kNameMaxChars) {
        setRefusal(refusal, QStringLiteral("A message name can be up to %1 characters.").arg(kNameMaxChars));
        return false;
    }
    ensureLoaded();
    if (m_names[indexOf(n)] == trimmed) {
        return true;
    }
    m_names[indexOf(n)] = trimmed;
    saveNames();
    emit changed();
    return true;
}

bool VoiceKeyer::clear(int n, QString* refusal)
{
    if (!validSlot(n)) {
        setRefusal(refusal, text(kSlotRangeReason));
        return false;
    }
    ensureLoaded();
    // A message going out is stopped with Stop, never by clearing it.
    if (isOnAir(n)) {
        setRefusal(refusal, onAirReason(n));
        return false;
    }
    const QString path = pathFor(n);
    if (QFileInfo::exists(path) && !QFile::remove(path)) {
        qCWarning(lcRecordings) << "voice message" << n << "cannot be cleared at" << path;
        setRefusal(refusal, QStringLiteral("M%1 cannot be cleared on the Core.").arg(n));
        return false;
    }
    const bool hadName = !m_names[indexOf(n)].isEmpty();
    const bool hadMark = m_imported[indexOf(n)];
    const bool hadMessage = m_filled[indexOf(n)];
    m_filled[indexOf(n)] = false;
    m_lengths[indexOf(n)] = 0;
    m_names[indexOf(n)].clear();
    m_imported[indexOf(n)] = false;
    if (hadName || hadMark) {
        saveNames();
    }
    if (hadName || hadMessage) {
        emit changed();
    }
    return true;
}

bool VoiceKeyer::setRepeatFrom(const KeyerIdentity& who, bool on, int delaySec, QString* refusal,
                               TxRefusal* held)
{
    // Turning Repeat off under a message ends its repeats: as Stop, the
    // sender's or the holder's only (ruling 8.5).
    if (!on && m_repeat && m_phase != Phase::Idle && delaySec >= kRepeatDelayMinSec
        && delaySec <= kRepeatDelayMaxSec) {
        TxRefusal refused;
        if (!mayEnd(who, &refused)) {
            setRefusal(refusal, refused.text);
            if (held != nullptr) {
                *held = refused;
            }
            return false;
        }
    }
    return setRepeat(on, delaySec, refusal);
}

bool VoiceKeyer::setRepeat(bool on, int delaySec, QString* refusal)
{
    if (delaySec < kRepeatDelayMinSec || delaySec > kRepeatDelayMaxSec) {
        setRefusal(refusal, QStringLiteral("The wait between repeats can be %1 to %2 seconds.")
                                .arg(kRepeatDelayMinSec)
                                .arg(kRepeatDelayMaxSec));
        return false;
    }
    if (on == m_repeat && delaySec == m_repeatDelaySec) {
        return true;
    }
    m_repeat = on;
    m_repeatDelaySec = delaySec;
    AppSettings& s = AppSettings::instance();
    s.setValue(kRepeatKey, on ? kTrue : kFalse);
    s.setValue(kRepeatDelayKey, delaySec);
    // Repeat turned off between repeats: the message is over.
    if (!on && m_phase == Phase::Waiting) {
        m_phase = Phase::Idle;
        m_clip.reset();
        updateTimer();
    }
    emit changed();
    return true;
}

void VoiceKeyer::importFile(int n, const QString& path, const QString& name, ImportDone done, bool imported)
{
    const auto refuse = [&done](const QString& reason) {
        if (done) {
            done(false, reason);
        }
    };
    if (!validSlot(n)) {
        refuse(text(kSlotRangeReason));
        return;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.size() > kNameMaxChars) {
        refuse(QStringLiteral("A message name can be up to %1 characters.").arg(kNameMaxChars));
        return;
    }
    if (isOnAir(n)) {
        refuse(onAirReason(n));
        return;
    }
    ensureLoaded();
    if (!QDir().mkpath(m_folder)) {
        qCWarning(lcRecordings) << "the voice keyer folder cannot be made:" << m_folder;
        refuse(text(kCannotKeepReason));
        return;
    }
    auto c = std::make_shared<Conversion>();
    c->id = ++m_nextImportId;
    c->from = path;
    c->folder = m_folder;
    // Its own file, so two imports never write over each other.
    c->to = QDir(m_folder).filePath(QStringLiteral("M%1-%2.part").arg(n).arg(c->id));
    m_conversions.push_back(c);
    m_pendingImports.insert(c->id, PendingImport{n, trimmed, std::move(done), imported});
    RecordingService* const service = &m_service;
    m_service.runOnWorker([service, owner = this, c] { stepConversion(service, owner, c); });
}

void VoiceKeyer::stepConversion(RecordingService* service, VoiceKeyer* owner, std::shared_ptr<Conversion> c)
{
    // The recordings worker thread. Cancelled (the keyer is going): nothing
    // is kept, and `owner` is not touched.
    if (c->cancelled.load(std::memory_order_acquire)) {
        c->discard();
        return;
    }
    const Conversion::Step step = c->opened ? c->step() : c->open();
    if (step == Conversion::Step::More) {
        // The next turn after what else the worker has to do.
        service->runOnWorker([service, owner, c] { stepConversion(service, owner, c); });
        return;
    }
    const bool ok = step == Conversion::Step::Done;
    QMetaObject::invokeMethod(owner, [owner, c, ok] { owner->finishConversion(c, ok); },
                              Qt::QueuedConnection);
}

void VoiceKeyer::finishConversion(const std::shared_ptr<Conversion>& c, bool ok)
{
    m_conversions.erase(std::remove(m_conversions.begin(), m_conversions.end(), c), m_conversions.end());
    const auto it = m_pendingImports.find(c->id);
    if (it == m_pendingImports.end()) {
        QFile::remove(c->to);
        return;
    }
    const PendingImport pending = std::move(it.value());
    m_pendingImports.erase(it);
    const auto finish = [&pending](bool kept, const QString& reason) {
        if (pending.done) {
            pending.done(kept, reason);
        }
    };
    if (!ok) {
        QFile::remove(c->to);
        finish(false, c->refusal.isEmpty() ? text(kCannotKeepReason) : c->refusal);
        return;
    }
    // The folder changed under it: it is not this folder's message.
    if (c->folder != m_folder) {
        QFile::remove(c->to);
        finish(false, text(kCannotKeepReason));
        return;
    }
    ensureLoaded();
    const int n = pending.n;
    // Sent while it converted: the message on the air is not replaced.
    if (isOnAir(n)) {
        QFile::remove(c->to);
        finish(false, onAirReason(n));
        return;
    }
    const QString final = pathFor(n);
    // The old message cannot be removed (on Windows, while it is being
    // listened to): it stays as it was and this import is refused.
    if (QFileInfo::exists(final) && !QFile::remove(final)) {
        qCWarning(lcRecordings) << "voice message" << n << "cannot be replaced at" << final;
        QFile::remove(c->to);
        finish(false, QStringLiteral("M%1 cannot be replaced while it is in use. Try again once it stops playing.")
                          .arg(n));
        return;
    }
    if (!QFile::rename(c->to, final)) {
        qCWarning(lcRecordings) << "cannot keep voice message" << n << "at" << final;
        QFile::remove(c->to);
        m_filled[indexOf(n)] = false;
        m_lengths[indexOf(n)] = 0;
        emit changed();
        finish(false, text(kCannotKeepReason));
        return;
    }
    m_filled[indexOf(n)] = true;
    m_lengths[indexOf(n)] = c->lengthMs;
    bool save = false;
    if (!pending.name.isEmpty() && pending.name != m_names[indexOf(n)]) {
        m_names[indexOf(n)] = pending.name;
        save = true;
    }
    // Importing over a mic message marks it imported; recording on the mic
    // over an imported one makes it a mic message again.
    if (m_imported[indexOf(n)] != pending.imported) {
        m_imported[indexOf(n)] = pending.imported;
        save = true;
    }
    if (save) {
        saveNames();
    }
    emit changed();
    finish(true, QString());
}

bool VoiceKeyer::listen(int n, const ListenerId& who, QString* refusal)
{
    if (!validSlot(n)) {
        setRefusal(refusal, text(kSlotRangeReason));
        return false;
    }
    ensureLoaded();
    if (!m_filled[indexOf(n)]) {
        setRefusal(refusal, emptyReason(n));
        return false;
    }
    m_service.playFile(who, QStringLiteral("voiceKeyer:M%1").arg(n), pathFor(n), m_lengths[indexOf(n)]);
    return true;
}

// ---- The station link's import and export.

bool VoiceKeyer::beginImport(const QString& owner, int n, const QString& name, qint64 size, QString* refusal,
                             bool imported)
{
    cancelImport(owner);
    if (!validSlot(n)) {
        setRefusal(refusal, text(kSlotRangeReason));
        return false;
    }
    if (name.trimmed().size() > kNameMaxChars) {
        setRefusal(refusal, QStringLiteral("A message name can be up to %1 characters.").arg(kNameMaxChars));
        return false;
    }
    if (isOnAir(n)) {
        setRefusal(refusal, onAirReason(n));
        return false;
    }
    if (size <= 0) {
        setRefusal(refusal, text(kUnreadableReason));
        return false;
    }
    if (size > kImportMaxBytes) {
        setRefusal(refusal, text(kTooLargeReason));
        return false;
    }
    // Its leftovers go before this import's file is made.
    ensureLoaded();
    if (!QDir().mkpath(m_folder)) {
        setRefusal(refusal, text(kCannotKeepReason));
        return false;
    }
    Import import;
    import.n = n;
    import.name = name;
    import.size = size;
    import.imported = imported;
    // Numbered per import: never another owner's file.
    import.file = std::make_unique<QFile>(
        QDir(m_folder).filePath(QStringLiteral("import-%1.part").arg(++m_nextImportId)));
    if (!import.file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcRecordings) << "cannot take a voice message import at" << import.file->fileName()
                                << import.file->errorString();
        setRefusal(refusal, text(kCannotKeepReason));
        return false;
    }
    m_imports.insert(owner, std::move(import));
    return true;
}

bool VoiceKeyer::importChunk(const QString& owner, const QByteArray& bytes, QString* refusal)
{
    const auto it = m_imports.find(owner);
    if (it == m_imports.end()) {
        setRefusal(refusal, text(kNoImportReason));
        return false;
    }
    if (bytes.isEmpty() || bytes.size() > kChunkBytes || it->received + bytes.size() > it->size) {
        cancelImport(owner);
        setRefusal(refusal, text(kNotWholeReason));
        return false;
    }
    if (it->file->write(bytes) != bytes.size()) {
        qCWarning(lcRecordings) << "cannot write a voice message import" << it->file->errorString();
        cancelImport(owner);
        setRefusal(refusal, text(kCannotKeepReason));
        return false;
    }
    it->received += bytes.size();
    return true;
}

void VoiceKeyer::finishImport(const QString& owner, ImportDone done)
{
    const auto it = m_imports.find(owner);
    if (it == m_imports.end()) {
        if (done) {
            done(false, text(kNoImportReason));
        }
        return;
    }
    if (it->received != it->size) {
        cancelImport(owner);
        if (done) {
            done(false, text(kNotWholeReason));
        }
        return;
    }
    // Out of the owner's hands: a new import of its starts afresh while
    // this one converts, and the file goes once the conversion ends.
    const Import import = it.value();
    m_imports.erase(it);
    import.file->close();
    const QString path = import.file->fileName();
    importFile(import.n, path, import.name, [path, done = std::move(done)](bool ok, const QString& reason) {
        QFile::remove(path);
        if (done) {
            done(ok, reason);
        }
    }, import.imported);
}

void VoiceKeyer::cancelImport(const QString& owner)
{
    const auto it = m_imports.find(owner);
    if (it == m_imports.end()) {
        return;
    }
    if (it->file) {
        it->file->close();
        it->file->remove();
    }
    m_imports.erase(it);
}

bool VoiceKeyer::exportChunk(int n, qint64 offset, QByteArray* chunk, qint64* size, QString* refusal)
{
    if (!validSlot(n)) {
        setRefusal(refusal, text(kSlotRangeReason));
        return false;
    }
    ensureLoaded();
    if (!m_filled[indexOf(n)]) {
        setRefusal(refusal, emptyReason(n));
        return false;
    }
    QFile file(pathFor(n));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcRecordings) << "cannot read voice message" << n << file.errorString();
        setRefusal(refusal, QStringLiteral("M%1 cannot be read on the Core.").arg(n));
        return false;
    }
    const qint64 total = file.size();
    if (offset < 0 || offset > total || !file.seek(offset)) {
        setRefusal(refusal, QStringLiteral("The request for the voice keyer was not understood."));
        return false;
    }
    const QByteArray bytes = file.read(kChunkBytes);
    if (bytes.size() < std::min(kChunkBytes, total - offset)) {
        qCWarning(lcRecordings) << "short read of voice message" << n << "at" << offset;
        setRefusal(refusal, QStringLiteral("M%1 cannot be read on the Core.").arg(n));
        return false;
    }
    *chunk = bytes;
    *size = total;
    return true;
}

}  // namespace NereusSDR
