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
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: a whole-pan
//                 recording plays its sound only. AI-assisted via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF): renaming or deleting a recording
//                 while it plays stops its playback first. AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/audio/MasterMixer.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingPlayer.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <cmath>
#include <cstring>
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

// One owner mix's speakers sum, as AudioEngine hands it to a tap.
class SumTap final : public MasterMixAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
    }
    std::vector<float> received;
};

double peakOf(const std::vector<float>& v)
{
    double p = 0.0;
    for (const float x : v) {
        p = std::max(p, std::fabs(static_cast<double>(x)));
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

    // Renaming a recording while two devices play it (Windows will not
    // rename an open file): both playbacks stop and are told, the file
    // goes to its new name, and a device not playing it goes on.
    void renameWhilePlayingStopsItAndRenames()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("before"), 48000, tone(48000, 440.0, 2.0));
        const QString other = addRecording(*svc, QStringLiteral("other"), 48000, tone(48000, 440.0, 2.0));
        QVERIFY(!id.isEmpty());
        QVERIFY(!other.isEmpty());
        const ListenerId a = QStringLiteral("local");
        const ListenerId b = QStringLiteral("deviceB");
        const ListenerId c = QStringLiteral("deviceC");
        svc->play(a, id);
        svc->play(b, id);
        svc->play(c, other);
        svc->pumpForTest();
        std::vector<float> mono(kBlock);
        QCOMPARE(svc->pullPlayback(a, mono.data(), kBlock), kBlock);
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);

        // A refused name stops nothing.
        QString error;
        QVERIFY(!svc->renameRecording(id, QStringLiteral("con"), &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(svc->playback(a).has_value());

        error.clear();
        QVERIFY2(svc->renameRecording(id, QStringLiteral("after"), &error), qPrintable(error));
        QVERIFY(!svc->playback(a).has_value());
        QVERIFY(!svc->playback(b).has_value());
        QVERIFY(svc->playback(c).has_value());
        QVERIFY(svc->playback(c)->playing);
        QCOMPARE(changed.count(), 2);
        const QString folder = svc->store()->folder();
        QCOMPARE(svc->store()->find(id)->fileName, QStringLiteral("after.wav"));
        QVERIFY(QFileInfo::exists(QDir(folder).filePath(QStringLiteral("after.wav"))));
        QVERIFY(!QFileInfo::exists(QDir(folder).filePath(QStringLiteral("before.wav"))));
        QCOMPARE(svc->pullPlayback(a, mono.data(), kBlock), 0);

        // It plays again under its new name.
        svc->play(a, id);
        svc->pumpForTest();
        QCOMPARE(svc->pullPlayback(a, mono.data(), kBlock), kBlock);
    }

    // Deleting a recording while it plays: its playback stops and is told,
    // the file and its row go.
    void deleteWhilePlayingStopsItAndDeletes()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("doomed"), 48000, tone(48000, 440.0, 2.0));
        const QString keep = addRecording(*svc, QStringLiteral("keep"), 48000, tone(48000, 440.0, 2.0));
        QVERIFY(!id.isEmpty());
        const ListenerId a = QStringLiteral("local");
        const ListenerId b = QStringLiteral("deviceB");
        svc->play(a, id);
        svc->play(b, keep);
        svc->pumpForTest();
        QSignalSpy changed(svc.get(), &RecordingService::playbackChanged);

        QString error;
        QVERIFY2(svc->removeRecordings({id}, &error), qPrintable(error));
        QVERIFY(!svc->playback(a).has_value());
        QVERIFY(svc->playback(b).has_value());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), a);
        QVERIFY(!svc->store()->find(id).has_value());
        QVERIFY(!QFileInfo::exists(QDir(svc->store()->folder()).filePath(QStringLiteral("doomed.wav"))));
        QVERIFY(svc->store()->find(keep).has_value());
    }

    // The speaker feed: nothing but the engine's own drains moves the
    // playback. A plays; A's owner mix carries it at its level; B's mix
    // and the local speakers stay silent; the position is what the drains
    // took.
    void theEngineFeedsTheListenersOwnSpeakers()
    {
        RadioModel radio(RadioModel::Role::Local);
        AudioEngine* engine = radio.audioEngine();
        RecordingService* svc = radio.recordingService();
        QVERIFY(engine && svc);
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat format;
        format.sampleRate = 48000;
        format.channels = 2;
        format.sample = AudioFormat::Sample::Float32;
        bus->open(format);
        FakeAudioBus* speakers = bus.get();
        engine->setSpeakersBusForTest(std::move(bus));
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        engine->setVolume(1.0f);
        const int slice = radio.addSlice();
        engine->setSliceStreaming(slice, true);
        engine->setLocalOutputSliceMask(0);   // the local speakers carry no receiver

        const QString disk = m_dir.filePath(QStringLiteral("engine-disk"));
        QDir().mkpath(disk);
        RecordingSettings s = svc->settings();
        s.disk = disk;
        s.format = AudioFileFormat::Wav;
        svc->setSettings(s);
        const QString id = addRecording(*svc, QStringLiteral("engine-tone"), 48000, tone(48000, 1000.0, 2.0));
        QVERIFY(!id.isEmpty());

        const int mixA = engine->acquireOwnerMix();
        const int mixB = engine->acquireOwnerMix();
        QVERIFY(mixA >= 0 && mixB >= 0);
        SumTap tapA;
        SumTap tapB;
        QVERIFY(engine->setOwnerMixAudioTap(mixA, &tapA, true));
        QVERIFY(engine->setOwnerMixAudioTap(mixB, &tapB, true));
        engine->setOwnerMixPlayback(mixA, true);
        engine->setOwnerMixPlayback(mixB, true);
        svc->setListenerSpeakerSlot(QStringLiteral("deviceA"), AudioEngine::ownerPlaybackSlotId(mixA));
        svc->setListenerSpeakerSlot(QStringLiteral("deviceB"), AudioEngine::ownerPlaybackSlotId(mixB));

        svc->play(QStringLiteral("deviceA"), id);
        svc->pumpForTest();
        constexpr int kPeriod = 64;
        const std::vector<float> silence(kPeriod * 2, 0.0f);
        for (int period = 0; period < 750; ++period) {
            engine->rxBlockReady(slice, silence.data(), kPeriod);
            if (period % 100 == 99) {
                svc->pumpForTest();
            }
        }
        // 750 drains of 64 frames: one second.
        QCOMPARE(svc->playback(QStringLiteral("deviceA"))->positionMs, qint64(1000));
        QCOMPARE(tapA.received.size(), size_t(750 * kPeriod * 2));
        QVERIFY2(std::fabs(peakOf(tapA.received) - 0.5) < 0.01, qPrintable(QString::number(peakOf(tapA.received))));
        // Every frame was the tone: no gap where a drain went short.
        int rising = 0;
        for (size_t i = 2; i < tapA.received.size(); i += 2) {
            if (tapA.received[i - 2] < 0.0f && tapA.received[i] >= 0.0f) {
                ++rising;
            }
        }
        QVERIFY2(std::abs(rising - 1000) <= 1, qPrintable(QString::number(rising)));
        QCOMPARE(peakOf(tapB.received), 0.0);
        const QByteArray& played = speakers->buffer();
        std::vector<float> local(static_cast<size_t>(played.size()) / sizeof(float));
        std::memcpy(local.data(), played.constData(), local.size() * sizeof(float));
        QVERIFY(!local.empty());
        QCOMPARE(peakOf(local), 0.0);

        svc->stopPlayback(QStringLiteral("deviceA"));
        engine->clearOwnerMixAudioTap(mixA, &tapA);
        engine->clearOwnerMixAudioTap(mixB, &tapB);
        engine->releaseOwnerMix(mixA);
        engine->releaseOwnerMix(mixB);
    }

    // A whole-pan recording played here is its sound only, rendered from
    // the saved stretch (Task 25); a file that is not a stretch cannot be
    // played, and without WDSP it says to open it on the waterfall.
    void wholePanThatIsNotAStretchIsRefused()
    {
        std::unique_ptr<RecordingService> svc = makeService();
        const QString id = addRecording(*svc, QStringLiteral("pan"), 48000, tone(48000, 440.0, 0.1),
                                        RecordingKind::WholePan);
        QVERIFY(!id.isEmpty());
        QSignalSpy refused(svc.get(), &RecordingService::playbackRefused);
        svc->play(QStringLiteral("deviceA"), id);
#ifdef HAVE_WDSP
        QTRY_COMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(1).toString(), QStringLiteral("That recording cannot be played."));
#else
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(1).toString(), QStringLiteral("Open it on the waterfall."));
        QVERIFY(!svc->playback(QStringLiteral("deviceA")).has_value());
#endif
    }
};

QTEST_GUILESS_MAIN(TestRecordingPlayer)
#include "tst_recording_player.moc"
