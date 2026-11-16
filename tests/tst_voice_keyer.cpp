// =================================================================
// tests/tst_voice_keyer.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The voice keyer on the Core
// (spec 3, 6.6, V3, section 7, A8, A9). The safety invariants first: only
// send() keys; every way a message is stopped unkeys (or, for a press that
// keys, keeps the radio keyed with the press's microphone); another station
// holding transmit refuses a send and nothing keys. Then the messages: 48 kHz
// 16-bit mono per station, a 44.1 kHz stereo import converted, two minutes at
// most, an export that loads back the same byte for byte, Repeat and its
// wait on an injected clock, the first empty message and the document.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Recording plan Task 11.
//   2026-11-16 - Review fixes. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Ruling 8.5: another device's press
//                 or Stop leaves the message going; the sender's own admitted
//                 key hands over; another device's send while a repeat waits
//                 is refused; the wait ends when the radio drops; imports
//                 convert off the main thread, which keeps running.
//   2026-11-16 - J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. A message on the air is never cleared, imported into
//                 or replaced; leftover import files go on load.
//   2026-11-16 - J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. Fix round 2: Repeat off under a message is the
//                 sender's or the holder's; a waiting repeat ends with its
//                 sender's session and never keys for a sender with none; a
//                 message that cannot be replaced is kept as it was.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "core/TxWorkerThread.h"
#include "core/recordings/AudioFileReader.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/OnAirSource.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/VoiceKeyer.h"
#include "core/safety/TransmitHolder.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include "fakes/QuietRadioConnection.h"

#ifdef Q_OS_MAC
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kBlock = TxWorkerThread::kBlockFrames;   // 64
constexpr int kRate = 48000;
constexpr int kTxChannelId = 1;
const QByteArray kPhoneB = QByteArrayLiteral("phone-b");
const QByteArray kPhoneC = QByteArrayLiteral("phone-c");
const QString kSession = QStringLiteral("session-b");

float rampAt(int frame, int frames)
{
    return 0.9f * static_cast<float>(frame % frames) / static_cast<float>(frames);
}

bool near(float a, float b)
{
    return std::fabs(a - b) < 5e-5f;
}

bool allZero(const std::vector<float>& v)
{
    return std::all_of(v.cbegin(), v.cend(), [](float s) { return s == 0.0f; });
}

KeyerIdentity phone(const QByteArray& id, const QString& session = kSession)
{
    KeyerIdentity k;
    k.deviceId = id;
    k.source = PttMode::None;
    k.session = session;
    return k;
}

VoiceKeyer::Requester phoneB()
{
    return {phone(kPhoneB), QStringLiteral("Phone"), QStringLiteral("cGhvbmUtYg")};
}

VoiceKeyer::Requester station()
{
    return {KeyerIdentity::station(PttMode::None), QString::fromLatin1(VoiceKeyer::kStationName),
            QStringLiteral("station")};
}

// An import's end, as importFile or finishImport report it.
struct Ended {
    bool finished = false;
    bool ok = false;
    QString refusal;
};

VoiceKeyer::ImportDone into(const std::shared_ptr<Ended>& ended)
{
    return [ended](bool ok, const QString& refusal) {
        ended->finished = true;
        ended->ok = ok;
        ended->refusal = refusal;
    };
}

// Waits for an import begun with into(ended); false when it did not end.
bool waitFor(const std::shared_ptr<Ended>& ended)
{
    return QTest::qWaitFor([ended] { return ended->finished; }, 30000);
}

bool importNow(VoiceKeyer& vk, int n, const QString& path, const QString& name, QString* refusal)
{
    auto ended = std::make_shared<Ended>();
    vk.importFile(n, path, name, into(ended));
    if (!waitFor(ended)) {
        return false;
    }
    if (refusal != nullptr) {
        *refusal = ended->refusal;
    }
    return ended->ok;
}

bool finishNow(VoiceKeyer& vk, const QString& owner, QString* refusal)
{
    auto ended = std::make_shared<Ended>();
    vk.finishImport(owner, into(ended));
    if (!waitFor(ended)) {
        return false;
    }
    if (refusal != nullptr) {
        *refusal = ended->refusal;
    }
    return ended->ok;
}

// The files a conversion or a link import leaves behind.
QStringList partFiles(const QString& folder)
{
    return QDir(folder).entryList({QStringLiteral("*.part")}, QDir::Files);
}

struct Switches {
    bool eq, cpdr, lev, cfc, phrot;
    bool operator==(const Switches&) const = default;
};

Switches switchesOf(const TxChannel& ch)
{
    return {ch.lastTxEqRunningForTest(), ch.lastTxCpdrOnForTest(), ch.lastTxLevelerOnForTest(),
            ch.lastTxCfcRunningForTest(), ch.lastPhaseRotatorRunForTest()};
}

Switches switchesOf(const TransmitModel& tm)
{
    return {tm.txEqEnabled(), tm.cpdrOn(), tm.txLevelerOn(), tm.cfcEnabled(), tm.phaseRotatorEnabled()};
}

// A radio as RadioModel has it once connected, with a TX channel that is
// never opened in WDSP, and its voice keyer kept in `folder`.
struct Rig {
    RadioModel model;
    Test::QuietRadioConnection connection;
    TxChannel ch{kTxChannelId};
    MoxController* mox = nullptr;
    RecordingService* svc = nullptr;
    VoiceKeyer* vk = nullptr;
    OnAirSource* onAir = nullptr;

    explicit Rig(const QString& folder)
    {
        svc = model.recordingService();
        mox = model.moxController();
        model.setCapsForTest(/*hasAlex=*/false);
        model.injectConnectionForTest(&connection);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.addSlice();
        model.injectTxChannelForTest(&ch);
        vk = svc->voiceKeyer();
        vk->setFolder(folder);
        onAir = svc->onAirSource();
    }
    ~Rig()
    {
        vk->stop();
        // The channel and connection go before the model does.
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
    }

    // One transmit block from the on-air source; false when the microphone
    // would go out.
    bool pull(std::vector<float>& tx)
    {
        tx.assign(kBlock, -1.0f);
        return onAir->pull(tx.data(), kBlock);
    }
};

}  // namespace

