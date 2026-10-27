// =================================================================
// tests/tst_wdsp_drain_timeout_flush.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test, no Thetis counterpart.
//
// Modification history (NereusSDR):
//   2026-10-27 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code: a flush that
//                 WDSP's drain timed out waiting for never lands after the
//                 channel is switched on again.
// =================================================================
//
// SetChannelState(ch, 0, 1) raises the down-slew and waits up to 100 ms for
// the channel's flush thread to report the flush done. The last fexchange of
// the down-slew clears the exchange bit and releases the flush thread, which
// takes csDSP and csEXCH, flushes the buffers, sets exec_bypass and only then
// clears flushflag. Upstream, a drain that times out returns with that flush
// still pending; if it lands after the next SetChannelState(ch, 1, 0), the
// running channel keeps exec_bypass set, its worker skips every dexchange,
// and a blocking (bfo) fexchange0 caller waits for output forever. That is
// how tst_hl2_tune_magnitude_replay hung on a loaded CI runner: its fake
// radio feeds the TX worker slowly enough that every unkey drain reaches the
// timeout.
//
// This test makes the timing certain instead of likely. The channel's worker
// is held inside dexchange (dsplock.c's test-only exchange hook, csDSP held)
// once the down-slew has been asked for, so the down-slew finishes on output
// the worker already released while the flush thread waits for csDSP. Then
// the channel is switched on again as soon as the drain returns, and the
// worker is let go. The channel must come back running, not bypassed.

#include <QtTest>

#include "core/wdsp_api.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

extern "C" int nereus_channel_exchange(int channel);
extern "C" int nereus_channel_downflag(int channel);
extern "C" int nereus_channel_exec_bypass(int channel);

