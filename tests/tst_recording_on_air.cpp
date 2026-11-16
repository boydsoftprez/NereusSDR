// =================================================================
// tests/tst_recording_on_air.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. A recording on the air (spec 3,
// 6.1 On-air source, 6.3, V5, V6, V7, A6, A7): keying while a recording
// plays sends it from where it is and takes it off that listener's
// speakers; another device keying sends its own microphone; the end of the
// recording hands over to the microphone within one block and the radio
// stays keyed; the five processing switches act on the TX channel while it
// goes out and come back from TransmitModel, which never changes; pull()
// neither allocates nor waits.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Recording plan Task 10.
//   2026-11-16 - A recording goes on the air at the fixed playback level
//                (Thetis cmaster.cs:1093-1110 [v2.10.3.15]), not the mic
//                gain; a voice message keeps the mic gain. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code. Recording
//                plan Task 10b.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "core/TxWorkerThread.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/OnAirSource.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingStore.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include "fakes/QuietRadioConnection.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>
#include <thread>
#include <vector>

// ---- An allocation counter for pull()'s no-allocation check.
namespace {
thread_local bool g_countAllocations = false;
std::atomic<int> g_allocations{0};

void* countedAlloc(std::size_t n)
{
    if (g_countAllocations) {
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    }
    void* p = std::malloc(n == 0 ? 1 : n);
    if (p == nullptr) {
        std::abort();
    }
    return p;
}
}  // namespace

