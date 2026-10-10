// =================================================================
// tests/tst_mic_uplink_collector.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// The remote window's microphone collector (bench, 2026-10-10: a window
// whose GUI thread was late lost microphone audio on every key).  The
// paced microphone here keeps the pacing of the PC microphone's reader
// (CaptureAudioBus::pull): a pull gives at most the clock's frames since
// the last pull plus kPaceSlackFrames, with unused credit capped at
// kPaceCreditCapFrames.
//
// Modification history (NereusSDR):
//   2026-10-10: created.  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================
#include <QtTest/QtTest>

#include "core/audio/CaptureAudioBus.h"
#include "core/audio/MicUplinkCollector.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

using namespace NereusSDR;

namespace {

// A microphone paced as CaptureAudioBus::pull() is, on a clock the test
// moves.  Frame n carries the value n, so order and loss both show.
class PacedMic {
public:
    std::int64_t nowFrames = 0;      // the clock, in 48 kHz frames

    void advanceMs(int ms) { nowFrames += std::int64_t(ms) * 48; }

    int pull(float* dst, int maxFrames)
    {
        if (!m_started) {
            m_started = true;
            m_lastFrames = nowFrames;
            m_credit = 0;
        }
        m_credit = std::min<std::int64_t>(m_credit + (nowFrames - m_lastFrames),
                                          CaptureAudioBus::kPaceCreditCapFrames);
        m_lastFrames = nowFrames;
        const std::int64_t allowed = m_credit + CaptureAudioBus::kPaceSlackFrames - m_early;
        const int give = int(std::clamp<std::int64_t>(allowed, 0, maxFrames));
        for (int i = 0; i < give; ++i) {
            dst[i] = float(m_next++);
        }
        // Frames given beyond the credit were given early (the slack).
        const std::int64_t fromCredit = std::min<std::int64_t>(give, m_credit);
        m_credit -= fromCredit;
        m_early += give - fromCredit;
        // The clock pays the early frames back first.
        const std::int64_t repay = std::min(m_early, m_credit);
        m_early -= repay;
        m_credit -= repay;
        return give;
    }

    std::int64_t given() const { return m_next; }

private:
    bool m_started = false;
    std::int64_t m_lastFrames = 0;
    std::int64_t m_credit = 0;
    std::int64_t m_early = 0;
    std::int64_t m_next = 0;
};

std::vector<float> drainAll(MicUplinkCollector& collector)
{
    std::vector<float> out;
    std::vector<float> block(960);
    for (;;) {
        const int got = collector.drain(block.data(), int(block.size()));
        if (got <= 0) {
            break;
        }
        out.insert(out.end(), block.begin(), block.begin() + got);
    }
    return out;
}

bool isRamp(const std::vector<float>& frames, std::int64_t first)
{
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (frames[i] != float(first + std::int64_t(i))) {
            return false;
        }
    }
    return true;
}

} // namespace

class TestMicUplinkCollector : public QObject {
    Q_OBJECT

private slots:
    // The defect: nobody pulls for 200 ms, then the consumer itself pulls
    // until the microphone gives nothing.  The pacing gives it 30 ms; the
    // other 170 ms is gone.
    void aLateConsumerPullingTheMicrophoneItselfLosesAudio()
    {
        PacedMic mic;
        std::vector<float> block(960);
        QCOMPARE(mic.pull(block.data(), 960), CaptureAudioBus::kPaceSlackFrames);
        const std::int64_t before = mic.given();
        mic.advanceMs(200);
        std::int64_t got = 0;
        for (;;) {
            const int n = mic.pull(block.data(), 960);
            if (n <= 0) {
                break;
            }
            got += n;
            if (n < 960) {
                break;
            }
        }
        QCOMPARE(mic.given() - before, got);
        QVERIFY2(got <= CaptureAudioBus::kPaceCreditCapFrames + CaptureAudioBus::kPaceSlackFrames,
                 qPrintable(QString::number(got)));
        QVERIFY2(got < 200 * 48, qPrintable(QString::number(got)));
    }

    // The fix: the collector steps every 5 ms of that clock while the
    // consumer is away for 200 ms; the consumer then receives every frame
    // of the 200 ms, in order.
    void aLateConsumerReceivesEveryFrameTheCollectorPulled()
    {
        PacedMic mic;
        MicUplinkCollector collector([&mic](float* dst, int n) { return mic.pull(dst, n); });
        collector.collectOnce();                       // the reader's first pull
        const std::int64_t first = mic.given();
        (void)drainAll(collector);
        for (int ms = 0; ms < 200; ms += MicUplinkCollector::kWakeIntervalMs) {
            mic.advanceMs(MicUplinkCollector::kWakeIntervalMs);
            collector.collectOnce();
        }
        QCOMPARE(collector.queuedFrames(), 200 * 48);
        const std::vector<float> heard = drainAll(collector);
        QCOMPARE(int(heard.size()), 200 * 48);
        QVERIFY(isRamp(heard, first));
        QCOMPARE(collector.droppedFrames(), quint64(0));
    }

    void nothingIsPulledUnlessRunning()
    {
        std::atomic<int> pulls{0};
        MicUplinkCollector collector([&pulls](float*, int) { ++pulls; return 0; }, 1);
        QVERIFY(!collector.isRunning());
        QTest::qWait(30);
        QCOMPARE(pulls.load(), 0);
        collector.start();
        QVERIFY(collector.isRunning());
        QTRY_VERIFY(pulls.load() > 0);
        collector.stop();
        QVERIFY(!collector.isRunning());
        const int after = pulls.load();
        QTest::qWait(30);
        QCOMPARE(pulls.load(), after);
    }