namespace {

using Clock = std::chrono::steady_clock;

// A TX channel at the Core's sizes (WdspEngine::createTxChannel): 64-frame
// input at 48 kHz, 2048-sample DSP buffer at 96 kHz, 192 kHz output. One
// dexchange releases 16 output blocks; the 10 ms down-slew at 192 kHz plus
// its zero block ends within 9 fexchange0 calls, so it finishes on output
// the held worker already released.
constexpr int kChannel = 0;
constexpr int kInSize = 64;
constexpr int kDspSize = 2048;
constexpr int kInRate = 48000;
constexpr int kDspRate = 96000;
constexpr int kOutRate = 192000;
constexpr int kOutSize = kInSize * kOutRate / kInRate;  // 256
// The held worker lets go once the channel is on again, or, when the drain
// is still waiting (it does once fixed: it waits for the flush the held
// worker blocks), after this long: far past the drain's own 100 ms timeout.
constexpr auto kHoldValve = std::chrono::milliseconds(1000);
// How long to look for a late flush after the worker lets go.
constexpr auto kLateFlushWatch = std::chrono::milliseconds(1000);
constexpr auto kProgressLimit = std::chrono::seconds(20);

std::atomic<int> g_exchanges{0};
std::atomic<bool> g_holdArmed{false};
std::atomic<bool> g_holding{false};
std::atomic<bool> g_held{false};
std::atomic<bool> g_keyOnDone{false};
std::atomic<bool> g_drainReturned{false};
std::atomic<bool> g_slewEndedInHold{false};
std::atomic<bool> g_pumpOn{false};
std::atomic<bool> g_pumpIdle{false};
std::atomic<bool> g_pumpStop{false};

// On the channel worker, right after dexchange releases Sem_OutReady, with
// csDSP held.
void onExchange(int channel)
{
    if (channel != kChannel) {
        return;
    }
    g_exchanges.fetch_add(1);
    bool armed = true;
    if (!g_holdArmed.compare_exchange_strong(armed, false)) {
        return;
    }
    g_holding.store(true);
    g_held.store(true);
    const auto valve = Clock::now() + kHoldValve;
    while (!g_keyOnDone.load() && Clock::now() < valve) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    g_holding.store(false);
}

void pump()
{
    std::vector<double> in(static_cast<size_t>(kInSize) * 2, 0.0);
    std::vector<double> out(static_cast<size_t>(kOutSize) * 2, 0.0);
    for (int i = 0; i < kInSize; ++i) {
        in[static_cast<size_t>(2 * i)] = 0.25;
    }
    while (!g_pumpStop.load()) {
        if (!g_pumpOn.load()) {
            g_pumpIdle.store(true);
            std::this_thread::yield();
            continue;
        }
        g_pumpIdle.store(false);
        int error = 0;
        fexchange0(kChannel, in.data(), out.data(), &error);
        if (nereus_channel_exchange(kChannel) == 0) {
            if (g_holding.load() && !g_drainReturned.load()) {
                g_slewEndedInHold.store(true);
            }
            std::this_thread::yield();
        }
    }
    g_pumpIdle.store(true);
}

template <typename Pred>
bool waitFor(Pred pred, std::chrono::milliseconds limit)
{
    const auto end = Clock::now() + limit;
    while (!pred()) {
        if (Clock::now() >= end) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

} // namespace

class TestWdspDrainTimeoutFlush : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { WDSPSetTestExchangeHook(onExchange); }
    void cleanupTestCase() { WDSPSetTestExchangeHook(nullptr); }

    void timedOutDrainsFlushNeverLandsOnARunningChannel()
    {
        OpenChannel(kChannel, kInSize, kDspSize, kInRate, kDspRate, kOutRate,
                    1,       // type: TX
                    0,       // state: off until started
                    0.000, 0.010, 0.000, 0.010,   // WdspEngine's TX slews
                    1);      // bfo: fexchange0 waits for output, as in the Core
        SetChannelState(kChannel, 1, 0);

        g_pumpOn.store(true);
        std::thread pumpThread(pump);
        const bool warmed = waitFor([] { return g_exchanges.load() >= 4; },
                                    std::chrono::milliseconds(20000));

        // Unkey, then key again as soon as the drain returns, with the pump
        // quiet in between, as the TX lane does on a quick re-key.
        std::thread keyThread([] {
            SetChannelState(kChannel, 0, 1);
            g_drainReturned.store(true);
            g_pumpOn.store(false);
            while (!g_pumpIdle.load()) {
                std::this_thread::yield();
            }
            SetChannelState(kChannel, 1, 0);
            g_keyOnDone.store(true);
        });
        // Hold the worker in its next dexchange once the down-slew is asked
        // for, so the flush thread will find csDSP taken.
        const bool slewAsked = waitFor([] { return nereus_channel_downflag(kChannel) == 1; },
                                       std::chrono::milliseconds(20000));
        g_holdArmed.store(true);
        keyThread.join();
        const bool held = waitFor([] { return g_held.load() && !g_holding.load(); },
                                  std::chrono::milliseconds(20000));
        // A late flush runs as soon as the worker leaves csDSP.
        waitFor([] { return nereus_channel_exec_bypass(kChannel) == 1; }, kLateFlushWatch);
        const int bypassAfterKeyOn = nereus_channel_exec_bypass(kChannel);
        const int exchangeAfterKeyOn = nereus_channel_exchange(kChannel);

        // The channel processes again (only pumped when it is not bypassed:
        // a bypassed bfo channel would block the pump for good).
        bool progressed = false;
        if (bypassAfterKeyOn == 0) {
            const int before = g_exchanges.load();
            g_pumpOn.store(true);
            progressed = waitFor([before] { return g_exchanges.load() >= before + 3; },
                                 std::chrono::milliseconds(kProgressLimit));
            g_pumpOn.store(false);
            waitFor([] { return g_pumpIdle.load(); }, std::chrono::milliseconds(20000));
        }
        g_pumpStop.store(true);
        pumpThread.join();
        SetChannelState(kChannel, 0, 0);
        CloseChannel(kChannel);

        QVERIFY2(warmed, "the channel's worker never exchanged a chunk");
        QVERIFY2(slewAsked, "the drain never raised the down-slew");
        QVERIFY2(held, "the worker was never held in dexchange");
        QVERIFY2(g_slewEndedInHold.load(),
                 "the down-slew did not finish while the worker was held");
        QCOMPARE(exchangeAfterKeyOn, 1);
        QCOMPARE(bypassAfterKeyOn, 0);
        QVERIFY2(progressed, "the channel did not process again after the re-key");
    }
};

QTEST_GUILESS_MAIN(TestWdspDrainTimeoutFlush)
#include "tst_wdsp_drain_timeout_flush.moc"