void* operator new(std::size_t n) { return countedAlloc(n); }
void* operator new[](std::size_t n) { return countedAlloc(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace NereusSDR;

namespace {

constexpr int kBlock = TxWorkerThread::kBlockFrames;   // 64
constexpr int kRate = 48000;
constexpr int kTxChannelId = 1;
constexpr int kPhoneSlot = -4;
const QString kLocal = QStringLiteral("local");
const QString kPhone = QStringLiteral("phone-b");

// A sawtooth: a sample says where in the recording it is (within a period
// far longer than any stretch compared). Its steps are well above the
// 16-bit WAV file's resolution, which `near` allows for.
constexpr int kRampPeriod = 8192;

float rampAt(int frame, int frames)
{
    // A short recording climbs over its whole length.
    const int period = std::min(frames, kRampPeriod);
    return 0.9f * static_cast<float>(frame % period) / static_cast<float>(period);
}

std::vector<float> ramp(int frames)
{
    std::vector<float> v(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        v[static_cast<size_t>(i)] = rampAt(i, frames);
    }
    return v;
}

bool near(float a, float b)
{
    return std::fabs(a - b) < 5e-5f;
}

// The five switches as the TX channel last had them.
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

// RadioModel's TransmitModel -> TxChannel connections for the five, as
// connectToRadio makes them before anything keys.
void wireLikeRadioModel(TransmitModel& tm, TxChannel& ch)
{
    QObject::connect(&tm, &TransmitModel::txEqEnabledChanged, &ch, [&ch](bool on) { ch.setTxEqRunning(on); });
    QObject::connect(&tm, &TransmitModel::cpdrOnChanged, &ch, [&ch](bool on) { ch.setTxCpdrOn(on); });
    QObject::connect(&tm, &TransmitModel::txLevelerOnChanged, &ch, [&ch](bool on) { ch.setTxLevelerOn(on); });
    QObject::connect(&tm, &TransmitModel::cfcEnabledChanged, &ch, [&ch](bool on) { ch.setTxCfcRunning(on); });
    QObject::connect(&tm, &TransmitModel::phaseRotatorEnabledChanged, &ch,
                     [&ch](bool on) { ch.setStageRunning(TxChannel::Stage::PhRot, on); });
    // The initial sync.
    ch.setTxEqRunning(tm.txEqEnabled());
    ch.setTxCpdrOn(tm.cpdrOn());
    ch.setTxLevelerOn(tm.txLevelerOn());
    ch.setTxCfcRunning(tm.cfcEnabled());
    ch.setStageRunning(TxChannel::Stage::PhRot, tm.phaseRotatorEnabled());
}

void setAllProcessing(TransmitModel& tm, bool on)
{
    tm.setTxEqEnabled(on);
    tm.setCpdrOn(on);
    tm.setTxLevelerOn(on);
    tm.setCfcEnabled(on);
    tm.setPhaseRotatorEnabled(on);
}

}  // namespace

class TestRecordingOnAir : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    int m_disks = 0;

    void useDisk(RecordingService& svc)
    {
        const QString disk = m_dir.filePath(QStringLiteral("disk%1").arg(++m_disks));
        QDir().mkpath(disk);
        RecordingSettings s = svc.settings();
        s.disk = disk;
        s.format = AudioFileFormat::Wav;
        svc.setSettings(s);
    }

    std::unique_ptr<RecordingService> makeService()
    {
        auto svc = std::make_unique<RecordingService>(nullptr);
        useDisk(*svc);
        return svc;
    }

    // A 48 kHz ramp recording of `frames` frames; its id.
    static QString addRamp(RecordingService& svc, int frames)
    {
        RecordingStore* store = svc.store();
        if (store == nullptr || store->folder().isEmpty()) {
            return {};
        }
        static int n = 0;
        const QString name = QStringLiteral("ramp-%1").arg(++n);
        const QString fileName = name + QStringLiteral(".wav");
        QString error;
        std::unique_ptr<AudioFileWriter> w = AudioFileWriter::open(
            QDir(store->folder()).filePath(fileName), AudioFileSpec{AudioFileFormat::Wav, kRate, 1, 0}, &error);
        const std::vector<float> v = ramp(frames);
        if (!w || !w->write(v.data(), frames) || !w->finish()) {
            return {};
        }
        RecordingEntry e;
        e.kind = RecordingKind::Contact;
        e.sliceId = 0;
        e.startUtc = QDateTime(QDate(2026, 11, 15), QTime(14, 32, 0), QTimeZone::UTC);
        e.lengthMs = static_cast<qint64>(frames) * 1000 / kRate;
        e.frequencyHz = 7236400;
        e.mode = QStringLiteral("LSB");
        e.name = name;
        e.fileName = fileName;
        return store->add(e);
    }

    static int speaker(RecordingService& svc, int slot, std::vector<float>& buf)
    {
        buf.assign(kBlock, -1.0f);
        return svc.pullSpeaker(slot, buf.data(), kBlock);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        AppSettings::instance().clear();
        // The TX channels here are never opened in WDSP; the switches still
        // reach them, and their "not initialized" notice is expected.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp.warning=false"));
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ---- V5, A6: keying sends the playback from where it plays.

    void keyingSendsThePlaybackFromWhereItPlays()
    {
        auto svc = makeService();
        const int frames = 2 * kRate;
        const QString id = addRamp(*svc, frames);
        QVERIFY(!id.isEmpty());
        svc->play(kLocal, id);
        svc->pumpForTest();

        std::vector<float> buf;
        for (int b = 0; b < 10; ++b) {
            QCOMPARE(speaker(*svc, AudioEngine::kLocalPlaybackSlotId, buf), kBlock);
        }
        QVERIFY(near(buf.back(), rampAt(10 * kBlock - 1, frames)));
        QVERIFY(!svc->playback(kLocal)->onAir);

        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        OnAirSource* onAir = svc->onAirSource();
        svc->keyChanged(true, kLocal);
        QCOMPARE(onAir->active(), OnAirSource::Kind::Playback);
        QCOMPARE(onAir->playbackListener().value_or(QString()), kLocal);
        QVERIFY(svc->playback(kLocal)->onAir);
        QVERIFY(svc->playback(kLocal)->playing);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().first().toString(), kLocal);

        // The transmitter takes it on from the next sample.
        std::vector<float> tx(kBlock, -1.0f);
        QVERIFY(onAir->pull(tx.data(), kBlock));
        QVERIFY(near(tx.front(), rampAt(10 * kBlock, frames)));
        QVERIFY(near(tx.back(), rampAt(11 * kBlock - 1, frames)));
        // Its speakers get nothing while it goes out.
        QCOMPARE(speaker(*svc, AudioEngine::kLocalPlaybackSlotId, buf), 0);
        for (const float s : buf) {
            QCOMPARE(s, 0.0f);
        }
        for (int b = 0; b < 4; ++b) {
            QVERIFY(onAir->pull(tx.data(), kBlock));
        }
        QVERIFY(near(tx.back(), rampAt(15 * kBlock - 1, frames)));

        // A6: unkeyed, it goes on in the speaker from where it got to.
        const qint64 before = svc->playback(kLocal)->positionMs;
        svc->keyChanged(false, kLocal);
        QCOMPARE(onAir->active(), OnAirSource::Kind::None);
        QVERIFY(!onAir->pull(tx.data(), kBlock));
        QVERIFY(!svc->playback(kLocal)->onAir);
        QVERIFY(svc->playback(kLocal)->playing);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(speaker(*svc, AudioEngine::kLocalPlaybackSlotId, buf), kBlock);
        QVERIFY(near(buf.front(), rampAt(15 * kBlock, frames)));
        QVERIFY(svc->playback(kLocal)->positionMs >= before);
        QCOMPARE(before, qint64(15 * kBlock) * 1000 / kRate);
    }

    // ---- V6, A6: another device keying sends its own microphone.

    void anotherDeviceKeyingSendsItsMic()
    {
        auto svc = makeService();
        const int frames = 2 * kRate;
        const QString id = addRamp(*svc, frames);
        svc->play(kLocal, id);
        svc->pumpForTest();
        std::vector<float> buf;
        QCOMPARE(speaker(*svc, AudioEngine::kLocalPlaybackSlotId, buf), kBlock);

        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        svc->keyChanged(true, kPhone);
        OnAirSource* onAir = svc->onAirSource();
        QCOMPARE(onAir->active(), OnAirSource::Kind::None);
        std::vector<float> tx(kBlock);
        QVERIFY(!onAir->pull(tx.data(), kBlock));
        QVERIFY(!svc->playback(kLocal)->onAir);
        QCOMPARE(changed.count(), 0);
        // The local window keeps hearing its own playback.
        QCOMPARE(speaker(*svc, AudioEngine::kLocalPlaybackSlotId, buf), kBlock);
        QVERIFY(near(buf.front(), rampAt(kBlock, frames)));
        svc->keyChanged(false, kPhone);
        QCOMPARE(speaker(*svc, AudioEngine::kLocalPlaybackSlotId, buf), kBlock);
        QVERIFY(near(buf.front(), rampAt(2 * kBlock, frames)));

        // And the other way round: the phone plays, the window keys.
        const QString other = addRamp(*svc, frames);
        svc->setListenerSpeakerSlot(kPhone, kPhoneSlot);
        svc->play(kPhone, other);
        svc->pumpForTest();
        svc->keyChanged(true, kLocal);
        QCOMPARE(onAir->playbackListener().value_or(QString()), kLocal);
        QVERIFY(!svc->playback(kPhone)->onAir);
        QCOMPARE(speaker(*svc, kPhoneSlot, buf), kBlock);
        QVERIFY(near(buf.front(), 0.0f));
        svc->keyChanged(false, kLocal);
    }

    void listenersForKeys()
    {
        QCOMPARE(RecordingService::listenerForKey(KeyerIdentity::station(PttMode::Mic), false).value_or(QString()),
                 kLocal);
        QCOMPARE(RecordingService::listenerForKey(KeyerIdentity::station(PttMode::None), false).value_or(QString()),
                 kLocal);
        KeyerIdentity phone;
        phone.deviceId = kPhone.toLatin1();
        QCOMPARE(RecordingService::listenerForKey(phone, false).value_or(QString()), kPhone);
        // A program's key (TCI, CAT), TUNE and CW send no recording.
        KeyerIdentity program = KeyerIdentity::station(PttMode::Tci);
        program.program = true;
        QVERIFY(!RecordingService::listenerForKey(program, false).has_value());
        QVERIFY(!RecordingService::listenerForKey(KeyerIdentity::station(PttMode::Manual), false).has_value());
        QVERIFY(!RecordingService::listenerForKey(KeyerIdentity::station(PttMode::None), true).has_value());
        QVERIFY(!RecordingService::listenerForKey(KeyerIdentity::radioKey(), false).has_value());
        // VOX neither: the playback on the speakers could key it.
        QVERIFY(!RecordingService::listenerForKey(KeyerIdentity::station(PttMode::Vox), false).has_value());
    }

    // ---- V5: the end of the recording hands over to the microphone.

    void theEndWhileKeyedHandsOverWithinOneBlock()
    {
        auto svc = makeService();
        const int frames = 75 * kBlock + 10;   // the last block is partly the recording
        const QString id = addRamp(*svc, frames);
        svc->play(kLocal, id);
        svc->pumpForTest();
        svc->keyChanged(true, kLocal);
        OnAirSource* onAir = svc->onAirSource();
        std::vector<float> tx(kBlock);
        for (int b = 0; b < 75; ++b) {
            QVERIFY(onAir->pull(tx.data(), kBlock));
            QVERIFY(near(tx.front(), rampAt(b * kBlock, frames)));
        }
        QVERIFY(onAir->pull(tx.data(), kBlock));
        QVERIFY(near(tx[9], rampAt(75 * kBlock + 9, frames)));
        for (int i = 10; i < kBlock; ++i) {
            QCOMPARE(tx[static_cast<size_t>(i)], 0.0f);
        }
        // The very next block is the microphone's.
        QVERIFY(!onAir->pull(tx.data(), kBlock));
        // The service learns of the end and stops sending; nothing unkeys.
        // It tells the change once: not playing, not on the air.
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        bool toldPlaying = true;
        bool toldOnAir = true;
        connect(svc.get(), &RecordingService::playbackChanged, this, [&](const ListenerId& who) {
            toldPlaying = svc->playback(who)->playing;
            toldOnAir = svc->playback(who)->onAir;
        });
        svc->pumpForTest();
        QCOMPARE(onAir->active(), OnAirSource::Kind::None);
        QVERIFY(!svc->playback(kLocal)->playing);
        QVERIFY(!svc->playback(kLocal)->onAir);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!toldPlaying);
        QVERIFY(!toldOnAir);
    }

    void theEndWhileKeyedLeavesTheRadioKeyed()
    {
        RadioModel model;
        RecordingService* svc = model.recordingService();
        QVERIFY(svc != nullptr);
        useDisk(*svc);
        MoxController* mox = model.moxController();
        model.setCapsForTest(/*hasAlex=*/false);
        Test::QuietRadioConnection connection;
        model.injectConnectionForTest(&connection);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.addSlice();
        TxChannel ch(kTxChannelId);
        model.injectTxChannelForTest(&ch);
        const QString id = addRamp(*svc, 10 * kBlock);
        svc->play(kLocal, id);
        svc->pumpForTest();

        mox->setMox(true);
        QVERIFY(mox->isMox());
        QCOMPARE(svc->onAirSource()->active(), OnAirSource::Kind::Playback);
        // The station's own key is the local window's.
        QVERIFY(svc->playback(kLocal)->onAir);
        std::vector<float> tx(kBlock);
        int sent = 0;
        while (svc->onAirSource()->pull(tx.data(), kBlock)) {
            ++sent;
            QVERIFY(sent <= 10);
        }
        QCOMPARE(sent, 10);
        svc->pumpForTest();
        QCOMPARE(svc->onAirSource()->active(), OnAirSource::Kind::None);
        QVERIFY(mox->isMox());
        mox->setMox(false);
        QTRY_VERIFY(!mox->isMox());
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
    }

    // ---- Another station holds transmit: the key is refused as today.

    void aRefusedKeyLeavesThePlaybackOnItsSpeaker()
    {
        RadioModel model;
        RecordingService* svc = model.recordingService();
        QVERIFY(svc != nullptr);
        useDisk(*svc);
        MoxController* mox = model.moxController();
        model.setCapsForTest(/*hasAlex=*/false);
        Test::QuietRadioConnection connection;
        model.injectConnectionForTest(&connection);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.addSlice();
        mox->setKeyingGate([](PttMode, const KeyerIdentity&) {
            KeyingAnswer refuse;
            refuse.verdict = KeyingVerdict::Refuse;
            return refuse;
        });
        const int frames = 2 * kRate;
        const QString id = addRamp(*svc, frames);
        svc->setListenerSpeakerSlot(kPhone, kPhoneSlot);
        svc->play(kPhone, id);
        svc->pumpForTest();
        std::vector<float> buf;
        QCOMPARE(speaker(*svc, kPhoneSlot, buf), kBlock);

        KeyerIdentity phone;
        phone.deviceId = kPhone.toLatin1();
        mox->setMox(true, phone);
        QVERIFY(!mox->isMox());
        QCOMPARE(svc->onAirSource()->active(), OnAirSource::Kind::None);
        QVERIFY(!svc->playback(kPhone)->onAir);
        QCOMPARE(speaker(*svc, kPhoneSlot, buf), kBlock);
        QVERIFY(near(buf.front(), rampAt(kBlock, frames)));
        model.injectConnectionForTest(nullptr);
    }

    // ---- A7: the five switches.

    void theSwitchesAreOffOnTheAirAndComeBackAfter()
    {
        for (const bool userOn : {true, false}) {
            auto svc = makeService();
            TransmitModel tm;
            TxChannel ch(kTxChannelId);
            setAllProcessing(tm, userOn);
            wireLikeRadioModel(tm, ch);
            svc->onAirSource()->setTransmitChain([&ch] { return &ch; }, &tm);
            const Switches user = switchesOf(tm);
            QCOMPARE(switchesOf(ch), user);
            const QString id = addRamp(*svc, 2 * kRate);
            svc->play(kLocal, id);
            svc->pumpForTest();

            QSignalSpy eq(&tm, &TransmitModel::txEqEnabledChanged);
            QSignalSpy cpdr(&tm, &TransmitModel::cpdrOnChanged);
            QSignalSpy lev(&tm, &TransmitModel::txLevelerOnChanged);
            QSignalSpy cfc(&tm, &TransmitModel::cfcEnabledChanged);
            QSignalSpy phrot(&tm, &TransmitModel::phaseRotatorEnabledChanged);
            svc->keyChanged(true, kLocal);
            QCOMPARE(switchesOf(ch), (Switches{false, false, false, false, false}));
            QCOMPARE(switchesOf(tm), user);
            svc->keyChanged(false, kLocal);
            QCOMPARE(switchesOf(ch), user);
            QCOMPARE(switchesOf(tm), user);
            // TransmitModel was never written.
            QCOMPARE(eq.count() + cpdr.count() + lev.count() + cfc.count() + phrot.count(), 0);
        }
    }

    void aSwitchSetToUseKeepsTheUsersValue()
    {
        auto svc = makeService();
        RecordingSettings s = svc->settings();
        QVERIFY(!s.onAirUseCompressor);   // off by default, as are the other four
        QVERIFY(!s.onAirUseTxEq && !s.onAirUseLeveler && !s.onAirUseCfc && !s.onAirUsePhaseRotator);
        s.onAirUseCompressor = true;
        svc->setSettings(s);
        TransmitModel tm;
        TxChannel ch(kTxChannelId);
        setAllProcessing(tm, true);
        wireLikeRadioModel(tm, ch);
        svc->onAirSource()->setTransmitChain([&ch] { return &ch; }, &tm);
        svc->play(kLocal, addRamp(*svc, 2 * kRate));
        svc->pumpForTest();
        svc->keyChanged(true, kLocal);
        QCOMPARE(switchesOf(ch), (Switches{false, true, false, false, false}));
        svc->keyChanged(false, kLocal);
        QCOMPARE(switchesOf(ch), (Switches{true, true, true, true, true}));
    }

    void aChangeOnTheAirIsKeptAndAppliedAtTheEnd()
    {
        auto svc = makeService();
        TransmitModel tm;
        TxChannel ch(kTxChannelId);
        setAllProcessing(tm, false);
        wireLikeRadioModel(tm, ch);
        svc->onAirSource()->setTransmitChain([&ch] { return &ch; }, &tm);
        svc->play(kLocal, addRamp(*svc, 2 * kRate));
        svc->pumpForTest();
        svc->keyChanged(true, kLocal);
        tm.setTxEqEnabled(true);
        tm.setCfcEnabled(true);
        QVERIFY(tm.txEqEnabled() && tm.cfcEnabled());
        QCOMPARE(switchesOf(ch), (Switches{false, false, false, false, false}));
        svc->keyChanged(false, kLocal);
        QCOMPARE(switchesOf(ch), (Switches{true, false, false, true, false}));
    }

    // ---- Keying while nothing plays changes nothing.

    void keyingWhileNothingPlaysChangesNothing()
    {
        auto svc = makeService();
        TransmitModel tm;
        TxChannel ch(kTxChannelId);
        setAllProcessing(tm, true);
        wireLikeRadioModel(tm, ch);
        svc->onAirSource()->setTransmitChain([&ch] { return &ch; }, &tm);
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        svc->keyChanged(true, kLocal);
        QCOMPARE(svc->onAirSource()->active(), OnAirSource::Kind::None);
        std::vector<float> tx(kBlock);
        QVERIFY(!svc->onAirSource()->pull(tx.data(), kBlock));
        QCOMPARE(switchesOf(ch), (Switches{true, true, true, true, true}));
        svc->keyChanged(false, kLocal);
        QCOMPARE(switchesOf(ch), (Switches{true, true, true, true, true}));
        QCOMPARE(changed.count(), 0);
    }

    // ---- The transmitter's input: on the air ahead of every microphone.

    void theTransmitWorkerSendsThePlaybackThenTheMic()
    {
        auto svc = makeService();
        Test::QuietRadioConnection connection;
        TxChannel channel(kTxChannelId, kBlock, kBlock);
        channel.setConnection(&connection);
        channel.setRunning(true);
        TxWorkerThread worker;
        worker.setTxChannel(&channel);
        worker.setOnAirSource(svc->onAirSource());
        const std::vector<float> mic(kBlock, 0.3f);
        const auto lastI = [&channel](int k) {
            return static_cast<float>(channel.inForTest()[static_cast<size_t>(2 * k)]);
        };

        worker.dispatchBlockForTest(mic.data());
        QVERIFY(near(lastI(0), 0.3f));

        const int frames = 3 * kBlock;
        svc->play(kLocal, addRamp(*svc, frames));
        svc->pumpForTest();
        // Playing alone sends nothing.
        worker.dispatchBlockForTest(mic.data());
        QVERIFY(near(lastI(0), 0.3f));

        // Keyed, but the TX channel is not running yet (the rf delay): the
        // recording stays where its speakers left it.
        channel.setRunning(false);
        svc->keyChanged(true, kLocal);
        QVERIFY(svc->playback(kLocal)->onAir);
        for (int b = 0; b < 3; ++b) {
            worker.dispatchBlockForTest(mic.data());
        }
        channel.setRunning(true);
        for (int b = 0; b < 3; ++b) {
            worker.dispatchBlockForTest(mic.data());
            QVERIFY(near(lastI(0), rampAt(b * kBlock, frames)));
            QVERIFY(near(lastI(kBlock - 1), rampAt(b * kBlock + kBlock - 1, frames)));
            QCOMPARE(channel.inForTest()[1], 0.0);
        }
        // The recording is over: the next block is the microphone's.
        worker.dispatchBlockForTest(mic.data());
        QVERIFY(near(lastI(0), 0.3f));
        svc->keyChanged(false, kLocal);
        worker.setOnAirSource(nullptr);
        channel.setRunning(false);
    }

    // ---- pull() takes no lock and allocates nothing.

    void pullNeitherAllocatesNorWaits()
    {
        auto svc = makeService();
        svc->play(kLocal, addRamp(*svc, 2 * kRate));
        svc->pumpForTest();
        svc->keyChanged(true, kLocal);
        OnAirSource* onAir = svc->onAirSource();
        std::vector<float> tx(kBlock);
        g_allocations.store(0);
        g_countAllocations = true;
        for (int b = 0; b < 200; ++b) {
            onAir->pull(tx.data(), kBlock);
        }
        g_countAllocations = false;
        QCOMPARE(g_allocations.load(), 0);
        svc->keyChanged(false, kLocal);

        // A voice keyer message, the same way.
        std::vector<float> clip(10 * kBlock, 0.25f);
        int at = 0;
        // The counter sees allocations made inside the library: starting a
        // message allocates there (its holder), so a zero below means none.
        g_allocations.store(0);
        g_countAllocations = true;
        onAir->beginVoiceMessage(QStringLiteral("M1"), [&clip, &at](float* out, int frames) {
            const int n = std::min(frames, static_cast<int>(clip.size()) - at);
            std::copy(clip.begin() + at, clip.begin() + at + n, out);
            at += n;
            return n;
        });
        g_countAllocations = false;
        QVERIFY(g_allocations.load() > 0);
        QCOMPARE(onAir->active(), OnAirSource::Kind::VoiceMessage);
        QCOMPARE(onAir->voiceMessageSlot(), QStringLiteral("M1"));
        g_allocations.store(0);
        g_countAllocations = true;
        int sent = 0;
        while (onAir->pull(tx.data(), kBlock)) {
            ++sent;
        }
        g_countAllocations = false;
        QCOMPARE(g_allocations.load(), 0);
        QCOMPARE(sent, 10);
        onAir->end();
        QCOMPARE(onAir->active(), OnAirSource::Kind::None);
        QVERIFY(onAir->voiceMessageSlot().isEmpty());
    }

    // ---- A seek on the air: the gap between two players is silence.

    void aSeekOnTheAirSendsNoMicrophone()
    {
        auto svc = makeService();
        const int frames = 4 * kRate;
        svc->play(kLocal, addRamp(*svc, frames));
        svc->pumpForTest();
        svc->keyChanged(true, kLocal);
        OnAirSource* onAir = svc->onAirSource();
        QCOMPARE(onAir->active(), OnAirSource::Kind::Playback);

        // The transmit worker pulls the whole time the seeks run.
        std::atomic<bool> stop{false};
        std::atomic<int> pulls{0};
        std::atomic<int> micBlocks{0};
        std::thread transmitter([&] {
            std::vector<float> tx(kBlock);
            while (!stop.load()) {
                if (!onAir->pull(tx.data(), kBlock)) {
                    micBlocks.fetch_add(1);
                }
                pulls.fetch_add(1);
            }
        });
        for (int k = 0; k < 200; ++k) {
            svc->seek(kLocal, (k % 30) * 100);
        }
        QTRY_VERIFY(pulls.load() > 200);
        stop.store(true);
        transmitter.join();
        QCOMPARE(micBlocks.load(), 0);
        QCOMPARE(onAir->active(), OnAirSource::Kind::Playback);

        // After the last seek the air carries the recording from there.
        svc->seek(kLocal, 1000);
        svc->pumpForTest();
        std::vector<float> tx(kBlock);
        QVERIFY(onAir->pull(tx.data(), kBlock));
        QVERIFY(near(tx.front(), rampAt(kRate, frames)));
        svc->keyChanged(false, kLocal);
    }

    // ---- VOX and CW keys leave the playback on its speaker.

    void voxAndCwKeysLeaveThePlaybackOnItsSpeaker()
    {
        for (const bool cw : {false, true}) {
            TxChannel ch(kTxChannelId);
            RadioModel model;
            RecordingService* svc = model.recordingService();
            QVERIFY(svc != nullptr);
            useDisk(*svc);
            MoxController* mox = model.moxController();
            model.setCapsForTest(/*hasAlex=*/false);
            Test::QuietRadioConnection connection;
            model.injectConnectionForTest(&connection);
            mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
            model.addSlice();
            model.injectTxChannelForTest(&ch);
            // The five as the user has them; a key that sends nothing out
            // must not touch them.
            ch.setTxEqRunning(true);
            ch.setTxCpdrOn(true);
            ch.setTxLevelerOn(true);
            ch.setTxCfcRunning(true);
            ch.setStageRunning(TxChannel::Stage::PhRot, true);
            const int frames = 2 * kRate;
            svc->play(kLocal, addRamp(*svc, frames));
            svc->pumpForTest();
            std::vector<float> buf;
            QCOMPARE(speaker(*svc, AudioEngine::kLocalPlaybackSlotId, buf), kBlock);

            if (cw) {
                mox->setCwModeActive(true);
                mox->setMox(true);
                QVERIFY(mox->isCwKeying());
            } else {
                mox->setMox(true, KeyerIdentity::station(PttMode::Vox));
            }
            QVERIFY2(mox->isMox(), qPrintable(mox->lastRefusal().text));
            QCOMPARE(svc->onAirSource()->active(), OnAirSource::Kind::None);
            QVERIFY(!svc->playback(kLocal)->onAir);
            QCOMPARE(speaker(*svc, AudioEngine::kLocalPlaybackSlotId, buf), kBlock);
            QVERIFY(near(buf.front(), rampAt(kBlock, frames)));
            QCOMPARE(switchesOf(ch), (Switches{true, true, true, true, true}));
            mox->setMox(false);
            QTRY_VERIFY(!mox->isMox());
            if (cw) {
                mox->setCwModeActive(false);
            }
            model.injectTxChannelForTest(nullptr);
            model.injectConnectionForTest(nullptr);
        }
    }

    // ---- RadioModel's own writers do not turn a held switch back on.

    void aProfileChangeOnTheAirKeepsTheSwitchesOff()
    {
        TxChannel ch(kTxChannelId);
        RadioModel model;
        RecordingService* svc = model.recordingService();
        QVERIFY(svc != nullptr);
        useDisk(*svc);
        TransmitModel& tm = model.transmitModel();
        setAllProcessing(tm, true);
        // The Core's transmit chain wiring, as connectToRadio runs it.
        model.wireTransmitChainForTest(&ch);
        QCOMPARE(switchesOf(ch), (Switches{true, true, true, true, true}));
        MicProfileManager* profiles = model.micProfileManager();
        QVERIFY(profiles != nullptr);
        svc->play(kLocal, addRamp(*svc, 2 * kRate));
        svc->pumpForTest();

        svc->keyChanged(true, kLocal);
        QCOMPARE(switchesOf(ch), (Switches{false, false, false, false, false}));
        // A profile pick pushes the whole chain from TransmitModel.
        emit profiles->activeProfileChanged(QStringLiteral("Default"));
        QCOMPARE(switchesOf(ch), (Switches{false, false, false, false, false}));
        // So does a change to one of them.
        tm.setTxEqEnabled(false);
        tm.setTxEqEnabled(true);
        QCOMPARE(switchesOf(ch), (Switches{false, false, false, false, false}));
        svc->keyChanged(false, kLocal);
        QCOMPARE(switchesOf(ch), (Switches{true, true, true, true, true}));
        model.injectTxChannelForTest(nullptr);
    }

    // ---- Stopping a playback that is on the air.

    void stoppingOnTheAirEndsItAndRestores()
    {
        auto svc = makeService();
        TransmitModel tm;
        TxChannel ch(kTxChannelId);
        setAllProcessing(tm, true);
        wireLikeRadioModel(tm, ch);
        svc->onAirSource()->setTransmitChain([&ch] { return &ch; }, &tm);
        svc->play(kLocal, addRamp(*svc, 2 * kRate));
        svc->pumpForTest();
        svc->keyChanged(true, kLocal);
        QCOMPARE(svc->onAirSource()->active(), OnAirSource::Kind::Playback);
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        svc->stopPlayback(kLocal);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(svc->onAirSource()->active(), OnAirSource::Kind::None);
        QCOMPARE(switchesOf(ch), (Switches{true, true, true, true, true}));
        std::vector<float> tx(kBlock);
        QVERIFY(!svc->onAirSource()->pull(tx.data(), kBlock));
        svc->keyChanged(false, kLocal);
    }
    // ---- Task 10b: a recording goes out at a fixed level, not the mic gain.

    void aRecordingGoesOutAtTheFixedLevel()
    {
        TxChannel ch(kTxChannelId);
        RadioModel model;
        RecordingService* svc = model.recordingService();
        QVERIFY(svc != nullptr);
        useDisk(*svc);
        TransmitModel& tm = model.transmitModel();
        tm.setMicMute(true);   // Thetis name: true = mic in use
        tm.setMicGainDb(20);
        model.wireTransmitChainForTest(&ch);
        const double micGain = std::pow(10.0, 20.0 / 20.0);
        QCOMPARE(ch.lastMicPreampForTest(), micGain);
        svc->play(kLocal, addRamp(*svc, 2 * kRate));
        svc->pumpForTest();
        QSignalSpy gainChanged(&tm, &TransmitModel::micPreampChanged);

        svc->keyChanged(true, kLocal);
        QCOMPARE(svc->onAirSource()->active(), OnAirSource::Kind::Playback);
        std::vector<float> tx(kBlock);
        for (int block = 0; block < 8; ++block) {
            QVERIFY(svc->onAirSource()->pull(tx.data(), kBlock));
            QCOMPARE(ch.lastMicPreampForTest(), 1.0);
        }
        svc->keyChanged(false, kLocal);
        QCOMPARE(ch.lastMicPreampForTest(), micGain);
        // TransmitModel was never written.
        QCOMPARE(gainChanged.count(), 0);
        QCOMPARE(tm.micGainDb(), 20);
        model.injectTxChannelForTest(nullptr);
    }

    void aMicGainChangeOnTheAirWaitsForTheEnd()
    {
        TxChannel ch(kTxChannelId);
        RadioModel model;
        RecordingService* svc = model.recordingService();
        QVERIFY(svc != nullptr);
        useDisk(*svc);
        TransmitModel& tm = model.transmitModel();
        tm.setMicMute(true);
        tm.setMicGainDb(0);
        model.wireTransmitChainForTest(&ch);
        svc->play(kLocal, addRamp(*svc, 2 * kRate));
        svc->pumpForTest();

        svc->keyChanged(true, kLocal);
        QCOMPARE(ch.lastMicPreampForTest(), 1.0);
        tm.setMicGainDb(-20);
        QCOMPARE(ch.lastMicPreampForTest(), 1.0);
        tm.setMicGainDb(10);
        QCOMPARE(ch.lastMicPreampForTest(), 1.0);
        svc->keyChanged(false, kLocal);
        QCOMPARE(ch.lastMicPreampForTest(), std::pow(10.0, 10.0 / 20.0));
        model.injectTxChannelForTest(nullptr);
    }

    void aMutedMicDoesNotMuteARecording()
    {
        TxChannel ch(kTxChannelId);
        RadioModel model;
        RecordingService* svc = model.recordingService();
        QVERIFY(svc != nullptr);
        useDisk(*svc);
        TransmitModel& tm = model.transmitModel();
        tm.setMicGainDb(6);
        tm.setMicMute(false);   // muted
        model.wireTransmitChainForTest(&ch);
        QCOMPARE(ch.lastMicPreampForTest(), 0.0);
        svc->play(kLocal, addRamp(*svc, 2 * kRate));
        svc->pumpForTest();

        svc->keyChanged(true, kLocal);
        QCOMPARE(ch.lastMicPreampForTest(), 1.0);
        // Muting or unmuting on the air does not move it either.
        tm.setMicMute(true);
        QCOMPARE(ch.lastMicPreampForTest(), 1.0);
        tm.setMicMute(false);
        QCOMPARE(ch.lastMicPreampForTest(), 1.0);
        svc->keyChanged(false, kLocal);
        QCOMPARE(ch.lastMicPreampForTest(), 0.0);
        model.injectTxChannelForTest(nullptr);
    }

    void aVoiceMessageKeepsTheMicGain()
    {
        TxChannel ch(kTxChannelId);
        RadioModel model;
        RecordingService* svc = model.recordingService();
        QVERIFY(svc != nullptr);
        useDisk(*svc);
        TransmitModel& tm = model.transmitModel();
        tm.setMicMute(true);
        tm.setMicGainDb(20);
        model.wireTransmitChainForTest(&ch);
        const double micGain = std::pow(10.0, 20.0 / 20.0);
        OnAirSource* onAir = svc->onAirSource();
        onAir->beginVoiceMessage(QStringLiteral("M1"), [](float* out, int frames) {
            std::fill(out, out + frames, 0.25f);
            return frames;
        });
        QCOMPARE(onAir->active(), OnAirSource::Kind::VoiceMessage);
        std::vector<float> tx(kBlock);
        QVERIFY(onAir->pull(tx.data(), kBlock));
        QCOMPARE(ch.lastMicPreampForTest(), micGain);
        tm.setMicGainDb(10);
        QCOMPARE(ch.lastMicPreampForTest(), std::pow(10.0, 10.0 / 20.0));
        onAir->end();
        QCOMPARE(ch.lastMicPreampForTest(), std::pow(10.0, 10.0 / 20.0));
        model.injectTxChannelForTest(nullptr);
    }
};

QTEST_MAIN(TestRecordingOnAir)
#include "tst_recording_on_air.moc"
