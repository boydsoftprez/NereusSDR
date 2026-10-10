// =================================================================
// tests/tst_capture_supervisor.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  CaptureSupervisor and
// CaptureAudioBus lifecycle tests (R-R3-36).  The helper is always the
// scripted fake, run by re-executing this binary with
// --fake-capture-child <scenario>; no real microphone is opened.
//
// Modification history (NereusSDR):
//   2026-09-22: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-08: native audio plan Task 1 (V-HW-8): the delay probe's
//               ProbeEnable and ProbeHit, and a protocol 1 helper refused.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17, R-AUD-18): the reader
//               over a clock matcher ring, the shared ring from the fake,
//               a Pcm record and a bad ring as protocol errors, the
//               device-in-use reason, the device facts in Ready, and a
//               status of an earlier request told apart.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 15 (R-AUD-19, R-AUD-21): an ASIO
//               demand keeps the helper running with no microphone open,
//               the ASIO answers come back, and a helper started only to
//               describe drivers stops after the answer.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: mic drain fix (R-AUD-17, R-R3-36): the reader is paced by
//               the 48 kHz clock; a drain until 0 ends in a bounded number
//               of pulls, and a paced caller still gets every frame.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (V-HW-8): the probe test
//               waits for the fake's mark of the disable, not a fixed
//               time; a helper that exits at once is found by its last
//               process id (waitForStartedPid).  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QSignalSpy>

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "core/audio/CaptureAudioBus.h"
#include "core/audio/CaptureSupervisor.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"
#include "fakes/FakeCaptureChild.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#endif

using namespace NereusSDR;
using Status = CaptureSupervisor::Status;
using State = Status::State;
using Reason = Status::Reason;

namespace {

CaptureSupervisor::Options fakeOptions(const QString& scenario)
{
    CaptureSupervisor::Options options;
    options.program = QCoreApplication::applicationFilePath();
    options.arguments = {QStringLiteral("--fake-capture-child"), scenario};
    return options;
}

// Waits on statusChanged until pred(status()) holds.
bool waitFor(CaptureSupervisor& supervisor, const std::function<bool(const Status&)>& pred,
             int timeoutMs)
{
    QSignalSpy spy(&supervisor, &CaptureSupervisor::statusChanged);
    QElapsedTimer timer;
    timer.start();
    while (!pred(supervisor.status())) {
        const qint64 left = timeoutMs - timer.elapsed();
        if (left <= 0) {
            return false;
        }
        spy.wait(static_cast<int>(left));
    }
    return true;
}

bool waitForState(CaptureSupervisor& supervisor, State state, int timeoutMs)
{
    return waitFor(supervisor, [state](const Status& s) { return s.state == state; }, timeoutMs);
}

// Records every published status.
struct Recorder {
    explicit Recorder(CaptureSupervisor& supervisor)
    {
        QObject::connect(&supervisor, &CaptureSupervisor::statusChanged, &context,
                         [this](const Status& status) { seen.push_back(status); });
    }
    bool sawState(State state) const
    {
        for (const Status& s : seen) {
            if (s.state == state) {
                return true;
            }
        }
        return false;
    }
    QObject context;
    std::vector<Status> seen;
};

qint64 waitForPid(const CaptureSupervisor& supervisor, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (supervisor.helperProcessId() == 0 && timer.elapsed() < timeoutMs) {
        QTest::qWait(5);
    }
    return supervisor.helperProcessId();
}

// The process id of a helper that may answer and exit before a poll of
// helperProcessId() sees it (a protocol error, a describe alone).
qint64 waitForStartedPid(const CaptureSupervisor& supervisor, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (supervisor.lastHelperProcessId() == 0 && timer.elapsed() < timeoutMs) {
        QTest::qWait(5);
    }
    return supervisor.lastHelperProcessId();
}

bool processIsGone(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr) {
        return true;
    }
    const DWORD result = WaitForSingleObject(handle, 0);
    CloseHandle(handle);
    return result == WAIT_OBJECT_0;
#else
    return ::kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH;
#endif
}

bool waitProcessGone(qint64 pid, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!processIsGone(pid)) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        QTest::qWait(5);
    }
    return true;
}

// Pulls at about the stream's own pace (240 frames each 5 ms, as the TX
// worker drains it) until at least `frames` frames were read or the time
// runs out.  The reader is paced by the 48 kHz clock, and past the
// written frames it gives the matcher's dry-run silence.
std::vector<float> pullFrames(CaptureAudioBus* reader, int frames, int timeoutMs)
{
    std::vector<float> out;
    std::vector<float> block(240);
    QElapsedTimer timer;
    timer.start();
    while (static_cast<int>(out.size()) < frames && timer.elapsed() < timeoutMs) {
        QTest::qWait(5);
        const qint64 got = reader->pull(reinterpret_cast<char*>(block.data()),
                                        static_cast<qint64>(block.size() * sizeof(float)));
        if (got > 0) {
            out.insert(out.end(), block.begin(), block.begin() + got / static_cast<qint64>(sizeof(float)));
        }
    }
    return out;
}