class TestVoiceKeyer : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    int m_folders = 0;
    int m_files = 0;

    QString newFolder()
    {
        return m_dir.filePath(QStringLiteral("vk%1").arg(++m_folders));
    }

    // A WAV of `frames` frames at `rate` with `channels`, every channel the
    // same ramp; its path.
    QString writeWav(int rate, int channels, int frames)
    {
        const QString path = m_dir.filePath(QStringLiteral("in-%1.wav").arg(++m_files));
        QString error;
        std::unique_ptr<AudioFileWriter> w =
            AudioFileWriter::open(path, AudioFileSpec{AudioFileFormat::Wav, rate, channels, 0}, &error);
        if (!w) {
            return {};
        }
        std::vector<float> v(static_cast<size_t>(frames) * static_cast<size_t>(channels));
        for (int i = 0; i < frames; ++i) {
            for (int c = 0; c < channels; ++c) {
                v[static_cast<size_t>(i * channels + c)] = rampAt(i, frames);
            }
        }
        if (!w->write(v.data(), frames) || !w->finish()) {
            return {};
        }
        return path;
    }

    // Mn holding a 48 kHz mono ramp of `frames` frames.
    bool fill(VoiceKeyer& vk, int n, int frames, const QString& name = QString())
    {
        const QString path = writeWav(kRate, 1, frames);
        QString refusal;
        return !path.isEmpty() && importNow(vk, n, path, name, &refusal);
    }

    static QByteArray readAll(const QString& path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    // The whole of Mn through exportChunk, as the link moves it.
    static QByteArray exportAll(VoiceKeyer& vk, int n, int* chunks)
    {
        QByteArray all;
        qint64 size = 0;
        *chunks = 0;
        do {
            QByteArray chunk;
            QString refusal;
            if (!vk.exportChunk(n, all.size(), &chunk, &size, &refusal) || chunk.isEmpty()) {
                return {};
            }
            ++*chunks;
            all.append(chunk);
        } while (all.size() < size);
        return all;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        AppSettings::instance().clear();
        QLoggingCategory::setFilterRules(
            QStringLiteral("nereus.*.debug=false\n"
                           "nereus.*.info=false\n"
                           "nereussdr.*.info=false\n"
                           "nereus.dsp.warning=false"));
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ---- Invariant: only send() keys.

    void nothingButSendKeys()
    {
        Rig r(newFolder());
        QSignalSpy accepted(r.mox, &MoxController::requestAccepted);
        QSignalSpy moxChanged(r.mox, &MoxController::moxChanged);
        QString refusal;
        QVERIFY(fill(*r.vk, 1, 10 * kBlock, QStringLiteral("CQ")));
        QVERIFY(r.vk->rename(1, QStringLiteral("CQ contest"), &refusal));
        QVERIFY(r.vk->setRepeat(true, 7, &refusal));
        QVERIFY(r.vk->listen(1, QStringLiteral("local"), &refusal));
        r.svc->pumpForTest();
        int chunks = 0;
        QVERIFY(!exportAll(*r.vk, 1, &chunks).isEmpty());
        const QString owner = QStringLiteral("owner");
        const QByteArray file = readAll(writeWav(kRate, 1, kBlock));
        QVERIFY(r.vk->beginImport(owner, 2, QStringLiteral("QRZ"), file.size(), &refusal));
        QVERIFY(r.vk->importChunk(owner, file, &refusal));
        QVERIFY(finishNow(*r.vk, owner, &refusal));
        QVERIFY(r.vk->clear(2, &refusal));
        r.vk->stop();
        TxRefusal stopRefusal;
        QVERIFY(r.vk->stopFrom(phone(kPhoneC), &stopRefusal));
        r.vk->keyAdmittedFor(kPhoneB, kSession);
        r.vk->connectionLost();
        r.vk->tickForTest();
        QCoreApplication::processEvents();
        QVERIFY(!r.mox->isMox());
        QCOMPARE(accepted.count(), 0);
        QCOMPARE(moxChanged.count(), 0);
        QVERIFY(!r.vk->sending().has_value());

        // An empty message keys nothing either.
        QVERIFY(!r.vk->send(3, station(), &refusal));
        QCOMPARE(refusal, QStringLiteral("M3 is empty."));
        QVERIFY(!r.mox->isMox());
        QCOMPARE(accepted.count(), 0);
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);

        QVERIFY(r.vk->send(1, station(), &refusal));
        QVERIFY(r.mox->isMox());
        QCOMPARE(accepted.count(), 1);
    }

    // ---- send() keys as the device that asked, sends the message through
    // the on-air source with the processing as it is, and unkeys at the end.

    void sendKeysAsTheRequesterAndUnkeysAtTheEnd()
    {
        Rig r(newFolder());
        TransmitModel& tm = r.model.transmitModel();
        tm.setTxEqEnabled(true);
        tm.setCpdrOn(true);
        tm.setTxLevelerOn(true);
        tm.setCfcEnabled(true);
        tm.setPhaseRotatorEnabled(true);
        r.ch.setTxEqRunning(true);
        r.ch.setTxCpdrOn(true);
        r.ch.setTxLevelerOn(true);
        r.ch.setTxCfcRunning(true);
        r.ch.setStageRunning(TxChannel::Stage::PhRot, true);
        const Switches before = switchesOf(r.ch);
        // None of the five on-air switches apply to a voice message.
        RecordingSettings s = r.svc->settings();
        s.onAirUseTxEq = false;
        s.onAirUseCompressor = false;
        s.onAirUseLeveler = false;
        s.onAirUseCfc = false;
        s.onAirUsePhaseRotator = false;
        r.svc->setSettings(s);

        const int frames = 10 * kBlock + 5;
        QVERIFY(fill(*r.vk, 2, frames));
        QString refusal;
        QSignalSpy changed(r.vk, &VoiceKeyer::changed);
        QVERIFY(r.vk->send(2, phoneB(), &refusal));
        QVERIFY(r.mox->isMox());
        QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneB);
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::VoiceMessage);
        QCOMPARE(r.onAir->voiceMessageSlot(), QStringLiteral("M2"));
        QVERIFY(changed.count() >= 1);
        std::optional<VoiceKeyer::Sending> sending = r.vk->sending();
        QVERIFY(sending.has_value());
        QCOMPARE(sending->n, 2);
        QCOMPARE(sending->by, QStringLiteral("Phone"));
        QCOMPARE(sending->lengthMs, qint64(frames) * 1000 / kRate);
        QCOMPARE(sending->waitingMs, qint64(0));

        std::vector<float> tx;
        for (int b = 0; b < 10; ++b) {
            QVERIFY(r.pull(tx));
            QVERIFY(near(tx.front(), rampAt(b * kBlock, frames)));
            QVERIFY(near(tx.back(), rampAt(b * kBlock + kBlock - 1, frames)));
        }
        QCOMPARE(switchesOf(r.ch), before);
        QCOMPARE(switchesOf(tm), (Switches{true, true, true, true, true}));
        // The last five samples, then silence; the air never falls back to
        // the microphone by itself.
        QVERIFY(r.pull(tx));
        QVERIFY(near(tx[4], rampAt(10 * kBlock + 4, frames)));
        for (int i = 5; i < kBlock; ++i) {
            QCOMPARE(tx[static_cast<size_t>(i)], 0.0f);
        }
        QVERIFY(r.pull(tx));
        QVERIFY(allZero(tx));
        QVERIFY(r.mox->isMox());

        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
        QVERIFY(!r.pull(tx));
        QVERIFY(!r.vk->sending().has_value());
        QCOMPARE(switchesOf(r.ch), before);
    }

    // ---- Invariant: every way a message is stopped unkeys.

    void stopUnkeys()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        r.vk->stop();
        QVERIFY(!r.mox->isMox());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
        QVERIFY(!r.vk->sending().has_value());
    }

    void theSendersUnkeyStopsIt()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        r.mox->setMox(false, phone(kPhoneB));
        QVERIFY(!r.mox->isMox());
        QVERIFY(!r.vk->sending().has_value());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
    }

    void theStationsMoxReleaseStopsIt()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, station(), &refusal));
        QCOMPARE(r.vk->sending()->by, QStringLiteral("Desktop"));
        r.mox->setMox(false, KeyerIdentity::station(PttMode::Manual));
        QVERIFY(!r.mox->isMox());
        QVERIFY(!r.vk->sending().has_value());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
    }

    void moxOffFromElsewhereStopsIt()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QVERIFY(r.vk->setRepeat(true, 5, nullptr));
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        // As Stop All TX does: MOX off whoever keyed it.
        r.mox->setMox(false);
        QVERIFY(!r.mox->isMox());
        QVERIFY(!r.vk->sending().has_value());   // no repeat after it
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
    }

    // Ruling 8.5: another device's press, refused by MOX, cuts nothing.
    // (The radio's own PTT takes transmit through the station's keying
    // gate, which unkeys the sender; tst_recordings_link covers that.)
    void anotherDevicesPressLeavesItGoing()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(!allZero(tx));
        // A PTT press from another device: MOX refuses it.
        r.mox->setMox(true, phone(kPhoneC));
        QCoreApplication::processEvents();
        QVERIFY(r.vk->sending().has_value());
        QVERIFY(r.mox->isMox());
        QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneB);
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::VoiceMessage);
        QVERIFY(r.pull(tx));
        QVERIFY(near(tx[1], rampAt(kBlock + 1, 100 * kBlock)));
        // Its release unkeys nothing either.
        r.mox->setMox(false, phone(kPhoneC));
        QVERIFY(r.mox->isMox());
        QVERIFY(r.vk->sending().has_value());

        // The station's MOX while a phone sends, with no keying gate to
        // take transmit: refused, and the message goes on.
        r.mox->setMox(true, KeyerIdentity::station(PttMode::Manual));
        QCoreApplication::processEvents();
        QVERIFY(r.vk->sending().has_value());
        QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneB);
        QVERIFY(r.pull(tx));
        QVERIFY(!allZero(tx));
    }

    // Ruling 8.5: only the sender (or the holder) stops a message.
    void anotherDevicesStopIsRefused()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        TxRefusal stopRefusal;
        QVERIFY(!r.vk->stopFrom(phone(kPhoneC), &stopRefusal));
        QCOMPARE(stopRefusal.text, TxRefusals::otherDeviceHoldsStop(QStringLiteral("Phone")).text);
        QVERIFY(!r.vk->stopFrom(KeyerIdentity::station(PttMode::None), &stopRefusal));
        QVERIFY(r.mox->isMox());
        QVERIFY(r.vk->sending().has_value());
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(!allZero(tx));
        // The sender's own Stop: silence, then unkeyed.
        QVERIFY(r.vk->stopFrom(phone(kPhoneB), &stopRefusal));
        QVERIFY(!r.mox->isMox());
        QVERIFY(!r.vk->sending().has_value());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);

        // With a holder, the refusal names it, and the holder may stop.
        TransmitHolder holder;
        TransmitHolder::KeyRequest take;
        take.deviceId = kPhoneB;
        QCOMPARE(holder.askKey(take).verdict, KeyingVerdict::Admit);
        r.vk->setTransmitHolder(&holder);
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        QVERIFY(!r.vk->stopFrom(phone(kPhoneC), &stopRefusal));
        QCOMPARE(stopRefusal.text, TxRefusals::otherDeviceHoldsStop(holder.holder()->name).text);
        QVERIFY(r.mox->isMox());
        QVERIFY(r.vk->stopFrom(phone(kPhoneB, QStringLiteral("another-session")), &stopRefusal));
        QVERIFY(!r.mox->isMox());
        r.vk->setTransmitHolder(nullptr);
    }

    void theSendersPttHandsTheAirToItsMic()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, station(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        // The station's PTT while its message goes out: the message stops,
        // the radio stays keyed and the microphone goes out from the next
        // block.
        r.mox->setMox(true, KeyerIdentity::station(PttMode::Mic));
        QVERIFY(!r.vk->sending().has_value());
        QVERIFY(r.mox->isMox());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
        QVERIFY(!r.pull(tx));
        QCoreApplication::processEvents();
        QVERIFY(r.mox->isMox());
        // The release unkeys as ever.
        r.mox->setMox(false, KeyerIdentity::station(PttMode::Mic));
        QVERIFY(!r.mox->isMox());
    }

    // RemoteKeying tells the keyer of a tx.key only once it is admitted.
    void theSendersAdmittedTxKeyHandsOver()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        // Another device's, or another session's of the same device: no
        // such key is admitted while B is keyed, and none cuts anything.
        r.vk->keyAdmittedFor(kPhoneC, QStringLiteral("session-c"));
        r.vk->keyAdmittedFor(kPhoneB, QStringLiteral("session-old"));
        QVERIFY(r.vk->sending().has_value());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::VoiceMessage);
        // The sender's own: its key stays and its microphone goes out.
        r.vk->keyAdmittedFor(kPhoneB, kSession);
        QVERIFY(!r.vk->sending().has_value());
        QVERIFY(r.mox->isMox());
        QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneB);
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
        std::vector<float> tx;
        QVERIFY(!r.pull(tx));
        r.mox->setMox(false, phone(kPhoneB));
        QVERIFY(!r.mox->isMox());
    }

    void voxCutsNothing()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, station(), &refusal));
        r.mox->setMox(true, KeyerIdentity::station(PttMode::Vox));
        QVERIFY(r.vk->sending().has_value());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::VoiceMessage);
    }

    // ---- Invariant: another station holding transmit refuses the send.

    void refusedWhenAnotherStationHoldsTransmit()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 10 * kBlock));
        TransmitHolder holder;
        TransmitHolder::KeyRequest take;
        take.deviceId = kPhoneC;
        QCOMPARE(holder.askKey(take).verdict, KeyingVerdict::Admit);
        r.vk->setTransmitHolder(&holder);
        QSignalSpy accepted(r.mox, &MoxController::requestAccepted);
        QString refusal;
        QVERIFY(!r.vk->send(1, phoneB(), &refusal));
        QVERIFY(!refusal.isEmpty());
        QCOMPARE(refusal, holder.keyRefusalFor(kPhoneB).text);
        QVERIFY(!r.vk->send(1, station(), &refusal));
        QCOMPARE(refusal, holder.keyRefusalFor(KeyerIdentity::station(PttMode::None).deviceId).text);
        QVERIFY(!r.mox->isMox());
        QCOMPARE(accepted.count(), 0);
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
        QVERIFY(!r.vk->sending().has_value());

        // The holder itself may send.
        KeyerIdentity holderKeyer = phone(kPhoneC);
        QVERIFY(r.vk->send(1, VoiceKeyer::Requester{holderKeyer, QStringLiteral("C")}, &refusal));
        QVERIFY(r.mox->isMox());
        r.vk->stop();
        QVERIFY(!r.mox->isMox());
        r.vk->setTransmitHolder(nullptr);
    }

    void refusedWhileTheRadioTransmits()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 10 * kBlock));
        r.mox->setMox(true, phone(kPhoneC));
        QString refusal;
        QVERIFY(!r.vk->send(1, phoneB(), &refusal));
        QCOMPARE(refusal, QString::fromLatin1(VoiceKeyer::kTransmittingReason));
        QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneC);
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
        r.mox->setMox(false, phone(kPhoneC));

        // Another device's send while a message goes out leaves it going.
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        QVERIFY(!r.vk->send(1, VoiceKeyer::Requester{phone(kPhoneC), QStringLiteral("C")}, &refusal));
        QCOMPARE(refusal, QString::fromLatin1(VoiceKeyer::kTransmittingReason));
        QVERIFY(r.mox->isMox());
        QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneB);
        QCOMPARE(r.vk->sending()->by, QStringLiteral("Phone"));
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::VoiceMessage);
    }

    void anotherDevicesSendWhileARepeatWaitsIsRefused()
    {
        Rig r(newFolder());
        qint64 t = 0;
        r.vk->setClockForTest([&t] { return t; });
        QVERIFY(fill(*r.vk, 1, kBlock));
        QString refusal;
        QVERIFY(r.vk->setRepeat(true, 5, &refusal));
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        QVERIFY(r.vk->sending().has_value());
        QVERIFY(!r.vk->send(1, VoiceKeyer::Requester{phone(kPhoneC), QStringLiteral("C"), {}}, &refusal));
        QCOMPARE(refusal, QString::fromLatin1(VoiceKeyer::kTransmittingReason));
        QVERIFY(!r.mox->isMox());
        QCOMPARE(r.vk->sending()->by, QStringLiteral("Phone"));
        // B's repeat still goes.
        t += 5000;
        r.vk->tickForTest();
        QVERIFY(r.mox->isMox());
        QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneB);
    }

    // A message on the air is never cleared, imported into or replaced:
    // "M1 is on the air.", whoever asks. It goes on, its file is the same,
    // and renaming it is allowed. The same while a repeat waits.
    void aMessageOnTheAirIsNeverClearedOrReplaced()
    {
        Rig r(newFolder());
        qint64 t = 0;
        r.vk->setClockForTest([&t] { return t; });
        QVERIFY(fill(*r.vk, 1, 2 * kBlock, QStringLiteral("CQ")));
        const QByteArray kept = readAll(r.vk->messagePath(1));
        const QString onAir = QStringLiteral("M1 is on the air.");
        const QString wav = writeWav(kRate, 1, kBlock);
        const QByteArray file = readAll(wav);
        QString refusal;

        // Begun before the send: a link import and a conversion that would
        // land on M1 while it goes out.
        const QString early = QStringLiteral("early");
        QVERIFY(r.vk->beginImport(early, 1, QString(), file.size(), &refusal));
        QVERIFY(r.vk->importChunk(early, file, &refusal));
        auto converting = std::make_shared<Ended>();
        r.vk->importFile(1, writeWav(44100, 2, 10 * 44100), QString(), into(converting));

        QVERIFY(r.vk->setRepeat(true, 5, &refusal));
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        QVERIFY(r.mox->isMox());

        const auto refusedEverything = [&](const char* where) {
            QString why;
            QVERIFY2(!r.vk->clear(1, &why), where);
            QCOMPARE(why, onAir);
            why.clear();
            QVERIFY2(!importNow(*r.vk, 1, wav, QStringLiteral("New"), &why), where);
            QCOMPARE(why, onAir);
            why.clear();
            QVERIFY2(!r.vk->beginImport(QStringLiteral("late"), 1, QString(), file.size(), &why), where);
            QCOMPARE(why, onAir);
            QVERIFY2(r.vk->sending().has_value(), where);
            QCOMPARE(r.vk->sending()->n, 1);
            QVERIFY2(!r.vk->messages().at(0).empty, where);
            QCOMPARE(readAll(r.vk->messagePath(1)), kept);
        };

        refusedEverything("going out");
        QVERIFY(r.mox->isMox());
        QCOMPARE(r.onAir->active(), OnAirSource::Kind::VoiceMessage);
        QVERIFY(!finishNow(*r.vk, early, &refusal));
        QCOMPARE(refusal, onAir);
        QVERIFY(waitFor(converting));
        QVERIFY(!converting->ok);
        QCOMPARE(converting->refusal, onAir);
        QVERIFY(r.mox->isMox());
        QCOMPARE(readAll(r.vk->messagePath(1)), kept);
        QCOMPARE(partFiles(r.vk->folder()), QStringList());
        // Renaming is allowed.
        QVERIFY(r.vk->rename(1, QStringLiteral("CQ test"), &refusal));
        QCOMPARE(r.vk->messages().at(0).name, QStringLiteral("CQ test"));
        // Another slot is not on the air.
        QVERIFY(importNow(*r.vk, 2, wav, QString(), &refusal));
        QVERIFY(r.vk->clear(2, &refusal));

        // Waiting to repeat, it is still on the air.
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        QVERIFY(r.vk->sending()->waitingMs > 0);
        refusedEverything("waiting");

        // Stop ends it, and then it can be cleared.
        r.vk->stop();
        QVERIFY(r.vk->clear(1, &refusal));
        QVERIFY(r.vk->messages().at(0).empty);
    }

    // A Core that shut down in the middle of an import left its files:
    // they go when the messages load, and nothing else does.
    void leftoverImportFilesGoOnLoad()
    {
        const QString folder = newFolder();
        {
            Rig r(folder);
            QVERIFY(fill(*r.vk, 1, kBlock, QStringLiteral("CQ")));
        }
        const QStringList leftovers{QStringLiteral("import-3.part"), QStringLiteral("M2-7.part")};
        for (const QString& name : leftovers) {
            QFile f(QDir(folder).filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("half a message");
        }
        {
            QFile f(QDir(folder).filePath(QStringLiteral("notes.txt")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("kept");
        }
        QCOMPARE(partFiles(folder).size(), 2);
        Rig again(folder);
        const QList<VoiceKeyer::Message> list = again.vk->messages();
        QCOMPARE(partFiles(folder), QStringList());
        QVERIFY(!list.at(0).empty);
        QCOMPARE(list.at(0).name, QStringLiteral("CQ"));
        QVERIFY(list.at(1).empty);
        QVERIFY(QFileInfo::exists(QDir(folder).filePath(QStringLiteral("notes.txt"))));
    }

    // The wait between repeats has MOX off: the radio dropping ends it, and
    // a reconnect keys nothing.
    void theWaitEndsWhenTheRadioDrops()
    {
        Rig r(newFolder());
        r.model.setConnectionStateForTest(ConnectionState::Connected);
        qint64 t = 0;
        r.vk->setClockForTest([&t] { return t; });
        QVERIFY(fill(*r.vk, 1, kBlock));
        QString refusal;
        QVERIFY(r.vk->setRepeat(true, 5, &refusal));
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        QVERIFY(r.vk->sending().has_value());
        QSignalSpy accepted(r.mox, &MoxController::requestAccepted);
        r.model.setConnectionStateForTest(ConnectionState::LinkLost);
        QVERIFY(!r.vk->sending().has_value());
        r.model.setConnectionStateForTest(ConnectionState::Disconnected);
        r.model.setConnectionStateForTest(ConnectionState::Connected);
        t += 10000;
        r.vk->tickForTest();
        QCoreApplication::processEvents();
        QVERIFY(!r.mox->isMox());
        QCOMPARE(accepted.count(), 0);
        QVERIFY(!r.vk->sending().has_value());

        // A message going out ends the same way.
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        QVERIFY(r.mox->isMox());
        r.model.setConnectionStateForTest(ConnectionState::Disconnected);
        QVERIFY(!r.vk->sending().has_value());
        QVERIFY(!r.mox->isMox());
    }

    // A large import converts off the main thread: importFile returns at
    // once, and the end of a message going out is found and unkeyed while
    // the conversion is still under way.
    void anImportLeavesTheMainThreadRunning()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 2 * kBlock));
        // 100 s of 44.1 kHz stereo (about 17.6 MB): resampled and folded.
        const QString big = writeWav(44100, 2, 100 * 44100);
        QVERIFY(!big.isEmpty());
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        auto ended = std::make_shared<Ended>();
        r.vk->importFile(2, big, QStringLiteral("Long"), into(ended));
        QVERIFY(!ended->finished);
        std::vector<float> tx;
        for (int b = 0; b < 3; ++b) {
            QVERIFY(r.pull(tx));
        }
        // The end-of-message tick runs and unkeys with the import pending.
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        QVERIFY(!r.vk->sending().has_value());
        QVERIFY(!ended->finished);
        QVERIFY(r.vk->messages().at(1).empty);
        // The timer drives it too, between the conversion's turns.
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        for (int b = 0; b < 3; ++b) {
            QVERIFY(r.pull(tx));
        }
        QTRY_VERIFY_WITH_TIMEOUT(!r.mox->isMox(), 5000);
        QVERIFY(waitFor(ended));
        QVERIFY2(ended->ok, qPrintable(ended->refusal));
        QVERIFY(!r.vk->messages().at(1).empty);
        QCOMPARE(r.vk->messages().at(1).name, QStringLiteral("Long"));
        QVERIFY(std::abs(r.vk->messages().at(1).lengthMs - 100000) <= 1);
        QCOMPARE(partFiles(r.vk->folder()), QStringList());
    }

    // ---- One message after another from the same device, no gap.

    void theNextMessageFollowsWithNoGap()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 100 * kBlock));
        QVERIFY(fill(*r.vk, 2, 50 * kBlock));
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QSignalSpy accepted(r.mox, &MoxController::requestAccepted);
        QVERIFY(r.vk->send(2, phoneB(), &refusal));
        QVERIFY(r.mox->isMox());
        QCOMPARE(accepted.count(), 0);
        QCOMPARE(r.onAir->voiceMessageSlot(), QStringLiteral("M2"));
        QCOMPARE(r.vk->sending()->n, 2);
        QVERIFY(r.pull(tx));
        QVERIFY(near(tx[1], rampAt(1, 50 * kBlock)));
    }

    // ---- Repeat: unkeys, waits (waitingMs counts down), sends again.

    void repeatWaitsAndSendsAgainUntilStopped()
    {
        Rig r(newFolder());
        qint64 t = 1000;
        r.vk->setClockForTest([&t] { return t; });
        const int frames = 4 * kBlock;
        QVERIFY(fill(*r.vk, 1, frames));
        QString refusal;
        QVERIFY(r.vk->setRepeat(true, 5, &refusal));
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        for (int round = 0; round < 2; ++round) {
            for (int b = 0; b < 5; ++b) {
                QVERIFY(r.pull(tx));
            }
            r.vk->tickForTest();
            QVERIFY(!r.mox->isMox());
            QCOMPARE(r.onAir->active(), OnAirSource::Kind::None);
            std::optional<VoiceKeyer::Sending> s = r.vk->sending();
            QVERIFY(s.has_value());
            QCOMPARE(s->n, 1);
            QCOMPARE(s->waitingMs, qint64(5000));
            t += 2000;
            QCOMPARE(r.vk->sending()->waitingMs, qint64(3000));
            r.vk->tickForTest();
            QVERIFY(!r.mox->isMox());
            t += 3000;
            r.vk->tickForTest();
            QVERIFY(r.mox->isMox());
            QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneB);
            QCOMPARE(r.vk->sending()->waitingMs, qint64(0));
            QVERIFY(r.pull(tx));
            QVERIFY(near(tx[1], rampAt(1, frames)));
        }
        r.vk->stop();
        QVERIFY(!r.mox->isMox());
        QVERIFY(!r.vk->sending().has_value());
        t += 10000;
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
    }

    void aPressDuringTheWaitEndsTheRepeat()
    {
        Rig r(newFolder());
        qint64 t = 0;
        r.vk->setClockForTest([&t] { return t; });
        QVERIFY(fill(*r.vk, 1, kBlock));
        QString refusal;
        QVERIFY(r.vk->setRepeat(true, 5, &refusal));
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(r.vk->sending().has_value());
        QVERIFY(!r.mox->isMox());
        // The station's PTT keys as ever and the repeat is over.
        r.mox->setMox(true, KeyerIdentity::station(PttMode::Mic));
        QVERIFY(!r.vk->sending().has_value());
        r.mox->setMox(false, KeyerIdentity::station(PttMode::Mic));
        t += 6000;
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());

        // Another holder by the time of the repeat: refused, nothing keys.
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        TransmitHolder holder;
        TransmitHolder::KeyRequest take;
        take.deviceId = kPhoneC;
        QCOMPARE(holder.askKey(take).verdict, KeyingVerdict::Admit);
        r.vk->setTransmitHolder(&holder);
        QSignalSpy refused(r.vk, &VoiceKeyer::sendRefused);
        t += 6000;
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        QVERIFY(!r.vk->sending().has_value());
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.first().at(0).toByteArray(), kPhoneB);
        QCOMPARE(refused.first().at(1).toString(), holder.keyRefusalFor(kPhoneB).text);
        r.vk->setTransmitHolder(nullptr);
    }

    void repeatOffInTheWaitEndsIt()
    {
        Rig r(newFolder());
        qint64 t = 0;
        r.vk->setClockForTest([&t] { return t; });
        QVERIFY(fill(*r.vk, 1, kBlock));
        QVERIFY(r.vk->setRepeat(true, 5, nullptr));
        QString refusal;
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(r.vk->sending().has_value());
        QVERIFY(r.vk->setRepeat(false, 5, &refusal));
        QVERIFY(!r.vk->sending().has_value());
        t += 6000;
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        QVERIFY(!r.vk->setRepeat(true, 0, &refusal));
        QCOMPARE(refusal, QStringLiteral("The wait between repeats can be 1 to 60 seconds."));
        QVERIFY(!r.vk->setRepeat(true, 61, &refusal));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("VoiceKeyerRepeat")).toString(),
                 QStringLiteral("False"));
    }

    // Ruling 8.5: turning Repeat off under a message ends its repeats, so
    // it is the sender's or the holder's only, while it goes out and while
    // it waits. Another device may still change the wait, and turn Repeat
    // off once nothing is on the air.
    void onlyTheSenderOrTheHolderTurnsRepeatOffUnderAMessage()
    {
        Rig r(newFolder());
        qint64 t = 0;
        r.vk->setClockForTest([&t] { return t; });
        QVERIFY(fill(*r.vk, 1, kBlock));
        QString refusal;
        QVERIFY(r.vk->setRepeat(true, 5, &refusal));
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        const QString words = TxRefusals::otherDeviceHoldsStop(QStringLiteral("Phone")).text;
        TxRefusal held;
        QVERIFY(!r.vk->setRepeatFrom(phone(kPhoneC), false, 5, &refusal, &held));
        QCOMPARE(refusal, words);
        QCOMPARE(held.text, words);
        QVERIFY(r.vk->repeat());
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(r.vk->sending().has_value());
        QVERIFY(!r.mox->isMox());
        // Waiting: refused again, and the repeat still goes.
        QVERIFY(!r.vk->setRepeatFrom(phone(kPhoneC), false, 5, &refusal));
        QCOMPARE(refusal, words);
        QVERIFY(r.vk->sending().has_value());
        QVERIFY(r.vk->setRepeatFrom(phone(kPhoneC), true, 7, &refusal));
        QCOMPARE(r.vk->repeatDelaySec(), 7);
        t += 7000;
        r.vk->tickForTest();
        QVERIFY(r.mox->isMox());
        QCOMPARE(r.mox->currentKeyer().deviceId, kPhoneB);
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        // The sender's own Repeat off ends the wait.
        QVERIFY(r.vk->setRepeatFrom(phone(kPhoneB), false, 7, &refusal));
        QVERIFY(!r.vk->sending().has_value());
        t += 8000;
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        // Nothing on the air: any device's.
        QVERIFY(r.vk->setRepeatFrom(phone(kPhoneC), true, 7, &refusal));
        QVERIFY(r.vk->setRepeatFrom(phone(kPhoneC), false, 7, &refusal));
        QVERIFY(!r.vk->repeat());

        // The holder, after a take, ends another device's wait.
        TransmitHolder holder;
        r.vk->setTransmitHolder(&holder);
        QVERIFY(r.vk->setRepeat(true, 5, &refusal));
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(r.vk->sending().has_value());
        TransmitHolder::KeyRequest take;
        take.deviceId = kPhoneC;
        QCOMPARE(holder.askKey(take).verdict, KeyingVerdict::Admit);
        QVERIFY(r.vk->setRepeatFrom(phone(kPhoneC), false, 5, &refusal));
        QVERIFY(!r.vk->sending().has_value());
        r.vk->setTransmitHolder(nullptr);
    }

    // A repeat whose sender's session ended (its link dropped, or it left)
    // is over; and one whose sender has no live session never keys again.
    void aWaitingRepeatGoesWithItsSendersSession()
    {
        Rig r(newFolder());
        qint64 t = 0;
        r.vk->setClockForTest([&t] { return t; });
        QVERIFY(fill(*r.vk, 1, kBlock));
        QString refusal;
        QVERIFY(r.vk->setRepeat(true, 5, &refusal));
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        std::vector<float> tx;
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(r.vk->sending().has_value());
        r.vk->sessionEnded(QStringLiteral("another-session"));
        QVERIFY(r.vk->sending().has_value());
        r.vk->sessionEnded(kSession);
        QVERIFY(!r.vk->sending().has_value());
        t += 6000;
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());

        // No live session by the time of the repeat: refused, nothing keys.
        bool alive = true;
        r.vk->setSenderAlive([&alive](const KeyerIdentity&) { return alive; });
        QVERIFY(r.vk->send(1, phoneB(), &refusal));
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        QVERIFY(r.vk->sending().has_value());
        alive = false;
        QSignalSpy refused(r.vk, &VoiceKeyer::sendRefused);
        QSignalSpy accepted(r.mox, &MoxController::requestAccepted);
        t += 6000;
        r.vk->tickForTest();
        QVERIFY(!r.mox->isMox());
        QCOMPARE(accepted.count(), 0);
        QVERIFY(!r.vk->sending().has_value());
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.first().at(0).toByteArray(), kPhoneB);
        QCOMPARE(refused.first().at(1).toString(), TxRefusals::notReady().text);
        // The station's own window is always here.
        QVERIFY(r.vk->send(1, station(), &refusal));
        QVERIFY(r.pull(tx));
        QVERIFY(r.pull(tx));
        r.vk->tickForTest();
        t += 6000;
        r.vk->tickForTest();
        QVERIFY(r.mox->isMox());
        r.vk->stop();
        r.vk->setSenderAlive({});
    }

    // A message whose file cannot be removed (on Windows, while it is
    // listened to) is kept as it was, and the import is refused saying so.
    void aMessageThatCannotBeReplacedIsKept()
    {
        const QString inUse =
            QStringLiteral("M%1 cannot be replaced while it is in use. Try again once it stops playing.");
        const QString wav = writeWav(kRate, 1, kBlock);
        QString refusal;
        {
            // A folder in M2's place: never a message, never removed.
            const QString folder = newFolder();
            QVERIFY(QDir().mkpath(QDir(folder).filePath(QStringLiteral("M2.wav"))));
            QFile inside(QDir(folder).filePath(QStringLiteral("M2.wav/keep")));
            QVERIFY(inside.open(QIODevice::WriteOnly));
            inside.close();
            Rig r(folder);
            QVERIFY(r.vk->messages().at(1).empty);
            QVERIFY(!importNow(*r.vk, 2, wav, QStringLiteral("New"), &refusal));
            QCOMPARE(refusal, inUse.arg(2));
            QVERIFY(r.vk->messages().at(1).empty);
            QVERIFY(QFileInfo::exists(inside.fileName()));
            QCOMPARE(partFiles(folder), QStringList());
        }
#ifdef Q_OS_MAC
        // A kept message the system will not let go of.
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 4 * kBlock, QStringLiteral("CQ")));
        const QString path = r.vk->messagePath(1);
        const QByteArray kept = readAll(path);
        const qint64 length = r.vk->messages().at(0).lengthMs;
        QCOMPARE(::chflags(QFile::encodeName(path).constData(), UF_IMMUTABLE), 0);
        const bool imported = importNow(*r.vk, 1, wav, QStringLiteral("New"), &refusal);
        ::chflags(QFile::encodeName(path).constData(), 0);
        QVERIFY(!imported);
        QCOMPARE(refusal, inUse.arg(1));
        QVERIFY(!r.vk->messages().at(0).empty);
        QCOMPARE(r.vk->messages().at(0).lengthMs, length);
        QCOMPARE(r.vk->messages().at(0).name, QStringLiteral("CQ"));
        QCOMPARE(readAll(path), kept);
        QCOMPARE(partFiles(r.vk->folder()), QStringList());
