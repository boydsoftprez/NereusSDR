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
// The test thread steps fexchange0 itself, so the order of events is set by
// counting calls, not by scheduling. The channel worker is held inside its
// first dexchange (dsplock.c's test-only exchange hook, with csDSP held)
// right after it releases 16 output blocks; 15 are left once the caller has
// made its 17th call. The unkey then raises the down-slew, and exactly the 9
// calls it needs end it on those blocks, so the flush thread is released
// while csDSP is taken. The channel is switched on again as soon as the
// drain returns, then the worker is let go. The channel must come back
// running, not bypassed.

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
// input at 48 kHz, 2048-sample DSP buffer at 96 kHz, 192 kHz output. A new
// channel starts with 16 output blocks (iobuffs.c create_iobuffs: one
// 4096-sample DSP output over 256-sample blocks); each dexchange takes 1024
// input frames (16 calls) and releases 16 more.
constexpr int kChannel = 0;
constexpr int kInSize = 64;
constexpr int kDspSize = 2048;
constexpr int kInRate = 48000;
constexpr int kDspRate = 96000;
constexpr int kOutRate = 192000;
constexpr int kOutSize = kInSize * kOutRate / kInRate;  // 256
constexpr int kCallsPerExchange = 16;
// The first dexchange runs on the 16th call; the 17th waits for its blocks.
constexpr int kCallsToHold = kCallsPerExchange + 1;
// The 10 ms down-slew at 192 kHz (iobuffs.c downslew0): one BEGIN sample,
// 1921 DOWNSLEW, 257 ZERO, then OFF to the end of that block: 2179 samples,
// so it clears downflag at the end of the 9th 256-sample block.
constexpr int kSlewCalls = 9;
// The held worker lets go once the channel is on again or, when the drain
// is still waiting (it does once fixed: it waits for the flush the held
// worker blocks), after this long: far past the drain's own 100 ms timeout.
constexpr auto kHoldValve = std::chrono::milliseconds(2000);
// How long to look for a late flush after the worker lets go.
constexpr auto kLateFlushWatch = std::chrono::milliseconds(1000);
constexpr auto kWaitLimit = std::chrono::milliseconds(20000);
constexpr int kAttempts = 3;

std::atomic<int> g_exchanges{0};
std::atomic<bool> g_holdArmed{false};
std::atomic<bool> g_holding{false};
std::atomic<bool> g_held{false};
std::atomic<bool> g_keyOnDone{false};
std::atomic<bool> g_drainReturned{false};

void idle()
{
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}

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

template <typename Pred>
bool waitFor(Pred pred, std::chrono::milliseconds limit)
{
    const auto end = Clock::now() + limit;
    while (!pred()) {
        if (Clock::now() >= end) {
            return false;
        }
        idle();
    }
    return true;
}

struct Attempt {
    bool held = false;
    bool slewAsked = false;
    // The down-slew ended on the held worker's blocks before the drain
    // returned. Only then does the attempt say anything about the fix.
    bool slewEndedInHold = false;
    int exchangeAfterKeyOn = -1;
    int bypassAfterKeyOn = -1;
    bool progressed = false;
};

Attempt runAttempt()
{
    g_exchanges.store(0);
    g_holding.store(false);
    g_held.store(false);
    g_keyOnDone.store(false);
    g_drainReturned.store(false);

    std::vector<double> in(static_cast<size_t>(kInSize) * 2, 0.0);
    std::vector<double> out(static_cast<size_t>(kOutSize) * 2, 0.0);
    for (int i = 0; i < kInSize; ++i) {
        in[static_cast<size_t>(2 * i)] = 0.25;
    }
    auto step = [&in, &out](int calls) {
        for (int c = 0; c < calls; ++c) {
            int error = 0;
            fexchange0(kChannel, in.data(), out.data(), &error);
        }
    };

    OpenChannel(kChannel, kInSize, kDspSize, kInRate, kDspRate, kOutRate,
                1,       // type: TX
                0,       // state: off until started
                0.000, 0.010, 0.000, 0.010,   // WdspEngine's TX slews
                1);      // bfo: fexchange0 waits for output, as in the Core
    SetChannelState(kChannel, 1, 0);

    Attempt a;
    g_holdArmed.store(true);
    step(kCallsToHold);
    a.held = waitFor([] { return g_held.load(); }, kWaitLimit);

    // Unkey, then key again as soon as the drain returns, as the TX lane
    // does on a quick re-key.
    std::thread keyThread([] {
        SetChannelState(kChannel, 0, 1);
        g_drainReturned.store(true);
        SetChannelState(kChannel, 1, 0);
        g_keyOnDone.store(true);
    });
    a.slewAsked = waitFor([] { return nereus_channel_downflag(kChannel) == 1; }, kWaitLimit);
    if (a.held && a.slewAsked) {
        step(kSlewCalls);
        a.slewEndedInHold = nereus_channel_exchange(kChannel) == 0
            && g_holding.load() && !g_drainReturned.load();
    }
    keyThread.join();
    waitFor([] { return !g_holding.load(); }, kWaitLimit);
    // A late flush runs as soon as the worker leaves csDSP.
    waitFor([] { return nereus_channel_exec_bypass(kChannel) == 1; }, kLateFlushWatch);
    a.bypassAfterKeyOn = nereus_channel_exec_bypass(kChannel);
    a.exchangeAfterKeyOn = nereus_channel_exchange(kChannel);

    // The channel processes again. Only stepped when it is not bypassed: a
    // bypassed bfo channel would block this thread for good. Three
    // exchanges' worth of calls: each of the first two later groups waits
    // for the exchange before it.
    if (a.bypassAfterKeyOn == 0 && a.exchangeAfterKeyOn == 1) {
        const int before = g_exchanges.load();
        step(3 * kCallsPerExchange);
        a.progressed = g_exchanges.load() >= before + 2;
    }
    SetChannelState(kChannel, 0, 0);
    CloseChannel(kChannel);
    return a;
}

} // namespace

class TestWdspDrainTimeoutFlush : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { WDSPSetTestExchangeHook(onExchange); }
    void cleanupTestCase() { WDSPSetTestExchangeHook(nullptr); }

    void timedOutDrainsFlushNeverLandsOnARunningChannel()
    {
        // An attempt in which the drain returned before the 9 calls ended
        // the down-slew (this thread stalled for 100 ms in between) proves
        // nothing either way and is run again.
        Attempt a;
        int attempts = 0;
        do {
            a = runAttempt();
            ++attempts;
            QVERIFY2(a.held, "the worker was never held in dexchange");
            QVERIFY2(a.slewAsked, "the drain never raised the down-slew");
        } while (!a.slewEndedInHold && attempts < kAttempts);

        QVERIFY2(a.slewEndedInHold,
                 "the down-slew never ended while the worker was held and the drain waited");
        QCOMPARE(a.exchangeAfterKeyOn, 1);
        QCOMPARE(a.bypassAfterKeyOn, 0);
        QVERIFY2(a.progressed, "the channel did not process again after the re-key");
    }
};

QTEST_GUILESS_MAIN(TestWdspDrainTimeoutFlush)
#include "tst_wdsp_drain_timeout_flush.moc"