// A clock the reader's pacing reads in the reader tests.
std::int64_t g_testNowNs = 1'000'000'000;
std::int64_t testNow()
{
    return g_testNowNs;
}
constexpr std::int64_t kNsPer480Frames = 10'000'000;   // 10 ms at 48 kHz

// Pulls `frames` frames per pull, the test clock advancing by stepNs
// before each, `pulls` times; returns the frames each pull gave.
std::vector<qint64> pacedPulls(CaptureAudioBus& reader, int frames, std::int64_t stepNs, int pulls,
                               std::vector<float>* into = nullptr)
{
    std::vector<qint64> got;
    std::vector<float> block(static_cast<std::size_t>(frames));
    for (int i = 0; i < pulls; ++i) {
        g_testNowNs += stepNs;
        const qint64 bytes = reader.pull(reinterpret_cast<char*>(block.data()),
                                         qint64(block.size() * sizeof(float)));
        got.push_back(bytes / qint64(sizeof(float)));
        if (into != nullptr) {
            into->insert(into->end(), block.begin(), block.begin() + got.back());
        }
    }
    return got;
}

// A clock matcher ring in local memory, as the helper builds one in the
// shared region: 48 kHz in and out, 480-frame bursts.
struct LocalRing {
    LocalRing()
    {
        config.inRate = 48000;
        config.outRate = 48000;
        config.writeBlockFrames = 64;
        config.callbackFrames = 480;
        config.delayMs = 0;
        bytes = DeviceRateMatcher::ringBytes(config);
        memory.resize(bytes / sizeof(std::uint64_t) + 1);
        matcher = std::make_unique<DeviceRateMatcher>(config, memory.data(), bytes);
    }
    MatcherRingHeader* header()
    {
        return attachMatcherRing(memory.data(), bytes);
    }
    // count blocks of 480 stereo frames at value, 10 ms apart.
    void write(float value, int count)
    {
        std::vector<float> block(960, value);
        for (int i = 0; i < count; ++i) {
            nowNs += 10'000'000;
            matcher->write(block.data(), 480, nowNs);
        }
    }
    DeviceRateMatcher::Config config;
    std::size_t bytes = 0;
    std::vector<std::uint64_t> memory;
    std::unique_ptr<DeviceRateMatcher> matcher;
    std::int64_t nowNs = 1'000'000'000;
};

} // namespace

class TstCaptureSupervisor : public QObject {
    Q_OBJECT
private slots:
    // ── Reader ───────────────────────────────────────────────────────────────

    void readerContract()
    {
        CaptureAudioBus reader;
        AudioFormat any;
        QVERIFY(reader.open(any));
        QVERIFY(!reader.isOpen());
        QCOMPARE(reader.push("abcd", 4), qint64(0));
        const AudioFormat format = reader.negotiatedFormat();
        QCOMPARE(format.sampleRate, 48000);
        QCOMPARE(format.channels, 1);
        QVERIFY(format.sample == AudioFormat::Sample::Float32);
        QVERIFY(!reader.ringAttached());
        QVERIFY(!reader.fillFrames().has_value());
        QVERIFY(!reader.attachRing(nullptr));

        char buffer[64];
        reader.setAvailable(true);
        QCOMPARE(reader.pull(buffer, sizeof(buffer)), qint64(0));   // open, but no ring
        reader.setAvailable(false);

        LocalRing ring;
        QVERIFY(ring.matcher->valid());
        QVERIFY(ring.header() != nullptr);
        QVERIFY(reader.attachRing(ring.header()));
        QVERIFY(reader.ringAttached());
        QVERIFY(!reader.attachRing(ring.header()));                 // one ring at a time
        ring.write(0.25f, 10);
        reader.noteWake();
        QCOMPARE(reader.txLevel(), 0.0f);                           // unavailable
        QCOMPARE(reader.pull(buffer, sizeof(buffer)), qint64(0));

        reader.setAvailable(true);
        QVERIFY(reader.isOpen());
        QVERIFY2(reader.txLevel() > 0.2f && reader.txLevel() < 0.3f,
                 qPrintable(QString::number(reader.txLevel())));
        QVERIFY(reader.fillFrames().has_value());
        QCOMPARE(reader.pull(buffer, 6), qint64(4));                // whole frames only
        reader.close();
        QVERIFY(reader.isOpen());                                   // close() changes nothing

        reader.setAvailable(false);
        QCOMPARE(reader.txLevel(), 0.0f);
        reader.detachRing();
        QVERIFY(!reader.ringAttached());
        reader.setAvailable(true);
        QCOMPARE(reader.pull(buffer, sizeof(buffer)), qint64(0));   // no ring after detach
        QCOMPARE(reader.overruns(), quint64(0));
        QCOMPARE(reader.lastWriteNs(), std::int64_t(0));
    }

