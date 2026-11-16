// =================================================================
// tests/tst_recording_player.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The recording player (spec 6.1
// Player, 6.3, V6, A5 for the Core): the mixer's playback slot reaches only
// its owner's speakers, playback is resampled to 48 kHz, seek, the end, a
// whole-pan recording refused, and the slot never holds up the drain.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/AudioEngine.h"
#include "core/audio/MasterMixer.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingPlayer.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingStore.h"

#include <cmath>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kLocalSlot = -3;
constexpr int kOwnerASlot = -4;
constexpr int kOwnerBSlot = -5;
constexpr int kBlock = 480;   // 10 ms at 48 kHz
constexpr double kPi = 3.14159265358979323846;

bool writeWav(const QString& path, int rateHz, const std::vector<float>& mono)
{
    QString error;
    std::unique_ptr<AudioFileWriter> w =
        AudioFileWriter::open(path, AudioFileSpec{AudioFileFormat::Wav, rateHz, 1, 0}, &error);
    if (!w) {
        return false;
    }
    return w->write(mono.data(), static_cast<int>(mono.size())) && w->finish();
}

std::vector<float> toStereo(const float* mono, int frames)
{
    std::vector<float> s(static_cast<size_t>(frames) * 2);
    for (int i = 0; i < frames; ++i) {
        s[static_cast<size_t>(2 * i)] = mono[i];
        s[static_cast<size_t>(2 * i + 1)] = mono[i];
    }
    return s;
}

double peak(const std::vector<float>& v, int samples)
{
    double p = 0.0;
    for (int i = 0; i < samples; ++i) {
        p = std::max(p, std::fabs(static_cast<double>(v[static_cast<size_t>(i)])));
    }
    return p;
}

}  // namespace

