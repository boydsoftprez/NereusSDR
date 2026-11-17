// =================================================================
// tests/tst_recording_contact.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The contact recorder and the
// recording service (spec 6.1, 6.2, V1, V2, V4, section 7, A2, A3, A17):
// 24 kHz files, your voice together or apart, start and stop from any
// device, start and stop by themselves, a full disk, MP3 without an
// encoder, the taps' copy-only contract and the call from Roundtable.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/recordings/AudioFileReader.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/ContactRecorder.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>
#include <vector>

// ---- An allocation counter for the taps' no-allocation check (A17). The
// taps are inline in ContactRecorder.h, so they are compiled here and their
// allocations, if any, come through these.
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

constexpr double kPi = 3.14159265358979323846;
constexpr double kRxToneHz = 600.0;
constexpr double kMeToneHz = 1800.0;
constexpr float kToneAmplitude = 0.3f;

// The amplitude of `hz` in one channel of interleaved audio (Goertzel).
double toneLevel(const std::vector<float>& audio, int channels, int channel, int rateHz, double hz,
                 qint64 from, qint64 count)
{
    const double coeff = 2.0 * std::cos(2.0 * kPi * hz / rateHz);
    double s1 = 0.0;
    double s2 = 0.0;
    for (qint64 i = from; i < from + count; ++i) {
        const double x = audio[static_cast<size_t>(i * channels + channel)];
        const double s0 = x + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return 2.0 * std::sqrt(std::max(0.0, power)) / static_cast<double>(count);
}

std::vector<float> readAll(const QString& path, int* rateHz, int* channels)
{
    QString error;
    std::unique_ptr<AudioFileReader> reader = AudioFileReader::open(path, &error);
    if (!reader) {
        qWarning() << "cannot read" << path << error;
        return {};
    }
    *rateHz = reader->sampleRateHz();
    *channels = reader->channels();
    std::vector<float> audio(static_cast<size_t>(reader->lengthFrames() * reader->channels()));
    qint64 at = 0;
    for (;;) {
        const int n = reader->read(audio.data() + at * reader->channels(), 4096);
        if (n <= 0) {
            break;
        }
        at += n;
    }
    audio.resize(static_cast<size_t>(at * reader->channels()));
    return audio;
}

// Ten seconds of fake receive blocks (48 kHz stereo, a 600 Hz tone) and fake
// monitor blocks (a 1800 Hz tone at `monitorRateHz`), drained as they come.
void driveTenSeconds(ContactRecorder& recorder, int monitorRateHz)
{
    constexpr int kRxRate = 48000;
    constexpr int kBlockMs = 10;
    const int rxFrames = kRxRate * kBlockMs / 1000;
    const int meFrames = monitorRateHz * kBlockMs / 1000;
    std::vector<float> rx(static_cast<size_t>(rxFrames) * 2);
    std::vector<float> me(static_cast<size_t>(meFrames));
    qint64 rxAt = 0;
    qint64 meAt = 0;
    for (int block = 0; block < 1000; ++block) {
        for (int i = 0; i < rxFrames; ++i, ++rxAt) {
            const float v = kToneAmplitude * static_cast<float>(std::sin(2.0 * kPi * kRxToneHz * rxAt / kRxRate));
            rx[static_cast<size_t>(2 * i)] = v;
            rx[static_cast<size_t>(2 * i + 1)] = v;
        }
        for (int i = 0; i < meFrames; ++i, ++meAt) {
            me[static_cast<size_t>(i)] =
                kToneAmplitude * static_cast<float>(std::sin(2.0 * kPi * kMeToneHz * meAt / monitorRateHz));
        }
        recorder.consumeMonitor(me.data(), meFrames, monitorRateHz);
        recorder.consume(rx.data(), rxFrames, kRxRate);
        QVERIFY(recorder.drain());
    }
}

struct Recorded {
    std::vector<float> audio;
    int rateHz = 0;
    int channels = 0;
    qint64 frames() const { return channels > 0 ? static_cast<qint64>(audio.size()) / channels : 0; }
};

Recorded recordTenSeconds(const QString& path, bool apart, bool keepMyVoice, bool transmitting,
                          int monitorRateHz, bool* hearing)
{
    Recorded out;
    ContactRecorder recorder(0);
    QString error;
    std::unique_ptr<AudioFileWriter> writer = AudioFileWriter::open(
        path, AudioFileSpec{AudioFileFormat::Wav, ContactRecorder::kFileRateHz, apart ? 2 : 1, 64}, &error);
    if (!writer) {
        qWarning() << "cannot write" << path << error;
        return out;
    }
    recorder.begin(std::move(writer), apart);
    recorder.setKeepMyVoice(keepMyVoice);
    recorder.setTransmitting(transmitting);
    recorder.setCapturing(true);
    *hearing = recorder.hearingMyVoice();
    driveTenSeconds(recorder, monitorRateHz);
    recorder.setCapturing(false);
    if (!recorder.finish()) {
        qWarning() << "finish failed";
        return out;
    }
    out.audio = readAll(path, &out.rateHz, &out.channels);
    return out;
}

QJsonObject turn(qint64 startMs, qint64 endMs, const QStringList& talkers, const QStringList& names)
{
    QJsonArray t;
    for (const QString& call : talkers) {
        t.append(QJsonObject{{QStringLiteral("callsign"), call}});
    }
    QJsonArray n;
    for (const QString& name : names) {
        n.append(QJsonObject{{QStringLiteral("name"), name}});
    }
    return QJsonObject{{QStringLiteral("start"), startMs},
                       {QStringLiteral("end"), endMs < 0 ? QJsonValue() : QJsonValue(endMs)},
                       {QStringLiteral("talkers"), t},
                       {QStringLiteral("names"), n}};
}

QJsonObject roomOf(const QList<QJsonObject>& turns)
{
    QJsonArray now;
    QJsonArray earlier;
    for (const QJsonObject& t : turns) {
        (t.value(QStringLiteral("end")).isNull() ? now : earlier).append(t);
    }
    return QJsonObject{{QStringLiteral("id"), QStringLiteral("ssb-7236400")},
                       {QStringLiteral("now"), now},
                       {QStringLiteral("earlier"), earlier}};
}

// A disk that takes `limit` bytes and refuses the rest.
class FullDiskFile : public QFile {
public:
    FullDiskFile(const QString& path, qint64 limit) : QFile(path), m_limit(limit) {}

protected:
    qint64 writeData(const char* data, qint64 len) override
    {
        if (pos() + len > m_limit) {
            return -1;
        }
        return QFile::writeData(data, len);
    }

private:
    qint64 m_limit;
};

// A WAV header and 2000 bytes of audio.
constexpr qint64 kFullDiskLimit = 44 + 2000;

const QDateTime kStartUtc(QDate(2026, 11, 15), QTime(14, 32, 0), QTimeZone::UTC);

}  // namespace