    // R-AUD-17: a paced reader gets every frame it asks for from the
    // ring's left channel; past the written frames it is a counted dry
    // run, and a pull larger than its scratch block is filled in pieces.
    void readerReadsTheMatcherRing()
    {
        LocalRing ring;
        CaptureAudioBus reader;
        reader.setClockForTest(&testNow);
        QVERIFY(reader.attachRing(ring.header()));
        reader.setAvailable(true);
        ring.write(0.25f, 6);                                       // 2880 frames
        QVERIFY(reader.lastWriteNs() > 1'000'000'000);

        std::vector<float> out;
        for (qint64 got : pacedPulls(reader, 480, kNsPer480Frames, 5, &out)) {
            QCOMPARE(got, qint64(480));
        }
        int nearQuarter = 0;
        for (float v : out) {
            QVERIFY(v >= -0.3f && v <= 0.3f);
            nearQuarter += (std::fabs(v - 0.25f) < 0.01f) ? 1 : 0;
        }
        QVERIFY2(nearQuarter > 500, qPrintable(QString::number(nearQuarter)));

        const quint64 dryBefore = reader.dryRuns();
        std::vector<float> more;
        for (qint64 got : pacedPulls(reader, 480, kNsPer480Frames, 17, &more)) {
            QCOMPARE(got, qint64(480));
        }
        QVERIFY(reader.dryRuns() > dryBefore);
        QCOMPARE(more.back(), 0.0f);                                // silence once dry

        // After a late pull: the credit cap plus the slack, over two
        // scratch blocks.
        const std::vector<qint64> late = pacedPulls(reader, 2000, 4 * kNsPer480Frames, 1);
        QCOMPARE(late.front(),
                 qint64(CaptureAudioBus::kPaceCreditCapFrames + CaptureAudioBus::kPaceSlackFrames));
        reader.flush();                                             // changes nothing
        QVERIFY(reader.ringAttached());
        reader.detachRing();
    }

    // R-AUD-17, R-R3-36: a caller that drains until pull() returns 0 (the
    // remote window's microphone uplink) stops after the clock's frames,
    // even though the reader pads a dry run.  Without the pacing this loop
    // reads padding until its bound and fails.
    void aDrainUntilZeroEnds()
    {
        LocalRing ring;
        CaptureAudioBus reader;
        reader.setClockForTest(&testNow);
        QVERIFY(reader.attachRing(ring.header()));
        reader.setAvailable(true);
        ring.write(0.25f, 6);

        static constexpr int kBoundPulls = 100;
        const auto drain = [&reader]() {
            std::vector<float> block(960);
            qint64 total = 0;
            for (int i = 0; i < kBoundPulls; ++i) {
                const qint64 got = reader.pull(reinterpret_cast<char*>(block.data()),
                                               qint64(block.size() * sizeof(float)));
                if (got <= 0) {
                    return std::make_pair(i, total);
                }
                total += got / qint64(sizeof(float));
            }
            return std::make_pair(kBoundPulls, total);
        };

        // The first drain: the slack only, then 0.
        auto [pulls, frames] = drain();
        QVERIFY2(pulls < kBoundPulls, "the drain never ended");
        QCOMPARE(frames, qint64(CaptureAudioBus::kPaceSlackFrames));

        // 20 ms later: the clock's 960 frames, then 0.
        g_testNowNs += 2 * kNsPer480Frames;
        std::tie(pulls, frames) = drain();
        QVERIFY2(pulls < kBoundPulls, "the drain never ended");
        QCOMPARE(frames, qint64(960));

        // No time passed: nothing.
        std::tie(pulls, frames) = drain();
        QCOMPARE(pulls, 0);
        QCOMPARE(frames, qint64(0));
        reader.detachRing();
    }

    // The TX worker's cadence: 240 frames every 5 ms, and a pull that
    // comes early by less than the slack, always a whole block.
    void aPacedCallerGetsEveryBlock()
    {
        LocalRing ring;
        CaptureAudioBus reader;
        reader.setClockForTest(&testNow);
        QVERIFY(reader.attachRing(ring.header()));
        reader.setAvailable(true);
        for (qint64 got : pacedPulls(reader, 240, kNsPer480Frames / 2, 200)) {
            QCOMPARE(got, qint64(240));
        }
        // One pull 1 ms after the last, then one 9 ms after it.
        QCOMPARE(pacedPulls(reader, 240, kNsPer480Frames / 10, 1).front(), qint64(240));
        QCOMPARE(pacedPulls(reader, 240, 9 * kNsPer480Frames / 10, 1).front(), qint64(240));
        for (qint64 got : pacedPulls(reader, 240, kNsPer480Frames / 2, 50)) {
            QCOMPARE(got, qint64(240));
        }
        reader.detachRing();
    }

    // ── Supervisor ───────────────────────────────────────────────────────────

    void noDemandMeansNoHelper()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        QCOMPARE(supervisor.status().state, State::Closed);
        QTest::qWait(200);
        QCOMPARE(supervisor.helperProcessId(), qint64(0));
        QCOMPARE(supervisor.status().state, State::Closed);
        QVERIFY(!supervisor.reader()->isOpen());
    }

