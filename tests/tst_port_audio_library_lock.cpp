// =================================================================
// tests/tst_port_audio_library_lock.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  PortAudioLibrary's lock (R-AUD-06):
// the older drivers' Rescan starts PortAudio again on the catalogue's
// thread while this thread queries PortAudio's host APIs and devices, and
// every query sees one whole, unchanged list (never PortAudio half
// stopped).  Uses the real PortAudio library; no stream is opened.  Also:
// Rescan leaves PortAudio alone while one of its streams is open, and a
// driver call that never returns (a thread holding the lock) hangs
// neither a stream open, the query helpers, release() nor an
// AudioEngine's destruction.  The only open tried is of a microphone by a
// name no device has, strictly by that name, so it cannot reach a device.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 fix (R-AUD-06). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: final review fix (R-AUD-03, R-AUD-06): the bounded waits.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QRegularExpression>

#include "core/AudioEngine.h"
#include "core/audio/AudioDeviceCatalog.h"
#include "core/audio/PortAudioBackend.h"
#include "core/audio/PortAudioBus.h"
#include "core/audio/PortAudioLibrary.h"
#include "fakes/FakeAudioEngineBackend.h"

#include <QElapsedTimer>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr int kRescans = 20;

QStringList hostApiNames()
{
    QStringList names;
    for (const PortAudioBus::HostApiInfo& api : PortAudioBus::hostApis()) {
        names << QStringLiteral("%1:%2").arg(api.index).arg(api.name);
    }
    return names;
}

QStringList deviceLines()
{
    QStringList lines;
    for (const PortAudioDeviceRecord& r : listPortAudioDevices()) {
        lines << QStringLiteral("%1|%2|%3|%4").arg(r.hostApi, r.name).arg(r.outputChannels).arg(r.inputChannels);
    }
    return lines;
}

QList<int> outputCounts()
{
    QList<int> counts;
    for (const PortAudioBus::HostApiInfo& api : PortAudioBus::hostApis()) {
        counts << PortAudioBus::outputDevicesFor(api.index).size()
               << PortAudioBus::inputDevicesFor(api.index).size();
    }
    return counts;
}

// A driver call that does not return: holds PortAudio's lock (a LongHold,
// as Rescan and the listing take it) until finish(), or until a watchdog
// of three bounds so a test that hangs still ends.
class StuckDriverCall {
public:
    // Blocks the calling thread inside the hold.
    void hold()
    {
        const PortAudioLibrary::LongHold lock;
        std::unique_lock<std::mutex> guard(m_mutex);
        m_entered = true;
        m_cv.notify_all();
        m_cv.wait_for(guard, std::chrono::milliseconds(3 * PortAudioLibrary::kBoundedWaitMs),
                      [this] { return m_finish; });
    }
    bool waitEntered()
    {
        std::unique_lock<std::mutex> guard(m_mutex);
        return m_cv.wait_for(guard, std::chrono::seconds(5), [this] { return m_entered; });
    }
    void finish()
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_finish = true;
        m_cv.notify_all();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_entered = false;
    bool m_finish = false;
};

// A microphone open that can only fail: no device has this name and the
// name is matched strictly.
std::unique_ptr<PortAudioBus> unopenableMic()
{
    PortAudioConfig config;
    config.direction = AudioDirection::Input;
    config.deviceName = QStringLiteral("NereusSDR test device that does not exist");
    auto bus = std::make_unique<PortAudioBus>();
    bus->setConfig(config);
    bus->setStrictInputDevice(true);
    return bus;
}

} // namespace