    // The thread hands over every frame, in order, to a consumer on
    // another thread that takes them late and in odd sizes.
    void theThreadHandsOverEveryFrameInOrder()
    {
        constexpr int kTotal = 48'000;                 // within the queue: none dropped
        std::atomic<int> given{0};
        MicUplinkCollector collector([&given](float* dst, int n) {
            const int at = given.load(std::memory_order_relaxed);
            const int give = std::min({n, 240, kTotal - at});
            for (int i = 0; i < give; ++i) {
                dst[i] = float(at + i);
            }
            given.store(at + give, std::memory_order_release);
            return give;
        }, 1);
        collector.start();
        std::vector<float> heard;
        std::vector<float> block(777);
        QElapsedTimer waited;
        waited.start();
        while (int(heard.size()) < kTotal && waited.elapsed() < 30'000) {
            const int got = collector.drain(block.data(), int(block.size()));
            if (got > 0) {
                heard.insert(heard.end(), block.begin(), block.begin() + got);
            } else {
                QTest::qSleep(1);
            }
        }
        collector.stop();
        QCOMPARE(int(heard.size()), kTotal);
        QVERIFY(isRamp(heard, 0));
        QCOMPARE(collector.droppedFrames(), quint64(0));
    }

    // stop() wakes the thread: it does not wait out the wake interval.
    void stopJoinsWithoutWaitingOutTheWake()
    {
        std::atomic<int> pulls{0};
        MicUplinkCollector collector([&pulls](float*, int) { ++pulls; return 0; },
                                     /*wakeIntervalMs=*/60'000);
        collector.start();
        QTRY_VERIFY(pulls.load() > 0);                 // the thread is in its wait
        QElapsedTimer timer;
        timer.start();
        collector.stop();
        QVERIFY(!collector.isRunning());
        QVERIFY2(timer.elapsed() < 30'000, qPrintable(QString::number(timer.elapsed())));
        QCOMPARE(pulls.load(), 1);
    }

    // A second run starts clean, and stop discards what was queued.
    void stopDiscardsWhatWasQueued()
    {
        int next = 0;
        MicUplinkCollector collector([&next](float* dst, int) {
            dst[0] = float(next++);
            return 1;
        });
        collector.collectOnce();
        collector.collectOnce();
        QCOMPARE(collector.queuedFrames(), 2);
        collector.stop();
        QCOMPARE(collector.queuedFrames(), 0);
        float out = -1.0f;
        QCOMPARE(collector.drain(&out, 1), 0);
    }

    // A full queue drops its oldest frames, counts them, and still hands
    // over the newest in order.
    void aFullQueueDropsTheOldestAndCountsIt()
    {
        std::int64_t next = 0;
        MicUplinkCollector collector([&next](float* dst, int n) {
            const int give = std::min(n, 480);
            for (int i = 0; i < give; ++i) {
                dst[i] = float(next++);
            }
            return give;
        });
        const int kOver = 4800;
        while (next < MicUplinkCollector::kQueueFrames + kOver) {
            collector.collectOnce();
        }
        QCOMPARE(collector.queuedFrames(), int(MicUplinkCollector::kQueueFrames));
        QCOMPARE(collector.droppedFrames(), quint64(next - MicUplinkCollector::kQueueFrames));
        const std::vector<float> heard = drainAll(collector);
        QCOMPARE(int(heard.size()), int(MicUplinkCollector::kQueueFrames));
        QVERIFY(isRamp(heard, next - MicUplinkCollector::kQueueFrames));
    }

    // The producer dropping the oldest while the consumer drains: what the
    // consumer receives is always in order (gaps only where frames were
    // dropped), and every frame is either received or counted as dropped.
    void droppingWhileDrainingKeepsOrderAndCountsEveryFrame()
    {
        constexpr std::int64_t kTotal = 4'000'000;
        std::atomic<std::int64_t> given{0};
        // Set by the pull after the last frame was queued.
        std::atomic<bool> allQueued{false};
        MicUplinkCollector collector([&given, &allQueued](float* dst, int n) {
            const std::int64_t at = given.load(std::memory_order_relaxed);
            const int give = int(std::min<std::int64_t>(n, kTotal - at));
            if (give == 0) {
                allQueued.store(true, std::memory_order_release);
            }
            for (int i = 0; i < give; ++i) {
                // Exact in a float: the frame's place in a 2^20 cycle.
                dst[i] = float((at + i) & 0xFFFFF);
            }
            given.store(at + give, std::memory_order_release);
            return give;
        }, 1);
        collector.start();
        std::vector<float> block(960);
        std::int64_t received = 0;
        std::int64_t expected = -1;
        bool ordered = true;
        int drains = 0;
        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < 60'000) {
            // A consumer that is away now and then, so the queue fills.
            if (++drains % 64 == 0) {
                QTest::qSleep(3);
            }
            const bool done = allQueued.load(std::memory_order_acquire);
            const int got = collector.drain(block.data(), int(block.size()));
            if (got <= 0) {
                if (done) {
                    break;
                }
                continue;
            }
            for (int i = 0; i < got; ++i) {
                const std::int64_t value = std::int64_t(block[std::size_t(i)]);
                // Within one drain the frames are consecutive; between
                // drains they may jump forward (a drop), never back.
                if (i > 0 && value != ((expected) & 0xFFFFF)) {
                    ordered = false;
                }
                expected = value + 1;
            }
            received += got;
        }
        collector.stop();
        QVERIFY(ordered);
        QCOMPARE(given.load(), kTotal);
        QVERIFY(collector.droppedFrames() > 0);
        QCOMPARE(received + std::int64_t(collector.droppedFrames()), kTotal);
    }
};

QTEST_MAIN(TestMicUplinkCollector)
#include "tst_mic_uplink_collector.moc"
