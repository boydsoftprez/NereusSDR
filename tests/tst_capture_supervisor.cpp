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
//   2026-10-09: native audio plan final fix wave (R-R3-36): the hello,
//               open and stop deadlines fired early through
//               fireDeadlinesNowForTest() fail nothing until the whole
//               interval has passed.  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-11: a Pcm record is refused before Ready and after it, each
//               in a case of its own that no timing decides: the one row
//               used to pass or fail on whether the ring's first wake or
//               the record reached the supervisor first.  The helper's
//               exit and its lost input after Ready, and the retry case,
//               no longer race a 100 ms clock in the fake: it acts on a
//               ProbeEnable sent here once Ready is seen.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11: the scripted helper's start is no longer on any case's
//               clock but the two that test the hello deadline: 30 s for
//               the hello and for a case's first wait after a start
//               (kHelperStartMs), the hang-open cases wait for the fake's
//               marker in place of a set time and measure from it, the
//               early open deadline is measured from its arming, and the
//               permission hold ends when the case says so.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-12: generation-tagged open-step diagnostic regression.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-16: the persistent helper: one standing helper serves every
//               generation, and a stuck one is still killed and replaced;
//               an open waits out the helper's warm-up (its own bound), a
//               helper that never starts is replaced once only, one that
//               warms and ends no more than three times a minute, and
//               CaptureStandby holds work until the helper has warmed or
//               its bound.  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-16: Warm names the window's bundle for its Mic Mode until a
//               helper has answered for it, and the answer is logged once;
//               an invalid bundle id is never sent; a Warmed out of step
//               with its Warm is a protocol error.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "core/audio/CaptureAudioBus.h"
#include "core/audio/CaptureStandby.h"
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

// What a case allows a newly started helper before it calls the start
// failed (FakeCaptureChild.h): the window's hello deadline, and the case's
// own wait for the first thing that start leads to.
constexpr int kHelperStartMs = Test::kFakeCaptureChildStartMs;

CaptureSupervisor::Options fakeOptions(const QString& scenario)
{
    CaptureSupervisor::Options options;
    options.program = QCoreApplication::applicationFilePath();
    options.arguments = {QStringLiteral("--fake-capture-child"), scenario};
    options.helloTimeoutMs = kHelperStartMs;                    // 3000 in production
    return options;
}

