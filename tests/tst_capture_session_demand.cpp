// =================================================================
// tests/tst_capture_session_demand.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  AudioEngine and RadioModel PC
// microphone session demand tests (R-R3-36): receive startup and
// connect/disconnect never wait for capture, the local session holds a
// capture lease only while PC mic is selected, and a daemon-flagged model
// never starts the helper.  The helper is always the scripted fake, run by
// re-executing this binary with --fake-capture-child <scenario>; no real
// microphone is opened.
//
// Modification history (NereusSDR):
//   2026-09-22: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "core/AudioEngine.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/TxChannel.h"
#include "core/TxWorkerThread.h"
#include "core/audio/CaptureSupervisor.h"
#include "core/audio/TxMicSource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "fakes/ConnectableRadioModel.h"
#include "fakes/FakeAudioBus.h"
#include "fakes/FakeCaptureChild.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#endif

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using State = CaptureSupervisor::Status::State;

namespace {

// R-R3-36: start, connect and disconnect must not wait for capture.  The
// hang-open helper answers Open with Opening and then ignores everything,
// Stop included, so capture only ends when a supervisor deadline fires
// (Failed at the open deadline, a kill at the stop deadline) or the test
// ends the helper.  Anything that waited for capture would therefore return
// only after that end.  The checks below compare against that end, the
// capture's own completion, instead of wall time: wall time also counts
// WDSP channel teardown, thread joins and scheduling, which a busy machine
// stretches past any fixed bound (1115 ms at load 29.9) without any wait on
// capture.  The hello, open and stop deadlines are set far beyond the
// receive work the tests do, so a loaded machine cannot reach them first;
// each test waits until the helper has really started hanging (its marker
// file, below) and ends the helper itself once the order is proven.
constexpr int kHangingHelperDeadlineMs = 30000;

CaptureSupervisor::Options fakeOptions(const QString& scenario)
{
    CaptureSupervisor::Options options;
    options.program = QCoreApplication::applicationFilePath();
    options.arguments = {QStringLiteral("--fake-capture-child"), scenario};
    return options;
}

CaptureSupervisor::Options hangingHelperOptions()
{
    CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-open"));
    options.helloTimeoutMs = kHangingHelperDeadlineMs;
    options.openTimeoutMs = kHangingHelperDeadlineMs;
    options.stopTimeoutMs = kHangingHelperDeadlineMs;
    return options;
}

// Opened fake speakers so start() never touches a real output device.
void installFakeSpeakers(AudioEngine& engine)
{
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("Fake speakers"));
    const bool opened = speakers->open(format);
    Q_ASSERT(opened);
    Q_UNUSED(opened);
    engine.setSpeakersBusForTest(std::move(speakers));
}

// Installs the scripted helper and leaves the TX input to the capture
// reader (the fixture's own initializer would inject a fake TX input bus,
// which takes precedence over the reader).
auto withFakeHelper(const CaptureSupervisor::Options& options, bool pcCaptureAllowed = true)
{
    return [options, pcCaptureAllowed](RadioModel& model) {
        model.setPcCaptureAllowed(pcCaptureAllowed);
        model.audioEngine()->setStartInitializerForTest(installFakeSpeakers);
        model.audioEngine()->setCaptureSupervisorOptionsForTest(options);
    };
}

auto withFakeHelper(const QString& scenario, bool pcCaptureAllowed = true)
{
    return withFakeHelper(fakeOptions(scenario), pcCaptureAllowed);
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

// While alive, the hang-open fake creates <dir>/<pid> once it has answered
// Open and ignores everything after (FakeCaptureChild.h).  Until then a
// Shutdown still ends it, so a demand released that early proves nothing.
class HangMarkers {
public:
    HangMarkers()
    {
        qputenv(kVariable, m_dir.path().toLocal8Bit());
    }
    ~HangMarkers()
    {
        qunsetenv(kVariable);
    }
    HangMarkers(const HangMarkers&) = delete;
    HangMarkers& operator=(const HangMarkers&) = delete;

    bool isValid() const { return m_dir.isValid(); }
    bool hanging(qint64 pid) const
    {
        return pid > 0 && QFile::exists(m_dir.filePath(QString::number(pid)));
    }

private:
    static constexpr const char* kVariable = "NEREUS_FAKE_CAPTURE_HANG_DIR";
    QTemporaryDir m_dir;
};

// Ends the fake helper (this binary's own child) the way its stop deadline
// would, without waiting for that deadline.
void killFakeHelper(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (handle != nullptr) {
        TerminateProcess(handle, 9);
        CloseHandle(handle);
    }
#else
    ::kill(static_cast<pid_t>(pid), SIGKILL);
#endif
}

// The hanging helper pid is still running and still the engine's helper:
// whatever just returned did not wait for capture to end.
bool hangingHelperStillRunning(const AudioEngine& engine, qint64 pid)
{
    return pid > 0 && engine.captureHelperProcessIdForTest() == pid && !processIsGone(pid);
}

bool sawCaptureState(const QSignalSpy& spy, State state)
{
    for (const QList<QVariant>& args : spy) {
        if (args.at(0).value<CaptureSupervisor::Status>().state == state) {
            return true;
        }
    }
    return false;
}

// Minimal connection that counts TX I/Q blocks (as tst_tx_worker_thread).
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void sendTxIq(const float*, int) override
    {
        callCount.fetch_add(1, std::memory_order_relaxed);
    }

    std::atomic<int> callCount{0};
};

} // namespace