class TestRecordingPlayer : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    int m_benchCount = 0;

    std::unique_ptr<RecordingService> makeService()
    {
        auto svc = std::make_unique<RecordingService>(nullptr);
        const QString disk = m_dir.filePath(QStringLiteral("disk%1").arg(++m_benchCount));
        QDir().mkpath(disk);
        RecordingSettings s = svc->settings();
        s.disk = disk;
        s.format = AudioFileFormat::Wav;
        svc->setSettings(s);
        return svc;
    }

    // A file in the service's folder and its row; the id.
    static QString addRecording(RecordingService& svc, const QString& name, int rateHz,
                                const std::vector<float>& mono,
                                RecordingKind kind = RecordingKind::Contact)
    {
        RecordingStore* store = svc.store();
        if (!store || store->folder().isEmpty()) {
            return {};
        }
        const QString fileName = name + QStringLiteral(".wav");
        if (!writeWav(QDir(store->folder()).filePath(fileName), rateHz, mono)) {
            return {};
        }
        RecordingEntry e;
        e.kind = kind;
        e.sliceId = kind == RecordingKind::WholePan ? -1 : 0;
        e.startUtc = QDateTime(QDate(2026, 11, 15), QTime(14, 32, 0), QTimeZone::UTC);
        e.lengthMs = static_cast<qint64>(mono.size()) * 1000 / rateHz;
        e.frequencyHz = 7236400;
        e.mode = QStringLiteral("LSB");
        e.name = name;
        e.fileName = fileName;
        return store->add(e);
    }

    static std::vector<float> tone(int rateHz, double hz, double seconds, float amplitude = 0.5f)
    {
        std::vector<float> v(static_cast<size_t>(rateHz * seconds));
        for (size_t i = 0; i < v.size(); ++i) {
            v[i] = amplitude * static_cast<float>(std::sin(2.0 * kPi * hz * static_cast<double>(i) / rateHz));
        }
        return v;
    }

    // value = 0.9 * t / length: the sample says where in the file it is.
    static std::vector<float> ramp(int rateHz, double seconds)
    {
        std::vector<float> v(static_cast<size_t>(rateHz * seconds));
        for (size_t i = 0; i < v.size(); ++i) {
            v[i] = 0.9f * static_cast<float>(i) / static_cast<float>(v.size());
        }
        return v;
    }

    static void setUpPlaybackMixer(MasterMixer& mix)
    {
        mix.setRampFrames(1);
        mix.setSlewUpFrames(0);
        mix.setSliceGain(0, 1.0f, 0.0f);
        for (const int slot : {kLocalSlot, kOwnerASlot, kOwnerBSlot}) {
            mix.setSliceGain(slot, 1.0f, 0.0f);
            mix.setPlaybackSlot(slot, true);
        }
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    // ---- The mixer's playback slot (Step 1).

    void playbackSlotReachesOnlyItsOwnersSpeakers()
    {
        MasterMixer mix;
        setUpPlaybackMixer(mix);
        constexpr int n = 64;
        std::vector<float> slice(n * 2, 0.1f);
        std::vector<float> play(n * 2, 0.5f);
        mix.accumulate(0, slice.data(), n);
        mix.accumulate(kOwnerASlot, play.data(), n);

        std::vector<float> spk(n * 2), hp(n * 2), radio(n * 2), every(n * 2);
        std::vector<float> aSpk(n * 2), aHp(n * 2), bSpk(n * 2), bHp(n * 2);
        MasterMixer::OwnerOutput owners[2];
        owners[0].speakers = aSpk.data();
        owners[0].headphones = aHp.data();
        owners[0].playbackSlot = kOwnerASlot;
        owners[0].monitor = MasterMixer::OwnerMonitor::Speakers;
        owners[1].speakers = bSpk.data();
        owners[1].headphones = bHp.data();
        owners[1].playbackSlot = kOwnerBSlot;
        owners[1].monitor = MasterMixer::OwnerMonitor::Speakers;
        const int got = mix.tryDrain(spk.data(), hp.data(), n, 0x1u, owners, 2, true, false, 0, nullptr,
                                     radio.data(), every.data(), kLocalSlot);
        QCOMPARE(got, n);
        // A: its playback only (no slice of its own), in its speakers.
        QVERIFY(std::fabs(aSpk[static_cast<size_t>(n)] - 0.5f) < 1e-5f);
        QCOMPARE(peak(aHp, n * 2), 0.0);
        // B and the local sums: none of it.
        QCOMPARE(peak(bSpk, n * 2), 0.0);
        QCOMPARE(peak(bHp, n * 2), 0.0);
        QVERIFY(std::fabs(spk[static_cast<size_t>(n)] - 0.1f) < 1e-5f);
        QCOMPARE(peak(hp, n * 2), 0.0);
        // Nor the radio's speaker or the every-slice sum.
        QVERIFY(std::fabs(radio[static_cast<size_t>(n)] - 0.1f) < 1e-5f);
        QVERIFY(std::fabs(every[static_cast<size_t>(n)] - 0.1f) < 1e-5f);
    }

    void localPlaybackSlotReachesOnlyTheLocalSpeakers()
    {
        MasterMixer mix;
        setUpPlaybackMixer(mix);
        constexpr int n = 64;
        std::vector<float> play(n * 2, 0.25f);
        mix.accumulate(kLocalSlot, play.data(), n, false, true);   // a headphones route is ignored
        std::vector<float> spk(n * 2), hp(n * 2), aSpk(n * 2);
        MasterMixer::OwnerOutput owner;
        owner.speakers = aSpk.data();
        owner.playbackSlot = kOwnerASlot;
        QCOMPARE(mix.tryDrain(spk.data(), hp.data(), n, 0xFFFFFFFFu, &owner, 1, true, false, 0, nullptr,
                              nullptr, nullptr, kLocalSlot),
                 n);
        QVERIFY(std::fabs(spk[static_cast<size_t>(n)] - 0.25f) < 1e-5f);
        QCOMPARE(peak(hp, n * 2), 0.0);
        QCOMPARE(peak(aSpk, n * 2), 0.0);
    }

    void playbackSlotNeverHoldsUpTheBarrier()
    {
        MasterMixer mix;
        setUpPlaybackMixer(mix);
        constexpr int n = 64;
        std::vector<float> out(n * 2), aSpk(n * 2);
        MasterMixer::OwnerOutput owner;
        owner.speakers = aSpk.data();
        owner.playbackSlot = kOwnerASlot;
        std::vector<float> slice(n * 2, 0.1f);
        std::vector<float> play(n * 2, 0.5f);

        // A short playback block does not shorten the members' drain.
        mix.accumulate(0, slice.data(), n);
        mix.accumulate(kOwnerASlot, play.data(), n / 2);
        QCOMPARE(mix.tryDrain(out.data(), nullptr, n, 0xFFFFFFFFu, &owner, 1), n);
        QCOMPARE(mix.producingSliceCount(), 1);

        // Playback queued while a member is late: no block leaves.
        mix.accumulate(kOwnerASlot, play.data(), n);
        QCOMPARE(mix.tryDrain(out.data(), nullptr, n, 0xFFFFFFFFu, &owner, 1), 0);

        // No members at all: the playback drains by itself.
        mix.setSliceStreaming(0, false);
        mix.setSlewUpFrames(0);
        QCOMPARE(mix.tryDrain(out.data(), nullptr, n, 0xFFFFFFFFu, &owner, 1), n);
        QVERIFY(std::fabs(aSpk[static_cast<size_t>(n)] - 0.5f) < 1e-5f);
    }

    void playbackSlotRefusesASliceId()
    {
        MasterMixer mix;
        mix.setSliceGain(3, 1.0f, 0.0f);
        mix.setPlaybackSlot(3, true);   // ignored: 0..31 are slices
        constexpr int n = 16;
        std::vector<float> in(n * 2, 0.5f), out(n * 2);
        mix.accumulate(3, in.data(), n);
        QCOMPARE(mix.producingSliceCount(), 1);
        QCOMPARE(mix.tryDrain(out.data(), n), n);
        QVERIFY(peak(out, n * 2) > 0.0);
    }

    void ownerMixCarriesItsPlaybackRoute()
    {
        AudioEngine engine;
        const int slot = engine.acquireOwnerMix();
        QVERIFY(slot >= 0);
        QVERIFY(!engine.ownerMixPlayback(slot));
        engine.setOwnerMixPlayback(slot, true);
        QVERIFY(engine.ownerMixPlayback(slot));
        engine.releaseOwnerMix(slot);
        QVERIFY(!engine.ownerMixPlayback(slot));
        QCOMPARE(AudioEngine::ownerPlaybackSlotId(0), kOwnerASlot);
        QCOMPARE(AudioEngine::ownerPlaybackSlotId(1), kOwnerBSlot);
        QCOMPARE(AudioEngine::kLocalPlaybackSlotId, kLocalSlot);
    }

    // ---- The player in the service (Step 2).

    // A5: two owners; A plays; only A's speakers carry it.
    void ownerAHearsItsPlaybackAlone()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("a-tone"), 48000, tone(48000, 1000.0, 1.0));
        QVERIFY(!id.isEmpty());
        svc->play(QStringLiteral("deviceA"), id);
        QVERIFY(svc->playback(QStringLiteral("deviceA")).has_value());
        QVERIFY(svc->playback(QStringLiteral("deviceA"))->playing);
        QVERIFY(!svc->playback(QStringLiteral("deviceB")).has_value());
        QVERIFY(!svc->playback(QStringLiteral("local")).has_value());
        svc->pumpForTest();

        MasterMixer mix;
        setUpPlaybackMixer(mix);
        std::vector<float> spk(kBlock * 2), aSpk(kBlock * 2), bSpk(kBlock * 2);
        MasterMixer::OwnerOutput owners[2];
        owners[0].speakers = aSpk.data();
        owners[0].playbackSlot = kOwnerASlot;
        owners[1].speakers = bSpk.data();
        owners[1].playbackSlot = kOwnerBSlot;
        std::vector<float> mono(kBlock);
        double aPeak = 0.0;
        for (int round = 0; round < 10; ++round) {
            QCOMPARE(svc->pullPlayback(QStringLiteral("deviceA"), mono.data(), kBlock), kBlock);
            std::vector<float> st = toStereo(mono.data(), kBlock);
            mix.accumulate(kOwnerASlot, st.data(), kBlock);
            QCOMPARE(svc->pullPlayback(QStringLiteral("deviceB"), mono.data(), kBlock), 0);
            QCOMPARE(peak(mono, kBlock), 0.0);
            st = toStereo(mono.data(), kBlock);
            mix.accumulate(kOwnerBSlot, st.data(), kBlock);
            QCOMPARE(svc->pullPlayback(QStringLiteral("local"), mono.data(), kBlock), 0);
            st = toStereo(mono.data(), kBlock);
            mix.accumulate(kLocalSlot, st.data(), kBlock);
            QCOMPARE(mix.tryDrain(spk.data(), nullptr, kBlock, 0xFFFFFFFFu, owners, 2, true, false, 0,
                                  nullptr, nullptr, nullptr, kLocalSlot),
                     kBlock);
            aPeak = std::max(aPeak, peak(aSpk, kBlock * 2));
            QCOMPARE(peak(bSpk, kBlock * 2), 0.0);
            QCOMPARE(peak(spk, kBlock * 2), 0.0);
        }
        QVERIFY2(aPeak > 0.45, qPrintable(QString::number(aPeak)));
    }

    // A 24 kHz 1 kHz tone arrives as a 1 kHz tone at 48 kHz.
    void playbackIsResampledTo48k()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("tone24k"), 24000, tone(24000, 1000.0, 1.0));
        QVERIFY(!id.isEmpty());
        const ListenerId who = QStringLiteral("local");
        svc->play(who, id);
        std::vector<float> heard;
        std::vector<float> mono(kBlock);
        for (int round = 0; round < 20 && heard.size() < 36000; ++round) {
            svc->pumpForTest();
            int n = 0;
            do {
                n = svc->pullPlayback(who, mono.data(), kBlock);
                heard.insert(heard.end(), mono.begin(), mono.begin() + n);
            } while (n > 0 && heard.size() < 36000);
        }
        QVERIFY2(heard.size() >= 36000, qPrintable(QString::number(heard.size())));
        // Past the resampler's start, count the rising zero crossings over
        // half a second.
        const size_t from = 4800;
        const size_t span = 24000;
        int rising = 0;
        for (size_t i = from + 1; i < from + span; ++i) {
            if (heard[i - 1] < 0.0f && heard[i] >= 0.0f) {
                ++rising;
            }
        }
        const double hz = rising / (static_cast<double>(span) / RecordingPlayer::kOutputRateHz);
        QVERIFY2(std::fabs(hz - 1000.0) <= 4.0, qPrintable(QString::number(hz)));
        double p = 0.0;
        for (size_t i = from; i < from + span; ++i) {
            p = std::max(p, std::fabs(static_cast<double>(heard[i])));
        }
        QVERIFY2(p > 0.45 && p < 0.55, qPrintable(QString::number(p)));
    }

    // Seek to 5 s of 10 s; the position follows what the mixer took.
    void seekContinuesFromThere()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("ramp"), 48000, ramp(48000, 10.0));
        QVERIFY(!id.isEmpty());
        const ListenerId who = QStringLiteral("deviceA");
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        svc->play(who, id);
        svc->pumpForTest();
        std::vector<float> mono(kBlock);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(svc->pullPlayback(who, mono.data(), kBlock), kBlock);
        }
        QCOMPARE(svc->playback(who)->positionMs, qint64(100));
        QCOMPARE(svc->playback(who)->lengthMs, qint64(10000));

        changed.clear();
        svc->seek(who, 5000);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(svc->playback(who)->positionMs, qint64(5000));
        QVERIFY(svc->playback(who)->playing);
        // Nothing taken, nothing moves, however long the worker runs.
        svc->pumpForTest();
        svc->pumpForTest();
        QCOMPARE(svc->playback(who)->positionMs, qint64(5000));

        QCOMPARE(svc->pullPlayback(who, mono.data(), kBlock), kBlock);
        QVERIFY2(std::fabs(mono[0] - 0.45f) < 0.001f, qPrintable(QString::number(mono[0])));
        for (int i = 1; i < 100; ++i) {
            QCOMPARE(svc->pullPlayback(who, mono.data(), kBlock), kBlock);
            svc->pumpForTest();
        }
        QCOMPARE(svc->playback(who)->positionMs, qint64(6000));
        QVERIFY(std::fabs(mono[kBlock - 1] - 0.9f * 47999.0f / 480000.0f - 0.45f) < 0.001f);
    }

    void theEndStopsAndSaysSoOnce()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("short"), 48000, tone(48000, 440.0, 0.2));
        QVERIFY(!id.isEmpty());
        const ListenerId who = QStringLiteral("deviceA");
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        svc->play(who, id);
        changed.clear();
        std::vector<float> mono(kBlock);
        int total = 0;
        for (int round = 0; round < 50; ++round) {
            svc->pumpForTest();
            total += svc->pullPlayback(who, mono.data(), kBlock);
        }
        svc->pumpForTest();
        QCOMPARE(total, 9600);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), who);
        const std::optional<PlaybackState> st = svc->playback(who);
        QVERIFY(st.has_value());
        QVERIFY(!st->playing);
        QCOMPARE(st->positionMs, qint64(200));
        QCOMPARE(svc->pullPlayback(who, mono.data(), kBlock), 0);
        svc->pumpForTest();
        QCOMPARE(changed.count(), 1);
    }

    void stopEndsIt()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("stopme"), 48000, tone(48000, 440.0, 1.0));
        QVERIFY(!id.isEmpty());
        const ListenerId who = QStringLiteral("local");
        svc->play(who, id);
        svc->pumpForTest();
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        svc->stopPlayback(who);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!svc->playback(who).has_value());
        std::vector<float> mono(kBlock, 1.0f);
        QCOMPARE(svc->pullPlayback(who, mono.data(), kBlock), 0);
        QCOMPARE(peak(mono, kBlock), 0.0);
    }

    void wholePanIsRefused()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("pan"), 48000, tone(48000, 440.0, 0.1),
                                        RecordingKind::WholePan);
        QVERIFY(!id.isEmpty());
        QSignalSpy refused(svc.get(), &RecordingService::playbackRefused);
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);
        svc->play(QStringLiteral("deviceA"), id);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(1).toString(), QStringLiteral("Open it on the waterfall."));
        QCOMPARE(changed.count(), 0);
        QVERIFY(!svc->playback(QStringLiteral("deviceA")).has_value());
    }
};

QTEST_GUILESS_MAIN(TestRecordingPlayer)
#include "tst_recording_player.moc"