// A program path where no helper exists.  A case that means "no helper"
// uses this rather than the default lookup, which finds the product's
// nereus-audio-capture when it sits beside this test binary (a combined
// runtime folder) and would start the real helper and open a microphone.
CaptureSupervisor::Options missingHelperOptions()
{
    CaptureSupervisor::Options options;
    options.program = QDir(QDir::tempPath()).absoluteFilePath(
        QStringLiteral("nereus-audio-capture-that-does-not-exist"));
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

// Counts the warnings (or messages of another type) that contain a text,
// from any thread, while it lives; every message still reaches the test log.
class WarningWatch {
public:
    explicit WarningWatch(QString text, QtMsgType type = QtWarningMsg)
        : m_previous(qInstallMessageHandler(&WarningWatch::handle))
    {
        QMutexLocker lock(&s_mutex);
        s_text = std::move(text);
        s_type = type;
        s_count = 0;
        s_previous = m_previous;
    }
    ~WarningWatch() { qInstallMessageHandler(m_previous); }
    WarningWatch(const WarningWatch&) = delete;
    WarningWatch& operator=(const WarningWatch&) = delete;

    int count() const
    {
        QMutexLocker lock(&s_mutex);
        return s_count;
    }

private:
    static void handle(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        QtMessageHandler previous = nullptr;
        {
            QMutexLocker lock(&s_mutex);
            if (type == s_type && message.contains(s_text)) {
                ++s_count;
            }
            previous = s_previous;
        }
        if (previous) {
            previous(type, context, message);
        }
    }

    QtMessageHandler m_previous;
    static inline QMutex s_mutex;
    static inline QString s_text;
    static inline QtMsgType s_type = QtWarningMsg;
    static inline int s_count = 0;
    static inline QtMessageHandler s_previous = nullptr;
};

// Fires the named deadline early once it is armed (R-R3-36); false if it
// never armed within timeoutMs.  *armedForMs, when given, gets an upper
// bound on how long the deadline had been armed when it was fired: the time
// since the last poll that found it unarmed, or -1 when the first poll
// already found it armed.
bool fireOnceArmed(CaptureSupervisor& supervisor, int deadline, int timeoutMs,
                   qint64* armedForMs = nullptr)
{
    QElapsedTimer timer;
    timer.start();
    qint64 lastUnarmedMs = -1;
    while (timer.elapsed() < timeoutMs) {
        const qint64 pollMs = timer.elapsed();
        if (supervisor.fireDeadlinesNowForTest(deadline) == deadline) {
            if (armedForMs != nullptr) {
                *armedForMs = lastUnarmedMs < 0 ? -1 : timer.elapsed() - lastUnarmedMs;
            }
            return true;
        }
        lastUnarmedMs = pollMs;
        QTest::qWait(1);
    }
    return false;
}

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

// While alive, the hang-open fake creates <dir>/<pid> once it has answered
// Open and ignores everything after (FakeCaptureChild.h), as in
// tst_capture_session_demand.  A case waits for that file in place of a set
// time: the helper really is hanging when the case goes on, and the helper's
// start is no part of what the case then measures.
class HangMarkers {
public:
    HangMarkers() { qputenv(kVariable, m_dir.path().toLocal8Bit()); }
    ~HangMarkers() { qunsetenv(kVariable); }
    HangMarkers(const HangMarkers&) = delete;
    HangMarkers& operator=(const HangMarkers&) = delete;

    bool isValid() const { return m_dir.isValid(); }
    bool waitHanging(qint64 pid, int timeoutMs) const
    {
        const QString marker = m_dir.filePath(QString::number(pid));
        QElapsedTimer timer;
        timer.start();
        while (!QFile::exists(marker)) {
            if (timer.elapsed() > timeoutMs) {
                return false;
            }
            QTest::qWait(5);
        }
        return true;
    }

private:
    static constexpr const char* kVariable = "NEREUS_FAKE_CAPTURE_HANG_DIR";
    QTemporaryDir m_dir;
};

// The file whose presence makes the permission-while-marked fake hold
// Permission (FakeCaptureChild.h); release() removes it.
class PermissionHold {
public:
    PermissionHold()
        : m_file(m_dir.filePath(QStringLiteral("held")))
    {
        QFile file(m_file);
        m_valid = m_dir.isValid() && file.open(QIODevice::WriteOnly);
        file.close();
        qputenv(kVariable, m_file.toLocal8Bit());
    }
    ~PermissionHold() { qunsetenv(kVariable); }
    PermissionHold(const PermissionHold&) = delete;
    PermissionHold& operator=(const PermissionHold&) = delete;

    bool isValid() const { return m_valid; }
    bool release() { return QFile::remove(m_file); }

private:
    static constexpr const char* kVariable = "NEREUS_FAKE_CAPTURE_PERMISSION_FILE";
    QTemporaryDir m_dir;
    QString m_file;
    bool m_valid = false;
};

// The file whose presence makes the warm-while-marked fake hold Warm
// (FakeCaptureChild.h); release() removes it.
class WarmHold {
public:
    WarmHold()
        : m_file(m_dir.filePath(QStringLiteral("held")))
    {
        QFile file(m_file);
        m_valid = m_dir.isValid() && file.open(QIODevice::WriteOnly);
        file.close();
        qputenv(kVariable, m_file.toLocal8Bit());
    }
    ~WarmHold() { qunsetenv(kVariable); }
    WarmHold(const WarmHold&) = delete;
    WarmHold& operator=(const WarmHold&) = delete;

    bool isValid() const { return m_valid; }
    bool release() { return QFile::remove(m_file); }

private:
    static constexpr const char* kVariable = "NEREUS_FAKE_CAPTURE_WARM_FILE";
    QTemporaryDir m_dir;
    QString m_file;
    bool m_valid = false;
};

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
        // The default lookup is covered by tst_capture_helper_locator; here
        // both demands meet an explicit missing program.
        CaptureSupervisor unconfigured(missingHelperOptions());
        auto lease = unconfigured.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(lease.isActive());
        QVERIFY(waitForState(unconfigured, State::Failed, 2000));
        QCOMPARE(unconfigured.status().reason, Reason::HelperMissing);
        QCOMPARE(unconfigured.helperProcessId(), qint64(0));

        CaptureSupervisor absent(missingHelperOptions());
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
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
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
        HangMarkers markers;
        QVERIFY(markers.isValid());
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-open"));
        options.openTimeoutMs = 500;                            // 10000 in production
        CaptureSupervisor supervisor(options);
        QElapsedTimer timer;
        timer.start();
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        const qint64 pid = waitForPid(supervisor, 3000);
        QVERIFY(pid > 0);
        // The open deadline runs from the Open, which the helper has
        // answered once its marker is there.  The upper bound is taken from
        // that point, so the helper's start is no part of it.
        QVERIFY(markers.waitHanging(pid, kHelperStartMs));
        QElapsedTimer sinceOpen;
        sinceOpen.start();
        QVERIFY(waitForState(supervisor, State::Failed, 5000));
        const qint64 elapsed = timer.elapsed();
        const qint64 afterOpen = sinceOpen.elapsed();
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        QVERIFY2(elapsed >= 500, qPrintable(QString::number(elapsed)));
        QVERIFY2(afterOpen < 3000, qPrintable(QString::number(afterOpen)));
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
        HangMarkers markers;
        QVERIFY(markers.isValid());
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("hang-open")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        const qint64 pid = waitForPid(supervisor, 3000);
        QVERIFY(pid > 0);
        QVERIFY(waitForState(supervisor, State::Opening, 3000));
        QVERIFY(markers.waitHanging(pid, kHelperStartMs));      // Open answered, child now silent

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
        HangMarkers markers;
        QVERIFY(markers.isValid());
        auto supervisor = std::make_unique<CaptureSupervisor>(fakeOptions(QStringLiteral("hang-open")));
        auto lease = supervisor->acquire(CaptureSupervisor::Demand::TestMic);
        const qint64 pid = waitForPid(*supervisor, 3000);
        QVERIFY(pid > 0);
        QVERIFY(waitForState(*supervisor, State::Opening, 3000));
        QVERIFY(markers.waitHanging(pid, kHelperStartMs));      // Open answered, child now silent

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

    // The fake holds Permission until this case lets it go, which it does
    // only after more than the whole open deadline has gone by.  The open
    // still succeeds, so the deadline did not run during the hold; the time
    // it has left afterwards is the whole deadline less what went by before
    // the helper asked.
    void permissionPausesOpenDeadline()
    {
        PermissionHold hold;
        QVERIFY(hold.isValid());
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("permission-while-marked"));
        options.openTimeoutMs = 2000;                           // 10000 in production
        CaptureSupervisor supervisor(options);
        Recorder recorder(supervisor);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::PreparingPermission, kHelperStartMs));
        QTest::qWait(options.openTimeoutMs + 200);              // the hold outlasts the deadline
        QCOMPARE(supervisor.status().state, State::PreparingPermission);
        QVERIFY(hold.release());
        QVERIFY(waitFor(supervisor, [](const Status& s) {
            return s.state == State::Ready || s.state == State::Failed;
        }, 5000));
        QCOMPARE(supervisor.status().state, State::Ready);
        QVERIFY(recorder.sawState(State::PreparingPermission));
        QVERIFY(!recorder.sawState(State::Failed));
        QVERIFY(supervisor.reader()->isOpen());
    }

    // The helper exits while the mic is Ready.  The fake exits in answer
    // to ProbeEnable, sent here once Ready is seen, so the exit cannot
    // come first and nothing waits a set time.
    void crashAfterReadyFails()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("crash-on-probe-enable")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
        supervisor.setProbeEnabled(true);
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
        // helper did not build is refused.  This fake builds no ring, so
        // Ready (the helper's Ready and the ring's first wake) cannot come
        // first; pcmRecordAfterReadyFails covers the record after Ready.
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
        QVERIFY(waitForState(supervisor, State::Failed, kHelperStartMs));
        QCOMPARE(supervisor.status().reason, Reason::ProtocolError);
        QVERIFY(!recorder.sawState(State::Ready));
        QVERIFY(waitProcessGone(pid, 1000));
        QVERIFY(!supervisor.reader()->isOpen());
    }

    // R-AUD-17: a Pcm record is refused while the mic is Ready too.  The
    // fake sends it in answer to ProbeEnable, sent here once Ready is
    // seen, so the order is fixed and nothing waits a set time.
    void pcmRecordAfterReadyFails()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("pcm-record-on-probe-enable")));
        Recorder recorder(supervisor);
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
        QVERIFY(supervisor.reader()->isOpen());
        const qint64 pid = supervisor.helperProcessId();
        QVERIFY(pid > 0);
        supervisor.setProbeEnabled(true);
        QVERIFY(waitForState(supervisor, State::Failed, 3000));
        QCOMPARE(supervisor.status().reason, Reason::ProtocolError);
        QVERIFY(recorder.sawState(State::Ready));
        QVERIFY(waitProcessGone(pid, 1000));
        QVERIFY(!supervisor.reader()->isOpen());
        char buffer[64];
        QCOMPARE(supervisor.reader()->pull(buffer, sizeof(buffer)), qint64(0));
    }

    // The helper's input stops while the mic is Ready; the order is fixed
    // the same way as in crashAfterReadyFails.
    void inputLostFails()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("input-lost-on-probe-enable")));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
        supervisor.setProbeEnabled(true);
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
        QVERIFY(waitForState(supervisor, State::Failed, kHelperStartMs));
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
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
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
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
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

    // R-R3-36: a deadline whose handler runs before its interval has gone
    // by (a timer firing early) fails nothing and warns nothing; the
    // failure still comes once the whole interval has passed.
    void earlyHelloDeadlineWaitsOutTheRest()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("no-hello"));
        options.helloTimeoutMs = 1000;
        CaptureSupervisor supervisor(options);
        WarningWatch warned(QStringLiteral("sent no hello within"));
        QElapsedTimer timer;
        timer.start();
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(fireOnceArmed(supervisor, CaptureSupervisor::HelloDeadline, 900));
        QCoreApplication::processEvents();
        QVERIFY2(timer.elapsed() < 1000, qPrintable(QString::number(timer.elapsed())));
        QCOMPARE(warned.count(), 0);
        QVERIFY(supervisor.status().state != State::Failed);

        QVERIFY(waitForState(supervisor, State::Failed, 5000));
        QCOMPARE(supervisor.status().reason, Reason::HelperDidNotStart);
        QVERIFY2(timer.elapsed() >= 1000, qPrintable(QString::number(timer.elapsed())));
        QCOMPARE(warned.count(), 1);
    }

    // The open deadline's warning names the last open step the helper
    // reported, so a slow open says where its time went.
    void openTimeoutNamesTheHelpersLastStep()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-open-after-step"));
        options.openTimeoutMs = 500;                            // 10000 in production
        CaptureSupervisor supervisor(options);
        WarningWatch warned(QStringLiteral("the helper's last open step: devices listed at 12 ms"));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Failed, kHelperStartMs));
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        QCOMPARE(warned.count(), 1);
    }

    void openTimeoutIgnoresStaleAndMalformedTaggedSteps()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-open-after-tagged-steps"));
        options.openTimeoutMs = 500;
        CaptureSupervisor supervisor(options);
        WarningWatch warned(QStringLiteral("the helper's last open step: current query at 12 ms"));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Failed, kHelperStartMs));
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        QCOMPARE(warned.count(), 1);
    }

    void openTimeoutSaysWhenTheHelperReportedNoStep()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-open"));
        options.openTimeoutMs = 500;                            // 10000 in production
        CaptureSupervisor supervisor(options);
        WarningWatch warned(QStringLiteral("the helper reported no open step"));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Failed, kHelperStartMs));
        QCOMPARE(warned.count(), 1);
    }

    void earlyOpenDeadlineWaitsOutTheRest()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-open"));
        options.openTimeoutMs = 1000;
        CaptureSupervisor supervisor(options);
        WarningWatch warned(QStringLiteral("no microphone samples within"));
        QElapsedTimer timer;
        timer.start();
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        // The open deadline is armed once the helper has said hello, however
        // long its start took, so "early" is measured from the arming.
        qint64 armedForMs = -1;
        QVERIFY(fireOnceArmed(supervisor, CaptureSupervisor::OpenDeadline, kHelperStartMs,
                              &armedForMs));
        QCoreApplication::processEvents();
        const qint64 early = armedForMs >= 0 ? armedForMs : timer.elapsed();
        QVERIFY2(early < 1000, qPrintable(QString::number(early)));
        QCOMPARE(warned.count(), 0);
        QVERIFY(supervisor.status().state != State::Failed);

        QVERIFY(waitForState(supervisor, State::Failed, 5000));
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        QVERIFY2(timer.elapsed() >= 1000, qPrintable(QString::number(timer.elapsed())));
        QCOMPARE(warned.count(), 1);
    }

    void earlyStopDeadlineWaitsOutTheRest()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("ignore-stop"));
        options.stopTimeoutMs = 1000;
        CaptureSupervisor supervisor(options);
        WarningWatch warned(QStringLiteral("did not stop within"));
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
        const qint64 pid = supervisor.helperProcessId();
        QVERIFY(pid > 0);

        QElapsedTimer timer;
        timer.start();
        lease.release();
        QVERIFY(fireOnceArmed(supervisor, CaptureSupervisor::StopDeadline, 900));
        QCoreApplication::processEvents();
        QVERIFY2(timer.elapsed() < 1000, qPrintable(QString::number(timer.elapsed())));
        QCOMPARE(warned.count(), 0);
        QCOMPARE(supervisor.helperProcessId(), pid);            // not killed

        QVERIFY(waitForState(supervisor, State::Closed, 5000));
        QVERIFY2(timer.elapsed() >= 1000, qPrintable(QString::number(timer.elapsed())));
        QCOMPARE(warned.count(), 1);
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
        QVERIFY(waitForState(supervisor, State::Failed, kHelperStartMs));
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        QVERIFY(!recorder.sawState(State::Ready));
        QVERIFY(!readerEverOpen);
        QVERIFY(!supervisor.reader()->isOpen());
    }

    void twoLeasesShareCapture()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        auto session = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
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
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("input-lost-on-probe-enable")));
        // The fake loses its input in answer to ProbeEnable, so each
        // failure follows a Ready seen here.  The probe goes off again at
        // once: left on, the window would send it at the next generation's
        // helper Ready, before its own Ready is published.
        const auto loseInput = [&supervisor]() {
            supervisor.setProbeEnabled(true);
            const bool failed = waitForState(supervisor, State::Failed, 3000);
            supervisor.setProbeEnabled(false);
            return failed;
        };
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
        QVERIFY(loseInput());
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
        QVERIFY(loseInput());
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
        QVERIFY(loseInput());
        const quint32 third = supervisor.status().generation;

        // Demand to zero keeps the failure visible; a new demand retries.
        lease.release();
        QTest::qWait(300);
        QCOMPARE(supervisor.status().state, State::Failed);
        QTRY_COMPARE_WITH_TIMEOUT(supervisor.helperProcessId(), qint64(0), 2500);
        auto again = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitFor(supervisor, [third](const Status& s) {
            return s.state == State::Ready && s.generation > third;
        }, kHelperStartMs));                                    // a new helper
    }

    void retryWithoutDemandClearsFailure()
    {
        CaptureSupervisor unconfigured(missingHelperOptions());
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
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
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
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
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
        QTRY_VERIFY_WITH_TIMEOUT(states.size() >= 1, kHelperStartMs);
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
        QTRY_VERIFY_WITH_TIMEOUT(caps.size() >= 1, kHelperStartMs);
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
        CaptureSupervisor unconfigured(missingHelperOptions());
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
        QVERIFY(waitForState(supervisor, State::Ready, kHelperStartMs));
        QTest::qWait(300);
        QCOMPARE(hits.size(), 0);
        QCOMPARE(supervisor.status().state, State::Ready);
    }

    // ── The persistent helper (2026-10-16) ──────────────────────────────────

    // Standby starts one helper and warms it with no microphone open.
    // Every generation after that (an open, a close, a reopen for the
    // remote window, a device change, a retry) runs on that same helper;
    // only the standby's release lets it stop.
    void standbyHelperServesEveryGeneration()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        QVERIFY(standby.isActive());
        QVERIFY(!supervisor.hasDemand());                       // not a microphone demand
        QTRY_VERIFY_WITH_TIMEOUT(warmed.size() >= 1, kHelperStartMs);
        const qint64 pid = supervisor.helperProcessId();
        QVERIFY(pid > 0);
        QCOMPARE(warmed.at(0).at(0).toLongLong(), pid);
        QCOMPARE(supervisor.status().state, State::Closed);     // no microphone opened
        QCOMPARE(supervisor.status().generation, quint32(0));
        QVERIFY(!supervisor.reader()->isOpen());

        auto mic = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Ready, 5000));
        const quint32 opened = supervisor.status().generation;
        QCOMPARE(supervisor.helperProcessId(), pid);

        mic.release();
        QVERIFY(waitForState(supervisor, State::Closed, 3000));
        QTest::qWait(300);
        QCOMPARE(supervisor.helperProcessId(), pid);            // standing by
        QVERIFY(!processIsGone(pid));

        auto uplink = supervisor.acquire(CaptureSupervisor::Demand::RemoteWindow);
        QVERIFY(waitFor(supervisor, [opened](const Status& s) {
            return s.state == State::Ready && s.generation > opened;
        }, 5000));
        const quint32 reopened = supervisor.status().generation;
        QCOMPARE(supervisor.helperProcessId(), pid);

        AudioDeviceConfig other = supervisor.config();
        other.deviceName = QStringLiteral("Other fake microphone");
        supervisor.configure(other);
        QVERIFY(waitFor(supervisor, [reopened](const Status& s) {
            return s.state == State::Ready && s.generation > reopened
                && s.actualDevice == QLatin1String("Other fake microphone");
        }, 5000));
        const quint32 changed = supervisor.status().generation;
        QCOMPARE(supervisor.helperProcessId(), pid);

        supervisor.retry();
        QVERIFY(waitFor(supervisor, [changed](const Status& s) {
            return s.state == State::Ready && s.generation > changed;
        }, 5000));
        QCOMPARE(supervisor.helperProcessId(), pid);
        QCOMPARE(supervisor.lastHelperProcessId(), pid);        // no other helper ever started
        QCOMPARE(warmed.size(), 1);

        uplink.release();
        QVERIFY(waitForState(supervisor, State::Closed, 3000));
        QTest::qWait(300);
        QCOMPARE(supervisor.helperProcessId(), pid);

        standby.release();                                      // nothing holds it now
        QVERIFY(waitProcessGone(pid, 3000));
        QTRY_COMPARE_WITH_TIMEOUT(supervisor.helperProcessId(), qint64(0), 3000);
        QCOMPARE(supervisor.status().state, State::Closed);
    }

    // A standby helper that misses its open deadline is still killed, and
    // standby starts one fresh helper at once, warmed with no microphone
    // open.  The failure stays published (the generation is never retried
    // by itself); the retry opens on the fresh helper, not on another.
    void stuckStandbyHelperIsKilledAndReplaced()
    {
        HangMarkers markers;
        QVERIFY(markers.isValid());
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-open"));
        options.openTimeoutMs = 500;                            // 10000 in production
        CaptureSupervisor supervisor(options);
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        QTRY_VERIFY_WITH_TIMEOUT(warmed.size() >= 1, kHelperStartMs);
        const qint64 stuck = supervisor.helperProcessId();
        QVERIFY(stuck > 0);

        auto mic = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(markers.waitHanging(stuck, kHelperStartMs));    // the open went to the warm helper
        QVERIFY(waitForState(supervisor, State::Failed, 5000));
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        const quint32 failed = supervisor.status().generation;
        QVERIFY(waitProcessGone(stuck, 1000));                  // killed

        QTRY_VERIFY_WITH_TIMEOUT(warmed.size() >= 2, kHelperStartMs);
        const qint64 fresh = warmed.at(1).at(0).toLongLong();
        QVERIFY(fresh > 0);
        QVERIFY(fresh != stuck);
        QCOMPARE(supervisor.helperProcessId(), fresh);
        QCOMPARE(supervisor.status().state, State::Failed);     // never retried by itself
        QCOMPARE(supervisor.status().generation, failed);

        supervisor.retry();
        QVERIFY(markers.waitHanging(fresh, kHelperStartMs));    // the retry's open went to it
        QVERIFY(waitFor(supervisor, [failed](const Status& s) {
            return s.state == State::Failed && s.generation > failed;
        }, 5000));
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        QVERIFY(waitProcessGone(fresh, 1000));
    }

    // An open asked for while the standing helper is still in its warm-up
    // (its first audio call, which a macOS Mic Mode re-apply can hold for
    // 40 to 100 s) stays Opening.  Its open deadline starts only once the
    // helper has warmed, so the helper is not killed at it, and the open
    // then succeeds on that same helper.
    void openDuringWarmUpWaitsForTheHelper()
    {
        HangMarkers markers;
        QVERIFY(markers.isValid());
        WarmHold hold;
        QVERIFY(hold.isValid());
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("warm-while-marked"));
        options.openTimeoutMs = 500;                            // 10000 in production
        CaptureSupervisor supervisor(options);
        Recorder recorder(supervisor);
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        const qint64 pid = waitForPid(supervisor, kHelperStartMs);
        QVERIFY(pid > 0);
        QVERIFY(markers.waitHanging(pid, kHelperStartMs));      // in its warm-up

        auto mic = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Opening, 3000));
        QCOMPARE(supervisor.fireDeadlinesNowForTest(CaptureSupervisor::OpenDeadline), 0);
        QTest::qWait(options.openTimeoutMs * 2);                // the warm-up outlasts it
        QCOMPARE(supervisor.status().state, State::Opening);
        QVERIFY(!recorder.sawState(State::Failed));
        QCOMPARE(supervisor.helperProcessId(), pid);
        QCOMPARE(warmed.size(), 0);

        QVERIFY(hold.release());
        QVERIFY(waitFor(supervisor, [](const Status& s) {
            return s.state == State::Ready || s.state == State::Failed;
        }, 5000));
        QCOMPARE(supervisor.status().state, State::Ready);
        QCOMPARE(warmed.size(), 1);
        QCOMPARE(supervisor.helperProcessId(), pid);
        QCOMPARE(supervisor.lastHelperProcessId(), pid);        // no other helper started
        QVERIFY(!recorder.sawState(State::Failed));
    }

    // A standing helper stuck in its warm-up is killed at the warm deadline
    // (not the shorter open deadline); an open that waited on it fails as
    // Timeout.  One fresh helper stands by in its place; when that one
    // sticks too it is killed in turn, and no third helper is started.
    void stuckWarmUpIsKilledAtTheWarmDeadline()
    {
        HangMarkers markers;
        QVERIFY(markers.isValid());
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("hang-warm"));
        options.openTimeoutMs = 300;                            // 10000 in production
        options.warmTimeoutMs = 1500;                           // 120000 in production
        CaptureSupervisor supervisor(options);
        Recorder recorder(supervisor);
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        const qint64 stuck = waitForPid(supervisor, kHelperStartMs);
        QVERIFY(stuck > 0);
        QVERIFY(markers.waitHanging(stuck, kHelperStartMs));

        auto mic = supervisor.acquire(CaptureSupervisor::Demand::LocalSession);
        QVERIFY(waitForState(supervisor, State::Opening, 3000));
        QTest::qWait(options.openTimeoutMs * 2);                // past the open deadline
        QCOMPARE(supervisor.status().state, State::Opening);
        QCOMPARE(supervisor.helperProcessId(), stuck);
        QVERIFY(waitForState(supervisor, State::Failed, options.warmTimeoutMs + 3000));
        QCOMPARE(supervisor.status().reason, Reason::Timeout);
        const quint32 failed = supervisor.status().generation;
        QVERIFY(waitProcessGone(stuck, 1000));                  // killed

        QElapsedTimer timer;
        timer.start();
        qint64 fresh = 0;
        while (timer.elapsed() < kHelperStartMs) {
            fresh = supervisor.helperProcessId();
            if (fresh > 0 && fresh != stuck) {
                break;
            }
            QTest::qWait(5);
        }
        QVERIFY(fresh > 0);
        QVERIFY(fresh != stuck);
        QVERIFY(markers.waitHanging(fresh, kHelperStartMs));    // stuck in its warm-up too
        QCOMPARE(supervisor.status().state, State::Failed);     // never retried by itself
        QCOMPARE(supervisor.status().generation, failed);
        QVERIFY(waitProcessGone(fresh, options.warmTimeoutMs + 3000));

        QTest::qWait(options.warmTimeoutMs);
        QCOMPARE(supervisor.helperProcessId(), qint64(0));      // no third helper
        QCOMPARE(supervisor.lastHelperProcessId(), fresh);
        QCOMPARE(warmed.size(), 0);
        QCOMPARE(supervisor.status().state, State::Failed);
    }

    // A standing helper that never says hello is killed at the hello
    // deadline and replaced once; when the replacement does the same,
    // standby gives up: no further helper is started.
    void standbyHelperThatNeverStartsIsReplacedOnce()
    {
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("no-hello"));
        options.helloTimeoutMs = 400;                           // 3000 in production
        CaptureSupervisor supervisor(options);
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        const qint64 first = waitForPid(supervisor, kHelperStartMs);
        QVERIFY(first > 0);
        QVERIFY(waitProcessGone(first, 3000));

        QElapsedTimer timer;
        timer.start();
        while (supervisor.lastHelperProcessId() == first && timer.elapsed() < kHelperStartMs) {
            QTest::qWait(5);
        }
        const qint64 second = supervisor.lastHelperProcessId();
        QVERIFY(second > 0);
        QVERIFY(second != first);
        QVERIFY(waitProcessGone(second, 3000));

        QTest::qWait(options.helloTimeoutMs * 3);               // room for a third start
        QCOMPARE(supervisor.helperProcessId(), qint64(0));
        QCOMPARE(supervisor.lastHelperProcessId(), second);
        QCOMPARE(supervisor.fireDeadlinesNowForTest(CaptureSupervisor::HelloDeadline), 0);
        QCOMPARE(warmed.size(), 0);
        QCOMPARE(supervisor.status().state, State::Closed);     // no microphone was asked for
    }

    // A standing helper that warms and then ends is replaced, and each
    // replacement that warms earns another, but no more than three within a
    // minute: the fourth helper to end is not replaced.
    void standbyHelperThatWarmsAndEndsIsReplacedThreeTimesAMinute()
    {
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("exit-after-warm")));
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        QTRY_VERIFY_WITH_TIMEOUT(warmed.size() >= 4, 2 * kHelperStartMs);
        const qint64 fourth = warmed.at(3).at(0).toLongLong();
        QVERIFY(fourth > 0);
        QVERIFY(waitProcessGone(fourth, 3000));

        QTest::qWait(2000);                                     // room for a fifth start
        QCOMPARE(warmed.size(), 4);
        QCOMPARE(supervisor.helperProcessId(), qint64(0));
        QCOMPARE(supervisor.lastHelperProcessId(), fourth);
        QCOMPARE(supervisor.status().state, State::Closed);     // no microphone was asked for
    }

    // ── The window's Mic Mode (2026-10-16) ──────────────────────────────────

    // Warm names the window's bundle until a helper has answered for it:
    // the first helper keeps the Mic Mode at Standard, and its replacements
    // are not asked again.  The window logs the answer once.  (The fake
    // answers a Warm that names a bundle as Voice Isolation made Standard,
    // and one that names none with no Mic Mode answer.)
    void standbyKeepsTheWindowsMicModeOnce()
    {
        WarningWatch line(QStringLiteral("Mic Mode: set to Standard for this app; it was "
                                         "Voice Isolation"),
                          QtInfoMsg);
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("exit-after-warm"));
        options.micModeBundle = QStringLiteral("com.example.window");
        CaptureSupervisor supervisor(options);
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        QTRY_VERIFY_WITH_TIMEOUT(warmed.size() >= 4, 2 * kHelperStartMs);
        QCOMPARE(line.count(), 1);
    }

    // A helper that ends in its warm-up has not answered for the Mic Mode,
    // so the next helper's Warm names the bundle again.
    void aHelperEndedInItsWarmUpLeavesTheMicModeToTheNext()
    {
        HangMarkers markers;
        QVERIFY(markers.isValid());
        WarmHold hold;
        QVERIFY(hold.isValid());
        WarningWatch line(QStringLiteral("Mic Mode: set to Standard for this app"), QtInfoMsg);
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("warm-while-marked"));
        options.micModeBundle = QStringLiteral("com.example.window");
        options.warmTimeoutMs = 1500;                           // 120000 in production
        CaptureSupervisor supervisor(options);
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        const qint64 stuck = waitForPid(supervisor, kHelperStartMs);
        QVERIFY(stuck > 0);
        QVERIFY(markers.waitHanging(stuck, kHelperStartMs));
        QVERIFY(waitProcessGone(stuck, options.warmTimeoutMs + 3000));  // killed
        QVERIFY(hold.release());                                // the next one answers
        QTRY_COMPARE_WITH_TIMEOUT(warmed.size(), 1, 2 * kHelperStartMs);
        QVERIFY(warmed.at(0).at(0).toLongLong() != stuck);
        QCOMPARE(line.count(), 1);
    }

    // With no bundle (nereusd, a bare binary), Warm asks nothing and the
    // window logs no Mic Mode line.  (A Mic Mode answer to it would be a
    // protocol error, and the helper would never be reported warmed.)
    void standbyWithoutABundleAsksNoMicMode()
    {
        WarningWatch line(QStringLiteral("Mic Mode:"), QtInfoMsg);
        CaptureSupervisor supervisor(fakeOptions(QStringLiteral("ready")));
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        QTRY_COMPARE_WITH_TIMEOUT(warmed.size(), 1, kHelperStartMs);
        QCOMPARE(line.count(), 0);
    }

    // A bundle id that is not one is never sent: the window warns once and
    // its helpers warm with no Mic Mode asked.  (Sent, it would make no
    // Warm record at all, and no helper would warm.)
    void standbyWithAnInvalidBundleIdAsksNoMicMode()
    {
        WarningWatch warning(QStringLiteral("capture: \"not a bundle id\" is no bundle id; "
                                            "Mic Mode left as it is"));
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("exit-after-warm"));
        options.micModeBundle = QStringLiteral("not a bundle id");
        CaptureSupervisor supervisor(options);
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        QTRY_VERIFY_WITH_TIMEOUT(warmed.size() >= 2, 2 * kHelperStartMs);
        QCOMPARE(warning.count(), 1);
    }

    // A Warmed out of step with its Warm (no Mic Mode answer when one was
    // asked, or one nobody asked for) is a protocol error: the helper is
    // killed and never reported warmed.
    void warmedMicModeOutOfStepIsAProtocolError_data()
    {
        QTest::addColumn<QString>("bundle");
        QTest::addColumn<QString>("detail");
        QTest::newRow("asked, not answered")
            << QStringLiteral("com.example.window")
            << QStringLiteral("a warm-up answer with no Mic Mode answer");
        QTest::newRow("answered, not asked")
            << QString() << QStringLiteral("a Mic Mode answer nobody asked for");
    }

    void warmedMicModeOutOfStepIsAProtocolError()
    {
        QFETCH(QString, bundle);
        QFETCH(QString, detail);
        WarningWatch error(QStringLiteral("capture: protocol error from helper: ") + detail);
        CaptureSupervisor::Options options = fakeOptions(QStringLiteral("warm-wrong-mic-mode"));
        options.micModeBundle = bundle;
        CaptureSupervisor supervisor(options);
        QSignalSpy warmed(&supervisor, &CaptureSupervisor::helperWarmed);
        auto standby = supervisor.acquire(CaptureSupervisor::Demand::Standby);
        const qint64 first = waitForStartedPid(supervisor, kHelperStartMs);
        QVERIFY(first > 0);
        QTRY_VERIFY_WITH_TIMEOUT(error.count() >= 1, kHelperStartMs);
        QVERIFY(waitProcessGone(first, 3000));
        QCOMPARE(warmed.size(), 0);
    }

    // ── CaptureStandby (2026-10-16) ─────────────────────────────────────────

    // Work handed to whenWarm() waits while the helper is in its warm-up
    // and runs from the event loop once it has warmed; later work runs at
    // once, but never inside the call.  The helper keeps running.
    void standbyHoldsWorkUntilTheHelperHasWarmed()
    {
        HangMarkers markers;
        QVERIFY(markers.isValid());
        WarmHold hold;
        QVERIFY(hold.isValid());
        CaptureStandby standby(fakeOptions(QStringLiteral("warm-while-marked")), 60000);
        QObject context;
        int ran = 0;
        standby.whenWarm(&context, [&ran]() { ++ran; });
        const qint64 pid = waitForPid(*standby.supervisor(), kHelperStartMs);
        QVERIFY(pid > 0);
        QVERIFY(markers.waitHanging(pid, kHelperStartMs));
        QTest::qWait(200);
        QCOMPARE(ran, 0);
        QVERIFY(!standby.warmed());

        QVERIFY(hold.release());
        QTRY_COMPARE_WITH_TIMEOUT(ran, 1, kHelperStartMs);
        QVERIFY(standby.warmed());

        standby.whenWarm(&context, [&ran]() { ++ran; });
        QCOMPARE(ran, 1);                                       // not inside the call
        QTRY_COMPARE_WITH_TIMEOUT(ran, 2, 1000);
        QCOMPARE(standby.supervisor()->helperProcessId(), pid);
        QVERIFY(!processIsGone(pid));
    }

    // The bound only stops the holding: the work runs once it has passed,
    // and the helper, still in its warm-up, is not killed for it.  Ending
    // the standby still ends that helper within its bounded shutdown.
    void standbyReleasesWorkAtItsBound()
    {
        HangMarkers markers;
        QVERIFY(markers.isValid());
        constexpr int kBoundMs = 300;
        auto standby = std::make_unique<CaptureStandby>(fakeOptions(QStringLiteral("hang-warm")),
                                                        kBoundMs);
        QObject context;
        int ran = 0;
        QElapsedTimer timer;
        timer.start();
        standby->whenWarm(&context, [&ran]() { ++ran; });
        QTRY_COMPARE_WITH_TIMEOUT(ran, 1, kHelperStartMs);
        QVERIFY2(timer.elapsed() >= kBoundMs, qPrintable(QString::number(timer.elapsed())));
        QVERIFY(!standby->warmed());
        const qint64 pid = waitForPid(*standby->supervisor(), kHelperStartMs);
        QVERIFY(pid > 0);
        QVERIFY(markers.waitHanging(pid, kHelperStartMs));
        QCOMPARE(standby->supervisor()->helperProcessId(), pid);

        timer.restart();
        standby.reset();
        QVERIFY2(timer.elapsed() < 2000 + 500, qPrintable(QString::number(timer.elapsed())));
        QVERIFY2(waitProcessGone(pid, 500), "helper outlived its standby");
    }

    // No helper program: nothing is started and nothing is held.
    void standbyWithoutAHelperHoldsNothing()
    {
        CaptureSupervisor::Options options;
        options.program = QDir(QCoreApplication::applicationDirPath())
                              .filePath(QStringLiteral("no-such-capture-helper"));
        CaptureStandby standby(options, 60000);
        QObject context;
        int ran = 0;
        standby.whenWarm(&context, [&ran]() { ++ran; });
        QCOMPARE(ran, 0);                                       // not inside the call
        QTRY_COMPARE_WITH_TIMEOUT(ran, 1, 1000);
        QVERIFY(!standby.warmed());
        QCOMPARE(standby.supervisor()->lastHelperProcessId(), qint64(0));
    }

    // Work whose context is destroyed while it is held never runs.
    void standbyDropsWorkWhoseContextIsGone()
    {
        HangMarkers markers;
        QVERIFY(markers.isValid());
        WarmHold hold;
        QVERIFY(hold.isValid());
        CaptureStandby standby(fakeOptions(QStringLiteral("warm-while-marked")), 60000);
        auto context = std::make_unique<QObject>();
        int ran = 0;
        standby.whenWarm(context.get(), [&ran]() { ++ran; });
        const qint64 pid = waitForPid(*standby.supervisor(), kHelperStartMs);
        QVERIFY(pid > 0);
        QVERIFY(markers.waitHanging(pid, kHelperStartMs));
        context.reset();
        QVERIFY(hold.release());
        QTRY_VERIFY_WITH_TIMEOUT(standby.warmed(), kHelperStartMs);
        QTest::qWait(100);
        QCOMPARE(ran, 0);
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