class TstPortAudioLibraryLock : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        PortAudioBus::allowPortAudioLibraryForTest(true);
        if (!PortAudioLibrary::acquire()) {
            PortAudioBus::allowPortAudioLibraryForTest(false);
            QSKIP("PortAudio does not start on this computer");
        }
        QCOMPARE(PortAudioLibrary::references(), 1);
    }

    void cleanupTestCase()
    {
        PortAudioLibrary::release();
        PortAudioBus::allowPortAudioLibraryForTest(false);
    }

    void queriesStayWholeWhileRescanning()
    {
        const QStringList apis = hostApiNames();
        const QStringList devices = deviceLines();
        const QList<int> counts = outputCounts();
        QVERIFY(!apis.isEmpty());

        PortAudioBackend backend(listPortAudioDevices, currentOlderDriverPlatform(), true);
        std::atomic<bool> done{false};
        std::atomic<int> rescans{0};
        std::thread catalogue([&]() {
            for (int i = 0; i < kRescans; ++i) {
                backend.rescan();
                rescans.fetch_add(1);
            }
            done.store(true);
        });
        int queries = 0;
        int mismatches = 0;
        while (!done.load()) {
            if (hostApiNames() != apis || deviceLines() != devices || outputCounts() != counts) {
                ++mismatches;
            }
            ++queries;
        }
        catalogue.join();

        QCOMPARE(rescans.load(), kRescans);
        QVERIFY(queries > 0);
        QCOMPARE(mismatches, 0);
        QCOMPARE(PortAudioLibrary::references(), 1);   // Rescan keeps every reference
        QCOMPARE(hostApiNames(), apis);
    }

    void rescanWaitsForOpenStreams()
    {
        PortAudioLibrary::streamOpened();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("not listed again")));
        QVERIFY(!PortAudioLibrary::reinitialize());
        QCOMPARE(PortAudioLibrary::references(), 1);
        PortAudioLibrary::streamClosed();
        QCOMPARE(PortAudioLibrary::openStreams(), 0);
        QVERIFY(PortAudioLibrary::reinitialize());
        QVERIFY(!hostApiNames().isEmpty());
    }

    // A thread stuck inside a driver call with the lock held: an open gives
    // up within the bound, then everything else at once.  The queries
    // return the lists last read; release() leaves PortAudio running.
    void boundedCallersGiveUpOnAStuckDriver()
    {
        const QStringList apis = hostApiNames();
        const QList<int> counts = outputCounts();
        QVERIFY(!apis.isEmpty());
        // The open fails without a stuck driver too: it names no device.
        QVERIFY(!unopenableMic()->open(AudioFormat{}));

        StuckDriverCall stuck;
        std::thread driver([&stuck] { stuck.hold(); });
        // A failed check returns early: the driver thread is still let go
        // and joined, so the failure is reported rather than aborting.
        struct LetGo {
            StuckDriverCall& stuck;
            std::thread& driver;
            ~LetGo()
            {
                stuck.finish();
                if (driver.joinable()) {
                    driver.join();
                }
            }
        } letGo{stuck, driver};
        QVERIFY(stuck.waitEntered());

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("PortAudio is busy in a driver call that has not returned")));
        std::unique_ptr<PortAudioBus> mic = unopenableMic();
        QElapsedTimer clock;
        clock.start();
        QVERIFY(!mic->open(AudioFormat{}));
        qint64 elapsed = clock.elapsed();
        QVERIFY2(elapsed >= PortAudioLibrary::kBoundedWaitMs - 50, qPrintable(QString::number(elapsed)));
        QVERIFY2(elapsed < PortAudioLibrary::kBoundedWaitMs + 1000, qPrintable(QString::number(elapsed)));
        QVERIFY(mic->errorString().contains(QStringLiteral("busy")));

        // The holder has now held it past the bound: the rest give up at once.
        clock.restart();
        QVERIFY(!unopenableMic()->open(AudioFormat{}));
        QCOMPARE(hostApiNames(), apis);
        QCOMPARE(outputCounts(), counts);
        QVERIFY(!PortAudioLibrary::acquire());
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("PortAudio left running at shutdown")));
        PortAudioLibrary::release();
        elapsed = clock.elapsed();
        QVERIFY2(elapsed < 1000, qPrintable(QString::number(elapsed)));
        QCOMPARE(PortAudioLibrary::references(), 1);   // nothing was terminated

        stuck.finish();
        driver.join();
        QCOMPARE(hostApiNames(), apis);
    }

    // The older drivers' Rescan stuck inside PortAudio: destroying the
    // engine returns within the catalogue's stop wait, and leaves
    // PortAudio running for the call still inside it.
    void engineShutdownDoesNotWaitOnAStuckRescan()
    {
        auto native = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::CoreAudio);
        auto older = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
        // Shared with the hook: on a failed check the catalogue's thread
        // may still be inside it after this function returns.
        auto stuck = std::make_shared<StuckDriverCall>();
        older->setRescanHook([stuck] { stuck->hold(); });
        struct LetGo {
            std::shared_ptr<StuckDriverCall> stuck;
            ~LetGo() { stuck->finish(); }
        } letGo{stuck};

        auto engine = std::make_unique<AudioEngine>();
        QCOMPARE(PortAudioLibrary::references(), 2);   // the engine's own
        engine->setVaxOutputsAllowed(false);
        engine->setAudioBackendsForTest({native, older});
        engine->rescanOlderDrivers();
        QVERIFY(stuck->waitEntered());

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("Audio device list did not stop within")));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("PortAudio is busy in a driver call that has not returned")));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("PortAudio left running at shutdown")));
        QElapsedTimer clock;
        clock.start();
        engine.reset();
        const qint64 elapsed = clock.elapsed();
        QVERIFY2(elapsed < AudioDeviceCatalog::kStopWaitMs + 1000, qPrintable(QString::number(elapsed)));
        QCOMPARE(PortAudioLibrary::references(), 2);   // not terminated under the stuck call

        // The call returns: the catalogue's thread finishes on its own,
        // and the engine's reference is the one left behind.
        stuck->finish();
        QTRY_COMPARE_WITH_TIMEOUT(older.use_count(), long(1), 5000);
        QCOMPARE(older->rescanCount(), 1);
        older->setRescanHook({});
        PortAudioLibrary::release();
        QCOMPARE(PortAudioLibrary::references(), 1);
    }
};

QTEST_GUILESS_MAIN(TstPortAudioLibraryLock)
#include "tst_port_audio_library_lock.moc"