class TstCaptureSessionDemand : public QObject {
    Q_OBJECT

private slots:
    // start() opens no input: with PC mic selected and a helper that would
    // be Ready at once, no helper process starts and capture stays Closed.
    void engineStartOpensNoInput()
    {
        AudioEngine engine;
        engine.setCaptureSupervisorOptionsForTest(fakeOptions(QStringLiteral("ready")));
        installFakeSpeakers(engine);
        engine.onMicSourceChanged(true);

        engine.start();
        QTest::qWait(300);

        QCOMPARE(engine.captureHelperProcessIdForTest(), qint64(0));
        QCOMPARE(engine.captureStatus().state, State::Closed);
        QVERIFY(!engine.isPcMicOverrideActive());
        float buffer[64] = {};
        QCOMPARE(engine.pullTxMic(buffer, 64), 0);
        engine.stop();
    }

    // A helper that hangs in open never delays start() or a demand; the
    // TX input stays unavailable (zeros) and stop() and the release return
    // while the helper still runs.
    void hangingHelperDoesNotDelayEngineStart()
    {
        HangMarkers markers;
        QVERIFY(markers.isValid());
        AudioEngine engine;
        engine.setCaptureSupervisorOptionsForTest(hangingHelperOptions());
        installFakeSpeakers(engine);
        engine.onMicSourceChanged(true);
        QSignalSpy statusSpy(&engine, &AudioEngine::captureStatusChanged);

        QElapsedTimer timer;
        timer.start();
        engine.start();
        CaptureSupervisor::Lease lease =
            engine.acquireCaptureDemand(CaptureSupervisor::Demand::LocalSession);
        qInfo("start and demand %lld ms", static_cast<long long>(timer.elapsed()));
        QVERIFY(lease.isActive());

        // Returned before capture ended: a start or demand that waited for
        // the helper would come back only at the open deadline, Failed.
        QTRY_COMPARE_WITH_TIMEOUT(engine.captureStatus().state, State::Opening, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(engine.captureHelperProcessIdForTest() > 0, 10000);
        const qint64 pid = engine.captureHelperProcessIdForTest();
        QTRY_VERIFY_WITH_TIMEOUT(markers.hanging(pid), 10000);
        QVERIFY(hangingHelperStillRunning(engine, pid));
        QVERIFY(!sawCaptureState(statusSpy, State::Failed));
        QVERIFY(!engine.isPcMicOverrideActive());
        QCOMPARE(engine.pcMicInputLevel(), 0.0f);
        float buffer[64] = {};
        QCOMPARE(engine.pullTxMic(buffer, 64), 0);

        timer.restart();
        engine.stop();
        lease.release();
        qInfo("stop and release %lld ms", static_cast<long long>(timer.elapsed()));
        // The helper ignores Stop, so it is still running unless something
        // waited for its stop deadline.
        QVERIFY(hangingHelperStillRunning(engine, pid));

        killFakeHelper(pid);
        QTRY_COMPARE_WITH_TIMEOUT(engine.captureHelperProcessIdForTest(), qint64(0), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine.captureStatus().state, State::Closed, 5000);
    }

    // setTxInputConfig stores the selection and emits as before, without
    // opening anything when there is no demand.
    void setTxInputConfigOnlyConfigures()
    {
        AudioEngine engine;
        engine.setCaptureSupervisorOptionsForTest(fakeOptions(QStringLiteral("ready")));
        QSignalSpy spy(&engine, &AudioEngine::txInputConfigChanged);

        AudioDeviceConfig cfg;
        cfg.deviceName = QStringLiteral("USB microphone");
        engine.setTxInputConfig(cfg);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(engine.txInputConfig().deviceName, QStringLiteral("USB microphone"));
        QTest::qWait(200);
        QCOMPARE(engine.captureHelperProcessIdForTest(), qint64(0));
    }

    // With a Ready helper the TX worker reads the fake tone from the
    // capture reader.
    void readyHelperFeedsTxWorker()
    {
        AudioEngine engine;
        engine.setCaptureSupervisorOptionsForTest(fakeOptions(QStringLiteral("ready")));
        engine.onMicSourceChanged(true);
        QSignalSpy statusSpy(&engine, &AudioEngine::captureStatusChanged);
        CaptureSupervisor::Lease lease =
            engine.acquireCaptureDemand(CaptureSupervisor::Demand::LocalSession);
        QTRY_COMPARE_WITH_TIMEOUT(engine.captureStatus().state, State::Ready, 5000);
        QVERIFY(statusSpy.count() > 0);
        QVERIFY(engine.isPcMicOverrideActive());
        QTRY_VERIFY_WITH_TIMEOUT(engine.pcMicInputLevel() > 0.1f, 2000);

        constexpr int kChannelId = 1;
        constexpr int kBufSize = TxWorkerThread::kBlockFrames;
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);
        TxMicSource src;
        src.start();
        TxWorkerThread worker;
        worker.setTxChannel(&ch);
        worker.setAudioEngine(&engine);
        worker.setMicSource(&src);

        // Let the reader hold at least one block of tone.
        QTest::qWait(50);
        std::vector<float> radio(kBufSize, 0.0f);
        src.inbound(radio.data(), kBufSize);
        worker.tickForTest();
        QCOMPARE(conn.callCount.load(), 1);

        double peak = 0.0;
        const auto& in = ch.inForTest();
        for (int i = 0; i < kBufSize; ++i) {
            peak = std::max(peak, std::abs(in[2 * i + 0]));
            QCOMPARE(in[2 * i + 1], 0.0);
        }
        QVERIFY2(peak > 0.1, qPrintable(QString::number(peak)));

        src.stop();
        lease.release();
    }