    void missingProgramFailsWithoutSpawning()
    {
        CaptureSupervisor unconfigured;
        auto lease = unconfigured.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(lease.isActive());
        QVERIFY(waitForState(unconfigured, State::Failed, 2000));
        QCOMPARE(unconfigured.status().reason, Reason::HelperMissing);
        QCOMPARE(unconfigured.helperProcessId(), qint64(0));

        CaptureSupervisor::Options options;
        options.program = QDir(QDir::tempPath()).absoluteFilePath(
            QStringLiteral("nereus-audio-capture-that-does-not-exist"));
        CaptureSupervisor absent(options);
        auto lease2 = absent.acquire(CaptureSupervisor::Demand::TestMic);
        QVERIFY(waitForState(absent, State::Failed, 2000));
        QCOMPARE(absent.status().reason, Reason::HelperMissing);
        QCOMPARE(absent.helperProcessId(), qint64(0));
    }

    void readyPathDeliversSamples()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        CaptureAudioBus* reader = supervisor.reader();
        Recorder recorder(supervisor);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        const Status ready = supervisor.status();
        QVERIFY(ready.generation >= 1);
        QCOMPARE(ready.actualDevice, QStringLiteral("Fake microphone"));
        QCOMPARE(ready.reason, Reason::None);
        // The device facts from the helper's Ready (R-AUD-18).
        QCOMPARE(ready.nativeRate, 48000);
        QCOMPARE(ready.deviceLatencyMs, 1.5);
        QCOMPARE(ready.deviceBufferMs, 10.0);
        QCOMPARE(ready.request, supervisor.requestSerial());
        QVERIFY(ready.request >= 1);
        QVERIFY(reader->ringAttached());
        QVERIFY(recorder.sawState(State::Opening));
        QVERIFY(reader->isOpen());
        const qint64 pid = supervisor.helperProcessId();
        QVERIFY(pid > 0);

        // The clock matcher starts with its target fill of silence (about
        // 30 ms here); the tone follows it.
        const std::vector<float> samples = pullFrames(reader, 4800, 5000);
        QVERIFY(samples.size() >= 4800);
        float peak = 0.0f;
        for (std::size_t i = samples.size() - 960; i < samples.size(); ++i) {
            peak = std::max(peak, std::fabs(samples[i]));
        }
        QVERIFY2(peak > 0.4f && peak <= 0.51f, qPrintable(QString::number(peak)));
        QVERIFY(reader->txLevel() > 0.0f);
        QCOMPARE(supervisor.reader(), reader);