class TestRecordingContact : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    qint64 m_clockMs = 0;

    // A radio of this window's own, two slices (A transmits), recording
    // into the temporary folder as WAV.
    struct Bench {
        std::unique_ptr<RadioModel> radio;
        RecordingService* service = nullptr;
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
    };

    Bench makeBench(const QString& folderName, bool autoStart = false)
    {
        Bench bench;
        bench.radio = std::make_unique<RadioModel>(RadioModel::Role::Local);
        bench.service = bench.radio->recordingService();
        bench.a = bench.radio->sliceById(bench.radio->addSlice());
        bench.b = bench.radio->sliceById(bench.radio->addSlice());
        if (!bench.service || !bench.a || !bench.b) {
            return bench;
        }
        bench.a->setFrequency(7236400.0);
        bench.a->setDspMode(DSPMode::LSB);
        bench.b->setFrequency(14250000.0);
        bench.b->setDspMode(DSPMode::USB);
        bench.a->setTxSlice(true);
        bench.b->setTxSlice(false);
        m_clockMs = kStartUtc.toMSecsSinceEpoch();
        bench.service->setClockForTest([this] { return m_clockMs; });
        const QString disk = m_dir.filePath(folderName);
        QDir().mkpath(disk);
        RecordingSettings s = bench.service->settings();
        s.disk = disk;
        s.format = AudioFileFormat::Wav;
        s.autoStartOnTransmit = autoStart;
        s.autoStopWhenQuiet = true;
        s.quietMinutes = 2;
        bench.service->setSettings(s);
        return bench;
    }

    static RecordState stateOf(const RecordingService& service, int sliceId)
    {
        for (const RecordingSliceState& st : service.sliceStates()) {
            if (st.sliceId == sliceId) {
                return st.state;
            }
        }
        return RecordState::Idle;
    }

    static RecordingSliceState sliceState(const RecordingService& service, int sliceId)
    {
        for (const RecordingSliceState& st : service.sliceStates()) {
            if (st.sliceId == sliceId) {
                return st;
            }
        }
        return RecordingSliceState{sliceId, RecordState::Idle, {}, {}, 0, false};
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qRegisterMetaType<NereusSDR::KeptEvent>();
    }

    // ---- The call from Roundtable.

    void callTalkerBeatsALongerListenerName()
    {
        const qint64 s = 1000000;
        const QJsonObject room = roomOf({turn(s, s + 90000, {QStringLiteral("K4ABC")}, {}),
                                         turn(s + 90000, s + 290000, {}, {QStringLiteral("W1AW")})});
        QCOMPARE(callForRecording(room, s, s + 300000, QStringLiteral("KG4VCF")), QStringLiteral("K4ABC"));
    }

    void callLongestTalkerWins()
    {
        const qint64 s = 1000000;
        const QJsonObject room = roomOf({turn(s, s + 40000, {QStringLiteral("K4ABC")}, {}),
                                         turn(s + 40000, s + 160000, {QStringLiteral("N0XYZ")}, {})});
        QCOMPARE(callForRecording(room, s, s + 200000, QStringLiteral("KG4VCF")), QStringLiteral("N0XYZ"));
    }

    void callNeverYourOwn()
    {
        const qint64 s = 1000000;
        const QJsonObject room = roomOf({turn(s, s + 200000, {QStringLiteral("KG4VCF/P")}, {}),
                                         turn(s + 200000, s + 210000, {QStringLiteral("K4ABC")}, {}),
                                         turn(s, s + 200000, {}, {QStringLiteral("KG4VCF")})});
        QCOMPARE(callForRecording(room, s, s + 300000, QStringLiteral("KG4VCF")), QStringLiteral("K4ABC"));
        const QJsonObject onlyMe = roomOf({turn(s, s + 200000, {QStringLiteral("KG4VCF")}, {})});
        QCOMPARE(callForRecording(onlyMe, s, s + 300000, QStringLiteral("KG4VCF/P")), QString());
    }

    void callCountsOnlyTheOverlap()
    {
        const qint64 s = 1000000;
        // N0XYZ talks 300 s, but only 30 s of it falls in the recording;
        // K4ABC's 60 s is all inside. A turn wholly before it counts nothing.
        const QJsonObject room = roomOf({turn(s - 500000, s - 100000, {QStringLiteral("W1AW")}, {}),
                                         turn(s - 270000, s + 30000, {QStringLiteral("N0XYZ")}, {}),
                                         turn(s + 30000, s + 90000, {QStringLiteral("K4ABC")}, {}),
                                         turn(s + 400000, -1, {QStringLiteral("W2XX")}, {})});
        QCOMPARE(callForRecording(room, s, s + 100000, QStringLiteral("KG4VCF")), QStringLiteral("K4ABC"));
        // An open turn counts up to the recording's end.
        const QJsonObject open = roomOf({turn(s + 10000, -1, {QStringLiteral("N0XYZ")}, {}),
                                         turn(s, s + 20000, {QStringLiteral("K4ABC")}, {})});
        QCOMPARE(callForRecording(open, s, s + 100000, QStringLiteral("KG4VCF")), QStringLiteral("N0XYZ"));
    }

    void callEmptyRoomGivesNothing()
    {
        QCOMPARE(callForRecording(QJsonObject(), 0, 100000, QStringLiteral("KG4VCF")), QString());
        QCOMPARE(callForRecording(roomOf({}), 0, 100000, QStringLiteral("KG4VCF")), QString());
    }

    // ---- The recorder's file (A2, V2).

    void togetherIsMonoWithBoth_data()
    {
        QTest::addColumn<int>("monitorRateHz");
        QTest::newRow("monitor 48 kHz") << 48000;
        QTest::newRow("monitor 192 kHz") << 192000;
    }

    void togetherIsMonoWithBoth()
    {
        QFETCH(int, monitorRateHz);
        bool hearing = false;
        const Recorded r = recordTenSeconds(m_dir.filePath(QStringLiteral("together-%1.wav").arg(monitorRateHz)),
                                            /*apart*/ false, /*keep*/ true, /*mox*/ true, monitorRateHz, &hearing);
        QVERIFY(hearing);
        QCOMPARE(r.rateHz, 24000);
        QCOMPARE(r.channels, 1);
        QVERIFY2(qAbs(r.frames() - 240000) < 2400, qPrintable(QString::number(r.frames())));
        QVERIFY(toneLevel(r.audio, 1, 0, 24000, kRxToneHz, 24000, 48000) > 0.2);
        QVERIFY(toneLevel(r.audio, 1, 0, 24000, kMeToneHz, 24000, 48000) > 0.2);
    }

    void apartIsReceiveLeftAndYouRight_data() { togetherIsMonoWithBoth_data(); }

    void apartIsReceiveLeftAndYouRight()
    {
        QFETCH(int, monitorRateHz);
        bool hearing = false;
        const Recorded r = recordTenSeconds(m_dir.filePath(QStringLiteral("apart-%1.wav").arg(monitorRateHz)),
                                            /*apart*/ true, true, true, monitorRateHz, &hearing);
        QCOMPARE(r.rateHz, 24000);
        QCOMPARE(r.channels, 2);
        QVERIFY2(qAbs(r.frames() - 240000) < 2400, qPrintable(QString::number(r.frames())));
        QVERIFY(toneLevel(r.audio, 2, 0, 24000, kRxToneHz, 24000, 48000) > 0.2);
        QVERIFY(toneLevel(r.audio, 2, 0, 24000, kMeToneHz, 24000, 48000) < 0.02);
        QVERIFY(toneLevel(r.audio, 2, 1, 24000, kMeToneHz, 24000, 48000) > 0.2);
        QVERIFY(toneLevel(r.audio, 2, 1, 24000, kRxToneHz, 24000, 48000) < 0.02);
    }

    void keepMyVoiceOffLeavesTheMonitorOut()
    {
        bool hearing = true;
        const Recorded r = recordTenSeconds(m_dir.filePath(QStringLiteral("no-voice.wav")), false,
                                            /*keep*/ false, /*mox*/ true, 48000, &hearing);
        QVERIFY(!hearing);
        QCOMPARE(r.channels, 1);
        QVERIFY(toneLevel(r.audio, 1, 0, 24000, kRxToneHz, 24000, 48000) > 0.2);
        QVERIFY(toneLevel(r.audio, 1, 0, 24000, kMeToneHz, 24000, 48000) < 0.02);
    }

    void monitorCountsOnlyWhileMoxIsOn()
    {
        bool hearing = true;
        const Recorded r = recordTenSeconds(m_dir.filePath(QStringLiteral("no-mox.wav")), true, true,
                                            /*mox*/ false, 192000, &hearing);
        QVERIFY(!hearing);
        QCOMPARE(r.channels, 2);
        QVERIFY(toneLevel(r.audio, 2, 1, 24000, kMeToneHz, 24000, 48000) < 0.02);
        QVERIFY(toneLevel(r.audio, 2, 0, 24000, kRxToneHz, 24000, 48000) > 0.2);
    }

    // ---- A17: the taps copy and return.

    void tapsNeitherAllocateNorBlock()
    {
        QVERIFY(std::atomic<std::size_t>::is_always_lock_free);
        ContactRecorder recorder(0);
        recorder.setCapturing(true);
        recorder.setTransmitting(true);
        recorder.setKeepMyVoice(true);
        std::vector<float> rx(2 * 1024, 0.1f);
        std::vector<float> me(4096, 0.1f);
        // Nothing drains: the worker has stalled.
        g_allocations.store(0);
        g_countAllocations = true;
        for (int i = 0; i < 200; ++i) {
            recorder.consume(rx.data(), 1024, 48000);
        }
        for (int i = 0; i < 10; ++i) {
            recorder.skip(1024, 48000);
        }
        for (int i = 0; i < 300; ++i) {
            recorder.consumeMonitor(me.data(), 4096, 192000);
        }
        g_countAllocations = false;
        QCOMPARE(g_allocations.load(), 0);
        // 2^18 floats hold 128 receive blocks of 1024 stereo frames; 2^19
        // hold 128 monitor blocks of 4096. The rest are dropped and counted.
        QCOMPARE(recorder.receiveOverflows(), quint64(200 + 10 - 128));
        QCOMPARE(recorder.monitorOverflows(), quint64(300 - 128));
    }

    // ---- The service.

    void remoteWindowHasNoService()
    {
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(remote.recordingService() == nullptr);
        RadioModel local(RadioModel::Role::Local);
        QVERIFY(local.recordingService() != nullptr);
    }

    void twoSlicesRecordAtOnceAndASecondStartDoesNothing()
    {
        Bench bench = makeBench(QStringLiteral("two"));
        QVERIFY(bench.service);
        RecordingService& svc = *bench.service;
        QSignalSpy kept(&svc, &RecordingService::kept);
        const int a = bench.a->sliceIndex();
        const int b = bench.b->sliceIndex();

        svc.startRecording(a, QStringLiteral("Desktop"));
        svc.startRecording(b, QStringLiteral("JJ's iPhone"));
        QCOMPARE(stateOf(svc, a), RecordState::Recording);
        QCOMPARE(stateOf(svc, b), RecordState::Recording);
        QCOMPARE(sliceState(svc, a).startUtc, kStartUtc);

        m_clockMs += 5000;
        svc.startRecording(a, QStringLiteral("Other"));   // V1: nothing
        QCOMPARE(sliceState(svc, a).startUtc, kStartUtc);
        QCOMPARE(sliceState(svc, a).startedBy, QStringLiteral("Desktop"));
        QCOMPARE(sliceState(svc, a).elapsedMs, qint64(5000));

        svc.pumpForTest();
        svc.stopRecording(a, QString());
        svc.stopRecording(b, QString());
        svc.pumpForTest();
        QCOMPARE(kept.count(), 2);
        QCOMPARE(svc.store()->entries().size(), 2);
        QStringList names;
        for (const RecordingEntry& e : svc.store()->entries()) {
            names << e.name;
            QVERIFY(QFileInfo::exists(QDir(svc.store()->folder()).filePath(e.fileName)));
        }
        QVERIFY(names.contains(recordingBaseName(kStartUtc, 7236400, QStringLiteral("LSB"), QString())));
        QVERIFY(names.contains(recordingBaseName(kStartUtc, 14250000, QStringLiteral("USB"), QString())));
        // No working files are left behind.
        QCOMPARE(QDir(svc.store()->folder()).entryList({QStringLiteral(".recording-*")},
                                                        QDir::Files | QDir::Hidden).size(), 0);
        QCOMPARE(kept.at(0).at(0).value<KeptEvent>().reason, KeptReason::StoppedHere);
        QCOMPARE(stateOf(svc, a), RecordState::Idle);
    }

    void stopFromAnotherDeviceSaysWhich()
    {
        Bench bench = makeBench(QStringLiteral("device"));
        QVERIFY(bench.service);
        RecordingService& svc = *bench.service;
        QSignalSpy kept(&svc, &RecordingService::kept);
        const int a = bench.a->sliceIndex();
        svc.startRecording(a, QString());
        svc.pumpForTest();
        svc.stopRecording(a, QStringLiteral("JJ's iPhone"));
        svc.pumpForTest();
        QCOMPARE(kept.count(), 1);
        const KeptEvent e = kept.at(0).at(0).value<KeptEvent>();
        QCOMPARE(e.reason, KeptReason::StoppedOnDevice);
        QCOMPARE(e.deviceName, QStringLiteral("JJ's iPhone"));
        QCOMPARE(e.sliceId, a);
    }

    void startByItselfArmsAndMoxStartsTheTransmitSlice()
    {
        Bench bench = makeBench(QStringLiteral("armed"), /*autoStart*/ true);
        QVERIFY(bench.service);
        RecordingService& svc = *bench.service;
        const int a = bench.a->sliceIndex();
        const int b = bench.b->sliceIndex();
        QCOMPARE(stateOf(svc, a), RecordState::Armed);
        QCOMPARE(stateOf(svc, b), RecordState::Armed);

        svc.setTransmitting(true);
        QCOMPARE(stateOf(svc, a), RecordState::Recording);
        QCOMPARE(stateOf(svc, b), RecordState::Armed);
        QVERIFY(sliceState(svc, a).hearingMyVoice);

        // Record while armed starts at once.
        svc.startRecording(b, QStringLiteral("Desktop"));
        QCOMPARE(stateOf(svc, b), RecordState::Recording);

        svc.setTransmitting(false);
        QVERIFY(!sliceState(svc, a).hearingMyVoice);
        svc.pumpForTest();
        svc.stopAll();
        QCOMPARE(stateOf(svc, a), RecordState::Armed);
        QCOMPARE(svc.store()->entries().size(), 2);
    }

    void quietStopKeepsAndRearms()
    {
        Bench bench = makeBench(QStringLiteral("quiet"), /*autoStart*/ true);
        QVERIFY(bench.service);
        RecordingService& svc = *bench.service;
        QSignalSpy kept(&svc, &RecordingService::kept);
        const int a = bench.a->sliceIndex();
        svc.startRecording(a, QString());
        svc.pumpForTest();
        m_clockMs += 119999;
        svc.checkQuietForTest();
        QCOMPARE(stateOf(svc, a), RecordState::Recording);
        m_clockMs += 1;
        svc.checkQuietForTest();
        QCOMPARE(stateOf(svc, a), RecordState::Armed);
        svc.pumpForTest();
        QCOMPARE(kept.count(), 1);
        QCOMPARE(kept.at(0).at(0).value<KeptEvent>().reason, KeptReason::StoppedQuiet);
    }

    void quietWithoutStartByItselfGoesIdle()
    {
        Bench bench = makeBench(QStringLiteral("quiet-idle"));
        QVERIFY(bench.service);
        RecordingService& svc = *bench.service;
        const int a = bench.a->sliceIndex();
        svc.startRecording(a, QString());
        svc.pumpForTest();
        m_clockMs += 60000;
        svc.setTransmitting(true);   // a transmit at 60 s moves quiet to 180 s
        svc.setTransmitting(false);
        m_clockMs += 60000;
        svc.checkQuietForTest();
        QCOMPARE(stateOf(svc, a), RecordState::Recording);
        m_clockMs += 60000;
        svc.checkQuietForTest();
        QCOMPARE(stateOf(svc, a), RecordState::Idle);
        svc.pumpForTest();
    }

    void fullDiskStopsAndKeepsWhatWasWritten()
    {
        Bench bench = makeBench(QStringLiteral("full"));
        QVERIFY(bench.service);
        RecordingService& svc = *bench.service;
        svc.setWriterFactoryForTest([](const QString& path, const AudioFileSpec& spec, QString* error) {
            auto file = std::make_unique<FullDiskFile>(path, kFullDiskLimit);
            if (!file->open(QIODevice::ReadWrite | QIODevice::Truncate)) {
                return std::unique_ptr<AudioFileWriter>();
            }
            return AudioFileWriter::openDevice(std::move(file), spec, error);
        });
        QSignalSpy kept(&svc, &RecordingService::kept);
        const int a = bench.a->sliceIndex();
        svc.startRecording(a, QString());
        svc.pumpForTest();
        ContactRecorder* recorder = svc.recorderForTest(a);
        QVERIFY(recorder);
        std::vector<float> block(2 * 480, 0.25f);
        for (int i = 0; i < 40 && kept.isEmpty(); ++i) {
            recorder->consume(block.data(), 480, 48000);
            svc.pumpForTest();
        }
        QCOMPARE(kept.count(), 1);
        const KeptEvent e = kept.at(0).at(0).value<KeptEvent>();
        QCOMPARE(e.reason, KeptReason::DiskFull);
        QCOMPARE(stateOf(svc, a), RecordState::Idle);
        const std::optional<RecordingEntry> entry = svc.store()->find(e.id);
        QVERIFY(entry.has_value());
        const QString path = QDir(svc.store()->folder()).filePath(entry->fileName);
        QVERIFY(QFileInfo(path).size() > 44);
        QVERIFY(QFileInfo(path).size() <= kFullDiskLimit);
        QVERIFY(e.lengthMs > 0);
        int rate = 0;
        int channels = 0;
        QVERIFY(!readAll(path, &rate, &channels).empty());
        QCOMPARE(rate, 24000);
    }

    void mp3WithoutAnEncoderIsKeptAsWav()
    {
        Bench bench = makeBench(QStringLiteral("mp3"));
        QVERIFY(bench.service);
        RecordingService& svc = *bench.service;
        RecordingSettings s = svc.settings();
        s.format = AudioFileFormat::Mp3;
        svc.setSettings(s);
        svc.setMp3AvailableForTest(false);
        QSignalSpy kept(&svc, &RecordingService::kept);
        const int a = bench.a->sliceIndex();
        svc.startRecording(a, QStringLiteral("JJ's iPhone"));
        svc.pumpForTest();
        svc.stopRecording(a, QStringLiteral("JJ's iPhone"));
        svc.pumpForTest();
        QCOMPARE(kept.count(), 1);
        const KeptEvent e = kept.at(0).at(0).value<KeptEvent>();
        QCOMPARE(e.reason, KeptReason::KeptAsWav);
        const std::optional<RecordingEntry> entry = svc.store()->find(e.id);
        QVERIFY(entry.has_value());
        QVERIFY(entry->fileName.endsWith(QStringLiteral(".wav")));
    }
};

QTEST_GUILESS_MAIN(TestRecordingContact)
#include "tst_recording_contact.moc"