    // Full local connect and disconnect through the connectable fixture
    // with a hanging helper: the radio receive path starts, the session
    // holds a demand (the helper is running and still Opening), and
    // disconnect and a reconnect both finish while the helper still hangs.
    void hangingHelperDoesNotDelayConnectOrDisconnect()
    {
        // R-R3-39: the receive channels open on the receive lane, so a cold
        // first connect no longer holds the event loop while FFTW plans; the
        // hanging helper's 10 s open timeout would then run out before the
        // check below. Plan first with a helper that answers, so the measured
        // connect is warm, as the reconnect below already is.
        {
            auto warm = ConnectableRadioModel::create(
                10000, RadioModel::Role::Local, withFakeHelper(QStringLiteral("ready"), false));
            QVERIFY(warm);
            QVERIFY(warm->model().waitForReceiveLaneForTest());
        }
        HangMarkers markers;
        QVERIFY(markers.isValid());
        QElapsedTimer connectTimer;
        connectTimer.start();
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, withFakeHelper(hangingHelperOptions()));
        QVERIFY(harness);
        // Informational: the first connect in a process also pays FFTW's
        // cold planning cost (see tst_connectable_radio_model).
        qInfo("first connect %lld ms", static_cast<long long>(connectTimer.elapsed()));
        RadioModel& model = harness->model();
        QCOMPARE(model.connectionState(), ConnectionState::Connected);
        QVERIFY(!model.slices().isEmpty());
        QVERIFY(model.slices().first()->streamIndex() >= 0);
        QVERIFY(model.pcCaptureRequired());
        AudioEngine* const engine = model.audioEngine();
        QSignalSpy statusSpy(engine, &AudioEngine::captureStatusChanged);
        // Connected before capture ended: a connect that waited for the
        // helper would come back only at the open deadline, Failed.
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, State::Opening, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureHelperProcessIdForTest() > 0, 10000);
        const qint64 firstPid = engine->captureHelperProcessIdForTest();
        QTRY_VERIFY_WITH_TIMEOUT(markers.hanging(firstPid), 10000);
        QVERIFY(hangingHelperStillRunning(*engine, firstPid));

        QElapsedTimer timer;
        timer.start();
        model.disconnectFromRadio();
        const qint64 disconnectMs = timer.elapsed();
        // Disconnect released the demand and returned while the helper,
        // which ignores Stop, still runs: it did not wait for capture.
        QVERIFY(hangingHelperStillRunning(*engine, firstPid));
        killFakeHelper(firstPid);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, State::Closed, 5000);

        RadioDiscovery::clearHoldOffForTest();
        statusSpy.clear();
        timer.restart();
        model.connectToRadioPreservingSlices(harness->radioInfo());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionState(), ConnectionState::Connected, 10000);
        const qint64 reconnectMs = timer.elapsed();
        QVERIFY(model.slices().first()->streamIndex() >= 0);
        // Reconnected before capture ended, as on the first connect.
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, State::Opening, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureHelperProcessIdForTest() > 0, 10000);
        const qint64 secondPid = engine->captureHelperProcessIdForTest();
        QVERIFY(secondPid != firstPid);
        QTRY_VERIFY_WITH_TIMEOUT(markers.hanging(secondPid), 10000);
        QVERIFY(hangingHelperStillRunning(*engine, secondPid));
        QVERIFY(!sawCaptureState(statusSpy, State::Failed));

        timer.restart();
        model.disconnectFromRadio();
        const qint64 secondDisconnectMs = timer.elapsed();
        qInfo("disconnect %lld ms, reconnect %lld ms, disconnect %lld ms",
              static_cast<long long>(disconnectMs), static_cast<long long>(reconnectMs),
              static_cast<long long>(secondDisconnectMs));
        QVERIFY(hangingHelperStillRunning(*engine, secondPid));
        killFakeHelper(secondPid);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, State::Closed, 5000);
    }

    // Switching the source away from PC releases the session's lease and
    // the helper exits; switching back takes a new demand.
    void sourceSwitchReleasesLease()
    {
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, withFakeHelper(QStringLiteral("ready")));
        QVERIFY(harness);
        RadioModel& model = harness->model();
        AudioEngine* const engine = model.audioEngine();
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, State::Ready, 5000);
        const qint64 pid = engine->captureHelperProcessIdForTest();
        QVERIFY(pid > 0);

        // The fixture's Hermes Lite 2 has no mic jack, which locks the
        // source to PC; lift the lock so Radio can be selected.
        model.transmitModel().setMicSourceLocked(false);
        model.transmitModel().setMicSource(MicSource::Radio);
        QVERIFY(!model.pcCaptureRequired());
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, State::Closed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(processIsGone(pid), 5000);

        model.transmitModel().setMicSource(MicSource::Pc);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, State::Ready, 5000);

        model.disconnectFromRadio();
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, State::Closed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
    }

    // A daemon-flagged model (setPcCaptureAllowed(false), as DaemonApp
    // does before connecting) connects with PC mic selected and never
    // starts the helper.
    void daemonFlaggedModelNeverStartsHelper()
    {
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local,
            withFakeHelper(QStringLiteral("ready"), /*pcCaptureAllowed=*/false));
        QVERIFY(harness);
        RadioModel& model = harness->model();
        AudioEngine* const engine = model.audioEngine();
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
        QVERIFY(!model.pcCaptureRequired());

        QTest::qWait(300);
        QCOMPARE(engine->captureHelperProcessIdForTest(), qint64(0));
        QCOMPARE(engine->captureStatus().state, State::Closed);

        model.disconnectFromRadio();
        QCOMPARE(engine->captureHelperProcessIdForTest(), qint64(0));
    }

    // A remote-role model never demands PC capture.
    void remoteRoleNeverRequiresCapture()
    {
        RadioModel model(RadioModel::Role::Remote);
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
        QVERIFY(!model.pcCaptureRequired());
        QCOMPARE(model.audioEngine()->captureStatus().state, State::Closed);
    }
};

int main(int argc, char* argv[])
{
    if (argc > 2 && std::strcmp(argv[1], "--fake-capture-child") == 0) {
        return NereusSDR::Test::runFakeCaptureChild(QString::fromLocal8Bit(argv[2]));
    }
    QCoreApplication app(argc, argv);
    TstCaptureSessionDemand test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_capture_session_demand.moc"