#endif
    }

    // ---- The messages.

    void aStereo44kImportIsConvertedTo48kMono()
    {
        Rig r(newFolder());
        const QString in = writeWav(44100, 2, 44100);   // one second
        QString refusal;
        QVERIFY(importNow(*r.vk, 1, in, QStringLiteral("CQ"), &refusal));
        const QString kept = r.vk->messagePath(1);
        QVERIFY(!kept.isEmpty());
        QString error;
        std::unique_ptr<AudioFileReader> reader = AudioFileReader::open(kept, &error);
        QVERIFY(reader);
        QCOMPARE(reader->sampleRateHz(), kRate);
        QCOMPARE(reader->channels(), 1);
        QVERIFY(std::abs(reader->lengthFrames() - qint64(kRate)) <= 2);
        const VoiceKeyer::Message m = r.vk->messages().at(0);
        QCOMPARE(m.name, QStringLiteral("CQ"));
        QVERIFY(!m.empty);
        QVERIFY(std::abs(m.lengthMs - 1000) <= 1);
        // The ramp survives: half way through it is about half way up.
        std::vector<float> s(static_cast<size_t>(reader->lengthFrames()));
        QCOMPARE(reader->read(s.data(), static_cast<int>(s.size())), static_cast<int>(s.size()));
        QVERIFY(std::fabs(s[kRate / 2] - 0.45f) < 0.01f);
    }

    void anImportOverTwoMinutesIsRefused()
    {
        Rig r(newFolder());
        const QString in = writeWav(kRate, 1, VoiceKeyer::kVoiceClipMaxMs / 1000 * kRate + kRate / 10);
        QString refusal;
        QVERIFY(!importNow(*r.vk, 1, in, QString(), &refusal));
        QCOMPARE(refusal, QStringLiteral("Voice messages can be up to 2 minutes long."));
        QVERIFY(r.vk->messages().at(0).empty);
        QVERIFY(r.vk->messagePath(1).isEmpty());
        QCOMPARE(partFiles(r.vk->folder()), QStringList());
        // Nor is something that is not sound.
        const QString junk = m_dir.filePath(QStringLiteral("junk.wav"));
        QFile f(junk);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not a sound file");
        f.close();
        QVERIFY(!importNow(*r.vk, 1, junk, QString(), &refusal));
        QCOMPARE(refusal, QStringLiteral("That file cannot be read as sound."));
        QCOMPARE(partFiles(r.vk->folder()), QStringList());
    }

    void anExportLoadsBackTheSameByteForByte()
    {
        Rig r(newFolder());
        // Three seconds: more than one chunk.
        const QString in = writeWav(kRate, 1, 3 * kRate);
        QString refusal;
        QVERIFY(importNow(*r.vk, 3, in, QString(), &refusal));
        QCOMPARE(readAll(r.vk->messagePath(3)), readAll(in));
        int chunks = 0;
        const QByteArray out = exportAll(*r.vk, 3, &chunks);
        QVERIFY(chunks > 1);
        QCOMPARE(out, readAll(in));

        // A converted message saved and loaded back is the same again.
        QVERIFY(importNow(*r.vk, 4, writeWav(44100, 2, 44100), QString(), &refusal));
        const QByteArray converted = exportAll(*r.vk, 4, &chunks);
        const QString saved = m_dir.filePath(QStringLiteral("saved-m4.wav"));
        QFile f(saved);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(converted);
        f.close();
        QVERIFY(importNow(*r.vk, 5, saved, QString(), &refusal));
        QCOMPARE(readAll(r.vk->messagePath(5)), converted);

        // Through the link's import, a chunk at a time.
        const QString owner = QStringLiteral("phone-b");
        QVERIFY(r.vk->beginImport(owner, 6, QStringLiteral("Link"), out.size(), &refusal));
        for (qint64 at = 0; at < out.size(); at += VoiceKeyer::kChunkBytes) {
            QVERIFY(r.vk->importChunk(owner, out.mid(at, VoiceKeyer::kChunkBytes), &refusal));
        }
        QVERIFY(finishNow(*r.vk, owner, &refusal));
        QCOMPARE(readAll(r.vk->messagePath(6)), out);
        QCOMPARE(r.vk->messages().at(5).name, QStringLiteral("Link"));
        QCOMPARE(partFiles(r.vk->folder()), QStringList());

        // An empty one has nothing to save.
        QByteArray chunk;
        qint64 size = 0;
        QVERIFY(!r.vk->exportChunk(7, 0, &chunk, &size, &refusal));
        QCOMPARE(refusal, QStringLiteral("M7 is empty."));
        // A short link import is refused and keeps nothing.
        QVERIFY(r.vk->beginImport(owner, 7, QString(), out.size(), &refusal));
        QVERIFY(r.vk->importChunk(owner, out.left(100), &refusal));
        QVERIFY(!finishNow(*r.vk, owner, &refusal));
        QCOMPARE(refusal, QStringLiteral("The file did not arrive whole. Try again."));
        QVERIFY(r.vk->messages().at(6).empty);

        // Two owners' imports at once each have a file of their own, and
        // one cancelled (its session ended) takes only its own.
        const QString other = QStringLiteral("tablet");
        QVERIFY(r.vk->beginImport(owner, 7, QString(), out.size(), &refusal));
        QVERIFY(r.vk->beginImport(other, 8, QString(), out.size(), &refusal));
        QVERIFY(r.vk->importChunk(owner, out.left(100), &refusal));
        QVERIFY(r.vk->importChunk(other, out.left(200), &refusal));
        QCOMPARE(partFiles(r.vk->folder()).size(), 2);
        r.vk->cancelImport(owner);
        QCOMPARE(partFiles(r.vk->folder()).size(), 1);
        QVERIFY(!r.vk->importChunk(owner, out.mid(100, 100), &refusal));
        QCOMPARE(refusal, QStringLiteral("No voice message is being loaded."));
        for (qint64 at = 200; at < out.size(); at += VoiceKeyer::kChunkBytes) {
            QVERIFY(r.vk->importChunk(other, out.mid(at, VoiceKeyer::kChunkBytes), &refusal));
        }
        QVERIFY(finishNow(*r.vk, other, &refusal));
        QCOMPARE(readAll(r.vk->messagePath(8)), out);
        QCOMPARE(partFiles(r.vk->folder()), QStringList());
    }

    void theFirstEmptyMessage()
    {
        Rig r(newFolder());
        QCOMPARE(r.vk->firstEmpty(), 1);
        QVERIFY(fill(*r.vk, 1, kBlock));
        QVERIFY(fill(*r.vk, 2, kBlock));
        QCOMPARE(r.vk->firstEmpty(), 3);
        for (int n = 3; n <= VoiceKeyer::kVoiceKeyerSlots; ++n) {
            QVERIFY(fill(*r.vk, n, kBlock));
        }
        QCOMPARE(r.vk->firstEmpty(), 0);
        QString refusal;
        QVERIFY(r.vk->clear(4, &refusal));
        QCOMPARE(r.vk->firstEmpty(), 4);
        QVERIFY(!r.vk->clear(9, &refusal));
        QCOMPARE(refusal, QStringLiteral("Voice messages are M1 to M8."));
    }

    void messagesAreKeptPerStation()
    {
        const QString folder = newFolder();
        {
            Rig r(folder);
            QVERIFY(fill(*r.vk, 2, kBlock, QStringLiteral("QRZ")));
        }
        Rig again(folder);
        const QList<VoiceKeyer::Message> list = again.vk->messages();
        QCOMPARE(list.size(), 8);
        QVERIFY(list.at(0).empty);
        QVERIFY(!list.at(1).empty);
        QCOMPARE(list.at(1).name, QStringLiteral("QRZ"));
    }

    void theDocument()
    {
        Rig r(newFolder());
        QVERIFY(fill(*r.vk, 1, 10 * kBlock, QStringLiteral("CQ")));
        QJsonObject doc = r.vk->documentJson();
        const QJsonArray slotList = doc.value(QStringLiteral("slots")).toArray();
        QCOMPARE(slotList.size(), 8);
        const QJsonObject first = slotList.at(0).toObject();
        QCOMPARE(first.value(QStringLiteral("n")).toInt(), 1);
        QCOMPARE(first.value(QStringLiteral("name")).toString(), QStringLiteral("CQ"));
        QCOMPARE(first.value(QStringLiteral("lengthMs")).toInteger(), qint64(10 * kBlock) * 1000 / kRate);
        QCOMPARE(first.value(QStringLiteral("empty")).toBool(), false);
        QCOMPARE(slotList.at(7).toObject().value(QStringLiteral("empty")).toBool(), true);
        QCOMPARE(doc.value(QStringLiteral("repeat")).toBool(), false);
        QCOMPARE(doc.value(QStringLiteral("repeatDelaySec")).toInt(), 5);
        QVERIFY(doc.value(QStringLiteral("sending")).isNull());

        QString refusal;
        QVERIFY(r.vk->send(1, station(), &refusal));
        doc = r.vk->documentJson();
        const QJsonObject sending = doc.value(QStringLiteral("sending")).toObject();
        QCOMPARE(sending.value(QStringLiteral("n")).toInt(), 1);
        QCOMPARE(sending.value(QStringLiteral("positionMs")).toInteger(), qint64(0));
        QCOMPARE(sending.value(QStringLiteral("lengthMs")).toInteger(), qint64(10 * kBlock) * 1000 / kRate);
        QCOMPARE(sending.value(QStringLiteral("waitingMs")).toInteger(), qint64(0));
        QCOMPARE(sending.value(QStringLiteral("by")).toString(), QStringLiteral("Desktop"));
        QCOMPARE(sending.value(QStringLiteral("deviceId")).toString(), QStringLiteral("station"));
    }
};

QTEST_MAIN(TestVoiceKeyer)
#include "tst_voice_keyer.moc"
