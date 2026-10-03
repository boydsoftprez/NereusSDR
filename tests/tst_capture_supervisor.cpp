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
// =================================================================

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QSignalSpy>

#include <cstring>
#include <functional>
#include <memory>
#include <vector>

#include "core/audio/CaptureAudioBus.h"
#include "core/audio/CaptureSupervisor.h"
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

// Pulls until at least `frames` frames were read or the time runs out.
std::vector<float> pullFrames(CaptureAudioBus* reader, int frames, int timeoutMs)
{
    std::vector<float> out;
    std::vector<float> block(256);
    QElapsedTimer timer;
    timer.start();
    while (static_cast<int>(out.size()) < frames && timer.elapsed() < timeoutMs) {
        const qint64 got = reader->pull(reinterpret_cast<char*>(block.data()),
                                        static_cast<qint64>(block.size() * sizeof(float)));
        if (got > 0) {
            out.insert(out.end(), block.begin(), block.begin() + got / static_cast<qint64>(sizeof(float)));
        } else {
            QTest::qWait(5);
        }
    }
    return out;
}

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

        std::vector<float> samples(480, 0.25f);
        samples[7] = -0.75f;
        QCOMPARE(reader.writeFrames(samples.data(), 480), 480);
        QCOMPARE(reader.txLevel(), 0.75f);
        char buffer[64];
        QCOMPARE(reader.pull(buffer, sizeof(buffer)), qint64(0));   // unavailable

        reader.setAvailable(true);
        QVERIFY(reader.isOpen());
        QCOMPARE(reader.pull(buffer, 6), qint64(4));                // whole frames only
        reader.close();
        QVERIFY(reader.isOpen());                                   // close() changes nothing

        reader.setAvailable(false);
        QCOMPARE(reader.txLevel(), 0.0f);
        reader.setAvailable(true);
        QCOMPARE(reader.pull(buffer, sizeof(buffer)), qint64(0));   // retired by withdrawal
    }

    void readerRingHolds4800FramesAndCountsDrops()
    {
        CaptureAudioBus reader;
        reader.setAvailable(true);
        std::vector<float> block(1000);
        for (int i = 0; i < 6; ++i) {
            for (int n = 0; n < 1000; ++n) {
                block[static_cast<std::size_t>(n)] = static_cast<float>(i * 1000 + n) / 10000.0f;
            }
            reader.writeFrames(block.data(), 1000);
        }
        QCOMPARE(CaptureAudioBus::kRingFrames, 4800);
        QCOMPARE(reader.bufferedFrames(), 4800);
        QCOMPARE(reader.droppedFrames(), quint64(1200));

        std::vector<float> out(6000);
        const qint64 got = reader.pull(reinterpret_cast<char*>(out.data()),
                                       static_cast<qint64>(out.size() * sizeof(float)));
        QCOMPARE(got, qint64(4800 * 4));
        QCOMPARE(out[0], 0.0f);                          // oldest kept, newest dropped
        QCOMPARE(out[4799], 4799.0f / 10000.0f);

        // Wraps correctly once space is free, and flush() retires everything.
        QCOMPARE(reader.writeFrames(block.data(), 1000), 1000);
        reader.flush();
        QCOMPARE(reader.bufferedFrames(), 0);
        QCOMPARE(reader.pull(reinterpret_cast<char*>(out.data()), 4000), qint64(0));
        QCOMPARE(reader.writeFrames(block.data(), 10), 10);
        QCOMPARE(reader.pull(reinterpret_cast<char*>(out.data()), 4000), qint64(40));
        QCOMPARE(out[0], block[0]);
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
        QVERIFY(recorder.sawState(State::Opening));
        QVERIFY(reader->isOpen());
        const qint64 pid = supervisor.helperProcessId();
        QVERIFY(pid > 0);

        const std::vector<float> samples = pullFrames(reader, 960, 3000);
        QVERIFY(samples.size() >= 960);
        float peak = 0.0f;
        for (float s : samples) {
            peak = std::max(peak, std::fabs(s));
        }
        QVERIFY2(peak > 0.4f && peak <= 0.5001f, qPrintable(QString::number(peak)));
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
    }

    void protocolErrors()
    {
        QFETCH(QString, scenario);
        CaptureSupervisor supervisor(fakeOptions(scenario));
        Recorder recorder(supervisor);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        const qint64 pid = waitForPid(supervisor, 3000);
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
