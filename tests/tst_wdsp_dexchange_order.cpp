// =================================================================
// tests/tst_wdsp_dexchange_order.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test, no Thetis counterpart.
//
// Modification history (NereusSDR):
//   2026-10-01 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code: a WDSP channel's
//                 output does not change when its worker is held right after
//                 dexchange releases Sem_OutReady while the caller runs ahead.
// =================================================================
//
// iobuffs.c's dexchange used to release Sem_OutReady before it copied the
// worker's input chunk out of r1. A worker preempted between the two let
// fexchange0's caller run a chunk ahead, and the caller's next input copy
// landed on the head of the chunk the worker had not read: a phase jump and a
// spike in the channel's output. This test holds the worker in dsplock.c's
// test-only exchange hook (called right after the release) until the caller
// stalls on Sem_OutReady, that is, after its input copy, and checks the
// output is bit-identical to a run without the hold.
//
// The guard depends on where dexchange calls the hook: it must sit between
// the Sem_OutReady release and the input copy whenever those are in the
// upstream order. A WDSP sync that restores the upstream order must keep the
// hook in that position, or this test will not catch the race.

#include <QtTest>

#include "core/wdsp_api.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// A TX channel at the Core's sizes: 64-frame input at 48 kHz, 2048-sample DSP
// buffer at 96 kHz, 192 kHz output (WdspEngine::createTxChannel). One worker
// chunk is 1024 input frames, 16 fexchange0 calls.
constexpr int kChannel = 0;
constexpr int kInSize = 64;
constexpr int kDspSize = 2048;
constexpr int kInRate = 48000;
constexpr int kDspRate = 96000;
constexpr int kOutRate = 192000;
constexpr int kOutSize = kInSize * kOutRate / kInRate;  // 256
constexpr int kCalls = 16 * 24;                          // 24 chunks
// The worker is held in the exchange hook of this dexchange call.
constexpr int kHeldExchange = 6;
// The caller counts as stalled once one fexchange0 call has run this long
// with the worker held: it is waiting for Sem_OutReady, after its input copy.
constexpr auto kStall = std::chrono::milliseconds(100);
constexpr auto kHoldLimit = std::chrono::seconds(5);

std::atomic<int> g_exchanges{0};
std::atomic<bool> g_holdArmed{false};
std::atomic<bool> g_held{false};
std::atomic<bool> g_sawStall{false};
std::atomic<bool> g_callerInCall{false};
std::atomic<int64_t> g_callStartNs{0};

int64_t nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               Clock::now().time_since_epoch()).count();
}

// On the channel worker, right after dexchange releases Sem_OutReady.
void onExchange(int channel)
{
    if (channel != kChannel) {
        return;
    }
    const int n = g_exchanges.fetch_add(1) + 1;
    if (!g_holdArmed.load() || n != kHeldExchange) {
        return;
    }
    g_held.store(true);
    const auto limit = Clock::now() + kHoldLimit;
    while (Clock::now() < limit) {
        if (g_callerInCall.load()
            && nowNs() - g_callStartNs.load()
                   >= std::chrono::duration_cast<std::chrono::nanoseconds>(kStall).count()) {
            g_sawStall.store(true);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    g_held.store(false);
}

// Deterministic broadband input, so any chunk differs from every other.
std::vector<double> makeInput()
{
    std::vector<double> in(static_cast<size_t>(kCalls) * kInSize * 2);
    uint32_t state = 0x12345678u;
    for (size_t i = 0; i < in.size(); i += 2) {
        state = state * 1664525u + 1013904223u;
        const double v = (static_cast<double>(state >> 8) / 16777216.0 - 0.5) * 0.2;
        in[i] = v;
        in[i + 1] = v;
    }
    return in;
}

struct Run {
    std::vector<double> out;
    int exchanges = 0;
};

Run runChannel(const std::vector<double>& in, bool hold)
{
    g_exchanges.store(0);
    g_held.store(false);
    g_sawStall.store(false);
    g_callerInCall.store(false);
    g_holdArmed.store(hold);

    OpenChannel(kChannel, kInSize, kDspSize, kInRate, kDspRate, kOutRate,
                1,       // type: TX
                0,       // state: off until started
                0.010, 0.025, 0.000, 0.010,
                1);      // bfo: fexchange0 waits for output, as in the Core
    SetChannelState(kChannel, 1, 0);

    Run run;
    run.out.assign(static_cast<size_t>(kCalls) * kOutSize * 2, 0.0);
    std::vector<double> block(static_cast<size_t>(kInSize) * 2);
    std::vector<double> outBlock(static_cast<size_t>(kOutSize) * 2);
    for (int call = 0; call < kCalls; ++call) {
        std::memcpy(block.data(), in.data() + static_cast<size_t>(call) * kInSize * 2,
                    block.size() * sizeof(double));
        int error = 0;
        g_callStartNs.store(nowNs());
        g_callerInCall.store(true);
        fexchange0(kChannel, block.data(), outBlock.data(), &error);
        g_callerInCall.store(false);
        std::memcpy(run.out.data() + static_cast<size_t>(call) * kOutSize * 2,
                    outBlock.data(), outBlock.size() * sizeof(double));
    }
    run.exchanges = g_exchanges.load();
    g_holdArmed.store(false);
    SetChannelState(kChannel, 0, 1);
    CloseChannel(kChannel);
    return run;
}

} // namespace

class TestWdspDexchangeOrder : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { WDSPSetTestExchangeHook(onExchange); }
    void cleanupTestCase() { WDSPSetTestExchangeHook(nullptr); }

    void outputUnchangedWhenWorkerHeldAfterRelease()
    {
        const std::vector<double> in = makeInput();
        const Run reference = runChannel(in, false);
        const Run held = runChannel(in, true);

        QVERIFY2(reference.exchanges > kHeldExchange + 4, "the reference run's worker exchanged too few chunks");
        QVERIFY2(held.exchanges > kHeldExchange + 4, "the held run's worker exchanged too few chunks");
        QVERIFY2(g_sawStall.load(), "the caller never stalled while the worker was held");

        double energy = 0.0;
        for (double v : reference.out) {
            energy += v * v;
        }
        QVERIFY2(energy > 0.0, "the channel produced no output");

        int firstDiff = -1;
        int diffs = 0;
        for (size_t i = 0; i < reference.out.size(); ++i) {
            if (std::memcmp(&reference.out[i], &held.out[i], sizeof(double)) != 0) {
                if (firstDiff < 0) {
                    firstDiff = static_cast<int>(i / 2);
                }
                ++diffs;
            }
        }
        if (diffs != 0) {
            qWarning("held run differs from the reference in %d values, first at output frame %d",
                     diffs, firstDiff);
        }
        QCOMPARE(diffs, 0);
    }
};

QTEST_GUILESS_MAIN(TestWdspDexchangeOrder)
#include "tst_wdsp_dexchange_order.moc"