        QElapsedTimer timer;
        timer.start();
        lease.release();
        QVERIFY(!lease.isActive());
        lease.release();                                        // twice is harmless
        QVERIFY(waitForState(supervisor, State::Closed, 3000));
        QVERIFY(timer.elapsed() < 2000);
        QVERIFY(recorder.sawState(State::Stopping));
        QVERIFY(!reader->isOpen());
        QVERIFY(!reader->ringAttached());
        char buffer[64];
        QCOMPARE(reader->pull(buffer, sizeof(buffer)), qint64(0));
        QVERIFY(waitProcessGone(pid, 1000));
        QCOMPARE(supervisor.helperProcessId(), qint64(0));
        QCOMPARE(supervisor.reader(), reader);
    }

    void hangOpenTimesOut()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-open"));
        options.openTimeoutMs = 500;                            // 10000 in production
        CaptureSupervisor supervisor(options);
        QElapsedTimer timer;
        timer.start();
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        const qint64 pid = waitForPid(supervisor, 3000);
        QVERIFY(pid > 0);
        QVERIFY(waitForState(supervisor, State::Failed, 5000));
        const qint64 elapsed = timer.elapsed();
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        QVERIFY2(elapsed >= 500 && elapsed < 3000, qPrintable(QString::number(elapsed)));
        QVERIFY(waitProcessGone(pid, 1000));                    // child killed
        QVERIFY(!supervisor.reader()->isOpen());

        // Failed persists: never retried by the supervisor itself.
        const quint32 generation = supervisor.status().generation;
        QTest::qWait(700);
        QCOMPARE(supervisor.status().state, State::Failed);
        QCOMPARE(supervisor.status().generation, generation);
        QCOMPARE(supervisor.helperProcessId(), qint64(0));
    }

    void shutdownIsBoundedWhileChildHangs()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("hang-open")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        const qint64 pid = waitForPid(supervisor, 3000);
        QVERIFY(pid > 0);
        QVERIFY(waitForState(supervisor, State::Opening, 3000));
        QTest::qWait(200);                                      // Open sent, child now silent

        QElapsedTimer timer;
        timer.start();
        supervisor.shutdown();
        const qint64 elapsed = timer.elapsed();
        QVERIFY2(elapsed < 2000 + 500, qPrintable(QString::number(elapsed)));
        QVERIFY(waitProcessGone(pid, 500));
        QCOMPARE(supervisor.status().state, State::Closed);
        QVERIFY(!lease.isActive());
        lease.release();
        supervisor.shutdown();                                  // idempotent
        QVERIFY(!supervisor.acquire(CaptureSupervisor::Demand::TestMic).isActive());
        qInfo("shutdown against a hanging child took %lld ms", elapsed);
    }

    void destroyingWithHangingChildLeavesNoProcess()
    {
        auto supervisor = std::make_unique<CaptureSupervisor>(fakeOptions(QStringLiteral("hang-open")));
        auto lease = supervisor->acquire(CaptureSupervisor::Demand::TestMic);
        const qint64 pid = waitForPid(*supervisor, 3000);
        QVERIFY(pid > 0);
        QVERIFY(waitForState(*supervisor, State::Opening, 3000));
        QTest::qWait(200);

        QElapsedTimer timer;
        timer.start();
        supervisor.reset();
        const qint64 elapsed = timer.elapsed();
        QVERIFY2(elapsed < 2000 + 500, qPrintable(QString::number(elapsed)));
        QVERIFY2(waitProcessGone(pid, 500), "helper outlived its supervisor");
        QVERIFY(!lease.isActive());
        lease.release();                                        // outliving the supervisor is harmless
        qInfo("destruction against a hanging child took %lld ms", elapsed);
    }

    void noHelloFails()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("no-hello"));
        options.helloTimeoutMs = 400;                           // 3000 in production
        CaptureSupervisor supervisor(options);
        QElapsedTimer timer;
        timer.start();
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        const qint64 pid = waitForPid(supervisor, 3000);
        QVERIFY(pid > 0);
        QVERIFY(waitForState(supervisor, State::Failed, 3000));
        QCOMPARE(supervisor.status().reason, Reason::HelperDidNotStart);
        QVERIFY(timer.elapsed() >= 400);
        QVERIFY(waitProcessGone(pid, 1000));
    }

    void permissionPausesOpenDeadline()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("permission-then-ready"));
        options.openTimeoutMs = 300;                            // the fake holds Permission for 300 ms
        CaptureSupervisor supervisor(options);
        Recorder recorder(supervisor);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitFor(supervisor, [](const Status& s) {
            return s.state == State::Ready || s.state == State::Failed;
        }, 5000));
        QCOMPARE(supervisor.status().state, State::Ready);
        QVERIFY(recorder.sawState(State::PreparingPermission));
        QVERIFY(supervisor.reader()->isOpen());
    }

    void crashAfterReadyFails()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("crash-after-ready")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        QVERIFY(waitForState(supervisor, State::Failed, 3000));
        QCOMPARE(supervisor.status().reason, Reason::HelperExited);
        QVERIFY(!supervisor.reader()->isOpen());
        char buffer[64];
        QCOMPARE(supervisor.reader()->pull(buffer, sizeof(buffer)), qint64(0));
        QCOMPARE(supervisor.reader()->txLevel(), 0.0f);
        QCOMPARE(supervisor.helperProcessId(), qint64(0));
    }

    void protocolErrors_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::newRow("malformed") << QStringLiteral("malformed");
        QTest::newRow("oversize") << QStringLiteral("oversize");
        // V-HW-8: protocol 2 rejects a protocol 1 helper, as before.
        QTest::newRow("version-1") << QStringLiteral("version-1");
        // R-AUD-17: version 3 audio is in the ring only, and a ring the
        // helper did not build is refused.
        QTest::newRow("pcm-record") << QStringLiteral("pcm-record");
        QTest::newRow("bad-ring") << QStringLiteral("bad-ring");
    }

    void protocolErrors()
    {
        QFETCH(QString, scenario);
        CaptureSupervisor supervisor(fakeOptions(scenario));
        Recorder recorder(supervisor);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        const qint64 pid = waitForStartedPid(supervisor, 3000);
        QVERIFY(pid > 0);
        QVERIFY(waitForState(supervisor, State::Failed, 3000));
        QCOMPARE(supervisor.status().reason, Reason::ProtocolError);
        QVERIFY(!recorder.sawState(State::Ready));
        QVERIFY(waitProcessGone(pid, 1000));
        QVERIFY(!supervisor.reader()->isOpen());
    }

    void inputLostFails()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("input-lost")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        QVERIFY(waitForState(supervisor, State::Failed, 3000));
        QCOMPARE(supervisor.status().reason, Reason::InputLost);
        QVERIFY(!supervisor.reader()->isOpen());
        QCOMPARE(supervisor.reader()->txLevel(), 0.0f);
    }

    // R-AUD-11: a mic another program holds fails as DeviceInUse, never
    // as a plain open failure.
    void busyFailsAsDeviceInUse()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("busy")));
        Recorder recorder(supervisor);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Failed, 5000));
        QCOMPARE(supervisor.status().reason, Reason::DeviceInUse);
        QVERIFY(!recorder.sawState(State::Ready));
        QVERIFY(!supervisor.reader()->isOpen());
    }

    // Carried finding (Task 5): two configures back to back.  Every status
    // that answers the later request is of the last generation, and every
    // status of an earlier request is of an earlier generation, so a
    // caller holding the later serial never takes the first open's result.
    void backToBackConfiguresAnswerTheirOwnRequest()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        const quint64 start = supervisor.requestSerial();

        Recorder recorder(supervisor);
        AudioDeviceConfig a;
        a.deviceName = QStringLiteral("Microphone A");
        AudioDeviceConfig b;
        b.deviceName = QStringLiteral("Microphone B");
        supervisor.configure(a);
        const quint64 serialA = supervisor.requestSerial();
        supervisor.configure(b);
        const quint64 serialB = supervisor.requestSerial();
        QCOMPARE(serialA, start + 1);
        QCOMPARE(serialB, start + 2);
        QVERIFY(waitFor(supervisor, [serialB](const Status& s) {
            return s.state == State::Ready && s.request == serialB;
        }, 5000));
        const quint32 last = supervisor.status().generation;
        QCOMPARE(supervisor.status().configuredDevice, QStringLiteral("Microphone B"));
        QVERIFY(!recorder.seen.empty());
        for (const Status& s : recorder.seen) {
            QVERIFY(s.request >= serialA);
            if (s.request == serialB) {
                QCOMPARE(s.generation, last);
            } else {
                QVERIFY2(s.generation < last, qPrintable(QString::number(s.generation)));
                QVERIFY(s.configuredDevice != QStringLiteral("Microphone B"));
            }
        }
    }

    void ignoredStopIsKilledWithinBound()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("ignore-stop"));
        options.stopTimeoutMs = 300;                            // 2000 in production
        CaptureSupervisor supervisor(options);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        const qint64 pid = supervisor.helperProcessId();
        QVERIFY(pid > 0);

        QElapsedTimer timer;
        timer.start();
        lease.release();
        QVERIFY(waitForState(supervisor, State::Closed, 3000));
        const qint64 elapsed = timer.elapsed();
        QVERIFY2(elapsed >= 250 && elapsed < 300 + 500, qPrintable(QString::number(elapsed)));
        QVERIFY(waitProcessGone(pid, 500));
    }

    void staleGenerationNeverReady()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("stale"));
        options.openTimeoutMs = 600;
        CaptureSupervisor supervisor(options);
        Recorder recorder(supervisor);
        bool readerEverOpen = false;
        QObject context;
        connect(&supervisor, &CaptureSupervisor::statusChanged, &context, [&]() {
            readerEverOpen = readerEverOpen || supervisor.reader()->isOpen();
        });
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Failed, 5000));
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        QVERIFY(!recorder.sawState(State::Ready));
        QVERIFY(!readerEverOpen);
        QVERIFY(!supervisor.reader()->isOpen());
    }

    void twoLeasesShareCapture()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        auto session = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        const Status ready = supervisor.status();
        const qint64 pid = supervisor.helperProcessId();
        auto testMic = supervisor.acquire(CaptureSupervisor::Demand::TestMic);
        QVERIFY(testMic.isActive());

        session.release();
        QTest::qWait(300);
        QCOMPARE(supervisor.status(), ready);
        QCOMPARE(supervisor.helperProcessId(), pid);
        QVERIFY(supervisor.reader()->isOpen());
        QVERIFY(!pullFrames(supervisor.reader(), 480, 2000).empty());

        // Moving a lease moves the demand; the moved-from lease is inert.
        CaptureSupervisor::Lease moved = std::move(testMic);
        QVERIFY(!testMic.isActive());
        QVERIFY(moved.isActive());
        testMic.release();
        QTest::qWait(100);
        QCOMPARE(supervisor.status().state, State::Ready);

        moved.release();
        QVERIFY(waitForState(supervisor, State::Closed, 3000));
        QVERIFY(waitProcessGone(pid, 1000));
    }

    void retryConfigureAndNewDemandAfterFailure()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("input-lost")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Failed, 5000));
        const quint32 first = supervisor.status().generation;

        // Same configuration: no new generation.
        supervisor.configure(AudioDeviceConfig{});
        QTest::qWait(200);
        QCOMPARE(supervisor.status().state, State::Failed);
        QCOMPARE(supervisor.status().generation, first);

        // retry() opens a new generation.
        supervisor.retry();
        QVERIFY(waitFor(supervisor, [first](const Status& s) {
            return s.state == State::Ready && s.generation > first;
        }, 5000));
        QVERIFY(waitForState(supervisor, State::Failed, 3000));
        const quint32 second = supervisor.status().generation;
        QVERIFY(second > first);

        // A different configuration opens a new generation for that device.
        AudioDeviceConfig other;
        other.deviceName = QStringLiteral("Other microphone");
        supervisor.configure(other);
        QVERIFY(waitFor(supervisor, [second](const Status& s) {
            return s.state == State::Ready && s.generation > second;
        }, 5000));
        QCOMPARE(supervisor.status().configuredDevice, QStringLiteral("Other microphone"));
        QVERIFY(waitForState(supervisor, State::Failed, 3000));
        const quint32 third = supervisor.status().generation;

        // Demand to zero keeps the failure visible; a new demand retries.
        lease.release();
        QTest::qWait(300);
        QCOMPARE(supervisor.status().state, State::Failed);
        QTRY_COMPARE_WITH_TIMEOUT(supervisor.helperProcessId(), qint64(0), 2500);
        auto again = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitFor(supervisor, [third](const Status& s) {
            return s.state == State::Ready && s.generation > third;
        }, 5000));
    }

    void retryWithoutDemandClearsFailure()
    {
        CaptureSupervisor unconfigured;
        auto lease = unconfigured.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(unconfigured, State::Failed, 2000));
        lease.release();
        QTest::qWait(100);
        QCOMPARE(unconfigured.status().state, State::Failed);
        unconfigured.retry();
        QVERIFY(waitForState(unconfigured, State::Closed, 2000));
        QCOMPARE(unconfigured.status().reason, Reason::None);
    }

    void configureRetiresCurrentGeneration()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        const quint32 first = supervisor.status().generation;
        const qint64 pid = supervisor.helperProcessId();

        Recorder recorder(supervisor);
        AudioDeviceConfig other;
        other.deviceName = QStringLiteral("USB microphone");
        supervisor.configure(other);
        QVERIFY(waitFor(supervisor, [first](const Status& s) {
            return s.state == State::Ready && s.generation > first;
        }, 5000));

        // The old generation never republished Ready, and the new one went
        // through Opening before Ready.
        QVERIFY(!recorder.seen.empty());
        QCOMPARE(recorder.seen.front().state, State::Opening);
        QVERIFY(recorder.seen.front().generation > first);
        for (const Status& s : recorder.seen) {
            QVERIFY(!(s.state == State::Ready && s.generation == first));
        }
        QCOMPARE(supervisor.status().configuredDevice, QStringLiteral("USB microphone"));
        QCOMPARE(supervisor.helperProcessId(), pid);             // same helper reused
        QVERIFY(!pullFrames(supervisor.reader(), 480, 2000).empty());
    }

    // V-HW-8: ProbeEnable goes only after Ready (the fake exits with code
    // 4 on one before), hits come back as probeHit(), disabled stops them
    // at the helper, and enabling again starts a new run there.
    void probeEnabledAfterReadyAndHitsForwarded()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("probe")));
        QSignalSpy hits(&supervisor, &CaptureSupervisor::probeHit);
        supervisor.setProbeEnabled(true);                       // before the helper exists
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::TestMic);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        QTRY_VERIFY_WITH_TIMEOUT(hits.size() >= 2, 3000);
        QCOMPARE(hits.at(0).at(0).toLongLong(), qint64(1000));
        QCOMPARE(hits.at(1).at(0).toLongLong(), qint64(1001));
        QCOMPARE(supervisor.status().state, State::Ready);

        // The fake marks the disable with a "Probe off" caps record once
        // it has stopped its hits; no fixed wait.
        QSignalSpy marks(&supervisor, &CaptureSupervisor::asioCaps);
        const auto probeOff = [&marks]() {
            for (const QList<QVariant>& args : marks) {
                if (args.at(0).value<CaptureProtocol::AsioCapsRecord>().driver
                    == QLatin1String("Probe off")) {
                    return true;
                }
            }
            return false;
        };
        supervisor.setProbeEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(probeOff(), 3000);
        hits.clear();

        supervisor.setProbeEnabled(true);                       // the helper is Ready now
        QTRY_VERIFY_WITH_TIMEOUT(hits.size() >= 1, 3000);
        QCOMPARE(hits.at(0).at(0).toLongLong(), qint64(2000));
        QCOMPARE(supervisor.status().state, State::Ready);
        lease.release();
        QVERIFY(waitForState(supervisor, State::Closed, 3000));
    }

    // Task 15 (R-AUD-19): the ASIO outputs play in the helper, so an ASIO
    // demand runs it with no microphone open, and the microphone's demand
    // coming and going leaves it running.
    void asioDemandKeepsTheHelperWithoutTheMic()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        QSignalSpy states(&supervisor, &CaptureSupervisor::asioState);
        QSignalSpy caps(&supervisor, &CaptureSupervisor::asioCaps);
        auto asio = supervisor.acquire(CaptureSupervisor::Demand::AsioDevice);
        QVERIFY(asio.isActive());
        QVERIFY(!supervisor.hasDemand());                       // not a microphone demand
        const qint64 pid = waitForPid(supervisor, 3000);
        QVERIFY(pid > 0);

        CaptureProtocol::AsioOpen open;
        open.serial = 7;
        open.driver = QStringLiteral("Fake ASIO");
        open.bufferFrames = 256;
        open.rate = 48000.0;
        CaptureProtocol::AsioOpenUse speakers;
        speakers.pair = AudioChannelPair{3, 2};
        speakers.direction = AudioDeviceDirection::Output;
        speakers.memory = QStringLiteral("/nereus-asio-test-m");
        speakers.wake = QStringLiteral("/nereus-asio-test-w");
        speakers.bytes = 4096;
        open.uses = {speakers};
        supervisor.openAsio(open);
        QTRY_VERIFY_WITH_TIMEOUT(states.size() >= 1, 3000);
        const auto running = qvariant_cast<CaptureProtocol::AsioState>(states.at(0).at(0));
        QCOMPARE(running.serial, quint32(7));
        QVERIFY(running.state == CaptureProtocol::AsioStateKind::Running);
        QCOMPARE(running.bufferFrames, 256);
        QCOMPARE(running.rate, 48000.0);
        QCOMPARE(supervisor.status().state, State::Closed);     // no microphone opened

        auto mic = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        QCOMPARE(supervisor.helperProcessId(), pid);
        mic.release();
        QVERIFY(waitForState(supervisor, State::Closed, 3000));
        QTest::qWait(300);
        QCOMPARE(supervisor.helperProcessId(), pid);            // still playing ASIO
        QVERIFY(!processIsGone(pid));

        supervisor.describeAsio(QStringLiteral("Fake ASIO"));
        QTRY_VERIFY_WITH_TIMEOUT(caps.size() >= 1, 3000);
        const auto record = qvariant_cast<CaptureProtocol::AsioCapsRecord>(caps.at(0).at(0));
        QCOMPARE(record.drivers, QStringList{QStringLiteral("Fake ASIO")});
        QCOMPARE(record.driver, QStringLiteral("Fake ASIO"));
        QVERIFY(record.caps.has_value());
        QCOMPARE(record.caps->outputChannels, 4);
        QCOMPARE(supervisor.helperProcessId(), pid);            // the demand still holds it

        asio.release();
        QVERIFY(waitProcessGone(pid, 3000));
        QTRY_COMPARE_WITH_TIMEOUT(supervisor.helperProcessId(), qint64(0), 3000);
    }

    // A helper started only to list the drivers stops after its answer.
    void describeAloneStopsTheHelperAfterTheAnswer()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        QSignalSpy caps(&supervisor, &CaptureSupervisor::asioCaps);
        supervisor.describeAsio(QString());
        const qint64 pid = waitForStartedPid(supervisor, 3000);
        QVERIFY(pid > 0);
        QTRY_VERIFY_WITH_TIMEOUT(caps.size() >= 1, 3000);
        const auto record = qvariant_cast<CaptureProtocol::AsioCapsRecord>(caps.at(0).at(0));
        QCOMPARE(record.drivers, QStringList{QStringLiteral("Fake ASIO")});
        QVERIFY(record.driver.isEmpty());
        QVERIFY(!record.caps.has_value());
        QVERIFY(waitProcessGone(pid, 3000));
        QCOMPARE(supervisor.status().state, State::Closed);
    }

    // A missing helper answers at once: no drivers, and an open fails.
    void asioWithoutTheHelperAnswersAtOnce()
    {
        CaptureSupervisor unconfigured;
        QSignalSpy caps(&unconfigured, &CaptureSupervisor::asioCaps);
        QSignalSpy states(&unconfigured, &CaptureSupervisor::asioState);
        unconfigured.describeAsio(QString());
        QTRY_VERIFY_WITH_TIMEOUT(caps.size() >= 1, 2000);
        QVERIFY(qvariant_cast<CaptureProtocol::AsioCapsRecord>(caps.at(0).at(0)).drivers.isEmpty());

        CaptureProtocol::AsioOpen open;
        open.serial = 3;
        open.driver = QStringLiteral("Fake ASIO");
        CaptureProtocol::AsioOpenUse use;
        use.memory = QStringLiteral("/nereus-asio-test-m");
        use.wake = QStringLiteral("/nereus-asio-test-w");
        use.bytes = 4096;
        open.uses = {use};
        unconfigured.openAsio(open);
        QTRY_VERIFY_WITH_TIMEOUT(states.size() >= 1, 2000);
        const auto failed = qvariant_cast<CaptureProtocol::AsioState>(states.at(0).at(0));
        QCOMPARE(failed.serial, quint32(3));
        QVERIFY(failed.state == CaptureProtocol::AsioStateKind::Failed);
        QCOMPARE(unconfigured.helperProcessId(), qint64(0));
    }

    // Without setProbeEnabled no ProbeEnable is sent and no hit arrives.
    void probeOffSendsNothing()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("probe")));
        QSignalSpy hits(&supervisor, &CaptureSupervisor::probeHit);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        QTest::qWait(300);
        QCOMPARE(hits.size(), 0);
        QCOMPARE(supervisor.status().state, State::Ready);
    }
};

int main(int argc, char* argv[])
{
    if (argc > 2 && std::strcmp(argv[1], "--fake-capture-child") == 0) {
        return NereusSDR::Test::runFakeCaptureChild(QString::fromLocal8Bit(argv[2]));
    }
    QCoreApplication app(argc, argv);
    TstCaptureSupervisor test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_capture_supervisor.moc"
